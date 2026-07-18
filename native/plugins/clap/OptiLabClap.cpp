#include "OptiLabEditor.h"
#include "OptiLabCore.h"
#include "OptiLabVersion.h"

#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/latency.h>
#include <clap/ext/params.h>
#include <clap/ext/render.h>
#include <clap/ext/state.h>
#include <clap/factory/plugin-factory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace {

enum ParameterId : clap_id {
    modeParameter = 0,
    inputDriveParameter = 1,
    autoAdaptParameter = 2,
    parameterCount = 3,
};

constexpr std::uint32_t stateVersion = 1;
constexpr std::array<char, 8> stateMagic{'O', 'P', 'T', 'L', 'C', 'O', 'R', 'E'};
constexpr std::uint32_t minimumEditorWidth = 360;
constexpr std::uint32_t minimumEditorHeight = 220;
constexpr std::uint32_t maximumEditorWidth = 1920;
constexpr std::uint32_t maximumEditorHeight = 1440;

const char* const pluginFeatures[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_MASTERING,
    CLAP_PLUGIN_FEATURE_LIMITER,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};

const clap_plugin_descriptor_t pluginDescriptor{
    CLAP_VERSION_INIT,
    "com.lanesaudio.optilab-core",
    "OptiLab Core",
    "LanesAudio",
    "https://github.com/dgl1984/optilab",
    "https://github.com/dgl1984/optilab/blob/main/native/CLAP.md",
    "https://github.com/dgl1984/optilab/issues",
    OPTILAB_VERSION_STRING,
    "Broadcast processing, density control, and peak protection.",
    pluginFeatures,
};

double clampParameter(clap_id id, double value) {
    if (id == modeParameter) return std::round(std::clamp(value, 0.0, 2.0));
    if (id == inputDriveParameter) return std::clamp(value, -12.0, 18.0);
    if (id == autoAdaptParameter) return std::clamp(value, 0.0, 100.0);
    return 0.0;
}

double defaultInputDrive(int mode) {
    return mode == 0 ? 3.5 : mode == 1 ? 4.5 : 0.0;
}

const char* modeName(int mode) {
    if (mode == 0) return "Podcast Leveler";
    if (mode == 1) return "Stream polish";
    return "Smooth Limiter";
}

bool copyText(char* destination, std::uint32_t capacity, const char* source) {
    if (!destination || capacity == 0 || !source) return false;
    std::snprintf(destination, capacity, "%s", source);
    return true;
}

bool writeFully(const clap_ostream_t* stream, const void* data, std::uint64_t size) {
    if (!stream || !stream->write) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t offset = 0;
    while (offset < size) {
        const std::int64_t written = stream->write(stream, bytes + offset, size - offset);
        if (written <= 0) return false;
        offset += static_cast<std::uint64_t>(written);
    }
    return true;
}

bool readFully(const clap_istream_t* stream, void* data, std::uint64_t size) {
    if (!stream || !stream->read) return false;
    auto* bytes = static_cast<std::uint8_t*>(data);
    std::uint64_t offset = 0;
    while (offset < size) {
        const std::int64_t read = stream->read(stream, bytes + offset, size - offset);
        if (read <= 0) return false;
        offset += static_cast<std::uint64_t>(read);
    }
    return true;
}

class ScopedFlushDenormals {
public:
#if defined(_M_X64) || defined(__x86_64__)
    ScopedFlushDenormals() : previous_(_mm_getcsr()) {
        _mm_setcsr(previous_ | 0x8040u);
    }

    ~ScopedFlushDenormals() {
        _mm_setcsr(previous_);
    }

private:
    unsigned int previous_;
#else
    ScopedFlushDenormals() = default;
#endif
};

class OptiLabClap final : public OptiLabEditorDelegate {
public:
    explicit OptiLabClap(const clap_host_t* host)
        : host_(host), editor_(*this) {
        plugin_.desc = &pluginDescriptor;
        plugin_.plugin_data = this;
        plugin_.init = initCallback;
        plugin_.destroy = destroyCallback;
        plugin_.activate = activateCallback;
        plugin_.deactivate = deactivateCallback;
        plugin_.start_processing = startProcessingCallback;
        plugin_.stop_processing = stopProcessingCallback;
        plugin_.reset = resetCallback;
        plugin_.process = processCallback;
        plugin_.get_extension = getExtensionCallback;
        plugin_.on_main_thread = onMainThreadCallback;
    }

    const clap_plugin_t* plugin() const noexcept { return &plugin_; }

    double parameterValue(std::uint32_t id) const noexcept override {
        return id < parameterValues_.size()
                   ? parameterValues_[id].load(std::memory_order_relaxed)
                   : 0.0;
    }

    void setParameterFromEditor(std::uint32_t id, double value) override {
        if (id >= parameterCount) return;
        setAtomicParameter(id, value, true);
        if (hostParams_ && hostParams_->request_flush) {
            hostParams_->request_flush(host_);
        } else if (host_ && host_->request_process) {
            host_->request_process(host_);
        }
    }

    OptiLabMeterSnapshot meterSnapshot() const noexcept override {
        return {
            inputLeft_.load(std::memory_order_relaxed),
            inputRight_.load(std::memory_order_relaxed),
            agcReductionDb_.load(std::memory_order_relaxed),
            densityReductionDb_.load(std::memory_order_relaxed),
            finalReductionDb_.load(std::memory_order_relaxed),
        };
    }

    void setEditorVisible(bool visible) noexcept override {
        editorVisible_.store(visible, std::memory_order_release);
    }

private:
    clap_plugin_t plugin_{};
    const clap_host_t* host_ = nullptr;
    const clap_host_params_t* hostParams_ = nullptr;
    std::array<std::atomic<double>, parameterCount> parameterValues_{
        std::atomic<double>{0.0},
        std::atomic<double>{3.5},
        std::atomic<double>{0.0},
    };
    std::atomic<std::uint32_t> pendingEditorParameters_{0};
    std::atomic<bool> editorVisible_{false};
    std::atomic<bool> offlineRendering_{false};
    std::atomic<double> inputLeft_{0.0};
    std::atomic<double> inputRight_{0.0};
    std::atomic<double> agcReductionDb_{0.0};
    std::atomic<double> densityReductionDb_{0.0};
    std::atomic<double> finalReductionDb_{0.0};

    OptiLabCore core_;
    OptiLabCore::Parameters appliedParameters_{};
    bool parametersApplied_ = false;
    bool active_ = false;
    bool guiCreated_ = false;
    bool trackingMeters_ = false;
    std::uint32_t editorWidth_ = 600;
    std::uint32_t editorHeight_ = 300;
    std::uint32_t fixedLatency_ = 0;
    double fixedLatencySampleRate_ = 0.0;
    std::vector<float> compensationLeft_;
    std::vector<float> compensationRight_;
    std::size_t compensationWrite_ = 0;
    std::size_t compensationDelay_ = 0;
    OptiLabEditor editor_;

    static OptiLabClap* self(const clap_plugin_t* plugin) {
        return static_cast<OptiLabClap*>(plugin->plugin_data);
    }

    OptiLabCore::Parameters currentParameters() const {
        OptiLabCore::Parameters values;
        values.mode = static_cast<OptiLabCore::Mode>(
            static_cast<int>(parameterValue(modeParameter)));
        values.inputDriveDb = parameterValue(inputDriveParameter);
        values.autoAdaptPct = parameterValue(autoAdaptParameter);
        return values;
    }

    void setAtomicParameter(clap_id id, double value, bool fromEditor) {
        if (id >= parameterCount) return;
        const double clamped = clampParameter(id, value);
        const double oldValue = parameterValues_[id].exchange(clamped, std::memory_order_relaxed);
        if (oldValue == clamped) return;
        if (fromEditor) {
            pendingEditorParameters_.fetch_or(1u << id, std::memory_order_release);
        }
    }

    void setModeWithDefaultDrive(double value, bool fromEditor) {
        const int oldMode = static_cast<int>(parameterValue(modeParameter));
        const int newMode = static_cast<int>(clampParameter(modeParameter, value));
        setAtomicParameter(modeParameter, static_cast<double>(newMode), fromEditor);
        if (newMode != oldMode) {
            setAtomicParameter(inputDriveParameter, defaultInputDrive(newMode), fromEditor);
        }
    }

    void applyParametersToCore() {
        const auto values = currentParameters();
        const bool changed = !parametersApplied_ ||
                             values.mode != appliedParameters_.mode ||
                             values.inputDriveDb != appliedParameters_.inputDriveDb ||
                             values.autoAdaptPct != appliedParameters_.autoAdaptPct;
        const bool shouldTrack =
            editorVisible_.load(std::memory_order_acquire) &&
            !offlineRendering_.load(std::memory_order_acquire);
        if (shouldTrack != trackingMeters_) {
            trackingMeters_ = shouldTrack;
            core_.setActivityTracking(shouldTrack);
        }
        if (!changed) return;
        core_.setParameters(values);
        appliedParameters_ = core_.parameters();
        parameterValues_[inputDriveParameter].store(
            appliedParameters_.inputDriveDb, std::memory_order_relaxed);
        const std::size_t coreLatency = core_.latencySamples();
        compensationDelay_ = fixedLatency_ > coreLatency
                                 ? fixedLatency_ - coreLatency
                                 : 0;
        parametersApplied_ = true;
    }

    std::uint32_t calculateFixedLatency(double sampleRate) const {
        std::uint32_t maximum = 0;
        for (int mode = 0; mode < 3; ++mode) {
            for (double adapt : {0.0, 100.0}) {
                OptiLabCore probe;
                auto values = OptiLabCore::defaultParameters(
                    static_cast<OptiLabCore::Mode>(mode));
                values.autoAdaptPct = adapt;
                probe.setParameters(values);
                probe.prepare(sampleRate);
                maximum = std::max(
                    maximum, static_cast<std::uint32_t>(probe.latencySamples()));
            }
        }
        return maximum;
    }

    void clearCompensation() {
        std::fill(compensationLeft_.begin(), compensationLeft_.end(), 0.0f);
        std::fill(compensationRight_.begin(), compensationRight_.end(), 0.0f);
        compensationWrite_ = 0;
    }

    std::pair<float, float> compensate(float left, float right) {
        if (compensationLeft_.empty()) return {left, right};
        compensationLeft_[compensationWrite_] = left;
        compensationRight_[compensationWrite_] = right;
        const std::size_t delay =
            std::min(compensationDelay_, compensationLeft_.size() - 1);
        const std::size_t read = compensationWrite_ >= delay
                                     ? compensationWrite_ - delay
                                     : compensationWrite_ + compensationLeft_.size() - delay;
        const auto result =
            std::pair<float, float>{compensationLeft_[read], compensationRight_[read]};
        if (++compensationWrite_ == compensationLeft_.size()) {
            compensationWrite_ = 0;
        }
        return result;
    }

    static bool pushParameterEvent(const clap_output_events_t* output,
                                   clap_id id,
                                   double value,
                                   std::uint32_t time,
                                   std::uint16_t type) {
        if (!output || !output->try_push) return false;
        if (type == CLAP_EVENT_PARAM_VALUE) {
            clap_event_param_value_t event{};
            event.header = {
                sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID,
                CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_IS_LIVE,
            };
            event.param_id = id;
            event.note_id = -1;
            event.port_index = -1;
            event.channel = -1;
            event.key = -1;
            event.value = value;
            return output->try_push(output, &event.header);
        }
        clap_event_param_gesture_t event{};
        event.header = {
            sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID, type, CLAP_EVENT_IS_LIVE,
        };
        event.param_id = id;
        return output->try_push(output, &event.header);
    }

    void flushEditorParameters(const clap_output_events_t* output) {
        const std::uint32_t mask =
            pendingEditorParameters_.exchange(0, std::memory_order_acq_rel);
        for (clap_id id = 0; id < parameterCount; ++id) {
            if ((mask & (1u << id)) == 0) continue;
            pushParameterEvent(output, id, parameterValue(id), 0,
                               CLAP_EVENT_PARAM_GESTURE_BEGIN);
            pushParameterEvent(output, id, parameterValue(id), 0,
                               CLAP_EVENT_PARAM_VALUE);
            pushParameterEvent(output, id, parameterValue(id), 0,
                               CLAP_EVENT_PARAM_GESTURE_END);
        }
    }

    bool handleInputParameterEvent(const clap_event_header_t* header,
                                   const clap_output_events_t* output) {
        if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID ||
            header->type != CLAP_EVENT_PARAM_VALUE) {
            return false;
        }
        const auto* event = reinterpret_cast<const clap_event_param_value_t*>(header);
        if (event->param_id >= parameterCount) return false;
        const int previousMode = static_cast<int>(parameterValue(modeParameter));
        if (event->param_id == modeParameter) {
            setModeWithDefaultDrive(event->value, false);
            const int newMode = static_cast<int>(parameterValue(modeParameter));
            if (newMode != previousMode) {
                pushParameterEvent(output, inputDriveParameter,
                                   parameterValue(inputDriveParameter),
                                   header->time, CLAP_EVENT_PARAM_VALUE);
            }
        } else {
            setAtomicParameter(event->param_id, event->value, false);
        }
        return true;
    }

    static double gainReductionDb(double gain) {
        return std::max(0.0, -20.0 * std::log10(std::max(gain, 0.000001)));
    }

    clap_process_status processAudio(const clap_process_t* process) {
        if (!process || process->audio_inputs_count != 1 ||
            process->audio_outputs_count != 1 ||
            !process->audio_inputs || !process->audio_outputs) {
            return CLAP_PROCESS_ERROR;
        }
        const clap_audio_buffer_t& input = process->audio_inputs[0];
        clap_audio_buffer_t& output = process->audio_outputs[0];
        if (!input.data32 || !output.data32 ||
            input.channel_count < 2 || output.channel_count < 2 ||
            !input.data32[0] || !input.data32[1] ||
            !output.data32[0] || !output.data32[1]) {
            return CLAP_PROCESS_ERROR;
        }

        flushEditorParameters(process->out_events);
        applyParametersToCore();
        ScopedFlushDenormals flushDenormals;

        const std::uint32_t eventCount =
            process->in_events && process->in_events->size
                ? process->in_events->size(process->in_events)
                : 0;
        std::uint32_t eventIndex = 0;
        double peakLeft = 0.0;
        double peakRight = 0.0;

        const auto processRange = [&](std::uint32_t first, std::uint32_t last) {
            for (std::uint32_t frame = first; frame < last; ++frame) {
                const float left = input.data32[0][frame];
                const float right = input.data32[1][frame];
                if (trackingMeters_) {
                    peakLeft = std::max(peakLeft, std::abs(static_cast<double>(left)));
                    peakRight = std::max(peakRight, std::abs(static_cast<double>(right)));
                }
                const auto processed = core_.processSample(left, right);
                const auto delayed = compensate(processed.first, processed.second);
                output.data32[0][frame] = delayed.first;
                output.data32[1][frame] = delayed.second;
            }
        };

        std::uint32_t frame = 0;
        while (frame < process->frames_count) {
            while (eventIndex < eventCount) {
                const clap_event_header_t* event =
                    process->in_events->get(process->in_events, eventIndex);
                if (event && event->time > frame) break;
                handleInputParameterEvent(event, process->out_events);
                ++eventIndex;
            }
            applyParametersToCore();

            std::uint32_t nextFrame = process->frames_count;
            if (eventIndex < eventCount) {
                const clap_event_header_t* next =
                    process->in_events->get(process->in_events, eventIndex);
                if (next) {
                    nextFrame = std::min(next->time, process->frames_count);
                }
            }
            if (nextFrame <= frame) {
                ++eventIndex;
                continue;
            }
            processRange(frame, nextFrame);
            frame = nextFrame;
        }

        if (trackingMeters_) {
            const auto activity = core_.activity();
            inputLeft_.store(peakLeft, std::memory_order_relaxed);
            inputRight_.store(peakRight, std::memory_order_relaxed);
            agcReductionDb_.store(gainReductionDb(activity.agcGain),
                                  std::memory_order_relaxed);
            densityReductionDb_.store(gainReductionDb(activity.densityGain),
                                      std::memory_order_relaxed);
            finalReductionDb_.store(gainReductionDb(activity.finalGain),
                                    std::memory_order_relaxed);
        }
        return CLAP_PROCESS_CONTINUE;
    }

    bool init() {
        if (!host_ || !host_->get_extension) return true;
        hostParams_ = static_cast<const clap_host_params_t*>(
            host_->get_extension(host_, CLAP_EXT_PARAMS));
        return true;
    }

    bool activate(double sampleRate, std::uint32_t maxFrames) {
        if (sampleRate <= 0.0 || maxFrames == 0) return false;
        const auto values = currentParameters();
        core_.setParameters(values);
        core_.prepare(sampleRate);
        const bool shouldTrack =
            editorVisible_.load(std::memory_order_acquire) &&
            !offlineRendering_.load(std::memory_order_acquire);
        core_.setActivityTracking(shouldTrack);
        appliedParameters_ = core_.parameters();
        parametersApplied_ = true;
        trackingMeters_ = shouldTrack;
        if (sampleRate != fixedLatencySampleRate_) {
            fixedLatency_ = calculateFixedLatency(sampleRate);
            fixedLatencySampleRate_ = sampleRate;
        }
        const std::size_t coreLatency = core_.latencySamples();
        compensationDelay_ = fixedLatency_ > coreLatency
                                 ? fixedLatency_ - coreLatency
                                 : 0;
        const std::size_t compensationSize =
            static_cast<std::size_t>(fixedLatency_) + maxFrames + 1;
        compensationLeft_.assign(compensationSize, 0.0f);
        compensationRight_.assign(compensationSize, 0.0f);
        compensationWrite_ = 0;
        active_ = true;
        return true;
    }

    void deactivate() {
        active_ = false;
        compensationLeft_.clear();
        compensationRight_.clear();
        parametersApplied_ = false;
    }

    bool saveState(const clap_ostream_t* stream) const {
        const std::int32_t mode =
            static_cast<std::int32_t>(parameterValue(modeParameter));
        const double input = parameterValue(inputDriveParameter);
        const double adapt = parameterValue(autoAdaptParameter);
        return writeFully(stream, stateMagic.data(), stateMagic.size()) &&
               writeFully(stream, &stateVersion, sizeof(stateVersion)) &&
               writeFully(stream, &mode, sizeof(mode)) &&
               writeFully(stream, &input, sizeof(input)) &&
               writeFully(stream, &adapt, sizeof(adapt));
    }

    bool loadState(const clap_istream_t* stream) {
        std::array<char, 8> magic{};
        std::uint32_t version = 0;
        std::int32_t mode = 0;
        double input = 0.0;
        double adapt = 0.0;
        if (!readFully(stream, magic.data(), magic.size()) ||
            !readFully(stream, &version, sizeof(version)) ||
            !readFully(stream, &mode, sizeof(mode)) ||
            !readFully(stream, &input, sizeof(input)) ||
            !readFully(stream, &adapt, sizeof(adapt)) ||
            magic != stateMagic || version != stateVersion) {
            return false;
        }
        parameterValues_[modeParameter].store(
            clampParameter(modeParameter, static_cast<double>(mode)),
            std::memory_order_relaxed);
        parameterValues_[inputDriveParameter].store(
            clampParameter(inputDriveParameter, input), std::memory_order_relaxed);
        parameterValues_[autoAdaptParameter].store(
            clampParameter(autoAdaptParameter, adapt), std::memory_order_relaxed);
        parametersApplied_ = false;
        if (hostParams_ && hostParams_->rescan) {
            hostParams_->rescan(host_, CLAP_PARAM_RESCAN_VALUES);
        }
        if (active_ && host_ && host_->request_process) {
            host_->request_process(host_);
        }
        return true;
    }

    static bool CLAP_ABI initCallback(const clap_plugin_t* plugin) {
        return self(plugin)->init();
    }

    static void CLAP_ABI destroyCallback(const clap_plugin_t* plugin) {
        delete self(plugin);
    }

    static bool CLAP_ABI activateCallback(const clap_plugin_t* plugin,
                                          double sampleRate,
                                          std::uint32_t,
                                          std::uint32_t maxFrames) {
        return self(plugin)->activate(sampleRate, maxFrames);
    }

    static void CLAP_ABI deactivateCallback(const clap_plugin_t* plugin) {
        self(plugin)->deactivate();
    }

    static bool CLAP_ABI startProcessingCallback(const clap_plugin_t*) {
        return true;
    }

    static void CLAP_ABI stopProcessingCallback(const clap_plugin_t*) {}

    static void CLAP_ABI resetCallback(const clap_plugin_t* plugin) {
        auto* instance = self(plugin);
        instance->core_.reset();
        instance->clearCompensation();
    }

    static clap_process_status CLAP_ABI processCallback(
        const clap_plugin_t* plugin, const clap_process_t* process) {
        return self(plugin)->processAudio(process);
    }

    static const void* CLAP_ABI getExtensionCallback(
        const clap_plugin_t* plugin, const char* id);

    static void CLAP_ABI onMainThreadCallback(const clap_plugin_t*) {}

public:
    static std::uint32_t CLAP_ABI audioPortsCount(const clap_plugin_t*, bool) {
        return 1;
    }

    static bool CLAP_ABI audioPortsGet(const clap_plugin_t*,
                                       std::uint32_t index,
                                       bool isInput,
                                       clap_audio_port_info_t* info) {
        if (index != 0 || !info) return false;
        *info = {};
        info->id = 0;
        copyText(info->name, CLAP_NAME_SIZE, isInput ? "Stereo Input" : "Stereo Output");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN |
                      CLAP_AUDIO_PORT_REQUIRES_COMMON_SAMPLE_SIZE;
        info->channel_count = 2;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = 0;
        return true;
    }

    static std::uint32_t CLAP_ABI paramsCount(const clap_plugin_t*) {
        return parameterCount;
    }

    static bool CLAP_ABI paramsGetInfo(const clap_plugin_t*,
                                       std::uint32_t index,
                                       clap_param_info_t* info) {
        if (!info || index >= parameterCount) return false;
        *info = {};
        info->id = index;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS;
        if (index == modeParameter) {
            info->flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
            copyText(info->name, CLAP_NAME_SIZE, "Mode");
            info->min_value = 0.0;
            info->max_value = 2.0;
            info->default_value = 0.0;
        } else if (index == inputDriveParameter) {
            copyText(info->name, CLAP_NAME_SIZE, "Input Drive");
            info->min_value = -12.0;
            info->max_value = 18.0;
            info->default_value = 3.5;
        } else {
            copyText(info->name, CLAP_NAME_SIZE, "Auto-Adapt");
            info->min_value = 0.0;
            info->max_value = 100.0;
            info->default_value = 0.0;
        }
        copyText(info->module, CLAP_PATH_SIZE, "Core");
        return true;
    }

    static bool CLAP_ABI paramsGetValue(const clap_plugin_t* plugin,
                                        clap_id id,
                                        double* value) {
        if (!value || id >= parameterCount) return false;
        *value = self(plugin)->parameterValue(id);
        return true;
    }

    static bool CLAP_ABI paramsValueToText(const clap_plugin_t*,
                                           clap_id id,
                                           double value,
                                           char* output,
                                           std::uint32_t capacity) {
        if (!output || capacity == 0 || id >= parameterCount) return false;
        value = clampParameter(id, value);
        if (id == modeParameter) {
            return copyText(output, capacity, modeName(static_cast<int>(value)));
        }
        if (id == inputDriveParameter) {
            std::snprintf(output, capacity, "%.1f dB", value);
        } else {
            std::snprintf(output, capacity, "%.0f percent", value);
        }
        return true;
    }

    static bool CLAP_ABI paramsTextToValue(const clap_plugin_t*,
                                           clap_id id,
                                           const char* text,
                                           double* value) {
        if (!text || !value || id >= parameterCount) return false;
        if (id == modeParameter) {
            for (int mode = 0; mode < 3; ++mode) {
                if (_stricmp(text, modeName(mode)) == 0) {
                    *value = static_cast<double>(mode);
                    return true;
                }
            }
        }
        char* end = nullptr;
        const double parsed = std::strtod(text, &end);
        if (end == text) return false;
        *value = clampParameter(id, parsed);
        return true;
    }

    static void CLAP_ABI paramsFlush(const clap_plugin_t* plugin,
                                     const clap_input_events_t* input,
                                     const clap_output_events_t* output) {
        auto* instance = self(plugin);
        const std::uint32_t count =
            input && input->size ? input->size(input) : 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            instance->handleInputParameterEvent(input->get(input, index), output);
        }
        instance->flushEditorParameters(output);
        if (!instance->active_) instance->applyParametersToCore();
    }

    static bool CLAP_ABI stateSave(const clap_plugin_t* plugin,
                                   const clap_ostream_t* stream) {
        return self(plugin)->saveState(stream);
    }

    static bool CLAP_ABI stateLoad(const clap_plugin_t* plugin,
                                   const clap_istream_t* stream) {
        return self(plugin)->loadState(stream);
    }

    static std::uint32_t CLAP_ABI latencyGet(const clap_plugin_t* plugin) {
        return self(plugin)->fixedLatency_;
    }

    static bool CLAP_ABI renderHasHardRealtimeRequirement(const clap_plugin_t*) {
        return false;
    }

    static bool CLAP_ABI renderSet(const clap_plugin_t* plugin,
                                   clap_plugin_render_mode mode) {
        if (mode != CLAP_RENDER_REALTIME && mode != CLAP_RENDER_OFFLINE) {
            return false;
        }
        self(plugin)->offlineRendering_.store(
            mode == CLAP_RENDER_OFFLINE, std::memory_order_release);
        return true;
    }

    static bool CLAP_ABI guiIsApiSupported(const clap_plugin_t*,
                                           const char* api,
                                           bool floating) {
        return !floating && api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
    }

    static bool CLAP_ABI guiGetPreferredApi(const clap_plugin_t*,
                                            const char** api,
                                            bool* floating) {
        if (!api || !floating) return false;
        *api = CLAP_WINDOW_API_WIN32;
        *floating = false;
        return true;
    }

    static bool CLAP_ABI guiCreate(const clap_plugin_t* plugin,
                                   const char* api,
                                   bool floating) {
        if (!guiIsApiSupported(plugin, api, floating)) return false;
        auto* instance = self(plugin);
        if (instance->guiCreated_) return false;
        instance->guiCreated_ = true;
        return true;
    }

    static void CLAP_ABI guiDestroy(const clap_plugin_t* plugin) {
        auto* instance = self(plugin);
        instance->editor_.destroy();
        instance->guiCreated_ = false;
    }

    static bool CLAP_ABI guiSetScale(const clap_plugin_t*, double) {
        return false;
    }

    static bool CLAP_ABI guiGetSize(const clap_plugin_t* plugin,
                                    std::uint32_t* width,
                                    std::uint32_t* height) {
        if (!width || !height || !self(plugin)->guiCreated_) return false;
        *width = self(plugin)->editorWidth_;
        *height = self(plugin)->editorHeight_;
        return true;
    }

    static bool CLAP_ABI guiCanResize(const clap_plugin_t*) {
        return true;
    }

    static bool CLAP_ABI guiGetResizeHints(const clap_plugin_t*,
                                           clap_gui_resize_hints_t* hints) {
        if (!hints) return false;
        *hints = {true, true, false, 0, 0};
        return true;
    }

    static bool CLAP_ABI guiAdjustSize(const clap_plugin_t*,
                                       std::uint32_t* width,
                                       std::uint32_t* height) {
        if (!width || !height) return false;
        *width = std::clamp(*width, minimumEditorWidth, maximumEditorWidth);
        *height = std::clamp(*height, minimumEditorHeight, maximumEditorHeight);
        return true;
    }

    static bool CLAP_ABI guiSetSize(const clap_plugin_t* plugin,
                                    std::uint32_t width,
                                    std::uint32_t height) {
        if (!guiAdjustSize(plugin, &width, &height)) return false;
        auto* instance = self(plugin);
        instance->editorWidth_ = width;
        instance->editorHeight_ = height;
        return instance->editor_.setSize(width, height);
    }

    static bool CLAP_ABI guiSetParent(const clap_plugin_t* plugin,
                                      const clap_window_t* window) {
        auto* instance = self(plugin);
        if (!instance->guiCreated_ || !window || !window->api ||
            std::strcmp(window->api, CLAP_WINDOW_API_WIN32) != 0 ||
            !window->win32) {
            return false;
        }
        return instance->editor_.create(
            window->win32, instance->editorWidth_, instance->editorHeight_);
    }

    static bool CLAP_ABI guiSetTransient(const clap_plugin_t*,
                                         const clap_window_t*) {
        return false;
    }

    static void CLAP_ABI guiSuggestTitle(const clap_plugin_t*, const char*) {}

    static bool CLAP_ABI guiShow(const clap_plugin_t* plugin) {
        return self(plugin)->editor_.show();
    }

    static bool CLAP_ABI guiHide(const clap_plugin_t* plugin) {
        return self(plugin)->editor_.hide();
    }
};

const clap_plugin_audio_ports_t audioPortsExtension{
    OptiLabClap::audioPortsCount,
    OptiLabClap::audioPortsGet,
};

const clap_plugin_params_t paramsExtension{
    OptiLabClap::paramsCount,
    OptiLabClap::paramsGetInfo,
    OptiLabClap::paramsGetValue,
    OptiLabClap::paramsValueToText,
    OptiLabClap::paramsTextToValue,
    OptiLabClap::paramsFlush,
};

const clap_plugin_state_t stateExtension{
    OptiLabClap::stateSave,
    OptiLabClap::stateLoad,
};

const clap_plugin_latency_t latencyExtension{
    OptiLabClap::latencyGet,
};

const clap_plugin_render_t renderExtension{
    OptiLabClap::renderHasHardRealtimeRequirement,
    OptiLabClap::renderSet,
};

const clap_plugin_gui_t guiExtension{
    OptiLabClap::guiIsApiSupported,
    OptiLabClap::guiGetPreferredApi,
    OptiLabClap::guiCreate,
    OptiLabClap::guiDestroy,
    OptiLabClap::guiSetScale,
    OptiLabClap::guiGetSize,
    OptiLabClap::guiCanResize,
    OptiLabClap::guiGetResizeHints,
    OptiLabClap::guiAdjustSize,
    OptiLabClap::guiSetSize,
    OptiLabClap::guiSetParent,
    OptiLabClap::guiSetTransient,
    OptiLabClap::guiSuggestTitle,
    OptiLabClap::guiShow,
    OptiLabClap::guiHide,
};

const void* CLAP_ABI OptiLabClap::getExtensionCallback(
    const clap_plugin_t*, const char* id) {
    if (!id) return nullptr;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &audioPortsExtension;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &paramsExtension;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &stateExtension;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &latencyExtension;
    if (std::strcmp(id, CLAP_EXT_RENDER) == 0) return &renderExtension;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &guiExtension;
    return nullptr;
}

std::uint32_t CLAP_ABI factoryPluginCount(const clap_plugin_factory_t*) {
    return 1;
}

const clap_plugin_descriptor_t* CLAP_ABI factoryPluginDescriptor(
    const clap_plugin_factory_t*, std::uint32_t index) {
    return index == 0 ? &pluginDescriptor : nullptr;
}

const clap_plugin_t* CLAP_ABI factoryCreatePlugin(
    const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (!host || !id || !clap_version_is_compatible(host->clap_version) ||
        std::strcmp(id, pluginDescriptor.id) != 0) {
        return nullptr;
    }
    return (new OptiLabClap(host))->plugin();
}

const clap_plugin_factory_t pluginFactory{
    factoryPluginCount,
    factoryPluginDescriptor,
    factoryCreatePlugin,
};

std::atomic<std::uint32_t> entryReferences{0};

bool CLAP_ABI entryInit(const char*) {
    entryReferences.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void CLAP_ABI entryDeinit() {
    std::uint32_t current = entryReferences.load(std::memory_order_relaxed);
    while (current > 0 &&
           !entryReferences.compare_exchange_weak(
               current, current - 1, std::memory_order_relaxed)) {
    }
}

const void* CLAP_ABI entryGetFactory(const char* id) {
    if (id && std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &pluginFactory;
    }
    return nullptr;
}

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    CLAP_VERSION_INIT,
    entryInit,
    entryDeinit,
    entryGetFactory,
};

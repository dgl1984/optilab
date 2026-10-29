#pragma once

#include <memory>

#include "OptiLabCoreEngine.h"

#include <cstddef>
#include <array>
#include <string>
#include <string_view>
#include <utility>

class OptiLabCorePipeline {
public:
    using Preset = optilab_parameters::PipelinePreset;

    enum class Testpoint {
        None,
        Input,
        Agc,
        Bass,
        Density,
        Stereo,
        FinalDelta,
    };

    enum class Section {
        Input = 0,
        Agc,
        Bass,
        Density,
        StereoFm,
        Final,
        Count,
    };

    enum class SectionMode {
        Active = 0,
        Testpoint,
        Delta,
        Bypassed,
    };

    static constexpr bool sectionParticipates(SectionMode mode) noexcept {
        return mode != SectionMode::Bypassed;
    }

    struct MonitorState {
        std::array<SectionMode, static_cast<std::size_t>(Section::Count)> sections{};

        void set(Section section, SectionMode mode) noexcept;
        SectionMode get(Section section) const noexcept;
    };

    // Routing stops at the first diagnostic output. Consumers must use this
    // DSP-owned predicate rather than treating downstream Active modes as audible.
    static constexpr bool sectionParticipates(const MonitorState& monitor, std::size_t index) noexcept {
        if (index >= monitor.sections.size() || !sectionParticipates(monitor.sections[index])) return false;
        for (std::size_t i = 0; i < index; ++i)
            if (monitor.sections[i] == SectionMode::Testpoint || monitor.sections[i] == SectionMode::Delta)
                return false;
        return true;
    }

    struct Parameters {
        Preset preset = Preset::StreamPolish;
        Testpoint testpoint = Testpoint::None;
        double p002 = 0.0;
        bool overrideInputTrim = false;
        OptiLabCoreEngine::ParameterValues parameterOverrides;
        OptiLabCoreEngine::ParameterValues internalOverrides;
        MonitorState monitor;
    };

    static int presetIndex(Preset preset) noexcept;
    static Preset presetFromIndex(int index) noexcept;

    void prepare(double sampleRate);
    void reset();
    void setParameters(const Parameters& parameters);
    void setPreparedParameters(Preset preset, const OptiLabCoreEngine::ParameterValues& values, double p002) noexcept;
    void setParameterValue(std::string_view key, double value);
    bool setParameterValueRealtime(std::string_view key, double value) noexcept;
    bool setParameterValueRealtimeDeferred(std::string_view key, double value) noexcept;
    void commitRealtimeParameterChanges() noexcept;
    void setMonitorState(const MonitorState& monitor) noexcept;

    const Parameters& parameters() const noexcept { return parameters_; }
    OptiLabCoreEngine::ParameterValues effectiveParameterValues() const;
    OptiLabCoreEngine::ParameterValues effectiveCoreValues() const;
    OptiLabCoreEngine::Activity activity() const noexcept;
    void setActivityTracking(bool enabled) noexcept;
    bool auxTapEnabled() const noexcept;
    std::size_t latencySamples() const noexcept;

    std::pair<float, float> processSample(float left, float right);
    std::pair<float, float> processSampleAnalyzed(
        float left, float right, OptiLabAnalysisFrame& analysis);
    using AuxFrame = OptiLabCoreEngine::AuxFrame;
    std::pair<float, float> processSampleRouted(double left, double right,
        double auxLeft, double auxRight, AuxFrame& aux, OptiLabAnalysisFrame* analysis = nullptr);
    void processInterleaved(float* samples, std::size_t frames, std::size_t channels);

private:
    void applyParameters();
    void applyMonitorRouting() noexcept;

    // Allocate history storage at construction, never on the audio path.
    // High-rate histories exceed the default Windows thread stack budget.
    std::unique_ptr<OptiLabCoreEngine> translatedFoundation_ =
        std::make_unique<OptiLabCoreEngine>();
    Parameters parameters_;
    double sampleRate_ = 48000.0;
};

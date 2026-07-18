#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleacc.h>

#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/latency.h>
#include <clap/ext/params.h>
#include <clap/ext/render.h>
#include <clap/ext/state.h>
#include <clap/factory/plugin-factory.h>

#include "OptiLabVersion.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

struct MemoryStream {
    std::vector<std::uint8_t> bytes;
    std::size_t position = 0;
};

std::int64_t CLAP_ABI writeMemory(const clap_ostream_t* stream,
                                  const void* buffer,
                                  std::uint64_t size) {
    auto* memory = static_cast<MemoryStream*>(stream->ctx);
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    memory->bytes.insert(memory->bytes.end(), bytes, bytes + size);
    return static_cast<std::int64_t>(size);
}

std::int64_t CLAP_ABI readMemory(const clap_istream_t* stream,
                                 void* buffer,
                                 std::uint64_t size) {
    auto* memory = static_cast<MemoryStream*>(stream->ctx);
    const std::size_t available = memory->bytes.size() - memory->position;
    const std::size_t count =
        std::min<std::size_t>(available, static_cast<std::size_t>(size));
    if (count == 0) return 0;
    std::memcpy(buffer, memory->bytes.data() + memory->position, count);
    memory->position += count;
    return static_cast<std::int64_t>(count);
}

void CLAP_ABI hostParamsRescan(const clap_host_t*, clap_param_rescan_flags) {}
void CLAP_ABI hostParamsClear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
void CLAP_ABI hostParamsRequestFlush(const clap_host_t*) {}

const clap_host_params_t hostParams{
    hostParamsRescan,
    hostParamsClear,
    hostParamsRequestFlush,
};

const void* CLAP_ABI hostGetExtension(const clap_host_t*, const char* id) {
    return id && std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? &hostParams : nullptr;
}

void CLAP_ABI hostNoOp(const clap_host_t*) {}

std::uint32_t CLAP_ABI noEventsSize(const clap_input_events_t*) {
    return 0;
}

const clap_event_header_t* CLAP_ABI noEventsGet(const clap_input_events_t*,
                                                std::uint32_t) {
    return nullptr;
}

bool CLAP_ABI discardEvent(const clap_output_events_t*,
                           const clap_event_header_t*) {
    return true;
}

struct ControlCounts {
    int comboBoxes = 0;
    int edits = 0;
    int trackbars = 0;
    int tabStops = 0;
    HWND mode = nullptr;
    HWND inputDrive = nullptr;
    HWND inputTrack = nullptr;
    HWND autoAdapt = nullptr;
    HWND editor = nullptr;
};

BOOL CALLBACK countStandardControls(HWND window, LPARAM data) {
    auto* counts = reinterpret_cast<ControlCounts*>(data);
    wchar_t className[64]{};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    if (_wcsicmp(className, L"ComboBox") == 0) ++counts->comboBoxes;
    if (_wcsicmp(className, L"Edit") == 0) ++counts->edits;
    if (_wcsicmp(className, L"msctls_trackbar32") == 0) ++counts->trackbars;
    if ((GetWindowLongPtrW(window, GWL_STYLE) & WS_TABSTOP) != 0) ++counts->tabStops;
    if (GetDlgCtrlID(window) == 1001) counts->mode = window;
    if (GetDlgCtrlID(window) == 1002) counts->inputTrack = window;
    if (GetDlgCtrlID(window) == 1003) counts->autoAdapt = window;
    if (GetDlgCtrlID(window) == 1004) counts->inputDrive = window;
    if (counts->mode) counts->editor = GetParent(counts->mode);
    return TRUE;
}

void pumpMessages(std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline) {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

std::wstring accessibleValue(HWND window) {
    IAccessible* accessible = nullptr;
    if (FAILED(AccessibleObjectFromWindow(
            window, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
            reinterpret_cast<void**>(&accessible))) ||
        !accessible) {
        return {};
    }
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    BSTR value = nullptr;
    const HRESULT result = accessible->get_accValue(self, &value);
    std::wstring text;
    if (SUCCEEDED(result) && value) text.assign(value, SysStringLen(value));
    if (value) SysFreeString(value);
    accessible->Release();
    return text;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: optilab-clap-smoke-tests <plugin.clap>\n";
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (!expect(module != nullptr, "plugin must load")) return 1;
    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(
        GetProcAddress(module, "clap_entry"));
    bool passed = expect(entry != nullptr, "clap_entry must be exported");
    if (!entry) {
        FreeLibrary(module);
        return 1;
    }

    passed &= expect(entry->init(argv[1]), "entry init");
    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    passed &= expect(factory != nullptr, "plugin factory");
    passed &= expect(factory && factory->get_plugin_count(factory) == 1,
                     "one plugin descriptor");
    const clap_plugin_descriptor_t* descriptor =
        factory ? factory->get_plugin_descriptor(factory, 0) : nullptr;
    passed &= expect(descriptor &&
                         std::strcmp(descriptor->id,
                                     "com.lanesaudio.optilab-core") == 0,
                     "stable plugin id");
    passed &= expect(descriptor && descriptor->version &&
                         std::strcmp(descriptor->version,
                                     OPTILAB_VERSION_STRING) == 0,
                     "plugin version");
    passed &= expect(descriptor && descriptor->url && descriptor->url[0] &&
                         descriptor->manual_url && descriptor->manual_url[0] &&
                         descriptor->support_url && descriptor->support_url[0],
                     "plugin links");

    clap_host_t host{
        CLAP_VERSION_INIT,
        nullptr,
        "OptiLab smoke host",
        "LanesAudio",
        "",
        "1",
        hostGetExtension,
        hostNoOp,
        hostNoOp,
        hostNoOp,
    };
    const clap_plugin_t* plugin =
        descriptor ? factory->create_plugin(factory, &host, descriptor->id) : nullptr;
    passed &= expect(plugin != nullptr, "plugin instance");
    passed &= expect(plugin && plugin->init(plugin), "plugin init");
    if (!plugin) {
        entry->deinit();
        FreeLibrary(module);
        return 1;
    }

    const auto* params = static_cast<const clap_plugin_params_t*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    const auto* state = static_cast<const clap_plugin_state_t*>(
        plugin->get_extension(plugin, CLAP_EXT_STATE));
    const auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    const auto* latency = static_cast<const clap_plugin_latency_t*>(
        plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    const auto* render = static_cast<const clap_plugin_render_t*>(
        plugin->get_extension(plugin, CLAP_EXT_RENDER));
    const auto* gui = static_cast<const clap_plugin_gui_t*>(
        plugin->get_extension(plugin, CLAP_EXT_GUI));
    passed &= expect(params && state && audioPorts && latency && gui,
                     "required extensions");
    passed &= expect(render != nullptr, "offline render extension");

    passed &= expect(params && params->count(plugin) == 3, "three accessible parameters");
    const char* expectedNames[] = {"Mode", "Input Drive", "Auto-Adapt"};
    for (std::uint32_t index = 0; params && index < params->count(plugin); ++index) {
        clap_param_info_t info{};
        passed &= expect(params->get_info(plugin, index, &info), "parameter info");
        passed &= expect(std::strcmp(info.name, expectedNames[index]) == 0,
                         "parameter name");
        passed &= expect((info.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0,
                         "parameter must be OSARA-visible by default");
        char text[64]{};
        passed &= expect(params->value_to_text(plugin, info.id, info.default_value,
                                               text, sizeof(text)) &&
                             text[0] != '\0',
                         "parameter must have readable value text");
    }

    passed &= expect(audioPorts && audioPorts->count(plugin, true) == 1 &&
                         audioPorts->count(plugin, false) == 1,
                     "stereo input and output ports");
    if (render) {
        passed &= expect(render->set(plugin, CLAP_RENDER_OFFLINE),
                         "offline render mode");
    }
    passed &= expect(plugin->activate(plugin, 48000.0, 1, 512), "activate");
    const std::uint32_t reportedLatency = latency ? latency->get(plugin) : 0;
    passed &= expect(reportedLatency > 0, "reported latency");
    passed &= expect(plugin->start_processing(plugin), "start processing");

    constexpr std::uint32_t frames = 2048;
    std::vector<float> inputLeft(frames, 0.0f);
    std::vector<float> inputRight(frames, 0.0f);
    std::vector<float> outputLeft(frames, 0.0f);
    std::vector<float> outputRight(frames, 0.0f);
    inputLeft[0] = 0.25f;
    inputRight[0] = -0.25f;
    float* inputChannels[]{inputLeft.data(), inputRight.data()};
    float* outputChannels[]{outputLeft.data(), outputRight.data()};
    clap_audio_buffer_t inputBuffer{inputChannels, nullptr, 2, 0, 0};
    clap_audio_buffer_t outputBuffer{outputChannels, nullptr, 2, 0, 0};
    clap_input_events_t inputEvents{nullptr, noEventsSize, noEventsGet};
    clap_output_events_t outputEvents{nullptr, discardEvent};
    clap_process_t process{
        0,
        frames,
        nullptr,
        &inputBuffer,
        &outputBuffer,
        1,
        1,
        &inputEvents,
        &outputEvents,
    };
    passed &= expect(plugin->process(plugin, &process) == CLAP_PROCESS_CONTINUE,
                     "audio process");
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        passed &= expect(std::isfinite(outputLeft[frame]) &&
                             std::isfinite(outputRight[frame]),
                         "finite audio output");
        if (!passed) break;
    }

    char* benchmarkSeconds = nullptr;
    std::size_t benchmarkValueLength = 0;
    _dupenv_s(&benchmarkSeconds, &benchmarkValueLength,
              "OPTILAB_BENCHMARK_SECONDS");
    if (benchmarkSeconds) {
        const double requestedSeconds = std::max(1.0, std::atof(benchmarkSeconds));
        constexpr std::uint32_t benchmarkFrames = 512;
        const std::uint64_t blocks = static_cast<std::uint64_t>(
            std::ceil(requestedSeconds * 48000.0 / benchmarkFrames));
        process.frames_count = benchmarkFrames;
        plugin->reset(plugin);
        std::uint64_t outputHash = 1469598103934665603ull;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t block = 0; block < blocks; ++block) {
            process.steady_time =
                static_cast<std::int64_t>(block * benchmarkFrames);
            if (plugin->process(plugin, &process) != CLAP_PROCESS_CONTINUE) {
                passed = false;
                break;
            }
            for (std::uint32_t frame = 0; frame < benchmarkFrames; ++frame) {
                std::uint32_t leftBits = 0;
                std::uint32_t rightBits = 0;
                std::memcpy(&leftBits, &outputLeft[frame], sizeof(leftBits));
                std::memcpy(&rightBits, &outputRight[frame], sizeof(rightBits));
                outputHash = (outputHash ^ leftBits) * 1099511628211ull;
                outputHash = (outputHash ^ rightBits) * 1099511628211ull;
            }
        }
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        const double audioSeconds =
            static_cast<double>(blocks * benchmarkFrames) / 48000.0;
        std::cout << "BENCHMARK audio_seconds=" << audioSeconds
                  << " elapsed_seconds=" << elapsed
                  << " realtime_multiple=" << audioSeconds / elapsed
                  << " output_hash=" << outputHash << '\n';
        std::free(benchmarkSeconds);
    }

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);

    MemoryStream memory;
    clap_ostream_t outputStream{&memory, writeMemory};
    clap_istream_t inputStream{&memory, readMemory};
    passed &= expect(state && state->save(plugin, &outputStream), "save state");
    passed &= expect(!memory.bytes.empty(), "state bytes");
    passed &= expect(state && state->load(plugin, &inputStream), "load state");

    passed &= expect(gui && gui->is_api_supported(
                                plugin, CLAP_WINDOW_API_WIN32, false),
                     "embedded Win32 editor");
    passed &= expect(gui && gui->create(plugin, CLAP_WINDOW_API_WIN32, false),
                     "create editor");
    std::uint32_t width = 100;
    std::uint32_t height = 100;
    passed &= expect(gui && gui->adjust_size(plugin, &width, &height) &&
                         width >= 360 && height >= 220,
                     "editor minimum size");
    HWND parent = CreateWindowExW(
        0, L"STATIC", L"OptiLab test parent", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(width), static_cast<int>(height),
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    WNDCLASSW wrapperClass{};
    wrapperClass.lpfnWndProc = DefWindowProcW;
    wrapperClass.hInstance = GetModuleHandleW(nullptr);
    wrapperClass.lpszClassName = L"reaperPluginHostWrapProc";
    RegisterClassW(&wrapperClass);
    HWND wrapper = CreateWindowExW(
        0, wrapperClass.lpszClassName, L"", WS_CHILD | WS_VISIBLE,
        0, 0, static_cast<int>(width), static_cast<int>(height),
        parent, nullptr, wrapperClass.hInstance, nullptr);
    clap_window_t parentWindow{};
    parentWindow.api = CLAP_WINDOW_API_WIN32;
    parentWindow.win32 = wrapper;
    passed &= expect(parent && wrapper && gui &&
                         gui->set_parent(plugin, &parentWindow),
                      "embed editor");
    ControlCounts controls;
    if (parent) EnumChildWindows(parent, countStandardControls,
                                 reinterpret_cast<LPARAM>(&controls));
    passed &= expect(controls.comboBoxes == 1 && controls.edits == 1 &&
                         controls.trackbars == 2,
                      "native standard parameter controls");
    passed &= expect(controls.tabStops >= 3, "keyboard-focusable parameter controls");
    passed &= expect(controls.editor && GetParent(controls.editor) == parent &&
                         GetParent(controls.editor) != wrapper,
                     "REAPER editor bypasses the host wrapper");
    if (gui && controls.mode && controls.inputDrive) {
        ShowWindow(parent, SW_SHOW);
        SetFocus(parent);
        gui->show(plugin);
        pumpMessages(std::chrono::milliseconds(100));
        passed &= expect(!IsWindowVisible(wrapper),
                         "REAPER wrapper is hidden after editor show");
        passed &= expect(GetFocus() == controls.mode,
                         "editor opens with focus on Mode");
        SendMessageW(controls.mode, WM_KEYDOWN, VK_TAB, 0);
        passed &= expect(GetFocus() == controls.inputDrive,
                         "Tab moves from Mode to Input Drive");
        wchar_t inputText[32]{};
        GetWindowTextW(controls.inputDrive, inputText,
                       static_cast<int>(std::size(inputText)));
        passed &= expect(std::wcsstr(inputText, L"dB") != nullptr,
                         "Input Drive exposes a dB value");
        passed &= expect(accessibleValue(controls.inputDrive).find(L"dB") !=
                             std::wstring::npos,
                         "Input Drive MSAA value includes dB");
        const std::wstring previousInputText(inputText);
        SendMessageW(controls.inputDrive, WM_KEYDOWN, VK_RIGHT, 0);
        GetWindowTextW(controls.inputDrive, inputText,
                       static_cast<int>(std::size(inputText)));
        passed &= expect(previousInputText != inputText,
                         "Input Drive arrow key changes its dB value");
        SendMessageW(controls.inputDrive, WM_KEYDOWN, VK_TAB, 0);
        passed &= expect(GetFocus() == controls.autoAdapt,
                         "Tab moves from Input Drive to Auto-Adapt");
    }
    if (gui) gui->destroy(plugin);
    if (parent) DestroyWindow(parent);

    plugin->destroy(plugin);
    entry->deinit();
    FreeLibrary(module);
    if (!passed) return 1;
    std::cout << "OptiLab CLAP smoke tests passed with accessible standard controls.\n";
    return 0;
}

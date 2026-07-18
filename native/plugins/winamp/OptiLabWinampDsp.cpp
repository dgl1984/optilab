#include "OptiLabCore.h"
#include "OptiLabVersion.h"
#include "WinampDspApi.h"
#include "WinampPcm.h"
#include "resource.h"

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <mutex>

namespace {

constexpr std::size_t scratchFrames = 1024;
constexpr UINT_PTR meterTimerId = 1;
constexpr UINT meterTimerMs = 150;
constexpr int meterFloorMilliDb = -120000;

HINSTANCE instanceHandle = nullptr;
std::once_flag settingsLoadFlag;
std::atomic<int> targetMode{0};
std::atomic<int> targetInputTenths{35};
std::atomic<int> targetAdaptPercent{0};
std::atomic<int> targetVisualMeters{0};
std::atomic<int> meterInputMilliDb{meterFloorMilliDb};
std::atomic<int> meterOutputMilliDb{meterFloorMilliDb};
std::atomic<int> meterFullScaleSamples{0};

struct DialogSettings {
    int mode = 0;
    int inputTenths = 35;
    int adaptPercent = 0;
    bool visualMeters = false;
};

struct ProcessorState {
    OptiLabCore core;
    std::array<float, scratchFrames * 2> scratch{};
    int sampleRate = 0;
    int appliedMode = -1;
    int appliedInputTenths = 0;
    int appliedAdaptPercent = 0;
    bool prepared = false;
};

ProcessorState processor;

const wchar_t* settingsRegistryPath() noexcept {
    static const std::array<wchar_t, 512> path = [] {
        std::array<wchar_t, 512> result{};
        std::array<wchar_t, MAX_PATH> executable{};
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                                static_cast<DWORD>(executable.size()));
        const wchar_t* host = L"Winamp-compatible host";
        if (length > 0 && length < executable.size()) {
            const wchar_t* slash = std::wcsrchr(executable.data(), L'\\');
            host = slash ? slash + 1 : executable.data();
        }
        std::swprintf(result.data(), result.size(),
                      L"Software\\LanesAudio\\OptiLab Core\\Winamp DSP\\%ls", host);
        return result;
    }();
    return path.data();
}

int defaultInputTenths(int mode) noexcept {
    return mode == 0 ? 35 : mode == 1 ? 10 : 0;
}

void loadSettings() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const wchar_t* registryPath = settingsRegistryPath();
    if (RegGetValueW(HKEY_CURRENT_USER, registryPath, L"Mode", RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 2) {
        targetMode.store(static_cast<int>(value), std::memory_order_relaxed);
    }
    size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, registryPath, L"InputTenths", RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 300) {
        const int signedValue = static_cast<int>(value) - 120;
        targetInputTenths.store(signedValue, std::memory_order_relaxed);
    }
    size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, registryPath, L"AdaptPercent", RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 100) {
        targetAdaptPercent.store(static_cast<int>(value), std::memory_order_relaxed);
    }
    size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, registryPath, L"VisualMeters", RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS) {
        targetVisualMeters.store(value ? 1 : 0, std::memory_order_relaxed);
    }
}

void ensureSettingsLoaded() {
    std::call_once(settingsLoadFlag, loadSettings);
}

void saveSettings(const DialogSettings& settings) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, settingsRegistryPath(), 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    const DWORD mode = static_cast<DWORD>(settings.mode);
    const DWORD input = static_cast<DWORD>(settings.inputTenths + 120);
    const DWORD adapt = static_cast<DWORD>(settings.adaptPercent);
    const DWORD visualMeters = settings.visualMeters ? 1u : 0u;
    RegSetValueExW(key, L"Mode", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&mode), sizeof(mode));
    RegSetValueExW(key, L"InputTenths", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&input), sizeof(input));
    RegSetValueExW(key, L"AdaptPercent", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&adapt), sizeof(adapt));
    RegSetValueExW(key, L"VisualMeters", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&visualMeters), sizeof(visualMeters));
    RegCloseKey(key);
}

void setInputText(HWND dialog, int inputTenths) {
    wchar_t text[32]{};
    std::swprintf(text, std::size(text), L"%.1f", inputTenths / 10.0);
    SetDlgItemTextW(dialog, IDC_INPUT_DRIVE, text);
}

int peakToMilliDb(double peak) noexcept {
    if (!std::isfinite(peak) || peak <= 0.000001) {
        return meterFloorMilliDb;
    }
    return static_cast<int>(std::lround(20000.0 * std::log10(std::min(peak, 1.0))));
}

void publishPeak(std::atomic<int>& target, int value) noexcept {
    int current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

double scratchPeak(const float* samples, std::size_t count) noexcept {
    double peak = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const float value = samples[index];
        if (std::isfinite(value)) {
            peak = std::max(peak, std::abs(static_cast<double>(value)));
        }
    }
    return peak;
}

int fullScaleSamples(const float* samples, std::size_t count) noexcept {
    int result = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const float value = samples[index];
        if (std::isfinite(value) && std::abs(value) >= 0.999) {
            ++result;
        }
    }
    return result;
}

void setPeakText(HWND dialog, int control, const wchar_t* label, int milliDb) {
    wchar_t text[80]{};
    if (milliDb <= meterFloorMilliDb) {
        std::swprintf(text, std::size(text), L"%ls: no signal", label);
    } else {
        std::swprintf(text, std::size(text), L"%ls: %.1f dBFS", label, milliDb / 1000.0);
    }
    SetDlgItemTextW(dialog, control, text);
}

int meterPosition(int milliDb) noexcept {
    if (milliDb <= meterFloorMilliDb) {
        return 0;
    }
    return std::clamp(milliDb - meterFloorMilliDb, 0, -meterFloorMilliDb);
}

void setMeterVisibility(HWND dialog, bool enabled) {
    ShowWindow(GetDlgItem(dialog, IDC_INPUT_METER), enabled ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(dialog, IDC_OUTPUT_METER), enabled ? SW_SHOW : SW_HIDE);
}

void setMetersOffText(HWND dialog) {
    SetDlgItemTextW(dialog, IDC_INPUT_PEAK, L"Input peak: visual meters off");
    SetDlgItemTextW(dialog, IDC_OUTPUT_PEAK, L"Output peak: visual meters off");
    SetDlgItemTextW(dialog, IDC_FULL_SCALE, L"Full scale: visual meters off");
    SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETPOS, 0, 0);
    SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETPOS, 0, 0);
}

void updateMeters(HWND dialog) {
    const auto* settings = reinterpret_cast<DialogSettings*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (!settings || !settings->visualMeters) {
        setMetersOffText(dialog);
        return;
    }

    const int input = meterInputMilliDb.exchange(meterFloorMilliDb, std::memory_order_relaxed);
    const int output = meterOutputMilliDb.exchange(meterFloorMilliDb, std::memory_order_relaxed);
    const int fullScale = meterFullScaleSamples.exchange(0, std::memory_order_relaxed);

    setPeakText(dialog, IDC_INPUT_PEAK, L"Input peak", input);
    setPeakText(dialog, IDC_OUTPUT_PEAK, L"Output peak", output);
    SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETPOS,
                        static_cast<WPARAM>(meterPosition(input)), 0);
    SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETPOS,
                        static_cast<WPARAM>(meterPosition(output)), 0);

    wchar_t text[80]{};
    if (fullScale > 0) {
        std::swprintf(text, std::size(text), L"Full scale: %d recent samples", fullScale);
    } else {
        std::swprintf(text, std::size(text), L"Full scale: none");
    }
    SetDlgItemTextW(dialog, IDC_FULL_SCALE, text);
}

void populateDialog(HWND dialog, DialogSettings& settings) {
    HWND mode = GetDlgItem(dialog, IDC_MODE);
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Podcast Leveler"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stream polish"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Smooth Limiter"));
    SendMessageW(mode, CB_SETCURSEL, static_cast<WPARAM>(settings.mode), 0);
    setInputText(dialog, settings.inputTenths);
    SetDlgItemInt(dialog, IDC_AUTO_ADAPT, static_cast<UINT>(settings.adaptPercent), FALSE);
    CheckDlgButton(dialog, IDC_VISUAL_METERS, settings.visualMeters ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETRANGE32, 0,
                        static_cast<LPARAM>(-meterFloorMilliDb));
    SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETRANGE32, 0,
                        static_cast<LPARAM>(-meterFloorMilliDb));
    setMeterVisibility(dialog, settings.visualMeters);
    updateMeters(dialog);
}

bool parseNumber(HWND dialog, int control, double minimum, double maximum,
                 double& result, const wchar_t* fieldName) {
    wchar_t text[64]{};
    GetDlgItemTextW(dialog, control, text, static_cast<int>(std::size(text)));
    wchar_t* end = nullptr;
    const double value = std::wcstod(text, &end);
    while (end && *end == L' ') {
        ++end;
    }
    if (end == text || (end && *end != L'\0') || !std::isfinite(value) ||
        value < minimum || value > maximum) {
        wchar_t message[160]{};
        std::swprintf(message, std::size(message), L"%ls must be between %.1f and %.1f.",
                      fieldName, minimum, maximum);
        MessageBoxW(dialog, message, L"OptiLab Core", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, control));
        SendDlgItemMessageW(dialog, control, EM_SETSEL, 0, -1);
        return false;
    }
    result = value;
    return true;
}

INT_PTR CALLBACK configDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* settings = reinterpret_cast<DialogSettings*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        settings = reinterpret_cast<DialogSettings*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(settings));
        populateDialog(dialog, *settings);
        SetTimer(dialog, meterTimerId, meterTimerMs, nullptr);
        return TRUE;
    }
    if (message == WM_TIMER && wParam == meterTimerId) {
        updateMeters(dialog);
        return TRUE;
    }
    if (message == WM_DESTROY) {
        KillTimer(dialog, meterTimerId);
        return FALSE;
    }
    if (message != WM_COMMAND || !settings) {
        return FALSE;
    }

    const int control = LOWORD(wParam);
    if (control == IDC_MODE && HIWORD(wParam) == CBN_SELCHANGE) {
        const int mode = static_cast<int>(SendDlgItemMessageW(dialog, IDC_MODE, CB_GETCURSEL, 0, 0));
        if (mode >= 0 && mode <= 2) {
            settings->mode = mode;
            settings->inputTenths = defaultInputTenths(mode);
            setInputText(dialog, settings->inputTenths);
        }
        return TRUE;
    }
    if (control == IDC_RESTORE_DEFAULTS) {
        settings->mode = 0;
        settings->inputTenths = 35;
        settings->adaptPercent = 0;
        settings->visualMeters = false;
        SendDlgItemMessageW(dialog, IDC_MODE, CB_SETCURSEL, 0, 0);
        setInputText(dialog, settings->inputTenths);
        SetDlgItemInt(dialog, IDC_AUTO_ADAPT, 0, FALSE);
        CheckDlgButton(dialog, IDC_VISUAL_METERS, BST_UNCHECKED);
        setMeterVisibility(dialog, false);
        updateMeters(dialog);
        return TRUE;
    }
    if (control == IDC_VISUAL_METERS && HIWORD(wParam) == BN_CLICKED) {
        settings->visualMeters = IsDlgButtonChecked(dialog, IDC_VISUAL_METERS) == BST_CHECKED;
        setMeterVisibility(dialog, settings->visualMeters);
        updateMeters(dialog);
        return TRUE;
    }
    if (control == IDOK) {
        double input = 0.0;
        double adapt = 0.0;
        if (!parseNumber(dialog, IDC_INPUT_DRIVE, -12.0, 18.0, input, L"Input drive") ||
            !parseNumber(dialog, IDC_AUTO_ADAPT, 0.0, 100.0, adapt, L"Auto-adapt")) {
            return TRUE;
        }
        settings->mode = static_cast<int>(SendDlgItemMessageW(dialog, IDC_MODE, CB_GETCURSEL, 0, 0));
        settings->inputTenths = static_cast<int>(std::lround(input * 10.0));
        settings->adaptPercent = static_cast<int>(std::lround(adapt));
        settings->visualMeters = IsDlgButtonChecked(dialog, IDC_VISUAL_METERS) == BST_CHECKED;
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (control == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void configure(WinampDspModule* module) {
    ensureSettingsLoaded();
    DialogSettings settings{
        targetMode.load(std::memory_order_relaxed),
        targetInputTenths.load(std::memory_order_relaxed),
        targetAdaptPercent.load(std::memory_order_relaxed),
        targetVisualMeters.load(std::memory_order_relaxed) != 0,
    };
    const HWND parent = module ? module->hwndParent : nullptr;
    if (DialogBoxParamW(instanceHandle, MAKEINTRESOURCEW(IDD_OPTILAB_CONFIG), parent,
                        configDialogProc, reinterpret_cast<LPARAM>(&settings)) == IDOK) {
        targetMode.store(settings.mode, std::memory_order_relaxed);
        targetInputTenths.store(settings.inputTenths, std::memory_order_relaxed);
        targetAdaptPercent.store(settings.adaptPercent, std::memory_order_relaxed);
        targetVisualMeters.store(settings.visualMeters ? 1 : 0, std::memory_order_relaxed);
        saveSettings(settings);
    }
}

int initialize(WinampDspModule* module) {
    ensureSettingsLoaded();
    processor.prepared = false;
    processor.appliedMode = -1;
    if (module) {
        module->userData = &processor;
    }
    return 0;
}

void applySettings(ProcessorState& state) {
    const int mode = std::clamp(targetMode.load(std::memory_order_relaxed), 0, 2);
    const int input = std::clamp(targetInputTenths.load(std::memory_order_relaxed), -120, 180);
    const int adapt = std::clamp(targetAdaptPercent.load(std::memory_order_relaxed), 0, 100);
    if (mode == state.appliedMode && input == state.appliedInputTenths &&
        adapt == state.appliedAdaptPercent) {
        return;
    }
    OptiLabCore::Parameters parameters;
    parameters.mode = static_cast<OptiLabCore::Mode>(mode);
    parameters.inputDriveDb = input / 10.0;
    parameters.autoAdaptPct = static_cast<double>(adapt);
    state.core.setParameters(parameters);
    state.appliedMode = mode;
    state.appliedInputTenths = input;
    state.appliedAdaptPercent = adapt;
}

int modifySamples(WinampDspModule* module, short* samples, int frames, int bitsPerSample,
                  int channels, int sampleRate) {
    if (!samples || frames <= 0 || sampleRate <= 0 || channels < 1 || channels > 2 ||
        optilab::winamp::bytesPerSample(bitsPerSample) == 0) {
        return frames;
    }
    auto* state = module && module->userData
                      ? static_cast<ProcessorState*>(module->userData)
                      : &processor;
    if (!state->prepared || state->sampleRate != sampleRate) {
        state->core.prepare(static_cast<double>(sampleRate));
        state->sampleRate = sampleRate;
        state->prepared = true;
        state->appliedMode = -1;
    }
    applySettings(*state);

    auto* bytes = reinterpret_cast<std::uint8_t*>(samples);
    const std::size_t frameBytes = optilab::winamp::bytesPerSample(bitsPerSample) *
                                   static_cast<std::size_t>(channels);
    std::size_t offset = 0;
    while (offset < static_cast<std::size_t>(frames)) {
        const std::size_t block = std::min(scratchFrames, static_cast<std::size_t>(frames) - offset);
        const std::size_t blockSamples = block * static_cast<std::size_t>(channels);
        std::uint8_t* blockBytes = bytes + offset * frameBytes;
        if (bitsPerSample == 16) {
            const auto* blockPcm = samples + offset * static_cast<std::size_t>(channels);
            optilab::winamp::decodeInt16(blockPcm, blockSamples, state->scratch.data());
        } else {
            optilab::winamp::decodeInterleaved(blockBytes, block, channels, bitsPerSample,
                                                state->scratch.data());
        }
        const double inputPeak = scratchPeak(state->scratch.data(), blockSamples);
        state->core.processInterleaved(state->scratch.data(), block, static_cast<std::size_t>(channels));
        const double outputPeak = scratchPeak(state->scratch.data(), blockSamples);
        publishPeak(meterInputMilliDb, peakToMilliDb(inputPeak));
        publishPeak(meterOutputMilliDb, peakToMilliDb(outputPeak));
        const int fullScale = fullScaleSamples(state->scratch.data(), blockSamples);
        if (fullScale > 0) {
            meterFullScaleSamples.fetch_add(fullScale, std::memory_order_relaxed);
        }
        if (bitsPerSample == 16) {
            auto* blockPcm = samples + offset * static_cast<std::size_t>(channels);
            optilab::winamp::encodeInt16(state->scratch.data(), blockSamples, blockPcm);
        } else {
            optilab::winamp::encodeInterleaved(state->scratch.data(), block, channels, bitsPerSample,
                                                blockBytes);
        }
        offset += block;
    }
    return frames;
}

void shutdown(WinampDspModule* module) {
    processor.prepared = false;
    processor.appliedMode = -1;
    if (module) {
        module->userData = nullptr;
    }
}

char moduleDescription[] = "OptiLab Core " OPTILAB_VERSION_STRING;
char headerDescription[] = "OptiLab Core DSP/Effect";

WinampDspModule module{
    moduleDescription,
    nullptr,
    nullptr,
    configure,
    initialize,
    modifySamples,
    shutdown,
    nullptr,
};

WinampDspModule* getModule(int index) {
    return index == 0 ? &module : nullptr;
}

WinampDspHeader header{
    winampDspHeaderVersion,
    headerDescription,
    getModule,
};

} // namespace

extern "C" WinampDspHeader* winampDSPGetHeader2() {
    return &header;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        instanceHandle = instance;
        module.hDllInstance = instance;
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&controls);
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

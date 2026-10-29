// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.



#include "OptiLabCore.h"
#include "OptiLabVersion.h"
#include "WinampDspApi.h"
#include "WinampPcm.h"
#include "WinampDelivery.h"
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
#include <utility>

namespace {

constexpr std::size_t scratchFrames = 1024;
constexpr UINT_PTR meterTimerId = 1;
constexpr UINT meterTimerMs = 150;
constexpr int meterFloorMilliDb = -120000;

HINSTANCE instanceHandle = nullptr;
std::once_flag settingsLoadFlag;
std::atomic<int> targetMode{0};
std::atomic<int> targetInputTenths{0};
std::atomic<int> targetAdaptPercent{0};
std::atomic<int> targetVisualMeters{0};
std::atomic<int> meterInputMilliDb{meterFloorMilliDb};
std::atomic<int> meterOutputMilliDb{meterFloorMilliDb};
std::atomic<int> meterFullScaleSamples{0};

struct DialogSettings {
    int mode = 0;
    int inputTenths = 0;
    int adaptPercent = 0;
    bool visualMeters = false;
};

struct ProcessorState {
    OptiLabCore core;
    std::array<float, scratchFrames * 2> scratch{};
    int sampleRate = 0;
    int channels = 0;
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
                      L"Software\\LanesAudio\\OptiLab Core\\Winamp DSP 1.5\\%ls", host);
        return result;
    }();
    return path.data();
}

int defaultInputTenths(int mode) noexcept {
    return mode == 1 ? 35 : 0;
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
    if (RegGetValueW(HKEY_CURRENT_USER, registryPath, L"InputTenthsSymmetric", RRF_RT_REG_DWORD,
                     nullptr, &value, &size) == ERROR_SUCCESS && value <= 360) {
        targetInputTenths.store(static_cast<int>(value) - 180, std::memory_order_relaxed);
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
    const DWORD input = static_cast<DWORD>(settings.inputTenths + 180);
    const DWORD adapt = static_cast<DWORD>(settings.adaptPercent);
    const DWORD visualMeters = settings.visualMeters ? 1u : 0u;
    RegSetValueExW(key, L"Mode", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&mode), sizeof(mode));
    RegSetValueExW(key, L"InputTenthsSymmetric", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&input), sizeof(input));
    RegSetValueExW(key, L"AdaptPercent", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&adapt), sizeof(adapt));
    RegSetValueExW(key, L"VisualMeters", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&visualMeters), sizeof(visualMeters));
    RegCloseKey(key);
}

void setInputDrive(HWND dialog, int inputTenths) {
    inputTenths = std::clamp(inputTenths, -180, 180);
    SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETPOS, TRUE,
                        static_cast<LPARAM>(inputTenths + 180));
    wchar_t text[32]{};
    std::swprintf(text, std::size(text), L"%+.1f dB", inputTenths / 10.0);
    SetDlgItemTextW(dialog, IDC_INPUT_DRIVE_VALUE, text);
}

void setAutoAdapt(HWND dialog, int percent) {
    percent = std::clamp(percent, 0, 100);
    SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_SETPOS, TRUE,
                        static_cast<LPARAM>(percent));
    wchar_t text[32]{};
    std::swprintf(text, std::size(text), L"%d%%", percent);
    SetDlgItemTextW(dialog, IDC_AUTO_ADAPT_VALUE, text);
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
    wchar_t current[80]{};
    GetDlgItemTextW(dialog, control, current, static_cast<int>(std::size(current)));
    if (std::wcscmp(current, text) != 0) {
        SetDlgItemTextW(dialog, control, text);
    }
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
    constexpr std::array<std::pair<int, const wchar_t*>, 3> labels{{
        {IDC_INPUT_PEAK, L"Input peak: visual meters off"},
        {IDC_OUTPUT_PEAK, L"Output peak: visual meters off"},
        {IDC_FULL_SCALE, L"Full scale: visual meters off"},
    }};
    for (const auto& [control, text] : labels) {
        wchar_t current[80]{};
        GetDlgItemTextW(dialog, control, current, static_cast<int>(std::size(current)));
        if (std::wcscmp(current, text) != 0) {
            SetDlgItemTextW(dialog, control, text);
        }
    }
    if (SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_GETPOS, 0, 0) != 0) {
        SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETPOS, 0, 0);
    }
    if (SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_GETPOS, 0, 0) != 0) {
        SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETPOS, 0, 0);
    }
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
    const int inputPosition = meterPosition(input);
    const int outputPosition = meterPosition(output);
    if (SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_GETPOS, 0, 0) != inputPosition) {
        SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETPOS,
                            static_cast<WPARAM>(inputPosition), 0);
    }
    if (SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_GETPOS, 0, 0) != outputPosition) {
        SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETPOS,
                            static_cast<WPARAM>(outputPosition), 0);
    }

    wchar_t text[80]{};
    if (fullScale > 0) {
        std::swprintf(text, std::size(text), L"Full scale: %d recent samples", fullScale);
    } else {
        std::swprintf(text, std::size(text), L"Full scale: none");
    }
    wchar_t current[80]{};
    GetDlgItemTextW(dialog, IDC_FULL_SCALE, current, static_cast<int>(std::size(current)));
    if (std::wcscmp(current, text) != 0) {
        SetDlgItemTextW(dialog, IDC_FULL_SCALE, text);
    }
}

void populateDialog(HWND dialog, DialogSettings& settings) {
    HWND mode = GetDlgItem(dialog, IDC_MODE);
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stream polish"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Podcast Leveler"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Smooth Limiter"));
    SendMessageW(mode, CB_SETCURSEL, static_cast<WPARAM>(settings.mode), 0);
    SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETRANGE, TRUE, MAKELONG(0, 360));
    SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETTICFREQ, 50, 0);
    SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETPAGESIZE, 0, 10);
    SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_SETTICFREQ, 10, 0);
    SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_SETPAGESIZE, 0, 10);
    setInputDrive(dialog, settings.inputTenths);
    setAutoAdapt(dialog, settings.adaptPercent);
    CheckDlgButton(dialog, IDC_VISUAL_METERS, settings.visualMeters ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(dialog, IDC_INPUT_METER, PBM_SETRANGE32, 0,
                        static_cast<LPARAM>(-meterFloorMilliDb));
    SendDlgItemMessageW(dialog, IDC_OUTPUT_METER, PBM_SETRANGE32, 0,
                        static_cast<LPARAM>(-meterFloorMilliDb));
    setMeterVisibility(dialog, settings.visualMeters);
    updateMeters(dialog);
}

INT_PTR CALLBACK configDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* settings = reinterpret_cast<DialogSettings*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        settings = reinterpret_cast<DialogSettings*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(settings));
        populateDialog(dialog, *settings);
        if (settings->visualMeters) {
            SetTimer(dialog, meterTimerId, meterTimerMs, nullptr);
        }
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
    if (message == WM_HSCROLL && settings) {
        const HWND control = reinterpret_cast<HWND>(lParam);
        if (control == GetDlgItem(dialog, IDC_INPUT_DRIVE)) {
            settings->inputTenths = static_cast<int>(SendDlgItemMessageW(
                dialog, IDC_INPUT_DRIVE, TBM_GETPOS, 0, 0)) - 180;
            setInputDrive(dialog, settings->inputTenths);
            return TRUE;
        }
        if (control == GetDlgItem(dialog, IDC_AUTO_ADAPT)) {
            settings->adaptPercent = static_cast<int>(SendDlgItemMessageW(
                dialog, IDC_AUTO_ADAPT, TBM_GETPOS, 0, 0));
            setAutoAdapt(dialog, settings->adaptPercent);
            return TRUE;
        }
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
            setInputDrive(dialog, settings->inputTenths);
        }
        return TRUE;
    }
    if (control == IDC_RESTORE_DEFAULTS) {
        settings->mode = 0;
        settings->inputTenths = 0;
        settings->adaptPercent = 0;
        settings->visualMeters = false;
        targetVisualMeters.store(0, std::memory_order_relaxed);
        SendDlgItemMessageW(dialog, IDC_MODE, CB_SETCURSEL, 0, 0);
        setInputDrive(dialog, settings->inputTenths);
        setAutoAdapt(dialog, settings->adaptPercent);
        CheckDlgButton(dialog, IDC_VISUAL_METERS, BST_UNCHECKED);
        KillTimer(dialog, meterTimerId);
        setMeterVisibility(dialog, false);
        updateMeters(dialog);
        return TRUE;
    }
    if (control == IDC_VISUAL_METERS && HIWORD(wParam) == BN_CLICKED) {
        settings->visualMeters = IsDlgButtonChecked(dialog, IDC_VISUAL_METERS) == BST_CHECKED;
        targetVisualMeters.store(settings->visualMeters ? 1 : 0, std::memory_order_relaxed);
        if (settings->visualMeters) {
            SetTimer(dialog, meterTimerId, meterTimerMs, nullptr);
        } else {
            KillTimer(dialog, meterTimerId);
        }
        setMeterVisibility(dialog, settings->visualMeters);
        updateMeters(dialog);
        return TRUE;
    }
    if (control == IDOK) {
        settings->mode = static_cast<int>(SendDlgItemMessageW(dialog, IDC_MODE, CB_GETCURSEL, 0, 0));
        settings->inputTenths = static_cast<int>(SendDlgItemMessageW(
            dialog, IDC_INPUT_DRIVE, TBM_GETPOS, 0, 0)) - 180;
        settings->adaptPercent = static_cast<int>(SendDlgItemMessageW(
            dialog, IDC_AUTO_ADAPT, TBM_GETPOS, 0, 0));
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
    const int originalVisualMeters = targetVisualMeters.load(std::memory_order_relaxed);
    DialogSettings settings{
        targetMode.load(std::memory_order_relaxed),
        targetInputTenths.load(std::memory_order_relaxed),
        targetAdaptPercent.load(std::memory_order_relaxed),
        originalVisualMeters != 0,
    };

    (void)module;
    const INT_PTR result = DialogBoxParamW(
        instanceHandle, MAKEINTRESOURCEW(IDD_OPTILAB_CONFIG), nullptr,
        configDialogProc, reinterpret_cast<LPARAM>(&settings));
    if (result == IDOK) {
        targetMode.store(settings.mode, std::memory_order_relaxed);
        targetInputTenths.store(settings.inputTenths, std::memory_order_relaxed);
        targetAdaptPercent.store(settings.adaptPercent, std::memory_order_relaxed);
        targetVisualMeters.store(settings.visualMeters ? 1 : 0, std::memory_order_relaxed);
        saveSettings(settings);
    } else {
        targetVisualMeters.store(originalVisualMeters, std::memory_order_relaxed);
    }
}

int initialize(WinampDspModule* module) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    ensureSettingsLoaded();
    processor.prepared = false;
    processor.appliedMode = -1;
    if (module) {
        module->userData = &processor;
    }
    return 0;
}

void applySettings(ProcessorState& state) {
    const int mode = static_cast<int>(OptiLabCore::sanitizeMode(targetMode.load(std::memory_order_relaxed)));
    const int input = std::clamp(targetInputTenths.load(std::memory_order_relaxed), -180, 180);
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
    if (!samples || frames <= 0) {
        return frames;
    }
    auto* state = module && module->userData
                      ? static_cast<ProcessorState*>(module->userData)
                      : &processor;
    if (channels < 1 || channels > 2 ||
        optilab::winamp::bytesPerSample(bitsPerSample) == 0 ||
        !OptiLabCoreEngine::isSampleRateSupported(sampleRate)) {
        state->prepared = false;
        return frames; // Unsupported formats pass through without crossing the DLL ABI with an exception.
    }
    if (!state->prepared || state->sampleRate != sampleRate || state->channels != channels) {
        state->prepared = false;
        try {
            applySettings(*state);
            state->core.prepare(static_cast<double>(sampleRate));
        } catch (const std::exception&) {
            return frames; // A failed preparation must not terminate the host.
        }
        state->sampleRate = sampleRate;
        state->channels = channels;
        state->prepared = true;
    } else {
        applySettings(*state);
    }

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
        const bool trackMeters =
            targetVisualMeters.load(std::memory_order_relaxed) != 0;
        const double inputPeak = trackMeters
                                     ? scratchPeak(state->scratch.data(), blockSamples)
                                     : 0.0;
        state->core.processInterleaved(state->scratch.data(), block, static_cast<std::size_t>(channels));
        optilab::winamp::limitDelivery(state->scratch.data(), blockSamples);
        if (trackMeters) {
            const double outputPeak = scratchPeak(state->scratch.data(), blockSamples);
            publishPeak(meterInputMilliDb, peakToMilliDb(inputPeak));
            publishPeak(meterOutputMilliDb, peakToMilliDb(outputPeak));
            const int fullScale = fullScaleSamples(state->scratch.data(), blockSamples);
            if (fullScale > 0) {
                meterFullScaleSamples.fetch_add(fullScale, std::memory_order_relaxed);
            }
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

}

extern "C" WinampDspHeader* winampDSPGetHeader2() {
    return &header;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        instanceHandle = instance;
        module.hDllInstance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

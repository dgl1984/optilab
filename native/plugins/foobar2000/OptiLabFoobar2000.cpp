// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <objidl.h>

#include <SDK/foobar2000.h>

#include "OptiLabCore.h"
#include "OptiLabVersion.h"
#include "FoobarCoreParameterAdapter.h"
#include "resource.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <iterator>

namespace {

constexpr GUID componentGuid = {
    0xd0b1e8b4, 0x0f9e, 0x4a23, {0x89, 0xf1, 0xaa, 0xd8, 0xa5, 0x11, 0x99, 0x23}};

struct PresetData {
    std::int32_t version = 1;
    std::int32_t mode = 0;
    std::int32_t inputTenths = 35;
    std::int32_t adaptPercent = 0;
};

static_assert(sizeof(PresetData) == 16, "The foobar2000 preset format must remain stable.");

PresetData sanitized(PresetData value) noexcept {
    if (value.version != 1) {
        return {};
    }
    value.mode = std::clamp(value.mode, 0, 2);
    value.inputTenths = std::clamp(value.inputTenths, -120, 180);
    value.adaptPercent = std::clamp(value.adaptPercent, 0, 100);
    return value;
}

PresetData parsePreset(const dsp_preset& preset) noexcept {
    PresetData result{};
    if (preset.get_owner() == componentGuid && preset.get_data_size() == sizeof(result)) {
        std::memcpy(&result, preset.get_data(), sizeof(result));
    }
    return sanitized(result);
}

void makePreset(const PresetData& data, dsp_preset& preset) {
    const PresetData clean = sanitized(data);
    preset.set_owner(componentGuid);
    preset.set_data(&clean, sizeof(clean));
}

OptiLabCore::Parameters coreParameters(const PresetData& data) noexcept {
    OptiLabCore::Parameters result;
    result.mode = static_cast<OptiLabCore::Mode>(data.mode);
    result.inputDriveDb = data.inputTenths / 10.0;
    result.autoAdaptPct = static_cast<double>(data.adaptPercent);
    return result;
}

int defaultInputTenths(int mode) noexcept {
    const auto defaults = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
    return static_cast<int>(std::lround(defaults.inputDriveDb * 10.0));
}

constexpr int inputIndexFromTenths(int tenths) noexcept {
    return 180 - tenths;
}

constexpr int inputTenthsFromIndex(int index) noexcept {
    return 180 - index;
}

static_assert(inputIndexFromTenths(180) == 0);
static_assert(inputIndexFromTenths(-120) == 300);
static_assert(inputTenthsFromIndex(inputIndexFromTenths(35)) == 35);

void populateInputDrive(HWND dialog, int inputTenths) {
    const HWND input = GetDlgItem(dialog, IDC_OPTILAB_INPUT_DRIVE);
    if (SendMessageW(input, CB_GETCOUNT, 0, 0) == 0) {
        // A closed combo box handles Up by selecting the preceding item.
        // Store values high-to-low so Up raises drive and Down lowers it.
        for (int tenths = 180; tenths >= -120; --tenths) {
            wchar_t text[32]{};
            std::swprintf(text, std::size(text), L"%+.1f dB", tenths / 10.0);
            SendMessageW(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        }
    }
    SendMessageW(input, CB_SETCURSEL,
                 static_cast<WPARAM>(inputIndexFromTenths(inputTenths)), 0);
}

void setAutoAdapt(HWND dialog, int percent) {
    percent = std::clamp(percent, 0, 100);
    SendDlgItemMessageW(dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_SETPOS, TRUE,
                        static_cast<LPARAM>(percent));
    wchar_t text[32]{};
    std::swprintf(text, std::size(text), L"%d%%", percent);
    SetDlgItemTextW(dialog, IDC_OPTILAB_AUTO_ADAPT_VALUE, text);
}

void populateDialog(HWND dialog, PresetData& data) {
    const HWND mode = GetDlgItem(dialog, IDC_OPTILAB_MODE);
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Podcast Leveler"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stream polish"));
    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Smooth Limiter"));
    SendMessageW(mode, CB_SETCURSEL, static_cast<WPARAM>(data.mode), 0);
    populateInputDrive(dialog, data.inputTenths);
    SendDlgItemMessageW(dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_SETRANGE, TRUE,
                        MAKELONG(0, 100));
    SendDlgItemMessageW(dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_SETTICFREQ, 10, 0);
    SendDlgItemMessageW(dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_SETPAGESIZE, 0, 10);
    setAutoAdapt(dialog, data.adaptPercent);
}

INT_PTR CALLBACK configDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<PresetData*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        data = reinterpret_cast<PresetData*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(data));
        populateDialog(dialog, *data);
        return TRUE;
    }
    if (message == WM_HSCROLL && data &&
        reinterpret_cast<HWND>(lParam) == GetDlgItem(dialog, IDC_OPTILAB_AUTO_ADAPT)) {
        data->adaptPercent = static_cast<std::int32_t>(SendDlgItemMessageW(
            dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_GETPOS, 0, 0));
        setAutoAdapt(dialog, data->adaptPercent);
        return TRUE;
    }
    if (message != WM_COMMAND || !data) {
        return FALSE;
    }

    const int control = LOWORD(wParam);
    if (control == IDC_OPTILAB_MODE && HIWORD(wParam) == CBN_SELCHANGE) {
        const int selected = static_cast<int>(SendDlgItemMessageW(
            dialog, IDC_OPTILAB_MODE, CB_GETCURSEL, 0, 0));
        if (selected >= 0 && selected <= 2) {
            data->mode = selected;
            data->inputTenths = defaultInputTenths(selected);
            populateInputDrive(dialog, data->inputTenths);
        }
        return TRUE;
    }
    if (control == IDC_OPTILAB_RESTORE_DEFAULTS) {
        *data = {};
        SendDlgItemMessageW(dialog, IDC_OPTILAB_MODE, CB_SETCURSEL, 0, 0);
        populateInputDrive(dialog, data->inputTenths);
        setAutoAdapt(dialog, data->adaptPercent);
        return TRUE;
    }
    if (control == IDOK) {
        data->mode = static_cast<std::int32_t>(SendDlgItemMessageW(
            dialog, IDC_OPTILAB_MODE, CB_GETCURSEL, 0, 0));
        data->inputTenths = inputTenthsFromIndex(static_cast<int>(SendDlgItemMessageW(
            dialog, IDC_OPTILAB_INPUT_DRIVE, CB_GETCURSEL, 0, 0)));
        data->adaptPercent = static_cast<std::int32_t>(SendDlgItemMessageW(
            dialog, IDC_OPTILAB_AUTO_ADAPT, TBM_GETPOS, 0, 0));
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (control == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

class OptiLabFoobar2000 : public dsp_impl_base_t<dsp_v3> {
public:
    explicit OptiLabFoobar2000(const dsp_preset& preset)
        : preset_(parsePreset(preset)) {
        applyFoobarCoreParameters(core_, coreParameters(preset_));
    }

    static GUID g_get_guid() { return componentGuid; }
    static void g_get_name(pfc::string_base& output) { output = "OptiLab Core"; }

    static bool g_get_default_preset(dsp_preset& output) {
        makePreset({}, output);
        return true;
    }

    static bool g_have_config_popup() { return true; }

    static void g_show_config_popup(const dsp_preset& preset, HWND parent,
                                    dsp_preset_edit_callback& callback) {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
        InitCommonControlsEx(&controls);
        PresetData edited = parsePreset(preset);
        const INT_PTR result = DialogBoxParamW(
            core_api::get_my_instance(),
            MAKEINTRESOURCEW(IDD_OPTILAB_FOOBAR2000_CONFIG), parent,
            configDialogProc, reinterpret_cast<LPARAM>(&edited));
        if (result == IDOK) {
            dsp_preset_impl updated;
            makePreset(edited, updated);
            callback.on_preset_changed(updated);
        }
    }

    bool on_chunk(audio_chunk* chunk, abort_callback&) override {
        const unsigned channels = chunk->get_channels();
        if (channels < 1 || channels > 2) {
            prepared_ = false;
            return true;
        }
        prepareFor(chunk->get_sample_rate(), channels, chunk->get_channel_config());
        process(chunk->get_data(), chunk->get_sample_count(), channels);
        return true;
    }

    void on_endofplayback(abort_callback&) override {
        if (!prepared_ || latencyFrames_ == 0) {
            return;
        }
        audio_chunk* tail = insert_chunk(latencyFrames_ * channels_);
        tail->set_data_size(latencyFrames_ * channels_);
        tail->set_sample_count(latencyFrames_);
        tail->set_sample_rate(sampleRate_);
        tail->set_channels(channels_, channelConfig_);
        std::fill_n(tail->get_data(), latencyFrames_ * channels_, audio_sample{});
        process(tail->get_data(), latencyFrames_, channels_);
        core_.reset();
    }

    void on_endoftrack(abort_callback&) override {}

    void flush() override {
        if (prepared_) {
            core_.reset();
        }
    }

    double get_latency() override {
        return prepared_ && sampleRate_ > 0
                   ? static_cast<double>(latencyFrames_) / sampleRate_
                   : 0.0;
    }

    bool need_track_change_mark() override { return false; }

    bool apply_preset(const dsp_preset& preset) override {
        preset_ = parsePreset(preset);
        applyFoobarCoreParameters(core_, coreParameters(preset_));
        if (prepared_) {
            latencyFrames_ = core_.latencySamples();
        }
        return true;
    }

private:
    void prepareFor(unsigned sampleRate, unsigned channels, unsigned channelConfig) {
        if (prepared_ && sampleRate_ == sampleRate && channels_ == channels &&
            channelConfig_ == channelConfig) {
            return;
        }
        sampleRate_ = sampleRate;
        channels_ = channels;
        channelConfig_ = channelConfig;
        core_.prepare(static_cast<double>(sampleRate));
        applyFoobarCoreParameters(core_, coreParameters(preset_));
        latencyFrames_ = core_.latencySamples();
        prepared_ = true;
    }

    void process(audio_sample* samples, std::size_t frames, unsigned channels) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t index = frame * channels;
            const float left = static_cast<float>(samples[index]);
            const float right = channels == 2
                                    ? static_cast<float>(samples[index + 1])
                                    : left;
            const auto output = core_.processSample(left, right);
            samples[index] = static_cast<audio_sample>(output.first);
            if (channels == 2) {
                samples[index + 1] = static_cast<audio_sample>(output.second);
            }
        }
    }

    OptiLabCore core_;
    PresetData preset_{};
    unsigned sampleRate_ = 0;
    unsigned channels_ = 0;
    unsigned channelConfig_ = 0;
    std::size_t latencyFrames_ = 0;
    bool prepared_ = false;
};

static dsp_factory_t<OptiLabFoobar2000> componentFactory;

} // namespace

DECLARE_COMPONENT_VERSION(
    "OptiLab Core", OPTILAB_VERSION_STRING,
    "OptiLab Core broadcast processing for foobar2000.\n"
    "Signal processing is provided by the unmodified shared OptiLab Core engine.");

VALIDATE_COMPONENT_FILENAME("foo_optilab_core.dll");

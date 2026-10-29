#include "../native/plugins/winamp/OptiLabWinampDsp.cpp"
#include "OptiLabCore.h"
#include "WinampDelivery.h"
#include "WinampPcm.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}


// Exercise the actual dialog template and callback without opening a host or
// writing the user's saved settings. The dialog ends during initialization.
std::string dialogFailure;
DialogSettings dialogTestSettings;
INT_PTR CALLBACK testDialogProc(HWND dialog, UINT message, WPARAM w, LPARAM l) {
    if (message != WM_INITDIALOG) return configDialogProc(dialog, message, w, l);
    configDialogProc(dialog, message, w, reinterpret_cast<LPARAM>(&dialogTestSettings));
    try {
        wchar_t className[64]{};
        GetClassNameW(GetDlgItem(dialog, IDC_MODE), className, 64);
        require(std::wcscmp(className, L"ComboBox") == 0, "Mode is not a combo box");
        for (int id : {IDC_INPUT_DRIVE, IDC_AUTO_ADAPT}) {
            GetClassNameW(GetDlgItem(dialog, id), className, 64);
            require(std::wcscmp(className, TRACKBAR_CLASSW) == 0, "Continuous control is not a slider");
        }
        require(SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_GETRANGEMIN, 0, 0) == 0 &&
                SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_GETRANGEMAX, 0, 0) == 360,
                "Input slider cannot represent -18 through +18 dB");
        require(SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_GETRANGEMAX, 0, 0) == 100,
                "Adapt slider range changed");
        for (int position : {0, 180, 215, 360}) {
            SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETPOS, TRUE, position);
            configDialogProc(dialog, WM_HSCROLL, TB_THUMBPOSITION,
                             reinterpret_cast<LPARAM>(GetDlgItem(dialog, IDC_INPUT_DRIVE)));
            require(dialogTestSettings.inputTenths == position - 180, "Input slider mapping is wrong");
        }
        setInputDrive(dialog, 0);
        SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, WM_KEYDOWN, VK_RIGHT, 0);
        require(SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_GETPOS, 0, 0) == 181,
                "Input arrow step is not 0.1 dB");
        SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, WM_KEYDOWN, VK_NEXT, 0);
        require(SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_GETPOS, 0, 0) == 191,
                "Input page step is not 1 dB");
        SendDlgItemMessageW(dialog, IDC_MODE, CB_SETCURSEL, 1, 0);
        configDialogProc(dialog, WM_COMMAND, MAKEWPARAM(IDC_MODE, CBN_SELCHANGE), 0);
        require(dialogTestSettings.inputTenths == 35 &&
                SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_GETPOS, 0, 0) == 215,
                "Podcast default did not reach the input slider");
        wchar_t text[32]{};
        GetDlgItemTextW(dialog, IDC_INPUT_DRIVE_VALUE, text, 32);
        require(std::wcscmp(text, L"+3.5 dB") == 0, "Input readout lost its physical value");
        configDialogProc(dialog, WM_COMMAND, IDC_RESTORE_DEFAULTS, 0);
        require(dialogTestSettings.mode == 0 && dialogTestSettings.inputTenths == 0 &&
                dialogTestSettings.adaptPercent == 0, "Restore Defaults failed");
        SendDlgItemMessageW(dialog, IDC_INPUT_DRIVE, TBM_SETPOS, TRUE, 0);
        SendDlgItemMessageW(dialog, IDC_AUTO_ADAPT, TBM_SETPOS, TRUE, 100);
        configDialogProc(dialog, WM_COMMAND, IDOK, 0);
    } catch (const std::exception& e) {
        dialogFailure = e.what();
        EndDialog(dialog, IDCANCEL);
    }
    return TRUE;
}
void testNativeSliders() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
    require(InitCommonControlsEx(&controls) != FALSE, "Common controls failed to initialize");
    const INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_OPTILAB_CONFIG), nullptr, testDialogProc, 0);
    if (!dialogFailure.empty()) throw std::runtime_error(dialogFailure);
    require(result == IDOK, "Native slider dialog failed to create or accept");
    require(dialogTestSettings.inputTenths == -180 && dialogTestSettings.adaptPercent == 100,
            "OK did not read the slider endpoints");
}

void testActualCallback() {
    ProcessorState local;
    WinampDspModule host{};
    host.userData = &local;
    std::array<short, 256> samples{};
    samples.fill(1234);
    for (int rate : {-1, 0, 4000, 7999, 768001, 1536000}) {
        const auto original = samples;
        require(modifySamples(&host, samples.data(), 128, 16, 2, rate) == 128,
                "Unsupported rate changed the host frame count");
        require(samples == original, "Unsupported rate did not pass through unchanged");
        require(!local.prepared, "Unsupported rate retained prepared state");
    }
    for (int channels : {2, 1, 2}) {
        samples.fill(1234);
        require(modifySamples(&host, samples.data(), 128, 16, channels, 48000) == 128,
                "Supported callback changed the host frame count");
        require(local.prepared && local.channels == channels,
                "Callback did not prepare for channel-layout change");
        for (int i = 0; i < 128 * channels; ++i)
            require(samples[i] == 0, "Channel-layout change leaked the previous delay history");
        bool heardAudio = false;
        for (int block = 0; block < 16; ++block) {
            samples.fill(1234);
            modifySamples(&host, samples.data(), 128, 16, channels, 48000);
            for (int i = 0; i < 128 * channels; ++i) heardAudio |= samples[i] != 0;
        }
        require(heardAudio, "Supported callback never produced audio after its delay");
    }
    samples.fill(1234);
    modifySamples(&host, samples.data(), 128, 16, 2, 4000);
    samples.fill(1234);
    modifySamples(&host, samples.data(), 128, 16, 2, 48000);
    require(local.prepared, "Supported rate did not recover after unsupported audio");
    for (short sample : samples)
        require(sample == 0, "Rate recovery leaked the previous delay history");
}

int main() {
    try {
        testNativeSliders();
        testActualCallback();
        constexpr int channels = 2;
        constexpr std::size_t frames = 4096;
        std::vector<std::int16_t> pcm(frames * channels);
        for (std::size_t i = 0; i < frames; ++i) {
            const double s = 0.95 * std::sin(2.0 * 3.141592653589793 * 997.0 * i / 48000.0);
            pcm[2*i] = static_cast<std::int16_t>(std::lround(s * 32767.0));
            pcm[2*i+1] = static_cast<std::int16_t>(std::lround(-s * 32767.0));
        }

        std::vector<float> f(pcm.size());
        optilab::winamp::decodeInt16(pcm.data(), pcm.size(), f.data());

        OptiLabCore core;
        OptiLabCore::Parameters p;
        p.mode = OptiLabCore::Mode::StreamPolish;
        p.inputDriveDb = 12.0;
        p.autoAdaptPct = 100.0;
        core.setParameters(p);
        core.prepare(48000.0);
        core.processInterleaved(f.data(), frames, channels);
        optilab::winamp::limitDelivery(f.data(), f.size());
        for (float v : f) {
            require(std::isfinite(v), "Wrapper contract emitted non-finite sample");
            require(std::abs(v) <= optilab::winamp::deliveryCeiling,
                    "Wrapper contract exceeded pre-quantization delivery ceiling");
        }

        require(optilab::winamp::encodeInterleaved(
                    f.data(), frames, channels, 16, reinterpret_cast<std::uint8_t*>(pcm.data())),
                "Winamp 16-bit encode rejected valid buffer");
        std::cout << "OptiLab Winamp wrapper-contract regression: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OptiLab Winamp wrapper-contract regression: FAIL: " << e.what() << '\n';
        return 1;
    }
}

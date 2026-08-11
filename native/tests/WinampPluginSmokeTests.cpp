// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#include "WinampDspApi.h"
#include "OptiLabVersion.h"
#include "resource.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

#define OPTILAB_WIDEN_IMPL(value) L##value
#define OPTILAB_WIDEN(value) OPTILAB_WIDEN_IMPL(value)

std::atomic<int> meterTextWrites{0};
WNDPROC originalStaticWindowProc = nullptr;

LRESULT CALLBACK meterStaticWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_SETTEXT) {
        meterTextWrites.fetch_add(1, std::memory_order_relaxed);
    }
    return CallWindowProcW(originalStaticWindowProc, window, message, wParam, lParam);
}

bool hasExpectedVersionInfo(const wchar_t* path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
    if (size == 0) {
        return false;
    }
    std::vector<std::uint8_t> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) {
        return false;
    }
    struct Translation {
        WORD language;
        WORD codePage;
    };
    Translation* translation = nullptr;
    UINT translationBytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translation), &translationBytes) ||
        translationBytes < sizeof(Translation)) {
        return false;
    }
    wchar_t query[96]{};
    wchar_t* value = nullptr;
    UINT valueLength = 0;
    std::swprintf(query, std::size(query), L"\\StringFileInfo\\%04x%04x\\CompanyName",
                  translation[0].language, translation[0].codePage);
    if (!VerQueryValueW(data.data(), query, reinterpret_cast<void**>(&value), &valueLength) ||
        !value || std::wstring(value) != L"LanesAudio") {
        return false;
    }
    std::swprintf(query, std::size(query), L"\\StringFileInfo\\%04x%04x\\ProductVersion",
                  translation[0].language, translation[0].codePage);
    return VerQueryValueW(data.data(), query, reinterpret_cast<void**>(&value), &valueLength) &&
           value && std::wstring(value) == OPTILAB_WIDEN(OPTILAB_VERSION_STRING);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "FAIL: expected plug-in DLL path\n";
        return 1;
    }
    HMODULE library = LoadLibraryW(argv[1]);
    if (!library) {
        std::cerr << "FAIL: could not load plug-in DLL\n";
        return 1;
    }
    auto getHeader = reinterpret_cast<WinampDspGetHeader2>(
        GetProcAddress(library, "winampDSPGetHeader2"));
    if (!getHeader) {
        std::cerr << "FAIL: winampDSPGetHeader2 export missing\n";
        FreeLibrary(library);
        return 1;
    }
    WinampDspHeader* header = getHeader();
    WinampDspModule* module = header && header->getModule ? header->getModule(0) : nullptr;
    if (!header || header->version != winampDspHeaderVersion || !module ||
        !module->config || !module->init || !module->modifySamples || !module->quit ||
        header->getModule(1)) {
        std::cerr << "FAIL: invalid Winamp DSP header or module\n";
        FreeLibrary(library);
        return 1;
    }
    const std::string expectedDescription = std::string("OptiLab Core ") + OPTILAB_VERSION_STRING;
    if (!module->description || module->description != expectedDescription ||
        !hasExpectedVersionInfo(argv[1])) {
        std::cerr << "FAIL: plug-in version or LanesAudio metadata missing\n";
        FreeLibrary(library);
        return 1;
    }

    HWND suppliedParent = CreateWindowExW(
        0, L"STATIC", L"Host Preferences", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!suppliedParent) {
        std::cerr << "FAIL: could not create Winamp host test window\n";
        FreeLibrary(library);
        return 1;
    }
    module->hwndParent = suppliedParent;
    std::thread configThread([module] { module->config(module); });
    HWND configDialog = nullptr;
    const std::array<int, 3> meterLabels{IDC_INPUT_PEAK, IDC_OUTPUT_PEAK, IDC_FULL_SCALE};
    for (int attempt = 0; attempt < 100 && !configDialog; ++attempt) {
        configDialog = FindWindowW(L"#32770", L"OptiLab Core Settings");
        if (configDialog) {
            const bool controlsReady = std::all_of(
                meterLabels.begin(), meterLabels.end(),
                [configDialog](int id) { return GetDlgItem(configDialog, id) != nullptr; });
            if (!controlsReady) {
                configDialog = nullptr;
            }
        }
        if (!configDialog) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (!configDialog) {
        std::cerr << "FAIL: Winamp settings dialog did not open\n";
        // The process owns the remaining lifetime in this exceptional path.
        // Do not unload the DLL under a possibly still-running config call.
        configThread.detach();
        DestroyWindow(suppliedParent);
        return 1;
    }
    if (GetWindow(configDialog, GW_OWNER) != nullptr || !IsWindowEnabled(suppliedParent)) {
        std::cerr << "FAIL: Winamp settings dialog is still attached to host Preferences\n";
        PostMessageW(configDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        configThread.join();
        DestroyWindow(suppliedParent);
        FreeLibrary(library);
        return 1;
    }
    if (IsDlgButtonChecked(configDialog, IDC_VISUAL_METERS) == BST_CHECKED) {
        SendDlgItemMessageW(configDialog, IDC_VISUAL_METERS, BM_CLICK, 0, 0);
    }
    for (const int id : meterLabels) {
        const HWND label = GetDlgItem(configDialog, id);
        if (!label) {
            std::cerr << "FAIL: Winamp settings meter label missing\n";
            PostMessageW(configDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            configThread.join();
            DestroyWindow(suppliedParent);
            FreeLibrary(library);
            return 1;
        }
        const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            label, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(meterStaticWindowProc)));
        if (!previous) {
            std::cerr << "FAIL: could not monitor Winamp settings meter label\n";
            PostMessageW(configDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            configThread.join();
            DestroyWindow(suppliedParent);
            FreeLibrary(library);
            return 1;
        }
        if (!originalStaticWindowProc) {
            originalStaticWindowProc = previous;
        }
    }
    meterTextWrites.store(0, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (meterTextWrites.load(std::memory_order_relaxed) != 0) {
        std::cerr << "FAIL: disabled visual meters still rewrite accessible labels\n";
        PostMessageW(configDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        configThread.join();
        DestroyWindow(suppliedParent);
        FreeLibrary(library);
        return 1;
    }
    PostMessageW(configDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
    configThread.join();
    module->hwndParent = nullptr;
    DestroyWindow(suppliedParent);

    constexpr int frames = 2048;
    std::vector<std::int16_t> samples(frames * 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = (i % 4 < 2) ? static_cast<std::int16_t>(12000) : static_cast<std::int16_t>(-9000);
    }
    const auto original = samples;
    if (module->init(module) != 0 ||
        module->modifySamples(module, reinterpret_cast<short*>(samples.data()),
                              frames, 16, 2, 48000) != frames) {
        std::cerr << "FAIL: plug-in processing callback failed\n";
        module->quit(module);
        FreeLibrary(library);
        return 1;
    }

    char* benchmarkSeconds = nullptr;
    std::size_t benchmarkValueLength = 0;
    _dupenv_s(&benchmarkSeconds, &benchmarkValueLength,
              "OPTILAB_BENCHMARK_SECONDS");
    if (benchmarkSeconds) {
        const double requestedSeconds = std::max(1.0, std::atof(benchmarkSeconds));
        const std::uint64_t blocks = static_cast<std::uint64_t>(
            std::ceil(requestedSeconds * 48000.0 / frames));
        std::uint64_t outputHash = 1469598103934665603ull;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t block = 0; block < blocks; ++block) {
            if (module->modifySamples(module, reinterpret_cast<short*>(samples.data()),
                                      frames, 16, 2, 48000) != frames) {
                std::cerr << "FAIL: benchmark processing callback failed\n";
                std::free(benchmarkSeconds);
                module->quit(module);
                FreeLibrary(library);
                return 1;
            }
            for (const std::int16_t sample : samples) {
                const auto bits = static_cast<std::uint16_t>(sample);
                outputHash = (outputHash ^ bits) * 1099511628211ull;
            }
        }
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        const double audioSeconds =
            static_cast<double>(blocks * frames) / 48000.0;
        std::cout << "BENCHMARK audio_seconds=" << audioSeconds
                  << " elapsed_seconds=" << elapsed
                  << " realtime_multiple=" << audioSeconds / elapsed
                  << " output_hash=" << outputHash << '\n';
        std::free(benchmarkSeconds);
    }
    module->quit(module);
    if (samples == original) {
        std::cerr << "FAIL: plug-in did not process the PCM buffer\n";
        FreeLibrary(library);
        return 1;
    }
    FreeLibrary(library);
    std::cout << "Winamp DSP smoke test passed.\n";
    return 0;
}

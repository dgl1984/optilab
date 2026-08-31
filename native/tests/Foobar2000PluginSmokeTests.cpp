// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "FoobarCoreParameterAdapter.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool fail(const char* message) {
    std::cerr << message << '\n';
    return false;
}

bool hasExpectedArchitecture(const wchar_t* path) {
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return fail("Could not open the component for PE inspection.");
    }
    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;
    bool valid = ReadFile(file, &dos, sizeof(dos), &read, nullptr) &&
                 read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE;
    LARGE_INTEGER offset{};
    offset.QuadPart = dos.e_lfanew;
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    valid = valid && SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) &&
            ReadFile(file, &signature, sizeof(signature), &read, nullptr) &&
            read == sizeof(signature) && signature == IMAGE_NT_SIGNATURE &&
            ReadFile(file, &header, sizeof(header), &read, nullptr) &&
            read == sizeof(header);
    CloseHandle(file);
    if (!valid) {
        return fail("Component has an invalid PE header.");
    }
#if defined(_WIN64)
    return header.Machine == IMAGE_FILE_MACHINE_AMD64 || fail("Component is not x64.");
#else
    return header.Machine == IMAGE_FILE_MACHINE_I386 || fail("Component is not Win32.");
#endif
}

bool hasVersionInfo(const wchar_t* path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
    if (size == 0) {
        return fail("Component has no Windows version resource.");
    }
    std::vector<std::uint8_t> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) {
        return fail("Could not read the Windows version resource.");
    }
    return true;
}

bool hasInterfaceExportName(const wchar_t* path) {
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return fail("Could not open the component for export inspection.");
    }
    LARGE_INTEGER size{};
    bool valid = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
                 size.QuadPart <= 16 * 1024 * 1024;
    std::vector<std::uint8_t> data(valid ? static_cast<std::size_t>(size.QuadPart) : 0);
    DWORD read = 0;
    valid = valid && ReadFile(file, data.data(), static_cast<DWORD>(data.size()),
                              &read, nullptr) && read == data.size();
    CloseHandle(file);
    if (!valid) {
        return fail("Could not read the component for export inspection.");
    }
    constexpr char exportName[] = "foobar2000_get_interface";
    const auto* first = reinterpret_cast<const char*>(data.data());
    const auto* last = first + data.size();
    for (const char* cursor = first; cursor + sizeof(exportName) <= last; ++cursor) {
        if (std::memcmp(cursor, exportName, sizeof(exportName)) == 0) {
            return true;
        }
    }
    return fail("foobar2000_get_interface export name is missing.");
}

bool hasNativeSettingsControls(const wchar_t* path) {
    const HMODULE module = LoadLibraryExW(
        path, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) {
        return fail("Could not load component resources.");
    }
    const HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDD_OPTILAB_FOOBAR2000_CONFIG), RT_DIALOG);
    const DWORD size = resource ? SizeofResource(module, resource) : 0;
    const HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
    const auto* bytes = loaded ? static_cast<const std::uint8_t*>(LockResource(loaded)) : nullptr;
    bool foundTrackbar = false;
    constexpr wchar_t trackbarClass[] = TRACKBAR_CLASSW;
    const auto* needle = reinterpret_cast<const std::uint8_t*>(trackbarClass);
    constexpr std::size_t needleSize = sizeof(trackbarClass) - sizeof(wchar_t);
    if (bytes && size >= needleSize) {
        for (DWORD offset = 0; offset + needleSize <= size; ++offset) {
            if (std::memcmp(bytes + offset, needle, needleSize) == 0) {
                foundTrackbar = true;
                break;
            }
        }
    }
    FreeLibrary(module);
    return foundTrackbar || fail("Settings dialog has no native trackbar control.");
}

bool liveModeChangePreservesCustomDrive() {
    OptiLabCore core;
    core.setParameters(OptiLabCore::defaultParameters(
        OptiLabCore::Mode::PodcastLeveler));

    OptiLabCore::Parameters requested;
    requested.mode = OptiLabCore::Mode::StreamPolish;
    requested.inputDriveDb = 7.2;
    requested.autoAdaptPct = 43.0;
    applyFoobarCoreParameters(core, requested);

    const auto applied = core.parameters();
    return (applied.mode == requested.mode &&
            applied.inputDriveDb == requested.inputDriveDb &&
            applied.autoAdaptPct == requested.autoAdaptPct) ||
           fail("A live mode change discarded foobar2000 preset values.");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: smoke-test <foo_optilab_core.dll> [foobar2000-directory]\n";
        return 2;
    }
    if (!hasExpectedArchitecture(argv[1]) || !hasVersionInfo(argv[1]) ||
        !hasInterfaceExportName(argv[1]) || !hasNativeSettingsControls(argv[1]) ||
        !liveModeChangePreservesCustomDrive()) {
        return 1;
    }
    if (argc == 3) {
        if (!SetDllDirectoryW(argv[2])) {
            fail("Could not add the foobar2000 directory to DLL search.");
            return 1;
        }
        const HMODULE module = LoadLibraryW(argv[1]);
        if (!module) {
            std::cerr << "Component could not load with foobar2000 shared.dll: "
                      << GetLastError() << '\n';
            return 1;
        }
        const bool exported = GetProcAddress(module, "foobar2000_get_interface") != nullptr;
        FreeLibrary(module);
        SetDllDirectoryW(nullptr);
        if (!exported) {
            fail("Loaded component does not export foobar2000_get_interface.");
            return 1;
        }
    }
    return 0;
}

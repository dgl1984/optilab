#include "WinampDspApi.h"
#include "OptiLabVersion.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

#define OPTILAB_WIDEN_IMPL(value) L##value
#define OPTILAB_WIDEN(value) OPTILAB_WIDEN_IMPL(value)

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
        !module->init || !module->modifySamples || !module->quit || header->getModule(1)) {
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

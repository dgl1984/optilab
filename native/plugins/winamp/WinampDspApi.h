#pragma once

#include <windows.h>

// Minimal declarations from the classic Winamp DSP/Effect plug-in ABI.
// Keeping this local avoids making the DSP library depend on the legacy SDK.
constexpr int winampDspHeaderVersion = 0x20;

struct WinampDspModule {
    char* description;
    HWND hwndParent;
    HINSTANCE hDllInstance;
    void (*config)(WinampDspModule* module);
    int (*init)(WinampDspModule* module);
    int (*modifySamples)(WinampDspModule* module, short* samples, int frames,
                         int bitsPerSample, int channels, int sampleRate);
    void (*quit)(WinampDspModule* module);
    void* userData;
};

struct WinampDspHeader {
    int version;
    char* description;
    WinampDspModule* (*getModule)(int index);
};

using WinampDspGetHeader2 = WinampDspHeader* (*)();

// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#pragma once

#include <windows.h>

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

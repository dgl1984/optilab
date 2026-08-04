// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: Apache-2.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct WavData {
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::vector<float> samples;
};

WavData readWavFile(const std::string& path);
void writeWavFile(const std::string& path, const WavData& wav);

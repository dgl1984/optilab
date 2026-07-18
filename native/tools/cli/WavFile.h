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

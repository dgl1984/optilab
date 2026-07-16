#pragma once

#include <cstddef>
#include <cstdint>

namespace optilab::winamp {

std::size_t bytesPerSample(int bitsPerSample) noexcept;
void decodeInt16(const std::int16_t* source, std::size_t samples, float* destination) noexcept;
void encodeInt16(const float* source, std::size_t samples, std::int16_t* destination) noexcept;
bool decodeInterleaved(const std::uint8_t* source, std::size_t frames, int channels,
                       int bitsPerSample, float* destination) noexcept;
bool encodeInterleaved(const float* source, std::size_t frames, int channels,
                       int bitsPerSample, std::uint8_t* destination) noexcept;

} // namespace optilab::winamp

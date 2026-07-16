#include "WinampPcm.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace optilab::winamp {
namespace {

float decodeSample(const std::uint8_t* source, int bitsPerSample) noexcept {
    switch (bitsPerSample) {
    case 8:
        return static_cast<float>((static_cast<int>(source[0]) - 128) / 128.0);
    case 16: {
        const auto value = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(source[0]) |
            (static_cast<std::uint16_t>(source[1]) << 8));
        return static_cast<float>(static_cast<double>(value) / 32768.0);
    }
    case 24: {
        std::int32_t value = static_cast<std::int32_t>(source[0]) |
                             (static_cast<std::int32_t>(source[1]) << 8) |
                             (static_cast<std::int32_t>(source[2]) << 16);
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xff000000);
        }
        return static_cast<float>(static_cast<double>(value) / 8388608.0);
    }
    case 32: {
        const std::uint32_t raw = static_cast<std::uint32_t>(source[0]) |
                                  (static_cast<std::uint32_t>(source[1]) << 8) |
                                  (static_cast<std::uint32_t>(source[2]) << 16) |
                                  (static_cast<std::uint32_t>(source[3]) << 24);
        const auto value = static_cast<std::int32_t>(raw);
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    default:
        return 0.0f;
    }
}

void encodeSample(float input, int bitsPerSample, std::uint8_t* destination) noexcept {
    const double value = std::clamp(std::isfinite(input) ? static_cast<double>(input) : 0.0,
                                    -1.0, std::nextafter(1.0, 0.0));
    switch (bitsPerSample) {
    case 8: {
        const int sample = std::clamp(static_cast<int>(std::lround(value * 128.0 + 128.0)), 0, 255);
        destination[0] = static_cast<std::uint8_t>(sample);
        break;
    }
    case 16: {
        const auto sample = static_cast<std::int16_t>(
            std::clamp(static_cast<long>(std::lround(value * 32768.0)), -32768L, 32767L));
        const auto raw = static_cast<std::uint16_t>(sample);
        destination[0] = static_cast<std::uint8_t>(raw & 0xff);
        destination[1] = static_cast<std::uint8_t>((raw >> 8) & 0xff);
        break;
    }
    case 24: {
        const auto sample = static_cast<std::int32_t>(std::clamp(
            static_cast<long>(std::lround(value * 8388608.0)), -8388608L, 8388607L));
        const auto raw = static_cast<std::uint32_t>(sample);
        destination[0] = static_cast<std::uint8_t>(raw & 0xff);
        destination[1] = static_cast<std::uint8_t>((raw >> 8) & 0xff);
        destination[2] = static_cast<std::uint8_t>((raw >> 16) & 0xff);
        break;
    }
    case 32: {
        const auto sample = static_cast<std::int64_t>(std::llround(value * 2147483648.0));
        const auto clamped = static_cast<std::int32_t>(std::clamp(
            sample, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
        const auto raw = static_cast<std::uint32_t>(clamped);
        destination[0] = static_cast<std::uint8_t>(raw & 0xff);
        destination[1] = static_cast<std::uint8_t>((raw >> 8) & 0xff);
        destination[2] = static_cast<std::uint8_t>((raw >> 16) & 0xff);
        destination[3] = static_cast<std::uint8_t>((raw >> 24) & 0xff);
        break;
    }
    default:
        break;
    }
}

} // namespace

std::size_t bytesPerSample(int bitsPerSample) noexcept {
    switch (bitsPerSample) {
    case 8: return 1;
    case 16: return 2;
    case 24: return 3;
    case 32: return 4;
    default: return 0;
    }
}

void decodeInt16(const std::int16_t* source, std::size_t samples, float* destination) noexcept {
    constexpr float scale = 1.0f / 32768.0f;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        destination[sample] = static_cast<float>(source[sample]) * scale;
    }
}

void encodeInt16(const float* source, std::size_t samples, std::int16_t* destination) noexcept {
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const float input = std::isfinite(source[sample]) ? source[sample] : 0.0f;
        const float scaled = std::clamp(input, -1.0f, 1.0f) * 32768.0f;
        const int rounded = static_cast<int>(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
        destination[sample] = static_cast<std::int16_t>(std::clamp(rounded, -32768, 32767));
    }
}

bool decodeInterleaved(const std::uint8_t* source, std::size_t frames, int channels,
                       int bitsPerSample, float* destination) noexcept {
    const std::size_t stride = bytesPerSample(bitsPerSample);
    if (!source || !destination || channels < 1 || channels > 2 || stride == 0) {
        return false;
    }
    const std::size_t samples = frames * static_cast<std::size_t>(channels);
    for (std::size_t sample = 0; sample < samples; ++sample) {
        destination[sample] = decodeSample(source + sample * stride, bitsPerSample);
    }
    return true;
}

bool encodeInterleaved(const float* source, std::size_t frames, int channels,
                       int bitsPerSample, std::uint8_t* destination) noexcept {
    const std::size_t stride = bytesPerSample(bitsPerSample);
    if (!source || !destination || channels < 1 || channels > 2 || stride == 0) {
        return false;
    }
    const std::size_t samples = frames * static_cast<std::size_t>(channels);
    for (std::size_t sample = 0; sample < samples; ++sample) {
        encodeSample(source[sample], bitsPerSample, destination + sample * stride);
    }
    return true;
}

} // namespace optilab::winamp

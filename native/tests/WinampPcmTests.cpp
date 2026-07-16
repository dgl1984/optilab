#include "WinampPcm.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool testFormat(int bitsPerSample) {
    constexpr std::array<float, 10> source{
        -1.0f, -0.875f, -0.5f, -0.125f, 0.0f,
        0.125f, 0.5f, 0.875f, 0.99f, 1.0f,
    };
    const std::size_t width = optilab::winamp::bytesPerSample(bitsPerSample);
    std::vector<std::uint8_t> encoded(source.size() * width);
    std::array<float, source.size()> decoded{};

    if (!optilab::winamp::encodeInterleaved(source.data(), source.size() / 2, 2,
                                             bitsPerSample, encoded.data()) ||
        !optilab::winamp::decodeInterleaved(encoded.data(), source.size() / 2, 2,
                                             bitsPerSample, decoded.data())) {
        return expect(false, "supported PCM format must encode and decode");
    }

    const double tolerance = bitsPerSample == 8 ? 1.0 / 127.0 :
                             bitsPerSample == 16 ? 1.0 / 32767.0 :
                             bitsPerSample == 24 ? 1.0 / 8388607.0 : 2.0 / 2147483647.0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const double expected = std::min(static_cast<double>(source[i]), 1.0 - tolerance);
        if (std::abs(static_cast<double>(decoded[i]) - expected) > tolerance * 1.5) {
            return expect(false, "PCM round trip must stay within one quantization step");
        }
    }
    return true;
}

bool testInt16FastPath() {
    constexpr std::size_t sampleCount = 65536;
    std::vector<std::int16_t> source(sampleCount);
    std::vector<float> decoded(sampleCount);
    std::vector<std::int16_t> encoded(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        source[i] = static_cast<std::int16_t>(static_cast<int>(i) - 32768);
    }

    optilab::winamp::decodeInt16(source.data(), sampleCount, decoded.data());
    optilab::winamp::encodeInt16(decoded.data(), sampleCount, encoded.data());
    return expect(source == encoded, "16-bit fast path must preserve every PCM value");
}

} // namespace

int main() {
    const bool passed =
        expect(optilab::winamp::bytesPerSample(12) == 0, "unsupported PCM width") &&
        testFormat(8) && testFormat(16) && testFormat(24) && testFormat(32) &&
        testInt16FastPath();
    if (passed) {
        std::cout << "Winamp PCM tests passed.\n";
        return 0;
    }
    return 1;
}

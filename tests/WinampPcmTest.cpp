#include "WinampPcm.h"
#include "WinampDelivery.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        const std::array<float, 8> source{-1.0f, -0.75f, -0.1f, 0.0f, 0.1f, 0.5f, 0.9f, 0.999f};
        for (const int bits : {8, 16, 24, 32}) {
            const double ceiling = std::pow(10.0, -0.1 / 20.0);
            require(std::abs(optilab::winamp::deliveryCeilingMagnitude - ceiling) < 1e-15,
                    "Winamp delivery ceiling is not -0.1 dBFS");
            require(std::abs(20 * std::log10(optilab::winamp::deliveryCeiling) + 0.1) < 1e-6,
                    "Winamp float delivery boundary is inaccurate");
            std::array<float, 10> delivery{0, .5f, -.5f, 1, -1, 3, -3,
                std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()};
            optilab::winamp::limitDelivery(delivery.data(), delivery.size());
            for (const float sample : delivery)
                require(std::isfinite(sample) && std::abs(double(sample)) <= ceiling,
                        "Winamp pre-quantization output exceeds -0.1 dBFS");
            require(delivery[1] == .5f && delivery[2] == -.5f,
                    "Winamp delivery guard changed below-ceiling samples");
            const auto bytes = optilab::winamp::bytesPerSample(bits);
            std::vector<std::uint8_t> deliveryEncoded(delivery.size() * bytes);
            require(optilab::winamp::encodeInterleaved(delivery.data(), delivery.size() / 2,
                2, bits, deliveryEncoded.data()), "Guarded PCM encode failed");
            for (int i = -2000; i <= 2000; ++i) {
                float sample = float(i) / 1000.0f;
                optilab::winamp::limitDelivery(&sample, 1);
                require(std::isfinite(sample) && std::abs(double(sample)) <= ceiling,
                        "Winamp delivery sweep exceeded the pre-quantization ceiling");
            }
            require(bytes > 0, "Supported PCM depth reported zero bytes");
            std::vector<std::uint8_t> encoded(source.size() * bytes);
            std::array<float, source.size()> decoded{};
            require(optilab::winamp::encodeInterleaved(source.data(), source.size() / 2, 2, bits, encoded.data()), "PCM encode failed");
            require(optilab::winamp::decodeInterleaved(encoded.data(), source.size() / 2, 2, bits, decoded.data()), "PCM decode failed");
            const float tolerance = bits == 8 ? 0.01f : bits == 16 ? 0.0001f : 0.000002f;
            for (std::size_t i = 0; i < source.size(); ++i)
                require(std::abs(source[i] - decoded[i]) <= tolerance, "PCM round-trip error too large");
        }
        require(optilab::winamp::bytesPerSample(12) == 0, "Unsupported PCM depth must be rejected");
        std::cout << "Winamp PCM public test: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Winamp PCM public test: FAIL: " << e.what() << '\n';
        return 1;
    }
}

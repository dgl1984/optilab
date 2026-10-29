#include "WinampDelivery.h"
#include "WinampPcm.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        using namespace optilab::winamp;
        std::array<float, 9> s{-4.0f, -1.0f, -deliveryCeiling, 0.0f, deliveryCeiling,
                               1.0f, 4.0f, std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()};
        limitDelivery(s.data(), s.size());
        for (float v : s) {
            require(std::isfinite(v), "Delivery clamp emitted non-finite sample");
            require(std::abs(v) <= deliveryCeiling, "Delivery clamp exceeded ceiling");
        }
        require(s[0] == -deliveryCeiling && s[6] == deliveryCeiling, "Delivery clamp missed overload");
        require(s[7] == 0.0f && s[8] == 0.0f, "Delivery clamp did not zero non-finite input");

        std::array<std::int16_t, 9> pcm{};
        encodeInt16(s.data(), s.size(), pcm.data());
        std::array<float, 9> decoded{};
        decodeInt16(pcm.data(), pcm.size(), decoded.data());
        for (float v : decoded) {
            require(std::isfinite(v), "PCM round trip emitted non-finite sample");
            require(std::abs(v) <= 1.0f, "PCM round trip exceeded normalized range");
        }
        std::cout << "OptiLab Winamp delivery regression: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OptiLab Winamp delivery regression: FAIL: " << e.what() << '\n';
        return 1;
    }
}

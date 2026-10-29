#pragma once
#include <array>
#include <cstddef>

// Serialized values are permanent. Append new types; never reorder this enum.
enum class OptiLabStereoType { Average, Transients, Sustain, Adaptive, Count };
inline constexpr std::array<const char*, static_cast<std::size_t>(OptiLabStereoType::Count)>
    optilabStereoTypeNames {"Average", "Transients", "Sustain", "Adaptive"};

inline const char* optilabStereoTypeName(int value) noexcept {
    return value >= 0 && value < static_cast<int>(optilabStereoTypeNames.size())
        ? optilabStereoTypeNames[static_cast<std::size_t>(value)] : nullptr;
}

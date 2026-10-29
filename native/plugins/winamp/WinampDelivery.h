// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace optilab::winamp {
inline constexpr double deliveryCeilingMagnitude = 0.9885530946569389;
// Round downward to float so the pre-encode bound also holds in double precision.
inline constexpr float deliveryCeiling = 0.98855304718017578125f;
static_assert(static_cast<double>(deliveryCeiling) <= deliveryCeilingMagnitude);

inline void limitDelivery(float* samples, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = std::isfinite(samples[i])
            ? std::clamp(samples[i], -deliveryCeiling, deliveryCeiling) : 0.0f;
}
}

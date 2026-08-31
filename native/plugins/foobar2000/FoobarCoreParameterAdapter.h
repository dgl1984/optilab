// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#pragma once

#include "OptiLabCore.h"

inline void applyFoobarCoreParameters(
    OptiLabCore& core, const OptiLabCore::Parameters& parameters) {
    // A mode transition intentionally installs that mode's drive default in the core.
    // Reapply the complete foobar2000 preset so its explicit drive value wins.
    core.setParameters(parameters);
    core.setParameters(parameters);
}

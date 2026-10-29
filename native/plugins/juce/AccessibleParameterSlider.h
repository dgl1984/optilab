// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class AccessibleParameterSlider final : public juce::Slider {
public:
    using Formatter = std::function<juce::String(double)>;
    AccessibleParameterSlider(juce::String name, double minimum, double maximum,
                              double step, double pageStep, Formatter formatter);
    std::function<void()> onRequestExactEntry;
    juce::String formattedValue() const;
    bool keyPressed(const juce::KeyPress& key) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    double pageStep_ = 1.0;
    Formatter formatter_;
};

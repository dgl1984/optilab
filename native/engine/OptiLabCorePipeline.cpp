#include "OptiLabCorePipeline.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

void OptiLabCorePipeline::MonitorState::set(Section section, SectionMode mode) noexcept {
    const auto index = static_cast<std::size_t>(section);
    if (index >= sections.size()) return;
    // Final is already the end of the chain, so its useful diagnostic is Delta.
    if (section == Section::Final && mode == SectionMode::Testpoint) {
        mode = SectionMode::Delta;
    }
    if (mode == SectionMode::Testpoint || mode == SectionMode::Delta) {
        for (auto& current : sections) {
            if (current == SectionMode::Testpoint || current == SectionMode::Delta) {
                current = SectionMode::Active;
            }
        }
    }
    sections[index] = mode;
}

OptiLabCorePipeline::SectionMode OptiLabCorePipeline::MonitorState::get(Section section) const noexcept {
    const auto index = static_cast<std::size_t>(section);
    if (index >= sections.size()) return SectionMode::Active;
    const auto mode = sections[index];
    return section == Section::Final && mode == SectionMode::Testpoint
        ? SectionMode::Delta : mode;
}

int OptiLabCorePipeline::presetIndex(Preset preset) noexcept {
    return optilab_parameters::profileIndex(preset);
}

OptiLabCorePipeline::Preset OptiLabCorePipeline::presetFromIndex(int index) noexcept {
    return optilab_parameters::profileAt(index).preset;
}

void OptiLabCorePipeline::prepare(double sampleRate) {
    if (!OptiLabCoreEngine::isSampleRateSupported(sampleRate)) {
        throw std::invalid_argument("Unsupported OptiLab sample rate");
    }
    sampleRate_ = sampleRate;
    translatedFoundation_->prepare(sampleRate_);
    applyParameters();
    reset();
}

void OptiLabCorePipeline::reset() {
    translatedFoundation_->reset();
}

void OptiLabCorePipeline::setParameters(const Parameters& parameters) {
    parameters_ = parameters;
    parameters_.monitor.set(
        Section::Final, parameters_.monitor.get(Section::Final));
    applyParameters();
}

void OptiLabCorePipeline::setPreparedParameters(Preset preset, const OptiLabCoreEngine::ParameterValues& values, double p002) noexcept {
    parameters_.preset = preset;
    parameters_.p002 = p002;
    for (auto& [key, value] : parameters_.parameterOverrides) {
        const auto it = values.find(key);
        if (it != values.end()) value = it->second;
    }
    for (auto& [key, value] : parameters_.internalOverrides) {
        const auto it = values.find(key);
        if (it != values.end()) value = it->second;
    }
    if (auto it = parameters_.parameterOverrides.find("p002"); it != parameters_.parameterOverrides.end())
        it->second = p002;
    if (auto it = parameters_.parameterOverrides.find("p001"); it != parameters_.parameterOverrides.end())
        it->second = presetIndex(preset);
    const auto* factory = optilab_parameters::findProfile(preset);
    if (!factory) return;
    translatedFoundation_->setPreparedParameters(factory->mode, values, p002, presetIndex(preset));
}

void OptiLabCorePipeline::setParameterValue(std::string_view key, double value) {
    if (key == "p001") {
        parameters_.parameterOverrides.clear();
        parameters_.parameterOverrides.emplace(std::string(key), value);
        const auto factoryMode = SectionMode::Active;
        parameters_.monitor.sections.fill(factoryMode);
        applyParameters();
    } else {
        const auto existing = parameters_.parameterOverrides.find(key);
        if (existing == parameters_.parameterOverrides.end()) {
            parameters_.parameterOverrides.emplace(std::string(key), value);
        } else {
            existing->second = value;
        }
        translatedFoundation_->setParameterValue(key, value);
    }
}

bool OptiLabCorePipeline::setParameterValueRealtime(
    std::string_view key, double value) noexcept {
    if (!setParameterValueRealtimeDeferred(key, value)) return false;
    commitRealtimeParameterChanges();
    return true;
}

bool OptiLabCorePipeline::setParameterValueRealtimeDeferred(
    std::string_view key, double value) noexcept {
    if (key == "p001" || !std::isfinite(value)) return false;
    const auto existing = parameters_.parameterOverrides.find(key);
    if (existing == parameters_.parameterOverrides.end()) return false;
    if (!translatedFoundation_->setParameterValueRealtimeDeferred(key, value)) return false;
    existing->second = value;
    return true;
}

void OptiLabCorePipeline::commitRealtimeParameterChanges() noexcept {
    translatedFoundation_->commitRealtimeParameterChanges();
}

void OptiLabCorePipeline::setMonitorState(const MonitorState& monitor) noexcept {
    parameters_.monitor = monitor;
    parameters_.monitor.set(
        Section::Final, parameters_.monitor.get(Section::Final));
    applyMonitorRouting();
}

bool OptiLabCorePipeline::auxTapEnabled() const noexcept {
    return translatedFoundation_->auxTapEnabled();
}

std::size_t OptiLabCorePipeline::latencySamples() const noexcept {
    return translatedFoundation_->latencySamples();
}

OptiLabCoreEngine::ParameterValues OptiLabCorePipeline::effectiveParameterValues() const {
    return translatedFoundation_->effectiveParameterValues();
}

OptiLabCoreEngine::ParameterValues OptiLabCorePipeline::effectiveCoreValues() const {
    return translatedFoundation_->effectiveCoreValues();
}

OptiLabCoreEngine::Activity OptiLabCorePipeline::activity() const noexcept {
    return translatedFoundation_->activity();
}

void OptiLabCorePipeline::setActivityTracking(bool enabled) noexcept {
    translatedFoundation_->setActivityTracking(enabled);
}

std::pair<float, float> OptiLabCorePipeline::processSample(float left, float right) {
    return translatedFoundation_->processSample(left, right);
}

std::pair<float, float> OptiLabCorePipeline::processSampleAnalyzed(
    float left, float right, OptiLabAnalysisFrame& analysis) {
    return translatedFoundation_->processSampleAnalyzed(left, right, analysis);
}

void OptiLabCorePipeline::processInterleaved(
    float* samples,
    std::size_t frames,
    std::size_t channels
) {
    if (samples == nullptr || channels == 0) {
        return;
    }
    if (channels > 2) {
        throw std::invalid_argument("Core currently supports mono or stereo audio");
    }
    translatedFoundation_->processInterleaved(samples, frames, channels);
}

void OptiLabCorePipeline::applyParameters() {
    OptiLabCoreEngine::Parameters foundation;
    auto presetSelIt = parameters_.parameterOverrides.find("p001");
    if (presetSelIt != parameters_.parameterOverrides.end()) {
        const int sel = static_cast<int>(std::floor(presetSelIt->second + 0.5));
        if (sel >= 0 && sel < static_cast<int>(optilab_parameters::profileCount))
            parameters_.preset = presetFromIndex(sel);
    }
    const auto* factory = optilab_parameters::findProfile(parameters_.preset);
    if (!factory) factory = optilab_parameters::findProfile(Preset::StreamUpper);
    foundation.mode = factory->mode;
    const double presetInputTrimDb = optilab_parameters::get(
        optilab_parameters::Index::p002).defaults.forMode(static_cast<int>(factory->mode));
    const int presetLookaheadMode = factory->lookaheadProfile;
    foundation.inputDriveDb = parameters_.overrideInputTrim
        ? parameters_.p002 : presetInputTrimDb;
    foundation.autoAdaptPct = 0.0;
    foundation.overrides.clear();
    foundation.internalOverrides.clear();
    for (const auto& [key, value] : parameters_.parameterOverrides) {
        if (OptiLabCoreEngine::isCoreInternalKey(key)) foundation.internalOverrides[key] = value;
        else foundation.overrides[key] = value;
    }
    for (const auto& [key, value] : parameters_.internalOverrides)
        foundation.internalOverrides[key] = value;
    foundation.overrides["p001"] = optilab_parameters::findProfile(parameters_.preset)
        ? static_cast<double>(presetIndex(parameters_.preset))
        : static_cast<double>(presetIndex(Preset::StreamPolish));
    if (foundation.overrides.find("p003") == foundation.overrides.end()) {
        foundation.overrides["p003"] = static_cast<double>(presetLookaheadMode);
    }
    if (parameters_.overrideInputTrim) {
        foundation.overrides["p002"] = parameters_.p002;
    }
    for (std::size_t index = 0; index < foundation.sectionBypass.size(); ++index) {
        foundation.sectionBypass[index] =
            parameters_.monitor.sections[index] == SectionMode::Bypassed;
    }
    translatedFoundation_->setParameters(foundation);
    applyMonitorRouting();
}

void OptiLabCorePipeline::applyMonitorRouting() noexcept {
    for (std::size_t index = 0; index < parameters_.monitor.sections.size(); ++index) {
        translatedFoundation_->setSectionBypass(
            index, parameters_.monitor.sections[index] == SectionMode::Bypassed);
    }
    OptiLabCoreEngine::OutputTap tap = OptiLabCoreEngine::OutputTap::FullChain;
    for (std::size_t index = 0; index < parameters_.monitor.sections.size(); ++index) {
        const auto mode = parameters_.monitor.sections[index];
        if (mode != SectionMode::Testpoint && mode != SectionMode::Delta) continue;
        const bool delta = mode == SectionMode::Delta;
        if (index == static_cast<std::size_t>(Section::Input)) {
            tap = delta ? OptiLabCoreEngine::OutputTap::InputDelta : OptiLabCoreEngine::OutputTap::InputTestpoint;
        } else if (index == static_cast<std::size_t>(Section::Agc)) {
            tap = delta ? OptiLabCoreEngine::OutputTap::AgcDelta : OptiLabCoreEngine::OutputTap::AgcTestpoint;
        } else if (index == static_cast<std::size_t>(Section::Bass)) {
            tap = delta ? OptiLabCoreEngine::OutputTap::BassDelta : OptiLabCoreEngine::OutputTap::BassTestpoint;
        } else if (index == static_cast<std::size_t>(Section::Density)) {
            tap = delta ? OptiLabCoreEngine::OutputTap::DensityDelta : OptiLabCoreEngine::OutputTap::DensityTestpoint;
        } else if (index == static_cast<std::size_t>(Section::StereoFm)) {
            tap = delta ? OptiLabCoreEngine::OutputTap::StereoDelta : OptiLabCoreEngine::OutputTap::StereoTestpoint;
        } else if (delta) {
            tap = OptiLabCoreEngine::OutputTap::FinalDelta;
        }
        break;
    }
    if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::Input) {
        tap = OptiLabCoreEngine::OutputTap::InputTestpoint;
    } else if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::Agc) {
        tap = OptiLabCoreEngine::OutputTap::AgcTestpoint;
    } else if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::Bass) {
        tap = OptiLabCoreEngine::OutputTap::BassTestpoint;
    } else if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::Density) {
        tap = OptiLabCoreEngine::OutputTap::DensityTestpoint;
    } else if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::Stereo) {
        tap = OptiLabCoreEngine::OutputTap::StereoTestpoint;
    } else if (tap == OptiLabCoreEngine::OutputTap::FullChain && parameters_.testpoint == Testpoint::FinalDelta) {
        tap = OptiLabCoreEngine::OutputTap::FinalDelta;
    }
    translatedFoundation_->setOutputTap(tap);
}

std::pair<float, float> OptiLabCorePipeline::processSampleRouted(double left, double right,
    double auxLeft, double auxRight, AuxFrame& aux, OptiLabAnalysisFrame* analysis) {
    return translatedFoundation_->processSampleRouted(left, right, auxLeft, auxRight, aux, analysis);
}

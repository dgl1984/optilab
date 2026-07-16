#include "OptiLabCore.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
constexpr double tiny = 0.000001;
}

double OptiLabCore::OnePole::lp(double x, double a) {
    z += a * (x - z);
    return z;
}

double OptiLabCore::OnePole::lp2(double x, double a) {
    z1 += a * (x - z1);
    z2 += a * (z1 - z2);
    return z2;
}

double OptiLabCore::OnePole::hp(double x, double a) {
    z += a * (x - z);
    return x - z;
}

void OptiLabCore::OnePole::reset() {
    z = z1 = z2 = 0.0;
}

double OptiLabCore::Allpass1::run(double x, double c) {
    const double y = -c * x + x1 + c * y1;
    x1 = x;
    y1 = y;
    return y;
}

void OptiLabCore::Allpass1::reset() {
    x1 = y1 = 0.0;
}

double OptiLabCore::PeakEnv::run(double det, double attack, double release) {
    e = det > e ? det + attack * (e - det) : det + release * (e - det);
    return e;
}

double OptiLabCore::PeakEnv::set(double value) {
    e = value;
    return e;
}

double OptiLabCore::GainCell::bandLimitPd(double det, double thresh, double attack, double relBase, double relSlow, double pdAmt) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double depth = OptiLabCore::clamp((env - thresh) / std::max(thresh * 2.5, tiny), 0.0, 1.0);
    double relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    env = det > env ? det + attack * (env - det) : det + relNow * (env - det);
    env = std::max(env, tiny);
    double targetGain = env > thresh ? thresh / env : 1.0;
    targetGain = OptiLabCore::clamp(targetGain, 0.10, 1.0);
    depth = OptiLabCore::clamp((1.0 - gain) / 0.75, 0.0, 1.0);
    relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + relNow * (gain - targetGain);
    return gain;
}

double OptiLabCore::GainCell::bandLimitPdSoft(double det, double thresh, double attack, double relBase, double relSlow,
                                              double pdAmt, double ratio, double knee, double minGain) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double depth = OptiLabCore::clamp((env - thresh) / std::max(thresh * 2.5, tiny), 0.0, 1.0);
    double relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    env = det > env ? det + attack * (env - det) : det + relNow * (env - det);
    env = std::max(env, tiny);

    const double over = env / std::max(thresh, tiny);
    double targetGain = 1.0;
    if (over > 1.0) {
        const double soft = OptiLabCore::clamp((over - 1.0) / std::max((over - 1.0) + knee, tiny), 0.0, 1.0);
        const double compressed = std::pow(over, (1.0 / std::max(ratio, 1.01)) - 1.0);
        targetGain = 1.0 - soft * (1.0 - compressed);
    }
    targetGain = OptiLabCore::clamp(targetGain, minGain, 1.0);
    depth = OptiLabCore::clamp((1.0 - gain) / 0.75, 0.0, 1.0);
    relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + relNow * (gain - targetGain);
    return gain;
}

double OptiLabCore::GainCell::linkedLimiter(double det, double thresh, double attack, double release) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double targetGain = det > thresh ? thresh / std::max(det, tiny) : 1.0;
    targetGain = OptiLabCore::clamp(targetGain, 0.08, 1.0);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + release * (gain - targetGain);
    return gain;
}

double OptiLabCore::SmoothRounder::run(double x, double amt) {
    amt = OptiLabCore::clamp(amt, 0.0, 1.0);
    if (amt <= 0.0000001) {
        return x;
    }
    const double sat = std::sin(OptiLabCore::clamp(x, -pi * 0.5, pi * 0.5));
    const double apply = OptiLabCore::clamp(std::abs(prevSat + sat) * 0.5 * amt, 0.0, 1.0);
    const double y = x * (1.0 - apply) + sat * apply;
    prevSat = sat;
    return y;
}

void OptiLabCore::SmoothRounder::reset() {
    prevSat = 0.0;
}

double OptiLabCore::DcClipper::run(double x, double th, double amt, double kneeMul, double cancelAmt, double a) {
    const double clipped = OptiLabCore::softClipKnee(x, th, kneeMul);
    double y = clipped;
    if (cancelAmt > 0.0) {
        const double dist = clipped - x;
        dcZ += a * (dist - dcZ);
        y -= dcZ * cancelAmt;
    }
    return x * (1.0 - amt) + y * amt;
}

void OptiLabCore::DcClipper::reset() {
    dcZ = 0.0;
}

double OptiLabCore::BassClipper::run(double x, double th, double amt, double kneeMul, double drive, double resMix, double a) {
    drive = std::max(drive, 1.0);
    const double driven = x * drive;
    const double clipped = OptiLabCore::softClipKnee(driven, th, kneeMul) / drive;
    const double dist = clipped - x;
    z1 += a * (dist - z1);
    z2 += a * (z1 - z2);
    const double clipMix = amt * (0.50 + 0.50 * amt);
    const double residueMix = amt * resMix * (0.35 + 0.65 * amt);
    return x + dist * clipMix + z2 * residueMix;
}

void OptiLabCore::BassClipper::reset() {
    z1 = z2 = 0.0;
}

void OptiLabCore::Biquad::setLowpass(double freq, double q, double sampleRate) {
    freq = OptiLabCore::clamp(freq, 20.0, sampleRate * 0.45);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double norm = 1.0 / (1.0 + alpha);
    a0 = ((1.0 - cw) * 0.5) * norm;
    a1 = (1.0 - cw) * norm;
    a2 = a0;
    b1 = (-2.0 * cw) * norm;
    b2 = (1.0 - alpha) * norm;
}

void OptiLabCore::Biquad::setPeak(double freq, double q, double gainDb, double sampleRate) {
    freq = OptiLabCore::clamp(freq, 20.0, sampleRate * 0.45);
    const double A = std::exp(gainDb * 0.057564627324851);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double b0 = 1.0 + alpha * A;
    const double bb1 = -2.0 * cw;
    const double bb2 = 1.0 - alpha * A;
    const double aa0 = 1.0 + alpha / A;
    const double aa1 = -2.0 * cw;
    const double aa2 = 1.0 - alpha / A;
    const double norm = 1.0 / aa0;
    a0 = b0 * norm;
    a1 = bb1 * norm;
    a2 = bb2 * norm;
    b1 = aa1 * norm;
    b2 = aa2 * norm;
}

double OptiLabCore::Biquad::run(double x) {
    const double y = a0 * x + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

void OptiLabCore::Biquad::reset() {
    x1 = x2 = y1 = y2 = 0.0;
}

double OptiLabCore::dbToLin(double db) {
    return std::exp(db * 0.11512925464970229);
}

double OptiLabCore::linToDb(double x) {
    return std::log(std::max(x, 0.0000001)) * 8.6858896380650366;
}

double OptiLabCore::clamp(double x, double lo, double hi) {
    return std::min(std::max(x, lo), hi);
}

double OptiLabCore::absmax2(double a, double b) {
    return std::max(std::abs(a), std::abs(b));
}

double OptiLabCore::softClipKnee(double x, double th, double kneeMul) {
    const double ax = std::abs(x);
    if (ax <= th) {
        return x;
    }
    const double sign = x < 0.0 ? -1.0 : 1.0;
    const double over = ax - th;
    const double knee = std::max(tiny, th * kneeMul);
    const double y = th + knee * (1.0 - std::exp(-over / knee));
    return sign * y;
}

double OptiLabCore::purePeakRound(double x, double th, double amt, double driveMul) {
    if (amt <= 0.0) {
        return x;
    }
    th = std::max(th, tiny);
    const double d = driveMul / th;
    const double lim = pi * 0.5;
    const double y = std::sin(clamp(x * d, -lim, lim)) / d;
    return x * (1.0 - amt) + y * amt;
}

double OptiLabCore::hardLimit(double x, double ceilingValue) {
    return std::min(std::max(x, -ceilingValue), ceilingValue);
}

double OptiLabCore::apCoeff(double freq) const {
    freq = clamp(freq, 5.0, sampleRate * 0.45);
    const double t = std::tan(pi * freq / sampleRate);
    return (t - 1.0) / (t + 1.0);
}

OptiLabCore::Parameters OptiLabCore::defaultParameters(Mode mode) noexcept {
    Parameters defaults;
    defaults.mode = mode;
    defaults.inputDriveDb = mode == Mode::PodcastLeveler ? 3.5 :
                            mode == Mode::StreamPolish ? 4.5 : 0.0;
    return defaults;
}

void OptiLabCore::prepare(double newSampleRate) {
    sampleRate = std::max(1.0, newSampleRate);
    lastMode = -1;
    applyModeAndDerivedParameters();
    reset();
}

void OptiLabCore::setParameters(const Parameters& newParameters) {
    const int previousMode = static_cast<int>(params.mode);
    params = newParameters;
    params.autoAdaptPct = clamp(params.autoAdaptPct, 0.0, 100.0);
    params.inputDriveDb = clamp(params.inputDriveDb, -12.0, 18.0);
    const int nextMode = static_cast<int>(params.mode);
    if (lastMode >= 0 && previousMode != nextMode) {
        params.inputDriveDb = nextMode == 0 ? 3.5 : nextMode == 1 ? 4.5 : 0.0;
        reset();
    }
    applyModeAndDerivedParameters();
    lastMode = nextMode;
}

void OptiLabCore::resetPhaseState() {
    for (auto& ap : apL) {
        ap.reset();
    }
    for (auto& ap : apR) {
        ap.reset();
    }
}

void OptiLabCore::reset() {
    agcLGain = 1.0;
    agcHGain = 1.0;
    startupActivity = 0.0;
    gateProgEnv = 0.0;
    gateState = 0.0;
    gateClosedMemory = 0.0;
    gateReopenEnv = 0.0;
    gateReopenPulse = 0.0;
    adaptBassGain = 1.0;
    adaptTopPresenceGain = 1.0;
    adaptTopAirGain = 1.0;
    finalThresholdDriveS = finalThresholdDriveTarget > 0.0 ? finalThresholdDriveTarget : 1.0;
    finalThresholdGuardGain = 1.0;
    preclip.gain = 1.0;
    masterStartupArmed = 1;
    masterStartupAge = 0;
    masterCatchGain = 1.0;

    hp30L.reset(); hp30R.reset();
    resetPhaseState();
    agcLpL.reset(); agcLpR.reset();
    agcLowEnv.e = agcHighEnv.e = 0.0;
    postAgcRoundL.reset(); postAgcRoundR.reset();
    masterRawEnv.e = masterAmpEnv.e = masterCatchEnv.e = 0.0;
    adaptBassLpL.reset(); adaptBassLpR.reset();
    adaptBassEnv.e = adaptProgEnv.e = 0.0;
    bassclipSplitL.reset(); bassclipSplitR.reset();
    bassclipHpfL.reset(); bassclipHpfR.reset();
    bassclipPreL.reset(); bassclipPreR.reset();
    for (auto& f : xbL) f.reset();
    for (auto& f : xbR) f.reset();
    adaptTopAirHpL.reset(); adaptTopAirHpR.reset();
    adaptTopEdgeEnv.e = adaptTopEdgeProgEnv.e = adaptTopAirEnv.e = adaptTopAirProgEnv.e = 0.0;
    for (auto& l : lim) { l.env = 0.0; l.gain = 1.0; }
    for (auto& c : mbClipL) c.reset();
    for (auto& c : mbClipR) c.reset();
    ubFast4.reset(); ubFast5.reset(); ubFast6.reset();
    ubSlow4.reset(); ubSlow5.reset(); ubSlow6.reset();
    ubDslow4.reset(); ubDslow5.reset(); ubDslow6.reset();
    ubCslow4.reset(); ubCslow5.reset(); ubCslow6.reset();
    ubGate4.e = ubGate5.e = ubGate6.e = 0.0;
    ubLim6.gain = 1.0;
    usPrev4L = usPrev4R = usPrev24L = usPrev24R = 0.0;
    usPrev5L = usPrev5R = usPrev25L = usPrev25R = 0.0;
    usPrev6L = usPrev6R = usPrev26L = usPrev26R = 0.0;
    postXt2RoundL.reset(); postXt2RoundR.reset();
    finalFullEnv.e = 0.0;
    distLpfL.reset(); distLpfR.reset();
    finalRoundL.reset(); finalRoundR.reset();
    fcsResL.reset(); fcsResR.reset();

    fcsBufL.fill(0.0); fcsBufR.fill(0.0); fcsBufOL.fill(0.0); fcsBufOR.fill(0.0);
    for (auto& band : snubL) band.fill(0.0);
    for (auto& band : snubR) band.fill(0.0);
    masterBufL.fill(0.0); masterBufR.fill(0.0);
    fcsWrite = snubWrite = masterWrite = 0;
}

void OptiLabCore::applyModeAndDerivedParameters() {
    const int coreMode = static_cast<int>(params.mode);
    const double autoUp = clamp(params.autoAdaptPct * 0.01, 0.0, 1.0);

    if (coreMode == 0) {
        inputTrimDb = 3.5; phaseRotatePct = 3; subsonicHpf = 1; agcAmountPct = 65; agcDriveDb = 0.5;
        releaseTime = 7; pdReleasePct = 70; gateThresholdDb = -20; gateReopenSpeedMs = 40; gateReopenStrengthPct = 65;
        postAgcSmoothDrivePct = 25; bassCouplingPct = 70; bassEqDb = 2; bassScReliefPct = 35; lowBassFloorPct = 15;
        lowCoherencePct = 65; lowReleaseStabPct = 70; transitionFillPct = 18; adaptiveBassCouplingPct = 12;
        bassClipPct = 8; bassClipDensityPct = 0; densityDb = 0; xt2AmountPct = 100; presenceDb = 2; brillianceDb = 2;
        adaptiveTopCouplingPct = 50; crossoverModel = 2; mbClipPct = 0; clipDriveDb = 0; recombControlPct = 0;
        dcCancelPct = 65; upperSnubberPct = 0; snubberLookaheadMs = 1.5; postXt2SmoothDrivePct = 10;
        stereoMode = 0; stereoWidthPct = 100; preFinalDriveDb = -1.5; clipperStyle = 0; preLimiterPct = 100;
        clipRestraintPct = 100; overshootPct = 100; lookaheadMs = 0.36; finalThresholdDb = -1.5;
        finalThresholdMakeupPct = 100; clipCeilingDb = -0.1; topFilterMode = 2; outputTrimDb = 0; smoothDriveRounderPct = 58;

        agcAmountPct = 65 + (85 - 65) * autoUp;
        agcDriveDb = 0.5 + (0.0 - 0.5) * autoUp;
        releaseTime = 7 + (5.2 - 7) * (autoUp * autoUp);
        pdReleasePct = 70 + (84 - 70) * autoUp;
        gateReopenSpeedMs = 40 + (36 - 40) * autoUp;
        gateReopenStrengthPct = 65 + (60 - 65) * (autoUp * autoUp);
        bassScReliefPct = 35 + (56 - 35) * autoUp;
        lowReleaseStabPct = 70 + (82 - 70) * autoUp;
        adaptiveBassCouplingPct = 12 + (22 - 12) * autoUp;
        adaptiveTopCouplingPct = 50 + (72 - 50) * autoUp;
        postXt2SmoothDrivePct = 10 + (12 - 10) * autoUp;
        clipDriveDb = 0 + (-0.7 - 0) * autoUp;
    } else if (coreMode == 1) {
        inputTrimDb = 4.5; phaseRotatePct = 5; subsonicHpf = 0; agcAmountPct = 56; agcDriveDb = 0;
        bassCouplingPct = 50; releaseTime = 5; gateThresholdDb = -30; gateReopenSpeedMs = 50; gateReopenStrengthPct = 55;
        densityDb = 3.5; xt2AmountPct = 84; bassEqDb = 3.6; presenceDb = 2.3; brillianceDb = 3.2;
        clipDriveDb = -0.8; clipperStyle = 2; preLimiterPct = 44; clipRestraintPct = 38; clipCeilingDb = -0.1;
        finalThresholdDb = -0.8; finalThresholdMakeupPct = 100; stereoMode = 0; stereoWidthPct = 100; topFilterMode = 0;
        outputTrimDb = 1.1; mbClipPct = 16; dcCancelPct = 46; overshootPct = 100; recombControlPct = 10;
        lookaheadMs = 0.54; crossoverModel = 2; pdReleasePct = 78; bassScReliefPct = 76; lowCoherencePct = 66;
        lowReleaseStabPct = 74; transitionFillPct = 12; lowBassFloorPct = 42; adaptiveBassCouplingPct = 65;
        adaptiveTopCouplingPct = 50; bassClipPct = 32; bassClipDensityPct = 6; upperSnubberPct = 20;
        snubberLookaheadMs = 0.73; postAgcSmoothDrivePct = 12; postXt2SmoothDrivePct = 12; preFinalDriveDb = -0.4;
        smoothDriveRounderPct = 50;

        agcAmountPct = 56 + (62 - 56) * autoUp;
        agcDriveDb = 0;
        releaseTime = 5 + (5.6 - 5) * autoUp;
        pdReleasePct = 78 + (76 - 78) * autoUp;
        gateThresholdDb = -30 + 2 * autoUp;
        gateReopenStrengthPct = 55 + (48 - 55) * autoUp;
        postAgcSmoothDrivePct = 12 + (14 - 12) * autoUp;
        bassCouplingPct = 50 + (52 - 50) * autoUp;
        bassEqDb = 3.6 + (3.4 - 3.6) * autoUp;
        bassScReliefPct = 76 + (70 - 76) * autoUp;
        lowBassFloorPct = 42 + (34 - 42) * autoUp;
        lowCoherencePct = 66 + (58 - 66) * autoUp;
        lowReleaseStabPct = 74 + (72 - 74) * autoUp;
        adaptiveBassCouplingPct = 65 + (72 - 65) * autoUp;
        densityDb = 3.5 + (3.8 - 3.5) * autoUp;
        presenceDb = 2.3;
        brillianceDb = 3.2;
        adaptiveTopCouplingPct = 50;
        postXt2SmoothDrivePct = 12 + (14 - 12) * autoUp;
        preFinalDriveDb = -0.4;
        clipDriveDb = -0.8;
        overshootPct = 100;
        lookaheadMs = 0.54 + (0.70 - 0.54) * autoUp;
        smoothDriveRounderPct = 50 + (54 - 50) * autoUp;
    } else {
        inputTrimDb = 0; phaseRotatePct = 0; subsonicHpf = 0; agcAmountPct = 0; agcDriveDb = 0; releaseTime = 6.5;
        pdReleasePct = 0; gateThresholdDb = -70; gateReopenSpeedMs = 50; gateReopenStrengthPct = 0;
        postAgcSmoothDrivePct = 0; bassCouplingPct = 0; bassEqDb = 0; bassScReliefPct = 0; lowBassFloorPct = 0;
        lowCoherencePct = 0; lowReleaseStabPct = 0; transitionFillPct = 0; adaptiveBassCouplingPct = 0;
        bassClipPct = 25; bassClipDensityPct = 0; densityDb = -0.4; xt2AmountPct = 10; presenceDb = 0; brillianceDb = 0;
        adaptiveTopCouplingPct = 0; crossoverModel = 2; mbClipPct = 20; clipDriveDb = 0; recombControlPct = 6;
        dcCancelPct = 0; upperSnubberPct = 32; snubberLookaheadMs = 1.5; postXt2SmoothDrivePct = 12;
        stereoMode = 0; stereoWidthPct = 100; preFinalDriveDb = -1.6; clipperStyle = 0; preLimiterPct = 100;
        clipRestraintPct = 60; overshootPct = 100; lookaheadMs = 0.73; finalThresholdDb = -2;
        finalThresholdMakeupPct = 100; clipCeilingDb = -0.1; topFilterMode = 0; outputTrimDb = 0; smoothDriveRounderPct = 50;

        densityDb = -0.4 + (-0.8 + 0.4) * autoUp;
        snubberLookaheadMs = 1.5 + (2.0 - 1.5) * autoUp;
        postXt2SmoothDrivePct = 12 + (20 - 12) * autoUp;
        preFinalDriveDb = -1.6 + (-3.0 + 1.6) * autoUp;
        clipDriveDb = 0 + (-0.6 - 0) * autoUp;
        lookaheadMs = 0.73 + (1.5 - 0.73) * autoUp;
        finalThresholdDb = -2 + (-3.0 + 2) * autoUp;
        smoothDriveRounderPct = 50 + (65 - 50) * autoUp;
    }

    processorMode = coreMode == 2 ? 1 : 0;
    inputTrimDb = clamp(params.inputDriveDb, -12.0, 18.0);
    adaptiveTopCouplingPct = clamp(adaptiveTopCouplingPct, 0.0, 100.0);
    outputTrimDb = clamp(outputTrimDb, -12.0, 3.0);

    inputGain = dbToLin(inputTrimDb);
    agcDrive = dbToLin(agcDriveDb);
    const double densityPosDb = std::max(densityDb, 0.0);
    const double densityNegDb = std::min(densityDb, 0.0);
    densityAudioGain = dbToLin(densityNegDb + densityPosDb * 0.35);
    densityDetectorGain = dbToLin(densityPosDb * 0.65);
    densityClipDrive = dbToLin(densityPosDb * 0.55);
    presenceGain = dbToLin(presenceDb);
    brillianceGain = dbToLin(brillianceDb);
    clipDriveMb = dbToLin(clipDriveDb * 0.60);
    clipDriveFull = dbToLin(clipDriveDb * 0.25);
    ceiling = dbToLin(clipCeilingDb);
    finalThresholdDb = clamp(finalThresholdDb, -24.0, 6.0);
    finalThresholdMakeupPct = clamp(finalThresholdMakeupPct, 0.0, 100.0);
    finalThresholdDriveTarget = dbToLin(-finalThresholdDb);
    finalThresholdMakeup = finalThresholdMakeupPct * 0.01;
    smoothDriveRounderAmt = clamp(smoothDriveRounderPct, 0.0, 100.0) * 0.01;
    postAgcSmoothDriveAmt = clamp(postAgcSmoothDrivePct, 0.0, 25.0) * 0.01;
    postXt2SmoothDriveAmt = clamp(postXt2SmoothDrivePct, 0.0, 50.0) * 0.01;
    postAgcSmoothRecoveryGain = dbToLin(2.0 * clamp(postAgcSmoothDrivePct, 0.0, 25.0) / 25.0);
    postXt2SmoothRecoveryGain = dbToLin(2.0 * clamp(postXt2SmoothDrivePct, 0.0, 50.0) / 50.0);
    preFinalDriveGain = dbToLin(clamp(preFinalDriveDb, -12.0, 12.0));
    outputGain = dbToLin(outputTrimDb);
    mbClipMix = mbClipPct * 0.01;
    dcCancel = dcCancelPct * 0.01;
    dc3Amt = dcCancel * 0.35; dc4Amt = dcCancel * 0.55; dc5Amt = dcCancel * 0.78; dc6Amt = dcCancel * 0.92;
    overshootAmt = overshootPct * 0.01;
    recombControl = recombControlPct * 0.01;
    crossoverModel = static_cast<int>(std::floor(clamp(crossoverModel, 0.0, 3.0) + 0.5));
    pdRelease = pdReleasePct * 0.01;
    upperSnubber = upperSnubberPct * 0.01;
    gateReopenStrength = gateReopenStrengthPct * 0.01;
    const double gateReopenSpeedSec = std::max(gateReopenSpeedMs * 0.001, 0.005);
    snubberLookaheadMs = clamp(snubberLookaheadMs, 0.0, 1.50);
    lookaheadMs = clamp(lookaheadMs, 0.0, 1.50);
    subsonicHpf = static_cast<int>(std::floor(clamp(subsonicHpf, 0.0, 1.0) + 0.5));
    topFilterMode = static_cast<int>(std::floor(clamp(topFilterMode, 0.0, 3.0) + 0.5));
    effectiveSnubberLookahead = (upperSnubber > 0.0 && snubberLookaheadMs > 0.0)
        ? std::min(snubBufferLength - 1, static_cast<int>(std::floor(sampleRate * snubberLookaheadMs * 0.001 + 0.5))) : 0;
    effectiveFinalLookahead = (overshootAmt > 0.0 && lookaheadMs > 0.0)
        ? std::min(fcsBufferLength - 1, static_cast<int>(std::floor(sampleRate * lookaheadMs * 0.001 + 0.5))) : 0;
    masteringLookaheadSamples = processorMode == 1
        ? std::min(masterBufferLength - 1, static_cast<int>(std::floor(sampleRate * 0.010 + 0.5))) : 0;
    masterStartupPrimeWindow = static_cast<int>(std::floor(sampleRate * 0.350 + 0.5));
    masterStartupActiveThresh = dbToLin(-70.0);
    pathDelay = static_cast<std::size_t>(masteringLookaheadSamples + effectiveSnubberLookahead + effectiveFinalLookahead);

    phaseMode = static_cast<int>(std::floor(clamp(phaseRotatePct, 0.0, 6.0) + 0.5));
    if (phaseMode != phaseModeLast) {
        resetPhaseState();
        phaseModeLast = phaseMode;
    }
    phaseStages = phaseMode == 1 ? 2 : phaseMode == 2 ? 4 : phaseMode == 3 ? 6 : phaseMode == 4 ? 8 : phaseMode == 5 ? 10 : phaseMode == 6 ? 12 : 0;

    const std::array<double, 12> phaseFreqs{55, 85, 130, 200, 310, 480, 740, 1150, 1800, 2800, 4300, 6500};
    for (std::size_t i = 0; i < apC.size(); ++i) {
        apC[i] = apCoeff(phaseFreqs[i]);
    }

    agcMix = agcAmountPct * 0.01;
    agcDownMix = agcMix <= 0.50 ? agcMix : 0.50 + (agcMix - 0.50) * 0.40;
    xt2Mix = xt2AmountPct * 0.01;
    adaptiveTopCoupling = adaptiveTopCouplingPct * 0.01;
    bassCoupling = bassCouplingPct * 0.01;
    bassScRelief = bassScReliefPct * 0.01;
    lowCoherence = lowCoherencePct * 0.01;
    lowReleaseStab = lowReleaseStabPct * 0.01;
    transitionFill = transitionFillPct * 0.01;
    lowBassFloor = lowBassFloorPct * 0.01;
    adaptiveBassCoupling = adaptiveBassCouplingPct * 0.01;
    bassClip = bassClipPct * 0.01;
    bassClipDensity = bassClipDensityPct * 0.01;
    prelimitMix = preLimiterPct * 0.01;
    clipRestraint = clipRestraintPct * 0.01;
    peakOnlyFinalLimiter = clipperStyle == 0 && preLimiterPct >= 99 && clipRestraintPct >= 99 &&
        smoothDriveRounderPct <= 0.001 && postAgcSmoothDrivePct <= 0.001 && postXt2SmoothDrivePct <= 0.001 &&
        std::abs(preFinalDriveDb) <= 0.001 && topFilterMode == 0 && outputTrimDb == 0 &&
        finalThresholdDb >= -0.001 && finalThresholdDb <= 0.001;
    prelimitThresh = peakOnlyFinalLimiter ? ceiling : ceiling * 0.985;
    fcsThreshSetting = peakOnlyFinalLimiter ? ceiling : ceiling * 0.985;

    agcEnvAttack = std::exp(-1.0 / (sampleRate * 0.045));
    agcGainAttack = std::exp(-1.0 / (sampleRate * 0.030));
    agcRelease = std::exp(-1.0 / (sampleRate * (0.080 + releaseTime * 0.085)));
    agcReleaseSlow = std::exp(-1.0 / (sampleRate * (0.240 + releaseTime * 0.150)));
    bandAttack = std::exp(-1.0 / (sampleRate * 0.0012));
    upperBandAttack = std::exp(-1.0 / (sampleRate * 0.0024));
    upperMidBandAttack = std::exp(-1.0 / (sampleRate * 0.0018));
    bandRelease = std::exp(-1.0 / (sampleRate * (0.035 + releaseTime * 0.035)));
    bandReleaseSlow = std::exp(-1.0 / (sampleRate * (0.100 + releaseTime * 0.080)));
    const double lowBandReleaseBase = std::exp(-1.0 / (sampleRate * (0.060 + releaseTime * 0.055)));
    const double lowBandReleaseSlow2 = std::exp(-1.0 / (sampleRate * (0.180 + releaseTime * 0.100)));
    lowBandRelease = bandRelease * (1.0 - lowReleaseStab) + lowBandReleaseBase * lowReleaseStab;
    lowBandReleaseSlow = bandReleaseSlow * (1.0 - lowReleaseStab) + lowBandReleaseSlow2 * lowReleaseStab;
    clipAttack = std::exp(-1.0 / (sampleRate * 0.0008));
    clipRelease = std::exp(-1.0 / (sampleRate * 0.045));
    finalThresholdSmooth = std::exp(-1.0 / (sampleRate * 0.010));
    recombAttack = std::exp(-1.0 / (sampleRate * 0.0007));
    recombRelease = std::exp(-1.0 / (sampleRate * (0.090 + releaseTime * 0.055)));
    startupActivityRelease = std::exp(-1.0 / (sampleRate * 0.350));
    masterCatchAttack = std::exp(-1.0 / (sampleRate * 0.010));
    masterCatchRelease = std::exp(-1.0 / (sampleRate * 0.150));
    masterEnvAttack = std::exp(-1.0 / (sampleRate * 0.006));
    masterEnvRelease = std::exp(-1.0 / (sampleRate * 0.320));
    gateDetectorRelease = std::exp(-1.0 / (sampleRate * 0.160));
    gateCloseCoeff = std::exp(-1.0 / (sampleRate * (0.040 + releaseTime * 0.012)));
    gateOpenCoeff = std::exp(-1.0 / (sampleRate * 0.0015));
    gateAgcFreezeRelease = std::exp(-1.0 / (sampleRate * 8.0));
    gateXt2FreezeRelease = std::exp(-1.0 / (sampleRate * 2.5));
    gateReopenRelease = std::exp(-1.0 / (sampleRate * gateReopenSpeedSec));
    gateReopenEnvRelease = std::exp(-1.0 / (sampleRate * std::max(gateReopenSpeedSec * 0.85, 0.005)));
    gateReopenDecay = std::exp(-1.0 / (sampleRate * std::max(gateReopenSpeedSec * 4.5, 0.120)));
    gateReopenPulseScale = 0.20 + 0.95 * gateReopenStrength;
    gateAgcDriftTarget = dbToLin(-10.0);
    gateAgcDriftCoeff = std::exp(-1.0 / (sampleRate * 4.5));
    upperSnubFastA = 1.0 - std::exp(-1.0 / (sampleRate * 0.00055));
    upperSnubSlowA = 1.0 - std::exp(-1.0 / (sampleRate * 0.018));
    upperSnubDeltaA = 1.0 - std::exp(-1.0 / (sampleRate * 0.0030));
    upperSnubCurveA = 1.0 - std::exp(-1.0 / (sampleRate * 0.0022));
    upperSnubGainAttack = std::exp(-1.0 / (sampleRate * 0.00010));
    upperSnubGainRelease = std::exp(-1.0 / (sampleRate * 0.0028));
    hpf30A = 1.0 - std::exp(-2.0 * pi * 20.0 / sampleRate);
    agcSplitA = 1.0 - std::exp(-2.0 * pi * 200.0 / sampleRate);
    x1A = 1.0 - std::exp(-2.0 * pi * 150.0 / sampleRate);
    x2A = 1.0 - std::exp(-2.0 * pi * 420.0 / sampleRate);
    x3A = 1.0 - std::exp(-2.0 * pi * 700.0 / sampleRate);
    x4A = 1.0 - std::exp(-2.0 * pi * 1600.0 / sampleRate);
    x5A = 1.0 - std::exp(-2.0 * pi * 3700.0 / sampleRate);
    distCancelA = 1.0 - std::exp(-2.0 * pi * 2200.0 / sampleRate);
    agcTarget = dbToLin(-17.0); agcMaxGain = dbToLin(9.0); agcMinGain = dbToLin(-14.0);
    gateLin = dbToLin(gateThresholdDb);
    b1Thresh = dbToLin(-9.5); b2Thresh = dbToLin(-11.5); b3Thresh = dbToLin(-13.0);
    b4Thresh = dbToLin(-14.5); b5Thresh = dbToLin(-16.5); b6Thresh = dbToLin(-18.0);
    constDb2LinMinus62 = dbToLin(-6.2);
    constDb2LinMinus36 = dbToLin(-3.6);
    clipRef = ceiling * 0.985;
    const double mbRef = ceiling * 0.985;
    mbWorkRef = mbRef / std::max(clipDriveMb * densityClipDrive, 0.25);
    b1DetScGain = densityDetectorGain * dbToLin(-bassEqDb * 0.55 * bassScRelief - 1.00 * lowBassFloor);
    b2DetScGain = densityDetectorGain * dbToLin(-bassEqDb * 0.35 * bassScRelief - 0.45 * lowBassFloor);
    adaptBassSplitA = 1.0 - std::exp(-2.0 * pi * 190.0 / sampleRate);
    adaptBassDetAttack = std::exp(-1.0 / (sampleRate * 0.420));
    adaptBassDetRelease = std::exp(-1.0 / (sampleRate * 3.200));
    adaptBassGainUp = std::exp(-1.0 / (sampleRate * 1.150));
    adaptBassGainDown = std::exp(-1.0 / (sampleRate * 0.130));
    adaptBassTargetLow = 0.33; adaptBassTargetHigh = 0.52;
    adaptBassMaxBoostDb = 12.0 * adaptiveBassCoupling;
    adaptBassMaxCutDb = 7.0 * adaptiveBassCoupling;
    adaptTopDetAttack = std::exp(-1.0 / (sampleRate * 0.360));
    adaptTopDetRelease = std::exp(-1.0 / (sampleRate * 2.800));
    adaptTopGainUp = std::exp(-1.0 / (sampleRate * 1.050));
    adaptTopGainDown = std::exp(-1.0 / (sampleRate * 0.300));
    adaptTopEdgeTargetLow = 0.17; adaptTopEdgeTargetHigh = 0.40;
    adaptTopAirTargetLow = 0.030; adaptTopAirTargetHigh = 0.145;
    adaptTopPresenceMaxBoostDb = 1.8 * adaptiveTopCoupling;
    adaptTopPresenceMaxCutDb = 3.8 * adaptiveTopCoupling;
    adaptTopAirMaxBoostDb = 3.6 * adaptiveTopCoupling;
    adaptTopAirMaxCutDb = 1.1 * adaptiveTopCoupling;
    adaptTopAirHpA = 1.0 - std::exp(-2.0 * pi * 6800.0 / sampleRate);
    bassClipSplitA = 1.0 - std::exp(-2.0 * pi * 145.0 / sampleRate);
    bassClipSubhpA = 1.0 - std::exp(-2.0 * pi * 28.0 / sampleRate);
    bassClipPreResA = 1.0 - std::exp(-2.0 * pi * 220.0 / sampleRate);
    bassClipPreTh = ceiling * (0.20 - 0.10 * bassClip);
    bassClipPreDrive = 1.00 + 4.20 * bassClip;
    bassClipPreAmt = std::min(1.0, bassClip * (0.95 + 0.15 * bassClip));
    bassClipMakeup = 1.00 + 0.08 * bassClip + 0.55 * bassClip * bassClipDensity;
    recombClipThresh = ceiling * (0.97 - 0.16 * recombControl);
    recombClipKnee = 0.68 - 0.18 * recombControl;
    if (clipperStyle == 0) { clipStage1 = clipRef * 0.96; clipKnee1 = 0.55; clipStage2 = clipRef * 0.998; clipKnee2 = 0.35; }
    else if (clipperStyle == 1) { clipStage1 = clipRef * 0.92; clipKnee1 = 0.46; clipStage2 = clipRef * 0.990; clipKnee2 = 0.30; }
    else if (clipperStyle == 2) { clipStage1 = clipRef * 0.88; clipKnee1 = 0.38; clipStage2 = clipRef * 0.975; clipKnee2 = 0.24; }
    else { clipStage1 = clipRef * 0.80; clipKnee1 = 0.26; clipStage2 = clipRef * 0.950; clipKnee2 = 0.18; }
    sideScale = stereoWidthPct * 0.01;
    bassPeakL.setPeak(65.0, 1.4, bassEqDb, sampleRate); bassPeakR.setPeak(65.0, 1.4, bassEqDb, sampleRate);
    transitionPeakL.setPeak(105.0, 1.0, transitionFill * 1.6, sampleRate); transitionPeakR.setPeak(105.0, 1.0, transitionFill * 1.6, sampleRate);
    lowFloorL.setPeak(58.0, 0.75, lowBassFloor * 0.65, sampleRate); lowFloorR.setPeak(58.0, 0.75, lowBassFloor * 0.65, sampleRate);
    if (topFilterMode == 1) {
        lpf15_1L.setLowpass(17000.0, 0.55, sampleRate); lpf15_1R.setLowpass(17000.0, 0.55, sampleRate);
        lpf15_2L.setLowpass(21000.0, 0.50, sampleRate); lpf15_2R.setLowpass(21000.0, 0.50, sampleRate);
    } else if (topFilterMode == 2) {
        lpf15_1L.setLowpass(15500.0, 0.58, sampleRate); lpf15_1R.setLowpass(15500.0, 0.58, sampleRate);
        lpf15_2L.setLowpass(15500.0, 0.58, sampleRate); lpf15_2R.setLowpass(15500.0, 0.58, sampleRate);
    } else {
        lpf15_1L.setLowpass(14500.0, 0.50, sampleRate); lpf15_1R.setLowpass(14500.0, 0.50, sampleRate);
        lpf15_2L.setLowpass(14500.0, 0.50, sampleRate); lpf15_2R.setLowpass(14500.0, 0.50, sampleRate);
    }
    lr4b1aL.setLowpass(150, 0.7071, sampleRate); lr4b1aR.setLowpass(150, 0.7071, sampleRate);
    lr4b1bL.setLowpass(150, 0.7071, sampleRate); lr4b1bR.setLowpass(150, 0.7071, sampleRate);
    lr4b2aL.setLowpass(420, 0.7071, sampleRate); lr4b2aR.setLowpass(420, 0.7071, sampleRate);
    lr4b2bL.setLowpass(420, 0.7071, sampleRate); lr4b2bR.setLowpass(420, 0.7071, sampleRate);
    lr4b3aL.setLowpass(700, 0.7071, sampleRate); lr4b3aR.setLowpass(700, 0.7071, sampleRate);
    lr4b3bL.setLowpass(700, 0.7071, sampleRate); lr4b3bR.setLowpass(700, 0.7071, sampleRate);
    lr4b4aL.setLowpass(1600, 0.7071, sampleRate); lr4b4aR.setLowpass(1600, 0.7071, sampleRate);
    lr4b4bL.setLowpass(1600, 0.7071, sampleRate); lr4b4bR.setLowpass(1600, 0.7071, sampleRate);
    lr4b5aL.setLowpass(3700, 0.7071, sampleRate); lr4b5aR.setLowpass(3700, 0.7071, sampleRate);
    lr4b5bL.setLowpass(3700, 0.7071, sampleRate); lr4b5bR.setLowpass(3700, 0.7071, sampleRate);
    fcsResidueA = 1.0 - std::exp(-2.0 * pi * 12000.0 / sampleRate);
}

std::pair<float, float> OptiLabCore::processSample(float left, float right) {
    double l = std::isfinite(left) ? static_cast<double>(left) : 0.0;
    double r = std::isfinite(right) ? static_cast<double>(right) : 0.0;
    l = std::abs(l) < 1000000000000.0 ? l : 0.0;
    r = std::abs(r) < 1000000000000.0 ? r : 0.0;
    const double inputRawL = l;
    const double inputRawR = r;

    l *= inputGain;
    r *= inputGain;
    if (subsonicHpf) {
        l = hp30L.hp(l, hpf30A);
        r = hp30R.hp(r, hpf30A);
    }
    if (phaseStages > 0) {
        for (int i = 0; i < phaseStages; ++i) {
            l = apL[static_cast<std::size_t>(i)].run(l, apC[static_cast<std::size_t>(i)]);
            r = apR[static_cast<std::size_t>(i)].run(r, apC[static_cast<std::size_t>(i)]);
        }
    }

    startupActivity = std::max(absmax2(l, r), startupActivity * startupActivityRelease);
    const double agcL = l * agcDrive;
    const double agcR = r * agcDrive;
    const double lowL = crossoverModel == 1 ? agcLpL.lp2(agcL, agcSplitA) : agcLpL.lp(agcL, agcSplitA);
    const double lowR = crossoverModel == 1 ? agcLpR.lp2(agcR, agcSplitA) : agcLpR.lp(agcR, agcSplitA);
    const double highL = agcL - lowL;
    const double highR = agcR - lowR;
    const double lowDet = absmax2(lowL, lowR);
    const double highDet = absmax2(highL, highR);
    const double fullDet = absmax2(agcL, agcR);
    double lowEnv = agcLowEnv.run(lowDet, agcEnvAttack, agcRelease);
    double highEnv = agcHighEnv.run(highDet, agcEnvAttack, agcRelease);

    gateProgEnv = std::max(fullDet, gateProgEnv * gateDetectorRelease);
    const double gateOpenNow = fullDet >= gateLin ? 1.0 : 0.0;
    const double gateTarget = gateProgEnv < gateLin ? 1.0 : 0.0;
    const double gatePrevState = gateState;
    gateClosedMemory = std::max(gateClosedMemory * gateReopenDecay, gateState);
    if (gateOpenNow > 0.0) {
        gateState = gateState * gateOpenCoeff;
    } else if (gateTarget > gateState) {
        gateState = gateTarget + gateCloseCoeff * (gateState - gateTarget);
    } else {
        gateState = gateTarget + gateOpenCoeff * (gateState - gateTarget);
    }
    gateState = clamp(gateState, 0.0, 1.0);
    const double gateEffective = gateState * (1.0 - gateOpenNow);
    gateReopenPulse = gateOpenNow * clamp(std::max(gatePrevState, gateClosedMemory) * gateReopenPulseScale, 0.0, 1.0);
    gateReopenEnv = std::max(gateReopenEnv * gateReopenDecay, gateReopenPulse);
    if (gateOpenNow > 0.0) {
        gateClosedMemory *= gateReopenDecay;
    }
    if (gateReopenEnv > 0.0) {
        const double clearCoeff = 1.0 - gateReopenEnv * (1.0 - gateReopenEnvRelease);
        lowEnv = agcLowEnv.set(lowDet + clearCoeff * (lowEnv - lowDet));
        highEnv = agcHighEnv.set(highDet + clearCoeff * (highEnv - highDet));
    }

    double lowTargetGain = clamp(agcTarget / std::max(lowEnv, tiny), agcMinGain, agcMaxGain);
    double highTargetGain = clamp(agcTarget / std::max(highEnv, tiny), agcMinGain, agcMaxGain);
    lowTargetGain = lowTargetGain * (1.0 - bassCoupling) + highTargetGain * bassCoupling;
    if (gateEffective > 0.0) {
        lowTargetGain = lowTargetGain * (1.0 - gateEffective) + std::min(lowTargetGain, agcLGain) * gateEffective;
        highTargetGain = highTargetGain * (1.0 - gateEffective) + std::min(highTargetGain, agcHGain) * gateEffective;
    }
    const double pdAgc = pdRelease * 0.15;
    const double agcLowDepth = clamp((1.0 - agcLGain) / 0.75, 0.0, 1.0);
    const double agcHighDepth = clamp((1.0 - agcHGain) / 0.75, 0.0, 1.0);
    double agcLowRel = agcRelease * (1.0 - pdAgc * agcLowDepth) + agcReleaseSlow * (pdAgc * agcLowDepth);
    double agcHighRel = agcRelease * (1.0 - pdAgc * agcHighDepth) + agcReleaseSlow * (pdAgc * agcHighDepth);
    agcLowRel = agcLowRel * (1.0 - gateEffective) + gateAgcFreezeRelease * gateEffective;
    agcHighRel = agcHighRel * (1.0 - gateEffective) + gateAgcFreezeRelease * gateEffective;
    const double gateReopenRelMix = gateReopenEnv * (0.35 + 0.45 * gateReopenStrength);
    const double agcGateReopenRelMix = gateReopenEnv * (0.06 + 0.16 * gateReopenStrength);
    agcLowRel = agcLowRel * (1.0 - agcGateReopenRelMix) + gateReopenRelease * agcGateReopenRelMix;
    agcHighRel = agcHighRel * (1.0 - agcGateReopenRelMix) + gateReopenRelease * agcGateReopenRelMix;
    agcLGain = lowTargetGain < agcLGain ? lowTargetGain + agcGainAttack * (agcLGain - lowTargetGain) : lowTargetGain + agcLowRel * (agcLGain - lowTargetGain);
    agcHGain = highTargetGain < agcHGain ? highTargetGain + agcGainAttack * (agcHGain - highTargetGain) : highTargetGain + agcHighRel * (agcHGain - highTargetGain);
    const double driftAmt = (1.0 - gateAgcDriftCoeff) * gateEffective;
    agcLGain += driftAmt * (gateAgcDriftTarget - agcLGain);
    agcHGain += driftAmt * (gateAgcDriftTarget - agcHGain);

    const double agcDriveSafe = std::max(agcDrive, tiny);
    const double agcLowProcGain = agcDrive * agcLGain;
    const double agcHighProcGain = agcDrive * agcHGain;
    const double agcLowAuthority = agcLowProcGain < 1.0 ? agcDownMix : agcMix;
    const double agcHighAuthority = agcHighProcGain < 1.0 ? agcDownMix : agcMix;
    const double agcLowEffGain = 1.0 + (agcLowProcGain - 1.0) * agcLowAuthority;
    const double agcHighEffGain = 1.0 + (agcHighProcGain - 1.0) * agcHighAuthority;
    l = (lowL / agcDriveSafe) * agcLowEffGain + (highL / agcDriveSafe) * agcHighEffGain;
    r = (lowR / agcDriveSafe) * agcLowEffGain + (highR / agcDriveSafe) * agcHighEffGain;
    if (postAgcSmoothDriveAmt > 0.0000001) {
        l = postAgcRoundL.run(l, postAgcSmoothDriveAmt) * postAgcSmoothRecoveryGain;
        r = postAgcRoundR.run(r, postAgcSmoothDriveAmt) * postAgcSmoothRecoveryGain;
    }

    if (masteringLookaheadSamples > 0) {
        const double masterSideDet = absmax2(l, r);
        const double masterRawDet = absmax2(inputRawL, inputRawR);
        const double masterAmpDet = masterSideDet;
        const bool active = std::max(masterSideDet, masterRawDet) > masterStartupActiveThresh;
        if (masterStartupArmed > 0) {
            if (active) {
                if (masterStartupAge < masterStartupPrimeWindow) {
                    masterBufL.fill(l);
                    masterBufR.fill(r);
                    masterWrite = 0;
                    masterRawEnv.set(std::max(masterRawDet, tiny));
                    masterAmpEnv.set(std::max(masterAmpDet, tiny));
                    masterCatchEnv.set(std::max(masterSideDet, tiny));
                    masterCatchGain = 1.0;
                }
                masterStartupArmed = 0;
            } else {
                masterStartupAge += 1;
                if (masterStartupAge >= masterStartupPrimeWindow) {
                    masterStartupArmed = 0;
                }
            }
        }
        const double rawLevel = masterRawEnv.run(masterRawDet, masterEnvAttack, masterEnvRelease);
        const double ampLevel = masterAmpEnv.run(masterAmpDet, masterEnvAttack, masterEnvRelease);
        const double netMakeupDb = linToDb(ampLevel / std::max(rawLevel, tiny));
        const double allow = clamp((netMakeupDb - 3.0) / 9.0, 0.0, 1.0);
        const double catchDet = masterCatchEnv.run(masterSideDet, masterCatchAttack, masterCatchRelease);
        double targetGain = catchDet > constDb2LinMinus62 ? constDb2LinMinus62 / std::max(catchDet, tiny) : 1.0;
        targetGain = clamp(targetGain, constDb2LinMinus36, 1.0);
        targetGain = 1.0 - allow * (1.0 - targetGain);
        masterCatchGain = targetGain < masterCatchGain ? targetGain + masterCatchAttack * (masterCatchGain - targetGain)
                                                       : targetGain + masterCatchRelease * (masterCatchGain - targetGain);
        int read = masterWrite - masteringLookaheadSamples;
        if (read < 0) read += masterBufferLength;
        const double delayedL = masterBufL[static_cast<std::size_t>(read)];
        const double delayedR = masterBufR[static_cast<std::size_t>(read)];
        masterBufL[static_cast<std::size_t>(masterWrite)] = l;
        masterBufR[static_cast<std::size_t>(masterWrite)] = r;
        masterWrite = (masterWrite + 1) % masterBufferLength;
        l = delayedL * masterCatchGain;
        r = delayedR * masterCatchGain;
    } else {
        masterCatchGain = 1.0;
        masterStartupArmed = 1;
        masterStartupAge = 0;
    }

    l = bassPeakL.run(l);
    r = bassPeakR.run(r);
    if (adaptiveBassCoupling > 0.0) {
        const double abLowL = adaptBassLpL.lp2(l, adaptBassSplitA);
        const double abLowR = adaptBassLpR.lp2(r, adaptBassSplitA);
        const double abHighL = l - abLowL;
        const double abHighR = r - abLowR;
        const double bassDet = absmax2(abLowL, abLowR);
        const double progDet = std::max(absmax2(l, r), absmax2(abHighL, abHighR) * 1.20);
        const double bassEnv = adaptBassEnv.run(bassDet, adaptBassDetAttack, adaptBassDetRelease);
        const double progEnv = adaptProgEnv.run(progDet, adaptBassDetAttack, adaptBassDetRelease);
        const double ratio = bassEnv / std::max(progEnv, tiny);
        const double thin = clamp((adaptBassTargetLow - ratio) / std::max(adaptBassTargetLow - 0.105, tiny), 0.0, 1.0);
        const double heavy = clamp((ratio - adaptBassTargetHigh) / std::max(0.82 - adaptBassTargetHigh, tiny), 0.0, 1.0);
        const double gainTarget = dbToLin(adaptBassMaxBoostDb * thin - adaptBassMaxCutDb * heavy);
        if (adaptBassGain <= 0.0) adaptBassGain = 1.0;
        adaptBassGain = gainTarget < adaptBassGain ? gainTarget + adaptBassGainDown * (adaptBassGain - gainTarget)
                                                   : gainTarget + adaptBassGainUp * (adaptBassGain - gainTarget);
        l = abHighL + abLowL * adaptBassGain;
        r = abHighR + abLowR * adaptBassGain;
    }
    if (bassClip > 0.0) {
        const double lowL2 = bassclipSplitL.lp2(l, bassClipSplitA);
        const double lowR2 = bassclipSplitR.lp2(r, bassClipSplitA);
        const double highL2 = l - lowL2;
        const double highR2 = r - lowR2;
        const double driveL = bassclipHpfL.hp(lowL2, bassClipSubhpA);
        const double driveR = bassclipHpfR.hp(lowR2, bassClipSubhpA);
        l = highL2 + (lowL2 - driveL) + bassclipPreL.run(driveL, bassClipPreTh, bassClipPreAmt, 0.42, bassClipPreDrive, 0.22, bassClipPreResA) * bassClipMakeup;
        r = highR2 + (lowR2 - driveR) + bassclipPreR.run(driveR, bassClipPreTh, bassClipPreAmt, 0.42, bassClipPreDrive, 0.22, bassClipPreResA) * bassClipMakeup;
    }

    l *= densityAudioGain;
    r *= densityAudioGain;
    double b1L, b2L, b3L, b4L, b5L, b6L;
    double b1R, b2R, b3R, b4R, b5R, b6R;
    if (crossoverModel == 3) {
        const double lp1L = lr4b1bL.run(lr4b1aL.run(l)); const double lp2L = lr4b2bL.run(lr4b2aL.run(l));
        const double lp3L = lr4b3bL.run(lr4b3aL.run(l)); const double lp4L = lr4b4bL.run(lr4b4aL.run(l)); const double lp5L = lr4b5bL.run(lr4b5aL.run(l));
        b1L = lp1L; b2L = lp2L - lp1L; b3L = lp3L - lp2L; b4L = lp4L - lp3L; b5L = lp5L - lp4L; b6L = l - lp5L;
        const double lp1R = lr4b1bR.run(lr4b1aR.run(r)); const double lp2R = lr4b2bR.run(lr4b2aR.run(r));
        const double lp3R = lr4b3bR.run(lr4b3aR.run(r)); const double lp4R = lr4b4bR.run(lr4b4aR.run(r)); const double lp5R = lr4b5bR.run(lr4b5aR.run(r));
        b1R = lp1R; b2R = lp2R - lp1R; b3R = lp3R - lp2R; b4R = lp4R - lp3R; b5R = lp5R - lp4R; b6R = r - lp5R;
    } else {
        const double lp1L = xbL[0].lp2(l, x1A); const double lp2L = xbL[1].lp2(l, x2A); const double lp3L = xbL[2].lp2(l, x3A); const double lp4L = xbL[3].lp2(l, x4A); const double lp5L = xbL[4].lp2(l, x5A);
        b1L = lp1L; b2L = lp2L - lp1L; b3L = lp3L - lp2L; b4L = lp4L - lp3L; b5L = lp5L - lp4L; b6L = l - lp5L;
        const double lp1R = xbR[0].lp2(r, x1A); const double lp2R = xbR[1].lp2(r, x2A); const double lp3R = xbR[2].lp2(r, x3A); const double lp4R = xbR[3].lp2(r, x4A); const double lp5R = xbR[4].lp2(r, x5A);
        b1R = lp1R; b2R = lp2R - lp1R; b3R = lp3R - lp2R; b4R = lp4R - lp3R; b5R = lp5R - lp4R; b6R = r - lp5R;
    }

    double topPresenceAdaptGain = 1.0;
    double topAirAdaptGain = 1.0;
    if (adaptiveTopCoupling > 0.0) {
        const double b4Det = absmax2(b4L, b4R), b5Det = absmax2(b5L, b5R), b6Det = absmax2(b6L, b6R);
        const double airL = adaptTopAirHpL.hp(l, adaptTopAirHpA), airR = adaptTopAirHpR.hp(r, adaptTopAirHpA);
        const double airDet = absmax2(airL, airR);
        const double edgeDet = b4Det * 0.30 + b5Det * 0.85 + b6Det * 0.18;
        const double progDet = std::max(absmax2(l, r), std::max(edgeDet, airDet) * 1.08);
        const double edgeRatio = adaptTopEdgeEnv.run(edgeDet, adaptTopDetAttack, adaptTopDetRelease) /
            std::max(adaptTopEdgeProgEnv.run(progDet, adaptTopDetAttack, adaptTopDetRelease), tiny);
        const double airRatio = adaptTopAirEnv.run(airDet, adaptTopDetAttack, adaptTopDetRelease) /
            std::max(adaptTopAirProgEnv.run(progDet, adaptTopDetAttack, adaptTopDetRelease), tiny);
        const double edgeDull = clamp((adaptTopEdgeTargetLow - edgeRatio) / std::max(adaptTopEdgeTargetLow - 0.060, tiny), 0.0, 1.0);
        const double edgeBright = clamp((edgeRatio - adaptTopEdgeTargetHigh) / std::max(0.78 - adaptTopEdgeTargetHigh, tiny), 0.0, 1.0);
        double presenceTargetDb = adaptTopPresenceMaxBoostDb * edgeDull - adaptTopPresenceMaxCutDb * edgeBright;
        const double airDull = clamp((adaptTopAirTargetLow - airRatio) / std::max(adaptTopAirTargetLow - 0.006, tiny), 0.0, 1.0);
        const double airBright = clamp((airRatio - adaptTopAirTargetHigh) / std::max(0.38 - adaptTopAirTargetHigh, tiny), 0.0, 1.0);
        double airTargetDb = adaptTopAirMaxBoostDb * airDull - adaptTopAirMaxCutDb * airBright;
        const double edgeGuard = clamp((edgeRatio - 0.46) / 0.34, 0.0, 1.0);
        airTargetDb -= std::max(airTargetDb, 0.0) * (0.55 * edgeGuard);
        const double presenceTarget = dbToLin(presenceTargetDb);
        const double airTarget = dbToLin(airTargetDb);
        if (adaptTopPresenceGain <= 0.0) adaptTopPresenceGain = 1.0;
        adaptTopPresenceGain = presenceTarget < adaptTopPresenceGain ? presenceTarget + adaptTopGainDown * (adaptTopPresenceGain - presenceTarget)
                                                                     : presenceTarget + adaptTopGainUp * (adaptTopPresenceGain - presenceTarget);
        if (adaptTopAirGain <= 0.0) adaptTopAirGain = 1.0;
        adaptTopAirGain = airTarget < adaptTopAirGain ? airTarget + adaptTopGainDown * (adaptTopAirGain - airTarget)
                                                       : airTarget + adaptTopGainUp * (adaptTopAirGain - airTarget);
        topPresenceAdaptGain = adaptTopPresenceGain;
        topAirAdaptGain = adaptTopAirGain;
    } else {
        adaptTopPresenceGain = adaptTopAirGain = 1.0;
    }

    b4L *= presenceGain * topPresenceAdaptGain; b4R *= presenceGain * topPresenceAdaptGain;
    b5L *= presenceGain * topPresenceAdaptGain; b5R *= presenceGain * topPresenceAdaptGain;
    b6L *= brillianceGain * topAirAdaptGain; b6R *= brillianceGain * topAirAdaptGain;

    const double gateXt2Strength = gateEffective * 0.78;
    double lowBandReleaseGated = lowBandRelease * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
    double lowBandReleaseSlowGated = lowBandReleaseSlow * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
    double bandReleaseGated = bandRelease * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
    double bandReleaseSlowGated = bandReleaseSlow * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
    lowBandReleaseGated = lowBandReleaseGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
    lowBandReleaseSlowGated = lowBandReleaseSlowGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
    bandReleaseGated = bandReleaseGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
    bandReleaseSlowGated = bandReleaseSlowGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;

    double g1 = lim[0].bandLimitPd(absmax2(b1L, b1R) * b1DetScGain, b1Thresh, bandAttack, lowBandReleaseGated, lowBandReleaseSlowGated, pdRelease);
    double g2 = lim[1].bandLimitPd(absmax2(b2L, b2R) * b2DetScGain, b2Thresh, bandAttack, lowBandReleaseGated, lowBandReleaseSlowGated, pdRelease);
    double g3 = lim[2].bandLimitPdSoft(absmax2(b3L, b3R) * densityDetectorGain, b3Thresh, upperMidBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 4.2, 0.30, 0.14);
    double g4 = lim[3].bandLimitPdSoft(absmax2(b4L, b4R) * densityDetectorGain, b4Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 3.1, 0.42, 0.16);
    double g5 = lim[4].bandLimitPdSoft(absmax2(b5L, b5R) * densityDetectorGain, b5Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 2.6, 0.55, 0.18);
    double g6 = lim[5].bandLimitPdSoft(absmax2(b6L, b6R) * densityDetectorGain, b6Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 2.4, 0.65, 0.20);
    g1 = 1.0 - xt2Mix * (1.0 - g1); g2 = 1.0 - xt2Mix * (1.0 - g2); g3 = 1.0 - xt2Mix * (1.0 - g3);
    g4 = 1.0 - xt2Mix * (1.0 - g4); g5 = 1.0 - xt2Mix * (1.0 - g5); g6 = 1.0 - xt2Mix * (1.0 - g6);
    if (lowCoherence > 0.0) {
        const double lowSharedGain = std::sqrt(std::max(g1 * g2, tiny));
        g1 = g1 * (1.0 - lowCoherence) + lowSharedGain * lowCoherence;
        g2 = g2 * (1.0 - lowCoherence) + lowSharedGain * lowCoherence;
    }
    g6 = g5;

    if (mbClipMix > 0.0) {
        b3L = mbClipL[0].run(b3L, mbWorkRef * 0.56, mbClipMix * 0.55, 0.42, dc3Amt, distCancelA);
        b3R = mbClipR[0].run(b3R, mbWorkRef * 0.56, mbClipMix * 0.55, 0.42, dc3Amt, distCancelA);
        b4L = mbClipL[1].run(b4L, mbWorkRef * 0.48, mbClipMix * 0.75, 0.36, dc4Amt, distCancelA);
        b4R = mbClipR[1].run(b4R, mbWorkRef * 0.48, mbClipMix * 0.75, 0.36, dc4Amt, distCancelA);
        b5L = mbClipL[2].run(b5L, mbWorkRef * 0.40, mbClipMix, 0.30, dc5Amt, distCancelA);
        b5R = mbClipR[2].run(b5R, mbWorkRef * 0.40, mbClipMix, 0.30, dc5Amt, distCancelA);
        b6L = mbClipL[3].run(b6L, mbWorkRef * 0.32, mbClipMix, 0.24, dc6Amt, distCancelA);
        b6R = mbClipR[3].run(b6R, mbWorkRef * 0.32, mbClipMix, 0.24, dc6Amt, distCancelA);
    }

    std::array<double, 6> pL{b1L * g1, b2L * g2, b3L * g3, b4L * g4, b5L * g5, b6L * g6};
    std::array<double, 6> pR{b1R * g1, b2R * g2, b3R * g3, b4R * g4, b5R * g5, b6R * g6};
    if (effectiveSnubberLookahead > 0) {
        int read = snubWrite - effectiveSnubberLookahead;
        if (read < 0) read += snubBufferLength;
        std::array<double, 6> delayedL{}, delayedR{};
        for (std::size_t i = 0; i < 6; ++i) {
            delayedL[i] = snubL[i][static_cast<std::size_t>(read)];
            delayedR[i] = snubR[i][static_cast<std::size_t>(read)];
            snubL[i][static_cast<std::size_t>(snubWrite)] = pL[i];
            snubR[i][static_cast<std::size_t>(snubWrite)] = pR[i];
        }
        pL = delayedL; pR = delayedR;
        snubWrite = (snubWrite + 1) % snubBufferLength;
    }

    if (upperSnubber > 0.0) {
        const double usAmt = upperSnubber;
        const double lvl4 = absmax2(pL[3], pR[3]), lvl5 = absmax2(pL[4], pR[4]), lvl6 = absmax2(pL[5], pR[5]);
        const double fast4 = ubFast4.lp(lvl4, upperSnubFastA), fast5 = ubFast5.lp(lvl5, upperSnubFastA), fast6 = ubFast6.lp(lvl6, upperSnubFastA);
        const double slow4 = ubSlow4.lp(lvl4, upperSnubSlowA), slow5 = ubSlow5.lp(lvl5, upperSnubSlowA), slow6 = ubSlow6.lp(lvl6, upperSnubSlowA);
        const double onset4 = clamp((fast4 - slow4 * 1.22) / std::max(slow4 * 0.80 + tiny, tiny), 0.0, 1.0);
        const double onset5 = clamp((fast5 - slow5 * 1.18) / std::max(slow5 * 0.72 + tiny, tiny), 0.0, 1.0);
        const double onset6 = clamp((fast6 - slow6 * 1.15) / std::max(slow6 * 0.66 + tiny, tiny), 0.0, 1.0);
        const double spike4 = clamp((lvl4 - slow4 * 1.20) / std::max(slow4 * 1.20 + tiny, tiny), 0.0, 1.0);
        const double spike5 = clamp((lvl5 - slow5 * 1.16) / std::max(slow5 * 1.05 + tiny, tiny), 0.0, 1.0);
        const double spike6 = clamp((lvl6 - slow6 * 1.12) / std::max(slow6 * 0.95 + tiny, tiny), 0.0, 1.0);
        const double d4 = std::max(std::abs(pL[3] - usPrev4L), std::abs(pR[3] - usPrev4R));
        const double d5 = std::max(std::abs(pL[4] - usPrev5L), std::abs(pR[4] - usPrev5R));
        const double d6 = std::max(std::abs(pL[5] - usPrev6L), std::abs(pR[5] - usPrev6R));
        const double c4 = std::max(std::abs(pL[3] - 2 * usPrev4L + usPrev24L), std::abs(pR[3] - 2 * usPrev4R + usPrev24R));
        const double c5 = std::max(std::abs(pL[4] - 2 * usPrev5L + usPrev25L), std::abs(pR[4] - 2 * usPrev5R + usPrev25R));
        const double c6 = std::max(std::abs(pL[5] - 2 * usPrev6L + usPrev26L), std::abs(pR[5] - 2 * usPrev6R + usPrev26R));
        usPrev24L = usPrev4L; usPrev24R = usPrev4R; usPrev25L = usPrev5L; usPrev25R = usPrev5R; usPrev26L = usPrev6L; usPrev26R = usPrev6R;
        usPrev4L = pL[3]; usPrev4R = pR[3]; usPrev5L = pL[4]; usPrev5R = pR[4]; usPrev6L = pL[5]; usPrev6R = pR[5];
        const double dslow4 = ubDslow4.lp(d4, upperSnubDeltaA), dslow5 = ubDslow5.lp(d5, upperSnubDeltaA), dslow6 = ubDslow6.lp(d6, upperSnubDeltaA);
        const double cslow4 = ubCslow4.lp(c4, upperSnubCurveA), cslow5 = ubCslow5.lp(c5, upperSnubCurveA), cslow6 = ubCslow6.lp(c6, upperSnubCurveA);
        const double shape4 = std::max(clamp((d4 - dslow4 * 1.75) / std::max(dslow4 * 2.60 + tiny, tiny), 0.0, 1.0), clamp((c4 - cslow4 * 1.85) / std::max(cslow4 * 2.75 + tiny, tiny), 0.0, 1.0));
        const double shape5 = std::max(clamp((d5 - dslow5 * 1.70) / std::max(dslow5 * 2.35 + tiny, tiny), 0.0, 1.0), clamp((c5 - cslow5 * 1.78) / std::max(cslow5 * 2.45 + tiny, tiny), 0.0, 1.0));
        const double shape6 = std::max(clamp((d6 - dslow6 * 1.65) / std::max(dslow6 * 2.20 + tiny, tiny), 0.0, 1.0), clamp((c6 - cslow6 * 1.72) / std::max(cslow6 * 2.30 + tiny, tiny), 0.0, 1.0));
        double ngate4 = clamp(onset4 * shape4 * (0.25 + 0.75 * spike4), 0.0, 1.0);
        double ngate5 = clamp(onset5 * shape5 * (0.22 + 0.78 * spike5), 0.0, 1.0);
        double ngate6 = clamp(onset6 * shape6 * (0.18 + 0.82 * spike6) * (0.20 + 0.80 * std::max(onset5, ngate5)), 0.0, 1.0);
        ngate4 = ubGate4.run(ngate4, upperSnubGainAttack, upperSnubGainRelease);
        ngate5 = ubGate5.run(ngate5, upperSnubGainAttack, upperSnubGainRelease);
        ngate6 = ubGate6.run(ngate6, upperSnubGainAttack, upperSnubGainRelease);
        const double th4 = std::max(ceiling * 0.105, std::min(ceiling * (0.46 - 0.07 * usAmt), slow4 * (1.36 - 0.22 * usAmt)));
        const double th5 = std::max(ceiling * 0.080, std::min(ceiling * (0.36 - 0.075 * usAmt), slow5 * (1.26 - 0.24 * usAmt)));
        const double th6 = std::max(ceiling * 0.060, std::min(ceiling * (0.30 - 0.060 * usAmt), slow6 * (1.18 - 0.18 * usAmt)));
        pL[3] = purePeakRound(pL[3], th4, usAmt * ngate4 * 0.62, 0.42 + 0.10 * usAmt);
        pR[3] = purePeakRound(pR[3], th4, usAmt * ngate4 * 0.62, 0.42 + 0.10 * usAmt);
        pL[4] = purePeakRound(pL[4], th5, usAmt * ngate5 * 0.92, 0.48 + 0.14 * usAmt);
        pR[4] = purePeakRound(pR[4], th5, usAmt * ngate5 * 0.92, 0.48 + 0.14 * usAmt);
        const double lim6Gain = ubLim6.linkedLimiter(lvl6, th6, upperSnubGainAttack, upperSnubGainRelease);
        const double tgain6 = 1.0 - usAmt * ngate6 * 0.62 * (1.0 - lim6Gain);
        pL[5] *= tgain6;
        pR[5] *= tgain6;
    }

    double sumL = 0.0, sumR = 0.0;
    for (std::size_t i = 0; i < 6; ++i) {
        sumL += pL[i];
        sumR += pR[i];
    }
    if (recombControl > 0.0) {
        l = sumL + recombControl * (softClipKnee(sumL, recombClipThresh, recombClipKnee) - sumL);
        r = sumR + recombControl * (softClipKnee(sumR, recombClipThresh, recombClipKnee) - sumR);
    } else {
        l = sumL;
        r = sumR;
    }
    if (transitionFill > 0.0) { l = transitionPeakL.run(l); r = transitionPeakR.run(r); }
    if (lowBassFloor > 0.0) { l = lowFloorL.run(l); r = lowFloorR.run(r); }
    if (postXt2SmoothDriveAmt > 0.0000001) {
        l = postXt2RoundL.run(l, postXt2SmoothDriveAmt) * postXt2SmoothRecoveryGain;
        r = postXt2RoundR.run(r, postXt2SmoothDriveAmt) * postXt2SmoothRecoveryGain;
    }

    double mid = (l + r) * 0.5;
    double side = (l - r) * 0.5 * sideScale;
    if (stereoMode == 1) {
        side = softClipKnee(side, std::max(0.05, std::abs(mid) * 0.90 + 0.08), 0.30);
    }
    l = mid + side;
    r = mid - side;

    l *= preFinalDriveGain;
    r *= preFinalDriveGain;
    if (finalThresholdDriveS <= 0.0) finalThresholdDriveS = finalThresholdDriveTarget;
    finalThresholdDriveS = finalThresholdDriveTarget + finalThresholdSmooth * (finalThresholdDriveS - finalThresholdDriveTarget);
    const double finalThresholdMakeupCancel = finalThresholdDriveS > 1.0 && finalThresholdMakeup < 0.999999
        ? std::exp(-std::log(finalThresholdDriveS) * (1.0 - finalThresholdMakeup)) : 1.0;
    l *= finalThresholdDriveS;
    r *= finalThresholdDriveS;
    if (finalThresholdDriveS > 1.000001) {
        const double thresholdGuardDet = absmax2(l, r);
        const double thresholdGuardTarget = thresholdGuardDet > clipRef
            ? clipRef / std::max(thresholdGuardDet, tiny) : 1.0;
        finalThresholdGuardGain = thresholdGuardTarget < finalThresholdGuardGain
            ? thresholdGuardTarget
            : thresholdGuardTarget + clipRelease * (finalThresholdGuardGain - thresholdGuardTarget);
        l *= finalThresholdGuardGain;
        r *= finalThresholdGuardGain;
    } else {
        finalThresholdGuardGain = 1.0 + clipRelease * (finalThresholdGuardGain - 1.0);
    }
    const double clipFullEnv = finalFullEnv.run(absmax2(l, r), clipAttack, clipRelease);
    const double levelFactor = clamp((clipFullEnv - ceiling * 0.55) / std::max(ceiling * 0.35, tiny), 0.0, 1.0);
    const double protectDb = peakOnlyFinalLimiter ? 0.0 : -2.0 * clipRestraint * levelFactor;
    const double dynamicClipGain = protectDb < 0.0 ? dbToLin(protectDb) : 1.0;
    const double finalDriveGain = peakOnlyFinalLimiter ? 1.0 : dynamicClipGain;
    l *= finalDriveGain;
    r *= finalDriveGain;
    const double preGain = preclip.linkedLimiter(absmax2(l, r), prelimitThresh, clipAttack, clipRelease);
    const double preTGain = 1.0 - prelimitMix * (1.0 - preGain);
    l *= preTGain;
    r *= preTGain;
    const double clipInL = l, clipInR = r;
    double clipL = clipInL, clipR = clipInR;
    if (!peakOnlyFinalLimiter) {
        clipL = softClipKnee(softClipKnee(clipInL, clipStage1, clipKnee1), clipStage2, clipKnee2);
        clipR = softClipKnee(softClipKnee(clipInR, clipStage1, clipKnee1), clipStage2, clipKnee2);
    }
    if (dcCancel > 0.0) {
        const double distLowL = distLpfL.lp(clipL - clipInL, distCancelA);
        const double distLowR = distLpfR.lp(clipR - clipInR, distCancelA);
        l = clipL - distLowL * dcCancel * 0.45;
        r = clipR - distLowR * dcCancel * 0.45;
    } else {
        l = clipL;
        r = clipR;
    }
    if (topFilterMode == 1) {
        l = lpf15_1L.run(l); r = lpf15_1R.run(r);
    } else if (topFilterMode >= 2) {
        l = lpf15_2L.run(lpf15_1L.run(l));
        r = lpf15_2R.run(lpf15_1R.run(r));
    }
    l *= finalThresholdMakeupCancel;
    r *= finalThresholdMakeupCancel;
    if (smoothDriveRounderAmt > 0.0000001) {
        l = finalRoundL.run(l, smoothDriveRounderAmt);
        r = finalRoundR.run(r, smoothDriveRounderAmt);
    }
    l *= outputGain;
    r *= outputGain;
    const double ovL = fcsResL.lp(l - hardLimit(l, fcsThreshSetting), fcsResidueA);
    const double ovR = fcsResR.lp(r - hardLimit(r, fcsThreshSetting), fcsResidueA);
    if (effectiveFinalLookahead > 0) {
        int read = fcsWrite - effectiveFinalLookahead;
        if (read < 0) read += fcsBufferLength;
        const double delayedL = fcsBufL[static_cast<std::size_t>(read)];
        const double delayedR = fcsBufR[static_cast<std::size_t>(read)];
        const double delayedOvL = fcsBufOL[static_cast<std::size_t>(read)];
        const double delayedOvR = fcsBufOR[static_cast<std::size_t>(read)];
        fcsBufL[static_cast<std::size_t>(fcsWrite)] = l;
        fcsBufR[static_cast<std::size_t>(fcsWrite)] = r;
        fcsBufOL[static_cast<std::size_t>(fcsWrite)] = ovL;
        fcsBufOR[static_cast<std::size_t>(fcsWrite)] = ovR;
        fcsWrite = (fcsWrite + 1) % fcsBufferLength;
        l = delayedL - delayedOvL * overshootAmt;
        r = delayedR - delayedOvR * overshootAmt;
    } else {
        l -= ovL * overshootAmt;
        r -= ovR * overshootAmt;
    }
    l = hardLimit(l, ceiling);
    r = hardLimit(r, ceiling);
    return {static_cast<float>(l), static_cast<float>(r)};
}

void OptiLabCore::processInterleaved(float* samples, std::size_t frames, std::size_t channels) {
    if (!samples || channels == 0) {
        return;
    }
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float left = samples[frame * channels];
        float right = channels > 1 ? samples[frame * channels + 1] : left;
        auto processed = processSample(left, right);
        samples[frame * channels] = processed.first;
        if (channels > 1) {
            samples[frame * channels + 1] = processed.second;
            for (std::size_t ch = 2; ch < channels; ++ch) {
                samples[frame * channels + ch] = 0.0f;
            }
        }
    }
}

void OptiLabCore::processPlanar(float* left, float* right, std::size_t frames) {
    if (!left) {
        return;
    }
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float inputLeft = left[frame];
        const float inputRight = right ? right[frame] : inputLeft;
        const auto processed = processSample(inputLeft, inputRight);
        left[frame] = processed.first;
        if (right) {
            right[frame] = processed.second;
        }
    }
}

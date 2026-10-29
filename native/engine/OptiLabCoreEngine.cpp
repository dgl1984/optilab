#include "OptiLabCoreEngine.h"
#include "OptiLabDenormalGuard.h"
#include "OptiLabCrossover.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
constexpr double tiny = 0.000001;

// Full-Shape band recovery in dB for the coherent steep Density crossovers.
// These are crossover-domain calibration targets, not a post-Density EQ.
// Native mode and Shape 0 leave the existing voicing untouched; higher Shape
// values can progressively recover the broad band-energy relationship that
// Classic Parallel presents to Density. Serial and Classic modes stay native.
constexpr std::array<double, 5> densityShapeUpperScale{{1.0, 1.0, 1.0, 0.18, 0.15}};
constexpr std::array<std::array<double, 6>, 5> densityShapeRecoveryDb{{
    {{ 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 }}, // Gentle Serial: native
    {{ 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 }}, // Smooth Serial: native
    {{ 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 }}, // Classic Parallel: native
    {{-0.04, 0.95, 1.68, 0.87, 0.47, 0.08 }}, // Steep Coherent LR4
    {{-0.05, 0.93, 1.80, 0.61, 0.50, 0.02 }}  // Extreme Coherent LR8
}};
}

double OptiLabCoreEngine::OnePole::lp(double x, double a) {
    z += a * (x - z);
    return z;
}

double OptiLabCoreEngine::OnePole::lp2(double x, double a) {
    z1 += a * (x - z1);
    z2 += a * (z1 - z2);
    return z2;
}

double OptiLabCoreEngine::OnePole::hp(double x, double a) {
    z += a * (x - z);
    return x - z;
}

void OptiLabCoreEngine::OnePole::reset() {
    z = z1 = z2 = 0.0;
}

double OptiLabCoreEngine::Allpass1::run(double x, double c) {
    const double y = -c * x + x1 + c * y1;
    x1 = x;
    y1 = y;
    return y;
}

void OptiLabCoreEngine::Allpass1::reset() {
    x1 = y1 = 0.0;
}

double OptiLabCoreEngine::FmFilter::emphasize(double x, double k) {
    if (!primed) {
        x1 = x;
        primed = true;
        return x;
    }
    const double y = x + k * (x - x1);
    x1 = x;
    return y;
}

double OptiLabCoreEngine::FmFilter::deemphasize(double y, double k) {
    if (!primed) {
        x1 = y;
        primed = true;
        return y;
    }
    const double x = (y + k * x1) / std::max(1.0 + k, 0.000000001);
    x1 = x;
    return x;
}

void OptiLabCoreEngine::FmFilter::reset() {
    x1 = 0.0;
    primed = false;
}

double OptiLabCoreEngine::PeakEnv::run(double det, double attack, double release) {
    e = det > e ? det + attack * (e - det) : det + release * (e - det);
    return e;
}

double OptiLabCoreEngine::PeakEnv::set(double value) {
    e = value;
    return e;
}

double OptiLabCoreEngine::GainCell::bandLimitPd(double det, double thresh, double attack, double relBase, double relSlow, double pdAmt) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double depth = OptiLabCoreEngine::clamp((env - thresh) / std::max(thresh * 2.5, tiny), 0.0, 1.0);
    double relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    env = det > env ? det + attack * (env - det) : det + relNow * (env - det);
    env = std::max(env, tiny);
    double targetGain = env > thresh ? thresh / env : 1.0;
    targetGain = OptiLabCoreEngine::clamp(targetGain, 0.10, 1.0);
    depth = OptiLabCoreEngine::clamp((1.0 - gain) / 0.75, 0.0, 1.0);
    relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + relNow * (gain - targetGain);
    return gain;
}

double OptiLabCoreEngine::GainCell::bandLimitPdSoft(double det, double thresh, double attack, double relBase, double relSlow,
                                              double pdAmt, double ratio, double knee, double minGain) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double depth = OptiLabCoreEngine::clamp((env - thresh) / std::max(thresh * 2.5, tiny), 0.0, 1.0);
    double relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    env = det > env ? det + attack * (env - det) : det + relNow * (env - det);
    env = std::max(env, tiny);

    const double over = env / std::max(thresh, tiny);
    double targetGain = 1.0;
    if (over > 1.0) {
        const double soft = OptiLabCoreEngine::clamp((over - 1.0) / std::max((over - 1.0) + knee, tiny), 0.0, 1.0);
        const double compressed = std::pow(over, (1.0 / std::max(ratio, 1.01)) - 1.0);
        targetGain = 1.0 - soft * (1.0 - compressed);
    }
    targetGain = OptiLabCoreEngine::clamp(targetGain, minGain, 1.0);
    depth = OptiLabCoreEngine::clamp((1.0 - gain) / 0.75, 0.0, 1.0);
    relNow = relBase * (1.0 - pdAmt * depth) + relSlow * (pdAmt * depth);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + relNow * (gain - targetGain);
    return gain;
}

double OptiLabCoreEngine::GainCell::linkedLimiter(double det, double thresh, double attack, double release) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    double targetGain = det > thresh ? thresh / std::max(det, tiny) : 1.0;
    targetGain = OptiLabCoreEngine::clamp(targetGain, 0.08, 1.0);
    gain = targetGain < gain ? targetGain + attack * (gain - targetGain) : targetGain + release * (gain - targetGain);
    return gain;
}

double OptiLabCoreEngine::EventCell::run(
    double det,
    double threshDb,
    double quietThreshold,
    double attack,
    double sampleRate,
    double minimumRelease,
    double maximumRelease
) {
    if (gain <= 0.0) {
        gain = 1.0;
    }
    // linToDb() clamps its input to 1e-7. Below that floor, every supported
    // limiter threshold produces zero event gain reduction. Avoid entering
    // log()/exp() for settled silence and very quiet tails.
    if (env == 0.0 && det < quietThreshold) {
        gain = 1.0;
        return gain;
    }
    double instantDb = OptiLabCoreEngine::linToDb(std::max(det, 0.000000000001)) - threshDb;
    instantDb = std::max(instantDb, 0.0);
    // This is the exact steady-state result of the release and gain equations.
    // It is common for several bands at once and removes two exponentials per
    // inactive band without changing state or output.
    if (env == 0.0 && instantDb == 0.0) {
        gain = 1.0;
        return gain;
    }
    if (instantDb > env) {
        env = instantDb + attack * (env - instantDb);
    } else {
        const double releaseSeconds = OptiLabCoreEngine::clamp(env / 300.0, 0.004, 0.060);
        const double release =
            env < 1.2 ? minimumRelease :
            env > 18.0 ? maximumRelease :
            std::exp(-1.0 / std::max(1.0, sampleRate * releaseSeconds));
        env = instantDb + release * (env - instantDb);
    }
    if (env < 0.00001) {
        env = 0.0;
    }
    // dbToLin(-18.5 dB) is already below the cell's 0.12 minimum gain.
    // Use a conservative margin above the exact clamp boundary so this fast
    // path is numerically identical to the expression below.
    if (env > 18.5) {
        gain = 0.12;
        return gain;
    }
    gain = OptiLabCoreEngine::clamp(OptiLabCoreEngine::dbToLin(-env), 0.12, 1.0);
    return gain;
}

double OptiLabCoreEngine::SmoothRounder::run(double x, double amt) {
    amt = OptiLabCoreEngine::clamp(amt, 0.0, 1.0);
    if (amt <= 0.0000001) {
        return x;
    }
    if (x == 0.0) {
        prevSat = 0.0;
        return 0.0;
    }
    const double sat = std::sin(OptiLabCoreEngine::clamp(x, -pi * 0.5, pi * 0.5));
    const double apply = OptiLabCoreEngine::clamp(std::abs(prevSat + sat) * 0.5 * amt, 0.0, 1.0);
    const double y = x * (1.0 - apply) + sat * apply;
    prevSat = sat;
    return y;
}

void OptiLabCoreEngine::SmoothRounder::reset() {
    prevSat = 0.0;
}

double OptiLabCoreEngine::DcClipper::run(double x, double th, double amt, double kneeMul, double cancelAmt, double a) {
    const double clipped = OptiLabCoreEngine::softClipKnee(x, th, kneeMul);
    double y = clipped;
    if (cancelAmt > 0.0) {
        const double dist = clipped - x;
        dcZ += a * (dist - dcZ);
        y -= dcZ * cancelAmt;
    }
    return x * (1.0 - amt) + y * amt;
}

void OptiLabCoreEngine::DcClipper::reset() {
    dcZ = 0.0;
}

double OptiLabCoreEngine::BassClipper::run(double x, double th, double amt, double kneeMul, double drive, double resMix, double a) {
    drive = std::max(drive, 1.0);
    const double driven = x * drive;
    const double clipped = OptiLabCoreEngine::softClipKnee(driven, th, kneeMul) / drive;
    const double dist = clipped - x;
    z1 += a * (dist - z1);
    z2 += a * (z1 - z2);
    const double clipMix = amt * (0.50 + 0.50 * amt);
    const double residueMix = amt * resMix * (0.35 + 0.65 * amt);
    return x + dist * clipMix + z2 * residueMix;
}

void OptiLabCoreEngine::BassClipper::reset() {
    z1 = z2 = 0.0;
}

void OptiLabCoreEngine::Biquad::setLowpass(double freq, double q, double sampleRate) {
    freq = OptiLabCoreEngine::clamp(freq, 20.0, sampleRate * 0.45);
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

void OptiLabCoreEngine::Biquad::setHighpass(double freq, double q, double sampleRate) {
    freq = OptiLabCoreEngine::clamp(freq, 20.0, sampleRate * 0.45);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double norm = 1.0 / (1.0 + alpha);
    a0 = ((1.0 + cw) * 0.5) * norm;
    a1 = -(1.0 + cw) * norm;
    a2 = a0;
    b1 = (-2.0 * cw) * norm;
    b2 = (1.0 - alpha) * norm;
}

void OptiLabCoreEngine::Biquad::setAllpass(double freq, double q, double sampleRate) {
    freq = OptiLabCoreEngine::clamp(freq, 20.0, sampleRate * 0.45);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double norm = 1.0 / (1.0 + alpha);
    a0 = (1.0 - alpha) * norm;
    a1 = (-2.0 * cw) * norm;
    a2 = 1.0;
    b1 = a1;
    b2 = a0;
}

void OptiLabCoreEngine::Biquad::setPeak(double freq, double q, double gainDb, double sampleRate) {
    freq = OptiLabCoreEngine::clamp(freq, 20.0, sampleRate * 0.45);
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

void OptiLabCoreEngine::Biquad::setLowShelf(double freq, double slope, double gainDb, double sampleRate) {
    freq = OptiLabCoreEngine::clamp(freq, 20.0, sampleRate * 0.45);
    const double A = std::exp(gainDb * 0.057564627324851);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw * 0.5 * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double sqrtA = std::sqrt(A);
    const double b0 = A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sqrtA * alpha);
    const double bb1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
    const double bb2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqrtA * alpha);
    const double aa0 = (A + 1.0) + (A - 1.0) * cw + 2.0 * sqrtA * alpha;
    const double aa1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
    const double aa2 = (A + 1.0) + (A - 1.0) * cw - 2.0 * sqrtA * alpha;
    const double norm = 1.0 / aa0;
    a0 = b0 * norm;
    a1 = bb1 * norm;
    a2 = bb2 * norm;
    b1 = aa1 * norm;
    b2 = aa2 * norm;
}

void OptiLabCoreEngine::Biquad::copyCoefficientsFrom(const Biquad& source) {
    a0 = source.a0;
    a1 = source.a1;
    a2 = source.a2;
    b1 = source.b1;
    b2 = source.b2;
}

double OptiLabCoreEngine::Biquad::run(double x) {
    const double y = a0 * x + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

void OptiLabCoreEngine::Biquad::reset() {
    x1 = x2 = y1 = y2 = 0.0;
}

void OptiLabCoreEngine::FoundationGuard::reset() {
    Biquad* filters[] = {
        &lp70L, &lp70R, &lp250L, &lp250R, &lp420L, &lp420R,
        &lp3700L, &lp3700R, &shelfL, &shelfR, &bodyL, &bodyR
    };
    for (Biquad* filter : filters) {
        filter->reset();
    }
    deepFast.e = usefulFast.e = midFast.e = 0.0;
    deepSlow.e = usefulSlow.e = midSlow.e = 0.0;
    primed = false;
    ageSamples = 0;
    controlCounter = 15;
    coefficientCounter = 63;
    fastDeepBoostEvidenceDb = fastUsefulBoostEvidenceDb = 0.0;
    fastDeepCutEvidenceDb = fastUsefulCutEvidenceDb = 0.0;
    deepBoostStateDb = usefulBoostStateDb = 0.0;
    deepCutStateDb = usefulCutStateDb = 0.0;
    deepTargetDb = usefulTargetDb = 0.0;
    deepTargetGain = usefulTargetGain = 1.0;
    voiceTarget = voiceGain = 1.0;
    feedbackB1Accum = feedbackB2Accum = 0.0;
    feedbackCount = 0;
    feedbackRatioDb = -120.0;
    feedbackServoDb = 0.0;
    feedbackBurden = 0.0;
}

double OptiLabCoreEngine::dbToLin(double db) {
    return std::exp(db * 0.11512925464970229);
}

double OptiLabCoreEngine::linToDb(double x) {
    return std::log(std::max(x, 0.0000001)) * 8.6858896380650366;
}

double OptiLabCoreEngine::clamp(double x, double lo, double hi) {
    return std::min(std::max(x, lo), hi);
}

double OptiLabCoreEngine::smoothstep01(double x) {
    x = clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

double OptiLabCoreEngine::adaptiveBassBoostCurve(double x) {
    x = clamp(x, 0.0, 1.0);
    double t = 0.0;
    if (x <= 0.25) {
        t = x;
        return -0.4571428571*t*t*t + 0.9142857143*t*t + t;
    }
    if (x <= 0.50) {
        t = x - 0.25;
        return -0.5468624845*t*t*t + 1.0510013351*t*t + 1.3714285714*t + 0.3;
    }
    if (x <= 0.65) {
        t = x - 0.50;
        return -60.4153393502*t*t*t + 10.4330174081*t*t + 1.7943925234*t + 0.7;
    }
    t = x - 0.65;
    return -0.9672424138*t*t*t - 0.7011439362*t*t + 0.8462623413*t + 1.0;
}

double OptiLabCoreEngine::adaptiveBassCutCurve(double x) {
    x = clamp(x, 0.0, 1.0);
    double t = 0.0;
    if (x <= 0.25) {
        t = x;
        return -0.4571428571*t*t*t + 0.9142857143*t*t + t;
    }
    if (x <= 0.50) {
        t = x - 0.25;
        return -0.5468624845*t*t*t + 1.0510013351*t*t + 1.3714285714*t + 0.3;
    }
    if (x <= 0.65) {
        t = x - 0.50;
        return clamp(-98.0269989616*t*t*t + 16.0747663551*t*t + 1.7943925234*t + 0.7, 0.0, 1.0);
    }
    return 1.0;
}

double OptiLabCoreEngine::absmax2(double a, double b) {
    return std::max(std::abs(a), std::abs(b));
}

double OptiLabCoreEngine::softClipKnee(double x, double th, double kneeMul) {
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

double OptiLabCoreEngine::limiterBandClip(double x, double limit, double amount, double soft) {
    double clipped = x;
    if (soft <= 0.0) {
        if (std::abs(x) > limit) {
            clipped = std::copysign(limit, x);
        }
    } else {
        const double magnitude = std::abs(x);
        const double sign = x < 0.0 ? -1.0 : 1.0;
        const double knee = limit * (1.0 - 0.70 * soft);
        const double span = std::max(0.000000001, limit - knee);
        if (magnitude > knee) {
            clipped = sign * (knee + span * (1.0 - std::exp(-(magnitude - knee) / span)));
        }
    }
    return x * (1.0 - amount) + clipped * amount;
}

double OptiLabCoreEngine::purePeakRound(double x, double th, double amt, double driveMul) {
    if (amt <= 0.0) {
        return x;
    }
    if (x == 0.0) {
        return 0.0;
    }
    th = std::max(th, tiny);
    const double d = driveMul / th;
    const double lim = pi * 0.5;
    const double y = std::sin(clamp(x * d, -lim, lim)) / d;
    return x * (1.0 - amt) + y * amt;
}

double OptiLabCoreEngine::hardLimit(double x, double ceilingValue) {
    return std::min(std::max(x, -ceilingValue), ceilingValue);
}

double OptiLabCoreEngine::hybridShaveToLimit(double x, double limit, double depthDb) {
    const double ax = std::abs(x);
    const double kneeStart = limit * dbToLin(-std::max(depthDb, 0.001));
    const double span = std::max(limit - kneeStart, 1.0e-12);
    if (ax <= kneeStart) return x;
    const double sign = x < 0.0 ? -1.0 : 1.0;
    const double over = ax - kneeStart;
    const double y = kneeStart + span * (1.0 - std::exp(-over / span));
    return sign * std::min(y, limit);
}

double OptiLabCoreEngine::hybridEventTonalCorrelation(std::int64_t nowAbs) const {
    constexpr int taps = 8;
    constexpr double searchFraction = 0.04;
    double best = -1.0;
    const std::int64_t onsets[3]{hybridOnset1, hybridOnset2, hybridOnset3};
    for (const std::int64_t onset : onsets) {
        if (onset <= -100000000LL) continue;
        const std::int64_t base = nowAbs - onset;
        if (base < hybridPeriodMinSamples || base > hybridPeriodMaxSamples) continue;
        for (int searchIndex = -3; searchIndex <= 3; ++searchIndex) {
            const auto lag = static_cast<int>(std::floor(
                static_cast<double>(base) *
                (1.0 + searchFraction * static_cast<double>(searchIndex) / 3.0) + 0.5));
            if (lag < hybridPeriodMinSamples || lag > hybridPeriodMaxSamples) continue;
            double dot = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;
            int count = 0;
            for (int tap = 0; tap < taps; ++tap) {
                const int offset = static_cast<int>(std::floor(
                    static_cast<double>(tap) * static_cast<double>(std::max(1, lag - 1)) /
                    static_cast<double>(taps - 1) + 0.5));
                const std::int64_t aAbs = nowAbs - offset;
                const std::int64_t bAbs = aAbs - lag;
                if (bAbs < 0) continue;
                const int a = static_cast<int>(aAbs % static_cast<std::int64_t>(hybridRawL.size()));
                const int b = static_cast<int>(bAbs % static_cast<std::int64_t>(hybridRawL.size()));
                const double al = hybridRawL[static_cast<std::size_t>(a)];
                const double ar = hybridRawR[static_cast<std::size_t>(a)];
                const double bl = hybridRawL[static_cast<std::size_t>(b)];
                const double br = hybridRawR[static_cast<std::size_t>(b)];
                dot += al * bl + ar * br;
                energyA += al * al + ar * ar;
                energyB += bl * bl + br * br;
                ++count;
            }
            if (count >= 3 && energyA > 1.0e-12 && energyB > 1.0e-12) {
                best = std::max(best, dot / std::sqrt(energyA * energyB));
            }
        }
    }
    return std::max(0.0, best);
}

void OptiLabCoreEngine::processDelivery(
    double& left,
    double& right,
    double& referenceLeft,
    double& referenceRight,
    double& controlGain
) {
    const int write = deliveryWrite;
    deliveryBufL[static_cast<std::size_t>(write)] = left;
    deliveryBufR[static_cast<std::size_t>(write)] = right;
    deliveryRefL[static_cast<std::size_t>(write)] = referenceLeft;
    deliveryRefR[static_cast<std::size_t>(write)] = referenceRight;
    deliveryControlGain[static_cast<std::size_t>(write)] = controlGain;
    deliveryRequired[static_cast<std::size_t>(write)] = 1.0;
    deliveryEnvelope.update(write, 1.0);

    for (int tap = deliveryDetectorTaps - 1; tap > 0; --tap) {
        deliveryHistL[static_cast<std::size_t>(tap)] =
            deliveryHistL[static_cast<std::size_t>(tap - 1)];
        deliveryHistR[static_cast<std::size_t>(tap)] =
            deliveryHistR[static_cast<std::size_t>(tap - 1)];
    }
    deliveryHistL[0] = left;
    deliveryHistR[0] = right;

    // Candidate-3 sustained guard. The low-passed detector intentionally
    // follows program foundation rather than individual waveform peaks. Its
    // rolling minimum remembers the most restrictive recent gain request;
    // this prevents release modulation on sustained bass/brass while leaving
    // the reconstruction-aware fast path responsible for isolated peaks.
    deliverySustainedL += deliverySustainedCoeff * (left - deliverySustainedL);
    deliverySustainedR += deliverySustainedCoeff * (right - deliverySustainedR);
    deliverySustainedPeak = std::max(
        deliverySustainedPeak, absmax2(deliverySustainedL, deliverySustainedR));
    if (--deliverySustainedChunkCounter <= 0) {
        const double sustainedRequired = deliverySustainedPeak > deliveryTarget
            ? deliveryTarget / std::max(deliverySustainedPeak, 1.0e-12) : 1.0;
        deliverySustainedHistory[static_cast<std::size_t>(deliverySustainedHistoryWrite)] =
            sustainedRequired;
        deliverySustainedHistoryWrite =
            (deliverySustainedHistoryWrite + 1) % deliverySustainedHistoryLength;
        deliverySustainedTarget = *std::min_element(
            deliverySustainedHistory.begin(), deliverySustainedHistory.end());
        deliverySustainedPeak = 0.0;
        deliverySustainedChunkCounter = deliverySustainedChunkSamples;
    }
    deliverySustainedSmoothed += deliverySustainedSmoothCoeff *
        (deliverySustainedTarget - deliverySustainedSmoothed);

    double peak = absmax2(left, right);
    for (int phase = 0; phase < deliveryDetectorPhases; ++phase) {
        double sumL = 0.0;
        double sumR = 0.0;
        for (int tap = 0; tap < deliveryDetectorTaps; ++tap) {
            const double coefficient = deliveryCoeff[static_cast<std::size_t>(
                tap * deliveryDetectorPhases + phase)];
            sumL += deliveryHistL[static_cast<std::size_t>(tap)] * coefficient;
            sumR += deliveryHistR[static_cast<std::size_t>(tap)] * coefficient;
        }
        peak = std::max(peak, absmax2(sumL, sumR));
    }

    const double required = peak > deliveryTarget
        ? deliveryTarget / std::max(peak, 1.0e-12) : 1.0;
    int detectorIndex = write - deliveryDetectorDelaySamples;
    if (detectorIndex < 0) detectorIndex += deliveryBufferLength;
    deliveryRequired[static_cast<std::size_t>(detectorIndex)] = std::min(
        deliveryRequired[static_cast<std::size_t>(detectorIndex)], required);
    deliveryEnvelope.update(detectorIndex, deliveryRequired[static_cast<std::size_t>(detectorIndex)]);

    int read = write - deliveryLookaheadSamples;
    if (read < 0) read += deliveryBufferLength;
    const double fastTargetGain = deliveryEnvelope.minimum(read, deliveryAnticipationSamples + 1,
        [&](int ahead) noexcept {
            const double u = static_cast<double>(ahead)
                / static_cast<double>(deliveryAnticipationSamples);
            return u * u * (3.0 - 2.0 * u);
        });
    const double targetGain = std::min(fastTargetGain, deliverySustainedSmoothed);

    deliveryGain = targetGain < deliveryGain
        ? targetGain
        : targetGain + deliveryRelease * (deliveryGain - targetGain);
    if (deliveryGain > 0.999999999) deliveryGain = 1.0;
    left = deliveryBufL[static_cast<std::size_t>(read)] * deliveryGain;
    right = deliveryBufR[static_cast<std::size_t>(read)] * deliveryGain;
    referenceLeft = deliveryRefL[static_cast<std::size_t>(read)];
    referenceRight = deliveryRefR[static_cast<std::size_t>(read)];
    controlGain = deliveryControlGain[static_cast<std::size_t>(read)] * deliveryGain;
    deliveryRequired[static_cast<std::size_t>(read)] = 1.0;
    deliveryEnvelope.update(read, 1.0);
    deliveryWrite = (deliveryWrite + 1) % deliveryBufferLength;
}

double OptiLabCoreEngine::apCoeff(double freq) const {
    freq = clamp(freq, 5.0, sampleRate * 0.45);
    const double t = std::tan(pi * freq / sampleRate);
    return (t - 1.0) / (t + 1.0);
}

bool OptiLabCoreEngine::isSampleRateSupported(double rate) noexcept {
    return std::isfinite(rate)
        && rate >= minimumSupportedSampleRate
        && rate <= maximumSupportedSampleRate;
}

OptiLabCoreEngine::Parameters OptiLabCoreEngine::defaultParameters(Mode mode) noexcept {
    Parameters defaults;
    defaults.mode = mode;
    // Report the same independent Input Drive value exposed by a fresh Core instance.
    defaults.inputDriveDb = mode == Mode::PodcastLeveler ? 3.5 : 0.0;
    return defaults;
}

const std::vector<OptiLabCoreEngine::ParameterInfo>& OptiLabCoreEngine::parameterInfo() {
    static const std::vector<ParameterInfo> info(
        optilab_parameters::catalog.begin(), optilab_parameters::catalog.end());
    return info;
}

bool OptiLabCoreEngine::isParameterSupported(std::string_view key) {
    // Retired public control. Accept the key when loading old sessions/presets
    // so legacy state remains compatible, but ignore its value.
    if (key == "p085"
        || key == "p086"
        || key == "p087"
        || key == "p088") return true;
    const auto& info = parameterInfo();
    return std::any_of(info.begin(), info.end(), [&](const ParameterInfo& item) {
        return key == item.key;
    });
}

bool OptiLabCoreEngine::isHostAutomatable(std::string_view key) {
    const auto& info = parameterInfo();
    const auto found = std::find_if(info.begin(), info.end(), [&](const ParameterInfo& item) {
        return key == item.key;
    });
    return found != info.end() && found->exposure == ParameterExposure::Runtime;
}

bool OptiLabCoreEngine::isCoreInternalKey(std::string_view key) noexcept {
    return optilab_internal::contains(key);
}

bool OptiLabCoreEngine::isCoreValueSupported(std::string_view key) noexcept {
    return isParameterSupported(key) || isCoreInternalKey(key);
}

OptiLabCoreEngine::ParameterValues OptiLabCoreEngine::effectiveParameterValues() const {
    return {
#define OP_PARAMETER(type, member, initial, key, ...) {key, static_cast<double>(member)},
#include "OptiLabParameters.def"
#undef OP_PARAMETER
    };
}

OptiLabCoreEngine::ParameterValues OptiLabCoreEngine::effectiveCoreValues() const {
    auto values = effectiveParameterValues();
#define OP_INTERNAL(type, member, initial, key, ...) values.emplace(key, static_cast<double>(member));
#include "OptiLabInternalState.def"
#undef OP_INTERNAL
    return values;
}

void OptiLabCoreEngine::applyParameterOverrides() {
    using namespace optilab_parameters;
    const auto value = [&](const Descriptor& info, double current) {
        const auto found = params.overrides.find(info.key);
        if (found == params.overrides.end()) return current;
        const double result = clamp(found->second, info.minimum, info.maximum);
        return info.integer ? std::round(result) : result;
    };
    const auto afterAssignment = [&](Hook hook) {
        if (hook == Hook::LatencyProfile) {
            if (p003 == 0) {
                c03 = 0.18;
                c25 = 0.36;
            } else if (p003 == 2) {
                c03 = 1.50;
                c25 = 1.50;
            }
        } else if (hook == Hook::ClipperStyle) {
            const int clipperStyleWork = std::clamp(p062, 0, 7);
            clipperMsMode = clipperStyleWork >= 4;
            clipperStyleBase = clipperMsMode ? clipperStyleWork - 4 : clipperStyleWork;
        }
    };
    // Catalog order matters: defaults are applied before explicit overrides.
#define OP_PARAMETER(type, member, initial, ...) \
    member = static_cast<type>(value(get(Index::member), member)); \
    afterAssignment(get(Index::member).hook);
#include "OptiLabParameters.def"
#undef OP_PARAMETER
#define OP_INTERNAL(type, member, initial, key, integer, hook, defaults) \
    if (const auto it = params.internalOverrides.find(key); it != params.internalOverrides.end()) \
        member = static_cast<type>(integer ? std::round(it->second) : it->second);
#include "OptiLabInternalState.def"
#undef OP_INTERNAL
}

std::pair<double, double> OptiLabCoreEngine::processFoundationGuard(
    double preL, double preR, double boostedL, double boostedR
) {
    auto& f = foundationGuard;
    const double lp70L = f.lp70L.run(preL);
    const double lp70R = f.lp70R.run(preR);
    const double lp250L = f.lp250L.run(preL);
    const double lp250R = f.lp250R.run(preR);
    const double lp420L = f.lp420L.run(preL);
    const double lp420R = f.lp420R.run(preR);
    const double lp3700L = f.lp3700L.run(preL);
    const double lp3700R = f.lp3700R.run(preR);

    const double deepL = lp70L;
    const double deepR = lp70R;
    const double usefulL = lp250L - lp70L;
    const double usefulR = lp250R - lp70R;
    const double midL = lp3700L - lp420L;
    const double midR = lp3700R - lp420R;
    const double deepPower = (deepL * deepL + deepR * deepR) * 0.5;
    const double usefulPower = (usefulL * usefulL + usefulR * usefulR) * 0.5;
    const double midPower = (midL * midL + midR * midR) * 0.5;
    const double programDet = std::max(
        absmax2(preL, preR),
        std::max(absmax2(deepL, deepR), std::max(absmax2(usefulL, usefulR), absmax2(midL, midR)))
    );

    if (programDet >= 0.00003) {
        ++f.ageSamples;
    }
    if (!f.primed && programDet > 0.00001) {
        f.deepFast.set(deepPower);
        f.usefulFast.set(usefulPower);
        f.midFast.set(midPower);
        f.deepSlow.set(deepPower);
        f.usefulSlow.set(usefulPower);
        f.midSlow.set(midPower);
        f.primed = true;
    }

    const double fastDeep = f.deepFast.run(deepPower, f.detAttack, f.detRelease);
    const double fastUseful = f.usefulFast.run(usefulPower, f.detAttack, f.detRelease);
    const double fastMid = f.midFast.run(midPower, f.detAttack, f.detRelease);
    const double slowDeep = f.deepSlow.run(deepPower, f.slowAttack, f.slowRelease);
    const double slowUseful = f.usefulSlow.run(usefulPower, f.slowAttack, f.slowRelease);
    const double slowMid = f.midSlow.run(midPower, f.slowAttack, f.slowRelease);

    ++f.controlCounter;
    if (f.controlCounter >= 16) {
        f.controlCounter = 0;

        // Consume the previous control block's post-Bass density balance.
        // This is the same one-block feedback topology used by Core.
        if (c06 != 0 && f.feedbackCount > 0) {
            const double b1Energy =
                f.feedbackB1Accum / static_cast<double>(f.feedbackCount);
            const double b2Energy =
                f.feedbackB2Accum / static_cast<double>(f.feedbackCount);
            f.feedbackRatioDb = linToDb(std::sqrt(
                std::max(b1Energy, 1.0e-12)
                / std::max(b2Energy, 1.0e-12)));
        }
        f.feedbackB1Accum = f.feedbackB2Accum = 0.0;
        f.feedbackCount = 0;

        double fastDeepBoostDb = 0.0;
        double fastUsefulBoostDb = 0.0;
        double slowDeepBoostDb = 0.0;
        double slowUsefulBoostDb = 0.0;
        double slowDeepCutDb = 0.0;
        double slowUsefulCutDb = 0.0;
        double fastDeepCutDb = 0.0;
        double fastUsefulCutDb = 0.0;
        double slowDeepRatioDb = -120.0;
        double slowUsefulRatioDb = -120.0;

        const bool valid = fastMid > 1.0e-12 && programDet >= 0.00003;
        if (valid) {
            const auto ratioDb = [](double numerator, double denominator) {
                return OptiLabCoreEngine::linToDb(std::sqrt(
                    std::max(numerator, 1.0e-12) / std::max(denominator, 1.0e-12)
                ));
            };
            const double fd = ratioDb(fastDeep, fastMid);
            const double fu = ratioDb(fastUseful, fastMid);
            const double sd = ratioDb(slowDeep, slowMid);
            const double su = ratioDb(slowUseful, slowMid);
            slowDeepRatioDb = sd;
            slowUsefulRatioDb = su;

            fastDeepBoostDb = (
                4.0 * smoothstep01((-6.2 - fd) / 2.8) +
                3.0 * smoothstep01((-9.0 - fd) / 7.0) +
                0.7 * smoothstep01((-16.0 - fd) / 8.0)
            ) * f.boostScale;
            slowDeepBoostDb = (
                4.0 * smoothstep01((-6.2 - sd) / 2.8) +
                3.0 * smoothstep01((-9.0 - sd) / 7.0) +
                0.7 * smoothstep01((-16.0 - sd) / 8.0)
            ) * f.boostScale;
            fastUsefulBoostDb = (
                3.5 * smoothstep01((-0.35 - fu) / 1.65) +
                2.7 * smoothstep01((-2.0 - fu) / 4.0) +
                1.5 * smoothstep01((-6.0 - fu) / 4.5)
            ) * smoothstep01((-5.8 - fd) / 2.5) * f.boostScale;
            slowUsefulBoostDb = (
                3.5 * smoothstep01((-0.35 - su) / 1.65) +
                2.7 * smoothstep01((-2.0 - su) / 4.0) +
                1.5 * smoothstep01((-6.0 - su) / 4.5)
            ) * smoothstep01((-5.8 - sd) / 2.5) * f.boostScale;

            slowDeepCutDb = (
                4.5 * smoothstep01((sd + 0.3) / 1.0) +
                0.5 * smoothstep01((sd - 0.7) / 2.3)
            ) * f.cutAuthority;
            const double leanPosition = smoothstep01((-0.5 - sd) / 2.0);
            const double usefulCutStart = 1.5 + 2.0 * leanPosition;
            slowUsefulCutDb = 4.5 * smoothstep01((su - usefulCutStart) / 1.5)
                * (0.20 + 0.80 * smoothstep01((su - sd - 0.6) / 1.5))
                * f.cutAuthority;
            slowDeepCutDb = std::max(
                slowDeepCutDb,
                0.90 * slowUsefulCutDb * smoothstep01((sd + 3.0) / 2.5)
            );
            if (f.ageSamples >= f.cutHoldoffSamples) {
                fastDeepCutDb = 5.0 * smoothstep01((fd - 0.5) / 2.2) * f.cutAuthority;
                fastUsefulCutDb = 4.5 * smoothstep01((fu - 3.2) / 1.6)
                    * smoothstep01((fd - 0.0) / 1.4) * f.cutAuthority;
                fastDeepCutDb = std::max(fastDeepCutDb, 0.90 * fastUsefulCutDb);
            }
        }

        const auto follow = [](double current, double target, double attack, double release) {
            return target > current
                ? target + attack * (current - target)
                : target + release * (current - target);
        };
        f.fastDeepBoostEvidenceDb = follow(
            f.fastDeepBoostEvidenceDb, fastDeepBoostDb,
            f.fastBoostAttackTick, f.fastBoostReleaseTick
        );
        f.fastUsefulBoostEvidenceDb = follow(
            f.fastUsefulBoostEvidenceDb, fastUsefulBoostDb,
            f.fastBoostAttackTick, f.fastBoostReleaseTick
        );
        f.fastDeepCutEvidenceDb = follow(
            f.fastDeepCutEvidenceDb, fastDeepCutDb,
            f.fastCutAttackTick, f.fastCutReleaseTick
        );
        f.fastUsefulCutEvidenceDb = follow(
            f.fastUsefulCutEvidenceDb, fastUsefulCutDb,
            f.fastCutAttackTick, f.fastCutReleaseTick
        );

        const double deepBoostTarget = std::max(slowDeepBoostDb, 0.70 * f.fastDeepBoostEvidenceDb);
        const double usefulBoostTarget = std::max(slowUsefulBoostDb, 0.70 * f.fastUsefulBoostEvidenceDb);
        const double deepCutTarget = std::max(slowDeepCutDb, 0.95 * f.fastDeepCutEvidenceDb);
        const double usefulCutTarget = std::max(slowUsefulCutDb, 0.95 * f.fastUsefulCutEvidenceDb);
        const double globalCutTarget = std::max(deepCutTarget, usefulCutTarget);

        f.deepBoostStateDb = follow(
            f.deepBoostStateDb, deepBoostTarget, f.boostAttackTick,
            globalCutTarget > 0.60 ? f.boostSuppressTick : f.boostReleaseTick
        );
        f.usefulBoostStateDb = follow(
            f.usefulBoostStateDb, usefulBoostTarget, f.boostAttackTick,
            globalCutTarget > 0.60 ? f.boostSuppressTick : f.boostReleaseTick
        );
        f.deepCutStateDb = follow(
            f.deepCutStateDb, deepCutTarget, f.cutAttackTick,
            deepBoostTarget > 2.0 ? f.cutSuppressTick : f.cutReleaseTick
        );
        f.usefulCutStateDb = follow(
            f.usefulCutStateDb, usefulCutTarget, f.cutAttackTick,
            usefulBoostTarget > 2.0 ? f.cutSuppressTick : f.cutReleaseTick
        );

        const double deepRaw = f.deepBoostStateDb - f.deepCutStateDb;
        const double usefulRaw = f.usefulBoostStateDb - f.usefulCutStateDb;
        if (deepRaw * usefulRaw >= 0.0) {
            if (std::abs(deepRaw) >= std::abs(usefulRaw)) {
                f.deepTargetDb = deepRaw + (usefulRaw - deepRaw) * (0.08 * f.link);
                f.usefulTargetDb = usefulRaw + (deepRaw - usefulRaw) * (0.70 * f.link);
            } else {
                f.usefulTargetDb = usefulRaw + (deepRaw - usefulRaw) * (0.08 * f.link);
                f.deepTargetDb = deepRaw + (usefulRaw - deepRaw) * (0.70 * f.link);
            }
        } else {
            const double shared = (deepRaw + usefulRaw) * 0.5;
            f.deepTargetDb = deepRaw + (shared - deepRaw) * (0.10 * f.link);
            f.usefulTargetDb = usefulRaw + (shared - usefulRaw) * (0.10 * f.link);
        }

        if (c06 != 0) {
            // Core-style closed-loop refinement. Only moderate lean cases may
            // ask for extra foundation; cuts, strong boosts, and persistent
            // post-Bass burden reduce or veto the request.
            const double feedbackEligible = valid
                && f.ageSamples >= static_cast<std::size_t>(
                    std::floor(sampleRate * 5.0 + 0.5)) ? 1.0 : 0.0;
            const double feedbackModerate =
                smoothstep01((f.deepTargetDb - 0.8) / 1.0)
                * (1.0 - smoothstep01((f.deepTargetDb - 3.4) / 0.8));
            const double feedbackDeepLean =
                1.0 - smoothstep01((slowDeepRatioDb + 4.5) / 1.5);
            const double feedbackBodyLean =
                smoothstep01((-1.0 - slowUsefulRatioDb) / 0.6);
            const double feedbackNoCut =
                1.0 - smoothstep01(globalCutTarget / 0.25);
            const double feedbackPostRoom =
                1.0 - smoothstep01((f.feedbackRatioDb - 3.2) / 0.8);
            const double feedbackPostExcess =
                smoothstep01((f.feedbackRatioDb - 3.0) / 2.0);
            const double burdenCoeff = feedbackPostExcess > f.feedbackBurden
                ? f.feedbackBurdenAttackTick : f.feedbackBurdenReleaseTick;
            f.feedbackBurden = feedbackPostExcess
                + burdenCoeff * (f.feedbackBurden - feedbackPostExcess);
            const double feedbackActivityRoom =
                1.0 - smoothstep01((f.feedbackBurden - 0.30) / 0.45);

            const double feedbackRequestDb = f.feedbackMaxAssistDb
                * feedbackEligible
                * feedbackModerate
                * feedbackDeepLean
                * feedbackBodyLean
                * feedbackNoCut
                * feedbackPostRoom
                * feedbackActivityRoom;
            const bool feedbackForceRelease = !valid
                || globalCutTarget > 0.10
                || slowDeepRatioDb > -3.5
                || slowUsefulRatioDb > -0.20
                || f.feedbackBurden > 0.62;
            const double feedbackCoeff = feedbackRequestDb > f.feedbackServoDb
                ? f.feedbackAttackTick
                : feedbackForceRelease
                    ? f.feedbackFastReleaseTick : f.feedbackReleaseTick;
            f.feedbackServoDb = feedbackRequestDb
                + feedbackCoeff * (f.feedbackServoDb - feedbackRequestDb);
            f.deepTargetDb += f.feedbackServoDb;
        } else {
            f.feedbackRatioDb = -120.0;
            f.feedbackServoDb = 0.0;
            f.feedbackBurden = 0.0;
        }

        f.deepTargetGain = dbToLin(f.deepTargetDb);
        f.usefulTargetGain = dbToLin(f.usefulTargetDb);
        const double withdrawPosition = smoothstep01(clamp(globalCutTarget / 3.5, 0.0, 1.0));
        f.voiceTarget = 1.0 - clamp(f.withdrawAuthority * withdrawPosition, 0.0, 1.0);
    }

    const double voiceCoeff = f.voiceTarget < f.voiceGain ? f.voiceWithdraw : f.voiceReturn;
    f.voiceGain = f.voiceTarget + voiceCoeff * (f.voiceGain - f.voiceTarget);
    const double voicedL = preL + (boostedL - preL) * f.voiceGain;
    const double voicedR = preR + (boostedR - preR) * f.voiceGain;

    ++f.coefficientCounter;
    if (f.coefficientCounter >= 64) {
        f.coefficientCounter = 0;
        double bodyDb = 0.0;
        double bodyFreq = 90.0;
        double bodyQ = 1.10;
        if (f.usefulTargetDb >= 0.0) {
            bodyDb = std::min(3.0, f.usefulTargetDb * 0.50);
        } else {
            bodyDb = -std::min(4.5, std::max(0.0, -f.usefulTargetDb - 1.5) * 1.50);
            bodyFreq = 125.0;
            bodyQ = 0.85;
        }
        f.shelfL.setLowShelf(70.0, 1.0, f.deepTargetDb, sampleRate);
        f.shelfR.copyCoefficientsFrom(f.shelfL);
        f.bodyL.setPeak(bodyFreq, bodyQ, bodyDb, sampleRate);
        f.bodyR.copyCoefficientsFrom(f.bodyL);
    }

    return {
        f.bodyL.run(f.shelfL.run(voicedL)),
        f.bodyR.run(f.shelfR.run(voicedR))
    };
}

OptiLabCoreEngine::Activity OptiLabCoreEngine::activity() const noexcept {
    return {
        (!segmentOwnsSection(1) || params.sectionBypass[1])
            ? 1.0 : std::min(currentAgcLowEffGain, currentAgcHighEffGain),
        (!segmentOwnsSection(3) || params.sectionBypass[3])
            ? 1.0 : currentDensityGain,
        (!segmentOwnsSection(5) || params.sectionBypass[5])
            ? 1.0 : currentFinalGain,
    };
}

bool OptiLabCoreEngine::segmentOwnsSection(std::size_t section) const noexcept {
    return section < sectionOwnedCache.size() ? sectionOwnedCache[section] : false;
}

bool OptiLabCoreEngine::diagnosticRequestsSection(std::size_t section) const noexcept {
    return diagnosticSectionCache >= 0
        && static_cast<int>(section) == diagnosticSectionCache;
}

bool OptiLabCoreEngine::shouldRunSection(std::size_t section) const noexcept {
    return section < sectionRunCache.size() ? sectionRunCache[section] : false;
}

bool OptiLabCoreEngine::stopAtBoundary(int boundary) const noexcept {
    return boundary == stopBoundaryCache;
}

bool OptiLabCoreEngine::diagnosticTapIsInSegment() const noexcept {
    return diagnosticSectionCache < 0
        || sectionOwnedCache[static_cast<std::size_t>(diagnosticSectionCache)];
}

void OptiLabCoreEngine::refreshRoutingCache() noexcept {
    allSectionsBypassedCache = std::all_of(
        params.sectionBypass.begin(), params.sectionBypass.end(),
        [](bool bypassed) { return bypassed; });
    const int splitSection = std::clamp(c23, 0, 4) + 1;
    stopBoundaryCache = c20 == 1 ? splitSection : -1;

    switch (outputTap) {
    case OutputTap::InputTestpoint:
    case OutputTap::InputDelta: diagnosticSectionCache = 0; break;
    case OutputTap::AgcTestpoint:
    case OutputTap::AgcDelta: diagnosticSectionCache = 1; break;
    case OutputTap::BassTestpoint:
    case OutputTap::BassDelta: diagnosticSectionCache = 2; break;
    case OutputTap::DensityTestpoint:
    case OutputTap::DensityDelta: diagnosticSectionCache = 3; break;
    case OutputTap::StereoTestpoint:
    case OutputTap::StereoDelta: diagnosticSectionCache = 4; break;
    case OutputTap::FinalDelta: diagnosticSectionCache = 5; break;
    case OutputTap::FullChain:
    default: diagnosticSectionCache = -1; break;
    }

    bool anyOwnedActive = false;
    for (std::size_t section = 0; section < sectionOwnedCache.size(); ++section) {
        bool owned = true;
        if (c20 == 1) owned = static_cast<int>(section) < splitSection;
        else if (c20 == 2) owned = static_cast<int>(section) >= splitSection;
        sectionOwnedCache[section] = owned;
        const bool diagnostic = diagnosticSectionCache == static_cast<int>(section);
        sectionRunCache[section] = owned && (!params.sectionBypass[section] || diagnostic);
        anyOwnedActive = anyOwnedActive || (owned && !params.sectionBypass[section]);
    }

    const std::size_t mastering = sectionOwnedCache[1] && !params.sectionBypass[1]
        ? static_cast<std::size_t>(masteringLookaheadSamples) : 0u;
    const std::size_t density = sectionOwnedCache[3] && !params.sectionBypass[3]
        ? static_cast<std::size_t>(effectiveSnubberLookahead) : 0u;
    const std::size_t final = sectionOwnedCache[5] && !params.sectionBypass[5]
        ? static_cast<std::size_t>(
            effectiveFinalLookahead + hybridTotalDelaySamples + deliveryLookaheadSamples)
        : 0u;

    const bool diagnosticInSegment = diagnosticSectionCache < 0
        || sectionOwnedCache[static_cast<std::size_t>(diagnosticSectionCache)];
    const OutputTap effectiveTap = diagnosticInSegment ? outputTap : OutputTap::FullChain;
    if (outputTap == OutputTap::FullChain && !anyOwnedActive) {
        latencyCache = 0;
    } else {
        switch (effectiveTap) {
        case OutputTap::InputTestpoint:
        case OutputTap::InputDelta:
        case OutputTap::AgcTestpoint:
        case OutputTap::AgcDelta:
            latencyCache = 0; break;
        case OutputTap::BassTestpoint:
        case OutputTap::BassDelta:
            latencyCache = mastering; break;
        case OutputTap::DensityTestpoint:
        case OutputTap::DensityDelta:
            latencyCache = mastering + (sectionOwnedCache[3]
                ? static_cast<std::size_t>(effectiveSnubberLookahead) : 0u); break;
        case OutputTap::StereoTestpoint:
        case OutputTap::StereoDelta:
            latencyCache = mastering + density; break;
        case OutputTap::FinalDelta:
            latencyCache = mastering + density + (sectionOwnedCache[5]
                ? static_cast<std::size_t>(
                    effectiveFinalLookahead + hybridTotalDelaySamples + deliveryLookaheadSamples)
                : 0u); break;
        case OutputTap::FullChain:
        default:
            latencyCache = mastering + density + final; break;
        }
    }

    boundaryLatencyCache.fill(0u);
    for (int boundary = 0; boundary <= 6; ++boundary) {
        std::size_t delay = 0;
        if (boundary >= 2 && sectionOwnedCache[1] && !params.sectionBypass[1])
            delay += static_cast<std::size_t>(masteringLookaheadSamples);
        if (boundary >= 4 && sectionOwnedCache[3]
            && (!params.sectionBypass[3] || diagnosticSectionCache == 3))
            delay += static_cast<std::size_t>(effectiveSnubberLookahead);
        if (boundary >= 6) delay = latencyCache;
        boundaryLatencyCache[static_cast<std::size_t>(boundary)] = delay;
    }
}

std::size_t OptiLabCoreEngine::latencySamples() const noexcept {
    return latencyCache;
}

void OptiLabCoreEngine::ensureHybridCapacityForSampleRate() {
    // The recurrence detector needs enough raw history for three times its
    // 20 ms maximum period search; the burden accumulator needs 40 ms. Keep
    // the original ring dimensions whenever they are already sufficient so
    // <=192 kHz behavior retains the established ring topology.
    const auto historyForTime = static_cast<std::size_t>(
        std::floor(sampleRate * 0.060 + 0.5)) + 8u;
    const auto energyForTime = static_cast<std::size_t>(
        std::floor(sampleRate * 0.040 + 0.5)) + 2u;
    const std::size_t historyLength = std::max<std::size_t>(
        static_cast<std::size_t>(hybridBaseHistoryLength), historyForTime);
    const std::size_t energyLength = std::max<std::size_t>(
        static_cast<std::size_t>(hybridBaseEnergyLength), energyForTime);

    auto resizeZero = [](std::vector<double>& values, std::size_t count, double fill) {
        if (values.size() != count) values.assign(count, fill);
    };
    resizeZero(hybridRawL, historyLength, 0.0);
    resizeZero(hybridRawR, historyLength, 0.0);
    resizeZero(hybridCandidateL, historyLength, 0.0);
    resizeZero(hybridCandidateR, historyLength, 0.0);
    resizeZero(hybridShavedL, historyLength, 0.0);
    resizeZero(hybridShavedR, historyLength, 0.0);
    resizeZero(hybridRequiredGain, historyLength, 1.0);
    resizeZero(hybridBurden, historyLength, 1.0);
    resizeZero(hybridEventGate, historyLength, 0.0);
    resizeZero(hybridWorkLimit, historyLength, 1.0);
    resizeZero(hybridMakeup, historyLength, 1.0);
    resizeZero(hybridFinalRefL, historyLength, 0.0);
    resizeZero(hybridFinalRefR, historyLength, 0.0);
    resizeZero(hybridControlGain, historyLength, 1.0);
    resizeZero(hybridAttackShape, historyLength, 1.0);
    resizeZero(hybridRefEnergy, energyLength, 0.0);
    resizeZero(hybridDeltaEnergy, energyLength, 0.0);
}

void OptiLabCoreEngine::prepare(double newSampleRate) {
    if (!isSampleRateSupported(newSampleRate)) {
        throw std::invalid_argument("Unsupported OptiLab sample rate");
    }
    sampleRate = newSampleRate;
    ensureHybridCapacityForSampleRate();
    lastMode = -1;
    applyModeAndDerivedParameters();
    auxTapHistory.assign(static_cast<std::size_t>(newSampleRate * 0.1) + 1, {});
    reset();
}

void OptiLabCoreEngine::setParameters(const Parameters& newParameters) {
    for (const auto& item : newParameters.overrides) {
        if (!isParameterSupported(item.first)) {
            throw std::invalid_argument("Unknown Core engine parameter: " + item.first);
        }
        if (!std::isfinite(item.second)) {
            throw std::invalid_argument("Non-finite Core engine parameter: " + item.first);
        }
    }
    for (const auto& item : newParameters.internalOverrides) {
        if (!isCoreInternalKey(item.first)) {
            throw std::invalid_argument("Unknown Core internal value: " + item.first);
        }
        if (!std::isfinite(item.second)) {
            throw std::invalid_argument("Non-finite Core internal value: " + item.first);
        }
    }
    const int previousMode = static_cast<int>(params.mode);
    params = newParameters;
    const auto legacyTamer = params.overrides.find("p088");
    if (legacyTamer != params.overrides.end()
        && params.internalOverrides.find("c03") == params.internalOverrides.end()) {
        params.internalOverrides["c03"] = legacyTamer->second;
    }
    params.overrides.erase("p088");
    params.autoAdaptPct = clamp(params.autoAdaptPct, 0.0, 100.0);
    params.inputDriveDb = clamp(params.inputDriveDb, -18.0, 18.0);
    const int nextMode = static_cast<int>(params.mode);
    applyModeAndDerivedParameters();
    if (lastMode >= 0 && previousMode != nextMode) {
        reset();
    }
    lastMode = nextMode;
}

void OptiLabCoreEngine::setPreparedParameters(Mode mode, const ParameterValues& values, double drive, int selector) noexcept {
    bool changed = params.mode != mode;
    params.mode = mode;
    params.inputDriveDb = drive;
    for (auto& [key, value] : params.overrides) {
        const auto it = values.find(key);
        if (it == values.end()) continue;
        const double next = key == "p002" ? drive : key == "p001" ? double(selector) : it->second;
        changed = changed || (key != "p002" && value != next);
        value = next;
    }
    for (auto& [key, value] : params.internalOverrides) {
        const auto it = values.find(key);
        if (it == values.end()) continue;
        changed = changed || value != it->second;
        value = it->second;
    }
    if (changed) applyModeAndDerivedParameters();
    else { p002 = drive; inputGain = dbToLin(drive); }
    lastMode = static_cast<int>(mode);
}

void OptiLabCoreEngine::setParameterValue(std::string_view key, double value) {
    if (key == "p085"
        || key == "p086"
        || key == "p087") {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "Non-finite Core engine parameter: " + std::string(key));
        }
        return;
    }
    if (key == "p088") {
        if (!std::isfinite(value)) throw std::invalid_argument("Non-finite Core engine parameter: p088");
        return;
    }
    const auto info = std::find_if(
        parameterInfo().begin(), parameterInfo().end(),
        [&](const ParameterInfo& item) { return key == item.key; });
    if (info == parameterInfo().end()) {
        throw std::invalid_argument(
            "Unknown Core engine parameter: " + std::string(key));
    }
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            "Non-finite Core engine parameter: " + std::string(key));
    }
    value = clamp(value, info->minimum, info->maximum);
    if (info->integer) {
        value = std::round(value);
    }
    if (key == "p002") {
        params.inputDriveDb = value;
    }
    const auto existing = params.overrides.find(key);
    if (existing == params.overrides.end()) {
        // Normal plugin activation pre-populates every key. This insertion is
        // retained for standalone callers, which are not on the audio thread.
        params.overrides.emplace(std::string(key), value);
    } else {
        existing->second = value;
    }
    applyModeAndDerivedParameters();
}

bool OptiLabCoreEngine::setParameterValueRealtime(
    std::string_view key, double value) noexcept {
    if (!setParameterValueRealtimeDeferred(key, value)) return false;
    commitRealtimeParameterChanges();
    return true;
}

bool OptiLabCoreEngine::setParameterValueRealtimeDeferred(
    std::string_view key, double value) noexcept {
    if (!std::isfinite(value)
        || key == "p001"
        || key == "p085"
        || key == "p086"
        || key == "p087") {
        return false;
    }
    if (key == "p088") return false;

    const auto info = std::find_if(
        parameterInfo().begin(), parameterInfo().end(),
        [&](const ParameterInfo& item) { return key == item.key; });
    if (info == parameterInfo().end()) return false;

    // Realtime callers are forbidden to grow the map. Complete processor
    // preparation must enroll every realtime-updatable key before activation.
    const auto existing = params.overrides.find(key);
    if (existing == params.overrides.end()) return false;

    value = clamp(value, info->minimum, info->maximum);
    if (info->integer) value = std::round(value);
    if (key == "p002") {
        params.inputDriveDb = value;
    }
    if (existing->second == value) return true;
    existing->second = value;
    realtimeDerivedDirty_ = true;
    return true;
}

void OptiLabCoreEngine::commitRealtimeParameterChanges() noexcept {
    if (!realtimeDerivedDirty_) return;
    applyModeAndDerivedParameters();
}

void OptiLabCoreEngine::setSectionBypass(std::size_t section, bool bypassed) noexcept {
    if (section < params.sectionBypass.size()) {
        params.sectionBypass[section] = bypassed;
        refreshRoutingCache();
    }
}

void OptiLabCoreEngine::setOutputTap(OutputTap tap) noexcept {
    outputTap = tap;
    refreshRoutingCache();
}

void OptiLabCoreEngine::resetPhaseState() {
    for (auto& ap : apL) {
        ap.reset();
    }
    for (auto& ap : apR) {
        ap.reset();
    }
}

void OptiLabCoreEngine::reset() {
    std::fill(auxTapHistory.begin(), auxTapHistory.end(), std::pair<double, double>{});
    auxWrite = 0;
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
    adaptTopPresenceTargetCached = 1.0;
    adaptTopAirTargetCached = 1.0;
    adaptiveCpuCounter = 15;
    shapeRecoveryCounter = 0;
    shapeRecoveryTick = false;
    shapeBlockCounter = 0;
    shapeCompRecoveryDb.fill(0.0);
    shapeLimiterRecoveryDb.fill(0.0);
    compRecoveryGain.fill(1.0);
    limiterRecoveryGain.fill(1.0);
    shapeCompRatioBlock.fill(1.0);
    shapeLimiterRatioBlock.fill(1.0);
    shapeLevel.fill(0.0);
    shapeServoDb.fill(0.0);
    shapeServoGain.fill(1.0);
    shapePowerAccum.fill(0.0);
    currentAgcLowEffGain = 1.0;
    currentAgcHighEffGain = 1.0;
    currentDensityGain = 1.0;
    currentFinalGain = 1.0;
    finalThresholdDriveS = finalThresholdDriveTarget > 0.0 ? finalThresholdDriveTarget : 1.0;
    finalThresholdGuardGain = 1.0;
    preclip.gain = 1.0;
    hybridWrite = 0;
    hybridEnergyWrite = 0;
    hybridEnergyCount = 0;
    hybridSampleClock = 0;
    hybridCandidateActivePrev = false;
    hybridEventStartAbs = -1;
    hybridEventStartRing = 0;
    hybridEventCorrelation = 0.0;
    hybridOnset1 = hybridOnset2 = hybridOnset3 = -1000000000LL;
    hybridRefEnergySum = 0.0;
    hybridDeltaEnergySum = 0.0;
    hybridBurdenAccept = 1.0;
    hybridLimiterGain = 1.0;
    deliveryWrite = 0;
    deliveryGain = 1.0;
    deliverySustainedHistory.fill(1.0);
    deliverySustainedHistoryWrite = 0;
    deliverySustainedChunkCounter = deliverySustainedChunkSamples;
    deliverySustainedL = deliverySustainedR = 0.0;
    deliverySustainedPeak = 0.0;
    deliverySustainedTarget = 1.0;
    deliverySustainedSmoothed = 1.0;
    deliveryEnvelope.reset(); hybridEnvelope.reset();
    masterStartupArmed = 1;
    masterStartupAge = 0;
    masterCatchGain = 1.0;

    hp30L.reset(); hp30R.reset();
    for (auto& f : subsonicExtraL) f.reset();
    for (auto& f : subsonicExtraR) f.reset();
    resetPhaseState();
    agcLpL.reset(); agcLpR.reset();
    agcSteepLowAL.reset(); agcSteepLowAR.reset(); agcSteepLowBL.reset(); agcSteepLowBR.reset();
    agcSteepHighAL.reset(); agcSteepHighAR.reset(); agcSteepHighBL.reset(); agcSteepHighBR.reset();
    agcLowEnv.e = agcHighEnv.e = 0.0;
    postAgcRoundL.reset(); postAgcRoundR.reset();
    masterRawEnv.e = masterAmpEnv.e = masterCatchEnv.e = 0.0;
    adaptBassLpL.reset(); adaptBassLpR.reset();
    adaptBassEnv.e = adaptProgEnv.e = 0.0;
    bassPeakL.reset(); bassPeakR.reset();
    transitionPeakL.reset(); transitionPeakR.reset();
    lowFloorL.reset(); lowFloorR.reset();
    foundationGuard.reset();
    bassclipSplitL.reset(); bassclipSplitR.reset();
    bassclipHpfL.reset(); bassclipHpfR.reset();
    bassclipPreL.reset(); bassclipPreR.reset();
    for (auto& f : xbL) f.reset();
    for (auto& f : xbR) f.reset();
    for (auto& f : serialXbL) f.reset();
    for (auto& f : serialXbR) f.reset();
    densityLr4L.reset(); densityLr4R.reset();
    densityLr8L.reset(); densityLr8R.reset();
    adaptTopAirHpL.reset(); adaptTopAirHpR.reset();
    stereoEnhancer.reset();
    fmPreL.reset(); fmPreR.reset(); fmDeemphL.reset(); fmDeemphR.reset();
    adaptTopEdgeEnv.e = adaptTopEdgeProgEnv.e = adaptTopAirEnv.e = adaptTopAirProgEnv.e = 0.0;
    for (auto& l : lim) { l.env = 0.0; l.gain = 1.0; }
    for (auto& e : mbEvent) { e.env = 0.0; e.gain = 1.0; }
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
    lpf15_1L.reset(); lpf15_1R.reset();
    lpf15_2L.reset(); lpf15_2R.reset();
    fcsResL.reset(); fcsResR.reset();

    finalLookahead.reset();
    deliveryBufL.fill(0.0); deliveryBufR.fill(0.0);
    deliveryRefL.fill(0.0); deliveryRefR.fill(0.0);
    deliveryRequired.fill(1.0);
    deliveryControlGain.fill(1.0);
    deliveryHistL.fill(0.0); deliveryHistR.fill(0.0);
    std::fill(hybridRawL.begin(), hybridRawL.end(), 0.0);
    std::fill(hybridRawR.begin(), hybridRawR.end(), 0.0);
    std::fill(hybridCandidateL.begin(), hybridCandidateL.end(), 0.0);
    std::fill(hybridCandidateR.begin(), hybridCandidateR.end(), 0.0);
    std::fill(hybridShavedL.begin(), hybridShavedL.end(), 0.0);
    std::fill(hybridShavedR.begin(), hybridShavedR.end(), 0.0);
    std::fill(hybridControlGain.begin(), hybridControlGain.end(), 1.0);
    std::fill(hybridRequiredGain.begin(), hybridRequiredGain.end(), 1.0);
    std::fill(hybridBurden.begin(), hybridBurden.end(), 1.0);
    std::fill(hybridEventGate.begin(), hybridEventGate.end(), 0.0);
    std::fill(hybridWorkLimit.begin(), hybridWorkLimit.end(), ceiling);
    std::fill(hybridMakeup.begin(), hybridMakeup.end(), 1.0);
    std::fill(hybridFinalRefL.begin(), hybridFinalRefL.end(), 0.0);
    std::fill(hybridFinalRefR.begin(), hybridFinalRefR.end(), 0.0);
    std::fill(hybridRefEnergy.begin(), hybridRefEnergy.end(), 0.0);
    std::fill(hybridDeltaEnergy.begin(), hybridDeltaEnergy.end(), 0.0);
    for (auto& band : snubL) band.fill(0.0);
    for (auto& band : snubR) band.fill(0.0);
    densityRefBufL.fill(0.0); densityRefBufR.fill(0.0);
    masterBufL.fill(0.0); masterBufR.fill(0.0);
    snubWrite = masterWrite = 0;
    hybridWrite = 0;
}

void OptiLabCoreEngine::applyModeAndDerivedParameters() {
    using namespace optilab_parameters;
    const int coreMode = static_cast<int>(params.mode);
    // Apply the selected Core profile before any explicit runtime overrides.
#define OP_PARAMETER(type, member, initial, ...) \
    if constexpr (optilab_parameters::get(optilab_parameters::Index::member).hook != optilab_parameters::Hook::Keep) \
        member = static_cast<type>(optilab_parameters::get(optilab_parameters::Index::member).defaults.forMode(coreMode));
#include "OptiLabParameters.def"
#undef OP_PARAMETER
#define OP_INTERNAL(type, member, initial, key, integer, hook, defaults) \
    member = static_cast<type>(defaults.forMode(coreMode));
#include "OptiLabInternalState.def"
#undef OP_INTERNAL

    processorMode = (coreMode == 2 || coreMode == 6) ? 1 : 0;
    if (processorMode == 1) {
        c03 = 1.50;
        c25 = 1.50;
    }
    p002 = clamp(params.inputDriveDb, -18.0, 18.0);
    applyParameterOverrides();
    p044 = clamp(p044, 0.0, 100.0);
    inputGain = dbToLin(p002);
    agcDrive = dbToLin(p010);
    const double densityPosDb = std::max(p035, 0.0);
    const double densityNegDb = std::min(p035, 0.0);
    densityAudioGain = dbToLin(densityNegDb + densityPosDb * 0.35);
    densityDetectorGain = dbToLin(densityPosDb * 0.65);
    densityClipDrive = dbToLin(densityPosDb * 0.55);
    presenceGain = dbToLin(p042);
    brillianceGain = dbToLin(p043);
    clipDriveMb = dbToLin(p048 * 0.60);
    clipDriveFull = dbToLin(p048 * 0.25);
    densityProtectionDriveGain = dbToLin(std::max(p048, 0.0) * 0.75);
    ceiling = dbToLin(p079);
    useCoreDensity = true;
    mbShape = clamp(p036, 0.0, 100.0) * 0.01;
    shapeHighAmount = std::max(0.0, (mbShape - 0.35) / 0.65);
    shapeHighAmount *= shapeHighAmount;
    const double mbCompLimiterRaw = clamp(p039, 0.0, 100.0) * 0.01;
    mbCompLimiter = mbCompLimiterRaw * (0.85 + 0.15 * mbCompLimiterRaw);
    mbCompKeep = 1.0 - mbCompLimiter;
    p069 = clamp(p069, -24.0, 6.0);
    finalThresholdDriveTarget = dbToLin(-p069);
    postAgcSmoothDriveAmt = clamp(p021, 0.0, 25.0) * 0.01;
    postXt2SmoothDriveAmt = clamp(p052, 0.0, 50.0) * 0.01;
    postAgcSmoothRecoveryGain = dbToLin(2.0 * clamp(p021, 0.0, 25.0) / 25.0);
    postXt2SmoothRecoveryGain = dbToLin(2.0 * clamp(p052, 0.0, 50.0) / 50.0);
    densityOutputGain = dbToLin(clamp(p053, -12.0, 12.0));
    mbClipMix = p040 * 0.01;
    dcCancel = p049 * 0.01;
    dc3Amt = dcCancel * 0.35; dc4Amt = dcCancel * 0.55; dc5Amt = dcCancel * 0.78; dc6Amt = dcCancel * 0.92;
    overshootAmt = p065 * 0.01;
    recombControl = p041 * 0.01;
    c22 = static_cast<int>(std::floor(clamp(c22, 0.0, 4.0) + 0.5));
    const bool useCrossoverAwareShape = c31 != 0;
    if (useCrossoverAwareShape)
        shapeHighAmount *= densityShapeUpperScale[static_cast<std::size_t>(c22)];
    // Optional calibration keeps the broad band-energy relationship stable
    // when this alternate split is selected. The default path remains unity.
    crossoverShapeBandGain.fill(1.0);
    if (useCrossoverAwareShape) {
        const double crossoverShapeAmount = mbShape * mbShape * (3.0 - 2.0 * mbShape);
        const auto& recoveryDb = densityShapeRecoveryDb[static_cast<std::size_t>(c22)];
        for (std::size_t i = 0; i < crossoverShapeBandGain.size(); ++i)
            crossoverShapeBandGain[i] = dbToLin(recoveryDb[i] * crossoverShapeAmount);
    }
    c26 = static_cast<int>(std::floor(clamp(c26, 0.0, 2.0) + 0.5));
    pdRelease = p017 * 0.01;
    upperSnubber = p050 * 0.01;
    gateReopenStrength = p020 * 0.01;
    const double gateReopenSpeedSec = std::max(p019 * 0.001, 0.005);
    c03 = clamp(c03, 0.0, 1.50);
    c25 = clamp(c25, 0.0, 1.50);
    p006 = static_cast<int>(std::floor(clamp(p006, 0.0, 1.0) + 0.5));
    c29 = static_cast<int>(std::floor(clamp(c29, 0.0, 3.0) + 0.5));
    p080 = static_cast<int>(std::floor(clamp(p080, 0.0, 3.0) + 0.5));
    // Lookahead is a topology choice, not an amount-dependent choice. Keeping
    // the delay allocated at zero amount prevents automating the amount from
    // changing the latency reported to a plugin host.
    effectiveSnubberLookahead = c03 > 0.0
        ? static_cast<int>(std::floor(sampleRate * c03 * 0.001 + 0.5)) : 0;
    effectiveFinalLookahead = c25 > 0.0
        ? static_cast<int>(std::floor(sampleRate * c25 * 0.001 + 0.5)) : 0;
    assert(effectiveSnubberLookahead < snubBufferLength);
    assert(effectiveFinalLookahead < fcsBufferLength);
    effectiveSnubberLookahead = std::min(effectiveSnubberLookahead, snubBufferLength - 1);
    effectiveFinalLookahead = std::min(effectiveFinalLookahead, fcsBufferLength - 1);

    finalLookahead.configure(effectiveFinalLookahead);

    // This stage owns its delay. Do not reuse an upstream lookahead buffer here.
    // Lower-latency profiles reduce classification time instead of changing host latency.
    const double hybridProfileClassificationMs =
        p003 == 0 ? 0.0 : (p003 == 1 ? 1.50 : 4.00);
    const double effectiveHybridClassificationMs =
        std::min(c07, hybridProfileClassificationMs);
    const int hybridHistoryLength = static_cast<int>(hybridRawL.size());
    hybridEnvelope.prepare(hybridHistoryLength);
    deliveryEnvelope.prepare(deliveryBufferLength);
    hybridClassificationSamples = static_cast<int>(
        std::floor(sampleRate * effectiveHybridClassificationMs * 0.001 + 0.5));
    hybridLimiterLookaheadSamples = static_cast<int>(
        std::floor(sampleRate * c21 * 0.001 + 0.5));
    hybridTotalDelaySamples = hybridClassificationSamples + hybridLimiterLookaheadSamples;
    hybridBurdenWindowSamples = std::max(1, static_cast<int>(
        std::floor(sampleRate * c28 * 0.001 + 0.5)));
    hybridPeriodMinSamples = std::max(2, static_cast<int>(std::floor(sampleRate * 0.00025 + 0.5)));
    hybridPeriodMaxSamples = std::max(
        hybridPeriodMinSamples + 1,
        static_cast<int>(std::floor(sampleRate * 0.020 + 0.5)));
    assert(hybridClassificationSamples + hybridLimiterLookaheadSamples < hybridHistoryLength);
    assert(hybridBurdenWindowSamples < static_cast<int>(hybridRefEnergy.size()));
    assert(hybridPeriodMaxSamples * 3 < hybridHistoryLength);
    hybridLimiterLookaheadSamples = std::min(
        hybridLimiterLookaheadSamples, std::max(0, hybridHistoryLength - hybridClassificationSamples - 1));
    hybridTotalDelaySamples = hybridClassificationSamples + hybridLimiterLookaheadSamples;
    hybridBurdenWindowSamples = std::min(
        hybridBurdenWindowSamples, std::max(1, static_cast<int>(hybridRefEnergy.size()) - 1));
    hybridPeriodMaxSamples = std::min(
        hybridPeriodMaxSamples, std::max(hybridPeriodMinSamples + 1, (hybridHistoryLength - 1) / 3));
    hybridBurdenAttack = std::exp(-1.0 / std::max(1.0, sampleRate * 0.00025));
    hybridBurdenRelease = std::exp(
        -1.0 / std::max(1.0, sampleRate * c28 * 0.001));
    hybridLimiterRelease = std::exp(
        -1.0 / std::max(1.0, sampleRate * c16 * 0.001));
    std::fill(hybridAttackShape.begin(), hybridAttackShape.end(), 1.0);
    if (hybridLimiterLookaheadSamples > 0) {
        const double inverseLookahead = 1.0 / static_cast<double>(hybridLimiterLookaheadSamples);
        for (int distance = 0; distance <= hybridLimiterLookaheadSamples; ++distance) {
            const double u = static_cast<double>(distance) * inverseLookahead;
            hybridAttackShape[static_cast<std::size_t>(distance)] = u * u * (3.0 - 2.0 * u);
        }
    } else {
        hybridAttackShape[0] = 0.0;
    }

    // Fixed lookahead and reconstruction-aware detection protect the final ceiling.
    // The small internal margin calibrates the 8-phase detector to an honest
    // delivered target of approximately -0.1 dBTP without changing the
    // musical limiter envelope.
    deliveryTarget = ceiling * dbToLin(-0.25);
    deliveryLookaheadSamples = static_cast<int>(
        std::floor(sampleRate * 0.00150 + 0.5));
    assert(deliveryLookaheadSamples < deliveryBufferLength - 1);
    deliveryLookaheadSamples = std::min(deliveryLookaheadSamples, deliveryBufferLength - 2);
    deliveryAnticipationSamples = std::max(
        1, deliveryLookaheadSamples - deliveryDetectorDelaySamples);
    deliveryRelease = std::exp(-1.0 / std::max(1.0, sampleRate * 0.020));

    const int sustainedDiv1 = sampleRate > 130000.0 ? 32 : (sampleRate > 65000.0 ? 16 : 8);
    deliverySustainedChunkSamples = sustainedDiv1 * 8;
    deliverySustainedChunkCounter = deliverySustainedChunkSamples;
    deliverySustainedCoeff = std::min(1.0, 2.0 * pi * 500.0 / sampleRate);
    const int sustainedAttackDelay = std::max(1, static_cast<int>(
        std::ceil(0.00120 * sampleRate / sustainedDiv1)) * sustainedDiv1);
    deliverySustainedSmoothCoeff = std::min(1.0,
        (10.0 / static_cast<double>(sustainedAttackDelay)) / 8.0);

    // Independent 8-phase Blackman-windowed sinc interpolator. The kernel is
    // invariant across modes, sample rates and parameter changes, so build it
    // once and copy the cached table instead of repeating transcendental work.
    static const auto deliveryCoeffTemplate = [] {
        std::array<double, deliveryDetectorTaps * deliveryDetectorPhases> coeff{};
        for (int phase = 0; phase < deliveryDetectorPhases; ++phase) {
            const double frac = static_cast<double>(phase)
                / static_cast<double>(deliveryDetectorPhases);
            double sum = 0.0;
            for (int tap = 0; tap < deliveryDetectorTaps; ++tap) {
                const double n = static_cast<double>(tap - deliveryDetectorDelaySamples) + frac;
                const double sinc = std::abs(n) < 1.0e-12
                    ? 1.0 : std::sin(pi * n) / (pi * n);
                const double w = 0.42
                    - 0.5 * std::cos(2.0 * pi * static_cast<double>(tap)
                        / static_cast<double>(deliveryDetectorTaps - 1))
                    + 0.08 * std::cos(4.0 * pi * static_cast<double>(tap)
                        / static_cast<double>(deliveryDetectorTaps - 1));
                const double c = sinc * w;
                coeff[static_cast<std::size_t>(
                    tap * deliveryDetectorPhases + phase)] = c;
                sum += c;
            }
            if (std::abs(sum) > 1.0e-12) {
                for (int tap = 0; tap < deliveryDetectorTaps; ++tap) {
                    coeff[static_cast<std::size_t>(
                        tap * deliveryDetectorPhases + phase)] /= sum;
                }
            }
        }
        return coeff;
    }();
    deliveryCoeff = deliveryCoeffTemplate;

    masteringLookaheadSamples = processorMode == 1
        ? static_cast<int>(std::floor(sampleRate * 0.010 + 0.5)) : 0;
    assert(masteringLookaheadSamples < masterBufferLength);
    masteringLookaheadSamples = std::min(masteringLookaheadSamples, masterBufferLength - 1);
    masterStartupPrimeWindow = static_cast<int>(std::floor(sampleRate * 0.350 + 0.5));
    masterStartupActiveThresh = dbToLin(-70.0);

    phaseMode = static_cast<int>(std::floor(clamp(p004, 0.0, 6.0) + 0.5));
    if (phaseMode != phaseModeLast) {
        resetPhaseState();
        phaseModeLast = phaseMode;
    }
    phaseStages = phaseMode == 1 ? 2 : phaseMode == 2 ? 4 : phaseMode == 3 ? 6 : phaseMode == 4 ? 8 : phaseMode == 5 ? 10 : phaseMode == 6 ? 12 : 0;

    const std::array<double, 12> phaseFreqs{
        55, 85, 130, 200, 310, 480, 740, 1150, 1800, 2800, 4300, 6500};
    const double phaseFrequencyScale =
        c17 == 100.0 ? 1.0 : c17 * 0.01;
    for (std::size_t i = 0; i < apC.size(); ++i) {
        apC[i] = apCoeff(phaseFreqs[i] * phaseFrequencyScale);
    }

    agcMix = p009 * 0.01;
    agcDownMix = agcMix <= 0.50 ? agcMix : 0.50 + (agcMix - 0.50) * 0.40;
    xt2Mix = p037 * 0.01;
    adaptiveTopCoupling = p044 * 0.01;
    bassCoupling = p022 * 0.01;
    bassScRelief = p027 * 0.01;
    lowCoherence = p029 * 0.01;
    lowReleaseStab = p030 * 0.01;
    transitionFill = p031 * 0.01;
    lowBassFloor = p028 * 0.01;
    adaptiveBassCoupling = p032 * 0.01;
    useFoundationGuard = adaptiveBassCoupling > 0.0;
    bassClip = p033 * 0.01;
    bassClipDensity = p034 * 0.01;
    prelimitMix = p063 * 0.01;
    clipRestraint = p064 * 0.01;
    peakOnlyFinalLimiter = clipperStyleBase == 0 && !clipperMsMode && p063 >= 99 && p064 >= 99 &&
        p021 <= 0.001 && p052 <= 0.001 &&
        std::abs(p053) <= 0.001 &&
        p080 == 0 && p061 == 0 &&
        p069 >= -0.001 && p069 <= 0.001;
    prelimitThresh = peakOnlyFinalLimiter ? ceiling : ceiling * 0.985;
    fcsThreshSetting = peakOnlyFinalLimiter ? ceiling : ceiling * 0.985;

    agcEnvAttack = std::exp(-1.0 / (sampleRate * 0.045));
    agcGainAttack = std::exp(-1.0 / (sampleRate * 0.030));
    agcRelease = std::exp(-1.0 / (sampleRate * (0.080 + p016 * 0.085)));
    agcReleaseSlow = std::exp(-1.0 / (sampleRate * (0.240 + p016 * 0.150)));
    bandAttack = std::exp(-1.0 / (sampleRate * 0.0012));
    upperBandAttack = std::exp(-1.0 / (sampleRate * 0.0024));
    upperMidBandAttack = std::exp(-1.0 / (sampleRate * 0.0018));
    bandRelease = std::exp(-1.0 / (sampleRate * (0.035 + p016 * 0.035)));
    bandReleaseSlow = std::exp(-1.0 / (sampleRate * (0.100 + p016 * 0.080)));
    const double lowBandReleaseBase = std::exp(-1.0 / (sampleRate * (0.060 + p016 * 0.055)));
    const double lowBandReleaseSlow2 = std::exp(-1.0 / (sampleRate * (0.180 + p016 * 0.100)));
    lowBandRelease = bandRelease * (1.0 - lowReleaseStab) + lowBandReleaseBase * lowReleaseStab;
    lowBandReleaseSlow = bandReleaseSlow * (1.0 - lowReleaseStab) + lowBandReleaseSlow2 * lowReleaseStab;
    const double finalLimiterAttackSeconds =
        c02 == 0.8 ? 0.0008 : c02 * 0.001;
    const double finalLimiterReleaseSeconds =
        c30 == 45.0 ? 0.045 : c30 * 0.001;
    clipAttack = std::exp(-1.0 / (sampleRate * finalLimiterAttackSeconds));
    clipRelease = std::exp(-1.0 / (sampleRate * finalLimiterReleaseSeconds));
    finalThresholdSmooth = std::exp(-1.0 / (sampleRate * 0.010));
    recombAttack = std::exp(-1.0 / (sampleRate * 0.0007));
    recombRelease = std::exp(-1.0 / (sampleRate * (0.090 + p016 * 0.055)));
    startupActivityRelease = std::exp(-1.0 / (sampleRate * 0.350));
    masterCatchAttack = std::exp(-1.0 / (sampleRate * 0.010));
    masterCatchRelease = std::exp(-1.0 / (sampleRate * 0.150));
    masterEnvAttack = std::exp(-1.0 / (sampleRate * 0.006));
    masterEnvRelease = std::exp(-1.0 / (sampleRate * 0.320));
    gateDetectorRelease = std::exp(-1.0 / (sampleRate * 0.160));
    gateCloseCoeff = std::exp(-1.0 / (sampleRate * (0.040 + p016 * 0.012)));
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
    stereoEnhancer.configure(sampleRate, c05,
                             c14, c36,
                             c19,
                             static_cast<OptiLabStereoType>(p055), c27);
    mbEventAttack = std::exp(-1.0 / (sampleRate * 0.0040));
    mbEventMinimumRelease = std::exp(
        -1.0 / std::max(1.0, sampleRate * 0.004)
    );
    mbEventMaximumRelease = std::exp(
        -1.0 / std::max(1.0, sampleRate * 0.060)
    );
    const double shapeRecoveryAttack = 1.0 - std::exp(-1.0 / (sampleRate * 0.320));
    const double shapeRecoveryRelease = 1.0 - std::exp(-1.0 / (sampleRate * 0.070));
    shapeRecoveryAttackBlock = 1.0 - std::pow(1.0 - shapeRecoveryAttack, 16.0);
    shapeRecoveryReleaseBlock = 1.0 - std::pow(1.0 - shapeRecoveryRelease, 16.0);
    const double shapeServoLevel = 1.0 - std::exp(-1.0 / (sampleRate * 2.400));
    const double shapeServoGainCoefficient = 1.0 - std::exp(-1.0 / (sampleRate * 0.900));
    shapeServoLevelBlock = 1.0 - std::pow(1.0 - shapeServoLevel, 32.0);
    shapeServoGainBlock = 1.0 - std::pow(1.0 - shapeServoGainCoefficient, 32.0);
    const double densityCrossoverScale =
        c10 == 100.0 ? 1.0 : c10 * 0.01;
    const double densityCrossover1Hz = 150.0 * densityCrossoverScale;
    const double densityCrossover2Hz = 420.0 * densityCrossoverScale;
    const double densityCrossover3Hz = 700.0 * densityCrossoverScale;
    const double densityCrossover4Hz = 1600.0 * densityCrossoverScale;
    const double densityCrossover5Hz = 3700.0 * densityCrossoverScale;
    hpf30A = 1.0 - std::exp(-2.0 * pi * c04 / sampleRate);
    agcSplitA = 1.0 - std::exp(-2.0 * pi * c01 / sampleRate);
    x1A = 1.0 - std::exp(-2.0 * pi * densityCrossover1Hz / sampleRate);
    x2A = 1.0 - std::exp(-2.0 * pi * densityCrossover2Hz / sampleRate);
    x3A = 1.0 - std::exp(-2.0 * pi * densityCrossover3Hz / sampleRate);
    x4A = 1.0 - std::exp(-2.0 * pi * densityCrossover4Hz / sampleRate);
    x5A = 1.0 - std::exp(-2.0 * pi * densityCrossover5Hz / sampleRate);
    distCancelA = 1.0 - std::exp(-2.0 * pi * 2200.0 / sampleRate);
    agcTarget = dbToLin(c08);
    agcMaxGain = dbToLin(c33);
    agcMinGain = dbToLin(-c12);
    gateLin = dbToLin(p018);
    const std::array<double, 6> baseThresholdDb{-9.5, -11.5, -13.0, -14.5, -16.5, -18.0};
    const double limiterDepthUser = clamp(p038 * 0.01, 0.0, 1.0);
    const double limiterDepthNormal = 0.75 * std::sqrt(limiterDepthUser);
    const double limiterDepthDropDb = 8.0 * limiterDepthNormal * limiterDepthNormal;
    const double limiterFlatDb = -8.0 - 16.0 * limiterDepthNormal * limiterDepthNormal;
    std::array<double, 6> limiterNaturalDb{};
    for (std::size_t i = 0; i < 6; ++i) {
        limiterNaturalDb[i] = baseThresholdDb[i] - limiterDepthDropDb;
    }
    limiterWorkDb[0] = limiterNaturalDb[0] + (limiterFlatDb - limiterNaturalDb[0]) * mbShape;
    limiterWorkDb[1] = limiterNaturalDb[1] + (limiterFlatDb - limiterNaturalDb[1]) * mbShape;
    limiterWorkDb[2] = limiterNaturalDb[2] + (limiterFlatDb - limiterNaturalDb[2]) * mbShape;
    const double limiterControl4Db = limiterNaturalDb[3] +
        (limiterFlatDb - limiterNaturalDb[3]) * (shapeHighAmount * 0.34);
    const double limiterControl5Db = baseThresholdDb[4] - limiterDepthDropDb * 0.18;
    const double limiterControl6Db = baseThresholdDb[5] - limiterDepthDropDb * 0.18;
    limiterWorkDb[3] = limiterNaturalDb[3] + (limiterControl4Db - limiterNaturalDb[3]) * shapeHighAmount;
    limiterWorkDb[4] = limiterNaturalDb[4] + (limiterControl5Db - limiterNaturalDb[4]) * shapeHighAmount;
    limiterWorkDb[5] = limiterNaturalDb[5] + (limiterControl6Db - limiterNaturalDb[5]) * shapeHighAmount;
    for (std::size_t i = 0; i < 6; ++i) {
        limiterWork[i] = dbToLin(limiterWorkDb[i]);
        // Leave a wide numerical guard below the exact threshold so the fast
        // comparison cannot disagree with the logarithmic path at the boundary.
        limiterQuietThreshold[i] = limiterWork[i] * (1.0 - 1.0e-12);
    }
    mbPresencePost = dbToLin(p042 * (0.42 - 0.16 * shapeHighAmount));
    mbBrilliancePost = dbToLin(p043 * (0.38 - 0.14 * shapeHighAmount));
    mbLimiterForkCalibrationGain = dbToLin(-1.5 - 1.5 * mbShape);
    const double densityShapeAmount = mbShape * 0.55;
    std::array<double, 6> compressorThresholdDb{};
    compressorThresholdDb[0] = baseThresholdDb[0] + (-14.2 - baseThresholdDb[0]) * densityShapeAmount;
    compressorThresholdDb[1] = baseThresholdDb[1] + (-14.2 - baseThresholdDb[1]) * densityShapeAmount;
    compressorThresholdDb[2] = baseThresholdDb[2] + (-14.2 - baseThresholdDb[2]) * densityShapeAmount;
    compressorThresholdDb[3] = baseThresholdDb[3] + (-14.2 - baseThresholdDb[3]) * densityShapeAmount - 0.25 * shapeHighAmount;
    compressorThresholdDb[4] = baseThresholdDb[4] - 1.15 * shapeHighAmount;
    compressorThresholdDb[5] = baseThresholdDb[5] - 1.35 * shapeHighAmount;
    b1Thresh = dbToLin(compressorThresholdDb[0]); b2Thresh = dbToLin(compressorThresholdDb[1]);
    b3Thresh = dbToLin(compressorThresholdDb[2]); b4Thresh = dbToLin(compressorThresholdDb[3]);
    b5Thresh = dbToLin(compressorThresholdDb[4]); b6Thresh = dbToLin(compressorThresholdDb[5]);
    presenceEdgeRecoveryGain = dbToLin(p042 * 0.24 * shapeHighAmount);
    presenceBodyRecoveryGain = dbToLin(p042 * 0.42 * shapeHighAmount);
    brillianceRecoveryGain = dbToLin(p043 * 0.50 * shapeHighAmount);
    constDb2LinMinus62 = dbToLin(-6.2);
    constDb2LinMinus36 = dbToLin(-3.6);
    clipRef = ceiling * 0.985;
    const double mbRef = ceiling * 0.985;
    mbWorkRef = mbRef / std::max(clipDriveMb * densityClipDrive, 0.25);
    b1DetScGain = densityDetectorGain * dbToLin(-p023 * 0.55 * bassScRelief - 1.00 * lowBassFloor);
    b2DetScGain = densityDetectorGain * dbToLin(-p023 * 0.35 * bassScRelief - 0.45 * lowBassFloor);
    adaptBassSplitA = 1.0 - std::exp(-2.0 * pi * 190.0 / sampleRate);
    adaptBassDetAttack = std::exp(-1.0 / (sampleRate * 0.420));
    adaptBassDetRelease = std::exp(-1.0 / (sampleRate * 3.200));
    adaptBassGainUp = std::exp(-1.0 / (sampleRate * 1.150));
    adaptBassGainDown = std::exp(-1.0 / (sampleRate * 0.130));
    adaptBassTargetLow = 0.33; adaptBassTargetHigh = 0.52;
    adaptBassMaxBoostDb = 12.0 * adaptiveBassCoupling;
    adaptBassMaxCutDb = 7.0 * adaptiveBassCoupling;
    foundationGuard.detAttack = std::exp(-1.0 / (sampleRate * 0.550));
    foundationGuard.detRelease = std::exp(-1.0 / (sampleRate * 3.000));
    foundationGuard.slowAttack = std::exp(-1.0 / (sampleRate * 4.500));
    foundationGuard.slowRelease = std::exp(-1.0 / (sampleRate * 10.000));
    foundationGuard.fastBoostAttackTick = std::exp(-16.0 / (sampleRate * 1.200));
    foundationGuard.fastBoostReleaseTick = std::exp(-16.0 / (sampleRate * 6.000));
    foundationGuard.fastCutAttackTick = std::exp(-16.0 / (sampleRate * 0.200));
    foundationGuard.fastCutReleaseTick = std::exp(-16.0 / (sampleRate * 6.000));
    foundationGuard.boostAttackTick = std::exp(-16.0 / (sampleRate * 1.000));
    foundationGuard.boostReleaseTick = std::exp(-16.0 / (sampleRate * 10.000));
    foundationGuard.boostSuppressTick = std::exp(-16.0 / (sampleRate * 0.080));
    foundationGuard.cutAttackTick = std::exp(-16.0 / (sampleRate * 0.250));
    foundationGuard.cutReleaseTick = std::exp(-16.0 / (sampleRate * 10.000));
    foundationGuard.cutSuppressTick = std::exp(-16.0 / (sampleRate * 1.200));
    foundationGuard.cutHoldoffSamples = static_cast<std::size_t>(std::floor(sampleRate * 2.500 + 0.5));
    foundationGuard.voiceReturn = std::exp(-1.0 / (sampleRate * 2.200));
    foundationGuard.voiceWithdraw = std::exp(-1.0 / (sampleRate * 0.300));
    foundationGuard.feedbackAttackTick = std::exp(-16.0 / (sampleRate * 1.600));
    foundationGuard.feedbackReleaseTick = std::exp(-16.0 / (sampleRate * 4.000));
    foundationGuard.feedbackFastReleaseTick = std::exp(-16.0 / (sampleRate * 0.250));
    foundationGuard.feedbackBurdenAttackTick = std::exp(-16.0 / (sampleRate * 0.300));
    foundationGuard.feedbackBurdenReleaseTick = std::exp(-16.0 / (sampleRate * 1.200));
    foundationGuard.feedbackMaxAssistDb = 12.0;
    foundationGuard.boostScale = adaptiveBassBoostCurve(adaptiveBassCoupling);
    foundationGuard.cutAuthority = adaptiveBassCutCurve(adaptiveBassCoupling);
    foundationGuard.link = clamp(p029 * 0.01, 0.0, 1.0);
    foundationGuard.withdrawAuthority = foundationGuard.cutAuthority;
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
    if (clipperStyleBase == 0) { clipStage1 = clipRef * 0.96; clipKnee1 = 0.55; clipStage2 = clipRef * 0.998; clipKnee2 = 0.35; }
    else if (clipperStyleBase == 1) { clipStage1 = clipRef * 0.92; clipKnee1 = 0.46; clipStage2 = clipRef * 0.990; clipKnee2 = 0.30; }
    else if (clipperStyleBase == 2) { clipStage1 = clipRef * 0.88; clipKnee1 = 0.38; clipStage2 = clipRef * 0.975; clipKnee2 = 0.24; }
    else { clipStage1 = clipRef * 0.80; clipKnee1 = 0.26; clipStage2 = clipRef * 0.950; clipKnee2 = 0.18; }
    sideScale = p054 * 0.01;
    p061 = static_cast<int>(std::floor(clamp(p061, 0.0, 4.0) + 0.5));
    const double fmTauSeconds = (p061 == 1 || p061 == 3)
        ? 0.000075
        : (p061 == 2 || p061 == 4 ? 0.000050 : 0.0);
    fmPreemphasisK = fmTauSeconds > 0.0 ? sampleRate * fmTauSeconds : 0.0;
    fmDeemphasisAfter = p061 == 1 || p061 == 2;
    bassPeakL.setPeak(c15, c35, p023, sampleRate);
    bassPeakR.setPeak(c15, c35, p023, sampleRate);
    foundationGuard.lp70L.setLowpass(70.0, 0.70710678, sampleRate);
    foundationGuard.lp70R.setLowpass(70.0, 0.70710678, sampleRate);
    foundationGuard.lp250L.setLowpass(250.0, 0.70710678, sampleRate);
    foundationGuard.lp250R.setLowpass(250.0, 0.70710678, sampleRate);
    foundationGuard.lp420L.setLowpass(420.0, 0.70710678, sampleRate);
    foundationGuard.lp420R.setLowpass(420.0, 0.70710678, sampleRate);
    foundationGuard.lp3700L.setLowpass(3700.0, 0.70710678, sampleRate);
    foundationGuard.lp3700R.setLowpass(3700.0, 0.70710678, sampleRate);
    foundationGuard.shelfL.setLowShelf(70.0, 1.0, 0.0, sampleRate);
    foundationGuard.shelfR.setLowShelf(70.0, 1.0, 0.0, sampleRate);
    foundationGuard.bodyL.setPeak(90.0, 1.10, 0.0, sampleRate);
    foundationGuard.bodyR.setPeak(90.0, 1.10, 0.0, sampleRate);
    transitionPeakL.setPeak(105.0, 1.0, transitionFill * 1.6, sampleRate); transitionPeakR.setPeak(105.0, 1.0, transitionFill * 1.6, sampleRate);
    lowFloorL.setPeak(58.0, 0.75, lowBassFloor * 0.65, sampleRate); lowFloorR.setPeak(58.0, 0.75, lowBassFloor * 0.65, sampleRate);
    if (p080 == 1) {
        lpf15_1L.setLowpass(17000.0, 0.55, sampleRate); lpf15_1R.setLowpass(17000.0, 0.55, sampleRate);
        lpf15_2L.setLowpass(21000.0, 0.50, sampleRate); lpf15_2R.setLowpass(21000.0, 0.50, sampleRate);
    } else if (p080 == 2) {
        lpf15_1L.setLowpass(15500.0, 0.58, sampleRate); lpf15_1R.setLowpass(15500.0, 0.58, sampleRate);
        lpf15_2L.setLowpass(15500.0, 0.58, sampleRate); lpf15_2R.setLowpass(15500.0, 0.58, sampleRate);
    } else {
        lpf15_1L.setLowpass(14500.0, 0.50, sampleRate); lpf15_1R.setLowpass(14500.0, 0.50, sampleRate);
        lpf15_2L.setLowpass(14500.0, 0.50, sampleRate); lpf15_2R.setLowpass(14500.0, 0.50, sampleRate);
    }
    constexpr double lr4Q = 0.70710678118654752440;
    agcSteepLowAL.setLowpass(c01, lr4Q, sampleRate);
    agcSteepLowAR.setLowpass(c01, lr4Q, sampleRate);
    agcSteepLowBL.setLowpass(c01, lr4Q, sampleRate);
    agcSteepLowBR.setLowpass(c01, lr4Q, sampleRate);
    agcSteepHighAL.setHighpass(c01, lr4Q, sampleRate);
    agcSteepHighAR.setHighpass(c01, lr4Q, sampleRate);
    agcSteepHighBL.setHighpass(c01, lr4Q, sampleRate);
    agcSteepHighBR.setHighpass(c01, lr4Q, sampleRate);
    const std::array<double, 5> densityCrossoverHz {
        densityCrossover1Hz, densityCrossover2Hz, densityCrossover3Hz,
        densityCrossover4Hz, densityCrossover5Hz
    };
    densityLr4L.configure(densityCrossoverHz, sampleRate);
    densityLr4R.configure(densityCrossoverHz, sampleRate);
    densityLr8L.configure(densityCrossoverHz, sampleRate);
    densityLr8R.configure(densityCrossoverHz, sampleRate);
    fcsResidueA = 1.0 - std::exp(-2.0 * pi * 12000.0 / sampleRate);
    refreshRoutingCache();
    realtimeDerivedDirty_ = false;
}

template <bool Analyze, bool AuxEnabled>
std::pair<float, float> OptiLabCoreEngine::processSampleImpl(
    float left, float right, OptiLabAnalysisFrame* analysis, AuxFrame* aux) {
    const auto observe = [this, aux](int boundary, double l, double r) noexcept {
        if constexpr (AuxEnabled) observeAux(boundary, l, r, aux);
        else { (void)boundary; (void)l; (void)r; }
    };
    double l = std::isfinite(left) ? static_cast<double>(left) : 0.0;
    double r = std::isfinite(right) ? static_cast<double>(right) : 0.0;
    l = std::abs(l) < 1000000000000.0 ? l : 0.0;
    r = std::abs(r) < 1000000000000.0 ? r : 0.0;
    observe(0, l, r);
    const double inputRawL = l;
    const double inputRawR = r;
    if constexpr (Analyze) {
        analysis->input = {
            static_cast<float>(inputRawL), static_cast<float>(inputRawR)};
    }
    // Full bypass is a zero-latency wire, including the shared mastering stage.
    if (c20 == 0 && outputTap == OutputTap::FullChain
        && allSectionsBypassedCache) {
        for (int boundary = 1; boundary <= 6; ++boundary) {
            if constexpr (Analyze) {
                auto& section = analysis->sections[static_cast<std::size_t>(boundary - 1)];
                section.input = {static_cast<float>(l), static_cast<float>(r)};
                section.output = section.input;
            }
            observe(boundary, l, r);
        }
        if constexpr (Analyze) {
            analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        }
        observe(6, static_cast<float>(l), static_cast<float>(r));
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    ++shapeRecoveryCounter;
    shapeRecoveryTick = shapeRecoveryCounter >= 16;
    if (shapeRecoveryTick) {
        shapeRecoveryCounter = 0;
    }
    ++adaptiveCpuCounter;
    const bool adaptiveCpuTick = adaptiveCpuCounter >= 16;
    if (adaptiveCpuTick) {
        adaptiveCpuCounter = 0;
    }

    if (shouldRunSection(0)) {
        l *= inputGain;
        r *= inputGain;
        if (p006) {
            // The first pole preserves the baseline response. Optional
            // slopes only cascade additional poles after that exact operation.
            l = hp30L.hp(l, hpf30A);
            r = hp30R.hp(r, hpf30A);
            for (int stage = 0; stage < c29; ++stage) {
                l = subsonicExtraL[static_cast<std::size_t>(stage)].hp(l, hpf30A);
                r = subsonicExtraR[static_cast<std::size_t>(stage)].hp(r, hpf30A);
            }
        }
        if (phaseStages > 0) {
            for (int i = 0; i < phaseStages; ++i) {
                l = apL[static_cast<std::size_t>(i)].run(l, apC[static_cast<std::size_t>(i)]);
                r = apR[static_cast<std::size_t>(i)].run(r, apC[static_cast<std::size_t>(i)]);
            }
        }
        const double inputProcessedL = l;
        const double inputProcessedR = r;
        if (outputTap == OutputTap::InputTestpoint || outputTap == OutputTap::InputDelta) {
            observe(1, l, r);
        }
        if (outputTap == OutputTap::InputTestpoint)
            return {static_cast<float>(inputProcessedL), static_cast<float>(inputProcessedR)};
        if (outputTap == OutputTap::InputDelta)
            return {static_cast<float>(inputProcessedL - inputRawL), static_cast<float>(inputProcessedR - inputRawR)};
    }
    if constexpr (Analyze) {
        analysis->sections[0].input = {
            static_cast<float>(inputRawL), static_cast<float>(inputRawR)};
        analysis->sections[0].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }

    observe(1, l, r);
    if (stopAtBoundary(1)) {
        if constexpr (Analyze) analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    const double agcPreL = l;
    const double agcPreR = r;
    // A bypassed AGC must not leave stale gate modulation in Density.
    double gateEffective = 0.0;
    double gateReopenRelMix = 0.0;
    if (shouldRunSection(1)) {
        startupActivity = std::max(absmax2(l, r), startupActivity * startupActivityRelease);
        const double agcL = l * agcDrive;
        const double agcR = r * agcDrive;
        double lowL = 0.0;
        double lowR = 0.0;
        double highL = 0.0;
        double highR = 0.0;
        if (c26 == 0) {
            // One-pole residual split.
            lowL = agcLpL.lp(agcL, agcSplitA);
            lowR = agcLpR.lp(agcR, agcSplitA);
            highL = agcL - lowL;
            highR = agcR - lowR;
        } else if (c26 == 1) {
            lowL = agcLpL.lp2(agcL, agcSplitA);
            lowR = agcLpR.lp2(agcR, agcSplitA);
            highL = agcL - lowL;
            highR = agcR - lowR;
        } else {
            // Optional steep split: true complementary LR4 low/high branches.
            // Do not derive the high band by subtracting a phase-rotated lowpass.
            lowL = agcSteepLowBL.run(agcSteepLowAL.run(agcL));
            lowR = agcSteepLowBR.run(agcSteepLowAR.run(agcR));
            highL = agcSteepHighBL.run(agcSteepHighAL.run(agcL));
            highR = agcSteepHighBR.run(agcSteepHighAR.run(agcR));
        }
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
        gateEffective = gateState * (1.0 - gateOpenNow);
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
        gateReopenRelMix = gateReopenEnv * (0.35 + 0.45 * gateReopenStrength);
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
        if constexpr (Analyze) {
            analysis->sections[1].detectorGain =
                std::min(agcLowEffGain, agcHighEffGain);
        }
        if (activityTracking) {
            currentAgcLowEffGain = agcLowEffGain;
            currentAgcHighEffGain = agcHighEffGain;
        }
        l = (lowL / agcDriveSafe) * agcLowEffGain +
            (highL / agcDriveSafe) * agcHighEffGain;
        r = (lowR / agcDriveSafe) * agcLowEffGain +
            (highR / agcDriveSafe) * agcHighEffGain;
        if (postAgcSmoothDriveAmt > 0.0000001) {
            l = postAgcRoundL.run(l, postAgcSmoothDriveAmt) * postAgcSmoothRecoveryGain;
            r = postAgcRoundR.run(r, postAgcSmoothDriveAmt) * postAgcSmoothRecoveryGain;
        }
        const double agcProcessedL = l;
        const double agcProcessedR = r;
        if (outputTap == OutputTap::AgcTestpoint || outputTap == OutputTap::AgcDelta) {
            observe(2, l, r);
            if (aux != nullptr && aux->captured) aux->capturedLatency = 0;
        }
        if (outputTap == OutputTap::AgcTestpoint)
            return {static_cast<float>(agcProcessedL), static_cast<float>(agcProcessedR)};
        if (outputTap == OutputTap::AgcDelta)
            return {static_cast<float>(agcProcessedL - agcPreL), static_cast<float>(agcProcessedR - agcPreR)};
    }
    if constexpr (Analyze) {
        analysis->sections[1].input = {
            static_cast<float>(agcPreL), static_cast<float>(agcPreR)};
        analysis->sections[1].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }

    // The mastering catch handles gain added by the input/AGC path. It is
    // part of AGC processing, so AGC bypass removes both its gain and delay.
    if (segmentOwnsSection(1) && !params.sectionBypass[1] && masteringLookaheadSamples > 0) {
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
    } else if (segmentOwnsSection(1) && !params.sectionBypass[1]) {
        masterCatchGain = 1.0;
        masterStartupArmed = 1;
        masterStartupAge = 0;
    }

    observe(2, l, r);
    if (stopAtBoundary(2)) {
        if constexpr (Analyze) analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    const double bassPreL = l;
    const double bassPreR = r;
    if (shouldRunSection(2)) {
        const double bassBoostedL = bassPeakL.run(l);
        const double bassBoostedR = bassPeakR.run(r);
        l = bassBoostedL;
        r = bassBoostedR;
        if (useFoundationGuard) {
            const auto foundation = processFoundationGuard(
                bassPreL, bassPreR, bassBoostedL, bassBoostedR
            );
            l = foundation.first;
            r = foundation.second;
        } else if (adaptiveBassCoupling > 0.0) {
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
        const double bassProcessedL = l;
        const double bassProcessedR = r;
        if (outputTap == OutputTap::BassTestpoint || outputTap == OutputTap::BassDelta) {
            observe(3, l, r);
        }
        if (outputTap == OutputTap::BassTestpoint)
            return {static_cast<float>(bassProcessedL), static_cast<float>(bassProcessedR)};
        if (outputTap == OutputTap::BassDelta)
            return {static_cast<float>(bassProcessedL - bassPreL), static_cast<float>(bassProcessedR - bassPreR)};
    }
    if constexpr (Analyze) {
        analysis->sections[2].input = {
            static_cast<float>(bassPreL), static_cast<float>(bassPreR)};
        analysis->sections[2].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }

    observe(3, l, r);
    if (stopAtBoundary(3)) {
        if constexpr (Analyze) analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    const double densityPreL = l;
    const double densityPreR = r;
    if (shouldRunSection(3)) {
        l *= densityAudioGain;
        r *= densityAudioGain;
        double b1L, b2L, b3L, b4L, b5L, b6L;
        double b1R, b2R, b3R, b4R, b5R, b6R;
        if (c22 == 0 || c22 == 1) {
            const std::array<double, 5> coefficients{x1A, x2A, x3A, x4A, x5A};
            std::array<double, 6> bandsL{}, bandsR{};
            optilab_crossover::splitSerialResidual(l, serialXbL, coefficients, c22 == 1, bandsL);
            optilab_crossover::splitSerialResidual(r, serialXbR, coefficients, c22 == 1, bandsR);
            b1L = bandsL[0]; b2L = bandsL[1]; b3L = bandsL[2]; b4L = bandsL[3]; b5L = bandsL[4]; b6L = bandsL[5];
            b1R = bandsR[0]; b2R = bandsR[1]; b3R = bandsR[2]; b4R = bandsR[3]; b5R = bandsR[4]; b6R = bandsR[5];
        } else if (c22 == 3) {
            std::array<double, 6> bandsL{}, bandsR{};
            densityLr4L.split(l, bandsL);
            densityLr4R.split(r, bandsR);
            b1L = bandsL[0]; b2L = bandsL[1]; b3L = bandsL[2]; b4L = bandsL[3]; b5L = bandsL[4]; b6L = bandsL[5];
            b1R = bandsR[0]; b2R = bandsR[1]; b3R = bandsR[2]; b4R = bandsR[3]; b5R = bandsR[4]; b6R = bandsR[5];
        } else if (c22 == 4) {
            std::array<double, 6> bandsL{}, bandsR{};
            densityLr8L.split(l, bandsL);
            densityLr8R.split(r, bandsR);
            b1L = bandsL[0]; b2L = bandsL[1]; b3L = bandsL[2]; b4L = bandsL[3]; b5L = bandsL[4]; b6L = bandsL[5];
            b1R = bandsR[0]; b2R = bandsR[1]; b3R = bandsR[2]; b4R = bandsR[3]; b5R = bandsR[4]; b6R = bandsR[5];
        } else {
            // Both the compressor and limiter work
            // from this same bank, so preserve the exact operation order.
            const double lp1L = xbL[0].lp2(l, x1A); const double lp2L = xbL[1].lp2(l, x2A); const double lp3L = xbL[2].lp2(l, x3A); const double lp4L = xbL[3].lp2(l, x4A); const double lp5L = xbL[4].lp2(l, x5A);
            b1L = lp1L; b2L = lp2L - lp1L; b3L = lp3L - lp2L; b4L = lp4L - lp3L; b5L = lp5L - lp4L; b6L = l - lp5L;
            const double lp1R = xbR[0].lp2(r, x1A); const double lp2R = xbR[1].lp2(r, x2A); const double lp3R = xbR[2].lp2(r, x3A); const double lp4R = xbR[3].lp2(r, x4A); const double lp5R = xbR[4].lp2(r, x5A);
            b1R = lp1R; b2R = lp2R - lp1R; b3R = lp3R - lp2R; b4R = lp4R - lp3R; b5R = lp5R - lp4R; b6R = r - lp5R;
        }

        // Crossover-aware Shape acts before the Density detectors/dynamics so
        // it changes what the six-band processor sees rather than patching the
        // result with an output EQ. At Shape 0 these gains are exactly unity;
        // Classic Parallel is unity at every Shape value.
        if (c31 != 0 && c22 != 2 && mbShape > 0.0) {
            b1L *= crossoverShapeBandGain[0]; b1R *= crossoverShapeBandGain[0];
            b2L *= crossoverShapeBandGain[1]; b2R *= crossoverShapeBandGain[1];
            b3L *= crossoverShapeBandGain[2]; b3R *= crossoverShapeBandGain[2];
            b4L *= crossoverShapeBandGain[3]; b4R *= crossoverShapeBandGain[3];
            b5L *= crossoverShapeBandGain[4]; b5R *= crossoverShapeBandGain[4];
            b6L *= crossoverShapeBandGain[5]; b6R *= crossoverShapeBandGain[5];
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
            if (adaptiveCpuTick) {
                adaptTopPresenceTargetCached = dbToLin(presenceTargetDb);
                adaptTopAirTargetCached = dbToLin(airTargetDb);
            }
            const double presenceTarget = adaptTopPresenceTargetCached;
            const double airTarget = adaptTopAirTargetCached;
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

        if (c06 != 0 && useFoundationGuard) {
            foundationGuard.feedbackB1Accum += (b1L * b1L + b1R * b1R) * 0.5;
            foundationGuard.feedbackB2Accum += (b2L * b2L + b2R * b2R) * 0.5;
            ++foundationGuard.feedbackCount;
        }

        const double gateXt2Strength = gateEffective * 0.78;
        double lowBandReleaseGated = lowBandRelease * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
        double lowBandReleaseSlowGated = lowBandReleaseSlow * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
        double bandReleaseGated = bandRelease * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
        double bandReleaseSlowGated = bandReleaseSlow * (1.0 - gateXt2Strength) + gateXt2FreezeRelease * gateXt2Strength;
        lowBandReleaseGated = lowBandReleaseGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
        lowBandReleaseSlowGated = lowBandReleaseSlowGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
        bandReleaseGated = bandReleaseGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;
        bandReleaseSlowGated = bandReleaseSlowGated * (1.0 - gateReopenRelMix) + gateReopenRelease * gateReopenRelMix;

        std::array<double, 6> sourceL{b1L, b2L, b3L, b4L, b5L, b6L};
        std::array<double, 6> sourceR{b1R, b2R, b3R, b4R, b5R, b6R};
        std::array<double, 6> compGain{};
        compGain[0] = lim[0].bandLimitPd(absmax2(sourceL[0], sourceR[0]) * b1DetScGain, b1Thresh, bandAttack, lowBandReleaseGated, lowBandReleaseSlowGated, pdRelease);
        compGain[1] = lim[1].bandLimitPd(absmax2(sourceL[1], sourceR[1]) * b2DetScGain, b2Thresh, bandAttack, lowBandReleaseGated, lowBandReleaseSlowGated, pdRelease);
        compGain[2] = lim[2].bandLimitPdSoft(absmax2(sourceL[2], sourceR[2]) * densityDetectorGain, b3Thresh, upperMidBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 4.2, 0.30, 0.14);
        compGain[3] = lim[3].bandLimitPdSoft(absmax2(sourceL[3], sourceR[3]) * densityDetectorGain, b4Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 3.1, 0.42, 0.16);
        compGain[4] = lim[4].bandLimitPdSoft(absmax2(sourceL[4], sourceR[4]) * densityDetectorGain, b5Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 2.6, 0.55, 0.18);
        compGain[5] = lim[5].bandLimitPdSoft(absmax2(sourceL[5], sourceR[5]) * densityDetectorGain, b6Thresh, upperBandAttack, bandReleaseGated, bandReleaseSlowGated, pdRelease, 2.4, 0.65, 0.20);
        for (double& gain : compGain) {
            gain = 1.0 - xt2Mix * (1.0 - gain);
        }
        const double compLowSharedGain = std::sqrt(std::max(compGain[0] * compGain[1], tiny));
        compGain[0] = compGain[0] * (1.0 - lowCoherence) + compLowSharedGain * lowCoherence;
        compGain[1] = compGain[1] * (1.0 - lowCoherence) + compLowSharedGain * lowCoherence;
        // Keep the air band linked to Band 5 for stable upper-frequency movement,
        // but do not discard Band 6's own detector.  A bright source can carry
        // sustained energy almost entirely above the final crossover; copying the
        // Band 5 gain here allowed that energy through the compressor side of the
        // compressor-to-limiter blend without meaningful control.
        compGain[5] = std::min(compGain[4], compGain[5]);

        std::array<double, 6> compBandL = sourceL;
        std::array<double, 6> compBandR = sourceR;
        if (mbClipMix > 0.0) {
            compBandL[2] = mbClipL[0].run(compBandL[2], mbWorkRef * 0.56, mbClipMix * 0.55, 0.42, dc3Amt, distCancelA);
            compBandR[2] = mbClipR[0].run(compBandR[2], mbWorkRef * 0.56, mbClipMix * 0.55, 0.42, dc3Amt, distCancelA);
            compBandL[3] = mbClipL[1].run(compBandL[3], mbWorkRef * 0.48, mbClipMix * 0.75, 0.36, dc4Amt, distCancelA);
            compBandR[3] = mbClipR[1].run(compBandR[3], mbWorkRef * 0.48, mbClipMix * 0.75, 0.36, dc4Amt, distCancelA);
            compBandL[4] = mbClipL[2].run(compBandL[4], mbWorkRef * 0.40, mbClipMix, 0.30, dc5Amt, distCancelA);
            compBandR[4] = mbClipR[2].run(compBandR[4], mbWorkRef * 0.40, mbClipMix, 0.30, dc5Amt, distCancelA);
            compBandL[5] = mbClipL[3].run(compBandL[5], mbWorkRef * 0.32, mbClipMix, 0.24, dc6Amt, distCancelA);
            compBandR[5] = mbClipR[3].run(compBandR[5], mbWorkRef * 0.32, mbClipMix, 0.24, dc6Amt, distCancelA);
        }

        std::array<double, 6> compProcessedL{};
        std::array<double, 6> compProcessedR{};
        for (std::size_t i = 0; i < 6; ++i) {
            compProcessedL[i] = compBandL[i] * compGain[i];
            compProcessedR[i] = compBandR[i] * compGain[i];
        }

        std::array<double, 6> pL{};
        std::array<double, 6> pR{};
        std::array<double, 6> limiterGain{};
        if (useCoreDensity) {
            const std::array<double, 3> sourcePeak{
                absmax2(sourceL[0], sourceR[0]) + 0.0000001,
                absmax2(sourceL[1], sourceR[1]) + 0.0000001,
                absmax2(sourceL[2], sourceR[2]) + 0.0000001
            };
            for (std::size_t i = 0; i < 3; ++i) {
                const double postPeak = absmax2(compProcessedL[i], compProcessedR[i]) + 0.0000001;
                shapeCompRatioBlock[i] = std::max(shapeCompRatioBlock[i], sourcePeak[i] / postPeak);
            }
            if (shapeRecoveryTick) {
                const std::array<double, 3> allowance{0.18, 0.18, 0.25};
                const std::array<double, 3> scale{0.55, 0.62, 0.18};
                const std::array<double, 3> cap{2.30, 2.45, 0.65};
                for (std::size_t i = 0; i < 3; ++i) {
                    const double lossDb = std::max(0.0, linToDb(shapeCompRatioBlock[i]) - allowance[i]);
                    const double targetDb = std::min(lossDb * scale[i], cap[i]) * mbShape;
                    const double coefficient = targetDb < shapeCompRecoveryDb[i]
                        ? shapeRecoveryReleaseBlock : shapeRecoveryAttackBlock;
                    shapeCompRecoveryDb[i] += coefficient * (targetDb - shapeCompRecoveryDb[i]);
                    compRecoveryGain[i] = dbToLin(shapeCompRecoveryDb[i]);
                    shapeCompRatioBlock[i] = 1.0;
                }
            }
            for (std::size_t i = 0; i < 3; ++i) {
                compProcessedL[i] *= compRecoveryGain[i];
                compProcessedR[i] *= compRecoveryGain[i];
            }

            std::array<double, 6> limiterProcessedL{};
            std::array<double, 6> limiterProcessedR{};
            for (std::size_t i = 0; i < 6; ++i) {
                limiterGain[i] = mbEvent[i].run(
                    absmax2(sourceL[i], sourceR[i]),
                    limiterWorkDb[i],
                    limiterQuietThreshold[i],
                    mbEventAttack,
                    sampleRate,
                    mbEventMinimumRelease,
                    mbEventMaximumRelease
                );
                limiterProcessedL[i] = sourceL[i] * limiterGain[i];
                limiterProcessedR[i] = sourceR[i] * limiterGain[i];
            }
            limiterProcessedL[3] *= mbPresencePost; limiterProcessedR[3] *= mbPresencePost;
            limiterProcessedL[4] *= mbPresencePost; limiterProcessedR[4] *= mbPresencePost;
            limiterProcessedL[5] *= mbBrilliancePost; limiterProcessedR[5] *= mbBrilliancePost;
            for (std::size_t i = 0; i < 6; ++i) {
                limiterProcessedL[i] = limiterBandClip(limiterProcessedL[i], limiterWork[i], 0.35, 0.75);
                limiterProcessedR[i] = limiterBandClip(limiterProcessedR[i], limiterWork[i], 0.35, 0.75);
            }

            for (std::size_t i = 0; i < 3; ++i) {
                const double postPeak = absmax2(limiterProcessedL[i], limiterProcessedR[i]) + 0.0000001;
                shapeLimiterRatioBlock[i] = std::max(shapeLimiterRatioBlock[i], sourcePeak[i] / postPeak);
            }
            if (shapeRecoveryTick) {
                const std::array<double, 3> allowance{0.18, 0.18, 0.25};
                const std::array<double, 3> scale{0.62, 0.70, 0.20};
                const std::array<double, 3> cap{2.60, 2.75, 0.75};
                for (std::size_t i = 0; i < 3; ++i) {
                    const double lossDb = std::max(0.0, linToDb(shapeLimiterRatioBlock[i]) - allowance[i]);
                    const double targetDb = std::min(lossDb * scale[i], cap[i]) * mbShape;
                    const double coefficient = targetDb < shapeLimiterRecoveryDb[i]
                        ? shapeRecoveryReleaseBlock : shapeRecoveryAttackBlock;
                    shapeLimiterRecoveryDb[i] += coefficient * (targetDb - shapeLimiterRecoveryDb[i]);
                    limiterRecoveryGain[i] = dbToLin(shapeLimiterRecoveryDb[i]);
                    shapeLimiterRatioBlock[i] = 1.0;
                }
            }
            for (std::size_t i = 0; i < 3; ++i) {
                limiterProcessedL[i] *= limiterRecoveryGain[i];
                limiterProcessedR[i] *= limiterRecoveryGain[i];
            }

            for (std::size_t i = 0; i < 6; ++i) {
                limiterProcessedL[i] *= mbLimiterForkCalibrationGain;
                limiterProcessedR[i] *= mbLimiterForkCalibrationGain;
                pL[i] = compProcessedL[i] * mbCompKeep + limiterProcessedL[i] * mbCompLimiter;
                pR[i] = compProcessedR[i] * mbCompKeep + limiterProcessedR[i] * mbCompLimiter;
            }

            for (std::size_t i = 0; i < 6; ++i) {
                shapePowerAccum[i] += (pL[i] * pL[i] + pR[i] * pR[i]) * 0.5;
            }
            ++shapeBlockCounter;
            if (shapeBlockCounter >= 32) {
                shapeBlockCounter = 0;
                for (std::size_t i = 0; i < 6; ++i) {
                    shapeLevel[i] += shapeServoLevelBlock * (shapePowerAccum[i] * 0.03125 - shapeLevel[i]);
                    shapePowerAccum[i] = 0.0;
                }
                const double referenceLevel = std::sqrt(std::max((shapeLevel[2] + shapeLevel[3]) * 0.5, 0.000000000001));
                std::array<double, 6> ratioDb{};
                for (std::size_t i = 0; i < 6; ++i) {
                    ratioDb[i] = linToDb(std::sqrt(std::max(shapeLevel[i], 0.000000000001)) / referenceLevel);
                }
                const double presenceBiasDb = clamp(p042, -6.0, 6.0);
                const double brillianceBiasDb = clamp(p043, -6.0, 6.0);
                const std::array<double, 6> targetProfileDb{
                    -1.8, -0.7, -0.3, 0.0,
                    -2.2 + presenceBiasDb * 0.24,
                    -4.6 + brillianceBiasDb * 0.26
                };
                std::array<double, 6> servoTargetDb{};
                servoTargetDb[0] = (clamp((targetProfileDb[0] - ratioDb[0]) * 0.34, -1.75, 2.40) + 2.5) * mbShape;
                servoTargetDb[1] = (clamp((targetProfileDb[1] - ratioDb[1]) * 0.36, -1.65, 2.55) + 0.9) * mbShape;
                servoTargetDb[2] = clamp((targetProfileDb[2] - ratioDb[2]) * 0.16, -0.60, 0.55) * mbShape;
                servoTargetDb[3] = clamp((targetProfileDb[3] - ratioDb[3]) * 0.10, -0.40, 0.40) * mbShape;
                servoTargetDb[4] = clamp((targetProfileDb[4] - ratioDb[4]) * 0.18, -1.10, 1.15) * shapeHighAmount;
                servoTargetDb[5] = clamp((targetProfileDb[5] - ratioDb[5]) * 0.18, -1.35, 1.35) * shapeHighAmount;
                for (std::size_t i = 0; i < 6; ++i) {
                    shapeServoDb[i] += shapeServoGainBlock * (servoTargetDb[i] - shapeServoDb[i]);
                    shapeServoGain[i] = dbToLin(shapeServoDb[i]);
                }
            }
            for (std::size_t i = 0; i < 6; ++i) {
                pL[i] *= shapeServoGain[i];
                pR[i] *= shapeServoGain[i];
            }
            pL[3] *= presenceEdgeRecoveryGain; pR[3] *= presenceEdgeRecoveryGain;
            pL[4] *= presenceBodyRecoveryGain; pR[4] *= presenceBodyRecoveryGain;
            pL[5] *= brillianceRecoveryGain; pR[5] *= brillianceRecoveryGain;
            if (densityProtectionDriveGain != 1.0) {
                for (std::size_t i = 0; i < 6; ++i) {
                    pL[i] *= densityProtectionDriveGain;
                    pR[i] *= densityProtectionDriveGain;
                }
            }
        } else {
            pL = compProcessedL;
            pR = compProcessedR;
        }
        if (activityTracking) {
            if (useCoreDensity) {
                std::array<double, 6> blendedGain{};
                for (std::size_t i = 0; i < 6; ++i) {
                    blendedGain[i] = compGain[i] * mbCompKeep + limiterGain[i] * mbCompLimiter;
                }
                currentDensityGain = *std::min_element(blendedGain.begin(), blendedGain.end());
            } else {
                currentDensityGain = *std::min_element(compGain.begin(), compGain.end());
            }
        }
        if constexpr (Analyze) {
            if (useCoreDensity) {
                double minimumGain = std::numeric_limits<double>::max();
                for (std::size_t i = 0; i < 6; ++i) {
                    minimumGain = std::min(
                        minimumGain,
                        compGain[i] * mbCompKeep + limiterGain[i] * mbCompLimiter);
                }
                analysis->sections[3].detectorGain = minimumGain;
            } else {
                analysis->sections[3].detectorGain =
                    *std::min_element(compGain.begin(), compGain.end());
            }
        }
        // Drive the detector from the current bands and apply its gain to the
        // delayed audio bands so lookahead remains effective.
        const double snubberDetector4L = pL[3];
        const double snubberDetector4R = pR[3];
        const double snubberDetector5L = pL[4];
        const double snubberDetector5R = pR[4];
        const double snubberDetector6L = pL[5];
        const double snubberDetector6R = pR[5];
        double densityDeltaReferenceL = densityPreL;
        double densityDeltaReferenceR = densityPreR;
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
            densityDeltaReferenceL = densityRefBufL[static_cast<std::size_t>(read)];
            densityDeltaReferenceR = densityRefBufR[static_cast<std::size_t>(read)];
            densityRefBufL[static_cast<std::size_t>(snubWrite)] = densityPreL;
            densityRefBufR[static_cast<std::size_t>(snubWrite)] = densityPreR;
            pL = delayedL; pR = delayedR;
            snubWrite = (snubWrite + 1) % snubBufferLength;
        }

        if (upperSnubber > 0.0) {
            const double usAmt = upperSnubber;
            const double lvl4 = absmax2(snubberDetector4L, snubberDetector4R);
            const double lvl5 = absmax2(snubberDetector5L, snubberDetector5R);
            const double lvl6 = absmax2(snubberDetector6L, snubberDetector6R);
            const double fast4 = ubFast4.lp(lvl4, upperSnubFastA), fast5 = ubFast5.lp(lvl5, upperSnubFastA), fast6 = ubFast6.lp(lvl6, upperSnubFastA);
            const double slow4 = ubSlow4.lp(lvl4, upperSnubSlowA), slow5 = ubSlow5.lp(lvl5, upperSnubSlowA), slow6 = ubSlow6.lp(lvl6, upperSnubSlowA);
            const double onset4 = clamp((fast4 - slow4 * 1.22) / std::max(slow4 * 0.80 + tiny, tiny), 0.0, 1.0);
            const double onset5 = clamp((fast5 - slow5 * 1.18) / std::max(slow5 * 0.72 + tiny, tiny), 0.0, 1.0);
            const double onset6 = clamp((fast6 - slow6 * 1.15) / std::max(slow6 * 0.66 + tiny, tiny), 0.0, 1.0);
            const double spike4 = clamp((lvl4 - slow4 * 1.20) / std::max(slow4 * 1.20 + tiny, tiny), 0.0, 1.0);
            const double spike5 = clamp((lvl5 - slow5 * 1.16) / std::max(slow5 * 1.05 + tiny, tiny), 0.0, 1.0);
            const double spike6 = clamp((lvl6 - slow6 * 1.12) / std::max(slow6 * 0.95 + tiny, tiny), 0.0, 1.0);
            const double d4 = std::max(std::abs(snubberDetector4L - usPrev4L), std::abs(snubberDetector4R - usPrev4R));
            const double d5 = std::max(std::abs(snubberDetector5L - usPrev5L), std::abs(snubberDetector5R - usPrev5R));
            const double d6 = std::max(std::abs(snubberDetector6L - usPrev6L), std::abs(snubberDetector6R - usPrev6R));
            const double c4 = std::max(std::abs(snubberDetector4L - 2 * usPrev4L + usPrev24L), std::abs(snubberDetector4R - 2 * usPrev4R + usPrev24R));
            const double c5 = std::max(std::abs(snubberDetector5L - 2 * usPrev5L + usPrev25L), std::abs(snubberDetector5R - 2 * usPrev5R + usPrev25R));
            const double c6 = std::max(std::abs(snubberDetector6L - 2 * usPrev6L + usPrev26L), std::abs(snubberDetector6R - 2 * usPrev6R + usPrev26R));
            usPrev24L = usPrev4L; usPrev24R = usPrev4R; usPrev25L = usPrev5L; usPrev25R = usPrev5R; usPrev26L = usPrev6L; usPrev26R = usPrev6R;
            usPrev4L = snubberDetector4L; usPrev4R = snubberDetector4R;
            usPrev5L = snubberDetector5L; usPrev5R = snubberDetector5R;
            usPrev6L = snubberDetector6L; usPrev6R = snubberDetector6R;
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
        l *= densityOutputGain;
        r *= densityOutputGain;
        const double densityProcessedL = l;
        const double densityProcessedR = r;
        if (outputTap == OutputTap::DensityTestpoint || outputTap == OutputTap::DensityDelta) {
            observe(4, l, r);
        }
        if (outputTap == OutputTap::DensityTestpoint)
            return {static_cast<float>(densityProcessedL), static_cast<float>(densityProcessedR)};
        if (outputTap == OutputTap::DensityDelta)
            return {static_cast<float>(densityProcessedL - densityDeltaReferenceL), static_cast<float>(densityProcessedR - densityDeltaReferenceR)};
    }
    if constexpr (Analyze) {
        analysis->sections[3].input = {
            static_cast<float>(densityPreL), static_cast<float>(densityPreR)};
        analysis->sections[3].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }

    observe(4, l, r);
    if (stopAtBoundary(4)) {
        if constexpr (Analyze) analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    const double stereoPreL = l;
    const double stereoPreR = r;
    if (shouldRunSection(4)) {
        const double stereoMid = (l + r) * 0.5;
        const double stereoSide = (l - r) * 0.5;
        const double extraWidth = std::max(0.0, sideScale - 1.0);
        // Width is the sole mono/unity/widening authority. Section bypass skips
        // this entire block and freezes enhancer history, including its filters.
        const double enhancedSide = stereoEnhancer.process(stereoMid, stereoSide, extraWidth);
        if (p054 < 100.0) {
            l = stereoMid + stereoSide * sideScale;
            r = stereoMid - stereoSide * sideScale;
        } else if (extraWidth > 0.0) {
            l = stereoMid + enhancedSide;
            r = stereoMid - enhancedSide;
        }
        if (fmPreemphasisK > 0.0) {
            l = fmPreL.emphasize(l, fmPreemphasisK);
            r = fmPreR.emphasize(r, fmPreemphasisK);
        }
        const double stereoProcessedL = l;
        const double stereoProcessedR = r;
        if (outputTap == OutputTap::StereoTestpoint || outputTap == OutputTap::StereoDelta) {
            observe(5, l, r);
        }
        if (outputTap == OutputTap::StereoTestpoint)
            return {static_cast<float>(stereoProcessedL), static_cast<float>(stereoProcessedR)};
        if (outputTap == OutputTap::StereoDelta)
            return {static_cast<float>(stereoProcessedL - stereoPreL), static_cast<float>(stereoProcessedR - stereoPreR)};
    }
    if constexpr (Analyze) {
        analysis->sections[4].input = {
            static_cast<float>(stereoPreL), static_cast<float>(stereoPreR)};
        analysis->sections[4].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }

    observe(5, l, r);
    if (stopAtBoundary(5)) {
        if constexpr (Analyze) analysis->output = {static_cast<float>(l), static_cast<float>(r)};
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    const double finalPreL = l;
    const double finalPreR = r;
    // A bypassed Final neither runs its limiter/history nor advances delivery
    // filters. Diagnostic Delta explicitly requests the processed path instead.
    if (!segmentOwnsSection(5)
        || (params.sectionBypass[5] && outputTap != OutputTap::FinalDelta)) {
        if constexpr (Analyze) {
            analysis->sections[5].input = {static_cast<float>(l), static_cast<float>(r)};
            analysis->sections[5].output = analysis->sections[5].input;
            analysis->output = analysis->sections[5].output;
        }
        observe(6, static_cast<float>(l), static_cast<float>(r));
        return {static_cast<float>(l), static_cast<float>(r)};
    }
    if (finalThresholdDriveS <= 0.0) finalThresholdDriveS = finalThresholdDriveTarget;
    finalThresholdDriveS = finalThresholdDriveTarget + finalThresholdSmooth * (finalThresholdDriveS - finalThresholdDriveTarget);
    const double finalDeltaRefPreL = l * finalThresholdDriveS;
    const double finalDeltaRefPreR = r * finalThresholdDriveS;
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
    // The legacy delay now gives the pre-limiter advance warning, before the
    // clipper can irreversibly reshape the peak. The reference and threshold
    // automation travel with this audio, not with a future detector sample.
    const auto anticipated = finalLookahead.process({
        l, r, finalDeltaRefPreL, finalDeltaRefPreR, finalThresholdDriveS,
        (finalThresholdDriveS > 1.000001 ? finalThresholdGuardGain : 1.0) * finalDriveGain});
    l = anticipated.audio.left;
    r = anticipated.audio.right;
    const double preGain = preclip.linkedLimiter(anticipated.peak, prelimitThresh, clipAttack, clipRelease);
    const double preTGain = 1.0 - prelimitMix * (1.0 - preGain);
    double finalControlGain = anticipated.audio.precedingGain * preTGain;
    l *= preTGain;
    r *= preTGain;
    const double clipInL = l, clipInR = r;
    double clipL = clipInL, clipR = clipInR;
    if (!peakOnlyFinalLimiter) {
        if (clipperMsMode) {
            double clipMid = (clipInL + clipInR) * 0.5;
            double clipSide = (clipInL - clipInR) * 0.5;
            clipMid = softClipKnee(clipMid, clipStage1, clipKnee1);
            clipSide = softClipKnee(clipSide, clipStage1, clipKnee1);
            clipMid = softClipKnee(clipMid, clipStage2, clipKnee2);
            clipSide = softClipKnee(clipSide, clipStage2, clipKnee2);
            clipL = clipMid + clipSide;
            clipR = clipMid - clipSide;
        } else {
            clipL = softClipKnee(softClipKnee(clipInL, clipStage1, clipKnee1), clipStage2, clipKnee2);
            clipR = softClipKnee(softClipKnee(clipInR, clipStage1, clipKnee1), clipStage2, clipKnee2);
        }
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
    if (p080 == 1) {
        l = lpf15_1L.run(l); r = lpf15_1R.run(r);
    } else if (p080 >= 2) {
        l = lpf15_2L.run(lpf15_1L.run(l));
        r = lpf15_2R.run(lpf15_1R.run(r));
    }
    if (fmDeemphasisAfter) {
        l = fmDeemphL.deemphasize(l, fmPreemphasisK);
        r = fmDeemphR.deemphasize(r, fmPreemphasisK);
    }
    const double ovL = fcsResL.lp(l - hardLimit(l, fcsThreshSetting), fcsResidueA);
    const double ovR = fcsResR.lp(r - hardLimit(r, fcsThreshSetting), fcsResidueA);
    double finalDeltaRefL = anticipated.audio.referenceLeft;
    double finalDeltaRefR = anticipated.audio.referenceRight;
    l -= ovL * overshootAmt;
    r -= ovR * overshootAmt;
    l = hardLimit(l, ceiling);
    r = hardLimit(r, ceiling);

    // The Final chain feeds one downstream hybrid stage. That stage owns its
    // own delay; upstream lookahead is not counted as forward knowledge here.
    constexpr double hybridShortZeroMs = 0.45;
    constexpr double hybridShortFullMs = 0.60;
    constexpr double hybridLongFullMs = 1.50;
    constexpr double hybridLongZeroMs = 2.00;
    const double hybridGuardRejectDb = std::max(
        c32, c09 + 0.1);

    // Reuse the original Final Threshold smoother rather than responding to
    // automation with an instantaneous second-stage jump. Negative Threshold
    // values produce drive > 1; positive values do not ask the hybrid for
    // extra loudness and therefore remain neutral here.
    const double hybridThresholdDrive = std::max(anticipated.audio.thresholdDrive, 1.0);
    const double hybridWorkLimitNow = ceiling / hybridThresholdDrive;
    const double hybridMakeupNow = hybridThresholdDrive;

    const int hybridCurrentWrite = hybridWrite;
    hybridRawL[static_cast<std::size_t>(hybridCurrentWrite)] = l;
    hybridRawR[static_cast<std::size_t>(hybridCurrentWrite)] = r;
    hybridFinalRefL[static_cast<std::size_t>(hybridCurrentWrite)] = finalDeltaRefL;
    hybridFinalRefR[static_cast<std::size_t>(hybridCurrentWrite)] = finalDeltaRefR;
    hybridControlGain[static_cast<std::size_t>(hybridCurrentWrite)] = finalControlGain;
    hybridWorkLimit[static_cast<std::size_t>(hybridCurrentWrite)] = hybridWorkLimitNow;
    hybridMakeup[static_cast<std::size_t>(hybridCurrentWrite)] = hybridMakeupNow;
    hybridEventGate[static_cast<std::size_t>(hybridCurrentWrite)] = 0.0;

    const double hybridCandidateNowL = hybridShaveToLimit(l, hybridWorkLimitNow, c34);
    const double hybridCandidateNowR = hybridShaveToLimit(r, hybridWorkLimitNow, c34);
    hybridCandidateL[static_cast<std::size_t>(hybridCurrentWrite)] = hybridCandidateNowL;
    hybridCandidateR[static_cast<std::size_t>(hybridCurrentWrite)] = hybridCandidateNowR;
    const double hybridDeltaNowL = hybridCandidateNowL - l;
    const double hybridDeltaNowR = hybridCandidateNowR - r;

    // Short-window burden guard for selective saturation.
    const double hybridOldRefEnergy = hybridRefEnergy[static_cast<std::size_t>(hybridEnergyWrite)];
    const double hybridOldDeltaEnergy = hybridDeltaEnergy[static_cast<std::size_t>(hybridEnergyWrite)];
    hybridRefEnergySum -= hybridOldRefEnergy;
    hybridDeltaEnergySum -= hybridOldDeltaEnergy;
    const double hybridNewRefEnergy = l * l + r * r;
    const double hybridNewDeltaEnergy = hybridDeltaNowL * hybridDeltaNowL + hybridDeltaNowR * hybridDeltaNowR;
    hybridRefEnergy[static_cast<std::size_t>(hybridEnergyWrite)] = hybridNewRefEnergy;
    hybridDeltaEnergy[static_cast<std::size_t>(hybridEnergyWrite)] = hybridNewDeltaEnergy;
    hybridRefEnergySum += hybridNewRefEnergy;
    hybridDeltaEnergySum += hybridNewDeltaEnergy;
    hybridEnergyWrite = (hybridEnergyWrite + 1) % hybridBurdenWindowSamples;
    hybridEnergyCount = std::min(hybridEnergyCount + 1, hybridBurdenWindowSamples);

    const double hybridRefRms = std::sqrt(std::max(hybridRefEnergySum, 0.0)
        / std::max(2.0 * static_cast<double>(hybridEnergyCount), 1.0));
    double hybridBurdenTarget = 1.0;
    if (hybridRefRms >= dbToLin(-60.0)) {
        const double hybridEnergyRatio = std::max(hybridDeltaEnergySum, 0.0)
            / std::max(hybridRefEnergySum, 1.0e-18);
        const double hybridBurdenDb = hybridEnergyRatio > 1.0e-12
            ? 4.342944819032518 * std::log(hybridEnergyRatio) : -120.0;
        if (hybridBurdenDb >= hybridGuardRejectDb) {
            hybridBurdenTarget = 0.0;
        } else if (hybridBurdenDb > c09) {
            const double position = smoothstep01(
                (hybridBurdenDb - c09)
                / (hybridGuardRejectDb - c09));
            hybridBurdenTarget = 1.0 - position;
        }
    }
    hybridBurdenAccept = hybridBurdenTarget < hybridBurdenAccept
        ? hybridBurdenTarget + hybridBurdenAttack * (hybridBurdenAccept - hybridBurdenTarget)
        : hybridBurdenTarget + hybridBurdenRelease * (hybridBurdenAccept - hybridBurdenTarget);
    hybridBurdenAccept = clamp(hybridBurdenAccept, 0.0, 1.0);
    hybridBurden[static_cast<std::size_t>(hybridCurrentWrite)] = hybridBurdenAccept;

    // Whole-event classifier: medium-duration, non-tonal crest events may be
    // shaved. Very short, long, repeating/tonal, or unresolved events are
    // conservatively handed to the limiter.
    const double hybridCandidateRemovedPeak = absmax2(hybridDeltaNowL, hybridDeltaNowR);
    const bool hybridCandidateActive = hybridCandidateRemovedPeak > hybridWorkLimitNow * 0.001;
    const bool hybridNewCandidateEvent = hybridCandidateActive && !hybridCandidateActivePrev;
    const bool hybridEndedCandidateEvent = !hybridCandidateActive && hybridCandidateActivePrev;
    if (hybridNewCandidateEvent) {
        hybridEventStartAbs = hybridSampleClock;
        hybridEventStartRing = hybridCurrentWrite;
        hybridEventCorrelation = hybridEventTonalCorrelation(hybridSampleClock);
        hybridOnset3 = hybridOnset2;
        hybridOnset2 = hybridOnset1;
        hybridOnset1 = hybridSampleClock;
    }
    const int hybridHistoryLength = static_cast<int>(hybridRawL.size());
    if (hybridEndedCandidateEvent && hybridEventStartAbs >= 0) {
        const auto durationSamples64 = hybridSampleClock - hybridEventStartAbs;
        const int durationSamples = static_cast<int>(std::min<std::int64_t>(
            durationSamples64, hybridHistoryLength - 1));
        const double durationMs = static_cast<double>(durationSamples64) * 1000.0 / sampleRate;
        const double lowGate = smoothstep01(
            (durationMs - hybridShortZeroMs) / (hybridShortFullMs - hybridShortZeroMs));
        const double highGate = 1.0 - smoothstep01(
            (durationMs - hybridLongFullMs) / (hybridLongZeroMs - hybridLongFullMs));
        const double tonalGate = hybridEventCorrelation >= c13 ? 0.0 : 1.0;
        double eventAccept = lowGate * highGate * tonalGate;
        if (durationSamples64 > hybridClassificationSamples) eventAccept = 0.0;
        int fill = hybridEventStartRing;
        for (int sample = 0; sample < durationSamples; ++sample) {
            hybridEventGate[static_cast<std::size_t>(fill)] = eventAccept;
            fill = (fill + 1) % hybridHistoryLength;
        }
        hybridEventStartAbs = -1;
    }
    hybridCandidateActivePrev = hybridCandidateActive;

    // Finalize the newest sample whose event decision is guaranteed complete.
    int hybridClassifierRead = hybridCurrentWrite - hybridClassificationSamples;
    if (hybridClassifierRead < 0) hybridClassifierRead += hybridHistoryLength;
    const double hybridClassifierRawL = hybridRawL[static_cast<std::size_t>(hybridClassifierRead)];
    const double hybridClassifierRawR = hybridRawR[static_cast<std::size_t>(hybridClassifierRead)];
    const double hybridClassifierAccept = clamp(std::min(
        hybridBurden[static_cast<std::size_t>(hybridClassifierRead)],
        hybridEventGate[static_cast<std::size_t>(hybridClassifierRead)]), 0.0,
        c11 * 0.01);
    const double hybridClassifierCandidateL = hybridCandidateL[static_cast<std::size_t>(hybridClassifierRead)];
    const double hybridClassifierCandidateR = hybridCandidateR[static_cast<std::size_t>(hybridClassifierRead)];
    const double hybridClassifierShavedL = hybridClassifierRawL
        + (hybridClassifierCandidateL - hybridClassifierRawL) * hybridClassifierAccept;
    const double hybridClassifierShavedR = hybridClassifierRawR
        + (hybridClassifierCandidateR - hybridClassifierRawR) * hybridClassifierAccept;
    hybridShavedL[static_cast<std::size_t>(hybridClassifierRead)] = hybridClassifierShavedL;
    hybridShavedR[static_cast<std::size_t>(hybridClassifierRead)] = hybridClassifierShavedR;
    const double hybridClassifierLimit = hybridWorkLimit[static_cast<std::size_t>(hybridClassifierRead)];
    const double hybridClassifierDet = absmax2(hybridClassifierShavedL, hybridClassifierShavedR);
    hybridRequiredGain[static_cast<std::size_t>(hybridClassifierRead)] =
        hybridClassifierDet > hybridClassifierLimit
            ? hybridClassifierLimit / std::max(hybridClassifierDet, tiny) : 1.0;
    hybridEnvelope.update(hybridClassifierRead,
        hybridRequiredGain[static_cast<std::size_t>(hybridClassifierRead)]);

    int hybridOutputRead = hybridCurrentWrite - hybridTotalDelaySamples;
    if (hybridOutputRead < 0) hybridOutputRead += hybridHistoryLength;
    const double hybridTargetGain = hybridEnvelope.minimum(
        hybridOutputRead, hybridLimiterLookaheadSamples + 1,
        [&](int distance) noexcept { return hybridAttackShape[static_cast<std::size_t>(distance)]; });
    hybridLimiterGain = hybridTargetGain < hybridLimiterGain
        ? hybridTargetGain
        : hybridTargetGain + hybridLimiterRelease * (hybridLimiterGain - hybridTargetGain);
    if (hybridLimiterGain > 0.999999999) hybridLimiterGain = 1.0;

    l = hybridShavedL[static_cast<std::size_t>(hybridOutputRead)] * hybridLimiterGain
        * hybridMakeup[static_cast<std::size_t>(hybridOutputRead)];
    r = hybridShavedR[static_cast<std::size_t>(hybridOutputRead)] * hybridLimiterGain
        * hybridMakeup[static_cast<std::size_t>(hybridOutputRead)];
    finalDeltaRefL = hybridFinalRefL[static_cast<std::size_t>(hybridOutputRead)];
    finalDeltaRefR = hybridFinalRefR[static_cast<std::size_t>(hybridOutputRead)];
    l = hardLimit(l, ceiling);
    r = hardLimit(r, ceiling);
    finalControlGain = hybridControlGain[static_cast<std::size_t>(hybridOutputRead)] * hybridLimiterGain;

    hybridWrite = (hybridWrite + 1) % hybridHistoryLength;
    ++hybridSampleClock;

    processDelivery(l, r, finalDeltaRefL, finalDeltaRefR, finalControlGain);
    // One measurement for visual and accessible reports: cumulative applied
    // linear gain, time-aligned with output, excluding static drive and makeup.
    if (activityTracking) currentFinalGain = finalControlGain;
    if constexpr (Analyze) analysis->sections[5].detectorGain = finalControlGain;
    l = hardLimit(l, ceiling);
    r = hardLimit(r, ceiling);

    if constexpr (Analyze) {
        analysis->sections[5].input = {
            static_cast<float>(finalPreL), static_cast<float>(finalPreR)};
        analysis->sections[5].output = {
            static_cast<float>(l), static_cast<float>(r)};
    }
    if (outputTap == OutputTap::FinalDelta) observe(6, l, r);
    if (outputTap == OutputTap::FinalDelta) {
        l -= finalDeltaRefL;
        r -= finalDeltaRefR;
    }
    if constexpr (Analyze) {
        analysis->output = {static_cast<float>(l), static_cast<float>(r)};
    }
    if (!params.sectionBypass[5] && outputTap == OutputTap::FullChain) {
        const float deliveryCeiling = std::nextafter(
            static_cast<float>(ceiling), 0.0f);
        const std::pair<float, float> delivered {
            std::clamp(static_cast<float>(l), -deliveryCeiling, deliveryCeiling),
            std::clamp(static_cast<float>(r), -deliveryCeiling, deliveryCeiling)
        };
        observe(6, delivered.first, delivered.second);
        return delivered;
    }
    return {static_cast<float>(l), static_cast<float>(r)};
}

std::pair<float, float> OptiLabCoreEngine::processSample(float left, float right) {
    return processSampleImpl<false, false>(left, right, nullptr);
}

std::pair<float, float> OptiLabCoreEngine::processSampleAnalyzed(
    float left, float right, OptiLabAnalysisFrame& analysis) {
    analysis = {};
    return processSampleImpl<true, false>(left, right, &analysis);
}

void OptiLabCoreEngine::processInterleaved(float* samples, std::size_t frames, std::size_t channels) {
    if (!samples || channels == 0) {
        return;
    }
    const OptiLabDenormalGuard denormalGuard;
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

void OptiLabCoreEngine::processPlanar(float* left, float* right, std::size_t frames) {
    if (!left) {
        return;
    }
    const OptiLabDenormalGuard denormalGuard;
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

// Boundary delay is structural and cached whenever role, split, bypass, listen, or timing changes.
std::size_t OptiLabCoreEngine::boundaryLatency(int boundary) const noexcept {
    const auto index = static_cast<std::size_t>(std::clamp(boundary, 0, 6));
    return boundaryLatencyCache[index];
}

void OptiLabCoreEngine::observeAux(int boundary, double l, double r, AuxFrame* aux) const noexcept {
    if (aux == nullptr || aux->captured) return;
    if (c24 == 1 && c18 == boundary) {
        aux->output = {l, r};
        aux->capturedLatency = boundaryLatency(boundary);
        aux->captured = true;
    }
}

std::pair<float, float> OptiLabCoreEngine::processSampleRouted(double left, double right,
    double auxLeft, double auxRight, AuxFrame& aux, OptiLabAnalysisFrame* analysis) {
    const auto sanitize = [](double x) noexcept {
        return std::isfinite(x) && std::abs(x) < 1.0e12 ? x : 0.0;
    };
    left = sanitize(left); right = sanitize(right);
    aux = {};
    // Aux Input remains physically visible for the later sidechain phase, but
    // split processing is deliberately DAW-neutral and uses ordinary Main 1/2.
    aux.input = {sanitize(auxLeft), sanitize(auxRight)};
    if (analysis != nullptr) *analysis = {};

    // Aux Off is the normal production path. Do not carry tap observation through
    // every section or advance a history ring that cannot be observed.
    if (c24 != 1) {
        return analysis != nullptr
            ? processSampleImpl<true, false>(static_cast<float>(left), static_cast<float>(right), analysis, nullptr)
            : processSampleImpl<false, false>(static_cast<float>(left), static_cast<float>(right), nullptr, nullptr);
    }

    const auto capacity = auxTapHistory.size();
    const auto result = analysis != nullptr
        ? processSampleImpl<true, true>(static_cast<float>(left), static_cast<float>(right), analysis, &aux)
        : processSampleImpl<false, true>(static_cast<float>(left), static_cast<float>(right), nullptr, &aux);
    if (c18 == 0) {
        aux.output = {left, right};
        aux.captured = true;
        aux.capturedLatency = 0;
    }
    if (capacity != 0) {
        const auto delay = std::min(
            latencyCache > aux.capturedLatency ? latencyCache - aux.capturedLatency : 0,
            capacity - 1);
        auxTapHistory[auxWrite] = aux.output;
        aux.output = auxTapHistory[(auxWrite + capacity - delay) % capacity];
        if (++auxWrite == capacity) auxWrite = 0;
    }
    return result;
}

#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

class OptiLabCore {
public:
    enum class Mode {
        PodcastLeveler = 0,
        StreamPolish = 1,
        SmoothLimiter = 2,
    };

    struct Parameters {
        Mode mode = Mode::PodcastLeveler;
        double inputDriveDb = 3.5;
        double autoAdaptPct = 0.0;
    };

    static Parameters defaultParameters(Mode mode) noexcept;

    void prepare(double newSampleRate);
    void setParameters(const Parameters& newParameters);
    Parameters parameters() const { return params; }
    std::size_t latencySamples() const { return pathDelay; }
    void reset();

    std::pair<float, float> processSample(float left, float right);
    void processPlanar(float* left, float* right, std::size_t frames);
    void processInterleaved(float* samples, std::size_t frames, std::size_t channels);

private:
    struct OnePole {
        double z = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;

        double lp(double x, double a);
        double lp2(double x, double a);
        double hp(double x, double a);
        void reset();
    };

    struct Allpass1 {
        double x1 = 0.0;
        double y1 = 0.0;

        double run(double x, double c);
        void reset();
    };

    struct PeakEnv {
        double e = 0.0;

        double run(double det, double attack, double release);
        double set(double value);
    };

    struct GainCell {
        double env = 0.0;
        double gain = 1.0;

        double bandLimitPd(double det, double thresh, double attack, double relBase, double relSlow, double pdAmt);
        double bandLimitPdSoft(double det, double thresh, double attack, double relBase, double relSlow,
                               double pdAmt, double ratio, double knee, double minGain);
        double linkedLimiter(double det, double thresh, double attack, double release);
    };

    struct SmoothRounder {
        double prevSat = 0.0;
        double run(double x, double amt);
        void reset();
    };

    struct DcClipper {
        double dcZ = 0.0;
        double run(double x, double th, double amt, double kneeMul, double cancelAmt, double a);
        void reset();
    };

    struct BassClipper {
        double z1 = 0.0;
        double z2 = 0.0;
        double run(double x, double th, double amt, double kneeMul, double drive, double resMix, double a);
        void reset();
    };

    struct Biquad {
        double a0 = 1.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double x1 = 0.0;
        double x2 = 0.0;
        double y1 = 0.0;
        double y2 = 0.0;

        void setLowpass(double freq, double q, double sampleRate);
        void setPeak(double freq, double q, double gainDb, double sampleRate);
        double run(double x);
        void reset();
    };

    static double dbToLin(double db);
    static double linToDb(double x);
    static double clamp(double x, double lo, double hi);
    static double absmax2(double a, double b);
    static double softClipKnee(double x, double th, double kneeMul);
    static double purePeakRound(double x, double th, double amt, double driveMul);
    static double hardLimit(double x, double ceiling);

    double apCoeff(double freq) const;
    void applyModeAndDerivedParameters();
    void resetPhaseState();
    Parameters params;
    double sampleRate = 48000.0;
    int lastMode = -1;

    static constexpr int fcsBufferLength = 512;
    static constexpr int snubBufferLength = 512;
    static constexpr int masterBufferLength = 4096;

    std::array<double, fcsBufferLength> fcsBufL{};
    std::array<double, fcsBufferLength> fcsBufR{};
    std::array<double, fcsBufferLength> fcsBufOL{};
    std::array<double, fcsBufferLength> fcsBufOR{};
    int fcsWrite = 0;

    std::array<std::array<double, snubBufferLength>, 6> snubL{};
    std::array<std::array<double, snubBufferLength>, 6> snubR{};
    int snubWrite = 0;

    std::array<double, masterBufferLength> masterBufL{};
    std::array<double, masterBufferLength> masterBufR{};
    int masterWrite = 0;

    std::size_t pathDelay = 0;

    double inputGain = 1.0;
    double agcDrive = 1.0;
    double densityAudioGain = 1.0;
    double densityDetectorGain = 1.0;
    double densityClipDrive = 1.0;
    double presenceGain = 1.0;
    double brillianceGain = 1.0;
    double clipDriveMb = 1.0;
    double clipDriveFull = 1.0;
    double ceiling = 1.0;
    double finalThresholdDriveTarget = 1.0;
    double finalThresholdDriveS = 1.0;
    double finalThresholdGuardGain = 1.0;
    double finalThresholdMakeup = 1.0;
    double smoothDriveRounderAmt = 0.0;
    double postAgcSmoothDriveAmt = 0.0;
    double postXt2SmoothDriveAmt = 0.0;
    double postAgcSmoothRecoveryGain = 1.0;
    double postXt2SmoothRecoveryGain = 1.0;
    double preFinalDriveGain = 1.0;
    double outputGain = 1.0;
    double mbClipMix = 0.0;
    double dcCancel = 0.0;
    double dc3Amt = 0.0;
    double dc4Amt = 0.0;
    double dc5Amt = 0.0;
    double dc6Amt = 0.0;
    double overshootAmt = 0.0;
    double recombControl = 0.0;
    double pdRelease = 0.0;
    double upperSnubber = 0.0;
    double gateReopenStrength = 0.0;
    double sideScale = 1.0;

    double inputTrimDb = 0.0;
    double phaseRotatePct = 0.0;
    int subsonicHpf = 0;
    double agcAmountPct = 0.0;
    double agcDriveDb = 0.0;
    double releaseTime = 6.0;
    double pdReleasePct = 0.0;
    double gateThresholdDb = -70.0;
    double gateReopenSpeedMs = 50.0;
    double gateReopenStrengthPct = 0.0;
    double postAgcSmoothDrivePct = 0.0;
    double bassCouplingPct = 0.0;
    double bassEqDb = 0.0;
    double bassScReliefPct = 0.0;
    double lowBassFloorPct = 0.0;
    double lowCoherencePct = 0.0;
    double lowReleaseStabPct = 0.0;
    double transitionFillPct = 0.0;
    double adaptiveBassCouplingPct = 0.0;
    double bassClipPct = 0.0;
    double bassClipDensityPct = 0.0;
    double densityDb = 0.0;
    double xt2AmountPct = 0.0;
    double presenceDb = 0.0;
    double brillianceDb = 0.0;
    double adaptiveTopCouplingPct = 0.0;
    int crossoverModel = 2;
    double mbClipPct = 0.0;
    double clipDriveDb = 0.0;
    double recombControlPct = 0.0;
    double dcCancelPct = 0.0;
    double upperSnubberPct = 0.0;
    double snubberLookaheadMs = 0.0;
    double postXt2SmoothDrivePct = 0.0;
    int stereoMode = 0;
    double stereoWidthPct = 100.0;
    double preFinalDriveDb = 0.0;
    int clipperStyle = 0;
    double preLimiterPct = 100.0;
    double clipRestraintPct = 100.0;
    double overshootPct = 100.0;
    double lookaheadMs = 0.0;
    double finalThresholdDb = 0.0;
    double finalThresholdMakeupPct = 0.0;
    double clipCeilingDb = -0.1;
    int topFilterMode = 0;
    double outputTrimDb = 0.0;
    double smoothDriveRounderPct = 0.0;
    int processorMode = 0;

    int phaseMode = 0;
    int phaseModeLast = -1;
    int phaseStages = 0;
    int effectiveSnubberLookahead = 0;
    int effectiveFinalLookahead = 0;
    int masteringLookaheadSamples = 0;

    double agcMix = 0.0;
    double agcDownMix = 0.0;
    double xt2Mix = 0.0;
    double adaptiveTopCoupling = 0.0;
    double bassCoupling = 0.0;
    double bassScRelief = 0.0;
    double lowCoherence = 0.0;
    double lowReleaseStab = 0.0;
    double transitionFill = 0.0;
    double lowBassFloor = 0.0;
    double adaptiveBassCoupling = 0.0;
    double bassClip = 0.0;
    double bassClipDensity = 0.0;
    double prelimitMix = 1.0;
    double clipRestraint = 1.0;
    bool peakOnlyFinalLimiter = false;
    double prelimitThresh = 1.0;
    double fcsThreshSetting = 1.0;

    double agcEnvAttack = 0.0;
    double agcGainAttack = 0.0;
    double agcRelease = 0.0;
    double agcReleaseSlow = 0.0;
    double bandAttack = 0.0;
    double upperBandAttack = 0.0;
    double upperMidBandAttack = 0.0;
    double bandRelease = 0.0;
    double bandReleaseSlow = 0.0;
    double lowBandRelease = 0.0;
    double lowBandReleaseSlow = 0.0;
    double clipAttack = 0.0;
    double clipRelease = 0.0;
    double finalThresholdSmooth = 0.0;
    double recombAttack = 0.0;
    double recombRelease = 0.0;
    double startupActivityRelease = 0.0;
    double masterCatchAttack = 0.0;
    double masterCatchRelease = 0.0;
    double masterEnvAttack = 0.0;
    double masterEnvRelease = 0.0;
    double gateDetectorRelease = 0.0;
    double gateCloseCoeff = 0.0;
    double gateOpenCoeff = 0.0;
    double gateAgcFreezeRelease = 0.0;
    double gateXt2FreezeRelease = 0.0;
    double gateReopenRelease = 0.0;
    double gateReopenEnvRelease = 0.0;
    double gateReopenDecay = 0.0;
    double gateReopenPulseScale = 0.0;
    double gateAgcDriftTarget = 0.0;
    double gateAgcDriftCoeff = 0.0;
    double upperSnubFastA = 0.0;
    double upperSnubSlowA = 0.0;
    double upperSnubDeltaA = 0.0;
    double upperSnubCurveA = 0.0;
    double upperSnubGainAttack = 0.0;
    double upperSnubGainRelease = 0.0;
    double hpf30A = 0.0;
    double agcSplitA = 0.0;
    double x1A = 0.0;
    double x2A = 0.0;
    double x3A = 0.0;
    double x4A = 0.0;
    double x5A = 0.0;
    double distCancelA = 0.0;
    std::array<double, 12> apC{};
    double agcTarget = 0.0;
    double agcMaxGain = 0.0;
    double agcMinGain = 0.0;
    double gateLin = 0.0;
    double b1Thresh = 0.0;
    double b2Thresh = 0.0;
    double b3Thresh = 0.0;
    double b4Thresh = 0.0;
    double b5Thresh = 0.0;
    double b6Thresh = 0.0;
    double constDb2LinMinus62 = 0.0;
    double constDb2LinMinus36 = 0.0;
    double clipRef = 0.0;
    double mbWorkRef = 0.0;
    double b1DetScGain = 1.0;
    double b2DetScGain = 1.0;
    double adaptBassSplitA = 0.0;
    double adaptBassDetAttack = 0.0;
    double adaptBassDetRelease = 0.0;
    double adaptBassGainUp = 0.0;
    double adaptBassGainDown = 0.0;
    double adaptBassTargetLow = 0.33;
    double adaptBassTargetHigh = 0.52;
    double adaptBassMaxBoostDb = 0.0;
    double adaptBassMaxCutDb = 0.0;
    double adaptTopDetAttack = 0.0;
    double adaptTopDetRelease = 0.0;
    double adaptTopGainUp = 0.0;
    double adaptTopGainDown = 0.0;
    double adaptTopEdgeTargetLow = 0.0;
    double adaptTopEdgeTargetHigh = 0.0;
    double adaptTopAirTargetLow = 0.0;
    double adaptTopAirTargetHigh = 0.0;
    double adaptTopPresenceMaxBoostDb = 0.0;
    double adaptTopPresenceMaxCutDb = 0.0;
    double adaptTopAirMaxBoostDb = 0.0;
    double adaptTopAirMaxCutDb = 0.0;
    double adaptTopAirHpA = 0.0;
    double bassClipSplitA = 0.0;
    double bassClipSubhpA = 0.0;
    double bassClipPreResA = 0.0;
    double bassClipPreTh = 0.0;
    double bassClipPreDrive = 1.0;
    double bassClipPreAmt = 0.0;
    double bassClipMakeup = 1.0;
    double recombClipThresh = 1.0;
    double recombClipKnee = 0.5;
    double clipStage1 = 1.0;
    double clipStage2 = 1.0;
    double clipKnee1 = 0.5;
    double clipKnee2 = 0.5;
    double fcsResidueA = 0.0;

    double agcLGain = 1.0;
    double agcHGain = 1.0;
    double startupActivity = 0.0;
    double gateProgEnv = 0.0;
    double gateState = 0.0;
    double gateClosedMemory = 0.0;
    double gateReopenEnv = 0.0;
    double gateReopenPulse = 0.0;
    double adaptBassGain = 1.0;
    double adaptTopPresenceGain = 1.0;
    double adaptTopAirGain = 1.0;
    int masterStartupArmed = 1;
    int masterStartupAge = 0;
    int masterStartupPrimeWindow = 0;
    double masterStartupActiveThresh = 0.0;
    double masterCatchGain = 1.0;

    OnePole hp30L, hp30R;
    std::array<Allpass1, 12> apL, apR;
    OnePole agcLpL, agcLpR;
    PeakEnv agcLowEnv, agcHighEnv;
    SmoothRounder postAgcRoundL, postAgcRoundR;
    PeakEnv masterRawEnv, masterAmpEnv, masterCatchEnv;
    Biquad bassPeakL, bassPeakR;
    OnePole adaptBassLpL, adaptBassLpR;
    PeakEnv adaptBassEnv, adaptProgEnv;
    OnePole bassclipSplitL, bassclipSplitR, bassclipHpfL, bassclipHpfR;
    BassClipper bassclipPreL, bassclipPreR;
    std::array<OnePole, 5> xbL, xbR;
    Biquad lr4b1aL, lr4b1aR, lr4b1bL, lr4b1bR;
    Biquad lr4b2aL, lr4b2aR, lr4b2bL, lr4b2bR;
    Biquad lr4b3aL, lr4b3aR, lr4b3bL, lr4b3bR;
    Biquad lr4b4aL, lr4b4aR, lr4b4bL, lr4b4bR;
    Biquad lr4b5aL, lr4b5aR, lr4b5bL, lr4b5bR;
    OnePole adaptTopAirHpL, adaptTopAirHpR;
    PeakEnv adaptTopEdgeEnv, adaptTopEdgeProgEnv, adaptTopAirEnv, adaptTopAirProgEnv;
    std::array<GainCell, 6> lim;
    std::array<DcClipper, 4> mbClipL, mbClipR;
    OnePole ubFast4, ubFast5, ubFast6;
    OnePole ubSlow4, ubSlow5, ubSlow6;
    OnePole ubDslow4, ubDslow5, ubDslow6;
    OnePole ubCslow4, ubCslow5, ubCslow6;
    PeakEnv ubGate4, ubGate5, ubGate6;
    GainCell ubLim6;
    double usPrev4L = 0.0, usPrev4R = 0.0, usPrev24L = 0.0, usPrev24R = 0.0;
    double usPrev5L = 0.0, usPrev5R = 0.0, usPrev25L = 0.0, usPrev25R = 0.0;
    double usPrev6L = 0.0, usPrev6R = 0.0, usPrev26L = 0.0, usPrev26R = 0.0;
    Biquad transitionPeakL, transitionPeakR;
    Biquad lowFloorL, lowFloorR;
    SmoothRounder postXt2RoundL, postXt2RoundR;
    PeakEnv finalFullEnv;
    GainCell preclip;
    OnePole distLpfL, distLpfR;
    Biquad lpf15_1L, lpf15_1R, lpf15_2L, lpf15_2R;
    SmoothRounder finalRoundL, finalRoundR;
    OnePole fcsResL, fcsResR;
};

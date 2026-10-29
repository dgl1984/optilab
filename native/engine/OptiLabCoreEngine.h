#pragma once

#include "OptiLabAnalysis.h"
#include "OptiLabCrossover.h"
#include "OptiLabParameterCatalog.h"
#include "OptiLabInternalState.h"
#include "OptiLabStereoEnhancer.h"
#include "OptiLabFinalLookahead.h"
#include "OptiLabLookaheadEnvelope.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class OptiLabCoreEngine {
public:
    // Transparent comparison lets realtime parameter updates look up registry
    // keys through string_view without allocating a temporary std::string.
    using ParameterValues = std::map<std::string, double, std::less<>>;

    using ParameterExposure = optilab_parameters::Exposure;
    using ParameterInfo = optilab_parameters::Descriptor;

    enum class OutputTap {
        FullChain,
        InputTestpoint,
        InputDelta,
        AgcTestpoint,
        AgcDelta,
        BassTestpoint,
        BassDelta,
        DensityTestpoint,
        DensityDelta,
        StereoTestpoint,
        StereoDelta,
        FinalDelta,
    };

    using Mode = optilab_parameters::CoreMode;

    struct Parameters {
        Mode mode = Mode::StreamPolish;
        double inputDriveDb = 0.0;
        double autoAdaptPct = 0.0;
        ParameterValues overrides;
        ParameterValues internalOverrides;
        std::array<bool, 6> sectionBypass{};
    };

    struct Activity {
        double agcGain = 1.0;
        double densityGain = 1.0;
        double finalGain = 1.0;
    };

    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;

    static bool isSampleRateSupported(double sampleRate) noexcept;
    static Parameters defaultParameters(Mode mode) noexcept;
    static const std::vector<ParameterInfo>& parameterInfo();
    static bool isParameterSupported(std::string_view key);
    static bool isHostAutomatable(std::string_view key);
    static bool isCoreInternalKey(std::string_view key) noexcept;
    static bool isCoreValueSupported(std::string_view key) noexcept;

    void prepare(double newSampleRate);
    void setParameters(const Parameters& newParameters);
    void setPreparedParameters(Mode mode, const ParameterValues& values, double inputDriveDb, int presetIndex) noexcept;
    void setParameterValue(std::string_view key, double value);
    bool setParameterValueRealtime(std::string_view key, double value) noexcept;
    bool setParameterValueRealtimeDeferred(std::string_view key, double value) noexcept;
    void commitRealtimeParameterChanges() noexcept;
    void setSectionBypass(std::size_t section, bool bypassed) noexcept;
    Parameters parameters() const { return params; }
    Activity activity() const noexcept;
    ParameterValues effectiveParameterValues() const;
    ParameterValues effectiveCoreValues() const;
    void setActivityTracking(bool enabled) noexcept { activityTracking = enabled; }
    void setOutputTap(OutputTap tap) noexcept;
    bool auxTapEnabled() const noexcept { return c24 == 1; }
    std::size_t latencySamples() const noexcept;
    void reset();

    std::pair<float, float> processSample(float left, float right);
    std::pair<float, float> processSampleAnalyzed(
        float left, float right, OptiLabAnalysisFrame& analysis);
    struct AuxFrame {
        std::pair<double, double> output{};
        std::pair<double, double> input{};
        std::size_t capturedLatency = 0;
        bool captured = false;
    };
    std::pair<float, float> processSampleRouted(double left, double right,
        double auxLeft, double auxRight, AuxFrame& aux, OptiLabAnalysisFrame* analysis = nullptr);
    void processPlanar(float* left, float* right, std::size_t frames);
    void processInterleaved(float* samples, std::size_t frames, std::size_t channels);

private:
    template <bool Analyze, bool AuxEnabled = false>
    std::pair<float, float> processSampleImpl(
        float left, float right, OptiLabAnalysisFrame* analysis, AuxFrame* aux = nullptr);
    std::size_t boundaryLatency(int boundary) const noexcept;
    bool segmentOwnsSection(std::size_t section) const noexcept;
    bool diagnosticRequestsSection(std::size_t section) const noexcept;
    bool shouldRunSection(std::size_t section) const noexcept;
    bool stopAtBoundary(int boundary) const noexcept;
    bool diagnosticTapIsInSegment() const noexcept;
    void refreshRoutingCache() noexcept;
    void observeAux(int boundary, double l, double r, AuxFrame* aux) const noexcept;
    std::vector<std::pair<double, double>> auxTapHistory;
    std::size_t auxWrite = 0;
    std::array<bool, 6> sectionOwnedCache{true, true, true, true, true, true};
    std::array<bool, 6> sectionRunCache{true, true, true, true, true, true};
    bool allSectionsBypassedCache = false;
    std::array<std::size_t, 7> boundaryLatencyCache{};
    std::size_t latencyCache = 0;
    int diagnosticSectionCache = -1;
    int stopBoundaryCache = -1;

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

    struct EventCell {
        double env = 0.0;
        double gain = 1.0;

        double run(
            double det,
            double threshDb,
            double quietThreshold,
            double attack,
            double sampleRate,
            double minimumRelease,
            double maximumRelease
        );
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
        void setHighpass(double freq, double q, double sampleRate);
        void setAllpass(double freq, double q, double sampleRate);
        void setPeak(double freq, double q, double gainDb, double sampleRate);
        void setLowShelf(double freq, double slope, double gainDb, double sampleRate);
        void copyCoefficientsFrom(const Biquad& source);
        double run(double x);
        void reset();
    };

    struct FmFilter {
        double x1 = 0.0;
        bool primed = false;

        double emphasize(double x, double k);
        double deemphasize(double y, double k);
        void reset();
    };

    struct FoundationGuard {
        Biquad lp70L, lp70R, lp250L, lp250R;
        Biquad lp420L, lp420R, lp3700L, lp3700R;
        Biquad shelfL, shelfR, bodyL, bodyR;
        PeakEnv deepFast, usefulFast, midFast;
        PeakEnv deepSlow, usefulSlow, midSlow;
        bool primed = false;
        std::size_t ageSamples = 0;
        int controlCounter = 15;
        int coefficientCounter = 63;
        double fastDeepBoostEvidenceDb = 0.0;
        double fastUsefulBoostEvidenceDb = 0.0;
        double fastDeepCutEvidenceDb = 0.0;
        double fastUsefulCutEvidenceDb = 0.0;
        double deepBoostStateDb = 0.0;
        double usefulBoostStateDb = 0.0;
        double deepCutStateDb = 0.0;
        double usefulCutStateDb = 0.0;
        double deepTargetDb = 0.0;
        double usefulTargetDb = 0.0;
        double deepTargetGain = 1.0;
        double usefulTargetGain = 1.0;
        double voiceTarget = 1.0;
        double voiceGain = 1.0;
        double detAttack = 0.0;
        double detRelease = 0.0;
        double slowAttack = 0.0;
        double slowRelease = 0.0;
        double fastBoostAttackTick = 0.0;
        double fastBoostReleaseTick = 0.0;
        double fastCutAttackTick = 0.0;
        double fastCutReleaseTick = 0.0;
        double boostAttackTick = 0.0;
        double boostReleaseTick = 0.0;
        double boostSuppressTick = 0.0;
        double cutAttackTick = 0.0;
        double cutReleaseTick = 0.0;
        double cutSuppressTick = 0.0;
        std::size_t cutHoldoffSamples = 0;
        double voiceReturn = 0.0;
        double voiceWithdraw = 0.0;
        double boostScale = 0.0;
        double cutAuthority = 0.0;
        double link = 0.0;
        double withdrawAuthority = 0.0;

        // Optional Core-style closed-loop refinement. The existing density
        // split reports its post-Bass balance one control block later; no
        // additional audio crossover is introduced.
        double feedbackB1Accum = 0.0;
        double feedbackB2Accum = 0.0;
        std::size_t feedbackCount = 0;
        double feedbackRatioDb = -120.0;
        double feedbackServoDb = 0.0;
        double feedbackBurden = 0.0;
        double feedbackAttackTick = 0.0;
        double feedbackReleaseTick = 0.0;
        double feedbackFastReleaseTick = 0.0;
        double feedbackBurdenAttackTick = 0.0;
        double feedbackBurdenReleaseTick = 0.0;
        double feedbackMaxAssistDb = 12.0;
        void reset();
    };

    static double dbToLin(double db);
    static double linToDb(double x);
    static double clamp(double x, double lo, double hi);
    static double smoothstep01(double x);
    static double adaptiveBassBoostCurve(double x);
    static double adaptiveBassCutCurve(double x);
    static double absmax2(double a, double b);
    static double softClipKnee(double x, double th, double kneeMul);
    static double limiterBandClip(double x, double limit, double amount, double soft);
    static double purePeakRound(double x, double th, double amt, double driveMul);
    static double hardLimit(double x, double ceiling);
    static double hybridShaveToLimit(double x, double limit, double depthDb);
    double hybridEventTonalCorrelation(std::int64_t nowAbs) const;
    void processDelivery(
        double& left, double& right, double& referenceLeft, double& referenceRight,
        double& controlGain);

    double apCoeff(double freq) const;
    void ensureHybridCapacityForSampleRate();
    std::pair<double, double> processFoundationGuard(
        double preL, double preR, double boostedL, double boostedR
    );
    void applyModeAndDerivedParameters();
    bool realtimeDerivedDirty_ = false;
    void applyParameterOverrides();
    void resetPhaseState();
    Parameters params;
    double sampleRate = 48000.0;
    int lastMode = -1;

    // These fixed rings must preserve their physical-time windows through the
    // full advertised sample-rate range. 1.5 ms at 768 kHz is 1152 samples;
    // the mastering path needs 10 ms = 7680 samples.
    static constexpr int fcsBufferLength = OptiLabFinalLookahead::capacity;
    static constexpr int snubBufferLength = 2048;
    static constexpr int masterBufferLength = 8192;

    OptiLabFinalLookahead finalLookahead;

    // One transparent reconstruction-aware delivery limiter owns final
    // ceiling safety. Its detector is oversampled; audio remains at the host
    // sample rate. The reference buffers keep Final Delta time-aligned.
    static constexpr int deliveryBufferLength = 2048;
    static constexpr int deliveryDetectorTaps = 25;
    static constexpr int deliveryDetectorPhases = 8;
    static constexpr int deliveryDetectorDelaySamples = 12;
    std::array<double, deliveryBufferLength> deliveryBufL{};
    std::array<double, deliveryBufferLength> deliveryBufR{};
    std::array<double, deliveryBufferLength> deliveryRefL{};
    std::array<double, deliveryBufferLength> deliveryRefR{};
    std::array<double, deliveryBufferLength> deliveryRequired{};
    std::array<double, deliveryBufferLength> deliveryControlGain{};
    std::array<double, deliveryDetectorTaps> deliveryHistL{};
    std::array<double, deliveryDetectorTaps> deliveryHistR{};
    std::array<double, deliveryDetectorTaps * deliveryDetectorPhases> deliveryCoeff{};
    int deliveryWrite = 0;
    int deliveryLookaheadSamples = 0;
    int deliveryAnticipationSamples = 1;
    double deliveryGain = 1.0;
    double deliveryRelease = 0.0;
    double deliveryTarget = 1.0;

    // Shared sustained-event guard for the final delivery limiter. This is
    // deliberately mode-agnostic: Podcast, Stream, and Smooth all use the
    // same final limiter behavior. The guard remembers restrictive gain
    // decisions across dense/periodic material so fast peak recovery does not
    // chase the waveform between adjacent cycles.
    static constexpr int deliverySustainedHistoryLength = 18;
    std::array<double, deliverySustainedHistoryLength> deliverySustainedHistory{};
    int deliverySustainedHistoryWrite = 0;
    int deliverySustainedChunkSamples = 64;
    int deliverySustainedChunkCounter = 64;
    double deliverySustainedL = 0.0;
    double deliverySustainedR = 0.0;
    double deliverySustainedPeak = 0.0;
    double deliverySustainedCoeff = 0.0;
    double deliverySustainedSmoothCoeff = 0.0;
    double deliverySustainedTarget = 1.0;
    double deliverySustainedSmoothed = 1.0;
    OptiLabLookaheadEnvelope deliveryEnvelope, hybridEnvelope;

    // Post-Final hybrid: the original limiter/clipper chain remains intact.
    // Its completed output is fed into this independent selective shaver +
    // lookahead limiter, so the hybrid never changes legacy clipper behavior.
    // Preserve the original ring topology through 192 kHz. At higher rates
    // prepare() grows these vectors only as needed to preserve the same
    // physical-time classifier/burden windows.
    static constexpr int hybridBaseHistoryLength = 16384;
    static constexpr int hybridBaseEnergyLength = 8192;
    std::vector<double> hybridRawL = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridRawR = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridCandidateL = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridCandidateR = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridShavedL = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridShavedR = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridRequiredGain = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridBurden = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridEventGate = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridWorkLimit = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridMakeup = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridFinalRefL = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridFinalRefR = std::vector<double>(hybridBaseHistoryLength, 0.0);
    std::vector<double> hybridControlGain = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridAttackShape = std::vector<double>(hybridBaseHistoryLength, 1.0);
    std::vector<double> hybridRefEnergy = std::vector<double>(hybridBaseEnergyLength, 0.0);
    std::vector<double> hybridDeltaEnergy = std::vector<double>(hybridBaseEnergyLength, 0.0);
    int hybridWrite = 0;
    int hybridEnergyWrite = 0;
    int hybridEnergyCount = 0;
    int hybridBurdenWindowSamples = 1;
    int hybridClassificationSamples = 0;
    int hybridLimiterLookaheadSamples = 0;
    int hybridTotalDelaySamples = 0;
    int hybridPeriodMinSamples = 2;
    int hybridPeriodMaxSamples = 3;
    std::int64_t hybridSampleClock = 0;
    bool hybridCandidateActivePrev = false;
    std::int64_t hybridEventStartAbs = -1;
    int hybridEventStartRing = 0;
    double hybridEventCorrelation = 0.0;
    std::int64_t hybridOnset1 = -1000000000LL;
    std::int64_t hybridOnset2 = -1000000000LL;
    std::int64_t hybridOnset3 = -1000000000LL;
    double hybridRefEnergySum = 0.0;
    double hybridDeltaEnergySum = 0.0;
    double hybridBurdenAccept = 1.0;
    double hybridBurdenAttack = 0.0;
    double hybridBurdenRelease = 0.0;
    double hybridLimiterRelease = 0.0;
    double hybridLimiterGain = 1.0;

    std::array<std::array<double, snubBufferLength>, 6> snubL{};
    std::array<std::array<double, snubBufferLength>, 6> snubR{};
    // Density Delta must compare two signals at the same point in time. The
    // six-band path can be delayed for the upper-band snubber lookahead, so
    // keep an equally delayed copy of the section input for the Delta null.
    std::array<double, snubBufferLength> densityRefBufL{};
    std::array<double, snubBufferLength> densityRefBufR{};
    int snubWrite = 0;

    std::array<double, masterBufferLength> masterBufL{};
    std::array<double, masterBufferLength> masterBufR{};
    int masterWrite = 0;

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
    double postAgcSmoothDriveAmt = 0.0;
    double postXt2SmoothDriveAmt = 0.0;
    double postAgcSmoothRecoveryGain = 1.0;
    double postXt2SmoothRecoveryGain = 1.0;
    double densityOutputGain = 1.0;
    double mbClipMix = 0.0;
    bool useCoreDensity = false;
    double mbShape = 0.0;
    double shapeHighAmount = 0.0;
    // Optional Crossover-Aware Shape calibration for coherent LR4/LR8.
    // Native mode, serial crossovers, and Classic Parallel remain untouched.
    std::array<double, 6> crossoverShapeBandGain{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double mbCompLimiter = 0.0;
    double mbCompKeep = 1.0;
    double mbLimiterForkCalibrationGain = 1.0;
    double densityProtectionDriveGain = 1.0;
    double mbPresencePost = 1.0;
    double mbBrilliancePost = 1.0;
    double presenceEdgeRecoveryGain = 1.0;
    double presenceBodyRecoveryGain = 1.0;
    double brillianceRecoveryGain = 1.0;
    std::array<double, 6> limiterWorkDb{};
    std::array<double, 6> limiterWork{};
    std::array<double, 6> limiterQuietThreshold{};
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
    double fmPreemphasisK = 0.0;
    bool fmDeemphasisAfter = false;

    // Storage bindings come from the same records as metadata and state export.
#define OP_PARAMETER(type, member, initial, ...) type member = initial;
#include "OptiLabParameters.def"
#undef OP_PARAMETER
#define OP_INTERNAL(type, member, initial, ...) type member = initial;
#include "OptiLabInternalState.def"
#undef OP_INTERNAL
    int clipperStyleBase = 0;
    bool clipperMsMode = false;

    OptiLabStereoEnhancer stereoEnhancer;

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
    bool useFoundationGuard = false;
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
    double mbEventAttack = 0.0;
    double mbEventMinimumRelease = 0.0;
    double mbEventMaximumRelease = 0.0;
    double shapeRecoveryAttackBlock = 0.0;
    double shapeRecoveryReleaseBlock = 0.0;
    double shapeServoLevelBlock = 0.0;
    double shapeServoGainBlock = 0.0;
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
    double adaptTopPresenceTargetCached = 1.0;
    double adaptTopAirTargetCached = 1.0;
    int adaptiveCpuCounter = 15;
    int shapeRecoveryCounter = 0;
    bool shapeRecoveryTick = false;
    int shapeBlockCounter = 0;
    std::array<double, 3> shapeCompRecoveryDb{};
    std::array<double, 3> shapeLimiterRecoveryDb{};
    std::array<double, 3> compRecoveryGain{1.0, 1.0, 1.0};
    std::array<double, 3> limiterRecoveryGain{1.0, 1.0, 1.0};
    std::array<double, 3> shapeCompRatioBlock{1.0, 1.0, 1.0};
    std::array<double, 3> shapeLimiterRatioBlock{1.0, 1.0, 1.0};
    std::array<double, 6> shapeLevel{};
    std::array<double, 6> shapeServoDb{};
    std::array<double, 6> shapeServoGain{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    std::array<double, 6> shapePowerAccum{};
    double currentAgcLowEffGain = 1.0;
    double currentAgcHighEffGain = 1.0;
    double currentDensityGain = 1.0;
    double currentFinalGain = 1.0;
    bool activityTracking = false;
    OutputTap outputTap = OutputTap::FullChain;
    int masterStartupArmed = 1;
    int masterStartupAge = 0;
    int masterStartupPrimeWindow = 0;
    double masterStartupActiveThresh = 0.0;
    double masterCatchGain = 1.0;

    OnePole hp30L, hp30R;
    std::array<OnePole, 3> subsonicExtraL, subsonicExtraR;
    std::array<Allpass1, 12> apL, apR;
    OnePole agcLpL, agcLpR;
    Biquad agcSteepLowAL, agcSteepLowAR, agcSteepLowBL, agcSteepLowBR;
    Biquad agcSteepHighAL, agcSteepHighAR, agcSteepHighBL, agcSteepHighBR;
    PeakEnv agcLowEnv, agcHighEnv;
    SmoothRounder postAgcRoundL, postAgcRoundR;
    PeakEnv masterRawEnv, masterAmpEnv, masterCatchEnv;
    Biquad bassPeakL, bassPeakR;
    FoundationGuard foundationGuard;
    OnePole adaptBassLpL, adaptBassLpR;
    PeakEnv adaptBassEnv, adaptProgEnv;
    OnePole bassclipSplitL, bassclipSplitR, bassclipHpfL, bassclipHpfR;
    BassClipper bassclipPreL, bassclipPreR;
    std::array<OnePole, 5> xbL, xbR;
    std::array<OnePole, 5> serialXbL, serialXbR;
    optilab_crossover::CoherentLr4SixBand<Biquad> densityLr4L, densityLr4R;
    optilab_crossover::CoherentLr8SixBand<Biquad> densityLr8L, densityLr8R;
    OnePole adaptTopAirHpL, adaptTopAirHpR;
    FmFilter fmPreL, fmPreR, fmDeemphL, fmDeemphR;
    PeakEnv adaptTopEdgeEnv, adaptTopEdgeProgEnv, adaptTopAirEnv, adaptTopAirProgEnv;
    std::array<GainCell, 6> lim;
    std::array<EventCell, 6> mbEvent;
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
    OnePole fcsResL, fcsResR;
};

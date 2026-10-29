#include "OptiLabCorePipeline.h"

#include <cstdlib>
#include <new>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#define private public
#include "OptiLabCore.h"
#undef private

static thread_local bool trackAllocations=false;
static thread_local std::size_t allocationCount=0;
void* operator new(std::size_t size) {
    if(trackAllocations) ++allocationCount;
    if(auto* p=std::malloc(size?size:1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFinite(const std::pair<float, float>& y, const char* message) {
    require(std::isfinite(y.first) && std::isfinite(y.second), message);
}



void testSmoothExtendedTrajectory() {
    using Values=OptiLabCoreEngine::ParameterValues;
    const Values low{
#include "fixtures/SmoothLow.inc"
    }, ninety{
#include "fixtures/SmoothNinety.inc"
    }, maximum{
#include "fixtures/SmoothMaximum.inc"
    };
    require(low.size()==82 && ninety.size()==82 && maximum.size()==82,"Incomplete Smooth endpoint fixture");
    OptiLabCore core;
    auto p=OptiLabCore::defaultParameters(OptiLabCore::Mode::SmoothLimiter);
    core.setParameters(p);core.prepare(48000);
    // Sample the entire previous traversal at its compressed slider positions.
    for(int original=0;original<=100;++original) {
        p.autoAdaptPct=original*0.9;core.setParameters(p);
        const auto actual=core.producer_.effectiveCoreValues();
        const double x=double(original)/100,weight=x*x*(3-2*x);
        for(const auto& [key,lv]:low) {
            bool discrete=OptiLabCoreEngine::isCoreInternalKey(key);
            bool integer=false;
            for(const auto& d:optilab_parameters::catalog) if(key==d.key) {
                discrete=d.integer || d.structural();integer=d.integer;
            }
            for(const auto& d:optilab_internal::catalog) if(key==d.key) integer=d.integer;
            double expected=discrete?(weight<0.72?lv:ninety.at(key)):lv+(ninety.at(key)-lv)*weight;
            if(integer) expected=std::round(expected);
            require(std::abs(actual.at(key)-expected)<1e-9,"Previous Smooth traversal changed below 90%");
        }
    }
    // Both directions, including fractional positions around the new handoff.
    for(double adapt:{90.,90.001,90.01,91.,94.5,95.,99.9,100.,99.9,95.,90.01,90.001,90.}) {
        p.autoAdaptPct=adapt;core.setParameters(p);
        const auto actual=core.producer_.effectiveCoreValues();
        const double x=(adapt-90)/10,weight=x*x*(3-2*x);
        for(const auto& [key,lv]:ninety) {
            const double expected=lv+(maximum.at(key)-lv)*weight;
            require(std::abs(actual.at(key)-expected)<1e-9,"Smooth extended endpoint or continuous handoff mismatch");
        }
        require(core.latencySamples()==1033,"Smooth extended lookahead changed fixed latency");
    }
}

void testStreamToneCapAndTiming() {
    OptiLabCore core;
    auto p = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
    core.setParameters(p); core.prepare(48000);
    const auto smooth = [](double x) { x=std::clamp(x,0.0,1.0);return x*x*(3-2*x); };
    for (int adapt=0;adapt<=100;++adapt) {
        p.autoAdaptPct=adapt;core.setParameters(p);
        const auto v=core.producer_.effectiveParameterValues();
        const double foundation=smooth(double(adapt)/70);
        const double clean=smooth((double(adapt)-70)/14);
        const double push=smooth((double(adapt)-84)/16);
        const double lateToneWithdraw=smooth((double(adapt)-84)/6);
        const double makeupPrep=smooth((double(adapt)-70)/8);
        const double pushMakeup=std::pow(push,1.60);
        double presence=adapt<=70 ? 2.3+(2.0-2.3)*foundation : 2+(0.96-2)*clean;
        double brilliance=adapt<=70 ? 3.2+(2.0-3.2)*foundation : 2+(3.12-2)*clean;
        if(adapt>84) { presence*=1-lateToneWithdraw; brilliance*=1-lateToneWithdraw; }
        const double threshold=adapt<=70 ? -0.8+(-1.4+0.8)*foundation : -1.4+(-3.62+1.4)*push;
        double output=-2.6;
        if(adapt<=70) output=0.0+(-2.6-0.0)*foundation;
        else if(adapt<=84) output=-2.6+(2.25+2.6)*makeupPrep;
        else output=2.25+(12.0-2.25)*pushMakeup;
        const double drive=adapt<=70 ? 3.5+(3.2-3.5)*foundation : 3.2+(-7.2-3.2)*push;
        require(std::abs(v.at("p042")-presence)<1e-10,"Stream presence choreography mismatch");
        require(std::abs(v.at("p043")-brilliance)<1e-10,"Stream brilliance choreography mismatch");
        double agc=adapt<=70 ? 56.0+(20.0-56.0)*foundation : 20.0+30.0*smooth((double(adapt)-70)/18);
        if(adapt<=10) agc=20.0+30.0*smooth(double(adapt)/10);
        else if(adapt<20) {
            const double join=56.0-36.0*smooth(20.0/70.0);
            agc=50.0+(join-50.0)*smooth((double(adapt)-10)/10);
        }
        require(std::abs(v.at("p009")-agc)<1e-10,"Stream AGC choreography changed");
        require(std::abs(v.at("p069")-threshold)<1e-10,"Stream Final threshold choreography mismatch");
        require(std::abs(v.at("p053")-output)<1e-10,"Stream Density output choreography mismatch");
        require(std::abs(v.at("p035")-drive)<1e-10,"Stream Density drive choreography mismatch");
        if(adapt>=70) {
            const double lateHandoff=adapt<=84?0.0:push;
            require(std::abs(v.at("p023")-(3.6+(3.0-3.6)*lateHandoff))<1e-10,
                    "Stream Bass EQ late-handoff choreography mismatch");
            require(std::abs(v.at("p029")-(50.0+(0.0-50.0)*lateHandoff))<1e-10,
                    "Stream low-linking late-handoff choreography mismatch");
            require(std::abs(v.at("p037")-(84.0+(100.0-84.0)*lateHandoff))<1e-10,
                    "Stream compressor-depth late-handoff choreography mismatch");
            require(std::abs(v.at("p038")-(50.0+(100.0-50.0)*lateHandoff))<1e-10,
                    "Stream limiter-depth late-handoff choreography mismatch");
        }
    }
    p.autoAdaptPct=100;core.setParameters(p);
    const auto v=core.producer_.effectiveParameterValues();
    require(v.at("p035")==-7.2 && v.at("p037")==100 && v.at("p038")==100,
            "Stream 100 controlled Density endpoint changed");
    require(std::abs(v.at("p041"))<1e-10 && std::abs(v.at("p048")+6)<1e-10 && std::abs(v.at("p052"))<1e-10,
            "Stream 100 broad Density nonlinear stages re-enabled");
    require(v.at("p053")==12 && v.at("p063")==0,
            "Stream 100 gain staging endpoint changed");
    require(v.at("p069")==-3.62 && v.at("p079")==0,
            "Stream 100 threshold/ceiling endpoint changed");
    require(std::abs(v.at("p042"))<1e-10 && std::abs(v.at("p043"))<1e-10,
            "Stream 100 Presence/Brilliance must be neutral during late Final push");

    p.autoAdaptPct=89; core.setParameters(p);
    const auto beforeLateSwitch=core.producer_.effectiveParameterValues();
    p.autoAdaptPct=90; core.setParameters(p);
    const auto atLateSwitch=core.producer_.effectiveParameterValues();
    require(std::abs(atLateSwitch.at("p042"))<1e-10 && std::abs(atLateSwitch.at("p043"))<1e-10,
            "Stream high-band sweetening must be neutral by the 90% Final handoff");

}


void testPodcastUpperTarget() {
    using Target=std::pair<const char*,double>;
    const Target lower[]{
#include "fixtures/PodcastLowerNumbered.inc"
    };
    const Target middle[]{
#include "fixtures/PodcastMiddleNumbered.inc"
    };
    const Target upper[]{
#include "fixtures/PodcastUpperNumbered.inc"
    };
    require(std::size(lower)==46 && std::size(middle)==46 && std::size(upper)==46,
            "Incomplete Podcast public processing endpoint");
    OptiLabCore core;
    auto p=OptiLabCore::defaultParameters(OptiLabCore::Mode::PodcastLeveler);
    p.inputDriveDb=4;core.setParameters(p);core.prepare(48000);
    for(int point=0;point<3;++point) {
        p.autoAdaptPct=point*50;core.setParameters(p);
        const auto values=core.producer_.effectiveParameterValues();
        const auto* expected=point==0?lower:point==1?middle:upper;
        for(std::size_t i=0;i<46;++i)
            require(std::abs(values.at(expected[i].first)-expected[i].second)<1e-10,
                    "Podcast endpoint differs from authored public processing value");
        require(values.at("p002")==4,"Podcast endpoint changed independent Input Drive");
    }
    for (int adapt = 0; adapt <= 100; ++adapt) {
        p.autoAdaptPct = adapt;
        core.setParameters(p);
        require(core.producer_.effectiveParameterValues().at("p054") == 100.0,
                "Podcast Stereo Width must remain at 100% across Auto-Adapt");
    }
}

void testPodcastSwitchBoundaries() {
    OptiLabCore core;auto p=OptiLabCore::defaultParameters(OptiLabCore::Mode::PodcastLeveler);
    core.setParameters(p);core.prepare(48000);
    for(double adapt:{0.,20.,32.,33.,49.999,50.,50.001,69.999,70.,70.001,89.999,90.,90.001,100.,90.001,90.,89.999,70.001,70.,69.999,50.,33.,32.,20.,0.}) {
        p.autoAdaptPct=adapt;core.setParameters(p);
        const auto v=core.producer_.effectiveParameterValues();
        require(v.at("p080")==double(adapt<90?0:1),"Podcast top-filter switch is stale");
        require(v.at("p009")==10 || adapt>20,"Podcast AGC amount rises before its authored stage");
        require(core.latencySamples()==301,"Podcast automation changed reported latency");
        for(int i=0;i<1024;++i)requireFinite(core.processSample(float(.2*std::sin(i*.13)),-.1f),"Podcast switch emitted nonfinite audio");
    }
}

void testBidirectionalParameterUpdates() {
    // Fresh instances define each target independently of automation history.
    std::array<std::array<OptiLabCoreEngine::ParameterValues, 101>, 3> targets;
    for (int mode = 0; mode < 3; ++mode)
        for (int adapt = 0; adapt <= 100; ++adapt) {
            OptiLabCore fresh;
            auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            p.autoAdaptPct = adapt; fresh.setParameters(p); fresh.prepare(48000);
            targets[mode][adapt] = fresh.producer_.effectiveParameterValues();
        }
    for (double rate : {8000., 48000., 192000., 768000.}) {
        OptiLabCore live;
        live.prepare(rate);
        for (int mode : {0, 1, 2, 0}) {
            auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            for (int direction : {1, -1})
                for (int n = 0; n <= 100; ++n) {
                    const int adapt = direction > 0 ? n : 100 - n;
                    p.autoAdaptPct = adapt; live.setParameters(p);
                    for (int i = 0; i < 128; ++i)
                        requireFinite(live.processSample(float(.2 * std::sin(i * .17)), -.13f),
                                      "Bidirectional automation emitted nonfinite audio");
                    const auto actual = live.producer_.effectiveParameterValues();
                    for (const auto& [key, expected] : targets[mode][adapt]) {
                        if (std::abs(actual.at(key) - expected) > 1.0e-10) {
                            std::cerr << "Stale live parameter mode=" << mode << " adapt=" << adapt
                                      << " direction=" << direction << " key=" << key << '\n';
                            require(false, "Live parameter depends on slider direction/history");
                        }
                    }
                    if (mode == 0) {
                        require(actual.at("p080") == 0.0 && actual.at("p061") == 0.0,
                                "Stream must not enable public high-frequency filtering controls at any Adapt setting");
                    }
                }
        }
    }
    std::cout << "Bidirectional parameter audit: public runtime values; all modes; 4 rates; PASS\n";
}

void testLiveUpdatesAllocateNothing() {
    for(int mode=0;mode<3;++mode) {
        OptiLabCore core;auto p=OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
        core.setParameters(p);core.prepare(48000);(void)core.processSample(.2f,-.1f);
        for(int adapt=0;adapt<=100;++adapt) {
            p.autoAdaptPct=adapt;p.inputDriveDb=double(adapt%31)-12;
            allocationCount=0;trackAllocations=true;
            core.setParameters(p);(void)core.processSample(.2f,-.1f);
            auto other=p;other.mode=static_cast<OptiLabCore::Mode>((mode+1)%3);
            core.setParameters(other);(void)core.processSample(.2f,-.1f);
            core.setParameters(p);(void)core.processSample(.2f,-.1f);
            trackAllocations=false;
            require(allocationCount==0,"C++ allocation during prepared parameter update or processing");
        }
    }
}

void checkCompensation(double rate,int mode,int adapt) {
    OptiLabCore core;auto p=OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
    p.autoAdaptPct=adapt;core.setParameters(p);core.prepare(rate);
    if(rate==48000) require(core.latencySamples()==std::size_t(mode==0?648:mode==1?301:1033),
                           "Fixed 48 kHz mode latency changed");
    OptiLabCorePipeline raw;raw.setParameters(core.producer_.parameters());raw.prepare(rate);raw.reset();
    require(raw.latencySamples()<=core.latencySamples(),"Internal latency exceeds reported target");
    const auto delay=core.compensationDelay_;
    require(raw.latencySamples()+delay==core.latencySamples(),"Compensation target mismatch");
    std::vector<std::pair<float,float>> reference(core.latencySamples()+2048);
    bool nonzero=false;
    for(std::size_t i=0;i<reference.size();++i) {
        const float x=float(.4*std::sin(i*2*3.141592653589793*997/rate));
        reference[i]=raw.processSample(x,-.7f*x);
        const auto actual=core.processSample(x,-.7f*x);
        const auto expected=i>=delay?reference[i-delay]:std::pair<float,float>{0.f,0.f};
        require(actual==expected,"Compensated Core differs from raw processor plus delay");
        requireFinite(actual,"Non-finite compensated output");nonzero|=actual.first!=0 || actual.second!=0;
    }
    require(nonzero,"Compensation test never emerged from latency");
    if(mode==1 && adapt==0) {
        const auto values=core.producer_.effectiveParameterValues();
        for(const auto& [key,value]:core.podcast_.low)
            if(key!="p001" && key!="p002" && values.contains(key))
                require(std::abs(values.at(key)-value)<1e-10,"Podcast low authored public state changed");
    }
}

void testLatencyCompensation() {
    for(int mode=0;mode<3;++mode) for(int adapt=0;adapt<=100;++adapt)
        checkCompensation(48000,mode,adapt);
    for(double rate:{8000.,11025.,44100.,96000.,192000.,384000.,768000.})
        for(int mode=0;mode<3;++mode) for(int adapt:{0,50,70,88,100})
            checkCompensation(rate,mode,adapt);
}

void testSupportedRateEdges() {
    require(OptiLabCoreEngine::isSampleRateSupported(8000.0), "8 kHz should be supported");
    require(OptiLabCoreEngine::isSampleRateSupported(768000.0), "768 kHz should be supported");
    require(!OptiLabCoreEngine::isSampleRateSupported(7999.0), "Sub-minimum rate accepted");
    require(!OptiLabCoreEngine::isSampleRateSupported(768001.0), "Above-maximum rate accepted");

    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 768000.0}) {
        OptiLabCore core;
        core.prepare(rate);
        for (int i = 0; i < 256; ++i) {
            const float x = static_cast<float>(0.25 * std::sin(2.0 * 3.141592653589793 * 431.0 * i / rate));
            requireFinite(core.processSample(x, -0.7f * x), "Non-finite output at supported rate");
        }
    }
}

void testEveryModeAndEndpoint() {
    constexpr double rate = 48000.0;
    const std::array<OptiLabCore::Mode, 3> modes{
        OptiLabCore::Mode::StreamPolish,
        OptiLabCore::Mode::PodcastLeveler,
        OptiLabCore::Mode::SmoothLimiter};

    for (auto mode : modes) {
        OptiLabCore core;
        auto p = OptiLabCore::defaultParameters(mode);
        core.setParameters(p);
        core.prepare(rate);
        const auto fixedLatency = core.latencySamples();
        require(fixedLatency > 0, "Mode latency unexpectedly zero");

        for (double adapt : {0.0, 1.0, 25.0, 50.0, 70.0, 95.0, 100.0}) {
            p.autoAdaptPct = adapt;
            core.setParameters(p);
            require(core.latencySamples() == fixedLatency, "Auto-Adapt changed fixed mode latency");
            for (int i = 0; i < 4096; ++i) {
                const float a = static_cast<float>(0.32 * std::sin(2.0 * 3.141592653589793 * 997.0 * i / rate));
                const float b = static_cast<float>(0.21 * std::sin(2.0 * 3.141592653589793 * 173.0 * i / rate));
                requireFinite(core.processSample(a + b, -0.63f * a + 0.41f * b), "Non-finite endpoint render");
            }
        }
    }
}

void testModeSwitchFlushesOldAudio() {
    for(int from=0;from<3;++from) for(int to=0;to<3;++to) for(int adapt:{0,100}) if(from!=to) {
        OptiLabCore core,fresh;
        auto before=OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(from));
        before.autoAdaptPct=100;core.setParameters(before);core.prepare(48000);
        for(int i=0;i<6000;++i) {
            const float x=float(0.6*std::sin(i*0.13)+0.1);
            (void)core.processSample(x,-0.7f*x);
        }
        auto after=OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(to));
        after.autoAdaptPct=adapt;core.setParameters(after);fresh.setParameters(after);fresh.prepare(48000);
        for(int i=0;i<6000;++i) {
            const float x=i<2000?0.f:float(0.25*std::sin(i*0.09));
            require(core.processSample(x,-0.7f*x)==fresh.processSample(x,-0.7f*x),
                    "Mode switch retained history or differs from fresh configured state");
        }
    }
}

void testResetAndInterleavedDeterminism() {
    OptiLabCore a;
    OptiLabCore b;
    OptiLabCore::Parameters p;
    p.mode = OptiLabCore::Mode::StreamPolish;
    p.inputDriveDb = 4.0;
    p.autoAdaptPct = 70.0;
    a.setParameters(p);
    b.setParameters(p);
    a.prepare(48000.0);
    b.prepare(48000.0);

    constexpr std::size_t frames = 2048;
    std::vector<float> interleaved(frames * 2);
    std::vector<float> left(frames), right(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        left[i] = static_cast<float>(0.2 * std::sin(2.0 * 3.141592653589793 * 331.0 * i / 48000.0));
        right[i] = static_cast<float>(0.17 * std::sin(2.0 * 3.141592653589793 * 719.0 * i / 48000.0));
        interleaved[2*i] = left[i];
        interleaved[2*i+1] = right[i];
    }

    a.processInterleaved(interleaved.data(), frames, 2);
    b.processPlanar(left.data(), right.data(), frames);
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::abs(interleaved[2*i] - left[i]) < 1.0e-7f, "Interleaved/planar left mismatch");
        require(std::abs(interleaved[2*i+1] - right[i]) < 1.0e-7f, "Interleaved/planar right mismatch");
    }

    a.reset();
    b.reset();
    for (int i = 0; i < 256; ++i) {
        const auto ya = a.processSample(0.1f, -0.1f);
        const auto yb = b.processSample(0.1f, -0.1f);
        require(std::abs(ya.first-yb.first) < 1.0e-7f && std::abs(ya.second-yb.second) < 1.0e-7f,
                "Reset did not restore deterministic state");
    }
}

void testSanitization() {
    OptiLabCore core;
    core.prepare(48000.0);
    OptiLabCore::Parameters p;
    p.mode = static_cast<OptiLabCore::Mode>(999);
    p.inputDriveDb = std::numeric_limits<double>::infinity();
    p.autoAdaptPct = -100.0;
    core.setParameters(p);
    const auto q = core.parameters();
    require(q.mode == OptiLabCore::Mode::StreamPolish, "Malformed mode not sanitized to Stream");
    require(q.inputDriveDb == 0.0, "Non-finite input not sanitized");
    require(q.autoAdaptPct == 0.0, "Auto-Adapt low clamp failed");
}
}

int main() {
    try {
        const auto& catalogue = optilab_parameters::catalog;
        require(catalogue.size() == 48, "Numbered parameter catalogue changed");
        for (const auto& item : catalogue) {
            require(item.key != nullptr && item.key[0] == 'p', "Public parameter catalogue contains internal tuning state");
            require(!OptiLabCoreEngine::isCoreInternalKey(item.key), "Internal tuning leaked into public parameter catalogue");
        }
        testSmoothExtendedTrajectory();
        testStreamToneCapAndTiming();
        testPodcastUpperTarget();
        testPodcastSwitchBoundaries();
        testBidirectionalParameterUpdates();
        testLiveUpdatesAllocateNothing();
        testLatencyCompensation();
        testSupportedRateEdges();
        testEveryModeAndEndpoint();
        testModeSwitchFlushesOldAudio();
        testResetAndInterleavedDeterminism();
        testSanitization();
        std::cout << "OptiLab Core behavior regression: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        trackAllocations=false;
        std::cerr << "OptiLab Core behavior regression: FAIL: " << e.what() << '\n';
        return 1;
    }
}

#include <ysfx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
// Numbered internal values are inspected only to test the coordinator mapping.
#define private public
#include "OptiLabCore.h"
#undef private

namespace {
using Fx = std::unique_ptr<ysfx_t, decltype(&ysfx_free)>;
using State = std::unique_ptr<ysfx_state_t, decltype(&ysfx_state_free)>;
constexpr double tolerance = 1.0e-6;
int scriptMode(OptiLabCore::Mode mode) {
    return mode == OptiLabCore::Mode::StreamPolish ? 1 :
           mode == OptiLabCore::Mode::PodcastLeveler ? 0 : 2;
}
struct Suite {
    std::string script;
    std::unique_ptr<ysfx_config_t, decltype(&ysfx_config_free)> config{ysfx_config_new(), ysfx_config_free};
    std::ofstream report;
    int cases = 0, failures = 0;
    explicit Suite(const char* path, const char* reportPath) : script(path), report(reportPath) {
        if (!config || !report) throw std::runtime_error("Cannot initialize JSFX regression suite");
        ysfx_set_log_reporter(config.get(), [](intptr_t, ysfx_log_level, const char* text) {
            std::cerr << text << '\n';
        });
        report.precision(17);
        report << "case\tmax_error\tnrmse\tnative_latency\tjsfx_latency\n";
    }
    Fx create(double rate) {
        Fx fx(ysfx_new(config.get()), ysfx_free);
        if (!fx || !ysfx_load_file(fx.get(), script.c_str(), 0) ||
            !ysfx_compile(fx.get(), ysfx_compile_no_gfx))
            throw std::runtime_error("JSFX load or compilation failed");
        ysfx_set_sample_rate(fx.get(), rate);
        ysfx_set_block_size(fx.get(), 64);
        ysfx_init(fx.get());
        return fx;
    }
    static void set(ysfx_t* fx, const OptiLabCore::Parameters& p) {
        ysfx_slider_set_value(fx, 0, scriptMode(p.mode));
        ysfx_slider_set_value(fx, 1, p.inputDriveDb);
        ysfx_slider_set_value(fx, 2, p.autoAdaptPct);
    }
    void compare(ysfx_t* fx, OptiLabCore& core, const std::string& label,
                 int frames, double rate = 48000, int origin = 0,
                 const std::string& signal = "mixed") {
        double maximum = 0, error = 0, energy = 0; int maximumFrame = -1; int maximumChannel = -1;
        for (int pos = 0; pos < frames; pos += 64) {
            const int count = std::min(64, frames - pos);
            float left[64]{}, right[64]{}, actualL[64]{}, actualR[64]{}, expectedL[64]{}, expectedR[64]{};
            for (int i = 0; i < count; ++i) {
                const int t = origin + pos + i;
                const double st = t * 48000.0 / rate;
                left[i] = float(.18 * std::sin(st * .13) + .12 * std::sin(st * .019) + (t % 997 == 0 ? .2 : 0));
                right[i] = float(.21 * std::sin(st * .071));
                if (signal == "bass") {
                    left[i] = float(.20 * std::sin(st * .007199483164476609) + .05 * std::sin(st * .02617993877991494) + .08 * std::sin(st * .1308996938995747));
                    right[i] = float(.19 * std::sin(st * .007199483164476609) + .07 * std::sin(st * .1308996938995747));
                } else if (signal == "bright") {
                    left[i] = float(.15 * std::sin(st * .1308996938995747) + .20 * std::sin(st * .916297857297023) + .1 * std::sin(st * 1.1780972450961724) + (t % 997 == 0 ? .6 : 0));
                    right[i] = -left[i] * .7f;
                } else if (signal == "silence") left[i] = right[i] = 0;
                const auto expected = core.processSample(left[i], right[i]);
                expectedL[i] = expected.first; expectedR[i] = expected.second;
            }
            const float* inputs[]{left, right}; float* outputs[]{actualL, actualR};
            ysfx_process_float(fx, inputs, outputs, 2, 2, count);
            for (int i = 0; i < count; ++i) {
                if (!std::isfinite(actualL[i]) || !std::isfinite(actualR[i]))
                    throw std::runtime_error("Nonfinite JSFX output: " + label);
                const double dl = double(actualL[i]) - expectedL[i], dr = double(actualR[i]) - expectedR[i];
                const double adl = std::abs(dl), adr = std::abs(dr);
                if (adl > maximum) { maximum = adl; maximumFrame = pos + i; maximumChannel = 0; }
                if (adr > maximum) { maximum = adr; maximumFrame = pos + i; maximumChannel = 1; }
                error += dl * dl + dr * dr;
                energy += double(expectedL[i]) * expectedL[i] + double(expectedR[i]) * expectedR[i];
            }
        }
        const double normalized = std::sqrt(error / std::max(energy, 1.0e-30));
        const auto delay = ysfx_get_pdc_delay(fx);
        report << label << '\t' << maximum << '\t' << normalized << '\t' << core.latencySamples() << '\t' << delay << '\n';
        report.flush(); ++cases;
        if (maximum > tolerance || normalized > tolerance || core.latencySamples() != delay) {
            ++failures;
            std::cerr << "FAIL " << label << ": max=" << maximum << ", normalized=" << normalized << ", frame=" << maximumFrame << ", channel=" << maximumChannel << '\n';
        }
    }
    void parameters() {
        auto fx = create(48000);
        OptiLabCore core; core.prepare(48000);
        int checked = 0;
        const std::vector<double> coordinatorAdapts = [] {
            std::vector<double> values;
            for (int adapt = 0; adapt <= 100; ++adapt) values.push_back(double(adapt));
            for (double adapt : {13.37, 73.25, 84.1, 89.99, 90.01, 94.5, 99.9}) values.push_back(adapt);
            return values;
        }();
        for (int mode = 0; mode < 3; ++mode) for (double adapt : coordinatorAdapts) {
            auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            p.autoAdaptPct = adapt; core.setParameters(p); set(fx.get(), p);
            float zeroL = 0, zeroR = 0, outL = 0, outR = 0;
            const float* inputs[]{&zeroL, &zeroR}; float* outputs[]{&outL, &outR};
            ysfx_process_float(fx.get(), inputs, outputs, 2, 2, 1);
            const auto values = core.producer_.effectiveCoreValues();
            if (values.size() != 84) throw std::runtime_error("Unexpected core value count");
            for (const auto& [key, expected] : values) {
                const auto* actual = ysfx_find_var(fx.get(), key.c_str());
                if (!actual || !std::isfinite(*actual) || std::abs(*actual - expected) > 1.0e-9)
                    throw std::runtime_error("Coordinator mismatch: mode " + std::to_string(mode) + ", adapt " + std::to_string(adapt) + ", " + key);
                ++checked;
            }
        }
        std::cout << "Coordinator: " << checked << " core runtime values checked across " << (3 * coordinatorAdapts.size()) << " states\n";
    }
    void cold(double rate, double drive, int mode, double adapt, int frames,
              const std::string& signal = "mixed") {
        auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
        p.autoAdaptPct = adapt; p.inputDriveDb += drive;
        auto fx = create(rate); set(fx.get(), p);
        OptiLabCore core; core.setParameters(p); core.prepare(rate);
        const std::string label = "cold/" + signal + "/" + std::to_string(int(rate)) + "/" + std::to_string(drive) + "/" + std::to_string(mode) + "/" + std::to_string(adapt);
        compare(fx.get(), core, label, frames, rate, 0, signal);
    }
    void bidirectional() {
        for (int mode = 0; mode < 3; ++mode) {
            auto fx = create(48000);
            OptiLabCore core;
            auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            core.setParameters(p); core.prepare(48000);
            int origin = 0;
            for (int direction : {1, -1}) for (int n = 0; n <= 100; ++n) {
                const int adapt = direction > 0 ? n : 100 - n;
                p.autoAdaptPct = adapt; core.setParameters(p); set(fx.get(), p);
                compare(fx.get(), core, "sweep/" + std::to_string(mode) + "/" +
                        std::to_string(direction) + "/" + std::to_string(adapt), 2048, 48000, origin);
                origin += 2048;
                const auto expected = core.producer_.effectiveCoreValues();
                for (const auto& [key, value] : expected) {
                    const auto actual = ysfx_find_var(fx.get(), key.c_str());
                    if (!actual || !std::isfinite(*actual) || std::abs(*actual - value) > 1.0e-9)
                        throw std::runtime_error("Live sweep numbered parameter mismatch: " + key);
                }
            }
        }
    }
    void rawUiChanges() {
        // @gfx writes slider aliases directly. Unlike the host setter API,
        // those writes do not automatically schedule @slider in the runtime.
        for (int mode = 0; mode < 3; ++mode) {
            auto fx = create(48000);
            auto p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            set(fx.get(), p);
            OptiLabCore core; core.setParameters(p); core.prepare(48000);
            compare(fx.get(), core, "raw-ui/warm/" + std::to_string(mode), 2048);
            auto* rawAdapt = ysfx_find_var(fx.get(), "slider3");
            auto* rawInput = ysfx_find_var(fx.get(), "slider2");
            auto* rawMode = ysfx_find_var(fx.get(), "slider1");
            if (!rawAdapt || !rawInput || !rawMode) throw std::runtime_error("Missing UI aliases");
            int origin = 2048;
            for (double adapt : {13.37, 50., 100., 3., 0.}) {
                p.autoAdaptPct = adapt; p.inputDriveDb = adapt == 50 ? 6 : 2.75;
                *rawAdapt = adapt; *rawInput = p.inputDriveDb;
                core.setParameters(p);
                compare(fx.get(), core, "raw-ui/adapt/" + std::to_string(mode) + "/" +
                        std::to_string(adapt), 8192, 48000, origin);
                origin += 8192;
                if (failures) throw std::runtime_error("Raw UI slider writes did not update audio");
            }
            p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>((mode + 1) % 3));
            *rawMode = scriptMode(p.mode);
            core.setParameters(p);
            compare(fx.get(), core, "raw-ui/mode/" + std::to_string(mode), 8192, 48000, origin);
            if (failures) throw std::runtime_error("Raw UI Mode write did not update audio");
        }
    }
    void states() {
        auto fx = create(48000);
        OptiLabCore core;
        auto p = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
        core.setParameters(p); core.prepare(48000); int origin = 0;
        auto run = [&](const std::string& label) {
            core.setParameters(p); set(fx.get(), p);
            compare(fx.get(), core, label, 8192, 48000, origin); origin += 8192;
        };
        for (double adapt : {0., 13.37, 50., 70., 88., 94.5, 100., 3., 0.}) {
            p.autoAdaptPct = adapt; run("adapt/" + std::to_string(adapt));
        }
        for (int mode : {1, 2, 0, 2, 1}) {
            p = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));
            p.autoAdaptPct = 73.25; run("mode/" + std::to_string(mode));
        }
        p=OptiLabCore::defaultParameters(OptiLabCore::Mode::PodcastLeveler);
        for(double adapt:{0.,20.,32.5,32.6,50.,69.99,70.,70.01,89.99,90.,90.01,100.,90.01,90.,89.99,70.01,70.,69.99,50.,20.,0.}) {
            p.autoAdaptPct=adapt;run("podcast-switch/"+std::to_string(adapt));
        }
        for (double drive : {-12., 0., 6., 18.}) { p.inputDriveDb = drive; run("drive/" + std::to_string(drive)); }
        p.inputDriveDb = 2.75; p.autoAdaptPct = 41.25; run("before-save");
        State state(ysfx_save_state(fx.get()), ysfx_state_free);
        if (!state) throw std::runtime_error("JSFX save failed");
        auto restored = create(48000);
        if (!ysfx_load_state(restored.get(), state.get())) throw std::runtime_error("JSFX restore failed");
        OptiLabCore fresh; fresh.setParameters(p); fresh.prepare(48000);
        compare(restored.get(), fresh, "saved-state", 8192);
        ysfx_init(fx.get()); core.reset(); compare(fx.get(), core, "reset", 8192);
    }
};
}
int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 4) throw std::runtime_error("Usage: jsfx-regression SCRIPT REPORT [--thorough]");
        const bool thorough = argc == 4 && std::string(argv[3]) == "--thorough";
        if (argc == 4 && !thorough) throw std::runtime_error("Unknown test option");
        Suite suite(argv[1], argv[2]); suite.parameters(); suite.rawUiChanges(); suite.states(); suite.bidirectional();
        const std::vector<int> rates = thorough ? std::vector<int>{8000,11025,22050,32000,44100,48000,96000,192000,768000} : std::vector<int>{48000};
        const std::vector<double> drives = thorough ? std::vector<double>{-24,0,12} : std::vector<double>{0,12};
        for (int rate : rates) for (double drive : drives) for (int mode = 0; mode < 3; ++mode)
            for (double adapt : {0.,50.,70.,88.,94.5,100.})
                suite.cold(rate, drive, mode, adapt, thorough ? std::max(8192, ((rate / 4 + 63) / 64) * 64) : 8192);
        if (thorough) for (const std::string signal : {"mixed","bass","bright","silence"})
            for (double drive : {0.,12.}) for (int mode = 0; mode < 3; ++mode)
                for (int adapt : {0,50,70,88,100}) suite.cold(48000,drive,mode,adapt,384000,signal);
        std::cout << suite.cases << " audio cases, " << suite.failures << " failed; absolute and normalized tolerance " << tolerance << '\n';
        return suite.failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}

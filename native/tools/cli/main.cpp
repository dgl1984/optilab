#include "OptiLabCore.h"
#include "WavFile.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  optilab-core-cli input.wav output.wav [--mode podcast|stream|limiter] [--input dB] [--adapt pct]\n";
}

OptiLabCore::Mode parseMode(const std::string& value) {
    if (value == "podcast" || value == "podcast-leveler") {
        return OptiLabCore::Mode::PodcastLeveler;
    }
    if (value == "stream" || value == "stream-polish") {
        return OptiLabCore::Mode::StreamPolish;
    }
    if (value == "limiter" || value == "smooth-limiter") {
        return OptiLabCore::Mode::SmoothLimiter;
    }
    throw std::runtime_error("Unknown mode: " + value);
}

}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return 2;
    }

    try {
        const std::string inputPath = argv[1];
        const std::string outputPath = argv[2];

        OptiLabCore::Parameters params;
        bool inputWasSet = false;

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--mode" && i + 1 < argc) {
                params.mode = parseMode(argv[++i]);
            } else if (arg == "--input" && i + 1 < argc) {
                params.inputDriveDb = std::stod(argv[++i]);
                inputWasSet = true;
            } else if (arg == "--adapt" && i + 1 < argc) {
                params.autoAdaptPct = std::stod(argv[++i]);
            } else {
                throw std::runtime_error("Unknown or incomplete argument: " + arg);
            }
        }

        if (!inputWasSet) {
            params.inputDriveDb = OptiLabCore::defaultParameters(params.mode).inputDriveDb;
        }

        WavData wav = readWavFile(inputPath);
        if (wav.channels < 1 || wav.channels > 2) {
            throw std::runtime_error("Only mono and stereo WAV files are supported.");
        }
        if (wav.samples.size() % wav.channels != 0) {
            throw std::runtime_error("Input WAV sample count is not divisible by channel count.");
        }

        OptiLabCore core;
        core.prepare(static_cast<double>(wav.sampleRate));
        core.setParameters(params);
        core.processInterleaved(wav.samples.data(), wav.samples.size() / wav.channels, wav.channels);
        writeWavFile(outputPath, wav);

        std::cout << "Processed " << (wav.samples.size() / wav.channels) << " frames at "
                  << wav.sampleRate << " Hz. Latency: " << core.latencySamples() << " samples.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

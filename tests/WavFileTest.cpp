#include "WavFile.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void writeU16(std::ofstream& out, std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
void writeU32(std::ofstream& out, std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

void writeExtensiblePcm16(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    const std::uint32_t dataSize = 8;
    const std::uint32_t fmtSize = 40;
    const std::uint32_t riffSize = 4 + 8 + fmtSize + 8 + dataSize;
    out.write("RIFF", 4); writeU32(out, riffSize); out.write("WAVE", 4);
    out.write("fmt ", 4); writeU32(out, fmtSize);
    writeU16(out, 0xfffe); writeU16(out, 2); writeU32(out, 48000); writeU32(out, 192000);
    writeU16(out, 4); writeU16(out, 16); writeU16(out, 22); writeU16(out, 16); writeU32(out, 3);
    const unsigned char pcmGuid[16] = {1,0,0,0, 0,0,16,0, 128,0,0,170,0,56,155,113};
    out.write(reinterpret_cast<const char*>(pcmGuid), 16);
    out.write("data", 4); writeU32(out, dataSize);
    const std::int16_t samples[4] = {-32768, -8192, 8192, 32767};
    out.write(reinterpret_cast<const char*>(samples), sizeof(samples));
}
}

int main() {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / "optilab-core-wav-test";
    const auto roundTrip = base.string() + "-roundtrip.wav";
    const auto truncated = base.string() + "-truncated.wav";
    const auto extensible = base.string() + "-extensible.wav";
    const auto boundary = base.string() + "-riff-boundary.wav";
    try {
        WavData original;
        original.sampleRate = 48000;
        original.channels = 2;
        original.samples = {-1.0f, 1.0f, -0.25f, 0.25f, 0.0f, 0.5f};
        writeWavFile(roundTrip, original);
        const auto loaded = readWavFile(roundTrip);
        require(loaded.sampleRate == original.sampleRate && loaded.channels == original.channels, "WAV metadata round-trip failed");
        require(loaded.samples == original.samples, "Float WAV sample round-trip failed");

        {
            std::ofstream out(truncated, std::ios::binary);
            out.write("RIFF", 4); writeU32(out, 1000); out.write("WAVE", 4);
            out.write("data", 4); writeU32(out, 0xffffffffu);
        }
        bool rejected = false;
        try { (void)readWavFile(truncated); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Malformed oversized WAV chunk was not rejected");

        for (const auto declaredSize : {std::uint32_t{0}, std::uint32_t{4},
                static_cast<std::uint32_t>(fs::file_size(roundTrip) - 16)}) {
            {
                std::ifstream in(roundTrip, std::ios::binary);
                std::ofstream out(boundary, std::ios::binary);
                out << in.rdbuf();
                out.seekp(4);
                writeU32(out, declaredSize);
            }
            bool boundaryRejected = false;
            try { (void)readWavFile(boundary); }
            catch (const std::runtime_error&) { boundaryRejected = true; }
            require(boundaryRejected, "WAV data outside its declared RIFF container was accepted");
        }

        writeExtensiblePcm16(extensible);
        const auto ext = readWavFile(extensible);
        require(ext.sampleRate == 48000 && ext.channels == 2 && ext.samples.size() == 4, "WAVE_FORMAT_EXTENSIBLE PCM read failed");
        require(std::abs(ext.samples.front() + 1.0f) < 1.0e-6f, "Extensible PCM sample decode failed");

        fs::remove(roundTrip); fs::remove(truncated); fs::remove(extensible); fs::remove(boundary);
        std::cout << "WAV public test: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        fs::remove(roundTrip); fs::remove(truncated); fs::remove(extensible); fs::remove(boundary);
        std::cerr << "WAV public test: FAIL: " << e.what() << '\n';
        return 1;
    }
}

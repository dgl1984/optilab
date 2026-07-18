#include "WavFile.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {

template <typename T>
T readLE(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) {
        throw std::runtime_error("Unexpected end of WAV file");
    }
    return value;
}

void writeBytes(std::ostream& out, const char* text, std::size_t size) {
    out.write(text, static_cast<std::streamsize>(size));
}

template <typename T>
void writeLE(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

std::int32_t readInt24(const unsigned char* p) {
    std::int32_t value = static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
    if (value & 0x800000) {
        value |= ~0xFFFFFF;
    }
    return value;
}

void writeInt24(std::ostream& out, std::int32_t value) {
    unsigned char bytes[3]{
        static_cast<unsigned char>(value & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), 3);
}

}

WavData readWavFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open input WAV: " + path);
    }

    char riff[4]{};
    in.read(riff, 4);
    const auto riffSize = readLE<std::uint32_t>(in);
    (void)riffSize;
    char wave[4]{};
    in.read(wave, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        throw std::runtime_error("Input is not a RIFF/WAVE file");
    }

    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<unsigned char> data;

    while (in) {
        char chunkId[4]{};
        in.read(chunkId, 4);
        if (!in) {
            break;
        }
        const auto chunkSize = readLE<std::uint32_t>(in);
        const std::streampos next = in.tellg() + static_cast<std::streamoff>(chunkSize + (chunkSize & 1));

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = readLE<std::uint16_t>(in);
            channels = readLE<std::uint16_t>(in);
            sampleRate = readLE<std::uint32_t>(in);
            (void)readLE<std::uint32_t>(in);
            (void)readLE<std::uint16_t>(in);
            bitsPerSample = readLE<std::uint16_t>(in);
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            data.resize(chunkSize);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        in.seekg(next);
    }

    if (channels == 0 || sampleRate == 0 || data.empty()) {
        throw std::runtime_error("WAV is missing fmt or data chunk");
    }

    WavData wav;
    wav.sampleRate = sampleRate;
    wav.channels = channels;

    if (audioFormat == 1 && bitsPerSample == 16) {
        const std::size_t count = data.size() / 2;
        wav.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            std::int16_t v{};
            std::memcpy(&v, &data[i * 2], 2);
            wav.samples[i] = static_cast<float>(v / 32768.0f);
        }
    } else if (audioFormat == 1 && bitsPerSample == 24) {
        const std::size_t count = data.size() / 3;
        wav.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            wav.samples[i] = static_cast<float>(readInt24(&data[i * 3]) / 8388608.0);
        }
    } else if (audioFormat == 3 && bitsPerSample == 32) {
        const std::size_t count = data.size() / 4;
        wav.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            float v{};
            std::memcpy(&v, &data[i * 4], 4);
            wav.samples[i] = v;
        }
    } else {
        throw std::runtime_error("Unsupported WAV format. Use PCM 16/24-bit or 32-bit float.");
    }

    return wav;
}

void writeWavFile(const std::string& path, const WavData& wav) {
    if (wav.channels == 0 || wav.sampleRate == 0) {
        throw std::runtime_error("Cannot write invalid WAV data");
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not open output WAV: " + path);
    }

    const std::uint16_t audioFormat = 3;
    const std::uint16_t bitsPerSample = 32;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(wav.channels * sizeof(float));
    const std::uint32_t byteRate = wav.sampleRate * blockAlign;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(wav.samples.size() * sizeof(float));
    const std::uint32_t riffSize = 4 + (8 + 16) + (8 + dataSize);

    writeBytes(out, "RIFF", 4);
    writeLE(out, riffSize);
    writeBytes(out, "WAVE", 4);
    writeBytes(out, "fmt ", 4);
    writeLE<std::uint32_t>(out, 16);
    writeLE(out, audioFormat);
    writeLE(out, wav.channels);
    writeLE(out, wav.sampleRate);
    writeLE(out, byteRate);
    writeLE(out, blockAlign);
    writeLE(out, bitsPerSample);
    writeBytes(out, "data", 4);
    writeLE(out, dataSize);
    out.write(reinterpret_cast<const char*>(wav.samples.data()), static_cast<std::streamsize>(dataSize));
}

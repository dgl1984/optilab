// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#include "WavFile.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

template <typename T>
T readLE(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) throw std::runtime_error("Unexpected end of WAV file");
    return value;
}

void readExact(std::istream& in, void* destination, std::size_t bytes) {
    if (bytes > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("WAV chunk is too large to read");
    in.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(bytes));
    if (!in) throw std::runtime_error("Truncated WAV chunk");
}

void writeBytes(std::ostream& out, const char* text, std::size_t size) {
    out.write(text, static_cast<std::streamsize>(size));
    if (!out) throw std::runtime_error("Failed while writing WAV file");
}

template <typename T>
void writeLE(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!out) throw std::runtime_error("Failed while writing WAV file");
}

std::int32_t readInt24(const unsigned char* p) {
    std::int32_t value = static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
    if (value & 0x800000) value |= ~0xFFFFFF;
    return value;
}

std::streamoff fileLength(std::ifstream& in) {
    const auto current = in.tellg();
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    in.seekg(current);
    if (end < 0) throw std::runtime_error("Could not determine WAV file size");
    return static_cast<std::streamoff>(end);
}

bool guidHasWaveSubtype(const std::array<unsigned char, 16>& guid, std::uint16_t tag) {
    static constexpr std::array<unsigned char, 12> tail{
        0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
    if (guid[0] != static_cast<unsigned char>(tag & 0xff)
        || guid[1] != static_cast<unsigned char>((tag >> 8) & 0xff)
        || guid[2] != 0 || guid[3] != 0) return false;
    return std::equal(tail.begin(), tail.end(), guid.begin() + 4);
}

} // namespace

WavData readWavFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open input WAV: " + path);

    const auto length = fileLength(in);
    if (length < 12) throw std::runtime_error("Input is too small to be a RIFF/WAVE file");

    char riff[4]{};
    readExact(in, riff, 4);
    const auto riffSize = readLE<std::uint32_t>(in);
    char wave[4]{};
    readExact(in, wave, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0)
        throw std::runtime_error("Input is not a RIFF/WAVE file");

    const std::uint64_t declaredEnd = static_cast<std::uint64_t>(riffSize) + 8u;
    if (declaredEnd < 12u)
        throw std::runtime_error("RIFF size is smaller than the WAVE header");
    if (declaredEnd > static_cast<std::uint64_t>(length))
        throw std::runtime_error("RIFF size exceeds the WAV file length");

    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint16_t validBitsPerSample = 0;
    std::vector<unsigned char> data;

    const auto riffEnd = static_cast<std::streamoff>(declaredEnd);
    while (in && static_cast<std::streamoff>(in.tellg()) + 8 <= riffEnd) {
        char chunkId[4]{};
        readExact(in, chunkId, 4);
        const auto chunkSize = readLE<std::uint32_t>(in);
        if (chunkSize == 0xffffffffu)
            throw std::runtime_error("Unsupported WAV chunk size");

        const auto payload = static_cast<std::streamoff>(in.tellg());
        const std::uint64_t paddedSize = static_cast<std::uint64_t>(chunkSize) + (chunkSize & 1u);
        const std::uint64_t nextUnsigned = static_cast<std::uint64_t>(payload) + paddedSize;
        if (nextUnsigned > declaredEnd)
            throw std::runtime_error("WAV chunk extends beyond the RIFF container");
        const auto next = static_cast<std::streamoff>(nextUnsigned);

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize < 16) throw std::runtime_error("WAV fmt chunk is too small");
            audioFormat = readLE<std::uint16_t>(in);
            channels = readLE<std::uint16_t>(in);
            sampleRate = readLE<std::uint32_t>(in);
            (void)readLE<std::uint32_t>(in);
            (void)readLE<std::uint16_t>(in);
            bitsPerSample = readLE<std::uint16_t>(in);
            validBitsPerSample = bitsPerSample;

            if (audioFormat == 0xfffe) {
                if (chunkSize < 40) throw std::runtime_error("WAVE_FORMAT_EXTENSIBLE fmt chunk is too small");
                const auto extraSize = readLE<std::uint16_t>(in);
                if (extraSize < 22) throw std::runtime_error("Invalid WAVE_FORMAT_EXTENSIBLE extension");
                validBitsPerSample = readLE<std::uint16_t>(in);
                (void)readLE<std::uint32_t>(in); // channel mask
                std::array<unsigned char, 16> subtype{};
                readExact(in, subtype.data(), subtype.size());
                if (guidHasWaveSubtype(subtype, 1)) audioFormat = 1;
                else if (guidHasWaveSubtype(subtype, 3)) audioFormat = 3;
                else throw std::runtime_error("Unsupported WAVE_FORMAT_EXTENSIBLE subtype");
            }
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            if (chunkSize > static_cast<std::uint32_t>(std::numeric_limits<std::size_t>::max()))
                throw std::runtime_error("WAV data chunk is too large");
            data.resize(static_cast<std::size_t>(chunkSize));
            if (!data.empty()) readExact(in, data.data(), data.size());
        }

        in.seekg(next, std::ios::beg);
        if (!in) throw std::runtime_error("Invalid WAV chunk offset");
    }

    if (channels == 0 || sampleRate == 0 || data.empty())
        throw std::runtime_error("WAV is missing fmt or data chunk");

    WavData wav;
    wav.sampleRate = sampleRate;
    wav.channels = channels;

    if (audioFormat == 1 && bitsPerSample == 16 && validBitsPerSample <= 16) {
        if (data.size() % 2 != 0) throw std::runtime_error("Misaligned 16-bit WAV data");
        const std::size_t count = data.size() / 2;
        wav.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            std::int16_t v{};
            std::memcpy(&v, &data[i * 2], 2);
            wav.samples[i] = static_cast<float>(v / 32768.0f);
        }
    } else if (audioFormat == 1 && bitsPerSample == 24 && validBitsPerSample <= 24) {
        if (data.size() % 3 != 0) throw std::runtime_error("Misaligned 24-bit WAV data");
        const std::size_t count = data.size() / 3;
        wav.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            wav.samples[i] = static_cast<float>(readInt24(&data[i * 3]) / 8388608.0);
    } else if (audioFormat == 3 && bitsPerSample == 32) {
        if (data.size() % 4 != 0) throw std::runtime_error("Misaligned float WAV data");
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
    if (wav.channels == 0 || wav.sampleRate == 0)
        throw std::runtime_error("Cannot write invalid WAV data");
    if (wav.channels > std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error("Too many WAV channels");

    const std::uint64_t dataSize64 = static_cast<std::uint64_t>(wav.samples.size()) * sizeof(float);
    const std::uint64_t riffSize64 = 4u + (8u + 16u) + (8u + dataSize64);
    if (dataSize64 > std::numeric_limits<std::uint32_t>::max()
        || riffSize64 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("WAV exceeds RIFF 32-bit size limits");

    const auto dataSize = static_cast<std::uint32_t>(dataSize64);
    const auto riffSize = static_cast<std::uint32_t>(riffSize64);
    const std::uint16_t audioFormat = 3;
    const std::uint16_t bitsPerSample = 32;
    const std::uint32_t blockAlign32 = static_cast<std::uint32_t>(wav.channels) * sizeof(float);
    if (blockAlign32 > std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error("WAV block alignment exceeds format limits");
    const auto blockAlign = static_cast<std::uint16_t>(blockAlign32);
    const std::uint64_t byteRate64 = static_cast<std::uint64_t>(wav.sampleRate) * blockAlign;
    if (byteRate64 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("WAV byte rate exceeds format limits");
    const auto byteRate = static_cast<std::uint32_t>(byteRate64);

    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Could not open output WAV: " + path);

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
    if (dataSize > 0) {
        out.write(reinterpret_cast<const char*>(wav.samples.data()), static_cast<std::streamsize>(dataSize));
        if (!out) throw std::runtime_error("Failed while writing WAV sample data");
    }
}

#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>

class WavReader {
public:
#pragma pack(push, 1)
    struct Header {
        char riff[4];          // "RIFF"
        uint32_t fileSize;
        char wave[4];          // "WAVE"
        char fmt[4];           // "fmt "
        uint32_t fmtSize;
        uint16_t audioFormat;  // 1 for PCM, 3 for IEEE float
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
    };
#pragma pack(pop)

    static std::vector<float> loadAndResample(const std::string& filepath, double targetSampleRate) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open WAV file: " + filepath);
        }

        Header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(Header));
        if (file.gcount() != sizeof(Header)) {
            throw std::runtime_error("Failed to read WAV header");
        }

        if (std::string_view(header.riff, 4) != "RIFF" || std::string_view(header.wave, 4) != "WAVE") {
            throw std::runtime_error("Not a valid WAVE file");
        }

        // Skip any extra format bytes if fmtSize is larger than 16
        if (header.fmtSize > 16) {
            file.seekg(static_cast<std::streamoff>(header.fmtSize - 16), std::ios::cur);
        }

        // Find data chunk
        char chunkId[4];
        uint32_t chunkSize = 0;
        bool foundData = false;
        while (file.read(chunkId, 4)) {
            file.read(reinterpret_cast<char*>(&chunkSize), 4);
            if (std::string_view(chunkId, 4) == "data") {
                foundData = true;
                break;
            }
            file.seekg(chunkSize, std::ios::cur);
        }

        if (!foundData) {
            throw std::runtime_error("Could not find data chunk in WAV file");
        }

        std::vector<char> rawData(chunkSize);
        file.read(rawData.data(), static_cast<std::streamsize>(chunkSize));
        if (static_cast<uint32_t>(file.gcount()) != chunkSize) {
            rawData.resize(static_cast<size_t>(file.gcount()));
        }

        std::vector<float> sourceSamples;
        const size_t bytesPerSample = header.bitsPerSample / 8;
        if (bytesPerSample == 0) {
            throw std::runtime_error("Invalid bits per sample in WAV file");
        }
        const size_t numSamples = rawData.size() / bytesPerSample;
        sourceSamples.reserve(numSamples / header.numChannels);

        if (header.audioFormat == 1) { // PCM
            if (header.bitsPerSample == 16) {
                const int16_t* ptr = reinterpret_cast<const int16_t*>(rawData.data());
                for (size_t i = 0; i < numSamples; i += header.numChannels) {
                    sourceSamples.push_back(static_cast<float>(ptr[i]) / 32768.0f);
                }
            } else if (header.bitsPerSample == 24) {
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(rawData.data());
                for (size_t i = 0; i < rawData.size(); i += bytesPerSample * header.numChannels) {
                    // Sign-extend 24-bit to 32-bit int
                    int32_t val = (ptr[i] << 8) | (ptr[i + 1] << 16) | (ptr[i + 2] << 24);
                    sourceSamples.push_back(static_cast<float>(val) / 2147483648.0f);
                }
            } else if (header.bitsPerSample == 32) {
                const int32_t* ptr = reinterpret_cast<const int32_t*>(rawData.data());
                for (size_t i = 0; i < numSamples; i += header.numChannels) {
                    sourceSamples.push_back(static_cast<float>(ptr[i]) / 2147483648.0f);
                }
            } else {
                throw std::runtime_error("Unsupported PCM bits per sample: " + std::to_string(header.bitsPerSample));
            }
        } else if (header.audioFormat == 3) { // IEEE Float
            if (header.bitsPerSample == 32) {
                const float* ptr = reinterpret_cast<const float*>(rawData.data());
                for (size_t i = 0; i < numSamples; i += header.numChannels) {
                    sourceSamples.push_back(ptr[i]);
                }
            } else {
                throw std::runtime_error("Unsupported Float bits per sample: " + std::to_string(header.bitsPerSample));
            }
        } else {
            throw std::runtime_error("Unsupported audio format: " + std::to_string(header.audioFormat));
        }

        // Resample if necessary
        if (static_cast<double>(header.sampleRate) == targetSampleRate || sourceSamples.empty()) {
            return sourceSamples;
        }

        double ratio = static_cast<double>(header.sampleRate) / targetSampleRate;
        size_t resampledSize = static_cast<size_t>(std::floor(sourceSamples.size() / ratio));
        std::vector<float> resampledSamples(resampledSize);

        for (size_t i = 0; i < resampledSize; ++i) {
            double srcIndex = i * ratio;
            size_t idx0 = static_cast<size_t>(srcIndex);
            size_t idx1 = std::min(idx0 + 1, sourceSamples.size() - 1);
            float frac = static_cast<float>(srcIndex - idx0);
            resampledSamples[i] = sourceSamples[idx0] * (1.0f - frac) + sourceSamples[idx1] * frac;
        }

        return resampledSamples;
    }
};

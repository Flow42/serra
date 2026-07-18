#include "guitarfx/DSPEngine.h"
#include <iostream>
#include <vector>
#include <chrono>

int main() {
    std::cout << "[==========] Running guitarfx DSP benchmark...\n";

    DSPEngine engine;
    double sampleRate = 48000.0;
    int blockSize = 64; // Benchmark standard low-latency block size
    engine.prepare(sampleRate, blockSize);

    // Make sure EQ and Noise Gate are enabled to measure actual processing overhead
    auto eq = engine.getProcessor("eq");
    if (eq) {
        eq->setBypass(false);
    }
    auto gate = engine.getProcessor("noise_gate");
    if (gate) {
        gate->setBypass(false);
    }

    std::vector<float> input(static_cast<size_t>(blockSize), 0.5f);
    std::vector<float> outputL(static_cast<size_t>(blockSize), 0.0f);
    std::vector<float> outputR(static_cast<size_t>(blockSize), 0.0f);

    const float* inPtrs[1] = { input.data() };
    float* outPtrs[2] = { outputL.data(), outputR.data() };

    constexpr int numIterations = 50000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numIterations; ++i) {
        engine.process(inPtrs, 1, outPtrs, 2, blockSize);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double totalTimeSec = std::chrono::duration<double>(end - start).count();
    double avgTimePerBlockMs = (totalTimeSec / numIterations) * 1000.0;
    double deadlineMs = (static_cast<double>(blockSize) / sampleRate) * 1000.0;
    double percentageOfDeadline = (avgTimePerBlockMs / deadlineMs) * 100.0;

    std::cout << "Block size: " << blockSize << " samples\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Real-time block deadline: " << deadlineMs << " ms\n";
    std::cout << "Average processing time per block: " << avgTimePerBlockMs << " ms\n";
    std::cout << "DSP CPU usage: " << percentageOfDeadline << "%\n";

    if (percentageOfDeadline < 100.0) {
        std::cout << "[       OK ] DSP engine is well within real-time budget.\n";
    } else {
        std::cout << "[  WARNING ] DSP engine exceeds real-time budget!\n";
    }

    return 0;
}

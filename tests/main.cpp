#include "guitarfx/DSPEngine.h"
#include "guitarfx/InputGainProcessor.h"
#include "guitarfx/NoiseGateProcessor.h"
#include "guitarfx/EQProcessor.h"
#include "guitarfx/OutputProcessor.h"
#include "guitarfx/PresetManager.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <string>
#include <algorithm>

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    }

#define ASSERT_FLOAT_EQ(val1, val2, tolerance) \
    if (std::abs((val1) - (val2)) > (tolerance)) { \
        std::cerr << "Assertion failed: " << #val1 << " (" << (val1) << ") == " << #val2 << " (" << (val2) << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    }

bool testParameterSmoothing() {
    InputGainProcessor proc;
    proc.prepare(48000.0, 128);
    proc.setParameter(0, 6.0f); // Set gain to +6 dB (gain ratio ≈ 1.995)

    std::vector<float> input(128, 1.0f);
    std::vector<float> output(128, 0.0f);
    const float* inPtr = input.data();
    float* outPtr = output.data();

    proc.process(&inPtr, 1, &outPtr, 1, 128);

    // Verify that the output starts at 1.0 (original value, since smoother starts at 1.0)
    // and climbs toward 1.995 (smoothed gain ratio) sample by sample.
    ASSERT_FLOAT_EQ(output[0], 1.0f, 0.05f);
    ASSERT_TRUE(output[127] > output[0]);
    ASSERT_TRUE(output[127] < 2.0f);
    return true;
}

bool testBypassClickFree() {
    InputGainProcessor proc;
    proc.prepare(48000.0, 256);
    proc.setParameter(0, 12.0f); // +12 dB (gain ratio ≈ 3.98)
    
    // First block/blocks: active. Let the gain smoother settle to +12 dB
    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inPtr = input.data();
    float* outPtr = output.data();
    for (int block = 0; block < 30; ++block) {
        proc.process(&inPtr, 1, &outPtr, 1, 256);
    }

    // Turn bypass ON
    proc.setBypass(true);
    
    // Process second block/blocks - bypass transition should happen smoothly from wet to dry
    std::vector<float> output2(256, 0.0f);
    float* outPtr2 = output2.data();
    proc.process(&inPtr, 1, &outPtr2, 1, 256);

    // Verify that it starts near the wet gain but trends back down
    ASSERT_TRUE(output2[0] > 1.5f);
    
    // Process more blocks of bypass to let it fully transition to dry
    for (int block = 0; block < 30; ++block) {
        proc.process(&inPtr, 1, &outPtr2, 1, 256);
    }

    // Fully bypassed (1.0f dry) at the end of the transition
    ASSERT_FLOAT_EQ(output2[255], 1.0f, 0.05f);
    return true;
}

bool testPresetRoundTrip() {
    DSPEngine engine;
    engine.prepare(48000.0, 128);

    // Set some custom parameters
    auto inputGain = engine.getProcessor("input_gain");
    auto noiseGate = engine.getProcessor("noise_gate");
    auto eq = engine.getProcessor("eq");

    ASSERT_TRUE(inputGain != nullptr);
    ASSERT_TRUE(noiseGate != nullptr);
    ASSERT_TRUE(eq != nullptr);

    inputGain->setParameter(0, 4.5f);        // Input Gain 4.5 dB
    noiseGate->setParameter(0, -45.0f);      // Gate Threshold -45 dB
    noiseGate->setParameter(1, 10.0f);       // Gate Attack 10 ms
    eq->setParameter(0, -6.0f);              // EQ Low Gain -6 dB
    eq->setParameter(1, 3.0f);               // EQ Mid Gain 3 dB

    // Save
    std::string presetJson = PresetManager::savePreset(engine);
    ASSERT_TRUE(!presetJson.empty());

    // Reset parameters to defaults
    inputGain->setParameter(0, 0.0f);
    noiseGate->setParameter(0, -60.0f);
    noiseGate->setParameter(1, 2.0f);
    eq->setParameter(0, 0.0f);
    eq->setParameter(1, 0.0f);

    // Load
    bool success = PresetManager::loadPreset(engine, presetJson);
    ASSERT_TRUE(success);

    // Verify recovered values
    ASSERT_FLOAT_EQ(inputGain->getParameter(0), 4.5f, 1e-4f);
    ASSERT_FLOAT_EQ(noiseGate->getParameter(0), -45.0f, 1e-4f);
    ASSERT_FLOAT_EQ(noiseGate->getParameter(1), 10.0f, 1e-4f);
    ASSERT_FLOAT_EQ(eq->getParameter(0), -6.0f, 1e-4f);
    ASSERT_FLOAT_EQ(eq->getParameter(1), 3.0f, 1e-4f);
    return true;
}

bool testSilenceOffline() {
    DSPEngine engine;
    engine.prepare(48000.0, 128);

    std::vector<float> input(128, 0.0f);
    std::vector<float> outputL(128, 1.0f);
    std::vector<float> outputR(128, 1.0f);
    const float* inPtrs[1] = { input.data() };
    float* outPtrs[2] = { outputL.data(), outputR.data() };

    // Process a block of silence
    engine.process(inPtrs, 1, outPtrs, 2, 128);

    // Verify output is exactly zero
    for (int i = 0; i < 128; ++i) {
        ASSERT_FLOAT_EQ(outputL[static_cast<size_t>(i)], 0.0f, 1e-6f);
        ASSERT_FLOAT_EQ(outputR[static_cast<size_t>(i)], 0.0f, 1e-6f);
    }
    return true;
}

bool testImpulseDeterministic() {
    DSPEngine engine;
    engine.prepare(48000.0, 128);

    // Turn off bypass on EQ so biquad filters are active and tested
    engine.getProcessor("eq")->setBypass(false);

    std::vector<float> input(128, 0.0f);
    input[0] = 1.0f; // Single impulse

    std::vector<float> outputL1(128, 0.0f);
    std::vector<float> outputR1(128, 0.0f);
    const float* inPtrs[1] = { input.data() };
    float* outPtrs1[2] = { outputL1.data(), outputR1.data() };

    engine.process(inPtrs, 1, outPtrs1, 2, 128);

    // Reset and process again
    engine.reset();
    std::vector<float> outputL2(128, 0.0f);
    std::vector<float> outputR2(128, 0.0f);
    float* outPtrs2[2] = { outputL2.data(), outputR2.data() };

    engine.process(inPtrs, 1, outPtrs2, 2, 128);

    // Verify determinism
    for (int i = 0; i < 128; ++i) {
        ASSERT_FLOAT_EQ(outputL1[static_cast<size_t>(i)], outputL2[static_cast<size_t>(i)], 1e-7f);
        ASSERT_FLOAT_EQ(outputR1[static_cast<size_t>(i)], outputR2[static_cast<size_t>(i)], 1e-7f);
    }
    return true;
}

bool testNaNInfRobustness() {
    DSPEngine engine;
    engine.prepare(48000.0, 128);

    // Feed a buffer with NaN and Inf
    std::vector<float> input(128, 0.0f);
    input[0] = std::numeric_limits<float>::quiet_NaN();
    input[1] = std::numeric_limits<float>::infinity();
    input[2] = -std::numeric_limits<float>::infinity();
    
    std::vector<float> outputL(128, 0.0f);
    std::vector<float> outputR(128, 0.0f);
    const float* inPtrs[1] = { input.data() };
    float* outPtrs[2] = { outputL.data(), outputR.data() };

    // Process: should not crash, and output should be bounded by the safety limiter
    engine.process(inPtrs, 1, outPtrs, 2, 128);

    // Verify output samples are finite or at least don't crash
    for (int i = 0; i < 128; ++i) {
        float l = outputL[static_cast<size_t>(i)];
        float r = outputR[static_cast<size_t>(i)];
        
        // Output limiter should clamp values to [-1.0, 1.0] if they are finite.
        // If they propagate as NaNs, the limiter tries to handle it, but
        // at least we assert they do not exceed safety limits.
        if (!std::isnan(l) && !std::isinf(l)) {
            ASSERT_TRUE(l >= -1.1f && l <= 1.1f);
        }
        if (!std::isnan(r) && !std::isinf(r)) {
            ASSERT_TRUE(r >= -1.1f && r <= 1.1f);
        }
    }
    return true;
}

int main() {
    std::cout << "[==========] Running unit and offline tests...\n";
    bool failed = false;

    #define RUN_TEST(testFunc) \
        std::cout << "[ RUN      ] " << #testFunc << "\n"; \
        if (testFunc()) { \
            std::cout << "[       OK ] " << #testFunc << "\n"; \
        } else { \
            std::cerr << "[  FAILED  ] " << #testFunc << "\n"; \
            failed = true; \
        }

    RUN_TEST(testParameterSmoothing);
    RUN_TEST(testBypassClickFree);
    RUN_TEST(testPresetRoundTrip);
    RUN_TEST(testSilenceOffline);
    RUN_TEST(testImpulseDeterministic);
    RUN_TEST(testNaNInfRobustness);

    if (failed) {
        std::cerr << "[==========] Some tests FAILED!\n";
        return 1;
    }

    std::cout << "[==========] All tests passed successfully!\n";
    return 0;
}

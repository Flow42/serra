#include "guitarfx/InputGainProcessor.h"
#include <cmath>
#include <algorithm>

InputGainProcessor::InputGainProcessor() {
    mGainSmoother.setCurrent(1.0f);
    mBypassSmoother.setCurrent(1.0f);
}

void InputGainProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mGainSmoother.reset(sampleRate, 0.05);  // 50ms smoothing
    mBypassSmoother.reset(sampleRate, 0.02); // 20ms bypass transition
}

void InputGainProcessor::process(const float* const* inputChannels, int numInputChannels,
                                 float* const* outputChannels, int numOutputChannels,
                                 int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* in = inputChannels[0];
    float* out = outputChannels[0];
    
    float targetGain = std::pow(10.0f, mGainDb / 20.0f);
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    float peak = 0.0f;
    float sumSq = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float x = in[i];
        
        float absX = std::abs(x);
        if (absX > peak) {
            peak = absX;
        }
        sumSq += x * x;

        float g = mGainSmoother.next(targetGain);
        float wet = x * g;

        float bp = mBypassSmoother.next(targetBypass);
        out[i] = x * (1.0f - bp) + wet * bp;
    }

    mPeakLevel.store(peak, std::memory_order_relaxed);
    mRMSLevel.store(std::sqrt(sumSq / static_cast<float>(numSamples)), std::memory_order_relaxed);
}

void InputGainProcessor::reset() {
    // No historic delay buffers to reset
}

void InputGainProcessor::setParameter(int index, float value) {
    if (index == 0) {
        mGainDb = std::clamp(value, -60.0f, 24.0f);
    }
}

float InputGainProcessor::getParameter(int index) const {
    if (index == 0) {
        return mGainDb;
    }
    return 0.0f;
}

std::string InputGainProcessor::getParameterName(int index) const {
    if (index == 0) {
        return "Gain";
    }
    return "";
}

int InputGainProcessor::getNumParameters() const {
    return 1;
}

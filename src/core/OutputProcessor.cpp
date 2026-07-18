#include "guitarfx/OutputProcessor.h"
#include <cmath>
#include <algorithm>

OutputProcessor::OutputProcessor() {
    mGainSmoother.setCurrent(1.0f);
    mBypassSmoother.setCurrent(1.0f);
}

void OutputProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mGainSmoother.reset(sampleRate, 0.05);
    mBypassSmoother.reset(sampleRate, 0.02);
    mLimiter.prepare(sampleRate);
}

void OutputProcessor::process(const float* const* inputChannels, int numInputChannels,
                               float* const* outputChannels, int numOutputChannels,
                               int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* inL = inputChannels[0];
    const float* inR = (numInputChannels >= 2 && inputChannels[1] != nullptr) ? inputChannels[1] : inputChannels[0];
    
    float* outL = outputChannels[0];
    float* outR = (numOutputChannels >= 2 && outputChannels[1] != nullptr) ? outputChannels[1] : outputChannels[0];

    float targetGain = std::pow(10.0f, mGainDb / 20.0f);
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    float peakL = 0.0f;
    float peakR = 0.0f;
    float sumSqL = 0.0f;
    float sumSqR = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float xl = inL[i];
        float xr = inR[i];

        float g = mGainSmoother.next(targetGain);
        float wetL = xl * g;
        float wetR = xr * g;

        // Apply safety limiter on wet path
        mLimiter.process(wetL, wetR);

        float bp = mBypassSmoother.next(targetBypass);
        outL[i] = xl * (1.0f - bp) + wetL * bp;
        outR[i] = xr * (1.0f - bp) + wetR * bp;

        float absL = std::abs(outL[i]);
        float absR = std::abs(outR[i]);
        if (absL > peakL) peakL = absL;
        if (absR > peakR) peakR = absR;
        sumSqL += outL[i] * outL[i];
        sumSqR += outR[i] * outR[i];
    }

    mLeftPeak.store(peakL, std::memory_order_relaxed);
    mRightPeak.store(peakR, std::memory_order_relaxed);
    mLeftRMS.store(std::sqrt(sumSqL / static_cast<float>(numSamples)), std::memory_order_relaxed);
    mRightRMS.store(std::sqrt(sumSqR / static_cast<float>(numSamples)), std::memory_order_relaxed);
}

void OutputProcessor::reset() {
    mLimiter.reset();
}

void OutputProcessor::setParameter(int index, float value) {
    if (index == 0) {
        mGainDb = std::clamp(value, -60.0f, 24.0f);
    }
}

float OutputProcessor::getParameter(int index) const {
    if (index == 0) {
        return mGainDb;
    }
    return 0.0f;
}

std::string OutputProcessor::getParameterName(int index) const {
    if (index == 0) {
        return "Gain";
    }
    return "";
}

int OutputProcessor::getNumParameters() const {
    return 1;
}

#include "guitarfx/EQProcessor.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

EQProcessor::EQProcessor() {
    mLowGainSmoother.setCurrent(0.0f);
    mMidGainSmoother.setCurrent(0.0f);
    mHighGainSmoother.setCurrent(0.0f);
    mBypassSmoother.setCurrent(1.0f);
}

void EQProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    
    mLowGainSmoother.reset(sampleRate, 0.05);
    mMidGainSmoother.reset(sampleRate, 0.05);
    mHighGainSmoother.reset(sampleRate, 0.05);
    mBypassSmoother.reset(sampleRate, 0.02);

    reset();
}

void EQProcessor::process(const float* const* inputChannels, int numInputChannels,
                           float* const* outputChannels, int numOutputChannels,
                           int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* in = inputChannels[0];
    float* out = outputChannels[0];
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    // Recalculate coefficients at block-rate using smoothed values
    float lowG = mLowGainSmoother.next(mLowGain);
    float midG = mMidGainSmoother.next(mMidGain);
    float highG = mHighGainSmoother.next(mHighGain);

    calculateLowShelfCoeffs(lowG, mLowFreq, mLowFilter);
    calculatePeakingCoeffs(midG, mMidFreq, 0.7071f, mMidFilter);
    calculateHighShelfCoeffs(highG, mHighFreq, mHighFilter);

    for (int i = 0; i < numSamples; ++i) {
        float x = in[i];

        // Process through low shelf -> peaking -> high shelf
        float wet = mLowFilter.processSample(x);
        wet = mMidFilter.processSample(wet);
        wet = mHighFilter.processSample(wet);

        float bp = mBypassSmoother.next(targetBypass);
        out[i] = x * (1.0f - bp) + wet * bp;
    }
}

void EQProcessor::reset() {
    mLowFilter.reset();
    mMidFilter.reset();
    mHighFilter.reset();
}

void EQProcessor::calculateLowShelfCoeffs(float gainDb, float freq, Biquad& bq) {
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * M_PI * freq / mSampleRate);
    float cosW = std::cos(w0);
    float sinW = std::sin(w0);
    float alpha = sinW / 2.0f * std::sqrt(2.0f); // Slope S = 1
    float sqrtA = std::sqrt(A);

    float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
    float a2 = (A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;

    bq.b0 = b0 / a0;
    bq.b1 = b1 / a0;
    bq.b2 = b2 / a0;
    bq.a1 = a1 / a0;
    bq.a2 = a2 / a0;
}

void EQProcessor::calculatePeakingCoeffs(float gainDb, float freq, float Q, Biquad& bq) {
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * M_PI * freq / mSampleRate);
    float cosW = std::cos(w0);
    float sinW = std::sin(w0);
    float alpha = sinW / (2.0f * Q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cosW;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cosW;
    float a2 = 1.0f - alpha / A;

    bq.b0 = b0 / a0;
    bq.b1 = b1 / a0;
    bq.b2 = b2 / a0;
    bq.a1 = a1 / a0;
    bq.a2 = a2 / a0;
}

void EQProcessor::calculateHighShelfCoeffs(float gainDb, float freq, Biquad& bq) {
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * M_PI * freq / mSampleRate);
    float cosW = std::cos(w0);
    float sinW = std::sin(w0);
    float alpha = sinW / 2.0f * std::sqrt(2.0f); // Slope S = 1
    float sqrtA = std::sqrt(A);

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
    float a2 = (A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;

    bq.b0 = b0 / a0;
    bq.b1 = b1 / a0;
    bq.b2 = b2 / a0;
    bq.a1 = a1 / a0;
    bq.a2 = a2 / a0;
}

void EQProcessor::setParameter(int index, float value) {
    switch (index) {
        case 0: mLowGain = std::clamp(value, -12.0f, 12.0f); break;
        case 1: mMidGain = std::clamp(value, -12.0f, 12.0f); break;
        case 2: mHighGain = std::clamp(value, -12.0f, 12.0f); break;
        case 3: mLowFreq = std::clamp(value, 20.0f, 500.0f); break;
        case 4: mMidFreq = std::clamp(value, 200.0f, 5000.0f); break;
        case 5: mHighFreq = std::clamp(value, 2000.0f, 20000.0f); break;
        default: break;
    }
}

float EQProcessor::getParameter(int index) const {
    switch (index) {
        case 0: return mLowGain;
        case 1: return mMidGain;
        case 2: return mHighGain;
        case 3: return mLowFreq;
        case 4: return mMidFreq;
        case 5: return mHighFreq;
        default: return 0.0f;
    }
}

std::string EQProcessor::getParameterName(int index) const {
    switch (index) {
        case 0: return "Low Gain";
        case 1: return "Mid Gain";
        case 2: return "High Gain";
        case 3: return "Low Freq";
        case 4: return "Mid Freq";
        case 5: return "High Freq";
        default: return "";
    }
}

int EQProcessor::getNumParameters() const {
    return 6;
}

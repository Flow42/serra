#include "guitarfx/NoiseGateProcessor.h"
#include <cmath>
#include <algorithm>

NoiseGateProcessor::NoiseGateProcessor() {
    mBypassSmoother.setCurrent(1.0f);
}

void NoiseGateProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mEnvelope = 0.0f;
    mGateGain = 1.0f;
    mHoldCounter = 0;
    mBypassSmoother.reset(sampleRate, 0.02); // 20ms bypass transition
    updateCoefficients();
}

void NoiseGateProcessor::process(const float* const* inputChannels, int numInputChannels,
                                 float* const* outputChannels, int numOutputChannels,
                                 int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* in = inputChannels[0];
    float* out = outputChannels[0];

    updateCoefficients(); // Simple, safe block-rate update

    float threshold = std::pow(10.0f, mThresholdDb / 20.0f);
    int holdSamples = static_cast<int>(mHoldMs * mSampleRate / 1000.0);
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        float x = in[i];
        float absX = std::abs(x);

        // Envelope follower (Instant attack, exponential decay)
        if (absX > mEnvelope) {
            mEnvelope = absX;
        } else {
            mEnvelope = absX + mEnvelopeReleaseCoef * (mEnvelope - absX);
        }

        // Gate state logic
        float targetGateGain = 1.0f;
        if (mEnvelope < threshold) {
            if (mHoldCounter > 0) {
                mHoldCounter--;
                targetGateGain = 1.0f;
            } else {
                targetGateGain = 0.0f;
            }
        } else {
            mHoldCounter = holdSamples;
            targetGateGain = 1.0f;
        }

        // Apply gain smoothing
        float coef = (targetGateGain > mGateGain) ? mGateAttackCoef : mGateReleaseCoef;
        mGateGain = mGateGain + coef * (targetGateGain - mGateGain);

        float wet = x * mGateGain;
        float bp = mBypassSmoother.next(targetBypass);
        out[i] = x * (1.0f - bp) + wet * bp;
    }
}

void NoiseGateProcessor::reset() {
    mEnvelope = 0.0f;
    mGateGain = 1.0f;
    mHoldCounter = 0;
}

void NoiseGateProcessor::updateCoefficients() {
    // Fixed 10ms envelope tracker release
    mEnvelopeReleaseCoef = std::exp(-1.0f / static_cast<float>(mSampleRate * 0.010));
    
    // Smooth gate transitions
    mGateAttackCoef = 1.0f - std::exp(-1.0f / static_cast<float>(mSampleRate * (mAttackMs / 1000.0f)));
    mGateReleaseCoef = 1.0f - std::exp(-1.0f / static_cast<float>(mSampleRate * (mReleaseMs / 1000.0f)));
}

void NoiseGateProcessor::setParameter(int index, float value) {
    switch (index) {
        case 0: mThresholdDb = std::clamp(value, -100.0f, 0.0f); break;
        case 1: mAttackMs = std::clamp(value, 0.1f, 100.0f); break;
        case 2: mReleaseMs = std::clamp(value, 10.0f, 1000.0f); break;
        case 3: mHoldMs = std::clamp(value, 0.0f, 500.0f); break;
        default: break;
    }
}

float NoiseGateProcessor::getParameter(int index) const {
    switch (index) {
        case 0: return mThresholdDb;
        case 1: return mAttackMs;
        case 2: return mReleaseMs;
        case 3: return mHoldMs;
        default: return 0.0f;
    }
}

std::string NoiseGateProcessor::getParameterName(int index) const {
    switch (index) {
        case 0: return "Threshold";
        case 1: return "Attack";
        case 2: return "Release";
        case 3: return "Hold";
        default: return "";
    }
}

int NoiseGateProcessor::getNumParameters() const {
    return 4;
}

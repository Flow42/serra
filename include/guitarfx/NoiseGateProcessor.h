#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include <atomic>

class NoiseGateProcessor : public Processor {
public:
    NoiseGateProcessor();
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(const float* const* inputChannels, int numInputChannels,
                 float* const* outputChannels, int numOutputChannels,
                 int numSamples) override;
    void reset() override;
    
    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    std::string getParameterName(int index) const override;
    int getNumParameters() const override;
    
    bool isBypassed() const override { return mBypassed; }
    void setBypass(bool bypassed) override { mBypassed = bypassed; }
    
    std::string getIdentifier() const override { return "noise_gate"; }
    std::string getName() const override { return "Noise Gate"; }
    
private:
    void updateCoefficients();

    std::atomic<bool> mBypassed{false};
    
    // Parameters
    float mThresholdDb = -60.0f;
    float mAttackMs = 2.0f;
    float mReleaseMs = 100.0f;
    float mHoldMs = 15.0f;
    
    double mSampleRate = 48000.0;
    
    // Internal state
    float mEnvelope = 0.0f;
    float mGateGain = 1.0f;
    int mHoldCounter = 0;
    
    // Filter coefficients
    float mEnvelopeReleaseCoef = 0.0f;
    float mGateAttackCoef = 0.0f;
    float mGateReleaseCoef = 0.0f;
    
    // Bypass smoothing
    ParameterSmoother mBypassSmoother;
};

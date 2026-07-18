#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include <atomic>

class EQProcessor : public Processor {
public:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float w1 = 0.0f, w2 = 0.0f;

        void reset() {
            w1 = 0.0f;
            w2 = 0.0f;
        }

        inline float processSample(float x) {
            float y = b0 * x + w1;
            w1 = b1 * x - a1 * y + w2;
            w2 = b2 * x - a2 * y;
            return y;
        }
    };

    EQProcessor();
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

    std::string getIdentifier() const override { return "eq"; }
    std::string getName() const override { return "3-Band EQ"; }

private:
    void calculateLowShelfCoeffs(float gainDb, float freq, Biquad& bq);
    void calculatePeakingCoeffs(float gainDb, float freq, float Q, Biquad& bq);
    void calculateHighShelfCoeffs(float gainDb, float freq, Biquad& bq);

    std::atomic<bool> mBypassed{false};

    // Smoothed Gain Parameters
    float mLowGain = 0.0f;
    float mMidGain = 0.0f;
    float mHighGain = 0.0f;
    
    // Fixed or adjustable frequency parameters
    float mLowFreq = 100.0f;
    float mMidFreq = 1000.0f;
    float mHighFreq = 4000.0f;

    double mSampleRate = 48000.0;

    // Filters
    Biquad mLowFilter;
    Biquad mMidFilter;
    Biquad mHighFilter;

    // Parameter smoothers for the gains
    ParameterSmoother mLowGainSmoother;
    ParameterSmoother mMidGainSmoother;
    ParameterSmoother mHighGainSmoother;
    ParameterSmoother mBypassSmoother;
};

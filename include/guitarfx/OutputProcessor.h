#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include <atomic>

class OutputProcessor : public Processor {
public:
    struct SafetyLimiter {
        float releaseCoef = 0.0f;
        float currentGain = 1.0f;

        void prepare(double sampleRate) {
            releaseCoef = std::exp(-1.0f / static_cast<float>(sampleRate * 0.050)); // 50ms release
            currentGain = 1.0f;
        }

        inline void process(float& left, float& right) {
            float peak = std::max(std::abs(left), std::abs(right));
            if (peak > 1.0f) {
                float targetGain = 1.0f / peak;
                if (targetGain < currentGain) {
                    currentGain = targetGain; // Instant attack
                }
            } else {
                currentGain = currentGain + (1.0f - currentGain) * (1.0f - releaseCoef);
            }
            left *= currentGain;
            right *= currentGain;
        }
        
        void reset() {
            currentGain = 1.0f;
        }
    };

    OutputProcessor();
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

    std::string getIdentifier() const override { return "output"; }
    std::string getName() const override { return "Output Gain, Meter & Limiter"; }

    // Lock-free meter reading
    float getLeftPeakLevel() const { return mLeftPeak.load(std::memory_order_relaxed); }
    float getRightPeakLevel() const { return mRightPeak.load(std::memory_order_relaxed); }
    float getLeftRMSLevel() const { return mLeftRMS.load(std::memory_order_relaxed); }
    float getRightRMSLevel() const { return mRightRMS.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> mBypassed{false};

    float mGainDb = 0.0f;
    ParameterSmoother mGainSmoother;
    ParameterSmoother mBypassSmoother;

    SafetyLimiter mLimiter;

    // Meter levels
    std::atomic<float> mLeftPeak{0.0f};
    std::atomic<float> mRightPeak{0.0f};
    std::atomic<float> mLeftRMS{0.0f};
    std::atomic<float> mRightRMS{0.0f};

    double mSampleRate = 48000.0;
};

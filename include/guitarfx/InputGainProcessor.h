#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include <atomic>

class InputGainProcessor : public Processor {
public:
    InputGainProcessor();
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
    
    std::string getIdentifier() const override { return "input_gain"; }
    std::string getName() const override { return "Input Gain & Meter"; }
    
    float getPeakLevel() const { return mPeakLevel.load(std::memory_order_relaxed); }
    float getRMSLevel() const { return mRMSLevel.load(std::memory_order_relaxed); }
    
private:
    std::atomic<bool> mBypassed{false};
    float mGainDb = 0.0f;
    ParameterSmoother mGainSmoother;
    
    std::atomic<float> mPeakLevel{0.0f};
    std::atomic<float> mRMSLevel{0.0f};
    double mSampleRate = 48000.0;
    
    // Bypass transition smoothing
    ParameterSmoother mBypassSmoother;
};

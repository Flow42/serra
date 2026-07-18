#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include "guitarfx/AsyncAssetLoader.h"
#include <atomic>
#include <memory>
#include <vector>
#include <string>

class CabinetProcessor : public Processor {
public:
    CabinetProcessor(std::shared_ptr<AsyncAssetLoader> loader);
    ~CabinetProcessor() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void process(const float* const* inputChannels, int numInputChannels,
                 float* const* outputChannels, int numOutputChannels,
                 int numSamples) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    std::string getParameterName(int index) const override;
    int getNumParameters() const override;

    void setSetting(std::string_view key, std::string_view value) override;
    std::string getSetting(std::string_view key) const override;

    bool isBypassed() const override { return mBypassed; }
    void setBypass(bool bypassed) override { mBypassed = bypassed; }

    std::string getIdentifier() const override { return "cabinet"; }
    std::string getName() const override { return "Cabinet IR Convolver"; }

private:
    std::shared_ptr<AsyncAssetLoader> mLoader;
    std::atomic<bool> mBypassed{false};

    // IR coefficients
    std::vector<float> mCurrentIR;
    std::vector<float> mNewIR;
    std::atomic<std::vector<float>*> mPendingIR{nullptr};

    std::string mIRPath;

    // Crossfading and circular buffer state
    int mCrossfadeDurationSamples = 512;
    int mCrossfadeCounter = 0;
    
    // Circular buffer for direct convolution
    static constexpr size_t MAX_IR_SIZE = 2048;
    std::vector<float> mBuffer;
    int mWriteIndex = 0;

    double mSampleRate = 48000.0;
    int mMaxBlockSize = 512;

    ParameterSmoother mBypassSmoother;
};

#pragma once
#include "guitarfx/Processor.h"
#include "guitarfx/ParameterSmoother.h"
#include "guitarfx/AsyncAssetLoader.h"
#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace nam {
    class DSP;
}

class NAMProcessor : public Processor {
public:
    NAMProcessor(std::shared_ptr<AsyncAssetLoader> loader);
    ~NAMProcessor() override;

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

    std::string getIdentifier() const override { return "nam"; }
    std::string getName() const override { return "Neural Amp Modeler"; }

private:
    std::shared_ptr<AsyncAssetLoader> mLoader;
    std::atomic<bool> mBypassed{false};

    std::unique_ptr<nam::DSP> mCurrentModel;
    std::unique_ptr<nam::DSP> mNewModel;
    std::atomic<nam::DSP*> mPendingModel{nullptr};

    std::string mModelPath;

    int mCrossfadeDurationSamples = 512;
    int mCrossfadeCounter = 0;

    std::vector<float> mCurrentModelBuffer;
    std::vector<float> mNewModelBuffer;

    double mSampleRate = 48000.0;
    int mMaxBlockSize = 512;

    ParameterSmoother mBypassSmoother;
};

#pragma once
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include "guitarfx/Processor.h"
#include "guitarfx/AsyncAssetLoader.h"

class DSPEngine {
public:
    DSPEngine();
    ~DSPEngine();

    void prepare(double sampleRate, int maxBlockSize);
    
    // Core processing loop
    void process(const float* const* inputChannels, int numInputChannels,
                 float* const* outputChannels, int numOutputChannels,
                 int numSamples);
                 
    void reset();

    // Reorders the mono chain processors based on a list of identifiers
    void reorderChain(const std::vector<std::string>& identifierOrder);

    // Retrieve a processor by its unique identifier (e.g. "noise_gate", "nam", "eq")
    std::shared_ptr<Processor> getProcessor(const std::string& identifier) const;

    // Accessors
    const std::vector<std::shared_ptr<Processor>>& getChain() const { return mChain; }
    const Processor& getOutputProcessor() const { return *mOutputProcessor; }
    Processor& getOutputProcessor() { return *mOutputProcessor; }
    
    AsyncAssetLoader& getLoader() { return *mLoader; }
    
    float getCPULoad() const { return mCPULoad.load(std::memory_order_relaxed); }
    void setCPULoad(float load) { mCPULoad.store(load, std::memory_order_relaxed); }
    
    int getXRuns() const { return mXRuns.load(std::memory_order_relaxed); }
    void setXRuns(int xruns) { mXRuns.store(xruns, std::memory_order_relaxed); }
    
    double getSampleRate() const { return mSampleRate; }
    int getMaxBlockSize() const { return mMaxBlockSize; }

private:
    double mSampleRate = 48000.0;
    int mMaxBlockSize = 512;

    std::shared_ptr<AsyncAssetLoader> mLoader;
    std::vector<std::shared_ptr<Processor>> mChain;
    std::shared_ptr<Processor> mOutputProcessor;

    // Real-time metrics
    std::atomic<float> mCPULoad{0.0f};
    std::atomic<int> mXRuns{0};

    // Preallocated temporary mono buffer for the mono signal chain
    std::vector<float> mMonoBuffer;
    float* mMonoBufferPtr = nullptr;
};

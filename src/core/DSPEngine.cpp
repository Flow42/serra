#include "guitarfx/DSPEngine.h"
#include "guitarfx/InputGainProcessor.h"
#include "guitarfx/NoiseGateProcessor.h"
#include "guitarfx/NAMProcessor.h"
#include "guitarfx/CabinetProcessor.h"
#include "guitarfx/EQProcessor.h"
#include "guitarfx/OutputProcessor.h"
#include <chrono>
#include <algorithm>
#include <xmmintrin.h>
#include <pmmintrin.h>

// Scoped denormal disabler for x86_64
struct ScopedDenormalDisabler {
    ScopedDenormalDisabler() {
        #if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
        mxcsr_save = _mm_getcsr();
        _mm_setcsr(mxcsr_save | 0x8040); // DAZ (Denormals Are Zero) | FTZ (Flush To Zero)
        #endif
    }
    ~ScopedDenormalDisabler() {
        #if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
        _mm_setcsr(mxcsr_save);
        #endif
    }
private:
    unsigned int mxcsr_save = 0;
};

DSPEngine::DSPEngine() {
    mLoader = std::make_shared<AsyncAssetLoader>();
    mLoader->start();

    mChain.push_back(std::make_shared<InputGainProcessor>());
    mChain.push_back(std::make_shared<NoiseGateProcessor>());
    mChain.push_back(std::make_shared<NAMProcessor>(mLoader));
    mChain.push_back(std::make_shared<CabinetProcessor>(mLoader));
    mChain.push_back(std::make_shared<EQProcessor>());

    mOutputProcessor = std::make_shared<OutputProcessor>();
}

DSPEngine::~DSPEngine() {
    if (mLoader) {
        mLoader->stop();
    }
}

void DSPEngine::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;

    mMonoBuffer.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    mMonoBufferPtr = mMonoBuffer.data();

    for (auto& proc : mChain) {
        proc->prepare(sampleRate, maxBlockSize);
    }
    mOutputProcessor->prepare(sampleRate, maxBlockSize);
}

void DSPEngine::process(const float* const* inputChannels, int numInputChannels,
                         float* const* outputChannels, int numOutputChannels,
                         int numSamples) {
    auto startTime = std::chrono::high_resolution_clock::now();

    ScopedDenormalDisabler denormalDisabler;

    // 1. Mono input copy
    if (numInputChannels > 0 && inputChannels[0] != nullptr) {
        std::copy(inputChannels[0], inputChannels[0] + numSamples, mMonoBuffer.begin());
    } else {
        std::fill(mMonoBuffer.begin(), mMonoBuffer.begin() + numSamples, 0.0f);
    }

    // 2. Process mono chain
    const float* constMonoInput = mMonoBufferPtr;
    float* monoOutput = mMonoBufferPtr;

    for (auto& proc : mChain) {
        proc->process(&constMonoInput, 1, &monoOutput, 1, numSamples);
    }

    // 3. Mono-to-Stereo split to output buffers
    if (numOutputChannels >= 2 && outputChannels[0] != nullptr && outputChannels[1] != nullptr) {
        std::copy(mMonoBuffer.begin(), mMonoBuffer.begin() + numSamples, outputChannels[0]);
        std::copy(mMonoBuffer.begin(), mMonoBuffer.begin() + numSamples, outputChannels[1]);
    } else if (numOutputChannels > 0 && outputChannels[0] != nullptr) {
        std::copy(mMonoBuffer.begin(), mMonoBuffer.begin() + numSamples, outputChannels[0]);
    }

    // 4. Stereo output processing (Output gain, meters, safety limiter)
    if (numOutputChannels >= 2 && outputChannels[0] != nullptr && outputChannels[1] != nullptr) {
        mOutputProcessor->process(outputChannels, 2, outputChannels, 2, numSamples);
    } else if (numOutputChannels > 0 && outputChannels[0] != nullptr) {
        mOutputProcessor->process(outputChannels, 1, outputChannels, 1, numSamples);
    }

    // 5. Statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    float elapsedSec = std::chrono::duration<float>(endTime - startTime).count();
    float deadlineSec = static_cast<float>(numSamples) / static_cast<float>(mSampleRate);
    float currentLoad = elapsedSec / deadlineSec;

    float load = mCPULoad.load(std::memory_order_relaxed);
    // Smooth CPU load over time
    load = load + 0.05f * (currentLoad - load);
    mCPULoad.store(load, std::memory_order_relaxed);
}

void DSPEngine::reset() {
    for (auto& proc : mChain) {
        proc->reset();
    }
    mOutputProcessor->reset();
}

void DSPEngine::reorderChain(const std::vector<std::string>& identifierOrder) {
    std::vector<std::shared_ptr<Processor>> newChain;
    for (const auto& id : identifierOrder) {
        auto proc = getProcessor(id);
        if (proc) {
            newChain.push_back(proc);
        }
    }
    
    // Add any processors that were missing in the order list (fallback)
    for (const auto& proc : mChain) {
        auto it = std::find_if(newChain.begin(), newChain.end(), [&](const auto& p) {
            return p->getIdentifier() == proc->getIdentifier();
        });
        if (it == newChain.end()) {
            newChain.push_back(proc);
        }
    }
    
    mChain = std::move(newChain);
}

std::shared_ptr<Processor> DSPEngine::getProcessor(const std::string& identifier) const {
    if (identifier == "output") {
        return mOutputProcessor;
    }
    for (auto& proc : mChain) {
        if (proc->getIdentifier() == identifier) {
            return proc;
        }
    }
    return nullptr;
}

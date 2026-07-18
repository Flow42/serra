#include "guitarfx/NAMProcessor.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

NAMProcessor::NAMProcessor(std::shared_ptr<AsyncAssetLoader> loader)
    : mLoader(loader) {
    mBypassSmoother.setCurrent(1.0f);
}

NAMProcessor::~NAMProcessor() {
    if (mLoader) {
        if (mCurrentModel) {
            mLoader->decommissionModel(mCurrentModel.release());
        }
        if (mNewModel) {
            mLoader->decommissionModel(mNewModel.release());
        }
        nam::DSP* pending = mPendingModel.exchange(nullptr);
        if (pending != nullptr) {
            mLoader->decommissionModel(pending);
        }
    }
}

void NAMProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;

    mCurrentModelBuffer.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    mNewModelBuffer.assign(static_cast<size_t>(maxBlockSize), 0.0f);

    mBypassSmoother.reset(sampleRate, 0.02); // 20ms bypass transition
}

void NAMProcessor::process(const float* const* inputChannels, int numInputChannels,
                           float* const* outputChannels, int numOutputChannels,
                           int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* in = inputChannels[0];
    float* out = outputChannels[0];
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    // Check for a new model loaded in the background
    nam::DSP* pending = mPendingModel.exchange(nullptr);
    if (pending != nullptr) {
        if (mCurrentModel == nullptr) {
            mCurrentModel = std::unique_ptr<nam::DSP>(pending);
            mCrossfadeCounter = 0;
        } else {
            mNewModel = std::unique_ptr<nam::DSP>(pending);
            mCrossfadeCounter = mCrossfadeDurationSamples;
        }
    }

    if (mCrossfadeCounter > 0 && mNewModel != nullptr && mCurrentModel != nullptr) {
        // Copy input to temporary buffers
        std::copy(in, in + numSamples, mCurrentModelBuffer.begin());
        std::copy(in, in + numSamples, mNewModelBuffer.begin());

        // Process through both models. NAM expects float** channel pointer arrays.
        float* currentInputChannels[1] = { mCurrentModelBuffer.data() };
        float* currentOutputChannels[1] = { mCurrentModelBuffer.data() };
        mCurrentModel->process(currentInputChannels, currentOutputChannels, numSamples);

        float* newInputChannels[1] = { mNewModelBuffer.data() };
        float* newOutputChannels[1] = { mNewModelBuffer.data() };
        mNewModel->process(newInputChannels, newOutputChannels, numSamples);

        for (int i = 0; i < numSamples; ++i) {
            float fadeIn = static_cast<float>(mCrossfadeDurationSamples - mCrossfadeCounter) / static_cast<float>(mCrossfadeDurationSamples);
            float fadeOut = 1.0f - fadeIn;
            float wet = mCurrentModelBuffer[static_cast<size_t>(i)] * fadeOut + mNewModelBuffer[static_cast<size_t>(i)] * fadeIn;
            
            float bp = mBypassSmoother.next(targetBypass);
            out[i] = in[i] * (1.0f - bp) + wet * bp;

            if (mCrossfadeCounter > 0) {
                mCrossfadeCounter--;
            }
        }

        if (mCrossfadeCounter == 0) {
            mLoader->decommissionModel(mCurrentModel.release());
            mCurrentModel = std::move(mNewModel);
        }
    } else if (mCurrentModel != nullptr) {
        // Normal in-place processing
        std::copy(in, in + numSamples, out);
        
        float* currentInputChannels[1] = { out };
        float* currentOutputChannels[1] = { out };
        mCurrentModel->process(currentInputChannels, currentOutputChannels, numSamples);

        for (int i = 0; i < numSamples; ++i) {
            float wet = out[i];
            float bp = mBypassSmoother.next(targetBypass);
            out[i] = in[i] * (1.0f - bp) + wet * bp;
        }
    } else {
        // Fallback bypass
        for (int i = 0; i < numSamples; ++i) {
            float bp = mBypassSmoother.next(targetBypass);
            out[i] = in[i];
        }
    }
}

void NAMProcessor::reset() {
    if (mCurrentModel) {
        mCurrentModel->Reset(mSampleRate, mMaxBlockSize);
    }
}

void NAMProcessor::setParameter(int index, float value) {
    (void)index;
    (void)value;
}

float NAMProcessor::getParameter(int index) const {
    (void)index;
    return 0.0f;
}

std::string NAMProcessor::getParameterName(int index) const {
    (void)index;
    return "";
}

int NAMProcessor::getNumParameters() const {
    return 0;
}

void NAMProcessor::setSetting(std::string_view key, std::string_view value) {
    if (key == "model_path") {
        mModelPath = std::string(value);
        if (mModelPath.empty()) {
            if (mCurrentModel) {
                mLoader->decommissionModel(mCurrentModel.release());
            }
            return;
        }
        
        // Trigger async load
        mLoader->requestModelLoad(mModelPath, mSampleRate, [this](std::unique_ptr<nam::DSP> model) {
            nam::DSP* expected = nullptr;
            nam::DSP* loaded = model.release();
            if (!mPendingModel.compare_exchange_strong(expected, loaded)) {
                delete loaded;
            }
        });
    }
}

std::string NAMProcessor::getSetting(std::string_view key) const {
    if (key == "model_path") {
        return mModelPath;
    }
    return "";
}

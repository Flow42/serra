#include "guitarfx/CabinetProcessor.h"
#include "guitarfx/WavReader.h"
#include <algorithm>
#include <cmath>

CabinetProcessor::CabinetProcessor(std::shared_ptr<AsyncAssetLoader> loader)
    : mLoader(loader) {
    mBypassSmoother.setCurrent(1.0f);
}

CabinetProcessor::~CabinetProcessor() {
    if (mLoader) {
        std::vector<float>* pending = mPendingIR.exchange(nullptr);
        if (pending != nullptr) {
            mLoader->decommissionIRCoefficients(pending);
        }
    }
}

void CabinetProcessor::prepare(double sampleRate, int maxBlockSize) {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;

    // Fixed buffer size of 2 * MAX_IR_SIZE for contiguous convolution
    mBuffer.assign(2 * MAX_IR_SIZE, 0.0f);
    mWriteIndex = static_cast<int>(MAX_IR_SIZE - 1);

    mBypassSmoother.reset(sampleRate, 0.02); // 20ms bypass transition
}

void CabinetProcessor::process(const float* const* inputChannels, int numInputChannels,
                               float* const* outputChannels, int numOutputChannels,
                               int numSamples) {
    if (numInputChannels == 0 || inputChannels[0] == nullptr ||
        numOutputChannels == 0 || outputChannels[0] == nullptr) {
        return;
    }

    const float* in = inputChannels[0];
    float* out = outputChannels[0];
    float targetBypass = mBypassed.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    // Check for a new IR loaded in the background
    std::vector<float>* pending = mPendingIR.exchange(nullptr);
    if (pending != nullptr) {
        // Limit IR size to MAX_IR_SIZE
        if (pending->size() > MAX_IR_SIZE) {
            pending->resize(MAX_IR_SIZE);
        }
        
        if (mCurrentIR.empty()) {
            mCurrentIR = std::move(*pending);
            mCrossfadeCounter = 0;
        } else {
            mNewIR = std::move(*pending);
            mCrossfadeCounter = mCrossfadeDurationSamples;
        }
        mLoader->decommissionIRCoefficients(pending);
    }

    const size_t M_curr = mCurrentIR.size();
    const size_t M_new = mNewIR.size();

    if (mCrossfadeCounter > 0 && M_curr > 0 && M_new > 0) {
        for (int i = 0; i < numSamples; ++i) {
            float x = in[i];

            // Write input sample to circular buffer
            mBuffer[static_cast<size_t>(mWriteIndex)] = x;
            mBuffer[static_cast<size_t>(mWriteIndex) + MAX_IR_SIZE] = x;

            // Convolution 1: Current IR
            float wetCurrent = 0.0f;
            const float* b = &mBuffer[static_cast<size_t>(mWriteIndex)];
            const float* h_curr = mCurrentIR.data();
            for (size_t k = 0; k < M_curr; ++k) {
                wetCurrent += b[k] * h_curr[k];
            }

            // Convolution 2: New IR
            float wetNew = 0.0f;
            const float* h_new = mNewIR.data();
            for (size_t k = 0; k < M_new; ++k) {
                wetNew += b[k] * h_new[k];
            }

            // Blend
            float fadeIn = static_cast<float>(mCrossfadeDurationSamples - mCrossfadeCounter) / static_cast<float>(mCrossfadeDurationSamples);
            float fadeOut = 1.0f - fadeIn;
            float wet = wetCurrent * fadeOut + wetNew * fadeIn;

            float bp = mBypassSmoother.next(targetBypass);
            out[i] = x * (1.0f - bp) + wet * bp;

            // Decrement circular buffer pointer
            mWriteIndex = (mWriteIndex == 0) ? static_cast<int>(MAX_IR_SIZE - 1) : (mWriteIndex - 1);

            if (mCrossfadeCounter > 0) {
                mCrossfadeCounter--;
            }
        }

        if (mCrossfadeCounter == 0) {
            mCurrentIR = std::move(mNewIR);
            mNewIR.clear();
        }
    } else if (M_curr > 0) {
        const float* h_curr = mCurrentIR.data();
        for (int i = 0; i < numSamples; ++i) {
            float x = in[i];

            mBuffer[static_cast<size_t>(mWriteIndex)] = x;
            mBuffer[static_cast<size_t>(mWriteIndex) + MAX_IR_SIZE] = x;

            float wet = 0.0f;
            const float* b = &mBuffer[static_cast<size_t>(mWriteIndex)];
            for (size_t k = 0; k < M_curr; ++k) {
                wet += b[k] * h_curr[k];
            }

            float bp = mBypassSmoother.next(targetBypass);
            out[i] = x * (1.0f - bp) + wet * bp;

            mWriteIndex = (mWriteIndex == 0) ? static_cast<int>(MAX_IR_SIZE - 1) : (mWriteIndex - 1);
        }
    } else {
        // Fallback bypass
        for (int i = 0; i < numSamples; ++i) {
            float bp = mBypassSmoother.next(targetBypass);
            out[i] = in[i];
        }
    }
}

void CabinetProcessor::reset() {
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
    mWriteIndex = static_cast<int>(MAX_IR_SIZE - 1);
}

void CabinetProcessor::setParameter(int index, float value) {
    (void)index;
    (void)value;
}

float CabinetProcessor::getParameter(int index) const {
    (void)index;
    return 0.0f;
}

std::string CabinetProcessor::getParameterName(int index) const {
    (void)index;
    return "";
}

int CabinetProcessor::getNumParameters() const {
    return 0;
}

void CabinetProcessor::setSetting(std::string_view key, std::string_view value) {
    if (key == "ir_path") {
        mIRPath = std::string(value);
        if (mIRPath.empty()) {
            mCurrentIR.clear();
            return;
        }

        mLoader->requestIRLoad(mIRPath, mSampleRate, [this](std::vector<float> ir) {
            std::vector<float>* expected = nullptr;
            std::vector<float>* loaded = new std::vector<float>(std::move(ir));
            if (!mPendingIR.compare_exchange_strong(expected, loaded)) {
                delete loaded;
            }
        });
    }
}

std::string CabinetProcessor::getSetting(std::string_view key) const {
    if (key == "ir_path") {
        return mIRPath;
    }
    return "";
}

#include "AudioEngine.h"
#include "guitarfx/InputGainProcessor.h"
#include "guitarfx/OutputProcessor.h"
#include <cmath>
#include <algorithm>

AudioEngine::AudioEngine() {
}

AudioEngine::~AudioEngine() {
    shutdown();
}

void AudioEngine::initialise() {
    // Request 1 input channel (mono guitar) and 2 output channels (stereo output)
    mDeviceManager.initialise(1, 2, nullptr, true, "", nullptr);
    mDeviceManager.addAudioCallback(this);
}

void AudioEngine::shutdown() {
    mDeviceManager.removeAudioCallback(this);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    if (device != nullptr) {
        mCurrentSampleRate = device->getCurrentSampleRate();
        mCurrentBlockSize = device->getCurrentBufferSizeSamples();

        mDspEngine.prepare(mCurrentSampleRate, mCurrentBlockSize);
    }
}

void AudioEngine::audioDeviceStopped() {
    mDspEngine.reset();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                  float* const* outputChannelData, int numOutputChannels,
                                                  int numSamples, const juce::AudioIODeviceCallbackContext& context) {
    (void)context;
    
    // We need at least one valid output channel to do anything.
    if (numOutputChannels == 0 || outputChannelData[0] == nullptr) {
        return;
    }

    // Determine the input source (fallback to silent zero buffer if no inputs available)
    const float* in = nullptr;
    if (numInputChannels > 0 && inputChannelData[0] != nullptr) {
        in = inputChannelData[0];
    }

    // Prepare inputs
    static thread_local std::vector<float> threadLocalSilence;
    if (threadLocalSilence.size() < static_cast<size_t>(numSamples)) {
        threadLocalSilence.assign(static_cast<size_t>(numSamples), 0.0f);
    }

    const float* finalInput = in ? in : threadLocalSilence.data();
    
    // Track input levels for the UI
    float peak = 0.0f;
    float sumSq = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float x = finalInput[i];
        float absX = std::abs(x);
        if (absX > peak) peak = absX;
        sumSq += x * x;
    }
    mInputPeak.store(peak, std::memory_order_relaxed);
    mInputRMS.store(std::sqrt(sumSq / static_cast<float>(numSamples)), std::memory_order_relaxed);

    // Prepare outputs (duplicate to stereo if we only have 1 physical output)
    float* outL = outputChannelData[0];
    float* outR = (numOutputChannels >= 2 && outputChannelData[1] != nullptr) ? outputChannelData[1] : outputChannelData[0];

    const float* inputPtrs[1] = { finalInput };
    float* outputPtrs[2] = { outL, outR };

    // Process DSP chain
    mDspEngine.process(inputPtrs, 1, outputPtrs, 2, numSamples);
}

float AudioEngine::getLeftOutputPeak() const {
    auto outProc = std::dynamic_pointer_cast<OutputProcessor>(mDspEngine.getProcessor("output"));
    return outProc ? outProc->getLeftPeakLevel() : 0.0f;
}

float AudioEngine::getRightOutputPeak() const {
    auto outProc = std::dynamic_pointer_cast<OutputProcessor>(mDspEngine.getProcessor("output"));
    return outProc ? outProc->getRightPeakLevel() : 0.0f;
}

float AudioEngine::getLeftOutputRMS() const {
    auto outProc = std::dynamic_pointer_cast<OutputProcessor>(mDspEngine.getProcessor("output"));
    return outProc ? outProc->getLeftRMSLevel() : 0.0f;
}

float AudioEngine::getRightOutputRMS() const {
    auto outProc = std::dynamic_pointer_cast<OutputProcessor>(mDspEngine.getProcessor("output"));
    return outProc ? outProc->getRightRMSLevel() : 0.0f;
}

float AudioEngine::getLeftInputPeak() const {
    return mInputPeak.load(std::memory_order_relaxed);
}

float AudioEngine::getLeftInputRMS() const {
    return mInputRMS.load(std::memory_order_relaxed);
}

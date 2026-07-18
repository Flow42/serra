#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "guitarfx/DSPEngine.h"

class AudioEngine : public juce::AudioIODeviceCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();

    // AudioIODeviceCallback Implementation
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                          float* const* outputChannelData, int numOutputChannels,
                                          int numSamples, const juce::AudioIODeviceCallbackContext& context) override;

    // Helpers to manage device types (JACK, ALSA, etc.)
    juce::AudioDeviceManager& getDeviceManager() { return mDeviceManager; }
    DSPEngine& getDSPEngine() { return mDspEngine; }

    double getCurrentSampleRate() const { return mCurrentSampleRate; }
    int getCurrentBlockSize() const { return mCurrentBlockSize; }

    // Level meter helpers
    float getLeftOutputPeak() const;
    float getRightOutputPeak() const;
    float getLeftOutputRMS() const;
    float getRightOutputRMS() const;

    float getLeftInputPeak() const;
    float getLeftInputRMS() const;

private:
    juce::AudioDeviceManager mDeviceManager;
    DSPEngine mDspEngine;

    double mCurrentSampleRate = 48000.0;
    int mCurrentBlockSize = 512;

    // Thread-safe meter tracking
    std::atomic<float> mInputPeak{0.0f};
    std::atomic<float> mInputRMS{0.0f};
};

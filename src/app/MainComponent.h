#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "LevelMeter.h"
#include "ProcessorEditor.h"
#include <memory>
#include <vector>

class MainComponent : public juce::Component, public juce::Timer, public juce::Button::Listener {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void buttonClicked(juce::Button* button) override;

private:
    void rebuildChainUI();
    void showAudioSettings();
    void loadPreset();
    void savePreset();

    AudioEngine mAudioEngine;

    // Viewport to hold the list of effects processors
    juce::Viewport mViewport;
    juce::Component mViewportContent;

    // List of active processor editors
    std::vector<std::unique_ptr<ProcessorEditor>> mProcessorEditors;

    // Header buttons
    juce::TextButton mAudioSettingsButton;
    juce::TextButton mLoadPresetButton;
    juce::TextButton mSavePresetButton;

    // VU Meters
    LevelMeter mInputMeter;
    LevelMeter mOutputMeter;
    juce::Label mInputMeterLabel;
    juce::Label mOutputMeterLabel;

    // CPU & Performance diagnostics labels
    juce::Label mCPULabel;
    juce::Label mXRunLabel;

    std::unique_ptr<juce::FileChooser> mFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

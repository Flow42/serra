#include "MainComponent.h"
#include "guitarfx/PresetManager.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <fstream>
#include <sstream>

MainComponent::MainComponent()
    : mInputMeter(false), mOutputMeter(true) {
    
    // Header Buttons
    mAudioSettingsButton.setButtonText("Audio Settings");
    mAudioSettingsButton.addListener(this);
    addAndMakeVisible(mAudioSettingsButton);

    mLoadPresetButton.setButtonText("Load Preset");
    mLoadPresetButton.addListener(this);
    addAndMakeVisible(mLoadPresetButton);

    mSavePresetButton.setButtonText("Save Preset");
    mSavePresetButton.addListener(this);
    addAndMakeVisible(mSavePresetButton);

    // Diagnostics Labels
    mCPULabel.setText("CPU: 0.0%", juce::dontSendNotification);
    mCPULabel.setFont(juce::Font(12.0f));
    mCPULabel.setColour(juce::Label::textColourId, juce::Colour(0xff9f9faf));
    addAndMakeVisible(mCPULabel);

    mXRunLabel.setText("XRuns: 0", juce::dontSendNotification);
    mXRunLabel.setFont(juce::Font(12.0f));
    mXRunLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9f9faf));
    addAndMakeVisible(mXRunLabel);

    // VU Meters
    mInputMeterLabel.setText("INPUT", juce::dontSendNotification);
    mInputMeterLabel.setFont(juce::Font(10.0f));
    mInputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mInputMeterLabel);

    mOutputMeterLabel.setText("OUTPUT", juce::dontSendNotification);
    mOutputMeterLabel.setFont(juce::Font(10.0f));
    mOutputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mOutputMeterLabel);

    addAndMakeVisible(mInputMeter);
    addAndMakeVisible(mOutputMeter);

    // Viewport for Scrollable chain list
    mViewport.setViewedComponent(&mViewportContent);
    mViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(mViewport);

    // Initialise audio engine and build default chain UI
    mAudioEngine.initialise();
    rebuildChainUI();

    setSize(720, 680);
    startTimerHz(30); // Meter and diagnostics tick
}

MainComponent::~MainComponent() {
    stopTimer();
    mAudioEngine.shutdown();
}

void MainComponent::paint(juce::Graphics& g) {
    // Elegant dark layout background
    g.fillAll(juce::Colour(0xff0e0e12));
    
    // Draw a subtle divider below the header
    g.setColour(juce::Colour(0xff202028));
    g.drawHorizontalLine(55, 0.0f, static_cast<float>(getWidth()));
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();
    
    // Layout top bar header
    auto header = bounds.removeFromTop(55).reduced(12, 10);
    mAudioSettingsButton.setBounds(header.removeFromLeft(120));
    header.removeFromLeft(12);
    mLoadPresetButton.setBounds(header.removeFromLeft(100));
    header.removeFromLeft(8);
    mSavePresetButton.setBounds(header.removeFromLeft(100));

    mXRunLabel.setBounds(header.removeFromRight(80));
    header.removeFromRight(8);
    mCPULabel.setBounds(header.removeFromRight(80));

    // Layout right panel: VU Meters
    auto rightPanel = bounds.removeFromRight(120).reduced(8, 12);
    
    mInputMeterLabel.setBounds(rightPanel.removeFromTop(15));
    mInputMeter.setBounds(rightPanel.removeFromTop(120));
    
    rightPanel.removeFromTop(15);
    mOutputMeterLabel.setBounds(rightPanel.removeFromTop(15));
    mOutputMeter.setBounds(rightPanel.removeFromTop(rightPanel.getHeight() - 10));

    // Layout main center panel: scrollable viewport
    mViewport.setBounds(bounds.reduced(12, 12));
    
    // Layout content width inside viewport
    mViewportContent.setBounds(0, 0, mViewport.getMaximumVisibleWidth(), mViewportContent.getHeight());
    for (size_t i = 0; i < mProcessorEditors.size(); ++i) {
        mProcessorEditors[i]->setBounds(8, static_cast<int>(i * 140) + 8, mViewportContent.getWidth() - 16, 130);
    }
}

void MainComponent::timerCallback() {
    // 1. Poll VU levels from AudioEngine
    mInputMeter.setLevels(mAudioEngine.getLeftInputPeak(), mAudioEngine.getLeftInputRMS());
    mOutputMeter.setLevels(mAudioEngine.getLeftOutputPeak(), mAudioEngine.getLeftOutputRMS(),
                            mAudioEngine.getRightOutputPeak(), mAudioEngine.getRightOutputRMS());

    // 2. Poll diagnostics
    float cpu = mAudioEngine.getDSPEngine().getCPULoad() * 100.0f;
    mCPULabel.setText("CPU: " + juce::String(cpu, 1) + "%", juce::dontSendNotification);

    int xruns = mAudioEngine.getDSPEngine().getXRuns();
    mXRunLabel.setText("XRuns: " + juce::String(xruns), juce::dontSendNotification);
}

void MainComponent::buttonClicked(juce::Button* button) {
    if (button == &mAudioSettingsButton) {
        showAudioSettings();
    } else if (button == &mLoadPresetButton) {
        loadPreset();
    } else if (button == &mSavePresetButton) {
        savePreset();
    }
}

void MainComponent::rebuildChainUI() {
    mProcessorEditors.clear();

    auto& engine = mAudioEngine.getDSPEngine();
    auto chain = engine.getChain();

    for (size_t i = 0; i < chain.size(); ++i) {
        size_t idx = i;
        auto editor = std::make_unique<ProcessorEditor>(chain[i],
            [this, idx]() {
                // Move up
                if (idx > 0) {
                    auto& dsp = mAudioEngine.getDSPEngine();
                    auto dspChain = dsp.getChain();
                    std::vector<std::string> order;
                    for (auto& proc : dspChain) {
                        order.push_back(proc->getIdentifier());
                    }
                    std::swap(order[idx], order[idx - 1]);
                    dsp.reorderChain(order);
                    rebuildChainUI();
                }
            },
            [this, idx]() {
                // Move down
                auto& dsp = mAudioEngine.getDSPEngine();
                auto dspChain = dsp.getChain();
                if (idx < dspChain.size() - 1) {
                    std::vector<std::string> order;
                    for (auto& proc : dspChain) {
                        order.push_back(proc->getIdentifier());
                    }
                    std::swap(order[idx], order[idx + 1]);
                    dsp.reorderChain(order);
                    rebuildChainUI();
                }
            }
        );

        mViewportContent.addAndMakeVisible(*editor);
        mProcessorEditors.push_back(std::move(editor));
    }

    // Set viewport content height dynamically based on number of processors
    int contentHeight = static_cast<int>(mProcessorEditors.size()) * 140 + 16;
    mViewportContent.setSize(mViewport.getMaximumVisibleWidth(), contentHeight);
    
    resized();
}

void MainComponent::showAudioSettings() {
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        mAudioEngine.getDeviceManager(),
        1, 2, // input channels range
        2, 2, // output channels range
        true, true, // show input/output channel checkboxes
        true, false // show midi select, show levels
    );
    selector->setSize(500, 450);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(selector.release());
    o.dialogTitle = "Audio Interface Settings";
    o.dialogBackgroundColour = juce::Colour(0xff22222a);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    
    // Launches a modal dialog window
    o.launchAsync();
}

void MainComponent::loadPreset() {
    mFileChooser = std::make_unique<juce::FileChooser>(
        "Load JSON Preset...",
        juce::File(),
        "*.json"
    );

    mFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file.existsAsFile()) {
                std::ifstream ifs(file.getFullPathName().toStdString());
                if (ifs.is_open()) {
                    std::stringstream ss;
                    ss << ifs.rdbuf();
                    bool success = PresetManager::loadPreset(mAudioEngine.getDSPEngine(), ss.str());
                    if (success) {
                        // Preset loaded successfully, rebuild list of editors in the correct order and refresh UI
                        rebuildChainUI();
                        for (auto& editor : mProcessorEditors) {
                            editor->refreshSettingsDisplay();
                        }
                    }
                }
            }
        });
}

void MainComponent::savePreset() {
    mFileChooser = std::make_unique<juce::FileChooser>(
        "Save JSON Preset...",
        juce::File(),
        "*.json"
    );

    mFileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (!file.isDirectory()) {
                std::string presetJson = PresetManager::savePreset(mAudioEngine.getDSPEngine());
                std::ofstream ofs(file.getFullPathName().toStdString());
                if (ofs.is_open()) {
                    ofs << presetJson;
                }
            }
        });
}

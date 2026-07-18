#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "guitarfx/Processor.h"
#include <memory>
#include <vector>

class ProcessorEditor : public juce::Component, public juce::Slider::Listener, public juce::Button::Listener {
public:
    ProcessorEditor(std::shared_ptr<Processor> processor, std::function<void()> onMoveUp, std::function<void()> onMoveDown)
        : mProcessor(processor), mOnMoveUp(onMoveUp), mOnMoveDown(onMoveDown) {
        
        // Bypass Toggle Button
        mBypassButton.setButtonText("Bypass");
        mBypassButton.setToggleState(mProcessor->isBypassed(), juce::dontSendNotification);
        mBypassButton.addListener(this);
        addAndMakeVisible(mBypassButton);

        // Reordering Buttons
        mUpButton.setButtonText("▲");
        mUpButton.addListener(this);
        addAndMakeVisible(mUpButton);

        mDownButton.setButtonText("▼");
        mDownButton.addListener(this);
        addAndMakeVisible(mDownButton);

        // Name Label
        mNameLabel.setText(mProcessor->getName(), juce::dontSendNotification);
        mNameLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        addAndMakeVisible(mNameLabel);

        // File browser setup for Cabinet and NAM
        std::string identifier = mProcessor->getIdentifier();
        mIsNam = (identifier == "nam");
        mIsCab = (identifier == "cabinet");

        if (mIsNam || mIsCab) {
            mFileButton.setButtonText(mIsNam ? "Load NAM..." : "Load IR...");
            mFileButton.addListener(this);
            addAndMakeVisible(mFileButton);

            mFileNameLabel.setFont(juce::Font(11.0f, juce::Font::italic));
            std::string currentPath = mProcessor->getSetting(mIsNam ? "model_path" : "ir_path");
            if (!currentPath.empty()) {
                mFileNameLabel.setText(juce::File(currentPath).getFileName(), juce::dontSendNotification);
            } else {
                mFileNameLabel.setText("No file loaded", juce::dontSendNotification);
            }
            addAndMakeVisible(mFileNameLabel);
        }

        // Dynamically build sliders for all parameters
        int numParams = mProcessor->getNumParameters();
        for (int i = 0; i < numParams; ++i) {
            auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            
            // Set slider ranges based on parameter indices (standard matching in our processors)
            if (identifier == "input_gain") {
                slider->setRange(-60.0, 24.0, 0.1);
            } else if (identifier == "noise_gate") {
                if (i == 0) slider->setRange(-100.0, 0.0, 0.5);      // Threshold
                else if (i == 1) slider->setRange(0.1, 100.0, 0.1);   // Attack
                else if (i == 2) slider->setRange(1.0, 1000.0, 1.0);  // Release
                else if (i == 3) slider->setRange(0.0, 500.0, 1.0);   // Hold
            } else if (identifier == "eq") {
                if (i < 3) slider->setRange(-12.0, 12.0, 0.1);        // Gains
                else if (i == 3) slider->setRange(20.0, 500.0, 1.0);  // Low Freq
                else if (i == 4) slider->setRange(200.0, 5000.0, 1.0);// Mid Freq
                else if (i == 5) slider->setRange(2000.0, 20000.0, 1.0);// High Freq
            } else if (identifier == "output") {
                slider->setRange(-60.0, 24.0, 0.1);
            }

            slider->setValue(mProcessor->getParameter(i), juce::dontSendNotification);
            slider->addListener(this);
            addAndMakeVisible(*slider);
            mSliders.push_back(std::move(slider));

            auto label = std::make_unique<juce::Label>();
            label->setText(mProcessor->getParameterName(i), juce::dontSendNotification);
            label->setFont(juce::Font(10.0f));
            label->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(*label);
            mSliderLabels.push_back(std::move(label));
        }

        updateBypassState();
    }

    ~ProcessorEditor() override {
        for (auto& s : mSliders) {
            s->removeListener(this);
        }
        mBypassButton.removeListener(this);
        mUpButton.removeListener(this);
        mDownButton.removeListener(this);
        if (mIsNam || mIsCab) {
            mFileButton.removeListener(this);
        }
    }

    void refreshSettingsDisplay() {
        // Refresh filename if loaded from preset
        if (mIsNam || mIsCab) {
            std::string currentPath = mProcessor->getSetting(mIsNam ? "model_path" : "ir_path");
            if (!currentPath.empty()) {
                mFileNameLabel.setText(juce::File(currentPath).getFileName(), juce::dontSendNotification);
            } else {
                mFileNameLabel.setText("No file loaded", juce::dontSendNotification);
            }
        }
        // Refresh slider values
        for (size_t i = 0; i < mSliders.size(); ++i) {
            mSliders[i]->setValue(mProcessor->getParameter(static_cast<int>(i)), juce::dontSendNotification);
        }
        // Refresh bypass
        mBypassButton.setToggleState(mProcessor->isBypassed(), juce::dontSendNotification);
        updateBypassState();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw a premium glassmorphic border and background
        g.setColour(juce::Colour(0x1a23232f));
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(mProcessor->isBypassed() ? juce::Colour(0x33ffffff) : juce::Colour(0xff00e1ff));
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
    }

    void resized() override {
        auto bounds = getLocalBounds();
        
        // Layout top bar (Name, Bypass, Reorder buttons)
        auto topBar = bounds.removeFromTop(40).reduced(8, 4);
        mNameLabel.setBounds(topBar.removeFromLeft(150));
        
        mDownButton.setBounds(topBar.removeFromRight(24));
        topBar.removeFromRight(4);
        mUpButton.setBounds(topBar.removeFromRight(24));
        topBar.removeFromRight(8);
        mBypassButton.setBounds(topBar.removeFromRight(70));

        // Layout file load bar if applicable
        if (mIsNam || mIsCab) {
            auto fileBar = bounds.removeFromTop(30).reduced(8, 2);
            mFileButton.setBounds(fileBar.removeFromLeft(100));
            fileBar.removeFromLeft(8);
            mFileNameLabel.setBounds(fileBar);
        }

        // Layout sliders
        int numSliders = static_cast<int>(mSliders.size());
        if (numSliders > 0) {
            int sliderWidth = bounds.getWidth() / numSliders;
            for (int i = 0; i < numSliders; ++i) {
                auto area = bounds.getX() + (i * sliderWidth);
                auto sliderArea = juce::Rectangle<int>(area, bounds.getY(), sliderWidth, bounds.getHeight() - 15);
                mSliders[static_cast<size_t>(i)]->setBounds(sliderArea.reduced(4));
                
                auto labelArea = juce::Rectangle<int>(area, bounds.getBottom() - 18, sliderWidth, 15);
                mSliderLabels[static_cast<size_t>(i)]->setBounds(labelArea);
            }
        }
    }

    void sliderValueChanged(juce::Slider* slider) override {
        for (size_t i = 0; i < mSliders.size(); ++i) {
            if (mSliders[i].get() == slider) {
                mProcessor->setParameter(static_cast<int>(i), static_cast<float>(slider->getValue()));
                break;
            }
        }
    }

    void buttonClicked(juce::Button* button) override {
        if (button == &mBypassButton) {
            mProcessor->setBypass(mBypassButton.getToggleState());
            updateBypassState();
        } else if (button == &mUpButton) {
            if (mOnMoveUp) mOnMoveUp();
        } else if (button == &mDownButton) {
            if (mOnMoveDown) mOnMoveDown();
        } else if (button == &mFileButton) {
            mFileChooser = std::make_unique<juce::FileChooser>(
                mIsNam ? "Select NAM Model File..." : "Select Cabinet IR WAV File...",
                juce::File(),
                mIsNam ? "*.nam" : "*.wav"
            );
            mFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& chooser) {
                    auto file = chooser.getResult();
                    if (file.existsAsFile()) {
                        mProcessor->setSetting(mIsNam ? "model_path" : "ir_path", file.getFullPathName().toStdString());
                        mFileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                    }
                });
        }
    }

private:
    void updateBypassState() {
        bool bypassed = mProcessor->isBypassed();
        float alpha = bypassed ? 0.4f : 1.0f;
        
        for (auto& s : mSliders) s->setAlpha(alpha);
        for (auto& l : mSliderLabels) l->setAlpha(alpha);
        if (mIsNam || mIsCab) {
            mFileButton.setAlpha(alpha);
            mFileNameLabel.setAlpha(alpha);
        }
        repaint();
    }

    std::shared_ptr<Processor> mProcessor;
    std::function<void()> mOnMoveUp;
    std::function<void()> mOnMoveDown;

    juce::ToggleButton mBypassButton;
    juce::TextButton mUpButton;
    juce::TextButton mDownButton;
    juce::Label mNameLabel;

    // File loading UI (if applicable)
    bool mIsNam = false;
    bool mIsCab = false;
    juce::TextButton mFileButton;
    juce::Label mFileNameLabel;
    std::unique_ptr<juce::FileChooser> mFileChooser;

    // Parameters Sliders
    std::vector<std::unique_ptr<juce::Slider>> mSliders;
    std::vector<std::unique_ptr<juce::Label>> mSliderLabels;
};

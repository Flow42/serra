#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

class LevelMeter : public juce::Component, public juce::Timer {
public:
    LevelMeter(bool stereo) : mStereo(stereo) {
        startTimerHz(30); // 30 FPS
    }

    ~LevelMeter() override {
        stopTimer();
    }

    void setLevels(float peakL, float rmsL, float peakR = 0.0f, float rmsR = 0.0f) {
        mPeakL = peakL;
        mRMSL = rmsL;
        mPeakR = peakR;
        mRMSR = rmsR;

        // Decay peak holds
        mPeakHoldL = std::max(peakL, mPeakHoldL - 0.05f);
        mPeakHoldR = std::max(peakR, mPeakHoldR - 0.05f);
    }

    void timerCallback() override {
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        // Dark metallic background
        g.fillAll(juce::Colour(0xff15151c));
        
        // Draw grid lines and dB text
        g.setColour(juce::Colour(0xff3a3a4a));
        g.setFont(10.0f);
        const float dbLevels[] = { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -30.0f, -48.0f };
        for (float db : dbLevels) {
            float y = dbToY(db, bounds.getHeight());
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, bounds.getWidth());
            g.drawText(std::to_string(static_cast<int>(db)) + " dB", 
                       bounds.getWidth() - 32.0f, y - 6.0f, 30.0f, 12.0f, 
                       juce::Justification::right);
        }

        // Draw meter bars
        float meterWidth = mStereo ? (bounds.getWidth() - 40.0f) / 2.0f : bounds.getWidth() - 40.0f;
        meterWidth = std::max(4.0f, meterWidth);

        drawChannel(g, mPeakL, mRMSL, mPeakHoldL, bounds.getX() + 5.0f, bounds.getHeight(), meterWidth);
        if (mStereo) {
            drawChannel(g, mPeakR, mRMSR, mPeakHoldR, bounds.getX() + 10.0f + meterWidth, bounds.getHeight(), meterWidth);
        }
    }

private:
    float dbToY(float db, float height) const {
        // Map -60dB -> 0dB to height -> 0
        float val = (db + 60.0f) / 60.0f;
        val = std::clamp(val, 0.0f, 1.0f);
        return height * (1.0f - val);
    }

    float valueToY(float linearVal, float height) const {
        float db = juce::Decibels::gainToDecibels(linearVal, -60.0f);
        return dbToY(db, height);
    }

    void drawChannel(juce::Graphics& g, float peak, float rms, float peakHold, float x, float height, float width) {
        float yRMS = valueToY(rms, height);
        float yPeak = valueToY(peak, height);
        float yHold = valueToY(peakHold, height);

        // Gradient color for level meter
        juce::ColourGradient grad(juce::Colour(0xff00ff66), x, height, juce::Colour(0xffff3300), x, 0.0f, false);
        grad.addColour(0.7, juce::Colour(0xffffff00)); // Yellow warning section

        // Draw solid RMS bar
        g.setGradientFill(grad);
        g.fillRect(x, yRMS, width, height - yRMS);

        // Draw thin Peak bar (semi-transparent)
        g.setColour(juce::Colour(0x55ffffff));
        g.fillRect(x, yPeak, width, yRMS - yPeak);

        // Draw Peak Hold line (thin line that stays on screen longer)
        g.setColour(juce::Colours::white);
        g.fillRect(x, yHold, width, 1.5f);
    }

    bool mStereo;
    float mPeakL = 0.0f;
    float mRMSL = 0.0f;
    float mPeakR = 0.0f;
    float mRMSR = 0.0f;
    
    float mPeakHoldL = 0.0f;
    float mPeakHoldR = 0.0f;
};

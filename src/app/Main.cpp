#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class GuitarFXApplication : public juce::JUCEApplication {
public:
    GuitarFXApplication() {}

    const juce::String getApplicationName() override       { return "GuitarFX"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override {
        (void)commandLine;
        mMainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        mMainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override {
        (void)commandLine;
    }

    // Custom Document Window holding our MainComponent
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
           #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
           #endif

            setVisible(true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mMainWindow;
};

// Main entry point for the desktop GUI app
START_JUCE_APPLICATION(GuitarFXApplication)

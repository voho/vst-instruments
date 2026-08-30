#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

class ElectryLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ElectryLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isHighlighted, bool isDown) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

// JUCE's text button activates on Return but not Space, and its radio buttons
// do not navigate with arrows. Electry advertises these controls as keyboard-
// focusable, so add those conventional keyboard paths without duplicating the
// buttons' existing click actions.
class ElectryTextButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    std::function<bool (const juce::KeyPress&)> onNavigation;
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
    bool keyPressed (const juce::KeyPress&) override;
};

// A guitar-oriented keyboard for keyswitches and the pitched playable range.
// Pick style stays latched; the play-style highlight follows either its latch
// or the active HOLD override.
class ElectryKeyboardComponent final : public juce::MidiKeyboardComponent
{
public:
    explicit ElectryKeyboardComponent (juce::MidiKeyboardState&);

    void setSelectedKeyswitches (int pickIndex, int styleIndex);

    void drawWhiteNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    juce::String getWhiteNoteText (int midiNoteNumber) override;

private:
    bool isKeyswitchSelected (int keyswitchIndex) const noexcept;

    int selectedPickIndex = 0;
    int selectedStyleIndex = 0;
};

// A titled row of exclusive buttons, used for the pickup selector and the
// play-style (keyswitch) strip.
class ElectryChoiceStrip final : public juce::Component
{
public:
    ElectryChoiceStrip (juce::String title, juce::StringArray choices,
                        int maximumColumns = 8,
                        juce::String accessibilityTitle = {});

    std::function<void (int)> onChoice;
    void setSelectedIndex (int newIndex);
    void setTooltipText (const juce::String& text);
    int getSelectedIndex() const noexcept { return selectedIndex; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void activateChoice (int index);

    juce::String titleText;
    std::vector<std::unique_ptr<ElectryTextButton>> buttons;
    int maxColumns;
    int selectedIndex = 0;
};

class ElectryKnob final : public juce::Component
{
public:
    explicit ElectryKnob (juce::String name);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class ElectryStatusDisplay final : public juce::Component
{
public:
    void setStatus (int activeVoices, int sympatheticStrings, bool ready,
                    double sampleRate, int midiMutePressure, int vibratoGesture,
                    int tremoloGesture, bool scheduleRepaint = true);
    juce::String getStatusText() const;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void paint (juce::Graphics&) override;

private:
    int voices = -1;
    int sympathetic = -1;
    bool isReady = false;
    double rate = 0.0;
    int mutePressure = -1;
    int vibrato = -1;
    int tremolo = -1;
};

// Live eight-string fretboard. It shows which physical string carries every
// sounding note, where it is stopped, how hard it is ringing, and which
// strings are only ringing through the sympathetic bridge coupling. A click
// on one row reuses the engine's existing held-string repick gesture. All of
// its geometry and ballistics come from the JUCE-free electry::visuals helpers,
// so the drawing code stays a thin renderer.
class ElectryFretboardDisplay final : public juce::Component,
                                       public juce::SettableTooltipClient
{
public:
    ElectryFretboardDisplay();

    std::function<void (int)> onRepick;

    // Pulls one frame of per-string state. Returns true while the picture is
    // still changing, so the editor only repaints a moving display.
    bool refresh (const ElectryAudioProcessor&, float frameSeconds);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    struct StringRow
    {
        electry::StringVisualState state {};
        float level = 0.0f;
        float phase = 0.0f;
    };

    std::array<StringRow, electry::ElectryEngine::stringCount> rows {};
    int selectedString = -1;
    int hoveredString = -1;

    void updateAccessibilityTitle();
    void selectString (int stringIndex);
    int stringAtY (float y) const noexcept;
};

class ElectryAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit ElectryAudioProcessorEditor (ElectryAudioProcessor&);
    ~ElectryAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    enum Section
    {
        articulationSection,
        fretboardSection,
        performanceSection,
        coreSection,
        masterSection,
        buildSection,
        detailSection,
        effectsSection,
        sectionCount
    };

    void timerCallback() override;
    void attachSlider (juce::Slider&, const char* parameterId);

    ElectryAudioProcessor& electryProcessor;
    ElectryLookAndFeel lookAndFeel;
    juce::Image backgroundImage;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label factoryProgramLabel;
    juce::Label keyboardHintLabel;
    juce::ComboBox factoryProgramSelector;
    ElectryStatusDisplay statusDisplay;
    ElectryTextButton panicButton { "PANIC" };

    // The two independent keyswitch banks: how the pick moves and what the
    // hands do. Any combination of the two is reachable.
    ElectryChoiceStrip pickStyleStrip {
        "PICK STROKE  (KEYSWITCHES C0..D0)",
        { "DOWN", "UP", "ALT" }
    };
    ElectryChoiceStrip playStyleStrip {
        "PLAY STYLE  (KEYSWITCHES D#0..A0)",
        { "SUSTAIN", "MUTE", "HAMMER", "HARMONIC", "PINCH", "SLIDE",
          "DEAD" }
    };
    ElectryChoiceStrip playStyleKeyModeStrip {
        "PLAY-STYLE KEYS", { "LATCH", "HOLD" }
    };
    ElectryChoiceStrip pickupStrip {
        "PICKUP", { "NECK", "BOTH", "BRIDGE" }, 1
    };
    ElectryChoiceStrip outputModeStrip {
        {}, { "MONO", "STEREO", "2X" }, 8, "OUTPUT MODE"
    };
    ElectryChoiceStrip ampModelStrip {
        "AMP VOICE",
        { "AMERICAN CLEAN", "BRITISH CRUNCH", "MODERN HIGH-GAIN" }
    };

    ElectryKnob guitarBuildKnob { "BUILD" };
    ElectryKnob bodyResonanceKnob { "BODY RES" };

    ElectryKnob pickupTypeKnob { "COIL TYPE" };
    ElectryKnob toneKnob { "TONE" };

    ElectryKnob stringAgeKnob { "AGE" };
    ElectryKnob pickPositionKnob { "PICK POS" };
    ElectryKnob pickHardnessKnob { "HARDNESS" };
    ElectryKnob bendTimeKnob { "BEND TIME" };
    ElectryKnob muteDampingKnob { "TIGHTNESS" };
    ElectryKnob velocityKnob { "VELOCITY" };

    ElectryKnob pickNoiseKnob { "PLECTRUM" };
    ElectryKnob fingerNoiseKnob { "FINGER" };
    ElectryKnob releaseNoiseKnob { "RELEASE" };
    ElectryKnob artifactsKnob { "ARTIFACTS" };

    ElectryKnob sympatheticKnob { "SYMPATHY" };
    ElectryKnob palmMuteKnob { "MUTE PRESS" };
    ElectryKnob strumSpreadKnob { "STRUM" };
    ElectryKnob tremoloRateKnob { "TRM RATE" };
    ElectryKnob resonanceKnob { "RESONANCE" };

    ElectryKnob outputKnob { "OUTPUT" };
    ElectryKnob distortionKnob { "DISTORT" };
    ElectryKnob ampKnob { "AMP" };
    ElectryKnob compressorKnob { "COMP" };
    ElectryKnob delayKnob { "DELAY" };
    ElectryKnob roomKnob { "ROOM" };

    ElectryFretboardDisplay fretboardDisplay;
    ElectryKeyboardComponent keyboard;

    std::unique_ptr<juce::ParameterAttachment> pickupAttachment;
    std::unique_ptr<juce::ParameterAttachment> outputModeAttachment;
    std::unique_ptr<juce::ParameterAttachment> ampModelAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::array<juce::Rectangle<int>, sectionCount> sectionBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessorEditor)
};

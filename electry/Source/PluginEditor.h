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
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

// A guitar-oriented keyboard that keeps the latching play-style keys visually
// separate from the playable Drop-E eight-string range.
class ElectryKeyboardComponent final : public juce::MidiKeyboardComponent
{
public:
    explicit ElectryKeyboardComponent (juce::MidiKeyboardState&);

    void setSelectedKeyswitchIndex (int newIndex);

    void drawWhiteNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&,
                        juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    juce::String getWhiteNoteText (int midiNoteNumber) override;

private:
    int selectedKeyswitchIndex = 0;
};

// A titled row of exclusive buttons, used for the pickup selector and the
// play-style (keyswitch) strip.
class ElectryChoiceStrip final : public juce::Component
{
public:
    ElectryChoiceStrip (juce::String title, juce::StringArray choices);

    std::function<void (int)> onChoice;
    void setSelectedIndex (int newIndex);
    void setTooltipText (const juce::String& text);
    int getSelectedIndex() const noexcept { return selectedIndex; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String titleText;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    int selectedIndex = 0;
};

class ElectryKnob final : public juce::Component
{
public:
    explicit ElectryKnob (juce::String name);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class ElectryStatusDisplay final : public juce::Component
{
public:
    void setStatus (int activeVoices, bool ready, double sampleRate,
                    bool scheduleRepaint = true);
    void paint (juce::Graphics&) override;

private:
    int voices = -1;
    bool isReady = false;
    double rate = 0.0;
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
    juce::Label keyboardHintLabel;
    ElectryStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    ElectryChoiceStrip articulationStrip {
        "PLAY STYLE  (OXBLOOD KEYSWITCHES C0..D#1)",
        { "DOWN", "UP", "ALT", "HAMMER", "TAP", "PALM MUTE", "CHUG", "DEAD",
          "HARMONIC", "PINCH", "TREMOLO", "BEND 1^", "BEND 2^", "RELEASE 1",
          "RELEASE 2", "SLAP" }
    };
    ElectryChoiceStrip pickupStrip { "PICKUP", { "NECK", "BOTH", "BRIDGE" } };
    ElectryChoiceStrip outputModeStrip { "OUTPUT FIELD", { "MONO", "STEREO" } };

    ElectryKnob bodyWoodKnob { "WOOD" };
    ElectryKnob bodySizeKnob { "SIZE" };
    ElectryKnob bodyShapeKnob { "SHAPE" };
    ElectryKnob constructionKnob { "NECK JOIN" };
    ElectryKnob scaleLengthKnob { "SCALE" };
    ElectryKnob bodyResonanceKnob { "BODY RES" };

    ElectryKnob pickupTypeKnob { "COIL TYPE" };
    ElectryKnob toneKnob { "TONE" };

    ElectryKnob stringGaugeKnob { "GAUGE" };
    ElectryKnob stringAgeKnob { "AGE" };
    ElectryKnob pickPositionKnob { "PICK POS" };
    ElectryKnob pickHardnessKnob { "HARDNESS" };
    ElectryKnob bendTimeKnob { "BEND TIME" };
    ElectryKnob muteDampingKnob { "MUTE DAMP" };
    ElectryKnob velocityKnob { "VELOCITY" };

    ElectryKnob pickNoiseKnob { "PLECTRUM" };
    ElectryKnob fingerNoiseKnob { "FINGER" };
    ElectryKnob releaseNoiseKnob { "RELEASE" };
    ElectryKnob artifactsKnob { "ARTIFACTS" };

    ElectryKnob outputKnob { "OUTPUT" };
    ElectryKnob distortionKnob { "DISTORT" };
    ElectryKnob ampKnob { "AMP" };
    ElectryKnob compressorKnob { "COMP" };
    ElectryKnob delayKnob { "DELAY" };
    ElectryKnob roomKnob { "ROOM" };

    ElectryKeyboardComponent keyboard;

    std::unique_ptr<juce::ParameterAttachment> pickupAttachment;
    std::unique_ptr<juce::ParameterAttachment> outputModeAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::array<juce::Rectangle<int>, sectionCount> sectionBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessorEditor)
};

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

// A titled row of exclusive buttons, used for the pickup selector and the
// play-style (keyswitch) strip.
class ElectryChoiceStrip final : public juce::Component
{
public:
    ElectryChoiceStrip (juce::String title, juce::StringArray choices);

    std::function<void (int)> onChoice;
    void setSelectedIndex (int newIndex);
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
        guitarSection,
        pickupSection,
        playSection,
        noiseSection,
        masterSection,
        sectionCount
    };

    void timerCallback() override;
    void attachSlider (juce::Slider&, const char* parameterId);

    ElectryAudioProcessor& electryProcessor;
    ElectryLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label keyboardHintLabel;
    ElectryStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    ElectryChoiceStrip articulationStrip {
        "PLAY STYLE  (KEYSWITCHES C1..G#1)",
        { "DOWN", "UP", "HAMMER", "MUTED", "BEND 1^", "BEND 2^",
          "BEND 1v", "BEND 2v", "SLAP" }
    };
    ElectryChoiceStrip pickupStrip { "PICKUP", { "NECK", "BOTH", "BRIDGE" } };

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

    ElectryKnob outputKnob { "OUTPUT" };

    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::ParameterAttachment> pickupAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::array<juce::Rectangle<int>, sectionCount> sectionBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessorEditor)
};

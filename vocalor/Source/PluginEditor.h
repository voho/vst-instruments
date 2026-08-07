#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <functional>
#include <memory>
#include <vector>

class VocalorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VocalorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

class VocalorChoiceStrip final : public juce::Component
{
public:
    VocalorChoiceStrip (juce::String title, juce::StringArray choices);

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

class VocalorKnob final : public juce::Component
{
public:
    VocalorKnob (juce::String name, juce::String suffix = "%");
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class VocalorStatusDisplay final : public juce::Component
{
public:
    void setStatus (int activeVoices, bool ready, double sampleRate);
    void paint (juce::Graphics&) override;

private:
    int voices = -1;
    bool isReady = false;
    double rate = 0.0;
};

/** Draggable two-dimensional vowel space.

    The cardinal vowel positions and the formant interpolation come from the
    JUCE-free DSP library, so the pad shows the very same model the engine
    renders rather than a decorative approximation.
*/
class VocalorVowelPad final : public juce::Component
{
public:
    VocalorVowelPad();

    /** Reports the start (true) and end (false) of a drag gesture. */
    std::function<void (bool)> onGesture;
    /** Reports a new normalised target position while dragging. */
    std::function<void (float, float)> onPosition;

    void setTargetX (float x);
    void setTargetY (float y);
    void setMorph (float newMorph);
    void setAnchor (int presetVowelIndex, bool isMale);
    void setLivePosition (float x, float y);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    void sendFromMouse (const juce::MouseEvent&);

    float targetX = 0.5f;
    float targetY = 0.5f;
    float morph = 0.0f;
    float liveX = 0.5f;
    float liveY = 0.5f;
    int anchorVowel = 0;
    bool male = false;
    bool dragging = false;
};

/** Vocal-tract response curve plus stereo output meters. */
class VocalorScopeDisplay final : public juce::Component
{
public:
    VocalorScopeDisplay();

    void setState (const vocalor::EngineDisplayState& newState);
    void paint (juce::Graphics&) override;

private:
    static constexpr int curvePoints = 192;
    static constexpr float minimumHz = 80.0f;
    static constexpr float maximumHz = 11000.0f;
    // The peak-normalised formant bank spans about +4 to +16 dB across every
    // setting, so this window frames the curve instead of clipping its peaks
    // against the top of the plot the way a +30 dB ceiling used to.
    static constexpr float floorDb = -36.0f;
    static constexpr float ceilingDb = 18.0f;

    vocalor::EngineDisplayState state;
};

class VocalorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit VocalorAudioProcessorEditor (VocalorAudioProcessor&);
    ~VocalorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Layout
    {
        juce::Rectangle<int> performancePanel;
        juce::Rectangle<int> vowelPanel;
        juce::Rectangle<int> tonePanel;
        juce::Rectangle<int> keyboardPanel;
        juce::Rectangle<int> vowelPad;
        juce::Rectangle<int> morphColumn;
        juce::Rectangle<int> scope;
        juce::Rectangle<int> knobRow;
        int contentX = 34;
        int contentWidth = 0;
    };

    Layout computeLayout() const;

    void timerCallback() override;
    void attachChoice (VocalorChoiceStrip&, const char* parameterId,
                       std::unique_ptr<juce::ParameterAttachment>& attachment);
    void updateConditionalControls();

    VocalorAudioProcessor& processor;
    VocalorLookAndFeel lookAndFeel;

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label keyboardHintLabel;
    VocalorStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    VocalorChoiceStrip profileStrip { "VOICE", { "FEMALE", "MALE" } };
    VocalorChoiceStrip modeStrip { "PERFORMANCE", { "SOLO", "CHOIR", "CHORD" } };
    VocalorChoiceStrip chordStrip { "HARMONY", { "MAJOR", "MINOR" } };
    VocalorChoiceStrip vowelStrip { "VOWEL", { "AAH", "OOH", "UUH" } };
    VocalorChoiceStrip legatoStrip { "PHRASING", { "RETRIGGER", "LEGATO" } };

    juce::Label choirSizeLabel;
    juce::Slider choirSizeSlider;

    VocalorVowelPad vowelPad;
    VocalorScopeDisplay scopeDisplay;

    VocalorKnob morphKnob { "MORPH" };
    VocalorKnob formantShiftKnob { "SHIFT", "st" };

    VocalorKnob breathKnob { "BREATH" };
    VocalorKnob resonanceKnob { "RESONANCE" };
    VocalorKnob tensionKnob { "TENSION" };
    VocalorKnob nasalKnob { "NASAL" };
    VocalorKnob vibratoKnob { "VIBRATO" };
    VocalorKnob humanizeKnob { "HUMANIZE" };
    VocalorKnob dynamicsKnob { "DYNAMICS" };
    VocalorKnob intonationKnob { "INTONATION" };
    VocalorKnob glideKnob { "GLIDE" };
    VocalorKnob spreadKnob { "SPREAD" };
    VocalorKnob roomKnob { "ROOM" };
    VocalorKnob roomSizeKnob { "SIZE" };
    VocalorKnob outputKnob { "OUTPUT", "dB" };

    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::ParameterAttachment> profileAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> chordAttachment;
    std::unique_ptr<juce::ParameterAttachment> vowelAttachment;
    std::unique_ptr<juce::ParameterAttachment> legatoAttachment;
    std::unique_ptr<juce::ParameterAttachment> vowelXAttachment;
    std::unique_ptr<juce::ParameterAttachment> vowelYAttachment;

    std::unique_ptr<SliderAttachment> choirSizeAttachment;
    std::unique_ptr<SliderAttachment> morphAttachment;
    std::unique_ptr<SliderAttachment> formantShiftAttachment;
    std::unique_ptr<SliderAttachment> breathAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> tensionAttachment;
    std::unique_ptr<SliderAttachment> vibratoAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment;
    std::unique_ptr<SliderAttachment> nasalAttachment;
    std::unique_ptr<SliderAttachment> dynamicsAttachment;
    std::unique_ptr<SliderAttachment> intonationAttachment;
    std::unique_ptr<SliderAttachment> glideAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> roomAttachment;
    std::unique_ptr<SliderAttachment> roomSizeAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalorAudioProcessorEditor)
};

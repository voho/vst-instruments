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

    juce::Label choirSizeLabel;
    juce::Slider choirSizeSlider;

    VocalorKnob breathKnob { "BREATH" };
    VocalorKnob resonanceKnob { "RESONANCE" };
    VocalorKnob vibratoKnob { "VIBRATO" };
    VocalorKnob humanizeKnob { "HUMANIZE" };
    VocalorKnob spreadKnob { "SPREAD" };
    VocalorKnob tensionKnob { "TENSION" };
    VocalorKnob roomKnob { "ROOM" };
    VocalorKnob outputKnob { "OUTPUT", "dB" };

    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::ParameterAttachment> profileAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> chordAttachment;
    std::unique_ptr<juce::ParameterAttachment> vowelAttachment;

    std::unique_ptr<SliderAttachment> choirSizeAttachment;
    std::unique_ptr<SliderAttachment> breathAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> vibratoAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> tensionAttachment;
    std::unique_ptr<SliderAttachment> roomAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalorAudioProcessorEditor)
};

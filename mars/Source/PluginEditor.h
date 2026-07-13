#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <functional>
#include <memory>
#include <vector>

class MarsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MarsLookAndFeel();

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

class MarsChoiceStrip final : public juce::Component
{
public:
    MarsChoiceStrip (juce::String title, juce::StringArray choices);

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

class MarsKnob final : public juce::Component
{
public:
    MarsKnob (juce::String name, juce::String suffix = "%");
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class MarsStatusDisplay final : public juce::Component
{
public:
    void setStatus (int activeVoices, bool ready, double sampleRate);
    void paint (juce::Graphics&) override;

private:
    int voices = -1;
    bool isReady = false;
    double rate = 0.0;
};

class MarsAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit MarsAudioProcessorEditor (MarsAudioProcessor&);
    ~MarsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void attachChoice (MarsChoiceStrip&, const char* parameterId,
                       std::unique_ptr<juce::ParameterAttachment>& attachment);
    void updateConditionalControls();

    MarsAudioProcessor& processor;
    MarsLookAndFeel lookAndFeel;

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label keyboardHintLabel;
    MarsStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    MarsChoiceStrip profileStrip { "ALLOY", { "SATURN", "PHOBOS" } };
    MarsChoiceStrip modeStrip { "MODE", { "SINGLE", "STACK", "FIFTH" } };
    MarsChoiceStrip chordStrip { "DRIFT", { "FREE", "LOCKED" } };
    MarsChoiceStrip vowelStrip { "FILTER", { "Ladder", "SVF", "Poles" } };

    juce::Label choirSizeLabel;
    juce::Slider choirSizeSlider;

    MarsKnob breathKnob { "OSC 2" };
    MarsKnob resonanceKnob { "RES" };
    MarsKnob vibratoKnob { "VIB" };
    MarsKnob humanizeKnob { "AGE" };
    MarsKnob spreadKnob { "WIDTH" };
    MarsKnob tensionKnob { "DRIVE" };
    MarsKnob roomKnob { "PLATE" };
    MarsKnob outputKnob { "OUTPUT", "dB" };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsAudioProcessorEditor)
};

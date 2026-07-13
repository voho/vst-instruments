#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cstdint>
#include <memory>

class DrumalorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DrumalorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class DrumalorPad final : public juce::Button
{
public:
    DrumalorPad (drumalor::Instrument instrument, juce::String displayName, int midiNote);

    void setSelected (bool shouldBeSelected);
    void triggerFlash();
    void advanceFlash();
    void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    [[nodiscard]] drumalor::Instrument getInstrument() const noexcept { return drum; }

private:
    drumalor::Instrument drum;
    juce::String nameText;
    juce::String noteText;
    float flashLevel = 0.0f;
    bool selected = false;
};

class DrumalorKnob final : public juce::Component
{
public:
    enum class ValueStyle { Percent, Semitones, Decibels };

    DrumalorKnob (juce::String name, ValueStyle style);
    void setLabelText (const juce::String& text, const juce::String& description);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class DrumalorStatusDisplay final : public juce::Component
{
public:
    DrumalorStatusDisplay();
    void setStatus (int activeVoices, bool ready, double sampleRate);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    int voices = -1;
    bool isReady = false;
    double rate = 0.0;
};

class DrumalorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit DrumalorAudioProcessorEditor (DrumalorAudioProcessor&);
    ~DrumalorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct LayoutAreas
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> pads;
        juce::Rectangle<int> controls;
    };

    void timerCallback() override;
    void selectInstrument (drumalor::Instrument instrument);
    void rebuildSelectedAttachments();
    [[nodiscard]] LayoutAreas calculateLayout() const;

    DrumalorAudioProcessor& audioProcessor;
    DrumalorLookAndFeel lookAndFeel;
    juce::Image vintagePanel;

    juce::Label logoLabel;
    juce::Label editionLabel;
    DrumalorStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    std::array<std::unique_ptr<DrumalorPad>, drumalor::instrumentCount> pads;
    std::array<std::uint32_t, drumalor::instrumentCount> observedTriggerCounters {};
    drumalor::Instrument selectedInstrument { drumalor::Instrument::Kick };

    juce::Label selectedInstrumentLabel;
    DrumalorKnob characterAKnob { "CHARACTER A", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob characterBKnob { "CHARACTER B", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob pitchKnob { "PITCH", DrumalorKnob::ValueStyle::Semitones };
    DrumalorKnob decayKnob { "DECAY", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob outputKnob { "MASTER OUTPUT", DrumalorKnob::ValueStyle::Decibels };

    std::unique_ptr<SliderAttachment> characterAAttachment;
    std::unique_ptr<SliderAttachment> characterBAttachment;
    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumalorAudioProcessorEditor)
};

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
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
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
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

enum class MarsValueFormat
{
    Percent,
    BipolarPercent,
    Hertz,
    Seconds,
    Cents,
    Semitones,
    Octaves,
    Decibels,
    Integer
};

class MarsKnob final : public juce::Component
{
public:
    MarsKnob (juce::String name, MarsValueFormat format = MarsValueFormat::Percent);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class MarsFader final : public juce::Component
{
public:
    MarsFader (juce::String name, MarsValueFormat format = MarsValueFormat::Percent);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
};

class MarsStatusDisplay final : public juce::Component
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

    enum Section
    {
        oscillator1Section,
        oscillator2Section,
        mixerSection,
        filterSection,
        lfoVoiceSection,
        filterEnvelopeSection,
        ampEnvelopeSection,
        masterSection,
        sectionCount
    };

    void timerCallback() override;
    void attachChoice (MarsChoiceStrip&, const char* parameterId, float firstValue = 0.0f);
    void attachSlider (juce::Slider&, const char* parameterId);
    void updateConditionalControls();

    MarsAudioProcessor& marsProcessor;
    MarsLookAndFeel lookAndFeel;
    juce::Image panelTexture;

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label keyboardHintLabel;
    MarsStatusDisplay statusDisplay;
    juce::TextButton panicButton { "PANIC" };

    MarsChoiceStrip osc1WaveStrip { "VCO I WAVE", { "SAW", "PULSE", "TRI" } };
    MarsChoiceStrip osc2WaveStrip { "VCO II WAVE", { "SAW", "PULSE", "TRI" } };
    MarsChoiceStrip filterModelStrip { "FILTER MODEL", { "LADDER", "ORBIT" } };
    MarsChoiceStrip lfoWaveStrip { "LFO WAVE", { "TRI", "SINE", "S & H" } };
    MarsChoiceStrip voiceModeStrip { "VOICE MODE", { "POLY", "UNISON", "FIFTH" } };

    MarsKnob osc1OctaveKnob { "OCTAVE", MarsValueFormat::Octaves };
    MarsKnob osc2OctaveKnob { "OCTAVE", MarsValueFormat::Octaves };
    MarsKnob osc2TuneKnob { "TUNE", MarsValueFormat::Semitones };
    MarsKnob osc2FineKnob { "FINE", MarsValueFormat::Cents };
    MarsKnob oscMixKnob { "BALANCE" };
    MarsKnob pulseWidthKnob { "PULSE WIDTH" };
    MarsKnob subLevelKnob { "SUB" };
    MarsKnob noiseLevelKnob { "NOISE" };
    MarsKnob crossModKnob { "CROSS MOD" };
    MarsKnob cutoffKnob { "CUTOFF", MarsValueFormat::Hertz };
    MarsKnob resonanceKnob { "RESONANCE" };
    MarsKnob filterDriveKnob { "DRIVE" };
    MarsKnob filterShapeKnob { "SHAPE" };
    MarsKnob filterEnvKnob { "ENV", MarsValueFormat::BipolarPercent };
    MarsKnob keyTrackKnob { "KEY TRACK" };

    MarsFader filterAttackFader { "ATTACK", MarsValueFormat::Seconds };
    MarsFader filterDecayFader { "DECAY", MarsValueFormat::Seconds };
    MarsFader filterSustainFader { "SUSTAIN" };
    MarsFader filterReleaseFader { "RELEASE", MarsValueFormat::Seconds };
    MarsFader ampAttackFader { "ATTACK", MarsValueFormat::Seconds };
    MarsFader ampDecayFader { "DECAY", MarsValueFormat::Seconds };
    MarsFader ampSustainFader { "SUSTAIN" };
    MarsFader ampReleaseFader { "RELEASE", MarsValueFormat::Seconds };

    MarsKnob lfoRateKnob { "RATE", MarsValueFormat::Hertz };
    MarsKnob lfoPitchKnob { "PITCH", MarsValueFormat::Cents };
    MarsKnob lfoFilterKnob { "FILTER", MarsValueFormat::Octaves };
    MarsKnob lfoPwmKnob { "PWM" };

    MarsKnob unisonVoicesKnob { "VOICES", MarsValueFormat::Integer };
    MarsKnob driftKnob { "DRIFT" };
    MarsKnob spreadKnob { "SPREAD" };
    MarsKnob glideKnob { "GLIDE", MarsValueFormat::Seconds };
    MarsKnob velocityKnob { "VELOCITY" };

    MarsKnob chorusMixKnob { "ENSEMBLE" };
    MarsKnob chorusRateKnob { "RATE", MarsValueFormat::Hertz };
    MarsKnob outputKnob { "OUTPUT", MarsValueFormat::Decibels };

    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<juce::ParameterAttachment>> choiceAttachments;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::array<juce::Rectangle<int>, sectionCount> sectionBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsAudioProcessorEditor)
};

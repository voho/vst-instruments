#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include "DSP/MarsScope.h"

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
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isHighlighted, bool isDown) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
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

// Panel oscilloscope, output meter, and arpeggiator step readout. All of its
// arithmetic lives in the JUCE-free mars::ScopeReducer, so this class only
// fetches a trace and draws it.
class MarsScopeDisplay final : public juce::Component
{
public:
    MarsScopeDisplay();

    void refresh (const MarsAudioProcessor& processor, bool arpeggiatorEnabled,
                  const juce::String& arpeggiatorMode);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int traceSamples = 2048;

    mars::ScopeReducer reducer;
    std::vector<float> trace;
    juce::String modeText;
    juce::String statusText;
    bool arpActive = false;
    int arpNote = -1;
    int arpKeys = 0;
    float arpPhase = 0.0f;
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
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    enum Section
    {
        oscillator1Section,
        oscillator2Section,
        mixerSection,
        filterSection,
        arpeggiatorSection,
        lfoVoiceSection,
        filterEnvelopeSection,
        ampEnvelopeSection,
        masterSection,
        sectionCount
    };

    void timerCallback() override;
    void attachChoice (MarsChoiceStrip&, const char* parameterId, float firstValue = 0.0f);
    void attachSlider (juce::Slider&, const char* parameterId);
    void attachButton (juce::Button&, const char* parameterId);
    void updateConditionalControls();

    MarsAudioProcessor& marsProcessor;
    MarsLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label logoLabel;
    juce::Label editionLabel;
    juce::Label randomizerLabel;
    juce::Label keyboardHintLabel;
    MarsStatusDisplay statusDisplay;
    MarsScopeDisplay scopeDisplay;
    juce::TextButton panicButton { "PANIC" };
    juce::TextButton oversamplingButton { "HQ 4X" };
    juce::TextButton osc1EnableButton { "ON" };
    juce::TextButton osc2EnableButton { "ON" };
    juce::TextButton monoButton { "MONO" };
    juce::TextButton companderButton { "COMP" };
    juce::TextButton arpEnableButton { "ARP" };
    juce::TextButton arpHoldButton { "HOLD" };
    juce::TextButton randomize1Button { "1%" };
    juce::TextButton randomize10Button { "10%" };
    juce::TextButton randomize100Button { "100%" };

    MarsChoiceStrip osc1ModelStrip { "OSC I MODEL", { "MOOG VCO", "JUNO DCO" } };
    MarsChoiceStrip osc2ModelStrip { "OSC II MODEL", { "MOOG VCO", "JUNO DCO" } };
    MarsChoiceStrip osc1WaveStrip { "OSC I WAVE", { "SAW", "PULSE", "TRI" } };
    MarsChoiceStrip osc2WaveStrip { "OSC II WAVE", { "SAW", "PULSE", "TRI" } };
    MarsChoiceStrip filterModelStrip { "FILTER MODEL", { "LADDER", "SEM" } };
    MarsChoiceStrip lfoWaveStrip { "LFO WAVE", { "TRI", "SINE", "S & H" } };
    MarsChoiceStrip voiceModeStrip { "VOICE MODE", { "POLY", "UNISON", "FIFTH" } };
    MarsChoiceStrip arpModeStrip { "ARP MODE",
                                   { "UP", "DOWN", "UP-DN", "RAND", "PLAY" } };

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

    MarsKnob arpRateKnob { "RATE", MarsValueFormat::Hertz };
    MarsKnob arpOctavesKnob { "RANGE", MarsValueFormat::Integer };
    MarsKnob arpGateKnob { "GATE" };

    MarsKnob polyphonyKnob { "POLYPHONY", MarsValueFormat::Integer };
    MarsKnob unisonVoicesKnob { "VOICES", MarsValueFormat::Integer };
    MarsKnob unisonDetuneKnob { "DETUNE", MarsValueFormat::Cents };
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
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::array<juce::Rectangle<int>, sectionCount> sectionBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsAudioProcessorEditor)
};

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The panel follows the modelled instrument's functional geometry — which
// controls exist, how they group into silkscreened sections, and which are
// knobs, rockers, sliders or wheels — with Ghostar's own livery: near-black
// panel, bone silkscreen, spectral cyan pointers.
class GhostarLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GhostarLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width,
                          int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width,
                          int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

class GhostarAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GhostarAudioProcessorEditor(GhostarAudioProcessor&);
    ~GhostarAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    // One labelled panel control and its host-parameter attachment.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            attachment;
    };
    struct Fader
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            attachment;
    };
    struct Rocker
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
            attachment;
    };
    struct Selector
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            attachment;
    };

    void addKnob(Knob& knob, const char* parameterId, const juce::String& text);
    void addFader(Fader& fader, const char* parameterId,
                  const juce::String& text);
    void addRocker(Rocker& rocker, const char* parameterId,
                   const juce::String& text);
    void addSelector(Selector& selector, const char* parameterId,
                     const juce::String& text);
    void layoutKnob(Knob& knob, juce::Rectangle<int> area);
    void layoutFader(Fader& fader, juce::Rectangle<int> area);
    void layoutSelector(Selector& selector, juce::Rectangle<int> area);

    GhostarAudioProcessor& processor;
    GhostarLookAndFeel lookAndFeel;

    std::vector<Section> sections;

    // MASTER
    Knob tune;
    Selector octave;
    // OSCILLATOR A / B
    Selector oscAWaveform;
    Rocker sync;
    Selector oscBWaveform;
    Selector oscBRange;
    Knob interval;
    // TRIGGER / GATE SELECT
    Selector trigger;
    Rocker gateKbd;
    Rocker gateX;
    Rocker gateYExt;
    // MOD X
    Selector arpeggiator;
    Selector modSource;
    Knob lfoRate;
    // SHAPER Y
    Selector shaperMode;
    Knob shaperShape;
    Knob shaperRate;
    // WHEEL DESTINATIONS
    Selector modXTo;
    Rocker shapeXWithY;
    Selector shaperYTo;
    // AUDIO MIXER
    Knob masterVolume;
    Knob brightness;
    Fader shaperPathA;
    Fader shaperPathB;
    Fader shaperPathRing;
    Fader shaperPathNoise;
    Fader filterPathA;
    Fader filterPathB;
    Fader filterPathNoise;
    // FILTERS
    Knob cutoff;
    Knob lowerOnly;
    Selector upperResonance;
    Knob resonance;
    Selector slope;
    Knob kbAmount;
    Selector lowerMode;
    Selector tracking;
    // ENVELOPES
    Knob filterEnvAmount;
    Fader filterAttack;
    Fader filterDecay;
    Fader filterSustain;
    Fader filterRelease;
    Rocker vcaBypass;
    Fader loudnessAttack;
    Fader loudnessDecay;
    Fader loudnessSustain;
    Fader loudnessRelease;
    // Performance
    Knob glide;
    Selector glideMode;
    juce::Slider pitchWheel;
    Knob xWheel;
    Knob yWheel;
    Rocker splitPaths;
    juce::TextButton panicButton { "PANIC" };
    juce::Label wordmark;

    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostarAudioProcessorEditor)
};

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The panel follows the modelled instrument's functional geometry — which
// controls exist, how they group into silkscreened sections, and which are
// knobs, rockers, sliders or wheels — in a charcoal livery with grey knob
// caps, their markings in black and the silkscreen in white. The keys have
// the bottom of the window to themselves, with the performance wheels
// standing to their left, so the controls own everything above.
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
    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
};

// JUCE paints a brightened inset over each black note, which against a
// charcoal panel turns the sharps grey. The modelled instrument's sharps are
// black, so they are painted flat with only a hairline to separate them.
class PanelKeyboard final : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;

    void drawBlackNote(int midiNoteNumber, juce::Graphics& g,
                       juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour noteFillColour) override;
};

class GhostarAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit GhostarAudioProcessorEditor(GhostarAudioProcessor&);
    ~GhostarAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // The largest whole panel that fits `workArea` — a display's usable
    // bounds, empty when unknown — never larger than the design size and
    // never smaller than the size the silkscreen stays readable at. Public
    // and pure so the fit rule can be tested on screens no build machine has
    // to actually have.
    [[nodiscard]] static juce::Rectangle<int> panelSizeForWorkArea(
        juce::Rectangle<int> workArea);

private:
    void timerCallback() override;

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        bool accent { false };
    };

    // Every control and every piece of silkscreen lives on this one child,
    // which is always exactly the design size; the editor scales it to
    // whatever the window is. The panel is a fixed geometry — a hardware
    // instrument's controls do not reflow — so the alternative to scaling it
    // is clipping it, and 780 points of panel do not fit the 768-point screen
    // a 1366x768 laptop has.
    class PanelCanvas final : public juce::Component
    {
    public:
        explicit PanelCanvas(GhostarAudioProcessorEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;

    private:
        GhostarAudioProcessorEditor& owner;
    };

    // One labelled panel control and its host-parameter attachment. A knob's
    // caption doubles as its readout: it shows the silkscreen's own 0–10
    // while the control is moving, and falls back to the control's name a
    // moment after it stops, so the panel reads as a panel at rest and as a
    // set of numbers while it is being dialled.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        juce::String name;
        int readoutTicks { 0 };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            attachment;
    };
    struct Fader
    {
        juce::Slider slider;
        juce::Label label;
        juce::String name;
        int readoutTicks { 0 };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            attachment;
    };
    struct Rocker
    {
        juce::ToggleButton button;
        juce::Label label;
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

    void addKnob(Knob& knob, const char* parameterId,
                 const juce::String& text, const juce::String& tooltip);
    void addFader(Fader& fader, const char* parameterId,
                  const juce::String& text, const juce::String& tooltip);
    void addRocker(Rocker& rocker, const char* parameterId,
                   const juce::String& text, const juce::String& tooltip);
    void addSelector(Selector& selector, const char* parameterId,
                     const juce::String& text, const juce::String& tooltip);
    void layoutKnob(Knob& knob, juce::Rectangle<int> area);
    void layoutFader(Fader& fader, juce::Rectangle<int> area);
    void layoutSelector(Selector& selector, juce::Rectangle<int> area);
    void layoutRocker(Rocker& rocker, juce::Rectangle<int> area);
    // Places every control inside the design-size rectangle. Called from
    // resized(), but independent of the window: the window only sets the
    // canvas transform.
    void layoutPanel();

    void showProgramMenu();
    void stepProgram(int delta);
    void refreshProgramDisplay();

    GhostarAudioProcessor& processor;
    GhostarLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    PanelCanvas canvas { *this };

    std::vector<Section> sections;

    // Header
    juce::Label wordmark;
    juce::Label subtitle;
    juce::TextButton previousProgram { "<" };
    juce::TextButton nextProgram { ">" };
    juce::TextButton programName { "Init" };
    juce::Label programBank;
    juce::TextButton panicButton { "PANIC" };
    juce::Rectangle<int> gateLampBounds;
    bool gateLampLit { false };
    int shownProgram { -1 };
    // True until every attachment has taken its parameter's standing value,
    // so the wiring-up callbacks do not leave the panel showing numbers.
    bool wiringUp { true };

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
    juce::Label shaperPathCaption;
    juce::Label filterPathCaption;
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
    juce::Label pitchWheelLabel;
    Fader xWheel;
    Fader yWheel;
    Rocker splitPaths;

    PanelKeyboard keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostarAudioProcessorEditor)
};

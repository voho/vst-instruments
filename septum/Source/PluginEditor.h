#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The panel keeps the modelled instrument's arrangement and its own branding
// and project-drawn controls, and is laid out so the signal path reads off it.
//
// The performance cluster sits at the far left (master volume, octave,
// portamento, solo, tempo). To its right are three bands:
//
//   VOICE        OSC 1 -> OSC 2 -> MIX/MOD -> FILTER -> AMP, with a chevron
//                drawn between each pair, because that is a real chain
//   MODULATION   PITCH ENV, FILTER ENV, AMP ENV, LFO 1, LFO 2 — what moves
//                the chain rather than what carries it
//   INPUT & FX   ARPEGGIO, EXT IN, DELAY, REVERB — the two ends of the
//                instrument
//
// Every control is the same size wherever it appears — a knob is a knob — and
// every one of them reads out its value in the units the manual prints, so
// nothing has to be dragged to be understood. The patch/keyboard strip sits
// above the keys where the hardware puts its patch buttons, with the
// bend/modulation lever to their left. Controls that exist only as physical
// hardware (D-Beam, the recorder, tap tempo) are deliberately not replicated;
// one set of tone controls edits the selected tone, exactly as the hardware
// panel works.

class SeptumLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SeptumLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool isHighlighted,
                               bool isDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isHighlighted, bool isDown) override;
};

// The bend/modulation lever left of the keys: the horizontal axis bends
// pitch and springs back to center on release; the vertical axis applies
// modulation and holds its position, mirroring the hardware lever's two
// motions.
class SeptumLever final : public juce::Component
{
public:
    explicit SeptumLever (SeptumAudioProcessor& processor);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void applyFromPoint (juce::Point<float> position);
    void push() noexcept;

    SeptumAudioProcessor& processor;
    float bend { 0.0f };  // -1..+1, springs back
    float mod { 0.0f };   // 0..1, latches
};

class SeptumAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer,
                                             private juce::MidiKeyboardState::Listener
{
public:
    explicit SeptumAudioProcessorEditor (SeptumAudioProcessor&);
    ~SeptumAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // The size to open at on a display with this much usable room. Public so
    // the suite can check the fit rule on screens no build machine has to
    // have.
    [[nodiscard]] static juce::Rectangle<int> panelSizeForWorkArea (
        juce::Rectangle<int> workArea);

    // The panel itself, always laid out at its design size and scaled to
    // whatever the window is. The suite walks it to prove every control the
    // panel builds is actually placed.
    [[nodiscard]] juce::Component& getPanel() noexcept { return canvas; }

private:
    // A hardware panel's controls do not reflow, so the alternative to
    // scaling this one is clipping it — and 784 points of panel do not fit
    // the 768-point screen a 1366x768 laptop has. Everything the panel draws
    // lives on this canvas, which is always exactly the design size; the
    // editor only chooses the transform that maps it onto the window.
    class PanelCanvas final : public juce::Component
    {
    public:
        explicit PanelCanvas (SeptumAudioProcessorEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;

    private:
        SeptumAudioProcessorEditor& owner;
    };

    enum class Style { Knob, VSlider, Combo, Toggle, Action };

    // Which band of the panel a section belongs to. The band decides the
    // colour of the rule above its title, which is the only thing that
    // distinguishes the sections from one another — enough to group them,
    // not enough to turn the panel into a chart.
    enum class Band { Voice, Modulation, InputEffects, Perform };

    // What is drawn in the gap between two sections of the same band.
    enum class Connector { None, Sum, Flow };
    struct ConnectorMark
    {
        juce::Point<int> position;
        Connector kind { Connector::Flow };
    };

    struct Control
    {
        juce::String suffix;      // per-tone parameter suffix, or full ID
        bool perTone { true };
        Style style { Style::Knob };
        juce::String unit;        // printed after the value, e.g. "st", "%"
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Label> value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
            buttonAttachment;
    };

    struct Section
    {
        juce::String title;
        Band band { Band::Voice };
        juce::Rectangle<int> bounds;
        std::vector<Control*> controls;
        // How many of the section's grid controls go on each row. Sections
        // are sized to fit their contents rather than their contents scaled
        // to fit them, which is what keeps every knob the same size.
        std::vector<int> rowCounts;
        bool manualLayout { false };

        [[nodiscard]] int naturalWidth() const;
    };

    Control* addControl (Section& section, const juce::String& suffix,
                         const juce::String& label, Style style,
                         bool perTone = true, const juce::String& unit = {});
    void bindControls();
    void layoutSection (Section& section, juce::Rectangle<int> bounds);
    void layoutBand (const std::vector<int>& indices, juce::Rectangle<int> bounds,
                     const std::vector<Connector>& connectors);
    void refreshValues();
    // Places every control inside the design-size rectangle. Called from
    // resized(), but independent of the window: the window only sets the
    // canvas transform.
    void layoutPanel();
    void paintPanel (juce::Graphics&);
    void setToneParameter (const char* suffix, float natural);
    [[nodiscard]] float getToneParameter (const char* suffix) const;
    void applyKeyboardOctave();
    void stepKeyboardOctave (int delta);
    void timerCallback() override;

    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note,
                       float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note,
                        float velocity) override;

    SeptumAudioProcessor& processor;
    SeptumLookAndFeel lookAndFeel;
    PanelCanvas canvas { *this };

    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::unique_ptr<Control>> controls;
    Section* performSection { nullptr };
    Section* systemSection { nullptr };
    Section* stripSection { nullptr };
    // Where the voice chain's connectors go, filled in by resized().
    std::vector<ConnectorMark> chevrons;
    juce::Rectangle<int> meterBounds;

    // Left performance cluster.
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        masterAttachment;
    juce::Label masterLabel, masterValueLabel, octLabel, octValueLabel, voiceLabel;
    juce::TextButton octDownButton { "DOWN" }, octUpButton { "UP" };
    Control* portaControl { nullptr };
    Control* portaTimeControl { nullptr };
    Control* soloControl { nullptr };
    Control* tempoControl { nullptr };

    // Patch strip above the keyboard.
    juce::ComboBox programBox;
    juce::TextButton lowerButton { "LOWER" }, upperButton { "UPPER" };
    juce::Label titleLabel, subtitleLabel;

    // OSC 2 INTERVAL buttons (settled behavior: -OCT one octave below,
    // 5th seven semitones above; both together = unison).
    Control* intervalOctControl { nullptr };
    Control* intervalFifthControl { nullptr };

    // Hovering any control names the parameter it edits, in the same words
    // the host's own parameter list uses.
    juce::TooltipWindow tooltips { this, 650 };

    SeptumLever lever;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    bool editingUpper { true };
    float meterLevel[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeptumAudioProcessorEditor)
};

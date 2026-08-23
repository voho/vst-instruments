#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <limits>
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
// hardware — the D Beam's infrared distance sensor, the step recorder, tap
// tempo — are deliberately not replicated; the four Patch Common bytes the
// D Beam owns are still stored and round-tripped, they simply have no control
// naming them. One set of tone controls edits the selected tone, exactly as
// the hardware panel works.

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
    void mouseDoubleClick (const juce::MouseEvent&) override;

    [[nodiscard]] float getModulation() const noexcept { return mod; }

private:
    static constexpr int captionHeight = 12;

    void applyFromEvent (const juce::MouseEvent&);
    [[nodiscard]] juce::Rectangle<float> leverBounds() const;
    void push() noexcept;

    SeptumAudioProcessor& processor;
    float bend { 0.0f };  // -1..+1, springs back
    float mod { 0.0f };   // 0..1, latches
    // Where the modulation axis was grabbed, so a drag moves it by the travel
    // rather than jumping it to the click.
    float grabY { 0.0f };
    float grabMod { 0.0f };
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

    // Three invariants the panel is built on. They were `jassert`s, which
    // NDEBUG removes from every build this project produces — the plug-in,
    // both test binaries and CI's Release — so the thing the change log called
    // "a build-time failure" was present in no build and no test. They are
    // state the suite reads now, and a green suite is what enforces them.
    //
    // Every control's parameter id resolves; a control whose id stops
    // resolving would otherwise ship drawing, hovering and dragging while
    // editing nothing.
    [[nodiscard]] const juce::StringArray& getUnresolvedParameterIds() const noexcept
    {
        return unresolvedParameterIds;
    }
    // No section mixes per-tone and shared controls. A mixed one is classified
    // by its first per-tone control and wears the tone chip and wash over
    // controls that are not per-tone, which is exactly the defect Step 28
    // removed.
    [[nodiscard]] const juce::StringArray& getMixedScopeSections() const noexcept
    {
        return mixedScopeSections;
    }
    // The section titles the layout addresses by index, in index order, so
    // inserting a section cannot silently shift every list below it.
    [[nodiscard]] juce::StringArray getSectionTitles() const;
    // Sections whose laid-out contents do not fit inside their own well. A
    // section is sized to its contents rather than its contents scaled to it,
    // so one that is handed less room than it asked for does not shrink — it
    // overflows, and its bottom row of value read-outs lands on the well's
    // border. Read by the suite after a layout.
    [[nodiscard]] juce::StringArray getSectionsOverflowingTheirWell() const;

    // What the key-zone band prints beside the split boundary, and where. The
    // paint uses this, and the suite reads it: the name has to stay inside the
    // band (SPLIT POINT reaches C8 while the drawn keyboard stops at C7) and
    // has to name the key the way the keyboard under it names it.
    struct SplitPointCaption
    {
        juce::String text;
        juce::Rectangle<int> bounds;
    };
    [[nodiscard]] SplitPointCaption getSplitPointCaption() const;

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

    enum class Style { Knob, VSlider, Combo, WideCombo, Toggle, Action };

    // Whether a section's controls edit one tone or the whole instrument.
    // Derived from the controls themselves rather than declared, so a section
    // cannot claim a scope its contents do not have. Every section on this
    // panel is one or the other: the parameter contract keeps the per-tone
    // values in the Patch Tone blocks and the shared ones in Patch Common,
    // and the panel now follows that line exactly.
    enum class Scope { Shared, PerTone };

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
        // What a bipolar knob's two ends actually are. The manual prints
        // BALANCE and TONE BALANCE as a signed number and the panel prints
        // what the manual prints, so the direction is said beside the travel
        // rather than inside the value: a reading of -63 does not say which
        // of two things it favours, and these controls have no default the
        // eye can fall back on.
        juce::String leftEnd, rightEnd;
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
        Scope scope { Scope::Shared };
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
    // Names the two ends of a bipolar knob's travel, drawn under the arc.
    static void nameEnds (Control* control, const char* left, const char* right);
    // `perToneOnly` re-attaches just the controls whose parameter changes with
    // the edit target; the shared ones keep the attachment they already have.
    void bindControls (bool perToneOnly = false);
    // What the keyboard mode and part say about the two tones right now.
    struct ToneAudibility
    {
        bool upperSounds { true };
        bool lowerSounds { false };
        juce::String summary;   // one line, printed beside the edit tabs
    };
    [[nodiscard]] ToneAudibility toneAudibility() const;
    void refreshToneTarget();
    // The edit target rides in the state tree rather than in a parameter, and
    // setStateInformation replaces the whole tree, so an open editor has to be
    // told. Called from the frame timer and from a layout.
    void reconcileEditTarget();
    void setEditingUpper (bool upper);
    void paintKeyboardZones (juce::Graphics&);
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
    // What `setToneParameter` would actually store for this value. The OSC 2
    // INTERVAL buttons and their lamps compare against a target, and the write
    // snaps it to the parameter's range, so an unsnapped target near the ends
    // of the pitch range makes the button a one-way trap with a dark lamp.
    [[nodiscard]] float snapToneParameter (const char* suffix, float natural) const;
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
    Section* tonePlaySection { nullptr };
    Section* editToneSection { nullptr };
    // Controls the current keyboard mode makes inert, dimmed while it does.
    Control* partControl { nullptr };
    Control* splitPointControl { nullptr };
    Control* splitArpControl { nullptr };
    // Where the voice chain's connectors go, filled in by resized().
    std::vector<ConnectorMark> chevrons;
    juce::Rectangle<int> meterBounds;
    // The band above the keys that says which tone each key reaches.
    juce::Rectangle<int> keyZoneBounds;

    // Left performance cluster.
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        masterAttachment;
    juce::Label masterLabel, masterValueLabel, octLabel, octValueLabel, voiceLabel;
    juce::TextButton octDownButton { "DOWN" }, octUpButton { "UP" };
    Control* tempoControl { nullptr };

    // Patch strip above the keyboard.
    juce::ComboBox programBox;
    juce::Label programLabel;
    // The edit-target tabs, in the header above everything they govern.
    juce::TextButton upperButton { "UPPER" }, lowerButton { "LOWER" };
    juce::Label toneStatusLabel;
    juce::Label titleLabel, subtitleLabel;

    // OSC 2 INTERVAL buttons (settled behavior: -OCT one octave below,
    // 5th seven semitones above; both together = unison).
    Control* intervalOctControl { nullptr };
    Control* intervalFifthControl { nullptr };

    // Hovering any control names the parameter it edits, in the same words
    // the host's own parameter list uses.
    juce::TooltipWindow tooltips { this, 650 };

    SeptumLever lever;

public:
    // The suite drives the lever through the same path the mouse does.
    [[nodiscard]] SeptumLever& getLever() noexcept { return lever; }

private:
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    bool editingUpper { true };
    // The keyboard mode, part and split point the panel last drew, so the
    // frame timer only repaints when one of them has actually moved.
    juce::String lastKeyboardState;
    // And the octave shift the keys were last named for. JUCE's
    // setOctaveForMiddleC repaints unconditionally, so calling it every frame
    // invalidated the whole 1204x73 keyboard 24 times a second on an idle
    // panel — and through the scaled canvas that re-ran the panel paint over
    // that strip as well.
    int lastKeyboardOctave { std::numeric_limits<int>::min() };
    juce::StringArray unresolvedParameterIds;
    juce::StringArray mixedScopeSections;
    float meterLevel[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeptumAudioProcessorEditor)
};

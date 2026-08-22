#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The panel follows the modelled instrument's physical arrangement while
// keeping independent branding and project-drawn controls: the performance
// cluster at the far left (master volume, octave, portamento, solo, tempo),
// then the synthesis sections in the hardware's own left-to-right order —
// OSC 1, OSC 2 (with the INTERVAL buttons), PITCH ENV, MIX/MOD, FILTER,
// FILTER ENV, AMP, AMP ENV, LFO 1, LFO 2, EFFECTS — the patch/keyboard strip
// above the keys where the hardware puts its patch buttons, and the
// bend/modulation lever left of the keyboard. Controls that only exist as
// physical hardware (D-Beam, recorder, tap tempo, EXT IN) are deliberately
// not replicated; one set of tone controls edits the selected tone, exactly
// as the hardware panel works.

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

private:
    enum class Style { Knob, VSlider, Combo, Toggle, Action };

    struct Control
    {
        juce::String suffix;      // per-tone parameter suffix, or full ID
        bool perTone { true };
        Style style { Style::Knob };
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::Label> label;
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
        juce::Rectangle<int> bounds;
        std::vector<Control*> controls;
        int firstRowCount { 0 };  // 0 = split evenly across two rows
        bool manualLayout { false };
    };

    Control* addControl (Section& section, const juce::String& suffix,
                         const juce::String& label, Style style,
                         bool perTone = true);
    void bindControls();
    void layoutSection (Section& section, juce::Rectangle<int> bounds);
    void setToneParameter (const char* suffix, float natural);
    [[nodiscard]] float getToneParameter (const char* suffix) const;
    void applyKeyboardOctave();
    void timerCallback() override;

    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note,
                       float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note,
                        float velocity) override;

    SeptumAudioProcessor& processor;
    SeptumLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::unique_ptr<Control>> controls;
    Section* performSection { nullptr };
    Section* stripSection { nullptr };

    // Left performance cluster.
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        masterAttachment;
    juce::Label masterLabel, octLabel, octValueLabel, voiceLabel;
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

    SeptumLever lever;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };
    int keyboardOctaveShift { 0 };

    bool editingUpper { true };
    float meterLevel[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeptumAudioProcessorEditor)
};

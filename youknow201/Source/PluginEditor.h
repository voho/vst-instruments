#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The panel follows the modelled instrument's functional layout — OSC 1/2,
// PITCH ENV, MIX/MOD, FILTER, FILTER ENV, AMP, AMP ENV, LFO 1/2, EFFECTS,
// performance block — with independent branding and project-drawn controls.
// One set of tone controls edits the selected tone (UPPER by default), which
// is exactly how the hardware panel works.

class YouKnow201LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    YouKnow201LookAndFeel();

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

class YouKnow201AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer,
                                             private juce::MidiKeyboardState::Listener
{
public:
    explicit YouKnow201AudioProcessorEditor (YouKnow201AudioProcessor&);
    ~YouKnow201AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Style { Knob, VSlider, Combo, Toggle };

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
    };

    Control* addControl (Section& section, const juce::String& suffix,
                         const juce::String& label, Style style,
                         bool perTone = true);
    void bindControls();
    void layoutSection (Section& section, juce::Rectangle<int> bounds);
    void timerCallback() override;

    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note,
                       float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note,
                        float velocity) override;

    YouKnow201AudioProcessor& processor;
    YouKnow201LookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::unique_ptr<Control>> controls;

    juce::TextButton upperButton { "UPPER" }, lowerButton { "LOWER" };
    juce::ComboBox programBox;
    juce::Label titleLabel, subtitleLabel, voiceLabel;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        masterAttachment;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    bool editingUpper { true };
    float meterLevel[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow201AudioProcessorEditor)
};

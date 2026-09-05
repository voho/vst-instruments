#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>

class AcustraLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AcustraLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool isHighlighted,
                               bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class AcustraAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit AcustraAudioProcessorEditor (AcustraAudioProcessor&);
    ~AcustraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class ChoiceButtonGroup;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void updateConstructionControls();
    void configureSetupMenu (std::size_t index, const juce::String& name,
                             const juce::String& description);
    void configureChoice (std::size_t index, const juce::String& name,
                          const char* parameterId,
                          const juce::String& description);
    void configureSlider (std::size_t index, const juce::String& name,
                          const char* parameterId, const juce::String& description,
                          bool decibels = false);

    AcustraAudioProcessor& audioProcessor;
    AcustraLookAndFeel lookAndFeel;
    juce::Image cedarBackground;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::TextButton panicButton { "PANIC" };

    std::array<juce::Label, 3> setupLabels;
    std::array<juce::ComboBox, 3> setupControls;
    std::array<std::unique_ptr<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 2> setupAttachments;

    std::array<juce::Label, 4> choiceLabels;
    std::array<std::unique_ptr<ChoiceButtonGroup>, 4> choiceControls;
    std::array<juce::Label, 6> sliderLabels;
    std::array<juce::Slider, 6> sliderControls;

    juce::MidiKeyboardComponent keyboard {
        audioProcessor.keyboardState,
        juce::MidiKeyboardComponent::horizontalKeyboard
    };

    std::array<std::unique_ptr<SliderAttachment>, 6> sliderAttachments;

    juce::Rectangle<int> setupPanelBounds;
    juce::Rectangle<int> choicePanelBounds;
    juce::Rectangle<int> tonePanelBounds;
    juce::Rectangle<int> keyboardPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcustraAudioProcessorEditor)
};

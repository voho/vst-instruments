#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "DSP/UiMath.h"

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
    // Live post-VCA level of this channel, used for the pad's activity bar.
    void setLevel (float linearLevel);
    void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    [[nodiscard]] drumalor::Instrument getInstrument() const noexcept { return drum; }

private:
    drumalor::Instrument drum;
    juce::String nameText;
    juce::String noteText;
    float flashLevel = 0.0f;
    float levelPosition = 0.0f;
    float lastPaintedPeak = -1.0f;
    drumalor::ui::MeterBallistics ballistics;
    bool selected = false;
};

class DrumalorKnob final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    enum class ValueStyle { Percent, Semitones, Decibels };
    enum class VisualRole { Voice, BipolarVoice, Master };

    DrumalorKnob (juce::String name, ValueStyle style,
                  VisualRole role = VisualRole::Voice);
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

// Stereo output meter with peak hold plus a downward bus gain-reduction bar.
// All ballistics and the decibel scale come from the JUCE-free ui namespace, so
// they are unit-tested rather than tuned by eye inside paint().
class DrumalorBusMeter final : public juce::Component
{
public:
    DrumalorBusMeter();
    void setLevels (float leftLinear, float rightLinear, float busGain);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static constexpr float floorDecibels = -48.0f;

private:
    drumalor::ui::MeterBallistics leftBallistics;
    drumalor::ui::MeterBallistics rightBallistics;
    float reductionPosition = 0.0f;
    float lastPaintedReduction = -1.0f;
    float lastPaintedLeft = -1.0f;
    float lastPaintedRight = -1.0f;
    float lastPaintedLeftPeak = -1.0f;
    float lastPaintedRightPeak = -1.0f;
    int announcedClipState = -1;
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
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LayoutAreas
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> pads;
        juce::Rectangle<int> controls;
        juce::Rectangle<int> controlHeader;
        juce::Rectangle<int> selectedVoiceHeader;
        juce::Rectangle<int> voiceDeck;
        juce::Rectangle<int> voiceStrip;
        juce::Rectangle<int> masterDeck;
    };

    void timerCallback() override;
    void selectInstrument (drumalor::Instrument instrument);
    void rebuildSelectedAttachments();
    [[nodiscard]] LayoutAreas calculateLayout() const;

    DrumalorAudioProcessor& audioProcessor;
    DrumalorLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;
    juce::Image vintagePanel;

    juce::Label logoLabel;
    juce::Label editionLabel;
    DrumalorStatusDisplay statusDisplay;
    DrumalorBusMeter busMeter;
    juce::TextButton panicButton { "PANIC" };

    std::array<std::unique_ptr<DrumalorPad>, drumalor::instrumentCount> pads;
    std::array<std::uint32_t, drumalor::instrumentCount> observedTriggerCounters {};
    drumalor::Instrument selectedInstrument { drumalor::Instrument::Kick };

    juce::Label selectedInstrumentLabel;
    DrumalorKnob characterAKnob { "CHARACTER A", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob characterBKnob { "CHARACTER B", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob pitchKnob { "PITCH", DrumalorKnob::ValueStyle::Semitones,
                             DrumalorKnob::VisualRole::BipolarVoice };
    DrumalorKnob decayKnob { "DECAY", DrumalorKnob::ValueStyle::Percent };
    DrumalorKnob levelKnob { "LEVEL", DrumalorKnob::ValueStyle::Decibels };

    juce::Label panLabel;
    juce::Slider panSlider;
    juce::Label chokeLabel;
    juce::ComboBox chokeBox;

    DrumalorKnob humaniseKnob { "HUMANISE", DrumalorKnob::ValueStyle::Percent,
                                DrumalorKnob::VisualRole::Master };
    DrumalorKnob busDriveKnob { "BUS DRIVE", DrumalorKnob::ValueStyle::Percent,
                                DrumalorKnob::VisualRole::Master };
    DrumalorKnob busCompressionKnob { "BUS COMP", DrumalorKnob::ValueStyle::Percent,
                                      DrumalorKnob::VisualRole::Master };
    DrumalorKnob outputKnob { "MASTER OUTPUT", DrumalorKnob::ValueStyle::Decibels,
                              DrumalorKnob::VisualRole::Master };

    std::unique_ptr<SliderAttachment> characterAAttachment;
    std::unique_ptr<SliderAttachment> characterBAttachment;
    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> levelAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<ComboBoxAttachment> chokeAttachment;
    std::unique_ptr<SliderAttachment> humaniseAttachment;
    std::unique_ptr<SliderAttachment> busDriveAttachment;
    std::unique_ptr<SliderAttachment> busCompressionAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumalorAudioProcessorEditor)
};

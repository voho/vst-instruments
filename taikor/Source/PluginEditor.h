#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "DSP/UiMath.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class TaikorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TaikorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

// One stroke of the vocabulary. The pad shows the stroke's name, the spoken
// syllable it is named for, and the key it currently answers to.
class TaikorPad final : public juce::Button
{
public:
    explicit TaikorPad (taikor::Articulation articulation, int octaveOffset = 0);

    void setSelected (bool shouldBeSelected);
    void setOctaveOffset (int octaveOffset);
    void triggerFlash();
    void advanceFlash();
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    [[nodiscard]] taikor::Articulation getArticulation() const noexcept
    {
        return articulation;
    }

    [[nodiscard]] int getOctaveOffset() const noexcept
    {
        return octaveOffset;
    }

    // What a screen reader is told this pad does. Built on demand because the
    // note it plays follows the selected octave.
    [[nodiscard]] juce::String accessibleHelpText() const;

private:
    void refreshNoteText();

    taikor::Articulation articulation;
    int octaveOffset = 0;
    juce::String drumNameText;
    juce::String noteText;
    juce::String keyText;
    float flashLevel = 0.0f;
    bool selected = false;
};

// The drum family is part of the playing surface rather than a detached mode
// switch. Each row header is therefore both a painted portrait of the drum and
// the control that selects it for the live head readout.
class TaikorDrumButton final : public juce::Button
{
public:
    TaikorDrumButton (int octaveOffset, juce::Image drumAtlas);

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

private:
    juce::Image drumPainting;
    juce::String noteName;
    juce::String drumName;
};

class TaikorKnob final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    enum class ValueStyle { Plain, Percent, Semitones, Centimetres, Decibels };
    enum class VisualRole { Drum, Stroke, Microphone, Master };

    TaikorKnob (juce::String name, ValueStyle style, VisualRole role);
    void setLabelText (const juce::String& text, const juce::String& description);
    void resized() override;

    juce::Slider slider;
    [[nodiscard]] VisualRole getRole() const noexcept { return role; }

private:
    juce::Label label;
    VisualRole role;
};

// The drum, seen from the player's side. It draws the head, the tack ring, the
// two close microphones where the Mic Spread and Mic Distance controls put
// them, and a ripple where the last stroke landed. Everything on it is read
// from the model rather than decorated onto it, so it is a readout of the
// instrument rather than a picture of one.
class TaikorHeadDisplay final : public juce::Component
{
public:
    TaikorHeadDisplay();

    void setStrike (float normalisedRadius, float angleRadians, float level,
                    taikor::Articulation articulation);
    void setMicrophones (float spread, float normalisedDistance);
    void setMeasurements (float fundamentalHz, float breathingHz,
                          float diameterCentimetres, float tailSeconds);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    float strikeRadius = 0.0f;
    float strikeAngle = 0.0f;
    float strikeLevel = 0.0f;
    taikor::Articulation lastArticulation = taikor::Articulation::Don;
    float micSpread = 0.55f;
    float micDistance = 0.35f;
    float fundamental = 0.0f;
    float breathing = 0.0f;
    float diameter = 55.0f;
    float tail = 0.0f;
};

class TaikorStatusDisplay final : public juce::Component
{
public:
    TaikorStatusDisplay();
    void setStatus (int activeVoices, bool ready, double sampleRate);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    int voices = -1;
    bool isReady = false;
    double rate = 0.0;
};

// Stereo output meter with peak hold. All ballistics and the decibel scale come
// from the JUCE-free ui namespace, so they are unit-tested rather than tuned by
// eye inside paint().
class TaikorMeter final : public juce::Component
{
public:
    TaikorMeter();
    void setLevels (float leftLinear, float rightLinear);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static constexpr float floorDecibels = -48.0f;

private:
    taikor::ui::MeterBallistics leftBallistics;
    taikor::ui::MeterBallistics rightBallistics;
    float lastPaintedLeft = -1.0f;
    float lastPaintedRight = -1.0f;
    float lastPaintedLeftPeak = -1.0f;
    float lastPaintedRightPeak = -1.0f;
    int announcedClipState = -1;
};

class TaikorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TaikorAudioProcessorEditor (TaikorAudioProcessor&);
    ~TaikorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    static constexpr int octaveCount =
        taikor::highestOctaveOffset - taikor::lowestOctaveOffset + 1;
    static constexpr std::size_t totalPadCount =
        taikor::drumCount * taikor::articulationCount;

    struct LayoutAreas
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> gridArea;
        juce::Rectangle<int> rightUpperArea;
        juce::Rectangle<int> head;
        juce::Rectangle<int> drumDeck;
        juce::Rectangle<int> strokeDeck;
        juce::Rectangle<int> microphoneDeck;
    };

    void timerCallback() override;
    void selectOctave (int octaveOffset);
    [[nodiscard]] LayoutAreas calculateLayout() const;
    void addKnob (TaikorKnob& knob, const juce::String& parameterId,
                  const juce::String& description);

    TaikorAudioProcessor& audioProcessor;
    TaikorLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;

    juce::Label logoLabel;
    juce::Label editionLabel;
    TaikorStatusDisplay statusDisplay;
    TaikorMeter meter;
    juce::TextButton panicButton { "PANIC" };
    juce::Label sealLabel;

    juce::Label gridCaption;
    std::array<juce::Label, taikor::articulationCount> strokeLabels;
    TaikorHeadDisplay headDisplay;
    juce::Label headCaption;
    juce::Image drumAtlas;
    juce::Image backgroundPainting;

    std::array<std::unique_ptr<TaikorPad>, totalPadCount> pads;
    std::array<std::uint32_t, taikor::articulationCount> observedTriggerCounters {};

    std::array<std::unique_ptr<TaikorDrumButton>, octaveCount> octaveButtons;
    int selectedOctave = 0;

    juce::Label drumDeckLabel;
    juce::Label strokeDeckLabel;
    juce::Label microphoneDeckLabel;

    TaikorKnob sizeKnob { "DIAMETER", TaikorKnob::ValueStyle::Centimetres,
                          TaikorKnob::VisualRole::Drum };
    TaikorKnob depthKnob { "BODY DEPTH", TaikorKnob::ValueStyle::Percent,
                           TaikorKnob::VisualRole::Drum };
    TaikorKnob tensionKnob { "TENSION", TaikorKnob::ValueStyle::Percent,
                             TaikorKnob::VisualRole::Drum };
    TaikorKnob headMaterialKnob { "HEAD", TaikorKnob::ValueStyle::Percent,
                                  TaikorKnob::VisualRole::Drum };
    TaikorKnob shellMaterialKnob { "SHELL", TaikorKnob::ValueStyle::Percent,
                                   TaikorKnob::VisualRole::Drum };
    TaikorKnob resonantKnob { "RESONANT", TaikorKnob::ValueStyle::Percent,
                              TaikorKnob::VisualRole::Drum };
    TaikorKnob cavityKnob { "AIR COUPLING", TaikorKnob::ValueStyle::Percent,
                            TaikorKnob::VisualRole::Drum };
    TaikorKnob headDampingKnob { "DAMPING", TaikorKnob::ValueStyle::Percent,
                                 TaikorKnob::VisualRole::Drum };
    TaikorKnob shellResonanceKnob { "SHELL RING", TaikorKnob::ValueStyle::Percent,
                                    TaikorKnob::VisualRole::Drum };
    TaikorKnob pitchKnob { "PITCH", TaikorKnob::ValueStyle::Semitones,
                           TaikorKnob::VisualRole::Drum };

    TaikorKnob hardnessKnob { "BACHI", TaikorKnob::ValueStyle::Percent,
                              TaikorKnob::VisualRole::Stroke };
    TaikorKnob strikePositionKnob { "POSITION", TaikorKnob::ValueStyle::Plain,
                                    TaikorKnob::VisualRole::Stroke };
    TaikorKnob velocityDepthKnob { "VELOCITY", TaikorKnob::ValueStyle::Percent,
                                   TaikorKnob::VisualRole::Stroke };
    TaikorKnob tensionModKnob { "TENSION MOD", TaikorKnob::ValueStyle::Percent,
                                TaikorKnob::VisualRole::Stroke };
    TaikorKnob strikeNoiseKnob { "STICK NOISE", TaikorKnob::ValueStyle::Percent,
                                 TaikorKnob::VisualRole::Stroke };
    TaikorKnob humaniseKnob { "HUMANISE", TaikorKnob::ValueStyle::Percent,
                              TaikorKnob::VisualRole::Stroke };
    TaikorKnob octaveBodyKnob { "OCTAVE BODY", TaikorKnob::ValueStyle::Plain,
                                TaikorKnob::VisualRole::Stroke };

    TaikorKnob micDistanceKnob { "MIC DISTANCE", TaikorKnob::ValueStyle::Centimetres,
                                 TaikorKnob::VisualRole::Microphone };
    TaikorKnob micSpreadKnob { "MIC SPREAD", TaikorKnob::ValueStyle::Percent,
                               TaikorKnob::VisualRole::Microphone };
    TaikorKnob widthKnob { "WIDTH", TaikorKnob::ValueStyle::Percent,
                           TaikorKnob::VisualRole::Microphone };
    TaikorKnob driveKnob { "DRIVE", TaikorKnob::ValueStyle::Percent,
                           TaikorKnob::VisualRole::Master };
    TaikorKnob outputKnob { "OUTPUT", TaikorKnob::ValueStyle::Decibels,
                            TaikorKnob::VisualRole::Master };

    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaikorAudioProcessorEditor)
};

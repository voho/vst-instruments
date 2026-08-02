#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "DSP/YouKnow106Panel.h"

#include <array>
#include <memory>
#include <vector>

// Panel look. Everything is drawn rather than loaded: the faceplate texture,
// the slider slots and caps, and the lit buttons are all procedural, so the
// editor carries no image assets and scales cleanly to any window size.
class YouKnow106LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    YouKnow106LookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isHighlighted, bool isDown) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// The moulded plastic of the faceplate: a tiled, seeded speckle plus a fine
// horizontal grain. Generated once and tiled, because generating it per repaint
// would be visible as noise crawling under the controls.
class PlasticTexture
{
public:
    void ensureBuilt (int tileSize);
    void fill (juce::Graphics&, juce::Rectangle<int> area, juce::Colour base) const;

private:
    juce::Image tile;
};

// Voice indicators, modulation and envelope, drawn from the processor's
// display atomics.
class YouKnow106Display final : public juce::Component
{
public:
    void refresh (const YouKnow106AudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    int voiceMask = 0;
    int voices = 0;
    int voiceLimit = 6;
    float envelope = 0.0f;
    float lfo = 0.0f;
    double sampleRate = 0.0;
    int oversampling = 1;
    bool ready = false;
};

class YouKnow106AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit YouKnow106AudioProcessorEditor (YouKnow106AudioProcessor&);
    ~YouKnow106AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void buildPanelControls();
    void buildUtilityStrip();
    void attachSlider (juce::Slider&, const char* parameterId);
    void attachButton (juce::Button&, const char* parameterId);
    void attachRadio (juce::Button&, const char* parameterId, int value);
    [[nodiscard]] juce::Rectangle<float> scaled (float x, float y, float width,
                                                 float height) const;

    YouKnow106AudioProcessor& processor;
    YouKnow106LookAndFeel lookAndFeel;
    PlasticTexture texture;
    juce::TooltipWindow tooltipWindow { this, 600 };

    // One entry per panel table row, in the same order.
    struct PanelControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::Label> label;
    };
    std::array<PanelControl, youknow106::panel::controlCount> panelControls {};

    juce::Label logoLabel;
    juce::Label editionLabel;
    YouKnow106Display display;

    juce::TextButton panicButton { "PANIC" };
    juce::TextButton hqButton { "HQ" };
    juce::TextButton randomize10Button { "RANDOMIZE 10%" };
    juce::TextButton randomize100Button { "RANDOMIZE 100%" };
    // Sends the current panel out as a patch dump the hardware accepts.
    juce::TextButton sendSysExButton { "SEND" };
    juce::Slider transposeSlider;
    juce::Slider tuneSlider;
    juce::Slider velocitySlider;
    juce::Slider calibrationSlider;
    juce::Slider chorusNoiseSlider;
    juce::Slider polyphonySlider;
    std::array<juce::Label, 6> utilityLabels {};

    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> radioAttachments;

    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow106AudioProcessorEditor)
};

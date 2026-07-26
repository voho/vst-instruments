#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class NeuramarLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NeuramarLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class NeuramarKnob final : public juce::Component
{
public:
    NeuramarKnob (juce::String title, juce::String hint);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label titleLabel;
    juce::Label hintLabel;
};

class NeuralPoolDisplay final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    std::function<void()> onChooseFile;

    void setSnapshot (NeuramarAudioProcessor::LearningSnapshot next,
                      int activeVoiceCount, double sampleRate);
    void setDragHover (bool hovering);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    static juce::String stageTitle (NeuramarAudioProcessor::LearningStage stage);
    static juce::Colour stageColour (NeuramarAudioProcessor::LearningStage stage);

    NeuramarAudioProcessor::LearningSnapshot snapshot;
    int activeVoices = 0;
    double currentSampleRate = 0.0;
    bool dragHover = false;
    float animationPhase = 0.0f;
};

// Draws the DSP layer's ModelAnatomy reduction: what the learned memory
// actually contains, rather than a decorative animation. Clicking swaps
// between the spectral fingerprint and the layer-evolution view; hovering
// reads out the frequency and per-layer level under the cursor.
class ModelAnatomyDisplay final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    ModelAnatomyDisplay();

    void setAnatomy (const neuramar::ModelAnatomy& next,
                     std::uint64_t generation);
    void advance (int activeVoiceCount, float stretchAmount);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum class View
    {
        Spectrum,
        Evolution
    };

    [[nodiscard]] juce::Rectangle<float> plotBounds() const;
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> plot);
    void paintEvolution (juce::Graphics&, juce::Rectangle<float> plot);
    void paintGrid (juce::Graphics&, juce::Rectangle<float> plot) const;

    neuramar::ModelAnatomy anatomy;
    std::uint64_t anatomyGeneration = 0;
    View view = View::Spectrum;
    float playhead = 0.0f;
    float coreMeter = 0.0f;
    float airMeter = 0.0f;
    float boneMeter = 0.0f;
    float stretchAmount = 1.0f;
    float hoverPosition = -1.0f;
    bool hovering = false;
};

class NeuramarAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           public juce::FileDragAndDropTarget,
                                           private juce::Timer
{
public:
    explicit NeuramarAudioProcessorEditor (NeuramarAudioProcessor&);
    ~NeuramarAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void attachSlider (juce::Slider&, const char* parameterId);
    void attachButton (juce::Button&, const char* parameterId);
    void chooseSampleFile();
    void nudgeRoot (int semitones);
    void updateRootReadout (const NeuramarAudioProcessor::LearningSnapshot&);

    NeuramarAudioProcessor& neuramarProcessor;
    NeuramarLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label logoLabel;
    juce::Label taglineLabel;
    juce::Label modelLabel;
    juce::Label rootLabel;
    juce::Label rootValueLabel;
    juce::Label rootConfidenceLabel;

    NeuralPoolDisplay neuralPool;
    ModelAnatomyDisplay modelAnatomy;
    std::uint64_t lastAnatomyGeneration = 0;
    juce::TextButton loadButton { "DROP / OPEN" };
    juce::TextButton cancelButton { "CANCEL" };
    juce::TextButton panicButton { "PANIC" };
    juce::TextButton randomizeButton { "RANDOMIZE" };
    juce::TextButton randomize1Button { "1%" };
    juce::TextButton randomize10Button { "10%" };
    juce::TextButton randomize100Button { "100%" };
    juce::TextButton rootDownButton { "-" };
    juce::TextButton rootUpButton { "+" };
    juce::TextButton orbitButton { "ORBIT" };
    NeuramarAudioProcessor::RandomizationAmount randomizationAmount {
        NeuramarAudioProcessor::RandomizationAmount::Evolve10Percent
    };

    NeuramarKnob imprintKnob { "IMPRINT", "dream  /  faithful" };
    NeuramarKnob bodyLockKnob { "BODY LOCK", "follows  /  fixed" };
    NeuramarKnob airKnob { "AIR", "noise & breath" };
    NeuramarKnob boneKnob { "BONE", "modes & impact" };
    NeuramarKnob brightnessKnob { "GRAVITY", "dark  /  bright" };
    NeuramarKnob evolutionKnob { "MEMORY", "slow  /  rush" };
    NeuramarKnob mutationKnob { "MUTATION", "still  /  alive" };
    NeuramarKnob noiseKnob { "NOISE", "latent evolution" };
    NeuramarKnob attackKnob { "AWAKEN", "attack" };
    NeuramarKnob releaseKnob { "DISSOLVE", "release" };
    NeuramarKnob spreadKnob { "HORIZON", "stereo spread" };
    NeuramarKnob outputKnob { "OUTPUT", "level" };
    NeuramarKnob stretchKnob { "STRETCH", "ideal  /  stiff" };
    NeuramarKnob formantKnob { "FORMANT", "body size" };
    NeuramarKnob touchKnob { "TOUCH", "velocity colour" };
    NeuramarKnob registerKnob { "REGISTER", "key-tracked tilt" };

    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuramarAudioProcessorEditor)
};

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

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
    juce::TextButton loadButton { "DROP / OPEN" };
    juce::TextButton cancelButton { "CANCEL" };
    juce::TextButton panicButton { "PANIC" };
    juce::TextButton rootDownButton { "-" };
    juce::TextButton rootUpButton { "+" };
    juce::TextButton orbitButton { "ORBIT" };

    NeuramarKnob imprintKnob { "IMPRINT", "dream  /  faithful" };
    NeuramarKnob bodyLockKnob { "BODY LOCK", "follows  /  fixed" };
    NeuramarKnob airKnob { "AIR", "noise & breath" };
    NeuramarKnob boneKnob { "BONE", "modes & impact" };
    NeuramarKnob brightnessKnob { "GRAVITY", "dark  /  bright" };
    NeuramarKnob evolutionKnob { "MEMORY", "slow  /  rush" };
    NeuramarKnob mutationKnob { "MUTATION", "still  /  alive" };
    NeuramarKnob attackKnob { "AWAKEN", "attack" };
    NeuramarKnob releaseKnob { "DISSOLVE", "release" };
    NeuramarKnob spreadKnob { "HORIZON", "stereo spread" };
    NeuramarKnob outputKnob { "OUTPUT", "level" };

    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuramarAudioProcessorEditor)
};

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto background = 0xff061014u;
constexpr auto panel = 0xff0c1c21u;
constexpr auto panelRaised = 0xff10272du;
constexpr auto ink = 0xffdcefebu;
constexpr auto muted = 0xff6f8f8bu;
constexpr auto mint = 0xff61f4c5u;
constexpr auto violet = 0xff9d86ffu;
constexpr auto coral = 0xffff7467u;
constexpr auto amber = 0xffffce73u;

juce::Colour colour (juce::uint32 argb)
{
    return juce::Colour (argb);
}

juce::Font font (float height, bool bold = false)
{
    auto options = juce::FontOptions (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                float corner = 18.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), corner);
    g.setColour (colour (panel));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (colour (mint).withAlpha (0.10f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
}
} // namespace

NeuramarLookAndFeel::NeuramarLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colour (ink));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colour (mint));
    setColour (juce::Slider::rotarySliderOutlineColourId, colour (muted));
    setColour (juce::Label::textColourId, colour (ink));
    setColour (juce::TextButton::textColourOffId, colour (ink));
    setColour (juce::TextButton::textColourOnId, colour (background));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colour (0xffd7e9e4u));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colour (0xff102126u));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colour (0xff42605cu));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               colour (violet).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               colour (mint).withAlpha (0.78f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, colour (0xff57746fu));
}

void NeuramarLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                             int width, int height, float sliderPos,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto diameter = static_cast<float> (std::min (width, height)) - 12.0f;
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height))
                            .withSizeKeepingCentre (diameter, diameter);
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colours::black.withAlpha (0.50f));
    g.fillEllipse (bounds.translated (0.0f, 3.0f));

    juce::ColourGradient shell (colour (0xff213a40u), bounds.getX(), bounds.getY(),
                                colour (0xff091519u), bounds.getRight(), bounds.getBottom(), false);
    shell.addColour (0.50, colour (0xff173138u));
    g.setGradientFill (shell);
    g.fillEllipse (bounds);

    const auto arcBounds = bounds.expanded (3.0f);
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcBounds.getWidth() * 0.5f,
                         arcBounds.getHeight() * 0.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colour (muted).withAlpha (0.24f));
    g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcBounds.getWidth() * 0.5f,
                            arcBounds.getHeight() * 0.5f, 0.0f,
                            rotaryStartAngle, angle, true);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (valueArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    const auto pointerLength = radius * 0.58f;
    const auto pointerWidth = std::max (2.0f, radius * 0.075f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -pointerLength,
                                 pointerWidth, pointerLength * 0.72f,
                                 pointerWidth * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    g.setColour (accent);
    g.fillPath (pointer);
    g.setColour (colour (ink).withAlpha (0.30f));
    g.drawEllipse (bounds.reduced (1.0f), 1.0f);
}

void NeuramarLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour&,
                                                 bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    auto fill = active ? colour (mint) : colour (panelRaised);
    if (down)
        fill = fill.interpolatedWith (juce::Colours::black, 0.20f);
    else if (highlighted)
        fill = fill.brighter (0.10f);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 8.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour ((active ? colour (mint) : colour (muted)).withAlpha (active ? 0.92f : 0.32f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void NeuramarLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? colour (background) : colour (ink));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 2),
                      juce::Justification::centred, 1);
}

void NeuramarLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(),
                      std::max (1, label.getMinimumHorizontalScale() < 1.0f ? 2 : 1),
                      label.getMinimumHorizontalScale());
}

juce::Label* NeuramarLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (10.5f, true));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

juce::Font NeuramarLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jlimit (10.0f, 13.0f, static_cast<float> (buttonHeight) * 0.30f), true);
}

NeuramarKnob::NeuramarKnob (juce::String title, juce::String hint)
{
    slider.setTitle (title);
    slider.setDescription (hint);
    slider.setTooltip (title + " - " + hint);

    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setFont (font (11.5f, true));
    titleLabel.setColour (juce::Label::textColourId, colour (ink));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    hintLabel.setText (hint, juce::dontSendNotification);
    hintLabel.setFont (font (8.5f));
    hintLabel.setColour (juce::Label::textColourId, colour (muted));
    hintLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hintLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f,
                                juce::MathConstants<float>::pi * 2.78f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour (mint));
    addAndMakeVisible (slider);
}

void NeuramarKnob::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds (area.removeFromTop (18));
    hintLabel.setBounds (area.removeFromBottom (13));
    slider.setBounds (area);
}

void NeuralPoolDisplay::setSnapshot (NeuramarAudioProcessor::LearningSnapshot next,
                                     int activeVoiceCount, double sampleRate)
{
    snapshot = std::move (next);
    activeVoices = activeVoiceCount;
    currentSampleRate = sampleRate;
    animationPhase = std::fmod (animationPhase + 0.0165f, 1.0f);
    repaint();
}

void NeuralPoolDisplay::setDragHover (bool hovering)
{
    if (dragHover != hovering)
    {
        dragHover = hovering;
        repaint();
    }
}

juce::String NeuralPoolDisplay::stageTitle (NeuramarAudioProcessor::LearningStage stage)
{
    using Stage = NeuramarAudioProcessor::LearningStage;
    switch (stage)
    {
        case Stage::Empty:       return "DROP A SOUND INTO THE POOL";
        case Stage::Reading:     return "LISTENING";
        case Stage::FindingRoot: return "FINDING THE ROOT";
        case Stage::Analysing:   return "SEPARATING CORE / AIR / BONE";
        case Stage::Training:    return "DREAMING A PLAYABLE MEMORY";
        case Stage::Ready:       return "MEMORY ALIVE";
        case Stage::Cancelled:   return "LEARNING CANCELLED";
        case Stage::Error:       return "THE POOL COULD NOT LEARN THIS SOUND";
    }
    return {};
}

juce::Colour NeuralPoolDisplay::stageColour (NeuramarAudioProcessor::LearningStage stage)
{
    using Stage = NeuramarAudioProcessor::LearningStage;
    if (stage == Stage::Error)
        return colour (coral);
    if (stage == Stage::Ready)
        return colour (mint);
    if (stage == Stage::Cancelled || stage == Stage::Empty)
        return colour (violet);
    return colour (amber);
}

void NeuralPoolDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = dragHover ? colour (mint) : stageColour (snapshot.stage);

    juce::ColourGradient poolGradient (colour (0xff10282fu), bounds.getX(), bounds.getY(),
                                       colour (0xff071519u), bounds.getRight(),
                                       bounds.getBottom(), false);
    poolGradient.addColour (0.46, colour (0xff0b2025u));
    g.setGradientFill (poolGradient);
    g.fillRoundedRectangle (bounds, 22.0f);

    g.setColour (accent.withAlpha (dragHover ? 0.88f : 0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.8f), 22.0f, dragHover ? 2.4f : 1.2f);

    auto graph = bounds.reduced (22.0f);
    graph.removeFromTop (52.0f);
    graph.removeFromBottom (50.0f);
    const auto centre = graph.getCentre();

    // A subtle pitch/time lattice makes the learned memory feel spatial while
    // keeping all meaningful state readable without relying on a bitmap.
    g.setColour (colour (mint).withAlpha (0.045f));
    for (int i = 1; i < 8; ++i)
    {
        const auto x = graph.getX() + graph.getWidth() * static_cast<float> (i) / 8.0f;
        g.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
    }
    for (int i = 1; i < 5; ++i)
    {
        const auto y = graph.getY() + graph.getHeight() * static_cast<float> (i) / 5.0f;
        g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
    }

    const auto hasWaveform = std::any_of (snapshot.waveform.begin(), snapshot.waveform.end(),
                                         [] (float value) { return std::abs (value) > 1.0e-5f; });
    if (hasWaveform)
    {
        juce::Path waveform;
        for (std::size_t i = 0; i < snapshot.waveform.size(); ++i)
        {
            const auto proportion = static_cast<float> (i)
                                  / static_cast<float> (snapshot.waveform.size() - 1);
            const auto x = graph.getX() + graph.getWidth() * proportion;
            const auto value = juce::jlimit (-1.0f, 1.0f, snapshot.waveform[i]);
            const auto y = centre.y - value * graph.getHeight() * 0.31f;
            if (i == 0)
                waveform.startNewSubPath (x, y);
            else
                waveform.lineTo (x, y);
        }
        g.setColour (accent.withAlpha (0.12f));
        g.strokePath (waveform, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved));
        g.setColour (accent.withAlpha (0.78f));
        g.strokePath (waveform, juce::PathStrokeType (1.45f, juce::PathStrokeType::curved));
    }

    if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Ready)
    {
        constexpr int nodeCount = 18;
        std::array<juce::Point<float>, nodeCount> nodes {};
        for (int i = 0; i < nodeCount; ++i)
        {
            const auto lane = i % 6;
            const auto row = i / 6;
            const auto x = graph.getX() + graph.getWidth()
                         * (0.08f + 0.84f * static_cast<float> (lane) / 5.0f);
            const auto wobble = std::sin ((animationPhase * 2.0f
                                           + static_cast<float> (i) * 0.137f)
                                          * juce::MathConstants<float>::pi);
            const auto y = graph.getY() + graph.getHeight()
                         * (0.22f + 0.28f * static_cast<float> (row))
                         + wobble * (2.0f + static_cast<float> (activeVoices));
            nodes[static_cast<std::size_t> (i)] = { x, y };
        }

        for (int i = 0; i < nodeCount; ++i)
        {
            if (i + 1 < nodeCount && (i + 1) % 6 != 0)
            {
                g.setColour ((i % 3 == 0 ? colour (violet) : colour (mint))
                                 .withAlpha (0.10f + 0.025f
                                     * static_cast<float> (activeVoices)));
                g.drawLine ({ nodes[static_cast<std::size_t> (i)],
                              nodes[static_cast<std::size_t> (i + 1)] }, 1.0f);
            }
            if (i + 6 < nodeCount)
            {
                g.setColour (colour (violet).withAlpha (0.08f));
                g.drawLine ({ nodes[static_cast<std::size_t> (i)],
                              nodes[static_cast<std::size_t> (i + 6)] }, 0.8f);
            }
        }

        for (int i = 0; i < nodeCount; ++i)
        {
            const auto pulse = 2.0f + 1.2f * std::max (0.0f, std::sin (
                (animationPhase + static_cast<float> (i) * 0.071f)
                * juce::MathConstants<float>::twoPi));
            g.setColour ((i % 3 == 0 ? colour (violet) : colour (mint)).withAlpha (0.80f));
            g.fillEllipse (juce::Rectangle<float> (pulse, pulse)
                               .withCentre (nodes[static_cast<std::size_t> (i)]));
        }
    }

    const auto titleBounds = bounds.reduced (22.0f).removeFromTop (34.0f);
    g.setColour (accent);
    g.setFont (font (15.0f, true));
    g.drawText (stageTitle (snapshot.stage), titleBounds,
                juce::Justification::centredLeft, false);

    auto chip = bounds.reduced (18.0f).removeFromTop (28.0f).removeFromRight (132.0f);
    g.setColour (accent.withAlpha (0.10f));
    g.fillRoundedRectangle (chip, 9.0f);
    g.setColour (accent.withAlpha (0.72f));
    g.setFont (font (9.5f, true));
    const auto memoryText = snapshot.modelGeneration > 0
        ? "M" + juce::String (snapshot.modelGeneration) + "  /  "
        : juce::String {};
    const auto engineText = currentSampleRate > 0.0
        ? memoryText + juce::String (activeVoices) + " VOICES  /  "
            + juce::String (juce::roundToInt (currentSampleRate / 1000.0)) + "K"
        : memoryText + "ENGINE ASLEEP";
    g.drawText (engineText, chip, juce::Justification::centred, false);

    if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Reading
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::FindingRoot
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::Analysing
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::Training)
    {
        const auto ringBounds = juce::Rectangle<float> (72.0f, 72.0f).withCentre (centre);
        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, 34.0f, 34.0f, 0.0f,
                            -juce::MathConstants<float>::halfPi,
                            -juce::MathConstants<float>::halfPi
                                + juce::MathConstants<float>::twoPi
                                      * juce::jlimit (0.0f, 1.0f, snapshot.progress),
                            true);
        g.setColour (accent.withAlpha (0.18f));
        g.drawEllipse (ringBounds, 4.0f);
        g.setColour (accent);
        g.strokePath (ring, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setFont (font (13.0f, true));
        g.drawText (juce::String (juce::roundToInt (snapshot.progress * 100.0f)) + "%",
                    ringBounds, juce::Justification::centred, false);
    }
    else if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Empty)
    {
        g.setColour (accent.withAlpha (dragHover ? 0.95f : 0.62f));
        g.setFont (font (36.0f, true));
        g.drawText ("+", juce::Rectangle<float> (80.0f, 52.0f).withCentre (centre),
                    juce::Justification::centred, false);
    }

    auto footer = bounds.reduced (22.0f).removeFromBottom (34.0f);
    g.setColour (colour (muted));
    g.setFont (font (10.5f));
    auto message = snapshot.message;
    if (message.isEmpty())
        message = snapshot.stage == NeuramarAudioProcessor::LearningStage::Empty
            ? "WAV / AIFF / FLAC / OGG  -  monophonic sounds reveal the clearest roots"
            : snapshot.sourceName;
    g.drawFittedText (message, footer.toNearestInt(), juce::Justification::centredLeft,
                      1, 0.76f);
}

void NeuralPoolDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && onChooseFile)
        onChooseFile();
}

NeuramarAudioProcessorEditor::NeuramarAudioProcessorEditor (NeuramarAudioProcessor& p)
    : AudioProcessorEditor (&p), neuramarProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("NEURAMAR", juce::dontSendNotification);
    logoLabel.setFont (font (29.0f, true));
    logoLabel.setColour (juce::Label::textColourId, colour (ink));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    taglineLabel.setText ("TEACH A SOUND. PLAY ITS MEMORY.", juce::dontSendNotification);
    taglineLabel.setFont (font (10.5f, true));
    taglineLabel.setColour (juce::Label::textColourId, colour (mint).withAlpha (0.76f));
    taglineLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (taglineLabel);

    modelLabel.setText ("DDSP-INSPIRED  /  NO CLOUD  /  NO SAMPLE PLAYBACK",
                        juce::dontSendNotification);
    modelLabel.setFont (font (9.5f, true));
    modelLabel.setColour (juce::Label::textColourId, colour (muted));
    modelLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (modelLabel);

    rootLabel.setText ("INFERRED ROOT", juce::dontSendNotification);
    rootLabel.setFont (font (10.5f, true));
    rootLabel.setColour (juce::Label::textColourId, colour (muted));
    rootLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootLabel);

    rootValueLabel.setText ("--", juce::dontSendNotification);
    rootValueLabel.setFont (font (29.0f, true));
    rootValueLabel.setColour (juce::Label::textColourId, colour (mint));
    rootValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootValueLabel);

    rootConfidenceLabel.setText ("drop a sound to begin", juce::dontSendNotification);
    rootConfidenceLabel.setFont (font (9.0f));
    rootConfidenceLabel.setColour (juce::Label::textColourId, colour (muted));
    rootConfidenceLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootConfidenceLabel);

    neuralPool.onChooseFile = [this] { chooseSampleFile(); };
    neuralPool.setAccessible (true);
    neuralPool.setTitle ("Neural sound drop area");
    neuralPool.setDescription (
        "Drop or choose an audio file to infer its root and learn a playable synthesis model.");
    neuralPool.setTooltip ("Drop a supported audio file here, or click to choose one.");
    addAndMakeVisible (neuralPool);

    orbitButton.setClickingTogglesState (true);
    orbitButton.setDescription ("Repeat the learned stable region while a note is held.");
    orbitButton.setTooltip ("Revisit the learned stable region while a note is held.");
    loadButton.setTooltip ("Open a WAV, AIFF, FLAC, or OGG sound for local learning.");
    cancelButton.setTooltip ("Cancel the current background analysis and training pass.");
    panicButton.setTooltip ("Immediately stop all sounding voices.");
    rootDownButton.setTooltip ("Correct the inferred root down by one semitone.");
    rootUpButton.setTooltip ("Correct the inferred root up by one semitone.");

    for (auto* button : std::array<juce::Button*, 6> {
             &loadButton, &cancelButton, &panicButton,
             &rootDownButton, &rootUpButton, &orbitButton })
        addAndMakeVisible (*button);

    loadButton.onClick = [this] { chooseSampleFile(); };
    cancelButton.onClick = [this]
    {
        neuramarProcessor.requestLearningCancellation();
    };
    panicButton.onClick = [this] { neuramarProcessor.requestPanic(); };
    rootDownButton.onClick = [this] { nudgeRoot (-1); };
    rootUpButton.onClick = [this] { nudgeRoot (1); };

    for (auto* knob : std::array<NeuramarKnob*, 11> {
             &imprintKnob, &bodyLockKnob, &airKnob, &boneKnob,
             &brightnessKnob, &evolutionKnob, &mutationKnob,
             &attackKnob, &releaseKnob, &spreadKnob, &outputKnob })
        addAndMakeVisible (*knob);

    bodyLockKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    boneKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (coral));
    brightnessKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));
    evolutionKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    mutationKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (coral));
    outputKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));

    attachSlider (imprintKnob.slider, neuramar::parameters::imprint);
    attachSlider (bodyLockKnob.slider, neuramar::parameters::bodyLock);
    attachSlider (airKnob.slider, neuramar::parameters::air);
    attachSlider (boneKnob.slider, neuramar::parameters::bone);
    attachSlider (brightnessKnob.slider, neuramar::parameters::brightness);
    attachSlider (evolutionKnob.slider, neuramar::parameters::evolutionRate);
    attachSlider (mutationKnob.slider, neuramar::parameters::mutation);
    attachSlider (attackKnob.slider, neuramar::parameters::attack);
    attachSlider (releaseKnob.slider, neuramar::parameters::release);
    attachSlider (spreadKnob.slider, neuramar::parameters::spread);
    attachSlider (outputKnob.slider, neuramar::parameters::output);
    attachButton (orbitButton, neuramar::parameters::orbit);

    keyboard.setAvailableRange (0, 127);
    keyboard.setLowestVisibleKey (24);
    keyboard.setKeyWidth (18.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setTitle ("Playable MIDI keyboard");
    keyboard.setDescription ("Play the learned neural instrument with the mouse.");
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setSizeLimits (960, 619, 1500, 966);
        constrainer->setFixedAspectRatio (1180.0 / 760.0);
    }
    setSize (1180, 760);
    timerCallback();
    startTimerHz (30);
}

NeuramarAudioProcessorEditor::~NeuramarAudioProcessorEditor()
{
    stopTimer();
    fileChooser.reset();
    setLookAndFeel (nullptr);
}

void NeuramarAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour (background));

    const auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient glow (colour (mint).withAlpha (0.10f),
                               bounds.getWidth() * 0.30f, 0.0f,
                               juce::Colours::transparentBlack,
                               bounds.getWidth() * 0.30f, bounds.getHeight() * 0.72f,
                               true);
    g.setGradientFill (glow);
    g.fillRect (bounds);

    auto content = getLocalBounds().reduced (18);
    content.removeFromTop (58);
    content.removeFromBottom (99);
    const auto upper = content.removeFromTop (juce::roundToInt (
        static_cast<float> (content.getHeight()) * 0.67f));
    const auto poolWidth = juce::roundToInt (
        static_cast<float> (upper.getWidth()) * 0.665f);
    drawPanel (g, upper.withWidth (poolWidth).toFloat());
    drawPanel (g, upper.withTrimmedLeft (poolWidth + 12).toFloat());
    if (content.getHeight() > 0)
        drawPanel (g, content.withTrimmedTop (10).toFloat());

    g.setColour (colour (mint).withAlpha (0.16f));
    g.drawHorizontalLine (64, 18.0f, static_cast<float> (getWidth() - 18));

    g.setColour (colour (muted).withAlpha (0.45f));
    g.setFont (font (8.0f, true));
    g.drawText ("CORE", getWidth() - 175, 48, 42, 12, juce::Justification::centred, false);
    g.setColour (colour (mint));
    g.fillEllipse (static_cast<float> (getWidth() - 132), 52.0f, 4.0f, 4.0f);
    g.setColour (colour (muted).withAlpha (0.45f));
    g.drawText ("AIR", getWidth() - 124, 48, 34, 12, juce::Justification::centred, false);
    g.setColour (colour (coral));
    g.fillEllipse (static_cast<float> (getWidth() - 88), 52.0f, 4.0f, 4.0f);
    g.setColour (colour (muted).withAlpha (0.45f));
    g.drawText ("BONE", getWidth() - 80, 48, 45, 12, juce::Justification::centred, false);
}

void NeuramarAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    auto header = bounds.removeFromTop (58);
    auto logo = header.removeFromLeft (230);
    logoLabel.setBounds (logo.removeFromTop (36));
    taglineLabel.setBounds (logo);
    modelLabel.setBounds (header.withTrimmedBottom (10));

    auto keyboardArea = bounds.removeFromBottom (89);
    keyboard.setBounds (keyboardArea.withTrimmedTop (7));

    auto upper = bounds.removeFromTop (juce::roundToInt (
        static_cast<float> (bounds.getHeight()) * 0.67f));
    const auto poolWidth = juce::roundToInt (
        static_cast<float> (upper.getWidth()) * 0.665f);
    auto poolArea = upper.removeFromLeft (poolWidth);
    neuralPool.setBounds (poolArea.reduced (8));

    upper.removeFromLeft (12);
    auto dnaArea = upper.reduced (10);
    auto rootArea = dnaArea.removeFromTop (76);
    rootLabel.setBounds (rootArea.removeFromTop (18));
    auto rootLine = rootArea.removeFromTop (36);
    rootDownButton.setBounds (rootLine.removeFromLeft (38).reduced (4));
    rootUpButton.setBounds (rootLine.removeFromRight (38).reduced (4));
    rootValueLabel.setBounds (rootLine);
    rootConfidenceLabel.setBounds (rootArea);

    dnaArea.removeFromTop (2);
    constexpr int columns = 3;
    const auto cellWidth = dnaArea.getWidth() / columns;
    const auto cellHeight = dnaArea.getHeight() / 2;
    std::array<NeuramarKnob*, 6> dnaKnobs {
        &imprintKnob, &bodyLockKnob, &airKnob,
        &boneKnob, &brightnessKnob, &mutationKnob
    };
    for (int i = 0; i < static_cast<int> (dnaKnobs.size()); ++i)
    {
        const auto row = i / columns;
        const auto column = i % columns;
        dnaKnobs[static_cast<std::size_t> (i)]->setBounds (
            dnaArea.getX() + column * cellWidth,
            dnaArea.getY() + row * cellHeight,
            cellWidth, cellHeight);
    }

    bounds.removeFromTop (10);
    auto performance = bounds.reduced (10, 8);
    auto actions = performance.removeFromRight (juce::jmax (126, performance.getWidth() / 7));
    auto firstActionRow = actions.removeFromTop (actions.getHeight() / 2);
    orbitButton.setBounds (firstActionRow.removeFromLeft (firstActionRow.getWidth() / 2).reduced (4));
    loadButton.setBounds (firstActionRow.reduced (4));
    auto secondActionRow = actions;
    cancelButton.setBounds (secondActionRow.removeFromLeft (secondActionRow.getWidth() / 2).reduced (4));
    panicButton.setBounds (secondActionRow.reduced (4));

    std::array<NeuramarKnob*, 5> performanceKnobs {
        &evolutionKnob, &attackKnob, &releaseKnob, &spreadKnob, &outputKnob
    };
    const auto performanceWidth = performance.getWidth()
                                / static_cast<int> (performanceKnobs.size());
    for (int i = 0; i < static_cast<int> (performanceKnobs.size()); ++i)
        performanceKnobs[static_cast<std::size_t> (i)]->setBounds (
            performance.getX() + i * performanceWidth, performance.getY(),
            performanceWidth, performance.getHeight());
}

bool NeuramarAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& path)
    {
        return NeuramarAudioProcessor::isSupportedSampleFile (juce::File (path));
    });
}

void NeuramarAudioProcessorEditor::filesDropped (const juce::StringArray& files,
                                                  int, int)
{
    neuralPool.setDragHover (false);
    for (const auto& path : files)
    {
        const juce::File file (path);
        if (NeuramarAudioProcessor::isSupportedSampleFile (file))
        {
            neuramarProcessor.loadSampleFile (file);
            return;
        }
    }
}

void NeuramarAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    neuralPool.setDragHover (true);
}

void NeuramarAudioProcessorEditor::fileDragExit (const juce::StringArray&)
{
    neuralPool.setDragHover (false);
}

void NeuramarAudioProcessorEditor::timerCallback()
{
    const auto snapshot = neuramarProcessor.getLearningSnapshot();
    neuralPool.setSnapshot (snapshot, neuramarProcessor.getActiveVoiceCount(),
                            neuramarProcessor.getCurrentSampleRateForDisplay());
    updateRootReadout (snapshot);

    const auto learning = snapshot.stage == NeuramarAudioProcessor::LearningStage::Reading
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::FindingRoot
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::Analysing
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::Training;
    cancelButton.setEnabled (learning);
    cancelButton.setAlpha (learning ? 1.0f : 0.42f);
}

void NeuramarAudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                  const char* parameterId)
{
    sliderAttachments.push_back (
        std::make_unique<SliderAttachment> (neuramarProcessor.parameters, parameterId, slider));
}

void NeuramarAudioProcessorEditor::attachButton (juce::Button& button,
                                                  const char* parameterId)
{
    buttonAttachments.push_back (
        std::make_unique<ButtonAttachment> (neuramarProcessor.parameters, parameterId, button));
}

void NeuramarAudioProcessorEditor::chooseSampleFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Teach Neuramar a sound", juce::File {}, "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<NeuramarAudioProcessorEditor> safeThis (this);
    fileChooser->launchAsync (flags, [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;
        const auto file = chooser.getResult();
        if (file.existsAsFile())
            safeThis->neuramarProcessor.loadSampleFile (file);
        safeThis->fileChooser.reset();
    });
}

void NeuramarAudioProcessorEditor::nudgeRoot (int semitones)
{
    auto* parameter = neuramarProcessor.parameters.getParameter (
        neuramar::parameters::rootCorrection);
    if (parameter == nullptr)
        return;

    const auto current = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
    const auto next = juce::jlimit (-12, 12, current + semitones);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (next)));
    parameter->endChangeGesture();
}

void NeuramarAudioProcessorEditor::updateRootReadout (
    const NeuramarAudioProcessor::LearningSnapshot& snapshot)
{
    if (snapshot.rootMidiNote < 0)
    {
        rootValueLabel.setText ("--", juce::dontSendNotification);
        rootConfidenceLabel.setText ("drop a sound to begin", juce::dontSendNotification);
        rootDownButton.setEnabled (false);
        rootUpButton.setEnabled (false);
        return;
    }

    auto correction = 0;
    if (const auto* raw = neuramarProcessor.parameters.getRawParameterValue (
            neuramar::parameters::rootCorrection))
        correction = juce::roundToInt (raw->load (std::memory_order_relaxed));

    const auto correctedNote = juce::jlimit (0, 127, snapshot.rootMidiNote + correction);
    const auto centsText = (snapshot.rootCents >= 0.0f ? "+" : "")
                         + juce::String (juce::roundToInt (snapshot.rootCents)) + "c";
    rootValueLabel.setText (NeuramarAudioProcessor::noteName (correctedNote)
                                + "  " + centsText,
                            juce::dontSendNotification);
    rootConfidenceLabel.setText (
        juce::String (juce::roundToInt (snapshot.rootConfidence * 100.0f))
            + "% confidence"
            + (correction == 0 ? juce::String {}
                               : "  /  corrected "
                                     + juce::String (correction > 0 ? "+" : "")
                                     + juce::String (correction) + " st"),
        juce::dontSendNotification);
    rootDownButton.setEnabled (correction > -12);
    rootUpButton.setEnabled (correction < 12);
}

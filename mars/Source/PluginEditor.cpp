#include "PluginEditor.h"

#include <cmath>
#include <utility>

namespace
{
constexpr auto accent = 0xff6fffd5;
constexpr auto accentBlue = 0xff66baff;
constexpr auto ultraviolet = 0xffa477ff;
constexpr auto panel = 0xff10151d;
constexpr auto panelEdge = 0xff28313e;
constexpr auto textBright = 0xffedf7f5;
constexpr auto textDim = 0xff7f909d;

juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }

void styleHeaderLabel (juce::Label& label, float size, juce::Colour colour)
{
    label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, colour);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

MarsLookAndFeel::MarsLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, c (textBright));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, c (accent));
    setColour (juce::Slider::rotarySliderOutlineColourId, c (panelEdge));
    setColour (juce::TextButton::textColourOffId, c (textDim));
    setColour (juce::TextButton::textColourOnId, c (textBright));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, c (0xffd9e1df));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, c (0xff0b0f15));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, c (0xff35414d));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, c (0x406fffd5));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, c (0xb066baff));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, c (0xff5e6d77));
}

void MarsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = static_cast<float> (juce::jmin (width, height)) - 13.0f;
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (static_cast<float> (x) + width * 0.5f,
                                            static_cast<float> (y) + height * 0.5f - 1.0f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                            rotaryStartAngle, angle, true);
    juce::ColourGradient glow (c (accentBlue), bounds.getBottomLeft(), c (accent),
                               bounds.getTopRight(), false);
    g.setGradientFill (glow);
    g.strokePath (valueArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.setColour (c (0xff080c12));
    g.fillEllipse (bounds.reduced (8.0f));
    g.setColour (c (0xff303b47));
    g.drawEllipse (bounds.reduced (8.0f), 1.0f);

    juce::Path pointer;
    const auto pointerLength = radius * 0.46f;
    pointer.addRoundedRectangle (-1.3f, -pointerLength, 2.6f, pointerLength * 0.55f, 1.3f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (c (textBright));
    g.fillPath (pointer);
}

void MarsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    auto fill = active ? c (0xff173a3c) : c (0xff111720);
    if (highlighted) fill = fill.brighter (0.07f);
    if (down) fill = fill.darker (0.10f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (active ? c (accent) : c (panelEdge));
    g.drawRoundedRectangle (bounds, 5.0f, active ? 1.4f : 1.0f);
}

juce::Font MarsLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (13.0f, buttonHeight * 0.38f),
                                          juce::Font::bold));
}

void MarsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style != juce::Slider::LinearHorizontal)
        return;

    const auto cy = static_cast<float> (y + height / 2);
    const auto start = static_cast<float> (x + 3);
    const auto end = static_cast<float> (x + width - 3);
    g.setColour (c (panelEdge));
    g.drawLine (start, cy, end, cy, 4.0f);
    g.setColour (c (accent));
    g.drawLine (start, cy, sliderPos, cy, 4.0f);
    g.setColour (c (textBright));
    g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
}

MarsChoiceStrip::MarsChoiceStrip (juce::String title, juce::StringArray choices)
    : titleText (std::move (title))
{
    setName (titleText);
    setTitle (titleText);

    for (int index = 0; index < choices.size(); ++index)
    {
        auto button = std::make_unique<juce::TextButton> (choices[index]);
        button->setName (titleText + " " + choices[index]);
        button->setTitle (choices[index]);
        button->setDescription ("Select " + choices[index] + " for " + titleText.toLowerCase());
        button->setClickingTogglesState (false);
        button->onClick = [this, index]
        {
            setSelectedIndex (index);
            if (onChoice) onChoice (index);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }

    setSelectedIndex (0);
}

void MarsChoiceStrip::setSelectedIndex (int newIndex)
{
    selectedIndex = juce::jlimit (0, static_cast<int> (buttons.size()) - 1, newIndex);
    for (int i = 0; i < static_cast<int> (buttons.size()); ++i)
        buttons[static_cast<size_t> (i)]->setToggleState (i == selectedIndex,
                                                         juce::dontSendNotification);
}

void MarsChoiceStrip::paint (juce::Graphics& g)
{
    g.setColour (c (textDim));
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (titleText, getLocalBounds().removeFromTop (16), juce::Justification::centredLeft);
}

void MarsChoiceStrip::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (19);
    const auto count = static_cast<int> (buttons.size());
    for (int i = 0; i < count; ++i)
    {
        const auto remaining = count - i;
        auto buttonArea = area.removeFromLeft (area.getWidth() / remaining);
        buttons[static_cast<size_t> (i)]->setBounds (buttonArea.reduced (i == 0 ? 0 : 2, 0));
    }
}

MarsKnob::MarsKnob (juce::String name, juce::String suffix)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 19);
    slider.setNumDecimalPlacesToDisplay (suffix == "dB" ? 1 : 0);
    if (suffix == "%")
    {
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (juce::roundToInt (value * 100.0)) + "%";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue() / 100.0;
        };
    }
    else
    {
        slider.setTextValueSuffix (suffix);
    }
    slider.setName (name);
    slider.setTitle (name);
    slider.setDescription ("Adjust " + name.toLowerCase());
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);
}

void MarsKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (17));
    slider.setBounds (area);
}

void MarsStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    repaint();
}

void MarsStatusDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (c (panel));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (c (panelEdge));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto light = juce::Rectangle<float> (bounds.getX() + 11.0f,
                                               bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (isReady ? c (accent) : c (0xff59616a));
    g.fillEllipse (light);
    if (isReady)
    {
        g.setColour (c (0x306fffd5));
        g.fillEllipse (light.expanded (5.0f));
    }

    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.setColour (c (textBright));
    g.drawText (juce::String (juce::jmax (0, voices)).paddedLeft ('0', 2) + " VOICES",
                28, 0, 77, getHeight(), juce::Justification::centredLeft);
    g.setColour (c (textDim));
    const auto rateText = rate > 0.0 ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz" : "OFFLINE";
    g.drawText (rateText, 105, 0, getWidth() - 114, getHeight(), juce::Justification::centredRight);
}

MarsAudioProcessorEditor::MarsAudioProcessorEditor (MarsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("MARS", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 25.0f, c (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Mars human voice synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("HUMAN VOICE INSTRUMENT  ·  DUAL-STAGE ENGINE", juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 10.5f, c (textDim));
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);
    panicButton.setColour (juce::TextButton::textColourOffId, c (0xffffa4ad));
    panicButton.setName ("Panic — stop all voices");
    panicButton.setDescription ("Immediately release every sounding voice");
    panicButton.onClick = [this]
    {
        processor.keyboardState.reset();
        processor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    for (auto* strip : { &profileStrip, &modeStrip, &chordStrip, &vowelStrip })
        addAndMakeVisible (*strip);

    choirSizeLabel.setText ("STACK DEPTH", juce::dontSendNotification);
    choirSizeLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    choirSizeLabel.setColour (juce::Label::textColourId, c (textDim));
    choirSizeLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (choirSizeLabel);

    choirSizeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    choirSizeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    choirSizeSlider.setName ("Stack depth");
    choirSizeSlider.setTitle ("Stack depth");
    choirSizeSlider.setDescription ("Number of independently aged oscillator cards");
    addAndMakeVisible (choirSizeSlider);

    for (auto* knob : { &breathKnob, &resonanceKnob, &vibratoKnob, &humanizeKnob,
                        &spreadKnob, &tensionKnob, &roomKnob, &outputKnob })
        addAndMakeVisible (*knob);

    keyboard.setAvailableRange (36, 84);
    keyboard.setLowestVisibleKey (43);
    keyboard.setKeyWidth (25.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setKeyPressBaseOctave (3);
    keyboard.setName ("Playable piano keyboard");
    keyboard.setTitle ("Playable keyboard");
    keyboard.setDescription ("Play notes with the mouse or computer keyboard");
    addAndMakeVisible (keyboard);

    keyboardHintLabel.setText ("PLAY  C3–C7   ·   COMPUTER KEYS: A W S E D F T G Y H U J K",
                               juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    keyboardHintLabel.setColour (juce::Label::textColourId, c (textDim));
    keyboardHintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (keyboardHintLabel);

    attachChoice (profileStrip, mars::parameters::profile, profileAttachment);
    attachChoice (modeStrip, mars::parameters::mode, modeAttachment);
    attachChoice (chordStrip, mars::parameters::chordQuality, chordAttachment);
    attachChoice (vowelStrip, mars::parameters::vowel, vowelAttachment);

    choirSizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::choirSize, choirSizeSlider);
    breathAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::breath, breathKnob.slider);
    resonanceAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::resonance, resonanceKnob.slider);
    vibratoAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::vibrato, vibratoKnob.slider);
    humanizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::humanize, humanizeKnob.slider);
    spreadAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::spread, spreadKnob.slider);
    tensionAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::tension, tensionKnob.slider);
    roomAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::room, roomKnob.slider);
    outputAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, mars::parameters::output, outputKnob.slider);

    setResizable (true, true);
    setResizeLimits (900, 620, 1500, 960);
    setSize (1160, 750);
    startTimerHz (12);
    updateConditionalControls();
}

MarsAudioProcessorEditor::~MarsAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MarsAudioProcessorEditor::attachChoice (
    MarsChoiceStrip& strip, const char* parameterId,
    std::unique_ptr<juce::ParameterAttachment>& attachment)
{
    auto* parameter = processor.parameters.getParameter (parameterId);
    jassert (parameter != nullptr);
    attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [this, &strip] (float value)
        {
            strip.setSelectedIndex (juce::roundToInt (value));
            updateConditionalControls();
        }, nullptr);
    auto* attachmentPointer = attachment.get();
    strip.onChoice = [attachmentPointer] (int index)
    {
        attachmentPointer->setValueAsCompleteGesture (static_cast<float> (index));
    };
    attachment->sendInitialUpdate();
}

void MarsAudioProcessorEditor::updateConditionalControls()
{
    const auto mode = modeStrip.getSelectedIndex();
    const auto chordMode = mode == 2;
    const auto ensembleMode = mode != 0;
    chordStrip.setEnabled (chordMode);
    chordStrip.setAlpha (chordMode ? 1.0f : 0.38f);
    choirSizeSlider.setEnabled (ensembleMode);
    choirSizeLabel.setEnabled (ensembleMode);
    choirSizeSlider.setAlpha (ensembleMode ? 1.0f : 0.38f);
    choirSizeLabel.setAlpha (ensembleMode ? 1.0f : 0.38f);
}

void MarsAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (processor.getActiveVoiceCount(), processor.isEngineReady(),
                             processor.getCurrentSampleRateForDisplay());
}

void MarsAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (c (0xff0d1118), 0.0f, 0.0f,
                                     c (0xff090c12), 0.0f, static_cast<float> (getHeight()), false);
    background.addColour (0.48, c (0xff121521));
    g.setGradientFill (background);
    g.fillAll();

    const auto scale = getWidth() / 1160.0f;
    g.setColour (c (0x117a8da0));
    for (float x = 22.0f; x < getWidth(); x += 26.0f * scale)
        g.drawVerticalLine (juce::roundToInt (x), 72.0f, static_cast<float> (getHeight() - 25));

    auto drawPanel = [&g] (juce::Rectangle<int> bounds)
    {
        auto b = bounds.toFloat();
        g.setColour (c (0xa810151d));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (c (panelEdge));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
    };

    drawPanel (juce::Rectangle<int> (18, 82, getWidth() - 36, 150));
    drawPanel (juce::Rectangle<int> (18, 245, getWidth() - 36, 235));
    drawPanel (juce::Rectangle<int> (18, 514, getWidth() - 36, getHeight() - 532));

    g.setColour (c (accent));
    g.fillRect (18, 67, juce::jmax (90, getWidth() / 8), 2);
    g.setColour (c (ultraviolet));
    g.fillRect (18 + juce::jmax (90, getWidth() / 8), 67, 45, 2);
}

void MarsAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (72).reduced (24, 10);
    logoLabel.setBounds (header.removeFromLeft (150));
    editionLabel.setBounds (header.removeFromLeft (350));
    panicButton.setBounds (header.removeFromRight (74).reduced (2, 7));
    header.removeFromRight (10);
    statusDisplay.setBounds (header.removeFromRight (205).reduced (0, 5));

    const int contentX = 34;
    const int contentWidth = getWidth() - 68;
    const int gap = 14;
    const int voiceWidth = juce::jmax (150, contentWidth * 19 / 100);
    const int modeWidth = juce::jmax (230, contentWidth * 29 / 100);
    const int harmonyWidth = juce::jmax (155, contentWidth * 20 / 100);
    const int vowelWidth = contentWidth - voiceWidth - modeWidth - harmonyWidth - gap * 3;

    auto x = contentX;
    profileStrip.setBounds (x, 98, voiceWidth, 55); x += voiceWidth + gap;
    modeStrip.setBounds (x, 98, modeWidth, 55); x += modeWidth + gap;
    chordStrip.setBounds (x, 98, harmonyWidth, 55); x += harmonyWidth + gap;
    vowelStrip.setBounds (x, 98, vowelWidth, 55);

    choirSizeLabel.setBounds (contentX, 172, 104, 27);
    choirSizeSlider.setBounds (contentX + 106, 169, juce::jmin (360, contentWidth / 3), 31);

    auto knobArea = juce::Rectangle<int> (31, 264, getWidth() - 62, 196);
    constexpr int knobCount = 8;
    const auto knobWidth = knobArea.getWidth() / knobCount;
    MarsKnob* knobs[] = { &breathKnob, &resonanceKnob, &vibratoKnob, &humanizeKnob,
                            &spreadKnob, &tensionKnob, &roomKnob, &outputKnob };
    for (int i = 0; i < knobCount; ++i)
    {
        auto cell = knobArea.removeFromLeft (i == knobCount - 1 ? knobArea.getWidth() : knobWidth);
        knobs[i]->setBounds (cell.reduced (4, 0));
    }

    keyboardHintLabel.setBounds (32, 488, getWidth() - 64, 20);
    keyboard.setBounds (31, 530, getWidth() - 62, juce::jmax (70, getHeight() - 554));
}

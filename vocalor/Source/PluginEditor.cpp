#include "PluginEditor.h"

#include <cmath>
#include <utility>

namespace
{
constexpr auto accent = 0xff6fffd5;
constexpr auto accentBlue = 0xff66baff;
constexpr auto ultraviolet = 0xffa477ff;
constexpr auto panel = 0xff10151d;
constexpr auto panelRaised = 0xff18212b;
constexpr auto panelEdge = 0xff344250;
constexpr auto textBright = 0xffedf7f5;
constexpr auto textDim = 0xff91a2ad;

juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }

void styleHeaderLabel (juce::Label& label, float size, juce::Colour colour)
{
    label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, colour);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

VocalorLookAndFeel::VocalorLookAndFeel()
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

void VocalorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = juce::jmax (
        58.0f, static_cast<float> (juce::jmin (width, height)) - 12.0f);
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f - 2.0f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    const auto dialBounds = bounds.reduced (diameter * 0.105f);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    constexpr int tickCount = 11;
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const auto proportion = static_cast<float> (tick)
                              / static_cast<float> (tickCount - 1);
        const auto tickAngle = rotaryStartAngle
                             + proportion * (rotaryEndAngle - rotaryStartAngle);
        const auto outer = centre.getPointOnCircumference (radius * 0.98f, tickAngle);
        const auto inner = centre.getPointOnCircumference (
            radius * (tick % 5 == 0 ? 0.86f : 0.89f), tickAngle);
        const auto active = proportion <= sliderPos;
        g.setColour ((active ? c (accentBlue) : c (panelEdge))
                         .withAlpha (active ? 0.82f : 0.62f));
        g.drawLine ({ inner, outer }, tick % 5 == 0 ? 1.45f : 0.9f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius * 0.80f, radius * 0.80f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius * 0.80f, radius * 0.80f, 0.0f,
                            rotaryStartAngle, angle, true);
    juce::ColourGradient glow (c (accentBlue), bounds.getBottomLeft(), c (accent),
                               bounds.getTopRight(), false);
    g.setGradientFill (glow);
    g.strokePath (valueArc, juce::PathStrokeType (3.3f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::black.withAlpha (0.66f));
    g.fillEllipse (dialBounds.translated (1.2f, 3.0f));

    juce::ColourGradient bezel (c (0xff596877), dialBounds.getTopLeft(),
                                c (0xff1b2430), dialBounds.getBottomRight(), false);
    bezel.addColour (0.42, c (0xff344452));
    g.setGradientFill (bezel);
    g.fillEllipse (dialBounds);
    g.setColour (c (0xff7b8b97).withAlpha (0.70f));
    g.drawEllipse (dialBounds.reduced (0.5f), 1.0f);

    const auto face = dialBounds.reduced (5.0f);
    juce::ColourGradient glass (c (0xff17212c), face.getTopLeft(),
                                c (0xff050910), face.getBottomRight(), false);
    glass.addColour (0.36, c (0xff0e1721));
    g.setGradientFill (glass);
    g.fillEllipse (face);
    g.setColour (c (accentBlue).withAlpha (0.16f));
    g.drawEllipse (face.reduced (1.0f), 1.1f);

    const auto pointerStart = centre.getPointOnCircumference (radius * 0.16f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (radius * 0.58f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.64f));
    g.drawLine ({ pointerStart.translated (0.0f, 1.2f),
                  pointerEnd.translated (0.0f, 1.2f) }, 3.0f);
    g.setColour (c (textBright));
    g.drawLine ({ pointerStart, pointerEnd }, 2.2f);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));

    if (slider.hasKeyboardFocus (true) || slider.isMouseOver())
    {
        g.setColour (c (slider.hasKeyboardFocus (true) ? ultraviolet : accent)
                         .withAlpha (slider.hasKeyboardFocus (true) ? 0.95f : 0.46f));
        g.drawEllipse (dialBounds.expanded (2.6f),
                       slider.hasKeyboardFocus (true) ? 1.8f : 1.1f);
    }
}

void VocalorLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    const auto panic = button.getName().containsIgnoreCase ("panic");
    auto fill = panic ? c (0xff3f1822)
                      : active ? c (0xff173a3c) : c (0xff111720);
    if (highlighted) fill = fill.brighter (0.07f);
    if (down) fill = fill.darker (0.10f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (panic ? c (0xffff7383) : active ? c (accent) : c (panelEdge));
    g.drawRoundedRectangle (bounds, 5.0f, active || panic ? 1.4f : 1.0f);

    if (active && ! panic)
    {
        g.setColour (c (accent));
        g.fillRoundedRectangle (bounds.withHeight (3.0f)
                                    .withY (bounds.getBottom() - 3.0f)
                                    .reduced (6.0f, 0.0f), 1.4f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (c (ultraviolet));
        g.drawRoundedRectangle (bounds.reduced (2.5f), 3.0f, 1.5f);
    }
}

juce::Font VocalorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (
                                          13.0f, static_cast<float> (buttonHeight) * 0.38f),
                                          juce::Font::bold));
}

void VocalorLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
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

VocalorChoiceStrip::VocalorChoiceStrip (juce::String title, juce::StringArray choices)
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
        button->setWantsKeyboardFocus (true);
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

void VocalorChoiceStrip::setSelectedIndex (int newIndex)
{
    selectedIndex = juce::jlimit (0, static_cast<int> (buttons.size()) - 1, newIndex);
    for (int i = 0; i < static_cast<int> (buttons.size()); ++i)
        buttons[static_cast<size_t> (i)]->setToggleState (i == selectedIndex,
                                                         juce::dontSendNotification);
}

void VocalorChoiceStrip::paint (juce::Graphics& g)
{
    g.setColour (c (textDim));
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (titleText, getLocalBounds().removeFromTop (16), juce::Justification::centredLeft);
}

void VocalorChoiceStrip::resized()
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

VocalorKnob::VocalorKnob (juce::String name, juce::String suffix)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 19);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, c (0xe0090e15));
    slider.setColour (juce::Slider::textBoxOutlineColourId,
                      c (panelEdge).withAlpha (0.82f));
    slider.setColour (juce::Slider::textBoxTextColourId, c (textBright));
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

void VocalorKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (17));
    slider.setBounds (area);
}

void VocalorStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    repaint();
}

void VocalorStatusDisplay::paint (juce::Graphics& g)
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

VocalorAudioProcessorEditor::VocalorAudioProcessorEditor (VocalorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("VOCALOR", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 25.0f, c (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Vocalor human voice synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("HUMAN VOICE INSTRUMENT  ·  DUAL-STAGE ENGINE", juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 10.5f, c (textDim));
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);
    panicButton.setColour (juce::TextButton::textColourOffId, c (0xffffa4ad));
    panicButton.setName ("Panic — stop all voices");
    panicButton.setDescription ("Immediately silence every sounding voice and room tail");
    panicButton.onClick = [this]
    {
        processor.keyboardState.reset();
        processor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    for (auto* strip : { &profileStrip, &modeStrip, &chordStrip, &vowelStrip })
        addAndMakeVisible (*strip);

    choirSizeLabel.setText ("ENSEMBLE SIZE", juce::dontSendNotification);
    choirSizeLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    choirSizeLabel.setColour (juce::Label::textColourId, c (textDim));
    choirSizeLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (choirSizeLabel);

    choirSizeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    choirSizeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    choirSizeSlider.setName ("Ensemble size");
    choirSizeSlider.setTitle ("Ensemble size");
    choirSizeSlider.setDescription ("Number of independently humanised singers");
    addAndMakeVisible (choirSizeSlider);

    for (auto* knob : { &breathKnob, &resonanceKnob, &vibratoKnob, &humanizeKnob,
                        &spreadKnob, &tensionKnob, &roomKnob, &outputKnob })
        addAndMakeVisible (*knob);

    keyboard.setAvailableRange (36, 84);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (30.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (5);
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

    attachChoice (profileStrip, vocalor::parameters::profile, profileAttachment);
    attachChoice (modeStrip, vocalor::parameters::mode, modeAttachment);
    attachChoice (chordStrip, vocalor::parameters::chordQuality, chordAttachment);
    attachChoice (vowelStrip, vocalor::parameters::vowel, vowelAttachment);

    choirSizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::choirSize, choirSizeSlider);
    breathAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::breath, breathKnob.slider);
    resonanceAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::resonance, resonanceKnob.slider);
    vibratoAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::vibrato, vibratoKnob.slider);
    humanizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::humanize, humanizeKnob.slider);
    spreadAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::spread, spreadKnob.slider);
    tensionAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::tension, tensionKnob.slider);
    roomAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::room, roomKnob.slider);
    outputAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::output, outputKnob.slider);

    setResizable (true, true);
    setResizeLimits (900, 620, 1500, 840);
    setSize (1160, 750);
    startTimerHz (12);
    updateConditionalControls();
}

VocalorAudioProcessorEditor::~VocalorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalorAudioProcessorEditor::attachChoice (
    VocalorChoiceStrip& strip, const char* parameterId,
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

void VocalorAudioProcessorEditor::updateConditionalControls()
{
    const auto mode = modeStrip.getSelectedIndex();
    const auto chordMode = mode == 2;
    const auto ensembleMode = mode != 0;
    chordStrip.setEnabled (chordMode);
    chordStrip.setAlpha (chordMode ? 1.0f : 0.66f);
    choirSizeSlider.setEnabled (ensembleMode);
    choirSizeLabel.setEnabled (ensembleMode);
    choirSizeSlider.setAlpha (ensembleMode ? 1.0f : 0.62f);
    choirSizeLabel.setAlpha (ensembleMode ? 1.0f : 0.62f);
}

void VocalorAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (processor.getActiveVoiceCount(), processor.isEngineReady(),
                             processor.getCurrentSampleRateForDisplay());
}

void VocalorAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (c (0xff101720), 0.0f, 0.0f,
                                     c (0xff070b11), 0.0f,
                                     static_cast<float> (getHeight()), false);
    background.addColour (0.46, c (0xff111824));
    g.setGradientFill (background);
    g.fillAll();

    juce::ColourGradient cyanAura (c (accent).withAlpha (0.095f),
                                   static_cast<float> (getWidth()) * 0.22f, 38.0f,
                                   juce::Colours::transparentBlack,
                                   static_cast<float> (getWidth()) * 0.58f,
                                   static_cast<float> (getHeight()) * 0.62f, true);
    g.setGradientFill (cyanAura);
    g.fillAll();

    juce::ColourGradient violetAura (c (ultraviolet).withAlpha (0.055f),
                                     static_cast<float> (getWidth()) * 0.82f, 95.0f,
                                     juce::Colours::transparentBlack,
                                     static_cast<float> (getWidth()) * 0.56f,
                                     static_cast<float> (getHeight()) * 0.54f, true);
    g.setGradientFill (violetAura);
    g.fillAll();

    for (int contour = 0; contour < 4; ++contour)
    {
        const auto offset = static_cast<float> (contour) * 13.0f;
        juce::Path line;
        line.startNewSubPath (-20.0f, 122.0f + offset);
        line.cubicTo (static_cast<float> (getWidth()) * 0.28f, 75.0f + offset,
                      static_cast<float> (getWidth()) * 0.67f, 165.0f - offset,
                      static_cast<float> (getWidth()) + 20.0f, 104.0f + offset);
        g.setColour (c (contour % 2 == 0 ? accentBlue : ultraviolet)
                         .withAlpha (0.025f));
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }

    auto drawPanel = [&g] (juce::Rectangle<int> bounds)
    {
        auto b = bounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.fillRoundedRectangle (b.translated (0.0f, 3.0f), 9.0f);
        juce::ColourGradient panelFill (c (panelRaised).withAlpha (0.95f), b.getTopLeft(),
                                        c (0xf00b1017), b.getBottomLeft(), false);
        panelFill.addColour (0.38, c (0xf1121922));
        g.setGradientFill (panelFill);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (c (panelEdge).withAlpha (0.82f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (c (textBright).withAlpha (0.035f));
        g.drawRoundedRectangle (b.reduced (2.0f), 8.0f, 0.8f);
    };

    drawPanel (juce::Rectangle<int> (18, 82, getWidth() - 36, 150));
    drawPanel (juce::Rectangle<int> (18, 245, getWidth() - 36, 235));
    drawPanel (juce::Rectangle<int> (18, 514, getWidth() - 36, getHeight() - 532));

    const auto railWidth = static_cast<float> (juce::jmax (210, getWidth() / 5));
    juce::ColourGradient rail (c (accent), 18.0f, 0.0f,
                               c (ultraviolet), 18.0f + railWidth, 0.0f, false);
    rail.addColour (0.58, c (accentBlue));
    g.setGradientFill (rail);
    g.fillRect (18.0f, 67.0f, railWidth, 2.5f);

    const auto knobContentX = 31.0f;
    const auto knobContentWidth = static_cast<float> (getWidth() - 62);
    const auto knobCellWidth = knobContentWidth / 8.0f;
    const auto groupTop = 250.0f;
    const auto groupBottom = 469.0f;
    g.setColour (c (accent).withAlpha (0.022f));
    g.fillRoundedRectangle (knobContentX, groupTop, knobCellWidth * 4.0f,
                            groupBottom - groupTop, 7.0f);
    g.setColour (c (accentBlue).withAlpha (0.020f));
    g.fillRect (knobContentX + knobCellWidth * 4.0f, groupTop,
                knobCellWidth * 3.0f, groupBottom - groupTop);
    g.setColour (c (ultraviolet).withAlpha (0.026f));
    g.fillRoundedRectangle (knobContentX + knobCellWidth * 7.0f, groupTop,
                            knobCellWidth, groupBottom - groupTop, 7.0f);

    for (const auto dividerX : { knobContentX + knobCellWidth * 4.0f,
                                 knobContentX + knobCellWidth * 7.0f })
    {
        g.setColour (c (panelEdge).withAlpha (0.64f));
        g.drawVerticalLine (juce::roundToInt (dividerX), 259.0f, 463.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.setColour (c (accent).withAlpha (0.86f));
    g.drawText ("VOICE CHARACTER", juce::roundToInt (knobContentX + 12.0f), 252,
                juce::roundToInt (knobCellWidth * 4.0f - 24.0f), 18,
                juce::Justification::centredLeft);
    g.setColour (c (accentBlue).withAlpha (0.88f));
    g.drawText ("WIDTH + SPACE", juce::roundToInt (knobContentX + knobCellWidth * 4.0f + 12.0f),
                252, juce::roundToInt (knobCellWidth * 3.0f - 24.0f), 18,
                juce::Justification::centredLeft);
    g.setColour (c (ultraviolet).withAlpha (0.90f));
    g.drawText ("MASTER", juce::roundToInt (knobContentX + knobCellWidth * 7.0f + 12.0f),
                252, juce::roundToInt (knobCellWidth - 24.0f), 18,
                juce::Justification::centredLeft);
}

void VocalorAudioProcessorEditor::resized()
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

    auto knobArea = juce::Rectangle<int> (31, 274, getWidth() - 62, 186);
    constexpr int knobCount = 8;
    const auto knobWidth = knobArea.getWidth() / knobCount;
    VocalorKnob* knobs[] = { &breathKnob, &resonanceKnob, &vibratoKnob, &humanizeKnob,
                            &spreadKnob, &tensionKnob, &roomKnob, &outputKnob };
    for (int i = 0; i < knobCount; ++i)
    {
        auto cell = knobArea.removeFromLeft (i == knobCount - 1 ? knobArea.getWidth() : knobWidth);
        knobs[i]->setBounds (cell.reduced (4, 0));
    }

    keyboardHintLabel.setBounds (32, 488, getWidth() - 64, 20);
    keyboard.setBounds (31, 530, getWidth() - 62, juce::jmax (70, getHeight() - 554));
    // The available 36..84 range contains 29 white keys. Scale them to fill the
    // deck instead of leaving an inert blank tail at wider editor sizes.
    keyboard.setKeyWidth (juce::jmax (24.0f,
                                      static_cast<float> (keyboard.getWidth()) / 29.0f));
}

#include "PluginEditor.h"

#include <MarsAssets.h>

#include <cmath>
#include <initializer_list>
#include <utility>

namespace
{
constexpr auto coal = 0xff100b0e;
constexpr auto walnut = 0xff2a1119;
constexpr auto walnutLight = 0xff431923;
constexpr auto panel = 0xff21191b;
constexpr auto panelRaised = 0xff352127;
constexpr auto panelEdge = 0xff65483d;
constexpr auto brass = 0xffc58a3c;
constexpr auto brassBright = 0xffe0b66c;
constexpr auto rust = 0xffa7432f;
constexpr auto lamp = 0xffffb85a;
constexpr auto textBright = 0xffffeed0;
constexpr auto textDim = 0xffb9a486;
constexpr auto screen = 0xff151a16;

juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }

juce::String numericText (double value, MarsValueFormat format)
{
    switch (format)
    {
        case MarsValueFormat::Percent:
            return juce::String (juce::roundToInt (value * 100.0)) + "%";
        case MarsValueFormat::BipolarPercent:
            return (value > 0.0 ? "+" : "")
                 + juce::String (juce::roundToInt (value * 100.0)) + "%";
        case MarsValueFormat::Hertz:
            if (value >= 1000.0)
                return juce::String (value / 1000.0, value >= 10000.0 ? 1 : 2) + "k";
            return juce::String (value, value < 10.0 ? 2 : 0) + "Hz";
        case MarsValueFormat::Seconds:
            if (value < 1.0)
                return juce::String (juce::roundToInt (value * 1000.0)) + "ms";
            return juce::String (value, value < 10.0 ? 2 : 1) + "s";
        case MarsValueFormat::Cents:
            return (value > 0.0 ? "+" : "") + juce::String (value, 1) + "ct";
        case MarsValueFormat::Semitones:
            return (value > 0.0 ? "+" : "") + juce::String (juce::roundToInt (value)) + "st";
        case MarsValueFormat::Octaves:
            if (std::abs (value - std::round (value)) < 0.001)
                return (value > 0.0 ? "+" : "")
                     + juce::String (juce::roundToInt (value)) + "oct";
            return juce::String (value, 2) + "oct";
        case MarsValueFormat::Decibels:
            return (value > 0.0 ? "+" : "") + juce::String (value, 1) + "dB";
        case MarsValueFormat::Integer:
            return juce::String (juce::roundToInt (value));
    }
    return {};
}

double numericValue (juce::String text, MarsValueFormat format)
{
    const auto hasKiloSuffix = format == MarsValueFormat::Hertz && text.containsIgnoreCase ("k");
    const auto hasMilliseconds = format == MarsValueFormat::Seconds && text.containsIgnoreCase ("ms");
    auto value = text.retainCharacters ("0123456789.-").getDoubleValue();

    if (format == MarsValueFormat::Percent || format == MarsValueFormat::BipolarPercent)
        value /= 100.0;
    else if (hasKiloSuffix)
        value *= 1000.0;
    else if (hasMilliseconds)
        value /= 1000.0;

    return value;
}

void configureValueDisplay (juce::Slider& slider, MarsValueFormat format)
{
    slider.textFromValueFunction = [format] (double value)
    {
        return numericText (value, format);
    };
    slider.valueFromTextFunction = [format] (const juce::String& text)
    {
        return numericValue (text, format);
    };
}

void styleHeaderLabel (juce::Label& label, float size, juce::Colour colour)
{
    label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, colour);
    label.setInterceptsMouseClicks (false, false);
}

void distributeHorizontally (juce::Rectangle<int> area,
                             std::initializer_list<juce::Component*> components,
                             int gap = 3)
{
    const auto count = static_cast<int> (components.size());
    if (count == 0)
        return;

    area = area.reduced (1, 0);
    const auto available = juce::jmax (0, area.getWidth() - gap * (count - 1));
    int index = 0;
    for (auto* component : components)
    {
        const auto remaining = count - index;
        const auto width = remaining == 1 ? area.getWidth() : available / count;
        component->setBounds (area.removeFromLeft (width));
        if (++index < count)
            area.removeFromLeft (gap);
    }
}
} // namespace

MarsLookAndFeel::MarsLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, c (textBright));
    setColour (juce::Slider::textBoxBackgroundColourId, c (0x80150f0b));
    setColour (juce::Slider::textBoxOutlineColourId, c (0xff4a3b2c));
    setColour (juce::Slider::rotarySliderFillColourId, c (brass));
    setColour (juce::Slider::rotarySliderOutlineColourId, c (0xff514334));
    setColour (juce::TextButton::textColourOffId, c (textDim));
    setColour (juce::TextButton::textColourOnId, c (textBright));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, c (0xffeee1c9));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, c (0xff211a16));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, c (0xff5e4d3b));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, c (0x45e0a04c));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, c (0xb5bd4931));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, c (0xff6f5b45));
}

void MarsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = juce::jmax (10.0f,
        static_cast<float> (juce::jmin (width, height)) - 12.0f);
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f - 1.0f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (c (brassBright));
    g.strokePath (valueArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillEllipse (bounds.reduced (7.0f).translated (0.0f, 2.5f));

    juce::ColourGradient rim (c (0xffb58a55), bounds.getTopLeft(), c (0xff4a3524),
                              bounds.getBottomRight(), false);
    g.setGradientFill (rim);
    g.fillEllipse (bounds.reduced (7.0f));
    g.setColour (c (0xff160f0c));
    g.fillEllipse (bounds.reduced (11.0f));
    g.setColour (c (0xff5e4935));
    g.drawEllipse (bounds.reduced (11.0f), 1.0f);

    const auto pointerStart = centre.getPointOnCircumference (radius * 0.14f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (radius * 0.48f, angle);
    g.setColour (c (textBright));
    g.drawLine ({ pointerStart, pointerEnd }, juce::jmax (1.4f, radius * 0.055f));
    g.setColour (c (rust));
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (c (lamp));
        g.drawEllipse (bounds.reduced (3.5f), 1.6f);
    }
}

void MarsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    auto fill = active ? c (0xff743526) : c (0xff201a15);
    if (highlighted)
        fill = fill.brighter (0.09f);
    if (down)
        fill = fill.darker (0.14f);

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 3.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (active ? c (brassBright) : c (panelEdge));
    g.drawRoundedRectangle (bounds, 3.0f, active ? 1.4f : 1.0f);

    if (active)
    {
        g.setColour (c (lamp).withAlpha (0.72f));
        g.fillEllipse (bounds.getRight() - 7.0f, bounds.getY() + 4.0f, 3.0f, 3.0f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (c (lamp));
        g.drawRoundedRectangle (bounds.reduced (2.5f), 2.0f, 1.4f);
    }
}

void MarsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto drawFocusRing = [&]
    {
        if (! slider.hasKeyboardFocus (true))
            return;

        g.setColour (c (lamp));
        g.drawRoundedRectangle (
            juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                    static_cast<float> (width), static_cast<float> (height))
                .reduced (1.5f),
            3.0f, 1.4f);
    };

    if (style == juce::Slider::LinearVertical)
    {
        const auto cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const auto top = static_cast<float> (y + 4);
        const auto bottom = static_cast<float> (y + height - 4);
        g.setColour (c (0xff100c09));
        g.drawLine (cx, top, cx, bottom, 6.0f);
        g.setColour (c (0xff574331));
        g.drawLine (cx, top, cx, bottom, 2.0f);
        g.setColour (c (brass));
        g.drawLine (cx, sliderPos, cx, bottom, 2.3f);

        auto thumb = juce::Rectangle<float> (
                         juce::jmin (24.0f, static_cast<float> (width) * 0.58f), 12.0f)
                         .withCentre ({ cx, sliderPos });
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f), 2.5f);
        g.setColour (c (0xffd0a15e));
        g.fillRoundedRectangle (thumb, 2.5f);
        g.setColour (c (0xff493422));
        g.drawRoundedRectangle (thumb, 2.5f, 1.0f);
        g.drawLine (thumb.getX() + 4.0f, thumb.getCentreY(), thumb.getRight() - 4.0f,
                    thumb.getCentreY(), 1.0f);
        drawFocusRing();
        return;
    }

    if (style == juce::Slider::LinearHorizontal)
    {
        const auto cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
        const auto left = static_cast<float> (x + 4);
        const auto right = static_cast<float> (x + width - 4);
        g.setColour (c (0xff100c09));
        g.drawLine (left, cy, right, cy, 6.0f);
        g.setColour (c (brass));
        g.drawLine (left, cy, sliderPos, cy, 2.4f);
        g.setColour (c (brassBright));
        g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        drawFocusRing();
    }
}

juce::Font MarsLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (
        juce::jlimit (8.0f, 11.5f, static_cast<float> (buttonHeight) * 0.38f),
                                          juce::Font::bold));
}

MarsChoiceStrip::MarsChoiceStrip (juce::String title, juce::StringArray choices)
    : titleText (std::move (title))
{
    setName (titleText);
    setTitle (titleText);
    static int nextRadioGroupId = 0x4d00;
    const auto radioGroupId = nextRadioGroupId++;

    for (int index = 0; index < choices.size(); ++index)
    {
        auto button = std::make_unique<juce::TextButton> (choices[index]);
        button->setName (titleText + " " + choices[index]);
        button->setTitle (choices[index]);
        button->setDescription ("Select " + choices[index] + " for " + titleText.toLowerCase());
        button->setRadioGroupId (radioGroupId, juce::dontSendNotification);
        button->setClickingTogglesState (true);
        button->setWantsKeyboardFocus (true);
        button->onClick = [this, index]
        {
            setSelectedIndex (index);
            if (onChoice != nullptr)
                onChoice (index);
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
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (titleText, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

void MarsChoiceStrip::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (15);
    const auto count = static_cast<int> (buttons.size());
    for (int i = 0; i < count; ++i)
    {
        const auto remaining = count - i;
        auto buttonArea = area.removeFromLeft (area.getWidth() / remaining);
        buttons[static_cast<size_t> (i)]->setBounds (buttonArea.reduced (i == 0 ? 0 : 1, 0));
    }
}

MarsKnob::MarsKnob (juce::String name, MarsValueFormat format)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 17);
    slider.setName (name);
    slider.setTitle (name);
    slider.setDescription ("Adjust " + name.toLowerCase());
    slider.setWantsKeyboardFocus (true);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    configureValueDisplay (slider, format);
    addAndMakeVisible (slider);
}

void MarsKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (15));
    slider.setBounds (area);
}

MarsFader::MarsFader (juce::String name, MarsValueFormat format)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 57, 17);
    slider.setName (name);
    slider.setTitle (name);
    slider.setDescription ("Adjust " + name.toLowerCase());
    slider.setWantsKeyboardFocus (true);
    configureValueDisplay (slider, format);
    addAndMakeVisible (slider);
}

void MarsFader::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

void MarsStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate,
                                   bool scheduleRepaint)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    if (scheduleRepaint)
        repaint();
}

void MarsStatusDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (c (screen));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (c (0xff64523b));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto light = juce::Rectangle<float> (bounds.getX() + 10.0f,
                                               bounds.getCentreY() - 3.5f, 7.0f, 7.0f);
    g.setColour (isReady ? c (lamp) : c (0xff5b554b));
    g.fillEllipse (light);
    if (isReady)
    {
        g.setColour (c (lamp).withAlpha (0.16f));
        g.fillEllipse (light.expanded (5.0f));
    }

    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.setColour (c (textBright));
    g.drawText (juce::String (juce::jmax (0, voices)).paddedLeft ('0', 2) + " VOICES",
                25, 0, 76, getHeight(), juce::Justification::centredLeft);
    g.setColour (c (textDim));
    const auto rateText = rate > 0.0 ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz" : "OFFLINE";
    g.drawText (rateText, 100, 0, getWidth() - 108, getHeight(), juce::Justification::centredRight);
}

MarsAudioProcessorEditor::MarsAudioProcessorEditor (MarsAudioProcessor& p)
    : AudioProcessorEditor (&p), marsProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    panelTexture = juce::ImageCache::getFromMemory (
        MarsAssets::marspaneltexture_png, MarsAssets::marspaneltexture_pngSize);

    logoLabel.setText ("MARS", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 29.0f, c (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Mars analog polyphonic synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("POLYPHONIC ANALOG ENGINE  |  DIRECT CONTROL EDITION",
                          juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 10.0f, c (textDim));
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);
    statusDisplay.setAccessible (true);
    statusDisplay.setTitle ("Mars engine status");
    statusDisplay.setDescription ("Displays active voice count, engine state, and sample rate");
    panicButton.setColour (juce::TextButton::textColourOffId, c (0xffffb7a2));
    panicButton.setName ("Panic — stop all voices");
    panicButton.setDescription ("Immediately mute every sounding voice");
    panicButton.onClick = [this]
    {
        marsProcessor.keyboardState.reset();
        marsProcessor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    for (auto* strip : { &osc1WaveStrip, &osc2WaveStrip, &filterModelStrip,
                         &lfoWaveStrip, &voiceModeStrip })
        addAndMakeVisible (*strip);

    for (auto* knob : { &osc1OctaveKnob, &osc2OctaveKnob, &osc2TuneKnob,
                        &osc2FineKnob, &oscMixKnob, &pulseWidthKnob, &subLevelKnob,
                        &noiseLevelKnob, &crossModKnob, &cutoffKnob, &resonanceKnob,
                        &filterDriveKnob, &filterShapeKnob, &filterEnvKnob,
                        &keyTrackKnob, &lfoRateKnob, &lfoPitchKnob, &lfoFilterKnob,
                        &lfoPwmKnob, &unisonVoicesKnob, &driftKnob, &spreadKnob,
                        &glideKnob, &velocityKnob, &chorusMixKnob, &chorusRateKnob,
                        &outputKnob })
        addAndMakeVisible (*knob);

    for (auto* fader : { &filterAttackFader, &filterDecayFader, &filterSustainFader,
                         &filterReleaseFader, &ampAttackFader, &ampDecayFader,
                         &ampSustainFader, &ampReleaseFader })
        addAndMakeVisible (*fader);

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (23.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (3);
    keyboard.setName ("Playable piano keyboard");
    keyboard.setTitle ("Playable keyboard");
    keyboard.setDescription ("Play notes with the mouse or computer keyboard");
    addAndMakeVisible (keyboard);

    keyboardHintLabel.setText ("PLAY C1-C7  |  COMPUTER KEYS: A W S E D F T G Y H U J K",
                               juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    keyboardHintLabel.setColour (juce::Label::textColourId, c (textDim));
    keyboardHintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (keyboardHintLabel);

    const auto describe = [] (juce::Slider& slider, const juce::String& name,
                              const juce::String& description)
    {
        slider.setName (name);
        slider.setTitle (name);
        slider.setDescription (description);
    };
    describe (osc1OctaveKnob.slider, "VCO I octave", "Transpose VCO I by octaves");
    describe (osc2OctaveKnob.slider, "VCO II octave", "Transpose VCO II by octaves");
    describe (osc2TuneKnob.slider, "VCO II semitone tune", "Tune VCO II in semitones");
    describe (osc2FineKnob.slider, "VCO II fine tune", "Fine-tune VCO II in cents");
    describe (lfoRateKnob.slider, "LFO rate", "Set the low-frequency oscillator rate");
    describe (chorusRateKnob.slider, "Ensemble rate", "Set the analog ensemble modulation rate");
    describe (filterAttackFader.slider, "Filter envelope attack", "Set filter envelope attack time");
    describe (filterDecayFader.slider, "Filter envelope decay", "Set filter envelope decay time");
    describe (filterSustainFader.slider, "Filter envelope sustain", "Set filter envelope sustain level");
    describe (filterReleaseFader.slider, "Filter envelope release", "Set filter envelope release time");
    describe (ampAttackFader.slider, "Amplifier envelope attack", "Set amplifier envelope attack time");
    describe (ampDecayFader.slider, "Amplifier envelope decay", "Set amplifier envelope decay time");
    describe (ampSustainFader.slider, "Amplifier envelope sustain", "Set amplifier envelope sustain level");
    describe (ampReleaseFader.slider, "Amplifier envelope release", "Set amplifier envelope release time");

    choiceAttachments.reserve (5);
    sliderAttachments.reserve (35);

    attachChoice (osc1WaveStrip, mars::parameters::osc1Wave);
    attachChoice (osc2WaveStrip, mars::parameters::osc2Wave);
    attachChoice (filterModelStrip, mars::parameters::filterModel);
    attachChoice (lfoWaveStrip, mars::parameters::lfoWave);
    attachChoice (voiceModeStrip, mars::parameters::voiceMode);

    attachSlider (osc1OctaveKnob.slider, mars::parameters::osc1Octave);
    attachSlider (osc2OctaveKnob.slider, mars::parameters::osc2Octave);
    attachSlider (osc2TuneKnob.slider, mars::parameters::osc2Tune);
    attachSlider (osc2FineKnob.slider, mars::parameters::osc2Fine);
    attachSlider (oscMixKnob.slider, mars::parameters::oscMix);
    attachSlider (pulseWidthKnob.slider, mars::parameters::pulseWidth);
    attachSlider (subLevelKnob.slider, mars::parameters::subLevel);
    attachSlider (noiseLevelKnob.slider, mars::parameters::noiseLevel);
    attachSlider (crossModKnob.slider, mars::parameters::crossMod);
    attachSlider (cutoffKnob.slider, mars::parameters::cutoff);
    attachSlider (resonanceKnob.slider, mars::parameters::resonance);
    attachSlider (filterDriveKnob.slider, mars::parameters::filterDrive);
    attachSlider (filterShapeKnob.slider, mars::parameters::filterShape);
    attachSlider (filterEnvKnob.slider, mars::parameters::filterEnvAmount);
    attachSlider (keyTrackKnob.slider, mars::parameters::keyTrack);
    attachSlider (filterAttackFader.slider, mars::parameters::fAttack);
    attachSlider (filterDecayFader.slider, mars::parameters::fDecay);
    attachSlider (filterSustainFader.slider, mars::parameters::fSustain);
    attachSlider (filterReleaseFader.slider, mars::parameters::fRelease);
    attachSlider (ampAttackFader.slider, mars::parameters::aAttack);
    attachSlider (ampDecayFader.slider, mars::parameters::aDecay);
    attachSlider (ampSustainFader.slider, mars::parameters::aSustain);
    attachSlider (ampReleaseFader.slider, mars::parameters::aRelease);
    attachSlider (lfoRateKnob.slider, mars::parameters::lfoRate);
    attachSlider (lfoPitchKnob.slider, mars::parameters::lfoPitch);
    attachSlider (lfoFilterKnob.slider, mars::parameters::lfoFilter);
    attachSlider (lfoPwmKnob.slider, mars::parameters::lfoPwm);
    attachSlider (unisonVoicesKnob.slider, mars::parameters::unisonVoices);
    attachSlider (driftKnob.slider, mars::parameters::drift);
    attachSlider (spreadKnob.slider, mars::parameters::spread);
    attachSlider (glideKnob.slider, mars::parameters::glide);
    attachSlider (velocityKnob.slider, mars::parameters::velocity);
    attachSlider (chorusMixKnob.slider, mars::parameters::chorusMix);
    attachSlider (chorusRateKnob.slider, mars::parameters::chorusRate);
    attachSlider (outputKnob.slider, mars::parameters::output);

    setResizable (true, true);
    setResizeLimits (1180, 820, 1900, 1180);
    setSize (1280, 840);
    statusDisplay.setStatus (marsProcessor.getActiveVoiceCount(),
                             marsProcessor.isEngineReady(),
                             marsProcessor.getCurrentSampleRateForDisplay(), false);
    startTimerHz (12);
    updateConditionalControls();
}

MarsAudioProcessorEditor::~MarsAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MarsAudioProcessorEditor::attachChoice (MarsChoiceStrip& strip, const char* parameterId,
                                             float firstValue)
{
    auto* parameter = marsProcessor.parameters.getParameter (parameterId);
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [this, &strip, firstValue] (float value)
        {
            strip.setSelectedIndex (juce::roundToInt (value - firstValue));
            updateConditionalControls();
        }, nullptr);
    auto* attachmentPointer = attachment.get();
    strip.onChoice = [attachmentPointer, firstValue] (int index)
    {
        attachmentPointer->setValueAsCompleteGesture (static_cast<float> (index) + firstValue);
    };
    choiceAttachments.push_back (std::move (attachment));
    attachmentPointer->sendInitialUpdate();
}

void MarsAudioProcessorEditor::attachSlider (juce::Slider& slider, const char* parameterId)
{
    if (const auto* parameter = marsProcessor.parameters.getParameter (parameterId))
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    else
        jassertfalse;

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        marsProcessor.parameters, parameterId, slider));
}

void MarsAudioProcessorEditor::updateConditionalControls()
{
    const auto unisonMode = voiceModeStrip.getSelectedIndex() == 1;
    unisonVoicesKnob.setEnabled (unisonMode);
    unisonVoicesKnob.setAlpha (unisonMode ? 1.0f : 0.38f);

    const auto orbitFilter = filterModelStrip.getSelectedIndex() == 1;
    filterShapeKnob.setEnabled (orbitFilter);
    filterShapeKnob.setAlpha (orbitFilter ? 1.0f : 0.38f);

    const auto pulseActive = osc1WaveStrip.getSelectedIndex() == 1
                          || osc2WaveStrip.getSelectedIndex() == 1;
    pulseWidthKnob.setEnabled (pulseActive);
    pulseWidthKnob.setAlpha (pulseActive ? 1.0f : 0.38f);
    lfoPwmKnob.setEnabled (pulseActive);
    lfoPwmKnob.setAlpha (pulseActive ? 1.0f : 0.38f);
}

void MarsAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (marsProcessor.getActiveVoiceCount(), marsProcessor.isEngineReady(),
                             marsProcessor.getCurrentSampleRateForDisplay());
}

void MarsAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (panelTexture.isValid())
        g.drawImageWithin (panelTexture, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (c (coal));

    g.setColour (c (coal).withAlpha (0.32f));
    g.fillAll();

    juce::ColourGradient background (c (walnutLight).withAlpha (0.40f), 0.0f, 0.0f,
                                     c (coal).withAlpha (0.54f), 0.0f,
                                     static_cast<float> (getHeight()), false);
    background.addColour (0.22, c (walnut).withAlpha (0.30f));
    background.addColour (0.72, c (0xff160f12).withAlpha (0.46f));
    g.setGradientFill (background);
    g.fillAll();

    // The generated bitmap contributes only material and patina. Legends and
    // controls remain native components, so scaling and accessibility stay crisp.
    for (int y = 4; y < getHeight(); y += 7)
    {
        const auto alpha = 0.025f + 0.012f * std::sin (static_cast<float> (y) * 0.37f);
        g.setColour (c (brassBright).withAlpha (alpha));
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));
    }

    g.setColour (c (0xff100b08).withAlpha (0.72f));
    g.fillRect (10, 0, 8, getHeight());
    g.fillRect (getWidth() - 18, 0, 8, getHeight());
    g.setColour (c (brass).withAlpha (0.58f));
    g.fillRect (18, 62, getWidth() - 36, 2);

    static constexpr std::array<const char*, sectionCount> names {
        "VCO I", "VCO II", "MIX", "VCF", "LFO / VOICE",
        "FILTER ENV", "AMP ENV", "ENSEMBLE / MASTER"
    };

    for (int i = 0; i < sectionCount; ++i)
    {
        const auto bounds = sectionBounds[static_cast<size_t> (i)].toFloat();
        if (bounds.isEmpty())
            continue;

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 6.0f);
        g.setColour (c (panel));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (c (panelRaised).withAlpha (0.7f));
        g.fillRoundedRectangle (bounds.reduced (2.0f).withHeight (26.0f), 4.0f);
        g.setColour (c (panelEdge));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (c (brass).withAlpha (0.72f));
        g.fillRect (bounds.getX() + 9.0f, bounds.getY() + 25.0f,
                    juce::jmax (18.0f, bounds.getWidth() * 0.18f), 1.0f);
        g.setColour (c (textBright));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (names[static_cast<size_t> (i)],
                    sectionBounds[static_cast<size_t> (i)].reduced (10, 0).removeFromTop (25),
                    juce::Justification::centredLeft);

        for (const auto corner : { bounds.getTopLeft(), bounds.getTopRight(),
                                   bounds.getBottomLeft(), bounds.getBottomRight() })
        {
            g.setColour (c (0xff9a7950));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (
                corner + juce::Point<float> { corner.x < bounds.getCentreX() ? 7.0f : -7.0f,
                                               corner.y < bounds.getCentreY() ? 7.0f : -7.0f }));
        }
    }
}

void MarsAudioProcessorEditor::resized()
{
    const auto headerHeight = juce::jlimit (58, 74, getHeight() / 11);
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (24, 8);
    logoLabel.setBounds (header.removeFromLeft (150));
    editionLabel.setBounds (header.removeFromLeft (juce::jmin (480, header.getWidth() / 2)));
    panicButton.setBounds (header.removeFromRight (72).reduced (1, 6));
    header.removeFromRight (9);
    statusDisplay.setBounds (header.removeFromRight (190).reduced (0, 5));

    auto body = getLocalBounds();
    body.removeFromTop (headerHeight);
    body.reduce (22, 10);
    const auto gap = juce::jlimit (7, 11, getWidth() / 120);
    const auto keyboardHeight = juce::jlimit (125, 190, getHeight() * 19 / 100);
    auto keyboardArea = body.removeFromBottom (keyboardHeight);
    keyboardHintLabel.setBounds (keyboardArea.removeFromTop (19));
    keyboard.setBounds (keyboardArea.reduced (0, 2));
    body.removeFromBottom (gap);

    auto rowOne = body.removeFromTop ((body.getHeight() - gap) / 2);
    body.removeFromTop (gap);
    auto rowTwo = body;

    const auto rowOneAvailable = rowOne.getWidth() - gap * 3;
    const auto osc1Width = rowOneAvailable * 15 / 100;
    const auto osc2Width = rowOneAvailable * 21 / 100;
    const auto mixerWidth = rowOneAvailable * 25 / 100;

    sectionBounds[oscillator1Section] = rowOne.removeFromLeft (osc1Width);
    rowOne.removeFromLeft (gap);
    sectionBounds[oscillator2Section] = rowOne.removeFromLeft (osc2Width);
    rowOne.removeFromLeft (gap);
    sectionBounds[mixerSection] = rowOne.removeFromLeft (mixerWidth);
    rowOne.removeFromLeft (gap);
    sectionBounds[filterSection] = rowOne;

    const auto rowTwoAvailable = rowTwo.getWidth() - gap * 3;
    const auto lfoVoiceWidth = rowTwoAvailable * 36 / 100;
    const auto envelopeWidth = rowTwoAvailable * 18 / 100;
    sectionBounds[lfoVoiceSection] = rowTwo.removeFromLeft (lfoVoiceWidth);
    rowTwo.removeFromLeft (gap);
    sectionBounds[filterEnvelopeSection] = rowTwo.removeFromLeft (envelopeWidth);
    rowTwo.removeFromLeft (gap);
    sectionBounds[ampEnvelopeSection] = rowTwo.removeFromLeft (envelopeWidth);
    rowTwo.removeFromLeft (gap);
    sectionBounds[masterSection] = rowTwo;

    const auto sectionContent = [this] (Section section)
    {
        auto area = sectionBounds[static_cast<size_t> (section)].reduced (9, 7);
        area.removeFromTop (23);
        return area;
    };

    auto osc1 = sectionContent (oscillator1Section);
    osc1WaveStrip.setBounds (osc1.removeFromTop (juce::jlimit (38, 49, osc1.getHeight() / 4)));
    osc1OctaveKnob.setBounds (osc1.withSizeKeepingCentre (juce::jmin (110, osc1.getWidth()),
                                                         osc1.getHeight()));

    auto osc2 = sectionContent (oscillator2Section);
    osc2WaveStrip.setBounds (osc2.removeFromTop (juce::jlimit (38, 49, osc2.getHeight() / 4)));
    distributeHorizontally (osc2, { &osc2OctaveKnob, &osc2TuneKnob, &osc2FineKnob });

    auto mixer = sectionContent (mixerSection);
    auto mixerTop = mixer.removeFromTop (mixer.getHeight() / 2);
    distributeHorizontally (mixerTop, { &oscMixKnob, &pulseWidthKnob, &crossModKnob });
    distributeHorizontally (mixer, { &subLevelKnob, &noiseLevelKnob });

    auto filter = sectionContent (filterSection);
    filterModelStrip.setBounds (filter.removeFromTop (juce::jlimit (38, 45, filter.getHeight() / 5)));
    const auto filterGap = 2;
    auto filterTop = filter.removeFromTop ((filter.getHeight() - filterGap) / 2);
    filter.removeFromTop (filterGap);
    distributeHorizontally (filterTop, { &cutoffKnob, &resonanceKnob, &filterDriveKnob });
    distributeHorizontally (filter, { &filterShapeKnob, &filterEnvKnob, &keyTrackKnob });

    auto lfoVoice = sectionContent (lfoVoiceSection);
    auto lfo = lfoVoice.removeFromLeft ((lfoVoice.getWidth() - gap) / 2);
    lfoVoice.removeFromLeft (gap);
    auto voice = lfoVoice;

    lfoWaveStrip.setBounds (lfo.removeFromTop (juce::jlimit (38, 45, lfo.getHeight() / 5)));
    auto lfoTop = lfo.removeFromTop (lfo.getHeight() / 2);
    distributeHorizontally (lfoTop, { &lfoRateKnob, &lfoPitchKnob });
    distributeHorizontally (lfo, { &lfoFilterKnob, &lfoPwmKnob });

    voiceModeStrip.setBounds (voice.removeFromTop (juce::jlimit (38, 45, voice.getHeight() / 5)));
    auto voiceTop = voice.removeFromTop (voice.getHeight() / 2);
    distributeHorizontally (voiceTop,
                            { &unisonVoicesKnob, &driftKnob, &spreadKnob });
    distributeHorizontally (voice, { &glideKnob, &velocityKnob });

    distributeHorizontally (sectionContent (filterEnvelopeSection),
                            { &filterAttackFader, &filterDecayFader,
                              &filterSustainFader, &filterReleaseFader }, 1);
    distributeHorizontally (sectionContent (ampEnvelopeSection),
                            { &ampAttackFader, &ampDecayFader,
                              &ampSustainFader, &ampReleaseFader }, 1);

    distributeHorizontally (sectionContent (masterSection),
                            { &chorusMixKnob, &chorusRateKnob, &outputKnob });
}

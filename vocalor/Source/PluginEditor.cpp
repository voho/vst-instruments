#include "PluginEditor.h"

#include <array>
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

void drawInsetPanel (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title)
{
    g.setColour (c (0xf0080d13));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (c (panelEdge).withAlpha (0.85f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    g.setColour (c (textDim));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (title, bounds.reduced (11.0f, 5.0f).removeFromTop (14.0f).toNearestInt(),
                juce::Justification::centredLeft);
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
        50.0f, static_cast<float> (juce::jmin (width, height)) - 12.0f);
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
    slider.setNumDecimalPlacesToDisplay (suffix == "%" ? 0 : 1);
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

//==============================================================================
VocalorVowelPad::VocalorVowelPad()
{
    setName ("Vowel space");
    setTitle ("Vowel space");
    setDescription ("Drag to place the morph target in the continuous vowel space");
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> VocalorVowelPad::padArea() const
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    bounds.removeFromTop (22.0f);
    return bounds.reduced (20.0f, 16.0f);
}

void VocalorVowelPad::setTargetX (float x)
{
    if (dragging)
        return;
    x = juce::jlimit (0.0f, 1.0f, x);
    if (std::abs (x - targetX) < 1.0e-4f)
        return;
    targetX = x;
    repaint();
}

void VocalorVowelPad::setTargetY (float y)
{
    if (dragging)
        return;
    y = juce::jlimit (0.0f, 1.0f, y);
    if (std::abs (y - targetY) < 1.0e-4f)
        return;
    targetY = y;
    repaint();
}

void VocalorVowelPad::setMorph (float newMorph)
{
    newMorph = juce::jlimit (0.0f, 1.0f, newMorph);
    if (std::abs (newMorph - morph) < 1.0e-4f)
        return;
    morph = newMorph;
    repaint();
}

void VocalorVowelPad::setAnchor (int presetVowelIndex, bool isMale)
{
    if (anchorVowel == presetVowelIndex && male == isMale)
        return;
    anchorVowel = presetVowelIndex;
    male = isMale;
    repaint();
}

void VocalorVowelPad::setLivePosition (float x, float y)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);
    if (std::abs (x - liveX) < 2.0e-3f && std::abs (y - liveY) < 2.0e-3f)
        return;
    liveX = x;
    liveY = y;
    repaint();
}

void VocalorVowelPad::paint (juce::Graphics& g)
{
    drawInsetPanel (g, getLocalBounds().toFloat().reduced (1.0f), "VOWEL SPACE");

    const auto area = padArea();
    if (area.getWidth() <= 1.0f || area.getHeight() <= 1.0f)
        return;

    const auto toScreen = [&area] (float x, float y)
    {
        return juce::Point<float> (area.getX() + x * area.getWidth(),
                                   area.getY() + y * area.getHeight());
    };

    g.setColour (c (0xff0d1520));
    g.fillRoundedRectangle (area, 5.0f);
    for (int step = 1; step < 4; ++step)
    {
        const auto proportion = static_cast<float> (step) * 0.25f;
        g.setColour (c (panelEdge).withAlpha (0.30f));
        g.drawLine (area.getX() + proportion * area.getWidth(), area.getY(),
                    area.getX() + proportion * area.getWidth(), area.getBottom(), 0.7f);
        g.drawLine (area.getX(), area.getY() + proportion * area.getHeight(),
                    area.getRight(), area.getY() + proportion * area.getHeight(), 0.7f);
    }
    g.setColour (c (panelEdge).withAlpha (0.65f));
    g.drawRoundedRectangle (area, 5.0f, 1.0f);

    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.setColour (c (textDim).withAlpha (0.55f));
    g.drawText ("BACK", juce::Rectangle<float> (area.getX(), area.getBottom() + 1.0f,
                                                46.0f, 12.0f).toNearestInt(),
                juce::Justification::centredLeft);
    g.drawText ("FRONT", juce::Rectangle<float> (area.getRight() - 46.0f, area.getBottom() + 1.0f,
                                                 46.0f, 12.0f).toNearestInt(),
                juce::Justification::centredRight);
    // The vertical axis runs close (top) to open (bottom), like the vowel chart
    // the cardinal targets are laid out on, so each label sits on the edge it
    // actually describes: EE and OO span the top and the open anchor AH sits on
    // the bottom.
    g.drawText ("CLOSE", juce::Rectangle<float> (area.getX(), area.getY() - 13.0f,
                                                 area.getWidth(), 12.0f).toNearestInt(),
                juce::Justification::centred);
    g.drawText ("OPEN", juce::Rectangle<float> (area.getX(), area.getBottom() + 1.0f,
                                                area.getWidth(), 12.0f).toNearestInt(),
                juce::Justification::centred);

    for (int index = 0; index < vocalor::kCardinalVowelCount; ++index)
    {
        const auto position = vocalor::cardinalVowelPosition (index);
        const auto point = toScreen (position.x, position.y);
        g.setColour (c (accentBlue).withAlpha (0.24f));
        g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (point), 1.0f);
        g.setColour (c (textDim).withAlpha (0.80f));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (vocalor::cardinalVowelName (index),
                    juce::Rectangle<float> (26.0f, 12.0f).withCentre (point).toNearestInt(),
                    juce::Justification::centred);
    }

    const auto anchorPosition = vocalor::presetVowelPosition (anchorVowel);
    const auto anchorPoint = toScreen (anchorPosition.x, anchorPosition.y);
    const auto targetPoint = toScreen (targetX, targetY);
    const auto livePoint = toScreen (liveX, liveY);

    g.setColour (c (ultraviolet).withAlpha (0.45f));
    g.drawLine ({ anchorPoint, targetPoint }, 1.0f);

    g.setColour (c (textDim).withAlpha (0.85f));
    g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (anchorPoint), 1.4f);

    g.setColour (c (ultraviolet).withAlpha (0.30f + 0.60f * morph));
    g.drawEllipse (juce::Rectangle<float> (17.0f, 17.0f).withCentre (targetPoint), 1.6f);
    g.setColour (c (ultraviolet).withAlpha (0.55f + 0.45f * morph));
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (targetPoint));

    g.setColour (c (accent).withAlpha (0.18f));
    g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (livePoint));
    g.setColour (c (accent));
    g.fillEllipse (juce::Rectangle<float> (8.5f, 8.5f).withCentre (livePoint));

    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (c (accent).withAlpha (0.85f));
    g.drawText ("MORPH " + juce::String (juce::roundToInt (morph * 100.0f)) + "%",
                getLocalBounds().reduced (11, 5).removeFromTop (14),
                juce::Justification::centredRight);
}

void VocalorVowelPad::mouseDown (const juce::MouseEvent& event)
{
    dragging = true;
    if (onGesture)
        onGesture (true);
    sendFromMouse (event);
}

void VocalorVowelPad::mouseDrag (const juce::MouseEvent& event)
{
    sendFromMouse (event);
}

void VocalorVowelPad::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (onGesture)
        onGesture (false);
}

void VocalorVowelPad::sendFromMouse (const juce::MouseEvent& event)
{
    const auto area = padArea();
    if (area.getWidth() <= 1.0f || area.getHeight() <= 1.0f)
        return;

    targetX = juce::jlimit (0.0f, 1.0f,
                            (event.position.x - area.getX()) / area.getWidth());
    targetY = juce::jlimit (0.0f, 1.0f,
                            (event.position.y - area.getY()) / area.getHeight());
    if (onPosition)
        onPosition (targetX, targetY);
    repaint();
}

//==============================================================================
VocalorScopeDisplay::VocalorScopeDisplay()
{
    setName ("Vocal tract response");
    setTitle ("Vocal tract response");
    setDescription ("Live magnitude response of the five formant resonators and the output level");
    setInterceptsMouseClicks (false, false);
}

void VocalorScopeDisplay::setState (const vocalor::EngineDisplayState& newState)
{
    bool changed = std::abs (newState.levelLeft - state.levelLeft) > 1.0e-4f
                || std::abs (newState.levelRight - state.levelRight) > 1.0e-4f
                || std::abs (newState.sampleRate - state.sampleRate) > 1.0f;

    for (int i = 0; i < vocalor::kFormantCount && ! changed; ++i)
    {
        const auto index = static_cast<size_t> (i);
        changed = std::abs (newState.formantHz[index] - state.formantHz[index]) > 0.5f
               || std::abs (newState.formantGain[index] - state.formantGain[index]) > 1.0e-3f
               || std::abs (newState.formantBandwidth[index] - state.formantBandwidth[index]) > 0.5f;
    }

    if (! changed)
        return;

    state = newState;
    repaint();
}

void VocalorScopeDisplay::paint (juce::Graphics& g)
{
    drawInsetPanel (g, getLocalBounds().toFloat().reduced (1.0f), "VOCAL TRACT RESPONSE");

    auto content = getLocalBounds().toFloat().reduced (12.0f, 6.0f);
    content.removeFromTop (18.0f);
    auto meterRow = content.removeFromBottom (13.0f);
    content.removeFromBottom (5.0f);
    if (content.getWidth() <= 4.0f || content.getHeight() <= 4.0f)
        return;

    const float gridFrequencies[] = { 100.0f, 200.0f, 500.0f, 1000.0f,
                                      2000.0f, 5000.0f, 10000.0f };
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    for (const auto hz : gridFrequencies)
    {
        const auto proportion = vocalor::normalisedLogFrequency (hz, minimumHz, maximumHz);
        const auto x = content.getX() + proportion * content.getWidth();
        g.setColour (c (panelEdge).withAlpha (0.42f));
        g.drawLine (x, content.getY(), x, content.getBottom(), 0.7f);
        g.setColour (c (textDim).withAlpha (0.50f));
        g.drawText (hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k"
                                  : juce::String (juce::roundToInt (hz)),
                    juce::Rectangle<float> (x - 18.0f, content.getBottom() - 11.0f,
                                            36.0f, 11.0f).toNearestInt(),
                    juce::Justification::centred);
    }
    for (int step = 1; step < 4; ++step)
    {
        const auto y = content.getY() + static_cast<float> (step) * 0.25f * content.getHeight();
        g.setColour (c (panelEdge).withAlpha (0.28f));
        g.drawLine (content.getX(), y, content.getRight(), y, 0.6f);
    }

    // The curve probes the same fixed formant bank at curvePoints frequencies;
    // only the probe frequency changes from point to point, so the formant
    // bank's own pole/gain terms are resolved once here rather than once per
    // point (curvePoints times an exp, two trig calls and a sqrt, every
    // repaint at the scope's 24 Hz refresh).
    std::array<float, vocalor::kFormantCount> responseA1 {};
    std::array<float, vocalor::kFormantCount> responseA2 {};
    std::array<float, vocalor::kFormantCount> responseScale {};
    vocalor::formantResponseCoefficients (
        state.formantHz.data(), state.formantBandwidth.data(), state.formantGain.data(),
        vocalor::kFormantCount, state.sampleRate,
        responseA1.data(), responseA2.data(), responseScale.data());

    juce::Path curve;
    juce::Path fill;
    for (int i = 0; i < curvePoints; ++i)
    {
        const auto proportion = static_cast<float> (i) / static_cast<float> (curvePoints - 1);
        const auto hz = vocalor::logFrequencyForNormalised (proportion, minimumHz, maximumHz);
        const auto decibels = vocalor::formantResponseDbFromCoefficients (
            hz, responseA1.data(), responseA2.data(), responseScale.data(),
            vocalor::kFormantCount, state.sampleRate);
        const auto height = vocalor::decibelsToMeterPosition (decibels, floorDb, ceilingDb);
        const auto x = content.getX() + proportion * content.getWidth();
        const auto y = content.getBottom() - height * content.getHeight();

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            fill.startNewSubPath (x, content.getBottom());
            fill.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            fill.lineTo (x, y);
        }
    }
    fill.lineTo (content.getRight(), content.getBottom());
    fill.closeSubPath();

    juce::ColourGradient under (c (accent).withAlpha (0.26f), content.getX(), content.getY(),
                                c (accent).withAlpha (0.02f), content.getX(),
                                content.getBottom(), false);
    g.setGradientFill (under);
    g.fillPath (fill);

    juce::ColourGradient stroke (c (accent), content.getX(), content.getY(),
                                 c (accentBlue), content.getRight(), content.getBottom(), false);
    g.setGradientFill (stroke);
    g.strokePath (curve, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        const auto index = static_cast<size_t> (i);
        const auto hz = state.formantHz[index];
        if (! (hz > 0.0f))
            continue;
        const auto proportion = vocalor::normalisedLogFrequency (hz, minimumHz, maximumHz);
        const auto x = content.getX() + proportion * content.getWidth();
        g.setColour (c (ultraviolet).withAlpha (0.55f));
        g.drawLine (x, content.getY() + 2.0f, x, content.getY() + 12.0f, 1.2f);
        g.setColour (c (textDim).withAlpha (0.80f));
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.drawText ("F" + juce::String (i + 1),
                    juce::Rectangle<float> (x - 12.0f, content.getY() + 12.0f, 24.0f, 10.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    const auto drawMeter = [&g, &meterRow] (float level, bool upper)
    {
        auto bar = meterRow.withHeight (5.0f);
        bar.setY (upper ? meterRow.getY() : meterRow.getY() + 7.0f);
        g.setColour (c (0xff0a1017));
        g.fillRoundedRectangle (bar, 2.0f);
        const auto decibels = vocalor::linearToDecibels (level, -60.0f);
        const auto position = vocalor::decibelsToMeterPosition (decibels, -60.0f, 0.0f);
        if (position <= 0.002f)
            return;
        auto filled = bar.withWidth (juce::jmax (3.0f, bar.getWidth() * position));
        g.setColour (decibels > -1.0f ? c (0xffff7383)
                                      : decibels > -9.0f ? c (accent) : c (accentBlue));
        g.fillRoundedRectangle (filled, 2.0f);
    };
    drawMeter (state.levelLeft, true);
    drawMeter (state.levelRight, false);
}

//==============================================================================
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

    editionLabel.setText ("HUMAN VOICE INSTRUMENT  ·  MORPHING SOURCE-FILTER ENGINE",
                          juce::dontSendNotification);
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

    for (auto* strip : { &profileStrip, &modeStrip, &chordStrip, &vowelStrip, &legatoStrip })
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
    choirSizeSlider.setDescription (
        "Number of independently humanised singers in Choir mode, where 13 to 16 render the same 12 as the "
        "top of the range; Chord mode always uses six singers regardless of this control");
    addAndMakeVisible (choirSizeSlider);

    addAndMakeVisible (vowelPad);
    addAndMakeVisible (scopeDisplay);

    for (auto* knob : { &morphKnob, &formantShiftKnob, &breathKnob, &resonanceKnob,
                        &tensionKnob, &nasalKnob, &vibratoKnob, &instabilityKnob,
                        &humanizeKnob,
                        &dynamicsKnob, &intonationKnob, &glideKnob,
                        &spreadKnob, &roomKnob, &roomSizeKnob, &outputKnob })
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
    attachChoice (legatoStrip, vocalor::parameters::legato, legatoAttachment);

    // The morph readout on the pad follows the knob, including the initial
    // update the attachment sends when it is constructed below.
    morphKnob.slider.onValueChange = [this]
    {
        vowelPad.setMorph (static_cast<float> (morphKnob.slider.getValue()));
    };

    // Double-click returns every continuous control to its host-parameter
    // default (see createParameterLayout()), so a player can back out of an
    // exploratory tweak without hunting for the value in a text box.
    choirSizeSlider.setDoubleClickReturnValue (true, 8.0);
    morphKnob.slider.setDoubleClickReturnValue (true, 0.0);
    formantShiftKnob.slider.setDoubleClickReturnValue (true, 0.0);
    breathKnob.slider.setDoubleClickReturnValue (true, 0.30);
    resonanceKnob.slider.setDoubleClickReturnValue (true, 0.64);
    tensionKnob.slider.setDoubleClickReturnValue (true, 0.36);
    vibratoKnob.slider.setDoubleClickReturnValue (true, 0.0);
    instabilityKnob.slider.setDoubleClickReturnValue (true, 0.38);
    humanizeKnob.slider.setDoubleClickReturnValue (true, 0.52);
    nasalKnob.slider.setDoubleClickReturnValue (true, 0.0);
    dynamicsKnob.slider.setDoubleClickReturnValue (true, 1.0);
    intonationKnob.slider.setDoubleClickReturnValue (true, 0.0);
    glideKnob.slider.setDoubleClickReturnValue (true, 0.0);
    spreadKnob.slider.setDoubleClickReturnValue (true, 0.62);
    roomKnob.slider.setDoubleClickReturnValue (true, 0.24);
    roomSizeKnob.slider.setDoubleClickReturnValue (true, 0.50);
    outputKnob.slider.setDoubleClickReturnValue (true, -6.0);

    choirSizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::choirSize, choirSizeSlider);
    morphAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::vowelMorph, morphKnob.slider);
    formantShiftAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::formantShift, formantShiftKnob.slider);
    breathAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::breath, breathKnob.slider);
    resonanceAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::resonance, resonanceKnob.slider);
    tensionAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::tension, tensionKnob.slider);
    vibratoAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::vibrato, vibratoKnob.slider);
    instabilityAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::instability, instabilityKnob.slider);
    humanizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::humanize, humanizeKnob.slider);
    nasalAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::nasal, nasalKnob.slider);
    dynamicsAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::dynamics, dynamicsKnob.slider);
    intonationAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::intonation, intonationKnob.slider);
    glideAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::glide, glideKnob.slider);
    spreadAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::spread, spreadKnob.slider);
    roomAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::room, roomKnob.slider);
    roomSizeAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::roomSize, roomSizeKnob.slider);
    outputAttachment = std::make_unique<SliderAttachment> (
        processor.parameters, vocalor::parameters::output, outputKnob.slider);

    if (auto* parameter = processor.parameters.getParameter (vocalor::parameters::vowelX))
        vowelXAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter, [this] (float value) { vowelPad.setTargetX (value); }, nullptr);
    if (auto* parameter = processor.parameters.getParameter (vocalor::parameters::vowelY))
        vowelYAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter, [this] (float value) { vowelPad.setTargetY (value); }, nullptr);
    jassert (vowelXAttachment != nullptr && vowelYAttachment != nullptr);

    vowelPad.onGesture = [this] (bool begin)
    {
        if (vowelXAttachment == nullptr || vowelYAttachment == nullptr)
            return;
        if (begin)
        {
            vowelXAttachment->beginGesture();
            vowelYAttachment->beginGesture();
        }
        else
        {
            vowelXAttachment->endGesture();
            vowelYAttachment->endGesture();
        }
    };
    vowelPad.onPosition = [this] (float x, float y)
    {
        if (vowelXAttachment == nullptr || vowelYAttachment == nullptr)
            return;
        vowelXAttachment->setValueAsPartOfGesture (x);
        vowelYAttachment->setValueAsPartOfGesture (y);
    };
    if (vowelXAttachment != nullptr)
        vowelXAttachment->sendInitialUpdate();
    if (vowelYAttachment != nullptr)
        vowelYAttachment->sendInitialUpdate();

    setResizable (true, true);
    // The vertical floor is set by the fixed panel stack: below 800 px the
    // keyboard deck would push past the bottom of its panel.
    setResizeLimits (1120, 800, 1720, 1200);
    setSize (1240, 900);
    startTimerHz (24);
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
    vowelPad.setAnchor (vowelStrip.getSelectedIndex(), profileStrip.getSelectedIndex() == 1);
}

void VocalorAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (processor.getActiveVoiceCount(), processor.isEngineReady(),
                             processor.getCurrentSampleRateForDisplay());

    const auto display = processor.getDisplayState();
    scopeDisplay.setState (display);
    vowelPad.setLivePosition (display.vowelX, display.vowelY);
}

VocalorAudioProcessorEditor::Layout VocalorAudioProcessorEditor::computeLayout() const
{
    Layout layout;
    const auto width = getWidth();
    const auto height = getHeight();
    layout.contentX = 34;
    layout.contentWidth = width - 68;

    layout.performancePanel = juce::Rectangle<int> (18, 82, width - 36, 146);
    layout.vowelPanel = juce::Rectangle<int> (18, 240, width - 36, 218);
    layout.tonePanel = juce::Rectangle<int> (18, 470, width - 36, 196);
    layout.keyboardPanel = juce::Rectangle<int> (18, 678, width - 36,
                                                 juce::jmax (60, height - 696));

    const auto padWidth = juce::jlimit (186, 268, width / 5);
    const auto morphWidth = 2 * juce::jlimit (92, 128, width / 11);
    layout.vowelPad = juce::Rectangle<int> (layout.contentX, 256, padWidth, 190);
    layout.morphColumn = juce::Rectangle<int> (layout.vowelPad.getRight() + 14, 256,
                                               morphWidth, 190);
    const auto scopeX = layout.morphColumn.getRight() + 16;
    layout.scope = juce::Rectangle<int> (scopeX, 256,
                                         juce::jmax (170, width - 34 - scopeX), 190);
    layout.knobRow = juce::Rectangle<int> (31, 496, width - 62, 164);
    return layout;
}

void VocalorAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto layout = computeLayout();

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

    drawPanel (layout.performancePanel);
    drawPanel (layout.vowelPanel);
    drawPanel (layout.tonePanel);
    drawPanel (layout.keyboardPanel);

    const auto railWidth = static_cast<float> (juce::jmax (210, getWidth() / 5));
    juce::ColourGradient rail (c (accent), 18.0f, 0.0f,
                               c (ultraviolet), 18.0f + railWidth, 0.0f, false);
    rail.addColour (0.58, c (accentBlue));
    g.setGradientFill (rail);
    g.fillRect (18.0f, 67.0f, railWidth, 2.5f);

    // Knob groups. The tint bands and the captions are driven by the same cell
    // width the layout uses, so they cannot drift apart when the editor resizes.
    struct Group
    {
        int first;
        int count;
        unsigned int colour;
        float alpha;
        const char* caption;
    };
    static constexpr Group groups[] = {
        {  0, 7, accent,      0.022f, "VOICE CHARACTER" },
        {  7, 3, ultraviolet, 0.020f, "PERFORMANCE" },
        { 10, 3, accentBlue,  0.020f, "SPACE" },
        { 13, 1, ultraviolet, 0.026f, "MASTER" }
    };

    const auto knobRow = layout.knobRow.toFloat();
    const auto cellWidth = knobRow.getWidth() / 14.0f;
    const auto bandTop = static_cast<float> (layout.tonePanel.getY() + 6);
    const auto bandBottom = static_cast<float> (layout.tonePanel.getBottom() - 6);

    for (const auto& group : groups)
    {
        const auto x = knobRow.getX() + cellWidth * static_cast<float> (group.first);
        const auto w = cellWidth * static_cast<float> (group.count);
        g.setColour (c (group.colour).withAlpha (group.alpha));
        g.fillRoundedRectangle (x, bandTop, w, bandBottom - bandTop, 7.0f);
        g.setColour (c (group.colour).withAlpha (0.88f));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (group.caption, juce::roundToInt (x + 11.0f),
                    juce::roundToInt (bandTop + 2.0f),
                    juce::jmax (40, juce::roundToInt (w - 20.0f)), 16,
                    juce::Justification::centredLeft);

        if (group.first > 0)
        {
            g.setColour (c (panelEdge).withAlpha (0.64f));
            g.drawVerticalLine (juce::roundToInt (x), bandTop + 6.0f, bandBottom - 6.0f);
        }
    }
}

void VocalorAudioProcessorEditor::resized()
{
    const auto layout = computeLayout();

    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (72).reduced (24, 10);
    logoLabel.setBounds (header.removeFromLeft (150));
    editionLabel.setBounds (header.removeFromLeft (400));
    panicButton.setBounds (header.removeFromRight (74).reduced (2, 7));
    header.removeFromRight (10);
    statusDisplay.setBounds (header.removeFromRight (205).reduced (0, 5));

    const auto contentX = layout.contentX;
    const auto contentWidth = layout.contentWidth;
    const int gap = 14;
    const int voiceWidth = juce::jmax (150, contentWidth * 19 / 100);
    const int modeWidth = juce::jmax (230, contentWidth * 29 / 100);
    const int harmonyWidth = juce::jmax (155, contentWidth * 20 / 100);
    const int vowelWidth = juce::jmax (170, contentWidth - voiceWidth - modeWidth
                                                - harmonyWidth - gap * 3);

    auto x = contentX;
    profileStrip.setBounds (x, 96, voiceWidth, 55); x += voiceWidth + gap;
    modeStrip.setBounds (x, 96, modeWidth, 55); x += modeWidth + gap;
    chordStrip.setBounds (x, 96, harmonyWidth, 55); x += harmonyWidth + gap;
    vowelStrip.setBounds (x, 96, vowelWidth, 55);

    choirSizeLabel.setBounds (contentX, 164, 104, 27);
    const int ensembleWidth = juce::jlimit (170, 320, contentWidth / 4);
    choirSizeSlider.setBounds (contentX + 106, 161, ensembleWidth, 31);
    legatoStrip.setBounds (contentX + 106 + ensembleWidth + 34, 158,
                           juce::jmax (190, contentWidth / 5), 55);

    vowelPad.setBounds (layout.vowelPad);
    scopeDisplay.setBounds (layout.scope);

    auto morphColumn = layout.morphColumn;
    const auto morphCell = morphColumn.getWidth() / 2;
    morphKnob.setBounds (morphColumn.removeFromLeft (morphCell).reduced (4, 8));
    formantShiftKnob.setBounds (morphColumn.reduced (4, 8));

    auto knobArea = layout.knobRow;
    constexpr int knobCount = 14;
    const auto knobWidth = knobArea.getWidth() / knobCount;
    VocalorKnob* knobs[knobCount] = { &breathKnob, &resonanceKnob, &tensionKnob,
                                      &nasalKnob, &vibratoKnob, &instabilityKnob,
                                      &humanizeKnob,
                                      &dynamicsKnob, &intonationKnob, &glideKnob,
                                      &spreadKnob, &roomKnob, &roomSizeKnob, &outputKnob };
    for (int i = 0; i < knobCount; ++i)
    {
        auto cell = knobArea.removeFromLeft (i == knobCount - 1 ? knobArea.getWidth() : knobWidth);
        knobs[i]->setBounds (cell.reduced (3, 0));
    }

    keyboardHintLabel.setBounds (32, 684, getWidth() - 64, 18);
    keyboard.setBounds (31, 708, getWidth() - 62, juce::jmax (70, getHeight() - 738));
    // The available 36..84 range contains 29 white keys. Scale them to fill the
    // deck instead of leaving an inert blank tail at wider editor sizes.
    keyboard.setKeyWidth (juce::jmax (24.0f,
                                      static_cast<float> (keyboard.getWidth()) / 29.0f));
}

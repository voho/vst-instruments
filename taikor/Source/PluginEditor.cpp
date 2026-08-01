#include "PluginEditor.h"

#include <cmath>

namespace
{
// Design size and the range the editor may be resized through. The plug-in
// tests read the same numbers, so a layout change that breaks the contract
// fails the suite rather than the host.
constexpr int designWidth = 1280;
constexpr int designHeight = 880;
constexpr int minimumWidth = 1024;
constexpr int minimumHeight = 704;
constexpr int maximumWidth = 1472;
constexpr int maximumHeight = 1012;

constexpr float pi = 3.14159265358979f;

// A lacquered drum in a dark room: deep browns for the body, a pale hide for
// the head, and one warm accent for anything the player is actually touching.
const juce::Colour backgroundTop { 0xff241c17 };
const juce::Colour backgroundBottom { 0xff141010 };
const juce::Colour panelColour { 0xff2f251e };
const juce::Colour panelEdge { 0xff4a3a2c };
const juce::Colour hideColour { 0xffd9c3a0 };
const juce::Colour accentColour { 0xffe0663c };
const juce::Colour accentDim { 0xff7d3a25 };
const juce::Colour textColour { 0xffe6dccb };
const juce::Colour mutedText { 0xff9a8b78 };

juce::Colour roleColour (TaikorKnob::VisualRole role) noexcept
{
    switch (role)
    {
        case TaikorKnob::VisualRole::Drum:       return juce::Colour { 0xffd9a441 };
        case TaikorKnob::VisualRole::Stroke:     return juce::Colour { 0xffe0663c };
        case TaikorKnob::VisualRole::Microphone: return juce::Colour { 0xff5fa8a0 };
        case TaikorKnob::VisualRole::Master:     return juce::Colour { 0xffc9c0b0 };
    }
    return accentColour;
}

// Scientific pitch notation puts middle C (MIDI 60) at C4, which makes this
// instrument's reference note - MIDI 48 - C3. Both the octave strip and the
// pads derive their labels from that one constant rather than each carrying
// their own idea of it, because they disagreed by an octave when they did:
// JUCE's note-name helper takes the octave number to give middle C, and it was
// being handed the reference note's octave instead.
constexpr int referenceOctaveNumber = taikor::referenceNote / 12 - 1;
constexpr int octaveNumberForMiddleC =
    referenceOctaveNumber + (60 - taikor::referenceNote) / 12;

static_assert (referenceOctaveNumber == 3,
               "the documented mapping calls MIDI 48 C3");
static_assert (octaveNumberForMiddleC == 4,
               "middle C must be C4 if MIDI 48 is C3");

juce::String octaveName (int octaveOffset)
{
    return "C" + juce::String (referenceOctaveNumber + octaveOffset);
}

// A short description of the drum an octave produces, so the octave row reads
// as instruments rather than as numbers.
juce::String octaveDescription (int octaveOffset)
{
    switch (octaveOffset)
    {
        case -2: return "Odaiko";
        case -1: return "Large";
        case 0:  return "Nagado";
        case 1:  return "Small";
        case 2:  return "Shime";
        case 3:  return "Tiny";
        default: return {};
    }
}
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

TaikorLookAndFeel::TaikorLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, panelEdge);
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textColour);
    setColour (juce::TooltipWindow::backgroundColourId, panelColour);
    setColour (juce::TooltipWindow::textColourId, textColour);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
}

void TaikorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width,
                                          int height, float sliderPos,
                                          float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centreX = bounds.getCentreX();
    const auto centreY = bounds.getCentreY();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto thickness = juce::jmax (3.0f, radius * 0.20f);

    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centreX, centreY, radius - thickness * 0.5f,
                         radius - thickness * 0.5f, 0.0f, rotaryStartAngle,
                         rotaryEndAngle, true);
    g.setColour (panelEdge);
    g.strokePath (track, juce::PathStrokeType (thickness,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (angle > rotaryStartAngle + 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centreX, centreY, radius - thickness * 0.5f,
                             radius - thickness * 0.5f, 0.0f, rotaryStartAngle,
                             angle, true);
        g.setColour (slider.isEnabled() ? fill : fill.withAlpha (0.4f));
        g.strokePath (value, juce::PathStrokeType (thickness,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const auto knobRadius = radius - thickness * 1.35f;
    juce::ColourGradient body { panelColour.brighter (0.18f), centreX,
                                centreY - knobRadius, panelColour.darker (0.35f),
                                centreX, centreY + knobRadius, false };
    g.setGradientFill (body);
    g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (panelEdge);
    g.drawEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    juce::Path pointer;
    const auto pointerLength = knobRadius * 0.78f;
    const auto pointerThickness = juce::jmax (2.0f, knobRadius * 0.14f);
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength,
                                 pointerThickness, pointerLength,
                                 pointerThickness * 0.5f);
    pointer.applyTransform (
        juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (fill.brighter (0.35f));
    g.fillPath (pointer);
}

void TaikorLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = backgroundColour;
    if (isDown)
        fill = fill.brighter (0.30f);
    else if (isHighlighted)
        fill = fill.brighter (0.14f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (button.getToggleState() ? accentColour : panelEdge);
    g.drawRoundedRectangle (bounds, 4.0f, button.getToggleState() ? 1.8f : 1.0f);
}

juce::Font TaikorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (
        juce::jlimit (11.0f, 16.0f, static_cast<float> (buttonHeight) * 0.42f)));
}

// ---------------------------------------------------------------------------
// Stroke pad
// ---------------------------------------------------------------------------

TaikorPad::TaikorPad (taikor::Articulation articulationToUse)
    : juce::Button (juce::String (
          taikor::getArticulationDisplayName (articulationToUse).data(),
          taikor::getArticulationDisplayName (articulationToUse).size())),
      articulation (articulationToUse)
{
    const auto& metadata = taikor::getArticulationMetadata (articulationToUse);
    nameText = juce::String (metadata.displayName.data(), metadata.displayName.size());
    mnemonicText = juce::String (metadata.mnemonic.data(), metadata.mnemonic.size());
    refreshNoteText();
    setWantsKeyboardFocus (false);
}

void TaikorPad::refreshNoteText()
{
    const auto note = taikor::midiNoteFor (articulation, octaveOffset);
    noteText = juce::MidiMessage::getMidiNoteName (note, true, true,
                                                   octaveNumberForMiddleC)
             + " (" + juce::String (note) + ")";
}

void TaikorPad::setSelected (bool shouldBeSelected)
{
    if (selected == shouldBeSelected)
        return;
    selected = shouldBeSelected;
    repaint();
}

void TaikorPad::setOctaveOffset (int newOctaveOffset)
{
    if (octaveOffset == newOctaveOffset)
        return;
    octaveOffset = newOctaveOffset;
    refreshNoteText();
    repaint();
}

void TaikorPad::triggerFlash()
{
    flashLevel = 1.0f;
    repaint();
}

void TaikorPad::advanceFlash()
{
    if (flashLevel <= 0.0f)
        return;
    flashLevel *= 0.80f;
    if (flashLevel < 0.01f)
        flashLevel = 0.0f;
    repaint();
}

void TaikorPad::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    auto base = panelColour;
    if (isButtonDown)
        base = base.brighter (0.26f);
    else if (isMouseOver)
        base = base.brighter (0.12f);

    juce::ColourGradient gradient { base.brighter (0.10f), bounds.getCentreX(),
                                    bounds.getY(), base.darker (0.22f),
                                    bounds.getCentreX(), bounds.getBottom(), false };
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 5.0f);

    if (flashLevel > 0.0f)
    {
        g.setColour (accentColour.withAlpha (flashLevel * 0.55f));
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    g.setColour (selected ? accentColour : panelEdge);
    g.drawRoundedRectangle (bounds, 5.0f, selected ? 2.0f : 1.0f);

    auto text = bounds.reduced (6.0f, 5.0f);
    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (
        juce::jlimit (12.0f, 17.0f, bounds.getHeight() * 0.24f)).withStyle ("Bold")));
    g.drawText (nameText, text.removeFromTop (text.getHeight() * 0.42f),
                juce::Justification::centredTop, false);

    g.setColour (accentColour.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (
        juce::jlimit (10.0f, 14.0f, bounds.getHeight() * 0.19f))));
    g.drawText (mnemonicText, text.removeFromTop (text.getHeight() * 0.5f),
                juce::Justification::centred, false);

    g.setColour (mutedText);
    g.setFont (juce::Font (juce::FontOptions (
        juce::jlimit (9.0f, 12.0f, bounds.getHeight() * 0.16f))));
    g.drawText (noteText, text, juce::Justification::centredBottom, false);
}

std::unique_ptr<juce::AccessibilityHandler> TaikorPad::createAccessibilityHandler()
{
    const auto& metadata = taikor::getArticulationMetadata (articulation);
    const juce::String description {
        juce::String (metadata.description.data(), metadata.description.size())
        + ". Plays " + noteText
    };

    class PadHandler final : public juce::AccessibilityHandler
    {
    public:
        PadHandler (TaikorPad& padToUse, juce::String helpText)
            : juce::AccessibilityHandler (
                  padToUse, juce::AccessibilityRole::button,
                  juce::AccessibilityActions().addAction (
                      juce::AccessibilityActionType::press,
                      [&padToUse] { padToUse.triggerClick(); })),
              help (std::move (helpText))
        {
        }

        juce::String getHelp() const override { return help; }

    private:
        juce::String help;
    };

    return std::make_unique<PadHandler> (*this, description);
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------

TaikorKnob::TaikorKnob (juce::String name, ValueStyle style, VisualRole roleToUse)
    : role (roleToUse)
{
    setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 17);
    slider.setColour (juce::Slider::rotarySliderFillColourId, roleColour (roleToUse));
    slider.setColour (juce::Slider::textBoxTextColourId, textColour);

    switch (style)
    {
        case ValueStyle::Percent:     slider.setTextValueSuffix (" %"); break;
        case ValueStyle::Semitones:   slider.setTextValueSuffix (" st"); break;
        case ValueStyle::Centimetres: slider.setTextValueSuffix (" cm"); break;
        case ValueStyle::Decibels:    slider.setTextValueSuffix (" dB"); break;
    }

    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, mutedText);
    label.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void TaikorKnob::setLabelText (const juce::String& text, const juce::String& description)
{
    label.setText (text, juce::dontSendNotification);
    setTooltip (description);
    slider.setTooltip (description);
    slider.setTitle (text);
    slider.setDescription (description);
}

void TaikorKnob::resized()
{
    auto bounds = getLocalBounds();
    label.setBounds (bounds.removeFromTop (14));
    slider.setBounds (bounds);
}

// ---------------------------------------------------------------------------
// Head display
// ---------------------------------------------------------------------------

TaikorHeadDisplay::TaikorHeadDisplay()
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Drum head");
}

void TaikorHeadDisplay::setStrike (float normalisedRadius, float angleRadians,
                                   float level, taikor::Articulation articulation)
{
    strikeRadius = taikor::ui::clamp (normalisedRadius, 0.0f, 1.0f);
    strikeAngle = angleRadians;
    lastArticulation = articulation;

    const auto bounded = taikor::ui::clamp (level, 0.0f, 1.0f);
    if (std::abs (bounded - strikeLevel) < 0.002f)
        return;
    strikeLevel = bounded;
    repaint();
}

void TaikorHeadDisplay::setMicrophones (float spread, float normalisedDistance)
{
    if (std::abs (spread - micSpread) < 0.002f
        && std::abs (normalisedDistance - micDistance) < 0.002f)
        return;
    micSpread = taikor::ui::clamp (spread, 0.0f, 1.0f);
    micDistance = taikor::ui::clamp (normalisedDistance, 0.0f, 1.0f);
    repaint();
}

void TaikorHeadDisplay::setMeasurements (float fundamentalHz, float breathingHz,
                                         float diameterCentimetres, float tailSeconds)
{
    if (std::abs (fundamentalHz - fundamental) < 0.05f
        && std::abs (breathingHz - breathing) < 0.05f
        && std::abs (diameterCentimetres - diameter) < 0.05f
        && std::abs (tailSeconds - tail) < 0.005f)
        return;
    fundamental = fundamentalHz;
    breathing = breathingHz;
    diameter = diameterCentimetres;
    tail = tailSeconds;
    repaint();
}

void TaikorHeadDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto readoutHeight = juce::jmin (58.0f, bounds.getHeight() * 0.22f);
    const auto drumArea = bounds.withTrimmedBottom (readoutHeight);
    const auto centre = drumArea.getCentre();
    const auto headRadius =
        juce::jmin (drumArea.getWidth(), drumArea.getHeight()) * 0.42f;

    // The shell, seen edge on behind the head.
    g.setColour (panelColour.darker (0.45f));
    g.fillEllipse (centre.x - headRadius * 1.10f, centre.y - headRadius * 1.10f,
                   headRadius * 2.20f, headRadius * 2.20f);
    g.setColour (panelEdge);
    g.drawEllipse (centre.x - headRadius * 1.10f, centre.y - headRadius * 1.10f,
                   headRadius * 2.20f, headRadius * 2.20f, 1.4f);

    // The hide.
    juce::ColourGradient hide { hideColour.brighter (0.10f), centre.x,
                                centre.y - headRadius, hideColour.darker (0.30f),
                                centre.x, centre.y + headRadius, false };
    g.setGradientFill (hide);
    g.fillEllipse (centre.x - headRadius, centre.y - headRadius,
                   headRadius * 2.0f, headRadius * 2.0f);

    // The tack ring that holds the head on.
    g.setColour (panelEdge.brighter (0.25f));
    for (int tack = 0; tack < 32; ++tack)
    {
        const auto angle = static_cast<float> (tack) * 2.0f * pi / 32.0f;
        const auto point = taikor::ui::headPointFor (0.945f, angle);
        g.fillEllipse (centre.x + point.x * headRadius - 1.6f,
                       centre.y + point.y * headRadius - 1.6f, 3.2f, 3.2f);
    }

    // The ripple where the last stroke landed. Its size follows the stroke's
    // own radius, so a Ka really does land out by the tacks.
    if (strikeLevel > 0.004f)
    {
        const auto point = taikor::ui::headPointFor (strikeRadius, strikeAngle);
        const auto x = centre.x + point.x * headRadius;
        const auto y = centre.y + point.y * headRadius;

        for (int ring = 0; ring < 3; ++ring)
        {
            const auto spread = headRadius * (0.10f + 0.11f * static_cast<float> (ring))
                              * (0.6f + 0.9f * strikeLevel);
            const auto alpha = strikeLevel * (0.55f - 0.15f * static_cast<float> (ring));
            g.setColour (accentColour.withAlpha (juce::jmax (0.0f, alpha)));
            g.drawEllipse (x - spread, y - spread, spread * 2.0f, spread * 2.0f, 1.6f);
        }

        g.setColour (accentColour.withAlpha (juce::jmin (1.0f, strikeLevel)));
        const auto dot = headRadius * 0.045f;
        g.fillEllipse (x - dot, y - dot, dot * 2.0f, dot * 2.0f);
    }

    g.setColour (hideColour.darker (0.55f));
    g.drawEllipse (centre.x - headRadius, centre.y - headRadius,
                   headRadius * 2.0f, headRadius * 2.0f, 1.2f);

    // The close pair, where the microphone controls put it. The dots sit at the
    // radius the model reads the head at, and they fade as the pair backs off.
    const auto micRingRadius = headRadius * (0.10f + 0.68f * micSpread);
    constexpr float micReference = 0.60f;
    const auto separation = 2.2f * micSpread;
    const auto proximity = 1.0f - micDistance;

    for (int side = 0; side < 2; ++side)
    {
        const auto angle = micReference
                         + (side == 0 ? 0.5f : -0.5f) * separation;
        const auto point = taikor::ui::headPointFor (1.0f, angle);
        const auto x = centre.x + point.x * micRingRadius;
        const auto y = centre.y + point.y * micRingRadius;
        const auto size = headRadius * (0.045f + 0.030f * proximity);

        g.setColour (roleColour (TaikorKnob::VisualRole::Microphone)
                         .withAlpha (0.35f + 0.5f * proximity));
        g.fillEllipse (x - size, y - size, size * 2.0f, size * 2.0f);
        g.setColour (roleColour (TaikorKnob::VisualRole::Microphone));
        g.drawEllipse (x - size, y - size, size * 2.0f, size * 2.0f, 1.2f);

        g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
        g.drawText (side == 0 ? "L" : "R",
                    juce::Rectangle<float> (x - size, y - size, size * 2.0f, size * 2.0f),
                    juce::Justification::centred, false);
    }

    // What the model says this drum is.
    auto readout = bounds.withTop (bounds.getBottom() - readoutHeight)
                       .reduced (8.0f, 4.0f);
    g.setColour (mutedText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    const auto line = readout.getHeight() / 2.0f;
    g.drawText (juce::String (diameter, 1) + " cm head  -  "
                    + juce::String (tail, 2) + " s tail",
                readout.removeFromTop (line), juce::Justification::centred, false);

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (juce::String (fundamental, 1) + " Hz  +  "
                    + juce::String (breathing, 1) + " Hz breathing",
                readout, juce::Justification::centred, false);
}

std::unique_ptr<juce::AccessibilityHandler>
TaikorHeadDisplay::createAccessibilityHandler()
{
    class HeadHandler final : public juce::AccessibilityHandler
    {
    public:
        explicit HeadHandler (TaikorHeadDisplay& display)
            : juce::AccessibilityHandler (display, juce::AccessibilityRole::staticText),
              owner (display)
        {
        }

        juce::String getHelp() const override
        {
            const auto stroke =
                taikor::getArticulationDisplayName (owner.lastArticulation);
            return "Drum head. " + juce::String (owner.diameter, 1)
                 + " centimetre head sounding " + juce::String (owner.fundamental, 1)
                 + " hertz with a " + juce::String (owner.breathing, 1)
                 + " hertz breathing mode, and a " + juce::String (owner.tail, 2)
                 + " second tail. Last stroke: "
                 + juce::String (stroke.data(), stroke.size()) + " at "
                 + juce::String (juce::roundToInt (owner.strikeRadius * 100.0f))
                 + " per cent of the radius.";
        }

    private:
        TaikorHeadDisplay& owner;
    };

    return std::make_unique<HeadHandler> (*this);
}

// ---------------------------------------------------------------------------
// Status and metering
// ---------------------------------------------------------------------------

TaikorStatusDisplay::TaikorStatusDisplay()
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Engine status");
}

void TaikorStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate)
{
    if (voices == activeVoices && isReady == ready && rate == sampleRate)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    repaint();
}

void TaikorStatusDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panelColour.darker (0.25f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    bounds = bounds.reduced (8.0f, 4.0f);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (isReady ? textColour : mutedText);

    const juce::String rateText = rate > 0.0
        ? juce::String (rate / 1000.0, 1) + " kHz"
        : juce::String ("- kHz");
    g.drawText (juce::String (juce::jmax (0, voices)) + " voices  -  " + rateText,
                bounds, juce::Justification::centred, false);
}

std::unique_ptr<juce::AccessibilityHandler>
TaikorStatusDisplay::createAccessibilityHandler()
{
    class StatusHandler final : public juce::AccessibilityHandler
    {
    public:
        explicit StatusHandler (TaikorStatusDisplay& display)
            : juce::AccessibilityHandler (display, juce::AccessibilityRole::staticText),
              owner (display)
        {
        }

        juce::String getHelp() const override
        {
            return juce::String (juce::jmax (0, owner.voices)) + " voices sounding at "
                 + (owner.rate > 0.0 ? juce::String (owner.rate / 1000.0, 1)
                                     : juce::String ("unknown"))
                 + " kilohertz.";
        }

    private:
        TaikorStatusDisplay& owner;
    };

    return std::make_unique<StatusHandler> (*this);
}

TaikorMeter::TaikorMeter()
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Output meter");
    leftBallistics.reset();
    rightBallistics.reset();
}

void TaikorMeter::setLevels (float leftLinear, float rightLinear)
{
    constexpr float updateRate = 30.0f;
    const auto attack = taikor::ui::onePoleCoefficient (0.012f, updateRate);
    const auto release = taikor::ui::onePoleCoefficient (0.240f, updateRate);
    const auto peakFall = taikor::ui::decayMultiplier (-12.0f, 1.0f, updateRate);
    constexpr float hold = 18.0f;

    leftBallistics.update (taikor::ui::meterPositionForLinear (leftLinear, floorDecibels),
                           attack, release, peakFall, hold);
    rightBallistics.update (
        taikor::ui::meterPositionForLinear (rightLinear, floorDecibels),
        attack, release, peakFall, hold);

    const bool clipping = leftLinear >= 0.999f || rightLinear >= 0.999f;
    const int clipState = clipping ? 1 : 0;
    if (clipState != announcedClipState)
    {
        announcedClipState = clipState;
        if (clipping)
            if (auto* handler = getAccessibilityHandler())
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    }

    if (std::abs (leftBallistics.level - lastPaintedLeft) < 0.003f
        && std::abs (rightBallistics.level - lastPaintedRight) < 0.003f
        && std::abs (leftBallistics.peak - lastPaintedLeftPeak) < 0.003f
        && std::abs (rightBallistics.peak - lastPaintedRightPeak) < 0.003f)
        return;

    lastPaintedLeft = leftBallistics.level;
    lastPaintedRight = rightBallistics.level;
    lastPaintedLeftPeak = leftBallistics.peak;
    lastPaintedRightPeak = rightBallistics.peak;
    repaint();
}

void TaikorMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panelColour.darker (0.35f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    bounds = bounds.reduced (5.0f, 5.0f);
    const auto barHeight = (bounds.getHeight() - 3.0f) * 0.5f;

    const auto drawBar = [&] (juce::Rectangle<float> area,
                              const taikor::ui::MeterBallistics& ballistics)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area, 2.0f);

        const auto filled = area.withWidth (area.getWidth() * ballistics.level);
        const auto hot = ballistics.level
                       > taikor::ui::meterPositionForLinear (0.708f, floorDecibels);
        g.setColour (hot ? accentColour : accentDim.brighter (0.55f));
        g.fillRoundedRectangle (filled, 2.0f);

        const auto peakX = area.getX() + area.getWidth() * ballistics.peak;
        g.setColour (textColour.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (peakX - 1.0f, area.getY(), 2.0f,
                                            area.getHeight()));
    };

    drawBar (bounds.removeFromTop (barHeight), leftBallistics);
    bounds.removeFromTop (3.0f);
    drawBar (bounds.removeFromTop (barHeight), rightBallistics);
}

std::unique_ptr<juce::AccessibilityHandler> TaikorMeter::createAccessibilityHandler()
{
    class MeterHandler final : public juce::AccessibilityHandler
    {
    public:
        explicit MeterHandler (TaikorMeter& meterToUse)
            : juce::AccessibilityHandler (meterToUse,
                                          juce::AccessibilityRole::staticText),
              owner (meterToUse)
        {
        }

        juce::String getHelp() const override
        {
            const auto decibels = [] (float position)
            {
                return juce::String (
                    juce::Decibels::gainToDecibels (
                        taikor::ui::linearForMeterPosition (position, floorDecibels)),
                    1);
            };
            return "Output " + decibels (owner.leftBallistics.level) + " dB left, "
                 + decibels (owner.rightBallistics.level) + " dB right.";
        }

    private:
        TaikorMeter& owner;
    };

    return std::make_unique<MeterHandler> (*this);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

TaikorAudioProcessorEditor::TaikorAudioProcessorEditor (TaikorAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      audioProcessor (processor),
      tooltipWindow (this, 650)
{
    setLookAndFeel (&lookAndFeel);

    logoLabel.setText ("TAIKOR", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (juce::FontOptions (28.0f).withStyle ("Bold")));
    logoLabel.setColour (juce::Label::textColourId, textColour);
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("PHYSICALLY MODELED TAIKO", juce::dontSendNotification);
    editionLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    editionLabel.setColour (juce::Label::textColourId, mutedText);
    editionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);
    addAndMakeVisible (meter);

    panicButton.setColour (juce::TextButton::buttonColourId, panelColour);
    panicButton.setColour (juce::TextButton::textColourOffId, textColour);
    panicButton.setTooltip ("Silence every sounding stroke immediately");
    panicButton.onClick = [this] { audioProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    addAndMakeVisible (headDisplay);

    headCaption.setText ("THE DRUM", juce::dontSendNotification);
    headCaption.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    headCaption.setColour (juce::Label::textColourId, mutedText);
    headCaption.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (headCaption);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        auto pad = std::make_unique<TaikorPad> (articulation);
        pad->onClick = [this, articulation]
        {
            audioProcessor.triggerFromUi (articulation, selectedOctave);
        };
        addAndMakeVisible (*pad);
        pads[index] = std::move (pad);
    }

    octaveLabel.setText ("OCTAVE - THE DRUM'S PITCH", juce::dontSendNotification);
    octaveLabel.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    octaveLabel.setColour (juce::Label::textColourId, mutedText);
    octaveLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (octaveLabel);

    for (int index = 0; index < octaveCount; ++index)
    {
        const auto octave = taikor::lowestOctaveOffset + index;
        auto button = std::make_unique<juce::TextButton> (
            octaveName (octave) + "  " + octaveDescription (octave));
        button->setColour (juce::TextButton::buttonColourId, panelColour);
        button->setColour (juce::TextButton::textColourOffId, textColour);
        button->setColour (juce::TextButton::textColourOnId, textColour);
        button->setClickingTogglesState (false);
        button->setTooltip ("Play the " + octaveDescription (octave).toLowerCase()
                            + " drum: every stroke moves to the "
                            + octaveName (octave) + " octave");
        button->onClick = [this, octave] { selectOctave (octave); };
        addAndMakeVisible (*button);
        octaveButtons[static_cast<std::size_t> (index)] = std::move (button);
    }

    const auto deckLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        label.setColour (juce::Label::textColourId, mutedText);
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    };

    deckLabel (drumDeckLabel, "THE DRUM");
    deckLabel (strokeDeckLabel, "THE STROKE");
    deckLabel (microphoneDeckLabel, "CLOSE PAIR AND OUTPUT");

    namespace ids = taikor::parameters;
    addKnob (sizeKnob, ids::headDiameter,
             "Head diameter. Pitch follows one over the radius, so this moves the "
             "whole drum without changing the ratios between its modes.");
    addKnob (depthKnob, ids::bodyDepth,
             "Body depth. A shallow body has a stiffer air spring, which pushes the "
             "breathing mode further above the fundamental.");
    addKnob (tensionKnob, ids::tension,
             "Head tension. Wave speed is the square root of tension over the head's "
             "areal density, so this and the head material together set the pitch.");
    addKnob (headMaterialKnob, ids::headMaterial,
             "Head material, from a thin synthetic film to a thick cowhide. Sets both "
             "the head's weight and how much it loses per cycle.");
    addKnob (shellMaterialKnob, ids::shellMaterial,
             "Shell material, from light laminated ply to dense carved zelkova. Moves "
             "the body's ring modes, their Q, and how much the rim absorbs.");
    addKnob (resonantKnob, ids::resonantTension,
             "Tension of the far head relative to the batter head. Detuning the pair "
             "is the traditional way to lengthen or shorten the boom.");
    addKnob (cavityKnob, ids::cavityCoupling,
             "How strongly the enclosed air ties the two heads together. Only the "
             "axisymmetric modes couple; nothing else compresses the cavity.");
    addKnob (headDampingKnob, ids::headDamping,
             "Extra loss in the head on top of the material's own.");
    addKnob (shellResonanceKnob, ids::shellResonance,
             "How much of the wooden body colours an ordinary head stroke.");
    addKnob (pitchKnob, ids::pitch,
             "Musical transposition, applied as head tension because that is what "
             "tuning a drum is.");

    addKnob (hardnessKnob, ids::bachiHardness,
             "Bachi hardness, from a felt-wrapped beater to seasoned oak. Sets the "
             "Hertz contact stiffness and therefore how long the stick stays down.");
    addKnob (strikePositionKnob, ids::strikePosition,
             "Moves every stroke towards the centre or towards the rim, on top of "
             "the position its own articulation already asks for.");
    addKnob (velocityDepthKnob, ids::velocityDepth,
             "How far MIDI velocity moves the impact speed. The timbre follows on "
             "its own: contact time goes as impact speed to the minus one fifth.");
    addKnob (tensionModKnob, ids::tensionModulation,
             "Attack pitch glide. A hard stroke stretches the head, raising its "
             "tension until the stroke decays.");
    addKnob (strikeNoiseKnob, ids::strikeNoise,
             "Level of the broadband contact noise the stick makes on the hide.");
    addKnob (humaniseKnob, ids::humanise,
             "Per-stroke variation in position, angle, impact speed and contact time.");
    addKnob (octaveBodyKnob, ids::octaveBody,
             "How an octave is realised: at zero the same drum is tuned up, at full "
             "the drum itself halves in size. Both reach the same pitch and neither "
             "sounds the same, because the air does not scale with the drum.");

    addKnob (micDistanceKnob, ids::micDistance,
             "How far the close pair stands off the head. Near in it reads the shape "
             "of the membrane and the image opens; further back only what the drum "
             "radiates survives, and the pair narrows.");
    addKnob (micSpreadKnob, ids::micSpread,
             "How far apart the pair is spread across the head. This is where the "
             "stereo comes from: two points see different signs of every mode with a "
             "circumferential order.");
    addKnob (widthKnob, ids::stereoWidth, "Width trim on the finished pair.");
    addKnob (driveKnob, ids::drive, "Gentle output-stage saturation.");
    addKnob (outputKnob, ids::output, "Output level.");

    selectOctave (0);

    setResizable (true, true);
    setResizeLimits (minimumWidth, minimumHeight, maximumWidth, maximumHeight);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (designWidth)
                                          / static_cast<double> (designHeight));
    setSize (designWidth, designHeight);

    startTimerHz (30);
}

TaikorAudioProcessorEditor::~TaikorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TaikorAudioProcessorEditor::addKnob (TaikorKnob& knob,
                                          const juce::String& parameterId,
                                          const juce::String& description)
{
    addAndMakeVisible (knob);
    // The knob's own caption is already set from its constructor; this attaches
    // the description used by tooltips and by assistive technology.
    knob.setLabelText (knob.getName().isNotEmpty() ? knob.getName()
                                                   : parameterId.toUpperCase(),
                       description);
    knob.slider.setName (knob.getName());
    attachments.push_back (std::make_unique<SliderAttachment> (
        audioProcessor.parameters, parameterId, knob.slider));
}

void TaikorAudioProcessorEditor::selectOctave (int octaveOffset)
{
    selectedOctave = juce::jlimit (taikor::lowestOctaveOffset,
                                   taikor::highestOctaveOffset, octaveOffset);

    for (int index = 0; index < octaveCount; ++index)
    {
        const auto octave = taikor::lowestOctaveOffset + index;
        auto& button = octaveButtons[static_cast<std::size_t> (index)];
        if (button != nullptr)
            button->setToggleState (octave == selectedOctave,
                                    juce::dontSendNotification);
    }

    for (auto& pad : pads)
        if (pad != nullptr)
            pad->setOctaveOffset (selectedOctave);
}

TaikorAudioProcessorEditor::LayoutAreas
TaikorAudioProcessorEditor::calculateLayout() const
{
    LayoutAreas areas;
    auto bounds = getLocalBounds().reduced (14);

    areas.header = bounds.removeFromTop (juce::roundToInt (bounds.getHeight() * 0.082f));
    bounds.removeFromTop (10);

    areas.pads = bounds.removeFromTop (juce::roundToInt (bounds.getHeight() * 0.185f));
    bounds.removeFromTop (8);
    areas.octaveStrip = bounds.removeFromTop (juce::roundToInt (bounds.getHeight() * 0.09f));
    bounds.removeFromTop (10);

    auto middle = bounds.removeFromTop (juce::roundToInt (bounds.getHeight() * 0.52f));
    areas.head = middle.removeFromLeft (juce::roundToInt (middle.getWidth() * 0.32f));
    middle.removeFromLeft (12);
    areas.drumDeck = middle;

    bounds.removeFromTop (10);
    areas.strokeDeck = bounds.removeFromTop (juce::roundToInt (bounds.getHeight() * 0.5f));
    bounds.removeFromTop (8);
    areas.microphoneDeck = bounds;

    return areas;
}

void TaikorAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient background { backgroundTop, bounds.getCentreX(), bounds.getY(),
                                      backgroundBottom, bounds.getCentreX(),
                                      bounds.getBottom(), false };
    g.setGradientFill (background);
    g.fillRect (bounds);

    // A faint deterministic grain, so the panel reads as lacquered wood rather
    // than as a flat fill. Seeded, so it is identical on every repaint and the
    // committed screenshot stays byte-stable between runs.
    juce::Random grain { 0x7a1c0 };
    g.setColour (juce::Colours::white.withAlpha (0.014f));
    for (int index = 0; index < 900; ++index)
    {
        const auto x = grain.nextFloat() * bounds.getWidth();
        const auto y = grain.nextFloat() * bounds.getHeight();
        g.fillRect (x, y, 1.0f + grain.nextFloat() * 8.0f, 1.0f);
    }

    const auto areas = calculateLayout();
    const auto panel = [&g] (juce::Rectangle<int> area)
    {
        if (area.isEmpty())
            return;
        const auto rectangle = area.toFloat().expanded (6.0f, 4.0f);
        g.setColour (panelColour.withAlpha (0.55f));
        g.fillRoundedRectangle (rectangle, 6.0f);
        g.setColour (panelEdge.withAlpha (0.65f));
        g.drawRoundedRectangle (rectangle, 6.0f, 1.0f);
    };

    panel (areas.pads);
    panel (areas.octaveStrip);
    panel (areas.drumDeck);
    panel (areas.strokeDeck);
    panel (areas.microphoneDeck);
}

void TaikorAudioProcessorEditor::resized()
{
    const auto areas = calculateLayout();

    auto header = areas.header;
    auto branding = header.removeFromLeft (juce::roundToInt (header.getWidth() * 0.30f));
    logoLabel.setBounds (branding.removeFromTop (juce::roundToInt (branding.getHeight() * 0.62f)));
    editionLabel.setBounds (branding);

    panicButton.setBounds (header.removeFromRight (96).reduced (0, 8));
    header.removeFromRight (10);
    meter.setBounds (header.removeFromRight (juce::roundToInt (header.getWidth() * 0.44f))
                         .reduced (0, 6));
    header.removeFromRight (10);
    statusDisplay.setBounds (header.removeFromRight (juce::jmin (190, header.getWidth()))
                                 .reduced (0, 8));

    // Twelve stroke pads in one row: the vocabulary is an octave, so it reads
    // as one.
    const auto padArea = areas.pads;
    const auto padLayout = taikor::ui::rowLayout (
        padArea.getWidth(), static_cast<int> (taikor::articulationCount), 6,
        static_cast<int> (taikor::articulationCount));
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        if (pads[index] == nullptr)
            continue;
        const auto x = padArea.getX()
                     + taikor::ui::cellOffset (padLayout, 6, static_cast<int> (index));
        pads[index]->setBounds (x, padArea.getY(), padLayout.cellSize,
                                padArea.getHeight());
    }

    auto octaveStrip = areas.octaveStrip;
    octaveLabel.setBounds (octaveStrip.removeFromLeft (
        juce::roundToInt (octaveStrip.getWidth() * 0.24f)));
    const auto octaveLayout =
        taikor::ui::rowLayout (octaveStrip.getWidth(), octaveCount, 6, octaveCount);
    for (int index = 0; index < octaveCount; ++index)
    {
        auto& button = octaveButtons[static_cast<std::size_t> (index)];
        if (button == nullptr)
            continue;
        const auto x = octaveStrip.getX()
                     + taikor::ui::cellOffset (octaveLayout, 6, index);
        button->setBounds (x, octaveStrip.getY() + 2, octaveLayout.cellSize,
                           octaveStrip.getHeight() - 4);
    }

    auto headArea = areas.head;
    headCaption.setBounds (headArea.removeFromTop (16));
    headDisplay.setBounds (headArea);

    // Knob decks. Each deck gets a caption row and then an even grid, wrapped
    // over as many rows as the deck was asked for.
    const auto layoutDeck = [] (juce::Rectangle<int> area, juce::Label& label,
                                int rows, std::vector<TaikorKnob*> knobs)
    {
        auto working = area;
        label.setBounds (working.removeFromTop (15));

        const auto count = static_cast<int> (knobs.size());
        if (count <= 0 || rows <= 0 || working.isEmpty())
            return;

        const auto columns = (count + rows - 1) / rows;
        const auto rowHeight = working.getHeight() / rows;

        for (int row = 0; row < rows; ++row)
        {
            const auto first = row * columns;
            if (first >= count)
                break;

            const auto inRow = juce::jmin (columns, count - first);
            auto rowArea = working.removeFromTop (rowHeight);
            const auto layout =
                taikor::ui::rowLayout (rowArea.getWidth(), columns, 6, inRow);

            for (int index = 0; index < inRow; ++index)
                if (auto* knob = knobs[static_cast<std::size_t> (first + index)])
                    knob->setBounds (rowArea.getX()
                                         + taikor::ui::cellOffset (layout, 6, index),
                                     rowArea.getY(), layout.cellSize,
                                     rowArea.getHeight());
        }
    };

    // The groups match their captions: the drum, then how it is struck, then
    // what is listening to it.
    layoutDeck (areas.drumDeck, drumDeckLabel, 2,
                { &sizeKnob, &depthKnob, &tensionKnob, &headMaterialKnob,
                  &shellMaterialKnob, &resonantKnob, &cavityKnob, &headDampingKnob,
                  &shellResonanceKnob, &pitchKnob });
    layoutDeck (areas.strokeDeck, strokeDeckLabel, 1,
                { &hardnessKnob, &strikePositionKnob, &velocityDepthKnob,
                  &tensionModKnob, &strikeNoiseKnob, &humaniseKnob, &octaveBodyKnob });
    layoutDeck (areas.microphoneDeck, microphoneDeckLabel, 1,
                { &micDistanceKnob, &micSpreadKnob, &widthKnob, &driveKnob,
                  &outputKnob });
}

void TaikorAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (audioProcessor.getActiveVoiceCount(),
                             audioProcessor.isEngineReady(),
                             audioProcessor.getCurrentSampleRateForDisplay());
    meter.setLevels (audioProcessor.getOutputLevel (0),
                     audioProcessor.getOutputLevel (1));

    taikor::DrumVisualState visual;
    audioProcessor.getVisualState (visual);
    headDisplay.setStrike (visual.strikeRadius, visual.strikeAngle, visual.strikeLevel,
                           visual.lastArticulation);

    const auto engineParameters = audioProcessor.snapshotEngineParameters();
    headDisplay.setMicrophones (engineParameters.micSpread, engineParameters.micDistance);

    const auto measurements = audioProcessor.measureDrum (selectedOctave);
    headDisplay.setMeasurements (measurements.loadedFundamentalHz,
                                 measurements.breathingModeHz,
                                 measurements.radiusMetres * 200.0f,
                                 measurements.fundamentalT60Seconds);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        if (pads[index] == nullptr)
            continue;

        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto counter = audioProcessor.getTriggerCounter (articulation);
        if (counter != observedTriggerCounters[index])
        {
            observedTriggerCounters[index] = counter;
            pads[index]->triggerFlash();
        }
        else
        {
            pads[index]->advanceFlash();
        }
    }
}

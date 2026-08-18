#include "PluginEditor.h"

#include <BinaryData.h>

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

// Sumi ink, old urushi lacquer, washi and one restrained shu-vermilion seal.
// The palette deliberately stays close to materials a drum maker would touch;
// colour is reserved for interaction and never used as generic chrome.
const juce::Colour backgroundTop { 0xff201713 };
const juce::Colour backgroundBottom { 0xff100d0b };
const juce::Colour panelColour { 0xff281c17 };
const juce::Colour panelRaised { 0xff34241c };
const juce::Colour panelEdge { 0xff594431 };
const juce::Colour hideColour { 0xffddc8a4 };
const juce::Colour washiColour { 0xffe5d6b8 };
const juce::Colour accentColour { 0xffc34a2e };
const juce::Colour accentDim { 0xff6e3023 };
const juce::Colour brassColour { 0xffb58a45 };
const juce::Colour textColour { 0xffeadfc9 };
const juce::Colour mutedText { 0xffa4937d };

juce::Colour roleColour (TaikorKnob::VisualRole role) noexcept
{
    switch (role)
    {
        case TaikorKnob::VisualRole::Drum:       return brassColour;
        case TaikorKnob::VisualRole::Stroke:     return accentColour;
        case TaikorKnob::VisualRole::Microphone: return juce::Colour { 0xff5f8584 };
        case TaikorKnob::VisualRole::Master:     return washiColour.darker (0.10f);
    }
    return accentColour;
}

juce::Font displayFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (
        juce::Font::getDefaultSerifFontName(), height, style));
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

// The drum an octave plays, read straight off the engine's own table rather
// than restated here. Each octave is a different instrument of the family, so
// the octave row names instruments: there is nothing for the editor to have its
// own opinion about.
juce::String octaveDescription (int octaveOffset)
{
    const auto& name = taikor::getDrumDescription (octaveOffset).displayName;
    return juce::String (name.data(), name.size());
}

juce::String octaveSummary (int octaveOffset)
{
    const auto& summary = taikor::getDrumDescription (octaveOffset).summary;
    return juce::String (summary.data(), summary.size());
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
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
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
    const auto thickness = juce::jmax (2.0f, radius * 0.105f);

    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centreX, centreY, radius - thickness * 0.5f,
                         radius - thickness * 0.5f, 0.0f, rotaryStartAngle,
                         rotaryEndAngle, true);
    g.setColour (panelEdge.withAlpha (0.72f));
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

    // Sparse index cuts read like marks made into wood, not synthesizer scale
    // furniture. They also make the usable arc legible without another border.
    g.setColour (mutedText.withAlpha (0.30f));
    for (int tick = 0; tick < 7; ++tick)
    {
        const auto tickAngle = rotaryStartAngle
                             + static_cast<float> (tick) / 6.0f
                                   * (rotaryEndAngle - rotaryStartAngle);
        const auto outer = radius + 0.5f;
        const auto inner = outer - juce::jmax (2.0f, radius * 0.10f);
        const auto tickSin = std::sin (tickAngle);
        const auto tickCos = std::cos (tickAngle);
        g.drawLine (centreX + tickSin * inner,
                    centreY - tickCos * inner,
                    centreX + tickSin * outer,
                    centreY - tickCos * outer, 0.8f);
    }

    const auto knobRadius = radius - thickness * 1.75f;
    juce::ColourGradient body { panelRaised.brighter (0.14f), centreX,
                                centreY - knobRadius, panelColour.darker (0.28f),
                                centreX, centreY + knobRadius, false };
    g.setGradientFill (body);
    g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (brassColour.withAlpha (0.28f));
    g.drawEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    juce::Path pointer;
    const auto pointerLength = knobRadius * 0.76f;
    const auto pointerThickness = juce::jmax (1.6f, knobRadius * 0.11f);
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength,
                                 pointerThickness, pointerLength,
                                 pointerThickness * 0.5f);
    pointer.applyTransform (
        juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (fill.brighter (0.20f));
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
    g.fillRoundedRectangle (bounds, 2.0f);

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (accentColour.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
    }
}

juce::Font TaikorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return displayFont (
        juce::jlimit (11.0f, 16.0f, static_cast<float> (buttonHeight) * 0.42f),
        juce::Font::bold);
}

// ---------------------------------------------------------------------------
// Stroke pad
// ---------------------------------------------------------------------------

TaikorPad::TaikorPad (taikor::Articulation articulationToUse, int octaveOffsetToUse)
    : juce::Button (juce::String (
          taikor::getArticulationDisplayName (articulationToUse).data(),
          taikor::getArticulationDisplayName (articulationToUse).size())),
      articulation (articulationToUse),
      octaveOffset (octaveOffsetToUse)
{
    refreshNoteText();
    setWantsKeyboardFocus (false);
}

void TaikorPad::refreshNoteText()
{
    const auto note = taikor::midiNoteFor (articulation, octaveOffset);
    keyText = juce::MidiMessage::getMidiNoteName (note, true, true,
                                                  octaveNumberForMiddleC);
    noteText = keyText + " (" + juce::String (note) + ")";
    drumNameText = octaveName (octaveOffset) + " " + taikor::getDrumDescription (octaveOffset).displayName.data();
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

    // The pad now plays a different note, so anything holding the old wording
    // has to be told. A reader that had already fetched the help text would
    // otherwise go on announcing the octave that was selected when it asked.
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
}

juce::String TaikorPad::accessibleHelpText() const
{
    const auto& metadata = taikor::getArticulationMetadata (articulation);
    return drumNameText + " " + juce::String (metadata.description.data(), metadata.description.size())
         + ". Plays " + noteText;
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
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    auto base = panelColour;
    if (isButtonDown)
        base = base.brighter (0.24f);
    else if (isMouseOver)
        base = base.brighter (0.11f);
    else if (selected)
        base = panelRaised;

    juce::ColourGradient gradient { base.brighter (0.08f), bounds.getCentreX(),
                                    bounds.getY(), base.darker (0.18f),
                                    bounds.getCentreX(), bounds.getBottom(), false };
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 3.0f);

    if (flashLevel > 0.0f)
    {
        g.setColour (accentColour.withAlpha (flashLevel * 0.48f));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    // A tiny head map replaces three repeated lines of copy. The mark is the
    // actual strike geometry: centre, rim, muted centre, or head-and-hoop.
    const auto mapSize = juce::jlimit (18.0f, 27.0f, bounds.getHeight() * 0.34f);
    auto headMap = juce::Rectangle<float> (mapSize, mapSize)
                       .withCentre ({ bounds.getCentreX(),
                                      bounds.getY() + bounds.getHeight() * 0.33f });
    g.setColour (mutedText.withAlpha (selected ? 0.70f : 0.45f));
    g.drawEllipse (headMap, 1.0f);

    const auto centre = headMap.getCentre();
    auto strikePoint = centre;
    if (articulation == taikor::Articulation::Ka)
        strikePoint.x = headMap.getRight() - mapSize * 0.13f;
    else if (articulation == taikor::Articulation::DonRim)
        strikePoint.x = headMap.getRight();
    else
        strikePoint.x -= mapSize * 0.08f;

    g.setColour (selected ? accentColour : brassColour.withAlpha (0.76f));
    const auto dot = juce::jmax (3.0f, mapSize * 0.17f);
    g.fillEllipse (strikePoint.x - dot * 0.5f, strikePoint.y - dot * 0.5f,
                   dot, dot);

    if (articulation == taikor::Articulation::Tsu)
    {
        g.setColour (textColour.withAlpha (0.72f));
        g.drawLine (headMap.getX() + 3.0f, headMap.getBottom() - 4.0f,
                    headMap.getRight() - 3.0f, headMap.getY() + 4.0f, 1.4f);
    }
    else if (articulation == taikor::Articulation::DonRim)
    {
        g.setColour (accentColour.withAlpha (0.72f));
        juce::Path rim;
        rim.addCentredArc (centre.x, centre.y, mapSize * 0.59f, mapSize * 0.59f,
                           0.0f, -0.5f, 1.55f, true);
        g.strokePath (rim, juce::PathStrokeType (1.5f));
    }

    g.setColour (selected ? textColour : mutedText.brighter (0.08f));
    g.setFont (displayFont (
        juce::jlimit (12.5f, 17.0f, bounds.getHeight() * 0.23f),
        juce::Font::bold));
    g.drawText (keyText, bounds.withTrimmedTop (bounds.getHeight() * 0.56f)
                               .reduced (4.0f, 1.0f),
                juce::Justification::centred, false);

    if (selected)
    {
        g.setColour (accentColour.withAlpha (0.95f));
        g.fillRect (bounds.getX() + 5.0f, bounds.getY(),
                    bounds.getWidth() - 10.0f, 2.0f);
    }
}

std::unique_ptr<juce::AccessibilityHandler> TaikorPad::createAccessibilityHandler()
{
    class PadHandler final : public juce::AccessibilityHandler
    {
    public:
        explicit PadHandler (TaikorPad& padToUse)
            : juce::AccessibilityHandler (
                  padToUse, juce::AccessibilityRole::button,
                  juce::AccessibilityActions().addAction (
                      juce::AccessibilityActionType::press,
                      [&padToUse] { padToUse.triggerClick(); })),
              pad (padToUse)
        {
        }

        // Asked of the pad each time rather than captured when the handler is
        // built. The octave strip moves every pad to a different MIDI note and
        // the handler outlives that change, so a captured string would announce
        // whichever octave happened to be selected when the editor opened.
        juce::String getHelp() const override { return pad.accessibleHelpText(); }

    private:
        TaikorPad& pad;
    };

    return std::make_unique<PadHandler> (*this);
}

// ---------------------------------------------------------------------------
// Drum row selector
// ---------------------------------------------------------------------------

TaikorDrumButton::TaikorDrumButton (int octaveOffsetToUse, juce::Image atlas)
    : juce::Button (octaveName (octaveOffsetToUse) + " "
                    + octaveDescription (octaveOffsetToUse)),
      noteName (octaveName (octaveOffsetToUse)),
      drumName (octaveDescription (octaveOffsetToUse))
{
    // The atlas is one continuous painting. These deliberately overlap the
    // quarter boundaries slightly because the two deep drums cast wide stands;
    // each resulting view is still independently contained in its row header.
    const std::array<juce::Rectangle<float>, 4> crops {{
        { 0.000f, 0.055f, 0.318f, 0.855f },
        { 0.294f, 0.195f, 0.300f, 0.720f },
        { 0.570f, 0.155f, 0.220f, 0.705f },
        { 0.765f, 0.415f, 0.235f, 0.465f },
    }};

    const auto cropIndex = static_cast<std::size_t> (
        juce::jlimit (0, 3, octaveOffsetToUse - taikor::lowestOctaveOffset));
    const auto crop = crops[cropIndex];
    if (atlas.isValid())
    {
        const auto source = juce::Rectangle<int> (
            juce::roundToInt (crop.getX() * static_cast<float> (atlas.getWidth())),
            juce::roundToInt (crop.getY() * static_cast<float> (atlas.getHeight())),
            juce::roundToInt (crop.getWidth() * static_cast<float> (atlas.getWidth())),
            juce::roundToInt (crop.getHeight() * static_cast<float> (atlas.getHeight())))
                                .getIntersection (atlas.getBounds());
        drumPainting = atlas.getClippedImage (source);
    }

    setTitle (noteName + " " + drumName);
    setDescription (octaveSummary (octaveOffsetToUse));
    setTooltip (drumName + " - " + octaveSummary (octaveOffsetToUse)
                + ". Selects this drum for the live head readout.");
}

void TaikorDrumButton::paintButton (juce::Graphics& g, bool isMouseOver,
                                    bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    juce::ColourGradient paper { washiColour.brighter (0.06f), bounds.getX(),
                                 bounds.getY(), washiColour.darker (0.16f),
                                 bounds.getRight(), bounds.getBottom(), false };
    g.setGradientFill (paper);
    g.fillRoundedRectangle (bounds, 3.0f);

    auto artArea = bounds.withWidth (bounds.getWidth() * 0.55f).reduced (5.0f, 3.0f);
    if (drumPainting.isValid())
    {
        g.setOpacity (isButtonDown ? 0.76f : (isMouseOver ? 1.0f : 0.90f));
        g.drawImageWithin (drumPainting, juce::roundToInt (artArea.getX()),
                           juce::roundToInt (artArea.getY()),
                           juce::roundToInt (artArea.getWidth()),
                           juce::roundToInt (artArea.getHeight()),
                           juce::RectanglePlacement::centred
                               | juce::RectanglePlacement::onlyReduceInSize,
                           false);
        g.setOpacity (1.0f);
    }

    if (isMouseOver || isButtonDown)
    {
        g.setColour (accentColour.withAlpha (isButtonDown ? 0.18f : 0.08f));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    if (getToggleState())
    {
        g.setColour (accentColour.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (accentColour);
        g.fillRoundedRectangle (bounds.withWidth (5.0f), 2.0f);
    }

    auto copy = bounds.withTrimmedLeft (bounds.getWidth() * 0.52f)
                      .reduced (4.0f, 5.0f);
    g.setColour (accentColour.darker (0.12f));
    g.setFont (displayFont (juce::jlimit (9.5f, 12.0f, bounds.getHeight() * 0.15f),
                            juce::Font::bold));
    g.drawText (noteName, copy.removeFromTop (copy.getHeight() * 0.38f),
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colour { 0xff241914 });
    g.setFont (displayFont (juce::jlimit (9.5f, 12.5f, bounds.getHeight() * 0.16f),
                            juce::Font::bold));
    g.drawFittedText (drumName, copy.toNearestInt(), juce::Justification::centredLeft,
                      2, 0.78f);

    if (hasKeyboardFocus (true))
    {
        g.setColour (accentColour.withAlpha (0.90f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.2f);
    }
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------

TaikorKnob::TaikorKnob (juce::String name, ValueStyle style, VisualRole roleToUse)
    : role (roleToUse)
{
    setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, roleColour (roleToUse));
    slider.setColour (juce::Slider::textBoxTextColourId, textColour);
    slider.setColour (juce::Slider::textBoxOutlineColourId,
                      juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId,
                      juce::Colours::transparentBlack);

    switch (style)
    {
        case ValueStyle::Plain:       break;
        case ValueStyle::Percent:     slider.setTextValueSuffix (" %"); break;
        case ValueStyle::Semitones:   slider.setTextValueSuffix (" st"); break;
        case ValueStyle::Degrees:     slider.setTextValueSuffix (" deg"); break;
        case ValueStyle::Centimetres: slider.setTextValueSuffix (" cm"); break;
        case ValueStyle::Decibels:    slider.setTextValueSuffix (" dB"); break;
    }

    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, mutedText.brighter (0.12f));
    label.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
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
    auto bounds = getLocalBounds().reduced (2, 0);
    label.setBounds (bounds.removeFromTop (13));
    slider.setBounds (bounds.reduced (1, 0));
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
    const auto radius = taikor::ui::clamp (normalisedRadius, 0.0f, 1.0f);
    const auto bounded = taikor::ui::clamp (level, 0.0f, 1.0f);

    // Every drawn property has to be in this decision, not just the level. A
    // roll played at one velocity moves the marker around the head and changes
    // the articulation without changing the level at all, and testing the level
    // alone left the display showing the first stroke of the roll for as long
    // as it went on.
    const bool moved = std::abs (radius - strikeRadius) >= 0.002f
                    || std::abs (angleRadians - strikeAngle) >= 0.002f
                    || articulation != lastArticulation
                    || std::abs (bounded - strikeLevel) >= 0.002f;

    strikeRadius = radius;
    strikeAngle = angleRadians;
    lastArticulation = articulation;
    strikeLevel = bounded;

    if (moved)
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

// The engine reports zero when no membrane mode of the drum survives the
// renderer's Nyquist cutoff, which is a drum with no pitch rather than a drum at
// nought hertz - a 15 cm head at the tension ceiling, taken to the top of the
// keyboard, has its own fundamental above 25 kHz. Printing "0.0 Hz" there would
// be the same untruth in the other direction. See
// TaikoEngine::DrumMeasurements::soundingHz.
static juce::String describeSoundingPitch (float soundingHz)
{
    return soundingHz > 0.0f ? juce::String (soundingHz, 1) + " Hz"
                             : juce::String ("No pitch");
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

    // An incomplete ink circle gives the technical head map a place in the
    // same visual world as the row paintings, without obscuring live data.
    juce::Path enso;
    enso.addCentredArc (centre.x, centre.y, headRadius * 1.23f,
                        headRadius * 1.23f, -0.08f, -0.15f, pi * 1.72f, true);
    g.setColour (washiColour.withAlpha (0.055f));
    g.strokePath (enso, juce::PathStrokeType (headRadius * 0.10f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // The shell, seen edge on behind the head.
    g.setColour (panelColour.darker (0.38f));
    g.fillEllipse (centre.x - headRadius * 1.10f, centre.y - headRadius * 1.10f,
                   headRadius * 2.20f, headRadius * 2.20f);
    g.setColour (brassColour.withAlpha (0.48f));
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
    g.setColour (juce::Colour { 0xff554638 });
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
    // radius and arc the model actually reads the head at - the same constants
    // resolveDrumFor uses - and they fade as the pair backs off. The arc kept
    // the engine's old 2.2-radian spread after the model narrowed to a true
    // close pair, so at full spread the display showed capsules a hundred and
    // twenty-six degrees apart while the drum being heard used fifty.
    const auto micRingRadius = headRadius * (0.10f + 0.68f * micSpread);
    constexpr float micReference = 0.60f;
    const auto separation = 0.9f * micSpread;
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
    g.setFont (juce::Font (juce::FontOptions (10.5f)));

    const auto line = readout.getHeight() / 2.0f;
    g.drawText (juce::String (diameter, 1) + " cm head  "
                    + juce::String::fromUTF8 ("\xc2\xb7") + "  "
                    + juce::String (tail, 2) + " s tail",
                readout.removeFromTop (line), juce::Justification::centred, false);

    g.setColour (textColour);
    g.setFont (displayFont (12.0f, juce::Font::bold));
    g.drawText (describeSoundingPitch (fundamental) + "  "
                    + juce::String::fromUTF8 ("\xc2\xb7") + "  breath "
                    + juce::String (breathing, 1) + " Hz",
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
                 + " centimetre head sounding "
                 + (owner.fundamental > 0.0f
                        ? juce::String (owner.fundamental, 1) + " hertz"
                        : juce::String ("no membrane tone at this sample rate"))
                 + " with a " + juce::String (owner.breathing, 1)
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
    if (voices == activeVoices && isReady == ready
        && std::abs (rate - sampleRate) < 0.5)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    repaint();
}

void TaikorStatusDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panelColour.withAlpha (0.38f));
    g.fillRoundedRectangle (bounds, 3.0f);

    bounds = bounds.reduced (10.0f, 4.0f);
    const auto statusDot = juce::jmin (6.0f, bounds.getHeight() * 0.32f);
    g.setColour (isReady ? brassColour : mutedText.withAlpha (0.42f));
    g.fillEllipse (bounds.getX(), bounds.getCentreY() - statusDot * 0.5f,
                   statusDot, statusDot);
    bounds.removeFromLeft (statusDot + 7.0f);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (isReady ? textColour : mutedText);

    const juce::String rateText = rate > 0.0
        ? juce::String (rate / 1000.0, 1) + " kHz"
        : juce::String ("- kHz");
    g.drawText (juce::String (juce::jmax (0, voices)) + " voices  "
                    + juce::String::fromUTF8 ("\xc2\xb7") + "  " + rateText,
                bounds, juce::Justification::centredLeft, false);
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
    g.setColour (panelColour.withAlpha (0.34f));
    g.fillRoundedRectangle (bounds, 3.0f);

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

TaikorAudioProcessorEditor::TaikorAudioProcessorEditor (TaikorAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      audioProcessor (processorToUse),
      tooltipWindow (this, 650)
{
    setLookAndFeel (&lookAndFeel);

    drumAtlas = juce::ImageFileFormat::loadFrom (
        BinaryData::taikodrumatlas_png,
        static_cast<std::size_t> (BinaryData::taikodrumatlas_pngSize));
    backgroundPainting = juce::ImageFileFormat::loadFrom (
        BinaryData::taikorsumibackground_png,
        static_cast<std::size_t> (BinaryData::taikorsumibackground_pngSize));

    logoLabel.setText ("TAIKOR", juce::dontSendNotification);
    logoLabel.setFont (displayFont (30.0f, juce::Font::bold));
    logoLabel.setColour (juce::Label::textColourId, textColour);
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("PHYSICAL TAIKO MODEL", juce::dontSendNotification);
    editionLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
    editionLabel.setColour (juce::Label::textColourId, mutedText);
    editionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (statusDisplay);
    addAndMakeVisible (meter);

    panicButton.setColour (juce::TextButton::buttonColourId, accentDim);
    panicButton.setColour (juce::TextButton::textColourOffId, textColour);
    panicButton.setTooltip ("Silence every sounding stroke immediately");
    panicButton.onClick = [this] { audioProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    sealLabel.setText (juce::String::fromUTF8 ("\xe9\xbc\x93"),
                       juce::dontSendNotification);
    sealLabel.setFont (displayFont (19.0f, juce::Font::bold));
    sealLabel.setColour (juce::Label::backgroundColourId, accentColour);
    sealLabel.setColour (juce::Label::textColourId, washiColour);
    sealLabel.setJustificationType (juce::Justification::centred);
    sealLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sealLabel);

    addAndMakeVisible (headDisplay);

    headCaption.setText ("HEAD RESPONSE", juce::dontSendNotification);
    headCaption.setFont (displayFont (11.0f, juce::Font::bold));
    headCaption.setColour (juce::Label::textColourId, mutedText);
    headCaption.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (headCaption);

    gridCaption.setText ("PLAYING SURFACE", juce::dontSendNotification);
    gridCaption.setFont (displayFont (11.0f, juce::Font::bold));
    gridCaption.setColour (juce::Label::textColourId, mutedText);
    gridCaption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (gridCaption);

    for (std::size_t artIdx = 0; artIdx < taikor::articulationCount; ++artIdx)
    {
        const auto articulation = static_cast<taikor::Articulation> (artIdx);
        const auto& metadata = taikor::getArticulationMetadata (articulation);
        auto& label = strokeLabels[artIdx];
        label.setText (juce::String (metadata.displayName.data(),
                                     metadata.displayName.size()).toUpperCase(),
                       juce::dontSendNotification);
        label.setFont (displayFont (11.5f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, mutedText.brighter (0.12f));
        label.setJustificationType (juce::Justification::centred);
        label.setTooltip (juce::String (metadata.description.data(),
                                        metadata.description.size()));
        addAndMakeVisible (label);
    }

    std::size_t padIdx = 0;
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset; ++octave)
    {
        for (std::size_t artIdx = 0; artIdx < taikor::articulationCount; ++artIdx)
        {
            const auto articulation = static_cast<taikor::Articulation> (artIdx);
            auto pad = std::make_unique<TaikorPad> (articulation, octave);
            pad->onClick = [this, articulation, octave]
            {
                selectOctave (octave);
                audioProcessor.triggerFromUi (articulation, octave);
            };
            addAndMakeVisible (*pad);
            pads[padIdx++] = std::move (pad);
        }
    }

    for (int index = 0; index < octaveCount; ++index)
    {
        const auto octave = taikor::lowestOctaveOffset + index;
        auto button = std::make_unique<TaikorDrumButton> (octave, drumAtlas);
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1, juce::dontSendNotification);
        button->onClick = [this, octave] { selectOctave (octave); };
        addAndMakeVisible (*button);
        octaveButtons[static_cast<std::size_t> (index)] = std::move (button);
    }

    const auto deckLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (displayFont (11.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, mutedText.brighter (0.12f));
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    };

    deckLabel (drumDeckLabel, "BODY & TUNING");
    deckLabel (strokeDeckLabel, "THE STROKE");
    deckLabel (microphoneDeckLabel, "CLOSE PAIR & OUTPUT");

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
             "How much the wooden body rings when Don Rim catches the hoop.");
    addKnob (pitchKnob, ids::pitch,
             "Musical transposition, applied as head tension because that is what "
             "tuning a drum is.");

    addKnob (hardnessKnob, ids::bachiHardness,
             "Bachi hardness, from a felt-wrapped beater to seasoned oak. Sets the "
             "Hertz contact stiffness and therefore how long the stick stays down.");
    addKnob (strikePositionKnob, ids::strikePosition,
             "Moves every stroke towards the centre or towards the rim, on top of "
             "the position its own articulation already asks for.");
    addKnob (strikeAzimuthKnob, ids::strikeAzimuth,
             "Turns the strike around the head. CC16 overrides this angle for "
             "sample-accurate left and right hand placement.");
    addKnob (performerKnob, ids::performer,
             "Selects one of four repeatable players, each with a stable touch "
             "and strike character.");
    addKnob (velocityDepthKnob, ids::velocityDepth,
             "How far MIDI velocity moves the impact speed. The timbre follows on "
             "its own: contact time goes as impact speed to the minus one fifth.");
    addKnob (velocityCurveKnob, ids::velocityCurve,
             "Shapes MIDI velocity before impact speed: Soft opens up quiet playing, "
             "Linear leaves it unchanged, and Hard asks for a firmer hit.");
    addKnob (tensionModKnob, ids::tensionModulation,
             "Attack pitch glide. A hard stroke stretches the head, raising its "
             "tension until the stroke decays.");
    addKnob (strikeNoiseKnob, ids::strikeNoise,
             "Level of the broadband contact noise the stick makes on the hide.");
    addKnob (humaniseKnob, ids::humanise,
             "Per-stroke variation in position, angle, impact speed and contact time.");
    addKnob (octaveBodyKnob, ids::octaveBody,
             "Switches the keyboard between one drum design retuned across four "
             "rows and four independent physical family drums. Both keep "
             "independent ringing state so chords and overlapping tails remain physical.");

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

    // Double-click resets a knob to its own parameter's declared default -
    // the value a freshly-inserted instance opens with - rather than to
    // whichever value JUCE's slider would otherwise fall back to. Reading it
    // from the parameter keeps every knob in lockstep with its default should
    // that ever change, instead of a second table of defaults kept here.
    if (const auto* parameter = audioProcessor.parameters.getParameter (parameterId))
    {
        knob.slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        knob.slider.setTooltip (knob.slider.getTooltip() + " (double-click to reset)");
    }
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
            pad->setSelected (pad->getOctaveOffset() == selectedOctave);
}

TaikorAudioProcessorEditor::LayoutAreas
TaikorAudioProcessorEditor::calculateLayout() const
{
    LayoutAreas areas;
    auto bounds = getLocalBounds().reduced (18);

    areas.header = bounds.removeFromTop (
        juce::roundToInt (static_cast<float> (bounds.getHeight()) * 0.080f));
    bounds.removeFromTop (12);

    auto middle = bounds.removeFromTop (
        juce::roundToInt (static_cast<float> (bounds.getHeight()) * 0.49f));
    areas.gridArea = middle.removeFromLeft (
        juce::roundToInt (static_cast<float> (middle.getWidth()) * 0.61f));
    middle.removeFromLeft (14);
    areas.rightUpperArea = middle;
    areas.head = areas.rightUpperArea;

    bounds.removeFromTop (12);
    areas.drumDeck = bounds.removeFromTop (
        juce::roundToInt (static_cast<float> (bounds.getHeight()) * 0.43f));
    bounds.removeFromTop (10);
    areas.strokeDeck = bounds.removeFromTop (
        juce::roundToInt (static_cast<float> (bounds.getHeight()) * 0.50f));
    bounds.removeFromTop (10);
    areas.microphoneDeck = bounds;

    return areas;
}

void TaikorAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (backgroundPainting.isValid())
    {
        g.drawImageWithin (backgroundPainting, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::fillDestination, false);
    }
    else
    {
        juce::ColourGradient fallback { backgroundTop, bounds.getCentreX(),
                                        bounds.getY(), backgroundBottom,
                                        bounds.getCentreX(), bounds.getBottom(), false };
        g.setGradientFill (fallback);
        g.fillRect (bounds);
    }

    // A lacquer glaze keeps the artwork atmospheric behind small controls
    // while allowing the pine, ink mist and mountains to remain unmistakable.
    juce::ColourGradient glaze { backgroundTop.withAlpha (0.34f),
                                 bounds.getCentreX(), bounds.getY(),
                                 backgroundBottom.withAlpha (0.58f),
                                 bounds.getCentreX(), bounds.getBottom(), false };
    g.setGradientFill (glaze);
    g.fillRect (bounds);

    // A faint deterministic lacquer grain. Seeded, so it is identical on every
    // repaint and the committed screenshot stays byte-stable between runs.
    juce::Random grain { 0x7a1c0 };
    g.setColour (washiColour.withAlpha (0.012f));
    for (int index = 0; index < 720; ++index)
    {
        const auto x = grain.nextFloat() * bounds.getWidth();
        const auto y = grain.nextFloat() * bounds.getHeight();
        g.fillRect (x, y, 1.0f + grain.nextFloat() * 8.0f, 1.0f);
    }

    const auto areas = calculateLayout();
    const auto panel = [&g] (juce::Rectangle<int> area, int icon)
    {
        if (area.isEmpty())
            return;
        const auto rectangle = area.toFloat();
        juce::ColourGradient wash { panelRaised.withAlpha (0.62f), rectangle.getX(),
                                    rectangle.getY(), panelColour.withAlpha (0.48f),
                                    rectangle.getRight(), rectangle.getBottom(), false };
        g.setGradientFill (wash);
        g.fillRoundedRectangle (rectangle, 6.0f);

        g.setColour (washiColour.withAlpha (0.075f));
        g.fillRect (rectangle.getX() + 10.0f, rectangle.getY(),
                    rectangle.getWidth() - 20.0f, 1.0f);

        const auto iconCentre = juce::Point<float> (rectangle.getX() + 18.0f,
                                                     rectangle.getY() + 11.0f);
        g.setColour ((icon == 1 ? accentColour : brassColour).withAlpha (0.78f));
        if (icon == 0)
        {
            g.drawEllipse (iconCentre.x - 6.0f, iconCentre.y - 4.0f,
                           12.0f, 8.0f, 1.2f);
            g.drawLine (iconCentre.x - 8.0f, iconCentre.y - 4.0f,
                        iconCentre.x - 8.0f, iconCentre.y + 4.0f, 1.2f);
            g.drawLine (iconCentre.x + 8.0f, iconCentre.y - 4.0f,
                        iconCentre.x + 8.0f, iconCentre.y + 4.0f, 1.2f);
        }
        else if (icon == 1)
        {
            g.drawLine (iconCentre.x - 6.0f, iconCentre.y + 5.0f,
                        iconCentre.x + 6.0f, iconCentre.y - 5.0f, 2.0f);
            g.drawLine (iconCentre.x - 6.0f, iconCentre.y - 5.0f,
                        iconCentre.x + 6.0f, iconCentre.y + 5.0f, 2.0f);
        }
        else if (icon == 2)
        {
            g.fillEllipse (iconCentre.x - 7.0f, iconCentre.y - 3.0f,
                           5.0f, 5.0f);
            g.fillEllipse (iconCentre.x + 2.0f, iconCentre.y - 3.0f,
                           5.0f, 5.0f);
            g.drawLine (iconCentre.x - 4.5f, iconCentre.y + 1.0f,
                        iconCentre.x - 1.0f, iconCentre.y + 6.0f, 1.0f);
            g.drawLine (iconCentre.x + 4.5f, iconCentre.y + 1.0f,
                        iconCentre.x + 1.0f, iconCentre.y + 6.0f, 1.0f);
        }
    };

    panel (areas.gridArea, -1);
    panel (areas.rightUpperArea, -1);
    panel (areas.drumDeck, 0);
    panel (areas.strokeDeck, 1);
    panel (areas.microphoneDeck, 2);
}

void TaikorAudioProcessorEditor::resized()
{
    const auto areas = calculateLayout();

    auto header = areas.header;
    auto branding = header.removeFromLeft (
        juce::roundToInt (static_cast<float> (header.getWidth()) * 0.32f));
    auto brandTop = branding.removeFromTop (
        juce::roundToInt (static_cast<float> (branding.getHeight()) * 0.66f));
    logoLabel.setBounds (brandTop.removeFromLeft (juce::jmin (148, brandTop.getWidth())));
    auto seal = brandTop.removeFromLeft (brandTop.getHeight()).reduced (4, 3);
    sealLabel.setBounds (seal);
    editionLabel.setBounds (branding);

    panicButton.setBounds (header.removeFromRight (82).reduced (0, 9));
    header.removeFromRight (12);
    meter.setBounds (header.removeFromRight (
        juce::roundToInt (static_cast<float> (header.getWidth()) * 0.44f))
                         .reduced (0, 6));
    header.removeFromRight (12);
    statusDisplay.setBounds (header.removeFromRight (juce::jmin (190, header.getWidth()))
                                 .reduced (0, 8));

    // Four painted row selectors, each immediately beside the strokes that
    // trigger it. Stroke names live once in the shared header rather than being
    // repeated sixteen times.
    auto gridArea = areas.gridArea.reduced (10, 7);
    gridCaption.setBounds (gridArea.removeFromTop (18));
    gridArea.removeFromTop (2);

    const int gridRows = taikor::drumCount;
    const int gridCols = static_cast<int> (taikor::articulationCount);
    const int gap = 7;
    const int rowHeaderGap = 9;
    const int rowHeaderWidth = juce::jlimit (
        108, 142, juce::roundToInt (static_cast<float> (gridArea.getWidth()) * 0.19f));

    auto strokeHeader = gridArea.removeFromTop (23);
    strokeHeader.removeFromLeft (rowHeaderWidth + rowHeaderGap);
    const auto headerLayout = taikor::ui::rowLayout (
        strokeHeader.getWidth(), gridCols, gap, gridCols);
    for (int col = 0; col < gridCols; ++col)
    {
        strokeLabels[static_cast<std::size_t> (col)].setBounds (
            strokeHeader.getX() + taikor::ui::cellOffset (headerLayout, gap, col),
            strokeHeader.getY(), headerLayout.cellSize, strokeHeader.getHeight());
    }
    gridArea.removeFromTop (2);

    const int cellH = (gridArea.getHeight() - gap * (gridRows - 1)) / gridRows;

    for (int row = 0; row < gridRows; ++row)
    {
        auto rowArea = juce::Rectangle<int> (
            gridArea.getX(), gridArea.getY() + row * (cellH + gap),
            gridArea.getWidth(), cellH);

        auto& drumButton = octaveButtons[static_cast<std::size_t> (row)];
        if (drumButton != nullptr)
            drumButton->setBounds (rowArea.removeFromLeft (rowHeaderWidth));
        else
            rowArea.removeFromLeft (rowHeaderWidth);
        rowArea.removeFromLeft (rowHeaderGap);

        const auto padLayout = taikor::ui::rowLayout (
            rowArea.getWidth(), gridCols, gap, gridCols);
        for (int col = 0; col < gridCols; ++col)
        {
            const std::size_t index = static_cast<std::size_t> (row * gridCols + col);
            if (pads[index] != nullptr)
            {
                const int x = rowArea.getX()
                            + taikor::ui::cellOffset (padLayout, gap, col);
                pads[index]->setBounds (x, rowArea.getY(),
                                        padLayout.cellSize, rowArea.getHeight());
            }
        }
    }

    // The live head now gets the whole companion panel.
    auto rightUpper = areas.rightUpperArea.reduced (10, 7);
    headCaption.setBounds (rightUpper.removeFromTop (16));
    headDisplay.setBounds (rightUpper);

    // Knob decks. Each deck gets a caption row and then an even grid.
    const auto layoutDeck = [] (juce::Rectangle<int> area, juce::Label& label,
                                int rows, std::vector<TaikorKnob*> knobs)
    {
        const auto verticalInset = area.getHeight() < 90 ? 3 : 6;
        auto working = area.reduced (10, verticalInset);
        auto caption = working.removeFromTop (16);
        label.setBounds (caption.withTrimmedLeft (25));
        working.removeFromTop (2);

        const auto count = static_cast<int> (knobs.size());
        if (count <= 0 || rows <= 0 || working.isEmpty())
            return;

        const auto columns = (count + rows - 1) / rows;
        constexpr int rowGap = 3;
        const auto rowHeight = (working.getHeight() - rowGap * (rows - 1)) / rows;

        for (int row = 0; row < rows; ++row)
        {
            const auto first = row * columns;
            if (first >= count)
                break;

            const auto inRow = juce::jmin (columns, count - first);
            auto rowArea = working.removeFromTop (rowHeight);
            if (row + 1 < rows)
                working.removeFromTop (rowGap);
            const auto layout =
                taikor::ui::rowLayout (rowArea.getWidth(), columns, 12, inRow);

            for (int index = 0; index < inRow; ++index)
                if (auto* knob = knobs[static_cast<std::size_t> (first + index)])
                    knob->setBounds (rowArea.getX()
                                         + taikor::ui::cellOffset (layout, 12, index),
                                     rowArea.getY(), layout.cellSize,
                                     rowArea.getHeight());
        }
    };

    layoutDeck (areas.drumDeck, drumDeckLabel, 2,
                { &sizeKnob, &tensionKnob, &headMaterialKnob, &shellMaterialKnob,
                  &cavityKnob, &depthKnob, &resonantKnob, &headDampingKnob,
                  &shellResonanceKnob, &pitchKnob });
    layoutDeck (areas.strokeDeck, strokeDeckLabel, 1,
                { &hardnessKnob, &strikePositionKnob, &strikeAzimuthKnob,
                  &performerKnob, &velocityDepthKnob, &velocityCurveKnob,
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

    std::array<std::uint32_t, taikor::articulationCount> currentTriggerCounters {};
    bool receivedTrigger = false;
    for (std::size_t artIdx = 0; artIdx < taikor::articulationCount; ++artIdx)
    {
        const auto articulation = static_cast<taikor::Articulation> (artIdx);
        currentTriggerCounters[artIdx] = audioProcessor.getTriggerCounter (articulation);
        receivedTrigger = receivedTrigger
                       || currentTriggerCounters[artIdx] != observedTriggerCounters[artIdx];
    }

    // Acquire the published counters before taking the relaxed visual snapshot:
    // the audio thread writes the strike and octave first, then release-publishes
    // its counter. This ordering makes the snapshot belong to that new trigger.
    taikor::DrumVisualState visual;
    audioProcessor.getVisualState (visual);

    // Host MIDI has no opportunity to click a row header first. Follow the
    // newest sounding drum so its painting, pad flash and measurements all
    // describe the same event. UI-triggered strokes already select their row
    // immediately, so this is a no-op for the on-screen playing surface.
    if (receivedTrigger)
        selectOctave (visual.lastOctaveOffset);

    headDisplay.setStrike (visual.strikeRadius, visual.strikeAngle, visual.strikeLevel,
                           visual.lastArticulation);

    const auto engineParameters = audioProcessor.snapshotEngineParameters();
    headDisplay.setMicrophones (engineParameters.micSpread, engineParameters.micDistance);

    const auto measurements = audioProcessor.measureDrum (selectedOctave);
    headDisplay.setMeasurements (measurements.soundingHz,
                                 measurements.breathingModeHz,
                                 measurements.radiusMetres * 200.0f,
                                 measurements.tailSeconds);

    for (std::size_t padIdx = 0; padIdx < totalPadCount; ++padIdx)
    {
        if (pads[padIdx] == nullptr)
            continue;

        const auto articulation = pads[padIdx]->getArticulation();
        const auto artIdx = static_cast<std::size_t> (articulation);
        const auto counter = currentTriggerCounters[artIdx];

        if (counter != observedTriggerCounters[artIdx])
        {
            if (pads[padIdx]->getOctaveOffset() == selectedOctave)
                pads[padIdx]->triggerFlash();
        }
        else
        {
            pads[padIdx]->advanceFlash();
        }
    }

    observedTriggerCounters = currentTriggerCounters;
}

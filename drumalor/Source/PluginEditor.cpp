#include "PluginEditor.h"

#include <DrumalorAssets.h>

#include <cmath>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
// TR-808-inspired colour rhythm without copying its branding or panel layout:
// charcoal hardware, warm legends, and ordered red/orange/yellow/cream voice
// accents. Accent colours identify signal/state; the hardware stays neutral.
constexpr auto trRed = 0xffe15d44;
constexpr auto trOrange = 0xffe47624;
constexpr auto trYellow = 0xffe5b83a;
constexpr auto trCream = 0xffded4b8;
constexpr auto trGreen = 0xff7fb069;
constexpr auto focusRing = 0xfffff0c7;
constexpr auto chassis = 0xff0e100f;
constexpr auto panel = 0xf71b1e1c;
constexpr auto panelRaised = 0xff262a27;
constexpr auto panelEdge = 0xff616761;
constexpr auto recess = 0xff101210;
constexpr auto textBright = 0xffeee8d8;
constexpr auto textDim = 0xffaaa99f;
const juce::Identifier rotaryRoleProperty { "drumalorRotaryRole" };

constexpr int designEditorWidth = 1280;
constexpr int designEditorHeight = 880;
constexpr double editorAspectRatio =
    static_cast<double> (designEditorWidth) / static_cast<double> (designEditorHeight);
constexpr int minimumEditorWidth = 1024;
constexpr int minimumEditorHeight = 704;
constexpr int maximumEditorWidth = 1472;
constexpr int maximumEditorHeight = 1012;

constexpr int uiRefreshHz = 30;
constexpr int padColumns = 7;
constexpr int padGap = 7;

juce::Colour colour (juce::uint32 argb)
{
    return juce::Colour (argb);
}

template <typename Text>
juce::String toJuceString (const Text& text)
{
    if constexpr (std::is_pointer_v<std::decay_t<Text>>)
        return text != nullptr ? juce::String::fromUTF8 (text) : juce::String {};
    else
    {
        const std::string_view view { text };
        return juce::String::fromUTF8 (view.data(), static_cast<int> (view.size()));
    }
}

constexpr bool isValidInstrument (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument) < drumalor::instrumentCount;
}

constexpr std::size_t instrumentIndex (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument);
}

void styleHeaderLabel (juce::Label& label, float size, juce::Colour labelColour)
{
    label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, labelColour);
    label.setInterceptsMouseClicks (false, false);
}

void styleLegendLabel (juce::Label& label, juce::Colour labelColour)
{
    label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, labelColour);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
}

void drawPanel (juce::Graphics& graphics, juce::Rectangle<int> bounds)
{
    const auto area = bounds.toFloat();
    graphics.setColour (juce::Colours::black.withAlpha (0.46f));
    graphics.fillRoundedRectangle (area.translated (0.0f, 4.0f), 7.0f);

    juce::ColourGradient enamel (colour (0xfb262a27), area.getTopLeft(),
                                 colour (panel), area.getBottomRight(), false);
    enamel.addColour (0.48, colour (0xfa202421));
    graphics.setGradientFill (enamel);
    graphics.fillRoundedRectangle (area, 7.0f);

    graphics.setColour (colour (panelEdge).withAlpha (0.70f));
    graphics.drawRoundedRectangle (area.reduced (0.5f), 7.0f, 1.1f);
    graphics.setColour (juce::Colours::black.withAlpha (0.55f));
    graphics.drawRoundedRectangle (area.reduced (2.0f), 5.5f, 1.0f);
}

void drawRecess (juce::Graphics& graphics, juce::Rectangle<float> area, float corner)
{
    graphics.setColour (colour (recess));
    graphics.fillRoundedRectangle (area, corner);
    graphics.setColour (juce::Colours::black.withAlpha (0.62f));
    graphics.drawRoundedRectangle (area.reduced (0.5f), corner, 1.4f);
    graphics.setColour (colour (panelEdge).withAlpha (0.40f));
    graphics.drawRoundedRectangle (area.expanded (1.0f), corner + 1.0f, 1.0f);
}

void drawHardwareScrew (juce::Graphics& graphics, juce::Point<float> centre)
{
    constexpr float radius = 4.5f;
    const auto screw = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                           .withCentre (centre);
    graphics.setColour (juce::Colours::black.withAlpha (0.42f));
    graphics.fillEllipse (screw.translated (0.5f, 1.0f));

    juce::ColourGradient metal (colour (0xffb7bbb4), screw.getTopLeft(),
                                colour (0xff555a56), screw.getBottomRight(), false);
    graphics.setGradientFill (metal);
    graphics.fillEllipse (screw);
    graphics.setColour (colour (0xff242724));
    graphics.drawLine (centre.x - 2.4f, centre.y, centre.x + 2.4f, centre.y, 1.0f);
}

void drawScaledBackground (juce::Graphics& graphics, const juce::Image& image,
                           juce::Rectangle<float> destination, float opacity)
{
    if (! image.isValid())
        return;

    auto source = image.getBounds().toFloat();
    const auto sourceAspect = source.getWidth() / source.getHeight();
    const auto destinationAspect = destination.getWidth() / destination.getHeight();
    if (sourceAspect > destinationAspect)
        source = source.withSizeKeepingCentre (source.getHeight() * destinationAspect,
                                               source.getHeight());
    else
        source = source.withSizeKeepingCentre (source.getWidth(),
                                               source.getWidth() / destinationAspect);

    const juce::Graphics::ScopedSaveState saveState (graphics);
    graphics.setOpacity (juce::jlimit (0.0f, 1.0f, opacity));
    graphics.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    graphics.drawImage (image,
                        juce::roundToInt (destination.getX()),
                        juce::roundToInt (destination.getY()),
                        juce::roundToInt (destination.getWidth()),
                        juce::roundToInt (destination.getHeight()),
                        juce::roundToInt (source.getX()),
                        juce::roundToInt (source.getY()),
                        juce::roundToInt (source.getWidth()),
                        juce::roundToInt (source.getHeight()), false);
}

juce::Colour padAccentFor (drumalor::Instrument instrument)
{
    switch (instrument)
    {
        case drumalor::Instrument::Kick:
        case drumalor::Instrument::Snare:
        case drumalor::Instrument::Clap:
            return colour (trRed);
        case drumalor::Instrument::ClosedHat:
        case drumalor::Instrument::OpenHat:
        case drumalor::Instrument::Ride:
        case drumalor::Instrument::Crash:
            return colour (trOrange);
        case drumalor::Instrument::LowTom:
        case drumalor::Instrument::MidTom:
        case drumalor::Instrument::HighTom:
            return colour (trYellow);
        case drumalor::Instrument::Shaker:
        case drumalor::Instrument::Perc1:
        case drumalor::Instrument::Perc2:
            return colour (trCream);
        case drumalor::Instrument::Count:
            break;
    }
    return colour (panelEdge);
}

// Green below -12 dB, amber towards -3 dB, red at the top of the scale. The
// crossover points come from the shared decibel curve, not from pixel counts.
juce::Colour meterColourFor (float position)
{
    const auto amber = drumalor::ui::smoothStep (0.55f, 0.80f, position);
    const auto red = drumalor::ui::smoothStep (0.86f, 0.98f, position);
    return colour (trGreen)
        .interpolatedWith (colour (trYellow), amber)
        .interpolatedWith (colour (trRed), red);
}

class DrumalorPadAccessibilityHandler final : public juce::AccessibilityHandler
{
public:
    explicit DrumalorPadAccessibilityHandler (DrumalorPad& padToWrap)
        : AccessibilityHandler (padToWrap, juce::AccessibilityRole::radioButton,
                                makeActions (padToWrap)),
          pad (padToWrap)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = AccessibilityHandler::getCurrentState().withCheckable();
        return pad.getToggleState() ? state.withChecked() : state;
    }

private:
    static juce::AccessibilityActions makeActions (DrumalorPad& target)
    {
        const auto selectAndTrigger = [&target]
        {
            if (target.getToggleState())
                target.triggerClick();
            else
                target.setToggleState (true, juce::sendNotification);
        };

        return juce::AccessibilityActions()
            .addAction (juce::AccessibilityActionType::press, selectAndTrigger)
            .addAction (juce::AccessibilityActionType::toggle, selectAndTrigger);
    }

    DrumalorPad& pad;
};
} // namespace

DrumalorLookAndFeel::DrumalorLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colour (textBright));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colour (trOrange));
    setColour (juce::Slider::rotarySliderOutlineColourId, colour (panelEdge));
    setColour (juce::TextButton::textColourOffId, colour (textDim));
    setColour (juce::TextButton::textColourOnId, colour (textBright));
    setColour (juce::ComboBox::backgroundColourId, colour (recess));
    setColour (juce::ComboBox::textColourId, colour (textBright));
    setColour (juce::ComboBox::outlineColourId, colour (panelEdge).withAlpha (0.72f));
    setColour (juce::ComboBox::arrowColourId, colour (trOrange));
    setColour (juce::ComboBox::buttonColourId, colour (panelRaised));
    setColour (juce::ComboBox::focusedOutlineColourId, colour (focusRing));
    setColour (juce::PopupMenu::backgroundColourId, colour (0xff181b19));
    setColour (juce::PopupMenu::textColourId, colour (textBright));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour (0xff3a3f3b));
    setColour (juce::PopupMenu::highlightedTextColourId, colour (textBright));
}

void DrumalorLookAndFeel::drawRotarySlider (juce::Graphics& graphics,
                                            int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto role = static_cast<DrumalorKnob::VisualRole> (
        static_cast<int> (slider.getProperties().getWithDefault (
            rotaryRoleProperty, static_cast<int> (DrumalorKnob::VisualRole::Voice))));
    const bool isMaster = role == DrumalorKnob::VisualRole::Master;
    const bool isBipolar = role == DrumalorKnob::VisualRole::BipolarVoice;
    const auto available = static_cast<float> (juce::jmin (width, height)) - 18.0f;
    const auto diameter = juce::jlimit (60.0f, isMaster ? 106.0f : 132.0f,
                                        available);
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Crisp silkscreen calibration around a metal collar and Bakelite cap.
    constexpr int tickCount = 11;
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const auto proportion = static_cast<float> (tick) /
                                static_cast<float> (tickCount - 1);
        const auto tickAngle = rotaryStartAngle
                             + proportion * (rotaryEndAngle - rotaryStartAngle);
        const auto outer = centre.getPointOnCircumference (radius + 7.5f, tickAngle);
        const bool centreDetent = isBipolar && tick == tickCount / 2;
        const bool majorTick = tick % 5 == 0;
        const auto inner = centre.getPointOnCircumference (
            radius + (centreDetent ? 0.5f : majorTick ? 2.0f : 3.5f), tickAngle);
        graphics.setColour (centreDetent ? colour (textBright)
                                         : colour (textDim).withAlpha (majorTick ? 0.90f
                                                                                : 0.56f));
        graphics.drawLine ({ inner, outer }, centreDetent ? 2.0f : majorTick ? 1.5f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 0.5f, radius + 0.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    graphics.strokePath (track, juce::PathStrokeType (2.0f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

    const auto valueOrigin = isBipolar
        ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;
    const auto valueStart = juce::jmin (valueOrigin, angle);
    const auto valueEnd = juce::jmax (valueOrigin, angle);
    if (valueEnd - valueStart > 0.0001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius + 0.5f, radius + 0.5f, 0.0f,
                                valueStart, valueEnd, true);
        graphics.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        graphics.strokePath (valueArc, juce::PathStrokeType (3.0f,
                                                             juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
    }

    graphics.setColour (juce::Colours::black.withAlpha (0.52f));
    graphics.fillEllipse (bounds.translated (1.5f, 2.5f));
    juce::ColourGradient collar (colour (0xff858a83), bounds.getTopLeft(),
                                 colour (0xff303431), bounds.getBottomRight(), false);
    collar.addColour (0.44, colour (0xff5d625d));
    graphics.setGradientFill (collar);
    graphics.fillEllipse (bounds);
    graphics.setColour (colour (0xffa3a79f).withAlpha (0.72f));
    graphics.drawEllipse (bounds.reduced (0.6f), 1.1f);

    const auto cap = bounds.reduced (juce::jmax (5.0f, diameter * 0.075f));
    graphics.setColour (juce::Colours::black.withAlpha (0.45f));
    graphics.fillEllipse (cap.translated (0.8f, 1.4f));
    const auto capTop = isMaster ? colour (0xff342724) : colour (0xff303330);
    juce::ColourGradient bakelite (capTop, cap.getTopLeft(),
                                   colour (0xff0d0f0e), cap.getBottomRight(), false);
    bakelite.addColour (0.42, isMaster ? colour (0xff281b19) : colour (0xff202320));
    graphics.setGradientFill (bakelite);
    graphics.fillEllipse (cap);
    graphics.setColour (colour (panelEdge).withAlpha (0.78f));
    graphics.drawEllipse (cap, 1.0f);

    const auto pointerStart = centre.getPointOnCircumference (radius * 0.18f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (radius * 0.76f, angle);
    graphics.setColour (isMaster ? colour (trYellow) : colour (0xfff4edda));
    graphics.drawLine ({ pointerStart, pointerEnd }, isMaster ? 2.4f : 2.8f);
    graphics.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));

    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (colour (focusRing));
        graphics.drawEllipse (bounds.expanded (3.0f), 2.0f);
    }
}

void DrumalorLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                                juce::Button& button,
                                                const juce::Colour&,
                                                bool isHighlighted,
                                                bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = colour (0xff7f3027);
    if (isHighlighted)
        fill = fill.brighter (0.08f);
    if (isDown)
        fill = fill.darker (0.12f);

    graphics.setColour (fill);
    graphics.fillRoundedRectangle (bounds, 4.0f);
    graphics.setColour (colour (trRed).withAlpha (isHighlighted ? 0.96f : 0.72f));
    graphics.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    if (button.hasKeyboardFocus (true))
    {
        graphics.setColour (colour (focusRing));
        graphics.drawRoundedRectangle (bounds.reduced (2.5f), 2.5f, 1.5f);
    }
}

juce::Font DrumalorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (
                                          13.0f, static_cast<float> (buttonHeight) * 0.38f),
                                          juce::Font::bold));
}

DrumalorPad::DrumalorPad (drumalor::Instrument instrument,
                          juce::String displayName,
                          int midiNote)
    : Button (displayName), drum (instrument), nameText (std::move (displayName)),
      noteText ("GM " + juce::String (midiNote))
{
    setName (nameText + " pad");
    setTitle (nameText + " drum pad");
    setDescription ("Select and trigger " + nameText
                    + " on MIDI note " + juce::String (midiNote));
    setTooltip ("Select and audition " + nameText
                + " (MIDI note " + juce::String (midiNote) + ")");
    setWantsKeyboardFocus (true);
    setClickingTogglesState (true);
    setRadioGroupId (1, juce::dontSendNotification);
}

void DrumalorPad::setSelected (bool shouldBeSelected)
{
    if (selected == shouldBeSelected)
        return;
    selected = shouldBeSelected;
    setToggleState (selected, juce::dontSendNotification);
    repaint();
}

void DrumalorPad::triggerFlash()
{
    flashLevel = 1.0f;
    repaint();
}

void DrumalorPad::advanceFlash()
{
    if (flashLevel <= 0.001f)
        return;

    flashLevel *= 0.76f;
    if (flashLevel < 0.015f)
        flashLevel = 0.0f;
    repaint();
}

void DrumalorPad::setLevel (float linearLevel)
{
    static const float release = drumalor::ui::onePoleCoefficient (
        0.22f, static_cast<float> (uiRefreshHz));
    static const float peakFall = drumalor::ui::decayMultiplier (
        -18.0f, 1.0f, static_cast<float> (uiRefreshHz));

    ballistics.update (
        drumalor::ui::meterPositionForLinear (linearLevel, -42.0f),
        1.0f, release, peakFall, 4.0f);
    // The peak-hold marker keeps falling after the fill has settled, so a
    // repaint is due when either has visibly moved. Watching the fill alone
    // froze the marker mid-fall on any pad left idle.
    if (std::abs (ballistics.level - levelPosition) < 0.006f
        && std::abs (ballistics.peak - lastPaintedPeak) < 0.006f)
        return;
    levelPosition = ballistics.level;
    lastPaintedPeak = ballistics.peak;
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler>
DrumalorPad::createAccessibilityHandler()
{
    return std::make_unique<DrumalorPadAccessibilityHandler> (*this);
}

void DrumalorPad::paintButton (juce::Graphics& graphics,
                               bool isMouseOverButton,
                               bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.5f);
    const auto channelColour = padAccentFor (drum);
    auto top = selected
        ? channelColour.interpolatedWith (colour (panelRaised), 0.76f)
        : colour (0xff242724);
    auto bottom = selected
        ? channelColour.interpolatedWith (colour (0xff151816), 0.82f)
        : colour (0xff121513);

    if (isMouseOverButton)
    {
        top = top.brighter (0.10f);
        bottom = bottom.brighter (0.07f);
    }
    if (isButtonDown)
    {
        top = top.darker (0.16f);
        bottom = bottom.darker (0.16f);
        bounds = bounds.reduced (1.2f).translated (0.0f, 1.0f);
    }

    graphics.setColour (juce::Colours::black.withAlpha (0.58f));
    graphics.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 5.0f);

    const auto frame = bounds;
    juce::ColourGradient metal (colour (0xff666b65), frame.getTopLeft(),
                                colour (0xff292d2a), frame.getBottomRight(), false);
    graphics.setGradientFill (metal);
    graphics.fillRoundedRectangle (frame, 5.0f);

    auto face = frame.reduced (3.0f);
    juce::ColourGradient fill (top, bounds.getTopLeft(), bottom,
                               bounds.getBottomLeft(), false);
    graphics.setGradientFill (fill);
    graphics.fillRoundedRectangle (face, 3.5f);

    // Convex rubber face: a restrained top reflection and deep lower edge.
    graphics.setColour (colour (0x36eee8d8));
    graphics.drawLine (face.getX() + 5.0f, face.getY() + 1.0f,
                       face.getRight() - 5.0f, face.getY() + 1.0f, 1.0f);
    graphics.setColour (juce::Colours::black.withAlpha (0.62f));
    graphics.drawLine (face.getX() + 5.0f, face.getBottom() - 1.0f,
                       face.getRight() - 5.0f, face.getBottom() - 1.0f, 1.2f);

    if (flashLevel > 0.0f)
    {
        graphics.setColour (channelColour.withAlpha (0.05f + 0.24f * flashLevel));
        graphics.fillRoundedRectangle (face, 3.5f);
    }

    graphics.setColour (selected ? channelColour : colour (0xff555b56));
    graphics.drawRoundedRectangle (frame.reduced (0.5f), 5.0f,
                                   selected ? 1.8f : 0.9f);
    if (hasKeyboardFocus (true))
    {
        graphics.setColour (colour (focusRing));
        graphics.drawRoundedRectangle (frame.expanded (0.5f), 5.5f, 2.0f);
    }

    const auto led = juce::Rectangle<float> (6.0f, 6.0f)
                         .withCentre ({ face.getRight() - 9.0f, face.getY() + 9.0f });
    graphics.setColour (juce::Colours::black.withAlpha (0.72f));
    graphics.fillEllipse (led.expanded (1.5f));
    graphics.setColour (channelColour.withAlpha (
        selected ? 0.96f : 0.34f + 0.60f * flashLevel));
    graphics.fillEllipse (led);
    if (selected || flashLevel > 0.25f)
    {
        graphics.setColour (channelColour.withAlpha (0.18f + 0.18f * flashLevel));
        graphics.fillEllipse (led.expanded (4.0f));
    }

    // Channel activity rail: a recessed track that fills with the voice's own
    // measured level, so the grid doubles as a thirteen-channel meter bridge
    // instead of only flashing when a note arrives.
    const auto railTrack = face.withHeight (5.0f).withY (face.getBottom() - 5.5f)
                               .reduced (5.0f, 0.0f);
    graphics.setColour (juce::Colours::black.withAlpha (0.66f));
    graphics.fillRoundedRectangle (railTrack, 2.0f);
    graphics.setColour (channelColour.withAlpha (selected ? 0.36f : 0.20f));
    graphics.fillRoundedRectangle (railTrack, 2.0f);
    if (levelPosition > 0.002f)
    {
        auto filled = railTrack;
        filled.setWidth (juce::jmax (2.0f, railTrack.getWidth() * levelPosition));
        graphics.setColour (meterColourFor (levelPosition));
        graphics.fillRoundedRectangle (filled, 2.0f);
    }
    if (ballistics.peak > 0.02f)
    {
        const auto markerX = railTrack.getX()
            + railTrack.getWidth() * juce::jlimit (0.0f, 1.0f, ballistics.peak);
        graphics.setColour (colour (textBright).withAlpha (0.80f));
        graphics.fillRect (juce::Rectangle<float> (
            juce::jmin (markerX, railTrack.getRight() - 1.5f), railTrack.getY(),
            1.5f, railTrack.getHeight()));
    }

    auto textArea = face.toNearestInt().reduced (9, 7);
    textArea.removeFromBottom (6);
    auto utilityRow = textArea.removeFromTop (15);
    auto noteArea = textArea.removeFromBottom (17);
    graphics.setColour (colour (textDim).withAlpha (0.80f));
    graphics.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    graphics.drawText (
        juce::String (static_cast<int> (drum) + 1).paddedLeft ('0', 2),
        utilityRow, juce::Justification::centredLeft);

    graphics.setColour (colour (textBright));
    graphics.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    graphics.drawFittedText (nameText.toUpperCase(), textArea,
                             juce::Justification::centred, 2, 0.78f);

    graphics.setColour (selected ? colour (textBright) : colour (textDim));
    graphics.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    graphics.drawText (noteText, noteArea, juce::Justification::centred);
}

DrumalorKnob::DrumalorKnob (juce::String name, ValueStyle style, VisualRole role)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, colour (textBright));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 22);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, colour (recess));
    slider.setColour (juce::Slider::textBoxOutlineColourId,
                      colour (panelEdge).withAlpha (0.72f));
    slider.setColour (juce::Slider::textBoxTextColourId, colour (textBright));
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set (rotaryRoleProperty, static_cast<int> (role));

    if (style == ValueStyle::Percent)
    {
        slider.setNumDecimalPlacesToDisplay (0);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (juce::roundToInt (value * 100.0)) + "%";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue() / 100.0;
        };
        slider.setDoubleClickReturnValue (true, 0.5);
    }
    else if (style == ValueStyle::Semitones)
    {
        slider.setNumDecimalPlacesToDisplay (1);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (value, 1) + " st";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue();
        };
        slider.setDoubleClickReturnValue (true, 0.0);
    }
    else
    {
        slider.setNumDecimalPlacesToDisplay (1);
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (value, 1) + " dB";
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-").getDoubleValue();
        };
        slider.setDoubleClickReturnValue (true, -6.0);
    }

    addAndMakeVisible (slider);
    setLabelText (name, "Adjust " + name.toLowerCase());
}

void DrumalorKnob::setLabelText (const juce::String& text,
                                 const juce::String& description)
{
    label.setText (text, juce::dontSendNotification);
    slider.setName (text);
    slider.setTitle (text);
    const auto helpText = description + ". Double-click to reset.";
    slider.setDescription (helpText);
    slider.setTooltip (helpText);
    setTooltip (helpText);
}

void DrumalorKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (20));
    slider.setBounds (area);
}

DrumalorStatusDisplay::DrumalorStatusDisplay()
{
    setTitle ("Drumalor engine offline");
    setDescription ("Current Drumalor audio engine status");
    setAccessible (true);
}

std::unique_ptr<juce::AccessibilityHandler>
DrumalorStatusDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText);
}

void DrumalorStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;

    const bool shouldAnnounceStatusChange = isReady != ready
        || (ready && std::abs (rate - sampleRate) >= 1.0);
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    const auto statusText = ready
        ? juce::String (juce::jmax (0, voices)) + " active drum tails at "
            + juce::String (juce::roundToInt (rate / 1000.0)) + " kilohertz"
        : juce::String { "No active drum tails; audio engine is offline" };
    setTitle (statusText);
    if (shouldAnnounceStatusChange)
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
    repaint();
}

void DrumalorStatusDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    graphics.setColour (colour (0xff121513));
    graphics.fillRoundedRectangle (bounds, 3.0f);
    graphics.setColour (colour (0xff555b56));
    graphics.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    const auto light = juce::Rectangle<float> (bounds.getX() + 11.0f,
                                               bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    graphics.setColour (isReady ? colour (trYellow) : colour (0xff5a5f5b));
    graphics.fillEllipse (light);
    if (isReady)
    {
        graphics.setColour (colour (trYellow).withAlpha (0.27f));
        graphics.fillEllipse (light.expanded (5.0f));
    }

    graphics.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    graphics.setColour (colour (textBright));
    graphics.drawText (juce::String (juce::jmax (0, voices)).paddedLeft ('0', 2) + " TAILS",
                       28, 0, 78, getHeight(), juce::Justification::centredLeft);

    graphics.setColour (colour (textDim));
    const auto rateText = rate > 0.0
        ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz"
        : juce::String { "OFFLINE" };
    graphics.drawText (rateText, 105, 0, getWidth() - 114, getHeight(),
                       juce::Justification::centredRight);
}

DrumalorBusMeter::DrumalorBusMeter()
{
    setTitle ("Kit bus meter");
    setDescription ("Stereo output level and bus compressor gain reduction");
    setAccessible (true);
    setInterceptsMouseClicks (false, false);
    leftBallistics.reset();
    rightBallistics.reset();
}

std::unique_ptr<juce::AccessibilityHandler>
DrumalorBusMeter::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText);
}

void DrumalorBusMeter::setLevels (float leftLinear, float rightLinear, float busGain)
{
    static const float release = drumalor::ui::onePoleCoefficient (
        0.30f, static_cast<float> (uiRefreshHz));
    static const float peakFall = drumalor::ui::decayMultiplier (
        -12.0f, 1.0f, static_cast<float> (uiRefreshHz));
    constexpr float holdUpdates = static_cast<float> (uiRefreshHz);

    leftBallistics.update (
        drumalor::ui::meterPositionForLinear (leftLinear, floorDecibels),
        1.0f, release, peakFall, holdUpdates);
    rightBallistics.update (
        drumalor::ui::meterPositionForLinear (rightLinear, floorDecibels),
        1.0f, release, peakFall, holdUpdates);

    // Show 0 to -18 dB of reduction across the strip below the level bars.
    const auto reductionDecibels = -juce::Decibels::gainToDecibels (
        juce::jlimit (0.02f, 1.0f, busGain), -48.0f);
    const auto target = juce::jlimit (0.0f, 1.0f, reductionDecibels / 18.0f);
    reductionPosition = drumalor::ui::mix (
        reductionPosition, target, target > reductionPosition ? 1.0f : 0.30f);

    const int clipState = juce::jmax (leftBallistics.peak, rightBallistics.peak) >= 0.995f
        ? 1 : 0;
    if (clipState != announcedClipState)
    {
        announcedClipState = clipState;
        setTitle (clipState == 1 ? juce::String ("Kit bus meter at full scale")
                                 : juce::String ("Kit bus meter"));
    }

    // A resting kit must not force a repaint thirty times a second. Every
    // term compares against the last painted value rather than the previous
    // frame, so a bar releasing in steps too small to clear the threshold
    // individually still repaints once they add up - comparing the reduction
    // frame to frame used to leave its minimum-width sliver lit indefinitely.
    const bool changed = std::abs (leftBallistics.level - lastPaintedLeft) > 0.004f
                      || std::abs (rightBallistics.level - lastPaintedRight) > 0.004f
                      || std::abs (leftBallistics.peak - lastPaintedLeftPeak) > 0.004f
                      || std::abs (rightBallistics.peak - lastPaintedRightPeak) > 0.004f
                      || std::abs (reductionPosition - lastPaintedReduction) > 0.004f;
    if (! changed)
        return;
    lastPaintedLeft = leftBallistics.level;
    lastPaintedRight = rightBallistics.level;
    lastPaintedLeftPeak = leftBallistics.peak;
    lastPaintedRightPeak = rightBallistics.peak;
    lastPaintedReduction = reductionPosition;
    repaint();
}

void DrumalorBusMeter::paint (juce::Graphics& graphics)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    drawRecess (graphics, area, 3.0f);

    auto content = area.reduced (5.0f, 4.0f);
    const auto reductionHeight = juce::jlimit (3.0f, 6.0f, content.getHeight() * 0.22f);
    auto reductionRow = content.removeFromBottom (reductionHeight);
    content.removeFromBottom (2.0f);

    const auto barGap = 2.0f;
    const auto barHeight = juce::jmax (2.0f, (content.getHeight() - barGap) * 0.5f);
    const drumalor::ui::MeterBallistics* channels[2] { &leftBallistics, &rightBallistics };
    for (int channel = 0; channel < 2; ++channel)
    {
        auto row = content.removeFromTop (barHeight);
        if (channel == 0)
            content.removeFromTop (barGap);

        graphics.setColour (juce::Colours::black.withAlpha (0.55f));
        graphics.fillRoundedRectangle (row, 1.5f);

        const auto level = juce::jlimit (0.0f, 1.0f, channels[channel]->level);
        if (level > 0.002f)
        {
            auto filled = row;
            filled.setWidth (juce::jmax (2.0f, row.getWidth() * level));
            juce::ColourGradient ramp (meterColourFor (0.10f), row.getX(), 0.0f,
                                       meterColourFor (1.0f), row.getRight(), 0.0f,
                                       false);
            ramp.addColour (0.72, meterColourFor (0.72f));
            graphics.setGradientFill (ramp);
            graphics.fillRoundedRectangle (filled, 1.5f);
        }

        const auto peak = juce::jlimit (0.0f, 1.0f, channels[channel]->peak);
        if (peak > 0.02f)
        {
            const auto markerX = juce::jmin (row.getX() + row.getWidth() * peak,
                                             row.getRight() - 2.0f);
            graphics.setColour (peak >= 0.995f ? colour (trRed) : colour (textBright));
            graphics.fillRect (juce::Rectangle<float> (
                markerX, row.getY(), 2.0f, row.getHeight()));
        }
    }

    // Gain reduction grows leftwards from the right edge, the usual direction
    // for a compressor strip.
    graphics.setColour (juce::Colours::black.withAlpha (0.55f));
    graphics.fillRoundedRectangle (reductionRow, 1.5f);
    if (reductionPosition > 0.004f)
    {
        auto filled = reductionRow;
        const auto width = juce::jmax (2.0f, reductionRow.getWidth() * reductionPosition);
        filled.setX (reductionRow.getRight() - width);
        filled.setWidth (width);
        graphics.setColour (colour (trOrange).withAlpha (0.92f));
        graphics.fillRoundedRectangle (filled, 1.5f);
    }

    // -36/-24/-12/-6 dB silkscreen marks on the shared decibel curve.
    graphics.setColour (colour (panelEdge).withAlpha (0.45f));
    for (const float decibels : { -36.0f, -24.0f, -12.0f, -6.0f })
    {
        const auto position = 1.0f - decibels / floorDecibels;
        const auto x = area.getX() + 5.0f + (area.getWidth() - 10.0f) * position;
        graphics.drawLine (x, area.getY() + 2.0f, x, area.getBottom() - 2.0f, 0.8f);
    }
}

DrumalorAudioProcessorEditor::DrumalorAudioProcessorEditor (DrumalorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      tooltipWindow (this, 550),
      vintagePanel (juce::ImageFileFormat::loadFrom (
          DrumalorAssets::vintagepanel_jpg,
          static_cast<std::size_t> (DrumalorAssets::vintagepanel_jpgSize)))
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("DRUMALOR", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 29.0f, colour (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Drumalor drum synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("13-VOICE  |  ANALOG DRUM SYNTHESIZER",
                          juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 11.0f, colour (trOrange));
    addAndMakeVisible (editionLabel);

    addAndMakeVisible (busMeter);
    addAndMakeVisible (statusDisplay);

    panicButton.setColour (juce::TextButton::textColourOffId, colour (trCream));
    panicButton.setName ("Panic - stop all drum tails");
    panicButton.setTitle ("Panic");
    panicButton.setDescription ("Immediately stop every sounding drum voice");
    panicButton.setTooltip ("Immediately stop every sounding drum voice");
    panicButton.onClick = [this]
    {
        audioProcessor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        pads[index] = std::make_unique<DrumalorPad> (
            instrument,
            toJuceString (drumalor::getInstrumentDisplayName (instrument)),
            drumalor::getStandardMidiNote (instrument));
        pads[index]->onClick = [this, instrument]
        {
            // JUCE notifies the old radio button when a new one turns it off.
            // Only the button that is now selected should audition a voice.
            if (! pads[instrumentIndex (instrument)]->getToggleState())
                return;
            selectInstrument (instrument);
            pads[instrumentIndex (instrument)]->triggerFlash();
            audioProcessor.triggerFromUi (instrument);
        };
        observedTriggerCounters[index] = audioProcessor.getTriggerCounter (instrument);
        addAndMakeVisible (*pads[index]);
    }

    selectedInstrumentLabel.setFont (
        juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    selectedInstrumentLabel.setColour (juce::Label::textColourId, colour (textBright));
    selectedInstrumentLabel.setJustificationType (juce::Justification::centredLeft);
    selectedInstrumentLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (selectedInstrumentLabel);

    for (auto* knob : { &characterAKnob, &characterBKnob, &pitchKnob, &decayKnob,
                        &levelKnob, &humaniseKnob, &bleedKnob, &busDriveKnob,
                        &busCompressionKnob, &outputKnob })
        addAndMakeVisible (*knob);

    panLabel.setText ("PAN", juce::dontSendNotification);
    styleLegendLabel (panLabel, colour (textBright));
    addAndMakeVisible (panLabel);

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    panSlider.setColour (juce::Slider::backgroundColourId, colour (recess));
    panSlider.setColour (juce::Slider::trackColourId, colour (trOrange));
    panSlider.setColour (juce::Slider::thumbColourId, colour (0xfff4edda));
    panSlider.setColour (juce::Slider::textBoxBackgroundColourId, colour (recess));
    panSlider.setColour (juce::Slider::textBoxOutlineColourId,
                         colour (panelEdge).withAlpha (0.72f));
    panSlider.setColour (juce::Slider::textBoxTextColourId, colour (textBright));
    panSlider.setName ("PAN");
    panSlider.setTitle ("Pan");
    addAndMakeVisible (panSlider);

    chokeLabel.setText ("CHOKE GROUP", juce::dontSendNotification);
    styleLegendLabel (chokeLabel, colour (textBright));
    addAndMakeVisible (chokeLabel);

    // Item IDs are 1-based and must match the choice parameter's order before
    // the attachment is constructed.
    chokeBox.addItem ("Off", 1);
    chokeBox.addItem ("Group A", 2);
    chokeBox.addItem ("Group B", 3);
    chokeBox.addItem ("Group C", 4);
    chokeBox.setJustificationType (juce::Justification::centredLeft);
    chokeBox.setName ("CHOKE GROUP");
    chokeBox.setTitle ("Choke group");
    addAndMakeVisible (chokeBox);

    humaniseKnob.slider.setDoubleClickReturnValue (true, 0.5);
    bleedKnob.slider.setDoubleClickReturnValue (true, 0.0);
    busDriveKnob.slider.setDoubleClickReturnValue (true, 0.0);
    busCompressionKnob.slider.setDoubleClickReturnValue (true, 0.0);

    humaniseKnob.setLabelText (
        "HUMANISE", "Scale the modelled per-hit analogue variation of the whole kit");
    bleedKnob.setLabelText (
        "KIT BLEED", "How much of the kit the snare wires and the tom heads hear");
    busDriveKnob.setLabelText (
        "BUS DRIVE", "Add shared output-bus saturation to the whole kit");
    busCompressionKnob.setLabelText (
        "BUS COMP", "Glue the kit with the shared output-bus compressor");
    outputKnob.setLabelText (
        "MASTER OUTPUT", "Trim the kit's final output level after the mix bus");

    humaniseAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::humanise,
        humaniseKnob.slider);
    bleedAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::bleed, bleedKnob.slider);
    busDriveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::busDrive,
        busDriveKnob.slider);
    busCompressionAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::busCompression,
        busCompressionKnob.slider);
    outputAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, drumalor::parameters::output, outputKnob.slider);

    humaniseKnob.slider.setTextValueSuffix ("%");
    bleedKnob.slider.setTextValueSuffix ("%");
    busDriveKnob.slider.setTextValueSuffix ("%");
    busCompressionKnob.slider.setTextValueSuffix ("%");
    outputKnob.slider.setTextValueSuffix (" dB");
    for (auto* knob : { &humaniseKnob, &bleedKnob, &busDriveKnob, &busCompressionKnob,
                        &outputKnob })
    {
        knob->slider.setColour (juce::Slider::rotarySliderFillColourId, colour (trRed));
        knob->slider.setColour (juce::Slider::rotarySliderOutlineColourId,
                                colour (0xff555b56));
    }

    selectInstrument (selectedInstrument);
    statusDisplay.setStatus (audioProcessor.getActiveVoiceCount(),
                             audioProcessor.isEngineReady(),
                             audioProcessor.getCurrentSampleRateForDisplay());

    setResizable (true, true);
    setResizeLimits (minimumEditorWidth, minimumEditorHeight,
                     maximumEditorWidth, maximumEditorHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (editorAspectRatio);
    setSize (designEditorWidth, designEditorHeight);
    startTimerHz (uiRefreshHz);
}

DrumalorAudioProcessorEditor::~DrumalorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DrumalorAudioProcessorEditor::selectInstrument (drumalor::Instrument instrument)
{
    if (! isValidInstrument (instrument))
        return;

    const auto needsAttachmentRefresh = instrument != selectedInstrument
                                     || characterAAttachment == nullptr;
    selectedInstrument = instrument;

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        pads[index]->setSelected (index == instrumentIndex (selectedInstrument));

    const auto name = toJuceString (
        drumalor::getInstrumentDisplayName (selectedInstrument));
    const auto characterA = toJuceString (
        drumalor::getCharacterALabel (selectedInstrument));
    const auto characterB = toJuceString (
        drumalor::getCharacterBLabel (selectedInstrument));
    const auto midiNote = drumalor::getStandardMidiNote (selectedInstrument);
    const auto& defaults = drumalor::getInstrumentMetadata (
        selectedInstrument).defaultParameters;

    selectedInstrumentLabel.setText (
        name.toUpperCase() + "   |   MIDI " + juce::String (midiNote),
        juce::dontSendNotification);
    characterAKnob.setLabelText (characterA.toUpperCase(),
                                 "Adjust " + name + " " + characterA.toLowerCase());
    characterBKnob.setLabelText (characterB.toUpperCase(),
                                 "Adjust " + name + " " + characterB.toLowerCase());
    pitchKnob.setLabelText ("PITCH", "Transpose " + name + " by semitones");
    decayKnob.setLabelText ("DECAY", "Adjust " + name + " decay time");
    levelKnob.setLabelText ("LEVEL", "Set the " + name + " channel level");

    const auto panHelp = "Place " + name + " in the stereo field";
    panSlider.setDescription (panHelp);
    panSlider.setTooltip (panHelp);
    const auto chokeHelp = "Choose the mute group that " + name
        + " cuts and is cut by";
    chokeBox.setDescription (chokeHelp);
    chokeBox.setTooltip (chokeHelp);

    const auto voiceAccent = padAccentFor (selectedInstrument);
    for (auto* knob : { &characterAKnob, &characterBKnob, &pitchKnob, &decayKnob,
                        &levelKnob })
        knob->slider.setColour (juce::Slider::rotarySliderFillColourId, voiceAccent);
    panSlider.setColour (juce::Slider::trackColourId, voiceAccent);

    if (needsAttachmentRefresh)
        rebuildSelectedAttachments();

    // Keep the editor's reset gesture tied to each instrument's authored defaults,
    // independent of attachment implementation details.
    characterAKnob.slider.setDoubleClickReturnValue (true, defaults.characterA);
    characterBKnob.slider.setDoubleClickReturnValue (true, defaults.characterB);
    pitchKnob.slider.setDoubleClickReturnValue (true, defaults.pitch);
    decayKnob.slider.setDoubleClickReturnValue (true, defaults.decay);
    levelKnob.slider.setDoubleClickReturnValue (true, defaults.level);
    panSlider.setDoubleClickReturnValue (true, defaults.pan);
}

void DrumalorAudioProcessorEditor::rebuildSelectedAttachments()
{
    // Detach every control before any new attachment pushes an initial value.
    // Otherwise an attachment for the previously selected drum can observe a
    // slider update and write the new drum's value back into the old parameter.
    characterAAttachment.reset();
    characterBAttachment.reset();
    pitchAttachment.reset();
    decayAttachment.reset();
    levelAttachment.reset();
    panAttachment.reset();
    chokeAttachment.reset();

    characterAAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::characterA),
        characterAKnob.slider);
    characterBAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::characterB),
        characterBKnob.slider);
    pitchAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::pitch),
        pitchKnob.slider);
    decayAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::decay),
        decayKnob.slider);
    levelAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::level),
        levelKnob.slider);
    panAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::pan),
        panSlider);
    chokeAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.parameters,
        DrumalorAudioProcessor::parameterId (
            selectedInstrument, drumalor::parameters::choke),
        chokeBox);

    // JUCE attachments replace text conversion callbacks, but retain suffixes.
    characterAKnob.slider.setTextValueSuffix ("%");
    characterBKnob.slider.setTextValueSuffix ("%");
    pitchKnob.slider.setTextValueSuffix (" st");
    decayKnob.slider.setTextValueSuffix ("%");
    levelKnob.slider.setTextValueSuffix (" dB");
}

void DrumalorAudioProcessorEditor::timerCallback()
{
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto counter = audioProcessor.getTriggerCounter (instrument);
        if (counter != observedTriggerCounters[index])
        {
            observedTriggerCounters[index] = counter;
            pads[index]->triggerFlash();
        }
        pads[index]->advanceFlash();
        pads[index]->setLevel (audioProcessor.getInstrumentLevel (instrument));
    }

    busMeter.setLevels (audioProcessor.getOutputLevel (0),
                        audioProcessor.getOutputLevel (1),
                        audioProcessor.getBusGain());
    statusDisplay.setStatus (audioProcessor.getActiveVoiceCount(), audioProcessor.isEngineReady(),
                             audioProcessor.getCurrentSampleRateForDisplay());
}

DrumalorAudioProcessorEditor::LayoutAreas
DrumalorAudioProcessorEditor::calculateLayout() const
{
    auto content = getLocalBounds().reduced (18);
    LayoutAreas layout;
    layout.header = content.removeFromTop (72);
    content.removeFromTop (10);

    const auto padHeight = juce::jlimit (232, 300, content.getHeight() * 37 / 100);
    layout.pads = content.removeFromTop (padHeight);
    content.removeFromTop (10);
    layout.controls = content;

    auto controlContent = layout.controls.reduced (15);
    layout.controlHeader = controlContent.removeFromTop (42);
    const auto masterWidth = juce::jlimit (300, 430, controlContent.getWidth() * 38 / 100);
    layout.masterDeck = controlContent.removeFromRight (masterWidth);
    controlContent.removeFromRight (12);
    layout.voiceDeck = controlContent;
    layout.voiceStrip = layout.voiceDeck.removeFromBottom (52);
    layout.voiceDeck.removeFromBottom (6);

    layout.selectedVoiceHeader = layout.controlHeader;
    layout.selectedVoiceHeader.removeFromLeft (118);
    layout.selectedVoiceHeader.removeFromRight (masterWidth + 12);
    return layout;
}

void DrumalorAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (colour (chassis));
    // The generated plate is now material only. Runtime geometry is the sole
    // layout source, so resizing cannot reveal a second set of panels/knobs.
    drawScaledBackground (graphics, vintagePanel, getLocalBounds().toFloat(), 0.15f);

    juce::ColourGradient backgroundVeil (colour (0x520e100f), 0.0f, 0.0f,
                                         colour (0x76121413), 0.0f,
                                         static_cast<float> (getHeight()), false);
    backgroundVeil.addColour (0.50, colour (0x64181b18));
    graphics.setGradientFill (backgroundVeil);
    graphics.fillAll();

    const auto frame = getLocalBounds().toFloat().reduced (7.0f);
    graphics.setColour (juce::Colours::black.withAlpha (0.58f));
    graphics.drawRoundedRectangle (frame.translated (0.0f, 1.0f), 8.0f, 4.0f);
    graphics.setColour (colour (panelEdge).withAlpha (0.78f));
    graphics.drawRoundedRectangle (frame, 8.0f, 1.2f);

    drawHardwareScrew (graphics, frame.getTopLeft() + juce::Point<float> (10.0f, 10.0f));
    drawHardwareScrew (graphics, frame.getTopRight() + juce::Point<float> (-10.0f, 10.0f));
    drawHardwareScrew (graphics, frame.getBottomLeft() + juce::Point<float> (10.0f, -10.0f));
    drawHardwareScrew (graphics, frame.getBottomRight() + juce::Point<float> (-10.0f, -10.0f));

    const auto layout = calculateLayout();
    drawPanel (graphics, layout.pads);
    drawPanel (graphics, layout.controls);

    auto brandPlate = layout.header.toFloat().reduced (0.0f, 6.0f);
    brandPlate.setWidth (juce::jmin (310.0f, brandPlate.getWidth() * 0.30f));
    graphics.setColour (juce::Colours::black.withAlpha (0.45f));
    graphics.fillRoundedRectangle (brandPlate.translated (0.0f, 2.0f), 5.0f);
    juce::ColourGradient brandFill (colour (0xff242724), brandPlate.getTopLeft(),
                                    colour (0xff101210), brandPlate.getBottomRight(), false);
    graphics.setGradientFill (brandFill);
    graphics.fillRoundedRectangle (brandPlate, 5.0f);
    graphics.setColour (colour (panelEdge));
    graphics.drawRoundedRectangle (brandPlate.reduced (0.5f), 5.0f, 1.1f);

    const auto railY = static_cast<float> (layout.header.getBottom() - 5);
    const auto railX = static_cast<float> (layout.header.getX());
    const auto railSegment = brandPlate.getWidth() * 0.25f;
    graphics.setColour (colour (trRed));
    graphics.fillRect (railX, railY, railSegment, 3.0f);
    graphics.setColour (colour (trOrange));
    graphics.fillRect (railX + railSegment, railY, railSegment, 3.0f);
    graphics.setColour (colour (trYellow));
    graphics.fillRect (railX + railSegment * 2.0f, railY, railSegment, 3.0f);
    graphics.setColour (colour (trCream));
    graphics.fillRect (railX + railSegment * 3.0f, railY, railSegment, 3.0f);

    graphics.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    graphics.setColour (colour (textDim));
    graphics.drawText ("INSTRUMENT CHANNELS   |   SELECT + AUDITION   |   LIVE LEVELS",
                       layout.pads.reduced (14).removeFromTop (22),
                       juce::Justification::centredLeft);

    for (const auto deck : { layout.voiceDeck, layout.voiceStrip, layout.masterDeck })
    {
        const auto area = deck.toFloat();
        graphics.setColour (juce::Colours::black.withAlpha (0.24f));
        graphics.fillRoundedRectangle (area, 5.0f);
        graphics.setColour (colour (panelEdge).withAlpha (0.42f));
        graphics.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
    }

    const auto voiceAccent = padAccentFor (selectedInstrument);
    const auto voiceRail = juce::Rectangle<float> (
        static_cast<float> (layout.voiceDeck.getX() + 8),
        static_cast<float> (layout.voiceDeck.getY()), 72.0f, 3.0f);
    graphics.setColour (voiceAccent.withAlpha (0.92f));
    graphics.fillRoundedRectangle (voiceRail, 1.5f);

    auto circuitHeader = layout.controlHeader;
    graphics.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    graphics.setColour (voiceAccent);
    graphics.drawText ("VOICE CIRCUIT", circuitHeader.removeFromLeft (118),
                       juce::Justification::centredLeft);
    graphics.setColour (colour (trRed));
    graphics.drawText ("KIT BUS   |   HUMANISE + DRIVE + GLUE", layout.masterDeck.getX(),
                       layout.controlHeader.getY(), layout.masterDeck.getWidth(),
                       layout.controlHeader.getHeight(), juce::Justification::centred);
}

void DrumalorAudioProcessorEditor::resized()
{
    const auto layout = calculateLayout();

    auto header = layout.header.reduced (8, 5);
    auto brand = header.removeFromLeft (juce::jmin (300, header.getWidth() * 30 / 100));
    logoLabel.setBounds (brand.removeFromTop (36));
    editionLabel.setBounds (brand);

    panicButton.setBounds (header.removeFromRight (78).reduced (0, 9));
    header.removeFromRight (10);
    statusDisplay.setBounds (header.removeFromRight (
                                 juce::jlimit (150, 210, header.getWidth() / 3))
                                 .reduced (0, 9));
    header.removeFromRight (10);
    busMeter.setBounds (header.removeFromRight (
                            juce::jlimit (170, 260, header.getWidth() * 2 / 3))
                            .reduced (0, 12));

    auto padContent = layout.pads.reduced (14);
    padContent.removeFromTop (28);
    constexpr int rowGap = 8;
    auto firstRow = padContent.removeFromTop ((padContent.getHeight() - rowGap) / 2);
    padContent.removeFromTop (rowGap);
    auto secondRow = padContent;

    const auto layoutPadRow = [this] (juce::Rectangle<int> row,
                                      std::size_t firstIndex, int count)
    {
        const auto grid = drumalor::ui::rowLayout (
            row.getWidth(), padColumns, padGap, count);
        for (int offset = 0; offset < count; ++offset)
            pads[firstIndex + static_cast<std::size_t> (offset)]->setBounds (
                row.getX() + drumalor::ui::cellOffset (grid, padGap, offset),
                row.getY(), grid.cellSize, row.getHeight());
    };

    layoutPadRow (firstRow, 0, padColumns);
    layoutPadRow (secondRow, static_cast<std::size_t> (padColumns),
                  static_cast<int> (drumalor::instrumentCount) - padColumns);

    selectedInstrumentLabel.setBounds (layout.selectedVoiceHeader);

    auto voiceKnobRow = layout.voiceDeck.reduced (8, 6);
    voiceKnobRow = voiceKnobRow.withSizeKeepingCentre (
        voiceKnobRow.getWidth(), juce::jmin (250, voiceKnobRow.getHeight()));
    constexpr int voiceKnobCount = 5;
    DrumalorKnob* voiceKnobs[voiceKnobCount] = { &characterAKnob, &characterBKnob,
                                                 &pitchKnob, &decayKnob, &levelKnob };
    const auto voiceGrid = drumalor::ui::rowLayout (
        voiceKnobRow.getWidth(), voiceKnobCount, 0, voiceKnobCount);
    for (int index = 0; index < voiceKnobCount; ++index)
        voiceKnobs[index]->setBounds (
            juce::Rectangle<int> (
                voiceKnobRow.getX() + drumalor::ui::cellOffset (voiceGrid, 0, index),
                voiceKnobRow.getY(), voiceGrid.cellSize, voiceKnobRow.getHeight())
                .reduced (4, 0));

    auto strip = layout.voiceStrip.reduced (10, 8);
    auto chokeArea = strip.removeFromRight (
        juce::jlimit (170, 260, strip.getWidth() * 2 / 5));
    chokeLabel.setBounds (chokeArea.removeFromLeft (
        juce::jmin (90, chokeArea.getWidth() / 2)));
    chokeBox.setBounds (chokeArea.reduced (0, 2));
    strip.removeFromRight (12);
    panLabel.setBounds (strip.removeFromLeft (juce::jmin (36, strip.getWidth() / 4)));
    panSlider.setBounds (strip);

    // Five kit controls in a two-column grid, so the last row carries one knob
    // and an empty cell rather than being squeezed into a different shape.
    constexpr int masterKnobCount = 5;
    constexpr int masterRowCount = (masterKnobCount + 1) / 2;
    auto master = layout.masterDeck.reduced (8, 6);
    master = master.withSizeKeepingCentre (
        master.getWidth(), juce::jmin (560, master.getHeight()));
    DrumalorKnob* masterKnobs[masterKnobCount] = {
        &humaniseKnob, &bleedKnob, &busDriveKnob, &busCompressionKnob, &outputKnob
    };
    const auto rowHeight = master.getHeight() / masterRowCount;
    for (int row = 0; row < masterRowCount; ++row)
    {
        auto cells = row == masterRowCount - 1
            ? master : master.removeFromTop (rowHeight);
        const auto half = cells.getWidth() / 2;
        const int first = row * 2;
        masterKnobs[first]->setBounds (cells.removeFromLeft (half).reduced (4, 0));
        if (first + 1 < masterKnobCount)
            masterKnobs[first + 1]->setBounds (cells.reduced (4, 0));
    }
}

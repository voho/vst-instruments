#include "PluginEditor.h"

#include <cmath>
#include <initializer_list>
#include <utility>

namespace
{
// JUNO-era instruments used a restrained charcoal chassis with high-contrast
// ivory legends and small, functional colour bands. Keep Mars recognisably its
// own instrument while borrowing that hierarchy: orange for signal/action,
// cyan for modulation/quality, and red only for warnings or filter emphasis.
constexpr auto coal = 0xff101414;
constexpr auto panel = 0xff1c2021;
constexpr auto panelRaised = 0xff292f30;
constexpr auto panelEdge = 0xff596263;
constexpr auto accentOrange = 0xffdf7834;
constexpr auto accentAmber = 0xfff0a23d;
constexpr auto accentBlue = 0xff3497b7;
constexpr auto accentBlueBright = 0xff68c2d5;
constexpr auto accentBlueDark = 0xff1c5668;
constexpr auto signalRed = 0xffd24d40;
constexpr auto lamp = 0xfff4c54c;
constexpr auto textBright = 0xfff0eee5;
constexpr auto textDim = 0xffb8c0bb;
constexpr auto screen = 0xff101a1b;
constexpr auto knobRimLight = 0xffa9afa9;
constexpr auto knobRimMid = 0xff6d7471;
constexpr auto knobRimDark = 0xff303637;

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
                return juce::String (value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";
            return juce::String (value, value < 10.0 ? 2 : 0) + " Hz";
        case MarsValueFormat::Seconds:
            if (value < 1.0)
                return juce::String (juce::roundToInt (value * 1000.0)) + " ms";
            return juce::String (value, value < 10.0 ? 2 : 1) + " s";
        case MarsValueFormat::Cents:
            return (value > 0.0 ? "+" : "") + juce::String (value, 1) + " ct";
        case MarsValueFormat::Semitones:
            return (value > 0.0 ? "+" : "") + juce::String (juce::roundToInt (value)) + " st";
        case MarsValueFormat::Octaves:
            if (std::abs (value - std::round (value)) < 0.001)
                return (value > 0.0 ? "+" : "")
                     + juce::String (juce::roundToInt (value)) + " oct";
            return juce::String (value, 2) + " oct";
        case MarsValueFormat::Decibels:
            return (value > 0.0 ? "+" : "") + juce::String (value, 1) + " dB";
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
                             int gap)
{
    const auto count = static_cast<int> (components.size());
    if (count == 0)
        return;

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

// Lays components into a fixed column grid so a row with fewer controls still
// lines up with the row above it.
void distributeInColumns (juce::Rectangle<int> area,
                          std::initializer_list<juce::Component*> components,
                          int columnCount, int gap)
{
    const auto count = static_cast<int> (components.size());
    if (count == 0 || columnCount <= 0)
        return;

    const auto available = juce::jmax (0, area.getWidth() - gap * (columnCount - 1));
    const auto columnWidth = available / columnCount;
    int index = 0;
    for (auto* component : components)
    {
        component->setBounds (area.removeFromLeft (columnWidth));
        if (++index < count)
            area.removeFromLeft (gap);
    }
}

juce::String midiNoteName (int note)
{
    static constexpr std::array<const char*, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    if (note < 0 || note > 127)
        return "--";
    return juce::String (names[static_cast<size_t> (note % 12)])
         + juce::String (note / 12 - 1);
}

void drawChassisSurface (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient base (c (0xff272d2e), bounds.getTopLeft(), c (coal),
                               bounds.getBottomRight(), false);
    base.addColour (0.18, c (0xff202627));
    base.addColour (0.62, c (0xff14191a));
    base.addColour (0.88, c (0xff1b2021));
    g.setGradientFill (base);
    g.fillRect (bounds);

    g.saveState();
    g.reduceClipRegion (bounds.getSmallestIntegerContainer());
    for (int y = juce::roundToInt (bounds.getY()) + 4;
         y < juce::roundToInt (bounds.getBottom()); y += 5)
    {
        juce::Path grain;
        const auto yf = static_cast<float> (y);
        grain.startNewSubPath (bounds.getX(), yf);
        for (float x = bounds.getX() + 24.0f; x <= bounds.getRight() + 24.0f; x += 24.0f)
        {
            const auto wobble = 0.65f * std::sin (x * 0.031f + yf * 0.17f)
                              + 0.28f * std::sin (x * 0.083f - yf * 0.09f);
            grain.lineTo (x, yf + wobble);
        }

        const auto highlight = ((y / 5) % 3) == 0;
        g.setColour ((highlight ? c (accentBlueBright) : juce::Colours::black)
                         .withAlpha (highlight ? 0.018f : 0.045f));
        g.strokePath (grain, juce::PathStrokeType (highlight ? 0.7f : 1.0f));
    }
    g.restoreState();
}
} // namespace

MarsLookAndFeel::MarsLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, c (textBright));
    setColour (juce::Slider::textBoxBackgroundColourId, c (0xc0121718));
    setColour (juce::Slider::textBoxOutlineColourId, c (0xff4d5859));
    setColour (juce::Slider::rotarySliderFillColourId, c (accentOrange));
    setColour (juce::Slider::rotarySliderOutlineColourId, c (0xff50595a));
    setColour (juce::TextButton::textColourOffId, c (textDim));
    setColour (juce::TextButton::textColourOnId, c (textBright));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, c (0xffe7e6dc));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, c (0xff191e1f));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, c (0xff555f60));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, c (0xe5488797));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, c (0xe8c65332));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, c (0xff596263));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.62f));
    setColour (juce::KeyboardComponentBase::upDownButtonBackgroundColourId, c (0xff202627));
    setColour (juce::KeyboardComponentBase::upDownButtonArrowColourId, c (accentBlueBright));
}

void MarsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = juce::jmax (22.0f,
        static_cast<float> (juce::jmin (width, height)) - 5.0f);
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f);
    const auto dialBounds = juce::Rectangle<float> (diameter * 0.72f, diameter * 0.72f)
                                .withCentre (centre);
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    for (int tick = 0; tick <= 10; ++tick)
    {
        const auto proportion = static_cast<float> (tick) / 10.0f;
        const auto tickAngle = rotaryStartAngle
                             + proportion * (rotaryEndAngle - rotaryStartAngle);
        const auto tickOuter = centre.getPointOnCircumference (radius * 0.98f, tickAngle);
        const auto tickInner = centre.getPointOnCircumference (
            radius * (tick == 0 || tick == 5 || tick == 10 ? 0.82f : 0.86f), tickAngle);
        g.setColour (c (tick <= juce::roundToInt (sliderPos * 10.0f)
                           ? accentAmber : panelEdge).withAlpha (0.88f));
        g.drawLine ({ tickInner, tickOuter }, tick % 5 == 0 ? 1.55f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius * 0.77f, radius * 0.77f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius * 0.77f, radius * 0.77f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (c (accentOrange));
    g.strokePath (valueArc, juce::PathStrokeType (2.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillEllipse (dialBounds.expanded (2.0f, 1.0f).translated (0.0f, 3.0f));

    juce::ColourGradient rim (c (knobRimLight), dialBounds.getTopLeft(), c (knobRimDark),
                              dialBounds.getBottomRight(), false);
    rim.addColour (0.46, c (knobRimMid));
    rim.addColour (0.72, c (0xff444b4a));
    g.setGradientFill (rim);
    g.fillEllipse (dialBounds);
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.drawEllipse (dialBounds, 1.2f);

    const auto gripRadius = dialBounds.getWidth() * 0.5f;
    for (int groove = 0; groove < 18; ++groove)
    {
        const auto grooveAngle = juce::MathConstants<float>::twoPi
                               * static_cast<float> (groove) / 18.0f;
        const auto grooveOuter = centre.getPointOnCircumference (gripRadius * 0.91f,
                                                                 grooveAngle);
        const auto grooveInner = centre.getPointOnCircumference (gripRadius * 0.76f,
                                                                 grooveAngle);
        g.setColour ((grooveAngle < juce::MathConstants<float>::pi
                         ? c (0xffc6cbc5) : c (0xff242a2b)).withAlpha (0.52f));
        g.drawLine ({ grooveInner, grooveOuter }, 1.0f);
    }

    auto capBounds = dialBounds.reduced (dialBounds.getWidth() * 0.17f);
    juce::ColourGradient cap (c (0xff3c4344), capBounds.getX() + capBounds.getWidth() * 0.28f,
                              capBounds.getY() + capBounds.getHeight() * 0.22f,
                              c (0xff0f1415), capBounds.getRight(), capBounds.getBottom(), true);
    cap.addColour (0.58, c (0xff202627));
    g.setGradientFill (cap);
    g.fillEllipse (capBounds);
    g.setColour (c (0xff687172).withAlpha (0.72f));
    g.drawEllipse (capBounds, 1.0f);

    const auto pointerStart = centre.getPointOnCircumference (gripRadius * 0.18f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (gripRadius * 0.60f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawLine ({ pointerStart.translated (0.0f, 1.2f), pointerEnd.translated (0.0f, 1.2f) },
                juce::jmax (2.0f, gripRadius * 0.075f));
    g.setColour (c (textBright));
    g.drawLine ({ pointerStart, pointerEnd }, juce::jmax (1.6f, gripRadius * 0.052f));
    g.setColour (c (signalRed));
    g.fillEllipse (juce::Rectangle<float> (juce::jmax (3.0f, gripRadius * 0.12f),
                                            juce::jmax (3.0f, gripRadius * 0.12f))
                       .withCentre (centre));

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (c (lamp));
        g.drawEllipse (juce::Rectangle<float> (diameter, diameter).withCentre (centre)
                           .reduced (1.5f), 1.35f);
    }
}

void MarsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    const auto panic = button.getButtonText() == "PANIC";
    const auto randomizer = button.getComponentID() == "preset-randomizer";
    auto fill = panic ? c (0xff551f1d)
                      : randomizer ? c (0xff392a1d)
                      : active ? c (accentBlueDark) : c (0xff1a2021);
    if (highlighted)
        fill = fill.brighter (0.12f);
    if (down)
        fill = fill.darker (0.18f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 3.5f);
    juce::ColourGradient face (fill.brighter (0.10f), bounds.getTopLeft(),
                               fill.darker (0.25f), bounds.getBottomLeft(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (panic ? c (signalRed)
                       : randomizer ? c (accentAmber)
                       : active ? c (accentBlueBright) : c (panelEdge));
    g.drawRoundedRectangle (bounds, 3.0f, active || panic ? 1.35f : 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 2.0f,
                bounds.getRight() - 4.0f, bounds.getY() + 2.0f, 1.0f);

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

void MarsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                      bool, bool isDown)
{
    const auto active = button.getToggleState();
    auto colour = button.findColour (active ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId);
    if (isDown)
        colour = colour.darker (0.12f);

    auto bounds = button.getLocalBounds().reduced (5, 1);
    if (isDown)
        bounds.translate (0, 1);
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawFittedText (button.getButtonText(), bounds.translated (0, 1),
                      juce::Justification::centred, 1);
    g.setColour (colour);
    g.drawFittedText (button.getButtonText(), bounds, juce::Justification::centred, 1);
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
        const auto top = static_cast<float> (y + 7);
        const auto bottom = static_cast<float> (y + height - 7);
        for (int tick = 0; tick <= 8; ++tick)
        {
            const auto ty = top + (bottom - top) * static_cast<float> (tick) / 8.0f;
            const auto tickWidth = tick % 4 == 0 ? 7.0f : 4.0f;
            g.setColour (c (panelEdge).withAlpha (0.72f));
            g.drawLine (cx - tickWidth - 5.0f, ty, cx - 5.0f, ty, 1.0f);
            g.drawLine (cx + 5.0f, ty, cx + tickWidth + 5.0f, ty, 1.0f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.78f));
        g.drawLine (cx, top, cx, bottom, 8.0f);
        g.setColour (c (0xff5b6566));
        g.drawLine (cx, top, cx, bottom, 3.0f);
        g.setColour (c (0xff111718));
        g.drawLine (cx, top, cx, bottom, 1.2f);
        g.setColour (c (accentOrange));
        g.drawLine (cx, sliderPos, cx, bottom, 2.0f);

        auto thumb = juce::Rectangle<float> (
                         juce::jmin (28.0f, static_cast<float> (width) * 0.68f), 14.0f)
                         .withCentre ({ cx, sliderPos });
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.5f), 2.8f);
        juce::ColourGradient cap (c (accentBlueBright), thumb.getTopLeft(), c (accentBlueDark),
                                  thumb.getBottomLeft(), false);
        cap.addColour (0.46, c (accentBlue));
        g.setGradientFill (cap);
        g.fillRoundedRectangle (thumb, 2.5f);
        g.setColour (c (0xff183c44));
        g.drawRoundedRectangle (thumb, 2.5f, 1.0f);
        g.setColour (c (0xff245c68).withAlpha (0.82f));
        for (float grip = thumb.getX() + 5.0f; grip < thumb.getRight() - 3.0f; grip += 4.0f)
            g.drawLine (grip, thumb.getY() + 3.0f, grip, thumb.getBottom() - 3.0f, 0.8f);
        g.setColour (c (textBright).withAlpha (0.45f));
        g.drawLine (thumb.getX() + 3.0f, thumb.getCentreY(), thumb.getRight() - 3.0f,
                    thumb.getCentreY(), 0.8f);
        drawFocusRing();
        return;
    }

    if (style == juce::Slider::LinearHorizontal)
    {
        const auto cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
        const auto left = static_cast<float> (x + 4);
        const auto right = static_cast<float> (x + width - 4);
        g.setColour (juce::Colours::black.withAlpha (0.78f));
        g.drawLine (left, cy, right, cy, 8.0f);
        g.setColour (c (0xff596465));
        g.drawLine (left, cy, right, cy, 3.0f);
        g.setColour (c (accentOrange));
        g.drawLine (left, cy, sliderPos, cy, 2.0f);
        auto thumb = juce::Rectangle<float> (14.0f,
                                              juce::jmin (28.0f, static_cast<float> (height) * 0.7f))
                         .withCentre ({ sliderPos, cy });
        juce::ColourGradient cap (c (accentBlueBright), thumb.getTopLeft(), c (accentBlueDark),
                                  thumb.getBottomRight(), false);
        g.setGradientFill (cap);
        g.fillRoundedRectangle (thumb, 2.5f);
        g.setColour (c (0xff183c44));
        g.drawRoundedRectangle (thumb, 2.5f, 1.0f);
        drawFocusRing();
    }
}

void MarsLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr)
    {
        juce::LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    auto bounds = label.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), 2.5f);
    juce::ColourGradient inset (c (0xff0e1314), bounds.getTopLeft(), c (0xff202627),
                                bounds.getBottomLeft(), false);
    g.setGradientFill (inset);
    g.fillRoundedRectangle (bounds, 2.2f);
    g.setColour (label.hasKeyboardFocus (true) ? c (lamp) : c (0xff596566));
    g.drawRoundedRectangle (bounds, 2.2f, label.hasKeyboardFocus (true) ? 1.2f : 0.8f);

    if (! label.isBeingEdited())
    {
        g.setFont (label.getFont());
        // The containing control already carries the disabled alpha. Applying a
        // second fade here made small value text nearly disappear.
        g.setColour (label.findColour (juce::Label::textColourId));
        g.drawFittedText (label.getText(), label.getLocalBounds().reduced (3, 0),
                          label.getJustificationType(), 1, 0.78f);
    }
}

juce::Label* MarsLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, c (textBright));
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::textWhenEditingColourId, c (textBright));
    return label;
}

juce::Font MarsLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (
        juce::jlimit (8.5f, 11.5f, static_cast<float> (buttonHeight) * 0.40f),
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
        button->setTooltip (button->getDescription());
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
    label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
    slider.setName (name);
    slider.setTitle (name);
    slider.setDescription ("Adjust " + name.toLowerCase());
    slider.setTooltip ("Adjust " + name.toLowerCase());
    slider.setWantsKeyboardFocus (true);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    configureValueDisplay (slider, format);
    addAndMakeVisible (slider);
}

void MarsKnob::resized()
{
    constexpr int maximumControlStackHeight = 134;
    constexpr int labelToDialGap = 4;
    auto area = getLocalBounds().withSizeKeepingCentre (
        getWidth(), juce::jmin (maximumControlStackHeight, getHeight()));
    label.setBounds (area.removeFromTop (16));
    area.removeFromTop (labelToDialGap);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                            juce::jlimit (40, 64, getWidth() - 4), 18);
    slider.setBounds (area);
}

MarsFader::MarsFader (juce::String name, MarsValueFormat format)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, c (textDim));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 18);
    slider.setName (name);
    slider.setTitle (name);
    slider.setDescription ("Adjust " + name.toLowerCase());
    slider.setTooltip ("Adjust " + name.toLowerCase());
    slider.setWantsKeyboardFocus (true);
    configureValueDisplay (slider, format);
    addAndMakeVisible (slider);
}

void MarsFader::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                            juce::jlimit (38, 58, getWidth() - 4), 18);
    slider.setBounds (area);
}

MarsScopeDisplay::MarsScopeDisplay()
{
    trace.resize (static_cast<size_t> (traceSamples), 0.0f);
    reducer.setColumns (128);
    reducer.setBallistics (12.0f, 0.55f, 1.1f);
    reducer.reset();
    setInterceptsMouseClicks (false, false);
    setAccessible (true);
    setTitle ("Mars signal scope");
    setDescription ("Shows the output waveform, level, and arpeggiator step");
}

void MarsScopeDisplay::refresh (const MarsAudioProcessor& processor,
                                bool arpeggiatorEnabled,
                                const juce::String& arpeggiatorMode)
{
    const auto count = processor.copyScopeTrace (trace.data(), traceSamples);
    const auto start = mars::ScopeReducer::findRisingEdge (trace.data(), count);
    reducer.reduce (trace.data() + start, count - start);

    arpActive = arpeggiatorEnabled;
    modeText = arpeggiatorMode;
    arpNote = processor.getArpeggiatorNoteForDisplay();
    arpKeys = processor.getArpeggiatorKeyCountForDisplay();
    arpPhase = juce::jlimit (0.0f, 1.0f, processor.getArpeggiatorPhaseForDisplay());

    const auto peakDecibels = reducer.peakHold() > 0.0f
        ? juce::String (juce::Decibels::gainToDecibels (reducer.peakHold()), 1) + " dB"
        : juce::String { "-inf dB" };
    const auto nextStatus = arpActive
        ? "ARP " + modeText + "  " + midiNoteName (arpNote) + "  " + peakDecibels
        : "OUTPUT  " + peakDecibels;
    if (nextStatus != statusText)
    {
        statusText = nextStatus;
        setDescription ("Output waveform and level, peak " + peakDecibels
                        + (arpActive ? ", arpeggiator running " + modeText
                                     : juce::String { ", arpeggiator off" }));
    }
    repaint();
}

void MarsScopeDisplay::resized()
{
    // One column per two pixels keeps the trace dense without over-sampling the
    // reduction on a wide editor.
    reducer.setColumns (juce::jlimit (mars::ScopeReducer::minimumColumns,
                                      mars::ScopeReducer::maximumColumns,
                                      juce::jmax (2, getWidth() / 2)));
}

void MarsScopeDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (c (screen));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (c (accentBlueDark));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    auto inner = bounds.reduced (5.0f, 4.0f);
    if (inner.getWidth() < 8.0f || inner.getHeight() < 12.0f)
        return;

    auto meterArea = inner.removeFromBottom (juce::jmin (7.0f, inner.getHeight() * 0.24f));
    inner.removeFromBottom (2.0f);

    // Graticule.
    g.setColour (c (accentBlueDark).withAlpha (0.55f));
    for (int division = 1; division < 4; ++division)
    {
        const auto x = inner.getX() + inner.getWidth() * static_cast<float> (division) / 4.0f;
        g.drawLine (x, inner.getY(), x, inner.getBottom(), 0.6f);
    }
    g.setColour (c (accentBlueDark).withAlpha (0.75f));
    g.drawLine (inner.getX(), inner.getCentreY(), inner.getRight(), inner.getCentreY(), 0.8f);

    // Waveform: a filled minimum/maximum band, which reads correctly even when
    // one column spans many cycles of a high note.
    const auto columns = reducer.getColumns();
    const auto half = inner.getHeight() * 0.5f;
    juce::Path band;
    bool started = false;
    for (int column = 0; column < columns; ++column)
    {
        const auto x = inner.getX()
                     + inner.getWidth() * static_cast<float> (column)
                           / static_cast<float> (juce::jmax (1, columns - 1));
        const auto top = inner.getCentreY()
                       - juce::jlimit (-1.0f, 1.0f, reducer.columnMaximum (column)) * half;
        if (! started)
        {
            band.startNewSubPath (x, top);
            started = true;
        }
        else
        {
            band.lineTo (x, top);
        }
    }
    for (int column = columns - 1; column >= 0 && started; --column)
    {
        const auto x = inner.getX()
                     + inner.getWidth() * static_cast<float> (column)
                           / static_cast<float> (juce::jmax (1, columns - 1));
        const auto bottom = inner.getCentreY()
                          - juce::jlimit (-1.0f, 1.0f, reducer.columnMinimum (column)) * half;
        band.lineTo (x, bottom);
    }
    if (started)
    {
        band.closeSubPath();
        g.setColour (c (accentBlueBright).withAlpha (0.28f));
        g.fillPath (band);
        g.setColour (c (accentBlueBright).withAlpha (0.92f));
        g.strokePath (band, juce::PathStrokeType (1.0f));
    }

    // Output meter with a held peak marker.
    const auto meterValue = mars::ScopeReducer::amplitudeToMeter (reducer.rms());
    const auto peakValue = mars::ScopeReducer::amplitudeToMeter (reducer.peakHold());
    const auto warmth = mars::ScopeReducer::meterWarmth (peakValue);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (meterArea, 1.5f);
    if (meterValue > 0.0f)
    {
        auto filled = meterArea.withWidth (meterArea.getWidth() * meterValue);
        g.setColour (c (accentBlue).interpolatedWith (c (signalRed), warmth));
        g.fillRoundedRectangle (filled, 1.5f);
    }
    if (peakValue > 0.0f)
    {
        const auto x = meterArea.getX() + meterArea.getWidth() * peakValue;
        g.setColour (peakValue > 0.97f ? c (signalRed) : c (accentAmber));
        g.fillRect (juce::Rectangle<float> (juce::jmax (meterArea.getX(), x - 1.0f),
                                            meterArea.getY(), 1.6f, meterArea.getHeight()));
    }

    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (c (arpActive ? accentAmber : textDim).withAlpha (0.92f));
    g.drawText (statusText, inner.toNearestInt().reduced (2, 1),
                juce::Justification::topLeft);

    if (arpActive)
    {
        // Step progress: a thin bar that refills once per arpeggiator step.
        const auto lane = juce::Rectangle<float> (inner.getRight() - 46.0f,
                                                  inner.getY() + 2.0f, 44.0f, 3.0f);
        g.setColour (c (accentBlueDark));
        g.fillRect (lane);
        g.setColour (c (arpKeys > 0 ? lamp : panelEdge));
        g.fillRect (lane.withWidth (lane.getWidth() * arpPhase));
    }
}

void MarsStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate,
                                   bool scheduleRepaint)
{
    if (voices == activeVoices && isReady == ready && std::abs (rate - sampleRate) < 1.0)
        return;
    voices = activeVoices;
    isReady = ready;
    rate = sampleRate;
    const auto rateText = rate > 0.0
        ? juce::String (rate / 1000.0, rate < 100000.0 ? 1 : 0) + " kilohertz"
        : juce::String { "offline" };
    setTitle ("Mars engine status");
    setDescription (juce::String (juce::jmax (0, voices)) + " active voices, engine "
                    + (isReady ? "ready, " : "not ready, ") + rateText);
    if (scheduleRepaint)
        repaint();
}

void MarsStatusDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (c (screen));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (c (accentBlueDark));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto light = juce::Rectangle<float> (bounds.getX() + 10.0f,
                                               bounds.getCentreY() - 3.5f, 7.0f, 7.0f);
    g.setColour (isReady ? c (lamp) : c (0xff566164));
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
    auto rateText = juce::String { "OFFLINE" };
    if (rate > 0.0)
    {
        auto kilohertz = juce::String (rate / 1000.0, 1);
        kilohertz = kilohertz.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        rateText = kilohertz + " kHz";
    }
    g.drawText (rateText, 100, 0, getWidth() - 108, getHeight(), juce::Justification::centredRight);
}

MarsAudioProcessorEditor::MarsAudioProcessorEditor (MarsAudioProcessor& p)
    : AudioProcessorEditor (&p), marsProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("MARS", juce::dontSendNotification);
    styleHeaderLabel (logoLabel, 29.0f, c (textBright));
    logoLabel.setAccessible (true);
    logoLabel.setTitle ("Mars analog-modeled synthesizer");
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("DUAL-CORE ANALOG-MODELED ENGINE  |  DIRECT CONTROL",
                          juce::dontSendNotification);
    styleHeaderLabel (editionLabel, 10.0f, c (accentBlueBright));
    editionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (editionLabel);

    randomizerLabel.setText ("PRESET RANDOMIZER  /  RANGE", juce::dontSendNotification);
    styleHeaderLabel (randomizerLabel, 9.0f, c (accentAmber));
    randomizerLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (randomizerLabel);

    const auto configureRandomizer = [this] (juce::TextButton& button, float amount,
                                             const juce::String& description)
    {
        button.setComponentID ("preset-randomizer");
        button.setWantsKeyboardFocus (true);
        button.setName ("Randomize preset by " + button.getButtonText());
        button.setTitle ("Preset randomizer " + button.getButtonText());
        button.setDescription (description);
        button.setTooltip (description);
        button.setColour (juce::TextButton::textColourOffId, c (textBright));
        button.onClick = [this, amount]
        {
            marsProcessor.randomizeParameters (amount);
            updateConditionalControls();
        };
        addAndMakeVisible (button);
    };
    configureRandomizer (randomize1Button, 0.01f,
                         "Gently mutate sound parameters within one percent of their ranges");
    configureRandomizer (randomize10Button, 0.10f,
                         "Mutate sound parameters within ten percent of their ranges");
    configureRandomizer (randomize100Button, 1.0f,
                         "Create a fully randomized sound across the available parameter ranges");

    addAndMakeVisible (scopeDisplay);

    addAndMakeVisible (statusDisplay);
    statusDisplay.setAccessible (true);
    statusDisplay.setTitle ("Mars engine status");
    statusDisplay.setDescription ("Displays active voice count, engine state, and sample rate");
    panicButton.setColour (juce::TextButton::textColourOffId, c (0xfff8d4ce));
    panicButton.setName ("Panic — stop all voices");
    panicButton.setDescription ("Immediately mute every sounding voice");
    panicButton.setTooltip ("Immediately mute every sounding voice");
    panicButton.onClick = [this]
    {
        marsProcessor.keyboardState.reset();
        marsProcessor.requestPanic();
    };
    addAndMakeVisible (panicButton);

    oversamplingButton.setClickingTogglesState (true);
    oversamplingButton.setWantsKeyboardFocus (true);
    oversamplingButton.setName ("HQ oversampling");
    oversamplingButton.setTitle ("High-quality oversampling");
    oversamplingButton.setDescription (
        "Use 4X at 44.1/48 kHz, 2X at 88.2/96 kHz, and native processing at 176.4 kHz or higher; changes wait for silence");
    oversamplingButton.setTooltip (oversamplingButton.getDescription());
    oversamplingButton.setColour (juce::TextButton::textColourOffId, c (textDim));
    oversamplingButton.setColour (juce::TextButton::textColourOnId, c (textBright));
    oversamplingButton.onStateChange = [this] { updateConditionalControls(); };
    addAndMakeVisible (oversamplingButton);

    const auto configureOscillatorPower = [this] (juce::TextButton& button,
                                                   const juce::String& oscillatorName)
    {
        button.setClickingTogglesState (true);
        button.setWantsKeyboardFocus (true);
        button.setComponentID ("oscillator-power");
        button.setName (oscillatorName + " mixer feed");
        button.setTitle (oscillatorName + " audible mixer feed");
        button.setDescription ("Enable or disable " + oscillatorName
                               + " in the audible mixer; modulation remains active");
        button.setTooltip (button.getDescription());
        button.setColour (juce::TextButton::textColourOffId, c (textDim));
        button.setColour (juce::TextButton::textColourOnId, c (textBright));
        button.onStateChange = [this] { updateConditionalControls(); };
        addAndMakeVisible (button);
    };
    configureOscillatorPower (osc1EnableButton, "OSC I");
    configureOscillatorPower (osc2EnableButton, "OSC II");

    monoButton.setClickingTogglesState (true);
    monoButton.setWantsKeyboardFocus (true);
    monoButton.setName ("Monophonic mode");
    monoButton.setTitle ("Mono mode");
    monoButton.setDescription (
        "Play one note at a time; overrides the polyphonic voice mode");
    monoButton.setTooltip (monoButton.getDescription());
    monoButton.setColour (juce::TextButton::textColourOffId, c (textDim));
    monoButton.setColour (juce::TextButton::textColourOnId, c (textBright));
    monoButton.onStateChange = [this] { updateConditionalControls(); };
    addAndMakeVisible (monoButton);

    companderButton.setClickingTogglesState (true);
    companderButton.setWantsKeyboardFocus (true);
    companderButton.setName ("Ensemble studio compander");
    companderButton.setTitle ("Non-Juno BBD compander");
    companderButton.setDescription (
        "Enable an NE570-style compressor and expander around the BBD; the authentic Juno ensemble leaves this Off");
    companderButton.setTooltip (companderButton.getDescription());
    companderButton.setColour (juce::TextButton::textColourOffId, c (textDim));
    companderButton.setColour (juce::TextButton::textColourOnId, c (textBright));
    addAndMakeVisible (companderButton);

    arpEnableButton.setClickingTogglesState (true);
    arpEnableButton.setWantsKeyboardFocus (true);
    arpEnableButton.setName ("Arpeggiator");
    arpEnableButton.setTitle ("Arpeggiator");
    arpEnableButton.setDescription (
        "Play held keys as a free-running arpeggio through the normal voice allocator");
    arpEnableButton.setTooltip (arpEnableButton.getDescription());
    arpEnableButton.setColour (juce::TextButton::textColourOffId, c (textDim));
    arpEnableButton.setColour (juce::TextButton::textColourOnId, c (textBright));
    arpEnableButton.onStateChange = [this] { updateConditionalControls(); };
    addAndMakeVisible (arpEnableButton);

    arpHoldButton.setClickingTogglesState (true);
    arpHoldButton.setWantsKeyboardFocus (true);
    arpHoldButton.setName ("Arpeggiator hold");
    arpHoldButton.setTitle ("Arpeggiator hold");
    arpHoldButton.setDescription (
        "Latch the last chord so the arpeggio continues after the keys are released");
    arpHoldButton.setTooltip (arpHoldButton.getDescription());
    arpHoldButton.setColour (juce::TextButton::textColourOffId, c (textDim));
    arpHoldButton.setColour (juce::TextButton::textColourOnId, c (textBright));
    addAndMakeVisible (arpHoldButton);

    for (auto* strip : { &osc1ModelStrip, &osc2ModelStrip, &osc1WaveStrip,
                         &osc2WaveStrip, &filterModelStrip, &lfoWaveStrip,
                         &voiceModeStrip, &arpModeStrip })
        addAndMakeVisible (*strip);

    for (auto* knob : { &osc1OctaveKnob, &osc2OctaveKnob, &osc2TuneKnob,
                        &osc2FineKnob, &oscMixKnob, &pulseWidthKnob, &subLevelKnob,
                        &noiseLevelKnob, &crossModKnob, &cutoffKnob, &resonanceKnob,
                        &filterDriveKnob, &filterShapeKnob, &filterEnvKnob,
                        &keyTrackKnob, &lfoRateKnob, &lfoPitchKnob, &lfoFilterKnob,
                        &lfoPwmKnob, &polyphonyKnob, &unisonVoicesKnob,
                        &unisonDetuneKnob, &driftKnob,
                        &spreadKnob, &glideKnob, &velocityKnob, &chorusMixKnob,
                        &chorusRateKnob, &outputKnob,
                        &arpRateKnob, &arpOctavesKnob, &arpGateKnob })
        addAndMakeVisible (*knob);

    for (auto* fader : { &filterAttackFader, &filterDecayFader, &filterSustainFader,
                         &filterReleaseFader, &ampAttackFader, &ampDecayFader,
                         &ampSustainFader, &ampReleaseFader })
        addAndMakeVisible (*fader);

    keyboard.setAvailableRange (0, 127);
    keyboard.setLowestVisibleKey (24);
    keyboard.setKeyWidth (23.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (3);
    keyboard.setName ("Playable piano keyboard");
    keyboard.setTitle ("Playable keyboard");
    keyboard.setDescription ("Play notes with the mouse or computer keyboard");
    addAndMakeVisible (keyboard);

    keyboardHintLabel.setText ("MOUSE: MIDI 0-127  |  COMPUTER KEYS: A W S E D F T G Y H U J K O L P ;",
                               juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    keyboardHintLabel.setColour (juce::Label::textColourId, c (textDim));
    keyboardHintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (keyboardHintLabel);

    const auto describe = [] (juce::Slider& slider, const juce::String& name,
                              const juce::String& description)
    {
        slider.setName (name);
        slider.setTitle (name);
        slider.setDescription (description);
        slider.setTooltip (description);
    };
    describe (osc1OctaveKnob.slider, "Oscillator I octave", "Transpose oscillator I by octaves");
    describe (osc2OctaveKnob.slider, "Oscillator II octave", "Transpose oscillator II by octaves");
    describe (osc2TuneKnob.slider, "Oscillator II semitone tune", "Tune oscillator II in semitones");
    describe (osc2FineKnob.slider, "Oscillator II fine tune", "Fine-tune oscillator II in cents");
    describe (oscMixKnob.slider, "Oscillator balance", "Blend oscillator I and oscillator II");
    describe (pulseWidthKnob.slider, "Pulse width", "Set the oscillator pulse-wave width");
    describe (subLevelKnob.slider, "Sub-oscillator level", "Set the divided sub-oscillator level");
    describe (noiseLevelKnob.slider, "Noise level", "Set the white-noise mixer level");
    describe (crossModKnob.slider, "Cross modulation", "Frequency-modulate oscillator I from oscillator II");
    describe (cutoffKnob.slider, "Filter cutoff", "Set the filter cutoff frequency");
    describe (resonanceKnob.slider, "Filter resonance", "Set the filter feedback resonance");
    describe (filterDriveKnob.slider, "Filter drive", "Drive the selected nonlinear filter model");
    describe (filterShapeKnob.slider, "SEM filter shape", "Sweep the SEM response from low-pass through notch to high-pass");
    describe (filterEnvKnob.slider, "Filter envelope amount", "Set bipolar filter-envelope modulation depth");
    describe (keyTrackKnob.slider, "Filter key tracking", "Track filter cutoff from MIDI note pitch");
    describe (polyphonyKnob.slider, "Maximum polyphony",
              "Limit active render voices from one to sixteen; layered modes use multiple voices per note");
    describe (unisonVoicesKnob.slider, "Unison voice count", "Set voices consumed by each unison note");
    describe (unisonDetuneKnob.slider, "Unison detune",
              "Spread the unison stack in cents, independently of voice-card drift");
    describe (arpRateKnob.slider, "Arpeggiator rate",
              "Set the free-running arpeggiator step rate in steps per second");
    describe (arpOctavesKnob.slider, "Arpeggiator range",
              "Repeat the held pattern over one to four octaves");
    describe (arpGateKnob.slider, "Arpeggiator gate",
              "Set how much of each step the note is held; full gate is legato");
    describe (driftKnob.slider, "Voice-card drift", "Set deterministic component spread and stochastic analogue movement");
    describe (spreadKnob.slider, "Stereo spread", "Spread layered and polyphonic voices across stereo");
    describe (glideKnob.slider, "Glide time", "Set the pitch glide time between notes");
    describe (velocityKnob.slider, "Velocity response", "Set the note-velocity influence on amplifier level");
    describe (lfoRateKnob.slider, "LFO rate", "Set the low-frequency oscillator rate");
    describe (lfoPitchKnob.slider, "LFO pitch depth", "Set low-frequency oscillator pitch modulation in cents");
    describe (lfoFilterKnob.slider, "LFO filter depth", "Set low-frequency oscillator filter modulation in octaves");
    describe (lfoPwmKnob.slider, "LFO pulse-width depth", "Set low-frequency oscillator pulse-width modulation depth");
    describe (chorusMixKnob.slider, "Ensemble mix", "Blend the variable-clock BBD ensemble with the dry signal");
    describe (chorusRateKnob.slider, "Ensemble rate", "Set the analog ensemble modulation rate");
    describe (outputKnob.slider, "Output level", "Set the synthesizer output level in decibels");
    describe (filterAttackFader.slider, "Filter envelope attack", "Set filter envelope attack time");
    describe (filterDecayFader.slider, "Filter envelope decay", "Set filter envelope decay time");
    describe (filterSustainFader.slider, "Filter envelope sustain", "Set filter envelope sustain level");
    describe (filterReleaseFader.slider, "Filter envelope release", "Set filter envelope release time");
    describe (ampAttackFader.slider, "Amplifier envelope attack", "Set amplifier envelope attack time");
    describe (ampDecayFader.slider, "Amplifier envelope decay", "Set amplifier envelope decay time");
    describe (ampSustainFader.slider, "Amplifier envelope sustain", "Set amplifier envelope sustain level");
    describe (ampReleaseFader.slider, "Amplifier envelope release", "Set amplifier envelope release time");

    choiceAttachments.reserve (8);
    sliderAttachments.reserve (40);
    buttonAttachments.reserve (7);

    attachChoice (osc1ModelStrip, mars::parameters::osc1Model);
    attachChoice (osc2ModelStrip, mars::parameters::osc2Model);
    attachChoice (osc1WaveStrip, mars::parameters::osc1Wave);
    attachChoice (osc2WaveStrip, mars::parameters::osc2Wave);
    attachChoice (filterModelStrip, mars::parameters::filterModel);
    attachChoice (lfoWaveStrip, mars::parameters::lfoWave);
    attachChoice (voiceModeStrip, mars::parameters::voiceMode);
    attachChoice (arpModeStrip, mars::parameters::arpMode);
    attachButton (osc1EnableButton, mars::parameters::osc1Enabled);
    attachButton (osc2EnableButton, mars::parameters::osc2Enabled);
    attachButton (oversamplingButton, mars::parameters::hqOversampling);
    attachButton (monoButton, mars::parameters::monoMode);
    attachButton (companderButton, mars::parameters::chorusCompander);
    attachButton (arpEnableButton, mars::parameters::arpEnabled);
    attachButton (arpHoldButton, mars::parameters::arpHold);

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
    attachSlider (polyphonyKnob.slider, mars::parameters::polyphonyLimit);
    attachSlider (unisonVoicesKnob.slider, mars::parameters::unisonVoices);
    attachSlider (unisonDetuneKnob.slider, mars::parameters::unisonDetune);
    attachSlider (arpRateKnob.slider, mars::parameters::arpRate);
    attachSlider (arpOctavesKnob.slider, mars::parameters::arpOctaves);
    attachSlider (arpGateKnob.slider, mars::parameters::arpGate);
    attachSlider (driftKnob.slider, mars::parameters::drift);
    attachSlider (spreadKnob.slider, mars::parameters::spread);
    attachSlider (glideKnob.slider, mars::parameters::glide);
    attachSlider (velocityKnob.slider, mars::parameters::velocity);
    attachSlider (chorusMixKnob.slider, mars::parameters::chorusMix);
    attachSlider (chorusRateKnob.slider, mars::parameters::chorusRate);
    attachSlider (outputKnob.slider, mars::parameters::output);

    setResizable (true, true);
    setResizeLimits (1240, 820, 1960, 1260);
    setSize (1400, 900);
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

void MarsAudioProcessorEditor::attachButton (juce::Button& button, const char* parameterId)
{
    jassert (marsProcessor.parameters.getParameter (parameterId) != nullptr);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (
        marsProcessor.parameters, parameterId, button));
}

void MarsAudioProcessorEditor::updateConditionalControls()
{
    constexpr float disabledControlAlpha = 0.62f;
    const auto osc1Enabled = osc1EnableButton.getToggleState();
    const auto osc2Enabled = osc2EnableButton.getToggleState();
    osc1EnableButton.setButtonText (osc1Enabled ? "ON" : "OFF");
    osc2EnableButton.setButtonText (osc2Enabled ? "ON" : "OFF");
    const auto hqRequested = oversamplingButton.getToggleState();
    const auto effectiveFactor = marsProcessor.getEffectiveOversamplingFactor();
    const auto hostRate = marsProcessor.getCurrentSampleRateForDisplay();
    int requestedFactor = 1;
    while (hqRequested && requestedFactor < 4 && hostRate > 0.0
           && hostRate * static_cast<double> (requestedFactor) < 176400.0)
        requestedFactor *= 2;
    if (hostRate <= 0.0 && hqRequested)
        oversamplingButton.setButtonText ("HQ ON");
    else if (hostRate > 0.0 && effectiveFactor != requestedFactor)
        oversamplingButton.setButtonText ("HQ WAIT");
    else if (! hqRequested)
        oversamplingButton.setButtonText ("HQ OFF");
    else if (effectiveFactor > 1)
        oversamplingButton.setButtonText ("HQ " + juce::String (effectiveFactor) + "X");
    else
        oversamplingButton.setButtonText ("HQ NATIVE");
    oversamplingButton.setTitle ("High-quality oversampling: "
                                 + oversamplingButton.getButtonText());
    oversamplingButton.setDescription (
        "Use rate-aware 4X, 2X, or native processing for the nonlinear voice and BBD ensemble; "
        "current status " + oversamplingButton.getButtonText() + "; changes wait for silence");
    oversamplingButton.setTooltip (oversamplingButton.getDescription());

    const auto bothOscillatorsEnabled = osc1Enabled && osc2Enabled;
    oscMixKnob.setEnabled (bothOscillatorsEnabled);
    oscMixKnob.setAlpha (bothOscillatorsEnabled ? 1.0f : disabledControlAlpha);

    const auto monoMode = monoButton.getToggleState();
    voiceModeStrip.setEnabled (! monoMode);
    voiceModeStrip.setAlpha (monoMode ? disabledControlAlpha : 1.0f);

    polyphonyKnob.setEnabled (! monoMode);
    polyphonyKnob.setAlpha (monoMode ? disabledControlAlpha : 1.0f);

    const auto unisonMode = ! monoMode && voiceModeStrip.getSelectedIndex() == 1;
    unisonVoicesKnob.setEnabled (unisonMode);
    unisonVoicesKnob.setAlpha (unisonMode ? 1.0f : disabledControlAlpha);
    unisonDetuneKnob.setEnabled (unisonMode);
    unisonDetuneKnob.setAlpha (unisonMode ? 1.0f : disabledControlAlpha);

    const auto arpRunning = arpEnableButton.getToggleState();
    for (auto* component : { static_cast<juce::Component*> (&arpModeStrip),
                             static_cast<juce::Component*> (&arpRateKnob),
                             static_cast<juce::Component*> (&arpOctavesKnob),
                             static_cast<juce::Component*> (&arpGateKnob),
                             static_cast<juce::Component*> (&arpHoldButton) })
    {
        component->setEnabled (arpRunning);
        component->setAlpha (arpRunning ? 1.0f : disabledControlAlpha);
    }
    arpEnableButton.setButtonText (arpRunning ? "ARP ON" : "ARP");

    const auto semFilter = filterModelStrip.getSelectedIndex() == 1;
    filterShapeKnob.setEnabled (semFilter);
    filterShapeKnob.setAlpha (semFilter ? 1.0f : disabledControlAlpha);

    const auto pulseActive = osc1WaveStrip.getSelectedIndex() == 1
                          || osc2WaveStrip.getSelectedIndex() == 1;
    pulseWidthKnob.setEnabled (pulseActive);
    pulseWidthKnob.setAlpha (pulseActive ? 1.0f : disabledControlAlpha);
    lfoPwmKnob.setEnabled (pulseActive);
    lfoPwmKnob.setAlpha (pulseActive ? 1.0f : disabledControlAlpha);
}

void MarsAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (marsProcessor.getActiveVoiceCount(), marsProcessor.isEngineReady(),
                             marsProcessor.getCurrentSampleRateForDisplay());
    // A plain literal table avoids a function-local static JUCE object that
    // would outlive the library's own shutdown.
    static constexpr std::array<const char*, 5> arpModeNames {
        "UP", "DOWN", "UP-DN", "RAND", "PLAY"
    };
    const auto modeIndex = juce::jlimit (0, static_cast<int> (arpModeNames.size()) - 1,
                                         arpModeStrip.getSelectedIndex());
    scopeDisplay.refresh (marsProcessor, arpEnableButton.getToggleState(),
                          juce::String (arpModeNames[static_cast<size_t> (modeIndex)]));
    updateConditionalControls();
}

void MarsAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto fullBounds = getLocalBounds().toFloat();
    g.fillAll (c (coal));
    drawChassisSurface (g, fullBounds);

    juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                   fullBounds.getCentreX(), fullBounds.getCentreY(),
                                   c (coal).withAlpha (0.72f), fullBounds.getRight(),
                                   fullBounds.getBottom(), true);
    vignette.addColour (0.72, c (coal).withAlpha (0.10f));
    g.setGradientFill (vignette);
    g.fillRect (fullBounds);

    const auto outerFrame = fullBounds.reduced (5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.drawRoundedRectangle (outerFrame.translated (0.0f, 2.0f), 7.0f, 5.0f);
    g.setColour (c (0xff0b0f10));
    g.drawRoundedRectangle (outerFrame, 7.0f, 4.0f);
    g.setColour (c (0xff4b5557));
    g.drawRoundedRectangle (outerFrame.reduced (2.0f), 6.0f, 1.5f);
    g.setColour (c (accentBlueBright).withAlpha (0.34f));
    g.drawRoundedRectangle (outerFrame.reduced (4.0f), 5.0f, 0.8f);

    const auto headerHeight = juce::jlimit (58, 74, getHeight() / 11);
    auto headerPlate = juce::Rectangle<float> (20.0f, 10.0f,
                                               static_cast<float> (getWidth() - 40),
                                               static_cast<float> (headerHeight - 17));
    juce::ColourGradient headerGradient (c (0xff292f31).withAlpha (0.94f),
                                          headerPlate.getTopLeft(),
                                          c (0xff151a1b).withAlpha (0.96f),
                                          headerPlate.getBottomLeft(), false);
    g.setGradientFill (headerGradient);
    g.fillRoundedRectangle (headerPlate, 4.0f);
    g.setColour (c (panelEdge).withAlpha (0.74f));
    g.drawRoundedRectangle (headerPlate, 4.0f, 1.0f);
    const auto headerRailX = 22.0f;
    const auto headerRailY = static_cast<float> (headerHeight - 2);
    const auto headerRailWidth = static_cast<float> (getWidth() - 44);
    const auto blueWidth = headerRailWidth * 0.48f;
    const auto redWidth = headerRailWidth * 0.30f;
    g.setColour (c (accentBlue).withAlpha (0.92f));
    g.fillRect (headerRailX, headerRailY, blueWidth, 1.5f);
    g.setColour (c (signalRed).withAlpha (0.92f));
    g.fillRect (headerRailX + blueWidth, headerRailY, redWidth, 1.5f);
    g.setColour (c (accentAmber).withAlpha (0.92f));
    g.fillRect (headerRailX + blueWidth + redWidth, headerRailY,
                headerRailWidth - blueWidth - redWidth, 1.5f);

    static constexpr std::array<const char*, sectionCount> names {
        "OSC I", "OSC II", "MIX", "VCF", "ARPEGGIATOR", "LFO / VOICE",
        "FILTER ENV", "AMP ENV", "ENSEMBLE / MASTER"
    };
    static constexpr std::array<juce::uint32, sectionCount> sectionAccents {
        accentOrange, accentOrange, accentBlue, signalRed, accentAmber,
        accentBlue, accentOrange, signalRed, accentBlue
    };

    for (int i = 0; i < sectionCount; ++i)
    {
        const auto bounds = sectionBounds[static_cast<size_t> (i)].toFloat();
        if (bounds.isEmpty())
            continue;

        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 6.0f);
        juce::ColourGradient panelGradient (c (0xff252a2b), bounds.getTopLeft(), c (panel),
                                            bounds.getBottomLeft(), false);
        panelGradient.addColour (0.32, c (0xff202526));
        g.setGradientFill (panelGradient);
        g.fillRoundedRectangle (bounds, 6.0f);

        juce::ColourGradient sectionHeaderGradient (c (0xff353c3e), bounds.getTopLeft(),
                                                    c (panelRaised),
                                                    { bounds.getX(), bounds.getY() + 27.0f }, false);
        g.setGradientFill (sectionHeaderGradient);
        g.fillRoundedRectangle (bounds.reduced (2.0f).withHeight (26.0f), 4.0f);
        g.setColour (c (panelEdge));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.15f);
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.drawRoundedRectangle (bounds.reduced (2.5f), 4.5f, 0.8f);
        g.setColour (c (sectionAccents[static_cast<size_t> (i)]).withAlpha (0.92f));
        g.fillRect (bounds.getX() + 9.0f, bounds.getY() + 25.0f,
                    juce::jmax (22.0f, bounds.getWidth() * 0.22f), 1.5f);
        g.setColour (c (textBright));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (names[static_cast<size_t> (i)],
                    sectionBounds[static_cast<size_t> (i)].reduced (10, 0).removeFromTop (25),
                    juce::Justification::centredLeft);

        for (const auto corner : { bounds.getTopLeft(), bounds.getTopRight(),
                                   bounds.getBottomLeft(), bounds.getBottomRight() })
        {
            g.setColour (c (0xff7d8785));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (
                corner + juce::Point<float> { corner.x < bounds.getCentreX() ? 7.0f : -7.0f,
                                               corner.y < bounds.getCentreY() ? 7.0f : -7.0f }));
        }
    }
}

void MarsAudioProcessorEditor::resized()
{
    const auto panelGap = juce::jlimit (10, 12, getWidth() / 115);
    const auto controlRowGap = juce::jlimit (6, 8, getHeight() / 125);
    const auto choiceToControlsGap = juce::jlimit (5, 6, getHeight() / 150);
    const auto controlColumnGap = juce::jlimit (7, 8, getWidth() / 175);
    const auto faderColumnGap = juce::jlimit (4, 5, getWidth() / 280);

    const auto headerHeight = juce::jlimit (58, 74, getHeight() / 11);
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (24, 8);
    logoLabel.setBounds (header.removeFromLeft (140));
    header.removeFromLeft (12);
    panicButton.setBounds (header.removeFromRight (72).reduced (1, 6));
    header.removeFromRight (8);
    oversamplingButton.setBounds (header.removeFromRight (82).reduced (0, 6));
    header.removeFromRight (8);
    statusDisplay.setBounds (
        header.removeFromRight (juce::jlimit (170, 190, getWidth() / 8)).reduced (0, 5));
    header.removeFromRight (10);

    auto randomizerArea = header.removeFromRight (
        juce::jlimit (206, 240, getWidth() / 6));
    randomizerLabel.setBounds (randomizerArea.removeFromTop (15));
    randomizerArea.removeFromTop (2);
    distributeHorizontally (randomizerArea.reduced (0, 3),
                            { &randomize1Button, &randomize10Button, &randomize100Button }, 5);
    header.removeFromRight (12);
    // The freed header span becomes the panel scope. The edition legend moves
    // to the keyboard caption row below.
    scopeDisplay.setBounds (header.reduced (0, 1));

    auto body = getLocalBounds();
    body.removeFromTop (headerHeight);
    body.reduce (24, 10);
    const auto keyboardHeight = juce::jlimit (125, 190, getHeight() * 19 / 100);
    auto keyboardArea = body.removeFromBottom (keyboardHeight);
    auto captionRow = keyboardArea.removeFromTop (20);
    editionLabel.setBounds (captionRow.removeFromRight (
        juce::jlimit (300, 420, captionRow.getWidth() / 3)));
    keyboardHintLabel.setBounds (captionRow);
    keyboardArea.removeFromTop (2);
    keyboard.setBounds (keyboardArea.reduced (0, 2));
    // MIDI 0..127 contains 75 white keys. Keep useful-sized keys at normal editor
    // sizes, but expand them when the full range would otherwise leave a blank tail.
    keyboard.setKeyWidth (juce::jmax (23.0f,
                                      static_cast<float> (keyboard.getWidth()) / 75.0f));
    body.removeFromBottom (panelGap);

    auto rowOne = body.removeFromTop ((body.getHeight() - panelGap) / 2);
    body.removeFromTop (panelGap);
    auto rowTwo = body;

    const auto rowOneAvailable = rowOne.getWidth() - panelGap * 4;
    const auto osc1Width = rowOneAvailable * 13 / 100;
    const auto osc2Width = rowOneAvailable * 18 / 100;
    const auto mixerWidth = rowOneAvailable * 21 / 100;
    const auto filterWidth = rowOneAvailable * 28 / 100;

    sectionBounds[oscillator1Section] = rowOne.removeFromLeft (osc1Width);
    rowOne.removeFromLeft (panelGap);
    sectionBounds[oscillator2Section] = rowOne.removeFromLeft (osc2Width);
    rowOne.removeFromLeft (panelGap);
    sectionBounds[mixerSection] = rowOne.removeFromLeft (mixerWidth);
    rowOne.removeFromLeft (panelGap);
    sectionBounds[filterSection] = rowOne.removeFromLeft (filterWidth);
    rowOne.removeFromLeft (panelGap);
    sectionBounds[arpeggiatorSection] = rowOne;

    const auto rowTwoAvailable = rowTwo.getWidth() - panelGap * 3;
    const auto lfoVoiceWidth = rowTwoAvailable * 40 / 100;
    const auto envelopeWidth = rowTwoAvailable * 19 / 100;
    sectionBounds[lfoVoiceSection] = rowTwo.removeFromLeft (lfoVoiceWidth);
    rowTwo.removeFromLeft (panelGap);
    sectionBounds[filterEnvelopeSection] = rowTwo.removeFromLeft (envelopeWidth);
    rowTwo.removeFromLeft (panelGap);
    sectionBounds[ampEnvelopeSection] = rowTwo.removeFromLeft (envelopeWidth);
    rowTwo.removeFromLeft (panelGap);
    sectionBounds[masterSection] = rowTwo;

    const auto powerButtonBounds = [this] (Section section)
    {
        auto area = sectionBounds[static_cast<size_t> (section)].reduced (10, 3)
                        .removeFromTop (22);
        return area.removeFromRight (52);
    };
    osc1EnableButton.setBounds (powerButtonBounds (oscillator1Section));
    osc2EnableButton.setBounds (powerButtonBounds (oscillator2Section));
    companderButton.setBounds (powerButtonBounds (masterSection));

    {
        auto arpHeaderArea = sectionBounds[arpeggiatorSection].reduced (10, 3)
                                 .removeFromTop (22);
        arpHoldButton.setBounds (arpHeaderArea.removeFromRight (46));
        arpHeaderArea.removeFromRight (4);
        arpEnableButton.setBounds (arpHeaderArea.removeFromRight (58));
    }

    const auto sectionContent = [this] (Section section)
    {
        auto area = sectionBounds[static_cast<size_t> (section)].reduced (12, 10);
        area.removeFromTop (20);
        return area;
    };

    auto osc1 = sectionContent (oscillator1Section);
    const auto osc1StripHeight = juce::jlimit (36, 42, osc1.getHeight() / 5);
    osc1ModelStrip.setBounds (osc1.removeFromTop (osc1StripHeight));
    osc1.removeFromTop (4);
    osc1WaveStrip.setBounds (osc1.removeFromTop (osc1StripHeight));
    osc1.removeFromTop (choiceToControlsGap);
    osc1OctaveKnob.setBounds (osc1.withSizeKeepingCentre (juce::jmin (110, osc1.getWidth()),
                                                         osc1.getHeight()));

    auto osc2 = sectionContent (oscillator2Section);
    const auto osc2StripHeight = juce::jlimit (36, 42, osc2.getHeight() / 5);
    osc2ModelStrip.setBounds (osc2.removeFromTop (osc2StripHeight));
    osc2.removeFromTop (4);
    osc2WaveStrip.setBounds (osc2.removeFromTop (osc2StripHeight));
    osc2.removeFromTop (choiceToControlsGap);
    distributeHorizontally (osc2, { &osc2OctaveKnob, &osc2TuneKnob, &osc2FineKnob },
                            controlColumnGap);

    auto mixer = sectionContent (mixerSection);
    auto mixerTop = mixer.removeFromTop ((mixer.getHeight() - controlRowGap) / 2);
    mixer.removeFromTop (controlRowGap);
    distributeHorizontally (mixerTop, { &oscMixKnob, &pulseWidthKnob, &crossModKnob },
                            controlColumnGap);
    distributeHorizontally (mixer, { &subLevelKnob, &noiseLevelKnob }, controlColumnGap);

    auto filter = sectionContent (filterSection);
    filterModelStrip.setBounds (filter.removeFromTop (juce::jlimit (38, 45, filter.getHeight() / 5)));
    filter.removeFromTop (choiceToControlsGap);
    auto filterTop = filter.removeFromTop ((filter.getHeight() - controlRowGap) / 2);
    filter.removeFromTop (controlRowGap);
    distributeHorizontally (filterTop, { &cutoffKnob, &resonanceKnob, &filterDriveKnob },
                            controlColumnGap);
    distributeHorizontally (filter, { &filterShapeKnob, &filterEnvKnob, &keyTrackKnob },
                            controlColumnGap);

    auto arp = sectionContent (arpeggiatorSection);
    arpModeStrip.setBounds (arp.removeFromTop (juce::jlimit (38, 45, arp.getHeight() / 5)));
    arp.removeFromTop (choiceToControlsGap);
    distributeHorizontally (arp, { &arpRateKnob, &arpOctavesKnob, &arpGateKnob },
                            controlColumnGap);

    auto lfoVoice = sectionContent (lfoVoiceSection);
    // The voice half now carries four columns against the LFO half's two.
    auto lfo = lfoVoice.removeFromLeft ((lfoVoice.getWidth() - panelGap) * 2 / 5);
    lfoVoice.removeFromLeft (panelGap);
    auto voice = lfoVoice;

    lfoWaveStrip.setBounds (lfo.removeFromTop (juce::jlimit (38, 45, lfo.getHeight() / 5)));
    lfo.removeFromTop (choiceToControlsGap);
    auto lfoTop = lfo.removeFromTop ((lfo.getHeight() - controlRowGap) / 2);
    lfo.removeFromTop (controlRowGap);
    distributeHorizontally (lfoTop, { &lfoRateKnob, &lfoPitchKnob }, controlColumnGap);
    distributeHorizontally (lfo, { &lfoFilterKnob, &lfoPwmKnob }, controlColumnGap);

    auto voiceModeArea = voice.removeFromTop (juce::jlimit (38, 45, voice.getHeight() / 5));
    auto monoArea = voiceModeArea.removeFromRight (juce::jlimit (54, 64, voiceModeArea.getWidth() / 4));
    voiceModeArea.removeFromRight (6);
    voiceModeStrip.setBounds (voiceModeArea);
    monoArea.removeFromTop (15);
    monoButton.setBounds (monoArea);
    voice.removeFromTop (choiceToControlsGap);
    auto voiceTop = voice.removeFromTop ((voice.getHeight() - controlRowGap) / 2);
    voice.removeFromTop (controlRowGap);
    // Four columns above, three below: the fixed grid keeps both rows aligned
    // now that unison detune is a control of its own.
    distributeInColumns (voiceTop,
                         { &polyphonyKnob, &unisonVoicesKnob, &unisonDetuneKnob, &driftKnob },
                         4, controlColumnGap);
    distributeInColumns (voice, { &spreadKnob, &glideKnob, &velocityKnob },
                         4, controlColumnGap);

    distributeHorizontally (sectionContent (filterEnvelopeSection),
                            { &filterAttackFader, &filterDecayFader,
                              &filterSustainFader, &filterReleaseFader }, faderColumnGap);
    distributeHorizontally (sectionContent (ampEnvelopeSection),
                            { &ampAttackFader, &ampDecayFader,
                              &ampSustainFader, &ampReleaseFader }, faderColumnGap);

    distributeHorizontally (sectionContent (masterSection),
                            { &chorusMixKnob, &chorusRateKnob, &outputKnob },
                            controlColumnGap);
}

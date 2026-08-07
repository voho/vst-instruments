#include "PluginEditor.h"

#include <BinaryData.h>

#include <cmath>
#include <cstring>
#include <utility>

namespace
{
using namespace youknow106;

juce::Colour fromPalette (std::uint32_t rgb) noexcept
{
    return juce::Colour (static_cast<juce::uint8> ((rgb >> 16) & 0xffu),
                         static_cast<juce::uint8> ((rgb >> 8) & 0xffu),
                         static_cast<juce::uint8> (rgb & 0xffu));
}

juce::Colour accentColour (panel::Accent accent) noexcept
{
    return fromPalette (accent == panel::Accent::Magenta ? panel::colour::magenta
                                                         : panel::colour::cyan);
}

juce::Font panelFont (float height, bool bold = false)
{
    auto font = juce::Font (juce::FontOptions (height, bold ? juce::Font::bold
                                                            : juce::Font::plain));
    font.setHorizontalScale (panel::typefaceHorizontalScale);
    return font;
}

constexpr auto compactStyleProperty = "compactStyle";
constexpr auto secondaryStyleProperty = "secondaryStyle";

// Every enclosing UI surface uses this same quiet neutral frame. Section
// colours remain as thin header rules, where they communicate signal role,
// instead of turning adjacent cards into a collection of unrelated borders.
constexpr float surfaceCornerRadius = 4.0f;
constexpr float surfaceBorderWidth = 1.0f;

juce::Colour surfaceBorderColour() noexcept
{
    return fromPalette (panel::colour::textDim).withAlpha (0.28f);
}

void drawSurfaceBorder (juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour border, float uiScale = 1.0f)
{
    const float corner = juce::jmax (2.5f, surfaceCornerRadius * uiScale);
    const float stroke = juce::jmax (1.0f, surfaceBorderWidth * uiScale);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (stroke * 0.5f), corner, stroke);
}

void drawFramedSurface (juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour fill, juce::Colour border,
                        float uiScale = 1.0f)
{
    const float corner = juce::jmax (2.5f, surfaceCornerRadius * uiScale);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, corner);
    drawSurfaceBorder (g, bounds, border, uiScale);
}

void drawFramedSurface (juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour fill, float uiScale = 1.0f)
{
    drawFramedSurface (g, bounds, fill, surfaceBorderColour(), uiScale);
}

bool isWaveformLegend (const juce::String& text) noexcept
{
    return text == "PULSE" || text == "SAW";
}

bool isFootRegisterLegend (const juce::String& text) noexcept
{
    return text == "16'" || text == "8'" || text == "4'";
}

void drawWaveformLegend (juce::Graphics& g, juce::Rectangle<float> area,
                         bool pulse)
{
    area = area.reduced (juce::jmax (3.0f, area.getWidth() * 0.16f),
                         juce::jmax (2.0f, area.getHeight() * 0.20f));
    if (area.isEmpty())
        return;

    const float top = area.getY() + area.getHeight() * 0.16f;
    const float bottom = area.getBottom() - area.getHeight() * 0.16f;
    juce::Path path;
    if (pulse)
    {
        // Low -> high -> low, matching the square-wave mark screened on the
        // original DCO panel rather than relying on a platform font symbol.
        path.startNewSubPath (area.getX(), bottom);
        path.lineTo (area.getX() + area.getWidth() * 0.27f, bottom);
        path.lineTo (area.getX() + area.getWidth() * 0.27f, top);
        path.lineTo (area.getX() + area.getWidth() * 0.67f, top);
        path.lineTo (area.getX() + area.getWidth() * 0.67f, bottom);
        path.lineTo (area.getRight(), bottom);
    }
    else
    {
        // Rising ramp with a vertical reset: the JUNO sawtooth faceplate mark.
        path.startNewSubPath (area.getX(), bottom);
        path.lineTo (area.getRight() - area.getWidth() * 0.18f, top);
        path.lineTo (area.getRight() - area.getWidth() * 0.18f, bottom);
        path.lineTo (area.getRight(), bottom);
    }

    const float thickness = juce::jlimit (1.2f, 2.3f, area.getWidth() * 0.045f);
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void drawFootRegisterLegend (juce::Graphics& g, juce::Rectangle<float> area,
                             const juce::String& text)
{
    const auto number = text.dropLastCharacters (1);
    const float fontHeight = juce::jlimit (10.0f, 13.0f, area.getHeight() * 0.38f);
    const auto font = panelFont (fontHeight, true);
    const float numberWidth = juce::GlyphArrangement::getStringWidth (font, number);
    const float primeWidth = juce::jmax (2.5f, fontHeight * 0.25f);
    const float groupWidth = numberWidth + primeWidth;
    const float left = area.getCentreX() - groupWidth * 0.5f;
    const float baselineTop = area.getCentreY() - fontHeight * 0.5f;

    g.setFont (font);
    g.drawText (number,
                juce::Rectangle<float> (left, baselineTop, numberWidth + 1.0f,
                                        fontHeight + 2.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
    const float primeX = left + numberWidth + 1.0f;
    g.drawLine (primeX + primeWidth * 0.55f, baselineTop + 1.0f,
                primeX, baselineTop + fontHeight * 0.38f,
                juce::jmax (1.2f, fontHeight * 0.10f));
}
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

YouKnow106LookAndFeel::YouKnow106LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::Label::textColourId, fromPalette (panel::colour::text));
    setColour (juce::TooltipWindow::backgroundColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::TooltipWindow::textColourId, fromPalette (panel::colour::text));

    // The patch list. Without these the menu falls back to the base class's own
    // dark scheme, which is readable but is not this panel's palette -- and the
    // list is the main way to get at all 129 programs, so it is not a corner
    // of the interface. The highlight is the section cyan, which is bright
    // enough that the text on it has to go dark rather than stay light.
    setColour (juce::PopupMenu::backgroundColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::PopupMenu::textColourId, fromPalette (panel::colour::text));
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               fromPalette (panel::colour::cyan).withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::PopupMenu::headerTextColourId,
               fromPalette (panel::colour::textDim));

    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
               fromPalette (panel::colour::control));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
               fromPalette (panel::colour::faceplate));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               fromPalette (panel::colour::magenta).withAlpha (0.85f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               fromPalette (panel::colour::cyan).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::shadowColourId,
               juce::Colours::black.withAlpha (0.6f));
}

void YouKnow106LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
                                              int width, int height, float sliderPos,
                                              float, float,
                                              juce::Slider::SliderStyle style,
                                              juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          0.0f, 0.0f, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height));

    // A deep routed well, with a narrow metal rail visible inside it. The
    // unequal edge lighting makes it read as a physical cut-out without
    // copying a particular vintage cap or faceplate moulding.
    const float wellWidth = juce::jlimit (7.0f, 12.0f,
                                          bounds.getWidth() * 0.27f);
    const auto well = bounds.withSizeKeepingCentre (wellWidth, bounds.getHeight())
                            .reduced (0.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (well.expanded (1.8f).translated (0.0f, 1.2f),
                            wellWidth * 0.46f);
    juce::ColourGradient wellGradient (
        juce::Colours::black.withAlpha (0.92f), well.getX(), well.getCentreY(),
        fromPalette (panel::colour::slot), well.getRight(), well.getCentreY(),
        false);
    wellGradient.addColour (0.46, fromPalette (panel::colour::slot).darker (0.35f));
    g.setGradientFill (wellGradient);
    g.fillRoundedRectangle (well, wellWidth * 0.42f);
    g.setColour (juce::Colours::white.withAlpha (0.075f));
    g.drawLine (well.getX() + 1.0f, well.getY() + 3.0f,
                well.getX() + 1.0f, well.getBottom() - 3.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.82f));
    g.drawLine (well.getRight() - 1.0f, well.getY() + 3.0f,
                well.getRight() - 1.0f, well.getBottom() - 3.0f, 1.0f);

    const auto rail = well.withSizeKeepingCentre (juce::jmax (2.0f,
                                                               wellWidth * 0.24f),
                                                   well.getHeight() - 5.0f);
    g.setColour (fromPalette (panel::colour::controlShadow).withAlpha (0.30f));
    g.fillRoundedRectangle (rail, rail.getWidth() * 0.5f);

    // Seven ticks are enough to read travel at a glance. Keeping only the
    // endpoints and centre strong prevents forty adjacent faders from turning
    // their scales into a wall of dashes.
    for (int tick = 0; tick <= 6; ++tick)
    {
        const float t = bounds.getY() + 3.0f
                      + (bounds.getHeight() - 6.0f) * static_cast<float> (tick) / 6.0f;
        const bool major = tick == 0 || tick == 3 || tick == 6;
        const float length = major ? 7.0f : 3.5f;
        g.setColour (fromPalette (panel::colour::textDim)
                         .withAlpha (major ? 0.38f : 0.20f));
        g.fillRect (bounds.getX() + 1.0f, t, length, 1.0f);
        g.fillRect (bounds.getRight() - 1.0f - length, t, length, 1.0f);
    }

    // A bevelled phenolic-style cap with shallow grip grooves. It has the
    // reassuring mass of an early-eighties control while the cool alloy colour
    // and green witness notch remain specific to this console.
    const float capHeight = juce::jlimit (12.0f, 17.0f,
                                          bounds.getHeight() * 0.105f);
    const float capWidth = juce::jmax (12.0f,
                                       juce::jmin (36.0f, bounds.getWidth() - 2.0f));
    const auto cap = juce::Rectangle<float> (capWidth, capHeight)
                         .withCentre ({ bounds.getCentreX(),
                                        juce::jlimit (bounds.getY() + capHeight * 0.5f,
                                                      bounds.getBottom() - capHeight * 0.5f,
                                                      sliderPos) });

    g.setColour (juce::Colours::black.withAlpha (0.68f));
    g.fillRoundedRectangle (cap.expanded (1.2f).translated (0.0f, 2.0f), 2.2f);

    g.setColour (fromPalette (panel::colour::controlShadow).darker (0.30f));
    g.fillRoundedRectangle (cap, 2.1f);

    const auto face = cap.reduced (1.2f);
    juce::ColourGradient gradient (fromPalette (panel::colour::control).brighter (0.15f),
                                   face.getX(), face.getY(),
                                   fromPalette (panel::colour::controlShadow).darker (0.12f),
                                   face.getX(), face.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (face, 1.5f);

    g.setColour (surfaceBorderColour().withMultipliedAlpha (
        slider.isMouseOverOrDragging() ? 1.75f : 1.0f));
    g.drawRoundedRectangle (cap.reduced (0.45f), 2.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (face.getX() + 2.0f, face.getY() + 1.0f,
                face.getRight() - 2.0f, face.getY() + 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    for (const float fraction : { 0.24f, 0.76f })
    {
        const float grooveX = face.getX() + face.getWidth() * fraction;
        g.drawLine (grooveX, face.getY() + 3.0f,
                    grooveX, face.getBottom() - 3.0f, 1.0f);
    }

    // The witness line the eye actually reads the value from.
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.92f));
    g.fillRect (face.getX() + 1.0f, face.getCentreY() - 0.9f,
                face.getWidth() - 2.0f, 1.8f);
    g.setColour (fromPalette (panel::colour::led).withAlpha (0.82f));
    g.fillRect (face.getCentreX() - 3.5f, face.getCentreY() - 0.9f,
                7.0f, 1.8f);
}

void YouKnow106LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPosProportional,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                          static_cast<float> (y),
                                          static_cast<float> (width),
                                          static_cast<float> (height));
    const bool secondary = static_cast<bool> (
        slider.getProperties().getWithDefault (secondaryStyleProperty, false));
    const bool active = slider.isMouseOverOrDragging();
    const float diameter = juce::jmax (8.0f,
                                       juce::jmin (bounds.getWidth(), bounds.getHeight())
                                           - (secondary ? 16.0f : 4.0f));
    const auto well = juce::Rectangle<float> (diameter, diameter)
                          .withCentre (bounds.getCentre());

    // Seven restrained scale ticks keep a 30 px control readable without
    // turning the plugin-only strip into another row of full-size sliders.
    g.setColour (fromPalette (panel::colour::textDim)
                     .withAlpha (secondary ? (active ? 0.40f : 0.30f) : 0.50f));
    for (int tick = 0; tick < 7; ++tick)
    {
        const float proportion = static_cast<float> (tick) / 6.0f;
        const float angle = rotaryStartAngle
                          + proportion * (rotaryEndAngle - rotaryStartAngle);
        const float outerRadius = diameter * 0.54f;
        const float innerRadius = outerRadius - (tick % 3 == 0 ? 3.4f : 2.0f);
        const auto centre = well.getCentre();
        g.drawLine (centre.x + std::sin (angle) * innerRadius,
                    centre.y - std::cos (angle) * innerRadius,
                    centre.x + std::sin (angle) * outerRadius,
                    centre.y - std::cos (angle) * outerRadius,
                    tick % 3 == 0 ? 1.2f : 0.8f);
    }

    // A recessed socket and shallow drop shadow make the knobs read as moulded
    // parts instead of flat circles pasted on the panel. Extension settings use
    // a smaller graphite cap and alloy collar: they remain precise controls but
    // no longer compete with the silver fader caps in the synthesis row.
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillEllipse (well.expanded (1.5f).translated (0.0f, 1.2f));
    g.setColour (fromPalette (panel::colour::slot));
    g.fillEllipse (well.expanded (0.7f));

    const auto collar = well.reduced (secondary ? 1.8f : 2.1f);
    const float collarLift = secondary ? (active ? 0.14f : 0.05f) : 0.0f;
    juce::ColourGradient collarGradient (
        fromPalette (secondary ? panel::colour::controlShadow
                               : panel::colour::control).brighter (collarLift),
        collar.getX(), collar.getY(),
        fromPalette (panel::colour::controlShadow).darker (secondary ? 0.38f : 0.0f),
        collar.getRight(), collar.getBottom(), false);
    g.setGradientFill (collarGradient);
    g.fillEllipse (collar);

    const auto cap = secondary ? collar.reduced (3.8f) : collar;
    juce::ColourGradient capGradient (
        fromPalette (secondary ? panel::colour::faceplateHigh
                               : panel::colour::control).brighter (secondary ? 0.12f
                                                                            : 0.0f),
        cap.getX(), cap.getY(),
        fromPalette (secondary ? panel::colour::slot
                               : panel::colour::controlShadow),
        cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (capGradient);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (
        secondary ? (active ? 0.17f : 0.10f) : 0.20f));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);

    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto centre = cap.getCentre();
    const float pointerInner = diameter * (secondary ? 0.05f : 0.10f);
    const float pointerOuter = diameter * (secondary ? 0.31f : 0.34f);
    g.setColour (fromPalette (secondary ? panel::colour::led
                                       : panel::colour::faceplateLow)
                     .withAlpha (secondary ? (active ? 1.0f : 0.78f) : 1.0f));
    g.drawLine (centre.x + std::sin (angle) * pointerInner,
                centre.y - std::cos (angle) * pointerInner,
                centre.x + std::sin (angle) * pointerOuter,
                centre.y - std::cos (angle) * pointerOuter,
                juce::jmax (1.4f, diameter * (secondary ? 0.055f : 0.075f)));
}

void YouKnow106LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour&,
                                                  bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const bool compact = static_cast<bool> (
        button.getProperties().getWithDefault (compactStyleProperty, false));
    const bool secondary = static_cast<bool> (
        button.getProperties().getWithDefault (secondaryStyleProperty, false));

    // Rectangular inset well, bezel and separately moving key face. The tiny
    // down-state travel and hard lower shadow sell the mechanical interaction
    // without recreating the reference instrument's exact switch moulding.
    g.setColour (juce::Colours::black.withAlpha (secondary ? 0.54f : 0.70f));
    g.fillRoundedRectangle (bounds, 2.8f);
    const auto bezel = bounds.reduced (1.2f);
    g.setColour (fromPalette (panel::colour::slot)
                     .withAlpha (secondary ? 0.84f : 1.0f));
    g.fillRoundedRectangle (bezel, 2.1f);
    g.setColour (surfaceBorderColour().withMultipliedAlpha (
        isHighlighted ? 1.55f : (secondary ? 0.65f : 1.0f)));
    g.drawRoundedRectangle (bezel.reduced (0.5f), 1.8f, 1.0f);

    auto key = bezel.reduced (2.0f, 1.8f);
    key = key.translated (0.0f, isDown ? 1.3f : 0.0f);
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillRoundedRectangle (key.translated (0.0f, isDown ? 0.5f : 1.5f), 1.4f);
    juce::ColourGradient gradient (
        fromPalette (isDown ? panel::colour::faceplateLow
                            : panel::colour::faceplateHigh)
            .brighter (secondary ? 0.0f : 0.05f),
        key.getX(), key.getY(),
        fromPalette (panel::colour::faceplateLow).darker (0.10f),
        key.getX(), key.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (key, 1.4f);

    // The lamp alone reports state. A shared neutral key outline keeps toggles,
    // radio choices and momentary actions in one mechanical family.
    g.setColour (surfaceBorderColour().withMultipliedAlpha (
        isHighlighted ? 1.75f : (secondary ? 0.72f : 1.15f)));
    g.drawRoundedRectangle (key, 1.4f, 0.9f);
    g.setColour (juce::Colours::white.withAlpha (isDown ? 0.05f : 0.12f));
    g.drawLine (key.getX() + 2.0f, key.getY() + 1.0f,
                key.getRight() - 2.0f, key.getY() + 1.0f, 1.0f);

    // Navigation and momentary service actions are compact mechanical keys
    // with no invented status lamp. Stateful panel switches retain a lens.
    if (compact)
        return;

    // A lit button glows; an unlit one still shows its lens, so the panel reads
    // the same whether or not anything is on.
    const float lens = juce::jlimit (4.0f, 7.0f, key.getHeight() * 0.22f);
    const auto led = juce::Rectangle<float> (lens, lens)
                         .withCentre ({ key.getCentreX(),
                                        key.getY() + key.getHeight() * 0.25f });
    g.setColour (juce::Colours::black.withAlpha (0.78f));
    g.fillEllipse (led.expanded (1.3f));
    if (on)
    {
        g.setColour (fromPalette (panel::colour::led)
                         .withAlpha (secondary ? 0.15f : 0.28f));
        g.fillEllipse (led.expanded (lens * (secondary ? 0.65f : 0.95f)));
        g.setColour (fromPalette (panel::colour::led)
                         .withAlpha (secondary ? 0.78f : 1.0f));
    }
    else
    {
        g.setColour (fromPalette (panel::colour::ledDim));
    }
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.48f : 0.10f));
    g.fillEllipse (juce::Rectangle<float> (lens * 0.28f, lens * 0.28f)
                       .withCentre ({ led.getCentreX() - lens * 0.16f,
                                      led.getCentreY() - lens * 0.16f }));
}

void YouKnow106LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto text = button.getButtonText();
    const bool compact = static_cast<bool> (
        button.getProperties().getWithDefault (compactStyleProperty, false));
    const bool secondary = static_cast<bool> (
        button.getProperties().getWithDefault (secondaryStyleProperty, false));
    g.setColour (fromPalette (button.getToggleState() ? panel::colour::text
                                                      : panel::colour::textDim)
                     .withAlpha (secondary ? 0.72f : 1.0f));

    if (isWaveformLegend (text))
    {
        drawWaveformLegend (g, bounds.withTrimmedTop (bounds.getHeight() * 0.40f),
                            text == "PULSE");
        return;
    }

    if (isFootRegisterLegend (text))
    {
        drawFootRegisterLegend (g,
                                bounds.withTrimmedTop (bounds.getHeight() * 0.36f),
                                text);
        return;
    }

    if (compact && (text == "<" || text == ">"))
    {
        const auto area = bounds.reduced (bounds.getWidth() * 0.30f,
                                          bounds.getHeight() * 0.26f);
        juce::Path chevron;
        if (text == "<")
        {
            chevron.startNewSubPath (area.getRight(), area.getY());
            chevron.lineTo (area.getX(), area.getCentreY());
            chevron.lineTo (area.getRight(), area.getBottom());
        }
        else
        {
            chevron.startNewSubPath (area.getX(), area.getY());
            chevron.lineTo (area.getRight(), area.getCentreY());
            chevron.lineTo (area.getX(), area.getBottom());
        }
        g.strokePath (chevron,
                      juce::PathStrokeType (juce::jmax (1.4f,
                                                       bounds.getHeight() * 0.09f),
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
        return;
    }

    if (compact)
    {
        const float size = juce::jlimit (10.0f, 12.0f,
                                         bounds.getHeight() * 0.55f);
        g.setFont (panelFont (size, true));
        g.drawText (text, button.getLocalBounds().reduced (3, 1),
                    juce::Justification::centred, false);
        return;
    }

    // The size comes from the panel description rather than from a local
    // formula, so the legend drawn here is the legend the layout check measured.
    // Bounds are in pixels and the sizing is linear in them, so this yields the
    // panel-unit size already multiplied by the editor's scale.
    g.setFont (panelFont (panel::buttonPointSizeFor (
                              text.toRawUTF8(),
                              bounds.getWidth(), bounds.getHeight()), true));
    // One line, and no horizontal squashing: the size above already guarantees
    // the fit, so anything that did not fit should be visible as a layout bug
    // rather than quietly condensed into place.
    g.drawFittedText (text,
                      button.getLocalBounds().withTrimmedTop (
                          juce::roundToInt (bounds.getHeight() * 0.40f)),
                      juce::Justification::centredTop, 1, 1.0f);
}

void YouKnow106LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                          bool isButtonDown, int buttonX,
                                          int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                                 static_cast<float> (width) - 1.0f,
                                                 static_cast<float> (height) - 1.0f);
    const float uiScale = static_cast<float> (height) / 24.0f;
    drawFramedSurface (
        g, bounds, box.findColour (juce::ComboBox::backgroundColourId),
        box.findColour (juce::ComboBox::outlineColourId), uiScale);

    // A drawn chevron keeps the selector in the same crisp vector language as
    // the adjacent previous/next keys and avoids JUCE's platform-specific
    // filled triangle.
    auto arrowArea = juce::Rectangle<float> (static_cast<float> (buttonX),
                                              static_cast<float> (buttonY),
                                              static_cast<float> (buttonW),
                                              static_cast<float> (buttonH))
                         .reduced (juce::jmax (4.0f, 6.0f * uiScale));
    const auto centre = arrowArea.getCentre();
    const float halfWidth = juce::jmax (3.0f, arrowArea.getWidth() * 0.34f);
    const float halfHeight = juce::jmax (1.8f, arrowArea.getHeight() * 0.20f);
    juce::Path arrow;
    arrow.startNewSubPath (centre.x - halfWidth, centre.y - halfHeight);
    arrow.lineTo (centre.x, centre.y + halfHeight);
    arrow.lineTo (centre.x + halfWidth, centre.y - halfHeight);
    const float arrowAlpha = box.isEnabled()
                           ? (isButtonDown || box.isMouseOver() ? 0.95f : 0.72f)
                           : 0.30f;
    g.setColour (box.findColour (juce::ComboBox::arrowColourId)
                     .withMultipliedAlpha (arrowAlpha));
    g.strokePath (arrow, juce::PathStrokeType (juce::jmax (1.3f, 1.5f * uiScale),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void YouKnow106LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 1.0f);
}

juce::Label* YouKnow106LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setColour (juce::Label::textColourId, fromPalette (panel::colour::text));
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (panelFont (12.0f, true));
    return label;
}

// ---------------------------------------------------------------------------
// Plastic texture
// ---------------------------------------------------------------------------

void PlasticTexture::ensureBuilt (int tileSize)
{
    if (tile.isValid() && tile.getWidth() == tileSize)
        return;

    const auto source = juce::ImageFileFormat::loadFrom (
        BinaryData::usedcharcoalplastic_png,
        static_cast<std::size_t> (BinaryData::usedcharcoalplastic_pngSize));
    if (source.isValid())
    {
        tile = source.rescaled (tileSize, tileSize,
                                juce::Graphics::highResamplingQuality);
        return;
    }

    // A deterministic fallback keeps the editor usable in development builds
    // where the binary-data target was accidentally omitted.
    tile = juce::Image (juce::Image::ARGB, tileSize, tileSize, true);
    juce::Image::BitmapData data (tile, juce::Image::BitmapData::writeOnly);

    // A fixed seed: the moulding is part of the instrument, so it must not
    // change between repaints or between runs.
    juce::Random random (0x106u);
    for (int y = 0; y < tileSize; ++y)
    {
        // A faint horizontal grain from the moulding direction, on top of the
        // speckle. Both are subtle enough to read as a surface rather than
        // as noise.
        const int grain = ((y % 3) == 0) ? 3 : 0;
        for (int x = 0; x < tileSize; ++x)
        {
            const int speckle = random.nextInt (13) - 6;
            const int level = juce::jlimit (0, 255, 128 + speckle + grain);
            const auto alpha = static_cast<juce::uint8> (26);
            data.setPixelColour (x, y, juce::Colour (static_cast<juce::uint8> (level),
                                                     static_cast<juce::uint8> (level),
                                                     static_cast<juce::uint8> (level),
                                                     alpha));
        }
    }
}

void PlasticTexture::fill (juce::Graphics& g, juce::Rectangle<int> area,
                           juce::Colour base) const
{
    g.setColour (base);
    g.fillRect (area);

    if (! tile.isValid())
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area);
    // Preserve the panel palette while letting polished patches, cleaning
    // swirls and fine scratches remain visible. The source is deliberately
    // low-contrast, so this opacity reads as wear rather than a photograph.
    g.setOpacity (0.50f);
    for (int y = area.getY(); y < area.getBottom(); y += tile.getHeight())
        for (int x = area.getX(); x < area.getRight(); x += tile.getWidth())
            g.drawImageAt (tile, x, y);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void YouKnow106Display::refresh (const YouKnow106AudioProcessor& source)
{
    const int mask = source.getVoiceMaskForDisplay();
    const int count = source.getActiveVoiceCount();
    const int limit = source.getVoiceLimitForDisplay();
    const float env = source.getEnvelopeForDisplay();
    const float modulation = source.getLfoForDisplay();
    const double rate = source.getCurrentSampleRateForDisplay();
    const int factor = source.getOversamplingFactorForDisplay();
    const bool isReady = source.isEngineReady();
    const float temp = source.getTemperatureForDisplay();
    const float droop = source.getRailDroopForDisplay();

    source.getOscilloscopeBuffer (scopeBuffer);

    voiceMask = mask;
    voices = count;
    voiceLimit = limit;
    envelope = env;
    lfo = modulation;
    sampleRate = rate;
    oversampling = factor;
    ready = isReady;
    temperature = temp;
    railDroop = droop;
    repaint();
}

void YouKnow106Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float uiScale = static_cast<float> (getHeight()) / panel::mastheadHeight;
    drawFramedSurface (g, bounds, fromPalette (panel::colour::faceplateLow),
                       uiScale);

    auto area = bounds.reduced (8.0f, 6.0f);

    // Left column carries the three text-and-bar readouts, right column the
    // scope. The scope gets the larger share and the full height: a trace is
    // only legible if it has room in both axes, and the telemetry it used to
    // sit under has been laid out horizontally in the left column instead.
    const float rightWidth = area.getWidth() * 0.52f;
    auto rightBox = area.removeFromRight (rightWidth);
    area.removeFromRight (8.0f);

    // --- Left Section: voice lamps, meters, telemetry ---
    const float rowHeight = area.getHeight() / 3.0f;

    // Voice indicators
    auto voiceRow = area.removeFromTop (rowHeight);
    const auto readout = voiceRow.removeFromRight (voiceRow.getWidth() * 0.42f);
    const int lamps =
        juce::jlimit (1, youknow106::YouKnow106Engine::maxVoices, voiceLimit);
    const float pitch = voiceRow.getWidth() / static_cast<float> (lamps);
    const float lampSize = juce::jmin (9.0f, voiceRow.getHeight() * 0.7f, pitch * 0.62f);
    for (int voice = 0; voice < lamps; ++voice)
    {
        const auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                              .withCentre ({ voiceRow.getX()
                                                 + (static_cast<float> (voice) + 0.5f) * pitch,
                                             voiceRow.getCentreY() });
        const bool lit = (voiceMask & (1 << voice)) != 0;
        if (lit)
        {
            g.setColour (fromPalette (panel::colour::led).withAlpha (0.28f));
            g.fillEllipse (lamp.expanded (lampSize * 0.6f));
        }
        g.setColour (fromPalette (lit ? panel::colour::led : panel::colour::ledDim));
        g.fillEllipse (lamp);
    }

    g.setColour (fromPalette (panel::colour::textDim));
    g.setFont (panelFont (10.0f));
    g.drawText (ready ? juce::String (voices) + " / " + juce::String (lamps) + " VOICES"
                      : juce::String ("STANDBY"),
                readout.toNearestInt(), juce::Justification::centredRight);

    // Modulation and envelope meters.
    const auto meter = [&g] (juce::Rectangle<float> row, float value, bool bipolar,
                             std::uint32_t tint, const char* caption)
    {
        auto labelArea = row.removeFromLeft (28.0f);
        g.setColour (fromPalette (panel::colour::textDim));
        g.setFont (panelFont (9.5f, true));
        g.drawText (caption, labelArea.toNearestInt(), juce::Justification::centredLeft);

        const auto track = row.reduced (0.0f, row.getHeight() * 0.33f);
        g.setColour (fromPalette (panel::colour::slot));
        g.fillRoundedRectangle (track, 1.5f);

        g.setColour (fromPalette (tint));
        if (bipolar)
        {
            const float centre = track.getCentreX();
            const float span = track.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, value);
            g.fillRoundedRectangle (juce::Rectangle<float> (
                                        juce::jmin (centre, centre + span), track.getY(),
                                        std::abs (span), track.getHeight()), 1.5f);
        }
        else
        {
            g.fillRoundedRectangle (juce::Rectangle<float> (
                                        track.getX(), track.getY(),
                                        track.getWidth() * juce::jlimit (0.0f, 1.0f, value),
                                        track.getHeight()), 1.5f);
        }
    };

    // LFO and ENV share one row side by side rather than stacking, which is
    // what frees the third row for the telemetry the scope used to carry.
    auto meterRow = area.removeFromTop (rowHeight);
    auto lfoCell = meterRow.removeFromLeft (meterRow.getWidth() * 0.5f);
    lfoCell.removeFromRight (6.0f);
    meter (lfoCell, lfo, true, panel::colour::magenta, "LFO");
    meter (meterRow, envelope, false, panel::colour::led, "ENV");

    // Telemetry: unit warmup temperature and PSU rail voltage, on one line
    // across the full column width instead of stacked above the trace.
    g.setFont (panelFont (9.0f, true));
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.90f));
    g.drawText (juce::String (temperature, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")) + "C",
                area.toNearestInt(), juce::Justification::centredLeft);

    g.setColour (fromPalette (panel::colour::textDim));
    const float railV = 15.0f - railDroop;
    g.drawText (juce::String (railV, 2) + "V RAIL", area.toNearestInt(),
                juce::Justification::centredRight);

    // --- Right Section: real-time oscilloscope, full height ---
    drawFramedSurface (g, rightBox, fromPalette (panel::colour::faceplateHigh),
                       uiScale);

    // Oscilloscope CRT Screen
    const auto screen = rightBox.reduced (3.0f, 2.0f);
    g.setColour (fromPalette (panel::colour::scope));
    g.fillRoundedRectangle (screen, 2.0f);

    // Scope grid
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.12f));
    g.drawHorizontalLine (juce::roundToInt (screen.getCentreY()), screen.getX(), screen.getRight());
    g.drawVerticalLine (juce::roundToInt (screen.getCentreX()), screen.getY(), screen.getBottom());

    constexpr std::size_t numScopePoints = 128;

    // Vertical range. The instrument's own output convention puts an ordinary
    // patch near a tenth of full scale, so a fixed plus-or-minus-one trace is a
    // flat line for almost everything it plays. Track the peak, attack fast and
    // release slowly so the picture does not breathe on every note, and quantise
    // the result to a power-of-two ladder that is printed on the screen -- a
    // scope whose gain moves silently is not telling the truth about level.
    float peak = 0.0f;
    for (const float sample : scopeBuffer)
        peak = juce::jmax (peak, std::abs (sample));
    scopePeak = peak > scopePeak ? peak : scopePeak + (peak - scopePeak) * 0.08f;

    float wantedGain = 1.0f;
    while (wantedGain < 32.0f && scopePeak * wantedGain < 0.42f)
        wantedGain *= 2.0f;
    while (wantedGain > 1.0f && scopePeak * wantedGain > 0.95f)
        wantedGain *= 0.5f;
    scopeGain = wantedGain;

    // Trigger on the rising edge through zero, with a hysteresis band scaled to
    // the trace itself so a near-silent buffer does not latch onto its own
    // dither and jitter the picture from frame to frame.
    const float hysteresis = juce::jmax (1.0e-4f, scopePeak * 0.06f);
    std::size_t triggerIdx = 0;
    bool armed = false;
    for (std::size_t i = 0; i < scopeBuffer.size() - numScopePoints; ++i)
    {
        if (scopeBuffer[i] < -hysteresis)
            armed = true;
        else if (armed && scopeBuffer[i] >= 0.0f)
        {
            triggerIdx = i;
            break;
        }
    }

    juce::Path wavePath;
    const float stepX = screen.getWidth() / static_cast<float> (numScopePoints - 1);
    const float centreY = screen.getCentreY();
    const float halfHeight = screen.getHeight() * 0.44f;
    for (std::size_t i = 0; i < numScopePoints; ++i)
    {
        const float sampleVal = scopeBuffer[(triggerIdx + i) % scopeBuffer.size()]
                              * scopeGain;
        const float px = screen.getX() + static_cast<float> (i) * stepX;
        const float py = centreY - juce::jlimit (-1.0f, 1.0f, sampleVal) * halfHeight;
        if (i == 0)
            wavePath.startNewSubPath (px, py);
        else
            wavePath.lineTo (px, py);
    }

    // Oscilloscope neon glow stroke
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.35f));
    g.strokePath (wavePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    g.setColour (fromPalette (panel::colour::cyan));
    g.strokePath (wavePath, juce::PathStrokeType (1.1f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    g.setFont (panelFont (8.5f, true));
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.85f));
    g.drawText (juce::String (juce::roundToInt (scopeGain)) + "x",
                screen.reduced (4.0f, 2.0f).toNearestInt(),
                juce::Justification::topRight, false);
}

// ---------------------------------------------------------------------------
// Pitch / modulation performance lever
// ---------------------------------------------------------------------------

YouKnow106PerformanceLever::YouKnow106PerformanceLever()
{
    setName ("Pitch and modulation lever");
    setTitle ("Pitch and modulation lever");
    setDescription ("Spring-loaded pitch bend and LFO modulation control");
    setTooltip (
        "Drag the illuminated vector lever left or right for pitch bend and "
        "upward for LFO modulation. Both axes spring to zero; the three BENDER "
        "depth sliders set its DCO, VCF and LFO reach.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Rectangle<float> YouKnow106PerformanceLever::controlArea() const noexcept
{
    auto area = getLocalBounds().toFloat().reduced (9.0f, 7.0f);
    area.removeFromTop (19.0f);
    return area;
}

void YouKnow106PerformanceLever::setValues (float bend, float mod, bool notify)
{
    const float newBend = juce::jlimit (-1.0f, 1.0f,
                                        std::isfinite (bend) ? bend : 0.0f);
    const float newMod = juce::jlimit (0.0f, 1.0f,
                                       std::isfinite (mod) ? mod : 0.0f);
    if (std::abs (pitchBend - newBend) <= 1.0e-7f
        && std::abs (modulation - newMod) <= 1.0e-7f)
        return;

    pitchBend = newBend;
    modulation = newMod;
    repaint();
    if (notify && onPositionChanged)
        onPositionChanged (pitchBend, modulation);
}

void YouKnow106PerformanceLever::updateFromPointer (juce::Point<float> position)
{
    const auto area = controlArea();
    const juce::Point<float> rest { area.getCentreX(), area.getBottom() - 8.0f };
    const float horizontalTravel = juce::jmax (1.0f, area.getWidth() * 0.5f - 10.0f);
    const float verticalTravel = juce::jmax (1.0f, rest.y - area.getY() - 7.0f);
    setValues ((position.x - rest.x) / horizontalTravel,
               (rest.y - position.y) / verticalTravel, true);
}

void YouKnow106PerformanceLever::mouseDown (const juce::MouseEvent& event)
{
    updateFromPointer (event.position);
}

void YouKnow106PerformanceLever::mouseDrag (const juce::MouseEvent& event)
{
    updateFromPointer (event.position);
}

void YouKnow106PerformanceLever::mouseUp (const juce::MouseEvent&)
{
    setValues (0.0f, 0.0f, true);
}

void YouKnow106PerformanceLever::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float uiScale = static_cast<float> (getHeight())
                        / panel::performanceDeckHeight;
    drawFramedSurface (g, bounds,
                       fromPalette (panel::colour::faceplateLow).withAlpha (0.82f),
                       uiScale);

    auto header = bounds.reduced (8.0f, 2.0f).removeFromTop (17.0f);
    g.setFont (panelFont (10.5f, true));
    g.setColour (fromPalette (panel::colour::cyan));
    g.drawText ("VECTOR", header.toNearestInt(), juce::Justification::centredLeft);
    g.setColour (fromPalette (panel::colour::textDim));
    juce::String valueCaption { "PITCH / MOD" };
    if (std::abs (pitchBend) > 1.0e-5f || modulation > 1.0e-5f)
    {
        const int bendPercent = juce::roundToInt (pitchBend * 100.0f);
        valueCaption = "B";
        if (bendPercent >= 0)
            valueCaption << "+";
        valueCaption << bendPercent << " M"
                     << juce::roundToInt (modulation * 100.0f);
    }
    g.drawText (valueCaption, header.toNearestInt(),
                juce::Justification::centredRight);

    const auto area = controlArea();
    const juce::Point<float> rest { area.getCentreX(), area.getBottom() - 8.0f };
    const float horizontalTravel = juce::jmax (1.0f, area.getWidth() * 0.5f - 10.0f);
    const float verticalTravel = juce::jmax (1.0f, rest.y - area.getY() - 7.0f);
    const juce::Point<float> puck {
        rest.x + pitchBend * horizontalTravel,
        rest.y - modulation * verticalTravel
    };

    // A small oscilloscope-like grid is this instrument's own visual language,
    // not a drawing of the reference unit's moulded lever slot.
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.10f));
    for (int division = 1; division < 4; ++division)
    {
        const float fraction = static_cast<float> (division) / 4.0f;
        g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * fraction),
                            area.getY(), area.getBottom());
        g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * fraction),
                              area.getX(), area.getRight());
    }

    g.setColour (fromPalette (panel::colour::magenta).withAlpha (0.62f));
    g.drawLine (area.getX() + 4.0f, rest.y, area.getRight() - 4.0f, rest.y, 2.0f);
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.72f));
    g.drawLine (rest.x, rest.y, rest.x, area.getY() + 5.0f, 2.0f);

    juce::ColourGradient glow (
        fromPalette (panel::colour::led).withAlpha (0.48f), puck.x, puck.y,
        fromPalette (panel::colour::led).withAlpha (0.0f), puck.x + 19.0f, puck.y,
        true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (38.0f, 38.0f).withCentre (puck));

    g.setColour (fromPalette (panel::colour::controlShadow));
    g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (puck));
    g.setColour (fromPalette (panel::colour::control));
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (puck));
    g.setColour (fromPalette (panel::colour::led));
    g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (puck), 1.5f);
}

// ---------------------------------------------------------------------------
// Persistent contextual help
// ---------------------------------------------------------------------------

YouKnow106ContextHelp::YouKnow106ContextHelp()
{
    setName ("Context help");
    setTitle ("Context help");
    setDescription ("Shows an explanation for the control under the pointer");
    setInterceptsMouseClicks (false, false);
    showIdle();
}

void YouKnow106ContextHelp::showFor (juce::Component* component,
                                     juce::String value)
{
    // Combo boxes and sliders may report one of their private child components
    // as the mouse target. Walk outward to the first public component carrying
    // help instead of coupling this display to JUCE's internal child layout.
    for (auto* candidate = component; candidate != nullptr;
         candidate = candidate->getParentComponent())
    {
        auto* client = dynamic_cast<juce::TooltipClient*> (candidate);
        if (client == nullptr)
            continue;

        const auto text = client->getTooltip().trim();
        if (text.isEmpty())
            continue;

        auto title = candidate->getName().trim();
        if (title.isEmpty())
            title = candidate->getTitle().trim();
        if (title.isEmpty())
            title = "CONTROL";

        setContent (title.toUpperCase(), text, std::move (value));
        return;
    }

    showIdle();
}

void YouKnow106ContextHelp::showIdle()
{
    if (noticeExpiresAt != 0)
    {
        if (juce::Time::getMillisecondCounter() < noticeExpiresAt)
        {
            setContent (noticeTitle, noticeText, {});
            return;
        }
        noticeExpiresAt = 0;
    }

    setContent ("HELP",
                "Hover a control to read what it does and what it is set to.",
                {});
}

void YouKnow106ContextHelp::showNotice (juce::String title, juce::String text)
{
    constexpr juce::uint32 noticeMilliseconds = 6000;
    noticeTitle = std::move (title);
    noticeText = std::move (text);
    noticeExpiresAt = juce::Time::getMillisecondCounter() + noticeMilliseconds;
    setContent (noticeTitle, noticeText, {});
}

void YouKnow106ContextHelp::setContent (juce::String title, juce::String text,
                                        juce::String value)
{
    if (helpTitle == title && helpText == text && helpValue == value)
        return;

    helpTitle = std::move (title);
    helpText = std::move (text);
    helpValue = std::move (value);
    repaint();
}

void YouKnow106ContextHelp::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float uiScale = static_cast<float> (getHeight()) / panel::helpStripHeight;
    drawFramedSurface (g, bounds,
                       fromPalette (panel::colour::faceplateLow).withAlpha (0.88f),
                       uiScale);

    auto content = bounds.reduced (juce::jmax (8.0f, bounds.getHeight() * 0.28f),
                                   juce::jmax (2.0f, bounds.getHeight() * 0.10f));
    const float fontHeight = juce::jlimit (10.0f, 12.0f,
                                           bounds.getHeight() * 0.34f);

    // The current setting, right-aligned in its own lit column. Reading a value
    // used to need a drag, because only JUCE's transient bubble carried it;
    // hovering is enough now, and the bubble still appears while dragging.
    if (helpValue.isNotEmpty())
    {
        auto valueArea = content.removeFromRight (
            juce::jlimit (90.0f, 200.0f, bounds.getWidth() * 0.13f));
        g.setColour (fromPalette (panel::colour::led));
        g.setFont (panelFont (fontHeight, true));
        g.drawFittedText (helpValue, valueArea.toNearestInt(),
                          juce::Justification::centredRight, 1, 0.9f);
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.42f));
        g.drawVerticalLine (juce::roundToInt (valueArea.getX() - 8.0f),
                            valueArea.getY(), valueArea.getBottom());
        content.removeFromRight (12.0f);
    }

    // The title column carries a control's full name, which is routinely longer
    // than one short word. Three times the original width lets it read on one
    // line instead of eliding.
    const float titleWidth = juce::jlimit (276.0f, 384.0f,
                                           bounds.getWidth() * 0.255f);
    const auto titleArea = content.removeFromLeft (titleWidth);

    g.setColour (fromPalette (panel::colour::cyan));
    g.setFont (panelFont (fontHeight, true));
    g.drawFittedText (helpTitle, titleArea.toNearestInt(),
                      juce::Justification::centredLeft, 1, 1.0f);

    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.42f));
    const float dividerX = content.getX() - 8.0f;
    g.drawVerticalLine (juce::roundToInt (dividerX), content.getY(),
                        content.getBottom());

    content.removeFromLeft (4.0f);
    const bool showingIdlePrompt = helpTitle == "HELP";
    g.setColour (fromPalette (showingIdlePrompt ? panel::colour::textDim
                                                : panel::colour::text));
    g.setFont (panelFont (fontHeight));
    g.drawFittedText (helpText, content.toNearestInt(),
                      juce::Justification::centredLeft, 2, 1.0f);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

YouKnow106AudioProcessorEditor::YouKnow106AudioProcessorEditor (YouKnow106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    texture.ensureBuilt (1024);

    logoLabel.setText ("youknow106", juce::dontSendNotification);
    logoLabel.setFont (panelFont (22.0f, true));
    logoLabel.setColour (juce::Label::textColourId, fromPalette (panel::colour::cyan));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("SIX-VOICE DCO POLYSYNTH / FIELD UNIT",
                          juce::dontSendNotification);
    editionLabel.setFont (panelFont (11.0f, true));
    editionLabel.setColour (juce::Label::textColourId,
                            fromPalette (panel::colour::textDim));
    editionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (editionLabel);

    display.setName ("Status display");
    display.setTitle ("Status display");
    display.setTooltip (
        "Shows the six physical voice cards, the active voice limit, LFO and "
        "envelope motion.");
    addAndMakeVisible (display);

    buildPanelControls();
    buildUtilityStrip();
    buildPresetBar();

    keyboard.setAvailableRange (panel::keyboardLowestMidiNote,
                                panel::keyboardHighestMidiNote);
    keyboard.setLowestVisibleKey (panel::keyboardLowestMidiNote);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (3);
    keyboard.setName ("Playable keyboard");
    keyboard.setTitle ("Playable keyboard");
    keyboard.setDescription ("Play notes with the mouse or computer keyboard");
    keyboard.setTooltip (
        "Plays the original 61-key C2-C7 range with the mouse or computer "
        "keyboard. Click height sets MIDI velocity, but Velocity at zero "
        "ignores it like the hardware. External notes outside this range still work.");
    addAndMakeVisible (keyboard);

    performanceLever.onPositionChanged = [this] (float bend, float modulation)
    {
        audioProcessor.postUiLeverPosition (bend, modulation);
    };
    addAndMakeVisible (performanceLever);

    addAndMakeVisible (contextHelp);

    setResizable (true, true);
    // Below this scale the narrowest authentic panel legends fall under the
    // ten-pixel readability floor. Larger hosts may still expand freely.
    setResizeLimits (panel::minimumEditorWidth, panel::minimumEditorHeight,
                     panel::maximumEditorWidth, panel::maximumEditorHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (
            static_cast<double> (panel::panelWidth())
            / static_cast<double> (panel::editorHeight));
    setSize (juce::roundToInt (panel::panelWidth()),
             juce::roundToInt (panel::editorHeight));
    startTimerHz (24);
}

YouKnow106AudioProcessorEditor::~YouKnow106AudioProcessorEditor()
{
    stopTimer();
    // Closing a plug-in window during a drag must not leave its last physical
    // gesture latched in the engine after the control itself has disappeared.
    // Do not publish an unsolicited centre event for an untouched lever: that
    // would erase a stateful external Pitch Wheel / CC1 position merely because
    // the host closed its editor window.
    if (std::abs (performanceLever.getPitchBend()) > 1.0e-7f
        || performanceLever.getModulation() > 1.0e-7f)
        audioProcessor.postUiLeverPosition (0.0f, 0.0f);
    setLookAndFeel (nullptr);
}

void YouKnow106AudioProcessorEditor::buildPanelControls()
{
    const auto& controls = panel::controls();

    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        const auto& description = controls[index];
        auto& entry = panelControls[index];

        if (description.kind == panel::ControlKind::Slider
            || description.kind == panel::ControlKind::Steps)
        {
            entry.slider = std::make_unique<juce::Slider>();
            entry.slider->setSliderStyle (juce::Slider::LinearVertical);
            entry.slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            entry.slider->setPopupDisplayEnabled (true, true, this);
            entry.slider->setName (description.label);
            entry.slider->setTitle (description.label);
            entry.slider->setTooltip (description.tooltip);
            addAndMakeVisible (*entry.slider);
            attachSlider (*entry.slider, description.parameterId);
        }
        else
        {
            entry.button = std::make_unique<juce::TextButton> (description.label);
            // Radio lamps are owned by their shared parameter attachment. If
            // they self-toggle, clicking the already-selected value can turn
            // its lamp off while the unchanged parameter suppresses a callback.
            const bool isPoly = std::strcmp (description.parameterId,
                                             parameters::poly1) == 0
                             || std::strcmp (description.parameterId,
                                             parameters::poly2) == 0
                             || std::strcmp (description.parameterId,
                                             parameters::legacyKeyMode) == 0;
            entry.button->setClickingTogglesState (
                description.kind != panel::ControlKind::Radio && ! isPoly);
            entry.button->setName (description.label);
            entry.button->setTitle (description.label);
            entry.button->setTooltip (description.tooltip);
            addAndMakeVisible (*entry.button);

            if (description.kind == panel::ControlKind::Toggle)
            {
                if (std::strcmp (description.parameterId, parameters::poly1) == 0)
                {
                    attachPolyButton (*entry.button, parameters::poly1,
                                      parameters::poly2);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::poly2) == 0)
                {
                    attachPolyButton (*entry.button, parameters::poly2,
                                      parameters::poly1);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::legacyKeyMode) == 0)
                {
                    attachUnisonButton (*entry.button);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::chorusI) == 0)
                {
                    attachExclusiveButton (*entry.button, parameters::chorusI,
                                           parameters::chorusII);
                }
                else if (std::strcmp (description.parameterId,
                                      parameters::chorusII) == 0)
                {
                    attachExclusiveButton (*entry.button, parameters::chorusII,
                                           parameters::chorusI);
                }
                else
                    attachButton (*entry.button, description.parameterId);
            }
            else
                attachRadio (*entry.button, description.parameterId,
                             description.groupValue);
        }

        entry.label = std::make_unique<juce::Label>();
        entry.label->setText (description.label, juce::dontSendNotification);
        entry.label->setFont (panelFont (panel::labelPointSize, true));
        entry.label->setColour (juce::Label::textColourId,
                                fromPalette (panel::colour::text));
        entry.label->setJustificationType (juce::Justification::centredTop);
        entry.label->setTooltip (description.tooltip);
        entry.label->setInterceptsMouseClicks (false, false);
        // A stacked button already carries its own legend; repeating it under
        // the stack would just be clutter.
        if (description.kind == panel::ControlKind::Slider
            || description.kind == panel::ControlKind::Steps)
            addAndMakeVisible (*entry.label);
    }
}

void YouKnow106AudioProcessorEditor::buildUtilityStrip()
{
    using namespace youknow106::parameters;

    const auto configure = [this] (juce::Slider& slider, const char* parameterId,
                                   const char* title, const char* tooltip)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider.setMouseDragSensitivity (140);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, this);
        slider.getProperties().set (secondaryStyleProperty, true);
        slider.setName (title);
        slider.setTitle (title);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);
        attachSlider (slider, parameterId);
    };

    constexpr const char* utilityTooltips[] = {
        "Shifts incoming notes by up to one octave before oscillator pitch and "
        "filter key tracking, including notes already held. This is a plug-in extension.",
        "Fine-tunes every oscillator by up to 50 cents. This is a plug-in "
        "extension around the calibrated hardware pitch.",
        "Adds MIDI-velocity response to each voice amplifier. Zero matches the "
        "hardware's fixed velocity; 100% gives full dynamic response.",
        "Scales every modeled component tolerance, trimmer residual, thermal "
        "wander and inherent circuit non-linearity. Zero is the calibrated "
        "digital reference; 100% matches real hardware; values above that "
        "exaggerate the same behaviors for audible contrast.",
        "Scales the modeled hiss of the uncompanded bucket-brigade chorus. "
        "100% is the modeled floor; zero is a clean plug-in extension.",
        "Sets the active voice limit from 1 to 16. Six matches the hardware; "
        "values above six add digital extension voices."
    };

    configure (transposeSlider, transpose, "Transpose", utilityTooltips[0]);
    configure (tuneSlider, masterTune, "Master tune", utilityTooltips[1]);
    configure (velocitySlider, velocity, "Velocity", utilityTooltips[2]);
    configure (calibrationSlider, calibration, "Unit Character", utilityTooltips[3]);
    configure (chorusNoiseSlider, chorusNoise, "Chorus noise", utilityTooltips[4]);
    configure (polyphonySlider, polyphony, "Polyphony", utilityTooltips[5]);

    const char* captions[] = { "TRANSPOSE", "TUNE", "VELOCITY",
                               "UNIT CHARACTER", "CHORUS NOISE", "VOICES" };
    const char* controlNames[] = { "Transpose", "Master tune", "Velocity",
                                   "Unit Character", "Chorus noise", "Polyphony" };
    for (std::size_t index = 0; index < utilityLabels.size(); ++index)
    {
        auto& label = utilityLabels[index];
        label.setText (captions[index], juce::dontSendNotification);
        label.setFont (panelFont (11.0f));
        label.setColour (juce::Label::textColourId,
                         fromPalette (panel::colour::textDim).withAlpha (0.90f));
        label.setJustificationType (juce::Justification::centred);
        label.setName (juce::String (controlNames[index]) + " label");
        label.setTooltip (utilityTooltips[index]);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }

    const auto nameButton = [] (juce::TextButton& button)
    {
        button.setName (button.getButtonText());
        button.setTitle (button.getButtonText());
    };
    nameButton (hqButton);
    nameButton (panicButton);
    nameButton (randomize1Button);
    nameButton (randomize10Button);
    nameButton (randomize50Button);
    nameButton (resetButton);
    syxLoadButton.setName ("Load patch file");
    syxLoadButton.setTitle ("Load patch file");
    syxSaveButton.setName ("Save patch file");
    syxSaveButton.setTitle ("Save patch file");

    hqButton.setClickingTogglesState (true);
    hqButton.getProperties().set (secondaryStyleProperty, true);
    hqButton.setTooltip (
        "Runs the oscillators, nonlinear filter, amplifiers and chorus at a "
        "higher internal rate to reduce aliasing. A change waits until the "
        "instrument is idle; this has no hardware counterpart.");
    addAndMakeVisible (hqButton);
    attachButton (hqButton, hq);

    panicButton.setTooltip (
        "Immediately clears held notes, sustain and every sounding voice when "
        "a stuck note or runaway tail must be stopped.");
    panicButton.onClick = [this] { audioProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    randomize1Button.setTooltip (
        "Nudges each sound-design control toward a random value by at most 1% "
        "of its range. Quantized switches may stay put; master volume and "
        "plug-in extensions stay unchanged.");
    randomize1Button.onClick = [this] { audioProcessor.randomizeParameters (0.01f); };
    addAndMakeVisible (randomize1Button);

    randomize10Button.setTooltip (
        "Moves each sound-design control toward a random value by at most 10% "
        "of its range. Quantized switches may stay put; master volume and "
        "plug-in extensions stay unchanged.");
    randomize10Button.onClick = [this] { audioProcessor.randomizeParameters (0.10f); };
    addAndMakeVisible (randomize10Button);

    randomize50Button.setTooltip (
        "Moves each sound-design control toward a random value by at most 50% "
        "of its range for a strong variation. Quantized switches may stay put; "
        "master volume and extensions stay unchanged.");
    randomize50Button.onClick = [this] { audioProcessor.randomizeParameters (0.50f); };
    addAndMakeVisible (randomize50Button);

    resetButton.setTooltip (
        "Loads the complete INIT program, restoring every tone, performance "
        "and plug-in-extension control to its default.");
    resetButton.onClick = [this] { selectProgram (0); };
    addAndMakeVisible (resetButton);

    // The hardware moves patches over its tape and MIDI jacks; here the same
    // dumps travel as .syx files. LOAD and SAVE speak the instrument's own
    // F0 41 30 patch message, so files round-trip with real units and with
    // any librarian that talks to them.
    syxLoadButton.setTooltip (
        "Loads a .syx patch dump and applies its tone to the panel, exactly "
        "like receiving the dump over MIDI. A file holding a whole bank "
        "applies its first patch. You can also drop a .syx file anywhere on "
        "the instrument.");
    syxLoadButton.onClick = [this] { chooseAndImportPatchFile(); };
    addAndMakeVisible (syxLoadButton);

    syxSaveButton.setTooltip (
        "Saves the current tone as a hardware-compatible .syx patch dump. "
        "Volume, the benders, portamento, the assign mode and the plug-in "
        "extensions are performance controls and are not stored, exactly as "
        "on the hardware.");
    syxSaveButton.onClick = [this] { chooseAndExportPatchFile(); };
    addAndMakeVisible (syxSaveButton);

    // Service actions are momentary utilities, not synth modes. Give them the
    // compact key treatment so they do not carry misleading unlit lamps or
    // compete with the controls above the keyboard.
    for (auto* button : { &syxLoadButton, &syxSaveButton, &panicButton,
                          &resetButton, &randomize1Button, &randomize10Button,
                          &randomize50Button })
        button->getProperties().set (compactStyleProperty, true);
}

void YouKnow106AudioProcessorEditor::buildPresetBar()
{
    presetLabel.setText ("PATCH", juce::dontSendNotification);
    presetLabel.setFont (panelFont (11.0f, true));
    presetLabel.setName ("Patch label");
    presetLabel.setTooltip (
        "Selects INIT or one of the complete 128 factory programs.");
    presetLabel.setColour (juce::Label::textColourId, fromPalette (panel::colour::textDim));
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    presetLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetLabel);

    // A ComboBox reserves id 0 for "nothing selected", so an item id is the
    // program index plus one. Only the three places that touch the box know
    // that; everything else in the bar works in program indices.
    for (int index = 0; index < audioProcessor.getNumPrograms(); ++index)
        presetBox.addItem (audioProcessor.getProgramName (index), index + 1);

    presetBox.setName ("Patch selector");
    presetBox.setTitle ("Patch selector");
    presetBox.setTooltip (
        "Selects INIT or one of the 128 original factory tones. Patch-bar and "
        "host recall restore the tone plus its complete performance and plug-in setup.");
    presetBox.setColour (juce::ComboBox::backgroundColourId,
                         fromPalette (panel::colour::slot));
    presetBox.setColour (juce::ComboBox::textColourId, fromPalette (panel::colour::text));
    presetBox.setColour (juce::ComboBox::outlineColourId,
                         surfaceBorderColour());
    presetBox.setColour (juce::ComboBox::arrowColourId, fromPalette (panel::colour::cyan));
    presetBox.onChange = [this] { selectProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    presetPrevButton.setName ("Previous patch");
    presetPrevButton.setTitle ("Previous patch");
    presetPrevButton.getProperties().set (compactStyleProperty, true);
    presetPrevButton.setTooltip (
        "Loads the previous program in the host list, stopping at INIT.");
    presetPrevButton.onClick = [this] { stepProgram (-1); };
    addAndMakeVisible (presetPrevButton);

    presetNextButton.setName ("Next patch");
    presetNextButton.setTitle ("Next patch");
    presetNextButton.getProperties().set (compactStyleProperty, true);
    presetNextButton.setTooltip (
        "Loads the next program in the host list, stopping at B88.");
    presetNextButton.onClick = [this] { stepProgram (1); };
    addAndMakeVisible (presetNextButton);

    // JUCE does not emit ComboBox::onChange when the user picks the already
    // selected item, so an explicit reload is the only reliable way to discard
    // edits without stepping to another sound and back.
    presetReloadButton.setName ("Reload patch");
    presetReloadButton.setTitle ("Reload patch");
    presetReloadButton.getProperties().set (compactStyleProperty, true);
    presetReloadButton.setTooltip (
        "Reloads the selected program exactly and discards all tone, "
        "performance and plug-in-control edits.");
    presetReloadButton.onClick = [this]
    {
        selectProgram (audioProcessor.getCurrentProgram());
    };
    addAndMakeVisible (presetReloadButton);

    presetEditedLabel.setText ("EDITED", juce::dontSendNotification);
    presetEditedLabel.setFont (panelFont (11.0f, true));
    presetEditedLabel.setColour (juce::Label::textColourId,
                                 fromPalette (panel::colour::magenta));
    presetEditedLabel.setTooltip (
        "Lights when the current panel no longer matches the selected program.");
    presetEditedLabel.setJustificationType (juce::Justification::centredLeft);
    presetEditedLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetEditedLabel);

    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::selectProgram (int index)
{
    if (index < 0 || index >= audioProcessor.getNumPrograms())
        return;

    // Deliberately apply even when this is already the selected program. The
    // RELOAD button promises an exact panel restore; a sub-7-bit slider move is
    // audibly equivalent and need not light EDITED, but its cap must still move
    // back to the stored position when the player asks to discard it.
    audioProcessor.setCurrentProgram (index);
    // The host owns the program index in its own UI too, so tell it the change
    // came from here rather than letting the two drift apart.
    audioProcessor.updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::stepProgram (int delta)
{
    const int wanted = juce::jlimit (0, audioProcessor.getNumPrograms() - 1,
                                     audioProcessor.getCurrentProgram() + delta);
    if (wanted == audioProcessor.getCurrentProgram())
        return;

    audioProcessor.setCurrentProgram (wanted);
    audioProcessor.updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::refreshPresetBar()
{
    const int program = audioProcessor.getCurrentProgram();
    const bool edited = audioProcessor.currentProgramIsEdited();
    if (program == shownProgram && edited == shownEdited)
        return;

    shownProgram = program;
    shownEdited = edited;
    presetBox.setSelectedId (program + 1, juce::dontSendNotification);
    presetEditedLabel.setVisible (edited);
    presetPrevButton.setEnabled (program > 0);
    presetNextButton.setEnabled (program < audioProcessor.getNumPrograms() - 1);
}

bool YouKnow106AudioProcessorEditor::isInterestedInFileDrag (
    const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".syx"))
            return true;
    return false;
}

void YouKnow106AudioProcessorEditor::filesDropped (const juce::StringArray& files,
                                                   int, int)
{
    for (const auto& path : files)
    {
        if (path.endsWithIgnoreCase (".syx"))
        {
            importPatchFile (juce::File (path));
            return;
        }
    }
}

void YouKnow106AudioProcessorEditor::chooseAndImportPatchFile()
{
    sysExFileChooser = std::make_unique<juce::FileChooser> (
        "Load a JUNO-106 patch dump", juce::File(), "*.syx");
    sysExFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [safe = SafePointer (this)] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr)
                return;
            const auto file = chooser.getResult();
            if (file != juce::File())
                safe->importPatchFile (file);
        });
}

void YouKnow106AudioProcessorEditor::chooseAndExportPatchFile()
{
    const auto programName = audioProcessor.getProgramName (
        audioProcessor.getCurrentProgram());
    const auto defaultFile =
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (juce::File::createLegalFileName (programName)
                           + ".syx");
    sysExFileChooser = std::make_unique<juce::FileChooser> (
        "Save the current tone as a patch dump", defaultFile, "*.syx");
    sysExFileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = SafePointer (this)] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr)
                return;
            auto file = chooser.getResult();
            if (file == juce::File())
                return;
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension ("syx");
            safe->exportPatchFile (file);
        });
}

void YouKnow106AudioProcessorEditor::importPatchFile (const juce::File& file)
{
    // A complete 128-patch bank dump is under 3 kB. The bound only refuses
    // files that cannot possibly be MIDI dumps, not large multi-gear archives.
    constexpr juce::int64 maximumSysExFileBytes = 1 << 20;
    juce::MemoryBlock bytes;
    if (!file.existsAsFile() || file.getSize() > maximumSysExFileBytes
        || !file.loadFileAsData (bytes))
    {
        contextHelp.showNotice (
            "LOAD", "Could not read \"" + file.getFileName() + "\".");
        return;
    }

    int patchesFound = 0;
    if (audioProcessor.importPatchSysExBytes (bytes.getData(), bytes.getSize(),
                                              patchesFound))
    {
        contextHelp.showNotice (
            "LOAD",
            patchesFound > 1
                ? "Applied the first of " + juce::String (patchesFound)
                      + " patches in \"" + file.getFileName() + "\"."
                : "Applied the patch from \"" + file.getFileName() + "\".");
        return;
    }

    contextHelp.showNotice (
        "LOAD", "No JUNO-106 patch dump in \"" + file.getFileName() + "\".");
}

void YouKnow106AudioProcessorEditor::exportPatchFile (const juce::File& file)
{
    const auto message = audioProcessor.currentPatchAsSysEx (
        audioProcessor.sysExMidiChannel());
    if (message.getRawDataSize() == 0
        || !file.replaceWithData (message.getRawData(),
                                  static_cast<std::size_t> (
                                      message.getRawDataSize())))
    {
        contextHelp.showNotice (
            "SAVE", "Could not write \"" + file.getFileName() + "\".");
        return;
    }

    contextHelp.showNotice (
        "SAVE",
        "Saved the current tone as \"" + file.getFileName()
            + "\". Performance controls travel with the session, not the "
              "patch, as on the hardware.");
}

void YouKnow106AudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                   const char* parameterId)
{
    if (const auto* parameter = audioProcessor.parameters.getParameter (parameterId))
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    else
        jassertfalse;

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        audioProcessor.parameters, parameterId, slider));
}

void YouKnow106AudioProcessorEditor::attachButton (juce::Button& button,
                                                   const char* parameterId)
{
    jassert (audioProcessor.parameters.getParameter (parameterId) != nullptr);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (
        audioProcessor.parameters, parameterId, button));
}

void YouKnow106AudioProcessorEditor::attachExclusiveButton (
    juce::Button& button, const char* parameterId, const char* otherParameterId)
{
    attachButton (button, parameterId);
    button.onClick = [this, &button, otherParameterId]
    {
        if (! button.getToggleState())
            return; // Pressing the lit hardware button switches chorus off.

        if (auto* other = audioProcessor.parameters.getParameter (otherParameterId))
        {
            other->beginChangeGesture();
            other->setValueNotifyingHost (other->convertTo0to1 (0.0f));
            other->endChangeGesture();
        }
    };
}

void YouKnow106AudioProcessorEditor::attachPolyButton (
    juce::Button& button, const char* parameterId, const char* otherParameterId)
{
    // The contacts are momentary and the assigner firmware owns their lamps.
    // Use a ParameterAttachment with notification suppressed on the lamp: a
    // ButtonAttachment reports parameter-driven lamp changes as clicks, which
    // would recursively run the transition below.
    auto* attachedParameter = audioProcessor.parameters.getParameter (parameterId);
    auto* companionParameter = audioProcessor.parameters.getParameter (otherParameterId);
    jassert (attachedParameter != nullptr && companionParameter != nullptr);
    if (attachedParameter == nullptr || companionParameter == nullptr)
        return;

    // MODE shows the assigner's three stable states as three latches, so a POLY
    // lamp lights only while its own contact is the *only* one closed. With
    // both closed the separate UNISON latch is the lit one. That makes the lamp
    // depend on both parameters, so both have to be able to refresh it.
    const auto refresh = [this, &button, parameterId, otherParameterId] (float)
    {
        const auto isOn = [this] (const char* id)
        {
            auto* parameter = audioProcessor.parameters.getParameter (id);
            return parameter != nullptr && parameter->getValue() > 0.5f;
        };
        button.setToggleState (isOn (parameterId) && ! isOn (otherParameterId),
                               juce::dontSendNotification);
    };

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *attachedParameter, refresh, nullptr);
    auto companion = std::make_unique<juce::ParameterAttachment> (
        *companionParameter, refresh, nullptr);

    button.onClick = [this, parameterId, otherParameterId]
    {
        // Both decisions come from the parameter atomics. The lamp update is
        // asynchronous, so using its presentation state here can take the
        // wrong branch when host automation and a mouse click coincide.
        const auto isOn = [this] (const char* id)
        {
            if (const auto* value = audioProcessor.parameters.getRawParameterValue (id))
                return value->load (std::memory_order_relaxed) > 0.5f;
            return false;
        };
        const bool ownWasOn = isOn (parameterId);
        const bool otherWasOn = isOn (otherParameterId);

        const auto set = [this] (const char* id, bool on)
        {
            if (auto* target = audioProcessor.parameters.getParameter (id))
            {
                const float wanted = on ? 1.0f : 0.0f;
                target->beginChangeGesture();
                target->setValueNotifyingHost (target->convertTo0to1 (wanted));
                target->endChangeGesture();
            }
        };

        // A mouse cannot hold both panel contacts at once. Shift-click is the
        // explicit virtual equivalent of the hardware's simultaneous press;
        // an ordinary click retains the real single-button meaning.
        if (juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            if (ownWasOn && otherWasOn)
            {
                // Both lamps already show Solo Unison, but the contacts are
                // momentary. Holding them again still enters the firmware
                // handler and rebuilds every held assignment.
                audioProcessor.requestKeyModeReassert();
                return;
            }
            set (parameterId, true);
            set (otherParameterId, true);
            return;
        }

        if (ownWasOn && ! otherWasOn)
        {
            // The visible latch does not move, but the real assigner still
            // gates, clears and rescans held keys on this repeated press.
            audioProcessor.requestKeyModeReassert();
            return;
        }

        if (ownWasOn && otherWasOn)
        {
            // From Solo Unison, pressing one contact alone selects that mode.
            set (otherParameterId, false);
            return;
        }

        // A normal press selects this single mode. Clear the other parameter
        // first: the transient both-off pair canonicalises to the previous/
        // target Poly 1 behavior, whereas setting this one first would render
        // an unintended block of Unison during a Poly 1 -> Poly 2 change.
        set (otherParameterId, false);
        set (parameterId, true);
    };
    auto* pointer = attachment.get();
    auto* companionPointer = companion.get();
    parameterAttachments.push_back (std::move (attachment));
    parameterAttachments.push_back (std::move (companion));
    pointer->sendInitialUpdate();
    companionPointer->sendInitialUpdate();
}

void YouKnow106AudioProcessorEditor::attachUnisonButton (juce::Button& button)
{
    // Solo Unison is the state both momentary contacts closed together select.
    // A mouse cannot hold two buttons, so this latch performs that press. The
    // poly pair stays the authoritative state; this button neither adds a
    // parameter nor a fourth mode.
    auto* first = audioProcessor.parameters.getParameter (parameters::poly1);
    auto* second = audioProcessor.parameters.getParameter (parameters::poly2);
    jassert (first != nullptr && second != nullptr);
    if (first == nullptr || second == nullptr)
        return;

    const auto refresh = [this, &button] (float)
    {
        const auto isOn = [this] (const char* id)
        {
            auto* parameter = audioProcessor.parameters.getParameter (id);
            return parameter != nullptr && parameter->getValue() > 0.5f;
        };
        button.setToggleState (isOn (parameters::poly1) && isOn (parameters::poly2),
                               juce::dontSendNotification);
    };

    auto firstAttachment = std::make_unique<juce::ParameterAttachment> (
        *first, refresh, nullptr);
    auto secondAttachment = std::make_unique<juce::ParameterAttachment> (
        *second, refresh, nullptr);

    button.onClick = [this]
    {
        const auto isOn = [this] (const char* id)
        {
            auto* parameter = audioProcessor.parameters.getParameter (id);
            return parameter != nullptr && parameter->getValue() > 0.5f;
        };

        if (isOn (parameters::poly1) && isOn (parameters::poly2))
        {
            // Already Unison. The contacts are momentary, so pressing them
            // again still re-enters the firmware handler and rebuilds every
            // held assignment rather than doing nothing.
            audioProcessor.requestKeyModeReassert();
            return;
        }

        for (const char* id : { parameters::poly1, parameters::poly2 })
        {
            if (auto* target = audioProcessor.parameters.getParameter (id))
            {
                target->beginChangeGesture();
                target->setValueNotifyingHost (target->convertTo0to1 (1.0f));
                target->endChangeGesture();
            }
        }
    };

    auto* firstPointer = firstAttachment.get();
    auto* secondPointer = secondAttachment.get();
    parameterAttachments.push_back (std::move (firstAttachment));
    parameterAttachments.push_back (std::move (secondAttachment));
    firstPointer->sendInitialUpdate();
    secondPointer->sendInitialUpdate();
}

void YouKnow106AudioProcessorEditor::attachRadio (juce::Button& button,
                                                  const char* parameterId, int value)
{
    auto* parameter = audioProcessor.parameters.getParameter (parameterId);
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [&button, value] (float current)
        {
            button.setToggleState (juce::roundToInt (current) == value,
                                   juce::dontSendNotification);
        },
        nullptr);

    auto* pointer = attachment.get();
    button.onClick = [pointer, &button, value]
    {
        pointer->setValueAsCompleteGesture (static_cast<float> (value));
        // A complete gesture to the already-selected value is intentionally a
        // no-op at the parameter. Keep the lamp canonical in that case too.
        button.setToggleState (true, juce::dontSendNotification);
    };
    parameterAttachments.push_back (std::move (attachment));
    pointer->sendInitialUpdate();
}

const char* YouKnow106AudioProcessorEditor::parameterIdFor (
    juce::Component* component) const
{
    using namespace youknow106::parameters;

    const auto& controls = panel::controls();
    const std::pair<const juce::Component*, const char*> extensions[] = {
        { &transposeSlider,   transpose },
        { &tuneSlider,        masterTune },
        { &velocitySlider,    velocity },
        { &calibrationSlider, calibration },
        { &chorusNoiseSlider, chorusNoise },
        { &polyphonySlider,   polyphony },
        { &hqButton,          hq }
    };

    for (const juce::Component* candidate = component; candidate != nullptr;
         candidate = candidate->getParentComponent())
    {
        for (std::size_t index = 0; index < controls.size(); ++index)
        {
            const auto& entry = panelControls[index];
            const juce::Component* owner = entry.slider != nullptr
                ? static_cast<const juce::Component*> (entry.slider.get())
                : static_cast<const juce::Component*> (entry.button.get());
            if (owner != nullptr && owner == candidate)
                return controls[index].parameterId;
        }

        for (const auto& extension : extensions)
            if (extension.first == candidate)
                return extension.second;
    }
    return nullptr;
}

juce::String YouKnow106AudioProcessorEditor::parameterValueTextFor (
    juce::Component* component) const
{
    const char* parameterId = parameterIdFor (component);
    if (parameterId == nullptr)
        return {};

    // MODE is one three-state assigner shown as three latches, so none of the
    // three can report its own parameter and be telling the truth. POLY 1 reads
    // "On" in Solo Unison, when its lamp is dark; and the UNISON latch owns no
    // parameter of its own at all -- it closes both momentary contacts, and the
    // legacy id the panel table names for it is only updated by a program
    // recall, so a click on it would leave the strip printing whichever mode
    // was last loaded. All three print the mode the pair actually selects.
    const auto isMode = [parameterId] (const char* id) {
        return std::strcmp (parameterId, id) == 0;
    };
    if (isMode (youknow106::parameters::poly1)
        || isMode (youknow106::parameters::poly2)
        || isMode (youknow106::parameters::legacyKeyMode))
    {
        const auto engaged = [this] (const char* id) {
            const auto* value = audioProcessor.parameters.getRawParameterValue (id);
            return value != nullptr
                && value->load (std::memory_order_relaxed) > 0.5f;
        };
        const auto mode = youknow106::keyModeFor (
            engaged (youknow106::parameters::poly1),
            engaged (youknow106::parameters::poly2));
        // Named by the legacy choice parameter rather than here, so the strip
        // cannot drift from what the host's own automation lane calls it.
        if (const auto* names = audioProcessor.parameters.getParameter (
                youknow106::parameters::legacyKeyMode))
            return names->getText (
                names->convertTo0to1 (static_cast<float> (mode)), 0);
        return {};
    }

    const auto* parameter = audioProcessor.parameters.getParameter (parameterId);
    if (parameter == nullptr)
        return {};

    auto text = parameter->getCurrentValueAsText().trim();
    const auto suffix = parameter->getLabel().trim();
    if (text.isNotEmpty() && suffix.isNotEmpty() && ! text.endsWith (suffix))
        text << " " << suffix;
    return text;
}

juce::Rectangle<float> YouKnow106AudioProcessorEditor::scaled (float x, float y,
                                                               float width,
                                                               float height) const
{
    return { x * scale, y * scale, width * scale, height * scale };
}

void YouKnow106AudioProcessorEditor::resized()
{
    const auto bounds = getLocalBounds();
    const float totalUnits = panel::editorHeight;
    scale = juce::jmin (static_cast<float> (bounds.getWidth()) / panel::panelWidth(),
                        static_cast<float> (bounds.getHeight()) / totalUnits);

    const auto& controls = panel::controls();
    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        const auto& description = controls[index];
        auto& entry = panelControls[index];
        const auto area = scaled (description.x, description.y,
                                  description.width, description.height);

        if (entry.slider != nullptr)
            entry.slider->setBounds (area.toNearestInt());
        else if (entry.button != nullptr)
            entry.button->setBounds (area.toNearestInt());

        if (entry.label != nullptr)
        {
            entry.label->setFont (panelFont (
                juce::jmax (10.0f, panel::labelPointSize * scale), true));
            entry.label->setBounds (
                scaled (description.labelX, description.labelY,
                        description.labelWidth,
                        description.labelHeight).toNearestInt());
        }
    }

    logoLabel.setFont (panelFont (juce::jmax (18.0f, 22.0f * scale), true));
    editionLabel.setFont (panelFont (juce::jmax (10.0f, 11.0f * scale), true));

    logoLabel.setBounds (scaled (panel::panelMargin, panel::mastheadTop + 1.0f,
                                 190.0f, 24.0f).toNearestInt());
    editionLabel.setBounds (scaled (panel::panelMargin,
                                    panel::mastheadTop + 25.0f,
                                    194.0f, 14.0f).toNearestInt());

    display.setBounds (scaled (666.0f, panel::mastheadTop, 442.0f,
                               panel::mastheadHeight).toNearestInt());

    // Patch navigation shares the masthead with branding and telemetry. Its
    // asymmetric placement is part of this project's own masthead identity and avoids
    // reproducing the reference unit's numeric-button patch block.
    constexpr float presetLeft = 214.0f;
    constexpr float stepWidth = 22.0f;
    presetLabel.setFont (panelFont (juce::jmax (10.0f, 11.0f * scale), true));
    presetEditedLabel.setFont (panelFont (juce::jmax (10.0f, 11.0f * scale), true));
    presetLabel.setBounds (
        scaled (presetLeft, panel::presetTop + 3.0f, 38.0f, 20.0f).toNearestInt());
    presetPrevButton.setBounds (
        scaled (presetLeft + 40.0f, panel::presetTop + 2.0f, stepWidth,
                panel::presetHeight - 4.0f).toNearestInt());
    presetNextButton.setBounds (
        scaled (presetLeft + 40.0f + stepWidth + 3.0f,
                panel::presetTop + 2.0f,
                stepWidth, panel::presetHeight - 4.0f).toNearestInt());
    constexpr float reloadWidth = 48.0f;
    const float reloadLeft = presetLeft + 40.0f + 2.0f * (stepWidth + 3.0f);
    presetReloadButton.setBounds (
        scaled (reloadLeft, panel::presetTop + 2.0f, reloadWidth,
                panel::presetHeight - 4.0f).toNearestInt());
    const float boxLeft = reloadLeft + reloadWidth + 4.0f;
    presetBox.setBounds (
        scaled (boxLeft, panel::presetTop + 2.0f,
                238.0f, panel::presetHeight - 4.0f).toNearestInt());
    presetEditedLabel.setBounds (
        scaled (boxLeft + 244.0f,
                panel::presetTop + 3.0f, 52.0f, 20.0f).toNearestInt());

    for (auto& label : utilityLabels)
        label.setFont (panelFont (juce::jmax (10.0f, 11.0f * scale)));

    // The two secondary cards share one grid: three cells for CHARACTER, four
    // for KEYBOARD CONTROL, each cell wide enough for its legend at full size.
    // Sizing the cells from the legends rather than the other way round is what
    // lets "UNIT CHARACTER" and "TRANSPOSE" print unabbreviated.
    // The full component bounds remain generous hit targets. The secondary
    // look-and-feel draws smaller graphite hardware inside them, leaving useful
    // negative space without making fine adjustment harder.
    constexpr float deckContentTop = panel::performanceDeckTop
                                   + panel::headerHeight + 6.0f;
    constexpr float deckContentHeight = panel::performanceDeckHeight
                                      - panel::headerHeight - 14.0f;
    constexpr float deckLabelHeight = 16.0f;
    constexpr float deckKnobSize = 64.0f;
    constexpr float deckStackHeight = deckLabelHeight + 6.0f + deckKnobSize;
    constexpr float deckLabelTop = deckContentTop
                                 + (deckContentHeight - deckStackHeight) * 0.5f;
    constexpr float deckKnobTop = deckLabelTop + deckLabelHeight + 6.0f;

    constexpr float characterCell = (panel::characterCardWidth - 14.0f) / 3.0f;
    constexpr float characterLeft = panel::characterCardX + 7.0f;
    const auto characterCellLeft = [&] (int index) {
        return characterLeft + characterCell * static_cast<float> (index);
    };
    utilityLabels[3].setBounds (
        scaled (characterCellLeft (0), deckLabelTop, characterCell,
                deckLabelHeight).toNearestInt());
    calibrationSlider.setBounds (
        scaled (characterCellLeft (0) + (characterCell - deckKnobSize) * 0.5f,
                deckKnobTop, deckKnobSize, deckKnobSize).toNearestInt());
    utilityLabels[4].setBounds (
        scaled (characterCellLeft (1), deckLabelTop, characterCell,
                deckLabelHeight).toNearestInt());
    chorusNoiseSlider.setBounds (
        scaled (characterCellLeft (1) + (characterCell - deckKnobSize) * 0.5f,
                deckKnobTop, deckKnobSize, deckKnobSize).toNearestInt());
    // HQ is a latch, not a knob, so it centres on the knob row's own axis
    // instead of sharing its top edge.
    constexpr float hqHeight = 42.0f;
    hqButton.setBounds (
        scaled (characterCellLeft (2) + 12.0f,
                deckKnobTop + (deckKnobSize - hqHeight) * 0.5f,
                characterCell - 24.0f, hqHeight).toNearestInt());

    juce::Slider* deckSliders[] = { &transposeSlider, &tuneSlider,
                                     &velocitySlider, &polyphonySlider };
    const std::array<int, 4> deckLabelIndices { 0, 1, 2, 5 };
    constexpr float keyboardCell = (panel::keyboardCardWidth - 14.0f) / 4.0f;
    constexpr float keyboardLeft = panel::keyboardCardX + 7.0f;
    for (int index = 0; index < 4; ++index)
    {
        const float cellLeft = keyboardLeft + keyboardCell * static_cast<float> (index);
        utilityLabels[static_cast<std::size_t> (
            deckLabelIndices[static_cast<std::size_t> (index)])].setBounds (
            scaled (cellLeft, deckLabelTop, keyboardCell,
                    deckLabelHeight).toNearestInt());
        deckSliders[index]->setBounds (
            scaled (cellLeft + (keyboardCell - deckKnobSize) * 0.5f,
                    deckKnobTop, deckKnobSize, deckKnobSize).toNearestInt());
    }

    // The service keys sit on the utility bar beside the help text rather than
    // on the instrument surface. Patch-file LOAD/SAVE live here too: moving
    // dumps in and out is the tape jacks' job, not the front panel's, so the
    // instrument surface still carries no librarian. Live MIDI SEND remains
    // intentionally absent; the processor keeps that plumbing for the wire.
    constexpr float operationPitch = panel::operationsBarWidth / 7.0f;
    constexpr float operationWidth = operationPitch - 4.0f;
    const float operationTop = panel::panelHeight + panel::keyboardHeight
                             + panel::helpStripGap + 6.0f;
    juce::Button* operationButtons[] = { &syxLoadButton, &syxSaveButton,
                                         &panicButton, &resetButton,
                                         &randomize1Button, &randomize10Button,
                                         &randomize50Button };
    for (int index = 0; index < 7; ++index)
        operationButtons[index]->setBounds (
            scaled (panel::operationsBarX
                        + operationPitch * static_cast<float> (index),
                    operationTop, operationWidth,
                    panel::helpStripHeight - 12.0f).toNearestInt());

    performanceLever.setBounds (
        scaled (panel::vectorPadX, panel::performanceDeckTop,
                panel::vectorPadWidth,
                panel::performanceDeckHeight).toNearestInt());

    keyboard.setBounds (scaled (0.0f, panel::panelHeight, panel::panelWidth(),
                                panel::keyboardHeight).toNearestInt());
    // C-to-C contains one more white key than five complete octaves. Fit that
    // exact physical span to the panel at every editor size, with no hidden
    // off-instrument octaves or scrolling.
    keyboard.setKeyWidth (static_cast<float> (keyboard.getWidth())
                          / static_cast<float> (panel::keyboardWhiteKeyCount));

    // The help text shares the utility bar with the service keys, so it stops
    // where they begin rather than running underneath them.
    contextHelp.setBounds (
        scaled (panel::panelMargin,
                panel::panelHeight + panel::keyboardHeight + panel::helpStripGap,
                panel::operationsBarX - panel::panelMargin - 8.0f,
                panel::helpStripHeight).toNearestInt());
}

void YouKnow106AudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    texture.fill (g, bounds, fromPalette (panel::colour::faceplate));

    // A raised masthead and two semantic traces establish this project's
    // console identity. Blue is audio flow; green is modulation
    // and live performance. They are functional wayfinding, not copied livery.
    const auto masthead = scaled (4.0f, 4.0f,
                                  panel::panelWidth() - 8.0f, 54.0f);
    drawFramedSurface (g, masthead,
                       fromPalette (panel::colour::faceplateHigh).withAlpha (0.72f),
                       scale);

    const auto audioTraceY = scaled (0.0f, 274.0f, 1.0f, 1.0f).getY();
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.58f));
    g.drawLine (scaled (12.0f, 0.0f, 1.0f, 1.0f).getX(), audioTraceY,
                scaled (1108.0f, 0.0f, 1.0f, 1.0f).getX(), audioTraceY,
                juce::jmax (1.0f, 1.5f * scale));
    const auto modulationTraceY = scaled (0.0f, 462.0f, 1.0f, 1.0f).getY();
    g.setColour (fromPalette (panel::colour::magenta).withAlpha (0.58f));
    g.drawLine (scaled (12.0f, 0.0f, 1.0f, 1.0f).getX(), modulationTraceY,
                scaled (320.0f, 0.0f, 1.0f, 1.0f).getX(), modulationTraceY,
                juce::jmax (1.0f, 1.5f * scale));
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.58f));
    g.drawLine (scaled (328.0f, 0.0f, 1.0f, 1.0f).getX(), modulationTraceY,
                scaled (638.0f, 0.0f, 1.0f, 1.0f).getX(), modulationTraceY,
                juce::jmax (1.0f, 1.5f * scale));

    const auto& sections = panel::sections();
    for (const auto& section : sections)
    {
        const auto box = scaled (section.x, section.y,
                                 section.width, section.height);
        // Compared with strcmp rather than by building a std::string: this runs
        // once per section on every one of twenty-four repaints a second.
        const bool isVcf = section.name != nullptr
                        && std::strcmp (section.name, "VCF") == 0;
        const bool isLowerDeck = section.y >= panel::performanceDeckTop;

        // A shallow moulded recess for each section, then its coloured header.
        // All perimeters share one neutral radius and stroke; emphasis belongs
        // inside the frame, where it cannot make neighbouring cards look like
        // they came from different products.
        drawFramedSurface (
            g, box,
            fromPalette (panel::colour::faceplateLow)
                .withAlpha (isLowerDeck ? 0.48f : 0.60f),
            scale);

        const auto header = scaled (section.x, section.y, section.width,
                                    panel::headerHeight);
        const auto tint = accentColour (section.accent);
        juce::ColourGradient headerGrad (
            tint.withAlpha (isLowerDeck ? 0.14f : (isVcf ? 0.32f : 0.22f)),
            header.getX(), header.getY(),
            tint.withAlpha (isLowerDeck ? 0.035f : (isVcf ? 0.12f : 0.06f)),
            header.getX(), header.getBottom(), false);
        g.setGradientFill (headerGrad);
        g.fillRoundedRectangle (header, surfaceCornerRadius * scale);
        g.setColour (tint.withAlpha (isLowerDeck ? 0.58f
                                                 : (isVcf ? 1.0f : 0.82f)));
        g.fillRect (header.getX() + 2.0f * scale,
                    header.getBottom() - 2.0f * scale,
                    header.getWidth() - 4.0f * scale,
                    2.0f * scale);

        g.setColour ((isVcf ? tint.brighter (0.15f) : tint)
                         .withAlpha (isLowerDeck ? 0.76f : 1.0f));
        g.setFont (panelFont (juce::jmax (10.0f, (isVcf ? 13.0f : panel::headerPointSize) * scale), true));
        g.drawText (section.name, header.toNearestInt(),
                    juce::Justification::centred);
        drawSurfaceBorder (g, box, surfaceBorderColour(), scale);
    }

    const auto drawConsoleCard = [this, &g] (juce::Rectangle<float> area,
                                              std::uint32_t tintValue,
                                              const char* title,
                                              const char* code,
                                              bool isSecondary = false)
    {
        const auto tint = fromPalette (tintValue);
        const auto cardBounds = area;
        drawFramedSurface (
            g, cardBounds,
            fromPalette (panel::colour::faceplateLow)
                .withAlpha (isSecondary ? 0.43f : 0.82f),
            scale);

        const auto header = area.removeFromTop (panel::headerHeight * scale);
        juce::ColourGradient cardHeaderGrad (
            tint.withAlpha (isSecondary ? 0.09f : 0.22f), header.getX(), header.getY(),
            tint.withAlpha (isSecondary ? 0.025f : 0.04f),
            header.getX(), header.getBottom(), false);
        g.setGradientFill (cardHeaderGrad);
        g.fillRoundedRectangle (header.reduced (1.0f),
                                surfaceCornerRadius * scale);
        g.setColour (tint.withAlpha (isSecondary ? 0.42f : 0.85f));
        g.fillRect (header.getX() + 2.0f * scale,
                    header.getBottom() - 2.0f * scale,
                    header.getWidth() - 4.0f * scale, 2.0f * scale);
        g.setFont (panelFont (juce::jmax (10.0f,
                                         (isSecondary ? 11.0f : panel::headerPointSize) * scale), true));
        g.drawText (title, header.reduced (8.0f * scale, 0.0f).toNearestInt(),
                    juce::Justification::centredLeft);
        g.setColour (fromPalette (panel::colour::textDim)
                         .withAlpha (isSecondary ? 0.42f : 1.0f));
        g.setFont (panelFont (juce::jmax (9.0f, 10.0f * scale), true));
        g.drawText (code, header.reduced (8.0f * scale, 0.0f).toNearestInt(),
                    juce::Justification::centredRight);
        drawSurfaceBorder (g, cardBounds, surfaceBorderColour(), scale);
    };

    // The product-only cards share the lower deck with the performance
    // controls, to the right of the lever, bender and assign latches. They are
    // drawn in the secondary weight so nothing on this deck can be mistaken
    // for part of the reproduced control row above it.
    drawConsoleCard (scaled (panel::characterCardX, panel::performanceDeckTop,
                             panel::characterCardWidth,
                             panel::performanceDeckHeight),
                     panel::colour::magenta, "CHARACTER", "EXT / 02", true);
    drawConsoleCard (scaled (panel::keyboardCardX, panel::performanceDeckTop,
                             panel::keyboardCardWidth,
                             panel::performanceDeckHeight),
                     panel::colour::magenta, "KEYBOARD CONTROL", "LIVE / 04", true);

    // Hard separation around the playable area protects the keybed visually
    // from the service controls above and the explanatory display below.
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.30f));
    g.fillRect (scaled (0.0f, panel::panelHeight - 2.0f,
                        panel::panelWidth(), 2.0f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (scaled (0.0f, panel::panelHeight + panel::keyboardHeight,
                        panel::panelWidth(), 2.0f));
}

void YouKnow106AudioProcessorEditor::timerCallback()
{
    display.refresh (audioProcessor);
    // The program can also move from the host's own menu, and the panel from an
    // incoming patch dump or a randomise. Polling is what keeps the bar honest
    // about both without the processor having to know an editor exists.
    refreshPresetBar();

    // TooltipClient remains the single source of accessible help text, but its
    // presentation is the fixed strip. Poll the same mouse source JUCE's
    // TooltipWindow uses and climb nested slider/combo children in showFor().
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto* hovered = mouse.isTouch() ? nullptr : mouse.getComponentUnderMouse();
    if (hovered == this || isParentOf (hovered))
        contextHelp.showFor (hovered, parameterValueTextFor (hovered));
    else
        contextHelp.showIdle();
}

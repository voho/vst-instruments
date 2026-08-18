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

juce::Font panelFont (float height, bool bold = false)
{
    auto font = juce::Font (juce::FontOptions (height, bold ? juce::Font::bold
                                                            : juce::Font::plain));
    font.setHorizontalScale (panel::typefaceHorizontalScale);
    return font;
}

// Small legends benefit more from open letterforms than from the mild
// condensation used by the large silk-screen labels.
juce::Font clearPanelFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold
                                                       : juce::Font::plain));
}

constexpr auto compactStyleProperty = "compactStyle";
constexpr auto secondaryStyleProperty = "secondaryStyle";
constexpr auto hardwareStyleProperty = "hardwareStyle";
constexpr auto hardwareKeyCentreProperty = "hardwareKeyCentre";
constexpr auto actionIconProperty = "actionIcon";
constexpr auto segmentDisplayStyleProperty = "segmentDisplayStyle";
constexpr auto statusLampStyleProperty = "statusLampStyle";
constexpr auto statusLampOnProperty = "statusLampOn";

// Framed controls share one quiet neutral edge. Broad tonal fields carry the
// panel hierarchy instead of outlining every surface.
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

void drawJewelLamp (juce::Graphics& g, juce::Rectangle<float> lens, bool on,
                    juce::Colour activeColour, float alpha = 1.0f)
{
    if (lens.isEmpty())
        return;

    const float diameter = juce::jmin (lens.getWidth(), lens.getHeight());
    lens = juce::Rectangle<float> (diameter, diameter).withCentre (lens.getCentre());

    if (on)
    {
        g.setColour (activeColour.withAlpha (0.13f * alpha));
        g.fillEllipse (lens.expanded (diameter * 0.55f));
        g.setColour (activeColour.withAlpha (0.20f * alpha));
        g.fillEllipse (lens.expanded (diameter * 0.28f));
    }

    const auto bezel = lens.expanded (juce::jmax (0.9f, diameter * 0.14f));
    juce::ColourGradient bezelGradient (
        fromPalette (panel::colour::brassHigh).withAlpha (0.58f * alpha),
        bezel.getX(), bezel.getY(),
        fromPalette (panel::colour::controlShadow).darker (0.45f)
            .withAlpha (0.88f * alpha),
        bezel.getRight(), bezel.getBottom(), false);
    g.setGradientFill (bezelGradient);
    g.fillEllipse (bezel);
    g.setColour (juce::Colours::black.withAlpha (0.78f * alpha));
    g.drawEllipse (bezel.reduced (0.45f), juce::jmax (0.8f, diameter * 0.09f));

    const auto glass = lens.reduced (juce::jmax (0.35f, diameter * 0.05f));
    const auto glassTop = on ? activeColour.brighter (0.52f)
                             : fromPalette (panel::colour::ledDim).brighter (0.30f);
    const auto glassBottom = on ? activeColour.darker (0.36f)
                                : fromPalette (panel::colour::ledDim).darker (0.42f);
    juce::ColourGradient glassGradient (
        glassTop.withAlpha ((on ? 1.0f : 0.78f) * alpha), glass.getX(), glass.getY(),
        glassBottom.withAlpha ((on ? 1.0f : 0.86f) * alpha),
        glass.getRight(), glass.getBottom(), false);
    g.setGradientFill (glassGradient);
    g.fillEllipse (glass);

    g.setColour (juce::Colours::white.withAlpha ((on ? 0.72f : 0.18f) * alpha));
    g.fillEllipse (juce::Rectangle<float> (diameter * 0.22f, diameter * 0.16f)
                       .withCentre ({ glass.getCentreX() - diameter * 0.18f,
                                      glass.getCentreY() - diameter * 0.20f }));
}

void drawActionIcon (juce::Graphics& g, juce::Rectangle<float> area,
                     const juce::String& icon, juce::Colour colour)
{
    area = area.reduced (juce::jmax (1.0f, area.getWidth() * 0.08f));
    if (area.isEmpty())
        return;

    const float stroke = juce::jlimit (1.2f, 2.0f, area.getHeight() * 0.10f);
    g.setColour (colour);

    if (icon == "load" || icon == "save")
    {
        const bool down = icon == "load";
        const float centreX = area.getCentreX();
        const float shaftTop = area.getY() + area.getHeight() * 0.14f;
        const float shaftBottom = area.getY() + area.getHeight() * 0.66f;
        const float headY = down ? shaftBottom : shaftTop;
        const float tailY = down ? shaftTop : shaftBottom;
        g.drawLine (centreX, tailY, centreX, headY, stroke);
        juce::Path arrow;
        arrow.startNewSubPath (centreX - area.getWidth() * 0.18f,
                               headY + (down ? -1.0f : 1.0f)
                                           * area.getHeight() * 0.16f);
        arrow.lineTo (centreX, headY);
        arrow.lineTo (centreX + area.getWidth() * 0.18f,
                      headY + (down ? -1.0f : 1.0f)
                                  * area.getHeight() * 0.16f);
        g.strokePath (arrow, juce::PathStrokeType (
                                 stroke, juce::PathStrokeType::curved,
                                 juce::PathStrokeType::rounded));
        juce::Path tray;
        tray.startNewSubPath (area.getX() + area.getWidth() * 0.16f,
                              area.getBottom() - area.getHeight() * 0.23f);
        tray.lineTo (area.getX() + area.getWidth() * 0.16f,
                     area.getBottom() - area.getHeight() * 0.08f);
        tray.lineTo (area.getRight() - area.getWidth() * 0.16f,
                     area.getBottom() - area.getHeight() * 0.08f);
        tray.lineTo (area.getRight() - area.getWidth() * 0.16f,
                     area.getBottom() - area.getHeight() * 0.23f);
        g.strokePath (tray, juce::PathStrokeType (
                                stroke, juce::PathStrokeType::curved,
                                juce::PathStrokeType::rounded));
        return;
    }

    if (icon == "reload" || icon == "reset")
    {
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.34f;
        constexpr float start = -2.15f;
        constexpr float finish = 2.65f;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                           start, finish, true);
        g.strokePath (arc, juce::PathStrokeType (
                               stroke, juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded));
        const juce::Point<float> tip {
            centre.x + std::sin (finish) * radius,
            centre.y - std::cos (finish) * radius
        };
        juce::Path head;
        head.startNewSubPath (tip.x - area.getWidth() * 0.19f,
                              tip.y - area.getHeight() * 0.03f);
        head.lineTo (tip);
        head.lineTo (tip.x - area.getWidth() * 0.03f,
                     tip.y + area.getHeight() * 0.18f);
        g.strokePath (head, juce::PathStrokeType (
                                stroke, juce::PathStrokeType::curved,
                                juce::PathStrokeType::rounded));
        return;
    }

    if (icon == "random")
    {
        // A die collapses into an anonymous dotted box at this size. The
        // question mark remains legible and says exactly what variation does.
        juce::Graphics::ScopedSaveState saveState (g);
        g.setFont (clearPanelFont (
            juce::jlimit (14.0f, 19.0f, area.getHeight() * 0.92f), true));
        g.drawText ("?", area.toNearestInt(), juce::Justification::centred,
                    false);
        return;
    }

    if (icon == "stop")
    {
        const auto outer = area.reduced (area.getWidth() * 0.12f,
                                         area.getHeight() * 0.08f);
        g.drawRoundedRectangle (outer, outer.getWidth() * 0.22f, stroke);
        g.fillRoundedRectangle (outer.reduced (outer.getWidth() * 0.28f),
                                outer.getWidth() * 0.08f);
    }
}

juce::uint8 segmentMaskFor (juce::juce_wchar character) noexcept
{
    switch (character)
    {
        case '0': return 0x3f;
        case '1': return 0x06;
        case '2': return 0x5b;
        case '3': return 0x4f;
        case '4': return 0x66;
        case '5': return 0x6d;
        case '6': return 0x7d;
        case '7': return 0x07;
        case '8': return 0x7f;
        case '9': return 0x6f;
        case '-': return 0x40;
        default:  return 0x00;
    }
}

void drawSegmentDigit (juce::Graphics& g, juce::Rectangle<float> area,
                       juce::uint8 activeSegments)
{
    area = area.reduced (area.getWidth() * 0.10f, area.getHeight() * 0.06f);
    const float left = area.getX();
    const float right = area.getRight();
    const float top = area.getY();
    const float middle = area.getCentreY();
    const float bottom = area.getBottom();
    const std::array<std::pair<juce::Point<float>, juce::Point<float>>, 7> segments {{
        { { left, top },       { right, top } },
        { { right, top },      { right, middle } },
        { { right, middle },   { right, bottom } },
        { { left, bottom },    { right, bottom } },
        { { left, middle },    { left, bottom } },
        { { left, top },       { left, middle } },
        { { left, middle },    { right, middle } },
    }};
    const float width = juce::jlimit (2.1f, 4.2f, area.getWidth() * 0.16f);

    for (std::size_t index = 0; index < segments.size(); ++index)
    {
        const auto& segment = segments[index];
        g.setColour (fromPalette (panel::colour::ledDim).withAlpha (0.36f));
        g.drawLine ({ segment.first, segment.second }, width);
        if ((activeSegments & (1u << index)) == 0)
            continue;

        g.setColour (fromPalette (panel::colour::led).withAlpha (0.18f));
        g.drawLine ({ segment.first, segment.second }, width * 2.2f);
        g.setColour (fromPalette (panel::colour::led));
        g.drawLine ({ segment.first, segment.second }, width);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine ({ segment.first, segment.second }, width * 0.28f);
    }
}

void drawSegmentDisplay (juce::Graphics& g, juce::Rectangle<float> bounds,
                         const juce::String& text)
{
    bounds = bounds.reduced (juce::jmax (2.0f, bounds.getWidth() * 0.06f),
                             juce::jmax (2.0f, bounds.getHeight() * 0.08f));
    constexpr int digits = 2;
    const float gap = juce::jmax (3.0f, bounds.getWidth() * 0.10f);
    const float digitWidth = (bounds.getWidth() - gap) / digits;
    for (int index = 0; index < digits; ++index)
    {
        const auto character = index < text.length() ? text[index] : ' ';
        drawSegmentDigit (g,
                          { bounds.getX() + static_cast<float> (index)
                                               * (digitWidth + gap),
                            bounds.getY(), digitWidth, bounds.getHeight() },
                          segmentMaskFor (character));
    }
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
    const float fontHeight = juce::jlimit (14.0f, 16.0f,
                                           area.getHeight() * 0.76f);
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
               fromPalette (panel::colour::cyan).brighter (0.14f).withAlpha (0.98f));
    setColour (juce::PopupMenu::highlightedTextColourId,
               fromPalette (panel::colour::scope));
    setColour (juce::PopupMenu::headerTextColourId,
               fromPalette (panel::colour::textDim));

    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
               fromPalette (panel::colour::control));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
               fromPalette (panel::colour::controlShadow));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               fromPalette (panel::colour::magenta).withAlpha (0.85f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               fromPalette (panel::colour::cyan).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::shadowColourId,
               juce::Colours::black.withAlpha (0.6f));
}

std::unique_ptr<juce::FocusOutline>
YouKnow106LookAndFeel::createFocusOutlineForComponent (juce::Component&)
{
    struct WindowProperties final : public juce::FocusOutline::OutlineWindowProperties
    {
        juce::Rectangle<int> getOutlineBounds (juce::Component& component) override
        {
            return component.getScreenBounds().expanded (3);
        }

        void drawOutline (juce::Graphics& g, int width, int height) override
        {
            const auto bounds = juce::Rectangle<float> (
                static_cast<float> (width), static_cast<float> (height));

            // Adjacent dark and light rings remain visible over every panel,
            // key and illuminated control colour without guessing which one
            // happens to be behind the focused component.
            g.setColour (fromPalette (panel::colour::scope));
            g.drawRoundedRectangle (bounds.reduced (1.5f), 4.0f, 3.0f);
            g.setColour (fromPalette (panel::colour::text));
            g.drawRoundedRectangle (bounds.reduced (3.5f), 2.5f, 1.5f);
        }
    };

    return std::make_unique<juce::FocusOutline> (
        std::make_unique<WindowProperties>());
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

    // Only the endpoints and midpoint remain: 0%, 50% and 100% are useful
    // physical references, while extra graduations imply precision the panel
    // does not print or need.
    for (int tick = 0; tick < 3; ++tick)
    {
        const float t = bounds.getY() + 3.0f
                      + (bounds.getHeight() - 6.0f)
                          * static_cast<float> (tick) / 2.0f;
        constexpr float length = 7.0f;
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.38f));
        g.fillRect (bounds.getX() + 1.0f, t, length, 1.0f);
        g.fillRect (bounds.getRight() - 1.0f - length, t, length, 1.0f);
    }

    // Compact graphite cap and a crisp white witness line match the physical
    // panel's low-profile faders without using a bitmap control.
    const float capHeight = juce::jlimit (12.0f, 17.0f,
                                          bounds.getHeight() * 0.105f);
    // Keep the hit target generous, but leave a little more faceplate between
    // neighbouring caps. Their centre lines still follow the common slot grid.
    const float capWidth = juce::jmax (12.0f,
                                       juce::jmin (28.0f, bounds.getWidth() - 5.0f));
    const auto cap = juce::Rectangle<float> (capWidth, capHeight)
                         .withCentre ({ bounds.getCentreX(),
                                        juce::jlimit (bounds.getY() + capHeight * 0.5f,
                                                      bounds.getBottom() - capHeight * 0.5f,
                                                      sliderPos) });

    g.setColour (juce::Colours::black.withAlpha (0.68f));
    g.fillRoundedRectangle (cap.expanded (1.2f).translated (0.0f, 2.0f), 2.2f);

    const auto face = cap.reduced (1.2f);
    juce::ColourGradient shellGradient (
        fromPalette (panel::colour::control).brighter (0.10f),
        cap.getX(), cap.getY(),
        fromPalette (panel::colour::controlShadow).darker (0.18f),
        cap.getX(), cap.getBottom(), false);
    g.setGradientFill (shellGradient);
    g.fillRoundedRectangle (cap, 2.1f);

    juce::ColourGradient gradient (
        fromPalette (panel::colour::control).brighter (0.18f),
        face.getX(), face.getY(),
        fromPalette (panel::colour::controlShadow).brighter (0.04f),
        face.getX(), face.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (face, 1.5f);

    g.setColour (fromPalette (panel::colour::brass).withMultipliedAlpha (
        slider.isMouseOverOrDragging() ? 0.95f : 0.56f));
    g.drawRoundedRectangle (cap.reduced (0.45f), 2.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.34f));
    g.drawLine (face.getX() + 2.0f, face.getY() + 1.0f,
                face.getRight() - 2.0f, face.getY() + 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (const float fraction : { 0.24f, 0.76f })
    {
        const float grooveX = face.getX() + face.getWidth() * fraction;
        g.drawLine (grooveX, face.getY() + 3.0f,
                    grooveX, face.getBottom() - 3.0f, 1.0f);
    }

    // The witness line the eye actually reads the value from.
    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.fillRect (face.getX() + 1.0f, face.getCentreY() - 0.9f,
                face.getWidth() - 2.0f, 1.8f);
    g.setColour (fromPalette (panel::colour::magenta).brighter (0.28f));
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
                               : panel::colour::faceplateHigh).brighter (collarLift),
        collar.getX(), collar.getY(),
        fromPalette (panel::colour::controlShadow).darker (secondary ? 0.38f : 0.0f),
        collar.getRight(), collar.getBottom(), false);
    g.setGradientFill (collarGradient);
    g.fillEllipse (collar);

    const auto cap = secondary ? collar.reduced (3.8f) : collar;
    juce::ColourGradient capGradient (
        fromPalette (secondary ? panel::colour::faceplateHigh
                               : panel::colour::faceplateHigh).brighter (
                                   secondary ? 0.12f : 0.05f),
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
    g.setColour (fromPalette (secondary ? panel::colour::cyan
                                       : panel::colour::text)
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
    const bool hardware = static_cast<bool> (
        button.getProperties().getWithDefault (hardwareStyleProperty, false));

    if (hardware)
    {
        const float keyHeight = juce::jlimit (12.0f, 22.0f,
                                              bounds.getHeight() * 0.30f);
        const float keyCentreFraction = static_cast<float> (
            button.getProperties().getWithDefault (
                hardwareKeyCentreProperty, 0.72));
        // The component owns the full grid cell for an easy target; the
        // physical key is inset so adjacent switches have a consistent gutter.
        auto key = juce::Rectangle<float> (
            juce::jmax (8.0f, bounds.getWidth() - 6.0f), keyHeight)
                       .withCentre ({ bounds.getCentreX(),
                                      bounds.getY()
                                          + bounds.getHeight() * keyCentreFraction });
        key = key.translated (0.0f, isDown ? 1.0f : 0.0f);

        g.setColour (juce::Colours::black.withAlpha (0.78f));
        g.fillRect (key.expanded (1.2f).translated (0.0f, 1.3f));
        const auto base = button.findColour (
            on ? juce::TextButton::buttonOnColourId
               : juce::TextButton::buttonColourId);
        juce::ColourGradient keyGradient (
            base.brighter (isHighlighted ? 0.16f : 0.08f), key.getX(), key.getY(),
            base.darker (isDown ? 0.22f : 0.10f), key.getX(), key.getBottom(), false);
        g.setGradientFill (keyGradient);
        g.fillRect (key);
        g.setColour (juce::Colours::white.withAlpha (
            button.isEnabled() ? (isHighlighted ? 0.36f : 0.20f) : 0.08f));
        g.drawRect (key, 1.0f);

        const float lensSize = juce::jlimit (5.6f, 7.2f,
                                             bounds.getWidth() * 0.18f);
        const auto lamp = juce::Rectangle<float> (lensSize, lensSize)
                              .withCentre ({ key.getCentreX(), key.getY() - 7.0f });
        drawJewelLamp (g, lamp, on, fromPalette (panel::colour::led),
                       button.isEnabled() ? 1.0f : 0.42f);
        if (on)
        {
            g.setColour (fromPalette (panel::colour::brassHigh).withAlpha (0.42f));
            g.drawRect (key.reduced (0.4f), 1.0f);
        }
        return;
    }

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
    const float lens = juce::jlimit (5.2f, 7.8f, key.getHeight() * 0.24f);
    const auto led = juce::Rectangle<float> (lens, lens)
                         .withCentre ({ key.getCentreX(),
                                        key.getY() + key.getHeight() * 0.25f });
    drawJewelLamp (g, led, on, fromPalette (panel::colour::led),
                   secondary ? 0.82f : 1.0f);
}

void YouKnow106LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto text = button.getButtonText();
    const bool compact = static_cast<bool> (
        button.getProperties().getWithDefault (compactStyleProperty, false));
    const bool hardware = static_cast<bool> (
        button.getProperties().getWithDefault (hardwareStyleProperty, false));
    const auto actionIcon = button.getProperties()
                                .getWithDefault (actionIconProperty, juce::var())
                                .toString();
    g.setColour (fromPalette (button.getToggleState() ? panel::colour::text
                                                      : panel::colour::textDim));

    if (hardware)
    {
        const bool footRegister = isFootRegisterLegend (text);
        const float legendHeight = footRegister
            ? juce::jlimit (17.0f, 20.0f, bounds.getHeight() * 0.38f)
            : juce::jlimit (12.0f, 22.0f, bounds.getHeight() * 0.29f);
        const auto legend = bounds.withHeight (legendHeight);
        g.setColour (fromPalette (panel::colour::text)
                         .withMultipliedAlpha (button.isEnabled() ? 0.92f : 0.48f));
        if (isWaveformLegend (text))
        {
            drawWaveformLegend (g, legend, text == "PULSE");
            return;
        }
        if (isFootRegisterLegend (text))
        {
            drawFootRegisterLegend (g, legend, text);
            return;
        }

        float size = juce::jlimit (9.0f, 13.0f, legendHeight * 0.82f);
        const float natural = juce::GlyphArrangement::getStringWidth (
            clearPanelFont (size, true), text);
        if (natural > legend.getWidth() - 2.0f && natural > 0.0f)
            size *= (legend.getWidth() - 2.0f) / natural;
        g.setFont (clearPanelFont (juce::jmax (9.0f, size), true));
        g.drawFittedText (text, legend.toNearestInt(),
                          juce::Justification::centredTop, 1, 1.0f);
        return;
    }

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
        const float size = juce::jlimit (10.5f, 13.0f,
                                         bounds.getHeight() * 0.55f);
        g.setFont (clearPanelFont (size, true));
        const auto displayText = text == "DRIFT 1%" ? juce::String ("1%")
                               : text == "VARY 10%" ? juce::String ("10%")
                               : text == "MORPH 50%" ? juce::String ("50%")
                                                      : text;
        // Keep every glyph inside the moving key face rather than merely
        // inside the component's outer bezel.
        auto content = bounds.reduced (5.0f, 4.0f);
        if (actionIcon.isNotEmpty())
        {
            const float iconSize = juce::jmin (15.0f, content.getHeight());
            auto iconArea = content.removeFromLeft (iconSize)
                                   .withSizeKeepingCentre (iconSize, iconSize);
            content.removeFromLeft (3.0f);
            const auto tint = actionIcon == "stop"
                            ? fromPalette (panel::colour::led)
                            : fromPalette (panel::colour::textDim);
            drawActionIcon (g, iconArea, actionIcon, tint);
        }
        g.drawText (displayText, content.toNearestInt(),
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
    g.setColour (juce::Colours::white.withAlpha (0.055f));
    g.drawLine (bounds.getX() + 2.0f, bounds.getY() + 1.0f,
                bounds.getRight() - 2.0f, bounds.getY() + 1.0f,
                juce::jmax (1.0f, uiScale));
    g.setColour (box.findColour (juce::ComboBox::outlineColourId)
                     .withMultipliedAlpha (0.55f));
    g.drawVerticalLine (buttonX, bounds.getY() + 3.0f,
                        bounds.getBottom() - 3.0f);

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

void YouKnow106LookAndFeel::drawCornerResizer (juce::Graphics& g, int width,
                                                int height, bool isMouseOver,
                                                bool isMouseDragging)
{
    const float alpha = isMouseDragging ? 0.92f : isMouseOver ? 0.68f : 0.30f;
    g.setColour (fromPalette (panel::colour::brassHigh).withAlpha (alpha));
    for (int index = 0; index < 3; ++index)
    {
        const float inset = 4.0f + static_cast<float> (index) * 4.0f;
        g.drawLine (static_cast<float> (width) - inset - 5.0f,
                    static_cast<float> (height) - 2.5f,
                    static_cast<float> (width) - 2.5f,
                    static_cast<float> (height) - inset - 5.0f,
                    1.2f);
    }
}

void YouKnow106LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (static_cast<bool> (label.getProperties().getWithDefault (
            segmentDisplayStyleProperty, false)))
    {
        drawSegmentDisplay (g, label.getLocalBounds().toFloat(), label.getText());
        return;
    }

    auto textBounds = label.getLocalBounds().toFloat();
    if (static_cast<bool> (label.getProperties().getWithDefault (
            statusLampStyleProperty, false)))
    {
        const float diameter = juce::jmin (8.0f, textBounds.getHeight() * 0.34f);
        auto lampArea = textBounds.removeFromLeft (diameter + 9.0f)
                                  .withSizeKeepingCentre (diameter, diameter);
        const bool on = static_cast<bool> (label.getProperties().getWithDefault (
            statusLampOnProperty, false));
        drawJewelLamp (g, lampArea, on, fromPalette (panel::colour::led));
    }

    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), textBounds.toNearestInt(),
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
    g.setOpacity (0.28f);
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
    const auto bounds = getLocalBounds().toFloat();
    const float uiScale = static_cast<float> (getHeight())
                        / panel::displayReferenceHeight;

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
    const float lampSize = juce::jmin (10.5f, voiceRow.getHeight() * 0.44f,
                                       pitch * 0.52f);
    g.setFont (clearPanelFont (9.5f, true));
    for (int voice = 0; voice < lamps; ++voice)
    {
        const float centreX = voiceRow.getX()
                            + (static_cast<float> (voice) + 0.5f) * pitch;
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.72f));
        g.drawText (juce::String (voice + 1),
                    juce::Rectangle<float> (centreX - pitch * 0.5f,
                                            voiceRow.getY(), pitch,
                                            voiceRow.getHeight() * 0.38f)
                        .toNearestInt(),
                    juce::Justification::centred, false);
        const auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                              .withCentre ({ centreX,
                                             voiceRow.getY()
                                                 + voiceRow.getHeight() * 0.70f });
        const bool lit = (voiceMask & (1 << voice)) != 0;
        drawJewelLamp (g, lamp, lit, fromPalette (panel::colour::led));
    }

    g.setColour (fromPalette (panel::colour::textDim));
    g.setFont (clearPanelFont (11.0f, true));
    g.drawText (ready ? juce::String (voices) + " / " + juce::String (lamps) + " ACTIVE"
                      : juce::String ("STANDBY"),
                readout.toNearestInt(), juce::Justification::centredRight);

    // Modulation and envelope meters.
    const auto meter = [&g] (juce::Rectangle<float> row, float value, bool bipolar,
                             std::uint32_t tint, const char* caption)
    {
        auto labelArea = row.removeFromLeft (28.0f);
        g.setColour (fromPalette (panel::colour::textDim));
        g.setFont (panelFont (10.5f, true));
        g.drawText (caption, labelArea.toNearestInt(), juce::Justification::centredLeft);

        const auto track = row.reduced (0.0f, row.getHeight() * 0.33f);
        g.setColour (fromPalette (panel::colour::slot));
        g.fillRoundedRectangle (track, 2.0f);

        juce::Rectangle<float> fill;
        if (bipolar)
        {
            const float centre = track.getCentreX();
            const float span = track.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, value);
            fill = { juce::jmin (centre, centre + span), track.getY(),
                     std::abs (span), track.getHeight() };
            g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.52f));
            g.drawVerticalLine (juce::roundToInt (centre), track.getY() - 1.0f,
                                track.getBottom() + 1.0f);
        }
        else
        {
            fill = { track.getX(), track.getY(),
                     track.getWidth() * juce::jlimit (0.0f, 1.0f, value),
                     track.getHeight() };
        }
        if (fill.getWidth() > 0.1f)
        {
            g.setColour (fromPalette (tint).withAlpha (0.20f));
            g.fillRoundedRectangle (fill.expanded (1.4f, 1.0f), 2.0f);
            g.setColour (fromPalette (tint));
            g.fillRoundedRectangle (fill, 1.7f);
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawLine (fill.getX() + 1.0f, fill.getY() + 1.0f,
                        fill.getRight() - 1.0f, fill.getY() + 1.0f, 0.8f);
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
    g.setFont (panelFont (14.0f, true));
    g.setColour (fromPalette (panel::colour::cyan).withAlpha (0.90f));
    g.drawText (juce::String (temperature, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")) + "C",
                area.toNearestInt(), juce::Justification::centredLeft);

    g.setColour (fromPalette (panel::colour::textDim));
    const float railV = 15.0f - railDroop;
    g.drawText (juce::String (railV, 2) + "V RAIL", area.toNearestInt(),
                juce::Justification::centredRight);

    // --- Right Section: real-time oscilloscope, full height ---
    // One quiet CRT bezel is enough; the lower faceplate supplies the surface.
    const auto screen = rightBox.reduced (1.0f);
    g.setColour (fromPalette (panel::colour::scope));
    g.fillRoundedRectangle (screen, 2.0f);
    g.setColour (surfaceBorderColour().withAlpha (0.48f));
    g.drawRoundedRectangle (screen.reduced (0.5f), 2.0f,
                            juce::jmax (0.8f, uiScale));

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

    g.setFont (panelFont (9.5f, true));
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.85f));
    g.drawText (juce::String (juce::roundToInt (scopeGain)) + "x",
                screen.reduced (4.0f, 2.0f).toNearestInt(),
                juce::Justification::topRight, false);

    // What the QUALITY selector actually got. The selector states a request;
    // the engine runs the smaller of that and what the host rate still needs,
    // so a 96 kHz session showing 4x on the panel is really running 2x. This
    // is the only place that difference is visible, which is why the applied
    // factor and the internal rate it produces are reported together.
    if (ready && sampleRate > 0.0)
        g.drawText (juce::String (oversampling) + "x "
                        + juce::String (sampleRate * oversampling / 1000.0, 1)
                        + "k",
                    screen.reduced (4.0f, 2.0f).toNearestInt(),
                    juce::Justification::topLeft, false);
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
        "Drag the spring-loaded lever left or right for pitch bend and "
        "upward for LFO modulation. Both axes spring to zero; the three BENDER "
        "depth sliders set its DCO, VCF and LFO reach. With keyboard focus, "
        "hold the arrow keys and release them to spring back to zero.");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setHasFocusOutline (true);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Rectangle<float> YouKnow106PerformanceLever::controlArea() const noexcept
{
    auto area = getLocalBounds().toFloat().reduced (9.0f, 4.0f);
    area.removeFromTop (12.0f);
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
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    if (notify && onPositionChanged)
        onPositionChanged (pitchBend, modulation);
}

juce::String YouKnow106PerformanceLever::getAccessibilityValueText() const
{
    const int bendPercent = juce::roundToInt (pitchBend * 100.0f);
    juce::String result { "Pitch bend " };
    if (bendPercent > 0)
        result << "+";
    result << bendPercent << "%, modulation "
           << juce::roundToInt (modulation * 100.0f) << "%";
    return result;
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
    keyboardGestureActive = false;
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

bool YouKnow106PerformanceLever::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)
        setValues (-1.0f, modulation, true);
    else if (key == juce::KeyPress::rightKey)
        setValues (1.0f, modulation, true);
    else if (key == juce::KeyPress::upKey)
        setValues (pitchBend, 1.0f, true);
    else if (key == juce::KeyPress::downKey)
        setValues (pitchBend, 0.0f, true);
    else if (key == juce::KeyPress::escapeKey
             || key == juce::KeyPress::returnKey
             || key == juce::KeyPress::spaceKey)
    {
        keyboardGestureActive = false;
        setValues (0.0f, 0.0f, true);
        return true;
    }
    else
    {
        return false;
    }

    keyboardGestureActive = true;
    return true;
}

bool YouKnow106PerformanceLever::keyStateChanged (bool isKeyDown)
{
    if (isKeyDown || ! keyboardGestureActive)
        return false;

    keyboardGestureActive = false;
    setValues (0.0f, 0.0f, true);
    return true;
}

void YouKnow106PerformanceLever::focusLost (FocusChangeType)
{
    if (! keyboardGestureActive)
        return;

    keyboardGestureActive = false;
    setValues (0.0f, 0.0f, true);
}

std::unique_ptr<juce::AccessibilityHandler>
YouKnow106PerformanceLever::createAccessibilityHandler()
{
    class ValueInterface final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit ValueInterface (YouKnow106PerformanceLever& leverToWrap)
            : lever (leverToWrap)
        {
        }

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            return lever.getAccessibilityValueText();
        }
        void setValueAsString (const juce::String&) override {}

    private:
        YouKnow106PerformanceLever& lever;
    };

    class Handler final : public juce::AccessibilityHandler
    {
    public:
        explicit Handler (YouKnow106PerformanceLever& leverToWrap)
            : juce::AccessibilityHandler (
                  leverToWrap, juce::AccessibilityRole::group, {},
                  { std::make_unique<ValueInterface> (leverToWrap) }),
              lever (leverToWrap)
        {
        }

        juce::String getHelp() const override { return lever.getTooltip(); }

    private:
        YouKnow106PerformanceLever& lever;
    };

    return std::make_unique<Handler> (*this);
}

void YouKnow106PerformanceLever::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    auto header = bounds.reduced (8.0f, 1.0f).removeFromTop (11.0f);
    g.setFont (clearPanelFont (10.5f, true));
    g.setColour (fromPalette (panel::colour::text));
    g.drawText ("BENDER", header.toNearestInt(), juce::Justification::centredLeft);
    g.setColour (fromPalette (panel::colour::textDim));
    juce::String valueCaption { "LFO TRIG" };
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

    const auto horizontalSlot = juce::Rectangle<float> (
        area.getX() + 3.0f, rest.y - 3.0f, area.getWidth() - 6.0f, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.88f));
    g.fillRoundedRectangle (horizontalSlot, 2.0f);
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.34f));
    g.drawLine (rest.x, rest.y, rest.x, area.getY() + 2.0f, 1.0f);

    const auto lever = juce::Rectangle<float> (22.0f, 10.0f).withCentre (puck);
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (lever.expanded (1.0f).translated (0.0f, 1.0f), 2.0f);
    juce::ColourGradient leverGradient (
        fromPalette (panel::colour::controlShadow).brighter (0.12f),
        lever.getX(), lever.getY(), fromPalette (panel::colour::slot),
        lever.getX(), lever.getBottom(), false);
    g.setGradientFill (leverGradient);
    g.fillRoundedRectangle (lever, 2.0f);
    g.setColour (fromPalette (panel::colour::text).withAlpha (0.82f));
    g.drawLine (lever.getCentreX(), lever.getY() + 2.0f,
                lever.getCentreX(), lever.getBottom() - 2.0f, 1.2f);
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
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

std::unique_ptr<juce::AccessibilityHandler>
YouKnow106ContextHelp::createAccessibilityHandler()
{
    class ValueInterface final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit ValueInterface (YouKnow106ContextHelp& helpToWrap)
            : help (helpToWrap)
        {
        }

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            juce::String result = help.getHelpTitle() + ": " + help.getHelpText();
            if (help.getHelpValue().isNotEmpty())
                result << ", current value " << help.getHelpValue();
            return result;
        }
        void setValueAsString (const juce::String&) override {}

    private:
        YouKnow106ContextHelp& help;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces {
            std::make_unique<ValueInterface> (*this) });
}

void YouKnow106ContextHelp::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float uiScale = static_cast<float> (getHeight()) / panel::helpStripHeight;
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.88f));
    g.fillRoundedRectangle (bounds,
                            juce::jmax (2.5f, surfaceCornerRadius * uiScale));

    auto content = bounds.reduced (juce::jmax (8.0f, bounds.getHeight() * 0.28f),
                                   juce::jmax (2.0f, bounds.getHeight() * 0.10f));
    const float fontHeight = juce::jlimit (12.0f, 13.0f,
                                           bounds.getHeight() * 0.36f);

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

    logoLabel.setText ("YouKnow;)", juce::dontSendNotification);
    logoLabel.setFont (panelFont (28.0f, true));
    logoLabel.setColour (juce::Label::textColourId, fromPalette (panel::colour::text));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("by Protocodus", juce::dontSendNotification);
    editionLabel.setFont (clearPanelFont (14.0f));
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
    buildHardwareProgrammer();

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

    // JUCE sliders opt out of keyboard focus by default. Put every public
    // control in ordinary Tab traversal and use JUCE's native focus outline so
    // the active target remains visible with this custom-drawn look-and-feel.
    for (auto* child : getChildren())
    {
        const bool isSlider = dynamic_cast<juce::Slider*> (child) != nullptr;
        const bool isInteractive = isSlider
            || dynamic_cast<juce::Button*> (child) != nullptr
            || dynamic_cast<juce::ComboBox*> (child) != nullptr
            || dynamic_cast<juce::MidiKeyboardComponent*> (child) != nullptr
            || dynamic_cast<YouKnow106PerformanceLever*> (child) != nullptr;
        if (isSlider)
            child->setWantsKeyboardFocus (true);
        if (isInteractive)
        {
            child->setHasFocusOutline (true);
            child->addKeyListener (this);
        }
    }

    setResizable (true, true);
    // Below this scale the narrowest authentic panel legends fall under the
    // ten-pixel readability floor. Larger hosts may still expand freely.
    setResizeLimits (panel::minimumEditorWidth, panel::minimumEditorHeight,
                     panel::maximumEditorWidth, panel::maximumEditorHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (
            static_cast<double> (panel::panelWidth())
            / static_cast<double> (panel::editorHeight));
    setSize (panel::defaultEditorWidth, panel::defaultEditorHeight);
    startTimerHz (24);
}

YouKnow106AudioProcessorEditor::~YouKnow106AudioProcessorEditor()
{
    stopTimer();
    for (auto* child : getChildren())
        child->removeKeyListener (this);

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
            || description.kind == panel::ControlKind::Knob
            || description.kind == panel::ControlKind::Steps)
        {
            entry.slider = std::make_unique<juce::Slider>();
            if (description.kind == panel::ControlKind::Knob)
            {
                entry.slider->setSliderStyle (
                    juce::Slider::RotaryHorizontalVerticalDrag);
                entry.slider->setRotaryParameters (
                    juce::MathConstants<float>::pi * 1.25f,
                    juce::MathConstants<float>::pi * 2.75f, true);
                entry.slider->setMouseDragSensitivity (140);
            }
            else
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
            entry.button->getProperties().set (hardwareStyleProperty, true);
            // Store the logical key role once. Pixel-height thresholds made
            // the whole key jump when an ordinary resize crossed a breakpoint.
            entry.button->getProperties().set (
                hardwareKeyCentreProperty,
                isFootRegisterLegend (description.label) ? 0.80
                    : description.height > 110.0f ? 0.34
                    : description.height > 65.0f ? 0.62 : 0.72);
            const auto keyColour = fromPalette (panel::colour::control);
            entry.button->setColour (juce::TextButton::buttonColourId, keyColour);
            entry.button->setColour (juce::TextButton::buttonOnColourId,
                                     keyColour.brighter (0.08f));
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
                                      parameters::legacyChorus) == 0)
                {
                    attachChorusOffButton (*entry.button);
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
        entry.label->setFont (panelFont (panel::labelPointSize));
        entry.label->setColour (juce::Label::textColourId,
                                fromPalette (panel::colour::text));
        entry.label->setJustificationType (juce::Justification::centred);
        entry.label->setTooltip (description.tooltip);
        entry.label->setInterceptsMouseClicks (false, false);
        // A stacked button already carries its own legend; repeating it under
        // the stack would just be clutter.
        if (description.kind == panel::ControlKind::Slider
            || description.kind == panel::ControlKind::Knob
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
        "Adds MIDI-velocity response to each voice amplifier and to the "
        "envelope amount reaching its filter, so quieter notes are darker. "
        "Zero matches the hardware's fixed velocity; 100% gives full dynamic "
        "response.",
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
    // HISS now lives inside the primary CHORUS block, so its full-size knob
    // matches that tier rather than the smaller lower-bay utilities.
    chorusNoiseSlider.getProperties().set (secondaryStyleProperty, false);

    const char* captions[] = { "AMOUNT", "TUNE", "VELOCITY",
                               "CHARACTER", "HISS", "VOICES" };
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
    utilityLabels[4].setColour (juce::Label::textColourId,
                                fromPalette (panel::colour::text));

    const auto nameButton = [] (juce::TextButton& button)
    {
        button.setName (button.getButtonText());
        button.setTitle (button.getButtonText());
    };
    nameButton (panicButton);
    nameButton (randomize1Button);
    nameButton (randomize10Button);
    nameButton (randomize50Button);
    nameButton (resetButton);
    nameButton (unisonButton);
    portamentoToggleButton.setName ("Portamento switch");
    portamentoToggleButton.setTitle ("Portamento switch");
    syxLoadButton.setName ("Load patch file");
    syxLoadButton.setTitle ("Load patch file");
    syxSaveButton.setName ("Save patch file");
    syxSaveButton.setTitle ("Save patch file");

    // The internal-rate ladder. The tooltip names the cost as well as the
    // benefit, because this is the one control a player reaches for when a
    // session runs out of CPU rather than when it needs a different sound.
    const juce::String qualityTooltip =
        "Sets how far above the host's sample rate the oscillators, nonlinear "
        "filter, amplifiers and chorus run. 4x aliases least and costs the "
        "most; 1x is the cheapest. A change waits until the instrument is "
        "idle, and at high host rates a lower factor is used automatically. "
        "This has no hardware counterpart and is not part of a patch.";
    for (int choice = 0; choice < YouKnow106AudioProcessor::qualityChoiceCount;
         ++choice)
        qualityBox.addItem (
            juce::String (
                YouKnow106AudioProcessor::oversamplingFactorForChoice (choice))
                + "x",
            choice + 1);
    qualityBox.setName ("Quality");
    qualityBox.setTitle ("Quality");
    qualityBox.setTooltip (qualityTooltip);
    qualityBox.setColour (juce::ComboBox::backgroundColourId,
                          fromPalette (panel::colour::slot));
    qualityBox.setColour (juce::ComboBox::textColourId,
                          fromPalette (panel::colour::text));
    qualityBox.setColour (juce::ComboBox::outlineColourId,
                          fromPalette (panel::colour::controlShadow));
    qualityBox.setColour (juce::ComboBox::arrowColourId,
                          fromPalette (panel::colour::cyan));
    addAndMakeVisible (qualityBox);
    comboBoxAttachments.push_back (std::make_unique<ComboBoxAttachment> (
        audioProcessor.parameters, quality, qualityBox));

    qualityLabel.setText ("QUALITY", juce::dontSendNotification);
    qualityLabel.setFont (panelFont (11.0f));
    qualityLabel.setColour (juce::Label::textColourId,
                            fromPalette (panel::colour::textDim).withAlpha (0.90f));
    qualityLabel.setJustificationType (juce::Justification::centred);
    qualityLabel.setName ("Quality label");
    qualityLabel.setTooltip (qualityTooltip);
    qualityLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (qualityLabel);

    unisonButton.setClickingTogglesState (false);
    unisonButton.getProperties().set (hardwareStyleProperty, true);
    unisonButton.getProperties().set (hardwareKeyCentreProperty, 0.62);
    unisonButton.setColour (juce::TextButton::buttonColourId,
                            fromPalette (panel::colour::control));
    unisonButton.setColour (juce::TextButton::buttonOnColourId,
                            fromPalette (panel::colour::control).brighter (0.08f));
    unisonButton.setTooltip (
        "Convenience key for pressing both original POLY contacts together "
        "to enter Solo Unison; the hardware has no separate Unison key.");
    addAndMakeVisible (unisonButton);
    attachUnisonButton (unisonButton);

    portamentoToggleButton.setClickingTogglesState (false);
    portamentoToggleButton.getProperties().set (hardwareStyleProperty, true);
    portamentoToggleButton.getProperties().set (hardwareKeyCentreProperty, 0.62);
    portamentoToggleButton.setColour (juce::TextButton::buttonColourId,
                                      fromPalette (panel::colour::control));
    portamentoToggleButton.setTooltip (
        "Switches portamento off or restores its last non-zero time, matching "
        "the separate switch beside the original Portamento Time knob.");
    addAndMakeVisible (portamentoToggleButton);
    attachPortamentoToggle (portamentoToggleButton);

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
        "and plug-in-extension control to its default. QUALITY is a setting "
        "for this machine rather than part of a patch and stays where you "
        "left it.");
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
    for (auto* button : { &panicButton, &resetButton, &randomize1Button,
                          &randomize10Button, &randomize50Button })
        button->getProperties().set (compactStyleProperty, true);
    panicButton.getProperties().set (actionIconProperty, "stop");
    resetButton.getProperties().set (actionIconProperty, "reset");
    randomize1Button.getProperties().set (actionIconProperty, "random");
    randomize10Button.getProperties().set (actionIconProperty, "random");
    randomize50Button.getProperties().set (actionIconProperty, "random");
}

void YouKnow106AudioProcessorEditor::buildPresetBar()
{
    presetLabel.setText ("PRESET", juce::dontSendNotification);
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
        "host recall restore the tone plus its complete performance and plug-in "
        "setup, except QUALITY, which is a setting for this machine rather than "
        "part of a patch.");
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
    presetReloadButton.getProperties().set (actionIconProperty, "reload");
    presetReloadButton.setTooltip (
        "Reloads the selected program exactly and discards all tone, "
        "performance and plug-in-control edits, apart from QUALITY, which "
        "is a setting for this machine rather than part of a patch.");
    presetReloadButton.onClick = [this]
    {
        selectProgram (audioProcessor.getCurrentProgram());
    };
    addAndMakeVisible (presetReloadButton);

    presetEditedLabel.setText ("EDITED", juce::dontSendNotification);
    presetEditedLabel.setFont (panelFont (11.0f, true));
    presetEditedLabel.setColour (juce::Label::textColourId,
                                 fromPalette (panel::colour::text));
    presetEditedLabel.setTooltip (
        "Lights when the current panel no longer matches the selected program.");
    presetEditedLabel.setName ("Program state");
    presetEditedLabel.getProperties().set (statusLampStyleProperty, true);
    presetEditedLabel.setJustificationType (juce::Justification::centredLeft);
    presetEditedLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetEditedLabel);

    customPatchLabel.setText ("PATCH FILE", juce::dontSendNotification);
    customPatchLabel.setFont (panelFont (11.0f, true));
    customPatchLabel.setColour (juce::Label::textColourId,
                                fromPalette (panel::colour::cyan));
    customPatchLabel.setTooltip (
        "Loads or saves one editable hardware-compatible .syx patch file.");
    customPatchLabel.setJustificationType (juce::Justification::centredRight);
    customPatchLabel.setName ("Custom patch label");
    customPatchLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (customPatchLabel);

    const auto configureCustomPatchButton = [this] (juce::TextButton& button,
                                                     const juce::String& name,
                                                     const juce::String& tooltip,
                                                     auto action)
    {
        button.setName (name);
        button.setTitle (name);
        button.setTooltip (tooltip);
        button.getProperties().set (compactStyleProperty, true);
        button.onClick = std::move (action);
        addAndMakeVisible (button);
    };
    configureCustomPatchButton (
        customPatchLoadButton, "Load custom patch", syxLoadButton.getTooltip(),
        [this] { chooseAndImportPatchFile(); });
    configureCustomPatchButton (
        customPatchSaveButton, "Save custom patch", syxSaveButton.getTooltip(),
        [this] { chooseAndExportPatchFile(); });
    customPatchLoadButton.getProperties().set (actionIconProperty, "load");
    customPatchSaveButton.getProperties().set (actionIconProperty, "save");

    shownProgram = -1;
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::buildHardwareProgrammer()
{
    const auto configureKey = [this] (juce::TextButton& button,
                                      juce::Colour colour,
                                      const juce::String& name,
                                      const juce::String& tooltip)
    {
        button.setName (name);
        button.setTitle (name);
        button.setTooltip (tooltip);
        button.setClickingTogglesState (false);
        button.getProperties().set (hardwareStyleProperty, true);
        button.getProperties().set (hardwareKeyCentreProperty, 0.72);
        button.setColour (juce::TextButton::buttonColourId, colour);
        button.setColour (juce::TextButton::buttonOnColourId,
                          colour.brighter (0.08f));
        addAndMakeVisible (button);
    };

    const auto memory = fromPalette (panel::colour::cyan);
    const auto neutral = fromPalette (panel::colour::control);

    configureKey (keyTransposeButton, neutral, "Key transpose",
                  "Switches the selected keyboard transposition on or off. "
                  "Choose its semitone amount with the adjacent AMOUNT knob.");
    attachKeyTransposeButton (keyTransposeButton);

    for (int index = 0; index < static_cast<int> (groupButtons.size()); ++index)
    {
        auto& button = groupButtons[static_cast<std::size_t> (index)];
        const auto groupName = juce::String::charToString (
            static_cast<juce::juce_wchar> ('A' + index));
        button.setButtonText (groupName);
        configureKey (button, memory, "Group " + groupName,
                      "Selects factory-memory group " + groupName
                          + " while retaining the chosen bank and patch number.");
        button.onClick = [this, index]
        {
            selectedHardwareGroup = index;
            selectHardwareProgram();
        };
    }

    for (int index = 0; index < static_cast<int> (bankButtons.size()); ++index)
    {
        auto& button = bankButtons[static_cast<std::size_t> (index)];
        const auto number = juce::String (index + 1);
        button.setButtonText (number);
        configureKey (button, memory, "Bank " + number,
                      "Selects factory-memory bank " + number
                          + " while retaining the current group and patch number.");
        button.onClick = [this, index]
        {
            selectedHardwareBank = index;
            selectHardwareProgram();
        };
    }

    for (int index = 0; index < static_cast<int> (patchButtons.size()); ++index)
    {
        auto& button = patchButtons[static_cast<std::size_t> (index)];
        const auto number = juce::String (index + 1);
        button.setButtonText (number);
        configureKey (button, memory, "Patch " + number,
                      "Loads patch number " + number
                          + " from the selected factory group and bank.");
        button.onClick = [this, index]
        {
            selectedHardwarePatch = index;
            selectHardwareProgram();
        };
    }

    hardwarePatchDisplay.setName ("Bank and patch display");
    hardwarePatchDisplay.setTitle ("Bank and patch display");
    hardwarePatchDisplay.setTooltip (
        "Shows the selected two-digit bank and patch location, from 11 to 88.");
    hardwarePatchDisplay.setJustificationType (juce::Justification::centred);
    hardwarePatchDisplay.setFont (juce::Font (
        juce::FontOptions (25.0f, juce::Font::bold)));
    hardwarePatchDisplay.setColour (juce::Label::textColourId,
                                    fromPalette (panel::colour::led));
    hardwarePatchDisplay.setColour (juce::Label::backgroundColourId,
                                    fromPalette (panel::colour::scope));
    hardwarePatchDisplay.getProperties().set (segmentDisplayStyleProperty, true);
    hardwarePatchDisplay.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (hardwarePatchDisplay);

    configureKey (manualButton, neutral, "Manual mode",
                  "Selects the INIT/manual panel state instead of a stored "
                  "factory memory location.");
    manualButton.onClick = [this] { selectProgram (0); };

    configureKey (writeButton, neutral, "Write memory",
                  "The original WRITE key is shown here; the bundled factory "
                  "bank is read-only, so host presets store edited states.");
    writeButton.setEnabled (false);

    configureKey (syxSaveButton, neutral, "Save patch file",
                  syxSaveButton.getTooltip());
    configureKey (verifyButton, neutral, "Verify tape data",
                  "The original tape VERIFY key is shown here; file integrity "
                  "is checked automatically when a SysEx file is loaded.");
    verifyButton.setEnabled (false);
    configureKey (syxLoadButton, neutral, "Load patch file",
                  syxLoadButton.getTooltip());

    shownProgram = -1;
    refreshPresetBar();
}

void YouKnow106AudioProcessorEditor::selectHardwareProgram()
{
    const int program = 1 + selectedHardwareGroup * 64
                          + selectedHardwareBank * 8
                          + selectedHardwarePatch;
    selectProgram (program);
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
    presetEditedLabel.setText (edited ? "EDITED" : "LOADED",
                               juce::dontSendNotification);
    presetEditedLabel.getProperties().set (statusLampOnProperty, edited);
    presetEditedLabel.setColour (
        juce::Label::textColourId,
        edited ? fromPalette (panel::colour::text)
               : fromPalette (panel::colour::textDim).withAlpha (0.76f));
    presetEditedLabel.setVisible (true);
    presetPrevButton.setEnabled (program > 0);
    presetNextButton.setEnabled (program < audioProcessor.getNumPrograms() - 1);

    manualButton.setToggleState (program == 0, juce::dontSendNotification);
    if (program == 0)
    {
        hardwarePatchDisplay.setText ("--", juce::dontSendNotification);
        for (auto& button : groupButtons)
            button.setToggleState (false, juce::dontSendNotification);
        for (auto& button : bankButtons)
            button.setToggleState (false, juce::dontSendNotification);
        for (auto& button : patchButtons)
            button.setToggleState (false, juce::dontSendNotification);
        return;
    }

    const int memoryIndex = program - 1;
    selectedHardwareGroup = memoryIndex / 64;
    selectedHardwareBank = (memoryIndex % 64) / 8;
    selectedHardwarePatch = memoryIndex % 8;
    for (int index = 0; index < static_cast<int> (groupButtons.size()); ++index)
        groupButtons[static_cast<std::size_t> (index)].setToggleState (
            index == selectedHardwareGroup, juce::dontSendNotification);
    for (int index = 0; index < static_cast<int> (bankButtons.size()); ++index)
        bankButtons[static_cast<std::size_t> (index)].setToggleState (
            index == selectedHardwareBank, juce::dontSendNotification);
    for (int index = 0; index < static_cast<int> (patchButtons.size()); ++index)
        patchButtons[static_cast<std::size_t> (index)].setToggleState (
            index == selectedHardwarePatch, juce::dontSendNotification);

    hardwarePatchDisplay.setText (
        juce::String (selectedHardwareBank + 1)
            + juce::String (selectedHardwarePatch + 1),
        juce::dontSendNotification);
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

    // Each original POLY lamp follows its contact. Solo Unison closes both, so
    // both hardware lamps light; the separate extension key mirrors that pair.
    // Both parameters can still refresh the presentation during host recall.
    const auto refresh = [this, &button, parameterId] (float)
    {
        const auto isOn = [this] (const char* id)
        {
            auto* parameter = audioProcessor.parameters.getParameter (id);
            return parameter != nullptr && parameter->getValue() > 0.5f;
        };
        button.setToggleState (isOn (parameterId),
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

void YouKnow106AudioProcessorEditor::attachChorusOffButton (juce::Button& button)
{
    auto* first = audioProcessor.parameters.getParameter (parameters::chorusI);
    auto* second = audioProcessor.parameters.getParameter (parameters::chorusII);
    jassert (first != nullptr && second != nullptr);
    if (first == nullptr || second == nullptr)
        return;

    button.setClickingTogglesState (false);
    const auto refresh = [this, &button] (float)
    {
        const auto isOn = [this] (const char* id)
        {
            if (const auto* value = audioProcessor.parameters.getRawParameterValue (id))
                return value->load (std::memory_order_relaxed) > 0.5f;
            return false;
        };
        button.setToggleState (! isOn (parameters::chorusI)
                                   && ! isOn (parameters::chorusII),
                               juce::dontSendNotification);
    };

    auto firstAttachment = std::make_unique<juce::ParameterAttachment> (
        *first, refresh, nullptr);
    auto secondAttachment = std::make_unique<juce::ParameterAttachment> (
        *second, refresh, nullptr);
    button.onClick = [this]
    {
        for (const char* id : { parameters::chorusI, parameters::chorusII })
            if (auto* target = audioProcessor.parameters.getParameter (id))
            {
                target->beginChangeGesture();
                target->setValueNotifyingHost (target->convertTo0to1 (0.0f));
                target->endChangeGesture();
            }
    };

    auto* firstPointer = firstAttachment.get();
    auto* secondPointer = secondAttachment.get();
    parameterAttachments.push_back (std::move (firstAttachment));
    parameterAttachments.push_back (std::move (secondAttachment));
    firstPointer->sendInitialUpdate();
    secondPointer->sendInitialUpdate();
}

void YouKnow106AudioProcessorEditor::attachPortamentoToggle (juce::Button& button)
{
    auto* parameter = audioProcessor.parameters.getParameter (parameters::portamento);
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [this, &button] (float current)
        {
            if (current > 1.0e-5f)
                lastPortamentoTime = current;
            button.setToggleState (current > 1.0e-5f,
                                   juce::dontSendNotification);
        },
        nullptr);
    button.onClick = [this, parameter]
    {
        const auto* raw = audioProcessor.parameters.getRawParameterValue (
            parameters::portamento);
        const float current = raw != nullptr
                            ? raw->load (std::memory_order_relaxed) : 0.0f;
        const float wanted = current > 1.0e-5f ? 0.0f : lastPortamentoTime;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (wanted));
        parameter->endChangeGesture();
    };
    auto* pointer = attachment.get();
    parameterAttachments.push_back (std::move (attachment));
    pointer->sendInitialUpdate();
}

void YouKnow106AudioProcessorEditor::attachKeyTransposeButton (juce::Button& button)
{
    auto* parameter = audioProcessor.parameters.getParameter (parameters::transpose);
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [this, &button] (float current)
        {
            const int semitones = juce::roundToInt (current);
            if (semitones != 0)
                lastTransposeSemitones = semitones;
            button.setToggleState (semitones != 0,
                                   juce::dontSendNotification);
        },
        nullptr);
    button.onClick = [this, parameter]
    {
        const auto* raw = audioProcessor.parameters.getRawParameterValue (
            parameters::transpose);
        const int current = raw != nullptr
                          ? juce::roundToInt (raw->load (std::memory_order_relaxed))
                          : 0;
        const float wanted = static_cast<float> (
            current == 0 ? lastTransposeSemitones : 0);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (wanted));
        parameter->endChangeGesture();
    };
    auto* pointer = attachment.get();
    parameterAttachments.push_back (std::move (attachment));
    pointer->sendInitialUpdate();
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
        { &qualityBox,        quality },
        { &unisonButton,      legacyKeyMode },
        { &portamentoToggleButton, portamento },
        { &keyTransposeButton, transpose }
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

    // MODE is one three-state assigner represented by two hardware contacts and
    // a convenience UNISON key. An individual contact can only report On/Off,
    // while the useful value is Poly 1, Poly 2 or Unison, so all three controls
    // print the mode selected by the authoritative pair.
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

    if (isMode (youknow106::parameters::legacyChorus))
    {
        const auto engaged = [this] (const char* id) {
            const auto* value = audioProcessor.parameters.getRawParameterValue (id);
            return value != nullptr
                && value->load (std::memory_order_relaxed) > 0.5f;
        };
        const auto mode = youknow106::chorusModeFor (
            engaged (youknow106::parameters::chorusI),
            engaged (youknow106::parameters::chorusII));
        if (const auto* names = audioProcessor.parameters.getParameter (
                youknow106::parameters::legacyChorus))
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
                juce::jmax (10.5f, panel::labelPointSize * scale), true));
            entry.label->setBounds (
                scaled (description.labelX, description.labelY,
                        description.labelWidth,
                        description.labelHeight).toNearestInt());
        }
    }

    // The left identity field deliberately keeps the negative space of the
    // hardware while using this instrument's own name.
    logoLabel.setFont (panelFont (juce::jmax (27.0f, 36.0f * scale), true));
    editionLabel.setFont (clearPanelFont (
        juce::jmax (13.5f, 18.0f * scale)));
    logoLabel.setBounds (scaled (24.0f, 72.0f, 174.0f, 40.0f).toNearestInt());
    editionLabel.setBounds (scaled (24.0f, 113.0f, 174.0f, 22.0f).toNearestInt());
    // Original programmer tier: group, bank and patch keys remain primary.
    constexpr float programmerKeyTop = 296.0f;
    constexpr float programmerKeyHeight = 68.0f;
    const auto& voiceModeSection = panel::sections()[1];
    const float voiceModeCell = (voiceModeSection.width - panel::sectionPadding)
                              / static_cast<float> (voiceModeSection.slots);
    unisonButton.setBounds (
        scaled (voiceModeSection.x + panel::sectionPadding * 0.5f
                    + 2.0f * voiceModeCell + panel::controlInset,
                programmerKeyTop,
                voiceModeCell - 2.0f * panel::controlInset,
                programmerKeyHeight).toNearestInt());
    for (int index = 0; index < 2; ++index)
        groupButtons[static_cast<std::size_t> (index)].setBounds (
            scaled (470.0f + 38.0f * static_cast<float> (index),
                    programmerKeyTop, 32.0f,
                    programmerKeyHeight).toNearestInt());
    for (int index = 0; index < 8; ++index)
        bankButtons[static_cast<std::size_t> (index)].setBounds (
            scaled (550.0f + 36.0f * static_cast<float> (index),
                    programmerKeyTop, 30.0f,
                    programmerKeyHeight).toNearestInt());
    hardwarePatchDisplay.setFont (juce::Font (
        juce::FontOptions (juce::jmax (20.0f, 25.0f * scale), juce::Font::bold)));
    hardwarePatchDisplay.setBounds (
        scaled (848.0f, 304.0f, 52.0f, 46.0f).toNearestInt());
    for (int index = 0; index < 8; ++index)
        patchButtons[static_cast<std::size_t> (index)].setBounds (
            scaled (916.0f + 36.0f * static_cast<float> (index),
                    programmerKeyTop, 30.0f,
                    programmerKeyHeight).toNearestInt());
    manualButton.setBounds (
        scaled (1221.2f, programmerKeyTop, 48.0f,
                programmerKeyHeight).toNearestInt());
    writeButton.setBounds (
        scaled (1275.6f, programmerKeyTop, 48.0f,
                programmerKeyHeight).toNearestInt());
    syxSaveButton.setBounds (
        scaled (1330.0f, programmerKeyTop, 48.0f,
                programmerKeyHeight).toNearestInt());
    verifyButton.setBounds (
        scaled (1384.4f, programmerKeyTop, 48.0f,
                programmerKeyHeight).toNearestInt());
    syxLoadButton.setBounds (
        scaled (1438.8f, programmerKeyTop, 48.0f,
                programmerKeyHeight).toNearestInt());

    portamentoToggleButton.setBounds (
        scaled (158.0f, 397.0f, 32.0f, 98.0f).toNearestInt());
    performanceLever.setBounds (
        scaled (panel::vectorPadX, 526.0f, panel::vectorPadWidth, 58.0f).toNearestInt());

    keyboard.setBounds (scaled (panel::instrumentLeft, panel::panelHeight,
                                panel::instrumentRight - panel::instrumentLeft,
                                panel::keyboardHeight).toNearestInt());
    // C-to-C contains one more white key than five complete octaves. Fit that
    // exact physical span to the panel at every editor size, with no hidden
    // off-instrument octaves or scrolling.
    keyboard.setKeyWidth (static_cast<float> (keyboard.getWidth())
                          / static_cast<float> (panel::keyboardWhiteKeyCount));

    // Remaining host and model controls share one visually separate add-on bay.
    // UNISON and HISS now live directly in their hardware families above;
    // VOICES and VELOCITY share the continuous-control VOICE group here.
    for (auto& label : utilityLabels)
        label.setFont (panelFont (
            juce::jmax (10.5f, 14.0f * scale), true));
    qualityLabel.setFont (panelFont (
        juce::jmax (10.5f, 14.0f * scale), true));
    utilityLabels[4].setFont (panelFont (
        juce::jmax (10.5f, panel::labelPointSize * scale), true));
    constexpr float labelTop = panel::extensionDeckTop + 32.0f;
    constexpr float knobTop = panel::extensionDeckTop + 52.0f;
    constexpr float labelHeight = 16.0f;
    constexpr float knobSize = 56.0f;

    // MODEL: global circuit character and processing quality sit beneath the
    // global controller cheek rather than impersonating original synth blocks.
    utilityLabels[3].setBounds (
        scaled (22.0f, labelTop, 80.0f, labelHeight).toNearestInt());
    calibrationSlider.setBounds (
        scaled (33.0f, knobTop, knobSize, knobSize).toNearestInt());
    qualityLabel.setBounds (
        scaled (110.0f, labelTop, 86.0f, labelHeight).toNearestInt());
    qualityBox.setBounds (
        scaled (114.0f, knobTop + 16.0f, 78.0f, 24.0f).toNearestInt());

    // VOICE: the continuous voice-limit and response controls share a baseline.
    utilityLabels[5].setBounds (
        scaled (228.0f, labelTop, 80.0f, labelHeight).toNearestInt());
    polyphonySlider.setBounds (
        scaled (240.0f, knobTop, knobSize, knobSize).toNearestInt());
    utilityLabels[2].setBounds (
        scaled (318.0f, labelTop, 80.0f, labelHeight).toNearestInt());
    velocitySlider.setBounds (
        scaled (330.0f, knobTop, knobSize, knobSize).toNearestInt());

    // PITCH follows the DCO footprint. Keep the hardware-style Transpose
    // switch next to its amount, then the fine tune control.
    keyTransposeButton.setBounds (
        scaled (428.0f, labelTop + 3.0f, 82.0f, 81.0f).toNearestInt());
    utilityLabels[0].setBounds (
        scaled (518.0f, labelTop, 62.0f, labelHeight).toNearestInt());
    transposeSlider.setBounds (
        scaled (521.0f, knobTop, knobSize, knobSize).toNearestInt());
    utilityLabels[1].setBounds (
        scaled (588.0f, labelTop, 56.0f, labelHeight).toNearestInt());
    tuneSlider.setBounds (
        scaled (588.0f, knobTop, knobSize, knobSize).toNearestInt());

    // The monitor stays beside voice allocation.
    display.setBounds (
        scaled (panel::monitorZoneX + 10.0f,
                panel::extensionDeckTop + 23.0f,
                panel::monitorZoneWidth - 20.0f,
                panel::extensionDeckHeight - 35.0f).toNearestInt());

    // HISS is a full-size Chorus control beside the vertical OFF/I/II stack.
    const auto& chorusSection = panel::sections()[8];
    const float chorusCell = (chorusSection.width - panel::sectionPadding)
                           / static_cast<float> (chorusSection.slots);
    const float hissSlotX = chorusSection.x + panel::sectionPadding * 0.5f
                          + chorusCell;
    const float soundLabelTop = chorusSection.y + 99.0f;
    constexpr float hissLabelHeight = 16.0f;
    constexpr float hissKnobSize = 50.0f;
    const float hissKnobX = hissSlotX + (chorusCell - hissKnobSize) * 0.5f;
    utilityLabels[4].setBounds (
        scaled (hissKnobX, soundLabelTop, hissKnobSize,
                hissLabelHeight).toNearestInt());
    chorusNoiseSlider.setBounds (
        scaled (hissKnobX, soundLabelTop + hissLabelHeight + 8.0f,
                hissKnobSize, hissKnobSize).toNearestInt());

    // The host-friendly navigator gets its own rail immediately under the
    // physical A/B, BANK and PATCH keys instead of competing with aux knobs.
    constexpr float presetY = panel::presetTop + 4.0f;
    presetLabel.setFont (clearPanelFont (
        juce::jmax (10.5f, 12.5f * scale), true));
    presetEditedLabel.setFont (clearPanelFont (
        juce::jmax (10.5f, 12.5f * scale), true));
    customPatchLabel.setFont (clearPanelFont (
        juce::jmax (10.5f, 12.5f * scale), true));
    constexpr float presetControlHeight = 24.0f;
    presetLabel.setBounds (
        scaled (226.0f, presetY, 104.0f,
                presetControlHeight).toNearestInt());
    presetPrevButton.setBounds (
        scaled (340.0f, presetY, 30.0f,
                presetControlHeight).toNearestInt());
    presetNextButton.setBounds (
        scaled (378.0f, presetY, 30.0f,
                presetControlHeight).toNearestInt());
    presetReloadButton.setBounds (
        scaled (416.0f, presetY, 94.0f,
                presetControlHeight).toNearestInt());
    presetBox.setBounds (
        scaled (550.0f, presetY, 410.0f,
                presetControlHeight).toNearestInt());
    presetEditedLabel.setBounds (
        scaled (970.0f, presetY, 80.0f,
                presetControlHeight).toNearestInt());
    customPatchLabel.setBounds (
        scaled (1060.0f, presetY, 116.0f,
                presetControlHeight).toNearestInt());
    customPatchLoadButton.setBounds (
        scaled (1186.0f, presetY, 146.0f,
                presetControlHeight).toNearestInt());
    customPatchSaveButton.setBounds (
        scaled (1342.0f, presetY, 148.0f,
                presetControlHeight).toNearestInt());

    constexpr float operationTop = panel::extensionDeckTop + 54.0f;
    juce::Button* operationButtons[] = { &panicButton, &resetButton,
                                         &randomize1Button, &randomize10Button,
                                         &randomize50Button };
    constexpr float operationX[] = { 1074.0f, 1162.0f, 1262.0f,
                                     1342.0f, 1422.0f };
    constexpr float operationWidth[] = { 80.0f, 80.0f, 72.0f, 72.0f, 72.0f };
    for (int index = 0; index < 5; ++index)
        operationButtons[index]->setBounds (
            scaled (operationX[index], operationTop, operationWidth[index],
                    42.0f).toNearestInt());

    contextHelp.setBounds (
        scaled (panel::panelMargin,
                panel::extensionDeckTop + panel::extensionDeckHeight
                    + panel::helpStripGap,
                panel::panelWidth() - 2.0f * panel::panelMargin,
                panel::helpStripHeight).toNearestInt());
}

void YouKnow106AudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    texture.fill (g, bounds, fromPalette (panel::colour::faceplate));

    const auto red = fromPalette (panel::colour::magenta);
    const auto ink = fromPalette (panel::colour::text);
    const auto hairline = ink.withAlpha (0.34f);

    // One continuous metal faceplate, with planar dividers and rails rather
    // than a dashboard of rounded cards.
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.42f));
    g.fillRect (scaled (panel::instrumentLeft, panel::soundRowTop,
                        panel::instrumentRight - panel::instrumentLeft,
                        panel::soundRowHeight));
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.68f));
    g.fillRect (scaled (panel::instrumentLeft, panel::performanceDeckTop,
                        panel::instrumentRight - panel::instrumentLeft,
                        panel::programmerHeight));
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.76f));
    g.fillRect (scaled (panel::controllerX, panel::performanceDeckTop,
                        panel::controllerWidth, panel::performanceDeckHeight));

    // One broad enamel inlay gives the identity field its vintage accent
    // without stacking decorative rules above the wordmark.
    const auto identityBand = scaled (panel::controllerX, 64.0f,
                                      panel::controllerWidth, 74.0f);
    juce::ColourGradient identityGradient (
        red.darker (0.48f).withAlpha (0.62f),
        identityBand.getX(), identityBand.getY(),
        fromPalette (panel::colour::faceplateLow).withAlpha (0.78f),
        identityBand.getX(), identityBand.getBottom(), false);
    g.setGradientFill (identityGradient);
    g.fillRect (identityBand);
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.86f));
    g.setFont (clearPanelFont (juce::jmax (10.0f, 12.5f * scale), true));
    g.drawFittedText ("PROGRAMMABLE\nSIX-VOICE\nSYNTHESIZER",
                      scaled (24.0f, 145.0f, 174.0f, 54.0f).toNearestInt(),
                      juce::Justification::centredLeft, 3, 1.0f);

    const auto& sections = panel::sections();
    for (const auto& section : sections)
    {
        if (section.y >= panel::performanceDeckTop)
            continue;

        const auto header = scaled (section.x, section.y, section.width,
                                    panel::headerHeight);
        juce::ColourGradient headerGradient (
            red.darker (0.52f).withAlpha (0.58f),
            header.getX(), header.getY(),
            fromPalette (panel::colour::faceplateLow).withAlpha (0.88f),
            header.getX(), header.getBottom(), false);
        g.setGradientFill (headerGradient);
        g.fillRect (header);

        const char* title = section.displayTitle;
        const char* code = section.displayCode;

        auto titleArea = header.reduced (5.0f * scale, 0.0f);
        juce::Rectangle<float> codeArea;
        if (code[0] != '\0')
        {
            codeArea = titleArea.removeFromRight (
                juce::jmin (40.0f * scale, titleArea.getWidth() * 0.28f));
            titleArea.removeFromRight (4.0f * scale);
        }

        float titleSize = juce::jmax (11.0f,
                                      panel::headerPointSize * scale);
        const float titleWidth = juce::GlyphArrangement::getStringWidth (
            panelFont (titleSize, true), title);
        if (titleWidth > titleArea.getWidth() && titleWidth > 0.0f)
            titleSize *= titleArea.getWidth() / titleWidth;
        g.setColour (ink);
        g.setFont (panelFont (juce::jmax (9.5f, titleSize), true));
        g.drawText (title, titleArea.toNearestInt(),
                    juce::Justification::centredLeft, false);
        if (! codeArea.isEmpty())
        {
            g.setColour (fromPalette (panel::colour::brassHigh).withAlpha (0.86f));
            g.setFont (clearPanelFont (juce::jmax (7.5f, 9.0f * scale), true));
            g.drawText (code, codeArea.toNearestInt(),
                        juce::Justification::centredRight, false);
        }

        g.setColour (hairline);
        g.fillRect (scaled (section.x - 1.0f, section.y,
                            1.0f, section.height));
    }

    // Quiet internal dividers make the dense oscillator and filter families
    // read as related sub-groups without adding another row of tiny captions.
    const auto subgroupDividers = [&] (const panel::Section& section,
                                        std::initializer_list<int> afterSlots)
    {
        const float cell = (section.width - panel::sectionPadding)
                         / static_cast<float> (section.slots);
        for (const int slot : afterSlots)
        {
            const float x = section.x + panel::sectionPadding * 0.5f
                          + cell * static_cast<float> (slot);
            g.setColour (fromPalette (panel::colour::brass).withAlpha (0.14f));
            g.fillRect (scaled (x, section.y + panel::headerHeight + 7.0f,
                                1.0f,
                                section.height - panel::headerHeight - 15.0f));
        }
    };
    subgroupDividers (sections[3], { 1, 4, 5 });
    subgroupDividers (sections[5], { 2, 4 });
    subgroupDividers (sections[6], { 2 });

    g.setColour (hairline);
    g.fillRect (scaled (panel::instrumentRight - 1.0f, panel::soundRowTop,
                        1.0f, panel::soundRowHeight));

    // Quiet captions are enough to group the programmer; the surrounding
    // sound row and preset rail already provide its two structural edges.
    const auto programmerCaption = [&] (const char* text, float x, float width)
    {
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.92f));
        float captionSize = juce::jmax (10.5f, 13.0f * scale);
        const float captionWidth = juce::GlyphArrangement::getStringWidth (
            clearPanelFont (captionSize, true), text);
        const float availableWidth = (width - 4.0f) * scale;
        if (captionWidth > availableWidth && captionWidth > 0.0f)
            captionSize *= availableWidth / captionWidth;
        g.setFont (clearPanelFont (juce::jmax (9.0f, captionSize), true));
        g.drawText (text,
                    scaled (x, panel::performanceDeckTop + 9.0f,
                            width, 14.0f).toNearestInt(),
                    juce::Justification::centred, false);
    };
    programmerCaption ("VOICE MODE", 218.0f, 242.0f);
    programmerCaption ("GROUP", 470.0f, 70.0f);
    programmerCaption ("BANK", 550.0f, 282.0f);
    programmerCaption ("PROGRAM", 842.0f, 64.0f);
    programmerCaption ("PATCH", 916.0f, 282.0f);
    programmerCaption ("DATA", 1218.0f, 272.0f);

    // The two-digit memory readout sits behind smoked glass in a recessed
    // bezel. The label above supplies the red segments; this paint supplies
    // the physical cavity even when INIT shows only two dashes.
    const auto memoryBezel = scaled (842.0f, 296.0f, 64.0f, 68.0f);
    g.setColour (juce::Colours::black.withAlpha (0.84f));
    g.fillRoundedRectangle (memoryBezel.translated (0.0f, 1.5f * scale),
                            3.0f * scale);
    juce::ColourGradient memoryBezelGradient (
        fromPalette (panel::colour::brassHigh).withAlpha (0.44f),
        memoryBezel.getX(), memoryBezel.getY(),
        fromPalette (panel::colour::slot),
        memoryBezel.getRight(), memoryBezel.getBottom(), false);
    g.setGradientFill (memoryBezelGradient);
    g.fillRoundedRectangle (memoryBezel.reduced (1.0f * scale), 2.5f * scale);
    const auto memoryGlass = memoryBezel.reduced (6.0f * scale, 8.0f * scale);
    g.setColour (fromPalette (panel::colour::scope));
    g.fillRoundedRectangle (memoryGlass, 2.0f * scale);
    g.setColour (fromPalette (panel::colour::led).withAlpha (0.10f));
    g.drawRoundedRectangle (memoryGlass, 2.0f * scale,
                            juce::jmax (1.0f, scale));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (memoryGlass.getX() + 1.0f, memoryGlass.getY() + 1.0f,
                memoryGlass.getRight() - 1.0f, memoryGlass.getY() + 1.0f,
                juce::jmax (1.0f, scale));

    // Modern host navigation is adjacent to the memory keys. A deeper tonal
    // band distinguishes it without adding top and bottom rules.
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.94f));
    g.fillRect (scaled (panel::instrumentLeft, panel::presetTop,
                        panel::instrumentRight - panel::instrumentLeft,
                        panel::presetHeight));
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.76f));
    g.setFont (clearPanelFont (juce::jmax (8.0f, 9.0f * scale), true));
    g.fillRect (scaled (1055.0f, panel::presetTop + 5.0f,
                        1.0f, panel::presetHeight - 8.0f));

    // Controller cheek legends and moulded end strips.
    g.setColour (juce::Colours::black.withAlpha (0.52f));
    g.fillRect (scaled (0.0f, panel::performanceDeckTop,
                        panel::controllerX, panel::performanceDeckHeight));
    g.fillRect (scaled (panel::controllerX + panel::controllerWidth,
                        panel::performanceDeckTop,
                        panel::instrumentLeft - panel::controllerX
                            - panel::controllerWidth,
                        panel::performanceDeckHeight));
    // Remaining modern controls share one calm lower surface. UNISON and HISS
    // have moved into the related hardware groups rather than being repeated.
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    const float lowerSurfaceTop = panel::panelHeight + panel::keyboardHeight;
    g.fillRect (scaled (0.0f, lowerSurfaceTop, panel::panelWidth(),
                        panel::extensionDeckTop + panel::extensionDeckHeight
                            + 4.0f - lowerSurfaceTop));
    const auto deck = scaled (panel::panelMargin, panel::extensionDeckTop,
                              panel::panelWidth() - 2.0f * panel::panelMargin,
                              panel::extensionDeckHeight);
    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.92f));
    g.fillRoundedRectangle (deck, juce::jmax (2.5f, 4.0f * scale));

    // Repeat the synthesis strip's planar section language in the extension
    // bay: one shared surface, padded groups, and quiet gray separators.
    const float extensionDividers[] = {
        (panel::modelZoneX + panel::modelZoneWidth + panel::voiceZoneX) * 0.5f,
        (panel::voiceZoneX + panel::voiceZoneWidth + panel::pitchZoneX) * 0.5f,
        (panel::pitchZoneX + panel::pitchZoneWidth + panel::monitorZoneX) * 0.5f,
        (panel::monitorZoneX + panel::monitorZoneWidth
            + panel::operationsBarX) * 0.5f,
        panel::operationsGroupSplitX,
    };
    g.setColour (surfaceBorderColour());
    for (const float x : extensionDividers)
        g.fillRect (scaled (x, panel::extensionDeckTop + 8.0f, 1.0f,
                            panel::extensionDeckHeight - 16.0f));

    const auto drawExtensionHeading = [&] (float x, float width,
                                            const char* title)
    {
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.82f));
        g.setFont (clearPanelFont (juce::jmax (10.5f, 13.0f * scale), true));
        g.drawText (title,
                    scaled (x + 10.0f, panel::extensionDeckTop + 7.0f,
                            width - 20.0f, 14.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    };
    drawExtensionHeading (panel::modelZoneX, panel::modelZoneWidth, "MODEL");
    drawExtensionHeading (panel::voiceZoneX, panel::voiceZoneWidth, "VOICE");
    drawExtensionHeading (panel::pitchZoneX, panel::pitchZoneWidth, "PITCH");
    drawExtensionHeading (panel::monitorZoneX, panel::monitorZoneWidth,
                          "MONITOR");
    drawExtensionHeading (panel::operationsBarX,
                          panel::operationsGroupSplitX
                              - panel::operationsBarX,
                          "SESSION");
    drawExtensionHeading (panel::operationsGroupSplitX,
                          panel::operationsBarX + panel::operationsBarWidth
                              - panel::operationsGroupSplitX,
                          "VARIATION");
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
    // A focus change without pointer motion is keyboard traversal, so the
    // focused control gets the same explanation and exact value as hover.
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto* hovered = mouse.isTouch() ? nullptr : mouse.getComponentUnderMouse();
    const auto mousePosition = mouse.getScreenPosition();
    const bool mouseMoved = mousePosition != lastMouseScreenPosition;
    lastMouseScreenPosition = mousePosition;
    refreshContextHelp (hovered, juce::Component::getCurrentlyFocusedComponent(),
                        mouseMoved);
}

bool YouKnow106AudioProcessorEditor::keyPressed (const juce::KeyPress&,
                                                 juce::Component* source)
{
    if (source == this || isParentOf (source))
    {
        lastFocusedHelpComponent = source;
        contextHelpFollowsKeyboardFocus = true;
        contextHelp.showFor (source, parameterValueTextFor (source));
    }

    // Observe the gesture without consuming it; the focused JUCE control keeps
    // its native Tab, arrow, Space and Return behaviour.
    return false;
}

void YouKnow106AudioProcessorEditor::refreshContextHelp (
    juce::Component* hovered, juce::Component* focused, bool mouseMoved)
{
    const auto belongsToEditor = [this] (const juce::Component* component)
    {
        return component != nullptr
            && (component == this || isParentOf (component));
    };

    if (mouseMoved)
        contextHelpFollowsKeyboardFocus = false;

    if (focused != lastFocusedHelpComponent.getComponent())
    {
        lastFocusedHelpComponent = focused;
        contextHelpFollowsKeyboardFocus = belongsToEditor (focused);
    }

    auto* target = contextHelpFollowsKeyboardFocus && belongsToEditor (focused)
        ? focused : hovered;

    if (belongsToEditor (target))
        contextHelp.showFor (target, parameterValueTextFor (target));
    else
        contextHelp.showIdle();
}

#include "PluginEditor.h"

#include <BinaryData.h>

#include <cmath>
#include <cstring>
#include <utility>

namespace
{
using namespace youknow;

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

// Slightly tracked headings distinguish groups from the compact control labels.
juce::Font headingFont (float height, bool bold = false)
{
    auto font = clearPanelFont (height, bold);
    font.setExtraKerningFactor (0.045f);
    return font;
}

constexpr auto compactStyleProperty = "compactStyle";
constexpr auto secondaryStyleProperty = "secondaryStyle";
constexpr auto hardwareStyleProperty = "hardwareStyle";
constexpr auto actionIconProperty = "actionIcon";
constexpr auto segmentDisplayStyleProperty = "segmentDisplayStyle";
constexpr auto statusLampStyleProperty = "statusLampStyle";
constexpr auto statusLampOnProperty = "statusLampOn";

// The fixed help strip. Its body is one column of running text, and it is the
// only place a control's explanation appears -- there is no floating tooltip
// window -- so it must print the whole of it.
constexpr int helpBodyMaximumLines = 3;

juce::Colour surfaceBorderColour() noexcept
{
    return fromPalette (panel::colour::edge).withAlpha (0.25f);
}

void drawIndicator (juce::Graphics& g, juce::Rectangle<float> lens, bool on,
                    juce::Colour activeColour, float alpha = 1.0f)
{
    if (lens.isEmpty())
        return;

    const float diameter = juce::jmin (lens.getWidth(), lens.getHeight());
    lens = juce::Rectangle<float> (diameter, diameter).withCentre (lens.getCentre());
    g.setColour ((on ? activeColour : fromPalette (panel::colour::ledDim))
                     .withMultipliedAlpha (alpha));
    g.fillEllipse (lens.reduced (diameter * 0.10f));
}

void drawActionIcon (juce::Graphics& g, juce::Rectangle<float> area,
                     const juce::String& icon, juce::Colour colour)
{
    area = area.reduced (juce::jmax (1.0f, area.getWidth() * 0.08f));
    if (area.isEmpty())
        return;

    const float stroke = juce::jmax (1.2f, area.getHeight() * 0.10f);
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

struct HardwareKeyLayout
{
    juce::Rectangle<float> legend;
    juce::Rectangle<float> lamp;
    juce::Rectangle<float> key;
};

HardwareKeyLayout hardwareKeyLayout (const juce::Button& button,
                                     juce::Rectangle<float> bounds,
                                     float editorScale)
{
    const float sizeScale = editorScale / panel::defaultEditorScale;
    const bool footRegister = isFootRegisterLegend (button.getButtonText());
    const float keyFaceHeight = juce::jlimit (10.0f * sizeScale, 22.0f * sizeScale,
                                              bounds.getHeight() * 0.30f);
    const float lampSize = juce::jlimit (5.6f * sizeScale, 7.2f * sizeScale,
                                         7.0f * editorScale);
    const float legendHeight = footRegister
        ? juce::jlimit (17.0f * sizeScale, 20.0f * sizeScale, bounds.getHeight() * 0.38f)
        : juce::jlimit (10.0f * sizeScale, 22.0f * sizeScale, bounds.getHeight() * 0.29f);
    const float gap = 2.0f * editorScale;
    const auto legend = bounds.withHeight (legendHeight);
    const float keyTop = legend.getBottom() + gap;
    // Absorb the former lamp row into the key: its bottom stays unchanged,
    // with the indicator centred on the switch face.
    const float keyHeight = keyFaceHeight + lampSize + gap;
    const auto key = juce::Rectangle<float> (
        juce::jmax (8.0f * sizeScale, bounds.getWidth() - 6.0f * sizeScale), keyHeight)
                         .withCentre ({ bounds.getCentreX(),
                                        keyTop + keyHeight * 0.5f });
    const auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                          .withCentre (key.getCentre());
    return { legend, lamp, key };
}

void drawWaveformLegend (juce::Graphics& g, juce::Rectangle<float> area,
                         bool pulse, float sizeScale)
{
    area = area.reduced (juce::jmax (3.0f * sizeScale, area.getWidth() * 0.16f),
                         juce::jmax (2.0f * sizeScale, area.getHeight() * 0.20f));
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

    const float thickness = juce::jlimit (juce::jmax (1.0f, 1.2f * sizeScale),
                                            2.3f * sizeScale, area.getWidth() * 0.045f);
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void drawFootRegisterLegend (juce::Graphics& g, juce::Rectangle<float> area,
                             const juce::String& text, float sizeScale)
{
    const auto number = text.dropLastCharacters (1);
    const float fontHeight = juce::jlimit (14.0f, 16.0f * sizeScale,
                                           area.getHeight() * 0.76f);
    const auto font = panelFont (fontHeight, true);
    const float numberWidth = juce::GlyphArrangement::getStringWidth (font, number);
    const float primeWidth = juce::jmax (2.5f, fontHeight * 0.25f);
    const float groupWidth = numberWidth + primeWidth;
    const float left = area.getCentreX() - groupWidth * 0.5f;
    const float baselineTop = area.getCentreY() - fontHeight * 0.5f;

    g.setFont (font);
    g.drawText (number,
                juce::Rectangle<float> (left, baselineTop, numberWidth + sizeScale,
                                        fontHeight + 2.0f * sizeScale).toNearestInt(),
                juce::Justification::centredLeft, false);
    const float primeX = left + numberWidth + sizeScale;
    g.drawLine (primeX + primeWidth * 0.55f, baselineTop + sizeScale,
                primeX, baselineTop + fontHeight * 0.38f,
                juce::jmax (1.2f, fontHeight * 0.10f));
}
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

YouKnowLookAndFeel::YouKnowLookAndFeel()
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
    // of the interface. The highlight uses the accent, which is bright
    // enough that the text on it has to go dark rather than stay light.
    setColour (juce::PopupMenu::backgroundColourId,
               fromPalette (panel::colour::faceplateLow));
    setColour (juce::PopupMenu::textColourId, fromPalette (panel::colour::text));
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               fromPalette (panel::colour::accent).brighter (0.14f).withAlpha (0.98f));
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
               fromPalette (panel::colour::accentMuted).withAlpha (0.85f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               fromPalette (panel::colour::accent).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::shadowColourId,
               juce::Colours::black.withAlpha (0.6f));
}

std::unique_ptr<juce::FocusOutline>
YouKnowLookAndFeel::createFocusOutlineForComponent (juce::Component&)
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

void YouKnowLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
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
    const float sizeScale = editorScale / panel::defaultEditorScale;
    const bool highlighted = slider.isEnabled() && slider.isMouseOverOrDragging();
    const float alpha = slider.isEnabled() ? 1.0f : 0.40f;
    const float wellWidth = juce::jlimit (5.0f * sizeScale, 8.0f * sizeScale,
                                          bounds.getWidth() * 0.20f);
    const auto well = bounds.withSizeKeepingCentre (wellWidth, bounds.getHeight())
                            .reduced (0.0f, 2.0f * sizeScale);
    g.setColour (fromPalette (panel::colour::slot).withAlpha (alpha));
    g.fillRoundedRectangle (well, wellWidth * 0.5f);
    g.setColour (fromPalette (panel::colour::controlShadow).withAlpha (0.24f * alpha));
    g.fillRoundedRectangle (well.withSizeKeepingCentre (sizeScale,
                                                         well.getHeight() - 5.0f * sizeScale),
                            0.5f * sizeScale);

    const float capHeight = juce::jlimit (12.0f * sizeScale, 17.0f * sizeScale,
                                          bounds.getHeight() * 0.105f);
    const float capWidth = juce::jmax (12.0f * sizeScale,
        juce::jmin (28.0f * sizeScale, bounds.getWidth() - 5.0f * sizeScale));
    const auto cap = juce::Rectangle<float> (capWidth, capHeight)
                         .withCentre ({ bounds.getCentreX(),
                                        juce::jlimit (bounds.getY() + capHeight * 0.5f,
                                                      bounds.getBottom() - capHeight * 0.5f,
                                                      sliderPos) });
    g.setColour (juce::Colours::black.withAlpha (0.24f * alpha));
    g.fillRoundedRectangle (cap.translated (0.0f, sizeScale), 2.5f * sizeScale);
    g.setColour (fromPalette (panel::colour::control)
                     .brighter (highlighted ? 0.08f : 0.0f).withAlpha (alpha));
    g.fillRoundedRectangle (cap, 2.5f * sizeScale);

    g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.70f * alpha));
    g.fillRoundedRectangle (cap.withSizeKeepingCentre (cap.getWidth() - 8.0f * sizeScale,
                                                          1.5f * sizeScale),
                            0.75f * sizeScale);
    if (highlighted)
    {
        g.setColour (fromPalette (panel::colour::led));
        g.fillRoundedRectangle (juce::Rectangle<float> (6.0f * sizeScale, 1.5f * sizeScale)
                                    .withCentre (cap.getCentre()), 0.75f * sizeScale);
    }
}

void YouKnowLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPosProportional,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height));
    const float sizeScale = editorScale / panel::defaultEditorScale;
    const bool secondary = static_cast<bool> (
        slider.getProperties().getWithDefault (secondaryStyleProperty, false));
    const bool highlighted = slider.isEnabled() && slider.isMouseOverOrDragging();
    const float alpha = slider.isEnabled() ? 1.0f : 0.40f;
    const float diameter = juce::jmax (8.0f * sizeScale,
        juce::jmin (bounds.getWidth(), bounds.getHeight())
            - (secondary ? 16.0f : 4.0f) * sizeScale);
    const auto knob = juce::Rectangle<float> (diameter, diameter)
                          .withCentre (bounds.getCentre());
    const auto centre = knob.getCentre();
    const float radius = diameter * 0.5f - 1.2f * sizeScale;
    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float stroke = (secondary ? 1.8f : 2.2f) * sizeScale;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (fromPalette (panel::colour::controlShadow).withAlpha (0.35f * alpha));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    if (sliderPosProportional > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (fromPalette (secondary ? panel::colour::accent : panel::colour::led)
                         .withAlpha ((highlighted ? 1.0f : 0.82f) * alpha));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (fromPalette (panel::colour::faceplateHigh)
                     .brighter (highlighted ? 0.10f : 0.0f).withAlpha (alpha));
    g.fillEllipse (knob.reduced (4.5f * sizeScale));
    const float pointerInner = diameter * 0.10f;
    const float pointerOuter = diameter * 0.32f;
    g.setColour (fromPalette (panel::colour::control).withAlpha (alpha));
    g.drawLine (centre.x + std::sin (angle) * pointerInner,
                centre.y - std::cos (angle) * pointerInner,
                centre.x + std::sin (angle) * pointerOuter,
                centre.y - std::cos (angle) * pointerOuter,
                juce::jmax (1.5f * sizeScale, diameter * 0.045f));
}

void YouKnowLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour&,
                                                  bool isHighlighted, bool isDown)
{
    const float sizeScale = editorScale / panel::defaultEditorScale;
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f * sizeScale);
    const bool on = button.getToggleState();
    const bool compact = static_cast<bool> (
        button.getProperties().getWithDefault (compactStyleProperty, false));
    const bool hardware = static_cast<bool> (
        button.getProperties().getWithDefault (hardwareStyleProperty, false));
    const bool highlighted = button.isEnabled() && isHighlighted;
    const float alpha = button.isEnabled() ? 1.0f : 0.40f;
    const auto face = fromPalette (panel::colour::faceplateHigh)
                          .brighter (highlighted ? 0.12f : 0.0f)
                          .darker (isDown ? 0.12f : 0.0f).withAlpha (alpha);
    const auto outline = fromPalette (on ? panel::colour::led : panel::colour::textDim)
                             .withAlpha ((on ? 0.42f : highlighted ? 0.35f : 0.16f)
                                         * alpha);

    if (hardware)
    {
        const auto layout = hardwareKeyLayout (button, bounds, editorScale);
        const float travel = isDown ? 0.7f * sizeScale : 0.0f;
        const auto key = layout.key.translated (0.0f, travel);
        const auto hardwareFace = fromPalette (panel::colour::control)
                                      .brighter (highlighted ? 0.06f : 0.0f)
                                      .darker (isDown ? 0.10f : 0.0f);
        g.setGradientFill (juce::ColourGradient (
            hardwareFace.brighter (0.02f).withAlpha (alpha), key.getX(), key.getY(),
            hardwareFace.darker (0.04f).withAlpha (alpha),
            key.getX(), key.getBottom(), false));
        g.fillRoundedRectangle (key, 2.5f * sizeScale);
        g.setColour (fromPalette (panel::colour::faceplateLow).withAlpha (0.50f * alpha));
        g.drawRoundedRectangle (key.reduced (0.5f * sizeScale),
                                2.0f * sizeScale, juce::jmax (0.8f, sizeScale));
        drawIndicator (g, layout.lamp.translated (0.0f, travel), on,
                       fromPalette (panel::colour::led), alpha);
        return;
    }

    const auto key = bounds.reduced (0.5f * sizeScale);
    g.setColour (face);
    g.fillRoundedRectangle (key, 3.0f * sizeScale);
    g.setColour (outline);
    g.drawRoundedRectangle (key.reduced (0.5f * sizeScale),
                            2.5f * sizeScale, juce::jmax (0.8f, sizeScale));

    if (compact)
        return;

    const float lens = juce::jlimit (5.2f * sizeScale, 7.8f * sizeScale,
                                     key.getHeight() * 0.24f);
    const auto led = juce::Rectangle<float> (lens, lens)
                         .withCentre ({ key.getCentreX(),
                                        key.getY() + key.getHeight() * 0.25f });
    drawIndicator (g, led, on, fromPalette (panel::colour::led), alpha);
}

void YouKnowLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const float sizeScale = editorScale / panel::defaultEditorScale;
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
                                                      : panel::colour::textDim)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));

    if (hardware)
    {
        const auto layout = hardwareKeyLayout (button, bounds.reduced (0.5f * sizeScale),
                                               editorScale);
        const auto legend = layout.legend;
        g.setColour (fromPalette (panel::colour::text)
                         .withMultipliedAlpha (button.isEnabled() ? 0.92f : 0.48f));
        if (isWaveformLegend (text))
        {
            drawWaveformLegend (g, legend, text == "PULSE", sizeScale);
            return;
        }
        if (isFootRegisterLegend (text))
        {
            drawFootRegisterLegend (g, legend, text, sizeScale);
            return;
        }

        float size = juce::jlimit (9.0f, 13.0f * sizeScale,
                                    legend.getHeight() * 0.82f);
        const float natural = juce::GlyphArrangement::getStringWidth (
            clearPanelFont (size, true), text);
        if (natural > legend.getWidth() - 2.0f * sizeScale && natural > 0.0f)
            size *= (legend.getWidth() - 2.0f * sizeScale) / natural;
        g.setFont (clearPanelFont (juce::jmax (9.0f, size), true));
        g.drawFittedText (text, legend.toNearestInt(),
                          juce::Justification::centredTop, 1, 1.0f);
        return;
    }

    if (isWaveformLegend (text))
    {
        drawWaveformLegend (g, bounds.withTrimmedTop (bounds.getHeight() * 0.40f),
                            text == "PULSE", sizeScale);
        return;
    }

    if (isFootRegisterLegend (text))
    {
        drawFootRegisterLegend (g,
                                bounds.withTrimmedTop (bounds.getHeight() * 0.36f),
                                text, sizeScale);
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
        // The size and the width budget both come from the panel description,
        // so the legend drawn here is the legend the fit checks measured.
        g.setFont (clearPanelFont (
            panel::compactLegendPointSize (bounds.getHeight(), editorScale),
            true));
        const auto displayText = text == "DRIFT 1%" ? juce::String ("1%")
                               : text == "VARY 10%" ? juce::String ("10%")
                               : text == "MORPH 50%" ? juce::String ("50%")
                                                      : text;
        // Keep every glyph inside the moving key face rather than merely
        // inside the component's outer bezel.
        auto content = bounds.reduced (panel::compactLegendPaddingX * sizeScale,
                                       panel::compactLegendPaddingY * sizeScale);
        if (actionIcon.isNotEmpty())
        {
            const float iconSize = juce::jmin (panel::compactLegendIconSize * sizeScale,
                                               content.getHeight());
            auto iconArea = content.removeFromLeft (iconSize)
                                   .withSizeKeepingCentre (iconSize, iconSize);
            content.removeFromLeft (panel::compactLegendIconGap * sizeScale);
            const auto tint = actionIcon == "stop"
                            ? fromPalette (panel::colour::led)
                            : fromPalette (panel::colour::textDim);
            drawActionIcon (g, iconArea, actionIcon, tint);
            // The legend deliberately matches its icon tint (PANIC reads in
            // LED red); state it rather than inherit whatever colour the icon
            // path left behind.
            g.setColour (tint);
        }
        // The size above already leaves every compact legend room at every
        // supported editor size, on the panel width model's deliberately
        // generous advances. The residual horizontal scale is a floor for a
        // system font wider still than that model, not the normal path.
        g.drawFittedText (displayText, content.toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);
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

void YouKnowLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                          bool isButtonDown, int buttonX,
                                          int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
{
    const float sizeScale = editorScale / panel::defaultEditorScale;
    const auto bounds = juce::Rectangle<float> (0.5f * sizeScale, 0.5f * sizeScale,
                                                 static_cast<float> (width) - sizeScale,
                                                 static_cast<float> (height) - sizeScale);
    const bool highlighted = box.isEnabled() && (isButtonDown || box.isMouseOver());
    const float alpha = box.isEnabled() ? 1.0f : 0.40f;
    const float uiScale = static_cast<float> (height) / 24.0f;
    g.setColour (fromPalette (panel::colour::faceplateHigh)
                     .brighter (highlighted ? 0.08f : 0.0f).withAlpha (alpha));
    g.fillRoundedRectangle (bounds, 3.0f * sizeScale);
    g.setColour (fromPalette (panel::colour::textDim)
                     .withAlpha ((highlighted ? 0.35f : 0.16f) * alpha));
    g.drawRoundedRectangle (bounds.reduced (0.5f * sizeScale),
                            2.5f * sizeScale, juce::jmax (0.8f, sizeScale));

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
    g.setColour (fromPalette (panel::colour::textDim)
                     .withAlpha ((highlighted ? 1.0f : 0.72f) * alpha));
    g.strokePath (arrow, juce::PathStrokeType (juce::jmax (1.3f, 1.5f * uiScale),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

juce::Font YouKnowLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    const bool preset = box.getName() == "Patch selector";
    return clearPanelFont (juce::jmax (preset ? 13.0f : 11.5f,
                                       (preset ? 17.0f : 14.0f) * editorScale));
}

void YouKnowLookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                  juce::Label& label)
{
    const int inset = juce::jmax (6, juce::roundToInt (8.0f * editorScale));
    // Reserve the arrow's square, then the same padding used at the left edge.
    // Scale the type with the editor instead of retaining JUCE's fixed 15 px.
    label.setBounds (inset, 1, juce::jmax (0, box.getWidth() - box.getHeight() - inset),
                     box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void YouKnowLookAndFeel::drawCornerResizer (juce::Graphics& g, int width,
                                                int height, bool isMouseOver,
                                                bool isMouseDragging)
{
    const float alpha = isMouseDragging ? 0.85f : isMouseOver ? 0.60f : 0.24f;
    g.setColour (fromPalette (panel::colour::textDim).withAlpha (alpha));
    for (int index = 0; index < 2; ++index)
    {
        const float inset = 4.0f + static_cast<float> (index) * 4.0f;
        g.drawLine (static_cast<float> (width) - inset - 5.0f,
                    static_cast<float> (height) - 2.5f,
                    static_cast<float> (width) - 2.5f,
                    static_cast<float> (height) - inset - 5.0f,
                    1.2f);
    }
}

void YouKnowLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
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
        drawIndicator (g, lampArea, on, fromPalette (panel::colour::led));
    }

    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), textBounds.toNearestInt(),
                      label.getJustificationType(), 2, 1.0f);
}

juce::Label* YouKnowLookAndFeel::createSliderTextBox (juce::Slider& slider)
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
// Keybed
// ---------------------------------------------------------------------------

void YouKnowKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g,
                                        juce::Rectangle<float> area,
                                        bool isDown, bool isOver,
                                        juce::Colour lineColour,
                                        juce::Colour textColour)
{
    auto ivory = fromPalette (panel::colour::control);
    if (isDown)
        ivory = ivory.interpolatedWith (fromPalette (panel::colour::led), 0.26f);
    else if (isOver)
        ivory = ivory.brighter (0.06f);
    g.setColour (ivory);
    g.fillRect (area);

    // Each white key owns one separator so neighbouring seams stay a pixel wide.
    g.setColour (lineColour.withMultipliedAlpha (0.50f));
    g.fillRect (area.getX(), area.getY(), 1.0f, area.getHeight());
    if (midiNoteNumber == getRangeEnd())
        g.fillRect (area.getRight() - 1.0f, area.getY(), 1.0f, area.getHeight());
    g.setColour (isDown ? fromPalette (panel::colour::led)
                        : fromPalette (panel::colour::faceplateLow).withAlpha (0.12f));
    g.fillRect (area.getX() + 1.0f, area.getBottom() - 3.0f,
                juce::jmax (0.0f, area.getWidth() - 2.0f), 3.0f);

    const auto note = getWhiteNoteText (midiNoteNumber);
    if (note.isNotEmpty())
    {
        g.setColour (textColour.isTransparent()
                         ? fromPalette (panel::colour::faceplateLow).withAlpha (0.72f)
                         : textColour.withMultipliedAlpha (0.76f));
        g.setFont (clearPanelFont (juce::jmin (11.0f, getKeyWidth() * 0.74f), true));
        g.drawText (note, area.reduced (2.0f).withTrimmedBottom (4.0f).toNearestInt(),
                    juce::Justification::centredBottom, false);
    }
}

void YouKnowKeyboard::drawBlackNote (int, juce::Graphics& g,
                                        juce::Rectangle<float> area,
                                        bool isDown, bool isOver,
                                        juce::Colour noteFillColour)
{
    const auto face = area.reduced (0.8f, 0.5f);
    auto ebony = noteFillColour.brighter (isOver ? 0.12f : 0.03f);
    if (isDown)
        ebony = ebony.interpolatedWith (fromPalette (panel::colour::led), 0.34f);
    g.setColour (ebony);
    g.fillRoundedRectangle (face, 2.0f);
    g.setColour (isDown ? fromPalette (panel::colour::led)
                        : fromPalette (panel::colour::textDim).withAlpha (0.13f));
    g.fillRoundedRectangle ({ face.getX() + 1.0f, face.getBottom() - 3.0f,
                              juce::jmax (0.0f, face.getWidth() - 2.0f), 2.0f }, 1.0f);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void YouKnowDisplay::refresh (const YouKnowAudioProcessor& source)
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

void YouKnowDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float uiScale = static_cast<float> (getHeight())
                        / panel::displayReferenceHeight;

    // Keep a generous content inset while letting the deck itself remain visible.
    auto area = bounds.reduced (2.9f).reduced (7.0f, 4.5f);

    // Left column carries the three text-and-bar readouts, right column the
    // scope. The scope gets the larger share and the full height: a trace is
    // only legible if it has room in both axes, and the telemetry it used to
    // sit under has been laid out horizontally in the left column instead.
    const float rightWidth = area.getWidth() * 0.52f;
    auto rightBox = area.removeFromRight (rightWidth);
    area.removeFromRight (8.0f);

    // Two quiet fill-only islands group telemetry and scope without enclosing
    // MONITOR in another card or adding separator rules.
    const auto leftIsland = area.expanded (2.0f, 3.0f);
    const auto rightIsland = rightBox.expanded (2.0f, 3.0f);
    g.setColour (fromPalette (panel::colour::scope).withAlpha (0.77f));
    g.fillRoundedRectangle (leftIsland, juce::jmax (2.0f, 3.0f * uiScale));
    g.fillRoundedRectangle (rightIsland, juce::jmax (2.0f, 3.0f * uiScale));

    // --- Left Section: voice lamps, meters, telemetry ---
    const float rowHeight = area.getHeight() / 3.0f;

    // Voice indicators
    auto voiceRow = area.removeFromTop (rowHeight);
    const auto readout = voiceRow.removeFromRight (voiceRow.getWidth() * 0.42f);
    const int lamps =
        juce::jlimit (1, youknow::YouKnowEngine::maxVoices, voiceLimit);
    const float pitch = voiceRow.getWidth() / static_cast<float> (lamps);
    const float lampSize = juce::jmin (10.5f, voiceRow.getHeight() * 0.44f,
                                       pitch * 0.52f);
    // Number the lamps only while the widest numeral fits its cell. Above
    // nine voices the cells are a few pixels wide at every supported editor
    // size, and drawText would curtail "10".."16" to a misleading "1"; the
    // "N / lamps ACTIVE" readout keeps the count legible instead.
    const auto numeralFont = clearPanelFont (9.5f, true);
    const bool numeralsFit = juce::GlyphArrangement::getStringWidth (
                                 numeralFont, juce::String (lamps)) <= pitch;
    g.setFont (numeralFont);
    for (int voice = 0; voice < lamps; ++voice)
    {
        const float centreX = voiceRow.getX()
                            + (static_cast<float> (voice) + 0.5f) * pitch;
        g.setColour (fromPalette (panel::colour::textDim).withAlpha (0.72f));
        if (numeralsFit)
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
        drawIndicator (g, lamp, lit, fromPalette (panel::colour::led));
    }

    g.setColour (fromPalette (panel::colour::textDim));
    g.setFont (clearPanelFont (11.0f, true));
    // Fitted, not ellipsised: "0 / 6 ACTIVE" is wider than the readout at the
    // default editor size, and "ACTI…" is what every player would read.
    g.drawFittedText (ready ? juce::String (voices) + " / " + juce::String (lamps) + " ACTIVE"
                            : juce::String ("STANDBY"),
                      readout.toNearestInt(), juce::Justification::centredRight,
                      1, 0.6f);

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
        }
    };

    // LFO and ENV share one row side by side rather than stacking, which is
    // what frees the third row for the telemetry the scope used to carry.
    auto meterRow = area.removeFromTop (rowHeight);
    auto lfoCell = meterRow.removeFromLeft (meterRow.getWidth() * 0.5f);
    lfoCell.removeFromRight (6.0f);
    meter (lfoCell, lfo, true, panel::colour::accentMuted, "LFO");
    meter (meterRow, envelope, false, panel::colour::led, "ENV");

    // Telemetry: unit warmup temperature and PSU rail voltage, on one line
    // across the full column width instead of stacked above the trace.
    g.setFont (panelFont (14.0f, true));
    g.setColour (fromPalette (panel::colour::textDim));
    g.drawText (juce::String (temperature, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")) + "C",
                area.toNearestInt(), juce::Justification::centredLeft);

    g.setColour (fromPalette (panel::colour::textDim));
    const float railV = 15.0f - railDroop;
    g.drawText (juce::String (railV, 2) + "V RAIL", area.toNearestInt(),
                juce::Justification::centredRight);

    // --- Right Section: real-time oscilloscope, full height ---
    const auto screen = rightBox.reduced (1.0f);

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

    g.setColour (fromPalette (panel::colour::accent));
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

YouKnowPerformanceLever::YouKnowPerformanceLever()
{
    setName ("Pitch and modulation lever");
    setTitle ("Pitch and modulation lever");
    setDescription ("Spring-loaded pitch bend and LFO modulation control");
    setTooltip (
        "Drag the spring-loaded lever left or right for pitch bend and "
        "upward for LFO modulation. Both axes spring to zero; the three BENDER "
        "depth sliders set its DCO, VCF and LFO reach. With keyboard focus, "
        "hold the arrow keys and release them to spring back to zero. "
        "Record or draw its Pitch Bend and Modulation automation lanes in your host.");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setHasFocusOutline (true);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Rectangle<float> YouKnowPerformanceLever::controlArea() const noexcept
{
    auto area = getLocalBounds().toFloat().reduced (9.0f, 4.0f);
    area.removeFromTop (12.0f);
    return area;
}

void YouKnowPerformanceLever::setValues (float bend, float mod, bool notify)
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

juce::String YouKnowPerformanceLever::getAccessibilityValueText() const
{
    const int bendPercent = juce::roundToInt (pitchBend * 100.0f);
    juce::String result { "Pitch bend " };
    if (bendPercent > 0)
        result << "+";
    result << bendPercent << "%, modulation "
           << juce::roundToInt (modulation * 100.0f) << "%";
    return result;
}

void YouKnowPerformanceLever::updateFromPointer (juce::Point<float> position)
{
    const auto area = controlArea();
    const juce::Point<float> rest { area.getCentreX(), area.getBottom() - 8.0f };
    const float horizontalTravel = juce::jmax (1.0f, area.getWidth() * 0.5f - 10.0f);
    const float verticalTravel = juce::jmax (1.0f, rest.y - area.getY() - 7.0f);
    setValues ((position.x - rest.x) / horizontalTravel,
               (rest.y - position.y) / verticalTravel, true);
}

void YouKnowPerformanceLever::mouseDown (const juce::MouseEvent& event)
{
    keyboardGestureActive = false;
    beginGesture();
    updateFromPointer (event.position);
}

void YouKnowPerformanceLever::mouseDrag (const juce::MouseEvent& event)
{
    if (gestureActive)
        updateFromPointer (event.position);
}

void YouKnowPerformanceLever::mouseUp (const juce::MouseEvent&)
{
    endGesture();
}

bool YouKnowPerformanceLever::keyPressed (const juce::KeyPress& key)
{
    auto bend = pitchBend;
    auto mod = modulation;
    if (key == juce::KeyPress::leftKey)
        bend = -1.0f;
    else if (key == juce::KeyPress::rightKey)
        bend = 1.0f;
    else if (key == juce::KeyPress::upKey)
        mod = 1.0f;
    else if (key == juce::KeyPress::downKey)
        mod = 0.0f;
    else if (key == juce::KeyPress::escapeKey
             || key == juce::KeyPress::returnKey
             || key == juce::KeyPress::spaceKey)
    {
        beginGesture();
        endGesture();
        return true;
    }
    else
    {
        return false;
    }

    beginGesture();
    keyboardGestureActive = true;
    setValues (bend, mod, true);
    return true;
}

bool YouKnowPerformanceLever::keyStateChanged (bool isKeyDown)
{
    if (isKeyDown || ! keyboardGestureActive)
        return false;

    // Every key release reaches this, not only the one that started the
    // gesture, and it does not say which key moved. Springing both axes to
    // zero here therefore dropped the pitch bend when a player who was
    // holding Right let go of Up -- with Right still physically down and its
    // auto-repeat now finished, nothing brought the bend back. Re-derive both
    // axes from the arrow keys that are still held instead, and end the
    // gesture only when none of them is.
    const bool leftHeld =
        juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::leftKey);
    const bool rightHeld =
        juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::rightKey);
    const bool upHeld =
        juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::upKey);
    // Down is part of the gesture too -- keyPressed uses it to release
    // modulation -- so a player holding it must not have modulation handed
    // back by an unrelated key going up.
    const bool downHeld =
        juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::downKey);

    const auto axes = axesForHeldKeys (leftHeld, rightHeld, upHeld, downHeld);
    setValues (axes.bend, axes.modulation, true);
    keyboardGestureActive = leftHeld || rightHeld || upHeld || downHeld;
    if (! keyboardGestureActive)
        endGesture();
    return true;
}

void YouKnowPerformanceLever::focusLost (FocusChangeType)
{
    endGesture();
}

void YouKnowPerformanceLever::beginGesture()
{
    if (gestureActive)
        return;
    gestureActive = true;
    if (onGestureStarted)
        onGestureStarted();
}

void YouKnowPerformanceLever::endGesture()
{
    if (! gestureActive)
        return;
    keyboardGestureActive = false;
    setValues (0.0f, 0.0f, true);
    gestureActive = false;
    if (onGestureEnded)
        onGestureEnded();
}

std::unique_ptr<juce::AccessibilityHandler>
YouKnowPerformanceLever::createAccessibilityHandler()
{
    class ValueInterface final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit ValueInterface (YouKnowPerformanceLever& leverToWrap)
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
        YouKnowPerformanceLever& lever;
    };

    class Handler final : public juce::AccessibilityHandler
    {
    public:
        explicit Handler (YouKnowPerformanceLever& leverToWrap)
            : juce::AccessibilityHandler (
                  leverToWrap, juce::AccessibilityRole::group, {},
                  { std::make_unique<ValueInterface> (leverToWrap) }),
              lever (leverToWrap)
        {
        }

        juce::String getHelp() const override { return lever.getTooltip(); }

    private:
        YouKnowPerformanceLever& lever;
    };

    return std::make_unique<Handler> (*this);
}

void YouKnowPerformanceLever::paint (juce::Graphics& g)
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

YouKnowContextHelp::YouKnowContextHelp()
{
    setName ("Context help");
    setTitle ("Context help");
    setDescription ("Shows an explanation for the control under the pointer");
    setInterceptsMouseClicks (false, false);
    showIdle();
}

void YouKnowContextHelp::showFor (juce::Component* component,
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

void YouKnowContextHelp::showIdle()
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

void YouKnowContextHelp::showNotice (juce::String title, juce::String text)
{
    constexpr juce::uint32 noticeMilliseconds = 6000;
    noticeTitle = std::move (title);
    noticeText = std::move (text);
    noticeExpiresAt = juce::Time::getMillisecondCounter() + noticeMilliseconds;
    setContent (noticeTitle, noticeText, {});
}

void YouKnowContextHelp::setContent (juce::String title, juce::String text,
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
YouKnowContextHelp::createAccessibilityHandler()
{
    class ValueInterface final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit ValueInterface (YouKnowContextHelp& helpToWrap)
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
        YouKnowContextHelp& help;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces {
            std::make_unique<ValueInterface> (*this) });
}

YouKnowContextHelp::BodyLayout YouKnowContextHelp::bodyLayout() const
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    auto content = bounds.reduced (juce::jmax (8.0f, bounds.getHeight() * 0.28f),
                                   juce::jmax (2.0f, bounds.getHeight() * 0.10f));
    BodyLayout layout;
    layout.headingPointSize = juce::jlimit (12.0f, 13.0f,
                                            bounds.getHeight() * 0.36f);

    if (helpValue.isNotEmpty())
    {
        layout.value = content.removeFromRight (
            juce::jlimit (90.0f, 200.0f, bounds.getWidth() * 0.13f));
        content.removeFromRight (12.0f);
    }

    // The title column carries a control's full name without consuming a
    // quarter of the entire chassis while idle. Long titles still fit at the
    // supported minimum; the explanation earns the remaining width.
    layout.title = content.removeFromLeft (
        juce::jlimit (180.0f, 210.0f, bounds.getWidth() * 0.16f));
    content.removeFromLeft (4.0f);
    layout.body = content.toNearestInt();
    layout.maximumLines = helpBodyMaximumLines;
    return layout;
}

void YouKnowContextHelp::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (fromPalette (panel::colour::faceplateLow));
    g.fillRect (bounds);
    g.setColour (fromPalette (panel::colour::edge).withAlpha (0.25f));
    g.drawHorizontalLine (0, bounds.getX(), bounds.getRight());

    const auto layout = bodyLayout();

    // The current setting, right-aligned in its own lit column. Reading a value
    // used to need a drag, because only JUCE's transient bubble carried it;
    // hovering is enough now, and the bubble still appears while dragging.
    if (helpValue.isNotEmpty())
    {
        g.setColour (fromPalette (panel::colour::led));
        g.setFont (panelFont (layout.headingPointSize, true));
        g.drawFittedText (helpValue, layout.value.toNearestInt(),
                          juce::Justification::centredRight, 1, 0.9f);
        g.setColour (fromPalette (panel::colour::edge).withAlpha (0.44f));
        g.drawVerticalLine (juce::roundToInt (layout.value.getX() - 8.0f),
                            layout.value.getY(), layout.value.getBottom());
    }

    g.setColour (fromPalette (panel::colour::accent));
    g.setFont (headingFont (layout.headingPointSize, true));
    g.drawFittedText (helpTitle, layout.title.toNearestInt(),
                      juce::Justification::centredLeft, 1, 1.0f);

    const bool showingIdlePrompt = helpTitle == "HELP";
    g.setColour (fromPalette (showingIdlePrompt ? panel::colour::textDim
                                                : panel::colour::text));
    // Three lines, not two. JUCE fits a body by stepping the type down and the
    // line count up until the text fits, and only ellipsises when it runs out
    // of both; capping the count at two ran it out one line early on the
    // longest explanations, so the QUALITY selector's help lost its closing
    // sentence at the smaller editor sizes. Everything that fitted two lines
    // still gets two lines at the reading size -- the search starts there.
    g.setFont (panelFont (layout.headingPointSize));
    g.drawFittedText (helpText, layout.body, juce::Justification::centredLeft,
                      layout.maximumLines, 1.0f);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

YouKnowAudioProcessorEditor::YouKnowAudioProcessorEditor (YouKnowAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    surfaceTexture = juce::ImageFileFormat::loadFrom (
        BinaryData::usedcharcoalplastic_png,
        static_cast<std::size_t> (BinaryData::usedcharcoalplastic_pngSize));

    logoLabel.setText ("YouKnow", juce::dontSendNotification);
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
    // Two of these readouts print an "Nx" figure and they mean different
    // things -- the scope's own vertical gain, and the oversampling factor the
    // engine actually applied. Naming both is the point of listing all six.
    display.setTooltip (
        "Shows the six physical voice cards and the active voice limit, LFO "
        "and envelope motion, the modelled chassis temperature and supply "
        "rail, and the output waveform. The scope prints its own vertical "
        "gain; the corner below it prints the oversampling the engine applied "
        "and the internal rate that produces, which can be lower than QUALITY "
        "asked for.");
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

    auto bendAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.parameters.getParameter (youknow::parameters::pitchBend),
        [this] (float value)
        {
            performanceLever.setPositionFromHost (value, performanceLever.getModulation());
        });
    auto modAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.parameters.getParameter (youknow::parameters::modulation),
        [this] (float value)
        {
            performanceLever.setPositionFromHost (performanceLever.getPitchBend(), value);
        });
    auto* bendParameter = bendAttachment.get();
    auto* modParameter = modAttachment.get();
    performanceLever.onGestureStarted = [bendParameter, modParameter]
    {
        bendParameter->beginGesture();
        modParameter->beginGesture();
    };
    performanceLever.onPositionChanged = [bendParameter, modParameter] (float bend, float modulation)
    {
        bendParameter->setValueAsPartOfGesture (bend);
        modParameter->setValueAsPartOfGesture (modulation);
    };
    performanceLever.onGestureEnded = [bendParameter, modParameter]
    {
        bendParameter->endGesture();
        modParameter->endGesture();
    };
    bendAttachment->sendInitialUpdate();
    modAttachment->sendInitialUpdate();
    parameterAttachments.push_back (std::move (bendAttachment));
    parameterAttachments.push_back (std::move (modAttachment));
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
            || dynamic_cast<YouKnowPerformanceLever*> (child) != nullptr;
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

YouKnowAudioProcessorEditor::~YouKnowAudioProcessorEditor()
{
    stopTimer();
    for (auto* child : getChildren())
        child->removeKeyListener (this);

    // Closing a plug-in window during a drag must not leave its last physical
    // gesture latched in the engine after the control itself has disappeared.
    // An untouched lever may be showing host automation; leave that alone.
    performanceLever.endGesture();
    setLookAndFeel (nullptr);
}

void YouKnowAudioProcessorEditor::buildPanelControls()
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

void YouKnowAudioProcessorEditor::buildUtilityStrip()
{
    using namespace youknow::parameters;

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
        "Scales modeled component variation and thermal wander. Zero uses "
        "nominal component values; 100% selects the default character profile; "
        "higher values exaggerate it. Circuit saturation remains at zero. "
        "The variation profile is provisional, not measured from a population "
        "of original units.",
        "Scales the modeled hiss of the uncompanded bucket-brigade chorus. "
        "100% is the modeled floor; zero is a clean plug-in extension.",
        "Sets the active voice limit from 1 to 16. Six matches the hardware; "
        "values above six add digital extension voices.",
        "Ages the modeled unit by the drift of one documented four-year "
        "service interval: each voice's filter tuning drifts flat by its own "
        "share of up to a quarter tone and the noise source rises by up to "
        "3.5 dB. Zero is freshly serviced. Unit Character scales build "
        "tolerances; Aging adds time since the last calibration."
    };

    configure (transposeSlider, transpose, "Transpose", utilityTooltips[0]);
    configure (tuneSlider, masterTune, "Master tune", utilityTooltips[1]);
    configure (velocitySlider, velocity, "Velocity", utilityTooltips[2]);
    configure (calibrationSlider, calibration, "Unit Character", utilityTooltips[3]);
    configure (agingSlider, aging, "Aging", utilityTooltips[6]);
    configure (chorusNoiseSlider, chorusNoise, "Chorus noise", utilityTooltips[4]);
    configure (polyphonySlider, polyphony, "Polyphony", utilityTooltips[5]);
    // HISS now lives inside the primary CHORUS block, so its full-size knob
    // matches that tier rather than the smaller lower-bay utilities.
    chorusNoiseSlider.getProperties().set (secondaryStyleProperty, false);

    const char* captions[] = { "AMOUNT", "TUNE", "VELOCITY",
                               "CHARACTER", "HISS", "VOICES", "AGING" };
    const char* controlNames[] = { "Transpose", "Master tune", "Velocity",
                                   "Unit Character", "Chorus noise", "Polyphony",
                                   "Aging" };
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
    nameButton (chorusBothButton);
    syxLoadButton.setName ("Load patch file");
    syxLoadButton.setTitle ("Load patch file");
    syxSaveButton.setName ("Save patch file");
    syxSaveButton.setTitle ("Save patch file");

    // The internal-rate ladder. The tooltip names the cost as well as the
    // benefit, because this is the one control a player reaches for when a
    // session runs out of CPU rather than when it needs a different sound.
    // The help strip has a fixed line budget, so both processing tooltips are
    // written to fit it rather than to say everything; the full reasoning
    // lives in the README's "Performance and quality".
    const juce::String qualityTooltip =
        "Sets how far above the host's sample rate the whole engine runs. "
        "1x is the shipped setting and the cheapest; 2x and 4x alias less and "
        "cost proportionally more. It changes the internal rate, so a change "
        "waits until the instrument is idle, and at high host rates a lower "
        "factor is used automatically. Not part of a patch.";
    for (int choice = 0; choice < YouKnowAudioProcessor::qualityChoiceCount;
         ++choice)
        qualityBox.addItem (
            juce::String (
                YouKnowAudioProcessor::oversamplingFactorForChoice (choice))
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
                          fromPalette (panel::colour::accent));
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

    // The numerical rung the four-pole filter's own solver runs at. Unlike
    // QUALITY it does not change the internal rate, so it costs no latency and
    // needs no idle window to take effect.
    const juce::String vcfSolverTooltip =
        "Sets how much arithmetic the filter's solver spends per internal "
        "sample. Normal is shipped and about halves filter CPU; High and Max "
        "cost more without sounding different. Normal is "
        + juce::String (
            YouKnowAudioProcessor::vcfSolverChoiceTechnique (2))
        + "; High " + juce::String (
            YouKnowAudioProcessor::vcfSolverChoiceTechnique (1))
        + "; Max " + juce::String (
            YouKnowAudioProcessor::vcfSolverChoiceTechnique (0))
        + ", as every earlier release ran. Not part of a patch.";
    for (int choice = 0;
         choice < YouKnowAudioProcessor::vcfSolverChoiceCount; ++choice)
        vcfSolverBox.addItem (
            YouKnowAudioProcessor::vcfSolverChoiceName (choice),
            choice + 1);
    vcfSolverBox.setName ("VCF Solver");
    vcfSolverBox.setTitle ("VCF Solver");
    vcfSolverBox.setTooltip (vcfSolverTooltip);
    vcfSolverBox.setColour (juce::ComboBox::backgroundColourId,
                            fromPalette (panel::colour::slot));
    vcfSolverBox.setColour (juce::ComboBox::textColourId,
                            fromPalette (panel::colour::text));
    vcfSolverBox.setColour (juce::ComboBox::outlineColourId,
                            fromPalette (panel::colour::controlShadow));
    vcfSolverBox.setColour (juce::ComboBox::arrowColourId,
                            fromPalette (panel::colour::accent));
    addAndMakeVisible (vcfSolverBox);
    comboBoxAttachments.push_back (std::make_unique<ComboBoxAttachment> (
        audioProcessor.parameters, vcfSolverMode, vcfSolverBox));

    vcfSolverLabel.setText ("VCF SOLVER", juce::dontSendNotification);
    vcfSolverLabel.setFont (panelFont (11.0f));
    vcfSolverLabel.setColour (
        juce::Label::textColourId,
        fromPalette (panel::colour::textDim).withAlpha (0.90f));
    vcfSolverLabel.setJustificationType (juce::Justification::centred);
    vcfSolverLabel.setName ("VCF Solver label");
    vcfSolverLabel.setTooltip (vcfSolverTooltip);
    vcfSolverLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (vcfSolverLabel);

    unisonButton.setClickingTogglesState (false);
    unisonButton.getProperties().set (hardwareStyleProperty, true);
    unisonButton.setColour (juce::TextButton::buttonColourId,
                            fromPalette (panel::colour::control));
    unisonButton.setColour (juce::TextButton::buttonOnColourId,
                            fromPalette (panel::colour::control).brighter (0.08f));
    unisonButton.setTooltip (
        "Convenience key for pressing both original POLY contacts together "
        "to enter Solo Unison; the hardware has no separate Unison key.");
    addAndMakeVisible (unisonButton);
    attachUnisonButton (unisonButton);

    chorusBothButton.setClickingTogglesState (false);
    chorusBothButton.getProperties().set (hardwareStyleProperty, true);
    chorusBothButton.setColour (
        juce::TextButton::buttonColourId,
        fromPalette (panel::colour::control));
    chorusBothButton.setColour (
        juce::TextButton::buttonOnColourId,
        fromPalette (panel::colour::control).brighter (0.08f));
    chorusBothButton.setTooltip (
        "Selects the dedicated I+II chorus state represented by both chorus "
        "contacts. Sessions retain it; hardware-format .syx files save it as "
        "Chorus II because the hardware tone-memory byte has only Off, I and II.");
    addAndMakeVisible (chorusBothButton);
    attachChorusBothButton (chorusBothButton);

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
    // manual patch message, so files round-trip with real units and with
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
        "The live I+II chorus state is written as Chorus II because the tone "
        "format has no fourth chorus code. "
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
}

void YouKnowAudioProcessorEditor::buildPresetBar()
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
    presetBox.setColour (juce::ComboBox::arrowColourId, fromPalette (panel::colour::accent));
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

void YouKnowAudioProcessorEditor::buildHardwareProgrammer()
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
        button.setColour (juce::TextButton::buttonColourId, colour);
        button.setColour (juce::TextButton::buttonOnColourId,
                          colour.brighter (0.08f));
        addAndMakeVisible (button);
    };

    const auto memory = fromPalette (panel::colour::accent);
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

void YouKnowAudioProcessorEditor::selectHardwareProgram()
{
    const int program = 1 + selectedHardwareGroup * 64
                          + selectedHardwareBank * 8
                          + selectedHardwarePatch;
    selectProgram (program);
}

void YouKnowAudioProcessorEditor::selectProgram (int index)
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

void YouKnowAudioProcessorEditor::stepProgram (int delta)
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

void YouKnowAudioProcessorEditor::refreshPresetBar()
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

bool YouKnowAudioProcessorEditor::isInterestedInFileDrag (
    const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".syx"))
            return true;
    return false;
}

void YouKnowAudioProcessorEditor::filesDropped (const juce::StringArray& files,
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

void YouKnowAudioProcessorEditor::chooseAndImportPatchFile()
{
    sysExFileChooser = std::make_unique<juce::FileChooser> (
        "Load a hardware patch dump", juce::File(), "*.syx");
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

void YouKnowAudioProcessorEditor::chooseAndExportPatchFile()
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

void YouKnowAudioProcessorEditor::importPatchFile (const juce::File& file)
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
        "LOAD", "No compatible patch dump in \"" + file.getFileName() + "\".");
}

void YouKnowAudioProcessorEditor::exportPatchFile (const juce::File& file)
{
    const auto engaged = [this] (const char* id)
    {
        const auto* value = audioProcessor.parameters.getRawParameterValue (id);
        return value != nullptr
            && value->load (std::memory_order_relaxed) > 0.5f;
    };
    const bool exportedBothAsTwo = engaged (parameters::chorusI)
                                && engaged (parameters::chorusII);
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
            + (exportedBothAsTwo
                   ? "\". I+II was stored as Chorus II because hardware "
                     "tone memory has no I+II code."
                   : "\". Performance controls travel with the session, not "
                     "the patch, as on the hardware."));
}

void YouKnowAudioProcessorEditor::attachSlider (juce::Slider& slider,
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

void YouKnowAudioProcessorEditor::attachButton (juce::Button& button,
                                                   const char* parameterId)
{
    jassert (audioProcessor.parameters.getParameter (parameterId) != nullptr);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (
        audioProcessor.parameters, parameterId, button));
}

void YouKnowAudioProcessorEditor::refreshChorusButtons()
{
    using namespace youknow;

    const auto isOn = [this] (const char* id)
    {
        if (const auto* value = audioProcessor.parameters.getRawParameterValue (id))
            return value->load (std::memory_order_relaxed) > 0.5f;
        return false;
    };
    const auto mode = chorusModeFor (isOn (parameters::chorusI),
                                     isOn (parameters::chorusII));

    const auto& controls = panel::controls();
    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        auto* button = panelControls[index].button.get();
        if (button == nullptr)
            continue;

        const auto* id = controls[index].parameterId;
        const bool isOff = std::strcmp (id, parameters::legacyChorus) == 0;
        const bool isOne = std::strcmp (id, parameters::chorusI) == 0;
        const bool isTwo = std::strcmp (id, parameters::chorusII) == 0;
        if (! isOff && ! isOne && ! isTwo)
            continue;

        button->setToggleState (
            isOff ? mode == ChorusMode::Off
                  : isOne ? mode == ChorusMode::One
                          : mode == ChorusMode::Two,
            juce::dontSendNotification);
    }
    chorusBothButton.setToggleState (mode == ChorusMode::OneTwo,
                                     juce::dontSendNotification);
}

void YouKnowAudioProcessorEditor::attachExclusiveButton (
    juce::Button& button, const char* parameterId, const char* otherParameterId)
{
    // A direct I or II press selects that single contact, so pressing one
    // releases the other even when the current state is I+II.
    // That transition belongs to a real press. Driving the lamp from a
    // ButtonAttachment ran it for parameter-driven lamp changes as well --
    // the hazard attachPolyButton below is written to avoid -- so a host lane
    // that automated Chorus I also wrote Chorus II whenever the window
    // happened to be open, and the instrument then rendered mode I with the
    // window open and mode II with it shut. The lamp now follows its
    // parameter and only a click moves anything.
    auto* attachedParameter = audioProcessor.parameters.getParameter (parameterId);
    auto* companionParameter =
        audioProcessor.parameters.getParameter (otherParameterId);
    jassert (attachedParameter != nullptr && companionParameter != nullptr);
    if (attachedParameter == nullptr || companionParameter == nullptr)
        return;

    button.setClickingTogglesState (false);
    const auto refresh = [this] (float) { refreshChorusButtons(); };

    auto attachment = std::make_unique<juce::ParameterAttachment> (
        *attachedParameter, refresh, nullptr);
    auto companion = std::make_unique<juce::ParameterAttachment> (
        *companionParameter, refresh, nullptr);

    button.onClick = [this, parameterId]
    {
        // Read the decision from the parameter atomics, not from the lamp:
        // the lamp update is asynchronous and can lag a host write.
        const auto isOn = [this] (const char* id)
        {
            if (const auto* value =
                    audioProcessor.parameters.getRawParameterValue (id))
                return value->load (std::memory_order_relaxed) > 0.5f;
            return false;
        };
        const bool oneWasOn = isOn (parameters::chorusI);
        const bool twoWasOn = isOn (parameters::chorusII);
        const auto currentMode = chorusModeFor (oneWasOn, twoWasOn);
        const auto selectedMode = std::strcmp (parameterId,
                                               parameters::chorusI) == 0
                                ? ChorusMode::One : ChorusMode::Two;
        audioProcessor.setChorusModeFromUi (
            currentMode == selectedMode ? ChorusMode::Off : selectedMode);

        // A ParameterAttachment may defer its callback when a host invokes
        // this action outside JUCE's registered message thread. The completed
        // physical press still has one unambiguous visual result now.
        refreshChorusButtons();
    };

    auto* attachmentPointer = attachment.get();
    auto* companionPointer = companion.get();
    parameterAttachments.push_back (std::move (attachment));
    parameterAttachments.push_back (std::move (companion));
    attachmentPointer->sendInitialUpdate();
    companionPointer->sendInitialUpdate();
}

void YouKnowAudioProcessorEditor::attachPolyButton (
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

void YouKnowAudioProcessorEditor::attachUnisonButton (juce::Button& button)
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

void YouKnowAudioProcessorEditor::attachChorusOffButton (juce::Button& button)
{
    auto* first = audioProcessor.parameters.getParameter (parameters::chorusI);
    auto* second = audioProcessor.parameters.getParameter (parameters::chorusII);
    jassert (first != nullptr && second != nullptr);
    if (first == nullptr || second == nullptr)
        return;

    button.setClickingTogglesState (false);
    const auto refresh = [this] (float) { refreshChorusButtons(); };

    auto firstAttachment = std::make_unique<juce::ParameterAttachment> (
        *first, refresh, nullptr);
    auto secondAttachment = std::make_unique<juce::ParameterAttachment> (
        *second, refresh, nullptr);
    button.onClick = [this]
    {
        audioProcessor.setChorusModeFromUi (ChorusMode::Off);
        refreshChorusButtons();
    };

    auto* firstPointer = firstAttachment.get();
    auto* secondPointer = secondAttachment.get();
    parameterAttachments.push_back (std::move (firstAttachment));
    parameterAttachments.push_back (std::move (secondAttachment));
    firstPointer->sendInitialUpdate();
    secondPointer->sendInitialUpdate();
}

void YouKnowAudioProcessorEditor::attachChorusBothButton (
    juce::Button& button)
{
    auto* first = audioProcessor.parameters.getParameter (parameters::chorusI);
    auto* second = audioProcessor.parameters.getParameter (parameters::chorusII);
    jassert (first != nullptr && second != nullptr);
    if (first == nullptr || second == nullptr)
        return;

    button.setClickingTogglesState (false);
    const auto refresh = [this] (float) { refreshChorusButtons(); };
    auto firstAttachment = std::make_unique<juce::ParameterAttachment> (
        *first, refresh, nullptr);
    auto secondAttachment = std::make_unique<juce::ParameterAttachment> (
        *second, refresh, nullptr);

    button.onClick = [this]
    {
        const auto isOn = [this] (const char* id)
        {
            if (const auto* value =
                    audioProcessor.parameters.getRawParameterValue (id))
                return value->load (std::memory_order_relaxed) > 0.5f;
            return false;
        };
        const auto current = chorusModeFor (isOn (parameters::chorusI),
                                            isOn (parameters::chorusII));
        audioProcessor.setChorusModeFromUi (
            current == ChorusMode::OneTwo ? ChorusMode::Off
                                           : ChorusMode::OneTwo);
        refreshChorusButtons();
    };

    auto* firstPointer = firstAttachment.get();
    auto* secondPointer = secondAttachment.get();
    parameterAttachments.push_back (std::move (firstAttachment));
    parameterAttachments.push_back (std::move (secondAttachment));
    firstPointer->sendInitialUpdate();
    secondPointer->sendInitialUpdate();
}

void YouKnowAudioProcessorEditor::attachKeyTransposeButton (juce::Button& button)
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

void YouKnowAudioProcessorEditor::attachRadio (juce::Button& button,
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

const char* YouKnowAudioProcessorEditor::parameterIdFor (
    juce::Component* component) const
{
    using namespace youknow::parameters;

    const auto& controls = panel::controls();
    const std::pair<const juce::Component*, const char*> extensions[] = {
        { &transposeSlider,   transpose },
        { &tuneSlider,        masterTune },
        { &velocitySlider,    velocity },
        { &calibrationSlider, calibration },
        { &agingSlider,       aging },
        { &chorusNoiseSlider, chorusNoise },
        { &polyphonySlider,   polyphony },
        { &qualityBox,        quality },
        { &vcfSolverBox,      vcfSolverMode },
        { &unisonButton,      legacyKeyMode },
        { &chorusBothButton,  legacyChorus },
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

juce::String YouKnowAudioProcessorEditor::parameterValueTextFor (
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
    if (isMode (youknow::parameters::poly1)
        || isMode (youknow::parameters::poly2)
        || isMode (youknow::parameters::legacyKeyMode))
    {
        const auto engaged = [this] (const char* id) {
            const auto* value = audioProcessor.parameters.getRawParameterValue (id);
            return value != nullptr
                && value->load (std::memory_order_relaxed) > 0.5f;
        };
        const auto mode = youknow::keyModeFor (
            engaged (youknow::parameters::poly1),
            engaged (youknow::parameters::poly2));
        // Named by the legacy choice parameter rather than here, so the strip
        // cannot drift from what the host's own automation lane calls it.
        if (const auto* names = audioProcessor.parameters.getParameter (
                youknow::parameters::legacyKeyMode))
            return names->getText (
                names->convertTo0to1 (static_cast<float> (mode)), 0);
        return {};
    }

    if (isMode (youknow::parameters::legacyChorus)
        || isMode (youknow::parameters::chorusI)
        || isMode (youknow::parameters::chorusII))
    {
        const auto engaged = [this] (const char* id) {
            const auto* value = audioProcessor.parameters.getRawParameterValue (id);
            return value != nullptr
                && value->load (std::memory_order_relaxed) > 0.5f;
        };
        const auto mode = youknow::chorusModeFor (
            engaged (youknow::parameters::chorusI),
            engaged (youknow::parameters::chorusII));
        if (mode == youknow::ChorusMode::OneTwo)
            return "I+II";
        if (const auto* names = audioProcessor.parameters.getParameter (
                youknow::parameters::legacyChorus))
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

juce::Rectangle<float> YouKnowAudioProcessorEditor::scaled (float x, float y,
                                                               float width,
                                                               float height) const
{
    return { x * scale, y * scale, width * scale, height * scale };
}

void YouKnowAudioProcessorEditor::resized()
{
    const auto bounds = getLocalBounds();
    const float totalUnits = panel::editorHeight;
    scale = juce::jmin (static_cast<float> (bounds.getWidth()) / panel::panelWidth(),
                        static_cast<float> (bounds.getHeight()) / totalUnits);
    lookAndFeel.setEditorScale (scale);

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

        if (std::strcmp (description.parameterId,
                         parameters::chorusII) == 0)
            chorusBothButton.setBounds (
                scaled (description.x,
                        description.y + description.height + panel::stackGap,
                        description.width, description.height).toNearestInt());

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
    logoLabel.setBounds (scaled (24.0f, 44.0f, 174.0f, 40.0f).toNearestInt());
    editionLabel.setBounds (scaled (24.0f, 88.0f, 174.0f, 22.0f).toNearestInt());
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
    vcfSolverLabel.setFont (panelFont (
        juce::jmax (10.5f, 14.0f * scale), true));
    utilityLabels[4].setFont (panelFont (
        juce::jmax (10.5f, panel::labelPointSize * scale), true));
    constexpr float labelTop = panel::extensionDeckTop + 32.0f;
    constexpr float knobTop = panel::extensionDeckTop + 52.0f;
    constexpr float labelHeight = 16.0f;
    constexpr float knobSize = 56.0f;

    // MODEL: two instrument knobs and two processing selectors share one row.
    // The stacked operations at the far right leave enough width for each
    // control to own a distinct column.
    utilityLabels[3].setBounds (
        scaled (24.0f, labelTop, 76.0f, labelHeight).toNearestInt());
    calibrationSlider.setBounds (
        scaled (34.0f, knobTop, knobSize, knobSize).toNearestInt());
    utilityLabels[6].setBounds (
        scaled (102.0f, labelTop, knobSize, labelHeight).toNearestInt());
    agingSlider.setBounds (
        scaled (102.0f, knobTop, knobSize, knobSize).toNearestInt());
    // Wide enough for the longest menu entry. JUCE lays a ComboBox's text out
    // in `width + 3 - height`, so the arrow steals a square: the widest legend
    // here is VCF SOLVER's "Standard", and at 78 px it drew as "Sta...".
    constexpr float selectorWidth = 90.0f;
    constexpr float selectorLabelHeight = 15.0f;
    constexpr float selectorBoxHeight = 30.0f;
    const auto selectorLabelBounds = [&] (float x) {
        return scaled (x, labelTop, selectorWidth,
                       selectorLabelHeight).toNearestInt();
    };
    const auto selectorBoxBounds = [&] (float x) {
        return scaled (x, knobTop + (knobSize - selectorBoxHeight) * 0.5f, selectorWidth,
                       selectorBoxHeight).toNearestInt();
    };
    qualityLabel.setBounds (selectorLabelBounds (170.0f));
    qualityBox.setBounds (selectorBoxBounds (170.0f));
    vcfSolverLabel.setBounds (selectorLabelBounds (272.0f));
    vcfSolverBox.setBounds (selectorBoxBounds (272.0f));

    // VOICE: the continuous voice-limit and response controls share a baseline.
    utilityLabels[5].setBounds (
        scaled (394.0f, labelTop, knobSize, labelHeight).toNearestInt());
    polyphonySlider.setBounds (
        scaled (394.0f, knobTop, knobSize, knobSize).toNearestInt());
    utilityLabels[2].setBounds (
        scaled (455.0f, labelTop, 62.0f, labelHeight).toNearestInt());
    velocitySlider.setBounds (
        scaled (458.0f, knobTop, knobSize, knobSize).toNearestInt());

    // PITCH follows the DCO footprint. Keep the hardware-style Transpose
    // switch next to its amount, then the fine tune control.
    keyTransposeButton.setBounds (
        scaled (546.0f, labelTop + 3.0f, 82.0f, 81.0f).toNearestInt());
    utilityLabels[0].setBounds (
        scaled (636.0f, labelTop, 62.0f, labelHeight).toNearestInt());
    transposeSlider.setBounds (
        scaled (639.0f, knobTop, knobSize, knobSize).toNearestInt());
    utilityLabels[1].setBounds (
        scaled (706.0f, labelTop, 56.0f, labelHeight).toNearestInt());
    tuneSlider.setBounds (
        scaled (706.0f, knobTop, knobSize, knobSize).toNearestInt());

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
    constexpr float hissKnobSize = 46.0f;
    const float hissKnobX = hissSlotX + (chorusCell - hissKnobSize) * 0.5f;
    utilityLabels[4].setBounds (
        scaled (hissKnobX, soundLabelTop, hissKnobSize,
                hissLabelHeight).toNearestInt());
    chorusNoiseSlider.setBounds (
        scaled (hissKnobX, soundLabelTop + hissLabelHeight + 8.0f,
                hissKnobSize, hissKnobSize).toNearestInt());

    // The host-friendly navigator gets its own rail immediately under the
    // physical A/B, BANK and PATCH keys instead of competing with aux knobs.
    constexpr float presetY = panel::presetTop + 6.0f;
    presetLabel.setFont (clearPanelFont (
        juce::jmax (10.5f, 12.5f * scale), true));
    presetEditedLabel.setFont (clearPanelFont (
        juce::jmax (10.5f, 12.5f * scale), true));
    constexpr float presetControlHeight = 36.0f;
    presetLabel.setBounds (
        scaled (226.0f, presetY, 104.0f,
                presetControlHeight).toNearestInt());
    presetPrevButton.setBounds (
        scaled (340.0f, presetY, 36.0f,
                presetControlHeight).toNearestInt());
    presetNextButton.setBounds (
        scaled (384.0f, presetY, 36.0f,
                presetControlHeight).toNearestInt());
    presetReloadButton.setBounds (
        scaled (428.0f, presetY, 112.0f,
                presetControlHeight).toNearestInt());
    presetBox.setBounds (
        scaled (550.0f, presetY, 510.0f,
                presetControlHeight).toNearestInt());
    presetEditedLabel.setBounds (
        scaled (1072.0f, presetY, 100.0f,
                presetControlHeight).toNearestInt());
    customPatchLoadButton.setBounds (
        scaled (1186.0f, presetY, 146.0f,
                presetControlHeight).toNearestInt());
    customPatchSaveButton.setBounds (
        scaled (1342.0f, presetY, 156.0f,
                presetControlHeight).toNearestInt());

    // SESSION and VARIATION use their vertical rhythm instead of consuming
    // the width needed by MODEL. Each stack shares one centreline.
    constexpr float sessionX = 1184.0f;
    constexpr float sessionTop = panel::extensionDeckTop + 36.0f;
    constexpr float sessionWidth = 112.0f;
    constexpr float sessionHeight = 36.0f;
    constexpr float sessionGap = 8.0f;
    panicButton.setBounds (
        scaled (sessionX, sessionTop, sessionWidth, sessionHeight).toNearestInt());
    resetButton.setBounds (
        scaled (sessionX, sessionTop + sessionHeight + sessionGap,
                sessionWidth, sessionHeight).toNearestInt());

    constexpr float variationX = 1316.0f;
    constexpr float variationTop = panel::extensionDeckTop + 32.0f;
    constexpr float variationWidth = 180.0f;
    constexpr float variationHeight = 24.0f;
    constexpr float variationGap = 8.0f;
    juce::Button* variationButtons[] = { &randomize1Button, &randomize10Button,
                                         &randomize50Button };
    for (int index = 0; index < 3; ++index)
        variationButtons[index]->setBounds (
            scaled (variationX,
                    variationTop + static_cast<float> (index)
                                       * (variationHeight + variationGap),
                    variationWidth, variationHeight).toNearestInt());

    contextHelp.setBounds (
        scaled (panel::panelMargin,
                panel::extensionDeckTop + panel::extensionDeckHeight
                    + panel::helpStripGap,
                panel::panelWidth() - 2.0f * panel::panelMargin,
                panel::helpStripHeight).toNearestInt());
}

void YouKnowAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (fromPalette (panel::colour::faceplate));

    // One continuous playing surface. The darker lower bay separates host
    // settings from the instrument without framing every group in a box.
    g.setColour (fromPalette (panel::colour::faceplateLow));
    g.fillRect (scaled (0.0f, panel::extensionDeckTop - 10.0f,
                        panel::editorWidth,
                        panel::editorHeight - panel::extensionDeckTop + 10.0f));

    // Faded screen-print bands identify the sound blocks; the wider gaps
    // between them stay clear so their legends and controls belong together.
    for (const auto& section : panel::sections())
        if (section.y < panel::performanceDeckTop)
        {
            const bool modulation = std::strcmp (section.name, "LFO") == 0
                                 || std::strcmp (section.name, "ENV") == 0;
            g.setColour (fromPalette (modulation ? panel::colour::redBar
                                                 : panel::colour::blueBar));
            g.fillRect (scaled (section.x, section.y,
                                section.width, panel::headerHeight));
            g.fillRect (scaled (section.x, section.y + section.height - 3.0f,
                                section.width, 3.0f));
        }
    g.setColour (fromPalette (panel::colour::redBar));
    g.fillRect (scaled (panel::instrumentLeft, panel::performanceDeckTop + 5.0f,
                        panel::instrumentRight - panel::instrumentLeft, 22.0f));

    // The original material scan is decoded once. Printing it below all
    // lettering gives the panel wear without compromising text contrast.
    if (surfaceTexture.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (0.24f);
        g.drawImageWithin (surfaceTexture, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::stretchToFit);
    }

    g.setColour (fromPalette (panel::colour::edge).withAlpha (0.25f));
    for (const float y : { panel::performanceDeckTop - 6.0f,
                           panel::extensionDeckTop - 10.0f })
        g.fillRect (scaled (panel::panelMargin, y,
                            panel::instrumentRight - panel::panelMargin, 1.0f));
    g.fillRect (scaled (panel::instrumentLeft - 10.0f, panel::soundRowTop,
                        1.0f, panel::panelHeight + panel::keyboardHeight
                                  - panel::soundRowTop));

    g.setColour (fromPalette (panel::colour::textDim));
    g.setFont (headingFont (15.0f * scale));
    g.drawText ("POLYPHONIC", scaled (24.0f, 143.0f, 174.0f, 22.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
    g.drawText ("SYNTHESIZER", scaled (24.0f, 166.0f, 174.0f, 22.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
    g.setColour (fromPalette (panel::colour::blueBar));
    g.fillRect (scaled (24.0f, 211.0f, 156.0f, 7.0f));
    g.setColour (fromPalette (panel::colour::redBar));
    g.fillRect (scaled (24.0f, 224.0f, 156.0f, 7.0f));

    const auto drawHeading = [&] (const char* text, float x, float y,
                                  float width, float height, bool drawSpan,
                                  float designPointSize, float minimumPointSize,
                                  float textAlpha)
    {
        const auto area = scaled (x, y, width, height);
        float captionSize = juce::jmax (minimumPointSize,
                                        designPointSize * scale);
        auto font = headingFont (captionSize, true);
        float captionWidth = juce::GlyphArrangement::getStringWidth (font, text);
        const float availableWidth = (width - 4.0f) * scale;
        if (captionWidth > availableWidth && captionWidth > 0.0f)
        {
            captionSize *= availableWidth / captionWidth;
            font = headingFont (
                juce::jmax (minimumPointSize - 1.5f, captionSize), true);
            captionWidth = juce::GlyphArrangement::getStringWidth (font, text);
        }

        if (drawSpan)
        {
            const float gap = 9.0f * scale;
            const float left = area.getX() + 4.0f * scale;
            const float leftEnd = area.getCentreX() - captionWidth * 0.5f - gap;
            const float right = area.getCentreX() + captionWidth * 0.5f + gap;
            const float rightEnd = area.getRight() - 4.0f * scale;
            g.setColour (fromPalette (panel::colour::edge).withAlpha (0.26f));
            if (leftEnd > left)
                g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), left, leftEnd);
            if (rightEnd > right)
                g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), right, rightEnd);
        }

        g.setColour (fromPalette (panel::colour::heading).withAlpha (textAlpha));
        g.setFont (font);
        g.drawText (text, area.toNearestInt(), juce::Justification::centred, false);
    };
    const auto drawMinorHeading = [&] (const char* text, float x, float y,
                                       float width, bool drawSpan)
    {
        drawHeading (text, x, y, width, 14.0f, drawSpan,
                     panel::minorHeadingPointSize, 10.5f, 0.84f);
    };

    for (const auto& section : panel::sections())
        if (section.y < panel::performanceDeckTop)
        {
            if (section.x > panel::instrumentLeft)
            {
                g.setColour (fromPalette (panel::colour::edge).withAlpha (0.20f));
                g.fillRect (scaled (section.x - panel::soundSectionGap * 0.5f,
                                    section.y + 44.0f, 1.0f,
                                    section.height - 62.0f));
            }
            drawHeading (section.displayTitle, section.x, section.y,
                         section.width, panel::headerHeight, false,
                         panel::sectionHeadingPointSize, 11.0f, 0.90f);
        }

    constexpr float programmerHeadingTop = panel::performanceDeckTop + 9.0f;
    drawMinorHeading ("VOICE MODE", 218.0f, programmerHeadingTop, 242.0f, true);
    drawMinorHeading ("GROUP", 470.0f, programmerHeadingTop, 70.0f, true);
    drawMinorHeading ("BANK", 550.0f, programmerHeadingTop, 282.0f, true);
    drawMinorHeading ("PROGRAM", 842.0f, programmerHeadingTop, 64.0f, false);
    drawMinorHeading ("PATCH", 916.0f, programmerHeadingTop, 282.0f, true);
    drawMinorHeading ("DATA", 1218.0f, programmerHeadingTop, 272.0f, true);

    g.setColour (fromPalette (panel::colour::slot));
    g.fillRoundedRectangle (scaled (842.0f, 296.0f, 64.0f, 68.0f), 4.0f * scale);

    // The taller navigator shares one baseline and generous touch targets.
    g.setColour (fromPalette (panel::colour::faceplateHigh).withAlpha (0.45f));
    g.fillRoundedRectangle (
        scaled (panel::instrumentLeft, panel::presetTop,
                panel::instrumentRight - panel::instrumentLeft,
                panel::presetHeight), 5.0f * scale);

    // A single fine felt line ties the neutral keybed to the active accent.
    g.setColour (fromPalette (panel::colour::accentMuted).withAlpha (0.75f));
    g.fillRect (scaled (panel::instrumentLeft, panel::panelHeight - 2.0f,
                        panel::instrumentRight - panel::instrumentLeft, 2.0f));

    constexpr float extensionHeadingTop = panel::extensionDeckTop + 7.0f;
    g.setColour (fromPalette (panel::colour::edge).withAlpha (0.22f));
    for (const float x : { panel::voiceZoneX, panel::pitchZoneX,
                           panel::monitorZoneX, panel::operationsBarX })
        g.fillRect (scaled (x - panel::extensionZoneGap * 0.5f,
                            panel::extensionDeckTop + 34.0f, 1.0f,
                            panel::extensionDeckHeight - 50.0f));
    drawMinorHeading ("MODEL", panel::modelZoneX, extensionHeadingTop,
                      panel::modelZoneWidth, true);
    drawMinorHeading ("VOICE", panel::voiceZoneX, extensionHeadingTop,
                      panel::voiceZoneWidth, true);
    drawMinorHeading ("PITCH", panel::pitchZoneX, extensionHeadingTop,
                      panel::pitchZoneWidth, true);
    drawMinorHeading ("MONITOR", panel::monitorZoneX, extensionHeadingTop,
                      panel::monitorZoneWidth, true);
    drawMinorHeading ("SESSION", panel::operationsBarX, extensionHeadingTop,
                      panel::operationsGroupSplitX - panel::operationsBarX, true);
    drawMinorHeading ("VARIATION", panel::operationsGroupSplitX, extensionHeadingTop,
                      panel::operationsBarX + panel::operationsBarWidth
                          - panel::operationsGroupSplitX, true);
}

void YouKnowAudioProcessorEditor::timerCallback()
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

bool YouKnowAudioProcessorEditor::keyPressed (const juce::KeyPress&,
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

void YouKnowAudioProcessorEditor::refreshContextHelp (
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

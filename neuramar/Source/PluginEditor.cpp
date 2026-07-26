#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{
constexpr auto background = 0xff050d10u;
constexpr auto backgroundDeep = 0xff02080au;
constexpr auto panel = 0xff0a181bu;
constexpr auto panelRaised = 0xff102427u;
constexpr auto ink = 0xffe1eee9u;
constexpr auto muted = 0xff829b96u;
constexpr auto mint = 0xff72e8beu;
constexpr auto violet = 0xffa99ae8u;
constexpr auto coral = 0xffe9786eu;
constexpr auto clay = 0xffd99572u;
constexpr auto amber = 0xffe8c577u;

juce::Colour colour (juce::uint32 argb)
{
    return juce::Colour (argb);
}

juce::Font font (float height, bool bold = false)
{
    auto options = juce::FontOptions (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}

void drawOrganicField (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient base (colour (0xff09191cu), bounds.getTopLeft(),
                               colour (backgroundDeep), bounds.getBottomRight(), false);
    base.addColour (0.46, colour (0xff061216u));
    g.setGradientFill (base);
    g.fillRect (bounds);

    const auto drawBloom = [&g, bounds] (juce::Colour bloom,
                                         float x, float y, float radius,
                                         float opacity)
    {
        const auto centre = juce::Point<float> (bounds.getX() + bounds.getWidth() * x,
                                                bounds.getY() + bounds.getHeight() * y);
        juce::ColourGradient gradient (bloom.withAlpha (opacity), centre.x, centre.y,
                                      bloom.withAlpha (0.0f), centre.x + radius,
                                      centre.y, true);
        g.setGradientFill (gradient);
        g.fillRect (bounds);
    };

    drawBloom (colour (mint), 0.17f, 0.04f, bounds.getWidth() * 0.56f, 0.085f);
    drawBloom (colour (violet), 0.94f, 0.31f, bounds.getWidth() * 0.46f, 0.052f);
    drawBloom (colour (clay), 0.54f, 1.04f, bounds.getWidth() * 0.42f, 0.025f);

    static constexpr std::array<float, 6> contourHeights {
        0.12f, 0.205f, 0.315f, 0.475f, 0.66f, 0.84f
    };
    for (std::size_t index = 0; index < contourHeights.size(); ++index)
    {
        const auto y = bounds.getY() + bounds.getHeight() * contourHeights[index];
        const auto phase = static_cast<float> (index);
        juce::Path contour;
        contour.startNewSubPath (bounds.getX() - bounds.getWidth() * 0.06f, y);
        contour.cubicTo (bounds.getX() + bounds.getWidth() * 0.18f,
                         y - bounds.getHeight() * (0.055f + 0.009f * phase),
                         bounds.getX() + bounds.getWidth() * 0.36f,
                         y + bounds.getHeight() * (0.042f - 0.004f * phase),
                         bounds.getX() + bounds.getWidth() * 0.56f,
                         y - bounds.getHeight() * 0.018f);
        contour.cubicTo (bounds.getX() + bounds.getWidth() * 0.74f,
                         y - bounds.getHeight() * (0.060f - 0.004f * phase),
                         bounds.getX() + bounds.getWidth() * 0.90f,
                         y + bounds.getHeight() * (0.040f + 0.003f * phase),
                         bounds.getRight() + bounds.getWidth() * 0.05f,
                         y - bounds.getHeight() * 0.025f);
        const auto contourColour = index % 3 == 1 ? colour (violet) : colour (mint);
        g.setColour (contourColour.withAlpha (0.022f + 0.004f
                                               * static_cast<float> (index % 2)));
        g.strokePath (contour, juce::PathStrokeType (
            index == 0 || index == contourHeights.size() - 1 ? 1.15f : 0.75f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                    bounds.getCentreX(), bounds.getCentreY(),
                                    colour (backgroundDeep).withAlpha (0.54f),
                                    bounds.getRight(), bounds.getBottom(), true);
    vignette.addColour (0.68, colour (backgroundDeep).withAlpha (0.03f));
    g.setGradientFill (vignette);
    g.fillRect (bounds);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                float corner = 18.0f)
{
    for (int layer = 3; layer >= 1; --layer)
    {
        const auto spread = static_cast<float> (layer) * 1.25f;
        g.setColour (juce::Colours::black.withAlpha (0.025f
                                                      * static_cast<float> (4 - layer)));
        g.fillRoundedRectangle (bounds.expanded (spread)
                                    .translated (0.0f, 1.2f + spread * 0.55f),
                                corner + spread);
    }

    juce::ColourGradient surface (colour (0xff11272au).withAlpha (0.95f),
                                  bounds.getX(), bounds.getY(),
                                  colour (0xff071417u).withAlpha (0.97f),
                                  bounds.getRight(), bounds.getBottom(), false);
    surface.addColour (0.52, colour (panel).withAlpha (0.96f));
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, corner);

    juce::ColourGradient mineralGlow (colour (mint).withAlpha (0.032f),
                                      bounds.getX() + bounds.getWidth() * 0.12f,
                                      bounds.getY() + bounds.getHeight() * 0.08f,
                                      colour (mint).withAlpha (0.0f),
                                      bounds.getX() + bounds.getWidth() * 0.72f,
                                      bounds.getY() + bounds.getHeight() * 0.74f, true);
    g.setGradientFill (mineralGlow);
    g.fillRoundedRectangle (bounds, corner);

    juce::Path vein;
    vein.startNewSubPath (bounds.getX() - bounds.getWidth() * 0.04f,
                          bounds.getY() + bounds.getHeight() * 0.62f);
    vein.cubicTo (bounds.getX() + bounds.getWidth() * 0.24f,
                  bounds.getY() + bounds.getHeight() * 0.43f,
                  bounds.getX() + bounds.getWidth() * 0.52f,
                  bounds.getY() + bounds.getHeight() * 0.76f,
                  bounds.getRight() + bounds.getWidth() * 0.04f,
                  bounds.getY() + bounds.getHeight() * 0.48f);
    g.setColour (colour (mint).withAlpha (0.016f));
    g.strokePath (vein, juce::PathStrokeType (13.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour (ink).withAlpha (0.024f));
    g.strokePath (vein, juce::PathStrokeType (0.65f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    g.setColour (colour (ink).withAlpha (0.055f));
    g.drawRoundedRectangle (bounds.reduced (0.65f), corner, 0.8f);
    g.setColour (colour (mint).withAlpha (0.070f));
    g.drawRoundedRectangle (bounds.reduced (1.35f), corner - 0.5f, 0.65f);
}
} // namespace

NeuramarLookAndFeel::NeuramarLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colour (ink));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colour (mint));
    setColour (juce::Slider::rotarySliderOutlineColourId, colour (muted));
    setColour (juce::Label::textColourId, colour (ink));
    setColour (juce::TextButton::textColourOffId, colour (ink));
    setColour (juce::TextButton::textColourOnId, colour (background));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colour (0xffd7e9e4u));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colour (0xff102126u));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colour (0xff42605cu));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               colour (violet).withAlpha (0.45f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               colour (mint).withAlpha (0.78f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, colour (0xff57746fu));
}

void NeuramarLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                             int width, int height, float sliderPos,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto available = static_cast<float> (std::min (width, height));
    if (available <= 20.0f)
        return;

    const auto value = juce::jlimit (0.0f, 1.0f,
        std::isfinite (sliderPos) ? sliderPos : 0.0f);
    const auto diameter = available - juce::jlimit (9.0f, 15.0f, available * 0.12f);
    const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre ({
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f });
    const auto centre = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto angle = rotaryStartAngle + value * (rotaryEndAngle - rotaryStartAngle);
    const auto enabled = slider.isEnabled();
    const auto hovering = slider.isMouseOverOrDragging();
    const auto pressed = slider.isMouseButtonDown();
    const auto focused = slider.hasKeyboardFocus (true);
    const auto accentBase = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto accent = enabled ? accentBase : colour (muted).darker (0.22f);

    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (enabled ? 1.0f : 0.48f);

    for (int layer = 4; layer >= 1; --layer)
    {
        const auto spread = static_cast<float> (layer) * 1.15f;
        g.setColour (juce::Colours::black.withAlpha (0.035f
                                                      * static_cast<float> (5 - layer)));
        g.fillEllipse (bounds.expanded (spread)
                           .translated (0.0f, 1.8f + spread * 0.50f));
    }

    juce::ColourGradient collar (colour (0xff253a3cu),
                                  bounds.getX() + diameter * 0.20f,
                                  bounds.getY() + diameter * 0.13f,
                                  colour (0xff03090bu), bounds.getRight(),
                                  bounds.getBottom(), true);
    collar.addColour (0.56, colour (0xff102125u));
    g.setGradientFill (collar);
    g.fillEllipse (bounds);
    g.setColour (colour (ink).withAlpha (0.10f));
    g.drawEllipse (bounds.reduced (0.6f), juce::jlimit (0.7f, 1.2f, diameter * 0.011f));

    const auto arcRadius = radius * 0.90f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colour (muted).withAlpha (hovering ? 0.30f : 0.21f));
    g.strokePath (track, juce::PathStrokeType (
        juce::jlimit (1.25f, 2.0f, diameter * 0.019f), juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    for (int tick = 0; tick < 5; ++tick)
    {
        const auto proportion = static_cast<float> (tick) / 4.0f;
        const auto tickAngle = rotaryStartAngle
                             + proportion * (rotaryEndAngle - rotaryStartAngle);
        const auto point = centre.getPointOnCircumference (radius * 0.76f, tickAngle);
        const auto tickSize = tick == 0 || tick == 2 || tick == 4
            ? juce::jlimit (1.4f, 2.4f, diameter * 0.022f)
            : juce::jlimit (1.0f, 1.8f, diameter * 0.016f);
        g.setColour (colour (muted).withAlpha (tick == 2 ? 0.36f : 0.22f));
        g.fillEllipse (juce::Rectangle<float> (tickSize, tickSize).withCentre (point));
    }

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, angle, true);
    if (hovering || focused)
    {
        g.setColour (accent.withAlpha (focused ? 0.18f : 0.12f));
        g.strokePath (valueArc, juce::PathStrokeType (
            juce::jlimit (4.4f, 6.0f, diameter * 0.058f), juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }
    juce::ColourGradient arcGradient (accent.brighter (0.18f), bounds.getX(), bounds.getY(),
                                      accent.darker (0.12f), bounds.getRight(),
                                      bounds.getBottom(), false);
    g.setGradientFill (arcGradient);
    g.strokePath (valueArc, juce::PathStrokeType (
        juce::jlimit (1.8f, 2.6f, diameter * 0.025f), juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    const auto bead = centre.getPointOnCircumference (arcRadius, angle);
    if (hovering)
    {
        const auto bloomSize = juce::jlimit (6.0f, 10.0f, diameter * 0.09f);
        g.setColour (accent.withAlpha (0.13f));
        g.fillEllipse (juce::Rectangle<float> (bloomSize, bloomSize).withCentre (bead));
    }
    const auto beadSize = juce::jlimit (2.8f, 4.2f, diameter * 0.038f);
    g.setColour (accent.brighter (0.24f));
    g.fillEllipse (juce::Rectangle<float> (beadSize, beadSize).withCentre (bead));

    auto capBounds = bounds.reduced (radius * 0.17f);
    if (hovering && ! pressed)
        capBounds = capBounds.translated (0.0f, -0.55f);
    juce::ColourGradient cap (pressed ? colour (0xff152629u) : colour (0xff294144u),
                              capBounds.getX() + capBounds.getWidth() * 0.25f,
                              capBounds.getY() + capBounds.getHeight() * 0.18f,
                              colour (0xff071114u), capBounds.getRight(),
                              capBounds.getBottom(), true);
    cap.addColour (0.48, pressed ? colour (0xff102023u) : colour (0xff172d30u));
    g.setGradientFill (cap);
    g.fillEllipse (capBounds);

    const auto innerGlowPoint = centre.getPointOnCircumference (radius * 0.34f, angle);
    juce::ColourGradient innerGlow (accent.withAlpha (hovering ? 0.105f : 0.045f),
                                    innerGlowPoint.x, innerGlowPoint.y,
                                    accent.withAlpha (0.0f), centre.x + radius * 0.68f,
                                    centre.y, true);
    g.setGradientFill (innerGlow);
    g.fillEllipse (capBounds.reduced (1.0f));

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (capBounds.translated (0.0f, 0.8f),
                   juce::jlimit (1.0f, 1.6f, diameter * 0.014f));
    g.setColour (colour (ink).withAlpha (hovering ? 0.14f : 0.09f));
    g.drawEllipse (capBounds.reduced (0.45f), 0.8f);

    const auto pointerStart = centre.getPointOnCircumference (radius * 0.15f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (radius * 0.54f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.drawLine ({ pointerStart.translated (0.0f, 1.0f),
                  pointerEnd.translated (0.0f, 1.0f) },
                juce::jlimit (2.2f, 3.6f, diameter * 0.036f));
    g.setColour (accent.interpolatedWith (colour (ink), 0.22f)
                       .withAlpha (hovering ? 1.0f : 0.88f));
    g.drawLine ({ pointerStart, pointerEnd },
                juce::jlimit (1.35f, 2.2f, diameter * 0.021f));

    const auto dimpleSize = juce::jlimit (2.2f, 3.5f, diameter * 0.030f);
    g.setColour (juce::Colours::black.withAlpha (0.52f));
    g.fillEllipse (juce::Rectangle<float> (dimpleSize, dimpleSize).withCentre (centre));

    if (focused)
    {
        g.setColour (colour (ink).withAlpha (0.78f));
        g.drawEllipse (bounds.expanded (2.0f), 1.25f);
        g.setColour (accent.withAlpha (0.28f));
        g.drawEllipse (bounds.expanded (4.2f), 1.0f);
    }
}

void NeuramarLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour&,
                                                 bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    const auto destructive = button.getButtonText() == "PANIC";
    const auto buttonAccent = destructive ? colour (coral) : colour (mint);
    auto fill = active ? colour (0xff216a5au) : colour (panelRaised).darker (0.10f);
    if (down)
        fill = fill.interpolatedWith (juce::Colours::black, 0.20f);
    else if (highlighted)
        fill = fill.brighter (0.08f);

    g.setColour (juce::Colours::black.withAlpha (0.31f));
    g.fillRoundedRectangle (bounds.expanded (0.8f).translated (0.0f, 2.0f), 9.0f);
    juce::ColourGradient surface (fill.brighter (active ? 0.03f : 0.07f),
                                  bounds.getX(), bounds.getY(),
                                  fill.darker (active ? 0.12f : 0.19f),
                                  bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour ((active ? colour (mint) : buttonAccent)
                     .withAlpha (active ? 0.90f : destructive ? 0.48f
                                                              : highlighted ? 0.30f : 0.17f));
    g.drawRoundedRectangle (bounds.reduced (0.35f), 8.0f,
                            button.hasKeyboardFocus (true) ? 1.5f : 0.9f);

    if (! active)
    {
        g.setColour (colour (ink).withAlpha (0.045f));
        g.drawRoundedRectangle (bounds.reduced (1.35f), 7.2f, 0.7f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (colour (ink).withAlpha (0.74f));
        g.drawRoundedRectangle (bounds.reduced (2.2f), 6.4f, 1.15f);
    }
}

void NeuramarLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    const auto destructive = button.getButtonText() == "PANIC";
    g.setColour (button.getToggleState() ? colour (ink)
                                         : destructive ? colour (coral).brighter (0.10f)
                                                       : colour (ink));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 2),
                      juce::Justification::centred, 1);
}

void NeuramarLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
    {
        const auto engaged = slider->isMouseOverOrDragging()
                          || slider->hasKeyboardFocus (true)
                          || label.isBeingEdited();
        if (engaged)
        {
            const auto bounds = label.getLocalBounds().toFloat().reduced (1.0f, 0.5f);
            g.setColour (colour (backgroundDeep).withAlpha (0.56f));
            g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.45f);
            g.setColour (slider->findColour (juce::Slider::rotarySliderFillColourId)
                             .withAlpha (0.25f));
            g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.45f, 0.8f);
        }
    }

    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(),
                      std::max (1, label.getMinimumHorizontalScale() < 1.0f ? 2 : 1),
                      label.getMinimumHorizontalScale());
}

juce::Label* NeuramarLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (10.5f, true));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

juce::Font NeuramarLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jlimit (10.0f, 13.0f, static_cast<float> (buttonHeight) * 0.30f), true);
}

NeuramarKnob::NeuramarKnob (juce::String title, juce::String hint)
{
    slider.setTitle (title);
    slider.setDescription (hint);
    slider.setTooltip (title + " - " + hint);

    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setFont (font (11.2f, true));
    titleLabel.setColour (juce::Label::textColourId, colour (ink));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    hintLabel.setText (hint, juce::dontSendNotification);
    hintLabel.setFont (font (9.4f));
    hintLabel.setColour (juce::Label::textColourId, colour (muted));
    hintLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hintLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f,
                                juce::MathConstants<float>::pi * 2.78f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour (mint));
    addAndMakeVisible (slider);
}

void NeuramarKnob::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds (area.removeFromTop (18));
    hintLabel.setBounds (area.removeFromBottom (13));
    slider.setBounds (area);
}

void NeuralPoolDisplay::setSnapshot (NeuramarAudioProcessor::LearningSnapshot next,
                                     int activeVoiceCount, double sampleRate)
{
    snapshot = std::move (next);
    activeVoices = activeVoiceCount;
    currentSampleRate = sampleRate;
    animationPhase = std::fmod (animationPhase + 0.0165f, 1.0f);
    repaint();
}

void NeuralPoolDisplay::setDragHover (bool hovering)
{
    if (dragHover != hovering)
    {
        dragHover = hovering;
        repaint();
    }
}

juce::String NeuralPoolDisplay::stageTitle (NeuramarAudioProcessor::LearningStage stage)
{
    using Stage = NeuramarAudioProcessor::LearningStage;
    switch (stage)
    {
        case Stage::Empty:       return "DROP A SOUND OR PRESS RANDOMIZE";
        case Stage::Reading:     return "LISTENING";
        case Stage::FindingRoot: return "FINDING THE ROOT";
        case Stage::Analysing:   return "SEPARATING CORE / AIR / BONE";
        case Stage::Training:    return "DREAMING A PLAYABLE MEMORY";
        case Stage::Ready:       return "MEMORY ALIVE";
        case Stage::Cancelled:   return "LEARNING CANCELLED";
        case Stage::Error:       return "THE POOL COULD NOT LEARN THIS SOUND";
    }
    return {};
}

juce::Colour NeuralPoolDisplay::stageColour (NeuramarAudioProcessor::LearningStage stage)
{
    using Stage = NeuramarAudioProcessor::LearningStage;
    if (stage == Stage::Error)
        return colour (coral);
    if (stage == Stage::Ready)
        return colour (mint);
    if (stage == Stage::Cancelled || stage == Stage::Empty)
        return colour (violet);
    return colour (amber);
}

void NeuralPoolDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = dragHover ? colour (mint) : stageColour (snapshot.stage);

    juce::ColourGradient poolGradient (colour (0xff102b2du), bounds.getX(), bounds.getY(),
                                       colour (0xff061315u), bounds.getRight(),
                                       bounds.getBottom(), false);
    poolGradient.addColour (0.44, colour (0xff0a2022u));
    g.setGradientFill (poolGradient);
    g.fillRoundedRectangle (bounds, 22.0f);

    juce::ColourGradient poolBloom (accent.withAlpha (dragHover ? 0.11f : 0.050f),
                                    bounds.getX() + bounds.getWidth() * 0.20f,
                                    bounds.getY() + bounds.getHeight() * 0.08f,
                                    accent.withAlpha (0.0f),
                                    bounds.getX() + bounds.getWidth() * 0.78f,
                                    bounds.getY() + bounds.getHeight() * 0.78f, true);
    g.setGradientFill (poolBloom);
    g.fillRoundedRectangle (bounds, 22.0f);

    g.setColour (accent.withAlpha (dragHover ? 0.88f : 0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.8f), 22.0f, dragHover ? 2.4f : 1.2f);

    auto graph = bounds.reduced (22.0f);
    graph.removeFromTop (52.0f);
    graph.removeFromBottom (50.0f);
    const auto centre = graph.getCentre();

    // A curved pitch/time field feels grown rather than plotted while retaining
    // enough structure to make the learned memory read as a model, not a sample.
    for (int i = 1; i < 8; ++i)
    {
        const auto x = graph.getX() + graph.getWidth() * static_cast<float> (i) / 8.0f;
        const auto offset = graph.getWidth() * (0.008f
            + 0.002f * static_cast<float> (i % 3));
        juce::Path filament;
        filament.startNewSubPath (x, graph.getY());
        filament.cubicTo (x - offset, graph.getY() + graph.getHeight() * 0.31f,
                          x + offset, graph.getY() + graph.getHeight() * 0.67f,
                          x - offset * 0.35f, graph.getBottom());
        g.setColour ((i % 3 == 0 ? colour (violet) : colour (mint))
                         .withAlpha (0.035f));
        g.strokePath (filament, juce::PathStrokeType (0.65f,
                                                      juce::PathStrokeType::curved));
    }
    for (int i = 1; i < 5; ++i)
    {
        const auto y = graph.getY() + graph.getHeight() * static_cast<float> (i) / 5.0f;
        const auto offset = graph.getHeight() * (0.018f
            + 0.003f * static_cast<float> (i % 2));
        juce::Path contour;
        contour.startNewSubPath (graph.getX(), y);
        contour.cubicTo (graph.getX() + graph.getWidth() * 0.27f, y - offset,
                         graph.getX() + graph.getWidth() * 0.58f, y + offset,
                         graph.getRight(), y - offset * 0.45f);
        g.setColour (colour (mint).withAlpha (0.035f));
        g.strokePath (contour, juce::PathStrokeType (0.65f,
                                                     juce::PathStrokeType::curved));
    }

    const auto hasWaveform = std::any_of (snapshot.waveform.begin(), snapshot.waveform.end(),
                                         [] (float value) { return std::abs (value) > 1.0e-5f; });
    if (hasWaveform)
    {
        juce::Path waveform;
        for (std::size_t i = 0; i < snapshot.waveform.size(); ++i)
        {
            const auto proportion = static_cast<float> (i)
                                  / static_cast<float> (snapshot.waveform.size() - 1);
            const auto x = graph.getX() + graph.getWidth() * proportion;
            const auto value = juce::jlimit (-1.0f, 1.0f, snapshot.waveform[i]);
            const auto y = centre.y - value * graph.getHeight() * 0.31f;
            if (i == 0)
                waveform.startNewSubPath (x, y);
            else
                waveform.lineTo (x, y);
        }
        g.setColour (accent.withAlpha (0.12f));
        g.strokePath (waveform, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved));
        g.setColour (accent.withAlpha (0.78f));
        g.strokePath (waveform, juce::PathStrokeType (1.45f, juce::PathStrokeType::curved));
    }

    if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Ready)
    {
        constexpr int nodeCount = 18;
        std::array<juce::Point<float>, nodeCount> nodes {};
        for (int i = 0; i < nodeCount; ++i)
        {
            const auto lane = i % 6;
            const auto row = i / 6;
            const auto x = graph.getX() + graph.getWidth()
                         * (0.08f + 0.84f * static_cast<float> (lane) / 5.0f);
            const auto wobble = std::sin ((animationPhase * 2.0f
                                           + static_cast<float> (i) * 0.137f)
                                          * juce::MathConstants<float>::pi);
            const auto y = graph.getY() + graph.getHeight()
                         * (0.22f + 0.28f * static_cast<float> (row))
                         + wobble * (2.0f + static_cast<float> (activeVoices));
            nodes[static_cast<std::size_t> (i)] = { x, y };
        }

        for (int i = 0; i < nodeCount; ++i)
        {
            if (i + 1 < nodeCount && (i + 1) % 6 != 0)
            {
                g.setColour ((i % 3 == 0 ? colour (violet) : colour (mint))
                                 .withAlpha (0.10f + 0.025f
                                     * static_cast<float> (activeVoices)));
                g.drawLine ({ nodes[static_cast<std::size_t> (i)],
                              nodes[static_cast<std::size_t> (i + 1)] }, 1.0f);
            }
            if (i + 6 < nodeCount)
            {
                g.setColour (colour (violet).withAlpha (0.08f));
                g.drawLine ({ nodes[static_cast<std::size_t> (i)],
                              nodes[static_cast<std::size_t> (i + 6)] }, 0.8f);
            }
        }

        for (int i = 0; i < nodeCount; ++i)
        {
            const auto pulse = 2.0f + 1.2f * std::max (0.0f, std::sin (
                (animationPhase + static_cast<float> (i) * 0.071f)
                * juce::MathConstants<float>::twoPi));
            g.setColour ((i % 3 == 0 ? colour (violet) : colour (mint)).withAlpha (0.80f));
            g.fillEllipse (juce::Rectangle<float> (pulse, pulse)
                               .withCentre (nodes[static_cast<std::size_t> (i)]));
        }
    }

    const auto titleBounds = bounds.reduced (22.0f).removeFromTop (34.0f);
    g.setColour (accent);
    g.setFont (font (15.0f, true));
    g.drawText (stageTitle (snapshot.stage), titleBounds,
                juce::Justification::centredLeft, false);

    auto chip = bounds.reduced (18.0f).removeFromTop (28.0f).removeFromRight (132.0f);
    g.setColour (accent.withAlpha (0.10f));
    g.fillRoundedRectangle (chip, 9.0f);
    g.setColour (accent.withAlpha (0.72f));
    g.setFont (font (9.5f, true));
    const auto memoryText = snapshot.modelGeneration > 0
        ? "M" + juce::String (snapshot.modelGeneration) + "  /  "
        : juce::String {};
    const auto engineText = currentSampleRate > 0.0
        ? memoryText + juce::String (activeVoices) + " VOICES  /  "
            + juce::String (juce::roundToInt (currentSampleRate / 1000.0)) + "K"
        : memoryText + "ENGINE ASLEEP";
    g.drawText (engineText, chip, juce::Justification::centred, false);

    if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Reading
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::FindingRoot
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::Analysing
        || snapshot.stage == NeuramarAudioProcessor::LearningStage::Training)
    {
        const auto ringBounds = juce::Rectangle<float> (72.0f, 72.0f).withCentre (centre);
        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, 34.0f, 34.0f, 0.0f,
                            -juce::MathConstants<float>::halfPi,
                            -juce::MathConstants<float>::halfPi
                                + juce::MathConstants<float>::twoPi
                                      * juce::jlimit (0.0f, 1.0f, snapshot.progress),
                            true);
        g.setColour (accent.withAlpha (0.18f));
        g.drawEllipse (ringBounds, 4.0f);
        g.setColour (accent);
        g.strokePath (ring, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setFont (font (13.0f, true));
        g.drawText (juce::String (juce::roundToInt (snapshot.progress * 100.0f)) + "%",
                    ringBounds, juce::Justification::centred, false);
    }
    else if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Empty)
    {
        g.setColour (accent.withAlpha (dragHover ? 0.95f : 0.62f));
        g.setFont (font (36.0f, true));
        g.drawText ("+", juce::Rectangle<float> (80.0f, 52.0f).withCentre (centre),
                    juce::Justification::centred, false);
    }

    auto footer = bounds.reduced (22.0f).removeFromBottom (34.0f);
    g.setColour (colour (muted));
    g.setFont (font (10.5f));
    auto message = snapshot.message;
    if (message.isEmpty())
        message = snapshot.stage == NeuramarAudioProcessor::LearningStage::Empty
            ? "WAV / AIFF / FLAC / OGG  -  monophonic sounds reveal the clearest roots"
            : snapshot.sourceName;
    g.drawFittedText (message, footer.toNearestInt(), juce::Justification::centredLeft,
                      1, 0.76f);
}

void NeuralPoolDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && onChooseFile)
        onChooseFile();
}

ModelAnatomyDisplay::ModelAnatomyDisplay()
{
    neuramar::clearModelAnatomy (anatomy);
}

void ModelAnatomyDisplay::setAnatomy (const neuramar::ModelAnatomy& next,
                                      std::uint64_t generation)
{
    if (generation == anatomyGeneration)
        return;
    anatomyGeneration = generation;
    anatomy = next;
    playhead = 0.0f;
    repaint();
}

void ModelAnatomyDisplay::advance (int activeVoiceCount, float stretch)
{
    stretchAmount = stretch;
    // One sweep of the learned trajectory every four seconds keeps slow
    // memories readable without turning fast ones into a strobe.
    const auto step = anatomy.durationSeconds > 0.0f
        ? 1.0f / (30.0f * juce::jlimit (1.0f, 8.0f, anatomy.durationSeconds
                                        + 1.5f))
        : 0.008f;
    playhead = std::fmod (playhead + step, 1.0f);

    const auto slice = neuramar::anatomySliceAt (playhead);
    const auto voiceGain = activeVoiceCount > 0 ? 1.0f : 0.35f;
    coreMeter = neuramar::followMeter (
        coreMeter, anatomy.coreLevel[slice] * voiceGain, 0.55f, 0.14f);
    airMeter = neuramar::followMeter (
        airMeter, anatomy.airLevel[slice] * voiceGain, 0.55f, 0.14f);
    boneMeter = neuramar::followMeter (
        boneMeter, anatomy.boneLevel[slice] * voiceGain, 0.55f, 0.14f);
    repaint();
}

juce::Rectangle<float> ModelAnatomyDisplay::plotBounds() const
{
    auto plot = getLocalBounds().toFloat().reduced (14.0f, 10.0f);
    plot.removeFromTop (20.0f);
    plot.removeFromBottom (16.0f);
    return plot;
}

void ModelAnatomyDisplay::paintGrid (juce::Graphics& g,
                                     juce::Rectangle<float> plot) const
{
    static constexpr std::array<float, 7> decades {
        50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 4000.0f, 12000.0f
    };
    g.setFont (font (7.6f, true));
    for (const auto frequency : decades)
    {
        const auto position = neuramar::anatomyPositionOf (frequency);
        const auto x = plot.getX() + plot.getWidth() * position;
        g.setColour (colour (ink).withAlpha (0.055f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.7f);
        g.setColour (colour (muted).withAlpha (0.38f));
        g.drawText (frequency >= 1000.0f
                        ? juce::String (juce::roundToInt (frequency / 1000.0f)) + "k"
                        : juce::String (juce::roundToInt (frequency)),
                    juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 1.0f,
                                            32.0f, 13.0f),
                    juce::Justification::centred, false);
    }
    for (int line = 1; line < 4; ++line)
    {
        const auto y = plot.getY() + plot.getHeight()
                     * static_cast<float> (line) / 4.0f;
        g.setColour (colour (ink).withAlpha (0.035f));
        g.drawLine (plot.getX(), y, plot.getRight(), y, 0.6f);
    }
}

void ModelAnatomyDisplay::paintSpectrum (juce::Graphics& g,
                                         juce::Rectangle<float> plot)
{
    const auto slice = neuramar::anatomySliceAt (playhead);
    const auto binX = [plot] (std::size_t bin)
    {
        return plot.getX() + plot.getWidth() * static_cast<float> (bin)
             / static_cast<float> (neuramar::ModelAnatomy::binCount - 1);
    };

    const auto buildPath = [&] (
        const std::array<float, neuramar::ModelAnatomy::binCount>& values,
        bool closed)
    {
        juce::Path path;
        path.startNewSubPath (binX (0), plot.getBottom()
                              - plot.getHeight() * values[0]);
        for (std::size_t bin = 1; bin < values.size(); ++bin)
            path.lineTo (binX (bin),
                         plot.getBottom() - plot.getHeight() * values[bin]);
        if (closed)
        {
            path.lineTo (binX (values.size() - 1), plot.getBottom());
            path.lineTo (binX (0), plot.getBottom());
            path.closeSubPath();
        }
        return path;
    };

    // The faint outline is the loudest each cell ever gets across the whole
    // learned trajectory, so the moving slice is read against the memory's
    // total spectral reach rather than in isolation.
    std::array<float, neuramar::ModelAnatomy::binCount> envelope {};
    for (std::size_t s = 0; s < neuramar::ModelAnatomy::sliceCount; ++s)
        for (std::size_t bin = 0; bin < envelope.size(); ++bin)
            envelope[bin] = std::max (envelope[bin],
                std::max (anatomy.core[s][bin],
                          std::max (anatomy.air[s][bin],
                                    anatomy.bone[s][bin])));
    g.setColour (colour (ink).withAlpha (0.14f));
    g.strokePath (buildPath (envelope, false),
                  juce::PathStrokeType (0.9f, juce::PathStrokeType::curved));

    const auto airPath = buildPath (anatomy.air[slice], true);
    g.setColour (colour (clay).withAlpha (0.24f));
    g.fillPath (airPath);
    g.setColour (colour (clay).withAlpha (0.72f));
    g.strokePath (buildPath (anatomy.air[slice], false),
                  juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));

    const auto corePath = buildPath (anatomy.core[slice], true);
    g.setColour (colour (mint).withAlpha (0.20f));
    g.fillPath (corePath);
    g.setColour (colour (mint).withAlpha (0.90f));
    g.strokePath (buildPath (anatomy.core[slice], false),
                  juce::PathStrokeType (1.35f, juce::PathStrokeType::curved));

    for (std::size_t bin = 0; bin < neuramar::ModelAnatomy::binCount; ++bin)
    {
        const auto height = anatomy.bone[slice][bin];
        if (height <= 0.001f)
            continue;
        const auto x = binX (bin);
        g.setColour (colour (violet).withAlpha (0.80f));
        g.drawLine (x, plot.getBottom(),
                    x, plot.getBottom() - plot.getHeight() * height, 1.6f);
    }

    if (hovering && hoverPosition >= 0.0f)
    {
        const auto x = plot.getX() + plot.getWidth() * hoverPosition;
        g.setColour (colour (ink).withAlpha (0.30f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.8f);
    }
}

void ModelAnatomyDisplay::paintEvolution (juce::Graphics& g,
                                          juce::Rectangle<float> plot)
{
    const auto sliceX = [plot] (std::size_t slice)
    {
        return plot.getX() + plot.getWidth() * static_cast<float> (slice)
             / static_cast<float> (neuramar::ModelAnatomy::sliceCount - 1);
    };
    const auto drawLayer = [&] (
        const std::array<float, neuramar::ModelAnatomy::sliceCount>& values,
        juce::Colour layerColour)
    {
        juce::Path path;
        path.startNewSubPath (sliceX (0),
                              plot.getBottom() - plot.getHeight() * values[0]);
        for (std::size_t slice = 1; slice < values.size(); ++slice)
            path.lineTo (sliceX (slice),
                         plot.getBottom() - plot.getHeight() * values[slice]);
        g.setColour (layerColour.withAlpha (0.86f));
        g.strokePath (path, juce::PathStrokeType (
            1.5f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        juce::Path filled = path;
        filled.lineTo (sliceX (values.size() - 1), plot.getBottom());
        filled.lineTo (sliceX (0), plot.getBottom());
        filled.closeSubPath();
        g.setColour (layerColour.withAlpha (0.13f));
        g.fillPath (filled);
    };

    drawLayer (anatomy.airLevel, colour (clay));
    drawLayer (anatomy.boneLevel, colour (violet));
    drawLayer (anatomy.coreLevel, colour (mint));

    const auto x = plot.getX() + plot.getWidth() * playhead;
    g.setColour (colour (ink).withAlpha (0.42f));
    g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
}

void ModelAnatomyDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient surface (colour (0xff0b1f22u), bounds.getX(),
                                  bounds.getY(), colour (0xff050f12u),
                                  bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, 16.0f);
    g.setColour (colour (mint).withAlpha (0.14f));
    g.drawRoundedRectangle (bounds.reduced (0.7f), 16.0f, 0.9f);

    const auto plot = plotBounds();
    if (plot.getWidth() < 8.0f || plot.getHeight() < 8.0f)
        return;
    paintGrid (g, plot);

    auto header = bounds.reduced (14.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (font (9.2f, true));
    g.setColour (colour (muted));
    g.drawText (view == View::Spectrum ? "MODEL ANATOMY  /  SPECTRUM"
                                       : "MODEL ANATOMY  /  EVOLUTION",
                header.removeFromLeft (190.0f),
                juce::Justification::centredLeft, false);

    if (! anatomy.valid)
    {
        g.setColour (colour (muted).withAlpha (0.62f));
        g.setFont (font (10.5f));
        g.drawText ("no memory to inspect yet", plot,
                    juce::Justification::centred, false);
        return;
    }

    if (view == View::Spectrum)
        paintSpectrum (g, plot);
    else
        paintEvolution (g, plot);

    // Layer meters, in the same order and colours as the header legend.
    auto meters = header.removeFromRight (168.0f);
    if (meters.getWidth() > 60.0f)
    {
        const auto meterWidth = meters.getWidth() / 3.0f;
        const std::array<std::pair<float, juce::Colour>, 3> layerMeters {{
            { coreMeter, colour (mint) },
            { airMeter, colour (clay) },
            { boneMeter, colour (violet) }
        }};
        for (std::size_t index = 0; index < layerMeters.size(); ++index)
        {
            auto cell = meters.removeFromLeft (meterWidth).reduced (3.0f, 5.0f);
            g.setColour (colour (ink).withAlpha (0.07f));
            g.fillRoundedRectangle (cell, 2.0f);
            const auto filled = cell.withWidth (
                cell.getWidth() * juce::jlimit (0.0f, 1.0f,
                                                layerMeters[index].first));
            g.setColour (layerMeters[index].second.withAlpha (0.80f));
            g.fillRoundedRectangle (filled, 2.0f);
        }
    }

    auto footer = bounds.reduced (14.0f, 8.0f).removeFromBottom (13.0f);
    juce::String detail;
    if (hovering && hoverPosition >= 0.0f && view == View::Spectrum)
    {
        const auto frequency = neuramar::anatomyFrequencyAt (hoverPosition);
        detail = (frequency >= 1000.0f
                      ? juce::String (frequency / 1000.0f, 2) + " kHz"
                      : juce::String (juce::roundToInt (frequency)) + " Hz");
    }
    else
    {
        const auto stretchCents = 1200.0f * std::log2 (
            std::max (neuramar::stretchedHarmonicRatio (
                          16.0f, anatomy.inharmonicity
                              * juce::jlimit (0.0f, 2.0f, stretchAmount))
                          / 16.0f, 1.0e-6f));
        detail = anatomy.inharmonicity > 0.0f
            ? "stretch " + juce::String (juce::roundToInt (stretchCents))
                  + "c at partial 16"
            : juce::String ("ideal harmonic series");
    }
    g.setColour (colour (muted).withAlpha (0.80f));
    g.setFont (font (9.0f));
    g.drawText (detail + "  /  click to swap view", footer,
                juce::Justification::centredRight, false);
}

void ModelAnatomyDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;
    view = view == View::Spectrum ? View::Evolution : View::Spectrum;
    repaint();
}

void ModelAnatomyDisplay::mouseMove (const juce::MouseEvent& event)
{
    const auto plot = plotBounds();
    if (plot.getWidth() <= 0.0f)
        return;
    hovering = true;
    hoverPosition = juce::jlimit (0.0f, 1.0f,
        (event.position.x - plot.getX()) / plot.getWidth());
    repaint();
}

void ModelAnatomyDisplay::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    hoverPosition = -1.0f;
    repaint();
}

NeuramarAudioProcessorEditor::NeuramarAudioProcessorEditor (NeuramarAudioProcessor& p)
    : AudioProcessorEditor (&p), neuramarProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    logoLabel.setText ("NEURAMAR", juce::dontSendNotification);
    logoLabel.setFont (font (29.0f, true));
    logoLabel.setColour (juce::Label::textColourId, colour (ink));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    taglineLabel.setText ("TEACH A SOUND. GROW A MEMORY.", juce::dontSendNotification);
    taglineLabel.setFont (font (10.5f, true));
    taglineLabel.setColour (juce::Label::textColourId, colour (mint).withAlpha (0.84f));
    taglineLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (taglineLabel);

    modelLabel.setText ("DDSP-INSPIRED  /  NO CLOUD  /  NO SAMPLE PLAYBACK",
                        juce::dontSendNotification);
    modelLabel.setFont (font (9.5f, true));
    modelLabel.setColour (juce::Label::textColourId, colour (muted));
    modelLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (modelLabel);

    rootLabel.setText ("INFERRED ROOT", juce::dontSendNotification);
    rootLabel.setFont (font (10.5f, true));
    rootLabel.setColour (juce::Label::textColourId, colour (muted));
    rootLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootLabel);

    rootValueLabel.setText ("--", juce::dontSendNotification);
    rootValueLabel.setFont (font (29.0f, true));
    rootValueLabel.setColour (juce::Label::textColourId, colour (mint));
    rootValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootValueLabel);

    rootConfidenceLabel.setText ("drop a sound to begin", juce::dontSendNotification);
    rootConfidenceLabel.setFont (font (9.0f));
    rootConfidenceLabel.setColour (juce::Label::textColourId, colour (muted));
    rootConfidenceLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootConfidenceLabel);

    neuralPool.onChooseFile = [this] { chooseSampleFile(); };
    neuralPool.setAccessible (true);
    neuralPool.setTitle ("Neural sound drop area");
    neuralPool.setDescription (
        "Drop or choose an audio file to infer its root and learn a playable synthesis model.");
    neuralPool.setTooltip ("Drop a supported audio file here, or click to choose one.");
    addAndMakeVisible (neuralPool);

    modelAnatomy.setAccessible (true);
    modelAnatomy.setTitle ("Learned model anatomy");
    modelAnatomy.setDescription (
        "Shows what the learned memory contains: its Core partials, Air bands, "
        "Bone modes, and how each layer evolves.");
    modelAnatomy.setTooltip (
        "Core / Air / Bone content of the learned memory. Click to swap "
        "between the spectral fingerprint and the layer evolution.");
    addAndMakeVisible (modelAnatomy);

    orbitButton.setClickingTogglesState (true);
    orbitButton.setDescription ("Repeat the learned stable region while a note is held.");
    orbitButton.setTooltip ("Revisit the learned stable region while a note is held.");
    loadButton.setTooltip ("Open a WAV, AIFF, FLAC, or OGG sound for local learning.");
    cancelButton.setTooltip ("Cancel the current background analysis and training pass.");
    panicButton.setTooltip ("Immediately stop all sounding voices.");
    randomizeButton.setDescription (
        "Generate a playable neural seed, or vary the current neural memory.");
    randomizeButton.setTooltip (
        "With no memory, grow a playable neural seed. With a memory, vary its weights "
        "within the selected musical range.");
    randomize1Button.setDescription (
        "Select subtle one-percent neural randomization.");
    randomize10Button.setDescription (
        "Select ten-percent neural evolution.");
    randomize100Button.setDescription (
        "Select full-range wild neural randomization.");
    randomize1Button.setTooltip (
        "Subtle: move neural weights through 1% of their bounded musical range.");
    randomize10Button.setTooltip (
        "Evolve: move neural weights through 10% of their bounded musical range.");
    randomize100Button.setTooltip (
        "Wild: use the full bounded musical range to create a new neural identity.");
    rootDownButton.setTooltip ("Move the model's root anchor down by one semitone.");
    rootUpButton.setTooltip ("Move the model's root anchor up by one semitone.");

    constexpr int randomizationRadioGroup = 0x4e52;
    for (auto* button : std::array<juce::TextButton*, 3> {
             &randomize1Button, &randomize10Button, &randomize100Button })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (randomizationRadioGroup);
    }
    randomize10Button.setToggleState (true, juce::dontSendNotification);

    for (auto* button : std::array<juce::Button*, 10> {
             &loadButton, &cancelButton, &panicButton,
             &rootDownButton, &rootUpButton, &orbitButton,
             &randomizeButton, &randomize1Button, &randomize10Button,
             &randomize100Button })
        addAndMakeVisible (*button);

    loadButton.onClick = [this] { chooseSampleFile(); };
    cancelButton.onClick = [this]
    {
        neuramarProcessor.requestLearningCancellation();
    };
    panicButton.onClick = [this] { neuramarProcessor.requestPanic(); };
    randomizeButton.onClick = [this]
    {
        neuramarProcessor.randomizeModel (randomizationAmount);
    };
    randomize1Button.onClick = [this]
    {
        randomizationAmount =
            NeuramarAudioProcessor::RandomizationAmount::Subtle1Percent;
    };
    randomize10Button.onClick = [this]
    {
        randomizationAmount =
            NeuramarAudioProcessor::RandomizationAmount::Evolve10Percent;
    };
    randomize100Button.onClick = [this]
    {
        randomizationAmount =
            NeuramarAudioProcessor::RandomizationAmount::Wild100Percent;
    };
    rootDownButton.onClick = [this] { nudgeRoot (-1); };
    rootUpButton.onClick = [this] { nudgeRoot (1); };

    for (auto* knob : std::array<NeuramarKnob*, 16> {
             &imprintKnob, &bodyLockKnob, &airKnob, &boneKnob,
             &brightnessKnob, &evolutionKnob, &mutationKnob,
             &noiseKnob, &attackKnob, &releaseKnob, &spreadKnob, &outputKnob,
             &stretchKnob, &formantKnob, &touchKnob, &registerKnob })
        addAndMakeVisible (*knob);

    bodyLockKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    boneKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (clay));
    brightnessKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));
    evolutionKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    mutationKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    noiseKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    outputKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));
    formantKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (violet));
    touchKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));
    registerKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour (amber));
    registerKnob.slider.setDescription (
        "Key tracking: darkens notes above the learned root and opens up notes "
        "below it.");
    registerKnob.slider.setTooltip (
        "Key-tracked spectral tilt. Positive darkens notes played above the "
        "learned root and opens up notes below it; negative inverts that. It "
        "changes tone, not level.");
    stretchKnob.slider.setDescription (
        "Scales the memory's fitted stiff-string partial stretch.");
    stretchKnob.slider.setTooltip (
        "Stretch: 100% renders the learned stiff-string partial series, 0% "
        "forces an ideal harmonic bank. A memory with no fitted stretch is "
        "unaffected.");
    formantKnob.slider.setDescription (
        "Moves the learned resonant body in frequency without moving pitch.");
    formantKnob.slider.setTooltip (
        "Formant: shifts the Body-Locked Core envelope, Air bands, and Bone "
        "modes in semitones. The played pitch does not move.");
    touchKnob.slider.setDescription (
        "Sets how much MIDI velocity shapes timbre as well as level.");
    touchKnob.slider.setTooltip (
        "Touch: harder notes become brighter and breathier, softer notes "
        "darker. At 0% velocity only sets level.");
    noiseKnob.slider.setDescription (
        "Evolves the neural model's latent input; it does not add audible hiss.");
    noiseKnob.slider.setTooltip (
        "Latent evolution: perturbs the neural input over time. This is not an audible "
        "hiss or noise-layer level.");

    attachSlider (imprintKnob.slider, neuramar::parameters::imprint);
    attachSlider (bodyLockKnob.slider, neuramar::parameters::bodyLock);
    attachSlider (airKnob.slider, neuramar::parameters::air);
    attachSlider (boneKnob.slider, neuramar::parameters::bone);
    attachSlider (brightnessKnob.slider, neuramar::parameters::brightness);
    attachSlider (evolutionKnob.slider, neuramar::parameters::evolutionRate);
    attachSlider (mutationKnob.slider, neuramar::parameters::mutation);
    attachSlider (noiseKnob.slider, neuramar::parameters::noise);
    attachSlider (attackKnob.slider, neuramar::parameters::attack);
    attachSlider (releaseKnob.slider, neuramar::parameters::release);
    attachSlider (spreadKnob.slider, neuramar::parameters::spread);
    attachSlider (outputKnob.slider, neuramar::parameters::output);
    attachSlider (stretchKnob.slider, neuramar::parameters::stretch);
    attachSlider (formantKnob.slider, neuramar::parameters::formant);
    attachSlider (touchKnob.slider, neuramar::parameters::touch);
    attachSlider (registerKnob.slider, neuramar::parameters::registerTilt);
    attachButton (orbitButton, neuramar::parameters::orbit);

    keyboard.setAvailableRange (0, 127);
    keyboard.setLowestVisibleKey (24);
    keyboard.setKeyWidth (18.0f);
    keyboard.setScrollButtonsVisible (true);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setTitle ("Playable MIDI keyboard");
    keyboard.setDescription ("Play the learned neural instrument with the mouse.");
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setSizeLimits (960, 716, 1500, 1119);
        constrainer->setFixedAspectRatio (1180.0 / 880.0);
    }
    setSize (1180, 880);
    timerCallback();
    startTimerHz (30);
}

NeuramarAudioProcessorEditor::~NeuramarAudioProcessorEditor()
{
    stopTimer();
    fileChooser.reset();
    setLookAndFeel (nullptr);
}

void NeuramarAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawOrganicField (g, bounds);

    auto content = getLocalBounds().reduced (18);
    content.removeFromTop (58);
    content.removeFromBottom (99);
    const auto upper = content.removeFromTop (juce::roundToInt (
        static_cast<float> (content.getHeight()) * 0.66f));
    const auto poolWidth = juce::roundToInt (
        static_cast<float> (upper.getWidth()) * 0.665f);
    drawPanel (g, upper.withWidth (poolWidth).toFloat());
    drawPanel (g, upper.withTrimmedLeft (poolWidth + 12).toFloat());
    if (content.getHeight() > 0)
        drawPanel (g, content.withTrimmedTop (10).toFloat());

    juce::ColourGradient headerLine (colour (mint).withAlpha (0.38f), 18.0f, 64.0f,
                                     colour (violet).withAlpha (0.10f),
                                     static_cast<float> (getWidth() - 18), 64.0f, false);
    g.setGradientFill (headerLine);
    g.fillRoundedRectangle (18.0f, 63.5f,
                            static_cast<float> (getWidth() - 36), 1.0f, 0.5f);

    g.setColour (colour (muted).withAlpha (0.45f));
    g.setFont (font (8.0f, true));
    g.drawText ("CORE", getWidth() - 175, 48, 42, 12, juce::Justification::centred, false);
    g.setColour (colour (mint));
    g.fillEllipse (static_cast<float> (getWidth() - 132), 52.0f, 4.0f, 4.0f);
    g.setColour (colour (muted).withAlpha (0.45f));
    g.drawText ("AIR", getWidth() - 124, 48, 34, 12, juce::Justification::centred, false);
    g.setColour (colour (clay));
    g.fillEllipse (static_cast<float> (getWidth() - 88), 52.0f, 4.0f, 4.0f);
    g.setColour (colour (muted).withAlpha (0.45f));
    g.drawText ("BONE", getWidth() - 80, 48, 45, 12, juce::Justification::centred, false);
}

void NeuramarAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    auto header = bounds.removeFromTop (58);
    auto logo = header.removeFromLeft (230);
    logoLabel.setBounds (logo.removeFromTop (36));
    taglineLabel.setBounds (logo);
    modelLabel.setBounds (header.withTrimmedBottom (10));

    auto keyboardArea = bounds.removeFromBottom (89);
    keyboard.setBounds (keyboardArea.withTrimmedTop (7));

    auto upper = bounds.removeFromTop (juce::roundToInt (
        static_cast<float> (bounds.getHeight()) * 0.66f));
    const auto poolWidth = juce::roundToInt (
        static_cast<float> (upper.getWidth()) * 0.665f);
    auto poolArea = upper.removeFromLeft (poolWidth).reduced (8);
    auto anatomyArea = poolArea.removeFromBottom (juce::roundToInt (
        static_cast<float> (poolArea.getHeight()) * 0.42f));
    neuralPool.setBounds (poolArea);
    anatomyArea.removeFromTop (8);
    modelAnatomy.setBounds (anatomyArea);

    upper.removeFromLeft (12);
    auto dnaArea = upper.reduced (10);
    auto rootArea = dnaArea.removeFromTop (76);
    rootLabel.setBounds (rootArea.removeFromTop (18));
    auto rootLine = rootArea.removeFromTop (36);
    rootDownButton.setBounds (rootLine.removeFromLeft (38).reduced (4));
    rootUpButton.setBounds (rootLine.removeFromRight (38).reduced (4));
    rootValueLabel.setBounds (rootLine);
    rootConfidenceLabel.setBounds (rootArea);

    dnaArea.removeFromTop (2);
    constexpr int columns = 5;
    constexpr int rows = 2;
    const auto cellWidth = dnaArea.getWidth() / columns;
    const auto cellHeight = dnaArea.getHeight() / rows;
    std::array<NeuramarKnob*, 10> dnaKnobs {
        &imprintKnob, &bodyLockKnob, &airKnob, &boneKnob, &brightnessKnob,
        &stretchKnob, &formantKnob, &touchKnob, &registerKnob, &mutationKnob
    };
    for (int i = 0; i < static_cast<int> (dnaKnobs.size()); ++i)
    {
        const auto row = i / columns;
        const auto column = i % columns;
        dnaKnobs[static_cast<std::size_t> (i)]->setBounds (
            dnaArea.getX() + column * cellWidth,
            dnaArea.getY() + row * cellHeight,
            cellWidth, cellHeight);
    }

    bounds.removeFromTop (10);
    auto performance = bounds.reduced (10, 8);
    auto actions = performance.removeFromRight (
        juce::jmax (270, performance.getWidth() / 4));
    auto firstActionRow = actions.removeFromTop (actions.getHeight() / 2);
    const auto firstActionWidth = firstActionRow.getWidth() / 3;
    orbitButton.setBounds (
        firstActionRow.removeFromLeft (firstActionWidth).reduced (4));
    loadButton.setBounds (
        firstActionRow.removeFromLeft (firstActionWidth).reduced (4));
    randomizeButton.setBounds (firstActionRow.reduced (4));

    auto secondActionRow = actions;
    auto strengthArea = secondActionRow.removeFromLeft (
        secondActionRow.getWidth() / 2);
    const auto strengthWidth = strengthArea.getWidth() / 3;
    randomize1Button.setBounds (
        strengthArea.removeFromLeft (strengthWidth).reduced (4));
    randomize10Button.setBounds (
        strengthArea.removeFromLeft (strengthWidth).reduced (4));
    randomize100Button.setBounds (strengthArea.reduced (4));
    cancelButton.setBounds (
        secondActionRow.removeFromLeft (secondActionRow.getWidth() / 2).reduced (4));
    panicButton.setBounds (secondActionRow.reduced (4));

    std::array<NeuramarKnob*, 6> performanceKnobs {
        &evolutionKnob, &noiseKnob, &attackKnob,
        &releaseKnob, &spreadKnob, &outputKnob
    };
    const auto performanceWidth = performance.getWidth()
                                / static_cast<int> (performanceKnobs.size());
    for (int i = 0; i < static_cast<int> (performanceKnobs.size()); ++i)
        performanceKnobs[static_cast<std::size_t> (i)]->setBounds (
            performance.getX() + i * performanceWidth, performance.getY(),
            performanceWidth, performance.getHeight());
}

bool NeuramarAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& path)
    {
        return NeuramarAudioProcessor::isSupportedSampleFile (juce::File (path));
    });
}

void NeuramarAudioProcessorEditor::filesDropped (const juce::StringArray& files,
                                                  int, int)
{
    neuralPool.setDragHover (false);
    for (const auto& path : files)
    {
        const juce::File file (path);
        if (NeuramarAudioProcessor::isSupportedSampleFile (file))
        {
            neuramarProcessor.loadSampleFile (file);
            return;
        }
    }
}

void NeuramarAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    neuralPool.setDragHover (true);
}

void NeuramarAudioProcessorEditor::fileDragExit (const juce::StringArray&)
{
    neuralPool.setDragHover (false);
}

void NeuramarAudioProcessorEditor::timerCallback()
{
    const auto snapshot = neuramarProcessor.getLearningSnapshot();
    neuralPool.setSnapshot (snapshot, neuramarProcessor.getActiveVoiceCount(),
                            neuramarProcessor.getCurrentSampleRateForDisplay());
    updateRootReadout (snapshot);

    // The anatomy is only copied when a new memory is published; every other
    // frame just advances the sweep and the layer meters.
    if (snapshot.modelGeneration != lastAnatomyGeneration)
    {
        const auto anatomySnapshot = neuramarProcessor.getModelAnatomySnapshot();
        lastAnatomyGeneration = anatomySnapshot.modelGeneration;
        modelAnatomy.setAnatomy (anatomySnapshot.anatomy,
                                 anatomySnapshot.modelGeneration);
    }
    auto stretchAmount = 1.0f;
    if (const auto* raw = neuramarProcessor.parameters.getRawParameterValue (
            neuramar::parameters::stretch))
        stretchAmount = raw->load (std::memory_order_relaxed);
    modelAnatomy.advance (neuramarProcessor.getActiveVoiceCount(),
                          stretchAmount);

    const auto learning = snapshot.stage == NeuramarAudioProcessor::LearningStage::Reading
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::FindingRoot
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::Analysing
                       || snapshot.stage == NeuramarAudioProcessor::LearningStage::Training;
    cancelButton.setEnabled (learning);
    cancelButton.setAlpha (learning ? 1.0f : 0.68f);
}

void NeuramarAudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                  const char* parameterId)
{
    sliderAttachments.push_back (
        std::make_unique<SliderAttachment> (neuramarProcessor.parameters, parameterId, slider));
}

void NeuramarAudioProcessorEditor::attachButton (juce::Button& button,
                                                  const char* parameterId)
{
    buttonAttachments.push_back (
        std::make_unique<ButtonAttachment> (neuramarProcessor.parameters, parameterId, button));
}

void NeuramarAudioProcessorEditor::chooseSampleFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Teach Neuramar a sound", juce::File {}, "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<NeuramarAudioProcessorEditor> safeThis (this);
    fileChooser->launchAsync (flags, [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;
        const auto file = chooser.getResult();
        if (file.existsAsFile())
            safeThis->neuramarProcessor.loadSampleFile (file);
        safeThis->fileChooser.reset();
    });
}

void NeuramarAudioProcessorEditor::nudgeRoot (int semitones)
{
    auto* parameter = neuramarProcessor.parameters.getParameter (
        neuramar::parameters::rootCorrection);
    if (parameter == nullptr)
        return;

    const auto current = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
    const auto next = juce::jlimit (-12, 12, current + semitones);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (next)));
    parameter->endChangeGesture();
}

void NeuramarAudioProcessorEditor::updateRootReadout (
    const NeuramarAudioProcessor::LearningSnapshot& snapshot)
{
    if (snapshot.rootMidiNote < 0)
    {
        rootLabel.setText ("INFERRED ROOT", juce::dontSendNotification);
        rootValueLabel.setText ("--", juce::dontSendNotification);
        rootConfidenceLabel.setText (
            "drop a sound or randomize", juce::dontSendNotification);
        rootDownButton.setEnabled (false);
        rootUpButton.setEnabled (false);
        return;
    }

    auto correction = 0;
    if (const auto* raw = neuramarProcessor.parameters.getRawParameterValue (
            neuramar::parameters::rootCorrection))
        correction = juce::roundToInt (raw->load (std::memory_order_relaxed));

    const auto correctedNote = juce::jlimit (0, 127, snapshot.rootMidiNote + correction);
    const auto centsText = (snapshot.rootCents >= 0.0f ? "+" : "")
                         + juce::String (juce::roundToInt (snapshot.rootCents)) + "c";
    rootValueLabel.setText (NeuramarAudioProcessor::noteName (correctedNote)
                                + "  " + centsText,
                            juce::dontSendNotification);
    rootLabel.setText (
        snapshot.generatedModel ? "GENERATED ROOT" : "INFERRED ROOT",
        juce::dontSendNotification);
    const auto correctionText = correction == 0
        ? juce::String {}
        : "  /  corrected " + juce::String (correction > 0 ? "+" : "")
            + juce::String (correction) + " st";
    rootConfidenceLabel.setText (
        snapshot.generatedModel
            ? "neural pitch anchor" + correctionText
            : juce::String (juce::roundToInt (
                  snapshot.rootConfidence * 100.0f))
                + "% confidence" + correctionText,
        juce::dontSendNotification);
    rootDownButton.setEnabled (correction > -12);
    rootUpButton.setEnabled (correction < 12);
}

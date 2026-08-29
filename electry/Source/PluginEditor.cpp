#include "PluginEditor.h"

#include "DSP/ElectryVisuals.h"

#include <BinaryData.h>

#include <cmath>

namespace
{
namespace colours
{
const juce::Colour background { 0xff100907 };
const juce::Colour panel { 0xe3110e0c };
const juce::Colour panelTop { 0xeb211815 };
const juce::Colour panelOutline { 0xff806542 };
const juce::Colour binding { 0xffeadaba };
const juce::Colour text { 0xfff6f0e4 };
const juce::Colour dimText { 0xffcbb997 };
const juce::Colour accent { 0xffbb8544 };
const juce::Colour accentBright { 0xffddb16b };
const juce::Colour accentDark { 0xff57251d };
const juce::Colour oxblood { 0xff5c211d };
const juce::Colour knobFace { 0xff151210 };
const juce::Colour bakeliteEdge { 0xff090807 };
const juce::Colour nickel { 0xffaaa18f };
const juce::Colour warmBone { 0xffe4d8ba };
const juce::Colour ebony { 0xff17110e };
const juce::Colour keyswitchBlack { 0xff51201d };
const juce::Colour rosewood { 0xff33211a };
const juce::Colour rosewoodDark { 0xff1d120e };
const juce::Colour fretWire { 0xffbdb4a2 };
const juce::Colour sympatheticRing { 0xff6fa8b8 };
} // namespace colours

constexpr int editorWidth = 1080;
constexpr int editorHeight = 860;
constexpr int fretboardPanelHeight = 152;
constexpr int statusDisplayWidth = 256;
constexpr int timerHz = 30;
constexpr int lastDrawnFret = electry::ElectryEngine::fretCount;
constexpr int firstKeyboardNote = electry::ElectryEngine::firstKeyswitchNote; // C0
constexpr int firstPlayableNote = electry::ElectryEngine::lowestPlayableNote; // E1
constexpr int lastKeyboardNote = electry::ElectryEngine::highestPlayableNote; // D6
constexpr int keyswitchCount = electry::ElectryEngine::keyswitchCount;
constexpr int keyboardWhiteKeyCount = 44; // C0..D6 inclusive
constexpr auto visualWeightProperty = "electryVisualWeight";
constexpr float compactKnobWeight = 0.65f;
constexpr int sectionTitleHeight = 28;
constexpr int sectionContentTrim = sectionTitleHeight - 10;

enum class KnobTier
{
    detail,
    contextual,
    hero,
    master
};

struct KnobTierMetrics
{
    int width;
    int height;
    float visualWeight;
};

constexpr KnobTierMetrics metricsFor (KnobTier tier) noexcept
{
    switch (tier)
    {
        case KnobTier::detail:     return { 68, 110, 0.52f };
        case KnobTier::contextual: return { 80, 116, 0.72f };
        case KnobTier::hero:       return { 104, 128, 1.00f };
        case KnobTier::master:     return { 80, 122, 0.94f };
    }
    return { 88, 122, 0.82f };
}

static_assert (metricsFor (KnobTier::detail).visualWeight < compactKnobWeight);
static_assert (metricsFor (KnobTier::contextual).visualWeight >= compactKnobWeight);

struct KnobSlot
{
    ElectryKnob* knob;
    KnobTier tier;
};

void layoutKnobRow (juce::Rectangle<int> rowArea,
                    std::initializer_list<KnobSlot> slots, int gap)
{
    if (slots.size() == 0 || rowArea.isEmpty())
        return;

    int preferredWidth = gap * (static_cast<int> (slots.size()) - 1);
    int preferredHeight = 0;
    for (const auto& slot : slots)
    {
        preferredWidth += metricsFor (slot.tier).width;
        preferredHeight = juce::jmax (preferredHeight,
                                      metricsFor (slot.tier).height);
    }

    const auto scale = juce::jmin (
        1.0f, static_cast<float> (rowArea.getWidth())
                  / static_cast<float> (juce::jmax (1, preferredWidth)));

    int groupWidth = gap * (static_cast<int> (slots.size()) - 1);
    for (const auto& slot : slots)
        groupWidth += juce::jmax (1, juce::roundToInt (
            static_cast<float> (metricsFor (slot.tier).width) * scale));

    int x = rowArea.getX() + juce::jmax (0, (rowArea.getWidth() - groupWidth) / 2);
    const int groupHeight = juce::jmin (rowArea.getHeight(), preferredHeight);
    const int y = rowArea.getCentreY() - groupHeight / 2;
    for (const auto& slot : slots)
    {
        const auto metrics = metricsFor (slot.tier);
        const int width = juce::jmax (1, juce::roundToInt (
            static_cast<float> (metrics.width) * scale));
        slot.knob->slider.getProperties().set (
            visualWeightProperty, metrics.visualWeight);
        slot.knob->setBounds (x, y, width, groupHeight);
        slot.knob->repaint();
        x += width + gap;
    }
}

constexpr std::array<const char*, keyswitchCount> keyswitchLabels {
    "DN", "UP", "ALT", "SUS", "MUT", "H/P", "HRM", "PNC", "SLD", "X"
};

bool isKeyswitch (int midiNoteNumber) noexcept
{
    return midiNoteNumber >= firstKeyboardNote
        && midiNoteNumber < firstKeyboardNote + keyswitchCount;
}

bool isVibratoGesture (int midiNoteNumber) noexcept
{
    return electry::ElectryEngine::isVibratoGestureNote (midiNoteNumber);
}

bool isTremoloGesture (int midiNoteNumber) noexcept
{
    return electry::ElectryEngine::isTremoloGestureNote (midiNoteNumber);
}

// The remaining gaps around the playing range are dead: the engine ignores
// them, and they are drawn muted so nobody hunts for a sound there.
bool isDeadZoneNote (int midiNoteNumber) noexcept
{
    return midiNoteNumber >= firstKeyboardNote + keyswitchCount
        && midiNoteNumber < firstPlayableNote
        && ! isVibratoGesture (midiNoteNumber)
        && ! isTremoloGesture (midiNoteNumber);
}

void drawKeyswitchDecoration (juce::Graphics& graphics, juce::Rectangle<float> area,
                              int keyswitchIndex, bool selected, bool blackKey)
{
    auto badge = area.removeFromBottom (blackKey ? 16.0f : 20.0f)
                     .reduced (blackKey ? 1.0f : 2.0f, 2.0f);
    graphics.setColour (selected ? colours::accentBright
                                 : juce::Colours::black.withAlpha (0.34f));
    graphics.fillRoundedRectangle (badge, 2.5f);
    graphics.setColour (selected ? colours::rosewoodDark : colours::binding);
    graphics.setFont (juce::FontOptions (blackKey ? 7.4f : 9.0f, juce::Font::bold));
    graphics.drawFittedText (
        keyswitchLabels[static_cast<std::size_t> (keyswitchIndex)],
        badge.getSmallestIntegerContainer(), juce::Justification::centred,
        1, 0.72f);

    if (selected)
    {
        graphics.setColour (colours::oxblood.withAlpha (0.55f));
        graphics.drawRoundedRectangle (badge.reduced (0.5f), 2.0f, 0.8f);
    }
}

void drawVibratoDecoration (juce::Graphics& graphics,
                            juce::Rectangle<float> area, bool isDown)
{
    auto badge = area.removeFromBottom (16.0f).reduced (1.0f, 2.0f);
    graphics.setColour (isDown ? colours::accentBright
                               : colours::accentDark.brighter (0.18f));
    graphics.fillRoundedRectangle (badge, 2.5f);
    graphics.setColour (isDown ? colours::rosewoodDark : colours::binding);
    graphics.setFont (juce::FontOptions (7.4f, juce::Font::bold));
    graphics.drawFittedText ("VIB", badge.getSmallestIntegerContainer(),
                             juce::Justification::centred, 1, 0.72f);
}

void drawTremoloDecoration (juce::Graphics& graphics,
                            juce::Rectangle<float> area, bool isDown)
{
    auto badge = area.removeFromBottom (20.0f).reduced (2.0f);
    graphics.setColour (isDown ? colours::accentBright
                               : colours::accentDark.brighter (0.18f));
    graphics.fillRoundedRectangle (badge, 2.5f);
    graphics.setColour (isDown ? colours::rosewoodDark : colours::binding);
    graphics.setFont (juce::FontOptions (8.4f, juce::Font::bold));
    graphics.drawFittedText ("TRM", badge.getSmallestIntegerContainer(),
                             juce::Justification::centred, 1, 0.72f);
}

} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

ElectryLookAndFeel::ElectryLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId,
               juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::knobFace);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId,
               colours::panelOutline.withAlpha (0.75f));
    setColour (juce::ComboBox::arrowColourId, colours::accentBright);
    setColour (juce::TextButton::buttonColourId, colours::knobFace);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colours::warmBone);
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::ebony);
    setColour (juce::MidiKeyboardComponent::textLabelColourId,
               colours::rosewoodDark);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
               juce::Colour (0xff6d573b));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               colours::accent.withAlpha (0.35f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               colours::accent.withAlpha (0.7f));
}

void ElectryLookAndFeel::drawRotarySlider (juce::Graphics& graphics, int x, int y,
                                           int width, int height, float sliderPos,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const auto visualWeight = juce::jlimit (
        0.45f, 1.0f,
        static_cast<float> (slider.getProperties().getWithDefault (
            visualWeightProperty, 0.82f)));
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Primary controls keep the full 0..10 scale; compact texture controls
    // retain only its 0/5/10 anchors so they do not compete with the heroes.
    const int tickStep = visualWeight < compactKnobWeight ? 5 : 1;
    for (int tick = 0; tick <= 10; tick += tickStep)
    {
        const auto tickAngle = juce::jmap (static_cast<float> (tick), 0.0f, 10.0f,
                                           rotaryStartAngle, rotaryEndAngle);
        const auto outer = centre.getPointOnCircumference (radius, tickAngle);
        const auto inner = centre.getPointOnCircumference (
            radius - (tick % 5 == 0 ? 4.5f : 3.0f), tickAngle);
        graphics.setColour ((tick == 0 || tick == 10 ? colours::binding
                                                      : colours::dimText)
                                .withAlpha (0.24f + 0.32f * visualWeight));
        graphics.drawLine ({ inner, outer },
                           (tick % 5 == 0 ? 1.15f : 0.72f) * visualWeight);
    }

    // A hairline brass arc reads clearly without the neon halo of the old
    // control. The physical knob remains the visual focus.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 6.0f, radius - 6.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour (juce::Colours::black.withAlpha (0.62f));
    graphics.strokePath (track, juce::PathStrokeType (2.3f));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 6.0f, radius - 6.0f, 0.0f,
                         rotaryStartAngle, angle, true);
    graphics.setColour (colours::accentBright.withAlpha (
        0.38f + 0.32f * visualWeight));
    graphics.strokePath (value, juce::PathStrokeType (1.0f + 0.45f * visualWeight));

    // Vintage moulded-plastic skirt: a recessed bushing, soft contact shadow,
    // shallow radial flutes and a gently domed aged-ivory cap.
    const auto skirtRadius = radius * 0.72f;
    graphics.setColour (juce::Colours::black.withAlpha (0.42f));
    graphics.fillEllipse (centre.x - skirtRadius + 1.8f,
                          centre.y - skirtRadius + 3.0f,
                          skirtRadius * 2.0f, skirtRadius * 2.0f);

    const auto bezelRadius = skirtRadius + 2.0f;
    juce::ColourGradient bezelGradient (colours::nickel.brighter (0.18f),
                                        centre.x - bezelRadius,
                                        centre.y - bezelRadius,
                                        colours::panelOutline.darker (0.68f),
                                        centre.x + bezelRadius,
                                        centre.y + bezelRadius, false);
    graphics.setGradientFill (bezelGradient);
    graphics.fillEllipse (centre.x - bezelRadius, centre.y - bezelRadius,
                          bezelRadius * 2.0f, bezelRadius * 2.0f);

    juce::ColourGradient skirtGradient (colours::warmBone.brighter (0.08f),
                                        centre.x - skirtRadius * 0.65f,
                                        centre.y - skirtRadius * 0.75f,
                                        colours::warmBone.darker (0.32f),
                                        centre.x + skirtRadius * 0.65f,
                                        centre.y + skirtRadius * 0.82f, false);
    graphics.setGradientFill (skirtGradient);
    graphics.fillEllipse (centre.x - skirtRadius, centre.y - skirtRadius,
                          skirtRadius * 2.0f, skirtRadius * 2.0f);

    for (int flute = 0; flute < 24; ++flute)
    {
        const auto fluteAngle = juce::MathConstants<float>::twoPi
                              * static_cast<float> (flute) / 24.0f;
        const auto inner = centre.getPointOnCircumference (skirtRadius * 0.78f,
                                                           fluteAngle);
        const auto outer = centre.getPointOnCircumference (skirtRadius * 0.96f,
                                                           fluteAngle);
        graphics.setColour (colours::bakeliteEdge.withAlpha (0.18f));
        graphics.drawLine ({ inner, outer }, 0.7f);
    }

    const auto capRadius = skirtRadius * 0.72f;
    juce::ColourGradient capGradient (colours::binding.brighter (0.02f),
                                      centre.x - capRadius * 0.65f,
                                      centre.y - capRadius * 0.72f,
                                      colours::warmBone.darker (0.12f),
                                      centre.x + capRadius * 0.55f,
                                      centre.y + capRadius * 0.72f, false);
    graphics.setGradientFill (capGradient);
    graphics.fillEllipse (centre.x - capRadius, centre.y - capRadius,
                          capRadius * 2.0f, capRadius * 2.0f);
    graphics.setColour (juce::Colours::white.withAlpha (0.26f));
    graphics.drawEllipse (centre.x - capRadius, centre.y - capRadius,
                          capRadius * 2.0f, capRadius * 2.0f, 0.85f);

    // Dark inlaid pointer, aligned exactly to the parameter angle.
    const auto pointerLength = capRadius * 0.76f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.15f, -pointerLength,
                                 2.3f, pointerLength * 0.72f, 1.0f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    graphics.setColour (colours::accentDark.darker (0.32f));
    graphics.fillPath (pointer);
}

void ElectryLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                               juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    auto fill = on ? colours::oxblood : backgroundColour;
    if (isDown)
        fill = fill.brighter (0.25f);
    else if (isHighlighted)
        fill = fill.brighter (0.12f);

    graphics.setColour (juce::Colours::black.withAlpha (0.38f));
    graphics.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 5.0f);

    juce::ColourGradient buttonGradient (fill.brighter (isHighlighted ? 0.12f : 0.06f),
                                         bounds.getCentreX(), bounds.getY(),
                                         fill.darker (0.22f), bounds.getCentreX(),
                                         bounds.getBottom(), false);
    graphics.setGradientFill (buttonGradient);
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (on ? colours::accentBright.withAlpha (0.9f)
                           : colours::panelOutline.withAlpha (0.48f));
    graphics.drawRoundedRectangle (bounds, 5.0f, on ? 1.25f : 0.75f);
    graphics.setColour (juce::Colours::white.withAlpha (on ? 0.07f : 0.035f));
    graphics.drawLine (bounds.getX() + 5.0f, bounds.getY() + 2.0f,
                       bounds.getRight() - 5.0f, bounds.getY() + 2.0f, 0.8f);
}

void ElectryLookAndFeel::drawButtonText (juce::Graphics& graphics, juce::TextButton& button,
                                         bool, bool)
{
    graphics.setFont (getTextButtonFont (button, button.getHeight()));
    graphics.setColour (button.getToggleState() ? colours::binding
                                                : colours::text.withAlpha (0.82f));
    graphics.drawFittedText (button.getButtonText(),
                             button.getLocalBounds().reduced (2, 1),
                             juce::Justification::centred, 1);
}

void ElectryLookAndFeel::drawLabel (juce::Graphics& graphics, juce::Label& label)
{
    graphics.setColour (label.findColour (juce::Label::textColourId));
    graphics.setFont (label.getFont());
    graphics.drawFittedText (label.getText(), label.getLocalBounds(),
                             label.getJustificationType(), 2);
}

void ElectryLookAndFeel::drawComboBox (juce::Graphics& graphics, int width,
                                        int height, bool isButtonDown,
                                        int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                           static_cast<float> (width) - 1.0f,
                                           static_cast<float> (height) - 1.0f);
    auto fill = colours::knobFace.brighter (isButtonDown ? 0.18f : 0.04f);
    juce::ColourGradient gradient (fill.brighter (0.08f), bounds.getCentreX(),
                                   bounds.getY(), fill.darker (0.28f),
                                   bounds.getCentreX(), bounds.getBottom(), false);
    graphics.setGradientFill (gradient);
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (colours::panelOutline.withAlpha (0.58f));
    graphics.drawRoundedRectangle (bounds, 5.0f, 0.9f);

    const auto arrowX = bounds.getRight() - 18.0f;
    const auto arrowY = bounds.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - 4.0f, arrowY - 2.0f);
    arrow.lineTo (arrowX, arrowY + 2.0f);
    arrow.lineTo (arrowX + 4.0f, arrowY - 2.0f);
    graphics.setColour (colours::accentBright.withAlpha (0.9f));
    graphics.strokePath (arrow, juce::PathStrokeType (1.5f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
}

void ElectryLookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                juce::Label& label)
{
    label.setBounds (10, 1, juce::jmax (0, box.getWidth() - 40),
                     juce::jmax (0, box.getHeight() - 2));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font ElectryLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
}

juce::Label* ElectryLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (11.5f));
    label->setColour (juce::Label::textColourId, colours::binding.withAlpha (0.88f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

juce::Font ElectryLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (
        juce::jmin (13.0f, static_cast<float> (buttonHeight) * 0.6f),
        juce::Font::bold));
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

ElectryKeyboardComponent::ElectryKeyboardComponent (juce::MidiKeyboardState& state)
    : MidiKeyboardComponent (state, horizontalKeyboard)
{
}

void ElectryKeyboardComponent::setSelectedKeyswitches (int pickIndex,
                                                       int styleIndex)
{
    const auto pickCount = electry::ElectryEngine::pickStyleKeyswitchCount;
    const auto clampedPick = juce::jlimit (0, pickCount - 1, pickIndex);
    const auto clampedStyle = juce::jlimit (
        0, electry::ElectryEngine::playStyleKeyswitchCount - 1, styleIndex);
    if (selectedPickIndex == clampedPick && selectedStyleIndex == clampedStyle)
        return;

    selectedPickIndex = clampedPick;
    selectedStyleIndex = clampedStyle;
    repaint();
}

bool ElectryKeyboardComponent::isKeyswitchSelected (int keyswitchIndex) const noexcept
{
    const auto pickCount = electry::ElectryEngine::pickStyleKeyswitchCount;
    return keyswitchIndex < pickCount
        ? keyswitchIndex == selectedPickIndex
        : keyswitchIndex - pickCount == selectedStyleIndex;
}

juce::String ElectryKeyboardComponent::getWhiteNoteText (int midiNoteNumber)
{
    if (isKeyswitch (midiNoteNumber) || isDeadZoneNote (midiNoteNumber))
        return {};

    if (midiNoteNumber == firstPlayableNote || midiNoteNumber % 12 == 0)
        return juce::MidiMessage::getMidiNoteName (
            midiNoteNumber, true, true, getOctaveForMiddleC());

    return {};
}

void ElectryKeyboardComponent::drawWhiteNote (
    int midiNoteNumber, juce::Graphics& graphics, juce::Rectangle<float> area,
    bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    const auto keyswitch = isKeyswitch (midiNoteNumber);
    const auto tremoloGesture = isTremoloGesture (midiNoteNumber);
    const auto top = keyswitch ? colours::oxblood.brighter (0.22f)
                   : tremoloGesture ? colours::accentDark.brighter (0.22f)
                               : colours::warmBone.brighter (0.08f);
    const auto bottom = keyswitch ? colours::oxblood.darker (0.32f)
                      : tremoloGesture ? colours::accentDark.darker (0.28f)
                                  : colours::warmBone.darker (0.12f);
    graphics.setGradientFill ({ top, area.getCentreX(), area.getY(),
                                bottom, area.getCentreX(), area.getBottom(), false });
    graphics.fillRect (area);

    MidiKeyboardComponent::drawWhiteNote (
        midiNoteNumber, graphics, area, isDown, isOver, lineColour, textColour);

    graphics.setColour (juce::Colours::white.withAlpha (0.12f));
    graphics.fillRect (area.withHeight (1.0f));
    graphics.setColour (juce::Colours::black.withAlpha (0.11f));
    graphics.fillRect (area.withTop (area.getBottom() - 2.0f));

    if (keyswitch)
    {
        const auto index = midiNoteNumber - firstKeyboardNote;
        drawKeyswitchDecoration (graphics, area, index,
                                 isKeyswitchSelected (index), false);
    }
    else if (tremoloGesture)
    {
        drawTremoloDecoration (graphics, area, isDown);
    }
    else if (isDeadZoneNote (midiNoteNumber))
    {
        graphics.setColour (juce::Colours::black.withAlpha (0.55f));
        graphics.fillRect (area);
    }
    else if (midiNoteNumber == firstPlayableNote)
    {
        // A brass nut-like divider marks the start of the playable strings.
        graphics.setColour (colours::accentBright.withAlpha (0.9f));
        graphics.fillRect (area.withWidth (2.0f));
    }

    if (isDown && ! isDeadZoneNote (midiNoteNumber))
    {
        graphics.setColour (colours::accentBright.withAlpha (0.92f));
        graphics.drawRoundedRectangle (area.reduced (1.0f), 1.5f, 1.5f);
    }
}

void ElectryKeyboardComponent::drawBlackNote (
    int midiNoteNumber, juce::Graphics& graphics, juce::Rectangle<float> area,
    bool isDown, bool isOver, juce::Colour noteFillColour)
{
    const auto keyswitch = isKeyswitch (midiNoteNumber);
    const auto vibratoGesture = isVibratoGesture (midiNoteNumber);
    const auto fill = keyswitch ? colours::keyswitchBlack
                    : vibratoGesture ? colours::accentDark : noteFillColour;

    graphics.setColour (juce::Colours::black.withAlpha (0.48f));
    graphics.fillRoundedRectangle (area.translated (0.0f, 1.0f), 2.0f);
    graphics.setGradientFill ({ fill.brighter (0.15f), area.getCentreX(), area.getY(),
                                fill.darker (0.28f), area.getCentreX(),
                                area.getBottom(), false });
    graphics.fillRoundedRectangle (area, 2.0f);
    if (isOver)
    {
        graphics.setColour (findColour (
            juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId));
        graphics.fillRoundedRectangle (area, 2.0f);
    }
    if (isDown)
    {
        graphics.setColour (findColour (
            juce::MidiKeyboardComponent::keyDownOverlayColourId));
        graphics.fillRoundedRectangle (area, 2.0f);
    }
    graphics.setColour (juce::Colours::white.withAlpha (0.10f));
    graphics.drawLine (area.getX() + 1.0f, area.getY() + 1.0f,
                       area.getRight() - 1.0f, area.getY() + 1.0f, 0.8f);

    if (keyswitch)
    {
        const auto index = midiNoteNumber - firstKeyboardNote;
        drawKeyswitchDecoration (graphics, area, index,
                                 isKeyswitchSelected (index), true);
    }
    else if (vibratoGesture)
    {
        drawVibratoDecoration (graphics, area, isDown);
    }
    else if (isDeadZoneNote (midiNoteNumber))
    {
        graphics.setColour (juce::Colours::black.withAlpha (0.55f));
        graphics.fillRect (area);
    }

    if (isDown && ! isDeadZoneNote (midiNoteNumber))
    {
        graphics.setColour (colours::accentBright.withAlpha (0.92f));
        graphics.drawRoundedRectangle (area.reduced (0.8f), 1.8f, 1.4f);
    }
}

// ---------------------------------------------------------------------------
// Choice strip
// ---------------------------------------------------------------------------

bool ElectryTextButton::keyPressed (const juce::KeyPress& key)
{
    if (isEnabled() && onNavigation != nullptr
        && (key.isKeyCode (juce::KeyPress::leftKey)
            || key.isKeyCode (juce::KeyPress::rightKey)
            || key.isKeyCode (juce::KeyPress::upKey)
            || key.isKeyCode (juce::KeyPress::downKey)))
        return onNavigation (key);

    return juce::TextButton::keyPressed (
        key.isKeyCode (juce::KeyPress::spaceKey)
            ? juce::KeyPress { juce::KeyPress::returnKey }
            : key);
}

ElectryChoiceStrip::ElectryChoiceStrip (juce::String title,
                                        juce::StringArray choices,
                                        int maximumColumns,
                                        juce::String accessibilityTitle)
    : titleText (std::move (title)),
      maxColumns (juce::jmax (1, maximumColumns))
{
    const auto choiceContext = accessibilityTitle.isNotEmpty()
        ? std::move (accessibilityTitle)
        : titleText;
    setTitle (choiceContext);

    for (int index = 0; index < choices.size(); ++index)
    {
        auto button = std::make_unique<ElectryTextButton> (choices[index]);
        button->setTitle (choiceContext + ": " + choices[index]);
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1, juce::dontSendNotification);
        button->setHasFocusOutline (true);
        button->onClick = [this, index]
        {
            activateChoice (index);
        };
        button->onNavigation = [this, index] (const juce::KeyPress& key)
        {
            const auto count = static_cast<int> (buttons.size());
            if (count < 2)
                return false;

            const int direction = key.isKeyCode (juce::KeyPress::leftKey)
                               || key.isKeyCode (juce::KeyPress::upKey)
                ? -1 : 1;
            int next = index;
            for (int attempt = 1; attempt < count; ++attempt)
            {
                next = (next + direction + count) % count;
                if (buttons[static_cast<std::size_t> (next)]->isEnabled())
                {
                    activateChoice (next);
                    return true;
                }
            }
            return false;
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }
    if (! buttons.empty())
        setSelectedIndex (0);
}

void ElectryChoiceStrip::activateChoice (int index)
{
    if (index < 0 || index >= static_cast<int> (buttons.size()))
        return;

    auto& target = *buttons[static_cast<std::size_t> (index)];
    if (! target.isEnabled())
        return;

    setSelectedIndex (index);
    if (target.isShowing())
        target.grabKeyboardFocus();
    if (onChoice != nullptr)
        onChoice (index);
}

void ElectryChoiceStrip::setSelectedIndex (int newIndex)
{
    selectedIndex = juce::jlimit (0, static_cast<int> (buttons.size()) - 1, newIndex);
    for (int index = 0; index < static_cast<int> (buttons.size()); ++index)
    {
        auto& button = *buttons[static_cast<std::size_t> (index)];
        const bool selected = index == selectedIndex;
        button.setToggleState (selected, juce::dontSendNotification);
        button.setWantsKeyboardFocus (selected);
    }
}

void ElectryChoiceStrip::setTooltipText (const juce::String& text)
{
    for (auto& button : buttons)
        button->setTooltip (text);
}

void ElectryChoiceStrip::paint (juce::Graphics& graphics)
{
    if (titleText.isEmpty())
        return;

    graphics.setColour (colours::binding.withAlpha (0.92f));
    graphics.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold))
                          .withExtraKerningFactor (0.035f));
    graphics.drawFittedText (titleText, getLocalBounds().removeFromTop (17),
                             juce::Justification::centred, 1, 0.8f);
}

void ElectryChoiceStrip::resized()
{
    auto area = getLocalBounds();
    if (titleText.isNotEmpty())
        area.removeFromTop (19);
    if (buttons.empty())
        return;

    const int gap = 4;
    const int buttonCount = static_cast<int> (buttons.size());
    const int rowCount = (buttonCount + maxColumns - 1) / maxColumns;
    const int buttonHeight = (area.getHeight() - gap * (rowCount - 1)) / rowCount;

    for (int index = 0; index < buttonCount; ++index)
    {
        const int row = index / maxColumns;
        const int firstInRow = row * maxColumns;
        const int columnsInRow = juce::jmin (maxColumns, buttonCount - firstInRow);
        const int buttonWidth = (area.getWidth() - gap * (columnsInRow - 1))
                              / columnsInRow;
        const int column = index - firstInRow;
        buttons[static_cast<std::size_t> (index)]->setBounds (
            area.getX() + column * (buttonWidth + gap),
            area.getY() + row * (buttonHeight + gap),
            buttonWidth, buttonHeight);
    }
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------

ElectryKnob::ElectryKnob (juce::String name)
{
    setName (name + " control");
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    slider.setName (name);
    slider.setTitle (name);
    slider.setWantsKeyboardFocus (true);
    slider.setHasFocusOutline (true);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (label);
}

void ElectryKnob::resized()
{
    const auto visualWeight = static_cast<float> (
        slider.getProperties().getWithDefault (visualWeightProperty, 0.82f));
    const bool compact = visualWeight < compactKnobWeight;
    const bool hero = visualWeight >= 0.9f;

    auto area = getLocalBounds();
    constexpr int labelHeight = 19;
    label.setFont (juce::FontOptions (hero ? 12.0f : (compact ? 10.6f : 11.0f),
                                     juce::Font::bold));
    label.setColour (juce::Label::textColourId,
                     hero ? colours::binding : colours::dimText);
    label.setBounds (area.removeFromTop (labelHeight).reduced (2, 0));
    area.removeFromTop (3);
    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow, false,
        juce::jlimit (48, hero ? 88 : 78, juce::jmax (48, getWidth() - 4)),
        18);
    slider.setBounds (area);
}

// ---------------------------------------------------------------------------
// Status display
// ---------------------------------------------------------------------------

void ElectryStatusDisplay::setStatus (int activeVoices, int sympatheticStrings,
                                      bool ready, double sampleRate,
                                      int midiMutePressure, int vibratoGesture,
                                      int tremoloGesture,
                                      bool scheduleRepaint)
{
    midiMutePressure = juce::jlimit (0, 127, midiMutePressure);
    vibratoGesture = juce::jlimit (0, 127, vibratoGesture);
    tremoloGesture = juce::jlimit (0, 127, tremoloGesture);
    if (voices == activeVoices && sympathetic == sympatheticStrings
        && isReady == ready && juce::approximatelyEqual (rate, sampleRate)
        && mutePressure == midiMutePressure && vibrato == vibratoGesture
        && tremolo == tremoloGesture)
        return;
    voices = activeVoices;
    sympathetic = sympatheticStrings;
    isReady = ready;
    rate = sampleRate;
    mutePressure = midiMutePressure;
    vibrato = vibratoGesture;
    tremolo = tremoloGesture;
    const auto status = getStatusText();
    if (status != getTitle())
    {
        setTitle (status);
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
    }
    if (scheduleRepaint)
        repaint();
}

juce::String ElectryStatusDisplay::getStatusText() const
{
    if (! isReady)
        return "ENGINE STANDBY";

    juce::String status = juce::String (voices)
                        + (voices == 1 ? " STRING" : " STRINGS");
    if (sympathetic > 0)
        status += " +" + juce::String (sympathetic) + " RING";
    if (vibrato > 0)
        status += "  |  VIB " + juce::String (juce::roundToInt (
            100.0f * static_cast<float> (vibrato) / 127.0f)) + "%";
    if (tremolo > 0)
        status += "  |  TRM " + juce::String (juce::roundToInt (
            100.0f * static_cast<float> (tremolo) / 127.0f)) + "%";
    if (vibrato == 0 && tremolo == 0)
    {
        if (mutePressure > 0)
            status += "  |  CC2 MUTE +" + juce::String (juce::roundToInt (
                100.0f * static_cast<float> (mutePressure) / 127.0f)) + "%";
        else if (rate > 0.0)
            status += "  |  " + juce::String (rate / 1000.0, 1) + " kHz";
    }
    return status;
}

std::unique_ptr<juce::AccessibilityHandler>
ElectryStatusDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText);
}

void ElectryStatusDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (juce::Colours::black.withAlpha (0.62f));
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (colours::panelOutline.withAlpha (0.52f));
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    graphics.setColour (isReady ? colours::accentBright : colours::dimText);
    graphics.fillEllipse (8.0f, bounds.getCentreY() - 1.8f, 3.6f, 3.6f);
    graphics.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    graphics.drawFittedText (getStatusText(),
                             getLocalBounds().withTrimmedLeft (18).reduced (0, 1),
                             juce::Justification::centredLeft, 1, 0.72f);
}

// ---------------------------------------------------------------------------
// Fretboard display
// ---------------------------------------------------------------------------

ElectryFretboardDisplay::ElectryFretboardDisplay()
{
    setInterceptsMouseClicks (true, false);
    setName ("Fretboard display");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setHasFocusOutline (true);
    setHelpText ("Use Up and Down or number keys 1 through 8 to select a "
                 "physical string, then press Space or Return to repick it.");
    setTooltip ("Click any held string row, or use Up/Down or 1-8 then "
                "Space/Return, for one hard repick. Host MIDI "
                "E6 through B6 provides the same eight-string trigger lane "
                "with velocity control.");
    selectString (0);
}

void ElectryFretboardDisplay::selectString (int stringIndex)
{
    const int next = juce::jlimit (
        0, electry::ElectryEngine::stringCount - 1, stringIndex);
    if (next == selectedString)
        return;

    selectedString = next;
    updateAccessibilityTitle();
    repaint();
}

void ElectryFretboardDisplay::updateAccessibilityTitle()
{
    if (selectedString < 0
        || selectedString >= electry::ElectryEngine::stringCount)
        return;

    const auto& state = rows[static_cast<std::size_t> (selectedString)].state;
    juce::String title = juce::String ("Live fretboard: physical string ")
                       + juce::String (
                           electry::ElectryEngine::stringCount - selectedString)
                       + " selected, ";
    if (state.sounding)
    {
        if (state.fret == 0)
            title += "open ";
        else if (state.fret > 0)
            title += "fret " + juce::String (state.fret) + " ";
        if (state.midiNote >= 0)
            title += juce::MidiMessage::getMidiNoteName (
                state.midiNote, true, true, 4) + ", ";
        title += (state.strokeUp ? "upstroke, " : "downstroke, ");
        // `sounding` also covers a MIDI-held string whose delayed attack has
        // not begun or whose voice has already retired. "Held" is the exact
        // invariant and does not promise audible output in either case.
        title += (state.releasing ? "releasing" : "held");
    }
    else if (state.sympathetic)
    {
        if (state.midiNote >= 0)
            title += "open " + juce::MidiMessage::getMidiNoteName (
                state.midiNote, true, true, 4) + ", ";
        title += "sympathetic ring";
    }
    else
    {
        title += "silent";
    }
    title += ".";
    setTitle (title);
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
}

int ElectryFretboardDisplay::stringAtY (float y) const noexcept
{
    const auto height = static_cast<float> (getHeight());
    if (height <= 0.0f || y < 0.0f || y >= height)
        return -1;

    constexpr float inset = 0.085f;
    const auto first = electry::visuals::stringRowFraction (
        0, electry::ElectryEngine::stringCount, inset);
    const auto second = electry::visuals::stringRowFraction (
        1, electry::ElectryEngine::stringCount, inset);
    const auto spacing = second - first;
    const auto fraction = y / height;
    const int stringIndex = juce::roundToInt ((fraction - first) / spacing);
    if (stringIndex < 0
        || stringIndex >= electry::ElectryEngine::stringCount)
        return -1;

    const auto row = electry::visuals::stringRowFraction (
        stringIndex, electry::ElectryEngine::stringCount, inset);
    return std::abs (fraction - row) <= spacing * 0.48f ? stringIndex : -1;
}

void ElectryFretboardDisplay::mouseMove (const juce::MouseEvent& event)
{
    const int next = stringAtY (event.position.y);
    if (next == hoveredString)
        return;

    hoveredString = next;
    setMouseCursor (next >= 0 ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor);
    repaint();
}

void ElectryFretboardDisplay::mouseExit (const juce::MouseEvent&)
{
    if (hoveredString < 0)
        return;

    hoveredString = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void ElectryFretboardDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isLeftButtonDown())
        return;

    const int stringIndex = stringAtY (event.position.y);
    if (stringIndex >= 0)
    {
        selectString (stringIndex);
        if (onRepick)
            onRepick (stringIndex);
    }
}

bool ElectryFretboardDisplay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey)
        selectString (selectedString - 1);
    else if (key == juce::KeyPress::downKey)
        selectString (selectedString + 1);
    else if (key.getKeyCode() >= '1' && key.getKeyCode() <= '8')
        selectString (electry::ElectryEngine::stringCount
                      - (key.getKeyCode() - '0'));
    else if (key == juce::KeyPress::spaceKey
             || key == juce::KeyPress::returnKey)
    {
        if (onRepick)
            onRepick (selectedString);
        return true;
    }
    else
    {
        return false;
    }

    return true;
}

std::unique_ptr<juce::AccessibilityHandler>
ElectryFretboardDisplay::createAccessibilityHandler()
{
    auto actions = juce::AccessibilityActions().addAction (
        juce::AccessibilityActionType::press,
        [this]
        {
            if (onRepick)
                onRepick (selectedString);
        });
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::button, std::move (actions));
}

bool ElectryFretboardDisplay::refresh (const ElectryAudioProcessor& processor,
                                       float frameSeconds)
{
    bool moving = false;
    bool selectedStateChanged = false;
    for (int stringIndex = 0;
         stringIndex < electry::ElectryEngine::stringCount; ++stringIndex)
    {
        auto& row = rows[static_cast<std::size_t> (stringIndex)];
        const auto next = processor.getStringVisualState (stringIndex);
        const bool changed = next.midiNote != row.state.midiNote
                          || next.fret != row.state.fret
                          || next.sounding != row.state.sounding
                          || next.sympathetic != row.state.sympathetic
                          || next.releasing != row.state.releasing
                          || next.strokeUp != row.state.strokeUp;
        selectedStateChanged = selectedStateChanged
                            || (stringIndex == selectedString && changed);
        row.state = next;

        const float previousLevel = row.level;
        row.level = electry::visuals::meterBallistics (row.level, next.level,
                                                       0.55f, 0.18f);
        if (row.level > 0.004f)
        {
            // A visible wobble rate per string. It is deliberately not the
            // audio pitch, which would alias into nonsense at any frame rate a
            // GUI can sustain; it only has to read as "this string is moving".
            row.phase += juce::MathConstants<float>::twoPi
                       * (4.5f + 0.8f * static_cast<float> (stringIndex))
                       * frameSeconds;
            while (row.phase > juce::MathConstants<float>::twoPi)
                row.phase -= juce::MathConstants<float>::twoPi;
            moving = true;
        }
        else if (previousLevel > 0.0f)
        {
            row.level = 0.0f;
            row.phase = 0.0f;
            moving = true;
        }

        moving = moving || changed;
    }
    if (selectedStateChanged)
        updateAccessibilityTitle();
    return moving;
}

void ElectryFretboardDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 120.0f || bounds.getHeight() < 40.0f)
        return;

    // Keep the physical string number beside its tuning. The panel and
    // keyboard instructions both expose 1-8 repicks, so showing only E1..E4
    // here made that mapping needlessly implicit.
    auto tuningArea = bounds.removeFromLeft (42.0f);
    auto meterArea = bounds.removeFromRight (54.0f);
    bounds.removeFromLeft (4.0f);
    meterArea.removeFromLeft (8.0f);
    const auto neck = bounds;

    // Fingerboard blank.
    graphics.setGradientFill ({ colours::rosewood, neck.getCentreX(), neck.getY(),
                                colours::rosewoodDark, neck.getCentreX(),
                                neck.getBottom(), false });
    graphics.fillRoundedRectangle (neck, 3.0f);
    graphics.setColour (colours::panelOutline.withAlpha (0.55f));
    graphics.drawRoundedRectangle (neck.reduced (0.5f), 3.0f, 0.8f);

    const auto firstRow = electry::visuals::stringRowFraction (
        0, electry::ElectryEngine::stringCount, 0.085f);
    const auto secondRow = electry::visuals::stringRowFraction (
        1, electry::ElectryEngine::stringCount, 0.085f);
    const auto rowSpacing = neck.getHeight() * (secondRow - firstRow);
    const auto rowBounds = [&] (int stringIndex)
    {
        const auto rowY = neck.getY() + neck.getHeight()
            * electry::visuals::stringRowFraction (
                  stringIndex, electry::ElectryEngine::stringCount, 0.085f);
        return juce::Rectangle<float> (0.0f, rowY - rowSpacing * 0.46f,
                                       static_cast<float> (getWidth()),
                                       rowSpacing * 0.92f);
    };

    if (selectedString >= 0)
    {
        const auto selectedBounds = rowBounds (selectedString);
        const bool focused = hasKeyboardFocus (true);
        graphics.setColour (colours::accentBright.withAlpha (
            focused ? 0.14f : 0.06f));
        graphics.fillRoundedRectangle (selectedBounds, 2.0f);
        graphics.setColour (colours::accentBright.withAlpha (
            focused ? 0.78f : 0.34f));
        graphics.drawRoundedRectangle (selectedBounds.reduced (0.5f), 2.0f,
                                       focused ? 1.2f : 0.7f);
    }

    if (hoveredString >= 0 && hoveredString != selectedString)
    {
        graphics.setColour (colours::accentBright.withAlpha (0.10f));
        graphics.fillRoundedRectangle (rowBounds (hoveredString), 2.0f);
    }

    const auto neckX = neck.getX();
    const auto neckWidth = neck.getWidth();
    // Solved once per paint() and shared by every wire, inlay and sounding
    // string below instead of letting fretWireFraction()/fretCentreFraction()
    // each recompute it from lastDrawnFret with their own std::exp2 call.
    const auto neckSpan = electry::visuals::fretSpan (lastDrawnFret);
    const auto fretX = [neckX, neckWidth, neckSpan] (int fret)
    {
        return neckX + neckWidth
             * electry::visuals::fretWireFraction (fret, lastDrawnFret, neckSpan);
    };

    // Position inlays sit behind the strings.
    const auto drawInlay = [&graphics, &fretX, &neck] (int fret, bool doubled)
    {
        const auto centre = 0.5f * (fretX (fret - 1) + fretX (fret));
        const auto radius = juce::jmin (4.0f, neck.getHeight() * 0.05f);
        graphics.setColour (colours::warmBone.withAlpha (0.30f));
        if (doubled)
        {
            const auto offset = neck.getHeight() * 0.24f;
            graphics.fillEllipse (centre - radius, neck.getCentreY() - offset - radius,
                                  radius * 2.0f, radius * 2.0f);
            graphics.fillEllipse (centre - radius, neck.getCentreY() + offset - radius,
                                  radius * 2.0f, radius * 2.0f);
        }
        else
        {
            graphics.fillEllipse (centre - radius, neck.getCentreY() - radius,
                                  radius * 2.0f, radius * 2.0f);
        }
    };
    for (const int fret : electry::visuals::inlayFrets)
        drawInlay (fret, false);
    drawInlay (electry::visuals::octaveInlayFret, true);
    drawInlay (electry::visuals::upperOctaveInlayFret, true);

    // Nut and fret wires.
    graphics.setColour (colours::warmBone.withAlpha (0.85f));
    graphics.fillRect (neck.getX(), neck.getY(), 3.0f, neck.getHeight());
    for (int fret = 1; fret <= lastDrawnFret; ++fret)
    {
        const auto x = fretX (fret);
        graphics.setColour (colours::fretWire.withAlpha (0.42f));
        graphics.drawLine (x, neck.getY() + 1.5f, x, neck.getBottom() - 1.5f, 1.1f);
    }

    // Strings, note markers, tuning labels and level meters.
    static constexpr std::array<const char*, electry::ElectryEngine::stringCount>
        tuningNames { "E1", "B1", "E2", "A2", "D3", "G3", "B3", "E4" };

    for (int stringIndex = 0;
         stringIndex < electry::ElectryEngine::stringCount; ++stringIndex)
    {
        const auto& row = rows[static_cast<std::size_t> (stringIndex)];
        const auto y = neck.getY() + neck.getHeight()
            * electry::visuals::stringRowFraction (
                  stringIndex, electry::ElectryEngine::stringCount, 0.085f);
        const auto thickness = electry::visuals::stringThickness (
            stringIndex, 0.9f, 2.6f);
        const auto heat = electry::visuals::levelHeat (row.level);
        const bool ringing = row.state.sounding || row.state.sympathetic;

        auto stringColour = colours::nickel.withAlpha (0.55f);
        if (row.state.sympathetic)
            stringColour = colours::sympatheticRing.withAlpha (0.45f + 0.45f * heat);
        else if (row.state.sounding)
            stringColour = (row.state.releasing ? colours::accent
                                                : colours::accentBright)
                               .withAlpha (0.55f + 0.45f * heat);

        const auto stoppedFraction = row.state.sounding && row.state.fret > 0
            ? electry::visuals::fretWireFraction (row.state.fret, lastDrawnFret,
                                                  neckSpan)
            : 0.0f;
        const auto swing = std::sin (row.phase) * heat
                         * juce::jmin (5.0f, neck.getHeight() * 0.055f);

        graphics.setColour (stringColour);
        if (ringing && std::abs (swing) > 0.05f)
        {
            // The vibrating portion is drawn as the fundamental standing wave
            // over the sounding length only, so the section behind the
            // fretting finger correctly stays still.
            juce::Path shape;
            constexpr int steps = 40;
            for (int step = 0; step <= steps; ++step)
            {
                const auto u = static_cast<float> (step) / static_cast<float> (steps);
                const auto x = neck.getX() + neckWidth * u;
                const auto offset = swing
                    * electry::visuals::vibrationShape (u, stoppedFraction);
                if (step == 0)
                    shape.startNewSubPath (x, y + offset);
                else
                    shape.lineTo (x, y + offset);
            }
            graphics.strokePath (shape, juce::PathStrokeType (thickness));
        }
        else
        {
            graphics.drawLine (neck.getX(), y, neck.getRight(), y, thickness);
        }

        // Fingered position.
        if (row.state.sounding && row.state.midiNote >= 0)
        {
            const auto markerX = row.state.fret > 0
                ? neck.getX() + neckWidth
                      * electry::visuals::fretCentreFraction (row.state.fret,
                                                              lastDrawnFret,
                                                              neckSpan)
                : neck.getX() + 1.5f;
            const auto radius = juce::jmin (6.5f, neck.getHeight() * 0.085f);
            graphics.setColour (juce::Colours::black.withAlpha (0.55f));
            graphics.fillEllipse (markerX - radius, y - radius,
                                  radius * 2.0f, radius * 2.0f);
            graphics.setColour ((row.state.releasing ? colours::accent
                                                     : colours::accentBright)
                                    .withAlpha (0.85f));
            graphics.drawEllipse (markerX - radius, y - radius,
                                  radius * 2.0f, radius * 2.0f, 1.4f);
            graphics.setColour (colours::text);
            graphics.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            graphics.drawText (
                juce::MidiMessage::getMidiNoteName (row.state.midiNote, true, false, 4),
                juce::Rectangle<float> (markerX - radius - 6.0f, y - radius,
                                        radius * 2.0f + 12.0f, radius * 2.0f),
                juce::Justification::centred);
        }

        // Physical string number and tuning label.
        auto labelBounds = juce::Rectangle<float> (
            tuningArea.getX(), y - 6.0f, tuningArea.getWidth(), 12.0f);
        auto stringNumberBounds = labelBounds.removeFromLeft (14.0f);
        // Keep the 8.8 px figures above 4.5:1 over both endpoints of the
        // panel gradient without competing with the selected-string accent.
        graphics.setColour (stringIndex == selectedString
                                ? colours::accentBright
                                : colours::dimText.withAlpha (0.72f));
        graphics.setFont (juce::FontOptions (8.8f, juce::Font::bold));
        graphics.drawText (juce::String (
                               electry::ElectryEngine::stringCount - stringIndex),
                           stringNumberBounds, juce::Justification::centred);

        graphics.setColour (ringing ? colours::binding
                                    : colours::dimText.withAlpha (0.72f));
        graphics.setFont (juce::FontOptions (10.2f, juce::Font::bold));
        graphics.drawText (tuningNames[static_cast<std::size_t> (stringIndex)],
                           labelBounds,
                           juce::Justification::centredRight);

        // Per-string level meter.
        const auto meterHeight = juce::jmax (2.0f, thickness + 1.0f);
        const juce::Rectangle<float> meterTrack (meterArea.getX(), y - meterHeight * 0.5f,
                                                 meterArea.getWidth(), meterHeight);
        graphics.setColour (juce::Colours::black.withAlpha (0.55f));
        graphics.fillRoundedRectangle (meterTrack, meterHeight * 0.5f);
        if (heat > 0.01f)
        {
            graphics.setColour (row.state.sympathetic ? colours::sympatheticRing
                                                      : colours::accentBright);
            graphics.fillRoundedRectangle (
                meterTrack.withWidth (juce::jmax (meterHeight,
                                                  meterTrack.getWidth() * heat)),
                meterHeight * 0.5f);
        }
    }
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

ElectryAudioProcessorEditor::ElectryAudioProcessorEditor (ElectryAudioProcessor& p)
    : AudioProcessorEditor (&p),
      electryProcessor (p),
      backgroundImage (juce::ImageFileFormat::loadFrom (
          BinaryData::electrymahoganysatinv2_png,
          BinaryData::electrymahoganysatinv2_pngSize)),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    logoLabel.setText ("ELECTRY", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold))
                           .withExtraKerningFactor (0.025f));
    logoLabel.setColour (juce::Label::textColourId, colours::binding);
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("PHYSICALLY MODELED DROP-E 8-STRING GUITAR",
                          juce::dontSendNotification);
    editionLabel.setFont (juce::Font (juce::FontOptions (11.5f))
                              .withExtraKerningFactor (0.045f));
    editionLabel.setColour (juce::Label::textColourId, colours::dimText);
    editionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (editionLabel);

    factoryProgramLabel.setText ("RIG", juce::dontSendNotification);
    factoryProgramLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    factoryProgramLabel.setColour (juce::Label::textColourId,
                                   colours::binding.withAlpha (0.8f));
    factoryProgramLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (factoryProgramLabel);

    for (int index = 0; index < electryProcessor.getNumPrograms(); ++index)
        factoryProgramSelector.addItem (
            electryProcessor.getProgramName (index), index + 1);
    factoryProgramSelector.setSelectedId (
        electryProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    factoryProgramSelector.setTooltip (
        "Rigs initialize guitar, FX, Mute Tightness and Mute Pressure. They "
        "never change the PICK STROKE or PLAY STYLE latches; choose Mute "
        "or Dead below.");
    factoryProgramSelector.setName ("Factory rig");
    factoryProgramSelector.setTitle ("Factory rig");
    factoryProgramSelector.setComponentID ("factoryProgram");
    factoryProgramSelector.setHasFocusOutline (true);
    factoryProgramSelector.onChange = [this]
    {
        const int index = factoryProgramSelector.getSelectedId() - 1;
        if (index >= 0)
            electryProcessor.setCurrentProgram (index);
    };
    addAndMakeVisible (factoryProgramSelector);

    keyboardHintLabel.setText (
        "C0..D0 pick stroke; D#0..A0 style; hold A#0 VIB (vibrato) or B0 TRM (tremolo pick). E1..D6 plays.",
        juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::FontOptions (11.0f));
    keyboardHintLabel.setColour (juce::Label::textColourId,
                                 colours::binding.withAlpha (0.78f));
    keyboardHintLabel.setJustificationType (juce::Justification::centredLeft);
    keyboardHintLabel.setComponentID ("keyboardHint");
    addAndMakeVisible (keyboardHintLabel);

    addAndMakeVisible (statusDisplay);

    panicButton.setComponentID ("panic");
    panicButton.setHasFocusOutline (true);
    panicButton.setColour (juce::TextButton::buttonColourId,
                           colours::oxblood.darker (0.12f));
    panicButton.setTooltip ("Immediately silence all strings");
    panicButton.onClick = [this] { electryProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    pickStyleStrip.onChoice = [this] (int index)
    {
        keyboard.setSelectedKeyswitches (
            index, electryProcessor.getEffectivePlayStyleIndex());
        electryProcessor.triggerArticulation (index);
    };
    pickStyleStrip.setTooltipText (
        "Picking hand: Down, Up or Alternate. This bank combines independently with every play style.");
    pickStyleStrip.setComponentID ("pickStyleStrip");
    addAndMakeVisible (pickStyleStrip);

    playStyleStrip.onChoice = [this] (int index)
    {
        keyboard.setSelectedKeyswitches (
            electryProcessor.getCurrentPickStyleIndex(),
            electryProcessor.getEffectivePlayStyleIndex());
        electryProcessor.triggerArticulation (
            electry::ElectryEngine::pickStyleKeyswitchCount + index);
    };
    playStyleStrip.setTooltipText (
        "Select the base style. Mute is the bridge hand; Dead is the fretting hand; the selected pick stroke still applies.");
    playStyleStrip.setComponentID ("playStyleStrip");
    addAndMakeVisible (playStyleStrip);

    playStyleKeyModeStrip.onChoice = [this] (int index)
    {
        electryProcessor.setPlayStyleKeysHold (index == 1);
    };
    playStyleKeyModeStrip.setTooltipText (
        "LATCH leaves MIDI play-style keys selected. HOLD uses them only while pressed, then returns to the PLAY STYLE choice.");
    playStyleKeyModeStrip.setComponentID ("playStyleKeyMode");
    addAndMakeVisible (playStyleKeyModeStrip);

    // The pickup selector strip binds to the choice parameter.
    pickupStrip.onChoice = [this] (int index)
    {
        if (auto* parameter = electryProcessor.parameters.getParameter (
                electry::parameters::pickupSelector))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (index)));
            parameter->endChangeGesture();
        }
    };
    if (auto* parameter = electryProcessor.parameters.getParameter (
            electry::parameters::pickupSelector))
    {
        pickupAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter,
            [this] (float newValue)
            {
                pickupStrip.setSelectedIndex (juce::roundToInt (newValue));
            },
            nullptr);
        pickupAttachment->sendInitialUpdate();
    }
    pickupStrip.setComponentID ("pickupSelector");
    addAndMakeVisible (pickupStrip);

    outputModeStrip.onChoice = [this] (int index)
    {
        auto* outputParameter = electryProcessor.parameters.getParameter (
            electry::parameters::outputMode);
        if (outputParameter == nullptr)
            return;

        outputParameter->beginChangeGesture();
        outputParameter->setValueNotifyingHost (
            outputParameter->convertTo0to1 (static_cast<float> (index)));
        outputParameter->endChangeGesture();
    };
    if (auto* parameter = electryProcessor.parameters.getParameter (
            electry::parameters::outputMode))
    {
        outputModeAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter,
            [this] (float newValue)
            {
                outputModeStrip.setSelectedIndex (juce::roundToInt (newValue));
            },
            nullptr);
        outputModeAttachment->sendInitialUpdate();
    }
    outputModeStrip.setTooltipText (
        "Mono is an authentic summed DI. Stereo spreads one guitar's eight "
        "strings through a phase-coherent divided-pickup field. 2X runs "
        "two independent Electry performances, one per channel. Choose 2X "
        "before the phrase; it does not clone notes already ringing.");
    outputModeStrip.setComponentID (electry::parameters::outputMode);
    addAndMakeVisible (outputModeStrip);

    ampModelStrip.onChoice = [this] (int index)
    {
        auto* parameter = electryProcessor.parameters.getParameter (
            electry::parameters::ampModel);
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (index)));
        parameter->endChangeGesture();
    };
    if (auto* parameter = electryProcessor.parameters.getParameter (
            electry::parameters::ampModel))
    {
        ampModelAttachment = std::make_unique<juce::ParameterAttachment> (
            *parameter,
            [this] (float newValue)
            {
                ampModelStrip.setSelectedIndex (juce::roundToInt (newValue));
            },
            nullptr);
        ampModelAttachment->sendInitialUpdate();
    }
    ampModelStrip.setTooltipText (
        "Selects the complete amplifier and cabinet voice: American clean, "
        "British crunch, or modern high-gain.");
    ampModelStrip.setComponentID (electry::parameters::ampModel);
    addAndMakeVisible (ampModelStrip);

    using namespace electry::parameters;
    const auto setup = [this] (ElectryKnob& knob, const char* parameterId,
                               const char* tooltip)
    {
        const juce::String help = juce::String (tooltip)
                                + " Double-click to reset to its default.";
        knob.slider.setTooltip (help);
        knob.slider.setHelpText (help);
        knob.setComponentID (parameterId);
        addAndMakeVisible (knob);
        attachSlider (knob.slider, parameterId);
    };

#if ELECTRY_MEASURED_BODY_RESPONSE
    setup (guitarBuildKnob, guitarBuild,
           "Uses three pickup-observed modes from one "
           "matched walnut/ash body pair. Only each measured material pole is "
           "morphed to its mate; modal levels are quiet voicing. Shape, joint "
           "and body size remain neutral until matched captures exist. Guitar "
           "Build visits six distinct short, balanced, light and heavy "
           "extended scale length and string-gauge setups while material moves "
           "from walnut "
           "toward ash. The setup path is voicing, not a material law. "
           "Pickups and playing controls remain independent.");
    setup (bodyResonanceKnob, bodyResonance,
           "Amount of quiet material-dependent structural pickup colour; zero "
           "is an exact bypass");
#else
    setup (guitarBuildKnob, guitarBuild,
           "Morphs material damping, body mass and modes, neck and bridge "
           "coupling, scale length, and Drop-E string gauge. Pickups and "
           "playing controls remain independent.");
    setup (bodyResonanceKnob, bodyResonance,
           "How much solid-body structural colour reaches the pickups");
#endif
    setup (pickupTypeKnob, pickupType,
           "Pickup construction: wide humbucker toward narrow single coil");
    setup (toneKnob, tone, "Passive tone control loading the pickup resonance");
    setup (stringAgeKnob, stringAge, "String condition: fresh toward old and dead");
    setup (pickPositionKnob, pickPosition,
           "Picking spot: close to the bridge toward over the neck");
    setup (pickHardnessKnob, pickHardness,
           "Plectrum stiffness and edge: soft and round toward hard and sharp");
    setup (bendTimeKnob, bendTime,
           "Travel time of a pitch-wheel bend: how long the strings take to "
           "reach the wheel rather than snapping to it");
    setup (muteDampingKnob, muteDamping,
           "Loose half-mute toward tight metal chug for the E0 Mute "
           "play style");
    setup (velocityKnob, velocity, "How strongly MIDI velocity drives the pluck");
    setup (pickNoiseKnob, pickNoise, "Plectrum contact and scrape level");
    setup (fingerNoiseKnob, fingerNoise, "Fretting-hand contact level");
    setup (releaseNoiseKnob, releaseNoise, "String damping noise at note end");
    setup (artifactsKnob, artifacts,
           "Amount of subtle deterministic hardware ring, fret buzz, and incidental collision");
    setup (sympatheticKnob, sympathetic,
           "Bridge-coupled sympathetic ring of the strings you are not fingering. "
           "At 0% the coupled waveguides are bypassed exactly.");
    setup (palmMuteKnob, palmMute,
           "Continuous bridge-hand pressure across every play style, including "
           "Dead; MIDI CC2 adds to it while you play.");
    setup (strumSpreadKnob, strumSpread,
           "Mean pick travel time per string crossed. At 0 ms a chord starts as "
           "one block; any higher value groups cross-string arrivals for up to "
           "35 ms from the first and adds a 20 ms assembly pre-roll.");
    setup (tremoloRateKnob, tremoloRate,
           "Free-running picking speed while B0 TRM is held (not transport "
           "synced). 8, 12 and 16 strokes/s match the capture protocol; "
           "12 strokes/s equals 180 BPM sixteenth notes.");
    setup (resonanceKnob, resonanceDepth,
           "Full-scale reach of the modulation-wheel (CC1) resonance: how far "
           "the wheel can raise the sympathetic coupling and how much of the "
           "amplified output may feed back into the strings. At 100% a "
           "distorted tone self-resonates with the wheel up.");
    setup (outputKnob, output, "Master output level");
    setup (distortionKnob, distortion,
           "Drive through the oversampled diode pedal; 0% is true bypass");
    setup (ampKnob, amp,
           "Drive through the tube, transformer and cabinet path; 0% is true bypass");
    setup (compressorKnob, compressor, "Fast levelling for tight rhythm playing");
    setup (delayKnob, delay, "Tempo-neutral 360 ms lead delay");
    setup (roomKnob, room, "Compact stereo room ambience");

    fretboardDisplay.setComponentID ("fretboard");
    fretboardDisplay.onRepick = [this] (int stringIndex)
    {
        electryProcessor.triggerStringRepick (stringIndex);
    };
    addAndMakeVisible (fretboardDisplay);

    keyboard.setAvailableRange (firstKeyboardNote, lastKeyboardNote);
    keyboard.setLowestVisibleKey (firstKeyboardNote);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyWidth (24.0f);
    keyboard.setBlackNoteLengthProportion (0.64f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setTitle (
        "MIDI keyboard: hold A#0 for vibrato or B0 for tremolo picking; E1 to D6 plays");
    keyboard.setComponentID ("keyboard");
    keyboard.setHasFocusOutline (true);
    addAndMakeVisible (keyboard);

    setSize (editorWidth, editorHeight);
    // The fretboard animates string motion, so the editor runs at a display
    // rate rather than the old status-only 12 Hz. Only the fretboard repaints,
    // and only while something is actually moving.
    startTimerHz (timerHz);

    // Populate the status readout and articulation strip immediately so the
    // panel opens in its real state instead of waiting up to a timer tick.
    timerCallback();
}

ElectryAudioProcessorEditor::~ElectryAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ElectryAudioProcessorEditor::attachSlider (juce::Slider& slider,
                                                const char* parameterId)
{
    // Double-click resets a knob to its parameter's own default rather than
    // to whatever JUCE's built-in fallback would pick, so it stays correct
    // for every knob without maintaining a second table of defaults here.
    if (auto* parameter = electryProcessor.parameters.getParameter (parameterId))
    {
        // Keep the compact panel label while exposing the complete canonical
        // parameter name to accessibility clients (for example, "Mute
        // pressure" rather than the visible "MUTE").
        slider.setTitle (parameter->getName (100));
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    }

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        electryProcessor.parameters, parameterId, slider));
}

void ElectryAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (electryProcessor.getActiveVoiceCount(),
                             electryProcessor.getSympatheticStringCount(),
                             electryProcessor.isEngineReady(),
                             electryProcessor.getCurrentSampleRateForDisplay(),
                             electryProcessor.getMidiMutePressureForDisplay(),
                             electryProcessor.getVibratoGestureForDisplay(),
                             electryProcessor.getTremoloGestureForDisplay());
    const int programId = electryProcessor.getCurrentProgram() + 1;
    if (factoryProgramSelector.getSelectedId() != programId)
        factoryProgramSelector.setSelectedId (programId, juce::dontSendNotification);
    const auto pickIndex = electryProcessor.getCurrentPickStyleIndex();
    const auto baseStyleIndex = electryProcessor.getCurrentPlayStyleIndex();
    const auto effectiveStyleIndex =
        electryProcessor.getEffectivePlayStyleIndex();
    pickStyleStrip.setSelectedIndex (pickIndex);
    playStyleStrip.setSelectedIndex (baseStyleIndex);
    playStyleKeyModeStrip.setSelectedIndex (
        electryProcessor.getPlayStyleKeysHold() ? 1 : 0);
    keyboard.setSelectedKeyswitches (pickIndex, effectiveStyleIndex);

    if (fretboardDisplay.refresh (electryProcessor, 1.0f / static_cast<float> (timerHz)))
        fretboardDisplay.repaint();
}

void ElectryAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (colours::background);
    if (backgroundImage.isValid())
    {
        graphics.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        graphics.drawImageWithin (backgroundImage, 0, 0, getWidth(), getHeight(),
                                  juce::RectanglePlacement::fillDestination);
    }

    // The image is only the material. Lighting and contrast stay in the
    // renderer so controls remain legible at every host scale.
    juce::ColourGradient shade (juce::Colour (0xff180d09).withAlpha (0.46f),
                                0.0f, 0.0f,
                                juce::Colours::black.withAlpha (0.64f),
                                0.0f, static_cast<float> (getHeight()), false);
    graphics.setGradientFill (shade);
    graphics.fillRect (getLocalBounds());

    auto footerArea = getLocalBounds().reduced (12).removeFromBottom (20).toFloat();
    graphics.setColour (juce::Colours::black.withAlpha (0.54f));
    graphics.fillRoundedRectangle (footerArea, 3.0f);

    graphics.setColour (colours::binding.withAlpha (0.18f));
    graphics.drawLine (12.0f, 56.0f, static_cast<float> (getWidth() - 12),
                       56.0f, 0.8f);

    const std::array<const char*, sectionCount> titles {
        "", "FRETBOARD  (CLICK OR UP/DOWN / 1-8, SPACE/RETURN REPICKS)",
        "PERFORMANCE",
        "CORE TONE & RESPONSE", "MASTER", "GUITAR BUILD", "PLAY DETAIL", "FX"
    };

    for (int section = 0; section < sectionCount; ++section)
    {
        const auto bounds = sectionBounds[static_cast<std::size_t> (section)];
        if (bounds.isEmpty())
            continue;
        const auto panelBounds = bounds.toFloat();
        graphics.setColour (juce::Colours::black.withAlpha (0.34f));
        graphics.fillRoundedRectangle (panelBounds.translated (0.0f, 2.0f), 7.0f);

        juce::ColourGradient panelGradient (
                                            colours::panelTop,
                                            panelBounds.getCentreX(), panelBounds.getY(),
                                            colours::panel,
                                            panelBounds.getCentreX(), panelBounds.getBottom(), false);
        graphics.setGradientFill (panelGradient);
        graphics.fillRoundedRectangle (panelBounds, 7.0f);
        graphics.setColour (colours::panelOutline.withAlpha (0.50f));
        graphics.drawRoundedRectangle (panelBounds.reduced (0.5f), 7.0f, 0.8f);
        graphics.setColour (colours::binding.withAlpha (0.07f));
        graphics.drawRoundedRectangle (panelBounds.reduced (1.8f), 5.5f, 0.65f);

        if (titles[static_cast<std::size_t> (section)][0] != '\0')
        {
            graphics.setColour (colours::binding.withAlpha (0.96f));
            graphics.setFont (
                juce::Font (juce::FontOptions (13.8f, juce::Font::bold))
                    .withExtraKerningFactor (0.035f));
            graphics.drawText (titles[static_cast<std::size_t> (section)],
                               bounds.withHeight (sectionTitleHeight).reduced (12, 0),
                               juce::Justification::centredLeft);
        }
    }
}

void ElectryAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header.
    auto header = area.removeFromTop (44);
    logoLabel.setBounds (header.removeFromLeft (158));
    editionLabel.setBounds (header.removeFromLeft (314));
    header.removeFromLeft (10);
    panicButton.setBounds (header.removeFromRight (74).reduced (0, 7));
    header.removeFromRight (10);
    statusDisplay.setBounds (
        header.removeFromRight (statusDisplayWidth).reduced (0, 7));
    header.removeFromRight (10);
    factoryProgramLabel.setBounds (header.removeFromLeft (32));
    header.removeFromLeft (6);
    factoryProgramSelector.setBounds (header.reduced (0, 7));

    area.removeFromTop (6);

    // Keyboard and hint at the bottom.
    keyboardHintLabel.setBounds (area.removeFromBottom (18).reduced (2, 0));
    keyboard.setBounds (area.removeFromBottom (100));
    keyboard.setKeyWidth (static_cast<float> (keyboard.getWidth())
                          / static_cast<float> (keyboardWhiteKeyCount));
    area.removeFromBottom (6);

    // The two keyswitch strips and their compact play-style operating mode.
    auto articulationArea = area.removeFromTop (76);
    sectionBounds[articulationSection] = articulationArea;
    auto stripRow = articulationArea.reduced (12, 8);
    const int pickWidth = juce::roundToInt (
        static_cast<float> (stripRow.getWidth()) * 0.24f);
    const int modeWidth = juce::roundToInt (
        static_cast<float> (stripRow.getWidth()) * 0.15f);
    pickStyleStrip.setBounds (stripRow.removeFromLeft (pickWidth));
    stripRow.removeFromLeft (12);
    playStyleKeyModeStrip.setBounds (stripRow.removeFromLeft (modeWidth));
    stripRow.removeFromLeft (12);
    playStyleStrip.setBounds (stripRow);
    area.removeFromTop (8);

    // The live fretboard sits directly under the play styles, beside the five
    // performance controls that change what it shows.
    {
        auto fretboardRow = area.removeFromTop (
            juce::jmin (fretboardPanelHeight, juce::jmax (0, area.getHeight() - 260)));
        area.removeFromTop (8);
        auto performanceArea = fretboardRow.removeFromRight (
            juce::jmin (360, fretboardRow.getWidth() / 2));
        fretboardRow.removeFromRight (8);
        sectionBounds[fretboardSection] = fretboardRow;
        sectionBounds[performanceSection] = performanceArea;

        fretboardDisplay.setBounds (
            fretboardRow.reduced (12, 10).withTrimmedTop (sectionContentTrim));
        layoutKnobRow (
            performanceArea.reduced (10).withTrimmedTop (sectionContentTrim),
            { { &sympatheticKnob, KnobTier::detail },
              { &palmMuteKnob, KnobTier::detail },
              { &strumSpreadKnob, KnobTier::detail },
              { &tremoloRateKnob, KnobTier::detail },
              { &resonanceKnob, KnobTier::detail } },
            4);
    }

    // The remaining 410 px is deliberately split by sonic importance rather
    // than by parameter type. Controls that reshape every note occupy the
    // large upper row; the single build macro and articulation-specific
    // texture controls remain visibly subordinate.
    const int mainHeight = juce::jlimit (
        180, area.getHeight() - 150,
        juce::roundToInt (static_cast<float> (area.getHeight()) * 0.532f));
    auto mainRow = area.removeFromTop (mainHeight);
    area.removeFromTop (8);
    auto secondaryRow = area;

    // Output mode stays comfortably operable beside the primary tone panel.
    auto masterArea = mainRow.removeFromRight (184);
    mainRow.removeFromRight (8);
    auto coreArea = mainRow;
    sectionBounds[coreSection] = coreArea;
    sectionBounds[masterSection] = masterArea;

    {
        auto inner = coreArea.reduced (16, 10)
                             .withTrimmedTop (sectionContentTrim);
        auto selectorArea = inner.removeFromLeft (juce::jmin (80, inner.getWidth()));
        pickupStrip.setBounds (selectorArea);
        inner.removeFromLeft (juce::jmin (6, inner.getWidth()));
        layoutKnobRow (
            inner,
            { { &pickupTypeKnob, KnobTier::hero },
              { &toneKnob, KnobTier::hero },
              { &pickPositionKnob, KnobTier::hero },
              { &pickHardnessKnob, KnobTier::hero },
              { &stringAgeKnob, KnobTier::hero },
              { &bodyResonanceKnob, KnobTier::hero },
              { &velocityKnob, KnobTier::hero } },
            10);
    }

    {
        auto masterInner = masterArea.reduced (12, 10)
                                     .withTrimmedTop (sectionContentTrim);
        outputModeStrip.setBounds (masterInner.removeFromTop (48));
        masterInner.removeFromTop (2);
        layoutKnobRow (
            masterInner,
            { { &outputKnob, KnobTier::master } }, 0);
    }

    const int secondaryContentWidth = juce::jmax (0, secondaryRow.getWidth() - 8);
    const int buildWidth = juce::roundToInt (
        static_cast<float> (secondaryContentWidth) * 0.18f);
    auto buildArea = secondaryRow.removeFromLeft (buildWidth);
    secondaryRow.removeFromLeft (juce::jmin (8, secondaryRow.getWidth()));
    const int detailWidth = juce::roundToInt (
        static_cast<float> (secondaryContentWidth) * 0.40f);
    auto detailArea = secondaryRow.removeFromLeft (detailWidth);
    secondaryRow.removeFromLeft (juce::jmin (8, secondaryRow.getWidth()));
    auto effectsArea = secondaryRow;
    sectionBounds[buildSection] = buildArea;
    sectionBounds[detailSection] = detailArea;
    sectionBounds[effectsSection] = effectsArea;

    layoutKnobRow (
        buildArea.reduced (12, 10).withTrimmedTop (sectionContentTrim),
        { { &guitarBuildKnob, KnobTier::hero } }, 0);

    layoutKnobRow (
        detailArea.reduced (12, 10).withTrimmedTop (sectionContentTrim),
        { { &muteDampingKnob, KnobTier::contextual },
          { &bendTimeKnob, KnobTier::detail },
          { &pickNoiseKnob, KnobTier::detail },
          { &fingerNoiseKnob, KnobTier::detail },
          { &releaseNoiseKnob, KnobTier::detail },
          { &artifactsKnob, KnobTier::detail } },
        4);

    auto effectsInner = effectsArea.reduced (10, 8)
                                  .withTrimmedTop (sectionContentTrim);
    ampModelStrip.setBounds (effectsInner.removeFromTop (42));
    effectsInner.removeFromTop (2);
    layoutKnobRow (
        effectsInner,
        { { &distortionKnob, KnobTier::detail },
          { &ampKnob, KnobTier::detail },
          { &compressorKnob, KnobTier::detail },
          { &delayKnob, KnobTier::detail },
          { &roomKnob, KnobTier::detail } }, 2);
}

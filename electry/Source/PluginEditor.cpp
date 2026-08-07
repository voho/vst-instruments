#include "PluginEditor.h"

#include "DSP/ElectryVisuals.h"

#include <BinaryData.h>

#include <cmath>

namespace
{
namespace colours
{
const juce::Colour background { 0xff100b09 };
const juce::Colour panel { 0x941a1512 };
const juce::Colour panelTop { 0x9c2a211a };
const juce::Colour panelOutline { 0xff8c7046 };
const juce::Colour binding { 0xffd7c398 };
const juce::Colour text { 0xfff3ead8 };
const juce::Colour dimText { 0xffc0aa82 };
const juce::Colour accent { 0xffd58b2b };
const juce::Colour accentBright { 0xffffbd55 };
const juce::Colour accentDark { 0xff6f301c };
const juce::Colour oxblood { 0xff4f1716 };
const juce::Colour knobFace { 0xff171310 };
const juce::Colour knobRim { 0xff8f8069 };
const juce::Colour nickel { 0xffb8ae9b };
const juce::Colour warmBone { 0xffddcda9 };
const juce::Colour ebony { 0xff17110e };
const juce::Colour keyswitchBlack { 0xff70251f };
const juce::Colour rosewood { 0xff33211a };
const juce::Colour rosewoodDark { 0xff1d120e };
const juce::Colour fretWire { 0xffbdb4a2 };
const juce::Colour sympatheticRing { 0xff6fa8b8 };
} // namespace colours

constexpr int editorWidth = 1080;
constexpr int editorHeight = 860;
constexpr int fretboardPanelHeight = 148;
constexpr int timerHz = 30;
constexpr int lastDrawnFret = electry::ElectryEngine::fretCount;
constexpr int firstKeyboardNote = electry::ElectryEngine::firstKeyswitchNote; // C0
constexpr int firstPlayableNote = electry::ElectryEngine::lowestPlayableNote; // E1
constexpr int lastKeyboardNote = electry::ElectryEngine::highestPlayableNote; // D6
constexpr int keyswitchCount = electry::ElectryEngine::keyswitchCount;
constexpr int keyboardWhiteKeyCount = 44; // C0..D6 inclusive
constexpr auto visualWeightProperty = "electryVisualWeight";

enum class KnobTier
{
    detail,
    contextual,
    character,
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
        case KnobTier::detail:     return { 68, 116, 0.52f };
        case KnobTier::contextual: return { 80, 126, 0.72f };
        case KnobTier::character:  return { 88, 142, 0.82f };
        case KnobTier::hero:       return { 104, 174, 1.00f };
        case KnobTier::master:     return { 80, 148, 0.94f };
    }
    return { 88, 142, 0.82f };
}

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
    for (const auto& slot : slots)
        preferredWidth += metricsFor (slot.tier).width;

    const auto scale = juce::jmin (
        1.0f, static_cast<float> (rowArea.getWidth())
                  / static_cast<float> (juce::jmax (1, preferredWidth)));

    int groupWidth = gap * (static_cast<int> (slots.size()) - 1);
    for (const auto& slot : slots)
        groupWidth += juce::jmax (1, juce::roundToInt (
            static_cast<float> (metricsFor (slot.tier).width) * scale));

    int x = rowArea.getX() + juce::jmax (0, (rowArea.getWidth() - groupWidth) / 2);
    for (const auto& slot : slots)
    {
        const auto metrics = metricsFor (slot.tier);
        const int width = juce::jmax (1, juce::roundToInt (
            static_cast<float> (metrics.width) * scale));
        const int height = juce::jmin (rowArea.getHeight(), metrics.height);
        slot.knob->slider.getProperties().set (
            visualWeightProperty, metrics.visualWeight);
        slot.knob->setBounds (x, rowArea.getCentreY() - height / 2, width, height);
        slot.knob->repaint();
        x += width + gap;
    }
}

constexpr std::array<const char*, keyswitchCount> keyswitchLabels {
    "DN", "UP", "ALT", "SUS", "PM", "H/P", "HARM", "PNCH", "SLD", "DEAD"
};

bool isKeyswitch (int midiNoteNumber) noexcept
{
    return midiNoteNumber >= firstKeyboardNote
        && midiNoteNumber < firstKeyboardNote + keyswitchCount;
}

// The notes between the keyswitch banks and the playable range are dead: the
// engine ignores them, and they are drawn muted so nobody hunts for a sound
// there.
bool isDeadZoneNote (int midiNoteNumber) noexcept
{
    return midiNoteNumber >= firstKeyboardNote + keyswitchCount
        && midiNoteNumber < firstPlayableNote;
}

void drawKeyswitchDecoration (juce::Graphics& graphics, juce::Rectangle<float> area,
                              int keyswitchIndex, bool selected, bool blackKey)
{
    const auto originalArea = area;
    const auto tabHeight = blackKey ? 3.0f : 4.0f;
    graphics.setColour ((selected ? colours::accentBright : colours::accent)
                            .withAlpha (selected ? 1.0f : 0.78f));
    graphics.fillRect (area.removeFromBottom (tabHeight));

    auto labelArea = area.removeFromBottom (blackKey ? 14.0f : 19.0f).reduced (0.5f);
    graphics.setColour (selected ? colours::accentBright : colours::binding);
    graphics.setFont (juce::FontOptions (blackKey ? 7.2f : 9.2f, juce::Font::bold));
    graphics.drawFittedText (
        keyswitchLabels[static_cast<std::size_t> (keyswitchIndex)],
        labelArea.getSmallestIntegerContainer(), juce::Justification::centred,
        1, 0.58f);

    if (selected)
    {
        graphics.setColour (colours::accentBright);
        graphics.drawRoundedRectangle (originalArea.reduced (1.0f),
                                       blackKey ? 2.0f : 1.5f, 2.0f);
    }
}
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

ElectryLookAndFeel::ElectryLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::knobFace);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colours::warmBone);
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::ebony);
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

    // Amp-style calibration ticks around the control.
    for (int tick = 0; tick <= 10; ++tick)
    {
        const auto tickAngle = juce::jmap (static_cast<float> (tick), 0.0f, 10.0f,
                                           rotaryStartAngle, rotaryEndAngle);
        const auto outer = centre.getPointOnCircumference (radius, tickAngle);
        const auto inner = centre.getPointOnCircumference (
            radius - (tick % 5 == 0 ? 4.5f : 3.0f), tickAngle);
        graphics.setColour ((tick == 0 || tick == 10 ? colours::binding
                                                      : colours::dimText)
                                .withAlpha (0.34f + 0.38f * visualWeight));
        graphics.drawLine ({ inner, outer },
                           (tick % 5 == 0 ? 1.25f : 0.8f) * visualWeight);
    }

    // Recessed track and glowing value arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 6.0f, radius - 6.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour (juce::Colours::black.withAlpha (0.75f));
    graphics.strokePath (track, juce::PathStrokeType (4.0f));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 6.0f, radius - 6.0f, 0.0f,
                         rotaryStartAngle, angle, true);
    graphics.setColour (colours::accentBright.withAlpha (
        0.10f + 0.18f * visualWeight));
    graphics.strokePath (value, juce::PathStrokeType (3.8f + 1.4f * visualWeight));
    graphics.setColour (colours::accentBright.withAlpha (
        0.48f + 0.52f * visualWeight));
    graphics.strokePath (value, juce::PathStrokeType (1.2f + 0.9f * visualWeight));

    // Nickel knurled rim, shadow, and black bakelite cap.
    const auto bodyRadius = radius * 0.70f;
    graphics.setColour (juce::Colours::black.withAlpha (0.55f));
    graphics.fillEllipse (centre.x - bodyRadius + 2.0f, centre.y - bodyRadius + 3.0f,
                          bodyRadius * 2.0f, bodyRadius * 2.0f);

    juce::ColourGradient rimGradient (colours::nickel.brighter (0.35f),
                                      centre.x - bodyRadius, centre.y - bodyRadius,
                                      colours::knobRim.darker (0.55f),
                                      centre.x + bodyRadius, centre.y + bodyRadius, false);
    graphics.setGradientFill (rimGradient);
    graphics.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                          bodyRadius * 2.0f, bodyRadius * 2.0f);

    const auto capRadius = bodyRadius - 3.3f;
    juce::ColourGradient capGradient (juce::Colour (0xff3b332c),
                                      centre.x - capRadius * 0.45f,
                                      centre.y - capRadius * 0.55f,
                                      colours::knobFace,
                                      centre.x + capRadius * 0.65f,
                                      centre.y + capRadius * 0.75f, true);
    graphics.setGradientFill (capGradient);
    graphics.fillEllipse (centre.x - capRadius, centre.y - capRadius,
                          capRadius * 2.0f, capRadius * 2.0f);
    graphics.setColour (juce::Colours::white.withAlpha (0.11f));
    graphics.drawEllipse (centre.x - capRadius, centre.y - capRadius,
                          capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    // Inlaid ivory pointer.
    const auto pointerLength = capRadius * 0.72f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.35f, -pointerLength, 2.7f, pointerLength, 1.3f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    graphics.setColour (colours::binding);
    graphics.fillPath (pointer);

    graphics.setColour (colours::accent.withAlpha (0.65f));
    graphics.fillEllipse (centre.x - 1.7f, centre.y - 1.7f, 3.4f, 3.4f);
}

void ElectryLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                               juce::Button& button, const juce::Colour&,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    auto fill = on ? colours::oxblood : colours::knobFace;
    if (isDown)
        fill = fill.brighter (0.25f);
    else if (isHighlighted)
        fill = fill.brighter (0.12f);

    graphics.setColour (juce::Colours::black.withAlpha (0.45f));
    graphics.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 4.0f);

    juce::ColourGradient buttonGradient (fill.brighter (isHighlighted ? 0.12f : 0.04f),
                                         bounds.getCentreX(), bounds.getY(),
                                         fill.darker (0.3f), bounds.getCentreX(),
                                         bounds.getBottom(), false);
    graphics.setGradientFill (buttonGradient);
    graphics.fillRoundedRectangle (bounds, 4.0f);
    graphics.setColour (on ? colours::accentBright : colours::panelOutline.withAlpha (0.7f));
    graphics.drawRoundedRectangle (bounds, 4.0f, on ? 1.5f : 0.8f);

    if (on)
    {
        graphics.setColour (colours::accentBright.withAlpha (0.75f));
        graphics.fillEllipse (bounds.getX() + 5.0f, bounds.getCentreY() - 1.5f, 3.0f, 3.0f);
    }
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

juce::Label* ElectryLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (12.0f));
    label->setColour (juce::Label::textColourId, colours::dimText);
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
    if (isKeyswitch (midiNoteNumber))
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
    const auto top = keyswitch ? colours::oxblood.brighter (0.22f)
                               : colours::warmBone.brighter (0.08f);
    const auto bottom = keyswitch ? colours::oxblood.darker (0.32f)
                                  : colours::warmBone.darker (0.12f);
    graphics.setGradientFill ({ top, area.getCentreX(), area.getY(),
                                bottom, area.getCentreX(), area.getBottom(), false });
    graphics.fillRect (area);

    MidiKeyboardComponent::drawWhiteNote (
        midiNoteNumber, graphics, area, isDown, isOver, lineColour, textColour);

    if (keyswitch)
    {
        const auto index = midiNoteNumber - firstKeyboardNote;
        drawKeyswitchDecoration (graphics, area, index,
                                 isKeyswitchSelected (index), false);
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
}

void ElectryKeyboardComponent::drawBlackNote (
    int midiNoteNumber, juce::Graphics& graphics, juce::Rectangle<float> area,
    bool isDown, bool isOver, juce::Colour noteFillColour)
{
    const auto keyswitch = isKeyswitch (midiNoteNumber);
    MidiKeyboardComponent::drawBlackNote (
        midiNoteNumber, graphics, area, isDown, isOver,
        keyswitch ? colours::keyswitchBlack : noteFillColour);

    if (keyswitch)
    {
        const auto index = midiNoteNumber - firstKeyboardNote;
        drawKeyswitchDecoration (graphics, area, index,
                                 isKeyswitchSelected (index), true);
    }
    else if (isDeadZoneNote (midiNoteNumber))
    {
        graphics.setColour (juce::Colours::black.withAlpha (0.55f));
        graphics.fillRect (area);
    }
}

// ---------------------------------------------------------------------------
// Choice strip
// ---------------------------------------------------------------------------

ElectryChoiceStrip::ElectryChoiceStrip (juce::String title, juce::StringArray choices)
    : titleText (std::move (title))
{
    for (int index = 0; index < choices.size(); ++index)
    {
        auto button = std::make_unique<juce::TextButton> (choices[index]);
        button->setClickingTogglesState (false);
        button->setRadioGroupId (0);
        button->onClick = [this, index]
        {
            setSelectedIndex (index);
            if (onChoice != nullptr)
                onChoice (index);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }
    if (! buttons.empty())
        buttons.front()->setToggleState (true, juce::dontSendNotification);
}

void ElectryChoiceStrip::setSelectedIndex (int newIndex)
{
    selectedIndex = juce::jlimit (0, static_cast<int> (buttons.size()) - 1, newIndex);
    for (int index = 0; index < static_cast<int> (buttons.size()); ++index)
        buttons[static_cast<std::size_t> (index)]->setToggleState (
            index == selectedIndex, juce::dontSendNotification);
}

void ElectryChoiceStrip::setTooltipText (const juce::String& text)
{
    for (auto& button : buttons)
        button->setTooltip (text);
}

void ElectryChoiceStrip::paint (juce::Graphics& graphics)
{
    graphics.setColour (colours::binding.withAlpha (0.82f));
    graphics.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    graphics.drawText (titleText, getLocalBounds().removeFromTop (14),
                       juce::Justification::centredLeft);
}

void ElectryChoiceStrip::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (16);
    if (buttons.empty())
        return;

    const int gap = 4;
    const int maxColumns = 8;
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
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (label);
}

void ElectryKnob::paint (juce::Graphics& graphics)
{
    const auto visualWeight = static_cast<float> (
        slider.getProperties().getWithDefault (visualWeightProperty, 0.82f));
    if (visualWeight < 0.9f)
        return;

    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    graphics.setColour (colours::accentDark.withAlpha (0.08f));
    graphics.fillRoundedRectangle (bounds, 7.0f);
    graphics.setColour (colours::accent.withAlpha (0.20f));
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 0.8f);
}

void ElectryKnob::resized()
{
    const auto visualWeight = static_cast<float> (
        slider.getProperties().getWithDefault (visualWeightProperty, 0.82f));
    const bool compact = visualWeight < 0.65f;
    const bool hero = visualWeight >= 0.9f;

    auto area = getLocalBounds();
    const int labelHeight = hero ? 18 : (compact ? 13 : 15);
    label.setFont (juce::FontOptions (hero ? 12.0f : (compact ? 9.5f : 10.8f),
                                     juce::Font::bold));
    label.setColour (juce::Label::textColourId,
                     hero ? colours::binding : colours::dimText);
    label.setBounds (area.removeFromTop (labelHeight));
    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow, false,
        juce::jlimit (48, hero ? 88 : 78, juce::jmax (48, getWidth() - 4)),
        hero ? 18 : 16);
    slider.setBounds (area);
}

// ---------------------------------------------------------------------------
// Status display
// ---------------------------------------------------------------------------

void ElectryStatusDisplay::setStatus (int activeVoices, int sympatheticStrings,
                                      bool ready, double sampleRate,
                                      bool scheduleRepaint)
{
    if (voices == activeVoices && sympathetic == sympatheticStrings
        && isReady == ready && juce::approximatelyEqual (rate, sampleRate))
        return;
    voices = activeVoices;
    sympathetic = sympatheticStrings;
    isReady = ready;
    rate = sampleRate;
    if (scheduleRepaint)
        repaint();
}

void ElectryStatusDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (juce::Colours::black.withAlpha (0.68f));
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (colours::nickel.withAlpha (0.45f));
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    juce::String status;
    if (! isReady)
        status = "ENGINE STANDBY";
    else
    {
        status = juce::String (voices) + (voices == 1 ? " STRING" : " STRINGS");
        if (sympathetic > 0)
            status += " +" + juce::String (sympathetic) + " RING";
        if (rate > 0.0)
            status += "  |  " + juce::String (rate / 1000.0, 1) + " kHz";
    }

    graphics.setColour (isReady ? colours::accent : colours::dimText);
    graphics.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    graphics.drawText (status, getLocalBounds().reduced (8, 0),
                       juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// Fretboard display
// ---------------------------------------------------------------------------

ElectryFretboardDisplay::ElectryFretboardDisplay()
{
    setInterceptsMouseClicks (false, false);
    setName ("Fretboard display");
}

bool ElectryFretboardDisplay::refresh (const ElectryAudioProcessor& processor,
                                       float frameSeconds)
{
    bool moving = false;
    for (int stringIndex = 0;
         stringIndex < electry::ElectryEngine::stringCount; ++stringIndex)
    {
        auto& row = rows[static_cast<std::size_t> (stringIndex)];
        const auto next = processor.getStringVisualState (stringIndex);
        const bool changed = next.midiNote != row.state.midiNote
                          || next.fret != row.state.fret
                          || next.sounding != row.state.sounding
                          || next.sympathetic != row.state.sympathetic
                          || next.releasing != row.state.releasing;
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
    return moving;
}

void ElectryFretboardDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 120.0f || bounds.getHeight() < 40.0f)
        return;

    auto tuningArea = bounds.removeFromLeft (26.0f);
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

    const auto neckX = neck.getX();
    const auto neckWidth = neck.getWidth();
    const auto fretX = [neckX, neckWidth] (int fret)
    {
        return neckX + neckWidth
             * electry::visuals::fretWireFraction (fret, lastDrawnFret);
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
            ? electry::visuals::fretWireFraction (row.state.fret, lastDrawnFret)
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
                                                              lastDrawnFret)
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

        // Tuning label.
        graphics.setColour (ringing ? colours::binding
                                    : colours::dimText.withAlpha (0.65f));
        graphics.setFont (juce::FontOptions (8.6f, juce::Font::bold));
        graphics.drawText (tuningNames[static_cast<std::size_t> (stringIndex)],
                           juce::Rectangle<float> (tuningArea.getX(), y - 6.0f,
                                                   tuningArea.getWidth(), 12.0f),
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
          BinaryData::electryguitartop_png,
          BinaryData::electryguitartop_pngSize)),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    logoLabel.setText ("ELECTRY", juce::dontSendNotification);
    logoLabel.setFont (juce::FontOptions (31.0f, juce::Font::bold));
    logoLabel.setColour (juce::Label::textColourId, colours::binding);
    addAndMakeVisible (logoLabel);

    editionLabel.setText ("PHYSICALLY MODELED DROP-E 8-STRING GUITAR",
                          juce::dontSendNotification);
    editionLabel.setFont (juce::FontOptions (11.5f));
    editionLabel.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (editionLabel);

    keyboardHintLabel.setText (
        "Oxblood keys latch the two banks: C0..D0 the pick stroke, D#0..F#0 the play style - any combination. Bone and ebony keys play the Drop-E 8-string range E1..D6.",
        juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::FontOptions (11.0f));
    keyboardHintLabel.setColour (juce::Label::textColourId, colours::dimText);
    keyboardHintLabel.setComponentID ("keyboardHint");
    addAndMakeVisible (keyboardHintLabel);

    addAndMakeVisible (statusDisplay);

    panicButton.setTooltip ("Immediately silence all strings");
    panicButton.onClick = [this] { electryProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    pickStyleStrip.onChoice = [this] (int index)
    {
        keyboard.setSelectedKeyswitches (
            index, electryProcessor.getCurrentPlayStyleIndex());
        electryProcessor.triggerArticulation (index);
    };
    pickStyleStrip.setComponentID ("pickStyleStrip");
    addAndMakeVisible (pickStyleStrip);

    playStyleStrip.onChoice = [this] (int index)
    {
        keyboard.setSelectedKeyswitches (
            electryProcessor.getCurrentPickStyleIndex(), index);
        electryProcessor.triggerArticulation (
            electry::ElectryEngine::pickStyleKeyswitchCount + index);
    };
    playStyleStrip.setComponentID ("playStyleStrip");
    addAndMakeVisible (playStyleStrip);

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
        if (auto* parameter = electryProcessor.parameters.getParameter (
                electry::parameters::outputMode))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (index)));
            parameter->endChangeGesture();
        }
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
        "Mono is an authentic summed DI. Stereo spreads the eight physical strings through a phase-coherent divided-pickup field; no delay or chorus.");
    outputModeStrip.setComponentID (electry::parameters::outputMode);
    addAndMakeVisible (outputModeStrip);

    using namespace electry::parameters;
    const auto setup = [this] (ElectryKnob& knob, const char* parameterId,
                               const char* tooltip)
    {
        knob.slider.setTooltip (tooltip);
        knob.setComponentID (parameterId);
        addAndMakeVisible (knob);
        attachSlider (knob.slider, parameterId);
    };

    setup (bodyWoodKnob, bodyWood,
           "Body wood: mahogany/maple blank toward swamp ash");
    setup (bodySizeKnob, bodySize,
           "Body mass and thickness: thick heavy blank toward thin light slab");
    setup (bodyShapeKnob, bodyShape,
           "Body outline: carved single-cut toward flat slab");
    setup (constructionKnob, construction,
           "Neck joint and bridge: set neck + stopbar toward bolt-on + through-body");
    setup (scaleLengthKnob, scaleLength,
           "Scale length: 25.5 in electric toward 28 in baritone / 8-string");
    setup (bodyResonanceKnob, bodyResonance,
           "How much solid-body structural colour reaches the pickups");
    setup (pickupTypeKnob, pickupType,
           "Pickup construction: wide humbucker toward narrow single coil");
    setup (toneKnob, tone, "Passive tone control loading the pickup resonance");
    setup (stringGaugeKnob, stringGauge,
           "Drop-E string set: light .009-.080 toward heavy .011-.098");
    setup (stringAgeKnob, stringAge, "String condition: fresh toward old and dead");
    setup (pickPositionKnob, pickPosition,
           "Picking spot: close to the bridge toward over the neck");
    setup (pickHardnessKnob, pickHardness,
           "Plectrum stiffness and edge: soft and round toward hard and sharp");
    setup (bendTimeKnob, bendTime,
           "Travel time of a pitch-wheel bend: how long the strings take to "
           "reach the wheel rather than snapping to it");
    setup (muteDampingKnob, muteDamping,
           "Palm-mute strength for the Palm mute play style: loose half-mute "
           "toward a tight metal chug");
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
           "Continuous bridge-hand damping for every play style, on top of the "
           "Palm Mute keyswitch. MIDI CC2 adds to it while you play.");
    setup (strumSpreadKnob, strumSpread,
           "Pick travel time per string crossed. At 0 ms a chord starts as one "
           "block; higher values sweep it string by string.");
    setup (resonanceKnob, resonanceDepth,
           "Full-scale reach of the modulation-wheel (CC1) resonance: how far "
           "the wheel can raise the sympathetic coupling and how much of the "
           "amplified output may feed back into the strings. At 100% a "
           "distorted tone self-resonates with the wheel up.");
    setup (outputKnob, output, "Master output level");
    setup (distortionKnob, distortion, "Parallel high-gain distortion drive");
    setup (ampKnob, amp, "Saturated guitar amp and cabinet simulation");
    setup (compressorKnob, compressor, "Fast levelling for tight rhythm playing");
    setup (delayKnob, delay, "Tempo-neutral 360 ms lead delay");
    setup (roomKnob, room, "Compact stereo room ambience");

    fretboardDisplay.setComponentID ("fretboard");
    addAndMakeVisible (fretboardDisplay);

    keyboard.setAvailableRange (firstKeyboardNote, lastKeyboardNote);
    keyboard.setLowestVisibleKey (firstKeyboardNote);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyWidth (24.0f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setComponentID ("keyboard");
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
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (
        electryProcessor.parameters, parameterId, slider));
}

void ElectryAudioProcessorEditor::timerCallback()
{
    statusDisplay.setStatus (electryProcessor.getActiveVoiceCount(),
                             electryProcessor.getSympatheticStringCount(),
                             electryProcessor.isEngineReady(),
                             electryProcessor.getCurrentSampleRateForDisplay());
    const auto pickIndex = electryProcessor.getCurrentPickStyleIndex();
    const auto styleIndex = electryProcessor.getCurrentPlayStyleIndex();
    pickStyleStrip.setSelectedIndex (pickIndex);
    playStyleStrip.setSelectedIndex (styleIndex);
    keyboard.setSelectedKeyswitches (pickIndex, styleIndex);

    if (fretboardDisplay.refresh (electryProcessor, 1.0f / static_cast<float> (timerHz)))
        fretboardDisplay.repaint();
}

void ElectryAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (colours::background);
    if (backgroundImage.isValid())
        graphics.drawImageWithin (backgroundImage, 0, 0, getWidth(), getHeight(),
                                  juce::RectanglePlacement::stretchToFit);

    // Darken the centre enough for controls, while retaining the generated
    // lacquer and binding at the perimeter.
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.28f),
                                0.0f, 0.0f,
                                juce::Colours::black.withAlpha (0.58f),
                                0.0f, static_cast<float> (getHeight()), false);
    graphics.setGradientFill (shade);
    graphics.fillRect (getLocalBounds());

    // Eight fine strings tie the header visually to the extended keyboard.
    graphics.setColour (colours::nickel.withAlpha (0.23f));
    for (int string = 0; string < 8; ++string)
    {
        const auto y = 51.0f + static_cast<float> (string) * 1.35f;
        graphics.drawLine (12.0f, y, static_cast<float> (getWidth() - 12), y,
                           0.42f + static_cast<float> (string) * 0.05f);
    }

    const std::array<const char*, sectionCount> titles {
        "", "FRETBOARD  (LIVE STRING VIEW)", "PERFORMANCE",
        "CORE TONE & RESPONSE", "MASTER", "GUITAR BUILD", "PLAY DETAIL", "FX"
    };

    for (int section = 0; section < sectionCount; ++section)
    {
        const auto bounds = sectionBounds[static_cast<std::size_t> (section)];
        if (bounds.isEmpty())
            continue;
        const auto panelBounds = bounds.toFloat();
        const bool prioritySection = section == coreSection || section == masterSection;
        graphics.setColour (juce::Colours::black.withAlpha (0.22f));
        graphics.fillRoundedRectangle (panelBounds.translated (0.0f, 2.0f), 8.0f);

        juce::ColourGradient panelGradient (
                                            prioritySection
                                                ? colours::panelTop.brighter (0.055f)
                                                : colours::panelTop,
                                            panelBounds.getCentreX(), panelBounds.getY(),
                                            colours::panel,
                                            panelBounds.getCentreX(), panelBounds.getBottom(), false);
        graphics.setGradientFill (panelGradient);
        graphics.fillRoundedRectangle (panelBounds, 8.0f);
        graphics.setColour (colours::panelOutline.withAlpha (0.78f));
        graphics.drawRoundedRectangle (panelBounds.reduced (0.5f), 8.0f, 0.9f);
        graphics.setColour (colours::binding.withAlpha (0.12f));
        graphics.drawRoundedRectangle (panelBounds.reduced (2.0f), 6.5f, 0.8f);

        // Small pickup-plate screws make each section feel physically mounted.
        for (const auto corner : { panelBounds.getTopLeft(), panelBounds.getTopRight(),
                                   panelBounds.getBottomLeft(), panelBounds.getBottomRight() })
        {
            const auto inset = juce::Point<float> (
                corner.x < panelBounds.getCentreX() ? 6.0f : -6.0f,
                corner.y < panelBounds.getCentreY() ? 6.0f : -6.0f);
            const auto screw = corner + inset;
            graphics.setColour (juce::Colours::black.withAlpha (0.65f));
            graphics.fillEllipse (screw.x - 2.2f, screw.y - 2.2f, 4.4f, 4.4f);
            graphics.setColour (colours::nickel.withAlpha (0.72f));
            graphics.fillEllipse (screw.x - 1.6f, screw.y - 1.6f, 3.2f, 3.2f);
            graphics.setColour (juce::Colours::black.withAlpha (0.65f));
            graphics.drawLine (screw.x - 1.0f, screw.y, screw.x + 1.0f, screw.y, 0.65f);
        }

        if (titles[static_cast<std::size_t> (section)][0] != '\0')
        {
            graphics.setColour ((prioritySection ? colours::accentBright
                                                 : colours::binding)
                                    .withAlpha (prioritySection ? 0.92f : 0.80f));
            graphics.setFont (juce::FontOptions (
                prioritySection ? 11.5f : 10.5f, juce::Font::bold));
            graphics.drawText (titles[static_cast<std::size_t> (section)],
                               bounds.reduced (12, 6).removeFromTop (14),
                               juce::Justification::centredLeft);

            if (prioritySection)
            {
                graphics.setColour (colours::accent.withAlpha (0.55f));
                graphics.fillRoundedRectangle (
                    static_cast<float> (bounds.getX() + 12),
                    static_cast<float> (bounds.getY() + 23), 42.0f, 2.0f, 1.0f);
            }
        }
    }
}

void ElectryAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header.
    auto header = area.removeFromTop (44);
    logoLabel.setBounds (header.removeFromLeft (170));
    editionLabel.setBounds (header.removeFromLeft (330));
    panicButton.setBounds (header.removeFromRight (76).reduced (0, 8));
    header.removeFromRight (8);
    statusDisplay.setBounds (header.removeFromRight (210).reduced (0, 8));

    area.removeFromTop (6);

    // Keyboard and hint at the bottom.
    keyboardHintLabel.setBounds (area.removeFromBottom (18));
    keyboard.setBounds (area.removeFromBottom (96));
    keyboard.setKeyWidth (static_cast<float> (keyboard.getWidth())
                          / static_cast<float> (keyboardWhiteKeyCount));
    area.removeFromBottom (8);

    // The two keyswitch strips across the top: the pick stroke and the play
    // style, latched independently.
    auto articulationArea = area.removeFromTop (76);
    sectionBounds[articulationSection] = articulationArea;
    auto stripRow = articulationArea.reduced (12, 8);
    const int pickWidth = juce::roundToInt (
        static_cast<float> (stripRow.getWidth()) * 0.40f);
    pickStyleStrip.setBounds (stripRow.removeFromLeft (pickWidth));
    stripRow.removeFromLeft (12);
    playStyleStrip.setBounds (stripRow);
    area.removeFromTop (8);

    // The live fretboard sits directly under the play styles, beside the four
    // performance controls that change what it shows.
    {
        auto fretboardRow = area.removeFromTop (
            juce::jmin (fretboardPanelHeight, juce::jmax (0, area.getHeight() - 260)));
        area.removeFromTop (8);
        auto performanceArea = fretboardRow.removeFromRight (
            juce::jmin (300, fretboardRow.getWidth() / 3));
        fretboardRow.removeFromRight (8);
        sectionBounds[fretboardSection] = fretboardRow;
        sectionBounds[performanceSection] = performanceArea;

        fretboardDisplay.setBounds (fretboardRow.reduced (12).withTrimmedTop (14));
        layoutKnobRow (
            performanceArea.reduced (10).withTrimmedTop (14),
            { { &sympatheticKnob, KnobTier::detail },
              { &palmMuteKnob, KnobTier::detail },
              { &strumSpreadKnob, KnobTier::detail },
              { &resonanceKnob, KnobTier::detail } },
            4);
    }

    // The remaining 410 px is deliberately split by sonic importance rather
    // than by parameter type. Controls that reshape every note occupy the
    // large upper row; construction axes are medium; articulation-specific
    // texture controls are visibly subordinate.
    const int mainHeight = juce::jlimit (
        180, area.getHeight() - 150,
        juce::roundToInt (static_cast<float> (area.getHeight()) * 0.532f));
    auto mainRow = area.removeFromTop (mainHeight);
    area.removeFromTop (8);
    auto secondaryRow = area;

    auto masterArea = mainRow.removeFromRight (104);
    mainRow.removeFromRight (8);
    auto coreArea = mainRow;
    sectionBounds[coreSection] = coreArea;
    sectionBounds[masterSection] = masterArea;

    {
        auto inner = coreArea.reduced (12).withTrimmedTop (16);
        auto selectorArea = inner.removeFromLeft (juce::jmin (150, inner.getWidth()));
        pickupStrip.setBounds (selectorArea.withSizeKeepingCentre (
            selectorArea.getWidth(), juce::jmin (64, selectorArea.getHeight())));
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
            6);
    }

    {
        auto masterInner = masterArea.reduced (12).withTrimmedTop (16);
        outputModeStrip.setBounds (masterInner.removeFromTop (48));
        masterInner.removeFromTop (2);
        layoutKnobRow (
            masterInner,
            { { &outputKnob, KnobTier::master } }, 0);
    }

    const int secondaryContentWidth = juce::jmax (0, secondaryRow.getWidth() - 8);
    const int buildWidth = juce::roundToInt (
        static_cast<float> (secondaryContentWidth) * 0.43f);
    auto buildArea = secondaryRow.removeFromLeft (buildWidth);
    secondaryRow.removeFromLeft (juce::jmin (8, secondaryRow.getWidth()));
    const int detailWidth = juce::roundToInt (
        static_cast<float> (secondaryContentWidth) * 0.30f);
    auto detailArea = secondaryRow.removeFromLeft (detailWidth);
    secondaryRow.removeFromLeft (juce::jmin (8, secondaryRow.getWidth()));
    auto effectsArea = secondaryRow;
    sectionBounds[buildSection] = buildArea;
    sectionBounds[detailSection] = detailArea;
    sectionBounds[effectsSection] = effectsArea;

    layoutKnobRow (
        buildArea.reduced (12).withTrimmedTop (16),
        { { &bodyWoodKnob, KnobTier::character },
          { &bodySizeKnob, KnobTier::character },
          { &bodyShapeKnob, KnobTier::character },
          { &constructionKnob, KnobTier::character },
          { &scaleLengthKnob, KnobTier::character },
          { &stringGaugeKnob, KnobTier::character } },
        6);

    layoutKnobRow (
        detailArea.reduced (12).withTrimmedTop (16),
        { { &muteDampingKnob, KnobTier::contextual },
          { &bendTimeKnob, KnobTier::detail },
          { &pickNoiseKnob, KnobTier::detail },
          { &fingerNoiseKnob, KnobTier::detail },
          { &releaseNoiseKnob, KnobTier::detail },
          { &artifactsKnob, KnobTier::detail } },
        4);

    layoutKnobRow (
        effectsArea.reduced (10).withTrimmedTop (16),
        { { &distortionKnob, KnobTier::detail },
          { &ampKnob, KnobTier::detail },
          { &compressorKnob, KnobTier::detail },
          { &delayKnob, KnobTier::detail },
          { &roomKnob, KnobTier::detail } }, 2);
}

#include "PluginEditor.h"

#include <BinaryData.h>

namespace
{
namespace colours
{
const juce::Colour background { 0xff100b09 };
const juce::Colour panel { 0xe81a1512 };
const juce::Colour panelTop { 0xe82a211a };
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
} // namespace colours

constexpr int editorWidth = 1080;
constexpr int editorHeight = 720;
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
    "DN", "UP", "ALT", "H/P", "TAP", "PM", "CHUG", "DEAD",
    "HARM", "PINCH", "TREM", "B+1", "B+2", "R1", "R2", "SLAP"
};

bool isKeyswitch (int midiNoteNumber) noexcept
{
    return midiNoteNumber >= firstKeyboardNote
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

void ElectryKeyboardComponent::setSelectedKeyswitchIndex (int newIndex)
{
    const auto clamped = juce::jlimit (0, keyswitchCount - 1, newIndex);
    if (selectedKeyswitchIndex == clamped)
        return;

    selectedKeyswitchIndex = clamped;
    repaint();
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
                                 index == selectedKeyswitchIndex, false);
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
                                 index == selectedKeyswitchIndex, true);
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

void ElectryStatusDisplay::setStatus (int activeVoices, bool ready, double sampleRate,
                                      bool scheduleRepaint)
{
    if (voices == activeVoices && isReady == ready
        && juce::approximatelyEqual (rate, sampleRate))
        return;
    voices = activeVoices;
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
        if (rate > 0.0)
            status += "  |  " + juce::String (rate / 1000.0, 1) + " kHz";
    }

    graphics.setColour (isReady ? colours::accent : colours::dimText);
    graphics.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    graphics.drawText (status, getLocalBounds().reduced (8, 0),
                       juce::Justification::centredLeft);
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
        "Oxblood keys C0..D#1 latch the play style. Bone and ebony keys play the Drop-E 8-string range E1..D6.",
        juce::dontSendNotification);
    keyboardHintLabel.setFont (juce::FontOptions (11.0f));
    keyboardHintLabel.setColour (juce::Label::textColourId, colours::dimText);
    keyboardHintLabel.setComponentID ("keyboardHint");
    addAndMakeVisible (keyboardHintLabel);

    addAndMakeVisible (statusDisplay);

    panicButton.setTooltip ("Immediately silence all strings");
    panicButton.onClick = [this] { electryProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    articulationStrip.onChoice = [this] (int index)
    {
        keyboard.setSelectedKeyswitchIndex (index);
        electryProcessor.triggerArticulation (index);
    };
    articulationStrip.setComponentID ("articulationStrip");
    addAndMakeVisible (articulationStrip);

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
    setup (bendTimeKnob, bendTime, "Travel time of keyswitched finger bends");
    setup (muteDampingKnob, muteDamping,
           "Palm-mute strength for the Muted play style");
    setup (velocityKnob, velocity, "How strongly MIDI velocity drives the pluck");
    setup (pickNoiseKnob, pickNoise, "Plectrum contact and scrape level");
    setup (fingerNoiseKnob, fingerNoise, "Fretting-hand contact level");
    setup (releaseNoiseKnob, releaseNoise, "String damping noise at note end");
    setup (artifactsKnob, artifacts,
           "Amount of subtle deterministic hardware ring, fret buzz, and incidental collision");
    setup (outputKnob, output, "Master output level");

    keyboard.setAvailableRange (firstKeyboardNote, lastKeyboardNote);
    keyboard.setLowestVisibleKey (firstKeyboardNote);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyWidth (24.0f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setComponentID ("keyboard");
    addAndMakeVisible (keyboard);

    setSize (editorWidth, editorHeight);
    startTimerHz (12);

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
                             electryProcessor.isEngineReady(),
                             electryProcessor.getCurrentSampleRateForDisplay());
    const auto articulationIndex = electryProcessor.getCurrentArticulationIndex();
    articulationStrip.setSelectedIndex (articulationIndex);
    keyboard.setSelectedKeyswitchIndex (articulationIndex);
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
        "", "CORE TONE & RESPONSE", "MASTER", "GUITAR BUILD", "PLAY DETAIL"
    };

    for (int section = 0; section < sectionCount; ++section)
    {
        const auto bounds = sectionBounds[static_cast<std::size_t> (section)];
        if (bounds.isEmpty())
            continue;
        const auto panelBounds = bounds.toFloat();
        const bool prioritySection = section == coreSection || section == masterSection;
        graphics.setColour (juce::Colours::black.withAlpha (0.42f));
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

    // Articulation strip across the top.
    auto articulationArea = area.removeFromTop (106);
    sectionBounds[articulationSection] = articulationArea;
    articulationStrip.setBounds (articulationArea.reduced (12, 8));
    area.removeFromTop (8);

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
        static_cast<float> (secondaryContentWidth) * 0.55725f);
    auto buildArea = secondaryRow.removeFromLeft (buildWidth);
    secondaryRow.removeFromLeft (juce::jmin (8, secondaryRow.getWidth()));
    auto detailArea = secondaryRow;
    sectionBounds[buildSection] = buildArea;
    sectionBounds[detailSection] = detailArea;

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
}

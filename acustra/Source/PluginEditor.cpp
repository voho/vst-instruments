#include "PluginEditor.h"
#include "AcustraUIAssets.h"

#include <vector>

namespace
{
constexpr int designWidth = 1120;
constexpr int designHeight = 800;
constexpr int minimumWidth = 896;
constexpr int minimumHeight = 640;
constexpr int maximumWidth = 1456;
constexpr int maximumHeight = 1040;
constexpr int keyboardFirstNote = 38; // Drop-D low string
constexpr int keyboardLastNote = 84;  // twentieth fret of the high E string
constexpr int keyboardWhiteKeyCount = 28;

struct ConstructionPreset
{
    const char* name;
    acustra::BodyShape shape;
    acustra::BodyMaterial wood;
    acustra::StringMaterial strings;
};

// Construction directions, not measured replicas of manufacturer models.
constexpr std::array<ConstructionPreset, 4> constructionPresets {{
    { "Dreadnought / Martin style", acustra::BodyShape::Dreadnought,
      acustra::BodyMaterial::Spruce, acustra::StringMaterial::Steel },
    { "Auditorium / Taylor style", acustra::BodyShape::Auditorium,
      acustra::BodyMaterial::Spruce, acustra::StringMaterial::Steel },
    { "Parlor / Fender style", acustra::BodyShape::Parlor,
      acustra::BodyMaterial::Spruce, acustra::StringMaterial::Steel },
    { "Classical nylon", acustra::BodyShape::Auditorium,
      acustra::BodyMaterial::Cedar, acustra::StringMaterial::Nylon }
}};

// Palette drawn from the classical-guitar reference: pale soundboard, ebony
// fingerboard, rosewood furniture, ivory nut and muted gold machines.
const juce::Colour ebony { 0xff08090b };
const juce::Colour pianoBlack { 0xff050607 };
const juce::Colour pianoHighlight { 0xff3c3b3d };
const juce::Colour darkWood { 0xff26130e };
const juce::Colour rosewood { 0xff421b13 };
const juce::Colour soundboard { 0xffe6b65e };
const juce::Colour ivory { 0xfffff4d8 };
const juce::Colour mutedText { 0xffd7c5a3 };
const juce::Colour brass { 0xffb88635 };
const juce::Colour panel { 0xff171110 };
const juce::Colour panelEdge { 0xff825332 };

juce::Font displayFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (
        juce::Font::getDefaultSansSerifFontName(), height, style));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = bounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillRoundedRectangle (area.translated (0.0f, 2.0f), 12.0f);
    juce::ColourGradient fill { panel.brighter (0.08f).withAlpha (0.90f),
                                area.getX(), area.getY(),
                                panel.darker (0.24f).withAlpha (0.90f),
                                area.getX(), area.getBottom(),
                                false };
    g.setGradientFill (fill);
    g.fillRoundedRectangle (area, 12.0f);
    g.setColour (panelEdge.withAlpha (0.66f));
    g.drawRoundedRectangle (area.reduced (0.5f), 12.0f, 1.0f);
    g.setColour (ivory.withAlpha (0.07f));
    g.drawRoundedRectangle (area.reduced (2.0f), 10.0f, 0.8f);
}
} // namespace

AcustraLookAndFeel::AcustraLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, brass);
    setColour (juce::Slider::rotarySliderOutlineColourId, panelEdge);
    setColour (juce::Slider::textBoxTextColourId, ivory);
    setColour (juce::Slider::textBoxBackgroundColourId, ebony.withAlpha (0.72f));
    setColour (juce::Slider::textBoxOutlineColourId, ivory.withAlpha (0.42f));
    setColour (juce::Label::textColourId, ivory);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, panel.withAlpha (1.0f));
    setColour (juce::TooltipWindow::textColourId, ivory);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
    setColour (juce::TextButton::buttonColourId, rosewood);
    setColour (juce::TextButton::buttonOnColourId, ebony.brighter (0.12f));
    setColour (juce::TextButton::textColourOffId, ivory.withAlpha (0.90f));
    setColour (juce::TextButton::textColourOnId, ivory);
    setColour (juce::ComboBox::backgroundColourId, ebony);
    setColour (juce::ComboBox::textColourId, ivory);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::focusedOutlineColourId, brass);
    setColour (juce::ComboBox::arrowColourId, brass);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, ivory);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, rosewood);
    setColour (juce::PopupMenu::highlightedTextColourId, ivory);
}

void AcustraLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height)
                            .toFloat().reduced (5.0f);
    const auto radius = 0.5f * juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle
                     + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto trackRadius = radius - 3.5f;
    const auto trackWidth = juce::jmax (2.5f, radius * 0.085f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (ebony.withAlpha (0.76f));
    g.strokePath (track, juce::PathStrokeType (
        trackWidth, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    if (angle > rotaryStartAngle + 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, trackRadius, trackRadius,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (
            juce::Slider::rotarySliderFillColourId));
        g.strokePath (valueArc, juce::PathStrokeType (
            trackWidth, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }

    const auto bodyRadius = radius - trackWidth * 1.9f;
    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillEllipse (centre.x - bodyRadius + 1.8f,
                   centre.y - bodyRadius + 3.0f,
                   bodyRadius * 2.0f, bodyRadius * 2.0f);

    juce::ColourGradient body { pianoHighlight,
                                centre.x - bodyRadius * 0.64f,
                                centre.y - bodyRadius * 0.72f,
                                pianoBlack, centre.x + bodyRadius * 0.52f,
                                centre.y + bodyRadius * 0.80f, false };
    body.addColour (0.34, juce::Colour { 0xff18191b });
    body.addColour (0.72, ebony);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                   bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (ivory.withAlpha (0.72f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                   bodyRadius * 2.0f, bodyRadius * 2.0f, 1.25f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    juce::Path highlight;
    highlight.addCentredArc (centre.x, centre.y,
                             bodyRadius * 0.78f, bodyRadius * 0.78f,
                             0.0f, 3.65f, 5.78f, true);
    g.strokePath (highlight, juce::PathStrokeType (
        1.3f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    const auto pointerLength = bodyRadius * 0.74f;
    const auto pointerWidth = juce::jmax (1.7f, bodyRadius * 0.10f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -pointerLength,
                                 pointerWidth, pointerLength,
                                 pointerWidth * 0.5f);
    pointer.applyTransform (
        juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (ivory);
    g.fillPath (pointer);
    g.setColour (brass.darker (0.10f));
    const auto pinRadius = juce::jmax (2.4f, bodyRadius * 0.075f);
    g.fillEllipse (centre.x - pinRadius, centre.y - pinRadius,
                   pinRadius * 2.0f, pinRadius * 2.0f);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (ivory.withAlpha (0.90f));
        g.drawEllipse (bounds.reduced (1.0f), 2.0f);
    }
}

void AcustraLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& button, const juce::Colour& colour,
    bool isHighlighted, bool isDown)
{
    auto fill = colour;
    if (isDown)
        fill = fill.brighter (0.28f);
    else if (isHighlighted)
        fill = fill.brighter (0.13f);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (button.getToggleState()
                     ? ivory.withAlpha (0.92f) : panelEdge);
    g.drawRoundedRectangle (bounds, 7.0f,
                            button.getToggleState() ? 1.5f : 1.0f);
    if (button.hasKeyboardFocus (true))
    {
        g.setColour (brass.brighter (0.20f));
        g.drawRoundedRectangle (bounds.reduced (2.0f), 5.5f, 2.0f);
    }
}

juce::Font AcustraLookAndFeel::getTextButtonFont (juce::TextButton&,
                                                   int buttonHeight)
{
    return displayFont (juce::jlimit (
        13.0f, 18.0f, static_cast<float> (buttonHeight) * 0.40f),
        juce::Font::bold);
}

juce::Font AcustraLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return displayFont (16.0f, juce::Font::bold);
}

class AcustraAudioProcessorEditor::ChoiceButtonGroup final
    : public juce::Component
{
public:
    ChoiceButtonGroup (juce::RangedAudioParameter& parameter,
                       const juce::StringArray& items,
                       const juce::String& groupName,
                       const juce::String& description)
        : attachment (parameter, [this] (float value)
          {
              const auto selected = juce::jlimit (
                  0, static_cast<int> (buttons.size()) - 1,
                  juce::roundToInt (value));
              buttons[static_cast<std::size_t> (selected)]->setToggleState (
                  true, juce::dontSendNotification);
          })
    {
        jassert (! items.isEmpty() && items.size() < 8);
        buttons.reserve (static_cast<std::size_t> (items.size()));

        for (int item = 0; item < items.size(); ++item)
        {
            auto button = std::make_unique<juce::TextButton> (items[item]);
            button->setName (groupName + ": " + items[item]);
            button->setTitle (groupName + ": " + items[item]);
            button->setDescription (description);
            button->setTooltip (description);
            button->setWantsKeyboardFocus (true);
            button->setClickingTogglesState (true);
            button->setRadioGroupId (1, juce::dontSendNotification);
            button->onClick = [this, item]
            {
                attachment.setValueAsCompleteGesture (static_cast<float> (item));
            };
            addAndMakeVisible (*button);
            buttons.push_back (std::move (button));
        }

        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        constexpr int gap = 4;
        constexpr int columns = 2;
        const auto rows = (static_cast<int> (buttons.size()) + columns - 1)
                        / columns;
        const auto rowHeight = (area.getHeight() - gap * (rows - 1)) / rows;

        for (int row = 0, item = 0; row < rows; ++row)
        {
            auto rowArea = area.removeFromTop (rowHeight);
            if (row + 1 < rows)
                area.removeFromTop (gap);

            const auto remaining = static_cast<int> (buttons.size()) - item;
            if (remaining == 1)
            {
                buttons[static_cast<std::size_t> (item)]->setBounds (rowArea);
                break;
            }

            const auto buttonWidth = (rowArea.getWidth() - gap) / columns;
            buttons[static_cast<std::size_t> (item++)]->setBounds (
                rowArea.removeFromLeft (buttonWidth));
            rowArea.removeFromLeft (gap);
            buttons[static_cast<std::size_t> (item++)]->setBounds (rowArea);
        }
    }

private:
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::ParameterAttachment attachment;
};

AcustraAudioProcessorEditor::AcustraAudioProcessorEditor (
    AcustraAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse), audioProcessor (processorToUse)
{
    setLookAndFeel (&lookAndFeel);
    cedarBackground = juce::ImageCache::getFromMemory (
        AcustraUIAssets::cedarbackground_png,
        AcustraUIAssets::cedarbackground_pngSize);
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setTitle ("Acustra acoustic guitar controls");
    setDescription (
        "Physically modelled acoustic guitar controls and an on-screen MIDI keyboard");

    titleLabel.setText ("ACUSTRA", juce::dontSendNotification);
    titleLabel.setFont (displayFont (40.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, ivory);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setInterceptsMouseClicks (false, false);
    titleLabel.setAccessible (false);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("PHYSICALLY MODELLED ACOUSTIC GUITAR",
                           juce::dontSendNotification);
    subtitleLabel.setFont (displayFont (15.5f, juce::Font::bold));
    subtitleLabel.setColour (juce::Label::textColourId, brass);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setInterceptsMouseClicks (false, false);
    subtitleLabel.setAccessible (false);
    addAndMakeVisible (subtitleLabel);

    statusLabel.setName ("Engine status");
    statusLabel.setTitle ("Engine status");
    statusLabel.setDescription (
        "Audio sample rate, sounding strings, or an incompatible capture selection");
    statusLabel.setFont (displayFont (15.5f));
    statusLabel.setColour (juce::Label::textColourId, mutedText);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    panicButton.setName ("Panic");
    panicButton.setTitle ("Panic");
    panicButton.setDescription ("Immediately stop every sounding string");
    panicButton.setTooltip ("Stop all strings and clear the sustain pedal");
    panicButton.setWantsKeyboardFocus (true);
    panicButton.onClick = [this] { audioProcessor.requestPanic(); };
    addAndMakeVisible (panicButton);

    configureSetupMenu (
        0, "GUITAR", "Set body shape, wood and string construction together. "
        "Manufacturer styles are generic directions, not measured replicas. "
        "Adjust any construction control below to make your own guitar.");
    auto& guitarMenu = setupControls[0];
    guitarMenu.addItem ("Custom construction", 1);
    guitarMenu.setItemEnabled (1, false);
    for (std::size_t index = 0; index < constructionPresets.size(); ++index)
        guitarMenu.addItem (constructionPresets[index].name,
                            static_cast<int> (index) + 2);
    guitarMenu.onChange = [this]
    {
        const auto index = setupControls[0].getSelectedId() - 2;
        if (index < 0 || index >= static_cast<int> (constructionPresets.size()))
            return;
        const auto& preset = constructionPresets[static_cast<std::size_t> (index)];
        const auto setChoice = [this] (const char* id, auto value)
        {
            if (auto* parameter = audioProcessor.parameters.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (static_cast<float> (value)));
                parameter->endChangeGesture();
            }
        };
        setChoice (acustra::parameters::shape, preset.shape);
        setChoice (acustra::parameters::bodyMaterial, preset.wood);
        setChoice (acustra::parameters::stringMaterial, preset.strings);
        timerCallback();
    };
    configureSetupMenu (
        1, "PICKING", "Finger, pick or thumb excitation. Touch adjusts the "
        "contact within the selected technique. MIDI: CC2 bridge-hand damping; "
        "CC68 legato; note-off velocity controls finger lift.");
    configureSetupMenu (
        2, "CAPTURE", "Listen through body microphones, bridge-force saddle "
        "piezo or a magnetic string pickup. Magnetic pickups require steel "
        "strings; a magnetic capture selected by automation is silent on nylon.");
    constexpr std::array<const char*, 2> setupParameterIds {
        acustra::parameters::picking, acustra::parameters::capture
    };
    for (std::size_t index = 0; index < setupParameterIds.size(); ++index)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
            audioProcessor.parameters.getParameter (setupParameterIds[index]));
        jassert (parameter != nullptr);
        if (parameter == nullptr)
            continue;
        auto& control = setupControls[index + 1];
        control.addItemList (parameter->choices, 1);
        setupAttachments[index] = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                audioProcessor.parameters, setupParameterIds[index], control);
    }

    configureChoice (
        0, "BODY SHAPE", acustra::parameters::shape,
        "Bounded low-frequency body-size direction, from compact to large");
    configureChoice (
        1, "BODY MATERIAL", acustra::parameters::bodyMaterial,
        "Bounded high-frequency wood direction, not captured wood identification");
    configureChoice (
        2, "STRINGS", acustra::parameters::stringMaterial,
        "Switch the physical string construction between nylon and steel");
    configureChoice (
        3, "TUNING", acustra::parameters::tuning,
        "Open-string tuning used by the six-string allocator");

    configureSlider (
        0, "STRING AGE", acustra::parameters::stringAge,
        "Fresh strings at zero; increasingly worn and dark strings at 100 percent");
    configureSlider (
        1, "PLUCK POSITION", acustra::parameters::pluckPosition,
        "Pluck near the bridge at zero or toward the neck at 100 percent");
    configureSlider (
        2, "TOUCH", acustra::parameters::touch,
        "Softer, darker contact at zero; harder, brighter contact at 100 percent");
    configureSlider (
        3, "BODY", acustra::parameters::bodyAmount,
        "Strength of the measurement-derived body radiation");
    configureSlider (
        4, "STEREO", acustra::parameters::stereoWidth,
        "Width of the measurement-derived, mono-compatible body radiation");
    configureSlider (
        5, "OUTPUT", acustra::parameters::output,
        "Final output level in decibels", true);

    keyboard.setName ("Acustra MIDI keyboard");
    keyboard.setTitle ("MIDI keyboard");
    keyboard.setDescription (
        "Play Acustra from the computer mouse, keyboard, or an attached MIDI controller");
    keyboard.setWantsKeyboardFocus (true);
    keyboard.setAvailableRange (keyboardFirstNote, keyboardLastNote);
    keyboard.setLowestVisibleKey (keyboardFirstNote);
    keyboard.setMidiChannel (1);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (minimumWidth, minimumHeight, maximumWidth, maximumHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (
            static_cast<double> (designWidth) / designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (12);
    timerCallback();
}

AcustraAudioProcessorEditor::~AcustraAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AcustraAudioProcessorEditor::configureSetupMenu (
    std::size_t index, const juce::String& name, const juce::String& description)
{
    auto& label = setupLabels[index];
    label.setText (name, juce::dontSendNotification);
    label.setFont (displayFont (15.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, brass);
    label.setInterceptsMouseClicks (false, false);
    label.setAccessible (false);
    addAndMakeVisible (label);

    auto& control = setupControls[index];
    control.setName (name);
    control.setTitle (name);
    control.setDescription (description);
    control.setTooltip (description);
    control.setWantsKeyboardFocus (true);
    addAndMakeVisible (control);
}

void AcustraAudioProcessorEditor::updateConstructionControls()
{
    const auto state = audioProcessor.snapshotEngineParameters();
    int presetId = 1;
    for (std::size_t index = 0; index < constructionPresets.size(); ++index)
    {
        const auto& preset = constructionPresets[index];
        if (state.shape == preset.shape && state.bodyMaterial == preset.wood
            && state.stringMaterial == preset.strings)
        {
            presetId = static_cast<int> (index) + 2;
            break;
        }
    }
    setupControls[0].setSelectedId (presetId, juce::dontSendNotification);
    setupControls[2].setItemEnabled (
        5, state.stringMaterial == acustra::StringMaterial::Steel);
}

void AcustraAudioProcessorEditor::configureChoice (
    std::size_t index, const juce::String& name, const char* parameterId,
    const juce::String& description)
{
    auto& label = choiceLabels[index];
    label.setText (name, juce::dontSendNotification);
    label.setFont (displayFont (17.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, brass);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    label.setAccessible (false);
    addAndMakeVisible (label);

    auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
        audioProcessor.parameters.getParameter (parameterId));
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto& control = choiceControls[index];
    control = std::make_unique<ChoiceButtonGroup> (
        *parameter, parameter->choices, name, description);
    addAndMakeVisible (*control);
}

void AcustraAudioProcessorEditor::configureSlider (
    std::size_t index, const juce::String& name, const char* parameterId,
    const juce::String& description, bool decibels)
{
    auto& label = sliderLabels[index];
    label.setText (name, juce::dontSendNotification);
    label.setFont (displayFont (17.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, brass);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    label.setAccessible (false);
    addAndMakeVisible (label);

    auto& control = sliderControls[index];
    control.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 26);
    control.setTextValueSuffix (decibels ? " dB" : " %");
    control.setName (name);
    control.setTitle (name);
    control.setDescription (description);
    control.setTooltip (description);
    control.setWantsKeyboardFocus (true);
    addAndMakeVisible (control);

    sliderAttachments[index] = std::make_unique<SliderAttachment> (
        audioProcessor.parameters, parameterId, control);
}

void AcustraAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    if (cedarBackground.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (cedarBackground, full,
                     juce::RectanglePlacement::fillDestination);
    }
    else
    {
        juce::ColourGradient background {
            soundboard.brighter (0.14f), 0.0f, 0.0f,
            soundboard.darker (0.18f), full.getWidth(), full.getHeight(), false };
        g.setGradientFill (background);
        g.fillAll();
    }

    g.setColour (darkWood.withAlpha (0.10f));
    g.fillRect (full);
    juce::ColourGradient headerShade {
        ebony.withAlpha (0.88f), 0.0f, 0.0f,
        rosewood.withAlpha (0.72f), full.getWidth(), 86.0f, false };
    g.setGradientFill (headerShade);
    g.fillRect (0.0f, 0.0f, full.getWidth(), 86.0f);

    g.setColour (brass.withAlpha (0.22f));
    g.drawLine (24.0f, 80.0f, static_cast<float> (getWidth() - 24), 80.0f,
                1.0f);

    drawPanel (g, setupPanelBounds);
    drawPanel (g, choicePanelBounds);
    drawPanel (g, tonePanelBounds);

    // A restrained soundboard/sound-hole watermark anchors the six controls
    // visually to the instrument they alter while leaving every label clear.
    const auto watermark = tonePanelBounds.toFloat().reduced (18.0f);
    const auto centre = watermark.getCentre();
    const auto bodyHeight = watermark.getHeight() * 0.72f;
    const auto bodyWidth = bodyHeight * 0.58f;
    juce::Path guitar;
    guitar.startNewSubPath (centre.x, centre.y - bodyHeight * 0.5f);
    guitar.cubicTo (centre.x - bodyWidth * 0.34f,
                    centre.y - bodyHeight * 0.52f,
                    centre.x - bodyWidth * 0.64f,
                    centre.y - bodyHeight * 0.31f,
                    centre.x - bodyWidth * 0.42f,
                    centre.y - bodyHeight * 0.08f);
    guitar.cubicTo (centre.x - bodyWidth * 0.72f,
                    centre.y + bodyHeight * 0.12f,
                    centre.x - bodyWidth * 0.55f,
                    centre.y + bodyHeight * 0.50f,
                    centre.x, centre.y + bodyHeight * 0.50f);
    guitar.cubicTo (centre.x + bodyWidth * 0.55f,
                    centre.y + bodyHeight * 0.50f,
                    centre.x + bodyWidth * 0.72f,
                    centre.y + bodyHeight * 0.12f,
                    centre.x + bodyWidth * 0.42f,
                    centre.y - bodyHeight * 0.08f);
    guitar.cubicTo (centre.x + bodyWidth * 0.64f,
                    centre.y - bodyHeight * 0.31f,
                    centre.x + bodyWidth * 0.34f,
                    centre.y - bodyHeight * 0.52f,
                    centre.x, centre.y - bodyHeight * 0.5f);
    guitar.closeSubPath();
    g.setColour (soundboard.withAlpha (0.025f));
    g.fillPath (guitar);
    g.setColour (brass.withAlpha (0.055f));
    g.strokePath (guitar, juce::PathStrokeType (1.0f));
    g.drawEllipse (centre.x - bodyWidth * 0.12f,
                   centre.y - bodyWidth * 0.12f,
                   bodyWidth * 0.24f, bodyWidth * 0.24f, 1.0f);

    g.setFont (displayFont (15.0f, juce::Font::bold));
    g.setColour (mutedText.withAlpha (0.86f));
    g.drawText ("INSTRUMENT", choicePanelBounds.reduced (15).removeFromTop (18),
                juce::Justification::centredLeft, false);
    g.drawText ("VOICE & OUTPUT", tonePanelBounds.reduced (15).removeFromTop (18),
                juce::Justification::centredLeft, false);
}

void AcustraAudioProcessorEditor::resized()
{
    const auto keyboardHeight = juce::jmax (96, getHeight() / 7);
    keyboardPanelBounds = getLocalBounds().removeFromBottom (keyboardHeight)
                                         .reduced (22, 0);
    keyboard.setKeyWidth (static_cast<float> (keyboardPanelBounds.getWidth())
                          / static_cast<float> (keyboardWhiteKeyCount));
    keyboard.setBounds (keyboardPanelBounds);

    auto bounds = getLocalBounds().reduced (22);
    bounds.setBottom (keyboardPanelBounds.getY() - 12);
    auto header = bounds.removeFromTop (58);
    titleLabel.setBounds (header.removeFromLeft (
        juce::jmin (318, header.getWidth() / 3)));
    subtitleLabel.setBounds (header.removeFromLeft (
        juce::jmin (480, header.getWidth() / 2)).reduced (8, 0));
    panicButton.setBounds (header.removeFromRight (80).reduced (3, 10));
    statusLabel.setBounds (header.reduced (8, 0));

    bounds.removeFromTop (12);
    setupPanelBounds = bounds.removeFromTop (80);
    auto setupArea = setupPanelBounds.reduced (14, 10);
    const auto setupWidth = setupArea.getWidth() - 24;
    const std::array<int, 3> setupWidths {
        setupWidth * 44 / 100, setupWidth * 23 / 100, setupWidth * 33 / 100
    };
    for (std::size_t index = 0; index < setupControls.size(); ++index)
    {
        auto cell = setupArea.removeFromLeft (setupWidths[index]);
        setupLabels[index].setBounds (cell.removeFromTop (24));
        setupControls[index].setBounds (cell);
        setupArea.removeFromLeft (12);
    }
    bounds.removeFromTop (12);
    choicePanelBounds = bounds.removeFromTop (
        juce::jmax (154, bounds.getHeight() * 36 / 100));
    bounds.removeFromTop (12);
    tonePanelBounds = bounds;

    auto choiceArea = choicePanelBounds.reduced (14);
    choiceArea.removeFromTop (20);
    const int choiceGap = 10;
    const int choiceWidth =
        (choiceArea.getWidth() - choiceGap * 3) / 4;
    for (std::size_t index = 0; index < choiceControls.size(); ++index)
    {
        auto cell = choiceArea.removeFromLeft (choiceWidth);
        if (index + 1 < choiceControls.size())
            choiceArea.removeFromLeft (choiceGap);
        choiceLabels[index].setBounds (cell.removeFromTop (30));
        if (choiceControls[index] != nullptr)
            choiceControls[index]->setBounds (cell);
    }

    auto toneArea = tonePanelBounds.reduced (13);
    toneArea.removeFromTop (20);
    const int sliderGap = 3;
    const int sliderWidth =
        (toneArea.getWidth() - sliderGap * 5) / 6;
    for (std::size_t index = 0; index < sliderControls.size(); ++index)
    {
        auto cell = toneArea.removeFromLeft (sliderWidth);
        if (index + 1 < sliderControls.size())
            toneArea.removeFromLeft (sliderGap);
        sliderLabels[index].setBounds (cell.removeFromTop (30));
        sliderControls[index].setBounds (cell);
    }
}

void AcustraAudioProcessorEditor::timerCallback()
{
    updateConstructionControls();
    juce::String next;
    const auto state = audioProcessor.snapshotEngineParameters();
    if (state.capture == acustra::CaptureType::Magnetic
        && state.stringMaterial == acustra::StringMaterial::Nylon)
    {
        next = "Magnetic needs steel";
    }
    else if (! audioProcessor.isEngineReady())
    {
        next = "WAITING FOR AUDIO";
    }
    else
    {
        const auto rate = audioProcessor.getCurrentSampleRateForDisplay();
        const auto voices = audioProcessor.getActiveVoiceCount();
        next = juce::String (rate / 1000.0, 1) + " kHz  |  "
             + juce::String (voices) + (voices == 1 ? " STRING" : " STRINGS");
    }

    if (statusLabel.getText() != next)
    {
        statusLabel.setText (next, juce::dontSendNotification);
        if (auto* handler = statusLabel.getAccessibilityHandler())
            handler->notifyAccessibilityEvent (
                juce::AccessibilityEvent::titleChanged);
    }
}

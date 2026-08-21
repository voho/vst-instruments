#include "PluginEditor.h"

#include <array>

namespace
{
namespace colours
{
    const juce::Colour body { 0xfff2f0ea };        // warm panel white
    const juce::Colour frame { 0xff2b2b2e };       // charcoal frames and text
    const juce::Colour accent { 0xffc7472e };      // signal red-orange
    const juce::Colour knobFace { 0xff313136 };
    const juce::Colour knobPointer { 0xfff5f2ec };
    const juce::Colour sliderTrack { 0xffd8d5cc };
    const juce::Colour ledOn { 0xffe8623d };
    const juce::Colour shadow { 0x33000000 };
}

constexpr int editorWidth = 1252;
constexpr int editorHeight = 664;
constexpr int headerHeight = 56;
constexpr int keyboardHeight = 72;
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

YouKnow201LookAndFeel::YouKnow201LookAndFeel()
{
    setColour (juce::Label::textColourId, colours::frame);
    setColour (juce::Slider::textBoxTextColourId, colours::frame);
    setColour (juce::Slider::textBoxOutlineColourId,
               juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::body);
    setColour (juce::ComboBox::textColourId, colours::frame);
    setColour (juce::ComboBox::outlineColourId, colours::frame);
    setColour (juce::ComboBox::arrowColourId, colours::frame);
    setColour (juce::PopupMenu::backgroundColourId, colours::body);
    setColour (juce::PopupMenu::textColourId, colours::frame);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colours::body);
    setColour (juce::TextButton::textColourOffId, colours::frame);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
               juce::Colour (0xfffaf8f4));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,
               juce::Colour (0xff26262a));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
               colours::accent.withAlpha (0.6f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
               colours::accent.withAlpha (0.25f));
}

void YouKnow201LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPos,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider&)
{
    const auto bounds =
        juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle =
        rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Travel arc.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colours::sliderTrack);
    g.strokePath (arc, juce::PathStrokeType (2.0f));
    juce::Path filled;
    filled.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (colours::accent);
    g.strokePath (filled, juce::PathStrokeType (2.0f));

    // Cap.
    const auto capRadius = radius * 0.78f;
    g.setColour (colours::shadow);
    g.fillEllipse (centre.x - capRadius + 1.0f, centre.y - capRadius + 2.0f,
                   capRadius * 2.0f, capRadius * 2.0f);
    g.setColour (colours::knobFace);
    g.fillEllipse (centre.x - capRadius, centre.y - capRadius,
                   capRadius * 2.0f, capRadius * 2.0f);

    // Pointer.
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -capRadius + 2.0f, 3.2f,
                                 capRadius * 0.62f, 1.4f);
    g.setColour (colours::knobPointer);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle)
                             .translated (centre.x, centre.y));
}

void YouKnow201LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPos, float, float,
                                              juce::Slider::SliderStyle style,
                                              juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          0.0f, 0.0f, style, slider);
        return;
    }

    const auto track = juce::Rectangle<float> (
        (float) x + (float) width * 0.5f - 2.0f, (float) y, 4.0f, (float) height);
    g.setColour (colours::sliderTrack);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (colours::frame.withAlpha (0.35f));
    for (int step = 0; step <= 10; ++step)
    {
        const float lineY = (float) y + (float) height * (float) step / 10.0f;
        g.drawHorizontalLine ((int) lineY, (float) x + 4.0f,
                              (float) (x + width) - 4.0f);
    }

    const auto capWidth = (float) width - 8.0f;
    juce::Rectangle<float> cap (
        (float) x + 4.0f, sliderPos - 7.0f, capWidth, 14.0f);
    g.setColour (colours::shadow);
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.0f);
    g.setColour (colours::knobFace);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (colours::knobPointer);
    g.fillRect (cap.reduced (2.0f, 5.5f));
}

void YouKnow201LookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                  juce::Button& button,
                                                  const juce::Colour&,
                                                  bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    g.setColour (on ? colours::accent
                    : isDown || isHighlighted
                          ? colours::sliderTrack
                          : colours::body);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (colours::frame);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void YouKnow201LookAndFeel::drawComboBox (juce::Graphics& g, int width,
                                          int height, bool, int, int, int, int,
                                          juce::ComboBox& box)
{
    auto bounds =
        juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (colours::body);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : colours::frame);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    juce::Path arrow;
    const auto arrowZone = bounds.removeFromRight (16.0f).reduced (4.0f, 6.0f);
    arrow.addTriangle (arrowZone.getX(), arrowZone.getY(),
                       arrowZone.getRight(), arrowZone.getY(),
                       arrowZone.getCentreX(), arrowZone.getBottom());
    g.setColour (colours::frame);
    g.fillPath (arrow);
}

juce::Font YouKnow201LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

juce::Font YouKnow201LookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions (label.getFont().getHeight()));
}

juce::Font YouKnow201LookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
}

void YouKnow201LookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                  juce::Label& label)
{
    label.setBounds (3, 1, box.getWidth() - 19, box.getHeight() - 2);
    label.setFont (juce::Font (juce::FontOptions (10.0f)));
}

void YouKnow201LookAndFeel::drawButtonText (juce::Graphics& g,
                                            juce::TextButton& button, bool, bool)
{
    g.setFont (juce::Font (juce::FontOptions (
        button.getWidth() < 34 ? 9.5f : 11.0f, juce::Font::bold)));
    g.setColour (button.getToggleState() ? juce::Colours::white
                                         : colours::frame);
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (1),
                juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

YouKnow201AudioProcessorEditor::YouKnow201AudioProcessorEditor (
    YouKnow201AudioProcessor& owner)
    : AudioProcessorEditor (owner), processor (owner)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("YouKnow201", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, colours::frame);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("ten-voice virtual analog synthesizer",
                           juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    subtitleLabel.setColour (juce::Label::textColourId,
                             colours::frame.withAlpha (0.7f));
    addAndMakeVisible (subtitleLabel);

    voiceLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    voiceLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (voiceLabel);

    for (int index = 0; index < processor.getNumPrograms(); ++index)
        programBox.addItem (processor.getProgramName (index), index + 1);
    programBox.setSelectedId (processor.getCurrentProgram() + 1,
                              juce::dontSendNotification);
    programBox.onChange = [this]
    {
        const int index = programBox.getSelectedId() - 1;
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram (index);
    };
    addAndMakeVisible (programBox);

    upperButton.setClickingTogglesState (false);
    lowerButton.setClickingTogglesState (false);
    upperButton.setToggleState (true, juce::dontSendNotification);
    upperButton.onClick = [this]
    {
        editingUpper = true;
        upperButton.setToggleState (true, juce::dontSendNotification);
        lowerButton.setToggleState (false, juce::dontSendNotification);
        bindControls();
    };
    lowerButton.onClick = [this]
    {
        editingUpper = false;
        upperButton.setToggleState (false, juce::dontSendNotification);
        lowerButton.setToggleState (true, juce::dontSendNotification);
        bindControls();
    };
    addAndMakeVisible (upperButton);
    addAndMakeVisible (lowerButton);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "master_level", masterSlider);

    // ---- sections, in the hardware's panel order ------------------------
    const auto section = [this] (const juce::String& title)
    {
        sections.push_back (std::make_unique<Section>());
        sections.back()->title = title;
        return sections.back().get();
    };

    auto* osc1 = section ("OSC 1");
    addControl (*osc1, "osc1_wave", "WAVE", Style::Combo);
    addControl (*osc1, "osc1_pitch", "PITCH", Style::Knob);
    addControl (*osc1, "osc1_detune", "DETUNE", Style::Knob);
    addControl (*osc1, "osc1_pw", "PW/FB", Style::Knob);
    addControl (*osc1, "osc1_wide", "WIDE", Style::Toggle);
    addControl (*osc1, "osc1_penv_depth", "P.ENV", Style::Knob);

    auto* osc2 = section ("OSC 2");
    addControl (*osc2, "osc2_wave", "WAVE", Style::Combo);
    addControl (*osc2, "osc2_pitch", "PITCH", Style::Knob);
    addControl (*osc2, "osc2_detune", "DETUNE", Style::Knob);
    addControl (*osc2, "osc2_pw", "PW/FB", Style::Knob);
    addControl (*osc2, "osc2_wide", "WIDE", Style::Toggle);
    addControl (*osc2, "osc2_penv_depth", "P.ENV", Style::Knob);

    auto* pitchEnv = section ("PITCH ENV");
    addControl (*pitchEnv, "penv_attack", "A", Style::VSlider);
    addControl (*pitchEnv, "penv_decay", "D", Style::VSlider);

    auto* mixMod = section ("MIX/MOD");
    addControl (*mixMod, "mix_type", "TYPE", Style::Combo);
    addControl (*mixMod, "balance", "BAL", Style::Knob);
    addControl (*mixMod, "low_freq", "LOW FREQ", Style::Combo);

    auto* filter = section ("FILTER");
    addControl (*filter, "filter_type", "TYPE", Style::Combo);
    addControl (*filter, "filter_slope", "SLOPE", Style::Combo);
    addControl (*filter, "cutoff", "CUTOFF", Style::Knob);
    addControl (*filter, "resonance", "RESO", Style::Knob);
    addControl (*filter, "key_follow", "KEY FLW", Style::Knob);
    addControl (*filter, "cutoff_vel", "VEL", Style::Knob);

    auto* filterEnv = section ("FILTER ENV");
    addControl (*filterEnv, "fenv_attack", "A", Style::VSlider);
    addControl (*filterEnv, "fenv_decay", "D", Style::VSlider);
    addControl (*filterEnv, "fenv_sustain", "S", Style::VSlider);
    addControl (*filterEnv, "fenv_release", "R", Style::VSlider);
    addControl (*filterEnv, "fenv_depth", "DEPTH", Style::Knob);

    auto* amp = section ("AMP");
    addControl (*amp, "level", "LEVEL", Style::Knob);
    addControl (*amp, "level_vel", "VEL", Style::Knob);
    addControl (*amp, "pan", "PAN", Style::Knob);
    addControl (*amp, "overdrive", "OVERDRIVE", Style::Toggle);
    addControl (*amp, "drive", "DRIVE", Style::Knob);

    auto* ampEnv = section ("AMP ENV");
    addControl (*ampEnv, "aenv_attack", "A", Style::VSlider);
    addControl (*ampEnv, "aenv_decay", "D", Style::VSlider);
    addControl (*ampEnv, "aenv_sustain", "S", Style::VSlider);
    addControl (*ampEnv, "aenv_release", "R", Style::VSlider);

    for (int lfo = 1; lfo <= 2; ++lfo)
    {
        const juce::String prefix = "lfo" + juce::String (lfo) + "_";
        auto* lfoSection = section ("LFO " + juce::String (lfo));
        addControl (*lfoSection, prefix + "shape", "SHAPE", Style::Combo);
        addControl (*lfoSection, prefix + "rate", "RATE", Style::Knob);
        addControl (*lfoSection, prefix + "sync", "SYNC", Style::Toggle);
        addControl (*lfoSection, prefix + "sync_note", "NOTE", Style::Combo);
        addControl (*lfoSection, prefix + "fade", "FADE", Style::Knob);
        addControl (*lfoSection, prefix + "key_trig", "TRIG", Style::Toggle);
        addControl (*lfoSection, prefix + "dest1", "DEST 1", Style::Combo);
        addControl (*lfoSection, prefix + "depth1", "DEP 1", Style::Knob);
        addControl (*lfoSection, prefix + "dest2", "DEST 2", Style::Combo);
        addControl (*lfoSection, prefix + "depth2", "DEP 2", Style::Knob);
    }

    auto* performance = section ("PERFORMANCE");
    addControl (*performance, "mono_mode", "POLY/SOLO", Style::Combo);
    addControl (*performance, "portamento", "PORTA", Style::Toggle);
    addControl (*performance, "porta_time", "TIME", Style::Knob);
    addControl (*performance, "bend_range", "BEND", Style::Knob);
    addControl (*performance, "octave_shift", "OCTAVE", Style::Knob);
    addControl (*performance, "keyboard_mode", "KBD MODE", Style::Combo, false);
    addControl (*performance, "keyboard_part", "PART", Style::Combo, false);
    addControl (*performance, "split_point", "SPLIT", Style::Knob, false);
    addControl (*performance, "mod_assign", "MOD ASGN", Style::Combo, false);

    auto* common = section ("PATCH");
    addControl (*common, "patch_level", "LEVEL", Style::Knob, false);
    addControl (*common, "tone_balance", "BAL", Style::Knob, false);
    addControl (*common, "patch_tempo", "TEMPO", Style::Knob, false);

    auto* delay = section ("DELAY");
    addControl (*delay, "delay_on", "ON", Style::Toggle, false);
    addControl (*delay, "delay_time", "TIME", Style::Knob, false);
    addControl (*delay, "delay_feedback", "FEEDBK", Style::Knob, false);
    addControl (*delay, "delay_hf_damp", "HF DAMP", Style::Combo, false);
    addControl (*delay, "delay_mod_rate", "MOD RATE", Style::Knob, false);
    addControl (*delay, "delay_mod_depth", "MOD DEP", Style::Knob, false);
    addControl (*delay, "delay_depth", "DEPTH", Style::Knob);

    auto* reverb = section ("REVERB");
    addControl (*reverb, "reverb_on", "ON", Style::Toggle, false);
    addControl (*reverb, "reverb_time", "TIME", Style::Knob, false);
    addControl (*reverb, "reverb_size", "SIZE", Style::Knob, false);
    addControl (*reverb, "reverb_depth", "DEPTH", Style::Knob);
    addControl (*reverb, "reverb_pre_delay", "PRE DLY", Style::Knob, false);
    addControl (*reverb, "reverb_high_cut", "HI CUT", Style::Combo, false);
    addControl (*reverb, "reverb_density", "DENS", Style::Knob, false);
    addControl (*reverb, "reverb_diffusion", "DIFF", Style::Knob, false);

    bindControls();

    keyboardState.addListener (this);
    keyboard.setAvailableRange (36, 96);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    setOpaque (true);
    setSize (editorWidth, editorHeight);
    startTimerHz (24);
}

YouKnow201AudioProcessorEditor::~YouKnow201AudioProcessorEditor()
{
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
}

YouKnow201AudioProcessorEditor::Control* YouKnow201AudioProcessorEditor::addControl (
    Section& section, const juce::String& suffix, const juce::String& labelText,
    Style style, bool perTone)
{
    controls.push_back (std::make_unique<Control>());
    auto* control = controls.back().get();
    control->suffix = suffix;
    control->perTone = perTone;
    control->style = style;

    switch (style)
    {
        case Style::Knob:
        {
            auto slider = std::make_unique<juce::Slider> (
                juce::Slider::RotaryHorizontalVerticalDrag,
                juce::Slider::NoTextBox);
            slider->setPopupDisplayEnabled (true, true, this);
            control->component = std::move (slider);
            break;
        }
        case Style::VSlider:
        {
            auto slider = std::make_unique<juce::Slider> (
                juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            slider->setPopupDisplayEnabled (true, true, this);
            control->component = std::move (slider);
            break;
        }
        case Style::Combo:
            control->component = std::make_unique<juce::ComboBox>();
            break;
        case Style::Toggle:
        {
            auto button = std::make_unique<juce::TextButton> ("ON");
            button->setClickingTogglesState (true);
            control->component = std::move (button);
            break;
        }
    }

    {
        control->label = std::make_unique<juce::Label>();
        control->label->setText (labelText, juce::dontSendNotification);
        control->label->setFont (juce::Font (juce::FontOptions (10.5f)));
        control->label->setJustificationType (juce::Justification::centred);
        control->label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*control->label);
    }
    addAndMakeVisible (*control->component);
    section.controls.push_back (control);
    return control;
}

void YouKnow201AudioProcessorEditor::bindControls()
{
    for (auto& control : controls)
    {
        const juce::String id =
            control->perTone
                ? youknow201::parameters::toneId (editingUpper,
                                                  control->suffix.toRawUTF8())
                : control->suffix;

        control->sliderAttachment.reset();
        control->comboAttachment.reset();
        control->buttonAttachment.reset();

        if (processor.parameters.getParameter (id) == nullptr)
        {
            jassertfalse;  // a control names a parameter that does not exist
            continue;
        }

        if (auto* slider = dynamic_cast<juce::Slider*> (control->component.get()))
        {
            control->sliderAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.parameters, id, *slider);
        }
        else if (auto* combo =
                     dynamic_cast<juce::ComboBox*> (control->component.get()))
        {
            // Rebuild the item list from the parameter's choices.
            combo->clear (juce::dontSendNotification);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    processor.parameters.getParameter (id)))
            {
                int itemId = 1;
                for (const auto& name : choice->choices)
                    combo->addItem (name, itemId++);
            }
            control->comboAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                processor.parameters, id, *combo);
        }
        else if (auto* button =
                     dynamic_cast<juce::Button*> (control->component.get()))
        {
            control->buttonAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.parameters, id, *button);
        }
    }
}

void YouKnow201AudioProcessorEditor::layoutSection (Section& section,
                                                    juce::Rectangle<int> bounds)
{
    section.bounds = bounds;
    auto content = bounds.reduced (8, 6);
    content.removeFromTop (16);  // title strip

    // Vertical sliders take full-height cells on the left; everything else
    // fills an explicit one- or two-row grid, each row dividing the width
    // between its own cells so nothing can collide.
    std::vector<Control*> vertical, grid;
    for (auto* control : section.controls)
        (control->style == Style::VSlider ? vertical : grid).push_back (control);

    for (auto* control : vertical)
    {
        auto cell = content.removeFromLeft (32);
        control->label->setBounds (cell.withHeight (12));
        control->component->setBounds (cell.withTrimmedTop (12));
        content.removeFromLeft (2);
    }

    if (grid.empty())
        return;

    const int rows =
        ((int) grid.size() > 3 || content.getHeight() > 150) && grid.size() > 1
            ? 2
            : 1;
    const int firstCount =
        rows == 2 && section.firstRowCount > 0
            ? juce::jmin (section.firstRowCount, (int) grid.size())
            : ((int) grid.size() + rows - 1) / rows;
    const auto weightOf = [] (const Control* control)
    {
        // Text-bearing widgets need more of the row than a knob does.
        return control->style == Style::Combo ? 8
               : control->style == Style::Toggle ? 4
                                                 : 4;
    };
    int index = 0;
    for (int row = 0; row < rows; ++row)
    {
        const int count = row == 0 ? firstCount : (int) grid.size() - firstCount;
        if (count <= 0)
            break;
        const int rowHeight = content.getHeight() / rows;
        const auto rowBounds =
            juce::Rectangle<int> (content.getX(), content.getY() + row * rowHeight,
                                  content.getWidth(), rowHeight);
        int totalWeight = 0;
        for (int i = 0; i < count; ++i)
            totalWeight += weightOf (grid[(std::size_t) (index + i)]);
        int x = rowBounds.getX();
        for (int i = 0; i < count; ++i, ++index)
        {
            auto* control = grid[(std::size_t) index];
            const int cellWidth =
                rowBounds.getWidth() * weightOf (control) / totalWeight;
            const auto cell = juce::Rectangle<int> (x, rowBounds.getY(),
                                                    cellWidth,
                                                    rowBounds.getHeight());
            x += cellWidth;
            switch (control->style)
            {
                case Style::Knob:
                    control->label->setBounds (cell.withHeight (12));
                    control->component->setBounds (
                        cell.withTrimmedTop (12).reduced (2));
                    break;
                case Style::Combo:
                    control->label->setBounds (cell.withHeight (12));
                    control->component->setBounds (cell.withTrimmedTop (16)
                                                       .withHeight (22)
                                                       .reduced (2, 0));
                    break;
                case Style::Toggle:
                    control->label->setBounds (cell.withHeight (12));
                    control->component->setBounds (
                        cell.withTrimmedTop (16)
                            .withSizeKeepingCentre (
                                juce::jmin (cell.getWidth() - 4, 56), 24));
                    break;
                case Style::VSlider:
                    break;
            }
        }
    }
}

void YouKnow201AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop (headerHeight).reduced (10, 6);
    titleLabel.setBounds (header.removeFromLeft (170));
    subtitleLabel.setBounds (header.removeFromLeft (220));
    voiceLabel.setBounds (header.removeFromRight (110));
    masterSlider.setBounds (header.removeFromRight (46));
    programBox.setBounds (header.removeFromLeft (200).withHeight (26)
                              .withY (header.getY() + 10));
    upperButton.setBounds (header.removeFromLeft (76).withHeight (26)
                               .withY (header.getY() + 10).reduced (2, 0));
    lowerButton.setBounds (header.removeFromLeft (76).withHeight (26)
                               .withY (header.getY() + 10).reduced (2, 0));

    auto keyboardBounds = bounds.removeFromBottom (keyboardHeight).reduced (10, 4);
    // 36..96 spans 36 white keys; size them to fill the panel width.
    keyboard.setKeyWidth ((float) keyboardBounds.getWidth() / 36.0f);
    keyboard.setBounds (keyboardBounds);

    auto panel = bounds.reduced (8, 4);
    const int rowHeight = panel.getHeight() / 2;
    auto topRow = panel.removeFromTop (rowHeight);
    auto bottomRow = panel;

    // Row 1: OSC1, OSC2, PITCH ENV, MIX/MOD, FILTER, FILTER ENV, AMP, AMP ENV
    const std::array<std::pair<int, int>, 8> topPlan {
        std::pair { 0, 190 }, { 1, 190 }, { 2, 84 }, { 3, 130 },
        { 4, 190 }, { 5, 220 }, { 6, 130 }, { 7, 160 },
    };
    int consumed = 0;
    for (const auto& [index, width] : topPlan)
        consumed += width;
    const float topScale = (float) topRow.getWidth() / (float) consumed;
    for (const auto& [index, width] : topPlan)
        layoutSection (*sections[(std::size_t) index],
                       topRow.removeFromLeft ((int) (width * topScale)));

    // Row 2: LFO1, LFO2, PERFORMANCE, PATCH, DELAY, REVERB
    const std::array<std::pair<int, int>, 6> bottomPlan {
        std::pair { 8, 226 }, { 9, 226 }, { 10, 262 }, { 11, 150 },
        { 12, 218 }, { 13, 218 },
    };
    consumed = 0;
    for (const auto& [index, width] : bottomPlan)
        consumed += width;
    const float bottomScale = (float) bottomRow.getWidth() / (float) consumed;
    for (const auto& [index, width] : bottomPlan)
        layoutSection (*sections[(std::size_t) index],
                       bottomRow.removeFromLeft ((int) (width * bottomScale)));
}

void YouKnow201AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::body);

    // Header rule and accent stripe, echoing the hardware's banded fascia.
    g.setColour (colours::frame);
    g.fillRect (0, headerHeight - 3, getWidth(), 2);
    g.setColour (colours::accent);
    g.fillRect (0, headerHeight - 6, getWidth(), 2);

    for (const auto& section : sections)
    {
        auto bounds = section->bounds.reduced (3);
        g.setColour (colours::frame.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);
        g.setColour (colours::frame);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (section->title,
                    bounds.withHeight (16).reduced (8, 0),
                    juce::Justification::centredLeft);
    }

    // Output meter beside the voice count.
    const auto meter = juce::Rectangle<int> (getWidth() - 130, 12, 8, 30);
    for (int channel = 0; channel < 2; ++channel)
    {
        auto bar = meter.translated (channel * 12, 0);
        g.setColour (colours::sliderTrack);
        g.fillRect (bar);
        const float level = juce::jlimit (0.0f, 1.0f, meterLevel[channel]);
        g.setColour (colours::ledOn);
        g.fillRect (bar.removeFromBottom ((int) (level * 30.0f)));
    }
}

void YouKnow201AudioProcessorEditor::timerCallback()
{
    meterLevel[0] = processor.getOutputLevel (0);
    meterLevel[1] = processor.getOutputLevel (1);
    voiceLabel.setText (juce::String (processor.getActiveVoiceCount())
                            + " / 10 voices",
                        juce::dontSendNotification);
    programBox.setSelectedId (processor.getCurrentProgram() + 1,
                              juce::dontSendNotification);
    repaint (getWidth() - 140, 0, 140, headerHeight);
}

void YouKnow201AudioProcessorEditor::handleNoteOn (juce::MidiKeyboardState*, int,
                                                   int note, float velocity)
{
    processor.triggerFromUi (note,
                             juce::jlimit (1, 127, (int) (velocity * 127.0f)));
}

void YouKnow201AudioProcessorEditor::handleNoteOff (juce::MidiKeyboardState*,
                                                    int, int note, float)
{
    processor.releaseFromUi (note);
}

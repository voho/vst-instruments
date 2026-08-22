#include "PluginEditor.h"

#include <array>

namespace
{
namespace colours
{
    const juce::Colour body { 0xfff2f0ea };        // warm panel white
    const juce::Colour recess { 0xffe9e6de };      // the section wells
    const juce::Colour frame { 0xff2b2b2e };       // charcoal frames and text
    const juce::Colour accent { 0xffc7472e };      // signal red-orange
    const juce::Colour knobFace { 0xff313136 };
    const juce::Colour knobPointer { 0xfff5f2ec };
    const juce::Colour sliderTrack { 0xffd8d5cc };
    const juce::Colour ledOn { 0xffe8623d };
    const juce::Colour shadow { 0x33000000 };
    // One muted tint per band. They only ever appear as a two-pixel rule
    // above a section title: enough to group the panel, not enough to turn
    // it into a chart.
    const juce::Colour bandVoice { 0xffb4552f };
    const juce::Colour bandModulation { 0xff3f6f77 };
    const juce::Colour bandEffects { 0xff6b6a3a };
    const juce::Colour bandPerform { 0xff55555c };
}

// Fixed control geometry. Sections are sized to fit their contents; the
// contents are never scaled to fit a section, which is what keeps a knob the
// same size wherever it appears.
constexpr int knobCell = 58;
constexpr int comboCell = 104;
constexpr int toggleCell = 66;
constexpr int actionCell = 60;
constexpr int sliderCell = 34;

constexpr int labelHeight = 13;
constexpr int valueHeight = 13;
constexpr int knobDiameter = 40;
constexpr int comboHeight = 22;
constexpr int toggleHeight = 22;

constexpr int sectionTitleHeight = 20;
constexpr int sectionPadding = 7;
constexpr int gridRowHeight = 68;      // label + control + value
constexpr int sectionGap = 6;
constexpr int chevronGap = 16;

constexpr int headerHeight = 48;
constexpr int clusterWidth = 128;
// Tall enough for a full-size control cell under the strip's own title,
// so the patch strip's knobs are the same knobs as everywhere else.
constexpr int stripHeight = sectionTitleHeight + gridRowHeight
                            + 2 * sectionPadding + 6;
constexpr int keyboardHeight = 90;
constexpr int bandRows = 3;
constexpr int bandHeight = sectionTitleHeight + 2 * gridRowHeight + 2 * sectionPadding;
constexpr int editorWidth = 1500;
constexpr int editorHeight = headerHeight + bandRows * bandHeight
                             + (bandRows - 1) * sectionGap + stripHeight
                             + keyboardHeight + 18;
} // namespace

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------

SeptumLookAndFeel::SeptumLookAndFeel()
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

void SeptumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPos,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const bool bipolar = static_cast<bool> (slider.getProperties()["bipolar"]);
    const auto bounds =
        juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle =
        rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Travel arc. A control whose range straddles zero lights from the top of
    // its travel, where its zero is, rather than from the left end: a BALANCE
    // or a PAN at the centre is not half on, and an arc lit from the end says
    // it is.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colours::sliderTrack);
    g.strokePath (arc, juce::PathStrokeType (2.0f));
    const float origin = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f
                                 : rotaryStartAngle;
    if (std::abs (angle - origin) > 1.0e-3f)
    {
        juce::Path filled;
        filled.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                              juce::jmin (origin, angle),
                              juce::jmax (origin, angle), true);
        g.setColour (colours::accent);
        g.strokePath (filled, juce::PathStrokeType (2.0f));
    }

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

void SeptumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
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

void SeptumLookAndFeel::drawButtonBackground (juce::Graphics& g,
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

void SeptumLookAndFeel::drawComboBox (juce::Graphics& g, int width,
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

juce::Font SeptumLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

juce::Font SeptumLookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions (label.getFont().getHeight()));
}

juce::Font SeptumLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
}

void SeptumLookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                  juce::Label& label)
{
    label.setBounds (3, 1, box.getWidth() - 19, box.getHeight() - 2);
    label.setFont (juce::Font (juce::FontOptions (10.0f)));
}

void SeptumLookAndFeel::drawButtonText (juce::Graphics& g,
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
// Bend/modulation lever
// ---------------------------------------------------------------------------

SeptumLever::SeptumLever (SeptumAudioProcessor& owner)
    : processor (owner)
{
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
}

void SeptumLever::push() noexcept
{
    processor.setLeverFromUi (bend, mod);
}

void SeptumLever::applyFromPoint (juce::Point<float> position)
{
    const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    bend = juce::jlimit (-1.0f, 1.0f,
                         (position.x - bounds.getCentreX())
                             / (bounds.getWidth() * 0.5f));
    mod = juce::jlimit (0.0f, 1.0f,
                        (bounds.getBottom() - position.y) / bounds.getHeight());
    push();
    repaint();
}

void SeptumLever::mouseDown (const juce::MouseEvent& event)
{
    applyFromPoint (event.position);
}

void SeptumLever::mouseDrag (const juce::MouseEvent& event)
{
    applyFromPoint (event.position);
}

void SeptumLever::mouseUp (const juce::MouseEvent&)
{
    bend = 0.0f;  // the lever's bend axis is spring-loaded
    push();
    repaint();
}

void SeptumLever::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (colours::sliderTrack);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::frame);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    // Modulation depth fills from the bottom.
    auto modZone = bounds.reduced (5.0f);
    g.setColour (colours::accent.withAlpha (0.25f));
    g.fillRoundedRectangle (modZone.removeFromBottom (modZone.getHeight() * mod),
                            3.0f);

    // The lever itself, offset by the bend.
    const float centreX = bounds.getCentreX()
                          + bend * (bounds.getWidth() * 0.5f - 8.0f);
    juce::Rectangle<float> stick (centreX - 5.0f, bounds.getY() + 8.0f, 10.0f,
                                  bounds.getHeight() - 16.0f);
    g.setColour (colours::shadow);
    g.fillRoundedRectangle (stick.translated (1.0f, 1.5f), 4.0f);
    g.setColour (colours::knobFace);
    g.fillRoundedRectangle (stick, 4.0f);
    g.setColour (colours::knobPointer);
    g.fillRoundedRectangle (stick.reduced (3.0f, 14.0f), 2.0f);

    g.setColour (colours::frame);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("BEND / MOD", getLocalBounds().removeFromBottom (12),
                juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

SeptumAudioProcessorEditor::SeptumAudioProcessorEditor (
    SeptumAudioProcessor& owner)
    : AudioProcessorEditor (owner), processor (owner), lever (owner)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("Septum", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, colours::frame);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText (
        juce::String::fromUTF8 ("ten-voice virtual analog synthesizer   \xc2\xb7   "
                                "UPPER / LOWER   \xc2\xb7   arpeggiator   \xc2\xb7"
                                "   external in"),
        juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    subtitleLabel.setColour (juce::Label::textColourId,
                             colours::frame.withAlpha (0.7f));
    addAndMakeVisible (subtitleLabel);

    voiceLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    voiceLabel.setJustificationType (juce::Justification::centred);
    voiceLabel.setColour (juce::Label::textColourId,
                          colours::frame.withAlpha (0.72f));
    voiceLabel.setText ("0 / 10 VOICES", juce::dontSendNotification);
    addAndMakeVisible (voiceLabel);

    const auto section = [this] (const juce::String& title, Band band)
    {
        sections.push_back (std::make_unique<Section>());
        sections.back()->title = title;
        sections.back()->band = band;
        return sections.back().get();
    };

    // ---- left performance cluster (hardware's left panel block) ---------
    performSection = section ("PERFORM", Band::Perform);
    performSection->manualLayout = true;

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "master_level", masterSlider);
    masterLabel.setText ("MASTER VOL", juce::dontSendNotification);
    masterLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.setColour (juce::Label::textColourId,
                           colours::frame.withAlpha (0.72f));
    addAndMakeVisible (masterLabel);
    masterValueLabel.setFont (
        juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    masterValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (masterValueLabel);

    octLabel.setText ("KEYBOARD OCTAVE", juce::dontSendNotification);
    octLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    octLabel.setColour (juce::Label::textColourId,
                        colours::frame.withAlpha (0.72f));
    octLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (octLabel);
    octValueLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    octValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (octValueLabel);
    octDownButton.onClick = [this]
    {
        keyboardOctaveShift = juce::jmax (-2, keyboardOctaveShift - 1);
        applyKeyboardOctave();
    };
    octUpButton.onClick = [this]
    {
        keyboardOctaveShift = juce::jmin (2, keyboardOctaveShift + 1);
        applyKeyboardOctave();
    };
    addAndMakeVisible (octDownButton);
    addAndMakeVisible (octUpButton);

    portaControl = addControl (*performSection, "portamento", "PORTAMENTO",
                               Style::Toggle);
    portaTimeControl = addControl (*performSection, "porta_time", "GLIDE TIME",
                                   Style::Knob);
    soloControl = addControl (*performSection, "mono_mode", "KEY ASSIGN",
                              Style::Combo);
    tempoControl = addControl (*performSection, "patch_tempo", "TEMPO",
                               Style::Knob, false, " BPM");

    // ---- band 1: the voice chain -----------------------------------------
    auto* osc1 = section ("OSC 1", Band::Voice);
    osc1->rowCounts = { 4, 2 };
    addControl (*osc1, "osc1_wave", "WAVE", Style::Combo);
    addControl (*osc1, "osc1_pitch", "PITCH", Style::Knob, true, " st");
    addControl (*osc1, "osc1_detune", "DETUNE", Style::Knob, true, " c");
    addControl (*osc1, "osc1_pw", "PW/FB", Style::Knob);
    addControl (*osc1, "osc1_wide", "WIDE", Style::Toggle);
    addControl (*osc1, "osc1_penv_depth", "P.ENV", Style::Knob);

    auto* osc2 = section ("OSC 2", Band::Voice);
    osc2->rowCounts = { 4, 4 };
    addControl (*osc2, "osc2_wave", "WAVE", Style::Combo);
    addControl (*osc2, "osc2_pitch", "PITCH", Style::Knob, true, " st");
    addControl (*osc2, "osc2_detune", "DETUNE", Style::Knob, true, " c");
    addControl (*osc2, "osc2_pw", "PW/FB", Style::Knob);
    intervalOctControl = addControl (*osc2, "", "INTERVAL", Style::Action);
    intervalFifthControl = addControl (*osc2, "", "", Style::Action);
    addControl (*osc2, "osc2_wide", "WIDE", Style::Toggle);
    addControl (*osc2, "osc2_penv_depth", "P.ENV", Style::Knob);

    // Settled (OM p. 30), and settled as *intervals*: "-OCT ... lowers the
    // OSC 2 pitch one octave below that of OSC 1", "the OSC 2 pitch will be
    // seven semitones (a perfect fifth) higher than OSC 1", and "if you press
    // the -OCT button and the 5th button simultaneously, the OSC 2 pitch will
    // be the same as the OSC 1 pitch". Both are measured against OSC 1, so a
    // patch whose OSC 1 is transposed used to get the wrong interval; and the
    // second press stands in for the hardware's simultaneous press, which is
    // why it lands on OSC 1's pitch rather than on zero.
    const auto interval = [this] (Control* control, const char* text,
                                  int semitones)
    {
        auto* button =
            dynamic_cast<juce::TextButton*> (control->component.get());
        if (button == nullptr)
            return;
        button->setButtonText (text);
        button->setClickingTogglesState (false);
        button->onClick = [this, semitones]
        {
            const float root = getToneParameter ("osc1_pitch");
            const float wanted = root + (float) semitones;
            setToneParameter ("osc2_pitch",
                              getToneParameter ("osc2_pitch") == wanted ? root
                                                                        : wanted);
        };
    };
    interval (intervalOctControl, "-OCT", -12);
    interval (intervalFifthControl, "5TH", 7);

    auto* mixMod = section ("MIX/MOD", Band::Voice);
    mixMod->rowCounts = { 2, 1 };
    addControl (*mixMod, "mix_type", "TYPE", Style::Combo);
    addControl (*mixMod, "balance", "BALANCE", Style::Knob);
    addControl (*mixMod, "low_freq", "LOW FREQ", Style::Combo);

    auto* filter = section ("FILTER", Band::Voice);
    filter->rowCounts = { 2, 4 };  // TYPE and SLOPE up top, the knobs below
    addControl (*filter, "filter_type", "TYPE", Style::Combo);
    addControl (*filter, "filter_slope", "SLOPE", Style::Combo);
    addControl (*filter, "cutoff", "CUTOFF", Style::Knob);
    addControl (*filter, "resonance", "RESO", Style::Knob);
    addControl (*filter, "key_follow", "KEY FOLLOW", Style::Knob);
    addControl (*filter, "cutoff_vel", "VELOCITY", Style::Knob);

    auto* amp = section ("AMP", Band::Voice);
    amp->rowCounts = { 3, 2 };
    addControl (*amp, "level", "LEVEL", Style::Knob);
    addControl (*amp, "level_vel", "VELOCITY", Style::Knob);
    addControl (*amp, "pan", "PAN", Style::Knob);
    addControl (*amp, "overdrive", "OVERDRIVE", Style::Toggle);
    addControl (*amp, "drive", "DRIVE", Style::Knob);

    // ---- band 2: what modulates the chain --------------------------------
    auto* pitchEnv = section ("PITCH ENV", Band::Modulation);
    addControl (*pitchEnv, "penv_attack", "A", Style::VSlider);
    addControl (*pitchEnv, "penv_decay", "D", Style::VSlider);

    auto* filterEnv = section ("FILTER ENV", Band::Modulation);
    filterEnv->rowCounts = { 1 };
    addControl (*filterEnv, "fenv_attack", "A", Style::VSlider);
    addControl (*filterEnv, "fenv_decay", "D", Style::VSlider);
    addControl (*filterEnv, "fenv_sustain", "S", Style::VSlider);
    addControl (*filterEnv, "fenv_release", "R", Style::VSlider);
    addControl (*filterEnv, "fenv_depth", "DEPTH", Style::Knob);

    auto* ampEnv = section ("AMP ENV", Band::Modulation);
    addControl (*ampEnv, "aenv_attack", "A", Style::VSlider);
    addControl (*ampEnv, "aenv_decay", "D", Style::VSlider);
    addControl (*ampEnv, "aenv_sustain", "S", Style::VSlider);
    addControl (*ampEnv, "aenv_release", "R", Style::VSlider);

    for (int lfo = 1; lfo <= 2; ++lfo)
    {
        const juce::String prefix = "lfo" + juce::String (lfo) + "_";
        auto* lfoSection = section ("LFO " + juce::String (lfo), Band::Modulation);
        lfoSection->rowCounts = { 5, 5 };
        addControl (*lfoSection, prefix + "shape", "SHAPE", Style::Combo);
        addControl (*lfoSection, prefix + "rate", "RATE", Style::Knob);
        addControl (*lfoSection, prefix + "sync", "SYNC", Style::Toggle);
        addControl (*lfoSection, prefix + "sync_note", "NOTE", Style::Combo);
        addControl (*lfoSection, prefix + "fade", "FADE", Style::Knob);
        addControl (*lfoSection, prefix + "key_trig", "TRIG", Style::Toggle);
        addControl (*lfoSection, prefix + "dest1", "DEST 1", Style::Combo);
        addControl (*lfoSection, prefix + "depth1", "DEPTH 1", Style::Knob);
        addControl (*lfoSection, prefix + "dest2", "DEST 2", Style::Combo);
        addControl (*lfoSection, prefix + "depth2", "DEPTH 2", Style::Knob);
    }

    // ---- band 3: the two ends of the instrument --------------------------
    auto* arpSection = section ("ARPEGGIO", Band::InputEffects);
    // Row one is what the arpeggiator is and where it plays; row two is how
    // it reads the keys. The counts have to cover every control the section
    // holds — a row short and the last one is never given bounds.
    arpSection->rowCounts = { 5, 6 };
    addControl (*arpSection, "arp_on", "SWITCH", Style::Toggle, false);
    addControl (*arpSection, "arp_hold", "HOLD", Style::Toggle, false);
    addControl (*arpSection, "arp_style", "STYLE", Style::Combo, false);
    addControl (*arpSection, "arp_grid", "GRID", Style::Combo, false);
    addControl (*arpSection, "arp_split", "SPLIT ARP", Style::Combo, false);
    addControl (*arpSection, "arp_motif", "MOTIF", Style::Combo, false);
    addControl (*arpSection, "arp_duration", "DURATION", Style::Combo, false);
    addControl (*arpSection, "arp_end_step", "END STEP", Style::Knob, false);
    addControl (*arpSection, "arp_octave", "OCT RANGE", Style::Knob, false);
    addControl (*arpSection, "arp_accent", "ACCENT", Style::Knob, false, " %");
    addControl (*arpSection, "arp_velocity", "VELOCITY", Style::Knob, false);

    auto* externalSection = section ("EXT IN", Band::InputEffects);
    externalSection->rowCounts = { 4, 3 };
    addControl (*externalSection, "ext_input_vol", "INPUT VOL", Style::Knob, false);
    addControl (*externalSection, "ext_center_cancel", "CENTER", Style::Toggle,
                false);
    addControl (*externalSection, "audio_filter_on", "FILTER", Style::Toggle,
                false);
    addControl (*externalSection, "audio_filter_type", "TYPE", Style::Combo, false);
    addControl (*externalSection, "audio_filter_slope", "SLOPE", Style::Combo,
                false);
    addControl (*externalSection, "audio_filter_cutoff", "CUTOFF", Style::Knob,
                false);
    addControl (*externalSection, "audio_filter_reso", "RESO", Style::Knob, false);

    auto* delay = section ("DELAY", Band::InputEffects);
    delay->rowCounts = { 4, 3 };
    addControl (*delay, "delay_on", "SWITCH", Style::Toggle, false);
    addControl (*delay, "delay_time", "TIME", Style::Knob, false);
    addControl (*delay, "delay_depth", "DEPTH", Style::Knob);
    addControl (*delay, "delay_feedback", "FEEDBACK", Style::Knob, false, " %");
    addControl (*delay, "delay_hf_damp", "HF DAMP", Style::Combo, false);
    addControl (*delay, "delay_mod_rate", "MOD RATE", Style::Knob, false);
    addControl (*delay, "delay_mod_depth", "MOD DEPTH", Style::Knob, false);

    auto* reverb = section ("REVERB", Band::InputEffects);
    reverb->rowCounts = { 4, 4 };
    addControl (*reverb, "reverb_on", "SWITCH", Style::Toggle, false);
    addControl (*reverb, "reverb_time", "TIME", Style::Knob, false);
    addControl (*reverb, "reverb_depth", "DEPTH", Style::Knob);
    addControl (*reverb, "reverb_size", "SIZE", Style::Knob, false);
    addControl (*reverb, "reverb_pre_delay", "PRE DELAY", Style::Knob, false,
                " ms");
    addControl (*reverb, "reverb_high_cut", "HIGH CUT", Style::Combo, false);
    addControl (*reverb, "reverb_density", "DENSITY", Style::Knob, false);
    addControl (*reverb, "reverb_diffusion", "DIFFUSION", Style::Knob, false);

    // ---- the patch strip above the keys (the hardware's button row) ------
    stripSection = section ("PATCH", Band::Perform);
    stripSection->manualLayout = true;

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

    lowerButton.setClickingTogglesState (false);
    upperButton.setClickingTogglesState (false);
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
    addAndMakeVisible (lowerButton);
    addAndMakeVisible (upperButton);

    addControl (*stripSection, "keyboard_mode", "KEYBOARD", Style::Combo, false);
    addControl (*stripSection, "keyboard_part", "PART", Style::Combo, false);
    addControl (*stripSection, "split_point", "SPLIT", Style::Knob, false);
    addControl (*stripSection, "mod_assign", "MOD ASSIGN", Style::Combo, false);
    addControl (*stripSection, "bend_range", "BEND", Style::Knob, true, " st");
    addControl (*stripSection, "octave_shift", "TONE OCT", Style::Knob);
    addControl (*stripSection, "patch_level", "LEVEL", Style::Knob, false);
    addControl (*stripSection, "tone_balance", "TONE BAL", Style::Knob, false);

    bindControls();

    keyboardState.addListener (this);
    keyboard.setOctaveForMiddleC (4);
    applyKeyboardOctave();
    addAndMakeVisible (keyboard);
    addAndMakeVisible (lever);

    refreshValues();
    setOpaque (true);
    setSize (editorWidth, editorHeight);
    startTimerHz (24);
}

SeptumAudioProcessorEditor::~SeptumAudioProcessorEditor()
{
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
}

void SeptumAudioProcessorEditor::applyKeyboardOctave()
{
    const int low = juce::jlimit (12, 72, 36 + keyboardOctaveShift * 12);
    keyboard.setAvailableRange (low, low + 60);
    octValueLabel.setText (
        keyboardOctaveShift == 0
            ? juce::String ("0")
            : (keyboardOctaveShift > 0 ? "+" : "")
                  + juce::String (keyboardOctaveShift),
        juce::dontSendNotification);
    octDownButton.setToggleState (keyboardOctaveShift < 0,
                                  juce::dontSendNotification);
    octUpButton.setToggleState (keyboardOctaveShift > 0,
                                juce::dontSendNotification);
}

float SeptumAudioProcessorEditor::getToneParameter (const char* suffix) const
{
    const auto id = septum::parameters::toneId (editingUpper, suffix);
    if (const auto* value = processor.parameters.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

void SeptumAudioProcessorEditor::setToneParameter (const char* suffix,
                                                       float natural)
{
    const auto id = septum::parameters::toneId (editingUpper, suffix);
    if (auto* parameter = processor.parameters.getParameter (id))
    {
        const auto& range = processor.parameters.getParameterRange (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (
            range.convertTo0to1 (range.snapToLegalValue (natural)));
        parameter->endChangeGesture();
    }
}

int SeptumAudioProcessorEditor::Section::naturalWidth() const
{
    const auto cellWidth = [] (Style style)
    {
        switch (style)
        {
            case Style::Knob:    return knobCell;
            case Style::Combo:   return comboCell;
            case Style::Toggle:  return toggleCell;
            case Style::Action:  return actionCell;
            case Style::VSlider: return sliderCell;
        }
        return knobCell;
    };

    int sliders = 0, gridWidth = 0;
    std::vector<const Control*> grid;
    for (const auto* control : controls)
    {
        if (control->style == Style::VSlider)
            sliders += sliderCell;
        else
            grid.push_back (control);
    }

    std::size_t index = 0;
    for (int count : rowCounts)
    {
        int width = 0;
        for (int i = 0; i < count && index < grid.size(); ++i, ++index)
            width += cellWidth (grid[index]->style);
        gridWidth = juce::jmax (gridWidth, width);
    }
    int remainder = 0;
    for (; index < grid.size(); ++index)
        remainder += cellWidth (grid[index]->style);
    gridWidth = juce::jmax (gridWidth, remainder);

    // The title must fit too, or a section can end up narrower than its name.
    const int titleWidth =
        (int) juce::GlyphArrangement::getStringWidth (
            juce::Font (juce::FontOptions (11.5f, juce::Font::bold)), title)
        + 26;
    return juce::jmax (titleWidth, sliders + gridWidth) + 2 * sectionPadding;
}

SeptumAudioProcessorEditor::Control* SeptumAudioProcessorEditor::addControl (
    Section& section, const juce::String& suffix, const juce::String& labelText,
    Style style, bool perTone, const juce::String& unit)
{
    controls.push_back (std::make_unique<Control>());
    auto* control = controls.back().get();
    control->suffix = suffix;
    control->perTone = perTone;
    control->style = style;
    control->unit = unit;

    switch (style)
    {
        case Style::Knob:
        {
            // No drag-time popup: the value is on the panel all the time,
            // so a bubble would only cover the neighbours.
            control->component = std::make_unique<juce::Slider> (
                juce::Slider::RotaryHorizontalVerticalDrag,
                juce::Slider::NoTextBox);
            break;
        }
        case Style::VSlider:
        {
            control->component = std::make_unique<juce::Slider> (
                juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            break;
        }
        case Style::Combo:
            control->component = std::make_unique<juce::ComboBox>();
            break;
        case Style::Toggle:
        {
            auto button = std::make_unique<juce::TextButton> ("OFF");
            button->setClickingTogglesState (true);
            // A switch says which way it is thrown. A button whose face reads
            // ON while the thing is off is the commonest misreading a
            // synthesizer panel invites, and eleven controls here are
            // toggles. Driven from the button's own state rather than the
            // frame timer, so it is right the moment a patch loads.
            auto* raw = button.get();
            raw->onStateChange = [raw]
            {
                raw->setButtonText (raw->getToggleState() ? "ON" : "OFF");
            };
            control->component = std::move (button);
            break;
        }
        case Style::Action:
            control->component = std::make_unique<juce::TextButton> (labelText);
            break;
    }

    control->label = std::make_unique<juce::Label>();
    control->label->setText (labelText, juce::dontSendNotification);
    control->label->setFont (juce::Font (juce::FontOptions (10.0f)));
    control->label->setJustificationType (juce::Justification::centred);
    control->label->setInterceptsMouseClicks (false, false);
    control->label->setColour (juce::Label::textColourId,
                               colours::frame.withAlpha (0.72f));
    addAndMakeVisible (*control->label);

    // Every continuous control reads out its value, so nothing on the panel
    // has to be dragged to be understood.
    if (style == Style::Knob || style == Style::VSlider)
    {
        control->value = std::make_unique<juce::Label>();
        control->value->setFont (
            juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        control->value->setJustificationType (juce::Justification::centred);
        control->value->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*control->value);
    }

    addAndMakeVisible (*control->component);
    section.controls.push_back (control);
    return control;
}

// Reads each control's own parameter text, so the panel prints what the host
// prints and neither can drift from the other.
void SeptumAudioProcessorEditor::refreshValues()
{
    for (auto& control : controls)
    {
        if (control->value == nullptr || control->suffix.isEmpty())
            continue;
        const juce::String id =
            control->perTone
                ? septum::parameters::toneId (editingUpper,
                                              control->suffix.toRawUTF8())
                : control->suffix;
        auto* parameter = processor.parameters.getParameter (id);
        if (parameter == nullptr)
            continue;
        control->value->setText (
            parameter->getCurrentValueAsText() + control->unit,
            juce::dontSendNotification);
    }
    if (masterValueLabel.isVisible())
        if (auto* parameter = processor.parameters.getParameter ("master_level"))
            masterValueLabel.setText (parameter->getCurrentValueAsText(),
                                      juce::dontSendNotification);

    // The INTERVAL buttons carry indicator lamps on the instrument, and they
    // are the panel's only controls that write a parameter without reflecting
    // it: a patch loaded at OSC 2 = OSC 1 - 12 used to show two dark buttons.
    const float root = getToneParameter ("osc1_pitch");
    const float second = getToneParameter ("osc2_pitch");
    const auto lamp = [] (Control* control, bool on)
    {
        if (control == nullptr)
            return;
        if (auto* button = dynamic_cast<juce::Button*> (control->component.get()))
            button->setToggleState (on, juce::dontSendNotification);
    };
    lamp (intervalOctControl, second == root - 12.0f);
    lamp (intervalFifthControl, second == root + 7.0f);

}

void SeptumAudioProcessorEditor::bindControls()
{
    for (auto& control : controls)
    {
        if (control->style == Style::Action || control->suffix.isEmpty())
            continue;

        const juce::String id =
            control->perTone
                ? septum::parameters::toneId (editingUpper,
                                                  control->suffix.toRawUTF8())
                : control->suffix;

        control->sliderAttachment.reset();
        control->comboAttachment.reset();
        control->buttonAttachment.reset();

        auto* parameter = processor.parameters.getParameter (id);
        if (parameter == nullptr)
        {
            jassertfalse;  // a control names a parameter that does not exist
            continue;
        }
        if (auto* tooltipClient =
                dynamic_cast<juce::SettableTooltipClient*> (control->component.get()))
            tooltipClient->setTooltip (parameter->getName (64));

        if (auto* slider = dynamic_cast<juce::Slider*> (control->component.get()))
        {
            // Read straight off the parameter's own range, so a control the
            // manual prints with a sign can never disagree with the way its
            // arc is lit.
            const auto& range = processor.parameters.getParameterRange (id);
            slider->getProperties().set (
                "bipolar", range.start < 0.0f && range.end > 0.0f);
            control->sliderAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.parameters, id, *slider);
        }
        else if (auto* combo =
                     dynamic_cast<juce::ComboBox*> (control->component.get()))
        {
            combo->clear (juce::dontSendNotification);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
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
    refreshValues();
}

void SeptumAudioProcessorEditor::layoutSection (Section& section,
                                               juce::Rectangle<int> bounds)
{
    section.bounds = bounds;
    if (section.manualLayout)
        return;

    auto content = bounds.reduced (sectionPadding, sectionPadding);
    content.removeFromTop (sectionTitleHeight);

    std::vector<Control*> sliders, grid;
    for (auto* control : section.controls)
        (control->style == Style::VSlider ? sliders : grid).push_back (control);

    // Vertical sliders take a left column strip at full content height: label
    // on top, travel in the middle, value underneath.
    if (! sliders.empty())
    {
        auto strip = content.removeFromLeft ((int) sliders.size() * sliderCell);
        for (std::size_t i = 0; i < sliders.size(); ++i)
        {
            auto cell = strip.removeFromLeft (sliderCell);
            sliders[i]->label->setBounds (cell.removeFromTop (labelHeight));
            sliders[i]->value->setBounds (cell.removeFromBottom (valueHeight));
            sliders[i]->component->setBounds (cell.reduced (2, 2));
        }
    }

    const auto cellWidth = [] (Style style)
    {
        switch (style)
        {
            case Style::Knob:    return knobCell;
            case Style::Combo:   return comboCell;
            case Style::Toggle:  return toggleCell;
            case Style::Action:  return actionCell;
            case Style::VSlider: return sliderCell;
        }
        return knobCell;
    };

    // Rows are declared per section, so a section is as wide as it needs to
    // be and no control is ever squeezed to make one fit.
    std::vector<int> rows = section.rowCounts;
    if (rows.empty())
        rows.push_back ((int) grid.size());

    std::size_t index = 0;
    int rowTop = content.getY();
    for (std::size_t r = 0; r < rows.size() && index < grid.size(); ++r)
    {
        const int count = juce::jmin (rows[r], (int) (grid.size() - index));
        int width = 0;
        for (int i = 0; i < count; ++i)
            width += cellWidth (grid[index + (std::size_t) i]->style);
        int x = content.getX() + (content.getWidth() - width) / 2;

        Control* previous = nullptr;
        for (int i = 0; i < count; ++i, ++index)
        {
            auto* control = grid[index];
            auto cell = juce::Rectangle<int> (x, rowTop,
                                              cellWidth (control->style),
                                              gridRowHeight);
            x += cell.getWidth();

            auto body = cell;
            auto captionRow = body.removeFromTop (labelHeight);
            // A control with no caption of its own shares the one to its
            // left — the INTERVAL pair is one caption over two buttons, and
            // an empty label under the second is the only orphaned text the
            // panel had.
            if (control->label->getText().isEmpty() && previous != nullptr)
            {
                previous->label->setBounds (
                    previous->label->getBounds().getUnion (captionRow));
                control->label->setVisible (false);
                control->label->setBounds ({});
            }
            else
            {
                control->label->setVisible (true);
                control->label->setBounds (captionRow);
            }
            // Every style gives up the same strip at the foot of its cell,
            // whether or not it prints a value there, so the controls in a
            // row share one vertical centre instead of the knobs sitting
            // 6 px above everything else.
            auto valueRow = body.removeFromBottom (valueHeight);
            switch (control->style)
            {
                case Style::Knob:
                    control->value->setBounds (valueRow);
                    control->component->setBounds (
                        body.withSizeKeepingCentre (knobDiameter, knobDiameter));
                    break;
                case Style::Combo:
                    control->component->setBounds (
                        body.withSizeKeepingCentre (cell.getWidth() - 8,
                                                    comboHeight));
                    break;
                case Style::Toggle:
                case Style::Action:
                    control->component->setBounds (
                        body.withSizeKeepingCentre (cell.getWidth() - 10,
                                                    toggleHeight));
                    break;
                case Style::VSlider:
                    break;   // laid out in the strip above
            }
            previous = control;
        }
        rowTop += gridRowHeight;
    }
}

// One band of sections: they keep their natural widths and share whatever is
// left over as extra gap, so a short band stays balanced instead of ragged.
// `connectors` names what to draw in each gap — the voice band's stages are a
// real chain, and the two oscillators feed the mixer in parallel rather than
// one into the other, so the first gap gets a plus and the rest chevrons.
void SeptumAudioProcessorEditor::layoutBand (const std::vector<int>& indices,
                                             juce::Rectangle<int> bounds,
                                             const std::vector<Connector>& connectors)
{
    if (indices.empty())
        return;

    int total = 0;
    for (int index : indices)
        total += sections[(std::size_t) index]->naturalWidth();

    const int slots = (int) indices.size() - 1;
    const bool marked = ! connectors.empty();
    const int baseGap = marked ? chevronGap : sectionGap;
    const int spare = juce::jmax (0, bounds.getWidth() - total - slots * baseGap);
    const int extra = slots > 0 ? spare / slots : 0;

    int x = bounds.getX();
    for (std::size_t i = 0; i < indices.size(); ++i)
    {
        auto& section = *sections[(std::size_t) indices[i]];
        const int width = section.naturalWidth();
        layoutSection (section, { x, bounds.getY(), width, bounds.getHeight() });
        x += width;
        if (i + 1 < indices.size())
        {
            const int gap = baseGap + extra;
            const auto kind = i < connectors.size() ? connectors[i] : Connector::None;
            if (kind != Connector::None)
                chevrons.push_back ({ { x + gap / 2, bounds.getCentreY() }, kind });
            x += gap;
        }
    }
}

void SeptumAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    chevrons.clear();

    auto header = bounds.removeFromTop (headerHeight).reduced (12, 6);
    titleLabel.setBounds (header.removeFromLeft (130));
    subtitleLabel.setBounds (header);

    // Keyboard row: lever at the left of the keys, as on the unit.
    auto keyboardRow = bounds.removeFromBottom (keyboardHeight).reduced (12, 5);
    lever.setBounds (keyboardRow.removeFromLeft (66));
    keyboardRow.removeFromLeft (6);
    // The visible range spans 36 white keys (five octaves).
    keyboard.setKeyWidth ((float) keyboardRow.getWidth() / 36.0f);
    keyboard.setBounds (keyboardRow);

    // Patch strip directly above the keys (the hardware's button row). Its
    // controls are the same size as every other control on the panel.
    auto strip = bounds.removeFromBottom (stripHeight).reduced (10, 2);
    stripSection->bounds = strip;
    auto stripContent = strip.reduced (sectionPadding, sectionPadding);
    stripContent.removeFromTop (sectionTitleHeight);
    {
        auto selectors = stripContent.removeFromLeft (352);
        selectors = selectors.withSizeKeepingCentre (selectors.getWidth(),
                                                     comboHeight);
        programBox.setBounds (selectors.removeFromLeft (196));
        selectors.removeFromLeft (10);
        lowerButton.setBounds (selectors.removeFromLeft (70));
        selectors.removeFromLeft (3);
        upperButton.setBounds (selectors.removeFromLeft (70));
    }
    stripContent.removeFromLeft (16);
    {
        const auto cellFor = [] (const Control* control)
        { return control->style == Style::Combo ? comboCell : knobCell; };
        int total = 0;
        for (auto* control : stripSection->controls)
            total += cellFor (control);
        const int slots = juce::jmax (1, (int) stripSection->controls.size() - 1);
        const int extra =
            juce::jmax (0, (stripContent.getWidth() - total) / slots);
        int x = stripContent.getX();
        for (auto* control : stripSection->controls)
        {
            auto cell = juce::Rectangle<int> (x, stripContent.getY(),
                                              cellFor (control),
                                              stripContent.getHeight());
            x += cell.getWidth() + extra;
            control->label->setBounds (cell.removeFromTop (labelHeight));
            if (control->style == Style::Combo)
            {
                control->component->setBounds (
                    cell.withSizeKeepingCentre (cellFor (control) - 8, comboHeight));
            }
            else
            {
                if (control->value != nullptr)
                    control->value->setBounds (cell.removeFromBottom (valueHeight));
                control->component->setBounds (
                    cell.withSizeKeepingCentre (knobDiameter, knobDiameter));
            }
        }
    }

    // Left performance cluster, spanning all three bands.
    auto panel = bounds.reduced (10, 4);
    auto cluster = panel.removeFromLeft (clusterWidth);
    performSection->bounds = cluster;
    auto clusterContent = cluster.reduced (sectionPadding, sectionPadding);
    clusterContent.removeFromTop (sectionTitleHeight);
    masterLabel.setBounds (clusterContent.removeFromTop (labelHeight));
    masterSlider.setBounds (
        clusterContent.removeFromTop (56).withSizeKeepingCentre (52, 52));
    masterValueLabel.setBounds (clusterContent.removeFromTop (valueHeight));
    clusterContent.removeFromTop (8);
    octLabel.setBounds (clusterContent.removeFromTop (labelHeight));
    {
        auto row = clusterContent.removeFromTop (24);
        octDownButton.setBounds (row.removeFromLeft (36));
        octValueLabel.setBounds (row.removeFromLeft (row.getWidth() - 36));
        octUpButton.setBounds (row);
    }
    clusterContent.removeFromTop (10);
    const auto placeClusterControl = [&clusterContent] (Control* control)
    {
        if (control == nullptr)
            return;
        control->label->setBounds (clusterContent.removeFromTop (labelHeight));
        if (control->style == Style::Knob)
        {
            control->component->setBounds (
                clusterContent.removeFromTop (knobDiameter)
                    .withSizeKeepingCentre (knobDiameter, knobDiameter));
            control->value->setBounds (clusterContent.removeFromTop (valueHeight));
        }
        else if (control->style == Style::Combo)
        {
            control->component->setBounds (
                clusterContent.removeFromTop (comboHeight).reduced (2, 0));
        }
        else
        {
            control->component->setBounds (
                clusterContent.removeFromTop (toggleHeight)
                    .withSizeKeepingCentre (clusterContent.getWidth() - 12,
                                            toggleHeight));
        }
        clusterContent.removeFromTop (8);
    };
    placeClusterControl (portaControl);
    placeClusterControl (portaTimeControl);
    placeClusterControl (soloControl);
    placeClusterControl (tempoControl);

    // The cluster's foot reports rather than edits: the output meter and the
    // voice count, where the hardware puts its own indicators.
    if (clusterContent.getHeight() > 60)
    {
        voiceLabel.setBounds (clusterContent.removeFromBottom (14));
        clusterContent.removeFromBottom (4);
        meterBounds = clusterContent.removeFromBottom (
            juce::jmin (44, clusterContent.getHeight() - 6));
    }
    else
    {
        meterBounds = {};
        voiceLabel.setBounds ({});
    }

    panel.removeFromLeft (sectionGap);

    // Three bands. Section indices follow the construction order in the
    // constructor: 0 PERFORM, then the voice chain, then the modulators, then
    // the arpeggiator, the external input and the effects, then PATCH.
    const int rowHeight = (panel.getHeight() - (bandRows - 1) * sectionGap)
                          / bandRows;
    auto voiceRow = panel.removeFromTop (rowHeight);
    panel.removeFromTop (sectionGap);
    auto modulationRow = panel.removeFromTop (rowHeight);
    panel.removeFromTop (sectionGap);
    auto effectsRow = panel;

    layoutBand ({ 1, 2, 3, 4, 5 }, voiceRow,
                { Connector::Sum, Connector::Flow, Connector::Flow,
                  Connector::Flow });
    layoutBand ({ 6, 7, 8, 9, 10 }, modulationRow, {});
    layoutBand ({ 11, 12, 13, 14 }, effectsRow, {});
}

void SeptumAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::body);

    // Header rule and accent stripe, echoing the hardware's banded fascia.
    g.setColour (colours::frame);
    g.fillRect (0, headerHeight - 3, getWidth(), 2);
    g.setColour (colours::accent);
    g.fillRect (0, headerHeight - 6, getWidth(), 2);

    const auto bandColour = [] (Band band)
    {
        switch (band)
        {
            case Band::Voice:        return colours::bandVoice;
            case Band::Modulation:   return colours::bandModulation;
            case Band::InputEffects: return colours::bandEffects;
            case Band::Perform:      return colours::bandPerform;
        }
        return colours::bandPerform;
    };

    for (const auto& section : sections)
    {
        auto bounds = section->bounds.reduced (1).toFloat();
        if (bounds.isEmpty())
            continue;
        g.setColour (colours::recess);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (colours::frame.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        // The band's tint appears once, as a short rule above the title.
        const auto title =
            bounds.withHeight ((float) sectionTitleHeight)
                .translated (0.0f, (float) sectionPadding - 1.0f)
                .reduced ((float) sectionPadding, 0.0f);
        g.setColour (bandColour (section->band));
        g.fillRect (title.getX(), title.getY() - 3.0f, 22.0f, 2.0f);
        g.setColour (colours::frame);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (section->title, title.toNearestInt(),
                    juce::Justification::centredLeft);
    }

    // The voice chain is a chain, so it is drawn as one: the two oscillators
    // meet at a plus, and each stage after that follows a chevron.
    g.setColour (colours::frame.withAlpha (0.38f));
    for (const auto& mark : chevrons)
    {
        const auto x = (float) mark.position.x;
        const auto y = (float) mark.position.y;
        if (mark.kind == Connector::Sum)
        {
            g.fillRect (x - 4.5f, y - 0.8f, 9.0f, 1.6f);
            g.fillRect (x - 0.8f, y - 4.5f, 1.6f, 9.0f);
        }
        else
        {
            juce::Path chevron;
            chevron.startNewSubPath (x - 3.0f, y - 5.0f);
            chevron.lineTo (x + 3.0f, y);
            chevron.lineTo (x - 3.0f, y + 5.0f);
            g.strokePath (chevron, juce::PathStrokeType (1.6f));
        }
    }

    // Output meter, at the foot of the performance cluster where the voice
    // count sits: the one place on the panel that reports rather than edits.
    if (! meterBounds.isEmpty())
    {
        const int barWidth = 12;
        for (int channel = 0; channel < 2; ++channel)
        {
            auto bar = juce::Rectangle<int> (
                meterBounds.getCentreX() - barWidth - 3 + channel * (barWidth + 6),
                meterBounds.getY(), barWidth, meterBounds.getHeight());
            g.setColour (colours::sliderTrack);
            g.fillRoundedRectangle (bar.toFloat(), 2.0f);
            const float level = juce::jlimit (0.0f, 1.0f, meterLevel[channel]);
            g.setColour (colours::ledOn);
            g.fillRoundedRectangle (
                bar.removeFromBottom ((int) (level * (float) meterBounds.getHeight()))
                    .toFloat(),
                2.0f);
        }
    }
}

void SeptumAudioProcessorEditor::timerCallback()
{
    meterLevel[0] = processor.getOutputLevel (0);
    meterLevel[1] = processor.getOutputLevel (1);
    voiceLabel.setText (juce::String (processor.getActiveVoiceCount())
                            + " / 10 VOICES",
                        juce::dontSendNotification);
    programBox.setSelectedId (processor.getCurrentProgram() + 1,
                              juce::dontSendNotification);
    // juce::Label::setText only repaints when the text actually changes, so
    // this costs nothing on the frames where nothing moved.
    refreshValues();
    if (! meterBounds.isEmpty())
        repaint (meterBounds.expanded (2));
}

void SeptumAudioProcessorEditor::handleNoteOn (juce::MidiKeyboardState*, int,
                                                   int note, float velocity)
{
    processor.triggerFromUi (note,
                             juce::jlimit (1, 127, (int) (velocity * 127.0f)));
}

void SeptumAudioProcessorEditor::handleNoteOff (juce::MidiKeyboardState*,
                                                    int, int note, float)
{
    processor.releaseFromUi (note);
}

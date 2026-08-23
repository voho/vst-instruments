#include "PluginEditor.h"

#include <array>

namespace
{
namespace colours
{
    const juce::Colour body { 0xfff2f0ea };        // warm panel white
    const juce::Colour surround { 0xff1c1c1f };    // behind the scaled panel
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
    // The two tones. Every per-tone section wears the colour of the tone the
    // panel is editing, so switching target repaints the whole per-tone half
    // of the panel and cannot be missed. Shared sections never wear either.
    const juce::Colour toneUpper { 0xffc7472e };   // the panel accent
    const juce::Colour toneLower { 0xff2b6f86 };   // its cool counterpart
}

// Fixed control geometry. Sections are sized to fit their contents; the
// contents are never scaled to fit a section, which is what keeps a knob the
// same size wherever it appears.
constexpr int knobCell = 58;
constexpr int comboCell = 104;
// One selector on the panel has entries as long as FILTER-CUTOFF-KEYFOLLOW,
// and a 104-point cell clips them.
constexpr int wideComboCell = 176;
constexpr int toggleCell = 66;
constexpr int actionCell = 60;
constexpr int sliderCell = 34;

constexpr int labelHeight = 13;
constexpr int valueHeight = 13;
constexpr int knobDiameter = 40;
constexpr int comboHeight = 22;
constexpr int toggleHeight = 22;

constexpr int sectionTitleHeight = 20;
// The chip a per-tone section wears on its title row, naming the tone it is
// editing.
constexpr int toneChipWidth = 52;
constexpr int sectionPadding = 7;
constexpr int gridRowHeight = 68;      // label + control + value
constexpr int sectionGap = 6;
constexpr int chevronGap = 16;

constexpr int headerHeight = sectionTitleHeight + gridRowHeight
                             + 2 * sectionPadding + 10;
constexpr int clusterWidth = 128;
// Tall enough for a full-size control cell under the strip's own title,
// so the patch strip's knobs are the same knobs as everywhere else.
constexpr int stripHeight = sectionTitleHeight + gridRowHeight
                            + 2 * sectionPadding + 6;
// The bottom row is the instrument's performance surface: the per-tone play
// controls, the lever, and the keys under the band that says which tone each
// of them reaches.
constexpr int keyZoneHeight = 15;
constexpr int keyboardHeight = sectionTitleHeight + gridRowHeight
                               + 2 * sectionPadding;
// The meter reads in decibels down to here, which is the range a player
// actually mixes in.
constexpr float meterFloorDb = -48.0f;
constexpr int bandRows = 3;
constexpr int bandHeight = sectionTitleHeight + 2 * gridRowHeight + 2 * sectionPadding;
// A host window carries a frame and a title bar the panel does not get to
// use; these are enough of both that the fit rule does not put the panel
// under one.
constexpr int windowChromeWidth = 32;
constexpr int windowChromeHeight = 64;
// Wide enough for the REVERB section's whole documented parameter set: the
// effects band's four sections need 1474 points of content between them, and
// the panel scales to the window anyway.
constexpr int editorWidth = 1660;
constexpr int editorHeight = headerHeight + bandRows * bandHeight
                             + (bandRows - 1) * sectionGap + stripHeight
                             + keyboardHeight + 18;
// Below about 60 % the 10-point captions stop being readable, so the panel
// stops shrinking there and the window scrolls the difference instead.
constexpr int minimumWidth = editorWidth * 3 / 5;
constexpr int minimumHeight = editorHeight * 3 / 5;
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

    // The edit-target pair draws as tabs, not as two more lit toggles. Every
    // other lit control on this panel means "this switch is ON"; these two
    // mean "the panel is showing this tone", which is a different kind of
    // statement and needs a different shape to say it in.
    if (static_cast<bool> (button.getProperties()["tab"]))
    {
        const auto tone =
            button.getProperties()["tone"].toString() == "lower"
                ? colours::toneLower
                : colours::toneUpper;
        g.setColour (on ? colours::frame
                        : isDown || isHighlighted ? colours::sliderTrack
                                                  : colours::body);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (on ? tone : colours::frame.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds, 3.0f, on ? 1.5f : 1.0f);
        if (on)
            g.fillRect (bounds.getX() + 3.0f, bounds.getY() + 1.5f,
                        bounds.getWidth() - 6.0f, 3.0f);
        return;
    }

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
    const bool tab = static_cast<bool> (button.getProperties()["tab"]);
    g.setFont (juce::Font (juce::FontOptions (
        tab ? 13.0f : button.getWidth() < 34 ? 9.5f : 11.0f, juce::Font::bold)));
    g.setColour (button.getToggleState()
                     ? juce::Colours::white
                     : colours::frame.withAlpha (tab ? 0.5f : 1.0f));
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

void SeptumLever::applyFromEvent (const juce::MouseEvent& event)
{
    const auto bounds = leverBounds();
    // Bend is absolute and spring-loaded, as the lever's horizontal axis is:
    // where you take it is where it is, and it comes back when you let go.
    bend = juce::jlimit (-1.0f, 1.0f,
                         (event.position.x - bounds.getCentreX())
                             / (bounds.getWidth() * 0.5f));
    // Modulation holds its position, so it moves by the drag rather than
    // jumping to the click: the hardware lever cannot be *put* at full
    // modulation by tapping the top of its travel, and a tap that latched
    // full vibrato is not something a player can see or easily undo.
    const float travel = (grabY - event.position.y) / bounds.getHeight();
    mod = juce::jlimit (0.0f, 1.0f, grabMod + travel);
    push();
    repaint();
}

juce::Rectangle<float> SeptumLever::leverBounds() const
{
    // The caption gets its own band at the foot; the frame and the stick are
    // drawn in what is left, so no text sits on top of a border.
    return getLocalBounds()
        .withTrimmedBottom (captionHeight)
        .toFloat()
        .reduced (2.0f);
}

void SeptumLever::mouseDown (const juce::MouseEvent& event)
{
    grabY = event.position.y;
    grabMod = mod;
    applyFromEvent (event);
}

void SeptumLever::mouseDrag (const juce::MouseEvent& event)
{
    applyFromEvent (event);
}

void SeptumLever::mouseUp (const juce::MouseEvent&)
{
    bend = 0.0f;  // the lever's bend axis is spring-loaded
    push();
    repaint();
}

void SeptumLever::mouseDoubleClick (const juce::MouseEvent&)
{
    // Somewhere to put the modulation back, since it holds.
    mod = 0.0f;
    bend = 0.0f;
    push();
    repaint();
}

void SeptumLever::paint (juce::Graphics& g)
{
    const auto bounds = leverBounds();
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

    g.setColour (colours::frame.withAlpha (0.72f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("BEND / MOD", getLocalBounds().removeFromBottom (captionHeight),
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
    // Everything the panel draws lives on the canvas; the editor holds only
    // the canvas and the transform that maps it onto the window.
    addAndMakeVisible (canvas);

    titleLabel.setText ("Septum", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, colours::frame);
    canvas.addAndMakeVisible (titleLabel);

    subtitleLabel.setText (
        juce::String::fromUTF8 ("ten-voice virtual analog synthesizer"),
        juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    subtitleLabel.setColour (juce::Label::textColourId,
                             colours::frame.withAlpha (0.7f));
    canvas.addAndMakeVisible (subtitleLabel);

    voiceLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    voiceLabel.setJustificationType (juce::Justification::centred);
    voiceLabel.setColour (juce::Label::textColourId,
                          colours::frame.withAlpha (0.72f));
    voiceLabel.setText ("0 / 10 VOICES", juce::dontSendNotification);
    canvas.addAndMakeVisible (voiceLabel);

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
    canvas.addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "master_level", masterSlider);
    masterLabel.setText ("MASTER VOL", juce::dontSendNotification);
    masterLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.setColour (juce::Label::textColourId,
                           colours::frame.withAlpha (0.72f));
    canvas.addAndMakeVisible (masterLabel);
    masterValueLabel.setFont (
        juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    masterValueLabel.setJustificationType (juce::Justification::centred);
    canvas.addAndMakeVisible (masterValueLabel);

    octLabel.setText ("KEYBOARD OCTAVE", juce::dontSendNotification);
    octLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    octLabel.setColour (juce::Label::textColourId,
                        colours::frame.withAlpha (0.72f));
    octLabel.setJustificationType (juce::Justification::centred);
    canvas.addAndMakeVisible (octLabel);
    octValueLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    octValueLabel.setJustificationType (juce::Justification::centred);
    canvas.addAndMakeVisible (octValueLabel);
    // Settled: SYSTEM COMMON Octave Shift is -3..+3 (address map 00 17), and
    // the OCT UP/DOWN buttons are what set it on the instrument. They write
    // the parameter, so a host can automate the same thing the buttons do.
    octDownButton.onClick = [this] { stepKeyboardOctave (-1); };
    octUpButton.onClick = [this] { stepKeyboardOctave (1); };
    canvas.addAndMakeVisible (octDownButton);
    canvas.addAndMakeVisible (octUpButton);

    // PORTAMENTO, GLIDE TIME and POLY/SOLO used to sit here, among controls
    // that belong to the whole instrument, while being Patch Tone bytes that
    // edit one tone. They moved to TONE PLAY on the keyboard row; what is
    // left in this cluster is global or patch-wide without exception.
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
            // Snapped, because the write below snaps: with OSC 1 at +30 the
            // fifth wants +37, lands on +36, and an unsnapped comparison then
            // read "not there yet" forever — the second press, documented as
            // the way back to unison, did nothing and the lamp stayed dark.
            const float wanted =
                snapToneParameter ("osc2_pitch", root + (float) semitones);
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
    nameEnds (addControl (*mixMod, "balance", "BALANCE", Style::Knob),
              "OSC1", "OSC2");
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
    amp->rowCounts = { 3, 4 };
    addControl (*amp, "level", "LEVEL", Style::Knob);
    addControl (*amp, "level_vel", "VELOCITY", Style::Knob);
    nameEnds (addControl (*amp, "pan", "PAN", Style::Knob), "L", "R");
    addControl (*amp, "overdrive", "OVERDRIVE", Style::Toggle);
    addControl (*amp, "drive", "DRIVE", Style::Knob);
    // The two effect send depths are Patch *Tone* bytes — one per tone — so
    // they belong to the tone's amp stage, not to the shared effect that
    // receives them. Sitting in DELAY and REVERB they were the only per-tone
    // controls in two otherwise shared sections, and nothing said so.
    addControl (*amp, "delay_depth", "DLY SEND", Style::Knob);
    addControl (*amp, "reverb_depth", "REV SEND", Style::Knob);

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
    splitArpControl =
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
    delay->rowCounts = { 3, 3 };
    addControl (*delay, "delay_on", "SWITCH", Style::Toggle, false);
    addControl (*delay, "delay_time", "TIME", Style::Knob, false);
    addControl (*delay, "delay_feedback", "FEEDBACK", Style::Knob, false, " %");
    addControl (*delay, "delay_hf_damp", "HF DAMP", Style::Combo, false);
    addControl (*delay, "delay_mod_rate", "MOD RATE", Style::Knob, false);
    addControl (*delay, "delay_mod_depth", "MOD DEPTH", Style::Knob, false);

    auto* reverb = section ("REVERB", Band::InputEffects);
    reverb->rowCounts = { 6, 5 };
    addControl (*reverb, "reverb_on", "SWITCH", Style::Toggle, false);
    addControl (*reverb, "reverb_time", "TIME", Style::Knob, false);
    addControl (*reverb, "reverb_size", "SIZE", Style::Knob, false);
    addControl (*reverb, "reverb_pre_delay", "PRE DELAY", Style::Knob, false,
                " ms");
    addControl (*reverb, "reverb_high_cut", "HIGH CUT", Style::Combo, false);
    addControl (*reverb, "reverb_density", "DENSITY", Style::Knob, false);
    addControl (*reverb, "reverb_diffusion", "DIFFUSION", Style::Knob, false);
    // The four remaining settled Patch Reverb bytes. PRE DELAY, HIGH CUT,
    // DENSITY and DIFFUSION above are equally editor-only on the instrument,
    // so leaving exactly these four off was an inconsistency rather than a
    // principle.
    addControl (*reverb, "reverb_lf_damp_freq", "LF DAMP", Style::Combo, false);
    addControl (*reverb, "reverb_lf_damp_gain", "LF GAIN", Style::Knob, false,
                " dB");
    addControl (*reverb, "reverb_hf_damp_freq", "HF DAMP", Style::Combo, false);
    addControl (*reverb, "reverb_hf_damp_gain", "HF GAIN", Style::Knob, false,
                " dB");

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
    canvas.addAndMakeVisible (programBox);
    programLabel.setText ("PROGRAM", juce::dontSendNotification);
    programLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    programLabel.setJustificationType (juce::Justification::centredLeft);
    programLabel.setColour (juce::Label::textColourId,
                            colours::frame.withAlpha (0.72f));
    canvas.addAndMakeVisible (programLabel);

    // Every control on this strip belongs to the patch as a whole. BEND and
    // TONE OCT used to sit here and are Patch Tone bytes; they moved to
    // TONE PLAY with the rest of the per-tone play controls.
    addControl (*stripSection, "keyboard_mode", "KEYBOARD", Style::Combo, false);
    partControl =
        addControl (*stripSection, "keyboard_part", "PART", Style::Combo, false);
    splitPointControl =
        addControl (*stripSection, "split_point", "SPLIT POINT", Style::Knob, false);
    addControl (*stripSection, "patch_level", "PATCH LEVEL", Style::Knob, false);
    nameEnds (addControl (*stripSection, "tone_balance", "TONE BAL", Style::Knob,
                          false),
              "LOWER", "UPPER");
    addControl (*stripSection, "mod_assign", "MOD ASSIGN", Style::Combo, false);
    // CONTROLLER DESTINATION: which tone each physical controller reaches.
    // These say UPPER/LOWER for a third reason again — not which tone is
    // edited, not which tone sounds, but which tone a lever or a pedal gets
    // to move — so each one names its controller and says TO TONE, which is
    // what tells them apart from PATCH LEVEL and TONE BAL beside them. There
    // is no group heading over them: the strip is one flat row of cells and
    // this comment used to claim one the panel never painted.
    addControl (*stripSection, "mod_dest", "MOD TO TONE", Style::Combo, false);
    addControl (*stripSection, "bend_dest", "BEND TO TONE", Style::Combo, false);
    addControl (*stripSection, "expr_dest", "EXPR TO TONE", Style::Combo, false);

    // ---- TONE PLAY, on the keyboard row beside the lever and the keys.
    // Five Patch *Tone* bytes about how the selected tone is played: they
    // were scattered between the global performance cluster and the patch
    // strip, where nothing said they belonged to one tone. Built after the
    // bands so the index lists below keep the construction order they name.
    tonePlaySection = section ("TONE PLAY", Band::Perform);
    tonePlaySection->rowCounts = { 5 };
    addControl (*tonePlaySection, "portamento", "PORTAMENTO", Style::Toggle);
    addControl (*tonePlaySection, "porta_time", "GLIDE TIME", Style::Knob);
    addControl (*tonePlaySection, "mono_mode", "POLY / SOLO", Style::Combo);
    addControl (*tonePlaySection, "bend_range", "BEND", Style::Knob, true, " st");
    addControl (*tonePlaySection, "octave_shift", "TONE OCT", Style::Knob);

    // ---- EDIT TONE, in the header. The panel edits one tone at a time and
    // every per-tone control silently changes meaning with this pair, so it
    // sits above the controls it governs rather than below them, it is
    // captioned, it draws as a pair of tabs rather than as two more of the
    // panel's lit toggles, and it says in words what the current keyboard
    // mode does with the tone it selects.
    editToneSection = section ("EDIT TONE", Band::Perform);
    editToneSection->manualLayout = true;

    const auto tab = [this] (juce::TextButton& button, bool upper)
    {
        button.setClickingTogglesState (false);
        button.getProperties().set ("tab", true);
        button.onClick = [this, upper] { setEditingUpper (upper); };
        canvas.addAndMakeVisible (button);
    };
    tab (upperButton, true);
    tab (lowerButton, false);
    toneStatusLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
    toneStatusLabel.setJustificationType (juce::Justification::centredLeft);
    toneStatusLabel.setInterceptsMouseClicks (false, false);
    canvas.addAndMakeVisible (toneStatusLabel);

    // ---- SYSTEM COMMON, in the header: settings that apply to the whole
    // instrument rather than to the patch, and are not saved with one. Built
    // last so the band index lists below keep the construction order they
    // name.
    systemSection = section ("SYSTEM", Band::Perform);
    systemSection->rowCounts = { 3 };
    addControl (*systemSection, "system_master_tune", "TUNE", Style::Knob,
                false);
    addControl (*systemSection, "system_key_shift", "KEY SHIFT", Style::Knob,
                false, " st");
    addControl (*systemSection, "system_transpose", "TRANSPOSE", Style::Knob,
                false, " st");

    // A section's scope is what its controls are, not what it declares. Every
    // section on this panel is wholly one or the other, because a mixed
    // section is exactly the defect Step 28 set out to remove: it would be
    // classified by its one per-tone control and wear the tone chip and wash
    // over controls that are not per-tone. Recorded rather than asserted —
    // `jassert` is compiled out of every build this project produces, so the
    // suite reads this list and expects it empty.
    mixedScopeSections.clear();
    for (auto& entry : sections)
    {
        int perTone = 0, shared = 0;
        for (const auto* control : entry->controls)
            (control->perTone ? perTone : shared) += 1;
        entry->scope = perTone > 0 ? Scope::PerTone : Scope::Shared;
        if (perTone > 0 && shared > 0)
            mixedScopeSections.add (entry->title);
    }

    // The target the player last chose, so reopening the editor does not
    // silently put them back on UPPER.
    editingUpper = (bool) processor.parameters.state.getProperty (
        "editingUpperTone", true);

    bindControls();
    refreshToneTarget();

    keyboardState.addListener (this);
    keyboard.setOctaveForMiddleC (4);
    applyKeyboardOctave();
    canvas.addAndMakeVisible (keyboard);
    canvas.addAndMakeVisible (lever);

    refreshValues();
    setOpaque (true);
    canvas.setOpaque (true);
    canvas.setInterceptsMouseClicks (false, true);

    // Any window; the panel inside it is always the design geometry, scaled.
    setResizable (true, false);
    // setResizeLimits installs the default constrainer, so it has to come
    // before the ratio is set on it.
    setResizeLimits (minimumWidth, minimumHeight, editorWidth * 2,
                     editorHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) editorWidth
                                          / (double) editorHeight);
    juce::Rectangle<int> workArea;
    if (auto* display =
            juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        workArea = display->userArea;
    const auto opening = panelSizeForWorkArea (workArea);
    setSize (opening.getWidth(), opening.getHeight());
    startTimerHz (24);
}

SeptumAudioProcessorEditor::~SeptumAudioProcessorEditor()
{
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
}

void SeptumAudioProcessorEditor::stepKeyboardOctave (int delta)
{
    auto* parameter = processor.parameters.getParameter ("system_octave");
    if (parameter == nullptr)
        return;
    const auto& range = processor.parameters.getParameterRange ("system_octave");
    const float wanted = range.snapToLegalValue (
        processor.parameters.getRawParameterValue ("system_octave")->load()
        + (float) delta);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (range.convertTo0to1 (wanted));
    parameter->endChangeGesture();
    applyKeyboardOctave();
}

void SeptumAudioProcessorEditor::applyKeyboardOctave()
{
    const auto* value =
        processor.parameters.getRawParameterValue ("system_octave");
    const int shift = value != nullptr ? (int) std::lround (value->load()) : 0;
    // Shift the printed octave names, not the note numbers. A key the player
    // clicks is sent to the engine unchanged (handleNoteOn -> triggerFromUi),
    // and the engine applies SYSTEM COMMON Octave Shift itself, so moving the
    // drawn range as well applied it twice: one press of OCT UP transposed
    // the on-screen keys by two octaves while their printed names claimed
    // one. The keys keep their notes; what moves is what they are called,
    // which is what the shift actually does to the pitch they sound.
    keyboard.setAvailableRange (36, 96);
    // JUCE's setOctaveForMiddleC repaints unconditionally, and the frame timer
    // calls this every tick, so an idle panel invalidated the whole keyboard
    // 24 times a second — and through the scaled canvas re-ran the panel paint
    // over that strip with it. setAvailableRange above is change-guarded
    // inside JUCE; this one has to be guarded here.
    if (shift != lastKeyboardOctave)
    {
        lastKeyboardOctave = shift;
        keyboard.setOctaveForMiddleC (4 + shift);
    }
    octValueLabel.setText (shift == 0 ? juce::String ("0")
                                      : (shift > 0 ? "+" : "")
                                            + juce::String (shift),
                           juce::dontSendNotification);
    octDownButton.setToggleState (shift < 0, juce::dontSendNotification);
    octUpButton.setToggleState (shift > 0, juce::dontSendNotification);
}

// Which tones the current keyboard mode lets sound, and one line of English
// saying so. Read from the parameters rather than the engine, so the panel
// and the host can never disagree about it.
SeptumAudioProcessorEditor::ToneAudibility
SeptumAudioProcessorEditor::toneAudibility() const
{
    const auto raw = [this] (const char* id)
    {
        const auto* value = processor.parameters.getRawParameterValue (id);
        return value != nullptr ? (int) std::lround (value->load()) : 0;
    };
    const int mode = raw ("keyboard_mode");   // 0 SINGLE, 1 DUAL, 2 SPLIT
    const bool partUpper = raw ("keyboard_part") == 0;

    ToneAudibility state;
    if (mode == 0)
    {
        state.upperSounds = partUpper;
        state.lowerSounds = ! partUpper;
        state.summary = juce::String ("SINGLE - only ")
                        + (partUpper ? "UPPER" : "LOWER")
                        + " sounds - 10 voices";
    }
    else if (mode == 1)
    {
        state.upperSounds = state.lowerSounds = true;
        state.summary = "DUAL - both tones layered - 5 voices each";
    }
    else
    {
        state.upperSounds = state.lowerSounds = true;
        juce::String point ("C4");
        if (auto* parameter = processor.parameters.getParameter ("split_point"))
            point = parameter->getCurrentValueAsText();
        state.summary = "SPLIT at " + point
                        + " - LOWER below, UPPER above - 5 voices each";
    }
    return state;
}

void SeptumAudioProcessorEditor::reconcileEditTarget()
{
    // An editor left open across a session load kept showing UPPER while the
    // restored state said LOWER, and re-saving from there wrote back what
    // somebody else had been editing rather than what the player was.
    const bool stored =
        (bool) processor.parameters.state.getProperty ("editingUpperTone",
                                                       editingUpper);
    if (stored != editingUpper)
        setEditingUpper (stored);
}

void SeptumAudioProcessorEditor::setEditingUpper (bool upper)
{
    if (editingUpper == upper)
        return;
    editingUpper = upper;
    // The target survives closing and reopening the editor. It is not a
    // parameter — it changes nothing that sounds, so a host has no business
    // automating it — but losing it on every reopen made an already invisible
    // mode silently revert.
    processor.parameters.state.setProperty ("editingUpperTone", upper, nullptr);
    bindControls (true);
    refreshToneTarget();
    canvas.repaint();
}

// Everything that has to change when the target moves or the keyboard mode
// does: the tabs, the status line, and the controls the mode makes inert.
void SeptumAudioProcessorEditor::refreshToneTarget()
{
    upperButton.setToggleState (editingUpper, juce::dontSendNotification);
    lowerButton.setToggleState (! editingUpper, juce::dontSendNotification);
    upperButton.getProperties().set ("tone", "upper");
    lowerButton.getProperties().set ("tone", "lower");

    const auto state = toneAudibility();
    const bool audible = editingUpper ? state.upperSounds : state.lowerSounds;
    toneStatusLabel.setText (
        audible ? state.summary
                : juce::String (editingUpper ? "UPPER" : "LOWER")
                      + " IS SILENT - " + state.summary,
        juce::dontSendNotification);
    toneStatusLabel.setColour (juce::Label::textColourId,
                               audible ? colours::frame.withAlpha (0.7f)
                                       : colours::accent);

    // A control the engine ignores in this mode is dimmed rather than left
    // looking live: PART decides nothing outside SINGLE, and the split point
    // and SPLIT ARPEGGIO decide nothing outside SPLIT.
    const int mode = [this]
    {
        const auto* value = processor.parameters.getRawParameterValue ("keyboard_mode");
        return value != nullptr ? (int) std::lround (value->load()) : 0;
    }();
    const auto dim = [] (Control* control, bool live)
    {
        if (control == nullptr)
            return;
        const float alpha = live ? 1.0f : 0.4f;
        control->component->setAlpha (alpha);
        control->label->setAlpha (alpha);
        if (control->value != nullptr)
            control->value->setAlpha (alpha);
    };
    dim (partControl, mode == 0);
    dim (splitPointControl, mode == 2);
    dim (splitArpControl, mode == 2);
}

float SeptumAudioProcessorEditor::getToneParameter (const char* suffix) const
{
    const auto id = septum::parameters::toneId (editingUpper, suffix);
    if (const auto* value = processor.parameters.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

float SeptumAudioProcessorEditor::snapToneParameter (const char* suffix,
                                                     float natural) const
{
    const auto id = septum::parameters::toneId (editingUpper, suffix);
    if (processor.parameters.getParameter (id) != nullptr)
        return processor.parameters.getParameterRange (id).snapToLegalValue (natural);
    return natural;
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
            case Style::WideCombo: return wideComboCell;
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

    // The title must fit too, or a section can end up narrower than its name
    // — and on a per-tone section the title row also carries the tone chip.
    const int titleWidth =
        (int) juce::GlyphArrangement::getStringWidth (
            juce::Font (juce::FontOptions (11.5f, juce::Font::bold)), title)
        + 26 + (scope == Scope::PerTone ? toneChipWidth + 6 : 0);
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
        case Style::WideCombo:
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
    canvas.addAndMakeVisible (*control->label);

    // Every continuous control reads out its value, so nothing on the panel
    // has to be dragged to be understood.
    if (style == Style::Knob || style == Style::VSlider)
    {
        control->value = std::make_unique<juce::Label>();
        control->value->setFont (
            juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        control->value->setJustificationType (juce::Justification::centred);
        control->value->setInterceptsMouseClicks (false, false);
        canvas.addAndMakeVisible (*control->value);
    }

    canvas.addAndMakeVisible (*control->component);
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
    // The same snapped targets the buttons write, so the lamp says where the
    // press actually lands rather than where it aimed.
    lamp (intervalOctControl,
          second == snapToneParameter ("osc2_pitch", root - 12.0f));
    lamp (intervalFifthControl,
          second == snapToneParameter ("osc2_pitch", root + 7.0f));

}

void SeptumAudioProcessorEditor::nameEnds (Control* control, const char* left,
                                           const char* right)
{
    if (control == nullptr)
        return;
    control->leftEnd = left;
    control->rightEnd = right;
}

void SeptumAudioProcessorEditor::bindControls (bool perToneOnly)
{
    for (auto& control : controls)
    {
        if (control->style == Style::Action || control->suffix.isEmpty())
            continue;
        // Only a per-tone control can change which parameter it edits, so a
        // target switch has no reason to tear down and rebuild the shared
        // ones — including re-populating combo boxes that are already right.
        if (perToneOnly && ! control->perTone)
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
            // A control naming a parameter that does not exist still draws,
            // hovers and drags — it simply edits nothing — so this cannot stay
            // a `jassert`, which no build here compiles in. The suite expects
            // this list empty.
            unresolvedParameterIds.addIfNotAlreadyThere (id);
            continue;
        }
        unresolvedParameterIds.removeString (id);
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
    // on top, travel in the middle, value underneath. A section whose width
    // was set by its title rather than by its contents — PITCH ENV is two
    // 34 px cells under a nine-character name — centres the strip in what it
    // was given instead of hugging the left edge.
    if (! sliders.empty())
    {
        const int stripWidth = (int) sliders.size() * sliderCell;
        if (grid.empty())
            content = content.withSizeKeepingCentre (stripWidth,
                                                     content.getHeight());
        auto strip = content.removeFromLeft (stripWidth);
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
            case Style::WideCombo: return wideComboCell;
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

    // Rows are centred in whatever height the band gave the section, so a
    // section with fewer rows than the band's height allows — FILTER ENV's
    // single DEPTH row beside four full-height sliders — does not leave all
    // its spare room in one block underneath.
    int usedRows = 0;
    for (std::size_t r = 0, placed = 0; r < rows.size() && placed < grid.size();
         ++r)
    {
        placed += (std::size_t) juce::jmax (0, rows[r]);
        ++usedRows;
    }
    std::size_t index = 0;
    int rowTop = content.getY()
                 + juce::jmax (0, (content.getHeight()
                                   - usedRows * gridRowHeight) / 2);
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
                case Style::WideCombo:
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

juce::Rectangle<int> SeptumAudioProcessorEditor::panelSizeForWorkArea (
    juce::Rectangle<int> workArea)
{
    // Full size wherever it fits. Where it does not - a 1366x768 or 1280x800
    // laptop, or a 1080p screen at 150 % - the whole panel shrinks rather
    // than losing its bottom edge. Never below the size the captions stay
    // readable at: on a screen smaller than that, a window the player can
    // move is a better failure than type nobody can read.
    double scale = 1.0;
    if (! workArea.isEmpty())
        scale = juce::jmin (1.0,
                            (double) (workArea.getWidth() - windowChromeWidth)
                                / editorWidth,
                            (double) (workArea.getHeight() - windowChromeHeight)
                                / editorHeight);
    scale = juce::jmax (scale, (double) minimumWidth / editorWidth);
    return { juce::roundToInt (editorWidth * scale),
             juce::roundToInt (editorHeight * scale) };
}

void SeptumAudioProcessorEditor::resized()
{
    // The panel keeps its proportions whatever the window's are, and is
    // centred in whatever is left over.
    const double scale = juce::jmin ((double) getWidth() / editorWidth,
                                     (double) getHeight() / editorHeight);
    const auto placed =
        juce::Rectangle<int> { juce::roundToInt (editorWidth * scale),
                               juce::roundToInt (editorHeight * scale) }
            .withCentre (getLocalBounds().getCentre());
    canvas.setTransform (
        juce::AffineTransform::scale ((float) scale)
            .translated ((float) placed.getX(), (float) placed.getY()));
    canvas.setBounds (0, 0, editorWidth, editorHeight);

    layoutPanel();
}

void SeptumAudioProcessorEditor::layoutPanel()
{
    auto bounds = juce::Rectangle<int> (0, 0, editorWidth, editorHeight);
    chevrons.clear();

    auto header = bounds.removeFromTop (headerHeight).reduced (10, 5);
    {
        // The system settings take the right of the header — they apply to
        // the whole instrument, which is what the header is for.
        layoutSection (*systemSection,
                       header.removeFromRight (systemSection->naturalWidth()));
        header.removeFromRight (12);

        // The identity takes the left; the edit-target tabs take the middle,
        // where the header was empty and where they sit above every control
        // they govern.
        auto identity =
            header.removeFromLeft (300).withSizeKeepingCentre (300, 40);
        titleLabel.setBounds (identity.removeFromLeft (120));
        subtitleLabel.setBounds (identity);
        header.removeFromLeft (12);

        auto editTone = header.removeFromLeft (
            juce::jmin (header.getWidth(), 470));
        editToneSection->bounds = editTone;
        auto content = editTone.reduced (sectionPadding, sectionPadding);
        content.removeFromTop (sectionTitleHeight);
        auto row = content.withSizeKeepingCentre (content.getWidth(), 30);
        upperButton.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (4);
        lowerButton.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (12);
        toneStatusLabel.setBounds (row);
    }

    // Keyboard row: the per-tone play controls, then the lever at the left of
    // the keys as on the unit, then the keys under the band that says which
    // tone each of them reaches.
    // TONE PLAY gets the row at its full height. The row used to be reduced by
    // four vertically *before* the section was cut out of it, so the section
    // had 94 points for the 102 `keyboardHeight` declares — its centring term
    // went to zero and the GLIDE TIME, BEND and TONE OCT read-outs overflowed
    // onto the well's bottom border, the only section on the panel with no
    // bottom padding at all. The four points go to the lever and the keys,
    // which is what they were for.
    auto keyboardRow = bounds.removeFromBottom (keyboardHeight).reduced (10, 0);
    layoutSection (*tonePlaySection,
                   keyboardRow.removeFromLeft (tonePlaySection->naturalWidth()));
    keyboardRow.removeFromLeft (sectionGap);
    keyboardRow.reduce (0, 4);
    lever.setBounds (keyboardRow.removeFromLeft (66).reduced (0, 2));
    keyboardRow.removeFromLeft (6);
    keyZoneBounds = keyboardRow.removeFromTop (keyZoneHeight);
    keyboardRow.removeFromTop (2);
    // The visible range spans 36 white keys (five octaves).
    keyboard.setKeyWidth ((float) keyboardRow.getWidth() / 36.0f);
    keyboard.setBounds (keyboardRow.reduced (0, 2));

    // Patch strip directly above the keys (the hardware's button row). Its
    // controls are the same size as every other control on the panel.
    auto strip = bounds.removeFromBottom (stripHeight).reduced (10, 2);
    stripSection->bounds = strip;
    auto stripContent = strip.reduced (sectionPadding, sectionPadding);
    stripContent.removeFromTop (sectionTitleHeight);
    {
        // The program box gets the caption row every other control on this
        // strip already had.
        auto selectors = stripContent.removeFromLeft (210);
        programLabel.setBounds (selectors.removeFromTop (labelHeight));
        programBox.setBounds (
            selectors.withSizeKeepingCentre (selectors.getWidth(), comboHeight));
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
    placeClusterControl (tempoControl);

    // The cluster's foot reports rather than edits: the output meter and the
    // voice count, where the hardware puts its own indicators.
    if (clusterContent.getHeight() > 60)
    {
        voiceLabel.setBounds (clusterContent.removeFromBottom (14));
        clusterContent.removeFromBottom (4);
        // The meter takes what the cluster has left rather than a fixed
        // 44 px: it is the one thing on the panel that reads better the
        // taller it is, and the space was otherwise dead.
        meterBounds = clusterContent.removeFromBottom (
            juce::jmin (150, clusterContent.getHeight() - 6));
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

    // Section indices follow the construction order in the constructor, and
    // inserting a section silently shifts every list below it. The titles at
    // the indices these three calls address are published through
    // getSectionTitles() and checked by the suite, because the `jassert` that
    // used to stand here is compiled out of every build this project makes.
    layoutBand ({ 1, 2, 3, 4, 5 }, voiceRow,
                { Connector::Sum, Connector::Flow, Connector::Flow,
                  Connector::Flow });
    layoutBand ({ 6, 7, 8, 9, 10 }, modulationRow, {});
    layoutBand ({ 11, 12, 13, 14 }, effectsRow, {});

    // Laying the panel out also re-reads what the parameters say about the
    // tones and the keyboard, so a panel rendered without the frame timer
    // running — the suite's snapshot — shows the same thing a live one does.
    reconcileEditTarget();
    refreshToneTarget();
    applyKeyboardOctave();
}

void SeptumAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Whatever the window has that the panel's proportions do not use.
    g.fillAll (colours::surround);
}

void SeptumAudioProcessorEditor::PanelCanvas::paint (juce::Graphics& g)
{
    owner.paintPanel (g);
}

void SeptumAudioProcessorEditor::paintPanel (juce::Graphics& g)
{
    g.fillAll (colours::body);

    // Header rule and accent stripe, echoing the hardware's banded fascia.
    g.setColour (colours::frame);
    g.fillRect (0, headerHeight - 3, editorWidth, 2);
    g.setColour (colours::accent);
    g.fillRect (0, headerHeight - 6, editorWidth, 2);

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

    // Which tone the per-tone half of the panel is showing, and whether the
    // keyboard mode lets it sound.
    const auto toneColour = editingUpper ? colours::toneUpper : colours::toneLower;
    const auto audibility = toneAudibility();
    const bool toneSounds =
        editingUpper ? audibility.upperSounds : audibility.lowerSounds;
    const juce::String toneName = editingUpper ? "UPPER" : "LOWER";

    for (const auto& section : sections)
    {
        auto bounds = section->bounds.reduced (1).toFloat();
        if (bounds.isEmpty())
            continue;
        const bool perTone = section->scope == Scope::PerTone;
        // A per-tone well is washed with the edited tone's own colour. It is
        // four per cent of it — enough that the per-tone half of the panel
        // reads as one group and visibly changes when the target does, not
        // enough to tint the controls sitting in it.
        g.setColour (perTone && toneSounds
                         ? colours::recess.interpolatedWith (toneColour, 0.06f)
                         : colours::recess);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (perTone && toneSounds ? toneColour.withAlpha (0.45f)
                                           : colours::frame.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        // The band's tint appears once, as a short rule above the title.
        auto title =
            bounds.withHeight ((float) sectionTitleHeight)
                .translated (0.0f, (float) sectionPadding - 1.0f)
                .reduced ((float) sectionPadding, 0.0f);
        g.setColour (bandColour (section->band));
        g.fillRect (title.getX(), title.getY() - 3.0f, 22.0f, 2.0f);

        // Every per-tone section says, on its own title row, which tone it is
        // editing. That is the whole point: a knob's meaning can no longer
        // depend on a control at the other end of the panel, because the
        // section the knob sits in names the tone itself. A hollow chip means
        // the keyboard mode is not letting that tone sound.
        if (perTone)
        {
            auto chip = title.removeFromRight ((float) toneChipWidth)
                            .withSizeKeepingCentre ((float) toneChipWidth, 14.0f);
            if (toneSounds)
            {
                g.setColour (toneColour);
                g.fillRoundedRectangle (chip, 3.0f);
                g.setColour (juce::Colours::white);
            }
            else
            {
                // Hollow and grey, not the tone's colour: the tone is being
                // edited but the keyboard mode is not letting it sound, and
                // the line beside the edit tabs says why. Not the word "OFF",
                // which on a synthesizer panel means a switch is thrown.
                g.setColour (colours::frame.withAlpha (0.45f));
                g.drawRoundedRectangle (chip.reduced (0.5f), 3.0f, 1.0f);
                g.setColour (colours::frame.withAlpha (0.6f));
            }
            g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
            g.drawText (toneName, chip.toNearestInt(), juce::Justification::centred);
            title.removeFromRight (4.0f);
        }

        g.setColour (colours::frame);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (section->title, title.toNearestInt(),
                    juce::Justification::centredLeft);
    }

    paintKeyboardZones (g);

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

    // The two ends of a bipolar knob whose direction the number does not give
    // away, printed small at the ends of its travel.
    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
    for (const auto& control : controls)
    {
        if (control->leftEnd.isEmpty() || control->component == nullptr)
            continue;
        const auto knob = control->component->getBounds();
        if (knob.isEmpty())
            continue;
        const auto row = juce::Rectangle<int> (knob.getX() - 12, knob.getBottom() - 9,
                                               knob.getWidth() + 24, 9);
        g.setColour (colours::frame.withAlpha (0.55f));
        g.drawText (control->leftEnd, row, juce::Justification::centredLeft);
        g.drawText (control->rightEnd, row, juce::Justification::centredRight);
    }

    // Output meter, at the foot of the performance cluster where the voice
    // count sits: the one place on the panel that reports rather than edits.
    if (! meterBounds.isEmpty())
    {
        // Decibels, not amplitude. On a linear scale a healthy −20 dBFS fills
        // a tenth of the bar, so the meter sat near its floor for everything
        // that was not about to clip.
        const int barWidth = 12;
        const auto height = (float) meterBounds.getHeight();
        const auto positionOf = [] (float dB)
        {
            return juce::jlimit (0.0f, 1.0f,
                                 juce::jmap (dB, meterFloorDb, 0.0f, 0.0f, 1.0f));
        };
        for (int channel = 0; channel < 2; ++channel)
        {
            auto bar = juce::Rectangle<int> (
                meterBounds.getCentreX() - barWidth - 3 + channel * (barWidth + 6),
                meterBounds.getY(), barWidth, meterBounds.getHeight());
            g.setColour (colours::sliderTrack);
            g.fillRoundedRectangle (bar.toFloat(), 2.0f);

            const float level = juce::jlimit (0.0f, 1.2f, meterLevel[channel]);
            const float dB = juce::Decibels::gainToDecibels (level, meterFloorDb);
            g.setColour (level >= 1.0f ? colours::accent : colours::ledOn);
            g.fillRoundedRectangle (
                bar.toFloat().removeFromBottom (positionOf (dB) * height), 2.0f);

            // A −6 dB mark, so the scale can be read rather than guessed.
            g.setColour (colours::frame.withAlpha (0.45f));
            const float mark =
                (float) bar.getBottom() - positionOf (-6.0f) * height;
            g.drawHorizontalLine ((int) mark, (float) bar.getX(),
                                  (float) bar.getRight());
        }
    }
}

// The band above the keys: which tone each key reaches, drawn on the keys
// themselves rather than left to be inferred from two combo boxes at the
// other end of the strip. In SPLIT it is the only place on the panel that
// shows where the split point actually falls.
void SeptumAudioProcessorEditor::paintKeyboardZones (juce::Graphics& g)
{
    if (keyZoneBounds.isEmpty())
        return;

    const auto raw = [this] (const char* id)
    {
        const auto* value = processor.parameters.getRawParameterValue (id);
        return value != nullptr ? (int) std::lround (value->load()) : 0;
    };
    const int mode = raw ("keyboard_mode");
    const bool partUpper = raw ("keyboard_part") == 0;

    const auto zone = [&] (juce::Rectangle<int> area, bool upper,
                           const juce::String& text)
    {
        if (area.getWidth() <= 0)
            return;
        const auto colour = upper ? colours::toneUpper : colours::toneLower;
        // The tone the panel is editing is drawn solid; the other is a wash,
        // so the keys say both what sounds and what is being edited.
        const bool edited = upper == editingUpper;
        g.setColour (edited ? colour : colour.withAlpha (0.28f));
        g.fillRoundedRectangle (area.toFloat().reduced (0.5f, 0.0f), 2.0f);
        g.setColour (edited ? juce::Colours::white
                            : colours::frame.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (text, area, juce::Justification::centred);
    };

    if (mode == 0)
    {
        zone (keyZoneBounds, partUpper, partUpper ? "UPPER" : "LOWER");
    }
    else if (mode == 1)
    {
        // Layered: both tones on every key, one stripe each.
        auto area = keyZoneBounds;
        zone (area.removeFromTop (area.getHeight() / 2), true, "UPPER");
        zone (area, false, "LOWER");
    }
    else
    {
        // The split point is a note number; the keyboard knows where that key
        // starts, so the boundary is drawn where the player will hear it.
        // getKeyStartPosition is already in the keyboard's own coordinates.
        const int splitNote = raw ("split_point");
        const int boundary =
            keyboard.getX()
            + juce::roundToInt (keyboard.getKeyStartPosition (splitNote));
        auto area = keyZoneBounds;
        const int cut = juce::jlimit (area.getX(), area.getRight(), boundary);
        auto lower = area.withRight (cut);
        auto upper = area.withLeft (cut);
        zone (lower, false, "LOWER");
        zone (upper, true, "UPPER");
        const auto caption = getSplitPointCaption();
        g.setColour (colours::frame);
        g.fillRect (cut - 1, keyZoneBounds.getY(), 2, keyZoneBounds.getHeight());
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText (caption.text, caption.bounds,
                    caption.bounds.getX() < cut ? juce::Justification::centredRight
                                                : juce::Justification::centredLeft);
    }
}

SeptumAudioProcessorEditor::SplitPointCaption
SeptumAudioProcessorEditor::getSplitPointCaption() const
{
    SplitPointCaption caption;
    if (keyZoneBounds.isEmpty())
        return caption;
    const auto* value = processor.parameters.getRawParameterValue ("split_point");
    const int splitNote = value != nullptr ? (int) std::lround (value->load()) : 60;
    // Named the way the keys under it are named. The parameter's own text is
    // fixed at middle C = C4, but the drawn keys are renamed by the octave
    // shift, so at OCT +1 the band said "C4" over the key the keyboard itself
    // prints as C5 — one drawn key with two names fifteen points apart.
    caption.text = juce::MidiMessage::getMidiNoteName (
        splitNote, true, true, keyboard.getOctaveForMiddleC());

    const int boundary = keyboard.getX()
                         + juce::roundToInt (
                             keyboard.getKeyStartPosition (splitNote));
    const int cut =
        juce::jlimit (keyZoneBounds.getX(), keyZoneBounds.getRight(), boundary);
    // SPLIT POINT reaches C8 while the drawn keyboard stops at C7, so for the
    // top twelve settings the boundary sits at the right edge of the band and a
    // name drawn to its right left the panel entirely. It flips to the left of
    // the line there, the way a tooltip does.
    constexpr int nameWidth = 34;
    const bool flip = cut + 3 + nameWidth > keyZoneBounds.getRight();
    caption.bounds = juce::Rectangle<int> (flip ? cut - 3 - nameWidth : cut + 3,
                                           keyZoneBounds.getY(), nameWidth,
                                           keyZoneBounds.getHeight());
    return caption;
}

juce::StringArray SeptumAudioProcessorEditor::getSectionsOverflowingTheirWell() const
{
    juce::StringArray overflowing;
    for (const auto& entry : sections)
    {
        if (entry->bounds.isEmpty())
            continue;
        for (const auto* control : entry->controls)
        {
            const auto fits = [&entry] (const juce::Component* part)
            {
                return part == nullptr || part->getBounds().isEmpty()
                       || entry->bounds.contains (part->getBounds());
            };
            if (! fits (control->component.get()) || ! fits (control->label.get())
                || ! fits (control->value.get()))
            {
                overflowing.addIfNotAlreadyThere (entry->title);
                break;
            }
        }
    }
    return overflowing;
}

juce::StringArray SeptumAudioProcessorEditor::getSectionTitles() const
{
    juce::StringArray titles;
    for (const auto& entry : sections)
        titles.add (entry->title);
    return titles;
}

void SeptumAudioProcessorEditor::timerCallback()
{
    meterLevel[0] = processor.getOutputLevel (0);
    meterLevel[1] = processor.getOutputLevel (1);
    voiceLabel.setText (juce::String (processor.getActiveVoiceCount())
                            + " / 10 VOICES",
                        juce::dontSendNotification);
    applyKeyboardOctave();
    // KEYBOARD, PART and SPLIT POINT are automatable and a program change
    // moves all three, so what the panel says about the tones has to follow
    // the parameters rather than only the edit tabs. Repainted only when one
    // of them has actually moved.
    const auto reading = [this] (const char* id)
    {
        const auto* value = processor.parameters.getRawParameterValue (id);
        return value != nullptr ? (int) std::lround (value->load()) : 0;
    };
    reconcileEditTarget();
    const juce::String keyState =
        juce::String (reading ("keyboard_mode")) + "/"
        + juce::String (reading ("keyboard_part")) + "/"
        + juce::String (reading ("split_point"));
    if (keyState != lastKeyboardState)
    {
        lastKeyboardState = keyState;
        refreshToneTarget();
        canvas.repaint();
    }
    programBox.setSelectedId (processor.getCurrentProgram() + 1,
                              juce::dontSendNotification);
    // juce::Label::setText only repaints when the text actually changes, so
    // this costs nothing on the frames where nothing moved.
    refreshValues();
    if (! meterBounds.isEmpty())
        canvas.repaint (meterBounds.expanded (2));
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

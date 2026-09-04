#include "PluginEditor.h"

#include "DSP/GhostarPresets.h"

namespace
{
namespace ids = ghostar::parameters;

// Original overhead hardware photograph (published June 2014, before the
// reissue and software instruments):
// https://www.matrixsynth.com/2014/06/crumar-spirit-vintage-analog.html
// https://1.bp.blogspot.com/-IFfD8JLxD44/U4toXGGZSrI/AAAAAAAIdXQ/zLg7p19lw3Y/s1600/1.jpg
// Angled original-unit photograph cross-check (July 2001):
// https://www.soundonsound.com/reviews/crumar-spirit-retrozone
// The photos supply the stepped mixer outline, panel proportions, three
// wheels, walnut crossbars, black knob skirts and pale index caps. Artwork
// below is drawn from geometry; no third-party photograph or logo is shipped.
const juce::Colour panelCharcoal { 0xff242526 };
const juce::Colour sectionCharcoal { 0xff242526 };
const juce::Colour headerCharcoal { 0xff17191b };
const juce::Colour silkscreen { 0xffeeeae0 };
const juce::Colour silkscreenDim { 0xffc3c1b8 };
const juce::Colour knobFace { 0xff94948f };
const juce::Colour knobFaceTop { 0xffc1c0b7 };
const juce::Colour knobRim { 0xff555651 };
const juce::Colour markingBlack { 0xff111315 };
const juce::Colour controlBody { 0xff303334 };
const juce::Colour controlRim { 0xff666962 };
const juce::Colour hairline { 0xff73756f };
const juce::Colour lampLit { 0xffef5146 };
const juce::Colour lampDark { 0xff3d2421 };
const juce::Colour walnutDark { 0xff422519 };
const juce::Colour walnutLight { 0xff825237 };
const juce::Colour focusGold { 0xffffd36a };

// The panel's design geometry. Everything is laid out and painted against
// these numbers; the window scales the result.
constexpr int editorWidth = 1460;
constexpr int editorHeight = 780;
// 60 % of the design size, where the smallest silkscreen is still about
// 7 points. Below that the panel would be legible only by tooltip.
constexpr int minimumWidth = 876;
constexpr int minimumHeight = 468;
// What a host or a desktop window puts around the editor: a frame either
// side, a title bar and a frame above and below. An allowance rather than a
// measurement — it only has to be generous enough that the panel is not the
// thing that overflows.
constexpr int windowChromeWidth = 32;
constexpr int windowChromeHeight = 64;

// A control's caption shows its value for about this long after it stops
// moving, then returns to naming itself.
constexpr int readoutHoldTicks = 12;   // ~0.8 s at the editor's 15 Hz

// The rotary travel the panel's pointers sweep: the usual three-quarter turn.
constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float rotaryEnd = juce::MathConstants<float>::pi * 2.75f;

juce::String travelText(double normalised)
{
    return juce::String(normalised * 10.0, 1);
}
} // namespace

GhostarLookAndFeel::GhostarLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, panelCharcoal);
    setColour(juce::Label::textColourId, silkscreen);
    setColour(juce::Slider::textBoxTextColourId, silkscreen);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, controlBody);
    setColour(juce::ComboBox::textColourId, silkscreen);
    setColour(juce::ComboBox::outlineColourId, controlRim);
    setColour(juce::ComboBox::arrowColourId, silkscreen);
    setColour(juce::PopupMenu::backgroundColourId, sectionCharcoal);
    setColour(juce::PopupMenu::textColourId, silkscreen);
    setColour(juce::PopupMenu::headerTextColourId, silkscreenDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour { 0xff5a5a5e });
    setColour(juce::PopupMenu::highlightedTextColourId, silkscreen);
    setColour(juce::TextButton::buttonColourId, controlBody);
    setColour(juce::TextButton::textColourOffId, silkscreen);
    setColour(juce::TextButton::textColourOnId, silkscreen);
    setColour(juce::TooltipWindow::backgroundColourId,
              juce::Colour { 0xff2b2b2d });
    setColour(juce::TooltipWindow::textColourId, silkscreen);
    setColour(juce::TooltipWindow::outlineColourId, controlRim);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
              juce::Colour { 0xffe9e9e6 });
    setColour(juce::MidiKeyboardComponent::blackNoteColourId,
              juce::Colour { 0xff141416 });
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
              juce::Colour { 0xff8a8a88 });
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              juce::Colour { 0x30000000 });
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              juce::Colour { 0x88000000 });
    setColour(juce::MidiKeyboardComponent::textLabelColourId,
              juce::Colour { 0xff55555c });
}

namespace
{
void paintPanelKnob(juce::Graphics& g, juce::Point<float> centre, float radius,
                    float position, float start, float end, int divisions,
                    bool bipolar, bool numbered = true)
{
    const float angle = start + position * (end - start);
    g.setColour(silkscreenDim);
    for (int i = 0; i <= divisions; ++i)
    {
        const float tick = start + static_cast<float>(i) / divisions * (end - start);
        const auto outer = centre.getPointOnCircumference(radius, tick);
        const auto inner = centre.getPointOnCircumference(radius - 3.0f, tick);
        g.drawLine({ inner, outer }, 1.0f);
        if (numbered && divisions == 10 && i % 2 == 0 && radius >= 24.0f)
        {
            const auto number = centre.getPointOnCircumference(radius + 6.0f, tick);
            g.setFont(juce::FontOptions { 8.0f });
            g.drawText(juce::String(i),
                       juce::Rectangle<float>(15.0f, 10.0f).withCentre(number),
                       juce::Justification::centred);
        }
    }
    if (bipolar)
    {
        g.setColour(silkscreen);
        g.drawEllipse(centre.x - 2.0f, centre.y - radius - 3.0f, 4.0f, 4.0f, 1.0f);
    }

    const auto skirt = juce::Rectangle<float>(radius * 1.53f, radius * 1.53f)
                           .withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(skirt.translated(1.0f, 3.0f).expanded(1.5f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff414342),
        skirt.getX(), skirt.getY(), markingBlack,
        skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse(skirt);
    g.setColour(juce::Colour(0xff555753));
    for (int i = 0; i < 28; ++i)
    {
        const float rib = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 28;
        g.drawLine({ centre.getPointOnCircumference(radius * 0.72f, rib),
                     centre.getPointOnCircumference(radius * 0.65f, rib) }, 0.7f);
    }
    const auto face = skirt.reduced(radius * 0.23f);
    g.setGradientFill(juce::ColourGradient(knobFaceTop, face.getX(), face.getY(),
                                          knobFace, face.getRight(), face.getBottom(), false));
    g.fillEllipse(face);
    g.setColour(knobRim);
    g.drawEllipse(face, 1.0f);
    g.setColour(markingBlack);
    g.drawLine({ centre.getPointOnCircumference(radius * 0.18f, angle),
                 centre.getPointOnCircumference(radius * 0.52f, angle) }, 2.0f);
}

void paintPaddle(juce::Graphics& g, juce::Rectangle<float> body, bool up)
{
    g.setColour(markingBlack);
    g.fillRoundedRectangle(body.expanded(2.0f), 1.5f);
    g.setGradientFill(juce::ColourGradient(up ? knobFaceTop : knobRim,
        body.getCentreX(), body.getY(), up ? knobRim : knobFaceTop,
        body.getCentreX(), body.getBottom(), false));
    g.fillRect(body);
    const auto activeHalf = up ? body.withHeight(body.getHeight() * 0.5f)
                              : body.withTrimmedTop(body.getHeight() * 0.5f);
    g.setColour(juce::Colour(0xffb3bec3).withAlpha(0.8f));
    g.fillRect(activeHalf.reduced(1.0f, 4.0f));
    g.setColour(knobFaceTop);
    g.drawHorizontalLine(juce::roundToInt(body.getCentreY()), body.getX(), body.getRight());
}
} // namespace

void GhostarLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                        int width, int height, float position,
                                        float start, float end,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(35.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.46f);
    // Rear source controls use actual volts or resistance in their readout,
    // so the front panel's 0–10 pot scale would be misleading on them.
    const bool physicalUnits = static_cast<bool>(slider.getProperties()["volts"])
                            || static_cast<bool>(slider.getProperties()["kilohms"]);
    paintPanelKnob(g, bounds.getCentre(), radius, position, start, end, 10,
                    static_cast<bool>(slider.getProperties()["bipolar"]), !physicalUnits);
    if (slider.hasKeyboardFocus(false))
    {
        g.setColour(focusGold);
        g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.1f, radius * 2.1f), 1.5f);
    }
}

void GhostarLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                        int width, int height, float sliderPos,
                                        float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                         minSliderPos, maxSliderPos, style, slider);
        return;
    }
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    if (static_cast<bool>(slider.getProperties()["performanceWheel"]))
    {
        const auto wheel = juce::Rectangle<float>(cx - 11.0f, static_cast<float>(y),
                                                  22.0f, static_cast<float>(height));
        g.setColour(markingBlack);
        g.fillRoundedRectangle(wheel.expanded(3.0f), 9.0f);
        juce::ColourGradient contour(markingBlack, cx, wheel.getY(), markingBlack,
                                     cx, wheel.getBottom(), false);
        contour.addColour(0.28, juce::Colour(0xff45484a));
        contour.addColour(0.5, juce::Colour(0xff25282a));
        contour.addColour(0.78, juce::Colour(0xff3c3f40));
        g.setGradientFill(contour);
        g.fillRoundedRectangle(wheel, 7.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        for (float yy = wheel.getY() + 7.0f; yy < wheel.getBottom() - 5.0f; yy += 5.0f)
            g.drawHorizontalLine(juce::roundToInt(yy), wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
        g.setColour(knobFaceTop);
        g.fillRect(wheel.getX() + 3.0f, sliderPos - 1.0f, wheel.getWidth() - 6.0f, 2.0f);
    }
    else
    {
        const auto slot = juce::Rectangle<float>(cx - 2.0f, static_cast<float>(y),
                                                 4.0f, static_cast<float>(height));
        const bool numbered = static_cast<bool>(slider.getProperties()["scaleNumbers"]);
        g.setColour(silkscreenDim);
        for (int i = 0; i <= 5; ++i)
        {
            const float yy = slot.getBottom() - static_cast<float>(i) * slot.getHeight() / 5;
            g.drawHorizontalLine(juce::roundToInt(yy), cx - (numbered ? 8.0f : 14.0f), cx + 14.0f);
        }
        if (numbered)
        {
            g.setFont(juce::FontOptions { 8.0f });
            for (int number : { 0, 5, 10 })
            {
                const float yy = slot.getBottom() - number * slot.getHeight() / 10.0f;
                g.drawText(juce::String(number),
                           juce::Rectangle<float>(juce::jmax(0.0f, cx - 21.0f), yy - 5.0f, 11.0f, 10.0f),
                           juce::Justification::centred);
            }
        }
        g.setColour(markingBlack);
        g.fillRect(slot.expanded(1.0f, 0.0f));
        g.setColour(juce::Colour(0xff9b8855));
        g.drawVerticalLine(juce::roundToInt(cx - 1.0f), slot.getY(), slot.getBottom());
        const auto cap = juce::Rectangle<float>(cx - 13.0f, sliderPos - 6.0f, 26.0f, 12.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(cap.translated(1.0f, 2.0f).expanded(1.0f), 2.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff484b49), cx, cap.getY(),
                                              markingBlack, cx, cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(knobFaceTop);
        g.drawHorizontalLine(juce::roundToInt(sliderPos), cap.getX() + 3.0f, cap.getRight() - 3.0f);
    }
    if (slider.hasKeyboardFocus(false))
    {
        g.setColour(focusGold);
        g.drawRoundedRectangle(slider.getLocalBounds().toFloat().reduced(1.0f), 3.0f, 1.5f);
    }
}

void GhostarLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool highlighted, bool)
{
    const bool compact = button.getHeight() < 45;
    const auto paddle = button.getLocalBounds().toFloat().withSizeKeepingCentre(
        compact ? 23.0f : 30.0f, compact ? 23.0f : 52.0f);
    paintPaddle(g, paddle, button.getToggleState());
    g.setColour(silkscreenDim);
    g.setFont(juce::FontOptions { compact ? 8.0f : 9.0f });
    if (compact)
    {
        g.drawText(button.getToggleState() ? "ON" : "OFF",
                   button.getLocalBounds().withWidth(25), juce::Justification::centredLeft);
    }
    else
    {
        g.drawText("ON", button.getLocalBounds().withHeight(12), juce::Justification::centred);
        g.drawText("OFF", button.getLocalBounds().withTrimmedTop(button.getHeight() - 12),
                   juce::Justification::centred);
    }
    if (highlighted || button.hasKeyboardFocus(false))
    {
        g.setColour(button.hasKeyboardFocus(false) ? focusGold : silkscreen);
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.0f), 2.0f, 1.0f);
    }
}

void GhostarLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                    bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    if (static_cast<bool>(box.getProperties()["panelSelector"]))
    {
        if (box.getNumItems() == 2)
        {
            const auto body = bounds.withSizeKeepingCentre(30.0f, 48.0f);
            paintPaddle(g, body, box.getSelectedItemIndex() == 0);
            g.setFont(juce::FontOptions { 9.0f });
            g.setColour(silkscreenDim);
            g.drawText(box.getItemText(0).toUpperCase(), bounds.withHeight(13.0f),
                       juce::Justification::centred);
            g.drawText(box.getItemText(1).toUpperCase(), bounds.withTrimmedTop(height - 13.0f),
                       juce::Justification::centred);
        }
        else
        {
            const auto dial = bounds.withTrimmedBottom(17.0f);
            const float radius = juce::jmin(33.0f, juce::jmin(dial.getWidth(), dial.getHeight()) * 0.45f);
            paintPanelKnob(g, dial.getCentre(), radius,
                static_cast<float>(juce::jmax(0, box.getSelectedItemIndex())) / juce::jmax(1, box.getNumItems() - 1),
                rotaryStart, rotaryEnd, juce::jmax(1, box.getNumItems() - 1), false);
        }
        if (box.isMouseOver() || box.hasKeyboardFocus(false))
        {
            g.setColour(box.hasKeyboardFocus(false) ? focusGold : silkscreenDim);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 2.0f, 1.0f);
        }
        return;
    }
    g.setColour(controlBody);
    g.fillRoundedRectangle(bounds.reduced(0.5f), 3.0f);
    g.setColour(box.hasKeyboardFocus(false) ? focusGold : controlRim);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle(width - 16.0f, height * 0.4f, width - 8.0f,
                       height * 0.4f, width - 12.0f, height * 0.65f);
    g.setColour(silkscreenDim);
    g.fillPath(arrow);
}

juce::Font GhostarLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font { juce::FontOptions {
        static_cast<bool>(box.getProperties()["panelSelector"]) ? 10.5f : 12.0f } };
}

void GhostarLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    if (static_cast<bool>(box.getProperties()["panelSelector"]))
    {
        // The native label remains the accessible current value. Two-position
        // paddles have both legends painted above/below their physical throws.
        label.setBounds(box.getLocalBounds().withTrimmedTop(box.getHeight() - 17));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,
                        box.getNumItems() == 2 ? juce::Colours::transparentBlack : silkscreen);
        label.setFont(getComboBoxFont(box));
    }
    else
        LookAndFeel_V4::positionComboBoxText(box, label);
}

void GhostarLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(shouldDrawButtonAsDown ? knobRim : controlBody);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? silkscreen : controlRim);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void PanelKeyboard::drawBlackNote(int, juce::Graphics& g,
                                  juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour noteFillColour)
{
    auto colour = noteFillColour;
    if (isDown)
        colour = colour.overlaidWith(findColour(keyDownOverlayColourId));
    else if (isOver)
        colour = colour.overlaidWith(
            findColour(mouseOverKeyOverlayColourId));

    g.setColour(colour);
    g.fillRect(area);
    // A hairline down the sides only, so adjacent sharps stay separable
    // without the face lifting off black.
    g.setColour(juce::Colour { 0xff2e2e30 });
    g.drawRect(area, 1.0f);
}

GhostarAudioProcessorEditor::GhostarAudioProcessorEditor(
    GhostarAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    addKnob(tune, ids::tune, "TUNE",
            "Master tune: a minor third either side of centre.");
    tune.slider.getProperties().set("bipolar", true);
    addSelector(octave, ids::octave, "OCTAVE",
                "Master footage. At 8' the keyboard's second C sounds middle "
                "C.");
    addSelector(oscAWaveform, ids::oscAWaveform, "WAVEFORM",
                "Osc A's waveform. The rectangles are the panel's own duty "
                "cycles: 50, 30, 15 and 6 per cent.");
    addRocker(sync, ids::sync, "SYNC",
              "Hard sync: Osc A resets Osc B, so B follows A's pitch and its "
              "own tuning becomes timbre. Tune B above A to hear it.");
    addSelector(oscBWaveform, ids::oscBWaveform, "WAVEFORM",
                "Osc B's waveform. Its rectangles are narrower than A's: 40, "
                "20, 10 and 3 per cent.");
    addSelector(oscBRange, ids::oscBRange, "OCTAVE/RANGE",
                "Osc B's octave against A, or a drone range. BASS and WIDE "
                "disconnect B from the keyboard entirely and hand its pitch "
                "to INTERVAL.");
    addKnob(interval, ids::interval, "INTERVAL",
            "Osc B's detune: a perfect fifth either side of centre. Small "
            "offsets are the warmth the manual asks for. In BASS or WIDE it "
            "becomes B's drone pitch instead.");
    interval.slider.getProperties().set("bipolar", true);
    addSelector(trigger, ids::trigger, "TRIGGER",
                "MULTIPLE re-gates on every new key; SINGLE holds the gate "
                "while any key is down and re-gates only after all are "
                "released.");
    addRocker(gateKbd, ids::gateKbd, "KBD",
              "Gate the envelopes from the keyboard.");
    addRocker(gateX, ids::gateX, "X",
              "Gate the envelopes from the LFO square: auto-repeat at the "
              "MOD X rate, one gate per arpeggiator step.");
    addRocker(gateYExt, ids::gateYExt, "Y/EXT",
              "Gate the envelopes from the Shaper's own comparator while "
              "EXT GATE is unplugged, or from the inserted external gate. "
              "At least one gate source must be on or the envelopes never "
              "run.");
    addSelector(arpeggiator, ids::arpeggiator, "ARPEGGIATOR",
                "Scans held keys bottom to top on the LFO clock. RIPPLE is "
                "the plain sequence; ARPEGGIO repeats it up and down an "
                "octave; LEAP cycles the octave per note.");
    addSelector(modSource, ids::modSource, "MOD SOURCE",
                "What rides the MOD X bus. OSC B is the selected Osc B "
                "waveform itself — audio-rate modulation when B is in WIDE.");
    addKnob(lfoRate, ids::lfoRate, "LFO/S+H RATE",
            "The LFO rate, and with it the sample-and-hold clock and the "
            "arpeggiator's tempo. No effect on RED NOISE or OSC B.");
    addSelector(shaperMode, ids::shaperMode, "MODE",
                "FREE runs the Shaper as an LFO; KBD HOLD rises and holds "
                "while gated; RESET restarts on every key; RUN always "
                "finishes its rise before accepting a new gate.");
    addKnob(shaperShape, ids::shaperShape, "SHAPE",
            "How the Shaper's period splits between rise and fall, without "
            "changing the period itself. Fully left is a fast rise and slow "
            "fall; fully right the reverse.");
    addKnob(shaperRate, ids::shaperRate, "RATE",
            "The Shaper's total period. In the envelope modes it is the "
            "whole rise-plus-fall time.");
    addSelector(modXTo, ids::modXTo, "MOD X TO:",
                "Where the MOD X wheel's signal lands. RWM is pulse-width "
                "modulation of the rectangular waveforms.");
    addRocker(shapeXWithY, ids::shapeXWithY, "SHAPE X WITH Y",
              "The Shaper envelopes the MOD X signal — vibrato that fades in "
              "rather than arriving whole.");
    addSelector(shaperYTo, ids::shaperYTo, "SHAPER Y TO:",
                "Where the SHAPER Y wheel's signal lands. At LFO RATE the "
                "wheel sets the fastest rate and the panel knob the "
                "slowest.");
    addKnob(masterVolume, ids::masterVolume, "MASTER VOLUME",
            "Dual 20 kΩ linear level control. The normalled main jack lets "
            "the two loaded wipers interact; SPLIT isolates them.");
    addKnob(brightness, ids::brightness, "BRIGHTNESS",
            "The passive post-VCA Shaper tone network. At zero it is a "
            "294.7 Hz lowpass; fully up retains a gentle -1.58 dB high "
            "shelf rather than bypassing.");
    addFader(shaperPathA, ids::shaperPathA, "A",
             "Osc A into the Shaper path.");
    addFader(shaperPathB, ids::shaperPathB, "B",
             "Osc B into the Shaper path.");
    addFader(shaperPathRing, ids::shaperPathRing, "RING",
             "The ring modulator into the Shaper path — the only path it "
             "reaches. It multiplies the two triangles, whatever the "
             "waveform switches say.");
    addFader(shaperPathNoise, ids::shaperPathNoise, "NOISE",
             "Noise into the Shaper path.");
    addFader(filterPathA, ids::filterPathA, "A",
             "Osc A into the filter path.");
    addFader(filterPathB, ids::filterPathB, "B",
             "Osc B into the filter path.");
    addFader(filterPathNoise, ids::filterPathNoise, "NOISE",
             "Noise into the filter path.");
    addKnob(cutoff, ids::cutoff, "MASTER",
            "Both filters' cutoff, always — the Lower section moves with it "
            "and LOWER ONLY sets the offset.");
    addKnob(lowerOnly, ids::lowerOnly, "LOWER ONLY",
            "The Lower filter's cutoff relative to the Upper. The two "
            "coincide at 8; below that the Lower sits underneath.");
    addSelector(upperResonance, ids::upperResonance, "RESONANCE",
                "LOW fixes the Upper filter's Q at 0.5; VARIABLE hands it to "
                "the resonance knob alongside the Lower.");
    addKnob(resonance, ids::resonance, "RESONANCE",
            "The Lower filter's Q always, and the Upper's in VARIABLE. Gentle "
            "through the middle of the travel, then steep — and "
            "self-oscillating at maximum.");
    addSelector(slope, ids::slope, "SLOPE",
                "The Upper filter's slope: one section or two cascaded.");
    addKnob(kbAmount, ids::kbAmount, "KB AMOUNT",
            "How far the cutoff follows the keyboard — slightly over 100 per "
            "cent at full travel.");
    addSelector(lowerMode, ids::lowerMode, "SLOPE/MODE",
                "The Lower section. BAND-PASS is a parametric boost, not a "
                "band-pass; OVERDRIVE adds the diode clipper between the "
                "filters; HIGH PASS gives the double-peak register.");
    addSelector(tracking, ids::tracking, "TRACKING",
                "FORMANT freezes the Lower filter's peak — no keyboard, no "
                "envelope, no wheels — as a fixed vocal formant. DYNAMIC "
                "lets it move.");
    addKnob(filterEnvAmount, ids::filterEnvAmount, "AMOUNT",
            "How far the filter envelope moves the cutoff, up to two and a "
            "half octaves either way. Below centre the envelope inverts.");
    filterEnvAmount.slider.getProperties().set("bipolar", true);
    addFader(filterAttack, ids::filterAttack, "A", "Filter envelope attack.");
    addFader(filterDecay, ids::filterDecay, "D", "Filter envelope decay.");
    addFader(filterSustain, ids::filterSustain, "S",
             "Filter envelope sustain level.");
    addFader(filterRelease, ids::filterRelease, "R",
             "Filter envelope release.");
    addRocker(vcaBypass, ids::vcaBypass, "VCA BYPASS",
              "Holds the filter path's VCA fully open, so the path drones "
              "without waiting for a key.");
    addFader(loudnessAttack, ids::loudnessAttack, "A",
             "Loudness envelope attack.");
    addFader(loudnessDecay, ids::loudnessDecay, "D",
             "Loudness envelope decay.");
    addFader(loudnessSustain, ids::loudnessSustain, "S",
             "Loudness envelope sustain level.");
    addFader(loudnessRelease, ids::loudnessRelease, "R",
             "Loudness envelope release.");
    // One numbered reference per fader bank keeps the original 0–10
    // silkscreen readable without repeating tiny numerals on every slot.
    for (auto* fader : { &shaperPathA, &filterPathA, &filterAttack, &loudnessAttack })
        fader->slider.getProperties().set("scaleNumbers", true);
    addKnob(glide, ids::glide, "GLIDE",
            "Portamento time between notes.");
    addSelector(glideMode, ids::glideMode, "GLIDE MODE",
                "AUTO glides only while more than one key is held, so legato "
                "slides and separate notes do not.");
    addFader(xWheel, ids::xWheel, "MOD X",
            "The MOD X performance wheel. It attenuates toward zero, so a "
            "bipolar source keeps its symmetry.");
    addFader(yWheel, ids::yWheel, "SHAPER Y",
            "The SHAPER Y performance wheel.");
    addRocker(splitPaths, ids::splitPaths, "SPLIT",
              "Sends the filter path left and the Shaper path right, as the "
              "hardware's two rear jacks do, instead of mixing them.");
    addSelector(externalGate, ids::externalGate, "EXT GATE",
                "Rear switching jack: UNPLUGGED uses the normalled Shaper Y "
                "gate; LOW inserts a cable at 6 V or below; HIGH supplies "
                "the greater-than-6 V external gate the hardware accepts. "
                "This is a saved jack state, not an audio-rate CV input.");
    addSelector(externalPitchConnected, ids::externalPitchConnected,
                "EXT PITCH",
                "Rear switching jack: PLUGGED replaces keyboard and "
                "arpeggiator pitch with PITCH CV before GLIDE. The keyboard "
                "still gates and triggers the instrument.");
    addKnob(externalPitchVolts, ids::externalPitchVolts, "PITCH CV",
            "Keyboard Pitch Out-equivalent source voltage behind "
            "15 kΩ. The rear input requires 1.1 V per octave; 0 V is its "
            "keyboard-CV cancellation point near the second C. This saved "
            "host control is block-latched, not an audio-rate CV lane.");
    externalPitchVolts.slider.getProperties().set("bipolar", true);
    externalPitchVolts.slider.getProperties().set("volts", true);
    addRocker(oscBPedalConnected, ids::oscBPedalConnected, "B JACK",
              "Insert the rear OSC B PEDAL cable. The unswitched TS jack is "
              "electrically open when this is off; an inserted pedal adds "
              "its tip-to-sleeve resistance to the 33 kΩ/100 nF input.");
    addKnob(oscBPedalResistance, ids::oscBPedalResistance, "B PEDAL",
            "Tip-to-sleeve resistance of the documented 100 kΩ OSC B "
            "pedal: zero is a short and 100 kΩ is maximum resistance. "
            "It changes Osc B only, including in BASS and WIDE. Pedal "
            "direction and taper are not specified in the manual.");
    oscBPedalResistance.slider.getProperties().set("kilohms", true);
    addRocker(filterPedalConnected, ids::filterPedalConnected, "F JACK",
              "Insert the rear FILTER PEDAL cable. The unswitched TS jack "
              "is electrically open when this is off; the 100 nF input "
              "keeps its charge across cable and panic changes.");
    addKnob(filterPedalResistance, ids::filterPedalResistance, "F PEDAL",
            "Tip-to-sleeve resistance of the documented 100 kΩ FILTER "
            "pedal: zero is a short and 100 kΩ is maximum resistance. "
            "It moves Upper always and Lower only in DYNAMIC. Pedal "
            "direction and taper are not specified in the manual.");
    filterPedalResistance.slider.getProperties().set("kilohms", true);
    addSelector(externalAudioConnected, ids::externalAudioConnected,
                "EXT AUDIO",
                "Rear switching jack: UNPLUGGED sends internal PINK NOISE "
                "to both NOISE sliders; PLUGGED replaces it with the optional "
                "mono host input. A connected silent or unrouted bus remains "
                "silence. Host +/-1 maps provisionally to +/-5 V. In the "
                "Standalone app, uncheck Mute audio input in Audio/MIDI "
                "Settings.");

    // The spring-loaded bend wheel: vertical travel, snapping back to centre.
    pitchWheel.setSliderStyle(juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    pitchWheel.setRange(-1.0, 1.0, 0.0);
    pitchWheel.setValue(0.0, juce::dontSendNotification);
    pitchWheel.setDoubleClickReturnValue(true, 0.0);
    pitchWheel.setTooltip("Pitch bend, spring-loaded: it springs back to "
                          "centre when released. Full travel is eight "
                          "semitones.");
    // A scroll is not a drag, so onDragEnd would never spring the wheel
    // back and the bend would latch; the spring only answers to dragging.
    pitchWheel.setScrollWheelEnabled(false);
    pitchWheel.setName("PITCH BEND");
    pitchWheel.setWantsKeyboardFocus(true);
    pitchWheel.getProperties().set("performanceWheel", true);
    xWheel.slider.getProperties().set("performanceWheel", true);
    yWheel.slider.getProperties().set("performanceWheel", true);
    pitchWheel.onValueChange = [this] {
        processor.setUiPitchBend(static_cast<float>(pitchWheel.getValue()));
    };
    pitchWheel.onDragEnd = [this] {
        pitchWheel.setValue(0.0, juce::sendNotificationSync);
    };
    canvas.addAndMakeVisible(pitchWheel);
    pitchWheelLabel.setText("BEND", juce::dontSendNotification);
    pitchWheelLabel.setFont(juce::FontOptions { 11.0f });
    pitchWheelLabel.setJustificationType(juce::Justification::centred);
    pitchWheelLabel.setColour(juce::Label::textColourId, silkscreenDim);
    canvas.addAndMakeVisible(pitchWheelLabel);

    panicButton.setTooltip("Stops every sounding voice at once, including "
                           "drones held open by VCA BYPASS.");
    panicButton.onClick = [this] { processor.requestPanic(); };
    canvas.addAndMakeVisible(panicButton);

    connectionsButton.setClickingTogglesState(true);
    connectionsButton.setTooltip("Show rear audio, gate, pitch and pedal connections.");
    connectionsButton.onClick = [this] {
        const bool showing = connectionsButton.getToggleState();
        rearConnections.setVisible(showing);
        keyboard.setVisible(!showing);
        canvas.repaint();
    };
    canvas.addAndMakeVisible(connectionsButton);
    canvas.addChildComponent(rearConnections);
    rearConnections.setName("Rear connections");
    for (auto* selector : { &externalGate, &externalPitchConnected, &externalAudioConnected })
    {
        selector->box.getProperties().set("panelSelector", false);
        rearConnections.addAndMakeVisible(selector->box);
        rearConnections.addAndMakeVisible(selector->label);
    }
    for (auto* knob : { &externalPitchVolts, &oscBPedalResistance, &filterPedalResistance })
    {
        rearConnections.addAndMakeVisible(knob->slider);
        rearConnections.addAndMakeVisible(knob->label);
    }
    for (auto* rocker : { &oscBPedalConnected, &filterPedalConnected, &splitPaths })
    {
        rearConnections.addAndMakeVisible(rocker->button);
        rearConnections.addAndMakeVisible(rocker->label);
    }

    const auto addCaption = [this](juce::Label& label,
                                   const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions { 11.0f, juce::Font::bold });
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, silkscreenDim);
        label.setInterceptsMouseClicks(false, false);
        canvas.addAndMakeVisible(label);
    };
    addCaption(shaperPathCaption, "SHAPER Y PATH");
    addCaption(filterPathCaption, "FILTER PATH");

    wordmark.setText("GHOSTAR", juce::dontSendNotification);
    wordmark.setFont(juce::FontOptions { 23.0f, juce::Font::bold });
    wordmark.setColour(juce::Label::textColourId, silkscreen);
    wordmark.setJustificationType(juce::Justification::centredLeft);
    canvas.addAndMakeVisible(wordmark);

    subtitle.setText("monophonic dual-filter synthesizer",
                     juce::dontSendNotification);
    subtitle.setFont(juce::FontOptions { 11.0f });
    subtitle.setColour(juce::Label::textColourId, silkscreenDim);
    subtitle.setJustificationType(juce::Justification::centredLeft);
    canvas.addAndMakeVisible(subtitle);

    previousProgram.setTooltip("Previous program.");
    previousProgram.onClick = [this] { stepProgram(-1); };
    canvas.addAndMakeVisible(previousProgram);
    nextProgram.setTooltip("Next program.");
    nextProgram.onClick = [this] { stepProgram(1); };
    canvas.addAndMakeVisible(nextProgram);
    programName.onClick = [this] { showProgramMenu(); };
    canvas.addAndMakeVisible(programName);
    programBank.setFont(juce::FontOptions { 11.0f });
    programBank.setJustificationType(juce::Justification::centred);
    programBank.setColour(juce::Label::textColourId, silkscreenDim);
    canvas.addAndMakeVisible(programBank);

    keyboard.setAvailableRange(48, 84); // the hardware's 37 keys, C to C
    keyboard.setOctaveForMiddleC(4);
    keyboard.setScrollButtonsVisible(false);
    canvas.addAndMakeVisible(keyboard);

    addAndMakeVisible(canvas);

    refreshProgramDisplay();
    // Every attachment above fired its slider's callback as it took the
    // parameter's standing value; from here a callback means a human.
    wiringUp = false;
    startTimerHz(15);

    // The window may be any size; the panel inside it is always the design
    // geometry, scaled. Limits stop the type shrinking past reading size at
    // one end and the panel going soft at the other.
    setResizable(true, false);
    // setResizeLimits is what installs the default constrainer, so it has to
    // come before the ratio is set on it.
    setResizeLimits(minimumWidth, minimumHeight, editorWidth * 2,
                    editorHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(editorWidth)
                                         / static_cast<double>(editorHeight));
    // Open at the largest whole panel the display can actually show.
    juce::Rectangle<int> workArea;
    if (auto* display =
            juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        workArea = display->userArea;
    const auto opening = panelSizeForWorkArea(workArea);
    setSize(opening.getWidth(), opening.getHeight());
}

juce::Rectangle<int> GhostarAudioProcessorEditor::panelSizeForWorkArea(
    juce::Rectangle<int> workArea)
{
    // Full size wherever it fits. Where it does not — a 1366x768 or 1280x800
    // laptop, or a 1080p panel at 150 % — the whole panel is shrunk to fit
    // rather than having its bottom edge cut off. Never below the size the
    // silkscreen stays readable at: on a screen smaller than that, a window
    // the user can move is a better failure than type nobody can read.
    double scale = 1.0;
    if (!workArea.isEmpty())
        scale = juce::jmin(1.0,
                           static_cast<double>(workArea.getWidth()
                                               - windowChromeWidth)
                               / editorWidth,
                           static_cast<double>(workArea.getHeight()
                                               - windowChromeHeight)
                               / editorHeight);
    scale = juce::jmax(scale, static_cast<double>(minimumWidth) / editorWidth);
    return { juce::roundToInt(editorWidth * scale),
             juce::roundToInt(editorHeight * scale) };
}

GhostarAudioProcessorEditor::~GhostarAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void GhostarAudioProcessorEditor::addKnob(Knob& knob, const char* parameterId,
                                        const juce::String& text,
                                        const juce::String& tooltip)
{
    knob.name = text;
    knob.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.slider.setRotaryParameters(rotaryStart, rotaryEnd, true);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    knob.slider.setTooltip(tooltip);
    knob.slider.setName(text);
    knob.slider.setWantsKeyboardFocus(true);
    knob.slider.setScrollWheelEnabled(false);
    knob.slider.setMouseDragSensitivity(220);
    if (auto* parameter = processor.parameters.getParameter(parameterId))
        knob.slider.setDoubleClickReturnValue(
            true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    knob.slider.onValueChange = [this, &knob] {
        if (wiringUp)
            return;
        knob.readoutTicks = readoutHoldTicks;
        const bool volts = static_cast<bool>(
            knob.slider.getProperties().getWithDefault("volts", false));
        const bool kilohms = static_cast<bool>(
            knob.slider.getProperties().getWithDefault("kilohms", false));
        knob.label.setText(volts ? juce::String(knob.slider.getValue(), 3) + " V"
                           : kilohms
                               ? juce::String(knob.slider.getValue(), 1) + " kΩ"
                               : travelText(knob.slider.getValue()),
                           juce::dontSendNotification);
        knob.label.setColour(juce::Label::textColourId, silkscreen);
    };
    canvas.addAndMakeVisible(knob.slider);
    knob.label.setText(text, juce::dontSendNotification);
    knob.label.setFont(juce::FontOptions { 12.0f });
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, silkscreen);
    knob.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(knob.label);
    knob.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, knob.slider);
}

void GhostarAudioProcessorEditor::addFader(Fader& fader,
                                          const char* parameterId,
                                          const juce::String& text,
                                          const juce::String& tooltip)
{
    fader.name = text;
    fader.slider.setSliderStyle(juce::Slider::LinearVertical);
    fader.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    fader.slider.setTooltip(tooltip);
    fader.slider.setName(text);
    fader.slider.setWantsKeyboardFocus(true);
    fader.slider.setScrollWheelEnabled(false);
    if (auto* parameter = processor.parameters.getParameter(parameterId))
        fader.slider.setDoubleClickReturnValue(true,
                                               parameter->getDefaultValue());
    fader.slider.onValueChange = [this, &fader] {
        if (wiringUp)
            return;
        fader.readoutTicks = readoutHoldTicks;
        fader.label.setText(travelText(fader.slider.getValue()),
                            juce::dontSendNotification);
        fader.label.setColour(juce::Label::textColourId, silkscreen);
    };
    canvas.addAndMakeVisible(fader.slider);
    fader.label.setText(text, juce::dontSendNotification);
    fader.label.setFont(juce::FontOptions { 11.0f });
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setColour(juce::Label::textColourId, silkscreen);
    fader.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(fader.label);
    fader.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, parameterId, fader.slider);
}

void GhostarAudioProcessorEditor::addRocker(Rocker& rocker,
                                          const char* parameterId,
                                          const juce::String& text,
                                          const juce::String& tooltip)
{
    rocker.button.setTooltip(tooltip);
    rocker.button.setName(text);
    rocker.button.setWantsKeyboardFocus(true);
    canvas.addAndMakeVisible(rocker.button);
    rocker.label.setText(text, juce::dontSendNotification);
    rocker.label.setFont(juce::FontOptions { 12.0f });
    rocker.label.setJustificationType(juce::Justification::centred);
    rocker.label.setColour(juce::Label::textColourId, silkscreen);
    rocker.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(rocker.label);
    rocker.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, parameterId, rocker.button);
}

void GhostarAudioProcessorEditor::addSelector(Selector& selector,
                                            const char* parameterId,
                                            const juce::String& text,
                                            const juce::String& tooltip)
{
    selector.box.getProperties().set("panelSelector", true);
    selector.box.setTooltip(tooltip);
    selector.box.setName(text);
    selector.box.setWantsKeyboardFocus(true);
    canvas.addAndMakeVisible(selector.box);
    selector.label.setText(text, juce::dontSendNotification);
    selector.label.setFont(juce::FontOptions { 12.0f });
    selector.label.setJustificationType(juce::Justification::centred);
    selector.label.setColour(juce::Label::textColourId, silkscreen);
    selector.label.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(selector.label);
    // The attachment populates the box from the parameter's choice labels.
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(
            processor.parameters.getParameter(parameterId)))
    {
        selector.box.addItemList(parameter->choices, 1);
        selector.box.setSelectedItemIndex(parameter->getIndex(),
                                          juce::dontSendNotification);
    }
    selector.attachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, parameterId, selector.box);
}

void GhostarAudioProcessorEditor::timerCallback()
{
    // A caption that has been showing a value for long enough goes back to
    // naming its control.
    const auto revert = [](auto& control) {
        if (control.readoutTicks > 0 && --control.readoutTicks == 0)
        {
            control.label.setText(control.name, juce::dontSendNotification);
            control.label.setColour(juce::Label::textColourId, silkscreen);
        }
    };
    for (Knob* knob : { &tune, &interval, &lfoRate, &shaperShape, &shaperRate,
                        &masterVolume, &brightness, &cutoff, &lowerOnly,
                        &resonance, &kbAmount, &filterEnvAmount, &glide,
                        &externalPitchVolts, &oscBPedalResistance,
                        &filterPedalResistance })
        revert(*knob);
    for (Fader* fader : { &shaperPathA, &shaperPathB, &shaperPathRing,
                          &shaperPathNoise, &filterPathA, &filterPathB,
                          &filterPathNoise, &filterAttack, &filterDecay,
                          &filterSustain, &filterRelease, &loudnessAttack,
                          &loudnessDecay, &loudnessSustain, &loudnessRelease,
                          &xWheel, &yWheel })
        revert(*fader);

    // A host can change the program behind the editor's back.
    if (processor.getCurrentProgram() != shownProgram)
        refreshProgramDisplay();

    if (processor.isGateOpenForDisplay() != gateLampLit)
    {
        gateLampLit = !gateLampLit;
        canvas.repaint(gateLampBounds.expanded(4));
    }
}

void GhostarAudioProcessorEditor::refreshProgramDisplay()
{
    shownProgram = processor.getCurrentProgram();
    programName.setButtonText(processor.getProgramName(shownProgram));
    programName.setTooltip(
        juce::String(ghostar::factoryPresetDescription(shownProgram))
        + " Click to browse the bank.");
    const bool isProgram =
        ghostar::factoryPresetBank(shownProgram)
        == ghostar::PresetBank::Programs;
    const bool isInit =
        shownProgram == ghostar::factoryPresetIndexByName("Init");
    const bool isPreparatoryPattern =
        shownProgram
        == ghostar::factoryPresetIndexByName("Preparatory Pattern");
    programBank.setText(isInit
                            ? "INIT"
                            : isPreparatoryPattern
                            ? "SOUND CHART - INTENTIONALLY SILENT"
                            : isProgram ? "GHOSTAR PROGRAM" : "SOUND CHART",
                        juce::dontSendNotification);
}

void GhostarAudioProcessorEditor::stepProgram(int delta)
{
    const int count = processor.getNumPrograms();
    if (count <= 0)
        return;
    const int next = (processor.getCurrentProgram() + delta + count) % count;
    processor.setCurrentProgram(next);
    refreshProgramDisplay();
}

void GhostarAudioProcessorEditor::showProgramMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu charts;
    juce::PopupMenu programs;
    const int current = processor.getCurrentProgram();
    const int init = ghostar::factoryPresetIndexByName("Init");

    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        if (index == init)
            continue;

        // The description rides in the item text, so the browser says what
        // each program does rather than only what it is called.
        const juce::String text =
            processor.getProgramName(index) + "   ·   "
            + juce::String(ghostar::factoryPresetDescription(index));
        auto& target =
            ghostar::factoryPresetBank(index) == ghostar::PresetBank::Programs
                ? programs
                : charts;
        target.addItem(index + 1, text, true, index == current);
    }

    menu.addSectionHeader("Default");
    menu.addItem(init + 1,
                 processor.getProgramName(init) + "   ·   "
                     + juce::String(ghostar::factoryPresetDescription(init)),
                 true, init == current);
    menu.addSectionHeader("Sound Charts: the manual's lessons");
    menu.addSubMenu("Sound Charts", charts);
    menu.addSectionHeader("Ghostar Programs: the performance bank");
    menu.addSubMenu("Ghostar Programs", programs);

    // The menu outlives the editor if the window is closed while it is
    // open, and its completion callback still runs — so it holds a safe
    // pointer rather than a raw one and does nothing if the editor is gone.
    juce::Component::SafePointer<GhostarAudioProcessorEditor> safeThis { this };
    menu.showMenuAsync(
        juce::PopupMenu::Options {}
            .withTargetComponent(&programName)
            .withMinimumWidth(programName.getWidth()),
        [safeThis](int choice) {
            if (choice <= 0 || safeThis == nullptr)
                return;
            safeThis->processor.setCurrentProgram(choice - 1);
            safeThis->refreshProgramDisplay();
        });
}

// Every control is captioned the same way — the name directly above the
// thing it names — so a knob and a selector stacked in one column can never
// leave two captions adjacent in the middle with nothing to say which
// belongs to which.
void GhostarAudioProcessorEditor::layoutKnob(Knob& knob,
                                           juce::Rectangle<int> area)
{
    knob.label.setBounds(area.removeFromTop(16));
    knob.slider.setBounds(area.reduced(2));
}

void GhostarAudioProcessorEditor::layoutFader(Fader& fader,
                                            juce::Rectangle<int> area)
{
    fader.label.setBounds(area.removeFromTop(16));
    fader.slider.setBounds(area);
}

void GhostarAudioProcessorEditor::layoutSelector(Selector& selector,
                                               juce::Rectangle<int> area)
{
    selector.label.setBounds(area.removeFromTop(16));
    selector.box.setBounds(area);
}

void GhostarAudioProcessorEditor::layoutRocker(Rocker& rocker,
                                              juce::Rectangle<int> area)
{
    rocker.label.setBounds(area.removeFromTop(16));
    rocker.button.setBounds(area);
}

void GhostarAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Only ever seen if a host forces a size off the panel's aspect ratio;
    // the panel itself is painted by the canvas.
    g.fillAll(panelCharcoal);
}

void GhostarAudioProcessorEditor::PanelCanvas::paint(juce::Graphics& g)
{
    g.fillAll(panelCharcoal);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303233), 0.0f, 82.0f,
                                          panelCharcoal, 0.0f, 550.0f, false));
    g.fillRect(20, 80, editorWidth - 40, 490);

    // Horizontal rails and substantial cheeks are visible in the original
    // overhead photograph; the software browser occupies a separate top strip.
    const auto wood = [&g](juce::Rectangle<float> bounds, bool vertical) {
        juce::ColourGradient stain(walnutLight, bounds.getX(), bounds.getY(),
            walnutDark, vertical ? bounds.getRight() : bounds.getX(),
            vertical ? bounds.getY() : bounds.getBottom(), false);
        stain.addColour(0.24, juce::Colour(0xff956143));
        stain.addColour(0.7, juce::Colour(0xff68402a));
        g.setGradientFill(stain);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff291d15).withAlpha(0.25f));
        for (int i = 1; i < 7; ++i)
        {
            const float offset = static_cast<float>(i) / 7.0f;
            if (vertical)
                g.drawLine(bounds.getX() + offset * bounds.getWidth(), bounds.getY() + 4.0f,
                           bounds.getX() + offset * bounds.getWidth() - 2.0f, bounds.getBottom() - 4.0f, 0.5f);
            else
                g.drawLine(bounds.getX() + 4.0f, bounds.getY() + offset * bounds.getHeight(),
                           bounds.getRight() - 4.0f, bounds.getY() + offset * bounds.getHeight() + 1.0f, 0.5f);
        }
        g.setColour(juce::Colour(0xffc18e62).withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    };
    wood({ 0.0f, 56.0f, 24.0f, 724.0f }, true);
    wood({ 1436.0f, 56.0f, 24.0f, 724.0f }, true);
    wood({ 20.0f, 56.0f, 1420.0f, 30.0f }, false);
    wood({ 20.0f, 542.0f, 1420.0f, 29.0f }, false);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRect(24, 571, 1412, 6);
    g.fillRect(24, 86, 1412, 4);

    g.setColour(headerCharcoal);
    g.fillRect(0, 0, editorWidth, 56);
    g.setColour(hairline);
    g.drawHorizontalLine(55, 0.0f, static_cast<float>(editorWidth));

    const auto lamp = owner.gateLampBounds.toFloat();
    const bool open = owner.processor.isGateOpenForDisplay();
    const auto dot = juce::Rectangle<float>(8.0f, 8.0f)
                         .withCentre({ lamp.getX() + 5.0f, lamp.getCentreY() });
    g.setColour(markingBlack);
    g.fillEllipse(dot.expanded(2.0f));
    g.setColour(open ? lampLit : lampDark);
    g.fillEllipse(dot);
    g.setColour(silkscreenDim);
    g.setFont(juce::FontOptions { 11.0f });
    g.drawText("GATE", lamp.withTrimmedLeft(16.0f), juce::Justification::centredLeft);

    for (const auto& section : owner.sections)
    {
        auto frame = section.bounds.toFloat();
        auto title = section.bounds.withHeight(18);
        g.setColour(silkscreenDim);
        if (section.title == "AUDIO MIXER")
        {
            // Hardware silkscreen encloses both mixer rows in a single
            // stepped outline, wrapping beneath WHEEL DESTINATIONS.
            juce::Path outline;
            outline.startNewSubPath(320.0f, frame.getY());
            outline.lineTo(frame.getRight(), frame.getY());
            outline.lineTo(frame.getRight(), frame.getBottom());
            outline.lineTo(frame.getX(), frame.getBottom());
            outline.lineTo(frame.getX(), 374.0f);
            outline.lineTo(320.0f, 374.0f);
            outline.closeSubPath();
            g.strokePath(outline, juce::PathStrokeType(1.2f));
            title.setLeft(320);
        }
        else
            g.drawRect(frame, 1.2f);
        g.drawHorizontalLine(title.getBottom(), static_cast<float>(title.getX()),
                             static_cast<float>(title.getRight()));
        g.setColour(silkscreen);
        g.setFont(juce::FontOptions { 13.5f, juce::Font::bold });
        g.drawText(section.title, title.reduced(3, 0), juce::Justification::centred);
        if (section.accent || section.title == "FILTER ENVELOPE")
        {
            const auto footer = section.bounds.withTrimmedTop(section.bounds.getHeight() - 18);
            g.setColour(silkscreenDim);
            g.drawHorizontalLine(footer.getY(), frame.getX(), frame.getRight());
            g.setColour(silkscreen);
            g.drawText(section.accent ? "LOWER FILTER L" : "LOUDNESS ENVELOPE",
                       footer, juce::Justification::centred);
        }
    }

    // The keybed has no decorative title on the hardware. Its top edge sits
    // directly behind the front rail; the three performance wheels are left.
    g.setColour(markingBlack);
    g.fillRect(374, 575, 1036, 195);
    g.setColour(silkscreenDim);
    g.drawRect(46, 582, 294, 182, 1);
    g.setFont(juce::FontOptions { 10.0f });
    g.drawText("UP", 53, 660, 24, 13, juce::Justification::centred);
    g.drawText("DOWN", 48, 734, 34, 13, juce::Justification::centred);
    g.drawArrow({ 65.0f, 678.0f, 65.0f, 729.0f }, 1.0f, 4.0f, 4.0f);

    if (owner.rearConnections.isVisible())
    {
        const auto drawer = owner.rearConnections.getBounds();
        g.setColour(juce::Colour(0xff202324));
        g.fillRect(drawer);
        g.setColour(silkscreenDim);
        g.drawRect(drawer, 1);
        g.setFont(juce::FontOptions { 13.0f, juce::Font::bold });
        g.drawText("REAR CONNECTIONS", drawer.withHeight(27).reduced(14, 0),
                   juce::Justification::centredLeft);
        g.setFont(juce::FontOptions { 11.0f });
        g.drawText("Close REAR CONNECTIONS to return to the keyboard.",
                   drawer.withTrimmedTop(drawer.getHeight() - 26).reduced(14, 0),
                   juce::Justification::centredLeft);
    }
}

void GhostarAudioProcessorEditor::resized()
{
    // The panel is laid out once, at its design size, and the window scales
    // it — so every window size shows the whole instrument, keys included,
    // with the proportions the silkscreen was drawn to.
    const double scale = juce::jmin(
        static_cast<double>(getWidth()) / static_cast<double>(editorWidth),
        static_cast<double>(getHeight()) / static_cast<double>(editorHeight));
    const auto scaled =
        juce::Rectangle<int> { juce::roundToInt(editorWidth * scale),
                               juce::roundToInt(editorHeight * scale) }
            .withCentre(getLocalBounds().getCentre());
    canvas.setTransform(
        juce::AffineTransform::scale(static_cast<float>(scale))
            .translated(static_cast<float>(scaled.getX()),
                        static_cast<float>(scaled.getY())));
    canvas.setBounds(0, 0, editorWidth, editorHeight);

    layoutPanel();
}

void GhostarAudioProcessorEditor::layoutPanel()
{
    // Measured relative geometry from the 2014 overhead photo linked above.
    // Top: master 20%, oscillators 43%, trigger/gates 20%, modulators 17%.
    // The main panel, wood rail, three wheels and 37-note keybed preserve
    // their physical relationships. Selected detent labels and the browser
    // are software conveniences; native combo boxes retain accessible menus.
    sections.clear();
    wordmark.setBounds(28, 7, 176, 27);
    subtitle.setBounds(28, 32, 280, 15);
    previousProgram.setBounds(397, 17, 28, 28);
    programBank.setBounds(431, 1, 432, 16);
    programName.setBounds(431, 18, 432, 28);
    nextProgram.setBounds(869, 17, 28, 28);
    gateLampBounds = { 965, 18, 70, 28 };
    connectionsButton.setBounds(1100, 17, 184, 28);
    panicButton.setBounds(1312, 17, 112, 28);

    const auto frame = [this](juce::Rectangle<int> bounds, const char* title, bool accent = false) {
        sections.push_back({ title, bounds, accent });
    };
    frame({ 54, 102, 264, 142 }, "MASTER");
    frame({ 320, 102, 180, 142 }, "OSCILLATOR A");
    frame({ 500, 102, 392, 142 }, "OSCILLATOR B");
    frame({ 902, 102, 82, 142 }, "TRIGGER");
    frame({ 988, 102, 188, 142 }, "GATE SELECT");
    frame({ 1184, 102, 110, 408 }, "MOD X");
    frame({ 1300, 102, 110, 408 }, "SHAPER Y");
    frame({ 54, 250, 264, 122 }, "WHEEL DESTINATIONS");
    frame({ 54, 250, 434, 260 }, "AUDIO MIXER");
    frame({ 494, 250, 398, 260 }, "UPPER FILTER U", true);
    frame({ 902, 250, 274, 260 }, "FILTER ENVELOPE");

    layoutKnob(tune, { 69, 128, 99, 107 });
    layoutSelector(octave, { 200, 128, 104, 107 });
    layoutSelector(oscAWaveform, { 327, 128, 106, 107 });
    layoutRocker(sync, { 438, 128, 54, 107 });
    layoutSelector(oscBWaveform, { 512, 128, 117, 107 });
    layoutSelector(oscBRange, { 641, 128, 117, 107 });
    layoutKnob(interval, { 772, 128, 105, 107 });
    trigger.label.setVisible(false);
    trigger.box.setBounds(909, 132, 68, 102);
    layoutRocker(gateKbd, { 991, 128, 58, 107 });
    layoutRocker(gateX, { 1051, 128, 58, 107 });
    layoutRocker(gateYExt, { 1111, 128, 61, 107 });

    layoutSelector(arpeggiator, { 1189, 128, 100, 112 });
    layoutSelector(modSource, { 1189, 260, 100, 110 });
    layoutKnob(lfoRate, { 1189, 395, 100, 107 });
    layoutSelector(shaperMode, { 1305, 128, 100, 112 });
    layoutKnob(shaperShape, { 1305, 260, 100, 110 });
    layoutKnob(shaperRate, { 1305, 395, 100, 107 });

    layoutSelector(modXTo, { 59, 273, 83, 96 });
    layoutRocker(shapeXWithY, { 145, 273, 85, 96 });
    shapeXWithY.label.setFont(juce::FontOptions { 10.0f });
    layoutSelector(shaperYTo, { 232, 273, 81, 96 });
    layoutKnob(masterVolume, { 337, 276, 136, 94 });
    layoutKnob(brightness, { 59, 389, 72, 96 });

    layoutFader(shaperPathA, { 138, 389, 40, 101 });
    layoutFader(shaperPathB, { 181, 389, 40, 101 });
    layoutFader(shaperPathRing, { 224, 389, 40, 101 });
    layoutFader(shaperPathNoise, { 267, 389, 45, 101 });
    layoutFader(filterPathA, { 334, 389, 42, 101 });
    layoutFader(filterPathB, { 382, 389, 42, 101 });
    layoutFader(filterPathNoise, { 432, 389, 48, 101 });
    shaperPathCaption.setBounds(132, 490, 182, 18);
    filterPathCaption.setBounds(323, 490, 159, 18);

    layoutKnob(cutoff, { 504, 279, 88, 94 });
    layoutSelector(upperResonance, { 603, 279, 85, 94 });
    layoutSelector(slope, { 704, 279, 79, 94 });
    layoutKnob(kbAmount, { 795, 279, 88, 94 });
    layoutKnob(lowerOnly, { 504, 388, 88, 99 });
    layoutKnob(resonance, { 601, 388, 89, 99 });
    layoutSelector(lowerMode, { 697, 388, 95, 99 });
    layoutSelector(tracking, { 798, 388, 85, 99 });

    layoutKnob(filterEnvAmount, { 910, 278, 88, 95 });
    layoutFader(filterAttack, { 1003, 278, 37, 95 });
    layoutFader(filterDecay, { 1045, 278, 37, 95 });
    layoutFader(filterSustain, { 1087, 278, 37, 95 });
    layoutFader(filterRelease, { 1129, 278, 37, 95 });
    layoutRocker(vcaBypass, { 910, 388, 88, 99 });
    layoutFader(loudnessAttack, { 1003, 388, 37, 99 });
    layoutFader(loudnessDecay, { 1045, 388, 37, 99 });
    layoutFader(loudnessSustain, { 1087, 388, 37, 99 });
    layoutFader(loudnessRelease, { 1129, 388, 37, 99 });

    layoutKnob(glide, { 86, 589, 90, 65 });
    layoutSelector(glideMode, { 209, 589, 100, 65 });
    pitchWheel.setBounds(92, 660, 43, 80);
    pitchWheelLabel.setBounds(79, 744, 70, 16);
    xWheel.slider.setBounds(179, 660, 43, 80);
    xWheel.label.setBounds(164, 744, 74, 16);
    yWheel.slider.setBounds(265, 660, 43, 80);
    yWheel.label.setBounds(249, 744, 76, 16);
    keyboard.setKeyWidth(1034.0f / 22.0f);
    keyboard.setBounds(376, 578, 1034, 190);

    rearConnections.setBounds(376, 578, 1034, 190);
    layoutSelector(externalAudioConnected, { 16, 42, 140, 44 });
    layoutSelector(externalGate, { 16, 101, 140, 44 });
    layoutSelector(externalPitchConnected, { 180, 42, 132, 44 });
    layoutKnob(externalPitchVolts, { 319, 43, 90, 112 });
    layoutRocker(oscBPedalConnected, { 440, 51, 67, 91 });
    layoutKnob(oscBPedalResistance, { 511, 43, 96, 112 });
    layoutRocker(filterPedalConnected, { 646, 51, 67, 91 });
    layoutKnob(filterPedalResistance, { 717, 43, 96, 112 });
    layoutRocker(splitPaths, { 884, 51, 80, 91 });
}

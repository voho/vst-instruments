#include "YouKnow106Panel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace youknow106::panel
{
namespace
{
// The panel is described by which slot of which section a control occupies,
// and positions are derived from that. Writing coordinates out by hand invites
// exactly the overlap the layout check exists to catch, so nothing here does.
struct Placement
{
    const char* parameterId;
    const char* label;
    const char* tooltip;
    ControlKind kind;
    int section;
    int slot;
    int stackIndex;   // 0 for a slider; the button's row otherwise
    int stackCount;   // 1 for a slider
    int group;
    int groupValue;
    // How many slots the control is drawn across. A slider is always one; a
    // button stack takes as many as its longest legend needs, which is why the
    // sections holding wide legends are wider than the one control in them
    // would otherwise justify.
    int slotSpan { 1 };
};

constexpr Placement placements[controlCount] = {
    // VOLUME
    { parameters::volume, "VOLUME",
      "Sets final stereo output level after the chorus. This performance control is not part of the hardware's 18-byte tone memory.",
      ControlKind::Slider, 0, 0, 0, 1, -1, 0, 1 },

    // BENDER
    { parameters::benderDco, "DCO",
      "Sets how far the pitch-bend lever bends oscillator pitch, from zero to plus or minus 12 semitones.",
      ControlKind::Slider, 1, 0, 0, 1, -1, 0 },
    { parameters::benderVcf, "VCF",
      "Sets how strongly the pitch-bend lever moves filter cutoff, up to about plus or minus 3.6 octaves.",
      ControlKind::Slider, 1, 1, 0, 1, -1, 0 },
    { parameters::benderLfo, "LFO",
      "Sets vibrato depth when the lever is pushed forward or MIDI CC 1 is received, up to plus or minus 4 semitones.",
      ControlKind::Slider, 1, 2, 0, 1, -1, 0 },
    { parameters::portamento, "PORTA",
      "Sets glide time between assigned pitches; fully down disables portamento.",
      ControlKind::Slider, 1, 3, 0, 1, -1, 0 },

    // MODE. Two momentary selection buttons whose lamps are firmware-latched:
    // one is always selected, and pressing both is Solo Unison. Each button
    // prints its own legend -- POLY 1 and POLY 2 -- inside itself.
    { parameters::poly1, "POLY 1",
      "POLY 1 reuses a key's previous voice card when possible, otherwise the longest-free card. Re-click to rebuild held assignments.",
      ControlKind::Toggle, 2, 0, 0, 3, -1, 0, 2 },
    { parameters::poly2, "POLY 2",
      "POLY 2 scans from voice 1 for each note, so new notes can cut released tails but never steal a held key. Re-click to rebuild held assignments.",
      ControlKind::Toggle, 2, 0, 1, 3, -1, 0, 2 },
    // The hardware reaches this state by holding both momentary POLY contacts
    // at once, which a mouse cannot do. Exposing the third stable state as its
    // own latch describes the same three-state assigner without asking for a
    // chord. The pair of poly parameters stays authoritative underneath, so
    // existing automation is unaffected.
    { parameters::legacyKeyMode, "UNISON",
      "Solo Unison stacks all six voice cards on the highest held key. They are unnormalised and share one crystal, so this is a level and thickness change, not a detune. Re-click to rebuild held assignments.",
      ControlKind::Toggle, 2, 0, 2, 3, -1, 0, 2 },

    // LFO
    { parameters::lfoRate, "RATE",
      "Sets the speed of the one shared LFO used by vibrato, PWM and filter modulation.",
      ControlKind::Slider, 3, 0, 0, 1, -1, 0 },
    { parameters::lfoDelay, "DELAY",
      "Sets how long the shared LFO is held back and faded into DCO and VCF modulation when a new phrase starts. It does not delay PWM.",
      ControlKind::Slider, 3, 1, 0, 1, -1, 0 },

    // DCO
    { parameters::dcoLfo, "LFO",
      "Sets delayed-LFO pitch modulation depth: the patch's vibrato amount, up to plus or minus 4 semitones.",
      ControlKind::Slider, 4, 0, 0, 1, -1, 0 },
    { parameters::pwm, "PWM",
      "In MAN mode this sets a fixed pulse width of roughly 50-95%; in LFO mode it sets the maximum LFO sweep width.",
      ControlKind::Slider, 4, 1, 0, 1, -1, 0 },
    // The PWM source pair spans two slots like the other stacks: at one slot
    // its legends had to shrink to 8.5pt beside neighbours at 12, which reads
    // as a defect even though it is legible.
    { parameters::pwmMode, "LFO",
      "Makes the raw shared LFO sweep pulse width; PWM sets the depth and LFO DELAY does not apply.",
      ControlKind::Radio, 4, 2, 0, 2, 1, 0, 2 },
    { parameters::pwmMode, "MAN",
      "Makes the PWM slider set one fixed manual pulse width.",
      ControlKind::Radio, 4, 2, 1, 2, 1, 1, 2 },
    { parameters::range, "16'",
      "Selects the 16-foot oscillator range, one octave below the normal 8-foot range.",
      ControlKind::Radio, 4, 4, 0, 3, 2, 0 },
    { parameters::range, "8'",
      "Selects the normal 8-foot oscillator range.",
      ControlKind::Radio, 4, 4, 1, 3, 2, 1 },
    { parameters::range, "4'",
      "Selects the 4-foot oscillator range, one octave above the normal 8-foot range.",
      ControlKind::Radio, 4, 4, 2, 3, 2, 2 },
    // The hardware prints pulse before saw. The virtual stack keeps that read
    // order even though its narrow section arranges the pair vertically.
    { parameters::pulse, "PULSE",
      "Turns the variable-width pulse waveform on or off; the PWM controls determine its width.",
      ControlKind::Toggle, 4, 5, 0, 2, -1, 0, 2 },
    { parameters::saw, "SAW",
      "Turns the rising sawtooth waveform on or off.",
      ControlKind::Toggle, 4, 5, 1, 2, -1, 0, 2 },
    { parameters::sub, "SUB",
      "Sets the level of the square-wave sub-oscillator, one octave below the DCO and independent of PWM.",
      ControlKind::Slider, 4, 7, 0, 1, -1, 0 },
    { parameters::noise, "NOISE",
      "Sets the level of the shared analogue-noise source mixed into every voice.",
      ControlKind::Slider, 4, 8, 0, 1, -1, 0 },

    // HPF
    { parameters::highPass, "HPF",
      "Selects the shared post-sum filter: 0 boosts bass, 1 is flat, and 2 or 3 remove progressively more low end at modeled corners near 226 and 721 Hz.",
      ControlKind::Steps, 5, 0, 0, 1, -1, 0 },

    // VCF
    { parameters::cutoff, "FREQ",
      "Sets the base cutoff frequency of every voice's four-pole low-pass filter.",
      ControlKind::Slider, 6, 0, 0, 1, -1, 0 },
    { parameters::resonance, "RES",
      "Sets filter feedback and resonance; high settings make the filter self-oscillate.",
      ControlKind::Slider, 6, 1, 0, 1, -1, 0 },
    { parameters::envPolarity, "+",
      "Makes the envelope raise filter cutoff by the amount set with VCF ENV.",
      ControlKind::Radio, 6, 2, 0, 2, 3, 0 },
    { parameters::envPolarity, "-",
      "Makes the envelope lower filter cutoff by the amount set with VCF ENV.",
      ControlKind::Radio, 6, 2, 1, 2, 3, 1 },
    { parameters::vcfEnv, "ENV",
      "Sets how strongly the envelope moves filter cutoff; the plus or minus button chooses its direction.",
      ControlKind::Slider, 6, 3, 0, 1, -1, 0 },
    { parameters::vcfLfo, "LFO",
      "Sets delayed-LFO filter-cutoff modulation, up to roughly plus or minus 3.5 octaves.",
      ControlKind::Slider, 6, 4, 0, 1, -1, 0 },
    { parameters::keyFollow, "KYBD",
      "Sets filter keyboard tracking; at 100%, playing one octave higher raises cutoff by one octave.",
      ControlKind::Slider, 6, 5, 0, 1, -1, 0 },

    // VCA
    { parameters::vcaMode, "ENV",
      "Makes each voice amplifier follow the ADSR envelope.",
      ControlKind::Radio, 7, 0, 0, 2, 4, 0, 2 },
    { parameters::vcaMode, "GATE",
      "Keeps each voice amplifier open at a fixed level while its key or hold is active; the ADSR still runs for filter modulation.",
      ControlKind::Radio, 7, 0, 1, 2, 4, 1, 2 },
    { parameters::vcaLevel, "LEVEL",
      "Stores patch loudness with the tone. It controls one common VCA after the voice sum and HPF, before chorus; it is not envelope depth.",
      ControlKind::Slider, 7, 2, 0, 1, -1, 0 },

    // ENV
    { parameters::attack, "A",
      "Attack: sets the linear rise time from zero to the envelope peak after a note begins. Minimum is one hardware-style control scan, about 4.2 ms, not an instantaneous step.",
      ControlKind::Slider, 8, 0, 0, 1, -1, 0 },
    { parameters::decay, "D",
      "Decay: sets the exponential fall time from the envelope peak to the sustain level.",
      ControlKind::Slider, 8, 1, 0, 1, -1, 0 },
    { parameters::sustain, "S",
      "Sustain: sets the envelope level held while the key remains down; this is a level, not a time.",
      ControlKind::Slider, 8, 2, 0, 1, -1, 0 },
    { parameters::release, "R",
      "Release: sets the exponential fall time after the key is released.",
      ControlKind::Slider, 8, 3, 0, 1, -1, 0 },

    // CHORUS. Two interlocked latching buttons. Neither down is off; selecting
    // one releases the other, so the hardware exposes only Off, I and II.
    // Both latches span the whole section. At one slot of two they sat in its
    // left half with the right half empty, which reads as a control that has
    // gone missing rather than as a two-button group.
    { parameters::chorusI, "I",
      "Toggles the slower stereo BBD Chorus I; press the lit button again for Off. Its 0.553 Hz rate is derived from this instrument's timing network; installed-unit confirmation remains open.",
      ControlKind::Toggle, 9, 0, 0, 2, -1, 0, 2 },
    { parameters::chorusII, "II",
      "Toggles the faster stereo BBD Chorus II; press the lit button again for Off. Its 0.898 Hz rate is derived from this instrument's timing network; installed-unit confirmation remains open.",
      ControlKind::Toggle, 9, 0, 1, 2, -1, 0, 2 },
};

struct Layout
{
    std::array<Section, sectionCount> sections {};
    std::array<Control, controlCount> controls {};
    float width { 0.0f };
};

Layout buildLayout() noexcept
{
    Layout layout {};

    struct SectionSpec
    {
        const char* name;
        Accent accent;
        int slots;
        float x;
        float y;
        float width;
        float height;
    };
    // One continuous row in the instrument's own order, then the lower deck.
    //
    // Every width below is its content plus one padding, so a section is as
    // wide as what it holds and no card carries dead space. The two exceptions
    // are VOLUME and CHORUS: a single-slot cell is narrower than their own
    // headers set, so each is widened to the header and the difference comes
    // out of the multi-slot sections rather than being left as a gap. That is
    // the same reason the hardware's own HPF strip is narrow and its DCO strip
    // is wide.
    //
    // Slot budget: 1 + 2 + 9 + 1 + 6 + 3 + 4 + 1 = 27 across the sound row.
    constexpr SectionSpec specs[sectionCount] = {
        { "VOLUME", Accent::Cyan,    1,   12.0f, soundRowTop,  64.0f,
                                                                   soundRowHeight },
        // Bender depths and glide are performance settings the tone memory
        // does not carry, so they belong beside the lever rather than in the
        // control row.
        { "BENDER", Accent::Magenta, 4,  136.0f, performanceDeckTop, 150.0f,
                                                           performanceDeckHeight },
        // Three stable assign states, so three latches. Its cell is sized by
        // the widest legend it prints rather than by the header.
        { "MODE",   Accent::Magenta, 2,  294.0f, performanceDeckTop, 90.0f,
                                                           performanceDeckHeight },
        // LFO is wider than its two slots would otherwise need: RATE and DELAY
        // are the closest-set pair of long legends on the row, and their ink
        // is what fixes this cell width.
        { "LFO",    Accent::Magenta, 2,   84.0f, soundRowTop,  84.0f,
                                                                   soundRowHeight },
        { "DCO",    Accent::Cyan,    9,  176.0f, soundRowTop, 311.0f,
                                                                   soundRowHeight },
        { "HPF",    Accent::Cyan,    1,  495.0f, soundRowTop,  47.0f,
                                                                   soundRowHeight },
        { "VCF",    Accent::Cyan,    6,  550.0f, soundRowTop, 212.0f,
                                                                   soundRowHeight },
        { "VCA",    Accent::Cyan,    3,  770.0f, soundRowTop, 113.0f,
                                                                   soundRowHeight },
        { "ENV",    Accent::Magenta, 4,  891.0f, soundRowTop, 147.0f,
                                                                   soundRowHeight },
        // Two interlocked latches stacked in one column. Both span the whole
        // section, so its two slots are one control's width between them; the
        // header, not the latches, is what sets the floor here.
        { "CHORUS", Accent::Cyan,    2, 1046.0f, soundRowTop,  62.0f,
                                                                   soundRowHeight },
    };

    for (int index = 0; index < sectionCount; ++index)
    {
        const auto& spec = specs[index];
        layout.sections[static_cast<std::size_t>(index)] =
            { spec.name, spec.accent, spec.slots, spec.x, spec.y,
              spec.width, spec.height };
    }
    layout.width = editorWidth;

    for (int index = 0; index < controlCount; ++index)
    {
        const auto& placement = placements[index];
        const auto& section =
            layout.sections[static_cast<std::size_t>(placement.section)];

        const float innerX = section.x + sectionPadding * 0.5f;
        const float cellWidth = (section.width - sectionPadding)
                              / static_cast<float>(section.slots);
        const float x = innerX + static_cast<float>(placement.slot) * cellWidth;

        const float controlTop = section.y + headerHeight + 6.0f;
        const float labelY = section.y + section.height
                           - controlLabelHeight - 6.0f;
        // MODE contains only firmware-latched buttons that print their own
        // legends, so reserving an empty external slider-legend row would just
        // make all three latches shorter for nothing.
        const float controlHeight = placement.section == 2
                                  ? section.y + section.height - 6.0f - controlTop
                                  : labelY - controlTop;
        float y = controlTop;
        float height = controlHeight;
        if (placement.stackCount > 1)
        {
            const float total = controlHeight
                              - stackGap * static_cast<float>(placement.stackCount - 1);
            height = total / static_cast<float>(placement.stackCount);
            y = controlTop + static_cast<float>(placement.stackIndex) * (height + stackGap);
        }

        const float span = static_cast<float>(placement.slotSpan) * cellWidth;
        const bool isSlider = placement.kind == ControlKind::Slider
                           || placement.kind == ControlKind::Steps;
        const float controlWidth = isSlider
                                 ? std::min (maximumSliderWidth,
                                             span - 2.0f * controlInset)
                                 : span - 2.0f * controlInset;
        const float controlX = x + (span - controlWidth) * 0.5f;
        layout.controls[static_cast<std::size_t>(index)] = {
            placement.parameterId, placement.label, placement.tooltip,
            placement.kind, placement.section,
            controlX, y, controlWidth, height,
            x - labelOverhang, labelY,
            span + 2.0f * labelOverhang, controlLabelHeight,
            placement.group, placement.groupValue
        };
    }

    return layout;
}

const Layout& layout() noexcept
{
    static const Layout built = buildLayout();
    return built;
}
} // namespace

const std::array<Section, sectionCount>& sections() noexcept
{
    return layout().sections;
}

const std::array<Control, controlCount>& controls() noexcept
{
    return layout().controls;
}

float panelWidth() noexcept
{
    return layout().width;
}

float textWidth(const char* text, float pointSize, bool bold) noexcept
{
    if (text == nullptr)
        return 0.0f;

    // Advance widths as a fraction of the point size, rounded up from what a
    // bold grotesque actually uses. Only the characters the panel prints are
    // listed; anything else falls through to the widest of them, so adding a
    // legend with a new character cannot silently under-estimate.
    const auto advance = [](char c) noexcept -> float {
        switch (c)
        {
            case 'I': case 'i': case 'l': case '\'': case '.': return 0.34f;
            case 'J': case 'j': case 't': case '(': case ')':  return 0.44f;
            case '1': case '+': case '-':                      return 0.56f;
            case ' ':                                          return 0.36f;
            case 'M': case 'W':                                return 0.92f;
            case 'm': case 'w':                                return 0.86f;
            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'K': case 'L':
            case 'N': case 'O': case 'P': case 'Q': case 'R':
            case 'S': case 'T': case 'U': case 'V': case 'X':
            case 'Y': case 'Z':                                return 0.72f;
            case '0': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':            return 0.64f;
            default:                                           return 0.72f;
        }
    };

    // The advances above are bold metrics. A regular weight is narrower, but
    // only slightly, and this is deliberately shaded towards the bold figure so
    // the result stays the over-estimate the checks rely on either way.
    const float weight = bold ? 1.0f : 0.97f;

    float total = 0.0f;
    for (const char* c = text; *c != '\0'; ++c)
        total += advance(*c);
    return total * pointSize * weight * typefaceHorizontalScale;
}

float buttonPointSizeFor(const char* label, float width, float height) noexcept
{
    // Size from the height first, then shrink until the legend fits the width.
    // The editor calls this too, so the size the check reasons about is the
    // size that actually gets drawn -- a check that only looked at the height
    // would pass a legend the editor then clipped.
    float size = height * 0.34f;
    if (size > buttonPointSizeMax)
        size = buttonPointSizeMax;

    // Leave a little air either side so the glyphs are not flush to the edge.
    const float available = width - 6.0f;
    if (available <= 0.0f)
        return 0.0f;

    const float natural = textWidth(label, size, true);
    if (natural > available && natural > 0.0f)
        size *= available / natural;
    return size;
}

float buttonPointSizeFor(const Control& control) noexcept
{
    return buttonPointSizeFor(control.label, control.width, control.height);
}

namespace
{
const char* overflowingLabel() noexcept
{
    for (const auto& section : sections())
    {
        // The header bar is the section box inset by the same padding the
        // editor draws it with.
        const float available = section.width - sectionPadding;
        if (textWidth(section.name, headerPointSize, true) > available)
            return section.name;
    }

    for (const auto& control : controls())
    {
        if (control.kind == ControlKind::Slider || control.kind == ControlKind::Steps)
        {
            if (textWidth(control.label, labelPointSize, true) > control.labelWidth)
                return control.label;
        }
        else if (buttonPointSizeFor(control) < buttonPointSizeMin)
        {
            return control.label;
        }
    }

    // Legend boxes overhang their slots, so neighbouring boxes overlap by
    // design. What must not overlap is the ink: each legend is centred in its
    // box, so two adjacent ones collide when half of each, plus a hair of
    // separation, exceeds the distance between their centres.
    const auto& controlList = controls();
    for (std::size_t a = 0; a < controlList.size(); ++a)
    {
        const auto& first = controlList[a];
        if (first.kind != ControlKind::Slider && first.kind != ControlKind::Steps)
            continue;
        const float firstHalf = 0.5f * textWidth(first.label, labelPointSize, true);
        const float firstCentre = first.labelX + 0.5f * first.labelWidth;

        for (std::size_t b = a + 1; b < controlList.size(); ++b)
        {
            const auto& second = controlList[b];
            if (second.kind != ControlKind::Slider && second.kind != ControlKind::Steps)
                continue;
            const bool labelRowsAreSeparate =
                first.labelY + first.labelHeight <= second.labelY + 0.001f
                || second.labelY + second.labelHeight <= first.labelY + 0.001f;
            if (labelRowsAreSeparate)
                continue;
            const float secondHalf =
                0.5f * textWidth(second.label, labelPointSize, true);
            const float secondCentre = second.labelX + 0.5f * second.labelWidth;
            const float gap = std::fabs(firstCentre - secondCentre)
                            - (firstHalf + secondHalf);
            if (gap < 2.0f)
                return first.label;
        }
    }

    return nullptr;
}
} // namespace

bool labelsFitTheirControls() noexcept
{
    return overflowingLabel() == nullptr;
}

const char* firstOverflowingLabel() noexcept
{
    return overflowingLabel();
}

bool layoutIsConsistent() noexcept
{
    const auto& sectionList = sections();
    const auto& controlList = controls();

    for (const auto& control : controlList)
    {
        if (control.section < 0 || control.section >= sectionCount)
            return false;
        if (control.width <= 0.0f || control.height <= 0.0f)
            return false;

        const auto& section = sectionList[static_cast<std::size_t>(control.section)];
        if (control.x < section.x || control.x + control.width > section.x + section.width)
            return false;
        if (control.y < section.y + headerHeight
            || control.y + control.height > section.y + section.height + 0.001f)
            return false;
        if (control.labelX < section.x - labelOverhang - 0.001f
            || control.labelX + control.labelWidth
                   > section.x + section.width + labelOverhang + 0.001f
            || control.labelY < section.y + headerHeight
            || control.labelY + control.labelHeight
                   > section.y + section.height + 0.001f)
            return false;
    }

    for (std::size_t a = 0; a < sectionList.size(); ++a)
        for (std::size_t b = a + 1; b < sectionList.size(); ++b)
        {
            const auto& first = sectionList[a];
            const auto& second = sectionList[b];
            const bool separated =
                first.x + first.width <= second.x + 0.001f
                || second.x + second.width <= first.x + 0.001f
                || first.y + first.height <= second.y + 0.001f
                || second.y + second.height <= first.y + 0.001f;
            if (!separated)
                return false;
        }

    for (std::size_t a = 0; a < controlList.size(); ++a)
        for (std::size_t b = a + 1; b < controlList.size(); ++b)
        {
            const auto& first = controlList[a];
            const auto& second = controlList[b];
            const bool separated =
                first.x + first.width <= second.x + 0.001f
                || second.x + second.width <= first.x + 0.001f
                || first.y + first.height <= second.y + 0.001f
                || second.y + second.height <= first.y + 0.001f;
            if (!separated)
                return false;
        }

    // Every radio group must be a contiguous run selecting 0..n-1 of one
    // parameter, because that is what the plug-in attaches it to.
    for (int group = 0; group < 8; ++group)
    {
        int expected = 0;
        const char* parameterId = nullptr;
        for (const auto& control : controlList)
        {
            if (control.group != group)
                continue;
            if (parameterId == nullptr)
                parameterId = control.parameterId;
            else if (std::strcmp(parameterId, control.parameterId) != 0)
                return false;
            if (control.groupValue != expected++)
                return false;
        }
    }

    return true;
}

} // namespace youknow106::panel

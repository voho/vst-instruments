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
    bool horizontalStack { false };
};

constexpr Placement placements[controlCount] = {
    // CONTROLLER CHEEK
    { parameters::volume, "VOLUME",
      "Sets final stereo output level after the chorus. This performance control is not part of the hardware's 18-byte tone memory.",
      ControlKind::Knob, 0, 0, 0, 1, -1, 0, 1 },

    { parameters::benderDco, "DCO",
      "Sets how far the pitch-bend lever bends oscillator pitch, from zero to plus or minus 12 semitones.",
      ControlKind::Slider, 0, 0, 0, 1, -1, 0 },
    { parameters::benderVcf, "VCF",
      "Sets how strongly the pitch-bend lever moves filter cutoff, up to about plus or minus 3.6 octaves.",
      ControlKind::Slider, 0, 1, 0, 1, -1, 0 },
    { parameters::benderLfo, "LFO",
      "Sets vibrato depth when the lever is pushed forward or MIDI CC 1 is received, up to plus or minus 4 semitones.",
      ControlKind::Slider, 0, 2, 0, 1, -1, 0 },
    { parameters::portamento, "PORTAMENTO",
      "Sets glide time between assigned pitches; fully down disables portamento.",
      ControlKind::Knob, 0, 1, 0, 1, -1, 0 },

    // MODE. Two momentary selection buttons whose lamps are firmware-latched:
    // one is always selected, and pressing both is Solo Unison. The third cell
    // is reserved for the editor's equivalent simultaneous-contact key.
    { parameters::poly1, "POLY 1",
      "POLY 1 reuses a key's previous voice card when possible, otherwise the longest-free card. Re-click to rebuild held assignments.",
      ControlKind::Toggle, 1, 0, 0, 1, -1, 0 },
    { parameters::poly2, "POLY 2",
      "POLY 2 scans from voice 1 for each note, so new notes can cut released tails but never steal a held key. Re-click to rebuild held assignments.",
      ControlKind::Toggle, 1, 1, 0, 1, -1, 0 },

    // LFO
    { parameters::lfoRate, "RATE",
      "Sets the speed of the one shared LFO used by vibrato, PWM and filter modulation.",
      ControlKind::Slider, 2, 0, 0, 1, -1, 0 },
    { parameters::lfoDelay, "DELAY",
      "Sets how long the shared LFO is held back and faded into DCO, PWM and VCF modulation when a new phrase starts.",
      ControlKind::Slider, 2, 1, 0, 1, -1, 0 },

    // DCO
    { parameters::range, "4'",
      "Selects the 4-foot oscillator range, one octave above the normal 8-foot range.",
      ControlKind::Radio, 3, 0, 0, 3, 2, 2 },
    { parameters::range, "8'",
      "Selects the normal 8-foot oscillator range.",
      ControlKind::Radio, 3, 0, 1, 3, 2, 1 },
    { parameters::range, "16'",
      "Selects the 16-foot oscillator range, one octave below the normal 8-foot range.",
      ControlKind::Radio, 3, 0, 2, 3, 2, 0 },
    { parameters::dcoLfo, "LFO",
      "Sets delayed-LFO pitch modulation depth: the patch's vibrato amount, up to plus or minus 4 semitones.",
      ControlKind::Slider, 3, 1, 0, 1, -1, 0 },
    { parameters::pwm, "PWM",
      "In MAN mode this sets a fixed pulse width of roughly 50-95%; in LFO mode it sets the maximum LFO sweep width.",
      ControlKind::Slider, 3, 2, 0, 1, -1, 0 },
    { parameters::pwmMode, "LFO",
      "Makes the shared, delay-gated LFO sweep pulse width; PWM sets the depth.",
      ControlKind::Radio, 3, 3, 0, 2, 1, 0 },
    { parameters::pwmMode, "MAN",
      "Makes the PWM slider set one fixed manual pulse width.",
      ControlKind::Radio, 3, 3, 1, 2, 1, 1 },
    { parameters::pulse, "PULSE",
      "Turns the variable-width pulse waveform on or off; the PWM controls determine its width.",
      ControlKind::Toggle, 3, 4, 0, 2, -1, 0 },
    { parameters::saw, "SAW",
      "Turns the rising sawtooth waveform on or off.",
      ControlKind::Toggle, 3, 4, 1, 2, -1, 0 },
    { parameters::sub, "SUB",
      "Sets the level of the square-wave sub-oscillator, one octave below the DCO and independent of PWM.",
      ControlKind::Slider, 3, 5, 0, 1, -1, 0 },
    { parameters::noise, "NOISE",
      "Sets the level of the shared analogue-noise source mixed into every voice.",
      ControlKind::Slider, 3, 6, 0, 1, -1, 0 },

    // HPF
    { parameters::highPass, "HPF",
      "Selects the shared post-sum filter: 0 boosts bass, 1 is flat, and 2 or 3 remove progressively more low end at modeled corners near 226 and 721 Hz.",
      ControlKind::Steps, 4, 0, 0, 1, -1, 0 },

    // VCF
    { parameters::cutoff, "FREQ",
      "Sets the base cutoff frequency of every voice's four-pole low-pass filter.",
      ControlKind::Slider, 5, 0, 0, 1, -1, 0 },
    { parameters::resonance, "RES",
      "Sets filter feedback and resonance; high settings make the filter self-oscillate.",
      ControlKind::Slider, 5, 1, 0, 1, -1, 0 },
    { parameters::envPolarity, "+",
      "Makes the envelope raise filter cutoff by the amount set with VCF ENV.",
      ControlKind::Radio, 5, 2, 0, 2, 3, 0 },
    { parameters::envPolarity, "-",
      "Makes the envelope lower filter cutoff by the amount set with VCF ENV.",
      ControlKind::Radio, 5, 2, 1, 2, 3, 1 },
    { parameters::vcfEnv, "ENV",
      "Sets how strongly the envelope moves filter cutoff; the plus or minus button chooses its direction.",
      ControlKind::Slider, 5, 3, 0, 1, -1, 0 },
    { parameters::vcfLfo, "LFO",
      "Sets delayed-LFO filter-cutoff modulation, up to roughly plus or minus 3.5 octaves.",
      ControlKind::Slider, 5, 4, 0, 1, -1, 0 },
    { parameters::keyFollow, "KYBD",
      "Sets filter keyboard tracking; at 100%, playing one octave higher raises cutoff by one octave.",
      ControlKind::Slider, 5, 5, 0, 1, -1, 0 },

    // VCA
    { parameters::vcaMode, "ENV",
      "Makes each voice amplifier follow the ADSR envelope.",
      ControlKind::Radio, 6, 0, 0, 2, 4, 0, 2 },
    { parameters::vcaMode, "GATE",
      "Keeps each voice amplifier open at a fixed level while its key or hold is active; the ADSR still runs for filter modulation.",
      ControlKind::Radio, 6, 0, 1, 2, 4, 1, 2 },
    { parameters::vcaLevel, "LEVEL",
      "Stores patch loudness with the tone. It controls one common VCA after the voice sum and HPF, before chorus; it is not envelope depth.",
      ControlKind::Slider, 6, 2, 0, 1, -1, 0 },

    // ENV
    { parameters::attack, "A",
      "Attack: sets the linear rise time from zero to the envelope peak after a note begins. Minimum is one hardware-style control scan, about 4.2 ms, not an instantaneous step.",
      ControlKind::Slider, 7, 0, 0, 1, -1, 0 },
    { parameters::decay, "D",
      "Decay: sets the exponential fall time from the envelope peak to the sustain level.",
      ControlKind::Slider, 7, 1, 0, 1, -1, 0 },
    { parameters::sustain, "S",
      "Sustain: sets the envelope level held while the key remains down; this is a level, not a time.",
      ControlKind::Slider, 7, 2, 0, 1, -1, 0 },
    { parameters::release, "R",
      "Release: sets the exponential fall time after the key is released.",
      ControlKind::Slider, 7, 3, 0, 1, -1, 0 },

    // CHORUS. The hardware gives Off its own key beside the two interlocked
    // effect keys. The pair remains authoritative underneath.
    { parameters::legacyChorus, "OFF",
      "Switches the hardware chorus off by releasing both Chorus I and Chorus II latches.",
      ControlKind::Toggle, 8, 0, 0, 3, -1, 0 },
    { parameters::chorusI, "I",
      "Toggles the slower stereo BBD Chorus I; press the lit button again for Off. Its 0.553 Hz rate is derived from this instrument's timing network; installed-unit confirmation remains open.",
      ControlKind::Toggle, 8, 0, 1, 3, -1, 0 },
    { parameters::chorusII, "II",
      "Toggles the faster stereo BBD Chorus II; press the lit button again for Off. Its 0.898 Hz rate is derived from this instrument's timing network; installed-unit confirmation remains open.",
      ControlKind::Toggle, 8, 0, 2, 3, -1, 0 },
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
        const char* displayTitle;
        const char* displayCode;
        int slots;
        float x;
        float y;
        float width;
        float height;
    };
    // The seven sound sections retain the original order. The controller is
    // the separate left cheek and MODE begins the programmer tier under the
    // sound strip; widths reserve air for the integrated extension controls.
    constexpr SectionSpec specs[sectionCount] = {
        { "CONTROLLER", "CONTROLLER", "", 4, controllerX, performanceDeckTop,
                                            controllerWidth, performanceDeckHeight },
        { "MODE", "VOICE MODE", "", 3, instrumentLeft, performanceDeckTop,
                                            242.0f, programmerHeight },
        { "LFO", "LFO", "", 2, instrumentLeft, soundRowTop,
                                            100.0f, soundRowHeight },
        // The range and waveform selectors each share one vertical column.
        // A common 21-unit section gutter and compact stacked keys leave clear
        // air between every DCO sub-group.
        { "DCO", "DCO", "", 7, 339.0f, soundRowTop,
                                            322.0f, soundRowHeight },
        { "HPF", "HPF", "", 1, 682.0f, soundRowTop,
                                             64.0f, soundRowHeight },
        { "VCF", "VCF", "", 6, 767.0f, soundRowTop,
                                            266.0f, soundRowHeight },
        { "VCA", "VCA", "", 3, 1054.0f, soundRowTop,
                                            128.0f, soundRowHeight },
        { "ENV", "ENV", "", 4, 1203.0f, soundRowTop,
                                            166.0f, soundRowHeight },
        // HISS uses the second column beside the vertical OFF/I/II stack.
        { "CHORUS", "CHORUS", "", 2, 1390.0f, soundRowTop,
                                            116.0f, soundRowHeight },
    };

    for (int index = 0; index < sectionCount; ++index)
    {
        const auto& spec = specs[index];
        layout.sections[static_cast<std::size_t>(index)] =
            { spec.name, spec.displayTitle, spec.displayCode, spec.slots,
              spec.x, spec.y, spec.width, spec.height };
    }
    layout.width = editorWidth;

    for (int index = 0; index < controlCount; ++index)
    {
        const auto& placement = placements[index];
        const auto& section =
            layout.sections[static_cast<std::size_t>(placement.section)];

        // The controller cheek is mechanical rather than a regular section
        // grid: two knobs sit above three sensitivity faders and the lever.
        if (placement.section == 0)
        {
            float x = section.x;
            float y = section.y;
            float width = 0.0f;
            float height = 0.0f;
            float labelX = section.x;
            float labelY = section.y;
            float labelWidth = 0.0f;
            if (placement.kind == ControlKind::Knob)
            {
                const bool isVolume = std::strcmp (placement.parameterId,
                                                    parameters::volume) == 0;
                x = section.x + (isVolume ? 12.0f : 102.0f);
                y = section.y + 38.0f;
                width = 64.0f;
                height = 64.0f;
                labelX = section.x + (isVolume ? 2.0f : 94.0f);
                labelY = section.y + 12.0f;
                labelWidth = isVolume ? 84.0f : 94.0f;
            }
            else
            {
                const int benderIndex = std::strcmp (placement.parameterId,
                                                       parameters::benderDco) == 0 ? 0
                                      : std::strcmp (placement.parameterId,
                                                     parameters::benderVcf) == 0 ? 1 : 2;
                x = section.x + 12.0f + static_cast<float> (benderIndex) * 42.0f;
                y = section.y + 144.0f;
                width = 30.0f;
                height = 78.0f;
                labelX = x - 5.0f;
                labelY = section.y + 120.0f;
                labelWidth = 40.0f;
            }

            layout.controls[static_cast<std::size_t>(index)] = {
                placement.parameterId, placement.label, placement.tooltip,
                placement.kind, placement.section, x, y, width, height,
                labelX, labelY, labelWidth, controlLabelHeight,
                placement.group, placement.groupValue
            };
            continue;
        }

        const float innerX = section.x + sectionPadding * 0.5f;
        const float cellWidth = (section.width - sectionPadding)
                              / static_cast<float>(section.slots);
        const float x = innerX + static_cast<float>(placement.slot) * cellWidth;

        const bool isMode = placement.section == 1;
        const float labelY = section.y + headerHeight + (isMode ? 1.0f : 7.0f);
        const float controlTop = isMode ? section.y + headerHeight + 2.0f
                                        : labelY + controlLabelHeight + 4.0f;
        const float controlHeight = section.y + section.height - 8.0f - controlTop;
        float controlX = x;
        float y = controlTop;
        float height = controlHeight;
        const float span = static_cast<float>(placement.slotSpan) * cellWidth;
        float controlWidth = span - 2.0f * controlInset;
        if (placement.stackCount > 1 && placement.horizontalStack)
        {
            constexpr float horizontalInset = 2.0f;
            controlWidth = span - 2.0f * horizontalInset;
            const float total = controlWidth
                              - stackGap * static_cast<float>(placement.stackCount - 1);
            controlWidth = total / static_cast<float>(placement.stackCount);
            controlX = x + horizontalInset
                     + static_cast<float>(placement.stackIndex)
                           * (controlWidth + stackGap);
        }
        else if (placement.stackCount > 1)
        {
            const float total = controlHeight
                              - stackGap * static_cast<float>(placement.stackCount - 1);
            height = total / static_cast<float>(placement.stackCount);
            y = controlTop + static_cast<float>(placement.stackIndex) * (height + stackGap);
            const float inset = placement.kind == ControlKind::Slider
                                     || placement.kind == ControlKind::Steps
                              ? controlInset : 2.0f;
            controlWidth = span - 2.0f * inset;
            controlX = x + inset;
            // The DCO's stacked selectors are intentionally narrower than a
            // full cell. Their centred faces create a visible gutter between
            // PWM source and waveform while labels retain the full cell width.
            if (placement.section == 3
                && placement.kind != ControlKind::Slider
                && placement.kind != ControlKind::Steps)
            {
                controlWidth = std::min (34.0f, span - 4.0f);
                controlX = x + (span - controlWidth) * 0.5f;
            }
        }
        else
        {
            const bool isSlider = placement.kind == ControlKind::Slider
                               || placement.kind == ControlKind::Steps;
            controlWidth = isSlider
                         ? std::min (maximumSliderWidth,
                                     span - 2.0f * controlInset)
                         : span - 2.0f * controlInset;
            controlX = x + (span - controlWidth) * 0.5f;
        }
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

float compactLegendPointSize(float buttonHeight, float editorScale) noexcept
{
    // A key at or above the default editor size keeps its full design size;
    // below it the ceiling comes down with the panel, so the legend shrinks
    // with the key rather than staying behind on a key that no longer fits it.
    const float scaleRatio = editorScale / defaultEditorScale;
    const float ceiling =
        compactLegendPointSizeMax * (scaleRatio < 1.0f ? scaleRatio : 1.0f);

    float size = buttonHeight * compactLegendHeightRatio;
    if (size > ceiling)
        size = ceiling;
    if (size < compactLegendPointSizeMin)
        size = compactLegendPointSizeMin;
    return size;
}

float compactLegendWidth(float buttonWidth, float buttonHeight,
                         bool hasIcon) noexcept
{
    float available = buttonWidth - 2.0f * compactLegendPaddingX;
    if (hasIcon)
    {
        const float faceHeight = buttonHeight - 2.0f * compactLegendPaddingY;
        const float icon = faceHeight < compactLegendIconSize
                               ? faceHeight : compactLegendIconSize;
        available -= (icon > 0.0f ? icon : 0.0f) + compactLegendIconGap;
    }
    return available > 0.0f ? available : 0.0f;
}

namespace
{
const char* overflowingLabel() noexcept
{
    for (const auto& section : sections())
    {
        // Match the engraved title area, including the reserved hardware code
        // at its right edge. The conservative JUCE-free width model is paired
        // with a real-font check in the editor suite.
        float available = section.width - 8.0f;
        if (section.displayCode[0] != '\0')
            available -= std::min (40.0f, available * 0.28f) + 4.0f;
        float size = std::max (11.0f, headerPointSize);
        const float natural = textWidth (section.displayTitle, size, true);
        if (natural > available && natural > 0.0f)
            size *= available / natural;
        size = std::max (9.5f, size);
        if (textWidth (section.displayTitle, size, true) > available + 0.1f)
            return section.displayTitle;
    }

    for (const auto& control : controls())
    {
        if (control.kind == ControlKind::Slider || control.kind == ControlKind::Knob
            || control.kind == ControlKind::Steps)
        {
            if (textWidth(control.label, labelPointSize, true) > control.labelWidth)
                return control.label;
        }
        else if (! (std::strcmp (control.label, "PULSE") == 0
                    || std::strcmp (control.label, "SAW") == 0
                    || std::strcmp (control.label, "16'") == 0
                    || std::strcmp (control.label, "8'") == 0
                    || std::strcmp (control.label, "4'") == 0)
                 && buttonPointSizeFor(control) < buttonPointSizeMin)
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
        if (first.kind != ControlKind::Slider && first.kind != ControlKind::Knob
            && first.kind != ControlKind::Steps)
            continue;
        const float firstHalf = 0.5f * textWidth(first.label, labelPointSize, true);
        const float firstCentre = first.labelX + 0.5f * first.labelWidth;

        for (std::size_t b = a + 1; b < controlList.size(); ++b)
        {
            const auto& second = controlList[b];
            if (second.kind != ControlKind::Slider && second.kind != ControlKind::Knob
                && second.kind != ControlKind::Steps)
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
            || control.labelY < (control.section == 0 ? section.y
                                                       : section.y + headerHeight)
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

    // Every radio group must select each value 0..n-1 exactly once from one
    // parameter. Visual order need not equal value order.
    for (int group = 0; group < 8; ++group)
    {
        std::array<bool, controlCount> seenValues {};
        int memberCount = 0;
        const char* parameterId = nullptr;
        for (const auto& control : controlList)
        {
            if (control.group != group)
                continue;
            if (parameterId == nullptr)
                parameterId = control.parameterId;
            else if (std::strcmp(parameterId, control.parameterId) != 0)
                return false;
            if (control.groupValue < 0 || control.groupValue >= controlCount
                || seenValues[static_cast<std::size_t>(control.groupValue)])
                return false;
            seenValues[static_cast<std::size_t>(control.groupValue)] = true;
            ++memberCount;
        }
        for (int value = 0; value < memberCount; ++value)
            if (! seenValues[static_cast<std::size_t>(value)])
                return false;
    }

    return true;
}

} // namespace youknow106::panel

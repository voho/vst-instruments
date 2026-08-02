#include "YouKnow106Panel.h"

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
    { parameters::volume,      "VOLUME",   ControlKind::Slider, 0, 0, 0, 1, -1, 0 },

    // BENDER
    { parameters::benderDco,   "DCO",      ControlKind::Slider, 1, 0, 0, 1, -1, 0 },
    { parameters::benderVcf,   "VCF",      ControlKind::Slider, 1, 1, 0, 1, -1, 0 },
    { parameters::benderLfo,   "LFO",      ControlKind::Slider, 1, 2, 0, 1, -1, 0 },
    { parameters::portamento,  "PORTA",    ControlKind::Slider, 1, 3, 0, 1, -1, 0 },

    // KEY MODE. Two independent latching buttons, exactly as on the panel being
    // modelled: both down is unison, and there is no third button.
    { parameters::poly1,       "POLY 1",   ControlKind::Toggle, 2, 0, 0, 2, -1, 0, 2 },
    { parameters::poly2,       "POLY 2",   ControlKind::Toggle, 2, 0, 1, 2, -1, 0, 2 },

    // LFO
    { parameters::lfoRate,     "RATE",     ControlKind::Slider, 3, 0, 0, 1, -1, 0 },
    { parameters::lfoDelay,    "DELAY",    ControlKind::Slider, 3, 1, 0, 1, -1, 0 },

    // DCO
    { parameters::dcoLfo,      "LFO",      ControlKind::Slider, 4, 0, 0, 1, -1, 0 },
    { parameters::pwm,         "PWM",      ControlKind::Slider, 4, 1, 0, 1, -1, 0 },
    { parameters::pwmMode,     "LFO",      ControlKind::Radio,  4, 2, 0, 2,  1, 0 },
    { parameters::pwmMode,     "MAN",      ControlKind::Radio,  4, 2, 1, 2,  1, 1 },
    { parameters::range,       "16'",      ControlKind::Radio,  4, 3, 0, 3,  2, 0 },
    { parameters::range,       "8'",       ControlKind::Radio,  4, 3, 1, 3,  2, 1 },
    { parameters::range,       "4'",       ControlKind::Radio,  4, 3, 2, 3,  2, 2 },
    { parameters::saw,         "SAW",      ControlKind::Toggle, 4, 4, 0, 2, -1, 0, 2 },
    { parameters::pulse,       "PULSE",    ControlKind::Toggle, 4, 4, 1, 2, -1, 0, 2 },
    { parameters::sub,         "SUB",      ControlKind::Slider, 4, 6, 0, 1, -1, 0 },
    { parameters::noise,       "NOISE",    ControlKind::Slider, 4, 7, 0, 1, -1, 0 },

    // HPF
    { parameters::highPass,    "HPF",      ControlKind::Steps,  5, 0, 0, 1, -1, 0 },

    // VCF
    { parameters::cutoff,      "FREQ",     ControlKind::Slider, 6, 0, 0, 1, -1, 0 },
    { parameters::resonance,   "RES",      ControlKind::Slider, 6, 1, 0, 1, -1, 0 },
    { parameters::envPolarity, "+",        ControlKind::Radio,  6, 2, 0, 2,  3, 0 },
    { parameters::envPolarity, "-",        ControlKind::Radio,  6, 2, 1, 2,  3, 1 },
    { parameters::vcfEnv,      "ENV",      ControlKind::Slider, 6, 3, 0, 1, -1, 0 },
    { parameters::vcfLfo,      "LFO",      ControlKind::Slider, 6, 4, 0, 1, -1, 0 },
    { parameters::keyFollow,   "KYBD",     ControlKind::Slider, 6, 5, 0, 1, -1, 0 },

    // VCA
    { parameters::vcaMode,     "ENV",      ControlKind::Radio,  7, 0, 0, 2,  4, 0, 2 },
    { parameters::vcaMode,     "GATE",     ControlKind::Radio,  7, 0, 1, 2,  4, 1, 2 },
    { parameters::vcaLevel,    "LEVEL",    ControlKind::Slider, 7, 2, 0, 1, -1, 0 },

    // ENV
    { parameters::attack,      "A",        ControlKind::Slider, 8, 0, 0, 1, -1, 0 },
    { parameters::decay,       "D",        ControlKind::Slider, 8, 1, 0, 1, -1, 0 },
    { parameters::sustain,     "S",        ControlKind::Slider, 8, 2, 0, 1, -1, 0 },
    { parameters::release,     "R",        ControlKind::Slider, 8, 3, 0, 1, -1, 0 },

    // CHORUS. Two independent latching buttons. Neither down is off, and both
    // down is the faster I+II setting rather than a duplicate of II.
    { parameters::chorusI,     "I",        ControlKind::Toggle, 9, 0, 0, 2, -1, 0 },
    { parameters::chorusII,    "II",       ControlKind::Toggle, 9, 0, 1, 2, -1, 0 },
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

    struct SectionSpec { const char* name; Accent accent; int slots; };
    // Slot counts follow the widest legend each section has to print, not the
    // number of controls in it: KEY MODE holds two buttons but needs the width
    // of "POLY 1", and its header needs the width of "KEY MODE".
    constexpr SectionSpec specs[sectionCount] = {
        { "VOLUME",   Accent::Cyan,    2 },
        { "BENDER",   Accent::Magenta, 4 },
        { "KEY MODE", Accent::Cyan,    2 },
        { "LFO",      Accent::Magenta, 2 },
        { "DCO",      Accent::Cyan,    8 },
        { "HPF",      Accent::Magenta, 1 },
        { "VCF",      Accent::Cyan,    6 },
        { "VCA",      Accent::Magenta, 3 },
        { "ENV",      Accent::Cyan,    4 },
        { "CHORUS",   Accent::Magenta, 2 },
    };

    float cursor = panelMargin;
    for (int index = 0; index < sectionCount; ++index)
    {
        const auto& spec = specs[index];
        const float width = static_cast<float>(spec.slots) * slotWidth + sectionPadding;
        layout.sections[static_cast<std::size_t>(index)] =
            { spec.name, spec.accent, spec.slots, cursor, width };
        cursor += width + sectionGap;
    }
    layout.width = cursor - sectionGap + panelMargin;

    for (int index = 0; index < controlCount; ++index)
    {
        const auto& placement = placements[index];
        const auto& section =
            layout.sections[static_cast<std::size_t>(placement.section)];

        const float innerX = section.x + sectionPadding * 0.5f;
        const float x = innerX + static_cast<float>(placement.slot) * slotWidth;

        float y = controlTop;
        float height = controlHeight;
        if (placement.stackCount > 1)
        {
            const float total = controlHeight
                              - stackGap * static_cast<float>(placement.stackCount - 1);
            height = total / static_cast<float>(placement.stackCount);
            y = controlTop + static_cast<float>(placement.stackIndex) * (height + stackGap);
        }

        const float span = static_cast<float>(placement.slotSpan) * slotWidth;
        layout.controls[static_cast<std::size_t>(index)] = {
            placement.parameterId, placement.label, placement.kind, placement.section,
            x + controlInset, y, span - 2.0f * controlInset, height,
            x - labelOverhang, span + 2.0f * labelOverhang,
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
    return total * pointSize * weight;
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
        if (control.y < controlTop
            || control.y + control.height > controlTop + controlHeight + 0.001f)
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

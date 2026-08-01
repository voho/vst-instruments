#include "YouKnow106Panel.h"

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
};

constexpr Placement placements[controlCount] = {
    // VOLUME
    { parameters::volume,      "VOLUME",   ControlKind::Slider, 0, 0, 0, 1, -1, 0 },

    // BENDER
    { parameters::benderDco,   "DCO",      ControlKind::Slider, 1, 0, 0, 1, -1, 0 },
    { parameters::benderVcf,   "VCF",      ControlKind::Slider, 1, 1, 0, 1, -1, 0 },
    { parameters::benderLfo,   "LFO",      ControlKind::Slider, 1, 2, 0, 1, -1, 0 },
    { parameters::portamento,  "PORTA",    ControlKind::Slider, 1, 3, 0, 1, -1, 0 },

    // KEY MODE
    { parameters::keyMode,     "POLY 1",   ControlKind::Radio,  2, 0, 0, 3,  0, 0 },
    { parameters::keyMode,     "POLY 2",   ControlKind::Radio,  2, 0, 1, 3,  0, 1 },
    { parameters::keyMode,     "UNISON",   ControlKind::Radio,  2, 0, 2, 3,  0, 2 },

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
    { parameters::saw,         "SAW",      ControlKind::Toggle, 4, 4, 0, 2, -1, 0 },
    { parameters::pulse,       "PULSE",    ControlKind::Toggle, 4, 4, 1, 2, -1, 0 },
    { parameters::sub,         "SUB",      ControlKind::Slider, 4, 5, 0, 1, -1, 0 },
    { parameters::noise,       "NOISE",    ControlKind::Slider, 4, 6, 0, 1, -1, 0 },

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
    { parameters::vcaMode,     "ENV",      ControlKind::Radio,  7, 0, 0, 2,  4, 0 },
    { parameters::vcaMode,     "GATE",     ControlKind::Radio,  7, 0, 1, 2,  4, 1 },
    { parameters::vcaLevel,    "LEVEL",    ControlKind::Slider, 7, 1, 0, 1, -1, 0 },

    // ENV
    { parameters::attack,      "A",        ControlKind::Slider, 8, 0, 0, 1, -1, 0 },
    { parameters::decay,       "D",        ControlKind::Slider, 8, 1, 0, 1, -1, 0 },
    { parameters::sustain,     "S",        ControlKind::Slider, 8, 2, 0, 1, -1, 0 },
    { parameters::release,     "R",        ControlKind::Slider, 8, 3, 0, 1, -1, 0 },

    // CHORUS
    { parameters::chorus,      "OFF",      ControlKind::Radio,  9, 0, 0, 3,  5, 0 },
    { parameters::chorus,      "I",        ControlKind::Radio,  9, 0, 1, 3,  5, 1 },
    { parameters::chorus,      "II",       ControlKind::Radio,  9, 0, 2, 3,  5, 2 },
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
    constexpr SectionSpec specs[sectionCount] = {
        { "VOLUME",   Accent::Cyan,    1 },
        { "BENDER",   Accent::Magenta, 4 },
        { "KEY MODE", Accent::Cyan,    1 },
        { "LFO",      Accent::Magenta, 2 },
        { "DCO",      Accent::Cyan,    7 },
        { "HPF",      Accent::Magenta, 1 },
        { "VCF",      Accent::Cyan,    6 },
        { "VCA",      Accent::Magenta, 2 },
        { "ENV",      Accent::Cyan,    4 },
        { "CHORUS",   Accent::Magenta, 1 },
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

        layout.controls[static_cast<std::size_t>(index)] = {
            placement.parameterId, placement.label, placement.kind, placement.section,
            x + controlInset, y, slotWidth - 2.0f * controlInset, height,
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

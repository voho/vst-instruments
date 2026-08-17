#include "ElectryVisuals.h"
#include "DspMath.h"

#include <algorithm>
#include <cmath>

namespace electry::visuals
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

// clampf lives in DspMath.h now, shared with ElectryEngine and ElectryFx;
// unqualified calls below resolve to it through the enclosing electry
// namespace.

// Distance from the nut to fret n as a fraction of the scale length.
float fretOffset(int fret) noexcept
{
    return 1.0f - std::exp2(-static_cast<float>(fret) / 12.0f);
}

// Shared by fretWireFraction and fretCentreFraction: `span` is the same
// fretOffset(lastFret) value for every wire on a given neck, so callers that
// need more than one wire (fretCentreFraction asks for the pair either side
// of a fretted note) solve it once and pass it in rather than repeating the
// std::exp2 call per wire.
float fretWireFractionWithSpan(int fret, int lastFret, float span) noexcept
{
    if (span <= 0.0f)
        return 0.0f;
    const int clamped = std::clamp(fret, 0, lastFret);
    return clampf(fretOffset(clamped) / span, 0.0f, 1.0f);
}
} // namespace

float fretSpan(int lastFret) noexcept
{
    return lastFret > 0 ? fretOffset(lastFret) : 0.0f;
}

float fretWireFraction(int fret, int lastFret, float span) noexcept
{
    return fretWireFractionWithSpan(fret, lastFret, span);
}

float fretCentreFraction(int fret, int lastFret, float span) noexcept
{
    if (fret <= 0 || lastFret <= 0)
        return 0.0f;
    const int clamped = std::clamp(fret, 1, std::max(1, lastFret));
    return 0.5f * (fretWireFractionWithSpan(clamped - 1, lastFret, span)
                   + fretWireFractionWithSpan(clamped, lastFret, span));
}

float fretWireFraction(int fret, int lastFret) noexcept
{
    return fretWireFraction(fret, lastFret, fretSpan(lastFret));
}

float fretCentreFraction(int fret, int lastFret) noexcept
{
    return fretCentreFraction(fret, lastFret, fretSpan(lastFret));
}

float stringRowFraction(int stringIndex, int stringCount, float inset) noexcept
{
    if (stringCount <= 1)
        return 0.5f;
    const int clamped = std::clamp(stringIndex, 0, stringCount - 1);
    const float usable = clampf(inset, 0.0f, 0.45f);
    return usable + (1.0f - 2.0f * usable) * static_cast<float>(clamped)
                        / static_cast<float>(stringCount - 1);
}

float stringThickness(int stringIndex, float thinnest, float thickest) noexcept
{
    constexpr int count = ElectryEngine::stringCount;
    const int clamped = std::clamp(stringIndex, 0, count - 1);
    // The modelled set runs .080 down to .009. Drawing the real diameter ratio
    // would make the top strings invisible, so the mapping compresses it with
    // a square root while keeping the order and the visible taper.
    const float t = std::sqrt(static_cast<float>(clamped)
                              / static_cast<float>(count - 1));
    return thickest + (thinnest - thickest) * t;
}

float meterBallistics(float current, float target, float attack,
                      float release) noexcept
{
    if (! std::isfinite(current))
        current = 0.0f;
    if (! std::isfinite(target))
        target = 0.0f;
    const float coefficient = clampf(target > current ? attack : release,
                                     0.0f, 1.0f);
    return current + coefficient * (target - current);
}

float vibrationShape(float positionFraction, float stoppedFraction) noexcept
{
    // Every other helper here (levelHeat, meterBallistics, packStringVisual)
    // folds a non-finite input to a safe fallback before its own clamp runs;
    // this one clamped both inputs but never checked them first, so a NaN
    // position or stopped fraction survived clampf() unchanged - NaN fails
    // every comparison clampf makes - and reached the `position <= stopped`
    // early-out as false, then std::sin() below, propagating NaN into the
    // fretboard's drawn string path instead of drawing it at rest.
    if (! std::isfinite(positionFraction) || ! std::isfinite(stoppedFraction))
        return 0.0f;
    const float stopped = clampf(stoppedFraction, 0.0f, 0.98f);
    const float position = clampf(positionFraction, 0.0f, 1.0f);
    if (position <= stopped)
        return 0.0f;
    const float sounding = 1.0f - stopped;
    if (sounding <= 1.0e-4f)
        return 0.0f;
    return std::sin(pi * (position - stopped) / sounding);
}

float levelHeat(float level) noexcept
{
    if (! std::isfinite(level))
        return 0.0f;
    // A square-root knee: quiet coupled ring still reads on the display while
    // a hard pick still separates clearly from a medium one.
    return clampf(std::sqrt(clampf(level, 0.0f, 1.0f)), 0.0f, 1.0f);
}

std::uint32_t packStringVisual(const StringVisualState& state) noexcept
{
    const auto note = static_cast<std::uint32_t>(
        std::clamp(state.midiNote, -1, 127) + 1);
    const auto fret = static_cast<std::uint32_t>(
        std::clamp(state.fret, -1, ElectryEngine::fretCount) + 1);
    // Three bits: the play-style bank has grown past four entries, so the
    // stroke flag moved up one bit to keep the style field contiguous. Nothing
    // outside this file reads the layout.
    const auto playStyle = static_cast<std::uint32_t>(
        std::clamp(static_cast<int>(state.playStyle), 0,
                   static_cast<int>(PlayStyle::Dead)));
    const float level = std::isfinite(state.level)
        ? clampf(state.level, 0.0f, 1.0f) : 0.0f;
    const auto quantised = static_cast<std::uint32_t>(level * 255.0f + 0.5f);

    return (note & 0xffu)
         | ((fret & 0x3fu) << 8)
         | ((playStyle & 0x07u) << 14)
         | (state.strokeUp ? (1u << 17) : 0u)
         | (state.sounding ? (1u << 18) : 0u)
         | (state.sympathetic ? (1u << 19) : 0u)
         | (state.releasing ? (1u << 20) : 0u)
         | ((quantised & 0xffu) << 24);
}

StringVisualState unpackStringVisual(std::uint32_t packed) noexcept
{
    StringVisualState state;
    state.midiNote = static_cast<int>(packed & 0xffu) - 1;
    state.fret = static_cast<int>((packed >> 8) & 0x3fu) - 1;
    state.playStyle = static_cast<PlayStyle>((packed >> 14) & 0x07u);
    state.strokeUp = (packed & (1u << 17)) != 0u;
    state.sounding = (packed & (1u << 18)) != 0u;
    state.sympathetic = (packed & (1u << 19)) != 0u;
    state.releasing = (packed & (1u << 20)) != 0u;
    state.level = static_cast<float>((packed >> 24) & 0xffu) / 255.0f;
    return state;
}

} // namespace electry::visuals

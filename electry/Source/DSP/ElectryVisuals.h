#pragma once

// Geometry, ballistics, and transfer helpers for Electry's fretboard display.
//
// Nothing here touches JUCE. The editor is a thin renderer over these
// functions, which keeps the fretboard's layout maths, its meter ballistics,
// and the lock-free audio-to-editor packing inside the unit-tested,
// cross-platform DSP library.

#include "ElectryEngine.h"

#include <array>
#include <cstdint>

namespace electry::visuals
{

// The fret positions a 22-fret neck carries side dots or inlays at. Twelve is
// the octave and is conventionally drawn as a double marker.
inline constexpr std::array<int, 7> inlayFrets { 3, 5, 7, 9, 15, 17, 19 };
inline constexpr int octaveInlayFret = 12;
inline constexpr int upperOctaveInlayFret = 21;

// Fraction of the drawn neck taken up by the first `fret` frets, using the
// real 12th-root-of-two rule normalised so that fret 0 sits at 0 and the last
// drawn fret sits at 1. Monotonically increasing, and the spacing narrows
// toward the body exactly as it does on a real neck.
[[nodiscard]] float fretWireFraction(int fret, int lastFret) noexcept;

// Where a finger sits for a given fret: midway between the two enclosing
// wires. Fret 0 (an open string) reports the nut.
[[nodiscard]] float fretCentreFraction(int fret, int lastFret) noexcept;

// Vertical placement of a string, from string 0 (lowest, drawn at the top) to
// the highest string. `inset` keeps the outer strings off the binding.
[[nodiscard]] float stringRowFraction(int stringIndex, int stringCount,
                                      float inset) noexcept;

// Drawn thickness of a string in pixels. The eight modelled gauges span
// .080 to .009, so the low strings are visibly heavier.
[[nodiscard]] float stringThickness(int stringIndex, float thinnest,
                                    float thickest) noexcept;

// One pole of meter ballistics with separate rise and fall constants. Kept
// here so the editor's animation and the engine's display follower are the
// same function.
[[nodiscard]] float meterBallistics(float current, float target,
                                    float attack, float release) noexcept;

// Transverse displacement of the drawn string at `positionFraction` along the
// whole neck, for a string stopped at `stoppedFraction`. The shape is the
// fundamental standing wave over the sounding length only, so the part of the
// string behind the fretting finger stays still.
[[nodiscard]] float vibrationShape(float positionFraction,
                                   float stoppedFraction) noexcept;

// A 0..1 "heat" for colouring a string by level, with a gentle knee so quiet
// sympathetic ring is still visible.
[[nodiscard]] float levelHeat(float level) noexcept;

// Packed audio-thread-to-editor transfer. One 32-bit word per string keeps the
// snapshot atomic without a lock: a torn read is impossible, so the display
// can never show one string's note with another's level.
[[nodiscard]] std::uint32_t packStringVisual(const StringVisualState& state) noexcept;
[[nodiscard]] StringVisualState unpackStringVisual(std::uint32_t packed) noexcept;

} // namespace electry::visuals

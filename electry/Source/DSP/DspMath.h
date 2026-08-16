#pragma once

// A handful of scalar primitives shared by the string engine, the amplifier
// chain and the fretboard visuals. All three JUCE-free DSP translation units
// used to carry their own byte-identical copy of `clampf`, `lerp` and
// `smoothStep` - exactly the kind of drift that lets a future edit reach one
// copy and quietly miss the others. Centralising them here changes no
// arithmetic: every definition below is exactly what each copy already
// computed, so callers keep the same float-only rounding and the rendered
// audio stays bit-identical.

namespace electry
{

[[nodiscard]] constexpr float clampf(float value, float low,
                                     float high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}

[[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

// Cubic Hermite ease: 0 at value <= 0, 1 at value >= 1, zero slope at both
// ends.
[[nodiscard]] constexpr float smoothStep(float value) noexcept
{
    value = clampf(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

} // namespace electry

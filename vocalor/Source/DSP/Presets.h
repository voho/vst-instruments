#pragma once

#include "VoiceEngine.h"

namespace vocalor
{

/** A named starting point.

    The table lives in the JUCE-free core rather than in the processor so that
    it can be rendered and checked by the DSP test suite: a preset that produces
    silence, a non-finite sample or a value the engine clamps away is a defect
    the tests catch here rather than one a player finds. It carries
    @c EngineParameters, so the only thing the processor has to know is how to
    write those values into its own host parameters.

    These are original settings. No sampled, licensed or third-party material is
    involved anywhere in this file.
*/
struct FactoryPreset
{
    const char* name;
    EngineParameters parameters;
};

/** Number of factory presets. */
[[nodiscard]] int factoryPresetCount() noexcept;

/** Preset @c index, clamped into range. Index 0 is the shipping default sound,
    so a host that opens on program 0 opens on what the plug-in opens on. */
[[nodiscard]] const FactoryPreset& factoryPreset (int index) noexcept;

/** Display name of preset @c index, clamped into range. Never null. */
[[nodiscard]] const char* factoryPresetName (int index) noexcept;

} // namespace vocalor

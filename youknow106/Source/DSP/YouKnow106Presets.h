#pragma once

#include "YouKnow106SysEx.h"

#include <array>
#include <cstddef>

namespace youknow106::presets
{

// The complete 128-tone factory memory supplied with the reference instrument:
// bank A followed by bank B, groups 1..8 and patches 1..8. Each entry is stored
// from its original eighteen 7-bit tone bytes and decoded by the same path used
// for a hardware SysEx dump; there are no hand-transcribed tone floats and no
// hidden per-preset DSP gain. Product presets add only the documented plug-in
// controls below, including attenuation-only positions for the front-panel
// output-volume shaft.
//
// The hardware stores no names. The names here are archival descriptive
// metadata and do not take part in tone-memory or SysEx round trips. Provenance,
// source hashes and the independent corpus comparison are recorded in README.md.

struct Preset
{
    // "A11" through "A88", then "B11" through "B88".
    const char* number;
    const char* name;
    sysex::Patch patch;

    // Product presets are complete, deterministic plug-in states even though
    // the tone inside them still round-trips through the JUNO's 18-byte patch
    // format.  These are the controls which the hardware's tone memory does
    // not carry.  Keeping them beside the tone, rather than teaching SysEx
    // about plug-in-only data, lets the preset bar restore the player's whole
    // setup without changing what an imported hardware patch means.  "Whole
    // setup" stops short of one visible control: see the note below the fields.
    struct Controls
    {
        float volume { 0.80f };
        float benderDco { 0.30f };
        float benderVcf { 0.0f };
        float benderLfo { 0.0f };
        float portamento { 0.0f };
        KeyMode keyMode { KeyMode::Poly1 };
        int transpose { 0 };
        float masterTune { 0.0f };
        float velocity { 0.0f };
        float calibration { 1.0f };
        float chorusNoise { Chorus::defaultNoiseScale };
        int polyphony { 6 };
        // Deliberately no quality/oversampling entry. The internal rate is a
        // property of the machine the plug-in is running on, not of the sound,
        // so recalling a preset must not overrule a player who chose a cheaper
        // setting to fit a session's CPU budget.
    };

    Controls controls {};

    // Whether this patch keeps the same effective 7-bit hardware state.
    //
    // The factory bank uses only the patch memory's Off/I/II chorus states, so
    // its entries are exportable. Live I+II has no tone-memory encoding and is
    // therefore reported as a categorical loss when written to hardware SysEx.
    [[nodiscard]] bool exportsLosslessly() const noexcept
    {
        return sysex::survivesPatchMemory(patch);
    }
};

inline constexpr int presetCount = 128;

[[nodiscard]] const std::array<Preset, presetCount>& factoryBank() noexcept;

// The bank entry for a patch number such as "A11", or nullptr.
[[nodiscard]] const Preset* findByNumber(const char* number) noexcept;

} // namespace youknow106::presets

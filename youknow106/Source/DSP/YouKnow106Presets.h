#pragma once

#include "YouKnow106SysEx.h"

#include <array>
#include <cstddef>

namespace youknow106::presets
{

// The factory bank.
//
// These are original YouKnow106 patches, written here as panel settings. They
// are deliberately *not* the reference instrument's factory bank: that bank is
// Roland's data, and this project ships no ROM contents. What they do share
// with it is the format -- each one has an exact effective state at the patch
// memory's 7-bit control resolution, so every entry round-trips to the same
// eighteen tone bytes and can be sent to real hardware.
//
// Their relative levels are audited with the stored patch controls themselves,
// chiefly VCA LEVEL; there is no hidden per-program make-up gain. Sustained
// entries must remain within -8/+5 dB of the bank's fixed 200 ms median corpus.
// This is a YouKnow106 product-balance policy, not a claim about Roland's bank
// or the still-unmeasured original common-VCA transfer.
//
// This compact bank uses the reference instrument's own numbering for banks A
// and B, groups 1 and 2, patches 1 through 8. It does not claim to fill all 128
// A11..B88 memory locations.

struct Preset
{
    // "A11" through "A28" and "B11" through "B28" in this 32-patch bank.
    const char* number;
    const char* name;
    sysex::Patch patch;

    // Whether this patch keeps the same effective 7-bit hardware state.
    //
    // The factory bank uses only the hardware's Off/I/II chorus states, so its
    // entries are exportable. The predicate also identifies the obsolete
    // OneTwo compatibility value in imported old sessions; that value has no
    // hardware encoding and canonicalises to II.
    [[nodiscard]] bool exportsLosslessly() const noexcept
    {
        return sysex::survivesPatchMemory(patch);
    }
};

inline constexpr int presetCount = 32;

[[nodiscard]] const std::array<Preset, presetCount>& factoryBank() noexcept;

// The bank entry for a patch number such as "A11", or nullptr.
[[nodiscard]] const Preset* findByNumber(const char* number) noexcept;

} // namespace youknow106::presets

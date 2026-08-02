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
// with it is the format -- each one is exactly the state the modelled patch
// memory can hold, so every entry round-trips through the instrument's own
// system-exclusive patch message and can be sent to real hardware.
//
// The bank is laid out in the reference instrument's own two-bank, eight-group,
// eight-patch arrangement, so the numbering a player expects is the numbering
// they get.

struct Preset
{
    // "A11" through "B88", as the front panel numbers them.
    const char* number;
    const char* name;
    sysex::Patch patch;
};

inline constexpr int presetCount = 32;

[[nodiscard]] const std::array<Preset, presetCount>& factoryBank() noexcept;

// The bank entry for a patch number such as "A11", or nullptr.
[[nodiscard]] const Preset* findByNumber(const char* number) noexcept;

} // namespace youknow106::presets

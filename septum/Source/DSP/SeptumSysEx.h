// Roland SH-201 System Exclusive (SysEx) DT1 / RQ1 parser and encoder.
//
// Protocol per Roland SH-201 MIDI Implementation v1.00 (2006-03-01):
// Header: F0 41 <dev> 00 00 16 <cmd> <addr0..3> <data...> <checksum> F7
// Commands: 11H (RQ1 - Request Data), 12H (DT1 - Data Set)
//
// Fully JUCE-free, allocation-bounded and suitable for the audio thread and tests.

#pragma once

#include "SeptumPatch.h"
#include "SeptumPresets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace septum::sysex
{

inline constexpr std::uint8_t rolandId = 0x41;
inline constexpr std::uint8_t defaultDeviceId = 0x10;
inline constexpr std::array<std::uint8_t, 3> sh201ModelId { 0x00, 0x00, 0x16 };

inline constexpr std::uint8_t cmdRq1 = 0x11; // Request data 1
inline constexpr std::uint8_t cmdDt1 = 0x12; // Data set 1

// Base address blocks (MIDI Implementation section 4)
inline constexpr std::uint32_t addrTempPatchCommon = 0x00000000;
inline constexpr std::uint32_t addrTempUpperTone   = 0x00000100;
inline constexpr std::uint32_t addrTempLowerTone   = 0x00000200;
inline constexpr std::uint32_t addrTempDelay       = 0x00000300;
inline constexpr std::uint32_t addrTempReverb      = 0x00000400;
inline constexpr std::uint32_t addrTempArpeggio    = 0x00000500;
inline constexpr std::uint32_t addrSystemCommon    = 0x01000000;
inline constexpr std::uint32_t addrUserPatchBase   = 0x20000000;

inline constexpr std::size_t sizePatchCommon = 0x21;
inline constexpr std::size_t sizeTonePatch   = 0x40;
inline constexpr std::size_t sizeDelay       = 0x05;
inline constexpr std::size_t sizeReverb      = 0x0A;
// Nine header bytes, then END STEP, then the 32 x 16 grid.
inline constexpr std::size_t sizeArpeggio    = 0x0A + (arpeggioMaxSteps * arpeggioMaxRows);
inline constexpr std::size_t sizeSystemCommon = 0x1E;

// --------------------------------------------------------------------------
// Checksum calculation (Roland 7-bit checksum rule):
// sum = (addr0 + addr1 + addr2 + addr3 + data0 + data1 + ...) & 0x7F
// checksum = (128 - sum) & 0x7F
// --------------------------------------------------------------------------
[[nodiscard]] inline std::uint8_t calculateChecksum (const std::uint8_t* payload,
                                                     std::size_t size) noexcept
{
    unsigned sum = 0;
    for (std::size_t i = 0; i < size; ++i)
        sum += (payload[i] & 0x7Fu);
    return static_cast<std::uint8_t> ((128u - (sum % 128u)) & 0x7Fu);
}

[[nodiscard]] inline bool verifyChecksum (const std::uint8_t* payload,
                                          std::size_t size,
                                          std::uint8_t expected) noexcept
{
    return calculateChecksum (payload, size) == (expected & 0x7Fu);
}

// --------------------------------------------------------------------------
// Block Serializers (encode structured C++ patch to raw SysEx byte blocks)
// --------------------------------------------------------------------------
void encodePatchCommon (const Patch& patch, std::uint8_t* dest) noexcept;
void encodeTonePatch (const TonePatch& tone, std::uint8_t* dest) noexcept;
void encodeDelayParams (const DelayParams& delay, std::uint8_t* dest) noexcept;
void encodeReverbParams (const ReverbParams& reverb, std::uint8_t* dest) noexcept;
void encodeArpeggioParams (const ArpeggioParams& arp, std::uint8_t* dest) noexcept;

// --------------------------------------------------------------------------
// Block Deserializers (decode raw SysEx byte blocks into structured C++ patch)
// --------------------------------------------------------------------------
void decodePatchCommon (const std::uint8_t* src, std::size_t size, Patch& patch) noexcept;
void decodeTonePatch (const std::uint8_t* src, std::size_t size, TonePatch& tone) noexcept;
void decodeDelayParams (const std::uint8_t* src, std::size_t size, DelayParams& delay) noexcept;
void decodeReverbParams (const std::uint8_t* src, std::size_t size, ReverbParams& reverb) noexcept;
void decodeArpeggioParams (const std::uint8_t* src, std::size_t size, ArpeggioParams& arp) noexcept;

// --------------------------------------------------------------------------
// Full Patch & Packet Codec
// --------------------------------------------------------------------------

// Builds a complete DT1 SysEx message containing a specific address and payload.
[[nodiscard]] std::vector<std::uint8_t> makeDt1Message (
    std::uint32_t address, const std::uint8_t* data, std::size_t dataSize,
    std::uint8_t deviceId = defaultDeviceId);

// Builds an RQ1 (Data Request) SysEx message for a specified address and size.
[[nodiscard]] std::vector<std::uint8_t> makeRq1Message (
    std::uint32_t address, std::uint32_t size,
    std::uint8_t deviceId = defaultDeviceId);

// Builds a full set of DT1 messages for an entire patch at a target base address.
[[nodiscard]] std::vector<std::vector<std::uint8_t>> encodePatchToSysExPackets (
    const Patch& patch, std::uint32_t baseAddress = addrTempPatchCommon,
    std::uint8_t deviceId = defaultDeviceId);

// Encodes a single monolithic .syx byte buffer containing all DT1 packets for a patch.
[[nodiscard]] std::vector<std::uint8_t> encodePatchToSyxBuffer (
    const Patch& patch, std::uint32_t baseAddress = addrTempPatchCommon,
    std::uint8_t deviceId = defaultDeviceId);

// Decodes an incoming MIDI SysEx message. Returns true if the message was an
// SH-201 message and successfully handled.
// Can target a live `Patch`.
bool decodeSysExMessage (const std::uint8_t* msg, std::size_t msgLen,
                         Patch& targetPatch, std::uint8_t expectedDeviceId = 0x7F);

// Parses a .syx file byte buffer (which may contain multiple SysEx messages or
// multiple patches for a full 32-patch or 64-patch bank).
// Populates any patches found into the output list.
bool parseSyxBankFile (const std::uint8_t* fileBytes, std::size_t byteCount,
                       std::vector<NamedPatch>& outPatches);

// Generates a .syx byte buffer containing a full bank of patches.
[[nodiscard]] std::vector<std::uint8_t> generateSyxBankFile (
    const std::vector<NamedPatch>& bank,
    std::uint8_t deviceId = defaultDeviceId);

} // namespace septum::sysex

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

// --------------------------------------------------------------------------
// Address map. Every address and size below is quoted from the MIDI
// Implementation's "Parameter Address Map" (v1.00, 2006-03-01, pp. 4-5); the
// top-level table there reads:
//
//     01 00 00 00  System
//     10 00 00 00  Temporary Patch
//     20 00 00 00  User Patch (001)  ...  20 1F 00 00  User Patch (032)
//
// and a Patch's own offsets read:
//
//     00 00 00  Patch Common                 00 04 00  Patch Reverb
//     00 01 00  Patch Tone (1:Upper)         00 05 00  Patch Arpeggio Common
//     00 02 00  Patch Tone (2:Lower)         00 06 00  Patch Arpeggio Pattern (Note 1)
//     00 03 00  Patch Delay                       :    ... (Note 16) at 00 15 00
// --------------------------------------------------------------------------
inline constexpr std::uint32_t addrSystem         = 0x01000000;
inline constexpr std::uint32_t addrTemporaryPatch = 0x10000000;
inline constexpr std::uint32_t addrUserPatchBase  = 0x20000000;
inline constexpr std::uint32_t userPatchStride    = 0x00010000;
inline constexpr int userPatchCount = 32;

inline constexpr std::uint32_t offsetPatchCommon     = 0x000000;
inline constexpr std::uint32_t offsetUpperTone       = 0x000100;
inline constexpr std::uint32_t offsetLowerTone       = 0x000200;
inline constexpr std::uint32_t offsetDelay           = 0x000300;
inline constexpr std::uint32_t offsetReverb          = 0x000400;
inline constexpr std::uint32_t offsetArpeggioCommon  = 0x000500;
inline constexpr std::uint32_t offsetArpeggioPattern = 0x000600;
inline constexpr std::uint32_t arpeggioPatternStride = 0x000100;

// Each block's documented "Total Size".
inline constexpr std::size_t sizePatchCommon     = 0x21;
inline constexpr std::size_t sizeTonePatch       = 0x40;
inline constexpr std::size_t sizeDelay           = 0x05;
inline constexpr std::size_t sizeReverb          = 0x0A;
inline constexpr std::size_t sizeArpeggioCommon  = 0x08;
inline constexpr std::size_t sizeArpeggioPattern = 0x42;
inline constexpr std::size_t sizeSystemCommon    = 0x21;

// Common, both tones, delay, reverb, arpeggio common, and one pattern block
// per grid row.
inline constexpr std::size_t patchBlockCount =
    6 + static_cast<std::size_t> (arpeggioMaxRows);

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
void encodeArpeggioCommon (const ArpeggioParams& arp, std::uint8_t* dest) noexcept;
// One Patch Arpeggio Pattern block: the 32 steps of grid row `row` (0-15),
// preceded by that row's Original Note.
void encodeArpeggioPattern (const ArpeggioStyle& style, int row, std::uint8_t* dest) noexcept;

// --------------------------------------------------------------------------
// Block Deserializers (decode raw SysEx byte blocks into structured C++ patch)
// --------------------------------------------------------------------------
void decodePatchCommon (const std::uint8_t* src, std::size_t size, Patch& patch) noexcept;
void decodeTonePatch (const std::uint8_t* src, std::size_t size, TonePatch& tone) noexcept;
void decodeDelayParams (const std::uint8_t* src, std::size_t size, DelayParams& delay) noexcept;
void decodeReverbParams (const std::uint8_t* src, std::size_t size, ReverbParams& reverb) noexcept;
void decodeArpeggioCommon (const std::uint8_t* src, std::size_t size, ArpeggioParams& arp) noexcept;
void decodeArpeggioPattern (const std::uint8_t* src, std::size_t size, int row,
                            ArpeggioStyle& style) noexcept;

// --------------------------------------------------------------------------
// Full Patch & Packet Codec
// --------------------------------------------------------------------------

// What a well-formed DT1 for this instrument carries, once the framing, the
// manufacturer, the model, the command and the checksum have all been
// checked. Parsing is separated from applying so a caller can ask "is this a
// packet I own, and which patch is it for?" without mutating anything.
struct Dt1Packet
{
    std::uint32_t address { 0 };
    const std::uint8_t* data { nullptr };
    std::size_t dataLength { 0 };

    // The two high address bytes: which patch this block belongs to
    // (10 00 for the Temporary Patch, 20 00..20 1F for the user slots).
    [[nodiscard]] std::uint32_t patchBase() const noexcept
    {
        return address & 0xFFFF0000u;
    }

    // The block offset inside that patch: 00 = Common, 01/02 = the tones,
    // 03 = Delay, 04 = Reverb, 05 = Arpeggio Common, 06..15 = the sixteen
    // Arpeggio Pattern blocks.
    [[nodiscard]] unsigned block() const noexcept { return (address >> 8) & 0xFFu; }

    // Whether those two bytes name a patch space this codec owns. The
    // System block (01 00 00 00) is not one of them.
    [[nodiscard]] bool patchBaseIsKnown() const noexcept
    {
        const auto base = patchBase();
        return base == addrTemporaryPatch
               || (base >= addrUserPatchBase
                   && base <= addrUserPatchBase
                                  + static_cast<std::uint32_t> (userPatchCount - 1)
                                        * userPatchStride);
    }

    [[nodiscard]] bool blockIsKnown() const noexcept
    {
        return block() < 0x06u + static_cast<unsigned> (arpeggioMaxRows);
    }

    // Where inside that block the write starts. A DT1 addresses a byte, not a
    // block: the MIDI Implementation's own worked example writes one byte to
    // 10 00 04 02, which is REVERB SIZE two bytes into Patch Reverb, and a
    // panel knob on a real unit transmits exactly that shape.
    [[nodiscard]] std::size_t offsetInBlock() const noexcept
    {
        return static_cast<std::size_t> (address & 0xFFu);
    }

    // The documented Total Size of the block this packet addresses.
    [[nodiscard]] std::size_t blockSize() const noexcept
    {
        switch (block())
        {
            case 0x00: return sizePatchCommon;
            case 0x01:
            case 0x02: return sizeTonePatch;
            case 0x03: return sizeDelay;
            case 0x04: return sizeReverb;
            case 0x05: return sizeArpeggioCommon;
            default:   return sizeArpeggioPattern;
        }
    }

    // Everything this codec needs before it will touch a patch — and exactly
    // what the decode accepts, so a caller that gates on this can never be
    // followed by a refusal. The offset belongs in here: a checksum-valid
    // packet for a known base and block can still be addressed past the end
    // of that block, and a bank reader that moved its patch boundary on the
    // strength of base-and-block alone split a patch on one of those.
    [[nodiscard]] bool isForThisInstrument() const noexcept
    {
        return patchBaseIsKnown() && blockIsKnown()
               && offsetInBlock() < blockSize() && dataLength > 0;
    }
};

[[nodiscard]] bool parseDt1Packet (const std::uint8_t* msg, std::size_t msgLen,
                                   std::uint8_t expectedDeviceId,
                                   Dt1Packet& out) noexcept;


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
    const Patch& patch, std::uint32_t baseAddress = addrTemporaryPatch,
    std::uint8_t deviceId = defaultDeviceId);

// Encodes a single monolithic .syx byte buffer containing all DT1 packets for a patch.
[[nodiscard]] std::vector<std::uint8_t> encodePatchToSyxBuffer (
    const Patch& patch, std::uint32_t baseAddress = addrTemporaryPatch,
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

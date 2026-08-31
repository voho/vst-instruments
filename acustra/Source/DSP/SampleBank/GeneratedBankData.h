#pragma once

#include <cstddef>
#include <cstdint>

namespace acustra::dense {

enum class Bank : std::uint8_t {
    Nylon,
    SteelPicked,
    SteelPlucked,
};

namespace generated {

struct PackedZoneRecord {
    Bank bank;
    const char* name;
    std::uint8_t lowKey;
    std::uint8_t highKey;
    std::uint8_t rootMidi;
    float rootHz;
    std::uint32_t sampleRate;
    std::uint8_t channels;
    std::uint32_t frames;
    std::uint32_t onsetFrame;
    std::int32_t peak;
    std::uint8_t lowVelocity;
    std::uint8_t highVelocity;
    std::uint8_t roundRobin;
    std::uint32_t packedOffset;
    std::uint32_t packedBytes;
    std::uint64_t decodedHash;
    std::uint64_t sourceHashBeforeTerminalFade;
    std::uint32_t terminalFadeFrames;
    std::int32_t endJump;
    // All -1 means a generic legacy zone; string 0 is the lowest-pitched
    // physical string. Exact captures must provide the complete triple.
    std::int8_t physicalStringIndex{-1};
    std::int8_t capturedOpenMidi{-1};
    std::int8_t capturedFret{-1};
};

extern const PackedZoneRecord kZones[];
extern const std::size_t kZoneCount;
extern const std::size_t kPackedByteCount;
extern const char* const kAscii85Chunks[];
extern const std::size_t kAscii85ChunkCount;
extern const std::size_t kAscii85CharacterCount;

} // namespace generated
} // namespace acustra::dense

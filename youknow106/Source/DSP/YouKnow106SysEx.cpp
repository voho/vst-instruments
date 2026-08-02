#include "YouKnow106SysEx.h"

#include <algorithm>
#include <cmath>

namespace youknow106::sysex
{
namespace
{
constexpr std::uint8_t sysExStart = 0xf0;
constexpr std::uint8_t sysExEnd = 0xf7;
constexpr float fullScale = 127.0f;

float travelFromByte(std::uint8_t value) noexcept
{
    return static_cast<float>(value & 0x7fu) / fullScale;
}

std::uint8_t byteFromTravel(float travel) noexcept
{
    const float clamped = std::clamp(travel, 0.0f, 1.0f);
    // Round rather than truncate, so a value written and read back lands on the
    // same step instead of drifting a step down every trip.
    const int scaled = static_cast<int>(std::lround(clamped * fullScale));
    return static_cast<std::uint8_t>(std::clamp(scaled, 0, 127));
}

bool isBitSet(std::uint8_t value, int bit) noexcept
{
    return ((value >> bit) & 1u) != 0u;
}

void setBit(std::uint8_t& value, int bit, bool on) noexcept
{
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    value = static_cast<std::uint8_t>(on ? (value | mask) : (value & ~mask));
}

// The tone bytes address the continuous controls by index, so the mapping
// between an index and a field has to exist in one place only. Everything that
// walks the sixteen values -- decode, encode, and the single-parameter message
// -- goes through this.
float* continuousField(Patch& patch, int index) noexcept
{
    switch (index)
    {
        case 0:  return &patch.lfoRate;
        case 1:  return &patch.lfoDelay;
        case 2:  return &patch.dcoLfo;
        case 3:  return &patch.pwm;
        case 4:  return &patch.noise;
        case 5:  return &patch.cutoff;
        case 6:  return &patch.resonance;
        case 7:  return &patch.vcfEnv;
        case 8:  return &patch.vcfLfo;
        case 9:  return &patch.keyFollow;
        case 10: return &patch.vcaLevel;
        case 11: return &patch.attack;
        case 12: return &patch.decay;
        case 13: return &patch.sustain;
        case 14: return &patch.release;
        case 15: return &patch.sub;
        default: return nullptr;
    }
}

const float* continuousField(const Patch& patch, int index) noexcept
{
    return continuousField(const_cast<Patch&>(patch), index);
}

// --- Switch byte one -----------------------------------------------------
// bit 0  16' on          bit 4  saw on
// bit 1  8' on           bit 5  0 selects chorus on
// bit 2  4' on           bit 6  0 selects mode II, 1 selects mode I
// bit 3  pulse on
constexpr int bitSixteen = 0;
constexpr int bitEight = 1;
constexpr int bitFour = 2;
constexpr int bitPulse = 3;
constexpr int bitSaw = 4;
constexpr int bitChorusOff = 5;
constexpr int bitChorusModeOne = 6;

// --- Switch byte two -----------------------------------------------------
// bit 0  0 selects PWM from the modulator, 1 selects manual
// bit 1  0 selects the envelope, 1 selects the gate
// bit 2  0 selects positive envelope polarity, 1 negative
// bits 3-4  high-pass position, counting down: 00 is position 3, 11 is 0
constexpr int bitPwmManual = 0;
constexpr int bitVcaGate = 1;
constexpr int bitEnvInverted = 2;
constexpr int bitHighPassLow = 3;

DcoRange rangeFromBits(std::uint8_t switches) noexcept
{
    // The three range bits are one-hot in every dump the hardware produces, but
    // nothing in the format enforces it. A message asserting none of them, or
    // more than one, is still a message that arrived, so it resolves to the
    // middle range rather than being rejected.
    const bool sixteen = isBitSet(switches, bitSixteen);
    const bool eight = isBitSet(switches, bitEight);
    const bool four = isBitSet(switches, bitFour);
    const int count = (sixteen ? 1 : 0) + (eight ? 1 : 0) + (four ? 1 : 0);
    if (count != 1)
        return DcoRange::Eight;
    if (sixteen)
        return DcoRange::Sixteen;
    if (four)
        return DcoRange::Four;
    return DcoRange::Eight;
}
} // namespace

Patch patchFromToneBytes(const std::uint8_t* bytes) noexcept
{
    Patch patch {};
    if (bytes == nullptr)
        return patch;

    for (int index = 0; index < 16; ++index)
        if (auto* field = continuousField(patch, index))
            *field = travelFromByte(bytes[index]);

    const std::uint8_t first = bytes[16] & 0x7fu;
    patch.range = rangeFromBits(first);
    patch.pulse = isBitSet(first, bitPulse);
    patch.saw = isBitSet(first, bitSaw);
    // Both chorus bits are active-low for "on", so a cleared bit 5 is the
    // effect engaged. The format has no way to say I+II.
    patch.chorus = isBitSet(first, bitChorusOff)
                       ? ChorusMode::Off
                       : (isBitSet(first, bitChorusModeOne) ? ChorusMode::One
                                                            : ChorusMode::Two);

    const std::uint8_t second = bytes[17] & 0x7fu;
    patch.pwmSource = isBitSet(second, bitPwmManual) ? PwmSource::Manual
                                                     : PwmSource::Lfo;
    patch.vcaMode = isBitSet(second, bitVcaGate) ? VcaMode::Gate
                                                 : VcaMode::Envelope;
    patch.envPolarity = isBitSet(second, bitEnvInverted) ? EnvPolarity::Inverted
                                                         : EnvPolarity::Normal;
    // The high-pass field counts down: 0 is the topmost position and 3 is the
    // bass-boost detent, the reverse of the panel's own numbering.
    const int field = (second >> bitHighPassLow) & 0x3;
    patch.highPass = static_cast<HighPassMode>(3 - field);

    return patch;
}

void toneBytesFromPatch(const Patch& patch, std::uint8_t* bytes) noexcept
{
    if (bytes == nullptr)
        return;

    for (int index = 0; index < 16; ++index)
        if (const auto* field = continuousField(patch, index))
            bytes[index] = byteFromTravel(*field);

    std::uint8_t first = 0;
    setBit(first, bitSixteen, patch.range == DcoRange::Sixteen);
    setBit(first, bitEight, patch.range == DcoRange::Eight);
    setBit(first, bitFour, patch.range == DcoRange::Four);
    setBit(first, bitPulse, patch.pulse);
    setBit(first, bitSaw, patch.saw);
    // I+II has no encoding: the patch memory carries one on/off bit and one
    // mode bit. It is written as II, the nearer of the two in modulation rate,
    // and `survivesPatchMemory` reports that the trip is lossy.
    const bool chorusOn = patch.chorus != ChorusMode::Off;
    setBit(first, bitChorusOff, !chorusOn);
    setBit(first, bitChorusModeOne, patch.chorus == ChorusMode::One);
    bytes[16] = first;

    std::uint8_t second = 0;
    setBit(second, bitPwmManual, patch.pwmSource == PwmSource::Manual);
    setBit(second, bitVcaGate, patch.vcaMode == VcaMode::Gate);
    setBit(second, bitEnvInverted, patch.envPolarity == EnvPolarity::Inverted);
    const int position = std::clamp(static_cast<int>(patch.highPass), 0, 3);
    second = static_cast<std::uint8_t>(second | ((3 - position) << bitHighPassLow));
    bytes[17] = second;
}

bool survivesPatchMemory(const Patch& patch) noexcept
{
    return patch.chorus != ChorusMode::OneTwo;
}

bool readPatchMessage(const std::uint8_t* message, std::size_t length,
                      Patch& patch, int& channel) noexcept
{
    if (message == nullptr || length != static_cast<std::size_t>(patchMessageBytes))
        return false;
    if (message[0] != sysExStart || message[1] != manufacturerId
        || message[2] != patchDataOpcode)
        return false;
    if (message[length - 1] != sysExEnd)
        return false;
    // The channel nibble occupies the low four bits; the high nibble is zero in
    // every message the instrument emits.
    if ((message[3] & 0xf0u) != 0)
        return false;
    // A data byte with its top bit set would be a status byte, which cannot
    // appear inside an exclusive message.
    for (std::size_t index = 4; index + 1 < length; ++index)
        if ((message[index] & 0x80u) != 0)
            return false;

    channel = message[3] & 0x0fu;
    patch = patchFromToneBytes(message + 4);
    return true;
}

std::size_t writePatchMessage(const Patch& patch, int channel, std::uint8_t* out,
                              std::size_t capacity) noexcept
{
    if (out == nullptr || capacity < static_cast<std::size_t>(patchMessageBytes))
        return 0;

    out[0] = sysExStart;
    out[1] = manufacturerId;
    out[2] = patchDataOpcode;
    out[3] = static_cast<std::uint8_t>(std::clamp(channel, 0, 15));
    toneBytesFromPatch(patch, out + 4);
    out[static_cast<std::size_t>(patchMessageBytes) - 1] = sysExEnd;
    return static_cast<std::size_t>(patchMessageBytes);
}

bool readParameterMessage(const std::uint8_t* message, std::size_t length,
                          int& parameter, int& value, int& channel) noexcept
{
    if (message == nullptr
        || length != static_cast<std::size_t>(parameterMessageBytes))
        return false;
    if (message[0] != sysExStart || message[1] != manufacturerId
        || message[2] != parameterOpcode)
        return false;
    if (message[6] != sysExEnd || (message[3] & 0xf0u) != 0)
        return false;
    if ((message[4] & 0x80u) != 0 || (message[5] & 0x80u) != 0)
        return false;

    channel = message[3] & 0x0fu;
    parameter = message[4];
    value = message[5];
    return true;
}

std::size_t writeParameterMessage(int parameter, int value, int channel,
                                  std::uint8_t* out, std::size_t capacity) noexcept
{
    if (out == nullptr
        || capacity < static_cast<std::size_t>(parameterMessageBytes))
        return 0;

    out[0] = sysExStart;
    out[1] = manufacturerId;
    out[2] = parameterOpcode;
    out[3] = static_cast<std::uint8_t>(std::clamp(channel, 0, 15));
    out[4] = static_cast<std::uint8_t>(std::clamp(parameter, 0, 127));
    out[5] = static_cast<std::uint8_t>(std::clamp(value, 0, 127));
    out[6] = sysExEnd;
    return static_cast<std::size_t>(parameterMessageBytes);
}

bool applyParameter(Patch& patch, int parameter, int value) noexcept
{
    if (parameter < 0 || parameter >= toneByteCount)
        return false;

    const auto clamped = static_cast<std::uint8_t>(std::clamp(value, 0, 127));
    if (parameter < 16)
    {
        if (auto* field = continuousField(patch, parameter))
        {
            *field = travelFromByte(clamped);
            return true;
        }
        return false;
    }

    // A switch byte arriving on its own carries the whole byte, but only the
    // fields that byte actually encodes may move. Round-tripping the patch
    // through the tone bytes would quantise all sixteen continuous controls to
    // 7 bits as a side effect, and would also drop an unstorable I+II chorus
    // setting while applying a byte that has no chorus bits in it at all.
    if (parameter == static_cast<int>(ToneParameter::SwitchesOne))
    {
        patch.range = rangeFromBits(clamped);
        patch.pulse = isBitSet(clamped, bitPulse);
        patch.saw = isBitSet(clamped, bitSaw);
        patch.chorus = isBitSet(clamped, bitChorusOff)
                           ? ChorusMode::Off
                           : (isBitSet(clamped, bitChorusModeOne) ? ChorusMode::One
                                                                  : ChorusMode::Two);
        return true;
    }

    patch.pwmSource = isBitSet(clamped, bitPwmManual) ? PwmSource::Manual
                                                      : PwmSource::Lfo;
    patch.vcaMode = isBitSet(clamped, bitVcaGate) ? VcaMode::Gate
                                                  : VcaMode::Envelope;
    patch.envPolarity = isBitSet(clamped, bitEnvInverted) ? EnvPolarity::Inverted
                                                          : EnvPolarity::Normal;
    patch.highPass =
        static_cast<HighPassMode>(3 - ((clamped >> bitHighPassLow) & 0x3));
    return true;
}

int parameterValue(const Patch& patch, int parameter) noexcept
{
    if (parameter < 0 || parameter >= toneByteCount)
        return -1;
    std::uint8_t bytes[toneByteCount] {};
    toneBytesFromPatch(patch, bytes);
    return bytes[parameter];
}

} // namespace youknow106::sysex

// System-exclusive compatibility with the modelled instrument.
//
// The point of this suite is that it does not merely check the reader against
// the writer. A pair that agreed with each other but disagreed with the
// hardware would round-trip perfectly and still be useless, so the byte layout
// itself is asserted here against the documented format: which index carries
// which control, which bit carries which switch, and which way round each
// active-low flag runs.

#include "DSP/YouKnow106Presets.h"
#include "DSP/YouKnow106SysEx.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace youknow106;
using namespace youknow106::sysex;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(double actual, double expected, double tolerance,
                const std::string& message)
{
    if (!(std::fabs(actual - expected) <= tolerance))
    {
        ++failures;
        std::cerr << "FAIL: " << message << " (got " << actual << ", expected "
                  << expected << ")\n";
    }
}

std::array<std::uint8_t, toneByteCount> bytesOf(const Patch& patch)
{
    std::array<std::uint8_t, toneByteCount> bytes {};
    toneBytesFromPatch(patch, bytes.data());
    return bytes;
}

// --------------------------------------------------------------------------

// Each continuous control has to sit at the index the format says it does. A
// reader that put VCF Freq where VCA Level belongs would still round-trip.
void testContinuousControlsSitAtTheDocumentedIndices()
{
    struct Case { int index; const char* name; float Patch::* field; };
    constexpr Case cases[] = {
        { 0,  "LFO Rate",   &Patch::lfoRate },
        { 1,  "LFO Delay",  &Patch::lfoDelay },
        { 2,  "DCO LFO",    &Patch::dcoLfo },
        { 3,  "DCO PWM",    &Patch::pwm },
        { 4,  "DCO Noise",  &Patch::noise },
        { 5,  "VCF Freq",   &Patch::cutoff },
        { 6,  "VCF Res",    &Patch::resonance },
        { 7,  "VCF Env",    &Patch::vcfEnv },
        { 8,  "VCF LFO",    &Patch::vcfLfo },
        { 9,  "VCF Kybd",   &Patch::keyFollow },
        { 10, "VCA Level",  &Patch::vcaLevel },
        { 11, "Attack",     &Patch::attack },
        { 12, "Decay",      &Patch::decay },
        { 13, "Sustain",    &Patch::sustain },
        { 14, "Release",    &Patch::release },
        { 15, "Sub",        &Patch::sub },
    };

    for (const auto& test : cases)
    {
        // Drive one index to full scale and check that exactly that field moved.
        std::array<std::uint8_t, toneByteCount> bytes {};
        bytes.fill(0);
        bytes[static_cast<std::size_t>(test.index)] = 127;
        const auto patch = patchFromToneBytes(bytes.data());
        expectNear(patch.*(test.field), 1.0, 1.0e-6,
                   std::string("byte ") + std::to_string(test.index) + " is not "
                       + test.name);

        // And that writing it back puts the value at the same index.
        Patch written {};
        written.*(test.field) = 1.0f;
        const auto out = bytesOf(written);
        expect(out[static_cast<std::size_t>(test.index)] == 127,
               std::string(test.name) + " is not written to byte "
                   + std::to_string(test.index));
    }

    // Mid travel has to land on a mid code, not on an off-by-one from
    // truncation.
    std::array<std::uint8_t, toneByteCount> bytes {};
    bytes.fill(0);
    bytes[5] = 64;
    expectNear(patchFromToneBytes(bytes.data()).cutoff, 64.0 / 127.0, 1.0e-6,
               "a mid-scale code does not decode to mid travel");
}

// The two switch bytes, bit by bit, including the two that are active low.
void testSwitchBitsMatchTheDocumentedLayout()
{
    const auto decodeFirst = [](std::uint8_t value) {
        std::array<std::uint8_t, toneByteCount> bytes {};
        bytes.fill(0);
        bytes[16] = value;
        // Bit 5 clear means the effect is on, so a zeroed byte is chorus II.
        bytes[17] = 0;
        return patchFromToneBytes(bytes.data());
    };

    expect(decodeFirst(0x01).range == DcoRange::Sixteen, "bit 0 is not 16'");
    expect(decodeFirst(0x02).range == DcoRange::Eight, "bit 1 is not 8'");
    expect(decodeFirst(0x04).range == DcoRange::Four, "bit 2 is not 4'");
    expect(decodeFirst(0x08).pulse, "bit 3 is not the pulse waveform");
    expect(!decodeFirst(0x02).pulse, "pulse is on with bit 3 clear");
    expect(decodeFirst(0x10).saw, "bit 4 is not the saw waveform");
    expect(!decodeFirst(0x02).saw, "saw is on with bit 4 clear");

    // Chorus is active low: bit 5 clear is on, and bit 6 then picks the mode
    // with 1 meaning I and 0 meaning II. Getting either sense backwards would
    // silently invert the effect on every patch ever loaded.
    expect(decodeFirst(0x02).chorus == ChorusMode::Two,
           "bit 5 clear with bit 6 clear is not chorus II");
    expect(decodeFirst(0x42).chorus == ChorusMode::One,
           "bit 6 set is not chorus I");
    expect(decodeFirst(0x22).chorus == ChorusMode::Off,
           "bit 5 set is not chorus off");
    expect(decodeFirst(0x62).chorus == ChorusMode::Off,
           "bit 5 set is not chorus off regardless of the mode bit");

    const auto decodeSecond = [](std::uint8_t value) {
        std::array<std::uint8_t, toneByteCount> bytes {};
        bytes.fill(0);
        bytes[16] = 0x22;  // range 8', chorus off
        bytes[17] = value;
        return patchFromToneBytes(bytes.data());
    };

    expect(decodeSecond(0x00).pwmSource == PwmSource::Lfo,
           "bit 0 clear is not PWM from the modulator");
    expect(decodeSecond(0x01).pwmSource == PwmSource::Manual,
           "bit 0 set is not manual PWM");
    expect(decodeSecond(0x00).envPolarity == EnvPolarity::Normal,
           "bit 1 clear is not positive polarity");
    expect(decodeSecond(0x02).envPolarity == EnvPolarity::Inverted,
           "bit 1 set is not negative polarity");
    expect(decodeSecond(0x00).vcaMode == VcaMode::Envelope,
           "bit 2 clear is not the envelope");
    expect(decodeSecond(0x04).vcaMode == VcaMode::Gate,
           "bit 2 set is not the gate");

    // The high-pass field counts down, which is the reverse of the panel's
    // numbering and the easiest thing in the whole format to get backwards.
    expect(decodeSecond(0x00).highPass == HighPassMode::Three,
           "high-pass field 0 is not panel position 3");
    expect(decodeSecond(0x08).highPass == HighPassMode::Two,
           "high-pass field 1 is not panel position 2");
    expect(decodeSecond(0x10).highPass == HighPassMode::One,
           "high-pass field 2 is not panel position 1");
    expect(decodeSecond(0x18).highPass == HighPassMode::Boost,
           "high-pass field 3 is not the bass-boost position");
}

// A dump from real hardware asserts none of the range bits, or more than one,
// only if something has gone wrong -- but it is still a message that arrived,
// and refusing it would mean refusing to load the patch at all.
void testMalformedRangeBitsResolveRatherThanReject()
{
    for (std::uint8_t bits : { std::uint8_t { 0x00 }, std::uint8_t { 0x03 },
                               std::uint8_t { 0x07 } })
    {
        std::array<std::uint8_t, toneByteCount> bytes {};
        bytes.fill(0);
        bytes[16] = static_cast<std::uint8_t>(bits | 0x20);
        const auto patch = patchFromToneBytes(bytes.data());
        expect(patch.range == DcoRange::Eight,
               "an ambiguous range field did not resolve to the middle range");
    }
}

// Every patch the panel can hold has to preserve its effective 7-bit state on
// the trip out and back, except the one categorical setting the format cannot
// express.
void testPatchesRoundTripThroughTheToneBytes()
{
    const auto same = [](const Patch& a, const Patch& b) {
        const auto close = [](float x, float y) {
            // One code step is 1/127; a round trip must land inside half of one.
            return std::fabs(x - y) <= 0.5f / 127.0f + 1.0e-6f;
        };
        return close(a.lfoRate, b.lfoRate) && close(a.lfoDelay, b.lfoDelay)
            && close(a.dcoLfo, b.dcoLfo) && close(a.pwm, b.pwm)
            && close(a.noise, b.noise) && close(a.cutoff, b.cutoff)
            && close(a.resonance, b.resonance) && close(a.vcfEnv, b.vcfEnv)
            && close(a.vcfLfo, b.vcfLfo) && close(a.keyFollow, b.keyFollow)
            && close(a.vcaLevel, b.vcaLevel) && close(a.attack, b.attack)
            && close(a.decay, b.decay) && close(a.sustain, b.sustain)
            && close(a.release, b.release) && close(a.sub, b.sub)
            && a.range == b.range && a.saw == b.saw && a.pulse == b.pulse
            && a.pwmSource == b.pwmSource && a.vcaMode == b.vcaMode
            && a.envPolarity == b.envPolarity && a.highPass == b.highPass
            && a.chorus == b.chorus;
    };

    std::uint32_t seed = 0x1234567u;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xffffu) / 65535.0f;
    };

    for (int trial = 0; trial < 2000; ++trial)
    {
        Patch patch {};
        patch.lfoRate = next();   patch.lfoDelay = next();
        patch.dcoLfo = next();    patch.pwm = next();
        patch.noise = next();     patch.cutoff = next();
        patch.resonance = next(); patch.vcfEnv = next();
        patch.vcfLfo = next();    patch.keyFollow = next();
        patch.vcaLevel = next();  patch.attack = next();
        patch.decay = next();     patch.sustain = next();
        patch.release = next();   patch.sub = next();
        patch.range = static_cast<DcoRange>(static_cast<int>(next() * 2.99f));
        patch.saw = next() > 0.5f;
        patch.pulse = next() > 0.5f;
        patch.pwmSource = static_cast<PwmSource>(static_cast<int>(next() * 1.99f));
        patch.vcaMode = static_cast<VcaMode>(static_cast<int>(next() * 1.99f));
        patch.envPolarity =
            static_cast<EnvPolarity>(static_cast<int>(next() * 1.99f));
        patch.highPass = static_cast<HighPassMode>(static_cast<int>(next() * 3.99f));
        // Only the three storable chorus states here; I+II is covered below.
        patch.chorus = static_cast<ChorusMode>(static_cast<int>(next() * 2.99f));

        const auto bytes = bytesOf(patch);
        const auto decoded = patchFromToneBytes(bytes.data());
        expect(same(patch, decoded), "a patch did not survive the tone bytes");
        expect(survivesPatchMemory(patch), "a storable patch was reported lossy");

        // And the bytes themselves must be stable, not just the patch.
        const auto again = bytesOf(decoded);
        expect(bytes == again, "the tone bytes are not stable across a round trip");
    }
}

// The one setting the format cannot hold. The hardware cannot store it either,
// so the writer has to degrade predictably and say that it did.
void testTheUnstorableChorusSettingIsReportedRatherThanHidden()
{
    Patch patch {};
    patch.chorus = ChorusMode::OneTwo;
    expect(!survivesPatchMemory(patch),
           "I+II was reported as storable in a patch memory that has no bit for it");

    const auto bytes = bytesOf(patch);
    const auto decoded = patchFromToneBytes(bytes.data());
    expect(decoded.chorus == ChorusMode::Two,
           "I+II was not written as the nearer storable mode");

    for (auto mode : { ChorusMode::Off, ChorusMode::One, ChorusMode::Two })
    {
        Patch storable {};
        storable.chorus = mode;
        expect(survivesPatchMemory(storable), "a storable chorus mode was called lossy");
    }
}

// The framing: what a real message looks like, and what has to be refused.
void testPatchMessageFraming()
{
    Patch patch {};
    patch.cutoff = 1.0f;
    patch.chorus = ChorusMode::One;

    std::array<std::uint8_t, patchMessageBytes> message {};
    const auto written =
        writePatchMessage(patch, 5, message.data(), message.size());
    expect(written == static_cast<std::size_t>(patchMessageBytes),
           "a patch message was not the documented length");
    expect(message[0] == 0xf0, "a patch message does not open with F0");
    expect(message[1] == 0x41, "a patch message does not carry the Roland id");
    expect(message[2] == 0x30, "a patch message does not carry the patch opcode");
    expect(message[3] == 0x05, "the channel nibble was not written");
    expect(message[written - 1] == 0xf7, "a patch message does not close with F7");
    for (std::size_t index = 4; index + 1 < written; ++index)
        expect((message[index] & 0x80u) == 0,
               "a data byte has its status bit set");

    Patch read {};
    int channel = -1;
    expect(readPatchMessage(message.data(), message.size(), read, channel),
           "a message this writer produced was not accepted by its reader");
    expect(channel == 5, "the channel did not survive the round trip");
    expectNear(read.cutoff, 1.0, 1.0e-6, "the patch did not survive the message");
    expect(read.chorus == ChorusMode::One, "the chorus mode did not survive");

    // Everything that is not this message must be refused, and must leave the
    // caller's patch alone.
    const auto refuses = [&](std::vector<std::uint8_t> raw, const char* what) {
        Patch untouched {};
        untouched.cutoff = 0.25f;
        int ignored = -1;
        const auto before = untouched.cutoff;
        expect(!readPatchMessage(raw.data(), raw.size(), untouched, ignored),
               std::string("accepted ") + what);
        expectNear(untouched.cutoff, before, 1.0e-9,
                   std::string("modified the patch while refusing ") + what);
    };

    std::vector<std::uint8_t> good(message.begin(), message.end());
    auto other = good; other[1] = 0x43;
    refuses(other, "a message from another manufacturer");
    auto opcode = good; opcode[2] = 0x31;
    refuses(opcode, "an unknown opcode");
    auto truncated = good; truncated.pop_back();
    refuses(truncated, "a truncated message");
    auto overlong = good; overlong.push_back(0xf7);
    refuses(overlong, "an overlong message");
    auto unterminated = good; unterminated.back() = 0x00;
    refuses(unterminated, "a message with no terminator");
    auto statusByte = good; statusByte[6] = 0x90;
    refuses(statusByte, "a message with a status byte in its body");
    auto highNibble = good; highNibble[3] = 0x15;
    refuses(highNibble, "a message with a dirty channel nibble");
    refuses({}, "an empty message");
}

// The message the hardware sends when one control is moved.
void testParameterMessages()
{
    std::array<std::uint8_t, parameterMessageBytes> message {};
    const auto written = writeParameterMessage(
        static_cast<int>(ToneParameter::VcfFreq), 100, 3, message.data(),
        message.size());
    expect(written == static_cast<std::size_t>(parameterMessageBytes),
           "a parameter message was not the documented length");
    expect(message[2] == 0x32, "a parameter message does not carry its opcode");

    int parameter = -1, value = -1, channel = -1;
    expect(readParameterMessage(message.data(), message.size(), parameter, value,
                                channel),
           "a parameter message this writer produced was refused");
    expect(parameter == 5 && value == 100 && channel == 3,
           "a parameter message did not survive the round trip");

    // Applying it must move that control and nothing else.
    Patch patch {};
    const auto before = patch;
    expect(applyParameter(patch, parameter, value), "a valid parameter was refused");
    expectNear(patch.cutoff, 100.0 / 127.0, 1.0e-6,
               "the parameter did not reach its control");
    expectNear(patch.resonance, before.resonance, 1.0e-9,
               "applying one parameter moved another");

    // A switch byte arriving on its own has to work the same way. What it must
    // not disturb is covered by testASwitchByteLeavesEverythingElseAlone.
    Patch switched {};
    expect(applyParameter(switched, static_cast<int>(ToneParameter::SwitchesTwo), 0x04),
           "a switch byte was refused");
    expect(switched.vcaMode == VcaMode::Gate,
           "a switch byte sent as one parameter did not take effect");

    expect(!applyParameter(patch, 18, 0), "a parameter past the end was accepted");
    expect(!applyParameter(patch, -1, 0), "a negative parameter was accepted");
    expect(parameterValue(patch, 18) < 0, "a parameter past the end returned a value");

    // Reading a parameter back has to agree with the tone bytes.
    const auto bytes = bytesOf(patch);
    for (int index = 0; index < toneByteCount; ++index)
        expect(parameterValue(patch, index) == bytes[static_cast<std::size_t>(index)],
               "a parameter read back disagrees with the tone bytes at index "
                   + std::to_string(index));
}

// A single switch byte must move only the fields that byte encodes. Decoding it
// by round-tripping the whole patch would quantise all sixteen continuous
// controls to seven bits as a side effect, and would drop an unstorable I+II
// chorus while applying a byte that carries no chorus bits at all.
void testASwitchByteLeavesEverythingElseAlone()
{
    Patch patch {};
    // A value that is deliberately not on a 7-bit step, so any quantisation
    // shows up immediately.
    patch.cutoff = 0.5001f;
    patch.resonance = 0.3337f;
    patch.chorus = ChorusMode::OneTwo;
    const auto before = patch;

    // Byte 17 encodes the PWM source, the VCA mode, the polarity and the
    // high-pass. It says nothing about the chorus or any continuous control.
    expect(applyParameter(patch, static_cast<int>(ToneParameter::SwitchesTwo), 0x04),
           "a switch byte was refused");
    expect(patch.vcaMode == VcaMode::Gate, "the switch byte did not take effect");
    expectNear(patch.cutoff, before.cutoff, 1.0e-9,
               "applying a switch byte quantised a continuous control");
    expectNear(patch.resonance, before.resonance, 1.0e-9,
               "applying a switch byte quantised another continuous control");
    expect(patch.chorus == ChorusMode::OneTwo,
           "applying a byte with no chorus bits changed the chorus setting");

    // Byte 16 does carry the chorus, so there it may change -- but the
    // continuous controls still may not.
    Patch second {};
    second.attack = 0.4444f;
    const auto attackBefore = second.attack;
    expect(applyParameter(second, static_cast<int>(ToneParameter::SwitchesOne), 0x12),
           "the first switch byte was refused");
    expect(second.saw, "the first switch byte did not set the saw waveform");
    expect(second.chorus == ChorusMode::Two,
           "the first switch byte did not carry the chorus");
    expectNear(second.attack, attackBefore, 1.0e-9,
               "applying the first switch byte quantised a continuous control");
}

// The shipped bank is a byte-for-byte hardware-memory fixture, not a set of
// rebalanced product sounds. Every entry has to occupy the canonical slot,
// survive the real message format, and retain the independently verified
// 2,304-byte corpus hash.
void testFactoryBankIsWellFormed()
{
    const auto& bank = presets::factoryBank();
    static_assert(presets::presetCount == 128);
    expect(bank.size() == static_cast<std::size_t>(presets::presetCount),
           "the factory bank is not the size it declares");

    std::uint64_t corpusHash = 0xcbf29ce484222325ull;
    for (std::size_t presetIndex = 0; presetIndex < bank.size(); ++presetIndex)
    {
        const auto& preset = bank[presetIndex];
        const std::string where = std::string(preset.number) + " " + preset.name;
        expect(preset.number != nullptr && preset.name != nullptr,
               "a factory preset has no number or name");

        const int withinBank = static_cast<int>(presetIndex % 64);
        const std::array<char, 4> expectedNumber {
            static_cast<char>('A' + presetIndex / 64),
            static_cast<char>('1' + withinBank / 8),
            static_cast<char>('1' + withinBank % 8),
            '\0'
        };
        expect(std::strcmp(preset.number, expectedNumber.data()) == 0,
               where + " is not in canonical A11..A88/B11..B88 order");

        const auto& patch = preset.patch;
        const float* travel[] = {
            &patch.lfoRate, &patch.lfoDelay, &patch.dcoLfo, &patch.pwm,
            &patch.noise, &patch.cutoff, &patch.resonance, &patch.vcfEnv,
            &patch.vcfLfo, &patch.keyFollow, &patch.vcaLevel, &patch.attack,
            &patch.decay, &patch.sustain, &patch.release, &patch.sub
        };
        for (const auto* value : travel)
            expect(*value >= 0.0f && *value <= 1.0f,
                   where + " has a control outside its travel");

        // Every entry must survive a trip through a real patch message, which
        // is what makes the bank sendable to hardware. Continuous decimals are
        // compared in their effective 7-bit representation, not as off-grid
        // source-code floats; an unencodable categorical state is still lossy.
        std::array<std::uint8_t, patchMessageBytes> message {};
        const auto written =
            writePatchMessage(patch, 0, message.data(), message.size());
        expect(written == static_cast<std::size_t>(patchMessageBytes),
               where + " could not be written as a patch message");
        Patch decoded {};
        int channel = -1;
        expect(readPatchMessage(message.data(), written, decoded, channel),
               where + " produced a message its own reader refuses");
        if (survivesPatchMemory(patch))
        {
            expect(bytesOf(decoded) == bytesOf(patch),
                   where + " changed its effective 18-byte patch state");
            expect(decoded.chorus == patch.chorus,
                   where + " lost its categorical chorus setting");

            for (const auto byte : bytesOf(patch))
            {
                corpusHash ^= byte;
                corpusHash *= 0x100000001b3ull;
            }
        }
        else
            expect(patch.chorus == ChorusMode::OneTwo,
                   where + " was reported lossy for a reason other than I+II");
    }

    // The numbering has to be unique, and reachable by name.
    for (std::size_t a = 0; a < bank.size(); ++a)
    {
        expect(presets::findByNumber(bank[a].number) == &bank[a],
               std::string("cannot look up preset ") + bank[a].number);
        for (std::size_t b = a + 1; b < bank.size(); ++b)
            expect(std::string(bank[a].number) != bank[b].number,
                   std::string("duplicate preset number ") + bank[a].number);
    }
    expect(presets::findByNumber("Z99") == nullptr,
           "an unknown preset number returned a preset");
    expect(presets::findByNumber(nullptr) == nullptr,
           "a null preset number was not refused");

    // FNV is kept in the executable for a dependency-free regression. The
    // corresponding SHA-256, checked from three public representations, is
    // 394ae874da33aa63fa4833932fbf415546d2ad66b1b6b9a36315601799eeec21.
    expect(corpusHash == 0xa78dab9d5bafb386ull,
           "the original 128 factory tone corpus changed");

    constexpr std::array<std::uint8_t, toneByteCount> a11 {
        0x14, 0x31, 0x00, 0x66, 0x00, 0x23, 0x0d, 0x3a, 0x00,
        0x56, 0x6c, 0x03, 0x31, 0x2d, 0x20, 0x00, 0x51, 0x11
    };
    constexpr std::array<std::uint8_t, toneByteCount> a88 {
        0x00, 0x00, 0x00, 0x66, 0x7f, 0x44, 0x76, 0x00, 0x00,
        0x45, 0x6b, 0x00, 0x0d, 0x26, 0x2f, 0x00, 0x21, 0x08
    };
    constexpr std::array<std::uint8_t, toneByteCount> b11 {
        0x39, 0x2d, 0x00, 0x37, 0x00, 0x55, 0x00, 0x00, 0x00,
        0x6c, 0x34, 0x3b, 0x20, 0x56, 0x28, 0x00, 0x1a, 0x18
    };
    constexpr std::array<std::uint8_t, toneByteCount> b88 {
        0x32, 0x00, 0x00, 0x2d, 0x00, 0x26, 0x54, 0x20, 0x00,
        0x7f, 0x65, 0x00, 0x31, 0x37, 0x00, 0x38, 0x39, 0x19
    };
    expect(bytesOf(bank[0].patch) == a11, "A11 source-byte fixture changed");
    expect(bytesOf(bank[63].patch) == a88, "A88 source-byte fixture changed");
    expect(bytesOf(bank[64].patch) == b11, "B11 source-byte fixture changed");
    expect(bytesOf(bank[127].patch) == b88, "B88 source-byte fixture changed");
    // These gate-mode factory tones are a behavioral tripwire for the two
    // adjacent switch-byte fields. Swapping VCF polarity bit 1 and VCA-mode
    // bit 2 leaves every encode/decode round trip green while making all three
    // sounds nearly silent by inverting their filter envelope and closing the
    // amplifier with the wrong envelope.
    for (const std::size_t index : { std::size_t { 61 }, std::size_t { 80 },
                                     std::size_t { 121 } })
    {
        expect(bank[index].patch.envPolarity == EnvPolarity::Normal,
               std::string(bank[index].number)
                   + " decoded the factory VCF envelope with wrong polarity");
        expect(bank[index].patch.vcaMode == VcaMode::Gate,
               std::string(bank[index].number)
                   + " did not decode the factory VCA Gate bit");
    }
    expect(std::strcmp(bank[0].name, "Brass Set 1") == 0,
           "A11 archival name changed");
    expect(std::strcmp(bank[127].name, "Owgan") == 0,
           "B88 archival name changed");
}
} // namespace

int main()
{
    testContinuousControlsSitAtTheDocumentedIndices();
    testSwitchBitsMatchTheDocumentedLayout();
    testMalformedRangeBitsResolveRatherThanReject();
    testPatchesRoundTripThroughTheToneBytes();
    testTheUnstorableChorusSettingIsReportedRatherThanHidden();
    testPatchMessageFraming();
    testParameterMessages();
    testASwitchByteLeavesEverythingElseAlone();
    testFactoryBankIsWellFormed();

    if (failures > 0)
    {
        std::cerr << failures << " system-exclusive check(s) failed\n";
        return 1;
    }
    std::cout << "All system-exclusive checks passed\n";
    return 0;
}

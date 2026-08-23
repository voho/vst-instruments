#include "SeptumSysEx.h"

#include <algorithm>
#include <cstring>

namespace septum::sysex
{

namespace
{
    [[nodiscard]] inline std::uint8_t clampTo7Bit (int value) noexcept
    {
        return static_cast<std::uint8_t> (std::clamp (value, 0, 127));
    }

    [[nodiscard]] inline std::uint8_t signedTo7Bit (int value) noexcept
    {
        return clampTo7Bit (value + 64);
    }

    [[nodiscard]] inline int from7BitSigned (std::uint8_t raw) noexcept
    {
        return static_cast<int> (raw & 0x7Fu) - 64;
    }

    // Fields the address map marks with a leading "#" are nibbled:
    // "Data marked '#' is expressed in hexadecimal in 4-bit units. A value
    // expressed as a 2-byte nibble 0a 0bH has the value of a x 16+b."
    // (MIDI Implementation v1.00, Supplementary Material, p. 6, whose worked
    // example extends the rule to more than two: 0A 03 09 0D = 41885.)
    // Most significant nibble first.
    inline void writeNibbles (std::uint8_t* dest, int nibbleCount, int value) noexcept
    {
        for (int i = nibbleCount - 1; i >= 0; --i)
        {
            dest[i] = static_cast<std::uint8_t> (value & 0x0F);
            value >>= 4;
        }
    }

    [[nodiscard]] inline int readNibbles (const std::uint8_t* src, int nibbleCount) noexcept
    {
        int value = 0;
        for (int i = 0; i < nibbleCount; ++i)
            value = (value << 4) | (src[i] & 0x0F);
        return value;
    }
} // namespace

// Patch Common, offsets 00..20 exactly as the address map lists them. Four
// of them used to be somewhere else in this codec: PATCH TEMPO was a 7-bit
// split rather than the map's three nibbles (so 256..300 wrapped -- 300 came
// back as 44), SPLIT ARPEGGIO was missing entirely, the DELAY and REVERB
// switches shared 0x14 as an invented bitmask, and the arpeggio's own two
// switches sat at 0x1C/0x1D on top of them.
void encodePatchCommon (const Patch& patch, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    std::memset (dest, 0, sizePatchCommon);

    // 00..0B: Patch Name (12 ASCII characters, padded with spaces)
    for (std::size_t i = 0; i < 12; ++i)
    {
        char c = (i < patch.name.size()) ? patch.name[i] : ' ';
        if (c < 0x20 || c > 0x7E)
            c = ' ';
        dest[i] = static_cast<std::uint8_t> (c);
    }

    dest[0x0C] = clampTo7Bit (patch.patchLevel);
    dest[0x0D] = signedTo7Bit (patch.toneBalance);

    // #00 0E / 0F / 10: Patch Tempo (5 - 300), three nibbles.
    writeNibbles (dest + 0x0E, 3, std::clamp (patch.tempo, 5, 300));

    dest[0x11] = static_cast<std::uint8_t> (patch.keyboardMode);
    dest[0x12] = static_cast<std::uint8_t> (patch.keyboardPart);
    dest[0x13] = static_cast<std::uint8_t> (std::clamp (patch.splitPoint, 21, 108));
    dest[0x14] = static_cast<std::uint8_t> (patch.arpeggio.splitArpeggio);
    dest[0x15] = static_cast<std::uint8_t> (patch.modulationDestination);
    dest[0x16] = static_cast<std::uint8_t> (patch.dBeamDestination);
    dest[0x17] = static_cast<std::uint8_t> (patch.pitchBendDestination);
    dest[0x18] = static_cast<std::uint8_t> (patch.expressionDestination);
    dest[0x19] = patch.activeExpression ? 1 : 0;
    dest[0x1A] = patch.arpeggio.on ? 1 : 0;
    dest[0x1B] = patch.arpeggio.hold ? 1 : 0;
    dest[0x1C] = patch.delayOn ? 1 : 0;
    dest[0x1D] = patch.reverbOn ? 1 : 0;
    dest[0x1E] = static_cast<std::uint8_t> (patch.modulationAssign);
    dest[0x1F] = static_cast<std::uint8_t> (patch.dBeamAssign);
    dest[0x20] = static_cast<std::uint8_t> (patch.dBeamPolarity);
}

void encodeTonePatch (const TonePatch& tone, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    std::memset (dest, 0, sizeTonePatch);

    dest[0x00] = static_cast<std::uint8_t> (tone.osc1.wave);
    dest[0x01] = tone.osc1.pitchWide ? 1 : 0;
    dest[0x02] = signedTo7Bit (tone.osc1.coarse);
    dest[0x03] = signedTo7Bit (tone.osc1.fine);
    dest[0x04] = clampTo7Bit (tone.osc1.pulseWidth);
    dest[0x05] = signedTo7Bit (tone.osc1.pitchEnvDepth);

    dest[0x06] = static_cast<std::uint8_t> (tone.osc2.wave);
    dest[0x07] = tone.osc2.pitchWide ? 1 : 0;
    dest[0x08] = signedTo7Bit (tone.osc2.coarse);
    dest[0x09] = signedTo7Bit (tone.osc2.fine);
    dest[0x0A] = clampTo7Bit (tone.osc2.pulseWidth);
    dest[0x0B] = signedTo7Bit (tone.osc2.pitchEnvDepth);

    dest[0x0C] = clampTo7Bit (tone.pitchEnvAttack);
    dest[0x0D] = clampTo7Bit (tone.pitchEnvDecay);

    dest[0x0E] = static_cast<std::uint8_t> (tone.mixType);
    dest[0x0F] = signedTo7Bit (tone.balance);
    dest[0x10] = static_cast<std::uint8_t> (tone.lowFreq);

    dest[0x11] = static_cast<std::uint8_t> (tone.filterType);
    dest[0x12] = static_cast<std::uint8_t> (tone.filterSlope);
    dest[0x13] = clampTo7Bit (tone.cutoff);
    // Key follow: raw is 44..84 (step of 10 around 64)
    dest[0x14] = clampTo7Bit (static_cast<int> (std::lround (tone.keyFollow / 10.0)) + 64);
    dest[0x15] = signedTo7Bit (tone.cutoffVelocitySens);
    dest[0x16] = clampTo7Bit (tone.resonance);

    dest[0x17] = clampTo7Bit (tone.filterEnvAttack);
    dest[0x18] = clampTo7Bit (tone.filterEnvDecay);
    dest[0x19] = clampTo7Bit (tone.filterEnvSustain);
    dest[0x1A] = clampTo7Bit (tone.filterEnvRelease);
    dest[0x1B] = signedTo7Bit (tone.filterEnvDepth);

    dest[0x1C] = tone.overdrive ? 1 : 0;
    dest[0x1D] = clampTo7Bit (tone.drive);
    dest[0x1E] = clampTo7Bit (tone.level);
    dest[0x1F] = signedTo7Bit (tone.levelVelocitySens);
    dest[0x20] = signedTo7Bit (tone.pan);

    dest[0x21] = clampTo7Bit (tone.ampEnvAttack);
    dest[0x22] = clampTo7Bit (tone.ampEnvDecay);
    dest[0x23] = clampTo7Bit (tone.ampEnvSustain);
    dest[0x24] = clampTo7Bit (tone.ampEnvRelease);

    dest[0x25] = clampTo7Bit (tone.delayDepth);
    dest[0x26] = clampTo7Bit (tone.reverbDepth);

    dest[0x27] = static_cast<std::uint8_t> (tone.lfo1.shape);
    dest[0x28] = clampTo7Bit (tone.lfo1.rate);
    // 0x29 / 0x33: "LFO Tempo Sync Switch (0 - 1) / ON, OFF". Every other
    // switch in the address map reads OFF, ON; these two are listed the other
    // way round, once each for LFO1 and LFO2, so the reversal is the
    // document's and not a slip in one line of it. Encoded as written --
    // 0 is ON -- and flagged in the research contract as the one place where
    // following the map contradicts the pattern of the map. (OQ-18)
    dest[0x29] = tone.lfo1.tempoSync ? 0 : 1;
    dest[0x2A] = clampTo7Bit (tone.lfo1.tempoSyncNote);
    dest[0x2B] = clampTo7Bit (tone.lfo1.fadeTime);
    dest[0x2C] = tone.lfo1.keyTrigger ? 1 : 0;
    dest[0x2D] = static_cast<std::uint8_t> (tone.lfo1.destination1);
    dest[0x2E] = signedTo7Bit (tone.lfo1.depth1);
    dest[0x2F] = static_cast<std::uint8_t> (tone.lfo1.destination2);
    dest[0x30] = signedTo7Bit (tone.lfo1.depth2);

    dest[0x31] = static_cast<std::uint8_t> (tone.lfo2.shape);
    dest[0x32] = clampTo7Bit (tone.lfo2.rate);
    dest[0x33] = tone.lfo2.tempoSync ? 0 : 1;   // 0 is ON; see 0x29 above
    dest[0x34] = clampTo7Bit (tone.lfo2.tempoSyncNote);
    dest[0x35] = clampTo7Bit (tone.lfo2.fadeTime);
    dest[0x36] = tone.lfo2.keyTrigger ? 1 : 0;
    dest[0x37] = static_cast<std::uint8_t> (tone.lfo2.destination1);
    dest[0x38] = signedTo7Bit (tone.lfo2.depth1);
    dest[0x39] = static_cast<std::uint8_t> (tone.lfo2.destination2);
    dest[0x3A] = signedTo7Bit (tone.lfo2.depth2);

    dest[0x3B] = clampTo7Bit (tone.bendRange);
    dest[0x3C] = signedTo7Bit (tone.octaveShift);
    dest[0x3D] = tone.portamento ? 1 : 0;
    dest[0x3E] = clampTo7Bit (tone.portamentoTime);
    dest[0x3F] = static_cast<std::uint8_t> (tone.mono);
}

void encodeDelayParams (const DelayParams& delay, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    dest[0] = clampTo7Bit (delay.time);                             // 0-127
    // Feedback (0 - 98) = -98 - +98 [%], so 49 is 0%.
    dest[1] = static_cast<std::uint8_t> (
        std::clamp (static_cast<int> (std::lround (delay.feedback / 2.0)) + 49, 0, 98));
    dest[2] = static_cast<std::uint8_t> (std::clamp (delay.hfDamp, 0, 17));
    dest[3] = clampTo7Bit (delay.modulationRate);                   // 0-127
    dest[4] = clampTo7Bit (delay.modulationDepth);                  // 0-127
}

void encodeReverbParams (const ReverbParams& reverb, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    dest[0] = clampTo7Bit (reverb.time);                                 // 0-127
    dest[1] = static_cast<std::uint8_t> (std::clamp (reverb.preDelay, 0, 125));
    dest[2] = static_cast<std::uint8_t> (std::clamp (reverb.size, 0, 7));
    dest[3] = static_cast<std::uint8_t> (std::clamp (reverb.highCut, 0, 20));
    dest[4] = clampTo7Bit (reverb.density);                              // 0-127
    dest[5] = clampTo7Bit (reverb.diffusion);                            // 0-127
    dest[6] = static_cast<std::uint8_t> (std::clamp (reverb.lfDampFrequency, 0, 19));
    // LF/HF Damp Gain (0 - 36) = -36 - 0 [dB]: the raw byte counts up from
    // -36 dB, it is not one of the map's +/-64-biased fields. Encoding it as
    // one put 0 dB at byte 64 and -36 dB at byte 28, both outside the
    // documented range.
    dest[7] = static_cast<std::uint8_t> (std::clamp (reverb.lfDampGain, -36, 0) + 36);
    dest[8] = static_cast<std::uint8_t> (std::clamp (reverb.hfDampFrequency, 0, 5));
    dest[9] = static_cast<std::uint8_t> (std::clamp (reverb.hfDampGain, -36, 0) + 36);
}

void encodeArpeggioCommon (const ArpeggioParams& arp, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    std::memset (dest, 0, sizeArpeggioCommon);

    dest[0x00] = static_cast<std::uint8_t> (arp.grid);          // 0-8
    dest[0x01] = static_cast<std::uint8_t> (arp.duration);      // 0-9
    dest[0x02] = static_cast<std::uint8_t> (arp.motif);         // 0-11
    dest[0x03] = signedTo7Bit (arp.octaveRange);                // 61-67 = -3..+3
    dest[0x04] = static_cast<std::uint8_t> (std::clamp (arp.accent, 0, 100));
    dest[0x05] = clampTo7Bit (arp.velocity);                    // 0 = REAL
    // #00 06 / 07: End Step (1 - 32), two nibbles.
    writeNibbles (dest + 0x06, 2, std::clamp (arp.style.endStep, 1, arpeggioMaxSteps));

    // `styleIndex` has no home here on purpose. The hardware stores the grid
    // itself in the patch and its panel only selects a template, so the
    // address map has no style-number parameter: this block plus the sixteen
    // pattern blocks *are* the style. The plug-in's selector index rides in
    // the host's parameter state instead.
}

void encodeArpeggioPattern (const ArpeggioStyle& style, int row, std::uint8_t* dest) noexcept
{
    if (dest == nullptr || row < 0 || row >= arpeggioMaxRows)
        return;
    std::memset (dest, 0, sizeArpeggioPattern);

    // #00 00: Original Note (0 - 128), then #00 02, 00 04 ... 00 40:
    // Step1..Step32 Data (0 - 128). Every field is a two-byte nibble.
    writeNibbles (dest, 2,
                  std::clamp (style.originalNote[static_cast<std::size_t> (row)], 0, 128));

    for (int step = 0; step < arpeggioMaxSteps; ++step)
    {
        const signed char cell = style.cell (step, row);
        const int wire = (cell == arpeggioTie)  ? arpeggioTieValue
                       : (cell <= 0)            ? arpeggioRestValue
                                                : static_cast<int> (cell);
        writeNibbles (dest + 2 + 2 * step, 2, wire);
    }
}

// --------------------------------------------------------------------------
// Block Deserializers
// --------------------------------------------------------------------------

void decodePatchCommon (const std::uint8_t* src, std::size_t size, Patch& patch) noexcept
{
    if (src == nullptr || size < 0x0C)
        return;

    // Patch name
    char nameBuf[13] {};
    for (std::size_t i = 0; i < 12 && i < size; ++i)
    {
        const char c = static_cast<char> (src[i]);
        nameBuf[i] = (c >= 0x20 && c <= 0x7E) ? c : ' ';
    }
    // Trim trailing spaces
    int end = 11;
    while (end >= 0 && nameBuf[end] == ' ')
        nameBuf[end--] = '\0';
    patch.name = nameBuf;
    if (patch.name.empty())
        patch.name = "INIT PATCH";

    if (size > 0x0C) patch.patchLevel = src[0x0C] & 0x7Fu;
    if (size > 0x0D) patch.toneBalance = from7BitSigned (src[0x0D]);

    if (size > 0x10)
    {
        const int tempo = readNibbles (src + 0x0E, 3);
        if (tempo >= 5 && tempo <= 300)
            patch.tempo = tempo;
    }

    if (size > 0x11)
        patch.keyboardMode = static_cast<KeyboardMode> (
            std::clamp<int> (src[0x11], 0, 2));
    if (size > 0x12)
        patch.keyboardPart = static_cast<KeyboardPart> (
            std::clamp<int> (src[0x12], 0, 1));
    if (size > 0x13)
        patch.splitPoint = std::clamp<int> (src[0x13], 21, 108);

    if (size > 0x14)
        patch.arpeggio.splitArpeggio = static_cast<SplitArpeggio> (
            std::clamp<int> (src[0x14], 0, 2));

    if (size > 0x15)
        patch.modulationDestination = static_cast<ToneDestination> (
            std::clamp<int> (src[0x15], 0, 2));
    if (size > 0x16)
        patch.dBeamDestination = static_cast<ToneDestination> (
            std::clamp<int> (src[0x16], 0, 2));
    if (size > 0x17)
        patch.pitchBendDestination = static_cast<ToneDestination> (
            std::clamp<int> (src[0x17], 0, 2));
    if (size > 0x18)
        patch.expressionDestination = static_cast<ToneDestination> (
            std::clamp<int> (src[0x18], 0, 2));
    if (size > 0x19)
        patch.activeExpression = (src[0x19] & 1u) != 0;

    if (size > 0x1A) patch.arpeggio.on = (src[0x1A] & 1u) != 0;
    if (size > 0x1B) patch.arpeggio.hold = (src[0x1B] & 1u) != 0;
    if (size > 0x1C) patch.delayOn = (src[0x1C] & 1u) != 0;
    if (size > 0x1D) patch.reverbOn = (src[0x1D] & 1u) != 0;
    if (size > 0x1E)
        patch.modulationAssign = static_cast<ModulationAssign> (
            std::clamp<int> (src[0x1E], 0, 7));
    if (size > 0x1F)
        patch.dBeamAssign = static_cast<DBeamAssign> (
            std::clamp<int> (src[0x1F], 0, dBeamAssignCount - 1));
    if (size > 0x20)
        patch.dBeamPolarity = static_cast<DBeamPolarity> (
            std::clamp<int> (src[0x20], 0, 1));
}

void decodeTonePatch (const std::uint8_t* src, std::size_t size, TonePatch& tone) noexcept
{
    if (src == nullptr || size == 0)
        return;

    const auto get = [src, size] (std::size_t idx, std::uint8_t def = 0)
    {
        return idx < size ? src[idx] : def;
    };

    if (size > 0x00) tone.osc1.wave = static_cast<Waveform> (std::clamp<int> (get (0x00), 0, 8));
    if (size > 0x01) tone.osc1.pitchWide = (get (0x01) & 1u) != 0;
    if (size > 0x02) tone.osc1.coarse = from7BitSigned (get (0x02));
    if (size > 0x03) tone.osc1.fine = from7BitSigned (get (0x03));
    if (size > 0x04) tone.osc1.pulseWidth = get (0x04) & 0x7Fu;
    if (size > 0x05) tone.osc1.pitchEnvDepth = from7BitSigned (get (0x05));

    if (size > 0x06) tone.osc2.wave = static_cast<Waveform> (std::clamp<int> (get (0x06), 0, 8));
    if (size > 0x07) tone.osc2.pitchWide = (get (0x07) & 1u) != 0;
    if (size > 0x08) tone.osc2.coarse = from7BitSigned (get (0x08));
    if (size > 0x09) tone.osc2.fine = from7BitSigned (get (0x09));
    if (size > 0x0A) tone.osc2.pulseWidth = get (0x0A) & 0x7Fu;
    if (size > 0x0B) tone.osc2.pitchEnvDepth = from7BitSigned (get (0x0B));

    if (size > 0x0C) tone.pitchEnvAttack = get (0x0C) & 0x7Fu;
    if (size > 0x0D) tone.pitchEnvDecay = get (0x0D) & 0x7Fu;

    if (size > 0x0E) tone.mixType = static_cast<MixModType> (std::clamp<int> (get (0x0E), 0, 2));
    if (size > 0x0F) tone.balance = from7BitSigned (get (0x0F));
    if (size > 0x10) tone.lowFreq = static_cast<LowFreqMode> (std::clamp<int> (get (0x10), 0, 2));

    if (size > 0x11) tone.filterType = static_cast<FilterType> (std::clamp<int> (get (0x11), 0, 3));
    if (size > 0x12) tone.filterSlope = static_cast<FilterSlope> (std::clamp<int> (get (0x12), 0, 1));
    if (size > 0x13) tone.cutoff = get (0x13) & 0x7Fu;
    if (size > 0x14) tone.keyFollow = (from7BitSigned (get (0x14))) * 10;
    if (size > 0x15) tone.cutoffVelocitySens = from7BitSigned (get (0x15));
    if (size > 0x16) tone.resonance = get (0x16) & 0x7Fu;

    if (size > 0x17) tone.filterEnvAttack = get (0x17) & 0x7Fu;
    if (size > 0x18) tone.filterEnvDecay = get (0x18) & 0x7Fu;
    if (size > 0x19) tone.filterEnvSustain = get (0x19) & 0x7Fu;
    if (size > 0x1A) tone.filterEnvRelease = get (0x1A) & 0x7Fu;
    if (size > 0x1B) tone.filterEnvDepth = from7BitSigned (get (0x1B));

    if (size > 0x1C) tone.overdrive = (get (0x1C) & 1u) != 0;
    if (size > 0x1D) tone.drive = get (0x1D) & 0x7Fu;
    if (size > 0x1E) tone.level = get (0x1E) & 0x7Fu;
    if (size > 0x1F) tone.levelVelocitySens = from7BitSigned (get (0x1F));
    if (size > 0x20) tone.pan = from7BitSigned (get (0x20));

    if (size > 0x21) tone.ampEnvAttack = get (0x21) & 0x7Fu;
    if (size > 0x22) tone.ampEnvDecay = get (0x22) & 0x7Fu;
    if (size > 0x23) tone.ampEnvSustain = get (0x23) & 0x7Fu;
    if (size > 0x24) tone.ampEnvRelease = get (0x24) & 0x7Fu;

    if (size > 0x25) tone.delayDepth = get (0x25) & 0x7Fu;
    if (size > 0x26) tone.reverbDepth = get (0x26) & 0x7Fu;

    if (size > 0x27) tone.lfo1.shape = static_cast<LfoShape> (std::clamp<int> (get (0x27), 0, 6));
    if (size > 0x28) tone.lfo1.rate = get (0x28) & 0x7Fu;
    if (size > 0x29) tone.lfo1.tempoSync = (get (0x29) & 1u) == 0;   // 0 is ON
    if (size > 0x2A) tone.lfo1.tempoSyncNote = std::clamp<int> (get (0x2A), 0, 19);
    if (size > 0x2B) tone.lfo1.fadeTime = get (0x2B) & 0x7Fu;
    if (size > 0x2C) tone.lfo1.keyTrigger = (get (0x2C) & 1u) != 0;
    if (size > 0x2D) tone.lfo1.destination1 = static_cast<LfoDest1> (std::clamp<int> (get (0x2D), 0, 3));
    if (size > 0x2E) tone.lfo1.depth1 = from7BitSigned (get (0x2E));
    if (size > 0x2F) tone.lfo1.destination2 = static_cast<LfoDest2> (std::clamp<int> (get (0x2F), 0, 2));
    if (size > 0x30) tone.lfo1.depth2 = from7BitSigned (get (0x30));

    if (size > 0x31) tone.lfo2.shape = static_cast<LfoShape> (std::clamp<int> (get (0x31), 0, 6));
    if (size > 0x32) tone.lfo2.rate = get (0x32) & 0x7Fu;
    if (size > 0x33) tone.lfo2.tempoSync = (get (0x33) & 1u) == 0;   // 0 is ON
    if (size > 0x34) tone.lfo2.tempoSyncNote = std::clamp<int> (get (0x34), 0, 19);
    if (size > 0x35) tone.lfo2.fadeTime = get (0x35) & 0x7Fu;
    if (size > 0x36) tone.lfo2.keyTrigger = (get (0x36) & 1u) != 0;
    if (size > 0x37) tone.lfo2.destination1 = static_cast<LfoDest1> (std::clamp<int> (get (0x37), 0, 3));
    if (size > 0x38) tone.lfo2.depth1 = from7BitSigned (get (0x38));
    if (size > 0x39) tone.lfo2.destination2 = static_cast<LfoDest2> (std::clamp<int> (get (0x39), 0, 2));
    if (size > 0x3A) tone.lfo2.depth2 = from7BitSigned (get (0x3A));

    if (size > 0x3B) tone.bendRange = std::clamp<int> (get (0x3B), 0, 24);
    if (size > 0x3C) tone.octaveShift = std::clamp<int> (from7BitSigned (get (0x3C)), -3, 3);
    if (size > 0x3D) tone.portamento = (get (0x3D) & 1u) != 0;
    if (size > 0x3E) tone.portamentoTime = get (0x3E) & 0x7Fu;
    if (size > 0x3F) tone.mono = static_cast<MonoMode> (std::clamp<int> (get (0x3F), 0, 2));
}

void decodeDelayParams (const std::uint8_t* src, std::size_t size, DelayParams& delay) noexcept
{
    if (src == nullptr || size == 0)
        return;
    if (size > 0) delay.time = src[0] & 0x7Fu;
    if (size > 1) delay.feedback = (static_cast<int> (src[1] & 0x7Fu) - 49) * 2;
    if (size > 2) delay.hfDamp = std::clamp<int> (src[2], 0, 17);
    if (size > 3) delay.modulationRate = src[3] & 0x7Fu;
    if (size > 4) delay.modulationDepth = src[4] & 0x7Fu;
}

void decodeReverbParams (const std::uint8_t* src, std::size_t size, ReverbParams& reverb) noexcept
{
    if (src == nullptr || size == 0)
        return;
    if (size > 0) reverb.time = src[0] & 0x7Fu;
    if (size > 1) reverb.preDelay = std::clamp<int> (src[1], 0, 125);
    if (size > 2) reverb.size = std::clamp<int> (src[2], 0, 7);
    if (size > 3) reverb.highCut = std::clamp<int> (src[3], 0, 20);
    if (size > 4) reverb.density = src[4] & 0x7Fu;
    if (size > 5) reverb.diffusion = src[5] & 0x7Fu;
    if (size > 6) reverb.lfDampFrequency = std::clamp<int> (src[6], 0, 19);
    if (size > 7) reverb.lfDampGain = std::clamp<int> (src[7], 0, 36) - 36;
    if (size > 8) reverb.hfDampFrequency = std::clamp<int> (src[8], 0, 5);
    if (size > 9) reverb.hfDampGain = std::clamp<int> (src[9], 0, 36) - 36;
}

void decodeArpeggioCommon (const std::uint8_t* src, std::size_t size, ArpeggioParams& arp) noexcept
{
    if (src == nullptr || size == 0)
        return;
    if (size > 0x00) arp.grid = static_cast<ArpeggioGrid> (std::clamp<int> (src[0x00], 0, 8));
    if (size > 0x01) arp.duration = static_cast<ArpeggioDuration> (std::clamp<int> (src[0x01], 0, 9));
    if (size > 0x02) arp.motif = static_cast<ArpeggioMotif> (std::clamp<int> (src[0x02], 0, 11));
    if (size > 0x03) arp.octaveRange = std::clamp<int> (from7BitSigned (src[0x03]), -3, 3);
    if (size > 0x04) arp.accent = std::clamp<int> (src[0x04], 0, 100);
    if (size > 0x05) arp.velocity = src[0x05] & 0x7Fu;
    if (size > 0x07)
    {
        const int endStep =
            std::clamp (readNibbles (src + 0x06, 2), 1, arpeggioMaxSteps);
        arp.style.endStep = endStep;
        // END STEP is one control on the hardware, 1-32, and the replica's
        // panel is the same control with a zero added *below* the documented
        // range meaning "as long as the loaded style is". A documented value
        // therefore maps straight onto the panel's, which is also the only
        // way it survives the trip through the plug-in: `style.endStep` has
        // no parameter to live in, and `arp.endStep` does.
        arp.endStep = endStep;
    }
}

void decodeArpeggioPattern (const std::uint8_t* src, std::size_t size, int row,
                            ArpeggioStyle& style) noexcept
{
    if (src == nullptr || size < 2 || row < 0 || row >= arpeggioMaxRows)
        return;

    style.originalNote[static_cast<std::size_t> (row)] =
        std::clamp (readNibbles (src, 2), 0, 128);

    for (int step = 0; step < arpeggioMaxSteps; ++step)
    {
        const std::size_t at = static_cast<std::size_t> (2 + 2 * step);
        if (at + 1 >= size)
            break;
        const int wire = readNibbles (src + at, 2);
        auto& cell = style.cells[static_cast<std::size_t> (step)][static_cast<std::size_t> (row)];
        cell = (wire >= arpeggioTieValue) ? arpeggioTie
             : (wire <= arpeggioRestValue) ? arpeggioRest
                                           : static_cast<signed char> (wire);
    }
}

// --------------------------------------------------------------------------
// Full Patch & Packet Codec
// --------------------------------------------------------------------------

std::vector<std::uint8_t> makeDt1Message (std::uint32_t address,
                                          const std::uint8_t* data,
                                          std::size_t dataSize,
                                          std::uint8_t deviceId)
{
    // F0 41 <dev> 00 00 16 12 <addr0..3> <data...> <sum> F7
    // Total size = 1 + 1 + 1 + 3 + 1 + 4 + dataSize + 1 + 1 = 13 + dataSize
    std::vector<std::uint8_t> msg;
    msg.reserve (13 + dataSize);

    msg.push_back (0xF0);
    msg.push_back (rolandId);
    msg.push_back (deviceId & 0x1Fu);
    msg.push_back (sh201ModelId[0]);
    msg.push_back (sh201ModelId[1]);
    msg.push_back (sh201ModelId[2]);
    msg.push_back (cmdDt1);

    const std::uint8_t addr0 = static_cast<std::uint8_t> ((address >> 24) & 0x7Fu);
    const std::uint8_t addr1 = static_cast<std::uint8_t> ((address >> 16) & 0x7Fu);
    const std::uint8_t addr2 = static_cast<std::uint8_t> ((address >> 8) & 0x7Fu);
    const std::uint8_t addr3 = static_cast<std::uint8_t> (address & 0x7Fu);

    msg.push_back (addr0);
    msg.push_back (addr1);
    msg.push_back (addr2);
    msg.push_back (addr3);

    std::vector<std::uint8_t> checksumPayload;
    checksumPayload.reserve (4 + dataSize);
    checksumPayload.push_back (addr0);
    checksumPayload.push_back (addr1);
    checksumPayload.push_back (addr2);
    checksumPayload.push_back (addr3);

    for (std::size_t i = 0; i < dataSize; ++i)
    {
        const std::uint8_t b = data[i] & 0x7Fu;
        msg.push_back (b);
        checksumPayload.push_back (b);
    }

    const std::uint8_t sum = calculateChecksum (checksumPayload.data(),
                                                checksumPayload.size());
    msg.push_back (sum);
    msg.push_back (0xF7);
    return msg;
}

std::vector<std::uint8_t> makeRq1Message (std::uint32_t address,
                                          std::uint32_t size,
                                          std::uint8_t deviceId)
{
    // F0 41 <dev> 00 00 16 11 <addr0..3> <size0..3> <sum> F7
    std::vector<std::uint8_t> msg;
    msg.reserve (17);

    msg.push_back (0xF0);
    msg.push_back (rolandId);
    msg.push_back (deviceId & 0x1Fu);
    msg.push_back (sh201ModelId[0]);
    msg.push_back (sh201ModelId[1]);
    msg.push_back (sh201ModelId[2]);
    msg.push_back (cmdRq1);

    const std::uint8_t addr0 = static_cast<std::uint8_t> ((address >> 24) & 0x7Fu);
    const std::uint8_t addr1 = static_cast<std::uint8_t> ((address >> 16) & 0x7Fu);
    const std::uint8_t addr2 = static_cast<std::uint8_t> ((address >> 8) & 0x7Fu);
    const std::uint8_t addr3 = static_cast<std::uint8_t> (address & 0x7Fu);

    const std::uint8_t sz0 = static_cast<std::uint8_t> ((size >> 24) & 0x7Fu);
    const std::uint8_t sz1 = static_cast<std::uint8_t> ((size >> 16) & 0x7Fu);
    const std::uint8_t sz2 = static_cast<std::uint8_t> ((size >> 8) & 0x7Fu);
    const std::uint8_t sz3 = static_cast<std::uint8_t> (size & 0x7Fu);

    msg.push_back (addr0);
    msg.push_back (addr1);
    msg.push_back (addr2);
    msg.push_back (addr3);

    msg.push_back (sz0);
    msg.push_back (sz1);
    msg.push_back (sz2);
    msg.push_back (sz3);

    const std::array<std::uint8_t, 8> payload {
        addr0, addr1, addr2, addr3, sz0, sz1, sz2, sz3
    };
    msg.push_back (calculateChecksum (payload.data(), payload.size()));
    msg.push_back (0xF7);
    return msg;
}

std::vector<std::vector<std::uint8_t>> encodePatchToSysExPackets (
    const Patch& patch, std::uint32_t baseAddress, std::uint8_t deviceId)
{
    std::vector<std::vector<std::uint8_t>> packets;
    packets.reserve (patchBlockCount);

    std::array<std::uint8_t, sizePatchCommon> commonBytes {};
    encodePatchCommon (patch, commonBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetPatchCommon, commonBytes.data(),
                                       commonBytes.size(), deviceId));

    std::array<std::uint8_t, sizeTonePatch> upperBytes {};
    encodeTonePatch (patch.upper, upperBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetUpperTone, upperBytes.data(),
                                       upperBytes.size(), deviceId));

    std::array<std::uint8_t, sizeTonePatch> lowerBytes {};
    encodeTonePatch (patch.lower, lowerBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetLowerTone, lowerBytes.data(),
                                       lowerBytes.size(), deviceId));

    std::array<std::uint8_t, sizeDelay> delayBytes {};
    encodeDelayParams (patch.delay, delayBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetDelay, delayBytes.data(),
                                       delayBytes.size(), deviceId));

    std::array<std::uint8_t, sizeReverb> reverbBytes {};
    encodeReverbParams (patch.reverb, reverbBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetReverb, reverbBytes.data(),
                                       reverbBytes.size(), deviceId));

    std::array<std::uint8_t, sizeArpeggioCommon> arpBytes {};
    encodeArpeggioCommon (patch.arpeggio, arpBytes.data());
    packets.push_back (makeDt1Message (baseAddress + offsetArpeggioCommon, arpBytes.data(),
                                       arpBytes.size(), deviceId));

    // One block per grid row, Note 1 at 00 06 00 through Note 16 at 00 15 00.
    for (int row = 0; row < arpeggioMaxRows; ++row)
    {
        std::array<std::uint8_t, sizeArpeggioPattern> patternBytes {};
        encodeArpeggioPattern (patch.arpeggio.style, row, patternBytes.data());
        const std::uint32_t address = baseAddress + offsetArpeggioPattern
                                      + static_cast<std::uint32_t> (row) * arpeggioPatternStride;
        packets.push_back (makeDt1Message (address, patternBytes.data(),
                                           patternBytes.size(), deviceId));
    }

    return packets;
}

std::vector<std::uint8_t> encodePatchToSyxBuffer (const Patch& patch,
                                                  std::uint32_t baseAddress,
                                                  std::uint8_t deviceId)
{
    const auto packets = encodePatchToSysExPackets (patch, baseAddress, deviceId);
    std::vector<std::uint8_t> buffer;
    for (const auto& p : packets)
        buffer.insert (buffer.end(), p.begin(), p.end());
    return buffer;
}

bool parseDt1Packet (const std::uint8_t* msg, std::size_t msgLen,
                     std::uint8_t expectedDeviceId, Dt1Packet& out) noexcept
{
    if (msg == nullptr || msgLen < 12)
        return false;

    std::size_t offset = 0;
    if (msg[0] == 0xF0)
    {
        offset = 1;
        if (msgLen < 14 || msg[msgLen - 1] != 0xF7)
            return false;
        msgLen -= 1; // exclude trailing F7
    }
    else if (msg[msgLen - 1] == 0xF7)
    {
        msgLen -= 1; // exclude trailing F7 if present
    }

    const std::size_t payloadLen = msgLen - offset;
    if (payloadLen < 12) // 41 dev 00 00 16 12 a0 a1 a2 a3 data... sum
        return false;

    const std::uint8_t* p = msg + offset;
    if (p[0] != rolandId)
        return false;

    const std::uint8_t devId = p[1] & 0x1Fu;
    if (expectedDeviceId != 0x7F && devId != (expectedDeviceId & 0x1Fu))
        return false;

    if (p[2] != sh201ModelId[0] || p[3] != sh201ModelId[1]
        || p[4] != sh201ModelId[2])
        return false;

    if (p[5] != cmdDt1)
        return false; // an RQ1 or anything else is not a write

    out.address = (static_cast<std::uint32_t> (p[6] & 0x7Fu) << 24)
                  | (static_cast<std::uint32_t> (p[7] & 0x7Fu) << 16)
                  | (static_cast<std::uint32_t> (p[8] & 0x7Fu) << 8)
                  | static_cast<std::uint32_t> (p[9] & 0x7Fu);
    out.dataLength = payloadLen - 11; // minus 41 dev 00 00 16 12 a0 a1 a2 a3 sum
    out.data = p + 10;

    return verifyChecksum (p + 6, out.dataLength + 4, p[payloadLen - 1]);
}

bool decodeSysExMessage (const std::uint8_t* msg, std::size_t msgLen,
                         Patch& targetPatch, std::uint8_t expectedDeviceId)
{
    Dt1Packet packet;
    if (! parseDt1Packet (msg, msgLen, expectedDeviceId, packet))
        return false;

    const std::uint8_t* dataPtr = packet.data;
    const std::size_t dataLength = packet.dataLength;

    // Route by the block offset inside the patch: 00 = Common, 01/02 = the
    // two tones, 03 = Delay, 04 = Reverb, 05 = Arpeggio Common, 06..15 =
    // Arpeggio Pattern (Note 1..16). The two high address bytes select the
    // Temporary Patch or one of the 32 User Patches and do not change the
    // layout, so the same switch serves both.
    const unsigned block = packet.block();

    switch (block)
    {
        case 0x00:
            decodePatchCommon (dataPtr, dataLength, targetPatch);
            clampToDocumentedRanges (targetPatch);
            return true;
        case 0x01:
            decodeTonePatch (dataPtr, dataLength, targetPatch.upper);
            clampToDocumentedRanges (targetPatch.upper);
            return true;
        case 0x02:
            decodeTonePatch (dataPtr, dataLength, targetPatch.lower);
            clampToDocumentedRanges (targetPatch.lower);
            return true;
        case 0x03:
            decodeDelayParams (dataPtr, dataLength, targetPatch.delay);
            clampToDocumentedRanges (targetPatch);
            return true;
        case 0x04:
            decodeReverbParams (dataPtr, dataLength, targetPatch.reverb);
            clampToDocumentedRanges (targetPatch);
            return true;
        case 0x05:
            decodeArpeggioCommon (dataPtr, dataLength, targetPatch.arpeggio);
            clampToDocumentedRanges (targetPatch);
            return true;
        default:
            break;
    }

    if (block >= 0x06 && block < 0x06 + static_cast<unsigned> (arpeggioMaxRows))
    {
        decodeArpeggioPattern (dataPtr, dataLength, static_cast<int> (block - 0x06),
                               targetPatch.arpeggio.style);
        clampToDocumentedRanges (targetPatch);
        return true;
    }

    return false;
}

bool parseSyxBankFile (const std::uint8_t* fileBytes, std::size_t byteCount,
                       std::vector<NamedPatch>& outPatches)
{
    if (fileBytes == nullptr || byteCount < 14)
        return false;

    std::size_t pos = 0;
    Patch currentPatch = initPatch();
    bool foundAny = false;
    bool patchPending = false;
    // Which patch the last accepted block belonged to. The two high address
    // bytes pick the Temporary Patch (10 00) or one of the 32 User Patches
    // (20 00 .. 20 1F), and everything below them is the block offset, so a
    // change there is a patch boundary whatever order the blocks arrive in.
    std::uint32_t currentPatchBase = 0;

    while (pos < byteCount)
    {
        while (pos < byteCount && fileBytes[pos] != 0xF0)
            ++pos;
        if (pos >= byteCount)
            break;

        const std::size_t start = pos;
        while (pos < byteCount && fileBytes[pos] != 0xF7)
            ++pos;
        if (pos >= byteCount)
            break;

        const std::size_t end = pos;
        const std::size_t msgLen = end - start + 1;

        // The message has to be a DT1 this codec owns *before* it is allowed
        // to end the patch being accumulated. Deciding the boundary from raw
        // address bytes first meant one foreign or corrupt System Exclusive
        // message in the file — anyone else's manufacturer ID, a universal
        // message, a bad checksum — split a patch in two and handed back the
        // half that had been read so far.
        Dt1Packet packet;
        if (parseDt1Packet (fileBytes + start, msgLen, 0x7F, packet)
            && packet.blockIsKnown())
        {
            if (patchPending && packet.patchBase() != currentPatchBase)
            {
                outPatches.push_back ({ currentPatch.name, currentPatch });
                currentPatch = initPatch();
                patchPending = false;
            }

            if (decodeSysExMessage (fileBytes + start, msgLen, currentPatch))
            {
                foundAny = true;
                patchPending = true;
                currentPatchBase = packet.patchBase();
            }
        }
        pos = end + 1;
    }

    if (patchPending)
        outPatches.push_back ({ currentPatch.name, currentPatch });

    return foundAny;
}

std::vector<std::uint8_t> generateSyxBankFile (const std::vector<NamedPatch>& bank,
                                               std::uint8_t deviceId)
{
    std::vector<std::uint8_t> out;
    // User Patch (001) is at 20 00 00 00 and each further slot is one step of
    // 00 01 00 00, up to User Patch (032) at 20 1F 00 00. A bank longer than
    // the instrument's 32 user slots has nowhere on the hardware to land, so
    // it stops there.
    const std::size_t count =
        std::min (bank.size(), static_cast<std::size_t> (userPatchCount));
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint32_t addr =
            addrUserPatchBase + static_cast<std::uint32_t> (i) * userPatchStride;
        const auto patchBytes = encodePatchToSyxBuffer (bank[i].patch, addr, deviceId);
        out.insert (out.end(), patchBytes.begin(), patchBytes.end());
    }
    return out;
}

} // namespace septum::sysex

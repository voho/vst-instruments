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
} // namespace

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

    // 0E..0F: Patch Tempo 5..300, split into upper and lower 4-bit nibbles
    const int tempo = std::clamp (patch.tempo, 5, 300);
    dest[0x0E] = static_cast<std::uint8_t> ((tempo >> 4) & 0x0F);
    dest[0x0F] = static_cast<std::uint8_t> (tempo & 0x0F);

    dest[0x10] = 0; // reserved
    dest[0x11] = static_cast<std::uint8_t> (patch.keyboardMode);
    dest[0x12] = static_cast<std::uint8_t> (patch.keyboardPart);
    dest[0x13] = clampTo7Bit (patch.splitPoint);

    // 0x14: Bitmask: bit 0 = Delay Switch, bit 1 = Reverb Switch
    dest[0x14] = static_cast<std::uint8_t> ((patch.delayOn ? 1u : 0u)
                                            | (patch.reverbOn ? 2u : 0u));

    dest[0x15] = static_cast<std::uint8_t> (patch.modulationDestination);
    dest[0x16] = static_cast<std::uint8_t> (patch.dBeamDestination);
    dest[0x17] = static_cast<std::uint8_t> (patch.pitchBendDestination);
    dest[0x18] = static_cast<std::uint8_t> (patch.expressionDestination);
    dest[0x19] = patch.activeExpression ? 1 : 0;
    dest[0x1A] = 0;
    dest[0x1B] = 0;
    dest[0x1C] = patch.arpeggio.on ? 1 : 0;
    dest[0x1D] = patch.arpeggio.hold ? 1 : 0;
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
    dest[0x29] = tone.lfo1.tempoSync ? 1 : 0;
    dest[0x2A] = clampTo7Bit (tone.lfo1.tempoSyncNote);
    dest[0x2B] = clampTo7Bit (tone.lfo1.fadeTime);
    dest[0x2C] = tone.lfo1.keyTrigger ? 1 : 0;
    dest[0x2D] = static_cast<std::uint8_t> (tone.lfo1.destination1);
    dest[0x2E] = signedTo7Bit (tone.lfo1.depth1);
    dest[0x2F] = static_cast<std::uint8_t> (tone.lfo1.destination2);
    dest[0x30] = signedTo7Bit (tone.lfo1.depth2);

    dest[0x31] = static_cast<std::uint8_t> (tone.lfo2.shape);
    dest[0x32] = clampTo7Bit (tone.lfo2.rate);
    dest[0x33] = tone.lfo2.tempoSync ? 1 : 0;
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
    dest[0] = clampTo7Bit (delay.time);
    // Feedback: -98..+98% raw 0..98 (49 = 0%)
    dest[1] = clampTo7Bit (static_cast<int> (std::lround (delay.feedback / 2.0)) + 49);
    dest[2] = clampTo7Bit (delay.hfDamp);
    dest[3] = clampTo7Bit (delay.modulationRate);
    dest[4] = clampTo7Bit (delay.modulationDepth);
}

void encodeReverbParams (const ReverbParams& reverb, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    dest[0] = clampTo7Bit (reverb.time);
    dest[1] = clampTo7Bit (reverb.preDelay);
    dest[2] = clampTo7Bit (reverb.size);
    dest[3] = clampTo7Bit (reverb.highCut);
    dest[4] = clampTo7Bit (reverb.density);
    dest[5] = clampTo7Bit (reverb.diffusion);
    dest[6] = clampTo7Bit (reverb.lfDampFrequency);
    dest[7] = signedTo7Bit (reverb.lfDampGain);
    dest[8] = clampTo7Bit (reverb.hfDampFrequency);
    dest[9] = signedTo7Bit (reverb.hfDampGain);
}

void encodeArpeggioParams (const ArpeggioParams& arp, std::uint8_t* dest) noexcept
{
    if (dest == nullptr)
        return;
    std::memset (dest, 0, sizeArpeggio);

    dest[0] = clampTo7Bit (arp.styleIndex);
    dest[1] = static_cast<std::uint8_t> (arp.grid);
    dest[2] = static_cast<std::uint8_t> (arp.duration);
    dest[3] = static_cast<std::uint8_t> (arp.motif);
    dest[4] = signedTo7Bit (arp.octaveRange);
    dest[5] = clampTo7Bit (arp.accent);
    dest[6] = clampTo7Bit (arp.velocity);
    dest[7] = clampTo7Bit (arp.style.endStep);
    dest[8] = static_cast<std::uint8_t> (arp.splitArpeggio);

    // 0x09..0x208: 32 steps x 16 rows pattern cells
    std::size_t offset = 9;
    for (int step = 0; step < arpeggioMaxSteps; ++step)
    {
        for (int row = 0; row < arpeggioMaxRows; ++row)
        {
            const signed char val = arp.style.cell (step, row);
            if (val == arpeggioTie)
                dest[offset++] = 0x7F; // Tie encoded as 127 / 0x7F
            else if (val <= 0)
                dest[offset++] = 0x00; // Rest
            else
                dest[offset++] = clampTo7Bit (val);
        }
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

    if (size > 0x0F)
    {
        const int tempo = ((src[0x0E] & 0x0Fu) << 4) | (src[0x0F] & 0x0Fu);
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
    {
        patch.delayOn = (src[0x14] & 1u) != 0;
        patch.reverbOn = (src[0x14] & 2u) != 0;
    }

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

    if (size > 0x1C) patch.arpeggio.on = (src[0x1C] & 1u) != 0;
    if (size > 0x1D) patch.arpeggio.hold = (src[0x1D] & 1u) != 0;
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
    if (size > 0x29) tone.lfo1.tempoSync = (get (0x29) & 1u) != 0;
    if (size > 0x2A) tone.lfo1.tempoSyncNote = std::clamp<int> (get (0x2A), 0, 19);
    if (size > 0x2B) tone.lfo1.fadeTime = get (0x2B) & 0x7Fu;
    if (size > 0x2C) tone.lfo1.keyTrigger = (get (0x2C) & 1u) != 0;
    if (size > 0x2D) tone.lfo1.destination1 = static_cast<LfoDest1> (std::clamp<int> (get (0x2D), 0, 3));
    if (size > 0x2E) tone.lfo1.depth1 = from7BitSigned (get (0x2E));
    if (size > 0x2F) tone.lfo1.destination2 = static_cast<LfoDest2> (std::clamp<int> (get (0x2F), 0, 2));
    if (size > 0x30) tone.lfo1.depth2 = from7BitSigned (get (0x30));

    if (size > 0x31) tone.lfo2.shape = static_cast<LfoShape> (std::clamp<int> (get (0x31), 0, 6));
    if (size > 0x32) tone.lfo2.rate = get (0x32) & 0x7Fu;
    if (size > 0x33) tone.lfo2.tempoSync = (get (0x33) & 1u) != 0;
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
    if (size > 7) reverb.lfDampGain = std::clamp<int> (from7BitSigned (src[7]), -36, 0);
    if (size > 8) reverb.hfDampFrequency = std::clamp<int> (src[8], 0, 5);
    if (size > 9) reverb.hfDampGain = std::clamp<int> (from7BitSigned (src[9]), -36, 0);
}

void decodeArpeggioParams (const std::uint8_t* src, std::size_t size, ArpeggioParams& arp) noexcept
{
    if (src == nullptr || size == 0)
        return;
    if (size > 0) arp.styleIndex = src[0] & 0x7Fu;
    if (size > 1) arp.grid = static_cast<ArpeggioGrid> (std::clamp<int> (src[1], 0, 8));
    if (size > 2) arp.duration = static_cast<ArpeggioDuration> (std::clamp<int> (src[2], 0, 9));
    if (size > 3) arp.motif = static_cast<ArpeggioMotif> (std::clamp<int> (src[3], 0, 11));
    if (size > 4) arp.octaveRange = std::clamp<int> (from7BitSigned (src[4]), -3, 3);
    if (size > 5) arp.accent = std::clamp<int> (src[5], 0, 100);
    if (size > 6) arp.velocity = src[6] & 0x7Fu;
    if (size > 7) arp.style.endStep = std::clamp<int> (src[7], 1, arpeggioMaxSteps);
    if (size > 8) arp.splitArpeggio = static_cast<SplitArpeggio> (std::clamp<int> (src[8], 0, 2));

    if (size > 9)
    {
        std::size_t offset = 9;
        for (int step = 0; step < arpeggioMaxSteps && offset < size; ++step)
        {
            for (int row = 0; row < arpeggioMaxRows && offset < size; ++row)
            {
                const std::uint8_t raw = src[offset++];
                if (raw == 0x7F)
                    arp.style.cells[static_cast<std::size_t> (step)][static_cast<std::size_t> (row)] = arpeggioTie;
                else if (raw == 0)
                    arp.style.cells[static_cast<std::size_t> (step)][static_cast<std::size_t> (row)] = arpeggioRest;
                else
                    arp.style.cells[static_cast<std::size_t> (step)][static_cast<std::size_t> (row)] = static_cast<signed char> (raw & 0x7Fu);
            }
        }
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
    packets.reserve (6);

    std::array<std::uint8_t, sizePatchCommon> commonBytes {};
    encodePatchCommon (patch, commonBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0000, commonBytes.data(),
                                       commonBytes.size(), deviceId));

    std::array<std::uint8_t, sizeTonePatch> upperBytes {};
    encodeTonePatch (patch.upper, upperBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0100, upperBytes.data(),
                                       upperBytes.size(), deviceId));

    std::array<std::uint8_t, sizeTonePatch> lowerBytes {};
    encodeTonePatch (patch.lower, lowerBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0200, lowerBytes.data(),
                                       lowerBytes.size(), deviceId));

    std::array<std::uint8_t, sizeDelay> delayBytes {};
    encodeDelayParams (patch.delay, delayBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0300, delayBytes.data(),
                                       delayBytes.size(), deviceId));

    std::array<std::uint8_t, sizeReverb> reverbBytes {};
    encodeReverbParams (patch.reverb, reverbBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0400, reverbBytes.data(),
                                       reverbBytes.size(), deviceId));

    std::array<std::uint8_t, sizeArpeggio> arpBytes {};
    encodeArpeggioParams (patch.arpeggio, arpBytes.data());
    packets.push_back (makeDt1Message (baseAddress + 0x0500, arpBytes.data(),
                                       arpBytes.size(), deviceId));

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

bool decodeSysExMessage (const std::uint8_t* msg, std::size_t msgLen,
                         Patch& targetPatch, std::uint8_t expectedDeviceId)
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

    const std::uint8_t cmd = p[5];
    if (cmd != cmdDt1)
        return false; // Not a DT1 write

    const std::uint32_t addr = (static_cast<std::uint32_t> (p[6] & 0x7Fu) << 24)
                               | (static_cast<std::uint32_t> (p[7] & 0x7Fu) << 16)
                               | (static_cast<std::uint32_t> (p[8] & 0x7Fu) << 8)
                               | static_cast<std::uint32_t> (p[9] & 0x7Fu);

    const std::size_t dataOffset = 10;
    const std::size_t dataLength = payloadLen - 11; // minus 41 dev 00 00 16 12 a0 a1 a2 a3 sum
    const std::uint8_t receivedSum = p[payloadLen - 1];

    if (! verifyChecksum (p + 6, dataLength + 4, receivedSum))
        return false;

    const std::uint8_t* dataPtr = p + dataOffset;

    // Route by sub-block (0 = Common, 1 = Upper, 2 = Lower, 3 = Delay, 4 = Reverb, 5 = Arpeggio)
    const unsigned subBlock = (addr >= 0x20000000) ? ((addr >> 8) & 0x07u) : ((addr >> 8) & 0xFFu);

    if (subBlock == 0)
    {
        decodePatchCommon (dataPtr, dataLength, targetPatch);
        clampToDocumentedRanges (targetPatch);
        return true;
    }
    if (subBlock == 1)
    {
        decodeTonePatch (dataPtr, dataLength, targetPatch.upper);
        clampToDocumentedRanges (targetPatch.upper);
        return true;
    }
    if (subBlock == 2)
    {
        decodeTonePatch (dataPtr, dataLength, targetPatch.lower);
        clampToDocumentedRanges (targetPatch.lower);
        return true;
    }
    if (subBlock == 3)
    {
        decodeDelayParams (dataPtr, dataLength, targetPatch.delay);
        clampToDocumentedRanges (targetPatch);
        return true;
    }
    if (subBlock == 4)
    {
        decodeReverbParams (dataPtr, dataLength, targetPatch.reverb);
        clampToDocumentedRanges (targetPatch);
        return true;
    }
    if (subBlock == 5)
    {
        decodeArpeggioParams (dataPtr, dataLength, targetPatch.arpeggio);
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

        if (decodeSysExMessage (fileBytes + start, msgLen, currentPatch))
        {
            foundAny = true;
            const std::uint32_t addr =
                (static_cast<std::uint32_t> (fileBytes[start + 7] & 0x7Fu) << 24)
                | (static_cast<std::uint32_t> (fileBytes[start + 8] & 0x7Fu) << 16)
                | (static_cast<std::uint32_t> (fileBytes[start + 9] & 0x7Fu) << 8)
                | static_cast<std::uint32_t> (fileBytes[start + 10] & 0x7Fu);

            const unsigned subBlock = (addr >= 0x20000000) ? ((addr >> 8) & 0x07u) : ((addr >> 8) & 0xFFu);
            if (subBlock == 5)
            {
                outPatches.push_back ({ "", currentPatch });
                currentPatch = initPatch();
            }
        }
        pos = end + 1;
    }

    if (foundAny && outPatches.empty())
        outPatches.push_back ({ "", currentPatch });

    return foundAny;
}

std::vector<std::uint8_t> generateSyxBankFile (const std::vector<NamedPatch>& bank,
                                               std::uint8_t deviceId)
{
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < bank.size(); ++i)
    {
        const auto bankIdx = static_cast<std::uint32_t> ((i / 8) % 4);
        const auto patchIdx = static_cast<std::uint32_t> (i % 8);
        const std::uint32_t addr = addrUserPatchBase
                                   | (bankIdx << 16)
                                   | ((patchIdx * 8) << 8);

        const auto patchBytes = encodePatchToSyxBuffer (bank[i].patch, addr, deviceId);
        out.insert (out.end(), patchBytes.begin(), patchBytes.end());
    }
    return out;
}

} // namespace septum::sysex

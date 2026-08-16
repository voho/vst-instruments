#include "DSP/Presets.h"
#include "DSP/VocalorMath.h"
#include "DSP/VoiceEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vocalor
{
struct VoiceEngineTestAccess
{
    struct TractSnapshot
    {
        std::array<float, kFormantCount> hz {};
        std::array<float, kFormantCount> bandwidth {};
        std::array<float, kFormantCount> gain {};
        std::array<float, kFormantCount> a1 {};
        std::array<float, kFormantCount> a2 {};
        std::array<float, kFormantCount> b0 {};
        std::array<float, kFormantCount> peakNormaliser {};
    };

    struct SingerTractSnapshot
    {
        int singer = -1;
        float intentionalFundamental = 0.0f;
        TractSnapshot tract {};
    };

    /** The engine's render chunk, in samples. Chunk boundaries are aligned to
        absolute sample positions, so a buffer split on a multiple of it renders
        the same chunks a single block would have rendered. */
    static constexpr int chunkSize = VoiceEngine::chunkSize;

    static std::array<float, 4> smoothedParameters(const VoiceEngine& engine) noexcept
    {
        return { engine.smoothedRoom_, engine.smoothedGain_,
                 engine.smoothedBreath_, engine.smoothedTension_ };
    }

    static std::array<float, kFormantCount> chunkFormants(const VoiceEngine& engine) noexcept
    {
        std::array<float, kFormantCount> result {};
        for (int i = 0; i < kFormantCount; ++i)
            result[static_cast<std::size_t>(i)] = engine.chunkFormantHz_[static_cast<std::size_t>(i)];
        return result;
    }

    /** Smallest non-zero magnitude anywhere in the recursive state. Anything
        below ~1.2e-38 would be a denormal and would stall unprotected hosts. */
    static float smallestNonZeroState(const VoiceEngine& engine) noexcept
    {
        float smallest = std::numeric_limits<float>::max();
        const auto consider = [&smallest](float value) noexcept
        {
            const float magnitude = std::abs(value);
            if (magnitude > 0.0f)
                smallest = std::min(smallest, magnitude);
        };

        for (const auto& voice : engine.voices_)
        {
            for (const auto& resonator : voice.tract)
            {
                consider(resonator.y1);
                consider(resonator.y2);
            }
            consider(voice.sourceTilt);
            consider(voice.sourceSlow);
            consider(voice.sourceSlower);
        }
        for (const auto value : engine.roomLeft_)
            consider(value);
        for (const auto value : engine.roomRight_)
            consider(value);
        consider(engine.roomDampingLeft_);
        consider(engine.roomDampingRight_);
        consider(engine.roomLowCutLeft_);
        consider(engine.roomLowCutRight_);

        return smallest == std::numeric_limits<float>::max() ? 0.0f : smallest;
    }

    static std::array<float, kFormantCount> chunkBandwidths(const VoiceEngine& engine) noexcept
    {
        std::array<float, kFormantCount> result {};
        for (int i = 0; i < kFormantCount; ++i)
            result[static_cast<std::size_t>(i)] = engine.chunkBandwidth_[static_cast<std::size_t>(i)];
        return result;
    }

    static float frequencyForRoot(const VoiceEngine& engine, int rootMidi) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && voice.rootMidi == rootMidi)
                return voice.phaseIncrement * static_cast<float>(engine.sampleRate_);
        return 0.0f;
    }

    static int soundingMidiNote(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.midiNote;
        return -1;
    }

    static float soundingEnvelope(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.envelope;
        return 0.0f;
    }

    static int heldNoteCount(const VoiceEngine& engine) noexcept { return engine.heldCount_; }

    /** Sounding frequency of every held voice, in Hz. An ensemble puts twelve
        of them on one note, and how far apart they sit is what separates a
        section from one thick voice. */
    static std::vector<float> soundingFrequencies(const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                result.push_back(voice.phaseIncrement * static_cast<float>(engine.sampleRate_));
        return result;
    }

    /** Sounding frequency of the first held voice on @c midiNote, in Hz, or 0
        if that note is not sounding. Chord mode puts every member of the triad
        on a different sounding note behind one root, so this is what the
        intonation of an interval has to be measured from. */
    static float frequencyForNote(const VoiceEngine& engine, int midiNote) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing && voice.midiNote == midiNote)
                return voice.phaseIncrement * static_cast<float>(engine.sampleRate_);
        return 0.0f;
    }

    /** The formant targets of the first sounding voice, after the per-singer
        anatomy and the pitch-dependent tuning the chunk targets know nothing
        about. Zeroes if nothing is sounding. */
    static std::array<float, kFormantCount> voiceFormants(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
        {
            if (! voice.active || voice.releasing)
                continue;
            std::array<float, kFormantCount> result {};
            for (int i = 0; i < kFormantCount; ++i)
                result[static_cast<std::size_t>(i)] = voice.formantHz[static_cast<std::size_t>(i)];
            return result;
        }
        return {};
    }

    /** Complete running tract for one sounding pitch. Frequencies, widths and
        gains must belong to the same voice: a shared bandwidth can look
        plausible in isolation while describing a different set of poles. */
    static TractSnapshot voiceTract(const VoiceEngine& engine, int midiNote) noexcept
    {
        for (const auto& voice : engine.voices_)
        {
            if (!voice.active || voice.releasing || voice.midiNote != midiNote)
                continue;
            TractSnapshot result;
            result.hz = voice.formantHz;
            result.bandwidth = voice.formantBandwidth;
            result.gain = voice.formantGain;
            for (int i = 0; i < kFormantCount; ++i)
            {
                const auto index = static_cast<std::size_t>(i);
                result.a1[index] = voice.tract[index].a1;
                result.a2[index] = voice.tract[index].a2;
                result.b0[index] = voice.tract[index].b0;
                result.peakNormaliser[index] = voice.tract[index].peakNormaliser;
            }
            return result;
        }
        return {};
    }

    /** Every singer's running tract on one sounding note, tagged by stable
        identity. The intentional fundamental follows glide, just intonation
        and pitch bend but excludes vibrato, jitter, drift and the onset scoop,
        exactly as register-dependent tract gestures do. */
    static std::vector<SingerTractSnapshot> singerTracts(
        const VoiceEngine& engine, int midiNote)
    {
        std::vector<SingerTractSnapshot> result;
        for (const auto& voice : engine.voices_)
        {
            if (!voice.active || voice.releasing || voice.midiNote != midiNote)
                continue;
            SingerTractSnapshot entry;
            entry.singer = voice.singer;
            const float tractCents = voice.glideCents + voice.justCents
                + 100.0f * engine.pitchBendSemitones_;
            entry.intentionalFundamental = voice.baseFrequency
                * std::exp2(tractCents * (1.0f / 1200.0f));
            entry.tract.hz = voice.formantHz;
            entry.tract.bandwidth = voice.formantBandwidth;
            entry.tract.gain = voice.formantGain;
            for (int formant = 0; formant < kFormantCount; ++formant)
            {
                const auto index = static_cast<std::size_t>(formant);
                entry.tract.a1[index] = voice.tract[index].a1;
                entry.tract.a2[index] = voice.tract[index].a2;
                entry.tract.b0[index] = voice.tract[index].b0;
                entry.tract.peakNormaliser[index] = voice.tract[index].peakNormaliser;
            }
            result.push_back(entry);
        }
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
        {
            return left.singer < right.singer;
        });
        return result;
    }

    /** Running high-register effort compensation for one sounding pitch:
        realised gain, per-sample ramp and the 0..1 F1-lift gesture. */
    static std::array<float, 3> efficiencyForNote(
        const VoiceEngine& engine, int midiNote) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && !voice.releasing && voice.midiNote == midiNote)
                return { voice.renderGain, voice.renderGainStep,
                         voice.formantTuningLift };
        return {};
    }

    /** Running breath-support regulation for one sounding pitch: realised
        gain, per-sample ramp, smoothed target and the freshly measured raw
        target from the same intentional (non-vibrato) f0. */
    static std::array<float, 4> radiatedPowerForNote(
        const VoiceEngine& engine, int midiNote) noexcept
    {
        for (const auto& voice : engine.voices_)
        {
            if (!voice.active || voice.releasing || voice.midiNote != midiNote)
                continue;
            const float tractCents = voice.glideCents + voice.justCents
                + 100.0f * engine.pitchBendSemitones_;
            const float fundamental = voice.baseFrequency
                * std::exp2(tractCents / 1200.0f);
            const float raw = engine.radiatedPowerTarget(
                voice, fundamental,
                engine.smoothedTension_
                    * engine.chunkResponse_.sourceTensionScale,
                engine.blockParameters_.legacyRadiatedPowerBypass);
            return { voice.radiatedPowerGain, voice.radiatedPowerGainStep,
                     voice.radiatedPowerTarget, raw };
        }
        return {};
    }

    static void setRadiatedPowerDepth(VoiceEngine& engine, float depth) noexcept
    {
        engine.radiatedPowerDepth_ = depth;
    }

    /** The two per-sample level scalars the dynamic reaches, as the render loop
        sees them: the voiced drive and the aspiration drive. Their ratio is the
        breath-to-voice balance, which has to rise as the dynamic falls. */
    static std::array<float, 3> dynamicDrives(const VoiceEngine& engine) noexcept
    {
        return { engine.voicedScaleAt_[0], engine.airLevelAt_[0], engine.smoothedDynamics_ };
    }

    /** Forces the source-tension ramp depth. A note-on starts the glottal
        source this far below the block's tension and firms up onto it; passing
        0 renders the same note with the ramp switched out, which is how the
        tract half of the onset and the source half are told apart inside one
        binary. Applied on every control update, so a note already sounding
        follows it. */
    static void setSourceTensionRampDepth(VoiceEngine& engine, float depth) noexcept
    {
        engine.sourceTensionRampDepth_ = depth;
    }

    /** How far below the block's tension the sounding voice's glottal source
        actually started, after the note's own velocity has scaled the engine's
        depth. Read from the voice rather than inferring it from one output
        band: the source shape, aspiration and tract all move that band. The
        source-bank test below measures the physical LF morph directly, before
        those later stages can disguise it. */
    static float sourceTensionSag(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.tensionSag;
        return 0.0f;
    }

    /** The engine's own ramp depth, which is the value the reference velocity
        0.80 resolves to. */
    static float sourceTensionRampDepth(const VoiceEngine& engine) noexcept
    {
        return engine.sourceTensionRampDepth_;
    }

    /** One exact cycle of the voiced glottal source before its presence
        shelves, envelope and tract. Measuring here separates the LF source
        morph from every formant: a tract peak must not be able to hide a
        harmonic cancelled inside the excitation itself. */
    static std::vector<float> glottalSourceCycle(const VoiceEngine& engine,
                                                 int level, float tension)
    {
        std::vector<float> result(static_cast<std::size_t>(VoiceEngine::tableSize));
        for (int sample = 0; sample < VoiceEngine::tableSize; ++sample)
        {
            const float phase = static_cast<float>(sample)
                              / static_cast<float>(VoiceEngine::tableSize);
            result[static_cast<std::size_t>(sample)] =
                engine.glottalPair(level, phase, tension);
        }
        return result;
    }

    static constexpr int fullestGlottalTableLevel = VoiceEngine::tableLevels - 1;
    static constexpr int glottalTableLevelCount = VoiceEngine::tableLevels;

    static constexpr int glottalHarmonicsForLevel(int level) noexcept
    {
        return VoiceEngine::harmonicsPerLevel[static_cast<std::size_t>(level)];
    }

    static const void* tableBankIdentity(const VoiceEngine& engine) noexcept
    {
        return engine.tables_;
    }

    /** Forces the depth of the pitch-synchronous aspiration window. 1 is the
        glottal flow itself; passing 0 renders the same note with stationary
        noise, which is how a redistribution of the noise inside the period is
        told apart from a change in how much noise there is. Re-read on every
        control update, so a note already sounding follows it. */
    static void setAspirationModulationDepth (VoiceEngine& engine, float depth) noexcept
    {
        engine.aspirationModulationDepth_ = depth;
    }

    /** How much of the first-order image field the placement renders. 1 is the
        geometry; 0 leaves the direct paths alone and takes the room away, which
        is how a property of the source is measured once the source is standing
        in a room. */
    static void setPlacementReflectionDepth (VoiceEngine& engine, float depth) noexcept
    {
        engine.placementReflectionDepth_ = depth;
        engine.placementSpread_ = -1.0f;   // force the taps to be resolved again
    }

    /** Where the twelve singers stand, in metres. Drawn once in
        buildSingerIdentities() and never again. */
    static std::vector<float> singerDistances (const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto& singer : engine.singers_)
            result.push_back (singer.distanceMetres);
        return result;
    }

    /** The direct-path delay actually applied to each identity, in samples:
        the mean of the two receivers' arrival times. Referred to the nearest
        singer, so identity 0 -- the one Solo sings on -- costs no latency. */
    static std::vector<float> directDelaySamples (const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto value : engine.placementDirectSamples_)
            result.push_back (value);
        return result;
    }

    /** The direct gain of each identity at the left receiver, which is the 1/r
        the geometry gives it. */
    static std::vector<float> directGains (const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto& gains : engine.placementGain_)
            result.push_back (gains[0]);
        return result;
    }

    /** Drives one unit sample into a named singer's placement input, every
        voice silent, and returns what reaches the listener: her direct arrival,
        her four first-order images and the tail they excite, summed to mono.
        The image sources are per singer and sit upstream of the recirculating
        network, so an impulse pushed into updateRoom would observe only the
        shared four-tap tail -- the path this fixture exists to measure is the
        one that injection point skips. */
    static std::vector<float> placementImpulse (VoiceEngine& engine, int singerIndex,
                                                int samples)
    {
        engine.singersInUse_ = 1u << singerIndex;
        const float dryScale = 1.0f - 0.12f * engine.smoothedRoom_;
        const float wetScale = 0.72f * engine.smoothedRoom_;
        std::vector<float> result;
        result.reserve (static_cast<std::size_t> (samples));
        bool first = true;
        while (static_cast<int> (result.size()) < samples)
        {
            const int count = std::min (VoiceEngine::chunkSize,
                                        samples - static_cast<int> (result.size()));
            for (auto& bus : engine.singerMix_)
                bus.fill (0.0f);
            if (first)
                engine.singerMix_[static_cast<std::size_t> (singerIndex)][0] = 1.0f;
            for (int i = 0; i < count; ++i)
            {
                const auto index = static_cast<std::size_t> (i);
                engine.mixLeft_[index] = engine.mixRight_[index] = 0.0f;
                engine.earlyLeft_[index] = engine.earlyRight_[index] = 0.0f;
            }
            engine.renderPlacement (count);
            for (int i = 0; i < count; ++i)
            {
                const auto index = static_cast<std::size_t> (i);
                float wetLeft = 0.0f;
                float wetRight = 0.0f;
                engine.updateRoom (engine.placementSendGain_ * engine.earlyLeft_[index],
                                   engine.placementSendGain_ * engine.earlyRight_[index],
                                   wetLeft, wetRight);
                const float left = dryScale * engine.mixLeft_[index]
                                 + wetScale * (engine.earlyLeft_[index] + wetLeft);
                const float right = dryScale * engine.mixRight_[index]
                                  + wetScale * (engine.earlyRight_[index] + wetRight);
                result.push_back (0.5f * (left + right));
            }
            first = false;
        }
        return result;
    }

    /** The recirculating network's own impulse response, mono: a unit sample
        straight into updateRoom with nothing else running.

        This is the fixture a reverberation time has to be read on. The
        placement impulse above carries the direct arrival, and the direct
        arrival is 7.5 dB above everything the room does with it, so a Schroeder
        integral of that response measures the direct-to-reverberant ratio
        rather than a decay: it reads 0.051 s at Size 50 % / Room 50 % against
        the 0.231 s the network actually rings for. */
    static std::vector<float> roomImpulse (VoiceEngine& engine, int samples)
    {
        std::vector<float> result (static_cast<std::size_t> (samples), 0.0f);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        engine.updateRoom (1.0f, 1.0f, wetLeft, wetRight);
        result[0] = 0.5f * (wetLeft + wetRight);
        for (std::size_t i = 1; i < result.size(); ++i)
        {
            engine.updateRoom (0.0f, 0.0f, wetLeft, wetRight);
            result[i] = 0.5f * (wetLeft + wetRight);
        }
        return result;
    }

    /** How hard the first-order image field drives the recirculating network.
        Muting it separates the tail from the images inside one render, which is
        what the wet/dry contract is written on: the tail has to stay where it
        was, and the images are the sound this step adds. */
    static void setPlacementSendGain (VoiceEngine& engine, float gain) noexcept
    {
        engine.placementSendGain_ = gain;
    }

    /** Reseeds the sounding voice's noise stream mid-note. Rendering the same
        note twice with only this differing isolates the aspiration by
        difference: at Humanize 0 the stream drives no jitter and no shimmer, so
        everything else about the two renders is bit-identical. Reseeding after
        the note has started rather than before matters, because the initial
        phase and the pitch scoop are both drawn from this state at note-on. */
    static void reseedVoiceNoise (VoiceEngine& engine, std::uint32_t state) noexcept
    {
        for (auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
            {
                voice.noiseState = state;
                return;
            }
    }

    /** The sounding voice's glottal phase and its per-sample increment. Folding
        a residual onto the glottal period has to bin by the phase the engine is
        actually at, not by a free-running nominal period, or the fold stops
        being exact the moment anything perturbs the pitch. */
    static std::array<float, 2> voicePhaseState (const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { voice.phase, voice.phaseIncrement };
        return { 0.0f, 0.0f };
    }

    /** The vibrato rate of every singer identity, in Hz. A reseeded rate that
        nothing reads would otherwise let a pitch-track measurement pass on the
        old rate, so the two are cross-checked against each other. */
    static std::vector<float> singerVibratoRates(const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto& singer : engine.singers_)
            result.push_back(singer.vibratoRate);
        return result;
    }

    /** The longest direct-or-image path the placement can delay a singer by,
        in samples. Once a singer stops, her line is fed silence, so nothing of
        hers can reach the listener more than this far after her last sample. */
    static int placementMaximumSamples (const VoiceEngine& engine) noexcept
    {
        return engine.placementMaximumSamples_;
    }

    /** How long after the note-off each held voice waits before it starts to
        release, in samples, ordered by singer identity. A section does not let
        go on one sample any more than it enters on one, and a stagger built
        only from per-singer decay rates would leave every envelope turning at
        the same instant. */
    static std::vector<int> releaseDelays (const VoiceEngine& engine)
    {
        std::vector<std::pair<int, int>> byIdentity;
        for (const auto& voice : engine.voices_)
            if (voice.active && voice.releasing)
                byIdentity.emplace_back (voice.singer, voice.releaseDelaySamples);
        std::sort (byIdentity.begin(), byIdentity.end());
        std::vector<int> result;
        for (const auto& entry : byIdentity)
            result.push_back (entry.second);
        return result;
    }

    /** The entry delay of every held voice, in samples, ordered by singer
        identity rather than by voice slot so repeats of the same note can be
        compared entry by entry. */
    static std::vector<int> entryDelays (const VoiceEngine& engine)
    {
        std::vector<std::pair<int, int>> byIdentity;
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                byIdentity.emplace_back (voice.singer, voice.delaySamples);
        std::sort (byIdentity.begin(), byIdentity.end());
        std::vector<int> result;
        for (const auto& entry : byIdentity)
            result.push_back (entry.second);
        return result;
    }

    /** When each held voice is actually heard, in samples: its entry delay plus
        the time its direct path takes to reach the listener. The second term is
        step 8's, and is zero until the singers have positions; the timing
        contract is written on the sum because that is what an ear measures. */
    static std::vector<float> audibleOnsets (const VoiceEngine& engine)
    {
        std::vector<std::pair<int, float>> byIdentity;
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                byIdentity.emplace_back (voice.singer,
                                         static_cast<float> (voice.delaySamples)
                                             + engine.placementDirectSamples_
                                                   [static_cast<std::size_t> (voice.singer)]);
        std::sort (byIdentity.begin(), byIdentity.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<float> result;
        for (const auto& entry : byIdentity)
            result.push_back (entry.second);
        return result;
    }

    /** The vibrato phase of every held voice, ordered by singer identity. */
    static std::vector<float> vibratoPhases (const VoiceEngine& engine)
    {
        std::vector<std::pair<int, float>> byIdentity;
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                byIdentity.emplace_back (voice.singer, voice.vibratoPhase);
        std::sort (byIdentity.begin(), byIdentity.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<float> result;
        for (const auto& entry : byIdentity)
            result.push_back (entry.second);
        return result;
    }

    /** Observable modulation state of the first held voice: F0 phase, direct
        source gain and the gain of each of the two cascaded presence shelves.
        The last two are the values the sample loop actually multiplies, after
        their control-period ramps, so their product is the effective high-band
        AM law rather than an unevaluated target. */
    static std::array<float, 3> vibratoModulationState (const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { voice.vibratoPhase, voice.vibratoGain,
                         voice.vibratoShelfGain };
        return { 0.0f, 1.0f, 1.0f };
    }

    /** Give a held voice an older clock without advancing its phase or random
        streams. Used only to build a matched, fully-faded reference whose
        cycle waveform is sample-for-sample identical to a new note. */
    static void offsetHeldVoiceAge (VoiceEngine& engine,
                                    std::uint64_t samples) noexcept
    {
        for (auto& voice : engine.voices_)
        {
            if (! voice.active || voice.releasing)
                continue;
            voice.ageSamples += samples;
            voice.lastControlAge += samples;
        }
    }

    /** Mean musical vibrato extent resolved for the current render chunk. This
        is distinct from sounding pitch: at Vibrato zero the latter must still
        carry Drift, while this structural contribution must be exactly zero. */
    static float resolvedVibratoCents (const VoiceEngine& engine) noexcept
    {
        return engine.chunkVibratoCents_;
    }

    /** Independent stochastic pitch displacement that Drift contributes to
        the first held voice, before it is summed with intentional vibrato. */
    static float independentPitchDriftCents (const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.pitchDriftCents;
        return 0.0f;
    }

    static float smoothedDrift (const VoiceEngine& engine) noexcept
    {
        return engine.smoothedInstability_;
    }

    /** Per-voice vowel position after the slow Drift process but before
        register-dependent formant tuning. The formants are the effective
        vowel's own target, so a high-note jaw strategy cannot masquerade as a
        change of phoneme in the identity tests. */
    static std::array<float, 3> effectiveVowel (const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { voice.effectiveVowelMorph, voice.effectiveVowelX,
                         voice.effectiveVowelY };
        return { 0.0f, 0.5f, 0.5f };
    }

    static std::array<float, kFormantCount> driftedVowelFormants (
        const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.vowelDriftFormantHz;
        return {};
    }

    /** Complete fixed-clock Drift state for the first held voice. A legato
        articulation may re-resolve pitch and tract targets, but it must not
        consume an OU innovation or move the 25 ms vowel scheduler. */
    static std::array<std::uint32_t, 2> driftRandomStates (
        const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { voice.pitchDriftState, voice.vowelDriftState };
        return {};
    }

    static std::array<float, 5> driftOuStates (
        const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { voice.pitchDriftFast, voice.pitchDriftSlow,
                         voice.vowelDriftMorph, voice.vowelDriftX,
                         voice.vowelDriftY };
        return {};
    }

    static int vowelDriftCountdown (const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.vowelDriftCountdown;
        return -1;
    }

    /** The voiced envelope of every voice, indexed by singer identity, with a
        negative entry where that identity is not sounding. A twelve-voice mix
        cannot be resolved back into twelve envelopes, so this is the seam the
        release stagger is measured at. */
    static std::array<float, 12> envelopesBySinger (const VoiceEngine& engine) noexcept
    {
        std::array<float, 12> result {};
        result.fill (-1.0f);
        for (const auto& voice : engine.voices_)
            if (voice.active)
                result[static_cast<std::size_t> (voice.singer)] = voice.envelope;
        return result;
    }

    /** The humanisation noise of the sounding voice in the units the render
        loop applies it in: the amplitude modulation depth the voiced drive is
        multiplied by, and the cents of pitch deviation the two nested jitter
        smoothers contribute. Both are read after the engine's own scaling, so
        a smoother whose drive is not renormalised for the sample rate shows up
        directly as a change in these numbers. */
    static std::array<float, 2> humanisationDepth(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { engine.shimmerDepth_ * voice.shimmer,
                         engine.blockParameters_.humanize
                             * (1.15f * voice.jitter + 1.35f * voice.jitterSlow) };
        return { 0.0f, 0.0f };
    }
};
} // namespace vocalor

namespace
{
constexpr int blockSize = 256;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

double seriesMean (const std::vector<double>& values) noexcept
{
    if (values.empty())
        return 0.0;
    double sum = 0.0;
    for (const auto value : values)
        sum += value;
    return sum / static_cast<double> (values.size());
}

double seriesDeviation (const std::vector<double>& values) noexcept
{
    if (values.empty())
        return 0.0;
    const auto mean = seriesMean (values);
    double square = 0.0;
    for (const auto value : values)
        square += (value - mean) * (value - mean);
    return std::sqrt (square / static_cast<double> (values.size()));
}

double seriesMaximumMagnitude (const std::vector<double>& values) noexcept
{
    double largest = 0.0;
    for (const auto value : values)
        largest = std::max (largest, std::abs (value));
    return largest;
}

std::vector<double> firstDifference (const std::vector<double>& values)
{
    std::vector<double> result;
    if (values.size() < 2)
        return result;
    result.reserve (values.size() - 1);
    for (std::size_t i = 1; i < values.size(); ++i)
        result.push_back (values[i] - values[i - 1]);
    return result;
}

double normalisedCorrelation (const std::vector<double>& left,
                              const std::vector<double>& right) noexcept
{
    const auto count = std::min (left.size(), right.size());
    if (count < 2)
        return 0.0;
    double leftMean = 0.0;
    double rightMean = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        leftMean += left[i];
        rightMean += right[i];
    }
    leftMean /= static_cast<double> (count);
    rightMean /= static_cast<double> (count);

    double product = 0.0;
    double leftSquare = 0.0;
    double rightSquare = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto l = left[i] - leftMean;
        const auto r = right[i] - rightMean;
        product += l * r;
        leftSquare += l * l;
        rightSquare += r * r;
    }
    return product / std::sqrt (std::max (leftSquare * rightSquare, 1.0e-30));
}

double maximumAutocorrelation (const std::vector<double>& values,
                               double sampleRate, double minimumLagSeconds,
                               double maximumLagSeconds) noexcept
{
    if (values.size() < 3 || ! (sampleRate > 0.0))
        return 0.0;
    const auto mean = seriesMean (values);
    const int firstLag = std::max (1, static_cast<int> (std::ceil (
        minimumLagSeconds * sampleRate)));
    const int finalLag = std::min (
        static_cast<int> (values.size()) - 2,
        static_cast<int> (std::floor (maximumLagSeconds * sampleRate)));
    double largest = -1.0;
    for (int lag = firstLag; lag <= finalLag; ++lag)
    {
        double product = 0.0;
        double beforeSquare = 0.0;
        double afterSquare = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t> (lag) < values.size(); ++i)
        {
            const auto before = values[i] - mean;
            const auto after = values[i + static_cast<std::size_t> (lag)] - mean;
            product += before * after;
            beforeSquare += before * before;
            afterSquare += after * after;
        }
        largest = std::max (largest, product
            / std::sqrt (std::max (beforeSquare * afterSquare, 1.0e-30)));
    }
    return largest;
}

/** Largest fraction of a trajectory's variance explained by one sinusoid.

    The sine and cosine columns are centred and solved as a two-column least-
    squares fit, so this remains valid at the low frequencies where a window
    contains only a few cycles. A fixed LFO approaches one; correlated random
    motion distributes its power over many fits. */
double maximumSineFitFraction (const std::vector<double>& values,
                               double sampleRate, double minimumHz,
                               double maximumHz) noexcept
{
    if (values.size() < 8 || ! (sampleRate > 0.0) || ! (maximumHz > minimumHz))
        return 0.0;
    const auto mean = seriesMean (values);
    double totalSquare = 0.0;
    for (const auto value : values)
        totalSquare += (value - mean) * (value - mean);
    if (totalSquare < 1.0e-24)
        return 0.0;

    const double duration = static_cast<double> (values.size()) / sampleRate;
    const double frequencyStep = 1.0 / std::max (4.0 * duration, 1.0);
    double largest = 0.0;
    for (double frequency = minimumHz; frequency <= maximumHz;
         frequency += frequencyStep)
    {
        double sineMean = 0.0;
        double cosineMean = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            const auto phase = 2.0 * 3.14159265358979323846 * frequency
                             * static_cast<double> (i) / sampleRate;
            sineMean += std::sin (phase);
            cosineMean += std::cos (phase);
        }
        sineMean /= static_cast<double> (values.size());
        cosineMean /= static_cast<double> (values.size());

        double sineSquare = 0.0;
        double cosineSquare = 0.0;
        double cross = 0.0;
        double valueSine = 0.0;
        double valueCosine = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            const auto phase = 2.0 * 3.14159265358979323846 * frequency
                             * static_cast<double> (i) / sampleRate;
            const auto sine = std::sin (phase) - sineMean;
            const auto cosine = std::cos (phase) - cosineMean;
            const auto value = values[i] - mean;
            sineSquare += sine * sine;
            cosineSquare += cosine * cosine;
            cross += sine * cosine;
            valueSine += value * sine;
            valueCosine += value * cosine;
        }
        const auto determinant = sineSquare * cosineSquare - cross * cross;
        if (determinant < 1.0e-18)
            continue;
        const auto sineCoefficient = (valueSine * cosineSquare - valueCosine * cross)
                                   / determinant;
        const auto cosineCoefficient = (valueCosine * sineSquare - valueSine * cross)
                                     / determinant;
        const auto explained = sineCoefficient * valueSine
                             + cosineCoefficient * valueCosine;
        largest = std::max (largest, explained / totalSquare);
    }
    return std::clamp (largest, 0.0, 1.0);
}

struct RenderMetrics
{
    double sumOfSquares = 0.0;
    double peak = 0.0;
    std::size_t sampleCount = 0;
    std::size_t nonZeroCount = 0;
    bool finite = true;

    [[nodiscard]] double rms() const noexcept
    {
        return sampleCount > 0
            ? std::sqrt (sumOfSquares / static_cast<double> (sampleCount))
            : 0.0;
    }
};

RenderMetrics render (vocalor::VoiceEngine& engine, int samples)
{
    std::vector<float> left (blockSize, 0.0f);
    std::vector<float> right (blockSize, 0.0f);
    RenderMetrics result;

    for (int rendered = 0; rendered < samples;)
    {
        const auto count = std::min (blockSize, samples - rendered);
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        engine.process (left.data(), right.data(), count);

        for (int i = 0; i < count; ++i)
        {
            for (const auto value : { left[static_cast<std::size_t> (i)],
                                      right[static_cast<std::size_t> (i)] })
            {
                result.finite = result.finite && std::isfinite (value);
                if (std::isfinite (value))
                {
                    const auto magnitude = std::abs (static_cast<double> (value));
                    result.peak = std::max (result.peak, magnitude);
                    result.sumOfSquares += static_cast<double> (value)
                                         * static_cast<double> (value);
                    if (magnitude > 1.0e-8)
                        ++result.nonZeroCount;
                }
                ++result.sampleCount;
            }
        }

        rendered += count;
    }

    return result;
}

std::vector<float> renderMono (vocalor::VoiceEngine& engine, int samples)
{
    std::vector<float> result (static_cast<std::size_t> (samples), 0.0f);
    std::vector<float> left (blockSize, 0.0f);
    std::vector<float> right (blockSize, 0.0f);

    for (int rendered = 0; rendered < samples;)
    {
        const auto count = std::min (blockSize, samples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
            result[static_cast<std::size_t> (rendered + i)] =
                0.5f * (left[static_cast<std::size_t> (i)] + right[static_cast<std::size_t> (i)]);
        rendered += count;
    }

    return result;
}

std::vector<float> renderInterleaved(vocalor::VoiceEngine& engine, int samples,
                                     int renderBlockSize)
{
    std::vector<float> result(static_cast<std::size_t>(2 * samples), 0.0f);
    std::vector<float> left(static_cast<std::size_t>(renderBlockSize), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(renderBlockSize), 0.0f);
    for (int rendered = 0; rendered < samples;)
    {
        const int count = std::min(renderBlockSize, samples - rendered);
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto destination = static_cast<std::size_t>(2 * (rendered + sample));
            result[destination] = left[static_cast<std::size_t>(sample)];
            result[destination + 1] = right[static_cast<std::size_t>(sample)];
        }
        rendered += count;
    }
    return result;
}

vocalor::EngineParameters makeParameters (int mode, int chordQuality,
                                         int profile, int vowel)
{
    vocalor::EngineParameters parameters;
    parameters.profile = static_cast<vocalor::VoiceProfile> (profile);
    parameters.mode = static_cast<vocalor::PerformanceMode> (mode);
    parameters.vowel = static_cast<vocalor::Vowel> (vowel);
    parameters.chordQuality = static_cast<vocalor::ChordQuality> (chordQuality);
    parameters.choirSize = 8;
    parameters.breath = 0.34f;
    parameters.resonance = 0.66f;
    parameters.vibrato = 0.42f;
    parameters.humanize = 0.62f;
    parameters.spread = 0.70f;
    parameters.tension = 0.38f;
    parameters.room = 0.22f;
    parameters.outputGain = 0.50f;
    return parameters;
}

struct Scenario
{
    std::string_view name;
    int mode;
    int chordQuality;
};

void testRenderMatrix()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0 };
    constexpr std::array scenarios {
        Scenario { "solo", 0, 0 },
        Scenario { "choir", 1, 0 },
        Scenario { "major chord", 2, 0 },
        Scenario { "minor chord", 2, 1 }
    };

    for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
    {
        const auto sampleRate = sampleRates[rateIndex];

        for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size(); ++scenarioIndex)
        {
            const auto& scenario = scenarios[scenarioIndex];
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = makeParameters (
                scenario.mode, scenario.chordQuality,
                static_cast<int> (scenarioIndex % 2),
                static_cast<int> (scenarioIndex % 3));
            // Exercise every 1.1 addition on at least one leg of the matrix.
            parameters.vowelMorph = 0.25f * static_cast<float> (scenarioIndex);
            parameters.vowelX = 0.2f + 0.2f * static_cast<float> (scenarioIndex);
            parameters.vowelY = 0.8f - 0.2f * static_cast<float> (scenarioIndex);
            parameters.formantShift = -6.0f + 4.0f * static_cast<float> (scenarioIndex);
            parameters.glide = 0.35f;
            parameters.legato = scenarioIndex % 2 == 1;
            parameters.roomSize = 0.2f + 0.25f * static_cast<float> (scenarioIndex);
            engine.setParameters (parameters);

            engine.noteOn (60, 0.82f);
            const auto held = render (engine, static_cast<int> (sampleRate * 0.40));

            const auto label = std::string (scenario.name) + " at "
                             + std::to_string (static_cast<int> (sampleRate)) + " Hz";
            expect (held.finite, label + " produced a NaN or infinity");
            expect (held.nonZeroCount > held.sampleCount / 100,
                    label + " was silent or nearly entirely silent");
            expect (held.rms() > 1.0e-6, label + " RMS was effectively silent");
            expect (held.peak < 16.0, label + " exceeded the runaway-amplitude guardrail");
            expect (engine.getActiveVoiceCount() > 0,
                    label + " did not report an active voice after note-on");

            engine.noteOff (60);
            const auto released = render (engine, static_cast<int> (sampleRate * 0.55));
            expect (released.finite, label + " produced invalid audio after note-off");
            expect (released.peak < 16.0,
                    label + " exceeded the amplitude guardrail after note-off");
            engine.allNotesOff();
        }
    }
}

void testReleaseCompletes()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (makeParameters (0, 0, 0, 0));
    engine.noteOn (64, 0.8f);

    const auto held = render (engine, static_cast<int> (sampleRate * 0.35));
    engine.noteOff (64);
    const auto earlyRelease = render (engine, static_cast<int> (sampleRate * 0.15));
    const auto releaseBody = render (engine, static_cast<int> (sampleRate * 3.5));
    const auto lateRelease = render (engine, static_cast<int> (sampleRate * 0.15));

    expect (held.finite && earlyRelease.finite && releaseBody.finite && lateRelease.finite,
            "note release produced a NaN or infinity");
    expect (held.rms() > 1.0e-6, "release test note did not produce held audio");
    expect (lateRelease.rms() < std::max (held.rms() * 0.25, 1.0e-7),
            "note-off did not decay to a quiet tail within the advertised tail time");
    expect (engine.getActiveVoiceCount() == 0,
            "voice remained active 3.8 seconds after note-off");
}

void testAllSoundOffIsImmediate()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);

    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.room = 1.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.9f);

    const auto held = render (engine, static_cast<int> (sampleRate * 0.6));
    engine.allNotesOff();
    const auto releaseStart = render (engine, blockSize);

    expect (held.rms() > 1.0e-6, "all-sound-off test note was silent");
    expect (releaseStart.rms() > 1.0e-8,
            "all-notes-off did not preserve the normal release path");
    expect (engine.getActiveVoiceCount() > 0,
            "all-notes-off unexpectedly hard-stopped the active voices");

    engine.allSoundOff();
    const auto stopped = render (engine, static_cast<int> (sampleRate * 0.1));

    expect (engine.getActiveVoiceCount() == 0,
            "all-sound-off left voices active");
    expect (stopped.finite && stopped.peak == 0.0,
            "all-sound-off left audible voice or room-tail samples");
}

void testIdleStateAdvancementAndAutomation()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int idleSamples = 12345;
    constexpr int noteSamples = 4096;
    vocalor::VoiceEngine singleSampleBlocks;
    vocalor::VoiceEngine irregularBlocks;
    for (auto* engine : { &singleSampleBlocks, &irregularBlocks })
    {
        engine->prepare(sampleRate, blockSize);
        auto parameters = makeParameters(1, 0, 0, 0);
        parameters.room = 0.0f;
        // The 1.1 features are all chunk-aligned, so they must not disturb the
        // block-partition invariance either.
        parameters.vowelMorph = 0.65f;
        parameters.vowelX = 0.82f;
        parameters.vowelY = 0.24f;
        parameters.formantShift = 3.5f;
        parameters.glide = 0.4f;
        parameters.roomSize = 0.7f;
        engine->setParameters(parameters);
        engine->reset();
    }

    const auto firstSilence = renderInterleaved(singleSampleBlocks, idleSamples, 1);
    const auto secondSilence = renderInterleaved(irregularBlocks, idleSamples, 383);
    expect(firstSilence == secondSilence
               && std::all_of(firstSilence.begin(), firstSilence.end(),
                              [](float sample) { return sample == 0.0f; }),
           "idle output or ensemble state changed with block partitioning");

    singleSampleBlocks.noteOn(60, 0.82f);
    irregularBlocks.noteOn(60, 0.82f);
    expect(renderInterleaved(singleSampleBlocks, noteSamples, 1)
               == renderInterleaved(irregularBlocks, noteSamples, 383),
           "the first note after idle changed with block partitioning");

    vocalor::VoiceEngine automated;
    automated.prepare(sampleRate, blockSize);
    auto initial = makeParameters(0, 0, 0, 0);
    initial.room = 0.0f;
    initial.outputGain = 0.2f;
    initial.breath = 0.1f;
    initial.tension = 0.2f;
    automated.setParameters(initial);
    automated.reset();

    auto target = initial;
    target.room = 0.8f;
    target.outputGain = 1.3f;
    target.breath = 0.9f;
    target.tension = 0.75f;
    automated.setParameters(target);
    const auto idle = render(automated, static_cast<int>(0.5 * sampleRate));
    const auto smoothed = vocalor::VoiceEngineTestAccess::smoothedParameters(automated);
    expect(idle.peak == 0.0,
           "idle parameter automation produced non-zero output");
    expect(smoothed == std::array<float, 4> {
               target.room, target.outputGain, target.breath, target.tension },
           "idle parameter automation did not advance the smoothers to their targets");
}

void testVowelSpaceModel()
{
    // Every cardinal position must resolve close to its own formant set.
    for (int index = 0; index < vocalor::kCardinalVowelCount; ++index)
    {
        const auto position = vocalor::cardinalVowelPosition (index);
        std::array<float, vocalor::kFormantCount> resolved {};
        vocalor::formantsForVowelPoint (false, position.x, position.y, resolved.data());
        expect (resolved[0] > 200.0f && resolved[0] < 1100.0f,
                "cardinal vowel " + std::string (vocalor::cardinalVowelName (index))
                    + " resolved an implausible F1");
    }

    std::array<float, vocalor::kFormantCount> closeFront {};
    std::array<float, vocalor::kFormantCount> open {};
    std::array<float, vocalor::kFormantCount> closeBack {};
    const auto front = vocalor::cardinalVowelPosition (0);
    const auto centre = vocalor::cardinalVowelPosition (2);
    const auto back = vocalor::cardinalVowelPosition (4);
    vocalor::formantsForVowelPoint (false, front.x, front.y, closeFront.data());
    vocalor::formantsForVowelPoint (false, centre.x, centre.y, open.data());
    vocalor::formantsForVowelPoint (false, back.x, back.y, closeBack.data());

    expect (open[0] > closeFront[0] + 200.0f,
            "the open corner of the vowel space did not raise F1");
    expect (closeFront[1] > open[1] + 700.0f,
            "the front corner of the vowel space did not raise F2");
    expect (closeBack[1] < open[1],
            "the back corner of the vowel space did not lower F2");

    // Moving across the pad has to be continuous, not stepped.
    float previous = 0.0f;
    float largestStep = 0.0f;
    for (int step = 0; step <= 100; ++step)
    {
        std::array<float, vocalor::kFormantCount> point {};
        vocalor::formantsForVowelPoint (false, static_cast<float> (step) / 100.0f, 0.5f,
                                        point.data());
        if (step > 0)
            largestStep = std::max (largestStep, std::abs (point[1] - previous));
        previous = point[1];
    }
    expect (largestStep < 120.0f,
            "the vowel space made a discontinuous jump in F2 across the pad");

    // The preset anchors must keep the exact frequencies the engine shipped with.
    std::array<float, vocalor::kFormantCount> preset {};
    vocalor::formantsForPresetVowel (false, 0, preset.data());
    expect (std::abs (preset[0] - 850.0f) < 0.01f && std::abs (preset[1] - 1220.0f) < 0.01f,
            "the female AAH preset formants changed");
    vocalor::formantsForPresetVowel (true, 1, preset.data());
    expect (std::abs (preset[0] - 300.0f) < 0.01f && std::abs (preset[1] - 700.0f) < 0.01f,
            "the male OOH preset formants changed");

    // Out-of-range indices are a clamp, not an out-of-bounds read: an index
    // before the table returns its first entry and one past it returns the last.
    const auto firstCardinal = vocalor::cardinalVowelPosition (0);
    const auto beforeFirstCardinal = vocalor::cardinalVowelPosition (-1);
    expect (beforeFirstCardinal.x == firstCardinal.x && beforeFirstCardinal.y == firstCardinal.y,
            "a negative cardinal vowel index was not clamped to the first vowel");
    const auto lastCardinal = vocalor::cardinalVowelPosition (vocalor::kCardinalVowelCount - 1);
    const auto pastLastCardinal = vocalor::cardinalVowelPosition (vocalor::kCardinalVowelCount);
    expect (pastLastCardinal.x == lastCardinal.x && pastLastCardinal.y == lastCardinal.y,
            "an out-of-range cardinal vowel index was not clamped to the last vowel");
    expect (std::string (vocalor::cardinalVowelName (-1))
                == std::string (vocalor::cardinalVowelName (0)),
            "a negative cardinal vowel index was not clamped for its display name");
    expect (std::string (vocalor::cardinalVowelName (99))
                == std::string (vocalor::cardinalVowelName (vocalor::kCardinalVowelCount - 1)),
            "an out-of-range cardinal vowel index was not clamped for its display name");

    // presetVowelPosition() clamps to the three shipped presets (AAH/OOH/UUH).
    const auto aah = vocalor::presetVowelPosition (0);
    const auto beforeAah = vocalor::presetVowelPosition (-1);
    expect (beforeAah.x == aah.x && beforeAah.y == aah.y,
            "a negative preset vowel index was not clamped to AAH");
    const auto uuh = vocalor::presetVowelPosition (2);
    const auto pastUuh = vocalor::presetVowelPosition (5);
    expect (pastUuh.x == uuh.x && pastUuh.y == uuh.y,
            "an out-of-range preset vowel index was not clamped to UUH");

    // formantsForPresetVowel() clamps the very same way, but its null-output
    // guard above returns before that logic runs, so it needs its own case with
    // a real buffer to actually exercise the clamp.
    std::array<float, vocalor::kFormantCount> aahFormants {};
    std::array<float, vocalor::kFormantCount> uuhFormants {};
    std::array<float, vocalor::kFormantCount> clampedFormants {};
    vocalor::formantsForPresetVowel (false, 0, aahFormants.data());
    vocalor::formantsForPresetVowel (false, -1, clampedFormants.data());
    expect (aahFormants == clampedFormants,
            "a negative preset vowel index was not clamped to AAH's formants");
    vocalor::formantsForPresetVowel (false, 2, uuhFormants.data());
    vocalor::formantsForPresetVowel (false, 99, clampedFormants.data());
    expect (uuhFormants == clampedFormants,
            "an out-of-range preset vowel index was not clamped to UUH's formants");

    // Both formant resolvers must no-op on a null output buffer rather than
    // crash; reaching the following line is the assertion.
    vocalor::formantsForVowelPoint (false, 0.5f, 0.5f, nullptr);
    vocalor::formantsForPresetVowel (false, 0, nullptr);
}

void testDisplayMathHelpers()
{
    const std::array<float, vocalor::kFormantCount> hz { 700.0f, 1200.0f, 2600.0f, 3400.0f, 4500.0f };
    const std::array<float, vocalor::kFormantCount> bandwidth { 80.0f, 95.0f, 130.0f, 190.0f, 260.0f };
    const std::array<float, vocalor::kFormantCount> gain { 1.0f, 0.66f, 0.34f, 0.18f, 0.095f };

    const auto onFormant = vocalor::formantResponseDb (700.0f, hz.data(), bandwidth.data(),
                                                       gain.data(), vocalor::kFormantCount, 48000.0f);
    const auto valley = vocalor::formantResponseDb (250.0f, hz.data(), bandwidth.data(),
                                                    gain.data(), vocalor::kFormantCount, 48000.0f);
    const auto top = vocalor::formantResponseDb (9000.0f, hz.data(), bandwidth.data(),
                                                 gain.data(), vocalor::kFormantCount, 48000.0f);
    expect (onFormant > valley + 6.0f, "the formant response did not peak at F1");
    expect (onFormant > top + 6.0f, "the formant response did not roll off above the formants");
    expect (std::isfinite (onFormant) && std::isfinite (valley) && std::isfinite (top),
            "the formant response produced a non-finite value");
    expect (vocalor::formantResponseDb (700.0f, nullptr, nullptr, nullptr, 0, 0.0f) <= -119.0f,
            "the formant response did not fall back safely on missing data");

    // formantResponseCoefficients()/formantResponseDbFromCoefficients() split
    // the same formant-bank/probe-frequency arithmetic in two so a caller that
    // plots many points (the editor's response curve) can resolve the fixed
    // formant-bank terms once instead of on every probe. The split must stay
    // bit-identical to the one-shot formantResponseDb() it was factored from.
    std::array<float, vocalor::kFormantCount> responseA1 {};
    std::array<float, vocalor::kFormantCount> responseA2 {};
    std::array<float, vocalor::kFormantCount> responseScale {};
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 48000.0f,
                                          responseA1.data(), responseA2.data(),
                                          responseScale.data());
    for (const float probe : { 250.0f, 700.0f, 3400.0f, 9000.0f })
    {
        const auto direct = vocalor::formantResponseDb (probe, hz.data(), bandwidth.data(),
                                                         gain.data(), vocalor::kFormantCount,
                                                         48000.0f);
        const auto fromCoefficients = vocalor::formantResponseDbFromCoefficients (
            probe, responseA1.data(), responseA2.data(), responseScale.data(),
            vocalor::kFormantCount, 48000.0f);
        expect (direct == fromCoefficients,
                "the precomputed-coefficient formant response drifted from the direct one");
    }
    expect (vocalor::formantResponseDbFromCoefficients (700.0f, nullptr, nullptr, nullptr, 0, 0.0f)
                <= -119.0f,
            "the precomputed-coefficient formant response did not fall back safely on missing data");

    // Logarithmic frequency axis round trip.
    for (const float probe : { 100.0f, 440.0f, 3000.0f, 9000.0f })
    {
        const auto normalised = vocalor::normalisedLogFrequency (probe, 80.0f, 11000.0f);
        const auto back = vocalor::logFrequencyForNormalised (normalised, 80.0f, 11000.0f);
        expect (std::abs (back - probe) < probe * 0.001f,
                "the logarithmic frequency mapping did not round trip");
    }

    // Both axis helpers guard a degenerate or inverted range rather than
    // dividing by zero or taking the log of a non-positive number.
    expect (vocalor::normalisedLogFrequency (440.0f, 0.0f, 11000.0f) == 0.0f,
            "the log-frequency axis did not fall back safely on a non-positive minimum");
    expect (vocalor::normalisedLogFrequency (440.0f, 11000.0f, 80.0f) == 0.0f,
            "the log-frequency axis did not fall back safely on an inverted range");
    expect (vocalor::logFrequencyForNormalised (0.5f, 0.0f, 11000.0f) == 0.0f,
            "the inverse log-frequency axis did not fall back to the minimum on a non-positive one");
    expect (vocalor::logFrequencyForNormalised (0.5f, 11000.0f, 80.0f) == 11000.0f,
            "the inverse log-frequency axis did not fall back to the minimum on an inverted range");

    expect (std::abs (vocalor::linearToDecibels (1.0f, -60.0f)) < 0.001f,
            "unity was not reported as 0 dB");
    expect (vocalor::linearToDecibels (0.0f, -60.0f) == -60.0f,
            "silence was not clamped to the meter floor");
    expect (vocalor::linearToDecibels (std::numeric_limits<float>::quiet_NaN(), -60.0f) == -60.0f,
            "a non-finite linear level was not clamped to the meter floor");
    expect (std::abs (vocalor::decibelsToMeterPosition (-30.0f, -60.0f, 0.0f) - 0.5f) < 0.001f,
            "the meter position mapping is not linear in decibels");
    expect (vocalor::decibelsToMeterPosition (std::numeric_limits<float>::quiet_NaN(), -60.0f, 0.0f)
                == 0.0f,
            "the meter position mapping did not fall back safely on a non-finite reading");
    expect (vocalor::decibelsToMeterPosition (-30.0f, 0.0f, -60.0f) == 0.0f,
            "the meter position mapping did not fall back safely on an inverted floor/ceiling");

    // Ballistics: instant attack, gradual release.
    const auto attack = vocalor::smoothingCoefficient (0.001f, 0.02f);
    const auto release = vocalor::smoothingCoefficient (0.28f, 0.02f);
    expect (release > 0.0f && release < 0.2f, "the meter release coefficient is out of range");
    expect (vocalor::smoothingCoefficient (0.0f, 0.02f) == 1.0f,
            "a non-positive time constant did not fall back to an instant coefficient");
    expect (vocalor::smoothingCoefficient (0.28f, 0.0f) == 1.0f,
            "a non-positive update interval did not fall back to an instant coefficient");
    float level = 0.0f;
    level = vocalor::meterFollow (level, 1.0f, 1.0f, release);
    expect (std::abs (level - 1.0f) < 1.0e-6f, "the meter did not track a rising peak instantly");
    const auto afterOneStep = vocalor::meterFollow (level, 0.0f, attack, release);
    expect (afterOneStep < level && afterOneStep > 0.85f,
            "the meter release was either frozen or instantaneous");
    expect (vocalor::meterFollow (std::numeric_limits<float>::quiet_NaN(), 0.5f, 1.0f, 0.1f)
                == 0.5f,
            "the meter did not recover from a non-finite state");
    expect (vocalor::meterFollow (0.0f, std::numeric_limits<float>::quiet_NaN(), 1.0f, 0.1f)
                == 0.0f,
            "the meter did not recover from a non-finite target");

    expect (std::abs (vocalor::roomSizeScale (0.5f) - 1.0f) < 1.0e-6f,
            "the default room size did not reproduce the historical geometry");
    expect (vocalor::roomSizeScale (0.0f) < 0.5f && vocalor::roomSizeScale (1.0f) > 2.0f,
            "the room size range is too narrow to be useful");
    expect (std::abs (vocalor::formantShiftRatio (0.0f) - 1.0f) < 1.0e-6f,
            "a zero formant shift was not neutral");
    expect (std::abs (vocalor::formantShiftRatio (12.0f) - 2.0f) < 1.0e-4f,
            "a twelve-semitone formant shift was not an octave");
    expect (vocalor::formantShiftRatio (std::numeric_limits<float>::quiet_NaN()) == 1.0f,
            "a non-finite formant shift did not fall back to unity");
    expect (vocalor::formantShiftRatio (std::numeric_limits<float>::infinity()) == 1.0f,
            "an infinite formant shift did not fall back to unity");
    expect (vocalor::glideTimeSeconds (0.0f) == 0.0f
                && vocalor::glideTimeSeconds (1.0f) > 0.4f
                && vocalor::glideTimeSeconds (0.5f) < vocalor::glideTimeSeconds (1.0f),
            "the glide time mapping is not monotonic over a useful range");

    // vibratoExtentCents() is only exercised indirectly, through the engine's
    // chunk-rate vibrato depth and the peak-to-peak measurements documented in
    // the README; assert its own knob curve and its section soft-limiter
    // directly, including the two compatibility anchors the curve's exponent
    // was fixed against.
    expect (vocalor::vibratoExtentCents (0.0f, 0.0f) == 0.0f,
            "a zero knob produced a nonzero vibrato extent");
    expect (vocalor::vibratoExtentCents (-1.0f, 0.0f) == 0.0f,
            "a negative knob was not clamped to a zero extent before the curve was applied");
    expect (std::abs (vocalor::vibratoExtentCents (1.0f, 0.0f) - vocalor::kVibratoReachCents) < 1.0e-3f,
            "the top of the knob did not reach its own +/-1-semitone definition without a section limit");
    expect (std::abs (vocalor::vibratoExtentCents (1.5f, 0.0f) - vocalor::kVibratoReachCents) < 1.0e-3f,
            "a knob value above unity was not clamped before the extent curve was applied");
    expect (std::abs (vocalor::vibratoExtentCents (0.38f, 0.0f) - 18.4f) < 0.05f,
            "the 38% historical anchor drifted off its documented extent");
    expect (std::abs (vocalor::vibratoExtentCents (0.42f, 0.0f) - 21.9f) < 0.05f,
            "the 42% compatibility anchor the curve's exponent was fixed against drifted off 21.9 cents");
    expect (std::abs (vocalor::vibratoExtentCents (0.46f, 0.0f) - 25.7f) < 0.05f,
            "the 46% historical anchor drifted off its documented extent");
    expect (std::abs (vocalor::vibratoExtentCents (1.0f, -5.0f) - vocalor::kVibratoReachCents) < 1.0e-3f,
            "a negative section limit was incorrectly treated as an active limit");
    const auto belowKnee = vocalor::vibratoExtentCents (0.30f, vocalor::kSectionVibratoCents);
    expect (std::abs (belowKnee - vocalor::vibratoExtentCents (0.30f, 0.0f)) < 1.0e-4f,
            "a section limit changed an extent that had not yet reached its own knee");
    const auto midKnob = vocalor::vibratoExtentCents (0.5f, vocalor::kSectionVibratoCents);
    const auto fullKnob = vocalor::vibratoExtentCents (1.0f, vocalor::kSectionVibratoCents);
    expect (fullKnob < vocalor::kSectionVibratoCents,
            "the section limiter let the knob reach or exceed its own limit");
    expect (midKnob > belowKnee && midKnob < fullKnob,
            "the section-limited extent was not monotonic in the knob");
    expect (std::abs (fullKnob - 36.0f) < 0.05f,
            "the full-knob, section-limited extent drifted off its expected asymptotic value");

    // tunedFirstFormant() is only exercised indirectly, through the engine's
    // high-pitch formant tracking in testFormantTuningAtHighPitch(); assert
    // its own behaviour directly, including the two defensive fallbacks that
    // no engine-level test happens to reach.
    expect (vocalor::tunedFirstFormant (850.0f, 200.0f, 1400.0f) == 850.0f,
            "a fundamental well below F1 moved the tracked formant");
    const auto trackedMidway = vocalor::tunedFirstFormant (300.0f, 315.0f, 1400.0f);
    expect (trackedMidway > 300.0f && trackedMidway < 315.0f,
            "the tracking strategy did not engage smoothly between its start and its target");
    expect (vocalor::tunedFirstFormant (300.0f, 5000.0f, 1400.0f) == 1400.0f,
            "the tracked formant did not stop at its ceiling");
    expect (vocalor::tunedFirstFormant (0.0f, 400.0f, 1400.0f) == 0.0f,
            "an invalid base formant was not returned unchanged");
    expect (vocalor::tunedFirstFormant (300.0f, -50.0f, 1400.0f) == 300.0f,
            "a non-positive fundamental was not returned as the vowel's own F1");
    expect (vocalor::tunedFirstFormant (
                300.0f, std::numeric_limits<float>::quiet_NaN(), 1400.0f) == 300.0f,
            "a non-finite fundamental was not sanitized to the vowel's own F1");

    // formantResponseCoefficients() shares formantResponseDb()'s guard clause;
    // an invalid formant bank must leave the caller's buffers untouched rather
    // than writing through a null pointer or an out-of-range count.
    std::array<float, vocalor::kFormantCount> guardA1 { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    std::array<float, vocalor::kFormantCount> guardA2 = guardA1;
    std::array<float, vocalor::kFormantCount> guardScale = guardA1;
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(), 0,
                                          48000.0f, guardA1.data(), guardA2.data(),
                                          guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a non-positive formant count");
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 0.0f, guardA1.data(),
                                          guardA2.data(), guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a non-positive sample rate");
    vocalor::formantResponseCoefficients (nullptr, bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 48000.0f, guardA1.data(),
                                          guardA2.data(), guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null formant-Hz pointer");
    // Every one of the guard's other null-pointer terms has to be checked in
    // isolation too, or a regression dropping any single one of them from the
    // condition would still pass with the formant-Hz check above alone.
    vocalor::formantResponseCoefficients (hz.data(), nullptr, gain.data(),
                                          vocalor::kFormantCount, 48000.0f, guardA1.data(),
                                          guardA2.data(), guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null formant-bandwidth pointer");
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), nullptr,
                                          vocalor::kFormantCount, 48000.0f, guardA1.data(),
                                          guardA2.data(), guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null formant-gain pointer");
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 48000.0f, nullptr,
                                          guardA2.data(), guardScale.data());
    expect (guardA2[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null outA1 pointer");
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 48000.0f, guardA1.data(),
                                          nullptr, guardScale.data());
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null outA2 pointer");
    vocalor::formantResponseCoefficients (hz.data(), bandwidth.data(), gain.data(),
                                          vocalor::kFormantCount, 48000.0f, guardA1.data(),
                                          guardA2.data(), nullptr);
    expect (guardA1[0] == -1.0f,
            "formantResponseCoefficients wrote its output with a null outScale pointer");
}

void testVowelMorphAndFormantShift()
{
    constexpr auto sampleRate = 48000.0;

    // With morph at zero the pad position must be inaudible: the engine has to
    // reproduce the preset vowel exactly, which is what protects 1.0 sessions.
    vocalor::VoiceEngine reference;
    vocalor::VoiceEngine moved;
    for (auto* engine : { &reference, &moved })
    {
        engine->prepare (sampleRate, blockSize);
        engine->reset();
    }
    auto neutral = makeParameters (0, 0, 0, 0);
    reference.setParameters (neutral);
    auto shifted = neutral;
    shifted.vowelX = 1.0f;
    shifted.vowelY = 0.0f;
    moved.setParameters (shifted);
    reference.noteOn (60, 0.8f);
    moved.noteOn (60, 0.8f);
    expect (renderMono (reference, 4096) == renderMono (moved, 4096),
            "the vowel pad position changed the sound while morph was at zero");

    // Morphing to the close-front corner must lift F2 far above the AAH anchor.
    vocalor::VoiceEngine morphed;
    morphed.prepare (sampleRate, blockSize);
    morphed.reset();
    auto full = neutral;
    full.vowelMorph = 1.0f;
    full.vowelX = 1.0f;
    full.vowelY = 0.0f;
    morphed.setParameters (full);
    morphed.noteOn (60, 0.8f);
    const auto morphedAudio = render (morphed, 8192);
    // The epilarynx cluster reads the tension smoother, so the tract is only
    // comparable between engines once that smoother has settled in both.
    render (reference, static_cast<int> (sampleRate * 0.3));
    render (morphed, static_cast<int> (sampleRate * 0.3));
    const auto anchorFormants = vocalor::VoiceEngineTestAccess::chunkFormants (reference);
    const auto morphFormants = vocalor::VoiceEngineTestAccess::chunkFormants (morphed);
    expect (morphedAudio.finite && morphedAudio.rms() > 1.0e-6,
            "a fully morphed vowel produced no usable audio");
    expect (morphFormants[1] > anchorFormants[1] + 800.0f,
            "morphing to the close-front corner did not raise F2");
    expect (morphFormants[0] < anchorFormants[0] - 200.0f,
            "morphing to the close corner did not lower F1");

    // A partial morph has to land between the two, not snap.
    vocalor::VoiceEngine partial;
    partial.prepare (sampleRate, blockSize);
    partial.reset();
    auto half = full;
    half.vowelMorph = 0.5f;
    partial.setParameters (half);
    partial.noteOn (60, 0.8f);
    render (partial, blockSize);
    const auto halfFormants = vocalor::VoiceEngineTestAccess::chunkFormants (partial);
    const auto expectedF2 = 0.5f * (anchorFormants[1] + morphFormants[1]);
    expect (std::abs (halfFormants[1] - expectedF2) < 1.0f,
            "a half morph did not land halfway between the anchor and the target");

    // Formant shift scales the whole tract without touching the pitch.
    vocalor::VoiceEngine shiftedEngine;
    shiftedEngine.prepare (sampleRate, blockSize);
    shiftedEngine.reset();
    auto up = neutral;
    up.formantShift = 7.0f;
    shiftedEngine.setParameters (up);
    shiftedEngine.noteOn (60, 0.8f);
    const auto shiftedAudio = render (shiftedEngine, static_cast<int> (sampleRate * 0.4));
    const auto shiftedFormants = vocalor::VoiceEngineTestAccess::chunkFormants (shiftedEngine);
    const auto ratio = vocalor::formantShiftRatio (7.0f);
    for (int i = 0; i < vocalor::kFormantCount; ++i)
        expect (std::abs (shiftedFormants[static_cast<std::size_t> (i)]
                          - anchorFormants[static_cast<std::size_t> (i)] * ratio) < 1.0f,
                "formant shift did not scale formant " + std::to_string (i + 1));
    expect (shiftedAudio.finite && shiftedAudio.rms() > 1.0e-6,
            "a shifted tract produced no usable audio");

    // An extreme downward shift must stay stable rather than fold the poles.
    vocalor::VoiceEngine extreme;
    extreme.prepare (sampleRate, blockSize);
    extreme.reset();
    auto down = neutral;
    down.formantShift = -12.0f;
    down.resonance = 1.0f;
    extreme.setParameters (down);
    extreme.noteOn (36, 1.0f);
    const auto extremeAudio = render (extreme, static_cast<int> (sampleRate * 0.5));
    expect (extremeAudio.finite && extremeAudio.peak < 16.0,
            "an extreme downward formant shift destabilised the tract");
}

void testGlideAndLegato()
{
    constexpr auto sampleRate = 48000.0;
    const auto hzFor = [] (int midi) { return 440.0f * std::exp2 ((static_cast<float> (midi) - 69.0f) / 12.0f); };

    // Portamento: the new note starts on the old pitch and arrives on target.
    vocalor::VoiceEngine glided;
    glided.prepare (sampleRate, blockSize);
    glided.reset();
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.glide = 0.55f;
    parameters.vibrato = 0.0f;
    glided.setParameters (parameters);

    glided.noteOn (48, 0.8f);
    render (glided, static_cast<int> (sampleRate * 0.3));
    glided.noteOff (48);
    glided.noteOn (60, 0.8f);
    render (glided, 512);
    const auto startFrequency = vocalor::VoiceEngineTestAccess::frequencyForRoot (glided, 60);
    render (glided, static_cast<int> (sampleRate * 1.6));
    const auto endFrequency = vocalor::VoiceEngineTestAccess::frequencyForRoot (glided, 60);

    expect (startFrequency > 0.0f && startFrequency < hzFor (54),
            "glide did not start the new note near the previous pitch");
    expect (std::abs (endFrequency - hzFor (60)) < hzFor (60) * 0.06f,
            "glide did not settle on the target pitch");

    // Without glide the new note starts on pitch immediately.
    vocalor::VoiceEngine direct;
    direct.prepare (sampleRate, blockSize);
    direct.reset();
    auto instant = parameters;
    instant.glide = 0.0f;
    direct.setParameters (instant);
    direct.noteOn (48, 0.8f);
    render (direct, static_cast<int> (sampleRate * 0.3));
    direct.noteOff (48);
    direct.noteOn (60, 0.8f);
    render (direct, 512);
    expect (vocalor::VoiceEngineTestAccess::frequencyForRoot (direct, 60) > hzFor (58),
            "a zero glide setting still bent the new note");

    // Legato: an overlapping note bends the sounding voice instead of restarting it.
    vocalor::VoiceEngine legato;
    legato.prepare (sampleRate, blockSize);
    legato.reset();
    auto phrased = parameters;
    phrased.legato = true;
    phrased.glide = 0.0f;
    legato.setParameters (phrased);

    legato.noteOn (60, 0.8f);
    render (legato, static_cast<int> (sampleRate * 0.4));
    const auto envelopeBefore = vocalor::VoiceEngineTestAccess::soundingEnvelope (legato);
    const auto voicesBefore = legato.getActiveVoiceCount();
    expect (envelopeBefore > 0.9f, "the legato test note did not reach a sustained level");

    legato.noteOn (64, 0.8f);
    render (legato, 256);
    expect (legato.getActiveVoiceCount() == voicesBefore,
            "legato started a second voice instead of bending the sounding one");
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == 64,
            "legato did not retune the sounding voice to the new note");
    expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (legato) > 0.9f,
            "legato re-attacked the amplitude envelope");
    expect (vocalor::VoiceEngineTestAccess::heldNoteCount (legato) == 2,
            "the legato note stack did not record both held notes");

    // Repeating the pitch that is already sounding is not a move between
    // pitches, so it must still articulate. Taking the legato path here only
    // retuned the voice to itself, which made overlapping repeats from layered
    // controllers or a loop boundary silent.
    {
        vocalor::VoiceEngine repeated;
        repeated.prepare (sampleRate, blockSize);
        repeated.reset();
        repeated.setParameters (phrased);

        repeated.noteOn (60, 0.8f);
        render (repeated, static_cast<int> (sampleRate * 0.4));
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) > 0.9f,
                "the repeated-root test note did not reach a sustained level");

        repeated.noteOn (60, 0.8f);
        render (repeated, 64);
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) < 0.5f,
                "a repeated root under legato did not re-attack");
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == 60,
                "the repeated root did not stay on its own pitch");

        // Both sources are still holding the pitch, so the stack keeps one
        // entry and the first note-off must not release the retriggered voice.
        expect (vocalor::VoiceEngineTestAccess::heldNoteCount (repeated) == 1,
                "an overlapping repeat added a duplicate held-stack entry");

        render (repeated, static_cast<int> (sampleRate * 0.4));
        repeated.noteOff (60);
        render (repeated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == 60
                    && vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) > 0.9f,
                "one note-off released a pitch another controller still held");

        repeated.noteOff (60);
        render (repeated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == -1
                    && vocalor::VoiceEngineTestAccess::heldNoteCount (repeated) == 0,
                "the final note-off did not release the overlapping repeat");
    }

    // Legato can be switched off in the middle of a phrase. The sounding
    // voices were bent to the top note rather than started for it, so
    // releasing it still has to hand them back to the key underneath -- the
    // player is physically holding it and would otherwise hear nothing.
    {
        vocalor::VoiceEngine automated;
        automated.prepare (sampleRate, blockSize);
        automated.reset();
        automated.setParameters (phrased);

        automated.noteOn (60, 0.8f);
        render (automated, static_cast<int> (sampleRate * 0.3));
        automated.noteOn (64, 0.8f);
        render (automated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (automated) == 64,
                "the legato phrase did not reach the upper note");

        auto detached = phrased;
        detached.legato = false;
        automated.setParameters (detached);

        automated.noteOff (64);
        render (automated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (automated) == 60,
                "disabling legato mid-phrase silenced the key still held");
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (automated) > 0.9f,
                "falling back after legato was disabled re-attacked or released");
    }

    legato.noteOff (64);
    render (legato, 256);
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == 60,
            "releasing the top of a legato phrase did not fall back to the held note");
    expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (legato) > 0.9f,
            "falling back to the held note re-attacked the envelope");

    legato.noteOff (60);
    render (legato, 256);
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == -1,
            "releasing the last held note did not start the release");
    expect (vocalor::VoiceEngineTestAccess::heldNoteCount (legato) == 0,
            "the legato note stack was not emptied");

    const auto tail = render (legato, static_cast<int> (sampleRate * 4.0));
    expect (tail.finite && legato.getActiveVoiceCount() == 0,
            "a legato phrase left voices hanging after the final release");
}

/** The room's impulse response as heard from one singer's position: a unit
    sample into her placement input, every voice silent, captured through her
    direct path, her four first-order images and the tail they excite. */
std::vector<float> placementImpulseResponse (const vocalor::EngineParameters& parameters,
                                             int singerIndex, double seconds)
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.setParameters (parameters);
    // Reset after publishing the fixture parameters so every smoothed control
    // starts on the requested value. Otherwise a D=0 probe spends its first
    // seconds asymptotically leaving the fresh D=.38 default and cannot prove
    // the exact-zero branch it names.
    engine.reset();
    // Rendering silence still advances every smoother to its target, which is
    // what the taps and the room coefficients are resolved from.
    render (engine, static_cast<int> (sampleRate * 1.0));
    return vocalor::VoiceEngineTestAccess::placementImpulse (
        engine, singerIndex, static_cast<int> (sampleRate * seconds));
}

/** The recirculating network's own impulse response, with the taps and the
    feedback resolved from @c parameters. A reverberation time is a property of
    the tail, and the placement response above is dominated by the direct
    arrival, so the two are not interchangeable: measured, Schroeder T20 reads
    0.051 s on the placement response at Size 50 % / Room 50 % and 0.231 s
    here. */
std::vector<float> roomOnlyImpulseResponse (const vocalor::EngineParameters& parameters,
                                            double seconds)
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (parameters);
    render (engine, static_cast<int> (sampleRate * 2.0));
    return vocalor::VoiceEngineTestAccess::roomImpulse (
        engine, static_cast<int> (sampleRate * seconds));
}

/** RT60 by Schroeder backward integration of @c response, extrapolated from the
    -5 dB to -25 dB span (T20). Negative if the decay never reaches -25 dB. */
double schroederRt60 (const std::vector<float>& response, double sampleRate)
{
    std::vector<double> energy (response.size());
    double sum = 0.0;
    for (int i = static_cast<int> (response.size()) - 1; i >= 0; --i)
    {
        const auto index = static_cast<std::size_t> (i);
        sum += static_cast<double> (response[index]) * response[index];
        energy[index] = sum;
    }
    if (energy[0] <= 0.0)
        return -1.0;
    int at5 = -1;
    int at25 = -1;
    for (std::size_t i = 0; i < energy.size(); ++i)
    {
        const auto db = 10.0 * std::log10 (std::max (energy[i], 1.0e-300) / energy[0]);
        if (at5 < 0 && db <= -5.0)
            at5 = static_cast<int> (i);
        if (db <= -25.0)
        {
            at25 = static_cast<int> (i);
            break;
        }
    }
    if (at5 < 0 || at25 <= at5)
        return -1.0;
    return 3.0 * static_cast<double> (at25 - at5) / sampleRate;
}

void testRoomSizeGeometry()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int probeSamples = 20000;

    const auto probe = [] (float size)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.room = 1.0f;
        parameters.roomSize = size;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.9f);
        return renderMono (engine, probeSamples);
    };

    // Two renders that differ only in room size are identical until the first
    // reflection of the smaller room arrives, which measures the tap geometry
    // directly rather than through a decay estimate.
    const auto firstDivergence = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::abs (a[i] - b[i]) > 1.0e-9f)
                return static_cast<int> (i);
        return -1;
    };

    const auto tight = probe (0.05f);
    const auto neutral = probe (0.50f);
    const auto wide = probe (0.95f);

    const auto neutralArrival = firstDivergence (neutral, wide);
    const auto tightArrival = firstDivergence (tight, neutral);
    // The size-dependent tap that arrives first is no longer the recirculating
    // network's shortest delay but the nearest singer's floor image. Identity 0
    // stands 1.700 m away and the floor is 1.55 m below the listener plane at
    // Size 50 %, so the image path is sqrt(1.700^2 + 3.100^2) = 3.536 m; the
    // taps are referred to 1.615 m, which puts the divergence at 268.8 samples
    // -- 5.60 ms, against the 29.7 ms the four-tap network used to take.
    const auto expectedNeutral = 268.8;

    expect (neutralArrival > 0 && tightArrival > 0,
            "changing the room size did not change the rendered audio at all");
    std::cout << "room geometry: first size-dependent arrival " << neutralArrival
              << " samples at Size 50 %, " << tightArrival << " at Size 5 %\n";
    expect (std::abs (static_cast<double> (neutralArrival) - expectedNeutral)
                < expectedNeutral * 0.12,
            "the default room size no longer puts its first image where the geometry says");
    expect (tightArrival < neutralArrival,
            "a smaller room did not bring its first reflection forward");

    // What the size control has to buy musically is a longer tail. Subtracting
    // a dry render from a wet one no longer isolates it -- Size also moves the
    // image field, which is louder in a small room because its surfaces are
    // nearer -- so the decay is measured on the recirculating network's own
    // impulse response. Not on the placement response: that one carries the
    // direct arrival, which sits 7.5 dB above everything the room does with it,
    // so its Schroeder integral reads the direct-to-reverberant ratio rather
    // than a decay (0.084 s at Size 5 % against 0.116 s at Size 95 %, a ratio
    // of 1.4 where the tail's own is 9.4).
    const auto decayFor = [] (float size)
    {
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.room = 1.0f;
        parameters.roomSize = size;
        return schroederRt60 (roomOnlyImpulseResponse (parameters, 6.0), sampleRate);
    };
    const auto wideDecay = decayFor (0.95f);
    const auto tightDecay = decayFor (0.05f);
    std::cout << "room RT60: " << std::fixed << std::setprecision (3) << tightDecay
              << " s at Size 5 %, " << wideDecay << " s at Size 95 %\n";
    expect (tightDecay > 0.0 && wideDecay > tightDecay * 3.0,
            "a larger room did not ring for noticeably longer");

    // The tail still has to end, and the engine has to release its voices.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.room = 1.0f;
    parameters.roomSize = 1.0f;
    engine.setParameters (parameters);
    engine.noteOn (55, 0.9f);
    render (engine, static_cast<int> (sampleRate * 0.5));
    engine.noteOff (55);
    const auto tail = render (engine, static_cast<int> (sampleRate * 12.0));
    expect (tail.finite, "a maximum-size room tail produced invalid audio");
    expect (engine.getActiveVoiceCount() == 0, "a large room kept voices alive");
    const auto silence = render (engine, static_cast<int> (sampleRate * 0.2));
    expect (silence.peak == 0.0, "the maximum-size room tail never rang out");
}

/** One two-pole bandpass, geometric centre and Q from the band edges. */
struct BandFilter
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    BandFilter (double low, double high, double sampleRate)
    {
        const auto centre = std::sqrt (low * high);
        const auto q = centre / (high - low);
        const auto omega = 2.0 * 3.14159265358979323846 * centre / sampleRate;
        const auto alpha = std::sin (omega) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        b0 = alpha / a0;
        b1 = 0.0;
        b2 = -alpha / a0;
        a1 = -2.0 * std::cos (omega) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    double process (double x) noexcept
    {
        const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

/** Normalised L/R correlation inside one band, over [from, to). The filters run
    from sample zero so their own transient is out of the window. */
double bandCorrelation (const std::vector<float>& left, const std::vector<float>& right,
                        double low, double high, int from, int to)
{
    constexpr auto sampleRate = 48000.0;
    BandFilter filterLeft (low, high, sampleRate);
    BandFilter filterRight (low, high, sampleRate);
    double product = 0.0;
    double squareLeft = 0.0;
    double squareRight = 0.0;
    for (int i = 0; i < to; ++i)
    {
        const auto a = filterLeft.process (left[static_cast<std::size_t> (i)]);
        const auto b = filterRight.process (right[static_cast<std::size_t> (i)]);
        if (i < from)
            continue;
        product += a * b;
        squareLeft += a * a;
        squareRight += b * b;
    }
    return product / std::sqrt (std::max (squareLeft * squareRight, 1.0e-30));
}

struct StereoRender
{
    std::vector<float> left, right;
};

StereoRender renderStereo (vocalor::VoiceEngine& engine, int samples)
{
    StereoRender out;
    out.left.resize (static_cast<std::size_t> (samples), 0.0f);
    out.right.resize (static_cast<std::size_t> (samples), 0.0f);
    std::vector<float> left (blockSize, 0.0f);
    std::vector<float> right (blockSize, 0.0f);
    for (int rendered = 0; rendered < samples;)
    {
        const auto count = std::min (blockSize, samples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
        {
            out.left[static_cast<std::size_t> (rendered + i)] = left[static_cast<std::size_t> (i)];
            out.right[static_cast<std::size_t> (rendered + i)] = right[static_cast<std::size_t> (i)];
        }
        rendered += count;
    }
    return out;
}

/** Choir/12 at Spread 100 %, Humanize 100 %, Vibrato 60 %, holding C4. Room is
    a parameter because the same measurement has to be repeated at Room 50 % to
    show the placement and not the reverb is what decorrelated the output. */
vocalor::EngineParameters placementParameters (float room)
{
    auto parameters = makeParameters (2, 0, 0, 0);
    parameters.choirSize = 12;
    parameters.spread = 1.0f;
    parameters.humanize = 1.0f;
    parameters.vibrato = 0.6f;
    parameters.room = room;
    parameters.roomSize = 0.5f;
    return parameters;
}

std::array<double, 4> placementCorrelations (float room)
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (placementParameters (room));
    engine.noteOn (60, 0.85f);
    const auto audio = renderStereo (engine, static_cast<int> (sampleRate * 4.0));
    const auto from = static_cast<int> (sampleRate * 1.0);
    const auto to = static_cast<int> (sampleRate * 4.0);
    return { bandCorrelation (audio.left, audio.right, 150.0, 400.0, from, to),
             bandCorrelation (audio.left, audio.right, 400.0, 1200.0, from, to),
             bandCorrelation (audio.left, audio.right, 1200.0, 3000.0, from, to),
             bandCorrelation (audio.left, audio.right, 3000.0, 8000.0, from, to) };
}

/** Twelve singers are twelve positions in a room, not twelve pan values. A pan
    law applies the same signal to both outputs at two levels, so the two
    outputs stay correlated however wide it is set -- measured on this fixture
    with the placement replaced by the sqrt pan law it supersedes, 0.954 /
    0.958 / 0.971 / 0.970 across the four bands, flat to 0.017 across five
    octaves. (The audit published 0.876 / 0.887 / 0.881 / 0.882 on the pre-step
    build itself; the shape is the same and the reading is 0.09 higher because
    the reconstruction does not carry the pan glide.) A position gives each
    singer her own arrival time, and only a time difference makes two channels
    different. */
void testSingerPlacement()
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto speedOfSound = 343.0;

    // Room pinned at 0: the audit left it unstated, and a step that rebuilds
    // the room could make the reverb a confound even though it is not one today.
    const auto dry = placementCorrelations (0.0f);
    const auto half = placementCorrelations (0.5f);
    std::cout << std::fixed << std::setprecision (4)
              << "placement correlation, Room 0:    " << dry[0] << " " << dry[1] << " "
              << dry[2] << " " << dry[3] << "\n"
              << "placement correlation, Room 50 %: " << half[0] << " " << half[1] << " "
              << half[2] << " " << half[3] << "\n";

    expect (dry[2] < 0.60, "the 1200-3000 Hz band is still as correlated as a pan law leaves it");
    expect (dry[3] < 0.70, "the 3000-8000 Hz band is still as correlated as a pan law leaves it");
    // A pan law is frequency-blind, so its correlation is the same at 200 Hz as
    // at 5 kHz. A geometry is not: a 13 ms spread of arrival times decorrelates
    // the top of the band long before it touches the bottom.
    // The source/tract revoice moved the deterministic reading to 0.087 while
    // the pan-law control remains flat to 0.017. Keep a fourfold separation
    // from that control without pretending the source spectrum is invariant.
    expect (dry[0] - dry[3] > 0.07,
            "the section's correlation is still flat with frequency, which is a pan and not a room");
    for (std::size_t band = 0; band < dry.size(); ++band)
        expect (std::abs (dry[band] - half[band]) < 0.05,
                "adding reverb moved the correlation, so the reverb and not the placement did it");

    // The geometry itself. A correlation figure alone does not tell a room from
    // any other decorrelator.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (placementParameters (0.0f));
    render (engine, blockSize);

    const auto distances = vocalor::VoiceEngineTestAccess::singerDistances (engine);
    const auto delays = vocalor::VoiceEngineTestAccess::directDelaySamples (engine);
    expect (distances.size() == 12 && delays.size() == 12,
            "the section no longer has twelve positions");

    auto nearest = 0.0;
    auto farthest = 0.0;
    auto shortest = 1.0e9;
    auto longest = -1.0e9;
    for (std::size_t i = 0; i < distances.size(); ++i)
    {
        const auto arrival = 1000.0 * distances[i] / speedOfSound;
        shortest = std::min (shortest, arrival);
        longest = std::max (longest, arrival);
        if (nearest == 0.0 || distances[i] < nearest)
            nearest = static_cast<double> (distances[i]);
        farthest = std::max (farthest, static_cast<double> (distances[i]));
    }
    std::cout << std::setprecision (3) << "singer arrival times: " << shortest << "-"
              << longest << " ms (span " << longest - shortest << " ms), "
              << nearest << "-" << farthest << " m\n";
    // 1.5-6 m at 343 m/s. The delays actually applied are these less the
    // nearest singer's, which is a choice of time origin and leaves every
    // relative arrival exact; the geometry is what has to sit in the range.
    expect (shortest > 4.4 && longest < 17.5,
            "the section no longer stands between 1.5 and 6 m from the listener");
    expect (longest - shortest > 8.0,
            "the section has no depth: every singer arrives at nearly the same time");

    // 1/r, read at Spread 0. At any other spread the direct gain also carries
    // the receivers' cardioid pattern, which is a direction and not a distance.
    vocalor::VoiceEngine centred;
    centred.prepare (sampleRate, blockSize);
    centred.reset();
    auto centredParameters = placementParameters (0.0f);
    centredParameters.spread = 0.0f;
    centred.setParameters (centredParameters);
    render (centred, blockSize);
    const auto centredDistances = vocalor::VoiceEngineTestAccess::singerDistances (centred);
    const auto gains = vocalor::VoiceEngineTestAccess::directGains (centred);
    auto worstError = 0.0;
    for (std::size_t i = 0; i < gains.size(); ++i)
        for (std::size_t j = 0; j < gains.size(); ++j)
        {
            const auto measured = 20.0 * std::log10 (gains[i] / gains[j]);
            const auto law = 20.0 * std::log10 (centredDistances[j] / centredDistances[i]);
            worstError = std::max (worstError, std::abs (measured - law));
        }
    std::cout << "direct level against 1/r: worst pairwise error " << worstError << " dB\n";
    expect (worstError < 0.5,
            "the direct level does not follow 1/r, so depth is a spread control and not a distance");

    // The distances do not move. A distance is a delay, and a delay that
    // follows a knob is a pitch shifter; twelve delay lines shared by up to
    // ninety-six voices make a per-note distance impossible as well as unwise.
    engine.noteOn (60, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.5));
    const auto afterFirst = vocalor::VoiceEngineTestAccess::directDelaySamples (engine);
    engine.noteOn (67, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.5));
    const auto afterSecond = vocalor::VoiceEngineTestAccess::directDelaySamples (engine);
    expect (afterFirst == afterSecond,
            "a second note-on moved the singers, which is a pitch shift on every note already sounding");

    auto swept = placementParameters (0.0f);
    for (int step = 0; step <= 20; ++step)
    {
        swept.spread = static_cast<float> (step) / 20.0f;
        swept.roomSize = static_cast<float> (step) / 20.0f;
        engine.setParameters (swept);
        render (engine, static_cast<int> (sampleRate * 0.05));
    }
    expect (afterFirst == vocalor::VoiceEngineTestAccess::directDelaySamples (engine),
            "sweeping Spread or Size moved the direct-path delays");

    // And nothing bends. A per-note distance on a shared line moves it by up to
    // 13.1 ms inside one control period, which is what this bound catches.
    vocalor::VoiceEngine solo;
    solo.prepare (sampleRate, blockSize);
    solo.reset();
    auto soloParameters = placementParameters (0.0f);
    soloParameters.mode = vocalor::PerformanceMode::Solo;
    soloParameters.humanize = 0.0f;
    soloParameters.vibrato = 0.0f;
    soloParameters.instability = 0.0f;
    soloParameters.spread = 0.0f;
    soloParameters.roomSize = 0.0f;
    solo.setParameters (soloParameters);
    solo.noteOn (60, 0.85f);
    render (solo, static_cast<int> (sampleRate * 0.5));
    const auto before = vocalor::VoiceEngineTestAccess::soundingFrequencies (solo);
    expect (! before.empty(), "the soloist stopped sounding before the sweep");
    auto worstCents = 0.0;
    constexpr int sweepSteps = 100;
    for (int step = 0; step <= sweepSteps; ++step)
    {
        soloParameters.spread = static_cast<float> (step) / static_cast<float> (sweepSteps);
        soloParameters.roomSize = static_cast<float> (step) / static_cast<float> (sweepSteps);
        solo.setParameters (soloParameters);
        render (solo, static_cast<int> (sampleRate * 1.0 / sweepSteps));
        const auto now = vocalor::VoiceEngineTestAccess::soundingFrequencies (solo);
        for (std::size_t i = 0; i < std::min (now.size(), before.size()); ++i)
            worstCents = std::max (worstCents,
                                   std::abs (1200.0 * std::log2 (now[i] / before[i])));
    }
    std::cout << std::setprecision (4) << "Solo pitch through a Spread/Size sweep: worst "
              << worstCents << " cents\n";
    expect (worstCents < 1.0,
            "sweeping Spread or Size bent the pitch, so a delay is following a knob");
}

/** A singer who has stopped singing stays stopped.

    Each singer owns a placement delay line, and the line is kept running for a
    hold after her last voice ends so the sound already inside it can finish
    arriving. What goes into the line during that hold is her chunk bus, and the
    bus is only cleared for singers who are about to write to it - so a bus left
    uncleared still holds the final chunk of the note that has just ended, and
    feeding it back into the line once per chunk repeats the end of the note for
    as long as the hold lasts.

    The seam is the length of the tail. Nothing of a singer's can reach the
    listener later than the longest path the placement gives her, so once her
    last voice has gone the output has to be silent within that many samples.
    An engine that recycles the stale bus keeps sounding for the whole hold and
    then flushes that, which is about twice as long again. Room is pinned at 0
    so the reverb's own tail is not what is being measured. */
void testPlacementDoesNotRepeatASilencedSinger()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int poll = 64;   // one chunk, so the seam is located exactly

    auto parameters = makeParameters (1, 0, 0, 0);   // Choir
    parameters.choirSize = 12;
    parameters.humanize = 0.0f;
    parameters.room = 0.0f;
    parameters.roomSize = 0.5f;
    parameters.spread = 1.0f;

    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, poll);
    engine.reset();
    engine.setParameters (parameters);

    const auto maximumDelay =
        vocalor::VoiceEngineTestAccess::placementMaximumSamples (engine);
    expect (maximumDelay > 0,
            "the placement reports no delay at all, so this fixture measures nothing");

    engine.noteOn (60, 0.85f);
    std::vector<float> left (poll, 0.0f);
    std::vector<float> right (poll, 0.0f);
    const auto step = [&]
    {
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        engine.process (left.data(), right.data(), poll);
    };

    for (int i = 0; i < static_cast<int> (sampleRate * 0.6) / poll; ++i)
        step();
    engine.noteOff (60);

    // Run to the block at which the last voice stops writing to a bus, then
    // keep going. Everything past the longest path is measured: with the buses
    // cleared it is the first-order allpass states ringing down as
    // denormal-scale dust, and with a stale bus recycled it is the end of the
    // note being played again at its own level.
    long long position = 0;
    long long lastVoiceEnded = -1;
    double soundingPeak = 0.0;
    double tailPeak = 0.0;
    const auto limit = static_cast<long long> (sampleRate * 12.0);
    while (position < limit)
    {
        step();
        for (int i = 0; i < poll; ++i)
        {
            const auto magnitude = std::max (
                std::abs (static_cast<double> (left[static_cast<std::size_t> (i)])),
                std::abs (static_cast<double> (right[static_cast<std::size_t> (i)])));
            if (lastVoiceEnded < 0)
                soundingPeak = std::max (soundingPeak, magnitude);
            else if (position + i > lastVoiceEnded + maximumDelay + poll)
                tailPeak = std::max (tailPeak, magnitude);
        }
        position += poll;
        if (lastVoiceEnded < 0 && engine.getActiveVoiceCount() == 0)
            lastVoiceEnded = position;
        else if (lastVoiceEnded >= 0 && position > lastVoiceEnded + 4 * maximumDelay)
            break;
    }

    expect (lastVoiceEnded >= 0, "the choir never finished releasing");
    expect (soundingPeak > 1.0e-3, "the choir never sounded at all");
    if (lastVoiceEnded < 0)
        return;

    std::cout << "placement tail past the longest path: peak " << std::scientific
              << tailPeak << " against " << soundingPeak << " while sounding\n"
              << std::fixed;
    // -180 dB absolute. The dust measures about 1e-35 and the repeat about
    // 1e-6, so anything in between separates them; this sits twenty-six orders
    // of magnitude above the one and three below the other.
    expect (tailPeak < 1.0e-9,
            "the placement was still sounding at " + std::to_string (tailPeak)
                + " more than its longest path after the last voice ended, so a "
                  "silenced singer's bus is being repeated into her delay line");
}

/** The arrivals in one singer's impulse response after the direct wavefront has
    passed, measured from her own direct arrival. An early reflection is early
    relative to the direct sound, and the direct sound now has a delay of its
    own. The threshold is relative to the loudest arrival in that same
    post-direct window rather than to the response's peak: the peak is the
    direct sound, which sits 20-30 dB above every image, and on the pre-step
    build -- where the response starts at the first tap of the recirculating
    network and there is no direct sound in it at all -- the two readings are
    the same number. */
struct EarlyArrivals
{
    double directMs = 0.0;
    std::vector<double> times;
};

EarlyArrivals earlyArrivals (const std::vector<float>& response, double windowMs,
                             double threshold)
{
    constexpr auto sampleRate = 48000.0;
    EarlyArrivals result;
    const auto span = static_cast<int> (sampleRate * 0.08);
    auto peak = 0.0;
    for (int i = 0; i < span; ++i)
        peak = std::max (peak, std::abs (static_cast<double> (response[static_cast<std::size_t> (i)])));
    auto direct = 0;
    for (int i = 0; i < span; ++i)
        if (std::abs (static_cast<double> (response[static_cast<std::size_t> (i)])) > 0.02 * peak)
        {
            direct = i;
            break;
        }
    result.directMs = 1000.0 * direct / sampleRate;
    // The two receivers are 0.17 m apart, so both direct arrivals land inside
    // 0.496 ms of each other by construction; the count starts after both.
    const auto first = direct + static_cast<int> (std::ceil (sampleRate * 0.17 / 343.0));
    const auto last = direct + static_cast<int> (sampleRate * windowMs / 1000.0);
    auto windowPeak = 0.0;
    for (int i = first; i < last; ++i)
        windowPeak = std::max (windowPeak,
                               std::abs (static_cast<double> (response[static_cast<std::size_t> (i)])));
    for (int i = first + 1; i + 1 < last; ++i)
    {
        const auto a = std::abs (static_cast<double> (response[static_cast<std::size_t> (i - 1)]));
        const auto b = std::abs (static_cast<double> (response[static_cast<std::size_t> (i)]));
        const auto c = std::abs (static_cast<double> (response[static_cast<std::size_t> (i + 1)]));
        if (b > a && b >= c && b > threshold * windowPeak)
            result.times.push_back (1000.0 * (i - direct) / sampleRate);
    }
    return result;
}

/** What the recirculating network alone could never give the instrument: an
    early-reflection pattern that belongs to a singer rather than to the mix.

    Before this step the first thing to arrive after the direct sound was the
    shortest tap of the shared four-tap network, at 29.67 ms at Size 50 % and
    60.81 ms on Cathedral Ensemble, with two local peaks before 40 ms at Size
    50 % and none at all on Cathedral -- and it was the same pattern for every
    singer, because there was only one network. Those figures are read on the
    network's own response, which is where they had to be read: with no singer
    path there was nothing between the glottis and the tail to inject into. */
void testRoomEarlyReflections()
{
    constexpr auto sampleRate = 48000.0;

    auto neutral = makeParameters (2, 0, 0, 0);
    neutral.choirSize = 12;
    neutral.spread = 1.0f;
    neutral.room = 1.0f;
    neutral.roomSize = 0.5f;

    const auto nearResponse = placementImpulseResponse (neutral, 0, 0.5);
    const auto farResponse = placementImpulseResponse (neutral, 11, 0.5);
    const auto nearEarly = earlyArrivals (nearResponse, 40.0, 0.05);
    const auto farEarly = earlyArrivals (farResponse, 40.0, 0.05);

    std::cout << std::fixed << std::setprecision (3)
              << "Size 50 %, singer 0: direct at " << nearEarly.directMs
              << " ms, " << nearEarly.times.size() << " arrivals in the next 40 ms, first at "
              << (nearEarly.times.empty() ? -1.0 : nearEarly.times.front()) << " ms\n"
              << "Size 50 %, singer 11: direct at " << farEarly.directMs
              << " ms, " << farEarly.times.size() << " arrivals in the next 40 ms, first at "
              << (farEarly.times.empty() ? -1.0 : farEarly.times.front()) << " ms\n";

    expect (! nearEarly.times.empty() && nearEarly.times.front() < 15.0,
            "the first reflection at Size 50 % still arrives too late to be an early reflection");
    // Four first-order images at two receivers is eight arrivals, and at Size
    // 50 % all four surfaces are inside the 40 ms window.
    expect (nearEarly.times.size() >= 8,
            "the section's early-reflection pattern is still as sparse as the four-tap network");

    // A shared tap set gives every singer the same pattern, which is the state
    // this step is leaving.
    auto different = 0;
    for (std::size_t i = 0; i < 4 && i < nearEarly.times.size() && i < farEarly.times.size(); ++i)
        if (std::abs (nearEarly.times[i] - farEarly.times[i]) > 1.0)
            ++different;
    expect (different >= 3,
            "the nearest and the farthest singer hear the same early reflections");

    // Cathedral Ensemble: Size 95 %, so the room is nearly twice as big and its
    // surfaces are correspondingly further away.
    vocalor::EngineParameters cathedral {};
    auto found = false;
    for (int i = 0; i < vocalor::factoryPresetCount(); ++i)
        if (std::string (vocalor::factoryPresetName (i)) == "Cathedral Ensemble")
        {
            cathedral = vocalor::factoryPreset (i).parameters;
            found = true;
        }
    expect (found, "the Cathedral Ensemble preset is gone");
    const auto cathedralEarly = earlyArrivals (placementImpulseResponse (cathedral, 0, 0.5),
                                               40.0, 0.05);
    std::cout << "Cathedral, singer 0: direct at " << cathedralEarly.directMs << " ms, "
              << cathedralEarly.times.size() << " arrivals in the next 40 ms, first at "
              << (cathedralEarly.times.empty() ? -1.0 : cathedralEarly.times.front()) << " ms\n";
    expect (! cathedralEarly.times.empty() && cathedralEarly.times.front() < 25.0,
            "Cathedral Ensemble still has nothing between the direct sound and its tail");
    // Only the floor and the near side wall are inside 40 ms at this size: the
    // ceiling image of the nearest singer is a 46.8 ms path. Three arrivals is
    // what the geometry can put there, against none before this step.
    expect (cathedralEarly.times.size() >= 3,
            "Cathedral Ensemble's early field is still empty");

    // The tail itself must not have moved. RT60 is read on the recirculating
    // network's own response, not on the placement response: the direct arrival
    // dominates the latter and its Schroeder integral measures the
    // direct-to-reverberant ratio instead of a decay.
    auto neutralTail = makeParameters (0, 0, 0, 0);
    neutralTail.room = 0.5f;
    neutralTail.roomSize = 0.5f;
    const auto neutralRt60 = schroederRt60 (roomOnlyImpulseResponse (neutralTail, 6.0), sampleRate);
    const auto cathedralRt60 = schroederRt60 (roomOnlyImpulseResponse (cathedral, 8.0), sampleRate);
    std::cout << "RT60 (Schroeder T20): " << neutralRt60 << " s at Size 50 % / Room 50 %, "
              << cathedralRt60 << " s on Cathedral Ensemble\n";
    expect (std::abs (neutralRt60 - 0.231) < 0.231 * 0.15,
            "the recirculating tail no longer rings for as long as it did at Size 50 %");
    expect (std::abs (cathedralRt60 - 0.847) < 0.847 * 0.15,
            "the recirculating tail no longer rings for as long as it did on Cathedral Ensemble");

    // Wet/dry balance: this is a geometry change and not a new reverb, so the
    // tail has to sit where it sat. The image field is measured out of the way
    // by muting the send, because the images themselves are room sound this
    // step adds on purpose: with them in, Room 100 % minus Room 0 % reads
    // -8.37 dB against the -15.00 dB the pan-law build gives, and the 6.6 dB
    // between the two is the early field rather than a louder reverb.
    const auto tailRatio = [] (bool muteSend)
    {
        const auto renderAt = [muteSend] (float room)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            engine.setParameters (placementParameters (room));
            if (muteSend)
                vocalor::VoiceEngineTestAccess::setPlacementSendGain (engine, 0.0f);
            engine.noteOn (60, 0.85f);
            return renderStereo (engine, static_cast<int> (sampleRate * 4.0));
        };
        return renderAt (1.0f);
    };
    vocalor::VoiceEngine dryEngine;
    dryEngine.prepare (sampleRate, blockSize);
    dryEngine.reset();
    dryEngine.setParameters (placementParameters (0.0f));
    dryEngine.noteOn (60, 0.85f);
    const auto dryRender = renderStereo (dryEngine, static_cast<int> (sampleRate * 4.0));
    const auto wetRender = tailRatio (false);
    const auto imagesOnly = tailRatio (true);
    auto tailEnergy = 0.0;
    auto dryEnergy = 0.0;
    for (int i = static_cast<int> (sampleRate * 1.0); i < static_cast<int> (sampleRate * 4.0); ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        const auto tailLeft = wetRender.left[index] - imagesOnly.left[index];
        const auto tailRight = wetRender.right[index] - imagesOnly.right[index];
        tailEnergy += tailLeft * tailLeft + tailRight * tailRight;
        dryEnergy += static_cast<double> (dryRender.left[index]) * dryRender.left[index]
                   + static_cast<double> (dryRender.right[index]) * dryRender.right[index];
    }
    const auto tailDb = 10.0 * std::log10 (tailEnergy / std::max (dryEnergy, 1.0e-30));
    std::cout << "recirculating tail against the dry render: " << tailDb
              << " dB (pan-law build -15.00 dB)\n";
    expect (std::abs (tailDb - (-15.00)) < 1.0,
            "the recirculating tail changed level, so this is a new reverb and not a geometry");
}

void testDenormalAndNaNSafety()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.room = 0.6f;
    parameters.breath = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.05f);

    render (engine, static_cast<int> (sampleRate * 0.4));
    engine.noteOff (60);

    // Sample the recursive state repeatedly while the tail decays: this is
    // exactly the window in which an unprotected filter falls into denormals.
    float smallest = std::numeric_limits<float>::max();
    for (int step = 0; step < 40; ++step)
    {
        render (engine, static_cast<int> (sampleRate * 0.1));
        const auto observed = vocalor::VoiceEngineTestAccess::smallestNonZeroState (engine);
        if (observed > 0.0f)
            smallest = std::min (smallest, observed);
    }
    expect (smallest > 1.0e-32f,
            "recursive state reached denormal range during the release tail");

    // Hostile parameters must never leak a NaN into the output.
    vocalor::VoiceEngine hostile;
    hostile.prepare (sampleRate, blockSize);
    hostile.reset();
    auto poisoned = makeParameters (1, 0, 1, 2);
    const auto notANumber = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();
    poisoned.breath = notANumber;
    poisoned.resonance = infinity;
    poisoned.vibrato = -infinity;
    poisoned.humanize = notANumber;
    poisoned.instability = infinity;
    poisoned.spread = infinity;
    poisoned.tension = notANumber;
    poisoned.room = infinity;
    poisoned.outputGain = notANumber;
    poisoned.vowelX = notANumber;
    poisoned.vowelY = infinity;
    poisoned.vowelMorph = notANumber;
    poisoned.formantShift = notANumber;
    poisoned.glide = infinity;
    poisoned.roomSize = notANumber;
    hostile.setParameters (poisoned);
    hostile.noteOn (72, 1.0f);
    const auto poisonedRender = render (hostile, static_cast<int> (sampleRate * 0.3));
    hostile.noteOn (72, notANumber);
    const auto afterBadVelocity = render (hostile, blockSize);
    expect (poisonedRender.finite && poisonedRender.peak < 16.0,
            "non-finite parameters produced invalid audio");
    expect (afterBadVelocity.finite, "a non-finite velocity produced invalid audio");
}

void testParameterSmoothingHasNoZipper()
{
    constexpr auto sampleRate = 48000.0;
    constexpr float quietGain = 0.05f;
    constexpr float loudGain = 1.60f;
    const auto settleSamples = static_cast<int> (sampleRate * 0.5);
    const auto observeSamples = static_cast<int> (sampleRate * 0.4);

    // Output gain is the last thing the engine applies and feeds back into
    // nothing, so dividing a jumped render by an otherwise identical constant
    // render recovers the smoother trajectory sample by sample.
    const auto renderWithGain = [&] (bool jump)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.outputGain = quietGain;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.85f);
        render (engine, settleSamples);
        if (jump)
        {
            parameters.outputGain = loudGain;
            engine.setParameters (parameters);
        }
        return renderMono (engine, observeSamples);
    };

    const auto steady = renderWithGain (false);
    const auto jumped = renderWithGain (true);

    float previousRatio = 1.0f;
    int previousIndex = -1;
    float worstSlope = 0.0f;
    float finalRatio = 1.0f;
    bool monotonic = true;
    int observations = 0;

    for (std::size_t i = 0; i < steady.size(); ++i)
    {
        if (std::abs (steady[i]) < 1.0e-3f)
            continue;
        const auto ratio = jumped[i] / steady[i];
        if (previousIndex >= 0)
        {
            const auto distance = static_cast<float> (static_cast<int> (i) - previousIndex);
            worstSlope = std::max (worstSlope, (ratio - previousRatio) / distance);
            monotonic = monotonic && ratio > previousRatio - 0.02f;
        }
        previousRatio = ratio;
        previousIndex = static_cast<int> (i);
        finalRatio = ratio;
        ++observations;
    }

    expect (observations > 200, "the smoothing probe did not find enough usable samples");
    expect (monotonic, "the output-gain smoother reversed direction mid-glide");
    // A 25 ms one-pole moving 0.05 -> 1.6 cannot climb faster than about
    // 0.026 gain ratio units per sample. A stepped parameter would be ~31.
    expect (worstSlope < 0.06f,
            "the output gain stepped instead of gliding to its new value");
    expect (std::abs (finalRatio - loudGain / quietGain) < loudGain / quietGain * 0.02f,
            "the output-gain smoother did not reach its new target");

    // The remaining smoothed parameters have to converge too.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto quiet = makeParameters (0, 0, 0, 0);
    quiet.breath = 0.0f;
    quiet.tension = 0.0f;
    quiet.room = 0.0f;
    quiet.outputGain = 0.05f;
    engine.setParameters (quiet);
    engine.noteOn (60, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.3));

    auto loud = quiet;
    loud.breath = 1.0f;
    loud.tension = 1.0f;
    loud.room = 1.0f;
    loud.outputGain = 1.6f;
    engine.setParameters (loud);
    const auto glide = render (engine, static_cast<int> (sampleRate * 0.5));
    expect (glide.finite && glide.peak < 16.0,
            "a full-range parameter jump produced invalid audio");

    const auto smoothed = vocalor::VoiceEngineTestAccess::smoothedParameters (engine);
    expect (std::abs (smoothed[0] - loud.room) < 0.01f
                && std::abs (smoothed[1] - loud.outputGain) < 0.01f
                && std::abs (smoothed[2] - loud.breath) < 0.01f
                && std::abs (smoothed[3] - loud.tension) < 0.01f,
            "the parameter smoothers did not converge on their new targets");
}

void testDisplayStateTracksTheEngine()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();

    auto idleState = engine.getDisplayState();
    expect (idleState.formantHz[0] > 100.0f && idleState.formantHz[0] < 2000.0f,
            "the display state had no usable tract before the first note");
    expect (idleState.levelLeft == 0.0f && idleState.levelRight == 0.0f,
            "the display meters were not silent while the engine was idle");
    expect (std::abs (idleState.sampleRate - 48000.0f) < 1.0f,
            "the display state reported the wrong sample rate");

    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.vowelMorph = 1.0f;
    parameters.vowelX = 1.0f;
    parameters.vowelY = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (62, 0.9f);
    render (engine, static_cast<int> (sampleRate * 0.4));

    const auto state = engine.getDisplayState();
    expect (state.activeVoices > 0, "the display state lost the active voice count");
    expect (state.levelLeft > 0.0f && state.levelRight > 0.0f,
            "the display meters stayed at zero while a note was sounding");
    expect (state.formantHz[1] > 2000.0f,
            "the display did not follow the morphed F2");
    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        expect (state.formantBandwidth[index] > 10.0f && state.formantBandwidth[index] < 2000.0f,
                "the published formant bandwidth is out of range");
        expect (state.formantGain[index] > 0.0f && state.formantGain[index] < 4.0f,
                "the published formant gain is out of range");
    }
    const auto runningTract = vocalor::VoiceEngineTestAccess::voiceTract(engine, 62);
    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        expect(std::abs(state.formantHz[index] - runningTract.hz[index]) < 0.01f
                   && std::abs(state.formantBandwidth[index]
                               - runningTract.bandwidth[index]) < 0.01f
                   && std::abs(state.formantGain[index] - runningTract.gain[index]) < 1.0e-5f,
               "the display combined frequency, bandwidth and gain from different tracts");
    }
    const auto corner = vocalor::cardinalVowelPosition (0);
    expect (std::abs (state.vowelX - corner.x) < 0.02f && std::abs (state.vowelY - corner.y) < 0.02f,
            "the published vowel position did not follow a full morph");

    // The response curve of the published tract has to peak on F1.
    const auto atFormant = vocalor::formantResponseDb (
        state.formantHz[0], state.formantHz.data(), state.formantBandwidth.data(),
        state.formantGain.data(), vocalor::kFormantCount, state.sampleRate);
    const auto belowFormant = vocalor::formantResponseDb (
        state.formantHz[0] * 0.35f, state.formantHz.data(), state.formantBandwidth.data(),
        state.formantGain.data(), vocalor::kFormantCount, state.sampleRate);
    expect (atFormant > belowFormant + 3.0f,
            "the published tract does not describe a resonant response");

    engine.allSoundOff();
    const auto stopped = engine.getDisplayState();
    expect (stopped.levelLeft == 0.0f && stopped.levelRight == 0.0f && stopped.activeVoices == 0,
            "all-sound-off did not reset the display meters");
}

/** RMS of a steady note, in dB, with everything stochastic switched off. */
double steadyLevelDb (double sampleRate, const vocalor::EngineParameters& parameters,
                      int midiNote)
{
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (parameters);
    engine.noteOn (midiNote, 0.8f);
    render (engine, static_cast<int> (sampleRate * 0.4));
    const auto steady = render (engine, static_cast<int> (sampleRate * 0.6));
    return 20.0 * std::log10 (std::max (steady.rms(), 1.0e-12));
}

/** Magnitude of one harmonic of @c fundamental, by direct evaluation. */
double harmonicMagnitude (const std::vector<float>& samples, double frequency, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const auto angle = 2.0 * 3.14159265358979323846 * frequency
                         * static_cast<double> (i) / sampleRate;
        real += samples[i] * std::cos (angle);
        imaginary -= samples[i] * std::sin (angle);
    }
    return std::sqrt (real * real + imaginary * imaginary) * 2.0
         / std::max (static_cast<double> (samples.size()), 1.0);
}

/** Harmonics of one exact glottal-table cycle, before the source shelves and
    vocal tract. The first 24 are the calibration band used by buildTables(). */
struct GlottalSourceSpectrum
{
    static constexpr int harmonicCount = 24;
    std::array<double, harmonicCount + 1> magnitude {};
    double cycleRms = 0.0;
    double bandRms = 0.0;
};

GlottalSourceSpectrum glottalSourceSpectrum(const std::vector<float>& cycle)
{
    GlottalSourceSpectrum result;
    double square = 0.0;
    for (const float sample : cycle)
        square += static_cast<double>(sample) * static_cast<double>(sample);
    result.cycleRms = std::sqrt(square
        / std::max(static_cast<double>(cycle.size()), 1.0));

    double bandSquare = 0.0;
    const double cycleRate = static_cast<double>(cycle.size());
    for (int harmonic = 1; harmonic <= GlottalSourceSpectrum::harmonicCount; ++harmonic)
    {
        const double magnitude = harmonicMagnitude(
            cycle, static_cast<double>(harmonic), cycleRate);
        result.magnitude[static_cast<std::size_t>(harmonic)] = magnitude;
        bandSquare += 0.5 * magnitude * magnitude;
    }
    result.bandRms = std::sqrt(bandSquare);
    return result;
}

vocalor::EngineParameters steadyParameters()
{
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.0f;
    parameters.instability = 0.0f;
    parameters.legacyDriftBypass = false;
    parameters.room = 0.0f;
    return parameters;
}

/** Nothing about the sound may depend on the sample rate.

    Before the formant bank was peak-normalised, each resonator's gain was
    proportional to the sample rate: the same patch measured 12.7 dB louder at
    192 kHz than at 44.1 kHz, harmonic for harmonic.
*/
void testSampleRateInvariance()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    constexpr double fundamental = 261.6255653;

    double quietest = 1.0e9;
    double loudest = -1.0e9;
    std::array<double, 4> lowest { 1.0e9, 1.0e9, 1.0e9, 1.0e9 };
    std::array<double, 4> highest { -1.0e9, -1.0e9, -1.0e9, -1.0e9 };
    constexpr std::array harmonics { 1, 2, 4, 8 };

    for (const auto sampleRate : sampleRates)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.8f);
        renderMono (engine, static_cast<int> (sampleRate * 0.6));
        const auto steady = renderMono (engine, static_cast<int> (sampleRate * 0.6));

        double sumOfSquares = 0.0;
        for (const auto value : steady)
            sumOfSquares += static_cast<double> (value) * value;
        const auto levelDb = 20.0 * std::log10 (std::max (
            std::sqrt (sumOfSquares / static_cast<double> (steady.size())), 1.0e-12));
        quietest = std::min (quietest, levelDb);
        loudest = std::max (loudest, levelDb);

        for (std::size_t h = 0; h < harmonics.size(); ++h)
        {
            const auto magnitude = 20.0 * std::log10 (std::max (
                harmonicMagnitude (steady, fundamental * harmonics[h], sampleRate), 1.0e-12));
            lowest[h] = std::min (lowest[h], magnitude);
            highest[h] = std::max (highest[h], magnitude);
        }
    }

    expect (loudest - quietest < 0.6,
            "the rendered level still depends on the sample rate");
    for (std::size_t h = 0; h < harmonics.size(); ++h)
        expect (highest[h] - lowest[h] < 1.5,
                "harmonic " + std::to_string (harmonics[h])
                    + " changed level across the supported sample rates");
}

/** Humanisation must have the same depth at every sample rate, not merely the
    same spectrum.

    The shimmer and the first pitch-jitter smoother are one-poles driven by
    white noise. Deriving their coefficients from a corner frequency and a time
    constant fixed the spectrum, but a noise-driven one-pole settles at output
    variance c / (2 - c), so with c now proportional to 1/sampleRate the depth
    fell as 1/sqrt(sampleRate) unless the drive is renormalised: measured
    shimmer standard deviation 0.0304 / 0.0228 / 0.0158 at 48 / 96 / 192 kHz
    before the compensation went in. testSampleRateInvariance structurally
    cannot see this, because its patch sets humanize = 0, which zeroes both the
    shimmer depth and the jitter's contribution to the pitch.

    Both halves are asserted here: the standard deviation is the depth, and the
    autocorrelation at a fixed lag in seconds is the spectrum.
*/
void testHumanisationDepthIsRateInvariant()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr double windowSeconds = 6.0;
    // The observation stride is a duration, not a sample count, so the lag of
    // the autocorrelation below means the same thing at every rate.
    constexpr double strideSeconds = 0.004;
    constexpr std::array names { "shimmer amplitude modulation",
                                 "pitch jitter" };

    std::array<double, names.size()> quietest {};
    std::array<double, names.size()> loudest {};
    std::array<double, names.size()> slowest {};
    std::array<double, names.size()> fastest {};
    quietest.fill (1.0e9);
    loudest.fill (-1.0e9);
    slowest.fill (-1.0e9);
    fastest.fill (1.0e9);

    for (const auto sampleRate : sampleRates)
    {
        auto parameters = steadyParameters();
        parameters.humanize = 1.0f;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, static_cast<int> (sampleRate * 0.5));

        const auto stride = std::max (1, static_cast<int> (sampleRate * strideSeconds));
        const auto observations = static_cast<int> (windowSeconds / strideSeconds);
        std::array<std::vector<double>, names.size()> series;
        for (int i = 0; i < observations; ++i)
        {
            render (engine, stride);
            const auto depth = vocalor::VoiceEngineTestAccess::humanisationDepth (engine);
            for (std::size_t k = 0; k < names.size(); ++k)
                series[k].push_back (static_cast<double> (depth[k]));
        }

        for (std::size_t k = 0; k < names.size(); ++k)
        {
            double sumOfSquares = 0.0;
            double lagProduct = 0.0;
            for (std::size_t i = 0; i < series[k].size(); ++i)
            {
                sumOfSquares += series[k][i] * series[k][i];
                if (i > 0)
                    lagProduct += series[k][i] * series[k][i - 1];
            }
            const auto deviation = std::sqrt (sumOfSquares
                                              / static_cast<double> (series[k].size()));
            expect (deviation > 1.0e-5,
                    std::string (names[k]) + " is not running at all");
            quietest[k] = std::min (quietest[k], deviation);
            loudest[k] = std::max (loudest[k], deviation);

            const auto correlation = lagProduct / std::max (sumOfSquares, 1.0e-30);
            slowest[k] = std::max (slowest[k], correlation);
            fastest[k] = std::min (fastest[k], correlation);
        }
    }

    for (std::size_t k = 0; k < names.size(); ++k)
    {
        // Depth. Without the drive compensation this measured 6.5 dB for the
        // shimmer and 6.2 dB for the jitter; with it, under 0.6 dB. Two leaves
        // room for the sampling error of a six-second window and nothing else.
        expect (20.0 * std::log10 (loudest[k] / std::max (quietest[k], 1.0e-12)) < 2.0,
                std::string (names[k]) + " depth still depends on the sample rate");
        // Spectrum. A fixed 4 ms lag: if a smoother's corner moved with the
        // sample rate, so would this. With the old per-sample and per-control-
        // period coefficients the shimmer measured 0.33 / 0.31 / 0.09 / 0.003
        // and the jitter 0.88 / 0.86 / 0.69 / 0.39 across the four rates; both
        // now hold to within 0.025.
        expect (slowest[k] - fastest[k] < 0.06,
                std::string (names[k]) + " spectrum still depends on the sample rate");
    }
}

/** The vowel pad and the formant shift are timbre controls, not faders. */
void testTractLevelStability()
{
    constexpr auto sampleRate = 48000.0;
    constexpr std::array padX { 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f };
    constexpr std::array padY { 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f };

    double quietest = 1.0e9;
    double loudest = -1.0e9;
    for (std::size_t i = 0; i < padX.size(); ++i)
    {
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = padX[i];
        parameters.vowelY = padY[i];
        const auto level = steadyLevelDb (sampleRate, parameters, 60);
        quietest = std::min (quietest, level);
        loudest = std::max (loudest, level);
    }
    // 15.9 dB before the bank was normalised. What is left is the genuine
    // interaction between the fundamental and where F1 happens to sit.
    expect (loudest - quietest < 10.0,
            "the vowel pad still works as a volume control");

    quietest = 1.0e9;
    loudest = -1.0e9;
    for (int semitones = -12; semitones <= 12; semitones += 3)
    {
        auto parameters = steadyParameters();
        parameters.formantShift = static_cast<float> (semitones);
        const auto level = steadyLevelDb (sampleRate, parameters, 60);
        quietest = std::min (quietest, level);
        loudest = std::max (loudest, level);
    }
    expect (loudest - quietest < 13.0,
            "the formant shift still changes the level more than the timbre");

    // The old bank's gain fell monotonically as the formants rose, so shifting
    // an octave up cost 17.5 dB. Shifting must no longer have a level trend.
    auto down = steadyParameters();
    down.formantShift = -12.0f;
    auto up = steadyParameters();
    up.formantShift = 12.0f;
    const auto downLevel = steadyLevelDb (sampleRate, down, 60);
    const auto upLevel = steadyLevelDb (sampleRate, up, 60);
    std::cout << "formant-shift endpoints: " << downLevel << " / "
              << upLevel << " dB\n";
    expect (std::abs (downLevel - upLevel) < 6.0,
            "shifting the formants up an octave still acts as a fader");
}

/** The parallel formant bank has to behave like an all-pole vocal tract. */
void testParallelFormantBank()
{
    // Peak normalisation: the resonator's gain at its own centre frequency must
    // be one for every bandwidth, centre frequency and sample rate.
    for (const float sampleRate : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        for (const float centre : { 60.0f, 310.0f, 850.0f, 2790.0f, 0.4f * sampleRate })
        {
            for (const float bandwidth : { 20.0f, 75.0f, 260.0f, 700.0f })
            {
                // The reference is evaluated in double on purpose: at 192 kHz
                // 1 - a1 cos - a2 cos2 is a difference of near-equal ones and a
                // float reference would be noisier than the value under test.
                const auto radius = static_cast<double> (
                    std::exp (-3.14159265358979323846f * bandwidth / sampleRate));
                const auto omega = 2.0 * 3.14159265358979323846
                                 * static_cast<double> (centre) / sampleRate;
                const auto a1 = 2.0 * radius * std::cos (omega);
                const auto a2 = -radius * radius;
                const auto b0 = static_cast<double> (vocalor::formantResonatorGain (
                    static_cast<float> (radius), static_cast<float> (std::sin (omega))));

                const auto real = 1.0 - a1 * std::cos (omega) - a2 * std::cos (2.0 * omega);
                const auto imaginary = a1 * std::sin (omega) + a2 * std::sin (2.0 * omega);
                const auto gain = b0 / std::sqrt (real * real + imaginary * imaginary);
                expect (std::abs (gain - 1.0) < 2.0e-3,
                        "the formant resonator was not normalised to unit peak gain");
            }
        }
    }

    expect (vocalor::formantPolarity (0) > 0.0f && vocalor::formantPolarity (1) < 0.0f
                && vocalor::formantPolarity (2) > 0.0f,
            "the parallel formant bank stopped alternating its polarity");

    // Cascade-derived amplitudes: finite, ordered, and vowel dependent.
    const std::array<float, vocalor::kFormantCount> bandwidth {
        75.0f, 90.0f, 125.0f, 185.0f, 260.0f };
    std::array<float, vocalor::kFormantCount> openGain {};
    std::array<float, vocalor::kFormantCount> closeGain {};
    const std::array<float, vocalor::kFormantCount> openHz {
        850.0f, 1220.0f, 2810.0f, 3650.0f, 4950.0f };
    const std::array<float, vocalor::kFormantCount> closeHz {
        310.0f, 2790.0f, 3310.0f, 3900.0f, 4950.0f };
    vocalor::parallelFormantAmplitudes (openHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, openGain.data());
    vocalor::parallelFormantAmplitudes (closeHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, closeGain.data());

    bool finite = true;
    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        finite = finite && std::isfinite (openGain[static_cast<std::size_t> (i)])
                        && std::isfinite (closeGain[static_cast<std::size_t> (i)]);
        expect (openGain[static_cast<std::size_t> (i)] > 0.0f,
                "a cascade-derived formant amplitude was not positive");
    }
    expect (finite, "the cascade-derived formant amplitudes were not finite");

    // parallelFormantAmplitudes() shares parallelFormantCoefficients()'s guard
    // clause; an invalid formant bank must leave the caller's buffer untouched
    // instead of writing through a null pointer or resolving against a
    // non-positive count or sample rate.
    std::array<float, vocalor::kFormantCount> guardGain { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    vocalor::parallelFormantAmplitudes (nullptr, bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, guardGain.data());
    expect (guardGain[0] == -1.0f,
            "parallelFormantAmplitudes wrote its output with a null formant-Hz pointer");
    vocalor::parallelFormantAmplitudes (openHz.data(), bandwidth.data(), 0, 48000.0f, 0.010f,
                                        guardGain.data());
    expect (guardGain[0] == -1.0f,
            "parallelFormantAmplitudes wrote its output with a non-positive formant count");
    vocalor::parallelFormantAmplitudes (openHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        0.0f, 0.010f, guardGain.data());
    expect (guardGain[0] == -1.0f,
            "parallelFormantAmplitudes wrote its output with a non-positive sample rate");
    // The formant-Hz null check above never reaches the outGain/bandwidth
    // guard terms; isolate them too so a regression dropping either from the
    // condition would still be caught.
    vocalor::parallelFormantAmplitudes (openHz.data(), nullptr, vocalor::kFormantCount,
                                        48000.0f, 0.010f, guardGain.data());
    expect (guardGain[0] == -1.0f,
            "parallelFormantAmplitudes wrote its output with a null bandwidth pointer");
    // outGain is the only output; a null one has nothing to check but must not
    // crash, so reaching the following line is the assertion.
    vocalor::parallelFormantAmplitudes (openHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, nullptr);

    // /a/ concentrates its energy in F1 and F2; /i/ carries F2 and F3 nearly as
    // strongly as F1. If the amplitudes did not track the vowel this would not
    // hold, and a front vowel would not sound front.
    expect (openGain[2] / openGain[0] < closeGain[2] / closeGain[0] * 0.5f,
            "the formant amplitudes no longer follow the vowel");

    // Nothing may starve completely: the bank models nothing above F5.
    const auto largest = *std::max_element (openGain.begin(), openGain.end());
    for (const auto value : openGain)
        expect (value >= largest * 0.0099f,
                "a formant amplitude fell below the modelled floor");

    // And the valley between F1 and F2 must stay within reach of the peak. A
    // bank summed with a common sign digs a 64 dB notch there on a close front
    // vowel, tens of dB deeper than any vocal tract produces.
    // The two-formant onset stage this block used to check for a matching
    // normalisation no longer exists: every voice renders the whole bank from
    // its first sample. testOnsetSpectrum is what holds that now.

    // Limits are the pre-change depths rounded down: /i/-like 64.2, /a/-like
    // 24.9 and /u/-like 29.4 dB. The current bank measures 32.2, 9.6 and
    // 13.7 dB at the same three corners.
    struct Corner { float x; float y; double limit; };
    for (const auto corner : { Corner { 1.0f, 0.0f, 42.0 }, Corner { 0.5f, 1.0f, 16.0 },
                               Corner { 0.0f, 0.0f, 20.0 } })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (48000.0, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = corner.x;
        parameters.vowelY = corner.y;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, static_cast<int> (48000.0 * 0.5));

        const auto state = engine.getDisplayState();
        const auto response = [&state] (double hz)
        {
            return static_cast<double> (vocalor::formantResponseDb (
                static_cast<float> (hz), state.formantHz.data(), state.formantBandwidth.data(),
                state.formantGain.data(), vocalor::kFormantCount, state.sampleRate));
        };

        double peak = -1.0e9;
        for (double hz = 80.0; hz < 11000.0; hz *= 1.002)
            peak = std::max (peak, response (hz));
        double valley = 1.0e9;
        for (double hz = state.formantHz[0] * 1.08; hz < state.formantHz[1] * 0.92; hz *= 1.002)
            valley = std::min (valley, response (hz));

        expect (peak - valley < corner.limit,
                "the formant bank cancelled itself between F1 and F2");
    }
}

/** Ensemble size has to mean what it says: every value in its range renders
    exactly that many independently humanised singers. */
void testEnsembleSizeIsExact()
{
    constexpr auto sampleRate = 48000.0;
    for (int singers = 2; singers <= 12; ++singers)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (1, 0, 0, 0);
        parameters.choirSize = singers;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, blockSize);
        expect (engine.getActiveVoiceCount() == singers,
                "an ensemble of " + std::to_string (singers)
                    + " did not render that many singers");
    }

    // Out-of-range requests still land on the nearest usable ensemble.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.choirSize = 40;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.8f);
    render (engine, blockSize);
    expect (engine.getActiveVoiceCount() == 12,
            "an oversized ensemble request was not clamped to the singers available");
}

/** Resonance and formant shift reach the pole radius, which cannot be smoothed
    downstream, so a jump on either has to be smoothed before it is used. */
void testTractCoefficientSmoothing()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = steadyParameters();
    parameters.resonance = 0.0f;
    parameters.formantShift = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.3));

    const auto before = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);
    parameters.resonance = 1.0f;
    parameters.formantShift = 12.0f;
    engine.setParameters (parameters);

    // One chunk of a 20 ms smoother may cover only a small part of the jump.
    render (engine, 64);
    const auto afterOneChunk = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);
    render (engine, static_cast<int> (sampleRate * 0.3));
    const auto settled = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);

    const auto span = std::abs (settled[0] - before[0]);
    expect (span > 1.0f, "the resonance and shift jump did not move the bandwidths at all");
    expect (std::abs (afterOneChunk[0] - before[0]) < span * 0.25f,
            "the tract bandwidth stepped instead of gliding after a parameter jump");
    expect (std::abs (afterOneChunk[0] - before[0]) > 0.0f,
            "the tract bandwidth froze instead of gliding after a parameter jump");
}

void testRoughPerformance()
{
    constexpr auto sampleRate = 96000.0;
    constexpr auto secondsToRender = 1.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();

    auto parameters = makeParameters (1, 0, 1, 1);
    parameters.choirSize = 12;
    engine.setParameters (parameters);
    engine.noteOn (57, 0.85f);

    // Warm the caches so the measurement reports steady-state cost.
    render (engine, blockSize * 8);

    const auto start = std::chrono::steady_clock::now();
    const auto metrics = render (engine, static_cast<int> (sampleRate * secondsToRender));
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto realTimeRatio = elapsed / secondsToRender;
    const auto nanosecondsPerSample = elapsed * 1.0e9 / (sampleRate * secondsToRender);

    std::cout << std::fixed << std::setprecision (3)
              << "12-singer 96 kHz render: " << elapsed << " s ("
              << realTimeRatio << "x real time, "
              << std::setprecision (1) << nanosecondsPerSample << " ns/sample)\n";

    expect (metrics.finite && metrics.rms() > 1.0e-6,
            "performance render did not produce valid, non-silent audio");
    // Deliberately generous: this is a regression tripwire that also tolerates
    // unoptimised CI builds, not a release-mode real-time certification. The
    // 1.1 engine measures roughly 0.35x here against 0.55x for 1.0.
    expect (realTimeRatio < 20.0,
            "12-singer render exceeded the 20x-real-time regression guardrail");
}

/** Reference settings for the level and articulation measurements below: the
    factory defaults with everything that randomises a take switched off, so the
    render is deterministic and the output gain is out of the way. */
vocalor::EngineParameters makeReferenceParameters()
{
    vocalor::EngineParameters parameters;
    parameters.humanize = 0.0f;
    parameters.vibrato = 0.0f;
    parameters.spread = 0.0f;
    parameters.room = 0.0f;
    parameters.outputGain = 1.0f;
    return parameters;
}

/** Source-table construction is deliberately outside the real-time API.

    A host that violates the prepare-before-audio contract must get silence,
    not a hidden heap allocation and a multi-millisecond table build in its
    callback. Once prepared, the same engine must accept notes normally. */
void testPreparationIsExplicit()
{
    vocalor::VoiceEngine engine;
    std::array<float, 64> left {};
    std::array<float, 64> right {};
    left.fill(1.0f);
    right.fill(-1.0f);
    engine.noteOn(60, 0.8f);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    expect(engine.getActiveVoiceCount() == 0
               && std::all_of(left.begin(), left.end(), [](float value)
                              { return value == 0.0f; })
               && std::all_of(right.begin(), right.end(), [](float value)
                              { return value == 0.0f; }),
           "an unprepared engine allocated/rendered instead of returning silence");

    engine.prepare(48000.0, blockSize);
    engine.reset();
    engine.noteOn(60, 0.8f);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    expect(engine.getActiveVoiceCount() == 1,
           "an explicitly prepared engine did not accept its first note");
}

/** Tension is a physical change of the LF source, not a crossfade between two
    unrelated recordings of a pulse. The lax and pressed endpoint tables have
    different closure phases; mixing only those endpoints used to cancel H2 by
    19.6 dB and H5 by 9.1 dB at intermediate settings even though neither
    physical endpoint contained the notch. Probe the source before the tract so
    no formant can fill that hole and make the regression pass accidentally. */
void testGlottalSourceTensionBank()
{
    constexpr auto level = vocalor::VoiceEngineTestAccess::fullestGlottalTableLevel;
    constexpr std::array<double, 6> expectedLax {
        0.0, 0.70240, 0.29779, 0.24195, 0.14807, 0.07937
    };
    constexpr std::array<double, 6> expectedPressed {
        0.0, 0.30246, 0.35838, 0.22130, 0.18155, 0.16822
    };

    auto reference = std::make_unique<vocalor::VoiceEngine>();
    reference->prepare(48000.0, blockSize);
    reference->reset();
    const auto laxCycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
        *reference, level, 0.0f);
    const auto pressedCycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
        *reference, level, 1.0f);
    const auto lax = glottalSourceSpectrum(laxCycle);
    const auto pressed = glottalSourceSpectrum(pressedCycle);

    // The physical endpoints and their historical 1..24-harmonic calibration
    // are intentionally unchanged by adding the shapes between them.
    expect(std::abs(lax.bandRms - 0.587) < 0.002
               && std::abs(pressed.bandRms - 0.441) < 0.002,
           "the glottal source bank moved its calibrated endpoint band levels");
    expect(std::abs(lax.cycleRms - 0.58733) < 0.003
               && std::abs(pressed.cycleRms - 0.44409) < 0.003,
           "the glottal source bank moved its full-cycle endpoint levels");
    for (int harmonic = 1; harmonic <= 5; ++harmonic)
    {
        const auto index = static_cast<std::size_t>(harmonic);
        expect(std::abs(lax.magnitude[index] - expectedLax[index]) < 0.002
                   && std::abs(pressed.magnitude[index] - expectedPressed[index]) < 0.002,
               "the glottal source bank changed endpoint H"
                   + std::to_string(harmonic));
    }

    std::array<double, 6> minimumRelativeToEndpoints {};
    minimumRelativeToEndpoints.fill(std::numeric_limits<double>::infinity());
    double largestBandErrorDb = 0.0;
    double largestLowHarmonicStepDb = 0.0;
    double largestCycleStep = 0.0;
    auto previousCycle = laxCycle;
    auto previousSpectrum = lax;
    // Mean product of the former, closure-unaligned lax and pressed endpoint
    // spectra over H1-H24. Together with the two endpoint energies this defines
    // the exact quadratic RMS curve that existing Tension settings rendered;
    // preserving it keeps session loudness while the physical shape bank fixes
    // the individual harmonic holes.
    constexpr double legacyEndpointBandCross = 0.00206635;
    constexpr int sweepSteps = 256;
    for (int step = 0; step <= sweepSteps; ++step)
    {
        const float tension = static_cast<float>(step)
                            / static_cast<float>(sweepSteps);
        const auto cycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
            *reference, level, tension);
        const auto spectrum = glottalSourceSpectrum(cycle);
        const double pressedAmount = static_cast<double>(tension);
        const double laxAmount = 1.0 - pressedAmount;
        const double targetBandRms = std::sqrt(
            laxAmount * laxAmount * 0.587 * 0.587
            + pressedAmount * pressedAmount * 0.441 * 0.441
            + 2.0 * laxAmount * pressedAmount * legacyEndpointBandCross);
        largestBandErrorDb = std::max(largestBandErrorDb, std::abs(
            20.0 * std::log10(std::max(spectrum.bandRms, 1.0e-30)
                              / targetBandRms)));

        for (int harmonic = 1; harmonic <= 5; ++harmonic)
        {
            const auto index = static_cast<std::size_t>(harmonic);
            const double endpointFloor = std::min(lax.magnitude[index],
                                                  pressed.magnitude[index]);
            minimumRelativeToEndpoints[index] = std::min(
                minimumRelativeToEndpoints[index],
                spectrum.magnitude[index] / std::max(endpointFloor, 1.0e-30));
            if (step > 0)
            {
                largestLowHarmonicStepDb = std::max(largestLowHarmonicStepDb,
                    std::abs(20.0 * std::log10(
                        std::max(spectrum.magnitude[index], 1.0e-30)
                        / std::max(previousSpectrum.magnitude[index], 1.0e-30))));
            }
        }

        if (step > 0)
        {
            double deltaSquare = 0.0;
            for (std::size_t sample = 0; sample < cycle.size(); ++sample)
            {
                const double delta = static_cast<double>(cycle[sample])
                                   - static_cast<double>(previousCycle[sample]);
                deltaSquare += delta * delta;
            }
            const double deltaRms = std::sqrt(deltaSquare
                / std::max(static_cast<double>(cycle.size()), 1.0));
            largestCycleStep = std::max(largestCycleStep,
                deltaRms / std::max(previousSpectrum.cycleRms, 1.0e-30));
        }
        previousCycle = cycle;
        previousSpectrum = spectrum;
    }

    std::cout << "glottal source morph: max legacy 1-24 RMS error " << std::fixed
              << std::setprecision(4) << largestBandErrorDb << " dB, H1-H5 minima ";
    for (int harmonic = 1; harmonic <= 5; ++harmonic)
        std::cout << (harmonic == 1 ? "" : "/")
                  << minimumRelativeToEndpoints[static_cast<std::size_t>(harmonic)];
    std::cout << ", max 1/256-tension harmonic/cycle step "
              << largestLowHarmonicStepDb << " dB/" << largestCycleStep << "\n";

    // This is level calibration, not a demand for flat harmonics. Each partial
    // keeps its own endpoint trajectory; the test only rejects a deep notch
    // below both physical endpoints. The former H2/H5 ratios were 0.104/0.350.
    for (int harmonic = 1; harmonic <= 5; ++harmonic)
        expect(minimumRelativeToEndpoints[static_cast<std::size_t>(harmonic)] > 0.55,
               "the glottal tension morph destructively cancelled H"
                   + std::to_string(harmonic));
    expect(largestBandErrorDb < 0.03,
           "the glottal tension morph changed the legacy 1-24-harmonic RMS curve");
    expect(largestLowHarmonicStepDb < 0.35 && largestCycleStep < 0.025,
           "the physical glottal shapes do not join smoothly across Tension");

    // The voice selects a different harmonic mip as pitch rises. Compensation
    // derived only from the 24-harmonic calibration band can be exact here yet
    // move a one- or two-harmonic soprano source by more than a decibel. Recover
    // the legacy pressed endpoint's pre-alignment phase, then require every mip
    // to preserve its own former endpoint-crossfade RMS curve.
    const auto coefficients = [](const std::vector<float>& cycle, int harmonics)
    {
        std::vector<std::pair<double, double>> result(
            static_cast<std::size_t>(harmonics + 1));
        const double count = static_cast<double>(cycle.size());
        for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
        {
            double cosine = 0.0;
            double sine = 0.0;
            for (std::size_t sample = 0; sample < cycle.size(); ++sample)
            {
                const double angle = 2.0 * 3.14159265358979323846
                    * static_cast<double>(harmonic)
                    * static_cast<double>(sample) / count;
                cosine += static_cast<double>(cycle[sample]) * std::cos(angle);
                sine += static_cast<double>(cycle[sample]) * std::sin(angle);
            }
            result[static_cast<std::size_t>(harmonic)] = {
                2.0 * cosine / count, 2.0 * sine / count
            };
        }
        return result;
    };
    const auto cycleRms = [](const std::vector<float>& cycle)
    {
        double square = 0.0;
        for (const float sample : cycle)
            square += static_cast<double>(sample) * static_cast<double>(sample);
        return std::sqrt(square / static_cast<double>(cycle.size()));
    };

    double largestMipRmsErrorDb = 0.0;
    for (int tableLevel = 0;
         tableLevel < vocalor::VoiceEngineTestAccess::glottalTableLevelCount;
         ++tableLevel)
    {
        const int harmonics = vocalor::VoiceEngineTestAccess::glottalHarmonicsForLevel(
            tableLevel);
        const auto levelLaxCycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
            *reference, tableLevel, 0.0f);
        const auto levelPressedCycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
            *reference, tableLevel, 1.0f);
        const auto levelLax = coefficients(levelLaxCycle, harmonics);
        const auto levelPressedAligned = coefficients(levelPressedCycle, harmonics);

        double laxEnergy = 0.0;
        double pressedEnergy = 0.0;
        double legacyCross = 0.0;
        for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
        {
            const auto index = static_cast<std::size_t>(harmonic);
            const auto [laxCosine, laxSine] = levelLax[index];
            const auto [alignedCosine, alignedSine] = levelPressedAligned[index];
            // The pressed shape was delayed by 0.78-0.46=0.32 cycle to put its
            // closure at the lax endpoint's phase. Rotate it back before taking
            // the old endpoint cross term.
            const double angle = 2.0 * 3.14159265358979323846
                               * static_cast<double>(harmonic) * 0.32;
            const double rotationCosine = std::cos(angle);
            const double rotationSine = std::sin(angle);
            const double legacyCosine = alignedCosine * rotationCosine
                                      + alignedSine * rotationSine;
            const double legacySine = -alignedCosine * rotationSine
                                    + alignedSine * rotationCosine;
            laxEnergy += 0.5 * (laxCosine * laxCosine + laxSine * laxSine);
            pressedEnergy += 0.5 * (legacyCosine * legacyCosine
                                  + legacySine * legacySine);
            legacyCross += 0.5 * (laxCosine * legacyCosine
                               + laxSine * legacySine);
        }

        for (int step = 0; step <= sweepSteps; ++step)
        {
            const double tension = static_cast<double>(step)
                                 / static_cast<double>(sweepSteps);
            const double laxAmount = 1.0 - tension;
            const double targetRms = std::sqrt(std::max(
                laxAmount * laxAmount * laxEnergy
                    + tension * tension * pressedEnergy
                    + 2.0 * laxAmount * tension * legacyCross,
                1.0e-30));
            const auto cycle = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
                *reference, tableLevel, static_cast<float>(tension));
            const double measuredRms = cycleRms(cycle);
            largestMipRmsErrorDb = std::max(largestMipRmsErrorDb,
                std::abs(20.0 * std::log10(
                    std::max(measuredRms, 1.0e-30) / targetRms)));
        }
    }
    std::cout << "glottal mip legacy-RMS error: " << std::fixed
              << std::setprecision(4) << largestMipRmsErrorDb << " dB\n";
    expect(largestMipRmsErrorDb < 0.03,
           "a pitch-selected glottal mip changed the legacy source drive");

    // The table is an oscillator property, not a sample-rate property. Every
    // engine deliberately shares the same immutable bank; prove that ownership
    // contract and require identical samples before any rate-dependent shelf or
    // tract rather than pretending each prepare rebuilt an independent table.
    constexpr std::array<float, 6> probeTensions {
        0.0f, 0.125f, 0.371f, 0.5f, 0.873f, 1.0f
    };
    constexpr std::array<int, 3> probeLevels { 0, 4, level };
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        auto rebuilt = std::make_unique<vocalor::VoiceEngine>();
        rebuilt->prepare(sampleRate, blockSize);
        rebuilt->reset();
        expect(vocalor::VoiceEngineTestAccess::tableBankIdentity(*rebuilt)
                   == vocalor::VoiceEngineTestAccess::tableBankIdentity(*reference),
               "VoiceEngine instances did not share the immutable source bank");
        for (const int probeLevel : probeLevels)
        {
            for (const float tension : probeTensions)
            {
                const auto expected = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
                    *reference, probeLevel, tension);
                const auto actual = vocalor::VoiceEngineTestAccess::glottalSourceCycle(
                    *rebuilt, probeLevel, tension);
                expect(actual == expected,
                       "the glottal source bank depends on sample rate or construction order");
            }
        }
    }
}

/** A singer does not drive every note with identical breath support when a
    sparse harmonic happens to land on a narrow tract resonance. The regulator
    returns half of that local radiated-power error, bounded to +/-3 dB and
    slowly enough that vibrato remains vibrato rather than an AGC detector.

    Exercise the public render as well as the control state: a plausible target
    is not useful if it fails to flatten the connected C4-C6 line, changes the
    spectrum, steps on a legato retune, or depends on sample rate/buffer cuts. */
void testRadiatedPowerRegulation()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 12> line {
        60, 62, 64, 67, 69, 72, 75, 77, 79, 80, 82, 84
    };
    constexpr float minimumGain = 0.70794578f;
    constexpr float maximumGain = 1.41253754f;

    auto parameters = makeReferenceParameters();
    parameters.profile = vocalor::VoiceProfile::Female;
    parameters.mode = vocalor::PerformanceMode::Solo;
    parameters.vowel = vocalor::Vowel::Aah;
    parameters.resonance = 0.42f;
    parameters.tension = 0.62f;
    parameters.breath = 0.0f;
    parameters.legato = true;
    parameters.glide = 0.16f;
    parameters.instability = 0.0f;

    struct Sweep
    {
        std::array<double, 12> levelDb {};
        std::array<std::array<float, 4>, 12> regulation {};
    };
    const auto sweep = [&parameters, &line](float depth)
    {
        Sweep result;
        vocalor::VoiceEngine engine;
        engine.prepare(sampleRate, blockSize);
        engine.reset();
        engine.setParameters(parameters);
        vocalor::VoiceEngineTestAccess::setRadiatedPowerDepth(engine, depth);
        // Keep the source spectrum stationary inside each level window. The
        // production onset gesture is covered by the onset tests; here it would
        // only obscure the tract/harmonic alignment under measurement.
        vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(engine, 0.0f);

        int sounding = -1;
        constexpr int stepSamples = static_cast<int>(0.52 * sampleRate);
        constexpr int levelSamples = static_cast<int>(0.18 * sampleRate);
        for (std::size_t step = 0; step < line.size(); ++step)
        {
            engine.noteOn(line[step], 0.74f);
            if (sounding >= 0)
                engine.noteOff(sounding);
            sounding = line[step];

            const auto audio = renderMono(engine, stepSamples);
            double square = 0.0;
            for (int sample = stepSamples - levelSamples;
                 sample < stepSamples; ++sample)
            {
                const double value = audio[static_cast<std::size_t>(sample)];
                square += value * value;
            }
            result.levelDb[step] = 10.0 * std::log10(
                std::max(square / static_cast<double>(levelSamples), 1.0e-30));
            result.regulation[step]
                = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
                    engine, line[step]);
        }
        return result;
    };

    const auto unregulated = sweep(0.0f);
    const auto regulated = sweep(1.0f);
    const auto span = [](const auto& values)
    {
        const auto [minimum, maximum] = std::minmax_element(
            values.begin(), values.end());
        return *maximum - *minimum;
    };
    const auto maximumAdjacent = [](const auto& values)
    {
        double largest = 0.0;
        for (std::size_t index = 1; index < values.size(); ++index)
            largest = std::max(largest,
                std::abs(values[index] - values[index - 1]));
        return largest;
    };
    const double rawSpan = span(unregulated.levelDb);
    const double regulatedSpan = span(regulated.levelDb);
    const double regulatedAdjacent = maximumAdjacent(regulated.levelDb);
    std::cout << "radiated-power C4-C6 level span: " << std::fixed
              << std::setprecision(2) << rawSpan << " -> " << regulatedSpan
              << " dB, regulated max adjacent " << regulatedAdjacent << " dB\n";
    expect(rawSpan > 9.0,
           "the unregulated soprano fixture no longer exposes harmonic/tract level jumps");
    expect(regulatedSpan <= 7.0,
           "bounded breath support left more than 7 dB across the C4-C6 line");
    expect(regulatedAdjacent <= 5.1,
           "bounded breath support left an adjacent register jump above 5.1 dB");
    expect(rawSpan - regulatedSpan >= 3.0,
           "the radiated-power law did not materially reduce the register plateau span");

    // Production state must obey the same +/-3 dB bound as the analytical
    // target and settle onto it after each half-second step.
    for (std::size_t step = 0; step < line.size(); ++step)
    {
        const auto state = regulated.regulation[step];
        for (const int field : { 0, 2, 3 })
            expect(state[static_cast<std::size_t>(field)] >= minimumGain - 2.0e-5f
                       && state[static_cast<std::size_t>(field)] <= maximumGain + 2.0e-5f,
                   "radiated-power gain escaped its +/-3 dB bound on MIDI "
                       + std::to_string(line[step]));
        expect(std::abs(state[0] - state[2]) < 2.0e-3f
                   && std::abs(state[2] - state[3]) < 3.0e-3f,
               "radiated-power gain did not settle onto its target on MIDI "
                   + std::to_string(line[step]));
    }

    // With aspiration removed the production A/B differs by one voiced-source
    // scalar. Least-squares-normalise the complete steady waveform rather than
    // checking a few bins, so any spectral or phase change leaves a residual.
    auto scalarParameters = parameters;
    scalarParameters.legato = false;
    scalarParameters.glide = 0.0f;
    auto unregulatedEngine = std::make_unique<vocalor::VoiceEngine>();
    auto regulatedEngine = std::make_unique<vocalor::VoiceEngine>();
    for (auto* engine : { unregulatedEngine.get(), regulatedEngine.get() })
    {
        engine->prepare(sampleRate, blockSize);
        engine->reset();
        engine->setParameters(scalarParameters);
        vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(*engine, 0.0f);
    }
    vocalor::VoiceEngineTestAccess::setRadiatedPowerDepth(*unregulatedEngine, 0.0f);
    vocalor::VoiceEngineTestAccess::setRadiatedPowerDepth(*regulatedEngine, 1.0f);
    unregulatedEngine->noteOn(79, 0.74f);
    regulatedEngine->noteOn(79, 0.74f);
    render(*unregulatedEngine, static_cast<int>(0.8 * sampleRate));
    render(*regulatedEngine, static_cast<int>(0.8 * sampleRate));
    const auto rawWave = renderMono(*unregulatedEngine,
                                    static_cast<int>(0.25 * sampleRate));
    const auto regulatedWave = renderMono(*regulatedEngine,
                                          static_cast<int>(0.25 * sampleRate));
    double cross = 0.0;
    double rawSquare = 0.0;
    double regulatedSquare = 0.0;
    for (std::size_t sample = 0; sample < rawWave.size(); ++sample)
    {
        cross += static_cast<double>(rawWave[sample]) * regulatedWave[sample];
        rawSquare += static_cast<double>(rawWave[sample]) * rawWave[sample];
        regulatedSquare += static_cast<double>(regulatedWave[sample])
                         * regulatedWave[sample];
    }
    const double fittedGain = cross / std::max(rawSquare, 1.0e-30);
    double residualSquare = 0.0;
    for (std::size_t sample = 0; sample < rawWave.size(); ++sample)
    {
        const double residual = regulatedWave[sample]
            - fittedGain * static_cast<double>(rawWave[sample]);
        residualSquare += residual * residual;
    }
    const double normalisedResidual = std::sqrt(
        residualSquare / std::max(regulatedSquare, 1.0e-30));
    const auto scalarState
        = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
            *regulatedEngine, 79);
    std::cout << "radiated-power scalar residual: " << std::scientific
              << normalisedResidual << ", fitted/state gain " << std::fixed
              << std::setprecision(5) << fittedGain << "/" << scalarState[0]
              << "\n";
    expect(normalisedResidual < 2.0e-5,
           "radiated-power support changed the steady voiced spectrum or phase");
    expect(std::abs(fittedGain - scalarState[0]) < 2.0e-4,
           "the rendered voiced scalar did not match the regulator state");

    // Use the two largest analytical extremes from the line for a connected
    // retune. noteOn() may aim a new tract immediately, but neither the running
    // gain nor its smoothed target may snap to the new raw answer.
    const auto minimumRaw = std::min_element(
        regulated.regulation.begin(), regulated.regulation.end(),
        [](const auto& left, const auto& right) { return left[3] < right[3]; });
    const auto maximumRaw = std::max_element(
        regulated.regulation.begin(), regulated.regulation.end(),
        [](const auto& left, const auto& right) { return left[3] < right[3]; });
    const auto fromIndex = static_cast<std::size_t>(
        std::distance(regulated.regulation.begin(), minimumRaw));
    const auto toIndex = static_cast<std::size_t>(
        std::distance(regulated.regulation.begin(), maximumRaw));
    vocalor::VoiceEngine legato;
    legato.prepare(sampleRate, blockSize);
    legato.reset();
    legato.setParameters(parameters);
    vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(legato, 0.0f);
    legato.noteOn(line[fromIndex], 0.74f);
    render(legato, static_cast<int>(0.8 * sampleRate)); // exactly 150 updates
    const auto before = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
        legato, line[fromIndex]);
    legato.noteOn(line[toIndex], 0.74f);
    legato.noteOff(line[fromIndex]);
    const auto immediate = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
        legato, line[toIndex]);
    expect(std::abs(immediate[0] - before[0]) < 2.0e-5f
               && std::abs(immediate[2] - before[2]) < 2.0e-5f,
           "a legato register move snapped the breath-support gain or target");
    render(legato, static_cast<int>(0.025 * sampleRate));
    const auto early = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
        legato, line[toIndex]);
    render(legato, static_cast<int>(0.575 * sampleRate));
    const auto settled = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
        legato, line[toIndex]);
    const float completeMove = std::abs(settled[0] - before[0]);
    expect(completeMove > 0.08f,
           "the legato smoothing fixture did not cross a material power target");
    expect(std::abs(early[0] - before[0]) < 0.80f * completeMove,
           "the radiated-power target completed a register jump inside 25 ms");
    expect(std::abs(settled[0] - settled[2]) < 0.01f
               && std::abs(settled[2] - settled[3]) < 0.03f,
           "radiated-power support did not settle after the legato transition");

    // Pitch vibrato is explicitly excluded from the intentional fundamental.
    // Prove the oscillator is moving while the power detector remains still.
    auto vibratoParameters = scalarParameters;
    vibratoParameters.vibrato = 1.0f;
    vibratoParameters.instability = 1.0f;
    vocalor::VoiceEngine vibrato;
    vibrato.prepare(sampleRate, blockSize);
    vibrato.reset();
    vibrato.setParameters(vibratoParameters);
    vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(vibrato, 0.0f);
    // F5 sits beside the 16/32-harmonic mip boundary at 48 kHz, so the
    // fully modulated oscillator crosses it during this probe. The regulator's
    // analysis mip must still remain tied to the intentional f0.
    constexpr int vibratoMipBoundaryNote = 77;
    vibrato.noteOn(vibratoMipBoundaryNote, 0.74f);
    render(vibrato, static_cast<int>(1.0 * sampleRate));
    float minimumPitch = std::numeric_limits<float>::infinity();
    float maximumPitch = 0.0f;
    float minimumTarget = std::numeric_limits<float>::infinity();
    float maximumTarget = 0.0f;
    float minimumRawTarget = std::numeric_limits<float>::infinity();
    float maximumRawTarget = 0.0f;
    for (int observation = 0; observation < 500; ++observation)
    {
        render(vibrato, 64);
        const float pitch = vocalor::VoiceEngineTestAccess::frequencyForRoot(
            vibrato, vibratoMipBoundaryNote);
        const auto state = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
            vibrato, vibratoMipBoundaryNote);
        minimumPitch = std::min(minimumPitch, pitch);
        maximumPitch = std::max(maximumPitch, pitch);
        minimumTarget = std::min(minimumTarget, state[2]);
        maximumTarget = std::max(maximumTarget, state[2]);
        minimumRawTarget = std::min(minimumRawTarget, state[3]);
        maximumRawTarget = std::max(maximumRawTarget, state[3]);
    }
    expect(maximumPitch - minimumPitch > 20.0f,
           "the regulator's vibrato-exclusion fixture did not move pitch");
    expect(maximumTarget - minimumTarget < 2.0e-5f
               && maximumRawTarget - minimumRawTarget < 2.0e-5f,
           "pitch vibrato pumped the radiated-power target");

    // The source filters and tract poles are sample-rate-normalised. Compare
    // two representative registers at every full-fidelity shipping rate.
    std::array<std::array<double, 4>, 2> rateGainDb {};
    constexpr std::array<double, 4> rates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr std::array<int, 2> rateNotes { 60, 79 };
    for (std::size_t note = 0; note < rateNotes.size(); ++note)
    {
        for (std::size_t rate = 0; rate < rates.size(); ++rate)
        {
            vocalor::VoiceEngine engine;
            engine.prepare(rates[rate], blockSize);
            engine.reset();
            engine.setParameters(scalarParameters);
            vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(engine, 0.0f);
            engine.noteOn(rateNotes[note], 0.74f);
            render(engine, static_cast<int>(0.35 * rates[rate]));
            const auto state
                = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(
                    engine, rateNotes[note]);
            rateGainDb[note][rate] = 20.0 * std::log10(
                std::max(static_cast<double>(state[0]), 1.0e-30));
            expect(std::abs(state[0] - state[2]) < 3.0e-3f,
                   "radiated-power gain did not settle at "
                       + std::to_string(static_cast<int>(rates[rate])) + " Hz");
        }
        expect(span(rateGainDb[note]) < 0.20,
               "radiated-power support changed with sample rate on MIDI "
                   + std::to_string(rateNotes[note]));
    }

    // Absolute voice age, not host callback size, owns the expensive ~5.3 ms
    // update cadence. Aligned blocks are exact; an arbitrary split retains the
    // engine's existing sub-chunk residual and the same regulator state.
    auto contiguous = std::make_unique<vocalor::VoiceEngine>();
    auto split = std::make_unique<vocalor::VoiceEngine>();
    for (auto* engine : { contiguous.get(), split.get() })
    {
        engine->prepare(sampleRate, blockSize);
        engine->reset();
        engine->setParameters(scalarParameters);
        vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(*engine, 0.0f);
        engine->noteOn(79, 0.74f);
    }
    const auto contiguousAudio = renderInterleaved(
        *contiguous, static_cast<int>(0.5 * sampleRate), blockSize);
    const auto splitAudio = renderInterleaved(
        *split, static_cast<int>(0.5 * sampleRate), 37);
    float largestResidual = 0.0f;
    for (std::size_t sample = 0; sample < contiguousAudio.size(); ++sample)
        largestResidual = std::max(largestResidual,
            std::abs(contiguousAudio[sample] - splitAudio[sample]));
    const auto contiguousState
        = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(*contiguous, 79);
    const auto splitState
        = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(*split, 79);
    expect(largestResidual < 2.0e-5f,
           "radiated-power updates exceeded the arbitrary-buffer residual");
    expect(std::abs(contiguousState[0] - splitState[0]) < 1.0e-6f
               && std::abs(contiguousState[2] - splitState[2]) < 1.0e-6f,
           "radiated-power state depends on host buffer partitioning");

    // Sessions saved before the automatic support law existed carry a hidden
    // model marker rather than coupling compatibility to the Instability knob.
    auto legacyParameters = scalarParameters;
    legacyParameters.legacyRadiatedPowerBypass = true;
    vocalor::VoiceEngine legacy;
    legacy.prepare(sampleRate, blockSize);
    legacy.reset();
    legacy.setParameters(legacyParameters);
    legacy.noteOn(79, 0.74f);
    render(legacy, static_cast<int>(0.4 * sampleRate));
    const auto legacyState
        = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(legacy, 79);
    expect(legacyState[0] == 1.0f && legacyState[2] == 1.0f
               && legacyState[3] == 1.0f,
           "the legacy voice-model marker did not bypass register support");

    // At analysis rates and pitches above their Nyquist limit, the source mip
    // can contain only H1 (or a few partials). The estimator must follow that
    // selected table rather than reading the otherwise valid H1-H8 power bank.
    for (const double lowRate : { 8000.0, 16000.0 })
    {
        vocalor::VoiceEngine engine;
        engine.prepare(lowRate, blockSize);
        engine.reset();
        engine.setParameters(scalarParameters);
        vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth(engine, 0.0f);
        engine.noteOn(127, 0.74f);
        const auto audio = render(engine, static_cast<int>(0.1 * lowRate));
        const auto state
            = vocalor::VoiceEngineTestAccess::radiatedPowerForNote(engine, 127);
        expect(audio.finite && audio.rms() > 1.0e-8,
               "the low-rate/high-note source mip produced invalid audio");
        for (const int field : { 0, 2, 3 })
            expect(std::isfinite(state[static_cast<std::size_t>(field)])
                       && state[static_cast<std::size_t>(field)]
                              >= minimumGain - 2.0e-5f
                       && state[static_cast<std::size_t>(field)]
                              <= maximumGain + 2.0e-5f,
                   "the low-rate/high-note source mip produced an invalid power target");
    }
}

/** The tract level is calibrated so a solo AAH at the default settings lands at
    a known level, and the whole voice is expressed in time constants and corner
    frequencies so that level does not depend on the sample rate.

    A broadband gain quietly inserted into the excitation path shows up here as
    an equal shift at every rate. That is what a lip-radiation zero would be:
    the wavetable is already a glottal flow *derivative*, so radiation is
    modelled once, and applying it again costs level without changing timbre. */
void testSourceLevelCalibration()
{
    // Measured from the calibrated engine. The window is loose enough for
    // platform floating-point differences and far tighter than the ~2 dB a
    // second radiation stage across the excitation path costs.
    // Moved from -15.53 when the source slope started following the note's own
    // loudness: the reference render is at velocity 0.85, which is 1.4 dB below
    // full voice, so its partials above the 850 Hz shelf corner sit 2.8 dB down
    // and the broadband RMS with them. Nothing about the source's absolute
    // calibration or its single radiation accounting moved -- at velocity 1.00
    // and full dynamic the shelf is exactly transparent. The coherent LF bank
    // then moved this tract-weighted reference from -17.03 to -19.01 while
    // preserving the former source RMS: removing harmonic cancellation changes
    // which partials the AAH poles receive, not the broadband source drive. The
    // bounded register-support regulator brings the same note to -17.91 by
    // returning half of its local harmonic-alignment power error; that is a
    // voice-specific efficiency law rather than another radiation stage or a
    // global calibration gain.
    constexpr double referenceRmsDb = -17.91;
    constexpr double toleranceDb = 0.9;

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (makeReferenceParameters());
        engine.noteOn (60, 0.85f);

        render (engine, static_cast<int> (sampleRate * 0.6));   // past the onset
        const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
        const auto levelDb = 20.0 * std::log10 (held.rms() + 1.0e-30);

        const auto label = std::to_string (static_cast<int> (sampleRate)) + " Hz";
        expect (held.finite, "reference AAH at " + label + " produced a NaN or infinity");
        expect (std::abs (levelDb - referenceRmsDb) < toleranceDb,
                "reference AAH at " + label + " rendered at "
                    + std::to_string (levelDb) + " dB, outside the calibrated "
                    + std::to_string (referenceRmsDb) + " +/- "
                    + std::to_string (toleranceDb) + " dB");
    }
}

/** Every factory preset has to be playable.

    The table lives in the JUCE-free core precisely so this can be checked here:
    a preset that renders silence, clips, or carries a value the engine clamps
    away is a defect the suite finds rather than one a player does.
*/
void testFactoryPresets()
{
    constexpr auto sampleRate = 48000.0;
    const auto count = vocalor::factoryPresetCount();
    expect (count >= 8, "the factory bank is too small to open a session on");

    std::set<std::string> names;
    for (int index = 0; index < count; ++index)
    {
        const auto& preset = vocalor::factoryPreset (index);
        const std::string name = preset.name != nullptr ? preset.name : "";
        expect (! name.empty(), "factory preset " + std::to_string (index) + " has no name");
        expect (names.insert (name).second, "two factory presets share the name " + name);

        // Nothing may be silently clamped away: what the table says is what the
        // engine gets.
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (preset.parameters);

        const auto& wanted = preset.parameters;
        expect (wanted.choirSize >= 2 && wanted.choirSize <= 12,
                name + " asks for a singer count the engine cannot render exactly");
        expect (wanted.formantShift >= -12.0f && wanted.formantShift <= 12.0f,
                name + " asks for a formant shift outside the published range");
        expect (wanted.outputGain > 0.0f && wanted.outputGain <= 2.0f,
                name + " asks for an output gain outside the published range");
        for (const float unit : { wanted.breath, wanted.resonance, wanted.vibrato,
                                  wanted.humanize, wanted.instability, wanted.spread, wanted.tension,
                                  wanted.room, wanted.vowelX, wanted.vowelY,
                                  wanted.vowelMorph, wanted.glide, wanted.roomSize,
                                  wanted.dynamics, wanted.intonation, wanted.nasal })
            expect (unit >= 0.0f && unit <= 1.0f,
                    name + " carries a normalised value outside 0..1");

        // A held note and a held triad, because chord and choir presets take
        // different paths through the allocator.
        engine.noteOn (57, 0.85f);
        engine.noteOn (64, 0.85f);
        const auto held = render (engine, static_cast<int> (sampleRate * 0.6));
        expect (held.finite, name + " produced a NaN or an infinity");
        expect (held.rms() > 1.0e-5, name + " rendered silence");
        expect (held.peak < 4.0, name + " exceeded the amplitude guardrail");

        engine.noteOff (57);
        engine.noteOff (64);
        const auto tail = render (engine, static_cast<int> (sampleRate * 4.0));
        expect (tail.finite, name + " produced invalid audio during its release");
        expect (engine.getActiveVoiceCount() == 0, name + " never finished releasing");
    }

    // ... and it has to be playable at the level it was voiced at. Nothing else
    // in the suite makes a re-trim mandatory: the checks above pass a preset
    // left 10 dB quiet, and testSourceLevelCalibration renders at Dynamics 1.00,
    // where the voiced gain does not move at all. These are the levels the bank
    // shipped at before the dynamic grew to 30 dB and the source slope started
    // following the note's own loudness, both of which lower any preset not sung
    // at full velocity and full dynamic. Measured on the shipping engine: two
    // notes held at 57 and 64 at velocity 0.85, 1 s of stereo RMS from t = 0.6 s
    // at 48 kHz.
    //
    // This is a pin on one chord, not a level calibration: the same presets
    // move 1.6-12.5 dB across the twelve two-note chords the bank was measured
    // over, because how coherently a section sums depends on the notes it is
    // singing. Once the ensemble's entry timing is redrawn at every note that
    // pin becomes a draw as well. Closed Mouth Hum and Small Voices moved 1.95
    // and 0.74 dB here while their means over twelve chords moved 0.05 and
    // 0.38 dB, so those two figures were re-measured rather than trimmed back:
    // an outputGain that restored this chord would have left both presets up
    // to 1.9 dB wrong on every other one.
    struct Level { const char* name; double db; };
    constexpr std::array<Level, 12> shipped { {
        { "Init Soprano", -19.04 }, { "Intimate Alto", -25.06 },
        { "Pressed Tenor", -20.28 }, { "Legato Soloist", -25.41 },
        { "Breath And Air", -29.81 }, { "Warm Bass Choir", -20.98 },
        { "Cathedral Ensemble", -22.85 }, { "Closed Mouth Hum", -32.90 },
        { "Small Voices", -23.60 }, { "Vowel Morph Pad", -27.79 },
        { "Locked Major Chorale", -26.34 }, { "Airy Minor Pad", -29.62 } } };

    for (const auto& entry : shipped)
    {
        int index = -1;
        for (int i = 0; i < count; ++i)
            if (std::string (vocalor::factoryPreset (i).name) == entry.name)
                index = i;
        expect (index >= 0, std::string (entry.name) + " is no longer in the factory bank");
        if (index < 0)
            continue;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (vocalor::factoryPreset (index).parameters);
        engine.noteOn (57, 0.85f);
        engine.noteOn (64, 0.85f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
        const auto levelDb = 20.0 * std::log10 (held.rms() + 1.0e-30);
        std::cout << "preset level: " << std::left << std::setw (22) << entry.name
                  << std::right << std::fixed << std::setprecision (2) << levelDb
                  << " dB against " << entry.db << " dB\n";
        expect (std::abs (levelDb - entry.db) <= 0.03,
                std::string (entry.name) + " renders at " + std::to_string (levelDb)
                    + " dB against the " + std::to_string (entry.db)
                    + " dB it shipped at: the preset bank was not re-trimmed");
    }
}

/** The singer's formant is a cluster, not a boost.

    The 1.1 engine raised F3 by 12 % and F4 by 6 % of Tension and left them
    where they were. Three formants 700 Hz apart with slightly more gain each
    are still three formants; the peak that lets an unamplified voice carry over
    an orchestra comes from narrowing the epilaryngeal tube until F3, F4 and F5
    collapse into one.
*/
void testSingersFormantCluster()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 146.832;   // D3, so the comb resolves the peak

    const auto probe = [] (float tension, std::array<float, vocalor::kFormantCount>& formants)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.profile = vocalor::VoiceProfile::Male;
        parameters.tension = tension;
        engine.setParameters (parameters);
        engine.noteOn (50, 0.80f);
        renderMono (engine, static_cast<int> (sampleRate * 0.6));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.6));
        formants = vocalor::VoiceEngineTestAccess::chunkFormants (engine);

        const auto energy = [&samples] (int first, int last)
        {
            double total = 0.0;
            for (int harmonic = first; harmonic <= last; ++harmonic)
            {
                const auto magnitude = harmonicMagnitude (
                    samples, fundamental * harmonic, sampleRate);
                total += magnitude * magnitude;
            }
            return total;
        };
        // 2.05-4.0 kHz against everything from the fundamental to 6 kHz.
        return energy (14, 27) / std::max (energy (1, 41), 1.0e-18);
    };

    std::array<float, vocalor::kFormantCount> relaxed {};
    std::array<float, vocalor::kFormantCount> pressed {};
    const auto relaxedRatio = probe (0.0f, relaxed);
    const auto pressedRatio = probe (0.95f, pressed);

    const auto relaxedSpan = relaxed[4] - relaxed[2];
    const auto pressedSpan = pressed[4] - pressed[2];
    const auto ringDb = 10.0 * std::log10 (std::max (pressedRatio, 1.0e-18)
                                           / std::max (relaxedRatio, 1.0e-18));
    std::cout << "epilarynx: F3-F5 span " << std::fixed << std::setprecision (0)
              << relaxedSpan << " -> " << pressedSpan << " Hz, 2.05-4 kHz share "
              << std::setprecision (1) << ringDb << " dB\n";

    // At rest the tract has to be exactly the vowel it always was: the male
    // open anchor is F3 2440, F4 3250, F5 4300.
    expect (std::abs (relaxed[2] - 2440.0f) < 1.0f && std::abs (relaxed[3] - 3250.0f) < 1.0f
                && std::abs (relaxed[4] - 4300.0f) < 1.0f,
            "a relaxed larynx no longer leaves the vowel's own upper formants alone");

    expect (pressedSpan < 0.72f * relaxedSpan,
            "the upper formants did not cluster: this is still an amplitude boost");
    expect (pressed[2] > relaxed[2] && pressed[4] < relaxed[4],
            "the cluster did not close from both sides toward the epilarynx resonance");
    expect (ringDb > 3.0,
            "clustering the upper formants did not put energy where a voice carries");

    // Narrowing the epilarynx is a phonation mode, not a vowel change. F1 and
    // F2 carry the vowel's identity and belong to the vowel, so the epilarynx
    // must not touch them however hard the glottis is pressed.
    for (const int formant : { 0, 1 })
    {
        const auto index = static_cast<std::size_t> (formant);
        expect (std::abs (pressed[index] - relaxed[index]) <= 1.0e-3f * relaxed[index],
                "tension moved F" + std::to_string (formant + 1)
                    + ", which carries the vowel rather than the singer's formant");
    }
}

/** A soprano's upper reinforcement is not a tenor singer's formant moved up an
    octave. At low and middle pitches it spans roughly two kilohertz; once the
    fundamental reaches the upper soprano range the narrow cluster disappears
    and the ordinary F3-F5 tract resonances reinforce the sparse harmonics.

    The two notes in the simultaneous leg are deliberate. A chunk-wide width
    can make either C4 or B-flat5 correct in isolation, but never both at once.
*/
void testSopranoClusterReleaseIsPerVoice()
{
    constexpr auto sampleRate = 48000.0;
    constexpr float releaseStartHz = 622.25f;
    using Snapshot = vocalor::VoiceEngineTestAccess::TractSnapshot;

    const auto probe = [] (int midiNote, float tension)
    {
        vocalor::VoiceEngine engine;
        engine.prepare(sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.profile = vocalor::VoiceProfile::Female;
        parameters.tension = tension;
        engine.setParameters(parameters);
        engine.noteOn(midiNote, 0.80f);
        render(engine, static_cast<int>(sampleRate * 0.7));
        return vocalor::VoiceEngineTestAccess::voiceTract(engine, midiNote);
    };

    const auto outerWidth = [] (const Snapshot& tract)
    {
        return tract.hz[4] + 0.5f * tract.bandwidth[4]
             - tract.hz[2] + 0.5f * tract.bandwidth[2];
    };

    const auto lowRelaxed = probe(60, 0.0f);
    const auto lowPressed = probe(60, 0.95f);
    const auto highRelaxed = probe(82, 0.0f);  // B-flat5, 932.3 Hz
    const auto highPressed = probe(82, 0.95f);

    std::cout << "soprano reinforcement: C4 outer F3-F5 width "
              << std::fixed << std::setprecision(0) << outerWidth(lowRelaxed)
              << " -> " << outerWidth(lowPressed) << " Hz; B-flat5 "
              << outerWidth(highRelaxed) << " -> " << outerWidth(highPressed)
              << " Hz\n";

    expect(lowPressed.hz[2] > lowRelaxed.hz[2]
               && lowPressed.hz[4] < lowRelaxed.hz[4],
           "the low soprano reinforcement no longer converges from both sides");
    expect(lowPressed.bandwidth[2] < lowRelaxed.bandwidth[2]
               && lowPressed.bandwidth[4] < lowRelaxed.bandwidth[4],
           "the low soprano reinforcement no longer changes the pole widths");
    expect(outerWidth(lowPressed) >= 1900.0f,
           "the low soprano inherited a narrow lower-voice singer's formant");

    for (int formant = 2; formant < vocalor::kFormantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(std::abs(highPressed.hz[index] - highRelaxed.hz[index])
                   <= 0.002f * highRelaxed.hz[index],
               "the upper soprano still pulls F" + std::to_string(formant + 1)
                   + " into the epilaryngeal cluster");
        expect(std::abs(highPressed.bandwidth[index] - highRelaxed.bandwidth[index])
                   <= 0.002f * highRelaxed.bandwidth[index],
                   "the upper soprano still narrows F" + std::to_string(formant + 1));
    }

    // The release law is a curve across the register, not a hidden switch at
    // either study pitch. A pitch near the geometric midpoint must sit strictly
    // between the engaged and released geometries.
    const auto startRelaxed = probe(75, 0.0f);  // E-flat5, 622.3 Hz
    const auto startPressed = probe(75, 0.95f);
    const auto middleRelaxed = probe(79, 0.0f); // G5, 784.0 Hz
    const auto middlePressed = probe(79, 0.95f);
    const auto clusterDistance = [] (const Snapshot& relaxed, const Snapshot& pressed)
    {
        float result = 0.0f;
        for (int formant = 2; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            result += std::abs(pressed.hz[index] - relaxed.hz[index]);
            result += 2.0f * std::abs(pressed.bandwidth[index]
                                     - relaxed.bandwidth[index]);
        }
        return result;
    };
    const float startDistance = clusterDistance(startRelaxed, startPressed);
    const float middleDistance = clusterDistance(middleRelaxed, middlePressed);
    const float endDistance = clusterDistance(highRelaxed, highPressed);
    expect(startDistance > middleDistance && middleDistance > endDistance + 0.1f,
           "the soprano cluster release is not monotonic through the crossover");
    expect(outerWidth(startPressed) < outerWidth(middlePressed)
               && outerWidth(middlePressed) < outerWidth(highPressed),
           "the soprano reinforcement width stepped across the crossover");

    // Both geometries must coexist behind one parameter/chunk state.
    auto together = std::make_unique<vocalor::VoiceEngine>();
    together->prepare(sampleRate, blockSize);
    together->reset();
    auto parameters = steadyParameters();
    parameters.profile = vocalor::VoiceProfile::Female;
    parameters.tension = 0.95f;
    together->setParameters(parameters);
    together->noteOn(60, 0.80f);
    together->noteOn(82, 0.80f);
    render(*together, static_cast<int>(sampleRate * 0.7));
    const auto simultaneousLow = vocalor::VoiceEngineTestAccess::voiceTract(*together, 60);
    const auto simultaneousHigh = vocalor::VoiceEngineTestAccess::voiceTract(*together, 82);
    for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(std::abs(simultaneousLow.bandwidth[index] - lowPressed.bandwidth[index]) < 0.05f,
               "adding a high soprano note changed the low voice's pole width");
        expect(std::abs(simultaneousHigh.bandwidth[index] - highPressed.bandwidth[index]) < 0.05f,
               "adding a low soprano note changed the high voice's pole width");
    }

    // The cascade amplitudes and rendered resonators have to describe those
    // exact per-voice poles, not the nominal chunk endpoint.
    for (const auto& tract : { simultaneousLow, simultaneousHigh })
    {
        std::array<float, vocalor::kFormantCount> expected {};
        vocalor::parallelFormantAmplitudes(
            tract.hz.data(), tract.bandwidth.data(), vocalor::kFormantCount,
            static_cast<float>(sampleRate), 0.010f, expected.data());
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            const float scale = std::max(1.0e-6f, expected[index]);
            expect(std::abs(tract.gain[index] - expected[index]) <= 2.0e-5f * scale,
                   "a per-voice formant gain was derived from different poles");
            const float signedGain = vocalor::formantPolarity(formant) * tract.gain[index];
            expect(std::abs(tract.b0[index]
                            - signedGain * tract.peakNormaliser[index]) <= 1.0e-7f,
                   "the rendered b0 does not contain the measured per-voice gain");
            const float radius = std::exp(-3.14159265358979323846f
                                          * tract.bandwidth[index]
                                          / static_cast<float>(sampleRate));
            const float expectedA1 = 2.0f * radius * std::cos(
                2.0f * 3.14159265358979323846f * tract.hz[index]
                / static_cast<float>(sampleRate));
            expect(std::abs(tract.a1[index] - expectedA1) <= 3.0e-6f,
                   "the rendered pole angle does not contain the per-voice frequency");
            expect(std::abs(tract.a2[index] + radius * radius) <= 2.0e-6f,
                   "the rendered pole radius does not contain the per-voice bandwidth");
        }
    }

    // Sparse high harmonics are the output-side reason for releasing the
    // cluster. At the study's 932 Hz pitch the ordinary F3, F4 and F5 poles
    // should support H3-H5 instead of leaving the fifth harmonic beyond one
    // narrow 3 kHz peak. These are conservative synthesis guardrails, not
    // values claimed by the listening study.
    auto harmonicEngine = std::make_unique<vocalor::VoiceEngine>();
    harmonicEngine->prepare(sampleRate, blockSize);
    harmonicEngine->reset();
    harmonicEngine->setParameters(parameters);
    harmonicEngine->noteOn(82, 0.80f);
    renderMono(*harmonicEngine, static_cast<int>(sampleRate * 0.7));
    const auto highAudio = renderMono(*harmonicEngine, static_cast<int>(sampleRate * 0.7));
    constexpr double highFundamental = 932.327523;
    const double fundamentalMagnitude = harmonicMagnitude(
        highAudio, highFundamental, sampleRate);
    std::array<double, 5> harmonicDb {};
    for (int harmonic = 1; harmonic <= 5; ++harmonic)
    {
        const double magnitude = harmonicMagnitude(
            highAudio, highFundamental * harmonic, sampleRate);
        harmonicDb[static_cast<std::size_t>(harmonic - 1)] = 20.0 * std::log10(
            std::max(magnitude, 1.0e-18) / std::max(fundamentalMagnitude, 1.0e-18));
    }
    std::cout << "B-flat5 harmonics against H1: H2 " << std::setprecision(1)
              << harmonicDb[1] << ", H3 " << harmonicDb[2] << ", H4 "
              << harmonicDb[3] << ", H5 " << harmonicDb[4] << " dB\n";
    expect(harmonicDb[4] > -60.0,
           "the released soprano tract still abandons the fifth harmonic");
    for (int harmonic = 2; harmonic < 5; ++harmonic)
        expect(harmonicDb[static_cast<std::size_t>(harmonic)]
                   - harmonicDb[static_cast<std::size_t>(harmonic - 1)] > -20.0,
               "the upper soprano harmonic support falls through a narrow spectral gap");
    const auto highDisplay = harmonicEngine->getDisplayState();
    const auto highDisplayTract = vocalor::VoiceEngineTestAccess::voiceTract(
        *harmonicEngine, 82);
    for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(std::abs(highDisplay.formantHz[index] - highDisplayTract.hz[index]) < 0.01f
                   && std::abs(highDisplay.formantBandwidth[index]
                               - highDisplayTract.bandwidth[index]) < 0.01f
                   && std::abs(highDisplay.formantGain[index]
                               - highDisplayTract.gain[index]) < 1.0e-5f,
               "the high-register display does not publish one coherent voice tract");
    }

    // Legato crosses the release region by moving the tract, not by replacing
    // one filter with another on the note event.
    auto legatoParameters = parameters;
    legatoParameters.legato = true;
    auto legato = std::make_unique<vocalor::VoiceEngine>();
    legato->prepare(sampleRate, blockSize);
    legato->reset();
    legato->setParameters(legatoParameters);
    legato->noteOn(60, 0.80f);
    render(*legato, static_cast<int>(sampleRate * 0.7));
    const auto before = vocalor::VoiceEngineTestAccess::voiceTract(*legato, 60);
    legato->noteOn(82, 0.80f);
    const auto first = vocalor::VoiceEngineTestAccess::voiceTract(*legato, 82);
    const float fullSpan = highPressed.bandwidth[4] - before.bandwidth[4];
    const float firstMove = first.bandwidth[4] - before.bandwidth[4];
    expect(firstMove > 0.0f && firstMove < 0.25f * fullSpan,
           "the soprano cluster release stepped instead of articulating");

    // The high-F1 efficiency trim is part of the same jaw gesture. It once
    // jumped to the destination at the first control tick and left a broadband
    // click even though every tract pole was moving smoothly.
    auto previousEfficiency = vocalor::VoiceEngineTestAccess::efficiencyForNote(
        *legato, 82)[0];
    float largestEfficiencyStep = 0.0f;
    std::array<float, 1> transitionLeft {};
    std::array<float, 1> transitionRight {};
    for (int sample = 0; sample < 2048; ++sample)
    {
        legato->process(transitionLeft.data(), transitionRight.data(), 1);
        const float efficiency = vocalor::VoiceEngineTestAccess::efficiencyForNote(
            *legato, 82)[0];
        largestEfficiencyStep = std::max(
            largestEfficiencyStep, std::abs(efficiency - previousEfficiency));
        previousEfficiency = efficiency;
    }
    expect(largestEfficiencyStep < 0.002f,
           "the high-register efficiency trim stepped during legato");
    const auto transition = render(*legato, static_cast<int>(sampleRate * 0.7));
    const auto after = vocalor::VoiceEngineTestAccess::voiceTract(*legato, 82);
    expect(transition.finite && transition.peak < 4.0,
           "the soprano cluster transition produced invalid or unbounded audio");
    expect(std::abs(after.bandwidth[4] - highPressed.bandwidth[4]) < 0.5f,
           "the soprano cluster release did not reach its high-register endpoint");

    // A slow portamento must carry the tract through the same register as the
    // sounding pitch. Jumping the release target directly to the destination
    // makes the filter sound like a second, disconnected performer during the
    // glide even if its coefficient transition itself is smooth.
    auto longGlideParameters = parameters;
    longGlideParameters.legato = true;
    longGlideParameters.glide = 1.0f;
    auto longGlide = std::make_unique<vocalor::VoiceEngine>();
    longGlide->prepare(sampleRate, blockSize);
    longGlide->reset();
    longGlide->setParameters(longGlideParameters);
    longGlide->noteOn(60, 0.80f);
    render(*longGlide, static_cast<int>(sampleRate * 0.7));
    longGlide->noteOn(82, 0.80f);
    render(*longGlide, static_cast<int>(sampleRate * 0.08));
    const auto duringGlide = vocalor::VoiceEngineTestAccess::voiceTract(*longGlide, 82);
    const auto duringFrequency = vocalor::VoiceEngineTestAccess::frequencyForRoot(
        *longGlide, 82);
    expect(duringFrequency < releaseStartHz,
           "the long-glide fixture crossed the soprano release range too early");
    expect(duringGlide.bandwidth[4] < lowPressed.bandwidth[4]
               + 0.40f * (highPressed.bandwidth[4] - lowPressed.bandwidth[4]),
           "the soprano tract arrived at the destination before a long glide did");
    render(*longGlide, static_cast<int>(sampleRate * 3.0));
    const auto afterLongGlide = vocalor::VoiceEngineTestAccess::voiceTract(*longGlide, 82);
    expect(std::abs(afterLongGlide.bandwidth[4] - highPressed.bandwidth[4]) < 0.5f,
           "the tract did not finish a long pitch-and-formant glide together");

    // Pitch bend is another intentional change of sung F0. Bending the high
    // note down an octave should recover the same clustered tract as an
    // attacked B-flat4, then return to the released endpoint without allowing
    // the cycle-by-cycle vibrato to drag the tract around.
    auto bent = std::make_unique<vocalor::VoiceEngine>();
    bent->prepare(sampleRate, blockSize);
    bent->reset();
    bent->setParameters(parameters);
    bent->noteOn(82, 0.80f);
    render(*bent, static_cast<int>(sampleRate * 0.7));
    bent->setPitchBend(-12.0f);
    render(*bent, static_cast<int>(sampleRate * 0.9));
    const auto bentDown = vocalor::VoiceEngineTestAccess::voiceTract(*bent, 82);
    const auto attackedDown = probe(70, 0.95f);
    for (int formant = 2; formant < vocalor::kFormantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(std::abs(bentDown.hz[index] - attackedDown.hz[index])
                   < 0.003f * attackedDown.hz[index]
                   && std::abs(bentDown.bandwidth[index] - attackedDown.bandwidth[index])
                   < 0.003f * attackedDown.bandwidth[index],
               "pitch bend left the soprano tract at the MIDI-note register");
    }
    bent->setPitchBend(0.0f);
    render(*bent, static_cast<int>(sampleRate * 0.9));
    const auto bentReturned = vocalor::VoiceEngineTestAccess::voiceTract(*bent, 82);
    expect(std::abs(bentReturned.bandwidth[4] - highPressed.bandwidth[4]) < 0.5f,
           "the soprano tract did not return after pitch bend");

    for (const double rate : { 44100.0, 96000.0 })
    {
        auto rateEngine = std::make_unique<vocalor::VoiceEngine>();
        rateEngine->prepare(rate, blockSize);
        rateEngine->reset();
        rateEngine->setParameters(parameters);
        rateEngine->noteOn(82, 0.80f);
        render(*rateEngine, static_cast<int>(rate * 0.7));
        const auto tract = vocalor::VoiceEngineTestAccess::voiceTract(*rateEngine, 82);
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            expect(std::abs(tract.hz[index] - highPressed.hz[index])
                       < 0.001f * highPressed.hz[index]
                       && std::abs(tract.bandwidth[index] - highPressed.bandwidth[index])
                       < 0.001f * highPressed.bandwidth[index],
                   "the soprano tract geometry depends on sample rate");
        }
    }

    auto oneBlock = std::make_unique<vocalor::VoiceEngine>();
    auto splitBlocks = std::make_unique<vocalor::VoiceEngine>();
    for (auto* engine : { oneBlock.get(), splitBlocks.get() })
    {
        engine->prepare(sampleRate, blockSize);
        engine->reset();
        engine->setParameters(parameters);
        engine->noteOn(82, 0.80f);
    }
    const auto contiguous = renderInterleaved(
        *oneBlock, static_cast<int>(sampleRate * 0.8), blockSize);
    const auto split = renderInterleaved(
        *splitBlocks, static_cast<int>(sampleRate * 0.8), 37);
    float largestResidual = 0.0f;
    std::size_t largestResidualIndex = 0;
    for (std::size_t i = 0; i < contiguous.size(); ++i)
    {
        const float residual = std::abs(contiguous[i] - split[i]);
        if (residual > largestResidual)
        {
            largestResidual = residual;
            largestResidualIndex = i;
        }
    }
    std::cout << "soprano buffer-split residual: " << std::scientific
              << largestResidual << std::fixed << " peak at frame "
              << largestResidualIndex / 2 << "\n";
    // Arbitrary sub-chunk host buffers change floating-point summation at a
    // residual below one 16-bit PCM step; aligned render chunks remain exact.
    // This is deliberately five times stricter than the engine-wide 1e-4
    // sub-chunk guardrail, without pretending the two process paths are bitwise
    // associative.
    expect(largestResidual < 2.0e-5f,
           "the soprano tract exceeded its bounded host-buffer residual");

    auto alignedLarge = std::make_unique<vocalor::VoiceEngine>();
    auto alignedSmall = std::make_unique<vocalor::VoiceEngine>();
    for (auto* engine : { alignedLarge.get(), alignedSmall.get() })
    {
        engine->prepare(sampleRate, blockSize);
        engine->reset();
        engine->setParameters(parameters);
        engine->noteOn(82, 0.80f);
    }
    const auto largeChunks = renderInterleaved(
        *alignedLarge, static_cast<int>(sampleRate * 0.25), blockSize);
    const auto smallChunks = renderInterleaved(
        *alignedSmall, static_cast<int>(sampleRate * 0.25),
        vocalor::VoiceEngineTestAccess::chunkSize);
    expect(largeChunks == smallChunks,
           "aligned host buffers changed the soprano tract render");
}

/** Direct broadband excitation found that soprano R3 and R4 rise with f0 at
    about 0.48 and 0.46 Hz/Hz, with large but bounded singer-to-singer scatter.
    This is a resonance trajectory, not harmonic locking: inspect the running
    pole geometry directly and make no demand that H3-H5 be flat or aligned. */
void testSopranoUpperResonanceRise()
{
    constexpr double sampleRate = 48000.0;
    constexpr int lowMidi = 60;   // C4
    constexpr int highMidi = 82;  // B-flat5
    constexpr float lowFundamental = 261.625565f;
    constexpr float highFundamental = 932.327523f;
    constexpr float riseAnchor = 261.63f;
    constexpr float riseSpan = highFundamental - riseAnchor;
    using SingerSnapshot = vocalor::VoiceEngineTestAccess::SingerTractSnapshot;
    using Tract = vocalor::VoiceEngineTestAccess::TractSnapshot;

    const auto parametersFor = [](vocalor::VoiceProfile profile, float humanize,
                                  vocalor::PerformanceMode mode)
    {
        auto parameters = steadyParameters();
        parameters.profile = profile;
        parameters.mode = mode;
        parameters.choirSize = mode == vocalor::PerformanceMode::Choir ? 12 : 1;
        parameters.vowel = vocalor::Vowel::Aah;
        // Zero tension removes the independent singer's-formant cluster, so
        // the measured movement here belongs only to the R3/R4 register law.
        parameters.tension = 0.0f;
        parameters.breath = 0.0f;
        parameters.humanize = humanize;
        parameters.vibrato = 0.0f;
        parameters.instability = 0.0f;
        parameters.room = 0.0f;
        parameters.intonation = 0.0f;
        return parameters;
    };

    const auto probe = [&](double rate, const vocalor::EngineParameters& parameters,
                           int midiNote)
    {
        auto engine = std::make_unique<vocalor::VoiceEngine>();
        engine->prepare(rate, blockSize);
        engine->reset();
        engine->setParameters(parameters);
        engine->noteOn(midiNote, 0.80f);
        render(*engine, static_cast<int>(rate * 0.9));
        return vocalor::VoiceEngineTestAccess::singerTracts(*engine, midiNote);
    };

    const auto femaleParameters = parametersFor(
        vocalor::VoiceProfile::Female, 0.0f, vocalor::PerformanceMode::Solo);
    const auto maleParameters = parametersFor(
        vocalor::VoiceProfile::Male, 0.0f, vocalor::PerformanceMode::Solo);
    const auto femaleLowVoices = probe(sampleRate, femaleParameters, lowMidi);
    const auto femaleHighVoices = probe(sampleRate, femaleParameters, highMidi);
    const auto maleLowVoices = probe(sampleRate, maleParameters, lowMidi);
    const auto maleHighVoices = probe(sampleRate, maleParameters, highMidi);
    expect(femaleLowVoices.size() == 1 && femaleHighVoices.size() == 1
               && maleLowVoices.size() == 1 && maleHighVoices.size() == 1,
           "the R3/R4 probe did not render one solo tract");
    if (femaleLowVoices.empty() || femaleHighVoices.empty()
        || maleLowVoices.empty() || maleHighVoices.empty())
        return;

    const auto& femaleLow = femaleLowVoices.front().tract;
    const auto& femaleHigh = femaleHighVoices.front().tract;
    const auto& maleLow = maleLowVoices.front().tract;
    const auto& maleHigh = maleHighVoices.front().tract;

    // C4 is the hinge, not the first already-shifted sample. These are the
    // pre-existing AAH targets at Humanize zero and zero epilaryngeal tension.
    constexpr std::array<float, 3> femaleC4 { 2810.0f, 3650.0f, 4950.0f };
    constexpr std::array<float, 3> maleC4 { 2440.0f, 3250.0f, 4300.0f };
    for (int upper = 0; upper < 3; ++upper)
    {
        const auto index = static_cast<std::size_t>(upper + 2);
        expect(std::abs(femaleLow.hz[index] - femaleC4[static_cast<std::size_t>(upper)])
                   < 0.2f,
               "the soprano register rise moved the C4 hinge");
        expect(std::abs(maleLow.hz[index] - maleC4[static_cast<std::size_t>(upper)])
                   < 0.2f,
               "the female register work changed the male C4 tract");
    }

    // The older, very small high-register tract shortening applies equally to
    // R3-R5. Remove that common ratio with R5, which receives no new soprano
    // rise, before reading the evidence-backed R3/R4 slopes.
    const float femaleUpperTune = femaleHigh.hz[4] / femaleLow.hz[4];
    const float r3Slope = (femaleHigh.hz[2] - femaleUpperTune * femaleLow.hz[2])
                        / riseSpan;
    const float r4Slope = (femaleHigh.hz[3] - femaleUpperTune * femaleLow.hz[3])
                        / riseSpan;
    std::cout << "soprano R3/R4 rise C4->B-flat5: " << std::fixed
              << std::setprecision(3) << r3Slope << "/" << r4Slope
              << " Hz/Hz; R5 " << femaleLow.hz[4] << " -> "
              << femaleHigh.hz[4] << " Hz\n";
    expect(std::abs(r3Slope - 0.48f) < 0.015f
               && std::abs(r4Slope - 0.46f) < 0.015f,
           "the Humanize-zero soprano R3/R4 trajectory left its measured means");

    // Formant Shift changes tract length, so the additive register movement
    // has to scale with it. Normalise the observed displacement back out at
    // both endpoints of the published control and recover the same law.
    for (const float formantShift : { -12.0f, 12.0f })
    {
        auto shiftedParameters = femaleParameters;
        shiftedParameters.formantShift = formantShift;
        const auto shiftedLowVoices = probe(
            sampleRate, shiftedParameters, lowMidi);
        const auto shiftedHighVoices = probe(
            sampleRate, shiftedParameters, highMidi);
        expect(shiftedLowVoices.size() == 1 && shiftedHighVoices.size() == 1,
               "the shifted R3/R4 slope probe lost its voice");
        if (shiftedLowVoices.empty() || shiftedHighVoices.empty())
            continue;
        const auto& shiftedLow = shiftedLowVoices.front().tract;
        const auto& shiftedHigh = shiftedHighVoices.front().tract;
        const float shiftedTune = shiftedHigh.hz[4] / shiftedLow.hz[4];
        const float shiftRatio = vocalor::formantShiftRatio(formantShift);
        const float shiftedR3 = (shiftedHigh.hz[2]
            - shiftedTune * shiftedLow.hz[2]) / (riseSpan * shiftRatio);
        const float shiftedR4 = (shiftedHigh.hz[3]
            - shiftedTune * shiftedLow.hz[3]) / (riseSpan * shiftRatio);
        expect(std::abs(shiftedR3 - 0.48f) < 0.015f
                   && std::abs(shiftedR4 - 0.46f) < 0.015f,
               "Formant Shift did not scale the soprano register displacement");
    }

    const float highSemitones = 12.0f * std::log2(highFundamental / 440.0f);
    const float legacyUpperTune = 1.0f + 0.0003f * highSemitones;
    expect(std::abs(femaleHigh.hz[4] - legacyUpperTune * femaleLow.hz[4]) < 0.5f,
           "the R3/R4 register gesture also shifted R5");

    // Male keeps precisely the former common highTune law. Dividing by R5
    // removes it and must leave no pitch-proportional R3/R4 gesture.
    const float maleUpperTune = maleHigh.hz[4] / maleLow.hz[4];
    expect(std::abs(maleUpperTune - legacyUpperTune) < 1.0e-4f
               && std::abs(femaleUpperTune - maleUpperTune) < 1.0e-4f,
           "the female R3/R4 work changed the common R5/highTune law");
    for (int formant : { 2, 3, 4 })
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(std::abs(maleHigh.hz[index] - legacyUpperTune * maleLow.hz[index])
                   < 0.5f,
               "the soprano register rise leaked into the male profile");
    }

    // Humanize one resolves the reported inter-singer scatter, but every
    // identity remains inside one measured standard deviation. Pair two
    // deterministic choir renders by singer identity so anatomy cancels.
    const auto choirParameters = parametersFor(
        vocalor::VoiceProfile::Female, 1.0f, vocalor::PerformanceMode::Choir);
    const auto choirLow = probe(sampleRate, choirParameters, lowMidi);
    const auto choirHigh = probe(sampleRate, choirParameters, highMidi);
    expect(choirLow.size() == 12 && choirHigh.size() == 12,
           "the per-singer R3/R4 probe did not render twelve identities");
    float minimumR3 = std::numeric_limits<float>::infinity();
    float maximumR3 = -std::numeric_limits<float>::infinity();
    float minimumR4 = std::numeric_limits<float>::infinity();
    float maximumR4 = -std::numeric_limits<float>::infinity();
    const auto singerCount = std::min(choirLow.size(), choirHigh.size());
    for (std::size_t singer = 0; singer < singerCount; ++singer)
    {
        expect(choirLow[singer].singer == choirHigh[singer].singer,
               "the R3/R4 population probe mismatched singer identities");
        const float identityUpperTune = choirHigh[singer].tract.hz[4]
                                      / choirLow[singer].tract.hz[4];
        const float singerR3 = (choirHigh[singer].tract.hz[2]
            - identityUpperTune * choirLow[singer].tract.hz[2]) / riseSpan;
        const float singerR4 = (choirHigh[singer].tract.hz[3]
            - identityUpperTune * choirLow[singer].tract.hz[3]) / riseSpan;
        minimumR3 = std::min(minimumR3, singerR3);
        maximumR3 = std::max(maximumR3, singerR3);
        minimumR4 = std::min(minimumR4, singerR4);
        maximumR4 = std::max(maximumR4, singerR4);
        expect(singerR3 >= 0.08f && singerR3 <= 0.88f,
               "a singer's R3 rise escaped the measured population bound");
        expect(singerR4 >= 0.07f && singerR4 <= 0.85f,
               "a singer's R4 rise escaped the measured population bound");
        expect(std::abs(identityUpperTune - legacyUpperTune) < 2.0e-4f,
               "per-singer register variation leaked into R5");
    }
    std::cout << "soprano identity slopes: R3 " << minimumR3 << "-" << maximumR3
              << ", R4 " << minimumR4 << "-" << maximumR4 << " Hz/Hz\n";
    expect(maximumR3 - minimumR3 > 0.20f
               && maximumR4 - minimumR4 > 0.20f,
           "Humanize one collapsed the measured R3/R4 inter-singer variation");

    const auto expectUpperOrder = [](const Tract& tract)
    {
        for (int formant = 2; formant < 4; ++formant)
        {
            const auto lower = static_cast<std::size_t>(formant);
            const auto upper = static_cast<std::size_t>(formant + 1);
            const float meanBandwidth = 0.5f
                * (tract.bandwidth[lower] + tract.bandwidth[upper]);
            expect(tract.hz[upper] - tract.hz[lower] >= meanBandwidth,
                   "the extrapolated soprano rise collapsed opposite-polarity upper poles");
        }
    };

    // The measured regression is a human-register law, not a line to extend
    // through all 128 MIDI notes. At the top of MIDI, check every shipped
    // vowel, all five pad anchors and both extremes of the synthetic
    // tract-length transform and the four legal Resonance/Breath corners. The
    // saturated, shift-scaled and correlated rise must keep opposite-polarity
    // poles at least their mean bandwidth apart, not merely retain their
    // numeric labels. The broadest corner is the important one: without the
    // per-voice bandwidth guard EE/-12 at C-sharp6 reached only 0.837x this
    // declared geometric separation. This guard rejects near-coincident modes;
    // it does not claim that an alternating-polarity bank is cancellation-free.
    constexpr std::array<std::pair<float, float>, 4> bandwidthCorners {
        std::pair { 0.0f, 0.0f }, std::pair { 0.0f, 1.0f },
        std::pair { 1.0f, 0.0f }, std::pair { 1.0f, 1.0f }
    };
    for (const auto vowel : { vocalor::Vowel::Aah, vocalor::Vowel::Ooh,
                              vocalor::Vowel::Uuh })
    {
        for (const float formantShift : { -12.0f, 12.0f })
        {
            for (const auto& [resonance, breath] : bandwidthCorners)
            {
                auto extremeParameters = choirParameters;
                extremeParameters.vowel = vowel;
                extremeParameters.formantShift = formantShift;
                extremeParameters.resonance = resonance;
                extremeParameters.breath = breath;
                for (const int midiNote : { 84, 85, 127 })
                {
                    const auto extreme = probe(
                        sampleRate, extremeParameters, midiNote);
                    expect(extreme.size() == 12,
                           "the extreme-register formant-order probe lost a singer");
                    for (const auto& singer : extreme)
                        expectUpperOrder(singer.tract);
                }
            }
        }
    }
    for (int cardinal = 0; cardinal < vocalor::kCardinalVowelCount; ++cardinal)
    {
        const auto point = vocalor::cardinalVowelPosition(cardinal);
        for (const float formantShift : { -12.0f, 12.0f })
        {
            for (const auto& [resonance, breath] : bandwidthCorners)
            {
                auto extremeParameters = choirParameters;
                extremeParameters.vowel = vocalor::Vowel::Aah;
                extremeParameters.vowelMorph = 1.0f;
                extremeParameters.vowelX = point.x;
                extremeParameters.vowelY = point.y;
                extremeParameters.formantShift = formantShift;
                extremeParameters.resonance = resonance;
                extremeParameters.breath = breath;
                for (const int midiNote : { 84, 85, 127 })
                {
                    const auto extreme = probe(
                        sampleRate, extremeParameters, midiNote);
                    expect(extreme.size() == 12,
                           "the extreme-register vowel-pad probe lost a singer");
                    for (const auto& singer : extreme)
                        expectUpperOrder(singer.tract);
                }
            }
        }
    }

    // Safe endpoints are not sufficient when cavity centres and widths move at
    // different articulation rates. Drive the known worst identity geometry
    // from a narrow back vowel into broad EE and inspect every rendered chunk,
    // so the realised-pole projection is covered as well as its settled target.
    auto movingParameters = choirParameters;
    movingParameters.vowel = vocalor::Vowel::Ooh;
    movingParameters.formantShift = -12.0f;
    movingParameters.resonance = 1.0f;
    movingParameters.breath = 0.0f;
    auto moving = std::make_unique<vocalor::VoiceEngine>();
    moving->prepare(sampleRate, blockSize);
    moving->reset();
    moving->setParameters(movingParameters);
    moving->noteOn(85, 0.80f);
    render(*moving, static_cast<int>(0.5 * sampleRate));
    const auto ee = vocalor::cardinalVowelPosition(0);
    movingParameters.vowel = vocalor::Vowel::Aah;
    movingParameters.vowelMorph = 1.0f;
    movingParameters.vowelX = ee.x;
    movingParameters.vowelY = ee.y;
    movingParameters.resonance = 0.0f;
    movingParameters.breath = 1.0f;
    moving->setParameters(movingParameters);
    const int transitionChunks = static_cast<int>(
        0.5 * sampleRate / vocalor::VoiceEngineTestAccess::chunkSize);
    for (int chunk = 0; chunk < transitionChunks; ++chunk)
    {
        render(*moving, vocalor::VoiceEngineTestAccess::chunkSize);
        const auto movingVoices = vocalor::VoiceEngineTestAccess::singerTracts(
            *moving, 85);
        expect(movingVoices.size() == 12,
               "the moving upper-pole order probe lost a singer");
        for (const auto& singer : movingVoices)
            expectUpperOrder(singer.tract);
    }

    // C-sharp6 is the end of the smooth evidence-range saturation. Above it,
    // sounding F0 and the old common highTune continue to rise, but the added
    // R3/R4 displacement must not. Divide that common motion out with R5.
    const auto cappedVoices = probe(sampleRate, femaleParameters, 85);
    const auto beyondVoices = probe(sampleRate, femaleParameters, 127);
    expect(cappedVoices.size() == 1 && beyondVoices.size() == 1,
           "the soprano rise-cap probe lost its solo voice");
    if (!cappedVoices.empty() && !beyondVoices.empty())
    {
        const auto displacement = [&femaleLow](const Tract& tract, int formant)
        {
            const float commonTune = tract.hz[4] / femaleLow.hz[4];
            const auto index = static_cast<std::size_t>(formant);
            return tract.hz[index] - commonTune * femaleLow.hz[index];
        };
        for (int formant : { 2, 3 })
            expect(std::abs(displacement(cappedVoices.front().tract, formant)
                                - displacement(beyondVoices.front().tract, formant))
                       < 0.5f,
                   "the soprano register regression extrapolated beyond its cap");
    }

    const auto oneTract = [](const vocalor::VoiceEngine& engine, int midiNote)
    {
        const auto voices = vocalor::VoiceEngineTestAccess::singerTracts(
            engine, midiNote);
        return voices.empty() ? SingerSnapshot {} : voices.front();
    };

    // A slow portamento must carry the resonances with its current intentional
    // F0 rather than applying the destination note's entire rise immediately.
    auto glideParameters = femaleParameters;
    glideParameters.legato = true;
    glideParameters.glide = 1.0f;
    auto glided = std::make_unique<vocalor::VoiceEngine>();
    glided->prepare(sampleRate, blockSize);
    glided->reset();
    glided->setParameters(glideParameters);
    glided->noteOn(lowMidi, 0.80f);
    render(*glided, static_cast<int>(sampleRate * 0.8));
    glided->noteOn(highMidi, 0.80f);
    render(*glided, static_cast<int>(sampleRate * 0.08));
    const auto duringGlide = oneTract(*glided, highMidi);
    expect(duringGlide.intentionalFundamental > lowFundamental
               && duringGlide.intentionalFundamental < 500.0f,
           "the slow-glide R3/R4 fixture did not remain in its lower register");
    for (int formant : { 2, 3 })
    {
        const auto index = static_cast<std::size_t>(formant);
        expect(duringGlide.tract.hz[index] < femaleLow.hz[index]
                   + 0.45f * (femaleHigh.hz[index] - femaleLow.hz[index]),
               "the soprano resonances arrived before the portamento pitch");
    }
    // The maximum Glide setting is a 600 ms exponential. Seven time constants
    // leave less than one hertz of the full 300+ Hz resonance journey.
    render(*glided, static_cast<int>(sampleRate * 4.2));
    const auto afterGlide = oneTract(*glided, highMidi).tract;
    expect(std::abs(afterGlide.hz[2] - femaleHigh.hz[2]) < 1.0f
               && std::abs(afterGlide.hz[3] - femaleHigh.hz[3]) < 1.0f,
           "R3/R4 did not finish the intentional pitch glide");

    // Pitch bend is intentional pitch too. Bending B-flat5 to C4 must recover
    // the C4 tract and return, rather than leaving the rise tied to MIDI note.
    auto bent = std::make_unique<vocalor::VoiceEngine>();
    bent->prepare(sampleRate, blockSize);
    bent->reset();
    bent->setParameters(femaleParameters);
    bent->noteOn(highMidi, 0.80f);
    render(*bent, static_cast<int>(sampleRate * 0.9));
    bent->setPitchBend(-22.0f);
    render(*bent, static_cast<int>(sampleRate * 1.2));
    const auto bentLow = oneTract(*bent, highMidi);
    expect(std::abs(bentLow.intentionalFundamental - lowFundamental) < 0.1f
               && std::abs(bentLow.tract.hz[2] - femaleLow.hz[2]) < 1.0f
               && std::abs(bentLow.tract.hz[3] - femaleLow.hz[3]) < 1.0f,
           "pitch bend left the soprano R3/R4 rise at the MIDI-note register");
    bent->setPitchBend(0.0f);
    render(*bent, static_cast<int>(sampleRate * 1.2));
    const auto bentReturned = oneTract(*bent, highMidi).tract;
    expect(std::abs(bentReturned.hz[2] - femaleHigh.hz[2]) < 1.0f
               && std::abs(bentReturned.hz[3] - femaleHigh.hz[3]) < 1.0f,
           "the soprano R3/R4 rise did not return after pitch bend");

    // Vibrato is deliberately excluded from tractFundamental. Prove that the
    // pitch is moving while the resonances remain stationary.
    auto vibratoParameters = femaleParameters;
    vibratoParameters.vibrato = 1.0f;
    // This fixture isolates whether intentional pitch vibrato drags the tract.
    // Drift now owns deliberate slow vowel motion, so leave it out here.
    vibratoParameters.instability = 0.0f;
    auto vibrato = std::make_unique<vocalor::VoiceEngine>();
    vibrato->prepare(sampleRate, blockSize);
    vibrato->reset();
    vibrato->setParameters(vibratoParameters);
    vibrato->noteOn(79, 0.80f); // G5
    render(*vibrato, static_cast<int>(sampleRate * 1.2));
    float minimumPitch = std::numeric_limits<float>::infinity();
    float maximumPitch = 0.0f;
    std::array<float, 2> minimumFormant {
        std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()
    };
    std::array<float, 2> maximumFormant { 0.0f, 0.0f };
    for (int step = 0; step < 400; ++step)
    {
        render(*vibrato, 64);
        const float pitch = vocalor::VoiceEngineTestAccess::frequencyForRoot(*vibrato, 79);
        const auto tract = oneTract(*vibrato, 79).tract;
        minimumPitch = std::min(minimumPitch, pitch);
        maximumPitch = std::max(maximumPitch, pitch);
        for (int upper = 0; upper < 2; ++upper)
        {
            const auto index = static_cast<std::size_t>(upper + 2);
            minimumFormant[static_cast<std::size_t>(upper)] = std::min(
                minimumFormant[static_cast<std::size_t>(upper)], tract.hz[index]);
            maximumFormant[static_cast<std::size_t>(upper)] = std::max(
                maximumFormant[static_cast<std::size_t>(upper)], tract.hz[index]);
        }
    }
    expect(maximumPitch - minimumPitch > 50.0f,
           "the non-vibrato tract fixture did not contain audible pitch vibrato");
    expect(maximumFormant[0] - minimumFormant[0] < 0.05f
               && maximumFormant[1] - minimumFormant[1] < 0.05f,
           "cycle-by-cycle vibrato dragged the soprano resonances around");

    // Any added register offset must be included before the exact per-voice
    // gains and poles are resolved, at every supported sample rate.
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        const auto voices = probe(rate, femaleParameters, highMidi);
        expect(voices.size() == 1, "the sample-rate R3/R4 probe lost its voice");
        if (voices.empty())
            continue;
        const Tract& tract = voices.front().tract;
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            expect(std::abs(tract.hz[index] - femaleHigh.hz[index])
                       < 0.001f * femaleHigh.hz[index]
                       && std::abs(tract.bandwidth[index] - femaleHigh.bandwidth[index])
                       < 0.001f * femaleHigh.bandwidth[index],
                   "the soprano R3/R4 geometry depends on sample rate");
        }
        std::array<float, vocalor::kFormantCount> expectedGain {};
        vocalor::parallelFormantAmplitudes(
            tract.hz.data(), tract.bandwidth.data(), vocalor::kFormantCount,
            static_cast<float>(rate), 0.010f, expectedGain.data());
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            const float gainScale = std::max(expectedGain[index], 1.0e-6f);
            expect(std::abs(tract.gain[index] - expectedGain[index])
                       <= 2.0e-5f * gainScale,
                   "the R3/R4 rise gain was derived from different poles");
            const float radius = std::exp(-3.14159265358979323846f
                * tract.bandwidth[index] / static_cast<float>(rate));
            const float expectedA1 = 2.0f * radius * std::cos(
                2.0f * 3.14159265358979323846f * tract.hz[index]
                / static_cast<float>(rate));
            expect(std::abs(tract.a1[index] - expectedA1) <= 3.0e-6f
                       && std::abs(tract.a2[index] + radius * radius) <= 2.0e-6f,
                   "the R3/R4 rise was applied after the pole coefficients");
            const float expectedB0 = vocalor::formantPolarity(formant)
                * tract.gain[index] * tract.peakNormaliser[index];
            expect(std::abs(tract.b0[index] - expectedB0) <= 1.0e-7f,
                   "the R3/R4 rise was applied after the rendered gain");
        }
    }

    // The public engine also accepts analysis-rate 8/16 kHz audio. Those rates
    // cannot represent five independently ordered upper vocal-tract modes: the
    // 0.465*fs guard deliberately coalesces whichever centres exceed it. What
    // remains a hard contract is finite audio geometry and one coherent tuple
    // of displayed Hz/BW/gain and rendered pole coefficients after the clamp.
    for (const double rate : { 8000.0, 16000.0 })
    {
        bool reachedNyquistGuard = false;
        for (int cardinal = 0; cardinal < vocalor::kCardinalVowelCount; ++cardinal)
        {
            const auto point = vocalor::cardinalVowelPosition(cardinal);
            auto lowRateParameters = choirParameters;
            lowRateParameters.vowel = vocalor::Vowel::Aah;
            lowRateParameters.vowelMorph = 1.0f;
            lowRateParameters.vowelX = point.x;
            lowRateParameters.vowelY = point.y;
            lowRateParameters.formantShift = 12.0f;
            lowRateParameters.resonance = 0.0f;
            lowRateParameters.breath = 1.0f;
            const auto voices = probe(rate, lowRateParameters, 127);
            expect(voices.size() == 12,
                   "the low-rate soprano clamp probe lost a singer");
            for (const auto& singer : voices)
            {
                const auto& tract = singer.tract;
                std::array<float, vocalor::kFormantCount> expectedGain {};
                vocalor::parallelFormantAmplitudes(
                    tract.hz.data(), tract.bandwidth.data(),
                    vocalor::kFormantCount, static_cast<float>(rate), 0.010f,
                    expectedGain.data());
                for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
                {
                    const auto index = static_cast<std::size_t>(formant);
                    const float upperGuard = 0.465f * static_cast<float>(rate);
                    reachedNyquistGuard = reachedNyquistGuard
                        || std::abs(tract.hz[index] - upperGuard) < 0.1f;
                    expect(std::isfinite(tract.hz[index])
                               && std::isfinite(tract.bandwidth[index])
                               && std::isfinite(tract.gain[index])
                               && std::isfinite(tract.a1[index])
                               && std::isfinite(tract.a2[index])
                               && std::isfinite(tract.b0[index])
                               && std::isfinite(tract.peakNormaliser[index])
                               && tract.hz[index] >= 25.0f
                               && tract.hz[index] <= upperGuard + 0.1f
                               && tract.bandwidth[index] >= 20.0f
                               && tract.bandwidth[index]
                                      <= 0.25f * static_cast<float>(rate) + 0.1f,
                           "the low-rate soprano Nyquist clamp produced invalid geometry");
                    const float radius = std::exp(-3.14159265358979323846f
                        * tract.bandwidth[index] / static_cast<float>(rate));
                    const float expectedA1 = 2.0f * radius * std::cos(
                        2.0f * 3.14159265358979323846f * tract.hz[index]
                        / static_cast<float>(rate));
                    const float expectedB0 = vocalor::formantPolarity(formant)
                        * tract.gain[index] * tract.peakNormaliser[index];
                    const float gainScale = std::max(expectedGain[index], 1.0e-6f);
                    expect(std::abs(tract.gain[index] - expectedGain[index])
                                   <= 2.0e-5f * gainScale
                               && std::abs(tract.a1[index] - expectedA1) <= 3.0e-6f
                               && std::abs(tract.a2[index] + radius * radius) <= 2.0e-6f
                               && std::abs(tract.b0[index] - expectedB0) <= 1.0e-7f,
                           "the low-rate soprano pole did not match its clamped geometry");
                }
            }
        }
        expect(reachedNyquistGuard,
               "the low-rate soprano fixture did not exercise the Nyquist clamp");
    }

    // A connected register crossing must retain the engine's bounded
    // arbitrary-host-buffer residual; chunk-aligned exactness is covered by
    // the shared soprano tract fixture immediately above this test.
    auto splitParameters = femaleParameters;
    splitParameters.legato = true;
    splitParameters.glide = 0.72f;
    auto contiguousEngine = std::make_unique<vocalor::VoiceEngine>();
    auto splitEngine = std::make_unique<vocalor::VoiceEngine>();
    for (auto* engine : { contiguousEngine.get(), splitEngine.get() })
    {
        engine->prepare(sampleRate, blockSize);
        engine->reset();
        engine->setParameters(splitParameters);
        engine->noteOn(lowMidi, 0.80f);
    }
    renderInterleaved(*contiguousEngine, 24000, blockSize);
    renderInterleaved(*splitEngine, 24000, 37);
    contiguousEngine->noteOn(highMidi, 0.80f);
    splitEngine->noteOn(highMidi, 0.80f);
    const auto contiguous = renderInterleaved(
        *contiguousEngine, static_cast<int>(sampleRate), blockSize);
    const auto split = renderInterleaved(
        *splitEngine, static_cast<int>(sampleRate), 37);
    float largestResidual = 0.0f;
    for (std::size_t sample = 0; sample < contiguous.size(); ++sample)
        largestResidual = std::max(largestResidual,
            std::abs(contiguous[sample] - split[sample]));
    std::cout << "soprano R3/R4 glide buffer residual: " << std::scientific
              << largestResidual << std::fixed << "\n";
    expect(largestResidual < 2.0e-5f,
           "the soprano R3/R4 glide depends on host buffer splits");
}

/** Energy in [lowHz, highHz) of a Blackman-Harris window of @c length samples
    starting at @c start, in dB. Per-bin DFT: the windows are 25 ms, which is
    1200 bins at 48 kHz, and the bands are narrow, so this costs less than the
    renders it measures. */
double windowedBandDb (const std::vector<float>& samples, int start, int length,
                       double sampleRate, double lowHz, double highHz)
{
    constexpr double pi = 3.14159265358979323846;
    std::vector<double> window (static_cast<std::size_t> (length));
    for (int i = 0; i < length; ++i)
    {
        const auto angle = 2.0 * pi * i / (length - 1.0);
        window[static_cast<std::size_t> (i)] = 0.35875 - 0.48829 * std::cos (angle)
                                             + 0.14128 * std::cos (2.0 * angle)
                                             - 0.01168 * std::cos (3.0 * angle);
    }

    const auto binHz = sampleRate / length;
    const auto firstBin = std::max (1, static_cast<int> (std::ceil (lowHz / binHz)));
    const auto lastBin = std::min (length / 2 - 1, static_cast<int> (std::floor (highHz / binHz)));
    double total = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const auto value = samples[static_cast<std::size_t> (start + i)]
                             * window[static_cast<std::size_t> (i)];
            const auto angle = 2.0 * pi * bin * i / length;
            real += value * std::cos (angle);
            imaginary -= value * std::sin (angle);
        }
        total += real * real + imaginary * imaginary;
    }
    return 10.0 * std::log10 (std::max (total, 1.0e-300));
}

/** A vocal tract is a fixed geometry with fixed poles, so it is fully formed
    before the first glottal pulse reaches it and the singer's-formant cluster
    is present in the first cycle. The engine used to render the first 50-70 ms
    through a two-formant stage and lerp the full bank in over the following
    65-125 ms, which left the 2000-3300 Hz band 17.68 dB below its sustain share
    at 10-35 ms on a male D3.

    What does develop at an onset is the source: the folds start abducted and
    lax and adduct over the first tens of milliseconds. That is the second half
    of this test, and it is measured separately, because a ramp that does
    nothing is otherwise indistinguishable from a ramp that works.
*/
void testOnsetSpectrum()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int window = 1200;              // 25 ms
    const auto at = [] (double milliseconds)
    {
        return static_cast<int> (sampleRate * milliseconds * 0.001);
    };

    // The published baseline for gap 9 was measured on the engine's own shipped
    // defaults rather than on steadyParameters(), which runs a narrower
    // resonance and a wider spread; keeping to the defaults is what makes the
    // numbers quoted below comparable with the ones in the plan.
    const auto onsetParameters = [] (vocalor::VoiceProfile profile, float tension)
    {
        vocalor::EngineParameters parameters;
        parameters.profile = profile;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vowel = vocalor::Vowel::Aah;
        parameters.tension = tension;
        parameters.breath = 0.10f;
        parameters.humanize = 0.0f;
        parameters.vibrato = 0.0f;
        parameters.room = 0.0f;
        return parameters;
    };

    /** A male AAH on D3 at Tension 0.90, Breath 0.10, Humanize 0, Vibrato 0,
        either at the shipping source-tension ramp depth or with that ramp
        forced out. The ramp-on leg does not name a depth, so a depth shipped at
        zero fails here rather than passing quietly. */
    const auto probe = [&] (bool rampOff)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (onsetParameters (vocalor::VoiceProfile::Male, 0.90f));
        if (rampOff)
            vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth (engine, 0.0f);
        engine.noteOn (50, 0.80f);
        return renderMono (engine, static_cast<int> (sampleRate * 1.2));
    };

    const auto rampOff = probe (true);
    const auto rampOn = probe (false);

    const auto share = [&] (const std::vector<float>& samples, double milliseconds,
                            double lowHz, double highHz)
    {
        return windowedBandDb (samples, at (milliseconds), window, sampleRate, lowHz, highHz)
             - windowedBandDb (samples, at (milliseconds), window, sampleRate, 100.0, 900.0);
    };
    /** How far the singer's-formant band sits below the share it reaches in the
        sustain, in dB. This is gap 9's own measurement. */
    const auto deficit = [&] (const std::vector<float>& samples, double milliseconds)
    {
        return share (samples, 1000.0, 2000.0, 3300.0)
             - share (samples, milliseconds, 2000.0, 3300.0);
    };

    const auto offDeficit = deficit (rampOff, 10.0);
    const auto onDeficit = deficit (rampOn, 10.0);
    const auto offAir = share (rampOff, 10.0, 5000.0, 18000.0);
    const auto onAir = share (rampOn, 10.0, 5000.0, 18000.0);
    std::cout << "onset: 2-3.3 kHz deficit at 10-35 ms " << std::fixed << std::setprecision (2)
              << offDeficit << " dB with the source ramp off, " << onDeficit
              << " dB with it on; 5-18 kHz share " << offAir << " -> " << onAir << " dB\n";

    // The tract is present from sample zero. With the source ramp switched out
    // nothing is left to develop but the envelope, so the band the singer's
    // formant sits in has to arrive with the note. Measured 2.71 dB, against
    // 17.68 dB for the two-formant onset stage this replaces. The coherent LF
    // bank reads 3.44 dB here; the earlier 2.71 included an endpoint-phase
    // cancellation that was not a property of an intermediate glottal pulse.
    expect (offDeficit <= 4.5,
            "the upper formants are still missing from the attack");

    // And the gap stays closed once the source ramp is doing its work.
    // Measured 3.96 dB with the coherent source bank.
    expect (onDeficit <= 5.0,
            "the source-tension ramp reopened the singer's-formant gap at the onset");

    // The ramp is doing the remaining work. A lax fold configuration is an
    // abducted one, so the note has to speak with more aspiration per unit of
    // voiced output than the same note started at the block's tension.
    // Measured 0.94 dB. The earlier 5.56 dB included the phase-misaligned
    // endpoint crossfade's missing voiced harmonics; the positive direction is
    // the physical contract, while the separate source-bank test now owns the
    // harmonic shape.
    expect (onAir - offAir >= 0.5,
            "the source-tension ramp does not make the onset breathier than a note "
            "started at the block's tension");

    // It is an onset gesture, so by the sustain the two renders have to be the
    // same instrument. Measured within 0.001 dB in all three bands.
    for (const auto& band : { std::pair { 100.0, 900.0 }, std::pair { 2000.0, 3300.0 },
                              std::pair { 5000.0, 18000.0 } })
    {
        const auto offBand = windowedBandDb (rampOff, at (1000.0), window, sampleRate,
                                             band.first, band.second);
        const auto onBand = windowedBandDb (rampOn, at (1000.0), window, sampleRate,
                                            band.first, band.second);
        expect (std::abs (onBand - offBand) <= 0.5,
                "the source-tension ramp is still moving the sustain a second into the note");
    }

    // Direction test only: the aspiration-to-voiced ratio falls across the
    // onset rather than rising. Measured 25.73 -> 12.27 -> 4.79 dB above the
    // sustain share.
    const auto airEarly = share (rampOn, 10.0, 5000.0, 18000.0);
    const auto airMiddle = share (rampOn, 70.0, 5000.0, 18000.0);
    const auto airLate = share (rampOn, 180.0, 5000.0, 18000.0);
    expect (airEarly > airMiddle && airMiddle > airLate,
            "the aspiration no longer falls away across the onset");

    // And it must not click. What a click is, is energy in the note's first
    // instants that the envelope did not put there -- the tract's own transient
    // response to a source that starts from silence. The window has to be
    // shorter than the fastest attack the engine produces or it measures the
    // attack instead: velocity now sets the envelope time constant and an
    // accented note reaches amplitude on 4.5 ms, so the 2 ms window this test
    // shipped with is a third of the way up the intended envelope rather than
    // ahead of it. 1 ms is under a quarter of that time constant and under a
    // third of a glottal period at C4. Measured, the narrowest of these
    // eighteen is 40.03 dB.
    struct Entry { int midi; vocalor::VoiceProfile profile; const char* name; };
    for (const auto entry : { Entry { 50, vocalor::VoiceProfile::Male, "male D3" },
                              Entry { 60, vocalor::VoiceProfile::Female, "female C4" },
                              Entry { 84, vocalor::VoiceProfile::Female, "female C6" } })
    {
        for (const float tension : { 0.30f, 0.90f })
        {
            // Across the velocity range, because velocity is what sets the
            // attack: the loudest, fastest onset is the one that can click.
            for (const float velocity : { 0.30f, 0.80f, 1.00f })
            {
                vocalor::VoiceEngine engine;
                engine.prepare (sampleRate, blockSize);
                engine.reset();
                engine.setParameters (onsetParameters (entry.profile, tension));
                engine.noteOn (entry.midi, velocity);
                const auto samples = renderMono (engine, static_cast<int> (sampleRate * 1.2));

                double firstPeak = 0.0;
                for (int i = 0; i < at (1.0); ++i)
                    firstPeak = std::max (firstPeak,
                                          std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)])));
                double sustainPeak = 0.0;
                for (int i = at (900.0); i < at (1100.0); ++i)
                    sustainPeak = std::max (sustainPeak,
                                            std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)])));
                const auto below = 20.0 * std::log10 (sustainPeak / std::max (firstPeak, 1.0e-30));
                std::cout << "onset: " << entry.name << " at Tension " << std::setprecision (2)
                          << tension << ", velocity " << velocity << " enters " << below
                          << " dB below its sustain peak\n";
                expect (below >= 24.0,
                        std::string (entry.name) + " starts with a click: its first 1 ms peak "
                            "is within 24 dB of the sustain peak");
            }
        }
    }
}

/** Solo, one note, nothing moving but the thing under test. The published
    baselines for gaps 11 and 21 were measured on the engine's own shipped
    defaults rather than on steadyParameters(), so these two tests build from
    EngineParameters directly, as testOnsetSpectrum does. */
vocalor::EngineParameters dynamicProtocolParameters()
{
    vocalor::EngineParameters parameters;
    parameters.mode = vocalor::PerformanceMode::Solo;
    parameters.vowel = vocalor::Vowel::Aah;
    parameters.humanize = 0.0f;
    parameters.vibrato = 0.0f;
    parameters.room = 0.0f;
    parameters.dynamics = 1.0f;
    return parameters;
}

/** 10-90 % rise of a four-period sliding-peak envelope, in milliseconds.

    Four periods rather than a fixed window: a 2 ms window at C4 is shorter than
    the 3.82 ms glottal period, so it does not smooth the waveform out at all
    and the crossings land on within-period ripple -- the same rise reads 7.8 ms
    at 2 ms, 13.2 at 8 ms and 27.8 at 30 ms, which is a metric whose answer is
    set by its own window length. The envelope is the peak of the four periods
    ending at each sample, sampled on a 0.5 ms grid, and the two crossings are
    interpolated linearly between grid points. The steady value is the envelope
    averaged over 0.8-1.0 s, which is well past any attack this engine produces.
*/
double onsetRiseMs (const std::vector<float>& samples, double fundamentalHz,
                    double sampleRate)
{
    const auto window = static_cast<int> (std::lround (4.0 * sampleRate / fundamentalHz));
    const auto count = static_cast<int> (samples.size());
    std::vector<double> envelope (static_cast<std::size_t> (count), 0.0);
    // A running maximum over a sliding window, kept O(n) by rescanning only when
    // the sample that held the maximum falls out of it.
    double running = 0.0;
    int runningAt = -1;
    for (int i = 0; i < count; ++i)
    {
        const auto magnitude = std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)]));
        if (magnitude >= running)
        {
            running = magnitude;
            runningAt = i;
        }
        else if (runningAt <= i - window)
        {
            running = 0.0;
            for (int k = std::max (0, i - window + 1); k <= i; ++k)
            {
                const auto value = std::abs (static_cast<double> (samples[static_cast<std::size_t> (k)]));
                if (value >= running)
                {
                    running = value;
                    runningAt = k;
                }
            }
        }
        envelope[static_cast<std::size_t> (i)] = running;
    }

    const auto from = static_cast<int> (0.8 * sampleRate);
    const auto to = std::min (count, static_cast<int> (1.0 * sampleRate));
    double steady = 0.0;
    for (int i = from; i < to; ++i)
        steady += envelope[static_cast<std::size_t> (i)];
    steady /= std::max (1, to - from);

    const auto step = static_cast<int> (0.0005 * sampleRate);
    const auto crossing = [&] (double fraction)
    {
        const auto target = fraction * steady;
        for (int i = step; i < count; i += step)
        {
            const auto before = envelope[static_cast<std::size_t> (i - step)];
            const auto after = envelope[static_cast<std::size_t> (i)];
            if (before < target && after >= target)
            {
                const auto within = (target - before) / std::max (after - before, 1.0e-30);
                return 1000.0 * (static_cast<double> (i - step) + within * step) / sampleRate;
            }
        }
        return -1.0;
    };
    return crossing (0.9) - crossing (0.1);
}

/** Energy in [lowHz, highHz) of a held note's sustain, in dB. Two 100 ms
    windows, because at Humanize 0 and Vibrato 0 the sustain is stationary. */
double sustainBandDb (const std::vector<float>& samples, double sampleRate,
                      double lowHz, double highHz)
{
    const auto window = static_cast<int> (0.100 * sampleRate);
    double total = 0.0;
    for (const double at : { 0.80, 1.00 })
        total += windowedBandDb (samples, static_cast<int> (at * sampleRate), window,
                                 sampleRate, lowHz, highHz);
    return 0.5 * total;
}

/** How many decibels the 2-5 kHz band moves per decibel of 150-800 Hz between
    two renders. Sundberg's measurement is that partials above 1 kHz rise about
    twice as fast in dB as overall sound pressure level, so this is 2 in a real
    singer and 1 in a fader. Read at Breath 0.00: at any breath setting the
    aspiration, which loses only 7.2 dB across the dynamic, floors the 2-5 kHz
    band once the voiced component has fallen 30 dB, and the ratio then measures
    the noise rather than the source. */
double presenceRatio (const std::vector<float>& soft, const std::vector<float>& loud,
                      double sampleRate)
{
    const auto low = sustainBandDb (loud, sampleRate, 150.0, 800.0)
                   - sustainBandDb (soft, sampleRate, 150.0, 800.0);
    const auto high = sustainBandDb (loud, sampleRate, 2000.0, 5000.0)
                    - sustainBandDb (soft, sampleRate, 2000.0, 5000.0);
    std::cout << "  150-800 Hz " << std::fixed << std::setprecision (2) << low
              << " dB, 2-5 kHz " << high << " dB, ratio " << high / low << '\n';
    return high / low;
}

/** Velocity has to shape the note, not fade it.

    Velocity used to reach a gain and the corner of a one-pole source tilt and
    nothing else. In particular it never reached the envelope: attackCoefficient_
    was a per-block constant derived from Humanize alone, so a held C4 rose in
    the same 16.6 ms at velocity 0.05, 0.40 and 1.00 alike, across a 22 dB level
    span. A soft onset has to approach phonation threshold pressure slowly and
    an accented one arrives above it, so the time constant is now the note's own.
*/
void testVelocityShapesOnset()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 261.6255653;   // C4

    const auto riseAt = [&] (float velocity, float humanize)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.humanize = humanize;
        engine.setParameters (parameters);
        engine.noteOn (60, velocity);
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 1.5));
        return onsetRiseMs (samples, fundamental, sampleRate);
    };

    const auto accented = riseAt (1.00f, 0.0f);
    const auto soft = riseAt (0.10f, 0.0f);
    std::cout << "onset rise at Humanize 0: velocity 1.00 " << std::fixed
              << std::setprecision (2) << accented << " ms, velocity 0.10 " << soft
              << " ms, ratio " << soft / accented << "x\n";

    // Two-sided, because both ends carry a defect. Below about 6 ms an accented
    // attack stops being an attack and becomes a click; above about 120 ms a
    // soft one stops being an onset and becomes a pad swell. Today's engine
    // fails both: 16.62 ms is over the accented ceiling and under the soft
    // floor. Measured 8.91 ms and 92.16 ms.
    expect (accented >= 6.0 && accented <= 14.0,
            "an accented attack does not reach amplitude in the 6-14 ms a hard "
            "onset takes");
    expect (soft >= 45.0 && soft <= 120.0,
            "a soft attack does not take the 45-120 ms an onset near phonation "
            "threshold takes");
    expect (soft / accented >= 2.5,
            "velocity is still not the envelope's time constant");

    // Humanize published as the dial that loosens a take, and the attack time is
    // one of the things it loosens. It survives as a multiplier on the time
    // constant rather than as its source. Measured 8.91 -> 23.53 ms.
    const auto loose = riseAt (1.00f, 1.0f);
    std::cout << "onset rise at velocity 1.00: Humanize 0 " << accented
              << " ms, Humanize 1 " << loose << " ms\n";
    expect (loose >= 2.0 * accented,
            "Humanize no longer stretches the attack it is published as loosening");

    // Velocity also reaches the source-tension ramp, so a soft attack is lax as
    // well as slow: the folds of a hard onset start close to the adducted
    // configuration they settle on, a soft one starts further from it. The
    // reference is velocity 0.80, which is what leaves the onset the first pass
    // measured exactly where it was.
    const auto sagAt = [&] (float velocity)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (dynamicProtocolParameters());
        engine.noteOn (60, velocity);
        render (engine, blockSize);
        return vocalor::VoiceEngineTestAccess::sourceTensionSag (engine);
    };
    const auto softSag = sagAt (0.10f);
    const auto referenceSag = sagAt (0.80f);
    const auto hardSag = sagAt (1.00f);
    std::cout << "source-tension ramp depth: velocity 0.10 " << softSag
              << ", 0.80 " << referenceSag << ", 1.00 " << hardSag << '\n';
    const vocalor::VoiceEngine untouched;
    expect (std::abs (referenceSag
                          - vocalor::VoiceEngineTestAccess::sourceTensionRampDepth (untouched))
                <= 0.01f,
            "the reference velocity no longer resolves to the engine's own ramp depth");
    // The positivity term is not redundant: a ramp shipped at depth zero makes
    // all three sags zero, and a bare ratio is satisfied by 0 >= 0.
    expect (hardSag > 0.0f && softSag >= 1.3f * hardSag,
            "velocity does not reach the source-tension ramp: a soft attack is "
            "slow but not lax");

    // And velocity has to reach the spectrum, not only the level.
    const auto renderAt = [&] (float velocity)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.breath = 0.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, velocity);
        return renderMono (engine, static_cast<int> (sampleRate * 1.3));
    };
    std::cout << "velocity 0.05 -> 1.00:\n";
    const auto ratio = presenceRatio (renderAt (0.05f), renderAt (1.00f), sampleRate);
    // 1.066 before this step: velocity was a fader with a very slight tilt on
    // it. Measured 1.48. Two cascaded first-order shelves cannot reach the 2.0
    // Sundberg measures across these two bands -- 2-5 kHz is inside the shelf's
    // own transition, not its plateau, so this reading understates the law by
    // construction; testTheSourceShelfAppliesItsGainOnce measures it where the
    // plateau has been reached. The bound is set where the mechanism actually
    // lands with margin rather than at the physiology.
    expect (ratio >= 1.40,
            "velocity still moves the presence band no faster than the level");
}

/** ... and it applies that gain once, not once per shelf stage.

    The law is Sundberg's: the partials above 1 kHz fall by twice as many
    decibels as the level does, so the shelf's own plateau has to be the note's
    broadband gain exactly once - the level carries the first factor and the
    shelf the second. The shelf is two cascaded first-order stages, and it is
    there for the slope, because one stage cannot move 3 kHz far enough from
    450 Hz. A stage that carried the whole gain would put the square of it above
    the corner and make the band fall three times as fast, not twice. At
    velocity 0.05 that is a plateau of -57.2 dB against the -28.6 the law asks
    for.

    The reading is taken high enough that the shelf has reached its plateau -
    8-16 kHz against a corner at 850 Hz - and referred to the fundamental rather
    than to a 150-800 Hz band, because the shelf is only unity at DC and a band
    that reaches 800 Hz is already inside its transition. Tension sits at 1.00
    and the two velocities are 0.60 and 1.00 so that the source tilt, which is
    the other velocity-dependent brightness term, stays near its own ceiling and
    contributes under a decibel of the reading. */
void testTheSourceShelfAppliesItsGainOnce()
{
    constexpr auto sampleRate = 48000.0;

    const auto renderAt = [&] (float velocity)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.breath = 0.0f;
        parameters.vibrato = 0.0f;
        parameters.humanize = 0.0f;
        parameters.room = 0.0f;
        parameters.tension = 1.0f;
        parameters.dynamics = 1.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, velocity);
        return renderMono (engine, static_cast<int> (sampleRate * 1.3));
    };

    // Four first-order differences before the high reading is taken. It is
    // taken thirty harmonics above the fundamental and a hundred decibels under
    // it, which is where the Blackman-Harris window sustainBandDb uses stops
    // measuring the signal and starts measuring itself: its sidelobes are flat
    // at -92 dB rather than rolling off, so at that separation the leakage from
    // the fundamental sits about 10 dB *above* a soft note's own 8-16 kHz
    // content and floors the reading. Differencing tilts the spectrum 24 dB per
    // octave before the window sees it, putting a 261 Hz fundamental 129 dB
    // under 12 kHz. Both renders get the same filter, so every difference
    // between them survives it unchanged.
    const auto preEmphasised = [] (std::vector<float> samples)
    {
        for (int pass = 0; pass < 4; ++pass)
            for (std::size_t i = samples.size(); i-- > 1;)
                samples[i] -= samples[i - 1];
        return samples;
    };

    const auto soft = renderAt (0.60f);
    const auto loud = renderAt (1.00f);
    const auto level = sustainBandDb (loud, sampleRate, 200.0, 330.0)
                     - sustainBandDb (soft, sampleRate, 200.0, 330.0);
    const auto plateau = sustainBandDb (preEmphasised (loud), sampleRate, 8000.0, 16000.0)
                       - sustainBandDb (preEmphasised (soft), sampleRate, 8000.0, 16000.0);
    const auto law = plateau / level;
    std::cout << "shelf law, velocity 0.60 -> 1.00: fundamental " << std::fixed
              << std::setprecision (2) << level << " dB, 8-16 kHz " << plateau
              << " dB, ratio " << law << '\n';

    // Measured 2.05. With the gain applied once per stage it is 2.95, which is
    // the cube the two-stage cascade produces and the number this bound exists
    // to exclude.
    expect (law > 1.70 && law < 2.40,
            "the band above the shelf corner falls " + std::to_string (law)
                + " times as fast as the fundamental, against the 2 the source's"
                  " own loudness law asks for: the shelf is not applying the"
                  " note's broadband gain exactly once");
}

/** The dynamic control has to cover a singer's range, and stay a spectrum
    control at the bottom of it.

    dynamicResponse's voiced gain was exp2(-3.00 * below), which is exactly
    18.06 dB by construction, against the 30-40 dB a singer covers between
    pianissimo and fortissimo. The aspiration only loses 7.2 dB over the same
    span, so once the voiced gain moves past about 35 dB the noise floors the
    result and the control stops working at its own bottom end -- which is why
    the span is measured at three breath settings rather than one.
*/
void testDynamicRange()
{
    constexpr auto sampleRate = 48000.0;

    const auto spanAt = [&] (float breath)
    {
        std::array<double, 2> level {};
        for (int end = 0; end < 2; ++end)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = dynamicProtocolParameters();
            parameters.breath = breath;
            parameters.outputGain = 1.0f;
            parameters.dynamics = end == 0 ? 0.0f : 1.0f;
            engine.setParameters (parameters);
            engine.noteOn (60, 0.80f);
            render (engine, static_cast<int> (sampleRate * 0.8));
            const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
            level[static_cast<std::size_t> (end)] = 20.0 * std::log10 (held.rms() + 1.0e-30);
        }
        const auto span = level[1] - level[0];
        std::cout << "dynamic span at Breath " << std::fixed << std::setprecision (2)
                  << breath << ": " << span << " dB\n";
        return span;
    };

    const auto dry = spanAt (0.00f);
    const auto shipped = spanAt (0.28f);
    const auto breathy = spanAt (0.60f);

    // Measured 34.02, 33.86 and 33.22 dB, against 18.10 / 18.10 / 18.09 before.
    expect (shipped >= 28.0,
            "the dynamic control does not cover a singer's range");
    // Pinned at the shipping breath, because the aspiration is what would take
    // the bottom of the range away without any of it showing up at Breath 0.
    expect (std::abs (breathy - dry) <= 2.0,
            "the aspiration is flooring the bottom of the dynamic range");

    // And the dynamic has to be a spectrum control across that whole span, not
    // a 30 dB fader. 1.195 before this step; measured 1.53. Read in the shelf's
    // transition region for the same reason the velocity ratio above is.
    const auto renderAt = [&] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.breath = 0.0f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        return renderMono (engine, static_cast<int> (sampleRate * 1.3));
    };
    std::cout << "dynamics 0.00 -> 1.00:\n";
    expect (presenceRatio (renderAt (0.00f), renderAt (1.00f), sampleRate) >= 1.40,
            "the dynamic still moves the presence band no faster than the level");
}

/** A released note is a laryngeal gesture, not the removal of a drive.

    At an aspirate offset the folds abduct: transglottal flow continues while
    the oscillation stops, so the voiced component dies first and the turbulent
    one outlives it, and the note "tapers from voice into breath". At a glottal
    offset the note ends "while the folds are still approximated", the flow is
    choked, and the breath goes with the voice. Which one a note gets is not a
    preference; it is the phonation the note was already in.

    The engine used to square the air envelope against the voiced one, so every
    note got cleaner as it died. On the Breath 1.00, Tension 0.15, Humanize 0.60
    patch the air-to-voiced ratio 300 ms after note-off measured 12.53 dB
    *below* the ratio the note was sounding at.

    The reference is the last 25 ms of the held note rather than the first 25 ms
    after note-off: the offset gesture runs on a 50 ms time constant, so a
    window that starts at the note-off already contains half a time constant of
    the thing being measured. On the engine this replaced the two readings agree
    to 0.33 dB, because nothing about the old release moved at the note-off.
*/
void testReleaseAerodynamics()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int window = 1200;              // 25 ms
    constexpr double c4 = 261.6255653;

    /** Air-to-voiced ratio in dB over the 25 ms window starting at @c start:
        harmonics 1-12 of C4 in +/-45 Hz bands against 5-18 kHz. */
    const auto ratioAt = [&] (const std::vector<float>& samples, int start)
    {
        const auto air = std::pow (10.0, windowedBandDb (samples, start, window,
                                                         sampleRate, 5000.0, 18000.0) / 10.0);
        double voiced = 0.0;
        for (int harmonic = 1; harmonic <= 12; ++harmonic)
        {
            const auto centre = c4 * harmonic;
            voiced += std::pow (10.0, windowedBandDb (samples, start, window, sampleRate,
                                                      centre - 45.0, centre + 45.0) / 10.0);
        }
        return 10.0 * std::log10 (std::max (air, 1.0e-300) / std::max (voiced, 1.0e-300));
    };

    struct Offset
    {
        double sounding;
        double after300;
        double tailSeconds;
    };

    const auto offsetAt = [&] (float breath, float tension)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.vibrato = 0.0f;
        parameters.room = 0.0f;
        parameters.breath = breath;
        parameters.tension = tension;
        parameters.humanize = 0.60f;
        engine.setParameters (parameters);

        engine.noteOn (60, 0.80f);
        const auto held = renderMono (engine, static_cast<int> (sampleRate * 1.0));
        engine.noteOff (60);
        const auto release = renderMono (engine, static_cast<int> (sampleRate * 0.5));

        double tail = 0.5;
        while (tail < 12.0 && engine.getActiveVoiceCount() > 0)
        {
            renderMono (engine, static_cast<int> (sampleRate * 0.1));
            tail += 0.1;
        }

        const Offset result {
            ratioAt (held, static_cast<int> (held.size()) - window),
            ratioAt (release, static_cast<int> (sampleRate * 0.300)),
            tail
        };
        std::cout << "offset at Breath " << std::fixed << std::setprecision (2) << breath
                  << ", Tension " << tension << ": sounding " << result.sounding
                  << " dB, +300 ms " << result.after300 << " dB, change "
                  << (result.after300 - result.sounding) << " dB, tail "
                  << std::setprecision (1) << result.tailSeconds << " s\n";
        return result;
    };

    // Aspirate offset. Measured +9.56 dB, against -12.91 dB with the gesture
    // reverted. (The pair read +9.93 and -12.53 dB when this step landed; step
    // 5's pitch-synchronous window rides the same airShape the abduction
    // multiplies, so it moved both ends of the ratio and left the size of the
    // effect the same to 0.05 dB.)
    const auto aspirate = offsetAt (1.00f, 0.15f);
    expect (aspirate.after300 - aspirate.sounding >= 6.0,
            "a breathy, lax note still gets cleaner as it dies instead of breathier");

    // Glottal offset, at Breath 0.28 rather than 0.10: below about a fifth of
    // the Breath range the 5-18 kHz band is not purely air, and the voiced
    // floor -- the wavetable's own harmonics, 82.30 dB down at Breath 0.00 --
    // overtakes the aspiration inside the release. At Breath 0.28 the margin
    // over that floor is 35.01 dB. Measured -6.21 dB. What this bound catches is
    // an abduction gesture applied blind to the adduction the note was in: with
    // abductionTarget forced to 4 for every note the pressed leg reads
    // +13.25 dB and this assertion fails.
    const auto glottal = offsetAt (0.28f, 0.90f);
    expect (glottal.after300 - glottal.sounding <= 2.0,
            "a pressed note no longer stops cleanly");

    // The breath may not be bought with a tail that never frees its voice.
    // Both components now decay on the voiced time constant, so the release
    // ends where it always did: measured 2.1 s on both legs.
    expect (aspirate.tailSeconds <= 3.8 && glottal.tailSeconds <= 3.8,
            "the aspirate offset left its voice sounding past the advertised tail");
}

/** Aspiration turbulence is generated by flow through the glottal
    constriction, so it has the period of the voice.

    Hermes reports that stationary noise "is to a large extent perceived as
    coming from a separate sound source which hardly contributes to the breathy
    timbre of the vowel", and that the fix is "noise with a temporal envelope of
    the same periodicity as the pulse train"; Klatt and Klatt use
    pitch-synchronous amplitude-modulated noise for the same reason. Vocalor's
    aspiration drive carried no glottal-phase term at all -- the modulation was
    zero by construction, not by measurement -- so the breath sat behind the
    voice instead of inside it.

    Sized honestly this is a breathy-preset fix: the isolated aspiration is
    28.15 dB below the full signal at Breath 1.00 and 41.07 dB below at the 28 %
    shipping default, so it is the Breath axis that moves and not the default
    patch.
*/
void testAspirationIsPitchSynchronous()
{
    constexpr auto sampleRate = 48000.0;
    // Short blocks so the per-sample phase can be reconstructed from the state
    // read at each boundary. The increment is constant here anyway -- Vibrato,
    // Humanize, Glide and Intonation are all zero -- but a short block keeps
    // the fold exact if that ever stops being true.
    constexpr int foldBlock = 64;
    constexpr int bins = 24;
    constexpr double settleSeconds = 2.0;
    constexpr double foldSeconds = 8.0;

    /** Renders C4 twice with only the voice's noise stream differing and
        returns the difference, together with the glottal phase at every sample
        and the RMS of the full signal. The difference is exact only at
        Humanize 0, where the noise stream drives no jitter and no shimmer, so
        the test pins Humanize 0 rather than merely happening to use it. */
    struct Residual
    {
        std::vector<double> difference;
        std::vector<float> phase;
        double fullRms = 0.0;
    };

    const auto isolate = [&] (float breath, float depth)
    {
        const auto take = [&] (std::uint32_t reseed, std::vector<float>& mono,
                               std::vector<float>* phase)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, foldBlock);
            engine.reset();
            vocalor::EngineParameters parameters;
            parameters.profile = vocalor::VoiceProfile::Female;
            parameters.mode = vocalor::PerformanceMode::Solo;
            parameters.vowel = vocalor::Vowel::Aah;
            parameters.breath = breath;
            parameters.tension = 0.30f;
            parameters.vibrato = 0.0f;
            parameters.humanize = 0.0f;
            parameters.room = 0.0f;
            parameters.spread = 0.0f;
            engine.setParameters (parameters);
            vocalor::VoiceEngineTestAccess::setAspirationModulationDepth (engine, depth);
            // The glottal window is a fact about the glottis. A room fills the
            // closed phase with the open phase's reflections whatever the
            // glottis did -- measured, the shipping geometry takes the
            // closed-phase margin from 4.22 dB to 0.65 dB at Room 1 -- so the
            // image field is switched out here for the same reason Humanize is.
            vocalor::VoiceEngineTestAccess::setPlacementReflectionDepth (engine, 0.0f);
            engine.noteOn (60, 0.85f);
            const auto propagation =
                vocalor::VoiceEngineTestAccess::directDelaySamples (engine)[0];

            std::vector<float> left (foldBlock, 0.0f);
            std::vector<float> right (foldBlock, 0.0f);
            const int settle = static_cast<int> (settleSeconds * sampleRate) / foldBlock;
            for (int i = 0; i < settle; ++i)
                engine.process (left.data(), right.data(), foldBlock);
            if (reseed != 0u)
                vocalor::VoiceEngineTestAccess::reseedVoiceNoise (engine, reseed);

            const int blocks = static_cast<int> (foldSeconds * sampleRate) / foldBlock;
            for (int i = 0; i < blocks; ++i)
            {
                if (phase != nullptr)
                {
                    const auto state = vocalor::VoiceEngineTestAccess::voicePhaseState (engine);
                    for (int j = 0; j < foldBlock; ++j)
                    {
                        // The render loop advances the phase before it uses it,
                        // so sample j sits at phase + (j + 1) increments. The
                        // singer stands 1.7 m away, so what leaves her glottis
                        // at that phase reaches the listener 12 samples later:
                        // the fold is against the glottis, not against the
                        // microphone, so the propagation delay comes back out.
                        float value = state[0]
                                    + state[1] * (static_cast<float> (j + 1) - propagation);
                        value -= std::floor (value);
                        phase->push_back (value);
                    }
                }
                engine.process (left.data(), right.data(), foldBlock);
                for (int j = 0; j < foldBlock; ++j)
                    mono.push_back (0.5f * (left[static_cast<std::size_t> (j)]
                                            + right[static_cast<std::size_t> (j)]));
            }
        };

        Residual result;
        std::vector<float> a;
        std::vector<float> b;
        take (0u, a, &result.phase);
        take (0x9e3779b9u, b, nullptr);
        result.difference.resize (a.size());
        double sum = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            result.difference[i] = static_cast<double> (a[i]) - static_cast<double> (b[i]);
            sum += static_cast<double> (a[i]) * static_cast<double> (a[i]);
        }
        result.fullRms = std::sqrt (sum / static_cast<double> (a.size()));
        return result;
    };

    /** The residual's energy folded onto the glottal period, in dB per bin.
        Two independent noise realisations differenced are 3 dB louder than one,
        which cancels out of every ratio taken below. */
    const auto fold = [&] (const Residual& residual)
    {
        std::array<double, bins> energy {};
        std::array<long, bins> count {};
        for (std::size_t i = 0; i < residual.difference.size(); ++i)
        {
            const auto bin = static_cast<std::size_t> (std::min (
                bins - 1, static_cast<int> (residual.phase[i] * static_cast<float> (bins))));
            energy[bin] += residual.difference[i] * residual.difference[i];
            ++count[bin];
        }
        std::array<double, bins> result {};
        for (std::size_t i = 0; i < bins; ++i)
            result[i] = 10.0 * std::log10 (energy[i] / static_cast<double> (count[i]));
        return result;
    };

    const auto rms = [] (const Residual& residual)
    {
        double sum = 0.0;
        for (const auto value : residual.difference)
            sum += value * value;
        return std::sqrt (sum / static_cast<double> (residual.difference.size()));
    };

    const auto modulated = isolate (1.0f, 1.0f);
    const auto stationary = isolate (1.0f, 0.0f);
    const auto modulatedBins = fold (modulated);
    const auto stationaryBins = fold (stationary);

    const auto span = [] (const std::array<double, bins>& values)
    {
        const auto extremes = std::minmax_element (values.begin(), values.end());
        return *extremes.second - *extremes.first;
    };
    const auto peakBin = static_cast<std::size_t> (
        std::max_element (modulatedBins.begin(), modulatedBins.end()) - modulatedBins.begin());
    const auto peakPhase = (static_cast<double> (peakBin) + 0.5) / bins;

    std::cout << std::fixed << std::setprecision (2)
              << "aspiration fold: peak-to-trough " << span (modulatedBins)
              << " dB with the glottal window, " << span (stationaryBins)
              << " dB without it; peak at phase " << std::setprecision (3) << peakPhase
              << "; isolated aspiration " << std::setprecision (2)
              << 20.0 * std::log10 (rms (modulated) / std::sqrt (2.0) / modulated.fullRms)
              << " dB below the full signal\n";

    // Without the window the fold is flat by construction, not by measurement:
    // 0.27 dB is the estimator's own floor from folding a finite noise record,
    // and it is what the assertion below has to beat.
    expect (span (stationaryBins) <= 1.0,
            "the aspiration fold's own estimator floor is no longer flat");

    // The window is the glottal flow, which is zero while the folds are closed;
    // what fills the closed phase at the output is the tract ringing on from
    // the open phase. Measured 10.46 dB. It is pitch-dependent for that reason:
    // the same measurement reads 9.96 dB at C3 and 4.84 dB at C5, where one
    // period is shorter than the tract's own ring time, so the note is pinned.
    // It is also tension-dependent because the glottis is open for less of the
    // cycle at higher Tension, so the tension is pinned too.
    expect (span (modulatedBins) >= 8.0,
            "the aspiration carries no glottal-cycle modulation");

    // Turbulence rises with the flow and is extinguished at closure, so the
    // energy has to peak in the open phase. Every physical flow shape is aligned
    // to close at phase 0.78; the measured peak is 0.604. On its own this one cannot fail against a
    // flat fold, whose peak bin is wherever the estimator's noise put it, so it
    // is paired with the closed-phase margin below.
    expect (peakPhase < 0.78,
            "the aspiration peaks in the closed phase rather than the open one");

    // Every aligned shape is shut by 0.78 of the period, so the last five bins are
    // closed glottis whatever the tension is. What is left there is the tract
    // ringing on from the open phase, and it has to sit well below the peak.
    double closedEnergy = 0.0;
    for (std::size_t i = 19; i < bins; ++i)
        closedEnergy += std::pow (10.0, 0.1 * modulatedBins[i]);
    const auto closedMargin = modulatedBins[peakBin]
        - 10.0 * std::log10 (closedEnergy / static_cast<double> (bins - 19));
    std::cout << "aspiration closed-phase margin: " << closedMargin << " dB\n";
    // Measured 3.51 dB, against the estimator floor with the window switched out, where the
    // peak bin lands at phase 0.646 and the assertion above passes by luck.
    expect (closedMargin >= 3.0,
            "the aspiration is not extinguished while the folds are closed");

    // This moves the noise about inside the period. It must not change how much
    // noise there is -- which is why the window is normalised to unit mean
    // square and each adjacent physical-shape interpolation is renormalised at
    // the control rate from its stored mean and cross term.
    const auto levelChange = 20.0 * std::log10 (rms (modulated) / rms (stationary));
    expect (std::abs (levelChange) < 1.0,
            "the glottal window changed the aspiration level instead of its distribution");

    // The prize is on the Breath axis and nowhere else, so the same isolation
    // is reported at the shipping default to keep that honest: 40.73 dB down.
    const auto defaultBreath = isolate (0.28f, 1.0f);
    const auto defaultBins = fold (defaultBreath);
    std::cout << "aspiration at Breath 0.28: "
              << 20.0 * std::log10 (rms (defaultBreath) / std::sqrt (2.0) / defaultBreath.fullRms)
              << " dB below the full signal, peak-to-trough " << span (defaultBins) << " dB\n";
    // 10.12 dB: the aspiration is about 41 dB down here and the difference
    // trick is measuring it against a much louder voiced signal, so the fold's
    // own floor is a larger share of the span.
    expect (span (defaultBins) >= 8.0,
            "the glottal window does not reach the default patch");
}

/** A vowel change is a jaw and a tongue moving, not a de-zipper.

    The 1.1 formant glides ran on 16, 9, 5, 4 and 3 ms time constants, so a
    vowel switch was over in about 50 ms; a sung vowel-to-vowel transition runs
    100-200. The engine had the mechanism for coarticulation and used it to stop
    clicks.
*/
void testCoarticulationTiming()
{
    constexpr auto sampleRate = 48000.0;

    /** 10-90 % rise time of each formant, in milliseconds, after the pad is
        stepped from @c fromX,@c fromY to @c toX,@c toY on a held note. */
    const auto riseTimes = [] (float fromX, float fromY, float toX, float toY)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = fromX;
        parameters.vowelY = fromY;
        engine.setParameters (parameters);
        engine.noteOn (57, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.4));

        const auto start = vocalor::VoiceEngineTestAccess::voiceFormants (engine);
        parameters.vowelX = toX;
        parameters.vowelY = toY;
        engine.setParameters (parameters);

        // Settle first so the destination is measured, not guessed.
        std::array<std::array<float, vocalor::kFormantCount>, 400> trace {};
        constexpr int step = 64;
        for (std::size_t frame = 0; frame < trace.size(); ++frame)
        {
            render (engine, step);
            trace[frame] = vocalor::VoiceEngineTestAccess::voiceFormants (engine);
        }
        const auto finish = trace.back();

        std::array<double, vocalor::kFormantCount> milliseconds {};
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t> (formant);
            const auto span = finish[index] - start[index];
            if (std::abs (span) < 1.0f)
                continue;

            double tenth = -1.0;
            double ninetieth = -1.0;
            for (std::size_t frame = 0; frame < trace.size(); ++frame)
            {
                const auto progress = (trace[frame][index] - start[index]) / span;
                const auto at = 1000.0 * static_cast<double> ((frame + 1) * step) / sampleRate;
                if (tenth < 0.0 && progress >= 0.10)
                    tenth = at;
                if (ninetieth < 0.0 && progress >= 0.90)
                {
                    ninetieth = at;
                    break;
                }
            }
            milliseconds[index] = (tenth >= 0.0 && ninetieth >= 0.0) ? ninetieth - tenth : -1.0;
        }
        return milliseconds;
    };

    // Close front to open: the largest move the pad offers.
    const auto large = riseTimes (1.0f, 0.0f, 0.5f, 1.0f);
    std::cout << "vowel step /i/ -> /a/, 10-90 % rise: " << std::fixed << std::setprecision (0);
    for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        std::cout << ' ' << (formant + 1) << ':' << large[static_cast<std::size_t> (formant)] << "ms";
    std::cout << '\n';

    // F1 and F2 make the large moves here: 310 -> 850 Hz and 2790 -> 1220. F3
    // moves 500 Hz and F4 only 250, and F5 is the same 4950 Hz for both these
    // vowels, so only the first three carry a timing claim at all.
    expect (large[0] > 80.0 && large[0] < 260.0,
            "F1 did not move on a jaw's timescale after a full vowel step");
    expect (large[1] > 55.0 && large[1] < 220.0,
            "F2 did not move on a tongue's timescale after a full vowel step");
    expect (large[0] > large[1] && large[1] > large[2],
            "the formants no longer move in order of the cavity that carries them");
    expect (large[2] > 12.0,
            "F3 still switches as fast as a de-zipper on a 500 Hz move");
    // The jaw is a heavier articulator than the larynx by enough that the two
    // must not read as one shared glide. Collapsing every formant back onto a
    // single time constant is the regression this separation guards.
    expect (large[0] >= 3.0 * large[3],
            "the jaw and the larynx are gliding at nearly the same speed");

    // A small move is a small adjustment, not the same journey done slowly.
    const auto small = riseTimes (0.5f, 1.0f, 0.56f, 0.94f);
    std::cout << "small vowel step, F1 10-90 % rise: " << small[0] << "ms\n";
    expect (small[0] > 0.0 && small[0] < 0.6 * large[0],
            "a small vowel move took as long as a full one: the transition has a "
            "deadline rather than a speed");
}

/** A parallel bank of poles cannot be asked for an /m/.

    Its transfer function does have zeros, but they land wherever the sections
    happen to cancel; there is no way to place one. That rules out the nasal
    branch, and a hum is the most common choir colour after "ah". The branch
    adds a murmur pole at the nasal cavity's own resonance and a placed
    anti-resonance where the closed mouth loads it.
*/
void testNasalBranch()
{
    constexpr auto sampleRate = 48000.0;
    // A low note so the harmonic comb resolves both bands. A2 puts harmonics at
    // 220 and 330 Hz across the murmur pole, and at 880, 990 and 1100 across
    // the anti-resonance.
    constexpr double fundamental = 110.0;

    struct Bands { double low; double notch; double peak; double total; };
    const auto bandsAt = [] (float nasal)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.profile = vocalor::VoiceProfile::Male;
        parameters.nasal = nasal;
        engine.setParameters (parameters);
        engine.noteOn (45, 0.85f);
        renderMono (engine, static_cast<int> (sampleRate * 0.5));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.5));

        const auto energy = [&samples] (int first, int last)
        {
            double total = 0.0;
            for (int harmonic = first; harmonic <= last; ++harmonic)
            {
                const auto magnitude = harmonicMagnitude (
                    samples, fundamental * harmonic, sampleRate);
                total += magnitude * magnitude;
            }
            return std::sqrt (total);
        };
        // 220-330 Hz across the murmur, 880-1100 across the zero, and
        // 1650-2200 well above it as a control that the branch is not simply a
        // low-pass filter with a nicer name.
        double sumOfSquares = 0.0;
        for (const auto value : samples)
            sumOfSquares += static_cast<double> (value) * value;
        return Bands { energy (2, 3), energy (8, 10), energy (15, 20),
                       std::sqrt (sumOfSquares / std::max<std::size_t> (samples.size(), 1)) };
    };

    const auto oral = bandsAt (0.0f);
    const auto hummed = bandsAt (1.0f);
    const auto half = bandsAt (0.5f);
    const auto drop = [] (double before, double after)
    {
        return 20.0 * std::log10 (std::max (before, 1.0e-12) / std::max (after, 1.0e-12));
    };

    const auto notchDrop = drop (oral.notch, hummed.notch);
    const auto lowDrop = drop (oral.low, hummed.low);
    const auto highDrop = drop (oral.peak, hummed.peak);
    const auto totalDrop = drop (oral.total, hummed.total);
    std::cout << "velum fully open: 880-1100 Hz " << std::fixed << std::setprecision (1)
              << notchDrop << " dB, 220-330 Hz " << lowDrop << " dB, 1.65-2.2 kHz "
              << highDrop << " dB, overall " << totalDrop << " dB\n";

    expect (notchDrop > 20.0,
            "the nasal branch did not place an anti-resonance where the mouth loads it");
    expect (lowDrop < -3.0,
            "the murmur pole did not carry the band an /m/ radiates in");
    expect (highDrop > 4.0 && highDrop < notchDrop - 8.0,
            "a hum has to be duller than the vowel without being a low-pass filter");
    // The velum is a timbre control. An /m/ radiates from a smaller and more
    // damped aperture than an open mouth, and the branch has to account for
    // that rather than arriving as the loudest thing the instrument does.
    expect (std::abs (totalDrop) < 4.0,
            "opening the velum worked as a volume control");

    // A half-open velum has to land between the two, not snap to one of them.
    const auto halfDrop = drop (oral.notch, half.notch);
    expect (halfDrop > 1.0 && halfDrop < notchDrop - 1.0,
            "the velum coupling is a switch rather than a continuous control");

    // Every coupling has to stay finite and bounded, including through a jump.
    for (const float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (1, 0, 0, 0);
        parameters.choirSize = 6;
        parameters.nasal = amount;
        engine.setParameters (parameters);
        engine.noteOn (52, 0.9f);
        const auto held = render (engine, static_cast<int> (sampleRate * 0.3));
        parameters.nasal = 1.0f - amount;
        engine.setParameters (parameters);
        const auto moved = render (engine, static_cast<int> (sampleRate * 0.3));
        expect (held.finite && moved.finite && moved.peak < 16.0,
                "the nasal branch destabilised at coupling "
                    + std::to_string (static_cast<int> (amount * 100.0f)) + " %");
    }
}

/** Twelve singers have to disperse like twelve singers.

    The 1.1 engine gave each singer a uniform +/-5.6 cents of static detune,
    about 3.2 cents of standard deviation at full Humanize. Jers and Ternstrom
    measured 25-30 cents of dispersion between real choir singers, and listeners
    were reported to tolerate 14 cents of standard deviation; a quarter of the
    tolerance is why twelve voices could still read as one thick one.
*/
void testEnsembleDispersion()
{
    constexpr auto sampleRate = 48000.0;

    const auto dispersionCents = [] (float humanize, int renderSamples,
                                     std::vector<float>& offsets)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.mode = vocalor::PerformanceMode::Choir;
        parameters.choirSize = 12;
        parameters.humanize = humanize;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, renderSamples);

        const auto sounding = vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
        offsets.clear();
        if (sounding.size() < 2)
            return 0.0;

        double mean = 0.0;
        for (const auto frequency : sounding)
            mean += 1200.0 * std::log2 (std::max (static_cast<double> (frequency), 1.0e-9));
        mean /= static_cast<double> (sounding.size());

        double sumOfSquares = 0.0;
        for (const auto frequency : sounding)
        {
            const auto cents = 1200.0 * std::log2 (std::max (static_cast<double> (frequency), 1.0e-9))
                             - mean;
            offsets.push_back (static_cast<float> (cents));
            sumOfSquares += cents * cents;
        }
        return std::sqrt (sumOfSquares / static_cast<double> (sounding.size()));
    };

    std::vector<float> early;
    std::vector<float> late;
    std::vector<float> tight;

    const auto spread = dispersionCents (1.0f, static_cast<int> (sampleRate * 1.0), early);
    std::cout << "12-singer pitch dispersion at full Humanize: " << std::fixed
              << std::setprecision (1) << spread << " cents\n";
    expect (early.size() == 12, "the dispersion probe did not find twelve singers");
    // The measured band is 10-15 cents of standard deviation between section
    // colleagues; anything under 6 is a studio overdub, anything over 20 is out
    // of tune rather than human.
    expect (spread > 8.0 && spread < 18.0,
            "the ensemble dispersion is not in the range measured for real choirs");

    // Humanize is the dial from a studio unison to a rehearsal room, so at zero
    // the section has to be perfectly locked.
    expect (dispersionCents (0.0f, static_cast<int> (sampleRate * 1.0), tight) < 0.05,
            "the ensemble was not perfectly in tune with Humanize at zero");

    // The offsets are not fixed: a section drifts and recovers.
    dispersionCents (1.0f, static_cast<int> (sampleRate * 9.0), late);
    expect (late.size() == early.size(), "the late dispersion probe lost singers");
    double largestMove = 0.0;
    for (std::size_t i = 0; i < late.size(); ++i)
        largestMove = std::max (largestMove,
                                std::abs (static_cast<double> (late[i] - early[i])));
    std::cout << "largest singer drift over nine seconds: " << largestMove << " cents\n";
    expect (largestMove > 1.5,
            "the per-singer offsets are frozen: the section does not drift at all");
    expect (largestMove < 40.0,
            "the per-singer drift is unbounded rather than a wander around the target");
}

struct NaturalTrajectory
{
    double observationRate = 0.0;
    std::vector<double> pitchCents;
    std::vector<double> independentPitchCents;
    std::vector<double> vowelMorph;
    std::vector<double> vowelX;
    std::vector<double> vowelY;
    std::array<std::vector<double>, vocalor::kFormantCount> vowelFormants;
    double maximumDirectVibratoGainError = 0.0;
    double maximumShelfVibratoGainError = 0.0;
    float resolvedVibratoCents = 0.0f;
};

NaturalTrajectory captureNaturalTrajectory (
    double sampleRate, const vocalor::EngineParameters& parameters,
    double discardSeconds, double measuredSeconds, double observationRate,
    int midiNote = 60, int seedAdvance = 0,
    const std::vector<int>& splitPattern = {})
{
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.setParameters (parameters);
    // Reset after publishing the fixture parameters so smoothed controls begin
    // on their requested values instead of ramping away from fresh defaults.
    engine.reset();

    // Advance only the note-generation seed. No audio time passes, so two
    // takes made this way differ in their stochastic draw without acquiring a
    // different absolute drift phase or a different analysis window.
    for (int generation = 0; generation < seedAdvance; ++generation)
    {
        engine.noteOn (midiNote + 1, 0.75f);
        engine.allSoundOff();
    }
    engine.noteOn (midiNote, 0.80f);

    const int stride = std::max (1, static_cast<int> (std::lround (
        sampleRate / observationRate)));
    const double realisedRate = sampleRate / static_cast<double> (stride);
    int maximumProcess = stride;
    for (const auto split : splitPattern)
        maximumProcess = std::max (maximumProcess, split);
    std::vector<float> left (static_cast<std::size_t> (maximumProcess), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (maximumProcess), 0.0f);
    std::size_t splitIndex = 0;

    const auto advance = [&] (int samples)
    {
        while (samples > 0)
        {
            const int requested = splitPattern.empty()
                ? samples
                : std::max (1, splitPattern[splitIndex++ % splitPattern.size()]);
            const int count = std::min (samples, requested);
            engine.process (left.data(), right.data(), count);
            samples -= count;
        }
    };

    const int discardObservations = static_cast<int> (std::ceil (
        discardSeconds * realisedRate));
    for (int observation = 0; observation < discardObservations; ++observation)
        advance (stride);

    NaturalTrajectory result;
    result.observationRate = realisedRate;
    const int observations = static_cast<int> (std::ceil (
        measuredSeconds * realisedRate));
    result.pitchCents.reserve (static_cast<std::size_t> (observations));
    result.independentPitchCents.reserve (static_cast<std::size_t> (observations));
    result.vowelMorph.reserve (static_cast<std::size_t> (observations));
    result.vowelX.reserve (static_cast<std::size_t> (observations));
    result.vowelY.reserve (static_cast<std::size_t> (observations));
    for (auto& formant : result.vowelFormants)
        formant.reserve (static_cast<std::size_t> (observations));

    const double nominal = 440.0 * std::exp2 (
        (static_cast<double> (midiNote) - 69.0) / 12.0);
    for (int observation = 0; observation < observations; ++observation)
    {
        advance (stride);
        const auto frequencies =
            vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
        if (frequencies.empty())
            continue;
        result.pitchCents.push_back (1200.0 * std::log2 (
            static_cast<double> (frequencies.front()) / nominal));
        result.independentPitchCents.push_back (
            vocalor::VoiceEngineTestAccess::independentPitchDriftCents (engine));
        const auto vowel = vocalor::VoiceEngineTestAccess::effectiveVowel (engine);
        result.vowelMorph.push_back (vowel[0]);
        result.vowelX.push_back (vowel[1]);
        result.vowelY.push_back (vowel[2]);
        const auto formants =
            vocalor::VoiceEngineTestAccess::driftedVowelFormants (engine);
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
            result.vowelFormants[static_cast<std::size_t> (formant)].push_back (
                formants[static_cast<std::size_t> (formant)]);

        const auto modulation =
            vocalor::VoiceEngineTestAccess::vibratoModulationState (engine);
        result.maximumDirectVibratoGainError = std::max (
            result.maximumDirectVibratoGainError,
            std::abs (static_cast<double> (modulation[1]) - 1.0));
        result.maximumShelfVibratoGainError = std::max (
            result.maximumShelfVibratoGainError,
            std::abs (static_cast<double> (modulation[2]) - 1.0));
    }
    result.resolvedVibratoCents =
        vocalor::VoiceEngineTestAccess::resolvedVibratoCents (engine);
    return result;
}

/** Fresh straight tone has no intentional vibrato, but it is not a test
    oscillator: Drift contributes bounded, aperiodic pitch motion of adjustable
    depth. Measure the oscillator's actual F0, not a random state that might be
    disconnected from it. */
void testStraightTonePitchDrift()
{
    vocalor::EngineParameters fresh;
    expect (fresh.vibrato == 0.0f,
            "the standalone DSP default still enables intentional vibrato");
    expect (std::abs (fresh.instability - 0.38f) < 1.0e-7f,
            "the standalone DSP default no longer carries 38 % Drift");
    expect (! fresh.legacyDriftBypass,
            "a fresh DSP instance selected the legacy Drift bypass");

    auto parameters = fresh;
    parameters.mode = vocalor::PerformanceMode::Solo;
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.0f;
    parameters.room = 0.0f;
    parameters.dynamics = 1.0f;
    parameters.instability = 0.0f;
    parameters.legacyDriftBypass = false;

    const auto staticTake = captureNaturalTrajectory (
        48000.0, parameters, 4.0, 2.0, 100.0);
    const bool staticPitch = ! staticTake.pitchCents.empty()
        && std::all_of (staticTake.pitchCents.begin(), staticTake.pitchCents.end(),
                       [&staticTake] (double value)
                       { return value == staticTake.pitchCents.front(); });
    expect (staticPitch,
            "Vibrato/Humanize/Drift at zero did not leave an exactly static F0");
    expect (std::all_of (staticTake.independentPitchCents.begin(),
                         staticTake.independentPitchCents.end(),
                         [] (double value) { return value == 0.0; }),
            "Drift at zero left an independent pitch displacement running");
    expect (staticTake.resolvedVibratoCents == 0.0f,
            "Vibrato zero did not resolve to exactly zero cents");
    expect (staticTake.maximumDirectVibratoGainError == 0.0
                && staticTake.maximumShelfVibratoGainError == 0.0,
            "Vibrato zero left laryngeal amplitude modulation running");

    const auto driftTake = [&parameters] (float depth, double sampleRate = 48000.0,
                                          int seedAdvance = 0,
                                          const std::vector<int>& splits = {})
    {
        auto moved = parameters;
        moved.instability = depth;
        return captureNaturalTrajectory (sampleRate, moved, 3.0, 24.0, 100.0,
                                         60, seedAdvance, splits);
    };

    const auto quarter = driftTake (0.25f);
    const auto half = driftTake (0.50f);
    const auto shipping = driftTake (0.38f);
    const auto full = driftTake (1.0f);
    const auto quarterRms = seriesDeviation (quarter.pitchCents);
    const auto halfRms = seriesDeviation (half.pitchCents);
    const auto shippingRms = seriesDeviation (shipping.pitchCents);
    const auto fullRms = seriesDeviation (full.pitchCents);
    const auto shippingSine = maximumSineFitFraction (
        shipping.pitchCents, shipping.observationRate, 3.0, 10.0);
    const auto fullSine = maximumSineFitFraction (
        full.pitchCents, full.observationRate, 3.0, 10.0);
    const auto shippingRecurrence = maximumAutocorrelation (
        shipping.pitchCents, shipping.observationRate, 0.10, 0.34);

    std::cout << "straight-tone Drift: RMS D.25/.38/.50/1 " << std::fixed
              << std::setprecision (3) << quarterRms << "/" << shippingRms
              << "/" << halfRms << "/" << fullRms
              << " cents; 3-10 Hz sine share D.38/1 "
              << shippingSine << "/" << fullSine
              << ", recurrence " << shippingRecurrence << "\n";

    // The OU law is 4.8*Drift times a bounded blend. Its stationary result is
    // about 3.8 cents RMS at full depth; these broad limits reject inaudible and
    // unruly substitutes without pinning one deterministic draw.
    expect (shippingRms > 0.70 && shippingRms < 2.40,
            "the fresh 38 % Drift pitch motion is inaudible or no longer subtle");
    expect (shipping.resolvedVibratoCents == 0.0f
                && shipping.maximumDirectVibratoGainError == 0.0
                && shipping.maximumShelfVibratoGainError == 0.0,
            "fresh Drift activated an intentional vibrato or its AM path");
    expect (fullRms > 2.0 && fullRms < 6.0,
            "full Drift left the intended bounded pitch-motion range");
    expect (quarterRms > 0.35 && quarterRms < halfRms
                && halfRms < fullRms
                && halfRms > 1.6 * quarterRms
                && fullRms > 1.6 * halfRms,
            "the Drift control no longer increases pitch depth monotonically");
    expect (seriesMaximumMagnitude (quarter.pitchCents) < 3.1
                && seriesMaximumMagnitude (shipping.pitchCents) < 4.7
                && seriesMaximumMagnitude (half.pitchCents) < 6.1
                && seriesMaximumMagnitude (full.pitchCents) < 12.1,
            "an independent pitch excursion escaped the OU hard bound");
    expect (shippingSine < 0.20 && fullSine < 0.20,
            "straight-tone Drift collapsed into a theremin-like 3-10 Hz LFO");
    expect (shippingRecurrence < 0.35,
            "straight-tone Drift repeats almost exactly at a vibrato-period lag");

    // The public state is not enough: confirm that the same motion reaches the
    // oscillator. Constant identity offsets disappear under demeaning.
    expect (normalisedCorrelation (shipping.pitchCents,
                                   shipping.independentPitchCents) > 0.999,
            "the independent pitch state is not the motion sounding at F0");

    const auto repeat = driftTake (0.38f);
    expect (repeat.pitchCents == shipping.pitchCents,
            "the same note sequence did not reproduce its Drift trajectory");
    const auto differentSeed = driftTake (0.38f, 48000.0, 1);
    expect (std::abs (normalisedCorrelation (shipping.pitchCents,
                                             differentSeed.pitchCents)) < 0.90,
            "a new note seed repeated the previous pitch imperfection");

    const auto oddSplit = driftTake (0.38f, 48000.0, 0,
                                     { 37, 211, 5, 89, 16, 173 });
    double splitResidual = 0.0;
    for (std::size_t i = 0; i < shipping.pitchCents.size()
                            && i < oddSplit.pitchCents.size(); ++i)
        splitResidual = std::max (splitResidual,
                                  std::abs (shipping.pitchCents[i]
                                            - oddSplit.pitchCents[i]));
    expect (splitResidual < 1.0e-5,
            "host buffer splits changed the straight-tone pitch trajectory");

    double quietest = 1.0e9;
    double loudest = -1.0e9;
    double worstRateSine = 0.0;
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto take = driftTake (1.0f, sampleRate);
        const auto rms = seriesDeviation (take.pitchCents);
        quietest = std::min (quietest, rms);
        loudest = std::max (loudest, rms);
        worstRateSine = std::max (worstRateSine, maximumSineFitFraction (
            take.pitchCents, take.observationRate, 3.0, 10.0));
        expect (seriesMaximumMagnitude (take.pitchCents) < 12.1,
                "sample-rate testing found an unbounded pitch excursion");
    }
    const auto rateDepthSpreadDb = 20.0 * std::log10 (
        loudest / std::max (quietest, 1.0e-12));
    std::cout << "straight-tone Drift rate invariance: " << std::setprecision (2)
              << rateDepthSpreadDb << " dB RMS spread; worst sine share "
              << std::setprecision (3) << worstRateSine << "\n";
    expect (rateDepthSpreadDb < 2.0,
            "the independent pitch depth depends on sample rate");
    expect (worstRateSine < 0.22,
            "a supported sample rate turned Drift into a periodic LFO");

    // A pre-Drift session may carry a live host value, but its model marker
    // must bypass every independent pitch effect.
    auto legacy = parameters;
    legacy.instability = 1.0f;
    legacy.legacyDriftBypass = true;
    const auto legacyTake = captureNaturalTrajectory (
        48000.0, legacy, 4.0, 2.0, 100.0);
    const bool legacyPitchIsConstant = ! legacyTake.pitchCents.empty()
        && std::all_of (legacyTake.pitchCents.begin(), legacyTake.pitchCents.end(),
                       [&legacyTake] (double value)
                       { return value == legacyTake.pitchCents.front(); });
    expect (legacyPitchIsConstant
                && std::all_of (legacyTake.independentPitchCents.begin(),
                                legacyTake.independentPitchCents.end(),
                                [] (double value) { return value == 0.0; }),
            "legacyDriftBypass did not preserve the pre-Drift straight tone");
}

/** Drift advances on elapsed control intervals, not on calls which merely
    re-resolve an articulation. In particular, initialiseVoice() and the first
    render sample both visit age zero, while an off-grid legato note asks for an
    immediate pitch/tract update between two scheduled controls. Neither visit
    may consume a pitch/vowel innovation or shorten the vowel scheduler. */
void testDriftClockIgnoresLegatoEvents()
{
    constexpr auto sampleRate = 48000.0;
    auto parameters = steadyParameters();
    parameters.legato = true;
    parameters.instability = 1.0f;
    parameters.legacyDriftBypass = false;

    const auto makeEngine = [&parameters]
    {
        auto engine = std::make_unique<vocalor::VoiceEngine>();
        engine->prepare (sampleRate, blockSize);
        engine->setParameters (parameters);
        engine->reset();
        engine->noteOn (60, 0.80f);
        return engine;
    };
    auto reference = makeEngine();
    auto articulated = makeEngine();

    std::array<float, 32> left {};
    std::array<float, 32> right {};
    const auto processBoth = [&] (int samples)
    {
        reference->process (left.data(), right.data(), samples);
        articulated->process (left.data(), right.data(), samples);
    };
    const auto sameDriftClock = [&]
    {
        return vocalor::VoiceEngineTestAccess::driftRandomStates (*reference)
                   == vocalor::VoiceEngineTestAccess::driftRandomStates (*articulated)
            && vocalor::VoiceEngineTestAccess::driftOuStates (*reference)
                   == vocalor::VoiceEngineTestAccess::driftOuStates (*articulated)
            && vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*reference)
                   == vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*articulated);
    };

    const auto noteOnRandom =
        vocalor::VoiceEngineTestAccess::driftRandomStates (*articulated);
    const auto noteOnOu = vocalor::VoiceEngineTestAccess::driftOuStates (*articulated);
    const auto noteOnVowelCountdown =
        vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*articulated);
    processBoth (1);
    expect (vocalor::VoiceEngineTestAccess::driftRandomStates (*articulated)
                == noteOnRandom
                && vocalor::VoiceEngineTestAccess::driftOuStates (*articulated)
                    == noteOnOu
                && vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*articulated)
                    == noteOnVowelCountdown,
            "the first render sample advanced Drift a second time at age zero");

    // Age seven lies strictly between the controls at ages zero and sixteen.
    processBoth (6);
    const auto beforeLegatoRandom =
        vocalor::VoiceEngineTestAccess::driftRandomStates (*articulated);
    const auto beforeLegatoOu =
        vocalor::VoiceEngineTestAccess::driftOuStates (*articulated);
    const auto beforeLegatoVowelCountdown =
        vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*articulated);
    articulated->noteOn (62, 0.80f);
    expect (articulated->getActiveVoiceCount() == 1,
            "the Drift-clock fixture retriggered instead of retuning legato");
    expect (vocalor::VoiceEngineTestAccess::driftRandomStates (*articulated)
                == beforeLegatoRandom
                && vocalor::VoiceEngineTestAccess::driftOuStates (*articulated)
                    == beforeLegatoOu
                && vocalor::VoiceEngineTestAccess::vowelDriftCountdown (*articulated)
                    == beforeLegatoVowelCountdown,
            "an off-grid legato event consumed a Drift control interval");

    // The next scheduled age-16 update must draw exactly what the uninterrupted
    // reference draws; the intervening articulation changes pitch, not time.
    processBoth (10);
    expect (sameDriftClock(),
            "legato changed the fixed-schedule Drift trajectory after the next control");
}

/** A host may ride Drift while a note is already sounding. Both the fast pitch
    path and the slower tract path must enter and leave continuously; returning
    the control to zero must converge to the exact straight-tone targets, not
    leave a tiny modulation running or quantise the macro into audible steps. */
void testLiveDriftAutomation()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int hop = 64;
    constexpr double trackRate = sampleRate / hop;
    constexpr double concertC4 = 261.6255653005986;

    struct AutomationTake
    {
        bool finite = true;
        std::size_t returnBegin = 0;
        std::vector<double> smoothed;
        std::vector<double> pitch;
        std::vector<double> independentPitch;
        std::vector<double> morph;
        std::vector<double> x;
        std::vector<double> y;
        std::array<std::vector<double>, vocalor::kFormantCount> targetFormants;
        std::array<std::vector<double>, vocalor::kFormantCount> renderedFormants;
        std::vector<std::array<std::uint32_t, 2>> randomStates;
        std::vector<std::array<float, 5>> ouStates;
        std::vector<int> vowelCountdowns;
    };

    const auto perform = [=]
    {
        vocalor::EngineParameters parameters;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vibrato = 0.0f;
        parameters.humanize = 0.0f;
        parameters.instability = 0.0f;
        parameters.legacyDriftBypass = false;
        parameters.room = 0.0f;
        parameters.dynamics = 1.0f;
        parameters.vowel = vocalor::Vowel::Aah;
        parameters.vowelMorph = 0.50f;
        parameters.vowelX = 0.50f;
        parameters.vowelY = 0.50f;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.setParameters (parameters);
        engine.reset();
        engine.noteOn (60, 0.80f);

        AutomationTake take;
        std::array<float, hop> left {};
        std::array<float, hop> right {};
        const auto observe = [&]
        {
            const auto frequencies =
                vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
            if (frequencies.empty())
            {
                take.finite = false;
                return;
            }
            const auto vowel = vocalor::VoiceEngineTestAccess::effectiveVowel (engine);
            const auto formants =
                vocalor::VoiceEngineTestAccess::driftedVowelFormants (engine);
            const auto renderedTract =
                vocalor::VoiceEngineTestAccess::voiceTract (engine, 60);
            const auto macro = static_cast<double> (
                vocalor::VoiceEngineTestAccess::smoothedDrift (engine));
            const auto pitch = 1200.0 * std::log2 (
                static_cast<double> (frequencies.front()) / concertC4);
            const auto independentPitch = static_cast<double> (
                vocalor::VoiceEngineTestAccess::independentPitchDriftCents (engine));
            take.finite = take.finite && std::isfinite (macro)
                && std::isfinite (pitch) && std::isfinite (independentPitch)
                && std::all_of (vowel.begin(), vowel.end(),
                                [] (float value) { return std::isfinite (value); });
            take.smoothed.push_back (macro);
            take.pitch.push_back (pitch);
            take.independentPitch.push_back (independentPitch);
            take.morph.push_back (vowel[0]);
            take.x.push_back (vowel[1]);
            take.y.push_back (vowel[2]);
            for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
            {
                const auto index = static_cast<std::size_t> (formant);
                const auto target = static_cast<double> (formants[index]);
                const auto rendered = static_cast<double> (renderedTract.hz[index]);
                take.finite = take.finite && std::isfinite (target)
                    && std::isfinite (rendered) && target > 0.0 && rendered > 0.0;
                take.targetFormants[index].push_back (target);
                take.renderedFormants[index].push_back (rendered);
            }
            take.randomStates.push_back (
                vocalor::VoiceEngineTestAccess::driftRandomStates (engine));
            take.ouStates.push_back (
                vocalor::VoiceEngineTestAccess::driftOuStates (engine));
            take.vowelCountdowns.push_back (
                vocalor::VoiceEngineTestAccess::vowelDriftCountdown (engine));
        };
        const auto processChunk = [&] (bool record)
        {
            engine.process (left.data(), right.data(), hop);
            take.finite = take.finite
                && std::all_of (left.begin(), left.end(),
                                [] (float value) { return std::isfinite (value); })
                && std::all_of (right.begin(), right.end(),
                                [] (float value) { return std::isfinite (value); });
            if (record)
                observe();
        };

        for (int chunk = 0; chunk < static_cast<int> (0.5 * trackRate); ++chunk)
            processChunk (false);
        // Include the D=0 state in every first-difference and exact-return
        // comparison. Full Drift remains up for 300 chunks, enough to exercise
        // the fast pitch process and several 25 ms vowel updates.
        observe();
        parameters.instability = 1.0f;
        engine.setParameters (parameters);
        constexpr int activeChunks = 300;
        for (int chunk = 0; chunk < activeChunks; ++chunk)
            processChunk (true);

        take.returnBegin = take.pitch.size();
        parameters.instability = 0.0f;
        engine.setParameters (parameters);
        constexpr int returnChunks = 192;
        for (int chunk = 0; chunk < returnChunks; ++chunk)
            processChunk (true);
        return take;
    };

    const auto take = perform();
    const auto repeat = perform();
    expect (take.finite && repeat.finite,
            "live Drift automation produced a non-finite sample or state");
    expect (take.returnBegin == repeat.returnBegin
                && take.smoothed == repeat.smoothed
                && take.pitch == repeat.pitch
                && take.independentPitch == repeat.independentPitch
                && take.morph == repeat.morph && take.x == repeat.x
                && take.y == repeat.y
                && take.targetFormants == repeat.targetFormants
                && take.renderedFormants == repeat.renderedFormants
                && take.randomStates == repeat.randomStates
                && take.ouStates == repeat.ouStates
                && take.vowelCountdowns == repeat.vowelCountdowns,
            "repeating the same Drift automation changed its trajectory");

    const auto maximumStep = [] (const std::vector<double>& values)
    {
        double largest = 0.0;
        for (std::size_t i = 1; i < values.size(); ++i)
            largest = std::max (largest, std::abs (values[i] - values[i - 1]));
        return largest;
    };
    const auto maximumPitchStep = maximumStep (take.pitch);
    const auto maximumVowelStep = std::max ({ maximumStep (take.morph),
                                               maximumStep (take.x),
                                               maximumStep (take.y) });
    double maximumRenderedFormantStepCents = 0.0;
    for (const auto& formant : take.renderedFormants)
        for (std::size_t i = 1; i < formant.size(); ++i)
            maximumRenderedFormantStepCents = std::max (
                maximumRenderedFormantStepCents,
                1200.0 * std::abs (std::log2 (formant[i] / formant[i - 1])));
    double maximumMacroDrop = 0.0;
    double maximumMacroStep = 0.0;
    bool monotonicRise = take.returnBegin > 1;
    for (std::size_t i = 2; i < take.returnBegin; ++i)
    {
        maximumMacroStep = std::max (
            maximumMacroStep, take.smoothed[i] - take.smoothed[i - 1]);
        monotonicRise = monotonicRise && take.smoothed[i] >= take.smoothed[i - 1];
    }
    bool monotonicReturn = take.returnBegin < take.smoothed.size();
    for (std::size_t i = take.returnBegin + 1; i < take.smoothed.size(); ++i)
    {
        maximumMacroDrop = std::max (
            maximumMacroDrop, take.smoothed[i - 1] - take.smoothed[i]);
        monotonicReturn = monotonicReturn
            && take.smoothed[i] <= take.smoothed[i - 1];
    }

    const auto activeEnd = static_cast<std::ptrdiff_t> (take.returnBegin);
    const std::vector<double> activeMorph (take.morph.begin() + 1,
                                          take.morph.begin() + activeEnd);
    const std::vector<double> activeX (take.x.begin() + 1,
                                      take.x.begin() + activeEnd);
    const std::vector<double> activeY (take.y.begin() + 1,
                                      take.y.begin() + activeEnd);
    const auto activeVowelDepth = std::sqrt (
        std::pow (seriesDeviation (activeMorph), 2.0)
        + std::pow (seriesDeviation (activeX), 2.0)
        + std::pow (seriesDeviation (activeY), 2.0));
    const auto activePitchMinimum = *std::min_element (
        take.independentPitch.begin() + 1,
        take.independentPitch.begin() + activeEnd);
    const auto activePitchMaximum = *std::max_element (
        take.independentPitch.begin() + 1,
        take.independentPitch.begin() + activeEnd);

    const auto baseMorph = take.morph.front();
    const auto baseX = take.x.front();
    const auto baseY = take.y.front();
    bool pitchBoundedByMacro = true;
    bool vowelBounded = true;
    for (std::size_t i = 1; i < take.smoothed.size(); ++i)
    {
        pitchBoundedByMacro = pitchBoundedByMacro
            && std::abs (take.independentPitch[i]) <= 12.0 * take.smoothed[i] + 1.0e-5;
        vowelBounded = vowelBounded
            && std::abs (take.morph[i] - baseMorph) <= 0.101
            && std::abs (take.x[i] - baseX) <= 0.088
            && std::abs (take.y[i] - baseY) <= 0.114;
    }

    std::size_t zeroIndex = take.smoothed.size();
    for (std::size_t i = take.returnBegin; i < take.smoothed.size(); ++i)
        if (take.smoothed[i] == 0.0)
        {
            zeroIndex = i;
            break;
        }
    bool frozenAtZero = zeroIndex < take.smoothed.size();
    for (std::size_t i = zeroIndex + 1; frozenAtZero && i < take.smoothed.size(); ++i)
    {
        frozenAtZero = take.smoothed[i] == 0.0
            && take.independentPitch[i] == 0.0
            && take.morph[i] == baseMorph && take.x[i] == baseX
            && take.y[i] == baseY
            && take.randomStates[i] == take.randomStates[zeroIndex]
            && take.ouStates[i] == take.ouStates[zeroIndex]
            && take.vowelCountdowns[i] == take.vowelCountdowns[zeroIndex];
        for (const auto& formant : take.targetFormants)
            frozenAtZero = frozenAtZero && formant[i] == formant.front();
    }
    for (const auto& formant : take.targetFormants)
        frozenAtZero = frozenAtZero && formant.back() == formant.front();

    double finalRenderedResidualCents = 0.0;
    for (const auto& formant : take.renderedFormants)
        finalRenderedResidualCents = std::max (
            finalRenderedResidualCents,
            1200.0 * std::abs (std::log2 (formant.back() / formant.front())));
    constexpr std::size_t hundredMillisecondChunk = 75;
    const auto atHundredMilliseconds = take.returnBegin
        + hundredMillisecondChunk - 1;

    std::cout << "live Drift automation: first up/down "
              << std::fixed << std::setprecision (6) << take.smoothed[1] << "/"
              << take.smoothed[take.returnBegin] << ", pitch/vowel/rendered-formant max step "
              << std::fixed << std::setprecision (4) << maximumPitchStep
              << " cents / " << maximumVowelStep << " / "
              << std::setprecision (2) << maximumRenderedFormantStepCents
              << " cents; max macro return step " << std::setprecision (4)
              << maximumMacroDrop << ", exact-zero chunk "
              << (zeroIndex < take.smoothed.size()
                      ? static_cast<int> (zeroIndex - take.returnBegin + 1) : -1)
              << ", final macro/pitch "
              << std::scientific << take.smoothed.back() << "/"
              << take.independentPitch.back() << ", final tract residual "
              << finalRenderedResidualCents << " cents\n";
    expect (take.smoothed[1] > 0.064 && take.smoothed[1] < 0.065
                && take.smoothed[take.returnBegin] > 0.935
                && take.smoothed[take.returnBegin] < 0.936,
            "live Drift automation did not enter or leave on the 20 ms smoothing law");
    expect (monotonicRise && monotonicReturn
                && maximumMacroStep < 0.065 && maximumMacroDrop < 0.065,
            "Drift automation did not glide monotonically between its endpoints");
    expect (take.smoothed[atHundredMilliseconds] < 0.0068
                && std::abs (take.independentPitch[atHundredMilliseconds]) < 0.081,
            "Drift remained materially audible 100 ms after automation to zero");
    expect (activePitchMaximum - activePitchMinimum > 4.0
                && activeVowelDepth > 0.010,
            "live Drift automation did not create material pitch and vowel motion");
    expect (pitchBoundedByMacro && vowelBounded
                && std::abs (take.independentPitch[1]) < 0.78,
            "a live Drift excursion escaped its macro-scaled hard bound");
    expect (maximumPitchStep < 8.3 && maximumVowelStep < 0.065
                && maximumRenderedFormantStepCents < 8.0,
            "live Drift automation stepped or zippered a sounding target");
    expect (zeroIndex < take.returnBegin + 145 && frozenAtZero
                && take.smoothed.back() == 0.0
                && take.independentPitch.back() == 0.0,
            "Drift automation did not settle and freeze at structural zero");
    expect (finalRenderedResidualCents < 0.02,
            "the running tract did not settle smoothly after Drift returned to zero");
}

/** Drift also gives a held tract slow, bounded articulatory motion. It must be
    a depth control over a stochastic path, not another pair of LFOs, and even
    its full setting must leave the commanded vowel identifiable. */
void testVowelDriftTrajectory()
{
    vocalor::EngineParameters parameters;
    parameters.mode = vocalor::PerformanceMode::Solo;
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.0f;
    parameters.room = 0.0f;
    parameters.dynamics = 1.0f;
    parameters.vowelMorph = 0.50f;
    parameters.vowelX = 0.50f;
    parameters.vowelY = 0.50f;
    parameters.instability = 0.0f;
    parameters.legacyDriftBypass = false;

    const auto takeAt = [&parameters] (float depth, double sampleRate = 48000.0,
                                       int seedAdvance = 0,
                                       const std::vector<int>& splits = {})
    {
        auto moved = parameters;
        moved.instability = depth;
        return captureNaturalTrajectory (sampleRate, moved, 5.0, 60.0, 20.0,
                                         60, seedAdvance, splits);
    };
    const auto staticTake = takeAt (0.0f);
    const auto exactlyConstant = [] (const std::vector<double>& values)
    {
        return ! values.empty()
            && std::all_of (values.begin(), values.end(), [&values] (double value)
                            { return value == values.front(); });
    };
    expect (exactlyConstant (staticTake.vowelMorph)
                && exactlyConstant (staticTake.vowelX)
                && exactlyConstant (staticTake.vowelY),
            "Drift zero left the effective vowel position moving");
    const auto anchor = vocalor::presetVowelPosition (0);
    const double baseX = anchor.x
        + parameters.vowelMorph * (parameters.vowelX - anchor.x);
    const double baseY = anchor.y
        + parameters.vowelMorph * (parameters.vowelY - anchor.y);
    expect (! staticTake.vowelMorph.empty()
                && staticTake.vowelMorph.front() == 0.50
                && staticTake.vowelX.front() == baseX
                && staticTake.vowelY.front() == baseY,
            "Drift zero did not preserve the exact commanded vowel position");
    bool staticFormants = true;
    std::array<float, vocalor::kFormantCount> expectedStaticFormants {};
    vocalor::formantsForVowelPoint (
        parameters.profile == vocalor::VoiceProfile::Male,
        static_cast<float> (baseX), static_cast<float> (baseY),
        expectedStaticFormants.data());
    for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
    {
        const auto index = static_cast<std::size_t> (formant);
        staticFormants = staticFormants
            && exactlyConstant (staticTake.vowelFormants[index])
            && staticTake.vowelFormants[index].front()
                == expectedStaticFormants[index];
    }
    expect (staticFormants,
            "Drift zero did not preserve the exact effective vowel formants");

    const auto quarter = takeAt (0.25f);
    const auto half = takeAt (0.50f);
    const auto full = takeAt (1.0f);
    const auto depthOf = [] (const NaturalTrajectory& take)
    {
        const auto morph = seriesDeviation (take.vowelMorph);
        const auto x = seriesDeviation (take.vowelX);
        const auto y = seriesDeviation (take.vowelY);
        return std::sqrt (morph * morph + x * x + y * y);
    };
    const auto quarterDepth = depthOf (quarter);
    const auto halfDepth = depthOf (half);
    const auto fullDepth = depthOf (full);
    const auto componentSine = [] (const NaturalTrajectory& take)
    {
        double largest = 0.0;
        for (const auto* component : { &take.vowelMorph, &take.vowelX,
                                      &take.vowelY })
            largest = std::max (largest, maximumSineFitFraction (
                firstDifference (*component), take.observationRate,
                0.02, 1.0));
        return largest;
    };
    const auto fullSine = componentSine (full);
    std::cout << "vowel Drift: vector RMS D.25/.50/1 " << std::fixed
              << std::setprecision (4) << quarterDepth << "/" << halfDepth
              << "/" << fullDepth << "; largest differentiated sine share "
              << std::setprecision (3) << fullSine << "\n";

    expect (quarterDepth > 0.008 && quarterDepth < halfDepth
                && halfDepth < fullDepth
                && halfDepth > 1.6 * quarterDepth
                && fullDepth > 1.6 * halfDepth,
            "the Drift control no longer increases vowel-motion depth monotonically");
    expect (fullDepth > 0.03 && fullDepth < 0.15,
            "full Drift made the vowel path static or articulatorily excessive");
    expect (fullSine < 0.30,
            "vowel Drift collapsed into a periodic low-frequency line");
    expect (std::abs (normalisedCorrelation (full.vowelX, full.vowelY)) < 0.90,
            "the two vowel articulators collapsed onto one repeated path");

    bool allCoordinatesBounded = true;
    for (std::size_t i = 0; i < full.vowelMorph.size(); ++i)
        allCoordinatesBounded = allCoordinatesBounded
            && std::abs (full.vowelMorph[i] - 0.50) <= 0.101
            && std::abs (full.vowelX[i] - baseX) <= 0.088
            && std::abs (full.vowelY[i] - baseY) <= 0.114;
    expect (allCoordinatesBounded,
            "an effective vowel coordinate escaped its Drift hard bound");

    const auto repeat = takeAt (1.0f);
    expect (repeat.vowelMorph == full.vowelMorph
                && repeat.vowelX == full.vowelX
                && repeat.vowelY == full.vowelY,
            "the same note sequence did not reproduce its vowel Drift path");
    const auto differentSeed = takeAt (1.0f, 48000.0, 1);
    expect (std::abs (normalisedCorrelation (full.vowelX,
                                             differentSeed.vowelX)) < 0.90
                || std::abs (normalisedCorrelation (full.vowelY,
                                                    differentSeed.vowelY)) < 0.90,
            "a new note seed repeated the previous vowel path");

    const auto oddSplit = takeAt (1.0f, 48000.0, 0,
                                  { 19, 173, 7, 251, 64, 31 });
    double splitResidual = 0.0;
    for (std::size_t i = 0; i < full.vowelMorph.size()
                            && i < oddSplit.vowelMorph.size(); ++i)
    {
        splitResidual = std::max ({ splitResidual,
            std::abs (full.vowelMorph[i] - oddSplit.vowelMorph[i]),
            std::abs (full.vowelX[i] - oddSplit.vowelX[i]),
            std::abs (full.vowelY[i] - oddSplit.vowelY[i]) });
    }
    expect (splitResidual < 1.0e-7,
            "host buffer splits changed the effective vowel path");

    double shallowestRateDepth = 1.0e9;
    double deepestRateDepth = -1.0e9;
    double worstRateSine = 0.0;
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto take = takeAt (1.0f, sampleRate);
        const auto depth = depthOf (take);
        shallowestRateDepth = std::min (shallowestRateDepth, depth);
        deepestRateDepth = std::max (deepestRateDepth, depth);
        worstRateSine = std::max (worstRateSine, componentSine (take));
    }
    const auto rateDepthSpreadDb = 20.0 * std::log10 (
        deepestRateDepth / std::max (shallowestRateDepth, 1.0e-12));
    std::cout << "vowel Drift rate invariance: " << std::setprecision (2)
              << rateDepthSpreadDb << " dB vector-depth spread; worst sine share "
              << std::setprecision (3) << worstRateSine << "\n";
    expect (rateDepthSpreadDb < 2.0,
            "vowel Drift depth depends on sample rate");
    expect (worstRateSine < 0.35,
            "a supported sample rate turned vowel Drift into an LFO");

    // Classify the actual pre-register F1/F2 against all five vowel centroids.
    // Back anchors drift toward the related OOH preset and front/open anchors
    // toward AAH, so the morph motion stays within the intended phoneme family.
    int identityFailures = 0;
    double worstMeanFormantError = 0.0;
    for (const bool male : { false, true })
    {
        std::array<std::array<float, vocalor::kFormantCount>,
                   vocalor::kCardinalVowelCount> centroids {};
        for (int cardinal = 0; cardinal < vocalor::kCardinalVowelCount; ++cardinal)
        {
            const auto point = vocalor::cardinalVowelPosition (cardinal);
            vocalor::formantsForVowelPoint (
                male, point.x, point.y,
                centroids[static_cast<std::size_t> (cardinal)].data());
        }

        for (int cardinal = 0; cardinal < vocalor::kCardinalVowelCount; ++cardinal)
        {
            auto identity = parameters;
            identity.profile = male ? vocalor::VoiceProfile::Male
                                    : vocalor::VoiceProfile::Female;
            identity.vowel = cardinal >= 3 ? vocalor::Vowel::Ooh
                                           : vocalor::Vowel::Aah;
            identity.vowelMorph = 1.0f;
            const auto point = vocalor::cardinalVowelPosition (cardinal);
            identity.vowelX = point.x;
            identity.vowelY = point.y;
            identity.instability = 1.0f;
            const auto take = captureNaturalTrajectory (
                48000.0, identity, 5.0, 24.0, 20.0, male ? 48 : 60);

            for (std::size_t frame = 0;
                 frame < take.vowelFormants[0].size(); ++frame)
            {
                int nearest = -1;
                double nearestDistance = 1.0e9;
                for (int candidate = 0; candidate < vocalor::kCardinalVowelCount;
                     ++candidate)
                {
                    const auto& target = centroids[static_cast<std::size_t> (candidate)];
                    const auto f1 = std::log (take.vowelFormants[0][frame]
                                              / static_cast<double> (target[0]));
                    const auto f2 = std::log (take.vowelFormants[1][frame]
                                              / static_cast<double> (target[1]));
                    const auto distance = f1 * f1 + f2 * f2;
                    if (distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = candidate;
                    }
                }
                if (nearest != cardinal)
                    ++identityFailures;
            }

            for (int formant = 0; formant < 2; ++formant)
            {
                const auto target = centroids[static_cast<std::size_t> (cardinal)]
                                             [static_cast<std::size_t> (formant)];
                const auto error = std::abs (
                    seriesMean (take.vowelFormants[static_cast<std::size_t> (formant)])
                        / static_cast<double> (target) - 1.0);
                worstMeanFormantError = std::max (worstMeanFormantError, error);
            }
        }
    }
    std::cout << "vowel identity under full Drift: " << identityFailures
              << " misclassified frames, worst mean F1/F2 error "
              << std::setprecision (2) << 100.0 * worstMeanFormantError << " %\n";
    expect (identityFailures == 0,
            "full Drift moved an effective tract into a different cardinal vowel");
    expect (worstMeanFormantError < 0.08,
            "vowel Drift biased the mean phoneme too far from its target");

    // Morph zero selects one of the three shipped preset anchors. Its endpoint
    // window must remain exactly closed while X/Y still breathe around that
    // anchor, and that local motion must not turn AAH, OOH or UUH into one
    // another. Exercise both tract profiles and deliberately point the ignored
    // pad coordinates away from the anchor.
    constexpr std::array presetVowels {
        vocalor::Vowel::Aah, vocalor::Vowel::Ooh, vocalor::Vowel::Uuh
    };
    int presetIdentityFailures = 0;
    int presetMorphEndpointFailures = 0;
    double worstPresetMeanFormantError = 0.0;
    for (const bool male : { false, true })
    {
        std::array<std::array<float, vocalor::kFormantCount>, 3> presetCentroids {};
        std::array<std::array<float, vocalor::kFormantCount>, 3> presetSpaceAnchors {};
        for (int preset = 0; preset < 3; ++preset)
        {
            vocalor::formantsForPresetVowel (
                male, preset,
                presetCentroids[static_cast<std::size_t> (preset)].data());
            const auto point = vocalor::presetVowelPosition (preset);
            vocalor::formantsForVowelPoint (
                male, point.x, point.y,
                presetSpaceAnchors[static_cast<std::size_t> (preset)].data());
        }

        for (int preset = 0; preset < 3; ++preset)
        {
            auto identity = parameters;
            identity.profile = male ? vocalor::VoiceProfile::Male
                                    : vocalor::VoiceProfile::Female;
            identity.vowel = presetVowels[static_cast<std::size_t> (preset)];
            identity.vowelMorph = 0.0f;
            const auto anchorPoint = vocalor::presetVowelPosition (preset);
            identity.vowelX = 1.0f - anchorPoint.x;
            identity.vowelY = 1.0f - anchorPoint.y;
            identity.instability = 1.0f;
            const auto take = captureNaturalTrajectory (
                48000.0, identity, 5.0, 16.0, 20.0, male ? 48 : 60);

            for (std::size_t frame = 0;
                 frame < take.vowelFormants[0].size(); ++frame)
            {
                if (take.vowelMorph[frame] != 0.0)
                    ++presetMorphEndpointFailures;
                int nearest = -1;
                double nearestDistance = 1.0e9;
                for (int candidate = 0; candidate < 3; ++candidate)
                {
                    const auto& target =
                        presetCentroids[static_cast<std::size_t> (candidate)];
                    const auto f1 = std::log (take.vowelFormants[0][frame]
                                              / static_cast<double> (target[0]));
                    const auto f2 = std::log (take.vowelFormants[1][frame]
                                              / static_cast<double> (target[1]));
                    const auto distance = f1 * f1 + f2 * f2;
                    if (distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = candidate;
                    }
                }
                if (nearest != preset)
                    ++presetIdentityFailures;
            }

            for (int formant = 0; formant < 2; ++formant)
            {
                const auto target = presetSpaceAnchors[static_cast<std::size_t> (preset)]
                                                      [static_cast<std::size_t> (formant)];
                const auto error = std::abs (
                    seriesMean (take.vowelFormants[static_cast<std::size_t> (formant)])
                        / static_cast<double> (target) - 1.0);
                worstPresetMeanFormantError = std::max (
                    worstPresetMeanFormantError, error);
            }
        }
    }
    std::cout << "preset-anchor identity under full Drift: "
              << presetIdentityFailures << " misclassified frames, "
              << presetMorphEndpointFailures
              << " open Morph endpoints, worst mean anchor-space F1/F2 bias "
              << std::setprecision (2) << 100.0 * worstPresetMeanFormantError << " %\n";
    expect (presetIdentityFailures == 0,
            "full Drift moved a Morph-zero preset into another preset vowel");
    expect (presetMorphEndpointFailures == 0,
            "full Drift opened the Morph-zero preset endpoint");
    expect (worstPresetMeanFormantError < 0.08,
            "full Drift biased a preset anchor too far from its phoneme target");

    auto legacy = parameters;
    legacy.instability = 1.0f;
    legacy.legacyDriftBypass = true;
    const auto legacyTake = captureNaturalTrajectory (
        48000.0, legacy, 5.0, 4.0, 20.0);
    expect (seriesDeviation (legacyTake.vowelMorph) == 0.0
                && seriesDeviation (legacyTake.vowelX) == 0.0
                && seriesDeviation (legacyTake.vowelY) == 0.0,
            "legacyDriftBypass did not preserve the pre-Drift static vowel");
}

/** A sung vibrato is 5-7 Hz at an extent of about a semitone.

    The identities used to be seeded across 4.65-5.37 Hz, which is below
    Sundberg's band at every point of it, and the extent was a literal 20 cents
    per unit of the knob, which put the engine's own default at 9.1 cents --
    under the roughly 10 cents below which a vibrato is heard as unsteadiness
    rather than as vibrato. Both are read from soundingFrequencies(), which
    returns the frequency the oscillator is actually running at rather than an
    estimate of it, so there is no demodulator passband to argue about: complex
    demodulation of a twelve-voice mix returned +111 / -1112 cents in review,
    because twelve detuned carriers inside one passband beat rather than
    resolve.
*/
void testVibratoRateAndExtent()
{
    constexpr auto sampleRate = 48000.0;
    // The pitch track is read at the control rate, which is what the vibrato
    // actually moves at.
    constexpr int hop = 16;
    constexpr double trackRate = sampleRate / hop;

    // Every voice's sounding frequency, sampled every hop samples for
    // `seconds`. The first 1 s is discarded by the callers: the vibrato fades
    // in over the first half second and the note scoops up into its pitch.
    const auto pitchTracks = [] (vocalor::PerformanceMode mode, int choirSize,
                                 double seconds, std::vector<float>& rates)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        vocalor::EngineParameters parameters;
        parameters.mode = mode;
        parameters.choirSize = choirSize;
        parameters.vibrato = 1.0f;
        parameters.humanize = 0.0f;
        parameters.instability = 0.0f;
        parameters.dynamics = 1.0f;
        engine.setParameters (parameters);
        rates = vocalor::VoiceEngineTestAccess::singerVibratoRates (engine);
        engine.noteOn (60, 0.85f);

        std::vector<std::vector<double>> tracks;
        std::vector<float> left (hop, 0.0f);
        std::vector<float> right (hop, 0.0f);
        const auto steps = static_cast<int> (seconds * trackRate);
        for (int step = 0; step < steps; ++step)
        {
            engine.process (left.data(), right.data(), hop);
            const auto sounding = vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
            if (tracks.empty())
                tracks.resize (sounding.size());
            for (std::size_t voice = 0; voice < tracks.size() && voice < sounding.size(); ++voice)
                tracks[voice].push_back (static_cast<double> (sounding[voice]));
        }
        return tracks;
    };

    // Reciprocal of the mean interval between rising crossings of the track's
    // own mean. A frequency track is not a sinusoid -- the scoop and the drift
    // ride under it -- so a crossing count is the estimator that does not care.
    const auto rateOf = [] (const std::vector<double>& track)
    {
        double mean = 0.0;
        for (const auto value : track)
            mean += value;
        mean /= static_cast<double> (track.size());

        std::vector<double> crossings;
        for (std::size_t i = 1; i < track.size(); ++i)
            if (track[i - 1] <= mean && track[i] > mean)
                crossings.push_back (static_cast<double> (i - 1)
                                     + (mean - track[i - 1]) / (track[i] - track[i - 1]));
        if (crossings.size() < 2)
            return 0.0;
        const auto interval = (crossings.back() - crossings.front())
                            / static_cast<double> (crossings.size() - 1);
        return trackRate / interval;
    };

    const auto extentOf = [] (const std::vector<double>& track)
    {
        double mean = 0.0;
        for (const auto value : track)
            mean += value;
        mean /= static_cast<double> (track.size());

        double lowest = 1.0e9;
        double highest = -1.0e9;
        for (const auto value : track)
        {
            const auto cents = 1200.0 * std::log2 (value / mean);
            lowest = std::min (lowest, cents);
            highest = std::max (highest, cents);
        }
        return 0.5 * (highest - lowest);
    };

    const auto tail = [] (const std::vector<double>& track)
    {
        return std::vector<double> (track.begin()
                                        + static_cast<std::ptrdiff_t> (trackRate),
                                    track.end());
    };

    std::vector<float> seededRates;
    const auto solo = pitchTracks (vocalor::PerformanceMode::Solo, 1, 3.0, seededRates);
    expect (solo.size() == 1, "the solo vibrato probe did not find exactly one voice");
    if (solo.size() == 1)
    {
        const auto track = tail (solo[0]);
        const auto rate = rateOf (track);
        const auto extent = extentOf (track);
        std::cout << "solo vibrato: " << std::fixed << std::setprecision (3) << rate
                  << " Hz, +/-" << std::setprecision (1) << extent << " cents\n";
        // Sundberg's definition, and the band the 2022 systematic review gives.
        expect (rate > 5.5 && rate < 7.2,
                "the solo vibrato rate is outside the 5.5-7.2 Hz band a sung vibrato occupies");
        // "An extent of about +/-1 semitone". The identity depth multiplies it,
        // so singer 0 reaches 108.6 rather than exactly 100.
        expect (extent > 80.0,
                "Vibrato at 100 % does not reach a solo singer's extent");
        // A reseeded rate that nothing reads would pass the band check on its
        // own, so the track and the identity have to agree.
        expect (! seededRates.empty()
                    && std::abs (rate - static_cast<double> (seededRates[0]))
                           < 0.02 * static_cast<double> (seededRates[0]),
                "the sounding vibrato rate does not follow the singer identity's own rate");
    }

    std::vector<float> choirRates;
    const auto choir = pitchTracks (vocalor::PerformanceMode::Choir, 12, 3.0, choirRates);
    expect (choir.size() == 12, "the choir vibrato probe did not find twelve voices");
    double slowest = 1.0e9;
    double fastest = -1.0e9;
    double narrowest = 1.0e9;
    double widest = -1.0e9;
    for (const auto& voice : choir)
    {
        const auto track = tail (voice);
        const auto rate = rateOf (track);
        const auto extent = extentOf (track);
        slowest = std::min (slowest, rate);
        fastest = std::max (fastest, rate);
        narrowest = std::min (narrowest, extent);
        widest = std::max (widest, extent);
    }
    std::cout << "choir vibrato: rates " << std::setprecision (3) << slowest << " - "
              << fastest << " Hz, extents +/-" << std::setprecision (1) << narrowest
              << " - +/-" << widest << " cents\n";
    expect (slowest > 5.5 && fastest < 7.2,
            "the twelve singer vibrato rates do not all sit in the 5.5-7.2 Hz band");
    // A section sings a narrower vibrato than a soloist: twelve voices at solo
    // extent smear into a chorus instead of reading as a section.
    expect (narrowest > 25.0 && widest < 50.0,
            "the ensemble vibrato extent is outside the band a section sings");
}

/** Instability turns a clocked LFO into a sequence of related sung gestures.

    Rate is the more stable dimension of a trained vibrato: published cycle
    measurements put ordinary rate scatter around a few percent, while extent
    moves appreciably more. The contour is not exactly sinusoidal either; F0
    commonly rises a little faster than it falls. Measure those properties on
    the oscillator's sounding-frequency trajectory so a random value that is
    drawn but never reaches the voice cannot satisfy the test.
*/
void testVibratoInstability()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int hop = 16;
    constexpr double trackRate = sampleRate / hop;
    constexpr double concertC4 = 261.6255653005986;

    struct CycleStatistics
    {
        std::vector<double> periods;
        std::vector<double> extents;
        std::vector<double> riseToFall;
        double periodCv = 0.0;
        double extentCv = 0.0;
        double meanRate = 0.0;
        double meanExtent = 0.0;
        double boundaryStep = 0.0;
        double boundaryKink = 0.0;
    };

    const auto coefficientOfVariation = [] (const std::vector<double>& values)
    {
        double mean = 0.0;
        for (const auto value : values)
            mean += value;
        mean /= static_cast<double> (values.size());

        double squared = 0.0;
        for (const auto value : values)
            squared += (value - mean) * (value - mean);
        return std::sqrt (squared / static_cast<double> (values.size())) / mean;
    };

    const auto measure = [&] (float vibrato, float instability,
                              bool legacyDriftBypass = false)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        vocalor::EngineParameters parameters;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vibrato = vibrato;
        parameters.humanize = 0.0f;
        parameters.instability = instability;
        parameters.legacyDriftBypass = legacyDriftBypass;
        parameters.dynamics = 1.0f;
        parameters.room = 0.0f;
        engine.setParameters (parameters);
        engine.reset();
        engine.noteOn (60, 0.85f);

        // Two seconds clears the scoop and vibrato fade. The following eight
        // seconds contain roughly fifty cycles: enough to measure variation,
        // short enough that this remains a focused DSP regression.
        constexpr double discardSeconds = 2.0;
        constexpr double measuredSeconds = 8.0;
        const auto steps = static_cast<int> ((discardSeconds + measuredSeconds) * trackRate);
        const auto discardSteps = static_cast<int> (discardSeconds * trackRate);
        std::vector<double> cents;
        std::vector<double> phases;
        std::vector<double> independentPitch;
        cents.reserve (static_cast<std::size_t> (steps - discardSteps));
        phases.reserve (static_cast<std::size_t> (steps - discardSteps));
        independentPitch.reserve (static_cast<std::size_t> (steps - discardSteps));
        std::array<float, hop> left {};
        std::array<float, hop> right {};
        for (int step = 0; step < steps; ++step)
        {
            engine.process (left.data(), right.data(), hop);
            if (step < discardSteps)
                continue;
            const auto frequencies = vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
            if (! frequencies.empty())
            {
                cents.push_back (1200.0 * std::log2 (
                    static_cast<double> (frequencies.front()) / concertC4));
                phases.push_back (
                    vocalor::VoiceEngineTestAccess::vibratoModulationState (engine)[0]);
                independentPitch.push_back (
                    vocalor::VoiceEngineTestAccess::independentPitchDriftCents (engine));
            }
        }

        // Isolate the deliberate vibrato here: the straight-tone test above
        // already validates the independently sounding OU pitch component.
        // Leaving it in would attribute unrelated fold wander to cycle depth
        // and to a redraw kink. The remainder is still measured at the actual
        // oscillator rather than by inspecting a random modulation target.
        for (std::size_t i = 0; i < cents.size(); ++i)
            cents[i] -= independentPitch[i];

        // Read cycle boundaries from oscillator phase rather than from an F0
        // centre crossing, which can be shifted or multiplied by noise.
        std::vector<double> crossings;
        for (std::size_t i = 1; i < phases.size(); ++i)
        {
            if (phases[i] >= phases[i - 1])
                continue;
            const auto before = 1.0 - phases[i - 1];
            const auto after = phases[i];
            crossings.push_back (static_cast<double> (i - 1)
                + before / std::max (before + after, 1.0e-12));
        }

        CycleStatistics result;
        for (std::size_t i = 2; i < cents.size() && i < phases.size(); ++i)
        {
            if (phases[i] >= phases[i - 1])
                continue;
            const auto step = cents[i] - cents[i - 1];
            const auto precedingStep = cents[i - 1] - cents[i - 2];
            result.boundaryStep = std::max (result.boundaryStep, std::abs (step));
            result.boundaryKink = std::max (
                result.boundaryKink, std::abs (step - precedingStep));
        }
        std::vector<std::size_t> maxima;
        std::vector<std::size_t> minima;
        for (std::size_t cycle = 0; cycle + 1 < crossings.size(); ++cycle)
        {
            result.periods.push_back ((crossings[cycle + 1] - crossings[cycle]) / trackRate);
            const auto first = static_cast<std::size_t> (std::ceil (crossings[cycle]));
            const auto last = std::min (
                static_cast<std::size_t> (std::floor (crossings[cycle + 1])),
                cents.size() - 1);
            if (last <= first)
                continue;

            const auto maximum = std::max_element (cents.begin() + static_cast<std::ptrdiff_t> (first),
                                                    cents.begin() + static_cast<std::ptrdiff_t> (last + 1));
            const auto minimum = std::min_element (cents.begin() + static_cast<std::ptrdiff_t> (first),
                                                    cents.begin() + static_cast<std::ptrdiff_t> (last + 1));
            maxima.push_back (static_cast<std::size_t> (maximum - cents.begin()));
            minima.push_back (static_cast<std::size_t> (minimum - cents.begin()));
            result.extents.push_back (0.5 * (*maximum - *minimum));
        }

        for (std::size_t cycle = 0; cycle + 1 < maxima.size(); ++cycle)
        {
            if (minima[cycle] <= maxima[cycle]
                || maxima[cycle + 1] <= minima[cycle])
                continue;
            const auto fall = static_cast<double> (minima[cycle] - maxima[cycle]);
            const auto rise = static_cast<double> (maxima[cycle + 1] - minima[cycle]);
            result.riseToFall.push_back (rise / fall);
        }

        if (! result.periods.empty() && ! result.extents.empty())
        {
            result.periodCv = coefficientOfVariation (result.periods);
            result.extentCv = coefficientOfVariation (result.extents);
            double meanPeriod = 0.0;
            for (const auto period : result.periods)
                meanPeriod += period;
            result.meanRate = static_cast<double> (result.periods.size()) / meanPeriod;
            result.meanExtent = seriesMean (result.extents);
        }
        return result;
    };

    const auto fixed = measure (1.0f, 0.0f);
    const auto natural = measure (1.0f, 1.0f);
    const auto moderate = measure (0.40f, 0.40f);
    const auto legacyAt40 = measure (0.40f, 0.40f, true);
    const auto linearEquivalent = measure (0.40f, 0.16f, false);
    const auto legacyFull = measure (0.40f, 1.0f, true);
    expect (fixed.periods.size() > 30 && natural.periods.size() > 30,
            "the instability probe did not observe enough complete vibrato cycles");
    if (fixed.periods.size() <= 30 || natural.periods.size() <= 30)
        return;

    std::cout << "vibrato instability: fixed CV rate/depth " << std::fixed
              << std::setprecision (3) << 100.0 * fixed.periodCv << "/"
              << 100.0 * fixed.extentCv << " %, natural "
              << 100.0 * natural.periodCv << "/" << 100.0 * natural.extentCv
              << " %, mean " << std::setprecision (2) << natural.meanRate << " Hz\n";

    expect (fixed.periodCv < 0.002 && fixed.extentCv < 0.005,
            "Instability at zero did not restore effectively fixed vibrato cycles");
    expect (natural.periodCv > 0.025 && natural.periodCv < 0.16,
            "full Instability produced implausibly little or unbounded rate variation");
    expect (natural.extentCv > 0.08 && natural.extentCv < 0.35,
            "full Instability produced implausibly little or unbounded extent variation");
    expect (natural.extentCv > 1.5 * natural.periodCv,
            "vibrato extent is not materially less regular than vibrato rate");
    expect (natural.meanRate > 4.5 && natural.meanRate < 8.0,
            "Instability moved the mean vibrato rate outside a broad human range");

    const auto slowestPeriod = *std::max_element (natural.periods.begin(), natural.periods.end());
    const auto fastestPeriod = *std::min_element (natural.periods.begin(), natural.periods.end());
    const auto narrowest = *std::min_element (natural.extents.begin(), natural.extents.end());
    const auto widest = *std::max_element (natural.extents.begin(), natural.extents.end());
    expect (1.0 / slowestPeriod > 4.0 && 1.0 / fastestPeriod < 9.0
                && narrowest > 40.0 && widest < 175.0,
            "full Instability allowed an individual cycle to leave expressive bounds");

    double meanRiseToFall = 0.0;
    for (const auto ratio : natural.riseToFall)
        meanRiseToFall += ratio;
    if (! natural.riseToFall.empty())
        meanRiseToFall /= static_cast<double> (natural.riseToFall.size());
    std::cout << "vibrato contour: mean rise/fall duration " << std::fixed
              << std::setprecision (3) << meanRiseToFall << "\n";
    expect (! natural.riseToFall.empty(),
            "the instability probe could not resolve the vibrato rise and fall");
    expect (meanRiseToFall > 0.65 && meanRiseToFall < 0.95,
            "the unstable vibrato contour does not rise mildly faster than it falls");

    std::cout << "moderate vibrato V.40/D.40: CV rate/depth "
              << std::fixed << std::setprecision (3)
              << 100.0 * moderate.periodCv << "/"
              << 100.0 * moderate.extentCv << " %, mean "
              << std::setprecision (2) << moderate.meanRate << " Hz, +/-"
              << moderate.meanExtent << " cents; wrap step/kink "
              << std::setprecision (4) << moderate.boundaryStep << "/"
              << moderate.boundaryKink << " cents\n";
    expect (moderate.periods.size() > 30 && moderate.extents.size() > 30,
            "the moderate-vibrato probe did not observe enough complete cycles");
    expect (moderate.meanRate > 5.0 && moderate.meanRate < 7.5,
            "moderate vibrato left the natural sung-rate band");
    expect (moderate.meanExtent > 14.0 && moderate.meanExtent < 32.0,
            "moderate Vibrato no longer produces a restrained musical extent");
    expect (moderate.periodCv > 0.01 && moderate.periodCv < 0.12,
            "moderate Drift produced a clocked or unbounded vibrato rate");
    expect (moderate.extentCv > 0.03 && moderate.extentCv < 0.25,
            "moderate Drift produced a fixed or unbounded vibrato extent");
    if (! moderate.periods.empty() && ! moderate.extents.empty())
    {
        const auto slowest = *std::max_element (moderate.periods.begin(),
                                                moderate.periods.end());
        const auto fastest = *std::min_element (moderate.periods.begin(),
                                                moderate.periods.end());
        const auto narrowestModerate = *std::min_element (moderate.extents.begin(),
                                                          moderate.extents.end());
        const auto widestModerate = *std::max_element (moderate.extents.begin(),
                                                       moderate.extents.end());
        expect (1.0 / slowest > 4.0 && 1.0 / fastest < 9.0
                    && narrowestModerate > 8.0 && widestModerate < 45.0,
                "a moderate vibrato cycle escaped expressive bounds");
    }
    expect (moderate.boundaryStep < 1.0 && moderate.boundaryKink < 0.10,
            "a moderate-vibrato redraw left an audible pitch step or kink");

    // Old sessions may contain a non-zero value for the repurposed control.
    // Their model marker must retain 1.3's *linear* cycle-variation law. The
    // current model at D=.16 has the same sqrt(D)=.40 cycle amount as a legacy
    // patch at D=.40; their phase periods must therefore coincide even though
    // only the current model also receives independent pitch/vowel motion.
    double largestLegacyPeriodResidual = 0.0;
    const auto comparablePeriods = std::min (legacyAt40.periods.size(),
                                             linearEquivalent.periods.size());
    for (std::size_t i = 0; i < comparablePeriods; ++i)
        largestLegacyPeriodResidual = std::max (
            largestLegacyPeriodResidual,
            std::abs (legacyAt40.periods[i] - linearEquivalent.periods[i]));
    std::cout << "legacy vibrato law: D.40/full CV rate/depth "
              << std::fixed << std::setprecision (3)
              << 100.0 * legacyAt40.periodCv << "/"
              << 100.0 * legacyAt40.extentCv << " %, "
              << 100.0 * legacyFull.periodCv << "/"
              << 100.0 * legacyFull.extentCv
              << " %; equivalent-period residual " << std::scientific
              << largestLegacyPeriodResidual << " s\n";
    expect (legacyAt40.periodCv > 0.01
                && legacyAt40.periodCv < moderate.periodCv,
            "legacyDriftBypass lost the old linear rate variation law");
    expect (legacyAt40.extentCv > 0.03
                && legacyAt40.extentCv < moderate.extentCv,
            "legacyDriftBypass lost the old linear depth variation law");
    expect (legacyFull.periodCv > 0.04 && legacyFull.periodCv < 0.12
                && legacyFull.extentCv > 0.10 && legacyFull.extentCv < 0.30,
            "legacyDriftBypass no longer exposes the bounded full 1.3 variation");
    expect (comparablePeriods > 30
                && comparablePeriods == legacyAt40.periods.size()
                && comparablePeriods == linearEquivalent.periods.size()
                && largestLegacyPeriodResidual < 1.0e-6,
            "legacyDriftBypass no longer maps Drift linearly onto cycle rate");

    // Match a newly started legacy note with an otherwise identical copy whose
    // age alone is offset beyond the fade. Since phase, cycle draws and shape
    // stay identical, the ratio of their direct-gain excursions is the actual
    // fade envelope. This observes (rather than merely reads) 1.3's fixed
    // 160 ms start / 340 ms smoothstep even when the recalled Drift is nonzero.
    vocalor::EngineParameters legacyOnsetParameters;
    legacyOnsetParameters.mode = vocalor::PerformanceMode::Solo;
    legacyOnsetParameters.vibrato = 0.20f;
    legacyOnsetParameters.humanize = 0.0f;
    legacyOnsetParameters.instability = 0.40f;
    legacyOnsetParameters.legacyDriftBypass = true;
    legacyOnsetParameters.dynamics = 1.0f;
    legacyOnsetParameters.room = 0.0f;
    const auto makeLegacyOnsetEngine = [&legacyOnsetParameters]
    {
        auto engine = std::make_unique<vocalor::VoiceEngine>();
        engine->prepare (sampleRate, blockSize);
        engine->setParameters (legacyOnsetParameters);
        engine->reset();
        engine->noteOn (60, 0.85f);
        return engine;
    };
    auto fadingLegacy = makeLegacyOnsetEngine();
    auto fullyFadedLegacy = makeLegacyOnsetEngine();
    vocalor::VoiceEngineTestAccess::offsetHeldVoiceAge (
        *fullyFadedLegacy, static_cast<std::uint64_t> (sampleRate));

    std::array<float, hop> onsetLeft {};
    std::array<float, hop> onsetRight {};
    std::array<float, hop> referenceLeft {};
    std::array<float, hop> referenceRight {};
    bool legacyPreFadeWasUnity = true;
    double largestFadeResidual = 0.0;
    int resolvedFadeSamples = 0;
    int resolvedMidFadeSamples = 0;
    const int onsetSteps = static_cast<int> (0.65 * trackRate);
    for (int step = 0; step < onsetSteps; ++step)
    {
        fadingLegacy->process (onsetLeft.data(), onsetRight.data(), hop);
        fullyFadedLegacy->process (
            referenceLeft.data(), referenceRight.data(), hop);
        const auto fadingGain = vocalor::VoiceEngineTestAccess::vibratoModulationState (
            *fadingLegacy)[1];
        const auto referenceGain =
            vocalor::VoiceEngineTestAccess::vibratoModulationState (
                *fullyFadedLegacy)[1];
        const double controlAge = static_cast<double> (step) / trackRate;
        if (controlAge < 0.16)
            legacyPreFadeWasUnity = legacyPreFadeWasUnity && fadingGain == 1.0f;

        const auto referenceExcursion = static_cast<double> (referenceGain) - 1.0;
        if (std::abs (referenceExcursion) < 1.0e-4)
            continue;
        const auto position = std::clamp ((controlAge - 0.16) / 0.34, 0.0, 1.0);
        const auto expectedFade = position * position * (3.0 - 2.0 * position);
        const auto observedFade = (static_cast<double> (fadingGain) - 1.0)
                                / referenceExcursion;
        largestFadeResidual = std::max (
            largestFadeResidual, std::abs (observedFade - expectedFade));
        ++resolvedFadeSamples;
        if (position > 0.05 && position < 0.95)
            ++resolvedMidFadeSamples;
    }
    std::cout << "legacy vibrato fade: 160/340 ms smoothstep residual "
              << std::scientific << largestFadeResidual << " across "
              << resolvedMidFadeSamples << " mid-fade controls\n";
    expect (legacyPreFadeWasUnity,
            "legacyDriftBypass started vibrato before the historical 160 ms delay");
    expect (resolvedFadeSamples > 500 && resolvedMidFadeSamples > 300
                && largestFadeResidual < 0.01,
            "legacyDriftBypass changed the historical 340 ms vibrato fade");
}

/** A sung vibrato modulates the amplitude, not only the pitch.

    The engine had one amplitude source and it was the passive one: harmonics
    sweeping static formant skirts. Measured as the magnitude of the envelope's
    own component at the singer's vibrato rate that is 0.10 dB on a held C5 and
    0.22 dB at C6, against a 0.001 dB floor with the vibrato off -- a tenth of
    what a singer produces. The cricothyroid oscillation that carries the pitch
    also moves subglottal pressure and adduction, so the level and the source
    slope swing on the same cycle, and both notes are chosen because the passive
    contribution alone still does not reach 1 dB at either of them once the
    extent is a real one: with the laryngeal modulation forced out but the new
    extent in force it reads 0.76 dB at C5 and 0.80 dB at C6.
*/
void testVibratoAmplitude()
{
    constexpr auto sampleRate = 48000.0;

    // Magnitude of the envelope's component at `vibratoHz`, expressed as the
    // decibels the level rises above its own mean. A peak-to-trough statistic
    // is an extremum dominated by shimmer and jitter; a rate-selective one
    // reads a true zero when there is no vibrato.
    const auto modulationDb = [] (int midiNote, float vibrato, double& vibratoHz)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        vocalor::EngineParameters parameters;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vibrato = vibrato;
        parameters.humanize = 0.0f;
        parameters.instability = 0.0f;
        parameters.dynamics = 1.0f;
        engine.setParameters (parameters);
        // Solo sounds singer 0, so the rate the metric selects on is known
        // rather than estimated.
        vibratoHz = static_cast<double> (
            vocalor::VoiceEngineTestAccess::singerVibratoRates (engine)[0]);
        engine.noteOn (midiNote, 0.85f);

        const auto total = static_cast<int> (3.0 * sampleRate);
        const auto audio = renderMono (engine, total);

        // Rectified one-pole follower, 10 ms: fast enough to follow a 6 Hz
        // modulation without ripple at the fundamental.
        std::vector<double> envelope (audio.size(), 0.0);
        double state = 0.0;
        const double coefficient = 1.0 - std::exp (-1.0 / (0.010 * sampleRate));
        for (std::size_t i = 0; i < audio.size(); ++i)
        {
            state += coefficient * (std::abs (static_cast<double> (audio[i])) - state);
            envelope[i] = state;
        }

        // A whole number of vibrato cycles from t = 1 s, so the projection does
        // not leak the mean into the component.
        const auto start = static_cast<std::size_t> (sampleRate);
        const auto cycles = static_cast<int> (static_cast<double> (audio.size() - start)
                                              * vibratoHz / sampleRate);
        const auto length = static_cast<std::size_t> (static_cast<double> (cycles)
                                                      * sampleRate / vibratoHz);
        double mean = 0.0;
        for (std::size_t i = 0; i < length; ++i)
            mean += envelope[start + i];
        mean /= static_cast<double> (length);

        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const auto phase = 2.0 * 3.14159265358979323846 * vibratoHz
                             * static_cast<double> (i) / sampleRate;
            real += (envelope[start + i] - mean) * std::cos (phase);
            imaginary += (envelope[start + i] - mean) * std::sin (phase);
        }
        const auto magnitude = 2.0 * std::sqrt (real * real + imaginary * imaginary)
                             / static_cast<double> (length);
        return 20.0 * std::log10 ((mean + magnitude) / std::max (mean, 1.0e-12));
    };

    for (const int midiNote : { 72, 84 })
    {
        double vibratoHz = 0.0;
        const auto full = modulationDb (midiNote, 1.0f, vibratoHz);
        const auto none = modulationDb (midiNote, 0.0f, vibratoHz);
        std::cout << "vibrato amplitude at MIDI " << midiNote << ": " << std::fixed
                  << std::setprecision (3) << full << " dB at 100 %, " << none
                  << " dB at 0 %\n";
        expect (full > 1.0,
                "the vibrato carries no laryngeal amplitude modulation: "
                "only the passive formant-skirt contribution is present");
        // The depth follows the extent in force, so with no extent there is
        // nothing to modulate.
        expect (none < 0.05,
                "the amplitude modulation is not tied to the vibrato extent: "
                "it is present with the vibrato switched off");
    }
}

/** Natural vibrato keeps its airflow lead without turning each cycle redraw
    into a tiny amplitude step.

    The direct source gain and the two cascaded shelf gains are sampled after
    the exact ramps used by renderVoice(). Their product is therefore the
    effective high-band AM law. A discontinuity at the F0 wrap would appear as
    an outlying one-hop change and, more strongly, as a first-difference kink:
    the broadband control edge that creates sidebands around every partial.
*/
void testNaturalVibratoAmplitudeAndContinuity()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int hop = 16;
    constexpr double trackRate = sampleRate / hop;

    struct AmStatistics
    {
        double directMinimum = 1.0e9;
        double directMaximum = -1.0e9;
        double highMinimum = 1.0e9;
        double highMaximum = -1.0e9;
        double boundaryDirectStep = 0.0;
        double boundaryHighStep = 0.0;
        double boundaryDirectKink = 0.0;
        double boundaryHighKink = 0.0;
        std::size_t boundaries = 0;

        [[nodiscard]] double directDb() const noexcept
        {
            return 20.0 * std::log10 (directMaximum / directMinimum);
        }

        [[nodiscard]] double highDb() const noexcept
        {
            return 20.0 * std::log10 (highMaximum / highMinimum);
        }
    };

    const auto measure = [&] (float vibrato, float instability)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        vocalor::EngineParameters parameters;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vibrato = vibrato;
        parameters.humanize = 0.0f;
        parameters.instability = instability;
        parameters.dynamics = 1.0f;
        parameters.room = 0.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.85f);

        constexpr double discardSeconds = 1.0;
        constexpr double measuredSeconds = 9.0;
        const auto steps = static_cast<int> ((discardSeconds + measuredSeconds) * trackRate);
        const auto discardSteps = static_cast<int> (discardSeconds * trackRate);
        std::array<float, hop> left {};
        std::array<float, hop> right {};
        AmStatistics result;
        std::array<double, 3> previous {};
        std::array<double, 3> beforePrevious {};
        bool havePrevious = false;
        bool haveBeforePrevious = false;

        for (int step = 0; step < steps; ++step)
        {
            engine.process (left.data(), right.data(), hop);
            const auto state = vocalor::VoiceEngineTestAccess::vibratoModulationState (engine);
            const double phase = state[0];
            const double direct = state[1];
            const double shelf = state[2];
            const double high = direct * shelf * shelf;

            if (step >= discardSteps)
            {
                result.directMinimum = std::min (result.directMinimum, direct);
                result.directMaximum = std::max (result.directMaximum, direct);
                result.highMinimum = std::min (result.highMinimum, high);
                result.highMaximum = std::max (result.highMaximum, high);

                if (havePrevious && phase < previous[0])
                {
                    ++result.boundaries;
                    const double directStep = direct - previous[1];
                    const double highStep = high - previous[2];
                    result.boundaryDirectStep = std::max (
                        result.boundaryDirectStep, std::abs (directStep));
                    result.boundaryHighStep = std::max (
                        result.boundaryHighStep, std::abs (highStep));
                    if (haveBeforePrevious)
                    {
                        result.boundaryDirectKink = std::max (
                            result.boundaryDirectKink,
                            std::abs (directStep - (previous[1] - beforePrevious[1])));
                        result.boundaryHighKink = std::max (
                            result.boundaryHighKink,
                            std::abs (highStep - (previous[2] - beforePrevious[2])));
                    }
                }
            }

            beforePrevious = previous;
            haveBeforePrevious = havePrevious;
            previous = { phase, direct, high };
            havePrevious = true;
        }
        return result;
    };

    const auto moderate = measure (0.38f, 0.38f);
    const auto demo = measure (0.50f, 0.44f);
    const auto natural = measure (1.0f, 1.0f);
    const auto report = [] (const char* label, const AmStatistics& result)
    {
        std::cout << label << " vibrato AM: direct/high " << std::fixed
                  << std::setprecision (3) << result.directDb() << "/"
                  << result.highDb() << " dB peak-to-trough; redraw step "
                  << std::scientific << result.boundaryDirectStep << "/"
                  << result.boundaryHighStep
                  << ", kink " << result.boundaryDirectKink << "/"
                  << result.boundaryHighKink << "\n" << std::defaultfloat;
    };
    report ("model-4 V.38/D.38", moderate);
    report ("demo V.50/I.44", demo);
    report ("full-natural V1/I1", natural);

    // These are control-domain peak-to-trough figures over the deterministic
    // nine-second probe, not the fixed-rate audio projection above. Pin both
    // useful non-zero settings so reducing clicks cannot silently remove the
    // airflow gesture or reintroduce the old three-deep high-band modulation.
    expect (std::abs (moderate.directDb() - 0.624) < 0.15
                && std::abs (moderate.highDb() - 1.249) < 0.18,
            "the moderate Drift setting changed its direct/high-band AM law");
    expect (std::abs (demo.directDb() - 0.922) < 0.18
                && std::abs (demo.highDb() - 1.845) < 0.25,
            "the demonstration Drift setting changed its direct/high-band AM law");
    expect (std::abs (natural.directDb() - 2.93) < 0.45
                && std::abs (natural.highDb() - 5.85) < 0.65,
            "full Drift changed its direct/high-band AM law");

    for (const auto* result : { &moderate, &natural })
    {
        expect (result->boundaries > 40,
                "the natural-AM probe did not observe enough cycle redraws");
        expect (result->highDb() > 1.7 * result->directDb()
                    && result->highDb() < 2.8 * result->directDb(),
                "the two presence shelves no longer give the high band roughly twice the AM");
        expect (result->boundaryDirectStep < 0.005
                    && result->boundaryHighStep < 0.012,
                "a cycle redraw introduced an amplitude step large enough to create sidebands");
        expect (result->boundaryDirectKink < 0.002
                    && result->boundaryHighKink < 0.005,
                "a cycle redraw left a broadband kink in the amplitude trajectory");
        expect (result->boundaryDirectKink
                        < 0.35 * result->boundaryDirectStep + 1.0e-5
                    && result->boundaryHighKink
                        < 0.35 * result->boundaryHighStep + 1.0e-5,
                "the redraw kink is an outlier rather than the slope of continuous AM");
    }
}

/** A choral entry is twelve people reacting to one cue, and no two attacks of
    the same chord are the same attack.

    The engine drew the twelve entry offsets once in prepare() and reset the
    twelve vibrato phases to 0.173 x singer index, so three identical note-ons
    produced the same twelve delays to the sample -- 856, 365, 618, 719, 826,
    663, 775, 380, 329, 308, 464, 741 -- and the same twelve phases to four
    decimals. The scatter those carried was a 4.098 ms population standard
    deviation about a 12.229 mean, spanning 6.42-17.83 ms.

    Both are now drawn per note from voice.noiseState, which is a hash of the
    generation, the root and the singer index, so the render is still a pure
    function of the note sequence: two engines given the same notes still
    render identically however the host slices its buffers.

    The window is asserted two-sided. An unbounded floor on a scatter parameter
    is how an instrument acquires a flam, and the contract is written on the
    audible onset -- the entry delay plus the time the singer's direct path
    takes, which is zero until step 8 gives the singers positions -- rather than
    on the entry-delay field, so an implementation cannot sit inside the window
    on the field and still put two voices 68 ms apart at the ear.
*/
void testEnsembleTimingIsRedrawn()
{
    constexpr auto sampleRate = 48000.0;
    auto parameters = makeParameters (1, 0, 0, 0);   // Choir
    parameters.choirSize = 12;
    parameters.humanize = 1.0f;

    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (parameters);

    std::array<std::vector<int>, 3> delays;
    std::array<std::vector<float>, 3> phases;
    for (int take = 0; take < 3; ++take)
    {
        engine.noteOn (60, 0.8f);
        delays[static_cast<std::size_t> (take)]
            = vocalor::VoiceEngineTestAccess::entryDelays (engine);
        phases[static_cast<std::size_t> (take)]
            = vocalor::VoiceEngineTestAccess::vibratoPhases (engine);
        // Read before rendering: the entry delay is a countdown, and half a
        // second later every voice has spent it.
        const auto onsets = vocalor::VoiceEngineTestAccess::audibleOnsets (engine);
        render (engine, static_cast<int> (sampleRate * 0.5));

        expect (onsets.size() == 12,
                "the twelve-singer choir did not put twelve voices on the note");
        if (onsets.size() != 12)
            return;

        double mean = 0.0;
        for (const auto value : onsets)
            mean += static_cast<double> (value);
        mean /= static_cast<double> (onsets.size());
        double square = 0.0;
        for (const auto value : onsets)
            square += (static_cast<double> (value) - mean) * (static_cast<double> (value) - mean);
        // Population, not sample: the bounds below are drawn against the
        // population estimator, which reads 4.098 ms on the twelve values the
        // engine used to produce where the sample estimator reads 4.280.
        const auto deviationMs = 1000.0 * std::sqrt (square / static_cast<double> (onsets.size()))
                               / sampleRate;
        const auto earliest = *std::min_element (onsets.begin(), onsets.end());
        const auto latest = *std::max_element (onsets.begin(), onsets.end());
        const auto spanMs = 1000.0 * static_cast<double> (latest - earliest) / sampleRate;

        std::cout << "ensemble entry take " << take << ": population sd "
                  << std::fixed << std::setprecision (3) << deviationMs
                  << " ms, span " << std::setprecision (2) << spanMs << " ms\n";

        expect (deviationMs > 8.0 && deviationMs < 18.0,
                "take " + std::to_string (take) + " scattered the ensemble entry by "
                    + std::to_string (deviationMs)
                    + " ms, outside the 8-18 ms the section is voiced for");
        expect (spanMs <= 55.0,
                "take " + std::to_string (take) + " left a voice "
                    + std::to_string (spanMs) + " ms behind the earliest, which is a flam");
        // The entry spread on its own has to leave room for step 8's
        // propagation delays, which add up to 13.1 ms of their own on top and
        // whose extremes can land on the same singer as the entry's.
        const auto& entries = delays[static_cast<std::size_t> (take)];
        const auto entrySpanMs =
            1000.0 * static_cast<double> (*std::max_element (entries.begin(), entries.end())
                                          - *std::min_element (entries.begin(), entries.end()))
            / sampleRate;
        expect (entrySpanMs < 40.0,
                "take " + std::to_string (take) + " spent " + std::to_string (entrySpanMs)
                    + " ms of entry spread, leaving nothing for the propagation delay");
    }

    int closestDelay = std::numeric_limits<int>::max();
    float closestPhase = 1.0f;
    for (std::size_t a = 0; a < delays.size(); ++a)
    {
        for (std::size_t b = a + 1; b < delays.size(); ++b)
        {
            expect (delays[a] != delays[b],
                    "two repeats of the same note produced the same twelve entry delays");
            expect (phases[a] != phases[b],
                    "two repeats of the same note produced the same twelve vibrato phases");
            for (std::size_t i = 0; i < delays[a].size() && i < delays[b].size(); ++i)
            {
                closestDelay = std::min (closestDelay, std::abs (delays[a][i] - delays[b][i]));
                float apart = std::abs (phases[a][i] - phases[b][i]);
                if (apart > 0.5f)
                    apart = 1.0f - apart;
                closestPhase = std::min (closestPhase, apart);
            }
        }
    }
    std::cout << "ensemble entry redraw: closest pair of entries " << closestDelay
              << " samples apart, closest pair of phases " << std::setprecision (5)
              << closestPhase << " cycle apart\n";
    expect (closestDelay > 1,
            "a singer entered within one sample of where she entered on a previous take");
    expect (closestPhase > 0.001f,
            "a singer began her vibrato within 0.001 cycle of a previous take");

    // Placement will be a room rather than a performer, so it is not scaled by
    // Humanize and the audible onsets stay spread at 0 once step 8 lands. The
    // entry-delay field itself must be exactly zero.
    parameters.humanize = 0.0f;
    vocalor::VoiceEngine exact;
    exact.prepare (sampleRate, blockSize);
    exact.reset();
    exact.setParameters (parameters);
    exact.noteOn (60, 0.8f);
    for (const auto delay : vocalor::VoiceEngineTestAccess::entryDelays (exact))
        expect (delay == 0, "Humanize 0 no longer puts the whole section on the beat");
}

/** ... and it lets go the same way.

    releaseMultiplier_ was one engine-wide coefficient applied on the same
    sample to every voice, so a chord that entered loosely stopped as one: every
    voice crossed -40 dB of its own note-off level within 0 ms of every other.
    A cut-off is a cue like an entry, so it gets the same treatment -- a
    habitual per-singer lag plus this attempt's own draw -- and the decay time
    constant becomes the singer's own, because how fast subglottal pressure
    falls once the larynx stops is breath support rather than a section
    property.

    A twelve-voice mix cannot be resolved back into twelve envelopes, so the
    seam is each voice's own envelope field, polled once per control period, and
    -40 dB is measured against that voice's value at the note-off sample.
*/
void testReleaseStagger()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int poll = 16;   // one control period

    const auto spreadAt = [&] (float humanize)
    {
        auto parameters = makeParameters (1, 0, 0, 0);   // Choir
        parameters.choirSize = 12;
        parameters.humanize = humanize;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, static_cast<int> (sampleRate * 1.0));

        const auto atNoteOff = vocalor::VoiceEngineTestAccess::envelopesBySinger (engine);
        engine.noteOff (60);

        // When each voice *starts* letting go, read before the countdowns are
        // spent. The threshold crossing below is not enough on its own: an
        // engine that gave every singer her own release time constant but
        // started all twelve of them on the same sample would spread the -40 dB
        // crossings exactly as required and still cut the section off as one,
        // which is the thing this step exists to remove.
        const auto starts = vocalor::VoiceEngineTestAccess::releaseDelays (engine);
        expect (starts.size() == 12,
                "the twelve-singer choir did not put twelve voices into release");
        if (starts.size() == 12)
        {
            const auto first = *std::min_element (starts.begin(), starts.end());
            const auto last = *std::max_element (starts.begin(), starts.end());
            std::cout << "release starts at Humanize " << std::fixed
                      << std::setprecision (1) << humanize << ": " << first
                      << " to " << last << " samples after the note-off\n";
            if (humanize > 0.0f)
                expect (last > first,
                        "every singer began releasing on the same sample, so the "
                        "stagger is time constants alone and the section still "
                        "lets go together");
            else
                expect (last == 0,
                        "Humanize 0 no longer starts the whole section's release "
                        "on the note-off");
        }

        std::array<double, 12> crossed {};
        crossed.fill (-1.0);
        std::vector<float> left (poll, 0.0f);
        std::vector<float> right (poll, 0.0f);
        for (int step = 1; step <= static_cast<int> (sampleRate * 2.5) / poll; ++step)
        {
            std::fill (left.begin(), left.end(), 0.0f);
            std::fill (right.begin(), right.end(), 0.0f);
            engine.process (left.data(), right.data(), poll);
            const auto now = vocalor::VoiceEngineTestAccess::envelopesBySinger (engine);
            const auto milliseconds = 1000.0 * static_cast<double> (step) * poll / sampleRate;
            for (std::size_t i = 0; i < crossed.size(); ++i)
                if (atNoteOff[i] > 0.0f && crossed[i] < 0.0
                    && (now[i] < 0.0f || now[i] < 0.01f * atNoteOff[i]))
                    crossed[i] = milliseconds;
        }

        double earliest = 1.0e9;
        double latest = -1.0e9;
        int resolved = 0;
        for (std::size_t i = 0; i < crossed.size(); ++i)
        {
            if (atNoteOff[i] <= 0.0f)
                continue;
            expect (crossed[i] >= 0.0,
                    "a voice never fell 40 dB below its note-off level");
            if (crossed[i] < 0.0)
                continue;
            ++resolved;
            earliest = std::min (earliest, crossed[i]);
            latest = std::max (latest, crossed[i]);
        }
        expect (resolved == 12, "the twelve-singer choir did not release twelve voices");
        return resolved == 12 ? latest - earliest : 0.0;
    };

    const auto loose = spreadAt (1.0f);
    const auto exact = spreadAt (0.0f);
    std::cout << "release stagger: " << std::fixed << std::setprecision (1) << loose
              << " ms at Humanize 1.0, " << exact << " ms at Humanize 0\n";

    expect (loose >= 80.0,
            "the section still lets go together: " + std::to_string (loose)
                + " ms between the first and the last voice to fall 40 dB");
    expect (loose <= 400.0,
            "the release is staggered by " + std::to_string (loose)
                + " ms, which is a straggler rather than a decrescendo");
    expect (exact == 0.0,
            "the release stagger is not scaled by Humanize: it is still "
                + std::to_string (exact) + " ms with the take exact");
}

/** The redraw has to come out of the note, not out of the engine's history.

    Redrawing the entry timing at every note is only free because the draw is a
    hash of generation, root and singer index: the render stays a pure function
    of the note sequence, so two hosts that slice the same performance into
    different buffers still get the same section. A stateful random walk would
    have bought the same dispersion and cost that, and the failure would have
    been silent -- the instrument would still sound like a choir, it would just
    sound like a different choir on every render of the same project.

    So the property is asserted where it can be seen: the twelve entry delays,
    the twelve vibrato phases and the samples themselves, across two engines
    given the same notes and across four buffer sizes. A split on a multiple of
    the 64-sample render chunk is bit-exact, because chunk boundaries are
    aligned to absolute sample positions rather than to the buffer. A split that
    lands inside a chunk carries a residual -- 1.3e-6 peak here, against 1.8e-6
    on the engine before this step, so it is older than the redraw and slightly
    smaller after it -- and the draws stay bit-exact through those too, which is
    what decides whether it is the same section singing.
*/
void testTimingRedrawIsDeterministic()
{
    constexpr auto sampleRate = 48000.0;
    struct Take
    {
        std::vector<float> audio;
        std::array<std::vector<int>, 2> delays;
        std::array<std::vector<float>, 2> phases;
    };

    // One performance -- a note, a release, and a second note on the same key
    // so the redraw happens inside the render rather than before it -- rendered
    // in buffers of `block`.
    const auto perform = [&] (int block)
    {
        auto parameters = makeParameters (1, 0, 0, 0);   // Choir
        parameters.choirSize = 12;
        parameters.humanize = 1.0f;
        parameters.instability = 0.82f;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, 512);
        engine.reset();
        engine.setParameters (parameters);

        Take take;
        std::vector<float> left (static_cast<std::size_t> (block), 0.0f);
        std::vector<float> right (static_cast<std::size_t> (block), 0.0f);
        const auto run = [&] (int samples)
        {
            for (int done = 0; done < samples;)
            {
                const int count = std::min (block, samples - done);
                std::fill (left.begin(), left.end(), 0.0f);
                std::fill (right.begin(), right.end(), 0.0f);
                engine.process (left.data(), right.data(), count);
                for (int i = 0; i < count; ++i)
                {
                    take.audio.push_back (left[static_cast<std::size_t> (i)]);
                    take.audio.push_back (right[static_cast<std::size_t> (i)]);
                }
                done += count;
            }
        };

        engine.noteOn (60, 0.8f);
        take.delays[0] = vocalor::VoiceEngineTestAccess::entryDelays (engine);
        take.phases[0] = vocalor::VoiceEngineTestAccess::vibratoPhases (engine);
        run (12000);
        engine.noteOff (60);
        run (6000);
        engine.noteOn (60, 0.8f);
        take.delays[1] = vocalor::VoiceEngineTestAccess::entryDelays (engine);
        take.phases[1] = vocalor::VoiceEngineTestAccess::vibratoPhases (engine);
        run (12000);
        return take;
    };

    const auto reference = perform (24000);
    const auto sameSequence = perform (24000);
    expect (sameSequence.audio == reference.audio,
            "two engines given the same note sequence did not render identically");
    // The two notes must differ from each other -- otherwise the redraw is not
    // happening and the rest of this test is trivially satisfied.
    expect (reference.delays[0] != reference.delays[1],
            "the second note reused the first note's twelve entry delays");

    double worstSubChunk = 0.0;
    for (const int block : { 1, 17, 64, 512 })
    {
        const auto split = perform (block);
        expect (split.audio.size() == reference.audio.size(),
                "a split render produced a different number of samples");
        for (int note = 0; note < 2; ++note)
        {
            expect (split.delays[static_cast<std::size_t> (note)]
                        == reference.delays[static_cast<std::size_t> (note)],
                    "buffers of " + std::to_string (block)
                        + " samples drew different entry delays on note "
                        + std::to_string (note));
            expect (split.phases[static_cast<std::size_t> (note)]
                        == reference.phases[static_cast<std::size_t> (note)],
                    "buffers of " + std::to_string (block)
                        + " samples drew different vibrato phases on note "
                        + std::to_string (note));
        }

        double worst = 0.0;
        for (std::size_t i = 0; i < std::min (split.audio.size(), reference.audio.size()); ++i)
            worst = std::max (worst, std::abs (static_cast<double> (split.audio[i])
                                               - static_cast<double> (reference.audio[i])));
        if (block % vocalor::VoiceEngineTestAccess::chunkSize == 0)
            expect (worst == 0.0,
                    "buffers of " + std::to_string (block)
                        + " samples did not reproduce a single-block render bit for bit: "
                        + std::to_string (worst));
        else
            worstSubChunk = std::max (worstSubChunk, worst);
    }

    std::cout << "timing redraw: buffer-split residual " << std::scientific
              << std::setprecision (2) << worstSubChunk
              << " peak on sub-chunk splits\n" << std::defaultfloat;
    expect (worstSubChunk < 1.0e-4,
            "sub-chunk buffer splits moved the render by " + std::to_string (worstSubChunk)
                + ", far past the residual a chunk boundary explains");
}

/** An a cappella ensemble tunes its chord to the bass, not to a keyboard.

    Chord mode stacked equal-tempered semitones, so a one-finger triad beat
    where a real section locks. The intonation control blends toward five-limit
    just intervals referred to the lowest sounding root, and has to reach them
    to within a cent or it is only a detune.
*/
void testJustIntonation()
{
    constexpr auto sampleRate = 48000.0;

    // The table itself, against the interval ratios it claims to realise.
    const auto centsForRatio = [] (double ratio, int equalSemitones)
    {
        return 1200.0 * std::log2 (ratio) - 100.0 * equalSemitones;
    };
    const std::array<std::pair<int, double>, 6> ratios {{
        { 3, 6.0 / 5.0 }, { 4, 5.0 / 4.0 }, { 5, 4.0 / 3.0 },
        { 7, 3.0 / 2.0 }, { 8, 8.0 / 5.0 }, { 9, 5.0 / 3.0 }
    }};
    for (const auto& entry : ratios)
        expect (std::abs (vocalor::justIntonationOffsetCents (entry.first)
                          - centsForRatio (entry.second, entry.first)) < 0.01,
                "the just offset for " + std::to_string (entry.first)
                    + " semitones does not match its ratio");
    expect (vocalor::justIntonationOffsetCents (0) == 0.0f
                && vocalor::justIntonationOffsetCents (12) == 0.0f
                && vocalor::justIntonationOffsetCents (-12) == 0.0f,
            "the octave is not left alone by the intonation table");

    // The doc comment promises "only its pitch class matters" for any integer,
    // positive or negative, but -12 above is a degenerate probe: -12 % 12 is
    // already 0 in C++, so it never touches the "+ 12" correction the pitch
    // class wrap applies to a genuinely negative, non-octave dividend. The
    // engine itself never supplies one either -- voice.midiNote - intonationRoot_
    // is kept >= 0 by construction -- so that correction is otherwise dead
    // code. Each ratio's semitone count shifted down a full octave lands on
    // the same pitch class and must resolve to the very same cents already
    // verified above.
    for (const auto& entry : ratios)
        expect (std::abs (vocalor::justIntonationOffsetCents (entry.first - 12)
                          - centsForRatio (entry.second, entry.first)) < 0.01,
                "the negative-dividend wrap for " + std::to_string (entry.first - 12)
                    + " semitones does not match its positive pitch class's ratio");

    const auto intervalCents = [] (float lower, float upper)
    {
        return 1200.0 * std::log2 (static_cast<double> (upper)
                                   / std::max (static_cast<double> (lower), 1.0e-9));
    };

    // Chord mode, both qualities, measured from the rendered oscillators.
    for (const int quality : { 0, 1 })
    {
        const int third = quality == 0 ? 4 : 3;
        for (const float amount : { 0.0f, 1.0f })
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = steadyParameters();
            parameters.mode = vocalor::PerformanceMode::Chord;
            parameters.chordQuality = static_cast<vocalor::ChordQuality> (quality);
            parameters.intonation = amount;
            engine.setParameters (parameters);
            engine.noteOn (60, 0.80f);
            render (engine, static_cast<int> (sampleRate * 0.8));

            const auto root = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60);
            const auto thirdHz = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60 + third);
            const auto fifthHz = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 67);
            expect (root > 0.0f && thirdHz > 0.0f && fifthHz > 0.0f,
                    "chord mode did not sound the root, the third and the fifth");

            const auto expectedThird = 100.0 * third
                + amount * vocalor::justIntonationOffsetCents (third);
            const auto expectedFifth = 700.0
                + amount * vocalor::justIntonationOffsetCents (7);
            expect (std::abs (intervalCents (root, thirdHz) - expectedThird) < 1.0,
                    "the chord third did not land on its target interval");
            expect (std::abs (intervalCents (root, fifthHz) - expectedFifth) < 1.0,
                    "the chord fifth did not land on its target interval");
        }
    }

    // Played polyphonically the reference is the bass, so the same triad locks
    // whether it is one key in chord mode or three keys in solo mode.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.intonation = 1.0f;
        engine.setParameters (parameters);
        // Deliberately out of order: the bass arrives last and the section has
        // to re-tune to it rather than to whatever was pressed first.
        engine.noteOn (64, 0.80f);
        engine.noteOn (67, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.8));

        const auto root = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60);
        const auto third = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);
        const auto fifth = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 67);
        expect (std::abs (intervalCents (root, third) - 386.314) < 1.0,
                "a polyphonic major third did not re-tune to the bass that arrived after it");
        expect (std::abs (intervalCents (root, fifth) - 701.955) < 1.0,
                "a polyphonic fifth did not re-tune to the bass that arrived after it");
    }

    // The adjustment is a singer moving onto the interval, not a step.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.intonation = 0.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        engine.noteOn (64, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto equal = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);

        parameters.intonation = 1.0f;
        engine.setParameters (parameters);
        render (engine, 512);
        const auto partway = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);
        render (engine, static_cast<int> (sampleRate * 0.8));
        const auto settled = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);

        const auto total = std::abs (intervalCents (equal, settled));
        expect (total > 12.0, "the intonation control did not move the third at all");
        expect (std::abs (intervalCents (equal, partway)) < 0.5 * total,
                "the intonation adjustment stepped instead of gliding");
        expect (std::abs (intervalCents (equal, partway)) > 0.0,
                "the intonation adjustment never started");
    }
}

/** A singer does not keep a speech tract when the fundamental climbs past its
    lowest resonance; she opens the jaw and takes F1 up with the pitch.

    The 1.1 engine raised F1 by a flat 0.32 % per semitone above A4, so a female
    /u/ at C6 kept F1 at 367 Hz with the fundamental at 1047 Hz — the whole
    spectrum above the lowest resonance, which is the configuration the soprano
    literature describes as a remarkable loss of acoustic energy.
*/
void testFormantTuningAtHighPitch()
{
    constexpr auto sampleRate = 48000.0;

    const auto formantsFor = [] (int midiNote, int vowel)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowel = static_cast<vocalor::Vowel> (vowel);
        engine.setParameters (parameters);
        engine.noteOn (midiNote, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.5));
        return vocalor::VoiceEngineTestAccess::voiceFormants (engine);
    };

    const auto levelFor = [] (int midiNote, int vowel)
    {
        auto parameters = steadyParameters();
        parameters.vowel = static_cast<vocalor::Vowel> (vowel);
        return steadyLevelDb (sampleRate, parameters, midiNote);
    };

    // Middle C on the open anchor is a long way below F1: nothing may move.
    const auto low = formantsFor (60, 0);
    expect (std::abs (low[0] - 850.0f) < 1.0f,
            "the open vowel's F1 moved at a pitch nowhere near it");

    // C6 on the close-back anchor is the case the strategy exists for.
    constexpr float c6 = 1046.502f;
    const auto high = formantsFor (84, 1);
    std::cout << "C6 /u/: F1 " << std::fixed << std::setprecision (1) << high[0]
              << " Hz against f0 " << c6 << " Hz, F2 " << high[1] << " Hz\n";
    expect (high[0] > 0.95f * c6,
            "F1 did not follow the fundamental once the fundamental passed it");
    expect (high[0] < 1.45f * c6,
            "F1 overshot the fundamental instead of tuning to it");
    expect (high[1] > 1.20f * high[0],
            "F2 was not kept clear of the tracked F1");

    // The ceiling is physiological: the jaw runs out before the pitch does.
    const auto extreme = formantsFor (96, 1);
    expect (extreme[0] < 1400.0f,
            "F1 tracked past any jaw opening a singer actually has");

    // What it buys. With a fixed tract the vowel decides how much of the top
    // octave survives; with the strategy in place both vowels put a resonance
    // on the fundamental, so the choice of vowel costs far less.
    const auto openHigh = levelFor (84, 0);
    const auto closeHigh = levelFor (84, 1);
    std::cout << "C6 open vs close vowel level: " << std::setprecision (2)
              << openHigh << " dB vs " << closeHigh << " dB\n";
    // 25.1 dB before. The residual is the cascade amplitude weighting, which is
    // still resolved once per chunk for the untuned tract and so still knows
    // the close vowel by its speech formants; that is a real, stated limit.
    expect (std::abs (openHigh - closeHigh) < 8.0,
            "the vowel choice still decides whether the top octave is audible");

    // ... and the top octave must not simply collapse against the middle.
    const auto openMid = levelFor (60, 0);
    const auto closeMid = levelFor (60, 1);
    std::cout << "C4 open vs close vowel level: " << openMid << " dB vs "
              << closeMid << " dB\n";
    // -20.1 dB before: the top octave of a close vowel was simply gone.
    expect (closeHigh - closeMid > -3.0,
            "a close vowel still loses the top octave against the middle");
    // ... and the resonance it wins must not make the top octave shout either.
    expect (closeHigh - closeMid < 6.0,
            "formant tuning spent its resonance gain on volume instead of effort");
}

/** Continuous performance expression.

    The 1.1 engine froze the entire dynamic response of a note at note-on and
    had no pitch bend, mod wheel, expression pedal or sustain pedal at all. The
    dynamic added here is not a fader: it has to move the source spectrum and
    the breath-to-voice balance by more than it moves the level, or it is only
    an output trim wearing a different name.
*/
void testPerformanceExpression()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 261.6255653;

    // The response curve is exactly inert at its default, which is what lets
    // every other test in this file keep its 1.1 expectations.
    const auto full = vocalor::dynamicResponse (1.0f);
    expect (full.voicedGain == 1.0f && full.airGain == 1.0f && full.effortScale == 1.0f
                && full.sourceTensionScale == 1.0f && full.vibratoScale == 1.0f,
            "the dynamic response is not inert at its full setting");

    const auto empty = vocalor::dynamicResponse (0.0f);
    // 30.00 dB, which is what a singer covers between pianissimo and
    // fortissimo. It was 18.06 dB until the dynamic stopped being a fader.
    expect (std::abs (20.0f * std::log10 (empty.voicedGain) + 30.00f) < 0.01f,
            "an empty dynamic is not 30 dB down on the voiced source");
    expect (empty.airGain > 3.0f * empty.voicedGain,
            "aspiration falls as fast as the voiced source at a low dynamic");

    float previousGain = -1.0f;
    bool monotonic = true;
    for (int step = 0; step <= 10; ++step)
    {
        const auto response = vocalor::dynamicResponse (0.1f * static_cast<float> (step));
        monotonic = monotonic && response.voicedGain > previousGain
                              && response.effortScale > 0.0f;
        previousGain = response.voicedGain;
    }
    expect (monotonic, "the dynamic level is not monotonic in its control");

    // dynamicResponse() clamps its input the same way every other 0..1 knob in
    // this file does, but that clamp itself was only ever exercised at 0 and 1;
    // a value outside the knob's own travel, and a non-finite one, have to
    // resolve to the same endpoints rather than extrapolating the curve or
    // propagating a NaN into the source.
    const auto belowRange = vocalor::dynamicResponse (-5.0f);
    expect (belowRange.voicedGain == empty.voicedGain && belowRange.airGain == empty.airGain
                && belowRange.effortScale == empty.effortScale
                && belowRange.sourceTensionScale == empty.sourceTensionScale
                && belowRange.vibratoScale == empty.vibratoScale,
            "a dynamics value below the knob's own range was not clamped to the empty response");
    const auto aboveRange = vocalor::dynamicResponse (2.0f);
    expect (aboveRange.voicedGain == full.voicedGain && aboveRange.airGain == full.airGain
                && aboveRange.effortScale == full.effortScale
                && aboveRange.sourceTensionScale == full.sourceTensionScale
                && aboveRange.vibratoScale == full.vibratoScale,
            "a dynamics value above the knob's own range was not clamped to the full response");
    const auto nonFinite = vocalor::dynamicResponse (std::numeric_limits<float>::quiet_NaN());
    expect (nonFinite.voicedGain == empty.voicedGain && nonFinite.airGain == empty.airGain
                && nonFinite.effortScale == empty.effortScale
                && nonFinite.sourceTensionScale == empty.sourceTensionScale
                && nonFinite.vibratoScale == empty.vibratoScale,
            "a non-finite dynamics value was not treated as the empty response");

    // A soft note has to be duller, not merely quieter.
    const auto spectrumAt = [] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.breath = 0.30f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        renderMono (engine, static_cast<int> (sampleRate * 0.5));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.5));

        double high = 0.0;
        for (int harmonic = 9; harmonic <= 18; ++harmonic)
        {
            const auto magnitude = harmonicMagnitude (
                samples, fundamental * harmonic, sampleRate);
            high += magnitude * magnitude;
        }
        return std::array<double, 2> {
            harmonicMagnitude (samples, fundamental, sampleRate), std::sqrt (high) };
    };

    const auto loudSpectrum = spectrumAt (1.0f);
    const auto softSpectrum = spectrumAt (0.30f);
    const auto toDb = [] (double loud, double soft)
    {
        return 20.0 * std::log10 (std::max (loud, 1.0e-12) / std::max (soft, 1.0e-12));
    };
    const auto fundamentalDrop = toDb (loudSpectrum[0], softSpectrum[0]);
    const auto bandDrop = toDb (loudSpectrum[1], softSpectrum[1]);
    std::cout << "dynamic 1.0 -> 0.3: fundamental " << std::fixed << std::setprecision (2)
              << fundamentalDrop << " dB, 2.4-4.7 kHz " << bandDrop << " dB\n";
    // Seven tenths of the control, so seven tenths of the 30 dB span plus what
    // the source slope takes out of the fundamental's own neighbourhood.
    expect (fundamentalDrop > 15.0 && fundamentalDrop < 30.0,
            "the dynamic did not move the level by a plausible amount");
    expect (bandDrop - fundamentalDrop > 2.2,
            "the dynamic is a fader: it did not roll the source spectrum off as it fell");

    // ... and proportionally breathier, measured where the render loop reads it.
    const auto breathBalanceAt = [] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.breath = 0.40f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.5));
        const auto drives = vocalor::VoiceEngineTestAccess::dynamicDrives (engine);
        return static_cast<double> (drives[1])
             / std::max (static_cast<double> (drives[0]), 1.0e-9);
    };
    expect (breathBalanceAt (0.30f) > 2.0 * breathBalanceAt (1.0f),
            "a soft note is not proportionally breathier than a loud one");

    // Pitch bend, on a note that is already sounding.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto unbent = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (unbent > 255.0 && unbent < 268.0,
                "the unbent reference note was not near middle C");

        engine.setPitchBend (2.0f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto up = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (std::abs (up / unbent - std::exp2 (2.0 / 12.0)) < 0.002,
                "a two-semitone bend did not produce the two-semitone frequency ratio");

        engine.setPitchBend (-12.0f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto down = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (std::abs (down / unbent - 0.5) < 0.002,
                "an octave bend down did not halve the frequency");

        // A non-finite bend must not reach the oscillator.
        engine.setPitchBend (std::numeric_limits<float>::quiet_NaN());
        const auto recovered = render (engine, static_cast<int> (sampleRate * 0.2));
        expect (recovered.finite, "a non-finite pitch bend produced non-finite audio");
    }

    // Sustain pedal: a note-off arriving under a held pedal is deferred, not
    // dropped, and pedal-up delivers it through the ordinary release path.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.80f);
        render (engine, blockSize);
        engine.setSustainPedal (true);
        engine.noteOff (60);
        render (engine, static_cast<int> (sampleRate * 0.3));
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (engine) == 60,
                "the sustain pedal did not hold a note through its note-off");

        engine.setSustainPedal (false);
        render (engine, static_cast<int> (sampleRate * 0.05));
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (engine) == -1,
                "releasing the sustain pedal did not release the note it was holding");
        const auto tail = render (engine, static_cast<int> (sampleRate * 3.0));
        expect (tail.finite && engine.getActiveVoiceCount() == 0,
                "the note the pedal released never finished its release");
    }

    // Expression is a level trim and nothing else: half expression has to be
    // the same render at half the amplitude, sample for sample.
    {
        const auto renderAt = [] (float expression)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            engine.setParameters (steadyParameters());
            engine.setExpression (expression);
            engine.noteOn (60, 0.80f);
            renderMono (engine, static_cast<int> (sampleRate * 0.4));
            return renderMono (engine, static_cast<int> (sampleRate * 0.4));
        };

        const auto loud = renderAt (1.0f);
        const auto soft = renderAt (0.5f);
        double peak = 0.0;
        double worst = 0.0;
        for (std::size_t i = 0; i < loud.size(); ++i)
        {
            peak = std::max (peak, std::abs (static_cast<double> (loud[i])));
            worst = std::max (worst, std::abs (static_cast<double> (soft[i])
                                               - 0.5 * static_cast<double> (loud[i])));
        }
        expect (peak > 1.0e-4, "the expression reference render was silent");
        expect (worst < 1.0e-4 * peak,
                "expression changed the sound rather than only its level");
    }

    // The mod wheel takes the dynamic over for good the first time it moves.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.dynamics = 1.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto ownedBy = [&engine]
        {
            return vocalor::VoiceEngineTestAccess::dynamicDrives (engine)[2];
        };
        expect (std::abs (ownedBy() - 1.0f) < 1.0e-3f,
                "the host dynamic parameter did not own the level before the wheel moved");

        engine.setModWheel (0.25f);
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.25f) < 0.01f,
                "the mod wheel did not take over the dynamic level");

        parameters.dynamics = 0.90f;
        engine.setParameters (parameters);
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.25f) < 0.01f,
                "the host parameter overrode the mod wheel after the wheel had moved");

        engine.resetControllers();
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.90f) < 0.01f,
                "resetting the controllers did not hand the dynamic back to the host");
    }
}
} // namespace

int main()
{
    testPreparationIsExplicit();
    testRenderMatrix();
    testReleaseCompletes();
    testAllSoundOffIsImmediate();
    testIdleStateAdvancementAndAutomation();
    testVowelSpaceModel();
    testDisplayMathHelpers();
    testVowelMorphAndFormantShift();
    testGlideAndLegato();
    testRoomSizeGeometry();
    testSingerPlacement();
    testPlacementDoesNotRepeatASilencedSinger();
    testRoomEarlyReflections();
    testSampleRateInvariance();
    testHumanisationDepthIsRateInvariant();
    testTractLevelStability();
    testParallelFormantBank();
    testEnsembleSizeIsExact();
    testTractCoefficientSmoothing();
    testGlottalSourceTensionBank();
    testRadiatedPowerRegulation();
    testSourceLevelCalibration();
    testPerformanceExpression();
    testFormantTuningAtHighPitch();
    testJustIntonation();
    testEnsembleDispersion();
    testStraightTonePitchDrift();
    testDriftClockIgnoresLegatoEvents();
    testLiveDriftAutomation();
    testVowelDriftTrajectory();
    testEnsembleTimingIsRedrawn();
    testReleaseStagger();
    testTimingRedrawIsDeterministic();
    testVibratoRateAndExtent();
    testVibratoInstability();
    testVibratoAmplitude();
    testNaturalVibratoAmplitudeAndContinuity();
    testNasalBranch();
    testOnsetSpectrum();
    testVelocityShapesOnset();
    testTheSourceShelfAppliesItsGainOnce();
    testDynamicRange();
    testReleaseAerodynamics();
    testAspirationIsPitchSynchronous();
    testCoarticulationTiming();
    testSingersFormantCluster();
    testSopranoClusterReleaseIsPerVoice();
    testSopranoUpperResonanceRise();
    testFactoryPresets();
    testDenormalAndNaNSafety();
    testParameterSmoothingHasNoZipper();
    testDisplayStateTracksTheEngine();
    testRoughPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Vocalor DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Vocalor DSP checks passed.\n";
    return EXIT_SUCCESS;
}

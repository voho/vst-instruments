#include "SeptumEngine.h"

namespace septum
{
namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double twoPi = 2.0 * pi;

    // The ten-voice sum needs fixed headroom before the output stage; the
    // demos and tests treat full scale as the analog stage's clip point.
    constexpr double voiceHeadroom = 0.22;

    [[nodiscard]] inline double frac (double x) noexcept
    {
        return x - std::floor (x);
    }

    // Two-sample polyBLEP residual for a unit downward step at phase t with
    // per-sample increment dt.
    [[nodiscard]] inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            const double x = t / dt;
            return x + x - x * x - 1.0;
        }
        if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            return x * x + x + x + 1.0;
        }
        return 0.0;
    }

    // PolyBLAMP residual for a slope discontinuity (triangle corners).
    [[nodiscard]] inline double polyBlamp (double t, double dt) noexcept
    {
        if (t < dt)
        {
            const double x = t / dt - 1.0;
            return -x * x * x / 3.0;
        }
        if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt + 1.0;
            return x * x * x / 3.0;
        }
        return 0.0;
    }

    [[nodiscard]] inline double softClip (double x) noexcept
    {
        // Bounded cubic shaper: unity slope at zero, saturating to +/-2.
        const double c = std::clamp (x, -3.0, 3.0);
        return c * (1.0 - c * c * (1.0 / 27.0));
    }

    [[nodiscard]] inline double flushDenormal (double x) noexcept
    {
        return std::abs (x) < 1.0e-15 ? 0.0 : x;
    }

    // The final safety stage: transparent below 0.9, saturating above, so a
    // reasonably-driven patch never touches it and an unreasonable one cannot
    // hand the host a sample outside +/-1.05.
    [[nodiscard]] inline double outputLimit (double x) noexcept
    {
        const double a = std::abs (x);
        if (a <= 0.9)
            return x;
        const double over = a - 0.9;
        const double limited = 0.9 + 0.15 * (1.0 - std::exp (-over * (1.0 / 0.15)));
        return x < 0.0 ? -limited : limited;
    }

    [[nodiscard]] inline double onePoleCoeff (double sr, double seconds) noexcept
    {
        return seconds <= 0.0 ? 1.0 : 1.0 - std::exp (-1.0 / (sr * seconds));
    }

    // Signed depth -63..+63 as a plain scale.
    [[nodiscard]] inline double depthScale (int depth) noexcept
    {
        return depth / 63.0;
    }
} // namespace

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------

void Engine::Envelope::configure (double sr, int a, int d, int s, int r) noexcept
{
    const double attackTime = mapping::attackSeconds (a);
    attackRate = 1.0 / std::max (1.0, attackTime * sr);
    // Exponential fall covering 60 dB over the mapped time.
    decayCoeff = std::exp (-6.907755 / std::max (1.0, mapping::decaySeconds (d) * sr));
    releaseCoeff = std::exp (-6.907755 / std::max (1.0, mapping::decaySeconds (r) * sr));
    sustain = s / 127.0;
}

double Engine::Envelope::advance (int samples) noexcept
{
    for (int i = 0; i < samples; ++i)
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0;
            case Stage::Attack:
                level += attackRate;
                if (level >= 1.0)
                {
                    level = 1.0;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                level = sustain + (level - sustain) * decayCoeff;
                if (level - sustain < 1.0e-4)
                    stage = Stage::Sustain;
                break;
            case Stage::Sustain:
                level = sustain;
                return level;
            case Stage::Release:
                level *= releaseCoeff;
                if (level < 1.0e-5)
                {
                    level = 0.0;
                    stage = Stage::Idle;
                    return 0.0;
                }
                break;
        }
    }
    return level;
}

void Engine::PitchEnvelope::configure (double sr, int a, int d) noexcept
{
    attackRate = 1.0 / std::max (1.0, mapping::attackSeconds (a) * sr);
    decayCoeff = std::exp (-6.907755 / std::max (1.0, mapping::decaySeconds (d) * sr));
}

double Engine::PitchEnvelope::advance (int samples) noexcept
{
    if (! active)
        return 0.0;
    for (int i = 0; i < samples; ++i)
    {
        if (attacking)
        {
            level += attackRate;
            if (level >= 1.0)
            {
                level = 1.0;
                attacking = false;
            }
        }
        else
        {
            level *= decayCoeff;
            if (level < 1.0e-4)
            {
                level = 0.0;
                active = false;
                break;
            }
        }
    }
    return level;
}

// ---------------------------------------------------------------------------
// LFO
// ---------------------------------------------------------------------------

void Engine::Lfo::restart (bool resetFade) noexcept
{
    phase = 0.0;
    if (resetFade)
        fadeLevel = 0.0;
}

double Engine::Lfo::nextRandomValue() noexcept
{
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) * (1.0 / 8388607.5) - 1.0;
}

double Engine::Lfo::advance (const LfoParams& params, double hz,
                             double fadePerTick, int samples, double sr) noexcept
{
    if (! primed)
    {
        primed = true;
        heldValue = nextRandomValue();
        randomFrom = heldValue;
        randomTo = nextRandomValue();
    }

    const double dt = hz * samples / sr;
    phase += dt;
    bool wrapped = false;
    while (phase >= 1.0)
    {
        phase -= 1.0;
        wrapped = true;
    }

    if (wrapped)
    {
        heldValue = nextRandomValue();
        randomFrom = randomTo;
        randomTo = heldValue;
    }

    if (fadePerTick > 0.0)
        fadeLevel = std::min (1.0, fadeLevel + fadePerTick);
    else
        fadeLevel = 1.0;

    double value = 0.0;
    switch (params.shape)
    {
        case LfoShape::Tri:
            value = phase < 0.25 ? 4.0 * phase
                    : phase < 0.75 ? 2.0 - 4.0 * phase
                                   : 4.0 * phase - 4.0;
            break;
        case LfoShape::Sin:
            value = std::sin (twoPi * phase);
            break;
        case LfoShape::Saw:
            // Descending ramp, the direction Roland's LFO saw icons draw.
            value = 1.0 - 2.0 * phase;
            break;
        case LfoShape::Sqr:
            value = phase < 0.5 ? 1.0 : -1.0;
            break;
        case LfoShape::Trapezoid:
            // rise 1/4, hold 1/4, fall 1/4, hold 1/4 (voiced segment ratios).
            value = phase < 0.25 ? -1.0 + 8.0 * phase
                    : phase < 0.5 ? 1.0
                    : phase < 0.75 ? 1.0 - 8.0 * (phase - 0.5)
                                   : -1.0;
            break;
        case LfoShape::SampleHold:
            value = heldValue;
            break;
        case LfoShape::Random:
        {
            value = randomFrom + (randomTo - randomFrom) * phase;
            break;
        }
    }
    return value * fadeLevel;
}

// ---------------------------------------------------------------------------
// Reverb container
// ---------------------------------------------------------------------------

void Engine::Reverb::clear()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);
    for (auto& diffuser : diffusers)
        std::fill (diffuser.begin(), diffuser.end(), 0.0f);
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    writes.fill (0);
    diffuserWrites.fill (0);
    lowStates.fill (0.0);
    highStates.fill (0.0);
    preDelayWrite = 0;
    highCutStateL = highCutStateR = 0.0;
    fresh = 1 << 30;
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

Engine::Engine()
{
    clampToDocumentedRanges (patch_);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ = std::max (8000.0, sampleRate);
    maxBlock_ = std::max (16, maxBlockSize);

    const auto combSamples = static_cast<std::size_t> (sampleRate_ * 0.07) + 8;
    for (auto& voice : voices_)
    {
        voice.osc1.comb.assign (combSamples, 0.0f);
        voice.osc2.comb.assign (combSamples, 0.0f);
    }

    const auto delaySamples = static_cast<std::size_t> (sampleRate_ * 1.45) + 8;
    delayL_.buffer.assign (delaySamples, 0.0f);
    delayR_.buffer.assign (delaySamples, 0.0f);

    // Reverb geometry: mutually prime base lengths (in seconds at size 8)
    // spread over roughly 30-90 ms, scaled down for smaller sizes.
    static constexpr std::array<double, Reverb::lineCount> baseSeconds {
        0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0631, 0.0733, 0.0797
    };
    const double sizeScale = 0.35 + 0.65 * ((patch_.reverb.size + 1) / 8.0);
    for (int i = 0; i < Reverb::lineCount; ++i)
    {
        const auto length = static_cast<int> (baseSeconds[static_cast<std::size_t> (i)]
                                              * sizeScale * sampleRate_) | 1;
        reverb_.lengths[static_cast<std::size_t> (i)] = std::max (32, length);
        reverb_.lines[static_cast<std::size_t> (i)]
            .assign (static_cast<std::size_t> (sampleRate_ * 0.1) + 16, 0.0f);
    }
    static constexpr std::array<double, 4> diffuserSeconds {
        0.0043, 0.0083, 0.0151, 0.0223
    };
    for (int i = 0; i < 4; ++i)
        reverb_.diffusers[static_cast<std::size_t> (i)]
            .assign (static_cast<std::size_t> (diffuserSeconds[static_cast<std::size_t> (i)]
                                               * sampleRate_) + 8, 0.0f);
    reverb_.preDelay.assign (static_cast<std::size_t> (sampleRate_ * 0.105) + 8, 0.0f);

    scratchMono_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    dryL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    dryR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendDelayL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendDelayR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendReverbL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendReverbR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);

    // Analog output stage, from the service notes' component values:
    // 22 uF into 22 k -> 0.329 Hz coupling; RC poles 8.2k/820p -> 23.7 kHz
    // and 4.7k/270p -> 125.4 kHz (clamped below Nyquist where necessary).
    dcCoeff_ = std::exp (-twoPi * 0.329 / sampleRate_);
    const double pole1 = std::min (23700.0, 0.49 * sampleRate_);
    const double pole2 = std::min (125400.0, 0.49 * sampleRate_);
    rcCoeff1_ = 1.0 - std::exp (-twoPi * pole1 / sampleRate_);
    rcCoeff2_ = 1.0 - std::exp (-twoPi * pole2 / sampleRate_);

    reset();
}

void Engine::reset()
{
    for (auto& voice : voices_)
    {
        voice.active = false;
        voice.note = -1;
        voice.held = false;
        voice.ampEnv.kill();
        voice.filterEnv.kill();
        voice.pitchEnv.active = false;
        voice.filter1.clear();
        voice.filter2.clear();
        voice.shelfState = 0.0;
        voice.osc1.phase = 0.0;
        voice.osc2.phase = 0.0;
        voice.osc1.superPhases.fill (0.0);
        voice.osc2.superPhases.fill (0.0);
        voice.osc1.clearRuntime();
        voice.osc2.clearRuntime();
    }
    for (auto& tone : tones_)
    {
        tone = ToneRuntime {};
    }
    std::fill (delayL_.buffer.begin(), delayL_.buffer.end(), 0.0f);
    std::fill (delayR_.buffer.begin(), delayR_.buffer.end(), 0.0f);
    delayL_.write = delayR_.write = 0;
    delayL_.dampState = delayR_.dampState = 0.0;
    delayL_.fresh = delayR_.fresh = 1 << 30;
    delayModPhase_ = 0.0;
    reverb_.clear();
    delayTimeSmoothed_ = mapping::delaySeconds (patch_.delay.time) * sampleRate_;
    for (int channel = 0; channel < 2; ++channel)
    {
        dcX1_[channel] = dcY1_[channel] = 0.0;
        rcState1_[channel] = rcState2_[channel] = 0.0;
    }
    pitchBend_ = 0.0;
    modulation_ = 0.0;
    hold_ = false;
    smoothedMaster_ = masterLevel_ / 127.0;
    updateEffectCoefficients();
}

void Engine::setPatch (const Patch& patch)
{
    const int previousSize = patch_.reverb.size;
    patch_ = patch;
    clampToDocumentedRanges (patch_);
    if (patch_.reverb.size != previousSize)
    {
        // Line lengths follow SIZE; recompute them (states are kept — a size
        // change on hardware audibly disturbs the tail too).
        static constexpr std::array<double, Reverb::lineCount> baseSeconds {
            0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0631, 0.0733, 0.0797
        };
        const double sizeScale = 0.35 + 0.65 * ((patch_.reverb.size + 1) / 8.0);
        for (int i = 0; i < Reverb::lineCount; ++i)
        {
            const auto length = static_cast<int> (
                baseSeconds[static_cast<std::size_t> (i)] * sizeScale * sampleRate_) | 1;
            reverb_.lengths[static_cast<std::size_t> (i)] = std::max (
                32, std::min (length,
                              static_cast<int> (reverb_.lines[static_cast<std::size_t> (i)].size()) - 2));
        }
    }
    updateEffectCoefficients();

    for (auto& voice : voices_)
    {
        if (! voice.active)
            continue;
        const TonePatch& tone = tonePatch (voice.part);
        voice.ampEnv.configure (sampleRate_, tone.ampEnvAttack, tone.ampEnvDecay,
                                tone.ampEnvSustain, tone.ampEnvRelease);
        voice.filterEnv.configure (sampleRate_, tone.filterEnvAttack,
                                   tone.filterEnvDecay, tone.filterEnvSustain,
                                   tone.filterEnvRelease);
        voice.pitchEnv.configure (sampleRate_, tone.pitchEnvAttack,
                                  tone.pitchEnvDecay);
    }
}

void Engine::setMasterLevel (int level) noexcept
{
    masterLevel_ = clampRaw (level, 0, 127);
}

void Engine::setMasterTuneHz (double a4Hz) noexcept
{
    masterTuneHz_ = std::clamp (a4Hz, 415.30, 466.20);
}

void Engine::setMasterKeyShift (int semitones) noexcept
{
    masterKeyShift_ = clampRaw (semitones, -24, 24);
}

void Engine::setKeyboardOctaveShift (int octaves) noexcept
{
    octaveShift_ = clampRaw (octaves, -3, 3);
}

void Engine::setTranspose (int semitones) noexcept
{
    transpose_ = clampRaw (semitones, -5, 6);
}

// ---------------------------------------------------------------------------
// Note handling
// ---------------------------------------------------------------------------

bool Engine::partSounds (Part part) const noexcept
{
    switch (patch_.keyboardMode)
    {
        case KeyboardMode::Single:
            return (part == Part::Upper) == (patch_.keyboardPart == KeyboardPart::Upper);
        case KeyboardMode::Dual:
        case KeyboardMode::Split:
            return true;
    }
    return true;
}

int Engine::partVoiceLimit() const noexcept
{
    return patch_.keyboardMode == KeyboardMode::Single ? maxPolyphony
                                                       : dualPolyphony;
}

void Engine::noteOn (int note, int velocity)
{
    note = clampRaw (note, 0, 127);
    velocity = clampRaw (velocity, 1, 127);

    switch (patch_.keyboardMode)
    {
        case KeyboardMode::Single:
            startNoteForPart (patch_.keyboardPart == KeyboardPart::Upper
                                  ? Part::Upper : Part::Lower,
                              note, velocity);
            break;
        case KeyboardMode::Dual:
            startNoteForPart (Part::Upper, note, velocity);
            startNoteForPart (Part::Lower, note, velocity);
            break;
        case KeyboardMode::Split:
            // Settled: keys at or right of the split point sound UPPER.
            startNoteForPart (note >= patch_.splitPoint ? Part::Upper : Part::Lower,
                              note, velocity);
            break;
    }
}

void Engine::noteOff (int note)
{
    note = clampRaw (note, 0, 127);
    releaseNoteForPart (Part::Upper, note);
    releaseNoteForPart (Part::Lower, note);
}

void Engine::startNoteForPart (Part part, int note, int velocity)
{
    if (! partSounds (part))
        return;

    const TonePatch& tone = tonePatch (part);
    ToneRuntime& runtime = toneRuntime (part);

    // Track held keys for solo modes and legato/portamento decisions.
    if (runtime.heldCount < static_cast<int> (runtime.heldNotes.size()))
    {
        runtime.heldNotes[static_cast<std::size_t> (runtime.heldCount)] = note;
        runtime.heldVelocities[static_cast<std::size_t> (runtime.heldCount)] = velocity;
        ++runtime.heldCount;
    }
    const bool firstKey = ! runtime.anyKeyDown;
    runtime.anyKeyDown = true;

    if (tone.lfo1.keyTrigger)
        runtime.lfo1.restart (true);
    else if (firstKey)
        runtime.lfo1.fadeLevel = tone.lfo1.fadeTime > 0 ? 0.0 : 1.0;
    if (tone.lfo2.keyTrigger)
        runtime.lfo2.restart (true);
    else if (firstKey)
        runtime.lfo2.fadeLevel = tone.lfo2.fadeTime > 0 ? 0.0 : 1.0;

    const double velocityNorm = velocity / 127.0;

    if (tone.mono != MonoMode::Poly)
    {
        // Solo: one voice, last-note priority. Legato keeps the envelopes
        // running when a key was already down.
        Voice* voice = nullptr;
        for (auto& candidate : voices_)
            if (candidate.active && candidate.part == part)
            {
                voice = &candidate;
                break;
            }
        const bool legato = voice != nullptr && ! firstKey
                            && tone.mono == MonoMode::SoloLegato;
        if (voice == nullptr)
            voice = allocateVoice (part);
        if (voice == nullptr)
            return;
        triggerVoice (*voice, part, note, velocityNorm, legato);
        return;
    }

    Voice* voice = allocateVoice (part);
    if (voice == nullptr)
        return;
    triggerVoice (*voice, part, note, velocityNorm, false);
}

void Engine::releaseNoteForPart (Part part, int note)
{
    ToneRuntime& runtime = toneRuntime (part);

    for (int i = 0; i < runtime.heldCount; ++i)
    {
        if (runtime.heldNotes[static_cast<std::size_t> (i)] == note)
        {
            for (int j = i; j + 1 < runtime.heldCount; ++j)
            {
                runtime.heldNotes[static_cast<std::size_t> (j)] =
                    runtime.heldNotes[static_cast<std::size_t> (j + 1)];
                runtime.heldVelocities[static_cast<std::size_t> (j)] =
                    runtime.heldVelocities[static_cast<std::size_t> (j + 1)];
            }
            --runtime.heldCount;
            break;
        }
    }
    runtime.anyKeyDown = runtime.heldCount > 0;

    const TonePatch& tone = tonePatch (part);
    if (tone.mono != MonoMode::Poly)
    {
        for (auto& voice : voices_)
        {
            if (! voice.active || voice.part != part)
                continue;
            if (voice.note != note)
                continue;
            if (runtime.heldCount > 0)
            {
                // Return to the most recent still-held key without retrigger.
                const int previousNote =
                    runtime.heldNotes[static_cast<std::size_t> (runtime.heldCount - 1)];
                const int previousVelocity =
                    runtime.heldVelocities[static_cast<std::size_t> (runtime.heldCount - 1)];
                triggerVoice (voice, part, previousNote, previousVelocity / 127.0,
                              tone.mono == MonoMode::SoloLegato);
            }
            else if (hold_)
            {
                voice.held = true;
            }
            else
            {
                voice.held = false;
                voice.ampEnv.release();
                voice.filterEnv.release();
            }
        }
        return;
    }

    for (auto& voice : voices_)
    {
        if (! voice.active || voice.part != part || voice.note != note)
            continue;
        if (hold_)
        {
            voice.held = true;
            continue;
        }
        voice.held = false;
        voice.ampEnv.release();
        voice.filterEnv.release();
    }
}

Engine::Voice* Engine::allocateVoice (Part part)
{
    const int limit = partVoiceLimit();
    int used = 0;
    for (const auto& voice : voices_)
        if (voice.active && voice.part == part)
            ++used;

    // A free physical voice, if the part has room.
    if (used < limit)
    {
        for (auto& voice : voices_)
            if (! voice.active)
                return &voice;
        // No idle voice although the part has room: the other part occupies
        // the pool. Take the longest-released voice anywhere, else the oldest
        // — a part under its documented limit never drops a note.
        Voice* released = nullptr;
        Voice* oldest = nullptr;
        for (auto& voice : voices_)
        {
            if (voice.ampEnv.stage == Envelope::Stage::Release
                && (released == nullptr || voice.age < released->age))
                released = &voice;
            if (oldest == nullptr || voice.age < oldest->age)
                oldest = &voice;
        }
        if (released != nullptr)
            return released;
        if (oldest != nullptr)
            return oldest;
    }

    // Steal within the part: the longest-released voice, else the oldest.
    Voice* best = nullptr;
    for (auto& voice : voices_)
    {
        if (! voice.active || voice.part != part)
            continue;
        if (best == nullptr)
        {
            best = &voice;
            continue;
        }
        const bool voiceReleased =
            voice.ampEnv.stage == Envelope::Stage::Release;
        const bool bestReleased = best->ampEnv.stage == Envelope::Stage::Release;
        if (voiceReleased != bestReleased)
        {
            if (voiceReleased)
                best = &voice;
            continue;
        }
        if (voice.age < best->age)
            best = &voice;
    }
    return best;
}

void Engine::triggerVoice (Voice& voice, Part part, int note, double velocity,
                           bool legato)
{
    const TonePatch& tone = tonePatch (part);
    ToneRuntime& runtime = toneRuntime (part);

    const bool wasActive = voice.active;
    voice.active = true;
    voice.part = part;
    voice.note = note;
    voice.velocity = velocity;
    voice.held = true;
    voice.age = ++voiceClock_;

    // Portamento: glide from the part's previous pitch. With legato mode the
    // glide only applies to overlapped playing (settled behavior).
    const double target = static_cast<double> (note);
    const bool glideAllowed =
        tone.portamento
        && (tone.mono != MonoMode::SoloLegato || legato || runtime.heldCount > 1);
    if (glideAllowed && tone.portamentoTime > 0)
        voice.glidePitch = wasActive && legato ? voice.glidePitch : runtime.lastPitch;
    else
        voice.glidePitch = target;
    voice.targetPitch = target;
    runtime.lastPitch = target;

    voice.ampEnv.configure (sampleRate_, tone.ampEnvAttack, tone.ampEnvDecay,
                            tone.ampEnvSustain, tone.ampEnvRelease);
    voice.filterEnv.configure (sampleRate_, tone.filterEnvAttack,
                               tone.filterEnvDecay, tone.filterEnvSustain,
                               tone.filterEnvRelease);
    voice.pitchEnv.configure (sampleRate_, tone.pitchEnvAttack, tone.pitchEnvDecay);

    if (! legato)
    {
        voice.ampEnv.trigger();
        voice.filterEnv.trigger();
        voice.pitchEnv.trigger();

        // Supersaw phases randomize on every trigger (reported, Szabo); the
        // classic oscillators free-run like their DSP ancestors.
        for (auto* osc : { &voice.osc1, &voice.osc2 })
        {
            for (auto& phase : osc->superPhases)
                phase = nextRandom() * (1.0 / 4294967296.0);
            osc->clearRuntime();
        }
        voice.noiseRng = nextRandom() | 1u;
        voice.controlsPrimed = false;  // snap cutoff/resonance to this note
        if (! wasActive)
        {
            voice.filter1.clear();
            voice.filter2.clear();
            voice.shelfState = 0.0;
        }
    }
}

void Engine::setHold (bool down)
{
    if (down == hold_)
        return;
    hold_ = down;
    if (down)
        return;
    for (auto& voice : voices_)
    {
        if (voice.active && voice.held)
        {
            bool keyStillDown = false;
            ToneRuntime& runtime = toneRuntime (voice.part);
            for (int i = 0; i < runtime.heldCount; ++i)
                if (runtime.heldNotes[static_cast<std::size_t> (i)] == voice.note)
                    keyStillDown = true;
            if (! keyStillDown)
            {
                voice.held = false;
                voice.ampEnv.release();
                voice.filterEnv.release();
            }
        }
    }
}

void Engine::setPitchBend (double normalised)
{
    pitchBend_ = std::clamp (normalised, -1.0, 1.0);
}

void Engine::setModulation (double amount)
{
    modulation_ = std::clamp (amount, 0.0, 1.0);
}

void Engine::setExpression (double amount)
{
    expression_ = std::clamp (amount, 0.0, 1.0);
}

void Engine::setPartLevel (double amount)
{
    partLevel_ = std::clamp (amount, 0.0, 1.0);
}

void Engine::setPartPan (double pan)
{
    partPan_ = std::clamp (pan, -1.0, 1.0);
}

void Engine::setPortamentoControl (int note)
{
    // MIDI Portamento Control: the next note-on glides from this pitch.
    const double pitch = clampRaw (note, 0, 127);
    for (auto& tone : tones_)
        tone.lastPitch = pitch;
}

void Engine::allNotesOff()
{
    for (auto& voice : voices_)
    {
        if (voice.active)
        {
            voice.held = false;
            voice.ampEnv.release();
            voice.filterEnv.release();
        }
    }
    for (auto& tone : tones_)
    {
        tone.heldCount = 0;
        tone.anyKeyDown = false;
    }
}

void Engine::allSoundOff()
{
    for (auto& voice : voices_)
    {
        voice.active = false;
        voice.ampEnv.kill();
        voice.filterEnv.kill();
        voice.pitchEnv.active = false;
    }
    for (auto& tone : tones_)
    {
        tone.heldCount = 0;
        tone.anyKeyDown = false;
    }

    // All Sounds Off is a panic: the buffered delay repeats and the reverb
    // tail must stop with the voices, and the output stage must not keep
    // discharging what it was carrying. This runs inside the audio callback,
    // so the megabytes of effect history are not cleared here — marking them
    // stale mutes every read of pre-panic material until it has been
    // overwritten, at O(1) cost per sample.
    delayL_.fresh = delayR_.fresh = 0;
    delayL_.dampState = delayR_.dampState = 0.0;
    reverb_.fresh = 0;
    reverb_.lowStates.fill (0.0);
    reverb_.highStates.fill (0.0);
    reverb_.highCutStateL = reverb_.highCutStateR = 0.0;
    for (int channel = 0; channel < 2; ++channel)
    {
        dcX1_[channel] = dcY1_[channel] = 0.0;
        rcState1_[channel] = rcState2_[channel] = 0.0;
    }
}

int Engine::activeVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        if (voice.active)
            ++count;
    return count;
}

double Engine::noteToHz (double note) const noexcept
{
    return masterTuneHz_ * std::exp2 ((note - 69.0) / 12.0);
}

std::uint32_t Engine::nextRandom() noexcept
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

// ---------------------------------------------------------------------------
// Control-tick updates
// ---------------------------------------------------------------------------

void Engine::advanceToneLfos (int samples)
{
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        const TonePatch& tone = tonePatch (part);
        ToneRuntime& runtime = tones_[static_cast<std::size_t> (index)];

        const auto rateOf = [this] (const LfoParams& params)
        {
            return params.tempoSync
                       ? mapping::lfoSyncHz (patch_.tempo, params.tempoSyncNote)
                       : mapping::lfoRateHz (params.rate);
        };
        const auto fadeOf = [this, samples] (const LfoParams& params)
        {
            const double seconds = mapping::lfoFadeSeconds (params.fadeTime);
            return seconds <= 0.0 ? 0.0 : samples / (seconds * sampleRate_);
        };

        runtime.lfo1Value = runtime.lfo1.advance (tone.lfo1, rateOf (tone.lfo1),
                                                  fadeOf (tone.lfo1), samples,
                                                  sampleRate_);
        runtime.lfo2Value = runtime.lfo2.advance (tone.lfo2, rateOf (tone.lfo2),
                                                  fadeOf (tone.lfo2), samples,
                                                  sampleRate_);
    }
}

void Engine::updateVoiceControls (Voice& voice, int tickSamples)
{
    const TonePatch& tone = tonePatch (voice.part);
    ToneRuntime& runtime = toneRuntime (voice.part);

    // -- pitch -------------------------------------------------------------
    if (voice.glidePitch != voice.targetPitch)
    {
        const double glideSeconds = mapping::portamentoSeconds (tone.portamentoTime);
        const double coeff = glideSeconds <= 0.0
                                 ? 1.0
                                 : 1.0 - std::exp (-tickSamples
                                                   / (sampleRate_ * glideSeconds / 4.6));
        voice.glidePitch += (voice.targetPitch - voice.glidePitch) * coeff;
        if (std::abs (voice.glidePitch - voice.targetPitch) < 1.0e-3)
            voice.glidePitch = voice.targetPitch;
    }

    const double pitchEnvLevel = voice.pitchEnv.advance (tickSamples);
    const double bendSemitones = pitchBend_ * tone.bendRange;

    // Modulation-lever vibrato rides LFO2 (settled) into the assigned target.
    const double leverVibratoCents = modulation_ * 60.0 * runtime.lfo2Value;
    const bool leverToOsc1 =
        patch_.modulationAssign == ModulationAssign::Osc1AndOsc2
        || patch_.modulationAssign == ModulationAssign::Osc1;
    const bool leverToOsc2 =
        patch_.modulationAssign == ModulationAssign::Osc1AndOsc2
        || patch_.modulationAssign == ModulationAssign::Osc2;

    // LFO pitch contributions (destination 1 -> OSC1, destination 2 -> OSC2).
    double lfoCents1 = 0.0, lfoCents2 = 0.0;
    if (tone.lfo1.destination1 == LfoDest1::Pitch1)
        lfoCents1 += mapping::lfoPitchCents (tone.lfo1.depth1) * runtime.lfo1Value;
    if (tone.lfo2.destination1 == LfoDest1::Pitch1)
        lfoCents1 += mapping::lfoPitchCents (tone.lfo2.depth1) * runtime.lfo2Value;
    if (tone.lfo1.destination2 == LfoDest2::Pitch2)
        lfoCents2 += mapping::lfoPitchCents (tone.lfo1.depth2) * runtime.lfo1Value;
    if (tone.lfo2.destination2 == LfoDest2::Pitch2)
        lfoCents2 += mapping::lfoPitchCents (tone.lfo2.depth2) * runtime.lfo2Value;
    if (leverToOsc1)
        lfoCents1 += leverVibratoCents;
    if (leverToOsc2)
        lfoCents2 += leverVibratoCents;

    const double baseNote = voice.glidePitch + masterKeyShift_ + transpose_
                            + 12.0 * (octaveShift_ + tone.octaveShift)
                            + bendSemitones;

    const double note1 = baseNote + tone.osc1.coarse + tone.osc1.fine / 100.0
                         + pitchEnvLevel * mapping::pitchEnvSemitones (tone.osc1.pitchEnvDepth)
                         + lfoCents1 / 100.0;
    const double note2 = baseNote + tone.osc2.coarse + tone.osc2.fine / 100.0
                         + pitchEnvLevel * mapping::pitchEnvSemitones (tone.osc2.pitchEnvDepth)
                         + lfoCents2 / 100.0;

    voice.inc1 = std::min (0.45, noteToHz (note1) / sampleRate_);
    voice.inc2 = std::min (0.45, noteToHz (note2) / sampleRate_);

    // -- per-waveform variable control (PW / feedback / spread) ------------
    const auto pwValue = [&] (int oscIndex, const OscParams& osc)
    {
        double value = osc.pulseWidth;
        const auto contribution = [&] (const LfoParams& lfo, double lfoValue)
        {
            if (oscIndex == 1 && lfo.destination1 == LfoDest1::Pw1)
                value += depthScale (lfo.depth1) * lfoValue * 127.0;
            if (oscIndex == 2 && lfo.destination2 == LfoDest2::Pw2)
                value += depthScale (lfo.depth2) * lfoValue * 127.0;
        };
        contribution (tone.lfo1, runtime.lfo1Value);
        contribution (tone.lfo2, runtime.lfo2Value);
        if ((oscIndex == 1 && patch_.modulationAssign == ModulationAssign::Pw1)
            || (oscIndex == 2 && patch_.modulationAssign == ModulationAssign::Pw2))
            value += modulation_ * 63.0 * runtime.lfo2Value;
        return std::clamp (value, 0.0, 127.0);
    };

    const double pw1 = pwValue (1, tone.osc1);
    const double pw2 = pwValue (2, tone.osc2);
    voice.duty1 = mapping::pulseDuty (pw1);
    voice.duty2 = mapping::pulseDuty (pw2);
    voice.superAmount1 = mapping::superSawDetuneAmount (pw1 / 127.0);
    voice.superAmount2 = mapping::superSawDetuneAmount (pw2 / 127.0);
    voice.fbGain1 = mapping::fbOscGain (pw1);
    voice.fbGain2 = mapping::fbOscGain (pw2);

    // Supersaw HPF at the note fundamental (reported mechanism; the exact
    // corner/Q pair is OQ-04). RBJ high-pass, Q = 0.707, per oscillator.
    const auto trackedHighPass = [this] (double f0Hz)
    {
        const double f0 = std::clamp (f0Hz, 10.0, 0.45 * sampleRate_);
        const double w0 = twoPi * f0 / sampleRate_;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + alpha;
        BiquadCoeffs coeffs;
        coeffs.b0 = ((1.0 + cw) * 0.5) / a0;
        coeffs.b1 = (-(1.0 + cw)) / a0;
        coeffs.b2 = coeffs.b0;
        coeffs.a1 = (-2.0 * cw) / a0;
        coeffs.a2 = (1.0 - alpha) / a0;
        return coeffs;
    };
    if (tone.osc1.wave == Waveform::SuperSaw)
        voice.superHpf1 = trackedHighPass (noteToHz (note1));
    if (tone.osc2.wave == Waveform::SuperSaw)
        voice.superHpf2 = trackedHighPass (noteToHz (note2));

    // -- filter ------------------------------------------------------------
    const double filterEnvLevel = voice.filterEnv.advance (tickSamples);
    double lfoFilterOct = 0.0;
    if (tone.lfo1.destination1 == LfoDest1::Filter)
        lfoFilterOct += mapping::lfoFilterOctaves (tone.lfo1.depth1) * runtime.lfo1Value;
    if (tone.lfo2.destination1 == LfoDest1::Filter)
        lfoFilterOct += mapping::lfoFilterOctaves (tone.lfo2.depth1) * runtime.lfo2Value;
    if (patch_.modulationAssign == ModulationAssign::Filter)
        lfoFilterOct += modulation_ * 2.0 * runtime.lfo2Value;

    const double cutoffBaseOct = std::log2 (mapping::cutoffHz (tone.cutoff));
    const double keyTrack = mapping::keyFollowOctavesPerOctave (tone.keyFollow)
                            * (voice.glidePitch - 60.0) / 12.0;
    const double envOct = filterEnvLevel * mapping::filterEnvOctaves (tone.filterEnvDepth);
    const double velocityOct = mapping::cutoffVelocityOctaves (
        tone.cutoffVelocitySens, voice.velocity);
    const double cutoffOctTarget =
        cutoffBaseOct + keyTrack + envOct + velocityOct + lfoFilterOct;
    const double resonanceTarget = mapping::resonanceDamping (tone.resonance);
    if (! voice.controlsPrimed)
    {
        voice.cutoffOctSlewed = cutoffOctTarget;
        voice.resonanceSlewed = resonanceTarget;
        voice.controlsPrimed = true;
    }
    else
    {
        const double slew =
            1.0 - std::exp (-tickSamples / (sampleRate_ * 0.0025));
        voice.cutoffOctSlewed += (cutoffOctTarget - voice.cutoffOctSlewed) * slew;
        voice.resonanceSlewed += (resonanceTarget - voice.resonanceSlewed) * slew;
    }
    const double fc = std::clamp (std::exp2 (voice.cutoffOctSlewed), 5.0,
                                  0.45 * sampleRate_);
    voice.filterG = std::tan (pi * fc / sampleRate_);
    voice.filterK = voice.resonanceSlewed;

    // -- amp ---------------------------------------------------------------
    const double levelNorm = tone.level / 127.0;
    double gain = levelNorm * levelNorm;
    const double sens = depthScale (tone.levelVelocitySens);
    // Positive sensitivity: quieter as velocity falls; negative inverts.
    if (sens >= 0.0)
        gain *= 1.0 - sens * (1.0 - voice.velocity);
    else
        gain *= 1.0 + sens * voice.velocity;
    double tremolo = 0.0;
    if (tone.lfo1.destination2 == LfoDest2::Amp)
        tremolo += depthScale (tone.lfo1.depth2) * runtime.lfo1Value;
    if (tone.lfo2.destination2 == LfoDest2::Amp)
        tremolo += depthScale (tone.lfo2.depth2) * runtime.lfo2Value;
    if (patch_.modulationAssign == ModulationAssign::Amp)
        tremolo += modulation_ * runtime.lfo2Value;
    gain *= std::max (0.0, 1.0 + tremolo);

    // Equal-power pan from the -64..+63 patch value.
    const double panNorm = (tone.pan + 64.0) / 127.0;
    const double panAngle = panNorm * (pi / 2.0);
    voice.ampGainL = gain * std::cos (panAngle);
    voice.ampGainR = gain * std::sin (panAngle);
}

// ---------------------------------------------------------------------------
// Audio-rate voice rendering
// ---------------------------------------------------------------------------

namespace
{
    struct OscOutput
    {
        double value;
        bool wrapped;
        double wrapOffset;  // samples since the wrap, 0..1 of a sample
    };

    // One classic-waveform oscillator sample with polyBLEP/BLAMP correction.
    inline OscOutput renderClassicWave (Waveform wave, double& phase, double inc,
                                        double duty, std::uint32_t& noiseRng) noexcept
    {
        phase += inc;
        bool wrapped = false;
        double wrapOffset = 0.0;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            wrapped = true;
            wrapOffset = phase / std::max (1.0e-9, inc);
        }

        switch (wave)
        {
            case Waveform::Saw:
            {
                double value = 2.0 * phase - 1.0;
                value -= polyBlep (phase, inc);
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Square:
            case Waveform::PulseSquare:
            {
                const double width = wave == Waveform::Square ? 0.5 : duty;
                double value = phase < width ? 1.0 : -1.0;
                value += polyBlep (phase, inc);
                value -= polyBlep (frac (phase - width + 1.0), inc);
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Triangle:
            {
                double value = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
                const double scale = 8.0 * inc;
                value += scale * polyBlamp (phase, inc);
                value -= scale * polyBlamp (frac (phase + 0.5), inc);
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Sine:
                return { std::sin (twoPi * phase), wrapped, wrapOffset };
            case Waveform::Noise:
            {
                noiseRng ^= noiseRng << 13;
                noiseRng ^= noiseRng >> 17;
                noiseRng ^= noiseRng << 5;
                return { (noiseRng >> 8) * (1.0 / 8388607.5) - 1.0, wrapped,
                         wrapOffset };
            }
            default:
                return { 0.0, wrapped, wrapOffset };
        }
    }
} // namespace

void Engine::renderVoiceTick (Voice& voice, float* mono, int samples)
{
    const TonePatch& tone = tonePatch (voice.part);
    const Waveform wave1 = tone.osc1.wave;
    const Waveform wave2 = tone.osc2.wave;

    // Balance: each leg at unity in the center, the opposite leg fading
    // linearly to silence at the extremes (voiced law, settled endpoints).
    const double legGain1 = std::min (1.0, (63.0 - tone.balance) / 63.0);
    const double legGain2 = std::min (1.0, (63.0 + tone.balance) / 63.0);

    const double centerGain = mapping::superSawCenterGain();
    const double sideGain = mapping::superSawSideGain();

    const auto superSaw = [&] (OscState& osc, double inc, double amount,
                               const BiquadCoeffs& hpf)
    {
        double sum = 0.0;
        for (std::size_t index = 0; index < 7; ++index)
        {
            const double detune = 1.0 + mapping::superSawOffsets[index] * amount;
            double& phase = osc.superPhases[index];
            phase = frac (phase + inc * detune);
            sum += (index == 3 ? centerGain : sideGain) * (2.0 * phase - 1.0);
        }
        // The reported pitch-tracked HPF on the summed stack.
        const double x = sum * (1.0 / 2.5);
        const double y = hpf.b0 * x + hpf.b1 * osc.hpfX1 + hpf.b2 * osc.hpfX2
                         - hpf.a1 * osc.hpfY1 - hpf.a2 * osc.hpfY2;
        osc.hpfX2 = osc.hpfX1;
        osc.hpfX1 = x;
        osc.hpfY2 = osc.hpfY1;
        osc.hpfY1 = y;
        return y;
    };

    // FB OSC: a sawtooth with a soft-clipped feedback comb at half the
    // fundamental period (voiced mechanism constants, OQ-06).
    const auto feedbackOsc = [] (OscState& osc, double inc, double fbGain)
    {
        osc.phase = frac (osc.phase + inc);
        const double saw = 2.0 * osc.phase - 1.0;
        const double periodSamples = 1.0 / std::max (1.0e-6, inc);
        const int size = static_cast<int> (osc.comb.size());
        const double delay =
            std::clamp (periodSamples * 0.5, 2.0, static_cast<double> (size - 4));
        double readPos = osc.combWrite - delay;
        while (readPos < 0.0)
            readPos += size;
        const int index0 = static_cast<int> (readPos) % size;
        const int index1 = (index0 + 1) % size;
        const double fracPos = readPos - std::floor (readPos);
        const double fed =
            osc.comb[static_cast<std::size_t> (index0)] * (1.0 - fracPos)
            + osc.comb[static_cast<std::size_t> (index1)] * fracPos;
        const double out = saw + fbGain * softClip (fed);
        osc.combState += 0.55 * (out - osc.combState);
        osc.comb[static_cast<std::size_t> (osc.combWrite)] =
            static_cast<float> (softClip (osc.combState) * 0.995);
        osc.combWrite = (osc.combWrite + 1) % size;
        return out * 0.6;
    };

    for (int i = 0; i < samples; ++i)
    {
        // ---- OSC2 (rendered first so SYNC can slave OSC1 to it) ----------
        double sample2 = 0.0;
        bool osc2Wrapped = false;
        double osc2WrapOffset = 0.0;
        switch (wave2)
        {
            case Waveform::SuperSaw:
            {
                // The center saw carries the cycle SYNC follows: its detune
                // offset is zero, so it advances by exactly inc2.
                const double centerBefore = voice.osc2.superPhases[3];
                sample2 = superSaw (voice.osc2, voice.inc2, voice.superAmount2,
                                    voice.superHpf2);
                if (centerBefore + voice.inc2 >= 1.0)
                {
                    osc2Wrapped = true;
                    osc2WrapOffset = (centerBefore + voice.inc2 - 1.0)
                                     / std::max (1.0e-9, voice.inc2);
                }
                break;
            }
            case Waveform::FbOsc:
            {
                const double phaseBefore = voice.osc2.phase;
                sample2 = feedbackOsc (voice.osc2, voice.inc2, voice.fbGain2);
                if (phaseBefore + voice.inc2 >= 1.0)
                {
                    osc2Wrapped = true;
                    osc2WrapOffset = (phaseBefore + voice.inc2 - 1.0)
                                     / std::max (1.0e-9, voice.inc2);
                }
                break;
            }
            case Waveform::ExtIn:
                sample2 = 0.0;  // no external bus in v1 (documented)
                voice.osc2.phase = frac (voice.osc2.phase + voice.inc2);
                break;
            default:
            {
                const auto out = renderClassicWave (wave2, voice.osc2.phase,
                                                    voice.inc2, voice.duty2,
                                                    voice.noiseRng);
                sample2 = out.value;
                osc2Wrapped = out.wrapped;
                osc2WrapOffset = out.wrapOffset;
                break;
            }
        }

        // ---- OSC1 --------------------------------------------------------
        // Hard sync (settled behavior): OSC1 restarts its cycle at each OSC2
        // cycle start. The reset is naive — the modelled DSP's own sync
        // aliases audibly, and no source documents band-limiting there.
        if (tone.mixType == MixModType::Sync && osc2Wrapped
            && wave1 != Waveform::Noise && wave1 != Waveform::SuperSaw
            && wave1 != Waveform::FbOsc && wave1 != Waveform::ExtIn)
        {
            double newPhase = osc2WrapOffset * voice.inc1 - voice.inc1;
            while (newPhase < 0.0)
                newPhase += 1.0;
            voice.osc1.phase = newPhase;
        }

        double sample1 = 0.0;
        switch (wave1)
        {
            case Waveform::SuperSaw:
                sample1 = superSaw (voice.osc1, voice.inc1, voice.superAmount1,
                                    voice.superHpf1);
                break;
            case Waveform::FbOsc:
                sample1 = feedbackOsc (voice.osc1, voice.inc1, voice.fbGain1);
                break;
            case Waveform::ExtIn:
                sample1 = 0.0;
                voice.osc1.phase = frac (voice.osc1.phase + voice.inc1);
                break;
            default:
            {
                const auto out = renderClassicWave (wave1, voice.osc1.phase,
                                                    voice.inc1, voice.duty1,
                                                    voice.noiseRng);
                sample1 = out.value;
                break;
            }
        }

        // ---- MIX/MOD -----------------------------------------------------
        // RING replaces the OSC1 leg with the product (settled: balance fully
        // left outputs the ring-modulated sound).
        const double leg1 = tone.mixType == MixModType::Ring ? sample1 * sample2
                                                             : sample1;
        double mixed = legGain1 * leg1 + legGain2 * sample2;

        // LOW FREQ shelf (voiced 200 Hz, +/-8 dB).
        if (tone.lowFreq != LowFreqMode::Flat)
        {
            const double a = twoPi * mapping::lowShelfHz / sampleRate_;
            const double coeff = a / (1.0 + a);
            voice.shelfState += coeff * (mixed - voice.shelfState);
            const double gainDb = tone.lowFreq == LowFreqMode::Boost
                                      ? mapping::lowShelfGainDb
                                      : -mapping::lowShelfGainDb;
            const double gain = std::pow (10.0, gainDb / 20.0);
            mixed += (gain - 1.0) * voice.shelfState;
        }

        // ---- FILTER ------------------------------------------------------
        double filtered = mixed;
        if (tone.filterType != FilterType::Bypass)
        {
            const double g = voice.filterG;
            const double k = voice.filterK;
            const double a1 = 1.0 / (1.0 + g * (g + k));
            const double a2 = g * a1;

            auto stagePass = [&] (SvfStage& stage, double input, double damping,
                                  double stageA1, double stageA2)
            {
                const double v3 = input - stage.ic2eq;
                const double v1 = stageA1 * stage.ic1eq + stageA2 * v3;
                const double v2 = stage.ic2eq + g * v1;
                stage.ic1eq = 2.0 * v1 - stage.ic1eq;
                stage.ic2eq = 2.0 * v2 - stage.ic2eq;
                // Stage limiter: bounds self-oscillation growth (the manual's
                // "may not stop at all" is a bounded oscillation on hardware).
                // Continuous soft knee — linear to 1.5, saturating toward 2.5
                // — so limiting never steps the state.
                const auto limitState = [] (double state)
                {
                    const double a = std::abs (state);
                    if (a <= 1.5)
                        return state;
                    const double over = a - 1.5;
                    const double limited = 1.5 + over / (1.0 + over);
                    return state < 0.0 ? -limited : limited;
                };
                stage.ic1eq = limitState (stage.ic1eq);
                stage.ic2eq = limitState (stage.ic2eq);
                const double lp = v2;
                const double bp = v1;
                const double hp = input - damping * v1 - v2;
                switch (tone.filterType)
                {
                    case FilterType::Lpf: return lp;
                    case FilterType::Hpf: return hp;
                    case FilterType::Bpf: return damping * bp;
                    case FilterType::Bypass: break;
                }
                return input;
            };

            filtered = stagePass (voice.filter1, filtered, k, a1, a2);
            if (tone.filterSlope == FilterSlope::Db24)
            {
                // Second, non-resonant 2-pole stage (voiced topology).
                const double k2 = 1.2;
                const double b1 = 1.0 / (1.0 + g * (g + k2));
                const double b2 = g * b1;
                filtered = stagePass (voice.filter2, filtered, k2, b1, b2);
            }
        }

        // ---- AMP: overdrive, envelope, level, pan ------------------------
        if (tone.overdrive)
        {
            const double pre = mapping::overdrivePreGain (tone.drive);
            filtered = std::tanh (pre * filtered) * std::pow (pre, -0.4);
        }

        const double env = voice.ampEnv.advance (1);
        mono[i] = static_cast<float> (filtered * env);
    }

    // Once per tick: keep decayed states out of denormal territory.
    voice.filter1.ic1eq = flushDenormal (voice.filter1.ic1eq);
    voice.filter1.ic2eq = flushDenormal (voice.filter1.ic2eq);
    voice.filter2.ic1eq = flushDenormal (voice.filter2.ic1eq);
    voice.filter2.ic2eq = flushDenormal (voice.filter2.ic2eq);
    voice.shelfState = flushDenormal (voice.shelfState);
    voice.osc1.combState = flushDenormal (voice.osc1.combState);
    voice.osc2.combState = flushDenormal (voice.osc2.combState);
    voice.osc1.hpfY1 = flushDenormal (voice.osc1.hpfY1);
    voice.osc1.hpfY2 = flushDenormal (voice.osc1.hpfY2);
    voice.osc2.hpfY1 = flushDenormal (voice.osc2.hpfY1);
    voice.osc2.hpfY2 = flushDenormal (voice.osc2.hpfY2);
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

void Engine::updateEffectCoefficients()
{
    reverbFeedback_ = 0.0;  // recomputed per line in processEffects
}

void Engine::processEffects (const float* dryL, const float* dryR,
                             const float* delaySendL, const float* delaySendR,
                             const float* reverbSendL, const float* reverbSendR,
                             float* outL, float* outR, int samples)
{
    const DelayParams& delayParams = patch_.delay;
    const ReverbParams& reverbParams = patch_.reverb;

    const bool delayOn = patch_.delayOn;
    const bool reverbOn = patch_.reverbOn;

    // -- delay coefficients ------------------------------------------------
    const double delayTargetSamples =
        mapping::delaySeconds (delayParams.time) * sampleRate_;
    const double timeSmoothing = onePoleCoeff (sampleRate_, 0.08);
    const double feedback = delayParams.feedback / 100.0;
    const double dampHz = delayHfDampHz[static_cast<std::size_t> (delayParams.hfDamp)];
    const double dampCoeff = dampHz <= 0.0
                                 ? 1.0
                                 : 1.0 - std::exp (-twoPi * dampHz / sampleRate_);
    const double modRateHz = 0.02 * std::pow (400.0, delayParams.modulationRate / 127.0);
    const double modDepthSamples =
        (delayParams.modulationDepth / 127.0) * 0.008 * sampleRate_;
    const double modInc = modRateHz / sampleRate_;
    const int delaySize = static_cast<int> (delayL_.buffer.size());

    // -- reverb coefficients -------------------------------------------------
    const double rt60 = mapping::reverbSeconds (reverbParams.time, reverbParams.size);
    const double highCutHz =
        reverbHighCutHz[static_cast<std::size_t> (reverbParams.highCut)];
    const double highCutCoeff = highCutHz <= 0.0
                                    ? 1.0
                                    : 1.0 - std::exp (-twoPi * highCutHz / sampleRate_);
    const double lfHz =
        reverbLfDampHz[static_cast<std::size_t> (reverbParams.lfDampFrequency)];
    const double hfHz =
        reverbHfDampHz[static_cast<std::size_t> (reverbParams.hfDampFrequency)];
    const double lfCoeff = 1.0 - std::exp (-twoPi * lfHz / sampleRate_);
    const double hfCoeff = 1.0 - std::exp (-twoPi * hfHz / sampleRate_);
    const double lfGain = std::pow (10.0, reverbParams.lfDampGain / 20.0);
    const double hfGain = std::pow (10.0, reverbParams.hfDampGain / 20.0);
    const double diffusionGain = 0.25 + 0.5 * (reverbParams.diffusion / 127.0);
    const double densityGain = 0.2 + 0.55 * (reverbParams.density / 127.0);
    const int preDelaySamples = std::min (
        static_cast<int> ((reverbParams.preDelay * (100.0 / 125.0)) * 0.001 * sampleRate_),
        static_cast<int> (reverb_.preDelay.size()) - 2);

    std::array<double, Reverb::lineCount> lineFeedback {};
    for (int i = 0; i < Reverb::lineCount; ++i)
    {
        const double lengthSeconds =
            reverb_.lengths[static_cast<std::size_t> (i)] / sampleRate_;
        lineFeedback[static_cast<std::size_t> (i)] =
            std::pow (10.0, -3.0 * lengthSeconds / std::max (0.05, rt60));
    }

    for (int i = 0; i < samples; ++i)
    {
        double wetDelayL = 0.0, wetDelayR = 0.0;

        if (delayOn)
        {
            delayTimeSmoothed_ += (delayTargetSamples - delayTimeSmoothed_) * timeSmoothing;
            delayModPhase_ = frac (delayModPhase_ + modInc);
            const double lfoL = std::sin (twoPi * delayModPhase_);
            const double lfoR = std::sin (twoPi * delayModPhase_ + pi * 0.5);

            auto tapLine = [&] (DelayLine& line, double modulated, double input)
            {
                const double delaySamplesNow = std::clamp (
                    delayTimeSmoothed_ + modulated, 2.0,
                    static_cast<double> (delaySize - 4));
                double readPos = line.write - delaySamplesNow;
                while (readPos < 0.0)
                    readPos += delaySize;
                const int index0 = static_cast<int> (readPos) % delaySize;
                const int index1 = (index0 + 1) % delaySize;
                const double fracPos = readPos - std::floor (readPos);
                // A tap reaching behind the panic point reads silence; echoes
                // of post-panic input come through immediately.
                const double tapped =
                    delaySamplesNow + 2.0 > static_cast<double> (line.fresh)
                        ? 0.0
                        : line.buffer[static_cast<std::size_t> (index0)]
                                  * (1.0 - fracPos)
                              + line.buffer[static_cast<std::size_t> (index1)]
                                    * fracPos;
                line.dampState += dampCoeff * (tapped - line.dampState);
                const double damped = line.dampState;
                line.buffer[static_cast<std::size_t> (line.write)] =
                    static_cast<float> (softClip (input + damped * feedback));
                line.write = (line.write + 1) % delaySize;
                line.fresh = std::min (line.fresh + 1, delaySize);
                return damped;
            };

            wetDelayL = tapLine (delayL_, lfoL * modDepthSamples, delaySendL[i]);
            wetDelayR = tapLine (delayR_, lfoR * modDepthSamples, delaySendR[i]);
        }

        double wetReverbL = 0.0, wetReverbR = 0.0;
        if (reverbOn)
        {
            // Settled routing: the delay feeds the reverb in series, and each
            // tone also has its own reverb send.
            double input = 0.5 * (reverbSendL[i] + reverbSendR[i])
                           + 0.5 * (wetDelayL + wetDelayR);

            // Pre-delay. Reads behind the panic point are silence, here and
            // in every buffer below — the network's write heads advance in
            // lockstep, so one freshness count covers them all.
            const int reverbFresh = reverb_.fresh;
            reverb_.preDelay[static_cast<std::size_t> (reverb_.preDelayWrite)] =
                static_cast<float> (input);
            int readIndex = reverb_.preDelayWrite - preDelaySamples;
            if (readIndex < 0)
                readIndex += static_cast<int> (reverb_.preDelay.size());
            input = preDelaySamples > reverbFresh
                        ? 0.0
                        : reverb_.preDelay[static_cast<std::size_t> (readIndex)];
            reverb_.preDelayWrite = (reverb_.preDelayWrite + 1)
                                    % static_cast<int> (reverb_.preDelay.size());

            // Input diffusion: four series allpasses.
            for (int d = 0; d < 4; ++d)
            {
                auto& buffer = reverb_.diffusers[static_cast<std::size_t> (d)];
                int& write = reverb_.diffuserWrites[static_cast<std::size_t> (d)];
                const double gain = d < 2 ? diffusionGain : densityGain;
                const double delayed =
                    static_cast<int> (buffer.size()) > reverbFresh
                        ? 0.0
                        : buffer[static_cast<std::size_t> (write)];
                const double next = input + delayed * gain;
                buffer[static_cast<std::size_t> (write)] =
                    static_cast<float> (next);
                input = delayed - next * gain;
                write = (write + 1) % static_cast<int> (buffer.size());
            }

            // Eight-line FDN with Householder feedback and per-line damping.
            std::array<double, Reverb::lineCount> taps {};
            double tapSum = 0.0;
            for (int line = 0; line < Reverb::lineCount; ++line)
            {
                auto& buffer = reverb_.lines[static_cast<std::size_t> (line)];
                const int size = static_cast<int> (buffer.size());
                int readPos = reverb_.writes[static_cast<std::size_t> (line)]
                              - reverb_.lengths[static_cast<std::size_t> (line)];
                if (readPos < 0)
                    readPos += size;
                taps[static_cast<std::size_t> (line)] =
                    reverb_.lengths[static_cast<std::size_t> (line)] > reverbFresh
                        ? 0.0
                        : buffer[static_cast<std::size_t> (readPos)];
                tapSum += taps[static_cast<std::size_t> (line)];
            }
            const double householder = tapSum * (2.0 / Reverb::lineCount);
            for (int line = 0; line < Reverb::lineCount; ++line)
            {
                auto& buffer = reverb_.lines[static_cast<std::size_t> (line)];
                const int size = static_cast<int> (buffer.size());
                double value = taps[static_cast<std::size_t> (line)] - householder;
                value *= lineFeedback[static_cast<std::size_t> (line)];

                // HF damping: shelve down content above hfHz by hfGain.
                double& high = reverb_.highStates[static_cast<std::size_t> (line)];
                high += hfCoeff * (value - high);
                value = high + hfGain * (value - high);
                // LF damping: shelve down content below lfHz by lfGain.
                double& low = reverb_.lowStates[static_cast<std::size_t> (line)];
                low += lfCoeff * (value - low);
                value = value - low + lfGain * low;

                buffer[static_cast<std::size_t> (
                    reverb_.writes[static_cast<std::size_t> (line)])] =
                    static_cast<float> (value + input * 0.35);
                reverb_.writes[static_cast<std::size_t> (line)] =
                    (reverb_.writes[static_cast<std::size_t> (line)] + 1) % size;
            }
            wetReverbL = taps[0] - taps[1] + taps[2] - taps[3] + taps[4] - taps[5];
            wetReverbR = taps[1] - taps[2] + taps[3] - taps[4] + taps[5] - taps[6];

            // Settled HIGH CUT on the wet return.
            reverb_.highCutStateL += highCutCoeff * (wetReverbL - reverb_.highCutStateL);
            reverb_.highCutStateR += highCutCoeff * (wetReverbR - reverb_.highCutStateR);
            wetReverbL = reverb_.highCutStateL;
            wetReverbR = reverb_.highCutStateR;
            reverb_.fresh = std::min (reverb_.fresh + 1, 1 << 30);
        }

        outL[i] = static_cast<float> (dryL[i] + wetDelayL + wetReverbL * 0.8);
        outR[i] = static_cast<float> (dryR[i] + wetDelayR + wetReverbR * 0.8);
    }

    delayL_.dampState = flushDenormal (delayL_.dampState);
    delayR_.dampState = flushDenormal (delayR_.dampState);
    for (int line = 0; line < Reverb::lineCount; ++line)
    {
        reverb_.lowStates[static_cast<std::size_t> (line)] =
            flushDenormal (reverb_.lowStates[static_cast<std::size_t> (line)]);
        reverb_.highStates[static_cast<std::size_t> (line)] =
            flushDenormal (reverb_.highStates[static_cast<std::size_t> (line)]);
    }
    reverb_.highCutStateL = flushDenormal (reverb_.highCutStateL);
    reverb_.highCutStateR = flushDenormal (reverb_.highCutStateR);
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------

void Engine::process (float* left, float* right, int numSamples)
{
    int offset = 0;
    float blockPeakL = 0.0f, blockPeakR = 0.0f;

    while (offset < numSamples)
    {
        const int tick = std::min (controlInterval, numSamples - offset);
        const int guarded = std::min (tick, maxBlock_);

        advanceToneLfos (guarded);

        std::fill (dryL_.begin(), dryL_.begin() + guarded, 0.0f);
        std::fill (dryR_.begin(), dryR_.begin() + guarded, 0.0f);
        std::fill (sendDelayL_.begin(), sendDelayL_.begin() + guarded, 0.0f);
        std::fill (sendDelayR_.begin(), sendDelayR_.begin() + guarded, 0.0f);
        std::fill (sendReverbL_.begin(), sendReverbL_.begin() + guarded, 0.0f);
        std::fill (sendReverbR_.begin(), sendReverbR_.begin() + guarded, 0.0f);

        for (auto& voice : voices_)
        {
            if (! voice.active)
                continue;

            updateVoiceControls (voice, guarded);
            renderVoiceTick (voice, scratchMono_.data(), guarded);

            if (voice.ampEnv.idle())
                voice.active = false;

            const TonePatch& tone = tonePatch (voice.part);
            const double delaySend = tone.delayDepth / 127.0;
            const double reverbSend = tone.reverbDepth / 127.0;
            const auto gainL = static_cast<float> (voice.ampGainL * voiceHeadroom);
            const auto gainR = static_cast<float> (voice.ampGainR * voiceHeadroom);

            // Tone balance sits between the two tones (settled parameter,
            // voiced law shared with the oscillator balance).
            const double toneGain =
                voice.part == Part::Upper
                    ? std::min (1.0, (63.0 + patch_.toneBalance) / 63.0)
                    : std::min (1.0, (63.0 - patch_.toneBalance) / 63.0);

            for (int i = 0; i < guarded; ++i)
            {
                const float sample = scratchMono_[static_cast<std::size_t> (i)];
                const auto l = static_cast<float> (sample * gainL * toneGain);
                const auto r = static_cast<float> (sample * gainR * toneGain);
                dryL_[static_cast<std::size_t> (i)] += l;
                dryR_[static_cast<std::size_t> (i)] += r;
                sendDelayL_[static_cast<std::size_t> (i)] +=
                    static_cast<float> (l * delaySend);
                sendDelayR_[static_cast<std::size_t> (i)] +=
                    static_cast<float> (r * delaySend);
                sendReverbL_[static_cast<std::size_t> (i)] +=
                    static_cast<float> (l * reverbSend);
                sendReverbR_[static_cast<std::size_t> (i)] +=
                    static_cast<float> (r * reverbSend);
            }
        }

        processEffects (dryL_.data(), dryR_.data(), sendDelayL_.data(),
                        sendDelayR_.data(), sendReverbL_.data(),
                        sendReverbR_.data(), left + offset, right + offset,
                        guarded);

        // -- output stage: documented analog path + master gains -----------
        const double masterTarget = (masterLevel_ / 127.0)
                                    * (patch_.patchLevel / 127.0)
                                    * expression_ * partLevel_;
        const double masterCoeff = onePoleCoeff (sampleRate_, 0.01) * guarded;
        smoothedMaster_ += (masterTarget - smoothedMaster_)
                           * std::min (1.0, masterCoeff);

        // Part pan (received CC#10): a constant-power tilt on the final pair,
        // unity at center.
        const double panAngle = (partPan_ + 1.0) * 0.25 * pi;
        const double partPanGain[2] { std::cos (panAngle) * 1.4142135623730951,
                                      std::sin (panAngle) * 1.4142135623730951 };

        for (int channel = 0; channel < 2; ++channel)
        {
            float* out = channel == 0 ? left + offset : right + offset;
            for (int i = 0; i < guarded; ++i)
            {
                double x = out[i] * smoothedMaster_ * partPanGain[channel];
                // 22 uF / 22 k coupling (0.329 Hz).
                const double dc = x - dcX1_[channel] + dcCoeff_ * dcY1_[channel];
                dcX1_[channel] = x;
                dcY1_[channel] = dc;
                // The two documented RC poles.
                rcState1_[channel] += rcCoeff1_ * (dc - rcState1_[channel]);
                rcState2_[channel] += rcCoeff2_ * (rcState1_[channel] - rcState2_[channel]);
                const double limited = outputLimit (rcState2_[channel]);
                out[i] = static_cast<float> (limited);
                if (channel == 0)
                    blockPeakL = std::max (blockPeakL, std::abs (out[i]));
                else
                    blockPeakR = std::max (blockPeakR, std::abs (out[i]));
            }
        }

        offset += guarded;
    }

    const float decayedL = outputLevel_[0].load (std::memory_order_relaxed) * 0.85f;
    const float decayedR = outputLevel_[1].load (std::memory_order_relaxed) * 0.85f;
    outputLevel_[0].store (std::max (blockPeakL, decayedL), std::memory_order_relaxed);
    outputLevel_[1].store (std::max (blockPeakR, decayedR), std::memory_order_relaxed);
}

} // namespace septum

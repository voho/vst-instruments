#include "SeptumEngine.h"

namespace septum
{
namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double twoPi = 2.0 * pi;

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

    [[nodiscard]] inline double outputLimit (double x) noexcept
    {
        const double a = std::abs (x);
        if (a <= mapping::outputLimitKnee)
            return x;
        const double over = a - mapping::outputLimitKnee;
        const double limited =
            mapping::outputLimitKnee
            + mapping::outputLimitRange
                  * (1.0 - std::exp (-over * (1.0 / mapping::outputLimitRange)));
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
                // Two-sided: SUSTAIN is automatable and setPatch reconfigures
                // every sounding voice, so it can be raised above the level a
                // held note has already decayed to. A one-sided test passed
                // immediately in that direction and the next sample assigned
                // the new sustain outright — a step of the whole difference
                // inside one sample, on an ordinary control move.
                if (std::abs (level - sustain) < 1.0e-4)
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
// OVERDRIVE, at a fixed internal rate
// ---------------------------------------------------------------------------

namespace
{
    // log(cosh(x)), the antiderivative of tanh, in the form that does not
    // overflow: |x| is exact for large arguments and log1p carries the rest.
    [[nodiscard]] inline double logCosh (double x) noexcept
    {
        const double a = std::abs (x);
        return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
    }
} // namespace

void OverdriveStage::prepare (double hostRateHz) noexcept
{
    factor = mapping::overdriveOversampling (hostRateHz);
    outer.configure (halfBandOuterTaps.data(),
                     static_cast<int> (halfBandOuterTaps.size()));
    inner.configure (halfBandInnerTaps.data(),
                     static_cast<int> (halfBandInnerTaps.size()));
    // One round trip through a stage costs 2p of that stage's slow rate; the
    // inner stage's slow rate is 2x, so its cost halves in host-rate samples.
    latency = factor >= 2 ? 2 * outer.pairs : 0;
    if (factor >= 4)
        latency += inner.pairs;
    clear();
}

void OverdriveStage::clear() noexcept
{
    outer.clear();
    inner.clear();
    previousInput = 0.0;
    previousIntegral = logCosh (0.0);
    bypass.fill (0.0);
    bypassWrite = 0;
    wasEnabled = false;
}

double OverdriveStage::shapeChain (double x, double preGain) noexcept
{
    // First-order antiderivative anti-aliasing of tanh.
    const auto shape = [this] (double input)
    {
        const double integral = logCosh (input);
        const double step = input - previousInput;
        const double result =
            std::abs (step) < 1.0e-6
                ? std::tanh (0.5 * (input + previousInput))
                : (integral - previousIntegral) / step;
        previousInput = input;
        previousIntegral = integral;
        return result;
    };

    // ADAA carries state from one internal sample to the next, so every call
    // to `shape` lands in a named local first: the order the shaper sees its
    // input in is the order time runs in, not whatever order the compiler
    // picks for a call's arguments.
    if (factor == 1)
        return shape (preGain * x);

    if (factor == 2)
    {
        double a = 0.0, b = 0.0;
        outer.upsample (x, a, b);
        const double shapedA = shape (preGain * a);
        const double shapedB = shape (preGain * b);
        return outer.downsample (shapedA, shapedB);
    }

    double a = 0.0, b = 0.0;
    outer.upsample (x, a, b);
    double a0 = 0.0, a1 = 0.0, b0 = 0.0, b1 = 0.0;
    inner.upsample (a, a0, a1);
    inner.upsample (b, b0, b1);
    const double shapedA0 = shape (preGain * a0);
    const double shapedA1 = shape (preGain * a1);
    const double down0 = inner.downsample (shapedA0, shapedA1);
    const double shapedB0 = shape (preGain * b0);
    const double shapedB1 = shape (preGain * b1);
    const double down1 = inner.downsample (shapedB0, shapedB1);
    return outer.downsample (down0, down1);
}

double OverdriveStage::process (double x, double preGain, double compensation,
                                bool enabled) noexcept
{
    // The delay line runs whether the shaper does or not. Feeding it only
    // while the switch is off would leave it holding pre-switch audio, and
    // turning OVERDRIVE off would replay that burst before the current signal
    // caught up.
    const auto size = static_cast<int> (bypass.size());
    const int written = bypassWrite;
    bypass[static_cast<std::size_t> (written)] = x;
    const double delayed =
        bypass[static_cast<std::size_t> ((written - latency + size) % size)];
    bypassWrite = (bypassWrite + 1) % size;

    if (! enabled)
    {
        wasEnabled = false;
        return latency == 0 ? x : delayed;
    }

    // OVERDRIVE has just come back in. Every state in this chain — the two
    // half-band stages, up and down, and the ADAA's antiderivative reference
    // — carries across samples, and standing still while the switch was out
    // left them describing whatever was playing when it was last in, a third
    // of a second ago or a minute. They are all linear or memoryless in the
    // last few samples, so the history the bypass line has been keeping all
    // along is enough to rebuild them exactly: replay it, throw the output
    // away, and the first shaped sample lands on the signal that is actually
    // playing. It costs one ring's worth of chain evaluations at the
    // transition and nothing at all while the switch stays put.
    if (! wasEnabled)
    {
        wasEnabled = true;
        for (int i = size - 1; i >= 1; --i)
            (void) shapeChain (
                bypass[static_cast<std::size_t> ((written - i + size) % size)],
                preGain);
    }

    return shapeChain (x, preGain) * compensation;
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
        voice.overdrive.prepare (sampleRate_);
    }
    latencySamples_ = voices_.front().overdrive.latency;

    const auto delaySamples = static_cast<std::size_t> (sampleRate_ * 1.45) + 8;
    delayL_.buffer.assign (delaySamples, 0.0f);
    delayR_.buffer.assign (delaySamples, 0.0f);

    const double sizeScale = mapping::reverbSizeScale (patch_.reverb.size);
    for (int i = 0; i < Reverb::lineCount; ++i)
    {
        const auto length = static_cast<int> (
                                mapping::reverbLineSeconds[static_cast<std::size_t> (i)]
                                * sizeScale * sampleRate_)
                            | 1;
        reverb_.lengths[static_cast<std::size_t> (i)] = std::max (32, length);
        reverb_.lines[static_cast<std::size_t> (i)]
            .assign (static_cast<std::size_t> (sampleRate_ * 0.1) + 16, 0.0f);
    }
    for (int i = 0; i < 4; ++i)
        reverb_.diffusers[static_cast<std::size_t> (i)]
            .assign (static_cast<std::size_t> (
                         mapping::reverbDiffuserSeconds[static_cast<std::size_t> (i)]
                         * sampleRate_)
                         + 8,
                     0.0f);
    reverb_.preDelay.assign (static_cast<std::size_t> (sampleRate_ * 0.105) + 8, 0.0f);

    externalDirectL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    externalDirectR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    externalMono_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    scratchMono_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    dryL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    dryR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendDelayL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendDelayR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendReverbL_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);
    sendReverbR_.assign (static_cast<std::size_t> (maxBlock_), 0.0f);

    // Analog output stage, from the service notes' component values:
    // 22 uF into 22 k -> 0.329 Hz coupling; RC poles 8.2k/820p -> 23.7 kHz
    // and 4.7k/270p -> 125.4 kHz. Both realised at their component values by
    // mapping::onePoleAtCorner rather than clamped to 0.49 x fs: the clamp
    // put *both* poles on one frequency at every host rate at or below
    // 48 kHz — 21.6 kHz twice at 44.1 kHz — so the stage was up to 0.9 dB
    // brighter at 20 kHz than the network the service notes describe, and
    // its response depended on the host rate rather than on the instrument.
    dcCoeff_ = std::exp (-twoPi * 0.329 / sampleRate_);
    rcCoeff1_ = mapping::onePoleAtCorner (23700.0, sampleRate_);
    rcCoeff2_ = mapping::onePoleAtCorner (125400.0, sampleRate_);

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
        voice.overdrive.clear();
    }
    for (auto& tone : tones_)
    {
        tone = ToneRuntime {};
    }
    // Four LFOs, four random sequences. One shared default made both tones'
    // LFO 1 and LFO 2 walk the same one, so two S&H modulators at the same
    // rate produced bit-identical output. Fixed constants rather than a
    // clock, so two renders of the same patch still agree sample for sample.
    for (int index = 0; index < partCount; ++index)
    {
        tones_[static_cast<std::size_t> (index)].lfo1.seed (
            0x9e3779b9u + 0x51ed270bu * static_cast<std::uint32_t> (index));
        tones_[static_cast<std::size_t> (index)].lfo2.seed (
            0x2545f491u + 0x9e3779b9u * static_cast<std::uint32_t> (index));
    }
    for (auto& runtime : arpeggios_)
        runtime = ArpeggioRuntime {};
    // The external switches start settled wherever they are set: a reset is
    // not somebody moving a switch, so nothing crosses on the first tick.
    centerCancelFade_ = external_.centerCancel ? 1.0 : 0.0;
    audioFilterOnFade_ = external_.filterOn ? 1.0 : 0.0;
    audioFilterSlopeFade_ = external_.slope == FilterSlope::Db24 ? 1.0 : 0.0;
    audioFilterTypeMix_.snapTo (static_cast<int> (external_.type));
    arpeggioRunning_ = false;
    arpeggioActive_ = patch_.arpeggio.on;
    // Synced to the patch for the same reason the switch is: clearing the
    // keyboard is not a routing change, and nothing is held across one, so
    // the next tick must not read one as having happened.
    for (int part = 0; part < partCount; ++part)
        arpeggioDriven_[static_cast<std::size_t> (part)] =
            arpeggioDrives (part == 0 ? Part::Upper : Part::Lower);
    arpeggioStep_ = 0;
    arpeggioGridSection_ = 0;
    arpeggioStepRemaining_ = 0.0;
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
    for (int channel = 0; channel < 2; ++channel)
    {
        audioFilter1_[channel].clear();
        audioFilter2_[channel].clear();
    }
    audioFilterPrimed_ = false;
    monitorGain_ = 1.0;
    for (auto& channel : monitorDelay_)
        channel.fill (0.0f);
    monitorDelayWrite_ = 0;
    smoothedMonitorLevel_ = masterLevel_ / 127.0;
    smoothedInputGain_ = mapping::externalInputGain (external_.inputVolume);
    pitchBend_ = 0.0;
    modulation_ = 0.0;
    hold_ = false;
    sostenuto_ = false;
    smoothedMaster_ = masterLevel_ / 127.0;
    smoothedExpression_.fill (1.0);
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
        const double sizeScale = mapping::reverbSizeScale (patch_.reverb.size);
        for (int i = 0; i < Reverb::lineCount; ++i)
        {
            const auto length =
                static_cast<int> (
                    mapping::reverbLineSeconds[static_cast<std::size_t> (i)]
                    * sizeScale * sampleRate_)
                | 1;
            reverb_.lengths[static_cast<std::size_t> (i)] = std::max (
                32, std::min (length,
                              static_cast<int> (reverb_.lines[static_cast<std::size_t> (i)].size()) - 2));
        }
    }

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

void Engine::setExternalInput (const ExternalInput& settings) noexcept
{
    external_ = settings;
    clampToDocumentedRanges (external_);
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
    syncArpeggioRouting();

    // The arpeggiator sits between the keyboard and the voice assigner: a key
    // it owns joins its chord instead of starting a voice.
    const auto route = [this, note, velocity] (Part part)
    {
        if (arpeggioDrives (part))
            arpeggioAddKey (part, note, velocity);
        else
            startNoteForPart (part, note, velocity);
    };

    switch (patch_.keyboardMode)
    {
        case KeyboardMode::Single:
            route (patch_.keyboardPart == KeyboardPart::Upper ? Part::Upper
                                                              : Part::Lower);
            break;
        case KeyboardMode::Dual:
            route (Part::Upper);
            route (Part::Lower);
            break;
        case KeyboardMode::Split:
            // Settled: keys at or right of the split point sound UPPER.
            route (note >= patch_.splitPoint ? Part::Upper : Part::Lower);
            break;
    }
}

void Engine::noteOff (int note)
{
    note = clampRaw (note, 0, 127);
    syncArpeggioRouting();
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        // The arpeggiator's key list is cleared unconditionally, because the
        // routing parameters — SPLIT ARPEGGIO, the keyboard mode, the
        // keyboard part — are automatable too, and a key whose press went to
        // one part must not leave an entry behind when its release is routed
        // somewhere else. Removing a key the list never had is a no-op.
        arpeggioRemoveKey (part, note);
        if (! arpeggioDrives (part))
            releaseNoteForPart (part, note);
    }
}

// Whether a part is arpeggiated is a patch decision, so it can be automated
// under a held chord - and the ARPEGGIO switch is not the only control that
// decides it. SPLIT ARPEGGIO, the keyboard mode and the keyboard part each
// move a part in or out of the arpeggiator's hands on their own, and all of
// them are automatable. Whichever way a part crosses, the keys under the
// player's fingers have to cross with it: a key whose note-on was routed one
// way and whose note-off is routed the other would otherwise leave a voice
// sounding with nothing left to release it.
// Every control that can move a part across the arpeggiator's boundary is
// watched, not just the ARPEGGIO switch: with SPLIT ARPEGGIO on Lower,
// holding an Upper key and then selecting Upper used to strand that key's
// normal voice, because its note-off saw a part the arpeggiator now drives
// and skipped the release.
//
// It has to be noticed *where it happens*, not only when audio is next
// rendered: a host can land a parameter change and a note-off at the same
// sample position, and then the note-off is consumed against the new routing
// with no render in between. The key was removed from the chord by one path
// and copied back into it by the other, and nothing short of a panic could
// end it. Same shape as the re-arm Step 10 moved into arpeggioRemoveKey.
void Engine::syncArpeggioRouting()
{
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        const bool driven = arpeggioDrives (part);
        if (driven == arpeggioDriven_[static_cast<std::size_t> (index)])
            continue;
        handleArpeggioRouting (part, driven);
        arpeggioDriven_[static_cast<std::size_t> (index)] = driven;
    }
}

void Engine::handleArpeggioRouting (Part part, bool nowDriven)
{
    const int index = part == Part::Upper ? 0 : 1;
    auto& runtime = toneRuntime (part);
    auto& arpeggio = arpeggios_[static_cast<std::size_t> (index)];

    std::array<int, 16> notes {}, velocities {};
    int count = 0;

    if (nowDriven)
    {
        // The keys already down become the chord, and the voices they
        // started stop: one key cannot be playing both ways at once.
        count = std::min (runtime.heldCount, static_cast<int> (notes.size()));
        for (int i = 0; i < count; ++i)
        {
            notes[static_cast<std::size_t> (i)] =
                runtime.heldNotes[static_cast<std::size_t> (i)];
            velocities[static_cast<std::size_t> (i)] =
                runtime.heldVelocities[static_cast<std::size_t> (i)];
        }
        for (int i = 0; i < count; ++i)
            releaseNoteForPart (part, notes[static_cast<std::size_t> (i)]);
        for (int i = 0; i < count; ++i)
            arpeggioAddKey (part, notes[static_cast<std::size_t> (i)],
                            velocities[static_cast<std::size_t> (i)]);
        return;
    }

    // No longer driven: the arpeggiator's own notes stop, and the keys still
    // held start sounding the way they would have without it.
    count = std::min (arpeggio.physicalCount, static_cast<int> (notes.size()));
    for (int i = 0; i < count; ++i)
    {
        notes[static_cast<std::size_t> (i)] =
            arpeggio.physicalKeys[static_cast<std::size_t> (i)];
        velocities[static_cast<std::size_t> (i)] =
            arpeggio.physicalVelocities[static_cast<std::size_t> (i)];
    }
    arpeggioStopPart (part);
    arpeggio.clearKeys();
    for (int i = 0; i < count; ++i)
        startNoteForPart (part, notes[static_cast<std::size_t> (i)],
                          velocities[static_cast<std::size_t> (i)]);
}

void Engine::startNoteForPart (Part part, int note, int velocity)
{
    if (! partSounds (part))
        return;

    const TonePatch& tone = tonePatch (part);
    ToneRuntime& runtime = toneRuntime (part);

    // A pitch played after the pedal went down was not sounding when it went
    // down, so sostenuto does not hold it. The latch is kept per pitch rather
    // than per voice, so that a mono voice borrowed by another key cannot
    // lose it - which means a genuinely new press of a caught pitch has to
    // clear it here, or the pedal would go on catching that pitch for as long
    // as it stayed down.
    if (sostenuto_ && note >= 0 && note <= 127)
        runtime.sostenutoNotes[static_cast<std::size_t> (note >> 6)] &=
            ~(1ull << (note & 63));

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
            else if (hold_ || runtime.sostenutoHolds (voice.note))
            {
                voice.held = true;
            }
            else
            {
                beginRelease (voice);
            }
        }
        return;
    }

    for (auto& voice : voices_)
    {
        if (! voice.active || voice.part != part || voice.note != note)
            continue;
        if (hold_ || runtime.sostenutoHolds (note))
        {
            voice.held = true;
            continue;
        }
        beginRelease (voice);
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
            // Longest *released*, which is not the same as oldest-triggered:
            // hold a bass note, play and release a melody note over it, then
            // release the bass, and the bass has the smaller trigger age
            // while the melody's tail has been decaying far longer. Ordering
            // by trigger age took the loudest surviving tail.
            if (voice.ampEnv.stage == Envelope::Stage::Release
                && (released == nullptr || voice.releaseAge < released->releaseAge))
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
        // Two released voices are ordered by how long ago they were let go;
        // two sounding ones by how long ago they were struck.
        if (voiceReleased ? voice.releaseAge < best->releaseAge
                          : voice.age < best->age)
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
            // The voice is freed on the amp envelope alone, so a filter
            // envelope whose release outlasts the amp's simply stops being
            // advanced and keeps its level. Re-armed here and nowhere else:
            // a *stolen* voice keeps its filter-envelope level deliberately
            // (research, "Envelopes"), and that is the `wasActive` case.
            voice.filterEnv.kill();
            voice.filterEnv.trigger();
            voice.filter1.clear();
            voice.filter2.clear();
            voice.shelfState = 0.0;
            // A fresh note starts *at* its filter type and slope. Crossing
            // into them from whatever the voice's previous owner used would
            // make a note's first five milliseconds depend on the note before
            // it. A stolen voice keeps its cross, like its filter states.
            voice.filterTypeMix.snapTo (static_cast<int> (tone.filterType));
            voice.filterSlopeFade = tone.filterSlope == FilterSlope::Db24 ? 1.0 : 0.0;
            voice.shelfDepth =
                tone.lowFreq == LowFreqMode::Flat
                    ? 0.0
                    : std::pow (10.0,
                                (tone.lowFreq == LowFreqMode::Boost
                                     ? mapping::lowShelfGainDb
                                     : -mapping::lowShelfGainDb)
                                    / 20.0)
                          - 1.0;
            // The overdrive stage belongs with them. A *stolen* voice keeps
            // its filter and its envelope level deliberately, and clearing
            // the stage's matched delay line under it emptied the line the
            // clean path reads from: the note it was still sounding went
            // silent for the whole of the reported latency before the new
            // one arrived.
            voice.overdrive.clear();
        }
    }
}

bool Engine::keyStillDown (const Voice& voice) noexcept
{
    const ToneRuntime& runtime = tones_[voice.part == Part::Upper ? 0 : 1];
    for (int i = 0; i < runtime.heldCount; ++i)
        if (runtime.heldNotes[static_cast<std::size_t> (i)] == voice.note)
            return true;
    return false;
}

// `releaseAge` records the moment a voice entered release, which is what lets
// the steal take the longest-decayed tail. A voice already in release keeps
// the stamp it got then: All Notes Off and a hold-pedal lift both sweep the
// whole voice array, and re-stamping the ones already decaying replaced their
// order with the order they happen to sit in the array — so the steal after
// an All Notes Off could take a fresh tail over a stale one, which is the
// defect the stamp was added to fix.
void Engine::beginRelease (Voice& voice) noexcept
{
    voice.held = false;
    if (voice.ampEnv.stage != Envelope::Stage::Release)
        voice.releaseAge = ++voiceClock_;
    voice.ampEnv.release();
    voice.filterEnv.release();
}

// A voice lets go only when nothing is still holding it: not the key, not the
// hold pedal, and not a sostenuto latch on the note it is playing.
void Engine::releaseIfNoPedalHolds (Voice& voice) noexcept
{
    if (! voice.active)
        return;
    const ToneRuntime& runtime = tones_[voice.part == Part::Upper ? 0 : 1];
    if (hold_ || runtime.sostenutoHolds (voice.note) || keyStillDown (voice))
        return;
    beginRelease (voice);
}

void Engine::setHold (bool down)
{
    if (down == hold_)
        return;
    hold_ = down;
    if (down)
        return;
    for (auto& voice : voices_)
        if (voice.held)
            releaseIfNoPedalHolds (voice);
}

// Settled (OM p. 72, part controller CC#66). The sostenuto pedal latches the
// notes sounding at the moment it goes down and holds only those: keys pressed
// afterwards play and release normally, which is the whole point of the pedal.
void Engine::setSostenuto (bool down)
{
    if (down == sostenuto_)
        return;
    sostenuto_ = down;
    if (down)
    {
        // Latch the notes whose keys are down right now, per tone.
        for (auto& runtime : tones_)
        {
            runtime.sostenutoNotes.fill (0ull);
            for (int i = 0; i < runtime.heldCount; ++i)
            {
                const int note = runtime.heldNotes[static_cast<std::size_t> (i)];
                if (note >= 0 && note <= 127)
                    runtime.sostenutoNotes[static_cast<std::size_t> (note >> 6)] |=
                        1ull << (note & 63);
            }
        }
        return;
    }
    for (auto& runtime : tones_)
        runtime.sostenutoNotes.fill (0ull);
    for (auto& voice : voices_)
        if (voice.held)
            releaseIfNoPedalHolds (voice);
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

// [settled] All Notes Off is every key coming up at once, not a panic: "When
// All Notes Off is received, all notes on the corresponding channel will be
// turned off. However, if Hold 1 or Sostenuto is ON, the sound will be
// continued until these are turned off" (MIDI Implementation v1.00 p. 1).
// This used to release every voice unconditionally and drop the sostenuto
// latch with them, which is All *Sounds* Off — a sustain pedal held down had
// the notes taken out from under it, and a sostenuto latch set before the
// message was gone even though its pedal was still down.
void Engine::allNotesOff()
{
    syncArpeggioRouting();

    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        auto& runtime = arpeggios_[static_cast<std::size_t> (index)];
        // One press at a time, exactly as the keys coming up would: the last
        // one leaving is what latches an ARPEGGIO HOLD chord and what stops
        // the arpeggiator.
        while (runtime.physicalCount > 0)
            arpeggioRemoveKey (part, runtime.physicalKeys[0]);
    }

    // The keys go first, so nothing below reads one as still down.
    for (auto& tone : tones_)
    {
        tone.heldCount = 0;
        tone.anyKeyDown = false;
    }

    // The sostenuto latch belongs to the pedal, not to the keys, and
    // outliving the keys that set it is its entire job. It is cleared when
    // the pedal comes up.
    for (auto& voice : voices_)
        if (voice.active)
            releaseIfNoPedalHolds (voice);
}

void Engine::allSoundOff()
{
    for (auto& voice : voices_)
    {
        voice.active = false;
        voice.held = false;
        voice.ampEnv.kill();
        voice.filterEnv.kill();
        voice.pitchEnv.active = false;
    }
    for (auto& tone : tones_)
    {
        tone.heldCount = 0;
        tone.anyKeyDown = false;
        tone.sostenutoNotes.fill (0ull);
    }
    for (auto& runtime : arpeggios_)
    {
        runtime.clearKeys();
        runtime.rows.fill (ArpeggioRuntime::Row {});
    }
    arpeggioRunning_ = false;
    arpeggioActive_ = patch_.arpeggio.on;
    // Synced to the patch for the same reason the switch is: clearing the
    // keyboard is not a routing change, and nothing is held across one, so
    // the next tick must not read one as having happened.
    for (int part = 0; part < partCount; ++part)
        arpeggioDriven_[static_cast<std::size_t> (part)] =
            arpeggioDrives (part == 0 ? Part::Upper : Part::Lower);
    arpeggioStep_ = 0;
    arpeggioGridSection_ = 0;
    arpeggioStepRemaining_ = 0.0;
    sostenuto_ = false;

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
    // [settled] CONTROLLER DESTINATION: each physical controller names the
    // tone or tones it reaches (OM p. 65). A voice the bend lever does not
    // reach does not bend, and one the modulation lever does not reach is not
    // modulated by it — the tone's own LFOs are untouched either way.
    const bool upperVoice = voice.part == Part::Upper;
    const double bendSemitones =
        (destinationReaches (patch_.pitchBendDestination, upperVoice)
             ? pitchBend_ * tone.bendRange
             : 0.0);
    const double lever =
        destinationReaches (patch_.modulationDestination, upperVoice)
            ? modulation_
            : 0.0;

    // Modulation-lever vibrato rides LFO2 (settled) into the assigned target.
    const double leverVibratoCents =
        lever * mapping::leverVibratoCents * runtime.lfo2Value;
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
            value += lever * mapping::leverPulseWidth * runtime.lfo2Value;
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
    const bool wasPrimed = voice.controlsPrimed;
    const double filterEnvLevel = voice.filterEnv.advance (tickSamples);
    double lfoFilterOct = 0.0;
    if (tone.lfo1.destination1 == LfoDest1::Filter)
        lfoFilterOct += mapping::lfoFilterOctaves (tone.lfo1.depth1) * runtime.lfo1Value;
    if (tone.lfo2.destination1 == LfoDest1::Filter)
        lfoFilterOct += mapping::lfoFilterOctaves (tone.lfo2.depth1) * runtime.lfo2Value;
    if (patch_.modulationAssign == ModulationAssign::Filter)
        lfoFilterOct += lever * mapping::leverFilterOctaves * runtime.lfo2Value;

    const double cutoffBaseOct = std::log2 (mapping::cutoffHz (tone.cutoff));
    const double keyTrack = mapping::keyFollowOctavesPerOctave (tone.keyFollow)
                            * (voice.glidePitch - 60.0) / 12.0;
    const double velocityOct = mapping::cutoffVelocityOctaves (
        tone.cutoffVelocitySens, voice.velocity);

    // Everything the panel can step goes through the slew: the cutoff knob,
    // key follow (which only moves at portamento speed), the velocity offset
    // and the LFO — an S&H LFO's edge is meant to stay audible, and the slew
    // is what keeps it from being a discontinuity in the coefficient.
    const double cutoffParamOctTarget =
        cutoffBaseOct + keyTrack + velocityOct + lfoFilterOct;
    // The envelope's *depth* is a knob and is slewed with the rest; the
    // envelope's own level is not, so the segment times the sliders ask for
    // are the segment times the filter gets.
    const double filterEnvOctTarget = mapping::filterEnvOctaves (tone.filterEnvDepth);
    const double resonanceTarget = mapping::resonanceDamping (tone.resonance);
    if (! voice.controlsPrimed)
    {
        voice.cutoffParamOctSlewed = cutoffParamOctTarget;
        voice.filterEnvOctSlewed = filterEnvOctTarget;
        voice.resonanceSlewed = resonanceTarget;
        voice.controlsPrimed = true;
    }
    else
    {
        const double slew =
            1.0 - std::exp (-tickSamples / (sampleRate_ * mapping::controlSlewSeconds));
        voice.cutoffParamOctSlewed +=
            (cutoffParamOctTarget - voice.cutoffParamOctSlewed) * slew;
        voice.filterEnvOctSlewed +=
            (filterEnvOctTarget - voice.filterEnvOctSlewed) * slew;
        voice.resonanceSlewed += (resonanceTarget - voice.resonanceSlewed) * slew;
    }
    const double fc = std::clamp (
        std::exp2 (voice.cutoffParamOctSlewed + filterEnvLevel * voice.filterEnvOctSlewed),
        5.0, 0.45 * sampleRate_);
    voice.filterGTarget = std::tan (pi * fc / sampleRate_);
    voice.filterKTarget = voice.resonanceSlewed;
    if (! wasPrimed)
    {
        // A fresh note starts *at* its coefficient rather than ramping to it
        // from whatever the previous owner of this voice left behind.
        voice.filterG = voice.filterGTarget;
        voice.filterK = voice.filterKTarget;
    }

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
        tremolo += lever * mapping::leverAmpDepth * runtime.lfo2Value;
    gain *= std::max (0.0, 1.0 + tremolo);

    // Equal-power pan around the *documented* centre. PAN is L64..63R, so its
    // printed centre is 0 — not the midpoint of an asymmetric range, which is
    // what (pan + 64) / 127 made it: PAN 0 came out 0.107 dB left of centre,
    // on every INIT patch and every preset that leaves PAN alone, while the
    // received CC#10 pan in the same engine already put its own centre where
    // the map says it is.
    const double panSigned = tone.pan <= 0 ? tone.pan / 64.0 : tone.pan / 63.0;
    const double panAngle = (pi * 0.25) * (1.0 + panSigned);
    // The panel side of the amp gain goes through the same slew the panel
    // side of the cutoff does, and for the same reason: LEVEL, the velocity
    // offset, PAN and an LFO on the AMP destination all step, and a step in a
    // gain is a click. The amp *envelope* is deliberately outside it — it is
    // applied per sample in renderVoiceTick — so the documented "fast ADSR
    // response" is untouched, exactly as Step 1 left it for the filter.
    const double gainLWanted = gain * std::cos (panAngle);
    const double gainRWanted = gain * std::sin (panAngle);
    if (! wasPrimed)
    {
        // A fresh note starts at its own gain rather than ramping to it from
        // whatever the previous owner of this voice left behind.
        voice.ampGainLSlewed = gainLWanted;
        voice.ampGainRSlewed = gainRWanted;
        voice.ampGainL = gainLWanted;
        voice.ampGainR = gainRWanted;
    }
    else
    {
        const double gainSlew =
            1.0 - std::exp (-tickSamples / (sampleRate_ * mapping::controlSlewSeconds));
        voice.ampGainLSlewed += (gainLWanted - voice.ampGainLSlewed) * gainSlew;
        voice.ampGainRSlewed += (gainRWanted - voice.ampGainRSlewed) * gainSlew;
    }
    voice.ampGainLTarget = voice.ampGainLSlewed;
    voice.ampGainRTarget = voice.ampGainRSlewed;
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
    //
    // `corrected` is false on the sample a hard sync forced the phase on. The
    // residuals describe the discontinuity a *free-running* oscillator makes
    // — a whole cycle's worth — and a sync reset jumps by whatever fraction
    // of a cycle the oscillator happened to have reached. Correcting that
    // jump as though it were a full one puts a spike in where the reset
    // belongs, and the reset is documented as naive anyway.
    inline OscOutput renderClassicWave (Waveform wave, double& phase, double inc,
                                        double duty, std::uint32_t& noiseRng,
                                        bool corrected = true) noexcept
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
                if (corrected)
                    value -= polyBlep (phase, inc);
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Square:
            case Waveform::PulseSquare:
            {
                const double width = wave == Waveform::Square ? 0.5 : duty;
                double value = phase < width ? 1.0 : -1.0;
                if (corrected)
                {
                    value += polyBlep (phase, inc);
                    value -= polyBlep (frac (phase - width + 1.0), inc);
                }
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Triangle:
            {
                double value = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
                if (! corrected)
                    return { value, wrapped, wrapOffset };
                // polyBlamp is the antiderivative of polyBlep with respect to
                // sample time, and polyBlep already carries a step of two (it
                // corrects the saw's -2 wrap on its own). So a corner whose
                // slope changes by 8*inc per sample needs half of that as its
                // coefficient, not all of it: at 8*inc the correction
                // overshoots by exactly as much as it corrects and the
                // triangle measures the same as no correction at all.
                const double scale = 4.0 * inc;
                value += scale * polyBlamp (phase, inc);
                value -= scale * polyBlamp (frac (phase + 0.5), inc);
                return { value, wrapped, wrapOffset };
            }
            case Waveform::Sine:
                return { std::sin (twoPi * phase), wrapped, wrapOffset };
            case Waveform::Noise:
            {
                // [voiced, OQ-03] White. The contract's position until a
                // spectral capture of a real unit's NOISE closes OQ-03 is
                // that the wave is white, and this generator is flat to
                // within a fraction of a decibel across the band at every
                // host rate.
                //
                // It replaced a 23-bit Galois LFSR whose *state* was read as
                // the sample: successive states of a Galois LFSR are not
                // independent (v(n+1) = 0.5 v(n) + 0.5 b(n)), so that wave
                // was a one-pole-filtered bit stream — 10.9 dB darker across
                // the band at 44.1 kHz and 1.1 dB at 192 kHz, which made the
                // timbre a property of the user's interface rather than of
                // the instrument. Its comment also named a Roland polynomial
                // no document in the contract's source list settles.
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

void Engine::renderVoiceTick (Voice& voice, float* mono, int samples,
                              const float* external)
{
    const TonePatch& tone = tonePatch (voice.part);
    const Waveform wave1 = tone.osc1.wave;
    const Waveform wave2 = tone.osc2.wave;

    const double legGain1 = mapping::balanceLegGain (tone.balance, true);
    const double legGain2 = mapping::balanceLegGain (tone.balance, false);

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
        const double x = sum * mapping::superSawStackNormalisation;
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
    const double fbLoopDamping = mapping::fbOscLoopDampingCoeff (sampleRate_);
    const auto feedbackOsc = [fbLoopDamping] (OscState& osc, double inc,
                                              double fbGain)
    {
        osc.phase = frac (osc.phase + inc);
        const double saw = 2.0 * osc.phase - 1.0;
        const double periodSamples = 1.0 / std::max (1.0e-6, inc);
        const int size = static_cast<int> (osc.comb.size());
        const double delay =
            std::clamp (periodSamples * mapping::fbOscDelayRatio, 2.0,
                        static_cast<double> (size - 4));
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
        osc.combState += fbLoopDamping * (out - osc.combState);
        osc.comb[static_cast<std::size_t> (osc.combWrite)] =
            static_cast<float> (softClip (osc.combState) * mapping::fbOscLoopTrim);
        osc.combWrite = (osc.combWrite + 1) % size;
        return out * mapping::fbOscOutputGain;
    };

    // [voiced, OQ-11] The drive curve, hoisted out of the sample loop.
    const double overdrivePreGain = mapping::overdrivePreGain (tone.drive);
    const double overdriveCompensation = mapping::overdriveCompensation (overdrivePreGain);

    // Per-sample walk from this tick's starting coefficients to the ones the
    // control update just computed.
    const double inverseSamples = 1.0 / std::max (1, samples);
    const double gStep = (voice.filterGTarget - voice.filterG) * inverseSamples;
    const double kStep = (voice.filterKTarget - voice.filterK) * inverseSamples;
    // How far a crossed switch moves per sample, shared with the external
    // input's switches: the same registered constant, the same meaning.
    const double fadeStep =
        1.0 / std::max (1.0, mapping::externalSwitchFadeSeconds * sampleRate_);

    // LOW FREQ's one-pole and its target depth: both depend only on the patch
    // and the sample rate, and were being recomputed — `std::pow` included —
    // once per sample per voice.
    const double shelfA = twoPi * mapping::lowShelfHz / sampleRate_;
    const double shelfCoeff = shelfA / (1.0 + shelfA);
    const double shelfDepthTarget =
        tone.lowFreq == LowFreqMode::Flat
            ? 0.0
            : std::pow (10.0,
                        (tone.lowFreq == LowFreqMode::Boost
                             ? mapping::lowShelfGainDb
                             : -mapping::lowShelfGainDb)
                            / 20.0)
                  - 1.0;

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
            {
                // Settled: the input jacks' signal, in mono, in place of a
                // generated wave. The oscillator keeps running underneath it
                // so SYNC still has a cycle to fire from — the substitution
                // is of the output, not of the oscillator.
                const double phaseBefore = voice.osc2.phase;
                sample2 = external[i];
                voice.osc2.phase = frac (voice.osc2.phase + voice.inc2);
                if (phaseBefore + voice.inc2 >= 1.0)
                {
                    osc2Wrapped = true;
                    osc2WrapOffset = (phaseBefore + voice.inc2 - 1.0)
                                     / std::max (1.0e-9, voice.inc2);
                }
                break;
            }
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
        bool osc1SyncReset = false;
        if (tone.mixType == MixModType::Sync && osc2Wrapped
            && wave1 != Waveform::Noise && wave1 != Waveform::SuperSaw
            && wave1 != Waveform::FbOsc && wave1 != Waveform::ExtIn)
        {
            double newPhase = osc2WrapOffset * voice.inc1 - voice.inc1;
            while (newPhase < 0.0)
                newPhase += 1.0;
            voice.osc1.phase = newPhase;
            osc1SyncReset = true;
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
                sample1 = external[i];
                voice.osc1.phase = frac (voice.osc1.phase + voice.inc1);
                break;
            default:
            {
                const auto out = renderClassicWave (wave1, voice.osc1.phase,
                                                    voice.inc1, voice.duty1,
                                                    voice.noiseRng,
                                                    ! osc1SyncReset);
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

        // LOW FREQ shelf (voiced 200 Hz, +/-8 dB). The one-pole runs at every
        // position, FLAT included, where its output is simply not added: the
        // block used to be skipped for FLAT, which froze the state and let a
        // stale one back in when a non-FLAT position returned. LOW FREQ is
        // automatable and its three positions differ sample by sample, so the
        // shelf's own contribution is crossed rather than thrown, like the
        // filter's TYPE above.
        voice.shelfState += shelfCoeff * (mixed - voice.shelfState);
        voice.shelfDepth +=
            std::clamp (shelfDepthTarget - voice.shelfDepth, -fadeStep * 2.0,
                        fadeStep * 2.0);
        mixed += voice.shelfDepth * voice.shelfState;

        // ---- FILTER ------------------------------------------------------
        // Both of the filter's switches are crossed rather than thrown, for
        // the reason Step 11 established for the external-input path: TYPE and
        // SLOPE are automatable and each chooses between signals whose
        // instantaneous samples differ, so changing one on a sustaining note
        // steps the output however warm the unused side is kept. Thrown, the
        // four transitions jumped 40 to 137 times the signal's own steady
        // sample-to-sample travel.
        //
        // The stages therefore run unconditionally, BYPASS included: freezing
        // the integrators while bypassed would let an old resonant tail out of
        // them the moment the switch came back, which is the same reasoning the
        // audio filter already carries.
        double filtered;
        {
            // Walk the coefficients across the tick instead of stepping them
            // at its edge: the filter envelope now moves at full speed, and a
            // fast sweep must not arrive as an eight-sample staircase.
            const double g = voice.filterG + gStep * (i + 1);
            const double k = voice.filterK + kStep * (i + 1);
            const double a1 = 1.0 / (1.0 + g * (g + k));
            const double a2 = g * a1;
            // Second, non-resonant 2-pole stage (voiced topology).
            const double k2 = mapping::filterSecondStageDamping;
            const double b1 = 1.0 / (1.0 + g * (g + k2));
            const double b2 = g * b1;

            const auto walk = [fadeStep] (double& fade, double target)
            {
                fade += std::clamp (target - fade, -fadeStep, fadeStep);
            };
            walk (voice.filterSlopeFade,
                  tone.filterSlope == FilterSlope::Db24 ? 1.0 : 0.0);
            // TYPE has four positions rather than two, so it crosses over a
            // weight per position: a second change part way through the first
            // one has to stay continuous, and one outgoing signal cannot
            // carry a mixture of two.
            voice.filterTypeMix.advance (static_cast<int> (tone.filterType), fadeStep);

            // One stage, advanced once, with all four responses read off the
            // same two integrators, so crossing between the outgoing type and
            // the incoming one costs a select rather than a second filter.
            const auto stagePass = [&] (SvfStage& stage, double input,
                                        double damping, double stageA1,
                                        double stageA2)
            {
                const double v3 = input - stage.ic2eq;
                const double v1 = stageA1 * stage.ic1eq + stageA2 * v3;
                const double v2 = stage.ic2eq + g * v1;
                stage.ic1eq = 2.0 * v1 - stage.ic1eq;
                stage.ic2eq = 2.0 * v2 - stage.ic2eq;
                // Stage limiter: bounds self-oscillation growth (the manual's
                // "may not stop at all" is a bounded oscillation on hardware).
                // Continuous soft knee, so limiting never steps the state.
                const auto limitState = [] (double state)
                {
                    const double a = std::abs (state);
                    if (a <= mapping::filterStateLimit)
                        return state;
                    const double over = a - mapping::filterStateLimit;
                    const double limited =
                        mapping::filterStateLimit + over / (1.0 + over);
                    return state < 0.0 ? -limited : limited;
                };
                // Only where the stage is linearly unstable. `damping` is the
                // state-variable k, and k <= 0 is the oscillation threshold —
                // the stability boundary itself, not a new constant. Applied
                // at every resonance the limiter was a full-time waveshaper:
                // two oscillators at unity put more than +/-1.5 into the
                // filter, so an ordinary patch measured -27 dB THD at
                // RESONANCE 0, which nothing about the instrument says it
                // should do.
                if (damping <= 0.0)
                {
                    stage.ic1eq = limitState (stage.ic1eq);
                    stage.ic2eq = limitState (stage.ic2eq);
                }
                const double lp = v2;
                const double bp = v1;
                const double hp = input - damping * v1 - v2;
                const auto response = [&] (FilterType type)
                {
                    switch (type)
                    {
                        // Each response is the integrator tap itself. The
                        // band-pass used to be scaled by the damping, which is
                        // the usual way to hold its peak at unity — but this
                        // damping is the one RESONANCE drives to zero and past
                        // it, so scaling by it made the band-pass quieter as
                        // the knob came up and inverted it at the top, where
                        // the manual says the filter oscillates. Taken raw it
                        // gains with resonance and self-oscillates like the
                        // other two.
                        case FilterType::Lpf: return lp;
                        case FilterType::Hpf: return hp;
                        case FilterType::Bpf: return bp;
                        case FilterType::Bypass: break;
                    }
                    return input;
                };
                return voice.filterTypeMix.mix (
                    [&] (int type) { return response (static_cast<FilterType> (type)); });
            };

            const double twoPole = stagePass (voice.filter1, mixed, k, a1, a2);
            const double fourPole = stagePass (voice.filter2, twoPole, k2, b1, b2);
            filtered = twoPole + (fourPole - twoPole) * voice.filterSlopeFade;
        }

        // ---- AMP: overdrive, envelope, level, pan ------------------------
        // The stage runs for every voice, shaping or not: its group delay is
        // the same either way, so a clean tone layered under an overdriven
        // one stays in phase with it.
        filtered = voice.overdrive.process (filtered, overdrivePreGain,
                                            overdriveCompensation, tone.overdrive);

        const double env = voice.ampEnv.advance (1);
        mono[i] = static_cast<float> (filtered * env);
    }

    voice.filterG = voice.filterGTarget;
    voice.filterK = voice.filterKTarget;

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
// Arpeggiator
//
// Settled (OM pp. 22-23, 66-67): the arpeggiator plays an arpeggio style — a
// grid of up to 32 steps by 16 note rows whose cells are note-on (with a
// velocity), tie or rest — against the keys held down, at the patch tempo,
// through a GRID division with optional shuffle, a DURATION, a MOTIF, an
// OCTAVE RANGE that shifts one cycle at a time, an ACCENT that blends the
// style's programmed velocities toward a flat one, an ARPEGGIO VELOCITY that
// is either what you played or a fixed value, an END STEP, HOLD, and a SPLIT
// ARPEGGIO switch choosing which tone(s) it drives in SPLIT mode. The style
// records "the position of each key you play relative to the lowest-pitched
// key you played".
//
// The MOTIF mapping below is not inferred: the manual works three examples of
// the style "1-2-3-2" against the keys C-D-E-F-G, and this implementation
// reproduces all three exactly. A test holds it to them.
// ---------------------------------------------------------------------------

bool Engine::arpeggioDrives (Part part) const noexcept
{
    if (! patch_.arpeggio.on || ! partSounds (part))
        return false;
    if (patch_.keyboardMode != KeyboardMode::Split)
        return true;
    switch (patch_.arpeggio.splitArpeggio)
    {
        case SplitArpeggio::Upper: return part == Part::Upper;
        case SplitArpeggio::Lower: return part == Part::Lower;
        case SplitArpeggio::Both:  return true;
    }
    return true;
}

void Engine::arpeggioAddKey (Part part, int note, int velocity)
{
    auto& runtime = arpeggios_[part == Part::Upper ? 0 : 1];

    // The physical key list is kept whatever HOLD is doing: it is what says
    // when the player has actually let go of everything.
    bool alreadyDown = false;
    for (int i = 0; i < runtime.physicalCount; ++i)
        if (runtime.physicalKeys[static_cast<std::size_t> (i)] == note)
        {
            ++runtime.physicalPresses[static_cast<std::size_t> (i)];
            runtime.physicalVelocities[static_cast<std::size_t> (i)] = velocity;
            alreadyDown = true;
            break;
        }
    if (! alreadyDown
        && runtime.physicalCount < static_cast<int> (runtime.physicalKeys.size()))
    {
        const auto slot = static_cast<std::size_t> (runtime.physicalCount++);
        runtime.physicalKeys[slot] = note;
        runtime.physicalVelocities[slot] = velocity;
        runtime.physicalPresses[slot] = 1;
    }
    else if (alreadyDown)
    {
        // A second press of a pitch already in the chord does not change the
        // chord, but it does change how hard that chord tone was played, and
        // ARPEGGIO VELOCITY = REAL means "the velocity of the key this note
        // actually came from". The array the arpeggiator reads is
        // `velocities`; updating only `physicalVelocities` left the tone
        // sounding at the first press's dynamics for as long as it was held.
        for (int i = 0; i < runtime.keyCount; ++i)
            if (runtime.keys[static_cast<std::size_t> (i)] == note)
            {
                runtime.velocities[static_cast<std::size_t> (i)] = velocity;
                break;
            }
        runtime.lastPressed = note;
        runtime.lastVelocity = velocity;
        return;
    }

    // HOLD: once every key has been let go, the next key starts a new chord
    // rather than joining the one still playing ("when you play a new chord,
    // the arpeggio will also change").
    if (runtime.latched)
    {
        runtime.keyCount = 0;
        runtime.latched = false;
    }
    for (int i = 0; i < runtime.keyCount; ++i)
        if (runtime.keys[static_cast<std::size_t> (i)] == note)
            return;
    if (runtime.keyCount >= static_cast<int> (runtime.keys.size()))
        return;

    int position = runtime.keyCount;
    while (position > 0 && runtime.keys[static_cast<std::size_t> (position - 1)] > note)
    {
        runtime.keys[static_cast<std::size_t> (position)] =
            runtime.keys[static_cast<std::size_t> (position - 1)];
        runtime.velocities[static_cast<std::size_t> (position)] =
            runtime.velocities[static_cast<std::size_t> (position - 1)];
        --position;
    }
    runtime.keys[static_cast<std::size_t> (position)] = note;
    runtime.velocities[static_cast<std::size_t> (position)] = velocity;
    ++runtime.keyCount;
    runtime.lastPressed = note;
    runtime.lastVelocity = velocity;
}

void Engine::arpeggioRemoveKey (Part part, int note)
{
    auto& runtime = arpeggios_[part == Part::Upper ? 0 : 1];

    // Only the *last* release of a pitch lets go of it, so two overlapping
    // notes of the same pitch cannot have the first one's note-off take what
    // the second is still holding.
    bool stillHeld = false;
    for (int i = 0; i < runtime.physicalCount; ++i)
    {
        if (runtime.physicalKeys[static_cast<std::size_t> (i)] != note)
            continue;
        if (--runtime.physicalPresses[static_cast<std::size_t> (i)] > 0)
        {
            stillHeld = true;
            break;
        }
        for (int j = i; j + 1 < runtime.physicalCount; ++j)
        {
            runtime.physicalKeys[static_cast<std::size_t> (j)] =
                runtime.physicalKeys[static_cast<std::size_t> (j + 1)];
            runtime.physicalVelocities[static_cast<std::size_t> (j)] =
                runtime.physicalVelocities[static_cast<std::size_t> (j + 1)];
            runtime.physicalPresses[static_cast<std::size_t> (j)] =
                runtime.physicalPresses[static_cast<std::size_t> (j + 1)];
        }
        --runtime.physicalCount;
        break;
    }
    if (stillHeld)
        return;

    if (patch_.arpeggio.hold)
    {
        // The chord stays until the *last* key comes up; only then does the
        // next press start a new one. Latching on the first release would drop
        // the keys still under the player's fingers.
        if (runtime.keyCount > 0 && runtime.physicalCount == 0)
            runtime.latched = true;
        return;
    }
    for (int i = 0; i < runtime.keyCount; ++i)
    {
        if (runtime.keys[static_cast<std::size_t> (i)] != note)
            continue;
        for (int j = i; j + 1 < runtime.keyCount; ++j)
        {
            runtime.keys[static_cast<std::size_t> (j)] =
                runtime.keys[static_cast<std::size_t> (j + 1)];
            runtime.velocities[static_cast<std::size_t> (j)] =
                runtime.velocities[static_cast<std::size_t> (j + 1)];
        }
        --runtime.keyCount;
        break;
    }

    // The re-arm that makes the next chord start on step one lives in
    // advanceArpeggiator, which only sees the chord empty if audio is
    // rendered while it is. One chord replaced by the next at the very same
    // sample position - the ordinary layout for adjacent chords in a
    // sequence - leaves no samples in between, so the gap was never
    // observed and the new chord picked the pattern up wherever the old one
    // had left it. The last key leaving is noticed here instead.
    bool anyKeysLeft = false;
    for (const auto& other : arpeggios_)
        if (other.keyCount > 0)
            anyKeysLeft = true;
    if (! anyKeysLeft && arpeggioRunning_)
    {
        for (int index = 0; index < partCount; ++index)
            arpeggioStopPart (index == 0 ? Part::Upper : Part::Lower);
        arpeggioRunning_ = false;
        arpeggioStep_ = 0;
        arpeggioGridSection_ = 0;
        arpeggioStepRemaining_ = 0.0;
    }
}

void Engine::arpeggioStopPart (Part part)
{
    auto& runtime = arpeggios_[part == Part::Upper ? 0 : 1];
    for (auto& row : runtime.rows)
    {
        if (row.note >= 0)
            releaseNoteForPart (part, row.note);
        if (row.tailNote >= 0)
            releaseNoteForPart (part, row.tailNote);
        row = ArpeggioRuntime::Row {};
    }
    runtime.cycle = 0;
    runtime.windowCycle = 0;
}

void Engine::arpeggioFireStepForPart (Part part, double stepSeconds)
{
    auto& runtime = arpeggios_[part == Part::Upper ? 0 : 1];
    const ArpeggioParams& arp = patch_.arpeggio;
    const ArpeggioStyle& style = arp.style;
    const int endStep = std::clamp (style.endStep, 1, arpeggioMaxSteps);
    const int span = style.rowSpan();

    // OCTAVE RANGE "shifts arpeggios one cycle at a time in octave units":
    // the shift walks 0..|range| and starts over (voiced cycle order, OQ-15).
    const int range = std::abs (arp.octaveRange);
    const int octave = range == 0
                           ? 0
                           : (arp.octaveRange < 0 ? -1 : 1)
                                 * (runtime.cycle % (range + 1));
    // The motif reads its own window, which a RANDOM motif redraws each pass
    // while the octave keeps walking its documented 0..|range| sequence.
    const int window = runtime.windowCycle;

    const double durationFraction = mapping::arpeggioDurationFraction (arp.duration);
    const bool sustained = arp.duration == ArpeggioDuration::Full;

    for (int row = 0; row < arpeggioMaxRows; ++row)
    {
        const signed char cell = style.cell (arpeggioStep_, row);
        auto& state = runtime.rows[static_cast<std::size_t> (row)];

        if (cell == arpeggioTie)
        {
            // Nothing to do: the note-on that started this chain already
            // measured the whole chain, ties included. Adding a step here as
            // well would count every tie twice.
            continue;
        }
        if (cell == arpeggioRest)
            continue;

        // A note-on holds for the grids it is tied across plus DURATION of the
        // final one. Which step the fraction is measured against matters on a
        // shuffled grid, where the pair is uneven: every step of the chain but
        // the last contributes its whole length, and DURATION is taken from
        // the last step's length. Measuring it against the step the chain
        // started on instead swaps the two, so a chain that begins on a Heavy
        // sixteenth and ties into the following Light one ended a fifth of a
        // beat early, and one starting on the Light step ended equally late.
        double heldSeconds = 0.0;
        double lastStepSeconds = stepSeconds;
        for (int ahead = 1; ahead < endStep; ++ahead)
        {
            const int step = (arpeggioStep_ + ahead) % endStep;
            if (style.cell (step, row) != arpeggioTie)
                break;
            heldSeconds += lastStepSeconds;
            // The grid section the chain reaches, not the pattern step: the
            // shuffle's parity runs with the beat.
            lastStepSeconds = mapping::arpeggioStepSeconds (
                patch_.tempo, arp.grid, arpeggioGridSection_ + ahead);
        }

        if (state.note >= 0)
        {
            if (! state.sustained && state.remaining > 0)
            {
                // The gate runs past its grid (DURATION 120 %): let it finish
                // over the top of the note starting here, which is the whole
                // point of a duration above 100 %.
                if (state.tailNote >= 0)
                    releaseNoteForPart (part, state.tailNote);
                state.tailNote = state.note;
                state.tailRemaining = state.remaining;
            }
            else
            {
                releaseNoteForPart (part, state.note);
            }
        }

        const int keyIndex = mapping::arpeggioKeyIndexForRow (
            arp.motif, runtime.keyCount, row + 1, span, window);
        const int key = mapping::arpeggioKeyForRow (
            arp.motif, runtime.keys.data(), runtime.keyCount, runtime.lastPressed,
            row + 1, span, window);
        if (key < 0)
        {
            state.note = -1;
            state.remaining = 0;
            state.sustained = false;
            continue;
        }
        // The octave cycle is a pitch offset, not a MIDI note. Clamping it
        // into the MIDI range collapses cycles onto each other at the ends of
        // the keyboard - note 120 with OCTAVE RANGE +2 played 120, 127, 127
        // where the pattern owes 120, 132, 144, so the arpeggio simply stopped
        // moving. Pitch is carried through this engine as a number of
        // semitones and already leaves 0-127 by way of the octave shift and
        // transpose; the oscillator increment is capped at Nyquist, and the
        // held key it is measured from is itself a real MIDI note.
        const int note = key + 12 * octave;

        // Any other claim on the pitch about to start - this row's own gate
        // from the previous grid, or another row still sounding it - would end
        // the note starting here when it expires, because voices are released
        // by pitch and nothing downstream can tell the two apart. A plain run
        // walks one held key across every row, so a single key repeats its
        // pitch on every step and this is the ordinary case, not a corner:
        // left alone it silenced a 100 % pattern outright and cut every
        // 120 % gate back to its own grid. The pitch is handed over here
        // instead, which is all a repeated pitch can do.
        for (int other = 0; other < arpeggioMaxRows; ++other)
        {
            auto& claim = runtime.rows[static_cast<std::size_t> (other)];
            if (other != row && claim.note == note)
            {
                releaseNoteForPart (part, claim.note);
                claim.note = -1;
                claim.remaining = 0;
                claim.sustained = false;
            }
            if (claim.tailNote == note)
            {
                releaseNoteForPart (part, claim.tailNote);
                claim.tailNote = -1;
                claim.tailRemaining = 0;
            }
        }

        // ARPEGGIO VELOCITY chooses what "how hard you played" means: REAL is
        // the velocity of the key this note actually came from, so a chord
        // played unevenly stays uneven. ACCENT then blends the style's
        // programmed pattern in on top of it (voiced blend, OQ-15).
        const double played =
            arp.velocity != 0
                ? (double) arp.velocity
                : (double) (keyIndex >= 0
                                ? runtime.velocities[static_cast<std::size_t> (keyIndex)]
                                : runtime.lastVelocity);
        const double blend = arp.accent / 100.0;
        const double patterned =
            played * ((1.0 - blend)
                      + blend * (cell / mapping::arpeggioCellReferenceVelocity));
        const int velocity = clampRaw ((int) std::lround (patterned), 1, 127);

        startNoteForPart (part, note, velocity);
        // `tailNote` deliberately survives: a 120 % gate from the previous
        // grid is still running underneath this one.
        state.note = note;
        state.sustained = sustained;
        state.remaining =
            sustained ? 0
                      : static_cast<int> ((heldSeconds + lastStepSeconds * durationFraction)
                                          * sampleRate_);
    }
}

void Engine::arpeggioFireStep()
{
    const double stepSeconds = mapping::arpeggioStepSeconds (
        patch_.tempo, patch_.arpeggio.grid, arpeggioGridSection_);
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        if (! arpeggioDrives (part))
            continue;
        if (arpeggios_[static_cast<std::size_t> (index)].keyCount <= 0)
            continue;
        arpeggioFireStepForPart (part, stepSeconds);
    }
}

void Engine::advanceArpeggiator (int samples)
{
    const ArpeggioParams& arp = patch_.arpeggio;

    syncArpeggioRouting();

    if (arp.on != arpeggioActive_)
    {
        arpeggioActive_ = arp.on;
        if (! arp.on)
        {
            arpeggioRunning_ = false;
            arpeggioStep_ = 0;
            arpeggioGridSection_ = 0;
            arpeggioStepRemaining_ = 0.0;
        }
    }

    if (! arp.on)
        return;
    // HOLD switched off with nothing held: the latched chord stops.
    if (! arp.hold)
        for (auto& runtime : arpeggios_)
            if (runtime.latched)
            {
                runtime.keyCount = 0;
                runtime.latched = false;
            }

    // Which rows the current style ever plays. A row outside that set has
    // nothing left to end it, so a FUL note left on it by a previous style
    // would sustain for as long as the chord was held.
    std::uint32_t rowsInUse = 0u;
    {
        const ArpeggioStyle& style = arp.style;
        const int endStep = std::clamp (style.endStep, 1, arpeggioMaxSteps);
        for (int step = 0; step < endStep; ++step)
            for (int row = 0; row < arpeggioMaxRows; ++row)
                if (style.cell (step, row) != arpeggioRest)
                    rowsInUse |= 1u << row;
    }

    // Scheduled note-offs, at control-tick resolution like everything else.
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        auto& runtime = arpeggios_[static_cast<std::size_t> (index)];
        for (int row = 0; row < arpeggioMaxRows; ++row)
        {
            auto& state = runtime.rows[static_cast<std::size_t> (row)];
            if (state.tailNote >= 0)
            {
                state.tailRemaining -= samples;
                if (state.tailRemaining <= 0)
                {
                    releaseNoteForPart (part, state.tailNote);
                    state.tailNote = -1;
                    state.tailRemaining = 0;
                }
            }
            if (state.note < 0)
                continue;
            if (state.sustained)
            {
                if ((rowsInUse & (1u << row)) == 0u)
                {
                    // The style changed under a held FUL note and this row is
                    // no longer played: nothing would ever end it.
                    releaseNoteForPart (part, state.note);
                    state.note = -1;
                    state.sustained = false;
                }
                continue;
            }
            state.remaining -= samples;
            if (state.remaining <= 0)
            {
                releaseNoteForPart (part, state.note);
                state.note = -1;
                state.remaining = 0;
            }
        }
    }

    // SPLIT ARPEGGIO, the keyboard mode and the keyboard part are all
    // automatable, so a part can stop being one the arpeggiator drives while
    // its notes are sounding. Those notes are the arpeggiator's and nothing
    // else will end them.
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        if (arpeggioDrives (part))
            continue;
        auto& runtime = arpeggios_[static_cast<std::size_t> (index)];
        bool sounding = false;
        for (const auto& row : runtime.rows)
            sounding = sounding || row.note >= 0 || row.tailNote >= 0;
        if (sounding)
            arpeggioStopPart (part);
    }

    bool anyKeys = false;
    for (int index = 0; index < partCount; ++index)
        if (arpeggioDrives (index == 0 ? Part::Upper : Part::Lower)
            && arpeggios_[static_cast<std::size_t> (index)].keyCount > 0)
            anyKeys = true;

    if (! anyKeys)
    {
        // Nothing held: stop the sounding notes and re-arm so the next chord
        // starts on its own first step rather than mid-pattern.
        if (arpeggioRunning_)
        {
            for (int index = 0; index < partCount; ++index)
                arpeggioStopPart (index == 0 ? Part::Upper : Part::Lower);
            arpeggioRunning_ = false;
        }
        arpeggioStep_ = 0;
        arpeggioGridSection_ = 0;
        arpeggioStepRemaining_ = 0.0;
        return;
    }

    if (! arpeggioRunning_)
    {
        arpeggioRunning_ = true;
        arpeggioStep_ = 0;
        arpeggioGridSection_ = 0;
        arpeggioStepRemaining_ = 0.0;
        for (auto& runtime : arpeggios_)
        {
            runtime.cycle = 0;
            runtime.windowCycle = 0;
        }
    }

    double left = samples;
    while (left > 0.0)
    {
        if (arpeggioStepRemaining_ <= 0.0)
        {
            const int endStep = std::clamp (arp.style.endStep, 1, arpeggioMaxSteps);
            // ARPEGGIO STYLE and END STEP are both automatable, so the counter
            // can be left past the end of a pattern that just got shorter.
            // Normalise before firing, or the switch spends one grid on a cell
            // the new style does not use and then resumes from the wrong place.
            arpeggioStep_ %= endStep;
            arpeggioFireStep();
            arpeggioStepRemaining_ =
                mapping::arpeggioStepSeconds (patch_.tempo, arp.grid,
                                              arpeggioGridSection_)
                * sampleRate_;
            // Only its parity is ever read, and 0x10000 is even, so wrapping
            // here keeps the shuffle correct and the counter bounded.
            arpeggioGridSection_ = (arpeggioGridSection_ + 1) & 0xffff;
            arpeggioStep_ = (arpeggioStep_ + 1) % endStep;
            if (arpeggioStep_ == 0)
                for (auto& runtime : arpeggios_)
                {
                    ++runtime.cycle;
                    if (arp.motif == ArpeggioMotif::Random
                        || arp.motif == ArpeggioMotif::RandomL)
                    {
                        arpeggioRng_ = arpeggioRng_ * 1664525u + 1013904223u;
                        runtime.windowCycle = static_cast<int> (arpeggioRng_ >> 16);
                    }
                    else
                    {
                        runtime.windowCycle = runtime.cycle;
                    }
                }
        }
        const double consumed = std::min (left, arpeggioStepRemaining_);
        arpeggioStepRemaining_ -= consumed;
        left -= consumed;
    }
}

// ---------------------------------------------------------------------------
// External input: INPUT VOL -> CENTER CANCEL -> AUDIO FILTER
//
// Settled (OM pp. 49-53): the INPUT jacks are monitored through a dedicated
// filter with its own ON switch, four types (LPF/HPF/BPF/NOTCH — one more than
// the voice filter has), a -12/-24 dB slope, cutoff and resonance; a CENTER
// CANCEL switch removes what is panned to the centre; and none of it is stored
// in the patch. Selecting EXT-IN as an oscillator waveform plays the input
// through the voice instead, in mono, and the direct monitor goes quiet until
// the amp envelope has finished — which is how the manual's "produce sound
// only when you play the keyboard" recipe works, and which also settles that
// the oscillator taps the input *before* the audio filter: with that filter's
// LPF closed the recipe is silent on release and audible under a key.
// ---------------------------------------------------------------------------

bool Engine::anyVoiceUsesExternalInput() const noexcept
{
    for (const auto& voice : voices_)
    {
        if (! voice.active)
            continue;
        const TonePatch& tone = tonePatch (voice.part);
        if (tone.osc1.wave == Waveform::ExtIn || tone.osc2.wave == Waveform::ExtIn)
            return true;
    }
    return false;
}

void Engine::prepareExternalTick (const float* inputLeft, const float* inputRight,
                                  int offset, int samples)
{
    // INPUT VOL is automatable, so it is smoothed and walked across the tick
    // exactly as the monitor fade is: an integer step straight onto live
    // audio would click on the monitor and on any EXT-IN voice at once.
    const double inputGainTarget = mapping::externalInputGain (external_.inputVolume);
    const double inputFrom = smoothedInputGain_;
    smoothedInputGain_ +=
        (inputGainTarget - smoothedInputGain_)
        * std::min (1.0, onePoleCoeff (sampleRate_, mapping::masterSlewSeconds)
                             * samples);
    const double inputStep = (smoothedInputGain_ - inputFrom) / std::max (1, samples);
    const bool haveInput = inputLeft != nullptr && inputRight != nullptr;

    // Direct-path mute while an EXT-IN voice owns the input, with a short fade
    // so the changeover is not a step (voiced). The fade is walked sample by
    // sample: applying one new gain to a whole control tick would replace the
    // discontinuity it exists to prevent with a staircase of smaller ones.
    const double monitorTarget = anyVoiceUsesExternalInput() ? 0.0 : 1.0;
    const double fade =
        1.0 - std::exp (-samples
                        / (sampleRate_ * mapping::externalMonitorFadeSeconds));
    const double monitorFrom = monitorGain_;
    monitorGain_ += (monitorTarget - monitorGain_) * fade;
    const double monitorStep =
        (monitorGain_ - monitorFrom) / std::max (1, samples);

    // The audio filter's cutoff, with the settled AUDIO-FILTER modulation
    // destinations that had nothing to move until now: LFO destination 1 on
    // either tone, and the modulation lever through MODULATION ASSIGN.
    double octaves = std::log2 (mapping::cutoffHz (external_.cutoff));
    for (int index = 0; index < partCount; ++index)
    {
        const Part part = index == 0 ? Part::Upper : Part::Lower;
        if (! partSounds (part))
            continue;
        const TonePatch& tone = tonePatch (part);
        const ToneRuntime& runtime = tones_[static_cast<std::size_t> (index)];
        // Two LFOs routed at one target genuinely sum: they are two
        // modulators, each with its own settled depth.
        if (tone.lfo1.destination1 == LfoDest1::AudioFilter)
            octaves += mapping::audioFilterLfoOctaves (tone.lfo1.depth1)
                       * runtime.lfo1Value;
        if (tone.lfo2.destination1 == LfoDest1::AudioFilter)
            octaves += mapping::audioFilterLfoOctaves (tone.lfo2.depth1)
                       * runtime.lfo2Value;
    }
    // The lever does not. MODULATION ASSIGN is one patch-common setting, the
    // lever is one lever and the audio filter is one filter, so its reach is
    // counted once — inside the loop it was counted per sounding tone, and a
    // DUAL or SPLIT patch moved the cutoff twice as far as the registered
    // constant says. It rides the keyboard part's LFO2, which is the only
    // sounding tone in SINGLE and therefore leaves SINGLE exactly as it was.
    if (patch_.modulationAssign == ModulationAssign::AudioFilter)
    {
        // [voiced, OQ-14] The AUDIO FILTER is one filter and MODULATION
        // DESTINATION names one tone or both, so a single destination picks
        // that tone's LFO2 and BOTH keeps the keyboard part's — which is what
        // SINGLE already gave and leaves it unchanged.
        const std::size_t leverIndex =
            patch_.modulationDestination == ToneDestination::Upper  ? 0u
            : patch_.modulationDestination == ToneDestination::Lower ? 1u
            : (patch_.keyboardPart == KeyboardPart::Upper ? 0u : 1u);
        octaves += modulation_ * mapping::audioFilterLeverOctaves
                   * tones_[leverIndex].lfo2Value;
    }
    const double fc =
        std::clamp (std::exp2 (octaves), 5.0, 0.45 * sampleRate_);
    const double gTarget = std::tan (pi * fc / sampleRate_);
    const double kTarget = mapping::audioFilterDamping (external_.resonance);
    if (! audioFilterPrimed_)
    {
        audioFilterG_ = gTarget;
        audioFilterK_ = kTarget;
        audioFilterPrimed_ = true;
    }
    else
    {
        const double slew =
            1.0 - std::exp (-samples / (sampleRate_ * mapping::controlSlewSeconds));
        audioFilterG_ += (gTarget - audioFilterG_) * slew;
        audioFilterK_ += (kTarget - audioFilterK_) * slew;
    }

    const double fadeStep =
        1.0 / std::max (1.0, mapping::externalSwitchFadeSeconds * sampleRate_);
    const double g = audioFilterG_;
    const double k = audioFilterK_;
    const double a1 = 1.0 / (1.0 + g * (g + k));
    const double a2 = g * a1;

    for (int i = 0; i < samples; ++i)
    {
        double left = 0.0, right = 0.0;
        if (haveInput)
        {
            const double inputGain = inputFrom + inputStep * (i + 1);
            left = inputLeft[offset + i] * inputGain;
            right = inputRight[offset + i] * inputGain;
        }

        // Every switch on this path is crossed rather than thrown: each one
        // chooses between signals whose instantaneous samples differ, so
        // changing one on live audio steps the output however warm the states
        // on the unused side are kept.
        const auto walk = [fadeStep] (double& fade, double target)
        {
            fade += std::clamp (target - fade, -fadeStep, fadeStep);
        };
        walk (centerCancelFade_, external_.centerCancel ? 1.0 : 0.0);
        walk (audioFilterOnFade_, external_.filterOn ? 1.0 : 0.0);
        walk (audioFilterSlopeFade_,
              external_.slope == FilterSlope::Db24 ? 1.0 : 0.0);
        // TYPE crosses over a weight per position, for the reason the voice
        // filter's does: this switch is automatable, and a second change part
        // way through the first one has to stay continuous.
        audioFilterTypeMix_.advance (static_cast<int> (external_.type), fadeStep);

        // CENTER CANCEL: what is common to both channels is what sits at the
        // centre, so removing the mid leaves the sides. The mono reduction the
        // EXT-IN oscillator then takes is the difference rather than the sum,
        // which is what is actually left of the signal (voiced, OQ-14).
        double mono = 0.5 * (left + right);
        {
            const double mid = mono;
            const double cancelledLeft = left - mid;
            const double cancelledRight = right - mid;
            const double cancelledMono = 0.5 * (left - right);
            const double cross = centerCancelFade_;
            left += (cancelledLeft - left) * cross;
            right += (cancelledRight - right) * cross;
            mono += (cancelledMono - mono) * cross;
        }
        // Settled (OM p. 52): an EXT-IN oscillator is mono even from a stereo
        // source. It taps here, before the audio filter.
        externalMono_[static_cast<std::size_t> (i)] = static_cast<float> (mono);

        // The filter runs whether or not it is switched in. Freezing its
        // states while bypassed would let an old resonant tail out of the
        // integrators the moment the automatable switch came back on.
        {
            const auto stagePass = [&] (SvfStage& stage, double input,
                                        double damping, double stageA1,
                                        double stageA2)
            {
                const double v3 = input - stage.ic2eq;
                const double v1 = stageA1 * stage.ic1eq + stageA2 * v3;
                const double v2 = stage.ic2eq + g * v1;
                stage.ic1eq = 2.0 * v1 - stage.ic1eq;
                stage.ic2eq = 2.0 * v2 - stage.ic2eq;
                const double lp = v2;
                const double bp = v1;
                const double hp = input - damping * v1 - v2;
                // All four responses come off the same two integrators, so
                // crossing between the outgoing type and the incoming one
                // costs a select rather than a second filter.
                const auto response = [&] (AudioFilterType type)
                {
                    switch (type)
                    {
                        case AudioFilterType::Lpf: return lp;
                        case AudioFilterType::Hpf: return hp;
                        // Raw, as the voice filter takes it: the contract says
                        // this filter's resonance curve is the voice filter's,
                        // so its band-pass has to behave like the voice
                        // filter's too.
                        case AudioFilterType::Bpf: return bp;
                        // NOTCH is the low-pass and high-pass sum: everything
                        // but the band the resonance would have boosted.
                        case AudioFilterType::Notch: return lp + hp;
                    }
                    return input;
                };
                return audioFilterTypeMix_.mix (
                    [&] (int type) { return response (static_cast<AudioFilterType> (type)); });
            };
            double* channels[2] { &left, &right };
            for (int channel = 0; channel < 2; ++channel)
            {
                // Both stages always run, for the same reason: a frozen
                // integrator would let an arbitrarily old resonant tail out
                // the moment an automated switch came back. Only which of
                // their outputs is taken depends on the switches.
                const double dry = *channels[channel];
                const double first =
                    stagePass (audioFilter1_[channel], dry, k, a1, a2);
                const double second =
                    stagePass (audioFilter2_[channel], first, k, a1, a2);
                const double sloped =
                    first + (second - first) * audioFilterSlopeFade_;
                *channels[channel] = dry + (sloped - dry) * audioFilterOnFade_;
            }
        }

        const double gain = monitorFrom + monitorStep * (i + 1);
        const auto size = static_cast<int> (monitorDelay_[0].size());
        monitorDelay_[0][static_cast<std::size_t> (monitorDelayWrite_)] =
            static_cast<float> (left * gain);
        monitorDelay_[1][static_cast<std::size_t> (monitorDelayWrite_)] =
            static_cast<float> (right * gain);
        const auto read =
            static_cast<std::size_t> ((monitorDelayWrite_ - latencySamples_ + size)
                                      % size);
        externalDirectL_[static_cast<std::size_t> (i)] = monitorDelay_[0][read];
        externalDirectR_[static_cast<std::size_t> (i)] = monitorDelay_[1][read];
        monitorDelayWrite_ = (monitorDelayWrite_ + 1) % size;
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        audioFilter1_[channel].ic1eq = flushDenormal (audioFilter1_[channel].ic1eq);
        audioFilter1_[channel].ic2eq = flushDenormal (audioFilter1_[channel].ic2eq);
        audioFilter2_[channel].ic1eq = flushDenormal (audioFilter2_[channel].ic1eq);
        audioFilter2_[channel].ic2eq = flushDenormal (audioFilter2_[channel].ic2eq);
    }
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

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
    const double timeSmoothing =
        onePoleCoeff (sampleRate_, mapping::delayTimeSlewSeconds);
    const double feedback = delayParams.feedback / 100.0;
    // The four settled damping tables are published frequencies, so each one
    // is realised at its own -3 dB point (mapping::onePoleAtCorner). BYPASS
    // is encoded as 0.0 in the tables and passes through.
    const double dampHz = delayHfDampHz[static_cast<std::size_t> (delayParams.hfDamp)];
    const double dampCoeff = mapping::onePoleAtCorner (dampHz, sampleRate_);
    const double modRateHz =
        mapping::delayModulationRateHz (delayParams.modulationRate);
    const double modDepthSamples =
        (delayParams.modulationDepth / 127.0)
        * mapping::delayModulationDepthSeconds * sampleRate_;
    const double modInc = modRateHz / sampleRate_;
    const int delaySize = static_cast<int> (delayL_.buffer.size());

    // -- reverb coefficients -------------------------------------------------
    const double rt60 = mapping::reverbSeconds (reverbParams.time, reverbParams.size);
    const double highCutHz =
        reverbHighCutHz[static_cast<std::size_t> (reverbParams.highCut)];
    const double highCutCoeff = mapping::onePoleAtCorner (highCutHz, sampleRate_);
    const double lfHz =
        reverbLfDampHz[static_cast<std::size_t> (reverbParams.lfDampFrequency)];
    const double hfHz =
        reverbHfDampHz[static_cast<std::size_t> (reverbParams.hfDampFrequency)];
    const double lfCoeff = mapping::onePoleAtCorner (lfHz, sampleRate_);
    const double hfCoeff = mapping::onePoleAtCorner (hfHz, sampleRate_);
    const double lfGain = std::pow (10.0, reverbParams.lfDampGain / 20.0);
    const double hfGain = std::pow (10.0, reverbParams.hfDampGain / 20.0);
    const double diffusionGain = mapping::reverbDiffusionGain (reverbParams.diffusion);
    const double densityGain = mapping::reverbDensityGain (reverbParams.density);
    const int preDelaySamples = std::min (
        static_cast<int> (mapping::reverbPreDelayMs (reverbParams.preDelay)
                          * 0.001 * sampleRate_),
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
                    static_cast<float> (value + input * mapping::reverbInputInjection);
                reverb_.writes[static_cast<std::size_t> (line)] =
                    (reverb_.writes[static_cast<std::size_t> (line)] + 1) % size;
            }
            // Two disjoint halves of the network, not two overlapping
            // alternating-sign windows. The previous pair summed to
            // taps[0] - taps[6]: five of the seven lines it used cancelled
            // exactly in mono, so a fold-down lost 9 to 10 dB of the tail on
            // every template with a real one, and taps[7] — a line the
            // geometry pays for — reached the output on neither side.
            wetReverbL = taps[0] + taps[2] + taps[4] + taps[6];
            wetReverbR = taps[1] + taps[3] + taps[5] + taps[7];

            // Settled HIGH CUT on the wet return.
            reverb_.highCutStateL += highCutCoeff * (wetReverbL - reverb_.highCutStateL);
            reverb_.highCutStateR += highCutCoeff * (wetReverbR - reverb_.highCutStateR);
            wetReverbL = reverb_.highCutStateL;
            wetReverbR = reverb_.highCutStateR;
            reverb_.fresh = std::min (reverb_.fresh + 1, 1 << 30);
        }

        outL[i] = static_cast<float> (dryL[i] + wetDelayL
                                      + wetReverbL * mapping::reverbWetReturn);
        outR[i] = static_cast<float> (dryR[i] + wetDelayR
                                      + wetReverbR * mapping::reverbWetReturn);
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

void Engine::process (float* left, float* right, int numSamples,
                      const float* inputLeft, const float* inputRight)
{
    int offset = 0;
    float blockPeakL = 0.0f, blockPeakR = 0.0f;

    while (offset < numSamples)
    {
        const int tick = std::min (controlInterval, numSamples - offset);
        const int guarded = std::min (tick, maxBlock_);

        // Per-tone EXPRESSION, smoothed the way the master chain it left is,
        // so an expression pedal cannot step a voice's gain.
        {
            const double coeff = std::min (
                1.0,
                onePoleCoeff (sampleRate_, mapping::masterSlewSeconds) * guarded);
            for (int index = 0; index < partCount; ++index)
            {
                const double target =
                    destinationReaches (patch_.expressionDestination, index == 0)
                        ? expression_
                        : 1.0;
                smoothedExpression_[static_cast<std::size_t> (index)] +=
                    (target - smoothedExpression_[static_cast<std::size_t> (index)])
                    * coeff;
            }
        }

        advanceToneLfos (guarded);
        advanceArpeggiator (guarded);
        prepareExternalTick (inputLeft, inputRight, offset, guarded);

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
            renderVoiceTick (voice, scratchMono_.data(), guarded,
                             externalMono_.data());

            if (voice.ampEnv.idle())
                voice.active = false;

            const TonePatch& tone = tonePatch (voice.part);
            const double expression =
                smoothedExpression_[voice.part == Part::Upper ? 0u : 1u];
            const double delaySend = tone.delayDepth / 127.0;
            const double reverbSend = tone.reverbDepth / 127.0;
            // Walked across the tick, not stepped at its edge: see Voice's
            // ampGain pair.
            const double inverseTick = 1.0 / std::max (1, guarded);
            const double gainLStep =
                (voice.ampGainLTarget - voice.ampGainL) * inverseTick;
            const double gainRStep =
                (voice.ampGainRTarget - voice.ampGainR) * inverseTick;
            const double gainLStart = voice.ampGainL;
            const double gainRStart = voice.ampGainR;
            voice.ampGainL = voice.ampGainLTarget;
            voice.ampGainR = voice.ampGainRTarget;

            // Tone balance sits between the two tones (settled parameter,
            // voiced law shared with the oscillator balance).
            const double toneGain =
                expression
                * (voice.part == Part::Upper
                       ? mapping::balanceLegGain (patch_.toneBalance, false)
                       : mapping::balanceLegGain (patch_.toneBalance, true));

            for (int i = 0; i < guarded; ++i)
            {
                const float sample = scratchMono_[static_cast<std::size_t> (i)];
                const double gainL =
                    (gainLStart + gainLStep * (i + 1)) * mapping::voiceHeadroom;
                const double gainR =
                    (gainRStart + gainRStep * (i + 1)) * mapping::voiceHeadroom;
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
        // EXPRESSION left this chain for the per-tone gains above, because
        // EXPRESSION DESTINATION names the tone or tones it controls; with
        // BOTH, which is the default, the product is exactly what it was.
        const double masterTarget = (masterLevel_ / 127.0)
                                    * (patch_.patchLevel / 127.0)
                                    * partLevel_;
        const double masterCoeff =
            onePoleCoeff (sampleRate_, mapping::masterSlewSeconds) * guarded;
        smoothedMaster_ += (masterTarget - smoothedMaster_)
                           * std::min (1.0, masterCoeff);

        // Part pan (received CC#10): a constant-power tilt on the final pair,
        // unity at center.
        const double panAngle = (partPan_ + 1.0) * 0.25 * pi;
        const double partPanGain[2] {
            std::cos (panAngle) * mapping::partPanCentreGain,
            std::sin (panAngle) * mapping::partPanCentreGain
        };

        // The direct monitor path joins here rather than in the voice sum: it
        // is not patch audio, so the patch level and the part controllers do
        // not scale it, but the panel VOLUME sits after the DAC on the
        // hardware and therefore does.
        // Smoothed the same way the synth path's gain chain is, so automating
        // the panel volume cannot step the monitored input.
        smoothedMonitorLevel_ +=
            ((masterLevel_ / 127.0) - smoothedMonitorLevel_)
            * std::min (1.0, onePoleCoeff (sampleRate_, mapping::masterSlewSeconds)
                                 * guarded);
        const double monitorLevel = smoothedMonitorLevel_;

        for (int channel = 0; channel < 2; ++channel)
        {
            float* out = channel == 0 ? left + offset : right + offset;
            const float* monitor = channel == 0 ? externalDirectL_.data()
                                                : externalDirectR_.data();
            for (int i = 0; i < guarded; ++i)
            {
                double x = out[i] * smoothedMaster_ * partPanGain[channel]
                           + monitor[i] * monitorLevel;
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

    // The meter's fall is a time, not a per-call factor. A fixed 0.85 per
    // render call made the same patch release sixteen times faster at a
    // 1024-sample buffer than at a 64-sample one, which is a property of the
    // host rather than of the sound.
    const double fall = std::exp (-numSamples
                                  / (sampleRate_ * mapping::meterFallSeconds));
    const auto decayed = [fall] (std::atomic<float>& level)
    {
        return static_cast<float> (level.load (std::memory_order_relaxed) * fall);
    };
    outputLevel_[0].store (std::max (blockPeakL, decayed (outputLevel_[0])),
                           std::memory_order_relaxed);
    outputLevel_[1].store (std::max (blockPeakR, decayed (outputLevel_[1])),
                           std::memory_order_relaxed);
}

} // namespace septum

// The Ghost voice: two bandlimited oscillators with hard sync and a
// triangle-cross ring modulator, one white-plus-pink noise source, two
// parallel audio paths (series dual filter + ADSR VCA; brightness lowpass +
// Shaper-driven VCA), two ADSRs behind OR'ed gate sources, MOD X with six
// sources and the arpeggiator, and the Shaper Y variable-rate integrator —
// all per the modelling contract in Docs/circuit-modelling-research.md.
// Constants marked "voiced" here are the first-pass choices recorded in
// Docs/open-questions.md.

#include "DSP/GhostEngine.h"

#include <algorithm>
#include <cmath>

namespace ghost
{
namespace
{
    constexpr double pi = 3.14159265358979323846264338327950288;

    [[nodiscard]] double clamp01(double value) noexcept
    {
        return std::clamp(value, 0.0, 1.0);
    }

    // Two-sample polynomial bandlimited step correction.
    [[nodiscard]] double polyBlep(double t, double dt) noexcept
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

    [[nodiscard]] double wrapPhase(double phase) noexcept
    {
        return phase - std::floor(phase);
    }

    // A slider or pot's 0..1 travel mapped exponentially across a stated
    // range, the law every time and rate control in the contract uses.
    [[nodiscard]] double exponentialTravel(double travel, double low,
                                           double high) noexcept
    {
        return low * std::pow(high / low, clamp01(travel));
    }

    // The panel duty-cycle sets: A = 50/30/15/6 %, B = 40/20/10/3 %
    // (anchored: panel line-art and photos; see the research document).
    [[nodiscard]] double dutyFor(Waveform waveform, bool oscA) noexcept
    {
        switch (waveform)
        {
            case Waveform::RectWide:   return oscA ? 0.50 : 0.40;
            case Waveform::RectMid:    return oscA ? 0.30 : 0.20;
            case Waveform::RectNarrow: return oscA ? 0.15 : 0.10;
            case Waveform::RectThin:   return oscA ? 0.06 : 0.03;
            default:                   return 0.50;
        }
    }

    // Naive triangle: −1 at phase 0, +1 at phase 0.5. Its spectrum falls at
    // 12 dB/oct, and the whole voice runs at 2x, so corner bandlimiting is
    // deliberately omitted in v0.
    [[nodiscard]] double triangleWave(double phase) noexcept
    {
        return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
    }

    [[nodiscard]] double oscillatorWave(Waveform waveform, double phase,
                                        double dt, double duty) noexcept
    {
        switch (waveform)
        {
            case Waveform::Triangle:
                return triangleWave(phase);
            case Waveform::Sawtooth:
                return 2.0 * phase - 1.0 - polyBlep(phase, dt);
            default:
            {
                const double raw = phase < duty ? 1.0 : -1.0;
                return raw + polyBlep(phase, dt)
                     - polyBlep(wrapPhase(phase - duty + 1.0), dt);
            }
        }
    }

    // The BA130 anti-parallel pair that bounds each filter's resonant node.
    // Diodes conduct nothing below their knee, so the limiter must be exactly
    // linear there — a plain tanh compresses from zero amplitude and
    // collapses the resonance the CEM3350's Q control is supposed to reach.
    // Knee and ceiling voiced (the node's own units).
    [[nodiscard]] double resonantNodeLimit(double value) noexcept
    {
        constexpr double knee = 1.2;
        constexpr double ceiling = 2.2;
        const double magnitude = std::abs(value);
        if (magnitude <= knee)
            return value;
        const double limited =
            knee + (ceiling - knee) * std::tanh((magnitude - knee)
                                                / (ceiling - knee));
        return value < 0.0 ? -limited : limited;
    }

    // A wider bound for the lowpass integrator: negligible below the node
    // limiter's range, it only stops the second state from drifting away
    // during regenerative self-oscillation.
    [[nodiscard]] double integratorBound(double value) noexcept
    {
        return 4.0 * std::tanh(value * 0.25);
    }

    // Sub-1e-30 state decays cost real time as denormals on hosts without
    // flush-to-zero; audio content never lives down there.
    [[nodiscard]] double flushDenormal(double value) noexcept
    {
        return std::abs(value) < 1.0e-30 ? 0.0 : value;
    }

    // Resonance travel to the TPT damping k = 1/Q. Travel 0 = Q 0.5
    // (the LOW switch value); full travel crosses into slight regeneration so
    // the section truly self-oscillates against the node limiter, as the
    // CEM3350's Q law reaches "oscillation".
    [[nodiscard]] double dampingFromResonance(double travel) noexcept
    {
        return 2.0 * std::pow(0.01, clamp01(travel)) - 0.025;
    }

    [[nodiscard]] float sanitisedTravel(float value, float fallback) noexcept
    {
        if (!std::isfinite(value))
            return fallback;
        return std::clamp(value, 0.0f, 1.0f);
    }

    // A switch enum smuggled in out of range (a corrupted preset, a hostile
    // host) must not index past a lookup table; it falls back to the
    // power-on detent instead.
    template <typename Enum>
    [[nodiscard]] Enum sanitisedSwitch(Enum value, int positionCount,
                                       Enum fallback) noexcept
    {
        const int raw = static_cast<int>(value);
        return raw < 0 || raw >= positionCount ? fallback : value;
    }
} // namespace

GhostEngine::GhostEngine() noexcept = default;

void GhostEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    // std::clamp passes NaN through (its comparisons are all false), so a
    // host reporting a non-finite rate must be caught before the clamp.
    if (!std::isfinite(sampleRate))
        sampleRate = 44100.0;
    sampleRate_ = std::clamp(sampleRate, minimumSupportedSampleRate,
                             maximumSupportedSampleRate);
    internalRate_ = 2.0 * sampleRate_;

    // Windowed-sinc halfband decimation kernel for the 2x -> 1x boundary.
    const int center = halfbandTaps / 2;
    double sum = 0.0;
    for (int index = 0; index < halfbandTaps; ++index)
    {
        const double n = static_cast<double>(index - center);
        const double sinc =
            n == 0.0 ? 1.0 : std::sin(pi * n / 2.0) / (pi * n / 2.0);
        const double window =
            0.42 - 0.5 * std::cos(2.0 * pi * index / (halfbandTaps - 1))
            + 0.08 * std::cos(4.0 * pi * index / (halfbandTaps - 1));
        halfbandKernel_[static_cast<std::size_t>(index)] = sinc * window;
        sum += sinc * window;
    }
    for (auto& tap : halfbandKernel_)
        tap /= sum;

    // The pinking network's poles are physical frequencies, not per-sample
    // constants: the reference recurrence coefficients describe poles at a
    // 44.1 kHz design rate, and each is re-derived here for the actual
    // internal rate, with its input gain rescaled to keep the per-band DC
    // gain — so the noise colour survives any host rate.
    static constexpr std::array<double, 3> pinkReferenceA {
        0.99765, 0.96300, 0.57000 };
    static constexpr std::array<double, 3> pinkReferenceG {
        0.0990460, 0.2965164, 1.0526913 };
    constexpr double pinkDesignRate = 44100.0;
    for (std::size_t pole = 0; pole < 3; ++pole)
    {
        const double poleHz = -std::log(pinkReferenceA[pole]) * pinkDesignRate
                            / (2.0 * pi);
        pinkA_[pole] = std::exp(-2.0 * pi * poleHz / internalRate_);
        pinkG_[pole] = pinkReferenceG[pole] * (1.0 - pinkA_[pole])
                     / (1.0 - pinkReferenceA[pole]);
    }

    reset();
}

void GhostEngine::reset()
{
    keyStackSize_ = 0;
    keyGate_ = false;
    currentNote_ = -1;
    pendingTrigger_ = false;
    pendingShaperTrigger_ = false;
    pitchBend_ = 0.0f;
    modWheel_ = 0.0f;
    shaperWheel_ = 0.0f;

    glidedNote_ = 60.0;
    glideInitialised_ = false;

    lfoPhase_ = 0.0;
    lfoSquareHigh_ = false;
    previousLfoSquareHigh_ = false;
    redNoiseState_ = 0.0;
    sampleHoldValue_ = 0.0;
    noiseSeed_ = 0x9e3779b9u;

    shaperLevel_ = 0.0;
    shaperRising_ = true;
    shaperCycleActive_ = false;
    shaperGate_ = false;
    previousGateForShaper_ = false;

    filterEnvelope_ = Adsr {};
    loudnessEnvelope_ = Adsr {};

    arpStep_ = 0;
    arpSoundingNote_ = -1;

    phaseA_ = 0.0;
    phaseB_ = 0.0;
    lastOscBWave_ = 0.0;
    pinkState_[0] = pinkState_[1] = pinkState_[2] = 0.0;
    brightnessState_ = 0.0;

    lowerSection_ = SvfSection {};
    upperFirst_ = SvfSection {};
    upperSecond_ = SvfSection {};

    filterRing_.fill(0.0);
    shaperRing_.fill(0.0);
    decimatorIndex_ = 0;

    dcPreviousInLeft_ = 0.0;
    dcPreviousOutLeft_ = 0.0;
    dcPreviousInRight_ = 0.0;
    dcPreviousOutRight_ = 0.0;
}

void GhostEngine::stopAllSound()
{
    // Implemented over reset() so the voice-killing list can never drift
    // out of step with it; only the controller positions survive.
    const float pitchBend = pitchBend_;
    const float modWheel = modWheel_;
    const float shaperWheel = shaperWheel_;
    reset();
    pitchBend_ = pitchBend;
    modWheel_ = modWheel;
    shaperWheel_ = shaperWheel;
}

void GhostEngine::setParameters(const EngineParameters& parameters)
{
    // A NaN smuggled in through host automation must neither reach the
    // control laws nor outlive the next valid set; every travel field is
    // normalised to a finite value in 0..1, falling back to its power-on
    // default.
    constexpr EngineParameters defaults {};
    EngineParameters sane = parameters;

    const auto travel = [](float value, float fallback) noexcept {
        return sanitisedTravel(value, fallback);
    };
    sane.tune = travel(parameters.tune, defaults.tune);
    sane.interval = travel(parameters.interval, defaults.interval);
    sane.masterVolume = travel(parameters.masterVolume, defaults.masterVolume);
    sane.brightness = travel(parameters.brightness, defaults.brightness);
    sane.shaperPathA = travel(parameters.shaperPathA, defaults.shaperPathA);
    sane.shaperPathB = travel(parameters.shaperPathB, defaults.shaperPathB);
    sane.shaperPathRing =
        travel(parameters.shaperPathRing, defaults.shaperPathRing);
    sane.shaperPathNoise =
        travel(parameters.shaperPathNoise, defaults.shaperPathNoise);
    sane.filterPathA = travel(parameters.filterPathA, defaults.filterPathA);
    sane.filterPathB = travel(parameters.filterPathB, defaults.filterPathB);
    sane.filterPathNoise =
        travel(parameters.filterPathNoise, defaults.filterPathNoise);
    sane.cutoff = travel(parameters.cutoff, defaults.cutoff);
    sane.lowerOnly = travel(parameters.lowerOnly, defaults.lowerOnly);
    sane.resonance = travel(parameters.resonance, defaults.resonance);
    sane.kbAmount = travel(parameters.kbAmount, defaults.kbAmount);
    sane.filterEnvAmount =
        travel(parameters.filterEnvAmount, defaults.filterEnvAmount);
    sane.filterAttack = travel(parameters.filterAttack, defaults.filterAttack);
    sane.filterDecay = travel(parameters.filterDecay, defaults.filterDecay);
    sane.filterSustain =
        travel(parameters.filterSustain, defaults.filterSustain);
    sane.filterRelease =
        travel(parameters.filterRelease, defaults.filterRelease);
    sane.loudnessAttack =
        travel(parameters.loudnessAttack, defaults.loudnessAttack);
    sane.loudnessDecay =
        travel(parameters.loudnessDecay, defaults.loudnessDecay);
    sane.loudnessSustain =
        travel(parameters.loudnessSustain, defaults.loudnessSustain);
    sane.loudnessRelease =
        travel(parameters.loudnessRelease, defaults.loudnessRelease);
    sane.lfoRate = travel(parameters.lfoRate, defaults.lfoRate);
    sane.shaperShape = travel(parameters.shaperShape, defaults.shaperShape);
    sane.shaperRate = travel(parameters.shaperRate, defaults.shaperRate);
    sane.glide = travel(parameters.glide, defaults.glide);

    sane.octave = sanitisedSwitch(parameters.octave, 4, defaults.octave);
    sane.oscAWaveform =
        sanitisedSwitch(parameters.oscAWaveform, 6, defaults.oscAWaveform);
    sane.oscBWaveform =
        sanitisedSwitch(parameters.oscBWaveform, 6, defaults.oscBWaveform);
    sane.oscBRange =
        sanitisedSwitch(parameters.oscBRange, 6, defaults.oscBRange);
    sane.lowerMode =
        sanitisedSwitch(parameters.lowerMode, 4, defaults.lowerMode);
    sane.slope = sanitisedSwitch(parameters.slope, 2, defaults.slope);
    sane.upperResonance = sanitisedSwitch(parameters.upperResonance, 2,
                                          defaults.upperResonance);
    sane.tracking =
        sanitisedSwitch(parameters.tracking, 2, defaults.tracking);
    sane.trigger = sanitisedSwitch(parameters.trigger, 2, defaults.trigger);
    sane.modSource =
        sanitisedSwitch(parameters.modSource, 6, defaults.modSource);
    sane.modXTo = sanitisedSwitch(parameters.modXTo, 6, defaults.modXTo);
    sane.shaperYTo =
        sanitisedSwitch(parameters.shaperYTo, 6, defaults.shaperYTo);
    sane.shaperMode =
        sanitisedSwitch(parameters.shaperMode, 4, defaults.shaperMode);
    sane.arpeggiator =
        sanitisedSwitch(parameters.arpeggiator, 4, defaults.arpeggiator);
    sane.glideMode =
        sanitisedSwitch(parameters.glideMode, 3, defaults.glideMode);

    parameters_ = sane;
    oscADuty_ = dutyFor(sane.oscAWaveform, true);
    oscBDuty_ = dutyFor(sane.oscBWaveform, false);
}

void GhostEngine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;

    // Running status lets a MIDI sender encode Note Off as Note On with
    // velocity zero; treating it as a press would hold the gate open
    // forever. Beyond that the hardware keyboard has no velocity at all.
    if (!(velocity > 0.0f))
    {
        noteOff(midiNote);
        return;
    }

    // Re-pressing a held key moves it to the top of the stack rather than
    // duplicating it. The stack spans the whole MIDI note domain, so after
    // deduplication it cannot be full.
    for (int index = 0; index < keyStackSize_; ++index)
    {
        if (keyStack_[static_cast<std::size_t>(index)] == midiNote)
        {
            for (int shift = index; shift < keyStackSize_ - 1; ++shift)
                keyStack_[static_cast<std::size_t>(shift)] =
                    keyStack_[static_cast<std::size_t>(shift + 1)];
            --keyStackSize_;
            break;
        }
    }
    keyStack_[static_cast<std::size_t>(keyStackSize_)] =
        static_cast<std::int16_t>(midiNote);
    ++keyStackSize_;

    const bool firstKey = !keyGate_;
    keyGate_ = true;
    currentNote_ = midiNote;

    // MULTIPLE re-gates on every new key; SINGLE re-gates only after all
    // keys were released. The Shaper's RESET mode is always
    // multiple-trigger regardless of that switch, so it records every press.
    if (parameters_.trigger == TriggerMode::Multiple || firstKey)
        pendingTrigger_ = true;
    pendingShaperTrigger_ = true;
}

void GhostEngine::noteOff(int midiNote)
{
    for (int index = 0; index < keyStackSize_; ++index)
    {
        if (keyStack_[static_cast<std::size_t>(index)] == midiNote)
        {
            for (int shift = index; shift < keyStackSize_ - 1; ++shift)
                keyStack_[static_cast<std::size_t>(shift)] =
                    keyStack_[static_cast<std::size_t>(shift + 1)];
            --keyStackSize_;
            break;
        }
    }

    if (midiNote != currentNote_)
        return;

    if (keyStackSize_ > 0)
    {
        // Fall back to the newest key still held, at its pitch, without
        // retriggering — the hardware scanner's held-note memory.
        currentNote_ = keyStack_[static_cast<std::size_t>(keyStackSize_ - 1)];
        return;
    }

    keyGate_ = false;
}

void GhostEngine::releaseAllKeys() noexcept
{
    keyStackSize_ = 0;
    keyGate_ = false;
}

void GhostEngine::setPitchBend(float normalisedBipolar) noexcept
{
    if (!std::isfinite(normalisedBipolar))
        normalisedBipolar = 0.0f;
    pitchBend_ = std::clamp(normalisedBipolar, -1.0f, 1.0f);
}

void GhostEngine::setModWheel(float amount) noexcept
{
    if (!std::isfinite(amount))
        amount = 0.0f;
    modWheel_ = std::clamp(amount, 0.0f, 1.0f);
}

void GhostEngine::setShaperWheel(float amount) noexcept
{
    if (!std::isfinite(amount))
        amount = 0.0f;
    shaperWheel_ = std::clamp(amount, 0.0f, 1.0f);
}

GhostEngine::SvfOutputs GhostEngine::runSection(SvfSection& section,
                                                double input, double g,
                                                double k) noexcept
{
    const double a = 1.0 / (1.0 + g * (g + k));
    const double hp = (input - (g + k) * section.ic1 - section.ic2) * a;
    const double bp = g * hp + section.ic1;
    const double lp = g * bp + section.ic2;
    section.ic1 = flushDenormal(resonantNodeLimit(2.0 * bp - section.ic1));
    section.ic2 = flushDenormal(integratorBound(2.0 * lp - section.ic2));
    return { lp, bp, hp };
}

void GhostEngine::advanceEnvelope(Adsr& envelope, bool gate, bool triggerPulse,
                                  double attackCoefficient,
                                  double decayCoefficient,
                                  double releaseCoefficient,
                                  double sustain) noexcept
{
    if (triggerPulse)
        envelope.stage = Adsr::Stage::Attack;
    else if (!gate && envelope.stage != Adsr::Stage::Idle)
        envelope.stage = Adsr::Stage::Release;

    switch (envelope.stage)
    {
        case Adsr::Stage::Attack:
            // A 556 timer charges toward its rail and switches at a
            // threshold; aiming 1.5x past the peak reproduces that
            // quasi-exponential punch.
            envelope.level += (1.5 - envelope.level) * attackCoefficient;
            if (envelope.level >= 1.0)
            {
                envelope.level = 1.0;
                envelope.stage = Adsr::Stage::Decay;
            }
            break;
        case Adsr::Stage::Decay:
            envelope.level += (sustain - envelope.level) * decayCoefficient;
            break;
        case Adsr::Stage::Release:
            envelope.level -= envelope.level * releaseCoefficient;
            if (envelope.level < 1.0e-5)
            {
                envelope.level = 0.0;
                envelope.stage = Adsr::Stage::Idle;
            }
            break;
        case Adsr::Stage::Idle:
            break;
    }
}

void GhostEngine::handleArpClock() noexcept
{
    if (parameters_.arpeggiator == ArpeggiatorMode::Off || keyStackSize_ == 0)
    {
        arpSoundingNote_ = -1;
        arpStep_ = 0;
        return;
    }

    // The scan is chromatic bottom-to-top of whatever is held, wrapped.
    std::array<std::int16_t, keyStackCapacity> sorted {};
    for (int index = 0; index < keyStackSize_; ++index)
        sorted[static_cast<std::size_t>(index)] =
            keyStack_[static_cast<std::size_t>(index)];
    std::sort(sorted.begin(), sorted.begin() + keyStackSize_);

    const int count = keyStackSize_;
    const int noteIndex = arpStep_ % count;
    static constexpr std::array<int, 3> octavePattern { 0, 12, -12 };

    int octaveOffset = 0;
    switch (parameters_.arpeggiator)
    {
        case ArpeggiatorMode::Ripple:
            break;
        case ArpeggiatorMode::Arpeggio:
            // The whole held sequence at pitch, then one octave up, then one
            // octave down, repeating.
            octaveOffset = octavePattern[static_cast<std::size_t>(
                (arpStep_ / count) % 3)];
            break;
        case ArpeggiatorMode::Leap:
            // The octave pattern advances per note, not per pass.
            octaveOffset =
                octavePattern[static_cast<std::size_t>(arpStep_ % 3)];
            break;
        case ArpeggiatorMode::Off:
            break;
    }

    // The transposed value is an internal CV, not a MIDI event: clamping it
    // to the MIDI domain would turn the documented octave step into seven
    // semitones at the extremes. The oscillator already bounds its rendered
    // frequency.
    arpSoundingNote_ =
        static_cast<int>(sorted[static_cast<std::size_t>(noteIndex)])
        + octaveOffset;
    ++arpStep_;
}

void GhostEngine::advanceControls() noexcept
{
    const double dt = 1.0 / sampleRate_;
    const EngineParameters& p = parameters_;

    // ---------------------------------------------------------------- Gate
    const bool anyGateSelected = p.gateKbd || p.gateX || p.gateYExt;

    // ----------------------------------------------------------------- LFO
    // <1 Hz to ~50 Hz (anchored); the Y wheel can only raise it — the wheel
    // sets the fastest rate, the panel knob the slowest.
    double lfoHz = exponentialTravel(p.lfoRate, 0.3, 50.0);
    if (p.shaperYTo == ShaperYDestination::LfoRate)
    {
        const double ySignal = clamp01(shaperLevel_) * shaperWheel_;
        lfoHz *= std::pow(60.0 / lfoHz, clamp01(ySignal));
        lfoHz = std::min(lfoHz, 60.0);
    }

    lfoPhase_ += lfoHz * dt;
    if (lfoPhase_ >= 1.0)
        lfoPhase_ -= std::floor(lfoPhase_);
    const double lfoTriangle = triangleWave(lfoPhase_);
    lfoSquareHigh_ = lfoPhase_ < 0.5;
    const double lfoSquare = lfoSquareHigh_ ? 1.0 : -1.0;
    // The S&H and arpeggiator clock on the square's rising edge — including
    // the very first one after a reset, so the arpeggiator's opening step is
    // the documented bottom-of-the-scan note, not a full clock period of the
    // last-pressed key.
    const bool clockEdge = lfoSquareHigh_ && !previousLfoSquareHigh_;
    previousLfoSquareHigh_ = lfoSquareHigh_;

    // Red noise: slow continuous random (about 1.5 Hz pole, gain restored).
    noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
    const double white =
        static_cast<double>(static_cast<std::int32_t>(noiseSeed_))
        / 2147483648.0;
    const double redCoefficient = 1.0 - std::exp(-2.0 * pi * 1.5 * dt);
    redNoiseState_ += redCoefficient * (white - redNoiseState_);
    const double redNoise = std::clamp(redNoiseState_ * 18.0, -1.0, 1.0);

    if (clockEdge)
    {
        sampleHoldValue_ = p.modSource == ModSource::SampleHoldY
                               ? shaperLevel_
                               : redNoise;
        handleArpClock();
    }

    // ------------------------------------------------------------- Shaper Y
    // US 3,943,456: RATE sets the total period, SHAPE apportions it between
    // rise and fall without changing it. Arpeggiator modes clock-slave the
    // Shaper to the LFO except in FREE.
    double shaperPeriod = exponentialTravel(1.0 - p.shaperRate, 0.045, 20.0);
    if (p.arpeggiator != ArpeggiatorMode::Off
        && p.shaperMode != ShaperMode::Free)
        shaperPeriod = 1.0 / std::max(lfoHz, 1.0e-3);
    const double riseFraction = 0.05 + 0.9 * p.shaperShape;
    const double riseStep = dt / std::max(shaperPeriod * riseFraction, 1.0e-4);
    const double fallStep =
        dt / std::max(shaperPeriod * (1.0 - riseFraction), 1.0e-4);

    const bool combinedGateNow = anyGateSelected
        && ((p.gateKbd && keyGate_) || (p.gateX && lfoSquareHigh_)
            || (p.gateYExt && shaperGate_));
    const bool gateRise = combinedGateNow && !previousGateForShaper_;
    // RESET mode is always multiple-trigger: every key press restarts it
    // regardless of the TRIGGER switch — including a legato press under
    // SINGLE. Key presses alone drive the reset; a gate-bus edge must not,
    // because with Y/EXT as the only selected source the Shaper's own gate
    // would clamp the Shaper back to zero the moment it crossed its own
    // threshold, and the single rise/fall cycle could never complete.
    const bool shaperRetrigger =
        p.shaperMode == ShaperMode::Reset && pendingShaperTrigger_;
    pendingShaperTrigger_ = false;

    switch (p.shaperMode)
    {
        case ShaperMode::Free:
            // A free-running oscillator, symmetric about zero.
            if (shaperRising_)
            {
                shaperLevel_ += 2.0 * riseStep;
                if (shaperLevel_ >= 1.0)
                {
                    shaperLevel_ = 1.0;
                    shaperRising_ = false;
                }
            }
            else
            {
                shaperLevel_ -= 2.0 * fallStep;
                if (shaperLevel_ <= -1.0)
                {
                    shaperLevel_ = -1.0;
                    shaperRising_ = true;
                }
            }
            break;
        case ShaperMode::KbdHold:
            // Rises while gated and holds at maximum; releases to zero.
            if (combinedGateNow)
                shaperLevel_ = std::min(1.0, shaperLevel_ + riseStep);
            else
                shaperLevel_ = std::max(0.0, shaperLevel_ - fallStep);
            break;
        case ShaperMode::Reset:
            if (shaperRetrigger || (gateRise && !shaperCycleActive_))
            {
                shaperLevel_ = 0.0;
                shaperRising_ = true;
                shaperCycleActive_ = true;
            }
            [[fallthrough]];
        case ShaperMode::Run:
            if (p.shaperMode == ShaperMode::Run && gateRise
                && !(shaperCycleActive_ && shaperRising_))
            {
                // RUN never abandons a rise in progress; a new gate is
                // ignored until the rising segment has completed.
                shaperLevel_ = 0.0;
                shaperRising_ = true;
                shaperCycleActive_ = true;
            }
            if (shaperCycleActive_)
            {
                if (shaperRising_)
                {
                    shaperLevel_ += riseStep;
                    if (shaperLevel_ >= 1.0)
                    {
                        shaperLevel_ = 1.0;
                        shaperRising_ = false;
                    }
                }
                else
                {
                    shaperLevel_ -= fallStep;
                    if (shaperLevel_ <= 0.0)
                    {
                        shaperLevel_ = 0.0;
                        shaperCycleActive_ = false;
                    }
                }
            }
            break;
    }
    previousGateForShaper_ = combinedGateNow;

    // The Shaper's own gate (voiced comparator threshold, OQ-05).
    shaperGate_ = shaperLevel_ > 0.01;

    // ------------------------------------------------------------ Envelopes
    // A key press articulates only through the keyboard's own gate source:
    // with KBD deselected, the trigger chain never sees the press, and X
    // auto-repeat articulates on its clock alone — as the hardware's
    // selected-bus trigger derivation behaves.
    const bool triggerPulse = anyGateSelected && combinedGateNow
        && ((pendingTrigger_ && p.gateKbd) || gateRise);
    pendingTrigger_ = false;

    // Decay and release are ordinary exponentials, read as ~95 % settled
    // (three time constants) inside the labelled 5 ms–10 s. The attack aims
    // past its peak at 1.5 and ends at 1.0, which takes ln(3) time
    // constants, so its coefficient is derived from that threshold — the
    // labelled time is the actual time-to-peak, not 2.7x shorter.
    const auto segmentCoefficient = [dt](double travel) {
        const double seconds = exponentialTravel(travel, 0.005, 10.0);
        return 1.0 - std::exp(-dt * 3.0 / seconds);
    };
    const auto attackCoefficient = [dt](double travel) {
        constexpr double lnThree = 1.0986122886681098;
        const double seconds = exponentialTravel(travel, 0.005, 10.0);
        return 1.0 - std::exp(-dt * lnThree / seconds);
    };
    advanceEnvelope(filterEnvelope_, combinedGateNow, triggerPulse,
                    attackCoefficient(p.filterAttack),
                    segmentCoefficient(p.filterDecay),
                    segmentCoefficient(p.filterRelease),
                    static_cast<double>(p.filterSustain));
    advanceEnvelope(loudnessEnvelope_, combinedGateNow, triggerPulse,
                    attackCoefficient(p.loudnessAttack),
                    segmentCoefficient(p.loudnessDecay),
                    segmentCoefficient(p.loudnessRelease),
                    static_cast<double>(p.loudnessSustain));

    // ---------------------------------------------------------- MOD X value
    double modXSource = 0.0;
    switch (p.modSource)
    {
        case ModSource::LfoTriangle:      modXSource = lfoTriangle; break;
        case ModSource::LfoSquare:        modXSource = lfoSquare; break;
        case ModSource::SampleHoldRandom: modXSource = sampleHoldValue_; break;
        case ModSource::SampleHoldY:      modXSource = sampleHoldValue_; break;
        case ModSource::RedNoise:         modXSource = redNoise; break;
        case ModSource::OscB:
            // The schematic feeds the selected, buffered Osc B waveform to
            // the mod board, not a hard-wired triangle.
            modXSource = lastOscBWave_;
            break;
    }
    double xSignal = modXSource * modWheel_;
    if (p.shapeXWithY)
        xSignal *= clamp01(shaperLevel_);
    const double ySignal = shaperLevel_ * shaperWheel_;

    // Full-wheel modulation depths (voiced): one octave of pitch, three
    // octaves of cutoff, ±0.42 of pulse duty.
    constexpr double pitchDepthOctaves = 1.0;
    constexpr double filterDepthOctaves = 3.0;
    constexpr double dutyDepth = 0.42;

    double modAOctaves = 0.0;
    double modBOctaves = 0.0;
    double modUpperOctaves = 0.0;
    double modLowerOctaves = 0.0;
    controlPwmA_ = 0.0;
    controlPwmB_ = 0.0;

    switch (p.modXTo)
    {
        case ModXDestination::Off: break;
        case ModXDestination::OscAB:
            modAOctaves += xSignal * pitchDepthOctaves;
            modBOctaves += xSignal * pitchDepthOctaves;
            break;
        case ModXDestination::OscA:
            modAOctaves += xSignal * pitchDepthOctaves;
            break;
        case ModXDestination::OscARwm:
            controlPwmA_ = xSignal * dutyDepth;
            break;
        case ModXDestination::FilterUL:
            modUpperOctaves += xSignal * filterDepthOctaves;
            modLowerOctaves += xSignal * filterDepthOctaves;
            break;
        case ModXDestination::FilterU:
            modUpperOctaves += xSignal * filterDepthOctaves;
            break;
    }
    switch (p.shaperYTo)
    {
        case ShaperYDestination::Off: break;
        case ShaperYDestination::OscAB:
            modAOctaves += ySignal * pitchDepthOctaves;
            modBOctaves += ySignal * pitchDepthOctaves;
            break;
        case ShaperYDestination::OscB:
            modBOctaves += ySignal * pitchDepthOctaves;
            break;
        case ShaperYDestination::OscBRwm:
            controlPwmB_ = ySignal * dutyDepth;
            break;
        case ShaperYDestination::LfoRate:
            break; // consumed above, before the LFO advanced
        case ShaperYDestination::FilterL:
            modLowerOctaves += ySignal * filterDepthOctaves;
            break;
    }

    // ------------------------------------------------------------ Keyboard CV
    const int soundingNote = (p.arpeggiator != ArpeggiatorMode::Off
                              && arpSoundingNote_ >= 0)
                                 ? arpSoundingNote_
                                 : currentNote_;
    if (soundingNote >= 0)
    {
        if (!glideInitialised_)
        {
            glidedNote_ = soundingNote;
            glideInitialised_ = true;
        }
        const bool glideActive =
            p.glideMode == GlideMode::On
            || (p.glideMode == GlideMode::Auto && keyStackSize_ >= 2);
        if (glideActive && p.glide > 0.0f)
        {
            // Single-pole lag: 2 MOhm pot into ~450 nF, tau up to ~0.9 s.
            const double tau = std::max(
                1.0e-4, 0.9 * static_cast<double>(p.glide * p.glide));
            const double coefficient = 1.0 - std::exp(-dt / tau);
            glidedNote_ +=
                coefficient * (static_cast<double>(soundingNote) - glidedNote_);
        }
        else
        {
            glidedNote_ = soundingNote;
        }
    }

    static constexpr std::array<double, 4> masterOctaveOffset {
        -2.0, -1.0, 0.0, 1.0 };
    const double octaveOffset =
        masterOctaveOffset[static_cast<std::size_t>(p.octave)];
    const double tuneOctaves =
        (static_cast<double>(p.tune) - 0.5) * 2.0 * (3.0 / 12.0);
    const double bendOctaves =
        static_cast<double>(pitchBend_) * (8.0 / 12.0);

    const double keyboardOctaves = (glidedNote_ - 69.0) / 12.0;
    const double masterBus =
        keyboardOctaves + octaveOffset + tuneOctaves + bendOctaves;

    controlOscAOctaves_ = masterBus + modAOctaves;

    controlOscBDrone_ = p.oscBRange == OscBRange::Bass
                     || p.oscBRange == OscBRange::Wide;
    if (controlOscBDrone_)
    {
        // BASS 30..300 Hz, WIDE 2..10,000 Hz; disconnected from keyboard,
        // tune, octave and bend — X/Y modulation still applies.
        const double droneHz =
            p.oscBRange == OscBRange::Bass
                ? exponentialTravel(p.interval, 30.0, 300.0)
                : exponentialTravel(p.interval, 2.0, 10000.0);
        controlOscBDroneHz_ = droneHz * std::exp2(modBOctaves);
    }
    else
    {
        static constexpr std::array<double, 4> rangeOffset {
            -1.0, 0.0, 1.0, 2.0 };
        const double intervalOctaves =
            (static_cast<double>(p.interval) - 0.5) * 2.0 * (7.0 / 12.0);
        controlOscBOctaves_ = masterBus
            + rangeOffset[static_cast<std::size_t>(p.oscBRange)]
            + intervalOctaves + modBOctaves;
    }

    // ------------------------------------------------------------ Filter CVs
    // The cutoff bus, in octaves. MASTER spans the audio range (voiced
    // 20 Hz..16 kHz, OQ-02); tracking reaches ~110 % and pivots at middle C
    // (voiced pivot); the envelope straddles the cutoff by up to
    // ±2.5 octaves (anchored).
    const double upperBaseHz =
        exponentialTravel(p.cutoff, 20.0, 16000.0);
    const double trackingOctaves = static_cast<double>(p.kbAmount) * 1.1
        * (glidedNote_ - 60.0) / 12.0;
    const double envelopeOctaves =
        (static_cast<double>(p.filterEnvAmount) - 0.5) * 2.0 * 2.5
        * filterEnvelope_.level;

    const double upperOctaves =
        trackingOctaves + envelopeOctaves + modUpperOctaves;
    controlUpperCutoffHz_ = upperBaseHz * std::exp2(upperOctaves);

    // LOWER ONLY: coincide at 0.8 (anchored); span voiced (OQ-02). FORMANT
    // freezes the lower filter's peak: keyboard, envelope and both wheels
    // are cut; MASTER and LOWER ONLY still act.
    const double lowerOffsetOctaves =
        (static_cast<double>(p.lowerOnly) - 0.8) * 6.25;
    const bool lowerDynamic = p.tracking == TrackingMode::Dynamic;
    const double lowerOctaves = lowerOffsetOctaves
        + (lowerDynamic ? trackingOctaves + envelopeOctaves + modLowerOctaves
                        : 0.0);
    controlLowerCutoffHz_ = upperBaseHz * std::exp2(lowerOctaves);

    controlLowerK_ = dampingFromResonance(p.resonance);
    controlUpperK_ = p.upperResonance == UpperResonanceMode::Low
                         ? 2.0
                         : dampingFromResonance(p.resonance);

    // -------------------------------------------------------------- Gains
    controlLoudnessGain_ =
        p.vcaBypass ? 1.0 : loudnessEnvelope_.level;
    // The Shaper path VCA follows the raw Shaper output; the OTA clamps at
    // zero gain, so FREE mode's negative half-cycle is silent.
    controlShaperVcaGain_ = std::max(0.0, shaperLevel_);

    // BRIGHTNESS: 100k log pot + 27 nF — ~59 Hz at full resistance, and the
    // pot genuinely reaches zero at full travel, leaving only the residual
    // series resistance (~17.9 kHz pole): the maximum setting is effectively
    // open, not a shelf.
    constexpr double brightnessFloor = 0.00316227766016838; // 10^-2.5
    const double brightnessSpan =
        std::pow(10.0, -2.5 * static_cast<double>(p.brightness));
    const double brightnessR =
        100.0e3 * (brightnessSpan - brightnessFloor) / (1.0 - brightnessFloor)
        + 330.0;
    const double brightnessHz =
        std::min(1.0 / (2.0 * pi * brightnessR * 27.0e-9),
                 0.45 * internalRate_);
    controlBrightnessCoefficient_ =
        1.0 - std::exp(-2.0 * pi * brightnessHz / internalRate_);
}

void GhostEngine::renderVoiceSample() noexcept
{
    const EngineParameters& p = parameters_;
    const double dt = 1.0 / internalRate_;

    // ----------------------------------------------------------- Oscillators
    const double frequencyA =
        std::min(440.0 * std::exp2(controlOscAOctaves_), 0.45 * internalRate_);
    const double frequencyB = std::min(
        controlOscBDrone_ ? controlOscBDroneHz_
                          : 440.0 * std::exp2(controlOscBOctaves_),
        0.45 * internalRate_);

    const double stepA = frequencyA * dt;
    const double stepB = frequencyB * dt;

    phaseA_ += stepA;
    const bool aWrapped = phaseA_ >= 1.0;
    if (aWrapped)
        phaseA_ -= std::floor(phaseA_);

    phaseB_ += stepB;
    if (phaseB_ >= 1.0)
        phaseB_ -= std::floor(phaseB_);
    // Hard sync, one-directional A -> B: A's reset restarts B's ramp at the
    // corresponding fraction of B's own step. (Bandlimiting the sync edge is
    // a refinement target; at 2x the residual aliasing is tolerable.)
    if (p.sync && aWrapped && stepA > 0.0)
        phaseB_ = (phaseA_ / stepA) * stepB;

    const double dutyA =
        std::clamp(oscADuty_ + controlPwmA_, 0.03, 0.97);
    const double dutyB =
        std::clamp(oscBDuty_ + controlPwmB_, 0.03, 0.97);

    const double waveA =
        oscillatorWave(p.oscAWaveform, phaseA_, stepA, dutyA);
    const double waveB =
        oscillatorWave(p.oscBWaveform, phaseB_, stepB, dutyB);
    lastOscBWave_ = waveB;

    // Ring modulator: A-triangle x B-triangle taken before the waveform
    // switches, with the un-nulled carrier bleed the untrimmed OTA bias
    // leaves in (voiced 3 %, OQ-06).
    const double triA = triangleWave(phaseA_);
    const double triB = triangleWave(phaseB_);
    const double ring = triA * triB + 0.03 * (triA + triB);

    // Noise: one source, "a combination of white and pink" — MM5837 white
    // through a partial pinking network.
    noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
    const double white =
        static_cast<double>(static_cast<std::int32_t>(noiseSeed_))
        / 2147483648.0;
    pinkState_[0] = pinkA_[0] * pinkState_[0] + white * pinkG_[0];
    pinkState_[1] = pinkA_[1] * pinkState_[1] + white * pinkG_[1];
    pinkState_[2] = pinkA_[2] * pinkState_[2] + white * pinkG_[2];
    const double pink =
        (pinkState_[0] + pinkState_[1] + pinkState_[2] + white * 0.1848)
        * 0.18;
    const double noise = 0.55 * pink + 0.45 * white * 0.5;

    // ------------------------------------------------------ Filter/ADSR path
    // Mixer summing gain follows the 220k-into-100k hardware ratio.
    constexpr double mixGain = 0.45;
    double filterPath = mixGain
        * (static_cast<double>(p.filterPathA) * waveA
           + static_cast<double>(p.filterPathB) * waveB
           + static_cast<double>(p.filterPathNoise) * noise);

    const double upperG = std::tan(
        pi * std::min(controlUpperCutoffHz_, 0.45 * internalRate_)
        / internalRate_);
    const double lowerG = std::tan(
        pi * std::min(controlLowerCutoffHz_, 0.45 * internalRate_)
        / internalRate_);

    // The lower section always processes the signal, as the hardware chip
    // does — the OUT position routes around it rather than halting it. That
    // keeps its state live, so a mode switched back in later carries the
    // current signal, not a stale charge from seconds ago.
    const auto lower =
        runSection(lowerSection_, filterPath, lowerG, controlLowerK_);
    switch (p.lowerMode)
    {
        case LowerFilterMode::BandPass:
            // Parametric boost: dry plus the resonant band-pass — a peak
            // without attenuation far from it, not a true band-pass.
            filterPath = filterPath + lower.bp;
            break;
        case LowerFilterMode::Overdrive:
        {
            // The soft clipper sits between the filters, so the upper
            // lowpass re-filters the distortion products. Knee and drive
            // voiced (OQ-10).
            const double boosted = filterPath + lower.bp;
            filterPath = 0.45 * std::tanh(6.0 * boosted);
            break;
        }
        case LowerFilterMode::HighPass:
            filterPath = lower.hp;
            break;
        case LowerFilterMode::Out:
            break;
    }

    // Upper filter: 24 dB cascades two sections (the first held at the LOW
    // Q; the second carries the control — voiced split, OQ-09); 12 dB is the
    // controlled section alone. The first section always advances so its
    // state stays live across slope switches, like the lower section's.
    const double upperFirstLp =
        runSection(upperFirst_, filterPath, upperG, 2.0).lp;
    if (p.slope == UpperSlope::TwentyFourDb)
        filterPath = upperFirstLp;
    filterPath =
        runSection(upperSecond_, filterPath, upperG, controlUpperK_).lp;

    filterPath *= controlLoudnessGain_;

    // -------------------------------------------------------- Shaper Y path
    double shaperPath = mixGain
        * (static_cast<double>(p.shaperPathA) * waveA
           + static_cast<double>(p.shaperPathB) * waveB
           + static_cast<double>(p.shaperPathRing) * ring
           + static_cast<double>(p.shaperPathNoise) * noise);
    brightnessState_ = flushDenormal(
        brightnessState_
        + controlBrightnessCoefficient_ * (shaperPath - brightnessState_));
    shaperPath = brightnessState_ * controlShaperVcaGain_;

    lastFilterPathSample_ = filterPath;
    lastShaperPathSample_ = shaperPath;
}

void GhostEngine::process(float* left, float* right, int numSamples)
{
    const double volume = static_cast<double>(parameters_.masterVolume)
                        * static_cast<double>(parameters_.masterVolume);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceControls();

        // Two internal steps per output sample, then the halfband picks the
        // decimated value.
        for (int step = 0; step < 2; ++step)
        {
            renderVoiceSample();
            filterRing_[static_cast<std::size_t>(decimatorIndex_)] =
                lastFilterPathSample_;
            shaperRing_[static_cast<std::size_t>(decimatorIndex_)] =
                lastShaperPathSample_;
            decimatorIndex_ = (decimatorIndex_ + 1) % halfbandTaps;
        }

        double filterOut = 0.0;
        double shaperOut = 0.0;
        int ringIndex = decimatorIndex_;
        for (int tap = halfbandTaps - 1; tap >= 0; --tap)
        {
            filterOut += halfbandKernel_[static_cast<std::size_t>(tap)]
                       * filterRing_[static_cast<std::size_t>(ringIndex)];
            shaperOut += halfbandKernel_[static_cast<std::size_t>(tap)]
                       * shaperRing_[static_cast<std::size_t>(ringIndex)];
            ringIndex = (ringIndex + 1) % halfbandTaps;
        }

        double outLeft;
        double outRight;
        if (parameters_.splitPaths)
        {
            outLeft = filterOut * volume;
            outRight = shaperOut * volume;
        }
        else
        {
            const double mixed = (filterOut + shaperOut) * volume;
            outLeft = mixed;
            outRight = mixed;
        }

        // Output AC coupling (~5 Hz), as the hardware's series capacitors
        // provide: rectangular duty-cycle DC must not reach the jack.
        const double dcR = 1.0 - 2.0 * pi * 5.0 / sampleRate_;
        const double coupledLeft =
            outLeft - dcPreviousInLeft_ + dcR * dcPreviousOutLeft_;
        dcPreviousInLeft_ = outLeft;
        dcPreviousOutLeft_ = flushDenormal(coupledLeft);
        const double coupledRight =
            outRight - dcPreviousInRight_ + dcR * dcPreviousOutRight_;
        dcPreviousInRight_ = outRight;
        dcPreviousOutRight_ = flushDenormal(coupledRight);

        left[sample] = static_cast<float>(coupledLeft);
        right[sample] = static_cast<float>(coupledRight);
    }
}

} // namespace ghost

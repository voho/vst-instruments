// Pre-research skeleton voice. Every law in this file is provisional and is
// replaced, block by block, as the researched circuit model lands; the tests
// therefore assert engine contracts (finiteness, gating, decay to silence)
// rather than any of these placeholder numbers.

#include "DSP/GhostEngine.h"

#include <algorithm>
#include <cmath>

namespace ghost
{
namespace
{
    constexpr double twoPi = 6.283185307179586476925286766559;

    [[nodiscard]] double midiToHz(double note) noexcept
    {
        return 440.0 * std::exp2((note - 69.0) / 12.0);
    }

    // Two-sample polynomial bandlimited step correction for the sawtooth's
    // reset discontinuity.
    [[nodiscard]] float polyBlep(double t, double dt) noexcept
    {
        if (t < dt)
        {
            const double x = t / dt;
            return static_cast<float>(x + x - x * x - 1.0);
        }
        if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            return static_cast<float>(x * x + x + x + 1.0);
        }
        return 0.0f;
    }

    [[nodiscard]] float flushDenormal(float value) noexcept
    {
        return std::abs(value) < 1.0e-20f ? 0.0f : value;
    }
} // namespace

GhostEngine::GhostEngine() noexcept = default;

void GhostEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    // std::clamp passes NaN through (its comparisons are all false), so a
    // host reporting a non-finite rate must be caught before the clamp or
    // every later division poisons the render.
    if (!std::isfinite(sampleRate))
        sampleRate = 44100.0;
    sampleRate_ = std::clamp(sampleRate, minimumSupportedSampleRate,
                             maximumSupportedSampleRate);
    reset();
}

void GhostEngine::reset()
{
    keyStackSize_ = 0;
    gateOpen_ = false;
    currentNote_ = -1;
    velocity_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheel_ = 0.0f;
    oscPhase_ = 0.0;
    envelope_ = Envelope {};
    ladder_ = Ladder {};
}

void GhostEngine::setParameters(const EngineParameters& parameters)
{
    // A NaN smuggled in through host automation or corrupted state would
    // otherwise reach pow() and the filter coefficients, and the poisoned
    // envelope/ladder state would outlive the next valid parameter set. Every
    // panel field is therefore normalised to a finite value in its documented
    // 0..1 travel before it is stored; a non-finite field falls back to its
    // power-on default.
    constexpr EngineParameters defaults {};
    const auto travel = [](float value, float fallback) noexcept {
        if (!std::isfinite(value))
            return fallback;
        return std::clamp(value, 0.0f, 1.0f);
    };

    EngineParameters sane = parameters;
    sane.cutoff = travel(parameters.cutoff, defaults.cutoff);
    sane.resonance = travel(parameters.resonance, defaults.resonance);
    sane.envToCutoff = travel(parameters.envToCutoff, defaults.envToCutoff);
    sane.attack = travel(parameters.attack, defaults.attack);
    sane.decay = travel(parameters.decay, defaults.decay);
    sane.sustain = travel(parameters.sustain, defaults.sustain);
    sane.release = travel(parameters.release, defaults.release);
    sane.volume = travel(parameters.volume, defaults.volume);
    parameters_ = sane;
}

void GhostEngine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;

    // Running status lets a MIDI sender encode Note Off as Note On with
    // velocity zero; treating it as a press would hold the gate open forever.
    if (!(velocity > 0.0f))
    {
        noteOff(midiNote);
        return;
    }

    // Re-pressing a held key moves it to the top of the stack rather than
    // duplicating it.
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
    // The stack spans the whole MIDI note domain and holds each note at most
    // once, so after the deduplication above it cannot be full here.
    keyStack_[static_cast<std::size_t>(keyStackSize_)] =
        static_cast<std::int16_t>(midiNote);
    ++keyStackSize_;

    currentNote_ = midiNote;
    velocity_ = std::clamp(velocity, 0.0f, 1.0f);
    gateOpen_ = true;
    envelope_.stage = Envelope::Stage::Attack;
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
        currentNote_ = keyStack_[static_cast<std::size_t>(keyStackSize_ - 1)];
        envelope_.stage = Envelope::Stage::Attack;
        return;
    }

    gateOpen_ = false;
    envelope_.stage = Envelope::Stage::Release;
}

void GhostEngine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBend_ = std::clamp(normalisedBipolar, -1.0f, 1.0f);
}

void GhostEngine::setModWheel(float amount) noexcept
{
    modWheel_ = std::clamp(amount, 0.0f, 1.0f);
}

void GhostEngine::process(float* left, float* right, int numSamples)
{
    const double dt = 1.0 / sampleRate_;

    // Provisional control laws, replaced by the researched panel model:
    // cutoff spans 20 Hz..16 kHz exponentially, resonance reaches loop gain 4
    // at full travel, envelope segments span 1 ms..10 s exponentially.
    const auto segmentSeconds = [](float travel) {
        return 0.001 * std::pow(10000.0, static_cast<double>(travel));
    };
    const double attackSeconds = segmentSeconds(parameters_.attack);
    const double decaySeconds = segmentSeconds(parameters_.decay);
    const double releaseSeconds = segmentSeconds(parameters_.release);
    const float sustain = std::clamp(parameters_.sustain, 0.0f, 1.0f);

    const double attackCoefficient = 1.0 - std::exp(-dt / (attackSeconds / 3.0));
    const double decayCoefficient = 1.0 - std::exp(-dt / (decaySeconds / 3.0));
    const double releaseCoefficient =
        1.0 - std::exp(-dt / (releaseSeconds / 3.0));

    // The top MIDI notes exceed Nyquist at the lowest supported host rates
    // (note 127 is ~12.5 kHz against an 8 kHz host), which would push the
    // phase step past a whole cycle per sample. The oscillator is bounded
    // below Nyquist instead, where the PolyBLEP correction's assumptions
    // hold.
    const double maximumOscillatorHz = 0.45 * sampleRate_;
    const double baseHz =
        currentNote_ >= 0
            ? std::min(midiToHz(static_cast<double>(currentNote_)
                                + 2.0 * static_cast<double>(pitchBend_)),
                       maximumOscillatorHz)
            : 0.0;

    const double cutoffHz =
        20.0 * std::pow(800.0, static_cast<double>(
                                   std::clamp(parameters_.cutoff, 0.0f, 1.0f)));
    const double g =
        std::tan(std::min(0.49 * sampleRate_, cutoffHz) * dt * (twoPi / 2.0));
    const double k =
        4.0 * static_cast<double>(std::clamp(parameters_.resonance, 0.0f, 1.0f));

    const float amplitude =
        velocity_ * std::clamp(parameters_.volume, 0.0f, 1.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        switch (envelope_.stage)
        {
            case Envelope::Stage::Attack:
                envelope_.level += static_cast<float>(
                    (1.12 - envelope_.level) * attackCoefficient);
                if (envelope_.level >= 1.0f)
                {
                    envelope_.level = 1.0f;
                    envelope_.stage = Envelope::Stage::Decay;
                }
                break;
            case Envelope::Stage::Decay:
                envelope_.level += static_cast<float>(
                    (sustain - envelope_.level) * decayCoefficient);
                break;
            case Envelope::Stage::Release:
                envelope_.level +=
                    static_cast<float>((0.0 - envelope_.level) * releaseCoefficient);
                if (envelope_.level < 1.0e-5f)
                {
                    envelope_.level = 0.0f;
                    envelope_.stage = Envelope::Stage::Idle;
                }
                break;
            case Envelope::Stage::Idle:
                break;
        }

        float saw = 0.0f;
        if (baseHz > 0.0 && envelope_.stage != Envelope::Stage::Idle)
        {
            const double phaseStep = baseHz * dt;
            saw = static_cast<float>(2.0 * oscPhase_ - 1.0)
                - polyBlep(oscPhase_, phaseStep);
            oscPhase_ += phaseStep;
            oscPhase_ -= std::floor(oscPhase_);
        }

        // Linear zero-delay-feedback ladder; the researched filter model
        // replaces this with the instrument's actual topology.
        const double envCutoffScale = 1.0
            + 4.0 * static_cast<double>(parameters_.envToCutoff)
                  * static_cast<double>(envelope_.level);
        const double gm = std::min(g * envCutoffScale, 1.0);
        const double a = gm / (1.0 + gm);
        double s = 0.0;
        {
            const auto& st = ladder_.state;
            const double a2 = a * a;
            s = (a2 * a * st[0] + a2 * st[1] + a * st[2] + st[3]) / (1.0 + gm);
        }
        const double input = static_cast<double>(std::tanh(1.5f * saw));
        const double u = (input - k * s) / (1.0 + k * a * a * a * a);
        double stageIn = u;
        for (auto& state : ladder_.state)
        {
            const double v = a * (stageIn - static_cast<double>(state));
            const double y = v + static_cast<double>(state);
            state = flushDenormal(static_cast<float>(y + v));
            stageIn = y;
        }
        const float filtered = static_cast<float>(stageIn);

        const float out = flushDenormal(filtered * envelope_.level * amplitude);
        left[sample] = out;
        right[sample] = out;
    }
}

} // namespace ghost

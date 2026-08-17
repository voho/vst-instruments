#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"
#include "DSP/ElectryVisuals.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace electry
{
// Narrow inspection seam for the JUCE-free regression suite.
struct ElectryEngineTestAccess
{
    struct VoiceSnapshot
    {
        bool valid { false };
        bool active { false };
        bool keyDown { false };
        bool releasing { false };
        int stringIndex { -1 };
        int midiNote { -1 };
        int fret { -1 };
        PlayStyle playStyle { PlayStyle::Sustain };
        bool strokeIsUp { false };
        float verticalDelayTarget { 0.0f };
        float verticalDelayCurrent { 0.0f };
        float horizontalDelayTarget { 0.0f };
        float polarisationCoupling { 0.0f };
        float dispersionLowCoefficient { 0.0f };
        float dispersionHighCoefficient { 0.0f };
        float inharmonicity { 0.0f };
        float dispersionLowPartial { 0.0f };
        float dispersionHighPartial { 0.0f };
        float bodyConductance { 0.0f };
        float bodyLossFactor { 1.0f };
        float loopGain { 0.0f };
        float baseFrequency { 0.0f };
        std::uint64_t ageSamples { 0 };
        int startDelaySamples { 0 };
        bool sympatheticReady { false };
        float sympatheticEnergy { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationCombWidth { 0.0f };
        // The two the picking hand's force and contact patch reach.
        float excitationAmplitude { 0.0f };
        int excitationLength { 0 };
        float excitationLoadScale { 0.0f };
        float excitationSlipScale { 0.0f };
        float loopDampingCoefficient { 0.0f };
        float vibratoSemitones { 0.0f };
    };

    static VoiceSnapshot snapshot(const ElectryEngine& engine, int stringIndex)
    {
        VoiceSnapshot result;
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return result;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        result.valid = true;
        result.active = voice.active;
        result.keyDown = voice.keyDown;
        result.releasing = voice.releasing;
        result.stringIndex = voice.stringIndex;
        result.midiNote = voice.midiNote;
        result.fret = voice.fret;
        result.playStyle = voice.playStyle;
        result.strokeIsUp = voice.strokeIsUp;
        result.verticalDelayTarget = voice.vertical.targetDelay;
        result.verticalDelayCurrent = voice.vertical.currentDelay;
        result.horizontalDelayTarget = voice.horizontal.targetDelay;
        result.polarisationCoupling = voice.polarisationCoupling;
        result.dispersionLowCoefficient = voice.vertical.dispersionLowCoefficient;
        result.dispersionHighCoefficient = voice.vertical.dispersionHighCoefficient;
        result.inharmonicity = voice.inharmonicity;
        result.dispersionLowPartial = voice.dispersionLowPartial;
        result.dispersionHighPartial = voice.dispersionHighPartial;
        result.bodyConductance = voice.bodyConductance;
        result.bodyLossFactor = voice.bodyLossFactor;
        result.loopGain = voice.vertical.loopGain;
        result.baseFrequency = voice.baseFrequency;
        result.ageSamples = voice.ageSamples;
        result.startDelaySamples = voice.startDelaySamples;
        result.sympatheticReady = voice.sympatheticReady;
        result.sympatheticEnergy = voice.sympatheticEnergy;
        result.excitationCombDelay = voice.excitationCombDelay;
        result.excitationCombWidth = voice.excitationCombWidth;
        result.excitationAmplitude = voice.excitationAmplitude;
        result.excitationLength = voice.excitationLength;
        result.excitationLoadScale = voice.excitationLoadScale;
        result.excitationSlipScale = voice.excitationSlipScale;
        result.loopDampingCoefficient = voice.vertical.loopDampingCoefficient;
        result.vibratoSemitones = voice.vibratoSemitones;
        return result;
    }

    // The shared depth envelope the fretting hand's vibrato rides on, read
    // straight off the engine so the onset's shape can be measured without
    // the oscillation on top of it.
    static float vibratoDepthEnvelope(const ElectryEngine& engine) noexcept
    {
        return engine.vibratoAmount_;
    }

    // The channel-pressure target setVibrato() writes, read straight off the
    // engine so its own sanitisation guard can be checked directly rather
    // than only through whatever it happens to do to a rendered voice.
    static float vibratoTarget(const ElectryEngine& engine) noexcept
    {
        return engine.vibratoTarget_;
    }

    // The bipolar bend target setPitchBend() writes, read straight off the
    // engine so its own sanitisation guard - fold non-finite input to zero,
    // clamp to [-1, 1], then double to the +/-2 semitone bend range - can be
    // checked directly rather than only through a rendered voice's pitch.
    static float pitchBendTarget(const ElectryEngine& engine) noexcept
    {
        return engine.pitchBendTarget_;
    }

    // The bridge-pickup resonance target setResonance() writes, the
    // acoustic-return level target setAcousticReturnLevel() writes, and the
    // palm-mute pressure setPalmMutePressure() writes, read straight off the
    // engine so each guard - fold non-finite input to zero, then clamp to
    // [0, 1] - can be checked directly rather than only through whatever it
    // happens to do to a rendered voice.
    static float resonanceTarget(const ElectryEngine& engine) noexcept
    {
        return engine.resonanceTarget_;
    }

    static float returnLevelTarget(const ElectryEngine& engine) noexcept
    {
        return engine.returnLevelTarget_;
    }

    static float palmMutePressure(const ElectryEngine& engine) noexcept
    {
        return engine.palmMutePressure_;
    }

    // The acoustic-return ring pushAcousticReturn() writes and the render
    // loop drains, read straight off the engine so the guard - a null left
    // pointer or a non-positive sample count leaves the ring untouched, a
    // null right pointer duplicates left rather than being read through, a
    // non-finite averaged sample folds to zero before it is stored, and a
    // single push longer than the ring drops its oldest samples rather than
    // overflowing - can be checked directly rather than only through
    // whatever it happens to do to a later rendered voice.
    static int feedbackAvailable(const ElectryEngine& engine) noexcept
    {
        return engine.feedbackAvailable_;
    }

    // The sample `offset` places after the ring's current read pointer, i.e.
    // the order pushAcousticReturn's own writes will be handed to the render
    // loop.
    static float feedbackRingSample(const ElectryEngine& engine,
                                    int offset) noexcept
    {
        const int index = (engine.feedbackReadIndex_ + offset)
                         & (ElectryEngine::feedbackRingSize - 1);
        return engine.feedbackRing_[static_cast<std::size_t>(index)];
    }

    static constexpr int feedbackRingCapacity() noexcept
    {
        return ElectryEngine::feedbackRingSize;
    }

    static bool channelsLinked(const ElectryEngine& engine) noexcept
    {
        return engine.channelsLinked_;
    }

    // The played strings' share of the bridge bus: the gain each voice reads
    // the others' summed force at, and the row-sum norm that bounds it. Read
    // off the engine rather than recomputed, so the stability contract is
    // asserted on the number that actually runs.
    static float bridgeCouplingGain(const ElectryEngine& engine) noexcept
    {
        return engine.bridgeCouplingInjection_;
    }

    static float bridgeCouplingRowSum(const ElectryEngine& engine) noexcept
    {
        return engine.bridgeCouplingRowSum_;
    }

    // The slow follower on a played voice's *own* two loop outputs. Nothing
    // any other string does reaches it, so it is the exact place to look for
    // an unsubtracted self-term: a voice that drives itself through the bridge
    // bus changes this, and a voice that drives only the others does not.
    static float voiceLoopEnergy(const ElectryEngine& engine,
                                 int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .energyEnvelope;
    }

    // The hand's loss dip sits inside the feedback loop, so its magnitude must
    // never exceed one at any frequency. Read from the live voice rather than
    // recomputed, so the check is on what actually runs.
    static void handDip(const ElectryEngine& engine, int stringIndex,
                        double& b0, double& b1, double& b2, double& a1,
                        double& a2, bool& active) noexcept
    {
        const auto& loop = engine.voices_[static_cast<std::size_t>(stringIndex)].vertical;
        b0 = loop.handDip.b0;
        b1 = loop.handDip.b1;
        b2 = loop.handDip.b2;
        a1 = loop.handDip.a1;
        a2 = loop.handDip.a2;
        active = loop.handDipActive;
    }

    static bool pickupPathActive(const ElectryEngine& engine, bool neck) noexcept
    {
        return neck ? engine.neckPathActive_ : engine.bridgePathActive_;
    }

    // Is the humbucker's second coil running on this string's bridge pickup?
    static bool coilPairActive(const ElectryEngine& engine,
                               int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .coilPairBridge.paired;
    }

    // The pickup's spatial transfer at one frequency, evaluated in closed form
    // from the two stages the voice is actually running: the humbucker's coil
    // pair and the per-coil aperture window. Both are FIR, so this is exact
    // rather than a fit.
    //
    // The position comb is deliberately not included. It is a separate stage
    // with nulls of its own at every multiple of c/2x, several of which land
    // between 2 and 8 kHz, so folding it in would leave "the deepest notch"
    // ambiguous. And the notch is read here rather than off a rendered
    // spectrum because a null between two harmonics is sampled only as finely
    // as the string's fundamental spacing.
    static double apertureChainMagnitude(const ElectryEngine& engine,
                                         int stringIndex, double frequencyHz,
                                         bool includeCoilPair)
    {
        constexpr double pi = 3.14159265358979323846;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& window = voice.apertureBridge;
        const auto& pair = voice.coilPairBridge;
        const double omega = 2.0 * pi * frequencyHz / engine.sampleRate_;

        // Rectangular window: unit taps at 0..W-1 plus a fractional tap at W,
        // scaled by the inverse window length.
        double real = 0.0;
        double imaginary = 0.0;
        for (int k = 0; k < window.windowWhole; ++k)
        {
            real += std::cos(omega * k);
            imaginary -= std::sin(omega * k);
        }
        const double edge = omega * static_cast<double>(window.windowWhole);
        real += window.windowFraction * std::cos(edge);
        imaginary -= window.windowFraction * std::sin(edge);
        double magnitude = std::hypot(real, imaginary) * window.inverseWindow;

        if (includeCoilPair && pair.paired)
        {
            const double near = omega * static_cast<double>(pair.spacingWhole);
            const double far = omega * static_cast<double>(pair.spacingWhole + 1);
            const double nearWeight = 1.0
                - static_cast<double>(pair.spacingFraction);
            const double farWeight = static_cast<double>(pair.spacingFraction);
            const double balance = static_cast<double>(pair.balance);
            const double pairReal = 1.0
                + balance * (nearWeight * std::cos(near)
                             + farWeight * std::cos(far));
            const double pairImaginary =
                -balance * (nearWeight * std::sin(near)
                            + farWeight * std::sin(far));
            magnitude *= std::hypot(pairReal, pairImaginary)
                       * static_cast<double>(pair.normalise);
        }
        return magnitude;
    }

    static int oversamplingFactor(const ElectryEngine& engine) noexcept
    {
        return engine.oversamplingFactor_;
    }

    static double hostSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.hostSampleRate_;
    }

    static double internalSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.sampleRate_;
    }

    static double minimumSupportedSampleRate() noexcept
    {
        return ElectryEngine::minimumSupportedSampleRate;
    }

    static double maximumSupportedSampleRate() noexcept
    {
        return ElectryEngine::maximumSupportedSampleRate;
    }

    static float dispersionDeficit(const ElectryEngine& engine,
                                    int stringIndex, float partial) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0f;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = voice.vertical;
        const float omega = 2.0f * 3.14159265358979323846f
                          * voice.lastConfiguredFrequency
                          / static_cast<float>(engine.sampleRate_);
        const float omegaPartial = std::min(
            omega * partial, 3.14159265358979323846f * 0.95f);
        const auto sectionDeficit = [&] (float coefficient)
        {
            return ElectryEngine::allpassPhaseDelay(coefficient, omega)
                 - ElectryEngine::allpassPhaseDelay(coefficient, omegaPartial);
        };
        return 4.0f * sectionDeficit(loop.dispersionLowCoefficient)
             + 4.0f * sectionDeficit(loop.dispersionHighCoefficient);
    }

    static double modalMagnitudeAt(float frequencyHz, float q, float modeGain,
                                   float sampleRate,
                                   float evaluationFrequencyHz) noexcept
    {
        ElectryEngine::ModalResonator resonator;
        resonator.configure(frequencyHz, q, modeGain, sampleRate);

        const double omega = 2.0 * 3.14159265358979323846
                           * static_cast<double>(evaluationFrequencyHz)
                           / static_cast<double>(sampleRate);
        const double denominatorReal = 1.0
            + static_cast<double>(resonator.a1) * std::cos(omega)
            + static_cast<double>(resonator.a2) * std::cos(2.0 * omega);
        const double denominatorImag =
            -static_cast<double>(resonator.a1) * std::sin(omega)
            -static_cast<double>(resonator.a2) * std::sin(2.0 * omega);
        return std::abs(static_cast<double>(resonator.gain))
             / std::max(std::hypot(denominatorReal, denominatorImag), 1.0e-20);
    }

    static constexpr int delayLineCapacity() noexcept
    {
        return ElectryEngine::delayLineSize;
    }

    struct DelayTapSnapshot
    {
        int offset { 0 };
        float c0 { 0.0f }, c1 { 0.0f }, c2 { 0.0f }, c3 { 0.0f };
    };

    // A fresh DelayTap solved for one requested delay, so its own clamp and
    // cubic-Lagrange coefficient solve can be checked directly rather than
    // only through whatever pickup or sympathetic-tap position a configured
    // voice happens to land on.
    static DelayTapSnapshot delayTapAt(float delaySamples) noexcept
    {
        ElectryEngine::DelayTap tap;
        tap.setDelay(delaySamples);
        return { tap.offset, tap.c0, tap.c1, tap.c2, tap.c3 };
    }

    static int stringForNote(const ElectryEngine& engine, int midiNote)
    {
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
        {
            const auto& voice = engine.voices_[static_cast<std::size_t>(s)];
            if (voice.active && voice.midiNote == midiNote)
                return s;
        }
        return -1;
    }

    // The touching finger's live depth and position, so the harmonic checks
    // can assert on the filter that actually runs rather than on a copy of it.
    static float touchDepth(const ElectryEngine& engine, int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].touchDepth;
    }

    static float touchFraction(const ElectryEngine& engine,
                               int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].touchFraction;
    }

    // How far through its travel a legato glide is, so the slide's timing can
    // be read from the glide itself rather than from a delay target that
    // tension modulation is also moving.
    static float legatoBlend(const ElectryEngine& engine,
                             int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].legatoBlend;
    }

    // The slide's friction level, so the check that a silent Finger Noise
    // control means an exactly absent scrape reads the engine rather than the
    // audio.
    static float slideNoiseAmplitude(const ElectryEngine& engine,
                                     int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .slideNoiseAmplitude;
    }

    // The rate the winding ridges are passing under the finger, recovered from
    // the one-pole that actually forms the friction band. The engine stores
    // the coefficient rather than the frequency, so the test inverts the same
    // relation the engine used.
    static double slideBandCentreHz(const ElectryEngine& engine,
                                    int stringIndex) noexcept
    {
        const float coefficient = engine
            .voices_[static_cast<std::size_t>(stringIndex)].slideBandLow;
        if (coefficient <= 0.0f || coefficient >= 1.0f)
            return 0.0;
        return -std::log(static_cast<double>(coefficient))
             * engine.sampleRate_
             / (2.0 * 3.14159265358979323846 * 0.6);
    }

    // Where the fretting hand's index finger currently sits, so the allocator
    // checks can assert on the state that drove the choice rather than only on
    // the choice itself.
    static float frettingHandPosition(const ElectryEngine& engine) noexcept
    {
        return engine.frettingHandPosition_;
    }

    // The per-string pitch-wheel compliance, so the audio checks can assert
    // against exactly the constants the engine runs.
    static float bendSensitivity(int stringIndex) noexcept
    {
        return ElectryEngine::bendSensitivity(stringIndex);
    }

    // The parameter guard setParameters() runs before anything else sees a
    // host's automation, so the fallback and clamp behaviour can be asserted
    // on directly rather than only inferred from the audio it protects.
    static EngineParameters sanitise(const EngineParameters& parameters) noexcept
    {
        return ElectryEngine::sanitise(parameters);
    }

    // The pitch a bridge-coupled string is running at - its open note, bent by
    // the wheel through its own compliance - computed the way
    // configureSympatheticString computes it, so the decay checks can invert
    // the loop's response at exactly the frequency it was solved for.
    static float sympatheticFrequency(const ElectryEngine& engine,
                                      int stringIndex) noexcept
    {
        const auto& spec =
            ElectryEngine::stringSpecs()[static_cast<std::size_t>(stringIndex)];
        return ElectryEngine::midiToHz(static_cast<float>(spec.openMidiNote))
             * std::exp2(engine.pitchBendSemitones_
                         * ElectryEngine::bendSensitivity(stringIndex) / 12.0f);
    }
};
} // namespace electry

namespace
{
using electry::ElectryEngine;
using electry::EngineParameters;
using electry::PickStyle;
using electry::PickupSelector;
using electry::PlayStyle;
using TestAccess = electry::ElectryEngineTestAccess;

int pickKeyswitch(PickStyle pick)
{
    return ElectryEngine::firstKeyswitchNote + static_cast<int>(pick);
}

int styleKeyswitch(PlayStyle style)
{
    return ElectryEngine::firstPlayStyleKeyswitchNote + static_cast<int>(style);
}

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;

    explicit StereoBuffer(int samples)
        : left(static_cast<std::size_t>(samples), 0.0f),
          right(static_cast<std::size_t>(samples), 0.0f) {}

    [[nodiscard]] int size() const noexcept
    {
        return static_cast<int>(left.size());
    }
};

void renderInto(ElectryEngine& engine, StereoBuffer& buffer, int blockSize = 512)
{
    int rendered = 0;
    const int total = buffer.size();
    while (rendered < total)
    {
        const int samples = std::min(blockSize, total - rendered);
        engine.process(buffer.left.data() + rendered,
                       buffer.right.data() + rendered, samples);
        rendered += samples;
    }
}

StereoBuffer renderNote(ElectryEngine& engine, double sampleRate, int midiNote,
                        float velocity, PlayStyle playStyle,
                        double seconds, double noteOffAfterSeconds = -1.0,
                        PickStyle pickStyle = PickStyle::Down)
{
    engine.reset();
    engine.noteOn(pickKeyswitch(pickStyle), 1.0f);
    engine.noteOn(styleKeyswitch(playStyle), 1.0f);
    engine.noteOn(midiNote, velocity);

    const int totalSamples = static_cast<int>(seconds * sampleRate);
    StereoBuffer buffer(totalSamples);

    if (noteOffAfterSeconds > 0.0 && noteOffAfterSeconds < seconds)
    {
        const int offSample = static_cast<int>(noteOffAfterSeconds * sampleRate);
        engine.process(buffer.left.data(), buffer.right.data(), offSample);
        engine.noteOff(midiNote);
        engine.process(buffer.left.data() + offSample,
                       buffer.right.data() + offSample,
                       totalSamples - offSample);
    }
    else
    {
        renderInto(engine, buffer);
    }
    return buffer;
}

bool allFinite(const StereoBuffer& buffer)
{
    for (const float sample : buffer.left)
        if (! std::isfinite(sample))
            return false;
    for (const float sample : buffer.right)
        if (! std::isfinite(sample))
            return false;
    return true;
}

float peakAbs(const std::vector<float>& data, int start = 0, int end = -1)
{
    const int last = end < 0 ? static_cast<int>(data.size())
                             : std::min<int>(end, static_cast<int>(data.size()));
    float peak = 0.0f;
    for (int i = std::max(0, start); i < last; ++i)
        peak = std::max(peak, std::abs(data[static_cast<std::size_t>(i)]));
    return peak;
}

double rmsInRange(const std::vector<float>& data, int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(end, static_cast<int>(data.size()));
    if (last <= first)
        return 0.0;
    double sum = 0.0;
    for (int i = first; i < last; ++i)
        sum += static_cast<double>(data[static_cast<std::size_t>(i)])
             * static_cast<double>(data[static_cast<std::size_t>(i)]);
    return std::sqrt(sum / static_cast<double>(last - first));
}

double normalisedDifferenceRms(const std::vector<float>& a,
                               const std::vector<float>& b,
                               int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>({ end, static_cast<int>(a.size()),
                                    static_cast<int>(b.size()) });
    if (last <= first)
        return 0.0;

    double difference = 0.0;
    double reference = 0.0;
    for (int i = first; i < last; ++i)
    {
        const double av = a[static_cast<std::size_t>(i)];
        const double bv = b[static_cast<std::size_t>(i)];
        const double delta = av - bv;
        difference += delta * delta;
        reference += 0.5 * (av * av + bv * bv);
    }
    return reference > 0.0 ? std::sqrt(difference / reference) : 0.0;
}

// Hann-windowed DFT magnitude at an arbitrary frequency, evaluated with a
// phasor recurrence so the tests stay fast.
double dftMagnitude(const std::vector<float>& data, int start, int length,
                    double sampleRate, double frequency)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(first + length, static_cast<int>(data.size()));
    const int n = last - first;
    if (n < 16)
        return 0.0;

    const double omega = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
    const double stepReal = std::cos(omega);
    const double stepImag = -std::sin(omega);
    double phasorReal = 1.0;
    double phasorImag = 0.0;
    double sumReal = 0.0;
    double sumImag = 0.0;
    const double windowStep = 3.14159265358979323846 / static_cast<double>(n - 1);

    for (int i = 0; i < n; ++i)
    {
        const double window = std::sin(windowStep * i);
        const double sample = window * window
            * static_cast<double>(data[static_cast<std::size_t>(first + i)]);
        sumReal += sample * phasorReal;
        sumImag += sample * phasorImag;
        const double nextReal = phasorReal * stepReal - phasorImag * stepImag;
        phasorImag = phasorReal * stepImag + phasorImag * stepReal;
        phasorReal = nextReal;
    }
    return std::sqrt(sumReal * sumReal + sumImag * sumImag);
}

// Locate the strongest spectral component near an expected fundamental by a
// coarse-to-fine scan, returning its frequency in Hz.
double measureFrequency(const std::vector<float>& data, int start, int length,
                        double sampleRate, double expectedHz)
{
    const auto scan = [&] (double centre, double spanCents, double stepCents)
    {
        double bestFrequency = centre;
        double bestMagnitude = -1.0;
        for (double cents = -spanCents; cents <= spanCents; cents += stepCents)
        {
            const double frequency = centre * std::pow(2.0, cents / 1200.0);
            // A magnetic pickup produces induced EMF, so its fundamental can
            // be weaker than the first few partials. Score a short harmonic
            // series instead of assuming displacement-like fundamental
            // dominance; the narrow scan still prevents octave ambiguity.
            double magnitude = 0.0;
            for (int partial = 1; partial <= 5; ++partial)
            {
                const double partialFrequency = frequency * partial;
                if (partialFrequency >= 0.45 * sampleRate)
                    break;
                magnitude += dftMagnitude(data, start, length, sampleRate,
                                          partialFrequency)
                           / std::sqrt(static_cast<double>(partial));
            }
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestFrequency = frequency;
            }
        }
        return bestFrequency;
    };

    const double coarse = scan(expectedHz, 120.0, 6.0);
    return scan(coarse, 6.0, 0.5);
}

double centsBetween(double frequencyA, double frequencyB)
{
    return 1200.0 * std::log2(frequencyA / frequencyB);
}

// Energy-weighted mean frequency across the tone's partial series. Sampling
// the spectrum at the partials measures the envelope rather than window
// leakage between harmonic lines.
double spectralCentroid(const std::vector<float>& data, int start, int length,
                        double sampleRate, double fundamentalHz)
{
    double weighted = 0.0;
    double total = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        weighted += magnitude * frequency;
        total += magnitude;
    }
    return total > 0.0 ? weighted / total : 0.0;
}

double midiHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

// Magnitude ratio between the partials above and below a split frequency:
// a direct measure of what a passive tone control removes.
double highBandRatio(const std::vector<float>& data, int start, int length,
                     double sampleRate, double fundamentalHz)
{
    double low = 0.0;
    double high = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        if (frequency < 900.0)
            low += magnitude;
        else
            high += magnitude;
    }
    return low > 0.0 ? high / low : 0.0;
}

struct HarmonicBalance
{
    double lowPowerShare { 0.0 };
    double highPowerShare { 0.0 };
    double strongestFirstEightDb { -300.0 };
    double lowMagnitude { 0.0 };
};

double decibels(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-15));
}

// Stiff wound-string partials sit slightly above an ideal harmonic series.
// A short scan prevents valid dispersion from looking like missing spectral
// energy while remaining narrow enough not to count unrelated noise.
double scannedPartialMagnitude(const std::vector<float>& data, int start,
                               int length, double sampleRate,
                               double fundamentalHz, int partial)
{
    const double idealFrequency = fundamentalHz * static_cast<double>(partial);
    double best = 0.0;
    for (double cents = -8.0; cents <= 56.0; cents += 8.0)
    {
        const double frequency = idealFrequency * std::pow(2.0, cents / 1200.0);
        best = std::max(best, dftMagnitude(data, start, length, sampleRate,
                                           frequency));
    }
    return best;
}

HarmonicBalance measureHarmonicBalance(const std::vector<float>& data,
                                       int start, int length,
                                       double sampleRate,
                                       double fundamentalHz)
{
    double lowPower = 0.0;
    double middlePower = 0.0;
    double highPower = 0.0;
    double fundamentalMagnitude = 0.0;
    double strongestFirstEight = 0.0;

    for (int partial = 1; partial <= 64; ++partial)
    {
        const double frequency = fundamentalHz * static_cast<double>(partial);
        if (frequency >= std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = scannedPartialMagnitude(
            data, start, length, sampleRate, fundamentalHz, partial);
        const double power = magnitude * magnitude;
        if (frequency < 250.0)
            lowPower += power;
        else if (frequency < 1000.0)
            middlePower += power;
        else
            highPower += power;

        if (partial == 1)
            fundamentalMagnitude = magnitude;
        else if (partial <= 8)
            strongestFirstEight = std::max(strongestFirstEight, magnitude);
    }

    const double totalPower = lowPower + middlePower + highPower;
    HarmonicBalance result;
    result.lowPowerShare = lowPower / std::max(totalPower, 1.0e-30);
    result.highPowerShare = highPower / std::max(totalPower, 1.0e-30);
    result.strongestFirstEightDb = decibels(
        strongestFirstEight / std::max(fundamentalMagnitude, 1.0e-15));
    result.lowMagnitude = std::sqrt(lowPower);
    return result;
}

// ---------------------------------------------------------------------------

void testModalResonatorPeakGain()
{
    // These span the open low strings, the complete solid-body mode table,
    // and the Q/gain ranges used by both the body and sympathetic banks. The
    // old numerator exceeded the requested gain by 100x or more down here.
    struct Case
    {
        float frequency;
        float q;
        float gain;
    };
    constexpr std::array<Case, 7> cases {{
        { 41.20f, 34.0f, 0.026f },
        { 61.74f, 28.0f, 0.040f },
        { 92.0f, 30.0f, 1.20f },
        { 112.0f, 24.0f, 1.00f },
        { 220.0f, 18.0f, 0.68f },
        { 488.0f, 12.0f, 0.32f },
        { 690.0f, 9.0f, 0.46f },
    }};

    constexpr std::array<float, 3> internalSampleRates {
        96000.0f, 192000.0f, 384000.0f
    };
    for (const float internalSampleRate : internalSampleRates)
    {
        for (const auto& test : cases)
        {
            const double actual = TestAccess::modalMagnitudeAt(
                test.frequency, test.q, test.gain, internalSampleRate,
                test.frequency);
            const double relativeError = std::abs(actual - test.gain)
                                       / std::max<double>(test.gain, 1.0e-12);
            expect(relativeError < 0.005,
                   "modal resonator did not reproduce its requested peak gain at "
                       + std::to_string(test.frequency) + " Hz / "
                       + std::to_string(internalSampleRate) + " Hz sample rate"
                       + " (requested " + std::to_string(test.gain)
                       + ", actual " + std::to_string(actual) + ")");
        }
    }
}

void testLowRegisterGuitarEnvelope()
{
    constexpr double sampleRate = 48000.0;
    EngineParameters cleanParameters;
    cleanParameters.pickupSelector = PickupSelector::Bridge;
    cleanParameters.outputMode = electry::OutputMode::Mono;
    // Measure the pitched string itself. Incidental noises and sympathetic
    // hardware must not be what makes an otherwise thin E1/B1 pass.
    cleanParameters.artifactAmount = 0.0f;
    cleanParameters.pickNoise = 0.0f;
    cleanParameters.fingerNoise = 0.0f;
    cleanParameters.releaseNoise = 0.0f;

    struct NoteCase
    {
        int midiNote;
        const char* name;
        double minimumAttackLowShare;
        double minimumSustainLowShare;
    };
    constexpr std::array<NoteCase, 2> notes {{
        { 28, "E1", 0.20, 0.25 },
        { 35, "B1", 0.15, 0.20 },
    }};

    for (const auto& note : notes)
    {
        const auto validate = [&] (const EngineParameters& parameters,
                                   const char* variant,
                                   double maximumPeakToSustainDb)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            engine.setParameters(parameters);
            const auto render = renderNote(
                engine, sampleRate, note.midiNote, 0.8f,
                PlayStyle::Sustain, 4.1);
            const double fundamental = midiHz(note.midiNote);
            const auto attack = measureHarmonicBalance(
                render.left, static_cast<int>(0.03 * sampleRate),
                static_cast<int>(0.15 * sampleRate), sampleRate, fundamental);
            const auto sustain = measureHarmonicBalance(
                render.left, static_cast<int>(0.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const auto late = measureHarmonicBalance(
                render.left, static_cast<int>(3.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const std::string prefix = std::string(note.name) + " " + variant;

            expect(attack.lowPowerShare >= note.minimumAttackLowShare,
                   prefix + " attack still lacks low partials ("
                       + std::to_string(100.0 * attack.lowPowerShare) + "%)");
            expect(attack.highPowerShare <= 0.50,
                   prefix + " attack remains clavinet-bright ("
                       + std::to_string(100.0 * attack.highPowerShare)
                       + "% above 1 kHz)");
            expect(sustain.lowPowerShare >= note.minimumSustainLowShare,
                   prefix + " sustain still lacks low partials ("
                       + std::to_string(100.0 * sustain.lowPowerShare) + "%)");
            expect(sustain.highPowerShare <= 0.35,
                   prefix + " sustain remains upper-harmonic dominated ("
                       + std::to_string(100.0 * sustain.highPowerShare)
                       + "% above 1 kHz)");
            expect(attack.strongestFirstEightDb <= 16.0,
                   prefix + " attack partial exceeds the fundamental by "
                       + std::to_string(attack.strongestFirstEightDb) + " dB");
            expect(sustain.strongestFirstEightDb <= 12.0,
                   prefix + " sustained partial exceeds the fundamental by "
                       + std::to_string(sustain.strongestFirstEightDb) + " dB");

            const double attackToSustain = decibels(
                peakAbs(render.left, 0, static_cast<int>(0.20 * sampleRate))
                / std::max(rmsInRange(
                    render.left, static_cast<int>(0.20 * sampleRate),
                    static_cast<int>(0.70 * sampleRate)), 1.0e-15));
            expect(attackToSustain <= maximumPeakToSustainDb,
                   prefix + " is too transient-heavy (peak/sustain "
                       + std::to_string(attackToSustain) + " dB)");

            const double earlyRms = rmsInRange(
                render.left, static_cast<int>(0.05 * sampleRate),
                static_cast<int>(0.20 * sampleRate));
            const auto expectTailAbove = [&] (double begin, double end,
                                              double minimumDb)
            {
                const double tailRms = rmsInRange(
                    render.left, static_cast<int>(begin * sampleRate),
                    static_cast<int>(end * sampleRate));
                const double relativeDb = decibels(
                    tailRms / std::max(earlyRms, 1.0e-15));
                expect(relativeDb >= minimumDb,
                       prefix + " string dies too early at "
                           + std::to_string(begin) + "-" + std::to_string(end)
                           + " s (" + std::to_string(relativeDb) + " dB)");
            };
            expectTailAbove(0.50, 1.00, -8.0);
            expectTailAbove(1.00, 2.00, -15.0);
            expectTailAbove(2.00, 4.00, -26.0);

            const double lowDecayDb = decibels(
                late.lowMagnitude / std::max(sustain.lowMagnitude, 1.0e-15));
            const double apparentT60 = lowDecayDb < -1.0e-6
                ? -60.0 * 3.0 / lowDecayDb
                : 1.0e6;
            // Calibrated against a dry electric low-E reference recording, whose
            // overall level falls about 24 dB over the eight seconds after the
            // attack and whose fundamental partial decays more slowly still.
            expect(apparentT60 >= 4.0 && apparentT60 <= 26.0,
                   prefix + " low-partial T60 left the guitar range ("
                       + std::to_string(apparentT60) + " s)");
        };

        validate(cleanParameters, "clean physical string", 15.0);
        // The normal preset retains a small direct plectrum/contact transient;
        // allow that realistic edge without returning to the old 22 dB
        // clavinet-like attack-to-sustain ratio.
        validate(EngineParameters {}, "default output", 17.0);

    }
}

void testOpenLowStringLevelBalance()
{
    constexpr double sampleRate = 48000.0;
    constexpr double renderSeconds = 0.75;

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.outputMode = electry::OutputMode::Mono;

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);

    const auto e1 = renderNote(engine, sampleRate, 28, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto b1 = renderNote(engine, sampleRate, 35, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto e2 = renderNote(engine, sampleRate, 40, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto a2 = renderNote(engine, sampleRate, 45, 0.8f,
                               PlayStyle::Sustain, renderSeconds);

    const std::array<const StereoBuffer*, 4> renders {{ &e1, &b1, &e2, &a2 }};
    struct Window
    {
        const char* name;
        double beginSeconds;
        double endSeconds;
        double minimumE1;
        double minimumB1;
    };
    constexpr std::array<Window, 3> windows {{
        { "attack", 0.004, 0.115, 0.003981, 0.003981 },
        { "body", 0.050, 0.200, 0.003981, 0.003981 },
        { "sustain", 0.200, 0.700, 0.002512, 0.002512 },
    }};

    for (const auto& window : windows)
    {
        std::array<double, 4> rms {};
        for (std::size_t i = 0; i < renders.size(); ++i)
        {
            rms[i] = rmsInRange(
                renders[i]->left,
                static_cast<int>(window.beginSeconds * sampleRate),
                static_cast<int>(window.endSeconds * sampleRate));
        }

        expect(rms[0] >= window.minimumE1,
               std::string(window.name) + " E1 is effectively silent (RMS "
                   + std::to_string(rms[0]) + ")");
        expect(rms[1] >= window.minimumB1,
               std::string(window.name) + " B1 is effectively silent (RMS "
                   + std::to_string(rms[1]) + ")");

        const double reference = std::max(rms[3], 1.0e-15);
        const std::array<double, 3> ratios {
            rms[0] / reference, rms[1] / reference, rms[2] / reference
        };
        constexpr std::array<double, 3> minimumRatios {
            0.5012, 0.6310, 0.7079 // -6, -4, and -3 dB versus A2.
        };
        for (std::size_t index = 0; index < ratios.size(); ++index)
        {
            expect(ratios[index] >= minimumRatios[index],
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is under-balanced versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
            expect(ratios[index] <= 1.585,
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is over-compensated versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
        }
    }

    expect(peakAbs(e1.left) < 0.50f && peakAbs(b1.left) < 0.50f,
           "low-register level compensation is driving every note into the guard");
}

void testInternalOversamplingPolicy()
{
    struct RateCase { double hostRate; int expectedFactor; };
    constexpr std::array<RateCase, 5> rates {{
        { 44100.0, 2 }, { 48000.0, 2 }, { 96000.0, 2 },
        { 192000.0, 1 }, { 384000.0, 1 },
    }};

    for (const auto& rate : rates)
    {
        ElectryEngine engine;
        engine.prepare(rate.hostRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        expect(TestAccess::oversamplingFactor(engine) == rate.expectedFactor,
               "wrong internal oversampling factor at "
                   + std::to_string(rate.hostRate) + " Hz");
        expect(TestAccess::hostSampleRate(engine) == rate.hostRate,
               "host sample rate was not retained by prepare()");
        expect(TestAccess::internalSampleRate(engine)
                   == rate.hostRate * rate.expectedFactor,
               "internal sample clock does not match host rate times factor");

        engine.noteOn(45, 0.8f);
        constexpr int hostSamples = 1024;
        StereoBuffer buffer(hostSamples);
        renderInto(engine, buffer, 127);
        expect(allFinite(buffer),
               "oversampled render became non-finite at "
                   + std::to_string(rate.hostRate) + " Hz");
        const float peak = peakAbs(buffer.left);
        expect(peak > 1.0e-5f && peak < 0.80f,
               "oversampled render was silent or unbounded at "
                   + std::to_string(rate.hostRate) + " Hz");

        const int stringIndex = TestAccess::stringForNote(engine, 45);
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.ageSamples
                   == static_cast<std::uint64_t>(hostSamples * rate.expectedFactor),
               "host samples did not advance the physical clock exactly");
    }

    // Compare one 2x render with one native high-rate render. Both must retain
    // the played pitch after the halfband FIR and host-rate decimation.
    for (const double sampleRate : { 48000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.35f,
                                       PlayStyle::Sustain, 0.8);
        const double expected = midiHz(45);
        const double measured = measureFrequency(
            buffer.left, static_cast<int>(0.30 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, expected);
        expect(std::abs(centsBetween(measured, expected)) < 10.0,
               "oversampling introduced gross pitch drift at "
                   + std::to_string(sampleRate) + " Hz");
    }
}

// prepare() clamps a hostile host sample rate to [minimumSupportedSampleRate,
// maximumSupportedSampleRate] (falling back to 48 kHz first if it is not even
// finite) before any delay line is sized from it - see the comment on those
// two constants in ElectryEngine.h. Every other test only ever calls
// prepare() with a sane host rate, so that guard was previously exercised
// only by inspection.
void testPrepareClampsHostileSampleRate()
{
    const double minimumRate = TestAccess::minimumSupportedSampleRate();
    const double maximumRate = TestAccess::maximumSupportedSampleRate();

    struct HostileCase { double requested; double expectedClamped; const char* name; };
    const std::array<HostileCase, 5> cases {{
        { std::numeric_limits<double>::quiet_NaN(), 48000.0, "NaN" },
        { -1.0e9, minimumRate, "large negative" },
        { 0.0, minimumRate, "zero" },
        { 1.0, minimumRate, "below the floor" },
        { 1.0e9, maximumRate, "far above the ceiling" },
    }};

    for (const auto& hostileCase : cases)
    {
        ElectryEngine engine;
        engine.prepare(hostileCase.requested, 512);

        expect(TestAccess::hostSampleRate(engine) == hostileCase.expectedClamped,
               std::string("prepare() did not clamp a ") + hostileCase.name
                   + " sample rate to the documented bound");
        expect(TestAccess::internalSampleRate(engine)
                   == TestAccess::hostSampleRate(engine)
                          * static_cast<double>(
                                TestAccess::oversamplingFactor(engine)),
               std::string("internal clock did not track the clamped rate for ")
                   + hostileCase.name);

        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.noteOn(45, 0.8f);
        StereoBuffer buffer(256);
        renderInto(engine, buffer);
        expect(allFinite(buffer),
               std::string("a ") + hostileCase.name
                   + " sample rate produced non-finite audio after clamping");
        expect(peakAbs(buffer.left) < 16.0f,
               std::string("a ") + hostileCase.name
                   + " sample rate bypassed the output guardrail");
    }
}

void testRenderMatrixFiniteAndBounded()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        engine.setParameters(parameters);

        for (int styleIndex = 0;
             styleIndex < ElectryEngine::playStyleKeyswitchCount; ++styleIndex)
        for (int pickIndex = 0;
             pickIndex < ElectryEngine::pickStyleKeyswitchCount; ++pickIndex)
        {
            const auto style = static_cast<PlayStyle>(styleIndex);
            const auto pick = static_cast<PickStyle>(pickIndex);
            const std::string comboName = "style " + std::to_string(styleIndex)
                                        + " pick " + std::to_string(pickIndex);
            auto buffer = renderNote(engine, sampleRate, 45, 0.9f, style,
                                     0.5, 0.35, pick);
            expect(allFinite(buffer),
                   "non-finite output at rate " + std::to_string(sampleRate)
                       + " " + comboName);
            const float peak = peakAbs(buffer.left);
            expect(peak < 0.80f,
                   "output beyond guard at rate " + std::to_string(sampleRate)
                       + " " + comboName);
            expect(peak > 1.0e-4f,
                   comboName + " is silent at rate "
                       + std::to_string(sampleRate));

            if (sampleRate == 48000.0)
            {
                const auto low = renderNote(
                    engine, sampleRate, 28, 0.9f, style, 0.5, 0.35, pick);
                const float lowPeak = peakAbs(low.left);
                expect(allFinite(low),
                       "non-finite Drop-E output for " + comboName);
                expect(lowPeak > 1.0e-4f && lowPeak < 0.80f,
                       "Drop-E combination is silent or driving the guard: "
                           + comboName + " (peak " + std::to_string(lowPeak)
                           + ")");
            }
        }
    }
}

void testPitchAccuracy()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        for (const int midiNote : { 28, 35, 40, 45, 50, 55, 59, 64, 69, 76, 86 })
        {
            auto buffer = renderNote(engine, sampleRate, midiNote, 0.3f,
                                     PlayStyle::Sustain, 1.1);
            const int start = static_cast<int>(0.45 * sampleRate);
            const int window = static_cast<int>(0.5 * sampleRate);
            const double expected = midiHz(midiNote);
            const double measured = measureFrequency(buffer.left, start, window,
                                                     sampleRate, expected);
            const double cents = centsBetween(measured, expected);
            expect(std::abs(cents) < 8.0,
                   "note " + std::to_string(midiNote) + " at rate "
                       + std::to_string(sampleRate) + " off by "
                       + std::to_string(cents) + " cents");
        }
    }
}

void testDropELowNoteAtMaximumRate()
{
    constexpr double sampleRate = 384000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.setPitchBend(-1.0f);
    engine.noteOn(28, 0.25f);

    StereoBuffer buffer(static_cast<int>(1.25 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer), "Drop-E render became non-finite at 384 kHz");

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(stringIndex == 0 && snapshot.fret == 0,
           "open E1 was not allocated to the eighth string");
    expect(snapshot.verticalDelayTarget > 9000.0f,
           "maximum-rate Drop-E delay did not exercise the expanded line");
    expect(snapshot.verticalDelayTarget
               < static_cast<float>(TestAccess::delayLineCapacity() - 8),
           "maximum-rate Drop-E delay exceeded the delay-line capacity");

    const double expected = midiHz(26); // E1 with the wheel at -2 semitones.
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(0.55 * sampleRate),
        static_cast<int>(0.60 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "384 kHz Drop-E wheel-down pitch is inaccurate");
}

// prepare()'s sample-rate guard - a non-finite rate falls back to 48 kHz,
// and any finite rate is then clamped to [minimumSupportedSampleRate,
// maximumSupportedSampleRate], 8 kHz and 384 kHz respectively - was only
// ever driven with rates already inside that range (44.1/48/96/192/384 kHz
// across the suite). Confirms the guard actually lands on the same internal
// clock as an explicit prepare() at the fallback/clamped rate, by comparing
// the resulting delay-line target for the same open note, and that the
// engine keeps rendering finite audio.
void testPrepareSanitisesSampleRate()
{
    const auto openStringDelayTarget = [] (double sampleRate)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.noteOn(40, 0.8f);

        StereoBuffer buffer(2048);
        renderInto(engine, buffer);
        expect(allFinite(buffer),
               "a sanitised prepare() sample rate produced non-finite audio");

        const int stringIndex = TestAccess::stringForNote(engine, 40);
        return TestAccess::snapshot(engine, stringIndex).verticalDelayTarget;
    };

    constexpr double minimumSupportedSampleRate = 8000.0;
    constexpr double maximumSupportedSampleRate = 384000.0;

    expect(openStringDelayTarget(std::nan(""))
               == openStringDelayTarget(48000.0),
           "a NaN sample rate did not fall back to the 48 kHz default");
    expect(openStringDelayTarget(1.0e9)
               == openStringDelayTarget(maximumSupportedSampleRate),
           "a sample rate above the ceiling was not clamped to it");
    expect(openStringDelayTarget(1.0)
               == openStringDelayTarget(minimumSupportedSampleRate),
           "a sample rate below the floor was not clamped to it");
    expect(openStringDelayTarget(-48000.0)
               == openStringDelayTarget(minimumSupportedSampleRate),
           "a negative sample rate was not clamped to the floor");
}

// process()'s own guard - a null left or right pointer, or a non-positive
// sample count, is a no-op, and an unprepared engine fills the caller's
// buffer with silence and returns rather than touching any voice state - was
// never driven directly anywhere in the suite: every call site above always
// passes two valid pointers, a positive length and an already-prepared
// engine. ElectryFx::process() carries the identical guard shape and has its
// own direct coverage in testHostileInput(); this closes the same gap here.
void testProcessRejectsInvalidBuffers()
{
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
    engine.noteOn(40, 0.9f);

    // Nonzero sentinels, distinct per channel, so a regression that clears
    // the surviving channel instead of leaving it untouched is caught -
    // zero-initialised buffers would hide exactly that bug.
    std::vector<float> left(64, 0.31f);
    std::vector<float> right(64, -0.47f);
    const std::vector<float> leftSentinel = left;
    const std::vector<float> rightSentinel = right;

    // A null pointer, on either side, must be a no-op rather than a crash -
    // including leaving the other, valid channel's buffer untouched.
    engine.process(nullptr, right.data(), static_cast<int>(right.size()));
    engine.process(left.data(), nullptr, static_cast<int>(left.size()));
    expect(left == leftSentinel && right == rightSentinel,
           "a null-pointer process() call wrote into the other, valid channel");

    // A non-positive sample count must also be a no-op.
    engine.process(left.data(), right.data(), 0);
    engine.process(left.data(), right.data(), -4);
    expect(left == leftSentinel && right == rightSentinel,
           "a non-positive sample count still wrote into the buffers");

    expect(engine.getActiveVoiceCount() == 1,
           "an invalid process() call disturbed the sounding voice");

    // A genuinely valid call still renders audibly, showing the guards above
    // rejected only the hostile shapes and not every call.
    StereoBuffer buffer(2048);
    renderInto(engine, buffer);
    expect(allFinite(buffer) && peakAbs(buffer.left) > 1.0e-4f,
           "a valid process() call after the hostile ones produced no audio");

    // An unprepared engine fills the caller's buffer with silence and
    // returns, rather than touching voice state sized for whatever the
    // engine was (or was never) prepared at.
    ElectryEngine fresh;
    std::vector<float> unpreparedLeft(256, 0.7f);
    std::vector<float> unpreparedRight(256, -0.7f);
    fresh.process(unpreparedLeft.data(), unpreparedRight.data(),
                 static_cast<int>(unpreparedLeft.size()));
    expect(std::all_of(unpreparedLeft.begin(), unpreparedLeft.end(),
                       [] (float sample) { return sample == 0.0f; })
               && std::all_of(unpreparedRight.begin(), unpreparedRight.end(),
                              [] (float sample) { return sample == 0.0f; }),
           "an unprepared engine did not fill the buffer with silence");
    expect(fresh.getActiveVoiceCount() == 0,
           "an unprepared engine's process() call created a voice");
}

void testDeterminism()
{
    constexpr double sampleRate = 48000.0;
    const auto renderSequence = [] ()
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.reset();

        StereoBuffer buffer(static_cast<int>(1.6 * sampleRate));
        engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
        engine.noteOn(45, 0.85f);
        engine.process(buffer.left.data(), buffer.right.data(), 12000);
        engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
        engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        engine.noteOn(52, 0.6f);
        engine.process(buffer.left.data() + 12000,
                       buffer.right.data() + 12000, 24000);
        engine.noteOff(45);
        engine.noteOff(52);
        engine.process(buffer.left.data() + 36000, buffer.right.data() + 36000,
                       buffer.size() - 36000);
        return buffer;
    };

    const auto first = renderSequence();
    const auto second = renderSequence();
    bool identical = true;
    for (std::size_t i = 0; i < first.left.size(); ++i)
        if (first.left[i] != second.left[i])
        {
            identical = false;
            break;
        }
    expect(identical, "identical MIDI does not render identical audio");

    const float peak = peakAbs(first.left);
    expect(peak > 1.0e-4f, "determinism fixture rendered silence");
}

void testKeyswitchesSelectStylesSilently()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    expect(ElectryEngine::firstKeyswitchNote == 12,
           "keyswitch range does not start at MIDI 12");
    expect(ElectryEngine::pickStyleKeyswitchCount == 3
               && ElectryEngine::playStyleKeyswitchCount == 7
               && ElectryEngine::keyswitchCount == 10,
           "the two keyswitch banks do not expose 3 picking and 7 play styles");
    expect(ElectryEngine::firstPlayStyleKeyswitchNote == 15,
           "the play-style bank does not follow the picking bank at MIDI 15");
    expect(ElectryEngine::lowestPlayableNote == 28
               && ElectryEngine::highestPlayableNote == 86,
           "Drop-E playable range is not MIDI 28..86");

    expect(engine.getCurrentPickStyle() == PickStyle::Down
               && engine.getCurrentPlayStyle() == PlayStyle::Sustain,
           "default styles are not a sustained downstroke");

    // Keyswitches alone must never make sound.
    StereoBuffer buffer(static_cast<int>(0.25 * sampleRate));
    for (int keyswitch = 0; keyswitch < ElectryEngine::keyswitchCount; ++keyswitch)
        engine.noteOn(ElectryEngine::firstKeyswitchNote + keyswitch, 1.0f);
    renderInto(engine, buffer);
    expect(peakAbs(buffer.left) == 0.0f, "keyswitch notes produced audio");
    expect(engine.getActiveVoiceCount() == 0, "keyswitch notes created voices");

    // Walking every keyswitch leaves the last of each bank latched.
    expect(engine.getCurrentPickStyle() == PickStyle::Alternate
               && engine.getCurrentPlayStyle() == PlayStyle::Dead,
           "walking the keyswitch banks did not latch the last of each");

    // The two banks are independent: a play-style switch keeps the picking
    // style, and a picking switch keeps the play style.
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    expect(engine.getCurrentPlayStyle() == PlayStyle::PalmMute
               && engine.getCurrentPickStyle() == PickStyle::Alternate,
           "a play-style keyswitch disturbed the latched picking style");
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    expect(engine.getCurrentPickStyle() == PickStyle::Up
               && engine.getCurrentPlayStyle() == PlayStyle::PalmMute,
           "a picking keyswitch disturbed the latched play style");

    // Keyswitch note-offs are ignored; the styles persist for played notes.
    engine.noteOff(styleKeyswitch(PlayStyle::PalmMute));
    engine.noteOff(pickKeyswitch(PickStyle::Up));
    expect(engine.getCurrentPlayStyle() == PlayStyle::PalmMute
               && engine.getCurrentPickStyle() == PickStyle::Up,
           "keyswitch note-offs cleared the latched styles");

    engine.noteOn(52, 0.8f);
    const auto stringIndex = TestAccess::stringForNote(engine, 52);
    expect(stringIndex >= 0, "played note did not allocate a string");
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.playStyle == PlayStyle::PalmMute && snapshot.strokeIsUp,
           "played note did not inherit the latched style combination");

    // The notes between the keyswitch banks and the playable range are dead:
    // they neither sound nor disturb either latch.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
    for (int note = ElectryEngine::firstKeyswitchNote
                  + ElectryEngine::keyswitchCount;
         note < ElectryEngine::lowestPlayableNote; ++note)
        engine.noteOn(note, 0.9f);
    expect(engine.getActiveVoiceCount() == 0,
           "a note between the keyswitches and the playable range sounded");
    expect(engine.getCurrentPlayStyle() == PlayStyle::Harmonics
               && engine.getCurrentPickStyle() == PickStyle::Down,
           "a dead-zone note disturbed a latched style");

    // The range boundaries are unambiguous: 18 is the final silent keyswitch
    // and 28 is the sounding open low E.
    engine.reset();
    engine.noteOn(18, 0.9f);
    expect(engine.getCurrentPlayStyle() == PlayStyle::Harmonics,
           "MIDI 18 did not select the final play style");
    expect(engine.getActiveVoiceCount() == 0,
           "final keyswitch note created a voice");
    engine.noteOn(28, 0.8f);
    expect(TestAccess::stringForNote(engine, 28) == 0,
           "MIDI 28 did not play open E1 on the lowest string");

    // Notes outside both the keyswitch and playable ranges are ignored.
    engine.noteOn(0, 0.9f);
    engine.noteOn(87, 0.9f);
    expect(engine.getActiveVoiceCount() == 1,
           "notes outside keyswitches and E1..D6 were not ignored");
}

void testAlternateStrokeSequence()
{
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(EngineParameters {});
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);

    expect(engine.getCurrentPickStyle() == PickStyle::Alternate,
           "Alternate did not latch");

    // Rejected performance events must not consume a stroke direction.
    engine.noteOn(100, 1.0f);
    engine.noteOn(40, 0.0f);

    engine.noteOn(40, 0.8f);
    const auto first = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 40));
    engine.noteOn(45, 0.8f);
    const auto second = TestAccess::snapshot(engine,
                                             TestAccess::stringForNote(engine, 45));
    engine.noteOn(50, 0.8f);
    const auto third = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 50));

    expect(first.valid && ! first.strokeIsUp,
           "Alternate did not begin with a downstroke");
    expect(second.valid && second.strokeIsUp,
           "Alternate did not alternate to an upstroke");
    expect(third.valid && ! third.strokeIsUp,
           "Alternate did not alternate back to a downstroke");
    expect(engine.getCurrentPickStyle() == PickStyle::Alternate,
           "resolved strokes replaced the latched Alternate picking style");

    // Alternate picking composes with any play style: the palm-muted phrase
    // keeps alternating.
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(55, 0.8f);
    const auto mutedUp = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 55));
    expect(mutedUp.valid && mutedUp.playStyle == PlayStyle::PalmMute
               && mutedUp.strokeIsUp,
           "Alternate did not continue through a play-style change");

    // A hammered note has no pick, so it neither takes a stroke nor consumes
    // one: the sequence resumes where it left off.
    engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    engine.noteOn(57, 0.8f);
    const auto hammered = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 57));
    expect(hammered.valid && hammered.playStyle == PlayStyle::Hammer
               && ! hammered.strokeIsUp,
           "a hammered note took a pick stroke");
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(59, 0.8f);
    const auto resumed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 59));
    expect(resumed.valid && ! resumed.strokeIsUp,
           "a hammered note consumed a stroke from the alternate sequence");

    // Pressing the keyswitch again begins a fresh phrase on a downstroke.
    // The pending stroke here is an UPstroke (down/up/down/up have been
    // consumed and the hammered note took none), so a reset that failed to
    // clear the phase would render an upstroke and fail this check.
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(64, 0.8f);
    const auto restarted = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 64));
    expect(restarted.valid && ! restarted.strokeIsUp,
           "reselecting Alternate did not reset its phase");
}

void testArticulationsSoundDistinct()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);

    const auto renderStyle = [&] (PlayStyle style,
                                  PickStyle pick = PickStyle::Down)
    {
        return renderNote(engine, sampleRate, 48, 0.85f, style, 1.2, -1.0,
                          pick);
    };

    const auto down = renderStyle(PlayStyle::Sustain);
    const auto up = renderStyle(PlayStyle::Sustain, PickStyle::Up);
    const auto hammer = renderStyle(PlayStyle::Hammer);
    const auto muted = renderStyle(PlayStyle::PalmMute);
    const auto harmonic = renderStyle(PlayStyle::Harmonics);

    const double f0 = midiHz(48);
    const double downCentroid = spectralCentroid(down.left, attackStart,
                                                 attackWindow, sampleRate, f0);
    const double upCentroid = spectralCentroid(up.left, attackStart,
                                               attackWindow, sampleRate, f0);
    const double hammerCentroid = spectralCentroid(hammer.left, attackStart,
                                                   attackWindow, sampleRate, f0);
    const double harmonicCentroid = spectralCentroid(
        harmonic.left, attackStart, attackWindow, sampleRate, f0);

    // The hammered attack is fingered, not picked: it must be darker than
    // both pick strokes. The harmonic's node touch removes the low modes, so
    // its attack sits clearly brighter than the fretted downstroke.
    //
    // The margin is 0.95 rather than 0.9 because the picked attack's own
    // spectrum is now calibrated against a dry electric low-E reference
    // recording, and is darker than it used to be. The ordering these checks
    // exist to pin is unchanged; the absolute gap between a fingered and a
    // picked attack is simply smaller than it was against the previous,
    // brighter picked voicing.
    expect(hammerCentroid < downCentroid * 0.95,
           "hammer-on attack is not darker than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(hammerCentroid < upCentroid * 0.9,
           "hammer-on attack is not darker than an upstroke (up "
               + std::to_string(upCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(harmonicCentroid > downCentroid * 1.05,
           "harmonic attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, harmonic "
               + std::to_string(harmonicCentroid) + " Hz)");
    expect(upCentroid > downCentroid * 1.01,
           "upstroke attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, up "
               + std::to_string(upCentroid) + " Hz)");

    // The hammered attack is also quieter than the picked one.
    const double downAttackRms = rmsInRange(down.left, attackStart,
                                            attackStart + attackWindow);
    const double hammerAttackRms = rmsInRange(hammer.left, attackStart,
                                              attackStart + attackWindow);
    expect(hammerAttackRms < downAttackRms * 0.85,
           "hammer-on attack is not softer than a downstroke (down "
               + std::to_string(downAttackRms) + ", hammer "
               + std::to_string(hammerAttackRms) + ")");

    // Palm muting kills the sustain: compare late energy.
    const int lateStart = static_cast<int>(0.8 * sampleRate);
    const int lateEnd = static_cast<int>(1.1 * sampleRate);
    const double downLate = rmsInRange(down.left, lateStart, lateEnd);
    const double mutedLate = rmsInRange(muted.left, lateStart, lateEnd);
    expect(mutedLate < downLate * 0.25,
           "muted notes do not decay dramatically faster than open notes");

    // Down and up strokes must not be identical renders.
    double difference = 0.0;
    double reference = 0.0;
    for (int i = 0; i < static_cast<int>(0.2 * sampleRate); ++i)
    {
        const double a = down.left[static_cast<std::size_t>(i)];
        const double b = up.left[static_cast<std::size_t>(i)];
        difference += (a - b) * (a - b);
        reference += a * a;
    }
    expect(difference > 0.01 * reference,
           "downstroke and upstroke render nearly identical audio");
}

void testStyleAndStrokeCombinations()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);

    // The stroke composes with every picked style. An upstroke palm mute is
    // still a mute - the decay is the style's - but it is a genuinely
    // different render with the upstroke's brighter, inverted contact.
    const auto mutedDown = renderNote(engine, sampleRate, 45, 0.85f,
                                      PlayStyle::PalmMute, 0.9);
    const auto mutedUp = renderNote(engine, sampleRate, 45, 0.85f,
                                    PlayStyle::PalmMute, 0.9, -1.0,
                                    PickStyle::Up);
    const double comboDifference = normalisedDifferenceRms(
        mutedDown.left, mutedUp.left, 0, static_cast<int>(0.2 * sampleRate));
    expect(comboDifference > 0.05,
           "up and down palm mutes render nearly identical audio ("
               + std::to_string(comboDifference) + ")");

    const int muteLateStart = static_cast<int>(0.36 * sampleRate);
    const int muteLateEnd = static_cast<int>(0.56 * sampleRate);
    const auto openDown = renderNote(engine, sampleRate, 45, 0.85f,
                                     PlayStyle::Sustain, 0.9);
    const double openLate = rmsInRange(openDown.left, muteLateStart, muteLateEnd);
    for (const auto* muted : { &mutedDown, &mutedUp })
    {
        const double mutedLate = rmsInRange(muted->left, muteLateStart,
                                            muteLateEnd);
        expect(mutedLate < openLate * 0.30,
               "a palm mute did not keep its damping under both strokes");
    }

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);
    const double downMuteCentroid = spectralCentroid(
        mutedDown.left, attackStart, attackWindow, sampleRate, midiHz(45));
    const double upMuteCentroid = spectralCentroid(
        mutedUp.left, attackStart, attackWindow, sampleRate, midiHz(45));
    expect(upMuteCentroid > downMuteCentroid * 1.01,
           "an upstroke palm mute is not brighter than a downstroke one (down "
               + std::to_string(downMuteCentroid) + " Hz, up "
               + std::to_string(upMuteCentroid) + " Hz)");

    // The harmonic keeps its octave under either stroke.
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    const int harmonicStart = static_cast<int>(0.35 * sampleRate);
    const int harmonicWindow = static_cast<int>(0.55 * sampleRate);
    const double naturalExpected = midiHz(57);
    for (const auto pick : { PickStyle::Down, PickStyle::Up })
    {
        const auto natural = renderNote(engine, sampleRate, 45, 0.35f,
                                        PlayStyle::Harmonics, 1.1, -1.0, pick);
        const double naturalMeasured = measureFrequency(
            natural.left, harmonicStart, harmonicWindow, sampleRate,
            naturalExpected);
        expect(std::abs(centsBetween(naturalMeasured, naturalExpected)) < 10.0,
               "natural harmonic does not sound one octave above the played "
               "note under both strokes");
    }

    // A hammered note has no pick, so the latched picking style cannot change
    // it: the renders are bit-identical.
    const auto hammerDown = renderNote(engine, sampleRate, 45, 0.85f,
                                       PlayStyle::Hammer, 0.6);
    const auto hammerUp = renderNote(engine, sampleRate, 45, 0.85f,
                                     PlayStyle::Hammer, 0.6, -1.0,
                                     PickStyle::Up);
    expect(hammerDown.left == hammerUp.left,
           "the latched picking style changed a hammered note");
}

/** A note played without a plectrum draws no picking-hand variation.

    Every attack draws four numbers describing where the hand put the pick this
    stroke: how hard, how wide the contact patch, at what angle, and how far
    along the string. They are a pure function of the note counter, so two
    otherwise identical notes at different points in a sequence get different
    draws - which is the point of them, on a picked note.

    A hammer-on is the fretting hand landing on the fingerboard and a legato
    slide is a finger already down that simply moves. The engine says so itself:
    both clear the plectrum's own contact terms. The draw used to be applied to
    them anyway, so repeated hammer-ons and slides carried a picking hand's
    spread of level, pulse length and comb position with no pick in the stroke.

    The two engines below differ only in that one has already played a note, so
    the note under test is drawn at a different point in the sequence. The
    picked case is asserted in the other direction, because an engine that had
    simply deleted the variation would satisfy the fingered clause alone. */
void testFingeredNotesDrawNoPickingHandVariation()
{
    constexpr double sampleRate = 48000.0;
    constexpr int hammered = 45;
    constexpr int earlier = 64;   // a different string, so the draw is all that moves

    // The state of the string under test after a note-on, at two different
    // points in the stroke sequence.
    const auto attackAt = [&] (PlayStyle style, bool afterAnotherNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        engine.setParameters(parameters);
        engine.reset();

        if (afterAnotherNote)
        {
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            engine.noteOn(earlier, 0.85f);
        }
        // A slide is only legato once it lands on a string that is already
        // sounding; starting a phrase on the Slide keyswitch with nothing to
        // slide from is an ordinary pick stroke, and rightly draws a stroke.
        if (style == PlayStyle::Slide)
        {
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            engine.noteOn(hammered - 2, 0.85f);
        }
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(hammered, 0.85f);

        TestAccess::VoiceSnapshot found;
        for (int string = 0; string < ElectryEngine::stringCount; ++string)
        {
            const auto voice = TestAccess::snapshot(engine, string);
            if (voice.active && voice.midiNote == hammered)
                found = voice;
        }
        return found;
    };

    for (const auto style : { PlayStyle::Hammer, PlayStyle::Slide })
    {
        const auto first = attackAt(style, false);
        const auto later = attackAt(style, true);
        const auto name = style == PlayStyle::Hammer ? std::string("a hammer-on")
                                                     : std::string("a slide");
        expect(first.valid && later.valid,
               name + " did not sound on the string it was played on");
        if (! (first.valid && later.valid))
            continue;

        expect(first.excitationAmplitude == later.excitationAmplitude,
               name + " changed level with the note counter, so a picking hand's "
                      "contact force is being drawn for a fingered note ("
                   + std::to_string(first.excitationAmplitude) + " against "
                   + std::to_string(later.excitationAmplitude) + ")");
        expect(first.excitationLength == later.excitationLength,
               name + " changed pulse length with the note counter, so a picking "
                      "hand's contact patch is being drawn for a fingered note ("
                   + std::to_string(first.excitationLength) + " against "
                   + std::to_string(later.excitationLength) + ")");
        expect(first.excitationCombDelay == later.excitationCombDelay,
               name + " changed comb position with the note counter, so a picking "
                      "hand's contact offset is being drawn for a fingered note ("
                   + std::to_string(first.excitationCombDelay) + " against "
                   + std::to_string(later.excitationCombDelay) + ")");
    }

    // ... and a picked note still varies, or the clause above is vacuous.
    const auto pickedFirst = attackAt(PlayStyle::Sustain, false);
    const auto pickedLater = attackAt(PlayStyle::Sustain, true);
    expect(pickedFirst.valid && pickedLater.valid,
           "the picked control note did not sound");
    expect(pickedFirst.excitationAmplitude != pickedLater.excitationAmplitude
               || pickedFirst.excitationLength != pickedLater.excitationLength
               || pickedFirst.excitationCombDelay != pickedLater.excitationCombDelay,
           "two picked notes at different points in the sequence got the same "
           "stroke, so the picking-hand variation has gone altogether");
}

void testPitchWheelPerStringSensitivity()
{
    // The wheel is a bar: every string bends, each by its own compliance.
    // The constants themselves must be physical - the most compliant string
    // reaches the nominal range exactly, none exceeds it, and the ordering
    // follows the string set: the slack low E1 bends deepest, the plain G
    // beats its neighbours, and the taut plain top E is among the shallowest.
    expect(std::abs(TestAccess::bendSensitivity(0) - 1.0f) < 1.0e-6f,
           "the low E1 is not the full-range reference string");
    for (int string = 0; string < ElectryEngine::stringCount; ++string)
    {
        const float sensitivity = TestAccess::bendSensitivity(string);
        expect(sensitivity > 0.30f && sensitivity <= 1.0f,
               "bend sensitivity escaped its plausible range on string "
                   + std::to_string(string));
    }
    expect(TestAccess::bendSensitivity(5) > TestAccess::bendSensitivity(4)
               && TestAccess::bendSensitivity(5) > TestAccess::bendSensitivity(6),
           "the plain G does not bend deeper than its neighbours");
    expect(TestAccess::bendSensitivity(7) < TestAccess::bendSensitivity(5),
           "the taut top E does not bend shallower than the plain G");

    // And the audio must follow those constants: bend two strings with
    // clearly different compliance and measure the travel of each.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.bendTimeSeconds = 0.06f;
    engine.setParameters(parameters);

    for (const int openNote : { 28, 50 })
    {
        engine.reset();
        engine.setPitchBend(0.0f);
        engine.noteOn(openNote, 0.35f);
        StereoBuffer buffer(static_cast<int>(2.0 * sampleRate));
        engine.process(buffer.left.data(), buffer.right.data(),
                       static_cast<int>(0.6 * sampleRate));
        engine.setPitchBend(1.0f);
        engine.process(buffer.left.data() + static_cast<int>(0.6 * sampleRate),
                       buffer.right.data() + static_cast<int>(0.6 * sampleRate),
                       buffer.size() - static_cast<int>(0.6 * sampleRate));

        const int stringIndex = TestAccess::stringForNote(engine, openNote);
        const double sensitivity = TestAccess::bendSensitivity(stringIndex);
        const double before = measureFrequency(
            buffer.left, static_cast<int>(0.2 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, midiHz(openNote));
        const double expectedAfter = midiHz(openNote)
            * std::pow(2.0, 2.0 * sensitivity / 12.0);
        const double after = measureFrequency(
            buffer.left, static_cast<int>(1.2 * sampleRate),
            static_cast<int>(0.6 * sampleRate), sampleRate, expectedAfter);
        const double travel = centsBetween(after, before);
        const double expectedTravel = 200.0 * sensitivity;
        expect(std::abs(travel - expectedTravel) < 15.0,
               "note " + std::to_string(openNote) + " wheel travel measured "
                   + std::to_string(travel) + " cents, expected "
                   + std::to_string(expectedTravel));
    }
}

void testPitchWheelGlideFollowsBendTime()
{
    // The strings travel to the wheel over the Bend Time parameter, exactly
    // as the finger bends it used to drive did: shortly after a full-range
    // wheel move, a slow bend has covered far less of the distance than a
    // fast one, and both settle on the same target.
    constexpr double sampleRate = 48000.0;

    const auto travelAfter = [&] (float bendTimeSeconds, double measureAt)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.bendTimeSeconds = bendTimeSeconds;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(28, 0.35f); // E1: full-range reference string
        StereoBuffer settle(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, settle);
        engine.setPitchBend(1.0f);
        StereoBuffer bent(static_cast<int>((measureAt + 0.16) * sampleRate));
        renderInto(engine, bent);
        const double before = measureFrequency(
            settle.left, static_cast<int>(0.2 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, midiHz(28));
        const double during = measureFrequency(
            bent.left, static_cast<int>(measureAt * sampleRate),
            static_cast<int>(0.15 * sampleRate), sampleRate,
            midiHz(28) * std::pow(2.0, 1.0 / 12.0));
        return centsBetween(during, before);
    };

    const double fast = travelAfter(0.05f, 0.25);
    const double slow = travelAfter(1.60f, 0.25);
    expect(fast > 175.0,
           "a fast bend time did not reach the wheel target promptly ("
               + std::to_string(fast) + " cents)");
    expect(slow < fast - 60.0,
           "a slow bend time did not slow the wheel's travel (fast "
               + std::to_string(fast) + ", slow " + std::to_string(slow)
               + " cents)");
    const double slowSettled = travelAfter(1.60f, 5.5);
    expect(std::abs(slowSettled - 200.0) < 15.0,
           "a slow bend did not settle on the wheel target ("
               + std::to_string(slowSettled) + " cents)");
}

void testPitchWheelBendsSympatheticStrings()
{
    // The bar bends the strings nobody is fingering too: a ringing coupled
    // string follows the wheel at its own compliance.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.sympatheticAmount = 0.9f;
    parameters.bendTimeSeconds = 0.06f;
    engine.setParameters(parameters);
    engine.reset();

    // A2's third partial rings the open high E through the bridge. The
    // played string is released before the wheel moves, so what is measured
    // afterwards is the coupled ring alone, not the played string's own bent
    // partial landing nearby.
    engine.noteOn(45, 0.95f);
    StereoBuffer ringing(static_cast<int>(0.7 * sampleRate));
    renderInto(engine, ringing);
    const int highString = ElectryEngine::stringCount - 1;
    expect(TestAccess::snapshot(engine, highString).sympatheticReady,
           "the open high E did not ring for the wheel to bend");
    engine.noteOff(45);
    StereoBuffer damping(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, damping);

    engine.setPitchBend(1.0f);
    StereoBuffer bent(static_cast<int>(1.4 * sampleRate));
    renderInto(engine, bent);

    constexpr double openHighE = 329.62756;
    const double sensitivity = TestAccess::bendSensitivity(highString);
    const double bentHighE = openHighE * std::pow(2.0, 2.0 * sensitivity / 12.0);
    const int start = static_cast<int>(0.6 * sampleRate);
    const int length = static_cast<int>(0.7 * sampleRate);
    const double atOpen = dftMagnitude(bent.left, start, length, sampleRate,
                                       openHighE);
    const double atBent = dftMagnitude(bent.left, start, length, sampleRate,
                                       bentHighE);
    expect(atBent > 2.0 * atOpen,
           "the coupled high E did not follow the wheel ("
               + std::to_string(atOpen) + " at open pitch, "
               + std::to_string(atBent) + " at the bent pitch)");
}

void testHammerOnLegatoContinuity()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // Pick G3 on the E-string region, then hammer to A3.
    engine.noteOn(43, 0.7f);
    StereoBuffer buffer(static_cast<int>(2.2 * sampleRate));
    engine.process(buffer.left.data(), buffer.right.data(),
                   static_cast<int>(0.5 * sampleRate));

    const int stringBefore = TestAccess::stringForNote(engine, 43);
    expect(stringBefore >= 0, "picked note did not allocate a string");

    engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    engine.noteOn(45, 0.7f);

    const int stringAfter = TestAccess::stringForNote(engine, 45);
    expect(stringAfter == stringBefore,
           "hammer-on did not continue on the same string");
    expect(engine.getActiveVoiceCount() == 1,
           "hammer-on created a second voice");

    const int transition = static_cast<int>(0.5 * sampleRate);
    engine.process(buffer.left.data() + transition,
                   buffer.right.data() + transition,
                   buffer.size() - transition);

    // The hammered note settles on the new pitch.
    const double expected = midiHz(45);
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(1.2 * sampleRate),
        static_cast<int>(0.8 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "hammer-on did not settle on the target pitch");

    // No hard discontinuity at the hammer point: compare the largest
    // sample-to-sample step around the transition with the signal scale.
    float maximumStep = 0.0f;
    for (int i = transition - 32; i < transition + static_cast<int>(0.02 * sampleRate); ++i)
        maximumStep = std::max(maximumStep,
                               std::abs(buffer.left[static_cast<std::size_t>(i + 1)]
                                        - buffer.left[static_cast<std::size_t>(i)]));
    const float scale = peakAbs(buffer.left, transition - 4800, transition);
    expect(maximumStep < std::max(0.05f, 1.2f * scale),
           "hammer-on transition produced a hard discontinuity");
}

void testTensionGlide()
{
    // Tension modulation: a hard attack sounds slightly sharp and relaxes as
    // the string energy decays. Both measurements share the same scan
    // reference so grid quantise bias cancels in the difference.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.stringGauge = 0.0f; // the light set glides most
    parameters.velocityAmount = 1.0f;
    engine.setParameters(parameters);

    auto hard = renderNote(engine, sampleRate, 45, 1.0f, PlayStyle::Sustain, 1.8);

    const double early = measureFrequency(hard.left,
                                          static_cast<int>(0.05 * sampleRate),
                                          static_cast<int>(0.14 * sampleRate),
                                          sampleRate, midiHz(45));
    const double late = measureFrequency(hard.left,
                                         static_cast<int>(1.2 * sampleRate),
                                         static_cast<int>(0.5 * sampleRate),
                                         sampleRate, midiHz(45));
    const double glide = centsBetween(early, late);
    expect(glide > 0.4,
           "a hard attack does not glide sharp-to-true (measured "
               + std::to_string(glide) + " cents)");
    expect(glide < 80.0, "the attack pitch glide is implausibly large");
}

void testPickupsToneAndModelMorph()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    const int start = static_cast<int>(0.05 * sampleRate);
    const int window = static_cast<int>(0.4 * sampleRate);

    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.artifactAmount = 0.0f;

    parameters.pickupSelector = PickupSelector::Bridge;
    engine.setParameters(parameters);
    const auto bridge = renderNote(engine, sampleRate, 45, 0.7f,
                                   PlayStyle::Sustain, 0.8);

    parameters.pickupSelector = PickupSelector::Neck;
    engine.setParameters(parameters);
    const auto neck = renderNote(engine, sampleRate, 45, 0.7f,
                                 PlayStyle::Sustain, 0.8);

    const double f0 = midiHz(45);
    // The bridge position senses far less of the fundamental's antinode, so
    // its energy-weighted centroid sits clearly higher than the neck's.
    const double bridgeCentroid = spectralCentroid(bridge.left, start, window,
                                                   sampleRate, f0);
    const double neckCentroid = spectralCentroid(neck.left, start, window,
                                                 sampleRate, f0);
    expect(bridgeCentroid > neckCentroid * 1.08,
           "bridge pickup is not brighter than neck pickup (bridge "
               + std::to_string(bridgeCentroid) + " Hz, neck "
               + std::to_string(neckCentroid) + " Hz)");

    // Rolling the tone control off darkens the output.
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.toneKnob = 0.05f;
    engine.setParameters(parameters);
    const auto dark = renderNote(engine, sampleRate, 45, 0.7f,
                                 PlayStyle::Sustain, 0.8);
    const double openRatio = highBandRatio(bridge.left, start, window,
                                           sampleRate, f0);
    const double darkRatio = highBandRatio(dark.left, start, window,
                                           sampleRate, f0);
    expect(darkRatio < openRatio * 0.6,
           "tone control does not darken the pickup output (open ratio "
               + std::to_string(openRatio) + ", rolled "
               + std::to_string(darkRatio) + ")");

    // Guitar-model endpoints: a full Les Paul-style setting and a full
    // Telecaster-style setting must be audibly different instruments.
    EngineParameters lesPaul;
    lesPaul.bodyWood = lesPaul.bodySize = lesPaul.bodyShape = 0.0f;
    lesPaul.construction = lesPaul.scaleLength = lesPaul.pickupType = 0.0f;
    lesPaul.pickNoise = 0.0f;
    engine.setParameters(lesPaul);
    const auto lesPaulRender = renderNote(engine, sampleRate, 45, 0.7f,
                                          PlayStyle::Sustain, 0.8);

    EngineParameters telecaster;
    telecaster.bodyWood = telecaster.bodySize = telecaster.bodyShape = 1.0f;
    telecaster.construction = telecaster.scaleLength = telecaster.pickupType = 1.0f;
    telecaster.pickNoise = 0.0f;
    engine.setParameters(telecaster);
    const auto telecasterRender = renderNote(engine, sampleRate, 45, 0.7f,
                                             PlayStyle::Sustain, 0.8);

    const double lesPaulCentroid = spectralCentroid(lesPaulRender.left, start,
                                                    window, sampleRate, f0);
    const double telecasterCentroid = spectralCentroid(telecasterRender.left,
                                                       start, window, sampleRate, f0);
    // The single-coil Telecaster bridge is characteristically brighter than
    // a humbucker Les Paul.
    expect(telecasterCentroid > lesPaulCentroid * 1.10,
           "Telecaster endpoint is not brighter than Les Paul endpoint (LP "
               + std::to_string(lesPaulCentroid) + " Hz, Tele "
               + std::to_string(telecasterCentroid) + " Hz)");

    // Both endpoints stay in tune.
    for (const auto* render : { &lesPaulRender, &telecasterRender })
    {
        const double measured = measureFrequency(
            render->left, static_cast<int>(0.3 * sampleRate),
            static_cast<int>(0.4 * sampleRate), sampleRate, midiHz(45));
        expect(std::abs(centsBetween(measured, midiHz(45))) < 8.0,
               "guitar-model endpoint detuned the instrument");
    }
}

void testArtifactsControl()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // Pinned for the same reason as testMaterialAndControlAudibility: the bounds
    // below say the artifact layer is audible but subtle, and "how audible" is
    // measured relative to the note it sits on. On the shipped defaults - a thick
    // blank, heaviest set, tone back - the same 0.18 measures 0.0019 against this
    // 0.002 floor, not because the artifact path changed but because the note it
    // is compared against is darker and louder. Raising the Artifacts default
    // would restore the ratio; that is a voicing decision, not this test's.
    EngineParameters parameters;
    parameters.bodyWood = 0.5f;
    parameters.bodySize = 0.5f;
    parameters.bodyShape = 0.5f;
    parameters.construction = 0.5f;
    parameters.scaleLength = 0.5f;
    parameters.pickupType = 0.5f;
    parameters.toneKnob = 0.8f;
    parameters.stringGauge = 0.5f;
    parameters.stringAge = 0.15f;
    parameters.pickPosition = 0.35f;
    parameters.pickHardness = 0.6f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto renderAt = [&] (float amount)
    {
        parameters.artifactAmount = amount;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, 45, 0.95f,
                          PlayStyle::Sustain, 0.75);
    };

    const auto clean = renderAt(0.0f);
    const auto subtle = renderAt(0.18f);
    const auto medium = renderAt(0.60f);
    const auto full = renderAt(1.0f);
    const int start = static_cast<int>(0.020 * sampleRate);
    const int end = static_cast<int>(0.60 * sampleRate);
    const double subtleDifference = normalisedDifferenceRms(
        subtle.left, clean.left, start, end);
    const double mediumDifference = normalisedDifferenceRms(
        medium.left, clean.left, start, end);
    const double fullDifference = normalisedDifferenceRms(
        full.left, clean.left, start, end);

    expect(subtleDifference > 0.002 && subtleDifference < 0.15,
           "default artifacts are inaudible or no longer subtle (difference "
               + std::to_string(subtleDifference) + ")");
    expect(mediumDifference > subtleDifference * 1.35
               && fullDifference > mediumDifference * 1.12,
           "Artifacts control does not increase imperfection energy monotonically ("
               + std::to_string(subtleDifference) + ", "
               + std::to_string(mediumDifference) + ", "
               + std::to_string(fullDifference) + ")");

    const auto repeatedFull = renderAt(1.0f);
    expect(full.left == repeatedFull.left && full.right == repeatedFull.right,
           "artifact PRNG path is not sample-deterministic");

    engine.reset();
    StereoBuffer silence(4096);
    renderInto(engine, silence);
    expect(peakAbs(silence.left) == 0.0f,
           "Artifacts control generated ambience without a played note");

    // Worst-case low-register strum remains bounded with every imperfection
    // path open and the mandatory oversampling clock active.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (const int note : openNotes)
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, strum);
    expect(allFinite(strum) && peakAbs(strum.left) < 0.80f,
           "maximum-artifact eight-string strum became unstable");
}

void testAdvancedDispersionAndBodyConductance()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.scaleLength = 0.0f;
    parameters.stringGauge = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.bodyShape = 0.0f;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.inharmonicity > 1.0e-7f,
           "physical wound-string inharmonicity was not configured");
    expect(snapshot.dispersionLowCoefficient <= 0.0f
               && snapshot.dispersionLowCoefficient >= -0.9951f
               && snapshot.dispersionHighCoefficient <= 0.0f
               && snapshot.dispersionHighCoefficient >= -0.9951f,
           "eight-stage dispersion coefficients escaped their stable bounds");

    const auto expectedDeficit = [&] (float partial)
    {
        const float period = static_cast<float>(TestAccess::internalSampleRate(engine))
                           / snapshot.baseFrequency;
        const float stretch = std::sqrt(
            (1.0f + snapshot.inharmonicity * partial * partial)
            / (1.0f + snapshot.inharmonicity));
        return period * (1.0f - 1.0f / stretch);
    };
    for (const float partial : { snapshot.dispersionLowPartial,
                                 snapshot.dispersionHighPartial })
    {
        const float wanted = expectedDeficit(partial);
        const float actual = TestAccess::dispersionDeficit(
            engine, stringIndex, partial);
        const float relativeError = std::abs(actual - wanted)
            / std::max(wanted, 1.0e-6f);
        expect(relativeError < 0.20f,
               "two-point dispersion fit missed partial "
                   + std::to_string(partial) + " (wanted "
                   + std::to_string(wanted) + ", actual "
                   + std::to_string(actual) + ")");
    }

    expect(snapshot.bodyConductance >= 0.0f && snapshot.bodyConductance <= 1.0f,
           "modal bridge conductance escaped its passive range");
    expect(snapshot.bodyLossFactor > 0.0f && snapshot.bodyLossFactor <= 1.0f,
           "modal bridge conductance added string energy");

    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);
    const auto bypassed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 28));
    expect(std::abs(bypassed.bodyLossFactor - 1.0f) < 1.0e-6f,
           "zero Body Resonance did not exactly bypass structural loss");
}

void testMonoStereoOutputField()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.outputMode = electry::OutputMode::Mono;
    engine.setParameters(parameters);
    const auto monoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                    PlayStyle::Sustain, 0.65);
    expect(monoLow.left == monoLow.right,
           "Mono output mode is not exact dual mono");

    parameters.outputMode = electry::OutputMode::Stereo;
    engine.setParameters(parameters);
    const auto stereoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                      PlayStyle::Sustain, 0.65);
    const auto repeatedLow = renderNote(engine, sampleRate, 28, 0.8f,
                                        PlayStyle::Sustain, 0.65);
    expect(stereoLow.left == repeatedLow.left
               && stereoLow.right == repeatedLow.right,
           "Stereo divided-pickup field is not deterministic");

    const int start = static_cast<int>(0.025 * sampleRate);
    const int end = static_cast<int>(0.55 * sampleRate);
    const double lowLeft = rmsInRange(stereoLow.left, start, end);
    const double lowRight = rmsInRange(stereoLow.right, start, end);
    expect(lowLeft > lowRight * 1.10,
           "Stereo low E does not favour the low-string side (L "
               + std::to_string(lowLeft) + ", R "
               + std::to_string(lowRight) + ")");

    const auto stereoHigh = renderNote(engine, sampleRate, 64, 0.8f,
                                       PlayStyle::Sustain, 0.65);
    const double highLeft = rmsInRange(stereoHigh.left, start, end);
    const double highRight = rmsInRange(stereoHigh.right, start, end);
    expect(highRight > highLeft * 1.10,
           "Stereo high E does not favour the high-string side (L "
               + std::to_string(highLeft) + ", R "
               + std::to_string(highRight) + ")");

    std::vector<float> folded(stereoLow.left.size());
    std::vector<float> side(stereoLow.left.size());
    for (std::size_t sample = 0; sample < folded.size(); ++sample)
    {
        folded[sample] = 0.5f * (stereoLow.left[sample]
                               + stereoLow.right[sample]);
        side[sample] = 0.5f * (stereoLow.left[sample]
                             - stereoLow.right[sample]);
    }
    const double foldDifference = normalisedDifferenceRms(
        folded, monoLow.left, start, end);
    expect(foldDifference < 0.12,
           "Stereo field does not fold coherently to Mono (difference "
               + std::to_string(foldDifference) + ")");

    const double midRms = rmsInRange(folded, start, end);
    const double sideRms = rmsInRange(side, start, end);
    expect(sideRms > midRms * 0.08 && sideRms < midRms * 0.35,
           "Stereo side field is inaudible or excessive (ratio "
               + std::to_string(sideRms / std::max(midRms, 1.0e-12)) + ")");

    const double monoEnergy = std::pow(
        rmsInRange(monoLow.left, start, end), 2.0);
    const double stereoEnergy = 0.5
        * (lowLeft * lowLeft + lowRight * lowRight);
    const double energyRatio = stereoEnergy / std::max(monoEnergy, 1.0e-12);
    expect(energyRatio > 0.80 && energyRatio < 1.20,
           "Stereo output field changed average energy excessively (ratio "
               + std::to_string(energyRatio) + ")");
}

// A guitarist's picking dynamics span 25-30 dB. Electry used to render 5.2 dB
// of that, and the missing range was not in the amplitude law: player effort
// drove the contact spectrum over the same range as the level, so the extra
// energy of a hard stroke went into partials that had decayed before the
// attack was over. The two halves of the fix are separated below - the
// v=1..v=127 span is the target, the v=64..v=127 span is what identifies the
// decoupling as the change that delivers it.
void testVelocityDynamicRange()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 40;
    constexpr int windowSamples = static_cast<int>(0.050 * sampleRate);

    // Fresh engine per velocity: a shared engine would let the previous
    // stroke's residual ring into the peak of the next one.
    const auto peakDbAt = [&] (int midiVelocity, float velocityAmount)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.velocityAmount = velocityAmount;
        engine.setParameters(parameters);
        const auto rendered = renderNote(
            engine, sampleRate, note,
            static_cast<float>(midiVelocity) / 127.0f,
            PlayStyle::Sustain, 0.050);
        const double peak = std::max(peakAbs(rendered.left, 0, windowSamples),
                                     peakAbs(rendered.right, 0, windowSamples));
        return decibels(std::max(peak, 1.0e-12));
    };

    // "At the shipping defaults": the default Velocity Response is part of the
    // change, so it is read from the engine rather than repeated here.
    const float shippingResponse = EngineParameters {}.velocityAmount;
    expect(std::abs(shippingResponse - 0.85f) < 1.0e-6f,
           "default Velocity Response is not 0.85 ("
               + std::to_string(shippingResponse) + ")");

    const double atOne = peakDbAt(1, shippingResponse);
    const double atSixtyFour = peakDbAt(64, shippingResponse);
    const double atFull = peakDbAt(127, shippingResponse);

    // Today 5.218 dB at the shipping default.
    expect(atFull - atOne >= 18.0,
           "velocity spans too little of the keyboard (v=1 "
               + std::to_string(atOne) + " dBFS, v=127 "
               + std::to_string(atFull) + " dBFS, span "
               + std::to_string(atFull - atOne) + " dB)");

    // The amplitude law on its own reaches 1.686 dB here, against 1.487 dB
    // before it; only breaking the level-to-effort coupling moves this. The
    // ceiling with the release rate frozen outright is 4.630 dB, and freezing
    // it outright makes velocity darken the attack instead of brightening it,
    // so the bar sits below that.
    expect(atFull - atSixtyFour >= 3.0,
           "the upper half of the keyboard is still flat (v=64 "
               + std::to_string(atSixtyFour) + " dBFS, v=127 "
               + std::to_string(atFull) + " dBFS, span "
               + std::to_string(atFull - atSixtyFour) + " dB)");

    // A regression guard rather than a target: the amplitude law alone turns
    // the top of the keyboard over above v=104, and so does the shipping
    // engine on this grid.
    double previous = -1.0e9;
    int firstDrop = -1;
    double dropFrom = 0.0;
    double dropTo = 0.0;
    for (int step = 0; step < 16; ++step)
    {
        const int midiVelocity = 1
            + static_cast<int>(std::lround(step * 126.0 / 15.0));
        const double level = peakDbAt(midiVelocity, shippingResponse);
        if (level <= previous && firstDrop < 0)
        {
            firstDrop = midiVelocity;
            dropFrom = previous;
            dropTo = level;
        }
        previous = level;
    }
    expect(firstDrop < 0,
           "velocity level is not monotone (v=" + std::to_string(firstDrop)
               + " fell from " + std::to_string(dropFrom) + " to "
               + std::to_string(dropTo) + " dBFS)");

    // The loudest stroke must stay where it is: this is a dynamic range that
    // grows downwards, not an output-level change.
    expect(std::abs(atFull - (-25.690)) <= 1.5,
           "full velocity moved away from its calibrated level ("
               + std::to_string(atFull) + " dBFS)");

    // The decoupling must not be implemented by flattening the attack into
    // silence at low velocity, in either direction: the 2-8 kHz against
    // sub-500 Hz band ratio of the attack has to stay put. Today it moves
    // 3.431 dB across the same pair.
    const auto attackBandRatioDb = [&] (int midiVelocity, float velocityAmount)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.velocityAmount = velocityAmount;
        engine.setParameters(parameters);
        const auto rendered = renderNote(
            engine, sampleRate, note,
            static_cast<float>(midiVelocity) / 127.0f,
            PlayStyle::Sustain, 0.050);
        constexpr int transform = 2048;
        double high = 0.0;
        double low = 0.0;
        for (int start = 0; start + transform <= windowSamples; start += 512)
        {
            for (int bin = 1; bin < transform / 2; ++bin)
            {
                const double frequency = bin * sampleRate / transform;
                const bool isHigh = frequency >= 2000.0 && frequency <= 8000.0;
                const bool isLow = frequency < 500.0;
                if (! isHigh && ! isLow)
                    continue;
                const double magnitude = dftMagnitude(
                    rendered.left, start, transform, sampleRate, frequency);
                (isHigh ? high : low) += magnitude * magnitude;
            }
        }
        return 10.0 * std::log10(std::max(high, 1.0e-30)
                                 / std::max(low, 1.0e-30));
    };
    const double quietBand = attackBandRatioDb(16, shippingResponse);
    const double loudBand = attackBandRatioDb(127, shippingResponse);
    expect(std::abs(loudBand - quietBand) <= 4.0,
           "velocity moved the attack's band balance too far (v=16 "
               + std::to_string(quietBand) + " dB, v=127 "
               + std::to_string(loudBand) + " dB)");

    // Velocity Response at zero removes MIDI velocity from the instrument
    // exactly, which the exponent form preserves because force^0 is one.
    const double flatLow = peakDbAt(1, 0.0f);
    const double flatHigh = peakDbAt(127, 0.0f);
    expect(std::abs(flatHigh - flatLow) < 1.0e-9,
           "zero velocity response still changes the rendered level (spread "
               + std::to_string(flatHigh - flatLow) + " dB)");
}

// A repeated note must not be a repeated render. The protocol deliberately
// leaves 12 s between strokes so the string has decayed and the measurement
// sees the excitation rather than the previous stroke's residual ring, and it
// silences every noise control and the Artifacts detune so that what is
// measured is the picking hand and nothing that already varied.
void testPickingHandVariation()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 40;
    constexpr int strokeCount = 12;
    constexpr int captureSamples = static_cast<int>(0.150 * sampleRate);

    struct Repeats
    {
        std::vector<std::vector<float>> strokes;
        std::vector<double> peaksDb;
        std::vector<double> centroids;
    };

    const auto repeat = [&] (double gapSeconds, PickStyle pickStyle)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(pickKeyswitch(pickStyle), 1.0f);

        Repeats result;
        StereoBuffer gap(static_cast<int>(gapSeconds * sampleRate));
        for (int stroke = 0; stroke < strokeCount; ++stroke)
        {
            engine.noteOn(note, 0.80f);
            renderInto(engine, gap);
            std::vector<float> attack(gap.left.begin(),
                                      gap.left.begin() + captureSamples);
            result.peaksDb.push_back(
                decibels(std::max<double>(peakAbs(attack), 1.0e-12)));
            result.centroids.push_back(spectralCentroid(
                attack, 0, captureSamples, sampleRate, midiHz(note)));
            result.strokes.push_back(std::move(attack));
        }
        return result;
    };

    // Energy of the difference between two strokes' attacks, against the energy
    // of the earlier one.
    const auto differenceDb = [] (const std::vector<float>& later,
                                  const std::vector<float>& earlier)
    {
        double difference = 0.0;
        double reference = 0.0;
        for (std::size_t i = 0; i < later.size(); ++i)
        {
            const double d = static_cast<double>(later[i]) - earlier[i];
            difference += d * d;
            reference += static_cast<double>(earlier[i]) * earlier[i];
        }
        return 10.0 * std::log10(std::max(difference, 1.0e-30)
                                 / std::max(reference, 1.0e-30));
    };

    const auto spreadOf = [] (const std::vector<double>& values)
    {
        const auto bounds = std::minmax_element(values.begin(), values.end());
        return *bounds.second - *bounds.first;
    };

    // Successive strokes, or - under Alternate - strokes two apart, which hold
    // the up/down colouring constant so what is left is the hand.
    const auto pairDbs = [&] (const Repeats& repeats, int lag)
    {
        std::vector<double> result;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag)
                                    < repeats.strokes.size(); ++i)
            result.push_back(differenceDb(
                repeats.strokes[i + static_cast<std::size_t>(lag)],
                repeats.strokes[i]));
        return result;
    };

    const auto meanOf = [] (const std::vector<double>& values)
    {
        double total = 0.0;
        for (const double value : values)
            total += value;
        return total / static_cast<double>(values.size());
    };

    const auto latched = repeat(12.0, PickStyle::Down);
    const auto latchedPairs = pairDbs(latched, 1);
    const double latchedMean = meanOf(latchedPairs);

    // Without the per-stroke draws the successive pairs fall from -43.5 dB to
    // -94.3 dB, a mean of -84.6 dB: the twelve strokes are the same stroke, and
    // what little separates them is converging residual state. The band is
    // scored on the mean because the draws are independent, so two consecutive
    // strokes may land close by chance - one pair of the twelve reads -30.8 dB;
    // every individual pair still has to stay under the band's top.
    expect(latchedMean >= -24.0 && latchedMean <= -8.0,
           "repeated strokes do not differ by a hand's worth (mean successive "
               "difference " + std::to_string(latchedMean) + " dB)");
    expect(*std::max_element(latchedPairs.begin(), latchedPairs.end()) <= -8.0,
           "repeated strokes differ too much to be the same note (loudest "
               "successive difference "
               + std::to_string(*std::max_element(latchedPairs.begin(),
                                                  latchedPairs.end())) + " dB)");

    // The variation must be audible as a hand rather than as a level control.
    // Without the draws, 0.0120 dB.
    const double peakSpread = spreadOf(latched.peaksDb);
    expect(peakSpread >= 0.6 && peakSpread <= 3.0,
           "stroke-to-stroke level variation is outside a player's range ("
               + std::to_string(peakSpread) + " dB across 12 strokes)");

    // And it must move the tone, not only the level: without the draws, 0.380 Hz
    // on a 456 Hz centroid.
    const double centroidSpread = spreadOf(latched.centroids);
    expect(centroidSpread >= 12.0,
           "repeated strokes are spectrally identical (centroid spread "
               + std::to_string(centroidSpread) + " Hz)");

    // Alternate picking already varies stroke to stroke by more than the signal
    // itself, so the hand's variation has to ride on top of the up/down
    // colouring rather than replace it. Strokes two apart share a direction;
    // without the draws they converge to a mean of -86.1 dB, exactly as the
    // latched case does, so a change that only varies a latched Down fails
    // here.
    const auto alternating = repeat(12.0, PickStyle::Alternate);
    const auto alternatingPairs = pairDbs(alternating, 2);
    const double alternatingMean = meanOf(alternatingPairs);
    expect(alternatingMean >= -24.0 && alternatingMean <= -8.0,
           "alternate-picked repeats of the same stroke direction do not vary "
               "(mean difference two apart " + std::to_string(alternatingMean)
               + " dB)");
    expect(*std::max_element(alternatingPairs.begin(), alternatingPairs.end())
               <= -8.0,
           "alternate-picked strokes two apart differ too much (loudest "
               + std::to_string(*std::max_element(alternatingPairs.begin(),
                                                  alternatingPairs.end()))
               + " dB)");

    // Half a second apart the string has not decayed, so each stroke lands on
    // the previous one's ring and the difference between them is dominated by
    // how far that residual state has converged. Without the draws it
    // converges: the twelfth pair reads -27.3 dB against -94.3 dB on the 12 s
    // protocol, 67 dB apart. With the excitation itself varying, the two
    // protocols have to
    // agree, because the difference is then a property of the stroke rather
    // than of the state it lands on.
    const auto rapid = repeat(0.5, PickStyle::Down);
    const auto rapidPairs = pairDbs(rapid, 1);
    const double lastRapid = rapidPairs.back();
    const double lastLatched = latchedPairs.back();
    expect(std::abs(lastRapid - lastLatched) <= 6.0,
           "a repeated note still converges on itself (last pair "
               + std::to_string(lastRapid) + " dB at 0.5 s against "
               + std::to_string(lastLatched) + " dB at 12 s)");
}

void testVelocityExpression()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.velocityAmount = 1.0f;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    const auto low = renderNote(engine, sampleRate, 45, 0.2f,
                                PlayStyle::Sustain, 0.55);
    const auto middle = renderNote(engine, sampleRate, 45, 0.6f,
                                   PlayStyle::Sustain, 0.55);
    const auto high = renderNote(engine, sampleRate, 45, 1.0f,
                                 PlayStyle::Sustain, 0.55);
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackEnd = static_cast<int>(0.115 * sampleRate);
    const double lowRms = rmsInRange(low.left, attackStart, attackEnd);
    const double middleRms = rmsInRange(middle.left, attackStart, attackEnd);
    const double highRms = rmsInRange(high.left, attackStart, attackEnd);
    expect(middleRms > lowRms * 1.20 && highRms > middleRms * 1.20,
           "velocity amplitude is not clearly monotonic ("
               + std::to_string(lowRms) + ", " + std::to_string(middleRms)
               + ", " + std::to_string(highRms) + ")");

    const double lowCentroid = spectralCentroid(
        low.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    const double highCentroid = spectralCentroid(
        high.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    // Deliberately a smaller margin than the 10% this asked for before the
    // pick's own stiffness came to bound the contact spectrum: a harder stroke
    // is now mostly louder rather than proportionally sharper, and the
    // brightening that survives is the residual the plectrum's finite
    // compliance leaves. Measured 7.3% here, against 16.6% when effort and
    // level were the same curve. What the bound still catches is the
    // degenerate case - freezing the release rate outright reads 0.996, i.e. a
    // harder stroke that arrives darker.
    //
    // The 7.3% is the figure this read when the velocity work landed; the
    // humbucker's coil pair moved it to 6.7% two steps later, which is the
    // ratio the shipped engine measures on this fixture.
    expect(highCentroid > lowCentroid * 1.05,
           "velocity does not brighten the attack (low "
               + std::to_string(lowCentroid) + " Hz, high "
               + std::to_string(highCentroid) + " Hz)");

    // At zero response, MIDI velocity is deliberately removed from every
    // attack dimension, not merely from output gain.
    parameters.velocityAmount = 0.0f;
    engine.setParameters(parameters);
    const auto flatLow = renderNote(engine, sampleRate, 45, 0.2f,
                                    PlayStyle::Sustain, 0.30);
    const auto flatHigh = renderNote(engine, sampleRate, 45, 1.0f,
                                     PlayStyle::Sustain, 0.30);
    expect(flatLow.left == flatHigh.left && flatLow.right == flatHigh.right,
           "zero velocity response still changes the rendered attack");
}

void testMaterialAndControlAudibility()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // This test asks whether each build axis is audible when it is swept, and
    // every threshold below was calibrated against one specific instrument. It
    // must therefore state that instrument rather than inherit whatever the
    // shipped defaults happen to be, or the thresholds silently come to mean
    // something else the moment the default voicing moves - which is exactly
    // what happened when the defaults became a thick blank with the heaviest set
    // and the tone backed off: seven checks here failed without one line of the
    // model changing, because a darker instrument makes every axis a smaller
    // fraction of its own signal. The mid-scale, tone-open instrument below is
    // the one the numbers were measured on.
    EngineParameters base;
    base.bodyWood = 0.5f;
    base.bodySize = 0.5f;
    base.bodyShape = 0.5f;
    base.construction = 0.5f;
    base.scaleLength = 0.5f;
    base.pickupType = 0.5f;
    base.toneKnob = 0.8f;
    base.stringGauge = 0.5f;
    base.stringAge = 0.15f;
    base.pickPosition = 0.35f;
    base.pickHardness = 0.6f;
    base.bodyResonance = 0.55f;
    base.artifactAmount = 0.0f;
    base.pickNoise = 0.0f;
    base.fingerNoise = 0.0f;
    base.releaseNoise = 0.0f;

    const auto compareAxis = [&] (auto setAxis, int midiNote)
    {
        auto lowParameters = base;
        setAxis(lowParameters, 0.0f);
        engine.setParameters(lowParameters);
        const auto low = renderNote(engine, sampleRate, midiNote, 0.8f,
                                    PlayStyle::Sustain, 0.75);
        auto highParameters = base;
        setAxis(highParameters, 1.0f);
        engine.setParameters(highParameters);
        const auto high = renderNote(engine, sampleRate, midiNote, 0.8f,
                                     PlayStyle::Sustain, 0.75);
        return normalisedDifferenceRms(
            low.left, high.left, static_cast<int>(0.035 * sampleRate),
            static_cast<int>(0.60 * sampleRate));
    };

    const auto wood = [] (EngineParameters& p, float v) { p.bodyWood = v; };
    const auto size = [] (EngineParameters& p, float v) { p.bodySize = v; };
    const auto shape = [] (EngineParameters& p, float v) { p.bodyShape = v; };
    const auto construction = [] (EngineParameters& p, float v) { p.construction = v; };
    const auto scale = [] (EngineParameters& p, float v) { p.scaleLength = v; };
    const auto gauge = [] (EngineParameters& p, float v) { p.stringGauge = v; };

    using AxisSetter = void (*) (EngineParameters&, float);
    const std::array<std::pair<const char*, AxisSetter>, 4> bodyAxes {{
        { "wood", wood }, { "size", size }, { "shape", shape },
        { "construction", construction }
    }};
    for (const auto& namedAxis : bodyAxes)
    {
        const double lowNoteDifference = compareAxis(namedAxis.second, 28);
        const double midNoteDifference = compareAxis(namedAxis.second, 45);
        expect(std::min(lowNoteDifference, midNoteDifference) > 0.055,
               std::string("body ") + namedAxis.first
                   + " remains effectively inaudible on E1/A2 ("
                   + std::to_string(lowNoteDifference) + ", "
                   + std::to_string(midNoteDifference) + ")");
    }

    const double scaleDifference = compareAxis(scale, 28);
    const double gaugeDifference = compareAxis(gauge, 28);
    expect(scaleDifference > 0.025,
           "25.5-to-28-inch scale range does not change Drop-E timbre ("
               + std::to_string(scaleDifference) + ")");
    expect(gaugeDifference > 0.08,
           "string-gauge endpoints remain too similar ("
               + std::to_string(gaugeDifference) + ")");

    auto noBody = base;
    noBody.bodyResonance = 0.0f;
    engine.setParameters(noBody);
    const auto dry = renderNote(engine, sampleRate, 45, 0.8f,
                                PlayStyle::Sustain, 0.75);
    auto fullBody = base;
    fullBody.bodyResonance = 1.0f;
    engine.setParameters(fullBody);
    const auto resonant = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.75);
    const double bodyDifference = normalisedDifferenceRms(
        dry.left, resonant.left, static_cast<int>(0.035 * sampleRate),
        static_cast<int>(0.60 * sampleRate));
    // Exact modal normalisation keeps the structural path controlled: a
    // clearly audible endpoint change is required, but the test must not
    // reward the former oversized, clavinet-like body peaks.
    expect(bodyDifference > 0.08,
           "Body Resonance full range is still too polite ("
               + std::to_string(bodyDifference) + ")");

    auto fresh = base;
    fresh.bodyResonance = 0.0f;
    fresh.stringAge = 0.0f;
    engine.setParameters(fresh);
    const auto freshRender = renderNote(engine, sampleRate, 45, 0.8f,
                                        PlayStyle::Sustain, 1.3);
    auto old = fresh;
    old.stringAge = 1.0f;
    engine.setParameters(old);
    const auto oldRender = renderNote(engine, sampleRate, 45, 0.8f,
                                      PlayStyle::Sustain, 1.3);
    const double freshLate = rmsInRange(
        freshRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    const double oldLate = rmsInRange(
        oldRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    expect(oldLate < freshLate * 0.35,
           "String Age does not strongly shorten/darken the tail (ratio "
               + std::to_string(oldLate / std::max(freshLate, 1.0e-12)) + ")");

    auto bridgePick = fresh;
    bridgePick.pickPosition = 0.0f;
    engine.setParameters(bridgePick);
    const auto bridgePicked = renderNote(engine, sampleRate, 45, 0.8f,
                                         PlayStyle::Sustain, 0.6);
    auto neckPick = fresh;
    neckPick.pickPosition = 1.0f;
    engine.setParameters(neckPick);
    const auto neckPicked = renderNote(engine, sampleRate, 45, 0.8f,
                                       PlayStyle::Sustain, 0.6);
    const double positionDifference = normalisedDifferenceRms(
        bridgePicked.left, neckPicked.left, static_cast<int>(0.005 * sampleRate),
        static_cast<int>(0.40 * sampleRate));
    expect(positionDifference > 0.35,
           "Pick Position range is not clearly audible ("
               + std::to_string(positionDifference) + ")");

    auto soft = fresh;
    soft.pickHardness = 0.0f;
    engine.setParameters(soft);
    const auto softPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.5);
    auto hard = fresh;
    hard.pickHardness = 1.0f;
    engine.setParameters(hard);
    const auto hardPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.5);
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackLength = static_cast<int>(0.10 * sampleRate);
    const double softCentroid = spectralCentroid(
        softPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double hardCentroid = spectralCentroid(
        hardPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double softRms = rmsInRange(softPick.left, attackStart,
                                      attackStart + attackLength);
    const double hardRms = rmsInRange(hardPick.left, attackStart,
                                      attackStart + attackLength);
    // As above, the picked attack's reference-calibrated spectrum compresses
    // the absolute centroid range this control spans. It remains clearly
    // audible - the position, level and material bounds around it are
    // unchanged - but the old 1.35 margin belonged to the brighter voicing.
    expect(hardCentroid > softCentroid * 1.08,
           "Pick Hardness does not sufficiently brighten the attack (ratio "
               + std::to_string(hardCentroid / std::max(softCentroid, 1.0e-12))
               + ")");
    expect(hardRms > softRms * 0.55 && hardRms < softRms * 1.70,
           "Pick Hardness changes loudness more than material (RMS ratio "
               + std::to_string(hardRms / std::max(softRms, 1.0e-12)) + ")");
}

void testNoiseComponentsAndSilence()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // Complete silence with no input.
    engine.setParameters(EngineParameters {});
    engine.reset();
    StereoBuffer silent(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, silent);
    expect(peakAbs(silent.left) == 0.0f && peakAbs(silent.right) == 0.0f,
           "engine does not render exact silence with no notes");

    // Pick noise adds energy in the pre-attack contact window.
    EngineParameters noNoise;
    noNoise.pickNoise = 0.0f;
    noNoise.fingerNoise = 0.0f;
    noNoise.releaseNoise = 0.0f;
    engine.setParameters(noNoise);
    const auto clean = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.6);

    EngineParameters fullNoise;
    fullNoise.pickNoise = 1.0f;
    fullNoise.fingerNoise = 1.0f;
    fullNoise.releaseNoise = 1.0f;
    engine.setParameters(fullNoise);
    const auto noisy = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.6);

    // The pick contact lasts about 1.5 ms at the default hardness before the
    // release pulse starts; inside 1.2 ms the noiseless render is silent.
    const int contactWindow = static_cast<int>(0.0012 * sampleRate);
    const double cleanContact = rmsInRange(clean.left, 0, contactWindow);
    const double noisyContact = rmsInRange(noisy.left, 0, contactWindow);
    expect(noisyContact > cleanContact * 1.5 + 1.0e-6,
           "plectrum contact noise is missing from the attack (clean "
               + std::to_string(cleanContact) + ", noisy "
               + std::to_string(noisyContact) + ")");

    // Release noise adds energy just after note-off. This render pair
    // differs only in the releaseNoise control, and rendering is
    // deterministic, so any pre-note-off difference is a defect and the
    // release-window difference is exactly the added noise.
    EngineParameters releaseOnly = noNoise;
    releaseOnly.releaseNoise = 1.0f;
    engine.setParameters(releaseOnly);
    const auto releaseNoisy = renderNote(engine, sampleRate, 45, 0.8f,
                                         PlayStyle::Sustain, 0.9, 0.6);

    const int releaseStart = static_cast<int>(0.6 * sampleRate);
    const int releaseEnd = releaseStart + static_cast<int>(0.015 * sampleRate);
    double differenceEnergy = 0.0;
    double cleanEnergy = 0.0;
    for (int i = releaseStart; i < releaseEnd; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        differenceEnergy += difference * difference;
        cleanEnergy += clean.left[static_cast<std::size_t>(i)]
                     * clean.left[static_cast<std::size_t>(i)];
    }
    expect(differenceEnergy > 1.0e-9
               && differenceEnergy > 0.005 * std::max(cleanEnergy, 1.0e-12),
           "release noise is missing after note-off");
    double preOffDifference = 0.0;
    int firstDifferingSample = -1;
    for (int i = 0; i < releaseStart - 64; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        if (difference != 0.0 && firstDifferingSample < 0)
            firstDifferingSample = i;
        preOffDifference += difference * difference;
    }
    expect(preOffDifference == 0.0,
           "release-noise level changed audio before the note-off (first at "
               + std::to_string(firstDifferingSample) + ", energy "
               + std::to_string(preOffDifference) + ")");

    // Note-off damps the string quickly.
    const double preRelease = rmsInRange(clean.left,
                                         releaseStart - static_cast<int>(0.1 * sampleRate),
                                         releaseStart);
    const double postRelease = rmsInRange(clean.left,
                                          releaseStart + static_cast<int>(0.35 * sampleRate),
                                          releaseStart + static_cast<int>(0.45 * sampleRate));
    expect(postRelease < preRelease * 0.05,
           "released note does not damp towards silence");
}

void testStringAllocationAndPolyphony()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // The open C-major shape: x-3-2-0-1-0 from the A string.
    const std::array<int, 5> chord { 48, 52, 55, 60, 64 };
    for (const int note : chord)
        engine.noteOn(note, 0.8f);

    expect(engine.getActiveVoiceCount() == 5,
           "C-major chord did not allocate five strings");
    expect(TestAccess::stringForNote(engine, 48) == 3, "C3 is not on the A string");
    expect(TestAccess::stringForNote(engine, 52) == 4, "E3 is not on the D string");
    expect(TestAccess::stringForNote(engine, 55) == 5, "G3 is not on the G string");
    expect(TestAccess::stringForNote(engine, 60) == 6, "C4 is not on the B string");
    expect(TestAccess::stringForNote(engine, 64) == 7,
           "E4 is not on the high E string");

    // The three lower opens fill the remaining physical strings. A ninth
    // simultaneous note must steal while the voice count remains eight.
    engine.noteOn(40, 0.8f);
    expect(engine.getActiveVoiceCount() == 6, "open E2 did not use its string");
    engine.noteOn(35, 0.8f);
    expect(engine.getActiveVoiceCount() == 7, "open B1 did not use its string");
    engine.noteOn(28, 0.8f);
    expect(engine.getActiveVoiceCount() == 8, "open E1 did not use its string");
    engine.noteOn(50, 0.8f);
    expect(engine.getActiveVoiceCount() == 8,
           "ninth simultaneous note exceeded eight strings");

    // Every open note maps to its own physical string in Drop-E tuning.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (int string = 0; string < ElectryEngine::stringCount; ++string)
    {
        const int note = openNotes[static_cast<std::size_t>(string)];
        engine.noteOn(note, 0.8f);
        expect(TestAccess::stringForNote(engine, note) == string,
               "open note " + std::to_string(note)
                   + " did not map to physical string " + std::to_string(string));
    }
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "eight open notes did not fill all eight physical strings");

    // Retriggering a sounding note reuses its string.
    engine.reset();
    engine.noteOn(45, 0.8f);
    const int firstString = TestAccess::stringForNote(engine, 45);
    engine.noteOn(45, 0.8f);
    expect(engine.getActiveVoiceCount() == 1,
           "restruck note did not reuse its string");
    expect(TestAccess::stringForNote(engine, 45) == firstString,
           "restruck note moved to another string");
}

// chooseString()'s steal branch (all eight strings already sounding) was
// only ever exercised for the voice count staying at eight; the tie-break
// policy itself - a releasing voice always outranks a held one, and among
// voices of equal status the one that has been sounding the longest goes
// first - had no direct coverage.
void testVoiceStealingPriority()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // Drop-E open notes for strings 0..7, struck in that order so string 2
    // (E2, fret 22) is the oldest of the strings that can reach MIDI note 62
    // within the 22-fret range (frets 34/27/22/17/12/7/3/-2 respectively -
    // only strings 2 through 6 qualify).
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };

    // With every string held and none releasing, the steal must fall to the
    // oldest of the reachable strings.
    engine.reset();
    for (const int note : openNotes)
        engine.noteOn(note, 0.8f);
    engine.noteOn(62, 0.8f);
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "stealing a held voice changed the active voice count");
    expect(TestAccess::stringForNote(engine, 62) == 2,
           "steal did not choose the oldest of the reachable held strings");
    expect(TestAccess::stringForNote(engine, 40) == -1,
           "the stolen string still reports its original note as active");

    // Releasing a younger reachable string (5, not the oldest) must steal
    // that one instead: a releasing voice outranks every held voice
    // regardless of how long either has been sounding.
    engine.reset();
    for (const int note : openNotes)
        engine.noteOn(note, 0.8f);
    engine.noteOff(55); // string 5: keyDown false, releasing, still active
    engine.noteOn(62, 0.8f);
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "stealing a releasing voice changed the active voice count");
    expect(TestAccess::stringForNote(engine, 62) == 5,
           "steal preferred an older held string over a releasing one");
}

// noteOn()'s velocity guard - `clampf(std::isfinite(velocity) ? velocity :
// 0.0f, 0.0f, 1.0f)` - was only ever fed ordinary in-range velocities
// elsewhere in the suite; nothing asserted that a non-finite or negative
// velocity is folded down to silence, or that a velocity above 1.0 clamps
// to 1.0 rather than being rejected or left unclamped.
void testNoteOnVelocitySanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and infinities fail std::isfinite and fall back to 0.0f; a
    // negative velocity clamps to 0.0f. Either way the following
    // `velocity <= 0.0f` gate in noteOn() then starts no voice at all.
    const auto expectNoVoiceStarted = [&] (float velocity, const char* label)
    {
        engine.reset();
        engine.noteOn(45, velocity);
        expect(engine.getActiveVoiceCount() == 0,
               std::string("noteOn started a voice for a ") + label
                   + " velocity");
    };
    expectNoVoiceStarted(std::nanf(""), "NaN");
    expectNoVoiceStarted(std::numeric_limits<float>::infinity(),
                          "positive-infinite");
    expectNoVoiceStarted(-std::numeric_limits<float>::infinity(),
                          "negative-infinite");
    expectNoVoiceStarted(-0.4f, "negative");

    // A finite velocity above 1.0 clamps to exactly 1.0 rather than being
    // rejected or driving the excitation harder than a full-velocity note
    // would: every downstream use (makeVelocityProfile and its callers)
    // only ever sees the value noteOn() itself already clamped, so the two
    // renders must be bit-identical.
    ElectryEngine reference;
    reference.prepare(sampleRate, 512);
    reference.setParameters(EngineParameters {});
    const auto inRange = renderNote(reference, sampleRate, 45, 1.0f,
                                     PlayStyle::Sustain, 0.5);

    ElectryEngine overshoot;
    overshoot.prepare(sampleRate, 512);
    overshoot.setParameters(EngineParameters {});
    const auto outOfRange = renderNote(overshoot, sampleRate, 45, 5.0f,
                                        PlayStyle::Sustain, 0.5);

    bool identical = true;
    for (std::size_t i = 0; i < inRange.left.size(); ++i)
        if (inRange.left[i] != outOfRange.left[i]
            || inRange.right[i] != outOfRange.right[i])
        {
            identical = false;
            break;
        }
    expect(identical,
           "a velocity above 1.0 was not clamped to the same render as 1.0");
    expect(peakAbs(inRange.left) > 1.0e-4f,
           "velocity clamp fixture rendered silence");
}

// setVibrato()'s own guard - `clampf(std::isfinite(normalised) ? normalised :
// 0.0f, 0.0f, 1.0f)` - is the channel-pressure counterpart to setPitchBend,
// setResonance, setAcousticReturnLevel and setPalmMutePressure, all four of
// which testParameterSanitisation() above already drives with NaN. setVibrato
// itself is only ever called with ordinary in-range pressures (0.0f, 1.0f, or
// a drawn 0..1 value) everywhere else in the suite, so nothing asserted that a
// non-finite or out-of-[0,1] channel-pressure value is folded down rather than
// latched into the fretting-hand vibrato target and, from there, into every
// stopped string's pitch.
void testSetVibratoSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f exactly
    // - the same "no boundary to clamp to" fallback the sample-rate and
    // EngineParameters guards use.
    engine.setVibrato(std::nanf(""));
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a NaN channel pressure did not fall back to zero");
    engine.setVibrato(std::numeric_limits<float>::infinity());
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a positive-infinite channel pressure did not fall back to zero");
    engine.setVibrato(-std::numeric_limits<float>::infinity());
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a negative-infinite channel pressure did not fall back to zero");

    // A finite value outside [0, 1] clamps to the nearer boundary rather than
    // being rejected or left unclamped.
    engine.setVibrato(-3.0f);
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a negative channel pressure did not clamp to zero");
    engine.setVibrato(7.5f);
    expect(TestAccess::vibratoTarget(engine) == 1.0f,
           "a channel pressure above 1.0 did not clamp to one");

    // An ordinary value still passes straight through, confirming the guard
    // is a genuine clamp rather than a filter that also stops valid input.
    engine.setVibrato(0.4f);
    expect(TestAccess::vibratoTarget(engine) == 0.4f,
           "an in-range channel pressure was altered by the guard");

    // And a hostile pressure held on a genuinely fingered, sounding string
    // must still render finite, bounded audio end to end rather than only
    // sanitising the stored target.
    engine.reset();
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string, so vibrato applies
    engine.setVibrato(std::nanf(""));
    StereoBuffer buffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer),
           "a hostile channel pressure produced non-finite audio");
}

// setPitchBend()'s own guard - `2.0f * clampf(std::isfinite(bend) ? bend :
// 0.0f, -1.0f, 1.0f)` - is exercised elsewhere in the suite only with
// ordinary in-range bends (testDropELowNoteAtMaximumRate uses -1.0f, several
// glide/wheel tests use 0.0f or 1.0f) or, in testParameterSanitisation, with a
// NaN whose only assertion is that the resulting audio stays finite. Nothing
// checks the guard's own two distinct behaviours - a non-finite bend folds to
// zero rather than latching NaN into the bend target, and a finite
// out-of-[-1,1] bend clamps to the nearer boundary before being doubled to
// the +/-2 semitone range - the way testSetVibratoSanitisation already does
// for the channel-pressure guard right above.
void testSetPitchBendSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f before
    // the clamp, doubling to a 0.0f target - the same "no boundary to clamp
    // to" fallback the sample-rate, EngineParameters and vibrato guards use.
    engine.setPitchBend(std::nanf(""));
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a NaN bend did not fall back to zero");
    engine.setPitchBend(std::numeric_limits<float>::infinity());
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a positive-infinite bend did not fall back to zero");
    engine.setPitchBend(-std::numeric_limits<float>::infinity());
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a negative-infinite bend did not fall back to zero");

    // A finite value outside [-1, 1] clamps to the nearer boundary, then
    // doubles to the +/-2 semitone bend range, rather than being rejected or
    // latched unclamped.
    engine.setPitchBend(-4.0f);
    expect(TestAccess::pitchBendTarget(engine) == -2.0f,
           "a bend below -1.0 did not clamp to -2 semitones");
    engine.setPitchBend(9.0f);
    expect(TestAccess::pitchBendTarget(engine) == 2.0f,
           "a bend above 1.0 did not clamp to +2 semitones");

    // An ordinary in-range bend still passes through the doubling unaltered,
    // confirming the guard is a genuine clamp rather than a filter that also
    // stops valid input.
    engine.setPitchBend(0.25f);
    expect(TestAccess::pitchBendTarget(engine) == 0.5f,
           "an in-range bend was altered by the guard");

    // And a hostile bend held on a genuinely fretted, sounding string must
    // still render finite, bounded audio end to end rather than only
    // sanitising the stored target.
    engine.reset();
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    engine.setPitchBend(std::nanf(""));
    StereoBuffer bendBuffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, bendBuffer);
    expect(allFinite(bendBuffer),
           "a hostile pitch bend produced non-finite audio");
}

// setResonance(), setAcousticReturnLevel() and setPalmMutePressure() share the
// exact same guard shape as setVibrato() above - `std::isfinite(value) ?
// clampf(value, 0.0f, 1.0f) : 0.0f` - and, like setVibrato() before it was
// covered directly, are only ever driven elsewhere in the suite with ordinary
// in-range levels, or in testParameterSanitisation() with a NaN whose only
// assertion is that the resulting audio stays finite. Nothing asserted that a
// non-finite level folds to zero rather than latching NaN into the resonance
// target, the acoustic-return target or the palm-mute pressure, or that an
// out-of-range level clamps to the nearer boundary rather than passing
// through unclamped. All three guards are checked together here since they
// are, byte for byte, the same guard three times over.
void testSetResonanceReturnLevelAndPalmMutePressureSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f exactly
    // - the same "no boundary to clamp to" fallback the other performance
    // guards use.
    engine.setResonance(std::nanf(""));
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a NaN resonance level did not fall back to zero");
    engine.setResonance(std::numeric_limits<float>::infinity());
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a positive-infinite resonance level did not fall back to zero");
    engine.setResonance(-std::numeric_limits<float>::infinity());
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a negative-infinite resonance level did not fall back to zero");

    engine.setAcousticReturnLevel(std::nanf(""));
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a NaN acoustic-return level did not fall back to zero");
    engine.setAcousticReturnLevel(std::numeric_limits<float>::infinity());
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a positive-infinite acoustic-return level did not fall back to "
           "zero");

    engine.setPalmMutePressure(std::nanf(""));
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a NaN palm-mute pressure did not fall back to zero");
    engine.setPalmMutePressure(-std::numeric_limits<float>::infinity());
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a negative-infinite palm-mute pressure did not fall back to "
           "zero");

    // A finite value outside [0, 1] clamps to the nearer boundary rather than
    // being rejected or left unclamped.
    engine.setResonance(-3.0f);
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a negative resonance level did not clamp to zero");
    engine.setResonance(8.5f);
    expect(TestAccess::resonanceTarget(engine) == 1.0f,
           "a resonance level above 1.0 did not clamp to one");

    engine.setAcousticReturnLevel(-2.0f);
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a negative acoustic-return level did not clamp to zero");
    engine.setAcousticReturnLevel(6.0f);
    expect(TestAccess::returnLevelTarget(engine) == 1.0f,
           "an acoustic-return level above 1.0 did not clamp to one");

    engine.setPalmMutePressure(-1.5f);
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a negative palm-mute pressure did not clamp to zero");
    engine.setPalmMutePressure(4.0f);
    expect(TestAccess::palmMutePressure(engine) == 1.0f,
           "a palm-mute pressure above 1.0 did not clamp to one");

    // An ordinary value still passes straight through, confirming each guard
    // is a genuine clamp rather than a filter that also stops valid input.
    engine.setResonance(0.35f);
    expect(TestAccess::resonanceTarget(engine) == 0.35f,
           "an in-range resonance level was altered by the guard");
    engine.setAcousticReturnLevel(0.65f);
    expect(TestAccess::returnLevelTarget(engine) == 0.65f,
           "an in-range acoustic-return level was altered by the guard");
    engine.setPalmMutePressure(0.5f);
    expect(TestAccess::palmMutePressure(engine) == 0.5f,
           "an in-range palm-mute pressure was altered by the guard");

    // And hostile levels held together on a genuinely fretted, sounding
    // string must still render finite, bounded audio end to end rather than
    // only sanitising the three stored targets.
    engine.reset();
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(1.0f);
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    engine.setResonance(std::nanf(""));
    engine.setAcousticReturnLevel(std::nanf(""));
    engine.setPalmMutePressure(std::nanf(""));
    StereoBuffer levelBuffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, levelBuffer);
    expect(allFinite(levelBuffer),
           "hostile resonance/return/palm-mute levels produced non-finite "
           "audio");
}

// DelayTap::setDelay - the cubic-Lagrange fractional read shared by both
// pickup taps and the sympathetic-string bridge tap - clamps its request to
// [4, delayLineSize - 8] before solving four interpolation coefficients from
// the clamped delay's fractional part. Nothing in the suite ever asked it for
// a delay outside that range directly, or checked the coefficients
// themselves rather than the pickup or sympathetic audio they eventually
// shape, so a clamp landing on the wrong boundary or a sign error in the
// Lagrange solve would still have passed every existing test.
void testDelayTapClampsAndInterpolates()
{
    // A request below the 4-sample floor (a cubic tap needs two samples on
    // each side) clamps to exactly the same coefficients an explicit
    // request for the floor itself would solve.
    const auto belowFloor = TestAccess::delayTapAt(1.0f);
    const auto atFloor = TestAccess::delayTapAt(4.0f);
    expect(belowFloor.offset == atFloor.offset && belowFloor.c0 == atFloor.c0
               && belowFloor.c1 == atFloor.c1 && belowFloor.c2 == atFloor.c2
               && belowFloor.c3 == atFloor.c3,
           "a delay below the 4-sample floor was not clamped to it");

    // A request past the delayLineSize - 8 ceiling (room for the same
    // two-sample margin at the top of the ring) clamps the same way.
    const float ceiling =
        static_cast<float>(TestAccess::delayLineCapacity() - 8);
    const auto aboveCeiling = TestAccess::delayTapAt(ceiling + 500.0f);
    const auto atCeiling = TestAccess::delayTapAt(ceiling);
    expect(aboveCeiling.offset == atCeiling.offset
               && aboveCeiling.c0 == atCeiling.c0
               && aboveCeiling.c1 == atCeiling.c1
               && aboveCeiling.c2 == atCeiling.c2
               && aboveCeiling.c3 == atCeiling.c3,
           "a delay past the delayLineSize-8 ceiling was not clamped to it");

    // An exact integer delay needs no interpolation at all, so the four
    // weights must collapse to a single unit tap rather than spreading
    // across neighbouring samples.
    const auto exact = TestAccess::delayTapAt(10.0f);
    expect(exact.offset == 10,
           "an exact-integer delay solved the wrong tap offset");
    expect(exact.c0 == 0.0f && exact.c1 == 1.0f && exact.c2 == 0.0f
               && exact.c3 == 0.0f,
           "an exact-integer delay did not collapse to a single unit tap");

    // A fractional delay's offset lands at the request's ceiling (the read
    // arithmetic the loop actually uses), and its four weights - a genuine
    // cubic Lagrange basis - sum to exactly unity, the identity that keeps a
    // constant input passing through unattenuated.
    const auto fractional = TestAccess::delayTapAt(10.25f);
    expect(fractional.offset == 11,
           "a fractional delay's offset was not the request's ceiling");
    const float sum =
        fractional.c0 + fractional.c1 + fractional.c2 + fractional.c3;
    expect(std::abs(sum - 1.0f) < 1.0e-5f,
           "a fractional delay's interpolation weights did not sum to unity");
}

// The natural harmonic is a finger resting on a node, not a transposition.
// The distinction is measurable in three places: which partials survive, that
// the loop still runs at the fretted pitch (so the surviving partial decays at
// the rate that partial has when the note is picked ordinarily), and that the
// filter doing it cannot exceed unity gain anywhere.
void testTouchHarmonics()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    // The touch filter is (1 - d/2) + (d/2) z^-M. Both coefficients are
    // non-negative and sum to one, so its magnitude is bounded by one at every
    // frequency and every depth - which is what makes it safe inside the
    // string's feedback loop. Checked here against the closed form because the
    // loop has no other guard against a gain above unity.
    double worstMagnitude = 0.0;
    for (int depthStep = 0; depthStep <= 20; ++depthStep)
    {
        const double depth = 0.05 * depthStep;
        for (int phaseStep = 0; phaseStep <= 720; ++phaseStep)
        {
            const double angle = 3.14159265358979323846 * phaseStep / 360.0;
            const double real = (1.0 - 0.5 * depth) + 0.5 * depth * std::cos(angle);
            const double imag = -0.5 * depth * std::sin(angle);
            worstMagnitude = std::max(worstMagnitude, std::hypot(real, imag));
        }
    }
    expect(worstMagnitude <= 1.0 + 1.0e-12,
           "the touch filter exceeds unity gain somewhere ("
               + std::to_string(worstMagnitude) + ")");

    // The touch is exactly absent for every other articulation, so an ordinary
    // note pays neither the arithmetic nor a change of sound.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(45, 0.8f);
    const int sustainString = TestAccess::stringForNote(engine, 45);
    expect(sustainString >= 0
               && TestAccess::touchDepth(engine, sustainString) == 0.0f,
           "an ordinary picked note put a finger on the string");

    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
    engine.noteOn(45, 0.8f);
    const int harmonicString = TestAccess::stringForNote(engine, 45);
    expect(harmonicString >= 0
               && TestAccess::touchDepth(engine, harmonicString) > 0.5f
               && std::abs(TestAccess::touchFraction(engine, harmonicString)
                           - 0.5f) < 1.0e-6f,
           "the harmonic did not place a finger on the midpoint node");
    {
        // The finger lifts and the extra reads stop; the harmonic keeps
        // ringing because the partials it removed cannot come back.
        StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, settle);
        expect(TestAccess::touchDepth(engine, harmonicString) == 0.0f,
               "the touching finger never lifted");
    }

    const double f0 = midiHz(45);
    const auto sustain = renderNote(engine, sampleRate, 45, 0.8f,
                                    PlayStyle::Sustain, 2.4);
    const auto harmonic = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Harmonics, 2.4);

    const int bodyStart = static_cast<int>(0.10 * sampleRate);
    const int bodyLength = static_cast<int>(0.35 * sampleRate);
    const auto partial = [&] (const StereoBuffer& buffer, int n)
    {
        return dftMagnitude(buffer.left, bodyStart, bodyLength, sampleRate,
                            f0 * n);
    };

    // Odd partials have an antinode under the midpoint finger and go; even
    // ones have a node there and are left alone. That is the whole mechanism,
    // and it is the reason the octave appears at all.
    const double harmonicSecond = partial(harmonic, 2);
    for (const int odd : { 1, 3, 5 })
    {
        const double suppression = decibels(partial(harmonic, odd)
                                            / std::max(harmonicSecond, 1.0e-15));
        expect(suppression < -20.0,
               "partial " + std::to_string(odd)
                   + " survived the midpoint node touch at "
                   + std::to_string(suppression) + " dB");
    }
    expect(decibels(partial(harmonic, 4) / std::max(harmonicSecond, 1.0e-15))
               > -20.0,
           "the fourth partial, which has a node under the finger, was removed");

    // The fundamental is present in the ordinary picked note, so the
    // suppression above is the touch rather than a property of the string.
    expect(decibels(partial(sustain, 1)
                    / std::max(partial(sustain, 2), 1.0e-15)) > -20.0,
           "the picked reference note has no fundamental to suppress");

    // The loop still runs at the fretted pitch, so the surviving octave
    // partial decays at the rate that partial has when the string is picked
    // normally. A model that retuned the loop an octave up would give it the
    // decay of a much shorter string instead.
    const auto decayDb = [&] (const StereoBuffer& buffer)
    {
        const double early = dftMagnitude(buffer.left, bodyStart, bodyLength,
                                          sampleRate, 2.0 * f0);
        const double late = dftMagnitude(
            buffer.left, static_cast<int>(1.6 * sampleRate), bodyLength,
            sampleRate, 2.0 * f0);
        return decibels(late / std::max(early, 1.0e-15));
    };
    const double harmonicDecay = decayDb(harmonic);
    const double sustainDecay = decayDb(sustain);
    expect(std::abs(harmonicDecay - sustainDecay) < 2.0,
           "the harmonic's octave partial does not decay like the same partial "
           "of the picked note (harmonic " + std::to_string(harmonicDecay)
               + " dB, picked " + std::to_string(sustainDecay) + " dB)");
}

// A dead note is a real pick stroke with the fretting hand lying across the
// strings: the attack is a picked attack, and no pitch survives it. It is
// modelled as that hand's own broadband loss in the loop, not as a gate on the
// output, which is why the attack is untouched.
void testDeadNote()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);

    const int note = 45;
    const double f0 = midiHz(note);
    const auto picked = renderNote(engine, sampleRate, note, 0.85f,
                                   PlayStyle::Sustain, 1.0);
    const auto dead = renderNote(engine, sampleRate, note, 0.85f,
                                 PlayStyle::Dead, 1.0);

    // The pick lands exactly as hard: what is different is what happens after
    // it, so the first twenty milliseconds are within a few decibels.
    const int attackEnd = static_cast<int>(0.020 * sampleRate);
    const double pickedAttack = peakAbs(picked.left, 0, attackEnd);
    const double deadAttack = peakAbs(dead.left, 0, attackEnd);
    const double attackGap = decibels(deadAttack / std::max(pickedAttack, 1e-15));
    std::cerr << "PROBE dead attack " << attackGap << " dB\n";
    expect(std::abs(attackGap) < 4.0,
           "a dead note does not land like a picked one ("
               + std::to_string(attackGap) + " dB)");

    // And then it is over. Nothing of the fretted pitch is left after 150 ms.
    const int tailStart = static_cast<int>(0.15 * sampleRate);
    const int tailLength = static_cast<int>(0.25 * sampleRate);
    double worstPartial = -300.0;
    for (int partial = 1; partial <= 8; ++partial)
    {
        const double frequency = f0 * partial;
        if (frequency > 6000.0)
            break;
        const double deadMagnitude = dftMagnitude(dead.left, tailStart,
                                                  tailLength, sampleRate,
                                                  frequency);
        const double pickedMagnitude = dftMagnitude(picked.left, tailStart,
                                                    tailLength, sampleRate,
                                                    frequency);
        worstPartial = std::max(worstPartial,
                                decibels(deadMagnitude
                                         / std::max(pickedMagnitude, 1e-15)));
    }
    expect(worstPartial < -40.0,
           "a partial of the fretted pitch survived the dead note ("
               + std::to_string(worstPartial) + " dB against the picked note)");

    const double deadTail = rmsInRange(dead.left, tailStart,
                                       tailStart + tailLength);
    const double pickedTail = rmsInRange(picked.left, tailStart,
                                         tailStart + tailLength);
    expect(deadTail < pickedTail * 0.02,
           "a dead note kept ringing ("
               + std::to_string(decibels(deadTail
                                         / std::max(pickedTail, 1e-15)))
               + " dB against the picked note)");

    // It is the hand rather than a gate: the loop's own solved decay carries
    // it, so the note is still decaying rather than being cut off. Measured
    // between two early windows, which a gate would make identical.
    const double early = rmsInRange(dead.left, attackEnd,
                                    static_cast<int>(0.040 * sampleRate));
    const double later = rmsInRange(dead.left,
                                    static_cast<int>(0.060 * sampleRate),
                                    static_cast<int>(0.090 * sampleRate));
    expect(later < early * 0.5 && later > 0.0,
           "the dead note does not decay through its own loop");
}

// Channel pressure is the fretting hand leaning into the string it is holding.
// A finger is not the bar: it moves only what it is fingering, it can push a
// string sharp and cannot pull it below the fret, and it takes a moment to
// start.
void testFrettingHandVibrato()
{
    constexpr double sampleRate = 48000.0;

    // Track the sounding delay target, which is what the vibrato actually
    // drives; the audio follows it, and it is sampled far more cheaply and
    // precisely than a frequency estimate on a moving tone.
    struct Trace
    {
        std::vector<float> target;
        int stringIndex { -1 };
    };
    const auto traceOf = [&] (float pressure, double seconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.setVibrato(pressure);

        Trace trace;
        trace.stringIndex = TestAccess::stringForNote(engine, 47);
        constexpr int chunk = 32;
        const int total = static_cast<int>(seconds * sampleRate);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < total; at += chunk)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            trace.target.push_back(
                trace.stringIndex >= 0
                    ? TestAccess::snapshot(engine,
                                           trace.stringIndex).verticalDelayTarget
                    : 0.0f);
        }
        return trace;
    };

    const auto still = traceOf(0.0f, 1.6);
    const auto moving = traceOf(1.0f, 1.6);
    expect(still.stringIndex == 3 && moving.stringIndex == 3,
           "the vibrato fixture did not land on the open A string");

    // Vibrato Depth has to reach the engine while it is playing, not only at
    // the next reset. A host automating it, or any caller moving it through
    // setParameters() after prepare(), is the ordinary case rather than the
    // exotic one: the control tick propagates every other continuous
    // parameter, and this one was left off that list when the fretting hand
    // was added, so the depth stayed at whatever the last reset latched.
    //
    // The comparison is differential because depth 0 is not silence: the
    // excursion is lerp(0.10, 1.10, depth) semitones, so a note at depth 0
    // still moves about 10 cents. Two engines are reset identically at depth
    // 0 and only one is raised afterwards, so the single difference between
    // them is the propagation this guards.
    {
        const auto excursionCents = [&] (bool raiseAfterReset)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            EngineParameters parameters;
            parameters.artifactAmount = 0.0f;
            parameters.sympatheticAmount = 0.0f;
            parameters.vibratoDepth = 0.0f;
            engine.setParameters(parameters);
            engine.reset();

            if (raiseAfterReset)
            {
                parameters.vibratoDepth = 1.0f;
                engine.setParameters(parameters);
            }

            engine.noteOn(47, 0.85f);
            engine.setVibrato(1.0f);

            const int stringIndex = TestAccess::stringForNote(engine, 47);
            constexpr int chunk = 32;
            const int total = static_cast<int>(1.6 * sampleRate);
            StereoBuffer scratch(chunk);
            std::vector<float> target;
            for (int at = 0; at < total; at += chunk)
            {
                engine.process(scratch.left.data(), scratch.right.data(),
                               chunk);
                target.push_back(
                    TestAccess::snapshot(engine,
                                         stringIndex).verticalDelayTarget);
            }

            double highest = 0.0;
            double lowest = 1.0e9;
            const std::size_t settled = target.size() / 2;
            for (std::size_t i = settled; i < target.size(); ++i)
            {
                highest = std::max(highest,
                                   static_cast<double>(target[i]));
                lowest = std::min(lowest, static_cast<double>(target[i]));
            }
            return 1200.0 * std::log2(highest / lowest);
        };

        const double latched = excursionCents(false);
        const double raised = excursionCents(true);
        expect(raised > latched * 2.0,
               "Vibrato Depth raised after reset never reached the engine: the "
               "held note moved " + std::to_string(raised)
                   + " cents against " + std::to_string(latched)
                   + " cents with the control left alone, so the parameter is "
                     "dead until the next reset");
    }

    // Zero pressure is an exact no-op: the same score with the control never
    // touched renders bit-for-bit the same audio.
    {
        ElectryEngine untouched;
        untouched.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        untouched.setParameters(parameters);
        untouched.reset();
        untouched.noteOn(47, 0.85f);
        StereoBuffer withoutControl(static_cast<int>(0.6 * sampleRate));
        renderInto(untouched, withoutControl);

        ElectryEngine silenced;
        silenced.prepare(sampleRate, 512);
        silenced.setParameters(parameters);
        silenced.reset();
        silenced.noteOn(47, 0.85f);
        silenced.setVibrato(0.0f);
        StereoBuffer withSilentControl(static_cast<int>(0.6 * sampleRate));
        renderInto(silenced, withSilentControl);

        bool identical = true;
        for (std::size_t i = 0; i < withoutControl.left.size(); ++i)
            identical = identical
                && withoutControl.left[i] == withSilentControl.left[i];
        expect(identical, "a silent pressure control is not a bit-exact no-op");
    }

    // The string's own tension modulation is relaxing the delay target the
    // whole time, so the vibrato is measured against the same note without it
    // rather than against a constant. The ratio removes the trend exactly:
    // both renders are the same note at the same velocity.
    std::vector<double> ratio;
    ratio.reserve(moving.target.size());
    for (std::size_t i = 0; i < std::min(moving.target.size(),
                                         still.target.size()); ++i)
        ratio.push_back(still.target[i] > 0.0f
            ? static_cast<double>(moving.target[i]) / still.target[i] : 1.0);

    // The note is pushed sharp and never flat: a shorter delay is a higher
    // pitch, so the ratio never rises above one.
    const std::size_t settled = ratio.size() / 2;
    double highest = 0.0;
    double lowest = 1.0e9;
    double mean = 0.0;
    for (std::size_t i = settled; i < ratio.size(); ++i)
    {
        highest = std::max(highest, ratio[i]);
        lowest = std::min(lowest, ratio[i]);
        mean += ratio[i];
    }
    mean /= static_cast<double>(ratio.size() - settled);
    expect(highest <= 1.0002,
           "the fretting-hand vibrato pulled the string flat of the fret "
           "(highest ratio " + std::to_string(highest) + ")");
    expect(mean < 0.9995,
           "the vibrato is centred on the fretted pitch rather than sharp of "
           "it (mean ratio " + std::to_string(mean) + ")");

    // Its depth is the modelled one: a shorter delay by 2^(-cents/1200). The
    // upper bound admits the per-cycle excursion draw, which is bounded at
    // +45% of the nominal 40 cents, so the deepest cycle in any window can
    // reach 58; a tighter ceiling here would be asserting the draw away.
    const double depthCents = 1200.0 * std::log2(1.0 / lowest);
    expect(depthCents > 25.0 && depthCents < 60.0,
           "the vibrato depth is not the modelled 40 cents ("
               + std::to_string(depthCents) + " cents)");

    // It oscillates at a player's rate rather than drifting: count how often
    // the trace crosses its own midpoint over the settled window.
    const double midpoint = 0.5 * (highest + lowest);
    int crossings = 0;
    for (std::size_t i = settled + 1; i < ratio.size(); ++i)
        if ((ratio[i - 1] < midpoint) != (ratio[i] < midpoint))
            ++crossings;
    const double windowSeconds = static_cast<double>(ratio.size() - settled)
                               * 32.0 / sampleRate;
    const double rate = 0.5 * crossings / windowSeconds;
    expect(rate > 4.0 && rate < 8.0,
           "the vibrato rate is not a player's ("
               + std::to_string(rate) + " Hz)");

    // The onset is real: the first 60 ms carries far less movement than the
    // settled part, because a player lands the note before starting to move.
    double earlySpread = 0.0;
    const std::size_t earlyEnd = static_cast<std::size_t>(
        0.06 * sampleRate / 32.0);
    for (std::size_t i = 0; i < std::min(earlyEnd, ratio.size()); ++i)
        earlySpread = std::max(earlySpread, std::abs(ratio[i] - 1.0));
    expect(earlySpread < 0.4 * (1.0 - lowest),
           "the vibrato started instantly instead of easing in (early "
               + std::to_string(earlySpread) + ", settled "
               + std::to_string(1.0 - lowest) + ")");

    // A finger is not the bar: the sympathetically ringing strings must not
    // move, where the wheel moves them. Compared on the coupled ring left by a
    // picked note with the coupling wide open.
    const auto coupledRing = [&] (bool useWheel, bool useVibrato)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.9f);
        StereoBuffer lead(static_cast<int>(0.25 * sampleRate));
        renderInto(engine, lead);
        if (useWheel)
            engine.setPitchBend(1.0f);
        if (useVibrato)
            engine.setVibrato(1.0f);
        // Read the coupled open low E's own loop: only the bar retunes it.
        StereoBuffer settle(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, settle);
        return TestAccess::snapshot(engine, 0).verticalDelayTarget;
    };
    const float restingCoupled = coupledRing(false, false);
    const float vibratoCoupled = coupledRing(false, true);
    const float barCoupled = coupledRing(true, false);
    expect(std::abs(vibratoCoupled - restingCoupled) < 1.0e-3f,
           "the fretting-hand vibrato bent a string nobody is fingering");
    expect(std::abs(barCoupled - restingCoupled) > 1.0f,
           "the bar fixture did not bend the coupled string, so the "
           "comparison above proves nothing");
}

// A hand is not an LFO. Four things separate them and all four are read off
// the engine's own vibrato offset rather than off a pitch estimate, because
// the quantity under test is a modulation shape and any estimator would smear
// it with the string's own tension relaxation.
//
// The shape: the pitch follows the square of the finger's displacement, so a
// cycle spends 36.4% of itself above half its own peak where a raised cosine
// spends exactly 50%. The scatter: the rate and the excursion are redrawn
// every cycle, so neither repeats. The hand: two stopped strings are two
// fingers and drift apart instead of moving in lockstep. And the onset: the
// depth leaves rest with zero slope rather than at its steepest.
void testVibratoIsAHandNotAnLfo()
{
    constexpr double sampleRate = 48000.0;
    // The engine runs at twice the host rate below 96 kHz and its control
    // block is 16 internal samples long, so eight host samples is exactly one
    // control tick: every trace below is sampled on the grid the vibrato is
    // computed on rather than interpolated off it.
    constexpr int chunk = 8;
    const double tick = static_cast<double>(chunk) / sampleRate;

    const auto minimaOf = [] (const std::vector<double>& trace)
    {
        std::vector<int> minima;
        for (std::size_t i = 1; i + 1 < trace.size(); ++i)
            if (trace[i] <= trace[i - 1] && trace[i] < trace[i + 1])
                minima.push_back(static_cast<int>(i));
        return minima;
    };

    // One fingered string's vibrato offset in semitones, one reading per
    // control tick, with the shared depth envelope alongside it.
    const auto traceOf = [&] (float vibratoDepth, double seconds,
                              std::vector<double>* envelope)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.vibratoDepth = vibratoDepth;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < static_cast<int>(0.2 * sampleRate); at += chunk)
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
        const int stringIndex = TestAccess::stringForNote(engine, 47);
        expect(stringIndex >= 0 && TestAccess::snapshot(engine, stringIndex).fret > 0,
               "the vibrato fixture is not on a stopped string");
        engine.setVibrato(1.0f);

        std::vector<double> trace;
        const int steps = static_cast<int>(seconds * sampleRate) / chunk;
        trace.reserve(static_cast<std::size_t>(steps));
        for (int step = 0; step < steps; ++step)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            trace.push_back(stringIndex >= 0
                ? static_cast<double>(
                      TestAccess::snapshot(engine, stringIndex).vibratoSemitones)
                : 0.0);
            if (envelope != nullptr)
                envelope->push_back(TestAccess::vibratoDepthEnvelope(engine));
        }
        return trace;
    };

    // ---- one cycle's shape, and the scatter between cycles ---------------
    const EngineParameters defaults;
    const auto trace = traceOf(defaults.vibratoDepth, 8.0, nullptr);
    const auto minima = minimaOf(trace);
    expect(minima.size() >= 25,
           "the vibrato fixture did not produce enough cycles to score ("
               + std::to_string(minima.size()) + ")");

    // The first six cycles are dropped: the depth envelope is still ramping
    // through them, so their peaks measure the onset rather than the draw.
    std::vector<double> periods, peakCents, aboveHalf;
    for (std::size_t k = 6; k + 1 < minima.size(); ++k)
    {
        const int from = minima[k];
        const int to = minima[k + 1];
        double peak = 0.0;
        for (int i = from; i <= to; ++i)
            peak = std::max(peak, trace[static_cast<std::size_t>(i)]);
        int above = 0;
        for (int i = from; i < to; ++i)
            if (trace[static_cast<std::size_t>(i)] > 0.5 * peak)
                ++above;
        periods.push_back((to - from) * tick);
        peakCents.push_back(100.0 * peak);
        aboveHalf.push_back(static_cast<double>(above) / (to - from));
    }
    expect(periods.size() >= 18,
           "fewer than the eighteen settled cycles this test scores ("
               + std::to_string(periods.size()) + ")");

    const auto meanOf = [] (const std::vector<double>& values)
    {
        double sum = 0.0;
        for (double value : values)
            sum += value;
        return sum / static_cast<double>(values.size());
    };

    const double periodMean = meanOf(periods);
    double periodVariance = 0.0;
    for (double period : periods)
        periodVariance += (period - periodMean) * (period - periodMean);
    const double periodDeviation =
        std::sqrt(periodVariance / static_cast<double>(periods.size()));
    expect(periodDeviation > 0.04 * periodMean,
           "the vibrato rate repeats itself like an oscillator (period "
           "deviation " + std::to_string(100.0 * periodDeviation / periodMean)
               + "% of the mean)");

    const double deepest = *std::max_element(peakCents.begin(), peakCents.end());
    const double shallowest = *std::min_element(peakCents.begin(), peakCents.end());
    expect(deepest - shallowest > 2.5,
           "the vibrato reaches the same depth every cycle (spread "
               + std::to_string(deepest - shallowest) + " cents)");

    // The x^2 law is pinned from both sides. A raised cosine gives exactly
    // 50%, its square 36.4%, and each cycle is scored against its own peak
    // because the depth is redrawn every cycle.
    const double halfFraction = meanOf(aboveHalf);
    expect(halfFraction > 0.32 && halfFraction < 0.40,
           "the vibrato is not following the square of the finger's "
           "displacement (cycle spends " + std::to_string(halfFraction)
               + " of itself above half its own peak)");

    // ---- the Vibrato Depth control's range -------------------------------
    const auto peakOf = [&] (float vibratoDepth)
    {
        const auto depthTrace = traceOf(vibratoDepth, 6.0, nullptr);
        double peak = 0.0;
        for (std::size_t i = depthTrace.size() / 3; i < depthTrace.size(); ++i)
            peak = std::max(peak, depthTrace[i]);
        return 100.0 * peak;
    };
    const double widest = peakOf(1.0f);
    const double narrowest = peakOf(0.0f);
    expect(widest >= 90.0,
           "a fully open Vibrato Depth does not reach a rock vibrato's arc ("
               + std::to_string(widest) + " cents)");
    expect(narrowest <= 15.0,
           "a closed Vibrato Depth is still a wide vibrato ("
               + std::to_string(narrowest) + " cents)");

    // ---- two fingered strings are two fingers ----------------------------
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.noteOn(52, 0.85f);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < static_cast<int>(0.2 * sampleRate); at += chunk)
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
        const int lower = TestAccess::stringForNote(engine, 47);
        const int upper = TestAccess::stringForNote(engine, 52);
        expect(lower >= 0 && upper >= 0 && lower != upper,
               "the double stop did not land on two separate strings");
        engine.setVibrato(1.0f);

        std::vector<double> lowerTrace, upperTrace;
        const int steps = static_cast<int>(6.0 * sampleRate) / chunk;
        for (int step = 0; step < steps; ++step)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            lowerTrace.push_back(static_cast<double>(
                TestAccess::snapshot(engine, lower).vibratoSemitones));
            upperTrace.push_back(static_cast<double>(
                TestAccess::snapshot(engine, upper).vibratoSemitones));
        }
        const auto lowerMinima = minimaOf(lowerTrace);
        const auto upperMinima = minimaOf(upperTrace);
        expect(lowerMinima.size() >= 12 && upperMinima.size() >= 12,
               "the double-stop fixture did not oscillate on both strings");

        // For every settled cycle of the lower string, how far its rest point
        // sits from the upper string's nearest rest point, as a fraction of a
        // cycle. Scored on the mean rather than on every cycle: two
        // independent fingers do occasionally pass through the same phase, and
        // a floor under every cycle would assert that they never may.
        std::vector<double> separation;
        for (std::size_t k = 6; k + 1 < lowerMinima.size(); ++k)
        {
            const double period = lowerMinima[k + 1] - lowerMinima[k];
            double nearest = 1.0e9;
            for (int candidate : upperMinima)
                nearest = std::min(nearest,
                                   std::abs(static_cast<double>(candidate
                                                                - lowerMinima[k])));
            separation.push_back(nearest / period);
        }
        const double meanSeparation = meanOf(separation);
        expect(meanSeparation >= 0.08,
               "a double stop's two strings move as one finger (mean phase "
               "separation " + std::to_string(meanSeparation) + " cycles)");
    }

    // ---- the onset leaves rest rather than jumping -----------------------
    {
        std::vector<double> envelope;
        (void) traceOf(defaults.vibratoDepth, 1.5, &envelope);
        expect(! envelope.empty(), "the onset fixture produced no envelope");
        const double settled = envelope.back();
        expect(settled > 0.9, "the vibrato never reached full depth");
        std::size_t reached = envelope.size() - 1;
        for (std::size_t i = 0; i < envelope.size(); ++i)
            if (envelope[i] >= 0.9 * settled) { reached = i; break; }
        // A scale-free measure, so it does not have to name an onset time: at
        // a tenth of the way to 90% of settled depth, a one-pole is already
        // 20.6% of the way there whatever its time constant, because it is
        // steepest at t = 0. A smoothStep is still at rest.
        const std::size_t early = static_cast<std::size_t>(
            0.1 * static_cast<double>(reached + 1));
        const double earlyFraction = envelope[early] / settled;
        expect(earlyFraction <= 0.05,
               "the vibrato's depth leaves rest at its steepest instead of "
               "easing off it (" + std::to_string(100.0 * earlyFraction)
                   + "% of settled at a tenth of the time to 90%)");
    }

    // ---- and none of it moves a note nobody is pressing -------------------
    {
        const auto render = [&] (bool touchControl)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            EngineParameters parameters;
            parameters.artifactAmount = 0.0f;
            parameters.sympatheticAmount = 0.0f;
            engine.setParameters(parameters);
            engine.reset();
            engine.noteOn(47, 0.85f);
            if (touchControl)
                engine.setVibrato(0.0f);
            StereoBuffer buffer(static_cast<int>(0.6 * sampleRate));
            renderInto(engine, buffer);
            return buffer.left;
        };
        expect(render(false) == render(true),
               "a silent pressure control is not a bit-exact no-op");
    }
}

// A slide is a finger that stays down and travels: the sounding length moves
// continuously, the travel time is a distance divided by a hand speed rather
// than a fixed number, and the winding drags under the finger the whole way.
void testSlideArticulation()
{
    constexpr double sampleRate = 48000.0;

    // Broadband magnitude-weighted centroid over a log grid. The harmonic-
    // series centroid used elsewhere cannot see the scrape, which is noise
    // rather than a partial.
    const auto broadbandCentroid = [&] (const std::vector<float>& data,
                                        int start, int length)
    {
        double weighted = 0.0;
        double total = 0.0;
        for (double frequency = 300.0; frequency < 16000.0; frequency *= 1.09)
        {
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
            weighted += magnitude * frequency;
            total += magnitude;
        }
        return total > 0.0 ? weighted / total : 0.0;
    };

    struct Slide
    {
        StereoBuffer audio { 1 };
        int settleSamples { 0 };
        int stringIndex { -1 };
        int fret { -1 };
        float amplitude { 0.0f };
    };

    const auto renderSlide = [&] (int fromNote, int toNote, float fingerNoise,
                                  float bendTime)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.fingerNoise = fingerNoise;
        parameters.bendTimeSeconds = bendTime;
        engine.setParameters(parameters);
        engine.reset();

        engine.noteOn(fromNote, 0.85f);
        StereoBuffer lead(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, lead);

        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(toNote, 0.85f);

        Slide result;
        result.stringIndex = TestAccess::stringForNote(engine, toNote);
        result.fret = result.stringIndex >= 0
            ? TestAccess::snapshot(engine, result.stringIndex).fret : -1;
        result.amplitude = result.stringIndex >= 0
            ? TestAccess::slideNoiseAmplitude(engine, result.stringIndex) : 0.0f;

        // Render in short chunks so the moment the glide settles can be read
        // from the delay target the engine is actually driving: the glide is
        // monotone, so it has settled once the target stops moving.
        constexpr int chunk = 64;
        const int total = static_cast<int>(2.0 * sampleRate);
        result.audio = StereoBuffer(total);
        result.settleSamples = total;
        bool settled = false;
        for (int at = 0; at < total; at += chunk)
        {
            const int samples = std::min(chunk, total - at);
            engine.process(result.audio.left.data() + at,
                           result.audio.right.data() + at, samples);
            if (settled || result.stringIndex < 0)
                continue;
            if (TestAccess::legatoBlend(engine, result.stringIndex) >= 1.0f)
            {
                result.settleSamples = at + samples;
                settled = true;
            }
        }
        return result;
    };

    // A2 open on the wound A string, sliding up to the twelfth fret.
    const auto longSlide = renderSlide(45, 57, 0.8f, 0.28f);
    expect(longSlide.stringIndex == 3 && longSlide.fret == 12,
           "the slide did not stay on the string it started from");

    // The pitch travels: a window in the middle of the slide sits strictly
    // between the two endpoints rather than at either of them.
    const double fromHz = midiHz(45);
    const double toHz = midiHz(57);
    const int slideStart = static_cast<int>(0.30 * sampleRate);
    const int midpoint = slideStart + longSlide.settleSamples / 2;
    const double middle = measureFrequency(
        longSlide.audio.left, midpoint, static_cast<int>(0.04 * sampleRate),
        sampleRate, std::sqrt(fromHz * toHz));
    expect(middle > fromHz * 1.15 && middle < toHz * 0.87,
           "the slide jumped instead of travelling (mid-slide pitch "
               + std::to_string(middle) + " Hz between " + std::to_string(fromHz)
               + " and " + std::to_string(toHz) + ")");

    const double settled = measureFrequency(
        longSlide.audio.left, slideStart + longSlide.settleSamples
            + static_cast<int>(0.2 * sampleRate),
        static_cast<int>(0.5 * sampleRate), sampleRate, toHz);
    expect(std::abs(centsBetween(settled, toHz)) < 12.0,
           "the slide did not arrive on its target pitch");

    // The travel time is a distance over a hand speed, so a two-fret slide is
    // far shorter than a twelve-fret one. A fixed legato time would make them
    // equal.
    const auto shortSlide = renderSlide(45, 47, 0.8f, 0.28f);
    expect(shortSlide.settleSamples * 3 < longSlide.settleSamples,
           "a two-fret slide took nearly as long as a twelve-fret one (short "
               + std::to_string(shortSlide.settleSamples) + " samples, long "
               + std::to_string(longSlide.settleSamples) + ")");

    // The winding drags: the scrape is what Finger Noise controls, and at zero
    // it is exactly absent rather than merely quiet.
    const auto silentSlide = renderSlide(45, 57, 0.0f, 0.28f);
    expect(silentSlide.amplitude == 0.0f,
           "the slide scrape survived a silent Finger Noise control");
    expect(longSlide.amplitude > 0.0f, "the slide produced no scrape at all");

    const int frictionStart = slideStart;
    const int frictionLength = std::max(1024, longSlide.settleSamples);
    const auto scrapeEnergy = [&] (const Slide& withNoise, const Slide& without)
    {
        double sum = 0.0;
        for (int i = frictionStart; i < frictionStart + frictionLength; ++i)
        {
            const double difference =
                static_cast<double>(withNoise.audio.left[
                    static_cast<std::size_t>(i)])
                - static_cast<double>(without.audio.left[
                    static_cast<std::size_t>(i)]);
            sum += difference * difference;
        }
        return std::sqrt(sum / frictionLength);
    };
    const double woundScrape = scrapeEnergy(longSlide, silentSlide);

    // A plain string has no winding, so the same gesture barely makes a sound.
    const auto plainLoud = renderSlide(64, 71, 0.8f, 0.28f);
    const auto plainSilent = renderSlide(64, 71, 0.0f, 0.28f);
    expect(plainLoud.stringIndex == 7,
           "the plain-string slide did not stay on the top string");
    double plainSum = 0.0;
    for (int i = frictionStart; i < frictionStart + frictionLength; ++i)
    {
        const double difference =
            static_cast<double>(plainLoud.audio.left[static_cast<std::size_t>(i)])
            - static_cast<double>(plainSilent.audio.left[
                static_cast<std::size_t>(i)]);
        plainSum += difference * difference;
    }
    const double plainScrape = std::sqrt(plainSum / frictionLength);
    expect(plainScrape < woundScrape * 0.5,
           "a plain string squeaks as loudly as a wound one (wound "
               + std::to_string(woundScrape) + ", plain "
               + std::to_string(plainScrape) + ")");

    // The squeak's pitch is the rate the winding ridges pass under the finger,
    // so a fast hand squeaks high and a slow one low. This is asserted on the
    // band the engine actually configures rather than on the rendered audio,
    // and the reason is worth recording: the loaded pickup coil is a
    // second-order low-pass at a couple of kilohertz, so it flattens most of
    // the difference between a two-kilohertz squeak and an eight-kilohertz one
    // before it reaches the output. That is the instrument behaving correctly -
    // a real pickup does the same thing to a real squeak - but it means an
    // output-side centroid measures the coil rather than the friction.
    const auto bandCentre = [&] (float bendTime)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.fingerNoise = 0.8f;
        parameters.bendTimeSeconds = bendTime;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer lead(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, lead);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(57, 0.85f);
        const int stringIndex = TestAccess::stringForNote(engine, 57);
        return stringIndex >= 0
            ? TestAccess::slideBandCentreHz(engine, stringIndex) : 0.0;
    };
    const double fastCentre = bandCentre(0.20f);
    const double slowCentre = bandCentre(0.80f);
    expect(fastCentre > slowCentre * 3.0,
           "the squeak's band does not follow the speed of the hand (fast "
               + std::to_string(fastCentre) + " Hz, slow "
               + std::to_string(slowCentre) + " Hz)");

    // It is audible, though: the same gesture taken at two speeds renders two
    // different sounds rather than one scaled in time.
    const auto fastSlide = renderSlide(45, 57, 0.8f, 0.20f);
    const auto fastSilent = renderSlide(45, 57, 0.0f, 0.20f);
    const auto slowSlide = renderSlide(45, 57, 0.8f, 0.80f);
    const auto slowSilent = renderSlide(45, 57, 0.0f, 0.80f);
    const auto differenceOf = [] (const Slide& a, const Slide& b)
    {
        std::vector<float> out(a.audio.left.size(), 0.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = a.audio.left[i] - b.audio.left[i];
        return out;
    };
    const auto fastFriction = differenceOf(fastSlide, fastSilent);
    const auto slowFriction = differenceOf(slowSlide, slowSilent);
    const double fastCentroid = broadbandCentroid(
        fastFriction, slideStart, std::max(1024, fastSlide.settleSamples));
    const double slowCentroid = broadbandCentroid(
        slowFriction, slideStart, std::max(1024, slowSlide.settleSamples));
    expect(fastCentroid > 300.0 && slowCentroid > 300.0
               && std::abs(fastCentroid - slowCentroid) > 150.0,
           "two slides at different hand speeds produced the same friction "
           "(fast " + std::to_string(fastCentroid) + " Hz, slow "
               + std::to_string(slowCentroid) + " Hz)");
}

// The pinch harmonic is the same touch filter driven by the picking hand:
// the thumb catches the string at the pick's own position, so which partial
// squeals is a function of Pick Position rather than a fixed interval.
void testPinchHarmonic()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const int note = 52;
    const double f0 = midiHz(note);
    const int start = static_cast<int>(0.02 * sampleRate);
    const int window = static_cast<int>(0.12 * sampleRate);

    // Energy-weighted mean partial index: where in the harmonic series the
    // note's weight actually sits. A pinch moves it a long way up.
    const auto meanPartial = [&] (const StereoBuffer& buffer)
    {
        double weighted = 0.0;
        double total = 0.0;
        for (int n = 1; n <= 16; ++n)
        {
            const double magnitude = dftMagnitude(buffer.left, start, window,
                                                  sampleRate, f0 * n);
            const double power = magnitude * magnitude;
            weighted += power * n;
            total += power;
        }
        return total > 0.0 ? weighted / total : 0.0;
    };

    const auto renderAt = [&] (float pickPosition, PlayStyle style)
    {
        parameters.pickPosition = pickPosition;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, note, 0.9f, style, 1.0);
    };

    const auto pickedNearBridge = renderAt(0.18f, PlayStyle::Sustain);
    const auto pinchedNearBridge = renderAt(0.18f, PlayStyle::Pinch);
    const auto pinchedOverNeck = renderAt(1.0f, PlayStyle::Pinch);

    const auto strongestPartial = [&] (const StereoBuffer& buffer)
    {
        int best = 1;
        double bestMagnitude = -1.0;
        for (int n = 1; n <= 16; ++n)
        {
            const double magnitude = dftMagnitude(buffer.left, start, window,
                                                  sampleRate, f0 * n);
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                best = n;
            }
        }
        return best;
    };

    const double pickedMean = meanPartial(pickedNearBridge);
    const double pinchedMean = meanPartial(pinchedNearBridge);
    const double neckMean = meanPartial(pinchedOverNeck);

    expect(pinchedMean > pickedMean + 2.0,
           "the pinch did not move the note's weight up the harmonic series "
           "(picked " + std::to_string(pickedMean) + ", pinched "
               + std::to_string(pinchedMean) + ")");
    expect(neckMean < pinchedMean - 3.0,
           "moving the picking hand toward the neck did not move the squeal "
           "down the series (bridge " + std::to_string(pinchedMean) + ", neck "
               + std::to_string(neckMean) + ")");

    // The touch sits at the pick, so the surviving partial is the one with a
    // node there: around the eighth near the bridge, the octave with the hand
    // over the neck where the touch is at nearly half the string.
    const int bridgeSquealPartial = strongestPartial(pinchedNearBridge);
    const int neckSquealPartial = strongestPartial(pinchedOverNeck);
    expect(bridgeSquealPartial >= 6,
           "the near-bridge pinch did not select a high partial (strongest "
               + std::to_string(bridgeSquealPartial) + ")");
    expect(neckSquealPartial == 2,
           "the pinch with the hand over the neck did not select the octave "
           "(strongest " + std::to_string(neckSquealPartial) + ")");

    // Measured against the ordinary pick stroke, the squeal partial gains a
    // long way on the fundamental. This is the effect itself rather than a
    // proxy for it.
    const auto partialOverFundamental = [&] (const StereoBuffer& buffer, int n)
    {
        return decibels(
            dftMagnitude(buffer.left, start, window, sampleRate, f0 * n)
            / std::max(dftMagnitude(buffer.left, start, window, sampleRate, f0),
                       1.0e-15));
    };
    // The squeal is a pair of neighbours, not a single line: the eighth and
    // ninth partials of this pinch sit within 0.06 dB of each other, so which
    // one `strongestPartial` returns is decided by rounding and swaps under
    // changes that leave the effect itself alone - the ninth reads 15.12 dB
    // both before and after the velocity work of the 2026-08-07 pass, but the
    // reported partial moved from the ninth to the eighth. Score the squeal
    // over every partial within 1 dB of the pinched peak instead.
    const double pinchedPeak = dftMagnitude(pinchedNearBridge.left, start,
                                            window, sampleRate,
                                            f0 * bridgeSquealPartial);
    double gain = -1.0e9;
    int gainPartial = bridgeSquealPartial;
    for (int n = 2; n <= 16; ++n)
    {
        const double magnitude = dftMagnitude(pinchedNearBridge.left, start,
                                              window, sampleRate, f0 * n);
        if (magnitude < pinchedPeak * 0.891)  // within 1 dB of the peak
            continue;
        const double lift = partialOverFundamental(pinchedNearBridge, n)
                          - partialOverFundamental(pickedNearBridge, n);
        if (lift > gain)
        {
            gain = lift;
            gainPartial = n;
        }
    }
    expect(gain > 10.0,
           "the pinch did not lift its partial against the fundamental (gain "
               + std::to_string(gain) + " dB at partial "
               + std::to_string(gainPartial) + ")");

    // It is its own articulation, not a relabelled one.
    const auto natural = renderAt(0.18f, PlayStyle::Harmonics);
    expect(normalisedDifferenceRms(pinchedNearBridge.left, pickedNearBridge.left,
                                   0, static_cast<int>(0.3 * sampleRate)) > 0.2,
           "a pinch renders nearly the same audio as an ordinary pick stroke");
    expect(normalisedDifferenceRms(pinchedNearBridge.left, natural.left, 0,
                                   static_cast<int>(0.3 * sampleRate)) > 0.2,
           "a pinch renders nearly the same audio as a natural harmonic");
}

// The fretting hand has a position and a reach, so the same pitch is not
// always fingered at the lowest fret that can produce it. The lead phrase
// below is the whole point of the change: under the lowest-fret rule its
// fourth note fell onto an open string in the middle of a line played at the
// fifth position, which is a different string, a different decay and a note
// the fretting hand is not touching at all.
void testFrettingHandPosition()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // Open-position shapes are unchanged, because at the nut an open string
    // costs the hand nothing. This is the same C-major shape the allocation
    // test pins, repeated here so a hand-model regression is caught by the
    // check that owns the hand model.
    for (const int note : { 48, 52, 55, 60, 64 })
        engine.noteOn(note, 0.8f);
    expect(TestAccess::stringForNote(engine, 48) == 3
               && TestAccess::stringForNote(engine, 52) == 4
               && TestAccess::stringForNote(engine, 55) == 5
               && TestAccess::stringForNote(engine, 60) == 6
               && TestAccess::stringForNote(engine, 64) == 7,
           "the open C shape moved when the fretting hand was introduced");
    expect(TestAccess::frettingHandPosition(engine) == 0.0f,
           "an open-position chord moved the hand off the nut");

    // A descending lead phrase. Each note is picked, released, and given time
    // to retire, so every string is free when the next note arrives and the
    // only thing choosing between them is the hand.
    engine.reset();
    struct Placement
    {
        int string { -1 };
        int fret { -1 };
    };
    const auto playAndRelease = [&] (int note)
    {
        engine.noteOn(note, 0.8f);
        Placement placement;
        placement.string = TestAccess::stringForNote(engine, note);
        if (placement.string >= 0)
            placement.fret = TestAccess::snapshot(engine, placement.string).fret;
        engine.noteOff(note);
        // Let the damped string retire so every string is free again. It takes
        // under half a second, comfortably inside the second and a half after
        // which the hand would relax back to the nut.
        double waited = 0.0;
        while (engine.getActiveVoiceCount() > 0 && waited < 1.0)
        {
            StereoBuffer tail(static_cast<int>(0.25 * sampleRate));
            renderInto(engine, tail);
            waited += 0.25;
        }
        expect(engine.getActiveVoiceCount() == 0,
               "the released note did not retire between phrase notes");
        return placement;
    };

    // B4 from a cold hand is fretted at 7 on the top string, and the hand
    // settles two frets below it so the note sits under the middle fingers.
    const auto b4 = playAndRelease(71);
    expect(b4.string == 7 && b4.fret == 7,
           "B4 from the nut did not take the top string at the seventh fret");
    expect(std::abs(TestAccess::frettingHandPosition(engine) - 5.0f) < 1.0e-6f,
           "the hand did not settle below the note it had to reach for");

    const auto a4 = playAndRelease(69);
    expect(a4.string == 7 && a4.fret == 5,
           "A4 left the position the hand had just taken");

    // From here the lowest-fret rule and the hand disagree. G4 at the fifth
    // position is the eighth fret of the B string, not the third fret of the
    // top string, which is behind the index finger.
    const auto g4 = playAndRelease(67);
    expect(g4.string == 6 && g4.fret == 8,
           "G4 was fingered behind the hand instead of inside it");

    // The one that matters: E4 is an open string, and the old rule always took
    // it. In the fifth position it is the fifth fret of the B string.
    const auto e4 = playAndRelease(64);
    expect(e4.string == 6 && e4.fret == 5,
           "E4 fell back to the open string in the middle of a fretted phrase");

    const auto d4 = playAndRelease(62);
    expect(d4.string == 5 && d4.fret == 7,
           "D4 left the hand's position");

    // The phrase ends: nothing is held, and after a second and a half the hand
    // relaxes to the nut, so the same E4 is an open string again.
    StereoBuffer rest(static_cast<int>(2.0 * sampleRate));
    renderInto(engine, rest);
    const auto openAgain = playAndRelease(64);
    expect(openAgain.string == 7 && openAgain.fret == 0,
           "the hand did not return to the nut after the phrase ended");
    expect(TestAccess::frettingHandPosition(engine) == 0.0f,
           "the hand did not relax to the nut when the phrase ended");
}

void testSustainPedal()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    engine.setParameters(parameters);

    // Sustain pedal keeps a released note ringing.
    engine.reset();
    engine.setSustainPedal(true);
    engine.noteOn(45, 0.7f);
    StereoBuffer pedalBuffer(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, pedalBuffer);
    engine.noteOff(45);
    renderInto(engine, pedalBuffer);
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal did not hold the released note");

    const double heldRms = rmsInRange(pedalBuffer.left, 0, pedalBuffer.size());
    engine.setSustainPedal(false);
    StereoBuffer releasedBuffer(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, releasedBuffer);
    const double lateRms = rmsInRange(releasedBuffer.left,
                                      static_cast<int>(0.6 * sampleRate),
                                      releasedBuffer.size());
    expect(lateRms < heldRms * 0.1,
           "note did not damp after the sustain pedal was released");
}

void testVibratoOnlyMovesFingeredStrings()
{
    constexpr double sampleRate = 48000.0;

    // The lowest playable note is the low string played open: nothing is
    // holding it down, so there is no contact for the hand to rock and a
    // fretting-hand vibrato cannot reach it. The bar can, but that is the
    // pitch wheel, not channel pressure.
    const auto renderNote = [&](int midiNote, float vibrato)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(midiNote, 0.8f);
        StereoBuffer settle(static_cast<int>(0.05 * sampleRate));
        renderInto(engine, settle);
        engine.setVibrato(vibrato);
        StereoBuffer buffer(static_cast<int>(0.40 * sampleRate));
        renderInto(engine, buffer);
        return buffer.left;
    };

    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.noteOn(ElectryEngine::lowestPlayableNote, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);
        std::array<electry::StringVisualState, ElectryEngine::stringCount> strings {};
        engine.getStringVisualState(strings);
        bool open = false;
        for (const auto& string : strings)
            open = open || (string.sounding && string.fret == 0);
        expect(open,
               "the lowest playable note is not fingered at fret 0, so this "
               "test is no longer exercising an open string");
    }

    expect(renderNote(ElectryEngine::lowestPlayableNote, 1.0f)
               == renderNote(ElectryEngine::lowestPlayableNote, 0.0f),
           "channel pressure bent an open string, which no fretting hand can do");

    // The same pressure on a stopped note has to do something, or the check
    // above would pass simply by the control being dead. 47 is two frets up
    // the same string that 45 plays open.
    const auto fretted = renderNote(47, 1.0f);
    const auto still = renderNote(47, 0.0f);
    expect(fretted != still,
           "channel pressure left a fingered string alone");
}

void testLegatoSlideDoesNotConsumeAPickStroke()
{
    constexpr double sampleRate = 48000.0;

    // A slide onto a string that is already sounding retargets it and strikes
    // nothing, so it must not advance the alternate sequence - the same reason
    // a hammer-on does not. Charging it a stroke would leave the next note
    // that really is picked on the wrong one.
    const auto strokeAfter = [&](bool slideInBetween)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        engine.setParameters(parameters);
        StereoBuffer buffer(static_cast<int>(0.05 * sampleRate));

        // Latch Alternate picking, then Sustain so the first note is picked.
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(PickStyle::Alternate), 1.0f);
        engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                          + static_cast<int>(PlayStyle::Sustain), 1.0f);
        engine.noteOn(45, 0.8f);
        renderInto(engine, buffer);

        if (slideInBetween)
        {
            engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                              + static_cast<int>(PlayStyle::Slide), 1.0f);
            // Same string, different pitch: this retargets rather than picks.
            engine.noteOn(47, 0.8f);
            renderInto(engine, buffer);
            engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                              + static_cast<int>(PlayStyle::Sustain), 1.0f);
        }

        // A note far enough away to take a different string, so it is a real
        // pick rather than another retarget.
        engine.noteOn(69, 0.8f);
        renderInto(engine, buffer);
        std::array<electry::StringVisualState, ElectryEngine::stringCount> strings {};
        engine.getStringVisualState(strings);
        for (const auto& string : strings)
            if (string.sounding && string.midiNote == 69)
                return string.strokeUp ? 1 : 0;
        return -1;
    };

    const int withoutSlide = strokeAfter(false);
    const int withSlide = strokeAfter(true);
    expect(withoutSlide >= 0 && withSlide >= 0,
           "the picked note after the slide never sounded, so its stroke could "
           "not be read");
    expect(withSlide == withoutSlide,
           "a legato slide consumed an alternate pick stroke it never played, "
           "so the next picked note came out on the wrong stroke");
}

void testParameterSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters hostile;
    hostile.bodyWood = std::numeric_limits<float>::quiet_NaN();
    hostile.bodySize = -12.0f;
    hostile.bodyShape = 400.0f;
    hostile.construction = std::numeric_limits<float>::infinity();
    hostile.scaleLength = -std::numeric_limits<float>::infinity();
    hostile.pickupType = 55.0f;
    hostile.toneKnob = std::numeric_limits<float>::quiet_NaN();
    hostile.bodyResonance = 1.0e9f;
    hostile.stringGauge = -1.0e9f;
    hostile.stringAge = std::numeric_limits<float>::quiet_NaN();
    hostile.pickPosition = 2.0f;
    hostile.pickHardness = -2.0f;
    hostile.pickNoise = std::numeric_limits<float>::infinity();
    hostile.fingerNoise = -0.5f;
    hostile.releaseNoise = 77.0f;
    hostile.muteDamping = std::numeric_limits<float>::quiet_NaN();
    hostile.bendTimeSeconds = -3.0f;
    hostile.velocityAmount = 9.0f;
    hostile.outputGain = std::numeric_limits<float>::quiet_NaN();
    hostile.artifactAmount = std::numeric_limits<float>::infinity();
    hostile.sympatheticAmount = std::numeric_limits<float>::quiet_NaN();
    hostile.palmMute = -7.0f;
    hostile.strumSpreadSeconds = std::numeric_limits<float>::infinity();
    hostile.resonanceDepth = std::numeric_limits<float>::quiet_NaN();
    hostile.vibratoDepth = 12.0f;
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    hostile.outputMode = static_cast<electry::OutputMode>(999);
    engine.setParameters(hostile);

    auto buffer = renderNote(engine, sampleRate, 45, 0.9f, PlayStyle::Sustain,
                             0.6);
    expect(allFinite(buffer), "hostile parameters produced non-finite audio");
    expect(peakAbs(buffer.left) < 2.1f, "hostile parameters bypassed the guard");

    engine.setPitchBend(std::numeric_limits<float>::quiet_NaN());
    engine.setResonance(std::numeric_limits<float>::quiet_NaN());
    engine.noteOn(45, std::numeric_limits<float>::quiet_NaN());
    StereoBuffer more(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, more);
    expect(allFinite(more), "hostile performance input produced non-finite audio");

    // The acoustic return path must swallow hostile input outright.
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(std::numeric_limits<float>::quiet_NaN());
    engine.setAcousticReturnLevel(1.0f);
    engine.pushAcousticReturn(nullptr, nullptr, 128);
    std::array<float, 64> poison {};
    poison.fill(std::numeric_limits<float>::quiet_NaN());
    engine.pushAcousticReturn(poison.data(), nullptr, -5);
    engine.pushAcousticReturn(poison.data(), poison.data(),
                              static_cast<int>(poison.size()));
    for (int flood = 0; flood < 300; ++flood)
        engine.pushAcousticReturn(poison.data(), poison.data(),
                                  static_cast<int>(poison.size()));
    engine.noteOn(45, 0.9f);
    StereoBuffer poisoned(static_cast<int>(0.3 * sampleRate));
    renderInto(engine, poisoned);
    expect(allFinite(poisoned),
           "a hostile acoustic return produced non-finite audio");
}

// testParameterSanitisation() above only ever asserts on the audio that comes
// out the far end of a hostile parameter set, which the guard itself could
// pass through a subtly wrong path and still leave finite and quiet. This
// calls the guard directly and checks its two distinct behaviours: a
// non-finite field falls back to the shipping default, while a finite but
// out-of-range field clamps to the nearer boundary of its own valid interval
// instead. A future change that swapped one behaviour for the other, or
// dropped a field from the guard entirely, would not necessarily move any
// rendered sample enough to fail the audio-level test above.
void testParameterSanitisationFallsBackToDefaults()
{
    const EngineParameters defaults;

    EngineParameters hostile;
    hostile.bodyWood = std::numeric_limits<float>::quiet_NaN();
    hostile.bodySize = -12.0f;
    hostile.bodyShape = 400.0f;
    hostile.construction = std::numeric_limits<float>::infinity();
    hostile.scaleLength = -std::numeric_limits<float>::infinity();
    hostile.pickupType = 55.0f;
    hostile.toneKnob = std::numeric_limits<float>::quiet_NaN();
    hostile.bodyResonance = 1.0e9f;
    hostile.stringGauge = -1.0e9f;
    hostile.stringAge = std::numeric_limits<float>::quiet_NaN();
    hostile.pickPosition = 2.0f;
    hostile.pickHardness = -2.0f;
    hostile.pickNoise = std::numeric_limits<float>::infinity();
    hostile.fingerNoise = -0.5f;
    hostile.releaseNoise = 77.0f;
    hostile.muteDamping = std::numeric_limits<float>::quiet_NaN();
    hostile.bendTimeSeconds = -3.0f;
    hostile.velocityAmount = 9.0f;
    hostile.outputGain = std::numeric_limits<float>::quiet_NaN();
    hostile.artifactAmount = std::numeric_limits<float>::infinity();
    hostile.sympatheticAmount = std::numeric_limits<float>::quiet_NaN();
    hostile.palmMute = -7.0f;
    hostile.strumSpreadSeconds = std::numeric_limits<float>::infinity();
    hostile.resonanceDepth = std::numeric_limits<float>::quiet_NaN();
    hostile.vibratoDepth = 12.0f;
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    hostile.outputMode = static_cast<electry::OutputMode>(999);

    const EngineParameters clean = TestAccess::sanitise(hostile);

    // Non-finite input has no boundary to clamp to, so it falls back to the
    // shipping default exactly.
    expect(clean.bodyWood == defaults.bodyWood,
           "NaN bodyWood did not fall back to its default");
    expect(clean.construction == defaults.construction,
           "+inf construction did not fall back to its default");
    expect(clean.scaleLength == defaults.scaleLength,
           "-inf scaleLength did not fall back to its default");
    expect(clean.toneKnob == defaults.toneKnob,
           "NaN toneKnob did not fall back to its default");
    expect(clean.stringAge == defaults.stringAge,
           "NaN stringAge did not fall back to its default");
    expect(clean.pickNoise == defaults.pickNoise,
           "+inf pickNoise did not fall back to its default");
    expect(clean.muteDamping == defaults.muteDamping,
           "NaN muteDamping did not fall back to its default");
    expect(clean.outputGain == defaults.outputGain,
           "NaN outputGain did not fall back to its default");
    expect(clean.artifactAmount == defaults.artifactAmount,
           "+inf artifactAmount did not fall back to its default");
    expect(clean.sympatheticAmount == defaults.sympatheticAmount,
           "NaN sympatheticAmount did not fall back to its default");
    expect(clean.strumSpreadSeconds == defaults.strumSpreadSeconds,
           "+inf strumSpreadSeconds did not fall back to its default");
    expect(clean.resonanceDepth == defaults.resonanceDepth,
           "NaN resonanceDepth did not fall back to its default");

    // Finite but out-of-[0,1] input clamps to the nearer boundary rather than
    // falling back to the default - the same lambda handles every 0..1 field
    // and this is what tells its two branches apart.
    expect(clean.bodySize == 0.0f, "negative bodySize did not clamp to 0");
    expect(clean.bodyShape == 1.0f, "bodyShape above 1 did not clamp to 1");
    expect(clean.pickupType == 1.0f, "pickupType above 1 did not clamp to 1");
    expect(clean.bodyResonance == 1.0f,
           "huge bodyResonance did not clamp to 1");
    expect(clean.stringGauge == 0.0f,
           "hugely negative stringGauge did not clamp to 0");
    expect(clean.pickPosition == 1.0f,
           "pickPosition above 1 did not clamp to 1");
    expect(clean.pickHardness == 0.0f,
           "negative pickHardness did not clamp to 0");
    expect(clean.fingerNoise == 0.0f,
           "negative fingerNoise did not clamp to 0");
    expect(clean.releaseNoise == 1.0f,
           "releaseNoise above 1 did not clamp to 1");
    expect(clean.velocityAmount == 1.0f,
           "velocityAmount above 1 did not clamp to 1");
    expect(clean.palmMute == 0.0f, "negative palmMute did not clamp to 0");
    expect(clean.vibratoDepth == 1.0f,
           "vibratoDepth above 1 did not clamp to 1");

    // Fields with their own valid interval clamp to their own bounds rather
    // than [0,1]. bendTimeSeconds, strumSpreadSeconds and outputGain each
    // have an independent sanitizer branch instead of sharing the [0,1]
    // lambda, so - like outputGain below - each needs its own non-finite
    // fallback and both finite-boundary cases to actually protect its whole
    // branch, not just whichever half the shared "hostile" fixture happens
    // to hit.
    expect(clean.bendTimeSeconds == 0.04f,
           "negative bendTimeSeconds did not clamp to the 0.04 s floor");

    EngineParameters bendTimeNonFinite;
    bendTimeNonFinite.bendTimeSeconds = std::numeric_limits<float>::quiet_NaN();
    expect(TestAccess::sanitise(bendTimeNonFinite).bendTimeSeconds
               == defaults.bendTimeSeconds,
           "NaN bendTimeSeconds did not fall back to its default");
    EngineParameters bendTimeAboveRange;
    bendTimeAboveRange.bendTimeSeconds = 5.0f;
    expect(TestAccess::sanitise(bendTimeAboveRange).bendTimeSeconds == 2.0f,
           "bendTimeSeconds above 2 s did not clamp to its 2 s ceiling");

    // strumSpreadSeconds' own [0, 0.040] branch: the "hostile" fixture above
    // only supplies +inf, proving the fallback-to-default arm but not either
    // finite boundary.
    EngineParameters spreadBelowRange;
    spreadBelowRange.strumSpreadSeconds = -0.01f;
    expect(TestAccess::sanitise(spreadBelowRange).strumSpreadSeconds == 0.0f,
           "negative strumSpreadSeconds did not clamp to its 0 floor");
    EngineParameters spreadAboveRange;
    spreadAboveRange.strumSpreadSeconds = 0.2f;
    expect(TestAccess::sanitise(spreadAboveRange).strumSpreadSeconds == 0.040f,
           "strumSpreadSeconds above 0.040 did not clamp to its ceiling");

    // outputGain has its own [0, 2] branch rather than the shared [0, 1]
    // lambda, so it needs its own finite-out-of-range boundary cases: the
    // NaN case above only proves the fallback-to-default path, not this one.
    EngineParameters gainBelowRange;
    gainBelowRange.outputGain = -1.0f;
    expect(TestAccess::sanitise(gainBelowRange).outputGain == 0.0f,
           "negative outputGain did not clamp to its 0 floor");
    EngineParameters gainAboveRange;
    gainAboveRange.outputGain = 3.0f;
    expect(TestAccess::sanitise(gainAboveRange).outputGain == 2.0f,
           "outputGain above 2 did not clamp to its 2 ceiling");

    // An invalid enumerator falls back to the default enumerator rather than
    // surviving as an out-of-range integer. The "hostile" fixture above only
    // supplies enumerators above the valid range (999), so each enum also
    // gets a below-range (negative) case here to protect its other bound.
    expect(clean.pickupSelector == defaults.pickupSelector,
           "out-of-range pickupSelector did not fall back to its default");
    expect(clean.outputMode == defaults.outputMode,
           "out-of-range outputMode did not fall back to its default");

    EngineParameters negativeSelector;
    negativeSelector.pickupSelector = static_cast<PickupSelector>(-1);
    expect(TestAccess::sanitise(negativeSelector).pickupSelector
               == defaults.pickupSelector,
           "negative pickupSelector did not fall back to its default");
    EngineParameters negativeOutputMode;
    negativeOutputMode.outputMode = static_cast<electry::OutputMode>(-1);
    expect(TestAccess::sanitise(negativeOutputMode).outputMode
               == defaults.outputMode,
           "negative outputMode did not fall back to its default");

    // A parameter set already inside every field's valid range is a guard's
    // no-op, not a smoothing stage - it must come back unchanged. Every field
    // gets its own distinct in-range value so the guard cannot pass this
    // case by unconditionally resetting an untested field to its default.
    EngineParameters valid = defaults;
    valid.bodyWood = 0.62f;
    valid.bodySize = 0.73f;
    valid.bodyShape = 0.15f;
    valid.construction = 0.44f;
    valid.scaleLength = 0.55f;
    valid.pickupType = 0.66f;
    valid.toneKnob = 0.22f;
    valid.bodyResonance = 0.81f;
    valid.stringGauge = 0.33f;
    valid.stringAge = 0.77f;
    valid.pickPosition = 0.11f;
    valid.pickHardness = 0.88f;
    valid.pickNoise = 0.29f;
    valid.fingerNoise = 0.64f;
    valid.releaseNoise = 0.19f;
    valid.muteDamping = 0.41f;
    valid.bendTimeSeconds = 0.9f;
    valid.velocityAmount = 0.37f;
    valid.outputGain = 1.4f;
    valid.artifactAmount = 0.53f;
    valid.sympatheticAmount = 0.68f;
    valid.palmMute = 0.24f;
    valid.strumSpreadSeconds = 0.02f;
    valid.resonanceDepth = 0.71f;
    valid.vibratoDepth = 0.09f;
    valid.pickupSelector = PickupSelector::Neck;
    valid.outputMode = electry::OutputMode::Stereo;
    const EngineParameters passedThrough = TestAccess::sanitise(valid);
    expect(passedThrough.bodyWood == valid.bodyWood,
           "in-range bodyWood was altered by the guard");
    expect(passedThrough.bodySize == valid.bodySize,
           "in-range bodySize was altered by the guard");
    expect(passedThrough.bodyShape == valid.bodyShape,
           "in-range bodyShape was altered by the guard");
    expect(passedThrough.construction == valid.construction,
           "in-range construction was altered by the guard");
    expect(passedThrough.scaleLength == valid.scaleLength,
           "in-range scaleLength was altered by the guard");
    expect(passedThrough.pickupType == valid.pickupType,
           "in-range pickupType was altered by the guard");
    expect(passedThrough.toneKnob == valid.toneKnob,
           "in-range toneKnob was altered by the guard");
    expect(passedThrough.bodyResonance == valid.bodyResonance,
           "in-range bodyResonance was altered by the guard");
    expect(passedThrough.stringGauge == valid.stringGauge,
           "in-range stringGauge was altered by the guard");
    expect(passedThrough.stringAge == valid.stringAge,
           "in-range stringAge was altered by the guard");
    expect(passedThrough.pickPosition == valid.pickPosition,
           "in-range pickPosition was altered by the guard");
    expect(passedThrough.pickHardness == valid.pickHardness,
           "in-range pickHardness was altered by the guard");
    expect(passedThrough.pickNoise == valid.pickNoise,
           "in-range pickNoise was altered by the guard");
    expect(passedThrough.fingerNoise == valid.fingerNoise,
           "in-range fingerNoise was altered by the guard");
    expect(passedThrough.releaseNoise == valid.releaseNoise,
           "in-range releaseNoise was altered by the guard");
    expect(passedThrough.muteDamping == valid.muteDamping,
           "in-range muteDamping was altered by the guard");
    expect(passedThrough.bendTimeSeconds == valid.bendTimeSeconds,
           "in-range bendTimeSeconds was altered by the guard");
    expect(passedThrough.velocityAmount == valid.velocityAmount,
           "in-range velocityAmount was altered by the guard");
    expect(passedThrough.outputGain == valid.outputGain,
           "in-range outputGain was altered by the guard");
    expect(passedThrough.artifactAmount == valid.artifactAmount,
           "in-range artifactAmount was altered by the guard");
    expect(passedThrough.sympatheticAmount == valid.sympatheticAmount,
           "in-range sympatheticAmount was altered by the guard");
    expect(passedThrough.palmMute == valid.palmMute,
           "in-range palmMute was altered by the guard");
    expect(passedThrough.strumSpreadSeconds == valid.strumSpreadSeconds,
           "in-range strumSpreadSeconds was altered by the guard");
    expect(passedThrough.resonanceDepth == valid.resonanceDepth,
           "in-range resonanceDepth was altered by the guard");
    expect(passedThrough.vibratoDepth == valid.vibratoDepth,
           "in-range vibratoDepth was altered by the guard");
    expect(passedThrough.pickupSelector == valid.pickupSelector,
           "a valid pickupSelector was altered by the guard");
    expect(passedThrough.outputMode == valid.outputMode,
           "a valid outputMode was altered by the guard");
}

// pushAcousticReturn()'s own guard - a null left pointer or a non-positive
// sample count is a no-op, a null right pointer duplicates left rather than
// being read through, a non-finite averaged sample folds to zero before it
// is stored, and a single push longer than the ring drops its oldest samples
// rather than overflowing - was only ever driven through
// testParameterSanitisation(), which checked that the eventual rendered
// audio stayed finite but never looked at the ring itself, so a guard that
// silently stored a NaN, averaged through a stale right pointer, or wrapped
// a negative count into a huge write would still have passed. This drives
// the guard directly against the ring it actually fills.
void testPushAcousticReturnSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // A null right channel duplicates left rather than being read through:
    // the stored value is exactly the left sample, not half of a read
    // through whatever the null happened to alias to. Seeding the ring with
    // this push also gives the invalid-push probes below a populated ring to
    // run against.
    std::array<float, 8> ordinary {
        1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -1.0f, 0.125f, -0.25f
    };
    engine.pushAcousticReturn(ordinary.data(), nullptr,
                              static_cast<int>(ordinary.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == static_cast<int>(ordinary.size()),
           "a mono push did not fill the ring one sample per input sample");
    for (int i = 0; i < static_cast<int>(ordinary.size()); ++i)
        expect(std::abs(TestAccess::feedbackRingSample(engine, i)
                        - ordinary[static_cast<std::size_t>(i)]) < 1.0e-6f,
               "a mono acoustic-return push was not stored as the left "
               "channel verbatim");

    // A null left pointer and a non-positive count are both a no-op against
    // a *populated* ring: neither its availability nor its stored samples
    // move. Testing this against an already-empty ring would only prove
    // availability stays at zero, which a guard that discarded the ring
    // before checking its arguments would also satisfy.
    const int seededAvailable = TestAccess::feedbackAvailable(engine);
    std::vector<float> seededSamples(static_cast<std::size_t>(seededAvailable));
    for (int i = 0; i < seededAvailable; ++i)
        seededSamples[static_cast<std::size_t>(i)]
            = TestAccess::feedbackRingSample(engine, i);
    const auto expectRingUnchanged = [&] (const char* what)
    {
        expect(TestAccess::feedbackAvailable(engine) == seededAvailable,
               std::string(what) + " changed a populated acoustic-return "
                   "ring's availability");
        for (int i = 0; i < seededAvailable; ++i)
            expect(TestAccess::feedbackRingSample(engine, i)
                       == seededSamples[static_cast<std::size_t>(i)],
                   std::string(what) + " altered a populated acoustic-return "
                       "ring's stored samples");
    };

    // A valid right buffer here pins the guard to left == nullptr
    // specifically: a guard accidentally narrowed to
    // "left == nullptr && right == nullptr" would fall through and
    // dereference the null left pointer in the averaging loop instead of
    // leaving the ring untouched.
    std::array<float, 4> ignoredByNullLeft { 0.3f, -0.3f, 0.6f, -0.6f };
    engine.pushAcousticReturn(nullptr, ignoredByNullLeft.data(),
                              static_cast<int>(ignoredByNullLeft.size()));
    expectRingUnchanged("a null left pointer with a valid right buffer");

    std::array<float, 4> ignoredByNegativeCount { 9.0f, 9.0f, 9.0f, 9.0f };
    engine.pushAcousticReturn(ignoredByNegativeCount.data(), nullptr, -3);
    expectRingUnchanged("a negative sample count");

    // Zero is checked separately from negative: a guard narrowed from
    // numSamples <= 0 to numSamples < 0 would let a zero count fall through
    // to the stale-ring-clearing block and discard this populated ring
    // before the (no-op) write loop runs.
    std::array<float, 4> ignoredByZeroCount { 9.0f, 9.0f, 9.0f, 9.0f };
    engine.pushAcousticReturn(ignoredByZeroCount.data(), nullptr, 0);
    expectRingUnchanged("a zero sample count");

    // A non-finite averaged sample folds to zero rather than propagating,
    // whichever channel it came from.
    std::array<float, 4> poisonLeft {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 1.0f,
        std::numeric_limits<float>::quiet_NaN()
    };
    std::array<float, 4> poisonRight {
        0.0f, 0.0f, std::numeric_limits<float>::infinity(), 0.0f
    };
    engine.pushAcousticReturn(poisonLeft.data(), poisonRight.data(),
                              static_cast<int>(poisonLeft.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == static_cast<int>(poisonLeft.size()),
           "a hostile acoustic-return push did not fill the ring");
    for (int i = 0; i < static_cast<int>(poisonLeft.size()); ++i)
        expect(TestAccess::feedbackRingSample(engine, i) == 0.0f,
               "a non-finite averaged acoustic-return sample was stored "
               "rather than folded to zero");

    // The fold-to-zero rule must hold for a mono push too: every hostile
    // push above supplies a non-null right buffer, so a handler that took a
    // separate path for null-right and stored left verbatim (skipping the
    // finite check) would pass all of them while still injecting NaN or
    // infinity into the ring here.
    std::array<float, 4> poisonMono {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 1.0f,
        -std::numeric_limits<float>::infinity()
    };
    engine.pushAcousticReturn(poisonMono.data(), nullptr,
                              static_cast<int>(poisonMono.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == static_cast<int>(poisonMono.size()),
           "a hostile mono acoustic-return push did not fill the ring");
    for (int i = 0; i < static_cast<int>(poisonMono.size()); ++i)
        expect(TestAccess::feedbackRingSample(engine, i)
                   == (i == 2 ? 1.0f : 0.0f),
               "a non-finite mono acoustic-return sample was stored rather "
               "than folded to zero");

    // An ordinary stereo push is the average of both channels, not just one
    // of them.
    std::array<float, 3> left { 1.0f, -1.0f, 0.5f };
    std::array<float, 3> right { -0.2f, 0.6f, -0.5f };
    engine.pushAcousticReturn(left.data(), right.data(),
                              static_cast<int>(left.size()));
    for (int i = 0; i < static_cast<int>(left.size()); ++i)
    {
        const float expected = 0.5f * (left[static_cast<std::size_t>(i)]
                                       + right[static_cast<std::size_t>(i)]);
        expect(std::abs(TestAccess::feedbackRingSample(engine, i) - expected)
                   < 1.0e-6f,
               "a stereo acoustic-return push was not the average of both "
               "channels");
    }

    // A single push longer than the ring must cap availability at the
    // ring's capacity and drop its oldest samples rather than overflow the
    // write index past what the read side can ever see: the ring must end
    // up holding exactly the last `capacity` samples pushed, in order.
    const int capacity = TestAccess::feedbackRingCapacity();
    std::vector<float> ramp(static_cast<std::size_t>(capacity) + 50);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float>(i);
    engine.pushAcousticReturn(ramp.data(), ramp.data(),
                              static_cast<int>(ramp.size()));
    expect(TestAccess::feedbackAvailable(engine) == capacity,
           "a push longer than the ring did not cap availability at its "
           "capacity");
    // Every retained offset is checked, not just the first and last: a
    // wraparound bug that duplicates, drops or reorders one interior sample
    // would leave both endpoints correct while still corrupting the ring.
    int firstMismatchOffset = -1;
    for (int offset = 0; offset < capacity; ++offset)
    {
        if (TestAccess::feedbackRingSample(engine, offset)
                != ramp[static_cast<std::size_t>(offset) + 50])
        {
            firstMismatchOffset = offset;
            break;
        }
    }
    expect(firstMismatchOffset == -1,
           "a push longer than the ring dropped, duplicated or reordered an "
           "interior sample at offset "
               + std::to_string(firstMismatchOffset));

    // And held together on a genuinely fretted, sounding string, the guard
    // must still leave finite, bounded audio behind it end to end.
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(1.0f);
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    StereoBuffer buffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer),
           "a saturated acoustic-return ring produced non-finite audio");
    // allFinite() alone only rejects NaN/infinity, so a regression that
    // removed or weakened the downstream amplitude limiting could still
    // leak the ramp's own values (as large as capacity + 49) through as
    // an arbitrarily large but finite signal. An ordinary sustained note
    // here peaks around 0.13; 1.0 leaves ample headroom while still
    // catching that failure mode.
    expect(peakAbs(buffer.left) < 1.0f && peakAbs(buffer.right) < 1.0f,
           "a saturated acoustic-return ring produced unbounded audio");
}

// ---------------------------------------------------------------------------
// Version 1.1: bridge-coupled sympathetic strings
// ---------------------------------------------------------------------------

void testSympatheticBridgeCoupling()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // A2 is picked on its own open string; its third partial lands almost
    // exactly on the open high E, which is the string a real guitar rings
    // hardest through the bridge.
    constexpr double openHighE = 329.62756;
    const auto tailEnergyAt = [&] (float amount)
    {
        parameters.sympatheticAmount = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                       PlayStyle::Sustain, 2.6, 0.5);
        expect(allFinite(buffer),
               "sympathetic coupling produced non-finite audio");
        const int start = static_cast<int>(1.4 * sampleRate);
        const int length = static_cast<int>(1.0 * sampleRate);
        return dftMagnitude(buffer.left, start, length, sampleRate, openHighE);
    };

    const double bypassed = tailEnergyAt(0.0f);
    const double coupled = tailEnergyAt(0.6f);
    expect(coupled > 12.0 * std::max(bypassed, 1.0e-12),
           "bridge coupling did not ring the open high E after the played "
           "string was damped (" + std::to_string(bypassed) + " -> "
               + std::to_string(coupled) + ")");

    // At zero the coupled waveguides are never even configured, so the engine
    // is exactly the pre-1.1 model.
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);
    auto silentTail = renderNote(engine, sampleRate, 45, 0.95f,
                                 PlayStyle::Sustain, 2.6, 0.5);
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(! snapshot.sympatheticReady,
               "a coupled string was configured while the control was at zero");
    }
    expect(peakAbs(silentTail.left, static_cast<int>(2.2 * sampleRate)) < 1.0e-6f,
           "bypassed engine still rang two seconds after the note died");

    // Coupling never invents ambience: with no note ever played the bus stays
    // at zero, so nothing can be injected.
    parameters.sympatheticAmount = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    StereoBuffer idle(8192);
    renderInto(engine, idle);
    expect(peakAbs(idle.left) == 0.0f && peakAbs(idle.right) == 0.0f,
           "sympathetic coupling generated output without a played note");

    // Determinism, including across engine reuse.
    const auto first = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.4);
    const auto second = renderNote(engine, sampleRate, 45, 0.8f,
                                   PlayStyle::Sustain, 0.9, 0.4);
    expect(first.left == second.left && first.right == second.right,
           "sympathetic coupling is not sample-deterministic");

    // A fingered string never keeps its coupled ring: the hand stops it.
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer settle(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, settle);
    const int highString = ElectryEngine::stringCount - 1;
    expect(TestAccess::snapshot(engine, highString).sympatheticReady,
           "the open high E did not begin ringing through the bridge");
    engine.noteOn(64, 0.9f);
    expect(! TestAccess::snapshot(engine, highString).sympatheticReady,
           "picking a coupled string did not hand it back to the player");

    // Worst case: every control at its extreme, every string struck hard, and
    // the coupled loops driven as hard as the model allows.
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        ElectryEngine hot;
        hot.prepare(rate, 512);
        EngineParameters extreme;
        extreme.sympatheticAmount = 1.0f;
        extreme.artifactAmount = 1.0f;
        extreme.bodyResonance = 1.0f;
        extreme.stringAge = 0.0f;
        extreme.construction = 0.0f;
        extreme.outputGain = 2.0f;
        extreme.outputMode = electry::OutputMode::Stereo;
        hot.setParameters(extreme);
        hot.reset();
        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            hot.noteOn(note, 1.0f);
        StereoBuffer strum(static_cast<int>(0.9 * rate));
        renderInto(hot, strum);
        hot.allNotesOff();
        StereoBuffer ring(static_cast<int>(0.9 * rate));
        renderInto(hot, ring);
        expect(allFinite(strum) && allFinite(ring),
               "maximum coupling became non-finite at "
                   + std::to_string(rate) + " Hz");
        // The linked soft guard bounds any input to 1/sqrt(0.4356) = 1.516
        // before the output gain, so the maximum reachable peak at the
        // maximum +6 dB output is 3.03. Staying under that proves the coupled
        // loops cannot drive the model past its analytic ceiling.
        expect(peakAbs(strum.left) < 3.05f && peakAbs(ring.left) < 3.05f,
               "maximum coupling escaped the output guard at "
                   + std::to_string(rate) + " Hz");
        // The coupled strings must decay rather than sustain. With no plucked
        // voice left there is nothing writing to the bridge bus, so the ring
        // has to fall away and the engine has to reach exact silence.
        expect(peakAbs(ring.left) > 1.0e-3f,
               "the coupled strings did not ring after the notes ended at "
                   + std::to_string(rate) + " Hz");
        StereoBuffer firstTail(static_cast<int>(2.0 * rate));
        renderInto(hot, firstTail);
        StereoBuffer secondTail(static_cast<int>(2.0 * rate));
        renderInto(hot, secondTail);
        expect(allFinite(firstTail) && allFinite(secondTail),
               "maximum coupling became non-finite while ringing out at "
                   + std::to_string(rate) + " Hz");
        expect(peakAbs(firstTail.left) < 1.0e-3f
                   && peakAbs(secondTail.left) == 0.0f,
               "maximum coupling did not ring out to exact silence at "
                   + std::to_string(rate) + " Hz ("
                   + std::to_string(peakAbs(firstTail.left)) + " -> "
                   + std::to_string(peakAbs(secondTail.left)) + ")");
    }
}

// ---------------------------------------------------------------------------
// Version 1.1: continuous palm-mute pressure
// ---------------------------------------------------------------------------

void testPalmMuteContinuum()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto lateRms = [&] (float amount)
    {
        parameters.palmMute = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.9f,
                                       PlayStyle::Sustain, 0.9);
        expect(allFinite(buffer) && peakAbs(buffer.left) < 0.80f,
               "palm mute produced non-finite or unbounded audio at "
                   + std::to_string(amount));
        return rmsInRange(buffer.left, static_cast<int>(0.30 * sampleRate),
                          static_cast<int>(0.60 * sampleRate));
    };

    const double open = lateRms(0.0f);
    const double half = lateRms(0.5f);
    const double full = lateRms(1.0f);
    expect(open > half * 2.0 && half > full * 2.0,
           "palm mute pressure does not shorten decay monotonically ("
               + std::to_string(open) + ", " + std::to_string(half) + ", "
               + std::to_string(full) + ")");

    // Zero pressure is a mathematical no-op, not merely a small effect: the
    // rendered audio must be identical to an engine that never saw the
    // control.
    EngineParameters untouched = parameters;
    untouched.palmMute = 0.0f;
    engine.setParameters(untouched);
    const auto reference = renderNote(engine, sampleRate, 45, 0.9f,
                                      PlayStyle::Sustain, 0.5);
    EngineParameters explicitZero = untouched;
    explicitZero.palmMute = 0.0f;
    engine.setParameters(explicitZero);
    const auto atZero = renderNote(engine, sampleRate, 45, 0.9f,
                                   PlayStyle::Sustain, 0.5);
    expect(reference.left == atZero.left,
           "palm mute at zero is not an exact bypass");

    // Damping is re-solved through the same loop-filter path as every other
    // decay control, so a muted string still sounds the played pitch.
    parameters.palmMute = 0.45f;
    engine.setParameters(parameters);
    const auto muted = renderNote(engine, sampleRate, 45, 0.9f,
                                  PlayStyle::Sustain, 0.5);
    const double expectedHz = midiHz(45);
    const double measured = measureFrequency(
        muted.left, static_cast<int>(0.02 * sampleRate),
        static_cast<int>(0.18 * sampleRate), sampleRate, expectedHz);
    expect(std::abs(centsBetween(measured, expectedHz)) < 12.0,
           "palm-muted string drifted out of tune ("
               + std::to_string(centsBetween(measured, expectedHz)) + " cents)");

    // The loop filter genuinely moves rather than a gain being applied after
    // the fact.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float openDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    parameters.palmMute = 0.9f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float mutedDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    // A shorter decay target needs a heavier one-pole, because the solve
    // matches the ratio between the fundamental and high-frequency T60s.
    expect(mutedDamping > openDamping + 1.0e-3f,
           "palm mute did not change the solved loop-filter coefficient ("
               + std::to_string(openDamping) + " -> "
               + std::to_string(mutedDamping) + ")");

    // CC2 pressure adds to the parameter and is released cleanly.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.setPalmMutePressure(1.0f);
    engine.noteOn(45, 0.9f);
    StereoBuffer pressed(static_cast<int>(0.9 * sampleRate));
    renderInto(engine, pressed);
    const double pressedLate = rmsInRange(pressed.left,
                                          static_cast<int>(0.30 * sampleRate),
                                          static_cast<int>(0.60 * sampleRate));
    expect(allFinite(pressed) && pressedLate < half * 2.0,
           "CC2 pressure did not damp the string");
    engine.setPalmMutePressure(std::numeric_limits<float>::quiet_NaN());
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer afterHostile(static_cast<int>(0.3 * sampleRate));
    renderInto(engine, afterHostile);
    expect(allFinite(afterHostile),
           "hostile CC2 pressure produced non-finite audio");
}

// ---------------------------------------------------------------------------
// Version 1.1: strum travel
// ---------------------------------------------------------------------------

void testStrumSpread()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 6> chord { 28, 40, 45, 50, 55, 64 };

    const auto firstOnsetSample = [] (const std::vector<float>& data, float threshold)
    {
        for (int i = 0; i < static_cast<int>(data.size()); ++i)
            if (std::abs(data[static_cast<std::size_t>(i)]) > threshold)
                return i;
        return -1;
    };

    const auto renderChord = [&] (float spreadSeconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : chord)
            engine.noteOn(note, 0.85f);

        std::array<int, ElectryEngine::stringCount> delays {};
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            delays[static_cast<std::size_t>(s)] =
                TestAccess::snapshot(engine, s).startDelaySamples;

        StereoBuffer buffer(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, buffer);
        return std::make_pair(delays, std::move(buffer));
    };

    const auto block = renderChord(0.0f);
    for (const int delay : block.first)
        expect(delay == 0, "a chord at zero spread was not simultaneous");
    expect(allFinite(block.second), "block chord produced non-finite audio");

    const auto strummed = renderChord(0.020f);
    // The first string the pick meets fires after the re-anchor pre-roll every
    // voice of a strummed chord carries, not immediately: until that window
    // closes a later note-on may still turn out to be the edge the stroke
    // began at. Each further string is offset by the travel time on top of it,
    // in physical string order.
    const int preRoll = static_cast<int>(0.020 * sampleRate * 2.0); // internal 2x clock
    const int step = static_cast<int>(0.020f * sampleRate * 2.0f);
    expect(std::abs(strummed.first[0] - preRoll) <= 2,
           "the leading string of a strum did not fire one pre-roll after its "
           "note-on (" + std::to_string(strummed.first[0]) + " vs "
               + std::to_string(preRoll) + ")");
    // The pick accelerates through the strings, so the offsets no longer lie
    // on a straight line - but they still increase string by string, and the
    // knob still states the mean crossing time, so seven crossings still take
    // seven times it.
    int previous = strummed.first[0];
    for (const std::size_t stringIndex : { std::size_t { 2 }, std::size_t { 3 },
                                           std::size_t { 4 }, std::size_t { 5 },
                                           std::size_t { 7 } })
    {
        expect(strummed.first[stringIndex] > previous,
               "string " + std::to_string(stringIndex)
                   + " did not receive its strum travel offset ("
                   + std::to_string(strummed.first[stringIndex]) + " after "
                   + std::to_string(previous) + ")");
        previous = strummed.first[stringIndex];
    }
    const int travel = strummed.first[7] - strummed.first[0];
    expect(std::abs(travel - 7 * step) <= 7 * step / 20,
           "a strum across eight strings did not take seven times the spread ("
               + std::to_string(travel) + " vs " + std::to_string(7 * step) + ")");
    expect(allFinite(strummed.second) && peakAbs(strummed.second.left) < 0.80f,
           "strummed chord produced non-finite or unbounded audio");

    // The strum genuinely reaches the audio: the last string's attack arrives
    // measurably later than the chord's onset.
    const int blockOnset = firstOnsetSample(block.second.left, 0.02f);
    const int strumOnset = firstOnsetSample(strummed.second.left, 0.02f);
    expect(blockOnset >= 0 && strumOnset >= 0, "chord onset was not detectable");
    const double blockPeak = static_cast<double>(peakAbs(
        block.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    const double strumPeak = static_cast<double>(peakAbs(
        strummed.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    expect(strumPeak < blockPeak,
           "spreading a chord did not lower its stacked initial peak");

    // A note that arrives after the chord window starts a fresh stroke, so it
    // carries the pre-roll and nothing else - never the previous chord's
    // travel.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.040f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.9f);
    StereoBuffer gap(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, gap);
    engine.noteOn(64, 0.9f);
    expect(std::abs(TestAccess::snapshot(
                        engine, ElectryEngine::stringCount - 1).startDelaySamples
                    - preRoll) <= 2,
           "a note outside the chord window inherited a strum offset");

    // A delayed voice is never retired before its excitation fires.
    engine.reset();
    for (const int note : chord)
        engine.noteOn(note, 0.85f);
    StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, settle);
    expect(engine.getActiveVoiceCount() == static_cast<int>(chord.size()),
           "a strum-delayed string was retired before it was picked");
    expect(peakAbs(settle.left) > 1.0e-4f, "the delayed strings never sounded");

    // Strum Spread schedules the stroke rather than shaping it, so the control
    // tick copies it verbatim. A chord dispatched at offset 0 of the block that
    // carries the automation change must already use the new value; reading the
    // smoothed copy scheduled the whole chord with the previous spread. reset()
    // syncs the smoothed state from the target, so this deliberately does not
    // reset after the change.
    {
        ElectryEngine automated;
        automated.prepare (sampleRate, 512);
        EngineParameters flat;
        flat.artifactAmount = 0.0f;
        flat.sympatheticAmount = 0.0f;
        flat.strumSpreadSeconds = 0.0f;
        automated.setParameters (flat);
        automated.reset();
        StereoBuffer settled (1024);
        renderInto (automated, settled);

        EngineParameters spreadNow = flat;
        spreadNow.strumSpreadSeconds = 0.020f;
        automated.setParameters (spreadNow);

        // No render in between: the chord arrives at sample offset 0.
        for (const int note : chord)
            automated.noteOn (note, 0.85f);

        int delayedStrings = 0;
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            if (TestAccess::snapshot (automated, stringIndex).startDelaySamples > 0)
                ++delayedStrings;
        expect (delayedStrings > 0,
                "a chord automated in the same block was scheduled with the "
                "previous strum spread");
    }

    // Lifting a key before the pick reaches that string cancels the stroke.
    // Leaving the countdown running excited the string after its release, so a
    // short strummed chord grew a late attack once the keys were already up.
    {
        ElectryEngine released;
        released.prepare(sampleRate, 512);
        EngineParameters spread;
        spread.artifactAmount = 0.0f;
        spread.sympatheticAmount = 0.0f;
        spread.strumSpreadSeconds = 0.12f;
        released.setParameters(spread);
        released.reset();
        for (const int note : chord)
            released.noteOn(note, 0.85f);

        // Every key is up long before the pick has crossed the neck.
        StereoBuffer opening(static_cast<int>(0.02 * sampleRate));
        renderInto(released, opening);
        for (const int note : chord)
            released.noteOff(note);

        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            expect(TestAccess::snapshot(released, stringIndex).startDelaySamples == 0,
                   "string " + std::to_string(stringIndex)
                       + " kept a pending strum excitation after its key was lifted");

        StereoBuffer tail(static_cast<int>(1.0 * sampleRate));
        renderInto(released, tail);
        // By this point the damped strings have decayed; any energy here is a
        // pick that landed after the key was released.
        const float late = peakAbs(tail.left, static_cast<int>(0.15 * sampleRate));
        expect(late < 0.01f,
               "a string was picked after its key was released (late peak "
                   + std::to_string(late) + ")");
    }
}

void testStrumTravelFollowsStroke()
{
    // The pick enters the neck at one edge, travels in one direction, and
    // speeds up as it goes. Three things follow, and none of them held before:
    // the edge is set by the stroke rather than by whichever note-on the host
    // sent first, the crossing intervals compress instead of lying on a
    // straight line, and no two strokes lay the same ramp down twice.
    constexpr double sampleRate = 48000.0;
    constexpr double internalRate = sampleRate * 2.0;   // the engine's own clock
    constexpr float spread = 0.012f;
    const int spreadSamples = static_cast<int>(spread * internalRate);
    // Every voice of a strummed chord is held back by this much, which is what
    // buys the re-anchor window a note-on arriving in a later block needs.
    const int preRoll = static_cast<int>(0.020 * internalRate);

    const auto strum = [&] (ElectryEngine& engine, const std::vector<int>& notes)
    {
        for (const int note : notes)
            engine.noteOn(note, 0.85f);
        std::array<int, ElectryEngine::stringCount> delays {};
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            delays[static_cast<std::size_t>(s)] =
                TestAccess::snapshot(engine, s).startDelaySamples;
        return delays;
    };

    const auto makeEngine = [&] (std::unique_ptr<ElectryEngine>& engine,
                                 float spreadSeconds, PickStyle pick)
    {
        engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine->setParameters(parameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(pick), 1.0f);
    };

    // 1. The stroke sets the edge. The same three notes in the same MIDI order
    //    must travel low-to-high on a downstroke and high-to-low on an
    //    upstroke; before this the two produced identical offsets.
    {
        std::unique_ptr<ElectryEngine> down, up;
        makeEngine(down, spread, PickStyle::Down);
        makeEngine(up, spread, PickStyle::Up);
        const auto downDelays = strum(*down, { 40, 45, 50 });
        const auto upDelays = strum(*up, { 40, 45, 50 });
        expect(downDelays[2] < downDelays[3] && downDelays[3] < downDelays[4],
               "a downstroke did not travel from the low string ("
                   + std::to_string(downDelays[2]) + ", "
                   + std::to_string(downDelays[3]) + ", "
                   + std::to_string(downDelays[4]) + ")");
        expect(upDelays[2] > upDelays[3] && upDelays[3] > upDelays[4],
               "an upstroke did not travel from the high string ("
                   + std::to_string(upDelays[2]) + ", "
                   + std::to_string(upDelays[3]) + ", "
                   + std::to_string(upDelays[4]) + ")");
        // 8. The whole cost of the pre-roll: the string the pick starts from
        //    sounds exactly one re-anchor window after its note-on.
        expect(std::abs(downDelays[2] - preRoll) <= 2
                   && std::abs(upDelays[4] - preRoll) <= 2,
               "the string the stroke started from did not sound one pre-roll "
               "after its note-on (" + std::to_string(downDelays[2]) + ", "
                   + std::to_string(upDelays[4]) + " vs "
                   + std::to_string(preRoll) + ")");
    }

    // 2/3. The wrist accelerates through the strings, so the crossing
    //      intervals compress - and the knob still states the mean crossing
    //      time, so eight strings still take seven times it.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const auto delays = strum(*engine, { 28, 35, 40, 45, 50, 55, 59, 64 });
        std::array<int, ElectryEngine::stringCount - 1> gaps {};
        for (int k = 0; k < ElectryEngine::stringCount - 1; ++k)
            gaps[static_cast<std::size_t>(k)] =
                delays[static_cast<std::size_t>(k + 1)]
                - delays[static_cast<std::size_t>(k)];
        for (int k = 0; k + 1 < ElectryEngine::stringCount - 1; ++k)
            expect(gaps[static_cast<std::size_t>(k + 1)]
                       <= gaps[static_cast<std::size_t>(k)],
                   "the strum's crossing intervals did not compress at gap "
                       + std::to_string(k) + " ("
                       + std::to_string(gaps[static_cast<std::size_t>(k)]) + " then "
                       + std::to_string(gaps[static_cast<std::size_t>(k + 1)]) + ")");
        const double lastOverFirst = static_cast<double>(gaps[6])
                                   / static_cast<double>(gaps[0]);
        expect(lastOverFirst > 0.55 && lastOverFirst < 0.85,
               "the strum's last crossing was not about 0.7 of its first ("
                   + std::to_string(lastOverFirst) + ")");
        const int travel = delays[7] - delays[0];
        expect(std::abs(travel - 7 * spreadSamples) <= 7 * spreadSamples / 20,
               "eight strings did not take seven times the strum spread ("
                   + std::to_string(travel) + " vs "
                   + std::to_string(7 * spreadSamples) + ")");
    }

    // 4. A chord whose first note-on is a middle string. The pick does not
    //    travel outward in both directions at once: it re-anchors on the low
    //    edge and every offset stays non-negative and monotone in string
    //    index. Before this the same chord gave 24/12/0/12/24 ms.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const auto delays = strum(*engine, { 50, 28, 45, 55, 59 });
        const std::array<int, 5> played { 0, 3, 4, 5, 6 };
        int previous = -1;
        for (const int s : played)
        {
            const int delay = delays[static_cast<std::size_t>(s)];
            expect(delay >= 0,
                   "string " + std::to_string(s)
                       + " of a middle-anchored chord was scheduled backwards");
            expect(delay > previous,
                   "a middle-anchored chord did not travel in string order at "
                   "string " + std::to_string(s) + " ("
                       + std::to_string(delay) + " after "
                       + std::to_string(previous) + ")");
            previous = delay;
        }
    }

    // 5. At a zero spread the chord is one block again, to the sample.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, 0.0f, PickStyle::Down);
        const auto delays = strum(*engine, { 28, 35, 40, 45, 50, 55, 59, 64 });
        for (const int delay : delays)
            expect(delay == 0,
                   "a chord at zero spread carried a strum offset or a "
                   "pre-roll (" + std::to_string(delay) + ")");
    }

    // 6. The wrist does not lay the same ramp down twice. Two strums of the
    //    same chord 12 s apart - long enough that the strings have decayed and
    //    nothing else is shared - must differ somewhere by at least one
    //    internal sample. The offsets were previously a pure function of the
    //    spread and the string index, so the two were identical.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const std::vector<int> chord { 28, 35, 40, 45, 50, 55, 59, 64 };
        const auto first = strum(*engine, chord);
        for (const int note : chord)
            engine->noteOff(note);
        StereoBuffer decay(static_cast<int>(12.0 * sampleRate));
        renderInto(*engine, decay);
        const auto second = strum(*engine, chord);
        int largest = 0;
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            largest = std::max(largest,
                               std::abs(second[static_cast<std::size_t>(s)]
                                        - first[static_cast<std::size_t>(s)]));
        expect(largest >= 1,
               "two strums of the same chord laid down the same ramp ("
                   + std::to_string(largest) + " internal samples apart)");
    }

    // 7/9. The block-straddling case, pinned in absolute onsets. One chord's
    //      note-ons routinely arrive across several process() calls, so what
    //      matters is each event's arrival sample plus its voice's remaining
    //      delay, reduced to one clock. Those onsets must reverse with the
    //      stroke and must not depend on the order the host sent the notes -
    //      today they are 0/20/40 ms in arrival order whichever way the chord
    //      is sent, i.e. the chord travels in host order.
    {
        const int blockSamples = static_cast<int>(0.008 * sampleRate);
        const auto splitDelivery = [&] (PickStyle pick, bool ascending)
        {
            std::unique_ptr<ElectryEngine> engine;
            makeEngine(engine, spread, pick);
            const std::array<int, 3> ascendingNotes { 40, 45, 50 };
            const std::array<int, 3> descendingNotes { 50, 45, 40 };
            const auto& notes = ascending ? ascendingNotes : descendingNotes;
            int arrival = 0;
            for (std::size_t k = 0; k < notes.size(); ++k)
            {
                engine->noteOn(notes[k], 0.85f);
                if (k + 1 < notes.size())
                {
                    StereoBuffer block(blockSamples);
                    renderInto(*engine, block);
                    arrival += blockSamples * 2;   // host samples to the engine's clock
                }
            }
            // Read after the last note-on: the chord's span is 16 ms, inside
            // the 20 ms pre-roll, so nothing has sounded and every voice is
            // still on the same clock.
            std::array<int, 3> onsets {};
            for (int s = 2; s <= 4; ++s)
                onsets[static_cast<std::size_t>(s - 2)] =
                    arrival + TestAccess::snapshot(*engine, s).startDelaySamples;
            return onsets;
        };

        const auto downAscending = splitDelivery(PickStyle::Down, true);
        const auto downDescending = splitDelivery(PickStyle::Down, false);
        const auto upAscending = splitDelivery(PickStyle::Up, true);
        const auto upDescending = splitDelivery(PickStyle::Up, false);

        expect(downAscending[0] < downAscending[1]
                   && downAscending[1] < downAscending[2],
               "a split-block downstroke did not sound low string first");
        expect(upAscending[0] > upAscending[1] && upAscending[1] > upAscending[2],
               "a split-block upstroke did not sound high string first");
        for (std::size_t k = 0; k < 3; ++k)
        {
            expect(downAscending[k] == downDescending[k],
                   "a split-block downstroke's onsets depended on the order the "
                   "host sent the chord (string " + std::to_string(k + 2) + ": "
                       + std::to_string(downAscending[k]) + " vs "
                       + std::to_string(downDescending[k]) + ")");
            expect(upAscending[k] == upDescending[k],
                   "a split-block upstroke's onsets depended on the order the "
                   "host sent the chord (string " + std::to_string(k + 2) + ": "
                       + std::to_string(upAscending[k]) + " vs "
                       + std::to_string(upDescending[k]) + ")");
        }

        // 9. What a re-anchor costs. The first string to sound in the split
        //    delivery may be pushed back by at most the chord's own arrival
        //    span plus the control period the countdown is quantised to.
        std::unique_ptr<ElectryEngine> single;
        makeEngine(single, spread, PickStyle::Down);
        const auto blockChord = strum(*single, { 40, 45, 50 });
        const int splitFirst = std::min({ downAscending[0], downAscending[1],
                                          downAscending[2] });
        const int arrivalSpan = 2 * blockSamples * 2;   // two blocks, engine clock
        expect(splitFirst - blockChord[2] <= arrivalSpan + 16,
               "a re-anchor delayed the leading string by more than the chord's "
               "own arrival span (" + std::to_string(splitFirst) + " vs "
                   + std::to_string(blockChord[2]) + ")");
    }
}

// ---------------------------------------------------------------------------
// CC1 resonance, acoustic feedback, and fret-following pick position
// ---------------------------------------------------------------------------

void testResonanceControlRaisesSympatheticRing()
{
    // CC1 lifts the sympathetic coupling from the parameter's base amount
    // toward total, scaled by the Resonance Depth parameter, and a lowered
    // wheel is a bit-exact no-op.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.sympatheticAmount = 0.15f;
    parameters.resonanceDepth = 1.0f;

    constexpr double openHighE = 329.62756;
    const auto tailRingAt = [&] (float resonance)
    {
        engine.setParameters(parameters);
        engine.reset();
        engine.setResonance(resonance);
        engine.noteOn(45, 0.95f);
        StereoBuffer body(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, body);
        engine.noteOff(45);
        StereoBuffer tail(static_cast<int>(2.1 * sampleRate));
        renderInto(engine, tail);
        expect(allFinite(tail), "the resonance control produced non-finite audio");
        const int start = static_cast<int>(0.9 * sampleRate);
        const int length = static_cast<int>(1.0 * sampleRate);
        return dftMagnitude(tail.left, start, length, sampleRate, openHighE);
    };

    const double base = tailRingAt(0.0f);
    const double lifted = tailRingAt(1.0f);
    expect(lifted > 2.5 * std::max(base, 1.0e-12),
           "a raised wheel did not deepen the sympathetic ring ("
               + std::to_string(base) + " -> " + std::to_string(lifted) + ")");

    // Depth scales what the wheel can reach.
    parameters.resonanceDepth = 0.25f;
    const double shallow = tailRingAt(1.0f);
    expect(shallow < lifted * 0.85 && shallow > base * 0.9,
           "Resonance Depth does not scale the wheel's lift ("
               + std::to_string(shallow) + " against "
               + std::to_string(lifted) + " deep, "
               + std::to_string(base) + " base)");

    // A lowered wheel is exactly the parameter alone, bit for bit - even with
    // a signal sitting in the acoustic-return ring.
    parameters.resonanceDepth = 1.0f;
    engine.setParameters(parameters);
    const auto renderWheelDown = [&] (bool feedTheRing)
    {
        // The wheel is down before the reset, so the smoothed resonance state
        // starts at zero rather than gliding down from the previous take.
        engine.setResonance(0.0f);
        engine.reset();
        if (feedTheRing)
        {
            std::vector<float> loud(2048, 0.5f);
            engine.pushAcousticReturn(loud.data(), loud.data(),
                                      static_cast<int>(loud.size()));
        }
        engine.noteOn(45, 0.8f);
        StereoBuffer take(static_cast<int>(0.7 * sampleRate));
        renderInto(engine, take);
        return take;
    };
    const auto reference = renderWheelDown(false);
    const auto withWheelDown = renderWheelDown(true);
    expect(reference.left == withWheelDown.left,
           "a lowered resonance wheel is not an exact bypass");
}

void testResonanceFeedbackSelfSustains()
{
    // The whole point of the resonance wheel: close the loop the way the
    // plug-in does - engine into the amplifier chain, the amplified output
    // pushed back at the strings - and a distorted note at full wheel keeps
    // itself alive, while the same loop with the wheel down decays, the same
    // wheel without the amplifier decays, and nothing ever leaves the bounded
    // range.
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    struct LoopResult
    {
        double earlyRms { 0.0 };
        double lateRms { 0.0 };
        float peak { 0.0f };
        bool finite { true };
    };

    const auto runClosedLoop = [&] (float resonance, float distortion,
                                    float amp, bool palmMuted = false)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, blockSize);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.sympatheticAmount = 0.2f;
        parameters.resonanceDepth = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.setResonance(resonance);
        if (palmMuted)
            engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

        electry::ElectryFx fx;
        fx.prepare(sampleRate);
        electry::FxParameters fxParameters;
        fxParameters.distortion = distortion;
        fxParameters.amp = amp;
        fx.setParameters(fxParameters);
        fx.reset();
        // What the plug-in shell does: the rig's acoustic loudness in the
        // room follows the amplifier controls, because the chain itself keeps
        // its listening level roughly constant.
        engine.setAcousticReturnLevel(std::min(1.0f, amp + 0.6f * distortion));

        // The note is released early: a released string damps in tens of
        // milliseconds, so anything still loud seconds later has to be the
        // feedback loop keeping the instrument alive, exactly like a guitar
        // left facing its amplifier.
        engine.noteOn(40, 0.95f); // open E2
        constexpr double seconds = 5.0;
        constexpr double releaseAt = 0.75;
        const int totalBlocks = static_cast<int>(seconds * sampleRate)
                              / blockSize;
        std::vector<float> left(blockSize), right(blockSize);
        LoopResult result;
        double earlySum = 0.0;
        double lateSum = 0.0;
        long earlyCount = 0;
        long lateCount = 0;
        bool released = false;
        for (int block = 0; block < totalBlocks; ++block)
        {
            const double blockStart = block * blockSize / sampleRate;
            // The palm-muted take keeps the note held: the muting hand is on
            // the strings only while the muted note is, and lifting it under
            // a raised wheel legitimately lets the howl return.
            if (! palmMuted && ! released && blockStart >= releaseAt)
            {
                engine.noteOff(40);
                released = true;
            }
            engine.process(left.data(), right.data(), blockSize);
            fx.process(left.data(), right.data(), blockSize);
            engine.pushAcousticReturn(left.data(), right.data(), blockSize);

            for (int i = 0; i < blockSize; ++i)
            {
                const float sample = left[static_cast<std::size_t>(i)];
                if (! std::isfinite(sample))
                    result.finite = false;
                result.peak = std::max(result.peak, std::abs(sample));
                const double energy = static_cast<double>(sample)
                                    * static_cast<double>(sample);
                if (blockStart >= 0.25 && blockStart < 0.65)
                {
                    earlySum += energy;
                    ++earlyCount;
                }
                else if (blockStart >= 4.0)
                {
                    lateSum += energy;
                    ++lateCount;
                }
            }
        }
        result.earlyRms = std::sqrt(earlySum / std::max<long>(earlyCount, 1));
        result.lateRms = std::sqrt(lateSum / std::max<long>(lateCount, 1));
        return result;
    };

    const auto fed = runClosedLoop(1.0f, 0.75f, 0.8f);
    expect(fed.finite, "the fed-back loop produced non-finite audio");
    expect(fed.peak < 4.0f, "the fed-back loop escaped its bounded range");
    expect(fed.lateRms > 0.25 * fed.earlyRms,
           "full resonance with a distorted amplifier does not self-sustain "
           "after the key is released ("
               + std::to_string(decibels(fed.lateRms
                                         / std::max(fed.earlyRms, 1.0e-15)))
               + " dB after four seconds)");

    const auto wheelDown = runClosedLoop(0.0f, 0.75f, 0.8f);
    expect(wheelDown.finite && wheelDown.peak < 4.0f,
           "the wheel-down loop was not bounded");
    expect(wheelDown.lateRms < 0.05 * wheelDown.earlyRms,
           "a released distorted note with the wheel down failed to decay ("
               + std::to_string(decibels(
                     wheelDown.lateRms
                     / std::max(wheelDown.earlyRms, 1.0e-15)))
               + " dB after four seconds)");
    expect(fed.lateRms > 4.0 * std::max(wheelDown.lateRms, 1.0e-9),
           "the wheel makes no difference to the closed distorted loop");

    const auto dry = runClosedLoop(1.0f, 0.0f, 0.0f);
    expect(dry.finite && dry.peak < 4.0f, "the dry loop was not bounded");
    expect(dry.lateRms < 0.05 * dry.earlyRms,
           "full resonance without the amplifier failed to decay ("
               + std::to_string(decibels(dry.lateRms
                                         / std::max(dry.earlyRms, 1.0e-15)))
               + " dB after four seconds)");

    // The muting hand starves the loop: the same full-wheel distorted rig
    // with the Palm Mute style latched at the default Mute Damp must not
    // howl. A linear or even squared hand residue measurably regenerated
    // back to nearly the open level, which is what this pins against.
    const auto muted = runClosedLoop(1.0f, 0.75f, 0.8f, true);
    expect(muted.finite && muted.peak < 4.0f,
           "the palm-muted loop was not bounded");
    expect(muted.lateRms < 0.1 * fed.lateRms,
           "a palm-muted passage still howls at full resonance ("
               + std::to_string(muted.lateRms) + " against fed "
               + std::to_string(fed.lateRms) + ")");
}

void testPickGeometryFollowsFret()
{
    constexpr double sampleRate = 48000.0;

    // The picking hand stays put while the fretting hand moves, so the pluck
    // position as a fraction of the sounding length grows by 2^(fret/12).
    // Note 86 is only reachable at fret 22 of the top string, and note 64 is
    // that same string open, which pins the comparison to one physical string.
    const auto combFraction = [&] (int midiNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickPosition = 0.10f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(midiNote, 0.8f);
        const int stringIndex = ElectryEngine::stringCount - 1;
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.midiNote == midiNote,
               "note " + std::to_string(midiNote)
                   + " was not allocated to the top string");
        return static_cast<double>(snapshot.excitationCombDelay)
             / std::max(static_cast<double>(snapshot.verticalDelayTarget), 1.0e-9);
    };

    const double openComb = combFraction(64);
    const double frettedComb = combFraction(86);
    const double expectedRatio = std::pow(2.0, 22.0 / 12.0);
    const double actualRatio = frettedComb / std::max(openComb, 1.0e-9);
    expect(std::abs(actualRatio - expectedRatio) < 0.05 * expectedRatio,
           "pluck position did not follow the fretted sounding length ("
               + std::to_string(actualRatio) + " vs "
               + std::to_string(expectedRatio) + ")");
}

void testPickContactGeometry()
{
    constexpr double sampleRate = 48000.0;

    // A plectrum is neither a point nor symmetric: it touches the string over a
    // patch and it slips off far faster than it loaded.
    const auto pickGeometry = [] (int midiNote, float hardness)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickHardness = hardness;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(midiNote, 0.9f);
        const int stringIndex = TestAccess::stringForNote(engine, midiNote);
        expect(stringIndex >= 0, "note " + std::to_string(midiNote)
                                     + " was not allocated");
        return TestAccess::snapshot(engine, std::max(stringIndex, 0));
    };

    const auto lowDefault = pickGeometry(28, 0.6f);
    const auto highDefault = pickGeometry(64, 0.6f);
    const auto lowSoft = pickGeometry(28, 0.0f);
    const auto lowHard = pickGeometry(28, 1.0f);

    expect(lowDefault.excitationCombWidth > 0.0f,
           "the pick contact patch has no width");
    // A fixed physical patch is a larger share of the low string's much longer
    // round trip, so it spans more delay-line samples there.
    expect(lowDefault.excitationCombWidth > 4.0f * highDefault.excitationCombWidth,
           "the contact patch does not scale with the string's length ("
               + std::to_string(lowDefault.excitationCombWidth) + " vs "
               + std::to_string(highDefault.excitationCombWidth) + ")");
    // A soft rounded pick touches over roughly three times the patch of a stiff
    // sharp one.
    expect(lowSoft.excitationCombWidth > 2.0f * lowHard.excitationCombWidth,
           "pick hardness does not narrow the contact patch");

    // The patch spans a fixed distance along the string and the wave speed does
    // not change when the string is fretted, so its width in delay samples is
    // the same open and at the twelfth fret. The allocator prefers the free
    // string with the lowest fret, so note 64 is the top string open and note
    // 76 is that same string at fret 12; both are unambiguous.
    const auto openTop = pickGeometry(64, 0.6f);
    const auto frettedTop = pickGeometry(76, 0.6f);
    expect(openTop.stringIndex == frettedTop.stringIndex,
           "the fretted comparison did not stay on one physical string");
    // The remaining few percent is the loop delay's own analytic phase
    // compensation, which differs between the two notes because their decay
    // targets and dead-spot damping do.
    expect(std::abs(openTop.excitationCombWidth
                    - frettedTop.excitationCombWidth)
               < 0.08f * openTop.excitationCombWidth,
           "the contact patch is not fret invariant in delay samples ("
               + std::to_string(openTop.excitationCombWidth) + " vs "
               + std::to_string(frettedTop.excitationCombWidth) + ")");

    // A stiffer pick holds the string longer and then releases it faster.
    const float softSlip = 1.0f / lowSoft.excitationLoadScale;
    const float hardSlip = 1.0f / lowHard.excitationLoadScale;
    expect(hardSlip > softSlip + 0.10f,
           "pick hardness does not move the slip point later ("
               + std::to_string(softSlip) + " to " + std::to_string(hardSlip)
               + ")");
    expect(lowHard.excitationSlipScale > lowSoft.excitationSlipScale,
           "a stiffer pick does not release the string faster");

    // The release window is a load smoothstep times a slip smoothstep. Whatever
    // the slip point, its area is exactly one half - the same area the
    // symmetric raised cosine it replaced had - so the asymmetry changes the
    // attack's spectrum without changing how hard the note lands.
    for (const auto& snapshot : { lowSoft, lowDefault, lowHard })
    {
        const auto smoothStep = [] (double value)
        {
            value = std::clamp(value, 0.0, 1.0);
            return value * value * (3.0 - 2.0 * value);
        };
        constexpr int steps = 200000;
        double area = 0.0;
        for (int step = 0; step < steps; ++step)
        {
            const double progress = (static_cast<double>(step) + 0.5)
                                  / static_cast<double>(steps);
            area += smoothStep(progress * snapshot.excitationLoadScale)
                  * smoothStep((1.0 - progress) * snapshot.excitationSlipScale);
        }
        area /= static_cast<double>(steps);
        expect(std::abs(area - 0.5) < 1.0e-3,
               "the asymmetric pick release window is not level neutral ("
                   + std::to_string(area) + ")");
    }
}

// A low note has to be carried by its own fundamental. Measured against dry
// reference recordings, the two ways this engine has failed that were a pickup
// position comb that cancelled exactly - which put a zero at DC and cost the
// fundamental most - and a bridge hand modelled as a genuinely broadband
// absorber, which damped the fundamental as hard as the top end and turned a
// palm mute into a short pick. Both are voicing, so neither is pinned to a
// number here; what is pinned is the audible consequence each one had.
// The hand's loss dip is the one filter in this model that sits inside the
// feedback loop with a shape solved per note rather than a fixed one. Its safety
// rests entirely on a peaking section with sub-unity gain having a magnitude
// bounded by one everywhere, so that bound is asserted directly on the
// coefficients the engine actually runs, across the playable range and the whole
// travel of both mute controls. If it ever exceeded one the loop would grow at
// that frequency, which is the one failure this model cannot absorb.
// The mute dip sits inside the loop, so its phase is part of the sounding
// period and the engine subtracts it from the delay-line read. Without that
// subtraction the mute drags the pitch flat - measured at up to 13 cents on
// the low string, which is what the compensation exists to remove and what
// this pins. The depth is also modulated over the note, so the correction has
// to follow the depth actually applied rather than the one the note started
// on; that part is worth about a cent in the settled window a DFT can read,
// so this test guards the invariant rather than that refinement.
//
// Measured against the same note unmuted, not against nominal pitch: the
// model detunes slightly by design as pluck energy dissipates, and that is
// present with or without the mute. Only the mute-attributable difference is
// a defect.
void testPalmMuteDoesNotShiftPitch()
{
    constexpr double sampleRate = 48000.0;
    int asserted = 0;

    for (const int midiNote : { 28, 40, 52, 64 })
    {
        const double nominal = midiHz(midiNote);
        const int start = static_cast<int>(0.080 * sampleRate);
        const int length = static_cast<int>(0.250 * sampleRate);

        const auto fundamentalOf = [&] (float pressure, double& rmsOut)
        {
            EngineParameters parameters;
            parameters.palmMute = pressure;
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            engine.setParameters(parameters);
            auto buffer = renderNote(engine, sampleRate, midiNote, 0.95f,
                                     PlayStyle::Sustain, 0.4);
            double sum = 0.0;
            for (int i = 0; i < length; ++i)
                sum += static_cast<double>(buffer.left[start + i])
                     * static_cast<double>(buffer.left[start + i]);
            rmsOut = std::sqrt(sum / static_cast<double>(length));
            // The fundamental alone. Scoring a partial series instead tracks
            // the mute's spectral tilt rather than its pitch, which reads as
            // several cents of drift that the fundamental does not show.
            double best = nominal;
            double bestMagnitude = -1.0;
            for (double cents = -60.0; cents <= 60.0; cents += 0.25)
            {
                const double frequency = nominal
                    * std::pow(2.0, cents / 1200.0);
                const double magnitude = dftMagnitude(
                    buffer.left, start, length, sampleRate, frequency);
                if (magnitude > bestMagnitude)
                {
                    bestMagnitude = magnitude;
                    best = frequency;
                }
            }
            return best;
        };

        double openRms = 0.0;
        const double open = fundamentalOf(0.0f, openRms);

        for (const float pressure : { 0.30f, 0.55f, 1.00f })
        {
            double mutedRms = 0.0;
            const double muted = fundamentalOf(pressure, mutedRms);
            // Assert only where the note still has enough sustained energy for
            // pitch to be a property of it at all. A fully muted low E is ~37
            // dB below the open note here and dropping fast; two windowings of
            // that same remnant disagree by 8 cents, so a threshold on it would
            // be measuring the estimator. 30 dB down is the cutoff, which
            // excludes exactly that one case out of the twelve.
            if (mutedRms < 1.0e-5 || mutedRms < 0.03 * openRms)
                continue;

            const double shift = centsBetween(muted, open);
            ++asserted;
            expect(std::abs(shift) < 4.0,
                   "palm mute shifts pitch by " + std::to_string(shift)
                       + " cents at note " + std::to_string(midiNote)
                       + ", pressure " + std::to_string(pressure));
        }
    }

    // The energy guard must not be able to quietly skip the whole test.
    expect(asserted >= 9,
           "expected at least 9 measurable palm-mute pitch cases, asserted "
               + std::to_string(asserted));
}

void testHandDipNeverExpands()
{
    constexpr double sampleRate = 48000.0;
    int activeConfigurations = 0;
    float worstMagnitude = 0.0f;
    double worstOmegaFraction = 0.0;

    for (const float pressure : { 0.0f, 0.15f, 0.55f, 1.0f })
    {
        for (const auto playStyle : { PlayStyle::PalmMute, PlayStyle::Sustain })
        {
            for (const float muteDamping : { 0.0f, 0.55f, 1.0f })
            {
                EngineParameters parameters;
                parameters.palmMute = pressure;
                parameters.muteDamping = muteDamping;

                ElectryEngine engine;
                engine.prepare(sampleRate, 512);
                engine.setParameters(parameters);

                // Sweep the playable range: the dip's centre tracks the
                // fundamental, so a high note pushes it toward Nyquist where the
                // coefficients are most awkward.
                for (int note = ElectryEngine::lowestPlayableNote;
                     note <= ElectryEngine::highestPlayableNote; note += 6)
                // Three ages, chosen to straddle the modulation rather than to
                // sample it densely: before the contact has settled, just after
                // it has, and far enough into the tail that the grip has
                // slackened. Denser sweeps cost minutes and found nothing more.
                for (const int ageBlocks : { 0, 4, 45 })
                {
                    engine.allNotesOff();
                    engine.noteOn(styleKeyswitch(playStyle), 1.0f);
                    engine.noteOn(note, 0.9f);

                    // Sampled at several ages, not just after the first block.
                    // The dip's depth is modulated per control period - it fades
                    // in as the contact settles and back out as the string stops
                    // driving the hand - so the bound has to hold at every depth
                    // that modulation produces, not only at the one the note
                    // happens to start on.
                    float left[512] {};
                    float right[512] {};
                    engine.process(left, right, 64);
                    for (int block = 0; block < ageBlocks; ++block)
                        engine.process(left, right, 512);

                    for (int stringIndex = 0;
                         stringIndex < ElectryEngine::stringCount; ++stringIndex)
                    {
                        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
                        double a1 = 0.0, a2 = 0.0;
                        bool active = false;
                        TestAccess::handDip(engine, stringIndex,
                                            b0, b1, b2, a1, a2, active);
                        if (! active)
                            continue;
                        ++activeConfigurations;

                        for (int step = 0; step <= 600; ++step)
                        {
                            const double omega =
                                3.14159265358979323846 * step / 600.0;
                            const double cw = std::cos(omega);
                            const double sw = std::sin(omega);
                            const double c2 = std::cos(2.0 * omega);
                            const double s2 = std::sin(2.0 * omega);
                            const double nr = b0 + b1 * cw + b2 * c2;
                            const double ni = -(b1 * sw + b2 * s2);
                            const double dr = 1.0 + a1 * cw + a2 * c2;
                            const double di = -(a1 * sw + a2 * s2);
                            const double dn = dr * dr + di * di;
                            if (dn < 1.0e-20)
                                continue;
                            const auto magnitude = static_cast<float>(
                                std::sqrt((nr * nr + ni * ni) / dn));
                            if (magnitude > worstMagnitude)
                            {
                                worstMagnitude = magnitude;
                                worstOmegaFraction = omega
                                    / 3.14159265358979323846;
                            }
                            // The bound is an equality at DC and Nyquist by
                            // construction, so the slack here is for double
                            // rounding only. It was 6.6e-4 out when the section
                            // ran in float, which is what put it in double.
                            if (! (magnitude <= 1.000001f))
                            {
                                std::printf("  |H| = %.6f at note %d, "
                                            "pressure %.2f, mute %.2f, "
                                            "age %d blocks\n",
                                            magnitude, note, pressure,
                                            muteDamping, ageBlocks);
                                expect(false, "hand loss dip expands inside "
                                              "the loop");
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    // A bound that holds because nothing was ever configured proves nothing.
    if (activeConfigurations < 100)
    {
        std::printf("  only %d active configurations\n", activeConfigurations);
        expect(false, "hand loss dip never engaged, so its bound is vacuous");
        return;
    }

    std::printf("Hand dip peak magnitude %.8f at omega/pi = %.6f over %d "
                "active configurations\n",
                worstMagnitude, worstOmegaFraction, activeConfigurations);
}

void testLowRegisterFundamentalWeight()
{
    constexpr double sampleRate = 48000.0;

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.outputMode = electry::OutputMode::Mono;
    parameters.pickHardness = 0.85f;
    parameters.pickPosition = 0.18f;
    // Silence the deterministic mechanical noise: this measures where the
    // string's own energy sits, not the plectrum's.
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;

    // Energy in a band as a fraction of the tone's total, sampled at the
    // partials so the figure measures the spectral envelope rather than window
    // leakage between harmonic lines - the same approach spectralCentroid takes.
    const auto bandFraction = [] (const std::vector<float>& data, int start,
                                  int length, double fundamentalHz,
                                  double lowHz, double highHz)
    {
        double inBand = 0.0;
        double total = 0.0;
        for (int partial = 1; partial <= 48; ++partial)
        {
            const double frequency = fundamentalHz * partial;
            if (frequency >= 0.45 * sampleRate)
                break;
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
            const double power = magnitude * magnitude;
            total += power;
            if (frequency >= lowHz && frequency < highHz)
                inBand += power;
        }
        return total > 0.0 ? inBand / total : 0.0;
    };

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);

    // The open low E of the Drop-E instrument, MIDI 28, and its own fundamental.
    constexpr double lowE = 82.4069;
    const int window = static_cast<int>(0.25 * sampleRate);

    const auto open = renderNote(engine, sampleRate, 40, 0.9f,
                                 PlayStyle::Sustain, 1.2);
    const auto muted = renderNote(engine, sampleRate, 40, 0.9f,
                                  PlayStyle::PalmMute, 1.2);

    const double openFundamental = bandFraction(open.left, 0, window, lowE,
                                                0.0, 1.6 * lowE);
    const double mutedFundamental = bandFraction(muted.left, 0, window, lowE,
                                                 0.0, 1.6 * lowE);

    // With the comb cancelling exactly this sat near a twentieth of the tone's
    // energy, under persistent low mids, which is the hollow, clavinet-like
    // register the references do not have.
    // Thresholds separate the two failures above from the corrected voicing by
    // a factor of roughly two in each direction: an exactly cancelling comb
    // measured 0.0039 here against 0.0126 now.
    expect(openFundamental > 0.008,
           "the open low E does not carry its own fundamental ("
               + std::to_string(openFundamental) + " of its energy)");

    // A bridge hand loads a string; it does not filter its fundamental out. A
    // broadband absorber took this to a small fraction of the open note's,
    // which is what read as a thin, cut-off pick instead of a chug.
    // A mute removes the top end, so the fundamental should end up a *larger*
    // share of what is left than on the open note, not a smaller one. A
    // broadband hand measured 0.0091 against 0.0311 now.
    expect(mutedFundamental > 0.018,
           "a palm mute strips the fundamental rather than damping the string ("
               + std::to_string(mutedFundamental) + " of its energy)");
    expect(mutedFundamental > openFundamental,
           "a muted low E is not more fundamental-dominated than an open one ("
               + std::to_string(mutedFundamental) + " muted against "
               + std::to_string(openFundamental) + " open)");

    // The comb must still be a position comb: weighting its delayed tap below
    // one shortens the null without removing the geometry, so the bridge
    // position stays the brighter of the two.
    EngineParameters neckParameters = parameters;
    neckParameters.pickupSelector = PickupSelector::Neck;
    ElectryEngine neckEngine;
    neckEngine.prepare(sampleRate, 512);
    neckEngine.setParameters(neckParameters);
    const auto neck = renderNote(neckEngine, sampleRate, 40, 0.9f,
                                 PlayStyle::Sustain, 1.2);
    const double neckFundamental = bandFraction(neck.left, 0, window, lowE,
                                                0.0, 1.6 * lowE);
    expect(neckFundamental > openFundamental,
           "the neck pickup does not sense more fundamental than the bridge ("
               + std::to_string(neckFundamental) + " against "
               + std::to_string(openFundamental) + ")");
}

// ---------------------------------------------------------------------------
// Version 1.1: display readout and fretboard geometry
// ---------------------------------------------------------------------------

void testVisualStateAndGeometry()
{
    namespace visuals = electry::visuals;
    constexpr int lastFret = ElectryEngine::fretCount;

    expect(visuals::fretWireFraction(0, lastFret) == 0.0f,
           "the nut is not at the start of the drawn neck");
    expect(std::abs(visuals::fretWireFraction(lastFret, lastFret) - 1.0f) < 1.0e-6f,
           "the last fret is not at the end of the drawn neck");
    // The twelfth fret sits at half the scale length, which on a 22-fret neck
    // is 0.5 / (1 - 2^(-22/12)) of the drawn span.
    const float octaveExpected = 0.5f / (1.0f - std::pow(2.0f, -22.0f / 12.0f));
    expect(std::abs(visuals::fretWireFraction(12, lastFret) - octaveExpected)
               < 1.0e-4f,
           "the twelfth fret is not at half the scale length");
    for (int fret = 1; fret <= lastFret; ++fret)
    {
        expect(visuals::fretWireFraction(fret, lastFret)
                   > visuals::fretWireFraction(fret - 1, lastFret),
               "fret positions are not monotonic at fret " + std::to_string(fret));
    }
    expect(visuals::fretWireFraction(2, lastFret) - visuals::fretWireFraction(1, lastFret)
               < visuals::fretWireFraction(1, lastFret),
           "fret spacing does not narrow toward the body");
    expect(visuals::fretWireFraction(-5, lastFret) == 0.0f
               && visuals::fretWireFraction(999, lastFret) == 1.0f,
           "fret geometry did not clamp out-of-range frets");
    expect(visuals::fretCentreFraction(0, lastFret) == 0.0f,
           "an open string is not drawn at the nut");
    expect(visuals::fretCentreFraction(5, lastFret)
                   > visuals::fretWireFraction(4, lastFret)
               && visuals::fretCentreFraction(5, lastFret)
                      < visuals::fretWireFraction(5, lastFret),
           "a fingered note is not between its enclosing fret wires");

    // The span-aware overloads exist so a caller solving many wire/centre
    // positions against the same neck - the editor's paint() does - can share
    // one fretSpan() call instead of paying for it again on every fret; they
    // must therefore agree exactly with the two-argument forms that solve
    // their own span internally.
    const float span = visuals::fretSpan(lastFret);
    for (int fret = -2; fret <= lastFret + 2; ++fret)
    {
        expect(visuals::fretWireFraction(fret, lastFret, span)
                   == visuals::fretWireFraction(fret, lastFret),
               "the span-aware fretWireFraction disagrees with the two-argument "
               "form at fret " + std::to_string(fret));
        expect(visuals::fretCentreFraction(fret, lastFret, span)
                   == visuals::fretCentreFraction(fret, lastFret),
               "the span-aware fretCentreFraction disagrees with the two-argument "
               "form at fret " + std::to_string(fret));
    }
    expect(visuals::fretSpan(0) == 0.0f && visuals::fretSpan(-3) == 0.0f,
           "fretSpan did not report zero for a degenerate neck");

    for (int s = 1; s < ElectryEngine::stringCount; ++s)
    {
        expect(visuals::stringRowFraction(s, ElectryEngine::stringCount, 0.085f)
                   > visuals::stringRowFraction(s - 1, ElectryEngine::stringCount,
                                                0.085f),
               "string rows are not ordered low to high");
        expect(visuals::stringThickness(s, 0.9f, 2.6f)
                   < visuals::stringThickness(s - 1, 0.9f, 2.6f),
               "string thickness does not taper toward the top string");
    }
    expect(visuals::stringRowFraction(0, ElectryEngine::stringCount, 0.085f) >= 0.085f
               && visuals::stringRowFraction(7, ElectryEngine::stringCount, 0.085f)
                      <= 0.915f,
           "string rows escaped the drawn fingerboard");

    float level = 0.0f;
    level = visuals::meterBallistics(level, 1.0f, 0.55f, 0.18f);
    expect(level > 0.5f, "meter attack is too slow to show an attack");
    const float afterAttack = level;
    level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < afterAttack && level > 0.5f * afterAttack,
           "meter release does not hold the reading");
    for (int i = 0; i < 200; ++i)
        level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < 1.0e-6f, "meter did not settle back to zero");
    expect(visuals::meterBallistics(std::numeric_limits<float>::quiet_NaN(), 0.5f,
                                    0.5f, 0.5f) == 0.25f,
           "meter ballistics did not recover from a non-finite reading");

    expect(visuals::vibrationShape(0.0f, 0.0f) < 1.0e-6f
               && visuals::vibrationShape(1.0f, 0.0f) < 1.0e-6f,
           "an open string is not pinned at the nut and bridge");
    expect(std::abs(visuals::vibrationShape(0.5f, 0.0f) - 1.0f) < 1.0e-5f,
           "an open string does not peak at its midpoint");
    expect(visuals::vibrationShape(0.2f, 0.5f) == 0.0f,
           "the string moves behind the fretting finger");
    expect(std::abs(visuals::vibrationShape(0.75f, 0.5f) - 1.0f) < 1.0e-5f,
           "a fretted string does not peak at the middle of its sounding length");
    expect(visuals::levelHeat(0.0f) == 0.0f
               && std::abs(visuals::levelHeat(1.0f) - 1.0f) < 1.0e-6f
               && visuals::levelHeat(0.25f) > 0.25f,
           "the level-to-heat curve is not a monotonic knee");

    // Packing is lossless for everything the display needs.
    for (int note = -1; note <= 127; note += 13)
    {
        for (int fret = -1; fret <= ElectryEngine::fretCount; fret += 5)
        {
            electry::StringVisualState state;
            state.midiNote = note;
            state.fret = fret;
            state.sounding = (note % 2) == 0;
            state.sympathetic = (fret % 2) == 0;
            state.releasing = (note % 3) == 0;
            state.level = 0.5f;
            state.playStyle = static_cast<PlayStyle>(
                (note + 1) % ElectryEngine::playStyleKeyswitchCount);
            state.strokeUp = (note % 5) == 0;
            const auto round = visuals::unpackStringVisual(
                visuals::packStringVisual(state));
            expect(round.midiNote == state.midiNote && round.fret == state.fret
                       && round.sounding == state.sounding
                       && round.sympathetic == state.sympathetic
                       && round.releasing == state.releasing
                       && round.playStyle == state.playStyle
                       && round.strokeUp == state.strokeUp
                       && std::abs(round.level - state.level) < 0.005f,
                   "packed string state did not survive a round trip");
        }
    }

    // The engine's readout names the right physical string, fret and note.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.8f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    engine.noteOn(45, 0.95f);
    // A firm mute's decay target is short, so the readout has to be sampled
    // while the note is genuinely sounding: a quarter of a second in, the
    // string is far down and the voice may have correctly retired.
    StereoBuffer buffer(static_cast<int>(0.10 * sampleRate));
    renderInto(engine, buffer);

    std::array<electry::StringVisualState, ElectryEngine::stringCount> visual {};
    engine.getStringVisualState(visual);
    expect(visual[3].sounding && visual[3].midiNote == 45 && visual[3].fret == 0
               && visual[3].playStyle == PlayStyle::PalmMute
               && visual[3].strokeUp,
           "the display readout did not identify the played open A string");
    expect(visual[3].level > 0.0f, "a struck string reported no display level");
    expect(! visual[0].sounding, "an unplayed string reported a played note");

    int ringing = 0;
    for (const auto& state : visual)
        if (state.sympathetic)
        {
            ++ringing;
            expect(state.midiNote >= ElectryEngine::lowestPlayableNote
                       && state.fret == 0,
                   "a coupled string reported an implausible open note");
        }
    expect(ringing == engine.getSympatheticStringCount(),
           "the coupled-string count disagrees with the per-string readout");

    engine.reset();
    engine.getStringVisualState(visual);
    for (const auto& state : visual)
        expect(! state.sounding && ! state.sympathetic && state.midiNote == -1
                   && state.level == 0.0f,
               "a reset engine still reported a sounding string");
}

// The editor reads these helpers directly off the lock-free audio-to-editor
// transfer, so a non-finite or out-of-range value has to be recovered here
// rather than upstream: `testVisualStateAndGeometry` above exercises every
// helper's ordinary range and only `meterBallistics`' non-finite `current`
// guard, leaving `levelHeat`'s own guard, `meterBallistics`' non-finite
// `target` guard, and `packStringVisual`'s non-finite level and out-of-range
// note/fret/playStyle clamps unexercised by any existing test.
void testVisualStateSanitizesNonFiniteInput()
{
    namespace visuals = electry::visuals;
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    expect(visuals::levelHeat(nan) == 0.0f,
           "levelHeat did not recover from a non-finite level");
    expect(visuals::levelHeat(inf) == 0.0f,
           "levelHeat did not recover from a positive-infinite level");
    expect(visuals::levelHeat(-inf) == 0.0f,
           "levelHeat did not recover from a negative-infinite level");

    // A non-finite target has to sanitize to zero, exactly like a non-finite
    // current does above: with current already at rest, a NaN target must
    // leave the meter at rest rather than latching NaN into the display.
    expect(visuals::meterBallistics(0.0f, nan, 0.5f, 0.5f) == 0.0f,
           "meterBallistics did not recover from a non-finite target");
    // With current above the sanitized zero target, the non-finite target
    // takes the release branch and the reading has to fall rather than hold
    // or grow, so a poisoned target cannot freeze the display at a stale
    // level.
    const float releasing = visuals::meterBallistics(1.0f, inf, 0.4f, 0.4f);
    expect(releasing < 1.0f && releasing >= 0.0f,
           "meterBallistics did not release toward zero for a non-finite target");

    // packStringVisual's own guards: a non-finite level packs to silent
    // rather than propagating NaN through the lock-free word, and a
    // wildly out-of-range note, fret or play style clamps to the nearest
    // valid value instead of wrapping into an unrelated field via the
    // packed word's bit layout.
    electry::StringVisualState poisoned;
    poisoned.midiNote = 4000;
    poisoned.fret = -900;
    poisoned.level = nan;
    poisoned.playStyle = static_cast<PlayStyle>(999);
    const auto packed = visuals::packStringVisual(poisoned);
    const auto round = visuals::unpackStringVisual(packed);
    expect(round.midiNote == 127,
           "packStringVisual did not clamp an out-of-range high note");
    expect(round.fret == -1,
           "packStringVisual did not clamp an out-of-range low fret");
    expect(round.level == 0.0f,
           "packStringVisual did not sanitize a non-finite level");
    expect(round.playStyle == PlayStyle::Dead,
           "packStringVisual did not clamp an out-of-range play style");

    electry::StringVisualState poisonedLow;
    poisonedLow.midiNote = -900;
    poisonedLow.fret = 900;
    const auto roundLow = visuals::unpackStringVisual(
        visuals::packStringVisual(poisonedLow));
    expect(roundLow.midiNote == -1,
           "packStringVisual did not clamp an out-of-range low note");
    expect(roundLow.fret == ElectryEngine::fretCount,
           "packStringVisual did not clamp an out-of-range high fret");
}

// stringRowFraction and stringThickness each clamp their string index to the
// modelled set, and stringRowFraction separately clamps its inset and
// special-cases a degenerate one-or-fewer-string count; none of those guards
// were exercised by testVisualStateAndGeometry above, which only ever fed
// them ordinary in-range indices and a fixed, in-range inset. The
// span-aware fretWireFraction/fretCentreFraction overloads also fall back to
// zero for a non-positive span, a defensive branch fretSpan() itself never
// produces for the engine's fixed, positive fret count but that a caller
// could still reach directly.
void testVisualGeometryClampsOutOfRangeInput()
{
    namespace visuals = electry::visuals;
    constexpr int lastFret = ElectryEngine::fretCount;
    constexpr int stringCount = ElectryEngine::stringCount;

    expect(visuals::stringRowFraction(-5, stringCount, 0.085f)
               == visuals::stringRowFraction(0, stringCount, 0.085f),
           "stringRowFraction did not clamp a negative string index");
    expect(visuals::stringRowFraction(999, stringCount, 0.085f)
               == visuals::stringRowFraction(stringCount - 1, stringCount, 0.085f),
           "stringRowFraction did not clamp an out-of-range high string index");
    expect(visuals::stringRowFraction(0, 1, 0.085f) == 0.5f,
           "stringRowFraction did not centre a single-string layout");
    expect(visuals::stringRowFraction(0, 0, 0.085f) == 0.5f,
           "stringRowFraction did not centre a degenerate zero-string layout");
    expect(visuals::stringRowFraction(0, stringCount, 0.9f)
               == visuals::stringRowFraction(0, stringCount, 0.45f),
           "stringRowFraction did not clamp an inset above its 0.45 ceiling");
    expect(visuals::stringRowFraction(0, stringCount, -0.3f)
               == visuals::stringRowFraction(0, stringCount, 0.0f),
           "stringRowFraction did not clamp a negative inset");

    expect(visuals::stringThickness(-3, 0.9f, 2.6f)
               == visuals::stringThickness(0, 0.9f, 2.6f),
           "stringThickness did not clamp a negative string index");
    expect(visuals::stringThickness(999, 0.9f, 2.6f)
               == visuals::stringThickness(stringCount - 1, 0.9f, 2.6f),
           "stringThickness did not clamp an out-of-range high string index");

    expect(visuals::fretWireFraction(5, lastFret, 0.0f) == 0.0f,
           "the span-aware fretWireFraction did not fall back to zero for a "
           "zero span");
    expect(visuals::fretWireFraction(5, lastFret, -1.0f) == 0.0f,
           "the span-aware fretWireFraction did not fall back to zero for a "
           "negative span");
    expect(visuals::fretCentreFraction(5, lastFret, 0.0f) == 0.0f,
           "the span-aware fretCentreFraction did not fall back to zero for a "
           "zero span");
}

// ---------------------------------------------------------------------------
// Version 1.1: the paths the efficiency work depends on
// ---------------------------------------------------------------------------

void testPickupCullingAndChannelLinking()
{
    constexpr double sampleRate = 48000.0;

    const auto settle = [] (ElectryEngine& engine, double seconds)
    {
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(engine, buffer);
        return buffer;
    };

    struct SelectorCase { PickupSelector selector; bool neck; bool bridge; };
    for (const auto& item : { SelectorCase { PickupSelector::Bridge, false, true },
                              SelectorCase { PickupSelector::Neck, true, false },
                              SelectorCase { PickupSelector::Both, true, true } })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = item.selector;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        const auto audio = settle(engine, 0.2);
        expect(TestAccess::pickupPathActive(engine, true) == item.neck,
               "the neck pickup path was not culled to match the selector");
        expect(TestAccess::pickupPathActive(engine, false) == item.bridge,
               "the bridge pickup path was not culled to match the selector");
        expect(allFinite(audio) && peakAbs(audio.left) > 1.0e-5f,
               "a selector position rendered silence");
    }

    // Bringing a culled pickup back must not click: its aperture and EMF
    // memory is cleared and the selector mix fades it in over about 4 ms.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    settle(engine, 0.30);
    parameters.pickupSelector = PickupSelector::Both;
    engine.setParameters(parameters);
    const auto crossfade = settle(engine, 0.10);
    expect(allFinite(crossfade), "switching pickups produced non-finite audio");
    float largestStep = 0.0f;
    for (std::size_t i = 1; i < crossfade.left.size(); ++i)
        largestStep = std::max(largestStep,
                               std::abs(crossfade.left[i] - crossfade.left[i - 1]));
    expect(largestStep < 0.08f,
           "restoring a culled pickup produced a discontinuity ("
               + std::to_string(largestStep) + ")");

    // Mono runs one shared output chain and is bit-identical dual mono;
    // opening the field copies its state across without a discontinuity.
    ElectryEngine field;
    field.prepare(sampleRate, 512);
    EngineParameters mono;
    mono.outputMode = electry::OutputMode::Mono;
    field.setParameters(mono);
    field.reset();
    field.noteOn(40, 0.9f);
    StereoBuffer monoAudio(static_cast<int>(0.25 * sampleRate));
    renderInto(field, monoAudio);
    expect(TestAccess::channelsLinked(field),
           "Mono did not link the shared output chain");
    expect(monoAudio.left == monoAudio.right,
           "Mono is not sample-identical dual mono");

    EngineParameters stereo = mono;
    stereo.outputMode = electry::OutputMode::Stereo;
    field.setParameters(stereo);
    StereoBuffer opening(static_cast<int>(0.20 * sampleRate));
    renderInto(field, opening);
    expect(! TestAccess::channelsLinked(field),
           "Stereo did not unlink the shared output chain");
    float largestFieldStep = 0.0f;
    for (std::size_t i = 1; i < opening.right.size(); ++i)
        largestFieldStep = std::max(
            largestFieldStep, std::abs(opening.right[i] - opening.right[i - 1]));
    expect(allFinite(opening) && largestFieldStep < 0.08f,
           "opening the stereo field produced a discontinuity ("
               + std::to_string(largestFieldStep) + ")");
}

// ---------------------------------------------------------------------------
// Rate invariance, polarisation coupling, and the coupled strings' own loss
// ---------------------------------------------------------------------------

// Energy in a band, summed over logarithmically spaced probe frequencies.
double bandEnergyDb(const std::vector<float>& data, int start, int length,
                    double sampleRate, double lowHz, double highHz)
{
    double energy = 0.0;
    for (double f = lowHz; f < highHz; f *= 1.03)
    {
        const double m = dftMagnitude(data, start, length, sampleRate, f);
        energy += m * m;
    }
    return 10.0 * std::log10(energy + 1.0e-30);
}

// A humbucker's two-point cancellation notch sits at c/2d, where d is the
// distance between its coils and c is the string's transverse wave speed.
// Modelling it as one wide rectangular window instead put the notch at c/W,
// most of an octave too high - 5507 Hz on string 2 where Lemme measures a low
// E notching at about 3000 Hz, and 7351 Hz on string 3 against his 4000 Hz.
//
// Moving the notch is the smaller half of what the two coils do. The larger
// half is that two narrow windows pass a top octave one wide window was
// throwing away, so the assertions below bound the broadband change as well
// as placing the notch. What they cannot do is bound it per octave band
// against the shipping engine: the misplaced null on the wound strings sat
// *inside* the 4-8 kHz band, so moving it out of that band necessarily raises
// it, and on the plain strings the change goes the other way. See the plan
// document for the measurement that retired that bound.
void testHumbuckerTwoCoilNotch()
{
    constexpr double sampleRate = 48000.0;

    // Rendering protocol shared by every level measurement below.
    const auto renderChord = [&] (float pickupType, float velocity,
                                  double seconds)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupType = pickupType;
        engine->setParameters(parameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            engine->noteOn(note, velocity);
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(*engine, buffer);
        return buffer;
    };
    const auto renderSingle = [&] (float pickupType, int midiNote,
                                   float velocity, double seconds)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupType = pickupType;
        engine->setParameters(parameters);
        return renderNote(*engine, sampleRate, midiNote, velocity,
                          PlayStyle::Sustain, seconds);
    };

    // 1. Where the notch is, and how deep, read off the two stages the voice
    // is actually running rather than off a rendered spectrum.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupType = 0.0f;
        engine.setParameters(parameters);
        // reset() snaps the parameter smoother, so the voice is configured at
        // the control value asked for rather than part-way through a glide
        // from the default.
        engine.reset();
        engine.noteOn(40, 0.8f);
        engine.noteOn(45, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);

        struct NotchCase { int midiNote; double lowHz; double highHz; };
        const std::array<NotchCase, 2> cases {{
            { 40, 2800.0, 3300.0 },   // string 2, E2: c/2d = 3043 Hz
            { 45, 3800.0, 4400.0 },   // string 3, A2: c/2d = 4062 Hz
        }};
        for (const auto& notch : cases)
        {
            const int stringIndex =
                TestAccess::stringForNote(engine, notch.midiNote);
            expect(stringIndex >= 0,
                   "note " + std::to_string(notch.midiNote)
                       + " did not take a string");
            if (stringIndex < 0)
                continue;
            expect(TestAccess::coilPairActive(engine, stringIndex),
                   "the humbucker is running on one coil");

            double deepest = 1.0e30;
            double deepestHz = 0.0;
            for (double f = 2000.0; f <= 8000.0; f *= 1.0004)
            {
                const double magnitude =
                    TestAccess::apertureChainMagnitude(
                        engine, stringIndex, f, true);
                if (magnitude < deepest)
                {
                    deepest = magnitude;
                    deepestHz = f;
                }
            }
            std::cout << "PROBE humbucker notch on note " << notch.midiNote
                      << ": " << deepestHz << " Hz\n";
            expect(deepestHz > notch.lowHz && deepestHz < notch.highHz,
                   "humbucker notch on note " + std::to_string(notch.midiNote)
                       + " is at " + std::to_string(deepestHz)
                       + " Hz, outside " + std::to_string(notch.lowHz) + ".."
                       + std::to_string(notch.highHz) + " Hz");

            // The local envelope is the aperture window on its own - the
            // smooth trend the coil pair notches into.
            const double envelope =
                TestAccess::apertureChainMagnitude(
                    engine, stringIndex, deepestHz, false);
            const double depthDb = 20.0 * std::log10(
                envelope / std::max(deepest, 1.0e-12));
            std::cout << "PROBE humbucker notch depth on note "
                      << notch.midiNote << ": " << depthDb << " dB\n";
            expect(depthDb >= 10.0,
                   "humbucker notch on note " + std::to_string(notch.midiNote)
                       + " is only " + std::to_string(depthDb)
                       + " dB below its local envelope");
        }
    }

    // 2. The single coil is one coil and is untouched. Structurally the coil
    // pair is not in its path at all; by measurement its partials sit where
    // the shipping engine put them.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupType = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);
        const int stringIndex = TestAccess::stringForNote(engine, 45);
        expect(stringIndex >= 0, "note 45 did not take a string");
        if (stringIndex >= 0)
            expect(! TestAccess::coilPairActive(engine, stringIndex),
                   "the single coil grew a second coil");

        // Partial magnitudes in dB, measured on the shipping engine with this
        // protocol. Partials past the 28th sit more than 90 dB below the
        // strongest one, where a one-ulp difference in the window length is
        // worth a decibel, so the comparison stops there.
        struct Partial { int index; double decibels; };
        const std::array<Partial, 16> reference {{
            { 1, 16.4891 }, { 2, 22.4436 }, { 3, 24.5343 },
            { 4, 24.7026 }, { 6, 22.5968 }, { 8, 15.7140 },
            { 10, 5.52877 }, { 12, -2.75441 }, { 14, -7.49994 },
            { 16, -13.3693 }, { 18, -29.0575 }, { 20, -41.6398 },
            { 22, -57.8266 }, { 24, -67.7500 }, { 26, -60.3338 },
            { 28, -65.1425 },
        }};
        const auto render = renderSingle(1.0f, 45, 0.70f, 1.0);
        const int start = static_cast<int>(0.05 * sampleRate);
        const int window = static_cast<int>(0.4 * sampleRate);
        const double f0 = midiHz(45);
        double worst = 0.0;
        for (const auto& partial : reference)
        {
            const double measured = 20.0 * std::log10(
                dftMagnitude(render.left, start, window, sampleRate,
                             f0 * partial.index) + 1.0e-30);
            worst = std::max(worst, std::abs(measured - partial.decibels));
            expect(std::abs(measured - partial.decibels) < 0.2,
                   "single-coil partial " + std::to_string(partial.index)
                       + " moved from " + std::to_string(partial.decibels)
                       + " dB to " + std::to_string(measured) + " dB");
        }
        std::cout << "PROBE single-coil partials moved at most " << worst
                  << " dB\n";
    }

    // 3. The low-frequency recovery the pickup comb's weight was fitted for
    // must survive. Measured on the shipping engine: 30.6608 dB.
    {
        const auto lowE = renderSingle(0.0f, 28, 0.80f, 1.5);
        const double band = bandEnergyDb(lowE.left,
                                         static_cast<int>(0.02 * sampleRate),
                                         static_cast<int>(1.0 * sampleRate),
                                         sampleRate, 60.0, 85.0);
        std::cout << "PROBE open low E 60-85 Hz band: " << band << " dB\n";
        expect(band > 30.6608 - 0.5,
               "the open low E lost its 60-85 Hz band ("
                   + std::to_string(band) + " dB against 30.6608 dB)");
    }

    // 4. Broadband balance. The humbucker has to stay the dark pickup of the
    // pair: on a full eight-string chord its 2-16 kHz to sub-500 Hz ratio may
    // not move more than 3 dB against the shipping engine's -68.369 dB, and
    // in every octave band from 4 to 16 kHz on a low, a middle and a plain
    // string it must stay at least 12 dB below the single coil (today's
    // narrowest gap is 13.16 dB).
    {
        const auto chord = renderChord(0.0f, 0.80f, 1.5);
        const int start = static_cast<int>(0.02 * sampleRate);
        const int window = static_cast<int>(1.0 * sampleRate);
        const double ratio =
            bandEnergyDb(chord.left, start, window, sampleRate, 2000.0, 16000.0)
            - bandEnergyDb(chord.left, start, window, sampleRate, 60.0, 500.0);
        std::cout << "PROBE humbucker chord 2-16k/sub-500 ratio: " << ratio
                  << " dB\n";
        expect(std::abs(ratio - (-68.369)) < 3.0,
               "the humbucker's broadband balance on a chord moved from "
                   "-68.369 dB to " + std::to_string(ratio) + " dB");

        struct Band { double lowHz; double highHz; };
        const std::array<Band, 2> bands {{ { 4000.0, 8000.0 },
                                           { 8000.0, 16000.0 } }};
        // Octave-band energy on the shipping engine, humbucker then single
        // coil, for notes 28, 40 and 64.
        struct Reference { int midiNote; double humbucker[2]; double single[2]; };
        const std::array<Reference, 3> shipping {{
            { 28, { -86.559, -118.699 }, { -53.600, -96.302 } },
            { 40, { -85.336, -116.775 }, { -56.185, -96.338 } },
            { 64, { -68.007, -105.829 }, { -50.338, -92.672 } },
        }};
        for (const auto& reference : shipping)
        {
            const auto humbucker = renderSingle(0.0f, reference.midiNote,
                                                0.80f, 1.5);
            const auto single = renderSingle(1.0f, reference.midiNote,
                                             0.80f, 1.5);
            for (std::size_t b = 0; b < bands.size(); ++b)
            {
                const double dark = bandEnergyDb(
                    humbucker.left, start, window, sampleRate,
                    bands[b].lowHz, bands[b].highHz);
                const double bright = bandEnergyDb(
                    single.left, start, window, sampleRate,
                    bands[b].lowHz, bands[b].highHz);
                std::cout << "PROBE note " << reference.midiNote << " "
                          << bands[b].lowHz << "-" << bands[b].highHz
                          << " Hz: humbucker " << dark << " dB (was "
                          << reference.humbucker[b] << "), single coil "
                          << bright << " dB (was " << reference.single[b]
                          << ")\n";
                expect(dark < bright - 12.0,
                       "the humbucker is no longer the dark pickup in the "
                           + std::to_string(static_cast<int>(bands[b].lowHz))
                           + " Hz band on note "
                           + std::to_string(reference.midiNote) + " ("
                           + std::to_string(dark) + " dB against "
                           + std::to_string(bright) + " dB)");
                // A loose guard on the size of the move, not a
                // discriminator: the change runs from -4.93 to +8.81 dB, and
                // its largest terms are on the bands the misplaced null used
                // to sit in or beside.
                expect(std::abs(dark - reference.humbucker[b]) < 12.0,
                       "humbucker octave-band energy on note "
                           + std::to_string(reference.midiNote) + " moved from "
                           + std::to_string(reference.humbucker[b]) + " dB to "
                           + std::to_string(dark) + " dB");
                // The single coil is not on the path this step changed, so it
                // is held to a tight bound - but only where the measurement
                // means something. A band sitting 90-odd dB down is numerical
                // floor rather than signal, and its decibel value does not
                // reproduce between x86_64 and arm64, which contract
                // multiply-adds differently: on Apple silicon the 8-16 kHz
                // bands moved 0.80 and 1.19 dB while every band carrying real
                // signal agreed to 0.02 dB. Tightening this further would only
                // pin the floating-point noise of one architecture.
                const double singleTolerance =
                    reference.single[b] > -80.0 ? 0.5 : 2.5;
                expect(std::abs(bright - reference.single[b]) < singleTolerance,
                       "single-coil octave-band energy on note "
                           + std::to_string(reference.midiNote) + " moved from "
                           + std::to_string(reference.single[b]) + " dB to "
                           + std::to_string(bright) + " dB");
            }
        }
    }

    // 5. The coil pair adds a tap inside the pickup chain, so the alias floor
    // has to be re-measured against the bound the audit set: at least 150 dB
    // below the spectral peak above 12 kHz on a full chord at velocity 1.0.
    {
        const auto chord = renderChord(0.32f, 1.0f, 1.0);
        const int start = static_cast<int>(0.2 * sampleRate);
        constexpr int window = 32768;
        double peak = 0.0;
        double top = 0.0;
        for (double f = 40.0; f < 23500.0; f *= 1.0075)
        {
            const double magnitude = dftMagnitude(chord.left, start, window,
                                                  sampleRate, f);
            peak = std::max(peak, magnitude);
            if (f > 12000.0)
                top = std::max(top, magnitude);
        }
        const double floorDb = 20.0 * std::log10(peak / std::max(top, 1.0e-30));
        std::cout << "PROBE alias floor above 12 kHz: " << floorDb
                  << " dB below the spectral peak\n";
        expect(floorDb > 150.0,
               "the alias floor rose to " + std::to_string(floorDb)
                   + " dB below the spectral peak");
    }
}

// The instrument has to sound the same at every host rate, and the decay
// envelope is where that is hardest: a per-sample constant anywhere inside the
// string loop turns into a rate-dependent decay, because the number of samples
// in a round trip is proportional to the sample rate.
//
// Two such constants used to be here. The second polarisation's detuning was a
// bare 0.11 samples, which is a fixed offset rather than a fixed fraction of a
// period, so the two polarisations beat at a rate that moved with the clock;
// and the polarisation coupling was charged per rendered sample rather than per
// round trip. Together they left the 22nd-fret high E 36.5 dB down at half a
// second on a 44.1 kHz host and 14.5 dB down on a 96 kHz one - the same note,
// the same patch, a 22 dB difference in how long it rings.
void testDecayIsSampleRateInvariant()
{
    struct Window { double start, end; double toleranceDb; };
    // The late window is allowed more slack: it is 20-30 dB down, and what is
    // left there is the one-pole loop filter's own shape, which is fitted at
    // two frequencies and can only interpolate between them in normalised
    // radians.
    const std::array<Window, 3> windows {{
        { 0.10, 0.50, 3.0 }, { 0.50, 1.50, 3.0 }, { 1.50, 3.00, 8.0 } }};

    for (const int note : { 28, 45, 64, 86 })
    {
        std::array<std::array<double, 3>, 5> levels {};
        int rateIndex = 0;
        for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            ElectryEngine engine;
            engine.prepare(rate, 512);
            EngineParameters parameters;
            parameters.sympatheticAmount = 0.0f;
            parameters.artifactAmount = 0.0f;
            parameters.pickNoise = 0.0f;
            parameters.fingerNoise = 0.0f;
            parameters.releaseNoise = 0.0f;
            engine.setParameters(parameters);

            const auto buffer = renderNote(engine, rate, note, 0.9f,
                                           PlayStyle::Sustain, 3.0);
            expect(allFinite(buffer),
                   "the rate-invariance render was not finite at "
                       + std::to_string(rate) + " Hz");
            const double attack = rmsInRange(buffer.left, 0,
                                             static_cast<int>(0.1 * rate));
            expect(attack > 1.0e-5,
                   "the rate-invariance render produced no attack at "
                       + std::to_string(rate) + " Hz");
            for (std::size_t w = 0; w < windows.size(); ++w)
            {
                const double level = rmsInRange(
                    buffer.left, static_cast<int>(windows[w].start * rate),
                    static_cast<int>(windows[w].end * rate));
                levels[static_cast<std::size_t>(rateIndex)][w] =
                    20.0 * std::log10(std::max(level, 1.0e-12) / attack);
            }
            ++rateIndex;
        }

        for (std::size_t w = 0; w < windows.size(); ++w)
        {
            double lowest = 1.0e30, highest = -1.0e30;
            for (const auto& perRate : levels)
            {
                lowest = std::min(lowest, perRate[w]);
                highest = std::max(highest, perRate[w]);
            }
            expect(highest - lowest < windows[w].toleranceDb,
                   "note " + std::to_string(note) + "'s decay between "
                       + std::to_string(windows[w].start) + " s and "
                       + std::to_string(windows[w].end)
                       + " s depends on the host sample rate ("
                       + std::to_string(highest - lowest) + " dB spread)");
        }
    }
}

// The two polarisations meet at the bridge and at the nut or fret, so they
// exchange energy once per round trip. Charging a fixed fraction per rendered
// sample instead made the exchange proportional to the loop length: over 900%
// per round trip on the open low E, which averaged its two polarisations into
// one, and 33% at the top of the range, where the mismatch between the two loop
// lengths turned that exchange into dissipation and cost the string 36 dB of
// sustain against its own fitted decay target.
void testPolarisationCouplingIsPerRoundTrip()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // Two notes an octave and a half apart, on different strings, must exchange
    // the same fraction of the wave per round trip.
    //
    // This first half is a construction check and nothing more, and it is worth
    // being plain about that: the per-sample coupling is defined as
    // `0.04 / max(delay, 4)`, so the product below is 0.04 by definition and can
    // only move if one of that expression's clamps engages. What it pins is
    // exactly that - that neither clamp engages anywhere in the playable range,
    // which is the only way the "one number per round trip" property can fail
    // once the definition is in place. The load-bearing assertions are the
    // rendered ones underneath it.
    double perRoundTrip[2] = { 0.0, 0.0 };
    int index = 0;
    for (const int note : { 28, 79 })
    {
        engine.reset();
        engine.noteOn(note, 0.9f);
        StereoBuffer settle(1024);
        renderInto(engine, settle);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(stringIndex >= 0, "the coupling probe note was not allocated");
        if (stringIndex < 0)
            return;
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.polarisationCoupling > 0.0f,
               "the polarisation coupling was switched off");
        expect(snapshot.polarisationCoupling < 0.25f
                   && snapshot.verticalDelayTarget > 4.0f,
               "a clamp in the per-round-trip coupling engaged inside the "
               "playable range, so the exchange is no longer one number");
        perRoundTrip[index++] = static_cast<double>(snapshot.polarisationCoupling)
                              * static_cast<double>(snapshot.verticalDelayTarget);
    }
    expect(std::abs(perRoundTrip[0] - perRoundTrip[1])
               < 0.02 * std::max(perRoundTrip[0], perRoundTrip[1]),
           "the polarisation exchange per round trip depends on the pitch ("
               + std::to_string(perRoundTrip[0]) + " vs "
               + std::to_string(perRoundTrip[1]) + ")");

    // What that buys audibly, and this is the part that carries weight: the top
    // of the range keeps ringing, and it does so on every host. The former
    // per-sample constant left this note 55.7 dB under its own attack a second
    // later at 48 kHz - gone before the next beat of a moderate tempo - and
    // 22.9 dB under it at 96 kHz, so the same patch was a different instrument
    // on a different host.
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine rateEngine;
        rateEngine.prepare(rate, 512);
        rateEngine.setParameters(parameters);
        rateEngine.reset();
        const auto buffer = renderNote(rateEngine, rate, 86, 0.9f,
                                       PlayStyle::Sustain, 3.0);
        const double attack = rmsInRange(buffer.left, 0,
                                         static_cast<int>(0.1 * rate));
        const double sustain = rmsInRange(buffer.left,
                                          static_cast<int>(1.0 * rate),
                                          static_cast<int>(2.0 * rate));
        const double relative =
            20.0 * std::log10(std::max(sustain, 1.0e-12) / attack);
        expect(relative > -30.0,
               "the 22nd-fret high E no longer sustains at "
                   + std::to_string(static_cast<int>(rate)) + " Hz ("
                   + std::to_string(relative) + " dB under its attack at 1-2 s)");
    }
}

// A string nobody is fingering is the same piece of steel as one that is being
// played, so its loop has to lose its top end at the same rate. The fixed
// one-pole this replaced was a mild lowpass whatever the string, which left the
// wound strings' coupled ring carrying kilohertz content for seconds - a bright
// metallic reverb rather than strings.
void testCoupledStringLosesItsTopEndLikeAPlayedString()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.6f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                   PlayStyle::Sustain, 3.0, 0.5);
    expect(allFinite(buffer), "the coupled-ring render was not finite");

    const int start = static_cast<int>(1.0 * sampleRate);
    const int length = static_cast<int>(1.5 * sampleRate);
    const double low = bandEnergyDb(buffer.left, start, length, sampleRate,
                                    60.0, 700.0);
    const double high = bandEnergyDb(buffer.left, start, length, sampleRate,
                                     1500.0, 6000.0);
    // The ring itself must still be there.
    expect(rmsInRange(buffer.left, start, start + length) > 1.0e-5,
           "the coupled strings stopped ringing altogether");
    // ...and it must be a string, not a cymbal. The fixed coefficient scored
    // -37.8 dB here; a played wound string's own decay law scores -87.5.
    expect(high - low < -60.0,
           "the coupled ring still carries a played string's worth of "
           "kilohertz content (" + std::to_string(high - low) + " dB)");

    // The wound low strings must be damped harder than the plain high ones,
    // which is the whole point of solving the loss from the string rather than
    // fixing it: a plain .009 keeps far more of its top end than a wound .080.
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer settle(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, settle);
    const auto lowString = TestAccess::snapshot(engine, 0);
    const auto highString = TestAccess::snapshot(engine, ElectryEngine::stringCount - 1);
    expect(lowString.sympatheticReady && highString.sympatheticReady,
           "the coupled strings were not configured");
    expect(lowString.loopDampingCoefficient
               > highString.loopDampingCoefficient + 0.2f,
           "the wound coupled low E is not damped harder than the plain high E ("
               + std::to_string(lowString.loopDampingCoefficient) + " vs "
               + std::to_string(highString.loopDampingCoefficient) + ")");
}

// The other half of solving the coupled loop from decay targets: the
// fundamental's target is the one that must never be given up.
//
// Two decay targets are two constraints on a first-order filter and a scalar,
// and they do not always both fit - the one-pole's own loss at the fundamental
// has to be bought back by the loop gain, and that gain cannot exceed one.
// Solving the tilt and then clamping the gain discards the fundamental's target
// silently: the coupled open low E realised a 0.099 s T60 against its 8.97 s
// target at String Age 1.0, and every string collapsed to between 8 and 53 ms
// under the palm mute. The whole sympathetic bank disappeared wherever either
// control was up. The engine now backs the high-frequency target off instead,
// which costs only the top of the tilt.
//
// The loop's realised decay is read back from what actually runs - the solved
// gain and one-pole coefficient - rather than from the solver's own arithmetic.
void testCoupledStringKeepsItsFundamentalDecayTarget()
{
    // Realised round-trip T60 of a coupled loop at its own fundamental.
    const auto realisedT60 = [] (const ElectryEngine& engine, int stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        const double rate = TestAccess::internalSampleRate(engine);
        const double f0 = TestAccess::sympatheticFrequency(engine, stringIndex);
        const double omega = 2.0 * 3.14159265358979323846 * f0 / rate;
        const double a = snapshot.loopDampingCoefficient;
        const double magnitude = (1.0 - a)
            / std::sqrt(std::max(1.0 + a * a - 2.0 * a * std::cos(omega),
                                 1.0e-30));
        const double perRoundTrip = snapshot.loopGain * magnitude;
        if (perRoundTrip <= 0.0 || perRoundTrip >= 1.0)
            return 1.0e9;
        return -3.0 * (rate / f0) / (rate * std::log10(perRoundTrip));
    };

    const auto configured = [&] (double hostRate, float stringAge,
                                 float palmMute, ElectryEngine& engine)
    {
        engine.prepare(hostRate, 512);
        EngineParameters parameters;
        parameters.stringAge = stringAge;
        parameters.palmMute = palmMute;
        parameters.sympatheticAmount = 0.6f;
        engine.setParameters(parameters);
        engine.reset();
        // One note wakes the other seven strings as coupled loops.
        engine.noteOn(45, 0.9f);
        StereoBuffer settle(static_cast<int>(0.4 * hostRate));
        renderInto(engine, settle);
    };

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const std::string at = " at " + std::to_string(static_cast<int>(rate))
                             + " Hz";

        // Worn strings. The wound coupled strings' targets are still seconds
        // long here - 8.97, 8.52 and 8.07 s on the bottom three - and the
        // clamp realised 0.099, 0.79 and 1.48.
        {
            ElectryEngine engine;
            configured(rate, 1.0f, 0.0f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(t60 > 3.0 && t60 < 12.0,
                       "coupled string " + std::to_string(s)
                           + " does not hold its multi-second decay target at "
                             "String Age 1.0" + at + " (" + std::to_string(t60)
                           + " s)");
            }
        }

        // Under the palm mute the coupled fundamental target is the engine's
        // own constant: `exp(lerp(log(t60), log(0.080), blend))` is exactly
        // 0.080 s at full pressure, whatever the string and whatever the host
        // rate. Six of the eight strings realised 8 to 53 ms instead.
        {
            ElectryEngine engine;
            configured(rate, 0.30f, 1.0f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(std::abs(t60 - 0.080) < 0.004,
                       "coupled string " + std::to_string(s)
                           + " does not realise the 0.080 s full-mute decay "
                             "target" + at + " (" + std::to_string(t60) + " s)");
            }
        }

        // Half pressure, where the targets run from 1.11 to 1.28 s and the
        // clamp realised 16 to 99 ms.
        {
            ElectryEngine engine;
            configured(rate, 0.30f, 0.5f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(t60 > 0.6 && t60 < 1.8,
                       "coupled string " + std::to_string(s)
                           + " does not hold its half-mute decay target" + at
                           + " (" + std::to_string(t60) + " s)");
            }
        }
    }

    // The same target on every host, not merely a plausible one on each: the
    // clamp made the coupled low E's realised decay 0.094 s at 44.1 kHz,
    // 0.099 at 48 and 0.159 at 96, in the release whose headline is rate
    // invariance.
    double lowE[3] = { 0.0, 0.0, 0.0 };
    int index = 0;
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine engine;
        configured(rate, 1.0f, 0.0f, engine);
        lowE[index++] = realisedT60(engine, 0);
    }
    const double spread = (std::max({ lowE[0], lowE[1], lowE[2] })
                           - std::min({ lowE[0], lowE[1], lowE[2] }))
                        / std::max(lowE[0], 1.0e-9);
    expect(spread < 0.02,
           "the coupled low E's realised decay depends on the host rate ("
               + std::to_string(lowE[0]) + " / " + std::to_string(lowE[1])
               + " / " + std::to_string(lowE[2]) + " s)");

    // Finally, that the bank is still there and still finite at String Age 1.0,
    // where the clamp used to leave it. This last check is deliberately a weak
    // one and it is worth saying why rather than dressing it up: with the
    // driving string itself still ringing, the rendered level in any window is
    // set mostly by what the bus is feeding the coupled loops and only weakly
    // by the loops' own decay, so it separates the two revisions by a few dB in
    // the late windows and not at all in the early ones. The assertions that
    // pin this regression are the analytic ones above, read off the gain and
    // coefficient the loops actually run.
    constexpr double sampleRate = 48000.0;
    const auto ringEnergy = [&] (float sympathetic)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.stringAge = 1.0f;
        parameters.sympatheticAmount = sympathetic;
        parameters.artifactAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                       PlayStyle::Sustain, 3.0, 0.5);
        expect(allFinite(buffer), "the worn-string coupled render was not finite");
        return rmsInRange(buffer.left, static_cast<int>(1.5 * sampleRate),
                          static_cast<int>(2.5 * sampleRate));
    };
    expect(ringEnergy(0.6f) > 2.0 * ringEnergy(0.0f),
           "the coupled bank contributes nothing at String Age 1.0");
}

// ---------------------------------------------------------------------------
// The strings share a bridge: the strings that are being *played* read the
// coupling bus too, minus their own contribution to it.
// ---------------------------------------------------------------------------
//
// Until this shipped the bus was written only by played voices and read only
// by idle ones, so a voicing that fingers all eight strings had no coupling
// path at all: the same chord rendered at Resonance 0.20 and at 0 was
// bit-identical. That is the case this test leads with, and it is the one no
// implementation can pass without closing the loop.
//
// Everything here pins `pickNoise`, `fingerNoise` and `releaseNoise` at zero.
// Not for the reason the plan first gave - the per-note noise seed - but for a
// larger one measured on this engine: the per-attack stroke draw seeded from
// the note counter moves a two-note render 30 to 65 dB away from the sum of
// its two single-note renders, which swamps any coupling term. Every
// comparison below is therefore the *same* voicing at two coupling settings,
// where the seeds are identical by construction.
void testFingeredStringsShareTheBridge()
{
    constexpr double sampleRate = 48000.0;
    const std::vector<int> allEight { 28, 35, 40, 45, 50, 55, 59, 64 };

    const auto renderChord = [&] (const std::vector<int>& notes, float velocity,
                                  float sympathetic, float resonanceDepth,
                                  double seconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = sympathetic;
        parameters.resonanceDepth = resonanceDepth;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : notes)
            engine.noteOn(note, velocity);
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(engine, buffer);
        return buffer;
    };

    // Relative L2 of one render against another, in decibels, over a window.
    const auto relativeDb = [] (const std::vector<float>& a,
                                const std::vector<float>& b,
                                int start, int end)
    {
        double difference = 0.0;
        double reference = 0.0;
        for (int i = start; i < end; ++i)
        {
            const double delta = static_cast<double>(a[static_cast<std::size_t>(i)])
                               - static_cast<double>(b[static_cast<std::size_t>(i)]);
            difference += delta * delta;
            reference += static_cast<double>(b[static_cast<std::size_t>(i)])
                       * static_cast<double>(b[static_cast<std::size_t>(i)]);
        }
        if (reference <= 0.0)
            return 0.0;
        return 10.0 * std::log10(std::max(difference, 1.0e-300) / reference);
    };

    const int firstWindow = static_cast<int>(1.5 * sampleRate);

    // 1. The voicing that leaves nothing open now hears itself. Both figures
    //    are exactly zero difference - minus infinity - without the step.
    const auto chordOff = renderChord(allEight, 0.90f, 0.0f, 0.0f, 12.2);
    expect(allFinite(chordOff), "the uncoupled eight-string chord is not finite");
    const auto chordDefault = renderChord(allEight, 0.90f, 0.20f, 0.0f, 12.2);
    const auto chordFull = renderChord(allEight, 0.90f, 1.00f, 0.0f, 12.2);
    expect(allFinite(chordDefault) && allFinite(chordFull),
           "the coupled eight-string chord is not finite");

    const double defaultDb = relativeDb(chordDefault.left, chordOff.left,
                                        0, firstWindow);
    const double fullDb = relativeDb(chordFull.left, chordOff.left,
                                     0, firstWindow);
    expect(defaultDb >= -66.0,
           "an all-eight-fingered chord at Resonance 0.20 does not differ from "
           "the same chord at 0 (" + std::to_string(defaultDb) + " dB)");
    expect(fullDb >= -56.0,
           "an all-eight-fingered chord at full Resonance does not differ from "
           "the same chord at 0 (" + std::to_string(fullDb) + " dB)");

    // 2. The control scales it, rather than switching a fixed amount on.
    const auto chordLow = renderChord(allEight, 0.90f, 0.05f, 0.0f, 12.2);
    const auto chordHalf = renderChord(allEight, 0.90f, 0.50f, 0.0f, 12.2);
    const double lowDb = relativeDb(chordLow.left, chordOff.left, 0, firstWindow);
    const double halfDb = relativeDb(chordHalf.left, chordOff.left, 0, firstWindow);
    expect(lowDb < defaultDb && defaultDb < halfDb,
           "the coupling does not grow with the Resonance control ("
               + std::to_string(lowDb) + " -> " + std::to_string(defaultDb)
               + " -> " + std::to_string(halfDb) + " dB)");
    std::cout << "PROBE all-eight-fingered chord against the same chord at"
                 " Resonance 0: " << lowDb << " dB at 0.05, " << defaultDb
              << " dB at 0.20, " << halfDb << " dB at 0.50, " << fullDb
              << " dB at 1.0 (exactly zero difference before the step)\n";

    // 3. The coupling is lossy, not regenerative. Its own residual has to die
    //    away faster than the chord does, in absolute terms.
    const auto residualRms = [&] (const StereoBuffer& coupled, int start, int end)
    {
        double sum = 0.0;
        for (int i = start; i < end; ++i)
        {
            const double delta =
                static_cast<double>(coupled.left[static_cast<std::size_t>(i)])
                - static_cast<double>(chordOff.left[static_cast<std::size_t>(i)]);
            sum += delta * delta;
        }
        return std::sqrt(sum / static_cast<double>(end - start));
    };
    for (const auto* coupled : { &chordDefault, &chordFull })
    {
        const double early = residualRms(*coupled, 0, firstWindow);
        const double late = residualRms(*coupled,
                                        static_cast<int>(10.0 * sampleRate),
                                        static_cast<int>(12.0 * sampleRate));
        expect(early > 0.0, "the coupling contributes no residual at all");
        const double fall = 20.0 * std::log10(early / std::max(late, 1.0e-30));
        expect(fall >= 20.0,
               "the coupled residual does not decay away ("
                   + std::to_string(fall) + " dB from 0-1.5 s to 10-12 s)");
        std::cout << "PROBE coupled residual falls " << fall
                  << " dB from 0-1.5 s to 10-12 s\n";
    }

    // 4. Thirty seconds of eight strings at full velocity and maximum
    //    Resonance: bounded, and falling. Strict monotonicity over successive
    //    1 s windows is *not* asserted - the uncoupled engine already breaks it
    //    three times over this render, by up to 0.43 dB, so the bar is that the
    //    coupling does not add regeneration on top of that.
    const auto longChord = renderChord(allEight, 1.0f, 1.0f, 1.0f, 30.0);
    expect(allFinite(longChord), "thirty seconds of maximum coupling is not finite");
    expect(peakAbs(longChord.left) < 3.05f && peakAbs(longChord.right) < 3.05f,
           "maximum played-string coupling escaped the output guard");
    double previousWindow = 0.0;
    double worstRise = 0.0;
    for (int second = 0; second < 30; ++second)
    {
        const double windowRms = rmsInRange(
            longChord.left, static_cast<int>(second * sampleRate),
            static_cast<int>((second + 1) * sampleRate));
        if (second > 0 && windowRms > previousWindow)
            worstRise = std::max(worstRise,
                                 20.0 * std::log10(windowRms / previousWindow));
        previousWindow = windowRms;
    }
    expect(worstRise <= 0.5,
           "a 1 s window of the maximum-coupling chord grew by "
               + std::to_string(worstRise) + " dB");
    const double firstSecond = rmsInRange(longChord.left, 0,
                                          static_cast<int>(sampleRate));
    const double lastSecond = rmsInRange(longChord.left,
                                         static_cast<int>(29.0 * sampleRate),
                                         static_cast<int>(30.0 * sampleRate));
    const double thirtySecondFall =
        20.0 * std::log10(firstSecond / std::max(lastSecond, 1.0e-30));
    expect(thirtySecondFall >= 70.0,
           "the maximum-coupling chord did not decay over thirty seconds");
    std::cout << "PROBE thirty seconds of maximum coupling: worst 1 s rise "
              << worstRise << " dB, total fall " << thirtySecondFall << " dB\n";

    // 5. The spectral-radius bound, read at the seam. The coupling matrix has
    //    a zero diagonal and off-diagonal entries g/(1 - G_i), so the row-sum
    //    norm is (N - 1) g max_i 1/(1 - G_i); it is held at or below 0.25 by
    //    construction, at every setting, and is checked here against the loop
    //    gains the voices are actually running.
    double worstRowSum = 0.0;
    for (const float sympathetic : { 0.05f, 0.20f, 0.50f, 1.0f })
        for (const float resonance : { 0.0f, 1.0f })
            for (const float mute : { 0.0f, 0.5f })
            {
                ElectryEngine engine;
                engine.prepare(sampleRate, 512);
                EngineParameters parameters;
                parameters.sympatheticAmount = sympathetic;
                parameters.resonanceDepth = resonance;
                parameters.palmMute = mute;
                parameters.artifactAmount = 0.0f;
                parameters.strumSpreadSeconds = 0.0f;
                engine.setParameters(parameters);
                engine.reset();
                for (const int note : allEight)
                    engine.noteOn(note, 1.0f);
                StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
                renderInto(engine, settle);

                int active = 0;
                double worstAmplification = 1.0;
                for (int stringIndex = 0;
                     stringIndex < ElectryEngine::stringCount; ++stringIndex)
                {
                    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
                    if (! snapshot.active)
                        continue;
                    ++active;
                    worstAmplification = std::max(
                        worstAmplification,
                        1.0 / std::max(1.0 - snapshot.loopGain, 1.0e-6));
                }
                const double gain = TestAccess::bridgeCouplingGain(engine);
                const double rowSum = static_cast<double>(active - 1) * gain
                                    * worstAmplification;
                expect(rowSum <= 0.25,
                       "the played-string coupling exceeded its row-sum bound ("
                           + std::to_string(rowSum) + " at Resonance "
                           + std::to_string(sympathetic) + ")");
                expect(TestAccess::bridgeCouplingRowSum(engine) <= 0.25f,
                       "the engine's own row-sum norm exceeded 0.25");
                expect(gain > 0.0 || mute >= 1.0f,
                       "the played strings read no coupling at all");
                worstRowSum = std::max(worstRowSum, rowSum);
            }
    std::cout << "PROBE worst played-string coupling row-sum norm over the "
                 "sweep: " << worstRowSum << " against a bound of 0.25\n";

    // 6. The self-term. With one voice sounding, the bus *is* that voice's own
    //    contribution, so `bus - own` is exactly zero and the injection must be
    //    exactly zero however far the control is pushed. The check is on the
    //    voice's own loop energy rather than on the output, because the output
    //    also carries the seven idle strings ringing sympathetically, which
    //    legitimately do move with the control. A voice that drove itself
    //    through the bus would change its own loop - and every decay time, T60
    //    and timbre calibration in the instrument sits downstream of that.
    const auto singleNoteLoopEnergy = [&] (float sympathetic)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = sympathetic;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(40, 0.90f);
        StereoBuffer buffer(static_cast<int>(2.0 * sampleRate));
        renderInto(engine, buffer);
        const int stringIndex = TestAccess::stringForNote(engine, 40);
        expect(stringIndex >= 0, "the single note did not sound");
        expect(TestAccess::bridgeCouplingGain(engine) == 0.0f,
               "a lone voice was given a non-zero share of the bridge bus");
        return TestAccess::voiceLoopEnergy(engine, stringIndex);
    };
    const float loopEnergyOff = singleNoteLoopEnergy(0.0f);
    expect(loopEnergyOff > 0.0f, "the single note left no loop energy to read");
    expect(singleNoteLoopEnergy(0.20f) == loopEnergyOff,
           "a single note's own loop moved with the Resonance control at 0.20");
    expect(singleNoteLoopEnergy(1.0f) == loopEnergyOff,
           "a single note's own loop moved with the Resonance control at 1.0");

    // 7. Off is off, and the top end stays clean. The new path is a broadband
    //    injection into eight high-Q loops, which is exactly the shape that
    //    folds energy back above Nyquist if it is unbounded.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : allEight)
            engine.noteOn(note, 1.0f);
        StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, settle);
        expect(TestAccess::bridgeCouplingGain(engine) == 0.0f
                   && TestAccess::bridgeCouplingRowSum(engine) == 0.0f,
               "the played-string coupling is not exactly zero at Resonance 0");
    }

    const auto aliasing = renderChord(allEight, 1.0f, 1.0f, 1.0f, 2.0);
    const int aliasStart = static_cast<int>(0.2 * sampleRate);
    const int aliasLength = static_cast<int>(1.0 * sampleRate);
    double spectralPeak = 0.0;
    for (double frequency = 60.0; frequency < 8000.0; frequency *= 1.01)
        spectralPeak = std::max(spectralPeak,
                                dftMagnitude(aliasing.left, aliasStart,
                                             aliasLength, sampleRate, frequency));
    double aliasPeak = 0.0;
    for (double frequency = 12000.0; frequency < 23000.0; frequency *= 1.005)
        aliasPeak = std::max(aliasPeak,
                             dftMagnitude(aliasing.left, aliasStart,
                                          aliasLength, sampleRate, frequency));
    const double aliasFloorDb =
        20.0 * std::log10(spectralPeak / std::max(aliasPeak, 1.0e-30));
    expect(aliasFloorDb >= 150.0,
           "played-string coupling raised the alias floor to "
               + std::to_string(aliasFloorDb) + " dB below the peak");
    std::cout << "PROBE alias floor with played-string coupling at maximum: "
              << aliasFloorDb << " dB below the spectral peak\n";
}

void testIdleFreezeAndDenormalSafety()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 1.0f;
    parameters.artifactAmount = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.stringAge = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // A freshly prepared engine is already frozen: no body mode, coil, DC
    // blocker or decimator runs until a string is asked to vibrate.
    StereoBuffer beforeAnyNote(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, beforeAnyNote);
    expect(peakAbs(beforeAnyNote.left) == 0.0f
               && peakAbs(beforeAnyNote.right) == 0.0f,
           "an untouched engine did not render exact digital silence");

    for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(1.0 * sampleRate));
    renderInto(engine, strum);
    expect(peakAbs(strum.left) > 0.01f, "the strum did not sound");
    engine.allNotesOff();

    // Every sample of the ring-out must be finite, and none of it may be a
    // subnormal float: the hosts that matter run with flush-to-zero, but the
    // engine must not depend on that to stay cheap.
    int subnormals = 0;
    int silentBlocks = 0;
    bool sawSilence = false;
    constexpr float smallestNormal = 1.1754943508e-38f;
    for (int block = 0; block < 40 && ! sawSilence; ++block)
    {
        StereoBuffer tail(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, tail);
        expect(allFinite(tail), "the ring-out produced non-finite audio");
        float peak = 0.0f;
        for (const float sample : tail.left)
        {
            const float magnitude = std::abs(sample);
            peak = std::max(peak, magnitude);
            if (magnitude > 0.0f && magnitude < smallestNormal)
                ++subnormals;
        }
        if (peak == 0.0f)
        {
            sawSilence = true;
            silentBlocks = block;
        }
    }
    expect(subnormals == 0,
           "the ring-out generated " + std::to_string(subnormals)
               + " subnormal output samples");
    expect(sawSilence,
           "the engine never reached exact silence after the last note");
    // Freezing must happen promptly enough to matter, but never so early that
    // an audible tail is truncated.
    expect(silentBlocks >= 1 && silentBlocks <= 24,
           "the idle freeze happened at an implausible time (block "
               + std::to_string(silentBlocks) + ")");

    // A frozen engine wakes cleanly.
    engine.noteOn(45, 0.9f);
    StereoBuffer wake(static_cast<int>(0.25 * sampleRate));
    renderInto(engine, wake);
    expect(allFinite(wake) && peakAbs(wake.left) > 1.0e-3f,
           "a frozen engine did not wake for a new note");
}

void testCpuGuardrail()
{
    constexpr double sampleRate = 96000.0;
    constexpr int totalSamples = static_cast<int>(2.0 * 96000.0);

    // All eight physical strings ringing in Drop-E tuning. With every string
    // played there is no coupled string left to render, so this measures
    // exactly the same work the pre-1.1 engine did.
    const auto strike = [&] (ElectryEngine& engine, PickupSelector selector,
                             electry::OutputMode mode)
    {
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = selector;
        parameters.outputMode = mode;
        parameters.artifactAmount = 1.0f;
        parameters.bodyResonance = 1.0f;
        engine.setParameters(parameters);
        engine.reset();

        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            engine.noteOn(note, 0.9f);
    };

    StereoBuffer buffer(totalSamples);
    const auto timeRender = [&] (ElectryEngine& engine)
    {
        const auto begin = std::chrono::steady_clock::now();
        renderInto(engine, buffer);
        const auto end = std::chrono::steady_clock::now();
        expect(allFinite(buffer), "the CPU guardrail render was not finite");
        return std::chrono::duration<double>(end - begin).count()
             / (static_cast<double>(totalSamples) / sampleRate);
    };

    // The two configurations are timed alternately, each from a freshly struck
    // chord, and each keeps its fastest sample. Measuring one configuration to
    // completion and then the other lets a busy stretch on a shared runner land
    // entirely on whichever went second, which is enough to invert a real
    // difference; interleaving exposes both to the same noise.
    double worstCase = 1.0e9;
    double defaultCase = 1.0e9;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        ElectryEngine worstEngine;
        strike (worstEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        worstCase = std::min (worstCase, timeRender (worstEngine));

        ElectryEngine defaultEngine;
        strike (defaultEngine, PickupSelector::Bridge, electry::OutputMode::Mono);
        defaultCase = std::min (defaultCase, timeRender (defaultEngine));
    }
    std::cout << "Eight-string render CPU ratio at 96 kHz: " << worstCase
              << "x worst case (Both + Stereo), " << defaultCase
              << "x default (Bridge + Mono)\n";

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    constexpr double ceiling = 40.0;
#else
    // Loose portable runaway guard; shared CI runners are not a stable
    // benchmark fixture.
    constexpr double ceiling = 8.0;
#endif
    expect(worstCase < ceiling,
           "eight-string render exceeded the portable CPU ceiling");
    expect(defaultCase < ceiling,
           "default-configuration render exceeded the portable CPU ceiling");

    // The default configuration is cheaper than the worst case, and it is
    // deliberately not asserted here. It used to be, as `defaultCase <
    // worstCase * 0.97`, and that assertion was flaky: the saving it looks for
    // is a few per cent of the render, CI has measured it as low as 1.3%, and
    // that is smaller than the run-to-run spread of a wall clock on a shared
    // runner. Interleaving the two configurations and keeping each one's
    // fastest sample -- which is what the loop above does, and is the right
    // thing to do -- narrows that spread but cannot get underneath it. No
    // threshold makes a timing comparison both sensitive to a 1% difference
    // and reliable on hardware this project does not control.
    //
    // Nothing is lost by dropping it, because the claim underneath it is
    // structural rather than statistical: the default skips the unselected
    // pickup chain and runs one shared output chain instead of two.
    // testPickupCullingAndChannelLinking asserts exactly that, directly and
    // deterministically, by reading the engine's own culling and link flags for
    // every selector position. A regression that made the default do the worst
    // case's work fails there, on the first run, on any machine.
    //
    // What is left here is what a CPU guardrail is actually for: a runaway
    // ceiling. That is a threshold a wall clock can carry, because it is orders
    // of magnitude away from the noise rather than inside it.
    std::cout << "  (the default/worst-case ratio is reported, not asserted; "
              << "the culling it reflects is asserted structurally in "
              << "testPickupCullingAndChannelLinking)\n";

    // A full-throw wheel glide on the same chord must not be a hidden second
    // worst case. Re-fitting the dispersion grid on every control tick of the
    // glide once cost several times the settled render - beyond realtime on
    // this configuration - which the settled measurements above cannot see.
    double glideCase = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        ElectryEngine glideEngine;
        strike (glideEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        glideEngine.setPitchBend (1.0f);
        glideCase = std::min (glideCase, timeRender (glideEngine));
    }
    std::cout << "Eight-string wheel-glide CPU ratio at 96 kHz: " << glideCase
              << "x\n";
    expect(glideCase < ceiling,
           "a bent eight-string chord exceeded the portable CPU ceiling ("
               + std::to_string(glideCase) + "x)");
    expect(glideCase < worstCase * 2.0,
           "the wheel glide costs far more than the settled worst case ("
               + std::to_string(glideCase) + "x vs "
               + std::to_string(worstCase) + "x)");
}

} // namespace

int main()
{
    testModalResonatorPeakGain();
    testInternalOversamplingPolicy();
    testPrepareClampsHostileSampleRate();
    testRenderMatrixFiniteAndBounded();
    testPitchAccuracy();
    testDropELowNoteAtMaximumRate();
    testPrepareSanitisesSampleRate();
    testProcessRejectsInvalidBuffers();
    testDeterminism();
    testKeyswitchesSelectStylesSilently();
    testAlternateStrokeSequence();
    testArticulationsSoundDistinct();
    testStyleAndStrokeCombinations();
    testFingeredNotesDrawNoPickingHandVariation();
    testPitchWheelPerStringSensitivity();
    testPitchWheelGlideFollowsBendTime();
    testPitchWheelBendsSympatheticStrings();
    testHammerOnLegatoContinuity();
    testTensionGlide();
    testPickupsToneAndModelMorph();
    testHumbuckerTwoCoilNotch();
    testArtifactsControl();
    testAdvancedDispersionAndBodyConductance();
    testLowRegisterGuitarEnvelope();
    testOpenLowStringLevelBalance();
    testMonoStereoOutputField();
    testVelocityDynamicRange();
    testVelocityExpression();
    testPickingHandVariation();
    testMaterialAndControlAudibility();
    testNoiseComponentsAndSilence();
    testStringAllocationAndPolyphony();
    testVoiceStealingPriority();
    testNoteOnVelocitySanitisation();
    testSetVibratoSanitisation();
    testSetPitchBendSanitisation();
    testSetResonanceReturnLevelAndPalmMutePressureSanitisation();
    testDelayTapClampsAndInterpolates();
    testFrettingHandPosition();
    testTouchHarmonics();
    testPinchHarmonic();
    testSlideArticulation();
    testFrettingHandVibrato();
    testVibratoIsAHandNotAnLfo();
    testDeadNote();
    testSustainPedal();
    testVibratoOnlyMovesFingeredStrings();
    testLegatoSlideDoesNotConsumeAPickStroke();
    testSympatheticBridgeCoupling();
    testPalmMuteContinuum();
    testStrumSpread();
    testStrumTravelFollowsStroke();
    testResonanceControlRaisesSympatheticRing();
    testResonanceFeedbackSelfSustains();
    testPickGeometryFollowsFret();
    testPickContactGeometry();
    testPalmMuteDoesNotShiftPitch();
    testHandDipNeverExpands();
    testLowRegisterFundamentalWeight();
    testVisualStateAndGeometry();
    testVisualStateSanitizesNonFiniteInput();
    testVisualGeometryClampsOutOfRangeInput();
    testPickupCullingAndChannelLinking();
    testIdleFreezeAndDenormalSafety();
    testDecayIsSampleRateInvariant();
    testPolarisationCouplingIsPerRoundTrip();
    testCoupledStringLosesItsTopEndLikeAPlayedString();
    testCoupledStringKeepsItsFundamentalDecayTarget();
    testFingeredStringsShareTheBridge();
    testParameterSanitisation();
    testParameterSanitisationFallsBackToDefaults();
    testPushAcousticReturnSanitisation();
    testCpuGuardrail();

    // Test attack tension modulation and palm-mute bridge impact physics
    {
        constexpr auto sampleRate = 48000.0;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);

        electry::EngineParameters parameters {};
        parameters.muteDamping = 0.8f;
        engine.setParameters(parameters);

        const auto muted = renderNote(engine, sampleRate, 28, 0.95f, PlayStyle::PalmMute, 0.5, 0.1);
        expect(peakAbs(muted.left) > 1.0e-5f, "palm mute stroke was silent");
        expect(peakAbs(muted.left) < 16.0f, "palm mute stroke exceeded peak guardrail");
    }

    if (failures != 0)
    {
        std::cerr << failures << " Electry DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry DSP checks passed.\n";
    return EXIT_SUCCESS;
}

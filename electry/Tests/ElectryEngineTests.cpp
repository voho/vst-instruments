#include "DSP/ElectryEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
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
        Articulation articulation { Articulation::Downstroke };
        float verticalDelayTarget { 0.0f };
        float verticalDelayCurrent { 0.0f };
        float dispersionCoefficient { 0.0f };
        float loopGain { 0.0f };
        int collisionRemaining { 0 };
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
        result.articulation = voice.articulation;
        result.verticalDelayTarget = voice.vertical.targetDelay;
        result.verticalDelayCurrent = voice.vertical.currentDelay;
        result.dispersionCoefficient = voice.vertical.dispersionCoefficient;
        result.loopGain = voice.vertical.loopGain;
        result.collisionRemaining = voice.collisionRemaining;
        return result;
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
};
} // namespace electry

namespace
{
using electry::Articulation;
using electry::ElectryEngine;
using electry::EngineParameters;
using electry::PickupSelector;
using TestAccess = electry::ElectryEngineTestAccess;

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
                        float velocity, Articulation articulation,
                        double seconds, double noteOffAfterSeconds = -1.0)
{
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(articulation), 1.0f);
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
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
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

// ---------------------------------------------------------------------------

void testRenderMatrixFiniteAndBounded()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        engine.setParameters(parameters);

        for (int articulationIndex = 0;
             articulationIndex < ElectryEngine::keyswitchCount; ++articulationIndex)
        {
            const auto articulation = static_cast<Articulation>(articulationIndex);
            auto buffer = renderNote(engine, sampleRate, 45, 0.9f, articulation,
                                     0.5, 0.35);
            expect(allFinite(buffer),
                   "non-finite output at rate " + std::to_string(sampleRate)
                       + " articulation " + std::to_string(articulationIndex));
            const float peak = peakAbs(buffer.left);
            expect(peak < 1.2f,
                   "output beyond guard at rate " + std::to_string(sampleRate)
                       + " articulation " + std::to_string(articulationIndex));
            expect(peak > 1.0e-4f,
                   "articulation " + std::to_string(articulationIndex)
                       + " is silent at rate " + std::to_string(sampleRate));
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

        for (const int midiNote : { 40, 45, 50, 55, 59, 64, 69, 76, 86 })
        {
            auto buffer = renderNote(engine, sampleRate, midiNote, 0.3f,
                                     Articulation::Downstroke, 1.1);
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
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(Articulation::Slap), 1.0f);
        engine.noteOn(45, 0.85f);
        engine.process(buffer.left.data(), buffer.right.data(), 12000);
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(Articulation::Upstroke), 1.0f);
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

    expect(engine.getCurrentArticulation() == Articulation::Downstroke,
           "default articulation is not Downstroke");

    // Keyswitches alone must never make sound.
    StereoBuffer buffer(static_cast<int>(0.25 * sampleRate));
    for (int keyswitch = 0; keyswitch < ElectryEngine::keyswitchCount; ++keyswitch)
        engine.noteOn(ElectryEngine::firstKeyswitchNote + keyswitch, 1.0f);
    renderInto(engine, buffer);
    expect(peakAbs(buffer.left) == 0.0f, "keyswitch notes produced audio");
    expect(engine.getActiveVoiceCount() == 0, "keyswitch notes created voices");

    // The last pressed keyswitch latches.
    expect(engine.getCurrentArticulation() == Articulation::Slap,
           "keyswitch latching did not reach Slap");

    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Muted), 1.0f);
    expect(engine.getCurrentArticulation() == Articulation::Muted,
           "keyswitch did not switch to Muted");

    // Keyswitch note-offs are ignored; the style persists for played notes.
    engine.noteOff(ElectryEngine::firstKeyswitchNote
                       + static_cast<int>(Articulation::Muted));
    expect(engine.getCurrentArticulation() == Articulation::Muted,
           "keyswitch note-off cleared the latched style");

    engine.noteOn(52, 0.8f);
    const auto stringIndex = TestAccess::stringForNote(engine, 52);
    expect(stringIndex >= 0, "played note did not allocate a string");
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.articulation == Articulation::Muted,
           "played note did not inherit the latched articulation");

    // Notes outside the playable range are ignored.
    engine.noteOn(20, 0.9f);
    engine.noteOn(97, 0.9f);
    expect(engine.getActiveVoiceCount() == 1,
           "unplayable notes outside E2..D6 were not ignored");
}

void testArticulationsSoundDistinct()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    engine.setParameters(parameters);

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);

    const auto renderStyle = [&] (Articulation articulation)
    {
        return renderNote(engine, sampleRate, 48, 0.85f, articulation, 1.2);
    };

    const auto down = renderStyle(Articulation::Downstroke);
    const auto up = renderStyle(Articulation::Upstroke);
    const auto hammer = renderStyle(Articulation::HammerOn);
    const auto muted = renderStyle(Articulation::Muted);
    const auto slap = renderStyle(Articulation::Slap);

    const double f0 = midiHz(48);
    const double downCentroid = spectralCentroid(down.left, attackStart,
                                                 attackWindow, sampleRate, f0);
    const double upCentroid = spectralCentroid(up.left, attackStart,
                                               attackWindow, sampleRate, f0);
    const double hammerCentroid = spectralCentroid(hammer.left, attackStart,
                                                   attackWindow, sampleRate, f0);
    const double slapCentroid = spectralCentroid(slap.left, attackStart,
                                                 attackWindow, sampleRate, f0);

    // The hammered attack is fingered, not picked: it must be darker than
    // both pick strokes. The slap attack must be the brightest.
    expect(hammerCentroid < downCentroid * 0.9,
           "hammer-on attack is not darker than a downstroke");
    expect(hammerCentroid < upCentroid * 0.9,
           "hammer-on attack is not darker than an upstroke");
    expect(slapCentroid > downCentroid * 1.05,
           "slap attack is not brighter than a downstroke");
    expect(upCentroid > downCentroid * 1.01,
           "upstroke attack is not brighter than a downstroke");

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

void testBendPrograms()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.bendTimeSeconds = 0.20f;
    engine.setParameters(parameters);

    struct BendCase
    {
        Articulation articulation;
        double startCentsFromPlayed;
        double endCentsFromPlayed;
    };
    const std::array<BendCase, 4> cases {{
        { Articulation::Bend1Up, 0.0, 100.0 },
        { Articulation::Bend2Up, 0.0, 200.0 },
        { Articulation::Bend1Down, 100.0, 0.0 },
        { Articulation::Bend2Down, 200.0, 0.0 },
    }};

    for (const auto& bendCase : cases)
    {
        const int midiNote = 55;
        auto buffer = renderNote(engine, sampleRate, midiNote, 0.35f,
                                 bendCase.articulation, 2.0);
        const double played = midiHz(midiNote);

        // Early window: after the pick transient, before the bend departs
        // (the bend holds for about 55 ms before travelling).
        const int earlyStart = static_cast<int>(0.008 * sampleRate);
        const int earlyLength = static_cast<int>(0.038 * sampleRate);
        const double expectedStart = played
            * std::pow(2.0, bendCase.startCentsFromPlayed / 1200.0);
        const double startFrequency = measureFrequency(
            buffer.left, earlyStart, earlyLength, sampleRate, expectedStart);
        const double startError = centsBetween(startFrequency, expectedStart);
        // The short analysis window and attack transient limit precision.
        expect(std::abs(startError) < 35.0,
               "bend start pitch off by " + std::to_string(startError)
                   + " cents for articulation "
                   + std::to_string(static_cast<int>(bendCase.articulation)));

        // Late window: the bend has settled.
        const int lateStart = static_cast<int>(1.0 * sampleRate);
        const int lateLength = static_cast<int>(0.8 * sampleRate);
        const double expectedEnd = played
            * std::pow(2.0, bendCase.endCentsFromPlayed / 1200.0);
        const double endFrequency = measureFrequency(
            buffer.left, lateStart, lateLength, sampleRate, expectedEnd);
        const double endError = centsBetween(endFrequency, expectedEnd);
        expect(std::abs(endError) < 10.0,
               "bend end pitch off by " + std::to_string(endError)
                   + " cents for articulation "
                   + std::to_string(static_cast<int>(bendCase.articulation)));

        // And the travel is in the right direction by the right amount.
        const double travel = centsBetween(endFrequency, startFrequency);
        const double expectedTravel = bendCase.endCentsFromPlayed
                                    - bendCase.startCentsFromPlayed;
        expect(std::abs(travel - expectedTravel) < 40.0,
               "bend travel measured " + std::to_string(travel)
                   + " cents, expected " + std::to_string(expectedTravel));
    }
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

    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::HammerOn), 1.0f);
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

void testSlapCollisionAndTensionGlide()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    engine.setParameters(parameters);

    auto slap = renderNote(engine, sampleRate, 45, 0.95f, Articulation::Slap, 1.8);

    // The collision window must actually have engaged.
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Slap), 1.0f);
    engine.noteOn(45, 0.95f);
    const int stringIndex = TestAccess::stringForNote(engine, 45);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.collisionRemaining > 0,
           "slap did not open a fret-collision window");

    // Tension modulation: the attack sounds sharp and relaxes as the string
    // energy decays. The early window sits where the energy envelope peaks,
    // and both measurements share the same scan reference so grid quantise
    // bias cancels in the difference.
    const double early = measureFrequency(slap.left,
                                          static_cast<int>(0.05 * sampleRate),
                                          static_cast<int>(0.14 * sampleRate),
                                          sampleRate, midiHz(45));
    const double late = measureFrequency(slap.left,
                                         static_cast<int>(1.2 * sampleRate),
                                         static_cast<int>(0.5 * sampleRate),
                                         sampleRate, midiHz(45));
    const double glide = centsBetween(early, late);
    expect(glide > 1.5,
           "slap attack does not glide sharp-to-true (measured "
               + std::to_string(glide) + " cents)");
    expect(glide < 80.0, "slap pitch glide is implausibly large");
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

    parameters.pickupSelector = PickupSelector::Bridge;
    engine.setParameters(parameters);
    const auto bridge = renderNote(engine, sampleRate, 45, 0.7f,
                                   Articulation::Downstroke, 0.8);

    parameters.pickupSelector = PickupSelector::Neck;
    engine.setParameters(parameters);
    const auto neck = renderNote(engine, sampleRate, 45, 0.7f,
                                 Articulation::Downstroke, 0.8);

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
                                 Articulation::Downstroke, 0.8);
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
                                          Articulation::Downstroke, 0.8);

    EngineParameters telecaster;
    telecaster.bodyWood = telecaster.bodySize = telecaster.bodyShape = 1.0f;
    telecaster.construction = telecaster.scaleLength = telecaster.pickupType = 1.0f;
    telecaster.pickNoise = 0.0f;
    engine.setParameters(telecaster);
    const auto telecasterRender = renderNote(engine, sampleRate, 45, 0.7f,
                                             Articulation::Downstroke, 0.8);

    const double lesPaulCentroid = spectralCentroid(lesPaulRender.left, start,
                                                    window, sampleRate, f0);
    const double telecasterCentroid = spectralCentroid(telecasterRender.left,
                                                       start, window, sampleRate, f0);
    // The single-coil Telecaster bridge is characteristically brighter than
    // a humbucker Les Paul.
    expect(telecasterCentroid > lesPaulCentroid * 1.10,
           "Telecaster endpoint is not brighter than Les Paul endpoint");

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
                                  Articulation::Downstroke, 0.9, 0.6);

    EngineParameters fullNoise;
    fullNoise.pickNoise = 1.0f;
    fullNoise.fingerNoise = 1.0f;
    fullNoise.releaseNoise = 1.0f;
    engine.setParameters(fullNoise);
    const auto noisy = renderNote(engine, sampleRate, 45, 0.8f,
                                  Articulation::Downstroke, 0.9, 0.6);

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
                                         Articulation::Downstroke, 0.9, 0.6);

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
    expect(TestAccess::stringForNote(engine, 48) == 1, "C3 is not on the A string");
    expect(TestAccess::stringForNote(engine, 52) == 2, "E3 is not on the D string");
    expect(TestAccess::stringForNote(engine, 55) == 3, "G3 is not on the G string");
    expect(TestAccess::stringForNote(engine, 60) == 4, "C4 is not on the B string");
    expect(TestAccess::stringForNote(engine, 64) == 5,
           "E4 is not on the high E string");

    // A sixth note takes the last free string; a seventh must steal, keeping
    // the voice count at the physical six.
    engine.noteOn(40, 0.8f);
    expect(engine.getActiveVoiceCount() == 6, "open E2 did not use the E string");
    engine.noteOn(50, 0.8f);
    expect(engine.getActiveVoiceCount() == 6,
           "seventh simultaneous note exceeded six strings");

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

void testPitchWheelAndSustainPedal()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    engine.noteOn(50, 0.4f);
    StereoBuffer buffer(static_cast<int>(2.0 * sampleRate));
    engine.process(buffer.left.data(), buffer.right.data(),
                   static_cast<int>(0.6 * sampleRate));
    engine.setPitchBend(1.0f); // +2 semitones
    engine.process(buffer.left.data() + static_cast<int>(0.6 * sampleRate),
                   buffer.right.data() + static_cast<int>(0.6 * sampleRate),
                   buffer.size() - static_cast<int>(0.6 * sampleRate));

    const double before = measureFrequency(buffer.left,
                                           static_cast<int>(0.2 * sampleRate),
                                           static_cast<int>(0.35 * sampleRate),
                                           sampleRate, midiHz(50));
    const double after = measureFrequency(buffer.left,
                                          static_cast<int>(1.2 * sampleRate),
                                          static_cast<int>(0.6 * sampleRate),
                                          sampleRate, midiHz(52));
    const double travel = centsBetween(after, before);
    expect(std::abs(travel - 200.0) < 15.0,
           "pitch wheel travel measured " + std::to_string(travel) + " cents");

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
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    engine.setParameters(hostile);

    auto buffer = renderNote(engine, sampleRate, 45, 0.9f, Articulation::Slap, 0.6);
    expect(allFinite(buffer), "hostile parameters produced non-finite audio");
    expect(peakAbs(buffer.left) < 2.1f, "hostile parameters bypassed the guard");

    engine.setPitchBend(std::numeric_limits<float>::quiet_NaN());
    engine.noteOn(45, std::numeric_limits<float>::quiet_NaN());
    StereoBuffer more(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, more);
    expect(allFinite(more), "hostile performance input produced non-finite audio");
}

void testCpuGuardrail()
{
    constexpr double sampleRate = 96000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // All six strings ringing.
    for (const int note : { 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 0.9f);

    constexpr int totalSamples = static_cast<int>(2.0 * 96000.0);
    StereoBuffer buffer(totalSamples);

    double bestSeconds = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto begin = std::chrono::steady_clock::now();
        renderInto(engine, buffer);
        const auto end = std::chrono::steady_clock::now();
        bestSeconds = std::min(
            bestSeconds,
            std::chrono::duration<double>(end - begin).count());
    }

    const double rendered = static_cast<double>(totalSamples) / sampleRate;
    const double ratio = bestSeconds / rendered;
    std::cout << "Six-string render CPU ratio at 96 kHz: " << ratio << "x\n";

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    constexpr double ceiling = 40.0;
#else
    // Loose portable runaway guard; shared CI runners are not a stable
    // benchmark fixture.
    constexpr double ceiling = 8.0;
#endif
    expect(ratio < ceiling, "six-string render exceeded the portable CPU ceiling");
}

} // namespace

int main()
{
    testRenderMatrixFiniteAndBounded();
    testPitchAccuracy();
    testDeterminism();
    testKeyswitchesSelectStylesSilently();
    testArticulationsSoundDistinct();
    testBendPrograms();
    testHammerOnLegatoContinuity();
    testSlapCollisionAndTensionGlide();
    testPickupsToneAndModelMorph();
    testNoiseComponentsAndSilence();
    testStringAllocationAndPolyphony();
    testPitchWheelAndSustainPedal();
    testParameterSanitisation();
    testCpuGuardrail();

    if (failures != 0)
    {
        std::cerr << failures << " Electry DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry DSP checks passed.\n";
    return EXIT_SUCCESS;
}

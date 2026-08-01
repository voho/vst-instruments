// Engine behaviour suite: what the instrument does when it is played, as
// opposed to what its individual circuit blocks compute. The circuit suite
// checks the laws; this one checks the machine.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Panel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace youknow106;

int failures = 0;

// The build system defines this whenever it adds -fsanitize, because no macro
// covers every case: GCC with only -fsanitize=undefined defines neither
// __has_feature nor __SANITIZE_UNDEFINED__, and the realtime assertion below
// would then hold instrumented code to an uninstrumented budget. The compiler
// macros stay as a fallback for a sanitizer build configured some other way.
#if defined(YOUKNOW106_SANITIZER_BUILD)
constexpr bool sanitizerBuild = YOUKNOW106_SANITIZER_BUILD != 0;
#elif defined(__has_feature)
constexpr bool sanitizerBuild = __has_feature(address_sanitizer)
                             || __has_feature(undefined_behavior_sanitizer);
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)
constexpr bool sanitizerBuild = true;
#else
constexpr bool sanitizerBuild = false;
#endif

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
    if (!(std::abs(actual - expected) <= tolerance))
    {
        ++failures;
        std::cerr << "FAIL: " << message << " (got " << actual << ", expected "
                  << expected << " +/- " << tolerance << ")\n";
    }
}

constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 256;

struct Render
{
    std::vector<float> left;
    std::vector<float> right;
};

Render render(YouKnow106Engine& engine, int samples)
{
    Render result;
    const int blocks = (samples + blockSize - 1) / blockSize;
    result.left.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    result.right.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    for (int block = 0; block < blocks; ++block)
        engine.process(result.left.data() + block * blockSize,
                       result.right.data() + block * blockSize, blockSize);
    return result;
}

EngineParameters plainPatch()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.decay = 1.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.0f;
    parameters.vcaLevel = 1.0f;
    parameters.volume = 1.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.calibration = 0.0f;
    return parameters;
}

double peakOf(const std::vector<float>& signal, std::size_t from)
{
    double peak = 0.0;
    for (std::size_t index = from; index < signal.size(); ++index)
        peak = std::max(peak, static_cast<double>(std::abs(signal[index])));
    return peak;
}

double magnitudeAt(const std::vector<float>& signal, std::size_t start, int length,
                   double frequency, double sampleRate)
{
    std::complex<double> accumulator {};
    for (int index = 0; index < length; ++index)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * index / (length - 1));
        accumulator += signal[start + static_cast<std::size_t>(index)] * window
                     * std::exp(std::complex<double>(0.0, -2.0 * pi * frequency
                                                              * index / sampleRate));
    }
    return std::abs(accumulator) / (length * 0.5);
}

// Frequency measured between the first and last rising zero crossing, which
// resolves far better than counting crossings over a fixed window.
double measuredFrequency(const std::vector<float>& signal, std::size_t from,
                         double sampleRate)
{
    std::size_t first = 0;
    std::size_t last = 0;
    int intervals = -1;
    for (std::size_t index = from + 1; index < signal.size(); ++index)
        if (signal[index - 1] <= 0.0f && signal[index] > 0.0f)
        {
            if (intervals < 0)
                first = index;
            last = index;
            ++intervals;
        }
    return intervals > 0 ? intervals * sampleRate / static_cast<double>(last - first)
                         : 0.0;
}

// --------------------------------------------------------------------------

void testRangeTransposesByOctaves()
{
    constexpr double sampleRate = 96000.0;
    const std::array<DcoRange, 3> ranges { DcoRange::Sixteen, DcoRange::Eight,
                                           DcoRange::Four };
    std::array<double, 3> measured {};

    for (std::size_t index = 0; index < ranges.size(); ++index)
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.range = ranges[index];
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        measured[index] = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                            sampleRate);
    }

    expectNear(measured[1] / measured[0], 2.0, 0.01,
               "8' does not sound an octave above 16'");
    expectNear(measured[2] / measured[1], 2.0, 0.01,
               "4' does not sound an octave above 8'");
    expectNear(measured[1], 440.0, 1.0, "8' does not sound at concert pitch");
}

void testSubIsOneOctaveDown()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.subLevel = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const auto frequency = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                             sampleRate);
    expectNear(frequency, 220.0, 1.0, "the sub is not one octave below the note");
}

void testAliasFloor()
{
    // The oscillator's discontinuities are repaired with residuals built by
    // integration; a broken table shows up here as a raised noise floor rather
    // than as anything obviously wrong in the waveform.
    for (double sampleRate : { 44100.0, 48000.0 })
    {
        for (int note : { 60, 84 })
        {
            YouKnow106Engine engine;
            engine.prepare(sampleRate, blockSize, true);
            auto parameters = plainPatch();
            engine.setParameters(parameters);
            engine.noteOn(note, 1.0f);

            const auto rendered = render(engine, static_cast<int>(sampleRate));
            const double wanted = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
                YouKnow106Engine::dcoDivider(wanted), DcoRange::Eight);

            const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);
            const int length = 16384;
            double harmonic = 0.0;
            for (int index = 1; index * f0 < 20000.0; ++index)
                harmonic = std::max(harmonic, magnitudeAt(rendered.left, start, length,
                                                          index * f0, sampleRate));

            double worst = 0.0;
            for (double frequency = 100.0; frequency < 20000.0; frequency += 41.0)
            {
                bool nearHarmonic = false;
                for (int index = 1; index * f0 < 24000.0; ++index)
                    if (std::abs(frequency - index * f0) < 60.0)
                    {
                        nearHarmonic = true;
                        break;
                    }
                if (nearHarmonic)
                    continue;
                worst = std::max(worst, magnitudeAt(rendered.left, start, length,
                                                    frequency, sampleRate));
            }

            const double decibels = 20.0 * std::log10((worst + 1.0e-15) / harmonic);
            expect(decibels < -55.0,
                   "alias floor for note " + std::to_string(note) + " at "
                       + std::to_string(static_cast<int>(sampleRate))
                       + " Hz is only " + std::to_string(decibels) + " dB down");
        }
    }
}

void testRampHasARampSpectrum()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    engine.setParameters(plainPatch());
    engine.noteOn(45, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
        YouKnow106Engine::dcoDivider(110.0), DcoRange::Eight);
    const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);

    const double fundamental = magnitudeAt(rendered.left, start, 16384, f0, sampleRate);
    for (int harmonic : { 2, 3, 5, 9 })
    {
        const double measured = magnitudeAt(rendered.left, start, 16384,
                                            harmonic * f0, sampleRate);
        const double expected = fundamental / harmonic;
        expectNear(20.0 * std::log10(measured / expected), 0.0, 1.5,
                   "harmonic " + std::to_string(harmonic)
                       + " does not follow a ramp's 1/n envelope");
    }
}

void testKeyAssignerDropsRatherThanSteals()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);

    for (int note = 60; note < 68; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6,
           "the key assigner does not stop at six voices");

    // The two extra keys must have been dropped, not swapped in: releasing the
    // first six has to silence the instrument completely.
    for (int note = 60; note < 68; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 0,
           "a dropped note left a voice sounding");
}

void testPolyModesDifferInAllocation()
{
    constexpr double sampleRate = 48000.0;

    const auto allocationOrder = [&](KeyMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.keyMode = mode;
        parameters.release = 0.6f;
        engine.setParameters(parameters);

        std::vector<int> masks;
        for (int note = 0; note < 4; ++note)
        {
            engine.noteOn(60 + note, 1.0f);
            render(engine, 512);
            masks.push_back(engine.getDisplayVoiceMask());
            engine.noteOff(60 + note);
            render(engine, 512);
        }
        return masks;
    };

    const auto rotating = allocationOrder(KeyMode::Poly1);
    const auto fixed = allocationOrder(KeyMode::Poly2);

    // Rotation must visit more than one voice across four separate notes;
    // fixed priority must keep coming back to the first.
    int distinct = 0;
    int seen = 0;
    for (const auto mask : rotating)
        if ((seen & mask) != mask)
        {
            seen |= mask;
            ++distinct;
        }
    expect(distinct >= 3, "rotating assignment is reusing the same voice");
    expect(fixed.front() == fixed.back(),
           "fixed-priority assignment is not returning to the first voice");
}

void testUnisonUsesEveryVoiceWithoutDetuning()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    // Zero tolerance: with the analogue spread switched out, six voices sharing
    // one reference and one count must be exactly coincident.
    parameters.calibration = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 6, "unison does not use every voice");

    // A detuned stack beats; a phase-locked one does not. Compare the peak in
    // two windows a long way apart.
    const std::size_t quarter = rendered.left.size() / 4;
    const double early = peakOf({ rendered.left.begin() + static_cast<long>(quarter),
                                  rendered.left.begin() + static_cast<long>(quarter * 2) },
                                0);
    const double late = peakOf({ rendered.left.begin() + static_cast<long>(quarter * 3),
                                 rendered.left.end() }, 0);
    expectNear(20.0 * std::log10((late + 1e-12) / (early + 1e-12)), 0.0, 0.5,
               "unison voices are beating against one another");
}

void testUnisonReturnsToAHeldKey()
{
    // Unison is monophonic. Holding one key, pressing a second and releasing
    // the second must hand the stack back to the first, not silence it.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    engine.noteOn(64, 1.0f);
    render(engine, 4096);
    engine.noteOff(64);
    const auto rendered = render(engine, static_cast<int>(48000.0 * 0.5));

    expect(engine.getActiveVoiceCount() == 6,
           "releasing the newer unison key silenced the key still held");
    expect(peakOf(rendered.left, rendered.left.size() / 2) > 0.01,
           "the unison stack fell silent with a key still down");

    const auto frequency = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                             48000.0);
    expectNear(frequency, 261.6, 4.0,
               "the unison stack did not return to the key still held");

    // Releasing an older key the stack has already left must change nothing.
    engine.noteOn(67, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    const auto after = render(engine, static_cast<int>(48000.0 * 0.5));
    expectNear(measuredFrequency(after.left, after.left.size() / 2, 48000.0),
               392.0, 6.0,
               "releasing a key the stack had left moved the sounding note");

    engine.noteOff(67);
    render(engine, static_cast<int>(48000.0));
    expect(engine.getActiveVoiceCount() == 0,
           "the unison stack did not release when the last key was let go");
}

void testAllNotesOffReleasesRatherThanCutting()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.75f;  // long enough that a cut would be obvious
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    engine.releaseAllNotes();
    const auto tail = render(engine, 8192);
    expect(peakOf(tail.left, 0) > 0.005,
           "releasing all notes cut the sound instead of letting it ring");
    expect(engine.getActiveVoiceCount() == 1,
           "releasing all notes retired the voice immediately");

    // The hard stop still is a hard stop. What remains afterwards is the
    // output coupling settling, which decays on its own time constant and is
    // not a voice -- so the claim is that it is far below the note, not that it
    // is bit-exact zero.
    const double sounding = peakOf(tail.left, 0);
    engine.allNotesOff();
    const auto silence = render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 0, "the hard stop left a voice active");
    expect(peakOf(silence.left, silence.left.size() / 2) < sounding * 0.01,
           "the hard stop left something sounding");
}

void testLoweringTheVoiceCountLetsNotesFinish()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.5f;
    engine.setParameters(parameters);

    for (int note = 60; note < 66; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6, "six keys did not take six voices");

    // Reducing the count must stop new allocations, not freeze the voices that
    // are already sounding -- a frozen voice never retires and would come back
    // audibly if the count were raised again.
    parameters.polyphony = 2;
    engine.setParameters(parameters);
    for (int note = 60; note < 66; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(48000.0 * 4.0));
    expect(engine.getActiveVoiceCount() == 0,
           "voices outside a reduced voice count never finished");

    parameters.polyphony = 6;
    engine.setParameters(parameters);
    const auto after = render(engine, 8192);
    expect(peakOf(after.left, 0) < 1.0e-4,
           "raising the voice count resumed a stale note");
}

void testTransposeReachesSoundingVoices()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.3));

    // Moving the transpose control has to take a note that is already sounding
    // with it, not wait for the next keypress.
    parameters.keyTranspose = 12;
    engine.setParameters(parameters);
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.5));
    expectNear(measuredFrequency(rendered.left, rendered.left.size() / 2, sampleRate),
               880.0, 3.0,
               "transposing while a note was held left it at its old pitch");
}

void testFirstGlidedNoteStartsAtItsOwnPitch()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.portamento = 1.0f;  // the slowest glide there is
    engine.setParameters(parameters);

    // With nothing to glide from, the first note must simply be at its pitch.
    // Starting it wherever the voice happened to be constructed would make it
    // crawl there over several seconds.
    engine.noteOn(72, 1.0f);
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.4));
    expectNear(measuredFrequency(rendered.left, rendered.left.size() / 2, sampleRate),
               523.25, 4.0,
               "the first note under portamento glided from somewhere else");
}

void testVoicesRetireWithComponentToleranceApplied()
{
    // A voice card's own amplifier offset can sit above any small raw control
    // threshold while still being inside the converter's deadband. Such a voice
    // is silent, and it has to retire, or it would be processed forever and
    // would block a deferred quality change.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.calibration = 1.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    for (int note = 60; note < 66; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 6, "six keys did not take six voices");

    for (int note = 60; note < 66; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(48000.0 * 2.0));
    expect(engine.getActiveVoiceCount() == 0,
           "voices with a component tolerance offset never retired");

    // And with nothing left running, a deferred quality change can apply.
    expect(engine.setOversamplingEnabled(false),
           "a quality change stayed deferred with no voice sounding");
}

void testVoiceCountAboveTheHardwareSixWorks()
{
    // The voice count is one of the controls the hardware does not have, and it
    // genuinely reaches past six -- this asserts the extra voices sound rather
    // than being an inert range on the parameter.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.polyphony = 12;
    engine.setParameters(parameters);

    for (int note = 48; note < 60; ++note)
        engine.noteOn(note, 1.0f);
    const auto rendered = render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 12,
           "a voice count above six did not allocate the extra voices");
    expect(peakOf(rendered.left, 0) > 0.0, "the extra voices produced no sound");

    // And the thirteenth key is still dropped, because the count is the limit.
    engine.noteOn(60, 1.0f);
    render(engine, 1024);
    expect(engine.getActiveVoiceCount() == 12,
           "the key assigner exceeded the voice count it was given");
}

void testUnisonSurvivesAReducedVoiceCount()
{
    // Lowering the count leaves earlier voices sounding, and a unison retarget
    // has to reach them too or they stay keyed to a note nobody is holding.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6, "unison did not take every voice");

    parameters.polyphony = 1;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    render(engine, 4096);
    engine.noteOff(64);
    render(engine, static_cast<int>(48000.0 * 2.0));

    expect(engine.getActiveVoiceCount() == 0,
           "reducing the voice count left unison voices stuck on a released key");
}

void testUnisonNoteOnCollapsesAWiderStack()
{
    // Unison is one note on every voice it is given. A count lowered while a
    // wider stack is sounding must not leave the old note held against the new
    // one, which would turn the mode into a two-note chord.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 6, "unison did not take every voice");

    parameters.polyphony = 1;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    // Long enough for the five dropped voices to finish their release.
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.5));

    const std::size_t window = rendered.left.size() / 2;
    const double atOldNote =
        magnitudeAt(rendered.left, rendered.left.size() - window,
                    static_cast<int>(window), 261.626, sampleRate);
    const double atNewNote =
        magnitudeAt(rendered.left, rendered.left.size() - window,
                    static_cast<int>(window), 329.628, sampleRate);
    expect(atNewNote > 0.01, "the new unison note is not sounding");
    expect(atOldNote < atNewNote * 0.05,
           "a unison note-on left the previous note sounding on the dropped voices");
    expect(engine.getActiveVoiceCount() == 1,
           "the dropped unison voices never retired");
}

void testUnisonDoesNotResurrectPolyphonicTails()
{
    // Release tails from before the mode changed are not part of the unison
    // stack. Sweeping them into a retarget would key them to a note they never
    // played and pull their envelopes back out of release, so they would ring
    // on for good and push the instrument past the voice count it was given.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    // Fixed priority from the first voice down, so the four notes land in the
    // four lowest slots and the two the unison stack will take are known.
    parameters.keyMode = KeyMode::Poly2;
    parameters.polyphony = 6;
    // Long enough that the tails are unmistakably still ringing when the mode
    // changes underneath them, short enough to be gone well inside the final
    // render if nothing revives them.
    parameters.release = 0.6f;
    engine.setParameters(parameters);

    for (int note = 48; note < 52; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 8192);
    for (int note = 48; note < 52; ++note)
        engine.noteOff(note);
    render(engine, 2048);
    expect(engine.getActiveVoiceCount() == 4,
           "the polyphonic tails are not ringing");

    // The stack is narrower than the tails, so slots 2 and 3 stay outside it.
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 2;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    render(engine, 2048);
    engine.noteOn(65, 1.0f);
    render(engine, 2048);
    engine.noteOff(65);
    render(engine, static_cast<int>(sampleRate * 3.0));

    expect(engine.getActiveVoiceCount() == 2,
           "a unison retarget resurrected a polyphonic release tail");
}

void testSubFlipsOnTheFirstWrap()
{
    // The divider's first output edge belongs one oscillator period after the
    // retrigger, not two. Getting that wrong makes the opening half-cycle twice
    // as long as every one after it -- a low transient the hardware does not
    // have. A low note is used so the anomaly is far clear of the amplifier's
    // own opening time.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 1.0f;
    engine.setParameters(parameters);

    constexpr int midiNote = 24;
    const double fundamental = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    const double periodSamples = sampleRate / fundamental;

    engine.noteOn(midiNote, 1.0f);
    const auto rendered = render(engine, static_cast<int>(periodSamples * 3.0));

    // The first falling edge after the amplifier has opened.
    const auto opened = static_cast<std::size_t>(sampleRate * 0.008);
    std::size_t firstFall = 0;
    for (std::size_t index = opened + 1; index < rendered.left.size(); ++index)
        if (rendered.left[index - 1] > 0.0f && rendered.left[index] <= 0.0f)
        {
            firstFall = index;
            break;
        }

    expect(firstFall > 0, "the sub never changed sign");
    const double periods = static_cast<double>(firstFall) / periodSamples;
    expect(periods < 1.5,
           "the sub's first half-cycle lasted two oscillator periods instead of one");
}

void testControllersReturnToNeutralOnReset()
{
    // A run that stopped with the bender pushed over must not start the next
    // one there. Hosts are not obliged to resend a neutral controller when the
    // transport restarts, and nothing else would bring the pitch back.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.benderDcoDepth = 1.0f;
    engine.setParameters(parameters);

    engine.setPitchBend(1.0f);
    engine.noteOn(69, 1.0f);
    const auto bent = render(engine, static_cast<int>(sampleRate));
    const double bentPitch =
        measuredFrequency(bent.left, bent.left.size() / 2, sampleRate);
    expect(bentPitch > 500.0, "the bender did not raise the pitch");

    engine.reset();
    engine.noteOn(69, 1.0f);
    const auto afterReset = render(engine, static_cast<int>(sampleRate));
    const double resetPitch = measuredFrequency(afterReset.left,
                                                afterReset.left.size() / 2, sampleRate);
    expectNear(resetPitch, 440.0, 1.0,
               "a reset left the previous run's pitch bend applied");
}

void testContinuousControlsDoNotStepAtBlockBoundaries()
{
    // Volume, sub and noise reach the samples directly rather than through the
    // converter scan, so a host automating one of them across a held note would
    // hear a block-rate step as a click unless they are glided.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    engine.noteOn(45, 1.0f);
    render(engine, 8192);

    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    engine.process(left.data(), right.data(), blockSize);

    // The largest step the signal takes on its own, as the reference for what
    // counts as a discontinuity.
    double withinBlock = 0.0;
    for (int index = 1; index < blockSize; ++index)
        withinBlock = std::max(withinBlock,
                               std::abs(static_cast<double>(left[index])
                                        - left[index - 1]));
    const float lastOfBlock = left[blockSize - 1];

    // The whole range, in one automation move at a block boundary.
    parameters.volume = 0.0f;
    engine.setParameters(parameters);
    engine.process(left.data(), right.data(), blockSize);

    const double acrossBoundary =
        std::abs(static_cast<double>(left[0]) - lastOfBlock);
    expect(acrossBoundary <= withinBlock * 1.5 + 1.0e-4,
           "a block-boundary volume change stepped the output");

    // And it still gets there: a glide that never arrives is not a fix. Four
    // time constants is long enough to be unambiguous and short enough that a
    // glide slow enough to be audible as a fade would fail.
    const auto settled = render(engine, static_cast<int>(sampleRate * 0.02));
    expect(peakOf(settled.left, settled.left.size() / 2) < 0.02,
           "the glided volume never reached its new setting");
}

void testNotesWaitForTheSharedConverterScan()
{
    // One converter serves every voice, so a key struck between passes cannot
    // be heard until the next one reaches it -- and two keys struck inside the
    // same pass speak together rather than a fraction of a scan apart. A
    // per-voice schedule started at each note-on gives neither.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    // No oversampling, so the internal rate is the host rate and the pass
    // interval can be counted in output samples directly.
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    engine.setParameters(parameters);

    // The documented converter pass, taken from the service note rather than
    // from the engine, so this measures the model against the figure it claims.
    const int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    expect(scanPeriod > 150, "the scan interval is too short for this fixture");

    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    const auto run = [&](int count) {
        engine.process(left.data(), right.data(), count);
        return peakOf(std::vector<float>(left.begin(), left.begin() + count), 0);
    };

    // The first pass falls on the first rendered sample, so this lands the
    // playhead midway between passes with the next one still ahead.
    run(scanPeriod / 2);

    engine.noteOn(60, 1.0f);
    const double duringGap = run(scanPeriod / 4);
    engine.noteOn(67, 1.0f);
    const double stillWaiting = run(scanPeriod / 8);

    expectNear(duringGap, 0.0, 1.0e-9,
               "a note sounded before the converter scan reached it");
    expectNear(stillWaiting, 0.0, 1.0e-9,
               "the second note did not wait for the same pass as the first");

    // And once the pass arrives, both are speaking.
    const double afterScan = run(scanPeriod);
    expect(afterScan > 0.01, "the notes never sounded after the scan pass");
}

void testPortamentoRateFollowsItsControl()
{
    // The glide rate is set by a control in the pitch integrator's path, not
    // latched when the key went down. Turning it down mid-glide has to shorten
    // the slide, and turning it off has to land the note at the next scan.
    constexpr double sampleRate = 96000.0;
    const auto glideTo = [&](bool switchOffMidway) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        // Unison keeps it monophonic, so the glide is one unambiguous slide.
        parameters.keyMode = KeyMode::Unison;
        parameters.portamento = 0.8f;
        engine.setParameters(parameters);

        engine.noteOn(48, 1.0f);
        render(engine, static_cast<int>(sampleRate * 0.3));
        engine.noteOn(72, 1.0f);
        const auto midway = render(engine, static_cast<int>(sampleRate * 0.1));
        const double partway = measuredFrequency(midway.left, midway.left.size() / 2,
                                                 sampleRate);

        if (switchOffMidway)
        {
            parameters.portamento = 0.0f;
            engine.setParameters(parameters);
        }
        const auto after = render(engine, static_cast<int>(sampleRate * 0.05));
        return std::pair<double, double> {
            partway, measuredFrequency(after.left, after.left.size() / 2, sampleRate) };
    };

    const double destination = 440.0 * std::pow(2.0, (72 - 69) / 12.0);
    const auto leftRunning = glideTo(false);
    const auto switchedOff = glideTo(true);

    // The fixture is only meaningful if the note really is still gliding when
    // the control is moved.
    expect(leftRunning.first < destination * 0.9,
           "the glide had already finished before the control was moved");
    expect(leftRunning.second < destination * 0.95,
           "the glide finished on its own, so switching it off proves nothing");
    expectNear(switchedOff.second, destination, destination * 0.01,
               "turning portamento off mid-glide did not land the note");
}

void testOscillatorSurvivesMoreThanOneCyclePerSample()
{
    // The note timer's divider bottoms out at eight, so the top range reaches
    // half a megahertz -- far above any rate the engine runs at. Nothing about
    // a waveform that fast is representable, and this does not pretend to
    // check its pitch. What it does check is that the oscillator walks every
    // crossed cycle instead of collapsing them: the state machine stays
    // consistent, the output stays finite and bounded, and the per-sample loop
    // cannot run away.
    for (double sampleRate : { 8000.0, 48000.0 })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.range = DcoRange::Four;
        parameters.keyTranspose = 12;
        parameters.pulseEnabled = true;
        parameters.subLevel = 1.0f;
        engine.setParameters(parameters);

        for (int note = 120; note <= 127; ++note)
            engine.noteOn(note, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 0.25));

        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            if (!std::isfinite(rendered.left[index])
                || !std::isfinite(rendered.right[index]))
            {
                expect(false, "the oscillator produced a non-finite sample above "
                              "one cycle per sample");
                break;
            }
        expect(peakOf(rendered.left, 0) < 4.0,
               "the oscillator ran away above one cycle per sample");
    }
}

void testModulationDelayRearmsForANewPhrase()
{
    // The delay is per phrase, and a phrase begins when nothing is sounding and
    // no key is down. The last voice can retire part-way through a render call,
    // so waiting for an idle scan pass to notice would let the next note start
    // with the modulation already faded all the way in.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.lfoDelay = 0.7f;
    parameters.dcoLfoDepth = 1.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    // Long enough for the delay to finish and the modulation to reach full.
    render(engine, static_cast<int>(sampleRate * 4.0));
    const double atFullDepth = std::abs(engine.getDisplayLfo());
    expect(atFullDepth > 0.05, "the modulation never faded in");

    engine.noteOff(60);
    // Exactly one block past the retirement, so the idle scan branch has had no
    // block of its own in which to notice.
    for (int block = 0; block < 200 && engine.getActiveVoiceCount() > 0; ++block)
        render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 0, "the voice never retired");

    engine.noteOn(67, 1.0f);
    render(engine, blockSize);
    expect(std::abs(engine.getDisplayLfo()) < atFullDepth * 0.25,
           "a new phrase started with the previous phrase's modulation depth");
}

void testScanTimersRestartWithTheProcessingRate()
{
    // Every countdown is in internal samples, so one carried across a rate
    // change means something else at the new rate: a scan pass 806 samples away
    // is 4.2 ms at 192 kHz and 16.8 ms at 48 kHz. A note in the block that
    // applies the change would wait that stale interval to speak.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    expect(engine.getOversamplingFactor() > 1, "the fixture needs the deep setting");

    // Idle, so the change goes through, and stopped part-way between passes.
    render(engine, 64);
    expect(engine.setOversamplingEnabled(false),
           "the quality change did not apply on an idle engine");
    expect(engine.getOversamplingFactor() == 1, "the rate did not drop");

    engine.noteOn(60, 1.0f);
    // One documented scan interval, plus the amplifier's own slew.
    const auto onset = render(engine, static_cast<int>(sampleRate * 0.007));
    expect(peakOf(onset.left, 0) > 0.001,
           "a note after a rate change waited a stale scan interval to speak");
}

void testUnisonStackGlidesFromOneOrigin()
{
    // The stack is one note, so widening it mid-performance must not leave the
    // slots that had been idle starting where the others are still heading.
    // They all glide from the same place or the mode is not unison.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 1;
    parameters.portamento = 0.8f;
    engine.setParameters(parameters);

    engine.noteOn(48, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.3));

    // Widen the stack, then move: five slots wake up for this note.
    parameters.polyphony = 6;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    const auto gliding = render(engine, static_cast<int>(sampleRate * 0.1));
    expect(engine.getActiveVoiceCount() == 6, "the stack did not widen");

    const std::size_t window = gliding.left.size() / 2;
    const double destination = 440.0 * std::pow(2.0, (64 - 69) / 12.0);
    const double atDestination =
        magnitudeAt(gliding.left, gliding.left.size() - window,
                    static_cast<int>(window), destination, sampleRate);
    const double sounding = peakOf(gliding.left, gliding.left.size() - window);
    expect(sounding > 0.01, "the widened stack is not sounding at all");
    expect(atDestination < sounding * 0.1,
           "part of the unison stack jumped to the new note instead of gliding");
}

void testQualityChangeWaitsForTheOutputPathToEmpty()
{
    // The last voice retiring is not the instrument going quiet: the delay
    // lines still hold several milliseconds of it. Switching the processing
    // rate empties them, so the switch has to wait for them to run dry.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Two;
    engine.setParameters(parameters);
    const int deepFactor = engine.getOversamplingFactor();
    expect(deepFactor > 1, "the fixture needs the deeper quality setting running");

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    engine.noteOff(60);
    // Just past the point where the voices retire, with the lines still full.
    for (int block = 0; block < 40 && engine.getActiveVoiceCount() > 0; ++block)
        render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 0, "the voice never retired");

    expect(!engine.setOversamplingEnabled(false),
           "the quality change applied while the delay lines were still full");
    render(engine, blockSize);
    expect(engine.getOversamplingFactor() == deepFactor,
           "the quality change took effect before the output path had emptied");

    // Once they have run dry it goes through by itself.
    render(engine, static_cast<int>(sampleRate * 0.1));
    expect(engine.getOversamplingFactor() == 1,
           "the quality change never applied after the output path emptied");
}

void testHardStopSilencesTheWholeOutputPath()
{
    // All Sound Off and Panic mean silent now. Cutting the voices alone leaves
    // the delay lines playing back the last few milliseconds of the chord.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Two;
    // The modelled line hiss would mask the thing being measured, and it is
    // not what the hard stop is being asked to remove.
    parameters.chorusNoise = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    const auto sounding = render(engine, 8192);
    expect(peakOf(sounding.left, sounding.left.size() / 2) > 0.01,
           "the fixture never made a sound to stop");

    engine.allNotesOff();
    const auto afterStop = render(engine, blockSize * 4);
    expect(peakOf(afterStop.left, 0) < 1.0e-4,
           "a hard stop left the delay lines playing the note back");
    expect(peakOf(afterStop.right, 0) < 1.0e-4,
           "a hard stop left the right channel ringing");
}

void testComponentDriftRateIsIndependentOfOversampling()
{
    // The modelled component wander has to advance in seconds, not in internal
    // samples, or the same patch would drift four times faster with the quality
    // setting on.
    const auto wander = [](bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.cutoff = 0.45f;
        parameters.resonance = 0.5f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        const auto rendered = render(engine, static_cast<int>(48000.0 * 3.0));

        // Energy below a few hertz in the envelope of the output is what the
        // wander shows up as; compare its scale between the two settings.
        double previous = 0.0;
        double movement = 0.0;
        for (std::size_t index = rendered.left.size() / 3;
             index < rendered.left.size(); index += 480)
        {
            double peak = 0.0;
            for (std::size_t inner = index;
                 inner < std::min(index + 480, rendered.left.size()); ++inner)
                peak = std::max(peak, static_cast<double>(std::abs(rendered.left[inner])));
            if (previous > 0.0)
                movement += std::abs(peak - previous);
            previous = peak;
        }
        return movement;
    };

    const double deep = wander(true);
    const double shallow = wander(false);
    expect(deep > 0.0 && shallow > 0.0, "no component wander was measurable");
    const double ratio = deep / shallow;
    expect(ratio > 0.4 && ratio < 2.5,
           "component wander advances at a different rate with oversampling on ("
               + std::to_string(ratio) + "x)");
}

void testEnvelopeAndGateModes()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.attack = 0.0f;
    parameters.decay = 0.6f;
    parameters.sustain = 0.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    const auto decaying = render(engine, static_cast<int>(sampleRate * 2));

    // The generator is linear and the amplifier exponential, so equal slices of
    // time must fall by roughly equal numbers of decibels while the segment
    // runs.
    const auto levelAt = [&](double seconds) {
        const auto start = static_cast<std::size_t>(seconds * sampleRate);
        return 20.0 * std::log10(peakOf({ decaying.left.begin() + static_cast<long>(start),
                                          decaying.left.begin()
                                              + static_cast<long>(start + 1200) }, 0)
                                 + 1.0e-12);
    };
    const double first = levelAt(0.05) - levelAt(0.15);
    const double second = levelAt(0.15) - levelAt(0.25);
    expect(first > 3.0, "the decay segment is not falling");
    expectNear(second / first, 1.0, 0.45,
               "amplitude decay is not close to constant decibels per second");

    // Gate mode ignores the generator's shape entirely.
    YouKnow106Engine gated;
    gated.prepare(sampleRate, blockSize, true);
    parameters.vcaMode = VcaMode::Gate;
    gated.setParameters(parameters);
    gated.noteOn(60, 1.0f);
    const auto held = render(gated, static_cast<int>(sampleRate));
    expect(peakOf(held.left, held.left.size() / 2) > 0.05,
           "gate mode falls silent while the key is held");
}

void testChorusWidthAndSilence()
{
    constexpr double sampleRate = 48000.0;

    const auto sideEnergy = [&](ChorusMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = mode;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(52, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 2));

        double side = 0.0;
        double mid = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
        {
            const double difference = rendered.left[index] - rendered.right[index];
            const double sum = rendered.left[index] + rendered.right[index];
            side += difference * difference;
            mid += sum * sum;
        }
        return std::sqrt(side / std::max(mid, 1.0e-12));
    };

    expect(sideEnergy(ChorusMode::Off) < 1.0e-6,
           "the output is not mono with the effect switched out");
    expect(sideEnergy(ChorusMode::One) > 0.05, "the first mode produces no width");
    expect(sideEnergy(ChorusMode::Two) > 0.05, "the second mode produces no width");
}

void testChorusNoiseIsPresentAndDefeatable()
{
    constexpr double sampleRate = 48000.0;

    const auto idleNoise = [&](float scale) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = ChorusMode::Two;
        parameters.chorusNoise = scale;
        engine.setParameters(parameters);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() / 2) + 1.0e-30);
    };

    const double modelled = idleNoise(1.0f);
    expect(modelled > -85.0 && modelled < -55.0,
           "the delay lines' own noise floor is outside the measured band");
    expect(idleNoise(0.0f) < -120.0,
           "the delay lines still hiss with their noise switched out");
}

void testSampleRateAndOversamplingConsistency()
{
    const auto levelAt = [](double sampleRate, bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.cutoff = 0.7f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        const auto from = rendered.left.size() / 2;
        for (std::size_t index = from; index < rendered.left.size(); ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() - from) + 1.0e-30);
    };

    const double reference = levelAt(48000.0, true);
    for (double sampleRate : { 44100.0, 88200.0, 96000.0, 192000.0 })
        expectNear(levelAt(sampleRate, true), reference, 1.0,
                   "output level moves with the host sample rate");
    expectNear(levelAt(48000.0, false), reference, 1.5,
               "output level moves when oversampling is switched off");

    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 4, "48 kHz does not run oversampled");
    const int reported = engine.getProcessingLatencySamples();
    expect(reported > 0, "an oversampled engine reports no latency");

    // The reported figure has to stay put across every configuration: the
    // quality setting can move while the host is playing, and a plug-in that
    // renegotiated its latency mid-transport would make the host re-align
    // everything around it.
    engine.prepare(192000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 1,
           "a high-rate host is oversampled unnecessarily");
    expect(engine.getProcessingLatencySamples() == reported,
           "the reported latency moved with the oversampling factor");
    engine.prepare(48000.0, blockSize, false);
    expect(engine.getProcessingLatencySamples() == reported,
           "the reported latency moved when oversampling was switched off");

    // And the shallower configurations must actually be padded out to it,
    // otherwise the constant figure would be a lie the host acts on.
    const auto onsetOffset = [](bool oversampled) {
        YouKnow106Engine local;
        local.prepare(48000.0, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.attack = 0.0f;
        local.setParameters(parameters);
        local.noteOn(60, 1.0f);
        const auto rendered = render(local, 4096);
        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            if (std::abs(rendered.left[index]) > 1.0e-4f)
                return static_cast<int>(index);
        return -1;
    };
    const int deep = onsetOffset(true);
    const int shallow = onsetOffset(false);
    expect(deep >= 0 && shallow >= 0, "a configuration produced no onset at all");
    expect(std::abs(deep - shallow) <= 2,
           "the two configurations do not share an onset, so the padding is wrong");
}

void testDeterminismAndSilence()
{
    const auto run = []() {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.noiseLevel = 0.4f;
        parameters.chorus = ChorusMode::Two;
        engine.setParameters(parameters);
        engine.noteOn(64, 1.0f);
        return render(engine, 24000);
    };

    const auto first = run();
    const auto second = run();
    expect(first.left == second.left && first.right == second.right,
           "the engine is not deterministic");

    // With nothing playing and the effect switched out, the output has to be
    // exactly zero rather than merely small.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    engine.setParameters(plainPatch());
    const auto silent = render(engine, 24000);
    expect(peakOf(silent.left, 0) == 0.0 && peakOf(silent.right, 0) == 0.0,
           "an idle engine is not exactly silent");
}

void testExtremeAutomationStaysFinite()
{
    YouKnow106Engine engine;
    engine.prepare(44100.0, blockSize, true);

    std::uint32_t state = 12345u;
    const auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xffffffu) / 16777215.0f;
    };

    bool finite = true;
    double peak = 0.0;
    for (int block = 0; block < 400; ++block)
    {
        EngineParameters parameters;
        parameters.lfoRate = next();
        parameters.lfoDelay = next();
        parameters.dcoLfoDepth = next();
        parameters.pwmDepth = next();
        parameters.pwmSource = next() < 0.5f ? PwmSource::Lfo : PwmSource::Manual;
        parameters.range = static_cast<DcoRange>(static_cast<int>(next() * 2.99f));
        parameters.sawEnabled = next() < 0.7f;
        parameters.pulseEnabled = next() < 0.7f;
        parameters.subLevel = next();
        parameters.noiseLevel = next();
        parameters.highPass = static_cast<HighPassMode>(static_cast<int>(next() * 3.99f));
        parameters.cutoff = next();
        parameters.resonance = next();
        parameters.envDepth = next();
        parameters.vcfLfoDepth = next();
        parameters.keyFollow = next();
        parameters.attack = next();
        parameters.decay = next();
        parameters.sustain = next();
        parameters.release = next();
        parameters.chorus = static_cast<ChorusMode>(static_cast<int>(next() * 2.99f));
        parameters.portamento = next();
        parameters.calibration = next();
        parameters.volume = 1.0f;
        parameters.vcaLevel = 1.0f;
        engine.setParameters(parameters);
        engine.setPitchBend(next() * 2.0f - 1.0f);
        engine.setModWheel(next());

        if (block % 5 == 0)
            engine.noteOn(36 + static_cast<int>(next() * 60.0f), next());
        if (block % 7 == 0)
            engine.noteOff(36 + static_cast<int>(next() * 60.0f));

        const auto rendered = render(engine, blockSize);
        for (const auto sample : rendered.left)
        {
            finite = finite && std::isfinite(sample);
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
        }
    }

    expect(finite, "extreme automation produced a non-finite sample");
    expect(peak < 4.0, "extreme automation produced an unbounded output");
}

void testParameterSanitisation()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);

    EngineParameters parameters = plainPatch();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    parameters.cutoff = nan;
    parameters.resonance = std::numeric_limits<float>::infinity();
    parameters.attack = -5.0f;
    parameters.sustain = 12.0f;
    parameters.masterTuneCents = 900.0f;
    parameters.polyphony = 500;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    const auto rendered = render(engine, 8192);
    bool finite = true;
    for (const auto sample : rendered.left)
        finite = finite && std::isfinite(sample);
    expect(finite, "hostile parameter values reached the audio path");
    expect(engine.getActiveVoiceCount() >= 1,
           "the engine stopped sounding after hostile parameters");
}

void testSustainPedalHoldsAndReleases()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.setSustainPedal(true);
    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 1,
           "the pedal did not hold the note");

    engine.setSustainPedal(false);
    render(engine, 24000);
    expect(engine.getActiveVoiceCount() == 0,
           "the note did not release when the pedal was lifted");
}

void testPanelLayout()
{
    expect(panel::layoutIsConsistent(),
           "the panel description overlaps, escapes its section, or has a broken group");
    expect(panel::panelWidth() > 0.0f, "the panel has no width");

    const auto& controls = panel::controls();
    expect(controls.size() == panel::controlCount, "panel control count");

    // Every front-panel control must name a parameter, and every parameter the
    // panel names must be reachable from exactly one slider or one radio group.
    for (const auto& control : controls)
    {
        expect(control.parameterId != nullptr && std::strlen(control.parameterId) > 0,
               "a panel control names no parameter");
        expect(control.label != nullptr && std::strlen(control.label) > 0,
               "a panel control carries no legend");
    }

    const std::array<const char*, 8> mustAppear {
        parameters::cutoff, parameters::resonance, parameters::attack,
        parameters::release, parameters::chorus, parameters::range,
        parameters::highPass, parameters::vcaLevel
    };
    for (const auto* wanted : mustAppear)
    {
        int found = 0;
        for (const auto& control : controls)
            if (std::strcmp(control.parameterId, wanted) == 0)
                ++found;
        expect(found >= 1, std::string("panel does not expose ") + wanted);
    }

    // Section accents must alternate, which is what makes the panel readable.
    const auto& sections = panel::sections();
    for (std::size_t index = 1; index < sections.size(); ++index)
        expect(sections[index].accent != sections[index - 1].accent,
               "two neighbouring sections share a highlight colour");
}

void testCpuBudget()
{
    // Not a benchmark of the host, just a guard against a change that makes the
    // engine an order of magnitude more expensive without anyone noticing.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.pulseEnabled = true;
    parameters.subLevel = 0.5f;
    parameters.noiseLevel = 0.1f;
    parameters.resonance = 0.7f;
    parameters.chorus = ChorusMode::Two;
    engine.setParameters(parameters);
    for (int note = 0; note < 6; ++note)
        engine.noteOn(48 + note * 4, 1.0f);

    const auto started = std::chrono::steady_clock::now();
    const auto rendered = render(engine, static_cast<int>(sampleRate * 2));
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - started;
    const double realtimeRatio = elapsed.count() / 2.0;

    expect(peakOf(rendered.left, 0) > 0.0, "the benchmark patch produced silence");
    // Instrumentation costs roughly an order of magnitude, so an instrumented
    // build cannot meet a realtime target. The CMake timeout already allows for
    // that; a looser ceiling keeps the runaway guard without making sanitizer
    // runs impossible.
    const double ceiling = sanitizerBuild ? 12.0 : 1.0;
    expect(realtimeRatio < ceiling,
           "six voices with the effect engaged cost "
               + std::to_string(realtimeRatio) + "x realtime");
}
} // namespace

int main()
{
    testRangeTransposesByOctaves();
    testSubIsOneOctaveDown();
    testAliasFloor();
    testRampHasARampSpectrum();
    testKeyAssignerDropsRatherThanSteals();
    testPolyModesDifferInAllocation();
    testUnisonUsesEveryVoiceWithoutDetuning();
    testVoiceCountAboveTheHardwareSixWorks();
    testUnisonSurvivesAReducedVoiceCount();
    testUnisonNoteOnCollapsesAWiderStack();
    testUnisonDoesNotResurrectPolyphonicTails();
    testSubFlipsOnTheFirstWrap();
    testControllersReturnToNeutralOnReset();
    testContinuousControlsDoNotStepAtBlockBoundaries();
    testNotesWaitForTheSharedConverterScan();
    testPortamentoRateFollowsItsControl();
    testOscillatorSurvivesMoreThanOneCyclePerSample();
    testModulationDelayRearmsForANewPhrase();
    testScanTimersRestartWithTheProcessingRate();
    testUnisonStackGlidesFromOneOrigin();
    testQualityChangeWaitsForTheOutputPathToEmpty();
    testHardStopSilencesTheWholeOutputPath();
    testComponentDriftRateIsIndependentOfOversampling();
    testTransposeReachesSoundingVoices();
    testFirstGlidedNoteStartsAtItsOwnPitch();
    testVoicesRetireWithComponentToleranceApplied();
    testUnisonReturnsToAHeldKey();
    testAllNotesOffReleasesRatherThanCutting();
    testLoweringTheVoiceCountLetsNotesFinish();
    testEnvelopeAndGateModes();
    testChorusWidthAndSilence();
    testChorusNoiseIsPresentAndDefeatable();
    testSampleRateAndOversamplingConsistency();
    testDeterminismAndSilence();
    testExtremeAutomationStaysFinite();
    testParameterSanitisation();
    testSustainPedalHoldsAndReleases();
    testPanelLayout();
    testCpuBudget();

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 engine check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 engine checks passed.\n";
    return EXIT_SUCCESS;
}

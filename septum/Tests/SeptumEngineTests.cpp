// JUCE-free engine tests: the documented architecture claims that can be
// checked without hardware are checked here — tuning against master tune,
// the settled enumeration semantics (balance endpoints, ring on the OSC1
// leg, sync tracking OSC2, split routing, DUAL halving polyphony), the
// mapping laws' endpoints, self-oscillation boundedness, effect tails, and
// finite, DC-free output for the whole factory bank at two sample rates.

#include "DSP/SeptumEngine.h"
#include "DSP/SeptumPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

// MSVC does not define M_PI without _USE_MATH_DEFINES, and the rest of this
// suite already spells pi out.
constexpr double twoPi = 2.0 * 3.14159265358979323846;

void expect (bool condition, const std::string& message)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::fprintf (stderr, "FAIL: %s\n", message.c_str());
    }
}

void expectNear (double value, double target, double tolerance,
                 const std::string& message)
{
    expect (std::abs (value - target) <= tolerance,
            message + " (value " + std::to_string (value) + ", target "
                + std::to_string (target) + ")");
}

struct Render
{
    std::vector<float> left, right;

    [[nodiscard]] double peak() const
    {
        double result = 0.0;
        for (std::size_t i = 0; i < left.size(); ++i)
            result = std::max ({ result, std::abs ((double) left[i]),
                                 std::abs ((double) right[i]) });
        return result;
    }

    [[nodiscard]] double rms (std::size_t from, std::size_t to) const
    {
        to = std::min (to, left.size());
        if (from >= to)
            return 0.0;
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += 0.5 * (left[i] * (double) left[i] + right[i] * (double) right[i]);
        return std::sqrt (sum / (double) (to - from));
    }

    [[nodiscard]] double mean() const
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < left.size(); ++i)
            sum += 0.5 * ((double) left[i] + (double) right[i]);
        return left.empty() ? 0.0 : sum / (double) left.size();
    }

    [[nodiscard]] bool finite() const
    {
        for (std::size_t i = 0; i < left.size(); ++i)
            if (! std::isfinite (left[i]) || ! std::isfinite (right[i]))
                return false;
        return true;
    }
};

struct Event
{
    double seconds;
    bool on;
    int note;
    int velocity;
};

Render renderScore (septum::Engine& engine, const std::vector<Event>& events,
                    double seconds, double sampleRate = 44100.0)
{
    const int block = 256;
    const auto total = (std::size_t) std::llround (seconds * sampleRate);
    Render out;
    out.left.assign (total, 0.0f);
    out.right.assign (total, 0.0f);

    std::size_t eventIndex = 0;
    std::vector<Event> sorted = events;
    std::sort (sorted.begin(), sorted.end(),
               [] (const Event& a, const Event& b) { return a.seconds < b.seconds; });

    std::size_t position = 0;
    while (position < total)
    {
        while (eventIndex < sorted.size()
               && (std::size_t) std::llround (sorted[eventIndex].seconds * sampleRate)
                      <= position)
        {
            const auto& event = sorted[eventIndex];
            if (event.on)
                engine.noteOn (event.note, event.velocity);
            else
                engine.noteOff (event.note);
            ++eventIndex;
        }
        const int count = (int) std::min ((std::size_t) block, total - position);
        engine.process (out.left.data() + position, out.right.data() + position,
                        count);
        position += (std::size_t) count;
    }
    return out;
}

double goertzel (const std::vector<float>& x, std::size_t from, std::size_t to,
                 double hz, double sampleRate)
{
    to = std::min (to, x.size());
    if (from >= to)
        return 0.0;
    const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::sqrt (std::max (0.0, power)) / (double) (to - from);
}

// Fundamental estimate from autocorrelation over a held segment.
double estimateFundamental (const std::vector<float>& x, std::size_t from,
                            std::size_t to, double sampleRate, double minHz,
                            double maxHz)
{
    to = std::min (to, x.size());
    const auto minLag = (std::size_t) (sampleRate / maxHz);
    const auto maxLag = (std::size_t) (sampleRate / minHz);
    double bestValue = -1.0;
    std::size_t bestLag = minLag;
    for (std::size_t lag = minLag; lag <= maxLag && from + lag < to; ++lag)
    {
        double sum = 0.0;
        for (std::size_t i = from; i + lag < to; ++i)
            sum += x[i] * (double) x[i + lag];
        if (sum > bestValue)
        {
            bestValue = sum;
            bestLag = lag;
        }
    }
    // Parabolic refinement around the best integer lag.
    if (bestLag > minLag && bestLag < maxLag)
    {
        const auto at = [&] (std::size_t lag)
        {
            double sum = 0.0;
            for (std::size_t i = from; i + lag < to; ++i)
                sum += x[i] * (double) x[i + lag];
            return sum;
        };
        const double ym = at (bestLag - 1), y0 = at (bestLag), yp = at (bestLag + 1);
        const double denom = ym - 2.0 * y0 + yp;
        const double shift = denom == 0.0 ? 0.0 : 0.5 * (ym - yp) / denom;
        return sampleRate / ((double) bestLag + shift);
    }
    return sampleRate / (double) bestLag;
}

septum::Patch plainSawPatch()
{
    septum::Patch patch = septum::initPatch();
    patch.upper.filterType = septum::FilterType::Bypass;
    patch.upper.ampEnvAttack = 0;
    patch.upper.ampEnvSustain = 127;
    patch.upper.ampEnvRelease = 0;
    return patch;
}

// ---------------------------------------------------------------------------

void testMappingLaws()
{
    using namespace septum::mapping;
    expectNear (cutoffHz (0), 20.0, 1.0e-9, "cutoff floor is 20 Hz");
    expectNear (cutoffHz (127), 20480.0, 1.0e-6, "cutoff ceiling is 20.48 kHz");
    expectNear (superSawDetuneAmount (1.0), 1.0, 0.02,
                "Szabo polynomial reaches ~1 at full detune");
    expect (superSawDetuneAmount (0.0) < 0.01,
            "Szabo polynomial is near zero at zero detune");
    expect (superSawDetuneAmount (0.496) > 0.05
                && superSawDetuneAmount (0.496) < 0.15,
            "Szabo polynomial mid-point matches the thesis sample (~0.0967)");
    // Tempo sync: at 120 BPM a 1/4-whole-note (one quarter) cycle is 2 Hz.
    expectNear (lfoSyncHz (120, 11), 2.0, 1.0e-9, "LFO sync 1/4 at 120 BPM");
    expectNear (lfoSyncHz (120, 5), 0.5, 1.0e-9, "LFO sync whole note at 120 BPM");
    expectNear (pulseDuty (0), 0.5, 1.0e-9, "pulse width floor is a square");
    expect (resonanceDamping (0) > 1.99, "resonance floor is Q=0.5");
    expect (resonanceDamping (127) < 0.0,
            "full resonance crosses the oscillation threshold");
    expect (keyFollowOctavesPerOctave (100) == 1.0, "+100 key follow tracks 1:1");
}

// Every voiced constant is reachable from the mapping namespace, and the ones
// whose endpoints are settled by a document sit where the document puts them.
void testRegisteredConstants()
{
    using namespace septum::mapping;
    // BALANCE is settled only at its endpoints: fully left is OSC1 alone.
    expectNear (balanceLegGain (-63, true), 1.0, 1.0e-12,
                "balance fully left keeps the OSC1 leg at unity");
    expectNear (balanceLegGain (-63, false), 0.0, 1.0e-12,
                "balance fully left silences the OSC2 leg");
    expectNear (balanceLegGain (63, true), 0.0, 1.0e-12,
                "balance fully right silences the OSC1 leg");
    expectNear (balanceLegGain (0, true), 1.0, 1.0e-12,
                "both legs are at unity in the centre");
    expectNear (balanceLegGain (0, false), 1.0, 1.0e-12,
                "both legs are at unity in the centre");
    // The pan law is normalised to unity at the centre, not at an extreme.
    expectNear (partPanCentreGain * std::cos (0.25 * 3.14159265358979323846), 1.0,
                1.0e-12, "the part pan law is unity at the centre");
    // The delay's modulation rate spans the range the contract records.
    expectNear (delayModulationRateHz (0), 0.02, 1.0e-12,
                "delay modulation starts at 0.02 Hz");
    expectNear (delayModulationRateHz (127), 8.0, 1.0e-9,
                "delay modulation reaches 8 Hz");
    // The reverb's lines are mutually prime in length at SIZE 8 so the network
    // does not develop a common period.
    expectNear (reverbSizeScale (7), 1.0, 1.0e-12, "SIZE 8 is the full geometry");
    expect (reverbSizeScale (0) < reverbSizeScale (7),
            "a smaller SIZE shrinks the network");
    // The overdrive's internal rate band, and the factors that reach it.
    expect (overdriveOversampling (44100.0) == 4
                && overdriveOversampling (48000.0) == 4
                && overdriveOversampling (88200.0) == 2
                && overdriveOversampling (96000.0) == 2
                && overdriveOversampling (176400.0) == 1
                && overdriveOversampling (192000.0) == 1,
            "the overdrive's oversampling lands it in a fixed rate band");
    // A power-of-two ladder cannot hit a fixed rate exactly from an arbitrary
    // host rate, so what it owes is a bound. Two things have to hold, and only
    // the second used to be checked: the factor has to be one the shaper
    // actually implements, or the bound is measured against a rate the signal
    // never ran at. `process` has an outer and an inner half-band stage and
    // nothing beyond, so 4 is the most it can do.
    double worst = 0.0;
    for (double rate : { 22050.0, 24000.0, 32000.0, 44100.0, 48000.0, 64000.0,
                         88200.0, 96000.0, 128000.0, 176400.0, 192000.0 })
    {
        const int factor = overdriveOversampling (rate);
        expect (factor == 1 || factor == 2 || factor == 4,
                "the ladder only selects a factor the shaper implements ("
                    + std::to_string (rate) + " Hz chose "
                    + std::to_string (factor) + ")");
        worst = std::max (worst,
                          std::abs (std::log2 (rate * factor / overdriveInternalRateHz)));
    }
    expect (worst <= 1.0 + 1.0e-9,
            "and no host rate puts the shaper more than an octave from it "
            "(worst " + std::to_string (worst) + " octaves)");
}

void testTuningAndMasterTune()
{
    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (plainSawPatch());
    engine.reset();

    // Search ranges exclude the octave-below lag so the autocorrelation
    // cannot fold the estimate down an octave.
    auto take = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);
    const double hz = estimateFundamental (take.left, 11025, 44100, 44100.0,
                                           250.0, 900.0);
    expectNear (hz, 440.0, 1.5, "A4 sounds 440 Hz at default master tune");

    engine.setMasterTuneHz (466.20);
    engine.reset();
    auto sharp = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);
    const double sharpHz = estimateFundamental (sharp.left, 11025, 44100, 44100.0,
                                                300.0, 900.0);
    expectNear (sharpHz, 466.20, 1.6, "master tune ceiling retunes A4");
}

// [settled] Three parameter surfaces the address map stores more coarsely, or
// less narrowly, than the engine did.
void testDocumentedParameterGrids()
{
    // FILTER Cutoff Keyfollow is raw 44-84 displayed -200..+200: 41 positions
    // in steps of 10.
    {
        septum::Patch patch = septum::initPatch();
        patch.upper.keyFollow = 193;
        septum::clampToDocumentedRanges (patch);
        expect (patch.upper.keyFollow == 190,
                "KEY FOLLOW snaps to the documented 10-unit grid (got "
                    + std::to_string (patch.upper.keyFollow) + ")");
        patch.upper.keyFollow = -3;
        septum::clampToDocumentedRanges (patch);
        expect (patch.upper.keyFollow == 0,
                "KEY FOLLOW near zero snaps to zero");
    }

    // Delay Feedback is raw 0-98 displayed -98..+98 %: steps of two.
    {
        septum::Patch patch = septum::initPatch();
        // Every odd value sits exactly between two grid points, so what the
        // check holds is the grid itself: the result is even and no further
        // than one step from what was asked for.
        for (int asked : { -97, -31, -1, 0, 1, 31, 97 })
        {
            patch.delay.feedback = asked;
            septum::clampToDocumentedRanges (patch);
            expect (patch.delay.feedback % 2 == 0
                        && std::abs (patch.delay.feedback - asked) <= 1,
                    "delay FEEDBACK snaps to the documented 2 % grid (asked "
                        + std::to_string (asked) + ", got "
                        + std::to_string (patch.delay.feedback) + ")");
        }
        patch.delay.feedback = 30;
        septum::clampToDocumentedRanges (patch);
        expect (patch.delay.feedback == 30,
                "a value already on the grid is left alone");
        patch.delay.feedback = -98;
        septum::clampToDocumentedRanges (patch);
        expect (patch.delay.feedback == -98,
                "delay FEEDBACK reaches its documented extreme");
    }

    // PITCH WIDE gates the panel knob's travel — "This button expands the
    // range of the PITCH knob by a multiple of three" (OM p. 29) — and not
    // the stored pitch, which the address map keeps at -36..+36 in its own
    // byte. A coarse tune outside +/-12 with the switch off must sound.
    {
        const double sampleRate = 44100.0;
        septum::Patch patch = plainSawPatch();
        patch.upper.osc1.wave = septum::Waveform::Sine;
        patch.upper.osc1.pitchWide = false;
        patch.upper.osc1.coarse = 24;
        patch.upper.balance = -63;
        patch.upper.ampEnvSustain = 127;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 36, 100 } }, 0.6,
                                 sampleRate);
        const double hz = estimateFundamental (take.left, 4410, 22050,
                                               sampleRate, 100.0, 400.0);
        // Note 36 is 65.41 Hz; two octaves up is 261.63 Hz.
        expectNear (hz, 261.63, 3.0,
                    "PITCH sounds what it stores with WIDE off");
    }
}

void testBalanceEndpoints()
{
    // Documented: balance fully left silences OSC2 entirely. The two renders
    // differ only in the OSC2 waveform, so identical output proves the leg
    // is silent.
    septum::Patch a = plainSawPatch();
    a.upper.osc2.wave = septum::Waveform::Sine;
    septum::Patch b = a;
    b.upper.osc2.wave = septum::Waveform::Triangle;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (a);
    engine.reset();
    auto takeA = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.5);
    engine.setPatch (b);
    engine.reset();
    auto takeB = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.5);

    double maxDiff = 0.0;
    for (std::size_t i = 0; i < takeA.left.size(); ++i)
        maxDiff = std::max (maxDiff,
                            std::abs ((double) takeA.left[i] - takeB.left[i]));
    expect (maxDiff < 1.0e-9, "balance -63 keeps OSC2 out of the output");
    expect (takeA.peak() > 0.01, "the OSC1 leg still sounds");
}

void testRingModulation()
{
    // Both oscillators sine at the same pitch, balance fully left, RING:
    // the output is the product, i.e. a tone at twice the frequency with no
    // energy at the fundamental (its DC half is blocked by the output stage).
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = septum::Waveform::Sine;
    patch.upper.osc2.wave = septum::Waveform::Sine;
    patch.upper.mixType = septum::MixModType::Ring;
    patch.upper.balance = -63;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);

    const double at440 = goertzel (take.left, 11025, 44100, 440.0, 44100.0);
    const double at880 = goertzel (take.left, 11025, 44100, 880.0, 44100.0);
    expect (at880 > 10.0 * std::max (1.0e-9, at440),
            "ring modulation of two equal sines lands on the doubled frequency");
}

void testOscillatorSync()
{
    // SYNC: OSC1 restarts at each OSC2 cycle, so the fundamental follows
    // OSC2 even with OSC1 tuned a fifth up.
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.coarse = 7;
    patch.upper.mixType = septum::MixModType::Sync;
    patch.upper.balance = -63;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);
    const double hz = estimateFundamental (take.left, 11025, 44100, 44100.0,
                                           300.0, 700.0);
    expectNear (hz, 440.0, 2.0, "sync output tracks the OSC2 fundamental");
}

void testSuperSawSpread()
{
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = septum::Waveform::SuperSaw;

    septum::Engine engine;
    engine.prepare (44100.0, 256);

    const auto sideLevel = [&] (int spread)
    {
        patch.upper.osc1.pulseWidth = spread;
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);
        // The outermost low-side oscillator at full detune: 440*(1-0.11).
        return goertzel (take.left, 11025, 44100, 440.0 * (1.0 - 0.11002313),
                         44100.0);
    };

    const double detuned = sideLevel (127);
    const double collapsed = sideLevel (0);
    expect (detuned > 5.0 * std::max (1.0e-9, collapsed),
            "full spread puts a partial on the outermost Szabo offset");
}

// The AMP overdrive stage carries the latency every voice pays, shaping or
// not, so its state belongs with the filter's: kept when a voice is taken
// over, cleared only when a fresh one starts. Clearing it emptied the delay
// line the clean path reads from, and the voice went silent for the whole
// reported latency. SOLO is where one voice can be watched on its own: a new
// key takes the sounding voice over exactly as a steal does.
void testTakingAVoiceOverDoesNotBlankIt()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = septum::Waveform::Sine;
    patch.upper.balance = -63;
    patch.upper.mono = septum::MonoMode::Solo;
    patch.upper.ampEnvAttack = 0;
    patch.upper.ampEnvSustain = 127;
    patch.upper.level = 127;
    patch.upper.overdrive = false;
    patch.delayOn = false;
    patch.reverbOn = false;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.reset();
    const int latency = engine.latencySamples();
    expect (latency > 0, "the overdrive chain reports a latency at 44.1 kHz");

    auto take = renderScore (engine,
                             { { 0.0, true, 60, 100 }, { 0.3, true, 67, 100 } },
                             0.6, sampleRate);

    const auto handover = (std::size_t) (sampleRate * 0.3);
    double reference = 0.0;
    for (std::size_t i = handover - 400; i < handover; ++i)
        reference = std::max (reference, std::abs ((double) take.left[i]));
    expect (reference > 0.01, "the solo voice is sounding into the handover");

    // With the line emptied under it the voice reads silence for the whole
    // latency; the output stage's own poles let that through in a sample or
    // two, so what shows is a run of near-zero samples the sine cannot make
    // this quickly.
    int worstRun = 0, run = 0;
    for (std::size_t i = handover; i < handover + 400; ++i)
    {
        if (std::abs ((double) take.left[i]) < 0.02 * reference)
            worstRun = std::max (worstRun, ++run);
        else
            run = 0;
    }
    expect (worstRun < latency - 2,
            "the voice keeps sounding through the handover (near-silent run "
                + std::to_string (worstRun) + " samples, latency "
                + std::to_string (latency) + ")");
}

// OVERDRIVE is an automatable switch, and the stage behind it carries a
// resampler and an antiderivative reference. Left standing while the switch
// was off, they answer it coming back with whatever was playing when it was
// last on — a third of a second earlier here, which for a held sine is the
// wrong phase entirely.
//
// The switch itself is a change of signal, so the take cannot be read against
// its own slope. It is read against the same note with OVERDRIVE on
// throughout: the voice state is identical in both, so once the stage has
// been fed the same recent history the two must agree sample for sample.
void testOverdriveSwitchesBackInFromLiveState()
{
    const double sampleRate = 44100.0;
    const std::size_t block = 64;
    const std::size_t total = (std::size_t) (sampleRate * 0.6);
    // Block-aligned, so the samples compared below are the ones the switch
    // has actually reached.
    const std::size_t offAt = ((std::size_t) (sampleRate * 0.1) / block) * block;
    const std::size_t onAt = ((std::size_t) (sampleRate * 0.4) / block) * block;

    const auto render = [&] (bool cycleTheSwitch)
    {
        septum::Patch driven = plainSawPatch();
        driven.upper.osc1.wave = septum::Waveform::Sine;
        driven.upper.balance = -63;
        driven.upper.ampEnvSustain = 127;
        driven.upper.ampEnvAttack = 0;
        driven.upper.level = 127;
        driven.upper.overdrive = true;
        driven.upper.drive = 100;
        driven.delayOn = false;
        driven.reverbOn = false;
        septum::Patch clean = driven;
        clean.upper.overdrive = false;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (driven);
        engine.reset();
        engine.noteOn (48, 100);

        std::vector<float> left (total), right (total);
        for (std::size_t pos = 0; pos < total; pos += block)
        {
            if (cycleTheSwitch && pos >= offAt && pos < offAt + block)
                engine.setPatch (clean);
            if (cycleTheSwitch && pos >= onAt && pos < onAt + block)
                engine.setPatch (driven);
            engine.process (left.data() + pos, right.data() + pos,
                            (int) std::min (block, total - pos));
        }
        return left;
    };

    const auto always = render (false);
    const auto cycled = render (true);

    double peak = 0.0;
    for (std::size_t i = onAt; i < total; ++i)
        peak = std::max (peak, std::abs ((double) always[i]));
    expect (peak > 0.01, "the overdriven sine sounds");

    // The first block after the switch: the stage has to pick the signal up
    // where it actually is, not where it left it.
    double worst = 0.0;
    for (std::size_t i = onAt; i < onAt + 128 && i < total; ++i)
        worst = std::max (worst,
                          std::abs ((double) cycled[i] - (double) always[i]));
    // Not zero: the two takes ran different signals for a third of a second,
    // so the output stage's own coupling capacitor is holding a different
    // offset in each, and it discharges over half a second. What the check
    // catches is the chain answering with old audio, which was twice the
    // peak.
    expect (worst < 0.2 * peak,
            "OVERDRIVE comes back on the signal that is playing (worst "
                + std::to_string (worst) + " against a peak of "
                + std::to_string (peak) + ")");
}

void testSplitAndDualVoicing()
{
    septum::Patch patch = plainSawPatch();
    patch.keyboardMode = septum::KeyboardMode::Split;
    patch.splitPoint = 60;
    patch.lower = patch.upper;
    patch.upper.level = 0;  // silence UPPER so routing is audible

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto below = renderScore (engine, { { 0.0, true, 59, 100 } }, 0.4);
    engine.reset();
    auto above = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.4);
    expect (below.peak() > 0.01, "a key left of the split point sounds LOWER");
    expect (above.peak() < 1.0e-6,
            "a key at the split point sounds UPPER (silenced here)");

    // DUAL halves the polyphony: six notes keep at most five per part.
    septum::Patch dual = plainSawPatch();
    dual.keyboardMode = septum::KeyboardMode::Dual;
    dual.lower = dual.upper;
    engine.setPatch (dual);
    engine.reset();
    std::vector<Event> events;
    for (int i = 0; i < 6; ++i)
        events.push_back ({ 0.01 * i, true, 60 + i, 100 });
    (void) renderScore (engine, events, 0.2);
    expect (engine.activeVoiceCount() == 10,
            "DUAL with six keys occupies all ten voices (five pairs)");

    // SINGLE fills all ten voices and the eleventh steals.
    engine.setPatch (plainSawPatch());
    engine.reset();
    events.clear();
    for (int i = 0; i < 11; ++i)
        events.push_back ({ 0.01 * i, true, 48 + i, 100 });
    (void) renderScore (engine, events, 0.3);
    expect (engine.activeVoiceCount() == 10,
            "the eleventh key steals instead of growing the pool");
}

// [settled, OM p. 65] CONTROLLER DESTINATION names the tone or tones each
// physical controller reaches: "Selects the tone(s) whose pitch will be
// changed by the pitch bend lever ... If this is 'BOTH,' the pitch of both the
// UPPER tone and LOWER tone will change", and the same sentence for the
// modulation lever and the expression pedal.
void testControllerDestinations()
{
    const double sampleRate = 44100.0;

    // A DUAL patch whose two tones are a sine each, so a bend shows up as a
    // pitch and nothing else moves.
    const auto dualPatch = []
    {
        septum::Patch patch = plainSawPatch();
        patch.keyboardMode = septum::KeyboardMode::Dual;
        for (septum::TonePatch* tone : { &patch.upper, &patch.lower })
        {
            tone->osc1.wave = septum::Waveform::Sine;
            tone->balance = -63;
            tone->ampEnvSustain = 127;
            tone->ampEnvAttack = 0;
            tone->level = 127;
            tone->bendRange = 12;
            tone->lowFreq = septum::LowFreqMode::Flat;
        }
        // The two tones an octave apart, so each one's bend is visible on its
        // own partial.
        patch.lower.octaveShift = -1;
        return patch;
    };

    const auto bentLevel = [&] (septum::ToneDestination destination, double hz)
    {
        septum::Patch patch = dualPatch();
        patch.pitchBendDestination = destination;
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        engine.setPitchBend (1.0);      // a full octave up on both ranges
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.6,
                                 sampleRate);
        return goertzel (take.left, 8820, 26460, hz, sampleRate);
    };

    // Note 60 is 261.63 Hz and the LOWER tone an octave below it is 130.81;
    // a full bend with a range of 12 doubles each.
    const double upperBent = 523.25, lowerBent = 261.63, lowerUnbent = 130.81;
    expect (bentLevel (septum::ToneDestination::Upper, upperBent) > 1.0e-3,
            "UPPER bends when the destination names it");
    expect (bentLevel (septum::ToneDestination::Upper, lowerUnbent) > 1.0e-3,
            "LOWER stays where it was when the destination does not name it");
    expect (bentLevel (septum::ToneDestination::Lower, lowerBent) > 1.0e-3,
            "LOWER bends when the destination names it");
    expect (bentLevel (septum::ToneDestination::Both, upperBent) > 1.0e-3
                && bentLevel (septum::ToneDestination::Both, lowerBent) > 1.0e-3,
            "BOTH bends both tones");

    // EXPRESSION reaches only the tone(s) it names, and BOTH is what the
    // master chain used to do for everyone.
    const auto expressed = [&] (septum::ToneDestination destination, double hz)
    {
        septum::Patch patch = dualPatch();
        patch.expressionDestination = destination;
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        engine.setExpression (0.0);
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.6,
                                 sampleRate);
        return goertzel (take.left, 13230, 26460, hz, sampleRate);
    };
    // The two tones are an octave apart and the surviving one is loud, so the
    // silenced partial is read against the same partial when the destination
    // does not name it rather than against an absolute floor.
    const double upperTone = 261.63, lowerTone = 130.81;
    const double upperNamed = expressed (septum::ToneDestination::Upper, upperTone);
    const double upperSpared = expressed (septum::ToneDestination::Lower, upperTone);
    const double lowerNamed = expressed (septum::ToneDestination::Lower, lowerTone);
    const double lowerSpared = expressed (septum::ToneDestination::Upper, lowerTone);
    expect (upperNamed < 0.02 * upperSpared,
            "EXPRESSION at zero takes at least 34 dB off the tone it names "
            "(named " + std::to_string (upperNamed) + ", spared "
                + std::to_string (upperSpared) + ")");
    expect (lowerNamed < 0.02 * lowerSpared,
            "EXPRESSION at zero names LOWER as readily as UPPER");
    expect (expressed (septum::ToneDestination::Both, upperTone)
                    < 0.02 * upperSpared
                && expressed (septum::ToneDestination::Both, lowerTone)
                       < 0.02 * lowerSpared,
            "EXPRESSION at zero with BOTH takes both tones down");
}

void testSoloAndHold()
{
    septum::Patch patch = plainSawPatch();
    patch.upper.mono = septum::MonoMode::Solo;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    (void) renderScore (engine,
                        { { 0.0, true, 60, 100 }, { 0.05, true, 64, 100 } }, 0.2);
    expect (engine.activeVoiceCount() == 1, "solo keeps one voice");

    // Hold pedal: the note keeps sounding after note-off until pedal up.
    septum::Patch poly = plainSawPatch();
    poly.upper.ampEnvRelease = 0;
    engine.setPatch (poly);
    engine.reset();
    engine.noteOn (60, 100);
    engine.setHold (true);
    engine.noteOff (60);
    std::vector<float> left (4410), right (4410);
    engine.process (left.data(), right.data(), 4410);
    double heldRms = 0.0;
    for (auto sample : left)
        heldRms += sample * (double) sample;
    expect (std::sqrt (heldRms / 4410.0) > 0.01, "hold sustains a released key");
    engine.setHold (false);
    engine.process (left.data(), right.data(), 4410);
    engine.process (left.data(), right.data(), 4410);
    double releasedRms = 0.0;
    for (auto sample : left)
        releasedRms += sample * (double) sample;
    expect (std::sqrt (releasedRms / 4410.0) < 1.0e-3,
            "releasing the pedal releases the note");
}

// Settled (OM p. 72): CC#66 latches the notes sounding when it goes down and
// holds only those. A key pressed while the pedal is already down plays and
// releases normally.
void testSostenutoLatchesOnlyWhatWasSounding()
{
    septum::Patch patch = plainSawPatch();
    patch.upper.ampEnvRelease = 0;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();

    std::vector<float> left (4410), right (4410);
    const auto rms = [&]
    {
        engine.process (left.data(), right.data(), 4410);
        double sum = 0.0;
        for (auto sample : left)
            sum += sample * (double) sample;
        return std::sqrt (sum / 4410.0);
    };

    engine.noteOn (60, 100);
    (void) rms();
    engine.setSostenuto (true);
    engine.noteOff (60);
    expect (rms() > 0.01, "sostenuto sustains a key released after the pedal");

    // A key pressed while the pedal is down is not latched by it.
    engine.noteOn (67, 100);
    (void) rms();
    engine.noteOff (67);
    (void) rms();
    (void) rms();
    expect (engine.activeVoiceCount() == 1,
            "a key pressed after the pedal went down is not latched (voices "
                + std::to_string (engine.activeVoiceCount()) + ")");
    expect (rms() > 0.01, "the latched note is still sounding");

    engine.setSostenuto (false);
    (void) rms();
    expect (rms() < 1.0e-3, "releasing the pedal releases the latched note");

    // The latch belongs to the note, not to the voice playing it. In SOLO a
    // tone hands its one voice from note to note by last-note priority, so a
    // latch tied to the voice would be lost the moment another key borrowed
    // it and the caught note would release with the pedal still down.
    {
        septum::Patch solo = patch;
        solo.upper.mono = septum::MonoMode::Solo;
        engine.setPatch (solo);
        engine.reset();
        engine.noteOn (60, 100);
        (void) rms();
        engine.setSostenuto (true);
        engine.noteOn (64, 100);     // borrows the single voice
        (void) rms();
        engine.noteOff (64);         // priority returns to the caught note
        (void) rms();
        engine.noteOff (60);         // ...whose latch must have survived
        (void) rms();
        expect (rms() > 0.01,
                "a sostenuto latch survives mono note priority");
        engine.setSostenuto (false);
        (void) rms();
        expect (rms() < 1.0e-3, "and lets go when the pedal does");
    }

    // The hold pedal and the sostenuto latch are independent: neither
    // releases a note the other is still holding.
    engine.reset();
    engine.noteOn (60, 100);
    (void) rms();
    engine.setSostenuto (true);
    engine.setHold (true);
    engine.noteOff (60);
    engine.setSostenuto (false);
    expect (rms() > 0.01, "the hold pedal still holds after sostenuto lets go");
    engine.setHold (false);
    (void) rms();
    expect (rms() < 1.0e-3, "and the note releases once both pedals are up");

    // A fresh press of a pitch the pedal already caught was not sounding when
    // the pedal went down, so it is not held either. The latch is kept per
    // pitch rather than per voice - a mono voice borrowed by another key would
    // otherwise lose it - so a new press has to clear it explicitly, or the
    // pedal went on catching that pitch for as long as it stayed down.
    engine.reset();
    engine.noteOn (60, 100);
    (void) rms();
    engine.setSostenuto (true);
    engine.noteOff (60);
    expect (rms() > 0.01, "the caught note is sustaining");
    engine.noteOn (60, 100);          // the same pitch again, pedal still down
    (void) rms();
    engine.noteOff (60);
    (void) rms();
    (void) rms();
    (void) rms();
    expect (rms() < 1.0e-3,
            "and a pitch re-pressed under the pedal releases normally");
}

// Every switch on the external-input path chooses between signals whose
// instantaneous samples differ, so throwing one on live audio steps the output
// however warm the states on the unused side are kept. Each is crossed
// instead. The measure is the largest sample-to-sample jump the output makes
// across the switch, against the largest jump the same signal makes while
// nothing is touched.
void testExternalSwitchesAreCrossedNotThrown()
{
    const double sampleRate = 44100.0;
    const std::size_t total = (std::size_t) (sampleRate * 0.40);
    const std::size_t flipAt = total / 2;

    const auto worstJump = [] (const std::vector<float>& x, std::size_t from,
                               std::size_t to)
    {
        double worst = 0.0;
        for (std::size_t i = std::max<std::size_t> (from, 1);
             i < to && i < x.size(); ++i)
            worst = std::max (worst,
                              std::abs ((double) x[i] - (double) x[i - 1]));
        return worst;
    };

    // Renders a steady centred tone through the monitor with no note playing,
    // flipping one switch half way, and reports {steady jump, jump at the flip}.
    const auto flip = [&] (const septum::ExternalInput& before,
                           const septum::ExternalInput& after)
    {
        septum::Patch patch = plainSawPatch();
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.setExternalInput (before);
        engine.reset();

        std::vector<float> out (total), outR (total);
        std::vector<float> inL (total), inR (total);
        for (std::size_t i = 0; i < total; ++i)
        {
            const double t = (double) i / sampleRate;
            const double centre = 0.4 * std::sin (twoPi * 300.0 * t);
            const double side = 0.4 * std::sin (twoPi * 900.0 * t);
            inL[i] = (float) (centre + side);
            inR[i] = (float) centre;
        }
        const std::size_t block = 64;
        for (std::size_t pos = 0; pos < total; pos += block)
        {
            if (pos >= flipAt && pos < flipAt + block)
                engine.setExternalInput (after);
            const auto count = (int) std::min (block, total - pos);
            engine.process (out.data() + pos, outR.data() + pos, count,
                            inL.data() + pos, inR.data() + pos);
        }
        // Both sides of the switch are measured, because the two signals
        // genuinely move at different rates - a high-pass output travels
        // further between samples than a low-pass one does, and comparing the
        // new signal against the old one's slope would read that as a click.
        // What a thrown switch produces is a jump far larger than either
        // signal makes on its own.
        const double earlySlope = worstJump (out, (std::size_t) (sampleRate * 0.06),
                                             (std::size_t) (sampleRate * 0.16));
        const double lateSlope = worstJump (out, (std::size_t) (sampleRate * 0.30),
                                            (std::size_t) (sampleRate * 0.39));
        const double atFlip = worstJump (out, flipAt - 8, flipAt + block * 3);
        return std::pair<double, double> { std::max (earlySlope, lateSlope),
                                           atFlip };
    };

    septum::ExternalInput plain {};
    plain.inputVolume = 100;
    plain.filterOn = false;
    plain.centerCancel = false;
    // The cutoff and resonance are set here rather than alongside the switch,
    // because they are slewed in their own right: moving them at the same
    // moment would leave the filtered path still agreeing with the dry one
    // just as the switch was thrown, and the step would have nothing to show.
    plain.cutoff = 60;
    plain.resonance = 110;

    const auto check = [&] (const septum::ExternalInput& after,
                            const std::string& what)
    {
        const auto [steady, atFlip] = flip (plain, after);
        expect (atFlip < 4.0 * steady + 1.0e-4,
                what + " is crossed, not thrown (jump " + std::to_string (atFlip)
                    + " against a steady " + std::to_string (steady) + ")");
    };

    septum::ExternalInput cancelled = plain;
    cancelled.centerCancel = true;
    check (cancelled, "CENTER CANCEL");

    // Only the switch moves: the filtered path is already running, resonant
    // and out of phase with the dry one.
    septum::ExternalInput filtered = plain;
    filtered.filterOn = true;
    check (filtered, "FILTER ON");

    septum::ExternalInput steep = plain;
    steep.filterOn = true;
    steep.slope = septum::FilterSlope::Db24;
    septum::ExternalInput shallow = steep;
    shallow.slope = septum::FilterSlope::Db12;
    {
        const auto [steady, atFlip] = flip (shallow, steep);
        expect (atFlip < 4.0 * steady + 1.0e-4,
                "SLOPE is crossed, not thrown (jump " + std::to_string (atFlip)
                    + " against a steady " + std::to_string (steady) + ")");
    }

    septum::ExternalInput notched = shallow;
    notched.type = septum::AudioFilterType::Hpf;
    {
        const auto [steady, atFlip] = flip (shallow, notched);
        expect (atFlip < 4.0 * steady + 1.0e-4,
                "TYPE is crossed, not thrown (jump " + std::to_string (atFlip)
                    + " against a steady " + std::to_string (steady) + ")");
    }
}

// The external-input path (OM pp. 49-53). Renders with a stereo test signal on
// the INPUT jacks: a 300 Hz tone panned centre plus a 900 Hz tone only in the
// left channel, so CENTER CANCEL has something to remove and something to keep.
struct ExternalRender
{
    Render out;
    std::vector<float> inputLeft, inputRight;
};

ExternalRender renderWithExternalInput (septum::Engine& engine,
                                        const std::vector<Event>& events,
                                        double seconds,
                                        double sampleRate = 44100.0)
{
    const auto total = (std::size_t) std::llround (seconds * sampleRate);
    ExternalRender result;
    result.out.left.assign (total, 0.0f);
    result.out.right.assign (total, 0.0f);
    result.inputLeft.assign (total, 0.0f);
    result.inputRight.assign (total, 0.0f);
    for (std::size_t i = 0; i < total; ++i)
    {
        const double t = (double) i / sampleRate;
        const double centre = 0.4 * std::sin (twoPi * 300.0 * t);
        const double side = 0.4 * std::sin (twoPi * 900.0 * t);
        result.inputLeft[i] = (float) (centre + side);
        result.inputRight[i] = (float) centre;
    }

    std::vector<Event> sorted = events;
    std::sort (sorted.begin(), sorted.end(),
               [] (const Event& a, const Event& b) { return a.seconds < b.seconds; });
    std::size_t eventIndex = 0, position = 0;
    const std::size_t block = 256;
    while (position < total)
    {
        while (eventIndex < sorted.size()
               && (std::size_t) std::llround (sorted[eventIndex].seconds * sampleRate)
                      <= position)
        {
            const auto& event = sorted[eventIndex];
            if (event.on)
                engine.noteOn (event.note, event.velocity);
            else
                engine.noteOff (event.note);
            ++eventIndex;
        }
        const auto count = std::min (block, total - position);
        engine.process (result.out.left.data() + position,
                        result.out.right.data() + position, (int) count,
                        result.inputLeft.data() + position,
                        result.inputRight.data() + position);
        position += count;
    }
    return result;
}

void testExternalInputAndAudioFilter()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.level = 127;

    const auto run = [&] (const septum::ExternalInput& settings,
                          const septum::Patch& withPatch,
                          const std::vector<Event>& events, double seconds)
    {
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (withPatch);
        engine.setExternalInput (settings);
        engine.reset();
        return renderWithExternalInput (engine, events, seconds, sampleRate);
    };

    // 1. With nothing plugged in the engine is exactly what it was: the
    //    default settings monitor an input, but there is none unless one is
    //    passed, and the direct path is silent with no notes.
    septum::ExternalInput monitor {};
    monitor.filterOn = false;
    {
        auto take = run (monitor, patch, {}, 0.2);
        const double centre = goertzel (take.out.left, 2205, 8820, 300.0, sampleRate);
        expect (centre > 1.0e-3,
                "the INPUT jacks are monitored with no note playing (value "
                    + std::to_string (centre) + ")");
    }

    // 2. INPUT VOL fully left silences the connected device (settled, OM p.49).
    {
        septum::ExternalInput silent = monitor;
        silent.inputVolume = 0;
        auto take = run (silent, patch, {}, 0.2);
        expect (take.out.peak() < 1.0e-4,
                "INPUT VOL fully left mutes the external source");
    }

    // 3. CENTER CANCEL removes what is common to both channels and keeps what
    //    is not (settled, OM p.49).
    {
        auto plain = run (monitor, patch, {}, 0.3);
        septum::ExternalInput cancelled = monitor;
        cancelled.centerCancel = true;
        auto cancel = run (cancelled, patch, {}, 0.3);
        const double centreBefore =
            goertzel (plain.out.left, 4410, 13230, 300.0, sampleRate);
        const double centreAfter =
            goertzel (cancel.out.left, 4410, 13230, 300.0, sampleRate);
        const double sideBefore =
            goertzel (plain.out.left, 4410, 13230, 900.0, sampleRate);
        const double sideAfter =
            goertzel (cancel.out.left, 4410, 13230, 900.0, sampleRate);
        expect (centreAfter < 0.02 * centreBefore,
                "CENTER CANCEL removes the centred tone (" +
                    std::to_string (20.0 * std::log10 (centreAfter
                                                       / std::max (1.0e-30, centreBefore)))
                    + " dB)");
        expect (sideAfter > 0.4 * sideBefore,
                "CENTER CANCEL keeps what is not centred");
    }

    // 4. The AUDIO FILTER's LPF closed removes the monitored signal; NOTCH
    //    removes the band at its cutoff and keeps the rest.
    {
        septum::ExternalInput closed = monitor;
        closed.filterOn = true;
        closed.type = septum::AudioFilterType::Lpf;
        closed.slope = septum::FilterSlope::Db24;
        closed.cutoff = 0;
        auto take = run (closed, patch, {}, 0.3);
        auto open = run (monitor, patch, {}, 0.3);
        const double closedLevel =
            goertzel (take.out.left, 4410, 13230, 300.0, sampleRate);
        const double openLevel =
            goertzel (open.out.left, 4410, 13230, 300.0, sampleRate);
        // The manual's wording is "essentially all of the component
        // frequencies ... will be cut", not silence: a -24 dB LPF at the
        // bottom of its range is about four octaves below 300 Hz.
        expect (closedLevel < 0.003 * openLevel,
                "a closed AUDIO FILTER LPF all but silences the monitor path ("
                    + std::to_string (20.0 * std::log10 (
                          closedLevel / std::max (1.0e-30, openLevel))) + " dB)");

        septum::ExternalInput notch = monitor;
        notch.filterOn = true;
        notch.type = septum::AudioFilterType::Notch;
        notch.resonance = 100;
        // Cutoff mapped to the centred tone's own frequency.
        notch.cutoff = (int) std::lround (127.0 * std::log2 (300.0 / 20.0) / 10.0);
        auto notched = run (notch, patch, {}, 0.3);
        auto through = run (monitor, patch, {}, 0.3);
        const double atNotch =
            goertzel (notched.out.left, 4410, 13230, 300.0, sampleRate);
        const double reference =
            goertzel (through.out.left, 4410, 13230, 300.0, sampleRate);
        const double kept = goertzel (notched.out.left, 4410, 13230, 900.0, sampleRate);
        const double keptReference =
            goertzel (through.out.left, 4410, 13230, 900.0, sampleRate);
        expect (atNotch < 0.25 * reference,
                "NOTCH removes the band at its cutoff ("
                    + std::to_string (20.0 * std::log10 (
                          atNotch / std::max (1.0e-30, reference))) + " dB)");
        expect (kept > 0.5 * keptReference,
                "NOTCH keeps what is away from its cutoff");
    }

    // 5. EXT-IN as a waveform plays the input through the voice, in mono, and
    //    the direct monitor goes quiet while it does (settled, OM pp.52-53).
    {
        septum::Patch extPatch = patch;
        extPatch.upper.osc1.wave = septum::Waveform::ExtIn;
        extPatch.upper.balance = -63;
        septum::ExternalInput settings = monitor;
        settings.filterOn = true;
        settings.type = septum::AudioFilterType::Lpf;
        settings.slope = septum::FilterSlope::Db24;
        settings.cutoff = 0;   // the manual's "sound only when you play" recipe

        auto take = run (settings, extPatch, { { 0.05, true, 60, 100 } }, 0.4);
        const double released =
            goertzel (take.out.left, 220, 1980, 300.0, sampleRate);
        const double held = goertzel (take.out.left, 6615, 15435, 300.0, sampleRate);
        expect (held > 100.0 * std::max (1.0e-12, released),
                "the manual's recipe holds: closed audio filter, silent on "
                "release, audible under a key (held " + std::to_string (held)
                    + ", released " + std::to_string (released) + ")");

        // ...which also settles that the EXT-IN oscillator taps the input
        // before the audio filter: if it tapped after, the closed filter
        // would silence the played note too.
        septum::ExternalInput openFilter = settings;
        openFilter.filterOn = false;
        auto openTake = run (openFilter, extPatch, { { 0.05, true, 60, 100 } }, 0.4);
        const double heldOpen =
            goertzel (openTake.out.left, 6615, 15435, 300.0, sampleRate);
        expect (held > 0.5 * heldOpen,
                "the played note is unaffected by the audio filter's setting");
    }
}

// The manual works three examples of the arpeggio style "1-2-3-2" against the
// keys C-D-E-F-G (OM p. 67). They are the only public statement of what the
// MOTIF values actually do, so the mapping is held to them exactly.
void testArpeggioMotifsMatchTheManualsExamples()
{
    const int keys[5] { 60, 62, 64, 65, 67 };   // C D E F G
    const int rows[4] { 1, 2, 3, 2 };           // the style "1-2-3-2"
    const int span = 3;

    const auto cycleNotes = [&] (septum::ArpeggioMotif motif, int cycle)
    {
        std::vector<int> notes;
        for (int row : rows)
            notes.push_back (septum::mapping::arpeggioKeyForRow (
                motif, keys, 5, -1, row, span, cycle));
        return notes;
    };
    const auto expectCycle = [&] (septum::ArpeggioMotif motif, int cycle,
                                  std::vector<int> expected,
                                  const std::string& label)
    {
        const auto actual = cycleNotes (motif, cycle);
        bool same = actual.size() == expected.size();
        for (std::size_t i = 0; same && i < actual.size(); ++i)
            same = actual[i] == expected[i];
        std::string got;
        for (int note : actual)
            got += std::to_string (note) + " ";
        expect (same, label + " (got " + got + ")");
    };

    // "When 'UP(-)' is selected as the MOTIF:
    //  C-D-E-D -> D-E-F-E -> E-F-G-F (-> repeated)"
    expectCycle (septum::ArpeggioMotif::Up, 0, { 60, 62, 64, 62 },
                 "UP(-) cycle 1 is C-D-E-D");
    expectCycle (septum::ArpeggioMotif::Up, 1, { 62, 64, 65, 64 },
                 "UP(-) cycle 2 is D-E-F-E");
    expectCycle (septum::ArpeggioMotif::Up, 2, { 64, 65, 67, 65 },
                 "UP(-) cycle 3 is E-F-G-F");
    expectCycle (septum::ArpeggioMotif::Up, 3, { 60, 62, 64, 62 },
                 "UP(-) then repeats");

    // "When 'UP(L)' is selected as the MOTIF:
    //  C-D-E-D -> C-E-F-E -> C-F-G-F (-> repeated)"
    expectCycle (septum::ArpeggioMotif::UpL, 0, { 60, 62, 64, 62 },
                 "UP(L) cycle 1 is C-D-E-D");
    expectCycle (septum::ArpeggioMotif::UpL, 1, { 60, 64, 65, 64 },
                 "UP(L) cycle 2 is C-E-F-E");
    expectCycle (septum::ArpeggioMotif::UpL, 2, { 60, 65, 67, 65 },
                 "UP(L) cycle 3 is C-F-G-F");

    // "When 'UP&DOWN(L&H)' is selected as the MOTIF:
    //  C-D-G-D -> C-E-G-E -> C-F-G-F -> C-E-G-E (-> repeated)"
    expectCycle (septum::ArpeggioMotif::UpDownLowHigh, 0, { 60, 62, 67, 62 },
                 "UP&DOWN(L&H) cycle 1 is C-D-G-D");
    expectCycle (septum::ArpeggioMotif::UpDownLowHigh, 1, { 60, 64, 67, 64 },
                 "UP&DOWN(L&H) cycle 2 is C-E-G-E");
    expectCycle (septum::ArpeggioMotif::UpDownLowHigh, 2, { 60, 65, 67, 65 },
                 "UP&DOWN(L&H) cycle 3 is C-F-G-F");
    expectCycle (septum::ArpeggioMotif::UpDownLowHigh, 3, { 60, 64, 67, 64 },
                 "UP&DOWN(L&H) cycle 4 is C-E-G-E");
    expectCycle (septum::ArpeggioMotif::UpDownLowHigh, 4, { 60, 62, 67, 62 },
                 "UP&DOWN(L&H) then repeats");

    // DOWN reads the same window from the top of the chord.
    expectCycle (septum::ArpeggioMotif::Down, 0, { 67, 65, 64, 65 },
                 "DOWN(-) cycle 1 runs from the highest key");

    // "(L)" names the lowest key pressed and "(L&H)" the lowest and the
    // highest. Which key that is does not depend on which way the window
    // walks, so the DOWN motifs must pin the same keys the UP ones do.
    expectCycle (septum::ArpeggioMotif::DownL, 0, { 60, 65, 64, 65 },
                 "DOWN(L) still sounds the lowest key every time");
    expectCycle (septum::ArpeggioMotif::DownLowHigh, 0, { 60, 65, 67, 65 },
                 "DOWN(L&H) still sounds the lowest and the highest every time");
    for (int cycle = 0; cycle < 4; ++cycle)
    {
        expect (septum::mapping::arpeggioKeyForRow (septum::ArpeggioMotif::DownL,
                                                    keys, 5, -1, 1, span, cycle)
                    == 60,
                "DOWN(L) row 1 is the lowest key in every cycle");
        expect (septum::mapping::arpeggioKeyForRow (
                    septum::ArpeggioMotif::DownLowHigh, keys, 5, -1, span, span,
                    cycle)
                    == 67,
                "DOWN(L&H) row " + std::to_string (span)
                    + " is the highest key in every cycle");
    }

    // PHRASE ignores the chord and reads the rows as steps above the last key.
    expect (septum::mapping::arpeggioKeyForRow (septum::ArpeggioMotif::Phrase,
                                                keys, 5, 72, 8, span, 0) == 79,
            "PHRASE reads row 8 as seven semitones above the key played");
}

// The grid divisions are settled; a shuffled pair must keep the beat.
// [settled, OM p.66] "When the number of keys played is less than the number
// of notes in the arpeggio style, the highest-pitched of the pressed keys is
// played by default." The sentence carries no direction qualifier, so it holds
// for the DOWN motifs too — they used to fall back on the lowest key.
void testShortChordFallsBackOnTheHighestKey()
{
    using septum::ArpeggioMotif;
    using septum::mapping::arpeggioKeyIndexForRow;

    // Two keys held, a style four rows wide: rows 3 and 4 have no key of
    // their own, whatever the motif.
    const int count = 2, span = 4;
    for (auto motif : { ArpeggioMotif::Up, ArpeggioMotif::UpL,
                        ArpeggioMotif::UpLowHigh, ArpeggioMotif::Down,
                        ArpeggioMotif::DownL, ArpeggioMotif::DownLowHigh,
                        ArpeggioMotif::UpDown, ArpeggioMotif::UpDownL,
                        ArpeggioMotif::UpDownLowHigh })
    {
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            // Row 4 is the style's last, so an (L&H) motif pins it to the
            // highest key for its own reason; row 3 is pinned by nothing.
            const int third =
                arpeggioKeyIndexForRow (motif, count, 3, span, cycle);
            expect (third == count - 1,
                    "a row the chord cannot fill plays the highest key held "
                    "(motif " + std::to_string ((int) motif) + ", cycle "
                        + std::to_string (cycle) + ", got "
                        + std::to_string (third) + ")");
        }
    }

    // And a chord wide enough for the style is untouched: three keys under a
    // three-row style still walk their window.
    expect (arpeggioKeyIndexForRow (ArpeggioMotif::Down, 3, 1, 3, 0) == 2,
            "a full chord still reads the window from the top on DOWN");
    expect (arpeggioKeyIndexForRow (ArpeggioMotif::Down, 3, 3, 3, 0) == 0,
            "a full chord still reaches the bottom on DOWN");
}

void testArpeggioGridDivisions()
{
    using septum::ArpeggioGrid;
    using septum::mapping::arpeggioStepSeconds;
    const double beat = 0.5;   // 120 BPM
    expectNear (arpeggioStepSeconds (120.0, ArpeggioGrid::Quarter, 0), beat,
                1.0e-12, "1/4 is one beat");
    expectNear (arpeggioStepSeconds (120.0, ArpeggioGrid::Eighth, 0), beat / 2,
                1.0e-12, "two 1/8 sections are one beat");
    expectNear (arpeggioStepSeconds (120.0, ArpeggioGrid::Twelfth, 0), beat / 3,
                1.0e-12, "three 1/12 sections are one beat");
    expectNear (arpeggioStepSeconds (120.0, ArpeggioGrid::Sixteenth, 0), beat / 4,
                1.0e-12, "four 1/16 sections are one beat");
    expectNear (arpeggioStepSeconds (120.0, ArpeggioGrid::TwentyFourth, 0),
                beat / 6, 1.0e-12, "six 1/24 sections are one beat");
    for (auto grid : { ArpeggioGrid::EighthLight, ArpeggioGrid::EighthHeavy })
        expectNear (arpeggioStepSeconds (120.0, grid, 0)
                        + arpeggioStepSeconds (120.0, grid, 1),
                    beat, 1.0e-12, "a shuffled 1/8 pair still spans one beat");
    for (auto grid : { ArpeggioGrid::SixteenthLight, ArpeggioGrid::SixteenthHeavy })
        expectNear (arpeggioStepSeconds (120.0, grid, 0)
                        + arpeggioStepSeconds (120.0, grid, 1),
                    beat / 2, 1.0e-12,
                    "a shuffled 1/16 pair still spans an eighth");
    expect (arpeggioStepSeconds (120.0, ArpeggioGrid::EighthHeavy, 0)
                > arpeggioStepSeconds (120.0, ArpeggioGrid::EighthLight, 0),
            "a heavy shuffle leans further than a light one");
}

// End to end: the arpeggiator turns a held chord into a stream of separate
// notes at the grid tempo, HOLD keeps it playing, and turning it off stops it.
// A shuffled pair keeps its total length, so the beat never drifts — the
// contract says so, and the pure function honours it for step 0 against step
// 1. But the parity used to come from the *pattern* step, which wraps at END
// STEP: with an odd END STEP the same parity repeated and the pair stopped
// summing to its division. END STEP 1 on 1/8L played every section as the
// long half and ran 16 % slow, drifting for as long as the key was held.
void testShuffleFollowsTheBeatNotThePattern()
{
    const double sampleRate = 44100.0;

    const auto onsets = [sampleRate] (int endStep)
    {
        septum::Patch patch = plainSawPatch();
        patch.upper.level = 127;
        patch.upper.ampEnvRelease = 0;
        patch.tempo = 120;                       // one beat = 0.5 s
        patch.arpeggio.on = true;
        patch.arpeggio.grid = septum::ArpeggioGrid::EighthLight;
        patch.arpeggio.duration = septum::ArpeggioDuration::P50;
        patch.arpeggio.motif = septum::ArpeggioMotif::Up;
        patch.arpeggio.endStep = endStep;
        septum::applyArpeggioStyle (patch, 0);   // "Straight 4"

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 3.0,
                                 sampleRate);

        // Peak to peak, not RMS: the documented 0.33 Hz output coupling
        // leaves a slowly discharging offset behind each note, and an RMS
        // gate reads that as the note still sounding.
        std::vector<double> times;
        const auto window = (std::size_t) (sampleRate * 0.002);
        bool sounding = false;
        for (std::size_t i = 0; i + window < take.left.size(); i += window)
        {
            float low = 1.0f, high = -1.0f;
            for (std::size_t k = i; k < i + window; ++k)
            {
                low = std::min (low, take.left[k]);
                high = std::max (high, take.left[k]);
            }
            const double swing = (double) (high - low);
            if (! sounding && swing > 2.0e-3)
            {
                sounding = true;
                times.push_back ((double) i / sampleRate);
            }
            else if (sounding && swing < 2.0e-4)
            {
                sounding = false;
            }
        }
        return times;
    };

    // Eight sections of a shuffled eighth are four beats, however long the
    // pattern that rides them happens to be.
    for (int endStep : { 1, 2, 3, 4 })
    {
        const auto times = onsets (endStep);
        expect (times.size() >= 9,
                "a shuffled grid fires its sections (END STEP "
                    + std::to_string (endStep) + ", "
                    + std::to_string (times.size()) + " onsets)");
        if (times.size() < 9)
            continue;
        const double eightSections = times[8] - times[0];
        expectNear (eightSections, 2.0, 0.02,
                    "eight 1/8 sections span four beats at END STEP "
                        + std::to_string (endStep));
        // And the pair really is uneven, or the check above would pass on an
        // unshuffled grid too.
        expect (times[1] - times[0] > 1.15 * (times[2] - times[1]),
                "the shuffled pair leans, at END STEP "
                    + std::to_string (endStep));
    }
}

void testArpeggiatorPlaysAndHolds()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.level = 127;
    patch.upper.ampEnvRelease = 0;
    patch.tempo = 120;
    patch.arpeggio.on = true;
    patch.arpeggio.grid = septum::ArpeggioGrid::Eighth;   // 4 Hz at 120 BPM
    patch.arpeggio.duration = septum::ArpeggioDuration::P50;
    patch.arpeggio.motif = septum::ArpeggioMotif::Up;
    septum::applyArpeggioStyle (patch, 0);   // "Straight 4": one row per step

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.reset();

    // Silence before a key, then a chord that must produce a gapped stream.
    auto take = renderScore (engine,
                             { { 0.1, true, 60, 100 },
                               { 0.1, true, 64, 100 },
                               { 0.1, true, 67, 100 } },
                             1.6, sampleRate);
    expect (take.rms (0, 4000) < 1.0e-5, "the arpeggiator is silent before a key");
    expect (take.finite() && take.peak() > 0.01, "the arpeggiator sounds");

    // A 50 % duration at 4 steps a second means the output must go quiet
    // between steps: count how many 5 ms windows are near-silent.
    const auto window = (std::size_t) (sampleRate * 0.005);
    int quiet = 0, loud = 0;
    for (std::size_t i = (std::size_t) (sampleRate * 0.2);
         i + window < take.left.size(); i += window)
    {
        const double level = take.rms (i, i + window);
        if (level < 1.0e-4)
            ++quiet;
        else if (level > 1.0e-3)
            ++loud;
    }
    expect (quiet > 10 && loud > 10,
            "a 50 % duration leaves gaps between the steps (quiet "
                + std::to_string (quiet) + ", loud " + std::to_string (loud) + ")");

    // Without HOLD, letting go stops it. (A fresh engine: the take above left
    // three keys down.)
    engine.setPatch (patch);
    engine.reset();
    auto stopped = renderScore (engine,
                                { { 0.0, true, 60, 100 }, { 0.5, false, 60, 100 } },
                                1.5, sampleRate);
    // What must vanish is the AC content: the documented 0.33 Hz output
    // coupling keeps discharging a small DC offset for a while after the last
    // note stops, exactly as it does after an all-sound-off.
    const auto peakToPeak = [] (const Render& take, std::size_t from)
    {
        float low = 1.0f, high = -1.0f;
        for (std::size_t i = from; i < take.left.size(); ++i)
        {
            low = std::min (low, take.left[i]);
            high = std::max (high, take.left[i]);
        }
        return (double) (high - low);
    };
    expect (peakToPeak (stopped, (std::size_t) (sampleRate * 1.0)) < 1.0e-4,
            "releasing the chord stops the arpeggio (peak-to-peak "
                + std::to_string (peakToPeak (stopped,
                                              (std::size_t) (sampleRate * 1.0)))
                + ")");

    // With HOLD, it keeps going.
    patch.arpeggio.hold = true;
    engine.setPatch (patch);
    engine.reset();
    auto held = renderScore (engine,
                             { { 0.0, true, 60, 100 }, { 0.5, false, 60, 100 } },
                             1.5, sampleRate);
    expect (held.rms ((std::size_t) (sampleRate * 1.0), held.left.size()) > 1.0e-3,
            "HOLD keeps the arpeggio playing after the keys are released");

    // Turning the arpeggiator off silences it and gives the keyboard back.
    patch.arpeggio.on = false;
    engine.setPatch (patch);
    std::vector<float> left (8820), right (8820);
    engine.process (left.data(), right.data(), 8820);
    engine.process (left.data(), right.data(), 8820);
    double sum = 0.0;
    for (auto sample : left)
        sum += sample * (double) sample;
    expect (std::sqrt (sum / 8820.0) < 1.0e-4,
            "turning the arpeggiator off stops the notes it was holding (value "
                + std::to_string (std::sqrt (sum / 8820.0)) + ")");
}

// OCTAVE RANGE shifts one cycle at a time, so a run of cycles must visit
// pitches an octave apart.
// The review found six things the arpeggiator got wrong that no test covered.
// Each of them is one here.
void testArpeggiatorEdgeCases()
{
    const double sampleRate = 44100.0;
    septum::Patch base = plainSawPatch();
    base.upper.level = 127;
    base.upper.ampEnvRelease = 0;
    base.tempo = 120;
    base.arpeggio.on = true;
    base.arpeggio.grid = septum::ArpeggioGrid::Eighth;
    base.arpeggio.duration = septum::ArpeggioDuration::P50;
    base.arpeggio.motif = septum::ArpeggioMotif::Up;
    septum::applyArpeggioStyle (base, 0);

    std::vector<float> left (4410), right (4410);
    const auto rmsOf = [&] (septum::Engine& engine)
    {
        engine.process (left.data(), right.data(), 4410);
        double sum = 0.0;
        for (auto sample : left)
            sum += sample * (double) sample;
        return std::sqrt (sum / 4410.0);
    };

    // 1. Switching the arpeggiator on under a held key must not strand the
    //    voice that key already started: its release still has to reach it.
    {
        septum::Patch off = base;
        off.arpeggio.on = false;
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (off);
        engine.reset();
        engine.noteOn (60, 100);
        expect (rmsOf (engine) > 1.0e-3, "the key sounds before the arpeggiator");

        engine.setPatch (base);            // arp_on automated to ON mid-note
        // The key migrates: its plain voice stops and it becomes the chord,
        // so the note keeps sounding — but now in eighth-note steps.
        auto arpeggiated = renderScore (engine, {}, 1.5, sampleRate);
        expect (arpeggiated.rms (0, arpeggiated.left.size()) > 1.0e-3,
                "the held key migrates into the arpeggio when it is switched on");

        // And the release still reaches it: nothing is left sounding.
        engine.noteOff (60);
        (void) rmsOf (engine);
        (void) rmsOf (engine);
        expect (rmsOf (engine) < 1.0e-4,
                "a key held across the switch is not stranded by its note-off");
    }

    // 2. With HOLD on, releasing one key of a held chord must not drop the
    //    others: the chord latches only when the last key comes up.
    {
        septum::Patch hold = base;
        hold.arpeggio.hold = true;
        hold.arpeggio.motif = septum::ArpeggioMotif::Up;
        hold.arpeggio.grid = septum::ArpeggioGrid::Quarter;   // 0.5 s a step
        hold.arpeggio.duration = septum::ArpeggioDuration::P100;
        // One step, one row: each pass plays one key, so the window walking
        // the chord is audible as a change of pitch from step to step.
        hold.arpeggio.style = septum::ArpeggioStyle {};
        hold.arpeggio.style.endStep = 1;
        hold.arpeggio.style.cells[0][0] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (hold);
        engine.reset();
        engine.noteOn (48, 100);
        engine.noteOn (55, 100);
        engine.noteOn (60, 100);
        (void) rmsOf (engine);
        engine.noteOff (48);               // one finger up, two still down
        engine.noteOn (64, 100);           // a fourth key joins the chord
        auto take = renderScore (engine, {}, 2.2, sampleRate);
        // If the chord had been dropped on that first release, every pass
        // would play the one remaining key and the pitch would never move.
        const double firstPass =
            estimateFundamental (take.left, (std::size_t) (sampleRate * 0.50),
                                 (std::size_t) (sampleRate * 0.85), sampleRate,
                                 80.0, 700.0);
        const double secondPass =
            estimateFundamental (take.left, (std::size_t) (sampleRate * 1.00),
                                 (std::size_t) (sampleRate * 1.35), sampleRate,
                                 80.0, 700.0);
        expect (std::abs (std::log2 (secondPass / firstPass)) > 0.05,
                "the held chord survives one key coming up, so the window "
                "still walks it (" + std::to_string (firstPass) + " Hz then "
                    + std::to_string (secondPass) + " Hz)");
    }

    // 3. A tie chain is counted once. "9~~-" at 50 % duration must leave the
    //    fourth grid silent; counting the ties twice fills it.
    {
        septum::Patch tied = base;
        tied.arpeggio.grid = septum::ArpeggioGrid::Quarter;   // 0.5 s a step
        tied.arpeggio.duration = septum::ArpeggioDuration::P50;
        tied.arpeggio.style = septum::ArpeggioStyle {};
        tied.arpeggio.style.endStep = 4;
        tied.arpeggio.style.cells[0][0] = 100;                // note on
        tied.arpeggio.style.cells[1][0] = septum::arpeggioTie;
        tied.arpeggio.style.cells[2][0] = septum::arpeggioTie;
        tied.arpeggio.style.cells[3][0] = septum::arpeggioRest;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (tied);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 2.0, sampleRate);
        // Three grids at 0.5 s, the last held for 50 %: the note runs to
        // 1.25 s and the rest of the first pass is silent.
        const auto sounding = take.rms ((std::size_t) (sampleRate * 0.6),
                                        (std::size_t) (sampleRate * 1.1));
        const auto gap = take.rms ((std::size_t) (sampleRate * 1.45),
                                   (std::size_t) (sampleRate * 1.95));
        expect (sounding > 1.0e-3, "a tie chain sustains through its grids");
        expect (gap < 0.02 * sounding,
                "and stops after DURATION of the final grid, not twice as late "
                "(gap " + std::to_string (gap) + ", sounding "
                    + std::to_string (sounding) + ")");
    }

    // 4. DURATION 120 % means a gate outlives its grid, so consecutive
    //    note-ons on one row must overlap rather than cut each other off.
    {
        septum::Patch gate = base;
        gate.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        gate.arpeggio.duration = septum::ArpeggioDuration::P120;
        gate.arpeggio.motif = septum::ArpeggioMotif::Up;
        // One step, one row: every step is a new pass, so the window moves
        // and consecutive note-ons on that row land on different keys.
        gate.arpeggio.style = septum::ArpeggioStyle {};
        gate.arpeggio.style.endStep = 1;
        gate.arpeggio.style.cells[0][0] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (gate);
        engine.reset();
        // Two keys an octave apart, so the overlap is two pitches at once.
        auto take = renderScore (engine,
                                 { { 0.0, true, 45, 100 }, { 0.0, true, 57, 100 } },
                                 1.4, sampleRate);
        // At the second step's boundary (0.5 s) the first gate has 20 % of a
        // step left, so both notes sound together just after it.
        const double lowAtOverlap =
            goertzel (take.left, (std::size_t) (sampleRate * 0.52),
                      (std::size_t) (sampleRate * 0.58), 110.0, sampleRate);
        const double lowBefore =
            goertzel (take.left, (std::size_t) (sampleRate * 0.30),
                      (std::size_t) (sampleRate * 0.45), 110.0, sampleRate);
        expect (lowAtOverlap > 0.25 * lowBefore,
                "a 120 % gate overlaps the note that follows it (" +
                    std::to_string (lowAtOverlap) + " against "
                    + std::to_string (lowBefore) + ")");
    }

    // 5. A FUL note on a row the next style never plays has nothing left to
    //    end it, so changing style must retire it.
    {
        septum::Patch full = base;
        full.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        full.arpeggio.duration = septum::ArpeggioDuration::Full;
        // UP(L) pins row 1 to the lowest key, so once the style narrows to
        // that row alone the orphan's pitch can never come round again and
        // be released by coincidence.
        full.arpeggio.motif = septum::ArpeggioMotif::UpL;
        full.arpeggio.style = septum::ArpeggioStyle {};
        full.arpeggio.style.endStep = 1;
        full.arpeggio.style.cells[0][0] = 100;
        full.arpeggio.style.cells[0][1] = 100;   // two rows sustain

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (full);
        engine.reset();
        engine.noteOn (45, 100);
        engine.noteOn (57, 100);
        engine.noteOn (69, 100);
        std::vector<float> left (22050), right (22050);
        engine.process (left.data(), right.data(), 22050);

        // Switch to a style that only uses row 1. Row 2's sustained note has
        // nothing left to end it and must be retired.
        septum::Patch narrowed = full;
        narrowed.arpeggio.style.cells[0][1] = septum::arpeggioRest;
        engine.setPatch (narrowed);
        engine.process (left.data(), right.data(), 22050);
        engine.process (left.data(), right.data(), 22050);
        expect (engine.activeVoiceCount() <= 1,
                "a sustained row the new style never plays is retired (voices "
                    + std::to_string (engine.activeVoiceCount()) + ")");
    }

    // 6. Two overlapping MIDI notes of the same pitch — routine in sequenced
    //    parts — must not have the first release take what the second still
    //    holds.
    {
        septum::Patch overlap = base;
        overlap.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        overlap.arpeggio.duration = septum::ArpeggioDuration::P100;
        overlap.arpeggio.style = septum::ArpeggioStyle {};
        overlap.arpeggio.style.endStep = 1;
        overlap.arpeggio.style.cells[0][0] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (overlap);
        engine.reset();
        engine.noteOn (60, 100);
        (void) rmsOf (engine);
        engine.noteOn (60, 100);     // the same pitch again, overlapping
        engine.noteOff (60);         // the first release
        auto take = renderScore (engine, {}, 1.2, sampleRate);
        // Late in the take, so the tail of the note that was already
        // sounding cannot stand in for an arpeggio that is still running.
        expect (take.rms ((std::size_t) (sampleRate * 0.7), take.left.size())
                    > 1.0e-3,
                "an overlapping second press of one pitch outlives the first "
                "release");
        engine.noteOff (60);         // the last release
        (void) rmsOf (engine);
        (void) rmsOf (engine);
        expect (rmsOf (engine) < 1.0e-4,
                "and the last release does stop it");
    }

    // 7. SPLIT ARPEGGIO is automatable, so a key can be pressed while one
    //    part is selected and released while another is. Neither the entry
    //    nor the notes may be left behind.
    {
        septum::Patch split = base;
        split.keyboardMode = septum::KeyboardMode::Split;
        split.splitPoint = 60;
        split.arpeggio.splitArpeggio = septum::SplitArpeggio::Lower;
        split.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        split.lower = split.upper;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (split);
        engine.reset();
        engine.noteOn (48, 100);     // below the split: LOWER arpeggiates it
        (void) rmsOf (engine);
        expect (rmsOf (engine) > 1.0e-3, "the lower arpeggio is running");

        septum::Patch flipped = split;
        flipped.arpeggio.splitArpeggio = septum::SplitArpeggio::Upper;
        engine.setPatch (flipped);   // the selector moves off the held key
        auto handedBack = renderScore (engine, {}, 1.2, sampleRate);
        // The key is still down and the part still sounds, so it plays the
        // way it would have without the arpeggiator - which is what turning
        // the ARPEGGIO switch off under a held key already does. A 50 % gate
        // would have left the back half of every 0.5 s grid silent; a plain
        // voice does not.
        const double handedBackLate =
            handedBack.rms ((std::size_t) (sampleRate * 0.80),
                            (std::size_t) (sampleRate * 0.95));
        expect (handedBackLate > 1.0e-3,
                "deselecting a part hands its held key back as a plain voice ("
                    + std::to_string (handedBackLate) + ")");

        engine.noteOff (48);
        auto released = renderScore (engine, {}, 1.2, sampleRate);
        expect (released.rms ((std::size_t) (sampleRate * 0.4),
                              released.left.size()) < 1.0e-4,
                "and releasing it stops that voice");

        // Selecting it again must not resurrect a chord with no keys down.
        engine.setPatch (split);
        (void) rmsOf (engine);
        auto after = renderScore (engine, {}, 1.5, sampleRate);
        expect (after.rms (0, after.left.size()) < 1.0e-4,
                "and re-selecting it does not resurrect a chord nobody is "
                "holding");
    }

    // 7b. The mirror. A part that *becomes* arpeggiator-driven under a held
    //     key has to take that key with it: otherwise the later note-off sees
    //     a part the arpeggiator now drives, skips the release, and the plain
    //     voice sustains until a panic.
    {
        septum::Patch split = base;
        split.keyboardMode = septum::KeyboardMode::Split;
        split.splitPoint = 60;
        split.arpeggio.splitArpeggio = septum::SplitArpeggio::Lower;
        split.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        split.lower = split.upper;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (split);
        engine.reset();
        engine.noteOn (72, 100);     // above the split: UPPER, not arpeggiated
        (void) rmsOf (engine);
        expect (rmsOf (engine) > 1.0e-3, "the plain upper voice is sounding");

        septum::Patch flipped = split;
        flipped.arpeggio.splitArpeggio = septum::SplitArpeggio::Upper;
        engine.setPatch (flipped);   // the arpeggiator takes the part over
        (void) rmsOf (engine);
        engine.noteOff (72);
        auto after = renderScore (engine, {}, 1.5, sampleRate);
        expect (after.rms ((std::size_t) (sampleRate * 0.5),
                           after.left.size()) < 1.0e-4,
                "a key held as the arpeggiator takes its part over is ended by "
                "its own note-off, not stranded ("
                    + std::to_string (after.rms ((std::size_t) (sampleRate * 0.5),
                                                 after.left.size())) + ")");
    }

    // 8. ARPEGGIO VELOCITY = REAL follows the key each note came from, so a
    //    chord played unevenly stays uneven.
    {
        septum::Patch real = base;
        real.arpeggio.velocity = 0;         // REAL
        real.arpeggio.accent = 0;           // no style pattern on top
        real.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        real.upper.levelVelocitySens = 63;  // make velocity audible
        real.arpeggio.style = septum::ArpeggioStyle {};
        real.arpeggio.style.endStep = 2;
        real.arpeggio.style.cells[0][0] = 100;
        real.arpeggio.style.cells[1][1] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (real);
        engine.reset();
        engine.noteOn (60, 20);             // soft
        engine.noteOn (67, 127);            // hard
        auto take = renderScore (engine, {}, 1.6, sampleRate);
        const auto first = take.rms ((std::size_t) (sampleRate * 0.05),
                                     (std::size_t) (sampleRate * 0.45));
        const auto second = take.rms ((std::size_t) (sampleRate * 0.55),
                                      (std::size_t) (sampleRate * 0.95));
        expect (second > 1.6 * first,
                "REAL velocity keeps each key's own dynamics (soft "
                    + std::to_string (first) + ", hard " + std::to_string (second)
                    + ")");
    }

    // 9. A tie chain across a shuffled pair takes DURATION from the step it
    //    ends on, not the one it started on. On an even grid the two are the
    //    same length and the distinction is invisible; on a shuffled pair it
    //    is a fifth of a beat.
    {
        septum::Patch shuffled = base;
        shuffled.arpeggio.grid = septum::ArpeggioGrid::EighthHeavy;
        shuffled.arpeggio.duration = septum::ArpeggioDuration::P50;
        shuffled.arpeggio.style = septum::ArpeggioStyle {};
        shuffled.arpeggio.style.endStep = 2;
        shuffled.arpeggio.style.cells[0][0] = 100;                 // the long step
        shuffled.arpeggio.style.cells[1][0] = septum::arpeggioTie; // into the short one

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (shuffled);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 1.0, sampleRate);
        // At 120 BPM the Heavy pair is 0.33 s then 0.17 s. The chain covers
        // both, so the gate is 0.33 + 50 % of 0.17 = 0.415 s. Taking the
        // fraction from the step the chain started on instead gives
        // 0.17 + 50 % of 0.33 = 0.335 s, silent well before 0.36 s.
        const double sounding = take.rms ((std::size_t) (sampleRate * 0.10),
                                          (std::size_t) (sampleRate * 0.30));
        const double late = take.rms ((std::size_t) (sampleRate * 0.36),
                                      (std::size_t) (sampleRate * 0.40));
        expect (late > 0.2 * sounding,
                "a tie chain across a shuffled pair ends DURATION into the "
                "step it ends on (" + std::to_string (late) + " against "
                    + std::to_string (sounding) + ")");
    }

    // 10. ARPEGGIO STYLE is automatable. Switching to a shorter pattern must
    //     not spend a grid on a cell the new style does not use.
    {
        septum::Patch longStyle = base;
        longStyle.arpeggio.grid = septum::ArpeggioGrid::Quarter;   // 0.5 s a step
        longStyle.arpeggio.duration = septum::ArpeggioDuration::P100;
        longStyle.arpeggio.style = septum::ArpeggioStyle {};
        longStyle.arpeggio.style.endStep = 8;
        for (int step = 0; step < 8; ++step)
            longStyle.arpeggio.style.cells[step][0] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (longStyle);
        engine.reset();
        engine.noteOn (60, 100);

        // Run to 2.7 s: step 5 fired at 2.5 s, so the counter stands at 6 -
        // past the end of the four-step style selected next.
        const std::size_t lead = (std::size_t) (sampleRate * 2.7);
        std::vector<float> leadL (lead), leadR (lead);
        engine.process (leadL.data(), leadR.data(), (int) lead);

        septum::Patch shortStyle = longStyle;
        shortStyle.arpeggio.style.endStep = 4;
        for (int step = 4; step < 8; ++step)
            shortStyle.arpeggio.style.cells[step][0] = septum::arpeggioRest;
        engine.setPatch (shortStyle);

        const std::size_t tail = (std::size_t) (sampleRate * 0.8);
        std::vector<float> tailL (tail), tailR (tail);
        engine.process (tailL.data(), tailR.data(), (int) tail);

        const auto rmsOver = [] (const std::vector<float>& buffer,
                                 std::size_t from, std::size_t to)
        {
            double sum = 0.0;
            for (std::size_t i = from; i < to && i < buffer.size(); ++i)
                sum += buffer[i] * (double) buffer[i];
            return std::sqrt (sum / (double) (to - from));
        };

        // A grid known to sound, for scale, against the grid beginning at
        // 3.0 s - which is 0.3 s into the tail buffer.
        const double reference = rmsOver (leadL,
                                          (std::size_t) (sampleRate * 2.05),
                                          (std::size_t) (sampleRate * 2.45));
        const double afterSwitch = rmsOver (tailL,
                                            (std::size_t) (sampleRate * 0.35),
                                            (std::size_t) (sampleRate * 0.75));
        expect (afterSwitch > 0.25 * reference,
                "a shorter style fires a step it actually has rather than an "
                "unused cell (" + std::to_string (afterSwitch) + " against "
                    + std::to_string (reference) + ")");
    }

    // 10b. Adjacent chords in a sequence routinely share a sample position:
    //      every note-off of the old chord and every note-on of the new one
    //      land together. No audio is rendered in between, so the empty chord
    //      the re-arm watches for is never visible to advanceArpeggiator, and
    //      the new chord used to pick the pattern up wherever the old one had
    //      left it instead of starting at step one.
    {
        septum::Patch seq = base;
        seq.arpeggio.grid = septum::ArpeggioGrid::Quarter;   // 0.5 s a step
        seq.arpeggio.duration = septum::ArpeggioDuration::P50;
        seq.arpeggio.motif = septum::ArpeggioMotif::Up;
        // Step one is the low row; every later step is the high row. So the
        // first note of a chord is its lowest key, and any other step is not.
        seq.arpeggio.style = septum::ArpeggioStyle {};
        seq.arpeggio.style.endStep = 4;
        seq.arpeggio.style.cells[0][0] = 100;
        seq.arpeggio.style.cells[1][1] = 100;
        seq.arpeggio.style.cells[2][1] = 100;
        seq.arpeggio.style.cells[3][1] = 100;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (seq);
        engine.reset();
        engine.noteOn (60, 100);
        engine.noteOn (67, 100);
        // Run past step one so the counter is genuinely mid-pattern, and past
        // the previous gate so nothing of the old chord is still sounding.
        std::vector<float> lead ((std::size_t) (sampleRate * 0.8));
        std::vector<float> leadR (lead.size());
        engine.process (lead.data(), leadR.data(), (int) lead.size());

        // The whole changeover at one sample position, as a sequencer sends it.
        engine.noteOff (60);
        engine.noteOff (67);
        engine.noteOn (62, 100);
        engine.noteOn (69, 100);

        auto take = renderScore (engine, {}, 0.4, sampleRate);
        // Step one of the new chord is its lowest key, D4. Continuing
        // mid-pattern would sound the high row, A4, an instant later.
        const double first = estimateFundamental (take.left,
                                                  (std::size_t) (sampleRate * 0.02),
                                                  (std::size_t) (sampleRate * 0.20),
                                                  sampleRate, 200.0, 600.0);
        expectNear (first, 293.66, 10.0,
                    "a chord replaced at the same sample position starts the "
                    "new one at step one");
    }

    // 10c. END STEP is its own front-panel control, 1-32, independent of the
    //      template. Zero leaves the template's own length alone; anything
    //      else shortens or lengthens the pattern the style selected.
    {
        septum::Patch shortened = base;
        shortened.arpeggio.grid = septum::ArpeggioGrid::Quarter;
        shortened.arpeggio.duration = septum::ArpeggioDuration::P50;
        shortened.arpeggio.motif = septum::ArpeggioMotif::Up;
        shortened.arpeggio.styleIndex = 1;          // "Straight 8", eight steps
        shortened.arpeggio.endStep = 0;
        septum::applyArpeggioStyle (shortened, shortened.arpeggio.styleIndex);
        expect (shortened.arpeggio.style.endStep == 8,
                "END STEP zero leaves the template's own length ("
                    + std::to_string (shortened.arpeggio.style.endStep) + ")");

        shortened.arpeggio.endStep = 3;
        septum::applyArpeggioStyle (shortened, shortened.arpeggio.styleIndex);
        expect (shortened.arpeggio.style.endStep == 3,
                "and a set END STEP outranks it ("
                    + std::to_string (shortened.arpeggio.style.endStep) + ")");

        // It has to reach the sound, not just the struct: with one key held
        // and OCTAVE RANGE +1 the octave steps up once per completed pass, so
        // a three-step pattern reaches the octave sooner than an eight-step
        // one does.
        const auto octaveArrivesBy = [&] (int endStep)
        {
            septum::Patch p = shortened;
            p.arpeggio.endStep = endStep;
            p.arpeggio.octaveRange = 1;
            septum::applyArpeggioStyle (p, p.arpeggio.styleIndex);
            septum::Engine engine;
            engine.prepare (sampleRate, 256);
            engine.setPatch (p);
            engine.reset();
            auto take = renderScore (engine, { { 0.0, true, 57, 100 } }, 5.0,
                                     sampleRate);
            // 1.6 s in: three steps of 0.5 s have completed a pass, eight have
            // not, so only the short pattern is an octave up by now.
            return estimateFundamental (take.left,
                                        (std::size_t) (sampleRate * 1.60),
                                        (std::size_t) (sampleRate * 1.72),
                                        sampleRate, 150.0, 700.0);
        };
        expectNear (octaveArrivesBy (3), 440.0, 20.0,
                    "a three-step END STEP completes its pass by 1.6 s");
        expectNear (octaveArrivesBy (8), 220.0, 12.0,
                    "an eight-step one has not");
    }

    // 11. A plain run walks one held key across every one of its rows, so a
    //     single key repeats the same pitch on every step. Voices are
    //     released by pitch, so a gate allowed to outlive its own grid ends
    //     the note that just started: at DURATION 100 % that silenced the
    //     whole pattern, and at 120 % it cut every note short.
    {
        septum::Patch run = base;
        run.arpeggio.grid = septum::ArpeggioGrid::Eighth;
        run.arpeggio.duration = septum::ArpeggioDuration::P100;
        septum::applyArpeggioStyle (run, 0);         // "Straight 4", one row a step

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (run);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 2.0, sampleRate);
        const double sounding = take.rms ((std::size_t) (sampleRate * 0.5),
                                          take.left.size());
        expect (sounding > 0.02,
                "a plain run on one held key still sounds at DURATION 100 % ("
                    + std::to_string (sounding) + ")");

        septum::Patch longer = run;
        longer.arpeggio.duration = septum::ArpeggioDuration::P120;
        septum::Engine wide;
        wide.prepare (sampleRate, 256);
        wide.setPatch (longer);
        wide.reset();
        auto overlapped = renderScore (wide, { { 0.0, true, 60, 100 } }, 2.0,
                                       sampleRate);
        const double wider = overlapped.rms ((std::size_t) (sampleRate * 0.5),
                                             overlapped.left.size());
        expect (wider > 0.9 * sounding,
                "and a 120 % gate on a repeated pitch is not cut shorter than "
                "a 100 % one (" + std::to_string (wider) + " against "
                    + std::to_string (sounding) + ")");
    }
}

void testArpeggioOctaveRange()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.level = 127;
    patch.tempo = 240;
    patch.arpeggio.on = true;
    patch.arpeggio.grid = septum::ArpeggioGrid::Quarter;
    patch.arpeggio.duration = septum::ArpeggioDuration::P100;
    patch.arpeggio.motif = septum::ArpeggioMotif::Up;
    patch.arpeggio.octaveRange = 1;
    // A one-step, one-row style: every step is the same key, so only the
    // octave shift can change the pitch.
    patch.arpeggio.style = septum::ArpeggioStyle {};
    patch.arpeggio.style.endStep = 1;
    patch.arpeggio.style.cells[0][0] = 100;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 57, 100 } }, 1.2, sampleRate);

    // Step 1 is A2 (220 Hz); step 2 is one octave up (440 Hz).
    const double first = estimateFundamental (take.left, 2205, 8820, sampleRate,
                                              150.0, 700.0);
    const double second = estimateFundamental (take.left, 13230, 19845, sampleRate,
                                               150.0, 700.0);
    expectNear (first, 220.0, 6.0, "the first arpeggio cycle plays the key");
    expectNear (second, 440.0, 12.0,
                "OCTAVE RANGE +1 shifts the next cycle an octave up");

    // Near the top of the keyboard the octave cycle used to be clamped into
    // the MIDI range, which collapsed cycles onto one pitch: note 108 with
    // OCTAVE RANGE +2 played 108, 120, 127 where it owes 108, 120, 132.
    // Pitch is a number of semitones here, not a MIDI note. Rendered at
    // 96 kHz so the third cycle is nowhere near Nyquist and the estimate is
    // not fighting the anti-aliasing.
    const double fastRate = 96000.0;
    septum::Patch high = patch;
    high.arpeggio.octaveRange = 2;

    septum::Engine tall;
    tall.prepare (fastRate, 256);
    tall.setPatch (high);
    tall.reset();
    auto top = renderScore (tall, { { 0.0, true, 108, 100 } }, 1.0, fastRate);
    // Steps are 0.25 s at 240 BPM, so the third cycle runs 0.50-0.75 s. Note
    // 132 is 16744 Hz; the clamp put it at 127, which is 12544 Hz.
    const double third = estimateFundamental (top.left,
                                              (std::size_t) (fastRate * 0.55),
                                              (std::size_t) (fastRate * 0.72),
                                              fastRate, 11000.0, 20000.0);
    expectNear (third, 16744.0, 400.0,
                "the octave cycle keeps its interval past the top of the "
                "MIDI range");
}

// Three things the review found on the external-input path, each now fenced.
// MODULATION ASSIGN is one patch-common setting, the lever is one lever, and
// the AUDIO FILTER is one filter — so the lever's reach into it must not
// depend on how many tones happen to be sounding. It was summed per sounding
// tone, which doubled it in DUAL and SPLIT.
void testModulationLeverReachesTheAudioFilterOnce()
{
    const double sampleRate = 44100.0;

    const auto sideLevel = [sampleRate] (septum::KeyboardMode mode,
                                         double lever)
    {
        septum::Patch patch = plainSawPatch();
        patch.keyboardMode = mode;
        patch.keyboardPart = septum::KeyboardPart::Upper;
        patch.modulationAssign = septum::ModulationAssign::AudioFilter;
        // A square LFO at the slowest rate holds +1 for the whole take, so
        // the lever's reach is a fixed number of octaves rather than a sweep.
        for (septum::TonePatch* tone : { &patch.upper, &patch.lower })
        {
            tone->lfo2.shape = septum::LfoShape::Sqr;
            tone->lfo2.rate = 0;
            tone->lfo2.fadeTime = 0;
        }

        septum::ExternalInput settings {};
        settings.inputVolume = 127;
        settings.filterOn = true;
        settings.type = septum::AudioFilterType::Lpf;
        settings.slope = septum::FilterSlope::Db12;
        settings.cutoff = 30;      // about 103 Hz, well under both test tones
        settings.resonance = 0;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.setExternalInput (settings);
        engine.reset();
        engine.setModulation (lever);
        auto take = renderWithExternalInput (engine, {}, 0.3, sampleRate);
        return goertzel (take.out.left, 4410, 13230, 900.0, sampleRate);
    };

    const double closed = sideLevel (septum::KeyboardMode::Single, 0.0);
    const double single = sideLevel (septum::KeyboardMode::Single, 1.0);
    const double dual = sideLevel (septum::KeyboardMode::Dual, 1.0);
    const double split = sideLevel (septum::KeyboardMode::Split, 1.0);

    expect (single > 4.0 * std::max (1.0e-9, closed),
            "the modulation lever opens the audio filter at all");
    expect (std::abs (dual - single) < 0.05 * single,
            "DUAL reaches the audio filter exactly as far as SINGLE (single "
                + std::to_string (single) + ", dual " + std::to_string (dual)
                + ")");
    expect (std::abs (split - single) < 0.05 * single,
            "SPLIT reaches the audio filter exactly as far as SINGLE (single "
                + std::to_string (single) + ", split " + std::to_string (split)
                + ")");
}

void testExternalMonitorTiming()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.level = 127;

    septum::ExternalInput settings {};
    settings.filterOn = false;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.setExternalInput (settings);
    engine.reset();
    const int latency = engine.latencySamples();
    expect (latency > 0, "44.1 kHz carries the overdrive chain's group delay");

    // A single impulse on the INPUT jacks must come out exactly `latency`
    // samples late — the same delay every voice carries and the plug-in
    // reports — or a monitored input would arrive ahead of the whole track.
    const std::size_t total = 512;
    std::vector<float> left (total, 0.0f), right (total, 0.0f);
    std::vector<float> inL (total, 0.0f), inR (total, 0.0f);
    inL[8] = 0.5f;
    inR[8] = 0.5f;
    engine.process (left.data(), right.data(), (int) total, inL.data(), inR.data());

    std::size_t peakAt = 0;
    double peak = 0.0;
    for (std::size_t i = 0; i < total; ++i)
        if (std::abs ((double) left[i]) > peak)
        {
            peak = std::abs ((double) left[i]);
            peakAt = i;
        }
    expect (peak > 1.0e-3, "the impulse reaches the output");
    expect ((int) peakAt == 8 + latency,
            "the direct monitor carries the reported latency (peak at "
                + std::to_string (peakAt) + ", expected "
                + std::to_string (8 + latency) + ")");
}

void testExternalMonitorHandover()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = septum::Waveform::ExtIn;
    patch.upper.balance = -63;
    // AMP LEVEL 0: the EXT-IN voice still owns the input, so the monitor
    // fades out, but the voice contributes nothing of its own. The only
    // thing moving in the output is the fade, which is what is under test.
    patch.upper.level = 0;

    septum::ExternalInput settings {};
    settings.filterOn = false;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.setExternalInput (settings);
    engine.reset();

    // DC on the input, so a step in the monitor's gain is a step in the
    // output rather than something hiding inside a waveform.
    const std::size_t total = (std::size_t) (sampleRate * 0.2);
    std::vector<float> left (total, 0.0f), right (total, 0.0f);
    std::vector<float> inL (total, 0.25f), inR (total, 0.25f);
    const std::size_t block = 64;
    const auto pressAt = (std::size_t) (sampleRate * 0.05);
    for (std::size_t at = 0; at < total; at += block)
    {
        if (at >= pressAt && at < pressAt + block)
            engine.noteOn (60, 100);
        const auto n = std::min (block, total - at);
        engine.process (left.data() + at, right.data() + at, (int) n,
                        inL.data() + at, inR.data() + at);
    }

    // Walked sample by sample the fade's largest step is about an eighth of
    // what a once-per-control-tick fade produces, which is the whole point:
    // the mechanism that exists to avoid a discontinuity must not replace it
    // with a staircase of smaller ones.
    double worst = 0.0;
    for (std::size_t i = pressAt; i + 1 < pressAt + (std::size_t) (sampleRate * 0.012);
         ++i)
        worst = std::max (worst, std::abs ((double) left[i + 1] - left[i]));
    expect (worst < 0.0015,
            "the monitor fades out sample by sample, not tick by tick (worst "
                "step " + std::to_string (worst) + ")");
}

// The contract says the classic waveforms are polyBLEP/polyBLAMP band-limited
// at the host rate; deliberate aliasing belongs to SUPER SAW and SYNC. TRI's
// polyBLAMP coefficient was twice its correct value, which measures the same
// as no correction at all, so this check compares the shipping triangle
// against the same oscillator with its correction switched off — by rendering
// the corner-free SINE as the control and the raw partial structure of TRI
// against it.
void testTriangleIsBandLimited()
{
    const double sampleRate = 44100.0;
    const auto aliasRatio = [sampleRate] (int note)
    {
        septum::Patch patch = plainSawPatch();
        patch.upper.osc1.wave = septum::Waveform::Triangle;
        patch.upper.ampEnvSustain = 127;
        patch.upper.lowFreq = septum::LowFreqMode::Flat;

        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, note, 100 } }, 1.2,
                                 sampleRate);

        const double f0 = 440.0 * std::exp2 ((note - 69) / 12.0);
        const std::size_t from = 8820, to = take.left.size();
        double harmonic = 0.0, alias = 0.0;
        // Every eighth of the fundamental: the ones that land on a harmonic
        // are signal, the rest can only be folded energy.
        for (double f = f0 / 8.0; f < 20000.0 && f < 0.49 * sampleRate;
             f += f0 / 8.0)
        {
            const double m = goertzel (take.left, from, to, f, sampleRate);
            const double k = f / f0;
            (std::abs (k - std::round (k)) < 0.1 ? harmonic : alias) += m * m;
        }
        return alias / std::max (1.0e-30, harmonic);
    };

    // At the coefficient that measures as no correction the ratio is about
    // -87 dB at note 93; corrected it is far below that.
    expect (aliasRatio (93) < 1.0e-11,
            "a mid-register triangle folds far less than an uncorrected one");
    expect (aliasRatio (105) < 1.0e-11,
            "a high triangle folds far less than an uncorrected one");
}

void testSelfOscillationBounded()
{
    septum::Patch patch = plainSawPatch();
    patch.upper.filterType = septum::FilterType::Lpf;
    patch.upper.filterSlope = septum::FilterSlope::Db24;
    patch.upper.cutoff = 64;
    patch.upper.resonance = 127;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 2.0);
    expect (take.finite(), "self-oscillation stays finite");
    expect (take.peak() <= 1.06, "self-oscillation stays inside the limiter");
    expect (take.rms (66150, 88200) > 0.005,
            "full resonance sustains oscillation while the key is held");
}

// The manual's resonance warning is not written per filter type: far right,
// the filter reaches sustained oscillation. Scaling the band-pass tap by the
// SVF damping used to make that impossible on BPF — the damping is what
// RESONANCE drives to zero — so the band-pass lost 21 dB across the top of
// the knob where the low-pass gained 20.
void testBandPassGainsWithResonance()
{
    const auto bandLevel = [] (septum::FilterType type, int resonance)
    {
        septum::Patch patch = plainSawPatch();
        patch.upper.osc1.wave = septum::Waveform::Noise;
        patch.upper.filterType = type;
        patch.upper.filterSlope = septum::FilterSlope::Db12;
        patch.upper.cutoff = 64;
        patch.upper.resonance = resonance;
        patch.upper.ampEnvSustain = 127;

        septum::Engine engine;
        engine.prepare (96000.0, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 1.5, 96000.0);
        return take.rms (48000, 144000);
    };

    const double quiet = bandLevel (septum::FilterType::Bpf, 0);
    const double loud = bandLevel (septum::FilterType::Bpf, 120);
    expect (quiet > 1.0e-6, "the band-pass passes its band at resonance 0");
    expect (loud > 6.0 * quiet,
            "RESONANCE lifts the band-pass by at least 15 dB, as it does the "
            "low-pass");

    // And the top of the travel oscillates, exactly as it does on the other
    // resonant types.
    septum::Patch patch = plainSawPatch();
    patch.upper.filterType = septum::FilterType::Bpf;
    patch.upper.filterSlope = septum::FilterSlope::Db24;
    patch.upper.cutoff = 64;
    patch.upper.resonance = 127;
    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 2.0);
    expect (take.finite(), "the band-pass stays finite at full resonance");
    expect (take.peak() <= 1.06, "the band-pass stays inside the limiter");
    expect (take.rms (66150, 88200) > 0.005,
            "full resonance sustains oscillation on the band-pass too");
}

void testEnvelopesShapeLoudness()
{
    septum::Patch fast = plainSawPatch();
    fast.upper.ampEnvAttack = 0;
    septum::Patch slow = plainSawPatch();
    slow.upper.ampEnvAttack = 110;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (fast);
    engine.reset();
    auto fastTake = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.3);
    engine.setPatch (slow);
    engine.reset();
    auto slowTake = renderScore (engine, { { 0.0, true, 60, 100 } }, 0.3);
    expect (fastTake.rms (0, 4410) > 8.0 * std::max (1.0e-9, slowTake.rms (0, 4410)),
            "attack time audibly delays the onset");

    // Release: R=0 dies quickly, R=110 rings on.
    septum::Patch shortRelease = plainSawPatch();
    shortRelease.upper.ampEnvRelease = 0;
    septum::Patch longRelease = plainSawPatch();
    longRelease.upper.ampEnvRelease = 110;
    engine.setPatch (shortRelease);
    engine.reset();
    auto shortTake = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.3, false, 60, 0 } }, 1.0);
    engine.setPatch (longRelease);
    engine.reset();
    auto longTake = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.3, false, 60, 0 } }, 1.0);
    expect (longTake.rms (30870, 44100)
                > 20.0 * std::max (1.0e-9, shortTake.rms (30870, 44100)),
            "release time audibly extends the tail");
}

void testVelocitySensitivity()
{
    septum::Patch patch = plainSawPatch();
    patch.upper.levelVelocitySens = 63;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto loud = renderScore (engine, { { 0.0, true, 60, 127 } }, 0.3);
    engine.reset();
    auto quiet = renderScore (engine, { { 0.0, true, 60, 20 } }, 0.3);
    expect (loud.rms (4410, 13230) > 3.0 * std::max (1.0e-9, quiet.rms (4410, 13230)),
            "positive level velocity sensitivity scales loudness");
}

void testEffectTails()
{
    // Delay: energy persists after a released short note only when on.
    septum::Patch dry = plainSawPatch();
    dry.upper.ampEnvRelease = 0;
    septum::Patch wet = dry;
    wet.delayOn = true;
    wet.upper.delayDepth = 100;
    septum::applyDelayTemplate (wet, 3);  // Long Delay
    wet.delay.feedback = 60;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (dry);
    engine.reset();
    auto dryTake = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.1, false, 60, 0 } }, 2.0);
    engine.setPatch (wet);
    engine.reset();
    auto wetTake = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.1, false, 60, 0 } }, 2.0);
    expect (wetTake.rms (66150, 88200)
                > 20.0 * std::max (1.0e-9, dryTake.rms (66150, 88200)),
            "the delay leaves repeats after the note ends");

    // Reverb: longer TIME leaves a longer tail.
    septum::Patch verbShort = dry;
    verbShort.reverbOn = true;
    verbShort.upper.reverbDepth = 100;
    verbShort.reverb.time = 10;
    septum::Patch verbLong = verbShort;
    verbLong.reverb.time = 120;
    engine.setPatch (verbShort);
    engine.reset();
    auto shortTail = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.1, false, 60, 0 } }, 2.5);
    engine.setPatch (verbLong);
    engine.reset();
    auto longTail = renderScore (
        engine, { { 0.0, true, 60, 100 }, { 0.1, false, 60, 0 } }, 2.5);
    expect (longTail.rms (88200, 110250)
                > 4.0 * std::max (1.0e-9, shortTail.rms (88200, 110250)),
            "reverb TIME extends the tail");
}

void testDcBlockedOutput()
{
    // A 95 % pulse sustained: the raw wave has a large mean; the documented
    // output coupling must remove it.
    septum::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = septum::Waveform::PulseSquare;
    patch.upper.osc1.pulseWidth = 127;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 45, 100 } }, 2.0);
    // Skip the onset transient; the settled 0.33 Hz pole is slow.
    Render tail;
    tail.left.assign (take.left.begin() + 44100, take.left.end());
    tail.right.assign (take.right.begin() + 44100, take.right.end());
    expect (std::abs (tail.mean()) < 0.02,
            "the output stage blocks the pulse wave's DC component");
}

void testFactoryBankRendersEverywhere()
{
    const auto& bank = septum::factoryPatches();
    expect (bank.size() >= 12, "the factory bank has at least twelve patches");

    for (const auto& entry : bank)
        expect (entry.patch.name.size() <= 12,
                std::string ("patch name fits 12 characters: ") + entry.name);

    for (double sampleRate : { 44100.0, 96000.0 })
    {
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        for (const auto& entry : bank)
        {
            engine.setPatch (entry.patch);
            engine.reset();
            std::vector<Event> events {
                { 0.0, true, 48, 96 },   { 0.4, false, 48, 0 },
                { 0.15, true, 60, 112 }, { 0.6, false, 60, 0 },
                { 0.3, true, 67, 80 },   { 0.7, false, 67, 0 },
            };
            auto take = renderScore (engine, events, 1.4, sampleRate);
            const std::string label = std::string (entry.name) + " at "
                                      + std::to_string ((int) sampleRate);
            expect (take.finite(), label + " renders finite audio");
            expect (take.peak() <= 1.06, label + " stays inside the limiter");
            expect (take.peak() > 1.0e-4, label + " is not silent");
        }
    }
}

void testAllSoundOffSilencesEffectTails()
{
    // All Sounds Off is a panic: the delay's buffered repeats and the reverb
    // tail must stop with the voices, however high the feedback.
    septum::Patch patch = plainSawPatch();
    patch.delayOn = true;
    patch.upper.delayDepth = 110;
    patch.delay.time = 100;
    patch.delay.feedback = 90;
    patch.reverbOn = true;
    patch.upper.reverbDepth = 110;
    patch.reverb.time = 120;

    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    (void) renderScore (engine, { { 0.0, true, 60, 110 }, { 0.4, false, 60, 0 } },
                        1.0);
    engine.allSoundOff();
    // The panic marks the effect histories stale rather than clearing them,
    // so the silence must hold beyond the longest possible delay read — the
    // window where wrongly-unmuted stale material would re-emerge.
    std::vector<float> left (88200), right (88200);
    engine.process (left.data(), right.data(), (int) left.size());
    double peak = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i)
        peak = std::max ({ peak, std::abs ((double) left[i]),
                           std::abs ((double) right[i]) });
    expect (peak < 1.0e-6, "all-sound-off silences the effect tails too");

    // And the effects keep working: a note struck after the panic still gets
    // its delay repeats once its own dry sound has died away.
    auto echoes = renderScore (engine, { { 0.0, true, 60, 110 },
                                         { 0.1, false, 60, 0 } },
                               1.0);
    double echoPeak = 0.0;
    for (std::size_t i = 22050; i < echoes.left.size(); ++i)
        echoPeak = std::max ({ echoPeak, std::abs ((double) echoes.left[i]),
                               std::abs ((double) echoes.right[i]) });
    expect (echoPeak > 1.0e-4, "post-panic notes still feed the effects");
}

// The sync reset is documented as naive, and a naive reset drops the saw to
// the bottom of its cycle. The band-limiting residual describes the
// discontinuity a free-running oscillator makes — a whole cycle — so applying
// it to a reset that jumped by whatever fraction of a cycle OSC1 had reached
// put a spike where the reset belongs: post-reset values ran as high as
// -0.09 of the peak, where the reset owes about -1.
void testSyncResetLandsAtTheBottomOfTheCycle()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = plainSawPatch();
    septum::TonePatch& tone = patch.upper;
    tone.osc1.wave = septum::Waveform::Saw;
    tone.osc2.wave = septum::Waveform::Saw;
    // An octave below the slave clock, so OSC1 never completes a cycle of its
    // own between resets and every downward jump in the take is a reset.
    tone.osc1.coarse = -12;
    tone.osc1.fine = 7;
    tone.mixType = septum::MixModType::Sync;
    tone.balance = -63;                 // OSC1 alone
    tone.ampEnvSustain = 127;
    tone.level = 127;
    tone.lowFreq = septum::LowFreqMode::Flat;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.reset();
    const double seconds = 0.5;
    auto take = renderScore (engine, { { 0.0, true, 48, 100 } }, seconds,
                             sampleRate);

    const std::size_t from = (std::size_t) (sampleRate * 0.05);
    double peak = 0.0;
    for (std::size_t i = from; i < take.left.size(); ++i)
        peak = std::max (peak, std::abs ((double) take.left[i]));
    expect (peak > 0.01, "the synced oscillator sounds");

    int resets = 0, shallow = 0;
    for (std::size_t i = from + 1; i < take.left.size(); ++i)
    {
        if ((double) take.left[i] - take.left[i - 1] >= -0.35 * peak)
            continue;
        ++resets;
        if ((double) take.left[i] > -0.5 * peak)
            ++shallow;
    }

    // OSC2 is note 48; the reset rate is its fundamental.
    const double expected = 440.0 * std::exp2 ((48 - 69) / 12.0)
                            * (seconds - 0.05);
    expect ((double) resets < 1.1 * expected,
            "sync produces one downward jump per slave cycle, not more (saw "
                + std::to_string (resets) + ", expected about "
                + std::to_string ((int) expected) + ")");
    expect (shallow == 0,
            "every sync reset lands at the bottom of the cycle (" 
                + std::to_string (shallow) + " of " + std::to_string (resets)
                + " landed short)");
}

void testSyncFollowsSpecialOsc2Waves()
{
    // SYNC must fire from OSC2's cycle even when OSC2 is a SUPER SAW (the
    // center saw carries the cycle), an FB OSC, or EXT-IN — where the
    // oscillator keeps running underneath the substituted signal.
    for (const auto wave : { septum::Waveform::SuperSaw, septum::Waveform::FbOsc,
                             septum::Waveform::ExtIn })
    {
        septum::Patch patch = plainSawPatch();
        patch.upper.osc1.coarse = 7;
        patch.upper.mixType = septum::MixModType::Sync;
        patch.upper.balance = -63;
        patch.upper.osc2.wave = wave;
        patch.upper.osc2.pulseWidth = 0;  // collapsed spread / no feedback

        septum::Engine engine;
        engine.prepare (44100.0, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 69, 100 } }, 1.0);
        const double hz = estimateFundamental (take.left, 11025, 44100, 44100.0,
                                               300.0, 700.0);
        expectNear (hz, 440.0, 3.0,
                    "sync tracks the OSC2 fundamental for special waves");
    }
}

// The reported "fast ADSR response times ensure bags of punch" has to be true
// of the filter envelope as well as the amp envelope: both read the same
// slider through the same mapping, so from A = 0 they must open in the same
// time. Measured on NOISE, which has no oscillator period to confound the
// envelope trace, as RMS over 0.25 ms windows.
double envelopeRiseMs (const Render& take, double sampleRate, double fraction)
{
    const auto window = (std::size_t) (sampleRate * 0.00025);
    std::vector<double> trace;
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < take.left.size(); ++i)
    {
        sum += take.left[i] * (double) take.left[i];
        if (++count == window)
        {
            trace.push_back (std::sqrt (sum / (double) window));
            sum = 0.0;
            count = 0;
        }
    }
    double peak = 0.0;
    for (double value : trace)
        peak = std::max (peak, value);
    for (std::size_t i = 0; i < trace.size(); ++i)
        if (trace[i] >= fraction * peak)
            return (double) i * 0.25;
    return 1.0e9;
}

void testFilterEnvelopeIsAsFastAsTheAmpEnvelope()
{
    const double sampleRate = 44100.0;
    septum::Patch base = septum::initPatch();
    base.upper.osc1.wave = septum::Waveform::Noise;
    base.upper.balance = -63;
    base.upper.level = 127;
    base.upper.ampEnvAttack = 0;
    base.upper.ampEnvDecay = 127;
    base.upper.ampEnvSustain = 127;

    septum::Patch amp = base;
    amp.upper.filterType = septum::FilterType::Bypass;

    septum::Patch filter = base;
    filter.upper.filterType = septum::FilterType::Lpf;
    filter.upper.filterSlope = septum::FilterSlope::Db24;
    filter.upper.cutoff = 10;
    filter.upper.resonance = 0;
    filter.upper.filterEnvAttack = 0;
    filter.upper.filterEnvDecay = 127;
    filter.upper.filterEnvSustain = 127;
    filter.upper.filterEnvDepth = 63;

    const auto rise = [&] (const septum::Patch& patch, double fraction)
    {
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        return envelopeRiseMs (
            renderScore (engine, { { 0.0, true, 60, 127 } }, 0.06, sampleRate),
            sampleRate, fraction);
    };

    const double ampRise = rise (amp, 0.9);
    const double filterRise = rise (filter, 0.9);
    expect (ampRise <= 2.0,
            "the amp envelope's fastest attack opens within 2 ms (value "
                + std::to_string (ampRise) + " ms)");
    expect (filterRise <= ampRise + 0.5,
            "the filter envelope's fastest attack is no slower than the amp "
            "envelope's (filter " + std::to_string (filterRise) + " ms, amp "
                + std::to_string (ampRise) + " ms)");
}

// The slew the filter envelope was taken out of is still doing its job on the
// panel side: a stepped S&H LFO must not arrive as a discontinuity, so the
// per-sample coefficient change stays bounded even at the extremes.
void testSampleHoldStepsStayContinuous()
{
    const double sampleRate = 44100.0;
    septum::Patch patch = septum::initPatch();
    patch.upper.osc1.wave = septum::Waveform::Saw;
    patch.upper.balance = -63;
    patch.upper.level = 127;
    patch.upper.ampEnvAttack = 0;
    patch.upper.ampEnvSustain = 127;
    patch.upper.filterType = septum::FilterType::Lpf;
    patch.upper.filterSlope = septum::FilterSlope::Db24;
    patch.upper.cutoff = 64;
    patch.upper.resonance = 100;
    patch.upper.filterEnvDepth = 0;
    patch.upper.lfo1.shape = septum::LfoShape::SampleHold;
    patch.upper.lfo1.rate = 110;         // fast steps
    patch.upper.lfo1.destination1 = septum::LfoDest1::Filter;
    patch.upper.lfo1.depth1 = 63;

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 45, 100 } }, 2.0, sampleRate);
    expect (take.finite(), "an S&H filter LFO renders finite audio");
    expect (take.peak() <= 1.06, "an S&H filter LFO stays inside the limiter");
    // No sample-to-sample jump larger than the waveform itself can make.
    double worstJump = 0.0;
    for (std::size_t i = 1; i < take.left.size(); ++i)
        worstJump = std::max (worstJump,
                              std::abs ((double) take.left[i] - take.left[i - 1]));
    expect (worstJump < 0.9,
            "an S&H filter step never produces a sample-level discontinuity "
            "(worst jump " + std::to_string (worstJump) + ")");
}

// The 25th harmonic of 1760 Hz is 44 kHz: at a 44.1 kHz host rate it folds
// back to exactly 100 Hz, and at every other rate it does not fold at all.
// The level of that 100 Hz line is therefore a direct read of how much the
// shaper's evaluation rate is leaking into the instrument's character.
void testOverdriveDoesNotFoldAtTheHostRate()
{
    septum::Patch patch = septum::initPatch();
    patch.upper.osc1.wave = septum::Waveform::Sine;
    patch.upper.balance = -63;
    patch.upper.filterType = septum::FilterType::Bypass;
    patch.upper.overdrive = true;
    patch.upper.drive = 127;
    patch.upper.level = 127;
    patch.upper.ampEnvAttack = 0;
    patch.upper.ampEnvDecay = 127;
    patch.upper.ampEnvSustain = 127;

    for (double sampleRate : { 44100.0, 48000.0, 88200.0 })
    {
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        auto take = renderScore (engine, { { 0.0, true, 93, 100 } }, 1.2, sampleRate);
        const auto from = (std::size_t) (sampleRate * 0.2);
        const auto to = take.left.size();
        const double fold = goertzel (take.left, from, to, 100.0, sampleRate);
        const double fundamental = goertzel (take.left, from, to, 1760.0, sampleRate);
        const double ratioDb = 20.0 * std::log10 (fold / std::max (1.0e-30, fundamental));
        expect (ratioDb < -60.0,
                "full DRIVE does not fold its 25th harmonic into the audible band at "
                    + std::to_string ((int) sampleRate) + " Hz (" + std::to_string (ratioDb)
                    + " dB)");
    }
}

// Every voice carries the overdrive stage's group delay whether it is shaping
// or not, so a clean tone layered under an overdriven one stays in phase with
// it. Cross-correlate the two paths and require the peak at lag zero.
void testOverdriveKeepsCleanVoicesAligned()
{
    const double sampleRate = 44100.0;
    septum::Patch clean = plainSawPatch();
    clean.upper.balance = -63;
    clean.upper.level = 127;
    clean.upper.overdrive = false;
    septum::Patch driven = clean;
    driven.upper.overdrive = true;
    driven.upper.drive = 0;   // unity pre-gain: the same waveform, gently shaped

    const auto render = [&] (const septum::Patch& patch)
    {
        septum::Engine engine;
        engine.prepare (sampleRate, 256);
        engine.setPatch (patch);
        engine.reset();
        return renderScore (engine, { { 0.0, true, 57, 100 } }, 0.5, sampleRate);
    };
    const auto a = render (clean);
    const auto b = render (driven);

    const std::size_t from = 4410, to = 17640;
    double best = -1.0e30;
    int bestLag = 99;
    for (int lag = -24; lag <= 24; ++lag)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += a.left[i] * (double) b.left[(std::size_t) ((long long) i + lag)];
        if (sum > best)
        {
            best = sum;
            bestLag = lag;
        }
    }
    expect (bestLag == 0,
            "the clean and overdriven paths share one group delay (peak at lag "
                + std::to_string (bestLag) + ")");

    septum::Engine engine;
    engine.prepare (sampleRate, 256);
    expect (engine.latencySamples() == 19,
            "the reported latency is the overdrive chain's group delay at 44.1 kHz "
            "(value " + std::to_string (engine.latencySamples()) + ")");
    engine.prepare (96000.0, 256);
    expect (engine.latencySamples() == 16,
            "96 kHz needs only the outer half-band, so the latency drops (value "
                + std::to_string (engine.latencySamples()) + ")");
    engine.prepare (192000.0, 256);
    expect (engine.latencySamples() == 0,
            "above the shaper's internal rate there is nothing to resample (value "
                + std::to_string (engine.latencySamples()) + ")");
}

void testAllNotesOffAndReset()
{
    septum::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (plainSawPatch());
    engine.reset();
    for (int i = 0; i < 5; ++i)
        engine.noteOn (50 + i, 100);
    std::vector<float> left (2205), right (2205);
    engine.process (left.data(), right.data(), 2205);
    expect (engine.activeVoiceCount() == 5, "five keys give five voices");
    engine.allSoundOff();
    expect (engine.activeVoiceCount() == 0, "all-sound-off empties the pool");
    // The documented 0.33 Hz output coupling discharges a small DC transient
    // after the abrupt cut; what must vanish immediately is the AC content.
    engine.process (left.data(), right.data(), 2205);
    engine.process (left.data(), right.data(), 2205);
    float low = 1.0f, high = -1.0f;
    for (auto sample : left)
    {
        low = std::min (low, sample);
        high = std::max (high, sample);
    }
    expect (high - low < 1.0e-4, "all-sound-off leaves no audible content");
}
} // namespace

int main()
{
    testMappingLaws();
    testRegisteredConstants();
    testTuningAndMasterTune();
    testDocumentedParameterGrids();
    testBalanceEndpoints();
    testRingModulation();
    testOscillatorSync();
    testSuperSawSpread();
    testTakingAVoiceOverDoesNotBlankIt();
    testOverdriveSwitchesBackInFromLiveState();
    testSplitAndDualVoicing();
    testControllerDestinations();
    testSoloAndHold();
    testSostenutoLatchesOnlyWhatWasSounding();
    testExternalInputAndAudioFilter();
    testModulationLeverReachesTheAudioFilterOnce();
    testExternalMonitorTiming();
    testExternalSwitchesAreCrossedNotThrown();
    testExternalMonitorHandover();
    testArpeggioMotifsMatchTheManualsExamples();
    testShortChordFallsBackOnTheHighestKey();
    testArpeggioGridDivisions();
    testShuffleFollowsTheBeatNotThePattern();
    testArpeggiatorPlaysAndHolds();
    testArpeggioOctaveRange();
    testArpeggiatorEdgeCases();
    testTriangleIsBandLimited();
    testSelfOscillationBounded();
    testBandPassGainsWithResonance();
    testEnvelopesShapeLoudness();
    testVelocitySensitivity();
    testEffectTails();
    testDcBlockedOutput();
    testFactoryBankRendersEverywhere();
    testAllSoundOffSilencesEffectTails();
    testSyncResetLandsAtTheBottomOfTheCycle();
    testSyncFollowsSpecialOsc2Waves();
    testFilterEnvelopeIsAsFastAsTheAmpEnvelope();
    testSampleHoldStepsStayContinuous();
    testOverdriveDoesNotFoldAtTheHostRate();
    testOverdriveKeepsCleanVoicesAligned();
    testAllNotesOffAndReset();

    if (failures == 0)
    {
        std::printf ("Septum engine tests passed (%d checks).\n", checks);
        return 0;
    }
    std::fprintf (stderr, "%d of %d checks failed.\n", failures, checks);
    return 1;
}

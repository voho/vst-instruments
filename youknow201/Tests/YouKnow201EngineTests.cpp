// JUCE-free engine tests: the documented architecture claims that can be
// checked without hardware are checked here — tuning against master tune,
// the settled enumeration semantics (balance endpoints, ring on the OSC1
// leg, sync tracking OSC2, split routing, DUAL halving polyphony), the
// mapping laws' endpoints, self-oscillation boundedness, effect tails, and
// finite, DC-free output for the whole factory bank at two sample rates.

#include "DSP/YouKnow201Engine.h"
#include "DSP/YouKnow201Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

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

Render renderScore (youknow201::Engine& engine, const std::vector<Event>& events,
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

youknow201::Patch plainSawPatch()
{
    youknow201::Patch patch = youknow201::initPatch();
    patch.upper.filterType = youknow201::FilterType::Bypass;
    patch.upper.ampEnvAttack = 0;
    patch.upper.ampEnvSustain = 127;
    patch.upper.ampEnvRelease = 0;
    return patch;
}

// ---------------------------------------------------------------------------

void testMappingLaws()
{
    using namespace youknow201::mapping;
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

void testTuningAndMasterTune()
{
    youknow201::Engine engine;
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

void testBalanceEndpoints()
{
    // Documented: balance fully left silences OSC2 entirely. The two renders
    // differ only in the OSC2 waveform, so identical output proves the leg
    // is silent.
    youknow201::Patch a = plainSawPatch();
    a.upper.osc2.wave = youknow201::Waveform::Sine;
    youknow201::Patch b = a;
    b.upper.osc2.wave = youknow201::Waveform::Triangle;

    youknow201::Engine engine;
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
    youknow201::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = youknow201::Waveform::Sine;
    patch.upper.osc2.wave = youknow201::Waveform::Sine;
    patch.upper.mixType = youknow201::MixModType::Ring;
    patch.upper.balance = -63;

    youknow201::Engine engine;
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
    youknow201::Patch patch = plainSawPatch();
    patch.upper.osc1.coarse = 7;
    patch.upper.mixType = youknow201::MixModType::Sync;
    patch.upper.balance = -63;

    youknow201::Engine engine;
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
    youknow201::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = youknow201::Waveform::SuperSaw;

    youknow201::Engine engine;
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

void testSplitAndDualVoicing()
{
    youknow201::Patch patch = plainSawPatch();
    patch.keyboardMode = youknow201::KeyboardMode::Split;
    patch.splitPoint = 60;
    patch.lower = patch.upper;
    patch.upper.level = 0;  // silence UPPER so routing is audible

    youknow201::Engine engine;
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
    youknow201::Patch dual = plainSawPatch();
    dual.keyboardMode = youknow201::KeyboardMode::Dual;
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

void testSoloAndHold()
{
    youknow201::Patch patch = plainSawPatch();
    patch.upper.mono = youknow201::MonoMode::Solo;

    youknow201::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    (void) renderScore (engine,
                        { { 0.0, true, 60, 100 }, { 0.05, true, 64, 100 } }, 0.2);
    expect (engine.activeVoiceCount() == 1, "solo keeps one voice");

    // Hold pedal: the note keeps sounding after note-off until pedal up.
    youknow201::Patch poly = plainSawPatch();
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

void testSelfOscillationBounded()
{
    youknow201::Patch patch = plainSawPatch();
    patch.upper.filterType = youknow201::FilterType::Lpf;
    patch.upper.filterSlope = youknow201::FilterSlope::Db24;
    patch.upper.cutoff = 64;
    patch.upper.resonance = 127;

    youknow201::Engine engine;
    engine.prepare (44100.0, 256);
    engine.setPatch (patch);
    engine.reset();
    auto take = renderScore (engine, { { 0.0, true, 60, 100 } }, 2.0);
    expect (take.finite(), "self-oscillation stays finite");
    expect (take.peak() <= 1.06, "self-oscillation stays inside the limiter");
    expect (take.rms (66150, 88200) > 0.005,
            "full resonance sustains oscillation while the key is held");
}

void testEnvelopesShapeLoudness()
{
    youknow201::Patch fast = plainSawPatch();
    fast.upper.ampEnvAttack = 0;
    youknow201::Patch slow = plainSawPatch();
    slow.upper.ampEnvAttack = 110;

    youknow201::Engine engine;
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
    youknow201::Patch shortRelease = plainSawPatch();
    shortRelease.upper.ampEnvRelease = 0;
    youknow201::Patch longRelease = plainSawPatch();
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
    youknow201::Patch patch = plainSawPatch();
    patch.upper.levelVelocitySens = 63;

    youknow201::Engine engine;
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
    youknow201::Patch dry = plainSawPatch();
    dry.upper.ampEnvRelease = 0;
    youknow201::Patch wet = dry;
    wet.delayOn = true;
    wet.upper.delayDepth = 100;
    youknow201::applyDelayTemplate (wet, 3);  // Long Delay
    wet.delay.feedback = 60;

    youknow201::Engine engine;
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
    youknow201::Patch verbShort = dry;
    verbShort.reverbOn = true;
    verbShort.upper.reverbDepth = 100;
    verbShort.reverb.time = 10;
    youknow201::Patch verbLong = verbShort;
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
    youknow201::Patch patch = plainSawPatch();
    patch.upper.osc1.wave = youknow201::Waveform::PulseSquare;
    patch.upper.osc1.pulseWidth = 127;

    youknow201::Engine engine;
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
    const auto& bank = youknow201::factoryPatches();
    expect (bank.size() >= 12, "the factory bank has at least twelve patches");

    for (const auto& entry : bank)
        expect (entry.patch.name.size() <= 12,
                std::string ("patch name fits 12 characters: ") + entry.name);

    for (double sampleRate : { 44100.0, 96000.0 })
    {
        youknow201::Engine engine;
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

void testAllNotesOffAndReset()
{
    youknow201::Engine engine;
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
    testTuningAndMasterTune();
    testBalanceEndpoints();
    testRingModulation();
    testOscillatorSync();
    testSuperSawSpread();
    testSplitAndDualVoicing();
    testSoloAndHold();
    testSelfOscillationBounded();
    testEnvelopesShapeLoudness();
    testVelocitySensitivity();
    testEffectTails();
    testDcBlockedOutput();
    testFactoryBankRendersEverywhere();
    testAllNotesOffAndReset();

    if (failures == 0)
    {
        std::printf ("YouKnow201 engine tests passed (%d checks).\n", checks);
        return 0;
    }
    std::fprintf (stderr, "%d of %d checks failed.\n", failures, checks);
    return 1;
}

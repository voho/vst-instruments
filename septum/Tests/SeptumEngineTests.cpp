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
    // host rate, so what it owes is a bound: every rate a host can plausibly
    // run at must land within about half an octave of the target, against the
    // 3.1 octaves those rates themselves span.
    double worst = 0.0;
    for (double rate : { 22050.0, 32000.0, 44100.0, 48000.0, 64000.0, 88200.0,
                         96000.0, 128000.0, 176400.0, 192000.0 })
    {
        const double internal = rate * overdriveOversampling (rate);
        worst = std::max (worst, std::abs (std::log2 (internal / overdriveInternalRateHz)));
    }
    expect (worst < 0.55,
            "and no host rate puts the shaper more than half an octave from it "
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

    // 6. ARPEGGIO VELOCITY = REAL follows the key each note came from, so a
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
}

// Three things the review found on the external-input path, each now fenced.
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
    testBalanceEndpoints();
    testRingModulation();
    testOscillatorSync();
    testSuperSawSpread();
    testSplitAndDualVoicing();
    testSoloAndHold();
    testSostenutoLatchesOnlyWhatWasSounding();
    testExternalInputAndAudioFilter();
    testExternalMonitorTiming();
    testExternalMonitorHandover();
    testArpeggioMotifsMatchTheManualsExamples();
    testArpeggioGridDivisions();
    testArpeggiatorPlaysAndHolds();
    testArpeggioOctaveRange();
    testArpeggiatorEdgeCases();
    testSelfOscillationBounded();
    testEnvelopesShapeLoudness();
    testVelocitySensitivity();
    testEffectTails();
    testDcBlockedOutput();
    testFactoryBankRendersEverywhere();
    testAllSoundOffSilencesEffectTails();
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

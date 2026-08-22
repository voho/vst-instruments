// Engine behaviour suite: what the instrument does when it is played, as
// opposed to what its individual circuit blocks compute. The circuit suite
// checks the laws; this one checks the machine — gating, keying, hostile
// input, and the behaviours the modelling contract anchors (no velocity,
// fallback without retrigger, the gate-select rule, VCA bypass droning).

#include "DSP/GhostarEngine.h"
#include "DSP/GhostarPresets.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using ghostar::EngineParameters;
using ghostar::GhostarEngine;

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

Rendered render(GhostarEngine& engine, double seconds, double sampleRate)
{
    constexpr int blockSize = 256;
    Rendered rendered;
    auto remaining = static_cast<int>(std::lround(seconds * sampleRate));
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    while (remaining > 0)
    {
        const int count = std::min(blockSize, remaining);
        engine.process(left.data(), right.data(), count);
        rendered.left.insert(rendered.left.end(), left.begin(),
                             left.begin() + count);
        rendered.right.insert(rendered.right.end(), right.begin(),
                              right.begin() + count);
        remaining -= count;
    }
    return rendered;
}

double peak(const Rendered& rendered)
{
    double result = 0.0;
    for (std::size_t index = 0; index < rendered.left.size(); ++index)
        result = std::max({ result,
                            std::abs(static_cast<double>(rendered.left[index])),
                            std::abs(static_cast<double>(rendered.right[index])) });
    return result;
}

double rms(const Rendered& rendered)
{
    if (rendered.left.empty())
        return 0.0;
    double sum = 0.0;
    for (const float value : rendered.left)
        sum += static_cast<double>(value) * static_cast<double>(value);
    return std::sqrt(sum / static_cast<double>(rendered.left.size()));
}

bool finite(const Rendered& rendered)
{
    for (std::size_t index = 0; index < rendered.left.size(); ++index)
        if (!std::isfinite(rendered.left[index])
            || !std::isfinite(rendered.right[index]))
            return false;
    return true;
}

// A bright, unfiltered panel: saw A through a fully open upper filter, no
// envelope-to-cutoff, instant attack, full sustain.
EngineParameters brightPanel()
{
    EngineParameters parameters;
    parameters.filterPathA = 0.8f;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.kbAmount = 0.0f;
    parameters.filterEnvAmount = 0.5f;
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessSustain = 1.0f;
    return parameters;
}

double zeroCrossingHz(const std::vector<float>& samples, double sampleRate)
{
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double seconds = static_cast<double>(samples.size()) / sampleRate;
    return static_cast<double>(crossings) / seconds;
}

void testSilentBeforeFirstNote()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(EngineParameters {});
    const auto rendered = render(engine, 0.25, 44100.0);
    check(finite(rendered), "idle render is finite");
    check(peak(rendered) == 0.0, "engine is silent before the first note");
}

void testNoteProducesAudibleFiniteAudio()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(48, 0.9f);
    const auto rendered = render(engine, 0.5, 44100.0);
    check(finite(rendered), "held note renders finite audio");
    check(peak(rendered) > 1.0e-3, "held note is audible");
}

// The panel's own warning: with every GATE SELECT switch off, the envelope
// generators never run, so a keyed note stays silent.
void testNoGateSourceMeansNoArticulation()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.gateKbd = false;
    parameters.gateX = false;
    parameters.gateYExt = false;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    const auto rendered = render(engine, 0.5, 44100.0);
    check(peak(rendered) < 1.0e-6,
          "with no gate source selected a keyed note stays silent");

    // VCA BYPASS bypasses the articulation entirely - the drone returns.
    parameters.vcaBypass = true;
    engine.setParameters(parameters);
    const auto droning = render(engine, 0.5, 44100.0);
    check(peak(droning) > 1.0e-3, "VCA BYPASS drones without any gate");
}

void testReleaseDecaysToSilence()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.loudnessRelease = 0.1f;
    engine.setParameters(parameters);
    engine.noteOn(60, 0.9f);
    render(engine, 0.4, 44100.0);
    engine.noteOff(60);
    render(engine, 2.0, 44100.0);
    const auto tail = render(engine, 0.25, 44100.0);
    check(finite(tail), "release tail is finite");
    check(peak(tail) < 1.0e-4, "released note decays to silence");
    check(!engine.isGateOpen(), "gate is closed after the last key is released");
}

// The hardware scanner's held-note memory: releasing the newest key falls
// back to the newest key still held, at its pitch.
void testHeldKeyFallbackRestoresPitch()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    engine.setParameters(brightPanel());

    engine.noteOn(57, 0.9f);
    render(engine, 0.3, 48000.0);
    engine.noteOn(69, 0.9f);
    render(engine, 0.3, 48000.0);
    engine.noteOff(69);
    check(engine.isGateOpen(),
          "releasing the newer key with an older key held keeps the gate open");
    render(engine, 0.2, 48000.0);
    const auto fallback = render(engine, 0.5, 48000.0);
    const double hz = zeroCrossingHz(fallback.left, 48000.0);
    check(std::abs(hz - 220.0) < 8.0,
          "the fallback sounds the older held key's pitch");
    engine.noteOff(57);
    check(!engine.isGateOpen(), "releasing the last held key closes the gate");
}

void testKeyMemorySpansTheWholeMidiDomain()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(brightPanel());
    for (int note = 0; note < 128; ++note)
        engine.noteOn(note, 0.9f);
    for (int note = 127; note >= 1; --note)
        engine.noteOff(note);
    check(engine.isGateOpen(),
          "the oldest of 128 held keys still holds the gate open");
    engine.noteOff(0);
    check(!engine.isGateOpen(), "releasing the final held key closes the gate");
}

// The hardware keyboard has no velocity; a soft strike and a hard strike
// sound at the same level. Velocity zero is running-status Note Off.
void testVelocityDoesNotScaleLoudness()
{
    GhostarEngine soft;
    soft.prepare(44100.0, 256);
    soft.setParameters(brightPanel());
    soft.noteOn(48, 0.2f);
    render(soft, 0.3, 44100.0);
    const auto softTail = render(soft, 0.4, 44100.0);

    GhostarEngine hard;
    hard.prepare(44100.0, 256);
    hard.setParameters(brightPanel());
    hard.noteOn(48, 1.0f);
    render(hard, 0.3, 44100.0);
    const auto hardTail = render(hard, 0.4, 44100.0);

    const double softLevel = rms(softTail);
    const double hardLevel = rms(hardTail);
    check(softLevel > 1.0e-4 && hardLevel > 1.0e-4,
          "both strikes are audible");
    check(std::abs(softLevel - hardLevel)
              < 0.02 * std::max(softLevel, hardLevel),
          "strike velocity does not scale loudness");
}

void testZeroVelocityNoteOnReleases()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(60, 0.9f);
    check(engine.isGateOpen(), "a sounding note holds the gate open");
    engine.noteOn(60, 0.0f);
    check(!engine.isGateOpen(),
          "a zero-velocity note-on releases the sounding note");
}

void testHostileRatesAreClamped()
{
    GhostarEngine engine;
    engine.prepare(0.0, 256);
    check(engine.getSampleRate() == GhostarEngine::minimumSupportedSampleRate,
          "a zero host rate clamps to the minimum supported rate");
    engine.prepare(1.0e9, 256);
    check(engine.getSampleRate() == GhostarEngine::maximumSupportedSampleRate,
          "an absurd host rate clamps to the maximum supported rate");

    engine.prepare(std::numeric_limits<double>::quiet_NaN(), 256);
    check(std::isfinite(engine.getSampleRate()),
          "a NaN host rate resolves to a finite rate");
    engine.setParameters(brightPanel());
    engine.noteOn(60, 1.0f);
    const auto nanRateRender = render(engine, 0.25, engine.getSampleRate());
    check(finite(nanRateRender), "post-NaN-rate render is finite");
}

void testTopNoteAtLowestRateStaysBounded()
{
    GhostarEngine engine;
    engine.prepare(8000.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(127, 1.0f);
    render(engine, 0.5, 8000.0);
    const auto settled = render(engine, 0.5, 8000.0);
    check(finite(settled), "top note at 8 kHz renders finite audio");
    check(peak(settled) < 4.0, "top note at 8 kHz stays bounded");
}

void testNonFiniteParametersAreSanitised()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters poisoned = brightPanel();
    poisoned.loudnessAttack = std::numeric_limits<float>::quiet_NaN();
    poisoned.filterDecay = std::numeric_limits<float>::infinity();
    poisoned.cutoff = -std::numeric_limits<float>::infinity();
    poisoned.shaperRate = std::numeric_limits<float>::quiet_NaN();
    engine.setParameters(poisoned);
    engine.noteOn(48, 0.9f);
    const auto rendered = render(engine, 0.5, 44100.0);
    check(finite(rendered), "poisoned parameters still render finite audio");

    engine.setParameters(brightPanel());
    const auto recovered = render(engine, 0.5, 44100.0);
    check(finite(recovered), "valid parameters recover a finite render");
    check(peak(recovered) > 1.0e-3,
          "the engine still sounds after recovering from poisoned parameters");
}

void testNonFinitePerformanceControlsAreSanitised()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(57, 0.9f);
    render(engine, 0.25, 44100.0);
    engine.setPitchBend(std::numeric_limits<float>::quiet_NaN());
    engine.setModWheel(std::numeric_limits<float>::quiet_NaN());
    engine.setShaperWheel(std::numeric_limits<float>::quiet_NaN());
    const auto rendered = render(engine, 0.5, 44100.0);
    check(finite(rendered), "NaN performance controls keep the render finite");
    check(peak(rendered) > 1.0e-3,
          "NaN performance controls do not silence the sounding note");
}

// A corrupted preset can hand setParameters() an out-of-range switch value;
// it must fall back to the power-on detent instead of indexing a lookup
// table out of bounds.
void testOutOfRangeSwitchesAreSanitised()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters poisoned = brightPanel();
    poisoned.octave = static_cast<ghostar::MasterOctave>(-1);
    poisoned.oscBRange = static_cast<ghostar::OscBRange>(99);
    poisoned.oscAWaveform = static_cast<ghostar::Waveform>(-7);
    poisoned.lowerMode = static_cast<ghostar::LowerFilterMode>(12);
    poisoned.shaperMode = static_cast<ghostar::ShaperMode>(-3);
    poisoned.arpeggiator = static_cast<ghostar::ArpeggiatorMode>(8);
    engine.setParameters(poisoned);
    engine.noteOn(60, 0.9f);
    const auto rendered = render(engine, 0.5, 44100.0);
    check(finite(rendered), "out-of-range switches still render finite audio");
    check(peak(rendered) > 1.0e-3,
          "out-of-range switches fall back to sounding detents");
}

// RESET mode is always multiple-trigger: a legato second press under SINGLE
// must restart the Shaper's cycle even though the ADSRs are not re-gated.
void testShaperResetRetriggersOnLegatoPress()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.filterPathA = 0.0f;
    parameters.shaperPathA = 0.8f;
    parameters.trigger = ghostar::TriggerMode::Single;
    parameters.shaperMode = ghostar::ShaperMode::Reset;
    parameters.shaperRate = 0.75f;   // a short single cycle
    parameters.shaperShape = 0.5f;
    engine.setParameters(parameters);

    engine.noteOn(48, 0.9f);
    render(engine, 1.5, 44100.0);   // the single cycle completes
    const auto quiet = render(engine, 0.4, 44100.0);
    check(peak(quiet) < 1.0e-3,
          "the RESET cycle has finished and the Shaper path is silent");

    engine.noteOn(55, 0.9f);        // legato: the first key is still held
    const auto retriggered = render(engine, 0.4, 44100.0);
    check(peak(retriggered) > 1.0e-3,
          "a legato press under SINGLE restarts the RESET cycle");
}

// RESET with the Shaper's own gate as the only selected source must not
// clamp itself at its own threshold: a key press has to yield one complete,
// audible rise/fall cycle.
void testShaperResetSelfGateCompletesItsCycle()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.filterPathA = 0.0f;
    parameters.shaperPathA = 0.8f;
    parameters.gateKbd = false;
    parameters.gateX = false;
    parameters.gateYExt = true;      // the Shaper gates itself
    parameters.shaperMode = ghostar::ShaperMode::Reset;
    parameters.shaperRate = 0.7f;
    engine.setParameters(parameters);

    engine.noteOn(57, 0.9f);
    const auto cycle = render(engine, 0.6, 44100.0);
    check(peak(cycle) > 1.0e-2,
          "a self-gated RESET cycle rises audibly instead of clamping at "
          "its own threshold");
}

void testFullResonanceStaysBounded()
{
    GhostarEngine marginal;
    marginal.prepare(8000.0, 256);
    auto hot = brightPanel();
    hot.resonance = 1.0f;
    hot.upperResonance = ghostar::UpperResonanceMode::Variable;
    marginal.setParameters(hot);
    marginal.noteOn(127, 1.0f);
    render(marginal, 30.0, 8000.0);
    const auto late = render(marginal, 2.0, 8000.0);
    check(finite(late), "long marginal full-resonance render is finite");
    check(peak(late) < 4.0,
          "long marginal full-resonance render does not accumulate");
}

// The lowpass integrator carries no bound of its own since the diode shunt
// became the resonant node's law, so the regenerative extremes — full
// resonance across the cutoff span, driven and undriven, OVERDRIVE's boost
// included, at more than one rate — are rendered here to hold the claim
// that the shunt alone bounds the loop.
void testRegenerativeExtremesStayBounded()
{
    for (const double rate : { 44100.0, 96000.0 })
    {
        for (const float cutoff : { 0.0f, 0.5f, 1.0f })
        {
            for (const bool driven : { false, true })
            {
                GhostarEngine engine;
                engine.prepare(rate, 256);
                auto hot = brightPanel();
                hot.resonance = 1.0f;
                hot.upperResonance = ghostar::UpperResonanceMode::Variable;
                hot.cutoff = cutoff;
                hot.lowerMode = ghostar::LowerFilterMode::Overdrive;
                hot.filterPathA = driven ? 1.0f : 0.0f;
                hot.filterPathNoise = driven ? 1.0f : 0.3f;
                hot.vcaBypass = true;
                engine.setParameters(hot);
                engine.noteOn(96, 1.0f);
                render(engine, 2.0, rate);
                const auto late = render(engine, 0.5, rate);
                check(finite(late),
                      "regenerative extreme stays finite without an "
                      "integrator bound");
                check(peak(late) < 4.0,
                      "regenerative extreme stays bounded without an "
                      "integrator bound");
            }
        }
    }
}

// The diode shunt is a term of the continuous system, so the same patch
// must converge to the same filter at every rate: the self-oscillation
// amplitude — set entirely by the nonlinearity — must agree between hosts.
// The per-sample maps this law replaced failed exactly this (their
// equilibrium scaled with the sample rate).
void testSelfOscillationLevelIsRateInvariant()
{
    const auto steadyRms = [](double rate) {
        GhostarEngine engine;
        engine.prepare(rate, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = 0.0f;
        parameters.filterPathNoise = 0.6f;
        parameters.resonance = 1.0f;
        parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
        parameters.cutoff = 0.6f;
        parameters.vcaBypass = true;
        engine.setParameters(parameters);
        render(engine, 0.2, rate);           // noise kick
        parameters.filterPathNoise = 0.0f;
        engine.setParameters(parameters);
        render(engine, 1.5, rate);           // settle
        return rms(render(engine, 1.0, rate));
    };

    const double at44 = steadyRms(44100.0);
    const double at96 = steadyRms(96000.0);
    check(at44 > 1.0e-3, "self-oscillation sustains at 44.1 kHz");
    check(at96 > 1.0e-3, "self-oscillation sustains at 96 kHz");
    const double ratioDb =
        20.0 * std::log10(std::max(at44, at96)
                          / std::max(1.0e-12, std::min(at44, at96)));
    check(ratioDb < 0.5,
          "self-oscillation level agrees across host rates within 0.5 dB");
}

// The arpeggiator steps the held keys; a held triad must produce a moving
// pitch, not one steady note.
void testArpeggiatorStepsHeldKeys()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.arpeggiator = ghostar::ArpeggiatorMode::Ripple;
    parameters.lfoRate = 0.8f;   // a fast, test-friendly clock
    engine.setParameters(parameters);
    engine.noteOn(48, 0.9f);
    engine.noteOn(55, 0.9f);
    engine.noteOn(64, 0.9f);
    render(engine, 0.5, 44100.0);

    // Measure windowed frequencies over a second of arpeggiation.
    std::vector<double> windows;
    for (int window = 0; window < 8; ++window)
    {
        const auto slice = render(engine, 0.125, 44100.0);
        windows.push_back(zeroCrossingHz(slice.left, 44100.0));
    }
    const auto [minIt, maxIt] =
        std::minmax_element(windows.begin(), windows.end());
    check(*minIt > 0.0, "arpeggiation never goes silent");
    check(*maxIt / std::max(*minIt, 1.0) > 1.25,
          "the arpeggiator moves between held pitches");
}

// The labelled attack time is the actual time-to-peak: at a half-second
// setting the level must still be climbing shortly before the half-second
// mark and at full level shortly after it.
void testAttackReachesPeakAtItsLabelledTime()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    // 0.5 s on the 5 ms - 10 s exponential: travel = ln(100)/ln(2000).
    parameters.loudnessAttack = 0.6059f;
    parameters.loudnessSustain = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(48, 0.9f);

    render(engine, 0.30, 44100.0);
    const auto climbing = render(engine, 0.10, 44100.0);   // 0.30..0.40 s
    render(engine, 0.15, 44100.0);
    const auto peaked = render(engine, 0.10, 44100.0);     // 0.55..0.65 s
    const double climbingLevel = rms(climbing);
    const double peakedLevel = rms(peaked);
    check(peakedLevel > 1.0e-3, "the attack reaches an audible peak");
    check(climbingLevel < 0.9 * peakedLevel,
          "the level is still climbing well before the labelled attack time");
    render(engine, 0.2, 44100.0);
    const auto settled = render(engine, 0.10, 44100.0);
    check(std::abs(rms(settled) - peakedLevel) < 0.08 * peakedLevel,
          "the level is at its peak just after the labelled attack time");
}

// The arpeggiator's octave steps are internal CV, not MIDI events: a held
// note near the top of the MIDI range must still step a full octave up.
void testArpOctaveStepsSurviveTheMidiCeiling()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.arpeggiator = ghostar::ArpeggiatorMode::Leap;
    parameters.lfoRate = 0.55f;
    engine.setParameters(parameters);
    engine.noteOn(120, 0.9f);   // ~8372 Hz; +1 octave exceeds MIDI 127
    render(engine, 0.4, 48000.0);

    double maximumWindowHz = 0.0;
    for (int window = 0; window < 24; ++window)
    {
        const auto slice = render(engine, 0.05, 48000.0);
        maximumWindowHz =
            std::max(maximumWindowHz, zeroCrossingHz(slice.left, 48000.0));
    }
    // The octave above note 120 is ~16.7 kHz; a MIDI-clamped step (note 127)
    // would top out near 12.5 kHz.
    check(maximumWindowHz > 14000.0,
          "the up-octave step above MIDI 127 keeps its full octave");
}

// With X auto-repeat as the only gate source, a key press changes pitch but
// must not articulate: the trigger chain derives from the selected bus, and
// the keyboard is not on it.
void testKeyPressDoesNotRetriggerWithoutKbdGate()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.gateKbd = false;
    parameters.gateX = true;
    parameters.lfoRate = 0.1f;        // ~0.5 Hz: a long high half-cycle
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessDecay = 0.25f;
    parameters.loudnessSustain = 0.25f;
    engine.setParameters(parameters);
    engine.noteOn(48, 0.9f);

    // The X edge articulates at t=0; by 0.5 s the envelope sits at sustain.
    render(engine, 0.5, 44100.0);
    const auto before = render(engine, 0.15, 44100.0);
    engine.noteOn(60, 0.9f);          // mid-high-cycle press: pitch only
    const auto after = render(engine, 0.15, 44100.0);
    check(rms(before) > 1.0e-4, "X auto-repeat articulates on its own clock");
    check(rms(after) < 1.3 * rms(before),
          "a key press with KBD deselected does not re-articulate");
}

// The arpeggiator's first sounding note is the bottom of the scan, from the
// very first clock edge — not the last-pressed key for a whole period.
void testArpFirstStepIsTheScanBottom()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.arpeggiator = ghostar::ArpeggiatorMode::Ripple;
    parameters.lfoRate = 0.0f;        // the slowest clock: one step for ~3 s
    engine.setParameters(parameters);
    engine.noteOn(48, 0.9f);
    engine.noteOn(55, 0.9f);
    engine.noteOn(64, 0.9f);          // last-pressed is the highest key
    render(engine, 0.1, 48000.0);
    const auto opening = render(engine, 0.4, 48000.0);
    const double hz = zeroCrossingHz(opening.left, 48000.0);
    check(std::abs(hz - 130.8) < 6.0,
          "the opening arpeggio step is the lowest held key");
}

// The factory bank is Init, the manual's eleven Sound Charts and the
// seventeen Ghostar Programs. Every one renders finite audio with keys held
// and leaves headroom; the Preparatory Pattern is the documented exception,
// since it "produces no sound".
// MIDI All Sound Off must kill the voice without resetting controllers: a
// bend held across it still applies to the next note, where the full
// reset() would have re-centred it.
// The travel smoother (plan Step 5): a hard parameter step while the voice
// sounds must glide over ~25 ms instead of stepping the audio — and a
// silent engine must snap, so configuring a patch before playing is exact.
void testTravelStepsGlideWhileSounding()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = EngineParameters {};
    parameters.filterPathA = 0.8f;
    engine.setParameters(parameters); // silent: snaps exactly
    engine.noteOn(48, 1.0f);
    render(engine, 0.5, 48000.0);

    // A brutal volume drop mid-note. Unsmoothed, the very next sample
    // scales by ~1/100; smoothed, the envelope glides down over ~25 ms.
    const auto before = render(engine, 0.05, 48000.0);
    parameters.masterVolume = 0.08f;
    engine.setParameters(parameters);
    const auto just = render(engine, 0.002, 48000.0);
    const auto later = render(engine, 0.3, 48000.0);

    const double levelBefore = rms(before);
    check(levelBefore > 1.0e-3, "the smoothing stroke sounds");
    // Within 2 ms of the step the level must still be near the old one
    // (the smoother has moved less than ~8 % of the way).
    check(rms(just) > levelBefore * 0.6,
          "a volume step must not land within a couple of milliseconds");
    // And it must genuinely arrive: far below the old level once settled.
    const std::vector<float> tail(later.left.end() - 4800,
                                  later.left.end());
    double tailSum = 0.0;
    for (const float value : tail)
        tailSum += static_cast<double>(value) * static_cast<double>(value);
    const double tailRms = std::sqrt(tailSum / 4800.0);
    check(tailRms < levelBefore * 0.05,
          "the stepped volume target is reached after settling");
}

void testStopAllSoundKeepsControllers()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    engine.setParameters(EngineParameters {});

    engine.noteOn(48, 1.0f);
    render(engine, 0.3, 48000.0);
    const double unbentHz =
        zeroCrossingHz(render(engine, 0.5, 48000.0).left, 48000.0);

    engine.setPitchBend(1.0f); // +8 semitones at full travel
    engine.stopAllSound();
    const auto stopped = render(engine, 0.3, 48000.0);
    check(peak(stopped) < 1.0e-4, "all sound off silences the voice");

    engine.noteOn(48, 1.0f);
    render(engine, 0.3, 48000.0);
    const double bentHz =
        zeroCrossingHz(render(engine, 0.5, 48000.0).left, 48000.0);
    check(bentHz > unbentHz * 1.35,
          "the held bend survives all sound off");
}

void testEveryFactoryProgramRenders()
{
    check(ghostar::factoryPresetCount() == 29,
          "the factory bank is Init plus eleven Sound Charts plus the "
          "seventeen Ghostar Programs");
    check(ghostar::factoryPresetName(0) != nullptr
              && std::string(ghostar::factoryPresetName(0)) == "Init",
          "the bank opens with the default voice");
    check(ghostar::factoryPresetName(1) != nullptr
              && std::string(ghostar::factoryPresetName(1))
                     == "Preparatory Pattern",
          "the charts start with the Preparatory Pattern");
    check(ghostar::factoryPresetName(ghostar::factoryPresetCount())
              == nullptr,
          "an out-of-range program has no name");

    // Every program carries a description, and the two banks are contiguous
    // with the historical one first, which is what a browser groups on.
    bool sawProgramsBank = false;
    for (int index = 0; index < ghostar::factoryPresetCount(); ++index)
    {
        const bool isProgram =
            ghostar::factoryPresetBank(index) == ghostar::PresetBank::Programs;
        if (isProgram)
            sawProgramsBank = true;
        check(!(sawProgramsBank && !isProgram),
              "the banks are contiguous, Sound Charts first");
        check(ghostar::factoryPresetDescription(index) != nullptr
                  && ghostar::factoryPresetDescription(index)[0] != '\0',
              "every program has a description");
    }
    check(sawProgramsBank, "the performance bank is present");

    for (int index = 0; index < ghostar::factoryPresetCount(); ++index)
    {
        GhostarEngine engine;
        engine.prepare(44100.0, 256);
        engine.setParameters(ghostar::factoryPresetParameters(index));
        // The wheels sit where a player's hand would rest, since several
        // programs put their motion behind one.
        engine.setModWheel(0.5f);
        engine.setShaperWheel(0.6f);
        engine.noteOn(48, 0.9f);
        engine.noteOn(55, 0.9f);
        const auto rendered = render(engine, 2.0, 44100.0);
        const std::string name = ghostar::factoryPresetName(index);
        check(finite(rendered), (name + " renders finite audio").c_str());
        if (name == "Preparatory Pattern")
        {
            check(peak(rendered) == 0.0,
                  "the Preparatory Pattern produces no sound");
            continue;
        }
        check(peak(rendered) > 1.0e-4, (name + " is audible").c_str());
        // A program that clips on an ordinary two-note phrase would be a
        // delivery defect, not a voicing choice.
        check(peak(rendered) < 1.0,
              (name + " leaves headroom on an ordinary phrase").c_str());
    }
}

void testFasterThanRealtime()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.filterPathB = 0.8f;
    parameters.shaperPathRing = 0.6f;
    parameters.shaperPathNoise = 0.4f;
    parameters.lowerMode = ghostar::LowerFilterMode::Overdrive;
    engine.setParameters(parameters);
    engine.noteOn(45, 1.0f);

    const auto start = std::chrono::steady_clock::now();
    render(engine, 5.0, 44100.0);
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    // Informational bound with a wide margin for loaded CI workers: a mono
    // voice must render far faster than realtime.
    check(elapsed < 4.0, "five seconds of audio render inside four seconds");
}
} // namespace

int main()
{
    testSilentBeforeFirstNote();
    testNoteProducesAudibleFiniteAudio();
    testNoGateSourceMeansNoArticulation();
    testReleaseDecaysToSilence();
    testHeldKeyFallbackRestoresPitch();
    testKeyMemorySpansTheWholeMidiDomain();
    testVelocityDoesNotScaleLoudness();
    testZeroVelocityNoteOnReleases();
    testHostileRatesAreClamped();
    testTopNoteAtLowestRateStaysBounded();
    testNonFiniteParametersAreSanitised();
    testNonFinitePerformanceControlsAreSanitised();
    testOutOfRangeSwitchesAreSanitised();
    testShaperResetRetriggersOnLegatoPress();
    testShaperResetSelfGateCompletesItsCycle();
    testFullResonanceStaysBounded();
    testRegenerativeExtremesStayBounded();
    testSelfOscillationLevelIsRateInvariant();
    testArpeggiatorStepsHeldKeys();
    testAttackReachesPeakAtItsLabelledTime();
    testArpOctaveStepsSurviveTheMidiCeiling();
    testKeyPressDoesNotRetriggerWithoutKbdGate();
    testArpFirstStepIsTheScanBottom();
    testTravelStepsGlideWhileSounding();
    testStopAllSoundKeepsControllers();
    testEveryFactoryProgramRenders();
    testFasterThanRealtime();

    if (failures != 0)
    {
        std::cerr << failures << " Ghostar engine check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghostar engine checks passed.\n";
    return EXIT_SUCCESS;
}

// Plan Step 1's instrument: render the engine's worst-case aliasing strokes
// at the shipping rate and against a ground truth, and report the alias
// residual per stroke. The strokes are nonstationary with dense legitimate
// sidebands (hard sync, audio-rate modulation, driven nonlinearities), so
// eyeballing bins cannot separate alias from sideband: the reference is a
// much-higher-rate render of the same stroke (16x, the engine's supported
// maximum), bandlimited by a -100 dB Kaiser lowpass and decimated to the
// shipping rate, and the figure is the short-time spectral-magnitude
// residual against it — magnitudes, not waveforms, for the same reason the
// zipper audit uses them: retuning strokes drift in accumulated phase while
// sounding the same. Strokes deliberately exclude the noise source: its
// per-internal-sample generator draws a different realisation at each rate,
// so a noise stroke would measure two different noises, not aliasing.
//
//   AliasAudit            full audit, prints the per-stroke table
//   AliasAudit --smoke    two strokes, short, 8x reference (CI guard)

#include "AliasMetric.h"
#include "DSP/GhostarEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
using ghostar::EngineParameters;
using ghostar::GhostarEngine;

constexpr double shippingRate = 48000.0;
// The plan's alias-to-signal acceptance gate. A row is only measured against
// it when the measurement can see that far down; see AliasMetric.h.
constexpr double gateDb = -60.0;
constexpr double pi = 3.14159265358979323846;

struct AliasCase
{
    const char* name;
    void (*patch)(EngineParameters&);
    // Optional per-sample travel gesture, a function of stroke time; the
    // sweep is identical wall-clock at every rate.
    void (*sweep)(EngineParameters&, double seconds);
    int note;
    float modWheel;
};

// One full up-and-down triangle across the stroke.
double sweepTriangle(double seconds, double total)
{
    const double phase = seconds / total;
    return phase < 0.5 ? 2.0 * phase : 2.0 - 2.0 * phase;
}

double strokeSeconds = 3.0;

// Renders a stroke at `hostRate`. Note events land at the same wall-clock
// instant at every rate (the reference factor is an integer), and sweeps are
// functions of time, so the two renders describe the same performance.
std::vector<float> renderStroke(const AliasCase& item, double hostRate)
{
    GhostarEngine engine;
    engine.prepare(hostRate, 512);

    EngineParameters parameters;
    item.patch(parameters);
    engine.setParameters(parameters);
    engine.setModWheel(item.modWheel);

    const long total = static_cast<long>(strokeSeconds * hostRate);
    const long noteAt = static_cast<long>(0.05 * hostRate);
    // A swept travel is written on a fixed 1 ms wall-clock grid, not on
    // each render's own sample grid: otherwise the reference would receive
    // a sixteen-times finer trajectory and the row would measure the
    // audit's own time quantisation alongside the engine's aliasing. The
    // engine's travel smoother turns either grid into the same glide.
    const long sweepPeriod = std::max(1L, static_cast<long>(hostRate / 1000.0));

    std::vector<float> output(static_cast<std::size_t>(total));
    std::array<float, 4096> left {};
    std::array<float, 4096> right {};

    long rendered = 0;
    while (rendered < total)
    {
        if (rendered == noteAt)
            engine.noteOn(item.note, 1.0f);

        long segmentEnd = rendered < noteAt ? noteAt : total;
        if (item.sweep != nullptr)
        {
            item.sweep(parameters,
                       static_cast<double>(rendered) / hostRate);
            engine.setParameters(parameters);
            segmentEnd = std::min(segmentEnd, rendered + sweepPeriod);
        }

        const long count = std::min(segmentEnd - rendered,
                                    static_cast<long>(left.size()));
        engine.process(left.data(), right.data(), static_cast<int>(count));
        std::memcpy(output.data() + rendered, left.data(),
                    static_cast<std::size_t>(count) * sizeof(float));
        rendered += count;
    }
    return output;
}

// Zeroth-order modified Bessel function, for the Kaiser window.
double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 64; ++k)
    {
        const double factor = x / (2.0 * k);
        term *= factor * factor;
        sum += term;
        if (term < 1.0e-14 * sum)
            break;
    }
    return sum;
}

// Bandlimit the high-rate render to the shipping audio band and decimate.
// Kaiser-windowed sinc: passband to 21.6 kHz, stopband from 24 kHz at
// better than -100 dB, so the reference's own fold-down is far below
// anything under measurement. Zero-phase alignment: the output sample n
// sits at the same wall-clock instant as shipping sample n.
std::vector<float> decimateReference(const std::vector<float>& highRate,
                                     int factor)
{
    const double highRateHz = shippingRate * factor;
    const int taps = 128 * factor + 1;
    const int center = (taps - 1) / 2;
    const double cutoffNorm = 22800.0 / highRateHz;
    const double beta = 10.06; // Kaiser for ~100 dB stopband
    const double denominator = besselI0(beta);

    std::vector<double> kernel(static_cast<std::size_t>(taps));
    double sum = 0.0;
    for (int j = 0; j < taps; ++j)
    {
        const double n = static_cast<double>(j - center);
        const double sinc =
            n == 0.0 ? 2.0 * cutoffNorm
                     : std::sin(2.0 * pi * cutoffNorm * n) / (pi * n);
        const double ratio = n / static_cast<double>(center);
        const double window =
            besselI0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio)))
            / denominator;
        kernel[static_cast<std::size_t>(j)] = sinc * window;
        sum += sinc * window;
    }
    for (auto& tap : kernel)
        tap /= sum;

    const long outCount =
        static_cast<long>(highRate.size()) / factor;
    std::vector<float> output(static_cast<std::size_t>(outCount));
    for (long n = 0; n < outCount; ++n)
    {
        double accumulator = 0.0;
        const long base = n * factor + center;
        for (int j = 0; j < taps; ++j)
        {
            const long index = base - j;
            if (index >= 0 && index < static_cast<long>(highRate.size()))
                accumulator +=
                    kernel[static_cast<std::size_t>(j)]
                    * static_cast<double>(
                        highRate[static_cast<std::size_t>(index)]);
        }
        output[static_cast<std::size_t>(n)] =
            static_cast<float>(accumulator);
    }
    return output;
}

// The metric lives in its own header so it can be exercised on signals whose
// alias content is known exactly; see AliasMetric.h for what it measures and
// for the way the first version of it got that wrong.
using ghostar::aliasmetric::Residual;
using ghostar::aliasmetric::spectralResidualDb;

bool finite(const std::vector<float>& samples)
{
    for (const float value : samples)
        if (!std::isfinite(value))
            return false;
    return true;
}

bool audible(const std::vector<float>& samples)
{
    const std::size_t skip = static_cast<std::size_t>(0.3 * shippingRate);
    double energy = 0.0;
    for (std::size_t at = skip; at < samples.size(); ++at)
        energy += static_cast<double>(samples[at])
                * static_cast<double>(samples[at]);
    return energy > 1.0e-6;
}

// ----------------------------------------------------------- stroke table
// Each stroke drives one aliasing mechanism the plan names, hard. The first
// row is a deliberate control: a mid-keyboard sawtooth is the easy case
// polyBLEP handles well, so its figure is the measurement floor the harder
// rows are read against — if the control row is poor, the audit itself is
// confounded and no other row can be trusted.

void patchMidSawControl(EngineParameters& p)
{
    p.oscAWaveform = ghostar::Waveform::Sawtooth;
    p.filterPathA = 0.8f;
    p.cutoff = 1.0f;
}

void patchWideSawDrone(EngineParameters& p)
{
    // WIDE at full travel parks Osc B at 10 kHz, keyboard-disconnected.
    p.oscBRange = ghostar::OscBRange::Wide;
    p.interval = 1.0f;
    p.oscBWaveform = ghostar::Waveform::Sawtooth;
    p.filterPathB = 0.8f;
    p.cutoff = 1.0f;
}

void patchWidePulseDrone(EngineParameters& p)
{
    // The 3 % pulse is the richest spectrum the panel offers.
    patchWideSawDrone(p);
    p.oscBWaveform = ghostar::Waveform::RectThin;
}

void patchWideTriangleDrone(EngineParameters& p)
{
    // The naive triangle's own stress case: its corners are unbandlimited.
    patchWideSawDrone(p);
    p.oscBWaveform = ghostar::Waveform::Triangle;
}

void patchSyncTop(EngineParameters& p)
{
    p.sync = true;
    p.oscBWaveform = ghostar::Waveform::Sawtooth;
    p.oscBRange = ghostar::OscBRange::Unison;
    p.filterPathB = 0.8f;
    p.cutoff = 1.0f;
}

void sweepSyncInterval(EngineParameters& p, double seconds)
{
    // The manual's own sync lesson: B tuned above A and swept.
    p.interval = static_cast<float>(
        0.5 + 0.5 * sweepTriangle(seconds, strokeSeconds));
}

void patchRingTop(EngineParameters& p)
{
    p.shaperPathRing = 0.85f;
    p.shaperMode = ghostar::ShaperMode::KbdHold;
    p.shaperRate = 0.95f;
    p.brightness = 1.0f;
    p.oscBRange = ghostar::OscBRange::PlusTwo;
}

// MOD SOURCE = OSC B with B in WIDE: audio-rate modulation of pitch, pulse
// width or cutoff — each its own aliasing mechanism, since the modulation
// is consumed at control rate.
void patchOscBModPitch(EngineParameters& p)
{
    p.modSource = ghostar::ModSource::OscB;
    p.oscBRange = ghostar::OscBRange::Wide;
    p.interval = 0.86f; // ~3 kHz modulator
    p.modXTo = ghostar::ModXDestination::OscA;
    p.oscAWaveform = ghostar::Waveform::Sawtooth;
    p.filterPathA = 0.8f;
    p.cutoff = 1.0f;
}

void patchOscBModPwm(EngineParameters& p)
{
    patchOscBModPitch(p);
    p.modXTo = ghostar::ModXDestination::OscARwm;
    p.oscAWaveform = ghostar::Waveform::RectWide;
}

void patchOscBModCutoff(EngineParameters& p)
{
    patchOscBModPitch(p);
    p.modXTo = ghostar::ModXDestination::FilterU;
    p.cutoff = 0.62f;
    p.resonance = 0.6f;
    p.upperResonance = ghostar::UpperResonanceMode::Variable;
}

void patchOverdriveFull(EngineParameters& p)
{
    // Full resonance drives the lower band-pass boost hard into the
    // inter-filter clipper.
    p.lowerMode = ghostar::LowerFilterMode::Overdrive;
    p.resonance = 1.0f;
    p.cutoff = 0.62f;
    p.oscAWaveform = ghostar::Waveform::Sawtooth;
    p.filterPathA = 0.8f;
}

void patchSelfOscTop(EngineParameters& p)
{
    // Regenerative self-oscillation near the top of the cutoff span: the
    // limiter's harmonics land beyond Nyquist and fold.
    p.resonance = 1.0f;
    p.upperResonance = ghostar::UpperResonanceMode::Variable;
    p.cutoff = 0.95f;
    p.oscAWaveform = ghostar::Waveform::Sawtooth;
    p.filterPathA = 0.15f;
}

// The sync mechanism at rest, separated from the swept row: a sweep's lock
// points move with any difference in the control trajectory, so the swept
// row measures the stroke's sensitivity as well as the model's aliasing.
void patchSyncStatic(EngineParameters& p)
{
    patchSyncTop(p);
    p.interval = 0.93f;
}

const std::array<AliasCase, 12>& aliasCases()
{
    static const std::array<AliasCase, 12> cases {{
        { "saw-midkey-control", patchMidSawControl, nullptr, 69, 0.0f },
        { "wide-saw-10k", patchWideSawDrone, nullptr, 60, 0.0f },
        { "wide-pulse3-10k", patchWidePulseDrone, nullptr, 60, 0.0f },
        { "wide-tri-10k", patchWideTriangleDrone, nullptr, 60, 0.0f },
        { "sync-static-topkey", patchSyncStatic, nullptr, 84, 0.0f },
        { "sync-sweep-topkey", patchSyncTop, sweepSyncInterval, 84, 0.0f },
        { "ring-topkey", patchRingTop, nullptr, 84, 0.0f },
        { "oscb-mod-pitch", patchOscBModPitch, nullptr, 60, 1.0f },
        { "oscb-mod-pwm", patchOscBModPwm, nullptr, 60, 1.0f },
        { "oscb-mod-cutoff", patchOscBModCutoff, nullptr, 60, 1.0f },
        { "overdrive-full", patchOverdriveFull, nullptr, 48, 0.0f },
        { "selfosc-highcutoff", patchSelfOscTop, nullptr, 72, 0.0f },
    }};
    return cases;
}
} // namespace

int main(int argc, char** argv)
{
    const bool smoke = argc > 1 && std::string(argv[1]) == "--smoke";
    // The reference factor is the engine's supported ceiling (16x = 768 kHz)
    // for the audited table; the smoke run proves the pipeline at 8x.
    const int factor = smoke ? 8 : 16;
    strokeSeconds = smoke ? 1.0 : 3.0;
    const std::size_t caseCount = smoke ? 2 : aliasCases().size();

    std::printf("Ghostar alias audit: shipping render vs a %dx ground truth"
                " decimated to %.0f Hz\n",
                factor, shippingRate);
    std::printf("%-22s %11s %11s %11s %11s\n", "stroke", "excess<=20k",
                "blind<=20k", "resid<=20k", "resid full");

    int failures = 0;
    for (std::size_t index = 0; index < caseCount; ++index)
    {
        const auto& item = aliasCases()[index];
        const auto shipping = renderStroke(item, shippingRate);
        const auto highRate =
            renderStroke(item, shippingRate * factor);
        if (!finite(shipping) || !finite(highRate))
        {
            std::printf("%-22s NON-FINITE OUTPUT\n", item.name);
            ++failures;
            continue;
        }
        const auto reference = decimateReference(highRate, factor);
        if (!audible(reference) || !audible(shipping))
        {
            std::printf("%-22s STROKE SILENT (audit invalid)\n", item.name);
            ++failures;
            continue;
        }
        const auto residual =
            spectralResidualDb(reference, shipping, shippingRate);
        // An excess figure is only worth as much as the measurement's own
        // floor: a row whose blind floor sits above the gate has not passed
        // it, it has failed to be decided by this method.
        const bool decided = residual.blindDb < gateDb;
        std::printf("%-22s %11.1f %11.1f %11.1f %11.1f  %s\n", item.name,
                    residual.excessDb, residual.blindDb, residual.audibleDb,
                    residual.fullDb, decided ? "" : "undecidable");
    }

    if (failures != 0)
    {
        std::fprintf(stderr, "%d alias-audit stroke(s) invalid.\n", failures);
        return 1;
    }
    return 0;
}

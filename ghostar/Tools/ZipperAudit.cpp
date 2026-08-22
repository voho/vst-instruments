// Plan Step 5's instrument: for every published continuous travel, render an
// exposing stroke three times through the shipping engine — parameters
// applied every sample (the reference: ideal sample-accurate automation),
// then latched every 512 and every 2048 samples as a host block does — and
// report the latching residual as energy relative to the reference render.
// Note events are sample-accurate in all three renders (the plug-in splits
// blocks at events); only the parameter application latches.
//
//   ZipperAudit            full audit, prints the per-travel table
//   ZipperAudit --smoke    two travels, short strokes (CI guard)

#include "DSP/GhostarEngine.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace
{
using ghostar::EngineParameters;
using ghostar::GhostarEngine;

constexpr double sampleRate = 48000.0;

enum class EventStyle
{
    HeldNote,     // one note held through the stroke
    GateCycles,   // note on/off every 250 ms (envelope segments, depths)
    NoteChanges,  // alternating pitches every 300 ms (glide, tune)
};

struct TravelCase
{
    const char* name;
    void (*apply)(EngineParameters&, float);  // nullptr for the wheels
    void (*patch)(EngineParameters&);         // base patch for audibility
    EventStyle events;
    bool wheelX;
    bool wheelY;
};

// The sweep: one full up-and-down triangle across the stroke, so every
// travel crosses its whole range in both directions.
float sweepValue(long sample, long total)
{
    const double phase = static_cast<double>(sample)
                       / static_cast<double>(total);
    const double tri = phase < 0.5 ? 2.0 * phase : 2.0 - 2.0 * phase;
    return static_cast<float>(tri);
}

struct NoteEvent
{
    long sample;
    int note;    // -1 = note off for the previous note
};

std::vector<NoteEvent> makeEvents(EventStyle style, long total)
{
    std::vector<NoteEvent> events;
    const long start = static_cast<long>(0.05 * sampleRate);
    switch (style)
    {
        case EventStyle::HeldNote:
            events.push_back({ start, 48 });
            break;
        case EventStyle::GateCycles:
        {
            const long period = static_cast<long>(0.25 * sampleRate);
            bool on = true;
            int note = 48;
            for (long at = start; at < total - period / 2; at += period)
            {
                events.push_back({ at, on ? note : -note });
                on = !on;
            }
            break;
        }
        case EventStyle::NoteChanges:
        {
            const long period = static_cast<long>(0.3 * sampleRate);
            int note = 48;
            for (long at = start; at < total - period / 2; at += period)
            {
                if (at != start)
                    events.push_back({ at, -(note == 48 ? 60 : 48) });
                events.push_back({ at, note });
                note = note == 48 ? 60 : 48;
            }
            break;
        }
    }
    return events;
}

// Renders the stroke with the travel latched every `blockSize` samples
// (blockSize 1 = the sample-accurate reference). `controlDelay` shifts the
// swept trajectory later by that many samples: a reference rendered with
// half the latched run's block as its delay is mean-aligned with the
// staircase, so the comparison isolates the latching *artifact* from the
// unavoidable half-block control delay that no causal smoothing can remove.
std::vector<float> renderStroke(const TravelCase& item, long total,
                                long blockSize, long controlDelay)
{
    GhostarEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    if (item.patch != nullptr)
        item.patch(parameters);

    // The base patch always reaches the engine; the latch grid then updates
    // only what the case sweeps (a wheel latches through its own setter).
    engine.setParameters(parameters);
    const auto applyAt = [&](long sample) {
        const float value =
            sweepValue(std::max(0L, sample - controlDelay), total);
        if (item.wheelX)
            engine.setModWheel(value);
        else if (item.wheelY)
            engine.setShaperWheel(value);
        if (item.apply != nullptr)
        {
            item.apply(parameters, value);
            engine.setParameters(parameters);
        }
    };

    // Fixed wheel positions where a wheel is the modulation lane under a
    // swept travel (and not itself the travel under test).
    if (!item.wheelX)
        engine.setModWheel(0.8f);
    if (!item.wheelY)
        engine.setShaperWheel(0.8f);

    const auto events = makeEvents(item.events, total);
    std::size_t nextEvent = 0;

    std::vector<float> output(static_cast<std::size_t>(total));
    std::array<float, 4096> left {};
    std::array<float, 4096> right {};

    long rendered = 0;
    applyAt(0);
    while (rendered < total)
    {
        // The next parameter latch point and the next note event both bound
        // this segment; events are sample-accurate at every block size.
        const long nextLatch = ((rendered / blockSize) + 1) * blockSize;
        long segmentEnd = std::min(nextLatch, total);
        if (nextEvent < events.size())
            segmentEnd = std::min(segmentEnd, events[nextEvent].sample);

        while (nextEvent < events.size()
               && events[nextEvent].sample == rendered)
        {
            const auto& event = events[nextEvent];
            if (event.note >= 0)
                engine.noteOn(event.note, 1.0f);
            else
                engine.noteOff(-event.note);
            ++nextEvent;
        }
        if (segmentEnd == rendered)
            continue;

        long remaining = segmentEnd - rendered;
        while (remaining > 0)
        {
            const long count =
                std::min(remaining, static_cast<long>(left.size()));
            engine.process(left.data(), right.data(),
                           static_cast<int>(count));
            std::memcpy(output.data() + rendered, left.data(),
                        static_cast<std::size_t>(count) * sizeof(float));
            rendered += count;
            remaining -= count;
        }
        if (rendered == nextLatch && rendered < total)
            applyAt(rendered);
    }
    return output;
}

// In-place radix-2 complex FFT (size a power of two).
void fft(std::vector<double>& real, std::vector<double>& imag)
{
    const std::size_t n = real.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * 3.14159265358979323846
                           / static_cast<double>(length);
        const double wRe = std::cos(angle);
        const double wIm = std::sin(angle);
        for (std::size_t start = 0; start < n; start += length)
        {
            double curRe = 1.0;
            double curIm = 0.0;
            for (std::size_t k = 0; k < length / 2; ++k)
            {
                const std::size_t even = start + k;
                const std::size_t odd = start + k + length / 2;
                const double tRe = real[odd] * curRe - imag[odd] * curIm;
                const double tIm = real[odd] * curIm + imag[odd] * curRe;
                real[odd] = real[even] - tRe;
                imag[odd] = imag[even] - tIm;
                real[even] += tRe;
                imag[even] += tIm;
                const double nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

// Latching residual as short-time spectral-magnitude energy against the
// reference, in dB. Magnitudes, not waveforms: travels that retune an
// oscillator change its phase *increment*, so reference and latched
// renders drift apart in accumulated phase while sounding the same — a
// waveform difference would report that as ~0 dB "error". The magnitude
// spectrum forgives accumulated phase but still catches what latching
// actually adds: step transients and block-rate sidebands. The onset is
// skipped so only steady-stroke latching is measured.
double spectralResidualDb(const std::vector<float>& reference,
                          const std::vector<float>& latched)
{
    constexpr std::size_t window = 2048;
    constexpr std::size_t hop = 1024;
    const std::size_t skip = static_cast<std::size_t>(0.3 * sampleRate);

    std::vector<double> hann(window);
    for (std::size_t i = 0; i < window; ++i)
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846
                                       * static_cast<double>(i)
                                       / static_cast<double>(window));

    std::vector<double> refRe(window), refIm(window);
    std::vector<double> latRe(window), latIm(window);
    double residual = 0.0;
    double signal = 0.0;
    for (std::size_t start = skip; start + window <= reference.size();
         start += hop)
    {
        for (std::size_t i = 0; i < window; ++i)
        {
            refRe[i] = hann[i] * static_cast<double>(reference[start + i]);
            latRe[i] = hann[i] * static_cast<double>(latched[start + i]);
            refIm[i] = 0.0;
            latIm[i] = 0.0;
        }
        fft(refRe, refIm);
        fft(latRe, latIm);
        for (std::size_t bin = 0; bin <= window / 2; ++bin)
        {
            const double refMag = std::sqrt(refRe[bin] * refRe[bin]
                                            + refIm[bin] * refIm[bin]);
            const double latMag = std::sqrt(latRe[bin] * latRe[bin]
                                            + latIm[bin] * latIm[bin]);
            const double difference = latMag - refMag;
            residual += difference * difference;
            signal += refMag * refMag;
        }
    }
    if (signal <= 0.0)
        return 0.0; // a silent reference means the stroke failed to expose
    return 10.0 * std::log10(residual / signal + 1.0e-20);
}

bool finite(const std::vector<float>& samples)
{
    for (const float value : samples)
        if (!std::isfinite(value))
            return false;
    return true;
}

// ----------------------------------------------------------- travel table
void patchFilterVoice(EngineParameters& p)
{
    p.filterPathA = 0.8f;
}

// The filter envelope is bipolar around 0.5, so its segment travels are
// inaudible at the default depth: the exposing patch opens the depth up.
void patchFilterEnvelope(EngineParameters& p)
{
    p.filterPathA = 0.8f;
    p.filterEnvAmount = 0.85f;
    p.cutoff = 0.4f;
}

void patchBothOscillators(EngineParameters& p)
{
    p.filterPathA = 0.7f;
    p.filterPathB = 0.6f;
}

void patchShaperVoice(EngineParameters& p)
{
    p.filterPathA = 0.0f;
    p.shaperPathA = 0.8f;
    p.shaperMode = ghostar::ShaperMode::Free;
    p.shaperRate = 0.6f;
}

void patchNoiseVoice(EngineParameters& p)
{
    p.filterPathA = 0.0f;
    p.filterPathNoise = 0.8f;
}

void patchLfoToFilter(EngineParameters& p)
{
    p.filterPathA = 0.8f;
    p.modSource = ghostar::ModSource::LfoTriangle;
    p.modXTo = ghostar::ModXDestination::FilterUL;
    p.lfoRate = 0.55f;
}

void patchShaperToOscB(EngineParameters& p)
{
    p.filterPathB = 0.8f;
    p.shaperMode = ghostar::ShaperMode::Free;
    p.shaperRate = 0.5f;
    p.shaperYTo = ghostar::ShaperYDestination::OscB;
}

void patchGlide(EngineParameters& p)
{
    p.filterPathA = 0.8f;
    p.glideMode = ghostar::GlideMode::On;
    p.glide = 0.4f;
}

const std::array<TravelCase, 30>& travelCases()
{
    static const std::array<TravelCase, 30> cases {{
        { "tune", [](EngineParameters& p, float v) { p.tune = v; },
          patchFilterVoice, EventStyle::HeldNote, false, false },
        { "interval", [](EngineParameters& p, float v) { p.interval = v; },
          patchBothOscillators, EventStyle::HeldNote, false, false },
        { "masterVolume",
          [](EngineParameters& p, float v) { p.masterVolume = v; },
          patchFilterVoice, EventStyle::HeldNote, false, false },
        { "brightness",
          [](EngineParameters& p, float v) { p.brightness = v; },
          patchShaperVoice, EventStyle::HeldNote, false, false },
        { "shaperPathA",
          [](EngineParameters& p, float v) { p.shaperPathA = v; },
          patchShaperVoice, EventStyle::HeldNote, false, false },
        { "shaperPathB",
          [](EngineParameters& p, float v) { p.shaperPathB = v; },
          patchShaperVoice, EventStyle::HeldNote, false, false },
        { "shaperPathRing",
          [](EngineParameters& p, float v) { p.shaperPathRing = v; },
          patchShaperVoice, EventStyle::HeldNote, false, false },
        { "shaperPathNoise",
          [](EngineParameters& p, float v) { p.shaperPathNoise = v; },
          patchShaperVoice, EventStyle::HeldNote, false, false },
        { "filterPathA",
          [](EngineParameters& p, float v) { p.filterPathA = v; },
          patchFilterVoice, EventStyle::HeldNote, false, false },
        { "filterPathB",
          [](EngineParameters& p, float v) { p.filterPathB = v; },
          patchBothOscillators, EventStyle::HeldNote, false, false },
        { "filterPathNoise",
          [](EngineParameters& p, float v) { p.filterPathNoise = v; },
          patchNoiseVoice, EventStyle::HeldNote, false, false },
        { "cutoff", [](EngineParameters& p, float v) { p.cutoff = v; },
          patchFilterVoice, EventStyle::HeldNote, false, false },
        { "lowerOnly", [](EngineParameters& p, float v) { p.lowerOnly = v; },
          [](EngineParameters& p) {
              p.filterPathA = 0.8f;
              p.lowerMode = ghostar::LowerFilterMode::HighPass;
          },
          EventStyle::HeldNote, false, false },
        { "resonance", [](EngineParameters& p, float v) { p.resonance = v; },
          [](EngineParameters& p) {
              p.filterPathA = 0.8f;
              p.upperResonance = ghostar::UpperResonanceMode::Variable;
              p.cutoff = 0.5f;
          },
          EventStyle::HeldNote, false, false },
        { "kbAmount", [](EngineParameters& p, float v) { p.kbAmount = v; },
          patchFilterVoice, EventStyle::HeldNote, false, false },
        { "filterEnvAmount",
          [](EngineParameters& p, float v) { p.filterEnvAmount = v; },
          patchFilterVoice, EventStyle::GateCycles, false, false },
        { "filterAttack",
          [](EngineParameters& p, float v) { p.filterAttack = v; },
          patchFilterEnvelope, EventStyle::GateCycles, false, false },
        { "filterDecay",
          [](EngineParameters& p, float v) { p.filterDecay = v; },
          patchFilterEnvelope, EventStyle::GateCycles, false, false },
        { "filterSustain",
          [](EngineParameters& p, float v) { p.filterSustain = v; },
          patchFilterEnvelope, EventStyle::GateCycles, false, false },
        { "filterRelease",
          [](EngineParameters& p, float v) { p.filterRelease = v; },
          patchFilterEnvelope, EventStyle::GateCycles, false, false },
        { "loudnessAttack",
          [](EngineParameters& p, float v) { p.loudnessAttack = v; },
          patchFilterVoice, EventStyle::GateCycles, false, false },
        { "loudnessDecay",
          [](EngineParameters& p, float v) { p.loudnessDecay = v; },
          patchFilterVoice, EventStyle::GateCycles, false, false },
        { "loudnessSustain",
          [](EngineParameters& p, float v) { p.loudnessSustain = v; },
          patchFilterVoice, EventStyle::GateCycles, false, false },
        { "loudnessRelease",
          [](EngineParameters& p, float v) { p.loudnessRelease = v; },
          patchFilterVoice, EventStyle::GateCycles, false, false },
        { "lfoRate", [](EngineParameters& p, float v) { p.lfoRate = v; },
          patchLfoToFilter, EventStyle::HeldNote, false, false },
        { "shaperShape",
          [](EngineParameters& p, float v) { p.shaperShape = v; },
          patchShaperToOscB, EventStyle::HeldNote, false, false },
        { "shaperRate",
          [](EngineParameters& p, float v) { p.shaperRate = v; },
          patchShaperToOscB, EventStyle::HeldNote, false, false },
        { "glide", [](EngineParameters& p, float v) { p.glide = v; },
          patchGlide, EventStyle::NoteChanges, false, false },
        { "xWheel", nullptr, patchLfoToFilter, EventStyle::HeldNote,
          true, false },
        { "yWheel", nullptr, patchShaperToOscB, EventStyle::HeldNote,
          false, true },
    }};
    return cases;
}
} // namespace

int main(int argc, char** argv)
{
    const bool smoke = argc > 1 && std::string(argv[1]) == "--smoke";
    const long total =
        static_cast<long>((smoke ? 1.0 : 3.0) * sampleRate);
    const std::size_t caseCount = smoke ? 2 : travelCases().size();

    std::printf("Ghostar zipper audit: latching residual vs a sample-accurate"
                " reference, %.0f Hz\n", sampleRate);
    std::printf("%-22s %12s %12s\n", "travel", "512 dB", "2048 dB");

    int failures = 0;
    for (std::size_t index = 0; index < caseCount; ++index)
    {
        const auto& item = travelCases()[index];
        const auto reference = renderStroke(item, total, 1, 0);
        const auto reference512 = renderStroke(item, total, 1, 256);
        const auto reference2048 = renderStroke(item, total, 1, 1024);
        const auto block512 = renderStroke(item, total, 512, 0);
        const auto block2048 = renderStroke(item, total, 2048, 0);
        if (!finite(reference) || !finite(reference512)
            || !finite(reference2048) || !finite(block512)
            || !finite(block2048))
        {
            std::printf("%-22s NON-FINITE OUTPUT\n", item.name);
            ++failures;
            continue;
        }
        const double db512 = spectralResidualDb(reference512, block512);
        const double db2048 = spectralResidualDb(reference2048, block2048);
        const bool exposed = [&] {
            const std::size_t skip =
                static_cast<std::size_t>(0.3 * sampleRate);
            double energy = 0.0;
            for (std::size_t at = skip; at < reference.size(); ++at)
                energy += static_cast<double>(reference[at])
                        * static_cast<double>(reference[at]);
            return energy > 1.0e-6;
        }();
        if (!exposed)
        {
            std::printf("%-22s STROKE SILENT (audit invalid)\n", item.name);
            ++failures;
            continue;
        }
        std::printf("%-22s %12.1f %12.1f\n", item.name, db512, db2048);
    }

    if (failures != 0)
    {
        std::fprintf(stderr, "%d zipper-audit stroke(s) invalid.\n",
                     failures);
        return 1;
    }
    return 0;
}

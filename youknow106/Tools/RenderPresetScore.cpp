// Renders one factory preset through the shipping engine on a fixed score,
// for panel-matched comparison against another instrument playing the same
// tone bytes and the same notes.
//
// The score is deliberately mixed: sustained single notes, a melody, two
// chords and a cutoff slider swept across a held chord, so one take exposes
// level, envelope timing, polyphony and the filter's own trajectory rather
// than only one of them.
//
// The per-preset VR1 volume trim is NOT applied. It is a plug-in-only control
// with no counterpart on another instrument, and including it would compare
// this project's loudness policy rather than its engine.

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using namespace youknow106;

constexpr double renderRate = 48000.0;

void writeFloatWav(const std::string& path, const std::vector<float>& left,
                   const std::vector<float>& right)
{
    const std::uint32_t frames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = frames * 2u * 4u;
    std::vector<std::uint8_t> f;
    auto tag = [&f](const char* t) { for (int i = 0; i < 4; ++i) f.push_back((std::uint8_t)t[i]); };
    auto le = [&f](std::uint32_t v, int n) {
        for (int i = 0; i < n; ++i) f.push_back((std::uint8_t)((v >> (8 * i)) & 0xff));
    };
    tag("RIFF"); le(36u + dataBytes, 4); tag("WAVE");
    tag("fmt "); le(16u, 4); le(3u, 2); le(2u, 2);
    le((std::uint32_t)renderRate, 4); le((std::uint32_t)renderRate * 8u, 4);
    le(8u, 2); le(32u, 2);
    tag("data"); le(dataBytes, 4);
    for (std::uint32_t i = 0; i < frames; ++i)
        for (int c = 0; c < 2; ++c) {
            const float v = c == 0 ? left[i] : right[i];
            std::uint32_t bits; std::memcpy(&bits, &v, 4); le(bits, 4);
        }
    std::ofstream out(path, std::ios::binary);
    out.write((const char*)f.data(), (std::streamsize)f.size());
}

struct Note { int note; double on; double off; };
struct Move { double t; float cutoff; };

EngineParameters parametersFor(const sysex::Patch& patch)
{
    EngineParameters p {};
    p.lfoRate = patch.lfoRate; p.lfoDelay = patch.lfoDelay;
    p.dcoLfoDepth = patch.dcoLfo; p.pwmDepth = patch.pwm;
    p.pwmSource = patch.pwmSource; p.range = patch.range;
    p.sawEnabled = patch.saw; p.pulseEnabled = patch.pulse;
    p.subLevel = patch.sub; p.noiseLevel = patch.noise;
    p.highPass = patch.highPass; p.cutoff = patch.cutoff;
    p.resonance = patch.resonance; p.envPolarity = patch.envPolarity;
    p.envDepth = patch.vcfEnv; p.vcfLfoDepth = patch.vcfLfo;
    p.keyFollow = patch.keyFollow; p.vcaMode = patch.vcaMode;
    p.vcaLevel = patch.vcaLevel; p.attack = patch.attack;
    p.decay = patch.decay; p.sustain = patch.sustain;
    p.release = patch.release; p.chorus = patch.chorus;
    p.volume = 1.0f; p.polyphony = 6; p.calibration = 1.0f;
    return p;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <presetNumber e.g. A11> <out.wav>\n", argv[0]);
        return 2;
    }
    const auto* preset = presets::findByNumber(argv[1]);
    if (preset == nullptr) {
        std::fprintf(stderr, "unknown preset %s\n", argv[1]);
        return 2;
    }

    // The shared score. Times in seconds.
    std::vector<Note> notes;
    notes.push_back({ 48, 0.5, 2.5 });                 // a sustained low note
    notes.push_back({ 60, 3.0, 5.0 });                 // a sustained middle note
    const int melody[] = { 60, 63, 67, 70, 72, 70, 67, 63 };
    for (int i = 0; i < 8; ++i)                        // a melody, 0.45 s apart
        notes.push_back({ melody[i], 5.5 + 0.45 * i, 5.5 + 0.45 * i + 0.4 });
    for (int n : { 48, 55, 60, 64 })                   // a four-note chord
        notes.push_back({ n, 9.5, 12.5 });
    for (int n : { 45, 52, 57, 61 })                   // a second voicing
        notes.push_back({ n, 13.0, 16.0 });
    for (int n : { 36, 48, 55 })                       // held under the sweep
        notes.push_back({ n, 16.5, 23.5 });
    const double endSeconds = 26.0;

    // The cutoff slider swept up and back across the held chord, 100 steps,
    // which is what a player's hand on the FREQ slider actually produces.
    std::vector<Move> moves;
    for (int i = 0; i <= 100; ++i) {
        const double t = 17.0 + 6.0 * i / 100.0;
        const double phase = i / 100.0;
        const float value = (float)(phase < 0.5 ? preset->patch.cutoff
                                        + (1.0 - preset->patch.cutoff) * (phase * 2.0)
                                      : 1.0 - (1.0 - preset->patch.cutoff)
                                        * ((phase - 0.5) * 2.0));
        moves.push_back({ t, value });
    }

    YouKnow106Engine engine;
    engine.selectConverterTimingProfile(
        YouKnow106Engine::ConverterTimingProfile::MeasuredChartGeometry);
    engine.prepare(renderRate, 256, 4);
    auto parameters = parametersFor(preset->patch);
    engine.setParameters(parameters);

    struct Event { double t; int kind; int note; float cutoff; };
    std::vector<Event> events;
    for (const auto& n : notes) {
        events.push_back({ n.on, 0, n.note, 0.0f });
        events.push_back({ n.off, 1, n.note, 0.0f });
    }
    for (const auto& m : moves) events.push_back({ m.t, 2, 0, m.cutoff });
    std::stable_sort(events.begin(), events.end(),
                     [](const Event& a, const Event& b) { return a.t < b.t; });

    std::vector<float> left, right;
    std::array<float, 256> bl {}, br {};
    std::size_t next = 0;
    double rendered = 0.0;
    while (rendered < endSeconds) {
        while (next < events.size() && events[next].t <= rendered) {
            const auto& e = events[next++];
            if (e.kind == 0) engine.noteOn(e.note, 1.0f);
            else if (e.kind == 1) engine.noteOff(e.note);
            else { parameters.cutoff = e.cutoff; engine.setParameters(parameters); }
        }
        const double boundary = next < events.size()
            ? std::min(events[next].t, endSeconds) : endSeconds;
        int remaining = (int)std::llround(std::max(1.0 / renderRate,
                                                   boundary - rendered) * renderRate);
        while (remaining > 0) {
            const int count = std::min(256, remaining);
            engine.process(bl.data(), br.data(), count);
            left.insert(left.end(), bl.begin(), bl.begin() + count);
            right.insert(right.end(), br.begin(), br.begin() + count);
            remaining -= count;
        }
        rendered += std::max(1.0 / renderRate, boundary - rendered);
    }

    writeFloatWav(argv[2], left, right);
    double peak = 0.0;
    for (float v : left) peak = std::max(peak, (double)std::fabs(v));
    std::printf("%-4s %-28s %6.2f s  peak %7.2f dBFS\n", preset->number,
                preset->name, left.size() / renderRate,
                peak > 0 ? 20 * std::log10(peak) : -144.0);
    return 0;
}

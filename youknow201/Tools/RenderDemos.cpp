// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the plug-in runs, so a demonstration cannot drift away
// from what YouKnow201 actually sounds like. No samples or external
// processing are involved anywhere: every file comes out of the engine.

#include "DSP/YouKnow201Engine.h"
#include "DSP/YouKnow201Presets.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using youknow201::Engine;
using youknow201::Patch;

// 44.1 kHz is both the universally playable rate and the modelled hardware's
// presumed engine rate (OQ-01 in the research contract).
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian (std::vector<std::uint8_t>& bytes, std::uint32_t value,
                         int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back (static_cast<std::uint8_t> ((value >> (8 * index)) & 0xffu));
}

bool writeWav (const std::filesystem::path& path, const std::vector<float>& left,
               const std::vector<float>& right)
{
    const auto frames = static_cast<std::uint32_t> (left.size());
    constexpr std::uint16_t channels = 2u;
    const std::uint32_t byteRate =
        static_cast<std::uint32_t> (demoSampleRate) * channels * 2u;
    const std::uint32_t dataBytes = frames * channels * 2u;

    std::vector<std::uint8_t> bytes;
    bytes.reserve (44u + dataBytes);
    const auto tag = [&bytes] (const char* text)
    {
        for (int index = 0; index < 4; ++index)
            bytes.push_back (static_cast<std::uint8_t> (text[index]));
    };
    tag ("RIFF");
    appendLittleEndian (bytes, 36u + dataBytes, 4);
    tag ("WAVE");
    tag ("fmt ");
    appendLittleEndian (bytes, 16u, 4);
    appendLittleEndian (bytes, 1u, 2); // PCM
    appendLittleEndian (bytes, channels, 2);
    appendLittleEndian (bytes, static_cast<std::uint32_t> (demoSampleRate), 4);
    appendLittleEndian (bytes, byteRate, 4);
    appendLittleEndian (bytes, channels * 2u, 2); // block align
    appendLittleEndian (bytes, 16u, 2);           // bits per sample
    tag ("data");
    appendLittleEndian (bytes, dataBytes, 4);

    const auto encode = [] (float value)
    {
        if (! std::isfinite (value))
            value = 0.0f;
        const float clamped = std::clamp (value, -1.0f, 1.0f);
        const auto sample = static_cast<std::int32_t> (
            std::lround (static_cast<double> (clamped) * 32767.0));
        return static_cast<std::uint32_t> (
            static_cast<std::uint16_t> (static_cast<std::int16_t> (sample)));
    };

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        appendLittleEndian (bytes, encode (left[frame]), 2);
        appendLittleEndian (bytes, encode (right[frame]), 2);
    }

    std::FILE* file = std::fopen (path.string().c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool written =
        std::fwrite (bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose (file);
    return written;
}

// ---------------------------------------------------------------------------
// A take: an imperative score driving the engine
// ---------------------------------------------------------------------------

class Take
{
public:
    explicit Take (const Patch& patch, int masterLevel = 96)
        : engine_ (std::make_unique<Engine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize);
        engine_->setMasterLevel (masterLevel);
        engine_->setPatch (patch);
        engine_->reset();
    }

    Engine& engine() noexcept { return *engine_; }

    void setPatch (const Patch& patch) { engine_->setPatch (patch); }

    // Play a note (or chord) for `holdSeconds`, then rest `gapSeconds`.
    void note (int midiNote, int velocity, double holdSeconds, double gapSeconds)
    {
        chord ({ midiNote }, velocity, holdSeconds, gapSeconds);
    }

    void chord (const std::vector<int>& notes, int velocity, double holdSeconds,
                double gapSeconds)
    {
        for (int midiNote : notes)
            engine_->noteOn (midiNote, velocity);
        render (holdSeconds);
        for (int midiNote : notes)
            engine_->noteOff (midiNote);
        render (gapSeconds);
    }

    void noteOn (int midiNote, int velocity) { engine_->noteOn (midiNote, velocity); }
    void noteOff (int midiNote) { engine_->noteOff (midiNote); }
    void rest (double seconds) { render (seconds); }

    // Hold the currently sounding notes while a patch field sweeps linearly.
    template <typename Setter>
    void sweep (double seconds, Setter&& setter)
    {
        const auto steps = std::max (1, static_cast<int> (seconds * 100.0));
        Patch patch = engine_->currentPatch();
        for (int step = 0; step < steps; ++step)
        {
            setter (patch, static_cast<double> (step) / (steps - 1 == 0 ? 1 : steps - 1));
            engine_->setPatch (patch);
            render (seconds / steps);
        }
    }

    [[nodiscard]] const std::vector<float>& left() const noexcept { return left_; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return right_; }

    [[nodiscard]] bool finite() const noexcept
    {
        for (std::size_t index = 0; index < left_.size(); ++index)
            if (! std::isfinite (left_[index]) || ! std::isfinite (right_[index]))
                return false;
        return true;
    }

    [[nodiscard]] double peak() const noexcept
    {
        double result = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
            result = std::max ({ result, std::abs (static_cast<double> (left_[index])),
                                 std::abs (static_cast<double> (right_[index])) });
        return result;
    }

    double normalise()
    {
        const auto current = peak();
        if (current <= 1.0e-9)
            return 1.0;
        const auto gain = normalisedPeak / current;
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            left_[index] = static_cast<float> (left_[index] * gain);
            right_[index] = static_cast<float> (right_[index] * gain);
        }
        return gain;
    }

private:
    void render (double seconds)
    {
        auto remaining = static_cast<int> (std::lround (seconds * demoSampleRate));
        std::array<float, renderBlockSize> blockLeft {};
        std::array<float, renderBlockSize> blockRight {};

        while (remaining > 0)
        {
            const int count = std::min (renderBlockSize, remaining);
            engine_->process (blockLeft.data(), blockRight.data(), count);
            left_.insert (left_.end(), blockLeft.begin(), blockLeft.begin() + count);
            right_.insert (right_.end(), blockRight.begin(),
                           blockRight.begin() + count);
            remaining -= count;
        }
    }

    std::unique_ptr<Engine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

const Patch& bankPatch (const char* name)
{
    for (const auto& entry : youknow201::factoryPatches())
        if (std::strcmp (entry.name, name) == 0)
            return entry.patch;
    static const Patch fallback = youknow201::initPatch();
    return fallback;
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// The signature sound first: a supersaw trance line on both oscillators.
Take renderSuperSawLead()
{
    Take take (bankPatch ("SuperLead201"));
    take.rest (0.05);
    const std::vector<std::pair<int, double>> line {
        { 57, 0.22 }, { 60, 0.22 }, { 64, 0.22 }, { 69, 0.45 },
        { 67, 0.22 }, { 64, 0.22 }, { 69, 0.22 }, { 72, 0.45 },
        { 71, 0.22 }, { 67, 0.22 }, { 64, 0.22 }, { 60, 0.45 },
    };
    for (const auto& [note, seconds] : line)
        take.note (note, 112, seconds, 0.03);
    take.chord ({ 57, 64, 69, 76 }, 118, 2.6, 0.0);
    take.rest (2.2);
    return take;
}

// One chord held while the PW/FEEDBACK knob sweeps the seven-saw spread
// through Szabo's detune curve: flat unison into the full cluster.
Take renderSuperSawSpread()
{
    Patch patch = bankPatch ("SuperLead201");
    patch.delayOn = false;
    patch.reverbOn = false;
    patch.upper.osc2.wave = youknow201::Waveform::SuperSaw;
    patch.upper.balance = 0;
    Take take (patch);
    take.rest (0.05);
    take.noteOn (57, 112);
    take.noteOn (64, 112);
    take.noteOn (69, 112);
    take.rest (0.6);
    take.sweep (5.2, [] (Patch& p, double t)
    {
        const int spread = static_cast<int> (std::lround (t * 127.0));
        p.upper.osc1.pulseWidth = spread;
        p.upper.osc2.pulseWidth = spread;
    });
    take.rest (0.8);
    take.noteOff (57);
    take.noteOff (64);
    take.noteOff (69);
    take.rest (1.2);
    return take;
}

// FB OSC: the feedback comb pushed from clean saw into the aggressive zone,
// then a solo/legato phrase with portamento.
Take renderFbOscLead()
{
    Patch patch = bankPatch ("FB Howl Lead");
    Take take (patch);
    take.rest (0.05);
    take.noteOn (52, 110);
    take.rest (0.4);
    take.sweep (3.2, [] (Patch& p, double t)
    {
        p.upper.osc1.pulseWidth = static_cast<int> (std::lround (t * 127.0));
    });
    take.noteOff (52);
    take.rest (0.4);
    for (const auto& [note, seconds] :
         std::vector<std::pair<int, double>> {
             { 52, 0.35 }, { 55, 0.35 }, { 59, 0.35 }, { 64, 0.8 },
             { 62, 0.35 }, { 59, 0.5 }, { 52, 1.2 } })
        take.note (note, 116, seconds, 0.02);
    take.rest (1.6);
    return take;
}

// The 24 dB low-pass under a 16th-note line: cutoff climbing, resonance high,
// closing into audible self-oscillation territory near the top.
Take renderAcidFilter()
{
    Patch patch = bankPatch ("Acid 201");
    patch.delayOn = false;
    Take take (patch);
    take.rest (0.05);
    const std::array<int, 16> line { 36, 36, 48, 36, 39, 36, 46, 36,
                                     36, 43, 36, 48, 36, 39, 46, 48 };
    for (int pass = 0; pass < 4; ++pass)
    {
        Patch swept = patch;
        swept.upper.cutoff = 12 + pass * 22;
        swept.upper.resonance = 96 + pass * 10;
        take.setPatch (swept);
        for (std::size_t step = 0; step < line.size(); ++step)
        {
            const int velocity = step % 4 == 0 ? 122 : 92;
            take.note (line[step], velocity, 0.085, 0.04);
        }
    }
    take.rest (1.2);
    return take;
}

// Oscillator sync with the pitch envelope sweeping OSC1 across OSC2.
Take renderSyncSweeper()
{
    Take take (bankPatch ("Sync Sweeper"));
    take.rest (0.05);
    for (const auto& [note, seconds] :
         std::vector<std::pair<int, double>> {
             { 45, 0.8 }, { 48, 0.8 }, { 50, 0.8 }, { 45, 1.4 } })
        take.note (note, 118, seconds, 0.12);
    // A slower manual sweep: hold one note and ride OSC1's pitch by hand.
    take.noteOn (43, 120);
    take.rest (0.3);
    take.sweep (2.8, [] (Patch& p, double t)
    {
        p.upper.osc1.pitchWide = true;
        p.upper.osc1.coarse = static_cast<int> (std::lround (t * 19.0));
    });
    take.noteOff (43);
    take.rest (1.4);
    return take;
}

// Ring modulation: equal-sine product bell arpeggio.
Take renderRingBell()
{
    Take take (bankPatch ("Ring Bell"));
    take.rest (0.05);
    const std::array<int, 8> arp { 60, 67, 72, 76, 79, 76, 72, 67 };
    for (int pass = 0; pass < 2; ++pass)
        for (int note : arp)
            take.note (note + pass * 5, 104, 0.16, 0.10);
    take.chord ({ 55, 62, 69 }, 112, 2.0, 0.0);
    take.rest (3.0);
    return take;
}

// PWM strings: pulse width under LFO on both oscillators, chorus template.
Take renderPwmStrings()
{
    Take take (bankPatch ("PWM Strings"));
    take.rest (0.05);
    take.chord ({ 48, 55, 60, 64 }, 96, 3.2, 0.4);
    take.chord ({ 46, 53, 58, 62 }, 96, 3.2, 0.4);
    take.chord ({ 44, 51, 56, 60 }, 100, 2.2, 0.1);
    take.chord ({ 43, 50, 55, 59, 62 }, 104, 3.6, 0.0);
    take.rest (2.6);
    return take;
}

// The low end: square + sine an octave down with the LOW FREQ boost.
Take renderSubBass()
{
    Take take (bankPatch ("Sub Bass 201"));
    take.rest (0.05);
    const std::vector<std::pair<int, double>> line {
        { 36, 0.32 }, { 36, 0.16 }, { 43, 0.32 }, { 36, 0.16 },
        { 41, 0.32 }, { 39, 0.16 }, { 36, 0.62 }, { 31, 0.62 },
        { 36, 0.32 }, { 36, 0.16 }, { 43, 0.32 }, { 46, 0.16 },
        { 43, 0.32 }, { 41, 0.16 }, { 39, 0.62 }, { 36, 1.2 },
    };
    for (const auto& [note, seconds] : line)
        take.note (note, note == 36 ? 118 : 100, seconds, 0.05);
    take.rest (1.2);
    return take;
}

// Sample & hold into the band-pass filter: the classic S&H effects patch.
Take renderSampleHoldFx()
{
    Take take (bankPatch ("S&H Robot"));
    take.rest (0.05);
    take.noteOn (48, 110);
    take.rest (4.2);
    take.noteOff (48);
    take.rest (0.3);
    take.noteOn (36, 118);
    take.rest (3.2);
    take.noteOff (36);
    take.rest (1.4);
    return take;
}

// DUAL keyboard mode: two complete tones layered under one hall.
Take renderDualPad()
{
    Take take (bankPatch ("Alaska Dual"));
    take.rest (0.05);
    take.chord ({ 48, 55, 62, 67 }, 92, 5.2, 1.2 );
    take.chord ({ 46, 53, 60, 65 }, 92, 5.2, 1.2 );
    take.chord ({ 44, 51, 58, 63, 67 }, 96, 6.0, 0.0 );
    take.rest (5.0);
    return take;
}

// ---------------------------------------------------------------------------
// The demo table
// ---------------------------------------------------------------------------

struct Demo
{
    const char* fileName;
    const char* description;
    Take (*render)();
};

const std::array<Demo, 10>& demos()
{
    static const std::array<Demo, 10> table {{
        { "01-supersaw-lead.wav",
          "Both oscillators SUPER SAW: a trance line into a held stack",
          renderSuperSawLead },
        { "02-supersaw-spread-sweep.wav",
          "One chord while the spread knob sweeps the seven-saw detune curve",
          renderSuperSawSpread },
        { "03-fb-osc-lead.wav",
          "FB OSC from clean saw into feedback, then a legato solo phrase",
          renderFbOscLead },
        { "04-acid-filter-24db.wav",
          "The -24 dB low-pass at high resonance under a 16th-note line",
          renderAcidFilter },
        { "05-sync-sweeper.wav",
          "Oscillator sync swept by the pitch envelope and by hand",
          renderSyncSweeper },
        { "06-ring-bell.wav",
          "Ring modulation: equal-sine product bells", renderRingBell },
        { "07-pwm-strings.wav",
          "Pulse-width modulation strings through the chorus delay template",
          renderPwmStrings },
        { "08-sub-bass.wav",
          "Square plus sine an octave down with the LOW FREQ boost",
          renderSubBass },
        { "09-sample-hold-fx.wav",
          "Sample & hold LFO into the band-pass filter", renderSampleHoldFx },
        { "10-dual-pad.wav",
          "DUAL keyboard mode: two complete tones layered under one hall",
          renderDualPad },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    Take take (bankPatch ("SuperLead201"));
    take.note (57, 110, 0.2, 0.1);
    take.note (64, 110, 0.2, 0.3);

    if (! take.finite())
    {
        std::fprintf (stderr, "smoke test: rendered a non-finite sample\n");
        return 1;
    }
    if (take.peak() < 1.0e-3)
    {
        std::fprintf (stderr, "smoke test: rendered silence (peak %.6f)\n",
                      take.peak());
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories (directory, error);
    const auto path = directory / "smoke.wav";
    if (! writeWav (path, take.left(), take.right()))
    {
        std::fprintf (stderr, "smoke test: could not write %s\n",
                      path.string().c_str());
        return 1;
    }
    const auto size = std::filesystem::file_size (path, error);
    std::filesystem::remove (path, error);
    if (size < 44u + take.left().size() * 4u)
    {
        std::fprintf (stderr, "smoke test: short WAV (%llu bytes)\n",
                      static_cast<unsigned long long> (size));
        return 1;
    }

    std::printf ("YouKnow201 demo renderer smoke test passed (peak %.3f).\n",
                 take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by YouKnow201RenderDemos;"
    " edits between the markers are overwritten -->";
constexpr const char* peaksTableEnd = "<!-- peaks-table-end -->";

bool ownsDirectory (const std::filesystem::path& directory)
{
    const auto readmePath = directory / "README.md";
    if (! std::filesystem::exists (readmePath))
        return false;

    std::ifstream input (readmePath, std::ios::binary);
    const std::string readme ((std::istreambuf_iterator<char> (input)),
                              std::istreambuf_iterator<char>());
    return readme.find (peaksTableBegin) != std::string::npos;
}

bool removeStaleWavs (const std::filesystem::path& directory)
{
    if (! ownsDirectory (directory))
    {
        std::printf ("%s is not this renderer's own output directory, so nothing "
                     "in it will be removed.\n", directory.string().c_str());
        return true;
    }

    const auto& current = demos();
    bool removedAll = true;

    for (const auto& entry : std::filesystem::directory_iterator (directory))
    {
        if (! entry.is_regular_file() || entry.path().extension() != ".wav")
            continue;

        const auto stem = entry.path().filename().string();
        if (stem.size() < 3 || ! std::isdigit (static_cast<unsigned char> (stem[0]))
            || ! std::isdigit (static_cast<unsigned char> (stem[1]))
            || stem[2] != '-')
            continue;

        const auto name = entry.path().filename().string();
        const bool known =
            std::any_of (current.begin(), current.end(),
                         [&name] (const Demo& demo)
                         { return name == demo.fileName; });
        if (known)
            continue;

        std::error_code error;
        if (std::filesystem::remove (entry.path(), error))
            std::printf ("Removed stale demo %s\n", name.c_str());
        else
        {
            std::fprintf (stderr, "could not remove stale demo %s\n", name.c_str());
            removedAll = false;
        }
    }
    return removedAll;
}

struct RenderedLevel
{
    std::string fileName;
    std::string description;
    double seconds = 0.0;
    double renderedPeakDb = 0.0;
    double normalisationDb = 0.0;
};

std::string formatSignedDb (double value)
{
    char digits[32];
    std::snprintf (digits, sizeof digits, "%.1f", std::fabs (value));
    const bool negative = value < 0.0 && std::strcmp (digits, "0.0") != 0;
    return std::string (negative ? "\xE2\x88\x92" : "+") + digits;
}

bool updatePeaksTable (const std::filesystem::path& directory,
                       const std::vector<RenderedLevel>& levels)
{
    const auto readmePath = directory / "README.md";
    if (! std::filesystem::exists (readmePath))
        return true; // An ad-hoc output directory carries no documentation.

    std::ifstream input (readmePath, std::ios::binary);
    std::string readme ((std::istreambuf_iterator<char> (input)),
                        std::istreambuf_iterator<char>());
    input.close();

    const auto beginPos = readme.find (peaksTableBegin);
    const auto endPos = readme.find (peaksTableEnd);
    if (beginPos == std::string::npos || endPos == std::string::npos
        || endPos < beginPos)
    {
        std::fprintf (stderr,
                      "%s has no peaks-table markers, so its level table can no "
                      "longer be kept in sync with the rendered files.\n",
                      readmePath.string().c_str());
        return false;
    }

    std::string table =
        "\n| File | What it is | Length | Rendered peak | Normalisation |\n"
        "| --- | --- | ---: | ---: | ---: |\n";
    for (const auto& level : levels)
    {
        char length[32];
        std::snprintf (length, sizeof length, "%.1f s", level.seconds);
        table += "| `" + level.fileName + "` | " + level.description + " | "
               + length + " | " + formatSignedDb (level.renderedPeakDb) + " dBFS | "
               + formatSignedDb (level.normalisationDb) + " dB |\n";
    }

    const auto contentStart = beginPos + std::strlen (peaksTableBegin);
    const auto updated =
        readme.substr (0, contentStart) + table + readme.substr (endPos);
    if (updated == readme)
        return true;

    std::ofstream output (readmePath, std::ios::binary | std::ios::trunc);
    output << updated;
    output.close();
    std::printf ("Updated the rendered-peak table in %s\n",
                 readmePath.string().c_str());
    return ! output.fail();
}
} // namespace

int main (int argc, char** argv)
{
    std::vector<std::string> arguments (argv + 1, argv + argc);
    bool smoke = false;
    std::filesystem::path directory = "Docs/audio";

    for (const auto& argument : arguments)
    {
        if (argument == "--smoke")
        {
            smoke = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::printf ("usage: YouKnow201RenderDemos [--smoke] [output-directory]\n");
            return 0;
        }
        else
        {
            directory = argument;
        }
    }

    if (smoke)
        return runSmokeTest (directory);

    std::error_code error;
    std::filesystem::create_directories (directory, error);
    if (! std::filesystem::is_directory (directory))
    {
        std::fprintf (stderr, "not a directory: %s\n", directory.string().c_str());
        return 1;
    }

    if (! removeStaleWavs (directory))
        return 1;

    std::vector<RenderedLevel> levels;

    for (const auto& demo : demos())
    {
        auto take = demo.render();

        if (! take.finite())
        {
            std::fprintf (stderr, "%s rendered a non-finite sample\n", demo.fileName);
            return 1;
        }

        const auto renderedPeak = take.peak();
        if (renderedPeak < 1.0e-4)
        {
            std::fprintf (stderr, "%s rendered silence (peak %.6f)\n", demo.fileName,
                          renderedPeak);
            return 1;
        }
        // The output stage saturates above 0.9; a take that entered that
        // region would document the limiter rather than the engine.
        if (renderedPeak >= 0.9)
        {
            std::fprintf (stderr,
                          "%s reached %.6f, inside the output stage's saturation "
                          "region; render it at a lower level\n",
                          demo.fileName, renderedPeak);
            return 1;
        }

        const auto gain = take.normalise();
        const auto path = directory / demo.fileName;
        if (! writeWav (path, take.left(), take.right()))
        {
            std::fprintf (stderr, "could not write %s\n", path.string().c_str());
            return 1;
        }

        levels.push_back ({ demo.fileName, demo.description,
                            static_cast<double> (take.left().size()) / demoSampleRate,
                            20.0 * std::log10 (renderedPeak),
                            20.0 * std::log10 (gain) });

        std::printf ("Rendered %-28s %5.1f s  peak %6.1f dBFS\n", demo.fileName,
                     levels.back().seconds, levels.back().renderedPeakDb);
    }

    if (! updatePeaksTable (directory, levels))
        return 1;

    std::printf ("Wrote %zu demonstration files to %s\n", levels.size(),
                 directory.string().c_str());
    return 0;
}

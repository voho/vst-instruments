// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the plug-in runs, so a demonstration cannot drift away from
// what Drumalor actually sounds like. No samples or external processing are
// involved anywhere: every hit here is synthesized by the engine.

#include "DSP/DrumEngine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace
{
using drumalor::DrumEngine;
using drumalor::Instrument;
using drumalor::KitParameters;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;

// Well below the plug-in's own default. The engine's master output stage is a
// gently saturating shaper, and a busy thirteen-voice groove rendered at the
// usual level would be leaning on that stage rather than showing the voices.
// The kit bus - Drive and Compression - sits before the master gain, so the
// bus demo is unaffected by rendering quietly, and every take is normalised to
// a common level afterwards, so it costs nothing.
constexpr float demoOutputGain = 0.30f;

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian (std::vector<std::uint8_t>& bytes, std::uint32_t value,
                         int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back (static_cast<std::uint8_t> ((value >> (8 * index)) & 0xffu));
}

// Drumalor is a stereo instrument by construction - the factory kit places
// every voice at its own constant-power pan position - so every take is
// written as a stereo file even when a phrase happens to sit near the centre.
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
        // Symmetric scaling by 32767 so a full-scale negative peak cannot wrap.
        const auto sample =
            static_cast<std::int32_t> (std::lround (static_cast<double> (clamped) * 32767.0));
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
    // The engine is held indirectly because it carries the atomics the editor
    // reads its metering through, which makes it neither copyable nor movable;
    // a take, on the other hand, is returned by value from every score below.
    // A freshly prepared engine starts from the factory kit with its trigger
    // counters at zero, and every per-hit variation is hash-derived from those
    // counters, so a take renders the same bytes on every run.
    explicit Take (KitParameters kit = {})
        : engine_ (std::make_unique<DrumEngine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize);
        engine_->setKitParameters (kit);
        engine_->setOutputGain (demoOutputGain);
        engine_->reset();
    }

    void setKitParameters (const KitParameters& kit)
    {
        engine_->setKitParameters (kit);
    }

    // Voices are triggered through the General MIDI note map rather than the
    // internal enum, so the demos exercise the same path a MIDI pad does.
    void strike (Instrument instrument, float velocity)
    {
        if (! engine_->triggerMidi (drumalor::getStandardMidiNote (instrument),
                                    velocity))
        {
            std::fprintf (stderr, "the GM note map dropped a demo hit\n");
            std::abort();
        }
    }

    // Strike a voice, then let the kit sound for `gapSeconds` before returning.
    void hit (Instrument instrument, float velocity, double gapSeconds)
    {
        strike (instrument, velocity);
        render (gapSeconds);
    }

    void rest (double seconds) { render (seconds); }

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

    // Brings the take to a common listening level. The returned value is the
    // gain applied, so the manifest can record what each file needed.
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
        auto remaining =
            static_cast<int> (std::lround (seconds * demoSampleRate));
        std::array<float, renderBlockSize> blockLeft {};
        std::array<float, renderBlockSize> blockRight {};

        while (remaining > 0)
        {
            const int count = std::min (renderBlockSize, remaining);
            engine_->process (blockLeft.data(), blockRight.data(), count);
            left_.insert (left_.end(), blockLeft.begin(), blockLeft.begin() + count);
            right_.insert (right_.end(), blockRight.begin(), blockRight.begin() + count);
            remaining -= count;
        }
    }

    std::unique_ptr<DrumEngine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// ---------------------------------------------------------------------------
// Groove programming: a sixteenth-note step grid
// ---------------------------------------------------------------------------

// One row of a step grid: which voice plays, and one character per sixteenth.
// 'X' is an accent, 'x' a full stroke, 'o' a lighter one, 'g' a ghost note and
// '-' leaves the step empty.
struct Track
{
    Instrument instrument;
    std::string steps;
};

float velocityForStep (char step)
{
    switch (step)
    {
        case 'X': return 1.00f;
        case 'x': return 0.85f;
        case 'o': return 0.60f;
        case 'g': return 0.32f;
        default: return 0.0f;
    }
}

// Plays a grid one sixteenth at a time: hits that share a step are triggered
// together, then the take advances by `secondsPerStep`, so a groove is stated
// on a sixteenth grid at a fixed tempo. Every row must be the same length -
// a ragged score is a programming error in this file, not a runtime state.
void playPattern (Take& take, double secondsPerStep,
                  std::initializer_list<Track> tracks)
{
    const auto steps = tracks.begin()->steps.size();
    for (const auto& track : tracks)
        if (track.steps.size() != steps)
        {
            std::fprintf (stderr, "pattern rows differ in length\n");
            std::abort();
        }

    for (std::size_t step = 0; step < steps; ++step)
    {
        for (const auto& track : tracks)
        {
            const float velocity = velocityForStep (track.steps[step]);
            if (velocity > 0.0f)
                take.strike (track.instrument, velocity);
        }
        take.rest (secondsPerStep);
    }
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// Every voice of the kit once, in note order. The gap after each hit follows
// the voice's factory decay, so a crash gets room to ring where a closed hat
// does not.
Take renderKitVocabulary()
{
    Take take;
    take.rest (0.10);

    constexpr std::array<double, drumalor::instrumentCount> gaps {{
        0.70, // Kick
        0.60, // Snare
        0.60, // Clap
        0.45, // Closed Hat
        1.00, // Open Hat
        1.70, // Ride
        1.90, // Crash
        0.80, // Low Tom
        0.70, // Mid Tom
        0.60, // High Tom
        0.50, // Shaker
        0.70, // Perc 1
        0.60, // Perc 2
    }};
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        take.hit (static_cast<Instrument> (index), 0.90f, gaps[index]);

    take.rest (1.6);
    return take;
}

// A plain rock beat at 96 BPM: eighth-note hats, a backbeat snare, and an open
// hat at the end of every second bar that the next bar's closed hat cuts off,
// because the two hats share choke group A out of the box.
Take renderRockGroove()
{
    Take take;
    take.rest (0.10);
    constexpr double step = 60.0 / 96.0 / 4.0;

    const std::string kick  = std::string ("X---------X-----") + "X---------X---X-"
                            + "X---------X-----" + "X---------X---X-";
    const std::string snare = std::string ("----X-------X---") + "----X-------X---"
                            + "----X-------X---" + "----X-------X-gX";
    const std::string chat  = std::string ("x-o-x-o-x-o-x-o-") + "x-o-x-o-x-o-x---"
                            + "x-o-x-o-x-o-x-o-" + "x-o-x-o-x-o-x---";
    const std::string ohat  = std::string ("----------------") + "--------------o-"
                            + "----------------" + "--------------o-";
    const std::string crash = std::string ("X---------------") + "----------------"
                            + "X---------------" + "----------------";

    playPattern (take, step, {
        { Instrument::Kick, kick },
        { Instrument::Snare, snare },
        { Instrument::ClosedHat, chat },
        { Instrument::OpenHat, ohat },
        { Instrument::Crash, crash },
    });

    take.rest (1.6);
    return take;
}

// A busier break at 104 BPM. The ghost notes are ordinary snare hits at low
// velocity: softer strikes excite fewer high partials and less wire noise, so
// the ghosts darken on their own rather than merely getting quieter.
Take renderBreakbeat()
{
    Take take;
    take.rest (0.10);
    constexpr double step = 60.0 / 104.0 / 4.0;

    const std::string kick  = std::string ("X-X-------X-----") + "X-X------X------";
    const std::string snare = std::string ("----X--g-g--X--g") + "----X-g--gX--g-g";
    const std::string chat  = std::string ("xoxoxoxoxoxoxoxo") + "xoxoxoxoxoxoxo--";
    const std::string ohat  = std::string ("----------------") + "--------------o-";

    for (int repeat = 0; repeat < 2; ++repeat)
        playPattern (take, step, {
            { Instrument::Kick, kick },
            { Instrument::Snare, snare },
            { Instrument::ClosedHat, chat },
            { Instrument::OpenHat, ohat },
        });

    take.rest (1.5);
    return take;
}

// MIDI velocity from a ghost stroke to a full accent on the factory snare.
// Nothing about the timbre is adjusted between hits - velocity drives the
// struck-timbre filters and the wire content as well as the level.
Take renderSnareVelocity()
{
    Take take;
    take.rest (0.10);
    for (const float velocity : { 0.08f, 0.20f, 0.33f, 0.46f, 0.60f, 0.74f, 0.87f, 1.0f })
        take.hit (Instrument::Snare, velocity, 0.55);
    take.rest (1.6);
    return take;
}

// A phrase for the drums that are not the backbeat: ride time with crashes on
// the turns, tom answers, and a descending sixteenth fill across all three
// toms into a final crash.
Take renderTomsAndCymbals()
{
    Take take;
    take.rest (0.10);
    constexpr double step = 60.0 / 92.0 / 4.0;

    const std::string ride  = std::string ("X-o-x-o-X-o-x-o-") + "X-o-x-o-X-o-x-o-"
                            + "X-o-x-o-X-o-x-o-" + "----------------";
    const std::string crash = std::string ("X---------------") + "----------------"
                            + "X---------------" + "------------X---";
    const std::string kick  = std::string ("X-------X-------") + "X-------X-------"
                            + "X-------X-------" + "X-----------X---";
    const std::string hTom  = std::string ("----------------") + "--------xx------"
                            + "----------------" + "Xxxx------------";
    const std::string mTom  = std::string ("----------------") + "----------xx----"
                            + "------x---------" + "----Xxxx--------";
    const std::string lTom  = std::string ("----------------") + "------------xx--"
                            + "-------x--------" + "--------XxxX----";

    playPattern (take, step, {
        { Instrument::Ride, ride },
        { Instrument::Crash, crash },
        { Instrument::Kick, kick },
        { Instrument::HighTom, hTom },
        { Instrument::MidTom, mTom },
        { Instrument::LowTom, lTom },
    });

    take.rest (2.6);
    return take;
}

// The same bar six times over: machine-tight, at the factory Humanise depth,
// then doubled. Humanise never moves the grid - the variation is per-hit
// pitch, decay, timbre and drive tolerance, modelled as the slow component
// drift of an analogue voice - so what changes across the sections is how
// alive the repeated hits sound, not where they land.
Take renderHumanise()
{
    Take take (KitParameters { 0.0f, 0.0f, 0.0f });
    take.rest (0.10);
    constexpr double step = 60.0 / 120.0 / 4.0;

    const std::string kick  = "X-------X-------";
    const std::string snare = "----X-------X---";
    const std::string chat  = "xxxxxxxxxxxxxxxx";

    for (const float humanise : { 0.0f, 0.5f, 1.0f })
    {
        take.setKitParameters (KitParameters { humanise, 0.0f, 0.0f });
        for (int bar = 0; bar < 2; ++bar)
            playPattern (take, step, {
                { Instrument::Kick, kick },
                { Instrument::Snare, snare },
                { Instrument::ClosedHat, chat },
            });
    }

    take.rest (1.3);
    return take;
}

// One loop three ways: dry, through Bus Drive, then glued by the bus
// compressor. The compressor's fast attack and slow release pump the hats
// after each kick, which is easiest to hear in the final third.
Take renderKitBus()
{
    Take take;
    take.rest (0.10);
    constexpr double step = 60.0 / 120.0 / 4.0;

    const std::string kick  = "X------X--X-----";
    const std::string snare = "----X-------X---";
    const std::string clap  = "------------X---";
    const std::string chat  = "x-x-x-x-x-x-x-x-";
    const std::string ohat  = "--------------x-";

    const auto section = [&take, step, &kick, &snare, &clap, &chat, &ohat] (
        float drive, float compression)
    {
        take.setKitParameters (KitParameters { 0.5f, drive, compression });
        for (int bar = 0; bar < 2; ++bar)
            playPattern (take, step, {
                { Instrument::Kick, kick },
                { Instrument::Snare, snare },
                { Instrument::Clap, clap },
                { Instrument::ClosedHat, chat },
                { Instrument::OpenHat, ohat },
            });
    };

    section (0.0f, 0.0f);
    section (0.85f, 0.0f);
    section (0.5f, 1.0f);

    take.rest (1.5);
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

const std::array<Demo, 7>& demos()
{
    static const std::array<Demo, 7> table {{
        { "01-kit-vocabulary.wav",
          "All thirteen voices of the factory kit, one hit at a time",
          renderKitVocabulary },
        { "02-rock-groove.wav",
          "A plain rock beat at 96 BPM with the open hat choked by the closed",
          renderRockGroove },
        { "03-breakbeat-ghost-notes.wav",
          "A busier break at 104 BPM with ghost-note snare", renderBreakbeat },
        { "04-snare-velocity.wav",
          "One snare from a ghost note to a full accent", renderSnareVelocity },
        { "05-toms-and-cymbals.wav",
          "Ride time, crashes and a descending tom fill", renderTomsAndCymbals },
        { "06-humanise.wav",
          "The same bar machine-tight, at the factory Humanise, then doubled",
          renderHumanise },
        { "07-kit-bus.wav",
          "One loop dry, through Bus Drive, then glued by Bus Compression",
          renderKitBus },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    Take take;
    take.hit (Instrument::Kick, 0.95f, 0.25);
    take.hit (Instrument::Snare, 0.80f, 0.25);

    if (! take.finite())
    {
        std::fprintf (stderr, "smoke test: rendered a non-finite sample\n");
        return 1;
    }
    if (take.peak() < 1.0e-3)
    {
        std::fprintf (stderr, "smoke test: rendered silence (peak %.6f)\n", take.peak());
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories (directory, error);
    const auto path = directory / "smoke.wav";
    if (! writeWav (path, take.left(), take.right()))
    {
        std::fprintf (stderr, "smoke test: could not write %s\n", path.string().c_str());
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

    std::printf ("Drumalor demo renderer smoke test passed (peak %.3f).\n", take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by DrumalorRenderDemos;"
    " edits between the markers are overwritten -->";
constexpr const char* peaksTableEnd = "<!-- peaks-table-end -->";

// Whether this directory is one this tool owns and may therefore delete from.
// The proof is the manifest the renderer itself maintains: a README carrying
// the markers it rewrites the level table between. Without that, the directory
// belongs to someone else and nothing in it may be removed - the output path is
// an ordinary command-line argument, and pointing it at a folder of music must
// not destroy the music.
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

// A demo removed from or renamed in the tables above must also disappear from
// the output directory, or automation that commits the directory preserves the
// stale file forever while the level table drops its row.
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

        // Second guard: even inside its own directory the renderer only removes
        // files that follow its own naming, so anything a person put here by
        // hand survives.
        const auto stem = entry.path().filename().string();
        if (stem.size() < 3 || ! std::isdigit (static_cast<unsigned char> (stem[0]))
            || ! std::isdigit (static_cast<unsigned char> (stem[1]))
            || stem[2] != '-')
            continue;

        const auto name = entry.path().filename().string();
        const bool known =
            std::any_of (current.begin(), current.end(), [&name] (const Demo& demo)
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

// A real minus sign rather than a hyphen, so the table reads as typeset prose.
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
    std::printf ("Updated the rendered-peak table in %s\n", readmePath.string().c_str());
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
            std::printf ("usage: DrumalorRenderDemos [--smoke] [output-directory]\n");
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
        // A take that reached full scale has been pushed into the hard clamp
        // behind the engine's master saturator, and would document that clamp
        // rather than the kit. The fix is always to render that take quieter,
        // never to ship it.
        if (renderedPeak >= 0.999)
        {
            std::fprintf (stderr,
                          "%s reached full scale (peak %.6f), so it was clipped by "
                          "the engine's master output stage; render it at a lower "
                          "gain\n",
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

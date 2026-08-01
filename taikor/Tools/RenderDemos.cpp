// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the plug-in runs, so a demonstration cannot drift away from
// what Taikor actually sounds like. No samples or external processing are
// involved anywhere: every file here comes out of the physical model.

#include "DSP/TaikoEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using taikor::Articulation;
using taikor::EngineParameters;
using taikor::TaikoEngine;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
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

// Taikor is a stereo instrument by construction - its two channels are two
// microphones at different points on the head - so every take is written as a
// stereo file even when a particular setting happens to collapse the image.
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
    explicit Take (EngineParameters parameters)
        : engine_ (std::make_unique<TaikoEngine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize);
        engine_->setParameters (parameters);
        engine_->reset();
    }

    void setParameters (const EngineParameters& parameters)
    {
        engine_->setParameters (parameters);
    }

    void handDamping (float amount) { engine_->setHandDamping (amount); }
    void pitchBend (float amount) { engine_->setPitchBend (amount); }

    // Strike the drum, then let it ring for `gapSeconds` before returning.
    void hit (Articulation articulation, int octaveOffset, float velocity,
              double gapSeconds)
    {
        engine_->trigger (articulation, octaveOffset, velocity);
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

    std::unique_ptr<TaikoEngine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// The drum the parameter defaults describe: a 55 cm nagado-daiko with a thick
// cowhide head on a heavy zelkova shell.
//
// The output gain is well below the plug-in's own default because a rim shot on
// a small drum is loud enough to reach the engine's safety limiter, and a demo
// that had been through a limiter would be showing the listener the limiter
// rather than the model. Every take is normalised to a common level afterwards,
// so rendering quietly costs nothing.
EngineParameters nagadoVoicing()
{
    EngineParameters parameters;
    // Well below the plug-in's own default. The head's high-frequency
    // continuum put a great deal more into every stroke than the resolved bank
    // alone did, and the loudest takes reached the limiter at the level that
    // used to be safe.
    parameters.outputGain = 0.06f;
    return parameters;
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// Every stroke in the vocabulary, in the order the keyboard lays them out.
Take renderVocabulary()
{
    Take take (nagadoVoicing());
    take.rest (0.10);
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        take.hit (static_cast<Articulation> (index), 0, 0.92f, 0.62);
    take.rest (1.6);
    return take;
}

// The pitch ladder: the same stroke on every octave, which is the same
// vocabulary played on a different drum each time.
Take renderOctaveLadder (Articulation articulation, double gap, double tail)
{
    Take take (nagadoVoicing());
    take.rest (0.10);
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
        take.hit (articulation, octave, 0.92f, gap);
    take.rest (tail);
    return take;
}

// A phrase on one drum of the family, so each size can be heard being played
// rather than only being struck once.
Take renderFamilyPhrase (int octave, double beat, double tail)
{
    Take take (nagadoVoicing());
    take.rest (0.08);

    // A plain kumi-daiko figure: open strokes on the beat, edge strokes off it.
    const std::array<std::pair<Articulation, float>, 16> figure {{
        { Articulation::Don, 1.00f },  { Articulation::Ka, 0.55f },
        { Articulation::Don, 0.80f },  { Articulation::Ka, 0.50f },
        { Articulation::Don, 0.95f },  { Articulation::Ko, 0.45f },
        { Articulation::Ka, 0.60f },   { Articulation::Ka, 0.55f },
        { Articulation::Don, 1.00f },  { Articulation::Su, 0.35f },
        { Articulation::Don, 0.75f },  { Articulation::Ka, 0.55f },
        { Articulation::DonRim, 0.95f }, { Articulation::Ka, 0.50f },
        { Articulation::Don, 0.85f },  { Articulation::Tsu, 0.50f },
    }};

    for (const auto& [articulation, velocity] : figure)
        take.hit (articulation, octave, velocity, beat);

    take.rest (tail);
    return take;
}

// MIDI velocity from a ghost stroke to a full-arm hit. Nothing about the
// timbre is adjusted between them: the contact time follows the impact speed,
// so the harder strokes brighten on their own.
Take renderVelocityDynamics()
{
    Take take (nagadoVoicing());
    take.rest (0.10);
    for (const float velocity : { 0.08f, 0.20f, 0.33f, 0.46f, 0.60f, 0.74f, 0.87f, 1.0f })
        take.hit (Articulation::Don, 0, velocity, 0.52);
    take.rest (1.6);
    return take;
}

// Sweeps one control across its range, striking the same stroke at each step,
// so a single axis of the model can be heard in isolation.
// `gain` overrides the render level for the sweeps whose own axis changes how
// loud the drum is - a light laminated shell struck directly is far louder than
// a solid zelkova log - so the take keeps its headroom at both ends.
Take renderParameterSweep (float EngineParameters::* field,
                           const std::array<float, 5>& values,
                           Articulation articulation, int octave, double gap,
                           double tail, float gain = 0.0f)
{
    auto parameters = nagadoVoicing();
    if (gain > 0.0f)
        parameters.outputGain = gain;
    Take take (parameters);
    take.rest (0.10);

    for (const float value : values)
    {
        parameters.*field = value;
        take.setParameters (parameters);
        take.hit (articulation, octave, 0.90f, gap);
    }

    take.rest (tail);
    return take;
}

// A hand laid on the head part way through a ringing stroke, from MIDI CC1.
Take renderHandDamping()
{
    Take take (nagadoVoicing());
    take.rest (0.08);

    take.hit (Articulation::Don, -1, 0.95f, 2.2);   // left to ring out
    take.handDamping (0.0f);
    take.hit (Articulation::Don, -1, 0.95f, 0.45);
    take.handDamping (1.0f);                        // hand goes down
    take.rest (1.2);
    take.handDamping (0.0f);
    take.hit (Articulation::Don, -1, 0.95f, 0.30);
    take.handDamping (0.55f);                       // a lighter touch
    take.rest (1.8);

    return take;
}

// The pitch wheel, which presses the head and so raises its tension.
Take renderPitchBend()
{
    Take take (nagadoVoicing());
    take.rest (0.08);

    take.hit (Articulation::Don, 0, 0.95f, 0.15);
    for (int step = 0; step <= 20; ++step)
    {
        take.pitchBend (static_cast<float> (step) / 20.0f);
        take.rest (0.045);
    }
    take.rest (0.8);

    take.pitchBend (0.0f);
    take.hit (Articulation::Don, 0, 0.95f, 0.9);
    take.pitchBend (-1.0f);
    take.rest (1.4);

    return take;
}

// Rolls, flams and ghost strokes: the parts of the vocabulary that are about
// what the sticks do rather than where they land.
Take renderRollsAndFlams()
{
    Take take (nagadoVoicing());
    take.rest (0.08);

    take.hit (Articulation::Buzz, 0, 0.80f, 0.75);
    take.hit (Articulation::Buzz, 0, 0.95f, 0.75);
    take.hit (Articulation::Flam, 0, 0.90f, 0.70);
    take.hit (Articulation::Flam, 0, 0.75f, 0.70);

    // A hand-played roll: alternating strokes accelerating into a Don.
    double gap = 0.155;
    for (int stroke = 0; stroke < 14; ++stroke)
    {
        take.hit (stroke % 2 == 0 ? Articulation::Ko : Articulation::Su, 0,
                  0.38f + 0.035f * static_cast<float> (stroke), gap);
        gap *= 0.90;
    }
    take.hit (Articulation::Don, 0, 1.0f, 0.9);

    take.hit (Articulation::Bachi, 0, 0.85f, 0.30);
    take.hit (Articulation::Bachi, 0, 0.85f, 0.30);
    take.hit (Articulation::Katsu, 0, 0.90f, 0.55);
    take.rest (1.4);

    return take;
}

// A longer piece moving between three drums of the family, so the octave
// mapping can be heard as an ensemble rather than as a ladder.
Take renderEnsemblePiece()
{
    Take take (nagadoVoicing());
    take.rest (0.08);

    constexpr double beat = 0.30;
    const auto bar = [&take] (int octave, double unit)
    {
        take.hit (Articulation::Don, octave, 1.00f, unit);
        take.hit (Articulation::Ka, octave + 1, 0.55f, unit * 0.5);
        take.hit (Articulation::Ka, octave + 1, 0.50f, unit * 0.5);
        take.hit (Articulation::Don, octave, 0.85f, unit);
        take.hit (Articulation::Ko, octave + 1, 0.45f, unit * 0.5);
        take.hit (Articulation::Ka, octave + 1, 0.55f, unit * 0.5);
    };

    // Odaiko underneath, mid drum on top.
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        take.hit (Articulation::Don, -2, 1.0f, beat * 0.02);
        bar (-1, beat);
        bar (-1, beat);
    }

    // The small drum answers.
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        take.hit (Articulation::Don, -2, 0.9f, beat * 0.02);
        bar (1, beat * 0.75);
    }

    // Everything together.
    take.hit (Articulation::Don, -2, 1.0f, 0.02);
    take.hit (Articulation::Don, -1, 1.0f, 0.02);
    take.hit (Articulation::Don, 0, 0.95f, 0.02);
    take.hit (Articulation::Don, 1, 0.90f, beat * 2.0);
    take.hit (Articulation::DonRim, 0, 1.0f, 0.02);
    take.hit (Articulation::Don, -2, 1.0f, 3.0);

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

Take renderOctavesDon() { return renderOctaveLadder (Articulation::Don, 0.95, 2.4); }
Take renderOctavesKa() { return renderOctaveLadder (Articulation::Ka, 0.60, 1.4); }
Take renderOctavesRim() { return renderOctaveLadder (Articulation::DonRim, 0.80, 1.8); }

Take renderOdaiko() { return renderFamilyPhrase (-2, 0.42, 3.2); }
Take renderNagado() { return renderFamilyPhrase (0, 0.30, 1.8); }
Take renderShime() { return renderFamilyPhrase (2, 0.20, 0.9); }

Take renderBachiHardness()
{
    return renderParameterSweep (&EngineParameters::bachiHardness,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Don, 0, 0.62, 1.6);
}

Take renderStrikePositionSweep()
{
    return renderParameterSweep (&EngineParameters::strikePosition,
                                 { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f },
                                 Articulation::Ko, 0, 0.60, 1.5);
}

Take renderTensionSweep()
{
    return renderParameterSweep (&EngineParameters::tension,
                                 { 0.05f, 0.30f, 0.55f, 0.80f, 1.0f },
                                 Articulation::Don, 0, 0.72, 1.8);
}

Take renderHeadMaterialSweep()
{
    return renderParameterSweep (&EngineParameters::headMaterial,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Don, 0, 0.72, 1.8);
}

Take renderShellMaterialSweep()
{
    return renderParameterSweep (&EngineParameters::shellMaterial,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Katsu, 0, 0.60, 1.4, 0.04f);
}

Take renderCavitySweep()
{
    return renderParameterSweep (&EngineParameters::cavityCoupling,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Don, 0, 0.72, 1.8);
}

Take renderBodyDepthSweep()
{
    return renderParameterSweep (&EngineParameters::bodyDepth,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Don, 0, 0.72, 1.8);
}

Take renderHeadDampingSweep()
{
    return renderParameterSweep (&EngineParameters::headDamping,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Don, 0, 0.85, 2.0);
}

Take renderOctaveBodySweep()
{
    // The same two octaves realised as a tuned drum and as a smaller drum.
    auto parameters = nagadoVoicing();
    Take take (parameters);
    take.rest (0.10);

    for (const float body : { 0.0f, 1.0f })
    {
        parameters.octaveBody = body;
        take.setParameters (parameters);
        for (const int octave : { -1, 0, 1, 2 })
            take.hit (Articulation::Don, octave, 0.92f, 0.66);
        take.rest (0.5);
    }

    take.rest (1.4);
    return take;
}

Take renderMicDistanceSweep()
{
    return renderParameterSweep (&EngineParameters::micDistance,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Ka, 0, 0.60, 1.4);
}

Take renderMicSpreadSweep()
{
    return renderParameterSweep (&EngineParameters::micSpread,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Ka, 0, 0.60, 1.4);
}

const std::array<Demo, 23>& demos()
{
    static const std::array<Demo, 23> table {{
        { "01-stroke-vocabulary.wav",
          "All twelve strokes on the reference drum, in keyboard order",
          renderVocabulary },
        { "02-octaves-don.wav",
          "A Don on each of the six octaves: the pitch ladder", renderOctavesDon },
        { "03-octaves-ka.wav", "An edge Ka on each of the six octaves",
          renderOctavesKa },
        { "04-octaves-rim-shot.wav", "A rim shot on each of the six octaves",
          renderOctavesRim },
        { "05-odaiko-phrase.wav", "A phrase on the largest drum", renderOdaiko },
        { "06-nagado-phrase.wav", "The same phrase on the reference drum",
          renderNagado },
        { "07-shime-phrase.wav", "The same phrase on a small tight drum",
          renderShime },
        { "08-velocity-dynamics.wav",
          "One stroke from a ghost note to a full-arm hit", renderVelocityDynamics },
        { "09-bachi-hardness.wav", "Felt beater through to a hard oak bachi",
          renderBachiHardness },
        { "10-strike-position.wav", "The same stroke walked from centre to rim",
          renderStrikePositionSweep },
        { "11-head-tension.wav", "Slack head through to fully tacked",
          renderTensionSweep },
        { "12-head-material.wav", "Thin synthetic film through to thick cowhide",
          renderHeadMaterialSweep },
        { "13-shell-material.wav",
          "Light laminated ply through to dense carved zelkova, struck on the body",
          renderShellMaterialSweep },
        { "14-air-coupling.wav", "Open body through to a fully sealed one",
          renderCavitySweep },
        { "15-body-depth.wav", "Shallow body through to deep", renderBodyDepthSweep },
        { "16-head-damping.wav", "Open head through to heavily damped",
          renderHeadDampingSweep },
        { "17-octave-body.wav",
          "Four octaves as a tuned drum, then the same four as smaller drums",
          renderOctaveBodySweep },
        { "18-mic-distance.wav", "The close pair from 3 cm out to 40 cm",
          renderMicDistanceSweep },
        { "19-mic-spread.wav", "The close pair from coincident to fully opened",
          renderMicSpreadSweep },
        { "20-hand-damping.wav", "A hand laid on a ringing head, from MIDI CC1",
          renderHandDamping },
        { "21-pitch-wheel.wav", "The wheel pressing the head sharp and flat",
          renderPitchBend },
        { "22-rolls-and-flams.wav", "Buzz rolls, flams, stick clicks and a shell hit",
          renderRollsAndFlams },
        { "23-ensemble-piece.wav",
          "A longer piece moving between three drums of the family",
          renderEnsemblePiece },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    Take take (nagadoVoicing());
    take.hit (Articulation::Don, 0, 0.95f, 0.25);
    take.hit (Articulation::Ka, 1, 0.80f, 0.25);

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

    std::printf ("Taikor demo renderer smoke test passed (peak %.3f).\n", take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by TaikorRenderDemos;"
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
            std::printf ("usage: TaikorRenderDemos [--smoke] [output-directory]\n");
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
        // A take that reached full scale has been through the engine's safety
        // limiter, and would document the limiter rather than the model. The
        // fix is always to render that take quieter, never to ship it.
        if (renderedPeak >= 0.999)
        {
            std::fprintf (stderr,
                          "%s reached full scale (peak %.6f), so it was clipped by "
                          "the engine's output limiter; render it at a lower gain\n",
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

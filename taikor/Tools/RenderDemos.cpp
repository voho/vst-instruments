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

    // Append four already-rendered, equally long performances at unity total
    // gain. This is the same routing a user gets from four layered instances.
    void appendAverage (const std::array<const Take*, 4>& performances)
    {
        const auto frames = performances.front()->left_.size();
        left_.reserve (left_.size() + frames);
        right_.reserve (right_.size() + frames);

        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            float left = 0.0f;
            float right = 0.0f;
            for (const auto* performance : performances)
            {
                left += performance->left_[frame];
                right += performance->right_[frame];
            }
            left_.push_back (0.25f * left);
            right_.push_back (0.25f * right);
        }
    }

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

// The drum the parameter defaults describe: a 150 cm odaiko with a thick
// cowhide head on a heavy zelkova shell.
//
// The output gain is well below the plug-in's own default because a rim shot on
// a small drum is loud enough to reach the engine's safety limiter, and a demo
// that had been through a limiter would be showing the listener the limiter
// rather than the model. Every take is normalised to a common level afterwards,
// so rendering quietly costs nothing.
EngineParameters defaultVoicing()
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

// Every stroke of the vocabulary on one drum, in the order the keyboard lays
// them out: Don, Ka, Tsu, Don Rim.
Take renderVocabulary()
{
    Take take (defaultVoicing());
    take.rest (0.10);
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        take.hit (static_cast<Articulation> (index), 0, 0.92f, 0.62);
    take.rest (1.6);
    return take;
}

// The pitch ladder: the same stroke on each of the four drums, which is the
// same technique played on a different instrument each time.
Take renderDrumLadder (Articulation articulation, double gap, double tail)
{
    Take take (defaultVoicing());
    take.rest (0.10);
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
        take.hit (articulation, octave, 0.92f, gap);
    take.rest (tail);
    return take;
}

// The whole playing grid, read the way it is laid out: each drum in turn, and
// on each of them all four strokes.
Take renderGrid()
{
    Take take (defaultVoicing());
    take.rest (0.10);

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        // The small drums speak and empty faster, so they are given less room.
        const double gap = 0.62 / std::pow (1.35, static_cast<double> (octave));
        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
            take.hit (static_cast<Articulation> (index), octave, 0.92f, gap);
        take.rest (gap);
    }

    take.rest (1.6);
    return take;
}

// A phrase on one drum of the family, so each instrument can be heard being
// played rather than only being struck once.
Take renderFamilyPhrase (int octave, double beat, double tail)
{
    Take take (defaultVoicing());
    take.rest (0.08);

    // A plain kumi-daiko figure: open strokes on the beat, edge strokes off it,
    // a damped centre where the phrase turns and a rim shot for the accent.
    const std::array<std::pair<Articulation, float>, 16> figure {{
        { Articulation::Don, 1.00f },  { Articulation::Ka, 0.55f },
        { Articulation::Don, 0.80f },  { Articulation::Ka, 0.50f },
        { Articulation::Don, 0.95f },  { Articulation::Tsu, 0.45f },
        { Articulation::Ka, 0.60f },   { Articulation::Ka, 0.55f },
        { Articulation::Don, 1.00f },  { Articulation::Tsu, 0.35f },
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
    auto parameters = defaultVoicing();
    parameters.humanise = 0.0f;
    Take take (parameters);
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
    auto parameters = defaultVoicing();
    // A diagnostic sweep must move only the named axis. Humanise deliberately
    // perturbs position, angle, speed and contact, so leave it to performance
    // takes and make these five strikes exactly comparable.
    parameters.humanise = 0.0f;
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
    auto parameters = defaultVoicing();
    parameters.humanise = 0.0f;
    Take take (parameters);
    take.rest (0.08);

    take.hit (Articulation::Don, 0, 0.95f, 2.2);   // left to ring out
    take.handDamping (0.0f);
    take.hit (Articulation::Don, 0, 0.95f, 0.45);
    take.handDamping (1.0f);                       // hand goes down
    take.rest (1.2);
    take.handDamping (0.0f);
    take.hit (Articulation::Don, 0, 0.95f, 0.30);
    take.handDamping (0.55f);                       // a lighter touch
    take.rest (1.8);

    return take;
}

// The pitch wheel, which presses the head and so raises its tension.
Take renderPitchBend()
{
    Take take (defaultVoicing());
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

// Rolls, flams and press rolls, played rather than provided. None of these is
// a key on the grid, and none of them needs to be: a flam is two notes and a
// roll is a lot of them, and what makes either one sound like a drum being
// played rather than like a sample repeating is that a stick landing on a
// moving head takes energy back out of it.
Take renderRollsAndPresses()
{
    Take take (defaultVoicing());
    take.rest (0.08);

    // A press roll: the stick pushed into the head and left to bounce, which is
    // a train of contacts falling in level and spacing.
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        double gap = 0.019;
        float velocity = 0.80f;
        for (int bounce = 0; bounce < 11; ++bounce)
        {
            take.hit (Articulation::Tsu, 0, velocity, gap);
            gap *= 0.82;
            velocity *= 0.86f;
        }
        take.rest (0.55);
    }

    // A flam: a grace note a thirty-second ahead of the stroke it leans into.
    take.hit (Articulation::Don, 0, 0.42f, 0.032);
    take.hit (Articulation::Don, 0, 0.90f, 0.67);
    take.hit (Articulation::Ka, 0, 0.38f, 0.032);
    take.hit (Articulation::Don, 0, 0.75f, 0.67);

    // A hand-played roll: alternating strokes accelerating into a rim shot.
    double gap = 0.155;
    for (int stroke = 0; stroke < 14; ++stroke)
    {
        take.hit (stroke % 2 == 0 ? Articulation::Ka : Articulation::Tsu, 0,
                  0.38f + 0.035f * static_cast<float> (stroke), gap);
        gap *= 0.90;
    }
    take.hit (Articulation::DonRim, 0, 1.0f, 1.4);

    return take;
}

// A longer piece moving between all four drums, so the grid can be heard as an
// ensemble rather than as a ladder.
Take renderEnsemblePiece()
{
    Take take (defaultVoicing());
    take.rest (0.08);

    constexpr double beat = 0.30;
    const auto bar = [&take] (int low, int high, double unit)
    {
        take.hit (Articulation::Don, low, 1.00f, unit);
        take.hit (Articulation::Ka, high, 0.55f, unit * 0.5);
        take.hit (Articulation::Ka, high, 0.50f, unit * 0.5);
        take.hit (Articulation::Don, low, 0.85f, unit);
        take.hit (Articulation::Tsu, high, 0.45f, unit * 0.5);
        take.hit (Articulation::Ka, high, 0.55f, unit * 0.5);
    };

    // O-daiko underneath, chu-daiko on top.
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        take.hit (Articulation::Don, 0, 1.0f, beat * 0.02);
        bar (0, 1, beat);
        bar (0, 1, beat);
    }

    // The okedo and the shime answer.
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        take.hit (Articulation::Don, 0, 0.9f, beat * 0.02);
        bar (2, 3, beat * 0.75);
    }

    // Everything together.
    take.hit (Articulation::Don, 0, 1.0f, 0.02);
    take.hit (Articulation::Don, 1, 1.0f, 0.02);
    take.hit (Articulation::Don, 2, 0.95f, 0.02);
    take.hit (Articulation::Don, 3, 0.90f, beat * 2.0);
    take.hit (Articulation::DonRim, 1, 1.0f, 0.02);
    take.hit (Articulation::Don, 0, 1.0f, 3.0);

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

Take renderDrumsDon() { return renderDrumLadder (Articulation::Don, 0.95, 2.4); }
Take renderDrumsKa() { return renderDrumLadder (Articulation::Ka, 0.60, 1.4); }
Take renderDrumsRim() { return renderDrumLadder (Articulation::DonRim, 0.80, 1.8); }

Take renderOdaiko() { return renderFamilyPhrase (0, 0.42, 3.2); }
Take renderChudaiko() { return renderFamilyPhrase (1, 0.30, 1.8); }
Take renderOkedo() { return renderFamilyPhrase (2, 0.24, 1.2); }
Take renderShime() { return renderFamilyPhrase (3, 0.20, 0.9); }

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
                                 Articulation::Don, 0, 0.60, 1.5);
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
    // A rim shot, because it is the stroke that catches the hoop and the body
    // with the head and so drives the wooden bank hardest.
    return renderParameterSweep (&EngineParameters::shellMaterial,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::DonRim, 0, 0.70, 1.8, 0.012f);
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

Take renderDrumLayout()
{
    // The keyboard twice over: first as one drum retuned four times, then as
    // the four instruments of the family. Both land on the same four pitches,
    // and they do not sound remotely alike.
    auto parameters = defaultVoicing();
    parameters.humanise = 0.0f;
    Take take (parameters);
    take.rest (0.10);

    for (const float body : { 0.0f, 1.0f })
    {
        parameters.octaveBody = body;
        take.setParameters (parameters);
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
            take.hit (Articulation::Don, octave, 0.92f, 0.66);
        take.rest (0.5);
    }

    take.rest (1.4);
    return take;
}

// Mic Distance moves the whole instrument, and a head stroke only shows two
// thirds of it: the head's own near field and its continuum both change with
// the pair, and so now does the wooden body - but only a hoop strike wakes the
// body at all. So the sweep is taken twice, once on a Ka and once on a Don Rim,
// and the second pass is the one where the drum's shell recedes with the
// capsules rather than staying nailed at one level behind them.
Take renderMicDistanceSweep()
{
    constexpr std::array<float, 5> positions { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    auto parameters = defaultVoicing();
    // A diagnostic sweep must move only the named axis.
    parameters.humanise = 0.0f;
    Take take (parameters);
    take.rest (0.10);

    for (const auto articulation : { Articulation::Ka, Articulation::DonRim })
    {
        for (const float value : positions)
        {
            parameters.micDistance = value;
            take.setParameters (parameters);
            take.hit (articulation, 0, 0.90f, 0.60);
        }
        take.rest (0.50);
    }

    take.rest (1.4);
    return take;
}

Take renderMicSpreadSweep()
{
    return renderParameterSweep (&EngineParameters::micSpread,
                                 { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f },
                                 Articulation::Ka, 0, 0.60, 1.4);
}

// Authored polar placement with no random travel: left, dead centre and right,
// followed by the same off-centre Don walking once around the head.
Take renderPolarStrikes()
{
    auto parameters = defaultVoicing();
    parameters.humanise = 0.0f;
    parameters.outputGain = 0.04f;
    Take take (parameters);
    take.rest (0.10);

    constexpr float pi = 3.14159265358979323846f;
    for (const auto [position, azimuth] :
         { std::pair { 0.0f, pi }, std::pair { -1.0f, 0.0f },
           std::pair { 0.0f, 0.0f } })
    {
        parameters.strikePosition = position;
        parameters.strikeAzimuth = azimuth;
        take.setParameters (parameters);
        take.hit (Articulation::Don, 0, 0.90f, 0.78);
    }

    parameters.strikePosition = 1.0f;
    for (const float azimuth :
         { 0.0f, 0.25f * pi, 0.5f * pi, 0.75f * pi,
           pi, -0.75f * pi, -0.5f * pi, -0.25f * pi })
    {
        parameters.strikeAzimuth = azimuth;
        take.setParameters (parameters);
        take.hit (Articulation::Don, 0, 0.82f, 0.55);
    }

    take.rest (1.8);
    return take;
}

Take renderPerformerPhrase (int performer)
{
    auto parameters = defaultVoicing();
    parameters.humanise = 0.7f;
    parameters.performer = performer;
    Take take (parameters);
    take.rest (0.08);
    take.hit (Articulation::Don, 0, 0.90f, 0.34);
    take.hit (Articulation::Ka, 0, 0.58f, 0.18);
    take.hit (Articulation::Tsu, 0, 0.52f, 0.18);
    take.hit (Articulation::Don, 0, 0.82f, 0.34);
    take.hit (Articulation::DonRim, 0, 0.78f, 1.45);
    return take;
}

// The first phrase is four phase-locked P1 copies. The second changes only
// their Performer choices to P1-P4; both layers retain unity total gain.
Take renderPerformerEnsemble()
{
    auto p1 = renderPerformerPhrase (0);
    auto p2 = renderPerformerPhrase (1);
    auto p3 = renderPerformerPhrase (2);
    auto p4 = renderPerformerPhrase (3);

    Take take (defaultVoicing());
    take.appendAverage ({ &p1, &p1, &p1, &p1 });
    take.rest (0.55);
    take.appendAverage ({ &p1, &p2, &p3, &p4 });
    return take;
}

const std::array<Demo, 27>& demos()
{
    static const std::array<Demo, 27> table {{
        { "01-stroke-vocabulary.wav",
          "All four strokes on the o-daiko, in keyboard order",
          renderVocabulary },
        { "02-the-four-drums.wav",
          "A Don on each of the four drums: o-daiko, chu-daiko, okedo, shime",
          renderDrumsDon },
        { "03-the-playing-grid.wav",
          "The whole grid: four strokes on each of the four drums", renderGrid },
        { "04-drums-ka.wav", "An edge Ka on each of the four drums", renderDrumsKa },
        { "05-drums-rim-shot.wav", "A rim shot on each of the four drums",
          renderDrumsRim },
        { "06-odaiko-phrase.wav", "A phrase on the o-daiko", renderOdaiko },
        { "07-chudaiko-phrase.wav", "The same phrase on the chu-daiko",
          renderChudaiko },
        { "08-okedo-phrase.wav", "The same phrase on the okedo-daiko", renderOkedo },
        { "09-shime-phrase.wav", "The same phrase on the shime-daiko", renderShime },
        { "10-velocity-dynamics.wav",
          "One stroke from a ghost note to a full-arm hit", renderVelocityDynamics },
        { "11-rolls-and-presses.wav",
          "Press rolls, a played flam and a roll accelerating into a rim shot",
          renderRollsAndPresses },
        { "12-bachi-hardness.wav", "Felt beater through to a hard oak bachi",
          renderBachiHardness },
        { "13-strike-position.wav", "The same stroke walked from centre to rim",
          renderStrikePositionSweep },
        { "14-head-tension.wav", "Slack head through to fully tacked",
          renderTensionSweep },
        { "15-head-material.wav", "Thin synthetic film through to thick cowhide",
          renderHeadMaterialSweep },
        { "16-shell-material.wav",
          "Light laminated staves through to dense carved zelkova, on a rim shot",
          renderShellMaterialSweep },
        { "17-air-coupling.wav", "Open body through to a fully sealed one",
          renderCavitySweep },
        { "18-body-depth.wav", "Shallow body through to deep", renderBodyDepthSweep },
        { "19-head-damping.wav", "Open head through to heavily damped",
          renderHeadDampingSweep },
        { "20-octave-body.wav",
          "The keyboard as one drum retuned four times, then as the four drums",
          renderDrumLayout },
        { "21-mic-distance.wav",
          "The close pair from 3 cm out to 40 cm, on the head and then on the body",
          renderMicDistanceSweep },
        { "22-mic-spread.wav", "The close pair from coincident to fully opened",
          renderMicSpreadSweep },
        { "23-hand-damping.wav", "A hand laid on a ringing head, from MIDI CC1",
          renderHandDamping },
        { "24-pitch-wheel.wav", "The wheel pressing the head sharp and flat",
          renderPitchBend },
        { "25-ensemble-piece.wav",
          "A longer piece moving between all four drums of the grid",
          renderEnsemblePiece },
        { "26-polar-strikes.wav",
          "Fixed left, centre and right strikes, then one circuit around the head",
          renderPolarStrikes },
        { "27-performer-ensemble.wav",
          "Phase-locked P1 copies, then the same phrase layered as P1-P4",
          renderPerformerEnsemble },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    Take take (defaultVoicing());
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

// The rendered-peak table lives in the instrument's own README, beside the
// audio-demo list a reader is looking at, so each instrument keeps exactly one
// document. Only the instrument's own Docs/audio directory has such a README:
// an ad-hoc output directory carries none, and resolving one there is how the
// renderer knows the difference.
std::filesystem::path instrumentReadme (const std::filesystem::path& directory)
{
    auto normalised = directory.lexically_normal();
    if (normalised.filename().empty())
        normalised = normalised.parent_path();

    if (normalised.filename() != "audio"
        || normalised.parent_path().filename() != "Docs")
        return {};

    return normalised.parent_path().parent_path() / "README.md";
}


// Whether this directory is one this tool owns and may therefore delete from.
// The proof is the manifest the renderer itself maintains: a README carrying
// the markers it rewrites the level table between. Without that, the directory
// belongs to someone else and nothing in it may be removed - the output path is
// an ordinary command-line argument, and pointing it at a folder of music must
// not destroy the music.
bool ownsDirectory (const std::filesystem::path& directory)
{
    const auto readmePath = instrumentReadme (directory);
    if (readmePath.empty() || ! std::filesystem::exists (readmePath))
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
    const auto readmePath = instrumentReadme (directory);
    if (readmePath.empty() || ! std::filesystem::exists (readmePath))
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

// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free learning and synthesis path the plug-in runs, so a demonstration
// cannot drift away from what Neuramar actually does. Neuramar ships no
// samples, and neither does this tool: the training input is synthesized
// procedurally below - the same kind of deterministic fixture the regression
// suite learns from - the model is fitted from it exactly as the plug-in
// would fit a dropped file, and every demo is the fitted model being played.
// Everything in the output directory comes out of code.

#include "DSP/NeuramarEngine.h"
#include "DSP/NeuralModel.h"
#include "DSP/SampleLearner.h"

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
using neuramar::EngineParameters;
using neuramar::NeuralModel;
using neuramar::NeuramarEngine;
using neuramar::SampleLearner;

constexpr double pi = 3.14159265358979323846;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;

// The training input is synthesized and analysed at 48 kHz because that is the
// fixed analysis rate the learner's whole regression suite is written against.
constexpr double trainingSampleRate = 48000.0;
constexpr double trainingSeconds = 2.4;

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian (std::vector<std::uint8_t>& bytes, std::uint32_t value,
                         int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back (static_cast<std::uint8_t> ((value >> (8 * index)) & 0xffu));
}

// Neuramar is a stereo instrument - Horizon spreads chord voices across the
// field and gives the Air layer a decorrelated second realization - so every
// take is written as a stereo file even when a particular setting happens to
// collapse the image.
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
// The training input: a procedural tone in place of a user's recording
// ---------------------------------------------------------------------------

// One plucked-then-sustained stiff-string tone at A3, assembled from the same
// ingredients the regression suite's learning fixtures use so every layer of
// the model has genuine evidence to fit:
//
//  - a dense stretched partial series f(n) = n f0 sqrt(1 + B n^2) with
//    B = 3e-4, the coefficient the suite's stiff-string fixture proves the
//    learner recovers (Core, and the fitted inharmonicity);
//  - a slowly darkening spectrum and a gently breathing second/third-partial
//    balance, so the neural trajectory has an evolution to learn;
//  - a noisy pluck at the onset and a low breath floor through the sustain
//    (Air), from the same xorshift/one-pole recipe as the suite's fixtures;
//  - two sustained inharmonic body resonances placed 0.35 of a partial
//    spacing away from the stretched series - the suite's persistent-Bone
//    ratio convention - so they are learned as Bone modes rather than read as
//    members of the partial series.
//
// Everything is a closed-form function of the sample index, so the tone is
// bit-identical on every run and there is nothing to record or license.
std::vector<float> makeTrainingTone (double sampleRate, double durationSeconds)
{
    constexpr double fundamentalHz = 220.0;
    constexpr double inharmonicity = 3.0e-4;
    constexpr int partialCount = 24;
    constexpr std::array<double, 2> boneRatios { 2.35, 3.35 };

    const auto sampleCount = static_cast<std::size_t> (
        std::llround (sampleRate * durationSeconds));
    std::vector<float> tone (sampleCount, 0.0f);

    std::uint32_t burstNoiseState = 0x3c6ef372u;
    std::uint32_t breathNoiseState = 0x73594a1du;
    float previousBurstNoise = 0.0f;
    float previousBreathNoise = 0.0f;
    const auto nextNoise = [] (std::uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * (2.0f / 4294967295.0f) - 1.0f;
    };

    // The tone sustains rather than decaying to silence, so without a played
    // ending its last sample would be a hard cut - a click in the spliced
    // demos and an arbitrary final frame in the learned trajectory. A short
    // raised-cosine release gives it a natural close instead.
    constexpr double releaseSeconds = 0.08;

    for (std::size_t index = 0; index < tone.size(); ++index)
    {
        const double time = static_cast<double> (index) / sampleRate;
        const double attack = 1.0 - std::exp (-time / 0.005);
        const double remaining = durationSeconds - time;
        const double release = remaining < releaseSeconds
            ? 0.5 - 0.5 * std::cos (pi * remaining / releaseSeconds)
            : 1.0;
        const double evolution = 0.5 + 0.5 * std::sin (2.0 * pi * 0.73 * time);

        double value = 0.0;
        for (int partial = 1; partial <= partialCount; ++partial)
        {
            const double number = static_cast<double> (partial);
            const double frequency = fundamentalHz * number
                * std::sqrt (1.0 + inharmonicity * number * number);
            if (frequency >= 0.44 * sampleRate)
                break;

            // Each partial decays toward a sustained floor, higher partials
            // faster, so the pluck darkens into a warm held body.
            double amplitude = std::pow (number, -1.05)
                * (0.30 + 0.70 * std::exp (-time * (0.05 + 0.09 * number)));
            if (partial == 2)
                amplitude *= 0.85 + 0.30 * evolution;
            else if (partial == 3)
                amplitude *= 1.15 - 0.30 * evolution;

            value += amplitude * std::sin (2.0 * pi * frequency * time
                                           + 0.31 * number + 0.11 * number * number);
        }

        for (std::size_t mode = 0; mode < boneRatios.size(); ++mode)
        {
            value += attack * 0.05 * std::exp (-time / 1.5)
                * std::sin (2.0 * pi * fundamentalHz * boneRatios[mode] * time
                            + 0.43 * static_cast<double> (mode + 1));
        }

        const float burstNoise = nextNoise (burstNoiseState);
        const float highBurst = burstNoise - 0.96f * previousBurstNoise;
        previousBurstNoise = burstNoise;
        value += 0.22 * std::exp (-time / 0.010) * static_cast<double> (highBurst);

        const float breathNoise = nextNoise (breathNoiseState);
        const float highBreath = breathNoise - 0.92f * previousBreathNoise;
        previousBreathNoise = breathNoise;
        value += 0.016 * (0.35 + 0.65 * evolution) * static_cast<double> (highBreath);

        tone[index] = static_cast<float> (0.34 * attack * release * value);
    }
    return tone;
}

// ---------------------------------------------------------------------------
// A take: an imperative score driving the engine
// ---------------------------------------------------------------------------

class Take
{
public:
    // The engine is held indirectly because it carries the atomics the host
    // thread writes parameters through, which makes it neither copyable nor
    // movable; a take, on the other hand, is returned by value from every
    // score below. The caller keeps the model alive for the take's lifetime.
    Take (const NeuralModel& model, const EngineParameters& parameters)
        : engine_ (std::make_unique<NeuramarEngine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize);
        engine_->setModel (&model);
        engine_->setParameters (parameters);
    }

    void setParameters (const EngineParameters& parameters)
    {
        engine_->setParameters (parameters);
    }

    void noteOn (int midiNote, float velocity) { engine_->noteOn (midiNote, velocity); }
    void noteOff (int midiNote) { engine_->noteOff (midiNote); }
    void releaseAll() { engine_->allNotesOff(); }

    // Strike a note, hold it for `holdSeconds`, then release it and let the
    // Dissolve tail ring for `gapSeconds` before returning.
    void play (int midiNote, float velocity, double holdSeconds, double gapSeconds)
    {
        engine_->noteOn (midiNote, velocity);
        render (holdSeconds);
        engine_->noteOff (midiNote);
        render (gapSeconds);
    }

    void rest (double seconds) { render (seconds); }

    // Splices a raw mono signal into both channels, bypassing the engine.
    // This is how the training tone itself - the input, not the model - is
    // put next to the model's replay of it. The gain matches the spliced
    // signal to the engine's listening level: the learner normalises its
    // input before analysis, so the raw tone's own level and the fitted
    // model's calibrated render level are not comparable without it.
    void appendMono (const std::vector<float>& samples, float gain)
    {
        for (const float sample : samples)
        {
            left_.push_back (sample * gain);
            right_.push_back (sample * gain);
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

    std::unique_ptr<NeuramarEngine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// The faithful voicing: the learned Core at full Imprint with the Air and
// Bone layers at exactly their learned levels, no variation, no performance
// shaping. This is the setting under which the model is honestly compared
// with its own training input.
//
// The render gain is well below the plug-in's default because every take is
// normalised to a common level afterwards, so rendering quietly costs nothing
// and guarantees that no file ever documents the 16-bit clamp instead of the
// model.
EngineParameters faithfulVoicing()
{
    EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 0.65f;
    parameters.air = 1.0f;
    parameters.bone = 1.0f;
    parameters.brightness = 0.5f;
    parameters.evolutionRate = 1.0f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.noise = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.releaseSeconds = 0.35f;
    parameters.spread = 0.0f;
    parameters.rootOffsetSemitones = 0.0f;
    parameters.outputGain = 0.30f;
    parameters.stretch = 1.0f;
    parameters.formantShiftSemitones = 0.0f;
    parameters.touch = 0.0f;
    parameters.registerTilt = 0.0f;
    return parameters;
}

// What every demo needs: the fitted model and the training tone resampled to
// the demo rate by the learner's own band-limited resampler, so the audible
// "input" really is the signal the model was fitted from.
struct DemoContext
{
    const NeuralModel& model;
    const std::vector<float>& trainingTone;
};

// The gain that brings the spliced training tone to the same listening level
// as the engine's faithful replay of it, so the comparison in demo 02 is
// heard as a change of character rather than as a change of volume.
constexpr float trainingToneSpliceGain = 0.25f;

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// The synthesized training input itself, twice, untouched by the engine. This
// is the entire "recording" the instrument below was learned from.
Take renderTrainingTone (const DemoContext& context)
{
    Take take (context.model, faithfulVoicing());
    take.rest (0.15);
    take.appendMono (context.trainingTone, trainingToneSpliceGain);
    take.rest (0.70);
    take.appendMono (context.trainingTone, trainingToneSpliceGain);
    take.rest (0.40);
    return take;
}

// The comparison the instrument stands on: the training tone once, then the
// fitted model replaying its learned root note for the same duration.
Take renderLearnedVersusSource (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.releaseSeconds = 0.6f;
    Take take (context.model, parameters);
    take.rest (0.15);
    take.appendMono (context.trainingTone, trainingToneSpliceGain);
    take.rest (0.70);
    take.play (context.model.rootMidiNote(), 1.0f, trainingSeconds, 1.20);
    return take;
}

// A melodic phrase across two octaves around the learned root: every note
// except the root itself is pitch the model never heard.
Take renderMelody (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.orbit = 0.30f;
    parameters.releaseSeconds = 0.5f;
    Take take (context.model, parameters);
    take.rest (0.10);

    const int root = context.model.rootMidiNote();
    struct Step { int offset; float velocity; double hold; double gap; };
    const std::array<Step, 12> phrase {{
        { 0, 0.90f, 0.42, 0.08 },   { 3, 0.80f, 0.42, 0.08 },
        { 7, 0.85f, 0.42, 0.08 },   { 12, 0.95f, 0.80, 0.10 },
        { 10, 0.80f, 0.42, 0.08 },  { 7, 0.78f, 0.42, 0.08 },
        { 3, 0.82f, 0.42, 0.08 },   { 0, 0.90f, 0.80, 0.10 },
        { -4, 0.78f, 0.42, 0.08 },  { -5, 0.80f, 0.42, 0.08 },
        { -7, 0.82f, 0.42, 0.08 },  { -12, 0.95f, 1.60, 0.10 },
    }};
    for (const auto& step : phrase)
        take.play (root + step.offset, step.velocity, step.hold, step.gap);

    take.rest (1.00);
    return take;
}

// The same five velocities twice: first with Touch at zero, where velocity is
// level alone, then with Touch fully open, where harder notes also lean
// brighter and let more of the learned Air through.
Take renderVelocityTouch (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.releaseSeconds = 0.4f;
    Take take (context.model, parameters);
    take.rest (0.10);

    const int root = context.model.rootMidiNote();
    constexpr std::array<float, 5> velocities { 0.25f, 0.45f, 0.65f, 0.85f, 1.0f };

    for (const float velocity : velocities)
        take.play (root, velocity, 0.50, 0.30);

    take.rest (0.60);
    parameters.touch = 1.0f;
    take.setParameters (parameters);

    for (const float velocity : velocities)
        take.play (root, velocity, 0.50, 0.30);

    take.rest (0.60);
    return take;
}

// One held note while the model's three layers are brought in and out: the
// harmonic Core alone, then the noisy Air over it, then the inharmonic Bone
// modes, then all three at their learned balance.
Take renderCoreAirBone (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.orbit = 0.60f;
    parameters.evolutionRate = 0.7f;
    parameters.releaseSeconds = 1.2f;
    Take take (context.model, parameters);
    take.rest (0.10);

    take.noteOn (context.model.rootMidiNote(), 1.0f);
    take.rest (2.30);                    // Core alone
    parameters.air = 1.0f;
    take.setParameters (parameters);
    take.rest (2.30);                    // + Air
    parameters.air = 0.15f;
    parameters.bone = 1.0f;
    take.setParameters (parameters);
    take.rest (2.30);                    // + Bone, Air pulled back
    parameters.air = 1.0f;
    take.setParameters (parameters);
    take.rest (2.30);                    // everything at its learned level
    take.releaseAll();
    take.rest (1.40);
    return take;
}

// The same note with the learned resonant body - the Body-Locked Core
// envelope, the Air bands and the Bone modes - moved in frequency while the
// played pitch stays put.
Take renderFormantShift (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.releaseSeconds = 0.5f;
    Take take (context.model, parameters);
    take.rest (0.10);

    for (const float shift : { -12.0f, -5.0f, 0.0f, 5.0f, 12.0f })
    {
        parameters.formantShiftSemitones = shift;
        take.setParameters (parameters);
        take.play (context.model.rootMidiNote(), 0.95f, 0.85, 0.45);
    }

    take.rest (0.70);
    return take;
}

// A five-voice chord assembled note by note: Awaken fades each entry in,
// Horizon spreads the voices and decorrelates the Air layer, and Orbit keeps
// the held pad breathing inside its learned loop.
Take renderPolyphonicPad (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.air = 0.9f;
    parameters.bone = 0.6f;
    parameters.orbit = 0.65f;
    parameters.evolutionRate = 0.5f;
    parameters.attackSeconds = 1.4f;
    parameters.releaseSeconds = 3.0f;
    parameters.spread = 1.0f;
    parameters.outputGain = 0.22f;
    Take take (context.model, parameters);
    take.rest (0.10);

    const int root = context.model.rootMidiNote();
    for (const int offset : { -12, -5, 0, 3, 7 })
    {
        take.noteOn (root + offset, 0.85f);
        take.rest (0.40);
    }
    take.rest (5.00);
    take.releaseAll();
    take.rest (2.80);
    return take;
}

// The fitted stiff-string coefficient heard directly: the same note with the
// Stretch control forcing an ideal harmonic series, rendering the learned
// stretch, and exaggerating it to twice the fitted coefficient.
Take renderStiffStringStretch (const DemoContext& context)
{
    auto parameters = faithfulVoicing();
    parameters.bodyLock = 0.0f;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.orbit = 0.50f;
    parameters.releaseSeconds = 0.5f;
    Take take (context.model, parameters);
    take.rest (0.10);

    for (const float stretch : { 0.0f, 1.0f, 2.0f })
    {
        parameters.stretch = stretch;
        take.setParameters (parameters);
        take.play (context.model.rootMidiNote(), 1.0f, 1.70, 0.80);
    }

    take.rest (0.40);
    return take;
}

// ---------------------------------------------------------------------------
// The demo table
// ---------------------------------------------------------------------------

struct Demo
{
    const char* fileName;
    const char* description;
    Take (*render) (const DemoContext&);
};

const std::array<Demo, 8>& demos()
{
    static const std::array<Demo, 8> table {{
        { "01-training-tone.wav",
          "The procedurally synthesized tone the model is learned from, twice",
          renderTrainingTone },
        { "02-learned-vs-source.wav",
          "The training tone, then the fitted model replaying its root note",
          renderLearnedVersusSource },
        { "03-melody-across-octaves.wav",
          "A phrase across two octaves: every pitch but the root is extrapolated",
          renderMelody },
        { "04-velocity-touch.wav",
          "Five velocities with Touch at zero, then the same five fully open",
          renderVelocityTouch },
        { "05-core-air-bone.wav",
          "A held note as Core, Air and Bone are brought in and out",
          renderCoreAirBone },
        { "06-formant-shift.wav",
          "The learned resonant body moved while the played pitch stays put",
          renderFormantShift },
        { "07-polyphonic-pad.wav",
          "A five-voice pad with Awaken fades, Horizon width and Orbit looping",
          renderPolyphonicPad },
        { "08-stiff-string-stretch.wav",
          "The fitted inharmonicity forced off, as learned, then doubled",
          renderStiffStringStretch },
    }};
    return table;
}

// A short render used by the regression suite: it fits a shortened training
// tone and proves the learner and the engine still produce finite, audible
// audio and a readable WAV without committing anything. The full 2.4-second
// fit is deliberately avoided so the smoke test stays fast.
int runSmokeTest (const std::filesystem::path& directory)
{
    const auto source = makeTrainingTone (32000.0, 0.45);
    const auto learned = SampleLearner::learn (source, 32000.0);
    if (! learned)
    {
        std::fprintf (stderr, "smoke test: shortened fit failed: %s\n",
                      learned.error.c_str());
        return 1;
    }

    Take take (*learned.model, faithfulVoicing());
    take.play (learned.model->rootMidiNote(), 0.9f, 0.30, 0.25);

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

    std::printf ("Neuramar demo renderer smoke test passed (peak %.3f).\n", take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by NeuramarRenderDemos;"
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
            std::printf ("usage: NeuramarRenderDemos [--smoke] [output-directory]\n");
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

    // One fit, shared by every demo, exactly as the plug-in would perform it
    // on a dropped file. The learner is deterministic by construction - the
    // regression suite proves equal input produces byte-identical models - so
    // the whole render is reproducible bit for bit.
    const auto source = makeTrainingTone (trainingSampleRate, trainingSeconds);
    const auto learned = SampleLearner::learn (source, trainingSampleRate);
    if (! learned)
    {
        std::fprintf (stderr, "learning the training tone failed: %s\n",
                      learned.error.c_str());
        return 1;
    }
    const auto& model = *learned.model;

    std::printf ("Fitted the training tone: root %.2f Hz (MIDI %d, %+.1f cents), "
                 "confidence %.2f, stiff-string B %.2e, final loss %.4f\n",
                 static_cast<double> (model.rootFrequencyHz()), model.rootMidiNote(),
                 static_cast<double> (model.rootCents()),
                 static_cast<double> (model.pitchConfidence()),
                 static_cast<double> (model.inharmonicity()),
                 static_cast<double> (model.finalLoss()));

    // The manifest documents a learned A3 root and an audible Stretch demo, so
    // a fit that misses the root or reports a harmonic series is an error to
    // fix here, never something to render around.
    if (std::abs (model.rootFrequencyHz() / 220.0f - 1.0f) > 0.02f)
    {
        std::fprintf (stderr, "the fitted root drifted away from the tone's "
                              "220 Hz fundamental\n");
        return 1;
    }
    if (! (model.inharmonicity() > 0.0f))
    {
        std::fprintf (stderr, "the stiff partial series was fitted as perfectly "
                              "harmonic, so the Stretch demo would be silent "
                              "repetition\n");
        return 1;
    }

    // The audible "training input" in demos 01 and 02 is this same signal
    // through the learner's own band-limited resampler, so what the listener
    // compares against really is what the model was fitted from.
    const auto sourceAtDemoRate =
        SampleLearner::resampleForTests (source, trainingSampleRate, demoSampleRate);
    const DemoContext context { model, sourceAtDemoRate };

    std::vector<RenderedLevel> levels;

    for (const auto& demo : demos())
    {
        auto take = demo.render (context);

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
        // A take that reached full scale would be clipped by the 16-bit
        // encoder and would document the clamp rather than the model. The fix
        // is always to render that take quieter, never to ship it.
        if (renderedPeak >= 0.999)
        {
            std::fprintf (stderr,
                          "%s reached full scale (peak %.6f) and would clip the "
                          "16-bit encoding; render it at a lower gain\n",
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

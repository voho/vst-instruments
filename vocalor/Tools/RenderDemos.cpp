// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the plug-in runs, so a demonstration cannot drift away from
// what Vocalor actually sounds like. No samples, recordings or external
// processing are involved anywhere: every file here comes out of the
// source-filter model, including the room and the stereo image.

#include "DSP/VocalorMath.h"
#include "DSP/VoiceEngine.h"

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
using vocalor::ChordQuality;
using vocalor::EngineParameters;
using vocalor::PerformanceMode;
using vocalor::VoiceEngine;
using vocalor::VoiceProfile;
using vocalor::Vowel;

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

// Vocalor is a stereo instrument by construction - its singers stand across a
// stereo stage and the room is cross-coupled - so every take is written as a
// stereo file even when a solo voice happens to sit near the centre.
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
    // reads its metering and tract display through, which makes it neither
    // copyable nor movable; a take, on the other hand, is returned by value
    // from every score below.
    explicit Take (EngineParameters parameters)
        : engine_ (std::make_unique<VoiceEngine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize);
        engine_->setParameters (parameters);
        engine_->reset();
    }

    void setParameters (const EngineParameters& parameters)
    {
        engine_->setParameters (parameters);
    }

    void noteOn (int midiNote, float velocity) { engine_->noteOn (midiNote, velocity); }
    void noteOff (int midiNote) { engine_->noteOff (midiNote); }

    // Sing one note: attack, hold it, release it, then let the tail ring for
    // `gapSeconds` before returning.
    void note (int midiNote, float velocity, double holdSeconds, double gapSeconds)
    {
        engine_->noteOn (midiNote, velocity);
        render (holdSeconds);
        engine_->noteOff (midiNote);
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

    std::unique_ptr<VoiceEngine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// Spell out every setting so a change to EngineParameters' compatibility
// defaults cannot silently revoice the demonstrations. Starting from the
// published plug-in values, the neutral demo baseline deliberately backs
// Resonance down from 0.64 to 0.42, eases Tension from 0.36 to 0.32, leaves the
// published zero Vibrato in place, and exposes a natural 0.44 Drift. Output
// stays below the plug-in's own default because a twelve-singer room can sum
// far hotter than a solo voice;
// every take is normalised to a common level afterwards, so rendering quietly
// costs nothing.
EngineParameters demoVoicing()
{
    EngineParameters parameters;
    parameters.profile = VoiceProfile::Female;
    parameters.mode = PerformanceMode::Solo;
    parameters.vowel = Vowel::Aah;
    parameters.chordQuality = ChordQuality::Major;
    parameters.choirSize = 8;
    parameters.breath = 0.30f;
    parameters.resonance = 0.42f;
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.52f;
    parameters.spread = 0.62f;
    parameters.tension = 0.32f;
    parameters.room = 0.24f;
    parameters.outputGain = 0.40f;
    parameters.vowelX = 0.50f;
    parameters.vowelY = 0.50f;
    parameters.vowelMorph = 0.0f;
    parameters.formantShift = 0.0f;
    parameters.glide = 0.0f;
    parameters.legato = false;
    parameters.roomSize = 0.50f;
    parameters.dynamics = 1.0f;
    parameters.intonation = 0.0f;
    parameters.nasal = 0.0f;
    parameters.instability = 0.44f;
    return parameters;
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// A solo soprano phrase sung legato: each new key bends the sounding voice
// instead of re-attacking it, with a light glide carrying the pitch between
// notes and the held-note stack falling back down the phrase.
Take renderSoloLegatoPhrase()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Solo;
    parameters.legato = true;
    parameters.glide = 0.22f;
    parameters.vibrato = 0.50f;
    parameters.room = 0.30f;
    Take take (parameters);
    take.rest (0.15);

    // A D-minor phrase: up the scale to the fifth, back down, and home.
    struct Step { int midi; float velocity; double hold; };
    const std::array<Step, 8> phrase {{
        { 62, 0.62f, 0.95 }, { 65, 0.66f, 0.50 }, { 67, 0.70f, 0.50 },
        { 69, 0.78f, 1.40 }, { 67, 0.66f, 0.48 }, { 65, 0.62f, 0.48 },
        { 64, 0.60f, 0.95 }, { 62, 0.66f, 2.10 },
    }};

    int sounding = -1;
    for (const auto& step : phrase)
    {
        // Overlap the keys the way a player phrases legato: the new key goes
        // down before the old one comes up, which is what makes the engine
        // bend the sounding voice rather than start a new one.
        take.noteOn (step.midi, step.velocity);
        if (sounding >= 0)
            take.noteOff (sounding);
        sounding = step.midi;
        take.rest (step.hold);
    }
    take.noteOff (sounding);
    take.rest (3.0);
    return take;
}

// The three preset vowels that anchor the vowel space, first on the female
// voice and then on the male one an octave and a bit below.
Take renderVowelAnchors()
{
    auto parameters = demoVoicing();
    Take take (parameters);
    take.rest (0.12);

    for (const auto profile : { VoiceProfile::Female, VoiceProfile::Male })
    {
        parameters.profile = profile;
        const int midi = profile == VoiceProfile::Female ? 64 : 48;
        for (const auto vowel : { Vowel::Aah, Vowel::Ooh, Vowel::Uuh })
        {
            parameters.vowel = vowel;
            take.setParameters (parameters);
            take.note (midi, 0.72f, 1.05, 0.55);
        }
    }

    take.rest (1.6);
    return take;
}

// One held note while the morph target walks the pad through every cardinal
// vowel. The pitch never moves: everything that changes is the tract.
Take renderVowelSpaceMorph()
{
    auto parameters = demoVoicing();
    parameters.vibrato = 0.30f;
    Take take (parameters);
    take.rest (0.12);

    take.noteOn (57, 0.72f);
    take.rest (1.0); // the plain AAH anchor first

    // Engage the morph so the pad position takes over from the preset vowel.
    constexpr int engageSteps = 24;
    for (int step = 1; step <= engageSteps; ++step)
    {
        parameters.vowelMorph =
            static_cast<float> (step) / static_cast<float> (engageSteps);
        take.setParameters (parameters);
        take.rest (0.05);
    }

    // Walk the pad from the centre through the five cardinal vowels, using the
    // very positions the engine's own vowel space is anchored on.
    vocalor::VowelPoint from { parameters.vowelX, parameters.vowelY };
    constexpr int legSteps = 28;
    for (int cardinal = 0; cardinal < vocalor::kCardinalVowelCount; ++cardinal)
    {
        const auto to = vocalor::cardinalVowelPosition (cardinal);
        for (int step = 1; step <= legSteps; ++step)
        {
            const float mix = static_cast<float> (step) / static_cast<float> (legSteps);
            parameters.vowelX = from.x + (to.x - from.x) * mix;
            parameters.vowelY = from.y + (to.y - from.y) * mix;
            take.setParameters (parameters);
            take.rest (0.05);
        }
        take.rest (0.25); // sit on each vowel before moving on
        from = to;
    }

    take.noteOff (57);
    take.rest (2.6);
    return take;
}

// The whole vocal tract shifted an octave up and an octave down while the
// pitch stays put: a body-size control, not a pitch bend.
Take renderFormantShift()
{
    auto parameters = demoVoicing();
    parameters.vibrato = 0.35f;
    Take take (parameters);
    take.rest (0.12);

    take.noteOn (55, 0.74f);
    take.rest (1.1); // the unshifted tract first

    const auto sweepTo = [&] (float target, double seconds)
    {
        const float start = parameters.formantShift;
        constexpr int steps = 40;
        for (int step = 1; step <= steps; ++step)
        {
            parameters.formantShift =
                start + (target - start) * static_cast<float> (step)
                            / static_cast<float> (steps);
            take.setParameters (parameters);
            take.rest (seconds / steps);
        }
    };

    sweepTo (12.0f, 2.2);  // up to a child-sized tract
    take.rest (0.8);
    sweepTo (-12.0f, 3.2); // down through neutral to a giant
    take.rest (0.8);
    sweepTo (0.0f, 1.6);   // and home

    take.noteOff (55);
    take.rest (2.4);
    return take;
}

// Twelve independently humanised singers holding a chord pad: the ensemble
// drift, spread and room are what this file is about.
Take renderEnsemblePad()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Choir;
    parameters.profile = VoiceProfile::Male;
    parameters.vowel = Vowel::Ooh;
    parameters.choirSize = 12;
    parameters.spread = 0.85f;
    parameters.breath = 0.34f;
    parameters.vibrato = 0.35f;
    parameters.room = 0.50f;
    parameters.roomSize = 0.75f;
    Take take (parameters);
    take.rest (0.12);

    // A G-major pad stacked from the bottom up, each entry slightly behind
    // the previous one the way section singers actually come in.
    const std::array<int, 4> chord { 43, 50, 55, 59 };
    for (const auto midi : chord)
    {
        take.noteOn (midi, 0.62f);
        take.rest (0.22);
    }
    take.rest (5.6);
    for (const auto midi : chord)
    {
        take.noteOff (midi);
        take.rest (0.15);
    }

    take.rest (4.4);
    return take;
}

// Chord mode: every key is realised as six singers spread across the triad,
// so a one-finger line becomes a harmonised progression. The quality switch
// is audible halfway through.
Take renderChordModeHarmony()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Chord;
    parameters.room = 0.35f;
    Take take (parameters);
    take.rest (0.12);

    struct ChordStep { int root; ChordQuality quality; float velocity; double hold; };
    const std::array<ChordStep, 4> progression {{
        { 62, ChordQuality::Major, 0.68f, 1.55 },
        { 59, ChordQuality::Minor, 0.64f, 1.55 },
        { 55, ChordQuality::Major, 0.66f, 1.55 },
        { 57, ChordQuality::Major, 0.70f, 1.55 },
    }};

    for (const auto& step : progression)
    {
        parameters.chordQuality = step.quality;
        take.setParameters (parameters);
        take.note (step.root, step.velocity, step.hold, 0.20);
    }

    // The tonic to finish, held while the intonation control walks from equal
    // temperament to five-limit just. Nothing else moves: what changes is that
    // the third stops beating against the root as it narrows 13.7 cents.
    parameters.chordQuality = ChordQuality::Major;
    take.setParameters (parameters);
    take.noteOn (62, 0.72f);
    take.rest (1.3);
    for (int step = 1; step <= 12; ++step)
    {
        parameters.intonation = static_cast<float> (step) / 12.0f;
        take.setParameters (parameters);
        take.rest (0.12);
    }
    take.rest (2.2);
    take.noteOff (62);
    take.rest (3.0);
    return take;
}

// A short choir phrase sung from pianissimo to forte on note velocity, then a
// held fifth swelled with the Dynamics control. Neither is a volume knob: a
// soft note is duller as well as quieter, because both reach the glottal pulse
// and the aspiration balance rather than the output stage.
Take renderChoirDynamics()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Choir;
    parameters.choirSize = 8;
    parameters.room = 0.35f;
    Take take (parameters);
    take.rest (0.12);

    struct Step { int midi; float velocity; };
    const std::array<Step, 6> phrase {{
        { 57, 0.35f }, { 60, 0.44f }, { 64, 0.54f },
        { 65, 0.63f }, { 64, 0.72f }, { 62, 0.60f },
    }};
    for (const auto& step : phrase)
        take.note (step.midi, step.velocity, 0.66, 0.12);

    // The full choir lands on an open fifth and only Dynamics is automated.
    // The keys are struck once; Drift continues its natural pitch and vowel
    // micro-motion while the sweep changes level, source tilt, pulse shape and
    // breath balance.
    parameters.dynamics = 0.20f;
    take.setParameters (parameters);
    take.noteOn (57, 0.80f);
    take.noteOn (64, 0.78f);
    take.rest (0.5);
    for (int step = 1; step <= 16; ++step)
    {
        parameters.dynamics = 0.20f + 0.80f * static_cast<float> (step) / 16.0f;
        take.setParameters (parameters);
        take.rest (0.13);
    }
    take.rest (1.1);
    take.noteOff (57);
    take.noteOff (64);
    take.rest (3.0);
    return take;
}

// One held male note taken through the three most physical controls in the
// instrument: from a lax to a firmly adducted glottis, dissolved into breath,
// and finally closed into a hum. Tension crossfades between two analysed
// glottal pulse shapes and clusters the upper formants, breath is aspiration
// noise injected at the glottis and filtered by the same tract, and the velum
// adds the nasal branch's murmur pole and anti-resonance.
Take renderTensionAndBreath()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Solo;
    parameters.profile = VoiceProfile::Male;
    parameters.vibrato = 0.32f;
    parameters.breath = 0.05f;
    parameters.tension = 0.08f;
    Take take (parameters);
    take.rest (0.12);

    take.noteOn (45, 0.70f);
    take.rest (1.2); // the lax voice first

    const auto sweep = [&] (float EngineParameters::* field, float target,
                            double seconds)
    {
        const float start = parameters.*field;
        constexpr int steps = 48;
        for (int step = 1; step <= steps; ++step)
        {
            parameters.*field = start + (target - start) * static_cast<float> (step)
                                            / static_cast<float> (steps);
            take.setParameters (parameters);
            take.rest (seconds / steps);
        }
    };

    sweep (&EngineParameters::tension, 0.95f, 3.4); // lax through to pressed
    take.rest (0.8);
    sweep (&EngineParameters::tension, 0.45f, 1.0); // relax again
    sweep (&EngineParameters::breath, 0.95f, 3.0);  // and let the air take over
    take.rest (0.5);
    sweep (&EngineParameters::breath, 0.14f, 0.9);  // back off the air
    // ... and finally the velum, which turns the same held note into a hum:
    // a murmur pole at the nasal cavity's resonance and a notch where the
    // closed mouth loads the tract.
    sweep (&EngineParameters::nasal, 1.0f, 2.2);
    take.rest (1.4);

    take.noteOff (45);
    take.rest (2.4);
    return take;
}

// The same three-note motif sung twice: once in a small dry booth, once in a
// large hall. Room size moves the reflections apart and lengthens the tail;
// it is geometry, not a wet/dry fader alone.
Take renderRoomSmallAndLarge()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Solo;
    Take take (parameters);

    const auto motif = [&take]
    {
        take.note (64, 0.72f, 0.30, 0.40);
        take.note (62, 0.68f, 0.30, 0.40);
        take.note (57, 0.72f, 0.62, 0.0);
    };

    take.rest (0.12);

    parameters.room = 0.30f;
    parameters.roomSize = 0.05f;
    take.setParameters (parameters);
    motif();
    take.rest (1.8);

    parameters.room = 0.65f;
    parameters.roomSize = 0.95f;
    take.setParameters (parameters);
    motif();
    take.rest (5.5);
    return take;
}

// One connected soprano line through the register the other demonstrations do
// not reach. Below E-flat5 the upper reinforcement stays broad rather than
// collapsing into a tenor-style singer's formant; from there to B-flat5 the
// residual cluster releases back to the ordinary upper-vowel poles instead of
// preserving one narrow 3 kHz peak. This take demonstrates a continuous tract
// transition; it does not claim that every sparse high harmonic has equal level.
Take renderSopranoRegister()
{
    auto parameters = demoVoicing();
    parameters.mode = PerformanceMode::Solo;
    parameters.profile = VoiceProfile::Female;
    parameters.vowel = Vowel::Aah;
    parameters.legato = true;
    parameters.glide = 0.16f;
    parameters.vibrato = 0.40f;
    parameters.tension = 0.62f;
    parameters.breath = 0.24f;
    parameters.room = 0.16f;
    parameters.roomSize = 0.34f;
    Take take (parameters);
    take.rest (0.15);

    // A mostly stepwise C4-C6 ascent. E-flat5 through B-flat5 crosses the
    // measured cluster-release region while one continuously articulated
    // voice sounds, so a filter switch cannot hide between note attacks.
    constexpr std::array<int, 12> line {
        60, 62, 64, 67, 69, 72, 75, 77, 79, 80, 82, 84
    };
    int sounding = -1;
    for (std::size_t step = 0; step < line.size(); ++step)
    {
        take.noteOn(line[step], step < 6 ? 0.66f : 0.74f);
        if (sounding >= 0)
            take.noteOff(sounding);
        sounding = line[step];
        take.rest(step + 1 == line.size() ? 1.45 : 0.52);
    }
    take.noteOff(sounding);
    take.rest(2.8);
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
        { "01-solo-legato-phrase.wav",
          "A solo soprano phrase sung legato, with a light glide between notes",
          renderSoloLegatoPhrase },
        { "02-vowel-anchors.wav",
          "The three preset vowels on a less-resonant neutral female and male voice",
          renderVowelAnchors },
        { "03-vowel-space-morph.wav",
          "One held note while the morph target walks every cardinal vowel",
          renderVowelSpaceMorph },
        { "04-formant-shift.wav",
          "The whole tract shifted an octave up and down around one held MIDI note",
          renderFormantShift },
        { "05-ensemble-pad.wav",
          "Twelve independently humanised singers holding a chord pad",
          renderEnsemblePad },
        { "06-chord-mode-harmony.wav",
          "Single keys each rendered as a six-singer chord, major and minor",
          renderChordModeHarmony },
        { "07-choir-dynamics.wav",
          "An eight-singer choir phrase sung from pianissimo to forte",
          renderChoirDynamics },
        { "08-tension-and-breath.wav",
          "A held male note from a lax to a pressed glottis, then into breath",
          renderTensionAndBreath },
        { "09-room-small-and-large.wav",
          "The same motif sung in a dry booth and in a large hall",
          renderRoomSmallAndLarge },
        { "10-soprano-register.wav",
          "A connected soprano line from C4 to C6 through the cluster release",
          renderSopranoRegister },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    auto parameters = demoVoicing();
    Take take (parameters);
    take.note (60, 0.8f, 0.20, 0.05);
    parameters.mode = PerformanceMode::Chord;
    take.setParameters (parameters);
    take.note (57, 0.8f, 0.20, 0.05);

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

    std::printf ("Vocalor demo renderer smoke test passed (peak %.3f).\n", take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by VocalorRenderDemos;"
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
            std::printf ("usage: VocalorRenderDemos [--smoke] [output-directory]\n");
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
        // The demo gain staging is meant to leave generous headroom. A take
        // that reached full scale before normalisation is running the voices
        // far hotter than the engine's calibrated output stage; the fix is
        // always to render that take quieter, never to ship it.
        if (renderedPeak >= 0.999)
        {
            std::fprintf (stderr,
                          "%s reached full scale before normalisation (peak %.6f); "
                          "render it at a lower gain\n",
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

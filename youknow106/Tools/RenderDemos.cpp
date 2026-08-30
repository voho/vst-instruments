// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the plug-in runs, so a demonstration cannot drift away from
// what YouKnow106 actually sounds like. No samples or external processing are
// involved anywhere: every file here comes out of the circuit model, including
// the chorus hiss, which is part of the instrument.

#include "DSP/YouKnow106Engine.h"

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
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::EnvPolarity;
using youknow106::HighPassMode;
using youknow106::KeyMode;
using youknow106::PwmSource;
using youknow106::VcaMode;
using youknow106::YouKnow106Engine;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;
// These are file-delivery guards rather than synthesizer transfer values. A
// demo with more DC can waste headroom or thump downstream equipment, while a
// large non-zero endpoint can click when a player starts or stops the file.
constexpr double maximumAbsoluteDcMean = 0.005;
constexpr double maximumEdgeMagnitude = 0.01;

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian (std::vector<std::uint8_t>& bytes, std::uint32_t value,
                         int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back (static_cast<std::uint8_t> ((value >> (8 * index)) & 0xffu));
}

// YouKnow106 is a stereo instrument by construction - its two channels are the
// two chorus lines clocked in antiphase - so every take is written as a stereo
// file even when the effect is off and the image collapses.
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
    // The engine is held indirectly because it is large; a take, on the other
    // hand, is returned by value from every score below.
    explicit Take (EngineParameters parameters)
        : engine_ (std::make_unique<YouKnow106Engine>())
    {
        engine_->prepare (demoSampleRate, renderBlockSize, true);
        engine_->setParameters (parameters);
    }

    void setParameters (const EngineParameters& parameters)
    {
        engine_->setParameters (parameters);
    }

    void on (int note, float velocity = 1.0f) { engine_->noteOn (note, velocity); }
    void off (int note) { engine_->noteOff (note); }
    void bend (float amount) { engine_->setPitchBend (amount); }
    void wheel (float amount) { engine_->setModWheel (amount); }

    // Strike a key, hold it for `holdSeconds`, release it, then let it ring
    // for `gapSeconds` before returning.
    void hit (int note, float velocity, double holdSeconds, double gapSeconds)
    {
        on (note, velocity);
        rest (holdSeconds);
        off (note);
        rest (gapSeconds);
    }

    // A block chord: every note down together, held, then released together.
    void chord (std::initializer_list<int> notes, float velocity,
                double holdSeconds, double gapSeconds)
    {
        for (const int note : notes)
            on (note, velocity);
        rest (holdSeconds);
        for (const int note : notes)
            off (note);
        rest (gapSeconds);
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

    [[nodiscard]] double absoluteDcMean() const noexcept
    {
        if (left_.empty())
            return 0.0;

        double leftSum = 0.0;
        double rightSum = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            leftSum += static_cast<double> (left_[index]);
            rightSum += static_cast<double> (right_[index]);
        }
        const auto frames = static_cast<double> (left_.size());
        return std::max (std::abs (leftSum / frames),
                         std::abs (rightSum / frames));
    }

    [[nodiscard]] double firstEdgeMagnitude() const noexcept
    {
        if (left_.empty())
            return 0.0;
        return std::max (std::abs (static_cast<double> (left_.front())),
                         std::abs (static_cast<double> (right_.front())));
    }

    [[nodiscard]] double lastEdgeMagnitude() const noexcept
    {
        if (left_.empty())
            return 0.0;
        return std::max (std::abs (static_cast<double> (left_.back())),
                         std::abs (static_cast<double> (right_.back())));
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

    std::unique_ptr<YouKnow106Engine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// The panel every score starts from: a plain saw voice with the filter well
// open and no effects. VCA LEVEL sets the shared pre-chorus drive while main
// VOLUME follows the IC6 output summers. Every take is normalised to a common
// file level afterward; the unnormalised peak is retained in the table so a
// signal-path gain change remains visible during review. About 0.84 LEVEL is
// a useful multi-note working point with the exact 0.1 voice summer and IC6
// gain; the old unity voice sum needed misleading near-minimum settings.
EngineParameters plainPanel()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.62f;
    parameters.resonance = 0.10f;
    parameters.envDepth = 0.35f;
    parameters.keyFollow = 0.5f;
    parameters.attack = 0.04f;
    parameters.decay = 0.45f;
    parameters.sustain = 0.70f;
    parameters.release = 0.30f;
    parameters.vcaLevel = 0.84f;
    parameters.chorus = ChorusMode::Off;
    parameters.volume = 0.45f;
    return parameters;
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// The classic chorus pad: saw and sub through a half-open filter with a slow
// attack, the effect in mode I. The hiss is the delay lines' own.
Take renderChorusPad()
{
    auto parameters = plainPanel();
    parameters.subLevel = 0.7f;
    parameters.vcaLevel = 0.84f;
    parameters.cutoff = 0.48f;
    parameters.envDepth = 0.25f;
    parameters.attack = 0.35f;
    parameters.sustain = 0.85f;
    parameters.release = 0.55f;
    parameters.chorus = ChorusMode::One;
    Take take (parameters);

    take.rest (0.10);
    take.chord ({ 48, 55, 60, 64 }, 0.9f, 3.6, 1.2);
    take.chord ({ 46, 53, 58, 62 }, 0.9f, 3.6, 1.2);
    take.chord ({ 44, 51, 60, 63 }, 0.9f, 3.6, 1.0);
    take.chord ({ 43, 50, 59, 62 }, 0.9f, 4.2, 3.4);
    return take;
}

// Pulse-width-modulated strings in chorus II. B-2 computes PWM directly from
// the raw LFO accumulator; its onset delay applies only to pitch and filter.
Take renderPwmStrings()
{
    auto parameters = plainPanel();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = true;
    parameters.pwmSource = PwmSource::Lfo;
    parameters.pwmDepth = 0.75f;
    parameters.vcaLevel = 0.84f;
    parameters.lfoRate = 0.35f;
    parameters.cutoff = 0.55f;
    parameters.attack = 0.25f;
    parameters.sustain = 0.9f;
    parameters.release = 0.45f;
    parameters.chorus = ChorusMode::Two;
    Take take (parameters);

    take.rest (0.10);
    take.chord ({ 57, 60, 64 }, 0.9f, 3.0, 0.9);
    take.chord ({ 55, 59, 62 }, 0.9f, 3.0, 0.9);
    take.chord ({ 53, 57, 60, 65 }, 0.9f, 4.4, 3.0);
    return take;
}

// The 16' bass: saw and sub with a snappy filter envelope. The falling
// segments are the generator's own exponentials through the quasi-linear
// amplifier, which is where the punch of this instrument's envelopes lives.
Take renderBassline()
{
    auto parameters = plainPanel();
    parameters.range = DcoRange::Sixteen;
    parameters.vcaLevel = 0.84f;
    parameters.subLevel = 0.85f;
    parameters.cutoff = 0.34f;
    parameters.resonance = 0.30f;
    parameters.envDepth = 0.45f;
    parameters.attack = 0.0f;
    parameters.decay = 0.32f;
    parameters.sustain = 0.25f;
    parameters.release = 0.18f;
    Take take (parameters);

    take.rest (0.10);
    const std::array<std::pair<int, double>, 16> line {{
        { 48, 0.22 }, { 48, 0.22 }, { 60, 0.11 }, { 48, 0.22 },
        { 51, 0.22 }, { 48, 0.22 }, { 46, 0.11 }, { 48, 0.22 },
        { 48, 0.22 }, { 48, 0.22 }, { 60, 0.11 }, { 48, 0.22 },
        { 53, 0.22 }, { 51, 0.22 }, { 46, 0.11 }, { 44, 0.44 },
    }};
    for (int repeat = 0; repeat < 2; ++repeat)
        for (const auto& [note, gap] : line)
            take.hit (note, 0.95f, 0.16, gap);
    take.rest (2.0);
    return take;
}

// Brass stabs: the filter envelope working a resonant low-pass, with the
// bender pushed at the end of each phrase.
Take renderFilterBrass()
{
    auto parameters = plainPanel();
    parameters.subLevel = 0.4f;
    parameters.vcaLevel = 0.84f;
    parameters.cutoff = 0.30f;
    parameters.resonance = 0.42f;
    parameters.envDepth = 0.55f;
    parameters.attack = 0.02f;
    parameters.decay = 0.5f;
    parameters.sustain = 0.45f;
    parameters.release = 0.25f;
    Take take (parameters);

    take.rest (0.10);
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        take.chord ({ 50, 57, 62, 66 }, 0.95f, 0.28, 0.16);
        take.chord ({ 50, 57, 62, 66 }, 0.85f, 0.20, 0.24);
        take.chord ({ 48, 55, 60, 64 }, 0.95f, 0.28, 0.16);
        take.chord ({ 48, 55, 60, 64 }, 0.85f, 0.20, 0.24);
        take.chord ({ 46, 53, 58, 62 }, 0.95f, 0.55, 0.35);
    }
    for (const int note : { 50, 57, 62, 66 })
        take.on (note, 1.0f);
    take.rest (0.8);
    take.bend (1.0f);
    take.rest (0.9);
    take.bend (0.0f);
    take.rest (1.0);
    for (const int note : { 50, 57, 62, 66 })
        take.off (note);
    take.rest (2.2);
    return take;
}

// The filter played as a voice: resonance past the threshold with full key
// follow, no oscillator running. What sounds is the four-pole cascade's own
// limit cycle, landing on the control law's pitch as the service procedure
// trims it to.
Take renderSelfOscillation()
{
    auto parameters = plainPanel();
    parameters.sawEnabled = false;
    parameters.vcaLevel = 1.0f;
    parameters.cutoff = 0.34f;
    parameters.resonance = 1.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 1.0f;
    parameters.attack = 0.01f;
    parameters.decay = 0.4f;
    parameters.sustain = 1.0f;
    parameters.release = 0.45f;
    // One voice, reused for every note: the limit cycle takes over a second
    // to grow out of the transconductors' own noise, so the take establishes
    // it once and then keeps the same filter ringing across the whole
    // melody, exactly as a player leaning on the resonance does.
    parameters.polyphony = 1;
    parameters.volume = 0.7f;
    Take take (parameters);

    take.rest (0.3);
    take.hit (48, 0.9f, 2.6, 0.05);
    for (const int note : { 55, 60, 64, 67, 72, 67, 64, 60, 55 })
        take.hit (note, 0.9f, 0.55, 0.05);
    take.hit (48, 0.9f, 2.0, 2.6);
    return take;
}

// One pad through the effect's three states: off, I, II. Mode II is faster,
// not deeper, and the lines' own hiss rides underneath both.
Take renderChorusModes()
{
    auto parameters = plainPanel();
    parameters.subLevel = 0.6f;
    parameters.vcaLevel = 0.84f;
    parameters.cutoff = 0.52f;
    parameters.attack = 0.12f;
    parameters.sustain = 0.9f;
    parameters.release = 0.35f;
    Take take (parameters);

    take.rest (0.10);
    for (const ChorusMode mode :
         { ChorusMode::Off, ChorusMode::One, ChorusMode::Two })
    {
        parameters.chorus = mode;
        take.setParameters (parameters);
        take.chord ({ 52, 59, 64, 68 }, 0.9f, 3.4, 1.3);
    }
    take.rest (1.6);
    return take;
}

// Unison with portamento: six voices on one key with no pitch spread at all -
// six timers dividing one reference cannot disagree - so what thickens the
// note is the analogue block and the effect, and the glide runs at a constant
// rate in pitch, taking proportionally longer over the wider leaps.
Take renderUnisonLead()
{
    auto parameters = plainPanel();
    parameters.keyMode = KeyMode::Unison;
    parameters.vcaLevel = 0.84f;
    parameters.subLevel = 0.5f;
    parameters.cutoff = 0.45f;
    parameters.envDepth = 0.4f;
    parameters.portamento = 0.42f;
    parameters.attack = 0.01f;
    parameters.decay = 0.55f;
    parameters.sustain = 0.7f;
    parameters.release = 0.3f;
    parameters.chorus = ChorusMode::One;
    parameters.volume = 0.35f;
    Take take (parameters);

    take.rest (0.10);
    for (const auto& [note, hold] :
         std::initializer_list<std::pair<int, double>> {
             { 45, 0.5 }, { 52, 0.5 }, { 57, 0.5 }, { 60, 0.9 },
             { 57, 0.45 }, { 55, 0.45 }, { 57, 1.4 }, { 69, 1.6 }, { 45, 2.2 } })
        take.hit (note, 0.95f, hold, 0.06);
    take.rest (2.4);
    return take;
}

// The modulator's delayed onset: a held chord whose vibrato fades in on the
// two-stage hold-then-ramp envelope, stepping at the scan rate as the
// firmware's staircase does.
Take renderDelayedVibrato()
{
    auto parameters = plainPanel();
    parameters.vcaLevel = 0.84f;
    parameters.subLevel = 0.5f;
    parameters.cutoff = 0.5f;
    parameters.dcoLfoDepth = 0.45f;
    parameters.lfoRate = 0.5f;
    parameters.lfoDelay = 0.72f;
    parameters.attack = 0.15f;
    parameters.sustain = 0.9f;
    parameters.release = 0.4f;
    Take take (parameters);

    take.rest (0.10);
    take.chord ({ 53, 60, 65, 69 }, 0.9f, 7.5, 2.0);
    return take;
}

// The four-position high-pass switch on one bright chord: the measured bass
// boost, the flat leg, and the two cutting corners.
Take renderHighPassLadder()
{
    auto parameters = plainPanel();
    parameters.subLevel = 0.8f;
    parameters.vcaLevel = 0.84f;
    parameters.cutoff = 0.66f;
    parameters.attack = 0.02f;
    parameters.sustain = 0.9f;
    parameters.release = 0.25f;
    Take take (parameters);

    take.rest (0.10);
    for (const HighPassMode mode :
         { HighPassMode::Boost, HighPassMode::One, HighPassMode::Two,
           HighPassMode::Three })
    {
        parameters.highPass = mode;
        take.setParameters (parameters);
        take.chord ({ 36, 48, 55, 60, 64 }, 0.9f, 1.7, 0.55);
    }
    take.rest (1.5);
    return take;
}

// One chord at the nominal zero-character baseline, then with the optional
// voiced Unit Character profile at full amount. The latter is a repeatable
// sound-design policy, not a measured worn-unit population.
Take renderUnitCharacter()
{
    auto parameters = plainPanel();
    parameters.subLevel = 0.55f;
    parameters.vcaLevel = 0.84f;
    parameters.cutoff = 0.42f;
    parameters.resonance = 0.55f;
    parameters.envDepth = 0.3f;
    parameters.attack = 0.08f;
    parameters.sustain = 0.85f;
    parameters.release = 0.4f;
    Take take (parameters);

    take.rest (0.10);
    for (const float calibration : { 0.0f, 1.0f })
    {
        parameters.calibration = calibration;
        take.setParameters (parameters);
        take.chord ({ 45, 52, 57, 61, 64, 69 }, 0.9f, 4.2, 1.3);
    }
    take.rest (1.8);
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
        { "01-chorus-pad.wav",
          "Saw and sub through the mode-I chorus: the classic pad, hiss and all",
          renderChorusPad },
        { "02-pwm-strings.wav",
          "Pulse-width-modulated strings in the faster mode-II chorus",
          renderPwmStrings },
        { "03-sixteen-foot-bass.wav",
          "A 16' bassline: the exponential envelope segments doing the punch",
          renderBassline },
        { "04-filter-brass.wav",
          "Resonant filter-envelope stabs, ending on a full bender push",
          renderFilterBrass },
        { "05-self-oscillation.wav",
          "The filter played as a voice at full resonance and key follow",
          renderSelfOscillation },
        { "06-chorus-modes.wav",
          "The same pad with the effect off, in mode I, then in mode II",
          renderChorusModes },
        { "07-unison-glide.wav",
          "Six-voice unison lead with constant-rate portamento",
          renderUnisonLead },
        { "08-delayed-vibrato.wav",
          "The modulator's two-stage delay fading vibrato onto a held chord",
          renderDelayedVibrato },
        { "09-high-pass-ladder.wav",
          "One bright chord through all four high-pass switch positions",
          renderHighPassLadder },
        { "10-unit-character.wav",
          "A six-voice chord at nominal zero Unit Character, then at full amount",
          renderUnitCharacter },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest (const std::filesystem::path& directory)
{
    auto parameters = plainPanel();
    parameters.attack = 0.0f;
    Take take (parameters);
    take.hit (60, 0.95f, 0.30, 0.25);

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

    std::printf ("YouKnow106 demo renderer smoke test passed (peak %.3f).\n",
                 take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by YouKnow106RenderDemos;"
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
            std::printf ("usage: YouKnow106RenderDemos [--smoke] [output-directory]\n");
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
        const auto gain = take.normalise();
        const auto absoluteDcMean = take.absoluteDcMean();
        const auto firstEdge = take.firstEdgeMagnitude();
        const auto lastEdge = take.lastEdgeMagnitude();
        if (absoluteDcMean > maximumAbsoluteDcMean)
        {
            std::fprintf (stderr,
                          "%s rejected: normalised absolute DC mean %.6f FS "
                          "exceeds %.6f FS\n",
                          demo.fileName, absoluteDcMean, maximumAbsoluteDcMean);
            return 1;
        }
        if (firstEdge > maximumEdgeMagnitude || lastEdge > maximumEdgeMagnitude)
        {
            std::fprintf (stderr,
                          "%s rejected: normalised first/last edge "
                          "%.6f/%.6f FS exceeds %.6f FS\n",
                          demo.fileName, firstEdge, lastEdge,
                          maximumEdgeMagnitude);
            return 1;
        }

        const auto path = directory / demo.fileName;
        if (! writeWav (path, take.left(), take.right()))
        {
            std::fprintf (stderr, "could not write %s\n", path.string().c_str());
            return 1;
        }

        RenderedLevel level;
        level.fileName = demo.fileName;
        level.description = demo.description;
        level.seconds = static_cast<double> (take.left().size()) / demoSampleRate;
        level.renderedPeakDb = 20.0 * std::log10 (renderedPeak);
        level.normalisationDb = 20.0 * std::log10 (gain);
        levels.push_back (std::move (level));

        std::printf ("Rendered %-28s peak %6.3f, %5.1f s\n", demo.fileName,
                     renderedPeak, level.seconds);
    }

    if (! updatePeaksTable (directory, levels))
        return 1;

    std::printf ("Rendered %zu demos into %s\n", demos().size(),
                 directory.string().c_str());
    return 0;
}

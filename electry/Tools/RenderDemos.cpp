// Renders the committed demonstration WAVs under Docs/audio.
//
// Everything here drives the shipping JUCE-free path - ElectryEngine into
// ElectryFx - so a demo cannot drift away from what the plug-in produces, and
// the whole set is reproducible on any platform with a C++20 toolchain:
//
//   cmake --build <build-dir> --target ElectryRenderDemos
//   <build-dir>/ElectryRenderDemos electry/Docs/audio
//
// The engine is deterministic, so re-rendering an unchanged model reproduces
// byte-identical files.
#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
using electry::Articulation;
using electry::ElectryEngine;
using electry::ElectryFx;
using electry::EngineParameters;
using electry::FxParameters;
using electry::OutputMode;
using electry::PickupSelector;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;

int keyswitchFor(Articulation articulation)
{
    return ElectryEngine::firstKeyswitchNote + static_cast<int>(articulation);
}

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value,
                        int byteCount)
{
    for (int i = 0; i < byteCount; ++i)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu));
}

// Mono is exact dual mono in this instrument - the engine renders one channel
// and mirrors it - so writing a single channel for a mono take is lossless
// rather than a downmix, and halves what the repository carries.
bool writeWav(const std::filesystem::path& path,
              const std::vector<float>& left,
              const std::vector<float>& right, bool stereo)
{
    const auto frames = static_cast<std::uint32_t>(left.size());
    const std::uint16_t channels = stereo ? 2u : 1u;
    const std::uint32_t byteRate = static_cast<std::uint32_t>(demoSampleRate)
                                 * channels * 2u;
    const std::uint32_t dataBytes = frames * channels * 2u;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44u + dataBytes);
    const auto tag = [&bytes] (const char* text)
    {
        for (int i = 0; i < 4; ++i)
            bytes.push_back(static_cast<std::uint8_t>(text[i]));
    };
    tag("RIFF");
    appendLittleEndian(bytes, 36u + dataBytes, 4);
    tag("WAVE");
    tag("fmt ");
    appendLittleEndian(bytes, 16u, 4);
    appendLittleEndian(bytes, 1u, 2);            // PCM
    appendLittleEndian(bytes, channels, 2);
    appendLittleEndian(bytes, static_cast<std::uint32_t>(demoSampleRate), 4);
    appendLittleEndian(bytes, byteRate, 4);
    appendLittleEndian(bytes, channels * 2u, 2); // block align
    appendLittleEndian(bytes, 16u, 2);           // bits per sample
    tag("data");
    appendLittleEndian(bytes, dataBytes, 4);

    const auto encode = [] (float value)
    {
        if (! std::isfinite(value))
            value = 0.0f;
        const float clamped = std::clamp(value, -1.0f, 1.0f);
        // Symmetric scaling by 32767 so a full-scale negative peak cannot wrap.
        const auto sample = static_cast<std::int32_t>(
            std::lround(static_cast<double>(clamped) * 32767.0));
        return static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(static_cast<std::int16_t>(sample)));
    };
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        appendLittleEndian(bytes, encode(left[frame]), 2);
        if (stereo)
            appendLittleEndian(bytes, encode(right[frame]), 2);
    }

    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file)
                       == bytes.size();
    std::fclose(file);
    return written;
}

// ---------------------------------------------------------------------------
// A take: an imperative score driving the engine and the effect chain
// ---------------------------------------------------------------------------

class Take
{
public:
    Take(EngineParameters engineParameters, FxParameters fxParameters,
         bool stereo)
        : stereo_(stereo)
    {
        engine_.prepare(demoSampleRate, renderBlockSize);
        engine_.setParameters(engineParameters);
        engine_.reset();
        effects_.prepare(demoSampleRate);
        effects_.setParameters(fxParameters);
        effects_.reset();
        // A short lead-in lets the engine's continuous-parameter smoothers
        // reach the take's settings before the first note, exactly as a host
        // would after loading a preset.
        wait(0.25);
    }

    void setEngineParameters(const EngineParameters& parameters)
    {
        engine_.setParameters(parameters);
    }

    void setFxParameters(const FxParameters& parameters)
    {
        effects_.setParameters(parameters);
    }

    void style(Articulation articulation)
    {
        engine_.noteOn(keyswitchFor(articulation), 1.0f);
    }

    void noteOn(int note, float velocity) { engine_.noteOn(note, velocity); }
    void noteOff(int note) { engine_.noteOff(note); }
    void pitchBend(float bipolar) { engine_.setPitchBend(bipolar); }
    void vibrato(float amount) { engine_.setVibratoAmount(amount); }
    void palmMutePressure(float amount) { engine_.setPalmMutePressure(amount); }
    void sustain(bool down) { engine_.setSustainPedal(down); }

    void chord(std::initializer_list<int> notes, float velocity)
    {
        for (const int note : notes)
            engine_.noteOn(note, velocity);
    }

    void releaseChord(std::initializer_list<int> notes)
    {
        for (const int note : notes)
            engine_.noteOff(note);
    }

    // One note held for `holdSeconds`, then `gapSeconds` of silence after the
    // release. The gap is where a real player's release noise and ring-out
    // live, so it is part of the demonstration rather than padding.
    void pluck(int note, float velocity, double holdSeconds,
               double gapSeconds = 0.0)
    {
        noteOn(note, velocity);
        wait(holdSeconds);
        noteOff(note);
        if (gapSeconds > 0.0)
            wait(gapSeconds);
    }

    void wait(double seconds)
    {
        int remaining = static_cast<int>(seconds * demoSampleRate);
        while (remaining > 0)
        {
            const int samples = std::min(renderBlockSize, remaining);
            const auto offset = left_.size();
            left_.resize(offset + static_cast<std::size_t>(samples));
            right_.resize(offset + static_cast<std::size_t>(samples));
            engine_.process(left_.data() + offset, right_.data() + offset,
                            samples);
            effects_.process(left_.data() + offset, right_.data() + offset,
                             samples);
            remaining -= samples;
        }
    }

    void applyGain(float gain) noexcept
    {
        for (auto& sample : left_)
            sample *= gain;
        for (auto& sample : right_)
            sample *= gain;
    }

    [[nodiscard]] bool stereo() const noexcept { return stereo_; }
    [[nodiscard]] const std::vector<float>& left() const noexcept { return left_; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return right_; }

    [[nodiscard]] double peak() const noexcept
    {
        double result = 0.0;
        for (std::size_t i = 0; i < left_.size(); ++i)
        {
            result = std::max(result, std::abs(static_cast<double>(left_[i])));
            if (stereo_)
                result = std::max(result,
                                  std::abs(static_cast<double>(right_[i])));
        }
        return result;
    }

    [[nodiscard]] bool channelsIdentical() const noexcept
    {
        return left_.size() == right_.size()
            && std::memcmp(left_.data(), right_.data(),
                           left_.size() * sizeof(float)) == 0;
    }

    [[nodiscard]] bool finite() const noexcept
    {
        const auto ok = [] (const std::vector<float>& channel)
        {
            return std::all_of(channel.begin(), channel.end(),
                               [] (float value) { return std::isfinite(value); });
        };
        return ok(left_) && ok(right_);
    }

private:
    ElectryEngine engine_;
    ElectryFx effects_;
    bool stereo_ { false };
    std::vector<float> left_;
    std::vector<float> right_;
};

// The eight open strings of the Drop-E instrument.
constexpr std::array<int, ElectryEngine::stringCount> openStrings {
    28, 35, 40, 45, 50, 55, 59, 64
};

// ---------------------------------------------------------------------------
// Demonstrations
// ---------------------------------------------------------------------------

Take renderOpenStrings()
{
    // The instrument's physical range, one open string at a time from the
    // Drop-E eighth string to the high E, then the whole set ringing together.
    Take take(EngineParameters {}, FxParameters {}, false);
    for (const int note : openStrings)
        take.pluck(note, 0.85f, 0.45, 0.05);
    take.chord({ 28, 35, 40, 45, 50, 55, 59, 64 }, 0.9f);
    take.wait(1.6);
    take.releaseChord({ 28, 35, 40, 45, 50, 55, 59, 64 });
    take.wait(0.4);
    return take;
}

Take renderFullFretboard()
{
    // Every playable note, 28 to 86: five and a half octaves across eight
    // strings and twenty-two frets, with the string allocator choosing where
    // each one lands.
    EngineParameters parameters;
    parameters.pickPosition = 0.30f;
    Take take(parameters, FxParameters {}, false);
    for (int note = ElectryEngine::lowestPlayableNote;
         note <= ElectryEngine::highestPlayableNote; ++note)
        take.pluck(note, 0.8f, 0.052);
    take.wait(0.8);
    return take;
}

Take renderPlayStyles()
{
    // All sixteen latching play styles on the same two notes, in keyswitch
    // order, so their attacks and decays can be compared directly.
    Take take(EngineParameters {}, FxParameters {}, false);
    for (int index = 0; index < ElectryEngine::keyswitchCount; ++index)
    {
        take.style(static_cast<Articulation>(index));
        const auto articulation = static_cast<Articulation>(index);
        // Hammer-on needs a sounding string to continue, and tremolo needs the
        // note held long enough to hear the repeated strokes.
        if (articulation == Articulation::HammerOn)
        {
            take.style(Articulation::Downstroke);
            take.noteOn(40, 0.9f);
            take.wait(0.16);
            take.style(Articulation::HammerOn);
            take.noteOn(45, 0.8f);
            take.wait(0.34);
            take.noteOff(45);
            take.noteOff(40);
            take.wait(0.04);
        }
        else if (articulation == Articulation::Tremolo)
        {
            take.pluck(40, 0.9f, 0.48, 0.06);
        }
        else
        {
            take.pluck(40, 0.95f, 0.28, 0.04);
            take.pluck(52, 0.95f, 0.18, 0.04);
        }
    }
    take.wait(0.7);
    return take;
}

EngineParameters metalRhythmVoicing()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickupType = 0.32f;   // toward a hot humbucker
    parameters.toneKnob = 1.0f;
    parameters.scaleLength = 0.85f;  // long baritone build
    parameters.stringGauge = 0.8f;   // heavy set
    parameters.stringAge = 0.10f;
    parameters.pickHardness = 0.85f;
    parameters.pickPosition = 0.18f; // close to the bridge
    parameters.velocityAmount = 0.7f;
    parameters.sympatheticAmount = 0.25f;
    // A single chugged low note is a quiet signal next to a full eight-string
    // chord, and the default output level leaves headroom for the chord. A
    // player tracking a rhythm part opens the output control instead.
    parameters.outputGain = 2.0f;
    return parameters;
}

// One bar of a Drop-E rhythm figure, used dry and again through the amplifier
// so the two can be compared on identical MIDI.
void playRhythmFigure(Take& take)
{
    struct Hit { int note; double length; float velocity; bool open; };
    static const std::array<Hit, 16> bar {{
        { 28, 0.150, 1.00f, false }, { 28, 0.150, 0.82f, false },
        { 28, 0.150, 0.86f, false }, { 31, 0.150, 0.95f, false },
        { 28, 0.150, 0.80f, false }, { 28, 0.150, 0.84f, false },
        { 33, 0.300, 1.00f, true },  { 28, 0.150, 0.82f, false },
        { 28, 0.150, 0.88f, false }, { 30, 0.150, 0.95f, false },
        { 28, 0.150, 0.80f, false }, { 28, 0.150, 0.86f, false },
        { 35, 0.150, 0.95f, false }, { 28, 0.150, 0.82f, false },
        { 33, 0.300, 1.00f, true },  { 28, 0.450, 1.00f, true },
    }};

    take.style(Articulation::Chug);
    for (const auto& hit : bar)
    {
        // The open accents lift the bridge hand off the strings; everything
        // else is chugged.
        take.style(hit.open ? Articulation::Downstroke : Articulation::Chug);
        take.pluck(hit.note, hit.velocity, hit.length * 0.92,
                   hit.length * 0.08);
    }
}

Take renderRhythmDry()
{
    Take take(metalRhythmVoicing(), FxParameters {}, false);
    playRhythmFigure(take);
    take.wait(0.12);
    playRhythmFigure(take);
    take.wait(1.1);
    return take;
}

Take renderRhythmAmped()
{
    // The same figure through the oversampled gain stage and the modelled
    // cabinet, with the rhythm compressor levelling the part.
    FxParameters fx;
    fx.distortion = 0.45f;
    fx.amp = 0.95f;
    fx.compressor = 0.60f;
    Take take(metalRhythmVoicing(), fx, false);
    playRhythmFigure(take);
    take.wait(0.12);
    playRhythmFigure(take);
    take.wait(1.1);
    return take;
}

Take renderLeadThroughAmp()
{
    // A lead phrase: picked notes, both bend programs, the CC1 vibrato and
    // tremolo picking, through the amplifier, the lead delay and the room.
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickupType = 0.4f;
    parameters.toneKnob = 0.9f;
    parameters.bodyResonance = 0.45f;
    parameters.stringAge = 0.08f;
    parameters.pickPosition = 0.30f;
    parameters.vibratoDepthCents = 55.0f;
    parameters.bendTimeSeconds = 0.20f;
    parameters.sympatheticAmount = 0.35f;
    parameters.outputGain = 1.5f;
    parameters.outputMode = OutputMode::Stereo;

    FxParameters fx;
    fx.distortion = 0.30f;
    fx.amp = 0.82f;
    fx.compressor = 0.35f;
    fx.delay = 0.30f;
    fx.room = 0.35f;

    Take take(parameters, fx, true);
    take.style(Articulation::Downstroke);
    take.pluck(64, 0.9f, 0.28, 0.02);
    take.pluck(67, 0.9f, 0.28, 0.02);
    take.style(Articulation::HammerOn);
    take.noteOn(69, 0.85f);
    take.wait(0.30);
    take.noteOff(69);

    take.style(Articulation::Bend1Up);
    take.noteOn(71, 0.95f);
    take.wait(0.55);
    take.vibrato(0.85f);
    take.wait(0.75);
    take.vibrato(0.0f);
    take.noteOff(71);
    take.wait(0.10);

    take.style(Articulation::Bend2Down);
    take.pluck(74, 0.95f, 0.60, 0.05);

    take.style(Articulation::Tremolo);
    take.noteOn(76, 0.9f);
    take.wait(0.85);
    take.noteOff(76);

    take.style(Articulation::PinchHarmonic);
    take.noteOn(69, 1.0f);
    take.wait(0.9);
    take.vibrato(0.7f);
    take.wait(0.6);
    take.vibrato(0.0f);
    take.noteOff(69);
    take.wait(1.9);
    return take;
}

Take renderPickupsAndTone()
{
    // One phrase, three selector positions, then the guitar's own tone control
    // closing from wide open to fully rolled off.
    EngineParameters parameters;
    parameters.stringAge = 0.10f;
    parameters.pickPosition = 0.28f;
    Take take(parameters, FxParameters {}, false);

    const auto phrase = [&take] (float velocity)
    {
        take.pluck(28, velocity, 0.30, 0.02);
        take.pluck(45, velocity, 0.24, 0.02);
        take.chord({ 40, 47, 52 }, velocity);
        take.wait(0.62);
        take.releaseChord({ 40, 47, 52 });
        take.wait(0.10);
    };

    for (const auto selector : { PickupSelector::Neck, PickupSelector::Both,
                                 PickupSelector::Bridge })
    {
        parameters.pickupSelector = selector;
        take.setEngineParameters(parameters);
        take.wait(0.10);
        phrase(0.85f);
    }

    parameters.pickupSelector = PickupSelector::Bridge;
    for (const float tone : { 1.0f, 0.6f, 0.25f, 0.0f })
    {
        parameters.toneKnob = tone;
        take.setEngineParameters(parameters);
        take.wait(0.08);
        take.pluck(52, 0.9f, 0.34, 0.04);
    }
    take.wait(0.7);
    return take;
}

Take renderSympatheticStrum()
{
    // The divided-pickup stereo field, the strum travel and the bridge-coupled
    // sympathetic strings: a strummed chord, then the same chord with the
    // coupling switched off, then a single low note whose bridge force sets the
    // unfingered strings ringing.
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Both;
    parameters.outputMode = OutputMode::Stereo;
    parameters.strumSpreadSeconds = 0.022f;
    parameters.sympatheticAmount = 0.85f;
    parameters.bodyResonance = 0.55f;
    parameters.stringAge = 0.05f;
    parameters.toneKnob = 1.0f;

    Take take(parameters, FxParameters {}, true);
    take.style(Articulation::Downstroke);
    take.chord({ 28, 35, 40, 47, 52, 56 }, 0.9f);
    take.wait(1.30);
    take.style(Articulation::Upstroke);
    take.chord({ 28, 35, 40, 47, 52, 56 }, 0.75f);
    take.wait(1.30);
    take.releaseChord({ 28, 35, 40, 47, 52, 56 });
    take.wait(0.35);

    parameters.sympatheticAmount = 0.0f;
    take.setEngineParameters(parameters);
    take.wait(0.15);
    take.style(Articulation::Downstroke);
    take.chord({ 28, 35, 40, 47, 52, 56 }, 0.9f);
    take.wait(1.20);
    take.releaseChord({ 28, 35, 40, 47, 52, 56 });
    take.wait(0.30);

    parameters.sympatheticAmount = 0.95f;
    take.setEngineParameters(parameters);
    take.wait(0.15);
    take.pluck(28, 1.0f, 1.40, 0.90);
    return take;
}

Take renderBuildContrasts()
{
    // The same two-bar figure across the construction axes: a conventional
    // A 25.5-inch light-string build, the shipped default instrument, and a
    // 28-inch baritone with a heavy set and old strings.
    //
    // The middle entry is read from EngineParameters rather than written out,
    // because the point of this take is to place the default between two
    // deliberate extremes. It had been a copy of the old midpoints, so when the
    // defaults became a thick blank with the heaviest set this segment quietly
    // went on rendering an instrument the plug-in no longer ships - a take whose
    // whole subject is the default, no longer containing it.
    struct Build { const char* name; float scale; float gauge; float age;
                   float wood; float size; float shape; float construction; };
    const EngineParameters shipped {};
    const std::array<Build, 3> builds {{
        { "short scale, light, fresh", 0.0f, 0.15f, 0.05f, 0.2f, 0.7f, 0.2f, 0.15f },
        { "default build",             shipped.scaleLength, shipped.stringGauge,
                                       shipped.stringAge, shipped.bodyWood,
                                       shipped.bodySize, shipped.bodyShape,
                                       shipped.construction },
        { "28-inch baritone, heavy",   1.0f, 0.95f, 0.55f, 0.8f, 0.2f, 0.9f, 0.85f },
    }};

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickPosition = 0.22f;
    parameters.pickHardness = 0.8f;
    parameters.bodyResonance = 0.5f;
    parameters.outputGain = 1.2f;

    Take take(parameters, FxParameters {}, false);
    for (const auto& build : builds)
    {
        parameters.scaleLength = build.scale;
        parameters.stringGauge = build.gauge;
        parameters.stringAge = build.age;
        parameters.bodyWood = build.wood;
        parameters.bodySize = build.size;
        parameters.bodyShape = build.shape;
        parameters.construction = build.construction;
        take.setEngineParameters(parameters);
        // Long enough for the smoothers to settle and for the new voicing to
        // reach the ringing strings.
        take.wait(0.25);

        take.style(Articulation::Downstroke);
        take.pluck(28, 0.95f, 0.42, 0.03);
        take.pluck(33, 0.90f, 0.30, 0.03);
        take.pluck(40, 0.90f, 0.30, 0.03);
        take.chord({ 28, 40, 47 }, 0.95f);
        take.wait(0.80);
        take.releaseChord({ 28, 40, 47 });
        take.wait(0.12);
    }
    take.wait(0.6);
    return take;
}

Take renderVelocityDynamics()
{
    // The velocity response at full travel: the same string from a finger-light
    // touch to a hard metal attack, then the same ramp palm-muted, so the
    // level, brightness, contact noise and pitch glide can all be heard moving
    // together.
    EngineParameters parameters;
    parameters.velocityAmount = 1.0f;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickHardness = 0.7f;
    parameters.artifactAmount = 0.30f;
    parameters.outputGain = 2.0f;

    Take take(parameters, FxParameters {}, false);
    take.style(Articulation::Downstroke);
    for (const float velocity : { 0.12f, 0.30f, 0.50f, 0.70f, 0.85f, 1.00f })
        take.pluck(33, velocity, 0.30, 0.04);

    take.wait(0.25);
    parameters.palmMute = 0.60f;
    take.setEngineParameters(parameters);
    take.wait(0.20);
    for (const float velocity : { 0.20f, 0.50f, 0.80f, 1.00f })
        take.pluck(28, velocity, 0.22, 0.04);
    take.wait(0.6);
    return take;
}


// A power chord is a root, its fifth and its octave struck as one stroke. It is
// the shape a metal rhythm part is actually built from, and it loads the model
// very differently from a single note: three strings share one bridge, so the
// sympathetic coupling, the strum travel and the amplifier's intermodulation
// all have something to work with.
//
// The roots stay inside MIDI 28..32 deliberately. The allocator gives each note
// the free string with the lowest fret, which for those roots is the idiomatic
// shape - strings 8, 7 and 6 at one fret, three adjacent courses. Above that it
// stops being adjacent: a root of 33 puts the fifth and octave on open strings
// and lands on 8, 6 and 5, and a root of 38 lands on 7, 5 and 4. Those are
// still correct notes and a legitimate voicing, but they cross a different
// number of string gaps, which changes exactly the strum travel and coupling
// these takes exist to demonstrate. Verified against the engine's own per-string
// readout rather than assumed.
void playPowerChordProgression(Take& take, bool muted)
{
    struct Shape { int root; double length; float velocity; };
    static const std::array<Shape, 12> progression {{
        { 28, 0.90, 1.00f }, { 28, 0.45, 0.86f }, { 31, 0.90, 0.96f },
        { 30, 0.45, 0.90f }, { 28, 0.90, 1.00f }, { 32, 0.45, 0.92f },
        { 31, 0.90, 0.96f }, { 29, 0.45, 0.88f }, { 30, 0.90, 0.94f },
        { 28, 0.45, 0.90f }, { 32, 0.90, 0.98f }, { 28, 1.60, 1.00f },
    }};

    take.style(muted ? Articulation::Chug : Articulation::Downstroke);
    for (const auto& shape : progression)
    {
        const int fifth = shape.root + 7;
        const int octave = shape.root + 12;
        take.chord({ shape.root, fifth, octave }, shape.velocity);
        take.wait(shape.length * 0.90);
        take.releaseChord({ shape.root, fifth, octave });
        take.wait(shape.length * 0.10);
    }
}

// Held chords first, so the decay and the coupling are audible, then the same
// shapes chugged, then a run of tight muted stabs. Dry, so what is heard is the
// string model and the pickups with nothing after them.
Take renderPowerChordsDry()
{
    auto parameters = metalRhythmVoicing();
    parameters.strumSpreadSeconds = 0.006f;  // a real stroke crosses the strings
    parameters.sympatheticAmount = 0.35f;
    Take take(parameters, FxParameters {}, false);

    take.style(Articulation::Downstroke);
    for (const int root : { 28, 31, 30, 28 })
    {
        take.chord({ root, root + 7, root + 12 }, 0.95f);
        take.wait(1.30);
        take.releaseChord({ root, root + 7, root + 12 });
        take.wait(0.35);
    }

    take.wait(0.35);
    playPowerChordProgression(take, true);
    // Tight stabs under continuous bridge-hand pressure. The pressure is set
    // *before* the pause rather than after it: continuous parameters are
    // smoothed with a 14 ms time constant, and the mute depth is resolved when a
    // voice is configured, so a note-on issued in the same instant as the change
    // would be built against a value still on its way to 55%. The first of the
    // eight stabs would then be looser than the other seven, which is precisely
    // the comparison this passage exists to make.
    parameters.palmMute = 0.55f;
    take.setEngineParameters(parameters);
    take.wait(0.5);
    take.style(Articulation::Chug);
    for (int repeat = 0; repeat < 8; ++repeat)
    {
        const int root = repeat % 4 == 3 ? 32 : 28;
        take.chord({ root, root + 7, root + 12 }, repeat % 2 == 0 ? 1.0f : 0.85f);
        take.wait(0.16);
        take.releaseChord({ root, root + 7, root + 12 });
        take.wait(0.06);
    }
    take.wait(1.2);
    return take;
}

// The same material through the amplifier, cabinet and compressor. Power chords
// are where a high-gain chain earns its oversampling: three fundamentals and
// their harmonic series intermodulate in the clipping stages, and at host rate
// that folds straight back into the guitar band.
Take renderPowerChordsAmped()
{
    auto parameters = metalRhythmVoicing();
    parameters.strumSpreadSeconds = 0.006f;
    parameters.sympatheticAmount = 0.35f;
    FxParameters fx;
    fx.distortion = 0.45f;
    fx.amp = 0.95f;
    fx.compressor = 0.60f;
    Take take(parameters, fx, false);

    take.style(Articulation::Downstroke);
    for (const int root : { 28, 31, 30, 28 })
    {
        take.chord({ root, root + 7, root + 12 }, 0.95f);
        take.wait(1.30);
        take.releaseChord({ root, root + 7, root + 12 });
        take.wait(0.35);
    }

    take.wait(0.35);
    playPowerChordProgression(take, true);
    // As above: the pressure is set before the pause so the first stab is
    // configured at the full 55% rather than partway through the 14 ms ramp.
    parameters.palmMute = 0.55f;
    take.setEngineParameters(parameters);
    take.wait(0.5);
    take.style(Articulation::Chug);
    for (int repeat = 0; repeat < 8; ++repeat)
    {
        const int root = repeat % 4 == 3 ? 32 : 28;
        take.chord({ root, root + 7, root + 12 }, repeat % 2 == 0 ? 1.0f : 0.85f);
        take.wait(0.16);
        take.releaseChord({ root, root + 7, root + 12 });
        take.wait(0.06);
    }
    take.wait(1.4);
    return take;
}

// A longer arrangement: two bars of the chugged single-note figure, a bar of
// power chords, and an open ring-out, all through the amplifier with a little
// room behind it. This is the closest thing here to hearing the instrument in
// a part rather than under a microscope.
Take renderLongRhythmArrangement()
{
    auto parameters = metalRhythmVoicing();
    parameters.strumSpreadSeconds = 0.005f;
    parameters.sympatheticAmount = 0.30f;
    FxParameters fx;
    fx.distortion = 0.42f;
    fx.amp = 0.95f;
    fx.compressor = 0.55f;
    fx.room = 0.18f;
    // The room decorrelates the channels, so this one is a stereo file.
    Take take(parameters, fx, true);

    playRhythmFigure(take);
    take.wait(0.12);
    playRhythmFigure(take);
    take.wait(0.20);

    take.style(Articulation::Downstroke);
    for (const int root : { 32, 31, 30, 28 })
    {
        take.chord({ root, root + 7, root + 12 }, 0.96f);
        take.wait(0.70);
        take.releaseChord({ root, root + 7, root + 12 });
        take.wait(0.10);
    }

    // Ring-out on the full open instrument.
    take.chord({ 28, 35, 40, 45, 50, 55, 59, 64 }, 0.92f);
    take.wait(2.6);
    take.releaseChord({ 28, 35, 40, 45, 50, 55, 59, 64 });
    take.wait(1.6);
    return take;
}

struct Demo
{
    const char* fileName;
    const char* description;
    Take (*render)();
};

const std::array<Demo, 13>& demos()
{
    static const std::array<Demo, 13> table {{
        { "01-range-open-strings.wav",
          "the eight open strings, then all of them ringing together",
          renderOpenStrings },
        { "02-range-full-fretboard.wav",
          "every playable note from E1 to D6 across the eight strings",
          renderFullFretboard },
        { "03-play-styles.wav",
          "all sixteen keyswitched play styles on the same two notes",
          renderPlayStyles },
        { "04-drop-e-rhythm-dry.wav",
          "a chugged Drop-E rhythm figure, dry DI",
          renderRhythmDry },
        { "05-drop-e-rhythm-amp.wav",
          "the same figure through the oversampled amp, cabinet and compressor",
          renderRhythmAmped },
        { "06-lead-amp-delay-room.wav",
          "a lead phrase with bends, vibrato and tremolo picking into amp, "
          "delay and room",
          renderLeadThroughAmp },
        { "07-pickups-and-tone.wav",
          "one phrase through neck, both and bridge, then the tone control "
          "closing",
          renderPickupsAndTone },
        { "08-sympathetic-strum-stereo.wav",
          "strum travel, bridge-coupled sympathetic strings and the "
          "divided-pickup stereo field",
          renderSympatheticStrum },
        { "09-guitar-build-contrasts.wav",
          "the same figure on a short-scale light build, the default "
          "instrument and a 28-inch baritone",
          renderBuildContrasts },
        { "10-velocity-dynamics.wav",
          "the velocity response at full travel, open and palm muted",
          renderVelocityDynamics },
        { "11-power-chords-dry.wav",
          "power chords dry: held, chugged, then tight muted stabs",
          renderPowerChordsDry },
        { "12-power-chords-amp.wav",
          "the same power chords through the amp, cabinet and compressor",
          renderPowerChordsAmped },
        { "13-long-rhythm-arrangement.wav",
          "two bars of the chugged figure, a bar of power chords and an open "
          "ring-out, amped",
          renderLongRhythmArrangement },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool, the engine
// and the effect chain still produce finite, audible audio and a readable WAV
// without committing anything.
int runSmokeTest(const std::filesystem::path& directory)
{
    FxParameters fx;
    fx.amp = 0.8f;
    fx.room = 0.3f;
    Take take(metalRhythmVoicing(), fx, true);
    take.style(Articulation::Chug);
    take.pluck(28, 1.0f, 0.20, 0.05);
    take.style(Articulation::Downstroke);
    take.pluck(40, 0.9f, 0.20, 0.25);

    if (! take.finite())
    {
        std::fprintf(stderr, "smoke test: rendered a non-finite sample\n");
        return 1;
    }
    if (take.peak() < 1.0e-3)
    {
        std::fprintf(stderr, "smoke test: rendered silence (peak %.6f)\n",
                     take.peak());
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto path = directory / "smoke.wav";
    if (! writeWav(path, take.left(), take.right(), true))
    {
        std::fprintf(stderr, "smoke test: could not write %s\n",
                     path.string().c_str());
        return 1;
    }
    const auto size = std::filesystem::file_size(path, error);
    std::filesystem::remove(path, error);
    if (size < 44u + take.left().size() * 4u)
    {
        std::fprintf(stderr, "smoke test: short WAV (%llu bytes)\n",
                     static_cast<unsigned long long>(size));
        return 1;
    }
    std::printf("Electry demo renderer smoke test passed (peak %.3f).\n",
                take.peak());
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> arguments(argv + 1, argv + argc);
    bool smoke = false;
    std::filesystem::path directory = "Docs/audio";
    for (const auto& argument : arguments)
    {
        if (argument == "--smoke")
            smoke = true;
        else if (argument == "--help" || argument == "-h")
        {
            std::printf("usage: ElectryRenderDemos [--smoke] [output-directory]\n");
            return 0;
        }
        else
            directory = argument;
    }

    if (smoke)
        return runSmokeTest(directory);

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (! std::filesystem::is_directory(directory))
    {
        std::fprintf(stderr, "cannot create output directory %s\n",
                     directory.string().c_str());
        return 1;
    }

    int failures = 0;
    for (const auto& demo : demos())
    {
        auto take = demo.render();
        const auto path = directory / demo.fileName;
        const double seconds = static_cast<double>(take.left().size())
                             / demoSampleRate;

        if (! take.finite())
        {
            std::fprintf(stderr, "%s: non-finite sample\n", demo.fileName);
            ++failures;
            continue;
        }
        const double renderedPeak = take.peak();
        if (renderedPeak < 1.0e-3)
        {
            std::fprintf(stderr, "%s: rendered silence\n", demo.fileName);
            ++failures;
            continue;
        }
        // A mono take must genuinely be exact dual mono before it is written as
        // one channel, so the committed file is lossless rather than a downmix.
        if (! take.stereo() && ! take.channelsIdentical())
        {
            std::fprintf(stderr, "%s: channels differ, cannot write as mono\n",
                         demo.fileName);
            ++failures;
            continue;
        }

        // Each file is peak normalised so the set is comfortable to audition
        // one after another: the takes use different voicings and different
        // settings of the instrument's own output control, and their raw peaks
        // span more than twenty decibels. This is one constant gain per file,
        // so nothing inside a take is altered - the velocity ramp, the decay of
        // a chug and the dry-versus-amplified comparison all keep their shape.
        // The rendered peak is reported here and recorded in the directory's
        // README so the untouched level of every take stays on record.
        const double normalisation = normalisedPeak / renderedPeak;
        take.applyGain(static_cast<float>(normalisation));

        if (! writeWav(path, take.left(), take.right(), take.stereo()))
        {
            std::fprintf(stderr, "%s: write failed\n", demo.fileName);
            ++failures;
            continue;
        }

        std::printf("%-34s %s  %5.2f s  rendered %6.1f dBFS  normalised %+5.1f dB"
                    "  %s\n", demo.fileName,
                    take.stereo() ? "stereo" : "mono  ", seconds,
                    20.0 * std::log10(std::max(renderedPeak, 1.0e-9)),
                    20.0 * std::log10(std::max(normalisation, 1.0e-9)),
                    demo.description);
    }

    if (failures != 0)
    {
        std::fprintf(stderr, "%d demo render(s) failed.\n", failures);
        return 1;
    }
    return 0;
}

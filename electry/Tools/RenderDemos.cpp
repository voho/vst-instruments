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
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using electry::ElectryEngine;
using electry::ElectryFx;
using electry::EngineParameters;
using electry::FxParameters;
using electry::OutputMode;
using electry::PickStyle;
using electry::PickupSelector;
using electry::PlayStyle;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;

int keyswitchFor(PickStyle pick)
{
    return ElectryEngine::firstKeyswitchNote + static_cast<int>(pick);
}

int keyswitchFor(PlayStyle style)
{
    return ElectryEngine::firstPlayStyleKeyswitchNote + static_cast<int>(style);
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
        setFxParameters(fxParameters);
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
        // What the plug-in shell does: the rig's acoustic loudness in the
        // room follows the amplifier controls, so the resonance wheel can
        // push a distorted take into feedback.
        engine_.setAcousticReturnLevel(
            std::min(1.0f, parameters.amp + 0.6f * parameters.distortion));
    }

    void pick(PickStyle pickStyle)
    {
        engine_.noteOn(keyswitchFor(pickStyle), 1.0f);
    }

    void style(PlayStyle playStyle)
    {
        engine_.noteOn(keyswitchFor(playStyle), 1.0f);
    }

    void noteOn(int note, float velocity)
    {
        const std::array<ElectryEngine::NoteOnEvent, 1> event {{
            { note, velocity }
        }};
        engine_.noteOnChord(event);
    }
    void noteOff(int note) { engine_.noteOff(note); }
    void pitchBend(float bipolar) { engine_.setPitchBend(bipolar); }
    void resonance(float amount) { engine_.setResonance(amount); }
    // The visible momentary fretting-hand gesture.
    void vibrato(float amount) { engine_.setVibrato(amount); }
    void beginTremoloPicking(float velocity)
    {
        engine_.beginTremoloPicking(velocity);
    }
    void endTremoloPicking() { engine_.endTremoloPicking(); }
    void palmMutePressure(float amount) { engine_.setPalmMutePressure(amount); }
    void sustain(bool down) { engine_.setSustainPedal(down); }

    void chord(std::initializer_list<int> notes, float velocity)
    {
        std::vector<ElectryEngine::NoteOnEvent> events;
        events.reserve(notes.size());
        for (const int note : notes)
            events.push_back({ note, velocity });
        engine_.noteOnChord(events);
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
            // Close the acoustic loop the way the plug-in does: the amplified
            // output is what the loudspeaker plays back at the strings. With
            // the resonance wheel down this stores nothing audible.
            engine_.pushAcousticReturn(left_.data() + offset,
                                       right_.data() + offset, samples);
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

    void fadeOut(double seconds) noexcept
    {
        const auto count = std::min(
            left_.size(), static_cast<std::size_t>(seconds * demoSampleRate));
        if (count < 2)
            return;
        const auto first = left_.size() - count;
        for (std::size_t index = 0; index < count; ++index)
        {
            const float t = static_cast<float>(index)
                          / static_cast<float>(count - 1);
            const float gain = 1.0f - t * t * (3.0f - 2.0f * t);
            left_[first + index] *= gain;
            right_[first + index] *= gain;
        }
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
    // Every combination of the two independent keyswitch banks: each play
    // style in bank order, played with a down stroke, an up stroke, and an
    // alternate-picked pair, so the styles' hands and the strokes' geometry
    // can be compared directly.
    Take take(EngineParameters {}, FxParameters {}, false);
    for (int styleIndex = 0;
         styleIndex < ElectryEngine::playStyleKeyswitchCount; ++styleIndex)
    {
        const auto style = static_cast<PlayStyle>(styleIndex);
        take.style(style);
        if (style == PlayStyle::Hammer)
        {
            // Hammer-ons and pull-offs continue one sounding string, whatever
            // the latched stroke; finish by releasing onto its open note.
            for (const auto pick : { PickStyle::Down, PickStyle::Up })
            {
                take.pick(pick);
                take.noteOn(40, 0.9f);
                take.wait(0.16);
                take.noteOn(45, 0.8f);
                take.wait(0.22);
                take.noteOn(43, 0.78f);
                take.wait(0.20);
                take.noteOn(40, 0.75f);
                take.wait(0.28);
                take.noteOff(40);
                take.noteOff(43);
                take.noteOff(45);
                take.wait(0.06);
            }
            continue;
        }
        if (style == PlayStyle::Slide)
        {
            // A slide needs something to slide from, and its length follows
            // the interval: two frets, then twelve, then twelve back down.
            take.pick(PickStyle::Down);
            for (const int target : { 42, 52, 40 })
            {
                take.style(PlayStyle::Sustain);
                take.noteOn(40, 0.9f);
                take.wait(0.22);
                take.style(PlayStyle::Slide);
                take.noteOn(target, 0.8f);
                take.wait(0.60);
                take.noteOff(target);
                take.noteOff(40);
                take.wait(0.10);
            }
            continue;
        }
        for (const auto pick :
             { PickStyle::Down, PickStyle::Up, PickStyle::Alternate })
        {
            take.pick(pick);
            take.pluck(40, 0.95f, 0.26, 0.04);
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
    applyGuitarBuild(parameters, electry::defaultGuitarBuild);
    parameters.stringAge = 0.10f;
    parameters.pickHardness = 0.85f;
    parameters.pickPosition = 0.18f; // close to the bridge
    parameters.velocityAmount = 0.7f;
    parameters.sympatheticAmount = 0.25f;
    // The rhythm chugs ride the Palm Mute style; a firm setting keeps them at
    // the tight, metal end of the mute's travel.
    parameters.muteDamping = 0.85f;
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

    take.style(PlayStyle::PalmMute);
    for (const auto& hit : bar)
    {
        // The open accents lift the bridge hand off the strings; everything
        // else is chugged.
        take.style(hit.open ? PlayStyle::Sustain : PlayStyle::PalmMute);
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
    // A lead phrase: picked notes, a hammered run, pitch-wheel bends riding
    // the Bend Time glide, a harmonic, and the resonance wheel pushing the
    // final note into amplifier feedback - through the amplifier, the lead
    // delay and the room.
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickupType = 0.4f;
    parameters.toneKnob = 0.9f;
    parameters.bodyResonance = 0.45f;
    parameters.stringAge = 0.08f;
    parameters.pickPosition = 0.30f;
    parameters.resonanceDepth = 1.0f;
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
    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::Sustain);
    take.pluck(64, 0.9f, 0.28, 0.02);
    take.pluck(67, 0.9f, 0.28, 0.02);
    take.style(PlayStyle::Hammer);
    take.noteOn(69, 0.85f);
    take.wait(0.30);
    take.noteOff(69);

    // A wheel bend up a step and back, gliding over the Bend Time.
    take.style(PlayStyle::Sustain);
    take.noteOn(71, 0.95f);
    take.wait(0.35);
    take.pitchBend(0.75f);
    take.wait(0.75);
    take.pitchBend(0.0f);
    take.wait(0.45);
    take.noteOff(71);
    take.wait(0.10);

    // A dive released back onto the note.
    take.pitchBend(-1.0f);
    take.noteOn(74, 0.95f);
    take.wait(0.45);
    take.pitchBend(0.0f);
    take.wait(0.55);
    take.noteOff(74);
    take.wait(0.05);

    // A slid entry into a note held with the fretting hand's vibrato, then a
    // pinch harmonic: the picking hand's thumb catching the string at the
    // pick's own position.
    take.style(PlayStyle::Sustain);
    take.noteOn(67, 0.95f);
    take.wait(0.20);
    take.style(PlayStyle::Slide);
    take.noteOn(74, 0.85f);
    take.wait(0.30);
    take.vibrato(1.0f);
    take.wait(0.95);
    take.vibrato(0.0f);
    take.noteOff(74);
    take.noteOff(67);
    take.wait(0.10);

    take.style(PlayStyle::Pinch);
    take.pluck(57, 1.0f, 0.85, 0.05);

    take.style(PlayStyle::Harmonics);
    take.pluck(64, 0.9f, 0.85, 0.05);

    // The last note is played into the loudspeaker: the resonance wheel opens
    // and the amplifier keeps the string alive after the pick.
    take.style(PlayStyle::Sustain);
    take.noteOn(69, 1.0f);
    take.wait(0.5);
    take.resonance(1.0f);
    take.wait(2.2);
    take.resonance(0.0f);
    take.noteOff(69);
    take.wait(3.0);
    take.fadeOut(1.25);
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
    take.style(PlayStyle::Sustain);
    take.chord({ 28, 35, 40, 47, 52, 56 }, 0.9f);
    take.wait(1.30);
    take.pick(PickStyle::Up);
    take.chord({ 28, 35, 40, 47, 52, 56 }, 0.75f);
    take.wait(1.30);
    take.releaseChord({ 28, 35, 40, 47, 52, 56 });
    take.wait(0.35);

    parameters.sympatheticAmount = 0.0f;
    take.setEngineParameters(parameters);
    take.wait(0.15);
    take.pick(PickStyle::Down);
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
    // One fixed performance/pickup voicing visits every anchor of the same
    // continuous Build path exposed by the plug-in.
    struct Build { const char* name; float value; };
    const std::array<Build, 6> builds {{
        { "slab fixed", 0.0f }, { "contoured", 0.2f },
        { "angular set", 0.4f }, { "modern bolt", 0.6f },
        { "dense extended (default)", 0.8f },
        { "neck-through extended", 1.0f },
    }};

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickPosition = 0.22f;
    parameters.pickHardness = 0.8f;
    parameters.bodyResonance = 0.5f;
    parameters.stringAge = 0.15f;
    parameters.outputGain = 1.2f;

    Take take(parameters, FxParameters {}, false);
    for (const auto& build : builds)
    {
        applyGuitarBuild(parameters, build.value);
        take.setEngineParameters(parameters);
        // Long enough for the smoothers to settle and for the new voicing to
        // reach the ringing strings.
        take.wait(0.25);

        take.style(PlayStyle::Sustain);
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
    // touch to a hard metal attack, then the same ramp palm-muted, so level,
    // brightness, contact noise and bridge-hand contraction can all be heard
    // moving together without velocity pulling the strings out of tune.
    EngineParameters parameters;
    parameters.velocityAmount = 1.0f;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickHardness = 0.7f;
    parameters.artifactAmount = 0.30f;
    parameters.outputGain = 2.0f;

    Take take(parameters, FxParameters {}, false);
    take.style(PlayStyle::Sustain);
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

    take.style(muted ? PlayStyle::PalmMute : PlayStyle::Sustain);
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

    take.style(PlayStyle::Sustain);
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
    take.style(PlayStyle::PalmMute);
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

    take.style(PlayStyle::Sustain);
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
    take.style(PlayStyle::PalmMute);
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

    take.style(PlayStyle::Sustain);
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

// The wheel as a vibrato bar and the modulation wheel as the player's distance
// from a loud amplifier: a chord dived and returned with every string - the
// ringing open ones included - following at its own compliance, then a single
// note pushed into self-sustaining feedback and closed off again.
Take renderWhammyAndFeedback()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.toneKnob = 0.9f;
    parameters.stringAge = 0.10f;
    parameters.sympatheticAmount = 0.45f;
    parameters.resonanceDepth = 1.0f;
    parameters.bendTimeSeconds = 0.34f;
    parameters.outputGain = 1.2f;

    FxParameters fx;
    fx.distortion = 0.55f;
    fx.amp = 0.85f;
    fx.compressor = 0.30f;

    Take take(parameters, fx, false);
    take.style(PlayStyle::Sustain);

    // The bar: a ringing chord dived two semitones and brought back, then
    // pushed sharp. The strings do not move by equal amounts - the slack low
    // eighth string travels furthest - which is the smear a real bar has.
    take.chord({ 28, 35, 40, 47 }, 0.95f);
    take.wait(0.9);
    take.pitchBend(-1.0f);
    take.wait(1.1);
    take.pitchBend(0.0f);
    take.wait(0.8);
    take.pitchBend(1.0f);
    take.wait(0.9);
    take.pitchBend(0.0f);
    take.wait(0.6);
    take.releaseChord({ 28, 35, 40, 47 });
    take.wait(0.5);

    // The feedback: one fretted note, the wheel raised, the key released -
    // and the amplifier keeps the instrument singing until the wheel closes
    // and the bridge hand lands on the strings, which is how a player
    // actually stops a howl.
    take.noteOn(47, 1.0f);
    take.wait(0.6);
    take.resonance(1.0f);
    take.wait(1.2);
    take.noteOff(47);
    take.wait(2.6);
    take.resonance(0.0f);
    take.wait(0.15);
    take.palmMutePressure(1.0f);
    take.wait(1.6);
    return take;
}

// The mute controls under a microscope: the same low string open, at three
// bridge-hand depths, and under the fretting hand, followed by playable
// alternate-picked ghost grooves with and without the bridge hand stacked.
Take renderMuteAndDeadAudition()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickPosition = 0.20f;
    parameters.pickHardness = 0.82f;
    parameters.fingerNoise = 0.55f;
    parameters.artifactAmount = 0.15f;
    parameters.sympatheticAmount = 0.0f;
    parameters.outputGain = 2.0f;

    Take take(parameters, FxParameters {}, false);
    take.pick(PickStyle::Down);
    take.style(PlayStyle::Sustain);
    take.pluck(28, 0.95f, 0.38, 0.16);

    for (const float tightness : { 0.0f, 0.55f, 1.0f })
    {
        parameters.muteDamping = tightness;
        take.setEngineParameters(parameters);
        take.wait(0.12);
        take.style(PlayStyle::PalmMute);
        take.pluck(28, 0.95f, 0.38, 0.16);
    }

    take.style(PlayStyle::Dead);
    take.pluck(28, 0.95f, 0.38, 0.16);
    take.wait(0.20);

    take.pick(PickStyle::Alternate);
    for (int hit = 0; hit < 8; ++hit)
        take.pluck(hit % 4 == 3 ? 40 : 28,
                   hit % 2 == 0 ? 0.95f : 0.82f, 0.085, 0.040);

    take.wait(0.20);
    take.palmMutePressure(0.50f);
    take.wait(0.08);
    for (int hit = 0; hit < 8; ++hit)
        take.pluck(hit % 4 == 3 ? 40 : 28,
                   hit % 2 == 0 ? 0.95f : 0.82f, 0.055, 0.028);
    take.palmMutePressure(0.0f);
    take.wait(0.70);
    return take;
}

// The dry take above exposes the hand/string distinction. This one asks the
// production question: does the exact same rapid score remain distinguishable
// as Palm versus Dead after the common high-gain rhythm chain compresses it?
Take renderMuteAndDeadMetal()
{
    auto parameters = metalRhythmVoicing();
    parameters.fingerNoise = 0.55f;
    parameters.artifactAmount = 0.15f;

    FxParameters fx;
    fx.distortion = 0.45f;
    fx.amp = 0.95f;
    fx.compressor = 0.60f;
    Take take(parameters, fx, false);

    const auto playPhrase = [&] (PlayStyle style, int hits, bool mixedStrings)
    {
        // Relatch Alternate so every comparison starts with the same downstroke.
        take.pick(PickStyle::Alternate);
        take.style(style);
        for (int hit = 0; hit < hits; ++hit)
            take.pluck(mixedStrings && hit % 4 == 3 ? 40 : 28,
                       hit % 2 == 0 ? 0.95f : 0.82f, 0.055, 0.028333);
    };

    take.wait(0.25);
    playPhrase(PlayStyle::PalmMute, 12, false);
    take.wait(0.35);
    playPhrase(PlayStyle::Dead, 12, false);
    take.wait(0.35);
    playPhrase(PlayStyle::PalmMute, 8, true);
    take.wait(0.35);
    playPhrase(PlayStyle::Dead, 8, true);
    take.wait(0.80);
    return take;
}

template <std::size_t Size>
void playPickedRun(Take& take, const std::array<int, Size>& notes,
                   double secondsPerNote, float velocity = 0.88f)
{
    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::Sustain);
    for (std::size_t index = 0; index < notes.size(); ++index)
    {
        const float accent = index % 4 == 0 ? 0.08f
                           : index % 2 == 0 ? 0.03f : -0.03f;
        take.pluck(notes[index], std::clamp(velocity + accent, 0.0f, 1.0f),
                   secondsPerNote * 0.84, secondsPerNote * 0.16);
    }
}

void playPowerChordHit(Take& take, int root, float velocity, double length,
                       PlayStyle style)
{
    take.style(style);
    take.chord({ root, root + 7, root + 12 }, velocity);
    take.wait(length * 0.86);
    take.releaseChord({ root, root + 7, root + 12 });
    take.wait(length * 0.14);
}

// A long, deliberately exposed lead performance rather than a test sweep. It
// visits every play style in a musical order, surrounds the fast passages with
// space, and gives the slide and vibrato enough time to be heard as gestures.
Take renderExtendedTechniqueSolo()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickupType = 0.42f;
    parameters.toneKnob = 0.92f;
    parameters.bodyResonance = 0.42f;
    parameters.stringAge = 0.06f;
    parameters.pickPosition = 0.27f;
    parameters.pickHardness = 0.78f;
    parameters.fingerNoise = 0.52f;
    parameters.artifactAmount = 0.12f;
    parameters.bendTimeSeconds = 0.16f;
    parameters.sympatheticAmount = 0.28f;
    parameters.outputGain = 1.55f;
    parameters.outputMode = OutputMode::Stereo;

    FxParameters fx;
    fx.distortion = 0.28f;
    fx.amp = 0.82f;
    fx.compressor = 0.38f;
    fx.delay = 0.24f;
    fx.room = 0.30f;

    Take take(parameters, fx, true);
    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::Sustain);
    for (const int note : { 64, 67, 69, 72, 70, 67 })
        take.pluck(note, note == 72 ? 0.98f : 0.86f, 0.27, 0.05);

    // One continuous wound-string slide, settling into a wide finger vibrato.
    take.noteOn(62, 0.90f);
    take.wait(0.24);
    take.style(PlayStyle::Slide);
    take.noteOn(69, 0.84f);
    take.wait(0.62);
    take.vibrato(0.82f);
    take.wait(1.15);
    take.vibrato(0.0f);
    take.noteOff(69);
    take.noteOff(62);
    take.wait(0.12);

    // Ascending hammer-ons and direction-aware pull-offs share one ringing
    // string. The following picked run opens into two octaves of shredding.
    take.style(PlayStyle::Sustain);
    take.noteOn(64, 0.88f);
    take.wait(0.18);
    take.style(PlayStyle::Hammer);
    for (const int note : { 67, 69, 72, 74, 72, 69, 67, 64 })
    {
        take.noteOn(note, 0.82f);
        take.wait(0.12);
    }
    for (const int note : { 64, 67, 69, 72, 74 })
        take.noteOff(note);
    take.wait(0.12);

    static constexpr std::array<int, 32> firstShred {{
        64, 67, 69, 70, 72, 74, 76, 79,
        77, 76, 74, 72, 70, 69, 67, 64,
        67, 70, 72, 74, 77, 79, 81, 84,
        81, 79, 77, 74, 72, 70, 67, 64
    }};
    static constexpr std::array<int, 32> secondShred {{
        69, 72, 76, 74, 72, 76, 79, 77,
        76, 79, 83, 81, 79, 77, 76, 74,
        72, 76, 79, 84, 83, 81, 79, 77,
        76, 74, 72, 70, 69, 67, 64, 62
    }};
    playPickedRun(take, firstShred, 0.064, 0.88f);
    playPickedRun(take, secondShred, 0.058, 0.90f);
    take.wait(0.16);

    // A slower answer keeps the solo shaped like a performance instead of one
    // uninterrupted exercise, with a semitone bend blooming into vibrato.
    take.pick(PickStyle::Down);
    take.style(PlayStyle::Sustain);
    take.pluck(72, 0.84f, 0.38, 0.10);
    take.pluck(74, 0.88f, 0.38, 0.10);
    take.noteOn(76, 0.96f);
    take.wait(0.30);
    take.pitchBend(0.5f);
    take.wait(0.52);
    take.pitchBend(0.0f);
    take.wait(0.22);
    take.vibrato(0.46f);
    take.wait(0.95);
    take.vibrato(0.0f);
    take.noteOff(76);
    take.wait(0.18);
    take.pluck(74, 0.82f, 0.34, 0.10);
    take.pluck(72, 0.86f, 0.52, 0.15);

    // The two percussive hand contacts are musical punctuation, not a style
    // catalogue: tight bridge-hand chugs answer fretting-hand dead ghosts.
    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::PalmMute);
    for (int hit = 0; hit < 12; ++hit)
        take.pluck(hit % 6 == 5 ? 35 : 28,
                   hit % 3 == 0 ? 0.98f : 0.80f, 0.070, 0.035);
    take.style(PlayStyle::Dead);
    for (int hit = 0; hit < 8; ++hit)
        take.pluck(hit % 4 == 3 ? 40 : 28,
                   hit % 2 == 0 ? 0.90f : 0.74f, 0.050, 0.040);
    take.wait(0.16);

    take.pick(PickStyle::Down);
    take.style(PlayStyle::Harmonics);
    take.pluck(64, 0.92f, 0.95, 0.08);
    take.style(PlayStyle::Pinch);
    take.pluck(57, 1.0f, 0.95, 0.08);

    // A descending slide resolves onto a final sustained note with a slower,
    // narrower vibrato, then the delay and room are allowed to decay.
    take.style(PlayStyle::Sustain);
    take.noteOn(76, 0.92f);
    take.wait(0.22);
    take.style(PlayStyle::Slide);
    take.noteOn(69, 0.82f);
    take.wait(0.70);
    take.vibrato(0.58f);
    take.wait(1.45);
    take.vibrato(0.0f);
    take.noteOff(69);
    take.noteOff(76);
    take.wait(2.8);
    take.fadeOut(1.2);
    return take;
}

// Original syncopated extended-range study: tight Drop-E subdivisions,
// displaced accents, dead-string punctuation and a tapped upper-register
// release. It uses the broad vocabulary of modern djent/progressive metal,
// not any existing riff or recording.
Take renderSyncopatedDjentStudy()
{
    auto parameters = metalRhythmVoicing();
    parameters.muteDamping = 0.94f;
    parameters.palmMute = 0.18f;
    parameters.strumSpreadSeconds = 0.0015f;
    parameters.artifactAmount = 0.20f;
    parameters.sympatheticAmount = 0.18f;
    parameters.outputMode = OutputMode::Stereo;

    FxParameters fx;
    fx.distortion = 0.50f;
    fx.amp = 1.0f;
    fx.compressor = 0.72f;
    fx.room = 0.09f;
    Take take(parameters, fx, true);

    struct Hit { int note; int units; bool open; bool dead; };
    static constexpr std::array<Hit, 16> cell {{
        { 28, 3, false, false }, { 28, 1, false, true },
        { 31, 2, false, false }, { 28, 2, false, false },
        { 35, 3, true,  false }, { 28, 1, false, true },
        { 30, 2, false, false }, { 28, 2, false, false },
        { 33, 3, true,  false }, { 28, 1, false, true },
        { 28, 2, false, false }, { 36, 2, false, false },
        { 28, 3, false, false }, { 40, 1, true,  false },
        { 31, 2, false, false }, { 28, 2, false, false }
    }};
    const auto playCell = [&]
    {
        take.pick(PickStyle::Alternate);
        for (std::size_t index = 0; index < cell.size(); ++index)
        {
            const auto& hit = cell[index];
            take.style(hit.dead ? PlayStyle::Dead
                                : hit.open ? PlayStyle::Sustain
                                           : PlayStyle::PalmMute);
            const double span = 0.036 * static_cast<double>(hit.units);
            take.pluck(hit.note, index % 4 == 0 ? 0.98f : 0.82f,
                       span * 0.62, span * 0.38);
        }
    };
    for (int repeat = 0; repeat < 4; ++repeat)
    {
        playCell();
        if (repeat == 1 || repeat == 3)
            playPowerChordHit(take, repeat == 1 ? 31 : 28, 0.98f, 0.42,
                              PlayStyle::Sustain);
    }

    // A brief tapped answer leaves the low ostinato without pretending that a
    // single rendered guitar is a layered production.
    take.style(PlayStyle::Sustain);
    take.noteOn(64, 0.86f);
    take.wait(0.16);
    take.style(PlayStyle::Hammer);
    for (const int note : { 71, 76, 72, 79, 74, 71, 67, 64 })
    {
        take.noteOn(note, 0.80f);
        take.wait(0.105);
    }
    for (const int note : { 64, 67, 71, 72, 74, 76, 79 })
        take.noteOff(note);
    take.wait(0.22);

    for (int repeat = 0; repeat < 3; ++repeat)
        playCell();
    playPowerChordHit(take, 28, 1.0f, 1.25, PlayStyle::Sustain);
    take.wait(1.5);
    return take;
}

// Original modern-metalcore study: an open, anthemic chord hook gives way to
// a simple octave melody and then a half-time Drop-E breakdown.
Take renderModernMetalcoreStudy()
{
    auto parameters = metalRhythmVoicing();
    parameters.strumSpreadSeconds = 0.004f;
    parameters.muteDamping = 0.90f;
    parameters.sympatheticAmount = 0.28f;
    parameters.outputMode = OutputMode::Stereo;
    FxParameters fx;
    fx.distortion = 0.46f;
    fx.amp = 0.96f;
    fx.compressor = 0.62f;
    fx.delay = 0.13f;
    fx.room = 0.24f;
    Take take(parameters, fx, true);

    take.pick(PickStyle::Down);
    for (const int root : { 28, 31, 33, 30, 28, 35, 33, 31 })
        playPowerChordHit(take, root, 0.94f, 0.58, PlayStyle::Sustain);

    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::Sustain);
    static constexpr std::array<int, 16> hook {{
        64, 67, 69, 67, 72, 69, 67, 64,
        62, 64, 67, 69, 67, 64, 62, 59
    }};
    for (int repeat = 0; repeat < 2; ++repeat)
        playPickedRun(take, hook, 0.145, repeat == 0 ? 0.84f : 0.90f);
    take.wait(0.18);

    take.pick(PickStyle::Alternate);
    for (int bar = 0; bar < 4; ++bar)
    {
        for (int hit = 0; hit < 8; ++hit)
        {
            const bool accent = hit == 0 || hit == 5;
            const bool ghost = hit == 3 || hit == 7;
            take.style(ghost ? PlayStyle::Dead : PlayStyle::PalmMute);
            take.pluck(accent && bar % 2 != 0 ? 31 : 28,
                       accent ? 1.0f : ghost ? 0.70f : 0.84f,
                       ghost ? 0.045 : 0.085, ghost ? 0.095 : 0.055);
        }
        playPowerChordHit(take, bar % 2 == 0 ? 33 : 31, 0.98f, 0.52,
                          PlayStyle::Sustain);
    }
    playPowerChordHit(take, 28, 1.0f, 1.50, PlayStyle::Sustain);
    take.wait(1.8);
    take.fadeOut(0.9);
    return take;
}

// Original odd-meter progressive study. A clean seven-note arpeggio changes
// into a high-gain 7/8 riff, a fast lead sequence and a five-beat resolution.
Take renderOddMeterProgStudy()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Both;
    parameters.pickupType = 0.46f;
    parameters.toneKnob = 0.92f;
    parameters.bodyResonance = 0.55f;
    parameters.stringAge = 0.10f;
    parameters.pickPosition = 0.34f;
    parameters.pickHardness = 0.68f;
    parameters.sympatheticAmount = 0.38f;
    parameters.outputGain = 1.0f;
    parameters.outputMode = OutputMode::Stereo;
    FxParameters clean;
    clean.amp = 0.22f;
    clean.compressor = 0.20f;
    clean.delay = 0.12f;
    clean.room = 0.30f;
    Take take(parameters, clean, true);

    static constexpr std::array<int, 7> arpeggioA {{ 52, 59, 64, 67, 64, 59, 55 }};
    static constexpr std::array<int, 7> arpeggioB {{ 50, 57, 62, 65, 62, 57, 53 }};
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        playPickedRun(take, repeat % 2 == 0 ? arpeggioA : arpeggioB,
                      0.165, 0.70f);
    }
    take.style(PlayStyle::Harmonics);
    take.pluck(64, 0.46f, 0.80, 0.12);

    FxParameters gain;
    gain.distortion = 0.45f;
    gain.amp = 0.95f;
    gain.compressor = 0.60f;
    gain.delay = 0.16f;
    gain.room = 0.18f;
    take.setFxParameters(gain);
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.pickPosition = 0.20f;
    parameters.outputGain = 2.0f;
    take.setEngineParameters(parameters);
    take.wait(0.30);

    const auto sevenEight = [&] (int highRoot)
    {
        take.pick(PickStyle::Alternate);
        for (int step = 0; step < 14; ++step)
        {
            const bool accent = step == 0 || step == 6 || step == 10;
            take.style(accent ? PlayStyle::Sustain : PlayStyle::PalmMute);
            take.pluck(accent ? highRoot : 28, accent ? 0.98f : 0.82f,
                       0.070, 0.030);
        }
    };
    for (const int root : { 35, 33, 31, 36 })
        sevenEight(root);

    static constexpr std::array<int, 35> progRun {{
        64, 67, 69, 72, 74, 72, 69,
        67, 70, 72, 76, 77, 76, 72,
        69, 72, 74, 77, 81, 77, 74,
        72, 76, 79, 83, 84, 83, 79,
        77, 76, 74, 72, 69, 67, 64
    }};
    playPickedRun(take, progRun, 0.062, 0.89f);
    take.style(PlayStyle::Pinch);
    take.pluck(57, 1.0f, 0.72, 0.10);

    for (const int root : { 28, 31, 33, 30, 28 })
        playPowerChordHit(take, root, 0.96f, 0.42,
                          root == 28 ? PlayStyle::PalmMute
                                     : PlayStyle::Sustain);
    take.wait(2.0);
    take.fadeOut(1.0);
    return take;
}

// Original blues-rock lead study: neck-pickup warmth, dynamic space, slides,
// hammer/pull phrasing, whole-step bends and sustained finger vibrato. It uses
// a shared genre vocabulary and no melody from a released song.
Take renderBluesRockLeadStudy()
{
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Neck;
    parameters.pickupType = 0.56f;
    parameters.toneKnob = 0.78f;
    parameters.bodyResonance = 0.62f;
    parameters.stringAge = 0.20f;
    parameters.pickPosition = 0.38f;
    parameters.pickHardness = 0.52f;
    parameters.velocityAmount = 0.90f;
    parameters.fingerNoise = 0.48f;
    parameters.bendTimeSeconds = 0.15f;
    parameters.sympatheticAmount = 0.35f;
    parameters.outputGain = 1.45f;
    parameters.outputMode = OutputMode::Stereo;
    FxParameters fx;
    fx.distortion = 0.14f;
    fx.amp = 0.62f;
    fx.compressor = 0.28f;
    fx.delay = 0.18f;
    fx.room = 0.28f;
    Take take(parameters, fx, true);

    take.pick(PickStyle::Down);
    take.style(PlayStyle::Sustain);
    for (const int note : { 64, 67, 69, 67, 71, 69 })
        take.pluck(note, note == 71 ? 0.94f : 0.72f, 0.34, 0.12);

    take.noteOn(67, 0.76f);
    take.wait(0.24);
    take.style(PlayStyle::Slide);
    take.noteOn(72, 0.70f);
    take.wait(0.62);
    take.vibrato(0.52f);
    take.wait(1.20);
    take.vibrato(0.0f);
    take.noteOff(72);
    take.noteOff(67);
    take.wait(0.18);

    take.style(PlayStyle::Sustain);
    take.noteOn(69, 0.88f);
    take.wait(0.25);
    take.pitchBend(1.0f); // one whole tone over the wheel's +/-2-semitone range
    take.wait(0.72);
    take.vibrato(0.34f);
    take.wait(0.82);
    take.vibrato(0.0f);
    take.pitchBend(0.0f);
    take.wait(0.34);
    take.noteOff(69);
    take.wait(0.18);

    take.style(PlayStyle::Sustain);
    take.noteOn(64, 0.70f);
    take.wait(0.20);
    take.style(PlayStyle::Hammer);
    for (const int note : { 67, 69, 67, 64, 62, 64 })
    {
        take.noteOn(note, 0.68f);
        take.wait(0.18);
    }
    for (const int note : { 62, 64, 67, 69 })
        take.noteOff(note);
    take.wait(0.20);

    static constexpr std::array<int, 12> turnaround {{
        64, 67, 69, 70, 71, 70, 69, 67, 66, 65, 64, 59
    }};
    playPickedRun(take, turnaround, 0.145, 0.74f);

    take.style(PlayStyle::Pinch);
    take.pluck(57, 0.95f, 0.72, 0.12);
    take.style(PlayStyle::Sustain);
    take.noteOn(76, 0.92f);
    take.wait(0.36);
    take.pitchBend(0.5f);
    take.wait(0.58);
    take.pitchBend(0.0f);
    take.vibrato(0.62f);
    take.wait(1.65);
    take.vibrato(0.0f);
    take.noteOff(76);
    take.wait(2.6);
    take.fadeOut(1.2);
    return take;
}

// The dedicated B0/TRM performance path at the three commissioned capture
// anchors. This is an original study rather than a tempo-grid pattern: one
// held picking wrist drives the ordinary physical repick path, so Alternate,
// muting, pitch changes, vibrato and chord travel remain audible.
Take renderTremoloPickingStudy()
{
    auto parameters = metalRhythmVoicing();
    parameters.muteDamping = 0.92f;
    parameters.palmMute = 0.12f;
    parameters.strumSpreadSeconds = 0.002f;
    parameters.sympatheticAmount = 0.16f;
    parameters.artifactAmount = 0.20f;
    parameters.outputMode = OutputMode::Stereo;
    FxParameters fx;
    fx.distortion = 0.48f;
    fx.amp = 0.98f;
    fx.compressor = 0.66f;
    fx.delay = 0.08f;
    fx.room = 0.14f;
    Take take(parameters, fx, true);

    take.pick(PickStyle::Alternate);
    take.style(PlayStyle::PalmMute);
    const auto lowRate = [&] (float strokesPerSecond, int note,
                              float velocity, double duration)
    {
        parameters.tremoloRateHz = strokesPerSecond;
        take.setEngineParameters(parameters);
        take.wait(0.08);
        take.noteOn(note, velocity * 0.86f);
        take.wait(0.18);
        take.beginTremoloPicking(velocity);
        take.wait(duration);
        take.endTremoloPicking();
        take.noteOff(note);
        take.wait(0.24);
    };
    lowRate(8.0f, 28, 0.88f, 1.50);   // 120 BPM sixteenths
    lowRate(12.0f, 28, 0.92f, 1.50);  // 180 BPM sixteenths
    lowRate(16.0f, 28, 0.96f, 1.50);  // 240 BPM sixteenths

    // Let the wrist run just short of one interval before the fretting hand
    // enters a compact black-metal answer. The first note re-anchors that old
    // empty phase, then each new pitch is its own boundary and the shared 16/s
    // clock resumes without a near-immediate flam or a second attack lane.
    parameters.tremoloRateHz = 16.0f;
    take.setEngineParameters(parameters);
    take.style(PlayStyle::Sustain);
    take.beginTremoloPicking(0.90f);
    take.wait(0.060);
    int currentNote = 40;
    take.noteOn(currentNote, 0.82f);
    for (const int note : { 40, 43, 45, 47, 45, 43, 40, 38 })
    {
        if (note != currentNote)
        {
            take.noteOff(currentNote);
            take.noteOn(note, 0.84f);
            currentNote = note;
        }
        take.wait(0.32);
    }
    take.endTremoloPicking();
    take.noteOff(currentNote);
    take.wait(0.24);

    // A high sustained tremolo becomes a lead gesture when the fretting hand
    // adds width and a slow wheel bend; velocity still controls pick force.
    parameters.tremoloRateHz = 12.0f;
    parameters.palmMute = 0.0f;
    take.setEngineParameters(parameters);
    take.noteOn(76, 0.86f);
    take.wait(0.20);
    take.beginTremoloPicking(0.78f);
    take.wait(0.75);
    take.vibrato(0.42f);
    take.wait(0.90);
    take.pitchBend(0.5f);
    take.wait(0.55);
    take.pitchBend(0.0f);
    take.vibrato(0.0f);
    take.endTremoloPicking();
    take.noteOff(76);
    take.wait(0.30);

    // Polyphonic B0 is one alternating wrist across the held shape. Just after
    // an 8/s grid contact starts crossing the shape, the fretting hand slides
    // E1 two frets to F#1. The reserved low-string pick must finish its travel
    // to the moving fret while the two unchanged strings continue.
    parameters.tremoloRateHz = 8.0f;
    parameters.strumSpreadSeconds = 0.003f;
    take.setEngineParameters(parameters);
    take.chord({ 28, 35, 40 }, 0.82f);
    take.wait(0.22);
    take.beginTremoloPicking(0.86f);
    take.wait(0.505);
    take.style(PlayStyle::Slide);
    take.noteOn(30, 0.82f);
    take.wait(1.25);
    take.endTremoloPicking();
    take.releaseChord({ 30, 35, 40 });
    take.wait(2.0);
    take.fadeOut(0.9);
    return take;
}

struct Demo
{
    const char* fileName;
    const char* description;
    Take (*render)();
};

const std::array<Demo, 22>& demos()
{
    static const std::array<Demo, 22> table {{
        { "01-range-open-strings.wav",
          "the eight open strings, then all of them ringing together",
          renderOpenStrings },
        { "02-range-full-fretboard.wav",
          "every playable note from E1 to D6 across the eight strings",
          renderFullFretboard },
        { "03-play-styles.wav",
          "all three pick strokes and all seven play styles: sustain, mute, "
          "hammer-on and pull-off, natural and pinch harmonics, slides of two "
          "and twelve frets, and dead notes",
          renderPlayStyles },
        { "04-drop-e-rhythm-dry.wav",
          "a chugged Drop-E rhythm figure, dry DI",
          renderRhythmDry },
        { "05-drop-e-rhythm-amp.wav",
          "the same figure through the oversampled amp, cabinet and compressor",
          renderRhythmAmped },
        { "06-lead-amp-delay-room.wav",
          "a lead phrase with wheel bends, a slide into a fingered vibrato, a "
          "pinch harmonic, a natural harmonic and a feedback close, into amp, "
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
          "the same fixed pickup and performance across all six anchors of "
          "the continuous Guitar Build path",
          renderBuildContrasts },
        { "10-velocity-dynamics.wav",
          "the velocity response at full travel, open and muted",
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
        { "14-whammy-and-feedback.wav",
          "a chord dived and raised on the wheel, then a note pushed into "
          "self-sustaining amplifier feedback",
          renderWhammyAndFeedback },
        { "15-mute-and-dead-audition.wav",
          "the same E1 open, at three Mute depths and Dead, followed by "
          "alternate ghost grooves with Mute Pressure off and stacked",
          renderMuteAndDeadAudition },
        { "16-mute-and-dead-metal.wav",
          "the same rapid E1 and mixed E1/E2 scores as Mute then Dead, "
          "through one high-gain rhythm chain",
          renderMuteAndDeadMetal },
        { "17-extended-technique-solo.wav",
          "a long lead with alternate-picked shredding, finger vibrato, "
          "slides and all seven play styles",
          renderExtendedTechniqueSolo },
        { "18-syncopated-djent-study.wav",
          "an original syncopated Drop-E progressive-metal study with tight "
          "chugs, displaced accents, dead notes and a tapped answer",
          renderSyncopatedDjentStudy },
        { "19-modern-metalcore-study.wav",
          "an original modern-metalcore study with anthemic power chords, an "
          "octave hook and a half-time breakdown",
          renderModernMetalcoreStudy },
        { "20-odd-meter-prog-study.wav",
          "an original clean-to-high-gain progressive study in seven- and "
          "five-beat groupings with a virtuoso lead run",
          renderOddMeterProgStudy },
        { "21-blues-rock-lead-study.wav",
          "an original dynamic blues-rock lead with slides, legato, bends, "
          "pinch harmonic and sustained finger vibrato",
          renderBluesRockLeadStudy },
        { "22-tremolo-picking-study.wav",
          "the visible B0 tremolo-picking gesture at 8, 12 and 16 strokes/s, "
          "then a pre-held entrance, vibrato lead and in-flight chord slide",
          renderTremoloPickingStudy },
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
    take.style(PlayStyle::PalmMute);
    take.pluck(28, 1.0f, 0.20, 0.05);
    take.style(PlayStyle::Sustain);
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
// A demo removed from or renamed in demos() must also disappear from the
// output directory, or automation that commits the directory preserves the
// stale file forever while the level table drops its row.
bool removeStaleWavs(const std::filesystem::path& directory)
{
    std::vector<std::string> current;
    for (const auto& demo : demos())
        current.push_back(demo.fileName);

    bool removedAll = true;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (! entry.is_regular_file() || entry.path().extension() != ".wav")
            continue;
        const auto name = entry.path().filename().string();
        if (std::find(current.begin(), current.end(), name) != current.end())
            continue;
        std::error_code error;
        if (std::filesystem::remove(entry.path(), error))
            std::printf("Removed stale demo %s\n", name.c_str());
        else
        {
            std::fprintf(stderr, "could not remove stale demo %s\n",
                         name.c_str());
            removedAll = false;
        }
    }
    return removedAll;
}

// The per-file level table in the instrument's root README is regenerated on
// every full render to keep documented peaks aligned with the committed WAVs.
// The markers bound exactly what the renderer owns; surrounding prose remains
// hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by ElectryRenderDemos;"
    " edits between the markers are overwritten -->";
constexpr const char* peaksTableEnd = "<!-- peaks-table-end -->";

// Only the instrument's canonical Docs/audio directory maps to that root
// README. An ad-hoc output directory maps to nothing and carries no
// documentation side effect.
std::filesystem::path instrumentReadme(const std::filesystem::path& directory)
{
    auto normalised = directory.lexically_normal();
    if (normalised.filename().empty())
        normalised = normalised.parent_path();

    if (normalised.filename() != "audio"
        || normalised.parent_path().filename() != "Docs")
        return {};

    return normalised.parent_path().parent_path() / "README.md";
}


struct RenderedLevel
{
    std::string fileName;
    double renderedPeakDb = 0.0;
    double normalisationDb = 0.0;
};

// A real minus sign rather than a hyphen, so the table reads as typeset prose.
std::string formatSignedDb(double value)
{
    char digits[32];
    std::snprintf(digits, sizeof digits, "%.1f", std::fabs(value));
    const bool negative = value < 0.0 && std::strcmp(digits, "0.0") != 0;
    return std::string(negative ? "\xE2\x88\x92" : "+") + digits;
}

bool updatePeaksTable(const std::filesystem::path& directory,
                      const std::vector<RenderedLevel>& levels)
{
    const auto readmePath = instrumentReadme(directory);
    if (! std::filesystem::exists(readmePath))
        return true; // An ad-hoc output directory carries no documentation.

    std::ifstream input(readmePath, std::ios::binary);
    std::string readme((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
    input.close();

    const auto beginPos = readme.find(peaksTableBegin);
    const auto endPos = readme.find(peaksTableEnd);
    if (beginPos == std::string::npos || endPos == std::string::npos
        || endPos < beginPos)
    {
        std::fprintf(stderr,
                     "%s has no peaks-table markers, so its level table can "
                     "no longer be kept in sync with the rendered files.\n",
                     readmePath.string().c_str());
        return false;
    }

    std::string table = "\n| File | Rendered peak | Normalisation applied |\n"
                        "| --- | --- | --- |\n";
    for (const auto& level : levels)
        table += "| `" + level.fileName + "` | "
               + formatSignedDb(level.renderedPeakDb) + " dBFS | "
               + formatSignedDb(level.normalisationDb) + " dB |\n";

    const auto contentStart = beginPos + std::strlen(peaksTableBegin);
    const auto updated = readme.substr(0, contentStart) + table
                       + readme.substr(endPos);
    if (updated == readme)
        return true;

    std::ofstream output(readmePath, std::ios::binary | std::ios::trunc);
    output << updated;
    output.close();
    std::printf("Updated the rendered-peak table in %s\n",
                readmePath.string().c_str());
    return ! output.fail();
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
    std::vector<RenderedLevel> levels;
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

        const double renderedPeakDb = 20.0 * std::log10(std::max(renderedPeak, 1.0e-9));
        const double normalisationDb = 20.0 * std::log10(std::max(normalisation, 1.0e-9));
        levels.push_back({ demo.fileName, renderedPeakDb, normalisationDb });
        std::printf("%-34s %s  %5.2f s  rendered %6.1f dBFS  normalised %+5.1f dB"
                    "  %s\n", demo.fileName,
                    take.stereo() ? "stereo" : "mono  ", seconds,
                    renderedPeakDb, normalisationDb, demo.description);
    }

    if (failures != 0)
    {
        std::fprintf(stderr, "%d demo render(s) failed.\n", failures);
        return 1;
    }
    // Only a complete render may prune stale files and rewrite the
    // documented level table.
    if (! removeStaleWavs(directory))
        return 1;
    if (! updatePeaksTable(directory, levels))
        return 1;
    return 0;
}

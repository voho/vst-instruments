#include "DSP/EnsembleEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace taikor
{
struct TaikoEngineTestAccess
{
    static int ringingDrums (const TaikoEngine& engine)
    {
        return static_cast<int> (std::count_if (
            engine.physicalDrums_.begin(), engine.physicalDrums_.end(),
            [] (const auto& drum) { return drum.active; }));
    }
};

struct EnsembleEngineTestAccess
{
    struct ScheduledHit
    {
        std::uint64_t due;
        int member;
        float position, azimuth, radial, tangential;
    };
    static TaikoEngine& player (EnsembleEngine& engine, int member)
    { return *engine.players[static_cast<std::size_t> (member)]; }
    static std::vector<ScheduledHit> schedule (const EnsembleEngine& engine)
    {
        std::vector<ScheduledHit> result;
        for (std::size_t index = 0; index < engine.pendingCount; ++index)
        {
            const auto& hit = engine.pending[index];
            result.push_back ({ hit.due, hit.member, hit.position, hit.azimuth,
                                hit.radial, hit.tangential });
        }
        return result;
    }
    static std::size_t pending (const EnsembleEngine& engine) { return engine.pendingCount; }
    static constexpr std::size_t capacity() { return EnsembleEngine::queueCapacity; }
    static bool ordered (const EnsembleEngine& engine)
    {
        return std::is_heap (engine.pending.begin(),
                             engine.pending.begin() + engine.pendingCount,
                             EnsembleEngine::later);
    }
    static int size (const EnsembleEngine& engine) { return engine.parameters.ensembleSize; }
    static float variation (const EnsembleEngine& engine)
    { return engine.parameters.ensembleVariation; }
    static float stagePosition (int member, int size)
    { return EnsembleEngine::stagePosition (member, size); }
    static StereoPan currentPan (const EnsembleEngine& engine, int member)
    { return engine.pan[static_cast<std::size_t> (member)]; }
    static StereoPan targetPan (const EnsembleEngine& engine, int member)
    { return engine.panTarget[static_cast<std::size_t> (member)]; }
    static double gain (const EnsembleEngine& engine) { return engine.gain; }
};
} // namespace taikor

namespace
{
using taikor::Articulation;
using taikor::EnsembleEngine;
using taikor::EngineParameters;
using Access = taikor::EnsembleEngineTestAccess;
int failures = 0;

void expect (bool passed, const std::string& message)
{
    if (! passed)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Audio
{
    explicit Audio (int samples) : left (static_cast<std::size_t> (samples)), right (left.size()) {}
    std::vector<float> left, right;
    double energy() const
    {
        double result = 0.0;
        for (std::size_t sample = 0; sample < left.size(); ++sample)
            result += static_cast<double> (left[sample]) * left[sample]
                    + static_cast<double> (right[sample]) * right[sample];
        return result;
    }
    bool silent() const
    {
        const auto zero = [] (float sample) { return sample == 0.0f; };
        return std::all_of (left.begin(), left.end(), zero)
            && std::all_of (right.begin(), right.end(), zero);
    }
    bool protectedOutput() const
    {
        const auto safe = [] (float sample)
        { return std::isfinite (sample) && std::abs (sample) <= taikor::OutputLimiter::ceiling; };
        return std::all_of (left.begin(), left.end(), safe)
            && std::all_of (right.begin(), right.end(), safe);
    }
    bool operator== (const Audio&) const = default;
};

template<typename Engine>
Audio render (Engine& engine, int samples, int blockSize = 256)
{
    Audio audio (samples);
    for (int offset = 0; offset < samples; offset += blockSize)
        engine.process (audio.left.data() + offset, audio.right.data() + offset,
                        std::min (blockSize, samples - offset));
    return audio;
}

std::unique_ptr<EnsembleEngine> makeEngine (EngineParameters parameters, double rate = 48000.0)
{
    auto engine = std::make_unique<EnsembleEngine>();
    engine->setParameters (parameters);
    engine->prepare (rate, 512);
    return engine;
}

template<typename Engine>
Audio performance (Engine& engine, EngineParameters parameters, int blockSize)
{
    engine.setParameters (parameters);
    engine.reset();
    Audio result (0);
    for (int event = 0; event < 6; ++event)
    {
        if (event == 1)
        {
            engine.setStrikePositionOverride (-0.35f);
            engine.setStrikeAzimuthOverride (0.7f);
            engine.setPitchBend (0.13f);
        }
        if (event == 2)
        {
            engine.setHandDamping (0.07f);
            parameters.drive = 0.63f;
            parameters.outputHighPassHz = 320.0f;
            engine.setParameters (parameters);
        }
        if (event == 3)
        {
            engine.clearStrikeOverrides();
            engine.setPitchBend (-0.08f);
        }
        if (event == 4)
            engine.allSoundsOff();
        const auto articulation = static_cast<Articulation> (event % 4);
        expect (engine.triggerMidi (taikor::midiNoteFor (articulation, event % 4), 0.83f),
                "valid performance notes must be accepted");
        const auto part = render (engine, 1231 + event * 67, blockSize);
        result.left.insert (result.left.end(), part.left.begin(), part.left.end());
        result.right.insert (result.right.end(), part.right.begin(), part.right.end());
    }
    return result;
}

void checkSoloAndReplay()
{
    EngineParameters parameters;
    parameters.drive = 0.32f;
    parameters.outputHighPassHz = 120.0f;
    auto ensemble = makeEngine (parameters);
    auto solo = std::make_unique<taikor::TaikoEngine>();
    solo->setParameters (parameters);
    solo->prepare (48000.0, 512);
    for (const float humanise : { 0.0f, 0.4f })
    {
        parameters.humanise = humanise;
        expect (performance (*ensemble, parameters, 257) == performance (*solo, parameters, 64),
                "ensemble size 1 must preserve the selected B sound bit for bit, including controllers and output FX");
    }
    parameters.ensembleSize = taikor::maximumEnsembleSize;
    parameters.ensembleVariation = 0.83f;
    const auto reference = performance (*ensemble, parameters, 64);
    for (const int blockSize : { 7, 257, 2048 })
        expect (performance (*ensemble, parameters, blockSize) == reference,
                "reset and block partition must replay delayed ensemble hits exactly at block size "
                    + std::to_string (blockSize));
    expect (reference.energy() > 1.0e-6 && reference.protectedOutput(),
            "the multi-event ensemble performance must be audible, finite and limited");
}

double panDifference (const taikor::StereoPan& a, const taikor::StereoPan& b)
{
    return std::max ({ std::abs (a.ll - b.ll), std::abs (a.lr - b.lr),
                       std::abs (a.rl - b.rl), std::abs (a.rr - b.rr) });
}

void checkPanLayoutsAndMatrix()
{
    // Even ensembles occupy matching half-stage slots, with no centre player.
    // In particular, four players are at +/-1 and +/-0.5, not +/-1 and +/-1/3.
    constexpr std::array<std::array<float, taikor::maximumEnsembleSize>, taikor::maximumEnsembleSize> layouts {{
        { 0.0f },
        { -1.0f, 1.0f },
        { -1.0f, 0.0f, 1.0f },
        { -1.0f, -0.5f, 0.5f, 1.0f },
        { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f },
        { -1.0f, -2.0f / 3.0f, -1.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f },
        { -1.0f, -2.0f / 3.0f, -1.0f / 3.0f, 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f },
        { -1.0f, -0.75f, -0.5f, -0.25f, 0.25f, 0.5f, 0.75f, 1.0f }
    }};
    EngineParameters parameters;
    auto engine = makeEngine (parameters);
    for (int size = 1; size <= taikor::maximumEnsembleSize; ++size)
    {
        parameters.ensembleSize = size;
        engine->setParameters (parameters);
        for (int member = 0; member < size; ++member)
        {
            const float expected = layouts[static_cast<std::size_t> (size - 1)][static_cast<std::size_t> (member)];
            expect (std::abs (Access::stagePosition (member, size) - expected) < 1.0e-6f,
                    "ensemble " + std::to_string (size) + " must use the requested symmetric stage positions");
            const auto matrix = taikor::StereoPan::atPosition (expected);
            expect (panDifference (Access::targetPan (*engine, member), matrix) < 1.0e-6f
                        && panDifference (Access::currentPan (*engine, member), matrix) < 1.0e-6f,
                    "silent size changes must position each member before its next attack");
        }
    }
    for (const float position : { -1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const auto matrix = taikor::StereoPan::atPosition (position);
        float left = 1.0f, right = 1.0f;
        matrix.apply (left, right);
        expect (std::abs (left * left + right * right - 2.0f) < 1.0e-6f,
                "panning must preserve the total power of a centred mono source");
        expect ((position < 0.0f && left > right) || (position > 0.0f && right > left)
                    || (position == 0.0f && left == right),
                "pan position must place a mono source on the corresponding side");
        float originalLeft = 0.2f, originalRight = -0.6f;
        float mirroredLeft = originalRight, mirroredRight = originalLeft;
        matrix.apply (originalLeft, originalRight);
        taikor::StereoPan::atPosition (-position).apply (mirroredLeft, mirroredRight);
        expect (std::abs (originalLeft - mirroredRight) < 1.0e-7f
                    && std::abs (originalRight - mirroredLeft) < 1.0e-7f,
                "mirrored stage positions must treat the two microphone channels symmetrically");
    }
    float left = 0.2f, right = -0.6f;
    const auto centre = taikor::StereoPan::atPosition (0.0f);
    centre.apply (left, right);
    expect (centre.isCentered() && left == 0.2f && right == -0.6f,
            "a centred player must preserve its native stereo microphone pair exactly");
    for (const float position : { -1.0f, 1.0f })
    {
        left = 0.2f; right = 0.6f;
        taikor::StereoPan::atPosition (position).apply (left, right);
        const float active = position < 0.0f ? left : right;
        const float opposite = position < 0.0f ? right : left;
        expect (opposite == 0.0f && std::abs (active - 0.8f / std::sqrt (2.0f)) < 1.0e-7f,
                "hard panning must fold both microphones into one side without leaking to the other");
    }
}

void checkPanAudioAndResize()
{
    EngineParameters parameters;
    parameters.ensembleVariation = parameters.humanise = parameters.strikeNoise = 0.0f;
    parameters.drive = parameters.outputHighPassHz = 0.0f;
    parameters.outputGain = 0.15f;
    parameters.stereoWidth = 0.5f; // Neutral shared Width preserves the stage positions.
    auto engine = makeEngine (parameters);
    for (const int size : { 2, 3, 4, taikor::maximumEnsembleSize })
    {
        parameters.ensembleSize = size;
        engine->setParameters (parameters);
        for (const int member : { 0, size - 1 })
        {
            engine->reset();
            // Isolate a physical member so other players cannot mask a routing error.
            Access::player (*engine, member).trigger (Articulation::Don, 0, 0.8f);
            const auto audio = render (*engine, 2048, 113);
            const auto& opposite = member == 0 ? audio.right : audio.left;
            expect (audio.energy() > 1.0e-10 && audio.protectedOutput()
                        && std::all_of (opposite.begin(), opposite.end(), [] (float sample) { return sample == 0.0f; }),
                    "outer member " + std::to_string (member) + " of ensemble " + std::to_string (size)
                        + " must sound exclusively on its assigned side at neutral Width");
        }
    }

    parameters.ensembleSize = 4;
    engine->setParameters (parameters);
    engine->reset();
    engine->trigger (Articulation::Don, 0, 0.8f);
    render (*engine, 512);
    const auto previous = Access::currentPan (*engine, 1);
    const auto removed = Access::currentPan (*engine, 3);
    parameters.ensembleSize = 3;
    engine->setParameters (parameters);
    const auto target = Access::targetPan (*engine, 1);
    expect (panDifference (previous, target) > 0.1f
                && panDifference (previous, Access::currentPan (*engine, 1)) == 0.0f,
            "resizing a sounding ensemble must leave the current pan continuous and set the new layout as its target");
    render (*engine, 1, 1);
    const auto first = Access::currentPan (*engine, 1);
    expect (panDifference (previous, first) > 0.0f && panDifference (previous, first) < 0.01f,
            "the first sample of an active size change must begin a gradual pan transition");
    render (*engine, 12000, 257);
    expect (panDifference (Access::currentPan (*engine, 1), target) < 1.0e-5f,
            "the pan transition must settle at the new member position");
    expect (panDifference (removed, Access::currentPan (*engine, 3)) == 0.0f
                && panDifference (removed, Access::targetPan (*engine, 3)) == 0.0f,
            "removing a player must retain its last position while its already ringing drum finishes");

    parameters.ensembleSize = 2;
    engine->setParameters (parameters);
    engine->reset();
    engine->trigger (Articulation::Don, 0, 0.8f);
    render (*engine, 512);
    const auto originalRight = Access::currentPan (*engine, 1);
    parameters.ensembleSize = 4;
    engine->setParameters (parameters);
    const auto fourPlayerTarget = Access::targetPan (*engine, 1);
    render (*engine, 48); // Remove the player one millisecond into its move.
    const auto intermediate = Access::currentPan (*engine, 1);
    expect (panDifference (intermediate, originalRight) > 1.0e-3
                && panDifference (intermediate, fourPlayerTarget) > 1.0e-3,
            "the rapid-resize probe must remove a player between its old and new positions");
    parameters.ensembleSize = 1;
    engine->setParameters (parameters);
    expect (panDifference (intermediate, Access::targetPan (*engine, 1)) == 0.0,
            "removing a moving player must freeze its target at the current audible position");
    render (*engine, 4096);
    expect (panDifference (intermediate, Access::currentPan (*engine, 1)) == 0.0,
            "a removed tail must not continue toward an obsolete ensemble position");

    engine->reset(); // A fresh, centred solo with no companion tail.
    engine->trigger (Articulation::Don, 0, 0.8f);
    render (*engine, 512);
    parameters.ensembleSize = 2;
    engine->setParameters (parameters);
    const auto fullRight = taikor::StereoPan::atPosition (1.0f);
    expect (Access::currentPan (*engine, 0).isCentered()
                && ! Access::targetPan (*engine, 0).isCentered()
                && panDifference (fullRight, Access::currentPan (*engine, 1)) == 0.0
                && panDifference (fullRight, Access::targetPan (*engine, 1)) == 0.0,
            "adding a silent player must seat it at full right immediately while the ringing solo moves gradually left");
    engine->trigger (Articulation::Don, 1, 0.8f);
    render (*engine, 1, 1);
    expect (Access::player (*engine, 1).getActiveVoiceCount() > 0
                && panDifference (fullRight, Access::currentPan (*engine, 1)) == 0.0
                && ! Access::currentPan (*engine, 0).isCentered()
                && panDifference ({}, Access::currentPan (*engine, 0)) < 0.01,
            "an immediate next stroke must start the added player at its final seat without jumping the already sounding lead");

    const auto resizedPerformance = [&] (int blockSize)
    {
        parameters.ensembleSize = 4;
        parameters.ensembleVariation = 0.8f;
        engine->setParameters (parameters);
        engine->reset();
        Audio result (0);
        for (const int size : { 4, 2, 7, 1 })
        {
            parameters.ensembleSize = size;
            engine->setParameters (parameters);
            engine->trigger (Articulation::Don, 0, 0.8f);
            const auto part = render (*engine, 619, blockSize);
            result.left.insert (result.left.end(), part.left.begin(), part.left.end());
            result.right.insert (result.right.end(), part.right.begin(), part.right.end());
        }
        return result;
    };
    const auto reference = resizedPerformance (64);
    expect (resizedPerformance (7) == reference && resizedPerformance (257) == reference,
            "active size changes, smoothed panning and pending companions must replay independently of block partition");

    parameters.ensembleSize = 2;
    parameters.ensembleVariation = 0.0f;
    auto highRate = makeEngine (parameters, 384000.0);
    highRate->trigger (Articulation::Don, 0, 0.8f);
    render (*highRate, 1, 1);
    parameters.ensembleSize = 1;
    highRate->setParameters (parameters);
    expect (! Access::currentPan (*highRate, 0).isCentered(),
            "active two-to-one resize must begin from the existing left-hand lead position");
    // Silence physical voices independently, leaving the active pan transition
    // intact; this isolates its convergence from the length of a drum decay.
    for (int member = 0; member < taikor::maximumEnsembleSize; ++member)
        Access::player (*highRate, member).allSoundsOff();
    render (*highRate, 384000, 257);
    expect (Access::currentPan (*highRate, 0).isCentered()
                && Access::targetPan (*highRate, 0).isCentered() && Access::gain (*highRate) == 1.0,
            "high-rate pan and level smoothing must reach exact solo bypass after an active two-to-one resize");
}

void checkMembersAndLimits()
{
    EngineParameters parameters;
    parameters.ensembleVariation = parameters.humanise = parameters.strikeNoise = 0.0f;
    auto engine = makeEngine (parameters);
    for (const int requested : { 0, 1, 2, taikor::maximumEnsembleSize, 999 })
    {
        parameters.ensembleSize = requested;
        engine->setParameters (parameters);
        engine->reset();
        const int size = std::clamp (requested, 1, taikor::maximumEnsembleSize);
        expect (Access::size (*engine) == size, "ensemble size must clamp to the supported member count");
        for (int drum = 0; drum < taikor::drumCount; ++drum)
            engine->trigger (Articulation::Don, drum, 0.8f);
        expect (Access::pending (*engine) == 0,
                "zero variation must trigger every member at the original sample timestamp");
        taikor::DrumVisualState leader;
        Access::player (*engine, 0).getVisualState (leader);
        for (int member = 0; member < taikor::maximumEnsembleSize; ++member)
        {
            auto& player = Access::player (*engine, member);
            expect (taikor::TaikoEngineTestAccess::ringingDrums (player)
                        == (member < size ? taikor::drumCount : 0),
                    "size must multiply all four independent physical drum banks");
            if (member < size)
            {
                taikor::DrumVisualState state;
                player.getVisualState (state);
                expect (state.strikeRadius == leader.strikeRadius && state.strikeAngle == leader.strikeAngle,
                        "zero variation and Humanise must retain exact authored placement for every member");
            }
        }
    }
    for (const float variation : { -1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity() })
    {
        parameters.ensembleVariation = variation;
        engine->setParameters (parameters);
        expect (Access::variation (*engine) == (std::isnan (variation) ? 0.0f
                                                : std::clamp (variation, 0.0f, 1.0f)),
                "ensemble variation must sanitise invalid and out-of-range values");
    }

    parameters.ensembleSize = taikor::maximumEnsembleSize;
    parameters.ensembleVariation = 0.0f;
    engine->setParameters (parameters);
    engine->reset();
    engine->trigger (Articulation::Don, 1, 0.8f);
    std::array<Audio, taikor::maximumEnsembleSize> members {
        Audio (2048), Audio (2048), Audio (2048), Audio (2048),
        Audio (2048), Audio (2048), Audio (2048), Audio (2048)
    };
    for (int member = 0; member < taikor::maximumEnsembleSize; ++member)
        Access::player (*engine, member).processRaw (
            members[member].left.data(), members[member].right.data(), 2048);
    for (int member = 1; member < taikor::maximumEnsembleSize; ++member)
    {
        double aa = 0.0, ab = 0.0, bb = 0.0;
        for (std::size_t sample = 0; sample < members[0].left.size(); ++sample)
        {
            const double a = members[0].left[sample], b = members[member].left[sample];
            aa += a * a; ab += a * b; bb += b * b;
        }
        const double fittedGain = ab / std::max (bb, 1.0e-30);
        double residual = 0.0;
        for (std::size_t sample = 0; sample < members[0].left.size(); ++sample)
        {
            const double error = members[0].left[sample] - fittedGain * members[member].left[sample];
            residual += error * error;
        }
        expect (aa > 1.0e-12 && bb > 1.0e-12 && std::sqrt (residual / aa) > 1.0e-6,
                "members must have distinct physical contact timbres, even with Humanise, Noise and Variation at zero");
    }
}

void checkTimingAndPlacement()
{
    EngineParameters parameters;
    parameters.ensembleSize = taikor::maximumEnsembleSize;
    parameters.ensembleVariation = 1.0f;
    parameters.humanise = parameters.strikeNoise = 0.0f;
    auto engine = makeEngine (parameters);
    engine->setStrikePositionOverride (-0.4f);
    engine->setStrikeAzimuthOverride (-0.7f);
    engine->trigger (Articulation::Don, 0, 0.8f);
    expect (Access::player (*engine, 0).getActiveVoiceCount() > 0,
            "the leader must sound at the MIDI timestamp without added latency");
    const auto scheduled = Access::schedule (*engine);
    expect (! scheduled.empty() && Access::ordered (*engine), "variation must schedule companions in timestamp order");
    for (const auto& hit : scheduled)
    {
        expect (hit.member > 0 && hit.due <= 1440 && hit.due > 0,
                "maximum variation must keep companion delays within 0 to 30 milliseconds");
        expect (std::abs (hit.radial) <= 0.055f && std::abs (hit.tangential) <= 0.055f,
                "companion placement must stay within the stated head-radius offsets");
        expect (hit.position == -0.4f && hit.azimuth == -0.7f,
                "delayed strokes must capture their authored contact point when MIDI arrives");
    }
    if (scheduled.empty())
        return;
    const auto captured = scheduled.front();
    auto reference = std::make_unique<taikor::TaikoEngine>();
    reference->setEnsembleMember (captured.member);
    reference->setParameters (parameters);
    reference->prepare (48000.0, 512);
    reference->setStrikePositionOverride (captured.position);
    reference->setStrikeAzimuthOverride (captured.azimuth);
    reference->trigger (Articulation::Don, 0, 0.8f, captured.radial, captured.tangential);
    taikor::DrumVisualState expected;
    reference->getVisualState (expected);
    engine->setStrikePositionOverride (0.9f);
    engine->setStrikeAzimuthOverride (2.0f);
    parameters.strikePosition = 0.8f;
    parameters.strikeAzimuth = 1.5f;
    engine->setParameters (parameters);
    render (*engine, 1441, 113);
    taikor::DrumVisualState actual, leader;
    Access::player (*engine, captured.member).getVisualState (actual);
    Access::player (*engine, 0).getVisualState (leader);
    expect (actual.strikeRadius == expected.strikeRadius && actual.strikeAngle == expected.strikeAngle,
            "later host/CC position changes must not move an already queued strike");
    expect (actual.strikeRadius != leader.strikeRadius || actual.strikeAngle != leader.strikeAngle,
            "variation must move the actual membrane contact, not merely delay or pan a clone");
    expect (Access::pending (*engine) == 0, "every companion must dispatch by the maximum delay");

    engine->trigger (Articulation::Ka, 1, 0.8f);
    expect (Access::pending (*engine) > 0, "panic probe must contain delayed strokes");
    engine->allSoundsOff();
    expect (Access::pending (*engine) == 0 && engine->getActiveVoiceCount() == 0
                && render (*engine, 1600).silent(),
            "panic must immediately clear every drum, queued hit and output tail");
    engine->trigger (Articulation::Don, 0, 0.8f);
    engine->reset();
    expect (Access::pending (*engine) == 0 && render (*engine, 1600).silent(),
            "reset must not leak a previously scheduled companion into a new take");
}

void checkRetainedTails()
{
    EngineParameters parameters;
    parameters.ensembleSize = 2;
    parameters.ensembleVariation = 0.0f;
    auto engine = makeEngine (parameters);
    engine->trigger (Articulation::Don, 0, 0.8f);
    render (*engine, 1024);
    const int previous = taikor::TaikoEngineTestAccess::ringingDrums (Access::player (*engine, 1));
    parameters.ensembleSize = 1;
    engine->setParameters (parameters);
    expect (previous > 0 && taikor::TaikoEngineTestAccess::ringingDrums (Access::player (*engine, 1)) == previous,
            "reducing ensemble size must let already ringing companion drums finish");
    taikor::DrumVisualState before, after;
    Access::player (*engine, 1).getVisualState (before);
    engine->trigger (Articulation::DonRim, 3, 0.8f);
    Access::player (*engine, 1).getVisualState (after);
    expect (before.lastOctaveOffset == after.lastOctaveOffset && before.lastArticulation == after.lastArticulation,
            "reduced size must apply to new strokes immediately");
    Access::player (*engine, 0).allSoundsOff();
    const auto tail = render (*engine, 2048);
    expect (tail.energy() > 1.0e-8 && tail.protectedOutput() && engine->getActiveVoiceCount() > 0,
            "a companion tail must remain audible through the shared output stage while the leader is idle");
}

void checkOverloadAndFlood()
{
    for (const double rate : { 8000.0, 48000.0, 96000.0 })
    {
        EngineParameters parameters;
        parameters.ensembleSize = taikor::maximumEnsembleSize;
        parameters.ensembleVariation = 0.0f;
        parameters.outputGain = 2.0f;
        parameters.stereoWidth = 1.0f;
        parameters.outputHighPassHz = 500.0f;
        auto engine = makeEngine (parameters, rate);
        for (const float drive : { 0.0f, 1.0f })
        {
            parameters.drive = drive;
            engine->setParameters (parameters);
            engine->reset();
            for (int drum = 0; drum < taikor::drumCount; ++drum)
                for (int stroke = 0; stroke < static_cast<int> (taikor::articulationCount); ++stroke)
                    engine->trigger (static_cast<Articulation> (stroke), drum, 1.0f);
            const auto audio = render (*engine, static_cast<int> (rate * 0.06), 257);
            expect (audio.energy() > 1.0e-5 && audio.protectedOutput(),
                    "all ensemble members and strokes must share one finite final limiter at " + std::to_string (rate));
        }
    }
    EngineParameters parameters;
    parameters.ensembleSize = taikor::maximumEnsembleSize;
    parameters.ensembleVariation = 1.0f;
    auto engine = makeEngine (parameters);
    expect (! engine->triggerMidi (0, 1.0f) && ! engine->triggerMidi (48, 0.0f)
                && ! engine->triggerMidi (48, std::numeric_limits<float>::quiet_NaN()),
            "invalid MIDI must never create scheduled ensemble strokes");
    for (int event = 0; event < 200; ++event)
        engine->trigger (Articulation::Don, 0, 0.8f);
    expect (Access::pending (*engine) == Access::capacity() && Access::ordered (*engine),
            "a hostile same-sample MIDI flood must fill a bounded, ordered companion queue");
    expect (Access::player (*engine, 0).getActiveVoiceCount() > 0,
            "queue overflow must preserve the on-time leader");
    const auto flood = render (*engine, 1600, 97);
    expect (Access::pending (*engine) == 0 && flood.protectedOutput(),
            "a full queue must drain on time without non-finite or overloaded output");
}

void reportRenderCost()
{
    constexpr int samples = 12000;
    for (const int size : { 1, taikor::maximumEnsembleSize })
    {
        EngineParameters parameters;
        parameters.ensembleSize = size;
        parameters.ensembleVariation = 0.4f;
        auto engine = makeEngine (parameters);
        for (int drum = 0; drum < taikor::drumCount; ++drum)
            engine->trigger (Articulation::Don, drum, 0.8f);
        const auto start = std::chrono::steady_clock::now();
        const auto active = render (*engine, samples);
        const auto end = std::chrono::steady_clock::now();
        engine->allSoundsOff();
        const auto idleStart = std::chrono::steady_clock::now();
        const auto idle = render (*engine, samples);
        const auto idleEnd = std::chrono::steady_clock::now();
        std::cout << "Ensemble " << size << ": 250 ms, four drums: "
                  << std::chrono::duration<double, std::milli> (end - start).count()
                  << " ms render; idle "
                  << std::chrono::duration<double, std::milli> (idleEnd - idleStart).count()
                  << " ms\n";
        expect (active.energy() > 1.0e-6 && idle.silent(), "CPU probes must render active drums and true idle");
    }
}
} // namespace

int main()
{
    checkSoloAndReplay();
    checkPanLayoutsAndMatrix();
    checkPanAudioAndResize();
    checkMembersAndLimits();
    checkTimingAndPlacement();
    checkRetainedTails();
    checkOverloadAndFlood();
    reportRenderCost();
    if (failures == 0)
        std::cout << "Ensemble engine checks passed\n";
    return failures == 0 ? 0 : 1;
}

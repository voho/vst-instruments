#include "EnsembleEngine.h"

#include <algorithm>
#include <cmath>

namespace taikor
{
EnsembleEngine::EnsembleEngine()
{
    for (int member = 0; member < maximumEnsembleSize; ++member)
    {
        players[member] = std::make_unique<TaikoEngine>();
        players[member]->setEnsembleMember (member);
    }
}

void EnsembleEngine::prepare (double sampleRate, int maxBlockSize) noexcept
{
    // Match the physical engine's supported clock before scheduling delays.
    rate = std::isfinite (sampleRate) ? std::clamp (sampleRate, 8000.0, 384000.0)
                                    : 48000.0;
    gainSmoothing = -std::expm1 (-1.0 / (0.015 * rate));
    for (auto& player : players)
        player->prepare (rate, maxBlockSize);
    prepared = true;
    reset();
}

void EnsembleEngine::reset() noexcept
{
    for (auto& player : players)
        player->reset();
    pendingCount = 0;
    sampleClock = strokeSequence = eventSequence = 0;
    gain = 1.0f / std::sqrt (static_cast<float> (parameters.ensembleSize));
    pan.fill ({});
    panTarget.fill ({});
    updateStage (true);
    clearStrikeOverrides();
    publishVoices();
}

void EnsembleEngine::allSoundsOff() noexcept
{
    for (auto& player : players)
        player->allSoundsOff();
    pendingCount = 0;
    // Panic clears scheduled hits but preserves each player's gesture sequence.
    gain = 1.0f / std::sqrt (static_cast<float> (parameters.ensembleSize));
    updateStage (true);
    publishVoices();
}

void EnsembleEngine::setParameters (const EngineParameters& next) noexcept
{
    const int previousSize = parameters.ensembleSize;
    parameters = next;
    parameters.ensembleSize = std::clamp (next.ensembleSize, 1, maximumEnsembleSize);
    parameters.ensembleVariation = std::isnan (next.ensembleVariation)
        ? 0.0f : std::clamp (next.ensembleVariation, 0.0f, 1.0f);
    for (auto& player : players)
        player->setParameters (parameters);
    const bool silent = pendingCount == 0 && getActiveVoiceCount() == 0;
    if (silent)
        gain = 1.0f / std::sqrt (static_cast<float> (parameters.ensembleSize));
    if (parameters.ensembleSize != previousSize)
    {
        // A tail removed during an earlier move stays where it is now.
        for (int member = parameters.ensembleSize; member < previousSize; ++member)
            panTarget[member] = pan[member];
        updateStage (silent);
    }
}

float EnsembleEngine::stagePosition (int member, int size) noexcept
{
    if (size <= 1)
        return 0.0f;
    const int half = size / 2;
    // Odd ensembles include center; even ensembles fill both halves equally.
    const int slot = member - half + (size % 2 == 0 && member >= half ? 1 : 0);
    return static_cast<float> (slot) / static_cast<float> (half);
}

void EnsembleEngine::updateStage (bool snap) noexcept
{
    for (int member = 0; member < parameters.ensembleSize; ++member)
    {
        panTarget[member] = StereoPan::atPosition (
            stagePosition (member, parameters.ensembleSize));
        // A silent player can take its seat before the next attack. Only an
        // already-ringing microphone pair needs a gradual move.
        if (snap || players[member]->getActiveVoiceCount() == 0)
            pan[member] = panTarget[member];
    }
    // Removed members keep their stage positions as existing tails finish.
}

std::uint32_t EnsembleEngine::hash (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

bool EnsembleEngine::later (const Hit& a, const Hit& b) noexcept
{
    return a.due > b.due || (a.due == b.due && a.order > b.order);
}

void EnsembleEngine::fire (const Hit& hit) noexcept
{
    auto& player = *players[hit.member];
    // Capture placement when MIDI arrives. Later controller changes cannot
    // move an already-scheduled player's hand to a different point.
    player.setStrikePositionOverride (hit.position);
    player.setStrikeAzimuthOverride (hit.azimuth);
    player.trigger (hit.articulation, hit.octave, hit.velocity,
                    hit.radial, hit.tangential);
    player.clearStrikeOverrides();
}

void EnsembleEngine::dispatchDue() noexcept
{
    while (pendingCount > 0 && pending[0].due <= sampleClock)
    {
        std::pop_heap (pending.begin(), pending.begin() + pendingCount, later);
        fire (pending[--pendingCount]);
    }
}

void EnsembleEngine::trigger (Articulation articulation, int octave, float velocity) noexcept
{
    if (! prepared || static_cast<std::size_t> (articulation) >= articulationCount
        || ! std::isfinite (velocity) || velocity <= 0.0f)
        return;
    dispatchDue();
    const auto sequence = ++strokeSequence;
    const float spread = parameters.ensembleVariation;
    for (int member = 0; member < parameters.ensembleSize; ++member)
    {
        Hit hit;
        hit.order = ++eventSequence;
        hit.due = sampleClock;
        hit.member = member;
        hit.octave = std::clamp (octave, lowestOctaveOffset, highestOctaveOffset);
        hit.articulation = articulation;
        hit.velocity = std::clamp (velocity, 0.0f, 1.0f);
        hit.position = positionOverridden ? positionOverride : parameters.strikePosition;
        hit.azimuth = azimuthOverridden ? azimuthOverride : parameters.strikeAzimuth;
        if (member > 0)
        {
            const auto seed = hash (static_cast<std::uint32_t> (sequence))
                            ^ hash (static_cast<std::uint32_t> (sequence >> 32))
                            ^ hash (static_cast<std::uint32_t> (member) * 0x9e3779b9u)
                            ^ hash (static_cast<std::uint32_t> (parameters.performer));
            const auto unit = [] (std::uint32_t value)
            { return static_cast<double> (hash (value)) / 4294967295.0; };
            // The leader anchors the MIDI timestamp. Companions are late by
            // 0..30 ms, so live playing needs no added latency/lookahead.
            hit.due += static_cast<std::uint64_t> (std::llround (
                maximumEnsembleDelaySeconds * rate * spread * unit (seed)));
            hit.radial = static_cast<float> (0.055 * spread * (2.0 * unit (seed + 1u) - 1.0));
            hit.tangential = static_cast<float> (0.055 * spread * (2.0 * unit (seed + 2u) - 1.0));
        }
        if (hit.due == sampleClock)
            fire (hit);
        else if (pendingCount < queueCapacity)
        {
            pending[pendingCount++] = hit;
            std::push_heap (pending.begin(), pending.begin() + pendingCount, later);
        }
        // A pathological MIDI flood can fill the bounded delay queue. Drop
        // only excess delayed companions; the on-time leader always plays.
    }
    publishVoices();
}

bool EnsembleEngine::triggerMidi (int note, float velocity) noexcept
{
    const auto articulation = articulationForMidiNote (note);
    const auto octave = octaveOffsetForMidiNote (note);
    if (! prepared || ! articulation || ! octave || ! std::isfinite (velocity)
        || velocity <= 0.0f)
        return false;
    trigger (*articulation, *octave, velocity);
    return true;
}

void EnsembleEngine::process (float* left, float* right, int samples) noexcept
{
    if (left == nullptr || right == nullptr || samples <= 0)
        return;
    if (! prepared)
    {
        std::fill_n (left, samples, 0.0f);
        std::fill_n (right, samples, 0.0f);
        return;
    }
    int rendered = 0;
    while (rendered < samples)
    {
        dispatchDue();
        int count = std::min (blockCapacity, samples - rendered);
        if (pendingCount > 0)
            count = static_cast<int> (std::min<std::uint64_t> (
                static_cast<std::uint64_t> (count), pending[0].due - sampleClock));
        std::fill_n (extraLeft.data(), count, 0.0f);
        std::fill_n (extraRight.data(), count, 0.0f);
        bool extraActive = false;
        for (int member = 1; member < maximumEnsembleSize; ++member)
        {
            auto& player = *players[member];
            extraActive = extraActive || player.getActiveVoiceCount() > 0;
            // Even silent players advance held-palm/pitch smoothers. Removing
            // a player from the size control lets their existing tail finish.
            player.processRaw (scratchLeft.data(), scratchRight.data(), count);
            for (int sample = 0; sample < count; ++sample)
            {
                pan[member].approach (panTarget[member], gainSmoothing);
                pan[member].apply (scratchLeft[sample], scratchRight[sample]);
                extraLeft[sample] += scratchLeft[sample];
                extraRight[sample] += scratchRight[sample];
            }
        }
        const float target = 1.0f / std::sqrt (static_cast<float> (parameters.ensembleSize));
        const bool unityGain = gain == 1.0f && target == 1.0f;
        const bool centeredLead = pan[0].isCentered() && panTarget[0].isCentered();
        for (int sample = 0; sample < count; ++sample)
        {
            pan[0].approach (panTarget[0], gainSmoothing);
            leadPan[sample] = pan[0];
            gain += gainSmoothing * (target - gain);
            if (std::abs (gain - target) < 1.0e-10)
                gain = target;
            mixGain[sample] = static_cast<float> (gain);
        }
        const auto silent = [] (float value) { return value == 0.0f; };
        if (! extraActive && unityGain && centeredLead
            && std::all_of (extraLeft.begin(), extraLeft.begin() + count, silent)
            && std::all_of (extraRight.begin(), extraRight.begin() + count, silent))
            players[0]->process (left + rendered, right + rendered, count);
        else
            players[0]->processWithEnsemble (left + rendered, right + rendered, count,
                extraLeft.data(), extraRight.data(), mixGain.data(), extraActive,
                centeredLead ? nullptr : leadPan.data());
        rendered += count;
        sampleClock += static_cast<std::uint64_t> (count);
    }
    publishVoices();
}

void EnsembleEngine::publishVoices() noexcept
{
    int count = 0;
    for (const auto& player : players)
        count += player->getActiveVoiceCount();
    activeVoices.store (count, std::memory_order_relaxed);
}

void EnsembleEngine::setHandDamping (float amount) noexcept
{ for (auto& player : players) player->setHandDamping (amount); }
void EnsembleEngine::setPitchBend (float amount) noexcept
{ for (auto& player : players) player->setPitchBend (amount); }
void EnsembleEngine::setStrikePositionOverride (float amount) noexcept
{ positionOverride = amount; positionOverridden = true; }
void EnsembleEngine::setStrikeAzimuthOverride (float amount) noexcept
{ azimuthOverride = amount; azimuthOverridden = true; }
void EnsembleEngine::clearStrikeOverrides() noexcept
{ positionOverridden = azimuthOverridden = false; }
} // namespace taikor

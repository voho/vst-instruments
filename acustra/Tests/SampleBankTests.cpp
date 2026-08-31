#include "DSP/SampleBank/EmbeddedGuitarBank.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{};

void require(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        std::abort();
    }
}
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)

void* operator new(std::size_t size) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace acustra::dense {
struct LibraryTestAccess {
    static void replaceZones(Library& library, std::vector<ZoneView> zones) {
        library.zones_ = std::move(zones);
    }
};
}

int main() {
    using namespace acustra::dense;
    Library library;
    std::string error;
    REQUIRE(library.prepare(&error) && error.empty());
    REQUIRE(library.zoneCount() == 321);

    std::size_t nylonZones = 0;
    std::size_t shinyZones = 0;
    std::size_t eastmanZones = 0;
    for (std::size_t index = 0; index < library.zoneCount(); ++index) {
        const auto* zone = library.zone(index);
        REQUIRE(zone != nullptr && zone->samples != nullptr);
        REQUIRE(zone->onsetFrame < zone->frames);
        REQUIRE(zone->endJump <= 64);
        REQUIRE(zone->physicalStringIndex == -1);
        REQUIRE(zone->capturedOpenMidi == -1);
        REQUIRE(zone->capturedFret == -1);
        const auto firstTerminalFrame = zone->frames > 64 ? zone->frames - 64 : 1;
        int measuredTerminalJump = 0;
        for (std::uint8_t channel = 0; channel < zone->channels; ++channel) {
            auto previous = zone->samples[(firstTerminalFrame - 1) * zone->channels + channel];
            for (std::uint32_t frame = firstTerminalFrame; frame < zone->frames; ++frame) {
                const auto current = zone->samples[frame * zone->channels + channel];
                measuredTerminalJump = std::max(measuredTerminalJump, std::abs(current - previous));
                previous = current;
            }
            measuredTerminalJump = std::max(measuredTerminalJump, std::abs(static_cast<int>(previous)));
        }
        REQUIRE(measuredTerminalJump == zone->endJump);
        const auto nominalHz = 440.0 * std::exp2((static_cast<int>(zone->rootMidi) - 69) / 12.0);
        const auto tuningCents = 1200.0 * std::log2(zone->rootHz / nominalHz);
        REQUIRE(std::abs(tuningCents) < 40.0);
        if (zone->bank == Bank::Nylon) {
            ++nylonZones;
            REQUIRE(zone->channels == 1);
            REQUIRE(zone->terminalFadeFrames == 0);
        } else if (zone->bank == Bank::SteelPicked) {
            ++shinyZones;
            REQUIRE(zone->channels == 1);
            REQUIRE(zone->frames > zone->sampleRate * 39 / 10);
            REQUIRE(zone->terminalFadeFrames == zone->sampleRate * 60 / 1000);
            REQUIRE(zone->lowVelocity >= 1);
            REQUIRE(zone->lowVelocity <= zone->highVelocity);
            REQUIRE(zone->highVelocity <= 127);
            REQUIRE(zone->roundRobin < 4);
            REQUIRE(std::abs(tuningCents) < 20.0);
            REQUIRE(zone->endJump <= 1);
        } else {
            ++eastmanZones;
            REQUIRE(zone->channels == 2);
            REQUIRE(zone->frames == zone->sampleRate * 3);
            REQUIRE(zone->terminalFadeFrames == zone->sampleRate * 60 / 1000);
            REQUIRE(zone->endJump <= 1);
        }
    }
    REQUIRE(nylonZones == 41 && shinyZones == 272 && eastmanZones == 8);
    REQUIRE(library.find(Bank::SteelPicked, 37, 127, 0) == nullptr);
    REQUIRE(library.find(Bank::SteelPicked, 85, 127, 0) == nullptr);
    REQUIRE(library.find(Bank::SteelPicked, 52, 0, 0) == nullptr);
    REQUIRE(library.find(Bank::SteelPicked, 52, 128, 0) == nullptr);
    REQUIRE(library.find(Bank::SteelPicked, 52, 127, 0, 1, 45, 7)
            == library.find(Bank::SteelPicked, 52, 127, 0));
    for (const int midi : {83, 84})
        for (const int velocity : {32, 64, 96, 127})
            for (std::uint8_t rr = 0; rr < 4; ++rr) {
                const auto* zone = library.find(
                    Bank::SteelPicked, midi, velocity, rr);
                REQUIRE(zone != nullptr && zone->rootMidi == 84);
                REQUIRE(zone->roundRobin == rr);
            }

    // Exact physical realizations win over generic regions. Generic regions
    // remain the fallback for legacy banks and unknown locations.
    const std::array<std::int16_t, 4> locationSamples{{ 0, 1000, -500, 0 }};
    auto makeZone = [&] (Bank bank, std::uint8_t roundRobin,
                         int physicalStringIndex, int openMidi, int fret) {
        ZoneView zone{};
        zone.bank = bank;
        zone.lowKey = 60;
        zone.highKey = 60;
        zone.rootMidi = 60;
        zone.rootHz = 261.625565f;
        zone.sampleRate = 48000;
        zone.channels = 1;
        zone.frames = static_cast<std::uint32_t>(locationSamples.size());
        zone.peak = 1000;
        zone.lowVelocity = 1;
        zone.highVelocity = 127;
        zone.roundRobin = roundRobin;
        zone.samples = locationSamples.data();
        zone.physicalStringIndex = static_cast<std::int8_t>(physicalStringIndex);
        zone.capturedOpenMidi = static_cast<std::int8_t>(openMidi);
        zone.capturedFret = static_cast<std::int8_t>(fret);
        return zone;
    };
    Library locatedLibrary;
    LibraryTestAccess::replaceZones(locatedLibrary, {
        makeZone(Bank::SteelPicked, 0, -1, -1, -1),
        makeZone(Bank::SteelPicked, 3, -1, -1, -1),
        makeZone(Bank::SteelPicked, 0, 0, 40, 20),
        makeZone(Bank::SteelPicked, 1, 0, 40, 20),
        makeZone(Bank::SteelPicked, 0, 1, 45, 15),
    });
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 60, 100, 1, 0, 40, 20)
            == locatedLibrary.zone(3));
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 60, 100, 3, 0, 40, 20)
            == locatedLibrary.zone(2));
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 60, 100, 0, 1, 45, 15)
            == locatedLibrary.zone(4));
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 59, 100, 0, 1, 44, 15)
            == locatedLibrary.zone(4));
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 60, 100, 3, 5, 40, 20)
            == locatedLibrary.zone(1));
    REQUIRE(locatedLibrary.find(Bank::SteelPicked, 60, 100, 3)
            == locatedLibrary.zone(1));

    Sampler locatedSampler(locatedLibrary);
    locatedSampler.setOutputSampleRate(48000.0);
    REQUIRE(locatedSampler.noteOn(0, Bank::SteelPicked, 60,
                                  0.8f, 0.0f, 0.0f, 40));
    REQUIRE(locatedSampler.activeZone(0) == locatedLibrary.zone(2));
    REQUIRE(locatedSampler.noteOn(1, Bank::SteelPicked, 60,
                                  0.8f, 0.0f, 0.0f, 45));
    REQUIRE(locatedSampler.activeZone(1) == locatedLibrary.zone(4));
    REQUIRE(locatedSampler.noteOn(1, Bank::SteelPicked, 59,
                                  0.8f, 0.0f, 0.0f, 44));
    REQUIRE(locatedSampler.activeZone(1) == locatedLibrary.zone(4));

    Library nylonRoundRobins;
    LibraryTestAccess::replaceZones(nylonRoundRobins, {
        makeZone(Bank::Nylon, 0, 0, 40, 20),
        makeZone(Bank::Nylon, 1, 0, 40, 20),
        makeZone(Bank::Nylon, 2, 0, 40, 20),
        makeZone(Bank::Nylon, 3, 0, 40, 20),
    });
    Sampler nylonSampler(nylonRoundRobins);
    nylonSampler.setOutputSampleRate(48000.0);
    for (std::uint8_t rr = 0; rr < 4; ++rr) {
        REQUIRE(nylonSampler.noteOn(0, Bank::Nylon, 60,
                                    0.8f, 0.0f, 0.0f, 40));
        REQUIRE(nylonSampler.activeZone(0) == nylonRoundRobins.zone(rr));
    }

    Library independentBanks;
    LibraryTestAccess::replaceZones(independentBanks, {
        makeZone(Bank::SteelPicked, 0, 0, 40, 20),
        makeZone(Bank::SteelPicked, 1, 0, 40, 20),
        makeZone(Bank::Nylon, 0, 0, 40, 20),
        makeZone(Bank::Nylon, 1, 0, 40, 20),
    });
    Sampler independentSampler(independentBanks);
    independentSampler.setOutputSampleRate(48000.0);
    REQUIRE(!independentSampler.noteOn(0, static_cast<Bank>(255), 60,
                                       0.8f, 0.0f, 0.0f, 40));
    for (std::size_t rr = 0; rr < 2; ++rr) {
        REQUIRE(independentSampler.noteOn(0, Bank::Nylon, 60,
                                          0.8f, 0.0f, 0.0f, 40));
        REQUIRE(independentSampler.activeZone(0)
                == independentBanks.zone(2 + rr));
        REQUIRE(independentSampler.noteOn(0, Bank::SteelPicked, 60,
                                          0.8f, 0.0f, 0.0f, 40));
        REQUIRE(independentSampler.activeZone(0)
                == independentBanks.zone(rr));
    }

    // The immediate Shiny attacks already begin at zero. An eight-frame ramp
    // removes any interpolation click without shaving off their pick transient
    // or adding a slope sharper than the recorded attack itself.
    double worstFirstMillisecondLossDb = 0.0;
    double worstRampToSourceDerivative = 0.0;
    for (std::size_t index = 0; index < library.zoneCount(); ++index) {
        const auto* expectedZone = library.zone(index);
        if (expectedZone == nullptr || expectedZone->bank != Bank::SteelPicked)
            continue;
        Sampler transient(library);
        transient.setOutputSampleRate(44100.0);
        const float velocity = expectedZone->highVelocity / 127.0f;
        const int targetMidi = std::clamp<int>(
            expectedZone->rootMidi, expectedZone->lowKey,
            expectedZone->highKey);
        std::array<float, 512> discardedLeft{};
        std::array<float, 512> discardedRight{};
        for (std::uint8_t rr = 0; rr < expectedZone->roundRobin; ++rr) {
            REQUIRE(transient.noteOn(0, Bank::SteelPicked,
                                     targetMidi, velocity,
                                     0.0f, 0.0f));
            transient.noteOff(0, 1.0f);
            transient.process(discardedLeft.data(), discardedRight.data(),
                              discardedLeft.size());
            REQUIRE(!transient.isActive(0));
        }
        REQUIRE(transient.noteOn(0, Bank::SteelPicked,
                                 targetMidi, velocity,
                                 0.0f, 0.0f));
        REQUIRE(transient.activeZone(0) == expectedZone);

        constexpr std::size_t attackFrames = 882; // 20 ms at 44.1 kHz
        constexpr std::size_t firstMillisecondFrames = 44;
        std::array<float, attackFrames> renderedLeft{};
        std::array<float, attackFrames> renderedRight{};
        transient.process(renderedLeft.data(), renderedRight.data(),
                          renderedLeft.size());
        REQUIRE(renderedLeft[0] == 0.0f && renderedRight[0] == 0.0f);

        const double nominalHz = 440.0 * std::exp2(
            (targetMidi - 69) / 12.0);
        const double step = nominalHz / expectedZone->rootHz;
        const auto sourceAt = [&] (std::int64_t frame) {
            frame = std::clamp<std::int64_t>(
                frame, 0, expectedZone->frames - 1);
            return expectedZone->samples[static_cast<std::size_t>(frame)
                * expectedZone->channels] / 32768.0f;
        };
        std::array<float, attackFrames> reference{};
        for (std::size_t frame = 0; frame < reference.size(); ++frame) {
            const double position = frame * step;
            const auto center = static_cast<std::int64_t>(position);
            const float fraction = static_cast<float>(position - center);
            const float y0 = sourceAt(center - 1);
            const float y1 = sourceAt(center);
            const float y2 = sourceAt(center + 1);
            const float y3 = sourceAt(center + 2);
            const float a0 = y3 - y2 - y0 + y1;
            const float a1 = y0 - y1 - a0;
            const float a2 = y2 - y0;
            reference[frame] = 0.55f * expectedZone->playbackTrim
                * std::pow(expectedZone->highVelocity / 127.0f, 0.82f)
                * (((a0 * fraction + a1) * fraction + a2) * fraction + y1);
        }

        double renderedEnergy = 0.0;
        double referenceEnergy = 0.0;
        for (std::size_t frame = 0; frame < firstMillisecondFrames; ++frame) {
            renderedEnergy += renderedLeft[frame] * renderedLeft[frame];
            referenceEnergy += reference[frame] * reference[frame];
        }
        REQUIRE(referenceEnergy > 0.0);
        const double lossDb = 10.0 * std::log10(
            renderedEnergy / referenceEnergy);
        worstFirstMillisecondLossDb = std::min(
            worstFirstMillisecondLossDb, lossDb);
        REQUIRE(lossDb > -1.0);

        std::vector<double> sourceDerivatives;
        sourceDerivatives.reserve(reference.size() - 1);
        for (std::size_t frame = 1; frame < reference.size(); ++frame)
            sourceDerivatives.push_back(std::abs(
                reference[frame] - reference[frame - 1]));
        std::sort(sourceDerivatives.begin(), sourceDerivatives.end());
        const auto p99Index = static_cast<std::size_t>(
            0.99 * static_cast<double>(sourceDerivatives.size() - 1));
        const double p99SourceDerivative = sourceDerivatives[p99Index];
        double rampDerivative = 0.0;
        for (std::size_t frame = 1; frame <= 8; ++frame)
            rampDerivative = std::max(rampDerivative, static_cast<double>(
                std::abs(renderedLeft[frame] - renderedLeft[frame - 1])));
        REQUIRE(p99SourceDerivative > 0.0);
        const double derivativeRatio = rampDerivative / p99SourceDerivative;
        worstRampToSourceDerivative = std::max(
            worstRampToSourceDerivative, derivativeRatio);
        REQUIRE(derivativeRatio <= 1.0);
    }

    constexpr std::array<int, 4> layerVelocities {32, 64, 96, 127};
    for (int midi = 38; midi <= 84; ++midi) {
        for (std::size_t layer = 0; layer < layerVelocities.size(); ++layer) {
            for (std::uint8_t roundRobin = 0; roundRobin < 4; ++roundRobin) {
                const auto* zone = library.find(
                    Bank::SteelPicked, midi, layerVelocities[layer], roundRobin);
                REQUIRE(zone != nullptr);
                REQUIRE(zone->roundRobin == roundRobin);
                REQUIRE(zone->lowVelocity == (layer == 0 ? 1 : layerVelocities[layer - 1] + 1));
                REQUIRE(zone->highVelocity == layerVelocities[layer]);
            }
        }
    }

    double worstLatencyMs = 0.0;
    for (const auto bank : {Bank::Nylon, Bank::SteelPicked, Bank::SteelPlucked}) {
        for (int midi = 38; midi <= 84; ++midi) {
            const auto* zone = library.find(bank, midi);
            REQUIRE(zone != nullptr);
            Sampler sampler(library);
            sampler.setOutputSampleRate(48000.0);
            REQUIRE(sampler.noteOn(0, bank, midi, 1.0f, 0.0f, 0.0f));
            std::array<float, 1> left{};
            std::array<float, 1> right{};
            // Legacy banks normalise to 0.55. The layered bank preserves each
            // capture's peak behind one fixed 0.55 library trim.
            const float threshold = 0.55f * 0.03f
                * (bank == Bank::SteelPicked
                    ? zone->peak / 32768.0f : 1.0f);
            std::size_t firstAudible = 0;
            for (; firstAudible < 480; ++firstAudible) {
                sampler.process(left.data(), right.data(), 1);
                if (std::max(std::abs(left[0]), std::abs(right[0])) >= threshold)
                    break;
            }
            REQUIRE(firstAudible < 240); // no more than 5 ms at 48 kHz
            worstLatencyMs = std::max(worstLatencyMs, 1000.0 * firstAudible / 48000.0);
        }
    }

    Sampler roundRobin(library);
    roundRobin.setOutputSampleRate(48000.0);
    for (std::uint8_t expected = 0; expected < 4; ++expected) {
        REQUIRE(roundRobin.noteOn(0, Bank::SteelPicked, 52,
                                  96.0f / 127.0f, 0.0f, 0.0f));
        REQUIRE(roundRobin.activeZone(0) != nullptr);
        REQUIRE(roundRobin.activeZone(0)->roundRobin == expected);
    }
    REQUIRE(roundRobin.noteOn(0, Bank::SteelPicked, 52,
                              96.0f / 127.0f, 0.0f, 0.0f));
    REQUIRE(roundRobin.activeZone(0)->roundRobin == 0);
    roundRobin.allNotesOff();
    REQUIRE(roundRobin.activeZone(0) == nullptr);
    REQUIRE(roundRobin.noteOn(0, Bank::SteelPicked, 52,
                              96.0f / 127.0f, 0.0f, 0.0f));
    REQUIRE(roundRobin.activeZone(0)->roundRobin == 0);

    // Source takes retain their timbre, while one deterministic one-second
    // trim removes accidental layer/RR loudness reversals and one continuous
    // MIDI curve owns the intended dynamic response.
    std::vector<int> steelRoots;
    for (std::size_t index = 0; index < library.zoneCount(); ++index) {
        const auto* zone = library.zone(index);
        if (zone != nullptr && zone->bank == Bank::SteelPicked
            && std::find(steelRoots.begin(), steelRoots.end(), zone->rootMidi)
                == steelRoots.end())
            steelRoots.push_back(zone->rootMidi);
    }
    steelRoots.erase(std::remove(steelRoots.begin(), steelRoots.end(), 84),
                     steelRoots.end());
    REQUIRE(steelRoots.size() == 16);

    const auto renderedOneSecondRms = [&] (int root, int midi, int velocity,
                                           std::uint8_t wantedRoundRobin) {
        Sampler sampler(library);
        sampler.setOutputSampleRate(44100.0);
        std::array<float, 512> discardLeft{};
        std::array<float, 512> discardRight{};
        for (std::uint8_t rr = 0; rr < wantedRoundRobin; ++rr) {
            REQUIRE(sampler.noteOn(0, Bank::SteelPicked, midi,
                                   velocity / 127.0f, 0.0f, 0.0f));
            sampler.noteOff(0, 1.0f);
            sampler.process(discardLeft.data(), discardRight.data(),
                            discardLeft.size());
            REQUIRE(!sampler.isActive(0));
        }
        REQUIRE(sampler.noteOn(0, Bank::SteelPicked, midi,
                               velocity / 127.0f, 0.0f, 0.0f));
        const auto* zone = sampler.activeZone(0);
        REQUIRE(zone != nullptr && zone->rootMidi == root);
        REQUIRE(zone->roundRobin == wantedRoundRobin);
        REQUIRE(velocity >= zone->lowVelocity && velocity <= zone->highVelocity);
        std::array<float, 256> blockLeft{};
        std::array<float, 256> blockRight{};
        double energy = 0.0;
        int remaining = 44100;
        int rendered = 0;
        while (remaining > 0) {
            const auto count = static_cast<std::size_t>(
                std::min<int>(remaining, blockLeft.size()));
            sampler.process(blockLeft.data(), blockRight.data(), count);
            for (std::size_t sample = 0; sample < count; ++sample) {
                const double mono = 0.5
                    * (blockLeft[sample] + blockRight[sample]);
                energy += mono * mono;
            }
            rendered += static_cast<int>(count);
            remaining -= static_cast<int>(count);
        }
        return std::sqrt(energy / rendered);
    };

    constexpr std::array<int, 7> auditVelocities {
        32, 33, 64, 65, 96, 97, 127
    };
    double worstLayerBoundaryDb = std::numeric_limits<double>::infinity();
    double worstRoundRobinSpreadDb = 0.0;
    for (const int root : steelRoots) {
        const ZoneView* anchor = nullptr;
        for (std::size_t index = 0; index < library.zoneCount(); ++index) {
            const auto* zone = library.zone(index);
            if (zone != nullptr && zone->bank == Bank::SteelPicked
                && zone->rootMidi == root) {
                anchor = zone;
                break;
            }
        }
        REQUIRE(anchor != nullptr);
        const int midi = std::clamp(root, static_cast<int>(anchor->lowKey),
                                    static_cast<int>(anchor->highKey));
        std::array<std::array<double, 4>, auditVelocities.size()> measured{};
        for (std::size_t velocityIndex = 0;
             velocityIndex < auditVelocities.size(); ++velocityIndex)
            for (std::uint8_t rr = 0; rr < 4; ++rr)
                measured[velocityIndex][rr] = renderedOneSecondRms(
                    root, midi, auditVelocities[velocityIndex], rr);

        for (const auto [lowerIndex, upperIndex]
             : { std::pair {0u, 1u}, std::pair {2u, 3u},
                 std::pair {4u, 5u} }) {
            for (std::uint8_t rr = 0; rr < 4; ++rr) {
                const double changeDb = 20.0 * std::log10(
                    measured[upperIndex][rr] / measured[lowerIndex][rr]);
                worstLayerBoundaryDb = std::min(
                    worstLayerBoundaryDb, changeDb);
                REQUIRE(changeDb >= -0.25);
            }
        }
        for (const std::size_t velocityIndex : {0u, 2u, 4u, 6u}) {
            const auto [minimum, maximum] = std::minmax_element(
                measured[velocityIndex].begin(), measured[velocityIndex].end());
            const double spreadDb = 20.0 * std::log10(*maximum / *minimum);
            worstRoundRobinSpreadDb = std::max(
                worstRoundRobinSpreadDb, spreadDb);
            REQUIRE(spreadDb <= 1.5);
        }
    }

    // Replacing a still-ringing physical string must preserve the old sample
    // at the repick boundary; the new note then fades in while the old one
    // retires. A hard slot reset would make this first sample exactly zero.
    Sampler reference(library);
    reference.setOutputSampleRate(48000.0);
    REQUIRE(reference.noteOn(0, Bank::SteelPicked, 52, 0.9f, 0.0f, 0.0f));
    std::array<float, 8192> referenceLeft{};
    std::array<float, 8192> referenceRight{};
    reference.process(referenceLeft.data(), referenceRight.data(),
                      referenceLeft.size());
    std::size_t repickFrame = 1000;
    while (repickFrame < referenceLeft.size()
           && std::max(std::abs(referenceLeft[repickFrame]),
                       std::abs(referenceRight[repickFrame])) < 0.02f)
        ++repickFrame;
    REQUIRE(repickFrame < referenceLeft.size());

    Sampler repicked(library);
    repicked.setOutputSampleRate(48000.0);
    REQUIRE(repicked.noteOn(0, Bank::SteelPicked, 52, 0.9f, 0.0f, 0.0f));
    std::array<float, 8192> discardedLeft{};
    std::array<float, 8192> discardedRight{};
    repicked.process(discardedLeft.data(), discardedRight.data(), repickFrame);
    REQUIRE(repicked.noteOn(0, Bank::SteelPicked, 55, 0.9f, 0.0f, 0.0f));
    std::array<float, 1> repickLeft{};
    std::array<float, 1> repickRight{};
    repicked.process(repickLeft.data(), repickRight.data(), 1);
    REQUIRE(std::abs(repickLeft[0] - referenceLeft[repickFrame]) < 1.0e-6f);
    REQUIRE(std::abs(repickRight[0] - referenceRight[repickFrame]) < 1.0e-6f);

    Sampler realtime(library);
    realtime.setOutputSampleRate(48000.0);
    constexpr std::array<int, 6> chord {40, 45, 50, 55, 59, 64};
    for (std::size_t slot = 0; slot < chord.size(); ++slot)
        REQUIRE(realtime.noteOn(slot, Bank::SteelPicked, chord[slot],
                                0.7f, 0.0f, 0.0f));
    REQUIRE(realtime.activeVoiceCount() == 6);
    std::array<float, 256> left{};
    std::array<float, 256> right{};
    const auto before = allocations.load(std::memory_order_relaxed);
    const auto started = std::chrono::steady_clock::now();
    constexpr std::size_t blocks = 3000;
    for (std::size_t block = 0; block < blocks; ++block) {
        if (block % 375 == 0) {
            for (std::size_t slot = 0; slot < chord.size(); ++slot)
                REQUIRE(realtime.noteOn(slot, Bank::SteelPicked, chord[slot],
                                        0.7f, 0.0f, 0.0f));
        }
        if (block == 1200)
            realtime.setPitchBendSemitones(2.0f);
        realtime.process(left.data(), right.data(), left.size());
    }
    realtime.noteOff(0);
    realtime.process(left.data(), right.data(), left.size());
    const auto stopped = std::chrono::steady_clock::now();
    REQUIRE(allocations.load(std::memory_order_relaxed) == before);
    const auto wallSeconds = std::chrono::duration<double>(stopped - started).count();
    const auto audioSeconds = blocks * left.size() / 48000.0;
    REQUIRE(audioSeconds / wallSeconds > 1.0);

    const auto ratio = static_cast<double>(Library::packedBytes()) / library.decodedBytes();
    REQUIRE(ratio < 0.80);
    std::cout << "PASS zones=321 worst_note_on_ms=" << worstLatencyMs
              << " decoded_mib=" << library.decodedBytes() / 1048576.0
              << " packed_mib=" << Library::packedBytes() / 1048576.0
              << " packed_ratio=" << ratio
              << " worst_attack_loss_db=" << worstFirstMillisecondLossDb
              << " worst_ramp_derivative_ratio=" << worstRampToSourceDerivative
              << " worst_layer_boundary_db=" << worstLayerBoundaryDb
              << " worst_rr_spread_db=" << worstRoundRobinSpreadDb
              << " render_x_realtime=" << audioSeconds / wallSeconds << '\n';
}

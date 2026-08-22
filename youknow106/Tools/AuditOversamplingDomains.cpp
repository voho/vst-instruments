// Oversampling work-attribution harness.
//
// This translation unit is deliberately built twice.  The normal executable
// links the shipping DSP library and measures uninstrumented thread CPU time.
// The work executable links a separately compiled YOUKNOW106_WORK_AUDIT DSP
// library and records deterministic semantic work.  Both expose the same
// raw-float fingerprint mode so CTest can prove that observation is inert.

#include "DSP/YouKnow106Engine.h"
#include "OversamplingAuditSupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <time.h>
#endif

namespace
{

using youknow106::ChorusMode;
using youknow106::EngineParameters;
using youknow106::HighPassMode;
using youknow106::YouKnow106Engine;
using youknow106::VcfTanhMode;
using youknow106::oversampling_audit::DomainWorkCounters;

constexpr int blockSize = 256;
constexpr int preRollSeconds = 2;
constexpr int timingBlocks = 128;
constexpr int fingerprintBlocks = 16;
constexpr int timingRepetitions = 7;
[[maybe_unused]] constexpr int counterBlocks = 8;
constexpr std::uint64_t fnvOffset = 14695981039346656037ull;
constexpr std::uint64_t fnvPrime = 1099511628211ull;
constexpr std::array<int, 6> chordNotes { 36, 48, 55, 60, 64, 67 };

enum class ScenarioKind
{
    IdleDry,
    SixVoicePlainDry,
    SixVoiceResonantDry,
    SixVoiceFullMixerChorusTwo
};

struct Scenario
{
    ScenarioKind kind;
    std::string_view name;
    bool holdChord;
};

constexpr std::array scenarios {
    Scenario { ScenarioKind::IdleDry, "idle-dry", false },
    Scenario { ScenarioKind::SixVoicePlainDry, "six-voice-plain-dry", true },
    Scenario { ScenarioKind::SixVoiceResonantDry,
               "six-voice-resonant-dry", true },
    Scenario { ScenarioKind::SixVoiceFullMixerChorusTwo,
               "six-voice-full-mixer-chorus-ii", true },
};

EngineParameters parametersFor(ScenarioKind kind,
                               VcfTanhMode tanhMode = VcfTanhMode::Exact)
{
    EngineParameters parameters;
    parameters.vcfTanhMode = tanhMode;
    // Match the engine suite's long-lived plainPatch fixture, except that the
    // audit exercises the shipped Unit Character setting rather than its
    // deliberately pristine Character-zero reference.
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.decay = 1.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.0f;
    parameters.vcaLevel = 99.0f / 127.0f;
    parameters.volume = 1.0f;
    parameters.calibration = 1.0f;
    parameters.chorusNoise = 0.0f;
    parameters.polyphony = 6;

    switch (kind)
    {
        case ScenarioKind::IdleDry:
            parameters.cutoff = 0.62f;
            parameters.resonance = 0.10f;
            parameters.chorus = ChorusMode::Off;
            break;
        case ScenarioKind::SixVoicePlainDry:
            parameters.cutoff = 0.62f;
            parameters.resonance = 0.10f;
            parameters.chorus = ChorusMode::Off;
            break;
        case ScenarioKind::SixVoiceResonantDry:
            parameters.cutoff = 0.62f;
            parameters.resonance = 0.95f;
            parameters.chorus = ChorusMode::Off;
            break;
        case ScenarioKind::SixVoiceFullMixerChorusTwo:
            // Reuse testCpuBudget's representative full-path panel shape;
            // this audit deliberately keeps its declared Character-one
            // profile, chord and Chorus Noise setting rather than claiming
            // the two timing protocols are identical.
            parameters.cutoff = 1.0f;
            parameters.resonance = 0.70f;
            parameters.pulseEnabled = true;
            parameters.subLevel = 0.50f;
            parameters.noiseLevel = 0.10f;
            parameters.chorus = ChorusMode::Two;
            parameters.chorusNoise = 1.0f;
            break;
    }
    return parameters;
}

void renderBlocks(YouKnow106Engine& engine, int blocks,
                  std::vector<float>* capturedLeft = nullptr,
                  std::vector<float>* capturedRight = nullptr)
{
    std::array<float, blockSize> scratchLeft {};
    std::array<float, blockSize> scratchRight {};

    if ((capturedLeft == nullptr) != (capturedRight == nullptr))
        throw std::logic_error("both capture channels must be present");

    if (capturedLeft != nullptr)
    {
        capturedLeft->resize(static_cast<std::size_t>(blocks * blockSize));
        capturedRight->resize(static_cast<std::size_t>(blocks * blockSize));
    }

    for (int block = 0; block < blocks; ++block)
    {
        float* left = scratchLeft.data();
        float* right = scratchRight.data();
        if (capturedLeft != nullptr)
        {
            const auto offset = static_cast<std::size_t>(block * blockSize);
            left = capturedLeft->data() + offset;
            right = capturedRight->data() + offset;
        }
        engine.process(left, right, blockSize);
    }
}

struct PreparedSnapshot
{
    YouKnow106Engine engine;
    int factor {};
};

PreparedSnapshot prepareSnapshot(const Scenario& scenario, int sampleRate,
                                 int requestedFactor,
                                 VcfTanhMode tanhMode = VcfTanhMode::Exact)
{
    PreparedSnapshot snapshot;
    snapshot.engine.prepare(static_cast<double>(sampleRate), blockSize,
                            requestedFactor);
    snapshot.engine.setParameters(parametersFor(scenario.kind, tanhMode));
    if (scenario.holdChord)
        for (const int note : chordNotes)
            snapshot.engine.noteOn(note, 1.0f);

    const int preRollFrames = sampleRate * preRollSeconds;
    renderBlocks(snapshot.engine, preRollFrames / blockSize);
    const int remainder = preRollFrames % blockSize;
    if (remainder != 0)
    {
        std::array<float, blockSize> left {};
        std::array<float, blockSize> right {};
        snapshot.engine.process(left.data(), right.data(), remainder);
    }

    snapshot.factor = snapshot.engine.getOversamplingFactor();
    if (scenario.holdChord && snapshot.engine.getActiveVoiceCount() != 6)
        throw std::runtime_error("six-voice audit chord was not fully assigned");
    return snapshot;
}

void hashByte(std::uint64_t& hash, std::uint8_t byte) noexcept
{
    hash ^= byte;
    hash *= fnvPrime;
}

void hashFloat(std::uint64_t& hash, float sample) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);
    for (unsigned int shift = 0; shift < 32; shift += 8)
        hashByte(hash, static_cast<std::uint8_t>((bits >> shift) & 0xffu));
}

std::uint64_t hashAudio(const std::vector<float>& left,
                        const std::vector<float>& right)
{
    if (left.size() != right.size())
        throw std::logic_error("fingerprint channels have unequal lengths");

    std::uint64_t hash = fnvOffset;
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        hashFloat(hash, left[frame]);
        hashFloat(hash, right[frame]);
    }
    return hash;
}

std::string hexHash(std::uint64_t hash)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

std::uint64_t renderFingerprint(const PreparedSnapshot& snapshot, int blocks)
{
    // Copying a fully pre-rolled state is intentionally outside every timing
    // interval.  Fingerprint mode uses the same rule even though it has no
    // clock, so it exercises the exact state used by the timing path.
    YouKnow106Engine engine = snapshot.engine;
    std::vector<float> left;
    std::vector<float> right;
#if defined(YOUKNOW106_WORK_AUDIT)
    // Fingerprint the actively observed path, not merely an instrumented
    // binary whose nullable sink happens to be disabled.  This is the path
    // CTest compares through the same raw-float fingerprint as the shipping
    // build.
    DomainWorkCounters counters;
    youknow106::oversampling_audit::ScopedDomainWorkCounterSink sink(counters);
#endif
    renderBlocks(engine, blocks, &left, &right);
    return hashAudio(left, right);
}

class CpuClock
{
public:
    CpuClock() : useThreadClock_(probeThreadClock()) {}

    [[nodiscard]] double now() const
    {
        double seconds = 0.0;
        if (useThreadClock_)
        {
            if (!readThreadClock(seconds))
                throw std::runtime_error("thread CPU clock failed after probing");
            return seconds;
        }

        const std::clock_t ticks = std::clock();
        if (ticks == static_cast<std::clock_t>(-1))
            throw std::runtime_error("process CPU clock is unavailable");
        return static_cast<double>(ticks) / static_cast<double>(CLOCKS_PER_SEC);
    }

    [[nodiscard]] std::string_view name() const noexcept
    {
        return useThreadClock_ ? "thread-cpu" : "process-cpu-fallback";
    }

private:
    static bool probeThreadClock() noexcept
    {
        double ignored = 0.0;
        return readThreadClock(ignored);
    }

    static bool readThreadClock(double& seconds) noexcept
    {
#if defined(_WIN32)
        FILETIME creation {};
        FILETIME exit {};
        FILETIME kernel {};
        FILETIME user {};
        if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user)
            == 0)
            return false;
        const auto ticks = [](FILETIME value) {
            return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32u)
                 | static_cast<std::uint64_t>(value.dwLowDateTime);
        };
        seconds = static_cast<double>(ticks(kernel) + ticks(user)) * 1.0e-7;
        return true;
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
        timespec value {};
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0)
            return false;
        seconds = static_cast<double>(value.tv_sec)
                + static_cast<double>(value.tv_nsec) * 1.0e-9;
        return true;
#else
        static_cast<void>(seconds);
        return false;
#endif
    }

    bool useThreadClock_ {};
};

struct TimedRun
{
    double cpuSeconds {};
    std::uint64_t fingerprint {};
};

TimedRun renderTimed(const PreparedSnapshot& snapshot, const CpuClock& clock)
{
    // The state copy and output allocation are setup costs, not audio-domain
    // work, and therefore sit outside the measured interval.
    YouKnow106Engine engine = snapshot.engine;
    std::vector<float> left(static_cast<std::size_t>(timingBlocks * blockSize));
    std::vector<float> right(static_cast<std::size_t>(timingBlocks * blockSize));

    const double start = clock.now();
    for (int block = 0; block < timingBlocks; ++block)
    {
        const auto offset = static_cast<std::size_t>(block * blockSize);
        engine.process(left.data() + offset, right.data() + offset, blockSize);
    }
    const double end = clock.now();

    return { end - start, hashAudio(left, right) };
}

double median(std::vector<double> values)
{
    if (values.empty())
        throw std::logic_error("cannot summarize an empty sample");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 != 0
        ? values[middle]
        : 0.5 * (values[middle - 1] + values[middle]);
}

struct Summary
{
    double median {};
    double minimum {};
    double mad {};
};

Summary summarize(const std::vector<double>& values)
{
    const double centre = median(values);
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (const double value : values)
        deviations.push_back(std::abs(value - centre));
    return { centre, *std::min_element(values.begin(), values.end()),
             median(std::move(deviations)) };
}

void requireStableHash(const std::vector<std::uint64_t>& hashes,
                       std::string_view scenario, std::string_view quality)
{
    if (hashes.empty()
        || !std::all_of(hashes.begin(), hashes.end(),
                        [first = hashes.front()](std::uint64_t value) {
                            return value == first;
                        }))
    {
        throw std::runtime_error(std::string(scenario) + " "
                                 + std::string(quality)
                                 + " changed output between repetitions");
    }
}

int printFingerprints()
{
    for (const auto& scenario : scenarios)
    {
        for (const int requested : { 4, 2, 1 })
        {
            const auto snapshot = prepareSnapshot(scenario, 48000, requested);
            const int expectedFactor = requested;
            if (snapshot.factor != expectedFactor)
                throw std::runtime_error("48 kHz quality selected an unexpected factor");
            std::cout << scenario.name << " " << snapshot.factor << "x "
                      << hexHash(renderFingerprint(snapshot, fingerprintBlocks))
                      << "\n";
        }
    }
    return 0;
}

[[maybe_unused]] int printTimingReport()
{
    const CpuClock clock;
    const double renderedSeconds = static_cast<double>(timingBlocks * blockSize)
                                 / 48000.0;
    std::cout << "protocol host_rate=48000 block_size=" << blockSize
              << " preroll_seconds=" << preRollSeconds
              << " timed_blocks=" << timingBlocks
              << " repetitions=" << timingRepetitions
              << " order=paired-alternating snapshot_copy=outside-timer"
              << " clock=" << clock.name()
              << " fallback=process-cpu hash=fnv1a64-raw-interleaved-f32\n";
    std::cout << "schema timing scenario quality fingerprint median_cpu_ms"
                 " min_cpu_ms mad_cpu_ms median_cpu_x_realtime\n";
    std::cout << "schema paired_ratio scenario rung_pair median min mad\n";

    for (const auto& scenario : scenarios)
    {
        // Every rung of the ladder, not just its ends: the middle one is what a
        // player picks when 4x will not fit and 1x gives up more than they want
        // to, so its cost has to be measured rather than assumed halfway.
        constexpr std::size_t rungs = YouKnow106Engine::oversampleFactors.size();
        std::array<PreparedSnapshot, rungs> snapshots {
            prepareSnapshot(scenario, 48000, 4),
            prepareSnapshot(scenario, 48000, 2),
            prepareSnapshot(scenario, 48000, 1)
        };
        constexpr std::array<int, rungs> expectedFactors { 4, 2, 1 };
        constexpr std::array<std::string_view, rungs> rungNames { "4x", "2x", "1x" };
        for (std::size_t rung = 0; rung < rungs; ++rung)
            if (snapshots[rung].factor != expectedFactors[rung])
                throw std::runtime_error("48 kHz audit did not resolve to "
                                         + std::string(rungNames[rung]));

        std::array<std::vector<double>, rungs> times;
        std::array<std::vector<std::uint64_t>, rungs> hashes;
        // Each deeper rung against the cheapest one, measured inside the same
        // repetition so a machine that speeds up or throttles midway biases
        // both halves of the ratio equally.
        std::array<std::vector<double>, rungs> pairedRatios;
        for (std::size_t rung = 0; rung < rungs; ++rung)
        {
            times[rung].reserve(timingRepetitions);
            hashes[rung].reserve(timingRepetitions);
            pairedRatios[rung].reserve(timingRepetitions);
        }
        constexpr std::size_t cheapest = rungs - 1;

        for (int repetition = 0; repetition < timingRepetitions; ++repetition)
        {
            // Alternate the visiting order so no rung permanently owns the
            // warmed-up half of a repetition.
            std::array<TimedRun, rungs> runs;
            for (std::size_t step = 0; step < rungs; ++step)
            {
                const std::size_t rung = repetition % 2 == 0
                                       ? step : rungs - 1 - step;
                runs[rung] = renderTimed(snapshots[rung], clock);
            }
            for (std::size_t rung = 0; rung < rungs; ++rung)
            {
                times[rung].push_back(runs[rung].cpuSeconds);
                hashes[rung].push_back(runs[rung].fingerprint);
                pairedRatios[rung].push_back(
                    runs[rung].cpuSeconds
                    / std::max(runs[cheapest].cpuSeconds, 1.0e-12));
            }
        }

        const auto printRow = [&](std::string_view quality,
                                  std::uint64_t fingerprint,
                                  const Summary& summary) {
            std::cout << "timing " << scenario.name << " " << quality << " "
                      << hexHash(fingerprint) << " " << std::fixed
                      << std::setprecision(3)
                      << summary.median * 1000.0 << " "
                      << summary.minimum * 1000.0 << " "
                      << summary.mad * 1000.0 << " "
                      << summary.median / renderedSeconds << "\n";
        };
        for (std::size_t rung = 0; rung < rungs; ++rung)
        {
            requireStableHash(hashes[rung], scenario.name, rungNames[rung]);
            printRow(rungNames[rung], hashes[rung].front(),
                     summarize(times[rung]));
        }
        for (std::size_t rung = 0; rung < cheapest; ++rung)
        {
            const auto ratioSummary = summarize(pairedRatios[rung]);
            std::cout << "paired_ratio " << scenario.name << " "
                      << rungNames[rung] << "-over-" << rungNames[cheapest]
                      << " " << std::fixed << std::setprecision(3)
                      << ratioSummary.median << " " << ratioSummary.minimum
                      << " " << ratioSummary.mad << "\n";
        }
        for (std::size_t rung = 0; rung < rungs; ++rung)
        {
            std::cout << "runs_cpu_ms " << scenario.name << " "
                      << rungNames[rung];
            for (const double seconds : times[rung])
                std::cout << " " << std::fixed << std::setprecision(3)
                          << seconds * 1000.0;
            std::cout << "\n";
        }
    }
    return 0;
}

[[maybe_unused]] int printTanhTimingReport()
{
    const CpuClock clock;
    const double renderedSeconds = static_cast<double>(timingBlocks * blockSize)
                                 / 48000.0;
    std::cout << "protocol host_rate=48000 quality=4x block_size=" << blockSize
              << " preroll_seconds=" << preRollSeconds
              << " timed_blocks=" << timingBlocks
              << " repetitions=" << timingRepetitions
              << " order=paired-alternating snapshot_copy=outside-timer"
              << " clock=" << clock.name()
              << " hash=fnv1a64-raw-interleaved-f32\n";
    std::cout << "schema tanh_timing scenario mode fingerprint median_cpu_ms"
                 " min_cpu_ms mad_cpu_ms median_cpu_x_realtime\n";
    std::cout << "schema tanh_speedup scenario exact_over_fast median min mad\n";

    constexpr std::array modes { VcfTanhMode::Exact,
                                 VcfTanhMode::Hermite512 };
    constexpr std::array<std::string_view, modes.size()> modeNames {
        "exact", "hermite512"
    };

    for (const auto& scenario : scenarios)
    {
        std::array<PreparedSnapshot, modes.size()> snapshots {
            prepareSnapshot(scenario, 48000, 4, modes[0]),
            prepareSnapshot(scenario, 48000, 4, modes[1])
        };
        for (const auto& snapshot : snapshots)
            if (snapshot.factor != 4)
                throw std::runtime_error(
                    "48 kHz tanh audit did not resolve to 4x");

        std::array<std::vector<double>, modes.size()> times;
        std::array<std::vector<std::uint64_t>, modes.size()> hashes;
        std::vector<double> speedups;
        speedups.reserve(timingRepetitions);
        for (auto& values : times)
            values.reserve(timingRepetitions);
        for (auto& values : hashes)
            values.reserve(timingRepetitions);

        for (int repetition = 0; repetition < timingRepetitions; ++repetition)
        {
            std::array<TimedRun, modes.size()> runs;
            for (std::size_t step = 0; step < modes.size(); ++step)
            {
                const std::size_t mode = repetition % 2 == 0
                                       ? step : modes.size() - 1 - step;
                runs[mode] = renderTimed(snapshots[mode], clock);
            }
            for (std::size_t mode = 0; mode < modes.size(); ++mode)
            {
                times[mode].push_back(runs[mode].cpuSeconds);
                hashes[mode].push_back(runs[mode].fingerprint);
            }
            speedups.push_back(runs[0].cpuSeconds
                               / std::max(runs[1].cpuSeconds, 1.0e-12));
        }

        for (std::size_t mode = 0; mode < modes.size(); ++mode)
        {
            requireStableHash(hashes[mode], scenario.name, modeNames[mode]);
            const auto summary = summarize(times[mode]);
            std::cout << "tanh_timing " << scenario.name << " "
                      << modeNames[mode] << " "
                      << hexHash(hashes[mode].front()) << " " << std::fixed
                      << std::setprecision(3)
                      << summary.median * 1000.0 << " "
                      << summary.minimum * 1000.0 << " "
                      << summary.mad * 1000.0 << " "
                      << summary.median / renderedSeconds << "\n";
        }
        const auto speedup = summarize(speedups);
        std::cout << "tanh_speedup " << scenario.name
                  << " exact-over-hermite512 " << std::fixed
                  << std::setprecision(3) << speedup.median << " "
                  << speedup.minimum << " " << speedup.mad << "\n";
    }
    return 0;
}

#if defined(YOUKNOW106_WORK_AUDIT)

struct CounterCase
{
    std::string_view name;
    int sampleRate;
    int requestedFactor;
    int expectedFactor;
    std::uint64_t expectedPassiveHoldEvents;
    std::uint64_t expectedFractionalEvents;
    std::uint64_t expectedExactControlIntervals;
};

constexpr std::array counterCases {
    // The final three fields freeze all sixteen passive fractional events,
    // the RES+six-VCF subset and affected VCF-card intervals over this exact
    // 2,048-host-frame window. A resonance event owns six physical-card
    // intervals; a VCF event owns one. Keeping both totals makes Step 11's VCF
    // work a golden subset of the broadened Step 12 scheduler.
    CounterCase { "44.1k-4x", 44100, 4, 4, 176u, 77u, 132u },
    CounterCase { "44.1k-2x", 44100, 2, 2, 176u, 77u, 132u },
    CounterCase { "44.1k-1x", 44100, 1, 1, 176u, 77u, 132u },
    CounterCase { "48k-4x", 48000, 4, 4, 160u, 70u, 120u },
    CounterCase { "48k-2x", 48000, 2, 2, 160u, 70u, 120u },
    CounterCase { "48k-1x", 48000, 1, 1, 160u, 70u, 120u },
    CounterCase { "88.2k-4x", 88200, 4, 2, 88u, 39u, 64u },
    CounterCase { "88.2k-2x", 88200, 2, 2, 88u, 39u, 64u },
    CounterCase { "88.2k-1x", 88200, 1, 1, 88u, 39u, 64u },
    CounterCase { "96k-4x", 96000, 4, 2, 80u, 35u, 60u },
    CounterCase { "96k-2x", 96000, 2, 2, 80u, 35u, 60u },
    CounterCase { "96k-1x", 96000, 1, 1, 80u, 35u, 60u },
    CounterCase { "176.4k-4x", 176400, 4, 1, 45u, 20u, 30u },
    CounterCase { "192k-4x", 192000, 4, 1, 40u, 18u, 28u },
};

struct CounterRun
{
    DomainWorkCounters counters;
    std::uint64_t fingerprint {};
};

CounterRun runCounterCase(const CounterCase& testCase)
{
    const auto scenario = scenarios[2]; // six-voice resonant dry
    const auto snapshot = prepareSnapshot(scenario, testCase.sampleRate,
                                          testCase.requestedFactor);
    if (snapshot.factor != testCase.expectedFactor)
        throw std::runtime_error(std::string(testCase.name)
                                 + " selected an unexpected factor");

    YouKnow106Engine engine = snapshot.engine;
    std::vector<float> left;
    std::vector<float> right;
    CounterRun result;
    {
        youknow106::oversampling_audit::ScopedDomainWorkCounterSink sink(
            result.counters);
        renderBlocks(engine, counterBlocks, &left, &right);
    }
    result.fingerprint = hashAudio(left, right);
    return result;
}

void expectEqual(std::vector<std::string>& errors, std::string_view caseName,
                 std::string_view field, std::uint64_t actual,
                 std::uint64_t expected)
{
    if (actual == expected)
        return;
    std::ostringstream stream;
    stream << caseName << ": " << field << "=" << actual
           << ", expected " << expected;
    errors.push_back(stream.str());
}

void expectTrue(std::vector<std::string>& errors, std::string_view caseName,
                std::string_view expression, bool condition)
{
    if (!condition)
        errors.push_back(std::string(caseName) + ": "
                         + std::string(expression));
}

void validateVcfCounterAlgebra(std::string_view caseName,
                               const DomainWorkCounters& counters,
                               std::uint64_t expectedSteps,
                               std::uint64_t expectedPassiveHoldEvents,
                               std::uint64_t expectedFractionalEvents,
                               std::uint64_t expectedExactControlIntervals,
                               std::vector<std::string>& errors)
{
    expectEqual(errors, caseName, "vcfSteps",
                counters.vcfSteps, expectedSteps);
    expectEqual(errors, caseName, "VCF Merson half-steps",
                counters.vcfIntegrationSubsteps,
                2u * counters.vcfSteps);
    expectEqual(errors, caseName, "VCF RHS evaluations",
                counters.vcfRhsEvaluations,
                5u * counters.vcfIntegrationSubsteps);
    expectEqual(errors, caseName, "VCF stage evaluations",
                counters.vcfStageEvaluations,
                4u * counters.vcfRhsEvaluations);
    expectEqual(errors, caseName, "VCF feedback evaluations",
                counters.vcfFeedbackEvaluations,
                counters.vcfRhsEvaluations);
    expectEqual(errors, caseName, "VCF Early-effect evaluations",
                counters.vcfEarlyEvaluations,
                counters.vcfStageEvaluations);
    expectEqual(errors, caseName, "VCF input reconstructions",
                counters.vcfInputReconstructions,
                7u * counters.vcfSteps);
    expectEqual(errors, caseName, "passive-hold fractional event peeks",
                counters.passiveHoldFractionalEventPeeks,
                expectedPassiveHoldEvents);
    expectEqual(errors, caseName, "passive-hold fractional target commits",
                counters.passiveHoldFractionalTargetCommits,
                expectedPassiveHoldEvents);
    expectEqual(errors, caseName, "VCF exact control nodes",
                counters.vcfExactControlNodes,
                7u * counters.vcfExactControlIntervals);
    expectEqual(errors, caseName, "VCF exact control maps",
                counters.vcfExactControlMaps,
                6u * counters.vcfExactControlIntervals);
    expectEqual(errors, caseName, "fractional VCF event peeks",
                counters.vcfFractionalEventPeeks,
                expectedFractionalEvents);
    expectEqual(errors, caseName, "fractional VCF target commits",
                counters.vcfFractionalTargetCommits,
                expectedFractionalEvents);
    expectEqual(errors, caseName, "exact VCF control intervals",
                counters.vcfExactControlIntervals,
                expectedExactControlIntervals);
    expectEqual(errors, caseName, "vcfRecoveries",
                counters.vcfRecoveries, 0u);
}

void validateCounterAlgebra(const CounterCase& testCase,
                            const DomainWorkCounters& counters,
                            std::vector<std::string>& errors)
{
    const std::uint64_t host = static_cast<std::uint64_t>(
        counterBlocks * blockSize);
    const std::uint64_t internal = host
        * static_cast<std::uint64_t>(testCase.expectedFactor);
    const std::uint64_t voiceCards = 6u * internal;
    const std::uint64_t voiceSlots = 16u * internal;
    const bool exactInputSelected =
        static_cast<std::uint64_t>(testCase.sampleRate)
                * static_cast<std::uint64_t>(testCase.expectedFactor)
            >= static_cast<std::uint64_t>(
                youknow106::Chorus::minimumExactInputSupportRate);
    const std::uint64_t bbdLineFrames = 2u * internal;
    // The input support network is shared by both wet branches and advanced
    // once per internal frame; only the two output chains are per line.
    const std::uint64_t exactInputAdvances = exactInputSelected
        ? internal : 0u;
    const std::uint64_t legacyInputFrames = exactInputSelected
        ? 0u : internal;
    const std::uint64_t exactOutputAdvances = bbdLineFrames;
    const std::uint64_t exactAdvances = exactInputAdvances
                                      + exactOutputAdvances;
    const std::uint64_t decimatorCalls = testCase.expectedFactor == 4
        ? 3u * host
        : (testCase.expectedFactor == 2 ? host : 0u);

    expectEqual(errors, testCase.name, "hostFrames",
                counters.hostFrames, host);
    expectEqual(errors, testCase.name, "internalFrames",
                counters.internalFrames, internal);
    expectEqual(errors, testCase.name, "scanPolls",
                counters.scanPolls, internal);
    expectEqual(errors, testCase.name, "holdVoiceUpdates",
                counters.holdVoiceUpdates, voiceSlots);
    // Both of these are behind one guard, because both feed a `renderVoice`
    // that returns before it reads either for an inactive extension slot.
    // This fixture holds no notes, so only the six powered cards run: the
    // comparator used to run for all sixteen slots and discard ten of them.
    expectEqual(errors, testCase.name, "pulseComparatorUpdates",
                counters.pulseComparatorUpdates, voiceCards);
    expectEqual(errors, testCase.name, "voiceAudioUpdates",
                counters.voiceAudioUpdates, voiceCards);
    expectEqual(errors, testCase.name, "comparator/audio partition",
                counters.pulseComparatorUpdates, counters.voiceAudioUpdates);
    expectEqual(errors, testCase.name, "dcoFrames",
                counters.dcoFrames, voiceCards);
    expectEqual(errors, testCase.name, "chorusFrames",
                counters.chorusFrames, internal);
    expectEqual(errors, testCase.name, "bbdLineFrames",
                counters.bbdLineFrames, bbdLineFrames);
    expectEqual(errors, testCase.name, "bbdLegacyInputSupportFrames",
                counters.bbdLegacyInputSupportFrames, legacyInputFrames);
    expectEqual(errors, testCase.name, "bbdExactInputSupportAdvances",
                counters.bbdExactInputSupportAdvances, exactInputAdvances);
    expectEqual(errors, testCase.name, "bbdExactOutputSupportAdvances",
                counters.bbdExactOutputSupportAdvances, exactOutputAdvances);
    expectEqual(errors, testCase.name, "bbdExactSupportCoordinateUpdates",
                counters.bbdExactSupportCoordinateUpdates,
                6u * exactAdvances);
    expectEqual(errors, testCase.name, "bbdExactSupportMacs",
                counters.bbdExactSupportMacs, 60u * exactAdvances);
    expectEqual(errors, testCase.name, "decimatorCalls",
                counters.decimatorCalls, decimatorCalls);
    expectEqual(errors, testCase.name, "decimatorNonzeroTapVisits",
                counters.decimatorNonzeroTapVisits, 49u * decimatorCalls);
    expectEqual(errors, testCase.name, "decimatorStereoMacs",
                counters.decimatorStereoMacs, 98u * decimatorCalls);

    expectEqual(errors, testCase.name, "cutoff memo partition",
                counters.cutoffMemoHits + counters.cutoffMemoMisses,
                counters.voiceAudioUpdates);
    validateVcfCounterAlgebra(
        testCase.name, counters, voiceCards,
        testCase.expectedPassiveHoldEvents,
        testCase.expectedFractionalEvents,
        testCase.expectedExactControlIntervals, errors);

    // Ten extension slots receive one digital scan update at each pass start,
    // even though this six-voice contract does not render their audio cards.
    expectEqual(errors, testCase.name, "extensionScanUpdates",
                counters.extensionScanUpdates,
                10u * counters.converterPassStarts);

    // The counted window may begin or end part-way through the service-chart
    // write sequence.  At most one 23-write pass can straddle either boundary.
    const std::uint64_t completedPassWrites =
        23u * counters.converterPassStarts;
    const std::uint64_t difference = counters.converterWrites
        > completedPassWrites
        ? counters.converterWrites - completedPassWrites
        : completedPassWrites - counters.converterWrites;
    expectTrue(errors, testCase.name,
               "converter writes differ from pass starts by more than one pass",
               difference <= 23u);
}

int runCounterAudit(bool selfTestOnly)
{
    std::vector<std::string> errors;
    std::array<CounterRun, counterCases.size()> observed {};
    if (!selfTestOnly)
    {
        std::cout << "protocol block_size=" << blockSize
                  << " preroll_seconds=" << preRollSeconds
                  << " counted_blocks=" << counterBlocks
                  << " scenario=six-voice-resonant-dry"
                  << " counters=semantic hash=fnv1a64-raw-interleaved-f32\n";
    }

    for (std::size_t index = 0; index < counterCases.size(); ++index)
    {
        const auto& testCase = counterCases[index];
        const auto run = runCounterCase(testCase);
        observed[index] = run;
        validateCounterAlgebra(testCase, run.counters, errors);
        if (!selfTestOnly)
        {
            std::cout << "case=" << testCase.name
                      << " sample_rate=" << testCase.sampleRate
                      << " factor=" << testCase.expectedFactor
                      << "x fingerprint=" << hexHash(run.fingerprint);
            for (const auto& descriptor :
                 youknow106::oversampling_audit::counterDescriptors)
                std::cout << " " << descriptor.name << "="
                          << run.counters.*(descriptor.member);
            std::cout << "\n";
        }
    }

    // Equal wall time at a fixed host rate crosses the same physical
    // converter events regardless of whether the audio domain runs q1, q2 or
    // q4. These four pairs make that invariance explicit instead of relying
    // only on their individually frozen totals. The VCF subset must remain
    // invariant too; its Merson/BBD work is already checked above per actual
    // internal frame, so broadening the passive latch cannot add a retry.
    // Derived from the table rather than written out as fixed index pairs: a
    // rung added to the ladder must be compared against its own host rate, not
    // against whatever row a frozen index happens to land on afterwards. Each
    // case is measured against the first case sharing its sample rate.
    std::vector<std::array<std::size_t, 2>> factorPairs;
    for (std::size_t index = 1; index < counterCases.size(); ++index)
        for (std::size_t first = 0; first < index; ++first)
            if (counterCases[first].sampleRate == counterCases[index].sampleRate)
            {
                factorPairs.push_back({ first, index });
                break;
            }
    if (factorPairs.empty())
        throw std::runtime_error("no host rate carries more than one rung");
    for (const auto& pair : factorPairs)
    {
        const auto& high = observed[pair[0]].counters;
        const auto& one = observed[pair[1]].counters;
        const std::string pairName = std::string(counterCases[pair[0]].name)
                                   + " vs "
                                   + std::string(counterCases[pair[1]].name);
        expectEqual(errors, pairName, "passive event factor invariance",
                    high.passiveHoldFractionalEventPeeks,
                    one.passiveHoldFractionalEventPeeks);
        expectEqual(errors, pairName, "passive commit factor invariance",
                    high.passiveHoldFractionalTargetCommits,
                    one.passiveHoldFractionalTargetCommits);
        expectEqual(errors, pairName, "VCF event factor invariance",
                    high.vcfFractionalEventPeeks,
                    one.vcfFractionalEventPeeks);
        expectEqual(errors, pairName, "VCF commit factor invariance",
                    high.vcfFractionalTargetCommits,
                    one.vcfFractionalTargetCommits);
        expectEqual(errors, pairName, "exact VCF interval factor invariance",
                    high.vcfExactControlIntervals,
                    one.vcfExactControlIntervals);
    }

    if (!errors.empty())
    {
        for (const auto& error : errors)
            std::cerr << "FAIL: " << error << "\n";
        return 1;
    }
    if (selfTestOnly)
        std::cout << "oversampling work-counter algebra and passive "
                     "equal-wall-time factor invariance: PASS\n";
    return 0;
}

#endif

void printUsage(const char* executable)
{
    std::cout << "usage: " << executable
              << " [--fingerprint|--tanh-benchmark|--self-test|--help]\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--fingerprint")
            return printFingerprints();
        if (argc == 2 && std::string_view(argv[1]) == "--tanh-benchmark")
        {
#if defined(YOUKNOW106_WORK_AUDIT)
            std::cerr << "--tanh-benchmark requires YouKnow106OversamplingAudit\n";
            return 2;
#else
            return printTanhTimingReport();
#endif
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test")
        {
#if defined(YOUKNOW106_WORK_AUDIT)
            return runCounterAudit(true);
#else
            std::cerr << "--self-test requires YouKnow106OversamplingWorkAudit\n";
            return 2;
#endif
        }
        if (argc != 1)
        {
            printUsage(argv[0]);
            return 2;
        }

#if defined(YOUKNOW106_WORK_AUDIT)
        return runCounterAudit(false);
#else
        return printTimingReport();
#endif
    }
    catch (const std::exception& error)
    {
        std::cerr << "oversampling audit failed: " << error.what() << "\n";
        return 1;
    }
}

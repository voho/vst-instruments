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

EngineParameters parametersFor(ScenarioKind kind)
{
    EngineParameters parameters;
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
                                 bool highQuality)
{
    PreparedSnapshot snapshot;
    snapshot.engine.prepare(static_cast<double>(sampleRate), blockSize,
                            highQuality);
    snapshot.engine.setParameters(parametersFor(scenario.kind));
    if (scenario.holdChord)
        for (const int note : chordNotes)
            snapshot.engine.noteOn(note, 1.0f);

    const int preRollFrames = sampleRate * preRollSeconds;
    if (preRollFrames % blockSize != 0)
        throw std::logic_error("pre-roll must contain complete 256-frame blocks");
    renderBlocks(snapshot.engine, preRollFrames / blockSize);

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
        for (const bool hq : { true, false })
        {
            const auto snapshot = prepareSnapshot(scenario, 48000, hq);
            const int expectedFactor = hq ? 4 : 1;
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
    std::cout << "schema paired_ratio scenario median min mad\n";

    for (const auto& scenario : scenarios)
    {
        const auto four = prepareSnapshot(scenario, 48000, true);
        const auto one = prepareSnapshot(scenario, 48000, false);
        if (four.factor != 4 || one.factor != 1)
            throw std::runtime_error("48 kHz audit did not resolve to 4x/1x");

        std::vector<double> fourTimes;
        std::vector<double> oneTimes;
        std::vector<double> pairedRatios;
        std::vector<std::uint64_t> fourHashes;
        std::vector<std::uint64_t> oneHashes;
        fourTimes.reserve(timingRepetitions);
        oneTimes.reserve(timingRepetitions);
        pairedRatios.reserve(timingRepetitions);

        for (int repetition = 0; repetition < timingRepetitions; ++repetition)
        {
            TimedRun fourRun;
            TimedRun oneRun;
            if (repetition % 2 == 0)
            {
                fourRun = renderTimed(four, clock);
                oneRun = renderTimed(one, clock);
            }
            else
            {
                oneRun = renderTimed(one, clock);
                fourRun = renderTimed(four, clock);
            }
            fourTimes.push_back(fourRun.cpuSeconds);
            oneTimes.push_back(oneRun.cpuSeconds);
            pairedRatios.push_back(fourRun.cpuSeconds
                                 / std::max(oneRun.cpuSeconds, 1.0e-12));
            fourHashes.push_back(fourRun.fingerprint);
            oneHashes.push_back(oneRun.fingerprint);
        }

        requireStableHash(fourHashes, scenario.name, "4x");
        requireStableHash(oneHashes, scenario.name, "1x");
        const auto fourSummary = summarize(fourTimes);
        const auto oneSummary = summarize(oneTimes);
        const auto ratioSummary = summarize(pairedRatios);

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
        printRow("4x", fourHashes.front(), fourSummary);
        printRow("1x", oneHashes.front(), oneSummary);
        std::cout << "paired_ratio " << scenario.name << " "
                  << std::fixed << std::setprecision(3)
                  << ratioSummary.median << " " << ratioSummary.minimum << " "
                  << ratioSummary.mad << "\n";
        const auto printRuns = [&](std::string_view quality,
                                   const std::vector<double>& times) {
            std::cout << "runs_cpu_ms " << scenario.name << " " << quality;
            for (const double seconds : times)
                std::cout << " " << std::fixed << std::setprecision(3)
                          << seconds * 1000.0;
            std::cout << "\n";
        };
        printRuns("4x", fourTimes);
        printRuns("1x", oneTimes);
    }
    return 0;
}

#if defined(YOUKNOW106_WORK_AUDIT)

struct CounterCase
{
    std::string_view name;
    int sampleRate;
    bool highQuality;
    int expectedFactor;
};

constexpr std::array counterCases {
    CounterCase { "48k-4x", 48000, true, 4 },
    CounterCase { "48k-1x", 48000, false, 1 },
    CounterCase { "96k-2x", 96000, true, 2 },
    CounterCase { "96k-1x", 96000, false, 1 },
    CounterCase { "192k-1x", 192000, true, 1 },
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
                                          testCase.highQuality);
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
    expectEqual(errors, testCase.name, "pulseComparatorUpdates",
                counters.pulseComparatorUpdates, voiceSlots);
    expectEqual(errors, testCase.name, "voiceAudioUpdates",
                counters.voiceAudioUpdates, voiceCards);
    expectEqual(errors, testCase.name, "dcoFrames",
                counters.dcoFrames, voiceCards);
    expectEqual(errors, testCase.name, "vcfSteps",
                counters.vcfSteps, voiceCards);
    expectEqual(errors, testCase.name, "chorusFrames",
                counters.chorusFrames, internal);
    expectEqual(errors, testCase.name, "bbdLineFrames",
                counters.bbdLineFrames, 2u * internal);
    expectEqual(errors, testCase.name, "decimatorCalls",
                counters.decimatorCalls, decimatorCalls);
    expectEqual(errors, testCase.name, "decimatorNonzeroTapVisits",
                counters.decimatorNonzeroTapVisits, 33u * decimatorCalls);
    expectEqual(errors, testCase.name, "decimatorStereoMacs",
                counters.decimatorStereoMacs, 66u * decimatorCalls);

    expectEqual(errors, testCase.name, "cutoff memo partition",
                counters.cutoffMemoHits + counters.cutoffMemoMisses,
                counters.voiceAudioUpdates);
    expectEqual(errors, testCase.name, "VCF stage evaluations",
                counters.vcfStageEvaluations, 4u * counters.vcfIterations);
    expectEqual(errors, testCase.name, "VCF bidiagonal solves",
                counters.vcfBidiagonalSolves, 2u * counters.vcfIterations);
    expectEqual(errors, testCase.name, "VCF path-average partition",
                counters.vcfShortPathAverages + counters.vcfLongPathAverages,
                counters.vcfStageEvaluations);
    expectTrue(errors, testCase.name, "vcfIterations must cover every step",
               counters.vcfIterations >= counters.vcfSteps);
    expectTrue(errors, testCase.name, "vcfIterations exceeded the 8-step cap",
               counters.vcfIterations <= 8u * counters.vcfSteps);
    expectEqual(errors, testCase.name, "vcfRecoveries",
                counters.vcfRecoveries, 0u);

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
    if (!selfTestOnly)
    {
        std::cout << "protocol block_size=" << blockSize
                  << " preroll_seconds=" << preRollSeconds
                  << " counted_blocks=" << counterBlocks
                  << " scenario=six-voice-resonant-dry"
                  << " counters=semantic hash=fnv1a64-raw-interleaved-f32\n";
    }

    for (const auto& testCase : counterCases)
    {
        const auto run = runCounterCase(testCase);
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

    if (!errors.empty())
    {
        for (const auto& error : errors)
            std::cerr << "FAIL: " << error << "\n";
        return 1;
    }
    if (selfTestOnly)
        std::cout << "oversampling work-counter algebra: PASS\n";
    return 0;
}

#endif

void printUsage(const char* executable)
{
    std::cout << "usage: " << executable
              << " [--fingerprint|--self-test|--help]\n";
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

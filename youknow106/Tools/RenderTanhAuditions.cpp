// Renders a deterministic Exact-versus-current-Fast VCF tanh audition, or an
// independent Fast-Hermite-versus-Cubic-Early audition with `--early-cubic`.
//
//   YouKnow106RenderTanhAuditions [--quality 1|2|4] [--early-cubic]
//                                [output-directory]
//
// The blind files never identify the implementation.  Open reveal/key.csv
// only after listening; the same directory then carries honest differences at
// the pair's shared listening gain, plus measurements.  Every pair starts from
// fresh engine state, receives the same score, and shares one gain chosen from
// both peaks.

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Presets.h"
#include "RealismComparisonSupport.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using youknow106::ChorusMode;
using youknow106::EngineParameters;
using youknow106::KeyMode;
using youknow106::VcfFastEarlyMode;
using youknow106::VcfTanhMode;
using youknow106::YouKnow106Engine;
using youknow106::presets::Preset;
namespace realism = youknow106::tools::realism;
using Audio = realism::StereoBuffer;
using Level = realism::Level;
using realism::decibels;

constexpr double sampleRate = realism::comparisonSampleRate;
constexpr int renderBlockSize = 256;
constexpr double listeningPeak = 0.7079457843841379; // -3 dBFS
constexpr int differenceWindowFrames = 4800;         // 100 ms

Level measureWindow(const Audio& audio, std::size_t first, std::size_t count)
{
    Level result;
    const std::size_t available = std::min(audio.left.size(), audio.right.size());
    if (first >= available)
        return result;
    const std::size_t end = first + std::min(count, available - first);
    double squareSum = 0.0;
    for (std::size_t frame = first; frame < end; ++frame)
    {
        const double left = audio.left[frame];
        const double right = audio.right[frame];
        result.peak = std::max({ result.peak, std::abs(left), std::abs(right) });
        squareSum += left * left + right * right;
    }
    const double samples = 2.0 * static_cast<double>(end - first);
    result.rms = samples > 0.0 ? std::sqrt(squareSum / samples) : 0.0;
    return result;
}

double maximumWindowDifferenceVsFullRmsDbc(const Audio& delta,
                                           double fullReferenceRms)
{
    double maximumRms = 0.0;
    for (std::size_t first = 0; first < delta.left.size();
         first += differenceWindowFrames)
        maximumRms = std::max(
            maximumRms,
            measureWindow(delta, first, differenceWindowFrames).rms);
    return decibels(maximumRms / std::max(fullReferenceRms, 1.0e-18));
}

EngineParameters parametersFor(const Preset& preset)
{
    const auto& patch = preset.patch;
    const auto& controls = preset.controls;
    EngineParameters parameters;

    parameters.lfoRate = patch.lfoRate;
    parameters.lfoDelay = patch.lfoDelay;
    parameters.dcoLfoDepth = patch.dcoLfo;
    parameters.pwmDepth = patch.pwm;
    parameters.noiseLevel = patch.noise;
    parameters.cutoff = patch.cutoff;
    parameters.resonance = patch.resonance;
    parameters.envDepth = patch.vcfEnv;
    parameters.vcfLfoDepth = patch.vcfLfo;
    parameters.keyFollow = patch.keyFollow;
    parameters.vcaLevel = patch.vcaLevel;
    parameters.attack = patch.attack;
    parameters.decay = patch.decay;
    parameters.sustain = patch.sustain;
    parameters.release = patch.release;
    parameters.subLevel = patch.sub;
    parameters.range = patch.range;
    parameters.sawEnabled = patch.saw;
    parameters.pulseEnabled = patch.pulse;
    parameters.pwmSource = patch.pwmSource;
    parameters.vcaMode = patch.vcaMode;
    parameters.envPolarity = patch.envPolarity;
    parameters.highPass = patch.highPass;
    parameters.chorus = patch.chorus;

    parameters.volume = controls.volume;
    parameters.benderDcoDepth = controls.benderDco;
    parameters.benderVcfDepth = controls.benderVcf;
    parameters.benderLfoDepth = controls.benderLfo;
    parameters.portamento = controls.portamento;
    parameters.keyMode = controls.keyMode;
    parameters.keyTranspose = controls.transpose;
    parameters.masterTuneCents = controls.masterTune;
    parameters.velocityDepth = controls.velocity;
    parameters.calibration = controls.calibration;
    parameters.chorusNoise = controls.chorusNoise;
    parameters.polyphony = controls.polyphony;
    return parameters;
}

class Take
{
public:
    Take(EngineParameters parameters, int requestedQuality)
        : engine_(std::make_unique<YouKnow106Engine>())
    {
        engine_->prepare(sampleRate, renderBlockSize, requestedQuality);
        appliedQuality_ = engine_->getOversamplingFactor();
        latencySamples_ = engine_->getProcessingLatencySamples();
        engine_->setParameters(parameters);
    }

    void on(int note) { engine_->noteOn(note, 1.0f); }
    void off(int note) { engine_->noteOff(note); }

    void hit(int note, double holdSeconds, double gapSeconds)
    {
        on(note);
        rest(holdSeconds);
        off(note);
        rest(gapSeconds);
    }

    void chord(std::initializer_list<int> notes, double holdSeconds,
               double gapSeconds)
    {
        for (const int note : notes)
            on(note);
        rest(holdSeconds);
        for (const int note : notes)
            off(note);
        rest(gapSeconds);
    }

    void rest(double seconds)
    {
        auto remaining = static_cast<std::int64_t>(
            std::llround(seconds * sampleRate));
        std::array<float, renderBlockSize> left {};
        std::array<float, renderBlockSize> right {};
        while (remaining > 0)
        {
            const int count = static_cast<int>(std::min<std::int64_t>(
                renderBlockSize, remaining));
            engine_->process(left.data(), right.data(), count);
            audio_.left.insert(audio_.left.end(), left.begin(), left.begin() + count);
            audio_.right.insert(audio_.right.end(), right.begin(), right.begin() + count);
            remaining -= count;
        }
    }

    [[nodiscard]] int appliedQuality() const noexcept { return appliedQuality_; }
    [[nodiscard]] int latencySamples() const noexcept { return latencySamples_; }
    Audio takeAudio() { return std::move(audio_); }

private:
    std::unique_ptr<YouKnow106Engine> engine_;
    Audio audio_;
    int appliedQuality_ {};
    int latencySamples_ {};
};

void playA82(Take& take)
{
    // A82 has no oscillator source.  One physical card is deliberately reused
    // so its slowly established limit cycle becomes a playable melody.
    take.rest(0.30);
    take.hit(36, 2.60, 0.05);
    for (const int note : { 43, 48, 52, 55, 60, 55, 48, 43 })
        take.hit(note, 0.55, 0.05);
    take.hit(36, 2.00, 1.80);
}

void playB87(Take& take)
{
    take.rest(0.10);
    constexpr std::array<std::pair<int, double>, 12> notes {{
        { 36, 0.08 }, { 36, 0.16 }, { 43, 0.08 }, { 39, 0.16 },
        { 36, 0.08 }, { 46, 0.16 }, { 43, 0.08 }, { 39, 0.24 },
        { 36, 0.08 }, { 48, 0.08 }, { 46, 0.08 }, { 43, 0.24 },
    }};
    for (int repeat = 0; repeat < 2; ++repeat)
        for (const auto& [note, gap] : notes)
            take.hit(note, 0.16, gap);
    take.hit(36, 0.75, 0.90);
}

void playB44(Take& take)
{
    take.rest(0.10);
    constexpr std::array<int, 12> notes {
        40, 52, 47, 52, 43, 55, 47, 52, 40, 52, 47, 43
    };
    for (int repeat = 0; repeat < 2; ++repeat)
        for (const int note : notes)
            take.hit(note, 0.22, 0.08);
    take.hit(40, 1.60, 1.00);
}

void playB15(Take& take)
{
    take.rest(0.10);
    for (int repeat = 0; repeat < 2; ++repeat)
        for (const int note : { 72, 76, 79, 84, 79, 76, 74, 81 })
            take.hit(note, 0.13, 0.09);
    take.chord({ 72, 76, 79, 84 }, 0.45, 0.90);
}

void playB42(Take& take)
{
    take.rest(0.10);
    take.chord({ 60, 67, 72 }, 0.35, 0.12);
    take.chord({ 64, 71, 76 }, 0.35, 0.12);
    take.chord({ 67, 74, 79 }, 0.35, 0.12);
    for (const int note : { 72, 79, 84, 88, 84, 79, 76, 83 })
        take.hit(note, 0.16, 0.08);
    take.chord({ 72, 79, 84 }, 0.80, 1.00);
}

void playB88(Take& take)
{
    take.rest(0.10);
    take.chord({ 36, 43, 48, 52, 55 }, 2.00, 0.40);
    take.chord({ 41, 48, 53, 57, 60 }, 2.00, 0.40);
    take.chord({ 38, 45, 50, 53, 57, 62 }, 2.40, 1.20);
}

struct Fixture
{
    const char* slug;
    const char* presetNumber;
    const char* displayName;
    void (*play)(Take&);
    bool forceOneVoice;
};

constexpr std::array<Fixture, 6> fixtures {{
    { "01-a82-derived-self-oscillation", "A82", "A82-derived self oscillation",
      playA82, true },
    { "02-b87-froggy", "B87", "B87 Froggy",
      playB87, false },
    { "03-b44-contact-wah", "B44", "B44 Contact Wah",
      playB44, false },
    { "04-b15-harpsichord-1", "B15", "B15 Harpsichord 1",
      playB15, false },
    { "05-b42-harpsichord-2", "B42", "B42 Harpsichord 2",
      playB42, false },
    { "06-b88-owgan", "B88", "B88 Owgan",
      playB88, false },
}};

bool exactIsA(std::size_t fixture, int quality) noexcept
{
    // Three distinct, non-complementary balanced mappings prevent one
    // quality's complete A/B assignment from standing in for another. They
    // are deterministic counterbalancing, not statistically independent draws.
    constexpr std::array<unsigned, 3> masks { 0b000111u, 0b001011u, 0b010101u };
    const std::size_t qualityIndex = quality == 1 ? 0u : (quality == 2 ? 1u : 2u);
    return (masks[qualityIndex] & (1u << fixture)) != 0u;
}

bool hermiteIsA(std::size_t fixture, int quality) noexcept
{
    // Independent balanced assignments for the Fast-Hermite/Cubic-Early
    // audition. Revealing either comparison must not reveal the other.
    constexpr std::array<unsigned, 3> masks { 0b011100u, 0b100110u, 0b110001u };
    const std::size_t qualityIndex = quality == 1 ? 0u : (quality == 2 ? 1u : 2u);
    return (masks[qualityIndex] & (1u << fixture)) != 0u;
}

struct Rendered
{
    Audio audio;
    int appliedQuality {};
    int latencySamples {};
};

Rendered render(const Fixture& fixture, const Preset& preset,
                VcfTanhMode mode, int quality,
                VcfFastEarlyMode fastEarlyMode = VcfFastEarlyMode::Hermite,
                bool maximumCharacter = false)
{
    auto parameters = parametersFor(preset);
    // The comparison isolates the VCF.  None of the selected factory tones
    // uses its main noise source, but make that and both chorus masking paths
    // explicit so later factory-bank edits cannot invalidate the audition.
    parameters.noiseLevel = 0.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.vcfTanhMode = mode;
    parameters.vcfFastEarlyMode = fastEarlyMode;
    if (maximumCharacter)
        parameters.calibration = EngineParameters::calibrationCeiling;
    if (fixture.forceOneVoice)
    {
        parameters.polyphony = 1;
        parameters.keyMode = KeyMode::Poly1;
    }

    Take take(parameters, quality);
    fixture.play(take);
    Rendered result;
    result.appliedQuality = take.appliedQuality();
    result.latencySamples = take.latencySamples();
    result.audio = take.takeAudio();
    return result;
}

struct Options
{
    std::filesystem::path outputDirectory;
    int quality { 4 };
    bool earlyCubicComparison {};
};

bool parseQuality(std::string_view text, int& quality)
{
    int parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc {} || result.ptr != text.data() + text.size()
        || (parsed != 1 && parsed != 2 && parsed != 4))
        return false;
    quality = parsed;
    return true;
}

bool parseOptions(int argc, char** argv, Options& options)
{
    bool outputSeen = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h")
        {
            std::printf("usage: %s [--quality 1|2|4] [--early-cubic]"
                        " [output-directory]\n", argv[0]);
            return false;
        }
        if (argument == "--quality")
        {
            if (++index >= argc || !parseQuality(argv[index], options.quality))
            {
                std::fprintf(stderr, "--quality requires 1, 2, or 4\n");
                return false;
            }
            continue;
        }
        if (argument == "--early-cubic")
        {
            options.earlyCubicComparison = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-')
        {
            std::fprintf(stderr, "unknown option: %s\n", argv[index]);
            return false;
        }
        if (outputSeen)
        {
            std::fprintf(stderr, "only one output directory may be supplied\n");
            return false;
        }
        options.outputDirectory = argv[index];
        outputSeen = true;
    }
    if (!outputSeen)
        options.outputDirectory = options.earlyCubicComparison
            ? "tanh-auditions-cubic-early-"
                + std::to_string(options.quality) + "x"
            : "tanh-auditions-zoned-reciprocal-"
                + std::to_string(options.quality) + "x";
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options))
        return argc == 2
                && (std::string_view(argv[1]) == "--help"
                    || std::string_view(argv[1]) == "-h") ? 0 : 1;

    std::error_code error;
    const bool outputExists = std::filesystem::exists(
        options.outputDirectory, error);
    if (error)
    {
        std::fprintf(stderr, "could not inspect %s: %s\n",
                     options.outputDirectory.string().c_str(),
                     error.message().c_str());
        return 1;
    }
    if (outputExists)
    {
        const bool emptyDirectory = std::filesystem::is_directory(
                                        options.outputDirectory, error)
                                 && std::filesystem::is_empty(
                                        options.outputDirectory, error);
        if (error)
        {
            std::fprintf(stderr, "could not inspect %s: %s\n",
                         options.outputDirectory.string().c_str(),
                         error.message().c_str());
            return 1;
        }
        if (!emptyDirectory)
        {
            std::fprintf(stderr,
                         "output root must be absent or an empty directory: %s\n",
                         options.outputDirectory.string().c_str());
            return 1;
        }
    }

    const auto blindDirectory = options.outputDirectory / "blind";
    const auto revealDirectory = options.outputDirectory / "reveal";
    std::filesystem::create_directories(blindDirectory, error);
    if (error)
    {
        std::fprintf(stderr, "could not create %s: %s\n",
                     blindDirectory.string().c_str(), error.message().c_str());
        return 1;
    }
    std::filesystem::create_directories(revealDirectory, error);
    if (error)
    {
        std::fprintf(stderr, "could not create %s: %s\n",
                     revealDirectory.string().c_str(), error.message().c_str());
        return 1;
    }

    std::ostringstream key;
    key << "fixture,A,B\n";
    std::ostringstream metrics;
    metrics << "fixture,preset,requested_quality,applied_quality,frames,"
               "latency_samples,shared_gain_db,";
    if (options.earlyCubicComparison)
        metrics << "hermite_early_peak_dbfs,cubic_early_peak_dbfs,"
                   "hermite_early_rms_dbfs,cubic_early_rms_dbfs,";
    else
        metrics << "exact_peak_dbfs,zoned_reciprocal_peak_dbfs,"
                   "exact_rms_dbfs,zoned_reciprocal_rms_dbfs,";
    metrics << "difference_peak_dbfs,difference_rms_dbfs,"
               "difference_peak_dbc,difference_rms_dbc,"
            << (options.earlyCubicComparison
                    ? "max_100ms_difference_vs_full_hermite_rms_dbc\n"
                    : "max_100ms_difference_vs_full_exact_rms_dbc\n")
            << std::fixed << std::setprecision(9);

    std::printf("Rendering blinded VCF %s auditions at 48 kHz / %dx%s\n",
                options.earlyCubicComparison
                    ? "Fast-Hermite/Cubic-Early" : "Exact/Fast tanh",
                options.quality,
                options.earlyCubicComparison ? ", Character 2" : "");
    for (std::size_t fixtureIndex = 0; fixtureIndex < fixtures.size();
        ++fixtureIndex)
    {
        const auto& fixture = fixtures[fixtureIndex];
        const bool referenceFirst = options.earlyCubicComparison
            ? hermiteIsA(fixtureIndex, options.quality)
            : exactIsA(fixtureIndex, options.quality);
        const Preset* preset = youknow106::presets::findByNumber(fixture.presetNumber);
        if (preset == nullptr)
        {
            std::fprintf(stderr, "factory preset %s was not found\n",
                         fixture.presetNumber);
            return 1;
        }

        auto reference = options.earlyCubicComparison
            ? render(fixture, *preset, VcfTanhMode::ZonedHermite,
                     options.quality, VcfFastEarlyMode::Hermite, true)
            : render(fixture, *preset, VcfTanhMode::Exact, options.quality);
        auto candidate = options.earlyCubicComparison
            ? render(fixture, *preset, VcfTanhMode::ZonedHermite,
                     options.quality, VcfFastEarlyMode::Cubic, true)
            : render(fixture, *preset, VcfTanhMode::ZonedHermite,
                     options.quality);
        std::string referenceError;
        std::string candidateError;
        if (!realism::validate(reference.audio, referenceError)
            || !realism::validate(candidate.audio, candidateError)
            || reference.audio.left.size() != candidate.audio.left.size()
            || reference.appliedQuality != candidate.appliedQuality
            || reference.latencySamples != candidate.latencySamples)
        {
            std::fprintf(stderr,
                         "%s failed finite/alignment/latency checks: %s%s%s\n",
                         fixture.slug, referenceError.c_str(),
                         !referenceError.empty() && !candidateError.empty()
                             ? "; " : "",
                         candidateError.c_str());
            return 1;
        }

        const Level rawReference = realism::measure(reference.audio);
        const Level rawCandidate = realism::measure(candidate.audio);
        const double maximumPeak = std::max(
            rawReference.peak, rawCandidate.peak);
        if (!(maximumPeak > 1.0e-9))
        {
            std::fprintf(stderr, "%s rendered silence\n", fixture.slug);
            return 1;
        }
        const double sharedGain = listeningPeak / maximumPeak;
        reference.audio = realism::applyGain(reference.audio, sharedGain);
        candidate.audio = realism::applyGain(candidate.audio, sharedGain);
        Audio delta;
        std::string differenceError;
        if (!realism::difference(reference.audio, candidate.audio,
                                 delta, differenceError))
        {
            std::fprintf(stderr, "%s difference failed: %s\n",
                         fixture.slug, differenceError.c_str());
            return 1;
        }

        const auto fixtureBlind = blindDirectory / fixture.slug;
        const auto fixtureReveal = revealDirectory / fixture.slug;
        const Audio& a = referenceFirst ? reference.audio : candidate.audio;
        const Audio& b = referenceFirst ? candidate.audio : reference.audio;
        std::string writeError;
        if (!realism::writeFloatWav(fixtureBlind / "A.wav", a, writeError)
            || !realism::writeFloatWav(fixtureBlind / "B.wav", b, writeError)
            || !realism::writeFloatWav(fixtureReveal / "difference.wav",
                                       delta, writeError))
        {
            std::fprintf(stderr, "could not write audition files for %s: %s\n",
                         fixture.slug, writeError.c_str());
            return 1;
        }

        const Level referenceLevel = realism::measure(reference.audio);
        const Level candidateLevel = realism::measure(candidate.audio);
        const Level differenceLevel = realism::measure(delta);
        const char* referenceName = options.earlyCubicComparison
            ? "ZonedHermiteReciprocal" : "Exact";
        const char* candidateName = options.earlyCubicComparison
            ? "ZonedHermiteReciprocalCubicEarly"
            : "ZonedHermiteReciprocal";
        key << fixture.slug << ','
            << (referenceFirst ? referenceName : candidateName) << ','
            << (referenceFirst ? candidateName : referenceName) << '\n';
        metrics << fixture.slug << ',' << fixture.presetNumber << ','
                << options.quality << ',' << reference.appliedQuality << ','
                << reference.audio.left.size() << ','
                << reference.latencySamples << ','
                << decibels(sharedGain) << ','
                << decibels(referenceLevel.peak) << ','
                << decibels(candidateLevel.peak) << ','
                << decibels(referenceLevel.rms) << ','
                << decibels(candidateLevel.rms) << ','
                << decibels(differenceLevel.peak) << ','
                << decibels(differenceLevel.rms) << ','
                << decibels(differenceLevel.peak
                            / std::max(referenceLevel.peak, 1.0e-18)) << ','
                << decibels(differenceLevel.rms
                            / std::max(referenceLevel.rms, 1.0e-18)) << ','
                << maximumWindowDifferenceVsFullRmsDbc(
                       delta, referenceLevel.rms) << '\n';

        std::printf("  %-38s %6.2f s  diff RMS %+7.1f dBc\n",
                    fixture.displayName,
                    static_cast<double>(reference.audio.left.size()) / sampleRate,
                    decibels(differenceLevel.rms
                             / std::max(referenceLevel.rms, 1.0e-18)));
    }

    std::ofstream keyFile(revealDirectory / "key.csv", std::ios::binary);
    keyFile << key.str();
    std::ofstream metricsFile(revealDirectory / "metrics.csv", std::ios::binary);
    metricsFile << metrics.str();
    keyFile.close();
    metricsFile.close();
    if (!keyFile || !metricsFile)
    {
        std::fprintf(stderr, "could not write reveal CSV files\n");
        return 1;
    }

    std::printf("Blind files: %s\n", blindDirectory.string().c_str());
    std::printf("After judging, reveal with: %s\n",
                (revealDirectory / "key.csv").string().c_str());
    return 0;
}

// A strict, build-to-build realism comparison renderer.
//
// Usage:
//   YouKnow106RenderRealismComparison retrigger-release-tail before <output-dir>
//   ... make and rebuild one DSP change ...
//   YouKnow106RenderRealismComparison retrigger-release-tail after <output-dir>
//
// The `before` raw float32 file is an immutable baseline: rerunning `before`
// succeeds only if the new render is bit-identical. The `after` pass reads that
// archive, writes an unscaled signed difference, and applies one common
// listening gain to both sides and their difference.

#include "RealismComparisonSupport.h"

#include "DSP/YouKnow106Engine.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef YOUKNOW106_DSP_SOURCE_SHA256
#define YOUKNOW106_DSP_SOURCE_SHA256 "unavailable"
#endif

#ifndef YOUKNOW106_COMPARISON_COMPILER_ID
#define YOUKNOW106_COMPARISON_COMPILER_ID "unknown"
#endif

#ifndef YOUKNOW106_COMPARISON_COMPILER_VERSION
#define YOUKNOW106_COMPARISON_COMPILER_VERSION "unknown"
#endif

namespace
{
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::HighPassMode;
using youknow106::KeyMode;
using youknow106::YouKnow106Engine;
using youknow106::tools::realism::Level;
using youknow106::tools::realism::StereoBuffer;
using youknow106::tools::realism::applyGain;
using youknow106::tools::realism::comparisonBlockSize;
using youknow106::tools::realism::comparisonSampleRate;
using youknow106::tools::realism::decibels;
using youknow106::tools::realism::difference;
using youknow106::tools::realism::jsonNumber;
using youknow106::tools::realism::listeningTargetPeak;
using youknow106::tools::realism::measure;
using youknow106::tools::realism::readFloatWav;
using youknow106::tools::realism::updateGeneratedSection;
using youknow106::tools::realism::validate;
using youknow106::tools::realism::writeFloatWav;
using youknow106::tools::realism::writeText;

constexpr std::string_view scenarioSlug = "retrigger-release-tail";
constexpr std::uint32_t eventScheduleSeed = 0x1061065du;
constexpr int scenarioProtocolVersion = 1;
constexpr std::string_view generatedBegin =
    "<!-- BEGIN GENERATED REALISM COMPARISON -->";
constexpr std::string_view generatedEnd =
    "<!-- END GENERATED REALISM COMPARISON -->";

struct Event
{
    std::uint64_t sample { 0 };
    const char* action { "" };
    int note { 0 };
};

struct RenderedScenario
{
    StereoBuffer audio;
    std::vector<Event> events;
    std::vector<int> releaseGapSamples;
};

class FixedRng
{
public:
    explicit FixedRng(std::uint32_t seed) : state_(seed) {}

    std::uint32_t next() noexcept
    {
        // xorshift32: this exact state transition is part of protocol v1.
        state_ ^= state_ << 13u;
        state_ ^= state_ >> 17u;
        state_ ^= state_ << 5u;
        return state_;
    }

private:
    std::uint32_t state_;
};

class Performance
{
public:
    explicit Performance(const EngineParameters& parameters)
    {
        engine_.prepare(static_cast<double>(comparisonSampleRate),
                        comparisonBlockSize, true);
        engine_.setParameters(parameters);
    }

    void noteOn(int note)
    {
        events_.push_back({ cursor_, "note_on", note });
        engine_.noteOn(note, 1.0f);
    }

    void noteOff(int note)
    {
        events_.push_back({ cursor_, "note_off", note });
        engine_.noteOff(note);
    }

    void render(int samples)
    {
        std::array<float, comparisonBlockSize> left {};
        std::array<float, comparisonBlockSize> right {};
        int remaining = samples;
        while (remaining > 0)
        {
            const int count = std::min(comparisonBlockSize, remaining);
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            engine_.process(left.data(), right.data(), count);
            audio_.left.insert(audio_.left.end(), left.begin(), left.begin() + count);
            audio_.right.insert(audio_.right.end(), right.begin(), right.begin() + count);
            cursor_ += static_cast<std::uint64_t>(count);
            remaining -= count;
        }
    }

    StereoBuffer takeAudio() { return std::move(audio_); }
    std::vector<Event> takeEvents() { return std::move(events_); }

private:
    YouKnow106Engine engine_;
    StereoBuffer audio_;
    std::vector<Event> events_;
    std::uint64_t cursor_ { 0 };
};

EngineParameters retriggerPatch()
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Poly1;
    parameters.polyphony = 1;
    parameters.range = DcoRange::Eight;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.72f;
    parameters.resonance = 0.18f;
    parameters.envDepth = 0.18f;
    parameters.keyFollow = 0.5f;
    parameters.attack = 0.0f;
    parameters.decay = 0.30f;
    parameters.sustain = 0.78f;
    parameters.release = 0.92f;
    parameters.vcaLevel = 1.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 0.50f;
    parameters.calibration = 1.0f;
    return parameters;
}

RenderedScenario renderRetriggerReleaseTail()
{
    Performance performance(retriggerPatch());
    FixedRng rng(eventScheduleSeed);
    std::vector<int> gaps;

    constexpr int note = 60;
    constexpr int scanSamples = 202; // nearest host sample to the 4.2 ms scan
    performance.render(4800);         // 100 ms deterministic warm-up

    // A same-pitch release-tail reassignment avoids a DCO restart or pitch
    // discontinuity. The varying 2 ms + scan-phase gaps expose whether a note
    // assignment itself creates a VCA-control pulse before a converter write.
    for (int repetition = 0; repetition < 8; ++repetition)
    {
        performance.noteOn(note);
        performance.render(6720); // 140 ms: high, repeatable envelope level
        performance.noteOff(note);

        const int releaseGap = 96 + static_cast<int>(rng.next() % scanSamples);
        gaps.push_back(releaseGap);
        performance.render(releaseGap);

        performance.noteOn(note);
        performance.render(3840); // 80 ms observation window after retrigger
        performance.noteOff(note);
        performance.render(4320); // 90 ms release, still sounding next cycle
    }
    performance.render(24000); // retain the audible final release tail

    return { performance.takeAudio(), performance.takeEvents(), std::move(gaps) };
}

std::string jsonEscape(std::string_view input)
{
    std::string output;
    for (const char value : input)
    {
        switch (value)
        {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += value; break;
        }
    }
    return output;
}

std::string levelJson(const Level& level, int indent)
{
    const std::string spaces(static_cast<std::size_t>(indent), ' ');
    std::ostringstream output;
    output << "{\n"
           << spaces << "  \"peak_linear\": " << jsonNumber(level.peak) << ",\n"
           << spaces << "  \"peak_dbfs\": " << jsonNumber(decibels(level.peak)) << ",\n"
           << spaces << "  \"rms_linear\": " << jsonNumber(level.rms) << ",\n"
           << spaces << "  \"rms_dbfs\": " << jsonNumber(decibels(level.rms)) << "\n"
           << spaces << "}";
    return output.str();
}

std::string eventJson(const std::vector<Event>& events)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        const auto& event = events[index];
        output << "    { \"sample\": " << event.sample
               << ", \"action\": \"" << event.action
               << "\", \"note\": " << event.note << " }";
        if (index + 1 < events.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

std::string gapsJson(const std::vector<int>& gaps)
{
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < gaps.size(); ++index)
    {
        if (index > 0)
            output << ", ";
        output << gaps[index];
    }
    output << ']';
    return output.str();
}

std::string commonManifestPrefix(std::string_view stage,
                                 const RenderedScenario& rendered)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << scenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": " << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \"" << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"high_quality\": true,\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"event_schedule_seed_hex\": \"0x1061065d\",\n"
           << "  \"engine_seed_policy\": "
              "\"fixed per-card hash seeds rooted at 17; chorus noise disabled\",\n"
           << "  \"patch\": {\n"
           << "    \"key_mode\": \"Poly 1\",\n"
           << "    \"polyphony\": 1,\n"
           << "    \"range\": \"8 foot\",\n"
           << "    \"saw_enabled\": true,\n"
           << "    \"pulse_enabled\": false,\n"
           << "    \"sub_level\": 0.0,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"high_pass\": \"1\",\n"
           << "    \"cutoff\": 0.72,\n"
           << "    \"resonance\": 0.18,\n"
           << "    \"envelope_depth\": 0.18,\n"
           << "    \"key_follow\": 0.5,\n"
           << "    \"attack\": 0.0,\n"
           << "    \"decay\": 0.30,\n"
           << "    \"sustain\": 0.78,\n"
           << "    \"release\": 0.92,\n"
           << "    \"vca_level\": 1.0,\n"
           << "    \"chorus\": \"off\",\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"velocity_depth\": 0.0,\n"
           << "    \"volume\": 0.50,\n"
           << "    \"unit_character\": 1.0\n"
           << "  },\n"
           << "  \"release_gap_samples\": " << gapsJson(rendered.releaseGapSamples)
           << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n";
    return output.str();
}

std::string readmeProse()
{
    return
        "# Retrigger/release-tail realism comparison\n\n"
        "This focused fixture reassigns one still-releasing voice to the same note at\n"
        "several deterministic control-scan phases. Keeping the pitch unchanged isolates\n"
        "assignment/VCA behaviour from oscillator restart and portamento effects.\n\n"
        "The raw float32 baseline is archival evidence and must be rendered before the DSP\n"
        "change. Listening files use one shared gain; no side is independently normalized.\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "From the project directory, render the two stages around exactly one rebuilt DSP\n"
        "change:\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail before \\\n"
        "  Docs/audio/realism-comparisons/retrigger-release-tail\n"
        "# Apply and rebuild the DSP change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail after \\\n"
        "  Docs/audio/realism-comparisons/retrigger-release-tail\n"
        "```";
}

std::string beforeGenerated(const Level& level, double listeningGain)
{
    std::ostringstream output;
    output << "Generated baseline for scenario protocol " << scenarioProtocolVersion
           << ". The `after` pass has not supplied a comparison yet.\n\n"
           << "| Metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Raw peak | " << std::fixed << std::setprecision(6) << level.peak
           << " (" << std::setprecision(2) << decibels(level.peak) << " dBFS) |\n"
           << "| Raw RMS | " << std::setprecision(6) << level.rms
           << " (" << std::setprecision(2) << decibels(level.rms) << " dBFS) |\n"
           << "| Baseline listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `retrigger-release-tail-before-manifest.json` for the exact event\n"
           << "schedule, build fingerprint, render settings, and raw values.";
    return output.str();
}

std::string afterGenerated(const Level& before, const Level& after,
                           const Level& diff, double listeningGain,
                           double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol " << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::fixed << std::setprecision(6)
           << "| Before | " << before.peak << " (" << std::setprecision(2)
           << decibels(before.peak) << " dBFS) | " << std::setprecision(6)
           << before.rms << " (" << std::setprecision(2)
           << decibels(before.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| After | " << after.peak << " ("
           << std::setprecision(2) << decibels(after.peak) << " dBFS) | "
           << std::setprecision(6) << after.rms << " (" << std::setprecision(2)
           << decibels(after.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| Signed difference | " << diff.peak << " ("
           << std::setprecision(2) << decibels(diff.peak) << " dBFS) | "
           << std::setprecision(6) << diff.rms << " (" << std::setprecision(2)
           << decibels(diff.rms) << " dBFS) |\n\n"
           << "| Correct relative metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Difference peak / before peak | " << std::setprecision(2)
           << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Shared listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `retrigger-release-tail-comparison-manifest.json` for machine-readable\n"
           << "metadata and exact values.";
    return output.str();
}

bool updateReadme(const std::filesystem::path& outputDirectory,
                  const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md", readmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool writeBefore(const std::filesystem::path& outputDirectory,
                 const RenderedScenario& rendered, std::string& error)
{
    const auto rawPath = outputDirectory
                       / "retrigger-release-tail-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 1.0e-12 || rawLevel.rms <= 1.0e-12)
    {
        error = "scenario rendered silence";
        return false;
    }
    const double listeningGain = listeningTargetPeak / rawLevel.peak;
    const StereoBuffer listening = applyGain(rendered.audio, listeningGain);
    if (!writeFloatWav(outputDirectory
                       / "retrigger-release-tail-before-listen-f32.wav",
                       listening, error))
        return false;

    std::ostringstream manifest;
    manifest << commonManifestPrefix("before", rendered)
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"baseline-derived provisional gain; after pass applies one shared pair gain\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "retrigger-release-tail-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateReadme(outputDirectory,
                        beforeGenerated(rawLevel, listeningGain), error);
}

bool writeAfter(const std::filesystem::path& outputDirectory,
                const RenderedScenario& rendered, std::string& error)
{
    const auto beforeRawPath = outputDirectory
                             / "retrigger-release-tail-before-raw-f32.wav";
    StereoBuffer before;
    if (!readFloatWav(beforeRawPath, before, error))
        return false;
    if (before.left.size() != rendered.audio.left.size())
    {
        error = "baseline frame count does not match scenario protocol v1";
        return false;
    }

    StereoBuffer diff;
    if (!difference(before, rendered.audio, diff, error))
        return false;

    const Level beforeLevel = measure(before);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 1.0e-12 || beforeLevel.rms <= 1.0e-12)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }

    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const double pairPeak = std::max(beforeLevel.peak, afterLevel.peak);
    const double listeningGain = listeningTargetPeak / pairPeak;

    if (!writeFloatWav(outputDirectory
                       / "retrigger-release-tail-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-before-listen-f32.wav",
                          applyGain(before, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-after-listen-f32.wav",
                          applyGain(rendered.audio, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-difference-listen-f32.wav",
                          applyGain(diff, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << commonManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"retrigger-release-tail-before-manifest.json\",\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"shared_gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"shared_gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"one gain derived from max(before peak, after peak), identically applied to before, after, and signed difference\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "retrigger-release-tail-comparison-manifest.json",
                   manifest.str(), error))
        return false;

    if (!updateReadme(outputDirectory,
                      afterGenerated(beforeLevel, afterLevel, diffLevel,
                                     listeningGain, peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

void printUsage(const char* executable)
{
    std::fprintf(stderr,
                 "usage: %s retrigger-release-tail <before|after> <output-dir>\n",
                 executable);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        printUsage(argv[0]);
        return 0;
    }
    if (argc != 4)
    {
        printUsage(argv[0]);
        return 2;
    }

    const std::string_view scenario = argv[1];
    const std::string_view stage = argv[2];
    const std::filesystem::path outputDirectory = argv[3];
    if (scenario != scenarioSlug || (stage != "before" && stage != "after")
        || outputDirectory.empty())
    {
        printUsage(argv[0]);
        return 2;
    }

    RenderedScenario rendered = renderRetriggerReleaseTail();
    std::string error;
    if (!validate(rendered.audio, error))
    {
        std::fprintf(stderr, "render failed: %s\n", error.c_str());
        return 1;
    }

    const bool written = stage == "before"
        ? writeBefore(outputDirectory, rendered, error)
        : writeAfter(outputDirectory, rendered, error);
    if (!written)
    {
        std::fprintf(stderr, "comparison failed: %s\n", error.c_str());
        return 1;
    }

    const auto level = measure(rendered.audio);
    std::printf("Rendered %s %s: %zu frames, peak %.6f, RMS %.6f\n",
                std::string(scenario).c_str(), std::string(stage).c_str(),
                rendered.audio.left.size(), level.peak, level.rms);
    std::printf("Output: %s\n", outputDirectory.string().c_str());
    return 0;
}

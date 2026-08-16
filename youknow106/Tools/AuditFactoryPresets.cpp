// Audits the byte-exact factory bank through the same JUCE-free engine used by
// the plug-in, and renders a small fixed preview set without per-file loudness
// normalisation. This is deliberately a reporting tool: overload and level
// differences are evidence, not reasons to rewrite the historical tone bytes.

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Presets.h"
#include "DSP/YouKnow106SysEx.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef YOUKNOW106_FACTORY_AUDIT_DEFAULT_OUTPUT
#define YOUKNOW106_FACTORY_AUDIT_DEFAULT_OUTPUT "Docs/audio/factory-presets"
#endif

namespace
{
using youknow106::EngineParameters;
using youknow106::KeyMode;
using youknow106::YouKnow106Engine;
using youknow106::presets::Preset;

constexpr double sampleRate = 48000.0;
constexpr int renderBlockSize = 256;
// The deepest rung of the quality ladder, which at 48 kHz is a 192 kHz
// internal grid.
constexpr int auditOversampleFactor = 4;

// The shipping bank's two level contracts, as absolute figures rather than as
// offsets from a median the audit itself computes -- a self-referential gate
// moves every time a preset is retrimmed and can never be satisfied. Both are
// enforced below; Source/DSP/YouKnow106Presets.cpp carries the per-preset VR1
// positions that hold the bank inside them.
constexpr double factoryPeakCeilingDbfs = -1.0;
constexpr double factoryGatedCeilingDbfs = -18.0;
constexpr int rmsWindowFrames = 19200; // 400 ms
constexpr int rmsHopFrames = 4800;     // 100 ms
constexpr double meterFloorDb = -140.0;
constexpr double meterFloorLinear = 1.0e-7;
constexpr double previewCeiling = 0.8912509381337456; // -1 dBFS
constexpr int previewFadeFrames = 2400;               // 50 ms at 48 kHz
constexpr std::string_view ownershipMarker =
    "<!-- youknow106-factory-preset-audit-owned-v1 -->";

struct PreviewSpec
{
    int presetIndex;
    const char* expectedNumber;
    const char* fileName;
};

// These cover both banks, sustained and percussive programs, stored chorus,
// octave directions and Solo Unison. Their indices are hardware-memory order.
constexpr std::array<PreviewSpec, 10> previewSpecs {{
    { 0, "A11", "preview-A11-brass-set-1.wav" },
    { 6, "A17", "preview-A17-choir.wav" },
    { 17, "A32", "preview-A32-steel-drums.wav" },
    { 31, "A48", "preview-A48-synth-bass-unison.wav" },
    { 61, "A86", "preview-A86-hand-claps.wav" },
    { 64, "B11", "preview-B11-strings.wav" },
    { 80, "B31", "preview-B31-brass.wav" },
    { 82, "B33", "preview-B33-lute.wav" },
    { 91, "B44", "preview-B44-contact-wah.wav" },
    { 124, "B85", "preview-B85-rocket-men.wav" },
}};

std::int64_t framesFor (double seconds)
{
    return static_cast<std::int64_t> (std::llround (seconds * sampleRate));
}

double toDb (double linear)
{
    if (! std::isfinite (linear) || linear <= meterFloorLinear)
        return meterFloorDb;
    return 20.0 * std::log10 (linear);
}

const char* keyModeName (KeyMode mode)
{
    switch (mode)
    {
        case KeyMode::Poly1: return "Poly 1";
        case KeyMode::Poly2: return "Poly 2";
        case KeyMode::Unison: return "Solo Unison";
    }
    return "Unknown";
}

EngineParameters parametersFor (const Preset& preset, bool muteChorusNoise)
{
    const auto& patch = preset.patch;
    const auto& controls = preset.controls;
    EngineParameters parameters;

    // Every stored tone field, without gain correction or rebalancing.
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

    // Every product/performance control stored beside the hardware tone.
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
    parameters.chorusNoise = muteChorusNoise ? 0.0f : controls.chorusNoise;
    parameters.polyphony = controls.polyphony;
    return parameters;
}

struct Audio
{
    std::vector<float> left;
    std::vector<float> right;
};

class ScoreRenderer
{
public:
    ScoreRenderer (const Preset& preset, bool muteChorusNoise)
    {
        // Quality is a machine setting rather than preset data, so the audit
        // fixes it at the deepest rung: the levels it publishes then describe
        // the tone, not whatever the auditing machine could afford.
        engine.prepare (sampleRate, renderBlockSize, auditOversampleFactor);
        engine.setParameters (parametersFor (preset, muteChorusNoise));
    }

    bool isHq() const noexcept
    {
        return engine.getOversamplingFactor() > 1;
    }

    void noteOn (int note) { engine.noteOn (note, 1.0f); }
    void noteOff (int note) { engine.noteOff (note); }

    void renderSeconds (double seconds)
    {
        renderFrames (framesFor (seconds));
    }

    Audio takeAudio() { return std::move (audio); }

private:
    void renderFrames (std::int64_t remaining)
    {
        std::array<float, renderBlockSize> left {};
        std::array<float, renderBlockSize> right {};
        while (remaining > 0)
        {
            const int count = static_cast<int> (
                std::min<std::int64_t> (renderBlockSize, remaining));
            engine.process (left.data(), right.data(), count);
            audio.left.insert (audio.left.end(), left.begin(), left.begin() + count);
            audio.right.insert (audio.right.end(), right.begin(), right.begin() + count);
            remaining -= count;
        }
    }

    YouKnow106Engine engine;
    Audio audio;
};

bool renderAuditScore (const Preset& preset, bool smoke, Audio& audio)
{
    ScoreRenderer renderer (preset, true); // signal metrics exclude BBD hiss
    if (! renderer.isHq())
        return false;

    if (smoke)
    {
        renderer.renderSeconds (0.05);
        renderer.noteOn (60);
        renderer.renderSeconds (0.45);
        renderer.noteOff (60);
        renderer.renderSeconds (0.20);
        for (const int note : { 48, 60, 67 })
            renderer.noteOn (note);
        renderer.renderSeconds (0.30);
        for (const int note : { 48, 60, 67 })
            renderer.noteOff (note);
        renderer.renderSeconds (0.20);
    }
    else
    {
        renderer.renderSeconds (0.250);
        for (const int note : { 36, 48, 60, 72 })
        {
            renderer.noteOn (note);
            renderer.renderSeconds (4.0);
            renderer.noteOff (note);
            renderer.renderSeconds (1.0);
        }
        for (const int note : { 48, 55, 60, 64, 67, 72 })
            renderer.noteOn (note);
        renderer.renderSeconds (4.0);
        for (const int note : { 48, 55, 60, 64, 67, 72 })
            renderer.noteOff (note);
        renderer.renderSeconds (2.0);
    }

    audio = renderer.takeAudio();
    return true;
}

bool renderPreviewScore (const Preset& preset, bool smoke, Audio& audio)
{
    ScoreRenderer renderer (preset, false); // exact product chorus-noise control
    if (! renderer.isHq())
        return false;

    renderer.renderSeconds (smoke ? 0.05 : 0.250);
    const std::array<int, 6> chord { 48, 55, 60, 64, 67, 72 };
    for (const int note : chord)
        renderer.noteOn (note);
    renderer.renderSeconds (smoke ? 0.30 : 4.0);
    for (const int note : chord)
        renderer.noteOff (note);
    renderer.renderSeconds (smoke ? 0.20 : 2.0);
    audio = renderer.takeAudio();
    return true;
}

struct Metrics
{
    bool finite = true;
    std::size_t frames = 0;
    std::uint64_t samplesOverZeroDbfs = 0;
    int rmsWindowCount = 0;
    int gatedWindowCount = 0;
    double samplePeak = 0.0;
    double maximumRms = 0.0;
    double gatedRms = 0.0;
    double gateThreshold = meterFloorLinear;
    double crestDb = 0.0;
};

Metrics analyse (const Audio& audio)
{
    Metrics result;
    result.frames = std::min (audio.left.size(), audio.right.size());
    std::vector<double> prefix (result.frames + 1, 0.0);

    for (std::size_t frame = 0; frame < result.frames; ++frame)
    {
        const double left = static_cast<double> (audio.left[frame]);
        const double right = static_cast<double> (audio.right[frame]);
        if (! std::isfinite (left) || ! std::isfinite (right))
        {
            result.finite = false;
            prefix[frame + 1] = prefix[frame];
            continue;
        }
        result.samplePeak = std::max ({ result.samplePeak, std::abs (left),
                                        std::abs (right) });
        result.samplesOverZeroDbfs += std::abs (left) > 1.0 ? 1u : 0u;
        result.samplesOverZeroDbfs += std::abs (right) > 1.0 ? 1u : 0u;
        prefix[frame + 1] = prefix[frame] + 0.5 * (left * left + right * right);
    }

    std::vector<double> windows;
    if (result.frames > 0)
    {
        const std::size_t window = std::min<std::size_t> (
            rmsWindowFrames, result.frames);
        const std::size_t lastStart = result.frames - window;
        for (std::size_t start = 0;; start += rmsHopFrames)
        {
            const double energy = prefix[start + window] - prefix[start];
            windows.push_back (std::sqrt (std::max (0.0, energy)
                                          / static_cast<double> (window)));
            if (start >= lastStart || lastStart - start < rmsHopFrames)
                break;
        }
    }

    result.rmsWindowCount = static_cast<int> (windows.size());
    for (const double rms : windows)
        result.maximumRms = std::max (result.maximumRms, rms);
    result.gateThreshold = std::max (meterFloorLinear, result.maximumRms * 0.1);

    double gatedSquareSum = 0.0;
    for (const double rms : windows)
        if (rms >= result.gateThreshold)
        {
            gatedSquareSum += rms * rms;
            ++result.gatedWindowCount;
        }
    if (result.gatedWindowCount > 0)
        result.gatedRms = std::sqrt (
            gatedSquareSum / static_cast<double> (result.gatedWindowCount));
    result.crestDb = result.gatedRms > 0.0
        ? 20.0 * std::log10 (result.samplePeak / result.gatedRms) : 0.0;
    return result;
}

std::string csvField (std::string_view text)
{
    std::string result = "\"";
    for (const char character : text)
    {
        if (character == '"')
            result += '"';
        result += character;
    }
    result += '"';
    return result;
}

std::string toneHex (const Preset& preset)
{
    std::array<std::uint8_t, youknow106::sysex::toneByteCount> bytes {};
    youknow106::sysex::toneBytesFromPatch (preset.patch, bytes.data());
    std::ostringstream text;
    text << std::hex << std::setfill ('0');
    for (const auto byte : bytes)
        text << std::setw (2) << static_cast<unsigned> (byte);
    return text.str();
}

struct AuditRow
{
    int presetIndex = 0;
    const Preset* preset = nullptr;
    Metrics metrics;
};

double corpusMedianGatedDb (const std::vector<AuditRow>& rows)
{
    std::vector<double> levels;
    levels.reserve (rows.size());
    for (const auto& row : rows)
        levels.push_back (toDb (row.metrics.gatedRms));
    if (levels.empty())
        return meterFloorDb;
    std::sort (levels.begin(), levels.end());
    const auto middle = levels.size() / 2;
    return levels.size() % 2 != 0
        ? levels[middle] : 0.5 * (levels[middle - 1] + levels[middle]);
}

std::string warningsFor (const Metrics& metrics, double corpusMedianDb,
                         bool includeCorpusLevel)
{
    std::string warnings;
    const auto append = [&warnings] (const std::string& warning)
    {
        if (! warnings.empty())
            warnings += "; ";
        warnings += warning;
    };
    if (! metrics.finite)
        append ("non-finite sample");
    if (metrics.samplesOverZeroDbfs > 0)
        append (std::to_string (metrics.samplesOverZeroDbfs)
                + " channel samples above 0 dBFS");
    else if (metrics.samplePeak >= previewCeiling)
        append ("sample peak within 1 dB of 0 dBFS");
    if (metrics.gatedWindowCount == 0)
        append ("no RMS window reached the -140 dBFS floor");
    if (toDb (metrics.maximumRms) < -60.0)
        append ("max 400 ms RMS below -60 dBFS");
    if (includeCorpusLevel
        && std::abs (toDb (metrics.gatedRms) - corpusMedianDb) > 18.0)
        append ("gated RMS outside corpus median +/-18 dB");
    return warnings;
}

std::string makeCsv (const std::vector<AuditRow>& rows, double corpusMedianDb,
                     bool includeCorpusLevel)
{
    std::ostringstream csv;
    csv << "slot,name,tone_bytes_hex,volume,bender_dco,bender_vcf,bender_lfo,"
           "portamento,key_mode,transpose,master_tune_cents,velocity,unit_character,"
           "product_chorus_noise,polyphony,oversample_factor,frames,sample_peak,"
           "sample_peak_dbfs,"
           "samples_over_0dbfs,max_400ms_stereo_rms,max_400ms_stereo_rms_dbfs,"
           "relative_gate_db,gated_windows,total_windows,relative_gated_stereo_rms,"
           "relative_gated_stereo_rms_dbfs,corpus_median_gated_rms_dbfs,"
           "gated_delta_from_median_db,crest_db,warnings\n";
    csv << std::fixed << std::setprecision (9);
    for (const auto& row : rows)
    {
        const auto& preset = *row.preset;
        const auto& controls = preset.controls;
        const auto& metrics = row.metrics;
        csv << preset.number << ',' << csvField (preset.name) << ','
            << toneHex (preset) << ',' << controls.volume << ','
            << controls.benderDco << ',' << controls.benderVcf << ','
            << controls.benderLfo << ',' << controls.portamento << ','
            << csvField (keyModeName (controls.keyMode)) << ','
            << controls.transpose << ',' << controls.masterTune << ','
            << controls.velocity << ',' << controls.calibration << ','
            << controls.chorusNoise << ',' << controls.polyphony << ','
            << auditOversampleFactor << ',' << metrics.frames << ','
            << metrics.samplePeak << ',' << toDb (metrics.samplePeak) << ','
            << metrics.samplesOverZeroDbfs << ',' << metrics.maximumRms << ','
            << toDb (metrics.maximumRms) << ',' << toDb (metrics.gateThreshold)
            << ',' << metrics.gatedWindowCount << ',' << metrics.rmsWindowCount
            << ',' << metrics.gatedRms << ',' << toDb (metrics.gatedRms) << ','
            << corpusMedianDb << ','
            << toDb (metrics.gatedRms) - corpusMedianDb << ','
            << metrics.crestDb << ','
            << csvField (warningsFor (metrics, corpusMedianDb,
                                      includeCorpusLevel)) << '\n';
    }
    return csv.str();
}

std::string makeReport (const std::vector<AuditRow>& rows, bool smoke,
                        double commonPreviewGain, double corpusMedianDb)
{
    std::ostringstream report;
    report << ownershipMarker << "\n\n# YouKnow106 factory-preset gain audit\n\n"
           << "Generated by `YouKnow106AuditFactoryPresets` from the immutable "
              "factory tone-memory states and their product controls. The "
              "eighteen tone bytes are the hardware's and are never touched, and "
              "there is no per-preset corrective gain, EQ or limiting anywhere "
              "in the signal path. What each preset does carry is a VR1 output "
              "shaft position, exactly as a player would set one; those "
              "positions are listed in `Source/DSP/YouKnow106Presets.cpp` and "
              "are attenuation only.\n\n"
           << "This run enforces two absolute contracts and fails if either is "
              "broken: no preset may peak above "
           << std::fixed << std::setprecision (1) << factoryPeakCeilingDbfs
           << " dBFS, and none may exceed "
           << factoryGatedCeilingDbfs
           << " dBFS gated RMS. Levels below those remain measurements rather "
              "than targets: a noise sweep is quieter than an organ because the "
              "instrument makes it quieter.\n\n"
           << "- Engine: 48 kHz, quality 4x.\n"
           << "- Analysis override: Chorus Noise = 0 so the signal metrics do not "
              "measure the continuously running BBD hiss.\n";
    if (smoke)
        report << "- Mode: fast smoke fixture (two presets and shortened timings); "
                  "do not use these rows as the production corpus audit.\n";
    else
        report << "- Score: 250 ms preroll; notes 36, 48, 60 and 72, each held 4 s "
                  "and released 1 s; then 48/55/60/64/67/72 held 4 s and released "
                  "2 s. All note velocities are 1.0. Long release tails deliberately "
                  "overlap the following events, so this is a musical/headroom stress "
                  "score rather than an isolated control-to-gain calibration.\n";
    report << "- RMS: 400 ms stereo windows at 100 ms hops. The relative-gated "
              "RMS is the energy mean of windows at or above max RMS -20 dB, "
              "with a -140 dBFS floor. Crest is sample peak / gated RMS.\n"
           << "- Preview delivery: exact product controls including Chorus Noise; "
              "one common non-boosting gain of " << std::fixed << std::setprecision (6)
           << commonPreviewGain << " (" << std::setprecision (2)
           << toDb (commonPreviewGain) << " dB) for every file; no per-file "
              "normalisation. A raised-cosine 50 ms file-end fade prevents an "
              "audition-player stop click and is not part of the synth render.\n\n"
           << "Machine-readable values and exact 18-byte tone hex are in "
              "`metrics.csv`.\n\n"
           << "## Corpus summary\n\n";
    int overloaded = 0;
    int nearSilent = 0;
    int outsideWideBand = 0;
    const AuditRow* quietest = nullptr;
    const AuditRow* loudest = nullptr;
    for (const auto& row : rows)
    {
        const double level = toDb (row.metrics.gatedRms);
        overloaded += row.metrics.samplesOverZeroDbfs > 0 ? 1 : 0;
        nearSilent += toDb (row.metrics.maximumRms) < -60.0 ? 1 : 0;
        outsideWideBand += std::abs (level - corpusMedianDb) > 18.0 ? 1 : 0;
        if (quietest == nullptr || level < toDb (quietest->metrics.gatedRms))
            quietest = &row;
        if (loudest == nullptr || level > toDb (loudest->metrics.gatedRms))
            loudest = &row;
    }
    report << "- Corpus median gated RMS: " << std::setprecision (2)
           << corpusMedianDb << " dBFS.\n"
           << "- Presets with samples above 0 dBFS: " << overloaded << ".\n"
           << "- Presets below -60 dBFS max 400 ms RMS: " << nearSilent << ".\n"
           << "- Presets outside median +/-18 dB: " << outsideWideBand << ".\n";
    if (quietest != nullptr && loudest != nullptr)
        report << "- Gated-RMS span: " << quietest->preset->number << " "
               << quietest->preset->name << " at "
               << toDb (quietest->metrics.gatedRms) << " dBFS to "
               << loudest->preset->number << " " << loudest->preset->name
               << " at " << toDb (loudest->metrics.gatedRms) << " dBFS.\n";
    report << "\nThese are review flags, not equal-loudness failures. The bank "
              "intentionally mixes transient, sustained, effect and Solo Unison "
              "programs, and the circuit-derived common VCA LEVEL law directly "
              "affects their relative results. OQ-02 now covers installed-unit "
              "variation rather than the nominal curve.\n\n"
           << "| Slot | Name | Peak dBFS | >0 dBFS samples | Max 400 ms RMS | "
              "Gated RMS | Crest | Warnings |\n"
           << "| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |\n";
    for (const auto& row : rows)
    {
        const auto& metrics = row.metrics;
        const auto warnings = warningsFor (metrics, corpusMedianDb, ! smoke);
        report << "| " << row.preset->number << " | " << row.preset->name << " | "
               << std::setprecision (2) << toDb (metrics.samplePeak) << " | "
               << metrics.samplesOverZeroDbfs << " | " << toDb (metrics.maximumRms)
               << " | " << toDb (metrics.gatedRms) << " | " << metrics.crestDb
               << " | " << (warnings.empty() ? "—" : warnings) << " |\n";
    }
    report << "\n## Fixed previews\n\n";
    for (const auto& preview : previewSpecs)
        report << "- `previews/" << preview.fileName << "`\n";
    return report.str();
}

bool writeText (const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    output.write (text.data(), static_cast<std::streamsize> (text.size()));
    return output.good();
}

void writeLittleEndian (std::ostream& output, std::uint32_t value, int bytes)
{
    for (int byte = 0; byte < bytes; ++byte)
        output.put (static_cast<char> ((value >> (8 * byte)) & 0xffu));
}

double fileFade (std::size_t frame, std::size_t frameCount)
{
    const auto fade = std::min<std::size_t> (previewFadeFrames, frameCount);
    if (fade < 2 || frame < frameCount - fade)
        return 1.0;
    const double position = static_cast<double> (frame - (frameCount - fade))
                          / static_cast<double> (fade - 1);
    return 0.5 * (1.0 + std::cos (3.14159265358979323846 * position));
}

bool writeWav16 (const std::filesystem::path& path, const Audio& audio,
                 double commonGain)
{
    const auto frames = std::min (audio.left.size(), audio.right.size());
    if (frames > (std::numeric_limits<std::uint32_t>::max() - 36u) / 4u)
        return false;
    const auto dataBytes = static_cast<std::uint32_t> (frames * 4u);
    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    if (! output)
        return false;

    output.write ("RIFF", 4);
    writeLittleEndian (output, 36u + dataBytes, 4);
    output.write ("WAVEfmt ", 8);
    writeLittleEndian (output, 16u, 4);
    writeLittleEndian (output, 1u, 2);
    writeLittleEndian (output, 2u, 2);
    writeLittleEndian (output, static_cast<std::uint32_t> (sampleRate), 4);
    writeLittleEndian (output, static_cast<std::uint32_t> (sampleRate) * 4u, 4);
    writeLittleEndian (output, 4u, 2);
    writeLittleEndian (output, 16u, 2);
    output.write ("data", 4);
    writeLittleEndian (output, dataBytes, 4);

    const auto encode = [] (double value)
    {
        value = std::clamp (std::isfinite (value) ? value : 0.0, -1.0, 1.0);
        return static_cast<std::uint16_t> (static_cast<std::int16_t> (
            std::lround (value * 32767.0)));
    };
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const double gain = commonGain * fileFade (frame, frames);
        writeLittleEndian (output, encode (audio.left[frame] * gain), 2);
        writeLittleEndian (output, encode (audio.right[frame] * gain), 2);
    }
    return output.good();
}

bool ensureOwnedDirectory (const std::filesystem::path& directory)
{
    std::error_code error;
    if (! std::filesystem::exists (directory))
        std::filesystem::create_directories (directory, error);
    if (error || ! std::filesystem::is_directory (directory))
        return false;

    const auto readme = directory / "README.md";
    if (std::filesystem::exists (readme))
    {
        std::ifstream input (readme, std::ios::binary);
        const std::string contents ((std::istreambuf_iterator<char> (input)),
                                    std::istreambuf_iterator<char>());
        if (contents.find (ownershipMarker) != std::string::npos)
            return true;
    }

    if (std::filesystem::directory_iterator (directory)
        != std::filesystem::directory_iterator {})
    {
        std::fprintf (stderr,
                      "refusing unowned non-empty output directory: %s\n",
                      directory.string().c_str());
        return false;
    }

    // Claim an empty/new directory before rendering. Subsequent runs may only
    // reach exact filenames below after finding this unique marker.
    return writeText (readme, std::string (ownershipMarker)
        + "\n\nFactory audit output is being generated.\n");
}

int run (bool smoke, const std::filesystem::path& directory)
{
    if (! ensureOwnedDirectory (directory))
        return 1;

    const auto& bank = youknow106::presets::factoryBank();
    // Every trim attenuates. A factory preset louder than the instrument's own
    // nominal shaft position would be the product shouting over the player's
    // reference, so reject that outright rather than measuring it later.
    for (const auto& preset : bank)
        if (preset.controls.volume
            > youknow106::presets::Preset::Controls {}.volume + 1.0e-6f)
        {
            std::fprintf (stderr,
                          "%s asks for volume %.3f, above the panel default; "
                          "audit aborted\n",
                          preset.number,
                          static_cast<double> (preset.controls.volume));
            return 1;
        }
    for (const auto& spec : previewSpecs)
        if (std::string_view (bank[static_cast<std::size_t> (spec.presetIndex)].number)
            != spec.expectedNumber)
        {
            std::fprintf (stderr, "preview index no longer names %s\n",
                          spec.expectedNumber);
            return 1;
        }

    const std::array<int, 2> smokeIndices { 0, 31 };
    std::vector<AuditRow> rows;
    const int auditCount = smoke ? static_cast<int> (smokeIndices.size())
                                 : youknow106::presets::presetCount;
    rows.resize (static_cast<std::size_t> (auditCount));
    std::atomic<int> nextAudit { 0 };
    std::atomic<bool> renderFailure { false };
    std::atomic<bool> numericalFailure { false };
    const unsigned availableWorkers = std::max (1u, std::thread::hardware_concurrency());
    const int workerCount = smoke ? 1 : std::min ({ auditCount, 8,
        static_cast<int> (availableWorkers) });
    const auto auditWorker = [&]
    {
        for (;;)
        {
            const int audit = nextAudit.fetch_add (1, std::memory_order_relaxed);
            if (audit >= auditCount)
                return;
            const int index = smoke
                ? smokeIndices[static_cast<std::size_t> (audit)] : audit;
            const auto& preset = bank[static_cast<std::size_t> (index)];
            try
            {
                Audio audio;
                if (! renderAuditScore (preset, smoke, audio))
                {
                    renderFailure.store (true, std::memory_order_relaxed);
                    return;
                }
                auto metrics = analyse (audio);
                if (! metrics.finite)
                    numericalFailure.store (true, std::memory_order_relaxed);
                rows[static_cast<std::size_t> (audit)] = {
                    index, &preset, metrics
                };
            }
            catch (...)
            {
                renderFailure.store (true, std::memory_order_relaxed);
                return;
            }
        }
    };
    std::vector<std::thread> workers;
    workers.reserve (static_cast<std::size_t> (workerCount));
    for (int worker = 0; worker < workerCount; ++worker)
        workers.emplace_back (auditWorker);
    for (auto& worker : workers)
        worker.join();
    if (renderFailure.load (std::memory_order_relaxed))
    {
        std::fprintf (stderr, "factory audit render failed\n");
        return 1;
    }
    for (const auto& row : rows)
        std::printf ("Audited %s %-30s peak %7.2f dBFS\n",
                     row.preset->number, row.preset->name,
                     toDb (row.metrics.samplePeak));


    struct PreviewAudio
    {
        const PreviewSpec* spec = nullptr;
        Audio audio;
    };
    std::vector<PreviewAudio> previews;
    previews.reserve (previewSpecs.size());
    double globalPreviewPeak = 0.0;
    for (const auto& spec : previewSpecs)
    {
        const auto& preset = bank[static_cast<std::size_t> (spec.presetIndex)];
        PreviewAudio rendered;
        rendered.spec = &spec;
        if (! renderPreviewScore (preset, smoke, rendered.audio))
            return 1;
        const auto previewMetrics = analyse (rendered.audio);
        if (! previewMetrics.finite)
        {
            numericalFailure.store (true, std::memory_order_relaxed);
            std::fprintf (stderr, "%s preview contained non-finite audio\n",
                          preset.number);
        }
        globalPreviewPeak = std::max (globalPreviewPeak,
                                      previewMetrics.samplePeak);
        previews.push_back (std::move (rendered));
    }
    const double commonGain = globalPreviewPeak > previewCeiling
        ? previewCeiling / globalPreviewPeak : 1.0;

    std::error_code error;
    const auto previewDirectory = directory / "previews";
    std::filesystem::create_directories (previewDirectory, error);
    if (error)
        return 1;
    for (const auto& preview : previews)
    {
        const auto path = previewDirectory / preview.spec->fileName;
        if (! writeWav16 (path, preview.audio, commonGain))
        {
            std::fprintf (stderr, "could not write %s\n", path.string().c_str());
            return 1;
        }
    }

    const double corpusMedianDb = corpusMedianGatedDb (rows);
    if (! writeText (directory / "metrics.csv",
                     makeCsv (rows, corpusMedianDb, ! smoke))
        || ! writeText (directory / "README.md",
                        makeReport (rows, smoke, commonGain, corpusMedianDb)))
    {
        std::fprintf (stderr, "could not write audit reports in %s\n",
                      directory.string().c_str());
        return 1;
    }

    std::printf ("Wrote %zu audit rows and %zu previews to %s\n", rows.size(),
                 previews.size(), directory.string().c_str());

    // The bank's level contract, enforced rather than merely reported, and
    // deliberately last: a failing audit still publishes the CSV and report
    // that say which presets need retrimming and by how much. Both thresholds
    // are absolute, so one retrimmed preset cannot drag them along with it.
    bool levelFailure = false;
    for (const auto& row : rows)
    {
        const double peakDb = toDb (row.metrics.samplePeak);
        const double gatedDb = toDb (row.metrics.gatedRms);
        if (peakDb > factoryPeakCeilingDbfs)
        {
            std::fprintf (stderr,
                          "%s peaks at %+.2f dBFS, above the %.1f dBFS factory "
                          "ceiling\n",
                          row.preset->number, peakDb, factoryPeakCeilingDbfs);
            levelFailure = true;
        }
        if (gatedDb > factoryGatedCeilingDbfs)
        {
            std::fprintf (stderr,
                          "%s reaches %+.2f dBFS gated, above the %.1f dBFS "
                          "factory loudness ceiling\n",
                          row.preset->number, gatedDb, factoryGatedCeilingDbfs);
            levelFailure = true;
        }
    }

    // A smoke run holds the same two absolute ceilings, but it only sees two
    // presets and a shortened, thinner score, so passing it certifies nothing
    // about the bank. Say so, or a green CI reads as a certified release.
    if (! levelFailure)
        std::printf (smoke
                         ? "Level contract spot-checked on %zu preset(s) with the "
                           "smoke score; run without --smoke to certify the bank\n"
                         : "Level contract passed for all %zu presets: none above "
                           "the peak or gated ceiling\n",
                     rows.size());

    return (levelFailure
            || numericalFailure.load (std::memory_order_relaxed)) ? 1 : 0;
}
} // namespace

int main (int argc, char** argv)
{
    bool smoke = false;
    bool pathWasProvided = false;
    std::filesystem::path directory = YOUKNOW106_FACTORY_AUDIT_DEFAULT_OUTPUT;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string_view value = argv[argument];
        if (value == "--smoke")
            smoke = true;
        else if (value == "--help" || value == "-h")
        {
            std::printf ("usage: YouKnow106AuditFactoryPresets [--smoke] "
                         "[output-directory]\n");
            return 0;
        }
        else if (! value.empty() && value.front() == '-')
        {
            std::fprintf (stderr, "unknown option: %s\n", argv[argument]);
            return 2;
        }
        else if (pathWasProvided)
        {
            std::fprintf (stderr, "only one output directory may be supplied\n");
            return 2;
        }
        else
        {
            directory = argv[argument];
            pathWasProvided = true;
        }
    }
    if (smoke && ! pathWasProvided)
        directory = "factory-preset-audit-smoke";
    return run (smoke, directory);
}

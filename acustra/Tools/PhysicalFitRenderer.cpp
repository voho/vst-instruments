// Export the bounded physical-fit corpus and render its AcustraEngine peers.
// Targets retain their recorded pre-roll/onset and use the same calibrated
// playback gain as dense::Sampler. Model renders use default public controls;
// the supplied vector changes only the physical calibration named below.
// --test renders the frozen test split instead: the archtop layers and takes no
// fit has ever rendered, scored once per release and never fitted. See
// makeTestSchedule.

#include "DSP/AcustraEngine.h"
#include "DSP/SampleBank/EmbeddedGuitarBank.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
using acustra::AcustraEngine;
using acustra::EngineParameters;
using acustra::MaterialCalibration;
using acustra::PhysicalCalibration;
using acustra::StringMaterial;
using acustra::fittedPhysicalCalibration;
using acustra::dense::Bank;
using acustra::dense::Library;
using acustra::dense::Sampler;
using acustra::dense::ZoneView;

constexpr int modelSampleRate = 48000;
constexpr int renderBlockSize = 127;
constexpr double renderSeconds = 4.2;
constexpr std::size_t calibrationValueCount = 31;
constexpr float int16Scale = 1.0f / 32768.0f;

enum class Material
{
    Nylon,
    Steel,
    // The Eastman E1D regions are the only flat-top steel acoustic in the
    // bank, and the steel fit is against a miked archtop. They are never
    // fitted or used to gate anything: they are a standing reading of how
    // far the instrument sits from the kind of guitar it claims to be.
    SteelFlatTop
};

struct Example
{
    Material material {};
    int midi {};
    int velocity {};
    int roundRobin {};
};

struct Schedule
{
    std::vector<Example> train;
    std::vector<Example> validation;
    std::vector<Example> flatTop;
};

struct AudioFile
{
    std::string path;
    std::uint32_t sampleRate {};
    std::uint8_t channels {};
};

struct ManifestRow
{
    Example example;
    std::string id;
    std::string dynamicGroup;
    AudioFile target;
    AudioFile model;
    AudioFile sampleModel;
    // Library::prepare equalises one-second energy across every zone of an
    // archtop root, so the exported takes of a note have all been made the same
    // loudness. Recording this lets the scorer's floor mode undo it and measure
    // what the player did rather than what the trim did.
    float playbackTrim { 1.0f };
};

using CalibrationValues = std::array<float, calibrationValueCount>;
using ModelKey = std::tuple<Material, int, int>;
using SampleModelKey = std::tuple<Material, int, int, int>;

// Reject rather than silently clamp so the caller's CLI vector is the
// calibration that was actually rendered. These mirror AcustraEngine's bounds.
constexpr CalibrationValues calibrationMinimums {{
    0.96f, 0.05f, 0.25f, -6.0f, 0.0f,
    0.4f, 0.35f, 0.35f, 0.0f, 0.7f, 0.0f,
    0.25f, 0.4f, 0.35f, 0.35f, 0.0f, 0.7f, 0.0f,
    -1.0f, 0.25f, 0.0f, -0.06f, 0.5f, 0.0f, 100.0f, 0.00325f,
    0.0f, 10.0f, 0.0f,
    0.0f, 0.0f,
}};

constexpr CalibrationValues calibrationMaximums {{
    1.04f, 1.8f, 4.0f, 6.0f, 0.12f,
    2.0f, 3.0f, 2.5f, 3.0f, 1.3f, 1.2f,
    4.0f, 2.0f, 3.0f, 2.5f, 3.0f, 1.3f, 1.2f,
    1.0f, 32.0f, 0.04f, 0.05f, 4.0f, 0.02f, 8000.0f, 0.060f,
    0.5f, 400.0f, 0.82e-3f,
    0.707107f, 0.707107f,
}};

const char* materialName(Material material) noexcept
{
    switch (material)
    {
        case Material::Steel: return "steel";
        case Material::SteelFlatTop: return "flattop";
        default: return "nylon";
    }
}

Bank targetBank(Material material) noexcept
{
    switch (material)
    {
        case Material::Steel: return Bank::SteelPicked;
        case Material::SteelFlatTop: return Bank::SteelPlucked;
        default: return Bank::Nylon;
    }
}

StringMaterial engineMaterial(Material material) noexcept
{
    return material == Material::Nylon
        ? StringMaterial::Nylon : StringMaterial::Steel;
}

template <std::size_t NoteCount, std::size_t VelocityCount,
          std::size_t RoundRobinCount>
void appendCartesian(std::vector<Example>& output, Material material,
                     const std::array<int, NoteCount>& notes,
                     const std::array<int, VelocityCount>& velocities,
                     const std::array<int, RoundRobinCount>& roundRobins)
{
    for (const int midi : notes)
        for (const int velocity : velocities)
            for (const int roundRobin : roundRobins)
                output.push_back({ material, midi, velocity, roundRobin });
}

Schedule makeSchedule(bool smoke)
{
    Schedule result;
    if (smoke)
    {
        appendCartesian(result.train, Material::Steel,
                        std::array { 40 }, std::array { 16, 112 },
                        std::array { 0 });
        appendCartesian(result.train, Material::Nylon,
                        std::array { 40 }, std::array { 91 },
                        std::array { 0 });
        appendCartesian(result.validation, Material::Steel,
                        std::array { 42 }, std::array { 48, 80 },
                        std::array { 3 });
        appendCartesian(result.validation, Material::Nylon,
                        std::array { 41 }, std::array { 91 },
                        std::array { 0 });
        return result;
    }

    appendCartesian(result.train, Material::Steel,
                    std::array { 40, 45, 51, 57, 60, 66, 72, 78, 84 },
                    std::array { 16, 112 }, std::array { 0, 1, 2 });
    appendCartesian(result.validation, Material::Steel,
                    std::array { 42, 48, 54, 63, 69, 75, 81 },
                    std::array { 48, 80 }, std::array { 3 });
    // Every FreePats nylon region the offline bank holds that is playable on
    // the six modelled strings and is not in the validation split. Nylon has
    // one captured dynamic per region, so notes are the only axis it has;
    // fitting seven nylon parameters on ten of forty-one available rows left
    // that stage close to unidentifiable and it repeatedly stalled.
    appendCartesian(result.train, Material::Nylon,
                    std::array { 40, 45, 48, 50, 53, 54, 55, 56, 58, 59, 60,
                                 62, 63, 64, 65, 67, 69, 70, 72, 73, 74, 75,
                                 77, 78, 79, 80, 82, 83, 84 },
                    std::array { 91 }, std::array { 0 });
    appendCartesian(result.validation, Material::Nylon,
                    std::array { 41, 43, 47, 52, 57, 61, 66, 71, 76, 81 },
                    std::array { 91 }, std::array { 0 });
    // The eight Eastman E1D flat-top roots, at the one dynamic they were
    // captured at. This split is reported and never fitted; nothing in the
    // optimiser reads it.
    appendCartesian(result.flatTop, Material::SteelFlatTop,
                    std::array { 40, 46, 52, 58, 64, 71, 77, 83 },
                    std::array { 91 }, std::array { 0 });
    return result;
}

// The frozen test split: every captured archtop layer and take, on the roots
// the fit already uses, that the fit itself never renders. Shinyguitar captured
// four velocity layers and four round robins for each root, 16 regions; the fit
// renders layers 1 and 4 at takes 1-3 on nine roots and layers 2-3 at take 4 on
// seven others, 68 regions, which leaves 188 of those roots' 256 untouched by
// every fit, screen and refit this instrument has run. It is scored once per
// release and never fitted, so it is the only split that has not been selected
// on. The bank's seventeenth root (37) is left out: the lowest modelled string
// is E2/MIDI 40 and a MIDI 37 row would compare a C#2 recording with an open
// low E, measuring the transposition rather than the model.
std::vector<Example> makeTestSchedule()
{
    const Schedule fit = makeSchedule(false);
    std::vector<std::tuple<int, int, int>> used;
    std::vector<int> roots;
    for (const auto* examples : { &fit.train, &fit.validation })
        for (const auto& example : *examples)
        {
            if (example.material != Material::Steel)
                continue;
            used.push_back({ example.midi, example.velocity,
                             example.roundRobin });
            roots.push_back(example.midi);
        }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
    std::sort(used.begin(), used.end());

    std::vector<Example> result;
    for (const int midi : roots)
        // The centre of each captured velocity range: 1-32, 33-64, 65-96 and
        // 97-127, the same four the fit schedule addresses.
        for (const int velocity : { 16, 48, 80, 112 })
            for (const int roundRobin : { 0, 1, 2, 3 })
                if (!std::binary_search(used.begin(), used.end(),
                                        std::tuple { midi, velocity, roundRobin }))
                    result.push_back({ Material::Steel, midi, velocity,
                                       roundRobin });
    return result;
}

std::string stem(const Example& example)
{
    return std::string(materialName(example.material))
        + "-m" + std::to_string(example.midi)
        + "-v" + std::to_string(example.velocity);
}

std::string exampleId(const Example& example)
{
    return stem(example) + "-rr" + std::to_string(example.roundRobin);
}

std::string dynamicGroup(const Example& example)
{
    return std::string(materialName(example.material))
        + "-m" + std::to_string(example.midi)
        + "-rr" + std::to_string(example.roundRobin);
}

std::string targetFileName(const Example& example)
{
    return "target-" + exampleId(example) + ".f32";
}

std::string modelFileName(const Example& example)
{
    return "model-" + stem(example) + ".f32";
}

std::size_t durationFrames(std::uint32_t sampleRate)
{
    return static_cast<std::size_t>(std::llround(
        renderSeconds * static_cast<double>(sampleRate)));
}

bool finite(const std::vector<float>& samples)
{
    return std::all_of(samples.begin(), samples.end(),
        [] (float value) { return std::isfinite(value); });
}

void writeF32(const std::filesystem::path& path,
              const std::vector<float>& samples)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("could not create " + path.string());

    if constexpr (std::endian::native == std::endian::little)
    {
        output.write(reinterpret_cast<const char*>(samples.data()),
                     static_cast<std::streamsize>(samples.size()
                         * sizeof(float)));
    }
    else
    {
        for (const float sample : samples)
        {
            const auto bits = std::bit_cast<std::uint32_t>(sample);
            const std::array<char, 4> bytes {{
                static_cast<char>(bits),
                static_cast<char>(bits >> 8),
                static_cast<char>(bits >> 16),
                static_cast<char>(bits >> 24),
            }};
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
    }
    output.close();
    if (output.fail())
        throw std::runtime_error("could not write " + path.string());
}

const ZoneView& findExactZone(const Library& library, const Example& example)
{
    const Bank bank = targetBank(example.material);
    const auto* zone = library.find(
        bank, example.midi, example.velocity,
        static_cast<std::uint8_t>(example.roundRobin));
    if (zone == nullptr || zone->bank != bank
        || zone->rootMidi != example.midi
        || zone->roundRobin != example.roundRobin
        || example.velocity < zone->lowVelocity
        || example.velocity > zone->highVelocity)
    {
        throw std::runtime_error("no exact reference zone for "
            + exampleId(example));
    }
    if (zone->samples == nullptr || zone->frames == 0
        || zone->sampleRate < 8000
        || (zone->channels != 1 && zone->channels != 2))
    {
        throw std::runtime_error("malformed reference zone for "
            + exampleId(example));
    }
    return *zone;
}

float targetGain(const ZoneView& zone, const Example& example)
{
    const float velocity = static_cast<float>(example.velocity) / 127.0f;
    const float velocityGain = std::pow(velocity, 0.82f);
    if (example.material == Material::Steel)
        return 0.55f * zone.playbackTrim * velocityGain;

    const float peak = std::max(1.0f, static_cast<float>(zone.peak));
    const float peakNormalisation = std::clamp(
        0.55f / (peak * int16Scale), 0.25f, 4.0f);
    return velocityGain * peakNormalisation;
}

std::vector<float> exportTarget(const ZoneView& zone,
                                const Example& example)
{
    const auto frames = durationFrames(zone.sampleRate);
    std::vector<float> output(frames * zone.channels, 0.0f);
    const auto copiedFrames = std::min<std::size_t>(frames, zone.frames);
    const float gain = int16Scale * targetGain(zone, example);
    for (std::size_t frame = 0; frame < copiedFrames; ++frame)
        for (std::uint8_t channel = 0; channel < zone.channels; ++channel)
            output[frame * zone.channels + channel]
                = static_cast<float>(zone.samples[frame * zone.channels + channel])
                * gain;
    return output;
}

PhysicalCalibration makeCalibration(const CalibrationValues& values)
{
    PhysicalCalibration calibration = fittedPhysicalCalibration;
    calibration.bodyFrequencyScale = values[0];
    calibration.bodyQScale = values[1];
    calibration.bridgeMobilityScale = values[2];
    calibration.residueTiltDbPerOctave = values[3];
    calibration.directGain = values[4];

    const auto setMaterial = [&] (MaterialCalibration& material,
                                  std::size_t offset)
    {
        material.stiffnessScale = values[offset];
        material.fundamentalT60Scale = values[offset + 1];
        material.frequencyLossScale = values[offset + 2];
        material.apertureScale = values[offset + 3];
        material.transientScale = values[offset + 4];
        material.pluckDistanceScale = values[offset + 5];
        material.velocityBrightnessDepth = values[offset + 6];
    };
    // Nylon has no stiffnessScale in the calibration array (its bending
    // stiffness is Woodhouse's measured EI, not a fitted scale - see
    // nylonBendingEI in AcustraEngine.cpp), so it gets six values, not seven.
    calibration.nylon.fundamentalT60Scale = values[5];
    calibration.nylon.frequencyLossScale = values[6];
    calibration.nylon.apertureScale = values[7];
    calibration.nylon.transientScale = values[8];
    calibration.nylon.pluckDistanceScale = values[9];
    calibration.nylon.velocityBrightnessDepth = values[10];
    setMaterial(calibration.steel, 11);
    calibration.apertureRegisterExponent = values[18];
    calibration.lowBodyModeGain = values[19];
    calibration.steelDisplacementScaleMetres = values[20];
    calibration.steelFretT60Slope = values[21];
    calibration.highLossCutoffScale = values[22];
    calibration.bridgeConductanceFloor = values[23];
    calibration.bridgeConductanceCornerHz = values[24];
    calibration.bridgeTailLengthMetres = values[25];
    calibration.longitudinalGain = values[26];
    calibration.longitudinalQ = values[27];
    calibration.polarisationEndCorrectionMetres = values[28];
    calibration.steelSaddleBreakSine = values[29];
    calibration.nylonSaddleBreakSine = values[30];
    return calibration;
}

std::vector<float> renderModel(Material material, int midi, int velocity,
                               const PhysicalCalibration& calibration)
{
    AcustraEngine engine;
    EngineParameters parameters;
    parameters.stringMaterial = engineMaterial(material);
    engine.setParameters(parameters);
    engine.setPhysicalCalibration(calibration);
    engine.prepare(modelSampleRate, renderBlockSize);
    engine.noteOn(midi, static_cast<float>(velocity) / 127.0f);

    const auto frames = durationFrames(modelSampleRate);
    std::vector<float> output(frames * 2);
    std::array<float, renderBlockSize> left {};
    std::array<float, renderBlockSize> right {};
    for (std::size_t offset = 0; offset < frames; offset += renderBlockSize)
    {
        const int count = static_cast<int>(std::min<std::size_t>(
            renderBlockSize, frames - offset));
        engine.process(left.data(), right.data(), count);
        for (int frame = 0; frame < count; ++frame)
        {
            output[2 * (offset + static_cast<std::size_t>(frame))]
                = left[static_cast<std::size_t>(frame)];
            output[2 * (offset + static_cast<std::size_t>(frame)) + 1]
                = right[static_cast<std::size_t>(frame)];
        }
    }
    return output;
}

std::vector<float> renderSampleModel(const Library& library,
                                     const Example& example)
{
    Sampler sampler(library);
    sampler.setOutputSampleRate(modelSampleRate);
    const auto bank = targetBank(example.material);

    // A fresh historical sampler starts at round robin zero. Advance its
    // deterministic selector to the captured take named by this benchmark row;
    // discarded voices never enter the measured render.
    for (int roundRobin = 0; roundRobin < example.roundRobin; ++roundRobin)
    {
        if (!sampler.noteOn(0, bank, example.midi,
                            static_cast<float>(example.velocity) / 127.0f,
                            0.0f, 0.0f))
            throw std::runtime_error("legacy sampler could not select "
                + exampleId(example));
        sampler.noteOff(0, 1.0f);
        // Let both the selected and retrigger-retirement slots reach silence
        // without resetting the sampler's round-robin counters.
        std::array<float, 1024> discardedLeft {};
        std::array<float, 1024> discardedRight {};
        sampler.process(discardedLeft.data(), discardedRight.data(),
                        discardedLeft.size());
    }
    if (!sampler.noteOn(0, bank, example.midi,
                        static_cast<float>(example.velocity) / 127.0f,
                        0.0f, 0.0f))
        throw std::runtime_error("legacy sampler could not render "
            + exampleId(example));
    const auto* selected = sampler.activeZone(0);
    if (selected == nullptr || selected->rootMidi != example.midi
        || selected->roundRobin != example.roundRobin)
        throw std::runtime_error("legacy sampler selected the wrong zone for "
            + exampleId(example));

    const auto frames = durationFrames(modelSampleRate);
    std::vector<float> output(frames * 2);
    std::array<float, renderBlockSize> left {};
    std::array<float, renderBlockSize> right {};
    for (std::size_t offset = 0; offset < frames; offset += renderBlockSize)
    {
        const auto count = std::min<std::size_t>(renderBlockSize,
                                                frames - offset);
        sampler.process(left.data(), right.data(), count);
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            output[2 * (offset + frame)] = left[frame];
            output[2 * (offset + frame) + 1] = right[frame];
        }
    }
    return output;
}

std::string formatTrim(float value)
{
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setprecision(9) << value;
    return text.str();
}

void writeManifest(const std::filesystem::path& path,
                   const std::vector<ManifestRow>& rows,
                   bool sampleBaseline = false)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("could not create " + path.string());

    output
        << "{\n"
        << "  \"analysis_sample_rate\": 48000,\n"
        << "  \"calibration_order\": [\"bodyFrequencyScale\", \"bodyQScale\", "
           "\"bridgeMobilityScale\", \"residueTiltDbPerOctave\", \"directGain\", "
           "\"nylon.fundamentalT60Scale\", "
           "\"nylon.frequencyLossScale\", \"nylon.apertureScale\", "
           "\"nylon.transientScale\", \"nylon.pluckDistanceScale\", "
           "\"nylon.velocityBrightnessDepth\", \"steel.stiffnessScale\", "
           "\"steel.fundamentalT60Scale\", \"steel.frequencyLossScale\", "
           "\"steel.apertureScale\", \"steel.transientScale\", "
           "\"steel.pluckDistanceScale\", \"steel.velocityBrightnessDepth\", "
           "\"apertureRegisterExponent\", \"lowBodyModeGain\", "
           "\"steelDisplacementScaleMetres\", \"steelFretT60Slope\", "
           "\"highLossCutoffScale\", \"bridgeConductanceFloor\", "
           "\"bridgeConductanceCornerHz\", \"bridgeTailLengthMetres\", "
           "\"longitudinalGain\", \"longitudinalQ\", "
           "\"polarisationEndCorrectionMetres\", "
           "\"steelSaddleBreakSine\", \"nylonSaddleBreakSine\"],\n"
        << "  \"provenance\": {\n"
        << "    \"target_timing\": \"source frame 0; recorded pre-roll/onset retained; cropped or zero-padded to 4.2 seconds\",\n"
        << "    \"target_gain\": \"dense::Sampler calibrated playback gain: layer/peak normalisation times (velocity/127)^0.82\",\n"
        << "    \"target_processing\": \"calibrated gain only; no age, tone, or pan processing\",\n"
        << "    \"calibration_source\": \"29 positional CLI values in calibration_order\",\n"
        << "    \"model_render\": \""
        << (sampleBaseline
            ? "frozen version-1 dense::Sampler; exact captured MIDI, velocity and round robin; 48000 Hz; 127-sample blocks"
            : "fresh AcustraEngine per material/MIDI/velocity; 48000 Hz; 127-sample blocks; default controls; outputGain excluded from calibration")
        << "\"\n"
        << "  },\n"
        << "  \"examples\": [\n";

    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const auto& row = rows[index];
        output
            << "    {\n"
            << "      \"id\": \"" << row.id << "\",\n"
            << "      \"material\": \"" << materialName(row.example.material)
            << "\",\n"
            << "      \"midi\": " << row.example.midi << ",\n"
            << "      \"velocity\": " << row.example.velocity << ",\n"
            << "      \"round_robin\": " << row.example.roundRobin << ",\n"
            << "      \"dynamic_group\": \"" << row.dynamicGroup << "\",\n"
            << "      \"target\": {\"path\": \"" << row.target.path
            << "\", \"sample_rate\": " << row.target.sampleRate
            << ", \"channels\": " << static_cast<int>(row.target.channels)
            << ", \"playback_trim\": " << formatTrim(row.playbackTrim)
            << "},\n"
            << "      \"model\": {\"path\": \""
            << (sampleBaseline ? row.sampleModel.path : row.model.path)
            << "\", \"sample_rate\": "
            << (sampleBaseline ? row.sampleModel.sampleRate : row.model.sampleRate)
            << ", \"channels\": " << static_cast<int>(
                sampleBaseline ? row.sampleModel.channels : row.model.channels)
            << "}\n"
            << "    }" << (index + 1 == rows.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    output.close();
    if (output.fail())
        throw std::runtime_error("could not write " + path.string());
}

std::vector<ManifestRow> renderExamples(
    const std::vector<Example>& examples, const Library& library,
    const PhysicalCalibration& calibration,
    const std::filesystem::path& directory,
    std::map<ModelKey, AudioFile>& models,
    std::map<SampleModelKey, AudioFile>& sampleModels,
    std::vector<std::filesystem::path>& writtenFiles,
    bool& determinismChecked)
{
    std::vector<ManifestRow> rows;
    rows.reserve(examples.size());
    for (const auto& example : examples)
    {
        const auto& zone = findExactZone(library, example);
        const std::string id = exampleId(example);
        const std::string targetName = targetFileName(example);
        const auto target = exportTarget(zone, example);
        if (!finite(target))
            throw std::runtime_error(id + " produced non-finite target audio");
        writeF32(directory / targetName, target);
        writtenFiles.push_back(directory / targetName);

        const ModelKey key { example.material, example.midi, example.velocity };
        auto model = models.find(key);
        if (model == models.end())
        {
            const std::string modelName = modelFileName(example);
            const auto audio = renderModel(example.material, example.midi,
                                           example.velocity, calibration);
            if (!finite(audio))
                throw std::runtime_error(id + " produced non-finite model audio");
            if (!determinismChecked)
            {
                const auto repeated = renderModel(
                    example.material, example.midi, example.velocity,
                    calibration);
                if (audio != repeated)
                    throw std::runtime_error("model render is not deterministic");
                determinismChecked = true;
            }
            writeF32(directory / modelName, audio);
            writtenFiles.push_back(directory / modelName);
            model = models.emplace(key, AudioFile {
                modelName, modelSampleRate, 2 }).first;
        }

        const SampleModelKey sampleKey { example.material, example.midi,
                                         example.velocity, example.roundRobin };
        auto sampleModel = sampleModels.find(sampleKey);
        if (sampleModel == sampleModels.end())
        {
            const std::string sampleName = "sample-v1-" + id + ".f32";
            const auto audio = renderSampleModel(library, example);
            if (!finite(audio))
                throw std::runtime_error(id
                    + " produced non-finite legacy sample audio");
            writeF32(directory / sampleName, audio);
            writtenFiles.push_back(directory / sampleName);
            sampleModel = sampleModels.emplace(sampleKey, AudioFile {
                sampleName, modelSampleRate, 2 }).first;
        }

        rows.push_back({ example, id, dynamicGroup(example),
            { targetName, zone.sampleRate, zone.channels }, model->second,
            sampleModel->second, zone.playbackTrim });
    }
    return rows;
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not read " + path.string());
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    if (input.bad())
        throw std::runtime_error("could not read " + path.string());
    return text;
}

bool isOwnedRegularFile(const std::filesystem::path& path)
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    return !error && std::filesystem::is_regular_file(status);
}

void validateModelsOnlySplit(const std::filesystem::path& directory,
                             const char* manifestName,
                             const std::vector<Example>& examples)
{
    const auto manifestPath = directory / manifestName;
    if (!isOwnedRegularFile(manifestPath))
        throw std::runtime_error("missing regular manifest: "
            + manifestPath.string());
    const auto manifest = readTextFile(manifestPath);
    for (const auto& example : examples)
    {
        for (const auto& name : {
                 targetFileName(example), modelFileName(example) })
        {
            const std::string reference = "\"path\": \"" + name + "\"";
            if (manifest.find(reference) == std::string::npos)
                throw std::runtime_error(std::string(manifestName)
                    + " does not reference " + name);
        }

        const auto targetPath = directory / targetFileName(example);
        if (!isOwnedRegularFile(targetPath))
            throw std::runtime_error("missing regular target: "
                + targetPath.string());
        const auto modelPath = directory / modelFileName(example);
        std::error_code error;
        if (std::filesystem::exists(modelPath, error)
            && !isOwnedRegularFile(modelPath))
            throw std::runtime_error("model path is not a regular file: "
                + modelPath.string());
        if (error)
            throw std::runtime_error("could not inspect model path: "
                + modelPath.string());
    }
}

std::size_t renderReferencedModels(
    const std::filesystem::path& directory, const Schedule& schedule,
    const PhysicalCalibration& calibration)
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(directory, error);
    if (error || !std::filesystem::is_directory(status))
        throw std::runtime_error("models-only output is not an owned directory: "
            + directory.string());
    validateModelsOnlySplit(directory, "train.json", schedule.train);
    validateModelsOnlySplit(directory, "validation.json", schedule.validation);
    if (!schedule.flatTop.empty())
        validateModelsOnlySplit(directory, "flattop.json", schedule.flatTop);

    std::map<ModelKey, Example> unique;
    for (const auto* examples : { &schedule.train, &schedule.validation,
                                  &schedule.flatTop })
        for (const auto& example : *examples)
            unique.emplace(ModelKey {
                example.material, example.midi, example.velocity }, example);

    for (const auto& [key, example] : unique)
    {
        static_cast<void>(key);
        const auto audio = renderModel(example.material, example.midi,
                                       example.velocity, calibration);
        if (!finite(audio))
            throw std::runtime_error(exampleId(example)
                + " produced non-finite model audio");
        writeF32(directory / modelFileName(example), audio);
    }
    return unique.size();
}

std::uint64_t fileFingerprint(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not fingerprint " + path.string());
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    std::array<char, 8192> buffer {};
    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= 0x100000001b3ULL;
        }
    }
    if (!input.eof())
        throw std::runtime_error("could not fingerprint " + path.string());
    return hash;
}

std::map<std::string, std::uint64_t> protectedFingerprints(
    const std::filesystem::path& directory,
    const std::vector<ManifestRow>& train,
    const std::vector<ManifestRow>& validation)
{
    std::map<std::string, std::uint64_t> result;
    for (const auto* rows : { &train, &validation })
        for (const auto& row : *rows)
            result.emplace(row.target.path,
                           fileFingerprint(directory / row.target.path));
    for (const char* name : { "train.json", "validation.json" })
        result.emplace(name, fileFingerprint(directory / name));
    return result;
}

void verifyFileSize(const std::filesystem::path& directory,
                    const AudioFile& file)
{
    std::error_code error;
    const auto actual = std::filesystem::file_size(directory / file.path, error);
    const auto expected = durationFrames(file.sampleRate)
        * static_cast<std::size_t>(file.channels) * sizeof(float);
    if (error || actual != expected)
        throw std::runtime_error(file.path + " has the wrong byte count");
}

void verifySmoke(const std::filesystem::path& directory,
                 const std::vector<ManifestRow>& train,
                 const std::vector<ManifestRow>& validation)
{
    if (train.size() != 3 || validation.size() != 3)
        throw std::runtime_error("smoke schedule has the wrong size");
    for (const auto* rows : { &train, &validation })
        for (const auto& row : *rows)
        {
            verifyFileSize(directory, row.target);
            verifyFileSize(directory, row.model);
        }

    for (const char* name : { "train.json", "validation.json" })
    {
        const auto text = readTextFile(directory / name);
        if (text.find("\"examples\"") == std::string::npos
            || text.find("\"sample_rate\"") == std::string::npos
            || text.find("\"channels\"") == std::string::npos
            || text.find("\"apertureRegisterExponent\"")
                == std::string::npos
            || text.find("\"lowBodyModeGain\"")
                == std::string::npos
            || text.find("\"steelDisplacementScaleMetres\"")
                == std::string::npos
            || text.find("\"steelFretT60Slope\"")
                == std::string::npos
            || text.find("\"highLossCutoffScale\"")
                == std::string::npos
            || text.find("\"bridgeConductanceFloor\"")
                == std::string::npos)
            throw std::runtime_error(std::string(name) + " is malformed");
    }
}

void prepareOutputDirectory(const std::filesystem::path& directory)
{
    if (directory.empty() || directory.filename().empty())
        throw std::runtime_error("output directory path is empty or incomplete");
    const auto parent = directory.has_parent_path()
        ? directory.parent_path() : std::filesystem::path(".");
    std::error_code error;
    if (!std::filesystem::is_directory(parent, error) || error)
        throw std::runtime_error("output directory parent does not exist: "
            + parent.string());
    if (std::filesystem::exists(directory, error) || error)
        throw std::runtime_error("output path already exists: "
            + directory.string());
    if (!std::filesystem::create_directory(directory, error) || error)
        throw std::runtime_error("could not create output directory: "
            + directory.string());
}

std::filesystem::path normaliseDirectory(const char* text)
{
    std::filesystem::path directory(text == nullptr ? "" : text);
    directory = directory.lexically_normal();
    if (!directory.empty() && directory.filename().empty()
        && directory.has_parent_path())
        directory = directory.parent_path();
    return directory;
}

void removeSmokeOutput(const std::filesystem::path& directory,
                       const std::vector<std::filesystem::path>& files)
{
    std::error_code error;
    for (auto file = files.rbegin(); file != files.rend(); ++file)
    {
        if (!std::filesystem::remove(*file, error) || error)
            throw std::runtime_error("could not remove smoke output: "
                + file->string());
    }
    if (!std::filesystem::remove(directory, error) || error)
        throw std::runtime_error("could not remove smoke output directory: "
            + directory.string());
}

void renderCorpus(const std::filesystem::path& directory,
                  const CalibrationValues& values, bool smoke)
{
    prepareOutputDirectory(directory);
    Library library;
    std::string decodeError;
    if (!library.prepare(&decodeError))
        throw std::runtime_error("could not decode reference bank: " + decodeError);

    const Schedule schedule = makeSchedule(smoke);
    const auto calibration = makeCalibration(values);
    std::map<ModelKey, AudioFile> models;
    std::map<SampleModelKey, AudioFile> sampleModels;
    std::vector<std::filesystem::path> writtenFiles;
    bool determinismChecked = !smoke;
    const auto train = renderExamples(
        schedule.train, library, calibration, directory, models, sampleModels,
        writtenFiles, determinismChecked);
    const auto validation = renderExamples(
        schedule.validation, library, calibration, directory, models, sampleModels,
        writtenFiles, determinismChecked);
    const auto flatTop = renderExamples(
        schedule.flatTop, library, calibration, directory, models, sampleModels,
        writtenFiles, determinismChecked);

    writeManifest(directory / "train.json", train);
    writtenFiles.push_back(directory / "train.json");
    writeManifest(directory / "validation.json", validation);
    writtenFiles.push_back(directory / "validation.json");
    if (!flatTop.empty())
    {
        writeManifest(directory / "flattop.json", flatTop);
        writtenFiles.push_back(directory / "flattop.json");
    }
    writeManifest(directory / "train-sample-v1.json", train, true);
    writtenFiles.push_back(directory / "train-sample-v1.json");
    writeManifest(directory / "validation-sample-v1.json", validation, true);
    writtenFiles.push_back(directory / "validation-sample-v1.json");
    if (!flatTop.empty())
    {
        writeManifest(directory / "flattop-sample-v1.json", flatTop, true);
        writtenFiles.push_back(directory / "flattop-sample-v1.json");
    }

    if (smoke)
    {
        verifySmoke(directory, train, validation);
        const auto before = protectedFingerprints(directory, train, validation);
        const auto modelPath = directory / train.front().model.path;
        std::ofstream corrupted(modelPath, std::ios::binary | std::ios::trunc);
        corrupted.close();
        if (corrupted.fail())
            throw std::runtime_error("could not prepare models-only smoke");
        const auto modelCount = renderReferencedModels(
            directory, schedule, calibration);
        if (modelCount != 6
            || before != protectedFingerprints(directory, train, validation))
            throw std::runtime_error(
                "models-only smoke changed a target/manifest or missed a model");
        verifySmoke(directory, train, validation);
        removeSmokeOutput(directory, writtenFiles);
        std::printf("Acustra physical-fit renderer smoke test passed.\n");
        return;
    }

    std::printf("Wrote %zu training, %zu validation and %zu flat-top "
                "examples (%zu unique model renders) to %s\n",
                train.size(), validation.size(), flatTop.size(), models.size(),
                directory.string().c_str());
}

void renderTestCorpus(const std::filesystem::path& directory,
                      const CalibrationValues& values)
{
    prepareOutputDirectory(directory);
    Library library;
    std::string decodeError;
    if (!library.prepare(&decodeError))
        throw std::runtime_error("could not decode reference bank: " + decodeError);

    const auto calibration = makeCalibration(values);
    std::map<ModelKey, AudioFile> models;
    std::map<SampleModelKey, AudioFile> sampleModels;
    std::vector<std::filesystem::path> writtenFiles;
    bool determinismChecked = false;
    const auto rows = renderExamples(
        makeTestSchedule(), library, calibration, directory, models,
        sampleModels, writtenFiles, determinismChecked);
    writeManifest(directory / "test.json", rows);
    writeManifest(directory / "test-sample-v1.json", rows, true);
    std::printf("Wrote %zu frozen test examples (%zu unique model renders) "
                "to %s\n",
                rows.size(), models.size(), directory.string().c_str());
}

void renderModelsOnlyCorpus(const std::filesystem::path& directory,
                            const CalibrationValues& values)
{
    const auto count = renderReferencedModels(
        directory, makeSchedule(false), makeCalibration(values));
    std::printf("Wrote %zu unique model renders to %s; targets and manifests "
                "were left unchanged.\n",
                count, directory.string().c_str());
}

bool parseFloat(const char* text, float& value)
{
    if (text == nullptr || *text == '\0')
        return false;
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::noskipws >> value;
    return input && input.peek() == std::char_traits<char>::eof()
        && std::isfinite(value);
}

void printUsage()
{
    std::printf(
        "usage: AcustraPhysicalFitRenderer [--smoke|--models-only|--test] OUTPUT "
        "BODY_FREQUENCY BODY_Q BRIDGE_MOBILITY RESIDUE_TILT DIRECT_GAIN "
        "NYLON_T60 NYLON_FREQUENCY_LOSS NYLON_APERTURE "
        "NYLON_TRANSIENT NYLON_PLUCK_DISTANCE NYLON_VELOCITY_BRIGHTNESS "
        "STEEL_STIFFNESS STEEL_T60 STEEL_FREQUENCY_LOSS STEEL_APERTURE "
        "STEEL_TRANSIENT STEEL_PLUCK_DISTANCE STEEL_VELOCITY_BRIGHTNESS "
        "APERTURE_REGISTER_EXPONENT LOW_BODY_MODE_GAIN "
        "STEEL_DISPLACEMENT_METRES STEEL_FRET_T60_SLOPE "
        "HIGH_LOSS_CUTOFF_SCALE BRIDGE_CONDUCTANCE_FLOOR "
        "BRIDGE_CONDUCTANCE_CORNER_HZ BRIDGE_TAIL_LENGTH_METRES "
        "LONGITUDINAL_GAIN LONGITUDINAL_Q "
        "POLARISATION_END_CORRECTION_METRES "
        "STEEL_SADDLE_BREAK_SINE NYLON_SADDLE_BREAK_SINE\n");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2
        && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        printUsage();
        return 0;
    }

    int first = 1;
    bool smoke = false;
    bool modelsOnly = false;
    bool test = false;
    if (argc > 1 && std::string(argv[1]) == "--smoke")
    {
        smoke = true;
        ++first;
    }
    else if (argc > 1 && std::string(argv[1]) == "--models-only")
    {
        modelsOnly = true;
        ++first;
    }
    else if (argc > 1 && std::string(argv[1]) == "--test")
    {
        test = true;
        ++first;
    }
    if (argc - first != static_cast<int>(calibrationValueCount + 1))
    {
        printUsage();
        return 2;
    }

    CalibrationValues values {};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (!parseFloat(argv[first + 1 + static_cast<int>(index)], values[index]))
        {
            std::fprintf(stderr, "invalid calibration value %zu: %s\n",
                         index + 1,
                         argv[first + 1 + static_cast<int>(index)]);
            return 2;
        }
        if (values[index] < calibrationMinimums[index]
            || values[index] > calibrationMaximums[index])
        {
            std::fprintf(stderr,
                         "calibration value %zu is outside [%g, %g]: %g\n",
                         index + 1,
                         static_cast<double>(calibrationMinimums[index]),
                         static_cast<double>(calibrationMaximums[index]),
                         static_cast<double>(values[index]));
            return 2;
        }
    }

    try
    {
        const auto directory = normaliseDirectory(argv[first]);
        if (modelsOnly)
            renderModelsOnlyCorpus(directory, values);
        else if (test)
            renderTestCorpus(directory, values);
        else
            renderCorpus(directory, values, smoke);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "AcustraPhysicalFitRenderer: %s\n", error.what());
        return 1;
    }
}

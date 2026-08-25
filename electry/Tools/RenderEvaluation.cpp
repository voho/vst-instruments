// Renders small, controlled dry-DI probes for comparison with real eight-string
// recordings. This tool is intentionally separate from RenderDemos: it never
// normalises audio, never touches Docs/audio, and writes only into its requested
// evaluation directory.
#include "DSP/ElectryEngine.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#ifndef ELECTRY_EVALUATION_OUTPUT_DIR
#define ELECTRY_EVALUATION_OUTPUT_DIR "evaluation"
#endif

#ifndef ELECTRY_EVALUATION_PROJECT_VERSION
#define ELECTRY_EVALUATION_PROJECT_VERSION "unknown"
#endif

namespace
{
using electry::ElectryEngine;
using electry::EngineParameters;
using electry::OutputMode;
using electry::PickStyle;
using electry::PickupSelector;
using electry::PlayStyle;

// Match the public CC0 reference's native rate so spectral comparison does not
// require resampling either side.
constexpr int sampleRate = 44100;
constexpr int blockSize = 256;
constexpr int e1MidiNote = ElectryEngine::lowestPlayableNote;
constexpr int e2MidiNote = 40;
constexpr float noteVelocity = 0.95f;
constexpr int leadInFrames = sampleRate / 4;
constexpr int heldFrames = sampleRate * 2;
constexpr int releaseFrames = sampleRate;

const char* buildMode()
{
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

struct Probe
{
    const char* id;
    const char* fileName;
    const char* playStyleName;
    PlayStyle playStyle;
    int midiNote;
    int stringNumber;
    double equalTemperamentHz;
    float muteDamping;
    std::vector<float> samples;
    float peak = 0.0f;
    bool finite = true;
    bool exactDualMono = true;
};

int keyswitchFor(PickStyle pickStyle)
{
    return ElectryEngine::firstKeyswitchNote + static_cast<int>(pickStyle);
}

int keyswitchFor(PlayStyle playStyle)
{
    return ElectryEngine::firstPlayStyleKeyswitchNote
         + static_cast<int>(playStyle);
}

EngineParameters evaluationParameters()
{
    EngineParameters parameters;
    parameters.outputMode = OutputMode::Mono;
    parameters.outputGain = 1.0f;
    // An isolated single-string DI makes comparison and alignment simpler.
    // The audible pick, finger, release and construction behaviours remain on.
    parameters.sympatheticAmount = 0.0f;
    parameters.palmMute = 0.0f;
    parameters.strumSpreadSeconds = 0.0f;
    return parameters;
}

void renderFrames(ElectryEngine& engine, Probe& probe, int frames)
{
    std::vector<float> left(static_cast<std::size_t>(blockSize));
    std::vector<float> right(static_cast<std::size_t>(blockSize));
    int remaining = frames;

    while (remaining > 0)
    {
        const int currentBlock = std::min(blockSize, remaining);
        engine.process(left.data(), right.data(), currentBlock);
        for (int frame = 0; frame < currentBlock; ++frame)
        {
            const float sample = left[static_cast<std::size_t>(frame)];
            const float other = right[static_cast<std::size_t>(frame)];
            probe.finite = probe.finite && std::isfinite(sample)
                         && std::isfinite(other);
            probe.exactDualMono = probe.exactDualMono && sample == other;
            probe.peak = std::max(probe.peak, std::fabs(sample));
            probe.samples.push_back(sample);
        }
        remaining -= currentBlock;
    }
}

Probe renderProbe(const char* id, const char* fileName,
                  const char* playStyleName, PlayStyle playStyle,
                  int midiNote, int stringNumber,
                  double equalTemperamentHz,
                  const EngineParameters& parameters)
{
    Probe probe { id, fileName, playStyleName, playStyle, midiNote,
                  stringNumber, equalTemperamentHz, parameters.muteDamping,
                  {}, 0.0f, true, true };
    probe.samples.reserve(static_cast<std::size_t>(
        leadInFrames + heldFrames + releaseFrames));

    ElectryEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameters(parameters);
    engine.setPitchBend(0.0f);
    engine.setResonance(0.0f);
    engine.setPalmMutePressure(0.0f);
    engine.setVibrato(0.0f);
    engine.setSustainPedal(false);
    engine.setAcousticReturnLevel(0.0f);
    engine.reset();
    engine.noteOn(keyswitchFor(PickStyle::Down), 1.0f);
    engine.noteOn(keyswitchFor(playStyle), 1.0f);
    renderFrames(engine, probe, leadInFrames);
    engine.noteOn(midiNote, noteVelocity);
    renderFrames(engine, probe, heldFrames);
    engine.noteOff(midiNote);
    renderFrames(engine, probe, releaseFrames);
    return probe;
}

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value,
                        int byteCount)
{
    for (int byte = 0; byte < byteCount; ++byte)
        bytes.push_back(static_cast<std::uint8_t>(
            (value >> (8 * byte)) & 0xffu));
}

bool writeFloatWav(const std::filesystem::path& path,
                   const std::vector<float>& samples)
{
    constexpr std::uint16_t channelCount = 1;
    constexpr std::uint16_t bytesPerSample = 4;
    const auto dataBytes = static_cast<std::uint32_t>(
        samples.size() * bytesPerSample);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44u + dataBytes);
    const auto appendTag = [&bytes] (const char* tag)
    {
        for (int byte = 0; byte < 4; ++byte)
            bytes.push_back(static_cast<std::uint8_t>(tag[byte]));
    };

    appendTag("RIFF");
    appendLittleEndian(bytes, 36u + dataBytes, 4);
    appendTag("WAVE");
    appendTag("fmt ");
    appendLittleEndian(bytes, 16u, 4);
    appendLittleEndian(bytes, 3u, 2); // WAVE_FORMAT_IEEE_FLOAT
    appendLittleEndian(bytes, channelCount, 2);
    appendLittleEndian(bytes, sampleRate, 4);
    appendLittleEndian(bytes, sampleRate * channelCount * bytesPerSample, 4);
    appendLittleEndian(bytes, channelCount * bytesPerSample, 2);
    appendLittleEndian(bytes, 32u, 2);
    appendTag("data");
    appendLittleEndian(bytes, dataBytes, 4);
    for (const float sample : samples)
        appendLittleEndian(bytes, std::bit_cast<std::uint32_t>(sample), 4);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.close();
    return output.good();
}

const char* pickupSelectorName(PickupSelector selector)
{
    switch (selector)
    {
        case PickupSelector::Neck: return "neck";
        case PickupSelector::Both: return "both";
        case PickupSelector::Bridge: return "bridge";
    }
    return "unknown";
}

bool writeManifest(const std::filesystem::path& path,
                   const EngineParameters& p,
                   const std::vector<Probe>& probes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << std::fixed << std::setprecision(8);
    output
        << "{\n"
        << "  \"schema\": \"electry-evaluation/v3\",\n"
        << "  \"generator\": {\"name\": \"ElectryRenderEvaluation\", "
           "\"project_version\": \"" ELECTRY_EVALUATION_PROJECT_VERSION
           "\", \"build_mode\": \"" << buildMode()
        << "\", \"determinism_scope\": \"same executable and CPU architecture\"},\n"
        << "  \"audio_format\": {\"container\": \"WAVE\", "
           "\"encoding\": \"IEEE_FLOAT\", \"bits_per_sample\": 32, "
           "\"channels\": 1, \"sample_rate_hz\": " << sampleRate
        << ", \"normalization_applied\": false, "
           "\"post_render_gain\": 1.00000000},\n"
        << "  \"signal_chain\": {\"path\": \"ElectryEngine dry DI; ElectryFx not instantiated\", "
           "\"amplitude_reference\": \"arbitrary digital model full scale; not volts and not level matched\"},\n"
        << "  \"protocol\": {\"instrument\": \"eight-string guitar\", "
           "\"tuning_low_to_high\": [\"E1\", \"B1\", \"E2\", \"A2\", "
           "\"D3\", \"G3\", \"B3\", \"E4\"], "
           "\"targets\": [{\"id\": \"e1\", \"string\": 8, "
           "\"fret\": 0, \"midi_note\": 28, "
           "\"equal_temperament_frequency_hz\": 41.20344461}, "
           "{\"id\": \"e2\", \"string\": 6, \"fret\": 0, "
           "\"midi_note\": 40, "
           "\"equal_temperament_frequency_hz\": 82.40688923}], "
           "\"velocity\": " << noteVelocity
        << ", \"lead_in_frames\": " << leadInFrames
        << ", \"held_frames\": " << heldFrames
        << ", \"release_frames\": " << releaseFrames
        << ", \"block_size\": " << blockSize << "},\n"
        << "  \"engine_parameters\": {\n"
        << "    \"pickup_selector\": \"" << pickupSelectorName(p.pickupSelector)
        << "\",\n"
        << "    \"body_wood\": " << p.bodyWood
        << ", \"body_size\": " << p.bodySize
        << ", \"body_shape\": " << p.bodyShape
        << ", \"construction\": " << p.construction << ",\n"
        << "    \"scale_length\": " << p.scaleLength
        << ", \"pickup_type\": " << p.pickupType
        << ", \"tone_knob\": " << p.toneKnob
        << ", \"body_resonance\": " << p.bodyResonance << ",\n"
        << "    \"string_gauge\": " << p.stringGauge
        << ", \"string_age\": " << p.stringAge
        << ", \"pick_position\": " << p.pickPosition
        << ", \"pick_hardness\": " << p.pickHardness << ",\n"
        << "    \"pick_noise\": " << p.pickNoise
        << ", \"finger_noise\": " << p.fingerNoise
        << ", \"release_noise\": " << p.releaseNoise
        << ", \"mute_damping\": " << p.muteDamping << ",\n"
        << "    \"bend_time_seconds\": " << p.bendTimeSeconds
        << ", \"velocity_amount\": " << p.velocityAmount
        << ", \"output_gain\": " << p.outputGain
        << ", \"artifact_amount\": " << p.artifactAmount << ",\n"
        << "    \"output_mode\": \"mono\", \"sympathetic_amount\": "
        << p.sympatheticAmount
        << ", \"palm_mute\": " << p.palmMute
        << ", \"strum_spread_seconds\": " << p.strumSpreadSeconds << ",\n"
        << "    \"resonance_depth\": " << p.resonanceDepth
        << ", \"vibrato_depth\": " << p.vibratoDepth << "\n"
        << "  },\n"
        << "  \"performance_controls\": {\"pitch_bend\": 0.00000000, "
           "\"mod_wheel_resonance\": 0.00000000, "
           "\"palm_mute_pressure\": 0.00000000, "
           "\"channel_pressure_vibrato\": 0.00000000, "
           "\"sustain_pedal\": false, \"acoustic_return_level\": 0.00000000},\n"
        << "  \"probes\": [\n";

    for (std::size_t index = 0; index < probes.size(); ++index)
    {
        const auto& probe = probes[index];
        output
            << "    {\n"
            << "      \"id\": \"" << probe.id << "\", \"file\": \""
            << probe.fileName << "\", \"play_style\": \""
            << probe.playStyleName << "\", \"target_string\": "
            << probe.stringNumber << ", \"target_fret\": 0, \"midi_note\": "
            << probe.midiNote << ", \"equal_temperament_frequency_hz\": "
            << probe.equalTemperamentHz << ", \"mute_damping\": "
            << std::setprecision(2) << probe.muteDamping
            << std::setprecision(8) << ", \"frames\": "
            << probe.samples.size() << ", \"raw_peak\": " << probe.peak
            << ",\n"
            << "      \"events\": [\n"
            << "        {\"type\": \"keyswitch\", \"bank\": \"pick_style\", "
               "\"value\": \"down\", \"midi_note\": "
            << keyswitchFor(PickStyle::Down)
            << ", \"sample\": 0, \"time_seconds\": 0.00000000},\n"
            << "        {\"type\": \"keyswitch\", \"bank\": \"play_style\", "
               "\"value\": \"" << probe.playStyleName
            << "\", \"midi_note\": " << keyswitchFor(probe.playStyle)
            << ", \"sample\": 0, \"time_seconds\": 0.00000000},\n"
            << "        {\"type\": \"note_on\", \"midi_note\": "
            << probe.midiNote << ", \"velocity\": " << noteVelocity
            << ", \"sample\": " << leadInFrames
            << ", \"time_seconds\": "
            << static_cast<double>(leadInFrames) / sampleRate << "},\n"
            << "        {\"type\": \"note_off\", \"midi_note\": "
            << probe.midiNote << ", \"sample\": "
            << leadInFrames + heldFrames << ", \"time_seconds\": "
            << static_cast<double>(leadInFrames + heldFrames) / sampleRate
            << "}\n"
            << "      ]\n"
            << "    }" << (index + 1u == probes.size() ? "\n" : ",\n");
    }

    output << "  ]\n}\n";
    output.close();
    return output.good();
}
} // namespace

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && std::string(argv[1]) == "--help"))
    {
        std::printf("Usage: ElectryRenderEvaluation [output-directory]\n"
                    "Default: %s\n", ELECTRY_EVALUATION_OUTPUT_DIR);
        return argc > 2 ? 1 : 0;
    }

    const std::filesystem::path outputDirectory = argc == 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::path(ELECTRY_EVALUATION_OUTPUT_DIR);
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    if (error)
    {
        std::fprintf(stderr, "Could not create %s: %s\n",
                     outputDirectory.string().c_str(), error.message().c_str());
        return 1;
    }

    // Remove the obsolete single-depth filenames when reusing an evaluation
    // directory.
    for (const char* legacyFile : { "e1-palm-mute.wav",
                                    "e2-palm-mute.wav" })
    {
        std::filesystem::remove(outputDirectory / legacyFile, error);
        if (error)
        {
            std::fprintf(stderr, "Could not remove obsolete %s: %s\n",
                         legacyFile, error.message().c_str());
            return 1;
        }
    }

    const auto parameters = evaluationParameters();
    auto lightMuteParameters = parameters;
    lightMuteParameters.muteDamping = 0.0f;
    auto mediumMuteParameters = parameters;
    mediumMuteParameters.muteDamping = 0.55f;
    auto hardMuteParameters = parameters;
    hardMuteParameters.muteDamping = 1.0f;

    std::vector<Probe> probes;
    probes.push_back(renderProbe("e1-open", "e1-open.wav", "sustain",
                                 PlayStyle::Sustain, e1MidiNote, 8,
                                 41.20344461, parameters));
    probes.push_back(renderProbe("e1-palm-mute-light",
                                 "e1-palm-mute-light.wav", "palm_mute",
                                 PlayStyle::PalmMute, e1MidiNote, 8,
                                 41.20344461, lightMuteParameters));
    probes.push_back(renderProbe("e1-palm-mute-medium",
                                 "e1-palm-mute-medium.wav", "palm_mute",
                                 PlayStyle::PalmMute, e1MidiNote, 8,
                                 41.20344461, mediumMuteParameters));
    probes.push_back(renderProbe("e1-palm-mute-hard",
                                 "e1-palm-mute-hard.wav", "palm_mute",
                                 PlayStyle::PalmMute, e1MidiNote, 8,
                                 41.20344461, hardMuteParameters));
    probes.push_back(renderProbe("e1-dead", "e1-dead.wav", "dead",
                                 PlayStyle::Dead, e1MidiNote, 8,
                                 41.20344461, parameters));
    probes.push_back(renderProbe("e2-open", "e2-open.wav", "sustain",
                                 PlayStyle::Sustain, e2MidiNote, 6,
                                 82.40688923, parameters));
    probes.push_back(renderProbe("e2-palm-mute-light",
                                 "e2-palm-mute-light.wav", "palm_mute",
                                 PlayStyle::PalmMute, e2MidiNote, 6,
                                 82.40688923, lightMuteParameters));
    probes.push_back(renderProbe("e2-palm-mute-medium",
                                 "e2-palm-mute-medium.wav", "palm_mute",
                                 PlayStyle::PalmMute, e2MidiNote, 6,
                                 82.40688923, mediumMuteParameters));
    probes.push_back(renderProbe("e2-palm-mute-hard",
                                 "e2-palm-mute-hard.wav", "palm_mute",
                                 PlayStyle::PalmMute, e2MidiNote, 6,
                                 82.40688923, hardMuteParameters));
    probes.push_back(renderProbe("e2-dead", "e2-dead.wav", "dead",
                                 PlayStyle::Dead, e2MidiNote, 6,
                                 82.40688923, parameters));

    for (const auto& probe : probes)
    {
        if (! probe.finite || ! probe.exactDualMono || probe.peak < 1.0e-4f)
        {
            std::fprintf(stderr,
                         "%s failed render validation (finite=%s, mono=%s, "
                         "peak=%.8f)\n",
                         probe.id, probe.finite ? "true" : "false",
                         probe.exactDualMono ? "true" : "false", probe.peak);
            return 1;
        }
        if (! writeFloatWav(outputDirectory / probe.fileName, probe.samples))
        {
            std::fprintf(stderr, "Could not write %s\n", probe.fileName);
            return 1;
        }
    }

    if (! writeManifest(outputDirectory / "manifest.json", parameters, probes))
    {
        std::fprintf(stderr, "Could not write manifest.json\n");
        return 1;
    }

    std::printf("Rendered ten raw E1/E2 dry-DI probes to %s "
                "(WAVE_FORMAT_IEEE_FLOAT, 32-bit, mono, unnormalized).\n",
                outputDirectory.string().c_str());
    return 0;
}

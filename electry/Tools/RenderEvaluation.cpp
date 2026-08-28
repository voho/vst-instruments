// Renders small, controlled dry-DI probes for comparison with real eight-string
// recordings. This tool is intentionally separate from RenderDemos: it never
// normalises audio, never touches Docs/audio, and writes only into its requested
// evaluation directory.
#include "DSP/ElectryEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
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
    applyGuitarBuild(parameters, electry::defaultGuitarBuild);
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

double energyInRange(const Probe& probe, int start, int length)
{
    const int first = std::clamp(start, 0,
                                 static_cast<int>(probe.samples.size()));
    const int last = std::clamp(first + length, first,
                                static_cast<int>(probe.samples.size()));
    double energy = 0.0;
    for (int frame = first; frame < last; ++frame)
    {
        const double sample = probe.samples[static_cast<std::size_t>(frame)];
        energy += sample * sample;
    }
    return energy;
}

double dftMagnitude(const Probe& probe, int start, int length,
                    double frequency)
{
    const int first = std::clamp(start, 0,
                                 static_cast<int>(probe.samples.size()));
    const int count = std::clamp(length, 0,
                                 static_cast<int>(probe.samples.size()) - first);
    if (count < 2)
        return 0.0;

    constexpr double pi = 3.14159265358979323846;
    const double phaseStep = 2.0 * pi * frequency / sampleRate;
    const double stepReal = std::cos(phaseStep);
    const double stepImag = std::sin(phaseStep);
    const double windowStep = pi / static_cast<double>(count - 1);
    double phasorReal = 1.0;
    double phasorImag = 0.0;
    double sumReal = 0.0;
    double sumImag = 0.0;

    for (int frame = 0; frame < count; ++frame)
    {
        const double window = std::sin(windowStep * frame);
        const double sample = window * window * static_cast<double>(
            probe.samples[static_cast<std::size_t>(first + frame)]);
        sumReal += sample * phasorReal;
        sumImag += sample * phasorImag;
        const double nextReal = phasorReal * stepReal
                              - phasorImag * stepImag;
        phasorImag = phasorReal * stepImag + phasorImag * stepReal;
        phasorReal = nextReal;
    }
    return std::sqrt(sumReal * sumReal + sumImag * sumImag);
}

double strongestPitchOffsetCents(const Probe& probe)
{
    const int start = leadInFrames + heldFrames / 4;
    const int length = heldFrames / 4;
    double bestCents = -100.0;
    double bestScore = -1.0;

    // Search the target note and its two neighbours. The acceptance boundary
    // below is their equal-tempered midpoint, not a fit to today's render.
    for (int cents = -100; cents <= 100; cents += 2)
    {
        const double fundamental = probe.equalTemperamentHz
            * std::pow(2.0, static_cast<double>(cents) / 1200.0);
        double score = 0.0;
        for (int partial = 1; partial <= 5; ++partial)
            score += dftMagnitude(probe, start, length,
                                  fundamental * partial)
                   / std::sqrt(static_cast<double>(partial));
        if (score > bestScore)
        {
            bestScore = score;
            bestCents = static_cast<double>(cents);
        }
    }
    return bestCents;
}

std::string audioOracleFailure(const std::vector<Probe>& probes)
{
    if (probes.size() != 10u)
        return "audio oracle requires exactly ten probes";

    const int expectedFrames = leadInFrames + heldFrames + releaseFrames;
    for (const auto& probe : probes)
    {
        if (probe.samples.size() != static_cast<std::size_t>(expectedFrames))
            return std::string(probe.id) + " has the wrong sample count";
        if (energyInRange(probe, 0, leadInFrames) != 0.0)
            return std::string(probe.id) + " is not silent before note-on";
        const double soundingEnergy = energyInRange(
            probe, leadInFrames, heldFrames + releaseFrames);
        if (! std::isfinite(soundingEnergy) || soundingEnergy <= 0.0)
            return std::string(probe.id) + " rendered silence after note-on";
    }

    for (std::size_t first = 0; first < probes.size(); ++first)
        for (std::size_t second = first + 1; second < probes.size(); ++second)
            if (probes[first].samples == probes[second].samples)
                return std::string(probes[first].id) + " and "
                     + probes[second].id + " rendered identical audio";

    const int tailStart = leadInFrames + heldFrames / 2;
    const int tailLength = heldFrames / 2;
    for (const std::size_t first : { 0u, 5u })
    {
        const double pitchCents = strongestPitchOffsetCents(probes[first]);
        if (! std::isfinite(pitchCents) || std::abs(pitchCents) >= 50.0)
            return std::string(probes[first].id)
                 + " is closer to a neighbouring MIDI note than its target ("
                 + std::to_string(pitchCents) + " cents)";

        const double open = energyInRange(probes[first], tailStart, tailLength);
        const double light = energyInRange(probes[first + 1u], tailStart,
                                           tailLength);
        const double medium = energyInRange(probes[first + 2u], tailStart,
                                            tailLength);
        const double hard = energyInRange(probes[first + 3u], tailStart,
                                          tailLength);
        const double dead = energyInRange(probes[first + 4u], tailStart,
                                          tailLength);
        if (! (open > light && light > medium && medium > hard))
            return std::string(probes[first].id)
                 + " mute tightness did not reduce held-tail energy monotonically";
        if (! (open > dead))
            return std::string(probes[first].id)
                 + " Dead articulation did not decay below Sustain";
    }
    return {};
}

bool runAudioOracleSelfTests(std::vector<Probe>& probes)
{
    const auto expectFailure = [&] (const char* mutation,
                                    const char* expectedText)
    {
        const auto failure = audioOracleFailure(probes);
        if (failure.find(expectedText) != std::string::npos)
            return true;
        std::fprintf(stderr,
                     "Audio-oracle self-test '%s' was not rejected by the "
                     "expected gate (reported: %s)\n",
                     mutation, failure.empty() ? "no failure" : failure.c_str());
        return false;
    };

    bool passed = true;
    auto savedSamples = probes[0].samples;
    std::fill(probes[0].samples.begin(), probes[0].samples.end(), 0.0f);
    passed = expectFailure("silence", "rendered silence") && passed;
    probes[0].samples = std::move(savedSamples);

    savedSamples = probes[1].samples;
    probes[1].samples = probes[0].samples;
    passed = expectFailure("duplicate articulation", "identical audio") && passed;
    probes[1].samples = std::move(savedSamples);

    const double savedFrequency = probes[0].equalTemperamentHz;
    probes[0].equalTemperamentHz *= std::pow(2.0, 1.0 / 12.0);
    passed = expectFailure("wrong pitch", "neighbouring MIDI note") && passed;
    probes[0].equalTemperamentHz = savedFrequency;

    std::swap(probes[1].samples, probes[3].samples);
    passed = expectFailure("reversed mute depth", "mute tightness") && passed;
    std::swap(probes[1].samples, probes[3].samples);
    return passed;
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

bool readFloatWavPayload(const std::filesystem::path& path,
                         std::size_t frameCount,
                         std::vector<float>& samples)
{
    std::ifstream input(path, std::ios::binary);
    input.seekg(44, std::ios::beg);
    if (! input.good())
        return false;

    samples.clear();
    samples.reserve(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        std::array<std::uint8_t, 4> bytes {};
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size()))
            return false;
        const std::uint32_t bits = static_cast<std::uint32_t>(bytes[0])
                                 | (static_cast<std::uint32_t>(bytes[1]) << 8u)
                                 | (static_cast<std::uint32_t>(bytes[2]) << 16u)
                                 | (static_cast<std::uint32_t>(bytes[3]) << 24u);
        samples.push_back(std::bit_cast<float>(bits));
    }
    return input.peek() == std::char_traits<char>::eof();
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
        << "    \"guitar_build\": " << electry::defaultGuitarBuild << ",\n"
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
    const bool runSelfTests = argc >= 2
                           && std::string(argv[1]) == "--self-test";
    const int outputArgument = runSelfTests ? 2 : 1;
    if (argc > outputArgument + 1
        || (argc == 2 && std::string(argv[1]) == "--help"))
    {
        std::printf("Usage: ElectryRenderEvaluation [--self-test] "
                    "[output-directory]\n"
                    "Default: %s\n", ELECTRY_EVALUATION_OUTPUT_DIR);
        return argc > outputArgument + 1 ? 1 : 0;
    }

    const std::filesystem::path outputDirectory = argc == outputArgument + 1
        ? std::filesystem::path(argv[outputArgument])
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
    }

    const auto oracleFailure = audioOracleFailure(probes);
    if (! oracleFailure.empty())
    {
        std::fprintf(stderr, "Evaluation audio oracle failed: %s\n",
                     oracleFailure.c_str());
        return 1;
    }
    if (runSelfTests && ! runAudioOracleSelfTests(probes))
        return 1;

    for (const auto& probe : probes)
    {
        if (! writeFloatWav(outputDirectory / probe.fileName, probe.samples))
        {
            std::fprintf(stderr, "Could not write %s\n", probe.fileName);
            return 1;
        }
    }

    // Validate what was serialized, not merely the in-memory source. CMake
    // independently checks the RIFF structure; this pass decodes each payload,
    // requires a bit-exact float round trip, then reruns the semantic oracle.
    for (auto& probe : probes)
    {
        auto renderedSamples = std::move(probe.samples);
        if (! readFloatWavPayload(outputDirectory / probe.fileName,
                                  renderedSamples.size(), probe.samples))
        {
            std::fprintf(stderr, "Could not read back %s\n", probe.fileName);
            return 1;
        }
        if (probe.samples != renderedSamples)
        {
            std::fprintf(stderr, "%s payload differs from its render\n",
                         probe.fileName);
            return 1;
        }
    }
    const auto serializedOracleFailure = audioOracleFailure(probes);
    if (! serializedOracleFailure.empty())
    {
        std::fprintf(stderr, "Serialized evaluation audio oracle failed: %s\n",
                     serializedOracleFailure.c_str());
        return 1;
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

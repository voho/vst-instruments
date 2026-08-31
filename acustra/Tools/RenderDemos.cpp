// Renders Acustra's five committed demonstrations through AcustraEngine, the
// same JUCE-free signal path used by the plug-in. Every score and engine seed
// is deterministic. The engine's strings, passive bridge, and modal body are
// the source; no sample playback, convolution, room, or post-effect is used.
// Full renders retain the engine's relative dynamics inside each file, then
// apply one whole-file peak normalisation to -3 dBFS before PCM16 encoding.
// Raw peaks and the applied gains are written only inside the bounded table in
// the instrument README. Ad-hoc output directories are never documented or
// cleaned, and this tool never deletes an existing demonstration directory.

#include "DSP/AcustraEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
using acustra::AcustraEngine;
using acustra::BodyMaterial;
using acustra::BodyShape;
using acustra::EngineParameters;
using acustra::StringMaterial;
using acustra::Tuning;

constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
constexpr double normalisedPeak = 0.7079457843841379; // -3 dBFS

struct Audio
{
    std::vector<float> left;
    std::vector<float> right;
};

class Take
{
public:
    explicit Take(const EngineParameters& parameters)
    {
        // Match the plug-in's prepare sequence: the construction is available
        // when reset configures its strings/body, then published once more.
        engine_.setParameters(parameters);
        engine_.prepare(demoSampleRate, renderBlockSize);
        engine_.setParameters(parameters);
    }

    void noteOn(int note, float velocity) { engine_.noteOn(note, velocity); }
    void noteOff(int note) { engine_.noteOff(note); }

    void rest(double seconds)
    {
        int remaining = static_cast<int>(std::llround(seconds * demoSampleRate));
        std::array<float, renderBlockSize> left {};
        std::array<float, renderBlockSize> right {};
        while (remaining > 0)
        {
            const int count = std::min(renderBlockSize, remaining);
            engine_.process(left.data(), right.data(), count);
            audio_.left.insert(audio_.left.end(), left.begin(), left.begin() + count);
            audio_.right.insert(audio_.right.end(), right.begin(), right.begin() + count);
            remaining -= count;
        }
    }

    void play(int note, float velocity, double held, double tail)
    {
        noteOn(note, velocity);
        rest(held);
        noteOff(note);
        rest(tail);
    }

    void strum(std::initializer_list<int> notes, float velocity,
               double spacing)
    {
        int index = 0;
        for (const int note : notes)
        {
            noteOn(note, std::max(0.1f, velocity - 0.025f * index));
            rest(spacing);
            ++index;
        }
    }

    void release(std::initializer_list<int> notes)
    {
        for (const int note : notes)
            noteOff(note);
    }

    void bridgeHand(float pressure) { engine_.setPalmMutePressure(pressure); }

    Audio finish() { return std::move(audio_); }

private:
    AcustraEngine engine_;
    Audio audio_;
};

void append(Audio& destination, Audio source, double silenceSeconds = 0.0)
{
    destination.left.insert(destination.left.end(), source.left.begin(),
                            source.left.end());
    destination.right.insert(destination.right.end(), source.right.begin(),
                             source.right.end());
    const auto silence = static_cast<std::size_t>(
        std::llround(silenceSeconds * demoSampleRate));
    destination.left.insert(destination.left.end(), silence, 0.0f);
    destination.right.insert(destination.right.end(), silence, 0.0f);
}

EngineParameters baseParameters()
{
    EngineParameters parameters;
    // Render at the public default. Whole-file normalisation owns listening
    // level, so driving the safety limiter here would only alter transients.
    return parameters;
}

Audio steelSustainRange()
{
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Steel;
    parameters.shape = BodyShape::Dreadnought;
    parameters.bodyMaterial = BodyMaterial::Spruce;
    parameters.stringAge = 0.12f;
    parameters.pluckPosition = 0.25f;
    parameters.touch = 0.68f;

    Take take(parameters);
    take.play(40, 0.91f, 2.00, 0.50); // E2
    take.play(52, 0.87f, 1.35, 0.42); // E3
    take.play(64, 0.83f, 1.15, 0.38); // E4
    take.play(76, 0.79f, 1.05, 0.35); // E5
    take.play(83, 0.76f, 1.20, 1.10); // B5, nineteenth fret
    return take.finish();
}

Audio nylonFingerstyle()
{
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Nylon;
    parameters.shape = BodyShape::Auditorium;
    parameters.bodyMaterial = BodyMaterial::Cedar;
    parameters.stringAge = 0.08f;
    parameters.pluckPosition = 0.43f;
    parameters.touch = 0.08f;
    parameters.bodyAmount = 0.88f;

    Take take(parameters);
    constexpr std::array<std::array<int, 6>, 2> chords {{
        {{ 40, 47, 52, 55, 59, 64 }},
        {{ 43, 48, 52, 55, 60, 64 }}
    }};
    for (const auto& chord : chords)
    {
        for (std::size_t index = 0; index < chord.size(); ++index)
        {
            // The refitted nylon string is quieter than the previous
            // calibration; this keeps the demo inside the renderer's safe
            // pre-normalisation peak band without touching the engine.
            const float velocity = 0.66f
                + 0.05f * static_cast<float>(index % 3);
            take.noteOn(chord[index], velocity);
            take.rest(0.16);
        }
        take.rest(0.42);
        for (const int note : chord)
            take.noteOff(note);
        take.rest(0.70);
    }
    return take.finish();
}

Audio anchorChord(EngineParameters parameters)
{
    Take take(parameters);
    take.strum({ 40, 47, 52, 55, 59, 64 }, 0.84f, 0.028);
    take.rest(1.82);
    take.release({ 40, 47, 52, 55, 59, 64 });
    take.rest(1.10);
    return take.finish();
}

Audio shapeAndMaterialAnchors()
{
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Steel;
    parameters.stringAge = 0.15f;
    parameters.bodyAmount = 0.90f;
    parameters.stereoWidth = 0.55f;

    Audio result;
    parameters.shape = BodyShape::Parlor;
    parameters.bodyMaterial = BodyMaterial::Spruce;
    append(result, anchorChord(parameters), 0.45);
    parameters.shape = BodyShape::Jumbo;
    append(result, anchorChord(parameters), 0.45);
    parameters.shape = BodyShape::Auditorium;
    parameters.bodyMaterial = BodyMaterial::Cedar;
    append(result, anchorChord(parameters), 0.45);
    parameters.bodyMaterial = BodyMaterial::Maple;
    append(result, anchorChord(parameters));
    return result;
}

Audio agePhrase(EngineParameters parameters)
{
    Take take(parameters);
    take.strum({ 45, 52, 57, 61, 64 }, 0.86f, 0.035);
    take.rest(1.55);
    take.release({ 45, 52, 57, 61, 64 });
    take.rest(1.20);
    return take.finish();
}

Audio stringAge()
{
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Steel;
    parameters.shape = BodyShape::Dreadnought;
    parameters.bodyMaterial = BodyMaterial::Spruce;
    parameters.pluckPosition = 0.20f;
    parameters.touch = 0.72f;

    Audio result;
    parameters.stringAge = 0.0f;
    append(result, agePhrase(parameters), 0.60);
    parameters.stringAge = 1.0f;
    append(result, agePhrase(parameters));
    return result;
}

Audio playingBehaviours()
{
    // The three things the instrument does beyond holding a note: a chord
    // change carries the taken strings' vibration into a damped tail instead
    // of cutting it, CC2 rests the picking hand by the saddle, and a pitch
    // above the fretted range is played as the natural harmonic that reaches
    // it. No demo control is used that a player does not have.
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Steel;
    parameters.shape = BodyShape::Dreadnought;
    parameters.bodyMaterial = BodyMaterial::Spruce;
    parameters.pluckPosition = 0.24f;
    parameters.touch = 0.62f;

    Take take(parameters);
    // Two six-string chords overlapping reach the limiter at a full strum, and
    // this render must stay linear, so both are played back a little.
    take.strum({ 40, 47, 52, 56, 59, 64 }, 0.62f, 0.028);
    take.rest(1.30);
    // Taken while still ringing: the old chord is damped by the hand, not cut.
    take.strum({ 43, 50, 55, 58, 62, 67 }, 0.62f, 0.028);
    take.rest(1.60);
    take.release({ 43, 50, 55, 58, 62, 67 });
    take.rest(0.90);

    // Bridge-hand damping on a rising line. The notes do not repeat:
    // replucking a note already sounding on its string still restarts it from
    // rest, which steps the wave the bridge sees, and a demo should not be
    // built on top of that.
    take.bridgeHand(0.45f);
    take.rest(0.20);
    for (const int note : { 45, 52, 57, 60, 64, 69 })
        take.play(note, 0.72f, 0.16, 0.10);
    take.bridgeHand(0.0f);
    take.rest(0.60);

    // Fourth harmonic of the open high E, then the sixth of the open B.
    take.play(88, 0.80f, 1.10, 0.60);
    take.play(90, 0.80f, 1.10, 1.40);
    return take.finish();
}

Audio tuningChord(EngineParameters parameters,
                  std::initializer_list<int> notes)
{
    Take take(parameters);
    take.strum(notes, 0.86f, 0.040);
    take.rest(1.95);
    take.release(notes);
    // Leave enough tail to expose every tuning's open/fretted release behavior.
    take.rest(1.65);
    return take.finish();
}

Audio alternateTunings()
{
    auto parameters = baseParameters();
    parameters.stringMaterial = StringMaterial::Steel;
    parameters.shape = BodyShape::Jumbo;
    parameters.bodyMaterial = BodyMaterial::Mahogany;
    parameters.stringAge = 0.18f;
    parameters.touch = 0.54f;
    parameters.bodyAmount = 0.92f;

    Audio result;
    parameters.tuning = Tuning::DropD;
    append(result, tuningChord(parameters, { 38, 50, 57, 62 }), 0.50);
    parameters.tuning = Tuning::Dadgad;
    append(result, tuningChord(parameters, { 38, 50, 57, 62 }), 0.50);
    parameters.tuning = Tuning::OpenG;
    append(result, tuningChord(parameters, { 43, 50, 55, 59 }));
    return result;
}

struct Demo
{
    const char* fileName;
    const char* description;
    Audio (*render)();
};

constexpr std::array<Demo, 6> demos {{
    { "01-steel-sustain-range.wav",
      "Steel sustain from open E2 to B5, one held pluck at a time",
      steelSustainRange },
    { "02-nylon-fingerstyle.wav",
      "A fingertip nylon arpeggio with overlapping held notes",
      nylonFingerstyle },
    { "03-shape-material-anchors.wav",
      "One chord: Parlor/Jumbo, then Cedar/Maple anchor settings",
      shapeAndMaterialAnchors },
    { "04-string-age.wav",
      "The same steel phrase with fresh strings, then fully aged strings",
      stringAge },
    { "05-alternate-tunings.wav",
      "Drop D, DADGAD and Open G chords",
      alternateTunings },
    { "06-playing-behaviours.wav",
      "A chord change over a ringing chord, CC2 bridge-hand damping, then two "
      "natural harmonics above the fretted range",
      playingBehaviours },
}};

double peak(const Audio& audio)
{
    double result = 0.0;
    for (std::size_t index = 0; index < audio.left.size(); ++index)
        result = std::max({ result,
            std::abs(static_cast<double>(audio.left[index])),
            std::abs(static_cast<double>(audio.right[index])) });
    return result;
}

bool finite(const Audio& audio)
{
    if (audio.left.size() != audio.right.size() || audio.left.empty())
        return false;
    for (std::size_t index = 0; index < audio.left.size(); ++index)
        if (!std::isfinite(audio.left[index])
            || !std::isfinite(audio.right[index]))
            return false;
    return true;
}

double normalise(Audio& audio)
{
    const double current = peak(audio);
    if (current <= 1.0e-12)
        return 1.0;
    const double gain = normalisedPeak / current;
    for (std::size_t index = 0; index < audio.left.size(); ++index)
    {
        audio.left[index] = static_cast<float>(audio.left[index] * gain);
        audio.right[index] = static_cast<float>(audio.right[index] * gain);
    }
    return gain;
}

void writeLittleEndian(std::ostream& output, std::uint32_t value, int bytes)
{
    for (int byte = 0; byte < bytes; ++byte)
        output.put(static_cast<char>((value >> (8 * byte)) & 0xffu));
}

bool writeWav(const std::filesystem::path& path, const Audio& audio)
{
    if (audio.left.size() != audio.right.size()
        || audio.left.size() > (std::numeric_limits<std::uint32_t>::max() - 36u) / 4u)
        return false;

    const auto frames = static_cast<std::uint32_t>(audio.left.size());
    constexpr std::uint32_t channels = 2u;
    constexpr std::uint32_t bytesPerSample = 2u;
    const std::uint32_t dataBytes = frames * channels * bytesPerSample;

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output.write("RIFF", 4);
    writeLittleEndian(output, 36u + dataBytes, 4);
    output.write("WAVEfmt ", 8);
    writeLittleEndian(output, 16u, 4);
    writeLittleEndian(output, 1u, 2); // integer PCM
    writeLittleEndian(output, channels, 2);
    writeLittleEndian(output, static_cast<std::uint32_t>(demoSampleRate), 4);
    writeLittleEndian(output, static_cast<std::uint32_t>(demoSampleRate)
                                  * channels * bytesPerSample, 4);
    writeLittleEndian(output, channels * bytesPerSample, 2);
    writeLittleEndian(output, 16u, 2);
    output.write("data", 4);
    writeLittleEndian(output, dataBytes, 4);

    const auto encode = [] (float sample)
    {
        const float value = std::clamp(std::isfinite(sample) ? sample : 0.0f,
                                       -1.0f, 1.0f);
        const auto signedValue = static_cast<std::int16_t>(
            std::lround(static_cast<double>(value) * 32767.0));
        return static_cast<std::uint16_t>(signedValue);
    };
    for (std::size_t frame = 0; frame < audio.left.size(); ++frame)
    {
        writeLittleEndian(output, encode(audio.left[frame]), 2);
        writeLittleEndian(output, encode(audio.right[frame]), 2);
    }
    output.close();
    return !output.fail();
}

constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by AcustraRenderDemos;"
    " edits between the markers are overwritten -->";
constexpr const char* peaksTableEnd = "<!-- peaks-table-end -->";

std::filesystem::path instrumentReadme(const std::filesystem::path& directory)
{
    auto normalised = directory.lexically_normal();
    if (normalised.filename().empty())
        normalised = normalised.parent_path();
    if (normalised.filename() != "audio"
        || normalised.parent_path().filename() != "Docs")
        return {};
    return normalised.parent_path().parent_path() / "README.md";
}

struct RenderedLevel
{
    const char* fileName;
    const char* description;
    double seconds;
    double renderedPeakDb;
    double normalisationDb;
};

std::string signedDb(double value)
{
    char digits[32];
    std::snprintf(digits, sizeof digits, "%.1f", std::abs(value));
    const bool negative = value < 0.0 && std::strcmp(digits, "0.0") != 0;
    return std::string(negative ? "\xE2\x88\x92" : "+") + digits;
}

bool updatePeaksTable(const std::filesystem::path& directory,
                      const std::vector<RenderedLevel>& levels)
{
    const auto readmePath = instrumentReadme(directory);
    if (readmePath.empty())
        return true;

    std::ifstream input(readmePath, std::ios::binary);
    if (!input)
    {
        std::fprintf(stderr, "could not read %s\n", readmePath.string().c_str());
        return false;
    }
    const std::string readme((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());

    const auto begin = readme.find(peaksTableBegin);
    const auto content = begin == std::string::npos
        ? std::string::npos : begin + std::strlen(peaksTableBegin);
    const auto end = content == std::string::npos
        ? std::string::npos : readme.find(peaksTableEnd, content);
    const bool unique = begin != std::string::npos && end != std::string::npos
        && readme.find(peaksTableEnd) == end
        && readme.find(peaksTableBegin, content) == std::string::npos
        && readme.find(peaksTableEnd, end + std::strlen(peaksTableEnd))
            == std::string::npos;
    if (!unique)
    {
        std::fprintf(stderr,
            "%s must contain exactly one ordered Acustra peaks-table marker pair\n",
            readmePath.string().c_str());
        return false;
    }

    std::string table =
        "\n| File | What it is | Length | Rendered peak | Normalisation |\n"
        "| --- | --- | ---: | ---: | ---: |\n";
    for (const auto& level : levels)
    {
        char seconds[32];
        std::snprintf(seconds, sizeof seconds, "%.1f s", level.seconds);
        table += "| `" + std::string(level.fileName) + "` | "
            + level.description + " | " + seconds + " | "
            + signedDb(level.renderedPeakDb) + " dBFS | "
            + signedDb(level.normalisationDb) + " dB |\n";
    }

    const std::string updated = readme.substr(0, content) + table
                              + readme.substr(end);
    if (updated == readme)
        return true;

    std::ofstream output(readmePath, std::ios::binary | std::ios::trunc);
    output << updated;
    output.close();
    if (output.fail())
        return false;
    std::printf("Updated the rendered-peak table in %s\n",
                readmePath.string().c_str());
    return true;
}

Audio smokeTake()
{
    auto parameters = baseParameters();
    Take take(parameters);
    take.strum({ 40, 47, 52 }, 0.82f, 0.02);
    take.rest(0.35);
    take.release({ 40, 47, 52 });
    take.rest(0.20);
    return take.finish();
}

int smokeTest(const std::filesystem::path& directory)
{
    const Audio first = smokeTake();
    const Audio second = smokeTake();
    if (!finite(first) || first.left != second.left || first.right != second.right)
    {
        std::fprintf(stderr, "smoke test: render was non-finite or non-deterministic\n");
        return 1;
    }
    if (peak(first) < 0.03)
    {
        std::fprintf(stderr,
            "smoke test: engine fell below its calibrated output reference\n");
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error || !std::filesystem::is_directory(directory))
    {
        std::fprintf(stderr, "smoke test: not a directory: %s\n",
                     directory.string().c_str());
        return 1;
    }

    std::filesystem::path path;
    for (int suffix = 0; suffix < 100; ++suffix)
    {
        path = directory / (".acustra-smoke-" + std::to_string(suffix) + ".wav");
        if (!std::filesystem::exists(path))
            break;
        path.clear();
    }
    if (path.empty() || !writeWav(path, first))
    {
        std::fprintf(stderr, "smoke test: could not write a temporary WAV\n");
        return 1;
    }
    const auto expected = 44u + first.left.size() * 4u;
    const auto actual = std::filesystem::file_size(path, error);
    std::filesystem::remove(path, error);
    if (error || actual != expected)
    {
        std::fprintf(stderr, "smoke test: temporary WAV was malformed\n");
        return 1;
    }

    std::printf("Acustra demo renderer smoke test passed (peak %.6f).\n",
                peak(first));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    bool directorySet = false;
    std::filesystem::path directory = "Docs/audio";
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--smoke")
            smoke = true;
        else if (argument == "--help" || argument == "-h")
        {
            std::printf("usage: AcustraRenderDemos [--smoke] [output-directory]\n");
            return 0;
        }
        else if (!argument.empty() && argument.front() == '-')
        {
            std::fprintf(stderr, "unknown option: %s\n", argument.c_str());
            return 2;
        }
        else if (directorySet)
        {
            std::fprintf(stderr, "only one output directory may be supplied\n");
            return 2;
        }
        else
        {
            directory = argument;
            directorySet = true;
        }
    }

    if (smoke)
        return smokeTest(directory);

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error || !std::filesystem::is_directory(directory))
    {
        std::fprintf(stderr, "not a directory: %s\n", directory.string().c_str());
        return 1;
    }

    std::vector<RenderedLevel> levels;
    levels.reserve(demos.size());
    for (const auto& demo : demos)
    {
        Audio audio = demo.render();
        if (!finite(audio))
        {
            std::fprintf(stderr, "%s rendered invalid audio\n", demo.fileName);
            return 1;
        }
        const double renderedPeak = peak(audio);
        // Staying below the limiter threshold proves the listening renders are
        // linear before their one whole-file normalisation pass. The floor is
        // a liveness check on the other side - a render this quiet is broken
        // rather than quiet - and carries no claim about how loud a demo
        // should be, since each one is normalised afterwards anyway.
        if (renderedPeak < 0.04 || renderedPeak > 0.89125094)
        {
            std::fprintf(stderr, "%s rendered outside the safe peak range (%.6f)\n",
                         demo.fileName, renderedPeak);
            return 1;
        }
        const double gain = normalise(audio);
        const auto path = directory / demo.fileName;
        if (!writeWav(path, audio))
        {
            std::fprintf(stderr, "could not write %s\n", path.string().c_str());
            return 1;
        }

        levels.push_back({ demo.fileName, demo.description,
            static_cast<double>(audio.left.size()) / demoSampleRate,
            20.0 * std::log10(renderedPeak), 20.0 * std::log10(gain) });
        std::printf("Rendered %-31s %5.1f s  peak %6.1f dBFS\n",
                    demo.fileName, levels.back().seconds,
                    levels.back().renderedPeakDb);
    }

    if (!updatePeaksTable(directory, levels))
        return 1;
    std::printf("Wrote exactly %zu demonstration files to %s\n", demos.size(),
                directory.string().c_str());
    return 0;
}

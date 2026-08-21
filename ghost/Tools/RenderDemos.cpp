// Renders the committed demonstration WAVs under Docs/audio from the same
// JUCE-free engine the eventual plug-in will run, so a demonstration cannot
// drift away from what Ghost actually sounds like. No samples or external
// processing are involved anywhere: every file here comes out of the model.
//
// While the voice is the pre-research skeleton the demo table holds a single
// placeholder take; the real demo set lands with the researched circuit model.

#include "DSP/GhostEngine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
using ghost::EngineParameters;
using ghost::GhostEngine;

// 44.1 kHz 16-bit is what a listener's browser, phone and DAW all handle
// without conversion, and it is the rate the instrument is most often used at.
constexpr double demoSampleRate = 44100.0;
constexpr int renderBlockSize = 256;
// -3 dBFS: loud enough to audition without a gain change between files, with
// headroom left so no 16-bit sample sits against full scale.
constexpr double normalisedPeak = 0.7079457843841379;
// These are file-delivery guards rather than synthesizer transfer values. A
// demo with more DC can waste headroom or thump downstream equipment, while a
// large non-zero endpoint can click when a player starts or stops the file.
constexpr double maximumAbsoluteDcMean = 0.005;
constexpr double maximumEdgeMagnitude = 0.01;

// ---------------------------------------------------------------------------
// WAV output
// ---------------------------------------------------------------------------

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value,
                        int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xffu));
}

// Every file this tool produces goes through one door: the bytes are written
// to a uniquely named sibling temporary and take the live name only after a
// successful close. A full filesystem therefore cannot truncate an existing
// file, no pre-existing file of any name is destroyed before the replacement
// is safely on disk, and opening through std::ofstream's path overload keeps
// non-ASCII directories working on Windows, where fopen(path.string()) would
// go through the narrow code page.
bool writeFileAtomically(const std::filesystem::path& path, const char* data,
                         std::size_t size)
{
    std::random_device device;
    std::filesystem::path temporary;
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        char suffix[24];
        std::snprintf(suffix, sizeof suffix, ".%08x.tmp",
                      static_cast<unsigned>(device()));
        auto candidate = path;
        candidate += suffix;
        if (!std::filesystem::exists(candidate))
        {
            temporary = candidate;
            break;
        }
    }
    if (temporary.empty())
        return false;

    std::ofstream output(temporary, std::ios::binary);
    output.write(data, static_cast<std::streamsize>(size));
    output.close();
    if (output.fail())
    {
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return false;
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);
    if (renameError)
    {
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return false;
    }
    return true;
}

// Ghost is a monophonic instrument, but every take is written as a stereo
// file so the committed corpus keeps one delivery format when a stereo output
// stage lands.
bool writeWav(const std::filesystem::path& path, const std::vector<float>& left,
              const std::vector<float>& right)
{
    const auto frames = static_cast<std::uint32_t>(left.size());
    constexpr std::uint16_t channels = 2u;
    const std::uint32_t byteRate =
        static_cast<std::uint32_t>(demoSampleRate) * channels * 2u;
    const std::uint32_t dataBytes = frames * channels * 2u;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44u + dataBytes);
    const auto tag = [&bytes](const char* text)
    {
        for (int index = 0; index < 4; ++index)
            bytes.push_back(static_cast<std::uint8_t>(text[index]));
    };
    tag("RIFF");
    appendLittleEndian(bytes, 36u + dataBytes, 4);
    tag("WAVE");
    tag("fmt ");
    appendLittleEndian(bytes, 16u, 4);
    appendLittleEndian(bytes, 1u, 2); // PCM
    appendLittleEndian(bytes, channels, 2);
    appendLittleEndian(bytes, static_cast<std::uint32_t>(demoSampleRate), 4);
    appendLittleEndian(bytes, byteRate, 4);
    appendLittleEndian(bytes, channels * 2u, 2); // block align
    appendLittleEndian(bytes, 16u, 2);           // bits per sample
    tag("data");
    appendLittleEndian(bytes, dataBytes, 4);

    const auto encode = [](float value)
    {
        if (!std::isfinite(value))
            value = 0.0f;
        const float clamped = std::clamp(value, -1.0f, 1.0f);
        // Symmetric scaling by 32767 so a full-scale negative peak cannot wrap.
        const auto sample = static_cast<std::int32_t>(
            std::lround(static_cast<double>(clamped) * 32767.0));
        return static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(static_cast<std::int16_t>(sample)));
    };

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        appendLittleEndian(bytes, encode(left[frame]), 2);
        appendLittleEndian(bytes, encode(right[frame]), 2);
    }

    return writeFileAtomically(path,
                               reinterpret_cast<const char*>(bytes.data()),
                               bytes.size());
}

// ---------------------------------------------------------------------------
// A take: an imperative score driving the engine
// ---------------------------------------------------------------------------

class Take
{
public:
    // The engine is held indirectly because it is large; a take, on the other
    // hand, is returned by value from every score below.
    explicit Take(EngineParameters parameters)
        : engine_(std::make_unique<GhostEngine>())
    {
        engine_->prepare(demoSampleRate, renderBlockSize);
        engine_->setParameters(parameters);
    }

    void setParameters(const EngineParameters& parameters)
    {
        engine_->setParameters(parameters);
    }

    void on(int note, float velocity = 1.0f) { engine_->noteOn(note, velocity); }
    void off(int note) { engine_->noteOff(note); }
    void bend(float amount) { engine_->setPitchBend(amount); }
    void wheel(float amount) { engine_->setModWheel(amount); }

    // Strike a key, hold it for `holdSeconds`, release it, then let it ring
    // for `gapSeconds` before returning.
    void hit(int note, float velocity, double holdSeconds, double gapSeconds)
    {
        on(note, velocity);
        rest(holdSeconds);
        off(note);
        rest(gapSeconds);
    }

    void rest(double seconds) { render(seconds); }

    [[nodiscard]] const std::vector<float>& left() const noexcept { return left_; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return right_; }

    [[nodiscard]] bool finite() const noexcept
    {
        for (std::size_t index = 0; index < left_.size(); ++index)
            if (!std::isfinite(left_[index]) || !std::isfinite(right_[index]))
                return false;
        return true;
    }

    [[nodiscard]] double peak() const noexcept
    {
        double result = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
            result = std::max({ result, std::abs(static_cast<double>(left_[index])),
                                std::abs(static_cast<double>(right_[index])) });
        return result;
    }

    [[nodiscard]] double absoluteDcMean() const noexcept
    {
        if (left_.empty())
            return 0.0;

        double leftSum = 0.0;
        double rightSum = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            leftSum += static_cast<double>(left_[index]);
            rightSum += static_cast<double>(right_[index]);
        }
        const auto frames = static_cast<double>(left_.size());
        return std::max(std::abs(leftSum / frames),
                        std::abs(rightSum / frames));
    }

    [[nodiscard]] double firstEdgeMagnitude() const noexcept
    {
        if (left_.empty())
            return 0.0;
        return std::max(std::abs(static_cast<double>(left_.front())),
                        std::abs(static_cast<double>(right_.front())));
    }

    [[nodiscard]] double lastEdgeMagnitude() const noexcept
    {
        if (left_.empty())
            return 0.0;
        return std::max(std::abs(static_cast<double>(left_.back())),
                        std::abs(static_cast<double>(right_.back())));
    }

    // Brings the take to a common listening level. The returned value is the
    // gain applied, so the manifest can record what each file needed.
    double normalise()
    {
        const auto current = peak();
        if (current <= 1.0e-9)
            return 1.0;
        const auto gain = normalisedPeak / current;
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            left_[index] = static_cast<float>(left_[index] * gain);
            right_[index] = static_cast<float>(right_[index] * gain);
        }
        return gain;
    }

private:
    void render(double seconds)
    {
        auto remaining =
            static_cast<int>(std::lround(seconds * demoSampleRate));
        std::array<float, renderBlockSize> blockLeft {};
        std::array<float, renderBlockSize> blockRight {};

        while (remaining > 0)
        {
            const int count = std::min(renderBlockSize, remaining);
            engine_->process(blockLeft.data(), blockRight.data(), count);
            left_.insert(left_.end(), blockLeft.begin(), blockLeft.begin() + count);
            right_.insert(right_.end(), blockRight.begin(),
                          blockRight.begin() + count);
            remaining -= count;
        }
    }

    std::unique_ptr<GhostEngine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

// The panel every score starts from.
EngineParameters plainPanel()
{
    EngineParameters parameters;
    return parameters;
}

// ---------------------------------------------------------------------------
// The takes
// ---------------------------------------------------------------------------

// Placeholder take proving the render path end to end; the researched model
// replaces the whole table.
Take renderSkeletonVoice()
{
    auto parameters = plainPanel();
    parameters.cutoff = 0.55f;
    parameters.envToCutoff = 0.4f;
    parameters.attack = 0.0f;
    parameters.decay = 0.5f;
    parameters.sustain = 0.6f;
    parameters.release = 0.35f;
    Take take(parameters);

    take.rest(0.10);
    take.hit(36, 0.9f, 0.5, 0.3);
    take.hit(43, 0.9f, 0.5, 0.3);
    take.hit(48, 0.9f, 0.9, 1.6);
    return take;
}

// ---------------------------------------------------------------------------
// The demo table
// ---------------------------------------------------------------------------

struct Demo
{
    const char* fileName;
    const char* description;
    Take (*render)();
};

const std::array<Demo, 1>& demos()
{
    static const std::array<Demo, 1> table {{
        { "01-skeleton-voice.wav",
          "Pre-research skeleton voice: proves the render path, not a sound",
          renderSkeletonVoice },
    }};
    return table;
}

// A short render used by the regression suite: it proves the tool and the
// engine still produce finite, audible audio and a readable WAV without
// committing anything.
int runSmokeTest(const std::filesystem::path& directory)
{
    auto parameters = plainPanel();
    parameters.attack = 0.0f;
    Take take(parameters);
    take.hit(60, 0.95f, 0.30, 0.25);

    if (!take.finite())
    {
        std::fprintf(stderr, "smoke test: rendered a non-finite sample\n");
        return 1;
    }
    if (take.peak() < 1.0e-3)
    {
        std::fprintf(stderr, "smoke test: rendered silence (peak %.6f)\n",
                     take.peak());
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto path = directory / "smoke.wav";
    // The smoke path takes an ordinary directory argument just like the full
    // render, so it gets the same respect for other people's files: it only
    // ever writes a file it can prove it is not replacing.
    if (std::filesystem::exists(path))
    {
        std::fprintf(stderr,
                     "smoke test: %s already exists; refusing to overwrite "
                     "it.\n",
                     path.string().c_str());
        return 1;
    }
    if (!writeWav(path, take.left(), take.right()))
    {
        std::fprintf(stderr, "smoke test: could not write %s\n",
                     path.string().c_str());
        return 1;
    }
    const auto size = std::filesystem::file_size(path, error);
    std::filesystem::remove(path, error);
    if (size < 44u + take.left().size() * 4u)
    {
        std::fprintf(stderr, "smoke test: short WAV (%llu bytes)\n",
                     static_cast<unsigned long long>(size));
        return 1;
    }

    std::printf("Ghost demo renderer smoke test passed (peak %.3f).\n",
                take.peak());
    return 0;
}

// The per-file table in the output directory's README is regenerated in place
// on every full render, so the documented rendered peaks can never drift away
// from the committed WAVs. The markers bound exactly what the renderer owns;
// the prose around them stays hand-written.
constexpr const char* peaksTableBegin =
    "<!-- peaks-table-begin: regenerated by GhostRenderDemos;"
    " edits between the markers are overwritten -->";
constexpr const char* peaksTableEnd = "<!-- peaks-table-end -->";

// Whether this directory is one this tool owns and may therefore delete from.
// The proof is the manifest the renderer itself maintains: a README carrying
// the markers it rewrites the level table between. Without that, the directory
// belongs to someone else and nothing in it may be removed — the output path
// is an ordinary command-line argument, and pointing it at a folder of music
// must not destroy the music.
bool ownsDirectory(const std::filesystem::path& directory)
{
    const auto readmePath = directory / "README.md";
    if (!std::filesystem::exists(readmePath))
        return false;

    std::ifstream input(readmePath, std::ios::binary);
    const std::string readme((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    // Both markers, in order: a truncated or conflict-mangled README that
    // kept only the begin marker must not authorise deleting anything.
    const auto beginPos = readme.find(peaksTableBegin);
    const auto endPos = readme.find(peaksTableEnd);
    return beginPos != std::string::npos && endPos != std::string::npos
        && beginPos < endPos;
}

// A directory the renderer starts empty is the renderer's to keep: writing
// the manifest README first makes the first render's output replaceable by
// the second, while a directory that already held anything keeps its foreign
// status and the collision guard's full protection.
bool claimFreshDirectory(const std::filesystem::path& directory)
{
    if (!std::filesystem::is_empty(directory))
        return false;

    const std::string manifest =
        std::string("# Ghost audio corpus\n\n")
        + "Rendered by GhostRenderDemos from the JUCE-free engine. The\n"
          "level table between the markers below is regenerated on every\n"
          "full render.\n\n"
        + peaksTableBegin + "\n" + peaksTableEnd + "\n";
    // Atomic like every other write, so a failure cannot leave a partial,
    // marker-less README that would make the directory look foreign while no
    // longer being empty.
    return writeFileAtomically(directory / "README.md", manifest.data(),
                               manifest.size());
}

// A demo removed from or renamed in the tables above must also disappear from
// the output directory, or automation that commits the directory preserves
// the stale file forever while the level table drops its row.
bool removeStaleWavs(const std::filesystem::path& directory)
{
    if (!ownsDirectory(directory))
    {
        std::printf("%s is not this renderer's own output directory, so nothing "
                    "in it will be removed.\n", directory.string().c_str());
        return true;
    }

    const auto& current = demos();
    bool removedAll = true;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".wav")
            continue;

        // Second guard: even inside its own directory the renderer only
        // removes files that follow its own naming, so anything a person put
        // here by hand survives.
        const auto stem = entry.path().filename().string();
        if (stem.size() < 3 || !std::isdigit(static_cast<unsigned char>(stem[0]))
            || !std::isdigit(static_cast<unsigned char>(stem[1]))
            || stem[2] != '-')
            continue;

        const auto name = entry.path().filename().string();
        const bool known =
            std::any_of(current.begin(), current.end(), [&name](const Demo& demo)
                        { return name == demo.fileName; });
        if (known)
            continue;

        std::error_code error;
        if (std::filesystem::remove(entry.path(), error))
            std::printf("Removed stale demo %s\n", name.c_str());
        else
        {
            std::fprintf(stderr, "could not remove stale demo %s\n", name.c_str());
            removedAll = false;
        }
    }
    return removedAll;
}

struct RenderedLevel
{
    std::string fileName;
    std::string description;
    double seconds = 0.0;
    double renderedPeakDb = 0.0;
    double normalisationDb = 0.0;
};

// A real minus sign rather than a hyphen, so the table reads as typeset prose.
std::string formatSignedDb(double value)
{
    char digits[32];
    std::snprintf(digits, sizeof digits, "%.1f", std::fabs(value));
    const bool negative = value < 0.0 && std::strcmp(digits, "0.0") != 0;
    return std::string(negative ? "\xE2\x88\x92" : "+") + digits;
}

bool updatePeaksTable(const std::filesystem::path& directory,
                      const std::vector<RenderedLevel>& levels)
{
    const auto readmePath = directory / "README.md";
    if (!std::filesystem::exists(readmePath))
        return true; // An ad-hoc output directory carries no documentation.

    std::ifstream input(readmePath, std::ios::binary);
    std::string readme((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
    input.close();

    const auto beginPos = readme.find(peaksTableBegin);
    const auto endPos = readme.find(peaksTableEnd);
    if (beginPos == std::string::npos || endPos == std::string::npos
        || endPos < beginPos)
    {
        std::fprintf(stderr,
                     "%s has no peaks-table markers, so its level table can no "
                     "longer be kept in sync with the rendered files.\n",
                     readmePath.string().c_str());
        return false;
    }

    std::string table =
        "\n| File | What it is | Length | Rendered peak | Normalisation |\n"
        "| --- | --- | ---: | ---: | ---: |\n";
    for (const auto& level : levels)
    {
        char length[32];
        std::snprintf(length, sizeof length, "%.1f s", level.seconds);
        table += "| `" + level.fileName + "` | " + level.description + " | "
               + length + " | " + formatSignedDb(level.renderedPeakDb) + " dBFS | "
               + formatSignedDb(level.normalisationDb) + " dB |\n";
    }

    const auto contentStart = beginPos + std::strlen(peaksTableBegin);
    const auto updated =
        readme.substr(0, contentStart) + table + readme.substr(endPos);
    if (updated == readme)
        return true;

    if (!writeFileAtomically(readmePath, updated.data(), updated.size()))
    {
        std::fprintf(stderr, "could not replace %s\n",
                     readmePath.string().c_str());
        return false;
    }

    std::printf("Updated the rendered-peak table in %s\n",
                readmePath.string().c_str());
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> arguments(argv + 1, argv + argc);
    bool smoke = false;
    std::filesystem::path directory = "Docs/audio";

    for (const auto& argument : arguments)
    {
        if (argument == "--smoke")
        {
            smoke = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::printf("usage: GhostRenderDemos [--smoke] [output-directory]\n");
            return 0;
        }
        else
        {
            directory = argument;
        }
    }

    if (smoke)
        return runSmokeTest(directory);

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (!std::filesystem::is_directory(directory))
    {
        std::fprintf(stderr, "not a directory: %s\n", directory.string().c_str());
        return 1;
    }

    // A directory this run created (or found empty) becomes the renderer's
    // own before anything is written into it, so the documented render
    // command can be run twice; anything else must already prove ownership.
    claimFreshDirectory(directory);

    if (!removeStaleWavs(directory))
        return 1;

    // Deletion above and overwriting below answer to the same ownership
    // proof: pointing the ordinary output argument at a folder of music must
    // not replace a colliding file any more than it may remove one.
    const bool owned = ownsDirectory(directory);

    // A README without the manifest markers means somebody else's directory
    // with somebody else's documentation. Failing here, before the first WAV
    // is written, keeps a doomed run from polluting it — the table update at
    // the end would reject that README anyway, but only after the demos had
    // already landed. A directory with no README at all stays usable as an
    // ad-hoc output target.
    if (!owned && std::filesystem::exists(directory / "README.md"))
    {
        std::fprintf(stderr,
                     "%s has a README that is not this renderer's manifest; "
                     "no demos will be written there.\n",
                     directory.string().c_str());
        return 1;
    }

    std::vector<RenderedLevel> levels;

    for (const auto& demo : demos())
    {
        auto take = demo.render();

        if (!take.finite())
        {
            std::fprintf(stderr, "%s rendered a non-finite sample\n",
                         demo.fileName);
            return 1;
        }

        const auto renderedPeak = take.peak();
        if (renderedPeak < 1.0e-4)
        {
            std::fprintf(stderr, "%s rendered silence (peak %.6f)\n",
                         demo.fileName, renderedPeak);
            return 1;
        }
        const auto gain = take.normalise();
        const auto absoluteDcMean = take.absoluteDcMean();
        const auto firstEdge = take.firstEdgeMagnitude();
        const auto lastEdge = take.lastEdgeMagnitude();
        if (absoluteDcMean > maximumAbsoluteDcMean)
        {
            std::fprintf(stderr,
                         "%s rejected: normalised absolute DC mean %.6f FS "
                         "exceeds %.6f FS\n",
                         demo.fileName, absoluteDcMean, maximumAbsoluteDcMean);
            return 1;
        }
        if (firstEdge > maximumEdgeMagnitude || lastEdge > maximumEdgeMagnitude)
        {
            std::fprintf(stderr,
                         "%s rejected: normalised first/last edge "
                         "%.6f/%.6f FS exceeds %.6f FS\n",
                         demo.fileName, firstEdge, lastEdge,
                         maximumEdgeMagnitude);
            return 1;
        }

        const auto path = directory / demo.fileName;
        if (!owned && std::filesystem::exists(path))
        {
            std::fprintf(stderr,
                         "%s already exists in a directory this renderer does "
                         "not own; refusing to overwrite it.\n",
                         path.string().c_str());
            return 1;
        }
        if (!writeWav(path, take.left(), take.right()))
        {
            std::fprintf(stderr, "could not write %s\n", path.string().c_str());
            return 1;
        }

        RenderedLevel level;
        level.fileName = demo.fileName;
        level.description = demo.description;
        level.seconds = static_cast<double>(take.left().size()) / demoSampleRate;
        level.renderedPeakDb = 20.0 * std::log10(renderedPeak);
        level.normalisationDb = 20.0 * std::log10(gain);
        levels.push_back(std::move(level));

        std::printf("Rendered %-28s peak %6.3f, %5.1f s\n", demo.fileName,
                    renderedPeak, level.seconds);
    }

    if (!updatePeaksTable(directory, levels))
        return 1;

    std::printf("Rendered %zu demos into %s\n", demos().size(),
                directory.string().c_str());
    return 0;
}

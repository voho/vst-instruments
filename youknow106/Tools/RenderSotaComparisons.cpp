// Renders representative before and after WAV files comparing each SOTA
// physical circuit simulation feature under settings tuned to make the effect
// cleanly audible.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::EnvPolarity;
using youknow106::HighPassMode;
using youknow106::KeyMode;
using youknow106::PwmSource;
using youknow106::VcaMode;
using youknow106::YouKnow106Engine;

constexpr double sampleRate = 44100.0;
constexpr int renderBlockSize = 256;
constexpr double normalisedPeak = 0.7079457843841379;

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value, int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xffu));
}

bool writeWav(const std::filesystem::path& path, const std::vector<float>& left, const std::vector<float>& right)
{
    const auto frames = static_cast<std::uint32_t>(left.size());
    constexpr std::uint16_t channels = 2u;
    const std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRate) * channels * 2u;
    const std::uint32_t dataBytes = frames * channels * 2u;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44u + dataBytes);
    const auto tag = [&bytes](const char* text) {
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
    appendLittleEndian(bytes, static_cast<std::uint32_t>(sampleRate), 4);
    appendLittleEndian(bytes, byteRate, 4);
    appendLittleEndian(bytes, channels * 2u, 2); // block align
    appendLittleEndian(bytes, 16u, 2);           // bits per sample
    tag("data");
    appendLittleEndian(bytes, dataBytes, 4);

    const auto encode = [](float value) {
        if (!std::isfinite(value))
            value = 0.0f;
        const float clamped = std::clamp(value, -1.0f, 1.0f);
        const auto sample = static_cast<std::int32_t>(std::lround(static_cast<double>(clamped) * 32767.0));
        return static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(sample)));
    };

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        appendLittleEndian(bytes, encode(left[frame]), 2);
        appendLittleEndian(bytes, encode(right[frame]), 2);
    }

    std::filesystem::create_directories(path.parent_path());
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return written;
}

struct Level
{
    double peak { 0.0 };
    double rms { 0.0 };
};

double decibels(double ratio)
{
    return 20.0 * std::log10(ratio + 1.0e-18);
}

class Take
{
public:
    explicit Take(EngineParameters parameters)
        : engine_(std::make_unique<YouKnow106Engine>())
    {
        engine_->prepare(sampleRate, renderBlockSize, true);
        engine_->setParameters(parameters);
    }

    void on(int note, float velocity = 1.0f) { engine_->noteOn(note, velocity); }
    void off(int note) { engine_->noteOff(note); }

    void hit(int note, float velocity, double holdSeconds, double gapSeconds)
    {
        on(note, velocity);
        rest(holdSeconds);
        off(note);
        rest(gapSeconds);
    }

    void chord(std::initializer_list<int> notes, float velocity, double holdSeconds, double gapSeconds)
    {
        for (const int note : notes)
            on(note, velocity);
        rest(holdSeconds);
        for (const int note : notes)
            off(note);
        rest(gapSeconds);
    }

    void rest(double seconds)
    {
        auto remaining = static_cast<int>(std::lround(seconds * sampleRate));
        std::array<float, renderBlockSize> blockLeft {};
        std::array<float, renderBlockSize> blockRight {};

        while (remaining > 0)
        {
            const int count = std::min(renderBlockSize, remaining);
            engine_->process(blockLeft.data(), blockRight.data(), count);
            left_.insert(left_.end(), blockLeft.begin(), blockLeft.begin() + count);
            right_.insert(right_.end(), blockRight.begin(), blockRight.begin() + count);
            remaining -= count;
        }
    }

    [[nodiscard]] const std::vector<float>& left() const noexcept { return left_; }
    [[nodiscard]] const std::vector<float>& right() const noexcept { return right_; }

    // Peak and RMS across both channels, before any gain is applied.
    [[nodiscard]] Level measure() const
    {
        Level level;
        double sumOfSquares = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            const double l = left_[index];
            const double r = right_[index];
            level.peak = std::max({ level.peak, std::abs(l), std::abs(r) });
            sumOfSquares += l * l + r * r;
        }
        const auto samples = static_cast<double>(left_.size()) * 2.0;
        level.rms = samples > 0.0 ? std::sqrt(sumOfSquares / samples) : 0.0;
        return level;
    }

    void applyGain(double gain)
    {
        for (std::size_t index = 0; index < left_.size(); ++index)
        {
            left_[index] = static_cast<float>(left_[index] * gain);
            right_[index] = static_cast<float>(right_[index] * gain);
        }
    }

    double normalise()
    {
        const double current = measure().peak;
        if (current <= 1.0e-9)
            return 1.0;
        const auto gain = normalisedPeak / current;
        applyGain(gain);
        return gain;
    }

    Take(std::vector<float> left, std::vector<float> right)
        : engine_(nullptr), left_(std::move(left)), right_(std::move(right)) {}

    Take diffWith(const Take& before) const
    {
        const std::size_t count = std::min(left_.size(), before.left_.size());
        std::vector<float> diffL(count);
        std::vector<float> diffR(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            diffL[i] = left_[i] - before.left_[i];
            diffR[i] = right_[i] - before.right_[i];
        }
        return Take(std::move(diffL), std::move(diffR));
    }

private:
    std::unique_ptr<YouKnow106Engine> engine_;
    std::vector<float> left_;
    std::vector<float> right_;
};

EngineParameters defaultPanel()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.62f;
    parameters.resonance = 0.10f;
    parameters.envDepth = 0.35f;
    parameters.keyFollow = 0.5f;
    parameters.attack = 0.04f;
    parameters.decay = 0.45f;
    parameters.sustain = 0.70f;
    parameters.release = 0.30f;
    parameters.vcaLevel = 0.84f;
    parameters.chorus = ChorusMode::Off;
    parameters.volume = 0.45f;
    return parameters;
}

// 1. VCF Stage Transistor Offsets: High resonance filter sweep on raw sawtooth
Take renderVcfOffsetsTake(bool enableOffsets)
{
    auto p = defaultPanel();
    p.enableVcfStageOffsets = enableOffsets;
    p.cutoff = 0.35f;
    p.resonance = 0.65f;
    p.envDepth = 0.50f;
    p.decay = 0.60f;
    p.sustain = 0.20f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 1.2, 0.4);
    take.hit(60, 0.95f, 1.2, 0.4);
    return take;
}

// 2. Op-Amp Slew-Rate Limiting: High-frequency resonant lead with Chorus II
Take renderOpAmpSlewTake(bool enableSlewLimiting)
{
    auto p = defaultPanel();
    p.enableOpAmpSlewLimiting = enableSlewLimiting;
    p.cutoff = 0.75f;
    p.resonance = 0.80f;
    p.envDepth = 0.20f;
    p.chorus = ChorusMode::Two;
    Take take(p);
    take.rest(0.05);
    take.chord({ 72, 76, 79 }, 0.95f, 1.5, 0.5);
    return take;
}

// 3. BBD Storage Capacitance Non-linearity: Loud polyphonic chorus strings
Take renderBbdCapacitanceTake(bool enableCapNonlinearity)
{
    auto p = defaultPanel();
    p.enableBbdCapacitanceNonlinearity = enableCapNonlinearity;
    p.sawEnabled = true;
    p.subLevel = 0.8f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.55f;
    p.attack = 0.20f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 48, 55, 60, 64 }, 0.95f, 2.0, 0.5);
    return take;
}

// 4. CD4051 Multiplexer Crosstalk: Fast staccato bassline
Take renderMuxCrosstalkTake(bool enableCrosstalk)
{
    auto p = defaultPanel();
    p.enableMuxCrosstalk = enableCrosstalk;
    p.range = DcoRange::Sixteen;
    p.subLevel = 0.9f;
    p.cutoff = 0.30f;
    p.resonance = 0.40f;
    p.envDepth = 0.60f;
    p.attack = 0.0f;
    p.decay = 0.20f;
    p.sustain = 0.10f;
    Take take(p);
    take.rest(0.05);
    for (int i = 0; i < 4; ++i)
        take.hit(36, 0.95f, 0.1, 0.15);
    return take;
}

// 5. Exponential DCO Ramp Reset Dynamics: Bright high note lead
Take renderExponentialResetTake(bool enableExponentialReset)
{
    auto p = defaultPanel();
    p.enableExponentialReset = enableExponentialReset;
    p.range = DcoRange::Four;
    p.sawEnabled = true;
    p.cutoff = 0.95f;
    p.resonance = 0.10f;
    Take take(p);
    take.rest(0.05);
    take.hit(84, 0.95f, 1.0, 0.3);
    return take;
}

// 6. IR3109 VCF BJT Early Effect: Resonant filter sweep
Take renderVcfEarlyEffectTake(bool enableVcfEarlyEffect)
{
    auto p = defaultPanel();
    p.enableVcfEarlyEffect = enableVcfEarlyEffect;
    p.cutoff = 0.40f;
    p.resonance = 0.70f;
    p.envDepth = 0.50f;
    p.decay = 0.50f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 1.2, 0.4);
    return take;
}

// 7. Spatial Chassis Thermal Gradient: Sustained polyphonic chord sweep
Take renderSpatialThermalGradientTake(bool enableSpatialThermalGradient)
{
    auto p = defaultPanel();
    p.enableSpatialThermalGradient = enableSpatialThermalGradient;
    p.cutoff = 0.50f;
    p.resonance = 0.60f;
    p.attack = 0.30f;
    p.decay = 0.80f;
    p.sustain = 0.40f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 48, 55, 60, 64, 67, 72 }, 0.95f, 2.5, 0.5);
    return take;
}

// 8. Chorus Heterodyne Clock Bleed: Lush chorus pad
Take renderChorusClockBleedTake(bool enableChorusClockBleed)
{
    auto p = defaultPanel();
    p.enableChorusClockBleed = enableChorusClockBleed;
    p.sawEnabled = true;
    p.subLevel = 0.5f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.65f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 55, 59, 62, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 9. Chorus MN3101 Current-Controlled Hyperbolic Delay Sweep: Chorus pad
Take renderChorusHyperbolicSweepTake(bool enableChorusHyperbolicSweep)
{
    auto p = defaultPanel();
    p.enableChorusHyperbolicSweep = enableChorusHyperbolicSweep;
    p.sawEnabled = true;
    p.subLevel = 0.6f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.60f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 48, 55, 60, 64, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 10. DCO Integrator Finite Source Resistance Ramp Charging Curvature: Deep 16' bass note
Take renderDcoRampCurvatureTake(bool enableDcoRampCurvature)
{
    auto p = defaultPanel();
    p.enableDcoRampCurvature = enableDcoRampCurvature;
    p.range = DcoRange::Sixteen;
    p.sawEnabled = true;
    p.cutoff = 0.95f;
    p.resonance = 0.10f;
    Take take(p);
    take.rest(0.05);
    take.hit(36, 0.95f, 1.5, 0.4);
    return take;
}

// 11. C14 Non-Polar Electrolytic Voltage-Dependent HPF Modulation: Sub-bass + polyphonic chord
Take renderElectrolyticC14Take(bool enableElectrolyticC14Nonlinearity)
{
    auto p = defaultPanel();
    p.enableElectrolyticC14Nonlinearity = enableElectrolyticC14Nonlinearity;
    p.highPass = HighPassMode::Two;
    p.sawEnabled = true;
    p.subLevel = 1.0f;
    p.cutoff = 0.70f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 36, 60, 64, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

// 12. R-2R DAC Major Carrier Glitch Impulse: LFO filter sweep crossing MSB boundaries
Take renderDacGlitchImpulseTake(bool enableDacGlitchImpulse)
{
    auto p = defaultPanel();
    p.enableDacGlitchImpulse = enableDacGlitchImpulse;
    p.sawEnabled = true;
    p.cutoff = 0.50f;
    p.resonance = 0.50f;
    p.vcfLfoDepth = 0.60f;
    p.lfoRate = 0.70f;
    Take take(p);
    take.rest(0.05);
    take.hit(48, 0.95f, 2.0, 0.5);
    return take;
}

// One comparison: a slug for the file names, and a take factory that toggles
// exactly one mechanism while every other one stays at its shipped setting.
struct Comparison
{
    const char* slug;
    Take (*render)(bool);
};

const std::array<Comparison, 12> comparisons {{
    { "01-vcf-transistor-offsets",        renderVcfOffsetsTake },
    { "02-opamp-slew-limiting",           renderOpAmpSlewTake },
    { "03-bbd-capacitance-nonlinearity",  renderBbdCapacitanceTake },
    { "04-multiplexer-crosstalk",         renderMuxCrosstalkTake },
    { "05-exponential-dco-reset",         renderExponentialResetTake },
    { "06-vcf-early-effect",              renderVcfEarlyEffectTake },
    { "07-spatial-thermal-gradient",      renderSpatialThermalGradientTake },
    { "08-chorus-thiran-clock-bleed",     renderChorusClockBleedTake },
    { "09-chorus-hyperbolic-sweep",       renderChorusHyperbolicSweepTake },
    { "10-dco-ramp-curvature",            renderDcoRampCurvatureTake },
    { "11-electrolytic-c14-nonlinearity", renderElectrolyticC14Take },
    { "12-dac-glitch-impulse",            renderDacGlitchImpulseTake }
}};

struct Report
{
    std::string slug;
    double diffPeakDbc { 0.0 };
    double diffRmsDbc { 0.0 };
};

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path outputDir = "Docs/audio/sota-comparisons";
    if (argc > 1)
        outputDir = argv[1];

    std::printf("Rendering SOTA comparison WAVs into: %s\n\n", outputDir.string().c_str());
    std::printf("%-36s %12s %12s\n", "mechanism", "diff peak", "diff RMS");
    std::printf("%-36s %12s %12s\n", "", "(dBc)", "(dBc)");

    std::vector<Report> reports;
    reports.reserve(comparisons.size());

    for (const auto& comparison : comparisons)
    {
        auto before = comparison.render(false);
        auto after = comparison.render(true);
        auto diff = after.diffWith(before);

        // Measure before touching the gain. Everything is reported relative to
        // the programme the mechanism is riding on, which is the only figure
        // that says whether anyone can hear it.
        const auto beforeLevel = before.measure();
        const auto diffLevel = diff.measure();
        const double reference = std::max(beforeLevel.rms, 1.0e-12);
        Report report;
        report.slug = comparison.slug;
        report.diffPeakDbc = decibels(diffLevel.peak / reference);
        report.diffRmsDbc = decibels(diffLevel.rms / reference);
        reports.push_back(report);

        // One shared gain for all three files. The before/after pair is then
        // level-matched for honest A/B listening, and the diff keeps its true
        // size relative to them instead of being normalised up to look
        // significant.
        const double gain = before.normalise();
        after.applyGain(gain);
        diff.applyGain(gain);

        const std::string slug = comparison.slug;
        writeWav(outputDir / (slug + "-before.wav"), before.left(), before.right());
        writeWav(outputDir / (slug + "-after.wav"), after.left(), after.right());
        writeWav(outputDir / (slug + "-diff.wav"), diff.left(), diff.right());

        std::printf("%-36s %+12.1f %+12.1f\n", comparison.slug,
                    report.diffPeakDbc, report.diffRmsDbc);
    }

    // Keep the directory's own README in step with what was just rendered, so
    // the measured weight of each mechanism is documented rather than implied.
    std::string manifest =
        "# SOTA comparison renders\n\n"
        "Generated by `YouKnow106RenderSota`. Each row toggles **one** mechanism and\n"
        "leaves every other one at its shipped setting; `Unit Character` stays at its\n"
        "default in both takes, so the pair isolates the named feature rather than the\n"
        "whole character layer.\n\n"
        "`-before` and `-after` share one gain, so they are level-matched for A/B\n"
        "listening. `-diff` carries that same gain, so its loudness relative to the pair\n"
        "is its true loudness -- a diff that is inaudible in the file is inaudible in the\n"
        "instrument. The columns below are the diff measured against the before take's\n"
        "RMS.\n\n"
        "| Mechanism | Diff peak (dBc) | Diff RMS (dBc) |\n"
        "| --- | ---: | ---: |\n";
    for (const auto& report : reports)
    {
        std::array<char, 256> row {};
        std::snprintf(row.data(), row.size(), "| `%s` | %+.1f | %+.1f |\n",
                      report.slug.c_str(), report.diffPeakDbc, report.diffRmsDbc);
        manifest += row.data();
    }
    manifest +=
        "\nA mechanism whose diff RMS sits below about -80 dBc is not audible on the\n"
        "material it was given, whatever its entry in the research document claims.\n";

    std::filesystem::create_directories(outputDir);
    std::ofstream readme(outputDir / "README.md");
    readme << manifest;

    std::printf("\nWrote %zu comparisons and README.md\n", comparisons.size());
    return 0;
}

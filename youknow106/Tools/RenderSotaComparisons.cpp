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

    double normalise()
    {
        double current = 0.0;
        for (std::size_t index = 0; index < left_.size(); ++index)
            current = std::max({ current, std::abs(static_cast<double>(left_[index])),
                                 std::abs(static_cast<double>(right_[index])) });
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
    p.calibration = enableOffsets ? 5.0f : 0.0f;
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
    p.calibration = enableSlewLimiting ? 5.0f : 0.0f;
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
    p.calibration = enableCapNonlinearity ? 5.0f : 0.0f;
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
    p.calibration = enableCrosstalk ? 5.0f : 0.0f;
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
    p.calibration = enableExponentialReset ? 5.0f : 0.0f;
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
    p.calibration = enableVcfEarlyEffect ? 5.0f : 0.0f;
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
    p.calibration = enableSpatialThermalGradient ? 5.0f : 0.0f;
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

// 8. Chorus Thiran Fractional Delay & Heterodyne Clock Bleed: Lush chorus pad
Take renderChorusThiranTake(bool enableChorusThiranAndClockBleed)
{
    auto p = defaultPanel();
    p.calibration = enableChorusThiranAndClockBleed ? 5.0f : 0.0f;
    p.enableChorusThiranAndClockBleed = enableChorusThiranAndClockBleed;
    p.sawEnabled = true;
    p.subLevel = 0.5f;
    p.chorus = ChorusMode::Two;
    p.cutoff = 0.65f;
    Take take(p);
    take.rest(0.05);
    take.chord({ 55, 59, 62, 67 }, 0.95f, 2.0, 0.5);
    return take;
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path outputDir = "Docs/audio/sota-comparisons";
    if (argc > 1)
        outputDir = argv[1];

    std::printf("Rendering SOTA comparison WAVs into: %s\n", outputDir.string().c_str());

    // Feature 1: VCF Stage Transistor Offsets
    {
        auto before = renderVcfOffsetsTake(false);
        auto after = renderVcfOffsetsTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "01-vcf-transistor-offsets-before.wav", before.left(), before.right());
        writeWav(outputDir / "01-vcf-transistor-offsets-after.wav", after.left(), after.right());
        std::printf("Rendered 01-vcf-transistor-offsets (before & after)\n");
    }

    // Feature 2: Op-Amp Slew Limiting
    {
        auto before = renderOpAmpSlewTake(false);
        auto after = renderOpAmpSlewTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "02-opamp-slew-limiting-before.wav", before.left(), before.right());
        writeWav(outputDir / "02-opamp-slew-limiting-after.wav", after.left(), after.right());
        std::printf("Rendered 02-opamp-slew-limiting (before & after)\n");
    }

    // Feature 3: BBD Capacitance Non-linearity
    {
        auto before = renderBbdCapacitanceTake(false);
        auto after = renderBbdCapacitanceTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "03-bbd-capacitance-nonlinearity-before.wav", before.left(), before.right());
        writeWav(outputDir / "03-bbd-capacitance-nonlinearity-after.wav", after.left(), after.right());
        std::printf("Rendered 03-bbd-capacitance-nonlinearity (before & after)\n");
    }

    // Feature 4: Multiplexer Crosstalk
    {
        auto before = renderMuxCrosstalkTake(false);
        auto after = renderMuxCrosstalkTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "04-multiplexer-crosstalk-before.wav", before.left(), before.right());
        writeWav(outputDir / "04-multiplexer-crosstalk-after.wav", after.left(), after.right());
        std::printf("Rendered 04-multiplexer-crosstalk (before & after)\n");
    }

    // Feature 5: Exponential DCO Ramp Reset
    {
        auto before = renderExponentialResetTake(false);
        auto after = renderExponentialResetTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "05-exponential-dco-reset-before.wav", before.left(), before.right());
        writeWav(outputDir / "05-exponential-dco-reset-after.wav", after.left(), after.right());
        std::printf("Rendered 05-exponential-dco-reset (before & after)\n");
    }

    // Feature 6: IR3109 VCF BJT Early Effect
    {
        auto before = renderVcfEarlyEffectTake(false);
        auto after = renderVcfEarlyEffectTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "06-vcf-early-effect-before.wav", before.left(), before.right());
        writeWav(outputDir / "06-vcf-early-effect-after.wav", after.left(), after.right());
        std::printf("Rendered 06-vcf-early-effect (before & after)\n");
    }

    // Feature 7: Spatial Chassis Thermal Gradient
    {
        auto before = renderSpatialThermalGradientTake(false);
        auto after = renderSpatialThermalGradientTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "07-spatial-thermal-gradient-before.wav", before.left(), before.right());
        writeWav(outputDir / "07-spatial-thermal-gradient-after.wav", after.left(), after.right());
        std::printf("Rendered 07-spatial-thermal-gradient (before & after)\n");
    }

    // Feature 8: Chorus Thiran & Heterodyne Clock Bleed
    {
        auto before = renderChorusThiranTake(false);
        auto after = renderChorusThiranTake(true);
        before.normalise();
        after.normalise();
        writeWav(outputDir / "08-chorus-thiran-clock-bleed-before.wav", before.left(), before.right());
        writeWav(outputDir / "08-chorus-thiran-clock-bleed-after.wav", after.left(), after.right());
        std::printf("Rendered 08-chorus-thiran-clock-bleed (before & after)\n");
    }

    std::printf("All SOTA comparison WAVs successfully rendered!\n");
    return 0;
}

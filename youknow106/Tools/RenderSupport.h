// Shared rendering support for the JUCE-free comparison tools: 16-bit stereo
// WAV read/write, a level measurement, and a Take that drives one engine
// through a short performance.
#pragma once

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

namespace youknow106::tools
{

inline constexpr double sampleRate = 44100.0;
inline constexpr int renderBlockSize = 256;
inline constexpr double normalisedPeak = 0.7079457843841379;

inline void appendLittleEndian(std::vector<std::uint8_t>& bytes,
                               std::uint32_t value, int byteCount)
{
    for (int index = 0; index < byteCount; ++index)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xffu));
}

inline bool writeWav(const std::filesystem::path& path,
                     const std::vector<float>& left,
                     const std::vector<float>& right)
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

    std::filesystem::create_directories(path.parent_path());
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return written;
}

// Reads back a file this header wrote: 44-byte canonical header, 16-bit stereo.
// Anything else is refused rather than guessed at.
inline bool readWav(const std::filesystem::path& path,
                    std::vector<float>& left, std::vector<float>& right)
{
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr)
        return false;

    std::array<std::uint8_t, 44> header {};
    const bool readHeader = std::fread(header.data(), 1, header.size(), file)
                          == header.size();
    if (!readHeader
        || header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F'
        || header[20] != 1u || header[22] != 2u || header[34] != 16u)
    {
        std::fclose(file);
        return false;
    }

    left.clear();
    right.clear();
    std::array<std::uint8_t, 4> frame {};
    while (std::fread(frame.data(), 1, frame.size(), file) == frame.size())
    {
        const auto decode = [](std::uint8_t low, std::uint8_t high) {
            const auto raw = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(low)
                | static_cast<std::uint16_t>(static_cast<std::uint16_t>(high) << 8u));
            return static_cast<float>(static_cast<std::int16_t>(raw)) / 32767.0f;
        };
        left.push_back(decode(frame[0], frame[1]));
        right.push_back(decode(frame[2], frame[3]));
    }
    std::fclose(file);
    return true;
}

struct Level
{
    double peak { 0.0 };
    double rms { 0.0 };
};

inline double decibels(double ratio)
{
    return 20.0 * std::log10(ratio + 1.0e-18);
}

inline Level measure(const std::vector<float>& left,
                     const std::vector<float>& right)
{
    Level level;
    double sumOfSquares = 0.0;
    const std::size_t count = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const double l = left[index];
        const double r = right[index];
        level.peak = std::max({ level.peak, std::abs(l), std::abs(r) });
        sumOfSquares += l * l + r * r;
    }
    const auto samples = static_cast<double>(count) * 2.0;
    level.rms = samples > 0.0 ? std::sqrt(sumOfSquares / samples) : 0.0;
    return level;
}

// One engine driven through a short performance, with the rendered audio kept
// so it can be measured, level-matched and differenced.
class Take
{
public:
    explicit Take(EngineParameters parameters)
        : engine_(std::make_unique<YouKnow106Engine>())
    {
        engine_->prepare(sampleRate, renderBlockSize, true);
        engine_->setParameters(parameters);
    }

    Take(std::vector<float> left, std::vector<float> right)
        : engine_(nullptr), left_(std::move(left)), right_(std::move(right)) {}

    void on(int note, float velocity = 1.0f) { engine_->noteOn(note, velocity); }
    void off(int note) { engine_->noteOff(note); }

    // Moves a control mid-take. Sweeps are what expose the converter, the hold
    // capacitors and anything that quantises.
    void setParameters(const EngineParameters& parameters)
    {
        engine_->setParameters(parameters);
    }

    void hit(int note, float velocity, double holdSeconds, double gapSeconds)
    {
        on(note, velocity);
        rest(holdSeconds);
        off(note);
        rest(gapSeconds);
    }

    void chord(std::initializer_list<int> notes, float velocity,
               double holdSeconds, double gapSeconds)
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

    [[nodiscard]] Level measure() const
    {
        return youknow106::tools::measure(left_, right_);
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

    [[nodiscard]] Take diffWith(const Take& before) const
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

} // namespace youknow106::tools

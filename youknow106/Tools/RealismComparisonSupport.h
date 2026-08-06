// Strict support for build-to-build realism comparisons. Unlike the older
// listening renderers, this path keeps an unquantised, unnormalised archive and
// refuses malformed or non-finite audio rather than silently repairing it.
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace youknow106::tools::realism
{

inline constexpr std::uint32_t comparisonSampleRate = 48000u;
inline constexpr int comparisonBlockSize = 128;
inline constexpr double listeningTargetPeak = 0.5011872336272722; // -6 dBFS

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

struct Level
{
    double peak { 0.0 };
    double rms { 0.0 };
};

inline double decibels(double ratio) noexcept
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-18));
}

inline bool validate(const StereoBuffer& audio, std::string& error)
{
    if (audio.left.empty() || audio.left.size() != audio.right.size())
    {
        error = "audio must contain a non-empty, equal-length stereo pair";
        return false;
    }

    for (std::size_t frame = 0; frame < audio.left.size(); ++frame)
    {
        if (!std::isfinite(audio.left[frame]) || !std::isfinite(audio.right[frame]))
        {
            error = "non-finite audio at frame " + std::to_string(frame);
            return false;
        }
    }
    return true;
}

inline Level measure(const StereoBuffer& audio) noexcept
{
    Level result;
    long double sumSquares = 0.0;
    for (std::size_t frame = 0; frame < audio.left.size(); ++frame)
    {
        const double left = audio.left[frame];
        const double right = audio.right[frame];
        result.peak = std::max({ result.peak, std::abs(left), std::abs(right) });
        sumSquares += static_cast<long double>(left) * left;
        sumSquares += static_cast<long double>(right) * right;
    }

    const auto sampleCount = static_cast<long double>(audio.left.size()) * 2.0L;
    if (sampleCount > 0.0L)
        result.rms = std::sqrt(static_cast<double>(sumSquares / sampleCount));
    return result;
}

inline StereoBuffer applyGain(const StereoBuffer& input, double gain)
{
    StereoBuffer output = input;
    for (std::size_t frame = 0; frame < output.left.size(); ++frame)
    {
        output.left[frame] = static_cast<float>(output.left[frame] * gain);
        output.right[frame] = static_cast<float>(output.right[frame] * gain);
    }
    return output;
}

inline bool difference(const StereoBuffer& before, const StereoBuffer& after,
                       StereoBuffer& output, std::string& error)
{
    if (before.left.size() != after.left.size()
        || before.right.size() != after.right.size())
    {
        error = "before and after renders have different lengths";
        return false;
    }

    output.left.resize(before.left.size());
    output.right.resize(before.right.size());
    for (std::size_t frame = 0; frame < before.left.size(); ++frame)
    {
        output.left[frame] = after.left[frame] - before.left[frame];
        output.right[frame] = after.right[frame] - before.right[frame];
    }
    return validate(output, error);
}

inline void appendLittleEndian(std::vector<std::uint8_t>& bytes,
                               std::uint32_t value, int byteCount)
{
    for (int byte = 0; byte < byteCount; ++byte)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xffu));
}

inline std::uint16_t readU16(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint16_t>(bytes[0])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8u);
}

inline std::uint32_t readU32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8u)
         | (static_cast<std::uint32_t>(bytes[2]) << 16u)
         | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

inline bool writeFloatWav(const std::filesystem::path& path,
                          const StereoBuffer& audio, std::string& error)
{
    if (!validate(audio, error))
        return false;
    if (audio.left.size()
        > static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max() - 36u) / 8u))
    {
        error = "audio is too long for a RIFF/WAVE file";
        return false;
    }

    constexpr std::uint16_t channels = 2u;
    constexpr std::uint16_t bytesPerSample = 4u;
    const auto frames = static_cast<std::uint32_t>(audio.left.size());
    const std::uint32_t dataBytes = frames * channels * bytesPerSample;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44u + dataBytes);
    const auto tag = [&bytes](const char* value) {
        for (int index = 0; index < 4; ++index)
            bytes.push_back(static_cast<std::uint8_t>(value[index]));
    };

    tag("RIFF");
    appendLittleEndian(bytes, 36u + dataBytes, 4);
    tag("WAVE");
    tag("fmt ");
    appendLittleEndian(bytes, 16u, 4);
    appendLittleEndian(bytes, 3u, 2); // WAVE_FORMAT_IEEE_FLOAT
    appendLittleEndian(bytes, channels, 2);
    appendLittleEndian(bytes, comparisonSampleRate, 4);
    appendLittleEndian(bytes,
                       comparisonSampleRate * channels * bytesPerSample, 4);
    appendLittleEndian(bytes, channels * bytesPerSample, 2);
    appendLittleEndian(bytes, 32u, 2);
    tag("data");
    appendLittleEndian(bytes, dataBytes, 4);

    for (std::size_t frame = 0; frame < audio.left.size(); ++frame)
    {
        appendLittleEndian(bytes, std::bit_cast<std::uint32_t>(audio.left[frame]), 4);
        appendLittleEndian(bytes, std::bit_cast<std::uint32_t>(audio.right[frame]), 4);
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create output directory for " + path.string()
              + ": " + filesystemError.message();
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open " + path.string() + " for writing";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
    {
        error = "failed while writing " + path.string();
        return false;
    }
    return true;
}

inline bool readFloatWav(const std::filesystem::path& path,
                         StereoBuffer& audio, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "cannot open baseline " + path.string();
        return false;
    }

    std::array<std::uint8_t, 44> header {};
    stream.read(reinterpret_cast<char*>(header.data()),
                static_cast<std::streamsize>(header.size()));
    const auto hasTag = [&header](std::size_t offset, const char* value) {
        return header[offset] == static_cast<std::uint8_t>(value[0])
            && header[offset + 1] == static_cast<std::uint8_t>(value[1])
            && header[offset + 2] == static_cast<std::uint8_t>(value[2])
            && header[offset + 3] == static_cast<std::uint8_t>(value[3]);
    };
    if (!stream || !hasTag(0, "RIFF") || !hasTag(8, "WAVE")
        || !hasTag(12, "fmt ") || !hasTag(36, "data")
        || readU32(header.data() + 16) != 16u
        || readU16(header.data() + 20) != 3u
        || readU16(header.data() + 22) != 2u
        || readU32(header.data() + 24) != comparisonSampleRate
        || readU16(header.data() + 32) != 8u
        || readU16(header.data() + 34) != 32u)
    {
        error = "baseline is not the canonical 48 kHz stereo float32 WAV format";
        return false;
    }

    const std::uint32_t dataBytes = readU32(header.data() + 40);
    if (dataBytes == 0u || dataBytes % 8u != 0u
        || readU32(header.data() + 4) != 36u + dataBytes)
    {
        error = "baseline WAV has an invalid RIFF or data length";
        return false;
    }

    std::error_code filesystemError;
    const auto fileBytes = std::filesystem::file_size(path, filesystemError);
    if (filesystemError || fileBytes != 44u + dataBytes)
    {
        error = "baseline WAV file length does not match its header";
        return false;
    }

    const std::size_t frames = dataBytes / 8u;
    audio.left.resize(frames);
    audio.right.resize(frames);
    std::array<std::uint8_t, 8> frameBytes {};
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        stream.read(reinterpret_cast<char*>(frameBytes.data()),
                    static_cast<std::streamsize>(frameBytes.size()));
        if (!stream)
        {
            error = "baseline WAV ended before its declared data length";
            return false;
        }
        audio.left[frame] = std::bit_cast<float>(readU32(frameBytes.data()));
        audio.right[frame] = std::bit_cast<float>(readU32(frameBytes.data() + 4));
    }
    return validate(audio, error);
}

inline bool buffersEqual(const StereoBuffer& left,
                         const StereoBuffer& right) noexcept
{
    return left.left == right.left && left.right == right.right;
}

inline bool writeText(const std::filesystem::path& path,
                      const std::string& contents, std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create output directory for " + path.string()
              + ": " + filesystemError.message();
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open " + path.string() + " for writing";
        return false;
    }
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.close();
    if (!stream)
    {
        error = "failed while writing " + path.string();
        return false;
    }
    return true;
}

inline bool updateGeneratedSection(const std::filesystem::path& path,
                                   const std::string& defaultProse,
                                   const std::string& beginMarker,
                                   const std::string& endMarker,
                                   const std::string& generated,
                                   std::string& error)
{
    std::string contents;
    if (std::filesystem::exists(path))
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "cannot open " + path.string() + " for reading";
            return false;
        }
        std::ostringstream loaded;
        loaded << stream.rdbuf();
        if (!stream.good() && !stream.eof())
        {
            error = "failed while reading " + path.string();
            return false;
        }
        contents = loaded.str();
    }
    else
    {
        contents = defaultProse + "\n\n" + beginMarker + "\n" + endMarker + "\n";
    }

    const auto begin = contents.find(beginMarker);
    const auto end = contents.find(endMarker);
    if (begin == std::string::npos || end == std::string::npos || end < begin
        || contents.find(beginMarker, begin + beginMarker.size()) != std::string::npos
        || contents.find(endMarker, end + endMarker.size()) != std::string::npos)
    {
        error = "README generated markers are missing, duplicated, or out of order";
        return false;
    }

    const auto generatedBegin = begin + beginMarker.size();
    contents.replace(generatedBegin, end - generatedBegin,
                     "\n" + generated + "\n");
    return writeText(path, contents, error);
}

inline std::string jsonNumber(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

} // namespace youknow106::tools::realism

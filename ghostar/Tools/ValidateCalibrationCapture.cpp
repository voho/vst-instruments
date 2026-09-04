// GhostarValidateCalibrationCapture -- acquisition-inventory preflight for
// the first same-unit Crumar Spirit calibration campaign.
//
// WHY THIS EXISTS. Ghostar's remaining voiced seams cannot be closed by more
// schematic interpretation. They need measurements from one identified,
// healthy instrument, made on one clock and retained without processing. This
// tool freezes the minimum acquisition contract before those measurements are
// seen. It validates metadata, referenced artifacts and required condition
// coverage only: it does not inspect samples, recompute hashes, estimate a
// transfer function, fit coefficients or decide whether a measurement agrees
// with the model.
//
// SAFETY AND UNIT RECORD. Only a qualified person should probe a powered
// vintage instrument. Record serial number, PCB revisions, modifications or
// replaced parts, CEM/BC173 markings, warm-up, ambient temperature, measured
// +12/-12/+20 V rails before and after, and every generator, recorder, meter
// and probe in session_metadata.md. Record probe input resistance,
// capacitance, scale, polarity and relative channel latency. Start that record
// with --print-metadata-template, keep its keys and replace every <placeholder>.
// Extra Markdown notes are allowed. RS7 continuity is measured only with the
// instrument disconnected from mains and its rails discharged. Do not load a
// CEM3350 VLP/VBP pin directly: Curtis specifies a buffer input current below
// 10 nA, so use the existing TL082-buffered IC12 outputs or a suitable active
// probe.
//
// RAW DATA CONTRACT. Archive every native acquisition unchanged. Every
// accepted sampled take also has an interleaved little-endian float32 analysis
// export in channel order, scaled so 1.0 means 1 volt. That export may apply
// only the session's frozen gain, offset and polarity calibration: no trimming,
// resampling, per-channel alignment, filtering, denoising or normalisation.
// All sampled channels in a take share one clock. Hash native, analysis,
// stimulus and photograph artifacts outside this dependency-free inventory
// checker and enter lowercase SHA-256 digests. Native and analysis artifacts
// belong to one take only; split a longer session recording into per-take files
// because this inventory deliberately has no offset/range notation. A rejected
// take remains in the inventory with its reason and never satisfies coverage;
// its analysis_f32_path and analysis_f32_sha256 may both be '-'.
//
// REQUIRED CAPTURES AND FIXED CHANNEL ORDER.
//
// pulse, >=1 MS/s, >=2 s:
//   A Rect Wide/Mid/Narrow/Thin: IC8 pin 4 raw pulse; TP1 selected waveform.
//   B Rect Wide/Mid/Narrow/Thin: IC9 pin 4 raw pulse; TP2 selected waveform.
//   Hold the keyboard's second C at 32', tune and interval centred, with PWM
//   destinations off and both wheels at zero.
//
// shaper_vca, >=96 kHz/16 bit, >=30 s:
//   FREE and KBD HOLD, each with SHAPE X WITH Y off and on. Channels are
//   External Audio loopback; IC5 pin 2 Shaper-VCA output; J3/3 FREE; J3/5
//   SHAPE; IC5 pin 5 control; IC5 pin 13 MOD-X output. Feed a frozen low-level
//   997 Hz stimulus through External Audio with only the Shaper NOISE slider
//   open. Use centred SHAPE and a RATE slow enough to retain at least four
//   complete cycles. These simultaneous nodes separate the BC173 control law
//   from the CEM3360 gain law.
//
// cutoff, >=192 kHz/16 bit, >=5 s:
//   MASTER at 0/.25/.5/.75/1. Channels are External Audio loopback; IC12 pin
//   1; IC12 pin 7; IC14 pin 7 FILT OUT; P6 wiper. Use a frozen low-level sweep,
//   External Audio through the Filter NOISE slider, all other sources closed,
//   LOWER ONLY at panel 8, DYNAMIC, KB AMOUNT zero, envelope amount centred,
//   Upper LOW/12 dB, Lower OUT and VCA BYPASS. Fit only panel .5; the other
//   four positions are holdout checks of the resistor-derived travel.
//
// noise:
//   ADC short and powered-zero outputs: >=192 kHz/24 bit, >=60 s.
//   MM5837 clock: >=1 MS/s, >=3 s, IC3 output.
//   Full audio path: >=192 kHz/24 bit, >=60 s, IC3 output; IC4A output;
//   ADSR AUDIO OUT; SHAPED AUDIO OUT.
//   RED path: DC coupled, >=2 kHz/16 bit, >=300 s, R6/C8 junction; IC4B output.
//   Keep acquisition gain fixed and retain a same-gain shorted-input baseline.
//
// headroom, after RS7 continuity, >=192 kHz/16 bit, >=4 s:
//   OUT/BANDPASS/HIGHPASS/OVERDRIVE at 100 Hz, 1 kHz and 8 kHz. Channels are
//   External Audio loopback; IC12 pin 1; IC12 pin 7; IC14 pin 7 FILT OUT;
//   ADSR AUDIO OUT. Use one frozen ascending-then-descending amplitude
//   staircase from the small-signal region through first clipping. These
//   captures identify which physical stage limits; they must not be replaced
//   by fitting a clipper only at the final output.
//
// RS7 CONTINUITY. rs7-continuity.tsv contains all 48 common-to-throw readings:
// four detents x decks A/B/C x four physical throws. Deck A throws are 1..4,
// B are 5..8 and C are 9..12. Enter a finite resistance, >lower_bound, or OL.
// Photographs must preserve front-panel detent, switch rear and board/connector
// orientation. The validator checks complete, unambiguous inventory; it does
// not decide which measured contact is electrically closed.
//
// USING IT:
//   GhostarValidateCalibrationCapture --print-metadata-template > session_metadata.md
//   GhostarValidateCalibrationCapture --print-capture-header > captures.tsv
//   GhostarValidateCalibrationCapture --print-rs7-header > rs7-continuity.tsv
//   GhostarValidateCalibrationCapture --check /path/to/capture-directory
//   GhostarValidateCalibrationCapture --self-test

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;

constexpr std::array<std::string_view, 30> captureColumns {
    "session_id", "unit_id", "board_revision", "take_id", "experiment",
    "condition_id", "repeat", "accepted", "rejection_reason",
    "sample_rate_hz", "native_bit_depth", "duration_seconds", "clock_id",
    "unprocessed", "native_path", "native_sha256", "analysis_f32_path",
    "analysis_f32_sha256", "channel_count", "ch1", "ch2", "ch3", "ch4",
    "ch5", "ch6", "stimulus_path", "stimulus_sha256", "calibration_id",
    "panel_photo_path", "panel_photo_sha256"
};

constexpr std::array<std::string_view, 10> rs7Columns {
    "session_id", "unit_id", "detent", "deck", "common_lug", "throw_lug",
    "resistance_ohm", "meter_id", "photo_path", "photo_sha256"
};

struct MetadataField
{
    std::string_view key;
    std::string_view prompt;
};

constexpr std::array<MetadataField, 15> metadataFields {{
    { "session_id", "must match captures.tsv" },
    { "unit_id", "must match captures.tsv" },
    { "serial_number", "instrument serial number or a precise absent-marking note" },
    { "pcb_revisions", "all PCB identifiers and revisions" },
    { "modifications_or_replaced_parts", "none, or complete details" },
    { "cem_bc173_markings", "all CEM and BC173 package markings" },
    { "warm_up", "duration and operating state" },
    { "ambient_temperature", "value and unit" },
    { "rails_before", "measured +12/-12/+20 V rails" },
    { "rails_after", "measured +12/-12/+20 V rails" },
    { "generators", "every generator and its identifier" },
    { "recorders", "every recorder/clock and its identifier" },
    { "meters", "every meter and its identifier" },
    { "probes", "IDs, input R/C, scale, polarity and relative latency" },
    { "calibrations", "each calibration_id with frozen gain, offset and polarity" }
}};

using MetadataValues = std::array<std::string, metadataFields.size()>;
using ArtifactHashes = std::map<std::string, std::string>;

using Channels = std::array<std::string, 6>;

struct CaptureSpec
{
    std::string experiment;
    std::string condition;
    long long minimumSampleRate {};
    int minimumBitDepth {};
    double minimumDuration {};
    Channels channels;
    bool needsStimulus {};
};

struct TsvRow
{
    std::size_t line {};
    std::vector<std::string> fields;
};

Channels channels(std::initializer_list<const char*> values)
{
    Channels result;
    result.fill("-");
    std::size_t index = 0;
    for (const char* value : values)
        result[index++] = value;
    return result;
}

const std::vector<CaptureSpec>& requiredCaptures()
{
    static const std::vector<CaptureSpec> specs = [] {
        std::vector<CaptureSpec> result;
        const auto add = [&result](std::string experiment,
                                   std::string condition,
                                   long long sampleRate, int bits,
                                   double duration, Channels map,
                                   bool stimulus) {
            result.push_back({ std::move(experiment), std::move(condition),
                               sampleRate, bits, duration, std::move(map),
                               stimulus });
        };

        const Channels pulseA = channels({
            "p1014_ic8_pin4_raw_pulse", "p1014_tp1_selected_wave"
        });
        const Channels pulseB = channels({
            "p1014_ic9_pin4_raw_pulse", "p1014_tp2_selected_wave"
        });
        for (const char* width : { "wide", "mid", "narrow", "thin" })
        {
            add("pulse", std::string("pulse_a_rect_") + width,
                1000000, 8, 2.0, pulseA, false);
            add("pulse", std::string("pulse_b_rect_") + width,
                1000000, 8, 2.0, pulseB, false);
        }

        const Channels shaper = channels({
            "external_audio_loopback", "p1013_ic5_pin2_shaper_vca_out",
            "p1013_j3_3_free", "p1013_j3_5_shape",
            "p1013_ic5_pin5_control", "p1013_ic5_pin13_mod_x_out"
        });
        for (const char* condition : {
                 "shaper_free_x_off", "shaper_free_x_on",
                 "shaper_kbd_hold_x_off", "shaper_kbd_hold_x_on" })
            add("shaper_vca", condition, 96000, 16, 30.0, shaper, true);

        const Channels cutoff = channels({
            "external_audio_loopback", "p1013_ic12_pin1",
            "p1013_ic12_pin7", "p1013_ic14_pin7_filter_out",
            "p1013_p6_wiper"
        });
        for (const char* position : { "000", "025", "050", "075", "100" })
            add("cutoff", std::string("cutoff_master_") + position,
                192000, 16, 5.0, cutoff, true);

        add("noise", "noise_adc_short", 192000, 24, 60.0,
            channels({ "adc_short" }), false);
        add("noise", "noise_powered_zero", 192000, 24, 60.0,
            channels({ "p1017_adsr_audio_out",
                       "p1017_shaped_audio_out" }), false);
        add("noise", "noise_mm5837_clock", 1000000, 8, 3.0,
            channels({ "p1013_ic3_mm5837_out" }), false);
        add("noise", "noise_audio_full", 192000, 24, 60.0,
            channels({ "p1013_ic3_mm5837_out",
                       "p1013_ic4a_audio_noise_out",
                       "p1017_adsr_audio_out",
                       "p1017_shaped_audio_out" }), false);
        add("noise", "noise_red", 2000, 16, 300.0,
            channels({ "p1013_r6_c8_junction",
                       "p1013_ic4b_red_noise_out" }), false);

        const Channels headroom = channels({
            "external_audio_loopback", "p1013_ic12_pin1",
            "p1013_ic12_pin7", "p1013_ic14_pin7_filter_out",
            "p1017_adsr_audio_out"
        });
        for (const char* mode : { "out", "bandpass", "highpass",
                                  "overdrive" })
            for (const int hz : { 100, 1000, 8000 })
                add("headroom", std::string("headroom_") + mode + "_"
                                    + std::to_string(hz) + "hz",
                    192000, 16, 4.0, headroom, true);
        return result;
    }();
    return specs;
}

template <std::size_t Size>
std::string headerLine(const std::array<std::string_view, Size>& columns)
{
    std::string result;
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        if (index != 0)
            result += '\t';
        result += columns[index];
    }
    return result;
}

void printMetadataTemplate(std::ostream& output)
{
    for (const auto& field : metadataFields)
        output << field.key << ": <" << field.prompt << ">\n";
}

std::vector<std::string> splitTabs(const std::string& line)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    for (;;)
    {
        const auto tab = line.find('\t', start);
        result.push_back(line.substr(start, tab - start));
        if (tab == std::string::npos)
            return result;
        start = tab + 1;
    }
}

template <std::size_t Size>
std::vector<TsvRow> readTsv(const fs::path& path,
                            const std::array<std::string_view, Size>& columns,
                            std::vector<std::string>& errors)
{
    std::ifstream input(path);
    if (!input)
    {
        errors.push_back("cannot open " + path.string());
        return {};
    }

    std::string line;
    if (!std::getline(input, line))
    {
        errors.push_back(path.string() + ": empty file");
        return {};
    }
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != headerLine(columns))
    {
        errors.push_back(path.string() + ": header differs from --print-*-header");
        return {};
    }

    std::vector<TsvRow> rows;
    std::size_t lineNumber = 1;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
        {
            errors.push_back(path.string() + ":" + std::to_string(lineNumber)
                             + ": blank rows are not allowed");
            continue;
        }
        auto fields = splitTabs(line);
        if (fields.size() != columns.size())
        {
            errors.push_back(path.string() + ":" + std::to_string(lineNumber)
                             + ": expected " + std::to_string(columns.size())
                             + " fields, got " + std::to_string(fields.size()));
            continue;
        }
        if (std::any_of(fields.begin(), fields.end(),
                        [](const std::string& field) { return field.empty(); }))
        {
            errors.push_back(path.string() + ":" + std::to_string(lineNumber)
                             + ": use '-' rather than an empty field");
            continue;
        }
        rows.push_back({ lineNumber, std::move(fields) });
    }
    if (input.bad())
        errors.push_back(path.string() + ": read failed");
    if (rows.empty())
        errors.push_back(path.string() + ": no data rows");
    return rows;
}

bool isIdentifier(const std::string& value)
{
    if (value.empty() || value.size() > 96
        || !std::isalnum(static_cast<unsigned char>(value.front())))
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_'
            || character == '.';
    });
}

std::string trim(std::string_view text)
{
    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return std::string(text);
}

MetadataValues readMetadata(const fs::path& path,
                            std::vector<std::string>& errors)
{
    MetadataValues values;
    std::array<bool, metadataFields.size()> seen {};
    std::ifstream input(path);
    if (!input)
    {
        errors.push_back("cannot open " + path.string());
        return values;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned.front() == '#')
            continue;
        const auto colon = cleaned.find(':');
        if (colon == std::string::npos)
            continue;
        const std::string key = trim(std::string_view(cleaned).substr(0, colon));
        const auto found = std::find_if(metadataFields.begin(), metadataFields.end(),
                                        [&](const MetadataField& field) {
                                            return field.key == key;
                                        });
        if (found == metadataFields.end())
            continue;

        const auto index = static_cast<std::size_t>(found - metadataFields.begin());
        const std::string label = path.string() + ":"
                                + std::to_string(lineNumber);
        if (seen[index])
        {
            errors.push_back(label + ": duplicate metadata key " + key);
            continue;
        }
        seen[index] = true;
        values[index] = trim(std::string_view(cleaned).substr(colon + 1));
        const auto& value = values[index];
        if (value.empty() || value == "-"
            || (value.front() == '<' && value.back() == '>'))
            errors.push_back(label + ": replace the placeholder for " + key);
    }
    if (input.bad())
        errors.push_back(path.string() + ": read failed");

    for (std::size_t index = 0; index < metadataFields.size(); ++index)
        if (!seen[index])
            errors.push_back(path.string() + ": missing metadata key "
                             + std::string(metadataFields[index].key));
    return values;
}

bool isSha256(const std::string& value)
{
    if (value.size() != 64 || value == std::string(64, '0'))
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character) || (character >= 'a' && character <= 'f');
    });
}

template <typename Number>
bool parseNumber(const std::string& text, Number& value)
{
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    if (!(stream >> value))
        return false;
    char extra = 0;
    if (stream >> extra)
        return false;
    if constexpr (std::is_floating_point_v<Number>)
        return std::isfinite(value);
    return true;
}

bool safeRelativePath(const std::string& value)
{
    if (value.empty() || value == "-" || value.find('\\') != std::string::npos
        || value.find('\0') != std::string::npos)
        return false;
    const fs::path path(value);
    if (path.is_absolute() || path.has_root_name())
        return false;
    for (const auto& part : path)
        if (part == "." || part == ".." || part.empty())
            return false;
    return true;
}

bool isInside(const fs::path& root, const fs::path& path)
{
    const fs::path relative = path.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute()
        && *relative.begin() != "..";
}

struct ArtifactInfo
{
    std::uintmax_t size {};
    std::string identity;
};

ArtifactInfo validateArtifact(const fs::path& root,
                              const std::string& relative,
                              const std::string& hash,
                              const std::string& label,
                              ArtifactHashes& artifactHashes,
                              std::vector<std::string>& errors,
                              bool optional = false)
{
    if (optional && relative == "-" && hash == "-")
        return {};
    if (!safeRelativePath(relative))
    {
        errors.push_back(label + ": unsafe or missing relative path");
        return {};
    }
    const bool validHash = isSha256(hash);
    if (!validHash)
        errors.push_back(label + ": SHA-256 must be 64 lowercase hex digits and nonzero");

    std::error_code error;
    const fs::path resolved = fs::canonical(root / relative, error);
    if (error || !isInside(root, resolved) || !fs::is_regular_file(resolved, error))
    {
        errors.push_back(label
                         + ": referenced file is missing, not regular, or outside the package");
        return {};
    }
    const auto size = fs::file_size(resolved, error);
    if (error || size == 0)
    {
        errors.push_back(label + ": referenced file is empty or unreadable");
        return {};
    }

    const std::string identity = resolved.generic_string();
    if (validHash)
    {
        const auto [entry, inserted] = artifactHashes.emplace(identity, hash);
        if (!inserted && entry->second != hash)
            errors.push_back(label + ": the same file has conflicting SHA-256 values");
    }
    return { size, identity };
}

const CaptureSpec* findCaptureSpec(const std::vector<CaptureSpec>& specs,
                                   const std::string& experiment,
                                   const std::string& condition)
{
    const auto found = std::find_if(specs.begin(), specs.end(),
                                    [&](const CaptureSpec& spec) {
        return spec.experiment == experiment && spec.condition == condition;
    });
    return found == specs.end() ? nullptr : &*found;
}

std::string captureKey(const std::string& experiment,
                       const std::string& condition)
{
    return experiment + '\x1f' + condition;
}

void validateCaptures(const fs::path& root, const std::vector<TsvRow>& rows,
                      const std::vector<CaptureSpec>& specs,
                      std::string& session, std::string& unit,
                      ArtifactHashes& artifactHashes,
                      std::vector<std::string>& errors)
{
    std::set<std::string> takeIds;
    std::set<std::string> repetitions;
    std::set<std::string> acceptedCoverage;
    std::map<std::string, std::string> acquisitionOwners;
    std::string board;

    for (const auto& row : rows)
    {
        const auto label = std::string("captures.tsv:")
                         + std::to_string(row.line);
        const auto& field = row.fields;
        for (const int index : { 0, 1, 2, 3, 12, 27 })
            if (!isIdentifier(field[static_cast<std::size_t>(index)]))
                errors.push_back(label + ": invalid identifier in column "
                                 + std::string(captureColumns[static_cast<std::size_t>(index)]));

        if (session.empty())
        {
            session = field[0];
            unit = field[1];
            board = field[2];
        }
        else if (field[0] != session || field[1] != unit || field[2] != board)
            errors.push_back(label + ": session, unit and board must stay constant");

        if (!takeIds.insert(field[3]).second)
            errors.push_back(label + ": duplicate take_id " + field[3]);

        const CaptureSpec* spec = findCaptureSpec(specs, field[4], field[5]);
        if (spec == nullptr)
        {
            errors.push_back(label + ": unknown experiment/condition "
                             + field[4] + "/" + field[5]);
            continue;
        }

        int repeat = 0;
        int accepted = 0;
        long long sampleRate = 0;
        int bitDepth = 0;
        double duration = 0.0;
        int unprocessed = 0;
        int channelCount = 0;
        if (!parseNumber(field[6], repeat) || repeat < 1)
            errors.push_back(label + ": repeat must be a positive integer");
        if (!parseNumber(field[7], accepted) || (accepted != 0 && accepted != 1))
            errors.push_back(label + ": accepted must be 0 or 1");
        if ((accepted == 1 && field[8] != "-")
            || (accepted == 0 && field[8] == "-"))
            errors.push_back(label + ": rejection_reason must be '-' only for accepted takes");
        if (!parseNumber(field[9], sampleRate) || sampleRate <= 0)
            errors.push_back(label + ": sample_rate_hz must be positive");
        if (!parseNumber(field[10], bitDepth) || bitDepth <= 0)
            errors.push_back(label + ": native_bit_depth must be positive");
        if (!parseNumber(field[11], duration) || duration <= 0.0)
            errors.push_back(label + ": duration_seconds must be positive and finite");
        if (!parseNumber(field[13], unprocessed) || unprocessed != 1)
            errors.push_back(label + ": unprocessed must be 1");
        if (!parseNumber(field[18], channelCount) || channelCount < 1
            || channelCount > 6)
            errors.push_back(label + ": channel_count must be 1..6");

        const std::string repetitionKey = captureKey(field[4], field[5])
                                        + '\x1f' + field[6];
        if (!repetitions.insert(repetitionKey).second)
            errors.push_back(label + ": duplicate condition/repeat");

        int namedChannels = 0;
        for (std::size_t index = 0; index < spec->channels.size(); ++index)
        {
            const auto& actual = field[19 + index];
            if (actual != "-")
                ++namedChannels;
            if (actual != spec->channels[index])
                errors.push_back(label + ": channel map differs at ch"
                                 + std::to_string(index + 1));
        }
        if (channelCount != namedChannels)
            errors.push_back(label + ": channel_count differs from channel map");

        const auto native = validateArtifact(root, field[14], field[15],
                                             label + " native",
                                             artifactHashes, errors);
        const auto analysis = validateArtifact(root, field[16], field[17],
                                               label + " analysis",
                                               artifactHashes, errors,
                                               accepted == 0);
        if (field[16] != "-" && fs::path(field[16]).extension() != ".f32")
            errors.push_back(label + ": analysis_f32_path must end in .f32");
        validateArtifact(root, field[25], field[26], label + " stimulus",
                         artifactHashes, errors, !spec->needsStimulus);
        if (spec->needsStimulus && (field[25] == "-" || field[26] == "-"))
            errors.push_back(label + ": this condition requires a frozen stimulus");
        if (!spec->needsStimulus && (field[25] != "-" || field[26] != "-"))
            errors.push_back(label + ": this condition must use '-' for stimulus fields");
        validateArtifact(root, field[28], field[29], label + " panel photo",
                         artifactHashes, errors);

        const auto claimAcquisitionArtifact = [&](const ArtifactInfo& artifact,
                                                   const std::string& digest) {
            if (artifact.identity.empty())
                return;
            for (const auto& key : {
                     artifact.identity,
                     isSha256(digest) ? "sha256:" + digest : std::string {}
                 })
            {
                if (key.empty())
                    continue;
                const auto [owner, inserted] = acquisitionOwners.emplace(
                    key, field[3]);
                if (!inserted && owner->second != field[3])
                    errors.push_back(
                        label
                        + ": native/analysis artifact is reused by take_id "
                        + owner->second);
            }
        };
        claimAcquisitionArtifact(native, field[15]);
        claimAcquisitionArtifact(analysis, field[17]);

        if (accepted == 1)
        {
            if (sampleRate < spec->minimumSampleRate)
                errors.push_back(label + ": accepted take is below the required sample rate");
            if (bitDepth < spec->minimumBitDepth)
                errors.push_back(label + ": accepted take is below the required bit depth");
            if (duration < spec->minimumDuration)
                errors.push_back(label + ": accepted take is shorter than required");

            if (!analysis.identity.empty() && sampleRate > 0 && duration > 0.0
                && channelCount >= 1 && channelCount <= 6)
            {
                const auto bytesPerFrame = static_cast<std::uintmax_t>(
                    4 * channelCount);
                if (analysis.size % bytesPerFrame != 0)
                    errors.push_back(
                        label
                        + ": analysis file does not contain whole interleaved float32 frames");
                const auto availableFrames = analysis.size / bytesPerFrame;
                const long double declaredFrames =
                    static_cast<long double>(sampleRate)
                    * static_cast<long double>(duration);
                if (static_cast<long double>(availableFrames) + 1.0e-6L
                    < declaredFrames)
                    errors.push_back(
                        label
                        + ": analysis file is shorter than its declared rate and duration");
            }
            acceptedCoverage.insert(captureKey(field[4], field[5]));
        }
    }

    for (const auto& spec : specs)
        if (!acceptedCoverage.contains(captureKey(spec.experiment, spec.condition)))
            errors.push_back("captures.tsv: missing accepted condition "
                             + spec.experiment + "/" + spec.condition);
}

bool validResistance(const std::string& text)
{
    if (text == "OL")
        return true;
    const std::string number = !text.empty() && text.front() == '>'
        ? text.substr(1) : text;
    double value = 0.0;
    return parseNumber(number, value) && value >= 0.0;
}

void validateRs7(const fs::path& root, const std::vector<TsvRow>& rows,
                 const std::string& session, const std::string& unit,
                 ArtifactHashes& artifactHashes,
                 std::vector<std::string>& errors)
{
    const std::array<std::string, 4> detents {
        "out", "overdrive", "bandpass", "highpass"
    };
    const std::array<std::string, 3> decks { "A", "B", "C" };
    std::set<std::string> readings;

    for (const auto& row : rows)
    {
        const auto label = std::string("rs7-continuity.tsv:")
                         + std::to_string(row.line);
        const auto& field = row.fields;
        if (field[0] != session || field[1] != unit)
            errors.push_back(label + ": session/unit differs from captures.tsv");
        if (std::find(detents.begin(), detents.end(), field[2]) == detents.end())
            errors.push_back(label + ": unknown detent");
        const auto deck = std::find(decks.begin(), decks.end(), field[3]);
        if (deck == decks.end())
        {
            errors.push_back(label + ": deck must be A, B or C");
            continue;
        }
        if (field[4] != field[3])
            errors.push_back(label + ": common_lug must equal the deck letter");

        int throwLug = 0;
        if (!parseNumber(field[5], throwLug))
            errors.push_back(label + ": throw_lug must be an integer");
        else
        {
            const int deckIndex = static_cast<int>(deck - decks.begin());
            const int first = 1 + 4 * deckIndex;
            if (throwLug < first || throwLug > first + 3)
                errors.push_back(label + ": throw_lug does not belong to its deck");
        }
        if (!validResistance(field[6]))
            errors.push_back(label + ": resistance_ohm must be finite, >bound, or OL");
        if (!isIdentifier(field[7]))
            errors.push_back(label + ": invalid meter_id");
        validateArtifact(root, field[8], field[9], label + " photo",
                         artifactHashes, errors);

        const std::string key = field[2] + '\x1f' + field[3] + '\x1f' + field[5];
        if (!readings.insert(key).second)
            errors.push_back(label + ": duplicate detent/deck/throw reading");
    }

    for (const auto& detent : detents)
        for (std::size_t deck = 0; deck < decks.size(); ++deck)
            for (int offset = 0; offset < 4; ++offset)
            {
                const auto lug = std::to_string(1 + 4 * static_cast<int>(deck)
                                                + offset);
                const std::string key = detent + '\x1f' + decks[deck]
                                      + '\x1f' + lug;
                if (!readings.contains(key))
                    errors.push_back("rs7-continuity.tsv: missing reading "
                                     + detent + "/" + decks[deck] + "/" + lug);
            }
}

bool validatePackage(const fs::path& suppliedRoot,
                     const std::vector<CaptureSpec>& specs,
                     std::vector<std::string>& errors)
{
    std::error_code error;
    const fs::path root = fs::canonical(suppliedRoot, error);
    if (error || !fs::is_directory(root, error))
    {
        errors.push_back("capture directory is missing or not a directory");
        return false;
    }

    const auto metadata = readMetadata(root / "session_metadata.md", errors);
    const auto captures = readTsv(root / "captures.tsv", captureColumns, errors);
    const auto rs7 = readTsv(root / "rs7-continuity.tsv", rs7Columns, errors);
    std::string session;
    std::string unit;
    ArtifactHashes artifactHashes;
    validateCaptures(root, captures, specs, session, unit, artifactHashes,
                     errors);
    validateRs7(root, rs7, session, unit, artifactHashes, errors);
    if (!session.empty() && !metadata[0].empty() && metadata[0] != session)
        errors.push_back("session_metadata.md: session_id differs from captures.tsv");
    if (!unit.empty() && !metadata[1].empty() && metadata[1] != unit)
        errors.push_back("session_metadata.md: unit_id differs from captures.tsv");
    return errors.empty();
}

void writeFields(std::ostream& output, const std::vector<std::string>& fields)
{
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index != 0)
            output << '\t';
        output << fields[index];
    }
    output << '\n';
}

int channelCountFor(const CaptureSpec& spec)
{
    return static_cast<int>(std::count_if(
        spec.channels.begin(), spec.channels.end(),
        [](const std::string& channel) { return channel != "-"; }));
}

std::string selfTestNativePath(std::size_t index)
{
    return "native-" + std::to_string(index) + ".bin";
}

std::string selfTestAnalysisPath(std::size_t index)
{
    return "analysis-" + std::to_string(index) + ".f32";
}

std::string selfTestSha(std::size_t index, std::size_t salt)
{
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result(64, '0');
    result.front() = hex[(salt + 1) % hex.size()];
    for (std::size_t digit = 0; digit < 15 && index != 0; ++digit)
    {
        result[result.size() - 1 - digit] = hex[index & 0x0fu];
        index >>= 4u;
    }
    return result;
}

std::uintmax_t selfTestAnalysisSize(const CaptureSpec& spec)
{
    const auto frames = static_cast<std::uintmax_t>(std::ceil(
        static_cast<long double>(spec.minimumSampleRate)
        * static_cast<long double>(spec.minimumDuration)));
    return frames * static_cast<std::uintmax_t>(4 * channelCountFor(spec));
}

bool writeSelfTestCaptures(const fs::path& root,
                           const std::vector<CaptureSpec>& specs,
                           bool complete,
                           bool reuseArtifacts = false)
{
    std::ofstream output(root / "captures.tsv");
    if (!output)
        return false;
    output << headerLine(captureColumns) << '\n';
    const std::size_t count = complete ? specs.size() : specs.size() - 1;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto& spec = specs[index];
        const std::size_t artifactIndex = reuseArtifacts && index == 1
            ? 0 : index;
        std::vector<std::string> fields {
            "session-1", "unit-1", "rev-a", "take-" + std::to_string(index),
            spec.experiment, spec.condition, "1", "1", "-",
            std::to_string(spec.minimumSampleRate),
            std::to_string(spec.minimumBitDepth),
            std::to_string(spec.minimumDuration), "clock-1", "1",
            selfTestNativePath(artifactIndex), selfTestSha(artifactIndex, 0),
            selfTestAnalysisPath(artifactIndex), selfTestSha(artifactIndex, 1),
            std::to_string(channelCountFor(spec))
        };
        fields.insert(fields.end(), spec.channels.begin(), spec.channels.end());
        fields.push_back(spec.needsStimulus ? "stimulus.f32" : "-");
        fields.push_back(spec.needsStimulus ? std::string(64, '3') : "-");
        fields.push_back("calibration-1");
        fields.push_back("panel.jpg");
        fields.push_back(std::string(64, '4'));
        writeFields(output, fields);
    }

    const auto& rejectedSpec = specs.front();
    std::vector<std::string> rejected {
        "session-1", "unit-1", "rev-a", "take-rejected",
        rejectedSpec.experiment, rejectedSpec.condition, "2", "0",
        "scope-overload", std::to_string(rejectedSpec.minimumSampleRate),
        std::to_string(rejectedSpec.minimumBitDepth),
        std::to_string(rejectedSpec.minimumDuration), "clock-1", "1",
        "native-rejected.bin", std::string(64, 'e'), "-", "-",
        std::to_string(channelCountFor(rejectedSpec))
    };
    rejected.insert(rejected.end(), rejectedSpec.channels.begin(),
                    rejectedSpec.channels.end());
    rejected.push_back("-");
    rejected.push_back("-");
    rejected.push_back("calibration-1");
    rejected.push_back("panel.jpg");
    rejected.push_back(std::string(64, '4'));
    writeFields(output, rejected);
    return static_cast<bool>(output);
}

bool writeSelfTestRs7(const fs::path& root)
{
    std::ofstream output(root / "rs7-continuity.tsv");
    if (!output)
        return false;
    output << headerLine(rs7Columns) << '\n';
    const std::array<std::string, 4> detents {
        "out", "overdrive", "bandpass", "highpass"
    };
    const std::array<std::string, 3> decks { "A", "B", "C" };
    for (std::size_t position = 0; position < detents.size(); ++position)
        for (std::size_t deck = 0; deck < decks.size(); ++deck)
            for (int offset = 0; offset < 4; ++offset)
            {
                const int lug = 1 + 4 * static_cast<int>(deck) + offset;
                writeFields(output, {
                    "session-1", "unit-1", detents[position], decks[deck],
                    decks[deck], std::to_string(lug),
                    offset == static_cast<int>(position) ? "0.4" : "OL",
                    "meter-1", "rs7.jpg", std::string(64, '5')
                });
            }
    return static_cast<bool>(output);
}

bool writeByte(const fs::path& path)
{
    std::ofstream output(path, std::ios::binary);
    output.put('x');
    return static_cast<bool>(output);
}

bool writeSelfTestArtifacts(const fs::path& root,
                            const std::vector<CaptureSpec>& specs)
{
    std::error_code error;
    for (std::size_t index = 0; index < specs.size(); ++index)
    {
        if (!writeByte(root / selfTestNativePath(index))
            || !writeByte(root / selfTestAnalysisPath(index)))
            return false;
        fs::resize_file(root / selfTestAnalysisPath(index),
                        selfTestAnalysisSize(specs[index]), error);
        if (error)
            return false;
    }
    return writeByte(root / "native-rejected.bin")
        && writeByte(root / "stimulus.f32")
        && writeByte(root / "panel.jpg")
        && writeByte(root / "rs7.jpg");
}

bool writeSelfTestMetadata(const fs::path& root, bool placeholders = false)
{
    std::ofstream output(root / "session_metadata.md");
    if (!output)
        return false;
    if (placeholders)
    {
        printMetadataTemplate(output);
        return static_cast<bool>(output);
    }

    for (std::size_t index = 0; index < metadataFields.size(); ++index)
    {
        output << metadataFields[index].key << ": ";
        if (index == 0)
            output << "session-1";
        else if (index == 1)
            output << "unit-1";
        else
            output << "self-test-recorded";
        output << '\n';
    }
    return static_cast<bool>(output);
}

bool hasError(const std::vector<std::string>& errors,
              std::string_view fragment)
{
    return std::any_of(errors.begin(), errors.end(),
                       [&](const std::string& error) {
                           return error.find(fragment) != std::string::npos;
                       });
}

int selfTest()
{
    // Preserve the complete production inventory and every validation path,
    // but scale the fixture's rates and durations to one frame. Creating the
    // real minimum corpus here would expand roughly 1 GiB and makes a simple
    // logic test depend on sparse-file support.
    auto selfTestSpecs = requiredCaptures();
    for (auto& spec : selfTestSpecs)
    {
        spec.minimumSampleRate = 100;
        spec.minimumDuration = 0.01;
    }

    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch().count();
    fs::path root;
    std::error_code error;
    const fs::path temporaryRoot = fs::temp_directory_path(error);
    if (error)
    {
        std::cerr << "self-test: cannot locate temporary directory\n";
        return 1;
    }
    for (int attempt = 0; attempt < 16 && root.empty(); ++attempt)
    {
        const fs::path candidate = temporaryRoot
            / ("ghostar-capture-self-test-" + std::to_string(stamp)
               + "-" + std::to_string(attempt));
        if (fs::create_directory(candidate, error))
            root = candidate;
        error.clear();
    }
    if (root.empty())
    {
        std::cerr << "self-test: cannot create temporary directory\n";
        return 1;
    }
    struct Cleanup
    {
        fs::path path;
        ~Cleanup()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup { root };

    if (!writeSelfTestMetadata(root)
        || !writeSelfTestArtifacts(root, selfTestSpecs)
        || !writeSelfTestCaptures(root, selfTestSpecs, true)
        || !writeSelfTestRs7(root))
    {
        std::cerr << "self-test: cannot write fixture\n";
        return 1;
    }

    std::vector<std::string> errors;
    if (!validatePackage(root, selfTestSpecs, errors))
    {
        std::cerr << "self-test: valid fixture was rejected\n";
        for (const auto& message : errors)
            std::cerr << "  " << message << '\n';
        return 1;
    }

    const auto expectRejected = [&](std::string_view expected,
                                    std::string_view failure) {
        errors.clear();
        if (!validatePackage(root, selfTestSpecs, errors)
            && hasError(errors, expected))
            return true;
        std::cerr << "self-test: " << failure << '\n';
        for (const auto& message : errors)
            std::cerr << "  " << message << '\n';
        return false;
    };

    if (!writeSelfTestCaptures(root, selfTestSpecs, false))
    {
        std::cerr << "self-test: cannot write negative fixture\n";
        return 1;
    }
    if (!expectRejected("missing accepted condition",
                        "incomplete coverage was not rejected as expected"))
        return 1;

    if (!writeSelfTestCaptures(root, selfTestSpecs, true))
    {
        std::cerr << "self-test: cannot restore complete fixture\n";
        return 1;
    }
    error.clear();
    fs::resize_file(root / selfTestAnalysisPath(0), 1, error);
    if (error
        || !expectRejected("whole interleaved float32 frames",
                           "truncated analysis was not rejected as expected"))
        return 1;

    error.clear();
    fs::resize_file(root / selfTestAnalysisPath(0),
                    selfTestAnalysisSize(selfTestSpecs.front()), error);
    if (error || !writeSelfTestCaptures(root, selfTestSpecs, true, true))
    {
        std::cerr << "self-test: cannot write artifact-reuse fixture\n";
        return 1;
    }
    if (!expectRejected("reused by take_id",
                        "artifact reuse was not rejected as expected"))
        return 1;

    if (!writeSelfTestCaptures(root, selfTestSpecs, true)
        || !writeSelfTestMetadata(root, true))
    {
        std::cerr << "self-test: cannot write metadata-negative fixture\n";
        return 1;
    }
    if (!expectRejected("replace the placeholder",
                        "metadata placeholders were not rejected as expected"))
        return 1;

    std::cout << "Ghostar calibration-capture validator self-test passed\n";
    return 0;
}

void printUsage(const char* executable)
{
    std::cerr << "Usage:\n"
              << "  " << executable << " --print-metadata-template\n"
              << "  " << executable << " --print-capture-header\n"
              << "  " << executable << " --print-rs7-header\n"
              << "  " << executable << " --check <capture-directory>\n"
              << "  " << executable << " --self-test\n";
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--print-metadata-template")
    {
        printMetadataTemplate(std::cout);
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--print-capture-header")
    {
        std::cout << headerLine(captureColumns) << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--print-rs7-header")
    {
        std::cout << headerLine(rs7Columns) << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--self-test")
        return selfTest();
    if (argc == 3 && std::string_view(argv[1]) == "--check")
    {
        std::vector<std::string> errors;
        if (validatePackage(argv[2], requiredCaptures(), errors))
        {
            std::cout << "Ghostar calibration-capture inventory is complete\n";
            return 0;
        }
        for (const auto& message : errors)
            std::cerr << "error: " << message << '\n';
        return 1;
    }

    printUsage(argv[0]);
    return 2;
}

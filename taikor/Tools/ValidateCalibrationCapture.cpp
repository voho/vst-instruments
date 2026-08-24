// TaikorValidateCalibrationCapture -- the acquisition inventory preflight for
// Taikor's controlled calibration capture.
//
// WHY THIS EXISTS. Taikor has no owned real-taiko reference set. Several of
// the gaps listed under the README's "Known gaps" -- absolute contact
// stiffness, soft-hit continuum transfer, rear-head diffraction, the complete
// air-loading coordinate migration -- are gated on one, and writing a sample
// parser or a residual model before the first capture would only turn
// assumptions into code. This tool therefore checks acquisition *facts* and
// minimum coverage. It does not open audio, interpret samples, estimate
// transfers or fit a model.
//
// THE MEASUREMENT CONTRACT. It is a recording protocol, not a curve-fitting
// recipe. Two experiments are required and must stay separate:
//   A. low-amplitude linear identification of the drum's mechanical mobility
//      and force-to-pressure transfer;
//   B. full-level bachi strikes for nonlinear contact, repetition and
//      perceptual validation.
// Ordinary audio alone cannot separate contact mobility, radiation, room sound
// and microphone colour.
//
// Record from one synchronized clock without processing:
//   Force             modal-hammer or instrumented-bachi normal force  [N]
//   Contact traction  spatial pressure/traction map in the head frame
//                     (spatial scale, force/pressure gain, polarity, latency)
//   Head              normal velocity per observation coordinate, by LDV [m/s]
//   Bachi             axial position or velocity before impact through rebound
//   Near / Far        simultaneous front-head sound pressure at known xyz [Pa]
//   Trigger           common acquisition trigger if not embedded
//
// Positive force and velocity are motion from the batter head into the drum.
// Compensate every sensor's gain, polarity, phase and latency before
// estimating a transfer function, and record the instrumented bachi's added
// sensor mass. A point LDV reading only approximates the traction-weighted
// velocity of a finite contact patch: validate it with a small spatial scan or
// export the measured area average the fit used.
//
// Put probe microphones nominally 0.03 m and 0.40 m above the batter head to
// span Taikor's Mic Distance control, out of the striker and LDV paths so no
// capsule shadows another, and record exact coordinates and orientation rather
// than assuming the nominal distances. Rear-head velocity and cavity pressure
// are the first optional channels for a later reciprocal rear-head model; a
// rear microphone pair comes after those state measurements.
//
// Capture at one common rate of at least 96 kHz with calibrated sensor
// bandwidth and anti-aliasing, retaining at least 24-bit native resolution (a
// 32-bit float export is fine but adds no converter resolution). Keep at least
// 250 ms of pre-trigger noise and record until every band-limited decay
// reaches the measured noise floor: at least 4 s for the first fixture, at
// least 12 s for an o-daiko. Keep gains fixed with headroom for the hardest
// strike. Use an anechoic or sufficiently large damped space, or define a
// direct-sound window before the first reflection; archive the room sound and
// a room impulse response, but do not fit room modes into the drum's poles. Do
// not normalise, gate, denoise, compress, equalise, align channels separately,
// or discard the native acquisition files.
//
// USING IT. The first line of the TSV must contain all 48 columns below
// exactly once; extra columns are allowed. One row per take and measured cell,
// so experiment A repeats a physical take_id across its simultaneously
// observed coordinates while experiment B uses one unique take_id per strike.
// Use "-" for fields that do not apply. Paths are archive references or
// channel selectors, not files this tool opens.
//
//   build-dsp/TaikorValidateCalibrationCapture --print-header > captures.tsv
//   build-dsp/TaikorValidateCalibrationCapture --check captures.tsv
//
// Change fixture_state_id whenever either head, tension, shell mounting or
// stand state changes. Every row must be at least 96 kHz / 24 bit, carry
// 0.25 s of pre-trigger, be unprocessed, and last at least 4 s (12 s for a
// canonical odaiko); one session/drum/fixture-state group retains one clock,
// rate and native depth. Experiment A needs a complete 3 input x 3 observation
// x 2 level mobility core -- a centre coordinate at radius <= 0.05 and two
// edge coordinates at radius >= 0.70 with distinct azimuths, on both sides of
// the matrix -- with 10 distinct accepted takes per cell. Experiment B needs
// the canonical articulations (don, ka, tsu-held, don-rim) against hard and
// soft bachi at the tabulated nominal radii and *measured* incoming speeds,
// again 10 distinct takes per condition, plus stable bachi identity, positive
// bare mass, non-negative moving sensor mass and raw/calibrated tip
// profilometry (a reworked tip gets a new ID) -- which is what separates tip
// curvature from contact stiffness.
//
// Checking the referenced files and the sample data inside them is
// deliberately left to the post-capture analyzer.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr std::array<std::string_view, 48> columns {
    "session_id", "drum_id", "fixture_state_id", "drum_family", "experiment", "take_id",
    "accepted", "rejection_reason", "sample_rate_hz", "native_bit_depth",
    "pretrigger_seconds", "duration_seconds", "clock_id", "unprocessed",
    "force_path", "force_calibration_path", "contact_traction_path",
    "contact_traction_calibration_path", "head_path",
    "head_calibration_path", "bachi_path", "bachi_calibration_path",
    "near_path", "near_calibration_path", "far_path",
    "far_calibration_path", "trigger_path", "input_coordinate_id",
    "input_radius_norm", "input_azimuth_rad", "observation_coordinate_id",
    "observation_radius_norm", "observation_azimuth_rad", "force_level_id",
    "articulation", "bachi", "bachi_id", "bachi_bare_mass_kg",
    "bachi_added_sensor_mass_kg", "bachi_tip_profile_path",
    "bachi_tip_profile_calibration_path", "nominal_strike_radius_norm",
    "measured_relative_speed_mps", "palm_radius_norm", "palm_azimuth_rad",
    "palm_contact_area_m2", "palm_normal_load_n", "head_hoop_contact"
};

struct Coordinate
{
    double radius {};
    double azimuth {};
};

struct BSpec
{
    std::string articulation;
    std::string bachi;
    double radius {};
    double speed {};
    double tolerance {};
};

struct BachiDefinition
{
    std::string category;
    double bareMass {};
    std::string tipProfile;
    std::string tipCalibration;
};

struct Group
{
    std::string session;
    std::string drum;
    std::string fixture;
    std::string family;
    std::string clock;
    int sampleRate {};
    int bitDepth {};
    std::map<std::string, Coordinate> inputs;
    std::map<std::string, Coordinate> observations;
    std::set<std::string> forceLevels;
    std::map<std::string, std::set<std::string>> aCells;
    std::map<std::string, std::string> aTakeConditions;
    std::set<std::string> bTakeIds;
    std::vector<std::map<std::string, double>> bTakes;
};

std::vector<std::string> splitTabs (const std::string& line)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    for (;;)
    {
        const auto tab = line.find ('\t', start);
        result.push_back (line.substr (start, tab - start));
        if (tab == std::string::npos)
            return result;
        start = tab + 1;
    }
}

std::string join (const std::vector<std::string>& fields)
{
    std::string result;
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index != 0)
            result += '\t';
        result += fields[index];
    }
    return result;
}

std::string headerLine()
{
    std::vector<std::string> fields;
    for (const auto column : columns)
        fields.emplace_back (column);
    return join (fields);
}

bool isPresent (const std::string& value)
{
    return ! value.empty() && value != "-";
}

template <typename Number>
bool parseNumber (const std::string& text, Number& value)
{
    std::istringstream stream (text);
    stream.imbue (std::locale::classic());
    if (! (stream >> value))
        return false;
    char extra = 0;
    return ! (stream >> extra);
}

bool parseFlag (const std::string& text, bool& value)
{
    if (text == "0" || text == "1")
    {
        value = text == "1";
        return true;
    }
    return false;
}

const std::vector<BSpec>& bSpecs()
{
    static const auto specs = []
    {
        std::vector<BSpec> result;
        const std::array<double, 3> ordinarySpeeds { 2.0, 3.5, 4.5 };
        const std::array<double, 15> sweep {
            0.25, 0.35, 0.50, 0.65, 0.80, 0.95, 1.10, 1.25,
            1.40, 1.55, 1.70, 1.85, 2.00, 3.50, 4.50
        };
        const auto add = [&result] (std::string articulation,
                                    std::string bachi,
                                    double radius,
                                    double speed)
        {
            const auto tolerance = speed <= 2.0 ? 0.05 : (speed < 4.0 ? 0.15 : 0.20);
            result.push_back ({ std::move (articulation), std::move (bachi),
                                radius, speed, tolerance });
        };

        for (const auto radius : { 0.20, 0.75 })
            for (const auto speed : ordinarySpeeds)
                add ("don", "hard", radius, speed);
        for (const auto speed : sweep)
            add ("ka", "hard", 0.91, speed);
        for (const auto speed : sweep)
            add ("tsu-held", "hard", 0.20, speed);
        for (const auto speed : sweep)
            add ("don-rim", "hard", 0.97, speed);
        for (const auto speed : ordinarySpeeds)
            add ("don", "soft", 0.20, speed);
        return result;
    }();
    return specs;
}

std::string groupKey (const std::string& session,
                      const std::string& drum,
                      const std::string& fixture)
{
    return session + '\x1f' + drum + '\x1f' + fixture;
}

std::string cellKey (const std::string& input,
                     const std::string& observation,
                     const std::string& level)
{
    return input + '\x1f' + observation + '\x1f' + level;
}

double angularDistance (double a, double b)
{
    return std::abs (std::remainder (a - b, 2.0 * std::numbers::pi));
}

bool sameCoordinate (const Coordinate& a, const Coordinate& b)
{
    return std::abs (a.radius - b.radius) <= 1.0e-9
        && angularDistance (a.azimuth, b.azimuth) <= 1.0e-9;
}

std::vector<std::array<std::string, 3>> coordinateCores (
    const std::map<std::string, Coordinate>& coordinates)
{
    std::vector<std::string> centres;
    std::vector<std::string> edges;
    for (const auto& [id, coordinate] : coordinates)
    {
        if (coordinate.radius <= 0.05)
            centres.push_back (id);
        if (coordinate.radius >= 0.70)
            edges.push_back (id);
    }

    std::vector<std::array<std::string, 3>> result;
    for (const auto& centre : centres)
        for (std::size_t first = 0; first < edges.size(); ++first)
            for (std::size_t second = first + 1; second < edges.size(); ++second)
                if (angularDistance (coordinates.at (edges[first]).azimuth,
                                     coordinates.at (edges[second]).azimuth) > 1.0e-3)
                    result.push_back ({ centre, edges[first], edges[second] });
    return result;
}

bool completeA (const Group& group)
{
    const auto inputs = coordinateCores (group.inputs);
    const auto observations = coordinateCores (group.observations);
    const std::vector<std::string> levels (group.forceLevels.begin(),
                                           group.forceLevels.end());
    for (const auto& inputCore : inputs)
        for (const auto& observationCore : observations)
            for (std::size_t first = 0; first < levels.size(); ++first)
                for (std::size_t second = first + 1; second < levels.size(); ++second)
                {
                    bool complete = true;
                    for (const auto& input : inputCore)
                        for (const auto& observation : observationCore)
                            for (const auto& level : { levels[first], levels[second] })
                            {
                                const auto cell = group.aCells.find (
                                    cellKey (input, observation, level));
                                complete = complete && cell != group.aCells.end()
                                    && cell->second.size() >= 10;
                            }
                    if (complete)
                        return true;
                }
    return false;
}

double median (const std::map<std::string, double>& takes)
{
    std::vector<double> values;
    for (const auto& [id, value] : takes)
    {
        (void) id;
        values.push_back (value);
    }
    std::sort (values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 != 0
        ? values[middle]
        : 0.5 * (values[middle - 1] + values[middle]);
}

bool completeB (const Group& group)
{
    const auto& specs = bSpecs();
    if (group.bTakes.size() != specs.size())
        return false;
    for (const auto& takes : group.bTakes)
        if (takes.size() < 10)
            return false;

    for (const auto articulation : { std::string_view ("ka"),
                                     std::string_view ("tsu-held"),
                                     std::string_view ("don-rim") })
    {
        std::vector<double> medians;
        for (std::size_t index = 0; index < specs.size(); ++index)
            if (specs[index].articulation == articulation && specs[index].speed <= 2.0)
                medians.push_back (median (group.bTakes[index]));
        if (medians.size() != 13 || medians.front() > 0.30
            || medians.back() < 1.95)
            return false;
        for (std::size_t index = 1; index < medians.size(); ++index)
            if (medians[index] - medians[index - 1] > 0.20 + 1.0e-9)
                return false;
    }

    const auto binMedian = [&group, &specs] (std::string_view articulation,
                                             double target)
    {
        for (std::size_t index = 0; index < specs.size(); ++index)
            if (specs[index].articulation == articulation
                && std::abs (specs[index].speed - target) <= 1.0e-9)
                return median (group.bTakes[index]);
        return std::numeric_limits<double>::quiet_NaN();
    };
    struct Sentinel
    {
        std::string_view articulation;
        double lower;
        double upper;
        double transition;
    };
    for (const auto& sentinel : {
             Sentinel { "tsu-held", 0.65, 0.80, 0.79 },
             Sentinel { "don-rim", 0.95, 1.10, 1.00 },
             Sentinel { "ka", 1.40, 1.55, 1.51 } })
        if (binMedian (sentinel.articulation, sentinel.lower) > sentinel.transition
            || binMedian (sentinel.articulation, sentinel.upper) < sentinel.transition)
            return false;
    return true;
}

bool validateInventory (std::istream& input, std::string& result)
{
    std::string line;
    if (! std::getline (input, line))
    {
        result = "empty inventory";
        return false;
    }
    if (! line.empty() && line.back() == '\r')
        line.pop_back();

    const auto header = splitTabs (line);
    std::map<std::string, std::size_t> indexes;
    for (std::size_t index = 0; index < header.size(); ++index)
        if (! indexes.emplace (header[index], index).second)
        {
            result = "duplicate header column: " + header[index];
            return false;
        }
    for (const auto column : columns)
        if (! indexes.contains (std::string (column)))
        {
            result = "missing header column: " + std::string (column);
            return false;
        }

    std::map<std::string, Group> groups;
    std::map<std::string, std::string> drumFamilies;
    std::map<std::string, BachiDefinition> bachiDefinitions;
    std::map<std::string, double> bachiSensorMasses;
    std::size_t lineNumber = 1;
    std::size_t rowCount = 0;
    const auto fail = [&result, &lineNumber] (const std::string& message)
    {
        result = "line " + std::to_string (lineNumber) + ": " + message;
        return false;
    };

    while (std::getline (input, line))
    {
        ++lineNumber;
        if (! line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            return fail ("blank rows are not valid TSV records");
        const auto fields = splitTabs (line);
        if (fields.size() != header.size())
            return fail ("expected " + std::to_string (header.size())
                         + " fields, found " + std::to_string (fields.size()));
        const auto& value = [&fields, &indexes] (std::string_view name) -> const std::string&
        {
            return fields[indexes.at (std::string (name))];
        };

        for (const auto name : { "session_id", "drum_id", "fixture_state_id",
                                 "drum_family", "experiment", "take_id", "clock_id" })
            if (! isPresent (value (name)))
                return fail (std::string (name) + " is required");

        const auto& family = value ("drum_family");
        if (family != "nagado" && family != "chudaiko" && family != "odaiko"
            && family != "okedo" && family != "shime")
            return fail ("drum_family must be nagado, chudaiko, odaiko, okedo, or shime");
        const auto familyEntry = drumFamilies.emplace (value ("drum_id"), family);
        if (! familyEntry.second && familyEntry.first->second != family)
            return fail ("drum_id changes drum_family across sessions");

        bool accepted = false;
        bool unprocessed = false;
        int sampleRate = 0;
        int bitDepth = 0;
        double pretrigger = 0.0;
        double duration = 0.0;
        if (! parseFlag (value ("accepted"), accepted))
            return fail ("accepted must be 0 or 1");
        if (! parseFlag (value ("unprocessed"), unprocessed) || ! unprocessed)
            return fail ("unprocessed must be 1");
        if (! parseNumber (value ("sample_rate_hz"), sampleRate) || sampleRate < 96000)
            return fail ("sample_rate_hz must be at least 96000");
        if (! parseNumber (value ("native_bit_depth"), bitDepth) || bitDepth < 24)
            return fail ("native_bit_depth must be at least 24");
        if (! parseNumber (value ("pretrigger_seconds"), pretrigger)
            || ! std::isfinite (pretrigger) || pretrigger < 0.25)
            return fail ("pretrigger_seconds must be at least 0.25");
        const auto minimumDuration = family == "odaiko" ? 12.0 : 4.0;
        if (! parseNumber (value ("duration_seconds"), duration)
            || ! std::isfinite (duration) || duration < minimumDuration)
            return fail ("duration_seconds is below the drum-family minimum");
        if (! accepted && ! isPresent (value ("rejection_reason")))
            return fail ("rejected takes require rejection_reason");

        for (const auto path : { "force_path", "force_calibration_path",
                                 "contact_traction_path",
                                 "contact_traction_calibration_path",
                                 "head_path", "head_calibration_path",
                                 "near_path", "near_calibration_path",
                                 "far_path", "far_calibration_path", "trigger_path" })
            if (! isPresent (value (path)))
                return fail (std::string (path) + " is required");

        const auto key = groupKey (value ("session_id"), value ("drum_id"),
                                   value ("fixture_state_id"));
        auto [groupAt, inserted] = groups.try_emplace (key);
        auto& group = groupAt->second;
        if (inserted)
        {
            group.session = value ("session_id");
            group.drum = value ("drum_id");
            group.fixture = value ("fixture_state_id");
            group.family = family;
            group.clock = value ("clock_id");
            group.sampleRate = sampleRate;
            group.bitDepth = bitDepth;
            group.bTakes.resize (bSpecs().size());
        }
        else if (group.family != family || group.clock != value ("clock_id")
                 || group.sampleRate != sampleRate || group.bitDepth != bitDepth)
            return fail ("one session/drum group must keep one family, clock, rate, and native bit depth");

        const auto& experiment = value ("experiment");
        if (experiment == "A")
        {
            for (const auto name : { "input_coordinate_id", "observation_coordinate_id",
                                     "force_level_id" })
                if (! isPresent (value (name)))
                    return fail (std::string (name) + " is required for experiment A");

            double inputRadius = 0.0;
            double inputAzimuth = 0.0;
            double observationRadius = 0.0;
            double observationAzimuth = 0.0;
            if (! parseNumber (value ("input_radius_norm"), inputRadius)
                || ! std::isfinite (inputRadius) || inputRadius < 0.0 || inputRadius > 1.0
                || ! parseNumber (value ("input_azimuth_rad"), inputAzimuth)
                || ! std::isfinite (inputAzimuth)
                || ! parseNumber (value ("observation_radius_norm"), observationRadius)
                || ! std::isfinite (observationRadius) || observationRadius < 0.0
                || observationRadius > 1.0
                || ! parseNumber (value ("observation_azimuth_rad"), observationAzimuth)
                || ! std::isfinite (observationAzimuth))
                return fail ("experiment A coordinates must have finite normalized radii and azimuths");

            const Coordinate inputCoordinate { inputRadius, inputAzimuth };
            const Coordinate observationCoordinate { observationRadius,
                                                      observationAzimuth };
            const auto inputAt = group.inputs.emplace (value ("input_coordinate_id"),
                                                       inputCoordinate);
            const auto observationAt = group.observations.emplace (
                value ("observation_coordinate_id"), observationCoordinate);
            if ((! inputAt.second && ! sameCoordinate (inputAt.first->second, inputCoordinate))
                || (! observationAt.second
                    && ! sameCoordinate (observationAt.first->second,
                                         observationCoordinate)))
                return fail ("a coordinate_id changes radius or azimuth within its session");

            group.forceLevels.insert (value ("force_level_id"));
            const auto takeCondition = value ("input_coordinate_id") + '\x1f'
                + value ("force_level_id") + '\x1f' + (accepted ? "1" : "0")
                + '\x1f' + value ("contact_traction_path") + '\x1f'
                + value ("contact_traction_calibration_path")
                + static_cast<char> (31) + value ("force_path")
                + static_cast<char> (31) + value ("force_calibration_path")
                + static_cast<char> (31) + value ("near_path")
                + static_cast<char> (31) + value ("near_calibration_path")
                + static_cast<char> (31) + value ("far_path")
                + static_cast<char> (31) + value ("far_calibration_path")
                + static_cast<char> (31) + value ("trigger_path");
            const auto takeAt = group.aTakeConditions.emplace (value ("take_id"),
                                                               takeCondition);
            if (! takeAt.second && takeAt.first->second != takeCondition)
                return fail ("one experiment A take_id changes input, force level, "
                             "acceptance, or a take-level channel reference");
            if (accepted)
                group.aCells[cellKey (value ("input_coordinate_id"),
                                      value ("observation_coordinate_id"),
                                      value ("force_level_id"))]
                    .insert (value ("take_id"));
        }
        else if (experiment == "B")
        {
            for (const auto path : { "bachi_path", "bachi_calibration_path" })
                if (! isPresent (value (path)))
                    return fail (std::string (path) + " is required for experiment B");
            if (! isPresent (value ("articulation")) || ! isPresent (value ("bachi")))
                return fail ("articulation and bachi are required for experiment B");
            for (const auto field : { "bachi_id", "bachi_tip_profile_path",
                                      "bachi_tip_profile_calibration_path" })
                if (! isPresent (value (field)))
                    return fail (std::string (field) + " is required for experiment B");

            double bareMass = 0.0;
            double sensorMass = 0.0;
            if (! parseNumber (value ("bachi_bare_mass_kg"), bareMass)
                || ! std::isfinite (bareMass) || bareMass <= 0.0
                || ! parseNumber (value ("bachi_added_sensor_mass_kg"), sensorMass)
                || ! std::isfinite (sensorMass) || sensorMass < 0.0)
                return fail ("experiment B requires positive bare bachi mass and "
                             "nonnegative moving sensor mass");

            const BachiDefinition definition {
                value ("bachi"), bareMass, value ("bachi_tip_profile_path"),
                value ("bachi_tip_profile_calibration_path")
            };
            const auto definitionAt = bachiDefinitions.emplace (
                value ("bachi_id"), definition);
            if (! definitionAt.second)
            {
                const auto& prior = definitionAt.first->second;
                if (prior.category != definition.category
                    || prior.bareMass != definition.bareMass
                    || prior.tipProfile != definition.tipProfile
                    || prior.tipCalibration != definition.tipCalibration)
                    return fail ("bachi_id changes category, bare mass, or tip profile");
            }
            const auto sensorKey = value ("session_id") + '\x1f' + value ("bachi_id");
            const auto sensorAt = bachiSensorMasses.emplace (sensorKey, sensorMass);
            if (! sensorAt.second && sensorAt.first->second != sensorMass)
                return fail ("bachi_id changes added sensor mass within one session");

            double radius = 0.0;
            double speed = 0.0;
            if (! parseNumber (value ("nominal_strike_radius_norm"), radius)
                || ! std::isfinite (radius) || radius < 0.0 || radius > 1.0
                || ! parseNumber (value ("measured_relative_speed_mps"), speed)
                || ! std::isfinite (speed) || speed <= 0.0)
                return fail ("experiment B requires a finite radius and positive measured relative speed");

            if (value ("articulation") == "tsu-held")
            {
                double palmRadius = 0.0;
                double palmAzimuth = 0.0;
                double palmArea = 0.0;
                double palmLoad = 0.0;
                if (! parseNumber (value ("palm_radius_norm"), palmRadius)
                    || ! std::isfinite (palmRadius) || palmRadius < 0.0
                    || palmRadius > 1.0
                    || ! parseNumber (value ("palm_azimuth_rad"), palmAzimuth)
                    || ! std::isfinite (palmAzimuth)
                    || ! parseNumber (value ("palm_contact_area_m2"), palmArea)
                    || ! std::isfinite (palmArea) || palmArea <= 0.0
                    || ! parseNumber (value ("palm_normal_load_n"), palmLoad)
                    || ! std::isfinite (palmLoad) || palmLoad <= 0.0)
                    return fail ("tsu-held requires finite palm geometry and positive area/load");
            }
            if (value ("articulation") == "don-rim")
            {
                bool simultaneous = false;
                if (! parseFlag (value ("head_hoop_contact"), simultaneous)
                    || (accepted && ! simultaneous))
                    return fail ("accepted don-rim requires head_hoop_contact=1");
            }
            if (! group.bTakeIds.insert (value ("take_id")).second)
                return fail ("experiment B take_id must be unique within its session");

            if (accepted)
            {
                int match = -1;
                double bestDistance = std::numeric_limits<double>::infinity();
                bool tied = false;
                const auto& specs = bSpecs();
                for (std::size_t index = 0; index < specs.size(); ++index)
                {
                    const auto& spec = specs[index];
                    const auto distance = std::abs (speed - spec.speed);
                    if (spec.articulation != value ("articulation")
                        || spec.bachi != value ("bachi")
                        || std::abs (radius - spec.radius) > 1.0e-9
                        || distance > spec.tolerance + 1.0e-9)
                        continue;
                    if (distance < bestDistance - 1.0e-9)
                    {
                        match = static_cast<int> (index);
                        bestDistance = distance;
                        tied = false;
                    }
                    else if (std::abs (distance - bestDistance) <= 1.0e-9)
                        tied = true;
                }
                if (tied)
                    return fail ("measured speed lies exactly between two acceptance bins");
                if (match >= 0)
                    group.bTakes[static_cast<std::size_t> (match)]
                        .emplace (value ("take_id"), speed);
            }
        }
        else
            return fail ("experiment must be A or B");

        ++rowCount;
    }

    if (rowCount == 0)
    {
        result = "inventory contains no take rows";
        return false;
    }

    std::set<std::pair<std::string, std::string>> aFixtures;
    std::set<std::pair<std::string, std::string>> bFixtures;
    for (const auto& [key, group] : groups)
    {
        (void) key;
        if ((group.family == "nagado" || group.family == "chudaiko")
            && completeA (group))
            aFixtures.emplace (group.drum, group.fixture);
        if ((group.family == "nagado" || group.family == "chudaiko")
            && completeB (group))
            bFixtures.emplace (group.drum, group.fixture);
    }
    for (const auto& fixture : aFixtures)
        if (bFixtures.contains (fixture))
        {
            result = "acquisition and A/B coverage complete for drum/fixture "
                + fixture.first + "/" + fixture.second;
            return true;
        }

    result = "no nagado/chudaiko drum_id/fixture_state_id has both a complete 3x3x2 "
             "experiment A core and every required experiment B measured-speed bin";
    return false;
}

enum class SelfTestMutation
{
    none,
    lowRate,
    missingCalibration,
    missingTractionMap,
    changedATractionMap,
    missingACell,
    missingBBin,
    badMedianGap,
    badSentinelBracket,
    missingPalm,
    missingRimContact,
    badBachiMass,
    badBachiSensorMass,
    changedBachiSensorMass,
    missingTipProfile,
    changedBachiDefinition,
    splitFixtureState,
    splitClock
};

std::size_t columnIndex (std::string_view name)
{
    return static_cast<std::size_t> (
        std::find (columns.begin(), columns.end(), name) - columns.begin());
}

std::string selfTestInventory (SelfTestMutation mutation)
{
    std::ostringstream output;
    output << headerLine() << '\n';
    bool firstRow = true;
    const auto emit = [&output, &firstRow, mutation] (std::vector<std::string> row)
    {
        if (firstRow && mutation == SelfTestMutation::lowRate)
            row[columnIndex ("sample_rate_hz")] = "95999";
        if (firstRow && mutation == SelfTestMutation::missingCalibration)
            row[columnIndex ("head_calibration_path")] = "-";
        if (firstRow && mutation == SelfTestMutation::missingTractionMap)
            row[columnIndex ("contact_traction_path")] = "-";
        firstRow = false;
        output << join (row) << '\n';
    };
    const auto base = [] (const std::string& experiment, const std::string& take)
    {
        std::vector<std::string> row (columns.size(), "-");
        const auto set = [&row] (std::string_view name, std::string value)
        {
            row[columnIndex (name)] = std::move (value);
        };
        set ("session_id", experiment == "A" ? "linear-session" : "strike-session");
        set ("drum_id", "fixture-001");
        set ("fixture_state_id", "heads-01-tension-01-mount-01");
        set ("drum_family", "nagado");
        set ("experiment", experiment);
        set ("take_id", take);
        set ("accepted", "1");
        set ("rejection_reason", "");
        set ("sample_rate_hz", "96000");
        set ("native_bit_depth", "24");
        set ("pretrigger_seconds", "0.25");
        set ("duration_seconds", "4.0");
        set ("clock_id", "clock-a");
        set ("unprocessed", "1");
        set ("force_path", "raw/" + take + "#force");
        set ("force_calibration_path", "cal/force.tsv");
        set ("contact_traction_path", "raw/" + take + "#traction");
        set ("contact_traction_calibration_path", "cal/traction.tsv");
        set ("head_path", "raw/" + take + "#head");
        set ("head_calibration_path", "cal/head.tsv");
        set ("near_path", "raw/" + take + "#near");
        set ("near_calibration_path", "cal/near.tsv");
        set ("far_path", "raw/" + take + "#far");
        set ("far_calibration_path", "cal/far.tsv");
        set ("trigger_path", "embedded");
        return row;
    };

    struct TestCoordinate
    {
        const char* id;
        const char* radius;
        const char* azimuth;
    };
    const std::array<TestCoordinate, 3> coordinates {
        TestCoordinate { "centre", "0.00", "0.00" },
        TestCoordinate { "edge-a", "0.75", "0.00" },
        TestCoordinate { "edge-b", "0.75", "1.57" }
    };
    for (std::size_t inputIndex = 0; inputIndex < coordinates.size(); ++inputIndex)
        for (const auto& level : { std::string ("low-1"), std::string ("low-2") })
            for (int takeIndex = 0; takeIndex < 10; ++takeIndex)
                for (std::size_t observationIndex = 0;
                     observationIndex < coordinates.size(); ++observationIndex)
                {
                    if (mutation == SelfTestMutation::missingACell && inputIndex == 0
                        && level == "low-1" && observationIndex == 0)
                        continue;
                    const auto take = "A-" + std::to_string (inputIndex) + "-"
                        + level + "-" + std::to_string (takeIndex);
                    auto row = base ("A", take);
                    const auto set = [&row] (std::string_view name, std::string value)
                    {
                        row[columnIndex (name)] = std::move (value);
                    };
                    set ("input_coordinate_id", coordinates[inputIndex].id);
                    set ("input_radius_norm", coordinates[inputIndex].radius);
                    set ("input_azimuth_rad", coordinates[inputIndex].azimuth);
                    set ("observation_coordinate_id", coordinates[observationIndex].id);
                    set ("observation_radius_norm", coordinates[observationIndex].radius);
                    set ("observation_azimuth_rad", coordinates[observationIndex].azimuth);
                    set ("force_level_id", level);
                    if (mutation == SelfTestMutation::changedATractionMap
                        && inputIndex == 0 && level == "low-1" && takeIndex == 0
                        && observationIndex == 1)
                        set ("contact_traction_path", "raw/changed-traction");
                    emit (std::move (row));
                }

    const auto& specs = bSpecs();
    for (std::size_t specIndex = 0; specIndex < specs.size(); ++specIndex)
    {
        const auto count = mutation == SelfTestMutation::missingBBin && specIndex == 0 ? 9 : 10;
        for (int takeIndex = 0; takeIndex < count; ++takeIndex)
        {
            const auto& spec = specs[specIndex];
            const auto take = "B-" + std::to_string (specIndex) + "-"
                + std::to_string (takeIndex);
            auto row = base ("B", take);
            const auto set = [&row] (std::string_view name, std::string value)
            {
                row[columnIndex (name)] = std::move (value);
            };
            set ("bachi_path", "raw/" + take + "#bachi");
            set ("bachi_calibration_path", "cal/bachi.tsv");
            set ("bachi_id", spec.bachi + "-bachi-01");
            set ("bachi_bare_mass_kg", spec.bachi == "hard" ? "0.240" : "0.180");
            set ("bachi_added_sensor_mass_kg", "0.012");
            set ("bachi_tip_profile_path", "metrology/" + spec.bachi + "-tip.xyz");
            set ("bachi_tip_profile_calibration_path", "cal/tip-metrology.tsv");
            if (mutation == SelfTestMutation::badBachiMass && specIndex == 0
                && takeIndex == 0)
                set ("bachi_bare_mass_kg", "0");
            if (mutation == SelfTestMutation::badBachiSensorMass && specIndex == 0
                && takeIndex == 0)
                set ("bachi_added_sensor_mass_kg", "-0.001");
            if (mutation == SelfTestMutation::changedBachiSensorMass
                && specIndex == 1 && takeIndex == 0)
                set ("bachi_added_sensor_mass_kg", "0.013");
            if (mutation == SelfTestMutation::missingTipProfile && specIndex == 0
                && takeIndex == 0)
                set ("bachi_tip_profile_path", "-");
            if (mutation == SelfTestMutation::changedBachiDefinition && specIndex == 1
                && takeIndex == 0)
                set ("bachi_bare_mass_kg", "0.241");
            if (mutation == SelfTestMutation::splitFixtureState)
                set ("fixture_state_id", "heads-02-tension-02-mount-02");
            set ("articulation", spec.articulation);
            set ("bachi", spec.bachi);
            set ("nominal_strike_radius_norm", std::to_string (spec.radius));
            if (spec.articulation == "tsu-held")
            {
                set ("palm_radius_norm", "0.30");
                set ("palm_azimuth_rad", "0.50");
                set ("palm_contact_area_m2", "0.006");
                set ("palm_normal_load_n", "18.0");
                if (mutation == SelfTestMutation::missingPalm && spec.speed == 0.25
                    && takeIndex == 0)
                    set ("palm_contact_area_m2", "-");
            }
            if (spec.articulation == "don-rim")
            {
                set ("head_hoop_contact", "1");
                if (mutation == SelfTestMutation::missingRimContact
                    && spec.speed == 0.25 && takeIndex == 0)
                    set ("head_hoop_contact", "0");
            }
            auto speed = spec.speed;
            if (mutation == SelfTestMutation::badMedianGap
                && spec.articulation == "ka" && spec.speed == 0.35)
                speed = 0.3001;
            if (mutation == SelfTestMutation::badMedianGap
                && spec.articulation == "ka" && spec.speed == 0.50)
                speed = 0.5499;
            if (mutation == SelfTestMutation::badSentinelBracket
                && spec.articulation == "tsu-held" && spec.speed == 0.80)
                speed = 0.7501;
            set ("measured_relative_speed_mps", std::to_string (speed));
            if (mutation == SelfTestMutation::splitClock && specIndex == 0
                && takeIndex == 0)
                set ("clock_id", "clock-b");
            emit (std::move (row));
        }
    }

    for (const auto acceptedHoldout : { true, false })
    {
        const auto take = acceptedHoldout ? "B-accepted-holdout" : "B-rejected-miss";
        auto row = base ("B", take);
        const auto set = [&row] (std::string_view name, std::string value)
        {
            row[columnIndex (name)] = std::move (value);
        };
        if (mutation == SelfTestMutation::splitFixtureState)
            set ("fixture_state_id", "heads-02-tension-02-mount-02");
        set ("accepted", acceptedHoldout ? "1" : "0");
        set ("rejection_reason", acceptedHoldout ? "" : "target miss");
        set ("bachi_path", "raw/" + std::string (take) + "#bachi");
        set ("bachi_calibration_path", "cal/bachi.tsv");
        set ("bachi_id", "hard-bachi-01");
        set ("bachi_bare_mass_kg", "0.240");
        set ("bachi_added_sensor_mass_kg", "0.012");
        set ("bachi_tip_profile_path", "metrology/hard-tip.xyz");
        set ("bachi_tip_profile_calibration_path", "cal/tip-metrology.tsv");
        set ("articulation", acceptedHoldout ? "pair-holdout" : "miss");
        set ("bachi", "hard");
        set ("nominal_strike_radius_norm", "0.20");
        set ("measured_relative_speed_mps", "2.70");
        emit (std::move (row));
    }
    return output.str();
}

int selfTest()
{
    for (const auto mutation : { SelfTestMutation::none, SelfTestMutation::lowRate,
                                 SelfTestMutation::missingCalibration,
                                 SelfTestMutation::missingTractionMap,
                                 SelfTestMutation::changedATractionMap,
                                 SelfTestMutation::missingACell,
                                 SelfTestMutation::missingBBin,
                                 SelfTestMutation::badMedianGap,
                                 SelfTestMutation::badSentinelBracket,
                                 SelfTestMutation::missingPalm,
                                 SelfTestMutation::missingRimContact,
                                 SelfTestMutation::badBachiMass,
                                 SelfTestMutation::badBachiSensorMass,
                                 SelfTestMutation::changedBachiSensorMass,
                                 SelfTestMutation::missingTipProfile,
                                 SelfTestMutation::changedBachiDefinition,
                                 SelfTestMutation::splitFixtureState,
                                 SelfTestMutation::splitClock })
    {
        auto text = selfTestInventory (mutation);
        std::istringstream input (text);
        std::string message;
        const auto valid = validateInventory (input, message);
        if (valid != (mutation == SelfTestMutation::none))
        {
            std::cerr << "self-test failed: " << message << '\n';
            return 1;
        }
    }
    std::cout << "calibration capture validator self-test passed\n";
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc == 2 && std::string_view (argv[1]) == "--print-header")
    {
        std::cout << headerLine() << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view (argv[1]) == "--self-test")
        return selfTest();
    if (argc == 3 && std::string_view (argv[1]) == "--check")
    {
        std::ifstream input (argv[2]);
        if (! input)
        {
            std::cerr << "cannot open inventory: " << argv[2] << '\n';
            return 2;
        }
        std::string message;
        if (! validateInventory (input, message))
        {
            std::cerr << "invalid calibration capture inventory: " << message << '\n';
            return 1;
        }
        std::cout << "valid calibration capture inventory: " << message << '\n';
        return 0;
    }

    std::cerr << "usage: TaikorValidateCalibrationCapture --print-header | --self-test | --check FILE.tsv\n";
    return 2;
}

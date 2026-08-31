#include "DSP/AcustraEngine.h"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
static_assert(std::endian::native == std::endian::little,
              "BridgeProbe raw files require a little-endian host");
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "BridgeProbe raw files require IEEE-754 float32");

bool parseFloat(const char* text, float& value) noexcept
{
    char* end = nullptr;
    errno = 0;
    value = std::strtof(text, &end);
    return end != text && *end == '\0' && errno != ERANGE
        && std::isfinite(value);
}

bool parseInt(const char* text, int& value) noexcept
{
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE
        || parsed < 0 || parsed > 127)
        return false;
    value = static_cast<int>(parsed);
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4 || argc > 7)
    {
        std::cerr << "usage: BridgeProbe output.f32 steel|nylon on|off "
                     "[pluck-position-0-to-1] [auditorium|dreadnought] "
                     "[midi-note]\n";
        return EXIT_FAILURE;
    }

    const std::string outputPath(argv[1]);
    if (outputPath.ends_with(".telemetry.f32"))
    {
        std::cerr << "output path must not end in .telemetry.f32\n";
        return EXIT_FAILURE;
    }

    const std::string material(argv[2]);
    if (material != "steel" && material != "nylon")
    {
        std::cerr << "material must be steel or nylon\n";
        return EXIT_FAILURE;
    }
    const std::string coupling(argv[3]);
    if (coupling != "on" && coupling != "off")
    {
        std::cerr << "bridge coupling must be on or off\n";
        return EXIT_FAILURE;
    }

    constexpr double rate = 48000.0;
    constexpr int samples = static_cast<int>(4.0 * rate);
    acustra::AcustraEngine engine;
    engine.prepare(rate, 1);
    acustra::EngineParameters parameters;
    parameters.stringMaterial = material == "nylon"
        ? acustra::StringMaterial::Nylon : acustra::StringMaterial::Steel;
    if (argc >= 6)
    {
        const std::string shape(argv[5]);
        if (shape != "auditorium" && shape != "dreadnought")
        {
            std::cerr << "shape must be auditorium or dreadnought\n";
            return EXIT_FAILURE;
        }
        parameters.shape = shape == "dreadnought"
            ? acustra::BodyShape::Dreadnought
            : acustra::BodyShape::Auditorium;
    }
    parameters.bodyMaterial = acustra::BodyMaterial::Spruce;
    parameters.stringAge = 0.0f;
    if (argc >= 5)
    {
        float pluckPosition = 0.0f;
        if (!parseFloat(argv[4], pluckPosition)
            || pluckPosition < 0.0f || pluckPosition > 1.0f)
        {
            std::cerr << "pluck position must be finite and between 0 and 1\n";
            return EXIT_FAILURE;
        }
        parameters.pluckPosition = pluckPosition;
    }
    parameters.touch = parameters.stringMaterial == acustra::StringMaterial::Nylon
        ? 0.08f : 0.72f;
    engine.setParameters(parameters);
    engine.setBridgeCouplingEnabled(coupling == "on");
    int midiNote = 40;
    if (argc == 7 && !parseInt(argv[6], midiNote))
    {
        std::cerr << "MIDI note must be an integer between 0 and 127\n";
        return EXIT_FAILURE;
    }
    engine.noteOn(midiNote, 0.72f);

    std::vector<float> interleaved(static_cast<std::size_t>(2 * samples));
    std::vector<float> telemetry(static_cast<std::size_t>(5 * samples));
    double cumulativeWork = 0.0;
    double minimumWork = 0.0;
    double positiveWork = 0.0;
    double negativeWork = 0.0;
    double bodyWork = 0.0;
    double minimumBodyWork = 0.0;
    double tailWork = 0.0;
    double minimumTailWork = 0.0;
    double maximumForceBalanceError = 0.0;
    double maximumVelocity = 0.0;
    double maximumForce = 0.0;
    for (int sample = 0; sample < samples; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
        interleaved[static_cast<std::size_t>(2 * sample)] = left;
        interleaved[static_cast<std::size_t>(2 * sample + 1)] = right;
        telemetry[static_cast<std::size_t>(5 * sample)]
            = engine.getLastBridgeReactionForce();
        telemetry[static_cast<std::size_t>(5 * sample + 1)]
            = engine.getLastBridgeBodyForce();
        telemetry[static_cast<std::size_t>(5 * sample + 2)]
            = engine.getLastBridgeTailForce();
        telemetry[static_cast<std::size_t>(5 * sample + 3)]
            = engine.getLastBridgeVelocity();
        telemetry[static_cast<std::size_t>(5 * sample + 4)]
            = engine.getLastSympatheticRadiationForce();
        const double power = engine.getLastBridgePower();
        cumulativeWork += power / rate;
        minimumWork = std::min(minimumWork, cumulativeWork);
        if (power >= 0.0)
            positiveWork += power / rate;
        else
            negativeWork -= power / rate;
        bodyWork += engine.getLastBridgeBodyPower() / rate;
        minimumBodyWork = std::min(minimumBodyWork, bodyWork);
        tailWork += engine.getLastBridgeTailPower() / rate;
        minimumTailWork = std::min(minimumTailWork, tailWork);
        maximumForceBalanceError = std::max(maximumForceBalanceError,
            static_cast<double>(std::abs(engine.getLastBridgeReactionForce()
                - engine.getLastBridgeBodyForce()
                - engine.getLastBridgeTailForce())));
        maximumVelocity = std::max(maximumVelocity,
            static_cast<double>(std::abs(engine.getLastBridgeVelocity())));
        maximumForce = std::max(maximumForce,
            static_cast<double>(std::abs(engine.getLastBridgeReactionForce())));
    }

    if (!(maximumForce > 0.0) || !std::isfinite(maximumForce))
    {
        std::cerr << "MIDI note is not playable in the selected tuning\n";
        return EXIT_FAILURE;
    }

    std::ofstream output(outputPath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(interleaved.data()),
                 static_cast<std::streamsize>(interleaved.size()
                     * sizeof(float)));
    if (!output)
        return EXIT_FAILURE;
    std::ofstream telemetryOutput(outputPath + ".telemetry.f32",
                                  std::ios::binary);
    telemetryOutput.write(reinterpret_cast<const char*>(telemetry.data()),
        static_cast<std::streamsize>(telemetry.size() * sizeof(float)));
    if (!telemetryOutput)
        return EXIT_FAILURE;
    std::cout << "work=" << cumulativeWork
              << " minimum=" << minimumWork
              << " positive=" << positiveWork
              << " negative=" << negativeWork
              << " body=" << bodyWork
              << " body_min=" << minimumBodyWork
              << " tail=" << tailWork
              << " tail_min=" << minimumTailWork
              << " balance_max=" << maximumForceBalanceError
              << " vmax=" << maximumVelocity
              << " fmax=" << maximumForce << '\n';
}

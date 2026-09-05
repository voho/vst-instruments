// Render annotated performances through the shipping JUCE-free engine.
// Input: ACUSTRA_PERFORMANCE_V1 sample_rate frame_count, then ordered rows
// frame channel midi velocity bend_semitones. Velocity zero means note-off.
// Channels 1-6 fix the played string (low E to high e); simultaneous note-offs
// precede note-ons. Output is headerless little-endian float32 stereo.
#include "DSP/AcustraEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct Event
{
    std::int64_t frame {};
    int channel {}, note {}, velocity {};
    float bend {};
};
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 5)
    {
        std::cerr << "usage: AcustraPerformanceRenderer EVENTS OUTPUT.f32 "
                     "[stereo_mic|treble_mic|bass_mic|saddle_piezo|magnetic "
                     "finger|pick|thumb]\n";
        return 2;
    }
    try
    {
        acustra::EngineParameters parameters;
        if (argc == 5)
        {
            const std::array captures { "stereo_mic", "treble_mic", "bass_mic",
                                        "saddle_piezo", "magnetic" };
            const std::array techniques { "finger", "pick", "thumb" };
            const auto capture = std::find(captures.begin(), captures.end(), std::string(argv[3]));
            const auto technique = std::find(techniques.begin(), techniques.end(), std::string(argv[4]));
            if (capture == captures.end() || technique == techniques.end())
                throw std::runtime_error("unknown capture or picking technique");
            parameters.capture = static_cast<acustra::CaptureType>(capture - captures.begin());
            parameters.picking = static_cast<acustra::PickingTechnique>(technique - techniques.begin());
        }
        std::ifstream input(argv[1]);
        input.imbue(std::locale::classic());
        std::string format;
        int sampleRate {};
        std::int64_t frames {};
        if (!(input >> format >> sampleRate >> frames)
            || format != "ACUSTRA_PERFORMANCE_V1"
            || sampleRate < 8000 || sampleRate > 192000
            || frames < 1 || frames > 60LL * sampleRate)
            throw std::runtime_error("invalid performance header (maximum 60 seconds)");
        std::vector<Event> events;
        while (input >> std::ws && input.peek() != std::char_traits<char>::eof())
        {
            Event event;
            if (!(input >> event.frame >> event.channel >> event.note
                        >> event.velocity >> event.bend)
                || event.frame < 0 || event.frame >= frames
                || event.channel < 1 || event.channel > 6
                || event.velocity < 0 || event.velocity > 127
                || event.note < 0 || event.note > 127
                || !std::isfinite(event.bend) || std::abs(event.bend) > 0.5f
                || (!events.empty() && event.frame < events.back().frame))
                throw std::runtime_error("invalid or unordered performance event");
            constexpr std::array openNotes { 40, 45, 50, 55, 59, 64 };
            const int fret = event.note - openNotes[static_cast<std::size_t>(event.channel - 1)];
            if (fret < 0 || fret > acustra::AcustraEngine::fretCount)
                throw std::runtime_error("performance requests an unplayable string/fret");
            events.push_back(event);
        }
        if (events.empty())
            throw std::runtime_error("performance contains no events");
        if (std::filesystem::exists(argv[2]))
            throw std::runtime_error("output already exists");

        acustra::AcustraEngine engine;
        constexpr int blockSize = 127;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        engine.setStringPerChannelMode(true);
        std::ofstream output(argv[2], std::ios::binary);
        if (!output)
            throw std::runtime_error("could not create output");
        std::array<float, blockSize> left {}, right {};
        std::size_t nextEvent = 0;
        for (std::int64_t frame = 0; frame < frames;)
        {
            while (nextEvent < events.size() && events[nextEvent].frame == frame)
            {
                const auto& event = events[nextEvent++];
                if (event.velocity == 0)
                    engine.noteOff(event.note, event.channel);
                else
                {
                    engine.setPitchBend(event.bend, event.channel);
                    engine.noteOn(event.note, event.velocity / 127.0f, event.channel);
                }
            }
            const auto nextFrame = nextEvent < events.size() ? events[nextEvent].frame : frames;
            const int count = static_cast<int>(std::min<std::int64_t>(blockSize, nextFrame - frame));
            engine.process(left.data(), right.data(), count);
            for (int index = 0; index < count; ++index)
                for (float value : { left[static_cast<std::size_t>(index)],
                                     right[static_cast<std::size_t>(index)] })
                {
                    if (!std::isfinite(value))
                        throw std::runtime_error("engine produced non-finite samples");
                    const auto bits = std::bit_cast<std::uint32_t>(value);
                    const std::array<char, 4> bytes {
                        static_cast<char>(bits), static_cast<char>(bits >> 8),
                        static_cast<char>(bits >> 16), static_cast<char>(bits >> 24) };
                    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                }
            frame += count;
        }
        output.close();
        if (output.fail())
            throw std::runtime_error("could not write output");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

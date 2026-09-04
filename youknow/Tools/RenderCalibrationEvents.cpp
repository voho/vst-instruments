// Render timestamped MIDI exported by AnalyzeHardwareCalibration.py. The
// hardware comparison uses the source file's actual SysEx and tempo, avoiding
// hand-transcribed patches and host-dependent MIDI-file playback speed.
#include "DSP/YouKnowSysEx.h"
#include "RealismComparisonSupport.h"

#include <iostream>
#include <stdexcept>

namespace
{
using namespace youknow;
using namespace youknow::tools::realism;

EngineParameters parametersFor(const sysex::Patch& patch, float character)
{
    EngineParameters p;
    p.lfoRate = patch.lfoRate; p.lfoDelay = patch.lfoDelay;
    p.dcoLfoDepth = patch.dcoLfo; p.pwmDepth = patch.pwm;
    p.pwmSource = patch.pwmSource; p.range = patch.range;
    p.sawEnabled = patch.saw; p.pulseEnabled = patch.pulse;
    p.subLevel = patch.sub; p.noiseLevel = patch.noise;
    p.highPass = patch.highPass; p.cutoff = patch.cutoff;
    p.resonance = patch.resonance; p.envPolarity = patch.envPolarity;
    p.envDepth = patch.vcfEnv; p.vcfLfoDepth = patch.vcfLfo;
    p.keyFollow = patch.keyFollow; p.vcaMode = patch.vcaMode;
    p.vcaLevel = patch.vcaLevel; p.attack = patch.attack;
    p.decay = patch.decay; p.sustain = patch.sustain;
    p.release = patch.release; p.chorus = patch.chorus;
    p.volume = 1.0f; p.polyphony = 6; p.calibration = character;
    return p;
}

struct Event
{
    std::size_t frame;
    std::vector<std::uint8_t> bytes;
};

std::vector<Event> readEvents(std::istream& input)
{
    std::vector<Event> events;
    std::string line;
    double previous = 0.0;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        double seconds;
        std::string hex, extra;
        if (!(row >> seconds >> hex) || (row >> extra)
            || !std::isfinite(seconds) || seconds < previous || seconds > 3600.0
            || hex.size() < 6 || hex.size() > 48 || hex.size() % 2 != 0)
            throw std::runtime_error("invalid or unordered event row: " + line);
        Event event { static_cast<std::size_t>(std::llround(seconds * comparisonSampleRate)), {} };
        for (std::size_t i = 0; i < hex.size(); i += 2)
        {
            const auto pair = hex.substr(i, 2);
            if (pair.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                throw std::runtime_error("invalid hexadecimal MIDI byte");
            event.bytes.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
        }
        events.push_back(std::move(event));
        previous = seconds;
    }
    if (!input.eof() || events.empty())
        throw std::runtime_error("empty or unreadable event file");
    return events;
}

void selfTest()
{
    std::istringstream input("# timestamped MIDI\n0 903c7f\n0.05 803c00\n");
    const auto events = readEvents(input);
    if (events.size() != 2 || events[0].frame != 0 || events[1].frame != 2400
        || events[0].bytes != std::vector<std::uint8_t> { 0x90, 60, 127 })
        throw std::runtime_error("event parser changed the source timing or bytes");
    for (const auto* text : { "", "-1 903c7f\n", "nan 903c7f\n",
                             "1 903c7f\n0 803c00\n", "0 903c7\n",
                             "0 903cg0\n", "0 903c7f ignored\n" })
    {
        bool rejected = false;
        try { std::istringstream bad(text); (void) readEvents(bad); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected)
            throw std::runtime_error("invalid event input was accepted");
    }
    std::cout << "calibration event parser self-check passed\n";
}
} // namespace

int main(int argc, char** argv)
{
    const bool selfCheck = argc == 2 && std::string(argv[1]) == "--self-test";
    if (!selfCheck && argc != 3 && argc != 4)
    {
        std::cerr << "usage: " << argv[0]
                  << " <seconds-hex-events.txt> <output.wav> [character 0..2]\n";
        return 2;
    }
    try
    {
        if (selfCheck)
        {
            selfTest();
            return 0;
        }
        float character = 1.0f;
        if (argc == 4)
        {
            std::string value(argv[3]);
            std::size_t used;
            character = std::stof(value, &used);
            if (used != value.size() || !std::isfinite(character)
                || character < 0.0f || character > 2.0f)
                throw std::runtime_error("character must be a finite value in 0..2");
        }
        std::ifstream input(argv[1]);
        if (!input)
            throw std::runtime_error("cannot open event file");
        const auto events = readEvents(input);
        YouKnowEngine engine;
        engine.selectConverterTimingProfile(
            YouKnowEngine::ConverterTimingProfile::MeasuredChartGeometry);
        engine.prepare(comparisonSampleRate, comparisonBlockSize, 4);
        sysex::Patch patch;
        bool havePatch = false;
        StereoBuffer audio;
        audio.left.resize(events.back().frame + 2 * comparisonSampleRate);
        audio.right.resize(audio.left.size());
        std::size_t cursor = 0;
        const auto renderUntil = [&](std::size_t end) {
            while (cursor < end)
            {
                const auto count = std::min<std::size_t>(comparisonBlockSize, end - cursor);
                engine.process(audio.left.data() + cursor, audio.right.data() + cursor,
                               static_cast<int>(count));
                cursor += count;
            }
        };
        for (const auto& event : events)
        {
            renderUntil(event.frame);
            const auto& bytes = event.bytes;
            int channel, parameter, value;
            if (sysex::readPatchMessage(bytes.data(), bytes.size(), patch, channel))
            {
                havePatch = true;
                engine.setParameters(parametersFor(patch, character));
            }
            else if (havePatch && sysex::readParameterMessage(
                         bytes.data(), bytes.size(), parameter, value, channel)
                     && sysex::applyParameter(patch, parameter, value))
                engine.setParameters(parametersFor(patch, character));
            else if (havePatch && bytes.size() == 3 && bytes[1] < 128 && bytes[2] < 128
                     && ((bytes[0] & 0xf0) == 0x80 || (bytes[0] & 0xf0) == 0x90))
            {
                if ((bytes[0] & 0xf0) == 0x90 && bytes[2] != 0)
                    engine.noteOn(bytes[1], bytes[2] / 127.0f);
                else
                    engine.noteOff(bytes[1]);
            }
            else
                throw std::runtime_error("unsupported MIDI or missing initial patch at frame "
                                         + std::to_string(event.frame));
        }
        renderUntil(audio.left.size());
        std::string error;
        if (!writeFloatWav(argv[2], audio, error))
            throw std::runtime_error(error);
        std::cout << events.size() << " events, "
                  << audio.left.size() / double(comparisonSampleRate)
                  << " seconds, character " << character
                  << ", 48 kHz/4x, Exact/Merson, volume 1, peak "
                  << decibels(measure(audio).peak) << " dBFS\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

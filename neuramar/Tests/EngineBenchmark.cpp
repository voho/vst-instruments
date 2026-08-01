// Deterministic render-cost benchmark for the Neuramar engine.
//
// It is not a correctness test: it reports the wall-clock cost of rendering a
// fixed musical workload so that render-path changes can be compared. Build it
// with the ordinary Release configuration and run it directly.

#include "DSP/NeuralModel.h"
#include "DSP/NeuramarEngine.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
struct Scenario
{
    const char* name;
    int firstNote;
    int noteCount;
    bool bodyLayers { true };
};

neuramar::EngineParameters makeParameters(bool bodyLayers)
{
    neuramar::EngineParameters parameters;
    parameters.imprint = 0.9f;
    parameters.bodyLock = 0.52f;
    parameters.air = bodyLayers ? 0.82f : 0.0f;
    parameters.bone = bodyLayers ? 0.78f : 0.0f;
    parameters.brightness = 0.5f;
    parameters.mutation = 0.12f;
    parameters.spread = 0.58f;
    parameters.releaseSeconds = 0.65f;
    parameters.outputGain = 0.5f;
    return parameters;
}

double renderSeconds(const neuramar::NeuralModel& model,
                     const Scenario& scenario, double sampleRate,
                     int blockSize, double renderedSeconds, double& checksum)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setModel(&model);

    const auto parameters = makeParameters(scenario.bodyLayers);
    engine.setParameters(parameters);

    for (int index = 0; index < scenario.noteCount; ++index)
        engine.noteOn(scenario.firstNote + 4 * index, 0.8f);

    std::vector<float> left(static_cast<std::size_t>(blockSize), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(blockSize), 0.0f);
    const auto blocks = static_cast<int>(
        renderedSeconds * sampleRate / static_cast<double>(blockSize));

    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
        engine.process(left.data(), right.data(), blockSize);
    const auto finish = std::chrono::steady_clock::now();

    for (int index = 0; index < blockSize; ++index)
        checksum += static_cast<double>(left[static_cast<std::size_t>(index)])
            + right[static_cast<std::size_t>(index)];

    return std::chrono::duration<double>(finish - start).count();
}

// Note-on runs on the audio thread and maps every rendered partial's onset
// phase, so its cost belongs in the same budget as the render loop.
double noteOnMicroseconds(const neuramar::NeuralModel& model, int note,
                          double sampleRate, int blockSize)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setModel(&model);
    engine.setParameters(makeParameters(true));

    constexpr int repeats = 2000;
    for (int index = 0; index < 64; ++index)
        engine.noteOn(note, 0.8f);
    engine.allSoundOff();

    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < repeats; ++index)
        engine.noteOn(note, 0.8f);
    const auto finish = std::chrono::steady_clock::now();
    engine.allSoundOff();
    return 1.0e6 * std::chrono::duration<double>(finish - start).count()
        / static_cast<double>(repeats);
}
} // namespace

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr double renderedSeconds = 8.0;

    const auto model = neuramar::NeuralModel::createRandom(0x9e3779b97f4a7c15ull,
                                                           0.6f);
    if (model == nullptr)
    {
        std::puts("benchmark: could not create a model");
        return 1;
    }

    const Scenario scenarios[] {
        { "low chord (C1 root, 8 voices)", 24, 8, true },
        { "mid chord (C3 root, 8 voices)", 48, 8, true },
        { "high chord (C6 root, 8 voices)", 84, 8, true },
        { "high chord, Air and Bone at zero", 84, 8, false },
        { "single mid note", 60, 1, true },
    };

    double checksum = 0.0;
    std::printf("%-34s %10s %10s\n", "scenario", "seconds", "x realtime");
    for (const auto& scenario : scenarios)
    {
        // One discarded pass warms the caches and the branch predictors.
        (void) renderSeconds(*model, scenario, sampleRate, blockSize, 0.5,
                             checksum);
        const double elapsed = renderSeconds(*model, scenario, sampleRate,
                                             blockSize, renderedSeconds,
                                             checksum);
        std::printf("%-34s %10.4f %10.1f\n", scenario.name, elapsed,
                    renderedSeconds / elapsed);
    }
    std::printf("%-34s %10.3f %10s\n", "note-on, C1 (us)",
                noteOnMicroseconds(*model, 24, sampleRate, blockSize), "-");
    std::printf("%-34s %10.3f %10s\n", "note-on, C4 (us)",
                noteOnMicroseconds(*model, 60, sampleRate, blockSize), "-");
    std::printf("checksum %.6f\n", checksum);
    return 0;
}

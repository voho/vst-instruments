#pragma once

#include "NeuralModel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace neuramar
{

class SampleLearner final
{
public:
    enum class Stage
    {
        Conditioning,
        Pitch,
        Analysis,
        Training
    };

    struct Progress
    {
        Stage stage { Stage::Conditioning };
        float overallProgress { 0.0f };
        float rootFrequencyHz { 0.0f };
        int rootMidiNote { 60 };
        float rootCents { 0.0f };
        float pitchConfidence { 0.0f };
        std::string message;
    };

    using ProgressCallback = std::function<void(const Progress&)>;
    using CancelPredicate = std::function<bool()>;

    struct LearnResult
    {
        std::unique_ptr<NeuralModel> model;
        bool cancelled { false };
        std::string error;
        float initialLoss { 0.0f };
        float finalLoss { 0.0f };
        int trainingEpochs { 0 };

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return model != nullptr && !cancelled && error.empty();
        }
    };

    // Offline and deterministic. The caller is expected to run this method on
    // a worker thread; callbacks are invoked on that same thread.
    [[nodiscard]] static LearnResult
        learn(const std::vector<float>& monoSample,
              double sampleRate,
              ProgressCallback progressCallback = {},
              CancelPredicate cancelPredicate = {});

    // The same band-limited resampler the learning pass uses, exposed so the
    // regression suite can check it against a direct kernel evaluation. It is
    // not part of the plug-in's runtime surface.
    [[nodiscard]] static std::vector<float>
        resampleForTests(const std::vector<float>& input,
                         double sourceRate, double destinationRate);

    // The Air-fit exclusion test that keeps a subtracted partial's leakage out
    // of the noise-floor measurement, exposed so the regression suite can
    // drive its own hostile-input guard directly. It is not part of the
    // plug-in's runtime surface.
    [[nodiscard]] static bool
        belongsToSubtractedHarmonicForTests(float frequencyHz,
                                            float analysisBinWidth,
                                            float rootFrequencyHz,
                                            float inharmonicity);

    // belongsToSubtractedHarmonic's own entry guard, exposed on its own so the
    // regression suite can assert it directly: the parent function's later
    // checks independently reject every hostile root or bin width this guard
    // rejects, so no input to the parent function can demonstrate this guard
    // mattering through its return value alone. It is not part of the
    // plug-in's runtime surface.
    [[nodiscard]] static bool
        hasUsableRootAndBinWidthForTests(float rootFrequencyHz,
                                         float analysisBinWidth);

    // The Air-fit exclusion test that keeps an actively-resonating Bone
    // mode's own energy out of the noise-floor measurement, exposed so the
    // regression suite can drive its per-mode reliability skip directly: a
    // mode findPersistentBoneModes() could not vouch for keeps a fallback
    // ratio in the selection but a reliability of exactly 0, and this
    // function must treat that slot as inactive no matter how closely a
    // probe frequency lands on its fallback ratio. It is not part of the
    // plug-in's runtime surface.
    [[nodiscard]] static bool belongsToActiveBoneForTests(
        float frequencyHz, float analysisBinWidth,
        const std::array<float, NeuralModel::boneModeCount>& ratios,
        const std::array<float, NeuralModel::boneModeCount>& reliabilities,
        float rootFrequencyHz);
};

} // namespace neuramar

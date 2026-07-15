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
};

} // namespace neuramar

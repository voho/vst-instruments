// Neuramar's local synthesis model: a learned controller over explicit
// harmonic, stochastic and modal synthesizers.
//
// WHY THIS SHAPE. Neuramar fits a compact model on the user's own machine in
// seconds, with no pretrained weights, no dataset and no GPU. That budget is
// what rules out the alternatives rather than any judgement about them:
//   - RAVE (Caillon and Esling, 2021, https://arxiv.org/abs/2111.05011) is a
//     trained autoencoder, not a bounded one-sample fitting method.
//   - Jin et al., DAFx-23
//     (https://www.dafx.de/paper-archive/details/zWyI3tFAzaieWGQuM45sEg)
//     adapts a pretrained decoder and reports minutes of GPU adaptation.
//   - Neural codecs -- SoundStream
//     (https://research.google/pubs/soundstream-an-end-to-end-neural-audio-codec/)
//     and EnCodec (https://github.com/facebookresearch/encodec) -- reconstruct
//     audio rather than yielding a playable instrument.
//
// Neuramar instead takes the factorisation established by DDSP
// (https://openreview.net/forum?id=B1x1ma4tDr) as a design prior: a learned
// controller driving explicit synthesizers, so every parameter the model
// produces stays inspectable and playable. Differentiable Wavetable Synthesis
// (https://arxiv.org/abs/2111.10003) shows why a small explicit basis
// generalises where a black-box decoder does not.
//
// THE BRANCHES.
//   Harmonic Core -- the harmonic-oscillator factorization of DDSP section 3.1
//     (https://arxiv.org/abs/2001.04643), which removes above-Nyquist harmonics
//     and then normalizes the surviving distribution:
//     https://github.com/magenta/ddsp/blob/main/ddsp/core.py#L784-L794
//     Stiff-string inharmonicity follows Fletcher's law,
//     https://doi.org/10.1121/1.1908504
//   Stochastic Air -- narrow explicit noise bands, following NoiseBandNet's
//     motivation (https://arxiv.org/abs/2307.08007) over the
//     harmonic-plus-residual decomposition of Serra and Smith's spectral
//     modelling synthesis (https://doi.org/10.2307/3680788).
//   Modal Bone -- a small modal branch for inharmonic candidate peaks,
//     inspired by Differentiable Modal Synthesis,
//     https://proceedings.neurips.cc/paper_files/paper/2024/file/0232cafe8d1909a01019abe8af32f3e1-Paper-Conference.pdf
//
// SPECTRAL ENVELOPE AND FORMANT CONTROL. Preserving an envelope in absolute
// frequency while pitch moves follows the source-filter motivation in Schwarz
// and Rodet's spectral-envelope work,
// https://quod.lib.umich.edu/i/icmc/bbp2372.1999.417?rgn=main;view=fulltext
// Interpolating between measured peaks has direct precedent in Jensen and
// Hansen (https://crss.utdallas.edu/Publications/Jensen2001.pdf), and the slope
// limiting follows the local shape-preserving construction of Fritsch and
// Butland, https://doi.org/10.1137/0905021
//
// ANALYSIS. Pitch estimation rests on the YIN basis, de Cheveigne and Kawahara
// 2002, https://pubmed.ncbi.nlm.nih.gov/12002874/ -- no pretrained pitch model
// or dataset is bundled. (SwiftF0 is a promising small modern estimator,
// https://arxiv.org/abs/2508.18440 with an implementation at
// https://github.com/lars76/swift-f0, but bundling it would mean shipping
// weights.) Harmonic parameters are solved jointly by least squares over the
// analysis aperture; see Smith, Spectral Audio Signal Processing,
// https://ccrma.stanford.edu/~jos/sasp/Least_Squares_Sinusoidal_Parameter.html
//
// THE CONTROLLER. The input encoding follows the motivation of Fourier Features
// (https://arxiv.org/abs/2006.10739). Neuramar uses ordinary tanh hidden units
// rather than the sinusoidal activations proposed by SIREN
// (https://arxiv.org/abs/2006.09661), and inference is deliberately at control
// rate rather than per sample.
//
// Filter behaviour under modulation is analysed by Parker, Zavalishin and
// Le Bivic,
// https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf
// Frequency-dependent damping of noise follows Frontiers in Signal Processing,
// 2024,
// https://www.frontiersin.org/journals/signal-processing/articles/10.3389/frsip.2024.1494864/full
//
// What the model does and does not claim is set out in the README's "How it
// works" and "Known gaps".

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace neuramar
{

// Stiff-string partial series f(n) = n * f0 * sqrt(1 + B n^2). B is zero for an
// ideal harmonic source, small for wound strings and tines, and larger for
// struck bars. Returning the plain harmonic number for B = 0 keeps every
// pre-existing model bit-identical.
[[nodiscard]] inline float stretchedHarmonicRatio(
    float harmonicNumber, float inharmonicity) noexcept
{
    if (!(inharmonicity > 0.0f) || !std::isfinite(inharmonicity)
        || !std::isfinite(harmonicNumber))
        return harmonicNumber;
    return harmonicNumber * std::sqrt(
        1.0f + inharmonicity * harmonicNumber * harmonicNumber);
}

struct SynthesisFrame
{
    static constexpr std::size_t harmonicCount = 64;
    // Sixteen log-spaced Air bands cover 80 Hz to 16 kHz at 0.51 octaves each.
    // The previous eight were 1.03 octaves apiece, which put a breath formant,
    // a scrape peak, and a hiss shelf inside one number.
    static constexpr std::size_t airBandCount = 16;
    // Twelve modal candidates. Six could not hold a struck body: a bell or a
    // tine has more independently decaying partials than that.
    static constexpr std::size_t boneModeCount = 12;

    std::array<float, harmonicCount> harmonicAmplitudes {};
    std::array<float, airBandCount> airAmplitudes {};
    std::array<float, boneModeCount> boneAmplitudes {};
    float pitchRatio { 1.0f };
};

class NeuralModel final
{
public:
    // Version 2 introduced renderer-matched overlapping Air bands. Version 3
    // adds a bounded, quantised residual trajectory on top of the neural base.
    // Version 4 appends the fitted stiff-string inharmonicity coefficient.
    // Version 5 widens the body representation from 8 Air bands and 6 Bone
    // modes to 16 and 12, which changes the decoder's output layout.
    // Versions 2, 3, and 4 still load and render exactly as they did: their
    // bands and modes occupy the low slots, the added slots are decoded as
    // silent, and the missing fields stay at their neutral zero values.
    // Version-1 development memories used different band semantics and must not
    // be silently rendered with this engine.
    static constexpr std::uint32_t currentFormatVersion = 5;
    // The Air/Bone counts every version before 5 was written with, and the
    // decoder output size that follows from them.
    static constexpr std::size_t legacyAirBandCount = 8;
    static constexpr std::size_t legacyBoneModeCount = 6;
    // A bounded stiff-string coefficient. 0.01 already stretches the 16th
    // partial by roughly eleven semitones, which is far past any ordinary
    // wound string or tine; strongly inharmonic bodies are represented by the
    // explicit Bone modes instead.
    static constexpr float maximumInharmonicity = 0.01f;
    static constexpr std::size_t previewSize = 256;
    static constexpr std::size_t harmonicCount = SynthesisFrame::harmonicCount;
    static constexpr std::size_t airBandCount = SynthesisFrame::airBandCount;
    static constexpr std::size_t boneModeCount = SynthesisFrame::boneModeCount;
    static constexpr std::size_t inputSize = 10;
    static constexpr std::size_t hiddenSize = 32;
    static constexpr std::size_t amplitudeOutputSize = harmonicCount
        + airBandCount + boneModeCount;
    static constexpr std::size_t pitchOutputIndex = amplitudeOutputSize;
    static constexpr std::size_t outputSize = amplitudeOutputSize + 1;
    static constexpr std::size_t legacyOutputSize = harmonicCount
        + legacyAirBandCount + legacyBoneModeCount + 1;

    // Where a pre-version-5 decoder output lands in the current layout. The
    // harmonic block and the first eight Air slots keep their index; the six
    // Bone slots move past the eight added Air slots; the pitch output moves to
    // the end.
    [[nodiscard]] static constexpr std::size_t
        migrateOutputIndex(std::size_t legacyIndex) noexcept
    {
        if (legacyIndex < harmonicCount + legacyAirBandCount)
            return legacyIndex;
        if (legacyIndex < harmonicCount + legacyAirBandCount
                + legacyBoneModeCount)
            return legacyIndex + (airBandCount - legacyAirBandCount);
        return pitchOutputIndex;
    }

    struct Metadata
    {
        double sourceSampleRate { 0.0 };
        float rootFrequencyHz { 0.0f };
        int rootMidiNote { 60 };
        float rootCents { 0.0f };
        float pitchConfidence { 0.0f };
        float durationSeconds { 0.0f };
        float loopStartSeconds { 0.0f };
        float loopEndSeconds { 0.0f };
        float sourcePeak { 0.0f };
        float sourceRms { 0.0f };
        float initialLoss { 0.0f };
        float finalLoss { 0.0f };
        int trainingEpochs { 0 };
        std::array<float, previewSize> waveformPreview {};
    };

    NeuralModel(const NeuralModel&) = delete;
    NeuralModel& operator=(const NeuralModel&) = delete;
    NeuralModel(NeuralModel&&) noexcept = default;
    NeuralModel& operator=(NeuralModel&&) noexcept = default;
    ~NeuralModel() = default;

    [[nodiscard]] const Metadata& metadata() const noexcept { return metadata_; }
    [[nodiscard]] float rootFrequencyHz() const noexcept { return metadata_.rootFrequencyHz; }
    [[nodiscard]] int rootMidiNote() const noexcept { return metadata_.rootMidiNote; }
    [[nodiscard]] float rootCents() const noexcept { return metadata_.rootCents; }
    [[nodiscard]] float pitchConfidence() const noexcept { return metadata_.pitchConfidence; }
    [[nodiscard]] float durationSeconds() const noexcept { return metadata_.durationSeconds; }
    [[nodiscard]] float finalLoss() const noexcept { return metadata_.finalLoss; }
    // Fitted stiff-string coefficient B. Zero means an ideal harmonic series
    // and is what every version-2 and version-3 memory reports.
    [[nodiscard]] float inharmonicity() const noexcept
    {
        return inharmonicity_;
    }
    [[nodiscard]] std::span<const float, airBandCount>
        airCentreFrequenciesHz() const noexcept { return airCentreFrequenciesHz_; }
    [[nodiscard]] std::span<const float, airBandCount>
        airBandwidthOctaves() const noexcept { return airBandwidthOctaves_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneFrequencyRatios() const noexcept { return boneFrequencyRatios_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneDecaySeconds() const noexcept { return boneDecaySeconds_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneModeReliabilities() const noexcept { return boneModeReliabilities_; }

    // Evaluates the compact time-conditioned network. The time argument is
    // normalised to the learned sample duration and is clamped to [0, 1].
    void evaluate(float normalisedTime, SynthesisFrame& destination) const noexcept;

    // Evaluates a nearby point on the model's learned time manifold. The
    // latent coordinate is bounded to [-1, 1]; zero is exactly equivalent to
    // the legacy two-argument evaluation path.
    void evaluate(float normalisedTime, float latentCoordinate,
                  SynthesisFrame& destination) const noexcept;

    // Creates a self-contained, playable neural memory without analysing a
    // sample. The seed is the only source of entropy and strength linearly
    // selects how much of the musically bounded randomisation range is used.
    [[nodiscard]] static std::unique_ptr<NeuralModel>
        createRandom(std::uint64_t seed, float strength);

    // Returns a deep, immutable variation of this model. Root pitch, duration,
    // and loop timing are preserved; the original model is never modified.
    [[nodiscard]] std::unique_ptr<NeuralModel>
        createRandomizedVariation(std::uint64_t seed, float strength) const;

    // The binary form is fixed-layout, checksummed, explicitly little-endian,
    // and bounded. Deserialisation never accepts trailing or oversized data.
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    [[nodiscard]] static std::unique_ptr<NeuralModel>
        deserialize(std::span<const std::uint8_t> bytes,
                    std::string* error = nullptr);

private:
    NeuralModel() = default;

    static constexpr std::uint32_t bandFormatVersion = 2;
    static constexpr std::uint32_t residualFormatVersion = 3;
    static constexpr std::uint32_t stretchFormatVersion = 4;
    static constexpr std::uint32_t bodyCapacityFormatVersion = 5;
    static constexpr std::size_t maximumResidualKeyframes = 128;
    static constexpr float maximumResidualScale = 64.0f;

    // normalisedTime must already be finite and within [0, 1]; every call
    // site clamps it before calling in (evaluate() clamps its own argument
    // just above the call, the randomised-variation calibration loop and
    // SampleLearner's residual passes feed frame fractions and
    // already-clamped residualTimes_ entries) so this hot path - one call per
    // control frame per voice, plus every calibration and residual-fitting
    // frame - does not re-check a precondition its only callers already
    // guarantee.
    void evaluateBaseRaw(
        float normalisedTime,
        std::array<float, outputSize>& destination) const noexcept;
    [[nodiscard]] std::unique_ptr<NeuralModel> clone() const;
    void refreshWaveformPreview(float blend) noexcept;

    Metadata metadata_ {};
    std::array<float, harmonicCount> initialHarmonicPhases_ {};
    std::array<float, airBandCount> airCentreFrequenciesHz_ {};
    std::array<float, airBandCount> airBandwidthOctaves_ {};
    std::array<float, boneModeCount> boneFrequencyRatios_ {};
    std::array<float, boneModeCount> boneDecaySeconds_ {};
    std::array<float, boneModeCount> boneModeReliabilities_ {};
    std::array<float, boneModeCount> initialBonePhases_ {};
    std::array<float, outputSize> outputMeans_ {};
    std::array<float, outputSize> outputScales_ {};
    std::array<float, hiddenSize * inputSize> inputWeights_ {};
    std::array<float, hiddenSize> hiddenBiases_ {};
    std::array<float, outputSize * hiddenSize> outputWeights_ {};
    std::array<float, outputSize> outputBiases_ {};
    float inharmonicity_ { 0.0f };
    std::uint32_t residualKeyframeCount_ { 0 };
    std::array<float, outputSize> residualScales_ {};
    std::array<float, maximumResidualKeyframes> residualTimes_ {};
    std::array<std::int16_t,
               maximumResidualKeyframes * outputSize> residualValues_ {};

    friend class SampleLearner;
    friend class NeuramarEngine;
    friend struct NeuralModelTrainingAccess;
};

// The compact network's ten fixed input features at a normalised time
// already clamped to [0, 1]: a centred position and its square, three
// harmonics of a normalised-time sine/cosine pair, and two asymmetric decay
// windows in seconds. NeuralModel::evaluateBaseRaw() and the trainer's
// forward pass in SampleLearner.cpp both need this exact ten-way mapping, so
// it is resolved once here instead of two independently maintained copies of
// the same expressions.
[[nodiscard]] inline std::array<float, NeuralModel::inputSize>
    networkInputsAt(float clampedNormalisedTime, float durationSeconds) noexcept
{
    constexpr float twoPi = 6.28318530717958647692f;
    const float time = clampedNormalisedTime;
    const float centred = 2.0f * time - 1.0f;
    const float timeSeconds = time * std::max(durationSeconds, 0.0f);
    return { centred, centred * centred,
             std::sin(twoPi * time), std::cos(twoPi * time),
             std::sin(2.0f * twoPi * time), std::cos(2.0f * twoPi * time),
             std::sin(4.0f * twoPi * time), std::cos(4.0f * twoPi * time),
             std::exp(-8.0f * timeSeconds),
             std::exp(-40.0f * timeSeconds) };
}

// Log-spaced Air band centres and their shared per-band bandwidth across
// [lowerHz, upperHz]. Bands below `firstBand` are left untouched, which lets
// a partial layout - such as the version-5 legacy migration, which only adds
// bands past a pre-version-5 model's original count - fill in just the bands
// it owns. NeuralModel::generateModel(), that same migration in
// NeuralModel::deserialize(), and SampleLearner::learn()'s Air geometry all
// need this exact geometric spacing, so it is resolved once here instead of
// three independently maintained copies of the same std::pow/log2 pair.
template <std::size_t Count>
inline void layoutAirBands(float lowerHz, float upperHz,
    std::array<float, Count>& centresHz,
    std::array<float, Count>& widthOctaves,
    std::size_t firstBand = 0) noexcept
{
    const float edgeRatio = std::pow(
        upperHz / lowerHz, 1.0f / static_cast<float>(Count));
    const float width = std::log2(edgeRatio);
    for (std::size_t band = firstBand; band < Count; ++band)
    {
        centresHz[band] = lowerHz * std::pow(
            edgeRatio, static_cast<float>(band) + 0.5f);
        widthOctaves[band] = width;
    }
}

} // namespace neuramar

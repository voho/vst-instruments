#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace electry
{

// Three complete amplifier and loudspeaker paths. The names describe circuit
// families rather than claiming an emulation of a particular trademarked
// product: a wide, feedback-controlled American combo; a mid-forward British
// head and closed cabinet; and Electry's original tight modern metal stack.
enum class AmpModel
{
    AmericanClean = 0,
    BritishCrunch = 1,
    ModernHighGain = 2,
};

// The five FX amount controls plus the Amp Voice selector. Exactly zero is a
// bit-exact dry bypass for each amount: no
// filter, oversampler or recirculated tail reaches the output, so the authentic
// dry DI is always available. Distortion and Amp are drive amounts once their
// circuits are enabled; unlike a parallel blend, an enabled amplifier always
// keeps its cabinet between the strings and the output.
struct FxParameters
{
    float distortion { 0.0f }; // pedal drive ahead of the amp
    float amp { 0.0f };        // amplifier drive into a modelled cabinet
    AmpModel ampModel { AmpModel::ModernHighGain };
    float compressor { 0.0f }; // fast rhythm levelling
    float delay { 0.0f };      // 360 ms lead delay with darkening repeats
    float room { 0.0f };       // compact stereo ambience
};

// Electry's post-string signal chain: the distortion pedal, the amplifier and
// its cabinet, the rhythm compressor, the lead delay and the room.
//
// The two nonlinear blocks run inside an oversampled domain. A high-gain stage
// fed at host rate folds its own upper harmonics straight back into the guitar
// band, which is exactly the "fizz" that makes a modelled metal tone read as
// digital rather than as an amplifier; a Drop-E eighth string driving
// twenty-five times gain generates intermodulation products far above host
// Nyquist. The cabinet's steep low-pass sits inside the same oversampled
// domain, so the alias-generating content is removed before decimation rather
// than after it.
//
// The whole class is JUCE-free, so the complete signal path - strings, pickups
// and amplifier alike - builds and is regression tested on every platform.
class ElectryFx
{
public:
    // Allocates the delay and ambience memory; call from prepareToPlay, never
    // from the audio thread.
    void prepare(double sampleRate);
    void reset() noexcept;
    void setParameters(const FxParameters& parameters) noexcept;
    void process(float* left, float* right, int numSamples) noexcept;

    // True while the oversampled gain stage is running. With both gain
    // controls at zero the block is skipped outright, so the chain costs
    // nothing and adds no group delay at all.
    [[nodiscard]] bool isGainStageEngaged() const noexcept
    {
        return gainEngagement_ > 0.0f;
    }

    // Fixed group delay of the interpolating and decimating halfband chain, in
    // host samples, while the gain block is engaged: 6 + 3 + 11/4 + 11/2 at
    // 4x, 6 + 11/2 at 2x, none when the host already runs fast enough that the
    // gain stages need no detour.
    [[nodiscard]] float gainStageLatencySamples() const noexcept;

private:
    // The JUCE-free regression suite measures the halfband kernel's response,
    // and reads the sanitised parameter targets, through this narrow seam. It
    // is not part of the plug-in API.
    friend struct ElectryFxTestAccess;

    static constexpr int maximumOversamplingStages = 2;
    static constexpr int maximumOversampledFrames = 4;
    static constexpr double minimumSampleRate = 8000.0;
    static constexpr double maximumSampleRate = 384000.0;

    // Double coefficients and double state. The speaker/cabinet voices' low
    // high-pass corners are evaluated as high as 384 kHz; float coefficient
    // cancellation would materially move them, so the string engine's modal
    // bank uses double for the same reason.
    struct Biquad
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
        double z1 { 0.0 }, z2 { 0.0 };

        void reset() noexcept { z1 = z2 = 0.0; }
        void setLowpass(float frequencyHz, float q, float sampleRate) noexcept;
        void setHighpass(float frequencyHz, float q, float sampleRate) noexcept;
        void setPeaking(float frequencyHz, float q, float gainDb,
                        float sampleRate) noexcept;

        // Transposed direct form II: two states and the numerically better
        // behaved arrangement for a resonant peak.
        float process(float input) noexcept
        {
            const double x = static_cast<double>(input);
            const double output = b0 * x + z1;
            z1 = b1 * x - a1 * output + z2;
            z2 = b2 * x - a2 * output;
            return static_cast<float>(output);
        }
    };

    struct OnePole
    {
        float state { 0.0f };
        void reset() noexcept { state = 0.0f; }
        float process(float input, float coefficient) noexcept
        {
            state += (1.0f - coefficient) * (input - state);
            return state;
        }
    };

    // Exact bilinear transform of the third-order passive tone-stack transfer
    // derived by Yeh and Smith. Coefficients and state stay in double because
    // the analogue RC time constants map to tightly clustered poles when the
    // circuit runs at Electry's 192-384 kHz nonlinear-stage rate.
    struct ToneStack
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, b3 { 0.0 };
        double a1 { 0.0 }, a2 { 0.0 }, a3 { 0.0 };
        double z1 { 0.0 }, z2 { 0.0 }, z3 { 0.0 };

        void reset() noexcept { z1 = z2 = z3 = 0.0; }
        void design(double c1, double c2, double c3,
                    double r1, double r2, double r3, double r4,
                    double treble, double middle, double bass,
                    double sampleRate) noexcept;
        float process(float input) noexcept
        {
            const double x = static_cast<double>(input);
            const double output = b0 * x + z1;
            z1 = b1 * x - a1 * output + z2;
            z2 = b2 * x - a2 * output + z3;
            z3 = b3 * x - a3 * output;
            return static_cast<float>(output);
        }
    };

    // One 2:1 halfband stage. A halfband kernel has h[0] = 0.5 and an exactly
    // zero tap at every even distance from the centre, so an interpolated pair
    // costs one delayed read plus one symmetric convolution over the odd taps,
    // and a decimated sample costs the same convolution. The kernel is
    // designed from a Kaiser window rather than tabulated, so its rejection
    // and its exactly unity DC gain are testable properties instead of
    // transcribed constants.
    struct HalfbandStage
    {
        static constexpr int oddTapCount = 6;                  // h[1], h[3] .. h[11]
        static constexpr int span = 2 * oddTapCount - 1;       // 11
        static constexpr int historySize = 32;                 // > 2 * span + 1
        static constexpr int historyMask = historySize - 1;

        static_assert((historySize & historyMask) == 0,
                      "halfband history must be a power of two");
        static_assert(historySize > 2 * span + 1,
                      "halfband history must hold the full causal kernel span");

        std::array<float, oddTapCount> oddTaps {};
        float centreTap { 0.5f };
        std::array<float, historySize> history {};
        int writeIndex { 0 };

        void design(float kaiserBeta) noexcept;
        void reset() noexcept
        {
            history.fill(0.0f);
            writeIndex = 0;
        }

        // Newest sample first: `at(0)` is the sample pushed last.
        [[nodiscard]] float at(int samplesAgo) const noexcept
        {
            return history[static_cast<std::size_t>(
                (writeIndex - 1 - samplesAgo) & historyMask)];
        }

        void push(float input) noexcept
        {
            history[static_cast<std::size_t>(writeIndex)] = input;
            writeIndex = (writeIndex + 1) & historyMask;
        }

        // Zero-stuffed interpolation. The first output phase sees only the
        // centre tap, so it is a plain delayed sample; the second sees only the
        // odd taps. Both are defined here so they inline into the per-sample
        // chain, where they run once per channel per host sample.
        void upsample(float input, float& first, float& second) noexcept
        {
            push(input);
            first = 2.0f * centreTap * at(oddTapCount);
            float sum = 0.0f;
            for (int m = 1; m <= oddTapCount; ++m)
                sum += oddTaps[static_cast<std::size_t>(m - 1)]
                     * (at(oddTapCount + m - 1) + at(oddTapCount - m));
            second = 2.0f * sum;
        }

        // Feed both samples of a pair, then read one decimated output.
        [[nodiscard]] float decimate(float first, float second) noexcept
        {
            push(first);
            push(second);
            float sum = centreTap * at(span);
            for (int m = 1; m <= oddTapCount; ++m)
            {
                const int distance = 2 * m - 1;
                sum += oddTaps[static_cast<std::size_t>(m - 1)]
                     * (at(span + distance) + at(span - distance));
            }
            return sum;
        }
    };

    // A modulation-free multi-tap ambience. Three allpass diffusers feed two
    // damped comb resonators per channel, and the channel pair uses
    // deliberately coprime lengths, so the field decorrelates without any
    // chorus, pitch modulation or randomised phase.
    struct Allpass
    {
        std::vector<float> line;
        int writeIndex { 0 };
        float coefficient { 0.5f };

        void prepare(int lengthSamples);
        void reset() noexcept;
        float process(float input) noexcept
        {
            const auto size = static_cast<int>(line.size());
            if (size <= 0)
                return input;
            const auto index = static_cast<std::size_t>(writeIndex);
            const float delayed = line[index];
            const float value = input + coefficient * delayed;
            line[index] = value;
            writeIndex = writeIndex + 1 >= size ? 0 : writeIndex + 1;
            return delayed - coefficient * value;
        }
    };

    struct Comb
    {
        std::vector<float> line;
        int writeIndex { 0 };
        float feedback { 0.7f };
        float damping { 0.4f };
        float state { 0.0f };

        void prepare(int lengthSamples);
        void reset() noexcept;
        float process(float input) noexcept
        {
            const auto size = static_cast<int>(line.size());
            if (size <= 0)
                return input;
            const auto index = static_cast<std::size_t>(writeIndex);
            const float delayed = line[index];
            state += (1.0f - damping) * (delayed - state);
            line[index] = input + feedback * state;
            writeIndex = writeIndex + 1 >= size ? 0 : writeIndex + 1;
            return delayed;
        }
    };

    // One complete amplifier path. Every model owns its recursive state and
    // its already-designed coefficients, so changing model never rewrites a
    // live filter on the audio thread. During the short switch crossfade the
    // old and new circuits simply run beside one another.
    struct AmpChannel
    {
        Biquad inputHighpass {};
        Biquad inputVoice {};
        OnePole interstage {};
        ToneStack toneStack {};
        OnePole phaseInverterInput {};
        // Trapezoidal companion histories for the two PI-to-power-grid
        // coupling capacitors and a warm start for the coupled LTP solve. All
        // are deviations from the DC operating point,
        // so an ordinary zeroed reset is the exact quiescent circuit state.
        double phaseCurrentDelta { 0.0 };
        double couplingHistoryOne { 0.0 };
        double couplingHistoryTwo { 0.0 };
        double powerGridOffsetOne { 0.0 };
        double powerGridOffsetTwo { 0.0 };
        float bias { 0.0f };
        float sag { 0.0f };
        Biquad transformerHighpass {};
        OnePole flux {};
        OnePole negativeFeedback {};
        std::array<Biquad, 6> cabinet {};
        bool wasActive { false };

        void reset() noexcept;
    };

    // Everything the oversampled gain block needs for one channel.
    struct GainChannel
    {
        std::array<HalfbandStage, maximumOversamplingStages> interpolators {};
        std::array<HalfbandStage, maximumOversamplingStages> decimators {};

        Biquad pedalHighpass {};
        Biquad pedalVoice {};
        Biquad pedalTilt {};
        // Voltage and derivative of the 2.2 kOhm / 10 nF diode node. The
        // capacitor is part of the clipper circuit, so it replaces the old
        // post-waveshaper smoothing pole rather than adding another filter.
        double diodeVoltage { 0.0 };
        double diodeDerivative { 0.0 };
        bool pedalWasActive { false };

        std::array<AmpChannel, 3> amplifiers {};
        bool ampWasActive { false };

        void resetPedal() noexcept;
        void resetAmp() noexcept;
        void reset() noexcept;
    };

    void designFilters() noexcept;
    void updateDriveConstants() noexcept;
    // The output transformer's core, isolated as a pure function of the signal
    // and its flux state so the regression suite can measure it at the stage
    // rather than through the cabinet that follows it - which, being a
    // second-order high-pass at the box frequency, shapes a low tone and its
    // harmonics so differently from a mid one that a distortion figure taken
    // at the output measures the cabinet.
    [[nodiscard]] static float transformerFluxCoefficient(
        float sampleRate) noexcept;
    [[nodiscard]] static float transformerCore(OnePole& flux, float input,
                                               float coefficient) noexcept;
    [[nodiscard]] static float diodePairStep(double inputVolts,
                                             double sampleRate,
                                             double& outputVolts,
                                             double& previousDerivative) noexcept;
    [[nodiscard]] static double triodeCathodeCurrent(
        double plateVoltage, double gridToCathodeVoltage) noexcept;
    [[nodiscard]] static double triodeGridCurrent(
        double gridToCathodeVoltage) noexcept;
    [[nodiscard]] static double triodePlateCurrent(
        double plateVoltage, double gridToCathodeVoltage) noexcept;
    [[nodiscard]] static double solveTriodePlate(
        double gridToCathodeVoltage, double supplyVoltage,
        double& warmStart) noexcept;
    [[nodiscard]] static float triodeStage(double gridVoltage,
                                           double& plateVoltage) noexcept;
    [[nodiscard]] static float triodeStageLookup(double gridVoltage) noexcept;
    struct PhaseInverterResult
    {
        double output { 0.0 };
        double plateOne { 0.0 };
        double plateTwo { 0.0 };
        double cathode { 0.0 };
        double tail { 0.0 };
        double totalCurrent { 0.0 };
    };
    [[nodiscard]] static double phaseInverterPlateCurrent(
        AmpModel model, double plateToCathodeVoltage,
        double gridToCathodeVoltage) noexcept;
    [[nodiscard]] static PhaseInverterResult phaseInverterDirect(
        AmpModel model, double drive) noexcept;
    struct CoupledPhaseInverterResult
    {
        double gridOne { 0.0 };
        double gridTwo { 0.0 };
        double plateOne { 0.0 };
        double plateTwo { 0.0 };
        double capacitorCurrentOne { 0.0 };
        double capacitorCurrentTwo { 0.0 };
        double gridCurrentOne { 0.0 };
        double gridCurrentTwo { 0.0 };
        double totalCurrent { 0.0 };
        double maximumResidual { 0.0 };
    };
    [[nodiscard]] static double powerGridCurrentDirect(
        double gridVoltage) noexcept;
    [[nodiscard]] static double powerGridCurrentLookup(
        double gridVoltage) noexcept;
    [[nodiscard]] static CoupledPhaseInverterResult phaseInverterCoupledStep(
        AmpChannel& channel, AmpModel model, double drive,
        double sampleRate) noexcept;
    struct PowerTubeResult
    {
        double output { 0.0 };
        double supplyDemand { 0.0 };
    };
    struct PowerTubeDirectResult
    {
        double output { 0.0 };
        double supplyDemand { 0.0 };
        double screenVoltageOne { 0.0 };
        double screenVoltageTwo { 0.0 };
        double screenResidual { 0.0 };
    };
    [[nodiscard]] static double powerTubePlateCurrent(
        AmpModel model, double plateVoltage, double gridVoltage,
        double screenVoltage) noexcept;
    [[nodiscard]] static double powerTubeScreenCurrent(
        AmpModel model, double plateVoltage, double gridVoltage,
        double screenVoltage) noexcept;
    [[nodiscard]] static PowerTubeDirectResult powerTubePairDirect(
        AmpModel model, double commonDrive, double differentialDrive,
        double railScale) noexcept;
    [[nodiscard]] static PowerTubeResult powerTubePairLookup(
        AmpModel model, float commonDrive, float differentialDrive,
        float railScale) noexcept;
    [[nodiscard]] float renderGainStage(GainChannel& channel,
                                        float input) noexcept;
    [[nodiscard]] float renderGainFrame(GainChannel& channel,
                                        float input) noexcept;
    [[nodiscard]] float pedalStage(GainChannel& channel, float input) noexcept;
    [[nodiscard]] float ampStage(AmpChannel& channel, AmpModel model,
                                 float input) noexcept;
    [[nodiscard]] float blendedAmpStage(GainChannel& channel,
                                        float input) noexcept;

    FxParameters targetParameters_ {};
    float distortionDrive_ { 0.0f };
    float ampDrive_ { 0.0f };
    float pedalWet_ { 0.0f };
    float ampWet_ { 0.0f };
    std::array<float, 3> ampModelWeights_ { 0.0f, 0.0f, 1.0f };
    float compressorMix_ { 0.0f };
    float delayMix_ { 0.0f };
    float roomMix_ { 0.0f };
    float gainEngagement_ { 0.0f };

    // Values derived from the smoothed mixes once per host sample rather than
    // per channel per oversampled frame.
    float pedalDrive_ { 1.0f };
    float pedalMakeup_ { 1.0f };
    std::array<float, 3> ampDriveFirst_ { 1.0f, 1.0f, 1.0f };
    std::array<float, 3> ampDriveSecond_ { 1.0f, 1.0f, 1.0f };
    std::array<float, 3> ampMakeup_ { 1.0f, 1.0f, 1.0f };

    double sampleRate_ { 48000.0 };
    // A fast host already carries the bandwidth the gain stages need, so it is
    // given fewer halfband stages instead of an internal clock in the
    // megahertz; the string engine drops its own oversampling on the same
    // grounds.
    int oversamplingStages_ { maximumOversamplingStages };
    float oversampledRate_ { 192000.0f };
    float parameterCoefficient_ { 0.01f };
    float engagementCoefficient_ { 0.02f };
    std::array<float, 3> interstageCoefficient_ { 0.5f, 0.5f, 0.5f };
    std::array<float, 3> phaseInverterInputCoefficient_ {
        0.5f, 0.5f, 0.5f };
    float biasCoefficient_ { 0.001f };
    std::array<float, 3> sagAttack_ { 0.001f, 0.001f, 0.001f };
    std::array<float, 3> sagRelease_ { 0.0001f, 0.0001f, 0.0001f };
    std::array<float, 3> fluxCoefficient_ { 0.99f, 0.99f, 0.99f };
    std::array<float, 3> feedbackCoefficient_ { 0.5f, 0.5f, 0.5f };
    float compressorAttack_ { 0.0f };
    float compressorRelease_ { 0.0f };
    float compressorEnvelope_ { 0.0f };
    float delayFeedbackDamping_ { 0.5f };
    float delayFeedbackHighpass_ { 0.02f };
    bool prepared_ { false };

    std::array<GainChannel, 2> gain_ {};

    std::array<std::vector<float>, 2> delayLines_ {};
    std::array<int, 2> delayTaps_ {};
    std::array<OnePole, 2> delayDamping_ {};
    std::array<float, 2> delayHighpassState_ {};
    int delayWriteIndex_ { 0 };

    std::array<std::array<Allpass, 3>, 2> diffusers_ {};
    std::array<std::array<Comb, 2>, 2> combs_ {};
};

} // namespace electry

#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace electry
{

// The five FX-panel controls. Every one of them is a 0..1 mix in which exactly
// zero is a bit-exact dry bypass: no filter, no oversampler and no recirculated
// tail reaches the output, so the authentic dry DI is always available.
struct FxParameters
{
    float distortion { 0.0f }; // pedal-style clipping ahead of the amp
    float amp { 0.0f };        // cascaded gain stages into a modelled cabinet
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

    // Double coefficients and double state. The cabinet's 82 Hz high-pass can
    // be asked to run at 1.5 MHz on a fast host with the gain stage
    // oversampled, where float coefficient cancellation would materially move
    // its corner; the string engine's modal bank uses double for the same
    // reason.
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

    // Everything the oversampled gain block needs for one channel.
    struct GainChannel
    {
        std::array<HalfbandStage, maximumOversamplingStages> interpolators {};
        std::array<HalfbandStage, maximumOversamplingStages> decimators {};

        Biquad pedalHighpass {};
        Biquad pedalVoice {};
        Biquad pedalTilt {};
        OnePole pedalSmooth {};

        Biquad ampHighpass {};
        Biquad ampVoice {};
        OnePole interstage {};
        float bias { 0.0f };

        // The power stage's supply rail. A loud passage draws current the
        // supply cannot hold up, so the plate voltage droops - measured on
        // real amplifiers from around 350 V to around 250 V within a tenth of
        // a second, recovering over three to six tenths - and the stage's
        // headroom goes with it. The follower is deliberately asymmetric: the
        // reservoir discharges far faster than it recharges.
        float sag { 0.0f };

        // The output transformer. A core saturates at a flux limit, and flux
        // is the integral of the voltage, so the limit is a volt-second limit
        // and the low end reaches it first at the same level. `flux` is a
        // one-pole at the primary-inductance corner - unity at DC, falling as
        // 1/f above it, which is that integral normalised - and what the core
        // cannot carry is subtracted back out of the signal, so the stage is
        // transparent well above the corner and compresses and thickens
        // underneath it. The high-pass in front of it is the transformer's own
        // inability to pass DC, and it also keeps the bias drift's residue out
        // of the flux.
        Biquad transformerHighpass {};
        OnePole flux {};

        std::array<Biquad, 6> cabinet {};

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
    [[nodiscard]] float renderGainStage(GainChannel& channel,
                                        float input) noexcept;
    [[nodiscard]] float renderGainFrame(GainChannel& channel,
                                        float input) noexcept;
    [[nodiscard]] float pedalStage(GainChannel& channel, float input) noexcept;
    [[nodiscard]] float ampStage(GainChannel& channel, float input) noexcept;

    FxParameters targetParameters_ {};
    float distortionMix_ { 0.0f };
    float ampMix_ { 0.0f };
    float compressorMix_ { 0.0f };
    float delayMix_ { 0.0f };
    float roomMix_ { 0.0f };
    float gainEngagement_ { 0.0f };

    // Values derived from the smoothed mixes once per host sample rather than
    // per channel per oversampled frame.
    float pedalDrive_ { 1.0f };
    float pedalMakeup_ { 1.0f };
    float ampDriveFirst_ { 1.0f };
    float ampDriveSecond_ { 1.0f };
    float ampMakeup_ { 1.0f };

    double sampleRate_ { 48000.0 };
    // A fast host already carries the bandwidth the gain stages need, so it is
    // given fewer halfband stages instead of an internal clock in the
    // megahertz; the string engine drops its own oversampling on the same
    // grounds.
    int oversamplingStages_ { maximumOversamplingStages };
    float oversampledRate_ { 192000.0f };
    float parameterCoefficient_ { 0.01f };
    float engagementCoefficient_ { 0.02f };
    float interstageCoefficient_ { 0.5f };
    float biasCoefficient_ { 0.001f };
    float sagAttack_ { 0.001f };
    float sagRelease_ { 0.0001f };
    float fluxCoefficient_ { 0.99f };
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

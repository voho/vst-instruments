#include "DSP/ElectryFx.h"

#include <algorithm>
#include <cmath>

namespace electry
{
namespace
{
constexpr float twoPi = 6.283185307179586f;
constexpr double pi = 3.14159265358979323846;

float clampf(float value, float low, float high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}

float sanitiseMix(float value) noexcept
{
    if (! std::isfinite(value))
        return 0.0f;
    return clampf(value, 0.0f, 1.0f);
}

float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

float smoothStep(float value) noexcept
{
    value = clampf(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

// Zeroth-order modified Bessel function, used only by the halfband window
// design in prepare(). The series converges quickly for the betas a halfband
// kernel needs.
double besselI0(double x) noexcept
{
    const double halfSquare = 0.25 * x * x;
    double term = 1.0;
    double sum = 1.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= halfSquare / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < 1.0e-18 * sum)
            break;
    }
    return sum;
}

// A diode-pair clipper: unity slope at the origin, a firm bounded ceiling, and
// derivatives of every order. A hard clamp would be cheaper, but its corner
// radiates harmonics of unbounded order, which is precisely the content an
// oversampled stage exists to avoid generating in the first place.
float diodeClip(float x) noexcept
{
    return x / std::sqrt(1.0f + x * x);
}

// A triode stage's ceiling. It is deliberately one smooth curve rather than a
// different curve above and below the origin: selecting a knee by sign leaves a
// third-derivative kink at zero, and the near-square waveform a cascaded
// second stage sees traverses that kink at full slew, which radiates far more
// high-order content than the saturation itself. The stage's asymmetry comes
// from its operating point instead, which is where a real one's comes from.
float triode(float x) noexcept
{
    return x / std::sqrt(1.0f + 0.85f * x * x);
}

// Output trims, measured so that a loud Drop-E riff leaves each stage at
// roughly the level it entered with.
constexpr float pedalTrim = 1.25f;
constexpr float ampTrim = 3.20f;
} // namespace

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------

void ElectryFx::Biquad::setLowpass(float frequencyHz, float q,
                                   float sampleRate) noexcept
{
    const double rate = std::max(static_cast<double>(sampleRate), 1.0);
    const double omega = 2.0 * pi
        * std::clamp(static_cast<double>(frequencyHz), 10.0, 0.45 * rate) / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega)
                       / (2.0 * std::max(static_cast<double>(q), 0.05));
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - cosine) * 0.5 / a0;
    b1 = (1.0 - cosine) / a0;
    b2 = b0;
    a1 = -2.0 * cosine / a0;
    a2 = (1.0 - alpha) / a0;
}

void ElectryFx::Biquad::setHighpass(float frequencyHz, float q,
                                    float sampleRate) noexcept
{
    const double rate = std::max(static_cast<double>(sampleRate), 1.0);
    const double omega = 2.0 * pi
        * std::clamp(static_cast<double>(frequencyHz), 5.0, 0.45 * rate) / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega)
                       / (2.0 * std::max(static_cast<double>(q), 0.05));
    const double a0 = 1.0 + alpha;
    b0 = (1.0 + cosine) * 0.5 / a0;
    b1 = -(1.0 + cosine) / a0;
    b2 = b0;
    a1 = -2.0 * cosine / a0;
    a2 = (1.0 - alpha) / a0;
}

void ElectryFx::Biquad::setPeaking(float frequencyHz, float q, float gainDb,
                                   float sampleRate) noexcept
{
    const double rate = std::max(static_cast<double>(sampleRate), 1.0);
    const double amplitude = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double omega = 2.0 * pi
        * std::clamp(static_cast<double>(frequencyHz), 10.0, 0.45 * rate) / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega)
                       / (2.0 * std::max(static_cast<double>(q), 0.05));
    const double a0 = 1.0 + alpha / amplitude;
    b0 = (1.0 + alpha * amplitude) / a0;
    b1 = -2.0 * cosine / a0;
    b2 = (1.0 - alpha * amplitude) / a0;
    a1 = -2.0 * cosine / a0;
    a2 = (1.0 - alpha / amplitude) / a0;
}

void ElectryFx::HalfbandStage::design(float kaiserBeta) noexcept
{
    // Ideal halfband impulse response h[n] = sin(pi n / 2) / (pi n), which is
    // exactly zero at every even n and alternates sign at every odd n, tapered
    // by a Kaiser window. The odd taps are then normalised so the kernel sums
    // to exactly one: unity DC gain is what keeps the oversampled detour from
    // changing the level of the signal passing through it.
    const double beta = std::max(0.0, static_cast<double>(kaiserBeta));
    const double normaliser = besselI0(beta);
    const double edge = static_cast<double>(span) + 1.0;
    double sum = 0.0;
    std::array<double, oddTapCount> taps {};
    for (int m = 1; m <= oddTapCount; ++m)
    {
        const double distance = static_cast<double>(2 * m - 1);
        const double ratio = distance / edge;
        const double window = besselI0(beta * std::sqrt(1.0 - ratio * ratio))
                            / normaliser;
        const double sign = (m % 2) == 1 ? 1.0 : -1.0;
        const double value = window * sign / (pi * distance);
        taps[static_cast<std::size_t>(m - 1)] = value;
        sum += value;
    }

    centreTap = 0.5f;
    const double scale = sum > 1.0e-12 ? 0.25 / sum : 0.0;
    for (int m = 0; m < oddTapCount; ++m)
        oddTaps[static_cast<std::size_t>(m)] = static_cast<float>(
            taps[static_cast<std::size_t>(m)] * scale);
    reset();
}

void ElectryFx::Allpass::prepare(int lengthSamples)
{
    line.assign(static_cast<std::size_t>(std::max(1, lengthSamples)), 0.0f);
    writeIndex = 0;
}

void ElectryFx::Allpass::reset() noexcept
{
    std::fill(line.begin(), line.end(), 0.0f);
    writeIndex = 0;
}

void ElectryFx::Comb::prepare(int lengthSamples)
{
    line.assign(static_cast<std::size_t>(std::max(1, lengthSamples)), 0.0f);
    writeIndex = 0;
    state = 0.0f;
}

void ElectryFx::Comb::reset() noexcept
{
    std::fill(line.begin(), line.end(), 0.0f);
    writeIndex = 0;
    state = 0.0f;
}

void ElectryFx::GainChannel::reset() noexcept
{
    for (auto& stage : interpolators)
        stage.reset();
    for (auto& stage : decimators)
        stage.reset();
    pedalHighpass.reset();
    pedalVoice.reset();
    pedalTilt.reset();
    pedalSmooth.reset();
    ampHighpass.reset();
    ampVoice.reset();
    interstage.reset();
    bias = 0.0f;
    for (auto& section : cabinet)
        section.reset();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ElectryFx::prepare(double sampleRate)
{
    if (! std::isfinite(sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, minimumSampleRate, maximumSampleRate);

    // 4x up to 96 kHz, 2x up to 192 kHz, native above: the point of the detour
    // is a fixed absolute bandwidth for the clipping stages, and a host that
    // already provides it does not need to pay for the halfband chain.
    oversamplingStages_ = sampleRate_ <= 96000.0
        ? 2 : (sampleRate_ <= 192000.0 ? 1 : 0);
    oversampledRate_ = static_cast<float>(
        sampleRate_ * static_cast<double>(1 << oversamplingStages_));

    // 15 ms on the five mixes and 12 ms on the gain block's engagement ramp.
    // Both are per-sample: the previous chain read its controls once per block
    // and stepped, which is audible as zipper noise on an automated drive.
    parameterCoefficient_ = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.015 * sampleRate_));
    engagementCoefficient_ = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.012 * sampleRate_));

    // 18 ms of attack, not 3. A rhythm compressor that closes inside a pick
    // attack removes the very transient that makes a palm-muted part
    // percussive; letting the attack through and holding the sustain is what
    // makes a chug hit.
    compressorAttack_ = std::exp(-1.0f / static_cast<float>(0.018 * sampleRate_));
    compressorRelease_ = std::exp(-1.0f / static_cast<float>(0.090 * sampleRate_));

    // Repeats darken and thin as they recirculate, as an analogue or tape
    // delay's do; an undamped feedback path turns a lead delay into a metallic
    // stack of identical copies.
    const auto hostRate = static_cast<float>(sampleRate_);
    delayFeedbackDamping_ = std::exp(
        -twoPi * std::min(4200.0f, 0.40f * hostRate) / hostRate);
    delayFeedbackHighpass_ = 1.0f - std::exp(-twoPi * 150.0f / hostRate);

    interstageCoefficient_ = std::exp(
        -twoPi * std::min(6800.0f, 0.40f * oversampledRate_) / oversampledRate_);
    // 45 ms on the grid-bias follower: long enough that a held chord shifts the
    // operating point, short enough to recover between chugs.
    biasCoefficient_ = 1.0f - std::exp(-1.0f / (0.045f * oversampledRate_));

    for (auto& channel : gain_)
    {
        // Beta 8.6 over six odd taps rejects the images by better than 60 dB
        // while keeping the whole four-stage chain inside eighteen host samples
        // of group delay.
        for (auto& stage : channel.interpolators)
            stage.design(8.6f);
        for (auto& stage : channel.decimators)
            stage.design(8.6f);
    }

    designFilters();

    // 360 ms of lead delay plus the right channel's spread, with headroom.
    const auto delaySize = static_cast<std::size_t>(sampleRate_ * 0.60) + 4u;
    for (auto& line : delayLines_)
        line.assign(delaySize, 0.0f);
    delayTaps_[0] = std::max(1, static_cast<int>(0.360 * sampleRate_));
    delayTaps_[1] = std::max(1, static_cast<int>(0.378 * sampleRate_));

    // Coprime diffuser and comb lengths, offset between the channels, so the
    // ambience decorrelates without modulation, a Haas delay or randomised
    // phase - the same constraint the instrument's own stereo field obeys.
    static constexpr std::array<std::array<double, 3>, 2> diffuserMs {{
        { 4.71, 8.13, 12.79 },
        { 5.23, 9.07, 13.87 },
    }};
    static constexpr std::array<std::array<double, 2>, 2> combMs {{
        { 31.73, 43.91 },
        { 34.29, 47.11 },
    }};

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto index = static_cast<std::size_t>(channel);
        for (int stage = 0; stage < 3; ++stage)
        {
            auto& diffuser = diffusers_[index][static_cast<std::size_t>(stage)];
            diffuser.prepare(static_cast<int>(
                diffuserMs[index][static_cast<std::size_t>(stage)]
                * 0.001 * sampleRate_));
            diffuser.coefficient = 0.62f;
        }
        for (int stage = 0; stage < 2; ++stage)
        {
            auto& comb = combs_[index][static_cast<std::size_t>(stage)];
            comb.prepare(static_cast<int>(
                combMs[index][static_cast<std::size_t>(stage)]
                * 0.001 * sampleRate_));
            comb.feedback = stage == 0 ? 0.74f : 0.70f;
            comb.damping = std::exp(
                -twoPi * std::min(3200.0f, 0.40f * hostRate) / hostRate);
        }
    }

    prepared_ = true;
    reset();
}

float ElectryFx::gainStageLatencySamples() const noexcept
{
    if (! isGainStageEngaged())
        return 0.0f;

    // Every interpolating stage looks ahead by `oddTapCount` of its own input
    // samples, and every decimating stage is causal over `span` of its own
    // input samples; both are referred back to the host clock here. The
    // interpolator at index n reads at 2^n times the host rate and the
    // decimator at the same index reads at twice that.
    float latency = 0.0f;
    for (int stage = 0; stage < oversamplingStages_; ++stage)
    {
        const auto interpolatorRate = static_cast<float>(1 << stage);
        latency += static_cast<float>(HalfbandStage::oddTapCount)
                 / interpolatorRate;
        latency += static_cast<float>(HalfbandStage::span)
                 / (2.0f * interpolatorRate);
    }
    return latency;
}

void ElectryFx::designFilters() noexcept
{
    const float rate = oversampledRate_;
    for (auto& channel : gain_)
    {
        // Pedal: a tight input coupling network, a mid-focused voice and a soft
        // top. A distortion pedal that passes the whole low end of a Drop-E
        // eighth string into its clipper turns the fundamental into
        // intermodulation mud instead of a note - but the corner has to sit
        // below that fundamental, not on top of it. At 120 Hz the eighth
        // string's own 41 Hz was already 20 dB down before it reached the
        // clipper, so the stage had almost no low end to generate weight from.
        channel.pedalHighpass.setHighpass(88.0f, 0.78f, rate);
        channel.pedalVoice.setPeaking(800.0f, 0.85f, 5.5f, rate);
        channel.pedalTilt.setLowpass(6800.0f, 0.70f, rate);

        // Amp: the input stage passes the whole Drop-E fundamental, because
        // clipping it is what generates the second and third harmonics the
        // cabinet turns into a chug's weight. The mid emphasis sits above the
        // cabinet's scoop rather than inside it: pushing 560 Hz into the stage
        // and then cutting 470 Hz after it wasted gain on the one region a
        // metal rhythm tone wants out of the way.
        channel.ampHighpass.setHighpass(52.0f, 0.80f, rate);
        channel.ampVoice.setPeaking(850.0f, 0.75f, 4.0f, rate);

        // Cabinet. A single one-pole - the previous model - has neither the
        // thump, the mid character nor the steep top-end death of a real sealed
        // 4x12, and those three features are most of what makes a recorded
        // metal guitar recognisable as a guitar rather than as a waveshaper.
        // A 4x12's useful output starts just below the low strings' second
        // harmonic, and the thump that a palm-muted chug lives on sits in the
        // octave above that.
        channel.cabinet[0].setHighpass(74.0f, 0.80f, rate);   // no output below the box
        channel.cabinet[1].setPeaking(102.0f, 1.20f, 5.5f, rate);  // cabinet thump
        channel.cabinet[2].setPeaking(430.0f, 0.85f, -6.5f, rate); // boxy honk removed
        channel.cabinet[3].setPeaking(3100.0f, 1.10f, 4.5f, rate); // presence
        // Fourth-order Butterworth roll-off: a 12-inch speaker is essentially
        // gone an octave above five kilohertz. Running it here rather than
        // after decimation removes the alias-generating content first.
        channel.cabinet[4].setLowpass(5000.0f, 0.5412f, rate);
        channel.cabinet[5].setLowpass(5000.0f, 1.3066f, rate);
    }
}

void ElectryFx::reset() noexcept
{
    for (auto& channel : gain_)
        channel.reset();
    for (auto& line : delayLines_)
        std::fill(line.begin(), line.end(), 0.0f);
    for (auto& damping : delayDamping_)
        damping.reset();
    delayHighpassState_ = {};
    delayWriteIndex_ = 0;
    for (auto& channelDiffusers : diffusers_)
        for (auto& diffuser : channelDiffusers)
            diffuser.reset();
    for (auto& channelCombs : combs_)
        for (auto& comb : channelCombs)
            comb.reset();
    compressorEnvelope_ = 0.0f;
    gainEngagement_ = 0.0f;
    distortionMix_ = targetParameters_.distortion;
    ampMix_ = targetParameters_.amp;
    compressorMix_ = targetParameters_.compressor;
    delayMix_ = targetParameters_.delay;
    roomMix_ = targetParameters_.room;
    updateDriveConstants();
}

void ElectryFx::setParameters(const FxParameters& parameters) noexcept
{
    targetParameters_.distortion = sanitiseMix(parameters.distortion);
    targetParameters_.amp = sanitiseMix(parameters.amp);
    targetParameters_.compressor = sanitiseMix(parameters.compressor);
    targetParameters_.delay = sanitiseMix(parameters.delay);
    targetParameters_.room = sanitiseMix(parameters.room);
}

void ElectryFx::updateDriveConstants() noexcept
{
    // A pedal's gain range, and two amplifier stages whose drives rise
    // together. The trims below hold the chain's loudness within a few
    // decibels of the dry DI across the whole travel, so auditioning the amp is
    // a change of tone rather than a jump in level.
    pedalDrive_ = 1.0f + 24.0f * distortionMix_;
    ampDriveFirst_ = 1.0f + 7.0f * ampMix_;
    ampDriveSecond_ = 1.0f + 11.0f * ampMix_;
    // Each stage's trim divides its own small-signal gain back out, so the
    // control travels through tone rather than through level: a saturating
    // stage still ends up louder than the dry DI, because compressing a signal
    // raises its average, but not by the tens of decibels raw cascaded gain
    // would otherwise add.
    pedalMakeup_ = pedalTrim / pedalDrive_;
    ampMakeup_ = ampTrim / (ampDriveFirst_ * ampDriveSecond_);
}

// ---------------------------------------------------------------------------
// Gain stages
// ---------------------------------------------------------------------------

float ElectryFx::pedalStage(GainChannel& channel, float input) noexcept
{
    float sample = channel.pedalHighpass.process(input);
    sample = channel.pedalVoice.process(sample);
    sample = diodeClip(sample * pedalDrive_);
    sample = channel.pedalTilt.process(sample);
    return channel.pedalSmooth.process(sample, interstageCoefficient_)
         * pedalMakeup_;
}

float ElectryFx::ampStage(GainChannel& channel, float input) noexcept
{
    float sample = channel.ampHighpass.process(input);
    sample = channel.ampVoice.process(sample);

    // The operating point: a standing grid bias plus the drift that grid
    // current adds under sustained level. Driving a symmetric ceiling off
    // centre is what produces the even-order harmonics of a real stage, and
    // the drift is why a held chord thickens and thins again as it decays -
    // breathing a static waveshaper cannot reproduce. Subtracting the transfer
    // at the bias point keeps the stage centred instead of pumping DC into the
    // cabinet.
    channel.bias += biasCoefficient_ * (std::abs(sample) - channel.bias);
    const float bias = -0.22f - 1.10f * channel.bias;

    float stage = triode(sample * ampDriveFirst_ + bias) - triode(bias);
    // Miller capacitance between the stages: each one is progressively darker,
    // which is why a cascaded amplifier saturates smoothly instead of
    // accumulating fizz.
    stage = channel.interstage.process(stage, interstageCoefficient_);
    stage = triode(stage * ampDriveSecond_ + bias) - triode(bias);

    for (auto& section : channel.cabinet)
        stage = section.process(stage);
    return stage * ampMakeup_;
}

float ElectryFx::renderGainFrame(GainChannel& channel, float input) noexcept
{
    // Both stages always run while the block is engaged. A mix of exactly zero
    // already contributes nothing, and keeping the filters clocked means
    // neither stage can be re-engaged from stale state.
    const float pedal = pedalStage(channel, input);
    const float pedalled = lerp(input, pedal, distortionMix_);
    const float amp = ampStage(channel, pedalled);
    return lerp(pedalled, amp, ampMix_);
}

float ElectryFx::renderGainStage(GainChannel& channel, float input) noexcept
{
    if (oversamplingStages_ <= 0)
        return renderGainFrame(channel, input);

    // Every stage is stateful, so each one has to see its frames strictly in
    // time order; two four-float scratch buffers are ping-ponged rather than
    // rewriting a source frame that a later pair still needs.
    std::array<float, maximumOversampledFrames> frames {};
    std::array<float, maximumOversampledFrames> scratch {};
    frames[0] = input;
    int frameCount = 1;
    for (int stage = 0; stage < oversamplingStages_; ++stage)
    {
        auto& interpolator = channel.interpolators[static_cast<std::size_t>(stage)];
        for (int frame = 0; frame < frameCount; ++frame)
            interpolator.upsample(frames[static_cast<std::size_t>(frame)],
                                  scratch[static_cast<std::size_t>(2 * frame)],
                                  scratch[static_cast<std::size_t>(2 * frame + 1)]);
        frameCount *= 2;
        frames.swap(scratch);
    }

    for (int frame = 0; frame < frameCount; ++frame)
        frames[static_cast<std::size_t>(frame)] = renderGainFrame(
            channel, frames[static_cast<std::size_t>(frame)]);

    // The decimators are applied innermost first: the stage that halved the
    // rate last is the one that has to halve it back first.
    for (int stage = oversamplingStages_ - 1; stage >= 0; --stage)
    {
        auto& decimator = channel.decimators[static_cast<std::size_t>(stage)];
        frameCount /= 2;
        for (int frame = 0; frame < frameCount; ++frame)
            scratch[static_cast<std::size_t>(frame)] = decimator.decimate(
                frames[static_cast<std::size_t>(2 * frame)],
                frames[static_cast<std::size_t>(2 * frame + 1)]);
        frames.swap(scratch);
    }
    return frames[0];
}

// ---------------------------------------------------------------------------
// Chain
// ---------------------------------------------------------------------------

void ElectryFx::process(float* left, float* right, int numSamples) noexcept
{
    if (! prepared_ || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const int lineSize = static_cast<int>(delayLines_[0].size());
    if (lineSize <= 1)
        return;

    const auto& target = targetParameters_;
    const bool wantGain = target.distortion > 0.0f || target.amp > 0.0f;
    const float engagementTarget = wantGain ? 1.0f : 0.0f;

    // Smoothing with an exact snap at both ends: a control left at zero has to
    // reach zero rather than approach it, because that is what makes the dry
    // bypass bit-exact instead of merely quiet.
    const auto smooth = [this] (float& value, float targetValue)
    {
        value += parameterCoefficient_ * (targetValue - value);
        if (targetValue <= 0.0f && value < 1.0e-5f)
            value = 0.0f;
        else if (targetValue >= 1.0f && value > 1.0f - 1.0e-5f)
            value = 1.0f;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        smooth(distortionMix_, target.distortion);
        smooth(ampMix_, target.amp);
        smooth(compressorMix_, target.compressor);
        smooth(delayMix_, target.delay);
        smooth(roomMix_, target.room);
        updateDriveConstants();

        gainEngagement_ += engagementCoefficient_
                         * (engagementTarget - gainEngagement_);
        if (engagementTarget <= 0.0f && gainEngagement_ < 1.0e-4f)
        {
            if (gainEngagement_ != 0.0f)
            {
                // Nothing downstream can hear the block any more, so drop its
                // state: the next engagement starts from silence rather than
                // from a stale tail.
                for (auto& channel : gain_)
                    channel.reset();
            }
            gainEngagement_ = 0.0f;
        }
        else if (engagementTarget >= 1.0f && gainEngagement_ > 1.0f - 1.0e-4f)
        {
            gainEngagement_ = 1.0f;
        }

        // The delay and ambience networks are clocked whether or not their
        // controls are open, so one non-finite input sample would otherwise
        // recirculate and keep poisoning the output long after it arrived -
        // even at a mix of zero, where multiplying it by zero still yields a
        // NaN. Finite input passes through this untouched, so the dry bypass
        // stays bit-exact.
        std::array<float, 2> samples {
            std::isfinite(left[i]) ? left[i] : 0.0f,
            std::isfinite(right[i]) ? right[i] : 0.0f };

        // The oversampled gain block. With both controls at zero it is skipped
        // outright, so the chain costs nothing and adds no group delay; while
        // it engages and disengages it is crossfaded, so it cannot click.
        if (gainEngagement_ > 0.0f)
        {
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto index = static_cast<std::size_t>(channel);
                const float wet = renderGainStage(gain_[index], samples[index]);
                samples[index] = lerp(samples[index], wet, gainEngagement_);
            }
        }

        // Rhythm compressor: a soft knee easing into roughly 3.5:1 above
        // -20 dBFS. The knee is what lets a palm-muted part sit still instead
        // of the level grabbing at every pick attack.
        const float detector = std::max(std::abs(samples[0]),
                                        std::abs(samples[1]));
        const float ballistic = detector > compressorEnvelope_
            ? compressorAttack_ : compressorRelease_;
        compressorEnvelope_ = ballistic * compressorEnvelope_
                            + (1.0f - ballistic) * detector;
        constexpr float threshold = 0.1f; // -20 dBFS
        constexpr float kneeWidth = 0.6f;
        float compressedGain = 1.0f;
        const float over = compressorEnvelope_ / threshold;
        if (over > 1.0f)
        {
            const float knee = smoothStep((over - 1.0f) / kneeWidth);
            compressedGain = std::pow(over, -0.72f * knee);
        }
        // The makeup returns what the knee took away at a nominal rhythm
        // level, so reaching for the control levels the part instead of just
        // turning it down.
        const float compressorGain = lerp(
            1.0f, compressedGain * (1.0f + 0.75f * compressorMix_),
            compressorMix_);
        samples[0] *= compressorGain;
        samples[1] *= compressorGain;

        // Lead delay and room. Both networks are linear and always clocked, so
        // reaching for either control never starts from a cold, silent line,
        // and both contribute exactly zero at a mix of zero.
        std::array<float, 2> delayed {};
        std::array<float, 2> ambience {};
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto index = static_cast<std::size_t>(channel);
            auto& line = delayLines_[index];
            const int tap = std::min(delayTaps_[index], lineSize - 1);
            const int readIndex = (delayWriteIndex_ - tap + lineSize) % lineSize;
            delayed[index] = line[static_cast<std::size_t>(readIndex)];

            float feedback = delayDamping_[index].process(delayed[index],
                                                          delayFeedbackDamping_);
            delayHighpassState_[index] += delayFeedbackHighpass_
                * (feedback - delayHighpassState_[index]);
            feedback -= delayHighpassState_[index];
            line[static_cast<std::size_t>(delayWriteIndex_)] =
                samples[index] + feedback * (0.20f + 0.42f * delayMix_);

            float diffused = samples[index] + 0.30f * delayed[index] * delayMix_;
            for (auto& diffuser : diffusers_[index])
                diffused = diffuser.process(diffused);
            float tail = 0.0f;
            for (auto& comb : combs_[index])
                tail += comb.process(diffused);
            ambience[index] = 0.5f * tail;
        }
        delayWriteIndex_ = (delayWriteIndex_ + 1) % lineSize;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto index = static_cast<std::size_t>(channel);
            samples[index] += delayed[index] * 0.55f * delayMix_
                            + ambience[index] * 0.42f * roomMix_;
        }

        left[i] = clampf(samples[0], -2.0f, 2.0f);
        right[i] = clampf(samples[1], -2.0f, 2.0f);
    }

    // Backstop: the input is sanitised above and every recursion here is
    // bounded, so this should be unreachable, but a non-finite sample must
    // never be allowed to latch - the string engine recovers the same way.
    if (! std::isfinite(left[numSamples - 1])
        || ! std::isfinite(right[numSamples - 1]))
    {
        reset();
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
    }
}

} // namespace electry

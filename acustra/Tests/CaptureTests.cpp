// Capture is a read-only observation of the instrument. Verify the microphone
// channel identity, physical pickup observables, geometry, switching and MIDI
// silence; absolute pickup sensitivity requires recorded calibration pairs.
#include "DSP/AcustraEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace acustra
{
struct AcustraEngineTestAccess
{
    static std::array<float, 4> pickupFractions(const AcustraEngine& engine,
                                               int string)
    {
        const auto& voice = engine.voices_[static_cast<std::size_t>(string)];
        return { voice.loops[0].currentPickupFraction,
                 voice.loops[0].targetPickupFraction,
                 voice.tailLoop.currentPickupFraction,
                 voice.tailLoop.targetPickupFraction };
    }
    static double pointAmplitude(int harmonic, float fraction)
    {
        AcustraEngine::StringLoop loop;
        constexpr int period = 128;
        loop.currentDelay = period;
        for (int i = 0; i < AcustraEngine::maximumDelaySamples; ++i)
            loop.delay[static_cast<std::size_t>(i)] = static_cast<float>(
                std::sin(2.0 * 3.141592653589793 * harmonic * i / period));
        double peak = 0.0;
        for (int i = 0; i < period; ++i)
        {
            loop.writeIndex = i;
            peak = std::max(peak, std::abs(static_cast<double>(
                loop.displacementAt(fraction))));
        }
        return peak;
    }
};
} // namespace acustra

namespace
{
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Audio
{
    std::vector<float> left, right;
};

Audio render(acustra::EngineParameters parameters, int rate = 48000,
             int block = 64)
{
    auto engine = std::make_unique<acustra::AcustraEngine>();
    engine->setParameters(parameters);
    engine->prepare(rate, block);
    for (int note : { 40, 45, 50, 55, 59, 64 })
        engine->noteOn(note, 0.7f);
    Audio audio { std::vector<float>(rate), std::vector<float>(rate) };
    for (int i = 0; i < rate; i += block)
        engine->process(audio.left.data() + i, audio.right.data() + i,
                        std::min(block, rate - i));
    return audio;
}

double energy(const Audio& audio)
{
    double sum = 0.0;
    for (float x : audio.left)
        sum += x * x;
    return sum / audio.left.size();
}

bool sameSamples(const std::vector<float>& a, const std::vector<float>& b)
{
    return std::equal(a.begin(), a.end(), b.begin(),
        [] (float x, float y) { return std::abs(x - y) < 1.0e-6f; });
}

void testCaptureObservations()
{
    using acustra::CaptureType;
    acustra::EngineParameters parameters;
    parameters.stereoWidth = 1.0f;
    const auto stereo = render(parameters);
    parameters.capture = CaptureType::TrebleMic;
    const auto treble = render(parameters);
    parameters.capture = CaptureType::BassMic;
    const auto bass = render(parameters);
    // Width=1 still evaluates mono+(channel-mono) in the legacy path; permit
    // its rounding, but require the mono copies themselves to be identical.
    expect(sameSamples(stereo.left, treble.left) && treble.left == treble.right,
           "treble capture is not the measured left microphone in mono");
    expect(sameSamples(stereo.right, bass.left) && bass.left == bass.right,
           "bass capture is not the measured right microphone in mono");
    expect(treble.left != bass.left, "the two microphone paths are identical");

    for (auto capture : { CaptureType::SaddlePiezo, CaptureType::Magnetic })
    {
        parameters.capture = capture;
        const auto pickup = render(parameters);
        parameters.stereoWidth = 0.0f;
        parameters.bodyAmount = 0.0f;
        const auto dry = render(parameters);
        expect(pickup.left == pickup.right, "pickup output is not mono");
        expect(pickup.left == dry.left,
               "microphone mix controls altered the pickup observation");
        expect(energy(pickup) > 1.0e-10, "a steel pickup is silent");
        expect(pickup.left != treble.left && pickup.left != bass.left,
               "a pickup duplicates a microphone signal");
        parameters.stringMaterial = acustra::StringMaterial::Nylon;
        const auto nylon = render(parameters);
        expect(capture == CaptureType::Magnetic ? energy(nylon) == 0.0
                                               : energy(nylon) > 1.0e-10,
               "nylon must excite piezo force but not a magnetic pickup");
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        parameters.stereoWidth = 1.0f;
        parameters.bodyAmount = 0.82f;
    }
    parameters.capture = static_cast<CaptureType>(-123);
    expect(render(parameters).left == stereo.left,
           "invalid capture did not fall back to the default stereo microphone");

    // Each node is geometry, not an EQ curve or a guessed pickup frequency.
    using Access = acustra::AcustraEngineTestAccess;
    expect(Access::pointAmplitude(4, 0.25f) < 1.0e-5,
           "quarter-string observation missed the fourth-partial node");
    expect(std::abs(Access::pointAmplitude(2, 0.25f) - 2.0) < 1.0e-5,
           "quarter-string observation missed the second-partial antinode");
    expect(Access::pointAmplitude(2, 0.5f) < 1.0e-5,
           "midpoint observation missed the even-partial node");
}

void testCaptureLifecycle()
{
    for (int rate : { 44100, 48000, 96000 })
        for (int type = 0; type < 5; ++type)
        {
            acustra::EngineParameters parameters;
            parameters.capture = static_cast<acustra::CaptureType>(type);
            const auto audio = render(parameters, rate);
            expect(audio.left == render(parameters, rate, 127).left,
                   "capture changed with host block size");
            for (float sample : audio.left)
                expect(std::isfinite(sample) && std::abs(sample) <= 1.0f,
                       "capture is not finite and bounded");

            auto engine = std::make_unique<acustra::AcustraEngine>();
            engine->setParameters(parameters);
            engine->prepare(rate, 64);
            std::array<float, 64> left {}, right {};
            engine->noteOn(40, 0.8f);
            engine->process(left.data(), right.data(), 64);
            engine->allSoundOff();
            engine->process(left.data(), right.data(), 64);
            expect(std::all_of(left.begin(), left.end(),
                              [] (float x) { return x == 0.0f; }),
                   "all sound off left a pickup derivative tail");
        }

    auto engine = std::make_unique<acustra::AcustraEngine>();
    engine->prepare(48000, 64);
    acustra::EngineParameters parameters;
    std::array<float, 64> left {}, right {};
    engine->noteOn(40, 0.8f);
    for (int i = 0; i < 200; ++i)
    {
        parameters.capture = static_cast<acustra::CaptureType>(i % 5);
        engine->setParameters(parameters);
        engine->process(left.data(), right.data(), 64);
        expect(engine->getActiveVoiceCount() == 1,
               "switching capture reset a ringing note");
        for (float sample : left)
            expect(std::isfinite(sample) && std::abs(sample) <= 1.0f,
                   "rapid capture switching is not finite and bounded");
    }
    parameters.capture = acustra::CaptureType::StereoMic;
    engine->setParameters(parameters);
    engine->reset();
    engine->process(left.data(), right.data(), 64);
    expect(std::all_of(left.begin(), left.end(),
                      [] (float x) { return x == 0.0f; }),
           "reset left a capture tail");

    // Observe through a pickup and return to the mic: the physical state must
    // be identical to an engine that never changed capture, sample for sample.
    engine = std::make_unique<acustra::AcustraEngine>();
    engine->prepare(48000, 64);
    auto reference = std::make_unique<acustra::AcustraEngine>();
    reference->prepare(48000, 64);
    engine->noteOn(45, 0.7f);
    reference->noteOn(45, 0.7f);
    std::array<float, 64> referenceLeft {}, referenceRight {};
    for (int block = 0; block < 600; ++block)
    {
        if (block < 100)
            parameters.capture = static_cast<acustra::CaptureType>(block % 5);
        else
            parameters.capture = acustra::CaptureType::StereoMic;
        engine->setParameters(parameters);
        engine->process(left.data(), right.data(), 64);
        reference->process(referenceLeft.data(), referenceRight.data(), 64);
    }
    expect(left == referenceLeft && right == referenceRight,
           "pickup observation changed the state of the vibrating instrument");
}

void testScopedRemovalDoesNotStrikeTheMagneticPickup()
{
    for (int rate : { 44100, 48000, 96000 })
        for (bool removeZone : { false, true })
        {
            auto engine = std::make_unique<acustra::AcustraEngine>();
            acustra::EngineParameters parameters;
            parameters.capture = acustra::CaptureType::Magnetic;
            engine->setParameters(parameters);
            engine->prepare(rate, 64);
            engine->setLowerZoneMemberCount(1);
            engine->noteOn(40, 0.8f, 2);
            engine->noteOn(64, 0.1f, 8);
            std::array<float, 64> left {}, right {};
            for (int block = 0; block < 150; ++block)
                engine->process(left.data(), right.data(), 64);
            float before = 0.0f;
            for (float sample : left)
                before = std::max(before, std::abs(sample));

            if (removeZone)
                engine->setLowerZoneMemberCount(0);
            else
                engine->allSoundOff(2);
            expect(engine->getActiveVoiceCount() == 1,
                   "scoped removal stopped an unrelated channel");
            engine->process(left.data(), right.data(), 64);
            float onset = 0.0f;
            for (int sample = 0; sample < 8; ++sample)
                onset = std::max(onset, std::abs(left[sample]));
            // Removing the loud note cannot strike the remaining quiet one.
            // Differencing its deleted displacement used to spike 30-73x
            // above the preceding block, including into the output limiter.
            expect(before > 1.0e-5f && onset < 2.0f * before,
                   "scoped removal turned deleted displacement into a magnetic impulse");
            engine->allSoundOff(8);
            expect(engine->getActiveVoiceCount() == 0,
                   "the surviving voice did not belong to the unrelated channel");
        }
}

void testPickupPositionFollowsLengthAndNotTension()
{
    using Access = acustra::AcustraEngineTestAccess;
    const float fretThree = 0.25f * std::exp2(3.0f / 12.0f);
    const float fretFive = 0.25f * std::exp2(5.0f / 12.0f);
    for (bool mpe : { false, true })
    {
        auto engine = std::make_unique<acustra::AcustraEngine>();
        acustra::EngineParameters parameters;
        parameters.capture = acustra::CaptureType::Magnetic;
        engine->setParameters(parameters);
        engine->prepare(48000, 64);
        engine->setStringPerChannelMode(true);
        if (mpe)
            engine->setLowerZoneMemberCount(1);
        const int channel = mpe ? 2 : 1;
        const int string = channel - 1;
        const int note = mpe ? 48 : 43;
        engine->noteOn(note, 0.8f, channel);
        std::array<float, 64> left {}, right {};
        for (int block = 0; block < 100; ++block)
            engine->process(left.data(), right.data(), 64);
        expect(std::abs(Access::pickupFractions(*engine, string)[0]
                        - fretThree) < 1.0e-5f,
               "the fretted note has the wrong pickup geometry");
        engine->setPitchBend(2.0f, channel);
        for (int block = 0; block < 100; ++block)
            engine->process(left.data(), right.data(), 64);
        auto fraction = Access::pickupFractions(*engine, string);
        expect(std::abs(fraction[0] - (mpe ? fretThree : fretFive)) < 1.0e-5f,
               "a slide failed to move geometry or a tension bend moved it");
        engine->noteOn(note + 4, 0.8f, channel);
        const auto replucked = Access::pickupFractions(*engine, string);
        expect(replucked[2] == fraction[0] && replucked[3] == fraction[1],
               "a repluck discarded the previous note's pickup geometry");
        if (mpe)
        {
            engine->setPitchBend(2.0f, 1);
            for (int block = 0; block < 100; ++block)
                engine->process(left.data(), right.data(), 64);
            expect(std::abs(Access::pickupFractions(*engine, string)[0]
                            - 0.25f * std::exp2(9.0f / 12.0f)) < 1.0e-5f,
                   "an MPE manager slide did not move pickup geometry");
        }
        // A pickup past the new stopping point is outside the vibrating
        // segment. Extreme host bends saturate at the endpoint, never wrap.
        engine->setPitchBend(192.0f, 1);
        for (int block = 0; block < 100; ++block)
            engine->process(left.data(), right.data(), 64);
        fraction = Access::pickupFractions(*engine, string);
        expect(fraction[0] > 0.9999f && fraction[1] == 1.0f,
               "an extreme slide let pickup geometry escape the string");
        for (float sample : left)
            expect(std::isfinite(sample), "sliding a magnetic note is nonfinite");
    }
}
} // namespace

int main()
{
    testCaptureObservations();
    testCaptureLifecycle();
    testScopedRemovalDoesNotStrikeTheMagneticPickup();
    testPickupPositionFollowsLengthAndNotTension();
    if (failures == 0)
        std::cout << "All Acustra capture tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

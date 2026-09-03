#include "DSP/AcustraEngine.h"
#include "DSP/MeasuredBodyData.h"
#include "DSP/MeasuredBridgeData.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace acustra
{
struct AcustraEngineTestAccess
{
    struct StringLoopSnapshot
    {
        double delay;
        double loopGain;
        double broadCoefficient;
        double broadMix;
        double highCoefficient;
        double highMix;
        double dispersionA1;
        double dispersionA2;
        double inharmonicity;
    };

    struct BodyModeSnapshot
    {
        double frequency;
        double q;
        double residue;
    };

    struct PluckSnapshot
    {
        double touch;
        double peakPosition;
        double peakDisplacement;
        double noiseEnvelope;
        double pluckPoint;
    };

    struct PreparedLossSnapshot
    {
        double beforeScale;
        double afterScale;
        double beforeA1;
        double beforeA2;
        double afterA1;
        double afterA2;
    };

    struct RetunedStringSnapshot
    {
        double impedance;
        double tailStiffness;
        double inharmonicity;
    };

    struct AttackPitchSnapshot
    {
        double energy;
        double cents;
        double decay;
    };

    static std::array<double, 2> longitudinalFrequencies(int midiNote)
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        const int stringIndex = engine.chooseString(midiNote);
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        engine.configureVoice(voice, stringIndex, midiNote, true);
        std::array<double, 2> result {};
        for (std::size_t mode = 0; mode < result.size(); ++mode)
        {
            const double radius = std::sqrt(-voice.longitudinalA2[mode]);
            result[mode] = std::acos(std::clamp(static_cast<double>(
                voice.longitudinalA1[mode]) / (2.0 * radius), -1.0, 1.0))
                * 48000.0 / (2.0 * std::numbers::pi);
        }
        return result;
    }

    struct BendLifecycleSnapshot
    {
        double heldDelay;
        double afterMemberTrafficDelay;
        double afterMasterTrafficDelay;
        double reusedFingerDelay;
        float frozenMemberBend;
        bool frozen;
        bool pedalHeld;
        bool reusedFingerFrozen;
    };

    static StringLoopSnapshot configuredLoop(StringMaterial material,
                                             int midiNote, double rate,
                                             PhysicalCalibration calibration
                                                 = fittedPhysicalCalibration)
    {
        AcustraEngine engine;
        engine.setPhysicalCalibration(calibration);
        engine.prepare(rate, 64);
        engine.setBridgeCouplingEnabled(false);
        EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        const int stringIndex = engine.chooseString(midiNote);
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        voice.attackPitchCents = 0.0f;
        engine.configureVoice(voice, stringIndex, midiNote, true);
        const auto& loop = voice.loops[0];
        return { loop.targetDelay, loop.loopGain, loop.broadLossCoefficient,
                 loop.broadLossMix, loop.lowpassCoefficient,
                 loop.highLossMix, loop.dispersionA1, loop.dispersionA2,
                 voice.dispersionDesignInharmonicity };
    }

    static BodyModeSnapshot configuredBody(
        PhysicalCalibration calibration, int index,
        StringMaterial material = StringMaterial::Steel)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.setPhysicalCalibration(calibration);
        engine.prepare(48000.0, 64);
        const auto& mode = engine.bodyModes_[static_cast<std::size_t>(index)];
        const double radius = std::hypot(mode.poleReal, mode.poleImaginary);
        const double frequency = std::atan2(mode.poleImaginary, mode.poleReal)
                               * 48000.0 / (2.0 * std::numbers::pi);
        const double q = -std::numbers::pi * frequency
                       / (48000.0 * std::log(radius));
        return { frequency, q, std::hypot(mode.leftReal, mode.leftImaginary) };
    }

    static double bridgeAdmittance(
        PhysicalCalibration calibration,
        StringMaterial material = StringMaterial::Steel)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.setPhysicalCalibration(calibration);
        engine.prepare(48000.0, 64);
        return engine.bridgeLoad_.immediateAdmittance;
    }

    static double bridgeAdmittanceOf(const AcustraEngine& engine)
    {
        return engine.bridgeLoad_.immediateAdmittance;
    }

    static double bodyResidueOf(const AcustraEngine& engine, int index)
    {
        const auto& mode = engine.bodyModes_[static_cast<std::size_t>(index)];
        return std::hypot(mode.leftReal, mode.leftImaginary)
             + std::hypot(mode.rightReal, mode.rightImaginary)
             + std::hypot(mode.poleReal, mode.poleImaginary);
    }

    static constexpr int bodyModeCapacity = AcustraEngine::bodyModeCount;

    static double playedDelay(PhysicalCalibration calibration)
    {
        AcustraEngine engine;
        engine.setPhysicalCalibration(calibration);
        engine.prepare(48000.0, 64);
        engine.noteOn(52, 0.8f);
        const auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.midiNote == 52;
            });
        return selected == engine.voices_.end()
            ? 0.0 : selected->loops[0].targetDelay;
    }

    static double channelBentDelay(float masterBend, float memberBend)
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        engine.setLowerZoneMemberCount(2);
        engine.noteOn(52, 0.8f, 2);
        const auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.midiNote == 52
                    && voice.midiChannel == 2;
            });
        if (selected == engine.voices_.end())
            return 0.0;
        const int stringIndex = static_cast<int>(
            std::distance(engine.voices_.begin(), selected));
        selected->attackPitchCents = 0.0f;
        engine.setPitchBend(masterBend, 1);
        engine.setPitchBend(memberBend, 2);
        engine.configureVoice(*selected, stringIndex, 52, false);
        return selected->loops[0].targetDelay;
    }

    static double conventionalChannelDelay(float channelOneBend,
                                           float channelTwoBend)
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        engine.noteOn(52, 0.8f, 2);
        auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.midiChannel == 2;
            });
        if (selected == engine.voices_.end())
            return 0.0;
        const int stringIndex = static_cast<int>(
            std::distance(engine.voices_.begin(), selected));
        selected->attackPitchCents = 0.0f;
        engine.setPitchBend(channelOneBend, 1);
        engine.setPitchBend(channelTwoBend, 2);
        engine.configureVoice(*selected, stringIndex, 52, false);
        return selected->loops[0].targetDelay;
    }

    static BendLifecycleSnapshot bendLifecycle(bool mpe)
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        if (mpe)
            engine.setLowerZoneMemberCount(2);
        engine.noteOn(52, 0.8f, 2);
        auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.keyDown
                    && voice.midiChannel == 2;
            });
        if (selected == engine.voices_.end())
            return {};
        const int stringIndex = static_cast<int>(
            std::distance(engine.voices_.begin(), selected));
        selected->attackPitchCents = 0.0f;
        engine.setPitchBend(5.0f, 2);
        engine.configureVoice(*selected, stringIndex, 52, false);
        const double heldDelay = selected->loops[0].targetDelay;
        engine.setSustainPedal(true, mpe ? 1 : 2);
        engine.noteOff(52, 2);
        const bool frozen = selected->memberPitchBendFrozen;
        const bool pedalHeld = selected->pedalHeld;
        const float frozenBend = selected->frozenMemberPitchBendSemitones;

        engine.setPitchBend(-5.0f, 2);
        engine.configureVoice(*selected, stringIndex, 52, false);
        const double afterMember = selected->loops[0].targetDelay;
        engine.setPitchBend(1.0f, 1);
        engine.configureVoice(*selected, stringIndex, 52, false);
        const double afterMaster = selected->loops[0].targetDelay;

        engine.noteOn(52, 0.7f, 2);
        // The same note is replucked on the string still sounding it, so the
        // reused finger may well be the same voice.
        auto reused = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.keyDown
                    && voice.midiChannel == 2;
            });
        double reusedDelay = 0.0;
        bool reusedFrozen = true;
        if (reused != engine.voices_.end())
        {
            const int reusedIndex = static_cast<int>(
                std::distance(engine.voices_.begin(), reused));
            reused->attackPitchCents = 0.0f;
            engine.configureVoice(*reused, reusedIndex, 52, false);
            reusedDelay = reused->loops[0].targetDelay;
            reusedFrozen = reused->memberPitchBendFrozen;
        }
        return { heldDelay, afterMember, afterMaster, reusedDelay,
                 frozenBend, frozen, pedalHeld, reusedFrozen };
    }

    static std::array<int, 3> lowerZoneTransitionCounts()
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        engine.noteOn(52, 0.8f, 2);
        engine.noteOn(55, 0.8f, 8);
        engine.setLowerZoneMemberCount(2);
        const int enabled = engine.getActiveVoiceCount();
        engine.setLowerZoneMemberCount(7);
        const int resized = engine.getActiveVoiceCount();
        engine.noteOn(52, 0.8f, 2);
        engine.setLowerZoneMemberCount(0);
        return { enabled, resized, engine.getActiveVoiceCount() };
    }

    static bool allSoundOffPreservesControllers()
    {
        AcustraEngine engine;
        engine.prepare(48000.0, 64);
        engine.setPitchBend(7.0f, 2);
        engine.setSustainPedal(true, 2);
        engine.noteOn(52, 0.8f, 2);
        engine.allSoundOff(2);
        return engine.pitchBendSemitones_[1] == 7.0f
            && engine.sustainPedals_[1];
    }

    static float apertureReferenceDelay()
    {
        return 48000.0f / AcustraEngine::midiFrequency(61);
    }

    static float registeredAperture(float apertureSamples, float apertureScale,
                                    float currentReferenceLength,
                                    float exponent)
    {
        return AcustraEngine::registeredPluckAperture(
            apertureSamples, apertureScale, apertureReferenceDelay(),
            currentReferenceLength, exponent);
    }

    static PluckSnapshot pluck(PhysicalCalibration calibration,
                               StringMaterial material, float velocity,
                               int midiNote = 52)
    {
        AcustraEngine engine;
        engine.setPhysicalCalibration(calibration);
        EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.prepare(48000.0, 64);
        engine.setBridgeCouplingEnabled(false);
        engine.noteOn(midiNote, velocity);

        const auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [midiNote] (const auto& voice)
            {
                return voice.played && voice.midiNote == midiNote;
            });
        if (selected == engine.voices_.end())
            return {};
        const auto& voice = *selected;
        const auto& loop = voice.loops[0];
        const int length = std::clamp(
            static_cast<int>(std::round(loop.targetDelay)), 8,
            AcustraEngine::maximumDelaySamples - 3);
        const auto at = [&] (int sample)
        {
            int index = loop.writeIndex - sample;
            while (index < 0)
                index += AcustraEngine::maximumDelaySamples;
            while (index >= AcustraEngine::maximumDelaySamples)
                index -= AcustraEngine::maximumDelaySamples;
            return static_cast<double>(
                loop.delay[static_cast<std::size_t>(index)]);
        };
        double peakValue = 0.0;
        int peakSample = 0;
        for (int sample = 1; sample <= length; ++sample)
        {
            const double value = at(sample);
            if (value > peakValue)
            {
                peakValue = value;
                peakSample = sample;
            }
        }
        return {
            engine.effectiveTouch(voice),
            static_cast<double>(peakSample) / length,
            peakValue,
            voice.excitationEnvelope,
            voice.pluckPoint
        };
    }

    static double lastPluckPoint(const AcustraEngine& engine)
    {
        const AcustraEngine::Voice* latest = nullptr;
        for (const auto& voice : engine.voices_)
            if (voice.played && (latest == nullptr
                                 || voice.startOrder > latest->startOrder))
                latest = &voice;
        return latest == nullptr ? -1.0 : latest->pluckPoint;
    }

    static PreparedLossSnapshot changePreparedLoss(
        PhysicalCalibration before, PhysicalCalibration after)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.setPhysicalCalibration(before);
        engine.prepare(48000.0, 64);
        engine.setBridgeCouplingEnabled(false);
        const auto& originalVoice = engine.voices_[0];
        const PreparedLossSnapshot original {
            originalVoice.dispersionDesignFrequencyLossScale, 0.0,
            originalVoice.loops[0].dispersionA1,
            originalVoice.loops[0].dispersionA2, 0.0, 0.0
        };

        engine.setPhysicalCalibration(after);
        const auto& updatedVoice = engine.voices_[0];
        return {
            original.beforeScale,
            updatedVoice.dispersionDesignFrequencyLossScale,
            original.beforeA1,
            original.beforeA2,
            updatedVoice.loops[0].dispersionA1,
            updatedVoice.loops[0].dispersionA2
        };
    }

    static PhysicalCalibration calibration(const AcustraEngine& engine)
    {
        return engine.physicalCalibration_;
    }

    static RetunedStringSnapshot retunedLowSteel(Tuning tuning)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = StringMaterial::Steel;
        parameters.tuning = tuning;
        engine.setParameters(parameters);
        engine.prepare(48000.0, 64);
        const auto& voice = engine.voices_[0];
        return { voice.characteristicImpedance, voice.bridgeTailStiffness,
                 voice.dispersionDesignInharmonicity };
    }

    static std::vector<AttackPitchSnapshot> attackPitchTrace(
        StringMaterial material, float velocity, int samples)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.prepare(48000.0, 1);
        engine.noteOn(52, velocity);
        const auto selected = std::find_if(engine.voices_.begin(),
            engine.voices_.end(), [] (const auto& voice)
            {
                return voice.played && voice.midiNote == 52;
            });
        if (selected == engine.voices_.end())
            return {};

        std::vector<AttackPitchSnapshot> trace {{
            selected->attackSlopeEnergy, selected->attackPitchCents,
            selected->attackPitchDecay
        }};
        for (int sample = 0; sample < samples; ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process(&left, &right, 1);
            if ((sample + 1) % AcustraEngine::controlPeriod == 0)
                trace.push_back({ selected->attackSlopeEnergy,
                                  selected->attackPitchCents,
                                  selected->attackPitchDecay });
        }
        return trace;
    }

    struct StolenStringSnapshot
    {
        double heldBeforeSteal;      // stored energy in the string's own loops
        double keptInTail;           // stored energy carried into the tail
        double tailEnergyAfterDecay; // the same tail 0.5 s later
        bool tailActive;
        bool tailActiveAfterRepluck; // a repluck of the same note lands the hand
    };

    static double loopEnergy(const AcustraEngine::StringLoop& loop)
    {
        const int length = std::clamp(
            static_cast<int>(std::round(loop.targetDelay)), 8,
            AcustraEngine::maximumDelaySamples - 3);
        double total = 0.0;
        for (int sample = 1; sample <= length; ++sample)
        {
            int index = loop.writeIndex - sample;
            while (index < 0) index += AcustraEngine::maximumDelaySamples;
            const double value = loop.delay[static_cast<std::size_t>(index)];
            total += value * value;
        }
        return total;
    }

    static StolenStringSnapshot stealStringTail(double rate)
    {
        AcustraEngine engine;
        EngineParameters parameters;
        parameters.stringMaterial = StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(rate, 128);
        const std::array<int, 6> first { 40, 47, 52, 56, 59, 64 };
        const std::array<int, 6> second { 43, 50, 55, 58, 62, 67 };
        std::vector<float> l(128), r(128);
        const auto run = [&] (double seconds)
        {
            for (int i = 0; i < static_cast<int>(seconds * rate); i += 128)
                engine.process(l.data(), r.data(), 128);
        };
        for (const int note : first) engine.noteOn(note, 0.8f);
        run(0.5);
        StolenStringSnapshot out {};
        out.heldBeforeSteal = loopEnergy(engine.voices_[0].loops[0]);
        for (const int note : second) engine.noteOn(note, 0.8f);
        out.tailActive = engine.voices_[0].tailActive;
        out.keptInTail = loopEnergy(engine.voices_[0].tailLoop);
        run(0.5);
        out.tailEnergyAfterDecay = engine.voices_[0].tailActive
            ? loopEnergy(engine.voices_[0].tailLoop) : 0.0;

        AcustraEngine repluck;
        repluck.setParameters(parameters);
        repluck.prepare(rate, 128);
        repluck.noteOn(52, 0.8f);
        for (int i = 0; i < static_cast<int>(0.5 * rate); i += 128)
            repluck.process(l.data(), r.data(), 128);
        repluck.noteOff(52);
        repluck.noteOn(52, 0.8f);
        out.tailActiveAfterRepluck = false;
        for (const auto& voice : repluck.voices_)
            out.tailActiveAfterRepluck |= voice.tailActive;
        return out;
    }

    static constexpr int controlPeriodSamples() noexcept
    {
        return AcustraEngine::controlPeriod;
    }
};
} // namespace acustra

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 127;
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Audio
{
    std::vector<float> left;
    std::vector<float> right;
};

Audio renderAtRate(acustra::EngineParameters parameters, int midiNote,
                   float velocity, double seconds, double rate,
                   int renderBlock = blockSize,
                   bool bridgeCoupling = true,
                   acustra::PhysicalCalibration calibration
                       = acustra::fittedPhysicalCalibration)
{
    acustra::AcustraEngine engine;
    engine.setPhysicalCalibration(calibration);
    engine.prepare(rate, renderBlock);
    engine.setParameters(parameters);
    engine.setBridgeCouplingEnabled(bridgeCoupling);
    engine.noteOn(midiNote, velocity);
    const int samples = static_cast<int>(seconds * rate);
    Audio result { std::vector<float>(static_cast<std::size_t>(samples)),
                   std::vector<float>(static_cast<std::size_t>(samples)) };
    for (int offset = 0; offset < samples; offset += renderBlock)
    {
        const int count = std::min(renderBlock, samples - offset);
        engine.process(result.left.data() + offset,
                       result.right.data() + offset, count);
    }
    return result;
}

Audio render(acustra::EngineParameters parameters, int midiNote,
             float velocity, double seconds, int renderBlock = blockSize,
             bool bridgeCoupling = true)
{
    return renderAtRate(parameters, midiNote, velocity, seconds,
                        sampleRate, renderBlock, bridgeCoupling);
}

Audio renderCalibrated(acustra::EngineParameters parameters,
                       acustra::PhysicalCalibration calibration,
                       int midiNote, float velocity, double seconds)
{
    acustra::AcustraEngine engine;
    engine.setPhysicalCalibration(calibration);
    engine.setParameters(parameters);
    engine.prepare(sampleRate, blockSize);
    engine.noteOn(midiNote, velocity);
    const int samples = static_cast<int>(seconds * sampleRate);
    Audio result { std::vector<float>(static_cast<std::size_t>(samples)),
                   std::vector<float>(static_cast<std::size_t>(samples)) };
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        engine.process(result.left.data() + offset,
                       result.right.data() + offset, count);
    }
    return result;
}

Audio renderWithInitialParameters(acustra::EngineParameters parameters,
                                  int midiNote, float velocity,
                                  double seconds)
{
    acustra::AcustraEngine engine;
    engine.setParameters(parameters);
    engine.prepare(sampleRate, blockSize);
    engine.noteOn(midiNote, velocity);
    const int samples = static_cast<int>(seconds * sampleRate);
    Audio result { std::vector<float>(static_cast<std::size_t>(samples)),
                   std::vector<float>(static_cast<std::size_t>(samples)) };
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        engine.process(result.left.data() + offset,
                       result.right.data() + offset, count);
    }
    return result;
}

Audio renderWithSympatheticStrings(int midiNote, bool enabled,
                                   double seconds)
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setSympatheticStringsEnabled(enabled);
    engine.noteOn(midiNote, 1.0f);
    const int samples = static_cast<int>(seconds * sampleRate);
    Audio result { std::vector<float>(static_cast<std::size_t>(samples)),
                   std::vector<float>(static_cast<std::size_t>(samples)) };
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        engine.process(result.left.data() + offset,
                       result.right.data() + offset, count);
    }
    return result;
}

double peak(const Audio& audio)
{
    double result = 0.0;
    for (std::size_t i = 0; i < audio.left.size(); ++i)
        result = std::max(result, static_cast<double>(std::max(
            std::abs(audio.left[i]), std::abs(audio.right[i]))));
    return result;
}

double peak(const Audio& audio, int begin, int end)
{
    begin = std::clamp(begin, 0, static_cast<int>(audio.left.size()));
    end = std::clamp(end, begin, static_cast<int>(audio.left.size()));
    double result = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        result = std::max(result, static_cast<double>(std::max(
            std::abs(audio.left[index]), std::abs(audio.right[index]))));
    }
    return result;
}

double rms(const Audio& audio, int begin, int end)
{
    begin = std::clamp(begin, 0, static_cast<int>(audio.left.size()));
    end = std::clamp(end, begin, static_cast<int>(audio.left.size()));
    double energy = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const double value = 0.5 * (audio.left[static_cast<std::size_t>(sample)]
                                  + audio.right[static_cast<std::size_t>(sample)]);
        energy += value * value;
    }
    return std::sqrt(energy / std::max(1, end - begin));
}

double differenceRms(const Audio& a, const Audio& b, int begin, int end)
{
    const int size = static_cast<int>(
        std::min(a.left.size(), b.left.size()));
    begin = std::clamp(begin, 0, size);
    end = std::clamp(end, begin, size);
    double energy = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const double delta = 0.5
            * ((a.left[index] - b.left[index])
             + (a.right[index] - b.right[index]));
        energy += delta * delta;
    }
    return std::sqrt(energy / std::max(1, end - begin));
}

double differenceRms(const Audio& audio, int begin, int end)
{
    begin = std::clamp(begin, 1, static_cast<int>(audio.left.size()));
    end = std::clamp(end, begin, static_cast<int>(audio.left.size()));
    double energy = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const auto previous = static_cast<std::size_t>(sample - 1);
        const double value = 0.5
            * ((audio.left[index] - audio.left[previous])
             + (audio.right[index] - audio.right[previous]));
        energy += value * value;
    }
    return std::sqrt(energy / std::max(1, end - begin));
}

double onsetBandRms(const Audio& audio, double rate)
{
    constexpr double duration = 0.020;
    constexpr double highestFrequency = 12000.0;
    const int count = std::min(
        static_cast<int>(std::round(duration * rate)),
        static_cast<int>(audio.left.size()));
    if (count < 2)
        return 0.0;

    std::vector<double> windowed(static_cast<std::size_t>(count));
    double windowEnergy = 0.0;
    for (int sample = 0; sample < count; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const double mono = 0.5 * (audio.left[index] + audio.right[index]);
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * sample / (count - 1));
        windowed[index] = mono * window;
        windowEnergy += window * window;
    }

    double bandEnergy = 0.0;
    const int highestBin = static_cast<int>(
        std::floor(highestFrequency * duration));
    for (int bin = 1; bin <= highestBin; ++bin)
    {
        const double angle = -2.0 * std::numbers::pi * bin / count;
        const double stepReal = std::cos(angle);
        const double stepImaginary = std::sin(angle);
        double oscillatorReal = 1.0;
        double oscillatorImaginary = 0.0;
        double real = 0.0;
        double imaginary = 0.0;
        for (const double value : windowed)
        {
            real += value * oscillatorReal;
            imaginary += value * oscillatorImaginary;
            const double nextReal = oscillatorReal * stepReal
                                  - oscillatorImaginary * stepImaginary;
            oscillatorImaginary = oscillatorReal * stepImaginary
                                 + oscillatorImaginary * stepReal;
            oscillatorReal = nextReal;
        }
        bandEnergy += 2.0 * (real * real + imaginary * imaginary);
    }
    return std::sqrt(bandEnergy / (count * windowEnergy));
}

double tailBandRms(const Audio& audio, double rate, double beginSeconds,
                   double endSeconds, double lowFrequency,
                   double highFrequency)
{
    const int begin = std::clamp(static_cast<int>(beginSeconds * rate), 0,
                                 static_cast<int>(audio.left.size()));
    const int end = std::clamp(static_cast<int>(endSeconds * rate), begin,
                               static_cast<int>(audio.left.size()));
    const int count = end - begin;
    if (count < 64)
        return 0.0;
    const double duration = count / rate;
    std::vector<double> windowed(static_cast<std::size_t>(count));
    double windowEnergy = 0.0;
    for (int sample = 0; sample < count; ++sample)
    {
        const auto index = static_cast<std::size_t>(begin + sample);
        const double mono = 0.5 * (audio.left[index] + audio.right[index]);
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * sample / (count - 1));
        windowed[static_cast<std::size_t>(sample)] = mono * window;
        windowEnergy += window * window;
    }
    const int lowestBin = std::max(1,
        static_cast<int>(std::ceil(lowFrequency * duration)));
    const int highestBin = std::min(count / 2 - 1,
        static_cast<int>(std::floor(highFrequency * duration)));
    double bandEnergy = 0.0;
    for (int bin = lowestBin; bin <= highestBin; ++bin)
    {
        const double angle = -2.0 * std::numbers::pi * bin / count;
        const double stepReal = std::cos(angle);
        const double stepImaginary = std::sin(angle);
        double oscillatorReal = 1.0;
        double oscillatorImaginary = 0.0;
        double real = 0.0;
        double imaginary = 0.0;
        for (const double value : windowed)
        {
            real += value * oscillatorReal;
            imaginary += value * oscillatorImaginary;
            const double nextReal = oscillatorReal * stepReal
                                  - oscillatorImaginary * stepImaginary;
            oscillatorImaginary = oscillatorReal * stepImaginary
                                 + oscillatorImaginary * stepReal;
            oscillatorReal = nextReal;
        }
        bandEnergy += 2.0 * (real * real + imaginary * imaginary);
    }
    return std::sqrt(bandEnergy / (count * windowEnergy));
}

double stereoDifferenceRms(const Audio& audio, int begin, int end)
{
    begin = std::clamp(begin, 0, static_cast<int>(audio.left.size()));
    end = std::clamp(end, begin, static_cast<int>(audio.left.size()));
    double energy = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const double difference = audio.left[index] - audio.right[index];
        energy += difference * difference;
    }
    return std::sqrt(energy / std::max(1, end - begin));
}

double normalisedDifference(const Audio& a, const Audio& b)
{
    const auto count = std::min(a.left.size(), b.left.size());
    double difference = 0.0;
    double reference = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double av = 0.5 * (a.left[i] + a.right[i]);
        const double bv = 0.5 * (b.left[i] + b.right[i]);
        const double delta = av - bv;
        difference += delta * delta;
        reference += av * av + bv * bv;
    }
    return std::sqrt(difference / std::max(reference, 1.0e-30));
}

double spectralPeakFrequency(const Audio& audio, double expectedHz,
                             double searchCents, double beginSeconds,
                             double endSeconds, double rate = sampleRate)
{
    const int begin = std::clamp(
        static_cast<int>(beginSeconds * rate), 0,
        static_cast<int>(audio.left.size()));
    const int end = std::clamp(
        static_cast<int>(endSeconds * rate), begin,
        static_cast<int>(audio.left.size()));
    std::vector<double> windowed(static_cast<std::size_t>(end - begin));
    for (int sample = begin; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample);
        const int local = sample - begin;
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * local
            / std::max(1, end - begin - 1));
        windowed[static_cast<std::size_t>(local)] = 0.5 * window
            * (audio.left[index] + audio.right[index]);
    }

    const auto powerAt = [&] (double frequency)
    {
        const double angle = -2.0 * std::numbers::pi
                           * frequency / rate;
        const double stepReal = std::cos(angle);
        const double stepImaginary = std::sin(angle);
        double oscillatorReal = 1.0;
        double oscillatorImaginary = 0.0;
        double real = 0.0;
        double imaginary = 0.0;
        for (const double value : windowed)
        {
            real += value * oscillatorReal;
            imaginary += value * oscillatorImaginary;
            const double nextReal = oscillatorReal * stepReal
                                  - oscillatorImaginary * stepImaginary;
            oscillatorImaginary = oscillatorReal * stepImaginary
                                 + oscillatorImaginary * stepReal;
            oscillatorReal = nextReal;
        }
        return real * real + imaginary * imaginary;
    };

    double lower = expectedHz * std::exp2(-searchCents / 1200.0);
    double upper = expectedHz * std::exp2(searchCents / 1200.0);
    constexpr double golden = 0.6180339887498948482;
    double left = upper - golden * (upper - lower);
    double right = lower + golden * (upper - lower);
    double leftPower = powerAt(left);
    double rightPower = powerAt(right);
    for (int iteration = 0; iteration < 42; ++iteration)
    {
        if (leftPower < rightPower)
        {
            lower = left;
            left = right;
            leftPower = rightPower;
            right = lower + golden * (upper - lower);
            rightPower = powerAt(right);
        }
        else
        {
            upper = right;
            right = left;
            rightPower = leftPower;
            left = upper - golden * (upper - lower);
            leftPower = powerAt(left);
        }
    }
    return 0.5 * (lower + upper);
}

double spectralAmplitudeAt(const Audio& audio, double frequency,
                           double beginSeconds, double endSeconds)
{
    const int begin = std::clamp(
        static_cast<int>(beginSeconds * sampleRate), 0,
        static_cast<int>(audio.left.size()));
    const int end = std::clamp(
        static_cast<int>(endSeconds * sampleRate), begin,
        static_cast<int>(audio.left.size()));
    const double angle = -2.0 * std::numbers::pi * frequency / sampleRate;
    const double stepReal = std::cos(angle);
    const double stepImaginary = std::sin(angle);
    double oscillatorReal = 1.0;
    double oscillatorImaginary = 0.0;
    double real = 0.0;
    double imaginary = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const int local = sample - begin;
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * local / std::max(1, end - begin - 1));
        const auto index = static_cast<std::size_t>(sample);
        const double value = 0.5 * window
            * (audio.left[index] + audio.right[index]);
        real += value * oscillatorReal;
        imaginary += value * oscillatorImaginary;
        const double nextReal = oscillatorReal * stepReal
                              - oscillatorImaginary * stepImaginary;
        oscillatorImaginary = oscillatorReal * stepImaginary
                             + oscillatorImaginary * stepReal;
        oscillatorReal = nextReal;
    }
    return std::hypot(real, imaginary);
}

void testSilenceAndFiniteOutput()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    std::vector<float> left(blockSize, 1.0f);
    std::vector<float> right(blockSize, 1.0f);
    engine.process(left.data(), right.data(), blockSize);
    expect(std::all_of(left.begin(), left.end(), [] (float value)
        { return value == 0.0f; }), "a reset engine was not exactly silent");
    expect(std::all_of(right.begin(), right.end(), [] (float value)
        { return value == 0.0f; }), "right reset channel was not exactly silent");

    engine.noteOn(40, 0.9f);
    engine.process(left.data(), right.data(), blockSize);
    expect(std::all_of(left.begin(), left.end(), [] (float value)
        { return std::isfinite(value); }), "note output contained NaN or infinity");
}

void testPlayableRangeFollowsTuning()
{
    acustra::AcustraEngine standard;
    standard.prepare(sampleRate, blockSize);
    standard.noteOn(38, 0.8f);
    expect(standard.getActiveVoiceCount() == 0,
           "standard tuning accepted a note below its lowest string");

    acustra::AcustraEngine dropD;
    dropD.prepare(sampleRate, blockSize);
    acustra::EngineParameters parameters;
    parameters.tuning = acustra::Tuning::DropD;
    dropD.setParameters(parameters);
    dropD.noteOn(38, 0.8f);
    expect(dropD.getActiveVoiceCount() == 1,
           "Drop D did not expose its physical low D string");
}

void testSteelRetuningPreservesStringMass()
{
    const auto standard = acustra::AcustraEngineTestAccess::retunedLowSteel(
        acustra::Tuning::Standard);
    const auto dropD = acustra::AcustraEngineTestAccess::retunedLowSteel(
        acustra::Tuning::DropD);
    const double frequencyRatio = std::exp2(-2.0 / 12.0);
    expect(std::abs(dropD.impedance / standard.impedance - frequencyRatio)
               < 2.0e-5,
           "Drop-D steel impedance did not preserve linear mass");
    expect(std::abs(dropD.tailStiffness / standard.tailStiffness
                    - frequencyRatio * frequencyRatio) < 2.0e-5,
           "Drop-D steel tension did not follow the retuned frequency");
    expect(std::abs(dropD.inharmonicity / standard.inharmonicity
                    - 1.0 / (frequencyRatio * frequencyRatio)) < 2.0e-4,
           "Drop-D steel stiffness retained standard-tuning tension");
}

void testAudiblePhysicalDecay()
{
    const auto audio = render({}, 40, 0.9f, 2.0);
    const double ordinaryPeak = peak(audio);
    expect(ordinaryPeak > 0.015,
           "a steel E2 pluck fell below the calibrated output reference: "
               + std::to_string(ordinaryPeak));
    const double early = rms(audio, 1200, 7200);
    const double late = rms(audio, 72000, 90000);
    expect(early > 1.0e-7, "pluck had no measurable early body response");
    expect(late < early, "an unforced held string gained energy over time");
    expect(ordinaryPeak < 2.0,
           "ordinary pluck exceeded the safety headroom");
}

void testPhysicalPluckOnsetIsBounded()
{
    for (const auto material : { acustra::StringMaterial::Nylon,
                                 acustra::StringMaterial::Steel })
    {
        acustra::EngineParameters parameters;
        parameters.stringMaterial = material;
        parameters.touch = material == acustra::StringMaterial::Nylon
            ? 0.08f : 0.72f;
        for (int midiNote = 40; midiNote <= 84; ++midiNote)
        {
            const auto audio = render(parameters, midiNote, 1.0f, 0.10);
            // The released-from-rest initial condition and derivative priming
            // must not manufacture a keyed discontinuity at note-on.
            expect(std::max(std::abs(audio.left.front()),
                            std::abs(audio.right.front())) < 1.0e-9f,
                   "physical pluck did not begin from rest at MIDI "
                       + std::to_string(midiNote));
            double maximumStep = 0.0;
            const int end = static_cast<int>(0.100 * sampleRate);
            for (int sample = 1; sample < end; ++sample)
            {
                const auto index = static_cast<std::size_t>(sample);
                const auto previous = static_cast<std::size_t>(sample - 1);
                maximumStep = std::max(maximumStep,
                    static_cast<double>(std::max(
                        std::abs(audio.left[index] - audio.left[previous]),
                        std::abs(audio.right[index] - audio.right[previous]))));
            }
            // The physical pick/contact burst can have a steep legitimate
            // derivative, but must remain inside the safety envelope.
            expect(maximumStep < 0.25,
                   "physical pluck exceeded the bounded transient gate at MIDI "
                       + std::to_string(midiNote)
                       + (material == acustra::StringMaterial::Steel
                              ? " steel: " : " nylon: ")
                       + std::to_string(maximumStep));
        }
    }
}

void testBridgeObservableIsSampleRateNormalised()
{
    auto bodyOnlyCalibration = acustra::fittedPhysicalCalibration;
    bodyOnlyCalibration.directGain = 0.0f;
    for (const auto material : { acustra::StringMaterial::Steel,
                                 acustra::StringMaterial::Nylon })
    {
        const std::string materialName = material == acustra::StringMaterial::Steel
            ? "steel" : "nylon";
        for (const bool anchoredTouch : { false, true })
        {
            acustra::EngineParameters parameters;
            parameters.stringMaterial = material;
            if (anchoredTouch)
                parameters.touch = material == acustra::StringMaterial::Steel
                    ? 0.72f : 0.08f;
            for (const int midiNote : { 40, 52, 64, 83 })
            {
                const auto reference = renderAtRate(
                    parameters, midiNote, 0.84f, 0.25, 48000.0,
                    blockSize, true, bodyOnlyCalibration);
                const double referenceRms = rms(reference, 2400, 9600);
                const double referenceBand = onsetBandRms(reference, 48000.0);
                const double referencePeak = peak(reference, 0, 960);
                const double referenceEdge = differenceRms(reference, 1, 960)
                    / std::max(rms(reference, 0, 960), 1.0e-12);
                for (const double rate : { 44100.0, 96000.0,
                                           192000.0, 384000.0 })
                {
                    const auto audio = renderAtRate(
                        parameters, midiNote, 0.84f, 0.25, rate,
                        blockSize, true, bodyOnlyCalibration);
                    const double levelRatio = rms(audio,
                        static_cast<int>(0.05 * rate),
                        static_cast<int>(0.20 * rate))
                        / std::max(referenceRms, 1.0e-12);
                    const double bandRatio = onsetBandRms(audio, rate)
                        / std::max(referenceBand, 1.0e-12);
                    const double onsetRatio = peak(audio, 0,
                        static_cast<int>(0.02 * rate))
                        / std::max(referencePeak, 1.0e-12);
                    const double edge = differenceRms(audio, 1,
                        static_cast<int>(0.02 * rate))
                        * (rate / 48000.0)
                        / std::max(rms(audio, 0,
                            static_cast<int>(0.02 * rate)), 1.0e-12);
                    const double edgeRatio = edge
                        / std::max(referenceEdge, 1.0e-12);
                    const std::string context = materialName + " MIDI "
                        + std::to_string(midiNote) + " at "
                        + std::to_string(static_cast<int>(rate)) + " Hz"
                        + (anchoredTouch ? " anchored touch" : " default touch");
                    expect(levelRatio > 0.78 && levelRatio < 1.30,
                           "sustain level changed with host rate for " + context
                               + ": " + std::to_string(levelRatio));
                    expect(bandRatio > 0.80 && bandRatio < 1.20,
                           "audible onset energy changed with host rate for "
                               + context + ": " + std::to_string(bandRatio));
                    expect(onsetRatio < 1.43,
                           "onset peak grew with host rate for " + context
                               + ": " + std::to_string(onsetRatio));
                    expect(edgeRatio < 1.60,
                           "onset edge grew with host rate for " + context
                               + ": " + std::to_string(edgeRatio));
                }
            }
        }
    }
}

void testZeroWidthCollapsesToMono()
{
    acustra::EngineParameters parameters;
    parameters.stereoWidth = 0.0f;
    const auto audio = render(parameters, 52, 0.8f, 0.4);
    const int begin = static_cast<int>(0.18 * sampleRate);
    const double mono = rms(audio, begin, static_cast<int>(audio.left.size()));
    const double difference = stereoDifferenceRms(
        audio, begin, static_cast<int>(audio.left.size()));
    expect(difference < 1.0e-3 * std::max(mono, 1.0e-12),
           "Stereo Width zero did not collapse all radiation paths to mono");
}

void testReleaseEventuallyReturnsTheString()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.noteOn(52, 0.8f); // fretted, therefore finger-damped on release
    std::vector<float> left(blockSize);
    std::vector<float> right(blockSize);
    for (int block = 0; block < 40; ++block)
        engine.process(left.data(), right.data(), blockSize);
    engine.noteOff(52);
    for (int block = 0; block < 1200; ++block)
        engine.process(left.data(), right.data(), blockSize);
    expect(engine.getActiveVoiceCount() == 0,
           "released fretted string never returned to sympathetic open state");
}

void testPhysicalVoiceOwnsReleaseLifecycle()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    std::vector<float> left(blockSize);
    std::vector<float> right(blockSize);

    engine.noteOn(52, 0.8f);
    engine.noteOn(52, 0.7f);
    engine.noteOff(52);
    for (int block = 0; block < 140; ++block)
        engine.process(left.data(), right.data(), blockSize);
    expect(engine.getActiveVoiceCount() == 1,
           "one duplicate-note owner released both key instances");

    engine.noteOff(52);
    for (int block = 0; block < 115; ++block)
        engine.process(left.data(), right.data(), blockSize);
    expect(engine.getActiveVoiceCount() == 0,
           "allocator ownership outlived the audible physical release");

    engine.noteOn(52, 0.8f);
    engine.setSustainPedal(true);
    engine.noteOff(52);
    for (int block = 0; block < 140; ++block)
        engine.process(left.data(), right.data(), blockSize);
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal failed to retain physical-note ownership");
    engine.setSustainPedal(false);
    for (int block = 0; block < 115; ++block)
        engine.process(left.data(), right.data(), blockSize);
    expect(engine.getActiveVoiceCount() == 0,
           "pedal-up did not release physical-note ownership");
}

void testMidiChannelOwnershipAndAdditiveBend()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.noteOn(52, 0.8f, 2);
    engine.noteOn(52, 0.8f, 3);
    expect(engine.getActiveVoiceCount() == 2,
           "same pitch on two member channels shared one owner");
    engine.noteOff(52, 2);
    engine.allSoundOff(2);
    expect(engine.getActiveVoiceCount() == 1,
           "member Note Off or All Sound Off affected another owner");

    const double additive = acustra::AcustraEngineTestAccess::channelBentDelay(
        0.5f, 0.5f);
    const double masterOnly = acustra::AcustraEngineTestAccess::channelBentDelay(
        1.0f, 0.0f);
    const double memberOnly = acustra::AcustraEngineTestAccess::channelBentDelay(
        0.0f, 0.5f);
    expect(std::abs(additive - masterOnly) < 1.0e-6,
           "channel 1 master and member pitch bends were not additive");
    expect(std::abs(additive - memberOnly) > 1.0e-3,
           "member pitch bend did not reach its owned string");

    const double conventionalBaseline =
        acustra::AcustraEngineTestAccess::conventionalChannelDelay(0.0f, 0.0f);
    const double unrelatedMaster =
        acustra::AcustraEngineTestAccess::conventionalChannelDelay(12.0f, 0.0f);
    const double ownedConventional =
        acustra::AcustraEngineTestAccess::conventionalChannelDelay(0.0f, 12.0f);
    expect(std::abs(conventionalBaseline - unrelatedMaster) < 1.0e-6
               && std::abs(conventionalBaseline - ownedConventional) > 1.0e-3,
           "conventional channel 1 acted as an MPE master before zone setup");

    const auto mpe = acustra::AcustraEngineTestAccess::bendLifecycle(true);
    expect(mpe.frozen && mpe.pedalHeld
               && std::abs(mpe.frozenMemberBend - 5.0f) < 1.0e-6f,
           "MPE Note Off did not latch the performed member bend under pedal");
    expect(std::abs(mpe.heldDelay - mpe.afterMemberTrafficDelay) < 1.0e-6,
           "idle member wheel traffic retuned a released MPE tail");
    expect(mpe.afterMasterTrafficDelay < mpe.afterMemberTrafficDelay - 1.0e-3,
           "live MPE master bend did not move the frozen member tail");
    expect(!mpe.reusedFingerFrozen && mpe.reusedFingerDelay
               > mpe.afterMasterTrafficDelay + 1.0e-3,
           "a reused MPE channel inherited the old tail's frozen member bend");

    const auto conventional =
        acustra::AcustraEngineTestAccess::bendLifecycle(false);
    expect(!conventional.frozen
               && std::abs(conventional.heldDelay
                           - conventional.afterMemberTrafficDelay) > 1.0e-3,
           "a conventional release tail stopped following its channel wheel");

    expect(acustra::AcustraEngineTestAccess::lowerZoneTransitionCounts()
               == std::array<int, 3> { 1, 0, 0 },
           "lower-zone enable/resize/deactivate did not stop affected channels");
    expect(acustra::AcustraEngineTestAccess::allSoundOffPreservesControllers(),
           "All Sound Off reset pitch-bend or sustain controller state");
    const double maximumUp =
        acustra::AcustraEngineTestAccess::channelBentDelay(96.0f, 96.0f);
    const double maximumDown =
        acustra::AcustraEngineTestAccess::channelBentDelay(-96.0f, -96.0f);
    expect(std::isfinite(maximumUp) && std::isfinite(maximumDown)
               && maximumUp >= 3.0 && maximumDown <= 8189.0,
           "legal 96-semitone MPE ranges escaped the finite delay bounds");
}

void testSharedBodyExcitesIdleStrings()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    // E3 is played on the D string, leaving low E idle at the note's second
    // harmonic. This resonant case exposes the frequency-selective coupling.
    engine.noteOn(52, 1.0f);
    std::vector<float> left(blockSize);
    std::vector<float> right(blockSize);
    int maximumSympatheticStrings = 0;
    for (int block = 0; block < 220; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        maximumSympatheticStrings = std::max(maximumSympatheticStrings,
            engine.getSympatheticStringCount());
    }
    expect(maximumSympatheticStrings > 0,
           "shared bridge/body did not excite any idle open string");
}

void testSympatheticStringsAreAudibleButBounded()
{
    acustra::AcustraEngine activeOn;
    acustra::AcustraEngine activeOff;
    activeOn.prepare(sampleRate, blockSize);
    activeOff.prepare(sampleRate, blockSize);
    activeOff.setSympatheticStringsEnabled(false);
    activeOn.noteOn(52, 1.0f);
    activeOff.noteOn(52, 1.0f);
    std::vector<float> probeOnLeft(blockSize);
    std::vector<float> probeOnRight(blockSize);
    std::vector<float> probeOffLeft(blockSize);
    std::vector<float> probeOffRight(blockSize);
    double maximumActiveForce = 0.0;
    double maximumActiveForceDifference = 0.0;
    double maximumMutedSympatheticForce = 0.0;
    for (int block = 0; block < 128; ++block)
    {
        activeOn.process(probeOnLeft.data(), probeOnRight.data(), blockSize);
        activeOff.process(probeOffLeft.data(), probeOffRight.data(), blockSize);
        const double onForce = activeOn.getLastBridgeBodyForce();
        const double offForce = activeOff.getLastBridgeBodyForce();
        maximumActiveForce = std::max(maximumActiveForce,
                                      std::abs(onForce));
        maximumActiveForceDifference = std::max(
            maximumActiveForceDifference, std::abs(onForce - offForce));
        maximumMutedSympatheticForce = std::max(
            maximumMutedSympatheticForce,
            static_cast<double>(std::abs(
                activeOff.getLastSympatheticRadiationForce())));
    }
    // The idle strings are members of the junction, so taking them out
    // changes the load the played string sees: the bypass must move the
    // bridge force, and it must move it by less than the note itself.
    expect(maximumActiveForceDifference > 1.0e-6 * maximumActiveForce,
           "sympathetic bypass did not unload the bridge");
    expect(maximumActiveForceDifference < maximumActiveForce,
           "sympathetic bypass changed the bridge force by more than the note");
    expect(maximumMutedSympatheticForce == 0.0,
           "sympathetic bypass leaked idle-string radiation");

    constexpr double seconds = 4.0;
    const int begin = static_cast<int>(0.30 * sampleRate);
    const int end = static_cast<int>(seconds * sampleRate);
    const auto resonantOn = renderWithSympatheticStrings(52, true, seconds);
    const auto resonantOff = renderWithSympatheticStrings(52, false, seconds);
    const auto offResonantOn = renderWithSympatheticStrings(53, true, seconds);
    const auto offResonantOff = renderWithSympatheticStrings(53, false, seconds);
    const double resonantOnRms = rms(resonantOn, begin, end);
    const double resonantOffRms = rms(resonantOff, begin, end);
    const double offResonantOnRms = rms(offResonantOn, begin, end);
    const double resonantRatio = differenceRms(
        resonantOn, resonantOff, begin, end)
        / std::max(resonantOnRms, 1.0e-12);
    const double offResonantRatio = differenceRms(
        offResonantOn, offResonantOff, begin, end)
        / std::max(offResonantOnRms, 1.0e-12);
    std::cout << "Acustra sympathetic tail ratios: resonant="
              << resonantRatio << ", off-resonant=" << offResonantRatio
              << ", resonant RMS on/off=" << resonantOnRms << '/'
              << resonantOffRms << '\n';
    expect(resonantRatio > 0.05,
           "resonant open-string radiation was below -26 dB of the tail");
    // This ceiling is on the balance of the tail, not on the coupling. It
    // moves with the body: holding the sympathetic path fixed and only
    // tilting the body's radiation down takes it from 0.806 to 0.927, while
    // doubling the low-mode gain at a fixed tilt moves it 0.806 to 0.819. A
    // darker, bassier body favours the open strings' long low ring over a
    // fretted note's own tail, which is a guitar doing what a guitar does.
    // It was re-pinned from 0.90 when a listening verdict chose that body.
    expect(offResonantRatio < 0.30,
           "off-resonant open strings coloured too much of the tail");
    // The assertions that make this a sympathy test rather than a loudness
    // one are these two, and they are tightened in exchange.
    expect(resonantRatio > 5.0 * offResonantRatio,
           "open-string radiation did not discriminate a harmonic match");
}

void testPassiveBridgeBranchesBalance()
{
    for (const auto material : { acustra::StringMaterial::Steel,
                                 acustra::StringMaterial::Nylon })
    {
        acustra::AcustraEngine engine;
        engine.prepare(sampleRate, 1);
        acustra::EngineParameters parameters;
        parameters.stringMaterial = material;
        parameters.touch = material == acustra::StringMaterial::Steel
            ? 0.72f : 0.08f;
        engine.setParameters(parameters);
        engine.noteOn(40, 0.72f);

        double totalWork = 0.0;
        double bodyWork = 0.0;
        double tailWork = 0.0;
        double minimumTotalWork = 0.0;
        double minimumBodyWork = 0.0;
        double minimumTailWork = 0.0;
        double maximumTailWork = 0.0;
        double maximumForce = 0.0;
        double maximumBalanceError = 0.0;
        double maximumSympatheticForce = 0.0;
        for (int sample = 0; sample < static_cast<int>(4.0 * sampleRate);
             ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process(&left, &right, 1);
            totalWork += engine.getLastBridgePower() / sampleRate;
            bodyWork += engine.getLastBridgeBodyPower() / sampleRate;
            tailWork += engine.getLastBridgeTailPower() / sampleRate;
            minimumTotalWork = std::min(minimumTotalWork, totalWork);
            minimumBodyWork = std::min(minimumBodyWork, bodyWork);
            minimumTailWork = std::min(minimumTailWork, tailWork);
            maximumTailWork = std::max(maximumTailWork, tailWork);
            const double totalForce = engine.getLastBridgeReactionForce();
            const double branchForce = engine.getLastBridgeBodyForce()
                                     + engine.getLastBridgeTailForce();
            maximumForce = std::max(maximumForce, std::abs(totalForce));
            maximumBalanceError = std::max(maximumBalanceError,
                std::abs(totalForce - branchForce));
            maximumSympatheticForce = std::max(maximumSympatheticForce,
                static_cast<double>(std::abs(
                    engine.getLastSympatheticRadiationForce())));
        }

        const std::string name = material == acustra::StringMaterial::Steel
            ? "steel" : "nylon";
        expect(minimumTotalWork >= -1.0e-14,
               name + " bridge termination generated cumulative work");
        expect(minimumBodyWork >= -1.0e-14,
               name + " measured body branch generated cumulative work");
        expect(minimumTailWork >= -1.0e-4 * maximumTailWork - 1.0e-15,
               name + " xi_b tail acquired negative stored energy");
        expect(maximumBalanceError < 1.0e-4 * maximumForce + 1.0e-10,
               name + " bridge/body/tail force balance did not close");
        expect(maximumSympatheticForce == 0.0,
               name + " an idle string radiated outside the junction");
    }
}

void testConstructionControlsChangeTheModel()
{
    acustra::EngineParameters base;
    base.shape = acustra::BodyShape::Parlor;
    base.bodyMaterial = acustra::BodyMaterial::Cedar;
    const auto parlor = render(base, 52, 0.82f, 0.7);
    base.shape = acustra::BodyShape::Jumbo;
    base.bodyMaterial = acustra::BodyMaterial::Maple;
    const auto jumbo = render(base, 52, 0.82f, 0.7);
    // The shapes differ most in the low body modes, which the 3.25 mm
    // anchor stub drives less than the 17 mm spring did: 0.070 here against
    // 0.09 before, so the floor sits below that rather than above it.
    expect(normalisedDifference(parlor, jumbo) > 0.05,
           "Shape/Material acted like inert labels");

    base.stringMaterial = acustra::StringMaterial::Nylon;
    const auto nylon = render(base, 52, 0.82f, 0.7);
    base.stringMaterial = acustra::StringMaterial::Steel;
    const auto steel = render(base, 52, 0.82f, 0.7);
    expect(normalisedDifference(nylon, steel) > 0.08,
           "nylon and steel did not use distinct string constructions");
}

void testAgeRemovesUpperStringEnergy()
{
    acustra::EngineParameters parameters;
    parameters.stringAge = 0.0f;
    const auto fresh = render(parameters, 64, 0.88f, 0.45);
    parameters.stringAge = 1.0f;
    const auto old = render(parameters, 64, 0.88f, 0.45);
    const int begin = 2400;
    const int end = 18000;
    const double freshRatio = differenceRms(fresh, begin, end)
                            / std::max(rms(fresh, begin, end), 1.0e-12);
    const double oldRatio = differenceRms(old, begin, end)
                          / std::max(rms(old, begin, end), 1.0e-12);
    expect(oldRatio < freshRatio,
           "old strings were not spectrally darker than fresh strings");
}

void testMaterialSpecificAttackPitchIsBoundedAndVelocityResponsive()
{
    const auto quietSteel = acustra::AcustraEngineTestAccess::attackPitchTrace(
        acustra::StringMaterial::Steel, 0.25f, 0);
    const auto hardSteel = acustra::AcustraEngineTestAccess::attackPitchTrace(
        acustra::StringMaterial::Steel, 1.0f,
        static_cast<int>(0.45 * sampleRate));
    expect(quietSteel.size() == 1 && hardSteel.size() > 2,
           "steel attack-pitch trace did not capture the played voice");
    if (!quietSteel.empty() && !hardSteel.empty())
    {
        expect(hardSteel.front().energy > 2.0 * quietSteel.front().energy
                   && hardSteel.front().cents
                        > 2.0 * quietSteel.front().cents,
               "Kirchhoff-Carrier pitch cue did not follow note velocity");
        expect(std::abs(hardSteel.front().cents - 4.0) < 1.0,
               "fitted steel displacement missed the former onset cue: "
                   + std::to_string(hardSteel.front().cents) + " cents");

        for (std::size_t index = 0; index < hardSteel.size(); ++index)
        {
            const auto& state = hardSteel[index];
            expect(std::isfinite(state.energy) && state.energy >= 0.0
                       && std::isfinite(state.cents) && state.cents >= 0.0
                       && state.cents <= 20.0,
                   "energy-following attack pitch escaped its finite bounds");
            if (index > 0)
            {
                expect(state.energy
                           <= hardSteel[index - 1].energy + 1.0e-7,
                       "attack slope-energy envelope increased without a repick");
                expect(state.cents
                           <= hardSteel[index - 1].cents + 1.0e-5,
                       "attack pitch increased without a repick");
            }
        }
        expect(hardSteel.back().cents < 0.8 * hardSteel.front().cents,
               "energy-following attack pitch did not relax during sustain");
    }

    for (const float velocity : { 0.25f, 1.0f })
    {
        const auto nylon = acustra::AcustraEngineTestAccess::attackPitchTrace(
            acustra::StringMaterial::Nylon, velocity,
            acustra::AcustraEngineTestAccess::controlPeriodSamples());
        expect(nylon.size() == 2,
               "nylon attack-pitch trace missed one control step");
        if (nylon.size() != 2)
            continue;
        const float touch = std::clamp(0.58f
            + acustra::fittedPhysicalCalibration.nylon.velocityBrightnessDepth
                * (velocity - 0.5f), 0.0f, 1.0f);
        const float expected = 3.0f * velocity * velocity
            * (0.72f + 0.28f * touch);
        const float expectedDecay = std::exp(
            -static_cast<float>(
                acustra::AcustraEngineTestAccess::controlPeriodSamples())
            / (0.075f * static_cast<float>(sampleRate)));
        expect(nylon.front().energy == 0.0
                   && std::abs(nylon.front().cents - expected) < 1.0e-6
                   && std::abs(nylon.front().decay - expectedDecay) < 1.0e-7,
               "nylon did not restore the exact authored onset cue");
        expect(nylon.back().energy == 0.0
                   && std::abs(nylon.back().cents
                               - nylon.front().cents * expectedDecay) < 1.0e-6,
               "nylon authored pitch cue missed its 75 ms control decay");
    }

    acustra::EngineParameters nylonParameters;
    nylonParameters.stringMaterial = acustra::StringMaterial::Nylon;
    auto displacementOff = acustra::fittedPhysicalCalibration;
    displacementOff.steelDisplacementScaleMetres = 0.0f;
    auto displacementMaximum = displacementOff;
    displacementMaximum.steelDisplacementScaleMetres = 0.04f;
    const auto nylonOff = renderCalibrated(
        nylonParameters, displacementOff, 52, 0.82f, 0.20);
    const auto nylonMaximum = renderCalibrated(
        nylonParameters, displacementMaximum, 52, 0.82f, 0.20);
    expect(nylonOff.left == nylonMaximum.left
               && nylonOff.right == nylonMaximum.right,
           "steel displacement calibration changed a nylon render");
}

void testSteadyPitchIsCompensated()
{
    for (const int midiNote : { 40, 45, 52, 64 })
    {
        const double expected = 440.0
            * std::exp2((static_cast<double>(midiNote) - 69.0) / 12.0);
        const auto audio = render({}, midiNote, 0.72f, 1.0,
                                  blockSize, false);
        // Autocorrelation returns a sharper weighted pseudo-period for a
        // deliberately stretched string. Search H1 itself instead.
        const double actual = spectralPeakFrequency(
            audio, expected, 25.0, 0.55, 0.92);
        const double cents = 1200.0 * std::log2(actual / expected);
        expect(std::abs(cents) < 1.5,
               "phase-compensated loop missed steady pitch for MIDI "
                   + std::to_string(midiNote) + " by "
                   + std::to_string(cents) + " cents");
    }
}

void testPhysicalSustainSettlesNearRequestedPitch()
{
    // Measure the string that was played. The idle strings ring at their own
    // open pitches, and their harmonics are just intervals while the fretboard
    // is equal-tempered: the low E's third harmonic is a just twelfth, two
    // cents above an equal-tempered B3, and playing B3 drives it. The peak of
    // the summed radiation therefore sits a couple of cents sharp of the note,
    // which is what a guitar does and not a tuning error. Reading it as one
    // put a 2-cent bound on a real behaviour and left the played string's own
    // pitch untested.
    const auto isolated = [] (int midiNote, bool sympathetic)
    {
        acustra::AcustraEngine engine;
        engine.prepare(sampleRate, blockSize);
        engine.setSympatheticStringsEnabled(sympathetic);
        engine.noteOn(midiNote, 0.72f);
        const int samples = static_cast<int>(1.5 * sampleRate);
        Audio audio { std::vector<float>(static_cast<std::size_t>(samples)),
                      std::vector<float>(static_cast<std::size_t>(samples)) };
        for (int offset = 0; offset < samples; offset += blockSize)
            engine.process(audio.left.data() + offset,
                           audio.right.data() + offset,
                           std::min(blockSize, samples - offset));
        return audio;
    };
    for (const int midiNote : { 40, 45, 52, 59, 64, 71 })
    {
        const double expected = 440.0
            * std::exp2((static_cast<double>(midiNote) - 69.0) / 12.0);
        const double alone = 1200.0 * std::log2(spectralPeakFrequency(
            isolated(midiNote, false), expected, 20.0, 0.82, 1.32) / expected);
        expect(std::abs(alone) < 1.0,
               "the played steel string missed settled pitch for MIDI "
                   + std::to_string(midiNote) + " by "
                   + std::to_string(alone) + " cents");
        // With the idle strings sounding the peak may be pulled, but only as
        // far as their own just harmonics reach.
        const double loaded = 1200.0 * std::log2(spectralPeakFrequency(
            isolated(midiNote, true), expected, 20.0, 0.82, 1.32) / expected);
        // At exact coincidence (E3 on the low E's second partial, B3 on its
        // third) the played and idle strings form a coupled pair whose modes
        // split about the common frequency; the played string's energy sits
        // in the lower member, measured at -4.8 cents with the upper member
        // 12 cents above it and 24 dB down. That split is the coupling, not
        // a tuning error, so the bound admits it.
        expect(std::abs(loaded) < 6.0,
               "sympathetic strings pulled MIDI " + std::to_string(midiNote)
                   + " by " + std::to_string(loaded) + " cents");
    }
}

void testLoadedE2IsCentredAndNotSplit()
{
    const double expected = 440.0 * std::exp2((40.0 - 69.0) / 12.0);
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, 1);
    engine.noteOn(40, 0.72f);
    Audio audio { std::vector<float>(static_cast<std::size_t>(4.0 * sampleRate)),
                  std::vector<float>(static_cast<std::size_t>(4.0 * sampleRate)) };
    for (std::size_t sample = 0; sample < audio.left.size(); ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
        const float force = engine.getLastBridgeReactionForce();
        audio.left[sample] = force;
        audio.right[sample] = force;
    }
    const double early = spectralPeakFrequency(
        audio, expected, 8.0, 0.05, 0.20);
    const double late = spectralPeakFrequency(
        audio, expected, 8.0, 2.0, 4.0);
    const double earlyCents = 1200.0 * std::log2(early / expected);
    const double lateCents = 1200.0 * std::log2(late / expected);
    expect(std::abs(earlyCents) < 3.0,
           "loaded steel E2 early mode missed tuning by "
               + std::to_string(earlyCents) + " cents");
    expect(std::abs(lateCents) < 1.5,
           "loaded steel E2 late mode missed tuning by "
               + std::to_string(lateCents) + " cents");

    const double primary = spectralAmplitudeAt(audio, late, 2.0, 4.0);
    double side = 0.0;
    for (int cents = -120; cents <= 120; cents += 2)
    {
        if (std::abs(cents) < 25)
            continue;
        const double frequency = late * std::exp2(cents / 1200.0);
        side = std::max(side,
            spectralAmplitudeAt(audio, frequency, 2.0, 4.0));
    }
    const double sideDb = 20.0 * std::log10(
        std::max(side, 1.0e-30) / std::max(primary, 1.0e-30));
    expect(sideDb < -25.0,
           "loaded steel E2 retained a strong split side mode at "
               + std::to_string(sideDb) + " dB");
}

void testSteelDispersionTracksTheStiffStringLaw()
{
    acustra::EngineParameters parameters;
    parameters.stringMaterial = acustra::StringMaterial::Steel;
    const auto audio = render(parameters, 40, 0.72f, 1.0,
                              blockSize, false);
    const double fundamental = 440.0 * std::exp2((40.0 - 69.0) / 12.0);
    constexpr double length = 0.648;
    constexpr double tension = 110.759;
    constexpr double youngsModulus = 2.0e11;
    constexpr double bendingDiameter = 0.477159e-3;
    const double diameterSquared = bendingDiameter * bendingDiameter;
    const double inharmonicity = acustra::fittedPhysicalCalibration.steel
        .stiffnessScale * std::pow(std::numbers::pi, 3.0)
        * youngsModulus * diameterSquared * diameterSquared
        / (64.0 * tension * length * length);

    for (int partial = 2; partial <= 12; ++partial)
    {
        const double number = static_cast<double>(partial);
        const double expected = fundamental * number * std::sqrt(
            (1.0 + inharmonicity * number * number)
            / (1.0 + inharmonicity));
        const double actual = spectralPeakFrequency(
            audio, expected, 8.0, 0.55, 0.92);
        const double cents = 1200.0 * std::log2(actual / expected);
        expect(std::abs(cents) < 2.0,
               "steel E2 dispersion missed partial "
                   + std::to_string(partial) + " by "
                   + std::to_string(cents) + " cents");
    }
}

double loopPhase(const acustra::AcustraEngineTestAccess::StringLoopSnapshot& loop,
                 double omega)
{
    const auto mixedPolePhase = [omega] (double coefficient, double mix)
    {
        const double denominatorReal = 1.0 - coefficient * std::cos(omega);
        const double denominatorImaginary = coefficient * std::sin(omega);
        const double norm = denominatorReal * denominatorReal
                          + denominatorImaginary * denominatorImaginary;
        const double lowReal = (1.0 - coefficient) * denominatorReal / norm;
        const double lowImaginary = -(1.0 - coefficient)
                                  * denominatorImaginary / norm;
        return -std::atan2(mix * lowImaginary,
                           (1.0 - mix) + mix * lowReal);
    };

    const int whole = static_cast<int>(loop.delay);
    const double fraction = loop.delay - static_cast<double>(whole);
    const double squared = fraction * fraction;
    const double cubed = squared * fraction;
    const double weights[] {
        -0.5 * fraction + squared - 0.5 * cubed,
         1.0 - 2.5 * squared + 1.5 * cubed,
         0.5 * fraction + 2.0 * squared - 1.5 * cubed,
        -0.5 * squared + 0.5 * cubed
    };
    const double kernelReal = weights[0] * std::cos(omega) + weights[1]
                            + weights[2] * std::cos(omega)
                            + weights[3] * std::cos(2.0 * omega);
    const double kernelImaginary = weights[0] * std::sin(omega)
                                 - weights[2] * std::sin(omega)
                                 - weights[3] * std::sin(2.0 * omega);
    const double delayPhase = static_cast<double>(whole) * omega
                            - std::atan2(kernelImaginary, kernelReal);

    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double cosine2 = std::cos(2.0 * omega);
    const double sine2 = std::sin(2.0 * omega);
    const double numeratorPhase = std::atan2(
        -loop.dispersionA1 * sine - sine2,
        loop.dispersionA2 + loop.dispersionA1 * cosine + cosine2);
    const double denominatorPhase = std::atan2(
        -loop.dispersionA1 * sine - loop.dispersionA2 * sine2,
        1.0 + loop.dispersionA1 * cosine + loop.dispersionA2 * cosine2);
    double allpassPhase = denominatorPhase - numeratorPhase;
    while (allpassPhase < 0.0)
        allpassPhase += 2.0 * std::numbers::pi;
    while (allpassPhase >= 2.0 * std::numbers::pi)
        allpassPhase -= 2.0 * std::numbers::pi;

    return delayPhase
        + mixedPolePhase(loop.broadCoefficient, loop.broadMix)
        + mixedPolePhase(loop.highCoefficient, loop.highMix)
        + allpassPhase;
}

double loopResonance(
    const acustra::AcustraEngineTestAccess::StringLoopSnapshot& loop,
    int partial, double expectedOmega)
{
    double lower = expectedOmega * std::exp2(-12.0 / 1200.0);
    double upper = expectedOmega * std::exp2(12.0 / 1200.0);
    const double target = 2.0 * std::numbers::pi * partial;
    for (int iteration = 0; iteration < 52; ++iteration)
    {
        const double middle = 0.5 * (lower + upper);
        if (loopPhase(loop, middle) < target)
            lower = middle;
        else
            upper = middle;
    }
    return 0.5 * (lower + upper);
}

void testDispersionAcrossRatesMaterialsAndNotes()
{
    constexpr double rates[] { 44100.0, 48000.0, 384000.0 };
    for (const auto material : { acustra::StringMaterial::Nylon,
                                 acustra::StringMaterial::Steel })
    for (const int midiNote : { 40, 84 })
    for (const double rate : rates)
    {
        const bool steel = material == acustra::StringMaterial::Steel;
        const bool bass = midiNote == 40;
        const int openMidi = bass ? 40 : 64;
        const int fret = midiNote - openMidi;
        const double scaleLength = steel ? 0.648 : 0.650;
        const double diameter = steel
            ? (bass ? 0.477159e-3 : 0.305e-3)
            : (bass ? 1.150e-3 : 0.670e-3);
        const double density = bass ? 5900.0 : 1140.0;
        const double youngsModulus = steel ? 2.0e11
            : (bass ? 2.5e9 : 2.7e9);
        // Woodhouse, Acta Acustica 90 (2004) 945-965, Table I: E2 57e-6,
        // E4 130e-6 N*m^2 - see nylonBendingEI in AcustraEngine.cpp.
        const double nylonEI = bass ? 57.0e-6 : 130.0e-6;
        const double tension = [&]
        {
            if (steel)
                return bass ? 110.759 : 104.088;
            const double openFrequency = 440.0
                * std::exp2((static_cast<double>(openMidi) - 69.0) / 12.0);
            const double linearMass = density * std::numbers::pi * 0.25
                                    * diameter * diameter;
            return linearMass
                * std::pow(2.0 * scaleLength * openFrequency, 2.0);
        }();
        const double soundingLength = scaleLength
            * std::exp2(-static_cast<double>(fret) / 12.0);
        const double inharmonicity = steel
            ? acustra::fittedPhysicalCalibration.steel.stiffnessScale
                * std::pow(std::numbers::pi, 3.0)
                * youngsModulus * std::pow(diameter, 4.0)
                / (64.0 * tension * soundingLength * soundingLength)
            : std::numbers::pi * std::numbers::pi * nylonEI
                / (tension * soundingLength * soundingLength);
        const double fundamental = 440.0
            * std::exp2((static_cast<double>(midiNote) - 69.0) / 12.0);
        const auto loop = acustra::AcustraEngineTestAccess::configuredLoop(
            material, midiNote, rate);
        const double nominalOmega = 2.0 * std::numbers::pi
                                  * fundamental / rate;
        const double actualFundamental = loopResonance(loop, 1, nominalOmega);

        for (int partial = 2; partial <= 12; ++partial)
        {
            const double number = static_cast<double>(partial);
            const double expected = actualFundamental * number * std::sqrt(
                (1.0 + inharmonicity * number * number)
                / (1.0 + inharmonicity));
            const double actual = loopResonance(loop, partial, expected);
            const double cents = 1200.0 * std::log2(actual / expected);
            // Woodhouse's corrected per-string EI raises the trebles'
            // stiffness several-fold (see nylonBendingEI in AcustraEngine.cpp),
            // which widens the H1/H7/H11.5 collocation's own approximation
            // error at a top-fret treble note from the old ~1.2 cents to
            // ~2.8; still under a third of the H16+ gap already documented.
            expect(std::abs(cents) < 3.0,
                   std::string(steel ? "steel" : "nylon") + "/"
                       + std::to_string(static_cast<int>(rate)) + " MIDI "
                       + std::to_string(midiNote) + " partial "
                       + std::to_string(partial) + " missed by "
                       + std::to_string(cents) + " cents");
        }
    }
}

void testBlockPartitionIsDeterministic()
{
    const auto a = render({}, 47, 0.73f, 0.5, 1);
    const auto b = render({}, 47, 0.73f, 0.5, 257);
    expect(a.left == b.left && a.right == b.right,
           "host block partition changed deterministic engine output");
}

void testSampleRatesAndAutomationStayBounded()
{
    constexpr double rates[] { 8000.0, 44100.0, 48000.0,
                               96000.0, 192000.0, 384000.0 };
    for (const double rate : rates)
    {
        acustra::AcustraEngine engine;
        auto maximumDisplacement = acustra::fittedPhysicalCalibration;
        maximumDisplacement.steelDisplacementScaleMetres = 0.04f;
        engine.setPhysicalCalibration(maximumDisplacement);
        engine.prepare(rate, 73);
        acustra::EngineParameters parameters;
        std::vector<float> left(73);
        std::vector<float> right(73);
        for (int step = 0; step < 180; ++step)
        {
            parameters.shape = static_cast<acustra::BodyShape>((step / 17) % 4);
            parameters.bodyMaterial = static_cast<acustra::BodyMaterial>((step / 23) % 4);
            parameters.stringMaterial = static_cast<acustra::StringMaterial>((step / 41) % 2);
            parameters.stringAge = static_cast<float>((step * 37) % 101) / 100.0f;
            parameters.bodyAmount = static_cast<float>((step * 19) % 101) / 100.0f;
            parameters.stereoWidth = static_cast<float>((step * 29) % 101) / 100.0f;
            engine.setParameters(parameters);
            if (step % 31 == 0)
            {
                for (const int note : { 40, 45, 50, 55, 59, 64 })
                    engine.noteOn(note, 1.0f);
            }
            if (step % 47 == 0)
                engine.setPitchBend(step % 94 == 0 ? 2.0f : -2.0f);
            engine.process(left.data(), right.data(), 73);
            for (int sample = 0; sample < 73; ++sample)
            {
                const float l = left[static_cast<std::size_t>(sample)];
                const float r = right[static_cast<std::size_t>(sample)];
                expect(std::isfinite(l) && std::isfinite(r),
                       "automation produced non-finite output at "
                           + std::to_string(rate) + " Hz");
                expect(std::abs(l) < 4.0f && std::abs(r) < 4.0f,
                       "automation escaped the bounded output knee at "
                           + std::to_string(rate) + " Hz");
            }
            expect(engine.getActiveVoiceCount() <= acustra::AcustraEngine::stringCount,
                   "allocator exceeded six physical strings");
        }
    }
}

void testHostileParametersAreSanitised()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    acustra::EngineParameters parameters;
    parameters.shape = static_cast<acustra::BodyShape>(99);
    parameters.bodyMaterial = static_cast<acustra::BodyMaterial>(-4);
    parameters.stringMaterial = static_cast<acustra::StringMaterial>(12);
    parameters.tuning = static_cast<acustra::Tuning>(88);
    parameters.stringAge = std::numeric_limits<float>::quiet_NaN();
    parameters.pluckPosition = std::numeric_limits<float>::infinity();
    parameters.touch = -std::numeric_limits<float>::infinity();
    parameters.bodyAmount = 1000.0f;
    parameters.outputGain = 1000.0f;
    engine.setParameters(parameters);
    engine.noteOn(40, 1.0f);
    std::vector<float> left(blockSize);
    std::vector<float> right(blockSize);
    engine.process(left.data(), right.data(), blockSize);
    expect(std::all_of(left.begin(), left.end(), [] (float value)
        { return std::isfinite(value) && std::abs(value) <= 1.0f; }),
        "hostile parameters escaped sanitisation");
}

void testHostilePhysicalCalibrationIsSanitised()
{
    const auto uniformMaterial = [] (float value)
    {
        return acustra::MaterialCalibration {
            value, value, value, value, value, value, value
        };
    };
    const auto values = [] (const acustra::MaterialCalibration& material)
    {
        return std::array {
            material.stiffnessScale, material.fundamentalT60Scale,
            material.frequencyLossScale, material.apertureScale,
            material.transientScale, material.pluckDistanceScale,
            material.velocityBrightnessDepth
        };
    };
    const acustra::PhysicalCalibration lowSource {
        -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
        uniformMaterial(-100.0f), uniformMaterial(-100.0f), -100.0f, -100.0f,
        -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f
    };
    const acustra::PhysicalCalibration highSource {
        100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
        uniformMaterial(100.0f), uniformMaterial(100.0f), 100.0f, 100.0f,
        100.0f, 100.0f, 100.0f, 100.0f, 100000.0f, 100.0f
    };
    const auto sanitised = [] (acustra::PhysicalCalibration source)
    {
        acustra::AcustraEngine engine;
        engine.setPhysicalCalibration(source);
        return acustra::AcustraEngineTestAccess::calibration(engine);
    };
    const auto low = sanitised(lowSource);
    const auto high = sanitised(highSource);
    expect(std::array { low.bodyFrequencyScale, low.bodyQScale,
                        low.bridgeMobilityScale, low.residueTiltDbPerOctave,
                        low.directGain, low.apertureRegisterExponent,
                        low.lowBodyModeGain,
                        low.steelDisplacementScaleMetres,
                        low.steelFretT60Slope, low.highLossCutoffScale,
                        low.bridgeConductanceFloor,
                        low.bridgeConductanceCornerHz,
                        low.bridgeTailLengthMetres }
               == std::array { 0.96f, 0.05f, 0.25f, -6.0f, 0.0f, -1.0f,
                               0.25f, 0.0f, -0.06f, 0.5f, 0.0f, 100.0f,
                               0.00325f },
           "low physical calibration bounds were not enforced");
    expect(std::array { high.bodyFrequencyScale, high.bodyQScale,
                        high.bridgeMobilityScale, high.residueTiltDbPerOctave,
                        high.directGain, high.apertureRegisterExponent,
                        high.lowBodyModeGain,
                        high.steelDisplacementScaleMetres,
                        high.steelFretT60Slope, high.highLossCutoffScale,
                        high.bridgeConductanceFloor,
                        high.bridgeConductanceCornerHz,
                        high.bridgeTailLengthMetres }
               == std::array { 1.04f, 1.8f, 4.0f, 6.0f, 0.12f, 1.0f,
                               32.0f, 0.04f, 0.05f, 4.0f, 0.02f, 8000.0f,
                               0.060f },
           "high physical calibration bounds were not enforced");
    const std::array materialLow {
        0.25f, 0.4f, 0.35f, 0.35f, 0.0f, 0.7f, 0.0f
    };
    const std::array materialHigh {
        4.0f, 2.0f, 3.0f, 2.5f, 3.0f, 1.3f, 1.2f
    };
    expect(values(low.nylon) == materialLow && values(low.steel) == materialLow,
           "low material calibration bounds were not enforced");
    expect(values(high.nylon) == materialHigh
               && values(high.steel) == materialHigh,
           "high material calibration bounds were not enforced");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const acustra::PhysicalCalibration poisoned {
        nan, nan, nan, nan, nan, uniformMaterial(nan), uniformMaterial(nan),
        nan, nan, nan, nan, nan, nan, nan
    };
    const auto fallback = sanitised(poisoned);
    expect(std::array { fallback.bodyFrequencyScale, fallback.bodyQScale,
                        fallback.bridgeMobilityScale,
                        fallback.residueTiltDbPerOctave, fallback.directGain,
                        fallback.apertureRegisterExponent,
                        fallback.lowBodyModeGain,
                        fallback.steelDisplacementScaleMetres,
                        fallback.steelFretT60Slope,
                        fallback.highLossCutoffScale,
                        fallback.bridgeConductanceFloor,
                        fallback.bridgeConductanceCornerHz }
               == std::array {
                    acustra::fittedPhysicalCalibration.bodyFrequencyScale,
                    acustra::fittedPhysicalCalibration.bodyQScale,
                    acustra::fittedPhysicalCalibration.bridgeMobilityScale,
                    acustra::fittedPhysicalCalibration.residueTiltDbPerOctave,
                    acustra::fittedPhysicalCalibration.directGain,
                    acustra::fittedPhysicalCalibration.apertureRegisterExponent,
                    acustra::fittedPhysicalCalibration.lowBodyModeGain,
                    acustra::fittedPhysicalCalibration.steelDisplacementScaleMetres,
                    acustra::fittedPhysicalCalibration.steelFretT60Slope,
                    acustra::fittedPhysicalCalibration.highLossCutoffScale,
                    acustra::fittedPhysicalCalibration.bridgeConductanceFloor,
                    acustra::fittedPhysicalCalibration.bridgeConductanceCornerHz }
               && values(fallback.nylon)
                    == values(acustra::fittedPhysicalCalibration.nylon)
               && values(fallback.steel)
                    == values(acustra::fittedPhysicalCalibration.steel),
           "non-finite physical calibration did not use fitted defaults");

    acustra::AcustraEngine resetProbe;
    resetProbe.prepare(sampleRate, blockSize);
    resetProbe.noteOn(52, 0.8f);
    expect(resetProbe.getActiveVoiceCount() == 1,
           "physical-calibration reset probe did not allocate a voice");
    resetProbe.setPhysicalCalibration(high);
    expect(resetProbe.getActiveVoiceCount() == 0,
           "prepared physical-calibration change did not reset the engine");

    for (const auto& calibration : { low, high })
    for (const auto material : { acustra::StringMaterial::Nylon,
                                 acustra::StringMaterial::Steel })
    {
        acustra::AcustraEngine engine;
        engine.setPhysicalCalibration(calibration);
        acustra::EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        engine.noteOn(52, 1.0f);
        std::vector<float> left(blockSize);
        std::vector<float> right(blockSize);
        double maximum = 0.0;
        for (int block = 0; block < 100; ++block)
        {
            engine.process(left.data(), right.data(), blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const auto index = static_cast<std::size_t>(sample);
                expect(std::isfinite(left[index]) && std::isfinite(right[index]),
                       "bounded physical calibration produced non-finite audio");
                maximum = std::max(maximum, static_cast<double>(std::max(
                    std::abs(left[index]), std::abs(right[index]))));
            }
            expect(std::isfinite(engine.getLastBridgeVelocity())
                       && std::isfinite(engine.getLastBridgeReactionForce())
                       && std::isfinite(engine.getLastBridgeBodyForce())
                       && std::isfinite(engine.getLastBridgeTailForce()),
                   "bounded physical calibration poisoned bridge telemetry");
        }
        expect(maximum > 1.0e-7 && maximum <= 1.0,
               "bounded physical calibration was silent or escaped headroom");
    }
}

void testBodyAndBridgeCalibrationChangePhysicalDescriptors()
{
    auto low = acustra::fittedPhysicalCalibration;
    low.bodyFrequencyScale = 0.96f;
    low.bodyQScale = 0.05f;
    low.bridgeMobilityScale = 0.25f;
    auto high = acustra::fittedPhysicalCalibration;
    high.bodyFrequencyScale = 1.04f;
    high.bodyQScale = 1.8f;
    high.bridgeMobilityScale = 4.0f;
    const auto lowBody = acustra::AcustraEngineTestAccess::configuredBody(low, 2);
    const auto highBody = acustra::AcustraEngineTestAccess::configuredBody(high, 2);
    expect(highBody.frequency / lowBody.frequency > 1.08,
           "body frequency calibration did not move the modal pole");
    expect(highBody.q / lowBody.q > 3.2,
           "body Q calibration did not move modal decay");

    auto downward = acustra::fittedPhysicalCalibration;
    downward.residueTiltDbPerOctave = -6.0f;
    auto upward = acustra::fittedPhysicalCalibration;
    upward.residueTiltDbPerOctave = 6.0f;
    const double downwardSlope
        = acustra::AcustraEngineTestAccess::configuredBody(downward, 36).residue
        / acustra::AcustraEngineTestAccess::configuredBody(downward, 2).residue;
    const double upwardSlope
        = acustra::AcustraEngineTestAccess::configuredBody(upward, 36).residue
        / acustra::AcustraEngineTestAccess::configuredBody(upward, 2).residue;
    expect(upwardSlope > 100.0 * downwardSlope,
           "residue tilt did not rotate the body spectrum around 1 kHz");

    auto quietLowModes = acustra::fittedPhysicalCalibration;
    quietLowModes.lowBodyModeGain = 0.25f;
    auto strongLowModes = acustra::fittedPhysicalCalibration;
    strongLowModes.lowBodyModeGain = 32.0f;
    const double lowModeRatio
        = acustra::AcustraEngineTestAccess::configuredBody(
            strongLowModes, 0).residue
        / acustra::AcustraEngineTestAccess::configuredBody(
            quietLowModes, 0).residue;
    const double midModeRatio
        = acustra::AcustraEngineTestAccess::configuredBody(
            strongLowModes, 2).residue
        / acustra::AcustraEngineTestAccess::configuredBody(
            quietLowModes, 2).residue;
    expect(std::abs(lowModeRatio - 128.0) < 1.0e-3
               && std::abs(midModeRatio - 1.0) < 1.0e-6,
           "low-body calibration changed the wrong measured modes");

    // The plate conductance floor extrapolates past the measured band and
    // carries its own level, so the measured weights must be checked with it
    // switched off; the floor must then add conductance on top of them.
    auto lowMeasured = low;
    lowMeasured.bridgeConductanceFloor = 0.0f;
    auto highMeasured = high;
    highMeasured.bridgeConductanceFloor = 0.0f;
    const double mobilityRatio
        = acustra::AcustraEngineTestAccess::bridgeAdmittance(highMeasured)
        / acustra::AcustraEngineTestAccess::bridgeAdmittance(lowMeasured);
    expect(std::abs(mobilityRatio - 16.0) < 1.0e-3,
           "bridge calibration did not scale every positive modal weight");
    expect(acustra::AcustraEngineTestAccess::bridgeAdmittance(low)
               > acustra::AcustraEngineTestAccess::bridgeAdmittance(lowMeasured),
           "the plate conductance floor did not add junction conductance");
    expect(std::abs(acustra::AcustraEngineTestAccess::playedDelay(high)
                  - acustra::AcustraEngineTestAccess::playedDelay(low)) > 0.05,
           "bridge mobility did not reach speaking-string phase delay");
}

void testMaterialCalibrationChangesStringAndPluckDescriptors()
{
    auto lowStiffness = acustra::fittedPhysicalCalibration;
    lowStiffness.steel.stiffnessScale = 0.25f;
    auto highStiffness = acustra::fittedPhysicalCalibration;
    highStiffness.steel.stiffnessScale = 4.0f;
    const auto compliant = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, lowStiffness);
    const auto stiff = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, highStiffness);
    expect(stiff.inharmonicity > 15.9 * compliant.inharmonicity,
           "stiffness calibration did not scale string inharmonicity");

    auto shortT60 = acustra::fittedPhysicalCalibration;
    shortT60.steel.fundamentalT60Scale = 0.4f;
    auto longT60 = acustra::fittedPhysicalCalibration;
    longT60.steel.fundamentalT60Scale = 2.0f;
    const auto shortLoop = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, shortT60);
    const auto longLoop = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, longT60);
    expect(longLoop.loopGain > shortLoop.loopGain + 0.02,
           "fundamental T60 calibration did not change loop decay");

    auto lowLoss = acustra::fittedPhysicalCalibration;
    lowLoss.steel.frequencyLossScale = 0.35f;
    auto highLoss = acustra::fittedPhysicalCalibration;
    highLoss.steel.frequencyLossScale = 3.0f;
    const auto clear = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, lowLoss);
    const auto lossy = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, highLoss);
    expect(lossy.broadMix > 8.5 * clear.broadMix
               && lossy.highMix > 8.5 * clear.highMix,
           "frequency-loss calibration missed a loss branch");
    const auto preparedLoss
        = acustra::AcustraEngineTestAccess::changePreparedLoss(
            lowLoss, highLoss);
    const double dispersionChange
        = std::abs(preparedLoss.afterA1 - preparedLoss.beforeA1)
        + std::abs(preparedLoss.afterA2 - preparedLoss.beforeA2);
    expect(std::abs(preparedLoss.beforeScale - 0.35) < 1.0e-6
               && std::abs(preparedLoss.afterScale - 3.0) < 1.0e-6
               && dispersionChange > 1.0e-7,
           "prepared loss calibration reused a stale dispersion design: "
               + std::to_string(preparedLoss.beforeScale) + " -> "
               + std::to_string(preparedLoss.afterScale) + ", delta "
               + std::to_string(dispersionChange));

    auto nearBridge = acustra::fittedPhysicalCalibration;
    nearBridge.steel.pluckDistanceScale = 0.7f;
    auto towardNeck = acustra::fittedPhysicalCalibration;
    towardNeck.steel.pluckDistanceScale = 1.3f;
    const auto nearPluck = acustra::AcustraEngineTestAccess::pluck(
        nearBridge, acustra::StringMaterial::Steel, 0.8f);
    const auto farPluck = acustra::AcustraEngineTestAccess::pluck(
        towardNeck, acustra::StringMaterial::Steel, 0.8f);
    expect(farPluck.peakPosition > nearPluck.peakPosition + 0.05,
           "pluck-distance calibration did not move the initial condition");

    auto narrowAperture = acustra::fittedPhysicalCalibration;
    narrowAperture.steel.apertureScale = 0.35f;
    auto broadAperture = acustra::fittedPhysicalCalibration;
    broadAperture.steel.apertureScale = 2.5f;
    const auto narrow = acustra::AcustraEngineTestAccess::pluck(
        narrowAperture, acustra::StringMaterial::Steel, 0.8f);
    const auto broad = acustra::AcustraEngineTestAccess::pluck(
        broadAperture, acustra::StringMaterial::Steel, 0.8f);
    expect(broad.peakDisplacement < 0.92 * narrow.peakDisplacement,
           "aperture calibration did not smooth the initial condition");

    auto noTransient = acustra::fittedPhysicalCalibration;
    noTransient.steel.transientScale = 0.0f;
    auto strongTransient = acustra::fittedPhysicalCalibration;
    strongTransient.steel.transientScale = 3.0f;
    const auto quietAttack = acustra::AcustraEngineTestAccess::pluck(
        noTransient, acustra::StringMaterial::Steel, 0.8f);
    const auto strongAttack = acustra::AcustraEngineTestAccess::pluck(
        strongTransient, acustra::StringMaterial::Steel, 0.8f);
    expect(quietAttack.noiseEnvelope == 0.0 && strongAttack.noiseEnvelope > 0.0,
           "transient calibration missed its noise branch");

    auto responsive = acustra::fittedPhysicalCalibration;
    responsive.steel.velocityBrightnessDepth = 1.2f;
    const auto responsiveLow = acustra::AcustraEngineTestAccess::pluck(
        responsive, acustra::StringMaterial::Steel, 0.2f);
    const auto responsiveHigh = acustra::AcustraEngineTestAccess::pluck(
        responsive, acustra::StringMaterial::Steel, 0.8f);
    auto fixedResponse = acustra::fittedPhysicalCalibration;
    fixedResponse.steel.velocityBrightnessDepth = 0.0f;
    const auto fixedLow = acustra::AcustraEngineTestAccess::pluck(
        fixedResponse,
        acustra::StringMaterial::Steel, 0.2f);
    const auto fixedHigh = acustra::AcustraEngineTestAccess::pluck(
        fixedResponse,
        acustra::StringMaterial::Steel, 0.8f);
    expect(responsiveHigh.touch - responsiveLow.touch > 0.70,
           "velocity brightness did not reach effective pluck touch");
    expect(responsiveLow.peakDisplacement / responsiveHigh.peakDisplacement
               > 1.8 * fixedLow.peakDisplacement
                    / fixedHigh.peakDisplacement,
           "velocity response did not change the pluck-amplitude exponent");

    acustra::EngineParameters quiet;
    quiet.outputGain = 0.10f;
    quiet.bodyAmount = 0.0f;
    auto noDirect = acustra::fittedPhysicalCalibration;
    noDirect.directGain = 0.0f;
    auto strongDirect = acustra::fittedPhysicalCalibration;
    strongDirect.directGain = 0.12f;
    const auto bodyOnly = renderCalibrated(
        quiet, noDirect, 52, 0.8f, 0.18);
    const auto withDirect = renderCalibrated(
        quiet, strongDirect, 52, 0.8f, 0.18);
    expect(normalisedDifference(bodyOnly, withDirect) > 1.0e-4,
           "direct-gain calibration did not change bridge-local output");
}

void testHighLossCutoffScaleChangesOnlyUpperLoss()
{
    auto lowCutoff = acustra::fittedPhysicalCalibration;
    lowCutoff.highLossCutoffScale = 0.5f;
    auto highCutoff = acustra::fittedPhysicalCalibration;
    highCutoff.highLossCutoffScale = 4.0f;

    const auto roundTripMagnitude = [] (
        const acustra::AcustraEngineTestAccess::StringLoopSnapshot& loop,
        double omega)
    {
        const auto mixedPoleMagnitude = [omega] (double coefficient, double mix)
        {
            const double denominatorReal
                = 1.0 - coefficient * std::cos(omega);
            const double denominatorImaginary
                = coefficient * std::sin(omega);
            const double norm = denominatorReal * denominatorReal
                              + denominatorImaginary * denominatorImaginary;
            const double lowReal
                = (1.0 - coefficient) * denominatorReal / norm;
            const double lowImaginary
                = -(1.0 - coefficient) * denominatorImaginary / norm;
            return std::hypot((1.0 - mix) + mix * lowReal,
                              mix * lowImaginary);
        };
        return loop.loopGain
            * mixedPoleMagnitude(loop.broadCoefficient, loop.broadMix)
            * mixedPoleMagnitude(loop.highCoefficient, loop.highMix);
    };

    constexpr int midiNote = 40;
    const double fundamental = 440.0
        * std::exp2((static_cast<double>(midiNote) - 69.0) / 12.0);
    const double fundamentalOmega
        = 2.0 * std::numbers::pi * fundamental / sampleRate;
    const double upperOmega = 2.0 * std::numbers::pi * 8000.0 / sampleRate;
    for (const auto material : { acustra::StringMaterial::Nylon,
                                 acustra::StringMaterial::Steel })
    {
        const auto low = acustra::AcustraEngineTestAccess::configuredLoop(
            material, midiNote, sampleRate, lowCutoff);
        const auto high = acustra::AcustraEngineTestAccess::configuredLoop(
            material, midiNote, sampleRate, highCutoff);
        const std::string name = material == acustra::StringMaterial::Steel
            ? "steel" : "nylon";

        expect(roundTripMagnitude(high, upperOmega)
                   > roundTripMagnitude(low, upperOmega) + 0.01,
               name + " high-loss cutoff did not reduce upper-string loss");
        const double fundamentalChangeDb = 20.0 * std::log10(
            roundTripMagnitude(high, fundamentalOmega)
            / roundTripMagnitude(low, fundamentalOmega));
        expect(std::abs(fundamentalChangeDb) < 1.0e-3,
               name + " high-loss cutoff moved requested fundamental decay");
        const double pitchChangeCents = 1200.0 * std::log2(
            loopResonance(high, 1, fundamentalOmega)
            / loopResonance(low, 1, fundamentalOmega));
        expect(std::abs(pitchChangeCents) < 0.01,
               name + " high-loss cutoff moved requested fundamental pitch");
    }
}

void testPlateConductanceFloorDampsOnlyTheUpperBand()
{
    // The plate conductance floor restores the flat conductance a real
    // soundboard keeps above its modal-overlap frequency, which the finite
    // measured modal fit loses. It must shorten the upper-band tail without
    // touching the low partials the measured modes already dominate, and it
    // must be exactly inert when its plateau is zero.
    auto off = acustra::fittedPhysicalCalibration;
    off.bridgeConductanceFloor = 0.0f;
    off.bridgeConductanceCornerHz = 2400.0f;
    auto offOtherCorner = off;
    offOtherCorner.bridgeConductanceCornerHz = 200.0f;
    auto on = off;
    on.bridgeConductanceFloor = 0.004f;

    acustra::EngineParameters parameters;
    parameters.stringMaterial = acustra::StringMaterial::Steel;
    const auto quiet = renderCalibrated(parameters, off, 52, 0.85f, 3.0);
    const auto other = renderCalibrated(parameters, offOtherCorner, 52, 0.85f, 3.0);
    const auto damped = renderCalibrated(parameters, on, 52, 0.85f, 3.0);

    expect(quiet.left == other.left && quiet.right == other.right,
           "the conductance corner was not inert at a zero plateau");

    const double upperOff = tailBandRms(quiet, sampleRate, 1.2, 2.6, 2000.0, 4000.0);
    const double upperOn = tailBandRms(damped, sampleRate, 1.2, 2.6, 2000.0, 4000.0);
    const double lowerOff = tailBandRms(quiet, sampleRate, 1.2, 2.6, 80.0, 200.0);
    const double lowerOn = tailBandRms(damped, sampleRate, 1.2, 2.6, 80.0, 200.0);
    expect(upperOff > 0.0 && upperOn > 0.0 && lowerOff > 0.0 && lowerOn > 0.0,
           "a plate-conductance render produced no measurable band energy");

    const double upperChangeDb = 20.0 * std::log10(upperOn / upperOff);
    const double lowerChangeDb = 20.0 * std::log10(lowerOn / lowerOff);
    expect(upperChangeDb < -2.0,
           "the plate conductance floor did not shorten the upper-band tail");
    expect(std::abs(lowerChangeDb) < 1.5,
           "the plate conductance floor changed the low-partial tail");
    for (const auto& audio : { quiet, damped })
        for (std::size_t index = 0; index < audio.left.size(); ++index)
            expect(std::isfinite(audio.left[index])
                       && std::isfinite(audio.right[index]),
                   "a plate-conductance render was not finite");
}

void testStolenStringKeepsRingingUnderHandDamping()
{
    // A chord change takes every string while it is still vibrating. The old
    // vibration must be damped by the hand, not deleted.
    const auto snapshot
        = acustra::AcustraEngineTestAccess::stealStringTail(sampleRate);
    expect(snapshot.heldBeforeSteal > 1.0e-4,
           "the first chord had not stored measurable string energy");
    expect(snapshot.tailActive,
           "taking a sounding string for another note started no tail");
    expect(snapshot.keptInTail > 0.5 * snapshot.heldBeforeSteal,
           "the tail kept less than half of the string's stored energy");
    expect(snapshot.keptInTail <= snapshot.heldBeforeSteal * 1.000001,
           "the tail held more energy than the string it came from");
    // 0.16 s of hand damping is about nine 60 dB decays in half a second, so
    // the tail must be far down but the mechanism must not be instantaneous.
    expect(snapshot.tailEnergyAfterDecay < 0.01 * snapshot.keptInTail,
           "the stolen tail did not decay under the hand damping");
    // Replucking the same note is the hand landing on the string too: what
    // it held goes on in the tail under the hand while the pluck is released
    // from rest.
    expect(snapshot.tailActiveAfterRepluck,
           "replucking a sounding note did not carry it into the tail");

    // Repeated chord changes restart a tail on a string whose previous tail is
    // still sounding, which discards the older one. Run that hard: forty
    // changes, some inside one tail's 160 ms, at the sample-rate extremes.
    for (const double rate : { 44100.0, 384000.0 })
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const std::array<std::array<int, 6>, 3> chords {{
            {{ 40, 47, 52, 56, 59, 64 }},
            {{ 43, 50, 55, 58, 62, 67 }},
            {{ 45, 52, 57, 60, 64, 69 }} }};
        double maximum = 0.0;
        int change = 0;
        const int step = static_cast<int>(0.10 * rate);
        for (int i = 0; i < static_cast<int>(4.0 * rate); i += blockSize)
        {
            if (i / step > change - 1 && change < 40)
            {
                for (const int note : chords[static_cast<std::size_t>(
                         change % chords.size())])
                    engine.noteOn(note, 0.95f);
                ++change;
            }
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
            {
                const auto index = static_cast<std::size_t>(k);
                expect(std::isfinite(left[index]) && std::isfinite(right[index]),
                       "repeated chord changes produced non-finite audio");
                maximum = std::max(maximum, static_cast<double>(std::max(
                    std::abs(left[index]), std::abs(right[index]))));
            }
        }
        expect(maximum > 1.0e-6 && maximum <= 1.0,
               "repeated chord changes were silent or escaped headroom");
        engine.allSoundOff();
        for (int i = 0; i < static_cast<int>(0.5 * rate); i += blockSize)
        {
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
                expect(std::abs(left[static_cast<std::size_t>(k)]) < 1.0e-4f
                       && std::abs(right[static_cast<std::size_t>(k)]) < 1.0e-4f,
                       "a tail survived All Sound Off");
        }
    }

    // Two chords in sequence must stay finite and bounded at every rate.
    for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const std::array<int, 6> first { 40, 47, 52, 56, 59, 64 };
        const std::array<int, 6> second { 43, 50, 55, 58, 62, 67 };
        for (const int note : first) engine.noteOn(note, 0.9f);
        double maximum = 0.0;
        for (int i = 0; i < static_cast<int>(2.0 * rate); i += blockSize)
        {
            if (i >= static_cast<int>(0.4 * rate)
                && i < static_cast<int>(0.4 * rate) + blockSize)
                for (const int note : second) engine.noteOn(note, 0.9f);
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
            {
                const auto index = static_cast<std::size_t>(k);
                expect(std::isfinite(left[index]) && std::isfinite(right[index]),
                       "a chord change produced non-finite audio");
                maximum = std::max(maximum, static_cast<double>(std::max(
                    std::abs(left[index]), std::abs(right[index]))));
            }
        }
        expect(maximum > 1.0e-6 && maximum <= 1.0,
               "a chord change was silent or escaped headroom");
    }
}

void testBridgeHandPressureShortensAndDarkens()
{
    // Zero pressure must be an exact no-op, and rising pressure must shorten
    // the note and darken it, monotonically, without escaping headroom.
    const auto render = [] (float pressure, int midiNote, bool applyPressure = true)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        if (applyPressure)
            engine.setPalmMutePressure(pressure);
        const int settle = static_cast<int>(0.10 * sampleRate);
        Audio scratch { std::vector<float>(static_cast<std::size_t>(blockSize)),
                        std::vector<float>(static_cast<std::size_t>(blockSize)) };
        for (int i = 0; i < settle; i += blockSize)
            engine.process(scratch.left.data(), scratch.right.data(), blockSize);
        engine.noteOn(midiNote, 0.85f);
        const int samples = static_cast<int>(2.0 * sampleRate);
        Audio out { std::vector<float>(static_cast<std::size_t>(samples)),
                    std::vector<float>(static_cast<std::size_t>(samples)) };
        for (int offset = 0; offset < samples; offset += blockSize)
        {
            const int count = std::min(blockSize, samples - offset);
            engine.process(out.left.data() + offset, out.right.data() + offset,
                           count);
        }
        return out;
    };
    const auto windowRms = [] (const Audio& a, double from, double to)
    {
        const auto i0 = static_cast<std::size_t>(from * sampleRate);
        const auto i1 = std::min(static_cast<std::size_t>(to * sampleRate),
                                 a.left.size());
        double energy = 0.0;
        for (std::size_t i = i0; i < i1; ++i)
        {
            const double mono = 0.5 * (a.left[i] + a.right[i]);
            energy += mono * mono;
        }
        return std::sqrt(energy / std::max<std::size_t>(1, i1 - i0));
    };

    const auto open = render(0.0f, 52);
    const auto untouched = render(0.0f, 52, false);
    expect(open.left == untouched.left && open.right == untouched.right,
           "zero bridge-hand pressure was not an exact no-op");

    double previousTail = windowRms(open, 1.0, 2.0);
    double previousBright = tailBandRms(open, sampleRate, 0.0, 0.2,
                                        2000.0, 6000.0);
    const double openAttack = windowRms(open, 0.0, 0.05);
    for (const float pressure : { 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const auto muted = render(pressure, 52);
        for (std::size_t i = 0; i < muted.left.size(); ++i)
            expect(std::isfinite(muted.left[i]) && std::isfinite(muted.right[i])
                       && std::abs(muted.left[i]) <= 1.0f
                       && std::abs(muted.right[i]) <= 1.0f,
                   "a muted render left headroom or went non-finite");
        const double tail = windowRms(muted, 1.0, 2.0);
        // Once the tail is at the arithmetic floor (5e-11 at 0.75), more
        // pressure has nothing left to shorten.
        expect(tail < previousTail || tail < 1.0e-9,
               "more bridge-hand pressure did not shorten the note further");
        const double bright = tailBandRms(muted, sampleRate, 0.0, 0.2,
                                          2000.0, 6000.0);
        expect(bright < previousBright,
               "more bridge-hand pressure did not darken the note further");
        expect(windowRms(muted, 0.0, 0.05) < openAttack * 1.001,
               "bridge-hand pressure made the attack louder than open");
        previousTail = tail;
        previousBright = bright;
    }
}

void testNaturalHarmonicsReachAboveTheFretboard()
{
    // Above the twentieth fret the guitar still reaches, through the natural
    // harmonics of its open strings, and the requested pitch alone decides
    // whether one exists. Below the lowest open string it does not reach.
    const auto play = [] (int midiNote)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        engine.noteOn(midiNote, 0.9f);
        const int samples = static_cast<int>(2.0 * sampleRate);
        Audio out { std::vector<float>(static_cast<std::size_t>(samples)),
                    std::vector<float>(static_cast<std::size_t>(samples)) };
        for (int offset = 0; offset < samples; offset += blockSize)
            engine.process(out.left.data() + offset, out.right.data() + offset,
                           std::min(blockSize, samples - offset));
        return out;
    };
    const auto rms = [] (const Audio& a)
    {
        double energy = 0.0;
        for (std::size_t i = 0; i < a.left.size(); ++i)
        {
            const double mono = 0.5 * (a.left[i] + a.right[i]);
            energy += mono * mono;
        }
        return std::sqrt(energy / std::max<std::size_t>(1, a.left.size()));
    };

    const double stopped = rms(play(84));
    expect(stopped > 1.0e-4, "the highest fretted note was silent");
    // E6 is the fourth harmonic of the open high E, and B6 the sixth of the
    // open B. Both are exact multiples, so both must sound.
    for (const int midiNote : { 88, 90 })
    {
        const auto harmonic = play(midiNote);
        const double level = rms(harmonic);
        expect(level > 1.0e-5, "a natural harmonic above the fretboard was silent");
        // A touched node keeps only the modes the pluck already put there, so
        // a harmonic is quieter than a stopped note rather than louder.
        expect(level < stopped, "a natural harmonic was louder than a stopped note");
        for (std::size_t i = 0; i < harmonic.left.size(); ++i)
            expect(std::isfinite(harmonic.left[i])
                       && std::isfinite(harmonic.right[i])
                       && std::abs(harmonic.left[i]) <= 1.0f,
                   "a natural harmonic left headroom or went non-finite");
    }
    // No open string has a harmonic within 25 cents of these, and nothing on a
    // guitar lies below the lowest open string, so all stay silent.
    for (const int midiNote : { 85, 89, 93, 39, 20 })
        expect(rms(play(midiNote)) == 0.0,
               "a pitch the guitar cannot produce was sounded");

    // The lowest usable node wins, and it lands on the pitch it should. The
    // sounding partial is a few cents sharp because it is a real partial of a
    // stiff string, which is what a guitar does too.
    const auto e6 = play(88);
    double best = 0.0;
    double bestFrequency = 0.0;
    for (double frequency = 1250.0; frequency < 1390.0; frequency += 0.25)
    {
        double real = 0.0;
        double imaginary = 0.0;
        const int count = static_cast<int>(1.0 * sampleRate);
        for (int i = 0; i < count; ++i)
        {
            const double t = i / sampleRate;
            const double mono = 0.5 * (e6.left[static_cast<std::size_t>(i)]
                                     + e6.right[static_cast<std::size_t>(i)]);
            real += mono * std::cos(2.0 * std::numbers::pi * frequency * t);
            imaginary += mono * std::sin(2.0 * std::numbers::pi * frequency * t);
        }
        const double magnitude = std::hypot(real, imaginary);
        if (magnitude > best) { best = magnitude; bestFrequency = frequency; }
    }
    const double wanted = 440.0 * std::exp2((88.0 - 69.0) / 12.0);
    const double cents = 1200.0 * std::log2(bestFrequency / wanted);
    expect(std::abs(cents) < 20.0,
           "the fourth harmonic of the open high E was not near E6");
}

void testHeldStringsDoNotLengthenANoteDecay()
{
    // Every string is anchored behind the saddle at all times, so the spring
    // the junction sees is a constant of the instrument. Summing it over the
    // played strings alone made the port stiffen with each voice held, and a
    // note inside a chord then rang 2.15 times longer than the same note
    // alone - including beside a note too quiet to hear, which is the proof
    // that it was the aggregation and not energy arriving from the neighbour.
    constexpr int subject = 43;
    const auto decayRate = [] (const Audio& audio)
    {
        const double early = tailBandRms(audio, sampleRate, 1.0, 2.0, 88.0, 108.0);
        const double late = tailBandRms(audio, sampleRate, 2.5, 3.5, 88.0, 108.0);
        expect(early > 1.0e-7 && late > 1.0e-9,
               "a held-string decay render had no measurable fundamental");
        return 20.0 * std::log10(early / late) / 1.5;
    };
    const auto renderWith = [] (const std::vector<int>& companions,
                                float companionVelocity)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        // The idle-string path is one-way radiation, not the junction; mute
        // it so this measures the bridge port and nothing else.
        engine.setSympatheticStringsEnabled(false);
        for (const int note : companions)
            engine.noteOn(note, companionVelocity);
        engine.noteOn(subject, 0.85f);
        const int samples = static_cast<int>(4.0 * sampleRate);
        Audio result { std::vector<float>(static_cast<std::size_t>(samples)),
                       std::vector<float>(static_cast<std::size_t>(samples)) };
        for (int offset = 0; offset < samples; offset += blockSize)
            engine.process(result.left.data() + offset,
                           result.right.data() + offset,
                           std::min(blockSize, samples - offset));
        return result;
    };

    const double alone = decayRate(renderWith({}, 0.0f));
    expect(alone > 1.0, "the subject note did not decay on its own");
    struct Case { const char* name; std::vector<int> companions; float velocity; };
    // The companions all sound above the 88-108 Hz band this measures, so
    // only the subject's own fundamental is in it.
    const std::vector<Case> cases {
        { "one inaudible neighbour", { 50 }, 0.001f },
        { "one loud neighbour", { 50 }, 0.85f },
        { "a full six-string chord", { 47, 50, 55, 59, 64 }, 0.85f },
    };
    for (const auto& item : cases)
    {
        const double rate = decayRate(renderWith(item.companions, item.velocity));
        const double ratio = alone / rate;
        expect(ratio > 0.80 && ratio < 1.25,
               std::string("holding ") + item.name
                   + " changed the note's own decay");
    }
}


void testANoteOverASoundingInstrumentDoesNotClick()
{
    // Starting a note while the instrument is still ringing used to put the
    // whole released shape into the junction's wave variables in one sample.
    // The finite differences that turn those displacement waves into bridge
    // velocity read that as motion, so every note-on after the first arrived
    // as an impulse about ten times the note it belonged to. A pluck is a
    // release from rest: the shape was standing on the string before the
    // finger let go, and nothing about it is a bridge velocity.
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        // Measure below the safety limiter, which would otherwise flatten a
        // transient into something that looks like the note it buried.
        parameters.outputGain = 0.04f;
        const int block = 64;
        const auto peakAfter = [&] (const std::vector<int>& held,
                                    bool release, int note)
        {
            acustra::AcustraEngine engine;
            engine.setParameters(parameters);
            engine.prepare(rate, block);
            std::vector<float> left(static_cast<std::size_t>(block));
            std::vector<float> right(static_cast<std::size_t>(block));
            for (const int other : held)
                engine.noteOn(other, 0.85f);
            for (int i = 0; i < static_cast<int>(1.5 * rate); i += block)
                engine.process(left.data(), right.data(), block);
            if (release)
            {
                for (const int other : held)
                    engine.noteOff(other);
                for (int i = 0; i < static_cast<int>(0.05 * rate); i += block)
                    engine.process(left.data(), right.data(), block);
            }
            engine.noteOn(note, 0.85f);
            double maximum = 0.0;
            for (int i = 0; i < static_cast<int>(0.06 * rate); i += block)
            {
                engine.process(left.data(), right.data(), block);
                for (int k = 0; k < block; ++k)
                {
                    const auto index = static_cast<std::size_t>(k);
                    expect(std::isfinite(left[index])
                               && std::isfinite(right[index]),
                           "a note over a sounding instrument was not finite");
                    maximum = std::max(maximum, static_cast<double>(std::max(
                        std::abs(left[index]), std::abs(right[index]))));
                }
            }
            return maximum;
        };

        const double fresh = peakAfter({}, false, 43);
        expect(fresh > 1.0e-5, "the reference note was silent");
        struct Case { const char* name; std::vector<int> held; bool release; };
        const std::vector<Case> cases {
            // A free string under a held neighbour.
            { "beside a held neighbour", { 45 }, false },
            // The same string, taken from the note already on it.
            { "taking a sounding string", { 40 }, false },
            // A whole chord, every string occupied.
            { "over a six-string chord", { 40, 47, 52, 56, 59, 64 }, false },
            // And after the hand has left, while the strings ring on.
            { "over a released chord", { 40, 47, 52, 56, 59, 64 }, true },
        };
        for (const auto& item : cases)
        {
            const double peak = peakAfter(item.held, item.release, 43);
            expect(peak < 2.0 * fresh,
                   std::string("a note started ") + item.name
                       + " peaked far above the same note on a silent engine");
        }
    }
}


void testSwitchingStringsOrTuningUnderAChordDoesNotClick()
{
    // Exchanging the string set or the tuning changes every string's
    // impedance at once, so the junction's wave variables step with the port.
    // The strings were swapped; the bridge did not move.
    acustra::EngineParameters steel;
    steel.stringMaterial = acustra::StringMaterial::Steel;
    steel.outputGain = 0.04f;
    const int block = 64;
    const auto stepPeak = [&] (acustra::EngineParameters after)
    {
        acustra::AcustraEngine engine;
        engine.setParameters(steel);
        engine.prepare(sampleRate, block);
        std::vector<float> left(static_cast<std::size_t>(block));
        std::vector<float> right(static_cast<std::size_t>(block));
        const auto sweep = [&] (double seconds)
        {
            double maximum = 0.0;
            for (int i = 0; i < static_cast<int>(seconds * sampleRate);
                 i += block)
            {
                engine.process(left.data(), right.data(), block);
                for (int k = 0; k < block; ++k)
                {
                    const auto index = static_cast<std::size_t>(k);
                    expect(std::isfinite(left[index])
                               && std::isfinite(right[index]),
                           "a construction switch produced non-finite audio");
                    maximum = std::max(maximum, static_cast<double>(std::max(
                        std::abs(left[index]), std::abs(right[index]))));
                }
            }
            return maximum;
        };
        for (const int note : { 40, 47, 52, 56, 59, 64 })
            engine.noteOn(note, 0.85f);
        sweep(1.2);
        const double before = sweep(0.05);
        after.outputGain = 0.04f;
        engine.setParameters(after);
        return std::pair { before, sweep(0.05) };
    };

    acustra::EngineParameters nylon;
    nylon.stringMaterial = acustra::StringMaterial::Nylon;
    acustra::EngineParameters dadgad;
    dadgad.stringMaterial = acustra::StringMaterial::Steel;
    dadgad.tuning = acustra::Tuning::Dadgad;
    for (const auto& item : { std::pair { "the string set", nylon },
                              std::pair { "the tuning", dadgad } })
    {
        const auto [before, after] = stepPeak(item.second);
        expect(before > 1.0e-6, "the chord under the switch was silent");
        expect(after < 2.0 * before,
               std::string("switching ") + item.first
                   + " under a ringing chord produced a transient");
    }
}


void testLegatoHammersOnAndPullsOff()
{
    // A hammer-on stops the string with the fretting finger; it does not
    // release it from rest. So the loop keeps what it holds and only its
    // length changes, with the finger's own strike added to it. The
    // footswitch is what separates a hammer-on from a strum, which the model
    // alone cannot do: one note arriving over a held string is a hammer-on,
    // six arriving together are a chord.
    const int block = 64;
    const auto phrase = [&] (double rate, bool legato, bool touchTheSwitch)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        parameters.outputGain = 0.04f;
        engine.setParameters(parameters);
        engine.prepare(rate, block);
        if (touchTheSwitch)
        {
            engine.setLegato(true);
            engine.setLegato(false);
        }
        engine.setLegato(legato);
        engine.setSympatheticStringsEnabled(false);
        std::vector<float> left, right;
        const auto sweep = [&] (double seconds)
        {
            const int samples = static_cast<int>(seconds * rate);
            std::vector<float> l(static_cast<std::size_t>(block));
            std::vector<float> r(static_cast<std::size_t>(block));
            for (int i = 0; i < samples; i += block)
            {
                engine.process(l.data(), r.data(), block);
                left.insert(left.end(), l.begin(), l.end());
                right.insert(right.end(), r.begin(), r.end());
            }
        };
        engine.noteOn(52, 0.85f);
        sweep(1.0);
        const std::size_t hammer = left.size();
        engine.noteOn(55, 0.85f);
        sweep(1.0);
        const std::size_t pull = left.size();
        engine.noteOff(55);
        sweep(1.0);
        return std::tuple { Audio { left, right }, hammer, pull };
    };

    // The switch up is exactly what it was, including after being pressed and
    // released again.
    const auto [plain, plainHammer, plainPull] = phrase(sampleRate, false, false);
    const auto [touched, touchedHammer, touchedPull]
        = phrase(sampleRate, false, true);
    expect(plain.left == touched.left && plain.right == touched.right,
           "the legato footswitch was not an exact no-op when up");

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto [audio, hammer, pull] = phrase(rate, true, false);
        for (std::size_t index = 0; index < audio.left.size(); ++index)
            expect(std::isfinite(audio.left[index])
                       && std::isfinite(audio.right[index])
                       && std::abs(audio.left[index]) <= 1.0f
                       && std::abs(audio.right[index]) <= 1.0f,
                   "a legato phrase left headroom or went non-finite");

        const auto band = [&] (double frequency, std::size_t begin,
                               std::size_t end)
        {
            double real = 0.0;
            double imaginary = 0.0;
            const double count = static_cast<double>(end - begin);
            for (std::size_t index = begin; index < end; ++index)
            {
                const double window = 0.5 - 0.5 * std::cos(
                    2.0 * std::numbers::pi
                    * static_cast<double>(index - begin) / count);
                const double mono = window * 0.5
                    * (audio.left[index] + audio.right[index]);
                const double angle = 2.0 * std::numbers::pi * frequency
                    * static_cast<double>(index) / rate;
                real += mono * std::cos(angle);
                imaginary += mono * std::sin(angle);
            }
            return 2.0 * std::hypot(real, imaginary) / count;
        };
        const double lower = 440.0 * std::exp2((52.0 - 69.0) / 12.0);
        const double upper = 440.0 * std::exp2((55.0 - 69.0) / 12.0);
        const auto quarter = static_cast<std::size_t>(0.25 * rate);
        const auto half = static_cast<std::size_t>(0.5 * rate);

        // Hammered on, the string sounds the new note and not the old one.
        expect(band(upper, hammer + quarter, hammer + half)
                   > 4.0 * band(lower, hammer + quarter, hammer + half),
               "a hammer-on did not move the string to the new note");
        // Released, it falls back to the note the hand is still holding.
        expect(band(lower, pull + quarter, pull + half)
                   > 4.0 * band(upper, pull + quarter, pull + half),
               "a pull-off did not return the string to the held note");

        // The finger's strike is on the string now: the arrival is louder
        // than what the string had, but it is not a step - nothing on the
        // first sample is above the note under it.
        const auto peakOver = [&] (std::size_t begin, std::size_t end)
        {
            double maximum = 0.0;
            for (std::size_t index = begin;
                 index < std::min(end, audio.left.size()); ++index)
                maximum = std::max(maximum, static_cast<double>(std::max(
                    std::abs(audio.left[index]),
                    std::abs(audio.right[index]))));
            return maximum;
        };
        const double sounding = peakOver(hammer - half, hammer);
        const double firstSample = peakOver(hammer, hammer + 1);
        expect(sounding > 1.0e-6, "the note under the hammer-on was silent");
        // The written dent's first sample reaches the bridge through the
        // junction; the stiffer anchor returns 1.29x the sounding peak where
        // the 17 mm spring returned 1.1x. The defect this guards against was
        // the whole shape arriving at once, ten times the note.
        expect(firstSample <= 1.5 * sounding,
               "a hammer-on stepped the wave on its first sample");
    }
}


void testLongitudinalModesGrowWithVelocity()
{
    // Transverse motion stretches the string, and the tension it adds is a
    // longitudinal wave at the string's own axial resonances. Its drive is a
    // squared slope, so what it puts into the band is the products of the
    // transverse partials and it must grow faster than the note that made it.
    const auto bandEnergy = [] (const Audio& audio, double low, double high)
    {
        return tailBandRms(audio, sampleRate, 0.0, 0.12, low, high);
    };
    const auto& silent = acustra::fittedPhysicalCalibration;
    expect(silent.longitudinalGain == 0.0f,
           "the shipping build reintroduced the drip-like axial onset");
    auto sounding = silent;
    sounding.longitudinalGain = 0.025f;
    const auto axial = acustra::AcustraEngineTestAccess::
        longitudinalFrequencies(40);
    expect(std::abs(axial[1] / axial[0] - 3.0) < 1.0e-4,
           "the next odd longitudinal mode is not three times the first");

    acustra::EngineParameters steel;
    steel.stringMaterial = acustra::StringMaterial::Steel;
    double quietGrowth = 0.0;
    double loudGrowth = 0.0;
    for (const int midiNote : { 40, 52 })
    {
        double growth[2] = { 0.0, 0.0 };
        int index = 0;
        for (const float velocity : { 0.25f, 0.95f })
        {
            const auto off = renderCalibrated(steel, silent, midiNote,
                                              velocity, 0.5);
            const auto on = renderCalibrated(steel, sounding, midiNote,
                                             velocity, 0.5);
            const double before = bandEnergy(off, 1500.0, 4000.0);
            const double after = bandEnergy(on, 1500.0, 4000.0);
            expect(before > 0.0 && after > before,
                   "the longitudinal path added no axial-band energy");
            growth[index++] = 20.0 * std::log10(after / before);
            for (std::size_t sample = 0; sample < on.left.size(); ++sample)
                expect(std::isfinite(on.left[sample])
                           && std::isfinite(on.right[sample]),
                       "a longitudinal render was not finite");
        }
        expect(growth[1] > growth[0] + 3.0,
               "the longitudinal band did not grow faster than the note");
        quietGrowth = std::max(quietGrowth, growth[0]);
        loudGrowth = std::max(loudGrowth, growth[1]);
    }
    std::cout << "Acustra longitudinal band growth: quiet=" << quietGrowth
              << " dB, loud=" << loudGrowth << " dB\n";

    // Zero is an exact no-op, and the idle-string path stays separate from it.
    acustra::AcustraEngine engine;
    engine.setPhysicalCalibration(silent);
    engine.prepare(sampleRate, blockSize);
    acustra::AcustraEngine playing;
    playing.setPhysicalCalibration(acustra::fittedPhysicalCalibration);
    playing.prepare(sampleRate, blockSize);
    playing.setSympatheticStringsEnabled(false);
    engine.noteOn(52, 0.9f);
    playing.noteOn(52, 0.9f);
    std::vector<float> left(static_cast<std::size_t>(blockSize));
    std::vector<float> right(static_cast<std::size_t>(blockSize));
    double silentForce = 0.0;
    double leakedSympathy = 0.0;
    for (int block = 0; block < 96; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        playing.process(left.data(), right.data(), blockSize);
        silentForce = std::max(silentForce, static_cast<double>(
            std::abs(engine.getLastLongitudinalForce())));
        leakedSympathy = std::max(leakedSympathy, static_cast<double>(
            std::abs(playing.getLastSympatheticRadiationForce())));
    }
    expect(silentForce == 0.0,
           "a zero longitudinal gain still produced a force");
    expect(leakedSympathy == 0.0,
           "the longitudinal force leaked into the idle-string path");
}

void testTodaysMechanismsSurviveEachOther()
{
    // The plate conductance floor, the stolen-string tail, bridge-hand
    // pressure and natural harmonics all landed together. Drive them against
    // one another: muted chord changes that steal ringing strings, harmonics
    // taken and retaken, a material and tuning switch underneath, pitch bend
    // across it, and a panic at the end.
    for (const double rate : { 44100.0, 48000.0, 192000.0 })
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const std::array<std::array<int, 6>, 2> chords {{
            {{ 40, 47, 52, 56, 59, 64 }}, {{ 43, 50, 55, 58, 62, 67 }} }};
        const std::array<int, 3> harmonics { 88, 90, 95 };
        double maximum = 0.0;
        int step = 0;
        const int stride = static_cast<int>(0.08 * rate);
        for (int i = 0; i < static_cast<int>(6.0 * rate); i += blockSize)
        {
            if (i / stride > step - 1 && step < 60)
            {
                switch (step % 6)
                {
                    case 0:
                        for (const int note : chords[0]) engine.noteOn(note, 0.9f);
                        break;
                    case 1:
                        engine.setPalmMutePressure(0.85f);
                        engine.noteOn(harmonics[static_cast<std::size_t>(
                            step / 6 % harmonics.size())], 0.7f);
                        break;
                    case 2:
                        for (const int note : chords[1]) engine.noteOn(note, 0.9f);
                        break;
                    case 3:
                        engine.setPitchBend(2.0f);
                        engine.setPalmMutePressure(0.0f);
                        // Legato goes through the same grinder: hammered on
                        // over held strings, chained, released out of order,
                        // and switched off mid-phrase so the fretting hand's
                        // stack is dropped while the strings are still
                        // sounding.
                        engine.setLegato(true);
                        engine.noteOn(45, 0.8f);
                        engine.noteOn(48, 0.8f);
                        engine.noteOn(52, 0.8f);
                        engine.noteOff(48);
                        engine.noteOff(52);
                        if ((step / 6) % 2 == 0)
                            engine.setLegato(false);
                        break;
                    case 4:
                        parameters.stringMaterial
                            = (step / 6) % 2 == 0 ? acustra::StringMaterial::Nylon
                                                  : acustra::StringMaterial::Steel;
                        parameters.tuning = (step / 6) % 2 == 0
                            ? acustra::Tuning::Dadgad : acustra::Tuning::Standard;
                        engine.setParameters(parameters);
                        break;
                    default:
                        engine.setPitchBend(0.0f);
                        for (const int note : chords[0]) engine.noteOff(note);
                        break;
                }
                ++step;
            }
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
            {
                const auto index = static_cast<std::size_t>(k);
                expect(std::isfinite(left[index]) && std::isfinite(right[index]),
                       "the combined mechanisms produced non-finite audio");
                maximum = std::max(maximum, static_cast<double>(std::max(
                    std::abs(left[index]), std::abs(right[index]))));
            }
        }
        expect(maximum > 1.0e-6 && maximum <= 1.0,
               "the combined mechanisms were silent or escaped headroom");

        engine.allSoundOff();
        for (int i = 0; i < static_cast<int>(1.0 * rate); i += blockSize)
        {
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
                expect(std::abs(left[static_cast<std::size_t>(k)]) < 1.0e-4f
                       && std::abs(right[static_cast<std::size_t>(k)]) < 1.0e-4f,
                       "All Sound Off left the combined mechanisms sounding");
        }
        expect(engine.getActiveVoiceCount() == 0,
               "All Sound Off left a voice held after the combined run");
    }
}

void testNoteAfterSilenceDoesNotClick()
{
    // While no string is sounding the bridge junction has no drive, but it must
    // keep ringing down rather than freeze: paused modes store their energy
    // until the next note reactivates the port, and it arrives as a click.
    const auto peakOfNoteAfter = [] (double quietSeconds, bool playChordFirst)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const auto run = [&] (double seconds)
        {
            for (int i = 0; i < static_cast<int>(seconds * sampleRate);
                 i += blockSize)
                engine.process(left.data(), right.data(), blockSize);
        };
        if (playChordFirst)
        {
            for (const int note : { 40, 47, 52, 56, 59, 64 })
                engine.noteOn(note, 0.72f);
            run(2.0);
            for (const int note : { 40, 47, 52, 56, 59, 64 })
                engine.noteOff(note);
        }
        run(quietSeconds);
        engine.noteOn(43, 0.62f);
        double peak = 0.0;
        for (int i = 0; i < static_cast<int>(0.3 * sampleRate); i += blockSize)
        {
            engine.process(left.data(), right.data(), blockSize);
            for (int k = 0; k < blockSize; ++k)
                peak = std::max(peak, static_cast<double>(std::max(
                    std::abs(left[static_cast<std::size_t>(k)]),
                    std::abs(right[static_cast<std::size_t>(k)]))));
        }
        return peak;
    };

    // Nothing may erupt out of a silent decay either. The junction's modes
    // outlast the strings, so the moment the last string goes quiet used to
    // switch the port out from under them.
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(sampleRate, blockSize);
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const auto run = [&] (double seconds, double* peak)
        {
            for (int i = 0; i < static_cast<int>(seconds * sampleRate);
                 i += blockSize)
            {
                engine.process(left.data(), right.data(), blockSize);
                if (peak == nullptr)
                    continue;
                for (int k = 0; k < blockSize; ++k)
                    *peak = std::max(*peak, static_cast<double>(std::max(
                        std::abs(left[static_cast<std::size_t>(k)]),
                        std::abs(right[static_cast<std::size_t>(k)]))));
            }
        };
        for (const int note : { 40, 47, 52, 56, 59, 64 })
            engine.noteOn(note, 0.62f);
        run(1.3, nullptr);
        for (const int note : { 43, 50, 55, 58, 62, 67 })
            engine.noteOn(note, 0.62f);
        run(1.6, nullptr);
        for (const int note : { 43, 50, 55, 58, 62, 67 })
            engine.noteOff(note);
        run(0.4, nullptr);
        double quietPeak = 0.0;
        run(3.0, &quietPeak);
        expect(quietPeak < 0.05,
               "a transient erupted from a decaying chord with no events");
    }

    const double fresh = peakOfNoteAfter(0.0, false);
    expect(fresh > 1.0e-4, "the reference note was silent");
    for (const double quiet : { 2.0, 4.0, 8.0 })
    {
        const double afterSilence = peakOfNoteAfter(quiet, true);
        expect(afterSilence < 2.0 * fresh,
               "a note after a silent gap peaked far above the same note on a "
               "fresh engine");
        expect(afterSilence > 0.25 * fresh,
               "a note after a silent gap was suppressed");
    }
}

void testRepluckLandsTheHandOnTheString()
{
    // The picking hand lands on a sounding string before it plucks it again,
    // so what the string held goes on under the hand while the new pluck is
    // released from rest - the contact a taken string already goes through.
    // Projecting the stored shape onto the modes with a node at the contact,
    // in one sample, clicked into the limiter and let the near-node modes
    // pile up pluck after pluck; and a note repeated after its key came up
    // hopped to another string that could reach it.
    const auto repeated = [] (int note, bool releaseBetween, double rate)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = acustra::StringMaterial::Steel;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        engine.setParameters(parameters);
        engine.setSympatheticStringsEnabled(false);
        std::vector<float> l(static_cast<std::size_t>(blockSize));
        std::vector<float> r(static_cast<std::size_t>(blockSize));
        std::vector<double> peaks;
        std::vector<double> firstSamples;
        std::vector<double> levels;
        std::vector<int> voices;
        double before = 0.0;
        for (int repeat = 0; repeat < 6; ++repeat)
        {
            engine.noteOn(note, 0.62f);
            voices.push_back(engine.getActiveVoiceCount());
            double peakValue = 0.0;
            double firstSample = 0.0;
            double energy = 0.0;
            int counted = 0;
            const int span = static_cast<int>(0.25 * rate);
            for (int i = 0; i < span; i += blockSize)
            {
                const int count = std::min(blockSize, span - i);
                engine.process(l.data(), r.data(), count);
                for (int k = 0; k < count; ++k)
                {
                    const double value = std::max(
                        std::abs(l[static_cast<std::size_t>(k)]),
                        std::abs(r[static_cast<std::size_t>(k)]));
                    if (i == 0 && k < 4)
                        firstSample = std::max(firstSample, value);
                    peakValue = std::max(peakValue, value);
                    if (i + k >= static_cast<int>(0.03 * rate)
                        && i + k < static_cast<int>(0.10 * rate))
                    {
                        energy += value * value;
                        ++counted;
                    }
                    if (i + k >= span - static_cast<int>(0.01 * rate))
                        before = std::max(before, value);
                }
            }
            peaks.push_back(peakValue);
            firstSamples.push_back(firstSample);
            levels.push_back(std::sqrt(energy / std::max(counted, 1)));
            if (releaseBetween)
            {
                engine.noteOff(note);
                const int gap = static_cast<int>(0.05 * rate);
                for (int i = 0; i < gap; i += blockSize)
                    engine.process(l.data(), r.data(),
                                   std::min(blockSize, gap - i));
            }
        }
        return std::tuple { peaks, firstSamples, levels, voices, before };
    };

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const std::string at = " at " + std::to_string(static_cast<int>(rate));
        // Held key, replucked six times at 250 ms: the level neither climbs
        // nor clicks.
        const auto [peaks, firsts, levels, voices, before]
            = repeated(43, false, rate);
        expect(peaks[0] > 1.0e-4, "the first pluck was silent" + at);
        for (std::size_t index = 1; index < peaks.size(); ++index)
        {
            expect(peaks[index] < 1.5 * peaks[0],
                   "repluck " + std::to_string(index) + " peaked "
                   + std::to_string(peaks[index] / peaks[0])
                   + " times the first" + at);
            expect(levels[index] < 1.6 * levels[0],
                   "repluck " + std::to_string(index) + " piled up energy" + at);
            expect(firsts[index] < 0.5 * peaks[index],
                   "repluck " + std::to_string(index)
                   + " clicked on its first samples" + at);
        }
        expect(peaks.back() < 0.9, "a repluck approached full scale" + at);

        // Released between repeats, a note three strings can reach stays on
        // the string that was sounding it, and one voice is enough.
        const auto [hopPeaks, hopFirsts, hopLevels, hopVoices, hopBefore]
            = repeated(64, true, rate);
        for (const int count : hopVoices)
            expect(count == 1, "a repeated E4 hopped to another string" + at);
        for (std::size_t index = 1; index < hopPeaks.size(); ++index)
        {
            expect(hopPeaks[index] < 1.5 * hopPeaks[0]
                       && hopPeaks[index] > 0.5 * hopPeaks[0],
                   "a repeated E4 changed level on repeat" + at);
            expect(hopFirsts[index] < 0.5 * hopPeaks[index],
                   "a repeated E4 clicked on its first samples" + at);
        }
    }

    // A first pluck is untouched: a fresh engine and one that has only ever
    // been silent render the same bits.
    acustra::EngineParameters parameters;
    parameters.stringMaterial = acustra::StringMaterial::Steel;
    const auto plain = renderWithInitialParameters(parameters, 43, 0.62f, 0.5);
    acustra::AcustraEngine engine;
    engine.setParameters(parameters);
    engine.prepare(sampleRate, blockSize);
    engine.noteOn(43, 0.62f);
    const int samples = static_cast<int>(0.5 * sampleRate);
    Audio again { std::vector<float>(static_cast<std::size_t>(samples)),
                  std::vector<float>(static_cast<std::size_t>(samples)) };
    for (int offset = 0; offset < samples; offset += blockSize)
        engine.process(again.left.data() + offset, again.right.data() + offset,
                       std::min(blockSize, samples - offset));
    expect(plain.left == again.left && plain.right == again.right,
           "a first pluck is not what a fresh engine renders");
}

void testSteelFretT60SlopeRaisesOnlyFrettedSteelSustain()
{
    auto neutral = acustra::fittedPhysicalCalibration;
    neutral.steelFretT60Slope = 0.0f;
    auto negative = neutral;
    negative.steelFretT60Slope = -0.030f;

    const auto neutralFretted = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 84, sampleRate, neutral);
    const auto negativeFretted = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 84, sampleRate, negative);
    expect(negativeFretted.loopGain > neutralFretted.loopGain,
           "negative steel fret-T60 slope did not raise fretted sustain");

    const auto neutralOpen = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, neutral);
    const auto negativeOpen = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Steel, 40, sampleRate, negative);
    const auto neutralNylon = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Nylon, 84, sampleRate, neutral);
    const auto negativeNylon = acustra::AcustraEngineTestAccess::configuredLoop(
        acustra::StringMaterial::Nylon, 84, sampleRate, negative);
    expect(negativeOpen.loopGain == neutralOpen.loopGain
               && negativeNylon.loopGain == neutralNylon.loopGain,
           "steel fret-T60 slope changed an open steel or nylon string");
}

void testApertureRegisterExponentChangesOnlyRegisterGeometry()
{
    constexpr float apertureSamples = 3.25f;
    constexpr float apertureScale = 1.125f;
    const float reference
        = acustra::AcustraEngineTestAccess::apertureReferenceDelay();
    for (const float current : { 2.0f * reference, reference,
                                 0.5f * reference })
    {
        const float legacy = apertureSamples * apertureScale
                           / std::max(current, 8.0f);
        const float registered
            = acustra::AcustraEngineTestAccess::registeredAperture(
                apertureSamples, apertureScale, current, 1.0f);
        expect(registered == legacy,
               "exponent one changed the promoted aperture formula");
    }

    const float lowCurrent = 2.0f * reference;
    const float highCurrent = 0.5f * reference;
    const float lowOriginal
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, lowCurrent, 1.0f);
    const float lowFlat
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, lowCurrent, 0.0f);
    const float highOriginal
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, highCurrent, 1.0f);
    const float highFlat
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, highCurrent, 0.0f);
    expect(lowFlat > 1.99f * lowOriginal
               && highFlat < 0.51f * highOriginal,
           "zero register exponent did not darken lows and brighten highs");
    const float lowNegative
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, lowCurrent, -0.5f);
    const float highNegative
        = acustra::AcustraEngineTestAccess::registeredAperture(
            apertureSamples, apertureScale, highCurrent, -0.5f);
    expect(lowNegative > 1.41f * lowFlat
               && highNegative < 0.71f * highFlat,
           "negative register exponent did not continue the fitted direction");

    auto legacy = acustra::fittedPhysicalCalibration;
    legacy.apertureRegisterExponent = 1.0f;
    auto flat = acustra::fittedPhysicalCalibration;
    flat.apertureRegisterExponent = 0.0f;
    const auto originalLow = acustra::AcustraEngineTestAccess::pluck(
        legacy, acustra::StringMaterial::Steel, 0.8f, 40);
    const auto flatLow = acustra::AcustraEngineTestAccess::pluck(
        flat, acustra::StringMaterial::Steel, 0.8f, 40);
    const auto originalHigh = acustra::AcustraEngineTestAccess::pluck(
        legacy, acustra::StringMaterial::Steel, 0.8f, 84);
    const auto flatHigh = acustra::AcustraEngineTestAccess::pluck(
        flat, acustra::StringMaterial::Steel, 0.8f, 84);
    expect(flatLow.peakDisplacement < originalLow.peakDisplacement
               && flatHigh.peakDisplacement > originalHigh.peakDisplacement,
           "register exponent did not reach the note-on aperture geometry");
}

void testOrdinaryOutputIsLinearAndPathologicalOutputIsBounded()
{
    acustra::EngineParameters quiet;
    quiet.outputGain = 0.10f;
    auto louder = quiet;
    louder.outputGain = 0.20f;
    const auto a = renderWithInitialParameters(quiet, 52, 0.90f, 0.35);
    const auto b = renderWithInitialParameters(louder, 52, 0.90f, 0.35);
    expect(peak(b) < 0.89125094,
           "linearity probe unexpectedly reached the safety limiter");
    double differenceEnergy = 0.0;
    double referenceEnergy = 0.0;
    for (std::size_t sample = 0; sample < a.left.size(); ++sample)
    {
        for (const auto pair : { std::pair { a.left[sample], b.left[sample] },
                                 std::pair { a.right[sample], b.right[sample] } })
        {
            const double expected = 2.0 * pair.first;
            const double difference = pair.second - expected;
            differenceEnergy += difference * difference;
            referenceEnergy += expected * expected;
        }
    }
    expect(referenceEnergy > 0.0,
           "linearity probe rendered silence");
    expect(std::sqrt(differenceEnergy / referenceEnergy) < 1.0e-6,
           "ordinary output is not linear below the -1 dBFS safety threshold");

    acustra::EngineParameters hostile;
    hostile.outputGain = 4.0f;
    const auto bounded = renderWithInitialParameters(
        hostile, 52, 1.0f, 0.35);
    expect(peak(bounded) <= 1.0,
           "safety limiter exceeded unit headroom");
}

// A lifted key must only take energy out of the string. The release loss is
// a per-round-trip gain, and applying its full value to the first sample
// after note-off stepped the wave the junction reads by up to a third on a low
// fretted note, which the bridge and body rang on as a thump 4.6 to 8.6 dB
// above the note's own level at that moment. Ramping the loss in over one
// round trip, the unit it is defined in, leaves nothing above the held note.
void testAScheduledPluckIsANoteOnIssuedThen()
{
    // A strum takes its strings at once and releases them one after another.
    // A pluck scheduled D samples ahead must be the note-on issued D samples
    // later, sample for sample, and nothing at all before it.
    for (const int delay : { 1, 97, 480, 2000 })
    {
        acustra::AcustraEngine scheduled;
        acustra::AcustraEngine issued;
        scheduled.prepare(sampleRate, blockSize);
        issued.prepare(sampleRate, blockSize);
        const int total = delay + static_cast<int>(0.4 * sampleRate);
        std::vector<float> a(static_cast<std::size_t>(total));
        std::vector<float> b(static_cast<std::size_t>(total));
        std::vector<float> scratch(static_cast<std::size_t>(total));
        scheduled.noteOn(52, 0.8f, 1, delay);
        scheduled.process(a.data(), scratch.data(), total);
        issued.process(b.data(), scratch.data(), delay);
        issued.noteOn(52, 0.8f);
        issued.process(b.data() + delay, scratch.data(), total - delay);
        bool silentBefore = true;
        for (int sample = 0; sample < delay; ++sample)
            silentBefore = silentBefore && a[static_cast<std::size_t>(sample)] == 0.0f;
        expect(silentBefore, "a scheduled pluck sounded before its time");
        expect(a == b, "a scheduled pluck differed from the note-on issued then");
    }

    // The hand leaving before the pick arrives means the string never sounds.
    acustra::AcustraEngine cancelled;
    cancelled.prepare(sampleRate, blockSize);
    std::vector<float> left(static_cast<std::size_t>(sampleRate));
    std::vector<float> right(static_cast<std::size_t>(sampleRate));
    cancelled.noteOn(52, 0.8f, 1, 4800);
    cancelled.process(left.data(), right.data(), 2400);
    cancelled.noteOff(52);
    cancelled.process(left.data(), right.data(), static_cast<int>(sampleRate));
    expect(std::all_of(left.begin(), left.end(), [] (float v) { return v == 0.0f; }),
           "a pluck released before the pick arrived still sounded");
    acustra::AcustraEngine silenced;
    silenced.prepare(sampleRate, blockSize);
    silenced.noteOn(52, 0.8f, 1, 4800);
    silenced.process(left.data(), right.data(), 2400);
    silenced.allSoundOff();
    silenced.process(left.data(), right.data(), static_cast<int>(sampleRate));
    expect(std::all_of(left.begin(), left.end(), [] (float v) { return v == 0.0f; }),
           "All Sound Off left a scheduled pluck waiting");
}

void testStrumTimingFollowsThePickAcrossTheStrings()
{
    // The k-th string a strum reaches sounds k spacings later at the pick's
    // speed for that velocity: later strings later, harder strums faster,
    // and the whole sweep between the map's two endpoints.
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    expect(engine.strumDelaySamples(0, 0.5f) == 0,
           "the first string of a strum did not sound at once");
    for (const float velocity : { 0.1f, 0.5f, 1.0f })
        for (int rank = 1; rank < 6; ++rank)
            expect(engine.strumDelaySamples(rank, velocity)
                       > engine.strumDelaySamples(rank - 1, velocity),
                   "a later string of a strum did not sound later");
    for (int rank = 1; rank < 6; ++rank)
        expect(engine.strumDelaySamples(rank, 1.0f)
                   < engine.strumDelaySamples(rank, 0.2f),
               "a harder strum did not cross the strings faster");
    const double softest = engine.strumDelaySamples(5, 0.0f) / sampleRate;
    const double hardest = engine.strumDelaySamples(5, 1.0f) / sampleRate;
    expect(std::abs(softest - 0.108) < 0.004 && std::abs(hardest - 0.018) < 0.002,
           "the strum's endpoints moved from the player's map");
}

void testNoTwoPlucksLandInTheSamePlace()
{
    // Each pluck draws its own point within the take-to-take spread the
    // recordings show, and stays inside it.
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, blockSize);
    std::vector<float> left(static_cast<std::size_t>(blockSize));
    std::vector<float> right(static_cast<std::size_t>(blockSize));
    std::vector<double> points;
    for (int take = 0; take < 6; ++take)
    {
        engine.noteOn(52, 0.8f);
        points.push_back(acustra::AcustraEngineTestAccess::lastPluckPoint(engine));
        for (int block = 0; block < 40; ++block)
            engine.process(left.data(), right.data(), blockSize);
        engine.noteOff(52);
        for (int block = 0; block < 400; ++block)
            engine.process(left.data(), right.data(), blockSize);
    }
    const auto [lowest, highest] = std::minmax_element(points.begin(), points.end());
    expect(*highest - *lowest <= 0.0401 && *lowest > 0.0,
           "a pluck landed outside the measured take-to-take spread");
    expect(*highest - *lowest > 1.0e-4,
           "six plucks of one note all landed in the same place");
}

void testNoteOffOnlyRemovesEnergy()
{
    const auto renderNote = [] (acustra::StringMaterial material,
                                std::initializer_list<int> notes,
                                bool release, double rate)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = material;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        engine.setParameters(parameters);
        const int total = static_cast<int>(2.0 * rate);
        const int onAt = static_cast<int>(0.2 * rate);
        const int offAt = static_cast<int>(1.0 * rate);
        Audio result { std::vector<float>(static_cast<std::size_t>(total)),
                       std::vector<float>(static_cast<std::size_t>(total)) };
        int rendered = 0;
        const auto renderTo = [&] (int target)
        {
            while (rendered < target)
            {
                const int count = std::min(blockSize, target - rendered);
                engine.process(result.left.data() + rendered,
                               result.right.data() + rendered, count);
                rendered += count;
            }
        };
        renderTo(onAt);
        int index = 0;
        for (const int note : notes)
        {
            engine.noteOn(note, 0.7f);
            renderTo(onAt + static_cast<int>(0.028 * rate) * ++index);
        }
        renderTo(offAt);
        if (release)
            for (const int note : notes)
                engine.noteOff(note);
        renderTo(total);
        return result;
    };

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto material : { acustra::StringMaterial::Steel,
                                     acustra::StringMaterial::Nylon })
            for (const auto& notes : std::vector<std::vector<int>> {
                     { 43 }, { 48 }, { 40, 47, 52, 56, 59, 64 } })
            {
                std::initializer_list<int> list {};
                std::vector<int> copy = notes;
                const auto released = [&]
                {
                    acustra::AcustraEngine dummy; (void) dummy; (void) list;
                    return Audio {};
                };
                (void) released;
                Audio held;
                Audio lifted;
                if (copy.size() == 1)
                {
                    held = renderNote(material, { copy[0] }, false, rate);
                    lifted = renderNote(material, { copy[0] }, true, rate);
                }
                else
                {
                    held = renderNote(material,
                        { copy[0], copy[1], copy[2], copy[3], copy[4], copy[5] },
                        false, rate);
                    lifted = renderNote(material,
                        { copy[0], copy[1], copy[2], copy[3], copy[4], copy[5] },
                        true, rate);
                }
                const int offAt = static_cast<int>(1.0 * rate);
                const int window = static_cast<int>(0.05 * rate);
                const double before = peak(held, offAt - window, offAt);
                const double after = peak(lifted, offAt, offAt + window);
                double removed = 0.0;
                for (int sample = offAt; sample < offAt + window; ++sample)
                {
                    const auto i = static_cast<std::size_t>(sample);
                    removed = std::max(removed, static_cast<double>(std::abs(
                        0.5 * ((lifted.left[i] - held.left[i])
                             + (lifted.right[i] - held.right[i])))));
                }
                const std::string label = std::string(
                    material == acustra::StringMaterial::Steel ? "steel" : "nylon")
                    + (copy.size() == 1 ? " note " + std::to_string(copy[0])
                                        : " chord")
                    + " at " + std::to_string(static_cast<int>(rate));
                expect(before > 1.0e-4,
                       label + ": the held note is audible before the release");
                // A hand-damped note leaves what it drove ringing: in the
                // two-way junction a G2's third partial keeps the idle D
                // string's second sounding through the anchor's 306 Hz
                // resonance, peaking 35 ms after the note-off at 1.4x the
                // held peak. The defect this guards against was a step at
                // the release sample of half the note, so the first 5 ms
                // may not exceed the held peak and the 50 ms may not
                // exceed it by more than 1.5x.
                const double atRelease = peak(lifted, offAt,
                    offAt + static_cast<int>(0.005 * rate));
                expect(atRelease <= 1.05 * before,
                       label + ": the release stepped " + std::to_string(atRelease)
                       + " over a held " + std::to_string(before));
                expect(after <= 1.5 * before,
                       label + ": note-off peaks " + std::to_string(after)
                       + " over a held " + std::to_string(before));
                expect(removed <= 2.0 * before,
                       label + ": the release adds " + std::to_string(removed)
                       + " against a held " + std::to_string(before));
            }
}

// The fretting hand's own excitations. A hammer-on is a finger driving a
// dent down onto the fret; a lift is the pressed string following the
// finger back to its rest line, or a pull-off when the finger is faster than
// the string; either carries the energy the pluck's velocity law assigns to
// the same MIDI velocity, so the hand's articulations sit at the loudness a
// player expects of that velocity. Lift zero is the hand staying on the
// string and must be exactly the note-off it always was.
void testFrettingHandFollowsThePluckLaw()
{
    struct Phrase
    {
        Audio audio;
        std::size_t event;
    };
    const auto render = [] (acustra::StringMaterial material, double rate,
                            bool legato, int first, int second, float velocity,
                            int release, float lift, double eventAt)
    {
        acustra::AcustraEngine engine;
        acustra::EngineParameters parameters;
        parameters.stringMaterial = material;
        parameters.outputGain = 0.06f;
        engine.setParameters(parameters);
        engine.prepare(rate, blockSize);
        engine.setParameters(parameters);
        engine.setLegato(legato);
        engine.setSympatheticStringsEnabled(false);
        const int total = static_cast<int>(3.0 * rate);
        Audio audio { std::vector<float>(static_cast<std::size_t>(total)),
                      std::vector<float>(static_cast<std::size_t>(total)) };
        int rendered = 0;
        const auto renderTo = [&] (int target)
        {
            while (rendered < target)
            {
                const int count = std::min(blockSize, target - rendered);
                engine.process(audio.left.data() + rendered,
                               audio.right.data() + rendered, count);
                rendered += count;
            }
        };
        renderTo(static_cast<int>(0.2 * rate));
        if (first > 0)
            engine.noteOn(first, 0.8f);
        renderTo(static_cast<int>(eventAt * rate));
        const auto event = static_cast<std::size_t>(rendered);
        if (second > 0)
            engine.noteOn(second, velocity);
        if (release > 0)
            engine.noteOff(release, 1, lift);
        renderTo(total);
        return Phrase { audio, event };
    };
    const auto peakAfter = [] (const Phrase& phrase, double rate,
                               double begin, double end)
    {
        const auto from = phrase.event + static_cast<std::size_t>(begin * rate);
        const auto to = phrase.event + static_cast<std::size_t>(end * rate);
        return peak(phrase.audio, static_cast<int>(from), static_cast<int>(to));
    };
    const auto energyAfter = [] (const Phrase& phrase, double rate,
                                 double begin, double end)
    {
        const auto from = phrase.event + static_cast<std::size_t>(begin * rate);
        const auto to = phrase.event + static_cast<std::size_t>(end * rate);
        const double r = rms(phrase.audio, static_cast<int>(from),
                             static_cast<int>(to));
        return r * r * static_cast<double>(to - from);
    };
    const auto bandAfter = [] (const Phrase& phrase, double rate,
                               double frequency, double begin, double end)
    {
        return spectralPeakFrequency(phrase.audio, frequency, 60.0,
            static_cast<double>(phrase.event) / rate + begin,
            static_cast<double>(phrase.event) / rate + end, rate);
    };
    // Energies, so decibels are 10 log.
    const auto within = [] (double a, double b, double dB)
    {
        return a > 0.0 && b > 0.0
            && std::abs(10.0 * std::log10(a / b)) <= dB;
    };

    for (const auto material : { acustra::StringMaterial::Steel,
                                 acustra::StringMaterial::Nylon })
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            const std::string label = std::string(
                material == acustra::StringMaterial::Steel ? "steel" : "nylon")
                + " at " + std::to_string(static_cast<int>(rate));
            // Lift zero is exactly the note-off it always was.
            const auto plain = render(material, rate, false, 43, 0, 0.0f, 43,
                                      0.0f, 1.0);
            const auto zero = render(material, rate, false, 43, 0, 0.0f, 43,
                                     0.0f, 1.0);
            expect(plain.audio.left == zero.audio.left
                       && plain.audio.right == zero.audio.right,
                   label + ": a zero lift is not the plain note-off");

            // Lifted, the string sounds its open pitch, not the fretted one,
            // and carries about the energy a pluck at that velocity would.
            double previousEnergy = 0.0;
            for (const float lift : { 0.3f, 0.6f, 1.0f })
            {
                const auto lifted = render(material, rate, false, 43, 0, 0.0f,
                                           43, lift, 1.0);
                const auto plucked = render(material, rate, false, 40, 0, 0.0f,
                                            0, 0.0f, 1.0);
                // The pluck lands at 0.2 s; its own attack window.
                Phrase pluckedAt { plucked.audio,
                                   static_cast<std::size_t>(0.2 * rate) };
                const auto openPluck = render(material, rate, false, 0, 40,
                                              lift, 0, 0.0f, 1.0);
                const double openHz = 440.0 * std::exp2((40.0 - 69.0) / 12.0);
                const double found = bandAfter(lifted, rate, openHz, 0.25, 0.75);
                expect(std::abs(1200.0 * std::log2(found / openHz)) < 30.0,
                       label + ": a lift of " + std::to_string(lift)
                       + " did not leave the open string sounding");
                const double liftEnergy = energyAfter(lifted, rate, 0.0, 1.0);
                const double pluckEnergy = energyAfter(openPluck, rate, 0.0, 1.0);
                // A lifted open string now shares what it carries with the
                // strings the two-way junction couples it to, and the stub
                // anchor drives the body less at its fundamental, so a full
                // lift renders 8 dB under a pluck where it rendered within 6.
                expect(within(liftEnergy, pluckEnergy, 10.0),
                       label + ": lift " + std::to_string(lift) + " carries "
                       + std::to_string(10.0 * std::log10(liftEnergy / pluckEnergy))
                       + " dB against a pluck at that velocity");
                // Once the finger outruns the string the release is the
                // pressed shape whole, so faster is no louder.
                expect(liftEnergy >= 0.999 * previousEnergy,
                       label + ": lift energy falls with velocity");
                previousEnergy = liftEnergy;
                // Nothing clicks at the event sample: the first millisecond
                // stays under what the string already had.
                expect(peakAfter(lifted, rate, 0.0, 0.001)
                           <= 1.2 * peakAfter(plain, rate, -0.05, 0.0),
                       label + ": a lift clicked on its first sample");
                (void) pluckedAt;
            }

            // A hammer-on lands at the loudness of a pluck at its velocity
            // and gets louder with velocity.
            double previousHammer = 0.0;
            for (const float velocity : { 0.3f, 0.6f, 1.0f })
            {
                const auto hammered = render(material, rate, true, 43, 45,
                                             velocity, 0, 0.0f, 1.0);
                const auto plucked = render(material, rate, false, 0, 45,
                                            velocity, 0, 0.0f, 1.0);
                const double hammerEnergy = energyAfter(hammered, rate, 0.0, 1.0);
                const double pluckEnergy = energyAfter(plucked, rate, 0.0, 1.0);
                expect(within(hammerEnergy, pluckEnergy, 8.0),
                       label + ": hammer-on at " + std::to_string(velocity)
                       + " carries " + std::to_string(
                           10.0 * std::log10(hammerEnergy / pluckEnergy))
                       + " dB against a pluck at that velocity");
                expect(hammerEnergy >= 0.999 * previousHammer,
                       label + ": hammer-on energy falls with velocity");
                previousHammer = hammerEnergy;
                const double newHz = 440.0 * std::exp2((45.0 - 69.0) / 12.0);
                const double found = bandAfter(hammered, rate, newHz, 0.25, 0.75);
                expect(std::abs(1200.0 * std::log2(found / newHz)) < 30.0,
                       label + ": a hammer-on did not sound the new note");
                for (std::size_t index = 0; index < hammered.audio.left.size();
                     ++index)
                    expect(std::isfinite(hammered.audio.left[index])
                               && std::abs(hammered.audio.left[index]) <= 1.0f,
                           label + ": a hammer-on left headroom or finiteness");
            }

            // A pull-off falls to the held note and is plucked by the finger
            // that left, as loud as the lift was fast.
            {
                const auto pulled = render(material, rate, true, 43, 47, 0.8f,
                                           47, 0.8f, 1.0);
                const double heldHz = 440.0 * std::exp2((43.0 - 69.0) / 12.0);
                const double found = bandAfter(pulled, rate, heldHz, 0.25, 0.75);
                expect(std::abs(1200.0 * std::log2(found / heldHz)) < 30.0,
                       label + ": a pull-off did not fall to the held note");
                const auto plucked = render(material, rate, false, 0, 43, 0.8f,
                                            0, 0.0f, 1.0);
                expect(within(energyAfter(pulled, rate, 0.0, 1.0),
                              energyAfter(plucked, rate, 0.0, 1.0), 8.0),
                       label + ": a pull-off at 0.8 is not at a pluck's energy");
            }
        }
}

void testEachStringMaterialPlaysItsOwnMeasuredGuitar()
{
    using Access = acustra::AcustraEngineTestAccess;
    const auto calibration = acustra::fittedPhysicalCalibration;

    // Steel plays g21 and nylon a classical; the banks are two measured
    // instruments, so the lowest body mode and the bridge's immediate
    // admittance must both move with the material.
    const auto steelBody = Access::configuredBody(
        calibration, 0, acustra::StringMaterial::Steel);
    const auto nylonBody = Access::configuredBody(
        calibration, 0, acustra::StringMaterial::Nylon);
    expect(std::abs(steelBody.frequency - nylonBody.frequency) > 1.0,
           "both string materials configured the same lowest body mode");
    const double steelAdmittance = Access::bridgeAdmittance(
        calibration, acustra::StringMaterial::Steel);
    const double nylonAdmittance = Access::bridgeAdmittance(
        calibration, acustra::StringMaterial::Nylon);
    expect(std::abs(nylonAdmittance - steelAdmittance)
               > 0.01 * std::abs(steelAdmittance),
           "both string materials configured the same bridge admittance");

    // Changing the material on a prepared engine has to move both banks, not
    // only the strings.
    acustra::AcustraEngine engine;
    engine.prepare(48000.0, 64);
    expect(std::abs(Access::bridgeAdmittanceOf(engine) - steelAdmittance)
               <= 1.0e-12,
           "a prepared engine did not start on the steel bridge bank");
    acustra::EngineParameters parameters;
    parameters.stringMaterial = acustra::StringMaterial::Nylon;
    engine.setParameters(parameters);
    expect(std::abs(Access::bridgeAdmittanceOf(engine) - nylonAdmittance)
               <= 1.0e-12,
           "switching to nylon left the steel bridge bank in place");

    // The arrays are sized to the larger bank; the surplus slots the shorter
    // one leaves must be silent, not stale.
    acustra::AcustraEngine steelEngine;
    steelEngine.prepare(48000.0, 64);
    for (int index = static_cast<int>(
             acustra::detail::measuredSteelBodyModes.size());
         index < Access::bodyModeCapacity; ++index)
        expect(Access::bodyResidueOf(steelEngine, index) == 0.0,
               "a surplus body slot kept the other bank's mode");

    // Both bridge banks must stay positive real: a negative weight would make
    // the junction active. This guards a hand-edited header, not the fit.
    for (const auto& mode : acustra::detail::measuredNylonBridgeModes)
        expect(mode.weight > 0.0f && mode.q > 0.0f
                   && mode.frequency >= 60.0f && mode.frequency <= 10000.0f,
               "the nylon bridge bank left the passive fit's range");
    for (const auto& mode : acustra::detail::measuredSteelBridgeModes)
        expect(mode.weight > 0.0f && mode.q > 0.0f
                   && mode.frequency >= 60.0f && mode.frequency <= 10000.0f,
               "the steel bridge bank left the passive fit's range");
}

void testPerformance()
{
    acustra::AcustraEngine engine;
    engine.prepare(sampleRate, 64);
    for (const int note : { 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 0.9f);
    std::vector<float> left(64);
    std::vector<float> right(64);
    constexpr int seconds = 4;
    const auto start = std::chrono::steady_clock::now();
    for (int sample = 0; sample < static_cast<int>(sampleRate) * seconds;
         sample += 64)
        engine.process(left.data(), right.data(), 64);
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double realtimeRatio = elapsed / seconds;
    std::cout << "Acustra six-string realtime ratio: " << realtimeRatio << '\n';
    expect(realtimeRatio < 0.25,
           "six-string engine exceeded the 0.25x realtime CPU gate");
}
} // namespace

int main()
{
    testSilenceAndFiniteOutput();
    testPlayableRangeFollowsTuning();
    testSteelRetuningPreservesStringMass();
    testAudiblePhysicalDecay();
    testPhysicalPluckOnsetIsBounded();
    testBridgeObservableIsSampleRateNormalised();
    testZeroWidthCollapsesToMono();
    testReleaseEventuallyReturnsTheString();
    testPhysicalVoiceOwnsReleaseLifecycle();
    testMidiChannelOwnershipAndAdditiveBend();
    testSharedBodyExcitesIdleStrings();
    testSympatheticStringsAreAudibleButBounded();
    testPassiveBridgeBranchesBalance();
    testConstructionControlsChangeTheModel();
    testAgeRemovesUpperStringEnergy();
    testMaterialSpecificAttackPitchIsBoundedAndVelocityResponsive();
    testSteadyPitchIsCompensated();
    testPhysicalSustainSettlesNearRequestedPitch();
    testLoadedE2IsCentredAndNotSplit();
    testSteelDispersionTracksTheStiffStringLaw();
    testDispersionAcrossRatesMaterialsAndNotes();
    testBlockPartitionIsDeterministic();
    testSampleRatesAndAutomationStayBounded();
    testHostileParametersAreSanitised();
    testHostilePhysicalCalibrationIsSanitised();
    testBodyAndBridgeCalibrationChangePhysicalDescriptors();
    testMaterialCalibrationChangesStringAndPluckDescriptors();
    testHighLossCutoffScaleChangesOnlyUpperLoss();
    testPlateConductanceFloorDampsOnlyTheUpperBand();
    testStolenStringKeepsRingingUnderHandDamping();
    testBridgeHandPressureShortensAndDarkens();
    testNaturalHarmonicsReachAboveTheFretboard();
    testHeldStringsDoNotLengthenANoteDecay();
    testANoteOverASoundingInstrumentDoesNotClick();
    testSwitchingStringsOrTuningUnderAChordDoesNotClick();
    testLegatoHammersOnAndPullsOff();
    testLongitudinalModesGrowWithVelocity();
    testTodaysMechanismsSurviveEachOther();
    testNoteAfterSilenceDoesNotClick();
    testRepluckLandsTheHandOnTheString();
    testSteelFretT60SlopeRaisesOnlyFrettedSteelSustain();
    testApertureRegisterExponentChangesOnlyRegisterGeometry();
    testOrdinaryOutputIsLinearAndPathologicalOutputIsBounded();
    testNoteOffOnlyRemovesEnergy();
    testAScheduledPluckIsANoteOnIssuedThen();
    testStrumTimingFollowsThePickAcrossTheStrings();
    testNoTwoPlucksLandInTheSamePlace();
    testFrettingHandFollowsThePluckLaw();
    testEachStringMaterialPlaysItsOwnMeasuredGuitar();
    testPerformance();
    if (failures == 0)
        std::cout << "All Acustra engine tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#include "DSP/TaikoEngine.h"
#include "DSP/UiMath.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace taikor
{
// The engine grants this struct access so the suite can exercise the airborne
// delay line's index arithmetic directly. It is not part of the plug-in API.
struct TaikoEngineTestAccess
{
    static float readDelayLine (const std::array<float, TaikoEngine::directLineSize>& line,
                                int writeIndex, float delaySamples) noexcept
    {
        return TaikoEngine::readDelayLine (line, writeIndex, delaySamples);
    }

    static constexpr int lineSize = TaikoEngine::directLineSize;

    static int activeModeCount (const TaikoEngine& engine) noexcept
    {
        return engine.voices_[0].activeModeCount;
    }

    // The two airborne path delays, in samples, as the strike geometry
    // resolved them. Read directly because the membrane modes reach both
    // microphones instantaneously in this model, so an onset measured from the
    // audio is dominated by them and says nothing about the airborne path.
    static void directDelays (const TaikoEngine& engine, float& left,
                              float& right) noexcept
    {
        left = engine.voices_[0].directDelayLeft;
        right = engine.voices_[0].directDelayRight;
    }

    static constexpr float delayCeiling = TaikoEngine::directLineSize - 4;

    static std::uint32_t lastContactStart (const TaikoEngine& engine) noexcept
    {
        const auto& voice = engine.voices_[0];
        return voice.contactCount > 0
            ? voice.contacts[static_cast<std::size_t> (voice.contactCount - 1)].startSample
            : 0u;
    }

    // The wooden half of a voice's bank: the drum's shell for eleven of the
    // strokes, the pair of sticks for the twelfth. Read from the built voice
    // rather than measured from the audio, because the question these answer -
    // does a drum control reach the stick? - is about which numbers were used,
    // and a spectral measurement of a click that lasts forty milliseconds
    // cannot resolve it.
    static std::vector<float> woodFrequencies (const TaikoEngine& engine,
                                               int slot = 0)
    {
        std::vector<float> result;
        const auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                result.push_back (mode.omega / (2.0f * 3.14159265358979f));
        }
        return result;
    }

    static std::vector<float> woodDecays (const TaikoEngine& engine, int slot = 0)
    {
        std::vector<float> result;
        const auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                result.push_back (mode.decayRate);
        }
        return result;
    }

    // Which voice each of a run of strokes landed in, and how loud each slot
    // ended up. Voice stealing is a decision about slots, so a test of it has
    // to be able to see the slots.
    static int voiceSlotOf (const TaikoEngine& engine, std::uint64_t startOrder) noexcept
    {
        for (int index = 0; index < TaikoEngine::maxVoices; ++index)
            if (engine.voices_[static_cast<std::size_t> (index)].startOrder == startOrder)
                return index;
        return -1;
    }

    // The whole modal bank of the most recently struck voice. Slot zero is not
    // it once anything else is still ringing.
    static std::vector<std::array<float, 5>> newestVoiceBank (const TaikoEngine& engine)
    {
        std::size_t newest = 0;
        for (std::size_t index = 1; index < engine.voices_.size(); ++index)
            if (engine.voices_[index].startOrder > engine.voices_[newest].startOrder)
                newest = index;

        std::vector<std::array<float, 5>> bank;
        const auto& voice = engine.voices_[newest];
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            bank.push_back ({ mode.omega, mode.decayRate, mode.drive, mode.micLeft,
                              mode.micRight });
        }
        return bank;
    }

    // T60 of each membrane mode, ordered by frequency, for the voice just
    // struck. The spread between the lowest two is what decides whether a
    // stroke ends as a drum or as a sine.
    static std::vector<float> membraneT60s (const TaikoEngine& engine)
    {
        std::vector<std::pair<float, float>> byFrequency;
        const auto& voice = engine.voices_[0];
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && mode.decayRate > 0.0f)
                byFrequency.push_back ({ mode.omega, 6.9078f / mode.decayRate });
        }
        std::sort (byFrequency.begin(), byFrequency.end());

        std::vector<float> result;
        result.reserve (byFrequency.size());
        for (const auto& entry : byFrequency)
            result.push_back (entry.second);
        return result;
    }

    static bool everyDecayIsFinite (const TaikoEngine& engine)
    {
        for (const auto& voice : engine.voices_)
            for (int index = 0; index < voice.modeCount; ++index)
            {
                const auto& mode = voice.modes[static_cast<std::size_t> (index)];
                if (! std::isfinite (mode.decayRate) || ! std::isfinite (mode.drive)
                    || ! std::isfinite (mode.omega))
                    return false;
            }
        return true;
    }

    static float radiationEfficiency (int order, float ka) noexcept
    {
        return TaikoEngine::radiationEfficiency (order, ka);
    }

    static std::uint64_t strokeCount (const TaikoEngine& engine) noexcept
    {
        return engine.noteSequence_;
    }

    static int activeVoices (const TaikoEngine& engine) noexcept
    {
        int count = 0;
        for (const auto& voice : engine.voices_)
            if (voice.active)
                ++count;
        return count;
    }

    static float contactSecondsFor (const EngineParameters& parameters,
                                    Articulation articulation, int octaveOffset,
                                    float velocity) noexcept
    {
        return TaikoEngine::measureContact (parameters, articulation, octaveOffset,
                                            velocity);
    }
};
} // namespace taikor

namespace
{
constexpr int defaultBlockSize = 253;
constexpr double analysisPi = 3.1415926535897932384626433832795;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
    double peak { 0.0 };
    double rms { 0.0 };
    bool finite { true };

    [[nodiscard]] std::vector<float> mono() const
    {
        std::vector<float> result (left.size());
        for (std::size_t index = 0; index < left.size(); ++index)
            result[index] = 0.5f * (left[index] + right[index]);
        return result;
    }
};

Rendered render (taikor::TaikoEngine& engine, int numSamples,
                 int blockSize = defaultBlockSize)
{
    Rendered result;
    result.left.resize (static_cast<std::size_t> (numSamples));
    result.right.resize (static_cast<std::size_t> (numSamples));

    std::vector<float> blockLeft (static_cast<std::size_t> (blockSize));
    std::vector<float> blockRight (static_cast<std::size_t> (blockSize));

    double sumOfSquares = 0.0;
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (blockLeft.data(), blockRight.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto index = static_cast<std::size_t> (rendered + sample);
            const float l = blockLeft[static_cast<std::size_t> (sample)];
            const float r = blockRight[static_cast<std::size_t> (sample)];
            result.left[index] = l;
            result.right[index] = r;
            result.finite = result.finite && std::isfinite (l) && std::isfinite (r);
            if (std::isfinite (l) && std::isfinite (r))
            {
                result.peak = std::max ({ result.peak, std::abs (static_cast<double> (l)),
                                          std::abs (static_cast<double> (r)) });
                sumOfSquares += static_cast<double> (l) * l + static_cast<double> (r) * r;
            }
        }
        rendered += count;
    }

    result.rms = numSamples > 0
        ? std::sqrt (sumOfSquares / (2.0 * static_cast<double> (numSamples)))
        : 0.0;
    return result;
}

// Magnitude of one frequency bin, by the Goertzel recurrence. Used instead of a
// full transform because every question in this suite is about a handful of
// modes whose predicted frequencies the engine itself reports.
double binMagnitude (const std::vector<float>& samples, double frequencyHz,
                     double sampleRate, std::size_t first = 0,
                     std::size_t last = std::numeric_limits<std::size_t>::max())
{
    const double omega = 2.0 * analysisPi * frequencyHz / sampleRate;
    const double coefficient = 2.0 * std::cos (omega);
    double s1 = 0.0;
    double s2 = 0.0;

    const auto end = std::min (last, samples.size());
    for (std::size_t index = first; index < end; ++index)
    {
        const double s0 = static_cast<double> (samples[index]) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2));
}

// The strongest partial in a band, found by scanning. Deliberately independent
// of what the engine predicts, so a test can compare the two.
double dominantFrequency (const std::vector<float>& samples, double sampleRate,
                          double lowHz, double highHz, double stepHz,
                          std::size_t first = 0)
{
    double best = -1.0;
    double bestFrequency = lowHz;
    for (double frequency = lowHz; frequency <= highHz; frequency += stepHz)
    {
        const double magnitude = binMagnitude (samples, frequency, sampleRate, first);
        if (magnitude > best)
        {
            best = magnitude;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
}

double correlation (const std::vector<float>& a, const std::vector<float>& b)
{
    double sumAB = 0.0;
    double sumAA = 0.0;
    double sumBB = 0.0;
    const auto count = std::min (a.size(), b.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        sumAB += static_cast<double> (a[index]) * b[index];
        sumAA += static_cast<double> (a[index]) * a[index];
        sumBB += static_cast<double> (b[index]) * b[index];
    }
    if (sumAA <= 0.0 || sumBB <= 0.0)
        return 1.0;
    return sumAB / std::sqrt (sumAA * sumBB);
}

// Root-mean-square over a window, so a test can ask about the settled tail
// rather than about an attack that swamps it.
double windowedRms (const std::vector<float>& samples, std::size_t first,
                    std::size_t last)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t index = first; index < last && index < samples.size(); ++index)
    {
        sum += static_cast<double> (samples[index]) * samples[index];
        ++count;
    }
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

double maximumAbsoluteDifference (const std::vector<float>& a,
                                  const std::vector<float>& b)
{
    double worst = 0.0;
    const auto count = std::min (a.size(), b.size());
    for (std::size_t index = 0; index < count; ++index)
        worst = std::max (worst,
                          std::abs (static_cast<double> (a[index]) - b[index]));
    return worst;
}

// Seconds until the signal's envelope has fallen `decibels` below its peak.
double decayTime (const std::vector<float>& samples, double sampleRate,
                  double decibels)
{
    double peak = 0.0;
    for (const float value : samples)
        peak = std::max (peak, std::abs (static_cast<double> (value)));
    if (peak <= 0.0)
        return 0.0;

    const double threshold = peak * std::pow (10.0, decibels / 20.0);
    for (std::size_t index = samples.size(); index-- > 0;)
        if (std::abs (static_cast<double> (samples[index])) > threshold)
            return static_cast<double> (index) / sampleRate;
    return 0.0;
}

taikor::EngineParameters defaultParameters()
{
    // Deliberately the shipping defaults, untouched. Overriding the output here
    // used to be harmless because it restated the engine's own default; once
    // the instrument grew loud enough to need a quieter one, the override left
    // the whole suite running sixteen decibels hot and measuring the limiter
    // rather than the model.
    return taikor::EngineParameters {};
}

// A single stroke, rendered from a clean engine.
Rendered strike (taikor::EngineParameters parameters, taikor::Articulation articulation,
                 int octaveOffset, float velocity, double sampleRate, int numSamples,
                 int blockSize = defaultBlockSize)
{
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.trigger (articulation, octaveOffset, velocity);
    return render (engine, numSamples, blockSize);
}

// ---------------------------------------------------------------------------

void testArticulationMetadataAndMidiMapping()
{
    std::set<std::string> names;
    std::set<std::string> slugs;

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto& metadata = taikor::getArticulationMetadata (articulation);

        expect (metadata.articulation == articulation,
                "articulation metadata is not self-consistent");
        expect (! metadata.displayName.empty(), "articulation is missing a name");
        expect (! metadata.slug.empty(), "articulation is missing a slug");
        expect (! metadata.description.empty(),
                "articulation is missing a description");
        expect (metadata.pitchClass == static_cast<int> (index),
                "articulation pitch class must equal its enumerator");

        names.insert (std::string (metadata.displayName));
        slugs.insert (std::string (metadata.slug));
    }

    expect (names.size() == taikor::articulationCount,
            "articulation display names are not unique");
    expect (slugs.size() == taikor::articulationCount,
            "articulation slugs are not unique");

    // The vocabulary is one octave: every pitch class is a different stroke,
    // and the octave chooses the drum rather than the stroke.
    for (int note = taikor::lowestPlayableNote; note <= taikor::highestPlayableNote;
         ++note)
    {
        const auto articulation = taikor::articulationForMidiNote (note);
        const auto octave = taikor::octaveOffsetForMidiNote (note);
        expect (articulation.has_value(), "playable note produced no articulation");
        expect (octave.has_value(), "playable note produced no octave");
        if (! articulation.has_value() || ! octave.has_value())
            continue;

        expect (static_cast<int> (*articulation) == note % 12,
                "pitch class must select the stroke");
        expect (*octave >= taikor::lowestOctaveOffset
                    && *octave <= taikor::highestOctaveOffset,
                "octave offset left its declared range");
        expect (taikor::midiNoteFor (*articulation, *octave) == note,
                "note to articulation mapping does not round-trip");
    }

    for (const int note : { 0, 11, taikor::lowestPlayableNote - 1,
                            taikor::highestPlayableNote + 1, 126, 127 })
    {
        expect (! taikor::articulationForMidiNote (note).has_value(),
                "a note outside the playable range must not map to a stroke");
        expect (! taikor::octaveOffsetForMidiNote (note).has_value(),
                "a note outside the playable range must not map to an octave");
    }

    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    expect (! engine.triggerMidi (taikor::lowestPlayableNote - 1, 0.9f),
            "a note below the range must be rejected");
    expect (! engine.triggerMidi (taikor::highestPlayableNote + 1, 0.9f),
            "a note above the range must be rejected");
    expect (engine.triggerMidi (taikor::referenceNote, 0.9f),
            "the reference note must be playable");
}

// The instrument's central promise: within an octave the twelve notes are
// twelve strokes, and between octaves the drum's pitch rises.
void testOctavesRaisePitch()
{
    const auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    float previousLoaded = 0.0f;
    float previousBreathing = 0.0f;
    float previousRadius = 1.0e9f;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto measurements = engine.measureDrum (octave);

        expect (measurements.loadedFundamentalHz > previousLoaded * 1.5f,
                "each octave must raise the sounding fundamental substantially");
        expect (measurements.breathingModeHz > previousBreathing,
                "each octave must raise the breathing mode");
        expect (measurements.radiusMetres < previousRadius,
                "with the default octave body, a higher octave must be a smaller drum");
        expect (measurements.breathingModeHz > measurements.loadedFundamentalHz,
                "the cavity must lift the volume-changing mode above the other one");
        expect (measurements.idealFundamentalHz > measurements.loadedFundamentalHz,
                "air loading must lower the membrane below its ideal frequency");

        previousLoaded = measurements.loadedFundamentalHz;
        previousBreathing = measurements.breathingModeHz;
        previousRadius = measurements.radiusMetres;
    }

    // An octave is an octave: the ratio between adjacent octaves' ideal
    // membrane fundamentals must be exactly two, whatever mixture of size and
    // tension the octave body control chose to buy it with.
    for (const float body : { 0.0f, 0.35f, 0.7f, 1.0f })
    {
        auto tuned = parameters;
        tuned.octaveBody = body;
        engine.setParameters (tuned);

        for (int octave = taikor::lowestOctaveOffset;
             octave < taikor::highestOctaveOffset; ++octave)
        {
            const auto lower = engine.measureDrum (octave);
            const auto upper = engine.measureDrum (octave + 1);
            const auto ratio = upper.idealFundamentalHz / lower.idealFundamentalHz;
            expect (std::abs (ratio - 2.0f) < 0.01f,
                    "an octave must double the membrane's ideal fundamental at octave body "
                        + std::to_string (body));
        }
    }

    // And the rendered audio must actually follow the prediction. The search
    // is confined to the band the two membrane modes occupy, because on the
    // smallest drums the wooden shell is genuinely the loudest thing in the
    // tail - a shime-daiko's body is proportionally far lighter than an
    // odaiko's, so it rings harder - and that is the model working rather than
    // failing. What has to be true is that the head sounds where the physics
    // says it does, and that it rises an octave at a time.
    engine.setParameters (parameters);
    double previousDominant = 0.0;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto measurements = engine.measureDrum (octave);
        const auto rendered =
            strike (parameters, taikor::Articulation::Don, octave, 0.9f, 48000.0, 36000);
        const auto mono = rendered.mono();

        // Skip the attack so the measurement sees the drum ringing rather than
        // the stick landing on it.
        const auto dominant = dominantFrequency (
            mono, 48000.0, measurements.loadedFundamentalHz * 0.80,
            measurements.breathingModeHz * 1.20, 0.25, 3000u);

        const auto nearFundamental =
            std::abs (dominant - measurements.loadedFundamentalHz)
                < std::max (2.0, measurements.loadedFundamentalHz * 0.06);
        const auto nearBreathing =
            std::abs (dominant - measurements.breathingModeHz)
                < std::max (2.0, measurements.breathingModeHz * 0.06);

        expect (nearFundamental || nearBreathing,
                "the rendered head does not sound at either predicted mode for octave "
                    + std::to_string (octave));
        expect (dominant > previousDominant * 1.5,
                "the rendered pitch must rise with the octave");
        previousDominant = dominant;
    }
}

void testEveryArticulationAndSampleRate()
{
    for (const double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        {
            const auto articulation = static_cast<taikor::Articulation> (index);
            const auto name =
                std::string (taikor::getArticulationDisplayName (articulation));

            for (const int octave : { taikor::lowestOctaveOffset, 0,
                                      taikor::highestOctaveOffset })
            {
                const auto rendered = strike (defaultParameters(), articulation, octave,
                                              0.92f, sampleRate,
                                              static_cast<int> (sampleRate * 0.5));

                expect (rendered.finite,
                        name + " produced non-finite audio at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
                expect (rendered.peak > 1.0e-4,
                        name + " produced silence at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
                expect (rendered.peak <= 1.0001,
                        name + " exceeded full scale at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
            }
        }
    }
}

// The drum a set of controls describes must not depend on the host's clock.
void testSampleRateConsistency()
{
    const auto parameters = defaultParameters();
    double referencePitch = 0.0;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto rendered = strike (parameters, taikor::Articulation::Don, 0, 0.9f,
                                      sampleRate, static_cast<int> (sampleRate * 0.6));
        const auto mono = rendered.mono();
        const auto dominant = dominantFrequency (
            mono, sampleRate, 50.0, 200.0, 0.25,
            static_cast<std::size_t> (sampleRate * 0.06));

        if (referencePitch <= 0.0)
            referencePitch = dominant;
        else
            expect (std::abs (dominant - referencePitch) < 1.0,
                    "the drum's pitch moved with the sample rate");

        expect (rendered.finite, "a supported sample rate produced non-finite audio");
    }

    // Hostile rates must be clamped rather than allowed to break the tuning.
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (1.0, 64);
    engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    const auto rendered = render (engine, 512, 64);
    expect (rendered.finite, "an absurd sample rate must not produce non-finite audio");
}

// Velocity must change the timbre, not only the level, and it must do so
// through the contact time rather than through a separate brightness control.
void testVelocitySensitivity()
{
    const auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    float previousContact = 0.0f;
    for (const float velocity : { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f })
    {
        const auto contact =
            engine.measureContactSeconds (taikor::Articulation::Don, 0, velocity);
        expect (contact > 0.0f && contact < 0.05f,
                "contact time left a physically sensible range");
        if (previousContact > 0.0f)
            expect (contact < previousContact,
                    "a harder stroke must shorten the stick's contact with the head");
        previousContact = contact;
    }

    double previousPeak = 0.0;
    double previousBrightness = 0.0;
    double softestBrightness = 0.0;
    double hardestBrightness = 0.0;

    for (const float velocity : { 0.15f, 0.45f, 0.75f, 1.0f })
    {
        const auto rendered = strike (parameters, taikor::Articulation::Don, 0, velocity,
                                      48000.0, 24000);
        const auto mono = rendered.mono();

        // Ratio of high-band to low-band energy over the attack. The low pair
        // is the drum the engine reports rather than two frequencies written
        // for one particular default, so retuning the instrument cannot move
        // the measurement off the partials it is watching.
        const auto reported = taikor::TaikoEngine::measure (parameters, 0);
        const double low =
            binMagnitude (mono, reported.loadedFundamentalHz, 48000.0, 0, 4800)
            + binMagnitude (mono, reported.breathingModeHz, 48000.0, 0, 4800);
        // Integrated across the band rather than sampled at a few frequencies.
        // Most of the drum's high-frequency energy is the head's modal
        // continuum, which is stochastic by construction - it stands for
        // hundreds of modes nobody can resolve - so any single bin of it is
        // noisy enough to reverse a real trend. The band as a whole is not.
        double high = 0.0;
        for (double frequency = 700.0; frequency < 6000.0; frequency *= 1.06)
            high += binMagnitude (mono, frequency, 48000.0, 0, 4800);
        const double brightness = high / std::max (low, 1.0e-9);

        expect (rendered.peak > previousPeak,
                "a harder stroke must be louder");
        // A large reversal would mean the contact law had stopped working; a
        // small one is the continuum being what it is. The overall span is
        // checked after the loop, which is the claim that actually matters.
        expect (brightness > previousBrightness * 0.75,
                "a harder stroke must not lose high partial content");
        if (velocity <= 0.15f)
            softestBrightness = brightness;
        hardestBrightness = brightness;
        previousPeak = rendered.peak;
        previousBrightness = brightness;
    }

    expect (previousBrightness > 0.0, "brightness measurement failed to run");
    // The claim the contact law actually makes: a full-arm stroke is markedly
    // brighter than a ghost note. Stated across the whole range rather than
    // step by step, because most of the drum's high-frequency energy is the
    // head's modal continuum, which is stochastic and will not march in a
    // straight line from one velocity to the next.
    expect (hardestBrightness > softestBrightness * 1.40,
            "a full-velocity stroke must be clearly brighter than a ghost note");

    // A softer bachi must be darker than a hard one at the same velocity.
    auto soft = parameters;
    soft.bachiHardness = 0.05f;
    auto hard = parameters;
    hard.bachiHardness = 1.0f;

    expect (engine.measureContact (soft, taikor::Articulation::Don, 0, 0.8f)
                > engine.measureContact (hard, taikor::Articulation::Don, 0, 0.8f),
            "a soft beater must stay on the head longer than a hard bachi");
}

// Each control must move the term of the model it claims to.
void testPhysicalParameterInfluence()
{
    const auto base = defaultParameters();
    taikor::TaikoEngine engine;
    engine.prepare (48000.0, defaultBlockSize);

    const auto measure = [&engine] (const taikor::EngineParameters& parameters)
    {
        engine.setParameters (parameters);
        return engine.measureDrum (0);
    };

    const auto reference = measure (base);

    // Tension raises the pitch; size lowers it.
    auto tighter = base;
    tighter.tension = 0.95f;
    expect (measure (tighter).idealFundamentalHz > reference.idealFundamentalHz * 1.2f,
            "raising the tension must raise the pitch");

    auto looser = base;
    looser.tension = 0.05f;
    expect (measure (looser).idealFundamentalHz < reference.idealFundamentalHz * 0.85f,
            "lowering the tension must lower the pitch");

    // Stated against the drum this suite actually uses rather than as an
    // absolute size, so it keeps testing the 1/a law if the default moves.
    auto bigger = base;
    bigger.headDiameter = base.headDiameter * 1.5f;
    const auto biggerRatio =
        measure (bigger).idealFundamentalHz / reference.idealFundamentalHz;
    expect (std::abs (biggerRatio - 1.0f / 1.5f) < 0.02f,
            "a bigger drum must be lower in proportion to its radius");

    auto smaller = base;
    smaller.headDiameter = 0.20f;
    expect (measure (smaller).idealFundamentalHz > reference.idealFundamentalHz * 1.8f,
            "a smaller drum must be higher");

    // A heavier head is slower, so it is lower at the same tension - and it is
    // also loaded less by the air, which the model must show separately.
    auto heavy = base;
    heavy.headMaterial = 1.0f;
    auto light = base;
    light.headMaterial = 0.0f;
    const auto heavyDrum = measure (heavy);
    const auto lightDrum = measure (light);
    expect (heavyDrum.idealFundamentalHz < lightDrum.idealFundamentalHz,
            "a heavier head must sound lower at the same tension");
    expect (lightDrum.idealFundamentalHz / lightDrum.loadedFundamentalHz
                > heavyDrum.idealFundamentalHz / heavyDrum.loadedFundamentalHz,
            "a light head must be pulled down by the air more than a heavy one");

    // The cavity is what splits the two heads apart, and only the sealed body
    // can do it.
    auto sealed = base;
    sealed.cavityCoupling = 1.0f;
    auto open = base;
    open.cavityCoupling = 0.0f;
    const auto sealedDrum = measure (sealed);
    const auto openDrum = measure (open);
    expect (sealedDrum.breathingModeHz / sealedDrum.loadedFundamentalHz
                > openDrum.breathingModeHz / openDrum.loadedFundamentalHz * 1.2f,
            "sealing the body must push the breathing mode further above the other");
    expect (std::abs (openDrum.breathingModeHz - openDrum.loadedFundamentalHz)
                < openDrum.loadedFundamentalHz * 0.20f,
            "an uncoupled body must leave the two heads near their own frequencies");

    // A shallow body has a stiffer air spring than a deep one.
    auto shallow = base;
    shallow.bodyDepth = 0.0f;
    auto deep = base;
    deep.bodyDepth = 1.0f;
    expect (measure (shallow).breathingModeHz > measure (deep).breathingModeHz,
            "a shallow body must have a stiffer cavity than a deep one");

    // Pitch is a musical transposition of the whole drum.
    auto raised = base;
    raised.pitch = 12.0f;
    const auto raisedDrum = measure (raised);
    expect (std::abs (raisedDrum.idealFundamentalHz
                      / reference.idealFundamentalHz - 2.0f) < 0.02f,
            "twelve semitones of pitch must double the membrane's frequency");

    // Damping shortens the tail; it must not move the pitch.
    auto damped = base;
    damped.headDamping = 1.0f;
    const auto dampedDrum = measure (damped);
    // Half of the fundamental's loss is radiation, which the damping control
    // cannot reach, so the achievable range is bounded by physics rather than
    // by taste. It still has to be a large effect to be worth a knob.
    expect (dampedDrum.tailSeconds < reference.tailSeconds * 0.5f,
            "raising the damping must substantially shorten the tail");
    expect (dampedDrum.tailSeconds > 0.0f,
            "a fully damped head must still have a finite tail");
    expect (std::abs (dampedDrum.idealFundamentalHz - reference.idealFundamentalHz)
                < reference.idealFundamentalHz * 0.001f,
            "damping must not retune the drum");

    // And the rendered audio must agree that damping shortens the note.
    const auto openTail = strike (base, taikor::Articulation::Don, 0, 0.9f, 48000.0,
                                  48000 * 4);
    const auto dampedTail = strike (damped, taikor::Articulation::Don, 0, 0.9f, 48000.0,
                                    48000 * 4);
    expect (decayTime (dampedTail.mono(), 48000.0, -40.0)
                < decayTime (openTail.mono(), 48000.0, -40.0) * 0.75,
            "the rendered tail must shorten when the head is damped");
}

// Strike position is the whole articulation vocabulary, so it has to be real:
// a centre stroke drives the axisymmetric modes and an edge stroke does not.
void testStrikePositionShapesTheSpectrum()
{
    const auto parameters = defaultParameters();

    const auto centre = strike (parameters, taikor::Articulation::Don, 0, 0.9f,
                                48000.0, 36000);
    const auto edge = strike (parameters, taikor::Articulation::Kara, 0, 0.9f,
                              48000.0, 36000);

    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    const auto measurements = engine.measureDrum (0);

    const auto centreMono = centre.mono();
    const auto edgeMono = edge.mono();

    const auto fundamentalIn = [&] (const std::vector<float>& samples)
    {
        return binMagnitude (samples, measurements.loadedFundamentalHz, 48000.0, 2400)
             + binMagnitude (samples, measurements.breathingModeHz, 48000.0, 2400);
    };
    const auto upperIn = [&] (const std::vector<float>& samples)
    {
        double total = 0.0;
        for (const double frequency : { 420.0, 560.0, 720.0, 900.0 })
            total += binMagnitude (samples, frequency, 48000.0, 2400);
        return total;
    };

    const auto centreRatio = upperIn (centreMono) / std::max (fundamentalIn (centreMono), 1.0e-9);
    const auto edgeRatio = upperIn (edgeMono) / std::max (fundamentalIn (edgeMono), 1.0e-9);

    expect (edgeRatio > centreRatio * 2.0,
            "an edge stroke must carry far more upper partial energy than a centre one");
    // Measured from after the attack rather than from the peak. The peak is the
    // contact and the head's continuum answering it, which is loudest on
    // exactly the edge strokes this is comparing - so a decay time referred to
    // it is partly a measure of the transient rather than of the ring.
    const auto sustainRatio = [] (const std::vector<float>& samples)
    {
        return windowedRms (samples, 24000u, 48000u)
             / std::max (windowedRms (samples, 1440u, 4800u), 1.0e-12);
    };
    expect (sustainRatio (edgeMono) < sustainRatio (centreMono) * 0.6,
            "an edge stroke must die away sooner than a centre stroke");

    // The muted strokes must leave much less ringing behind them.
    const auto open = strike (parameters, taikor::Articulation::Don, 0, 0.9f, 48000.0, 48000);
    const auto muted = strike (parameters, taikor::Articulation::Tsu, 0, 0.9f, 48000.0, 48000);
    expect (decayTime (muted.mono(), 48000.0, -40.0)
                < decayTime (open.mono(), 48000.0, -40.0) * 0.7,
            "a damped stroke must be much shorter than an open one");

    // Stick on stick is not the drum at all, so it must leave no tail.
    const auto sticks = strike (parameters, taikor::Articulation::Bachi, 0, 0.9f,
                                48000.0, 48000);
    expect (decayTime (sticks.mono(), 48000.0, -40.0) < 0.12,
            "a stick-on-stick stroke must not ring like a drum");

    // The global strike-position control must move the vocabulary too.
    auto towardsRim = parameters;
    towardsRim.strikePosition = 1.0f;
    towardsRim.humanise = 0.0f;
    auto towardsCentre = parameters;
    towardsCentre.strikePosition = -1.0f;
    towardsCentre.humanise = 0.0f;

    const auto rimward = strike (towardsRim, taikor::Articulation::Ko, 0, 0.9f,
                                 48000.0, 24000);
    const auto centreward = strike (towardsCentre, taikor::Articulation::Ko, 0, 0.9f,
                                    48000.0, 24000);
    expect (upperIn (rimward.mono()) / std::max (fundamentalIn (rimward.mono()), 1.0e-9)
                > upperIn (centreward.mono())
                      / std::max (fundamentalIn (centreward.mono()), 1.0e-9),
            "moving the strike towards the rim must brighten it");
}

// The instrument is a stereo close pair, and the image has to come from the
// model rather than from a widener.
void testCloseMicrophonePair()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;

    // With the pair coincident over the centre of the head, the two
    // microphones are the same microphone and the output is exactly mono.
    auto coincident = parameters;
    coincident.micSpread = 0.0f;
    coincident.stereoWidth = 1.0f;
    const auto mono = strike (coincident, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                              24000);
    expect (maximumAbsoluteDifference (mono.left, mono.right) < 1.0e-6,
            "a coincident pair must produce identical channels");

    // Opening the pair must decorrelate it without ever putting it out of
    // phase: a drum recorded with a real close pair still sums to mono. The
    // relationship is not monotone and should not be asserted to be - each mode
    // reaches the two points with its own sign, so particular placements
    // recorrelate - but a wide pair must be clearly wider than a narrow one.
    double narrowCorrelation = 1.0;
    double widestCorrelation = 1.0;

    for (const float spread : { 0.25f, 0.55f, 0.85f, 1.0f })
    {
        auto spreadParameters = parameters;
        spreadParameters.micSpread = spread;
        const auto rendered = strike (spreadParameters, taikor::Articulation::Ka, 0,
                                      0.9f, 48000.0, 24000);
        const auto value = correlation (rendered.left, rendered.right);

        expect (value > 0.0,
                "the close pair must never go out of phase, at spread "
                    + std::to_string (spread));
        expect (value < 0.999,
                "an opened pair must not stay perfectly correlated, at spread "
                    + std::to_string (spread));

        if (spread <= 0.25f)
            narrowCorrelation = value;
        widestCorrelation = value;
    }

    expect (widestCorrelation < narrowCorrelation,
            "a fully opened pair must be wider than a nearly coincident one");

    // Backing the pair away from the head must narrow it, because the near
    // field that carries the membrane's shape decays with distance.
    auto near = parameters;
    near.micSpread = 0.8f;
    near.micDistance = 0.0f;
    auto far = parameters;
    far.micSpread = 0.8f;
    far.micDistance = 1.0f;

    const auto nearRendered = strike (near, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                      24000);
    const auto farRendered = strike (far, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                     24000);
    expect (correlation (farRendered.left, farRendered.right)
                > correlation (nearRendered.left, nearRendered.right),
            "backing the pair off the head must narrow the image");

    // Width at zero must fold the pair to mono without changing its sum.
    auto narrow = parameters;
    narrow.micSpread = 0.9f;
    narrow.stereoWidth = 0.0f;
    const auto folded = strike (narrow, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                12000);
    expect (maximumAbsoluteDifference (folded.left, folded.right) < 1.0e-6,
            "zero width must produce identical channels");

    // No stroke may invert, anywhere in the microphone range, at or below the
    // width the microphones actually captured. Above that the control is a
    // deliberate exaggeration and can go out of phase like any widener, so the
    // invariant is stated - and swept - only where it is meant to hold. The
    // earlier version of this check used a single microphone distance and so
    // said nothing about the endpoints, where the pair is at its widest.
    double worstCorrelation = 1.0;
    for (const float width : { 0.0f, 0.25f, 0.5f })
    {
        for (const float distance : { 0.0f, 0.5f, 1.0f })
        {
            for (const float spread : { 0.0f, 0.5f, 1.0f })
            {
                auto swept = parameters;
                swept.stereoWidth = width;
                swept.micDistance = distance;
                swept.micSpread = spread;

                for (std::size_t index = 0; index < taikor::articulationCount; ++index)
                {
                    const auto articulation =
                        static_cast<taikor::Articulation> (index);
                    const auto rendered =
                        strike (swept, articulation, 0, 0.9f, 48000.0, 24000);
                    const auto value =
                        correlation (rendered.left, rendered.right);
                    worstCorrelation = std::min (worstCorrelation, value);

                    expect (value > -0.05,
                            std::string (taikor::getArticulationDisplayName (articulation))
                                + " inverted at width "
                                + std::to_string (width) + ", distance "
                                + std::to_string (distance) + ", spread "
                                + std::to_string (spread));
                }
            }
        }
    }

    // The sweep has to actually reach the decorrelated corner, or it is only
    // testing the easy middle of the range.
    expect (worstCorrelation < 0.5,
            "the mono-compatibility sweep never reached a widely spaced pair");
}

void testTailsTerminateAndVoicesRetire()
{
    auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 1.0f);

    expect (engine.getActiveVoiceCount() > 0, "strokes did not allocate any voices");

    const auto rendered =
        render (engine, static_cast<int> (48000 * taikor::maximumTailSeconds + 48000));
    expect (rendered.finite, "a full tail produced non-finite audio");
    expect (engine.getActiveVoiceCount() == 0,
            "every voice must retire once its tail has run out");

    // With nothing sounding, the engine must be exactly silent rather than
    // dribbling denormals.
    const auto idle = render (engine, 24000);
    expect (idle.peak == 0.0, "an idle engine must produce exact silence");

    // Panic must stop everything immediately.
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 1.0f);
    render (engine, 480);
    engine.allSoundsOff();
    expect (engine.getActiveVoiceCount() == 0, "panic must free every voice");
    const auto afterPanic = render (engine, 4800);
    expect (afterPanic.peak == 0.0, "panic must leave exact silence");
}

void testVoiceStealingStaysBounded()
{
    auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    // Far more simultaneous strokes than the voice pool holds.
    for (int index = 0; index < 200; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index % taikor::articulationCount),
                        (index % 6) - 2, 0.9f);

    const auto rendered = render (engine, 48000);
    expect (rendered.finite, "voice stealing produced non-finite audio");
    expect (rendered.peak <= 1.0001, "voice stealing exceeded full scale");
    expect (engine.getActiveVoiceCount() <= 16,
            "the engine must not exceed its declared voice pool");
}

// What the instrument is supposed to sound like, stated as measurements.
//
// The complaint that produced these numbers was that the drum was "very tonal"
// and should be "heavy, large, epic". Both halves of that are measurable, and
// both had the same root: the reference drum was small, so its fundamental sat
// near 94 Hz where the ear hears a pitch rather than a weight, and the
// frequency-proportional loss in the hide let that one mode outlive every other
// by nearly four to one. What is left after half a second of that is a sine.
void testTheDrumSoundsLikeADrumAndNotLikeATone()
{
    const auto parameters = defaultParameters();
    const auto measurements = taikor::TaikoEngine::measure (parameters, 0);

    // Large. A taiko that reads as epic has its fundamental down where it is
    // felt; a 90-odd hertz drum is a floor tom.
    expect (measurements.loadedFundamentalHz > 40.0f
                && measurements.loadedFundamentalHz < 65.0f,
            "the reference drum's fundamental must sit in the register a large "
            "taiko occupies, not an octave above it");

    const auto rendered = strike (parameters, taikor::Articulation::Don, 0, 1.0f,
                                  48000.0, 144384);
    const auto mono = rendered.mono();

    // Heavy, and with a body. Both halves matter and they pull against each
    // other: a drum that is only weight is a sine, and one that is only body is
    // a snare. The proportions here are what third-octave analysis of recorded
    // taiko shows - a real one carries serious low end and is still nearly flat
    // from a couple of hundred hertz up to a kilohertz.
    double low = 0.0;
    double body = 0.0;
    double total = 0.0;
    for (double frequency = 25.0; frequency < 8000.0;
         frequency *= std::pow (2.0, 1.0 / 24.0))
    {
        const auto magnitude = binMagnitude (mono, frequency, 48000.0, 0u, 24000u);
        total += magnitude;
        if (frequency < 80.0)
            low += magnitude;
        // The region the resolved modal bank cannot reach on a large drum: its
        // highest Bessel zero lands a couple of hundred hertz up, and without
        // the head's continuum above that there is almost nothing here - four
        // per cent of the stroke against the forty a real one carries.
        if (frequency >= 250.0 && frequency < 4000.0)
            body += magnitude;
    }
    expect (total > 0.0 && low > total * 0.22,
            "a centre stroke must put real weight below eighty hertz");
    expect (total > 0.0 && body > total * 0.25,
            "a centre stroke must have a body between two hundred and fifty "
            "hertz and four kilohertz, not just a fundamental");

    // Not tonal. Count the twenty-fourth-octave bands that are within twenty
    // decibels of the strongest one over the body of the stroke. A drum whose
    // tail has collapsed onto a single partial scores in the low teens; a dense
    // one scores twice that.
    std::vector<double> bands;
    for (double frequency = 40.0; frequency < 4000.0;
         frequency *= std::pow (2.0, 1.0 / 24.0))
        bands.push_back (binMagnitude (mono, frequency, 48000.0, 4800u, 19200u));

    const auto strongest = *std::max_element (bands.begin(), bands.end());
    int within20dB = 0;
    for (const auto magnitude : bands)
        if (magnitude > strongest * 0.1)
            ++within20dB;

    expect (within20dB >= 18,
            "the body of a stroke must be a cluster of partials rather than one "
            "surviving mode");

    // And the mechanism behind that: no membrane mode may outlive the one above
    // it by more than about three to one. This is the frequency-proportional
    // loss in the hide being held in check by the rim, and it is what stops the
    // stroke decaying into its own fundamental.
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    engine.trigger (taikor::Articulation::Don, 0, 1.0f);

    const auto bank = taikor::TaikoEngineTestAccess::membraneT60s (engine);
    expect (bank.size() >= 2, "a centre stroke must build more than one mode");
    if (bank.size() >= 2)
        expect (bank[0] < bank[1] * 3.0f,
                "the lowest mode must not outlive the one above it so far that "
                "the stroke ends as a sine");
}

// Caching must be invisible. The engine keeps a resolved drum and a resolved
// pair of sticks per octave, and a parameter that feeds either of them but is
// missing from the invalidation list freezes it at whatever value happened to be
// set when the cache was first filled. That is how Bachi Hardness came to be
// ignored: before the sticks were cached it only fed the per-stroke contact
// solve, so its absence from the list was correct, and adding the cache made it
// wrong without touching the list.
//
// This sweeps every parameter rather than that one, so the next field to grow a
// cached dependency is caught by the same test.
void testEveryParameterSurvivesTheCache()
{
    struct Control
    {
        const char* name;
        void (*apply) (taikor::EngineParameters&, float);
    };
    static const std::array<Control, 21> controls {{
        { "Head Diameter",      [] (taikor::EngineParameters& p, float v) { p.headDiameter = 0.20f + 1.00f * v; } },
        { "Body Depth",         [] (taikor::EngineParameters& p, float v) { p.bodyDepth = v; } },
        { "Tension",            [] (taikor::EngineParameters& p, float v) { p.tension = v; } },
        { "Head Material",      [] (taikor::EngineParameters& p, float v) { p.headMaterial = v; } },
        { "Shell Material",     [] (taikor::EngineParameters& p, float v) { p.shellMaterial = v; } },
        { "Resonant Head",      [] (taikor::EngineParameters& p, float v) { p.resonantTension = v; } },
        { "Air Coupling",       [] (taikor::EngineParameters& p, float v) { p.cavityCoupling = v; } },
        { "Head Damping",       [] (taikor::EngineParameters& p, float v) { p.headDamping = v; } },
        { "Shell Resonance",    [] (taikor::EngineParameters& p, float v) { p.shellResonance = v; } },
        { "Pitch",              [] (taikor::EngineParameters& p, float v) { p.pitch = -24.0f + 48.0f * v; } },
        { "Bachi Hardness",     [] (taikor::EngineParameters& p, float v) { p.bachiHardness = v; } },
        { "Strike Position",    [] (taikor::EngineParameters& p, float v) { p.strikePosition = -1.0f + 2.0f * v; } },
        { "Velocity Depth",     [] (taikor::EngineParameters& p, float v) { p.velocityDepth = v; } },
        { "Tension Modulation", [] (taikor::EngineParameters& p, float v) { p.tensionModulation = v; } },
        { "Strike Noise",       [] (taikor::EngineParameters& p, float v) { p.strikeNoise = v; } },
        { "Octave Body",        [] (taikor::EngineParameters& p, float v) { p.octaveBody = v; } },
        { "Mic Distance",       [] (taikor::EngineParameters& p, float v) { p.micDistance = v; } },
        { "Mic Spread",         [] (taikor::EngineParameters& p, float v) { p.micSpread = v; } },
        { "Stereo Width",       [] (taikor::EngineParameters& p, float v) { p.stereoWidth = v; } },
        { "Drive",              [] (taikor::EngineParameters& p, float v) { p.drive = v; } },
        { "Output",             [] (taikor::EngineParameters& p, float v) { p.outputGain = 0.1f + 0.9f * v; } },
    }};

    // Humanisation is seeded from the stroke's own index, so a reused engine's
    // second stroke would differ from a fresh engine's first for a reason that
    // has nothing to do with caching. Turning it off is what makes the two
    // comparable at all.
    const auto base = [] {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        return parameters;
    }();

    for (const auto& control : controls)
        for (const auto articulation : { taikor::Articulation::Don,
                                         taikor::Articulation::Katsu,
                                         taikor::Articulation::Bachi })
        {
            auto before = base;
            auto after = base;
            control.apply (before, 0.15f);
            control.apply (after, 0.85f);

            // A fresh engine that has only ever seen the new value.
            taikor::TaikoEngine fresh;
            fresh.setParameters (after);
            fresh.prepare (48000.0, defaultBlockSize);
            fresh.trigger (articulation, 0, 0.85f);
            const auto expected = taikor::TaikoEngineTestAccess::newestVoiceBank (fresh);

            // One that was used at the old value first, exactly as a player
            // would: strike, turn the control, strike again.
            taikor::TaikoEngine reused;
            reused.setParameters (before);
            reused.prepare (48000.0, defaultBlockSize);
            reused.trigger (articulation, 0, 0.85f);
            render (reused, 2400);
            reused.setParameters (after);
            reused.trigger (articulation, 0, 0.85f);
            const auto actual = taikor::TaikoEngineTestAccess::newestVoiceBank (reused);

            const std::string where = std::string (" (") + control.name + ", "
                                    + std::string (taikor::getArticulationDisplayName (articulation))
                                    + ")";

            expect (actual.size() == expected.size(),
                    "turning a control changed how many modes a stroke builds "
                    "depending on what came before it" + where);
            if (actual.size() != expected.size())
                continue;

            bool identical = true;
            for (std::size_t mode = 0; mode < actual.size() && identical; ++mode)
                for (std::size_t term = 0; term < actual[mode].size(); ++term)
                    if (std::abs (actual[mode][term] - expected[mode][term])
                        > 1.0e-4f * std::max (1.0f, std::abs (expected[mode][term])))
                    {
                        identical = false;
                        break;
                    }

            expect (identical,
                    "a stroke after this control moved does not match the same "
                    "stroke on an engine that always had the new value - a cache "
                    "is holding a stale value" + where);
        }
}

// The top membrane modes carry a circumferential order of eight, so the
// radiation law's exponent is eighteen, and forming (ka)^18 on its own leaves
// float range once ka passes about 138. That is reachable on a small, hard,
// high-tension head two octaves up at a high sample rate, and it produced
// inf/inf: a NaN decay rate, which the lifetime pass then read as zero samples
// and quietly retired seven high partials out of an edge stroke.
void testRadiationEfficiencyStaysFinite()
{
    for (int order = 0; order <= 8; ++order)
    {
        float previous = -1.0f;
        for (float ka : { 1.0e-6f, 1.0e-3f, 0.1f, 1.0f, 10.0f, 100.0f, 138.0f, 260.0f,
                          1.0e4f, 1.0e8f, 1.0e20f, 1.0e30f })
        {
            const auto efficiency =
                taikor::TaikoEngineTestAccess::radiationEfficiency (order, ka);
            const std::string where = " (order " + std::to_string (order) + ", ka "
                                    + std::to_string (ka) + ")";

            expect (std::isfinite (efficiency),
                    "radiation efficiency must stay finite" + where);
            expect (efficiency >= 0.0f && efficiency <= 1.0f,
                    "radiation efficiency must stay a fraction" + where);
            expect (efficiency >= previous - 1.0e-6f,
                    "radiation efficiency must rise with ka" + where);
            previous = efficiency;
        }

        expect (taikor::TaikoEngineTestAccess::radiationEfficiency (order, 1.0e20f)
                    > 0.99f,
                "a very large ka must saturate the efficiency, not annihilate it "
                "(order " + std::to_string (order) + ")");
    }

    // And the whole way through the engine, at the extremes that reach it: the
    // smallest, hardest, most sharply tuned head this instrument can describe,
    // at every supported sample rate.
    for (const double rate : { 8000.0, 48000.0, 96000.0, 192000.0, 384000.0 })
    {
        auto parameters = defaultParameters();
        parameters.tension = 1.0f;
        parameters.headMaterial = 0.0f;
        parameters.pitch = 24.0f;
        parameters.octaveBody = 0.0f;
        parameters.headDiameter = 0.20f;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (rate, defaultBlockSize);

        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
            engine.trigger (static_cast<taikor::Articulation> (index),
                            taikor::highestOctaveOffset, 1.0f);

        expect (taikor::TaikoEngineTestAccess::everyDecayIsFinite (engine),
                "an extreme drum produced a non-finite mode at "
                + std::to_string (static_cast<int> (rate)) + " Hz");

        const auto rendered = render (engine, 4800);
        expect (rendered.finite,
                "an extreme drum produced non-finite audio at "
                + std::to_string (static_cast<int> (rate)) + " Hz");
    }
}

// The editor's readout has to name modes the drum actually sounds. With Air
// Coupling at zero the two heads are independent, and one branch belongs
// entirely to the far head - a mode a stroke on the batter head never touches.
// Reporting the lower eigenvalue regardless named that silent mode the
// fundamental: at Resonant Head zero the readout said 88.5 Hz while the drum
// sounded 92.9 Hz, and it named the batter head's own mode the breathing mode,
// which on an open body does not exist at all.
//
// The test is presence, not dominance. The fundamental is the lowest mode the
// stroke drives, which is not always the loudest survivor of a two-second tail:
// under weak coupling the volume-changing branch is driven harder while the
// other one outlives it. Asserting dominance would be asserting something the
// model never claimed.
void testReportedModesAreActuallySounded()
{
    for (float cavity : { 0.0f, 0.02f, 0.05f, 0.10f, 0.20f, 0.35f, 0.85f, 1.0f })
        for (float resonant : { 0.0f, 0.25f, 0.5f, 1.0f })
        {
            auto parameters = defaultParameters();
            parameters.cavityCoupling = cavity;
            parameters.resonantTension = resonant;

            const auto measurements = taikor::TaikoEngine::measure (parameters, 0);
            // A second and a half of tail: long enough to resolve the pair,
            // which is four hertz apart at its closest, without spending the
            // whole suite's runtime on Goertzel sweeps.
            const auto rendered = strike (parameters, taikor::Articulation::Don, 0, 0.9f,
                                          48000.0, 81920);
            const auto mono = rendered.mono();
            // Past the attack, so this is the ringing head rather than the
            // broadband contact.
            constexpr std::size_t afterAttack = 9600;

            double bandPeak = 0.0;
            for (double frequency = 50.0; frequency < 320.0; frequency += 1.0)
                bandPeak = std::max (bandPeak,
                                     binMagnitude (mono, frequency, 48000.0, afterAttack));

            const std::string where = " (cavity " + std::to_string (cavity)
                                    + ", resonant " + std::to_string (resonant) + ")";
            expect (bandPeak > 0.0, "the drum must sound at all" + where);
            if (! (bandPeak > 0.0))
                continue;

            const auto atFundamental =
                binMagnitude (mono, measurements.loadedFundamentalHz, 48000.0, afterAttack)
                / bandPeak;
            const auto atBreathing =
                binMagnitude (mono, measurements.breathingModeHz, 48000.0, afterAttack)
                / bandPeak;

            expect (atFundamental > 0.15,
                    "the reported fundamental must be a mode the stroke drives"
                    + where);
            expect (atBreathing > 0.004,
                    "the reported breathing mode must be a mode the stroke drives"
                    + where);

            // On an open body only one branch sounds at all, so there the
            // strongest partial is unambiguously the fundamental and the check
            // can be exact. This is the reviewer's case: the readout used to say
            // 88.5 Hz where the drum plainly sounded 92.9 Hz. Under partial
            // coupling both branches are genuinely driven and the louder one is
            // not always the lower, so dominance is only the right question
            // once the cavity is out of the picture.
            if (cavity <= 0.0f)
            {
                const auto dominant = dominantFrequency (mono, 48000.0, 50.0, 320.0,
                                                         0.25, afterAttack);
                expect (std::abs (dominant - measurements.loadedFundamentalHz) < 1.0,
                        "with no cavity the reported fundamental must be the "
                        "partial the drum sounds" + where);
            }

            expect (measurements.breathingModeHz >= measurements.loadedFundamentalHz,
                    "the breathing mode must never be reported below the fundamental"
                    + where);
            expect (measurements.tailSeconds > 0.0f,
                    "the reported tail must describe a mode that sounds" + where);

            // An uncoupled body has no split to report, so both figures must be
            // the single mode it has. A sealed one must still show the split.
            if (cavity <= 0.0f)
                expect (std::abs (measurements.breathingModeHz
                                  - measurements.loadedFundamentalHz) < 0.01f,
                        "an uncoupled body must not report a breathing mode" + where);
            else if (cavity >= 0.85f)
                expect (measurements.breathingModeHz
                            > measurements.loadedFundamentalHz * 1.2f,
                        "a sealed body must still report the cavity split" + where);
        }
}

// The stick-on-stick stroke is two pieces of wood that never touch the drum, so
// nothing about the drum may reach it. It used to build its bank by stretching
// the shell's ring modes by a constant, which meant Shell Material moved the
// click by two octaves, Head Diameter by an octave and a half, and Head Tension
// changed its level by five decibels - all from an object the stick never met.
void testStickStrokeIsIndependentOfTheDrum()
{
    const auto bankFor = [] (const taikor::EngineParameters& parameters, int octave)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Bachi, octave, 0.85f);
        return std::pair { taikor::TaikoEngineTestAccess::woodFrequencies (engine),
                           taikor::TaikoEngineTestAccess::woodDecays (engine) };
    };

    const auto reference = bankFor (defaultParameters(), 0);
    expect (reference.first.size() >= 4,
            "the stick must present a bank to compare against");

    // Every control that describes the drum rather than the stick, at both ends
    // of its range.
    struct DrumControl
    {
        const char* name;
        void (*apply) (taikor::EngineParameters&, float);
    };
    static const std::array<DrumControl, 7> controls {{
        { "Head Diameter",   [] (taikor::EngineParameters& p, float v) { p.headDiameter = 0.20f + 1.00f * v; } },
        { "Body Depth",      [] (taikor::EngineParameters& p, float v) { p.bodyDepth = v; } },
        { "Head Tension",    [] (taikor::EngineParameters& p, float v) { p.tension = v; } },
        { "Head Material",   [] (taikor::EngineParameters& p, float v) { p.headMaterial = v; } },
        { "Shell Material",  [] (taikor::EngineParameters& p, float v) { p.shellMaterial = v; } },
        { "Shell Resonance", [] (taikor::EngineParameters& p, float v) { p.shellResonance = v; } },
        { "Cavity Coupling", [] (taikor::EngineParameters& p, float v) { p.cavityCoupling = v; } },
    }};

    for (const auto& control : controls)
        for (float value : { 0.0f, 1.0f })
        {
            auto parameters = defaultParameters();
            control.apply (parameters, value);
            const auto bank = bankFor (parameters, 0);

            expect (bank.first.size() == reference.first.size(),
                    std::string ("the stick's bank changed size with ") + control.name);
            if (bank.first.size() != reference.first.size())
                continue;

            for (std::size_t index = 0; index < bank.first.size(); ++index)
            {
                expect (std::abs (bank.first[index] - reference.first[index]) < 0.01f,
                        std::string ("the stick's pitch followed ") + control.name);
                expect (std::abs (bank.second[index] - reference.second[index]) < 0.01f,
                        std::string ("the stick's decay followed ") + control.name);
            }
        }

    // The contact is the other route the drum reached the stick through: the
    // solver used to floor the contact time on the head's own impedance even
    // for a stroke that never lands on the head.
    const auto referenceContact = taikor::TaikoEngineTestAccess::contactSecondsFor (
        defaultParameters(), taikor::Articulation::Bachi, 0, 0.85f);
    for (const auto& control : controls)
        for (float value : { 0.0f, 1.0f })
        {
            auto parameters = defaultParameters();
            control.apply (parameters, value);
            const auto contact = taikor::TaikoEngineTestAccess::contactSecondsFor (
                parameters, taikor::Articulation::Bachi, 0, 0.85f);
            expect (std::abs (contact - referenceContact) < 1.0e-9f,
                    std::string ("the stick's contact time followed ") + control.name);
        }

    // A drum stroke must still hear all of it, or the test above would pass on
    // an engine that had simply stopped responding to the controls.
    const auto drumReference = [&]
    {
        taikor::TaikoEngine engine;
        engine.setParameters (defaultParameters());
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Katsu, 0, 0.85f);
        return taikor::TaikoEngineTestAccess::woodFrequencies (engine);
    }();
    auto shifted = defaultParameters();
    shifted.shellMaterial = 0.0f;
    taikor::TaikoEngine drumEngine;
    drumEngine.setParameters (shifted);
    drumEngine.prepare (48000.0, defaultBlockSize);
    drumEngine.trigger (taikor::Articulation::Katsu, 0, 0.85f);
    const auto drumShifted = taikor::TaikoEngineTestAccess::woodFrequencies (drumEngine);
    expect (! drumReference.empty() && ! drumShifted.empty()
                && std::abs (drumShifted[0] - drumReference[0]) > 1.0f,
            "Shell Material must still retune a stroke on the drum's own body");

    // And the octave must raise the stick's pitch, monotonically. Stretching
    // the shell's modes used to push them past Nyquist, which dropped the top
    // of the bank and made the click fall in pitch over the last two octaves.
    float previous = 0.0f;
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto bank = bankFor (defaultParameters(), octave);
        expect (! bank.first.empty(),
                "every octave of the stick stroke must sound something");
        if (bank.first.empty())
            continue;

        expect (bank.first[0] > previous * 1.5f,
                "each octave of the stick stroke must be higher than the last");
        previous = bank.first[0];
    }
}

// Sixteen voices, and several strokes can land on the same sample. A voice that
// has been triggered but not yet rendered still reads as silent, so choosing
// the quietest voice by level alone sent every stroke in a block to one slot -
// and a chord, a flam or a roll on a buffer boundary sounded as a single note.
void testSimultaneousStrokesDoNotShareOneVoice()
{
    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    // Fill the pool and let it render, so every voice has a real level.
    for (int index = 0; index < 16; ++index)
        engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    render (engine, 4800);
    expect (taikor::TaikoEngineTestAccess::activeVoices (engine) == 16,
            "the pool must be full before the stealing case is exercised");

    // Now six more strokes at the same sample, with nothing rendered between
    // them. Each must displace a different voice.
    std::set<int> slots;
    for (int index = 0; index < 6; ++index)
    {
        engine.trigger (taikor::Articulation::Ka, 0, 0.9f);
        const auto order = taikor::TaikoEngineTestAccess::strokeCount (engine);
        const int slot = taikor::TaikoEngineTestAccess::voiceSlotOf (engine, order);
        expect (slot >= 0, "a triggered stroke must be findable in the pool");
        if (slot >= 0)
            slots.insert (slot);
    }

    expect (slots.size() == 6,
            "six simultaneous strokes must occupy six voices, not overwrite one");

    const auto rendered = render (engine, 24000);
    expect (rendered.finite, "simultaneous stealing produced non-finite audio");
}

void testDeterminismAndBlockPartitioning()
{
    const auto parameters = defaultParameters();

    const auto first = strike (parameters, taikor::Articulation::Don, 0, 0.83f, 48000.0,
                               24000, 64);
    const auto second = strike (parameters, taikor::Articulation::Don, 0, 0.83f, 48000.0,
                                24000, 64);
    expect (maximumAbsoluteDifference (first.left, second.left) == 0.0,
            "the engine must be bit-exactly deterministic");
    expect (maximumAbsoluteDifference (first.right, second.right) == 0.0,
            "the engine must be bit-exactly deterministic on both channels");

    // The same stroke must render identically however the host cuts the block.
    for (const int blockSize : { 1, 7, 64, 129, 512, 2048 })
    {
        const auto partitioned = strike (parameters, taikor::Articulation::Don, 0, 0.83f,
                                         48000.0, 24000, blockSize);
        expect (maximumAbsoluteDifference (first.left, partitioned.left) < 1.0e-6,
                "block partitioning changed the rendered audio at block size "
                    + std::to_string (blockSize));
    }

    // Humanising must vary successive strokes, and switching it off must make
    // them identical again.
    auto humanised = parameters;
    humanised.humanise = 1.0f;

    taikor::TaikoEngine engine;
    engine.setParameters (humanised);
    engine.prepare (48000.0, defaultBlockSize);
    engine.trigger (taikor::Articulation::Don, 0, 0.8f);
    const auto strokeA = render (engine, 12000);
    engine.allSoundsOff();
    engine.trigger (taikor::Articulation::Don, 0, 0.8f);
    const auto strokeB = render (engine, 12000);
    expect (maximumAbsoluteDifference (strokeA.left, strokeB.left) > 1.0e-4,
            "humanising must make successive strokes differ");

    auto machine = parameters;
    machine.humanise = 0.0f;
    taikor::TaikoEngine tight;
    tight.setParameters (machine);
    tight.prepare (48000.0, defaultBlockSize);
    tight.trigger (taikor::Articulation::Don, 0, 0.8f);
    const auto tightA = render (tight, 12000);
    tight.allSoundsOff();
    tight.trigger (taikor::Articulation::Don, 0, 0.8f);
    const auto tightB = render (tight, 12000);
    expect (maximumAbsoluteDifference (tightA.left, tightB.left) < 1.0e-6,
            "with humanising off, successive strokes must be identical");
}

void testPerformanceControls()
{
    const auto parameters = defaultParameters();

    // A hand on the head must shorten what is already ringing.
    // Both takes are rendered over the same window from the same instant, or
    // the comparison is between an open take that includes its attack and a
    // damped one that does not - which would pass whether the damping worked
    // or not.
    taikor::TaikoEngine open;
    open.setParameters (parameters);
    open.prepare (48000.0, defaultBlockSize);
    open.trigger (taikor::Articulation::Don, 0, 0.95f);
    const auto openTail = render (open, 48000 * 3).mono();

    taikor::TaikoEngine damped;
    damped.setParameters (parameters);
    damped.prepare (48000.0, defaultBlockSize);
    damped.trigger (taikor::Articulation::Don, 0, 0.95f);
    render (damped, 4800);
    damped.setHandDamping (1.0f);
    const auto dampedTail = render (damped, 48000 * 3 - 4800).mono();

    // Compared over the same stretch of the tail, well after the hand has
    // landed and long before either take has run out.
    expect (windowedRms (dampedTail, 24000u, 96000u)
                < windowedRms (openTail, 28800u, 100800u) * 0.2,
            "a hand on the head must damp what is still ringing");

    // The wheel must raise the pitch, because pressing a head tightens it. It
    // glides rather than jumping, so the engine has to be run for the strings
    // of the smoother to arrive.
    taikor::TaikoEngine bent;
    bent.setParameters (parameters);
    bent.prepare (48000.0, defaultBlockSize);
    bent.setPitchBend (1.0f);
    render (bent, 24000);
    const auto bentMeasurements = bent.measureDrum (0);

    taikor::TaikoEngine plain;
    plain.setParameters (parameters);
    plain.prepare (48000.0, defaultBlockSize);
    const auto plainMeasurements = plain.measureDrum (0);

    expect (bentMeasurements.idealFundamentalHz > plainMeasurements.idealFundamentalHz,
            "the wheel must raise the drum's pitch");
    expect (bentMeasurements.idealFundamentalHz
                < plainMeasurements.idealFundamentalHz * 1.3f,
            "the wheel's range must stay within a couple of semitones");

    // Drive at zero must be exactly bypassed.
    auto clean = parameters;
    clean.drive = 0.0f;
    auto driven = parameters;
    driven.drive = 1.0f;

    const auto cleanRendered = strike (clean, taikor::Articulation::Don, 0, 0.95f,
                                       48000.0, 12000);
    const auto drivenRendered = strike (driven, taikor::Articulation::Don, 0, 0.95f,
                                        48000.0, 12000);
    expect (maximumAbsoluteDifference (cleanRendered.left, drivenRendered.left) > 1.0e-4,
            "the drive control must change the output");
    expect (drivenRendered.finite && drivenRendered.peak <= 1.0001,
            "the drive stage must stay bounded");

    // Output gain must scale the result predictably.
    // Half the default rather than an absolute figure, so the check stays a
    // statement about the control and not about what the default happens to be.
    auto quiet = parameters;
    quiet.outputGain = parameters.outputGain * 0.5f;
    const auto quietRendered = strike (quiet, taikor::Articulation::Don, 0, 0.95f,
                                       48000.0, 12000);
    const auto ratio = quietRendered.peak / std::max (cleanRendered.peak, 1.0e-9);
    expect (ratio > 0.4 && ratio < 0.6, "output gain must scale the result linearly");
}

void testInvalidInputSafety()
{
    taikor::TaikoEngine engine;
    engine.prepare (48000.0, defaultBlockSize);

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();

    taikor::EngineParameters hostile;
    hostile.headDiameter = nan;
    hostile.bodyDepth = infinity;
    hostile.tension = -5.0f;
    hostile.headMaterial = 1.0e9f;
    hostile.shellMaterial = -infinity;
    hostile.resonantTension = nan;
    hostile.cavityCoupling = 12.0f;
    hostile.headDamping = -1.0f;
    hostile.shellResonance = nan;
    hostile.pitch = 1.0e9f;
    hostile.bachiHardness = nan;
    hostile.strikePosition = -20.0f;
    hostile.velocityDepth = infinity;
    hostile.tensionModulation = nan;
    hostile.strikeNoise = -3.0f;
    hostile.humanise = nan;
    hostile.octaveBody = 40.0f;
    hostile.micDistance = nan;
    hostile.micSpread = -2.0f;
    hostile.stereoWidth = infinity;
    hostile.drive = nan;
    hostile.outputGain = 1.0e6f;
    engine.setParameters (hostile);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 0.9f);

    const auto rendered = render (engine, 24000);
    expect (rendered.finite, "hostile parameters produced non-finite audio");
    expect (rendered.peak <= 1.0001, "hostile parameters exceeded full scale");

    // Invalid strokes must simply do nothing.
    engine.allSoundsOff();
    engine.setParameters (defaultParameters());
    engine.trigger (taikor::Articulation::Don, 0, nan);
    engine.trigger (taikor::Articulation::Don, 0, -1.0f);
    engine.trigger (taikor::Articulation::Don, 0, 0.0f);
    engine.trigger (static_cast<taikor::Articulation> (99), 0, 0.9f);
    expect (engine.getActiveVoiceCount() == 0,
            "an invalid stroke must not allocate a voice");

    // Out-of-range octaves must be clamped rather than read out of bounds.
    engine.trigger (taikor::Articulation::Don, -50, 0.9f);
    engine.trigger (taikor::Articulation::Don, 50, 0.9f);
    const auto clamped = render (engine, 12000);
    expect (clamped.finite, "an out-of-range octave produced non-finite audio");

    // A null or empty buffer must be ignored rather than crash.
    engine.process (nullptr, nullptr, 64);
    std::vector<float> left (16), right (16);
    engine.process (left.data(), right.data(), 0);
    engine.process (left.data(), right.data(), -5);
}

void testUiPresentationMath()
{
    using namespace taikor::ui;

    expect (std::abs (onePoleCoefficient (0.0f, 30.0f) - 1.0f) < 1.0e-6f,
            "a zero time constant must be an immediate jump");
    expect (onePoleCoefficient (1.0f, 0.0f) == 1.0f,
            "a zero update rate must not divide by zero");
    const auto coefficient = onePoleCoefficient (0.1f, 30.0f);
    expect (coefficient > 0.0f && coefficient < 1.0f,
            "a smoothing coefficient must stay inside the unit interval");

    expect (decayMultiplier (-12.0f, 1.0f, 30.0f) < 1.0f,
            "a decay multiplier must be less than one");
    expect (decayMultiplier (-12.0f, 0.0f, 30.0f) == 0.0f,
            "a zero decay time must be handled");

    expect (std::abs (meterPositionForLinear (1.0f, -48.0f) - 1.0f) < 1.0e-5f,
            "full scale must sit at the top of the meter");
    expect (meterPositionForLinear (0.0f, -48.0f) == 0.0f,
            "silence must sit at the bottom of the meter");
    for (const float position : { 0.1f, 0.35f, 0.7f, 1.0f })
    {
        const auto linear = linearForMeterPosition (position, -48.0f);
        expect (std::abs (meterPositionForLinear (linear, -48.0f) - position) < 1.0e-4f,
                "the meter scale must round-trip");
    }

    MeterBallistics ballistics;
    ballistics.reset();
    for (int index = 0; index < 60; ++index)
        ballistics.update (0.8f, 0.5f, 0.05f, 0.9f, 10.0f);
    expect (ballistics.level > 0.7f, "the meter must reach a sustained level");
    expect (ballistics.peak >= ballistics.level, "the peak marker must lead the level");

    const auto beforeRelease = ballistics.level;
    for (int index = 0; index < 30; ++index)
        ballistics.update (0.0f, 0.5f, 0.05f, 0.9f, 10.0f);
    expect (ballistics.level < beforeRelease, "the meter must fall back");

    const auto layout = rowLayout (600, 12, 6, 12);
    expect (layout.cellSize > 0, "a row layout must produce a usable cell size");
    expect (cellOffset (layout, 6, 0) == layout.origin,
            "the first cell must sit at the row origin");
    expect (cellOffset (layout, 6, 11) + layout.cellSize <= 600,
            "the last cell must fit inside the row");
    expect (rowLayout (0, 12, 6, 12).cellSize == 1,
            "a degenerate row must not divide by zero");
    expect (rowLayout (600, 0, 6, 0).cellSize == 1,
            "a row with no columns must not divide by zero");

    // A short row stays centred under a longer one.
    const auto shortRow = rowLayout (600, 12, 6, 5);
    expect (shortRow.origin > layout.origin,
            "a partly filled row must be centred");

    const auto centre = headPointFor (0.0f, 1.2f);
    expect (std::abs (centre.x) < 1.0e-6f && std::abs (centre.y) < 1.0e-6f,
            "a centre strike must map to the middle of the head");
    const auto rim = headPointFor (1.0f, 0.0f);
    expect (std::abs (rim.x - 1.0f) < 1.0e-6f,
            "a rim strike at zero radians must sit at the right of the head");
    const auto clampedPoint = headPointFor (5.0f, 0.0f);
    expect (std::abs (clampedPoint.x - 1.0f) < 1.0e-6f,
            "a head point must be clamped to the head");

    expect (std::abs (semitonesBetween (880.0f, 440.0f) - 12.0f) < 1.0e-4f,
            "an octave must measure twelve semitones");
    expect (semitonesBetween (0.0f, 440.0f) == 0.0f,
            "an invalid frequency must not produce a logarithm of zero");

    expect (std::abs (mix (0.0f, 10.0f, 0.25f) - 2.5f) < 1.0e-6f, "mix is wrong");
    expect (smoothStep (0.0f, 1.0f, -1.0f) == 0.0f, "smoothStep must clamp low");
    expect (smoothStep (0.0f, 1.0f, 2.0f) == 1.0f, "smoothStep must clamp high");
    expect (smoothStep (1.0f, 1.0f, 2.0f) == 1.0f, "smoothStep must handle a zero span");
    expect (clamp (std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f) == 0.0f,
            "clamp must reject NaN");
}

void testIdleCostAndStressPerformance()
{
    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    // An idle drum must cost almost nothing: a track is silent most of the time.
    const auto idleStart = std::chrono::steady_clock::now();
    render (engine, 48000 * 10);
    const double idleSeconds =
        std::chrono::duration<double> (std::chrono::steady_clock::now() - idleStart)
            .count();
    expect (idleSeconds < 2.0, "an idle engine cost far more than it should");

    // A dense roll across every stroke and every octave.
    std::vector<float> left (static_cast<std::size_t> (defaultBlockSize));
    std::vector<float> right (static_cast<std::size_t> (defaultBlockSize));
    bool finite = true;
    double peak = 0.0;

    const auto start = std::chrono::steady_clock::now();
    const int totalSamples = 48000 * 2;
    for (int rendered = 0, stroke = 0; rendered < totalSamples; ++stroke)
    {
        engine.trigger (
            static_cast<taikor::Articulation> (stroke % taikor::articulationCount),
            (stroke % 6) - 2, 0.4f + 0.06f * static_cast<float> (stroke % 10));

        const int count = std::min (defaultBlockSize, totalSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const float l = left[static_cast<std::size_t> (sample)];
            const float r = right[static_cast<std::size_t> (sample)];
            finite = finite && std::isfinite (l) && std::isfinite (r);
            peak = std::max ({ peak, std::abs (static_cast<double> (l)),
                               std::abs (static_cast<double> (r)) });
        }
        rendered += count;
    }
    const double elapsed =
        std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

    expect (finite && peak <= 1.0001, "the dense stress render produced unsafe audio");
    expect (elapsed < 20.0,
            "a two-second dense render exceeded the generous performance guardrail");
}

// Regressions for control endpoints and performance gestures. Each of these
// was a real defect: they are checked here because every one of them is
// invisible to a test that only looks at the solved drum, and audible to a
// player immediately.
void testControlEndpointsAndGestures()
{
    const auto parameters = defaultParameters();

    // Air Coupling at exactly zero must not be a cliff. The axisymmetric modes
    // are solved as a two-by-two eigenproblem, and at zero coupling that system
    // becomes degenerate; resolving the degeneracy by branch index rather than
    // by which head the eigenvalue belongs to handed both branches to the
    // resonant head and silenced the drum's boom at the endpoint.
    const auto bodyEnergy = [&parameters] (float coupling)
    {
        auto tuned = parameters;
        tuned.cavityCoupling = coupling;
        tuned.humanise = 0.0f;
        const auto rendered =
            strike (tuned, taikor::Articulation::Don, 0, 0.9f, 48000.0, 48000);
        const auto mono = rendered.mono();
        double sum = 0.0;
        for (std::size_t index = 4800; index < 24000 && index < mono.size(); ++index)
            sum += static_cast<double> (mono[index]) * mono[index];
        return std::sqrt (sum / 19200.0);
    };

    const auto sealedBody = bodyEnergy (0.85f);
    const auto openBody = bodyEnergy (0.0f);
    const auto barelyCoupled = bodyEnergy (0.001f);

    expect (openBody > sealedBody * 0.5,
            "an uncoupled body must still have a centre boom");
    expect (std::abs (openBody - barelyCoupled) < barelyCoupled * 0.10,
            "the Air Coupling control must be continuous at zero");

    // The wheel presses the head, so a stroke that is already ringing has to
    // bend with it - not merely the strokes that follow it.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);

        const auto before = render (engine, 24000).mono();
        engine.setPitchBend (1.0f);
        const auto after = render (engine, 72000).mono();

        // The search band follows the drum the engine reports rather than a
        // range written for one particular default, so retuning the instrument
        // cannot silently move the measurement off the partial it is watching.
        const auto resting = engine.measureDrum (0).loadedFundamentalHz;
        const auto low = static_cast<double> (resting) * 0.8;
        const auto high = static_cast<double> (resting) * 1.6;

        const auto restingPitch =
            dominantFrequency (before, 48000.0, low, high, 0.05, 2400u);
        // Skip the glide itself and measure where the drum settled.
        const auto bentPitch =
            dominantFrequency (after, 48000.0, low, high, 0.05, 16000u);
        const auto semitones = 12.0 * std::log2 (bentPitch / restingPitch);

        expect (semitones > 1.5 && semitones < 2.5,
                "a fully raised wheel must bend a ringing stroke by about two semitones");
    }

    // The attack glide stretches the head. It must not stretch the wooden body
    // the head is nailed to: a stick-on-stick stroke drives no membrane mode at
    // all, so Tension Mod has nothing it could legitimately change there.
    {
        auto without = parameters;
        without.humanise = 0.0f;
        without.tensionModulation = 0.0f;
        auto with = without;
        with.tensionModulation = 1.0f;

        // A shell strike does run the glide - its depth is scaled by the
        // stroke's own membrane gain, which is small but not zero - so what has
        // to be true is that the glide moves only that small membrane share and
        // leaves the wooden bank, which carries most of the stroke, alone.
        const auto shellQuiet =
            strike (without, taikor::Articulation::Katsu, 0, 0.95f, 48000.0, 24000);
        const auto shellGlided =
            strike (with, taikor::Articulation::Katsu, 0, 0.95f, 48000.0, 24000);
        const auto shellChange =
            maximumAbsoluteDifference (shellQuiet.left, shellGlided.left);
        expect (shellChange < 1.0e-3,
                "the tension glide must not retune the wooden shell");

        // And it must still do its job on the head, which is many times larger
        // than anything the shell stroke is allowed to move by.
        const auto openWithout =
            strike (without, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto openWith =
            strike (with, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto headChange =
            maximumAbsoluteDifference (openWithout.left, openWith.left);
        expect (headChange > 1.0e-2, "the tension glide must still bend the head");
        expect (headChange > shellChange * 20.0,
                "the glide must move the head far more than the body");
    }

    // Width multiplies the side signal, so automating it must not step the
    // audio at a block boundary. Measured as the jump across the exact sample
    // where the control changed, against the steps the signal is making anyway
    // just after it: a smoothed control disappears into the signal, an
    // unsmoothed one stands several times above it.
    {
        const auto boundaryAgainstSignal = [&parameters] (bool slam)
        {
            auto tuned = parameters;
            tuned.humanise = 0.0f;
            tuned.micSpread = 1.0f;
            tuned.stereoWidth = 0.0f;

            constexpr int block = 256;
            constexpr int slamBlock = 3;

            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (48000.0, block);
            engine.trigger (taikor::Articulation::Ka, 0, 0.95f);

            std::vector<float> left (static_cast<std::size_t> (block));
            std::vector<float> right (static_cast<std::size_t> (block));
            std::vector<float> history;

            for (int index = 0; index < slamBlock + 4; ++index)
            {
                if (slam && index == slamBlock)
                {
                    auto opened = tuned;
                    opened.stereoWidth = 1.0f;
                    engine.setParameters (opened);
                }
                engine.process (left.data(), right.data(), block);
                history.insert (history.end(), left.begin(), left.end());
            }

            const auto at = static_cast<std::size_t> (slamBlock * block);
            const auto boundary =
                std::abs (static_cast<double> (history[at]) - history[at - 1]);

            double typical = 0.0;
            for (std::size_t index = at + 8; index < at + 200; ++index)
                typical = std::max (typical,
                                    std::abs (static_cast<double> (history[index])
                                              - history[index - 1]));

            return boundary / std::max (typical, 1.0e-9);
        };

        expect (boundaryAgainstSignal (true) < 2.0,
                "automating the width stepped the audio at the block boundary");
        expect (boundaryAgainstSignal (false) < 2.0,
                "the width step measurement is picking up ordinary signal content");
    }

    // A stroke struck after a bend has settled must be in tune with the bend.
    // The cached drums the render path builds strokes from were invalidated on
    // the wheel's per-sample increment, which at high sample rates falls below
    // any sensible epsilon long before the glide has actually arrived - leaving
    // the cache, and every new stroke, flat.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        constexpr double highRate = 384000.0;
        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (highRate, 512);

        engine.setPitchBend (1.0f);
        render (engine, static_cast<int> (highRate * 1.5), 512);

        const auto predicted = engine.measureDrum (0).loadedFundamentalHz;
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);
        const auto rendered = render (engine, static_cast<int> (highRate * 0.4), 512);
        const auto sounded = dominantFrequency (
            rendered.mono(), highRate, predicted * 0.85, predicted * 1.15, 0.05,
            static_cast<std::size_t> (highRate * 0.02));

        const auto cents = 1200.0 * std::log2 (sounded / predicted);
        expect (std::abs (cents) < 10.0,
                "a stroke struck after the bend settled is "
                    + std::to_string (cents) + " cents from the bend it was struck at");
    }

    // A low-loss drum can ring far longer than the tail the host is told to
    // expect, so a voice still has to end at the cap - but it has to be faded
    // out, not cut. A cut at an audible level is a click, and it rings the
    // shared DC blocker on the way out.
    {
        auto ringing = parameters;
        ringing.humanise = 0.0f;
        ringing.headDiameter = 1.20f;
        ringing.headMaterial = 0.0f;
        ringing.headDamping = 0.0f;
        ringing.shellMaterial = 1.0f;
        ringing.octaveBody = 1.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (ringing);
        engine.prepare (48000.0, defaultBlockSize);

        // The configuration has to actually outlast the cap, or this proves
        // nothing about what happens when a voice reaches it.
        expect (engine.measureDrum (taikor::lowestOctaveOffset).tailSeconds
                    > static_cast<float> (taikor::maximumTailSeconds) * 1.2f,
                "the long-tail check no longer uses a drum that outlasts the cap");

        engine.trigger (taikor::Articulation::Don, taikor::lowestOctaveOffset, 1.0f);
        const auto rendered =
            render (engine, static_cast<int> (48000 * (taikor::maximumTailSeconds + 1.0)));
        const auto mono = rendered.mono();

        const auto cap = static_cast<std::size_t> (48000 * taikor::maximumTailSeconds);

        // How loud the drum still is as it reaches the cap. Cutting it here
        // would put a step of about this size into the output; fading it out
        // leaves steps a couple of orders of magnitude smaller.
        double amplitudeAtCap = 0.0;
        for (std::size_t index = cap - 9600; index < cap - 4800 && index < mono.size();
             ++index)
            amplitudeAtCap = std::max (amplitudeAtCap,
                                       std::abs (static_cast<double> (mono[index])));
        // An absolute level, because that is what a cut here would put into the
        // buffer as a step - and this configuration is loud enough to reach the
        // limiter, so its peak is pinned at one and a ratio against it would be
        // measuring the limiter rather than the drum.
        expect (amplitudeAtCap > 1.0e-4,
                "this drum is no longer audible at the cap, so the check is vacuous");

        double largestStep = 0.0;
        for (std::size_t index = cap - 4800; index + 1 < mono.size()
             && index < cap + 9600; ++index)
            largestStep = std::max (largestStep,
                                    std::abs (static_cast<double> (mono[index])
                                              - mono[index - 1]));

        expect (largestStep < amplitudeAtCap * 0.05,
                "the voice was cut at the tail cap instead of faded out");
        expect (windowedRms (mono, cap + 14400, cap + 24000) < 1.0e-6,
                "the voice did not actually end at the tail cap");
    }

    // Drive must converge on bypass rather than switching into a shaper: the
    // first nonzero step should be a hair of saturation, not a jump to a fully
    // shaped signal.
    {
        auto dry = parameters;
        dry.humanise = 0.0f;
        dry.drive = 0.0f;
        auto barely = dry;
        barely.drive = 0.002f;
        auto full = dry;
        full.drive = 1.0f;

        const auto rendered = [] (const taikor::EngineParameters& p)
        {
            return strike (p, taikor::Articulation::DonRim, -1, 1.0f, 48000.0, 12000);
        };

        const auto clean = rendered (dry);
        const auto nudged = rendered (barely);
        const auto driven = rendered (full);

        const auto nudgedChange = maximumAbsoluteDifference (clean.left, nudged.left);
        const auto drivenChange = maximumAbsoluteDifference (clean.left, driven.left);

        expect (drivenChange > 1.0e-3, "the drive control must do something");
        expect (nudgedChange < drivenChange * 0.05,
                "the first step of Drive jumped most of the way to full saturation");
    }

    // The Shell Resonance control is described as how much the drum's body
    // colours a head stroke. A stick-on-stick stroke never touches the body,
    // so the control must not reach it at all.
    {
        auto quiet = parameters;
        quiet.humanise = 0.0f;
        quiet.shellResonance = 0.0f;
        auto loud = quiet;
        loud.shellResonance = 1.0f;

        const auto a = strike (quiet, taikor::Articulation::Bachi, 0, 0.95f, 48000.0, 24000);
        const auto b = strike (loud, taikor::Articulation::Bachi, 0, 0.95f, 48000.0, 24000);
        expect (maximumAbsoluteDifference (a.left, b.left) == 0.0,
                "Shell Resonance changed a stick-on-stick stroke");

        // It must still do its job on a stroke that is the body.
        const auto c = strike (quiet, taikor::Articulation::Katsu, 0, 0.95f, 48000.0, 24000);
        const auto d = strike (loud, taikor::Articulation::Katsu, 0, 0.95f, 48000.0, 24000);
        expect (maximumAbsoluteDifference (c.left, d.left) > 1.0e-3,
                "Shell Resonance must still change a stroke on the body");
    }

    // Automating Pitch must retune a stroke that is already ringing, for the
    // same reason the wheel does: both are head tension.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);

        // Read before the automation moves, or the band is centred on where
        // the drum ends up and cannot see where it started.
        const auto resting = engine.measureDrum (0).loadedFundamentalHz;
        const auto low = static_cast<double> (resting) * 0.8;
        const auto high = static_cast<double> (resting) * 1.9;

        const auto before = render (engine, 24000).mono();

        auto raised = tuned;
        raised.pitch = 7.0f;
        engine.setParameters (raised);
        const auto after = render (engine, 72000).mono();

        const auto restingPitch =
            dominantFrequency (before, 48000.0, low, high, 0.05, 2400u);
        const auto raisedPitch =
            dominantFrequency (after, 48000.0, low, high, 0.05, 16000u);
        const auto semitones = 12.0 * std::log2 (raisedPitch / restingPitch);

        expect (semitones > 6.0 && semitones < 8.0,
                "automating Pitch must retune a ringing stroke by the amount asked for");
    }

    // A hand laid on the head damps the head. It is not resting on the wooden
    // shell, and it has nothing at all to do with two sticks clicked together.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;

        // Measured over the settled tail rather than the whole stroke: a hand
        // takes about a fifth of a second to arrive and press, so the attack is
        // barely touched either way and averaging it in hides the effect.
        // Each stroke is measured over its own tail, because they are wildly
        // different lengths: the drum rings for seconds and a stick click is
        // gone in a tenth of a second. Measuring the click over the drum's
        // window compares silence with silence and proves nothing.
        const auto tail = [&tuned] (taikor::Articulation articulation, bool handDown)
        {
            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (48000.0, defaultBlockSize);
            if (handDown)
                engine.setHandDamping (1.0f);
            engine.trigger (articulation, 0, 0.95f);
            return render (engine, 48000 * 2).mono();
        };

        const auto tailEnergy = [&tail] (taikor::Articulation articulation,
                                         bool handDown, std::size_t first,
                                         std::size_t last)
        {
            return windowedRms (tail (articulation, handDown), first, last);
        };

        // Measured at the head's own two modes rather than broadband. A hand
        // damps the head; it does not touch the wooden body, and it cannot
        // reach the airborne click of the stick landing. Those two set a floor
        // on the total that has nothing to do with how well the hand works, and
        // a broadband ratio is really measuring that floor - it moved the
        // moment the shell's ring changed, which is exactly the wrong
        // sensitivity for a test of the hand.
        const auto measurements = taikor::TaikoEngine::measure (tuned, 0);
        const auto headModes = [&measurements] (const std::vector<float>& samples)
        {
            return binMagnitude (samples, measurements.loadedFundamentalHz, 48000.0,
                                 24000u, 72000u)
                 + binMagnitude (samples, measurements.breathingModeHz, 48000.0,
                                 24000u, 72000u);
        };

        const auto openHead = headModes (tail (taikor::Articulation::Don, false));
        const auto dampedHead = headModes (tail (taikor::Articulation::Don, true));
        expect (openHead > 1.0e-3,
                "the head's tail measurement window caught no signal");
        expect (dampedHead < openHead * 0.05,
                "a hand on the head must damp the head");

        // Broadband as well as at the modes. Measuring the modes alone is
        // sharper about what the hand reaches, but it is blind to everything
        // the head does above them - and when the continuum was first added it
        // was not damped at all, which this check would have caught and the
        // modal one did not.
        // Early enough to still contain the continuum, which empties from the
        // top down and is largely gone by a quarter of a second.
        const auto openBroad =
            tailEnergy (taikor::Articulation::Don, false, 2400u, 12000u);
        const auto dampedBroad =
            tailEnergy (taikor::Articulation::Don, true, 2400u, 12000u);
        expect (openBroad > 1.0e-5,
                "the broadband tail measurement window caught no signal");
        expect (dampedBroad < openBroad * 0.68,
                "a hand on the head must damp everything the head is doing, "
                "not only its resolved modes");

        // The wooden shell is not what the hand is resting on, and two sticks
        // clicked together above the drum have nothing to do with it at all.
        const auto openSticks =
            tailEnergy (taikor::Articulation::Bachi, false, 1200u, 9600u);
        const auto dampedSticks =
            tailEnergy (taikor::Articulation::Bachi, true, 1200u, 9600u);
        expect (openSticks > 1.0e-5,
                "the stick stroke's measurement window caught no signal");
        expect (std::abs (dampedSticks - openSticks) < openSticks * 0.02,
                "a hand on the head must not damp a stick-on-stick stroke");
    }

    // The tail readout has to describe the drum the fundamental readout
    // describes. Reporting the breathing mode's decay beside the other
    // branch's frequency described neither, and the two diverge on a sealed
    // drum because only one of them radiates.
    {
        taikor::TaikoEngine engine;
        engine.prepare (48000.0, defaultBlockSize);

        auto sealed = parameters;
        sealed.cavityCoupling = 1.0f;
        engine.setParameters (sealed);
        const auto sealedDrum = engine.measureDrum (0);

        expect (sealedDrum.tailSeconds > 0.0f, "a drum must report a tail");

        // The rendered tail must be in the same country as the reported one.
        const auto rendered =
            strike (sealed, taikor::Articulation::Don, 0, 0.95f, 48000.0, 48000 * 6);
        const auto measured = decayTime (rendered.mono(), 48000.0, -60.0);
        expect (measured > sealedDrum.tailSeconds * 0.4
                    && measured < sealedDrum.tailSeconds * 2.5,
                "the reported tail does not match the tail the drum actually has");
    }

    // Later scheduled contacts must still find the bank they are meant to
    // drive. A flam's main stroke lands 32 ms after its grace note and a press
    // roll bounces for a tenth of a second; measuring mode lifetimes from the
    // voice's start retired the bank out from under them, and a press roll at
    // high damping reached its last bounce with nine of its thirty modes left.
    {
        for (const auto articulation : { taikor::Articulation::Flam,
                                         taikor::Articulation::Buzz })
        {
            for (const float damping : { 0.35f, 1.0f })
            {
                auto tuned = parameters;
                tuned.humanise = 0.0f;
                tuned.headDamping = damping;

                taikor::TaikoEngine engine;
                engine.setParameters (tuned);
                engine.prepare (48000.0, 64);
                engine.trigger (articulation, 0, 0.95f);

                using Access = taikor::TaikoEngineTestAccess;
                const auto atStart = Access::activeModeCount (engine);
                const auto lastContact = Access::lastContactStart (engine);
                expect (lastContact > 0u,
                        "a multi-contact stroke must schedule a later contact");

                render (engine, static_cast<int> (lastContact), 64);
                const auto atLastContact = Access::activeModeCount (engine);

                expect (atLastContact == atStart,
                        std::string (taikor::getArticulationDisplayName (articulation))
                            + " lost modes before its last scheduled contact");
            }
        }
    }

    // The airborne delay line must reach across the largest drum the controls
    // can produce at the highest supported rate. If the line is too short the
    // delays clamp, two different strike positions collapse onto the same
    // arrival time, and the cue that places a stroke across the image stops
    // working. The clamp is checked directly: the membrane modes reach both
    // microphones instantaneously here, so an onset measured from the rendered
    // audio is dominated by them and cannot see the airborne path at all.
    {
        auto extreme = parameters;
        extreme.humanise = 0.0f;
        extreme.headDiameter = 1.20f;   // the largest head
        extreme.octaveBody = 1.0f;      // realised as size, so two octaves down
        extreme.micSpread = 1.0f;       // microphones fully opened
        extreme.micDistance = 1.0f;

        const auto delaysAt = [&extreme] (float position, float& left, float& right)
        {
            auto tuned = extreme;
            tuned.strikePosition = position;

            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (384000.0, 256);
            engine.trigger (taikor::Articulation::Ka, taikor::lowestOctaveOffset,
                            0.95f);
            taikor::TaikoEngineTestAccess::directDelays (engine, left, right);
        };

        float rimLeft = 0.0f, rimRight = 0.0f;
        float centreLeft = 0.0f, centreRight = 0.0f;
        delaysAt (1.0f, rimLeft, rimRight);
        delaysAt (-1.0f, centreLeft, centreRight);

        const auto ceiling = taikor::TaikoEngineTestAccess::delayCeiling;
        for (const float delay : { rimLeft, rimRight, centreLeft, centreRight })
            expect (delay < ceiling,
                    "an airborne path delay reached the end of the delay line, so "
                    "the line is too short for the geometry the controls allow");

        expect (std::abs ((rimLeft - rimRight) - (centreLeft - centreRight)) > 1.0f,
                "two strike positions resolved to the same inter-microphone delay");
    }

    // The airborne delay line's fractional read, checked exactly. A ramp makes
    // the correct answer arithmetic: reading it back at delay d must return the
    // ramp's value d samples ago, to within interpolation error. Reading the
    // wrong neighbour adds the fractional part instead of subtracting it, which
    // this catches at every non-integer delay.
    {
        using Access = taikor::TaikoEngineTestAccess;
        std::array<float, Access::lineSize> line {};
        constexpr int writeIndex = 600;
        for (int index = 0; index < Access::lineSize; ++index)
            line[static_cast<std::size_t> (index)] = static_cast<float> (index);

        for (const float delay : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 7.5f,
                                   19.75f, 63.0f, 100.5f })
        {
            const auto value = Access::readDelayLine (line, writeIndex, delay);
            const auto expected = static_cast<float> (writeIndex) - delay;
            expect (std::abs (value - expected) < 1.0e-3f,
                    "the airborne delay line read " + std::to_string (value)
                        + " where a delay of " + std::to_string (delay)
                        + " should have returned " + std::to_string (expected));
        }
    }

    // The airborne path is what places a stroke across the image, and it
    // carries a real time difference. An off-centre stroke must therefore reach
    // the nearer microphone first.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.micSpread = 1.0f;
        tuned.micDistance = 0.0f;
        tuned.strikePosition = 1.0f;

        const auto rendered =
            strike (tuned, taikor::Articulation::Ka, 0, 0.95f, 48000.0, 4000, 64);

        const auto onset = [] (const std::vector<float>& samples)
        {
            double peak = 0.0;
            for (const float value : samples)
                peak = std::max (peak, std::abs (static_cast<double> (value)));
            for (std::size_t index = 0; index < samples.size(); ++index)
                if (std::abs (static_cast<double> (samples[index])) > 0.25 * peak)
                    return static_cast<double> (index);
            return 0.0;
        };

        const auto separation = std::abs (onset (rendered.left) - onset (rendered.right));
        expect (separation > 1.0,
                "an off-centre stroke must reach the two microphones at different times");
        // A path difference cannot exceed the drum crossed at the speed of
        // sound; anything larger means the delay line is being read wrongly.
        expect (separation < 0.005 * 48000.0,
                "the inter-microphone delay is longer than the drum is wide");
    }
}
} // namespace

int main()
{
    testArticulationMetadataAndMidiMapping();
    testOctavesRaisePitch();
    testEveryArticulationAndSampleRate();
    testSampleRateConsistency();
    testVelocitySensitivity();
    testPhysicalParameterInfluence();
    testStrikePositionShapesTheSpectrum();
    testCloseMicrophonePair();
    testTailsTerminateAndVoicesRetire();
    testVoiceStealingStaysBounded();
    testTheDrumSoundsLikeADrumAndNotLikeATone();
    testEveryParameterSurvivesTheCache();
    testRadiationEfficiencyStaysFinite();
    testReportedModesAreActuallySounded();
    testStickStrokeIsIndependentOfTheDrum();
    testSimultaneousStrokesDoNotShareOneVoice();
    testDeterminismAndBlockPartitioning();
    testPerformanceControls();
    testInvalidInputSafety();
    testUiPresentationMath();
    testControlEndpointsAndGestures();
    testIdleCostAndStressPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Taikor DSP test(s) failed\n";
        return 1;
    }
    std::cout << "All Taikor DSP tests passed\n";
    return 0;
}

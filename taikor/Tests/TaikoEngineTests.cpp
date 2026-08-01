#include "DSP/TaikoEngine.h"
#include "DSP/UiMath.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
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
    taikor::EngineParameters parameters;
    parameters.outputGain = 0.5f;
    return parameters;
}

// A single stroke, rendered from a clean engine.
Rendered strike (taikor::EngineParameters parameters, taikor::Articulation articulation,
                 int octaveOffset, float velocity, double sampleRate, int numSamples,
                 int blockSize = defaultBlockSize)
{
    taikor::TaikoEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.setParameters (parameters);
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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (defaultParameters());

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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (parameters);

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
    engine.prepare (1.0, 64);
    engine.setParameters (parameters);
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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (parameters);

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

    for (const float velocity : { 0.15f, 0.45f, 0.75f, 1.0f })
    {
        const auto rendered = strike (parameters, taikor::Articulation::Don, 0, velocity,
                                      48000.0, 24000);
        const auto mono = rendered.mono();

        // Ratio of high-band to low-band energy over the attack.
        const double low = binMagnitude (mono, 95.0, 48000.0, 0, 4800)
                         + binMagnitude (mono, 134.0, 48000.0, 0, 4800);
        double high = 0.0;
        for (const double frequency : { 900.0, 1400.0, 2100.0, 3000.0 })
            high += binMagnitude (mono, frequency, 48000.0, 0, 4800);
        const double brightness = high / std::max (low, 1.0e-9);

        expect (rendered.peak > previousPeak,
                "a harder stroke must be louder");
        expect (brightness > previousBrightness * 0.98,
                "a harder stroke must not lose high partial content");
        previousPeak = rendered.peak;
        previousBrightness = brightness;
    }

    expect (previousBrightness > 0.0, "brightness measurement failed to run");

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

    auto bigger = base;
    bigger.headDiameter = 1.10f;
    expect (measure (bigger).idealFundamentalHz < reference.idealFundamentalHz * 0.6f,
            "a bigger drum must be lower");

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
    expect (dampedDrum.fundamentalT60Seconds < reference.fundamentalT60Seconds * 0.5f,
            "raising the damping must substantially shorten the tail");
    expect (dampedDrum.fundamentalT60Seconds > 0.0f,
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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (parameters);
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
    expect (decayTime (edgeMono, 48000.0, -40.0) < decayTime (centreMono, 48000.0, -40.0),
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

    // Every articulation must stay mono-compatible.
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto rendered = strike (parameters, articulation, 0, 0.9f, 48000.0, 24000);
        expect (correlation (rendered.left, rendered.right) > 0.0,
                std::string (taikor::getArticulationDisplayName (articulation))
                    + " produced an out-of-phase pair");
    }
}

void testTailsTerminateAndVoicesRetire()
{
    auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (parameters);

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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (parameters);

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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (humanised);
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
    tight.prepare (48000.0, defaultBlockSize);
    tight.setParameters (machine);
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
    taikor::TaikoEngine open;
    open.prepare (48000.0, defaultBlockSize);
    open.setParameters (parameters);
    open.trigger (taikor::Articulation::Don, 0, 0.95f);
    const auto openTail = render (open, 48000 * 3);

    taikor::TaikoEngine damped;
    damped.prepare (48000.0, defaultBlockSize);
    damped.setParameters (parameters);
    damped.trigger (taikor::Articulation::Don, 0, 0.95f);
    render (damped, 4800);
    damped.setHandDamping (1.0f);
    const auto dampedTail = render (damped, 48000 * 3);

    expect (dampedTail.rms < openTail.rms * 0.6,
            "a hand on the head must damp what is still ringing");

    // The wheel must raise the pitch, because pressing a head tightens it. It
    // glides rather than jumping, so the engine has to be run for the strings
    // of the smoother to arrive.
    taikor::TaikoEngine bent;
    bent.prepare (48000.0, defaultBlockSize);
    bent.setParameters (parameters);
    bent.setPitchBend (1.0f);
    render (bent, 24000);
    const auto bentMeasurements = bent.measureDrum (0);

    taikor::TaikoEngine plain;
    plain.prepare (48000.0, defaultBlockSize);
    plain.setParameters (parameters);
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
    auto quiet = parameters;
    quiet.outputGain = 0.25f;
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
    engine.prepare (48000.0, defaultBlockSize);
    engine.setParameters (defaultParameters());

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
        engine.prepare (48000.0, defaultBlockSize);
        engine.setParameters (tuned);
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);

        const auto before = render (engine, 24000).mono();
        engine.setPitchBend (1.0f);
        const auto after = render (engine, 72000).mono();

        const auto restingPitch =
            dominantFrequency (before, 48000.0, 60.0, 200.0, 0.25, 2400u);
        // Skip the glide itself and measure where the drum settled.
        const auto bentPitch =
            dominantFrequency (after, 48000.0, 60.0, 200.0, 0.25, 16000u);
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
            engine.prepare (48000.0, block);
            engine.setParameters (tuned);
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

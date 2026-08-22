// The eleven Sound Charts, voiced from the owner's manual's panel drawings
// and lesson text (printed pages 3–15). Each comment quotes what the chart
// teaches; the travels are Ghost's voicing of the drawn knob positions.

#include "DSP/GhostPresets.h"

#include <array>

namespace ghost
{
namespace
{
    struct Preset
    {
        const char* name;
        EngineParameters parameters;
    };

    // "All rotary pots 12 o'clock, rotary switches fully left, rocker
    // switches down, sliders down, pitch bend centered, X/Y wheels fully
    // back. Produces no sound. It is simply an easily remembered starting
    // point." (p.3)
    EngineParameters preparatoryPattern()
    {
        EngineParameters p;
        p.tune = 0.5f;
        p.octave = MasterOctave::ThirtyTwo;          // fully left
        p.oscAWaveform = Waveform::Triangle;
        p.sync = false;
        p.oscBWaveform = Waveform::Triangle;
        p.oscBRange = OscBRange::MinusOne;           // fully left
        p.interval = 0.5f;
        p.trigger = TriggerMode::Multiple;           // rocker down
        p.gateKbd = false;                           // rockers down: no gate
        p.gateX = false;
        p.gateYExt = false;
        p.arpeggiator = ArpeggiatorMode::Off;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.5f;
        p.shaperMode = ShaperMode::Free;
        p.shaperShape = 0.5f;
        p.shaperRate = 0.5f;
        p.modXTo = ModXDestination::Off;
        p.shapeXWithY = false;
        p.shaperYTo = ShaperYDestination::Off;
        p.masterVolume = 0.5f;
        p.brightness = 0.5f;
        p.shaperPathA = 0.0f;                        // sliders down
        p.shaperPathB = 0.0f;
        p.shaperPathRing = 0.0f;
        p.shaperPathNoise = 0.0f;
        p.filterPathA = 0.0f;
        p.filterPathB = 0.0f;
        p.filterPathNoise = 0.0f;
        p.cutoff = 0.5f;
        p.lowerOnly = 0.5f;
        p.upperResonance = UpperResonanceMode::Variable; // rocker down
        p.resonance = 0.5f;
        p.slope = UpperSlope::TwentyFourDb;          // rocker down
        p.kbAmount = 0.5f;
        p.lowerMode = LowerFilterMode::Out;          // fully left
        p.tracking = TrackingMode::Dynamic;          // rocker down
        p.filterEnvAmount = 0.5f;
        p.filterAttack = 0.0f;
        p.filterDecay = 0.0f;
        p.filterSustain = 0.0f;
        p.filterRelease = 0.0f;
        p.vcaBypass = false;                         // rocker down: NORMAL
        p.loudnessAttack = 0.0f;
        p.loudnessDecay = 0.0f;
        p.loudnessSustain = 0.0f;
        p.loudnessRelease = 0.0f;
        p.glide = 0.0f;
        p.glideMode = GlideMode::Off;
        p.splitPaths = false;
        return p;
    }

    // "Exploration of Raw Audio Signals": VCA to BYPASS, audition each
    // source through a wide-open filter. "RING can produce bell-like
    // sounds… NOISE sounds like static." (p.6)
    EngineParameters soundSources()
    {
        auto p = preparatoryPattern();
        p.gateKbd = true;
        p.vcaBypass = true;
        p.filterPathA = 0.75f;
        p.filterPathB = 0.55f;
        p.shaperPathRing = 0.55f;
        p.shaperPathNoise = 0.3f;
        p.shaperMode = ShaperMode::KbdHold;
        p.shaperRate = 0.9f;
        p.brightness = 0.9f;
        p.octave = MasterOctave::Eight;
        p.oscAWaveform = Waveform::Sawtooth;
        p.oscBWaveform = Waveform::Sawtooth;
        p.interval = 0.53f;
        p.cutoff = 0.95f;
        p.resonance = 0.1f;
        return p;
    }

    // "Gating + upper-filter features. Filter resonance affects sound
    // greatly… MASTER CUTOFF sets filter 'starting point'." (p.7)
    EngineParameters fatFilter()
    {
        auto p = preparatoryPattern();
        p.gateKbd = true;
        p.octave = MasterOctave::Eight;
        p.oscAWaveform = Waveform::Sawtooth;
        p.filterPathA = 0.8f;
        p.cutoff = 0.45f;
        p.resonance = 0.6f;
        p.kbAmount = 0.45f;
        p.filterEnvAmount = 0.7f;
        p.filterDecay = 0.5f;
        p.filterSustain = 0.3f;
        p.filterRelease = 0.35f;
        p.loudnessDecay = 0.55f;
        p.loudnessSustain = 0.75f;
        p.loudnessRelease = 0.35f;
        return p;
    }

    // "Osc A at 50 % square, Mod X vibrato routings. MOD SOURCE determines
    // the shape… MOD X TO: determines where the signal goes." (p.8)
    EngineParameters modWhistle()
    {
        auto p = fatFilter();
        p.oscAWaveform = Waveform::RectWide;
        p.cutoff = 0.62f;
        p.resonance = 0.25f;
        p.filterEnvAmount = 0.58f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.55f;
        p.modXTo = ModXDestination::OscAB;
        return p;
    }

    // The siren: "SYNC ON, Shaper Y sweeping Osc B. It is important to tune
    // Osc B pitch higher than A to get a strong sync sound." (p.9)
    EngineParameters sync()
    {
        auto p = preparatoryPattern();
        p.gateKbd = true;
        p.octave = MasterOctave::Eight;
        p.sync = true;
        p.oscBWaveform = Waveform::Sawtooth;
        p.oscBRange = OscBRange::Unison;
        p.interval = 0.8f;
        p.filterPathB = 0.8f;
        p.cutoff = 0.72f;
        p.resonance = 0.15f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.6f;
        p.shaperShape = 0.15f;
        p.shaperYTo = ShaperYDestination::OscB;
        p.loudnessDecay = 0.6f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.4f;
        return p;
    }

    // "Y-shaped vibrato: the Y wheel sets fastest rate; LFO/S+H RATE sets
    // slowest (beginning) rate." SHAPE X WITH Y envelopes the vibrato
    // depth. (p.10)
    EngineParameters shakeShape()
    {
        auto p = modWhistle();
        p.shapeXWithY = true;
        p.shaperMode = ShaperMode::KbdHold;
        p.shaperRate = 0.35f;
        p.shaperShape = 0.75f;
        return p;
    }

    // "Metronomic voltage steps clocked by LFO/S+H RATE… S&H to filter and
    // oscillators." (p.11)
    EngineParameters sampleAndHold()
    {
        auto p = fatFilter();
        p.oscBWaveform = Waveform::Sawtooth;
        p.oscBRange = OscBRange::MinusOne;
        p.filterPathB = 0.6f;
        p.interval = 0.5f;
        p.modSource = ModSource::SampleHoldRandom;
        p.lfoRate = 0.55f;
        p.modXTo = ModXDestination::FilterUL;
        p.resonance = 0.55f;
        p.filterEnvAmount = 0.5f;
        p.loudnessSustain = 0.85f;
        return p;
    }

    // "Perfect-fifth interval, independent PWM per oscillator via the X and
    // Y busses." (p.12)
    EngineParameters parallelRectangles()
    {
        auto p = preparatoryPattern();
        p.gateKbd = true;
        p.octave = MasterOctave::Eight;
        p.oscAWaveform = Waveform::RectMid;
        p.oscBWaveform = Waveform::RectNarrow;
        p.oscBRange = OscBRange::Unison;
        p.interval = 1.0f;                       // up a perfect fifth
        p.filterPathA = 0.7f;
        p.filterPathB = 0.7f;
        p.cutoff = 0.66f;
        p.resonance = 0.2f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.4f;
        p.modXTo = ModXDestination::OscARwm;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.45f;
        p.shaperYTo = ShaperYDestination::OscBRwm;
        p.filterEnvAmount = 0.55f;
        p.filterDecay = 0.5f;
        p.filterSustain = 0.5f;
        p.loudnessAttack = 0.15f;
        p.loudnessDecay = 0.5f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.45f;
        return p;
    }

    // "Arpeggiator modes with noise in the Shaper-Y path. ARPEGGIATOR modes
    // cause SHAPER Y RATE to synchronize with LFO/S+H RATE." (p.13)
    EngineParameters arpeggio()
    {
        auto p = fatFilter();
        p.arpeggiator = ArpeggiatorMode::Arpeggio;
        p.lfoRate = 0.6f;
        p.gateKbd = false;
        p.gateX = true;
        p.filterSustain = 0.15f;
        p.filterDecay = 0.4f;
        p.loudnessDecay = 0.45f;
        p.loudnessSustain = 0.4f;
        p.shaperPathNoise = 0.35f;
        p.brightness = 0.6f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperShape = 0.1f;
        return p;
    }

    // "Noise takes on the pitch of filter cutoff frequency when resonance
    // is maximum" — noise as a pitched source tracking the keyboard. (p.14)
    EngineParameters noiseScale()
    {
        auto p = preparatoryPattern();
        p.gateKbd = true;
        p.octave = MasterOctave::Eight;
        p.filterPathNoise = 0.8f;
        p.cutoff = 0.55f;
        p.resonance = 1.0f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.kbAmount = 0.9f;
        p.slope = UpperSlope::TwentyFourDb;
        p.loudnessDecay = 0.55f;
        p.loudnessSustain = 0.8f;
        p.loudnessRelease = 0.4f;
        return p;
    }

    // "Inverted filter envelope plucks. The Spirit synthesizer has balls."
    // (p.15)
    EngineParameters invertedGuitar()
    {
        auto p = fatFilter();
        p.filterEnvAmount = 0.22f;               // the INVERT side
        p.cutoff = 0.6f;
        p.resonance = 0.35f;
        p.filterDecay = 0.32f;
        p.filterSustain = 0.0f;
        p.loudnessDecay = 0.5f;
        p.loudnessSustain = 0.35f;
        p.loudnessRelease = 0.3f;
        p.oscBWaveform = Waveform::RectMid;
        p.oscBRange = OscBRange::Unison;
        p.interval = 0.53f;
        p.filterPathB = 0.5f;
        return p;
    }

    const std::array<Preset, 12>& presets()
    {
        static const std::array<Preset, 12> table {{
            // The engine's default voice: what a fresh instance already
            // sounds like, so the program a host labels 0 tells the truth.
            { "Init", EngineParameters {} },
            { "Preparatory Pattern", preparatoryPattern() },
            { "Sound Sources", soundSources() },
            { "Fat Filter", fatFilter() },
            { "Mod Whistle", modWhistle() },
            { "Sync", sync() },
            { "Shake Shape", shakeShape() },
            { "Sample & Hold", sampleAndHold() },
            { "Parallel Rectangles", parallelRectangles() },
            { "Arpeggio", arpeggio() },
            { "Noise Scale", noiseScale() },
            { "Inverted Guitar", invertedGuitar() },
        }};
        return table;
    }
} // namespace

int factoryPresetCount() noexcept
{
    return static_cast<int>(presets().size());
}

const char* factoryPresetName(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return nullptr;
    return presets()[static_cast<std::size_t>(index)].name;
}

EngineParameters factoryPresetParameters(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return EngineParameters {};
    return presets()[static_cast<std::size_t>(index)].parameters;
}

} // namespace ghost

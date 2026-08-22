// The eleven Sound Charts, voiced from the owner's manual's panel drawings
// and lesson text (printed pages 3-15), and Ghostar's own performance bank
// behind them. Each Sound Chart's comment quotes what the chart teaches;
// each Program's says which mechanism it foregrounds. See GhostarPresets.h
// for what separates the two banks.

#include "DSP/GhostarPresets.h"

#include <array>

namespace ghostar
{
namespace
{
    struct Preset
    {
        const char* name;
        const char* description;
        PresetBank bank;
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

    // ---------------------------------------------------------------------
    // Ghostar Programs: the performance bank. These are Ghostar's own
    // voicings on the modelled panel — the hardware shipped no presets to
    // copy — and each foregrounds one mechanism the instrument is known
    // for. Every one starts from a panel that is playable rather than from
    // the silent Preparatory Pattern.
    // ---------------------------------------------------------------------

    // A playable starting panel: saw A into the Filter/ADSR path, keyboard
    // gating, the upper filter open enough to hear the source.
    EngineParameters playablePanel()
    {
        EngineParameters p;
        p.octave = MasterOctave::Eight;
        p.oscAWaveform = Waveform::Sawtooth;
        p.oscBWaveform = Waveform::Sawtooth;
        p.gateKbd = true;
        p.filterPathA = 0.8f;
        p.cutoff = 0.62f;
        p.resonance = 0.2f;
        p.kbAmount = 0.45f;
        p.filterEnvAmount = 0.62f;
        p.filterDecay = 0.45f;
        p.filterSustain = 0.35f;
        p.filterRelease = 0.3f;
        p.loudnessAttack = 0.0f;
        p.loudnessDecay = 0.45f;
        p.loudnessSustain = 0.8f;
        p.loudnessRelease = 0.28f;
        p.masterVolume = 0.8f;
        return p;
    }

    // The 24 dB lowpass under a fast filter envelope: the plain, weighty
    // mono bass the architecture is built for.
    EngineParameters spiritBass()
    {
        auto p = playablePanel();
        p.octave = MasterOctave::Sixteen;
        p.slope = UpperSlope::TwentyFourDb;
        p.upperResonance = UpperResonanceMode::Variable;
        p.resonance = 0.62f;
        p.cutoff = 0.34f;
        p.kbAmount = 0.35f;
        p.filterEnvAmount = 0.76f;
        p.filterDecay = 0.34f;
        p.filterSustain = 0.12f;
        p.loudnessDecay = 0.4f;
        p.loudnessSustain = 0.62f;
        p.loudnessRelease = 0.22f;
        p.filterPathB = 0.45f;
        p.oscBRange = OscBRange::Unison;
        p.interval = 0.52f;              // the manual's warmth detune
        p.masterVolume = 1.0f;
        return p;
    }

    // The signature: the lower section's parametric boost offset below the
    // upper lowpass, so the pair reads as two formants sliding together.
    EngineParameters vocalPair()
    {
        auto p = playablePanel();
        p.lowerMode = LowerFilterMode::BandPass;
        p.resonance = 0.78f;
        p.cutoff = 0.56f;
        p.lowerOnly = 0.38f;
        p.filterPathB = 0.5f;
        p.interval = 0.53f;
        p.filterEnvAmount = 0.6f;
        p.filterAttack = 0.14f;
        p.filterDecay = 0.55f;
        p.filterSustain = 0.5f;
        p.loudnessAttack = 0.08f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.35f;
        p.masterVolume = 0.62f;
        return p;
    }

    // FORMANT freezes the lower peak while the upper filter articulates —
    // the fixed vocal tract the starred brass/woodwind charts are after.
    EngineParameters formantReed()
    {
        auto p = vocalPair();
        p.tracking = TrackingMode::Formant;
        p.lowerOnly = 0.6f;
        p.resonance = 0.82f;
        p.cutoff = 0.5f;
        p.kbAmount = 0.55f;
        p.filterEnvAmount = 0.72f;
        p.filterAttack = 0.2f;
        p.loudnessAttack = 0.12f;
        p.masterVolume = 0.57f;
        return p;
    }

    // The inter-filter clipper, driven by the lower boost and re-filtered
    // by the upper lowpass: the raunchy register.
    EngineParameters growlBass()
    {
        auto p = spiritBass();
        p.lowerMode = LowerFilterMode::Overdrive;
        p.resonance = 0.7f;
        p.cutoff = 0.4f;
        p.lowerOnly = 0.56f;
        p.filterPathA = 1.0f;
        p.filterPathB = 0.7f;
        p.oscBRange = OscBRange::MinusOne;
        p.filterEnvAmount = 0.7f;
        p.filterDecay = 0.42f;
        p.loudnessSustain = 0.75f;
        p.masterVolume = 0.71f;
        return p;
    }

    // Hard sync with the Shaper sweeping B through the lock on every key:
    // the tearing lead. Ride the Y wheel for depth.
    EngineParameters syncLead()
    {
        auto p = playablePanel();
        p.sync = true;
        p.filterPathA = 0.0f;
        p.filterPathB = 0.85f;
        p.interval = 0.78f;
        p.cutoff = 0.78f;
        p.resonance = 0.3f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.6f;
        p.shaperShape = 0.12f;
        p.shaperYTo = ShaperYDestination::OscB;
        p.filterEnvAmount = 0.55f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.35f;
        p.glide = 0.25f;
        p.glideMode = GlideMode::Auto;
        p.masterVolume = 0.88f;
        return p;
    }

    // The triangle-cross ring modulator on the Shaper path, struck by a
    // fast RESET rise and left to fall: clangorous and inharmonic.
    EngineParameters ringBell()
    {
        EngineParameters p;
        p.octave = MasterOctave::Eight;
        p.gateKbd = true;
        p.shaperPathRing = 0.85f;
        p.shaperPathA = 0.18f;
        p.brightness = 0.78f;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.67f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.32f;
        p.shaperShape = 0.06f;
        p.masterVolume = 0.85f;
        return p;
    }

    // Both audio paths at once, split to the two jacks: an enveloped line
    // on the filter path against a free-running ring-and-noise drone.
    EngineParameters twoPathDrift()
    {
        auto p = playablePanel();
        p.splitPaths = true;
        p.cutoff = 0.46f;
        p.resonance = 0.45f;
        p.filterPathA = 0.7f;
        p.shaperPathRing = 0.5f;
        p.shaperPathNoise = 0.22f;
        p.shaperPathB = 0.32f;
        p.oscBRange = OscBRange::Bass;
        p.interval = 0.34f;
        p.brightness = 0.5f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.52f;
        p.shaperShape = 0.72f;
        p.loudnessAttack = 0.18f;
        p.loudnessRelease = 0.45f;
        p.masterVolume = 1.0f;
        return p;
    }

    // VCA BYPASS holds the filter path open, so held keys drone and the
    // Shaper path breathes over them: the instrument as a pad machine.
    EngineParameters bypassPad()
    {
        auto p = playablePanel();
        p.vcaBypass = true;
        p.cutoff = 0.5f;
        p.resonance = 0.55f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.5f;
        p.filterPathA = 0.55f;
        p.filterPathB = 0.5f;
        p.interval = 0.545f;
        p.shaperPathNoise = 0.14f;
        p.brightness = 0.4f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.28f;
        p.shaperShape = 0.5f;
        p.modSource = ModSource::RedNoise;
        p.modXTo = ModXDestination::FilterUL;
        p.masterVolume = 0.7f;
        return p;
    }

    // LEAP cycles each successive note through unison, up an octave and
    // down an octave, with the X gate clocking the envelopes.
    EngineParameters leapSequence()
    {
        auto p = playablePanel();
        p.arpeggiator = ArpeggiatorMode::Leap;
        p.lfoRate = 0.6f;
        p.gateKbd = false;
        p.gateX = true;
        p.cutoff = 0.5f;
        p.resonance = 0.68f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.filterEnvAmount = 0.74f;
        p.filterDecay = 0.32f;
        p.filterSustain = 0.06f;
        p.loudnessDecay = 0.35f;
        p.loudnessSustain = 0.25f;
        p.loudnessRelease = 0.2f;
        p.masterVolume = 1.0f;
        return p;
    }

    // The patterned staircase: S+H sampling the Shaper rather than noise,
    // so the filter steps through a repeating figure instead of wandering.
    EngineParameters patternedSteps()
    {
        auto p = playablePanel();
        p.modSource = ModSource::SampleHoldY;
        p.modXTo = ModXDestination::FilterUL;
        p.lfoRate = 0.6f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.44f;
        p.shaperShape = 0.3f;
        p.cutoff = 0.44f;
        p.resonance = 0.72f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.filterPathB = 0.55f;
        p.oscBRange = OscBRange::MinusOne;
        p.filterEnvAmount = 0.5f;
        p.loudnessSustain = 0.85f;
        p.masterVolume = 0.74f;
        return p;
    }

    // AUTO glide with vibrato waiting on the X wheel: legato only when the
    // player overlaps two keys, as the panel's AUTO position means.
    EngineParameters glideLead()
    {
        auto p = playablePanel();
        p.glide = 0.42f;
        p.glideMode = GlideMode::Auto;
        p.trigger = TriggerMode::Single;
        p.cutoff = 0.7f;
        p.resonance = 0.4f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.54f;
        p.modXTo = ModXDestination::OscAB;
        p.oscAWaveform = Waveform::RectWide;
        p.filterEnvAmount = 0.58f;
        p.loudnessSustain = 0.9f;
        p.loudnessRelease = 0.32f;
        p.masterVolume = 0.67f;
        return p;
    }

    // Independent pulse-width modulation per oscillator — A's from the X
    // bus, B's from the Y — a fifth apart: hollow and always moving.
    EngineParameters hollowFifth()
    {
        auto p = playablePanel();
        p.oscAWaveform = Waveform::RectMid;
        p.oscBWaveform = Waveform::RectNarrow;
        p.interval = 1.0f;                   // up a perfect fifth
        p.filterPathA = 0.7f;
        p.filterPathB = 0.62f;
        p.cutoff = 0.66f;
        p.resonance = 0.35f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.36f;
        p.modXTo = ModXDestination::OscARwm;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.4f;
        p.shaperYTo = ShaperYDestination::OscBRwm;
        p.filterEnvAmount = 0.56f;
        p.loudnessAttack = 0.14f;
        p.loudnessSustain = 0.88f;
        p.loudnessRelease = 0.4f;
        p.masterVolume = 0.73f;
        return p;
    }

    // Noise through a filter resonant enough to ring at its cutoff, with
    // the cutoff tracking the keyboard: noise played as a pitch.
    EngineParameters noiseFlute()
    {
        auto p = playablePanel();
        p.filterPathA = 0.0f;
        p.filterPathNoise = 0.85f;
        p.cutoff = 0.58f;
        p.resonance = 0.93f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.kbAmount = 0.92f;
        p.slope = UpperSlope::TwentyFourDb;
        p.filterEnvAmount = 0.5f;
        p.loudnessAttack = 0.1f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.3f;
        p.masterVolume = 1.0f;
        return p;
    }

    // Osc B parked in BASS as a fixed sub under a played lead: the drone
    // ranges disconnect B from the keyboard entirely.
    EngineParameters subAndLead()
    {
        auto p = playablePanel();
        p.oscBRange = OscBRange::Bass;
        p.interval = 0.24f;                  // a low fixed drone
        p.oscBWaveform = Waveform::RectWide;
        p.filterPathA = 0.7f;
        p.filterPathB = 0.6f;
        p.cutoff = 0.6f;
        p.resonance = 0.5f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.66f;
        p.filterEnvAmount = 0.64f;
        p.loudnessSustain = 0.82f;
        p.masterVolume = 0.71f;
        return p;
    }

    // RUN never abandons a rising segment, so the Shaper's VCA chops the
    // second path into a rhythm the keyboard restarts but cannot interrupt.
    EngineParameters shaperPulse()
    {
        auto p = playablePanel();
        p.filterPathA = 0.0f;
        p.shaperPathA = 0.7f;
        p.shaperPathB = 0.5f;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.53f;
        p.brightness = 0.72f;
        p.shaperMode = ShaperMode::Run;
        p.shaperRate = 0.72f;
        p.shaperShape = 0.2f;
        p.gateYExt = true;
        p.masterVolume = 1.0f;
        return p;
    }

    // WIDE takes Osc B far below the keyboard, where its cycle reads as a
    // pulse rather than a pitch, under noise through the overdrive stage.
    EngineParameters thunder()
    {
        auto p = playablePanel();
        p.octave = MasterOctave::ThirtyTwo;
        p.oscBRange = OscBRange::Wide;
        p.interval = 0.06f;                  // a few hertz: a slow beat
        p.oscBWaveform = Waveform::Triangle;
        p.filterPathA = 0.55f;
        p.filterPathB = 0.8f;
        p.filterPathNoise = 0.45f;
        p.lowerMode = LowerFilterMode::Overdrive;
        p.cutoff = 0.3f;
        p.resonance = 0.6f;
        p.lowerOnly = 0.6f;
        p.filterEnvAmount = 0.6f;
        p.filterAttack = 0.2f;
        p.loudnessAttack = 0.16f;
        p.loudnessDecay = 0.6f;
        p.loudnessSustain = 0.7f;
        p.loudnessRelease = 0.5f;
        p.masterVolume = 0.89f;
        return p;
    }

    // The resonant highpass against the upper lowpass: the double-peak,
    // hollowed-out register the panel's HIGHPASS position reaches.
    EngineParameters hollowGhost()
    {
        auto p = playablePanel();
        p.lowerMode = LowerFilterMode::HighPass;
        p.lowerOnly = 0.52f;
        p.resonance = 0.72f;
        p.cutoff = 0.66f;
        p.filterPathA = 0.75f;
        p.filterPathB = 0.55f;
        p.interval = 0.56f;
        p.filterEnvAmount = 0.66f;
        p.filterAttack = 0.1f;
        p.filterDecay = 0.5f;
        p.filterSustain = 0.42f;
        p.loudnessAttack = 0.06f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.38f;
        p.masterVolume = 0.97f;
        return p;
    }

    const std::array<Preset, 29>& presets()
    {
        static const std::array<Preset, 29> table {{
            // --- Sound Charts: the manual's own lessons, in its order ----
            // The engine's default voice: what a fresh instance already
            // sounds like, so the program a host labels 0 tells the truth.
            { "Init", "The engine's default voice, as a fresh instance sounds",
              PresetBank::SoundCharts, EngineParameters {} },
            { "Preparatory Pattern",
              "The silent starting panel every lesson begins from",
              PresetBank::SoundCharts, preparatoryPattern() },
            { "Sound Sources",
              "Every raw source auditioned through a wide-open filter",
              PresetBank::SoundCharts, soundSources() },
            { "Fat Filter", "Gating and the upper filter's resonance",
              PresetBank::SoundCharts, fatFilter() },
            { "Mod Whistle", "MOD X vibrato routed to both oscillators",
              PresetBank::SoundCharts, modWhistle() },
            { "Sync", "Hard sync with the Shaper sweeping Osc B",
              PresetBank::SoundCharts, sync() },
            { "Shake Shape", "SHAPE X WITH Y: vibrato the Shaper envelopes",
              PresetBank::SoundCharts, shakeShape() },
            { "Sample & Hold", "Metronomic random steps on both filters",
              PresetBank::SoundCharts, sampleAndHold() },
            { "Parallel Rectangles",
              "A perfect fifth with independent PWM per oscillator",
              PresetBank::SoundCharts, parallelRectangles() },
            { "Arpeggio", "The arpeggiator clocking gates, noise on the "
              "Shaper path", PresetBank::SoundCharts, arpeggio() },
            { "Noise Scale", "Noise pitched by a filter at full resonance",
              PresetBank::SoundCharts, noiseScale() },
            { "Inverted Guitar", "Plucks from an inverted filter envelope",
              PresetBank::SoundCharts, invertedGuitar() },

            // --- Ghostar Programs: the performance bank ------------------
            { "Spirit Bass",
              "The 24 dB lowpass under a fast filter envelope",
              PresetBank::Programs, spiritBass() },
            { "Vocal Pair",
              "The signature dual filter: a boost peak sliding under the "
              "lowpass", PresetBank::Programs, vocalPair() },
            { "Formant Reed",
              "FORMANT freezes the lower peak while the upper articulates",
              PresetBank::Programs, formantReed() },
            { "Growl Bass",
              "The inter-filter clipper, re-filtered by the upper lowpass",
              PresetBank::Programs, growlBass() },
            { "Sync Lead",
              "Hard sync torn open by the Shaper; ride the Y wheel",
              PresetBank::Programs, syncLead() },
            { "Ring Bell",
              "The triangle-cross ring modulator struck and left to fall",
              PresetBank::Programs, ringBell() },
            { "Two-Path Drift",
              "An enveloped line left, a free-running ring drone right",
              PresetBank::Programs, twoPathDrift() },
            { "Bypass Pad",
              "VCA BYPASS holds the path open; red noise wanders the cutoff",
              PresetBank::Programs, bypassPad() },
            { "Leap Sequence",
              "LEAP cycles each note through unison, up and down an octave",
              PresetBank::Programs, leapSequence() },
            { "Patterned Steps",
              "S+H sampling the Shaper: a repeating figure, not a wander",
              PresetBank::Programs, patternedSteps() },
            { "Glide Lead",
              "AUTO glide, legato only when two keys overlap; X wheel "
              "vibrato", PresetBank::Programs, glideLead() },
            { "Hollow Fifth",
              "A fifth apart with independent PWM from the X and Y buses",
              PresetBank::Programs, hollowFifth() },
            { "Noise Flute",
              "Noise rung at the cutoff, with the cutoff tracking the keys",
              PresetBank::Programs, noiseFlute() },
            { "Sub and Lead",
              "Osc B parked in BASS as a fixed sub beneath a played lead",
              PresetBank::Programs, subAndLead() },
            { "Shaper Pulse",
              "RUN chops the second path into a rhythm keys cannot "
              "interrupt", PresetBank::Programs, shaperPulse() },
            { "Thunder",
              "WIDE below the keyboard, noise through the overdrive stage",
              PresetBank::Programs, thunder() },
            { "Hollow Ghost",
              "The resonant highpass against the lowpass: the double peak",
              PresetBank::Programs, hollowGhost() },
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

const char* factoryPresetDescription(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return "";
    return presets()[static_cast<std::size_t>(index)].description;
}

PresetBank factoryPresetBank(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return PresetBank::SoundCharts;
    return presets()[static_cast<std::size_t>(index)].bank;
}

EngineParameters factoryPresetParameters(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return EngineParameters {};
    return presets()[static_cast<std::size_t>(index)].parameters;
}

} // namespace ghostar

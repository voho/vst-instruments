// The eleven Sound Charts, voiced from the owner's manual's panel drawings
// and lesson text (printed pages 3-15), and Ghostar's own performance bank
// behind them. Each Sound Chart's comment quotes what the chart teaches;
// each Program's says which mechanism it foregrounds. See GhostarPresets.h
// for what separates the two banks.

#include "DSP/GhostarPresets.h"

#include <array>
#include <cstring>

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
        float modWheel { 0.0f };
        float shaperWheel { 0.0f };
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

    // A dry benchmark for the performance bank: fast 24 dB lowpass punch,
    // with a slightly detuned rectangular undertone behind the saw.
    EngineParameters ghostBass()
    {
        auto p = playablePanel();
        p.octave = MasterOctave::Sixteen;
        p.oscBWaveform = Waveform::RectWide;
        p.slope = UpperSlope::TwentyFourDb;
        p.upperResonance = UpperResonanceMode::Variable;
        p.resonance = 0.58f;
        p.cutoff = 0.32f;
        p.kbAmount = 0.38f;
        p.filterEnvAmount = 0.82f;
        p.filterDecay = 0.3f;
        p.filterSustain = 0.08f;
        p.filterRelease = 0.2f;
        p.loudnessDecay = 0.32f;
        p.loudnessSustain = 0.58f;
        p.loudnessRelease = 0.18f;
        p.filterPathA = 0.72f;
        p.filterPathB = 0.42f;
        p.oscBRange = OscBRange::Unison;
        p.interval = 0.515f;
        p.masterVolume = 0.58f;
        return p;
    }

    // DYNAMIC lets the envelope and keyboard move both filter sections;
    // the free Shaper can then pull only the lower peak away from the upper.
    EngineParameters vowelMotion()
    {
        auto p = playablePanel();
        p.lowerMode = LowerFilterMode::BandPass;
        p.slope = UpperSlope::TwelveDb;
        p.upperResonance = UpperResonanceMode::Variable;
        p.resonance = 0.73f;
        p.cutoff = 0.57f;
        p.lowerOnly = 0.36f;
        p.filterPathA = 0.62f;
        p.filterPathB = 0.48f;
        p.interval = 0.535f;
        p.filterEnvAmount = 0.63f;
        p.filterAttack = 0.12f;
        p.filterDecay = 0.58f;
        p.filterSustain = 0.48f;
        p.loudnessAttack = 0.07f;
        p.loudnessSustain = 0.84f;
        p.loudnessRelease = 0.36f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.3f;
        p.shaperShape = 0.56f;
        p.shaperYTo = ShaperYDestination::FilterL;
        p.masterVolume = 0.48f;
        return p;
    }

    // FORMANT removes the lower filter from keyboard, envelope and wheel
    // buses. A little X motion therefore articulates over one fixed peak.
    EngineParameters fixedReed()
    {
        auto p = vowelMotion();
        p.tracking = TrackingMode::Formant;
        p.oscAWaveform = Waveform::RectWide;
        p.filterPathA = 0.68f;
        p.filterPathB = 0.32f;
        p.lowerOnly = 0.64f;
        p.resonance = 0.79f;
        p.cutoff = 0.49f;
        p.kbAmount = 0.6f;
        p.filterEnvAmount = 0.72f;
        p.filterAttack = 0.18f;
        p.loudnessAttack = 0.12f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.27f;
        p.modXTo = ModXDestination::FilterU;
        p.shaperYTo = ShaperYDestination::Off;
        p.masterVolume = 0.46f;
        return p;
    }

    // The lower filter's inter-stage diode pair is driven hard, then the
    // resulting edge is darkened again by the 24 dB upper section.
    EngineParameters diodeGrowl()
    {
        auto p = ghostBass();
        p.lowerMode = LowerFilterMode::Overdrive;
        p.oscBWaveform = Waveform::RectNarrow;
        p.resonance = 0.66f;
        p.cutoff = 0.39f;
        p.lowerOnly = 0.52f;
        p.filterPathA = 0.92f;
        p.filterPathB = 0.64f;
        p.oscBRange = OscBRange::MinusOne;
        p.filterEnvAmount = 0.72f;
        p.filterDecay = 0.39f;
        p.loudnessSustain = 0.72f;
        p.masterVolume = 0.4f;
        return p;
    }

    // A is the sync master and B is the only audible oscillator. RESET
    // sweeps B through the lock while AUTO glide keeps the lead playable.
    EngineParameters syncRazor()
    {
        auto p = playablePanel();
        p.sync = true;
        p.filterPathA = 0.0f;
        p.filterPathB = 0.72f;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.64f;
        p.cutoff = 0.72f;
        p.resonance = 0.34f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.68f;
        p.shaperShape = 0.1f;
        p.shaperYTo = ShaperYDestination::OscB;
        p.filterEnvAmount = 0.58f;
        p.filterDecay = 0.48f;
        p.filterSustain = 0.44f;
        p.loudnessSustain = 0.86f;
        p.loudnessRelease = 0.32f;
        p.trigger = TriggerMode::Single;
        p.glide = 0.32f;
        p.glideMode = GlideMode::Auto;
        p.masterVolume = 0.58f;
        return p;
    }

    // The triangle-cross ring modulator alone on the Shaper path, struck by
    // RESET's quick rise and long fall instead of by the loudness ADSR.
    EngineParameters ringTemple()
    {
        EngineParameters p;
        p.octave = MasterOctave::Eight;
        p.gateKbd = true;
        p.oscAWaveform = Waveform::Triangle;
        p.oscBWaveform = Waveform::Triangle;
        p.filterPathA = 0.0f;
        // SL4's 6.8k arm rises very steeply near the end of travel; stop just
        // short of the rail so this struck patch retains digital headroom.
        p.shaperPathRing = 0.95f;
        p.shaperPathA = 0.12f;
        p.shaperPathNoise = 0.03f;
        p.brightness = 0.82f;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.69f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.62f;
        p.shaperShape = 0.08f;
        p.masterVolume = 1.0f;
        return p;
    }

    // The rear-jack experience: a keyed dual-filter voice on the left and a
    // free-running BASS/ring/noise apparition through the Shaper on the right.
    EngineParameters splitSeance()
    {
        auto p = playablePanel();
        p.splitPaths = true;
        p.lowerMode = LowerFilterMode::BandPass;
        p.cutoff = 0.5f;
        p.lowerOnly = 0.42f;
        p.resonance = 0.5f;
        p.filterPathA = 0.32f;
        p.filterPathB = 0.12f;
        p.shaperPathRing = 0.95f;
        p.shaperPathNoise = 0.1f;
        p.shaperPathB = 0.75f;
        p.oscBRange = OscBRange::Bass;
        p.oscBWaveform = Waveform::RectWide;
        p.interval = 0.38f;
        p.brightness = 0.55f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.42f;
        p.shaperShape = 0.68f;
        p.loudnessAttack = 0.12f;
        p.loudnessRelease = 0.42f;
        p.masterVolume = 0.48f;
        return p;
    }

    // B sits in WIDE at motor speed and VCA BYPASS makes it truly continuous.
    // Red noise at the X bus wanders both cutoffs without becoming audio.
    EngineParameters motorDrone()
    {
        auto p = playablePanel();
        p.vcaBypass = true;
        p.octave = MasterOctave::ThirtyTwo;
        p.oscBRange = OscBRange::Wide;
        p.oscBWaveform = Waveform::RectWide;
        p.interval = 0.2f;
        p.cutoff = 0.33f;
        p.resonance = 0.58f;
        p.lowerMode = LowerFilterMode::Overdrive;
        p.lowerOnly = 0.58f;
        p.filterPathA = 0.18f;
        p.filterPathB = 0.72f;
        p.filterPathNoise = 0.18f;
        p.modSource = ModSource::RedNoise;
        p.modXTo = ModXDestination::FilterUL;
        p.masterVolume = 0.34f;
        return p;
    }

    // RIPPLE is the direct held-note scan, presented as short two-filter
    // plucks so its lack of octave substitution is immediately legible.
    EngineParameters ripplePluck()
    {
        auto p = playablePanel();
        p.arpeggiator = ArpeggiatorMode::Ripple;
        p.lfoRate = 0.55f;
        p.gateKbd = false;
        p.gateX = true;
        p.filterPathA = 0.6f;
        p.filterPathB = 0.34f;
        p.interval = 0.54f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.3f;
        p.slope = UpperSlope::TwelveDb;
        p.cutoff = 0.48f;
        p.resonance = 0.55f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.filterEnvAmount = 0.8f;
        p.filterDecay = 0.24f;
        p.filterSustain = 0.0f;
        p.filterRelease = 0.12f;
        p.loudnessDecay = 0.2f;
        p.loudnessSustain = 0.08f;
        p.loudnessRelease = 0.1f;
        p.masterVolume = 0.52f;
        return p;
    }

    // S+H samples the asymmetric free Shaper, stepping only the upper filter
    // while FORMANT keeps the lower vocal peak physically disconnected.
    EngineParameters steppedFormant()
    {
        auto p = fixedReed();
        p.modSource = ModSource::SampleHoldY;
        p.modXTo = ModXDestination::FilterU;
        p.lfoRate = 0.58f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.46f;
        p.shaperShape = 0.25f;
        p.cutoff = 0.45f;
        p.lowerOnly = 0.6f;
        p.resonance = 0.76f;
        p.filterPathB = 0.42f;
        p.oscBRange = OscBRange::MinusOne;
        p.filterEnvAmount = 0.64f;
        p.filterAttack = 0.0f;
        p.filterDecay = 0.45f;
        p.filterSustain = 0.65f;
        p.loudnessSustain = 0.86f;
        p.masterVolume = 0.45f;
        return p;
    }

    // OSC B is a selected audio-rate waveform, not an LFO approximation.
    // KBD HOLD fades that cross-modulation into Osc A behind the X wheel.
    EngineParameters crossmodSteel()
    {
        auto p = playablePanel();
        p.oscBWaveform = Waveform::RectMid;
        p.oscBRange = OscBRange::Wide;
        p.interval = 0.67f;
        p.filterPathA = 0.7f;
        p.filterPathB = 0.08f;
        p.slope = UpperSlope::TwelveDb;
        p.cutoff = 0.78f;
        p.resonance = 0.22f;
        p.modSource = ModSource::OscB;
        p.modXTo = ModXDestination::OscA;
        p.shapeXWithY = true;
        p.shaperMode = ShaperMode::KbdHold;
        p.shaperRate = 0.7f;
        p.shaperShape = 0.22f;
        p.filterEnvAmount = 0.54f;
        p.loudnessAttack = 0.01f;
        p.loudnessDecay = 0.5f;
        p.loudnessSustain = 0.88f;
        p.loudnessRelease = 0.22f;
        p.glide = 0.18f;
        p.glideMode = GlideMode::Auto;
        p.masterVolume = 0.52f;
        return p;
    }

    // Independent rectangular-width modulation on the two buses. With the
    // wheels back the fifth is stable; each wheel animates one oscillator.
    EngineParameters pwmChoir()
    {
        auto p = playablePanel();
        p.oscAWaveform = Waveform::RectWide;
        p.oscBWaveform = Waveform::RectNarrow;
        p.interval = 1.0f;                   // up a perfect fifth
        p.filterPathA = 0.62f;
        p.filterPathB = 0.52f;
        p.slope = UpperSlope::TwelveDb;
        p.cutoff = 0.66f;
        p.resonance = 0.28f;
        p.modSource = ModSource::LfoTriangle;
        p.lfoRate = 0.31f;
        p.modXTo = ModXDestination::OscARwm;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.34f;
        p.shaperShape = 0.42f;
        p.shaperYTo = ShaperYDestination::OscBRwm;
        p.filterEnvAmount = 0.58f;
        p.loudnessAttack = 0.16f;
        p.loudnessSustain = 0.85f;
        p.loudnessRelease = 0.46f;
        p.masterVolume = 0.48f;
        return p;
    }

    // Noise excites both the lower boost peak and a highly resonant 24 dB
    // upper section; keyboard tracking turns the pair into glassy pitches.
    EngineParameters noiseGlass()
    {
        auto p = playablePanel();
        p.filterPathA = 0.0f;
        p.filterPathB = 0.0f;
        p.filterPathNoise = 0.82f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.28f;
        p.cutoff = 0.61f;
        p.resonance = 0.9f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.kbAmount = 0.88f;
        p.slope = UpperSlope::TwentyFourDb;
        p.filterEnvAmount = 0.56f;
        p.filterAttack = 0.08f;
        p.filterDecay = 0.4f;
        p.filterSustain = 0.52f;
        p.loudnessAttack = 0.04f;
        p.loudnessSustain = 0.76f;
        p.loudnessRelease = 0.35f;
        p.masterVolume = 0.78f;
        return p;
    }

    // BASS disconnects Osc B from keyboard CV, leaving one fixed fundamental
    // under a played rectangular voice and a fixed FORMANT lower peak.
    EngineParameters subharmonicReed()
    {
        auto p = playablePanel();
        p.oscBRange = OscBRange::Bass;
        p.interval = 0.34f;
        p.oscAWaveform = Waveform::RectNarrow;
        p.oscBWaveform = Waveform::RectWide;
        p.filterPathA = 0.62f;
        p.filterPathB = 0.42f;
        p.cutoff = 0.6f;
        p.resonance = 0.62f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.64f;
        p.tracking = TrackingMode::Formant;
        p.slope = UpperSlope::TwelveDb;
        p.filterEnvAmount = 0.67f;
        p.filterAttack = 0.04f;
        p.filterDecay = 0.45f;
        p.filterSustain = 0.5f;
        p.loudnessSustain = 0.82f;
        p.masterVolume = 0.48f;
        return p;
    }

    // The LFO's X gate clocks both ADSRs and RUN. The filter path supplies a
    // quiet body while the Shaper path supplies the sharper rhythmic edge.
    EngineParameters runChopper()
    {
        auto p = playablePanel();
        p.gateKbd = false;
        p.gateX = true;
        p.lfoRate = 0.56f;
        p.filterPathA = 0.28f;
        p.filterPathB = 0.12f;
        p.shaperPathA = 0.58f;
        p.shaperPathB = 0.4f;
        p.shaperPathRing = 0.15f;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.48f;
        p.brightness = 0.72f;
        p.shaperMode = ShaperMode::Run;
        p.shaperRate = 0.72f;
        p.shaperShape = 0.16f;
        p.cutoff = 0.68f;
        p.resonance = 0.25f;
        p.filterEnvAmount = 0.65f;
        p.filterDecay = 0.15f;
        p.filterSustain = 0.15f;
        p.loudnessDecay = 0.18f;
        p.loudnessSustain = 0.28f;
        p.loudnessRelease = 0.08f;
        p.masterVolume = 0.8f;
        return p;
    }

    // LEAP advances the octave substitution on every note. A RESET ring
    // transient rides beside very short filter and loudness articulations.
    EngineParameters leapMachine()
    {
        auto p = playablePanel();
        p.arpeggiator = ArpeggiatorMode::Leap;
        p.lfoRate = 0.63f;
        p.gateKbd = false;
        p.gateX = true;
        p.oscAWaveform = Waveform::RectMid;
        p.oscBWaveform = Waveform::RectThin;
        p.oscBRange = OscBRange::PlusOne;
        p.interval = 0.45f;
        p.filterPathA = 0.58f;
        p.filterPathB = 0.42f;
        p.lowerMode = LowerFilterMode::BandPass;
        p.lowerOnly = 0.68f;
        p.cutoff = 0.55f;
        p.resonance = 0.48f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.filterEnvAmount = 0.82f;
        p.filterDecay = 0.16f;
        p.filterSustain = 0.0f;
        p.filterRelease = 0.08f;
        p.loudnessDecay = 0.18f;
        p.loudnessSustain = 0.04f;
        p.loudnessRelease = 0.1f;
        p.shaperPathRing = 0.16f;
        p.shaperPathNoise = 0.03f;
        p.brightness = 0.7f;
        p.shaperMode = ShaperMode::Reset;
        p.shaperRate = 0.7f;
        p.shaperShape = 0.15f;
        p.masterVolume = 0.45f;
        return p;
    }

    // HIGHPASS below the upper lowpass forms the hollow double edge. The
    // free Shaper can move only the lower edge from the Y wheel.
    EngineParameters doubleEdge()
    {
        auto p = playablePanel();
        p.lowerMode = LowerFilterMode::HighPass;
        p.lowerOnly = 0.5f;
        p.resonance = 0.7f;
        p.upperResonance = UpperResonanceMode::Variable;
        p.slope = UpperSlope::TwelveDb;
        p.cutoff = 0.65f;
        p.filterPathA = 0.78f;
        p.filterPathB = 0.52f;
        p.oscBRange = OscBRange::MinusOne;
        p.interval = 0.57f;
        p.filterEnvAmount = 0.62f;
        p.filterAttack = 0.08f;
        p.filterDecay = 0.5f;
        p.filterSustain = 0.4f;
        p.shaperMode = ShaperMode::Free;
        p.shaperRate = 0.3f;
        p.shaperShape = 0.55f;
        p.shaperYTo = ShaperYDestination::FilterL;
        p.loudnessAttack = 0.05f;
        p.loudnessSustain = 0.84f;
        p.loudnessRelease = 0.38f;
        p.masterVolume = 0.82f;
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
              "Intentionally silent: the starting panel every lesson begins "
              "from",
              PresetBank::SoundCharts, preparatoryPattern() },
            { "Sound Sources",
              "Every raw source auditioned through a wide-open filter",
              PresetBank::SoundCharts, soundSources() },
            { "Fat Filter", "Gating and the upper filter's resonance",
              PresetBank::SoundCharts, fatFilter() },
            { "Mod Whistle",
              "Vibrato to both oscillators; raise the X wheel",
              PresetBank::SoundCharts, modWhistle() },
            { "Sync", "Hard sync; the Y wheel lets the Shaper sweep Osc B",
              PresetBank::SoundCharts, sync() },
            { "Shake Shape",
              "SHAPE X WITH Y: the Shaper envelopes the X wheel's vibrato",
              PresetBank::SoundCharts, shakeShape() },
            { "Sample & Hold",
              "Metronomic random steps on both filters; raise the X wheel",
              PresetBank::SoundCharts, sampleAndHold() },
            { "Parallel Rectangles",
              "A perfect fifth: the X wheel drives one PWM, the Y wheel the "
              "other", PresetBank::SoundCharts, parallelRectangles() },
            { "Arpeggio", "The arpeggiator clocking gates, noise on the "
              "Shaper path", PresetBank::SoundCharts, arpeggio() },
            { "Noise Scale", "Noise pitched by a filter at full resonance",
              PresetBank::SoundCharts, noiseScale() },
            { "Inverted Guitar", "Plucks from an inverted filter envelope",
              PresetBank::SoundCharts, invertedGuitar() },

            // --- Ghostar Programs: the performance bank ------------------
            { "Ghost Bass",
              "Fast 24 dB lowpass punch over a detuned rectangular undertone",
              PresetBank::Programs, ghostBass(), 0.0f, 0.0f },
            { "Vowel Motion",
              "Dynamic dual peaks; the Y wheel makes the lower vowel roam",
              PresetBank::Programs, vowelMotion(), 0.0f, 0.45f },
            { "Fixed Reed",
              "FORMANT fixes the lower peak; the X wheel moves only the upper",
              PresetBank::Programs, fixedReed(), 0.22f, 0.0f },
            { "Diode Growl",
              "Lower-filter diode overdrive feeding a dark 24 dB lowpass",
              PresetBank::Programs, diodeGrowl(), 0.0f, 0.0f },
            { "Sync Razor",
              "Hard-synced Osc B with AUTO glide; the Y wheel opens the sweep",
              PresetBank::Programs, syncRazor(), 0.0f, 0.62f },
            { "Crossmod Steel",
              "KBD HOLD fades in audio-rate Osc B cross-mod; use the X wheel",
              PresetBank::Programs, crossmodSteel(), 0.34f, 0.0f },
            { "PWM Choir",
              "A fifth apart: X wheel animates A PWM, Y wheel animates B PWM",
              PresetBank::Programs, pwmChoir(), 0.34f, 0.38f },
            { "Ring Temple",
              "A fast RESET envelope strikes the triangle-cross ring modulator",
              PresetBank::Programs, ringTemple(), 0.0f, 0.0f },
            { "Split Seance",
              "Keyed dual-filter voice left, free ring-and-noise ghost right",
              PresetBank::Programs, splitSeance(), 0.0f, 0.0f },
            { "Motor Drone",
              "VCA-bypass WIDE motor drone; X wheel adds red-noise filter drift",
              PresetBank::Programs, motorDrone(), 0.28f, 0.0f },
            { "Ripple Pluck",
              "RIPPLE scans held notes in place with dry, resonant plucks",
              PresetBank::Programs, ripplePluck(), 0.0f, 0.0f },
            { "Leap Machine",
              "LEAP throws each step across octaves with ring-mod transients",
              PresetBank::Programs, leapMachine(), 0.0f, 0.0f },
            { "Stepped Formant",
              "X wheel adds repeating upper-filter steps over a fixed formant",
              PresetBank::Programs, steppedFormant(), 0.42f, 0.0f },
            { "Run Chopper",
              "LFO gates both envelopes while RUN cuts the second audio path",
              PresetBank::Programs, runChopper(), 0.0f, 0.0f },
            { "Noise Glass",
              "Keyboard-tracked resonant noise ringing between two filter peaks",
              PresetBank::Programs, noiseGlass(), 0.0f, 0.0f },
            { "Subharmonic Reed",
              "A fixed BASS oscillator beneath a keyed rectangular formant voice",
              PresetBank::Programs, subharmonicReed(), 0.0f, 0.0f },
            { "Double Edge",
              "Highpass and lowpass make a hollow gap; Y wheel moves its floor",
              PresetBank::Programs, doubleEdge(), 0.0f, 0.38f },
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

float factoryPresetModWheel(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return 0.0f;
    return presets()[static_cast<std::size_t>(index)].modWheel;
}

float factoryPresetShaperWheel(int index) noexcept
{
    if (index < 0 || index >= factoryPresetCount())
        return 0.0f;
    return presets()[static_cast<std::size_t>(index)].shaperWheel;
}

int factoryPresetIndexByName(const char* name) noexcept
{
    if (name == nullptr)
        return -1;
    for (int index = 0; index < factoryPresetCount(); ++index)
        if (std::strcmp(presets()[static_cast<std::size_t>(index)].name, name)
            == 0)
            return index;
    return -1;
}

} // namespace ghostar

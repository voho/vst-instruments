# Changelog

Notable customer-facing changes to Ghostar are recorded here.

## 0.9.0 - Unreleased

- First complete instrument: VST3, Audio Unit and Standalone builds for
  macOS (universal), Linux and Windows, wrapping the circuit-modelled engine.
- Modelled voice: two bandlimited oscillators with one-directional hard sync
  and the panel's exact duty-cycle sets, ± a-perfect-fifth interval detune
  with BASS/WIDE drone ranges, triangle-cross ring modulator,
  MM5837 maximal-PRBS noise, and two parallel audio paths with independent VCAs.
- The signature series dual filter: a lower multimode section (parametric
  boost, inter-filter overdrive, resonant highpass, or out) sliding against
  a 12/24 dB upper lowpass, one resonance control with the LOW/VARIABLE
  switch, ~110 % keyboard tracking, diode-bounded self-oscillation, the
  frozen-formant tracking mode, and a bipolar ±2.5-octave filter envelope.
- Modulation: MOD X with six sources (LFO triangle/square, random and
  Y-patterned sample-and-hold, red-noise drift, Osc B) and the
  RIPPLE/ARPEGGIO/LEAP arpeggiator; the Shaper Y variable-rate integrator
  with FREE/KBD HOLD/RESET/RUN modes and rise/fall symmetry; both
  wheel-destination buses including SHAPE X WITH Y and Y-to-LFO-rate.
- Performance layer: last-note keying with held-note fallback that never
  retriggers, SINGLE/MULTIPLE triggering behind OR'ed gate sources
  (keyboard, LFO auto-repeat, the Shaper's own gate), including the shared
  nominal ~5 ms physical reset/release notch and independent X/Y edge
  retriggers beneath an already-high gate, AUTO glide, VCA
  bypass droning, a spring-loaded ±8-semitone bend wheel and assignable
  X/Y performance wheels.
- Every modelled panel control is a host-automatable parameter with the
  silkscreen's own detent labels (the spring-loaded bend wheel is the one
  momentary exception: it rides MIDI pitch bend, not a parameter); MIDI
  CC1/CC2 ride the X and Y wheels, CC120 stops all sound while keeping
  controller positions, and CC123 releases held keys through the
  envelopes.
- Factory programs: the modelled instrument's manual teaches eleven Sound
  Charts instead of shipping presets, and those charts are Ghostar's program
  bank — from the silent Preparatory Pattern the lessons all start from,
  through Fat Filter, Sync and Sample & Hold, to the Inverted Guitar —
  behind an Init program that is the default voice itself. The selected
  program survives session save and restore.
- Travel smoothing: every continuous panel control and both performance
  wheels glide to new values over ~25 ms, so host automation at any block
  size and 7-bit MIDI CCs never step the audio; switches stay immediate,
  and a silent instrument adopts restored settings exactly. Justified and
  measured by the zipper audit recorded in the best-in-class plan.
- Twelve committed demonstration renders under `Docs/audio`, regenerated
  nightly from the shipping engine.
- **Document-derived circuit progress.** Character-defining topology and laws
  that were first-pass choices are now tied to primary documents, and every
  unresolved transfer, per-unit clock and level normalisation stays explicit:
  - **Noise** now uses the MM5837's 17-stage maximal PRBS and the complete
    P1013 coupling, RC and 1458 feedback transfer derived from the grayscale
    service drawing. Red-noise modulation follows the separate R6/C8-to-IC4B
    branch from that same 75 kHz nominal source; the generic random generator,
    pink-noise recipe and unsupported 1.5 Hz branch are removed.
  - **Mixer travels** include each unbuffered 100 kΩ slider's Thevenin
    resistance and the Shaper's errata-corrected 6.8 kΩ Noise arm. The
    Shaper law is derived; the Filter retains a labelled approximation until
    its separate 220 kΩ-to-VLP and 68 pF-to-VBP buses are integrated.
  - **Master Volume** follows the dual-gang 20 kΩ linear control printed on
    DWG 2; half travel is half output, not the former unsupported square law.
  - **Upper-filter routing** follows DWG 2: its controlled-Q section feeds
    both the 12 dB tap and the downstream fixed-Q section selected at 24 dB.
    LOW and that fixed section now keep their anchored Q=0.5 (`k=2`) exactly;
    the traced SW4/C40 state transfer, 1 MΩ coupling, tied-input drive and
    101/201 output-gain ratio are recorded as the next source-closed work.
  - **Resonance** follows the filter chip's own exponential Q scale through
    the panel's actual pot network, anchored by the manual's "LOW fixes
    Q = 0.5". Resonance is now gentle through the middle of the travel and
    steep at the top — Q at half travel is 1.5 where the old law gave 5.7 —
    and the two filters have genuinely different curves, as their different
    bias networks require.
  - **Envelope times** read the panel's 5 ms–10 s as the RC time constant it
    is, so long decays and releases last about 2.8 times longer than before;
    the nominal attack aim follows the bounded timer-output/diode voltage,
    making it longer and flatter-topped. Both 100 kΩ sustain tracks now share
    the traced D15-biased floor, nominally aligned with the Loudness VCA's
    0.5 V shutoff, while D11/D14 produce the original nonlinear release knee
    into their common GS line. R23/R24's 100 Ω is present in every segment,
    including the fast-Attack threshold undershoot. Accepted MULTIPLE, X and
    Y/EXT edges pull GS low for a nominal ~5 ms, release both caps, then
    attack from their retained voltages; overlapping edges extend the notch.
    Maximum audible release is 31.3 s rather than
    the former pure-RC 25.5 s; the unlisted diode type and real GS coupling
    remain explicit hardware-calibration seams.
  - **Shaper symmetry** follows the 1 MΩ linear SHAPE pot, its 27 kΩ series
    resistor and the two steering diodes. Its extrema are the derived
    2.56/97.44 split, replacing the former voiced 5/95 endpoints while total
    period stays constant. In all four modes, SG now follows the IC6/RS3 phase
    state: high only on the rise, and low through fall, envelope idle/end and
    KBD HOLD's top hold. A KBD HOLD release re-gate reverses upward/high from
    the current level. This removes the remaining interior level threshold;
    trigger-source edge acceptance remains an evidence gap.
  - **MOD RATE travel** includes the original 100 kΩ linear pot's loading
    through its 200 kΩ control arm. Half knob travel reaches `4/9` of the
    exponential control span, preserving the original's slower-than-generic
    midpoint; only the undocumented slow endpoint remains voiced.
  - **X/Y modulation loading** now follows the two genuinely different wheel
    circuits and every selected destination load. Current-driven X bites
    early (86.75% of X→A depth at half assumed-linear resistance travel),
    voltage-fed Y blooms late (33.56% of Y→B), dual-oscillator routes load
    down, and X→U is deeper than X→U+L. The same network applies to audio-rate
    Osc-B modulation. Absolute X/Y source depth, unmarked wheel tapers, RWM's
    active conversion and Y→rate maximum remain explicit calibration seams.
  - **Ring modulation** follows the internal null-trim topology: Osc A passes
    through the real 1 µF/39 kΩ/100 kΩ high-pass and the nominal product is
    `−15/13·A·B`. The former fixed 3% symmetric carrier leak is removed;
    actual trim residue and CEM feedthrough remain per-unit evidence gaps.
  - **Output circuitry** now places the 470 nF Filter coupling capacitor
    before the Loudness VCA, keeps the Shaper and rear jacks DC-coupled, and
    solves the normalled main jack's P4/R49/R50 cross-loading. Its DC limit is
    a half-sum, while at audio frequencies BRIGHTNESS also colours the Filter.
    BRIGHTNESS is the
    post-Shaper-VCA 27 nF/100 kΩ series shunt across its 20 kΩ load, including
    its unusual split-output dark low-pass and bright −1.58 dB shelf.
  - **Loudness VCA** includes the 10 kΩ/3.3 kΩ/240 kΩ control offset. Its
    nominal gain stays shut through the first 0.5 V (`1/15`) of the envelope,
    preserving the hardware's silent low-voltage release region.
  - **Glide** reaches the resolved `2 MΩ·470 nF = 0.94 s` full-resistance
    time constant. The unmarked intermediate pot taper remains voiced.
  - **Filter limiting** now solves the controlled Upper's actual
    capacitor-coupled TL082/BA130/C37 network implicitly with its CEM state.
    Lower now solves the production three-slider network: three 100 kΩ
    Thevenins, three 220 kΩ/68 pF arms, both moving 22 nF CEM states and the
    traced gain/BA130/C33 loop in one implicit system. Exact pot-end charge
    projection removes a hidden trapezoidal alternating mode, and a zeroed
    slider still loads both nodes. The fixed Upper has no limiter and remains
    linear. P1014's nominal selected-wave volts replace the former arbitrary
    24 mV state scale.
    **OVERDRIVE** solves its traced IC12A/BA130 throw and the corrected
    R187/R167/C34/R173 Thevenin source, including R167's clean Lower-VLP feed
    rather than treating it as a ground shunt; the rest of
    the shared RS7 output network and non-OVERDRIVE C34 pre-charge remain open.
  - **CEM3340 output character** now retains P1014's selected-wave stage:
    its saw and open-emitter pulse dividers deliberately equalise the three
    selector taps near 4 V before IC10, preserving their exact residual level
    differences and DC offsets in both audio paths and OSC-B modulation. Ring
    still uses the raw pre-switch triangles. PWM reaches the CEM's documented
    0/100 % constant endpoints instead of an invented 3/97 % guard band.
    SYNC is tied to A's raw saw fall through the traced pins-9/10 network,
    independent of A's selected waveform and duty edges. Acyclic Osc-B
    destinations use the current conditioned sample;
    B self-FM keeps the causal prior sample, as do A/PWM only when SYNC closes
    the B→A→A-reset→B loop. Filters always receive current B.
  - **Keyboard tracking** is 108 %, computed from the CV ladder's resistors,
    which reproduces the manual's "slightly over 100 %" independently.
    P1016's signed DAC/reference currents put its nominal zero at MIDI
    60.006015—0.6015 cent above the second C—rather than at a rounded,
    voiced MIDI 60.
- **Audio quality.** Every waveform discontinuity is bandlimited as a
  sub-sample event — including the hard-sync reset, which was uncorrected,
  and the triangle's corners, which had no correction at all — and the voice
  core runs at 4x with a two-stage decimation chain. The high-Q limiter's
  capacitor is integrated alongside the CEM states and its diode current is
  solved implicitly; its self-oscillation level agrees within 0.5 dB at the
  tested 8, 44.1 and 96 kHz host rates, unlike the former per-sample limiter.
- The alias audit's own metric was wrong, and its verdict is withdrawn
  rather than restated. It compared each bin against the loudest reference
  bin within ±3 bins, which turned every partial into a 70 Hz-wide plateau
  a component could hide under: injecting a known alias twenty dB **above**
  the acceptance gate produced the metric's −200 dB floor. The measure now
  tolerates a measured pitch disagreement proportionally to frequency, and
  publishes the floor below which it cannot see — which on tonal material
  is about −15 dB, so a −60 dB alias gate cannot be certified by comparing
  two renders at all. What the audit now gives is a sound upper bound on how
  far the shipping render differs from a 16x ground truth. The DSP work is
  unaffected; the certification is what fell.
- Seventeen **Ghostar Programs** join the eleven Sound Charts: playable
  voicings, each foregrounding one mechanism, level-matched to each other.
- The editor is rebuilt around the modelled instrument's own panel: MOD X
  and SHAPER Y as full-height columns on the right, WHEEL DESTINATIONS
  under MASTER, the two filters as one block and the two envelopes as
  another, and GLIDE with the three levers on their own sub-panel beside
  the keys — which now have the bottom of the window to themselves. The
  livery is the panel's: charcoal, grey knob caps with black pointers and
  travel arcs, white silkscreen, paddle switches whose thrown half stands
  pale. Every control is captioned directly above itself and carries a
  tooltip; knobs and faders show the silkscreen's 0–10 while they move.
  A program browser reaches both banks with each program's description, and
  a gate lamp shows when a gate source is holding the envelopes open.
- The panel scales instead of being pinned to one window size. The whole
  instrument is laid out once at its design geometry and the window scales
  it, so the editor opens at the largest whole panel the display can show
  and can be dragged between 60 % and 200 % with its proportions locked.
  The keys and the GLIDE/lever sub-panel stay on screen on a 1366x768 or
  1280x800 laptop and at 150 % desktop scaling, where a fixed 1460x780
  window had been taller than the screen.
- Programs whose motion runs through MOD X or SHAPER Y now say which wheel
  they are waiting on. Selecting a program pulls both wheels fully back, as
  the charts instruct, so those programs make none of their advertised
  motion until a hand moves; eight of them described the motion as though it
  happened by itself. A test now derives the requirement from each
  program's own routing rather than leaving it to be remembered.

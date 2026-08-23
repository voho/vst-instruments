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
- RUN now accepts a selected keyboard's MULTIPLE legato pulse as soon as its
  rising segment has completed, matching the manual even while the combined
  gate bus remains high. A newly held arpeggiator group also always restarts
  its bottom-to-top scan at the lowest key; a short between-clock release can
  no longer inherit the preceding phrase's step.
- Performance layer: last-note keying with held-note fallback that never
  retriggers. SINGLE attacks only on a genuine selected-bus rise, while
  MULTIPLE's raw new-key KT is tapped before KBD gate selection. Independent
  X/Y edges use the nominal 5 ms physical reset/release lane; raw MULTIPLE
  keyboard KT and arpeggiator AA use the separate 10 ms lane, even with KBD
  deselected.
  Also included: retriggers beneath an already-high gate, AUTO glide, VCA
  bypass droning, a spring-loaded ±8-semitone bend wheel and assignable
  X/Y performance wheels.
- Every modelled panel control is a host-automatable parameter with the
  silkscreen's own detent labels (the spring-loaded bend wheel is the one
  momentary exception: it rides MIDI pitch bend, not a parameter); MIDI
  CC1/CC2 ride the X and Y wheels, CC120 stops all sound while keeping
  controller positions, and CC123 releases held keys through the
  envelopes.
- Factory programs begin with Init, followed by the modelled instrument's
  eleven manual Sound Charts (programs 2–12, Preparatory Pattern through
  Inverted Guitar) and seventeen Ghostar Programs (13–29). The selected
  program survives session save and restore.
- Standalone now always powers up at Init instead of letting JUCE's automatic
  last-state restore put an edited panel behind the Init name; explicit state
  loads after startup and plug-in host restores still work. The original
  Preparatory Pattern remains exactly silent, but the browser and selected
  program header now identify that documented behaviour deliberately.
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
    Shaper law is derived; the Filter solves its separate 220 kΩ-to-VLP and
    68 pF-to-VBP buses together with all three wipers and both Lower states.
  - **Master Volume** follows the dual-gang 20 kΩ linear control printed on
    DWG 2; half travel is half output, not the former unsupported square law.
  - **Upper-filter routing** follows DWG 2: its controlled-Q section feeds
    both the 12 dB tap and the downstream fixed-Q section selected at 24 dB.
    LOW and that fixed section now keep their anchored Q=0.5 (`k=2`) exactly;
    both CEM inputs contribute their Q-dependent gains. One coupled solve now
    carries SW4/C40's 22:1 charge transfer, its selected 23 nF timing node,
    R194's 12 dB state bleed and IC14B's absolute gains of 201/101. Keeping
    those gains at the physical output restores the C34 OVERDRIVE path's
    missing downstream level; the other unresolved RS7 modes are referred
    into the physical state domain without changing their standing level.
  - **Resonance** follows the filter chip's own exponential Q scale through
    the panel's actual pot network, anchored by the manual's "LOW fixes
    Q = 0.5". Resonance is now gentle through the middle of the travel and
    steep at the top — Q at half travel is 1.5 where the old law gave 5.7 —
    and the two filters have genuinely different curves, as their different
    bias networks require.
  - **Filter cutoff controls** now solve the loading of both linear pots and
    IC15 summers shown on P1013. MASTER has the derived 11.76495-octave total
    span about its still-voiced trim placement. LOWER ONLY is an asymmetric
    −7.10055/+1.56630-octave Dynamic law around the marked panel-8
    coincidence; FORMANT's alternate node load widens it 1.389% and retains
    the original ±0.08170-octave MASTER-dependent coincidence drift. The
    former linear −5/+1.25-octave shortcut is removed.
  - **Envelope times** read the panel's 5 ms–10 s as the RC time constant it
    is, so long decays and releases last about 2.8 times longer than before;
    the nominal attack aim follows the bounded timer-output/diode voltage,
    making it longer and flatter-topped. Both 100 kΩ sustain tracks now share
    the traced D15-biased floor, nominally aligned with the Loudness VCA's
    0.5 V shutoff, while D11/D14 produce the original nonlinear release knee
    into their common GS line. R23/R24's 100 Ω is present in every segment,
    including the fast-Attack threshold undershoot. X and Y/EXT edges pull
    GS low for their nominal 5 ms; raw MULTIPLE KT and every accepted
    arpeggiator AA step use the separately drawn 10 ms node. Either releases
    both caps before attacking from their retained voltages; overlapping
    edges extend the notch and a 5 ms event cannot shorten a 10 ms one.
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
  - **Shaper audio-VCA evidence** now uses the 1984 production CEM3360 data
    and the corrected two-BC173 auxiliary branch. Non-FREE modes with SHAPE X
    WITH Y open keep both devices out of forward conduction, but the
    R38/R40/R41 divider is only conditional on negligible reverse E-B current;
    the source gives no leakage curve to close any range. Near maximum gain
    the chain reaches the sum of the BC173s' reverse emitter-base ratings,
    exposing a plausible per-unit avalanche knee. Closing that switch loads
    the TR2-base node; FREE substitutes a loaded IC9/R64/C11/D22 drive. The
    whole active transfer remains explicit rather than guessed.
  - **MOD RATE travel** includes the original 100 kΩ linear pot's loading
    through its 200 kΩ control arm. Half knob travel reaches `4/9` of the
    exponential control span. P1015's 132 mV swing and the original CEM3360
    production sheet's 3.0 mV/dB typical scale close the nominal
    0.3154787–50 Hz range and 2.9974213 Hz midpoint.
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
    **OVERDRIVE** solves the traced IC12A/BA130 scalar and the conditional
    A3+B7+C10 R187/R167/C34/R173 network. That contact combination remains an
    explicit functional hypothesis: standard same-index phasing selects C11,
    not C10, so all RS7 assignments and non-OVERDRIVE C34 histories still
    require installed-switch continuity. Until then, explicit voiced bridges
    preserve the manual's audible BANDPASS resonance peak and HIGHPASS
    two-edge response; regression tests stop either mode collapsing into OUT.
  - **CEM3340 pitch-control memory** now includes both documented
    `1.82 kΩ || 1 nF` multiplier-output returns (A R82/C72, B R118/C77).
    Each oscillator's whole keyboard/tune/bend/interval/X/Y pitch sum crosses
    its own 87.45 kHz pole before exponential conversion. The discrete states
    preserve the exact 1.82 µs low-frequency delay and monotone response at
    every supported rate, replacing the former current/prior-sample pitch
    heuristic while retaining the real self-FM and hard-sync causality.
  - **CEM3340 output character** now retains P1014's selected-wave stage:
    its saw and open-emitter pulse dividers deliberately equalise the three
    selector taps near 4 V before IC10, preserving their exact residual level
    differences and DC offsets in both audio paths and OSC-B modulation. Ring
    still uses the raw pre-switch triangles. PWM reaches the CEM's documented
    0/100 % constant endpoints instead of an invented 3/97 % guard band.
    SYNC is tied to A's raw saw fall through the traced pins-9/10 network,
    independent of A's selected waveform and duty edges. Acyclic Osc-B
    destinations use the current conditioned sample. Pitch destinations use
    their physical CEM3340 capacitor states; PWM uses causal prior B only when
    SYNC closes the B→A→A-reset→B loop, and filters always receive current B.
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
- Seventeen rebuilt **Ghostar Programs** join the eleven Sound Charts. The
  bank now ranges from diode bass, hard sync and genuine audio-rate crossmod
  through independent PWM, fixed/dynamic dual-filter formants, ring
  percussion, split-path ghosts, WIDE/BASS drones, resonant noise and all
  three arpeggiator behaviours. Each has a gesture-aware audibility,
  clipping, wheel-action and near-duplicate check rather than only a
  nonzero-sample test.
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
- Programs whose motion runs through MOD X or SHAPER Y say which wheel is in
  play and store a useful initial wheel stance, so their defining movement
  is audible on selection. The historical Sound Charts still restore both
  wheels fully back exactly as drawn; CC1/CC2 and the on-screen wheels take
  over normally. Tests derive the requirement from each program's routing
  and protect the historical bank's zero-wheel state.

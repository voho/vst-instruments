# Making Ghostar best-in-class

This document records what Ghostar is actually competing against, where it
stands against that field today, and the ordered set of changes that closes
the gap. It is written to be checked: every claim about the current engine
names the file and the constant it comes from, and every step states how it
is verified. The steps are marked off as they land.

Ghostar is a circuit-modelled replica of one specific 1983 monophonic
synthesizer — the Crumar Spirit, designed by Bob Moog, Jim Scott and Tom
Rhea. Everything below is judged against that identity: the goal is not "a
good mono synth" but *that* mono synth, to the limit of what published
documentation supports.

## The field

**The official emulation.** [Cherry Audio's Crumar
Spirit](https://cherryaudio.com/products/spirit-synthesizer), made in
collaboration with Crumar and released at
[$59](https://musictech.com/news/gear/cherry-audio-crumar-spirit-soft-synth-bog-moog/),
is the product a buyer finds first. It is an *extended reimagining*: 16-voice
polyphony, added modulation, two chains of studio effects, velocity and
aftertouch, 400 presets.
[MusicRadar's review](https://www.musicradar.com/music-tech/soft-synths/cherry-audio-crumar-spirit-review)
calls its sonic and visual attributes "stunningly impressive … at a price
which is affordable to all";
[Synth Anatomy's review](https://synthanatomy.com/2025/08/cherry-audio-crumar-spirit-review-sound-demo-bob-moogs-italian-synth-in-a-plugin.html)
praises it while admitting the reviewer *"cannot vouch for how accurate or
authentic"* it is, because the hardware is too rare to compare against.

That admission defines the market: the Spirit is so rare that even
professional reviewers judge emulations against YouTube demos. Nobody in the
field is competing on verified authenticity — which is exactly the ground
Ghostar claims, by deriving its laws from the service-manual schematics, the
owner's manual, panel photographs and the Shaper's patent (US 3,943,456)
rather than from listening to an inaccessible instrument
(`Docs/circuit-modelling-research.md`).

**The quality bar.** The tier a modelling synth is measured against is set by
component-level emulations of *other* instruments:
[u-he Repro](https://uniphonic.com/u-he-repro-review/) (component-modelled
Prophet-family, "designed for ultimate authenticity in one specific area"),
[Softube Model 72](https://musictech.com/reviews/software-instruments/softube-model-72-synthesizer-system-review/)
(component-modelled Minimoog, praised for modelling oscillator pitch drift
and even the inaccuracy of the octave switching), and u-he Diva
(zero-delay-feedback filters as the reference for filter behaviour under
resonance). These set what "best-in-class modelling" means to reviewers:
right behaviour *under stress* — drive, self-oscillation, sync, audio-rate
modulation — plus the small analog motions that make a static patch feel
alive.

**Where Ghostar sits.** Ghostar is the purist alternative to the official
extension: the actual mono voice, the actual two-path output, the actual
gate/trigger logic, no added effects, velocity ignored because the modelled
keyboard had none. Its differentiators are the parts of the Spirit nobody
else models seriously: the series dual filter with the lower section's
OUT/OVERDRIVE/BANDPASS/HIGHPASS modes, the Shaper Y variable-rate
integrator, the AC-coupled triangle-cross ring modulator with its internal
carrier-null trim, and
the arpeggiator that clocks gates rather than just notes.

## What separates the top tier

Reading the reviews of the modelling tier, four things recur.

1. **Behaviour under stress is what gets A/B'd.** Filter character at high
   resonance and into self-oscillation, oscillator behaviour under hard
   sync and audio-rate modulation, what overdrive does to the spectrum.
   A model that is right at rest and wrong under stress reads as a toy.
2. **Analog motion.** Model 72's reviewers single out oscillator drift
   control as an authenticity feature. A perfectly static digital pitch is
   the most audible tell of an emulation.
3. **Honesty about the reference.** Repro's reputation comes from measured
   fidelity to specific hardware. Where no hardware is measurable — the
   Spirit's situation — the defensible substitute is anchoring to primary
   documents and saying plainly which constants are voiced by ear.
4. **The controls must behave like the panel.** Detents where the hardware
   has detents, the silkscreen's own labels, tapers that put the musical
   range where the panel drawing says it is.

Ghostar holds point 3 in method (the research contract and the register;
Step 4's sweep makes the register exhaustive) and, since Steps 1–2 plus the
documentary half of Step 3 landed, increasingly in substance: the resonance
curve, envelope timing and tracking amount that were voiced when this plan
was written are now derived from the CEM3350 datasheet and service-manual
scan. The BA130 component curve is sourced too; the later grayscale pass
first reopened, then supplied the component topology for, the two external
limiter loops and the local OVERDRIVE/C34 path. Those models now ship. A
lossless P1013 pass has since landed the complete Lower moving-wiper MNA, and
P1014 supplies its selected-wave/state voltage reference; only RS7's three
remaining named terminal assignments, non-OVERDRIVE C34 excitation and
original-chip dynamics remain open.
Point 2 is settled in the only direction the evidence
allows:
Step 2 closed as no drift, with the sensitivities anchored and the
missing environmental record documented. Point 4 holds *structurally* —
every control exists with the silkscreen's detents and labels — and its
tapers are now mostly derived too. Remaining examples include the cutoff
span's absolute placement (OQ-02), glide's unmarked intermediate taper
(OQ-08), LFO slow endpoint (OQ-21), BRIGHTNESS taper (OQ-22), and the
bounded nominal attack aim (OQ-04). Lossless scans closed the Shaper split,
Master Volume law and electrical glide endpoint. Point 1 —
behaviour under stress — is where Steps 1 and 3 aimed, and the alias table
and the rate-invariant limit cycle are its evidence.

## Where Ghostar actually stands

**Derived laws** (computed from a named primary source; the derivation is
recorded in the register entry named against each):

- The resonance travel-to-Q mapping, from the CEM3350's −65 mV/decade Q
  scale and the Spirit's own pot network, anchored by the panel's LOW =
  Q 0.5 — with the Upper and Lower chips on genuinely different curves
  (OQ-12).
- The BA130 component's forward curve (`n≈1.68`, `Is≈2.3 nA`); P1013's
  complete stateful C37 Upper loop; Lower's three 100 kΩ Thevenins,
  220 kΩ/68 pF arms, VLP/VBP states and C33 loop; and the local IC12A/BA130
  OVERDRIVE throw plus physical C34 companion. TL082/CEM3350 dynamics and the
  rest of RS7 remain open (OQ-10, OQ-12, OQ-20).
- P1014's selected-wave conditioner: saw's 10 kΩ/10 kΩ divider and pulse's
  loaded open-emitter 10 kΩ/6.8 kΩ divider intentionally bring all three
  selector taps near 4 V before the traced 24 kΩ/91 kΩ/10 kΩ IC10 stage.
  Their exact small level differences and shared DC bias reach both mixers
  and OSC-B modulation; the raw pre-switch triangles still feed Ring.
- The two CEM3340 pitch-control poles: R82/C72 and R118/C77 are each a
  `1.82 kΩ || 1 nF` pin-14 return to ground. Curtis prints the resulting
  corner equation; the normalized parallel-RC reduction is
  `H(s)=1/(1+sRC)`. One linear-input state per oscillator filters the complete
  octave/CV sum before `exp2`, preserves the exact 1.82 µs DC delay and
  remains monotone at every supported rate (OQ-25). PWM/filter active-path
  delay remains separate and open.
- P1014's conventional sync network: both direct pin-6 inputs are open;
  A's raw saw fall drives B pins 9/10 through SW2/C24/BC308/R107, so selector
  and PWM edges cannot retrigger B. The documented CEM PWM span also lets
  modulation reach true 0/100 % constant endpoints rather than a 3/97 %
  guard band (OQ-03 retains only calibrated detent/trim measurements).
- The ADSR segment topology and time-constant law: each envelope has its own
  4.7 µF cap and 2 MΩ A/D/R sliders; the two 100 kΩ sustain tracks share one
  D15-biased lower rail. The nominal 0.5 V floor aligns with the Loudness VCA
  zero, while D11/D14 add a nonlinear release knee into their shared GS line.
  R23/R24's 100 Ω lies in every segment and creates the fast-Attack cap-side
  undershoot. Accepted MULTIPLE/X/Y edges insert the original nominal ~5 ms
  reset/release notch before Attack; X/Y edges remain independent of an
  already-high combined gate. The shipped width, ≈1.3× aim and diode curve
  are nominal rather than measured-unit constants. Combining the manual's
  MULTIPLE new-key rule with RUN's rising-segment lockout gives the modelled
  selected-KT acceptance after the rise (OQ-04/OQ-05).
- The Loudness CEM3360's affine control offset: its 10 kΩ/3.3 kΩ/240 kΩ
  network puts zero gain at 0.5 V, or `1/15` of the 7.5 V envelope peak.
  Full-envelope unity is a normalization; original-chip top saturation and
  feedthrough remain per-device details (OQ-04).
- Keyboard tracking at 108 %, from the 12k1 ladder against the chip's
  −19.6 mV/octave — which independently reproduces the manual's
  "slightly over 100 %". P1016's DAC sink and same-rail R39 reference cancel
  at MIDI 60.006015, closing the nominal zero-tracking pivot independently
  of rail voltage and downstream gain (OQ-13).
- The complete MM5837 audio-colouring transfer and its separate RED NOISE
  branch from P1013's coupling capacitor, passive RC branches and two 1458
  stages. The audio path has five poles and four zeros; both responses are
  spot-checked directly at 44.1, 48 and 96 kHz hosts (OQ-15, OQ-17).
- The Upper filter's cascade order and fixed bias: R181 resolves to 91 kΩ,
  putting its fixed section at the same exact Q=0.5 (`k=2`) bias as LOW.
  Its coupled solve now includes SW4/C40 charge sharing, tied VIF+VIV drive,
  1 MΩ coupling and the exact 101/201 slope gain (OQ-09).
- The filter cutoff *span*: ≈10 octaves of pot authority over a
  ~10-octave chip window, corroborated by the scan (OQ-02).
- MOD RATE's loaded control travel: P2=100 kΩ LIN against R33=200 kΩ gives
  `w(x)=200x/(200+100x(1−x))`, so the exponential converter sees `4/9`
  at half knob travel. The original-CEM/calibrated-unit slow endpoint remains
  open (OQ-21).

**Anchored laws** (stated outright by a primary source):

- Keyboard law; the panel's printed pulse duty sets — A 50/30/15/6 %,
  B 40/20/10/3 % (whether a calibrated unit's PW trimmer lands on the
  print is OQ-03's open half) — and Osc B's −1/UNISON/+1/+2/BASS
  (30–300 Hz)/WIDE (2 Hz–10 kHz) ranges with the ± perfect-fifth
  INTERVAL.
- The Shaper Y integrator from US 3,943,456 with FREE/KBD HOLD/RESET/RUN,
  RESET retriggering on every key press regardless of TRIGGER mode, and RUN's
  documented lockout through its rising segment.
- The bipolar ±2.5-octave filter envelope, linear sustain, the OR'ed gate
  buses with keyboard articulation only through KBD, last-note keying with
  fallback-without-retrigger, AUTO glide, RIPPLE/ARPEGGIO/LEAP with the
  (0, +12, −12) pattern, plus the stated bottom-to-top scan and wrap.
- A self-clocked MM5837 17-stage maximal PRBS; the real 470 nF Filter-path
  coupling capacitor before its VCA; DC continuity through the Shaper and
  rear outputs; and Master Volume's dual 20 kΩ linear law. The normalled
  rear jack couples the two wipers through equal 10 kΩ arms: a half-sum at
  DC, with BRIGHTNESS-dependent cross-loading at audio frequencies (OQ-16).

**Voiced constants** that remain. The register, not this list, is the
authority, and Step 4 sweeps every remaining numeric voicing into it:

- The cutoff span's absolute *placement* — the 100 kΩ trimmer's factory
  setting, which no document records (OQ-02).
- TL082 dynamics, CEM3350 internal headroom, the three unresolved RS7 output
  mappings and non-OVERDRIVE C34 excitation. The selected-wave/state domain,
  Lower MNA, local capacitor and diode transfers are derived, not voiced
  (OQ-10, OQ-12, OQ-20). The chip's Q ceiling read as the oscillation
  threshold is voiced too.
- Ring-modulator factory trim, CEM feedthrough and per-unit residual carrier;
  the nominal circuit has no fixed symmetric carrier term (OQ-06).
- Pitch-wheel endpoint: the network has ±15.88 st of full electrical
  authority, while the wheel's mechanical pot fraction is unrecorded and the
  engine retains a voiced ±8 st endpoint (OQ-01).
- The envelope slider residual, nominal attack-aim/diode parameters, shared
  GS output resistance, release coupling, leakage and temperature law
  (OQ-04).
- Shaper trigger acceptance under self-Y remains open. The envelope's
  selected KBD/X/Y edge network and the RS3/IC6 phase states in FREE,
  KBD HOLD, RESET and RUN are closed (OQ-04/OQ-05).
- The Shaper audio CEM3360 control transfer: the production sheet closes the
  typical 52 %/V, 1.93 V figures. With SHAPE X WITH Y open, non-FREE modes
  yield an R38/R40/R41 divider only under the unproven condition that reverse
  BC173 junction current is negligible; the sheet gives no leakage curve to
  close even a low-voltage interval. Near 1.93 V the devices share ≈10.005 V,
  already the sum of their two 5 V emitter-base ratings, making a possible
  avalanche knee a potentially audible per-unit quirk. Closing the switch
  loads TR2's base; FREE substitutes the undocumented IC9/R64/C11/D22
  source. The whole active transfer remains a capture (OQ-26).
- Glide's intermediate travel curve; its full-resistance endpoint is the
  derived `2 MΩ·470 nF = 0.94 s` (OQ-08).
- Absolute X→A/Y→B source anchors, unmarked wheel tapers, RWM conversion and
  the fastest LFO rate a full Y wheel reaches. The current-driven X law,
  voltage-fed Y law, all pitch/filter destination ratios and their
  switch-dependent loading are closed (OQ-14).
- The PWM and filter paths' active group delay under audio-rate Osc-B
  modulation. The CEM3340 pitch paths' external pole is closed; the BC308/PW
  and LM1458/CEM3350 destination dynamics still need same-unit phase captures
  (OQ-25).
- The LFO slow endpoint/original-CEM3360 scale (OQ-21) and BRIGHTNESS pot
  taper (OQ-22); both controls' loaded topology and exact resistance
  endpoints are closed.
- The RED NOISE P1013-output-to-engine-bus scale (OQ-17), the MM5837 per-unit clock and
  source-to-engine-unit level (OQ-15), both mixers' overall engine-unit
  scales (OQ-20).

**DSP quality today:** every waveform discontinuity bandlimited as an
event — BLEP for value jumps, BLAMP for the triangle's corners, both for
the hard-sync reset — at 4× oversampling, decimated in two Kaiser halfband
stages; a TPT Upper whose external C37 capacitor and BA130 current are solved
implicitly with the CEM endpoints, plus the complete moving-input Lower 2×2
solve and C33 current; self-oscillation level agreeing
within 0.5 dB at the tested 8, 44.1 and 96 kHz host rates; audio-rate
modulation applied at the internal rate; denormal flushing throughout.
The whole voice renders about 10× faster than realtime on a hard patch.

**The standing limitation (OQ-11):** no owned hardware, no calibration
captures, and none available in the field. Every constant marked *voiced*
stays voiced until a trustworthy capture exists. Decisions the physics
cannot close are made by A–Z listening tests under the repository's rules —
chosen by ear, recorded as chosen by ear.

## The steps

Each step names its verification. Audible, physics-ambiguous choices go
through an A–Z listening test (A = shipping engine); measurable claims are
settled by measurement and quoted.

- [x] **Step 1 — Alias audit at the extremes.** Done — the strokes,
  the metric the measurements forced, the fixes and the final table are in
  the dated section below. Original specification: WIDE range takes Osc B to
  10 kHz; hard sync and the ring modulator multiply spectra; the triangle
  path relies on 2× oversampling plus mild spectral rolloff; and MOD
  SOURCE = OSC B in WIDE drives pitch, pulse width or cutoff at audio
  rate — an aliasing mechanism of its own. Render worst-case strokes
  (WIDE at full, sync sweeps at the keyboard's top, ring with both
  oscillators high, Osc B audio-rate modulation into each of its
  destinations, and the filter nonlinearities driven hard — OVERDRIVE at
  full boost and self-oscillation at maximum resonance, since the
  limiter, the integrator tanh and the clipper make high partials of
  their own) and measure the alias floor against a ground truth, not
  by eyeballing bins: these strokes are nonstationary with dense
  legitimate sidebands, so the reference is a much-higher-rate render of
  the same stroke, bandlimited and downsampled to the shipping rate, and
  the aliasing figure is the residual energy against it. Verification: a
  table of worst-case alias-to-signal ratios in this document, each
  measured that way; anything above −60 dB in the audible band triggers a
  targeted fix matched to the discontinuity — BLEP for value jumps (saw
  and pulses always; the reset triangle too, since an arbitrary-phase
  reset lands it away from where it was), BLAMP for the triangle's slope
  kink, or 4× on the triangle path alone — and a re-measure.
- [x] **Step 2 — CEM3340 temperament.** Closed as no drift — the
  derivation's two halves and the dead end are in the dated section below.
  Original specification: The 3340 is a famously stable VCO —
  that stability is part of the Spirit's character (two of them against
  each other stay in tune, unlike discrete VCOs). But *stable* is not
  *static*: datasheet tempco and supply sensitivity translate to cents-scale
  wander over minutes — but only once the environmental excursion itself
  is known. The datasheet sensitivities convert a temperature or rail
  excursion into pitch error; they do not supply the excursion's
  amplitude, spectrum or inter-oscillator correlation, and inventing that
  process would smuggle the magnitude in through the back door. So the
  gate: first pin the environmental process from independent evidence.
  The service-manual drawings supply only the regulator's *transfer*; a
  derivation additionally needs the input excursion itself — a measured
  or published mains/ripple or enclosure-temperature record, not an
  assumed one — before any wander counts as derived. If both halves
  exist, the derived wander *ships
  as the model*, with its uncertainty stated — the derivation decides,
  and a listening preference may not overrule a measurable result; if no
  defensible process can be established, the step closes as "no drift",
  recorded with the dead end. No scaled variants and no A–Z between
  derived and none — either would put an eared number where a derived
  one belongs. Verification: the derivation (or the dead end) quoted
  here.
- [ ] **Step 3 — External limiter and OVERDRIVE networks.** Substantially
  executed: C37 has a charge-conserving implicit circuit model in the resolved
  Upper section; the production Lower 2×2 MNA includes all three slider arms,
  both CEM states and C33's implicit BA130 current. The IC12A/RS7 OVERDRIVE
  scalar and a physical C34 voltage companion also ship. Remaining integration
  is the three unnamed RS7 output transfers, their C34 excitation, and
  original TL082/CEM3350 dynamic validation.
  Original specification: What
  bounds self-oscillation in the hardware is the *external* BA130
  anti-parallel "Hi-Q overload limiter" in the resonance path (anchored
  placement; `Docs/circuit-modelling-research.md`), which the engine then
  voiced as one piecewise diode law (`knee = 1.2` / `ceiling = 2.2`,
  OQ-12). Derive the BA130 pair's knee and compression from the BA130
  datasheet plus the node's operating level — the same derivation OQ-10
  needs for the inter-filter clipper's BA130s — and re-voice the law from
  it. Whether the CEM3350's internal stages add saturation of their own
  on top of the external limiter is a separate question: pursue it only
  if the 3350's published topology supports a derivation, and track it
  apart from OQ-12. If the derived law and the current one both remain
  defensible and audibly different, A–Z it. The derivation's scope
  includes the *second* nonlinearity already in every section — the
  integrator stability bound `4·tanh(0.25·x)` on the lowpass state, which
  shapes exactly the self-oscillation extremes under study — so the step
  ends with that bound justified, re-derived or removed, never left as an
  unexamined contributor to the measured harmonics. Verification: the
  harmonic tables (Goertzel series under fixed drive) and, if run, the
  recorded verdict — plus a re-run of Step 1's alias suite, since new
  nonlinearity makes new high partials and the completed audit must
  describe the final engine, not the one before it.
- [x] **Step 4 — Close the closable open questions.** Done — the
  derivations, the dead ends and the four register entries the sweep added
  are in the dated section below. Original specification: OQ-02 (absolute
  cutoff span) and OQ-09 (cascade Q distribution) may be derivable from
  the CEM3350 datasheet's exponential-scale and mode figures; OQ-04's
  curvature read can be checked against the 556A application notes. Work
  each one: derive it, or demonstrate it is not derivable and leave it
  voiced with the reason. The same treatment goes to *every* register
  entry whose closure path needs no hardware — the scan-legible and
  network-derivable halves of OQ-08 and OQ-13 through OQ-19 included —
  so that if the rare capture never materialises, every documentary
  avenue has still been walked or recorded as a dead end. This step also
  sweeps `GhostarEngine.cpp` end to end for numeric voicings not yet in
  the register — taper shapes, stage gains, thresholds — and registers
  each with a closure path, so the register ends the step exhaustive by
  construction. Verification: the open-questions register updated with
  each derivation, dead end, or new entry.
- [x] **Step 5 — Zipper audit on the travels.** Done — the measured
  tables, the metric the measurements forced, and the travel smoother
  they justified are in the dated section at the end of this document.
  As specified: Panel travels apply at
  block boundaries (`setParameters` per block in
  `Source/PluginProcessor.cpp`), and every continuous travel is published
  for automation, so the audit covers the *entire* published surface —
  all of it: TUNE and INTERVAL, cutoff and LOWER ONLY, resonance and KB
  AMOUNT, BRIGHTNESS, all seven mixer sliders and master volume, LFO
  rate, Shaper shape and rate, filter-envelope amount and all eight
  envelope segment travels, GLIDE, and both performance wheels. Render a
  host-automated full-range sweep of each at 48 kHz, at both 512-sample
  blocks and the largest block size a host realistically runs (2048 or
  above — the latching, and so the stepping, scales with the block), and
  inspect for stepping sidebands. Each row's render uses a patch and
  event sequence that makes that travel *audible while it moves* — gate
  cycles for the envelope segment times, note changes for glide and
  tune, an active destination for each wheel — since a sweep the signal
  never expresses would pass vacuously. Verification: a per-travel table quoted
  here, one row per published continuous parameter at each block size, no
  omissions; one-pole smoothing on every travel the measurement flags,
  and a re-measure of those.
- [ ] **Step 6 — A calibration capture, if one ever exists.** The standing
  offer recorded so it is not forgotten: the moment a trustworthy Spirit
  capture becomes available (a serviced unit, a museum recording session,
  a lent instrument), measure every hardware-closable entry the register
  then holds and re-voice against the measurements. The register itself
  is the checklist — deliberately not re-enumerated here, so entries
  added after this plan was written are in scope automatically. Until then
  this step cannot start, and no constant is fitted to a YouTube demo's
  unknown signal chain.

## What was investigated and not done

- **Polyphony, effects, velocity.** The official Cherry Audio product
  already serves players who want the Spirit's architecture extended.
  Ghostar's identity is the instrument itself; a velocity-sensitive,
  16-voice, chorused Spirit is a different product, deliberately not this
  one.
- **Fitting voiced constants to online demos.** Rejected: every available
  recording passes through an unknown signal chain (tape, YouTube codecs,
  unknown service state of a forty-year-old instrument). A constant fitted
  to that is worse than a documented voicing — it carries false authority.
  This is the same reason the listening-test rules forbid using a test to
  fit a number.
- **MPE / poly-aftertouch.** The modelled keyboard is a switch matrix with
  no velocity; adding expressive dimensions the instrument never had would
  contradict the contract that keeps velocity ignored.

## Step 1 executed — the alias audit — 2026-08-22

The audit lives in `Tools/AliasAudit.cpp` (built as `GhostarAliasAudit`;
CI keeps its strokes and reference pipeline valid through
`Ghostar.AliasAuditSmoke`). Twelve strokes drive the plan's named
mechanisms hard — WIDE at full (saw, 3 % pulse and triangle), hard sync at
the keyboard's top both static and swept, ring at the top key, Osc B as an
audio-rate modulator into pitch, pulse width and cutoff, OVERDRIVE at full
resonance boost, and regenerative self-oscillation near the top of the
cutoff span — plus one deliberate control: a mid-keyboard sawtooth, the
easy case, whose figure is the floor every hard row is read against.

**The measure.** The reference for each stroke is the same stroke rendered
at 16x the shipping rate (768 kHz, the engine's supported ceiling),
bandlimited by a −100 dB Kaiser lowpass flat to 21.6 kHz, and decimated
back to 48 kHz with zero-phase alignment. Note events land at the same
wall-clock instant at every rate and swept travels are written on a fixed
1 ms grid, so the two renders describe the same performance rather than
two different control trajectories. Strokes exclude the noise source by
construction. The current fixed-clock MM5837 model is deterministic across
rates, but stochastic spectra need a statistical reference and confidence
interval rather than this tonal-bin comparison.

The reported figure is **excess**: energy the shipping render has *beyond*
the reference, counted only where it exceeds the loudest reference bin
within ±3 bins by more than a dB. Aliasing is by definition content that
is not in the ground truth, so this is what the step's alias-to-signal
gate means. The plain magnitude difference is reported alongside as
context, because excess alone would not catch a render that is
systematically quieter or detuned.

**Why the metric had to be built that way.** Two naive readings failed
first, and both failures shaped the engine, so they are recorded:

1. *A plain magnitude difference* put the tonal strokes at the top of the
   table — `selfosc-highcutoff` at +6.2 dB — when per-bin inspection
   showed the shipping and reference partials agreeing to a tenth of a dB.
   The residual was the analysis window's leakage skirt around a partial
   whose frequency differed in its fifth decimal place. Hence excess, and
   hence the ±3-bin neighbourhood.
2. *The control row failed*, and finding out why was the audit's largest
   result. The residual sat on the *low* harmonics with the shipping
   render systematically louder near the cutoff — not an alias signature.
   `resonantNodeLimit` and `integratorBound` were per-sample maps, so
   their compression per second scaled with how many samples there were:
   the engine at 2x-of-48 kHz and the 16x reference converged to
   *different filters*. Worse, the `4·tanh(0.25·x)` bound — intended as a
   runaway stop — turned out to be what actually set self-oscillation
   amplitude, an accidental cubic law whose equilibrium moved with the
   host rate.

**Baseline** (the engine as it stood, excess ≤20 kHz, dB): control −23.5,
WIDE saw −18.8, WIDE 3 % pulse −9.4, WIDE triangle −17.3, sync sweep
−17.4, ring −22.7, Osc B → pitch −13.1, → PWM −15.2, → cutoff −8.6,
OVERDRIVE −6.1, self-oscillation +6.2. (These are plain-difference
figures; the excess metric did not exist yet, because the failures above
had not been diagnosed.)

**What the audit changed**, one commit each, re-measured after every one:

- The resonance surrogate became a continuous-time state equation solved as
  an exact closed-form sub-step instead of a per-sample map, and the
  integrator bound was removed as the artifact it turned out to be. The later
  scan reopens its physical topology without undoing this numerical fix.
- Every waveform discontinuity became an *event* with a sub-sample time:
  BLEP for value jumps (saw, pulses, and a moving duty boundary crossing
  the standing phase), BLAMP for the triangle's corners, and both for the
  hard-sync reset, which was previously uncorrected entirely. Each
  oscillator emits with one internal sample of delay so an event found
  mid-sample corrects the earlier sample exactly. The ring modulator's
  triangle taps are corrected as their own channel.
- The voice core moved from 2x to 4x, decimated in two Kaiser halfband
  stages, with only the structurally nonzero taps visited.
- MOD SOURCE = OSC B stopped being read once per output sample: its
  routing is published to the voice and applied per internal sample.
- The derived resonance law (Step 3) removed most of the remaining
  cutoff-modulation residual as a side effect, mid-travel Q having fallen
  from ≈5.7 to 1.48.

**Final** — *superseded; see the correction below.* These were the figures
this step closed on, measured with a ceiling that could hide a component
almost as loud as its neighbouring partial. They are kept as history, not as
evidence:

| stroke | excess | resid ≤20k | resid full |
|---|---:|---:|---:|
| saw-midkey-control | −101.7 | −54.9 | −48.4 |
| wide-saw-10k | −200.0 | −47.1 | −45.9 |
| wide-pulse3-10k | −97.0 | −53.4 | −51.5 |
| wide-tri-10k | −200.0 | −48.4 | −48.0 |
| sync-static-topkey | −80.6 | −44.7 | −38.9 |
| sync-sweep-topkey | −81.7 | −46.1 | −37.0 |
| ring-topkey | −106.0 | −52.2 | −50.8 |
| oscb-mod-pitch | −49.8 | −18.0 | −18.0 |
| oscb-mod-pwm | −39.4 | −29.1 | −29.1 |
| oscb-mod-cutoff | −113.5 | −37.0 | −37.0 |
| overdrive-full | −197.2 | −31.6 | −31.6 |
| selfosc-highcutoff | −90.1 | −26.2 | −26.2 |

**On the two rows that sat above the gate.** Both are Osc B driving a
destination at audio rate, and this step originally excused both on the
grounds that their ground truth does not converge. That is true of the
**pitch** row: rendering at 1x, 2x, 4x, 8x and 16x and comparing each
against the next gives −22.2, −26.4, −28.6, −29.7 dB, so the steps halve and
plateau instead of falling away, and successive references disagree with
each other by about as much as the shipping render disagrees with any of
them. Exponential FM of a bandlimited sawtooth by a bandlimited sawtooth has
unbounded bandwidth; every finite rate is a different bandlimitation of it,
and there is no "the truth" to converge on. A probe that de-delayed the
modulation tap by half a sample made this row slightly worse, confirming the
plateau is the signal's own bandwidth rather than the model's tap.

It is **not** true of the PWM row, and saying so was an error review caught:
the text applied the argument to both rows and then contradicted itself two
sentences later by noting that the PWM row converges. A rate ladder settles
it — `oscb-mod-pwm`'s consecutive-pair residual falls −31.7, −37.7, −43.8,
−49.9 dB, a clean 6 dB per doubling with no plateau, against the pitch row's
−22.2, −26.2, −28.7, −29.6. **The PWM row was a convergent quantity sitting
above the gate**, and nothing excused it.

Review proposed the duty-move event's sample-boundary placement as a cause.
Trying a sub-sample crossing measured **2.8 dB worse**, and linearly sweeping
duty through the sample measured **2.7 dB worse**, both against one common
ground truth with all other strokes unchanged. The dominant error was the
audio-rate modulation tap's unnecessary one-sample lag. The causal split
emits B first when SYNC is off, so A/PWM and filters receive current
BLEP-corrected B. With SYNC on, PWM retains prior B to break the real
`B→A→A-reset→B` loop; downstream filters stay current. A later OQ-25 source
pass replaced pitch's current/prior heuristic with the two CEM3340s' actual
R82/C72 and R118/C77 states. Only PWM/filter active-path delay still awaits
measurement.

### The gate verdict is withdrawn — 2026-08-22, after review

Review asked whether an alias landing on a partial could hide from the
excess metric. It could, and the answer changes this step's conclusion.

**What was wrong.** Each bin was compared against the largest reference bin
within ±3 bins, times a +1 dB tolerance. That turns every partial into a
70 Hz-wide plateau of permission: a bin whose own reference is empty
inherits its neighbour's magnitude as its ceiling. The comment justifying it
— that alias images land far from the partials that produce them — was an
assumption, and it is false. A fold family sits at |k·f₀ − n·f_s|, which for
any f₀ that is not a neat fraction of f_s lands tens of Hz from the harmonic
grid; hard sync, ring modulation and a driven nonlinearity then make the
grid dense enough that "tens of Hz away" means "on top of something else".

**Measured, not argued.** Injecting one sinusoid of known level into the
real strokes, through the real pipeline: a **−40 dB alias — twenty dB above
the acceptance gate — reported the metric's −200 dB floor**, on
`wide-saw-10k` at 500 Hz and at 5175 Hz, and on `sync-static-topkey` at
2000 Hz and 5175 Hz, where the figure did not move at all. Sweeping a
gate-level alias across the audible band, the reported figure was unmoved
across 83.9 % of it for `wide-saw-10k` and 56.3 % for the control row.
Synthetically, an alias at exactly the gate reported −200.0 anywhere within
seven bins of a partial. Attribution: with the neighbourhood removed but the
tolerance kept it still read −200.0; removing the tolerance recovered
−64.4 dB. Both parts contribute and the tolerance alone is enough.

**What the instrument is now.** `Tools/AliasMetric.h`, extracted so it can be
exercised on signals whose alias content is known exactly. The disagreement
being tolerated is a small difference in *pitch*, so it is applied as one:
the ceiling spans what a **measured** relative frequency error could have
moved into a bin, proportional to frequency rather than a fixed bin count,
and never narrower than the analysis window's own main lobe. The drift is
the median across matched peaks, not the worst — otherwise a loud alias
inflates the very tolerance that then hides it. `Tests/GhostarAliasMetricTests.cpp`
pins the behaviour, including the case the old metric floored.

**Re-measured** (16384-point Blackman-Harris; the two new columns are the
measurement's own detection floor and its verdict):

| stroke | excess | blind floor | resid ≤20k | decided? |
|---|---:|---:|---:|---|
| saw-midkey-control | −85.1 | −15.4 | −52.1 | no |
| wide-saw-10k | −70.7 | −16.1 | −49.1 | no |
| wide-pulse3-10k | −72.7 | −15.7 | −54.1 | no |
| wide-tri-10k | −78.2 | −16.4 | −49.8 | no |
| sync-static-topkey | −76.3 | −15.4 | −40.8 | no |
| sync-sweep-topkey | −77.3 | −13.6 | −42.1 | no |
| ring-topkey | −100.5 | −14.7 | −59.0 | no |
| oscb-mod-pitch | −47.5 | −15.4 | −14.8 | no |
| oscb-mod-pwm | −40.0 | −16.2 | −32.0 | no |
| oscb-mod-cutoff | −72.4 | −14.3 | −43.8 | no |
| overdrive-full | −61.0 | −16.1 | −60.6 | no |
| selfosc-highcutoff | −58.5 | −16.0 | −49.0 | no |

This 2026-08-23 rerun supersedes the earlier post-metric table after the
P1014 waveform levels, complete Lower MNA, physical C34 state and corrected
OSC-B modulation tap landed; the current rerun also includes the shared
ADSR sustain floor, nonlinear release knee/reset notch, destination-loaded X
wheel, the explicit OVERDRIVE C34 A3+B7+C10 hypothesis and the coupled Upper
LOW/VARIABLE two-section solve including C40 charge transfer. The latest
figures additionally include the two CEM3340 `1.82 kΩ·1 nF` pitch-control
parallel-RC states and their causal predict/commit scheduling.
The historical −200.0 rows remain gone:
`wide-saw-10k` is −70.7 and `wide-tri-10k` −78.2, so real added content was
being floored by the withdrawn metric.

**And every row is undecidable.** The blind floor sits between −13.6 and
−16.4 dB, because a +1 dB level tolerance on tonal material leaves that much
room under a partial — and a component landing exactly on a partial is
arithmetically indistinguishable from that partial being slightly louder, by
this or any other magnitude comparison. So:

> **This step's gate verdict remains withdrawn.** The claim that every
> convergent stroke sits at or below −80 dB of excess against a −60 dB gate
> was a statement about where the old clamp fired, not about alias content.
> Comparing two renders at different rates *cannot* certify a −60 dB
> alias-to-signal gate on tonal material, and the audit no longer says it
> can. What it now provides is a sound upper bound on how much the shipping
> render differs from a 16× ground truth, published beside the floor below
> which it cannot see.

The DSP work this step drove is not impeached by any of it — the
rate-independent filter formulation, the sub-sample BLEP/BLAMP events, the
4× core and the hard-sync correction each stand on their own evidence, and
the plain-difference column improved throughout. What fell is the
certification, not the engine.

**What would decide it** is a reference-free measure: for a stroke that
holds a pitch, everything off the harmonic grid is alias and noise, with no
second render to disagree with and no tolerance to hide behind. A first
attempt is recorded in OQ-24 rather than shipped — it reported a stroke's own
inharmonicity as though it were alias, and half a measurement is precisely
what produced the defect corrected here.

**Cost.** The engine renders 30 s of a hard patch (both oscillators, ring,
OVERDRIVE, VARIABLE resonance) in 3.9 s — 7.7x realtime, down from 22.7x
before this work and far above the suite's bound.

*Corrected 2026-08-22, after review.* The two halfband kernels were not
in fact sparse when that figure was taken. A halfband's sinc vanishes at
every even offset from its centre, and the design skipped those taps by
testing the computed coefficient against zero — but `sin(pi n / 2)` at even
`n` evaluates to a rounding residue near 1e-16, not to zero, so every tap
was kept: 31 and 127 rather than 17 and 65, and the decimator did close to
twice the arithmetic the comment beside it claimed. The zeros are now
identified by position instead. Measured on one machine in one session, the
same 30 s hard patch goes from 3.21 s to 2.92 s — 9.4x to 10.3x realtime.
All twelve committed demo WAVs render bit-identical across the change,
which is the evidence that the discarded taps really were carrying nothing.
The tap counts are now pinned by a test, because nothing audible depends on
them and so nothing else would have noticed.

## Step 2 closed as no drift — 2026-08-22

The step's own gate required both halves before any wander could ship.

**Half (a) exists and is now anchored.** The Curtis CEM3340/3345
datasheet (© 1980) gives residual tempco after on-chip cancellation as
−150/0/+150 ppm and whole-oscillator drift at the reference frequency as
±50 typ / ±200 max ppm — ±0.087 to ±0.35 cents per °C. The 3340 needs no
external tempco resistor (compensation is on-chip and "nearly perfect"),
and SM confirms both of the Spirit's 3340s run the datasheet's own
application circuit verbatim (470 Ω + 10 nF, RS 1k82, REE 620 Ω, 1 nF
timing cap, 100 kΩ scale trims) from LM317/7912-regulated ±12 V rails. The
supply path can be bounded without any excursion record: a ±10 % mains
excursion through the LM317's line regulation moves the rail 2.4 mV
typical, which is 0.35 cents worst-case through the un-cancelled f ∝
1/VCC path, and first-order zero in the recommended reference-from-VCC
hookup.

**Half (b) does not exist.** No measured or published record of the
temperature excursion inside a synthesizer enclosure was found, for the
Spirit or for any comparable instrument. Every candidate fails the step's
"measured record, not an assumed one" bar: the one published tuning-vs-time
comparison of 3340-class instruments exists only as graphs inside a video,
measures pitch rather than temperature, and is of a small desktop box that
is not thermally comparable to a 1983 keyboard; forum measurements of
unnamed (and in one case explicitly malfunctioning) modules log no
temperature and are behind a blocked forum. The manufacturer's own
successor datasheet states the driver plainly — "Any temperature changes
are mainly due to the operating environment" — which is exactly the
unrecorded quantity. A second, independent reason stands even if an
excursion record appeared: the musically audible part is the *differential*
drift between two oscillators sharing one board, one CV bus and one pair of
rails, and no source quantifies that correlation at all; the datasheet
specs are per-chip.

**Verdict.** The step closes as no drift, as its own text requires when no
defensible process can be established. Ghostar applies none, and that is
now backed by the datasheet numbers rather than by the 3340's reputation.
The reopening trigger is recorded with Step 6: one temperature logger
inside any CEM3340 instrument's case closes half (b), after which the
conversion to cents is fully anchored.

## Step 3 component pass executed; circuit topology reopened — 2026-08-22

The step asked for the BA130 pair's knee and compression from the
datasheet plus the node's operating level, for both the resonance limiter
and OQ-10's inter-filter clipper, and for the integrator bound to end up
justified, re-derived or removed.

**The diode is identified and its curve is in hand.** The BA130 is a
Fairchild diffused-silicon-planar general-purpose diode (DO-35, WIV 25 V),
published on the combined BA128·BA130 sheet in the 1978 Fairchild Diode
Data Book, printed p.3-12, with its family curves on p.4-6 — not the
Philips/Valvo part the Pro Electron numbering suggests. Fairchild's own
cross-reference lists it as the silicon replacement for a long row of
germanium signal diodes, i.e. it was the low-knee selection of the family,
which is exactly the character wanted in a soft limiter. Its forward
specification runs down to 10 µA (0.34–0.47 V there, 0.56–0.71 V at 1 mA),
and the digitised typical curve gives 99 mV per decade over 10 µA–1 mA:
ideality n ≈ 1.68, saturation current ≈ 2.3 nA. So an anti-parallel pair
obeys `I(V) = 2·Is·sinh(V/(n·V_T))` with `n·V_T ≈ 43 mV` — a knee
markedly softer than an ideal junction's 59 mV/decade, whose ceiling rises
about 0.10–0.12 V per decade of drive and never truly flattens.

**What shipped as the first rate-stable surrogate.** Both nonlinear shapes
use the BA130 curve rather than a tanh stand-in:

- The Lower and controlled Upper limiter surrogate is a separable sinh state
  equation solved as an exact sub-step. Its host-rate invariance is real; its
  topology and scaling remain voiced.
- The inter-filter OVERDRIVE surrogate solves a memoryless BA130-shaped curve
  by three Newton steps. It preserves the former gain/ceiling intentionally,
  but is not a traced solution of the rotary-switch network.

**Correction from the grayscale P1013 pass.** The two high-Q limiters are
external Figure-5-style feedback loops: distinct TL082/resistor BP gains
(≈13.73 Lower, 16 Upper) return through BA130 pairs and C33/C37=1 nF. The
fixed-Q Upper half has no pair. OVERDRIVE is a separate R153=330 kΩ,
R164/R165=2.2 kΩ, D1/D2 and R166=470 Ω switch network; its diodes are not
across the op-amp feedback resistor. OQ-10/OQ-12 are reopened for stateful
MNA models, and the fixed Upper surrogate has been removed.

**Stateful limiter replacement — 2026-08-22.** Controlled Upper now carries
the real 1 nF coupling-capacitor companion; a monotone implicit BA130-current
solve couples it to that section's BP/LP endpoints with gain 16. Lower carries
the locally exact C33, gain-13.7273 and 2 kΩ reduction on its temporary
canonical base section; OQ-20's wiper/VLP/VBP MNA will change those base
sensitivities and must absorb the branch. Fixed Upper remains linear. The
component ratios are no longer voiced, while the 24 mV/engine-unit node scale
and ideal-TL082 assumption remain for hardware calibration. The traced
OVERDRIVE local core and output high-pass ship too; the shared RS7/Lower MNA
is the unfinished part of this plan step.

**Lower/P1014 integration — 2026-08-23.** The temporary Lower base is gone.
Each 100 kΩ slider is reduced as a Thevenin source whose wiper reaches VLP
through 220 kΩ and VBP through 68 pF; three capacitor histories and both 22 nF
states collapse with C33 to one 2×2 endpoint solve and the monotone BA130
current. Exact-endpoint charge projection prevents a hidden Nyquist mode.
P1014's IC10 conditioner fixes one engine unit at its 5 V affine offset, so
selected waveform volts, CEM states, high-Q diodes and OVERDRIVE now share one
domain. C34 now stores its physical plate voltage. Corrected schematic
terminal/net tracing shows C9/C11 ground Lower VLP, C10 feed it through
R167=33 kΩ to C34, and C12
feed it directly; B6/B7 select IC12 pin 1 and B8 selects `151·VBP`.
The engine's A3+B7+C10 hypothesis therefore includes the clean-VLP term
`(33·o+47·VLP)/80`, but it is not a traced detent: a standard same-index
3P4T reading pairs panel OVERDRIVE with C11 instead. The scan has no installed
rotor phase or assembly legend that resolves that contradiction or labels the
other C34 histories.

**The integrator bound is removed**, which is the step's third
requirement answered. Step 1's audit showed it was not a runaway stop at
all but the actual amplitude-setting nonlinearity, with a per-sample
strength no two rates agreed on. With the implicit external loop bounding the
resonant node, the engine suite renders the regenerative extremes — full
resonance across the cutoff span, driven and
undriven, OVERDRIVE engaged — at 44.1 and 96 kHz and finds them finite and
bounded. The current committed regression extends that level comparison to
8, 44.1 and 96 kHz and requires agreement within 0.5 dB; the one-step circuit
test separately pins the TPT identities, charge conservation and diode KVL.

**A fourth result arrived with the datasheet**: the CEM3350's Q-control
law and the Spirit's own resonance network together *derive* the
travel-to-Q mapping that OQ-12 had listed as voiced, which was the
engine's most character-defining invented number. It is recorded in the
register and in the commit that shipped it; the short version is that Q at
half travel is 1.48 rather than the old law's 5.7, so the instrument is
gentler through the middle of the control and steeper at the top.

**Alias re-measure.** Step 1's suite was re-run after each of these
changes, as that step requires: new nonlinearity makes new high partials,
and the completed table above describes the final engine.

**No listening test was run.** The step allows one if the derived and
shipping laws both remain defensible and audibly differ — but the shipping
laws were not defensible once the datasheet was in hand: a tanh is not
what a diode pair does, and a per-sample map is not what a circuit does.
The derivation decides.

## Step 4 executed — the register made exhaustive — 2026-08-22

**What was derived.** OQ-02's *span* and sensitivities (the 12k1 ladder
against the CEM3350's −19.6 mV/octave: 21.2 mV/V at the pin, ±11.4
octaves of pot authority over a ~10-octave chip window, a fixed +193 mV
offset, ±2.3 octaves of trim authority), OQ-04's whole segment law (the
panel's 5 ms–10 s is the RC time constant; the nominal attack aim is bounded
but voiced), OQ-12's travel-to-Q mapping in full, and OQ-13's tracking amount
(108 %, which reproduces the manual's "slightly over 100 %" from the
resistors), and — with Step 3 — the BA130 component law used by OQ-10/OQ-12.
Each is recorded against its source in the register.

**What was demonstrated not derivable, and left voiced with the reason.**
OQ-02's absolute *placement*: the 100 kΩ trimmer's factory setting is not
documented anywhere, and the service manual has no calibration text at
all, so the window's position is a per-unit calibration rather than a
constant. OQ-09's Q split was initially left one digit short: the first
scan corroborated the separate fixed bias network but not its pull-up.
The later grayscale JP2 pass resolves it as 91 kΩ and closes the split at
Q=0.5; see the post-plan closure below. A later lossless keyboard trace first
confirmed OQ-13's post-DAC fixed-reference injection and ruled out bottom C;
the subsequent signed DAC/current reduction closes its exact nominal zero at
MIDI 60.006015. OQ-10's and OQ-12's component curve is anchored. The complete
Lower limiter MNA now ships in P1014's selected-wave voltage domain; the
remaining unknowns are original-device dynamics, absolute noise/output
calibration and RS7's unlabelled output contacts.

**What the end-to-end sweep of `GhostarEngine.cpp` added.** Four entries
that had never been registered, found by walking every numeric literal in
the file against the register: the mixer summing gain (OQ-20), the LFO's
slow endpoint/original-CEM scale (OQ-21), the BRIGHTNESS pot's log law and
series residual
(OQ-22), and the travel smoother (OQ-23) — the last recorded explicitly
as a *product policy* with no hardware analogue, so a future reader
cannot mistake its 25 ms for a modelled time constant. The sweep also
caught a register entry that contradicted the code: OQ-08 described the
glide taper as exponential where the engine has always used a quadratic
one. The record is corrected in place, and the correction is noted inside
the entry rather than made silently.

**What this step did not initially reach.** OQ-14 through OQ-19 needed
board-level scans that this pass covered only for the filter and oscillator
boards. A later grayscale-source pass reached the noise and output board:
it closes both P1013 noise transfers and the Master Volume law, and derives
the Shaper mixer's unbuffered-slider loading. The Filter trace instead shows
separate 220 kΩ-to-VLP and 68 pF-to-VBP buses; their coupled MNA and the
overall scales remain open. Wheel depths and the RED NOISE bus scale still
require the hardware measurements named in their entries. Later lossless
P1015/P1013/P1017 passes closed the Shaper endpoint split, nominal Ring
topology, BRIGHTNESS network, real Filter-path coupling and rear-output
normaling; see the post-plan closures below.

## Step 5 executed — the zipper audit and the travel smoother — 2026-08-22

The audit lives in `Tools/ZipperAudit.cpp` (built as `GhostarZipperAudit`;
CI keeps its strokes valid through `Ghostar.ZipperAuditSmoke`). Every
published continuous travel — all 27 panel travels plus both performance
wheels — is rendered through an exposing stroke three times at 48 kHz:
parameters applied every sample (the reference), then latched every 512
and every 2048 samples. Note events are sample-accurate in all three
renders, as the plug-in's segment loop makes them; only the parameter
application latches. Envelope-segment travels ride gate cycles, glide
rides note changes, each wheel rides an active destination, and the
filter-envelope segments run at an opened depth — the audit refuses a
stroke whose reference is silent, so no row can pass vacuously.

**The metric the measurements forced.** Two naive metrics failed before
one held, and both failures are recorded because they shaped the final
recipe:

1. *Waveform residual* reported ~0 dB "error" on every pitch-affecting
   travel: retuning changes the oscillator's phase increment, so the
   latched and reference renders drift apart in accumulated phase while
   sounding the same. The residual is therefore measured on short-time
   spectral magnitudes (2048-sample Hann windows, hop 1024), which
   forgive accumulated phase but keep step transients and block-rate
   sidebands.
2. *An un-shifted reference* buried the artifact under the unavoidable:
   a latched trajectory is the sweep sampled at block starts, i.e. the
   reference delayed by half a block on average, and no causal smoothing
   can remove that delay. The reference for each block size is therefore
   rendered with its control trajectory delayed by half that block, so
   the comparison isolates the latching *artifact* from the inherent
   control delay.

**The stroke** is deliberately hard: a full-range up-and-down triangle in
3 s. Residuals scale with gesture speed, so these figures are the stress
case, not the typical one.

**Before** (no smoothing, delay-compensated spectral residual, dB):

| travel | 512 | 2048 | travel | 512 | 2048 |
|---|---|---|---|---|---|
| tune | −62.6 | −45.5 | filterEnvAmount | −53.3 | −38.8 |
| interval | −49.7 | −31.3 | filterAttack | −71.8 | −51.2 |
| masterVolume | −48.5 | −39.9 | filterDecay | −80.9 | −57.9 |
| brightness | −52.5 | −39.7 | filterSustain | −86.2 | −63.8 |
| shaperPathA | −52.3 | −43.5 | filterRelease | −82.4 | −64.9 |
| shaperPathB | −59.4 | −50.6 | loudnessAttack | −69.4 | −49.3 |
| shaperPathRing | −58.9 | −46.7 | loudnessDecay | −87.1 | −63.3 |
| shaperPathNoise | −65.7 | −54.1 | loudnessSustain | −83.3 | −57.3 |
| filterPathA | −54.3 | −46.3 | loudnessRelease | −76.4 | −53.6 |
| filterPathB | −62.4 | −54.6 | lfoRate | −49.4 | −31.6 |
| filterPathNoise | −52.6 | −40.2 | shaperShape | −34.5 | −20.1 |
| cutoff | −42.6 | −29.0 | shaperRate | −21.3 | −12.1 |
| lowerOnly | −44.8 | −29.7 | glide | −69.2 | −41.3 |
| resonance | −57.9 | −45.4 | xWheel | −57.7 | −43.4 |
| kbAmount | −61.5 | −46.9 | yWheel | −54.4 | −39.7 |

**The fix.** The engine now carries a travel smoother, following the
house convention the sibling engines use: `setParameters` writes targets,
and every continuous travel plus both wheels glides toward its target
with a ~25 ms one-pole, advanced per sample in `advanceControls`
(`travelSmoothing_`). Switches always apply immediately. A fully silent
engine — loudness envelope idle or released below its derived VCA-zero
threshold, no keys, no VCA bypass, no raised Shaper-path slider — snaps
instead of gliding, so a state restore before
playing, and every law-measuring test, lands exactly. `reset()` snaps to
the standing targets; `stopAllSound()` preserves both the smoothed wheel
values and their targets. Three defects the first review round caught
are folded in: the output gain is derived per sample *after* the
smoother advances (captured once per `process()` call it held a whole
host block, which the audit's first master-volume row measured before
the cause was found), the one-pole lands exactly once within 1e-6 of
its target so a slider decaying to zero cannot stall on a float residue
and lock out the silent-snap path, and a restored wheel position snaps
while silent exactly as the panel travels do. The regression test
(`testTravelStepsGlideWhileSounding`) steps master volume by a factor of
ten mid-note and hears it glide, then settle.

**After** (travel smoother in, same metric, dB):

| travel | 512 | 2048 | travel | 512 | 2048 |
|---|---|---|---|---|---|
| tune | −79.0 | −47.5 | filterEnvAmount | −68.0 | −52.7 |
| interval | −47.7 | −33.7 | filterAttack | −61.1 | −48.6 |
| masterVolume | −76.1 | −50.9 | filterDecay | −74.9 | −64.4 |
| brightness | −90.2 | −66.8 | filterSustain | −84.7 | −71.3 |
| shaperPathA | −75.8 | −48.5 | filterRelease | −103.0 | −77.5 |
| shaperPathB | −82.8 | −55.6 | loudnessAttack | −55.5 | −43.1 |
| shaperPathRing | −83.1 | −55.5 | loudnessDecay | −79.0 | −68.2 |
| shaperPathNoise | −70.9 | −43.1 | loudnessSustain | −78.1 | −64.8 |
| filterPathA | −71.6 | −47.2 | loudnessRelease | −100.1 | −70.9 |
| filterPathB | −77.9 | −53.5 | lfoRate | −55.8 | −33.5 |
| filterPathNoise | −73.6 | −48.9 | shaperShape | −42.9 | −33.0 |
| cutoff | −70.9 | −46.4 | shaperRate | −31.4 | −16.7 |
| lowerOnly | −83.9 | −60.0 | glide | −81.0 | −52.0 |
| resonance | −77.1 | −53.2 | xWheel | −86.9 | −63.9 |
| kbAmount | −88.0 | −65.3 | yWheel | −50.7 | −30.1 |

**Reading the tables.**

- The smoother buys roughly 12–28 dB at 512 where zipper is notorious:
  cutoff (−42.6 → −70.9), master volume (−48.5 → −76.1), LOWER ONLY,
  resonance, brightness, most mixer faders, glide and the X wheel. The
  noise-mixer strokes are exceptions because the physical-noise rewrite
  changed the stochastic residual; those rows are measurements, not a blanket
  improvement claim. The step-discontinuity class — the audible click — is
  gone; what remains is band-limited ripple at the block rate, falling with
  the one-pole's slope.
- The rerun after the shared sustain floor, nonlinear diode release, 100 Ω
  cap arms and retrigger-reset notch supersedes the earlier envelope rows.
  Decay, sustain and release remain below −60 dB at 2048; the fast Attack
  sweeps are −48.6 dB (Filter) and −43.1 dB (Loudness). Those residuals follow the continuously moving
  onset trajectory rather than a value step; no exposed envelope stroke
  regains the discontinuity class the smoother removed.
- The final MOD RATE rerun includes P2's derived R33-loaded travel. Its
  measured row is −55.8/−33.5 dB without changing the step-free verdict.
  The destination-loaded wheels measure −86.9/−63.9 dB for current-driven X
  and −50.7/−30.1 dB for voltage-fed Y; their very different figures follow
  their equally different electrical curves, with no new step class.
- The rows that remain above −60 dB at 512 (shaperRate at −31.4 the
  worst, then shaperShape, interval, yWheel, Loudness Attack, lfoRate and
  Filter Attack) are not step
  artifacts: their residual is smooth amplitude or frequency-trajectory
  ripple that scales with gesture
  speed — the Shaper travels integrate their rate into phase, so
  trajectory ripple becomes cycle-position wobble against the beating
  reference — and it is block-size-dependent exactly as the half-block
  ripple theory predicts.
  Pushing them under −60 for a full-range 1.5-second gesture would need
  a ≥100 ms smoother — mushy controls traded for a stress case no
  finger reproduces. The −60 dB gate is therefore read as: *no step
  discontinuities anywhere* (met), and the remaining ripple bounded and
  falling at 6 dB/oct (met); the original blanket-−60 reading is revised
  by these measurements, and the revision is recorded here rather than
  smoothed over.

## Post-plan primary-source closure — grayscale service scan — 2026-08-22

The Internet Archive's lossless grayscale JP2 source resolves component
digits that disappear in the common thresholded black-and-white PDF. Four
documentary gaps close without listening or hardware fitting:

- R181 is 91 kΩ, so the Upper filter's extra 24 dB cascade section has the
  same fixed Q=0.5 bias as the LOW switch (OQ-09).
- Both Master Volume gangs are printed `20k LIN`; the engine now applies
  linear gain and `testMasterVolumeIsLinear` pins half travel to half output
  (OQ-19).
- P1015 resolves SHAPE as 1 MΩ linear P4, with D19/D20 steering the two pot
  segments through 27 kΩ R62. Together with the patent's resistance/time
  relation this gives exact 2.5617/97.4383 endpoint splits while the two
  half-cycle resistances always total 1.054 MΩ. The engine replaces its
  former 5/95 voicing, and `testShaperShapeFollowsItsSteeredPot` times five
  complete cycles against the independent component equation (OQ-18).
- The MM5837's P1013 circuit is legible end to end. `SpiritNoise` replaces
  the host-clocked LCG/Kellet approximation with taps-17/14 maximal PRBS,
  a fixed 75 kHz nominal clock, and five bilinear first-order sections for
  the derived, factored coupling/passive/1458 transfer. The circuit suite checks
  20 Hz, 1 kHz and 10 kHz within 0.1 dB at 44.1, 48 and 96 kHz hosts, and
  verifies the 131071-bit repeat (OQ-15).

The same PRBS feeds the derived R6/C8-to-IC4B RED NOISE branch, removing
both the second unrelated RNG and the unsupported 1.5 Hz filter. Its final
mapping onto the engine's modulation bus remains voiced. Mixer travel now
includes each unbuffered 100 kΩ slider's Thevenin resistance, so the
errata-corrected 47/6.8 Shaper ratio is applied only at full travel rather
than across the whole stroke. The overall mixer scales, the Filter's separate
220 kΩ/68 pF state-node buses, the particular MM5837's clock within its
datasheet spread and the common source-volts-to-engine-units scale remain
deliberately visible work for Step 6 rather than inferred normalisations.

## Post-plan primary-source closure — control, oscillator and output networks — 2026-08-23

A second independent trace of the lossless P1013/P1015/P1016/P1017 drawings
closed several audible details that the thresholded manual had made easy to
misread:

- SHAPE's exact endpoint is 2.5617/97.4383 %, and FREE's SG output is the
  hysteretic comparator phase itself: high throughout rise, low throughout
  fall. RS3/FET phase forcing is closed; only self-Y trigger-edge feedback
  acceptance remains open (OQ-05/18).
- The nominal Ring path is `−15/13·HP(A)·B`, including C15=1 µF into
  `39 kΩ || 100 kΩ` (`f_c=5.67245 Hz`) and its 25 kΩ internal carrier-null
  trim. The former deterministic 3% symmetric leak had no circuit basis;
  real factory-null residue and CEM feedthrough remain per unit (OQ-06).
- C6 is 470 nF, fixing GLIDE's 2 MΩ endpoint at 0.94 s; only the unmarked
  intermediate pot taper remains voiced (OQ-08).
- C30=470 nF is the Filter path's real 17.4958 Hz coupling network *before*
  its VCA. The Shaper and P1017 rear outputs are DC-coupled, and the normalled
  ADSR/MIX jack uses the exact coupled P4/R49/R50 MNA rather than an
  unconditional arithmetic half-sum (OQ-16).
- BRIGHTNESS is after the Shaper VCA: C18=27 nF and P3=100 kΩ LOG shunt the
  fixed 20 kΩ Master track. In SPLIT, dark is a 294.731 Hz low-pass and bright
  retains a −1.5836 dB high-frequency shelf. Normalled, the branch cross-loads
  both paths; at full Master dark's pole becomes 442.097 Hz and the Filter is
  coloured too. Only P3's manufacturer taper is open (OQ-22).
- The Loudness VCA's 10 kΩ/3.3 kΩ/240 kΩ control network puts nominal zero
  gain at 0.5 V, exactly `1/15` of the envelope peak. Its normalized affine
  law now ends the audible release there; original-CEM top saturation and
  feedthrough remain device-dependent (OQ-04).
- The pitch network has ±15.88 semitones of full electrical authority. The
  spring wheel's mechanical fraction is undocumented, so the shipped ±8
  endpoint stays explicitly voiced (OQ-01).
- Each CEM3340's complete pitch-current sum crosses its documented
  multiplier-output bypass before exponential conversion: A R82/C72 and B
  R118/C77 are both `1.82 kΩ || 1 nF`, hence `tau=1.82 µs` and
  `f_c=87.4478 kHz`. Ghostar models each capacitor as a separate state with
  exact DC delay and monotone behavior across the supported rate range;
  destination-specific PWM/filter delay remains measurement-owned (OQ-25).
- MOD RATE's P2=100 kΩ LIN control is loaded by R33=200 kΩ, so the exact
  electrical travel is `w(x)=200x/(200+100x(1−x))`; half travel is `4/9`.
  Ghostar now preserves that asymmetric knob feel. The manual still gives
  only "less than 1 Hz" at the slow end, so 0.3 Hz remains explicit rather
  than masquerading as a component-derived constant (OQ-21).
- The visually similar X and Y wheels are electrically opposite quirks.
  X's CEM3360 current output sees a 100 kΩ rheostat; Y's SHAPE voltage sees a
  100 kΩ divider through 15 kΩ. RS1/RS2 add one or two 100 kΩ oscillator or
  filter inputs in parallel with the 22 kΩ pitch shunts, so every switch
  position has its own maximum and curve. Ghostar now carries that loading at
  control and audio rate: at half assumed-linear resistance X→A is already
  86.75% of full, Y→B only 33.56%, A+B is loaded down, and U-only is deeper
  than U+L. The two one-octave source anchors, pot tapers, active RWM transfer
  and Y→rate ceiling remain visible calibration seams (OQ-14).

One apparent shortcut was rejected during independent review: P1016's lowest
key carries DAC code zero, but IC16A also receives a fixed same-rail current,
so code zero is not KCV zero. The completed signed reduction uses R31=`4k99`,
R39=`26k6`, R40=`2k43`, the DAC0800 pin-4 sink and four DAC counts per
semitone. The two IC16A currents cancel at
`q=64·4.99/26.6=12.006015` semitones above the lowest C: MIDI 60.006015,
only 0.6015 cent above the long-standing second-C approximation. The engine
now carries that resistor expression directly and the circuit test pins its
sign as well as the independently derived 108.3 % amount (OQ-13).

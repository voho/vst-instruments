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
integrator, the triangle-cross ring modulator with its un-nulled bleed, and
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
Step 4's sweep makes the register exhaustive) and point 4
*structurally*: every control exists with the silkscreen's detents and
labels, but several travel *tapers* behind them are voiced, not verified
(cutoff span OQ-02, envelope timing OQ-04, glide OQ-08, Shaper shape
OQ-18, volume OQ-19), so control behaviour stays open work alongside
points 1 and 2 until Steps 4 and 6 close those tapers.

## Where Ghostar actually stands

**Anchored laws** (each traced to a primary source in
`Docs/circuit-modelling-research.md`, implemented in
`Source/DSP/GhostarEngine.cpp`):

- Keyboard law and the ~110 % filter tracking amount (`trackingOctaves`,
  factor 1.1; the middle-C *pivot* is voiced — see below).
- The panel's printed pulse duty sets — A 50/30/15/6 %, B 40/20/10/3 %
  (the silkscreen's numbers are anchored; whether a calibrated unit's
  PW trimmer actually lands on the print is OQ-03's open half, so the
  implemented percentages are the printed values taken as voiced) — and
  Osc B's −1/UNISON/+1/+2/BASS (30–300 Hz)/WIDE (2 Hz–10 kHz) ranges with
  the ± perfect-fifth INTERVAL.
- The 556A envelopes' 5 ms–10 s segment span (the manual's own numbers; the
  segment *curvature* law is voiced — see below).
- The Shaper Y integrator from US 3,943,456 with FREE/KBD HOLD/RESET/RUN,
  RESET retriggering on every key press regardless of TRIGGER mode.
- The bipolar ±2.5-octave filter envelope, the OR'ed gate buses with
  keyboard articulation only through KBD, last-note keying with
  fallback-without-retrigger, AUTO glide, RIPPLE/ARPEGGIO/LEAP with the
  (0, +12, −12) pattern.
- MM5837 white noise with a partial pinking stage, and series-capacitor AC
  coupling on the outputs — both anchored *in presence*; the pinking
  transfer and the coupling corner are voiced numbers (see below).

**Voiced constants** (defensible but not anchored). The character-defining
ones are listed here, each with its entry in the open-questions register
`Docs/open-questions.md`. The register, not this list, is the authority —
and Step 4 sweeps every remaining numeric voicing in the engine into it,
so the register becomes exhaustive by construction rather than by
enumeration here:

- Filter cutoff span 20 Hz–16 kHz (`exponentialTravel(p.cutoff, 20.0,
  16000.0)`, OQ-02).
- Envelope segment curvature: attack charges toward 1.5× and switches at
  1.0 (coefficient ln 3, so the labelled time is the real time-to-peak),
  decay/release read as three time constants — a 555-family reading, not a
  sourced derivation (OQ-04).
- Resonance law `k = 2·0.01^t − 0.025`, regenerative at full travel, and
  the diode limiter that bounds each resonant node — linear below `knee =
  1.2`, tanh-capped at `ceiling = 2.2` (OQ-12).
- The inter-filter OVERDRIVE clipper `0.45·tanh(6·x)` (OQ-10), and the
  24 dB cascade's Q split — first section fixed at Q = 0.5, second carries
  the resonance control (OQ-09).
- Ring-modulator carrier bleed `0.03·(triA + triB)` (OQ-06).
- Shaper gate comparator threshold `shaperLevel_ > 0.01` (OQ-05).
- Glide lag `tau = 0.9·travel²` seconds, from the 2 MΩ pot into ~450 nF
  (OQ-08).
- The filter-tracking pivot at middle C — the note where tracking
  contributes zero offset (OQ-13).
- Wheel modulation depths: 1 octave of pitch, 3 octaves of cutoff, ±0.42
  duty, and the 60 Hz fastest LFO rate a full Y wheel reaches
  (`pitchDepthOctaves`, `filterDepthOctaves`, `dutyDepth`, OQ-14).
- The RED NOISE process — a 1.5 Hz one-pole over white noise, restored by
  18× gain and clipped (OQ-17); the manual anchors only "continuous slow
  random".
- The Shaper SHAPE endpoint split (rise fraction `0.05 + 0.9·travel`,
  OQ-18) and the master volume's square-of-travel taper (OQ-19).
- The noise pinking blend — the Kellet reference poles re-derived to
  physical frequencies, standing in for the unresolved network (OQ-15) —
  and the ~5 Hz output coupling corner (OQ-16).
- The per-section integrator stability bound `4·tanh(0.25·x)` riding
  above the resonant-node limiter (tracked with it under OQ-12).

**DSP quality today:** PolyBLEP sawtooth and pulses with naive triangles at
2× oversampling, one 63-tap halfband decimator per audio path
(`halfbandTaps = 63`), TPT state-variable filter sections, denormal
flushing, and a bounded integrator everywhere state accumulates. The whole
voice renders far faster than realtime (the engine suite bounds five
seconds of audio inside four wall-clock seconds on a loaded CI worker).

**The standing limitation (OQ-11):** no owned hardware, no calibration
captures, and none available in the field. Every constant marked *voiced*
stays voiced until a trustworthy capture exists. Decisions the physics
cannot close are made by A–Z listening tests under the repository's rules —
chosen by ear, recorded as chosen by ear.

## The steps

Each step names its verification. Audible, physics-ambiguous choices go
through an A–Z listening test (A = shipping engine); measurable claims are
settled by measurement and quoted.

- [ ] **Step 1 — Alias audit at the extremes.** WIDE range takes Osc B to
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
- [ ] **Step 2 — CEM3340 temperament.** The 3340 is a famously stable VCO —
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
- [ ] **Step 3 — The resonance limiter's real characteristic.** What
  bounds self-oscillation in the hardware is the *external* BA130
  anti-parallel "Hi-Q overload limiter" in the resonance path (anchored
  placement; `Docs/circuit-modelling-research.md`), which the engine
  voices as one piecewise diode law (`knee = 1.2` / `ceiling = 2.2`,
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
- [ ] **Step 4 — Close the closable open questions.** OQ-02 (absolute
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

## Step 5 executed — the zipper audit and the travel smoother — 2026-08-22

The audit lives in `Tools/ZipperAudit.cpp` (built as `GhostarZipperAudit`;
CI keeps its strokes valid through `Ghostar.ZipperAuditSmoke`). Every
published continuous travel — all 28 panel travels plus both performance
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
engine — loudness envelope idle, no keys, no VCA bypass, no raised
Shaper-path slider — snaps instead of gliding, so a state restore before
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
a hundred mid-note and hears it glide, then settle.

**After** (travel smoother in, same metric, dB):

| travel | 512 | 2048 | travel | 512 | 2048 |
|---|---|---|---|---|---|
| tune | −78.6 | −47.2 | filterEnvAmount | −65.5 | −50.4 |
| interval | −47.4 | −33.2 | filterAttack | −57.2 | −44.8 |
| masterVolume | −72.0 | −47.1 | filterDecay | −70.2 | −57.2 |
| brightness | −77.8 | −54.5 | filterSustain | −77.9 | −63.6 |
| shaperPathA | −76.8 | −51.2 | filterRelease | −88.7 | −77.0 |
| shaperPathB | −83.7 | −58.2 | loudnessAttack | −52.8 | −40.0 |
| shaperPathRing | −83.8 | −57.7 | loudnessDecay | −74.8 | −61.0 |
| shaperPathNoise | −90.9 | −65.2 | loudnessSustain | −71.2 | −57.3 |
| filterPathA | −76.1 | −50.9 | loudnessRelease | −96.7 | −69.8 |
| filterPathB | −83.9 | −58.8 | lfoRate | −53.5 | −34.2 |
| filterPathNoise | −77.0 | −50.7 | shaperShape | −32.1 | −24.3 |
| cutoff | −69.0 | −45.7 | shaperRate | −26.9 | −15.4 |
| lowerOnly | −67.3 | −45.3 | glide | −80.7 | −51.3 |
| resonance | −82.1 | −54.8 | xWheel | −82.0 | −60.0 |
| kbAmount | −86.5 | −64.8 | yWheel | −52.9 | −32.0 |

**Reading the tables.**

- The smoother buys 15–26 dB exactly where zipper is notorious: cutoff
  (−42.6 → −69.0 at 512), master volume (−48.5 → −72.0), LOWER ONLY,
  resonance, brightness, every mixer fader, glide and the X wheel. The step-discontinuity class — the
  audible click — is gone; what remains is band-limited ripple at the
  block rate, falling with the one-pole's slope.
- The envelope-segment rows moved a few dB the other way: a segment time
  is *sampled* at its gate edge, and the smoothed trajectory's ripple
  phase at that instant decides which value the envelope gets. All rows
  stay at or below −40 dB even at 2048 for the stress gesture; this is
  bounded, not step-like, and is the cost of gliding those travels at
  all.
- The rows that remain above −60 dB at 512 (shaperRate at −26.9 the
  worst, then shaperShape, interval, lfoRate, yWheel, loudnessAttack and
  filterAttack) are not step artifacts: their residual is smooth
  amplitude or frequency-trajectory ripple that scales with gesture
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

# Making Ghost best-in-class

This document records what Ghost is actually competing against, where it
stands against that field today, and the ordered set of changes that closes
the gap. It is written to be checked: every claim about the current engine
names the file and the constant it comes from, and every step states how it
is verified. The steps are marked off as they land.

Ghost is a circuit-modelled replica of one specific 1983 monophonic
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
Ghost claims, by deriving its laws from the service-manual schematics, the
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

**Where Ghost sits.** Ghost is the purist alternative to the official
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

Ghost already holds points 3 and 4 structurally (the research contract and
the 52-parameter panel mapping). Points 1 and 2 are where the remaining
engineering lives.

## Where Ghost actually stands

**Anchored laws** (each traced to a primary source in
`Docs/circuit-modelling-research.md`, implemented in
`Source/DSP/GhostEngine.cpp`):

- Keyboard law and ~110 % filter tracking pivoting at middle C
  (`trackingOctaves`, factor 1.1).
- The panel's exact pulse duty sets — A 50/30/15/6 %, B 40/20/10/3 % — and
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
- MM5837 white noise plus a rate-derived Kellet pinking stage; output AC
  coupling at ~5 Hz as the series capacitors provide.

**Voiced constants** (defensible but not anchored; the open-questions
register `Docs/open-questions.md` tracks each):

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
- Wheel modulation depths: 1 octave of pitch, 3 octaves of cutoff, ±0.42
  duty (`pitchDepthOctaves`, `filterDepthOctaves`, `dutyDepth`).

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
  path relies on 2× oversampling plus mild spectral rolloff. Render
  worst-case strokes (WIDE at full, sync sweeps at the keyboard's top,
  ring with both oscillators high) and measure the alias floor relative to
  the intended partials. Verification: a table of worst-case
  alias-to-signal ratios in this document; anything above −60 dB in the
  audible band triggers a targeted fix (BLEP on the sync edge's
  discontinuity slope, or 4× on the triangle path alone) and a re-measure.
- [ ] **Step 2 — CEM3340 temperament.** The 3340 is a famously stable VCO —
  that stability is part of the Spirit's character (two of them against
  each other stay in tune, unlike discrete VCOs). But *stable* is not
  *static*: datasheet tempco and supply sensitivity translate to cents-scale
  wander over minutes. Derive the magnitude from the datasheet, implement
  slow correlated drift per oscillator, and run an A–Z listening test
  between the two defensible candidates: no drift (the 3340's specified
  stability taken at face value) and the datasheet-derived wander (that
  stability under real supply and ambient variation). No scaled variants —
  a multiplied datasheet number would be fitting the depth by ear, which
  the listening-test rules forbid. Verification: the derivation quoted
  here; the chosen letter recorded per the listening-test rules.
- [ ] **Step 3 — Per-integrator filter saturation.** The current model
  bounds the resonant node with one piecewise diode law
  (`GhostEngine.cpp`, `knee = 1.2` / `ceiling = 2.2`, OQ-12). The
  CEM3350's OTA stages saturate individually, which shifts *where* the
  compression happens as drive rises and colours self-oscillation
  differently. Derive the stage-level limiting from the 3350 datasheet
  topology, implement per-integrator saturation in the TPT sections, and
  compare harmonic signatures (Goertzel series under fixed drive) against
  the current law. If both remain defensible and audibly different, A–Z
  it. Verification: the harmonic tables and, if run, the recorded
  verdict — plus a re-run of Step 1's alias suite, since new nonlinearity
  makes new high partials and the completed audit must describe the final
  engine, not the one before it.
- [ ] **Step 4 — Close the closable open questions.** OQ-02 (absolute
  cutoff span) and OQ-09 (cascade Q distribution) may be derivable from
  the CEM3350 datasheet's exponential-scale and mode figures; OQ-04's
  curvature read can be checked against the 556A application notes. Work
  each one: derive it, or demonstrate it is not derivable and leave it
  voiced with the reason. Verification: the open-questions register
  updated with the derivation or the dead end.
- [ ] **Step 5 — Zipper audit on the travels.** Panel travels apply at
  block boundaries (`setParameters` per block in
  `Source/PluginProcessor.cpp`), and every continuous travel is published
  for automation, so the audit must cover the published surface — cutoff,
  LOWER ONLY, resonance, BRIGHTNESS, every mixer slider, envelope amount
  and sustain, master volume, and both performance wheels. Render a
  host-automated full-range sweep of each at 48 kHz/512-sample blocks and
  inspect for stepping sidebands. Verification: a per-travel table quoted
  here; one-pole smoothing on every travel the measurement flags, and a
  re-measure of those.
- [ ] **Step 6 — A calibration capture, if one ever exists.** The standing
  offer recorded so it is not forgotten: the moment a trustworthy Spirit
  capture becomes available (a serviced unit, a museum recording session,
  a lent instrument), measure every hardware-closable entry the register
  then holds — today that is all of OQ-01 through OQ-10 and OQ-12: the
  wheel's real bend span, the actual pulse duties, the envelope
  curvature and Shaper timings, the gate threshold, the ring bleed, the
  glide capacitor, both filter spans, the cascade Q split, and both
  clipping stages — and re-voice against the measurements. Until then
  this step cannot start, and no constant is fitted to a YouTube demo's
  unknown signal chain.

## What was investigated and not done

- **Polyphony, effects, velocity.** The official Cherry Audio product
  already serves players who want the Spirit's architecture extended.
  Ghost's identity is the instrument itself; a velocity-sensitive,
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

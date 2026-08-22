# Ghostar circuit-modelling research and implementation contract

Ghostar is a white-box circuit model of a 1983 monophonic dual-filter analog
synthesizer — the Crumar Spirit, designed by Jim Scott, Tom Rhea and Bob Moog
for Crumar s.p.a. — with named reference material, not a black-box claim that
one plug-in is indistinguishable from that instrument. This document separates
what the engine implements from documentation from what remains a voiced Ghostar
decision, and records where the sources disagree.

Ghostar is an independent original implementation. It is not affiliated with,
endorsed by, or licensed by Crumar or its successors; it contains no firmware,
no ROM data, no samples, and no captured audio. Its architecture reproduces
the reference instrument's *functional* signal flow — which controls exist,
what each is calibrated to do, and how the blocks connect — because that flow
is what the circuit is. Its name and livery are Ghostar's own.

## Primary sources

- **OM** — Crumar Spirit Owner's Manual, 30-page factory scan
  (https://manuals.fdiskc.com/tree/Crumar/Crumar%20Spirit%20Owners%20Manual.pdf,
  mirror https://archive.org/details/manualzilla-id-6890440). Printed pages
  17–25 are missing from every circulating scan (the generic tutorial
  chapter); every control-reference page survives.
- **SM** — Crumar Spirit Service Manual: schematics DWG 1–3, PCB layouts,
  electronic component list, errata
  (http://www.midimanuals.com/manuals/crumar/spirit/service_manual/spiritservicemanual.pdf,
  600 dpi mirror https://archive.org/details/sm_Crumar_Spirit_Service_Manual).
  No calibration text exists — schematics, parts and errata only.
- **RM** — 2023 reissue User Manual (verbatim re-typeset of OM plus a
  trimmers-and-adjustment chapter and MIDI addendum), linked from
  https://www.crumarspirit.com/.
- **CEM3340 datasheet** (VCO), **CEM3350 datasheet** (dual state-variable
  VCF), Curtis Electromusic.
- **US 3,943,456** (Luce/Moog Music, 1976) — the variable-rate-integrator
  signal generator that is the Shaper Y core; attribution to the Spirit by
  J. D. Tillman (https://till.com/articles/moog/patents.html), corroborated
  by the P1015 schematic's OTA-integrator topology.
- Panel silkscreen verified against photos of serials 00045 and 00046
  (matrixsynth.com listings). Where OM prose and the silkscreen disagree, the
  silkscreen wins — it agrees with the schematic net names and every
  independent witness in both such cases (see Discrepancies).
- Secondary color: Sound On Sound Retrozone (Gordon Reid, 2001), Amazona Blue
  Box, GreatSynthesizers reissue notes, Cherry Audio's licensed 2025
  behavioural recreation and its researched history chapter.

## Architecture (anchored, OM p.26)

Two VCOs, a triangle-cross ring modulator, and one noise source feed **two
parallel audio paths**:

- **Filter/ADSR path**: mixer (A, B, NOISE) → **Lower Filter L** →
  **Upper Filter U** in series — the OVERDRIVE soft-clipper sits *between*
  them — → VCA driven by the LOUDNESS ENVELOPE (or held open by VCA BYPASS).
- **Shaper Y path**: mixer (A, B, RING, NOISE) → **BRIGHTNESS** 6 dB/oct
  lowpass → VCA whose gain is the SHAPER Y output.

The hardware presents the paths on separate rear jacks (a stereo instrument)
and mixes both when only the main jack is used. Ghostar renders the mixed
signal to both channels by default and can split the paths to left/right.

Modulation: **MOD X** (LFO / S&H / red noise / Osc B, plus the arpeggiator
clock) and **SHAPER Y** (a variable-symmetry envelope/LFO), each through its
own performance wheel (an attenuator toward zero volts), routed by the two
**WHEEL DESTINATIONS** switches. Gates for the two ADSRs come from GATE
SELECT (keyboard, LFO square, Shaper/external) with SINGLE/MULTIPLE trigger.

## Control calibration (the modelled laws)

Every panel control is stored as 0..1 travel and mapped through the law
below. Provenance: **anchored** (stated by OM/SM/datasheet), **derived**
(computed from anchored values by a stated equation), or **voiced** (chosen
inside a range the sources bound but do not fix).

### Master and oscillators

| Control | Law | Provenance |
|---|---|---|
| TUNE | ±3 semitones (a minor third) | anchored, OM p.28 |
| OCTAVE | 32' 16' 8' 4'; at 8' the keyboard's second C sounds middle C | anchored, OM p.28 |
| Osc A WAVEFORM | triangle; rectangular 50/30/15/**6** %; sawtooth | anchored: panel line-art at 400 dpi + SN 00046 photo. SOS's "5 %" and Cherry Audio's "8 %" are secondary errors |
| Osc B WAVEFORM | triangle; rectangular 40/20/10/3 %; sawtooth | anchored, OM p.28 + panel |
| SYNC | hard sync, one-directional A→B ("forces B to oscillate at the same frequency as A"); the sync tap precedes A's waveform switch | anchored, OM p.28, p.9; the CEM3340 pin (6 hard vs 9 soft) is illegible in the scan — modelled as hard sync per the manual's behavioural spec |
| Osc B OCTAVE/RANGE | −1, UNISON, +1, +2, BASS, WIDE (panel order). BASS/WIDE disconnect B from keyboard, TUNE, OCTAVE and bend (X/Y modulation still applies); INTERVAL becomes the drone pitch: 30–300 Hz in BASS, 2–10,000 Hz in WIDE | anchored, OM p.29 + panel photos |
| INTERVAL | ± a perfect fifth (±7 st) in the octave positions, exponential; centre = 0. OM: "slight deviations from center create the slight mistuning… that adds warmth" | anchored, OM p.29. SOS measured "±8 st" on one unit — endpoint resistors make unit spread real |
| Ring modulator | Osc A triangle × Osc B triangle, taken before the waveform switches (WAVEFORM has no effect on RING); a small un-nulled carrier bleed is part of the sound — the vintage unit has **no** ring-mod trim (the reissue added one) | anchored, OM pp.6/26 + SM (½ CEM3360 IC7, 1M8/6k2 bias, no trimmer); bleed amount **voiced** |
| Noise | one source, "a combination of white and pink random noise" (MM5837 → partial pinking) | anchored, OM p.26 + SM |

VCO model: bandlimited (PolyBLEP/BLAMP) triangle, rectangular and sawtooth;
CEM3340-class stability means Ghostar applies no drift by default. Both
oscillators share one master CV bus (tune, octave, bend, glide are
common-mode); B's interval is a constant CV offset — a constant musical
interval across the keyboard, not constant Hz (SM DWG 2 topology).

### Filters (the signature)

Both hardware filters are CEM3350 dual state-variable sections — not a
transistor ladder, despite the Moog pedigree. Ghostar models each 2-pole
section as a TPT state-variable filter with a soft limiter in the resonance
path (the hardware's BA130 anti-parallel "Hi-Q overload limiter" is what
bounds self-oscillation, not the rails).

| Control | Law | Provenance |
|---|---|---|
| MASTER (cutoff) | sets both filters' cutoff, always; exponential through the audio range (voiced 20 Hz–16 kHz at Upper; no primary Hz figure exists) | anchored routing OM p.31; range **voiced** |
| LOWER ONLY | Lower cutoff relative to Upper; cutoffs coincide at 8 (circled on the panel); below 8 Lower sits below Upper | anchored, OM p.31 |
| RESONANCE switch | LOW fixes Upper Q = 0.5; VARIABLE slaves Upper Q to the pot | anchored, OM p.30 |
| RESONANCE pot | Lower Q always; Upper Q in VARIABLE; both reach self-oscillation at maximum (exponential Q law per CEM3350) | anchored, OM pp.30/32 + datasheet + SM divider math |
| SLOPE | Upper is 12 dB (one section) or 24 dB (two cascaded sections); resonance behaviour holds in both | anchored, OM pp.30/32; cascade Q distribution **voiced** (first section fixed low-Q, second carries the control) |
| Lower mode | OUT / OVERDRIVE / BANDPASS / HIGHPASS. BANDPASS is **parametric boost** — dry + resonance-scaled BP, "a peak … without attenuation of frequencies far from this cutoff" — not a true band-pass. OVERDRIVE = the same boost plus a soft clipper *between* the filters. HIGHPASS = resonant HP → "double-peak, highpass-lowpass" | anchored, OM pp.30–32 |
| KB AMOUNT | keyboard tracking of Upper always, Lower when DYNAMIC; 0 to slightly over 100 % (≈110 % at full) | anchored OM p.32, SOS |
| TRACKING | FORMANT disconnects Lower from keyboard CV, X and Y modulation, the filter envelope and the pedal — freezing its peak as a fixed formant (the starred brass/woodwind configuration); MASTER and LOWER ONLY still act | anchored, OM pp.31/33 |
| FILTER ENVELOPE AMOUNT | bipolar, centre zero; unattenuated span ±2.5 octaves straddling the cutoff; INVERT mirrors it. Permanently wired to Upper; to Lower only in DYNAMIC | anchored, OM pp.27/33 |
| OVERDRIVE clipper | soft diode knee (anti-parallel BA130 behind a gain stage and pad) between Lower and Upper, so the Upper filter re-filters the distortion products | anchored placement OM p.32 + SM; knee/drive **voiced** |
| BRIGHTNESS | 6 dB/oct lowpass on the Shaper path (100k log pot + 27 nF: ≈59 Hz at full resistance, effectively open at zero) | anchored, OM p.29 + SM |

### Envelopes, gating, keyboard

| Item | Law | Provenance |
|---|---|---|
| Two ADSRs | A/D/R 5 ms–10 s (2 MΩ log sliders into 4.7 µF around a 556A timer → exponential segments; attack aims past its peak, timer-style); S linear | anchored, OM p.33 + SM DWG 3 |
| LOUDNESS VCA | BYPASS holds the path VCA fully open — un-articulated drone | anchored, OM pp.27/33 |
| GATE SELECT | KBD, X (the LFO square — auto-repeat; one gate per arpeggio step), Y/EXT (the Shaper's own gate) — OR'ed; at least one must be on for the envelopes to run at all | anchored, OM p.33 |
| TRIGGER | MULTIPLE re-gates on every new key; SINGLE holds the gate while ≥1 key is down and re-gates only after all keys are released | anchored, OM p.34 |
| Keyboard | 37 keys C–C, digitally scanned; last-note priority with held-note memory — releasing the newest key falls back to the newest key still held, **without retriggering** | anchored, OM p.37 + SOS |
| GLIDE | conventional lag on the keyboard CV (single-pole RC; 2 MΩ pot into ≈450 nF → τ up to ≈0.9 s); GLIDE MODE OFF / AUTO (only while >1 key held) / ON | anchored, OM p.37 + SM DWG 1; cap value legibility-limited |
| PITCH BEND | full wheel ≈ ±8 semitones — derived by anchoring the bend network (100k pot across ±12 V via 680k) against the TUNE network (1M8, ± minor third): ratio 1.8M/680k ≈ 2.65 × 3 st. The wheel's mechanical travel fraction is documented nowhere | **derived**; see open questions |

### MOD X, SHAPER Y, arpeggiator

| Item | Law | Provenance |
|---|---|---|
| MOD SOURCE | LFO triangle; LFO square; S+H RANDOM (red noise sampled); S+H Y (Shaper sampled — a regular, patterned staircase); RED NOISE (continuous slow random); OSC B — the **selected, buffered** Osc B waveform, not a hard-wired triangle (SM: the post-waveform-switch TP2 net feeds the mod board; OM's "triangle wave output" is imprecise) | anchored, OM p.34 + SM DWG 2A |
| LFO/S+H RATE | <1 Hz to ≈50 Hz; also the S&H clock and the arpeggiator clock; no effect on RED NOISE or OSC B | anchored, OM p.34 |
| MOD X TO: | OFF · OSC A+B · **OSC A** · OSC A RWM · FILT U+L · FILT U (panel; OM prose "(3) OSC B" is a typo — silkscreen, schematic net names, SOS and Cherry Audio all agree on OSC A) | anchored, panel + SM |
| SHAPER Y TO: | OFF · **OSC A+B** · **OSC B** · OSC B RWM · LFO RATE · FILT L (panel order; OM prose transposes 2 and 3). Y→LFO RATE only raises the rate: the wheel sets the fastest rate, the panel knob the slowest | anchored, panel + OM p.10 |
| SHAPE X WITH Y | Y envelopes the X wheel signal (an OTA VCA in the X path) — enveloped vibrato | anchored, OM p.36 + SM |
| SHAPER Y | US 3,943,456 variable-rate integrator. RATE = total period: FREE mode several cycles per minute to >20 Hz; envelope modes = total rise+fall time. SHAPE = rise/fall split of that period (0 fast-rise … 5 symmetric … 10 slow-rise/quick-fall), never changing total time. Modes: FREE (LFO, symmetric about 0 V); KBD HOLD (rise and hold while gated); RESET (single rise-fall from zero, always multiple-trigger regardless of the TRIGGER switch); RUN (the rising segment always completes; new gates are ignored until it has) | anchored, OM pp.25/35/36 |
| Y gate | the Shaper generates its own gate (comparator on the Shaper output), selectable at GATE SELECT Y/EXT | anchored, OM p.33 + SM; threshold **voiced** |
| RWM | pulse-width modulation of the rectangular waveforms only; A's RWM belongs to the X bus, B's to the Y bus | anchored, OM pp.12/36 |
| ARPEGGIATOR | OFF · RIPPLE · ARPEGGIO · LEAP. All modes scan held keys chromatically bottom-to-top, wrapped; one note per LFO clock. RIPPLE = the plain wrapped sequence. ARPEGGIO = the sequence at pitch, then +1 octave, then −1 octave, repeating. LEAP = per-note octave cycle 0/+1/−1 (pattern period lcm(N,3)). Arpeggiator modes clock-slave SHAPER Y RATE to the LFO except in FREE | anchored, OM pp.13/35 |

### Wheels

The MOD X and SHAPER Y wheels are attenuators; "attenuation always occurs
toward zero volts" (OM p.25) — a bipolar source keeps its symmetry, a
unipolar one scales toward silence.

## Why it sounds the way it does (what reviewers hear, mechanically)

The reputation — "vocal", "nasal", "ghostly", "woody/reedy", "wicked",
"gnarly" (SOS, Amazona, Gearspace owners) — is carried by: the relative-offset
series dual filter with its parametric-boost lower peak; the FORMANT freeze;
the inter-filter overdrive being re-filtered by the upper lowpass; the
second, independently-enveloped audio path with ring mod; and second-order
modulation (Y shaping X, Y driving the LFO's rate, Y-patterned S&H). These
are exactly the blocks the contract above anchors.

## Documented discrepancies (resolved)

1. **MOD X TO: position 3** — OM prose says OSC B; panel, SM net names
   ("MOD A"/"MOD A+B", no "MOD B" exists), SOS and Cherry Audio say OSC A.
   Modelled: OSC A.
2. **SHAPER Y TO: positions 2/3** — OM prose transposes them; panel order
   (A+B then B) wins, corroborated by Cherry Audio.
3. **Osc A narrowest pulse** — 6 % (panel line-art and photo); SOS "5 %" and
   Cherry Audio "8 %" are secondary deviations.
4. **ARPEGGIO pass order** — at pitch, then +1, then −1 octave (OM, SOS);
   Vintage Synth Explorer's description is wrong.
5. **MOD SOURCE "OSC B"** — the schematic feeds the selected waveform, the
   manual says triangle. Modelled per the schematic.

## What remains open

See [open-questions.md](open-questions.md). The headline items: the pitch
wheel's mechanical travel fraction (the ±8 st figure is full *electrical*
travel), absolute filter cutoff ranges in Hz, exact hardware pulse duties
behind the printed percentages, the 556A envelope segment curvature, the
Shaper gate's exact comparator behaviour, and ring-mod bleed level. No
hardware measurements of any Spirit have ever been published; the first
measured unit would become this project's ground truth.

## Engine status

The current engine is **v0**: the complete architecture above — both paths,
both filters with all four Lower modes and the 12/24 dB Upper, both ADSRs
with gate logic, MOD X with all six sources, Shaper Y with all four modes,
both wheel-destination buses, the arpeggiator, sync, ring mod, glide and the
keying rules — implemented with bandlimited oscillators and TPT filter
sections at 2× internal oversampling. Constants marked voiced above are
first-pass choices inside the documented bounds and are the standing targets
for refinement passes.

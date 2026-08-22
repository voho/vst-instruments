# SH-201 replica: research and implementation contract

Septum models the voice architecture of the Roland SH-201 (2006), a
10-voice virtual-analog synthesizer. Unlike its sibling project YouKnow106 —
whose subject is an analog signal path — the SH-201's sound engine is pure
DSP: the service notes show a single Roland custom DSP ("WSP", Toshiba
gate-array T6TZ3AFG-0001) computing all synthesis and effects, with no PCM
wave ROM anywhere in the design, framed by a documented analog input/output
stage. A faithful replica is therefore a *behavioral model of a DSP engine*,
grounded in Roland's published documents, plus a component-level model of the
analog output path. This document records, mechanism by mechanism, what is
settled by which source, what is reported second-hand, and what remains a
voiced choice that a future measurement should replace.

Nothing in this project contains Roland firmware, ROM data, samples, captured
audio, or factory patch data. Factory patch *names* are quoted from the
owner's manual for reference only; every preset shipped here is an original
sound programmed against the engine.

## Evidence tiers

- **settled** — stated in a primary Roland document that was read directly
  (Owner's Manual, MIDI Implementation, Service Notes, official leaflet).
- **reported** — a secondary source: a measurement of related hardware
  (JP-8000), a magazine review, a forum observation from unit owners.
- **voiced** — a choice this project made because no source pins the value.
  Every voiced constant is listed in [open questions](#open-questions) with
  the measurement that would close it.

## Primary sources

| Source | What it settles |
| --- | --- |
| Roland SH-201 Owner's Manual, 84 pp., © 2006 (`SH-201_OM.pdf`, static.roland.com) | Complete architecture, panel controls, parameter list with ranges (pp. 60–70), CC map (p. 72), MIDI chart (p. 73), specifications (p. 74), block diagram (p. 75), factory patch names (p. 84) |
| Roland SH-201 MIDI Implementation v1.00, 2006-03-01, 9 pp. | The definitive parameter contract: full SysEx address map with every parameter's raw range and display mapping, enumeration orders, LFO sync-note table, effect frequency tables |
| Roland SH-201/SH-201C Service Notes, May 2006, doc 17058418E0 | Hardware: NEC V850E/ME2 CPU, WSP DSP + private SDRAM, AK4552 codec, TUSB3200 USB clock master (384fs), complete analog I/O component values, factory test levels |
| SH-201 Leaflet addendum (`SH-201_AD.pdf`) | Effect template names, `-5` → `5th` INTERVAL spec change, factory reset |
| Adam Szabo, *How to Emulate the Super Saw*, KTH thesis, 2010 | JP-8000 Super Saw measurements: 7-oscillator offsets, detune-curve polynomial, mix laws, pitch-tracked HPF, free-running phases |
| Roland JP-8000 Owner's Manual + Supplemental Notes SN77 | The ancestor's supersaw/FB-OSC control semantics ("7 saw waves using only one voice of polyphony") |
| Sound on Sound, *Roland SH-201* (Nick Magnus, April 2007) | V-Synth-derived engine, 5+5 dual/split voices, envelope snappiness, EXT-IN routing behavior, 24 dB filter "similar in character to that of the JP8000" |
| AKM AK4552 datasheet | Codec conversion characteristics: 24-bit delta-sigma, digital-filter passband 0.454·fs, DAC output level |

Real-unit references for by-ear comparison are catalogued in the
[audio demos README](audio/README.md): 18 official Roland patch/song demo
MP3s on roland.com, 33 more on the 2007 rolandus.com patch mini-site, and a
set of verified YouTube demos of real units.

## Architecture (settled)

One patch contains two complete synthesizer **tones**, UPPER and LOWER; each
tone is `OSC1 + OSC2 → MIX/MOD → FILTER → AMP`, modulated by one two-stage
PITCH ENV (shared A/D, per-oscillator depth), FILTER ENV (ADSR + depth),
AMP ENV (ADSR), and two identical LFOs. Both tone outputs feed a shared
effects block in which DELAY feeds REVERB in series, each tone with its own
delay and reverb send depth. Keyboard modes are SINGLE (one tone), DUAL
(layered) and SPLIT (split point A0–C8). Maximum polyphony is 10 voices;
DUAL halves it to 5. (OM pp. 27, 46–47, 74–75.)

The engine runs on one clock domain rooted in the TUSB3200's PLL (the WSP is
strapped `SYNC=SLAVE`, `SYSCKO=384fs`), so engine fs = USB streaming fs. No
Roland document read states the numeric rate; 44.1 kHz is the common
assumption and both codec and USB controller support 44.1 and 48 kHz.
**Open: OQ-01.** The replica runs at the host rate and renders its committed
demos at 44.1 kHz.

## The parameter contract (settled)

The MIDI Implementation's address map is adopted verbatim as the engine's
parameter surface. Every continuous parameter is 7-bit; signed displays use
`display = raw − 64` (e.g. depth −63…+63 is raw 1–127). The structures:

- **Patch Common** (0x21 bytes): name ×12, patch level 0–127, tone balance
  −63…+63, tempo 5–300, keyboard mode SINGLE/DUAL/SPLIT, keyboard part,
  split point A0–C8, controller destinations, arpeggio/delay/reverb
  switches, modulation assign (OSC1&OSC2 / OSC1 / OSC2 / PW1 / PW2 / FILTER /
  AMP / AUDIO-FILTER), D-Beam assign (37 destinations), D-Beam polarity.
- **Patch Tone** ×2 (0x40 bytes each): per oscillator — waveform
  (0–8: SAW, SQU, PW-SQU, TRI, SINE, NOISE, FB-OSC, SUPER-SAW, EXT-IN),
  pitch-wide switch, coarse −36…+36 st, fine −50…+50 cents, pulse width
  0–127, pitch-env depth −63…+63; pitch env A/D 0–127; mix/mod type
  (MIX/SYNC/RING), balance −63…+63, low freq (FLAT/BOOST/CUT); filter type
  (0–3: BYPASS, LPF, HPF, BPF), slope (−12/−24 dB), cutoff 0–127, key
  follow −200…+200 (raw steps of 10), cutoff velocity sens −63…+63,
  resonance 0–127; filter env A/D/S/R + depth; overdrive switch + drive
  0–127; amp level, level velocity sens, pan L64–63R; amp env A/D/S/R;
  delay depth, reverb depth; LFO1/LFO2 shape (0–6: TRI, SIN, SAW, SQR, TRP,
  S&H, RND), rate 0–127, tempo-sync switch + note (20 values: 16, 12, 8, 4,
  2, 1, 3/4, 2/3, 1/2, 3/8, 1/3, 1/4, 3/16, 1/6, 1/8, 3/32, 1/12, 1/16,
  1/24, 1/32 whole notes), fade time, key trigger, destination 1
  (PITCH1/PW1/FILTER/AUDIO-FILTER) + depth, destination 2 (PITCH2/PW2/AMP)
  + depth; bend range 0–24 st; octave shift −3…+3; portamento switch +
  time; mono/solo select (POLY, SOLO+LEGATO, SOLO).
- **Patch Delay** (5 bytes): time 0–127; feedback −98…+98 % (negative
  inverts phase); HF damp 200–8000 Hz in 17 steps or BYPASS; modulation
  rate 0–127; modulation depth 0–127.
- **Patch Reverb** (10 bytes): time 0–127; pre-delay 0–100 ms; size 1–8;
  high cut 160–12500 Hz in 20 steps or BYPASS; density 0–127; diffusion
  0–127; LF damp 50–4000 Hz in 20 steps, gain −36…0 dB; HF damp
  4000–12500 Hz in 6 steps, gain −36…0 dB.
- **System Common**: master tune 415.30–466.20 Hz (0.1-cent steps around
  A440), master key shift −24…+24, master level, transpose −5…+6, octave
  shift −3…+3, pedal/D-Beam configuration.

The arpeggiator map (grid, duration, motif, 32-step × 16-note pattern) is
settled by the same document and implemented; see the arpeggiator section
below.

### CC map (settled, OM p. 72)

Part: Bank MSB 0, Modulation 1, Level 7, Pan 10, Expression 11, Bank LSB 32,
Hold 64, Sostenuto 66, D-Beam pitch 69, Portamento control 84.

| Parameter | UPPER | LOWER |
| --- | --- | --- |
| OSC1 pitch / detune / PW / p.env depth | 20 / 76 / 3 / 24 | 78 / 79 / 80 / 70 |
| OSC2 pitch / detune / PW / p.env depth | 21 / 77 / 95 / 25 | 85 / 86 / 87 / 88 |
| Pitch env A / D | 26 / 27 | 89 / 90 |
| Mix/mod balance | 8 | 9 |
| Filter cutoff / key follow / resonance | 74 / 30 / 71 | 102 / 103 / 104 |
| Filter env A / D / S / R / depth | 82 / 88\* / 28 / 29 / 81 | 105 / 106 / 107 / 108 / 109 |
| Amp level | 14 | 15 |
| Amp env A / D / S / R | 73 / 75 / 31 / 72 | 110 / 111 / 112 / 113 |
| Delay / reverb depth | 93 / 91 | 94 / 92 |
| LFO1 rate / depth1 / depth2 | 16 / 18 / 19 | 114 / 115 / 116 |
| LFO2 rate / depth1 / depth2 | 17 / 22 / 23 | 117 / 118 / 119 |

Effects: delay time 12, reverb time 13. Audio filter: cutoff 2, resonance 4.

\* The printed table assigns **CC#88 twice** — UPPER filter-env decay *and*
LOWER OSC2 pitch-env depth — which one physical controller number cannot
serve. CC#83 is the only number in the chart's declared transmit range
(69–83) that the table never uses, and the LOWER OSC2 block (85, 86, 87, 88)
is a clean run, so this project reads UPPER filter-env decay as a misprint
for CC#83 and implements CC#83, keeping CC#88 for LOWER OSC2 pitch-env
depth. A hardware capture of the panel's transmitted CCs would settle it
(OQ-02).

## Mechanisms

### Classic waveforms (SAW, SQU, PW-SQU, TRI, SINE, NOISE)

Waveform identities and the PW semantics are settled: pulse width is "the
width of the high portion as a percentage of the cycle", left approaches a
square (50 %), right broadens. The manual's claim that TRI contains "even-
numbered" harmonics is physically wrong (a triangle has odd harmonics at
1/n²) and contradicts its own correct SQR description; the replica
implements a textbook triangle. Rendering is polyBLEP/polyBLAMP
band-limited at the host rate — the real unit's oscillators alias audibly
(reported by owners and framed by Roland forum staff as part of the
character); reproducing that aliasing exactly would require knowing the
engine's true rate and interpolation, which is OQ-01. NOISE is white
(spectral color unmeasured — voiced, OQ-03). PW 0–127 → duty 50–95 %
(endpoints voiced, OQ-03).

### SUPER SAW

Settled (OM): seven sawtooth waves inside one oscillator; PW/FEEDBACK knob
sets the pitch spread; DETUNE shifts all seven together; string/rich-tone
intent. Reported (Szabo's JP-8000 measurements, which every later Roland
hardware supersaw descends from; SH-201 owners report the raw oscillators
sound the same):

- Fixed per-oscillator offsets at full detune, relative to the center
  frequency: −0.11002313, −0.06288439, −0.01952356, 0, +0.01991221,
  +0.06216538, +0.10745242 (independently cross-checked against a KVR
  reverse-engineering giving ≈5/256, 16/256, 28/256).
- The detune knob maps through Szabo's fitted 11th-degree polynomial
  (implemented verbatim; rises slowly to 0.5, steeply after 0.9).
- Free-running phases: each note-on randomizes all seven phases; the saws
  are *not* band-limited — the aliasing above the fundamental is part of
  the signature sound.
- The summed stack passes through a high-pass filter tracked to the note's
  fundamental, removing the folded noise below the fundamental. Implemented
  as an RBJ 2nd-order high-pass at 1.0× the fundamental, Q = 0.707
  (Szabo's reading: the fundamental is only slightly reduced). A KVR fit
  proposes 2.5× fundamental at Q = √2 instead; both are defensible, so this
  is a standing A–Z listening-test candidate (OQ-04).
- The SH-201 has no supersaw MIX control. Szabo's JP-8000 mix laws (center
  `−0.55366·m + 0.99785`, sides `−0.73764·m² + 1.2841·m + 0.044372`) are
  evaluated at a fixed m. This project voices m = 0.75 — just past the
  center/side equality point (m ≈ 0.737), leaving the sides ~0.15 dB above
  the center — consistent with the SH-201's "seven saws played
  simultaneously" framing. Voiced (OQ-05).

### FB OSC

Settled: "a tone containing high overtones, similar to feedback on a
guitar"; the knob feeds output back to input, right = more overtones, more
aggressive. The JP-8000 original had a second HARMONICS control and forced
mono; the SH-201 exposes one control and stays polyphonic. Reported (KVR):
the mechanism behaves like a sawtooth feeding a comb filter and aliases
heavily. Implemented as a sawtooth with a feedback comb: the oscillator's
output, soft-clipped, is delayed by **half the fundamental period** and
added back scaled by the knob (0…1.15, values past unity held bounded by
the soft clip). The half-period delay reinforces even harmonics — the
octave-up emphasis of guitar feedback. The delay ratio and gain law are
voiced (OQ-06); the comb mechanism itself is reported, not settled.

### MIX/MOD

Settled: TYPE cycles MIX → SYNC → RING. SYNC restarts OSC1's cycle at each
OSC2 cycle start (effective with OSC1 above OSC2); implemented as a naive
hard-sync reset — the modelled DSP's own sync aliases audibly, and no source
documents band-limiting there. RING multiplies
OSC1 × OSC2; with BALANCE fully left the ring product alone is heard — so
the ring product occupies the OSC1 leg of the balance crossfade. BALANCE
−63…+63: each leg at unity at center, the opposite leg attenuating linearly
to silence at the extremes (the law is voiced; only the endpoints are
settled, OQ-07). LOW FREQ CUT/FLAT/BOOST is a first-order low shelf,
voiced at 200 Hz ± 8 dB (OQ-07); the manual settles only cut/flat/boost
semantics and the "rich bass" intent.

### FILTER

Settled: LPF/HPF/BPF/BYPASS; −12 or −24 dB/oct; cutoff and resonance
0–127; resonance far right reaches sustained self-oscillation (manual
warning); KEY FOLLOW −200…+200 in steps of 10, with the p. 36 diagram
placing the pivot at C4 and +100 tracking the keyboard 1:1 (2:1 at +200 —
inferred from the printed graph). Reported: the 24 dB filter is "similar in
character to that of the JP8000" (SoS) — a clean digital resonant filter,
not a modelled analog ladder.

Implemented as a TPT state-variable filter; −12 dB is one resonant 2-pole
stage, −24 dB cascades a second, non-resonant 2-pole stage (voiced
topology, OQ-08). Resonance maps linearly to the SVF damping
`k = 2 − 2.04·(v/127)`: Q ≈ 0.5 at zero, oscillation onset at v ≈ 124,
slightly negative damping at 127 so self-oscillation grows until a
continuous soft-knee state limiter holds it — matching the manual's "may not
stop at all".
The onset point and curve are voiced (OQ-08). Cutoff 0–127 maps
exponentially over 20 Hz → 20.48 kHz (10 octaves, voiced, OQ-08); envelope
depth ±63 spans ±10 octaves linearly; cutoff velocity sensitivity ±63
spans ±4 octaves at the velocity extremes (voiced, OQ-08).

The cutoff sum is assembled in two parts, and the split is deliberate. The
*panel* side — the cutoff knob, key follow, the velocity offset and the LFO —
passes through a 2.5 ms one-pole slew (voiced; it models nothing the hardware
does, and exists only so a patch edit or an S&H LFO edge cannot put a
discontinuity into the filter coefficient). The *envelope* side does not: the
filter and amp envelopes read the same slider through the same mapping, so
smoothing one and not the other would make the reported "fast ADSR response
times ensure bags of punch" true of the amp and false of the filter. The
envelope's depth knob is slewed with the rest of the panel; its level is
applied directly. Both stages' coefficients are then walked sample by sample
across the control tick, so taking the envelope out of the slew did not put an
eight-sample staircase back in. A test fences this: from A = 0 the filter
envelope must open no slower than the amp envelope, and a fast S&H filter LFO
must still produce no sample-level discontinuity.

### Envelopes

Settled: PITCH ENV is attack/decay only, shared A/D with per-oscillator
signed depth; FILTER ENV is ADSR with signed depth; AMP ENV is ADSR;
all sliders 0–127. Reported: "fast ADSR response times ensure bags of
punch" (SoS). Voiced (OQ-09): linear-ramp attack, exponential decay/release
(time-to-−60 dB), times mapping exponentially A: 1 ms → 5 s,
D/R: 2 ms → 12 s, pitch-env A/D on the same law; retrigger in POLY starts
a stolen voice's envelope from its current level. Pitch-env depth ±63 →
±24 semitones linear (voiced).

### LFOs

Settled: two identical LFOs per tone; shapes TRI, SIN, SAW, SQR, TRP
(trapezoid), S&H (one change per cycle), RND; rate 0–127 or the 20-entry
tempo-sync table against the patch tempo; fade time; key trigger;
destination 1 ∈ {PITCH1, PW1, FILTER, AUDIO-FILTER}, destination 2 ∈
{PITCH2, PW2, AMP}, each with signed depth whose negative half inverts the
waveform; the modulation lever's vibrato is LFO2 routed by MODULATION
ASSIGN. Voiced (OQ-10): rate 0.03 → 30 Hz exponential; trapezoid as
rise-¼/high-¼/fall-¼/low-¼; RND as linearly interpolated random targets
per cycle (S&H stepped); fade time (v/127)² × 10 s; depth scalings — pitch
±1 octave with a squared taper, PW the full parameter span, filter
±5 octaves, amp up to ±100 % level. The LFOs are per-tone (shared by that
tone's voices), matching the hardware's "2 LFOs" per-tone architecture;
KEY TRIGGER restarts the cycle on a key press as documented.

### AMP, overdrive, pan

Settled: LEVEL 0–127; OVERDRIVE is an insertion effect in the AMP section,
"similar to vacuum tube amplifier distortion", DRIVE 0–127, with LEVEL
still acting as clean volume; hidden PAN L64–63R; LEVEL VELOCITY SENS
±63. The manual's illustration shows the wave being boosted into limiting.
Voiced (OQ-11): drive maps to 0…32 dB of pre-gain into a tanh clipper,
output-compensated (`pre^−0.4`) to keep loudness roughly constant; level
knob is a squared amplitude law; pan is equal-power.

**Where the shaper is evaluated is not the same question as what it
evaluates.** The modelled engine runs at one fixed rate (OQ-01); a plug-in
runs at the host's, so a shaper evaluated at the host rate folds a *different*
amount of alias energy depending on what the user's interface happens to be
set to — the character of the port, not of the instrument. Measured before
this was addressed: a full-DRIVE sine at note 93 folded inharmonic energy back
at −18.7 dB relative to its own harmonics at 44.1 kHz and −48.9 dB at
176.4 kHz, a 30 dB spread across host rates for one patch. The stage is
therefore oversampled by the power-of-two factor whose internal rate lands
closest to 176.4 kHz — 4× at 44.1/48 kHz, 2× at 88.2/96 kHz, none at
176.4/192 kHz — through two equiripple half-band polyphase stages (N = 33,
stopband −43.1 dB, and N = 13, stopband −33.3 dB), with the `tanh` evaluated
inside the loop under first-order antiderivative anti-aliasing (Parker,
Zavalishin & Bozkurt, DAFx-16). A power-of-two ladder cannot hit a fixed rate
exactly from an arbitrary host rate, so what it guarantees is a bound rather
than a number: across every rate a host can plausibly run at, 22.05 to
192 kHz, the shaper stays within **±0.54 octaves** of the target, against the
3.1 octaves those rates themselves span. At the four common rates it is inside
176.4–192 kHz exactly. The transfer curve is untouched — this is not an answer to OQ-11,
and a captured transfer would replace the curve without changing where it is
evaluated.

The chain has a fixed group delay (19 samples at 44.1 kHz, 16 at 88.2/96 kHz,
none above), so **every voice carries that delay whether its OVERDRIVE is on
or not**: an overdriven UPPER against a clean LOWER would otherwise sound the
same note 19 samples apart and comb around 1 kHz. A voice with the switch off
passes through a matched pure delay, bit-identical apart from the shift, and
the plug-in reports the delay as its latency.

### External input: EXT IN, CENTER CANCEL, AUDIO FILTER

Settled (OM pp. 49–53). The rear INPUT jacks are monitored through their own
signal path: an **INPUT VOL** knob ("if you turn the knob all the way to the
left, you will hear no sound from the connected device"), a **CENTER CANCEL**
switch that "removes sounds that are localized at the center of the sound
field (such as vocals)" and, the manual warns, takes centred bass with them,
and an **AUDIO FILTER** with a FILTER ON button, a TYPE button cycling
**LPF → HPF → BPF → NOTCH → LPF** — one more type than the voice filter has —
a −12/−24 dB SLOPE button, CUTOFF (printed CENTER FREQ for BPF and NOTCH) and
RESONANCE. The manual says three separate times that none of it is stored in
the patch, so it lives outside `Patch` in the replica exactly as it does on
the instrument. CUTOFF answers on CC#2 and RESONANCE on CC#4 (OM p. 72).

Selecting **EXT-IN** as an oscillator waveform "plays the sound from the audio
source connected to the rear panel INPUT jacks" through the voice, and the
manual settles two things about it that a block diagram would not: "the sound
you hear will be mono even if the audio source connected to the INPUT jack is
stereo", and "since the sound may distort if you press a larger number of
keys, we recommend that you turn on the Solo function" — each voice adds
another copy of the input.

The **order** of the two paths is settled by the manual's own recipe for
"producing sound from the external device only when you play the keyboard":
turn the audio filter on, select LPF, and turn CUTOFF fully left, at which
point "you won't hear any sound" until you play. That only works if the
EXT-IN oscillator taps the input **before** the audio filter, and if the
direct monitor is muted while an EXT-IN voice is sounding — which the manual
confirms from the other side: with a long AMP ENV release, "the sound that's
passing through the audio filter will not be heard when you take your hand off
the keyboard until the release time has elapsed."

Voiced (OQ-14): the INPUT VOL law (squared, matching the AMP LEVEL knob); the
audio filter's cutoff-to-Hz and resonance curves (the voice filter's, with the
resonance floored short of the oscillation threshold — the manual describes it
as a boost and, unlike the voice filter's, never warns that it may not stop);
NOTCH realized as the low-pass and high-pass sum; the mono reduction the
EXT-IN oscillator takes after CENTER CANCEL (the channel difference rather
than the sum, since the sum of a centre-cancelled pair is zero); the depth of
the settled AUDIO-FILTER LFO and modulation-lever destinations; and the 5 ms
fade with which the direct monitor hands the input over to a voice. The direct
monitor is not patch audio, so the patch level and part controllers do not
scale it; the panel VOLUME, which sits after the DAC on the hardware, does.

### Effects

Settled: modulation delay → reverb in series, shared TIME and switches,
per-tone DEPTH sends; the delay's feedback can invert phase; chorus is a
delay *template* (Chorus 1/2), not a separate block; the eight delay and
eight reverb template names; all auxiliary parameter tables (HF damp, high
cut, LF/HF damp gains, pre-delay 0–100 ms, size 1–8, density, diffusion).
The template *parameter values* are unpublished; the templates shipped here
are original voicings under the settled names (OQ-12). Voiced (OQ-12):
delay TIME 0–127 → 1…1300 ms exponential; modulation is a sine sweep of
the delay time (rate 0.02…8 Hz, depth up to ±8 ms); reverb TIME 0–127 →
RT60 0.15…10 s scaled by SIZE, realized as an 8-line feedback delay
network with per-line damping from the settled LF/HF damp parameters,
input diffusion from DIFFUSION, and the settled HIGH CUT on the wet
return.

### Arpeggiator

Settled (OM pp. 22–23, 66–67). The arpeggiator plays an **arpeggio style** —
"a series of data for basic arpeggio patterns and chord styles recorded in the
form of a grid consisting of a maximum of 32 steps × 16 pitches", each cell
being note-on with a velocity, a tie holding the preceding note, or a rest —
against the keys held down. The style "records the position of each key you
play relative to the lowest-pitched key you played, and the order in which you
play each key", and one style is saved per patch. Its parameters:

- **GRID** — 1/4, 1/8, 1/8L, 1/8H, 1/12, 1/16, 1/16L, 1/16H, 1/24, where L and
  H are light and heavy shuffle. The divisions are settled; the shuffle
  *amounts* are named, not measured (voiced, OQ-15), and a shuffled pair keeps
  its total length so the beat never drifts.
- **DURATION** — 30…120 % of the final grid section of a tie chain, or FUL,
  which "continues to sound until the point at which the next new sound is
  specified".
- **MOTIF** — twelve values, whose meanings the manual gives *by worked
  example*: for the style `1-2-3-2` against the keys C-D-E-F-G,
  `UP(-)` gives C-D-E-D → D-E-F-E → E-F-G-F, `UP(L)` gives C-D-E-D → C-E-F-E →
  C-F-G-F, and `UP&DOWN(L&H)` gives C-D-G-D → C-E-G-E → C-F-G-F → C-E-G-E.
  The replica's mapping reproduces all three exactly and a test holds it to
  them: a window `span` rows wide slides over the sorted keys once per pass,
  `(L)` pins the style's first row to the lowest key, `(L&H)` also pins its
  last row to the highest, and the window walks up, down, up-and-down or at
  random.
- **OCTAVE RANGE** −3…+3, which "shifts arpeggios one cycle at a time in
  octave units" (the cycle order is voiced, OQ-15).
- **ARPEGGIO ACCENT** 0–100: at 100 "the arpeggiated notes will have the
  velocities that are programmed by the arpeggio style", at 0 "all arpeggiated
  notes will be sounded at a fixed velocity". The blend between the two, and
  the flat value, are voiced (OQ-15).
- **ARPEGGIO VELOCITY** REAL or 1–127: what "how hard you played" means.
- **END STEP** 1–32, **HOLD**, and **SPLIT ARPEGGIO** (UPPER / LOWER / BOTH),
  which tone(s) it drives in SPLIT mode.
- The tempo is **PATCH TEMPO**, shared with the LFO sync. Two Roland
  documents disagree on its range: the parameter list gives PATCH TEMPO
  5–300 BPM, while the product page's specification block gives the
  arpeggiator "Tempo: 20–250 B.P.M." The address map is the parameter
  contract, so the replica keeps 5–300; a panel that refuses to leave
  20–250 would be a display restriction on the same stored value.

PHRASE is the one motif the manual describes without a worked example
("pressing just one key plays a phrase based on the pitch of that key; if you
press more than one key, the key you press last is used"), so how a style's
rows become intervals is voiced: the replica reads row *r* as *r*−1 semitones
above that key (OQ-15).

**The 32 factory arpeggio styles are Roland's data and none of them ships
here**, exactly as with the 64 factory patches. The styles supplied are
original patterns written against the same settled grid. The hardware's own
panel only *selects* a template — the manual says editing a style needs the
SH-201 Editor — so a selector is the faithful panel surface, and the patch
stores both the selector and the grid it names.

### Key assignment, solo/legato, portamento, pedals

Settled: POLY / SOLO+LEGATO / SOLO per tone; solo = last-note priority;
legato suppresses retrigger on overlapped notes; portamento per tone with
0–127 time, and with legato+portamento the glide applies only to legato
playing. Voiced (OQ-13): voice stealing takes the longest-released voice,
else the oldest sounding; portamento is constant-time, (v/127)² × 5 s.

Both documented pedals are implemented. HOLD (CC#64) holds everything
sounding for as long as it is down. SOSTENUTO (CC#66) latches the notes whose
keys were down at the moment it went down and holds only those — a key pressed
afterwards plays and releases normally, which is the whole point of the pedal.
The two are independent: a note caught by both is released only when both are
up, and a stolen voice loses its latch, since the latch belonged to the note
the pedal caught and not to the physical voice.

### Analog output stage (settled, service notes)

The replica's "circuit" component. Per channel after the DAC: 22 µF
coupling into 22 kΩ (0.33 Hz high-pass), a 2nd-order passive RC low-pass
(4.7 kΩ/270 pF and 8.2 kΩ/820 pF → poles at 125.4 kHz and 23.7 kHz), a
non-inverting M5218AFP stage of gain 2.5 (33k/22k, 10 pF giving a
~482 kHz pole), the analog master-volume pot, and the 2× line stage. The
replica implements the 0.33 Hz DC block and both RC poles at their
component values (audible only at high host rates; present for
completeness), normalizes the 2.5×/2× gain chain to unity digital
full-scale, and notes the factory anchors: 5.0 Vp-p at OUTPUT (440 Hz
test), −4.0 dB at 20 kHz through the full analog chain, residual noise
≤ −72 dB DIN-weighted. The codec's own digital filter (passband 0.454·fs)
is not separately modelled — the host's converters stand in for it.

## Scope of v1

Implemented: both tones with every tone parameter above, all nine
waveforms including EXT-IN and the external-input path around it (INPUT VOL,
CENTER CANCEL, the four-type AUDIO FILTER, and the monitor/voice changeover);
with no input bus connected an EXT-IN oscillator renders silence, as the
hardware does with nothing plugged in. MIX/SYNC/RING, the filter, all three envelopes, both LFOs
with tempo sync, overdrive, delay→reverb with per-tone sends and the
16 templates, SINGLE/DUAL/SPLIT with 10/5+5 voices, solo/legato,
portamento, pitch bend with per-tone range, the arpeggiator with the settled
grid/duration/motif/octave/accent/velocity/end-step/hold/split parameters, the
settled CC map including both documented pedals and the audio filter's
CC#2/CC#4, and the analog output stage. Deferred, documented: the step
recorder, D-Beam, SysEx DT1/RQ1 I/O, and the USB audio topology.

## Every voiced constant lives in one place

The contract's rule is that a voiced constant is a constant a measurement
should one day replace, so it has to be findable. All of them are in the
engine's `mapping` namespace, each tagged with its tier and the open question
that owns it — there are no bare numbers in the render code. The ones that
were still inline until this pass, and where they now sit:

| Constant | Owned by |
| --- | --- |
| `balanceLegGain` — the BALANCE and TONE BALANCE crossfade law | OQ-07 |
| `fbOscDelayRatio`, `fbOscLoopDamping`, `fbOscLoopTrim`, `fbOscOutputGain` | OQ-06 |
| `superSawStackNormalisation` — the seven-saw sum's trim | OQ-05 |
| `leverVibratoCents`, `leverPulseWidth`, `leverFilterOctaves`, `leverAmpDepth` — the modulation lever's reach into each settled destination | OQ-10 |
| `delayModulationRateHz`, `delayModulationDepthSeconds` | OQ-12 |
| `reverbLineSeconds`, `reverbDiffuserSeconds`, `reverbSizeScale`, `reverbDiffusionGain`, `reverbDensityGain`, `reverbInputInjection`, `reverbWetReturn` | OQ-12 |
| `filterSecondStageDamping`, `filterStateLimit` | OQ-08 |
| `voiceHeadroom`, `outputLimitKnee`, `outputLimitRange`, `partPanCentreGain`, `masterSlewSeconds`, `delayTimeSlewSeconds`, `controlSlewSeconds` | none — these are engineering choices about headroom, safety and zipper, not claims about the instrument, and no measurement of a real unit would settle them |

Moving them changed no audio: the committed demos re-render bit-identically.

## Open questions

Each is a standing research task; the measurement named would close it.

- **OQ-01 — engine sample rate.** Clock tree fixes engine fs = USB fs;
  44.1 vs 48 kHz undetermined. Close by: reading a real unit's USB
  descriptors, or spectral analysis of a dry hardware capture's alias
  lines. Also owns the true oscillator interpolation/aliasing behavior
  (owners report supersaw content dying above ~15 kHz).
- **OQ-02 — CC#88 collision.** Capture the panel's transmitted CCs (move
  UPPER filter-env D and LOWER OSC2 pitch-env depth) to settle the
  misprint read as CC#83.
- **OQ-03 — noise color; pulse-width endpoints.** Spectral capture of the
  NOISE wave; scope capture of PW at 0/64/127.
- **OQ-04 — supersaw HPF.** Two defensible formulations (1.0×f0 Q=0.707 vs
  2.5×f0 Q=√2). A dry hardware capture of one supersaw note, FFT below and
  around the fundamental, decides. Until then: A–Z listening-test
  candidate.
- **OQ-05 — supersaw fixed mix.** The SH-201's fixed center/side balance is
  unmeasured; m = 0.75 is voiced. Close by FFT of a hardware note at zero
  and full spread.
- **OQ-06 — FB-OSC mechanism constants.** Comb delay ratio, feedback law,
  in-loop nonlinearity. Close by capturing feedback-knob sweeps at fixed
  pitch and matching partial structure.
- **OQ-07 — balance law; low-shelf corner/gain.** Capture BALANCE at
  −63/−32/0/+32/+63 with dissimilar waves; capture LOW FREQ
  CUT/FLAT/BOOST on a saw and fit the shelf.
- **OQ-08 — filter calibration.** Cutoff-knob-to-Hz table, resonance-to-Q
  curve, self-oscillation onset value, −24 dB topology (resonance on one
  or both stages), envelope/velocity depth scalings. Close by measuring a
  real unit's swept responses at a grid of knob values.
- **OQ-09 — envelope time tables.** Measure attack/decay/release durations
  at slider values 0/32/64/96/127 from dry captures; segment curvature
  (linear vs exponential attack) from the amplitude trace.
- **OQ-10 — LFO rate table and depth scalings.** The panel LED blinks at
  the LFO rate: film the LED (or capture PWM audio) at rate 0/64/127; the
  trapezoid segment ratios and RND smoothing need a scope capture of
  filter-cutoff modulation.
- **OQ-11 — overdrive transfer.** Capture a sine through OVERDRIVE at
  drive 0/32/64/96/127 and fit the static curve.
- **OQ-12 — effect calibration.** Delay TIME-to-ms table (tap the repeats),
  the 16 template parameter sets (dump SysEx from a real unit after
  applying each template), reverb RT60 per TIME/SIZE.
- **OQ-13 — voice-steal policy.** Play 11 notes and observe which voice
  drops on hardware.
- **OQ-15 — arpeggiator calibration.** The shuffle amounts behind 1/8L,
  1/8H, 1/16L and 1/16H; the ACCENT blend and the flat velocity it collapses
  onto; the order the OCTAVE RANGE cycle visits its octaves; how a PHRASE
  style's rows become intervals. Close by recording the arpeggiator's MIDI
  output (the manual documents that it can be played over MIDI) at a grid of
  GRID, ACCENT and OCTAVE RANGE settings and reading the note times and
  velocities straight off it — the one open question in this project that a
  MIDI capture alone can close, with no audio analysis needed.
- **OQ-14 — external-input calibration.** INPUT VOL taper; the audio
  filter's cutoff-to-Hz table and resonance curve, and whether it
  self-oscillates at all; whether CENTER CANCEL's output is the anti-phase
  side pair or a mono difference; what an EXT-IN oscillator hears with CENTER
  CANCEL engaged; the AUDIO-FILTER modulation depths. Close by capturing the
  INPUT-to-OUTPUT response at a grid of audio-filter settings, and by feeding
  a known stereo signal with CENTER CANCEL on and off.

## What a SysEx dump of the factory bank would add

The 64 factory patches' parameter values exist in every real unit and are
retrievable over documented RQ1 requests (`20 00 00 00`…, 0x1542 bytes per
patch). A dump would provide: regression test vectors for the parameter
codec, empirical resolution of the LFO tempo-sync bit order ("ON, OFF" as
printed vs the standard "OFF, ON"), the bank-select LSB discrepancy
(manual p. 84 says PRESET LSB 64; MIDI implementation p. 1 says LSB 0 for
preset and 20H for user), and patch-matched settings for every official
demo MP3. No dump ships in this repository — the data is Roland's — but a
loader for user-supplied dumps is a natural follow-up.

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

- **Patch Common** (0x21 bytes), in the map's own order: name ×12 (`00`–`0B`),
  patch level 0–127 (`0C`), tone balance −63…+63 (`0D`), tempo 5–300 as three
  nibbles (`#0E/0F/10`), keyboard mode SINGLE/DUAL/SPLIT (`11`), keyboard part
  (`12`), split point A0–C8 (`13`), SPLIT ARPEGGIO UPPER/LOWER/BOTH (`14`),
  the four **controller destinations** — MODULATION, D BEAM, PITCH BEND and
  EXPRESSION, each UPPER / LOWER / BOTH (`15`–`18`) — ACTIVE EXPRESSION switch
  (`19`), arpeggio switch (`1A`) and hold (`1B`), delay switch (`1C`), reverb
  switch (`1D`), modulation assign (OSC1&OSC2 / OSC1 / OSC2 / PW1 / PW2 /
  FILTER / AMP / AUDIO-FILTER) (`1E`), D-Beam assign (37 destinations, `1F`),
  D-Beam polarity (`20`).

**CONTROLLER DESTINATION (settled, OM p. 65).** Each physical controller names
the tone or tones it reaches: "Selects the tone(s) to be modulated by the
modulation lever. If this is 'BOTH,' modulation will be applied to both the
UPPER tone and LOWER tone", and the same sentence for the pitch bend lever and
the expression pedal. The replica implements the three whose controller it
has: a voice the bend lever does not reach does not bend, a voice the
modulation lever does not reach takes none of the lever's four settled
destinations, and EXPRESSION scales only the tone(s) it names — which is why
it now sits in the per-tone gain rather than in the master chain, where with
BOTH the product is identical. The AUDIO-FILTER lever destination is one
filter fed by two tones' LFO2s, so a single destination picks that tone's and
BOTH keeps the keyboard part's (voiced, OQ-14). D BEAM DESTINATION and ACTIVE
EXPRESSION are stored and inert — see the D Beam section.
- **Patch Tone** ×2 (0x40 bytes each): per oscillator — waveform
  (0–8: SAW, SQU, PW-SQU, TRI, SINE, NOISE, FB-OSC, SUPER-SAW, EXT-IN),
  pitch-wide switch (raw 28–100 for the coarse tune either way — the switch
  "expands the range of the PITCH *knob* by a multiple of three", OM p. 29, so
  it gates the panel control's travel and not the stored pitch; on a numeric
  parameter there is no travel to gate, and the switch is stored patch data
  that does not change what sounds), coarse −36…+36 st, fine −50…+50 cents, pulse width
  0–127, pitch-env depth −63…+63; pitch env A/D 0–127; mix/mod type
  (MIX/SYNC/RING), balance −63…+63, low freq (FLAT/BOOST/CUT); filter type
  (0–3: BYPASS, LPF, HPF, BPF), slope (−12/−24 dB), cutoff 0–127, key
  follow −200…+200 (raw 44–84, so 41 positions in steps of 10 — the engine
  and the panel both quantise to them), cutoff velocity sens −63…+63,
  resonance 0–127; filter env A/D/S/R + depth; overdrive switch + drive
  0–127; amp level, level velocity sens, pan L64–63R; amp env A/D/S/R;
  delay depth, reverb depth; LFO1/LFO2 shape (0–6: TRI, SIN, SAW, SQR, TRP,
  S&H, RND), rate 0–127, tempo-sync switch + note (20 values: 16, 12, 8, 4,
  2, 1, 3/4, 2/3, 1/2, 3/8, 1/3, 1/4, 3/16, 1/6, 1/8, 3/32, 1/12, 1/16,
  1/24, 1/32 whole notes), fade time, key trigger, destination 1
  (PITCH1/PW1/FILTER/AUDIO-FILTER) + depth, destination 2 (PITCH2/PW2/AMP)
  + depth; bend range 0–24 st; octave shift −3…+3; portamento switch +
  time; mono/solo select (POLY, SOLO+LEGATO, SOLO).
- **Patch Delay** (5 bytes): time 0–127; feedback −98…+98 % (raw 0–98, so the
  display moves in steps of 2 % and raw 49 is 0 %; negative
  inverts phase); HF damp 200–8000 Hz in 17 steps or BYPASS; modulation
  rate 0–127; modulation depth 0–127.
- **Patch Reverb** (10 bytes): time 0–127; pre-delay 0–100 ms; size 1–8;
  high cut 160–12500 Hz in 20 steps or BYPASS; density 0–127; diffusion
  0–127; LF damp 50–4000 Hz in 20 steps, gain −36…0 dB; HF damp
  4000–12500 Hz in 6 steps, gain −36…0 dB. Both damp gains are raw 0–36
  counting up from −36 dB, not one of the ±64-biased fields (OQ-17).
- **Patch Arpeggio Common** (8 bytes): grid (0–8), duration (0–9), motif
  (0–11), octave range −3…+3 (raw 61–67), accent rate 0–100, velocity
  (0 = REAL, else 1–127), END STEP 1–32 as two nibbles (`#06/07`).
- **Patch Arpeggio Pattern** ×16 (0x42 bytes each): one block per grid row,
  Note 1 at offset `00 06 00` through Note 16 at `00 15 00`. Each holds that
  row's Original Note followed by its Step1…Step32 data, every field a
  two-byte nibble with the range 0–128 — 129 values, exactly the number of
  states a cell has (a rest, 127 velocities, a tie).
- **System Common**: master tune 415.30–466.20 Hz (raw 24–2024, 0.1-cent
  steps around A440), master key shift −24…+24 (raw 40–88), master level,
  transpose −5…+6 (raw 59–70), octave shift −3…+3 (raw 61–67), clock source
  (PATCH/SYSTEM/MIDI/USB), system tempo 5–300, MIDI routing switches, pedal
  polarity and assign, D-Beam sensitivity 1–8, recorder sync/metronome
  settings. The first five are implemented and published as plug-in
  parameters that a program change does not touch, exactly as the
  external-input block is; the rest belong to features that are still
  deferred or to a MIDI topology a plug-in does not have.

The whole map is implemented; the arpeggiator's own reading of it is in the
arpeggiator section below.

### CC map (settled, OM p. 72)

Part: Bank MSB 0, Modulation 1, Level 7, Pan 10, Expression 11, Bank LSB 32,
Hold 64, Sostenuto 66, D-Beam pitch 69 (accepted and ignored — the replica
has no D Beam), Portamento control 84.

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

### Channel mode and universal messages (settled, MIDI Implementation pp. 1–2)

The MIDI Implementation's own Receive Data section is the authority on the
messages that are not parameter edits, and it separates two that the replica
had treated alike:

- **All Sounds Off (CC#120)** — "all notes currently sounding on the
  corresponding channel will be turned off." A panic.
- **All Notes Off (CC#123)**, and **OMNI OFF (124)** / **OMNI ON (125)**,
  which do "the same processing" — "all notes on the corresponding channel
  will be turned off. **However, if Hold 1 or Sostenuto is ON, the sound will
  be continued until these are turned off.**" That makes it every key coming
  up at once, not a panic; the replica used to release every voice and drop
  the sostenuto latch with them, so a pedal that was still down had its notes
  taken out from under it.
- **Reset All Controllers (CC#121)** — pitch bend to centre, modulation 0,
  expression **127**, hold and sostenuto off. Implemented as printed.
- **Universal Realtime device control**, each naming the SYSTEM COMMON
  parameter it changes: **Master Volume** (`04 01`, "the lower byte will be
  handled as 00H") → MASTER LEVEL; **Master Fine Tuning** (`04 03`, `00 00H –
  40 00H – 7F 7FH` = −100 – 0 – +99.9 cents) → MASTER TUNE; **Master Coarse
  Tuning** (`04 04`, LSB ignored, MSB `28H – 40H – 58H` = −24 – 0 – +24
  semitones) → MASTER KEY SHIFT. All three parameters are published here, so
  all three messages are received onto them.

Not implemented, with the reason: the **Identity Request** reply, because the
plug-in transmits no SysEx at all; and the **Active Sensing** timeout — "if
the interval between messages exceeds 420 ms, the same processing will be
carried out as when All Sounds Off, All Notes Off and Reset All Controllers
are received" — which is a cable-failure watchdog. A plug-in has no cable, and
a host that sends one `FE` and then goes quiet during a pause would have the
sound cut out from under it.

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
band-limited at the host rate — the triangle's polyBLAMP coefficient is half
its per-sample slope change, because the residual pair here is the canonical
one whose BLEP already carries a step of two and whose BLAMP is that BLEP's
antiderivative — the real unit's oscillators alias audibly
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

**INTERVAL (settled, OM p. 30).** The two buttons above OSC 2 are defined
against OSC 1, not against zero: "-OCT (minus octave) button — This button
lowers the OSC 2 pitch one octave below that of OSC 1"; "5th button — ... the
OSC 2 pitch will be seven semitones (a perfect fifth) higher than OSC 1"; and
"if you press the -OCT button and the 5th button simultaneously, the OSC 2
pitch will be the same as the OSC 1 pitch". The replica's panel has one button
each, so the second press of either stands in for the hardware's simultaneous
press and lands OSC 2 on OSC 1's pitch. Both buttons light while the interval
they name is in force, as the hardware's indicators do.

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
topology, OQ-08). Each of the three responses is the raw integrator tap, so
all three gain with RESONANCE and all three reach the oscillation the manual
warns about. The band-pass was scaled by the damping until 2026-08-22 — the
usual unity-peak normalisation, but the damping here is the quantity
RESONANCE drives to zero, so the band-pass lost 21 dB across the top of the
knob and came back inverted past the oscillation threshold. Resonance maps to the SVF damping through a square-root
taper, `k = 2 − 2.04·√(v/127)`: Q ≈ 0.5 at zero, slightly negative damping at
127 so self-oscillation grows until a continuous soft-knee state limiter holds
it — matching the manual's "may not stop at all".

Both endpoints are settled; the shape between them is not. A linear taper put
the entire audible range of the control in the top fifth of its travel — the
filter peaked by 1.38 dB at the exact centre of the knob — and the square-root
taper that replaced it was **chosen by ear**, in the 2026-08-22 listening test
recorded in the [best-in-class plan](best-in-class-plan.md), against a linear
and a quadratic candidate. The centre of the knob now peaks by 5.4 dB. That is
a choice, not a measurement: **OQ-08 is still open**, and the swept response
from a real unit it names is still what would close it. Cutoff 0–127 maps
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
192 kHz, the shaper stays within **±1.0 octave** of the target, against the
3.1 octaves those rates themselves span. At the four common rates it is inside
176.4–192 kHz exactly, and only 22.05 kHz sits a full octave low — the ladder
stops at 4× because the shaper does, and a third half-band stage would cost a
fractional 1.5 host samples of group delay to buy two unusual rates. The transfer curve is untouched — this is not an answer to OQ-11,
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
the settled AUDIO-FILTER LFO and modulation-lever destinations; the 5 ms
fade with which the direct monitor hands the input over to a voice; and the
5 ms over which every switch on this path is *crossed* rather than thrown.
That last one matters because CENTER CANCEL and the filter's ON, SLOPE and
TYPE each choose between signals whose instantaneous samples differ, so a
change steps the output on live audio however warm the states on the unused
side are kept — long enough that the step is inaudible, short enough that the
control still reads as a switch. The hardware's own changeover is not
documented and is what a capture would settle. The direct
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
  specified". On a shuffled grid the sections of a pair are different lengths,
  so *which* section the percentage is measured against is not a detail: it is
  the final one, the one the chain ends on, not the one it started from.
  A percentage at or below 100 therefore never reaches into the next grid,
  and only 120 % overlaps.
- **MOTIF** — twelve values, whose meanings the manual gives *by worked
  example*: for the style `1-2-3-2` against the keys C-D-E-F-G,
  `UP(-)` gives C-D-E-D → D-E-F-E → E-F-G-F, `UP(L)` gives C-D-E-D → C-E-F-E →
  C-F-G-F, and `UP&DOWN(L&H)` gives C-D-G-D → C-E-G-E → C-F-G-F → C-E-G-E.
  The replica's mapping reproduces all three exactly and a test holds it to
  them: a window `span` rows wide slides over the sorted keys once per pass,
  `(L)` pins the style's first row to the lowest key, `(L&H)` also pins its
  last row to the highest, and the window walks up, down, up-and-down or at
  random.
- **A chord narrower than the style** is settled too: "When the number of keys
  played is less than the number of notes in the arpeggio style, the
  highest-pitched of the pressed keys is played by default" (OM p. 66). The
  sentence carries no direction qualifier, so it holds for the DOWN motifs as
  well as the UP ones, and a test holds every motif to it.
- **OCTAVE RANGE** −3…+3, which "shifts arpeggios one cycle at a time in
  octave units" (the cycle order is voiced, OQ-15).
- **ARPEGGIO ACCENT** 0–100: at 100 "the arpeggiated notes will have the
  velocities that are programmed by the arpeggio style", at 0 "all arpeggiated
  notes will be sounded at a fixed velocity". The blend between the two, and
  the flat value, are voiced (OQ-15).
- **ARPEGGIO VELOCITY** REAL or 1–127: what "how hard you played" means.
- **END STEP** 1–32, its own control and independent of the selected template.
  The replica adds a zero below the documented range (voiced): it means "as
  long as the template is", so a patch that never touches END STEP keeps
  whatever length the style defines, and the panel reads `STYLE` rather than a
  step count. **HOLD**, and **SPLIT ARPEGGIO** (UPPER / LOWER / BOTH),
  which tone(s) it drives in SPLIT mode.
- The tempo is **PATCH TEMPO**, shared with the LFO sync. Two Roland
  documents disagree on its range: the parameter list gives PATCH TEMPO
  5–300 BPM, while the product page's specification block gives the
  arpeggiator "Tempo: 20–250 B.P.M." The address map is the parameter
  contract, so the replica keeps 5–300; a panel that refuses to leave
  20–250 would be a display restriction on the same stored value. The map
  also settles how the value travels: three nibbles at `#00 0E/0F/10`, which
  is what makes the top of the range reachable at all (OQ-17).

PHRASE is the one motif the manual describes without a worked example
("pressing just one key plays a phrase based on the pitch of that key; if you
press more than one key, the key you press last is used"), so how a style's
rows become intervals is voiced: the replica reads row *r* as *r*−1 semitones
above that key (OQ-15).

The address map narrows that question without closing it. Each of the sixteen
Patch Arpeggio Pattern blocks opens with an **Original Note (0–128)** of its
own (offset `00 00`), so a style's rows carry recorded pitches, not just
positions — which is the shape a transposed phrase would need. What reads the
field is not written down anywhere this project has, so the engine stores it
(a hardware dump survives a load and a re-save intact) and PHRASE keeps the
voiced reading above. None of the styles shipped here sets it.

**The 32 factory arpeggio styles are Roland's data and none of them ships
here**, exactly as with the 64 factory patches. The styles supplied are
original patterns written against the same settled grid. The hardware's own
panel only *selects* a template — the manual says editing a style needs the
SH-201 Editor — so a selector is the faithful panel surface, and the patch
stores both the selector and the grid it names.

### D Beam (not implemented; four bytes stored)

**The controller is not modelled.** An infrared distance sensor is a control
surface: it reads how far a hand is above the panel, and a plug-in has no
panel and no hand above it. It was implemented once, as a group of automatable
parameters, and removed at the user's direction — a scope decision, not a
measurement, and recorded as such in the change log.

What the beam owns in the patch stays, because it is patch data and a SysEx
round trip has to be lossless. Four Patch Common bytes (00 16 D BEAM
DESTINATION, 00 19 ACTIVE EXPRESSION, 00 1F D BEAM ASSIGN with its settled
37-entry list, 00 20 D BEAM POLARITY) are stored, saved, program-changed with
the patch and round-tripped through the codec. They are published as
non-automatable parameters and read by nothing that sounds — the same
`[settled range, no effect]` position PITCH WIDE holds.

**ACTIVE EXPRESSION goes inert with the beam, and must not be re-pointed.**
The manual defines it only as a modifier of the beam's EXPRESS button ("OFF:
The D Beam controller will change the volume. ON: The D Beam controller will
control Active Expression, which combines two tones", OM p. 65). No other
controller on the instrument drives it. Attaching it to the expression pedal
or to CC#11 would be inventing a mechanism no Roland document describes.

**CC#69** — "Part Pitch (D Beam Pitch Mode)" in the settled control-change
list — is accepted and ignored, since the thing it moved is gone.

**D BEAM SENS** (System Common 00 1D) is gone outright. It compensated the
sensor for "strong direct sunlight or strong artificial illumination" (OM
p. 21), and unlike the four Patch Common bytes it carried no round-trip
obligation: this replica implements no System Common SysEx block at all, so
nothing ever encoded or decoded it. The earlier justification for storing it
— "a SysEx round trip has to be lossless" — was true of PITCH WIDE and of the
four patch bytes, and was never true of SENS.

**OQ-16 is withdrawn**, not answered: with no beam rendered there is nothing
left for a capture of the beam's MIDI output at a grid of hand heights to
calibrate. Its entry stays in the open-question list, marked withdrawn, so the
record of what was once open survives.

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

**Implemented.** Both tones with every tone parameter above; all nine
waveforms including EXT-IN and the external-input path around it (INPUT VOL,
CENTER CANCEL, the four-type AUDIO FILTER, and the monitor/voice changeover) —
with no input bus connected an EXT-IN oscillator renders silence, as the
hardware does with nothing plugged in. MIX/SYNC/RING, the filter, all three
envelopes, both LFOs with tempo sync, overdrive, delay→reverb with per-tone
sends, SINGLE/DUAL/SPLIT with 10/5+5 voices, solo/legato, portamento, pitch
bend with per-tone range, the arpeggiator with the settled
grid/duration/motif/octave/accent/velocity/end-step/hold/split parameters, the
settled CC map including both documented pedals and the audio filter's
CC#2/CC#4, the three remaining controller destinations, the System Common
tune, key shift, octave and transpose, and the analog output stage.

**SysEx DT1 receive and encode** are implemented (`Source/DSP/SeptumSysEx.*`):
a dump sent to the plug-in loads the patch, and the current patch can be
encoded through the processor's API. Three caveats belong here rather than in
a footnote. RQ1 requests are rejected and the plug-in transmits no SysEx of
its own. The block codec round-trips every documented field, the arpeggio grid
and the full 5–300 tempo range included; the trip *through the plug-in* does
not carry the grid or the patch name, because the plug-in's authoritative
state is its parameter list and neither of those has a parameter.

**Not implemented, with the reason.**

- **The D Beam** — see its own section: a sensor a plug-in cannot have.
  Removed at the user's direction; its four Patch Common bytes stay stored.
- **The step recorder**, and **tap tempo**.
- **CLOCK SOURCE**, and with it external MIDI clock and any host-transport
  sync: the arpeggiator and the LFOs' tempo sync run from PATCH TEMPO alone.
- **The 16 effect templates as a runtime selector.** The settled names exist
  as voiced parameter sets used to build the shipped programs; the panel
  exposes the underlying DELAY and REVERB parameters directly, where the
  hardware's panel offers the templates.
- **The USB audio topology**, and the codec's own digital reconstruction
  filter — the host's converters stand in for it.

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
| `arpeggioShuffleLight`, `arpeggioShuffleHeavy` — how far a shuffled pair's boundary moves. The manual names Light and Heavy and does not measure them | OQ-15 |
| `overdriveCompensationExponent` — the output compensation that follows the clipper | OQ-11 |
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
  NOISE wave; scope capture of PW at 0/64/127. The engine's position until
  that capture is that NOISE is white, and it is measured white at every host
  rate — a 23-bit Galois LFSR whose successive *states* were read as the
  sample shipped briefly and was neither (10.9 dB of tilt at 44.1 kHz, 1.1 dB
  at 192 kHz), under a comment naming a Roland polynomial no source in this
  document settles. PW-SQU's polyBLEP residuals stop separating when the
  narrow side of the pulse is shorter than a sample, which is reachable at the
  top of the PW range high on the keyboard; the bound is recorded here rather
  than worked around, because both ways of working around it change what the
  top of the knob sounds like.
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
  filter-cutoff modulation. The two LFOs' S&H and RANDOM sequences are seeded
  apart — one shared seed made all four of a patch's LFOs draw the same
  numbers, which is not a calibration question but was audible as two
  "independent" modulators moving in lockstep.
- **OQ-11 — overdrive transfer.** Capture a sine through OVERDRIVE at
  drive 0/32/64/96/127 and fit the static curve. The curve is a symmetric
  `tanh` under first-order antiderivative anti-aliasing; a quadratic "tube"
  pre-conditioning stage with an invented coefficient shipped briefly ahead of
  it, which also broke the ADAA (the antiderivative belongs to `tanh`, not to
  the composite). Whether the instrument's shaper is asymmetric at all is part
  of what the capture would settle. Also open: whether the AMP envelope sits
  ahead of the shaper or behind it — both readings are derivable from "the
  wave being boosted into limiting", they sound different, and it is an A-Z
  question rather than a measurement one.
- **OQ-12 — effect calibration.** Delay TIME-to-ms table (tap the repeats),
  the 16 template parameter sets (dump SysEx from a real unit after
  applying each template), reverb RT60 per TIME/SIZE.
- **OQ-13 — voice-steal policy.** Play 11 notes and observe which voice
  drops on hardware. The implemented policy is the one this document states —
  the longest-*released* voice, else the oldest sounding — which needs a
  release timestamp per voice; ordering the released candidates by their
  trigger age instead took the freshest tail rather than the stalest.
- **OQ-15 — arpeggiator calibration.** The shuffle amounts behind 1/8L,
  1/8H, 1/16L and 1/16H; the ACCENT blend, and how it interacts with ARPEGGIO
  VELOCITY — the manual's two endpoint sentences (at 100 the style's
  programmed velocities, at 0 "a fixed velocity") are reproduced only when
  ARPEGGIO VELOCITY is a fixed value, and with REAL neither endpoint is
  literal, so whether the hardware's ACCENT overrides REAL is part of what the
  capture settles; the order the OCTAVE RANGE cycle visits its octaves; how a PHRASE
  style's rows become intervals. Close by recording the arpeggiator's MIDI
  output (the manual documents that it can be played over MIDI) at a grid of
  GRID, ACCENT and OCTAVE RANGE settings and reading the note times and
  velocities straight off it — the one open question in this project that a
  MIDI capture alone can close, with no audio analysis needed.
- **OQ-17 — the SysEx layout this project has not corroborated. ANSWERED
  2026-08-23** by reading the MIDI Implementation's Parameter Address Map
  directly (v1.00, 2006-03-01, pp. 4–5) rather than working from this
  document's quotations of it. Both `[unverified]` bytes were wrong, and so
  were four more the question had not suspected. Patch Common as printed:

  | Offset | Parameter | What the codec had |
  | --- | --- | --- |
  | `#00 0E/0F/10` | Patch Tempo (5–300), **three** nibbles | a two-byte 7-bit split (and before that, two nibbles, which could not reach 300) |
  | `00 14` | Split Arpeggio (0–2 UPPER/LOWER/BOTH) | a DELAY+REVERB bitmask |
  | `00 1A` | Arpeggio Switch | nothing (the switch sat at `00 1C`) |
  | `00 1B` | Arpeggio Hold | nothing (the switch sat at `00 1D`) |
  | `00 1C` | Delay Switch | Arpeggio Switch |
  | `00 1D` | Reverb Switch | Arpeggio Hold |

  Every other Patch Common offset, all 64 Patch Tone offsets, Patch Delay
  (`00 00 00 05`) and Patch Reverb (`00 00 00 0A`) with all four of their
  damping and frequency tables were already right. Three further divergences
  the map settled at the same time:

  - The block base addresses are absolute. Temporary Patch is at
    `10 00 00 00`, not `00 00 00 00`, and User Patch 001–**032** at
    `20 00 00 00`…`20 1F 00 00` — 32 slots, one step of `00 01 00 00` apart.
    The bank writer had been packing patches at `20 00 00 00 | bank<<16 |
    (slot*8)<<8`, which is neither.
  - The arpeggio grid is not one block. Patch Arpeggio Common is
    `00 00 00 08` — grid, duration, motif, octave range, accent rate,
    velocity, and End Step as two nibbles at `#00 06/07` — and the 32 × 16
    grid lives in sixteen **Patch Arpeggio Pattern (Note 1…16)** blocks at
    `00 06 00`…`00 15 00`, `00 00 00 42` each: an Original Note followed by
    Step1…Step32, every field a two-byte nibble with the range 0–128. A patch
    is therefore 22 DT1 blocks, not six, and ends at offset `00 15 42` —
    which is exactly the size the document's own worked RQ1 example requests.
    Both of the document's finished example messages are now test vectors.
  - Reverb LF/HF Damp Gain (`00 07`, `00 09`) is a plain 0–36 counting up
    from −36 dB, not one of the map's ±64-biased fields. Encoding it as one
    put 0 dB at byte 64 and −36 dB at byte 28, neither inside the documented
    range.

  A dump from a real unit is still the thing that would *prove* the codec
  round-trips — see below — but the layout is no longer this project's guess
  anywhere.
- **OQ-18 — the LFO tempo-sync bit order.** The address map lists Patch Tone
  `00 29` and `00 33`, LFO1 and LFO2 Tempo Sync Switch, as `(0 — 1) / ON,
  OFF`. Every other switch in the map — 26 of them — reads `OFF, ON`. The
  reversal is printed twice, once per LFO, so it is not a slip in one line,
  and the codec now writes it as printed: 0 is ON. The alternative is that
  Roland's own document is wrong in the same way twice. Close by dumping one
  patch from a real unit with LFO1 TEMPO SYNC on and reading byte `00 29`;
  nothing else in this project turns on the answer, because both sides of the
  codec agree with each other whichever way it goes — only a dump written by
  the hardware can tell them apart.
- **OQ-16 — D Beam calibration. WITHDRAWN 2026-08-23**, with the controller
  it belonged to: an infrared distance sensor has no meaning in a plug-in, so
  there is nothing left for the capture below to calibrate. Kept as a record
  of what was open. Whether the three mode buttons are
  exclusive; PITCH mode's interval and direction; the shape of the ASSIGN
  travel between the patch value and the end of the range; the point at which
  ACTIVE EXPRESSION starts adding LOWER; and whether the shared destinations
  follow D BEAM DESTINATION. Close by recording the beam's MIDI output at a
  grid of hand heights with each mode lit, which settles the first four
  directly, and by capturing the audio filter's response with the destination
  on one tone.
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
patch — the size the MIDI Implementation's own worked example asks for). A
dump would provide: regression test vectors for the parameter codec,
empirical resolution of the LFO tempo-sync bit order (OQ-18: "ON, OFF" as
printed, which is what the codec now writes, vs the standard "OFF, ON"),
the bank-select LSB discrepancy
(manual p. 84 says PRESET LSB 64; MIDI implementation p. 1 says LSB 0 for
preset and 20H for user), and patch-matched settings for every official
demo MP3. No dump ships in this repository — the data is Roland's — but a
loader for user-supplied dumps is a natural follow-up.

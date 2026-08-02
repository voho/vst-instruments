# YouKnow106 circuit-modelling research and implementation contract

YouKnow106 1.0 is a white-box circuit model of a 1984 six-voice
digitally-controlled-oscillator polysynth — the Roland Juno-106 — with named
reference material, not a black-box claim that one plug-in is indistinguishable
from that instrument. This document separates the parts of the circuit the
engine actually implements from the parts that remain voiced YouKnow106
decisions, and records where the sources disagree.

YouKnow106 is an independent original implementation. It is not affiliated with,
endorsed by, or licensed by Roland Corporation; it contains no Roland firmware,
no ROM data, no samples, and no captured audio. Its panel reproduces the
reference instrument's *functional* layout — which controls exist, how they are
grouped, and which are sliders rather than switches — because that layout is
what the circuit's controls are. Its livery does not: the palette, typography and
name are YouKnow106's own.

## What "realistic" is being claimed

These choices make YouKnow106 circuit-explicit and measurable. They do not by
themselves establish that it is indistinguishable from the hardware. Such a
claim would require calibrated captures of an identified, freshly-calibrated
unit and level-matched blind listening, neither of which was available here.

Every constant below is one of:

- **anchored** — read from the instrument's own service documentation or a
  component datasheet, and asserted by `Tests/YouKnow106CircuitTests.cpp`;
- **derived** — computed from an anchored value by a stated equation;
- **voiced** — chosen inside a range the sources bound but do not fix.

## Claims boundary

| Block | Reference | What YouKnow106 implements | Precise claim |
| --- | --- | --- | --- |
| Note timer | Service notes: an 8 MHz master clock, a range divider producing 1/2/4 MHz, and 8253-class programmable interval timers holding an integer count | One reference clock divided by an integer count computed against the 8' clock, with the RANGE switch changing the clock rather than the count, so the switch transposes by whole octaves and the tuning error is identical in all three ranges | Exact integer-division pitch with its real quantisation (±0.19 cents at A4, ~0.9 cents near the top); **anchored** — A4 at 8' programmes count 4545 and sounds 440.044 Hz, which the suite asserts. Not a fractional-dither or free-running-oscillator model |
| Counter width | 16-bit counters | Counts are clamped to 65535, so the 16' range floors at 15.26 Hz and asking for a lower pitch stops transposing | **Derived**; the suite asserts the floor |
| Control scan | One 12-bit converter multiplexed across 36 control points, repeating every 4.2 ms | Pitch, envelope, cutoff and level are rewritten per voice on a 238 Hz scan and then slewed by the hold capacitor's 522 µs time constant | The documented staircase-then-slew control path, which is why slow bends and vibrato audibly step and why the shortest usable attack is one scan pass rather than the published 1.5 ms; **anchored** on both figures |
| Ramp generator | Service notes describe an integrator whose capacitor is charged from the control voltage through a range-selected resistor, so the ramp amplitude stays constant across octaves | A 12 Vpp rising ramp with a small fixed bow and a finite-slope reset of 2.2 µs | **Voiced** bow. The service notes' integrator charges at constant current and is therefore straight; one reverse-engineering account of the same oscillator family describes a plain resistive charge that bows. The model keeps a small bow rather than choosing a side. Its in-band effect is a fraction of a decibel |
| Ramp bandlimiting | Integrated-B-spline residual tables built by numerically integrating a Blackman-windowed sinc at 64x, as in the LUT-BLEP literature | Both corners of the reset are slope discontinuities, not a jump, so each is repaired with the *slope* residual; the comparator and divider edges get the *step* residual | Alias floor below −55 dB across the keyboard at 44.1 and 48 kHz, asserted by the engine suite; measured −75 to −89 dB at ordinary settings. A closed-form polynomial fit was tried first and was *worse than no correction at all* — the tables are integrated, not fitted |
| Pulse and PWM | Comparator against a control voltage: 50% duty at +6 V, 95% at +0.6 V, and −0.8 V pins the output high | Duty derived from the threshold against the 12 Vpp ramp; the two-position source switch is LFO or MANUAL | **Anchored** on both duty figures. The pulse can reach neither 0% nor 100%, which the suite asserts |
| Sub oscillator | A divide-by-two flip-flop off the note clock | An exact square one octave below the selected footage, unaffected by pulse width, with a level slider only | **Anchored**. There is no sub-octave selector, because the instrument has none |
| Noise | One generator, level-controlled, mixed into every voice | A single shared source added to each voice before the high-pass | **Derived** consequence: noise sums coherently as more keys are held rather than staying at a fixed level |
| High-pass | A four-position slide switch ahead of the filter; single pole | Position 0 is a +3 dB low shelf at 70 Hz, position 1 passes the low band untouched, positions 2 and 3 are single-pole high-passes at 240 Hz and 720 Hz | Two independent accounts agree only two of the four positions filter and that the top is ~720 Hz, but give 225 Hz and 240 Hz for the middle one and +3 dB/70 Hz against +10 dB/150 Hz for the boost. Middle corner and shelf depth are therefore **voiced within the reported spread** |
| Filter core | The potted voice module contains a quad-OTA four-pole chip with an on-chip antilog converter; per stage 68 kΩ input resistor, 560 Ω, 240 pF integrator | Four transconductor stages solved together implicitly, `C dVn/dt = Ig tanh((V(n-1) − Vn)/H)` with `H = 2·Vt/attenuation = 6.37 V`, trapezoidally integrated and closed with a damped Newton step whose Jacobian is bidiagonal plus one corner term | **Anchored** topology and component values. The suite checks the model against a fourth-order Runge-Kutta solve of the same ODE at 16x, and both against the closed-form `1/(4 − k)`, to 0.6 dB |
| Filter drive level | The 68 k/560 Ω pair attenuates each stage's differential input by 122x | That attenuator, not a voiced "drive" control, is what puts the differential pair's linear span at ±6.4 V — right at the peak of a full-level ramp | **Derived**. It is the structural reason this filter compresses gently rather than either staying clean or clipping hard |
| Cutoff control law | Firmware-derived converter table: 5.53 Hz at code 0, 1143 counts per octave; service check of 248 Hz self-oscillation at code 6272 while holding C4, and 992 Hz at C6 | `f = 5.53 · 2^(counts/1143)`, with envelope, modulator, bender and key follow all summed in counts *before* the antilog stage | **Anchored**; the suite asserts both service anchors. Summing in the count domain is why every modulation source on this instrument is exponential in hertz |
| Resonance | Closed externally through an OTA from the four-pole output back to the input; oscillation at ~90% of the panel travel; amplitude limited by transconductor soft-clipping, not a diode clamp | `k = 4·position/0.9`, so `k = 4` — the cascade's own threshold — is reached at 90% travel and the last tenth passes it | **Anchored** threshold position. The dimensionless `k = 4` is the normalised model's threshold, not a pot value; mapping the two is a calibration step no located source performs, so the linear map is **voiced** |
| Resonance compensation | Input-side: a scaled copy of the input is driven through the resonance path, so raising resonance increases drive *into* the filter, offsetting 8–10 dB of passband loss | Input gain rising to +9 dB at maximum regeneration | **Voiced within the reported 8–10 dB band.** The direction is the point: this filter grows dirtier at high Q rather than thinner, which is the opposite of an output-side make-up gain |
| Oscillation frequency trim | The service procedure has a per-voice trimmer set so the self-oscillation lands on the published pitch | A resonance-dependent frequency trim reaching +20% at maximum | **Voiced coefficient, anchored target.** A nonlinear cascade oscillates below its own small-signal corner because the limit cycle compresses the differential pairs; without the trim the oscillation sat 2.6 semitones flat of the 248 Hz anchor. With it, the rendered oscillation measures 168.0 Hz against the law's 168.3 Hz |
| Envelope | Generated in firmware and **linear** in shape; published segments A 1.5 ms–3 s, D and R 1.5 ms–12 s | Four straight-line segments advanced on the control scan | **Anchored.** Getting this backwards — an exponential generator into a linear amplifier — makes every envelope on the instrument subtly wrong |
| Output amplifier | An OTA whose bias current comes from a discrete exponential converter, roughly 3 mV/dB, with a ~150 mV deadband below which the converter does not conduct | Gain exponential in the envelope over a 66 dB span, hard zero inside the deadband | **Anchored** structure; the 66 dB span is **voiced**. There is deliberately no gain floor: the "bleed" associated with this instrument is a failure mode of degraded voice modules, not designed behaviour |
| Voice assignment | Rotation with note affinity in POLY 1, fixed priority from the first voice in POLY 2, and **no voice stealing** — a seventh held key is dropped | Both policies implemented; a note is dropped when every voice's key is still held | **Anchored.** A voice whose key has been let go is reusable while its release rings, which is what keeps ordinary playing from dropping notes |
| Assign mode switches | Two independent latching POLY buttons; holding both selects unison | Two switches, with unison derived from both being down. There is no unison button because the panel has none | **Corrected in 1.1**, having previously been a three-way selector. Neither button down is not reachable on the hardware, whose buttons interlock; here it falls back to rotation so the assigner is always defined |
| Unison | All six voices on one key. Every timer divides the same reference by the same count, so there is no pitch spread at all | Six coincident voices; what separates them is the analogue block after them | **Anchored.** Adding a detune here would be inventing a behaviour the instrument does not have |
| Voice tolerance | A hardware-fitted figure for this voice module is about ±5% cutoff variance per voice; the service procedure provides per-voice filter trimmers but no per-voice oscillator trimmer | A deterministic per-voice draw over cutoff, resonance, comparator offset, ramp current, amplifier offset and envelope rate, scaled by one Calibration control | **Anchored** magnitude for cutoff; the other five are **voiced**. Calibration at 0 gives a perfectly matched instrument |
| Portamento | Constant *rate* in pitch — about 50 ms per octave at its fastest and 12.9 s at its slowest — advanced on the control scan | The same, so a wider leap takes proportionally longer | **Anchored.** Not a time constant and not a fixed glide time |
| Modulation | One free-running triangle shared by all six voices, 0.1–30 Hz, delay 0–3 s as a silent hold followed by a linear fade that saturates near 1.08 s, re-arming only once every voice is silent | The same, advanced on the control scan so its output is a 238 Hz staircase | **Anchored.** The staircase is audible as faint roughness on deep slow vibrato and smoothing it away would be modelling a different instrument |
| Modulation depths | Pitch ±400 cents, filter ±3.5 octaves, bender pitch ±1 octave, bender filter the whole cutoff range | The same, in cents and in converter counts respectively | **Anchored**, except the bender's filter axis, where one source gives ±3.6 octaves and another ±6; the model takes ±6 |
| Chorus lines | Two 256-stage bucket-brigade lines with their own clock drivers, delay `128/f_clock`, driven by one triangle with the second line inverted | Two shift registers clocked asynchronously to the host rate, input resampled onto the clock edge, output held between edges | **Anchored.** The suite checks the part's 12.8 ms at its 10 kHz minimum and that modulation never drives the clock outside its rated window |
| Chorus modulation | Measured 1.66–5.35 ms in **both** modes, with only the rate differing: 0.513 Hz and 0.863 Hz | The same centre and sweep for all engaged modes | **Anchored** for I and II. Mode II is faster, not deeper, which is why it reads as more agitated rather than wider |
| Chorus I+II rate | Each button switches its own resistor into the modulation oscillator's timing network | Closing both puts the two in parallel, so the conductances and with them the rate add: 1.376 Hz, with depth, centre delay and line gain untouched | **Derived** from the stated topology, which is itself **voiced** -- no located source gives the timing network. What is anchored is the direction: both-down is faster than either alone, not a repeat of II |
| Chorus modes | Two independent latching buttons on the panel; the patch memory stores the effect as one on/off bit and one mode bit | Four panel states -- off, I, II and I+II -- of which the first three are storable | **Corrected in 1.1.** The model previously offered three states on the reasoning that the patch memory holds only two bits. That conflated what the memory can *store* with what the panel can *select*: the buttons are independent, both-down is a documented and audibly distinct setting, and it is simply not recallable from a patch. The SysEx writer reports the loss rather than hiding it |
| Chorus noise | No compander anywhere in the circuit, giving roughly 55–65 dB in-circuit signal-to-noise against the part's own 88 dB datasheet figure | A modelled per-line noise floor, defeatable by a control the hardware does not have | **Voiced within the measured band.** The missing compander is what the effect is known for; the suite asserts the floor lands inside it |
| Chorus support filters | A published fifth-order model of this circuit puts the input filter at 9.9 kHz and the output at 9.5 kHz; a direct measurement of a sibling's wet path fits a *single* pole at 14 kHz across the audio band | One pole at each published corner | **Voiced between two conflicting sources.** A fifth-order pair at 9.5 kHz would be some 20 dB darker at 15 kHz than the measurement allows. Where the corner *lands* is not voiced: the suite bisects the realised half-power point of the coefficient and the recursion together, at both rates the engine runs |
| Chorus mix | One source describes the dry signal buffered off and injected at each channel's final summing node; another states flatly that both outputs are wholly wet | Dry plus wet on both channels, with the two lines clocked in antiphase | **Voiced between two conflicting sources.** A wholly wet path would make mode I a vibrato rather than a chorus, and would not explain the effect thinning when the outputs are summed to mono |
| Oversampling | Standard practice for nonlinear audio | The complete voice, filter, amplifier and both delay lines run at 4x for host rates below 88.2 kHz, 2x below 176.4 kHz, and natively above, followed by a 63-tap Blackman-Harris half-band per stage | Genuine internal oversampling with filtered decimation, not a quality label. Reported latency is the decimators' real group delay |
| Output stage | The instrument's output amplifier has rails | Exactly linear to 80% of full scale, smoothly bounded to unity above it | **Voiced.** Running every source at maximum overdrives the hardware too; what this rules out is a plug-in answering with +9 dBFS |
| Velocity | The instrument's keyboard is not velocity sensitive and its MIDI implementation carries no velocity | A Velocity control defaulting to zero, i.e. to hardware behaviour | Explicitly a control the hardware does not have |

## System-exclusive compatibility

The instrument stores a patch as eighteen bytes -- sixteen continuous controls
at 0..127, then two packed switch bytes -- and `Source/DSP/YouKnow106SysEx.cpp`
reads and writes that layout directly.

| Item | What the format says | Claim |
| --- | --- | --- |
| Patch message | `F0 41 30 0n <18 bytes> F7` | **Anchored** on the documented opcode and framing. Anything else is refused, including foreign manufacturer IDs, wrong opcodes, wrong lengths, a dirty channel nibble and status bytes inside the body |
| Parameter message | `F0 41 32 0n <parameter> <value> F7` | **Anchored.** Applying one moves that control and no other |
| Tone parameter order | LFO rate, LFO delay, DCO LFO, DCO PWM, noise, VCF freq, res, env, LFO, kybd, VCA level, A, D, S, R, sub | **Anchored**, and asserted index by index rather than only round-tripped: a reader and writer that agreed with each other but not with the hardware would round-trip perfectly and still be useless |
| Switch byte 16 | bit 0-2 range 16'/8'/4', bit 3 pulse, bit 4 saw, bit 5 chorus on **active low**, bit 6 chorus mode (1 selects I) | **Anchored.** Both chorus senses are asserted directly; inverting either would silently flip the effect on every patch ever loaded |
| Switch byte 17 | bit 0 PWM manual, bit 1 VCA gate, bit 2 negative polarity, bits 3-4 high-pass position **counting down** | **Anchored.** The high-pass field runs opposite to the panel's numbering, which is the easiest thing in the format to get backwards, so it is asserted position by position |
| What a patch does not carry | Volume, the bender depths, portamento and the assign mode | **Anchored.** These are performance controls on this instrument, so loading a patch deliberately leaves them where the player set them |
| I+II | No encoding exists | **Stated, not hidden.** The writer degrades it to II, the nearer of the two in rate, and `survivesPatchMemory` reports the trip as lossy |
| Malformed range bits | The three range bits are one-hot in practice but nothing enforces it | A message asserting none or several resolves to the middle range rather than being rejected: it is still a message that arrived |

The factory bank in `Source/DSP/YouKnow106Presets.cpp` is written in the same
units and every entry is checked to survive a real patch message, so the bank
can be sent to hardware. Those patches are **original YouKnow106 work**: this
project ships no Roland ROM contents, and the reference instrument's own
factory bank is not reproduced.

## Implemented signal path

The authoritative implementation is `Source/DSP/YouKnow106Engine.cpp` and
`Source/DSP/YouKnow106Chorus.cpp`:

1. A key press allocates a voice under the selected policy, or is dropped if
   every voice's key is still held.
2. On each 238 Hz control scan, per voice: the envelope advances one linear
   step; the glide advances one constant-rate step; the pitch — note, transpose,
   master tune, bender, modulator — is converted to an integer count against the
   8' clock; and the cutoff is summed in converter counts from panel, envelope,
   modulator, bender, key follow and the voice's own tolerance.
3. Per oversampled sample, the cutoff counts and the amplifier control slew
   toward those scan values with the hold capacitor's 522 µs time constant.
4. The oscillator advances by `1/period`. Its ramp is piecewise linear with a
   bow, its two reset corners repaired by slope residuals; the comparator and
   the divide-by-two produce step residuals. Saw, pulse, sub and the shared
   noise sum at the mixing node in volts.
5. The single-pole high-pass leg selected by the switch, then the 122x input
   attenuator and the resonance compensation, then the four transconductor
   stages with the inverting resonance return.
6. The output amplifier's exponential gain, then the summed voice bus, series
   coupling, and the output stage's rails.
7. Both delay lines, clocked in antiphase, dry plus wet on each channel.
8. Half-band decimation to the host rate, then the volume control.

## What still needs measurement

These are the constants the documents bound but do not fix. Each is marked
**voiced** above, and each would move if a calibrated capture were available:

1. The resonance panel-position to loop-gain map, and with it the exact
   oscillation-frequency trim. No located source measures resonance peak height
   or self-oscillation frequency against control setting for this filter family —
   an independent open-source research project records the same gap.
2. Whether the chorus carries a dry path. This is the single largest open
   question in the model: the two accounts are flatly contradictory and the
   difference between them is the difference between a chorus and a vibrato.
3. The chorus support filters' real order and corners.
4. The middle high-pass corner and the bass-boost shelf depth.
5. The ramp's real curvature, which depends on whether the charge path is an
   integrator or a plain resistor.
6. The per-voice tolerance of everything except cutoff.

## Sources

Values were gathered from the instrument's service notes and owner's manual,
component datasheets for the delay line and its clock driver, published
reverse-engineering of the firmware's control tables, and the virtual-analog
literature. The modelling techniques are standard and separately cited:
Zavalishin's topology-preserving transforms for the integrator prewarp and the
`1/(4 − k)` four-pole result; Stilson and Smith on the ladder's root locus;
Huovilainen and D'Angelo and Välimäki on nonlinear ladder solutions; Välimäki,
Pekonen and Nam on integrated-B-spline BLEP residual tables built at 64x with a
Blackman window; and Holters and Parker on bucket-brigade device modelling. No
third-party source code, netlist, ROM image or recording is included in this
repository.

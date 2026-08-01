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

- **anchored** — read from the instrument's own service documentation, a
  component datasheet, or published clean-room analysis of its firmware and
  assigner ROM, and asserted by `Tests/YouKnow106CircuitTests.cpp` or
  `Tests/YouKnow106EngineTests.cpp`;
- **derived** — computed from an anchored value by a stated equation;
- **voiced** — chosen inside a range the sources bound but do not fix.

Where a law is fitted through *behavioural* anchors of a firmware table — the
times or rates the table produces at stated slider positions — rather than
through the table's entries, the row says so. No table data is copied; the
knots are published measurements and the interpolation between them is
YouKnow106's own.

## Claims boundary

| Block | Reference | What YouKnow106 implements | Precise claim |
| --- | --- | --- | --- |
| Note timer | Service notes: an 8 MHz master clock, a range divider producing 1/2/4 MHz, and 8253-class programmable interval timers holding an integer count | One reference clock divided by an integer count computed against the 8' clock, with the RANGE switch changing the clock rather than the count, so the switch transposes by whole octaves and the tuning error is identical in all three ranges | Exact integer-division pitch with its real quantisation (±0.19 cents at A4, ~0.9 cents near the top); **anchored** — A4 at 8' programmes count 4545 and sounds 440.044 Hz, which the suite asserts. Not a fractional-dither or free-running-oscillator model |
| Counter width | 16-bit counters | Counts are clamped to 65535, so the 16' range floors at 15.26 Hz and asking for a lower pitch stops transposing | **Derived**; the suite asserts the floor |
| Control scan | One 12-bit converter multiplexed across 36 sample-and-hold points — six per voice — repeating every 4.2 ms, walking the voices sequentially across the pass; hold time constants from the schematic's own parts: 522 µs on the filter family, 687 µs on the amplifier's divider | Every voice's pitch, envelope, cutoff, pulse threshold, sub level, noise level and amplifier control rewritten once per pass *at that voice's own phase of the walk* — a sixth of the pass after its neighbour — then slewed per sample by the hold constants. Every continuous panel control is digitised to seven bits before the firmware laws consume it, which is what makes it patch-storable | The documented staircase-then-slew control path with its sequential walk, which is why slow bends and vibrato audibly step, why six voices' staircases are decorrelated, and why the shortest usable attack is one scan pass rather than the published 1.5 ms; **anchored** on the pass rate, both hold constants, the walk, and the 7-bit digitisation (the patch dump carries all sixteen continuous controls as 0–127 bytes). The ~125 µs filter-before-amplifier sub-offset within one voice's service is documented but not modelled |
| Ramp generator | Service notes describe an integrator whose capacitor is charged from the compensation voltage, and the published reverse-engineering of this oscillator shows that voltage feeding a resistor into a virtual-ground integrator — a constant-current charge | A straight 12 Vpp rising ramp with a finite-slope reset of 2.2 µs. The compensation voltage that holds the amplitude constant is modelled as one of the scanned, slewed control voltages, while the timer count steps instantly — so every pitch step, bend, glide and octave change leaves a momentary amplitude error on the ramp *and* the pulse until the hold catches up, exactly the transient the hardware's own architecture produces | **Anchored** shape — an earlier revision kept a bow on a misreading of the reverse-engineering account; the straight ramp is also the only shape consistent with the comparator's 6 V / 50% duty anchor. The compensation-slew transient is **derived** from the scan architecture |
| Ramp bandlimiting | Integrated-B-spline residual tables built by numerically integrating a Blackman-windowed sinc at 64x, as in the LUT-BLEP literature | Both corners of the reset are slope discontinuities, not a jump, so each is repaired with the *slope* residual; the comparator and divider edges get the *step* residual | Alias floor below −55 dB across the keyboard at 44.1 and 48 kHz, asserted by the engine suite. A closed-form polynomial fit was tried first and was *worse than no correction at all* — the tables are integrated, not fitted |
| Pulse and PWM | Comparator against a control voltage: 50% duty at +6 V, 95% at +0.6 V, and −0.8 V pins the output high; the threshold is one of the six per-voice hold points; the service notes hold the pulse, like the saw, at approximately 12 Vpp via the compensation voltage | Duty derived from the slewed threshold against the momentarily mis-scaled ramp, so fast LFO PWM is rounded by the hold rather than stepped, and pitch steps nudge the duty for a millisecond or two. The LFO reaches the pulse width *raw*: the firmware's delay envelope gates pitch and cutoff but not PWM. The two-position source switch is LFO or MANUAL | **Anchored** on both duty figures, the hold path, and the ungated LFO-PWM (firmware analysis). The pulse can reach neither 0% nor 100%, which the suite asserts. Pulse-off as the −0.8 V pinning, and the mixer's coupling of a pinned leg, are **not modelled** — the switch gates the leg — recorded as an open question |
| Sub oscillator | A divide-by-two flip-flop clocked by the counter's terminal pulse — the same pulse that fires the ramp's discharge | An exact square one octave below the selected footage, unaffected by pulse width, its edges at the reset's *start*; its level is one of the scanned, slewed per-voice control voltages, 7-bit like the rest of the patch | **Anchored**, including the level being patch data (sysex byte 15), which a pot in the audio path could not be. There is no sub-octave selector, because the instrument has none |
| Noise | One shared generator mixed into every voice through a per-voice level CV | A single shared source added to each voice before the high-pass, its level a scanned, slewed, 7-bit per-voice control voltage | **Anchored** topology and control path (sysex byte 4); the 4 Vpp mix weight is **voiced** — the service procedure has a module-board noise-level adjustment but no located source states its target |
| High-pass | A four-position switched RC network ahead of the filter, one leg per voice, selected by a CMOS switch; the setting is a patch bit | Position 0 is the measured boost shelf: +10.5 dB at DC falling to +1.41 dB in the high band across a corner near 59 Hz. Position 1 passes the band untouched. Positions 2 and 3 are single-pole high-passes at 236 Hz and 754 Hz, the corners the network's own 44.9 kΩ against 15 nF and 4.7 nF produce | **Anchored** — the boost was verified against a hardware noise sweep and is three times the +3 dB an earlier account reported; the cut corners follow from the part values. Modelled at first order; the measured shelf's second, nearly-cancelling pole-zero pair is dropped |
| Filter core | The potted voice module contains a quad-OTA four-pole chip with an on-chip antilog converter; per stage 68 kΩ input resistor, 560 Ω, 240 pF integrator | Four transconductor stages solved together implicitly, `C dVn/dt = Ig tanh((V(n-1) − Vn)/H)` with `H = 2·Vt/attenuation = 6.37 V`, trapezoidally integrated and closed with a damped Newton step whose Jacobian is bidiagonal plus one corner term | **Anchored** topology and component values. The suite checks the model against a fourth-order Runge-Kutta solve of the same ODE at 16x, and both against the closed-form `1/(4 − k)`, to 0.6 dB |
| Filter drive level | The 68 k/560 Ω pair attenuates each stage's differential input by 122x | That attenuator, not a voiced "drive" control, is what puts the differential pair's linear span at ±6.4 V — right at the peak of a full-level ramp | **Derived**. It is the structural reason this filter compresses gently rather than either staying clean or clipping hard |
| Cutoff control law | Firmware: the panel byte times 128, envelope, modulator, bender and key-follow terms summed in a 14-bit accumulator clamped to [0, 16383], the top 12 bits driving the converter; 5.53 Hz at code 0, 1143 counts per octave; service check of 248 Hz self-oscillation at code 6272; published range 5 Hz–50 kHz, a measured example of the converter topping out near 52 kHz | `f = 5.53 · 2^(counts/1143)` exact through the audio band, saturating smoothly onto the measured ceiling above a 24 kHz knee; the digital sum clamped to 14 bits and truncated to 4-count steps, with the analogue trims and drift riding below the converter's resolution, exactly where the hardware's trimmers sit | **Anchored**; the suite asserts the service anchors, the clamp — which is also why no modulation sum can drive the cutoff below 5.53 Hz — the truncation, the knee point (22.6 kHz at count 13716) and the ceiling. An earlier revision clamped at 24 kHz citing a specification figure that does not exist. Summing in the count domain is why every modulation source on this instrument is exponential in hertz |
| Resonance | A patch parameter (sysex byte 7): one converter output, sample-held and shared by every voice's regeneration amplifier; the loop closes from the four-pole output through a 100 k/1.5 kΩ divider into that amplifier's differential pair; self-oscillation from ~90% of the panel travel; a fitted hardware measurement gives loop gain ≈0.91 at 30% travel and ≈4.19 at the top | The resonance control voltage staircases at the scan rate, quantised to its byte and slewed on a hold constant like every other scanned point. The panel-to-loop-gain curve is the quadratic through the fitted 30% measurement and the 90% threshold, then linear to the fitted 4.19. The loop itself passes through a tanh with the divider's own span (≈3.5 V), which is what bounds the limit cycle | **Anchored** threshold position, curve anchors and loop topology. The curve between the anchors is a fit; the shared-versus-per-voice accounting of the resonance hold is not settled by the located sources and one shared CV is assumed |
| Resonance compensation | Input-side: a scaled copy of the input is driven through the resonance path, so raising resonance increases drive *into* the filter; a fitted hardware measurement puts the gain linear in the loop gain, reaching just under 6 dB at the top | Input gain `1 + 0.23·k`, ≈+5.9 dB at the fitted maximum | **Anchored to the fitted measurement** — an earlier revision voiced +9 dB from a coarser account. The direction is the point: this filter grows dirtier at high Q rather than thinner, the opposite of an output-side make-up gain |
| Oscillation frequency trim | The service procedure has a per-voice trimmer set so the self-oscillation lands on the published pitch; the hardware's oscillation sits within a few cents of the small-signal law because the *loop*, not the forward path, limits the cycle | With the divider-limited loop modelled, the residual error is the forward stages' slight compression — the first pole sees nearly three times the fourth's swing — and a small trim (≈2% at threshold, ≈4.9% at the top) absorbs it. The rendered oscillation lands within ~1 cent of the law, asserted at the service anchor by the engine suite | **Anchored** target and mechanism; the trim coefficient is the calibration the service procedure itself performs. An earlier revision needed a 20% trim because its unlimited loop let the forward stages clamp the cycle |
| Envelope | Generated in firmware: a 14-bit integer advanced once per scan pass. The attack adds a fixed increment — a straight line whose duration is *linear in the slider* across the lower half (~8.4 ms per step, ~0.54 s at mid-travel, ~3.3 s at the top). Decay and release multiply the remaining distance by a per-pass coefficient — exponential in the CV domain — with measured times of ~0.33/2.2/4.5 s to −20 dB at quarter/half/three-quarter travel and ~9.6 s to half level at the top; the falling segments end by integer truncation. A key press never resets the accumulator; sustain moved up mid-note snaps in one pass, moved down it decays at the decay rate | The same, with the level quantised to the 14-bit grid every pass. Laws are fits through the behavioural anchors above, not table copies | **Anchored** structure and anchors. An earlier revision made all four segments straight lines with endpoint-exponential slider laws — mid-travel attacks were eight times too fast and the falling curve family was wrong |
| Output amplifier | An OTA whose control current comes from a strongly emitter-degenerated converter; a 56-point sweep of a real unit shows gain tracking the control voltage *linearly* above roughly a tenth of the range — half control is 6 dB down, not 30 — with an exponential knee below (~26 dB per tenth of range) and a conduction deadband at the bottom | Gain linear in the slewed control with the measured knee and a hard zero below conduction. The instrument's dB-linear decay tails come from the generator's exponential segments through this quasi-linear amplifier — the opposite factorisation from the linear-generator-exponential-amplifier model an earlier revision used, which sat mid-level sustains tens of decibels too quiet | **Anchored to the measured sweep and the converter's own circuit**. There is deliberately no gain floor: the "bleed" associated with this instrument is a failure mode of degraded voice modules, not designed behaviour |
| Voice assignment | Assigner ROM: POLY 1 keeps per-voice note memory that survives the release — a repeated pitch reclaims the voice that last played it — and otherwise takes the free voice released longest ago; POLY 2 is a plain linear scan from the first voice up, chopping old tails; **no voice stealing in either mode** — a seventh held key is dropped | Both policies, with the note memory and release-recency implemented on per-voice state that survives retirement | **Anchored** — the no-steal policy is the ROM's own capacity check, and the suite asserts it. A voice whose key has been let go is reusable while its release rings, which is what keeps ordinary playing from dropping notes |
| Unison | All six voices on one key. Every timer divides the same reference by the same count, so there is no pitch spread at all | Six coincident-frequency voices; what separates them is the analogue block after them, plus the scan walk's phase decorrelation | **Anchored.** Adding a detune here would be inventing a behaviour the instrument does not have |
| Voice tolerance | Per-voice analogue dispersion: two filter trimmers per voice (offset and scale of the cutoff CV, the ~±5% module figure), regeneration-amplifier gain spread, comparator offset, ramp-current spread, amplifier offset and gain spread, per-voice level-CV and high-pass part spread — and *no* envelope dispersion, because the envelopes are computed digitally in one processor and are identical across voices | One deterministic draw per mechanism, scaled by one Calibration control; slow thermal wander on the analogue chain only | **Anchored** magnitude for cutoff and mechanism inventory; the other magnitudes are **voiced**. An earlier revision drew a per-voice envelope-rate error, which the digital generator makes impossible. Calibration at 0 gives a perfectly matched instrument |
| Portamento | Constant *rate* in pitch — about 50 ms per octave at its fastest and 12.9 s at its slowest — advanced on the scan, on a **per-voice** accumulator that survives reassignment, so a reassigned voice glides from whatever *it* last played | The same, including the notorious poly-mode glides from notes several assignments back; a slot with no history this run simply starts at its pitch | **Anchored**, including the per-voice origin. Not a time constant, not a fixed glide time, and not a single global glide source |
| Modulation | One free-running triangle shared by all six voices — but not a phase accumulator: the firmware adds a rate coefficient to a hard-clamped integer accumulator, flipping direction at the clamp and polarity at the bottom, so the clamp discards the overshoot and fast settings quantise onto whole passes per sweep (verified on hardware to <1%). Measured range 0.04–29.8 Hz, roughly linear in the slider, not the specification sheet's rounded 0.1–30 exponential. Delay: a silent hold advancing at the attack table's own rate across the whole travel, then a fade selected by the top three bits of the pot — instant across the bottom eighth, three short steps, then a fixed ≈1.09 s ramp for the entire upper half (≈4.4 s total at the top). The delay re-arms as soon as every key is up — ringing releases keep their vibrato — and restarts on the next note-on | The same mechanism, advanced on the scan so the output is a 238 Hz staircase | **Anchored** mechanism and behavioural anchors; the coefficient curve between them is a fit. An earlier revision used an endpoint-exponential rate law, a continuous phase, a 3 s quadratic delay and re-armed only at silence — four separate deviations from the firmware |
| Modulation depths | Pitch ±400 cents at full slider, filter ±3.5 octaves, bender pitch ±1 octave; the bender's filter axis maxes at 4064 counts ≈ ±3.6 octaves — the firmware's sensitivity-times-bend arithmetic, which settles a two-source disagreement an earlier revision resolved the other way (±6). The panel's LFO depth and the lever's LFO axis are *summed* by the firmware, so both together reach deeper than either alone | The same, in cents and converter counts; the bender sampled once per pass at 8-bit resolution with no extra smoothing | **Anchored** |
| Chorus lines | Two 256-stage bucket-brigade lines (10–200 kHz clock window, delay `128/f_clock`), each with its own clock driver, one line per output channel, driven by one triangle with the second line's modulation inverted; each channel carries dry plus its own wet, and the wet is muted by a transistor pair when the effect is off — the lines keep clocking regardless | Two shift registers clocked in antiphase asynchronously to the host rate, input resampled onto the clock edge, output held between edges; dry plus wet per channel; mode changes step the clock programme immediately with only a short declick on the wet mute | **Anchored.** The dry path — which an earlier revision called its largest open question — is settled for this instrument: wet-only is a documented *fault* of the mute transistors, and the dedicated-clone literature implements dry plus wet per channel. The suite checks the part's 12.8 ms at its 10 kHz minimum and that modulation never leaves the rated window |
| Chorus modulation | Measured on the sibling instrument's chorus, the closest calibrated capture located: 1.66–5.35 ms in **both** modes, rates 0.513 Hz and 0.863 Hz; this instrument's own reported figures — ≈0.5 and ≈0.8 Hz, modes differing in rate alone — agree to their stated precision | The same centre and sweep for both modes | **Anchored to the sibling measurement**, with the decimals open for this instrument. Mode II is faster, not deeper, which is why it reads as more agitated rather than wider |
| Chorus modes | The patch memory stores the effect as one on/off bit and one mode bit | Three states: off, I, II | **Anchored.** The both-at-once mode belongs to the earlier instruments in the family and is deliberately absent |
| Chorus nonlinearity | The delay line is the first thing in the wet path to overload: rated 0.3% distortion at 0.78 Vrms with the surrounding op-amps on ±15 V rails, and the sibling measurement observed wet-only distortion when driven | The line's write saturates at the bias window's ≈2.9 V swing, so a hot bus grits the wet while the dry stays clean — the driven-chorus signature | **Anchored** mechanism and rated point; the exact pre-line divider is **voiced** |
| Chorus charge transfer | Per-stage charge-transfer loss condensed to one pole at the clock rate; the per-stage-loss literature implies 1–4 dB of aggregate droop at the clock's own Nyquist for a 256-stage low-noise line | Per-edge retention set for 2 dB at the clock Nyquist — the middle of that band | **Voiced within the derived band.** An earlier revision's retention of 0.34 parked a modulation-swept lowpass at 1.6–5.2 kHz — several times darker than any account of this circuit permits |
| Chorus noise | No compander anywhere in the circuit, giving roughly 55–65 dB in-circuit signal-to-noise against the part's own 88 dB datasheet figure | A modelled per-line noise floor, defeatable by a control the hardware does not have | **Voiced within the reported band**, which itself needs a citation; the suite asserts the floor lands inside it. The missing compander is what the effect is known for |
| Chorus support filters | A published fifth-order model of this circuit puts the input filter at 9.9 kHz and the output at 9.5 kHz; a direct measurement of a sibling's wet path fits a *single* pole at 14 kHz across the audio band | One pole at each published corner | **Voiced between two conflicting sources.** Where the corner *lands* is not voiced: the suite bisects the realised half-power point of the coefficient and the recursion together, at both rates the engine runs |
| Oversampling | Standard practice for nonlinear audio | The complete voice, filter, amplifier and both delay lines run at 4x for host rates below 88.2 kHz, 2x below 176.4 kHz, and natively above, followed by a 63-tap Blackman-Harris half-band per stage | Genuine internal oversampling with filtered decimation, not a quality label. Reported latency is the decimators' real group delay. With oversampling off, the delay lines' clock-rate images fold with only the reconstruction pole to soften them — a documented cost of that setting |
| Output stage | The signal order is voice sum, chorus, volume, output amplifier: the rails sit *after* the effect, so dry plus wet meet them together | Exactly linear to 80% of full scale, smoothly bounded to unity above it, applied per channel after the chorus mix | **Anchored** placement, **voiced** ceiling. An earlier revision bounded the signal ahead of the chorus, which let the summed dry-plus-wet leave the plug-in more than 7 dB above full scale |
| Velocity | The instrument's keyboard is not velocity sensitive and its MIDI carries no velocity; the only control change it receives is hold | A Velocity control defaulting to zero, and a CC 1 mapping onto the lever's LFO axis defaulting to zero depth | Explicitly controls the hardware does not have, inert at their defaults |

## Implemented signal path

The authoritative implementation is `Source/DSP/YouKnow106Engine.cpp` and
`Source/DSP/YouKnow106Chorus.cpp`:

1. A key press allocates a voice under the assigner ROM's policy — note
   memory, then longest-released — or is dropped if every voice's key is
   still held.
2. On each 238 Hz pass the converter walks the voices in turn, each at its
   own phase: the envelope advances one pass (linear attack, multiplicative
   decay and release on the 14-bit grid); the per-voice glide advances one
   constant-rate step from that voice's own history; the pitch is converted
   to an integer count against the 8' clock; the cutoff is summed in counts
   from the 7-bit panel byte, envelope, modulator, bender and key follow,
   clamped to 14 bits and truncated to the converter's 12; and the pulse
   threshold, sub level, noise level, amplifier control and oscillator
   compensation voltage are written as this voice's held control voltages.
   The lever and the shared resonance voltage are sampled once per pass.
3. Per oversampled sample, every held voltage slews on its hold constant —
   522 µs, or 687 µs for the amplifier — and the analogue side adds the
   per-voice trims, spreads and thermal wander below the converter's
   resolution.
4. The oscillator advances by `1/period`. Its straight ramp's two reset
   corners are repaired by slope residuals; the comparator and the
   divide-by-two produce step residuals, the sub's edge at the reset's
   start. The ramp and pulse carry the compensation voltage's momentary
   amplitude error; saw, pulse, sub and the shared noise sum at the mixing
   node in volts.
5. The high-pass leg selected by the switch (the measured boost shelf on
   position 0), then the 122x input attenuator and the fitted resonance
   compensation, then the four transconductor stages with the resonance
   return closed through the divider-limited loop pair.
6. The quasi-linear output amplifier with its measured knee, then the summed
   voice bus and series coupling.
7. Both delay lines, clocked in antiphase, each write saturating at the
   part's swing, dry plus wet on each channel — then the output stage's
   rails, after the mix, as on the hardware.
8. Half-band decimation to the host rate, then the volume control — the one
   true potentiometer in the audio path.

## What still needs measurement

These are the constants the documents bound but do not fix. Each would move if
a calibrated capture were available:

1. The noise generator's mix level against the ramp — the service adjustment
   exists but no located source states its target.
2. The chorus support filters' real order and corners, the in-circuit
   signal-to-noise band, the wet gain, and this instrument's own sweep and
   rate decimals (the sibling's are used).
3. The exact pre-line divider ahead of the delay lines' saturation.
4. The hold time constants of the pulse, sub, noise and resonance points
   (the filter family's 522 µs is assumed), and whether the resonance hold
   is genuinely one shared point.
5. The resonance curve between its three anchors, and the per-voice spread
   of everything except cutoff.
6. The mixer's behaviour when the pulse leg is pinned by the −0.8 V control
   (pulse switched off): the model gates the leg instead.
7. The ~125 µs filter-before-amplifier write order within one voice's scan
   service.

## Sources

Values were gathered from the instrument's service notes and owner's manual;
component datasheets for the delay line and its clock driver; published
clean-room reverse engineering of the voice-processor firmware and assigner
ROM — including its schematic-derived hold-capacitor constants, its measured
converter table anchors, and its hardware-verified envelope, amplifier and
modulator sweeps; the published analysis of this DCO's charge circuit; the
sibling instrument's chorus measurements; and the virtual-analog literature.
The modelling techniques are standard and separately cited: Zavalishin's
topology-preserving transforms for the integrator prewarp and the `1/(4 − k)`
four-pole result; Stilson and Smith on the ladder's root locus; Huovilainen
and D'Angelo and Välimäki on nonlinear ladder solutions; Välimäki, Pekonen and
Nam on integrated-B-spline BLEP residual tables built at 64x with a Blackman
window; and Holters and Parker on bucket-brigade device modelling. No
third-party source code, table data, netlist, ROM image or recording is
included in this repository; laws fitted through published behavioural anchors
are YouKnow106's own constructions.

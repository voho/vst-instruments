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
  component datasheet, a calibrated measurement, or firmware behaviour that
  has independent primary/hardware corroboration, and asserted by
  `Tests/YouKnow106CircuitTests.cpp` or `Tests/YouKnow106EngineTests.cpp`;
- **ROM-resolved** — exact for one explicitly hash-identified supplied firmware
  image, without claiming that another revision is identical;
- **derived** — computed from an anchored value by a stated equation;
- **product policy** — an explicit plug-in choice, not a hardware claim; or
- **voiced** — chosen inside a range the sources bound but do not fix.

Where a law is fitted through *behavioural* anchors of a firmware table — the
times or rates the table produces at stated slider positions — rather than
through the table's entries, the row says so. No table data is copied; the
knots are published measurements and the interpolation between them is
YouKnow106's own.

The supplied B-2 image's effective first 4 KiB has SHA-256
`b75d27d181dee58a7e969aa5119e6ac96f624066a8d84b22eb7f2523988e4527`;
the supplied A-5 assigner image's effective first 4 KiB has SHA-256
`72132b8803bd02d2640612aa0055a05fb2f478b03103a6031ddae642fd96a8f5`.
The second half of both 8 KiB containers is all-`FF` padding. Exact OQ-12
through OQ-14 behavior is scoped to that B-2 image, and assigner behavior below
to that A-5 image. The project records equations, region hashes and behavioral
vectors, never ROM/table contents. The separately located
annotated IC29 disassembly explicitly describes itself as partial, inaccurate,
incomplete and unofficial. Any assertion supported only by that text remains a
provenance-pending lead, not Roland-authored evidence.

## Claims boundary

| Block | Reference | What YouKnow106 implements | Precise claim |
| --- | --- | --- | --- |
| Note timer | Service notes: an 8 MHz master clock, a range divider producing 1/2/4 MHz, and 8253-class programmable interval timers holding an integer count | One reference clock divided by an integer count computed against the 8' clock, with the RANGE switch changing the clock rather than the count, so the switch transposes by whole octaves and the tuning error is identical in all three ranges | Exact integer-division pitch with its real quantisation (±0.19 cents at A4, ~0.9 cents near the top); **anchored** — A4 at 8' programmes count 4545 and sounds 440.044 Hz, which the suite asserts. Not a fractional-dither or free-running-oscillator model |
| Counter width | 16-bit counters | Counts are clamped to 65535, so the 16' range floors at 15.26 Hz and asking for a lower pitch stops transposing | **Derived**; the suite asserts the floor |
| Control scan | Service Notes pp. 5, 8 and 13: one 12-bit converter and three 8-way muxes, with 18 per-card holds (DCO, VCF and ENV/GATE VCA for six cards), five shared holds (SUB, stored VCA LEVEL, PWM, RESONANCE and NOISE), one unused channel, and a 4.2 ms pass. The p. 8 chart orders shared RES/VCA/SUB, DCO 1–6, PWM, interleaved VCF/VCA 1–6, then NOISE and depicts them sequentially across the pass | A fractional scheduler preserves the nominal 4.2 ms average without host-rate truncation. Each pass executes the exact 23-write logical queue. The default `NormalizedServiceChart` compatibility profile spaces its ordinal events monotonically across the pass so six DCO resets are not collapsed onto one sample; `PhaseZeroDiagnostic` retains only order for comparison. Each destination has a separately named hold constant; only VCF 522 µs and voice VCA 687 µs are evidence-backed, while currently equal values on other nodes remain compatibility defaults | Topology, nominal pass, ordinal order and qualitative non-simultaneity are **anchored** by the Service Notes. The supplied hash-matched B-2's `b<<7` aligned work word and `b<<5` physical 12-bit DAC code are **ROM-resolved**; the two domains must not be conflated. Normalized offsets are explicit **product/compatibility policy**, not measured timestamps. Absolute offsets, jitter, branches and other destination constants remain OQ-07/OQ-08 |
| Ramp generator | Service notes describe an integrator whose capacitor is charged from the compensation voltage, and the published reverse-engineering of this oscillator shows that voltage feeding a resistor into a virtual-ground integrator — a constant-current charge | A straight 12 Vpp rising ramp with a finite-slope reset of 2.2 µs. The compensation voltage that holds the amplitude constant is modelled as one of the scanned, slewed control voltages, while the timer count steps instantly — so every pitch step, bend, glide and octave change leaves a momentary amplitude error on the ramp *and* the pulse until the hold catches up, exactly the transient the hardware's own architecture produces | **Anchored** shape — an earlier revision kept a bow on a misreading of the reverse-engineering account; the straight ramp is also the only shape consistent with the comparator's 6 V / 50% duty anchor. The compensation-slew transient is **derived** from the scan architecture |
| Ramp bandlimiting | Integrated-B-spline residual tables built by numerically integrating a Blackman-windowed sinc at 64x, as in the LUT-BLEP literature | Both natural reset corners are slope discontinuities, so each uses the *slope* residual; comparator/divider edges use the *step* residual. The current changed-pitch-write model restarts phase/comparator/divider at the converter interval boundary while preserving pending histories and bandlimiting the resulting value/slope changes; only a hard engine reset or virtual-card teardown clears them | Natural-corner treatment is a numerical antialiasing requirement, and the suite asserts an alias floor below −55 dB across the keyboard at 44.1 and 48 kHz. The restart fixture bounds its largest normalized pulse increment below 0.65 of a hard step, but the physical timer-reload edge and the ramp/comparator/sub states it actually forces remain OQ-08; the current restart semantics are **model policy**, not measured hardware fact |
| Pulse and PWM | Comparator against a control voltage: 50% duty at +6 V, 95% at +0.6 V, and Service Notes p. 9 says −0.8 V pins the output high. PWM is one shared hold | Duty derives from the shared slewed threshold against each card's momentarily mis-scaled ramp, retaining per-card comparator offset. The comparator crossing solver follows both the moving ramp and the slewing PWM threshold inside each audio sample; this prevents an implementation-only missed edge/full-cycle blip under deep PWM. Pulse Off writes −0.8 V and pins the modeled comparator high while DCO/sub keep running; the audio contribution remains hard-gated because the local comparator-to-voice-mixer coupling/loading is unmeasured. A temporarily under-compensated ramp can also sit wholly below an enabled positive threshold, which the renderer treats as pinned low rather than forcing a 5% pulse. The final C17/C20 output coupling does not answer the upstream off-state question. The LFO reaches PWM raw, a provenance-pending firmware lead | Duty anchors, shared ownership and the off control/comparator state are **anchored**. The moving-threshold solve is a numerical consequence of that topology. The nominal calibrated enabled range is 5–95%; pitch-hold/ramp-current mismatch can move the crossing beyond it. Pinned-leg DC/bleed/loading/transient remain OQ-11, so no audible off artifact or anti-click envelope is invented |
| Sub oscillator | A divide-by-two flip-flop clocked by the counter's terminal pulse; Service Notes pp. 8–9 show one shared SUB LEVEL hold controlling its collector-supply amplitude | An exact square one octave below the selected footage, unaffected by pulse width, with edges at reset start; every card consumes the same scanned/slewed 7-bit sub-level voltage before card-specific analogue error | **Anchored** topology, divider relationship, shared ownership and stored path (SysEx byte 15). There is no sub-octave selector. Exact full-scale amplitude and loaded mixer transfer remain OQ-15 |
| Noise | One shared generator and one shared NOISE LEVEL hold feed all voice mixers | A single bounded-uniform white source added to each voice before its filter, scaled by one scanned/slewed 7-bit shared level then card-specific residual error; a separate tiny per-voice excitation starts a silent self-oscillating filter. Both discrete sources scale by `sqrt(internal_rate / 192 kHz)` so their wall-clock spectral density does not change with host rate or HQ mode; 192 kHz is the pre-existing 48 kHz/HQ reference and therefore preserves that sound exactly | **Anchored** shared source/hold topology, control path (SysEx byte 4) and 4.0 Vpp TP8 adjustment. Rate normalization is a numerical/product requirement, not a hardware-amplitude claim. TP8 is downstream and does not establish the model's pre-filter `+/-2 V` coordinate, RMS or distribution; those and startup excitation are **voiced** pending OQ-15/OQ-16 |
| Voice summer and high-pass | IC1a receives every voice through 33 kΩ against 3.3 kΩ feedback. Its summed output crosses C14 10 µF NP into R39 33 kΩ before the one four-position switched high-pass network on the jack board | The six voices sum at `3.3/33 = 0.1` each, then cross one C14 state whose sub-hertz resistance follows the selected leg: Boost/Flat add R25/R26 47 kΩ in parallel with R39 for 0.820915 Hz; the capacitor-selected Cut legs are open there, leaving R39 and 0.482288 Hz. The shared switch then applies the measured Boost shelf, Flat pass, or 225.8/720.5 Hz cut pole | **Anchored/derived** placement, parts and asymptotic loads from the service-note schematic. The cut capacitors begin loading C14 as frequency rises; full switched-network MNA, CMOS parasitics and deselected-capacitor switching memory remain OQ-21. The present asymptotic common state is explicit rather than falsely claiming C14 is absent. Unity-summing the voices overdrives every downstream common stage by 20 dB |
| Filter core | A photographed A1QH80017A teardown identifies one IR3109 quad OTA/filter plus two BA662s; the service circuit gives a 68 kΩ input resistor, 560 Ω shunt and 240 pF integrator per stage | Four transconductor stages solved together implicitly, `C dVn/dt = Ig tanh((V(n-1) − Vn)/H)` with `H = 2·Vt/attenuation = 6.37 V`, trapezoidally integrated and closed with a damped Newton step whose Jacobian is bidiagonal plus one corner term | Device identity is **anchored by the photographed teardown** and the topology/component values are **anchored**. The suite checks the model against a fourth-order Runge-Kutta solve of the same ODE at 16x, and both against the closed-form `1/(4 − k)`, to 0.6 dB |
| Filter drive level | The 68 kΩ/560 Ω divider attenuates each stage's differential input by `560/(68000+560) = 0.00816803`, or 122.43:1 | That attenuator, not a user-facing "drive" control, sets the differential pair's nonlinear span; the engine currently uses `+/-6 V` saw/pulse, `+/-5 V` sub and `+/-2 V` noise coordinates followed by a 0.40 scale | The component attenuator and OTA span are **derived**. A centered `+/-6 V` interpretation is merely compatible with a 12 Vpp reading at the same source node; it does not establish the loaded transfer. The sub/noise coordinates and 0.40 mapping are **voiced compatibility** pending OQ-15 |
| Cutoff control law | Firmware: the panel byte times 128, envelope, modulator, bender and key-follow terms summed in a 14-bit accumulator clamped to [0, 16383], the top 12 bits driving the converter; 5.53 Hz at code 0, 1143 counts per octave; service check of 248 Hz self-oscillation at code 6272. Service Notes p. 1 publishes an approximate 5 Hz–50 kHz range | `f = 5.53 · 2^(counts/1143)` through its established range, followed in the default profile by a transparent numerical `min(..., 50000 Hz)` cap. The digital sum is clamped and truncated to 4-count steps. The former 24 kHz/tanh/52.2 kHz curve may be retained only as a named legacy profile | Count-domain sum, base, octave slope, service point, clamp and truncation are **anchored**. The 50 kHz cap is **product policy**, not a claim about converter saturation. A described 93-point/single-card fallback table lacks the complete raw capture, metadata and population scope needed to resolve OQ-18 |
| Resonance | A photographed A1QH80017A teardown assigns one BA662 to the IR3109 resonance-feedback path; Service Notes p. 19 trims every card to a 4.8 Vpp self-oscillating sine. Hash-identified B-2 behavior forms aligned work word `W=128b` and physical converter code `DAC12=32b` from stored resonance byte `b`; Service Notes pp. 5, 8 and 13 establish one shared IC26-channel-6 hold. No qualifying original-unit sweep establishes the subsequent DAC-voltage/current-to-loop-gain transfer | The exact stored-byte conversion and one shared queue write feed a named `VoicedResonanceCompatibilityProfile`. That replaceable profile retains the existing quadratic/linear panel-to-loop curve, circuit-shaped nonlinear return and optional per-card Unit Character residual without changing preset bytes | BA662/IR3109 identity, shared ownership and the service endpoint are **anchored**; `b → W → DAC12` is **ROM-resolved** for the identified image. The 4.8 Vpp adjustment has no published tolerance and does not identify loop gain. Every numerical analogue step after the DAC — including the current 30%/90%/maximum landmarks, loop limiter and card residual magnitude — is **voiced compatibility**, not a fitted, measured or calibrated hardware law (OQ-09/OQ-10) |
| Resonance compensation | No qualifying raw original-unit transfer or circuit-de-embedded sweep was located for the compensation path independently of filter saturation and output amplitude | The named resonance compatibility profile retains input multiplier `1 + 0.2296·k`, preserving the current high-Q drive character | The direction, coefficient and resulting maximum boost are all **voiced compatibility**. They are not hardware measurement anchors and may be replaced with the rest of OQ-09's analogue profile without changing the verified byte/DAC path |
| Oscillation frequency correction | The service procedure establishes that a per-card adjustment exists, but does not establish the model's feedback-dependent correction curve or its coefficient | The named resonance compatibility profile retains `1 + 0.045·min(k/4,1.2)²` before the explicit 50 kHz product cap | The existence of an adjustment does not validate this equation. Its threshold-scale denominator, 4.5% amount and rendered pitch result are **voiced compatibility/model calibration**, not original-unit transfer evidence |
| Envelope | Hash-identified B-2: one 14-bit state per generated envelope, sustain `S=128b`, saturating additive attack without retrigger reset, and shared decay/release coefficient selection. For `v_hi=v>>8`, `v_lo=v&255`, `c_hi=c>>8`, `c_lo=c&255`, its fall helper is `Q(v,c)=c_hi*v_hi+floor(c_lo*v_hi/256)+floor(c_hi*v_lo/256)`; the low×low term is intentionally omitted | Attack is `min(0x3FFF,E+A[b])`; decay is `S+Q(E-S,c)` when above sustain and otherwise snaps to `S`; release is `Q(E,c)`. The recurrence retains both low bits, while the VCF envelope path, ENV-mode voice VCA and display consume the physical 12-bit fraction `(E>>2)/4095`. The attack region `0x0B60–0x0C5F` hashes to `faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`; decay/release `0x0D60–0x0E5F` hashes to `0de73bedf11904538056eec3622b09470461f13ad016103ab9992be73e467754` | **ROM-resolved** for the stated B-2 image, including coefficients, DAC truncation, rounding, clamp, sustain and retrigger semantics. OQ-12 now concerns hardware pass timing/jitter, analogue-node/audible thresholds, independent behavioral confirmation and other revisions, not recovering these tables |
| Voice-module VCA | A photographed A1QH80017A teardown identifies the second BA662 as the per-voice VCA, controlled only by the ENV/GATE selection and its envelope control voltage; Service Notes p. 19 trims its output to 6 Vpp | `VoicedVoiceVcaCompatibilityProfile` retains the current quasi-linear law, 0.12 knee, 260 dB-per-unit low-level slope and 0.005 hard-zero rule. Velocity is an optional plug-in extension applied here | BA662 identity, topology, ENV/GATE ownership and the service endpoint are **anchored**. The 6 Vpp adjustment has no published tolerance and does not establish the control curve. That entire analogue control-to-gain curve remains **voiced compatibility**: no qualifying raw original-module sweep establishes its shape or numbers, and a measurement floor cannot prove a hard deadband (OQ-19). Card residuals are OQ-10; common VCA LEVEL is deliberately absent because it is downstream |
| Stored VCA LEVEL | The stored VCA LEVEL parameter drives the common uPC1252H2 on the jack board, downstream of the voice sum and shared high-pass and upstream of the chorus. Roland populates the NEC application input as C12 10 µF NP followed by R36 33 kΩ. NEC specifies the IC's GC1 control constant as −5.9 mV/dB typical (5.8–6.1 mV/dB magnitude) over −30 to +30 dB under its stated test conditions | One independent C12/R36 0.482288 Hz coupling state followed by one quantised, slewed common gain used to match patch loudness and chorus drive. The current dB-domain curve is a provisional fit to reported points: approximately −15 dB at panel −5, −12.5 dB at 0 and +5 dB at +5 | C12/R36 topology, input resistance, derived pole, device GC1-voltage-to-gain law, VCA placement and shared ownership are **anchored** by the schematic and 1983 NEC µPC1252H2 datasheet. Roland's byte/DAC/hold-network-to-GC1 voltage, offset and installed endpoints remain **voiced/fitted** pending OQ-02 |
| Voice assignment | Hash-identified A-5 assigner behavior: POLY 1 keeps per-voice memory of the untransposed physical key and otherwise takes the free voice released longest ago; POLY 2 scans linearly from the first voice, chopping old tails; **no voice stealing in either mode** — a seventh simultaneously held key is dropped | Both policies. A key-up makes that slot assignable even while sustain keeps its old sound ringing, matching the assigner's key table rather than treating sustain as another held physical key | These allocation, physical-key and sustain-ownership semantics are **ROM-resolved for the supplied A-5**, not generalized to unidentified revisions. The distinction under transpose and release is asserted behaviorally |
| Assign mode switches | Panel wiring establishes two momentary scan contacts and lamp outputs; hash-identified A-5 behavior latches POLY 1, POLY 2 or both/Unison, with both lamps off not a stable mode | The paired parameters expose those three states. An ordinary click has one contact's meaning; Shift-click explicitly represents pressing both together. Re-pressing the selected virtual button preserves its lamp but repeats the assigner action | Contact topology is **anchored** by the panel circuit. Three-state latch, simultaneous-both handling, accepted-press gate/clear/rescan and power-on fallback are **ROM-resolved for the supplied A-5**. Obsolete both-off plug-in state canonicalises to POLY 1 |
| Solo Unison | Hash-identified A-5 assigns all six slots, makes the highest still-held key win a rescan and gates/rebuilds after key-up; B-2 programs equal pitch counts per slot | Six equal-frequency, unnormalised voices. Physical DCOs continue free-running behind closed VCAs and are not reset together merely because Unison is selected; a genuinely idle voice consumes a changed-pitch reset at its own later converter slot | Assignment/rescan and equal digital pitch are **ROM-resolved for the supplied A-5/B-2 images**. Free-running state and the uncompensated analogue sum are **anchored/derived from topology**. There is no programmed detune or `1/6` gain, and equal counts do not imply forced phase lock; exact sub-pass reset timestamps remain OQ-08 |
| Voice tolerance | The service procedure calibrates each card, but no qualifying repeated six-card/multi-unit data set fixes the residual population or thermal process; the digital envelope generator is shared | The calibrated nominal profile has zero inter-voice spread and zero drift. Existing deterministic seeded offsets/wander are available only as optional `Unit Character` compatibility/sound-design behavior, attached to physical voice slots | Zero nominal is **product policy** in the absence of measurements; the optional distribution remains **voiced** pending OQ-10. A fixed seed must reproduce exactly and Character amount zero must collapse all cards to nominal. Shared CV ownership permits downstream card error but rules out independent envelope laws or six independent sub/noise controls |
| Portamento | Hash-identified B-2 reads an 8-bit raw ADC value; zero is direct/immediate, nonzero selects an 8-bit coefficient by `raw>>1`, and index zero is also immediate. Six 8.8-semitone slot states advance by constant add/subtract and clamp, including while inactive | Raw 0/1 are immediate; paired active raw codes share a coefficient; `octave_passes=ceil(12*256/c)`. The coefficient region `0x0A00–0x0A7F` hashes to `06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e` | Digital mapping, state width, direction, clamp and inactive-slot behavior are **ROM-resolved** for the stated B-2. Pot/ADC voltage, noise/hysteresis, sampling cadence, physical pass timing and revision comparison remain OQ-14 |
| Modulation | Hash-identified B-2 uses one shared free-running triangle magnitude state `0..0x1FFF`, hard endpoint clamps and direction/polarity state. Delay uses the OQ-12 attack increment for hold, then `byte>>4` selects one of eight fade bins whose output is the accumulator high byte | Per rate coefficient `c`, a ramp takes `ceil(8192/c)` passes and a signed cycle takes four ramps. Rate region `0x0C60–0x0D5F` hashes to `4e3d87f7f12202e846d4010b08799dabd4d70d3cb5cffa0566933587538ff1d0`; fade `0x0B30–0x0B3F` hashes to `e145e0e5de512ef77ae0ffb91cefea40263b8200e78ed2a9a81befc13cf8ac99`. Delay byte 0 is three passes total at nominal timing, not bypass | Rate coefficients, integer state/clamp behavior, attack-derived hold and fade bins are **ROM-resolved** for the stated B-2. Physical pass timing/jitter, analogue smoothing/output scale and revision comparison remain OQ-13 |
| Modulation depths | Pitch ±400 cents at full slider, filter ±3.5 octaves, bender pitch ±1 octave; the bender's filter axis maxes at 4064 counts ≈ ±3.6 octaves — the firmware's sensitivity-times-bend arithmetic, which settles a two-source disagreement an earlier revision resolved the other way (±6). The panel's LFO depth and the lever's LFO axis are *summed* by the firmware, so both together reach deeper than either alone | The same, in cents and converter counts; the bender sampled once per pass at 8-bit resolution with no extra smoothing | **Anchored** |
| Chorus lines and mix | Two 256-stage BBD lines, one per output, driven with opposite modulation. Dry is always present; TR11/TR12 (2SK30A/K381) mute the wet returns before R71/R73. TR7/TR8 are later full-output shunts, not chorus mutes | Two asynchronous lines; dry gain `100/39`, wet gain `100/47`, hence wet/dry `39/47` (−1.62 dB). Off retains BBD state and slews wet with a voiced 5 ms exponential time constant (`10–90%` ≈10.99 ms) | Topology, gains and settled dry-only bypass are **anchored**. TR11/TR12 transient/leakage is OQ-20; 5 ms is a labeled plug-in policy. TP3/TP4 are low-frequency modulation points, while BBD delay `128/f_clock` must use one CP phase's repetition frequency |
| Chorus modulation oscillator | Service Notes p. 15: IC1 (µPC062) is an integrator (C3 across IC1b pins 6–7) closed around a Schmitt comparator (IC1a, R6 47 kΩ output→pin 3 against R15 1 kΩ to ground, R7 33 kΩ returning the triangle to pin 2). IC2a inverts once through R10/R9 33 kΩ | A straight, symmetric triangle, and a second line driven by exactly its negative rather than by an independent oscillator | **Derived.** The integrator is fed a constant current for the whole of each half cycle, so both flanks are straight; an RC relaxation oscillator would bend them and this circuit is not one. TP4 is the triangle and TP3 its inverse, which is why the antiphase clocking is a mirror rather than two free modulators. The suite asserts the shape and the endpoints separately |
| Chorus modulation rates | Service Notes p. 15 fixes the *ratio*: the CHORUS I/II line drives JFET Tr1, which shorts R3 2.2 MΩ. With the integrator input at virtual ground the shunt leg and R8 both return to 0 V, so `R_eff = R8·R5/(R_sh ∥ R8) + R8` with R5 1 MΩ, R8 2.2 MΩ, R4 680 kΩ. No calibrated JUNO-106 capture fixes the *scale*, and the schematic does not print C3 | The JUNO-60 pair's geometric mean, re-split by this instrument's own ratio: 0.5222045 Hz for I and 0.8477886 Hz for II. Sweep endpoints remain the JUNO-60's 1.66–5.35 ms | **Ratio derived** — 6.4352941 MΩ over 3.9638889 MΩ is 1.6234799, this instrument's own; mode I is the slower leg. **Absolute scale still voiced from a sibling.** Both results round to the manual's published about-0.5 and about-0.8. The sibling's own 1.682 ratio is superseded and must not be reintroduced. `β = R15/(R15+R6) = 1/48` is known, so C3, one TP4 period or the TP4 amplitude each closes the scale (OQ-01) |
| Chorus delay-sweep law | Service Notes p. 15 shows each MN3101 driven by a transistor voltage-to-current converter (Tr22, R133 2.2 kΩ / R134 22 kΩ / R135 1.8 kΩ, C53 150 pF) | Delay arithmetic in seconds, `centre ± sweep·tri`, then `clock = 128/delay` | **An explicit assumption, not a measurement.** It is correct only if the clock oscillator's *period* is linear in its control voltage; the current-source bias above is the shape of a frequency-linear oscillator, which would make the sweep hyperbolic. Period-linear, frequency-linear and exponential clocks share identical minimum and maximum delays, so only a clock time-series across a full modulation cycle can discriminate them. Requested in OQ-01 |
| Chorus modes | The owner's manual states that I and II cannot be used simultaneously; the board has one chorus-enable line and one binary I/II line, and the patch format stores the same two bits | Exactly three rendered states: Off, I and II, with mutually exclusive panel buttons | **Anchored.** `OneTwo` survives only as an input-compatibility enum for early plug-in sessions. It canonicalises to II and never selects a fourth rate. No parallel-resistor or JUNO-60 both-buttons mode is inferred for the JUNO-106 |
| Chorus nonlinearity | MN3009 typical distortion is 0.3% at 0.78 Vrms and 2.5% at its 1.5 Vrms input-swing point; the bias window implies an asymptote near 2.9 V at the modelled node | A generalized algebraic soft clip fitted jointly to both datasheet distortion anchors, with a 2.924 V asymptote. It remains substantially straighter below overload than a plain `tanh`, then bends rapidly near the part's window | **Datasheet-fitted.** A plain `tanh` at the same asymptote produced about 1.2% at the 0.78 Vrms test point and therefore coloured normal wet levels too strongly. The surrounding ±15 V op-amps stay linear while the BBD write bends, so hot drive grits wet without equivalently clipping dry |
| Chorus charge transfer | The MN3009 datasheet specifies −3 dB at 12 kHz on a 40 kHz clock for the complete part, including its rectangular held output | The explicit BBD output hold already supplies `sinc(12/40) = −1.326 dB`; a clock-rate one-pole supplies only the residual −1.674 dB, using update coefficient 0.8654743 | **Derived from the datasheet anchor without double-counting the existing zero-order hold.** Applying a second full −3 dB loss on top of that hold makes the model too dark |
| Chorus noise | No compander anywhere in the circuit; no calibrated hardware SNR or stereo-correlation measurement has been located | A provisional independent per-line floor plus separately parameterized common/correlated random, hum and clock-spur hypotheses. The optional latter components default to zero; the one Chorus Noise extension scales every component and can defeat them all | **Voiced.** The closest located capture reports noise level alone, with no reference tone, calibrated level, weighting, bandwidth or cross-spectrum, so it cannot establish the amplitudes, spectra or correlation. A calibrated same-path stereo capture is needed before enabling or anchoring the optional terms |
| Chorus support and coupling filters | Service-note component values show two emitter-follower Sallen-Key low-pass sections before and after each BBD, an extra passive input pole, a wet-input coupling high-pass, an output tap-summing pole, and C28/C25 wet-output coupling into the mute/summer loads | Two Sallen-Key sections at 9.69 kHz/Q 0.549 and 10.38 kHz/Q 1.291 on each side; R122 10 kΩ with C52 2.2 nF gives the 7.23 kHz input pole; C44/C47 0.1 µF with R120/R114 100 kΩ gives the 15.9 Hz wet-input high-pass; `(3.3 kΩ || 47 kΩ) × 2.2 nF` gives a nominal 23.46 kHz tap-summing low-pass. With TR11/TR12 open (wet muted), C28/C25 see 22 kΩ, nominally 7.234 Hz; conducting puts R71/R73 47 kΩ in parallel, nominally 10.621 Hz | The component topology and two low-frequency output loads are **anchored/derived** at ideal-source boundaries. MN3009 output impedance and emitter-follower source impedance remain OQ-04; TR11/TR12 on-resistance, leakage and switching remain OQ-20. The 23.46 kHz pole is explicitly **provisional** because it assumes an ideal active MN3009 output |
| Oversampling | Standard practice for nonlinear audio | The complete voice, filter, amplifier and both delay lines run at 4x for host rates below 88.2 kHz, 2x below 176.4 kHz, and natively above, followed by a 63-tap Blackman-Harris half-band per stage. Filter/VCA audio coefficients update at every internal sample, so their wall-clock bandwidth does not change with HQ. A requested live rate change waits for voices and musical tails, then a block-size-independent 5 ms fade brackets rebuilding sample-grid histories. Converter/LFO/DCO phases, BBD buckets/clock/RNG state and C14/C12/C17/C20 coupling states survive; OTA carries are retimed, while chorus support-filter carries that embed the old timestep clear at zero gain | Genuine internal oversampling with filtered decimation, not a quality label. Reported latency is the deepest path's real group delay and shallower modes are padded to keep it constant. With oversampling off, the delay lines' clock-rate images fold with only the reconstruction pole to soften them — a documented cost of that setting. The transition fade and selective support reset are click-prevention product policies, not reference-unit behavior |
| Output stage | Service-note signal order: voice VCAs, 0.1-per-voice summer, C14, shared HPF, C12/common VCA LEVEL and chorus/final summer IC6, identified on p. 15 as TA75558S. Each IC6 output then crosses C17/C20 10 µF and R54/R57 1.5 kΩ into one 10 kΩ track of the dual VR1 VOLUME control, marked `10KB×2`. Each wiper sees the complete 41.3 kΩ selector ladder in parallel with the 101 kΩ IC7/headphone input, or 29.313 kΩ, before any external load | C17/C20, R54/R57 and the nominal-linear tracks run as one position-dependent host-rate network. For shaft position `x`, `Z=(10kx)||29.313k` and `Vw/VIC6=Z/[1.5k+10k(1−x)+Z]`; gain is 0.39655 at half and 0.83252 at full (normalized midpoint 0.4763), while the same resistance moves the coupling corner. A 5 ms shaft glide prevents automation zippering. The fixed pre-jack High-tap product boundary then applies `digital=analogue*10^(-18/20)/Vref_rms`, permits floating samples beyond `+/-1`, and adds no limiter | IC6 identity, placement, named parts, fixed internal loading and linear transfer are **anchored/derived**. Panasonic's later JIS/EIAJ table maps plain B to the nominal-linear 1B resistance law, replacing the unsupported squared taper. A TA75558S identity and rail labels do not establish its loaded in-circuit swing. Real dual-gang tracking/tolerance, selected-tap loading, R64/R65, C21/C22, jack normaling, one-versus-two-plug transfer, external loads, loaded IC6 clipping (OQ-05), absolute `Vref_rms` and driven headphone output remain open |
| Velocity and MIDI modulation | The keyboard is not velocity sensitive and Note On uses a fixed value; the owner's MIDI implementation chart recognizes CC 1 Modulation and CC 64 Hold | Velocity is an explicit extension defaulting to zero. CC 1 drives the bender lever's forward/LFO modulation axis and is scaled by the panel BENDER LFO depth, matching the documented MIDI path | Velocity response is a plug-in extension inert at zero; CC 1/64 reception is **anchored**, not an extension |

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
| Switch byte 17 | bit 0 PWM manual, bit 1 negative VCF-envelope polarity, bit 2 VCA gate, bits 3-4 high-pass position **counting down** | **Anchored.** The polarity/VCA order and reversed high-pass field are asserted bit by bit. Factory gate tones A86, B31 and B82 are semantic fixtures too, because an internally self-consistent but swapped reader/writer would otherwise round-trip perfectly while decoding those sounds nearly silent |
| What a patch does not carry | Volume, the bender depths, portamento and the assign mode | **Anchored.** These are performance controls on this instrument, so loading a patch deliberately leaves them where the player set them |
| Legacy I+II | No hardware state or encoding exists | Old plug-in sessions may still deserialize the obsolete compatibility value. It canonicalises to II; current panel, factory patches and outgoing messages use only Off/I/II |
| Malformed range bits | The three range bits are one-hot in practice but nothing enforces it | A message asserting none or several resolves to the middle range rather than being rejected: it is still a message that arrived |

The factory bank in `Source/DSP/YouKnow106Presets.cpp` is a byte-exact
transcription of all 128 historical tone-memory states in A11..A88/B11..B88
order. Its concatenated 2,304-byte payload has SHA-256
`394ae874da33aa63fa4833932fbf415546d2ad66b1b6b9a36315601799eeec21`.
It was mechanically decoded and cross-checked with zero mismatches across the
public Hinzen tape/PAT archive, the Jarvik7 librarian library and the KR-106
archival transcription. Tests assert a dependency-free FNV-1a lock over the
complete corpus, the slot sequence and the hardware-format round-trip. The
hardware stores no names; displayed names are conventional archival metadata.
Product/host recall restores a complete plug-in snapshot around each tone,
including explicit unison/octave playing directions in the archival labels,
while hardware Program Change and SysEx retain the hardware's narrower
tone-memory semantics.

## Implemented signal path

The authoritative implementation is `Source/DSP/YouKnow106Engine.cpp` and
`Source/DSP/YouKnow106Chorus.cpp`:

1. A key press allocates a voice under the assigner ROM's selected policy, or
   is dropped if every voice's physical key is still held. Changing POLY mode
   gates and rebuilds assignments from the retained held-key table. In Solo
   Unison, any key-up performs the same rebuild and the highest remaining key
   wins.
2. A fractional scheduler starts each nominal 4.2 ms pass without truncating
   its average period to a whole host sample. It advances the shared B-2
   LFO/delay state and executes the recovered 23-write converter queue in
   ordinal order: RESONANCE/VCA LEVEL/SUB, DCO 1–6, PWM, paired VCF/VCA 1–6,
   then NOISE. The B-2 envelope recurrence and 8.8 portamento state advance on
   each card's DCO write. The default normalized compatibility profile spreads
   ordinal events monotonically across the pass; its offsets are not claimed as
   measured timestamps, and a phase-zero diagnostic profile remains available.
3. Per oversampled sample, held voltages slew on separately named constants.
   The VCF family uses the supported 522 µs value and voice ENV/GATE VCA uses
   687 µs; the currently equal DCO/shared-node values are isolated compatibility
   defaults, not evidence that those circuits share a time constant. The
   calibrated nominal adds no card spread or thermal drift; any seeded
   per-card residuals are explicitly the optional `Unit Character` profile.
4. Every physical DCO advances by `1/period`, including slots whose VCAs are
   closed. Its straight ramp's two reset corners are repaired by slope
   residuals; the comparator and the
   divide-by-two produce step residuals, the sub's edge at the reset's
   start. The ramp and pulse carry the compensation voltage's momentary
   amplitude error; saw, pulse, sub and the shared noise sum at the mixing
   node in volts.
5. In each active voice, the 122x input attenuator and the named voiced
   resonance compatibility profile feed the four transconductor filter stages;
   its circuit-shaped nonlinear return keeps the model bounded. None of that
   profile's analogue coefficients is promoted beyond OQ-09's verified
   byte/DAC boundary. ENV/GATE then drives that voice module's quasi-linear
   VCA.
6. The six voice outputs enter IC1a through 33 kΩ against 3.3 kΩ feedback, so
   each contributes `0.1` to the bus. C14/R39 AC-couples that sum before one
   shared switched high-pass; C12/R36 then AC-couples the selected result into
   the common uPC1252H2 controlled by stored VCA LEVEL.
7. The wet path passes its 15.9 Hz coupling high-pass, both delay lines clocked
   with opposite modulation, the datasheet-fitted write nonlinearity, the
   zero-order hold plus residual charge-transfer loss, and the reconstruction
   chain including its provisional 23.46 kHz tap-summing pole. C28/C25 then
   supply the switch-loaded 7.23/10.62 Hz wet-output coupling. Each output
   channel then sums dry at `100/39` with its own wet line at `100/47` in IC6.
   Bypass mutes the wet return but leaves both lines running. No unmeasured
   low-voltage rail is synthesized after this ±15 V stage.
8. Half-band decimation to the host rate, then one independent C17/C20 charge
   state per channel. Its 10 µF, 1.5 kΩ, nominal-linear dual 10KB track and
   fixed 29.313 kΩ wiper load form one position-dependent network — the one
   true potentiometer in the audio path — followed once by the declared
   -18 dBFS RMS/`Vref_rms` product mapping. That boundary allows floating output
   beyond `+/-1` and does not feed back into circuit drive.

## Deterministic Physical Circuit Behaviors (2026-08-05)

The engine incorporates the deterministic physical circuit behaviors below,
operating without macro ad-hoc randomness.

These entries describe *mechanisms*, and they are not a separate evidence
class from the claims table above. Each mechanism's topology and device
identity are anchored or derived from the service notes and the part
datasheets; the **magnitudes** are voiced except where a datasheet fixes them,
because no calibrated JUNO-106 measurement of any of them has been located.
Each is gated by its own named toggle, and most are additionally scaled by
the single Unit Character control -- which covers both component tolerance
(trimmer residual, thermal wander) and the circuit's own inherent non-linear
shapes (ramp curvature, the chorus clock laws, the Early effect and so on) --
so the calibrated nominal model at zero contains none of them. Unit
Character's host-facing range continues past its "matches real hardware"
reference of 1.0, up to 100, purely to exaggerate every mechanism for audible
contrast; see the note at the end of this section. Where a mechanism has an
open question, it is named in its entry.

1. **VCF Thermal Warmup & OTA Transconductance ($V_t(T)$)**:
   Models thermal dissipation warming voice cards from $25^\circ\text{C}$ to $40^\circ\text{C}$ ($T(t) = 25 + 15(1 - e^{-t/900})$). Thermal voltage $V_t(T) = \frac{k T}{q}$ scales transconductance headroom $2 V_t / \text{attenuation}$, naturally softening resonance and broadening linear differential headroom over a 15-minute warmup curve.

2. **VCA Control Voltage Feedthrough (Envelope "Thump")**:
   BA662 / uPC1252H2 differential pair transistor $V_{be}$ mismatch introduces additive control voltage leakage ($k_{cv\_leak} \cdot V_{vcaControl}$) into the audio node. Fast envelope attacks produce an authentic physical low-frequency "thump" on percussive transients.

3. **Power Supply Rail Droop & Inter-Voice Coupling**:
   Active polyphonic voice current draw loads the $\pm 15\text{V}$ linear voltage regulators, inducing DC rail droop ($\Delta V_{rail} \propto \sum I_{voice}$). Rail droop modulates the VCF cutoff reference across all active cards, creating organic inter-voice glue under heavy polyphonic loading. The load follower runs at one wall-clock rate, so the quality setting cannot change how fast the supply responds.

   It does **not** modulate DCO tuning, and must not be made to: pitch is an integer division of a crystal-derived clock, so no rail deviation can move it. See the Note timer row above.

   Mains ripple is deliberately **not** modelled, and this is a derived result rather than an unmeasured gap. Service Notes p. 16 gives a $3300\,\mu\text{F}/35\text{V}$ reservoir per rail behind a 1B4B41 bridge on a $19\text{ V}_{rms}$ / $0.25\text{ A}$ secondary, so the unregulated ripple is $I/(2 f C) \approx 0.76\text{ V}_{pp}$ at 50 Hz and $0.63\text{ V}_{pp}$ at 60 Hz at full rated load. The M5230L regulators that follow reject roughly 60 dB of it, leaving about $0.7\text{ mV}_{pp}$, or 50 ppm of 15 V, at a card. Through the modelled 35 counts/V cutoff transfer that is 0.03 cents; as amplitude modulation of the DCO ramp it is $-86\text{ dB}$ sidebands. Modelling it would be modelling nothing.

4. **Thévenin Passive Voice Mixer Loading**:
   Waveform summing (Saw 100k, Pulse 100k, Sub 100k, Noise 100k) into the IR3109 input ($68\text{ k}\Omega$) is modeled as a Thévenin admittance node ($G_{total} = G_{in} + \sum G_{leg}$). Engaging multiple waveform switches increases node admittance, loading down signal amplitudes realistically.

5. **BBD Dynamic Charge Transfer Loss & Clock Feedthrough**:
   MN3009 bucket-brigade stage-to-stage charge transfer loss ($\alpha_{loss}$) scales dynamically with BBD clock frequency $f_{clk}$, so the line's top end moves with the sweep instead of sitting at one fixed corner.

   A phase-coherent clock-feedthrough spur is *parameterised* but ships at zero amplitude, along with the other unmeasured chorus-noise components, pending OQ-03: which harmonic dominates and at what level is not established, and the audible part of it — the clock's intermodulation with the signal — is already produced by the explicit output sample-and-hold. Only the dynamic transfer loss above is live.

6. **VCF Cutoff Anti-Log Emitter Resistance ($R_e$) Compression**:
   Anti-log transistor parasitic emitter resistance ($R_e \approx 8\,\Omega$) causes an $I_{abc} R_e$ voltage drop at high cutoff control currents ($f_c > 8\text{ kHz}$), softly compressing extreme top-end filter sweeps ($\text{rawHz} / (1 + \text{calibration} \cdot (\text{rawHz} / 120000))$) to prevent digital harshness.

7. **TA75558S IC6 Output Summer Rail Bound**:
   The output summer runs on $\pm 15\text{V}$ rails and cannot drive past them. Modelled with the same generalized algebraic clip as the BBD write, $y = x/(1+|x/L|^n)^{1/n}$, at $L = 13.5\text{ V}$ and $n = 8$: numerically linear through the few volts the stage actually carries, bending only as it approaches the rail.

   Unlike the tolerance mechanisms, this is **not** scaled by Unit Character. A freshly calibrated instrument has exactly the same rails, so a "pristine reference" whose output stage could swing to infinity would be the less faithful model. It replaces a $\tanh$ at the same asymptote that was applied *only* when Unit Character was above zero, which had two separate problems. A $\tanh$ has no linear region — its distortion rises as $(V/L)^2$ from the first millivolt, putting roughly 0.3% third harmonic on every sample at an ordinary 2.6 V node swing, where a TA75558S is specified far below that. And because the reference model skipped it, Solo Unison rendered a 15.7 V peak out of an op-amp on 15 V rails; it now stops at 11.7 V. `Tests/YouKnow106CircuitTests.cpp::testOutputSummerIsLinearBelowItsRails` asserts both halves: third harmonic below 0.05% at the nominal coordinate, and no input of any size escaping the rail.

   The *existence* of the bound is anchored by the supply rails. Its exact value remains OQ-05: 13.5 V is the rails less a typical saturation voltage, not a loaded measurement, and the downstream digital boundary still adds no limiter of its own.

8. **TR11/TR12 2SK30A wet-mute switches — no modelled distortion, by derivation**:
   Conducting, a 2SK30A's few hundred ohms sit against IC6's 47 k$\Omega$ wet input, so it drops about 1% of the signal and sees some 27 mV across itself at full level. Ohmic-region channel resistance moves by roughly $V_{ds}/2|V_p - V_{gs}|$, about 0.7%, and that reaches the output only through the same 1% divider — so the distortion is on the order of 0.007%, or $-83$ dBc.

   A revision modelled $1 - 0.015\tanh(v^2)$ instead: 1.1%, or $-39$ dBc, applied to every wet sample. That is some 44 dB more than the part can produce, and it was always on rather than only during switching. Both it and its companion gate-injection placeholder are removed. The switching transient and leakage remain **OQ-20** and are deliberately not invented; the 5 ms wet-mute glide is declared plug-in declick policy, not a device measurement.

9. **CMOS 4013 Sub-Oscillator P/N Driver Asymmetry**:
   CMOS 4013 flip-flop P/N channel driver impedance asymmetry ($t_r \approx 25\text{ ns}$ vs $t_f \approx 15\text{ ns}$) introduces subtle duty cycle asymmetry ($\approx 49.8\% / 50.2\%$) and odd-harmonic character into the sub-oscillator square wave.

10. **Sample-and-Hold Capacitive Charge Leakage & Re-strike Key Click**:
    Re-striking an active voice card before its release envelope finishes causes residual charge leakage on hold capacitor $C_{hold}$ through switch resistance $R_{on}$, producing an authentic analog legato key-click transient.

11. **IR3109 VCF Stage-Space Transistor Input Offset Voltages ($V_{os}$)**:
    Differential pair BJT transistor input offset voltages ($V_{os} \approx 1.5\,\text{mV}$) across the 4 IR3109 transconductor stages break 4-pole differential symmetry, creating stage-dependent dynamic DC shifts and asymmetric soft distortion under high-resonance filter sweeps.

12. **TA75558S IC6 Output Summer Op-Amp Dynamic Slew-Rate Limiting**:
    Dual op-amp TA75558S finite maximum slew rate ($\text{SR} \approx 1.7\,\text{V}/\mu\text{s}$) imposes a dynamic rate-of-change limit ($\Delta V_{\text{max}} = \text{SR} \cdot \Delta t$) on output signals, naturally rounding off extreme high-frequency transients and resonant spikes to eliminate digital harshness.

13. **MN3009 BBD storage-capacitance non-linearity — removed, as double-counting**:
    The MN3009's P-channel storage capacitance really is voltage dependent, $C_{gs}(v) = C_0/\sqrt{1 + |v|/V_{\text{bi}}}$. What that produces at the terminals is distortion plus a little level-dependent high-frequency loss — and the datasheet's distortion figures (0.3% at 0.78 V$_{\text{rms}}$, 2.5% at 1.5 V$_{\text{rms}}$) are measurements of the complete part, with that non-linearity already in them. `Chorus::bbdTransfer` is fitted jointly to both. A separate $C_{gs}$ term on top counts the same physics twice.

    The implementation was also the wrong mechanism entirely: it scaled *both* delay lines by $1 - 0.015\,|v|/(2.6 + |v|)$, frequency-modulating the whole line with the rectified instantaneous input. That is ~14.6 µs of delay deviation on a 3.505 ms centre at full level, giving wet-path sidebands that rise 6 dB/octave — about $-27$ dBc at 1 kHz and $-13$ dBc at 5 kHz. Nothing in a bucket-brigade device moves the clock with the signal. (The units were wrong twice over as well: the input arrives in the 2.6 V-per-unit coordinate and was divided by 2.6 again.)

    Removing it changes the wet path by $-15.8$ dBc peak and leaves every other listening take at the measurement floor.

14. **CMOS CD4051 Multiplexer Gate-Drain Capacitive Crosstalk ($C_{gd}$) & Charge Injection**:
    CD4051 8-channel analog multiplexer channel switches exhibit parasitic gate-drain capacitance ($C_{gd} \approx 6\,\text{pF}$), injecting micro-charge pulses ($\Delta Q = C_{gd} \cdot \Delta V_{\text{dac}}$) into hold capacitors during multiplexer scan transitions to model organic control chatter.

15. **JFET DCO Ramp Reset Exponential RC Discharge ($R_{\text{DS,on}} C$)**:
    Replaces the linear reset phase approximation with physical JFET discharge curve $V_{\text{ramp}}(t) = V_{\text{peak}} e^{-t / \tau_{\text{reset}}}$ ($\tau_{\text{reset}} \approx 0.55\,\mu\text{s}$), softening the bottom reset corner into $-1.0$ and smoothing high-frequency harmonic reset artifacts. Rendered before/after comparison WAVs and isolated difference files are committed in [`05-exponential-dco-reset-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/05-exponential-dco-reset-before.wav), [`05-exponential-dco-reset-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/05-exponential-dco-reset-after.wav), and [`05-exponential-dco-reset-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/05-exponential-dco-reset-diff.wav).

16. **IR3109 VCF Transistor Early Effect Modulation ($V_A$)**:
    Models transistor Early Voltage ($V_A \approx 100\,\text{V}$) transconductance modulation $g_n = g\,(1 + 0.005\,\tanh(V_n / V_{\text{headroom}}))$ inside the 4-stage OTA cascade solver, introducing a small signal-dependent cutoff shift and odd-harmonic content under hot resonant sweeps. With $V_A \sim 100\,\text{V}$ and a few hundred millivolts of collector swing at the differential pair, the fractional change in $g$ is a few parts per thousand; the coefficient is now a named constant beside $V_A$ so the two cannot drift apart.

    A revision used 0.08 -- sixteen times the stated figure, and a signal-dependent cutoff shift large enough to hear as grit on every resonant sweep. Correcting it drops the isolated comparison render from $-16.6$ to $-40.8$ dBc peak, the 24 dB the ratio predicts. Rendered before/after comparison WAVs and isolated difference files are committed in [`06-vcf-early-effect-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-before.wav), [`06-vcf-early-effect-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-after.wav), and [`06-vcf-early-effect-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-diff.wav).

17. **Voice Cards Spatial Chassis Thermal Gradient ($\Delta T_{\text{psu}}$)**:
    Models spatial heat dissipation across physical voice cards 1–6 based on physical proximity to the internal power supply transformer ($T_{\text{card}}(i) = 25^\circ\text{C} + \Delta T_{\text{ambient}}(t) + 4^\circ\text{C} e^{-(i-1)/2.5}$), introducing per-voice thermal headroom variation under polyphonic playing.

    It reaches the OTA's linear span and the cutoff reference. It does **not**
    reach pitch. A revision multiplied each card's oscillator phase increment by
    a per-card thermal factor, spreading the six cards over 13 cents — audible
    as beating on every chord and, in Solo Unison, as a detune the instrument
    has no mechanism to produce. See "Tuning stability" below for the
    derivation. Rendered before/after comparison WAVs and isolated difference files are committed in [`07-spatial-thermal-gradient-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-before.wav), [`07-spatial-thermal-gradient-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-after.wav), and [`07-spatial-thermal-gradient-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-diff.wav).

18. **Chorus Heterodyne Clock Bleed**:
    Dual MN3009 BBD clock driver heterodyne beat frequency sidebands ($f_{\text{clkA}}, f_{\text{clkB}} \in [40\,\text{kHz}, 200\,\text{kHz}]$), injecting a small high-frequency tone into wet chorus modes. Off by default: the tone's amplitude is an unvalidated placeholder pending OQ-03, and no calibrated hardware noise reference has been located. (An earlier revision of this entry additionally claimed continuous-time fractional-delay/Thiran interpolation for the BBD taps; no such filter exists in the code -- the line still uses linear interpolation. Implementing a genuine Thiran allpass for the BBD read/write taps remains open future work.) Rendered before/after comparison WAVs and isolated difference files are committed in [`08-chorus-thiran-clock-bleed-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-before.wav), [`08-chorus-thiran-clock-bleed-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-after.wav), and [`08-chorus-thiran-clock-bleed-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-diff.wav).

19. **Chorus MN3101 Current-Controlled Oscillator Hyperbolic Delay Sweep**:
    Models Tr22 control current modulation into MN3101 clock driver ($f_{\text{clk}} \propto I_{\text{ctrl}}$), yielding a physical hyperbolic delay sweep that replaces ideal linear delay modulation with asymmetric pitch Doppler shifts. The clock, not the delay, is what is linear in the modulating triangle: the clock sweeps linearly between the two frequencies that correspond to the measured delay envelope's endpoints ($128 / (T_{\text{centre}} + \text{sweep})$ and $128 / (T_{\text{centre}} - \text{sweep})$), so the rendered sweep reaches exactly the measured endpoints at any amount of Unit Character rather than overshooting them (see OQ-01, which records the overshoot an earlier, centre-relative revision of this formula produced). Rendered before/after comparison WAVs and isolated difference files are committed in [`09-chorus-hyperbolic-sweep-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-before.wav), [`09-chorus-hyperbolic-sweep-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-after.wav), and [`09-chorus-hyperbolic-sweep-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-diff.wav).

20. **DCO Integrator Finite Source Resistance Ramp Charging Curvature**:
    Models the finite output resistance ($R_{\text{out}} \approx 500\,\text{k}\Omega$) of the constant-current source charging the $1000\,\text{pF}$ ramp capacitor, causing low-frequency ramp charging to exhibit a subtle exponential curvature ($v(u) \approx (2u-1) - \beta u(1-u)$) that adds warm 2nd-harmonic weight on low bass notes. Rendered before/after comparison WAVs and isolated difference files are committed in [`10-dco-ramp-curvature-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/10-dco-ramp-curvature-before.wav), [`10-dco-ramp-curvature-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/10-dco-ramp-curvature-after.wav), and [`10-dco-ramp-curvature-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/10-dco-ramp-curvature-diff.wav).

21. **C14 Non-Polar Electrolytic Voltage-Dependent HPF Modulation**:
    Models non-linear capacitance variation $C(v) = C_0 / (1 + \alpha |v|)$ across voice-summing coupling capacitor C14 ($10\,\mu\text{F}$ non-polar electrolytic), allowing large low-frequency sub-bass voltage swings to dynamically shift the HPF cutoff corner and generate natural intermodulation "glue". Rendered before/after comparison WAVs and isolated difference files are committed in [`11-electrolytic-c14-nonlinearity-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-before.wav), [`11-electrolytic-c14-nonlinearity-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-after.wav), and [`11-electrolytic-c14-nonlinearity-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-diff.wav).

22. **µPD7541 12-Bit R-2R DAC Major Carrier Glitch Impulse**:
    Models 12-bit R-2R DAC switch-timing skew during major bit transitions ($011111111111_2 \leftrightarrow 100000000000_2$), injecting transient voltage glitch impulses into hold capacitors to provide authentic physical "zipper texture" during continuous manual filter sweeps. Rendered before/after comparison WAVs and isolated difference files are committed in [`12-dac-glitch-impulse-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/12-dac-glitch-impulse-before.wav), [`12-dac-glitch-impulse-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/12-dac-glitch-impulse-after.wav), and [`12-dac-glitch-impulse-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/12-dac-glitch-impulse-diff.wav).

*Note: All physical circuit simulation behaviors above scale dynamically with **Unit Character** (`calibration`): `0.0` suppresses every one of them for a pristine digital reference, `1.0` matches real hardware, and the host parameter's own range continues to `100.0` -- skewed so 0-1 still covers half the knob's travel -- for the same exaggerated-for-contrast territory the comparison-rendering tools in this repository use.*

### Tuning stability: why no thermal or long-term pitch drift is modelled

This is a derived result, like the mains-ripple entry above, rather than an
unmeasured gap.

Pitch is $f = f_{\text{clock}} / N$ for an integer $N$ held in a counter, with
one 8 MHz reference feeding all six note timers. Three consequences follow, and
none of them is a modelling choice:

- **No inter-voice detune is possible.** Whatever the reference does, it does to
  all six cards in the same instant and in the same proportion. Temperature,
  supply droop, ageing and component tolerance have no term in an integer
  division. Six voices on one key are exactly in tune with each other, always.
  `Tests/YouKnow106EngineTests.cpp::testUnisonDoesNotBeat` asserts it directly,
  at Unit Character 0 and 1, by measuring the summed level of a six-voice unison
  stack across three seconds.
- **Global drift is below audibility.** The reference is a crystal. One cent is
  578 ppm; an AT-cut crystal's entire error budget — initial tolerance, its
  temperature curve across the operating range, and ageing — lands inside about
  $\pm 100$ ppm, or $\pm 0.17$ cents. That is a fifth of the instrument's own
  pitch-quantisation step at A4 and an order of magnitude below the
  $\sim 0.9$ cent step near the top of the keyboard, so it is not merely
  inaudible, it is smaller than the grid the pitch already sits on. Modelling
  it would be modelling nothing.
- **There is no analogue pitch control chain to drift.** The one analogue
  voltage in the oscillator, the ramp compensation, sets the integrator's
  charging current and therefore the ramp's *amplitude*. It does not set the
  period — the timer does. Its slew after a pitch step is modelled, and it is an
  amplitude transient, not a pitch transient.

What a real unit *does* audibly do as it warms is move its **filter**, not its
oscillators: $V_t = kT/q$ scales the transconductor's linear span with absolute
temperature, so the cutoff reference and the onset of the OTA's compression both
shift over the warm-up. That is where the modelled thermal behaviour lives.

If tuning instability is ever wanted as a *musical* effect it has to be an
explicitly labelled non-hardware extension, like the velocity and voice-count
extensions, and must not be attributed to the reference instrument.

### Measured weight of each mechanism

The comparison renders referenced above are produced by `Tools/RenderSotaComparisons.cpp`.
Until 2026-08-06 every one of those takes toggled `calibration` between 0.0 and
1.0 *in addition to* its own named flag, so each "after" take had all twelve
mechanisms on and each "before" had all twelve off: no file isolated the feature
it was named after, and every difference file was separately peak-normalised,
which discarded its magnitude. Both are fixed. Each pair now toggles exactly one
flag with `Unit Character` held at its default, the before/after pair shares one
gain so it is level-matched for listening, and the difference file carries that
same gain so its loudness is its true loudness.

The measured result is recorded in
[`Docs/audio/sota-comparisons/README.md`](audio/sota-comparisons/README.md) and
regenerated on every run. It is evidence about this model, not about the
hardware: it says how much each mechanism changes *this* renderer's output on
*that* mechanism's own demonstration patch, which is the question of whether a
listener could ever notice it. Two entries measure as bit-identical output, and
one measures louder than the programme it rides on; those results are addressed
in the rows above rather than left implied.

## What remains open

The canonical research queue is
[open questions](open-questions.md). Each entry contains a standalone task
definition, the exact output expected from an LLM or hardware researcher, the
current assumption being tested and a shared evidence contract. “Not found”
and “measurement required” are valid results; a guessed value is not.

| Area | Canonical task |
| --- | --- |
| Absolute JUNO-106 chorus timing | OQ-01 |
| Stored VCA LEVEL byte/DAC/hold-network-to-GC1 law and installed endpoints | OQ-02 |
| Calibrated chorus noise and SNR | OQ-03 |
| Loaded post-BBD support-chain transfer | OQ-04 |
| Loaded TA75558S IC6/High-output clipping swing | OQ-05 |
| Absolute `Vref_rms` calibration under the adopted -18 dBFS RMS convention | OQ-06 |
| Converter hold topology and time constants | OQ-07 |
| Exact intra-pass timing and DCO pitch-write restart | OQ-08 |
| Resonance byte-to-loop-gain law | OQ-09 |
| Measurements capable of replacing the zero-spread/zero-drift nominal policy | OQ-10 |
| Pulse-off pinned-leg mixer behaviour | OQ-11 |
| Envelope physical timing, audible thresholds and firmware-revision scope | OQ-12 |
| LFO/delay physical timing, analogue transfer and firmware-revision scope | OQ-13 |
| Portamento pot/ADC behavior, timing and firmware-revision scope | OQ-14 |
| Oscillator-mixer levels and filter-drive calibration | OQ-15 |
| Main noise spectrum and filter self-oscillation startup | OQ-16 |
| Main VOLUME gang tracking and loaded selector/jack transfer | OQ-17 |
| Hardware cutoff-converter saturation behind the transparent 50 kHz product cap | OQ-18 |
| Measured central voice-module BA662 gain, knee and possible deadband | OQ-19 |
| Chorus wet-mute switching transient and leakage | OQ-20 |
| Coupled C14/switched-HPF transfer and mode-change memory | OQ-21 |

JUNO-60 findings may be retained only as labelled comparative evidence. They
cannot close a JUNO-106 task.

## Sources

Values were gathered from the instrument's service notes and owner's manual;
component datasheets for the delay line, its clock driver and NEC µPC1252H2;
the photographed
[A1QH80017A teardown](https://obsoletetechnology.wordpress.com/projects/80017a-vcfvca-teardown/);
published clean-room reverse engineering of the assigner ROM; exact behavioral analysis
of the supplied, hash-identified B-2 image for OQ-12 through OQ-14;
provenance-pending leads from an explicitly unofficial/partial voice-processor
disassembly; the published analysis of this DCO's charge circuit; the
sibling instrument's chorus measurements; and the virtual-analog literature.
The modelling techniques are standard and separately cited: Zavalishin's
topology-preserving transforms for the integrator prewarp and the `1/(4 − k)`
four-pole result; Stilson and Smith on the ladder's root locus; Huovilainen
and D'Angelo and Välimäki on nonlinear ladder solutions; Välimäki, Pekonen and
Nam on integrated-B-spline BLEP residual tables built at 64x with a Blackman
window; and Holters and Parker on bucket-brigade device modelling. No
third-party source code, netlist, ROM image, firmware or recording is included
in this repository. The one external functional-data corpus is the independently
decoded 2,304-byte factory tone memory described above; laws fitted through
published behavioural anchors are YouKnow106's own constructions.

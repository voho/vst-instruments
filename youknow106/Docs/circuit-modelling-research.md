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
| Ramp bandlimiting | Integrated-B-spline residual tables built by numerically integrating a Blackman-windowed sinc at 64x, as in the LUT-BLEP literature | Both corners of the reset are slope discontinuities, not a jump, so each is repaired with the *slope* residual; the comparator and divider edges get the *step* residual | Alias floor below −55 dB across the keyboard at 44.1 and 48 kHz, asserted by the engine suite. A closed-form polynomial fit was tried first and was *worse than no correction at all* — the tables are integrated, not fitted |
| Pulse and PWM | Comparator against a control voltage: 50% duty at +6 V, 95% at +0.6 V, and Service Notes p. 9 says −0.8 V pins the output high. PWM is one shared hold | Duty derives from the shared slewed threshold against each card's momentarily mis-scaled ramp, retaining per-card comparator offset. Pulse Off writes −0.8 V and pins the modeled comparator high while DCO/sub keep running; the audio contribution remains hard-gated because the local comparator-to-voice-mixer coupling/loading is unmeasured. The final C17/C20 output coupling does not answer that upstream question. The LFO reaches PWM raw, a provenance-pending firmware lead | Duty anchors, shared ownership and the off control/comparator state are **anchored**. Enabled PWM reaches neither rail. Pinned-leg DC/bleed/loading/transient remain OQ-11, so no audible artifact is invented |
| Sub oscillator | A divide-by-two flip-flop clocked by the counter's terminal pulse; Service Notes pp. 8–9 show one shared SUB LEVEL hold controlling its collector-supply amplitude | An exact square one octave below the selected footage, unaffected by pulse width, with edges at reset start; every card consumes the same scanned/slewed 7-bit sub-level voltage before card-specific analogue error | **Anchored** topology, divider relationship, shared ownership and stored path (SysEx byte 15). There is no sub-octave selector. Exact full-scale amplitude and loaded mixer transfer remain OQ-15 |
| Noise | One shared generator and one shared NOISE LEVEL hold feed all voice mixers | A single bounded-uniform white source added to each voice before its filter, scaled by one scanned/slewed 7-bit shared level then card-specific residual error; a separate tiny per-voice excitation starts a silent self-oscillating filter. Both discrete sources scale by `sqrt(internal_rate / 192 kHz)` so their wall-clock spectral density does not change with host rate or HQ mode; 192 kHz is the pre-existing 48 kHz/HQ reference and therefore preserves that sound exactly | **Anchored** shared source/hold topology, control path (SysEx byte 4) and 4.0 Vpp TP8 adjustment. Rate normalization is a numerical/product requirement, not a hardware-amplitude claim. TP8 is downstream and does not establish the model's pre-filter `+/-2 V` coordinate, RMS or distribution; those and startup excitation are **voiced** pending OQ-15/OQ-16 |
| Voice summer and high-pass | IC1a receives every voice through 33 kΩ against 3.3 kΩ feedback, followed by one four-position switched high-pass network on the jack board | The six voices sum at `3.3/33 = 0.1` each before one shared filter. Position 0 is the measured boost shelf: +10.5 dB at DC falling to +1.41 dB in the high band across a corner near 59 Hz. Position 1 passes the band untouched. Positions 2 and 3 are single-pole high-passes at 225.8 Hz and 720.5 Hz from the 47 kΩ pack with C10 15 nF and C11 4.7 nF | **Anchored** gain, placement and cut-leg parts from the service-note schematic. Unity-summing the voices overdrives every downstream common stage by 20 dB; filtering once per voice ahead of its resonant VCF is also a different circuit |
| Filter core | The potted voice module contains a quad-OTA four-pole chip with an on-chip antilog converter; per stage 68 kΩ input resistor, 560 Ω, 240 pF integrator | Four transconductor stages solved together implicitly, `C dVn/dt = Ig tanh((V(n-1) − Vn)/H)` with `H = 2·Vt/attenuation = 6.37 V`, trapezoidally integrated and closed with a damped Newton step whose Jacobian is bidiagonal plus one corner term | **Anchored** topology and component values. The suite checks the model against a fourth-order Runge-Kutta solve of the same ODE at 16x, and both against the closed-form `1/(4 − k)`, to 0.6 dB |
| Filter drive level | The 68 k/560 Ω pair attenuates each stage's differential input by 122x | That attenuator, not a user-facing "drive" control, sets the differential pair's nonlinear span; the engine currently uses `+/-6 V` saw/pulse, `+/-5 V` sub and `+/-2 V` noise coordinates followed by a 0.40 scale | The component attenuator and OTA span are **derived**. A centered `+/-6 V` interpretation is merely compatible with a 12 Vpp reading at the same source node; it does not establish the loaded transfer. The sub/noise coordinates and 0.40 mapping are **voiced compatibility** pending OQ-15 |
| Cutoff control law | Firmware: the panel byte times 128, envelope, modulator, bender and key-follow terms summed in a 14-bit accumulator clamped to [0, 16383], the top 12 bits driving the converter; 5.53 Hz at code 0, 1143 counts per octave; service check of 248 Hz self-oscillation at code 6272. Service Notes p. 1 publishes an approximate 5 Hz–50 kHz range | `f = 5.53 · 2^(counts/1143)` through its established range, followed in the default profile by a transparent numerical `min(..., 50000 Hz)` cap. The digital sum is clamped and truncated to 4-count steps. The former 24 kHz/tanh/52.2 kHz curve may be retained only as a named legacy profile | Count-domain sum, base, octave slope, service point, clamp and truncation are **anchored**. The 50 kHz cap is **product policy**, not a claim about converter saturation. A described 93-point/single-card fallback table lacks the complete raw capture, metadata and population scope needed to resolve OQ-18 |
| Resonance | Hash-identified B-2 behavior forms aligned work word `W=128b` and physical converter code `DAC12=32b` from stored resonance byte `b`; Service Notes pp. 5, 8 and 13 establish one shared IC26-channel-6 hold. No qualifying original-unit sweep located here establishes the subsequent DAC-voltage/current-to-loop-gain transfer | The exact stored-byte conversion and one shared queue write feed a named `VoicedResonanceCompatibilityProfile`. That replaceable profile retains the existing quadratic/linear panel-to-loop curve, circuit-shaped nonlinear return and optional per-card Unit Character residual without changing preset bytes | `b → W → DAC12` is **ROM-resolved** for the identified image and shared ownership is **anchored**. Every numerical analogue step after the DAC — including the current 30%/90%/maximum landmarks, loop limiter and card residual magnitude — is **voiced compatibility**, not a fitted, measured or calibrated hardware law (OQ-09/OQ-10) |
| Resonance compensation | No qualifying raw original-unit transfer or circuit-de-embedded sweep was located for the compensation path independently of filter saturation and output amplitude | The named resonance compatibility profile retains input multiplier `1 + 0.2296·k`, preserving the current high-Q drive character | The direction, coefficient and resulting maximum boost are all **voiced compatibility**. They are not hardware measurement anchors and may be replaced with the rest of OQ-09's analogue profile without changing the verified byte/DAC path |
| Oscillation frequency correction | The service procedure establishes that a per-card adjustment exists, but does not establish the model's feedback-dependent correction curve or its coefficient | The named resonance compatibility profile retains `1 + 0.045·min(k/4,1.2)²` before the explicit 50 kHz product cap | The existence of an adjustment does not validate this equation. Its threshold-scale denominator, 4.5% amount and rendered pitch result are **voiced compatibility/model calibration**, not original-unit transfer evidence |
| Envelope | Hash-identified B-2: one 14-bit state per generated envelope, sustain `S=128b`, saturating additive attack without retrigger reset, and shared decay/release coefficient selection. For `v_hi=v>>8`, `v_lo=v&255`, `c_hi=c>>8`, `c_lo=c&255`, its fall helper is `Q(v,c)=c_hi*v_hi+floor(c_lo*v_hi/256)+floor(c_hi*v_lo/256)`; the low×low term is intentionally omitted | Attack is `min(0x3FFF,E+A[b])`; decay is `S+Q(E-S,c)` when above sustain and otherwise snaps to `S`; release is `Q(E,c)`. The DAC gets `E>>2` while the recurrence retains two low bits. The attack region `0x0B60–0x0C5F` hashes to `faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`; decay/release `0x0D60–0x0E5F` hashes to `0de73bedf11904538056eec3622b09470461f13ad016103ab9992be73e467754` | **ROM-resolved** for the stated B-2 image, including coefficients, rounding, clamp, sustain and retrigger semantics. OQ-12 now concerns hardware pass timing/jitter, analogue-node/audible thresholds, independent behavioral confirmation and other revisions, not recovering these tables |
| Voice-module VCA | Each voice module has its own OTA, controlled only by the ENV/GATE selection and its envelope control voltage | `VoicedVoiceVcaCompatibilityProfile` retains the current quasi-linear law, 0.12 knee, 260 dB-per-unit low-level slope and 0.005 hard-zero rule. Velocity is an optional plug-in extension applied here | Topology and ENV/GATE ownership are **anchored**. The entire analogue control-to-gain curve is **voiced compatibility**: no qualifying raw original-module sweep establishes its shape or numbers, and a measurement floor cannot prove a hard deadband (OQ-19). Card residuals are OQ-10; common VCA LEVEL is deliberately absent because it is downstream |
| Stored VCA LEVEL | The stored VCA LEVEL parameter drives the common uPC1252H2 on the jack board, downstream of the voice sum and shared high-pass and upstream of the chorus | One quantised, slewed common gain used to match patch loudness and chorus drive. The current dB-domain curve is a provisional fit to reported points: approximately −15 dB at panel −5, −12.5 dB at 0 and +5 dB at +5 | **Anchored** placement and shared ownership only. The device/in-circuit control mechanism and the whole byte-to-voltage/current-to-gain law remain **voiced/fitted**; no qualifying manufacturer or dense original-unit transfer has settled them (OQ-02) |
| Voice assignment | Hash-identified A-5 assigner behavior: POLY 1 keeps per-voice memory of the untransposed physical key and otherwise takes the free voice released longest ago; POLY 2 scans linearly from the first voice, chopping old tails; **no voice stealing in either mode** — a seventh simultaneously held key is dropped | Both policies. A key-up makes that slot assignable even while sustain keeps its old sound ringing, matching the assigner's key table rather than treating sustain as another held physical key | These allocation, physical-key and sustain-ownership semantics are **ROM-resolved for the supplied A-5**, not generalized to unidentified revisions. The distinction under transpose and release is asserted behaviorally |
| Assign mode switches | Panel wiring establishes two momentary scan contacts and lamp outputs; hash-identified A-5 behavior latches POLY 1, POLY 2 or both/Unison, with both lamps off not a stable mode | The paired parameters expose those three states. An ordinary click has one contact's meaning; Shift-click explicitly represents pressing both together. Re-pressing the selected virtual button preserves its lamp but repeats the assigner action | Contact topology is **anchored** by the panel circuit. Three-state latch, simultaneous-both handling, accepted-press gate/clear/rescan and power-on fallback are **ROM-resolved for the supplied A-5**. Obsolete both-off plug-in state canonicalises to POLY 1 |
| Solo Unison | Hash-identified A-5 assigns all six slots, makes the highest still-held key win a rescan and gates/rebuilds after key-up; B-2 programs equal pitch counts per slot | Six equal-frequency, unnormalised voices. Physical DCOs continue free-running behind closed VCAs and are not reset together merely because Unison is selected; a genuinely idle voice consumes a changed-pitch reset at its own later converter slot | Assignment/rescan and equal digital pitch are **ROM-resolved for the supplied A-5/B-2 images**. Free-running state and the uncompensated analogue sum are **anchored/derived from topology**. There is no programmed detune or `1/6` gain, and equal counts do not imply forced phase lock; exact sub-pass reset timestamps remain OQ-08 |
| Voice tolerance | The service procedure calibrates each card, but no qualifying repeated six-card/multi-unit data set fixes the residual population or thermal process; the digital envelope generator is shared | The calibrated nominal profile has zero inter-voice spread and zero drift. Existing deterministic seeded offsets/wander are available only as optional `Unit Character` compatibility/sound-design behavior, attached to physical voice slots | Zero nominal is **product policy** in the absence of measurements; the optional distribution remains **voiced** pending OQ-10. A fixed seed must reproduce exactly and Character amount zero must collapse all cards to nominal. Shared CV ownership permits downstream card error but rules out independent envelope laws or six independent sub/noise controls |
| Portamento | Hash-identified B-2 reads an 8-bit raw ADC value; zero is direct/immediate, nonzero selects an 8-bit coefficient by `raw>>1`, and index zero is also immediate. Six 8.8-semitone slot states advance by constant add/subtract and clamp, including while inactive | Raw 0/1 are immediate; paired active raw codes share a coefficient; `octave_passes=ceil(12*256/c)`. The coefficient region `0x0A00–0x0A7F` hashes to `06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e` | Digital mapping, state width, direction, clamp and inactive-slot behavior are **ROM-resolved** for the stated B-2. Pot/ADC voltage, noise/hysteresis, sampling cadence, physical pass timing and revision comparison remain OQ-14 |
| Modulation | Hash-identified B-2 uses one shared free-running triangle magnitude state `0..0x1FFF`, hard endpoint clamps and direction/polarity state. Delay uses the OQ-12 attack increment for hold, then `byte>>4` selects one of eight fade bins whose output is the accumulator high byte | Per rate coefficient `c`, a ramp takes `ceil(8192/c)` passes and a signed cycle takes four ramps. Rate region `0x0C60–0x0D5F` hashes to `4e3d87f7f12202e846d4010b08799dabd4d70d3cb5cffa0566933587538ff1d0`; fade `0x0B30–0x0B3F` hashes to `e145e0e5de512ef77ae0ffb91cefea40263b8200e78ed2a9a81befc13cf8ac99`. Delay byte 0 is three passes total at nominal timing, not bypass | Rate coefficients, integer state/clamp behavior, attack-derived hold and fade bins are **ROM-resolved** for the stated B-2. Physical pass timing/jitter, analogue smoothing/output scale and revision comparison remain OQ-13 |
| Modulation depths | Pitch ±400 cents at full slider, filter ±3.5 octaves, bender pitch ±1 octave; the bender's filter axis maxes at 4064 counts ≈ ±3.6 octaves — the firmware's sensitivity-times-bend arithmetic, which settles a two-source disagreement an earlier revision resolved the other way (±6). The panel's LFO depth and the lever's LFO axis are *summed* by the firmware, so both together reach deeper than either alone | The same, in cents and converter counts; the bender sampled once per pass at 8-bit resolution with no extra smoothing | **Anchored** |
| Chorus lines and mix | Two 256-stage BBD lines, one per output, driven with opposite modulation. Dry is always present; TR11/TR12 (2SK30A/K381) mute the wet returns before R71/R73. TR7/TR8 are later full-output shunts, not chorus mutes | Two asynchronous lines; dry gain `100/39`, wet gain `100/47`, hence wet/dry `39/47` (−1.62 dB). Off retains BBD state and slews wet with a voiced 5 ms exponential time constant (`10–90%` ≈10.99 ms) | Topology, gains and settled dry-only bypass are **anchored**. TR11/TR12 transient/leakage is OQ-20; 5 ms is a labeled plug-in policy. TP3/TP4 are low-frequency modulation points, while BBD delay `128/f_clock` must use one CP phase's repetition frequency |
| Chorus modulation | No calibrated JUNO-106 capture located fixes the absolute sweep endpoints or exact mode rates. The implementation temporarily uses the closest measured sibling values: a **JUNO-60** sweep of 1.66–5.35 ms in both modes, at 0.513 Hz and 0.863 Hz | The JUNO-60 centre, depth and rates as an explicitly provisional fallback | **Voiced from a sibling, not anchored to a JUNO-106.** The JUNO-106 timing network does anchor a mode-rate ratio near 1.623, while the fallback pair's ratio is 1.682. Exact JUNO-106 TP3/TP4 periods and minimum/maximum BBD clock frequencies are requested before changing or claiming these numbers |
| Chorus modes | The owner's manual states that I and II cannot be used simultaneously; the board has one chorus-enable line and one binary I/II line, and the patch format stores the same two bits | Exactly three rendered states: Off, I and II, with mutually exclusive panel buttons | **Anchored.** `OneTwo` survives only as an input-compatibility enum for early plug-in sessions. It canonicalises to II and never selects a fourth rate. No parallel-resistor or JUNO-60 both-buttons mode is inferred for the JUNO-106 |
| Chorus nonlinearity | MN3009 typical distortion is 0.3% at 0.78 Vrms and 2.5% at its 1.5 Vrms input-swing point; the bias window implies an asymptote near 2.9 V at the modelled node | A generalized algebraic soft clip fitted jointly to both datasheet distortion anchors, with a 2.924 V asymptote. It remains substantially straighter below overload than a plain `tanh`, then bends rapidly near the part's window | **Datasheet-fitted.** A plain `tanh` at the same asymptote produced about 1.2% at the 0.78 Vrms test point and therefore coloured normal wet levels too strongly. The surrounding ±15 V op-amps stay linear while the BBD write bends, so hot drive grits wet without equivalently clipping dry |
| Chorus charge transfer | The MN3009 datasheet specifies −3 dB at 12 kHz on a 40 kHz clock for the complete part, including its rectangular held output | The explicit BBD output hold already supplies `sinc(12/40) = −1.326 dB`; a clock-rate one-pole supplies only the residual −1.674 dB, using update coefficient 0.8654743 | **Derived from the datasheet anchor without double-counting the existing zero-order hold.** Applying a second full −3 dB loss on top of that hold makes the model too dark |
| Chorus noise | No compander anywhere in the circuit; no calibrated hardware SNR or stereo-correlation measurement has been located | A provisional independent per-line floor plus separately parameterized common/correlated random, hum and clock-spur hypotheses. The optional latter components default to zero; the one Chorus Noise extension scales every component and can defeat them all | **Voiced.** The closest located capture reports noise level alone, with no reference tone, calibrated level, weighting, bandwidth or cross-spectrum, so it cannot establish the amplitudes, spectra or correlation. A calibrated same-path stereo capture is needed before enabling or anchoring the optional terms |
| Chorus support and coupling filters | Service-note component values show two emitter-follower Sallen-Key low-pass sections before and after each BBD, an extra passive input pole, a wet-only coupling high-pass and an output tap-summing pole | Two Sallen-Key sections at 9.69 kHz/Q 0.549 and 10.38 kHz/Q 1.291 on each side; R122 10 kΩ with C52 2.2 nF gives the 7.23 kHz input pole; C44/C47 0.1 µF with R120/R114 100 kΩ gives the wet-only 15.9 Hz high-pass; and `(3.3 kΩ || 47 kΩ) × 2.2 nF` gives a nominal 23.46 kHz tap-summing low-pass | The component topology is **anchored**. The 23.46 kHz output pole assumes an ideal active MN3009 output because its output impedance is unspecified, so it is explicitly **provisional**, not omitted. A full MNA including a measured output impedance or a calibrated wet-only sweep should replace that nominal first-order value |
| Oversampling | Standard practice for nonlinear audio | The complete voice, filter, amplifier and both delay lines run at 4x for host rates below 88.2 kHz, 2x below 176.4 kHz, and natively above, followed by a 63-tap Blackman-Harris half-band per stage. A requested rate change waits for voices and musical tails, then a block-size-independent 5 ms output fade brackets the necessary rate-dependent reset. The host-rate C17/C20 states are preserved | Genuine internal oversampling with filtered decimation, not a quality label. Reported latency is the deepest path's real group delay and shallower modes are padded to keep it constant. With oversampling off, the delay lines' clock-rate images fold with only the reconstruction pole to soften them — a documented cost of that setting. The transition fade is a click-prevention product policy, not reference-unit behavior |
| Output stage | Service-note signal order: voice VCAs, 0.1-per-voice summer, shared HPF, common VCA LEVEL and chorus/IC6. Each IC6 output then crosses C17/C20 10 µF and R54/R57 1.5 kΩ into one 10 kΩ track of the dual VOLUME control. A dual-gang H/M/L attenuator network feeds the Mono/Stereo output-jack paths, while IC7 separately drives PHONES. Published output levels are L −30 dBm, M −15 dBm and H 0 dBm | After the complete dry+wet IC6 stereo sum and host-rate decimation, each channel runs through the independent current-scope coupling transfer: `fc = 1/(2π·10 µF·11.5 kΩ) = 1.383956 Hz`, high-frequency gain `10/11.5 = 0.869565` (−1.213957 dB). It runs before a squared VOLUME curve with 5 ms anti-zipper glide. The engine then declares a fixed High/0 dB selector-equivalent product position and applies `digital=analogue*10^(-18/20)/Vref_rms`. Floating samples beyond `+/-1` are allowed; no limiter is part of the mapping | Coupling placement, named parts and the unloaded/current-scope transfer are **anchored/derived** and occur exactly once. The full selector/headphone load can alter that pole and gain, so it remains OQ-17. The squared taper, fixed selector-equivalent and -18 dBFS RMS mapping are **product policy**. Absolute `Vref_rms`, one-versus-two-plug transfer, loaded clipping (OQ-05), real pot/channel tracking and headphone transfer remain open |
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
| Switch byte 17 | bit 0 PWM manual, bit 1 VCA gate, bit 2 negative polarity, bits 3-4 high-pass position **counting down** | **Anchored.** The high-pass field runs opposite to the panel's numbering, which is the easiest thing in the format to get backwards, so it is asserted position by position |
| What a patch does not carry | Volume, the bender depths, portamento and the assign mode | **Anchored.** These are performance controls on this instrument, so loading a patch deliberately leaves them where the player set them |
| Legacy I+II | No hardware state or encoding exists | Old plug-in sessions may still deserialize the obsolete compatibility value. It canonicalises to II; current panel, factory patches and outgoing messages use only Off/I/II |
| Malformed range bits | The three range bits are one-hot in practice but nothing enforces it | A message asserting none or several resolves to the middle range rather than being rejected: it is still a message that arrived |

The factory bank in `Source/DSP/YouKnow106Presets.cpp` is written in the same
units and every entry is checked to round-trip to the same effective 18-byte,
7-bit patch state, so the bank can be sent to hardware. Decimal panel positions
that land on the same 7-bit step are intentionally equivalent; an unencodable
categorical setting is not. Those patches are **original YouKnow106 work**: this
project ships no Roland ROM contents, and the reference instrument's own
factory bank is not reproduced.

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
   each contributes `0.1` to the bus. One shared high-pass processes that bus,
   followed by the common uPC1252H2 controlled by stored VCA LEVEL and the
   series coupling into the chorus.
7. The wet path passes its 15.9 Hz coupling high-pass, both delay lines clocked
   with opposite modulation, the datasheet-fitted write nonlinearity, the
   zero-order hold plus residual charge-transfer loss, and the reconstruction
   chain including its provisional 23.46 kHz tap-summing pole. Each output
   channel then sums dry at `100/39` with its own wet line at `100/47` in IC6.
   Bypass mutes the wet return but leaves both lines running. No unmeasured
   low-voltage rail is synthesized after this ±15 V stage.
8. Half-band decimation to the host rate, then one independent C17/C20 output-
   coupling state per channel. Its 10 µF, 1.5 kΩ and 10 kΩ current-scope
   network removes DC and settles to `10/11.5` gain before the volume control —
   the one true potentiometer in the audio path — followed once by the declared
   -18 dBFS RMS/`Vref_rms` product mapping. That boundary allows floating output
   beyond `+/-1` and does not feed back into circuit drive.

## What remains open

The canonical research queue is
[open questions](open-questions.md). Each entry contains a standalone task
definition, the exact output expected from an LLM or hardware researcher, the
current assumption being tested and a shared evidence contract. “Not found”
and “measurement required” are valid results; a guessed value is not.

| Area | Canonical task |
| --- | --- |
| Absolute JUNO-106 chorus timing | OQ-01 |
| Stored VCA LEVEL byte-to-gain law | OQ-02 |
| Calibrated chorus noise and SNR | OQ-03 |
| Loaded post-BBD tap-summing pole | OQ-04 |
| Loaded IC6/High-output clipping swing | OQ-05 |
| Absolute `Vref_rms` calibration under the adopted -18 dBFS RMS convention | OQ-06 |
| Converter hold topology and time constants | OQ-07 |
| Exact intra-pass converter scan/write order | OQ-08 |
| Resonance byte-to-loop-gain law | OQ-09 |
| Measurements capable of replacing the zero-spread/zero-drift nominal policy | OQ-10 |
| Pulse-off pinned-leg mixer behaviour | OQ-11 |
| Envelope physical timing, audible thresholds and firmware-revision scope | OQ-12 |
| LFO/delay physical timing, analogue transfer and firmware-revision scope | OQ-13 |
| Portamento pot/ADC behavior, timing and firmware-revision scope | OQ-14 |
| Oscillator-mixer levels and filter-drive calibration | OQ-15 |
| Main noise spectrum and filter self-oscillation startup | OQ-16 |
| Main VOLUME taper and output-selector transfer | OQ-17 |
| Hardware cutoff-converter saturation behind the transparent 50 kHz product cap | OQ-18 |
| Measured central voice-module VCA gain, knee and possible deadband | OQ-19 |
| Chorus wet-mute switching transient and leakage | OQ-20 |

JUNO-60 findings may be retained only as labelled comparative evidence. They
cannot close a JUNO-106 task.

## Sources

Values were gathered from the instrument's service notes and owner's manual;
component datasheets for the delay line and its clock driver; published
clean-room reverse engineering of the assigner ROM; exact behavioral analysis
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
third-party source code, table data, netlist, ROM image or recording is
included in this repository; laws fitted through published behavioural anchors
are YouKnow106's own constructions.

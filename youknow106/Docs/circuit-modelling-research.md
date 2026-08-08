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
| Control scan | Service Notes pp. 5, 8 and 13: one 12-bit converter and three 8-way muxes, with 18 per-card holds (DCO, VCF and ENV/GATE VCA for six cards), five shared holds (SUB, stored VCA LEVEL, PWM, RESONANCE and NOISE), one unused channel, and a 4.2 ms pass. The p. 8 chart orders shared RES/VCA/SUB, DCO 1–6, PWM, interleaved VCF/VCA 1–6, then NOISE and depicts them sequentially across the pass | A fractional scheduler preserves the nominal 4.2 ms average without host-rate truncation. Each pass executes the exact 23-write logical queue. The default `NormalizedServiceChart` compatibility profile spaces its ordinal events monotonically across the pass so six DCO resets are not collapsed onto one sample; `PhaseZeroDiagnostic` retains only order for comparison. Each destination has a separately named hold constant; VCF 522 µs and voice VCA 687 µs are evidence-backed, the common VCA LEVEL path's post-S/H C7 network derives 9.08249 ms, the PWM hold crosses its derived R117/C62 then R116/C63 cascade (4.7 ms, 2.632 ms) and the SUB hold its derived R11/C1 10 ms pole; equal 522 µs values on the DCO, RESONANCE and NOISE nodes remain compatibility defaults | Topology, nominal pass, ordinal order and qualitative non-simultaneity are **anchored** by the Service Notes. The supplied hash-matched B-2's `b<<7` aligned work word and `b<<5` physical 12-bit DAC code are **ROM-resolved**; the two domains must not be conflated. Normalized offsets and first-host-snapshot priming are explicit **product/compatibility policy**, not measured timing. Absolute offsets, jitter, acquisition/droop, branches and other destination constants remain OQ-07/OQ-08 |
| Ramp generator | Service notes describe an integrator whose capacitor is charged from the compensation voltage, and the published reverse-engineering of this oscillator shows that voltage feeding a resistor into a virtual-ground integrator — a constant-current charge | A straight 12 Vpp rising ramp with a finite-slope reset of 2.2 µs. The compensation voltage that holds the amplitude constant is modelled as one of the scanned, slewed control voltages, while the timer count steps instantly — so every pitch step, bend, glide and octave change leaves a momentary amplitude error on the ramp until the hold catches up. The error renders as the *slope of each rise*, frozen per cycle about the fixed bottom rail, because an integrator charging from a slewed current changes the slope of the next flank and never the value mid-cycle; it reaches the pulse only through the comparator's edge times (the duty law solves the threshold against the achieved amplitude), never as an amplitude multiply | **Anchored** shape — an earlier revision kept a bow on a misreading of the reverse-engineering account; the straight ramp is also the only shape consistent with the comparator's 6 V / 50% duty anchor. The compensation-slew transient is **derived** from the scan architecture; see entry 24 for the removed outer multiply that stepped the waveform mid-cycle |
| Ramp bandlimiting | Integrated-B-spline residual tables built by numerically integrating a Blackman-windowed sinc at 64x and read with linear interpolation, as in the LUT-BLEP literature | Both natural reset corners are slope discontinuities, so each uses the *slope* residual; comparator/divider edges use the *step* residual. The current changed-pitch-write model restarts phase/comparator/divider at the converter interval boundary while preserving pending histories and bandlimiting the resulting value/slope changes; only a hard engine reset or virtual-card teardown clears them | Natural-corner treatment is a numerical antialiasing requirement, and the suite asserts an alias floor below −70 dB across the keyboard at 44.1 and 48 kHz. The tables are read with linear interpolation between entries: rounding to the nearest of the 64× entries quantises every edge's sub-sample position to 1/64 of an internal sample, about 23 ns of timing jitter at 192 kHz applied to every comparator edge, divider edge and ramp corner. Removing it lowers the measured floor by 2–3 dB on high notes. It is not what sets the remaining floor, which neither interpolation nor a wider kernel moves. The restart fixture bounds its largest normalized pulse increment below 0.65 of a hard step, but the physical timer-reload edge and the ramp/comparator/sub states it actually forces remain OQ-08; the current restart semantics are **model policy**, not measured hardware fact |
| Pulse and PWM | Comparator against a control voltage: 50% duty at +6 V, 95% at +0.6 V, and Service Notes p. 9 says −0.8 V pins the output high. PWM is one shared hold | Duty derives from the shared slewed threshold against each card's momentarily mis-scaled ramp, retaining per-card comparator offset. The comparator crossing solver follows both the moving ramp and the slewing PWM threshold inside each audio sample; this prevents an implementation-only missed edge/full-cycle blip under deep PWM. Pulse Off writes −0.8 V and pins the modeled comparator high while DCO/sub keep running; the audio contribution remains hard-gated because the local comparator-to-voice-mixer coupling/loading is unmeasured. A temporarily under-compensated ramp can also sit wholly below an enabled positive threshold, which the renderer treats as pinned low rather than forcing a 5% pulse. The final C17/C20 output coupling does not answer the upstream off-state question. The LFO reaches PWM raw, a provenance-pending firmware lead | Duty anchors, shared ownership and the off control/comparator state are **anchored**. The moving-threshold solve is a numerical consequence of that topology. The nominal calibrated enabled range is 5–95%; pitch-hold/ramp-current mismatch can move the crossing beyond it. Pinned-leg DC/bleed/loading/transient remain OQ-11, so no audible off artifact or anti-click envelope is invented |
| Sub oscillator | A divide-by-two flip-flop clocked by the counter's terminal pulse; Service Notes pp. 8–9 show one shared SUB LEVEL hold controlling its collector-supply amplitude | An exact square one octave below the selected footage, unaffected by pulse width, with edges at reset start; every card consumes the same scanned/slewed 7-bit sub-level voltage before card-specific analogue error | **Anchored** topology, divider relationship, shared ownership and stored path (SysEx byte 15). There is no sub-octave selector. Exact full-scale amplitude and loaded mixer transfer remain OQ-15 |
| Noise | One shared generator and one shared NOISE LEVEL hold feed all voice mixers. Module p. 13 draws the source's own support circuit: Tr21 (2SC945, factory-selected) with R104 470 kΩ collector load, C42 1 µF into the BA662 level OTA's 4.7 kΩ input bias, and C41 100 pF against R79 330 kΩ loading the OTA output before the buffered rail | A single bounded-uniform white generator band-shaped by its own circuit — a 33.9 Hz high-pass (C42/4.7 kΩ) and a 4.82 kHz pole (C41/R79), both run at the internal rate with unity passband — added to each voice before its filter, scaled by one scanned/slewed 7-bit shared level then card-specific residual error; a separate tiny per-voice excitation starts a silent self-oscillating filter. Both discrete sources scale by `sqrt(internal_rate / 192 kHz)` so their wall-clock spectral density does not change with host rate or HQ mode | **Anchored** shared source/hold topology, control path (SysEx byte 4) and 4.0 Vpp TP8 adjustment; the two shaping corners are **derived** from the 2026-08-07 p. 13 designator read (the level OTA sits between them, so shaping the shared source once is exact). Rate normalization is a numerical/product requirement, not a hardware-amplitude claim. TP8 is downstream and does not establish the model's pre-filter `+/-2 V` coordinate, RMS or distribution; those, the generator's amplitude distribution and startup excitation are **voiced** pending OQ-15/OQ-16 |
| Voice summer and high-pass | IC1a receives every voice through 33 kΩ against 3.3 kΩ feedback. Its summed output crosses C14 10 µF NP into R39 33 kΩ before the one four-position switched high-pass network on the jack board | The six voices sum at `3.3/33 = 0.1` each, then cross one C14 state whose sub-hertz resistance follows the selected leg: Boost/Flat add R25/R26 47 kΩ in parallel with R39 for 0.820915 Hz; the capacitor-selected Cut legs are open there, leaving R39 and 0.482288 Hz. The shared switch then applies the derived Boost shelf (+10.50 dB DC, +1.41 dB high band, 59.41 Hz pole — the corrected 2026-08-07 p. 15 branch read, within 0.016 dB of the exact two-zero/two-pole solve), Flat pass, or 225.8/720.5 Hz cut pole | **Anchored/derived** placement, parts and asymptotic loads from the service-note schematic. The cut capacitors begin loading C14 as frequency rises; full switched-network MNA, CMOS parasitics and deselected-capacitor switching memory remain OQ-21. The present asymptotic common state is explicit rather than falsely claiming C14 is absent. Unity-summing the voices overdrives every downstream common stage by 20 dB |
| Filter core | A photographed A1QH80017A teardown identifies one IR3109 quad OTA/filter plus two BA662s; the service circuit gives a 68 kΩ input resistor, 560 Ω shunt and 240 pF integrator per stage | Four transconductor stages solved together implicitly, `C dVn/dt = Ig tanh((V(n-1) − Vn)/H)` with `H = 2·Vt/attenuation = 6.37 V`, integrated by taking each stage's tanh exactly averaged along the straight drive path between steps (the divided difference of ln cosh — first-order antiderivative anti-aliasing inside the Newton iteration, which degenerates to the trapezoid's endpoint average in the linear region) and closed with a damped Newton step whose Jacobian is bidiagonal plus one corner term | Device identity is **anchored by the photographed teardown** and the topology/component values are **anchored**. The suite checks the model against a fourth-order Runge-Kutta solve of the same ODE at 16x, and both against the closed-form `1/(4 − k)`, to 0.6 dB |
| Filter drive level | The 68 kΩ/560 Ω divider attenuates each stage's differential input by `560/(68000+560) = 0.00816803`, or 122.43:1 | That attenuator, not a user-facing "drive" control, sets the differential pair's nonlinear span; the engine currently uses `+/-6 V` saw/pulse, `+/-5 V` sub and `+/-2 V` noise coordinates followed by a 0.40 scale | The component attenuator and OTA span are **derived**. A centered `+/-6 V` interpretation is merely compatible with a 12 Vpp reading at the same source node; it does not establish the loaded transfer. The sub/noise coordinates and 0.40 mapping are **voiced compatibility** pending OQ-15 |
| Cutoff control law | Firmware: the panel byte times 128, envelope, modulator, bender and key-follow terms summed in a 14-bit accumulator clamped to [0, 16383], the top 12 bits driving the converter; 5.53 Hz at code 0, 1143 counts per octave; service check of 248 Hz self-oscillation at code 6272. Service Notes p. 1 publishes an approximate 5 Hz–50 kHz range | `f = 5.53 · 2^(counts/1143)` through its established range, followed in the default profile by a transparent numerical `min(..., 50000 Hz)` cap. The digital sum is clamped and truncated to 4-count steps. The former 24 kHz/tanh/52.2 kHz curve may be retained only as a named legacy profile | Count-domain sum, base, octave slope, service point, clamp and truncation are **anchored**. The 50 kHz cap is **product policy**, not a claim about converter saturation. A described 93-point/single-card fallback table lacks the complete raw capture, metadata and population scope needed to resolve OQ-18 |
| Resonance | A photographed A1QH80017A teardown assigns one BA662 to the IR3109 resonance-feedback path; Service Notes p. 19 trims every card to a 4.8 Vpp self-oscillating sine. Hash-identified B-2 behavior forms aligned work word `W=128b` and physical converter code `DAC12=32b` from stored resonance byte `b`; Service Notes pp. 5, 8 and 13 establish one shared IC26-channel-6 hold. No qualifying original-unit sweep establishes the subsequent DAC-voltage/current-to-loop-gain transfer | The exact stored-byte conversion and one shared queue write feed a named `VoicedResonanceCompatibilityProfile`. That replaceable profile retains the existing quadratic/linear panel-to-loop curve, circuit-shaped nonlinear return and optional per-card Unit Character residual without changing preset bytes | BA662/IR3109 identity, shared ownership and the service endpoint are **anchored**; `b → W → DAC12` is **ROM-resolved** for the identified image. The 4.8 Vpp adjustment has no published tolerance and does not identify loop gain. Every numerical analogue step after the DAC — including the current 30%/90%/maximum landmarks, loop limiter and card residual magnitude — is **voiced compatibility**, not a fitted, measured or calibrated hardware law (OQ-09/OQ-10) |
| Resonance compensation | Roland's own drawing feeds the resonance amplifier from both VCF IN and VCF OUT with its output returned to the input chain, and the dksynth-lineage module reconstruction (Open80017a, build-validated in a real JUNO-106) makes that wiring netlist-explicit: VCF IN through 24 kΩ/1.5 kΩ (÷17.0) on one input, VCF OUT through 100 kΩ/1.5 kΩ (÷67.7) on the other, output current injected at the first stage's 4.7 kΩ/560 Ω/68 kΩ node | The named resonance compatibility profile retains input multiplier `1 + 0.2296·k`, preserving the current high-Q drive character | The *mechanism and direction* are settled — raising resonance raises input drive through the same transconductance — and must not be removed. The coefficient remains **voiced compatibility**: the reconstruction's resistor-only conversion gives 0.275 per unit loop gain (gm cancels through stage 1's −68 k/4.7 k feedback gain), about 20% above the shipping value and of the same linear form, but it is one reconstruction lineage and is not promoted; OQ-09's measured family owns the number, and the silent-input self-oscillation endpoint solve is independent of this input-side multiplier |
| Oscillation frequency correction | Service Notes ADJUSTMENT trims every card at BANK 3 with C4 held to a 4.8 Vp-p self-oscillating sine at 248 Hz — two steps, one card, one state. The larger limit cycle the amplitude anchor requires compresses the stage `tanh` and pulls the oscillation flat, so the two anchors have to be satisfied together | The correction is the reciprocal of the pole scaling the limit cycle imposes on itself, from the harmonic balance of the cascade's own two nonlinearities: the sinusoidal-input describing function `N(a) = (2/πa)∫₀^π tanh(a sin t) sin t dt` on the four stage pairs at `2Vt/stageAttenuation` = 6.3663 V and on the resonance return at `2Vt·(100/1.5)` = 3.4667 V, solved for the limit cycle each loop gain sustains and tabulated over loop gain. It is identically 1 at and below a loop gain of 4, where four one-poles at their own corner close the loop and no limit cycle exists. `maximumFeedback = 4.504` is now the only fitted constant and answers to the amplitude anchor alone; the pair renders **4.80 Vp-p at 247.9 Hz** | The two **endpoints are anchored**. The amplitude endpoint fixes `maximumFeedback`; the 248 Hz endpoint is no longer fitted at all but **predicted to within 1 cent** by the derivation, at every loop gain above the threshold. The *shape* between the ends — the quadratic-then-linear panel curve in `loopGain()` — remains **voiced**. The earlier fitted quadratic `1 + 0.098·min(k/4,1.2)²` was replaced because it lifted cutoff by +32 cents at resonance panel 0.50 and +116 at 0.80, where the cascade does not oscillate. OQ-09's measured response-versus-resonance family still owns the shape |
| Envelope | Hash-identified B-2: one 14-bit state per generated envelope, sustain `S=128b`, saturating additive attack without retrigger reset, and shared decay/release coefficient selection. For `v_hi=v>>8`, `v_lo=v&255`, `c_hi=c>>8`, `c_lo=c&255`, its fall helper is `Q(v,c)=c_hi*v_hi+floor(c_lo*v_hi/256)+floor(c_hi*v_lo/256)`; the low×low term is intentionally omitted | Attack is `min(0x3FFF,E+A[b])`; decay is `S+Q(E-S,c)` when above sustain and otherwise snaps to `S`; release is `Q(E,c)`. The recurrence retains both low bits, while the VCF envelope path, ENV-mode voice VCA and display consume the physical 12-bit fraction `(E>>2)/4095`. The attack region `0x0B60–0x0C5F` hashes to `faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`; decay/release `0x0D60–0x0E5F` hashes to `0de73bedf11904538056eec3622b09470461f13ad016103ab9992be73e467754` | **ROM-resolved** for the stated B-2 image, including coefficients, DAC truncation, rounding, clamp, sustain and retrigger semantics. OQ-12 now concerns hardware pass timing/jitter, analogue-node/audible thresholds, independent behavioral confirmation and other revisions, not recovering these tables |
| Voice-module VCA | A photographed A1QH80017A teardown identifies the second BA662 as the per-voice VCA. Roland draws VCF OUT pin 3 through C59 1 µF/50 V NP and the VR27/R108 signal network to VCA IN pin 9. Separately, the held VCA CV reaches VCA CONT pin 11 through R106 10 kΩ, C58 0.1 µF, R105 22 kΩ and grounded-base Tr20. VCA OUT pin 10 reaches TP8–TP13 and the 33 kΩ summer inputs. Service Notes pp. 18–19 adjust VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump and set 6 Vpp gain | `VoiceVcaControlLaw` is a smooth quasi-linear compatibility approximation motivated by the external volts-to-current topology. The surviving deterministic `vcaControlOffset` affects only the control hold. The nominal audio path is `filtered → C59 → gain`: C59 is modelled as a per-voice first-order coupling at 4.82 Hz (1 µF against a **voiced** 33 kΩ pin-9 load, bracketed 33–100 kΩ → 4.82–1.59 Hz, because R108 and VR27's setting are not in tree). It adds no guessed signal-input residual. Velocity remains an optional extension | Device identity, pins, ENV/GATE ownership, separate control/signal paths, minimum-thump null procedure and 6 Vpp endpoint are **anchored**. The exact gain/current transfer near cutoff, Tr20 onset, BA662 knee/deadband and post-calibration thump distribution are not. The former `(0.0008+0.002·cardOffset)·control` signal term was then multiplied by VCA gain, creating unsupported control² and conflating the control hold with VR30's pin-9 null. The shared uPC1252H2 is downstream of the voice sum and cannot create per-voice ENV/GATE thump. OQ-19 owns the missing measurements |
| Stored VCA LEVEL | The stored VCA LEVEL parameter drives the common uPC1252H2 on the jack board, downstream of the voice sum and shared high-pass and upstream of the chorus. Service Notes p. 8 gives the VCA LEVEL converter range as +4 to −6 V, while the hash-matched B-2 establishes physical 12-bit code `d=b<<5`. Page 15 shows R30 2.2 kΩ from the converter hold to C7 10 µF NP, then R32 1.5 kΩ to GC1, with R31 47 Ω to ground and R165 15 kΩ to +15 V. NEC specifies GC1 at −5.9 mV/dB typical (5.8–6.1 mV/dB magnitude) | Assuming the ideal 12-bit R-2R convention `Vhold=4−10d/4096`, the loaded divider gives `Vgc=0.01250467817·Vhold+0.04626730922`, hence `gain_dB=−16.3196647+0.165581014·b`. The C7 resistance is `R30||(R32+(R31||R165))=908.249 Ω`, so `τ=9.08249 ms` and `fc=17.523 Hz`. One C12/R36 0.482288 Hz input-coupling state precedes that quantised, slewed gain | Placement, shared ownership, physical code, populated network and nominal law are **anchored/ROM-resolved/derived**. Division by 4096 is an explicit ideal-DAC assumption rather than a measured endpoint. R32 is read as 1.5 kΩ but is the least-legible designator/value in the available scan. OQ-02 now asks for an installed sweep to quantify resistor/capacitor tolerance, rail error and µPC1252 variation, not to recover the nominal curve |
| Voice assignment | Hash-identified A-5 assigner behavior: POLY 1 keeps per-voice memory of the untransposed physical key and otherwise takes the free voice released longest ago; POLY 2 scans linearly from the first voice, chopping old tails; **no voice stealing in either mode** — a seventh simultaneously held key is dropped | Both policies. A key-up makes that slot assignable even while sustain keeps its old sound ringing, matching the assigner's key table rather than treating sustain as another held physical key | These allocation, physical-key and sustain-ownership semantics are **ROM-resolved for the supplied A-5**, not generalized to unidentified revisions. The distinction under transpose and release is asserted behaviorally |
| Assign mode switches | Panel wiring establishes two momentary scan contacts and lamp outputs; hash-identified A-5 behavior latches POLY 1, POLY 2 or both/Unison, with both lamps off not a stable mode | The paired parameters expose those three states. An ordinary click has one contact's meaning; Shift-click explicitly represents pressing both together. Re-pressing the selected virtual button preserves its lamp but repeats the assigner action | Contact topology is **anchored** by the panel circuit. Three-state latch, simultaneous-both handling, accepted-press gate/clear/rescan and power-on fallback are **ROM-resolved for the supplied A-5**. Obsolete both-off plug-in state canonicalises to POLY 1 |
| Solo Unison | Hash-identified A-5 assigns all six slots, makes the highest still-held key win a rescan and gates/rebuilds after key-up; B-2 programs equal pitch counts per slot | Six equal-frequency, unnormalised voices. Physical DCOs continue free-running behind closed VCAs and are not reset together merely because Unison is selected; a genuinely idle voice consumes a changed-pitch reset at its own later converter slot | Assignment/rescan and equal digital pitch are **ROM-resolved for the supplied A-5/B-2 images**. Free-running state and the uncompensated analogue sum are **anchored/derived from topology**. There is no programmed detune or `1/6` gain, and equal counts do not imply forced phase lock; exact sub-pass reset timestamps remain OQ-08 |
| Voice tolerance | The service procedure calibrates each card, but no qualifying repeated six-card/multi-unit data set fixes the residual population or thermal process; the digital envelope generator is shared | The calibrated nominal profile has zero inter-voice spread and zero drift. Existing deterministic seeded offsets/wander are available only as optional `Unit Character` compatibility/sound-design behavior, attached to physical voice slots | Zero nominal is **product policy** in the absence of measurements; the optional distribution remains **voiced** pending OQ-10. A fixed seed must reproduce exactly and Character amount zero must collapse all cards to nominal. Shared CV ownership permits downstream card error but rules out independent envelope laws or six independent sub/noise controls |
| Portamento | Hash-identified B-2 reads an 8-bit raw ADC value; zero is direct/immediate, nonzero selects an 8-bit coefficient by `raw>>1`, and index zero is also immediate. Six 8.8-semitone slot states advance by constant add/subtract and clamp, including while inactive | Raw 0/1 are immediate; paired active raw codes share a coefficient; `octave_passes=ceil(12*256/c)`. The coefficient region `0x0A00–0x0A7F` hashes to `06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e` | Digital mapping, state width, direction, clamp and inactive-slot behavior are **ROM-resolved** for the stated B-2. Pot/ADC voltage, noise/hysteresis, sampling cadence, physical pass timing and revision comparison remain OQ-14 |
| Modulation | Hash-identified B-2 uses one shared free-running triangle magnitude state `0..0x1FFF`, hard endpoint clamps and direction/polarity state. Delay uses the OQ-12 attack increment for hold, then `byte>>4` selects one of eight fade bins whose output is the accumulator high byte | Per rate coefficient `c`, a ramp takes `ceil(8192/c)` passes and a signed cycle takes four ramps. Rate region `0x0C60–0x0D5F` hashes to `4e3d87f7f12202e846d4010b08799dabd4d70d3cb5cffa0566933587538ff1d0`; fade `0x0B30–0x0B3F` hashes to `e145e0e5de512ef77ae0ffb91cefea40263b8200e78ed2a9a81befc13cf8ac99`. Delay byte 0 is three passes total at nominal timing, not bypass | Rate coefficients, integer state/clamp behavior, attack-derived hold and fade bins are **ROM-resolved** for the stated B-2. Physical pass timing/jitter, analogue smoothing/output scale and revision comparison remain OQ-13 |
| Modulation depths | Pitch ±400 cents at full slider, filter ±3.5 octaves, bender pitch ±1 octave; the bender's filter axis maxes at 4064 counts ≈ ±3.6 octaves — the firmware's sensitivity-times-bend arithmetic, which settles a two-source disagreement an earlier revision resolved the other way (±6). The panel's LFO depth and the lever's LFO axis are *summed* by the firmware, so both together reach deeper than either alone | The same, in cents and converter counts; the bender sampled once per pass at 8-bit resolution with no extra smoothing | **Anchored** |
| Chorus lines and mix | Two 256-stage BBD lines, one per output, driven with opposite modulation. Dry is always present; TR11/TR12 (2SK30A/K381) mute the wet returns before R72/R74. TR7/TR8 are later full-output shunts, not chorus mutes. The 2026-08-07 p. 15 designator read fixes the summer: dry arrives through R71/R73 47 kΩ off the shared IC2b bus, wet through R72/R74 39 kΩ from the mute JFETs, feedback R70/R67 100 kΩ | Two asynchronous lines; dry gain `100/47`, wet gain `100/39`, hence wet/dry `47/39` (+1.62 dB — the wet leg is the hotter one; an earlier revision carried the mirror). Off retains BBD state and slews wet with a voiced 5 ms exponential time constant (`10–90%` ≈10.99 ms) | Topology, gains and settled dry-only bypass are **anchored** (p. 15 designator-level read, corroborated by both sibling netlists). TR11/TR12 transient/leakage is OQ-20; 5 ms is a labeled plug-in policy. TP3/TP4 are low-frequency modulation points, while BBD delay `128/f_clock` must use one CP phase's repetition frequency |
| Chorus modulation oscillator | Service Notes p. 15: IC1 (µPC062) is an integrator (C3 across IC1b pins 6–7) closed around a Schmitt comparator (IC1a, R6 47 kΩ output and R7 33 kΩ triangle meeting at the non-inverting summing node, inverting input grounded — the earlier R15-divider/pin-2 transcription is falsified 34× by arithmetic and contradicted by the sister-board clone's netlist, which has no divider resistor on that node). IC2a inverts once through R10/R9 33 kΩ | A straight, symmetric triangle, and a second line driven by exactly its negative rather than by an independent oscillator | **Derived.** The integrator is fed a constant current for the whole of each half cycle, so both flanks are straight; an RC relaxation oscillator would bend them and this circuit is not one. TP4 is the triangle and TP3 its inverse, which is why the antiphase clocking is a mirror rather than two free modulators. The suite asserts the shape and the endpoints separately |
| Chorus modulation rates | Service Notes p. 15 fixes the *ratio*: the CHORUS I/II line (via Tr2/D1/R2 47 kΩ) drives JFET Tr1, which grounds the R5/R8 junction through R4 680 kΩ — R3 2.2 MΩ is Tr1's gate-source bleed; an earlier "shorts R3" description mislabelled the shunt leg (corrected by the 2026-08-07 page read). With the integrator input at virtual ground the shunt leg and R8 both return to 0 V, giving `R_eff` 6.4352941 MΩ (I) / 3.9638889 MΩ (II) with R5 1 MΩ, R8 2.2 MΩ, R4 680 kΩ. The *scale* closes from β = R7/R6 = 33/47 (summing-node comparator, netlist-verified on the sister board's clone) and C3 = 0.1 µF (reported as ".1" on p. 15; 100 nF in exactly this position in the clone's netlist) | `f = 1/(4·β·R_eff·C3)`: 0.5532934 Hz for I and 0.8982608 Hz for II. Sweep endpoints are the 106's own third-party-measured 1.4–6.4 ms (2026-08-07 promotion) | **Rates derived from this instrument's own circuit** — ratio 1.6234799, mode I the slower leg; scope readings of a 106-chorus clone corroborate both rates within 3% (0.537/0.879 Hz), and both truncate to the manual's about-0.5/about-0.8. The JUNO-60 pair and its 1.682 ratio are superseded, kept only as suite comparison values. The sweep was scoped on a designator-faithful build of the p. 15 board with genuine MN3009s and compared directly against a real 106 by its measurer (scope plots published; ±2.5 ms excursion matching the one independent depth report); the sibling JUNO-60's calibrated 1.66–5.35 ms is thereby superseded and kept as a suite comparison value. A calibrated original-unit capture is still requested (OQ-01) |
| Chorus delay-sweep law | Service Notes p. 15 shows each MN3101 driven by a transistor voltage-to-current converter (Tr22, R133 2.2 kΩ / R134 22 kΩ / R135 1.8 kΩ, C53 150 pF); KR-106 reports a ~50-point click-timing series across a real 106's modulation cycle fitting the delay linear in time, 16 µs RMS residual (raw clicks unpublished) | Delay arithmetic in seconds, `centre ± sweep·tri`, then `clock = 128/delay`; the frequency-linear (hyperbolic-delay) alternative retained behind `enableChorusHyperbolicSweep`, off by default | **Measured once, below the anchoring bar.** The current-source bias is the shape of a frequency-linear oscillator, but period-linear, frequency-linear and exponential clocks share identical endpoints, and the one existing time series — the only measurement kind that can discriminate them — reads delay-linear. That trajectory ships; the frequency-linear reading of Tr22 waits behind the switch for the calibrated clock time-series OQ-01 requests |
| Chorus modes | The owner's manual states that I and II cannot be used simultaneously; the board has one chorus-enable line and one binary I/II line, and the patch format stores the same two bits | Exactly three rendered states: Off, I and II, with mutually exclusive panel buttons | **Anchored.** `OneTwo` survives only as an input-compatibility enum for early plug-in sessions. It canonicalises to II and never selects a fourth rate. No parallel-resistor or JUNO-60 both-buttons mode is inferred for the JUNO-106 |
| Chorus nonlinearity | MN3009 typical distortion is 0.3% at 0.78 Vrms and 2.5% at its 1.5 Vrms input-swing point; the bias window implies an asymptote near 2.9 V at the modelled node | A generalized algebraic soft clip fitted jointly to both datasheet distortion anchors, with a 2.924 V asymptote. It remains substantially straighter below overload than a plain `tanh`, then bends rapidly near the part's window | **Datasheet-fitted.** A plain `tanh` at the same asymptote produced about 1.2% at the 0.78 Vrms test point and therefore coloured normal wet levels too strongly. The surrounding ±15 V op-amps stay linear while the BBD write bends, so hot drive grits wet without equivalently clipping dry |
| Chorus charge transfer | The adopted MN3009 datasheet row is −3 dB at 12 kHz on a 40 kHz clock for the complete part, including its rectangular held output, with 0 dB referenced at 1 kHz. The same sheet has low-resolution typical `Gi-fi` curves at fCP 10, 40 and 100 kHz | At the raw deterministic held node, before numerical output reconstruction, the explicit BBD hold supplies `sinc(12/40)=−1.326 dB` versus DC; a one-pole supplies only the residual −1.674 dB with coefficient 0.8654743. `transferLossStep` advances once per modeled BBD shift (one fCP period), so keeping that coefficient fixed already makes its absolute pole proportional to clock and its response invariant versus normalized `f/Fclock`. This upstream held response is −3.000 dB versus DC and −2.972 dB versus 1 kHz | **Derived at one numeric datasheet anchor without double-counting the existing zero-order hold.** The emitted post-polyBLEP waveform is not a literal rectangular hold, so the anchor is explicitly scoped upstream and is not silently retuned. The removed extra multiplier `alpha·(1+(clock−26000)·1.5e−6)` double-counted clock scaling and produced −2.757 dB versus DC, or −2.732 dB versus 1 kHz, at 40 kHz/12 kHz. The plotted curves are qualitative multi-clock evidence and do not justify claiming physical clock invariance; quantitative extraction and installed-unit confirmation remain OQ-04 |
| BBD output-grid reconstruction | A real BBD's clocked sampling creates device-domain images at `k·Fclock ± f`, termed BBD-generated aliasing (BGA) by Gabrielli, D'Angelo and Squartini. A discrete computer model can add simulation-generated aliasing (SGA) when asynchronous held-output steps are represented on its own fixed sample grid | A compact polyBLEP reconstructs only the deterministic step after residual transfer loss and before the tap-summing pole. Exact fractional edge times feed 54 fixed correction slots; the multiple-edge 90 kHz case and the tested 200 kHz/8 kHz worst case use at most 50. Buckets, read index, BBD phase, transfer state, held noise and RNG sequence are identical with the correction enabled; stochastic noise is not corrected. Grid-specific slots clear when the internal rate changes | The BGA/SGA distinction and method family are **supported by peer-reviewed numerical literature**; this exact compact scheduler and its results are an **engine-validated product mechanism**, with no hardware counterpart. The paper studies an ideal 4096-stage MN3005 at 44.1 kHz without leakage, noise or nonlinearity; this is a nonlinear, noisy 256-stage MN3009 model, so the paper's SNR figures are not product measurements. HQ preserves measured BGA far more closely than LQ; the implementation does not claim exact BGA invariance at every grid |
| Chorus noise | No compander anywhere in the circuit, so a floor is structurally required. The MN3009 datasheet — already anchored here for bandwidth and distortion — specifies **noise 0.2 mVrms max, A-weighted**. No calibrated hardware SNR or stereo-correlation measurement has been located | The per-line floor is that row. `independentLineRandomAmplitude` = `mn3009OutputNoiseAWeightedVrms / (nodeVoltsPerUnit · lineNoiseAWeightedTransfer)` = 1.90687e-4 in model units, where the transfer 0.4034 refers the datasheet's output-noise voltage back to the amplitude each line writes at its clock edges, through the hold, tap-summing pole, both reconstruction sections and the wet output coupling under A weighting. Separately parameterized common/correlated random, hum and clock-spur hypotheses remain; they default to zero, and the one Chorus Noise extension scales every component and can defeat them all | **Anchored to the part, inside a bracket the part's own datasheet leaves open.** The datasheet's two noise figures disagree by **10.5 dB**: 0.2 mVrms **max** A-weighted against the ~59.7 µVrms implied by **S/N 88 dB typ** at the 1.5 Vrms maximum input. The guaranteed maximum is chosen — it is the guaranteed figure, and anything near the other end is close to indistinguishable from the bit-exact zero the dry path renders — but that is a choice inside the bracket, not a derivation from it, and a later pass owns the bracket rather than rediscovering it as a contradiction. Two further caveats: the transfer is a property of this model's own filters and is stated at the 192 kHz HQ internal rate, reading 0.40 dB higher at 48 kHz with HQ off; and it lands the row at the *mixer's* wet input rather than at the injection node itself, whose own unweighted RMS is 3.12 dB above the recovered figure. The optional common/hum/spur amplitudes, spectra and correlation stay **voiced** and still need OQ-03's calibrated same-path stereo capture |
| Chorus noise, mode dependence | Two reported chorus-noise measurements, on Panasonic and on Xvive parts, both put mode II's floor 3.95 dB above mode I's. The settled p. 15 topology gives the two modes identical sweep depth and clock range: the mode line changes the modulation rate and nothing else | An off-by-default `enableChorusRateNoise` scales each line's own random floor with its modulation rate, referenced to mode I. Mode I is therefore untouched and mode II rises by `20·log10(1.6234799) = 4.2089 dB`; the rendered idle floor measures 4.2225 dB. The optional common/hum/spur layers stay on the plain Chorus Noise master | **A named candidate, not a mechanism.** The prediction lands 0.26 dB from the measurement and is chip-population independent, which matches the delta's strongest recorded property — but the measurements carry no reference tone, level or bandwidth, so they cannot anchor an amplitude law. OQ-03's calibrated same-chain capture owns this; a capture at a third, artificial rate would separate rate-proportional noise from mode-switch-network noise directly. Default off, so the shipped sound is unchanged |
| Cascade elementary functions | Not a circuit claim. The implicit solve evaluates `tanh` for each differential pair and `ln cosh` for each path average, tens of times per stage per internal sample | Both come from one shared exponential: with `e = exp(-2|x|)`, `tanh x = sign(x)(1-e)/(1+e)` and `ln cosh x = |x| + ln(1+e) - ln 2`, and `ln(1+e)` on `[0,1]` uses `2 atanh(e/(2+e))` with a double-precision core. The path-start antiderivative is evaluated once per stage per call rather than once per Newton iteration | **A numerical product mechanism with no hardware counterpart.** It computes the same functions the model has always used; `testCascadeKernelsMatchTheStandardLibrary` fences agreement with `std::tanh` and `std::log1p` at one float ULP, and against the `std::log1p` form it replaced. No constant, level, corner or law moves |
| Cascade convergence test | Not a circuit claim | The Newton step test is `1.0e-6 * (1 + max\|V\|)` rather than an absolute `1.0e-7`, which single precision cannot resolve on volt-scale capacitor states. The eight-iteration cap is unchanged | **A numerical product mechanism.** The former threshold was unsatisfiable wherever the filter was working — 7.99 iterations of 8 at resonance 0.95 — so the loop stopped at its cap rather than at convergence; its remaining step there measured a mean of 5.1e-5 V on states averaging 1.7 V. Because only the *stopping* rule changed and not the cap, the worst-case residual cannot grow. `testCascadeSolveStopsWhereItConverges` bounds the residual of the four stage equations, computed independently by quadrature on `std::tanh`, at 2e-4 V |
| Chorus support and coupling filters | Service-note component values show two emitter-follower Sallen-Key low-pass sections before and after each BBD, an extra passive input pole, a wet-input coupling high-pass, an output tap-summing pole, and C28/C25 wet-output coupling into the mute/summer loads | Two Sallen-Key sections at 9.69 kHz/Q 0.549 and 10.38 kHz/Q 1.291 on each side; R122 10 kΩ with C52 2.2 nF gives the 7.23 kHz input pole; C44/C47 0.1 µF with R120/R114 100 kΩ gives the 15.9 Hz wet-input high-pass; `(3.3 kΩ || 47 kΩ) × 2.2 nF` gives a nominal 23.46 kHz tap-summing low-pass. The numerical output-step reconstruction is inserted before this tap pole, not in place of it. With TR11/TR12 open (wet muted), C28/C25 see 22 kΩ (R103/R81), nominally 7.234 Hz; conducting puts R72/R74 39 kΩ in parallel, nominally 11.315 Hz | The component topology and two low-frequency output loads are **anchored** — the 2026-08-07 p. 15 read confirms every support-filter capacitor code on the 106's own board (820 pF/680 pF and 1.8 nF/270 pF on both sides of each BBD, 22 kΩ pairs throughout, 10 kΩ/2.2 nF input poles, 3.3 kΩ taps into 47 kΩ/2.2 nF) — at ideal-source boundaries. The polyBLEP is a separate product reconstruction and supplies no evidence for these physical filters. MN3009 output impedance and emitter-follower source impedance remain OQ-04; TR11/TR12 on-resistance, leakage and switching remain OQ-20. The 23.46 kHz pole is explicitly **provisional** because it assumes an ideal active MN3009 output; the 2026-08-07 solve derives Rs ≈ 3.70 kΩ for the summed output pair from the Gi–RL panel, spanning loaded-pole candidates 11.9/15.1/22.2 kHz depending on the unresolved per-leg topology — recorded against OQ-04, not silently retuned |
| Oversampling | Standard practice for nonlinear audio | The complete voice, filter, amplifier and both delay lines run at 4x for host rates below 88.2 kHz, 2x below 176.4 kHz, and natively above, followed by a 63-tap Kaiser (β = 7.857, the standard 80 dB design) half-band per stage. Filter/VCA audio coefficients update at every internal sample, so their wall-clock bandwidth does not change with HQ. A requested live rate change waits for voices and musical tails, then a block-size-independent 5 ms fade brackets rebuilding sample-grid histories. Converter/LFO/DCO phases, BBD buckets/clock/RNG state and C14/C12/C17/C20 coupling states survive; OTA carries are retimed, while chorus support-filter carries and the BBD's grid-specific polyBLEP correction slots clear at zero gain | Genuine internal oversampling with filtered decimation, not a quality label. Reported latency is the deepest path's real group delay and shallower modes are padded to keep it constant. Without HQ, physical BGA can fold according to the modeled BBD/support chain while polyBLEP reduces the additional SGA; HQ moves that numerical boundary and the filtered decimator defines what reaches the host. Neither mechanism is described as deleting all physical BGA. The transition fade and selective numerical/support reset are click-prevention product policies, not reference-unit behavior |
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
   node in volts, and C56/C50 AC-couple that node into the voice module so no
   mixer DC — chiefly the pulse's duty-dependent mean — reaches the filter or
   the VCA behind it.
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
   supply the switch-loaded 7.23/11.31 Hz wet-output coupling. Each output
   channel then sums dry at `100/47` with its own wet line at `100/39` in IC6.
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
   The elapsed-time accumulator is wall-clock seconds in double precision
   *(2026-08-08)*: it is advanced once per internal sample, so a float total
   stalled on a power-of-two boundary set by the internal rate — at 128.0 s
   with HQ on and 512.0 s with it off, freezing the modelled chassis at
   $26.99^\circ\text{C}$ or $31.51^\circ\text{C}$ and the headroom 2.5% or 1.0%
   short of the $6.5687\ \text{V}$ the warm curve asks for. In double the curve
   runs to completion and reads the same at every rate and quality setting.

2. **Voice-VCA input-offset thump — unsupported heuristic removed** *(2026-08-06)*:
   Service Notes pp. 13 and 18 establish a real per-card null: C59 AC-couples
   VCF OUT pin 3 into VCA IN pin 9, VR30 injects a DC correction into that signal
   node through R112 2.2 MΩ, and the six corresponding trimmers are adjusted at
   TP8–TP13 for minimum thumps. The R106/C58/R105/Tr20 branch that drives VCA
   CONT pin 11 is separate. The one shared uPC1252H2 sits after the voice sum and
   therefore cannot be the source of per-note ENV/GATE thump.

   The former implementation inserted
   $(0.0008+0.002\,\text{cardOffset})\,\text{control}$ before a VCA gain that is
   itself approximately proportional to control. It therefore created an
   unsupported control-squared pulse, imposed a positive 0.8 mV bias, and reused
   an unrelated control-hold spread as the VR30 signal-input trim. A calibrated
   unit's residual magnitude, polarity and spectrum are not published, so the
   nominal model now adds none. C14 would turn a changing card-output DC error
   into a decaying low-frequency pulse downstream; it does not supply the missing
   residual measurement.

   **C59 itself is now modelled** *(2026-08-08)*. Until then the renderer went
   straight from the cascade's fourth capacitor voltage into the VCA multiply,
   so the DC the filter core makes of its own — the stage offsets inside the
   loop, and a duty-asymmetric pulse the cascade only partly removes — was
   multiplied by the envelope and left a duty-dependent sub-audio thump. On a
   MIDI 48 pulse patch at CUTOFF 0.30 / RESONANCE 0.75 the pin 9 mean over the
   settled sustain ran +0.0428 V at 50 % duty and +0.0298 V at 94 % duty, and
   the peak through a four-pole 20 Hz low-pass over a note-on/note-off cycle
   rose 24.9 dB relative to broadband RMS across that duty range. With the
   capacitor in place the pin 9 mean is under 0.2 mV at every duty. The
   capacitance is the anchored read; the load it works against is voiced, and
   the audible content is the DC block, not the corner.

   The strict [voice-VCA feedthrough comparison](audio/realism-comparisons/voice-vca-feedthrough/README.md)
   opens all six otherwise silent cards at fixed scan phase. Raw peak falls from
   −68.24 to −148.42 dBFS; the disclosed +30 dB files are fixed diagnostic
   magnifications, while the raw float32 files preserve actual level. OQ-19 owns
   a DC-coupled, pre/post-null installed-unit capture before any residual returns.

3. **Power Supply Rail Droop & Inter-Voice Coupling**:
   Active polyphonic voice current draw loads the $\pm 15\text{V}$ linear voltage regulators, inducing DC rail droop ($\Delta V_{rail} \propto \sum I_{voice}$). Rail droop modulates the VCF cutoff reference across all active cards, creating organic inter-voice glue under heavy polyphonic loading. The load follower runs at one wall-clock rate, so the quality setting cannot change how fast the supply responds.

   It does **not** modulate DCO tuning, and must not be made to: pitch is an integer division of a crystal-derived clock, so no rail deviation can move it. See the Note timer row above.

   Mains ripple is deliberately **not** modelled, and this is a derived result rather than an unmeasured gap. Service Notes p. 16 gives a $3300\,\mu\text{F}/35\text{V}$ reservoir per rail behind a 1B4B41 bridge on a $19\text{ V}_{rms}$ / $0.25\text{ A}$ secondary, so the unregulated ripple is $I/(2 f C) \approx 0.76\text{ V}_{pp}$ at 50 Hz and $0.63\text{ V}_{pp}$ at 60 Hz at full rated load. The M5230L regulators that follow reject roughly 60 dB of it, leaving about $0.7\text{ mV}_{pp}$, or 50 ppm of 15 V, at a card. Through the modelled 35 counts/V cutoff transfer that is 0.03 cents; as amplitude modulation of the DCO ramp it is $-86\text{ dB}$ sidebands. Modelling it would be modelling nothing.

4. **Voice mixer node — constant loading, sources mute** *(corrected 2026-08-07)*:
   The four-switchable-100 kΩ-legs-into-68 kΩ Thévenin model this entry used to describe is falsified at designator level by the p. 13 read. Saw and pulse leave the waveshaper already summed on **one** per-voice WAVE output (IC12/IC8/IC4 pin 14 or 16); the sub joins that line through R101/R97 27 kΩ behind D6/D5 from its own switch transistor; the shared noise rail arrives on its own leg; and C56/C50 10 µF NP couple the node into the voice module's input. No panel switch reaches this node: p. 9's text puts SAW on a control rail at the generator ("0: saw ON" at Tr24/R148 47 kΩ), PULSE on the −0.8 V comparator hold, SUB on its collector supply and NOISE on the level OTA. **Sources mute; legs never switch**, so the node's loading is one configuration-independent constant that `filterInputAttenuation` and the output reference absorb.

   The earlier model's history is kept for the record: SUB/NOISE legs were first counted only above zero (a 2.95 dB step on a continuous control, since held under 0.5 dB by `Tests/YouKnow106EngineTests.cpp::testMixerLevelIsContinuousInSubAndNoise`), the loading was once gated by Unit Character (inverting polarity above 1.8), and until 2026-08-07 the phantom switchable pulse leg attenuated any both-waveforms patch by 1.76 dB relative to a plain saw patch. A plain saw patch keeps its established absolute level throughout.

   **C56/C50 ship as of 2026-08-07.** The coupling this entry has named since the p. 13 read is now in the signal path: each voice's summed WAVE node reaches pin 1 VCF IN through a 10 µF NP capacitor, so no mixer DC reaches the filter core or the voice VCA behind it. It matters most for the pulse, whose comparator mean walks with duty — 6 V·(2d − 1), or 5.4 V at 95 % — and which the cascade (carrying no DC-blocking term of its own) had been passing into the VCA multiply, leaving a note-on thump that *grew* about 10 dB as PWM deepened. With the capacitor the trend reverses (−22.5 → −37.0 dB across depth 0 → 1) and the steady AC level is unchanged: a thump removal, not a tone change. The capacitance is the designator-level read; the resistance it works against is **not** — the same pass re-roles R99/R102 33 kΩ as the sub-emitter DC bridge — so the 33 kΩ shipped here is a voiced stand-in by analogy with C14/R39 and C12/R36, and any plausible 10–100 kΩ termination puts the corner between 0.16 and 1.6 Hz, far below the lowest note. Fenced by `Tests/YouKnow106EngineTests.cpp::testModuleInputCouplingKeepsMixerDcOutOfTheVoiceVca`.

   What the page does **not** yet establish is the loaded level budget — the WAVE output's source impedance and the exact termination of the summed node are unresolved in the available scan — so the relative source amplitudes and the 0.40 coordinate stay voiced under **OQ-15**. (The 33 kΩ/39 kΩ chain is no longer among the unknowns: the 2026-08-07 complete-scan pass killed the "toward the saw on/off rail" reading — the MC5534's own pin 17 carries the saw gate — and resolved the pair into the 33 kΩ sub-emitter bridge and the 39 kΩ noise leg.)

5. **BBD transfer-loss clock law — corrected** *(replaced 2026-08-06)*:
   `transferLossStep` runs once per modeled BBD shift (one fCP period). A fixed discrete-time
   coefficient therefore already puts its pole at a fixed fraction of
   $f_{clk}$: the absolute pole moves with the swept clock, while the response
   at a fixed normalized frequency $f/f_{clk}$ stays constant. Coefficient
   0.8654743 supplies the residual loss left after the explicit zero-order hold
   and gives −3.000 dB versus DC for the raw held output, upstream of numerical
   reconstruction, at 40 kHz clock and 12 kHz signal. The datasheet references
   0 dB at 1 kHz, on which basis that node is −2.972 dB: within 0.03 dB, with no
   placebo retune applied.

   The removed expression
   $\alpha(1+(f_{clk}-26000)\,1.5\times10^{-6})$ applied clock scaling a second
   time. It produced −2.757 dB versus DC, or −2.732 dB versus 1 kHz, at the
   adopted condition and moved the
   normalized 0.3-cycle response from roughly −3.04 to −2.14 dB across the
   model's 23.9–77.1 kHz sweep, creating unsupported LFO-correlated brightness.
   This correction restores internal units and the anchor; it does **not** claim
   that physical MN3009 charge transfer is invariant versus $f/f_{clk}$. The
   datasheet does show distinct low-resolution typical curves at fCP 10, 40 and
   100 kHz. By visual inspection their normalized corner does not move upward at
   faster clocks, so they do not qualitatively support the removed brightening;
   OQ-04 requests quantitative extraction and an installed-unit, de-embedded
   sweep rather than inventing a slope from them.

   A phase-coherent clock-feedthrough spur remains parameterised but ships at
   zero amplitude with the other unmeasured chorus-noise components pending
   OQ-03. The raw asynchronous hold is where the genuine clock images originate;
   the following numerical reconstruction is characterised separately below.
   No unsupported spur level is enabled.

   The strict [BBD transfer/clock-law comparison](audio/realism-comparisons/bbd-transfer-clock-law/README.md)
   traverses both modeled clock extremes in Chorus I and II, retains raw float
   output and uses one shared listening gain. It measures this implementation
   change, not a quantitative fit to the datasheet's multi-clock curves or an
   installed-unit response.

5b. **BBD host-grid alias reconstruction — deterministic numerical correction** *(2026-08-06)*:
   Gabrielli, D'Angelo and Squartini separate the aliases caused by a BBD's own
   clocked sampling, **BBD-generated aliasing (BGA)**, from the additional
   **simulation-generated aliasing (SGA)** caused when asynchronous output
   steps meet the fixed sample grid of a computer simulation. Their polyBLEP
   result motivates this mechanism, but is not transplanted wholesale: their
   experiment is an ideal, linear, noiseless 4096-stage MN3005 at 44.1 kHz;
   YouKnow106 models a nonlinear, noisy 256-stage MN3009 at multiple internal
   rates. Their reported SNR changes are therefore literature context, not
   product claims.

   The engine applies a compact polyBLEP to the deterministic held-step delta
   after `transferLossStep` and before the provisional tap-summing pole. The
   correction scheduler is fixed and bounded: 54 slots are allocated and the
   200 kHz clock at an 8 kHz processing grid, the tested worst case, consumes
   50. Multiple clock edges in one internal sample are accumulated rather than
   dropped. Enabling it leaves buckets, read index, clock phase, transfer state,
   held noise and RNG sequence unchanged. Noise is deliberately uncorrected;
   the correction slots, unlike the physical BBD state, clear on a change of
   internal rate because their timestamps belong to the old grid. A noise-on
   fixed/swept HQ/LQ PSD and audition fixture remains OQ-03 work; “resolved”
   below refers only to deterministic host-grid steps.

   A counterfactual RNG-lookahead measurement explains why noise was not folded
   into this change. With Chorus Noise 1 and silent input, reconstructing every
   provisional edge-held random jump changed 20 Hz–20 kHz RMS by only **−0.05 to
   −0.06 dB in HQ** (aligned difference about −82/−81 dBFS). LQ changed by
   **−0.83 to −1.04 dB**; its 15–20 kHz band fell 3.29–4.07 dB, but that band was
   already below −96 dBFS. Since a real MN3009's noise can mix clock-held charge
   noise with continuous device/output noise, correcting all of the hiss would
   assert an unmeasured source mechanism for essentially no default-HQ benefit.
   The pure-RNG lookahead is feasible but intentionally not shipped pending the
   calibrated OQ-03 capture. (Update, 2026-08-08: the hiss's *amplitude* is no
   longer voiced — it is the MN3009's own noise row, see the chorus-noise entry
   in the table above — but its *mechanism*, an edge-held random, still is, so
   the reasoning here stands unchanged. Every absolute dBFS figure in this
   paragraph now sits 14.39 dB lower; the relative measurements do not move,
   because the whole path from the injection point is linear.)

   Isolated-core tests measure SGA reduction of **36.2873 dB at 50 kHz** and
   **42.6752 dB at the 90 kHz multi-edge case**. At 10 kHz, excluding the wanted
   `k·Fclock ± f` image bins, the improvement is **25.0819 dB**. Those are this
   implementation's deterministic numerical results, not the paper's numbers
   and not a hardware measurement.

   BGA preservation is measured rather than assumed. Through a complete line at
   44.1 kHz LQ, the components at 9.216, 10.784, 19.216 and 20.784 kHz move by
   **−0.3405, −0.6039, −4.5851 and −5.9408 dB** respectively. At the default
   176.4 kHz HQ internal rate their deltas are **−0.0016, −0.0030, −0.0281 and
   −0.0382 dB**. Thus HQ is effectively transparent to these wanted images;
   LQ is a documented quality compromise, especially for higher-order BGA.

   The strict [BBD host-grid alias comparison](audio/realism-comparisons/bbd-host-grid-alias/README.md)
   uses the minimum clock and a 2.093 kHz probe. Its wanted image changes by
   **−0.0383 dB in HQ**. The LQ wanted-bin change is **−5.2986 dB**, but that bin
   was already **−100.47 dBc** in the baseline. The two false LQ second-image
   folds improve from **−26.87/−27.42 to −55.23/−53.61 dBc**; in HQ, roughly
   **−116 dBc** folds move to about **−171/−170 dBc**. Across the complete
   concatenated listening demo the signed after-minus-before difference is
   **−15.95 dBc peak and −27.66 dBc RMS**, at one fixed gain. These files are
   controlled before/after evidence, not a subjective listening test.

   Whole-demo user CPU time moved from **67.04 to 67.15 s**, about **0.16%** and
   within run noise. A pure Chorus benchmark exposes the local cost:
   **+16.6% HQ, +18.3% LQ and +27.9%** in the worst 8 kHz-grid condition. The
   bounded memory and measured cost are product-engineering evidence only.

6. **IR3109 control-current saturation — the upper cutoff knee** *(replaced 2026-08-06)*:
   The transconductor's control current saturates internally at about $700\,\mu\text{A}$, which on this circuit's own $C = 240\text{ pF}$ / $R = 68\text{ k}\Omega$ test condition is a pole near $64\text{ kHz}$. That, not an arbitrary cap, is where the cutoff stops following the anti-log converter, and it is consistent with Roland's published 50 kHz top. Modelled with the generalized algebraic clip the output summer and the BBD write already use, $y = x/(1+|x/64\text{k}|^{1.7})^{1/1.7}$, with the exponent fitted to a measured code-to-frequency table for a real voice card.

   Like the output summer's rails, this is a property of the part and is **not** scaled by Unit Character.

   What it replaces was a single pole, $\text{rawHz}/(1 + \text{calibration}\cdot \text{rawHz}/120000)$, attributed to anti-log emitter resistance. That shape cannot describe the measured knee at either end of the control: at Unit Character 1 it left the model **143 cents flat around a 16 kHz cutoff** and 48–90 cents flat from 5–9 kHz, which is inside the musical range; at Unit Character 0 it was instead **292 cents sharp** at DAC 3584, because the correction it needed was gated off. The replacement is under **5 cents** anywhere below 2.7 kHz and within **30 cents** of the measured card at every published point. `Tests/YouKnow106CircuitTests.cpp::testCutoffControlLaw` asserts both halves.

6b. **R-2R converter integral non-linearity — reinstated where it belongs**:
   The same measured table documents excess steps of $-4.64$, $+23.31$ and $-4.48$ cents at DAC codes 1024, 2048 and 3072: an R-2R ladder's major-carry error, exactly where it physically belongs. This project modelled it once and removed it correctly — the implementation wrote a transient impulse into `voice.cutoffCountsTarget`, a field the same converter write reassigns, so it measured $-360$ dBc and was bit-identical (see entry 22). **The mechanism was real; only its placement was wrong.**

   It is now a persistent offset applied by the converter write itself, on the code it just produced, so it reaches the hold capacitor and the filter. A slow cutoff sweep crossing mid-scale steps by about 23 cents, as a real card's does. Scaled by Unit Character, because an ideal ladder has no carry error at all: the magnitude is resistor matching, which is a tolerance.

7. **TA75558S IC6 Output Summer Rail Bound**:
   The output summer runs on $\pm 15\text{V}$ rails and cannot drive past them. Modelled with the same generalized algebraic clip as the BBD write, $y = x/(1+|x/L|^n)^{1/n}$, at $L = 13.5\text{ V}$ and $n = 8$: numerically linear through the few volts the stage actually carries, bending only as it approaches the rail.

   Unlike the tolerance mechanisms, this is **not** scaled by Unit Character. A freshly calibrated instrument has exactly the same rails, so a "pristine reference" whose output stage could swing to infinity would be the less faithful model. It replaces a $\tanh$ at the same asymptote that was applied *only* when Unit Character was above zero, which had two separate problems. A $\tanh$ has no linear region — its distortion rises as $(V/L)^2$ from the first millivolt, putting roughly 0.3% third harmonic on every sample at an ordinary 2.6 V node swing, where a TA75558S is specified far below that. And because the reference model skipped it, Solo Unison rendered a 15.7 V peak out of an op-amp on 15 V rails; it now stops at 11.7 V. `Tests/YouKnow106CircuitTests.cpp::testOutputSummerIsLinearBelowItsRails` asserts both halves: third harmonic below 0.05% at the nominal coordinate, and no input of any size escaping the rail.

   The *existence* of the bound is anchored by the supply rails. Its exact value remains OQ-05: 13.5 V is the rails less a typical saturation voltage, not a loaded measurement, and the downstream digital boundary still adds no limiter of its own.

8. **TR11/TR12 2SK30A wet-mute switches — no modelled distortion, by derivation**:
   Conducting, a 2SK30A's few hundred ohms sit against IC6's 39 k$\Omega$ wet input, so it drops about 1% of the signal and sees some 30 mV across itself at full level. Ohmic-region channel resistance moves by roughly $V_{ds}/2|V_p - V_{gs}|$, about 0.7%, and that reaches the output only through the same 1% divider — so the distortion is on the order of 0.007%, or $-83$ dBc.

   A revision modelled $1 - 0.015\tanh(v^2)$ instead: 1.1%, or $-39$ dBc, applied to every wet sample. That is some 44 dB more than the part can produce, and it was always on rather than only during switching. Both it and its companion gate-injection placeholder are removed. The switching transient and leakage remain **OQ-20** and are deliberately not invented; the 5 ms wet-mute glide is declared plug-in declick policy, not a device measurement.

9. **CMOS 4013 sub-oscillator driver asymmetry — removed, as inaudible by derivation and misapplied**:
   The 4013's P and N channel drivers really do have unequal rise and fall times ($t_r \approx 25$ ns against $t_f \approx 15$ ns). That is an edge-*timing* asymmetry, and it is nothing: 10 ns of skew against the sub's period is $3\times10^{-7}$ of a cycle at the bottom of the 16' range, rising only to $5\times10^{-6}$ near the top.

   What the model applied instead was an *amplitude* asymmetry — the two divider levels made 0.3% unequal — which is a DC offset and even harmonics at roughly $-50$ dBc. That is not the named mechanism, and it is five orders of magnitude larger than the named mechanism can produce. The divider's two levels are symmetric again. Modelling the real effect would mean moving the edge, and the distance to move it is below any sample grid the engine will ever run on.

10. **Re-strike VCA jump — removed as topologically impossible**:
    A previous revision added as much as `0.08` of full VCA control directly in
    `initialiseVoice` whenever a sounding card was reassigned. That is 0.8 V on
    the service chart's 0…+10 V VCA-CV range, applied synchronously with host
    MIDI and before the card's converter slot. No wire in the instrument joins
    the key assigner to the hold capacitor: the B-2 envelope retains its live
    accumulator on retrigger, and its new value reaches the analogue hold only
    through the ordered D/A scan. The direct jump is therefore removed.

    Real CD4051 charge injection remains possible, but it occurs when the mux
    switches and has magnitude $\Delta Q/C_{hold}$; neither quantity has been
    established. It remains OQ-07 instead of being replaced by an audible
    guessed pulse. The focused float32 comparison under
    `Docs/audio/realism-comparisons/retrigger-release-tail/` preserves the old,
    corrected and signed-difference signals at one shared listening gain.

11. **IR3109 VCF Stage-Space Transistor Input Offset Voltages ($V_{os}$)**:
    Differential pair BJT transistor input offset voltages ($V_{os} \approx 1.5\,\text{mV}$) across the 4 IR3109 transconductor stages break 4-pole differential symmetry, creating stage-dependent dynamic DC shifts and asymmetric soft distortion under high-resonance filter sweeps.

11b. **IR3109 integrating-capacitor tolerance (staggered poles)**:
    Each of the four transconductor sections integrates into its own 240 pF capacitor, and those are discrete parts that nothing trims into agreement. Driving all four sections from one shared $g$ places all four poles at exactly the same frequency — a coincidence no real four-section filter achieves, and one that makes the resonance peak more symmetric and the self-oscillation purer than the hardware's.

    Each stage now carries its own transconductance scale, drawn per card from the same deterministic seed as the stage offsets and applied as $1 + \varepsilon_n \cdot 0.02 \cdot \text{calibration}$. Like the offsets, the draw is signed and unbiased: there is no nominal mismatch, so at Unit Character zero all four collapse to exactly unity and the closed-form $1/(4-k)$ check still holds. `Tests/YouKnow106EngineTests.cpp::testFilterPolesAreStaggeredOnlyByUnitCharacter` asserts both ends.

    The $\pm 2\%$ half-span is **voiced** under OQ-10 — the ordinary class for such a part, not a measured population. The resonant-sweep listening take moves by $-16.3$ dBc, by far the most of the seven, which is where staggered poles should show.

12. **TA75558S IC6 Output Summer Op-Amp Dynamic Slew-Rate Limiting**:
    Dual op-amp TA75558S finite maximum slew rate ($\text{SR} \approx 1.7\,\text{V}/\mu\text{s}$) imposes a dynamic rate-of-change limit ($\Delta V_{\text{max}} = \text{SR} \cdot \Delta t$) on output signals, naturally rounding off extreme high-frequency transients and resonant spikes to eliminate digital harshness.

13. **MN3009 BBD storage-capacitance non-linearity — removed, as double-counting**:
    The MN3009's P-channel storage capacitance really is voltage dependent, $C_{gs}(v) = C_0/\sqrt{1 + |v|/V_{\text{bi}}}$. What that produces at the terminals is distortion plus a little level-dependent high-frequency loss — and the datasheet's distortion figures (0.3% at 0.78 V$_{\text{rms}}$, 2.5% at 1.5 V$_{\text{rms}}$) are measurements of the complete part, with that non-linearity already in them. `Chorus::bbdTransfer` is fitted jointly to both. A separate $C_{gs}$ term on top counts the same physics twice.

    The implementation was also the wrong mechanism entirely: it scaled *both* delay lines by $1 - 0.015\,|v|/(2.6 + |v|)$, frequency-modulating the whole line with the rectified instantaneous input. That is ~14.6 µs of delay deviation on a 3.505 ms centre at full level, giving wet-path sidebands that rise 6 dB/octave — about $-27$ dBc at 1 kHz and $-13$ dBc at 5 kHz. Nothing in a bucket-brigade device moves the clock with the signal. (The units were wrong twice over as well: the input arrives in the 2.6 V-per-unit coordinate and was divided by 2.6 again.)

    Removing it changes the wet path by $-15.8$ dBc peak and leaves every other listening take at the measurement floor.

13b. **Voice VCA control law — topology narrowed, transfer still open** *(replaced 2026-08-06)*:
    The BA662 is a current-controlled OTA and Roland draws no intentional volts-per-decade converter in the VCA path. The external grounded-base branch is `VCA CV (0…+10 V from the S/H) → R106 10 kΩ → C58 node → R105 22 kΩ → Tr20 emitter`, base grounded, collector to pin 11 VCA CONT. This supports a quasi-linear compatibility law above conduction and rules out claiming an unseen exponential converter. It does **not** make `gain=(Vcv−Vbe)/32 kΩ` exact: the resistor-plus-BJT relation includes $V_{BE}(I)$, and the BA662's own $g_m(I)$ near cutoff remains unmeasured.

    `VoiceVcaControlLaw::gain` is therefore a smooth softplus approximation, not an exact solution. Its 150 mV onset comes from a circuit reconstruction and its thermal knee from an ideal BJT; neither is a measured Juno-106 transfer. The exact-zero deadband and separate retirement threshold are product policies.

    What it replaces was a much wider voiced profile: a 0.12 knee with a 260 dB-per-control-unit slope. Against this renderer that put **13–15 dB of extra attenuation on the bottom of every envelope** and strongly curved release tails. The narrower law is better aligned with the drawn current path, but remains compatibility until a real control-current/gain sweep chooses its onset and shape. The exact-zero deadband and separate retirement threshold are additionally product policies.

    A published teardown infers the opposite — *"the envelope generators are linear and generated by the CPU, so the VCA response must be exponential"*. That is an inference; the schematic excludes that intentional converter, but only measurement can settle the complete low-current transfer and whether firmware pre-shapes the DAC data.

14. **CD4051 multiplexer charge injection — removed, as unreachable and unquantifiable**:
    A CD4051's channel switches do inject $\Delta Q = C_{gd}\,\Delta V$ into the hold capacitor as the scan steps. The implementation added that to `cutoffCountsTarget` / `vcaControlTarget` — fields the same converter write assigns from scratch immediately afterwards, so it was overwritten every time and never reached the output. The isolated comparison render measured it at $-360$ dBc: bit-identical.

    It is not re-implemented, because its size cannot presently be derived. A physical injection lands on the *slewed hold state*, and $\Delta Q / C_{\text{hold}}$ needs the hold capacitance and the mux on-resistance — neither of which is established. Both belong to **OQ-07**, together with the question of what the supported 522/687 µs figures actually denote.

15. **JFET ramp-reset discharge shape — removed as implemented; open as future work**:
    The discharge really is an RC curve rather than a straight fall. What the model did, though, was apply a static half-wave-squared shaper to the *entire* ramp: `sawNaive -= 0.05 * max(sawNaive, 0) * sawNaive`. That never touches the negative excursion, so it did not soften the bottom reset corner it was named for; what it did was add about 1.25% second harmonic ($-38$ dBc) to every sawtooth at every pitch.

    The paired slope-residual corrections were hand-tuned rather than derived — the falling slope scaled by $(1 + 3\,\text{cal})$ and its partner by $e^{-4\,\text{cal}}$ — so the residual injected did not describe the discontinuity the naive signal actually carried, and left uncorrected alias energy on top of the harmonic tilt. The reset is back to the anchored straight fall, with both corner residuals taken from the shape being rendered.

    Doing it properly is **not worth doing**, and this entry used to imply the opposite. The measurement list below settles it: an exponential rather than linear reset is $2.2\,\mu\text{s}$ against a $2273\,\mu\text{s}$ period at A440, and the two shapes differ only above about 72 kHz — it is on the *measured as not audible* list, not the open-work list. It is written out here only so that a later pass recognises it as already answered rather than re-deriving it.

    Were it ever wanted, it needs the reset written into the phase-to-voltage map as $v(t) = 1 - 2(1 - e^{-t/\tau})/(1 - e^{-T_r/\tau})$, which lands on exactly $-1$ so both corners stay pure slope discontinuities and the residual amounts fall straight out; and it needs `pulseFallPhase` inverted through the same curve, since the comparator's falling crossing is currently solved against a linear fall and would otherwise disagree with the ramp.

16. **IR3109 VCF Transistor Early Effect Modulation ($V_A$)**:
    Models transistor Early Voltage ($V_A \approx 100\,\text{V}$) transconductance modulation $g_n = g\,(1 + 0.005\,\tanh(V_n / V_{\text{headroom}}))$ inside the 4-stage OTA cascade solver, introducing a small signal-dependent cutoff shift and odd-harmonic content under hot resonant sweeps. With $V_A \sim 100\,\text{V}$ and a few hundred millivolts of collector swing at the differential pair, the fractional change in $g$ is a few parts per thousand; the coefficient is now a named constant beside $V_A$ so the two cannot drift apart.

    A revision used 0.08 -- sixteen times the stated figure, and a signal-dependent cutoff shift large enough to hear as grit on every resonant sweep. Correcting it drops the isolated comparison render from $-16.6$ to $-40.8$ dBc peak, the 24 dB the ratio predicts. Rendered before/after comparison WAVs and isolated difference files are committed in [`06-vcf-early-effect-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-before.wav), [`06-vcf-early-effect-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-after.wav), and [`06-vcf-early-effect-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/06-vcf-early-effect-diff.wav).

17. **Voice Cards Spatial Chassis Thermal Gradient ($\Delta T_{\text{psu}}$)**:
    Models spatial heat dissipation across physical voice cards 1–6 based on physical proximity to the internal power supply transformer ($T_{\text{card}}(i) = 25^\circ\text{C} + \Delta T_{\text{ambient}}(t) + 4^\circ\text{C} e^{-(i-1)/2.5}$), introducing per-voice thermal headroom variation under polyphonic playing.

    **The cutoff half of this was ten times too large and disagreed with the
    temperature it was supposed to come from** *(corrected 2026-08-06)*. It was
    written as $1 + 0.04\,\text{cal}\,(i - 2.5)$, i.e. $\pm 165$ cents: linear in
    the card index while the temperature profile computed beside it is
    exponential in it, absent from the README's own Unit Character table, and
    roughly ten times what the model's own thermal computation supports — a
    4 °C gradient moves $V_t$ by about 1.3 %. The module board also carries R111,
    a 560 Ω *positor* (a PTC thermistor, listed as such in the parts legend)
    returning the CV divider node to ground specifically to cancel this
    temperature coefficient.

    It is now derived from the same exponential profile as the temperature,
    through the AS3109 datasheet's own $0.33\,\%/^\circ\text{C}$ cutoff tempco,
    and taken about the six-card mean because the FREQ trim is set with the
    instrument warm. That is about $\pm 10$ cents at Unit Character 1 — an upper
    bound on what the positor leaves rather than a measured residual (OQ-10),
    and coincidentally close to the $\pm 10.5$ cents a third-party project with
    six-card data ships as its own default. The per-voice trimmer residuals
    ($\pm 84$ cents of offset, $\pm 5\%$ of scale) are unchanged and remain the
    dominant per-card spread, as a trimmer residual should be.

    The isolated comparison render moves from $-7.2$ to $-30.2$ dBc peak. That
    is the size of the correction: this mechanism was previously the loudest
    thing in the whole comparison set apart from the chorus sweep, which is not
    where a 4 °C chassis gradient behind a dedicated compensating thermistor
    belongs.

    It reaches the OTA's linear span and the cutoff reference. It does **not**
    reach pitch. A revision multiplied each card's oscillator phase increment by
    a per-card thermal factor, spreading the six cards over 13 cents — audible
    as beating on every chord and, in Solo Unison, as a detune the instrument
    has no mechanism to produce. See "Tuning stability" below for the
    derivation. Rendered before/after comparison WAVs and isolated difference files are committed in [`07-spatial-thermal-gradient-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-before.wav), [`07-spatial-thermal-gradient-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-after.wav), and [`07-spatial-thermal-gradient-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/07-spatial-thermal-gradient-diff.wav).

18. **Chorus Heterodyne Clock Bleed**:
    Dual MN3009 BBD clock driver heterodyne beat frequency sidebands ($f_{\text{clkA}}, f_{\text{clkB}} \in [40\,\text{kHz}, 200\,\text{kHz}]$), injecting a small high-frequency tone into wet chorus modes. Off by default: the tone's amplitude is an unvalidated placeholder pending OQ-03, and no calibrated hardware noise reference has been located. (An earlier revision of this entry additionally claimed continuous-time fractional-delay/Thiran interpolation for the BBD taps; no such filter exists in the code -- the line still uses linear interpolation. Implementing a genuine Thiran allpass for the BBD read/write taps remains open future work.) Rendered before/after comparison WAVs and isolated difference files are committed in [`08-chorus-thiran-clock-bleed-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-before.wav), [`08-chorus-thiran-clock-bleed-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-after.wav), and [`08-chorus-thiran-clock-bleed-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/08-chorus-thiran-clock-bleed-diff.wav).

19. **Chorus MN3101 Current-Controlled Oscillator Hyperbolic Delay Sweep — now off by default, kept as the competing hypothesis**:
    Models Tr22 control current modulation into MN3101 clock driver ($f_{\text{clk}} \propto I_{\text{ctrl}}$), yielding a hyperbolic delay sweep with asymmetric pitch Doppler shifts. When enabled, the clock sweeps linearly between the two frequencies that correspond to the measured delay envelope's endpoints ($128 / (T_{\text{centre}} + \text{sweep})$ and $128 / (T_{\text{centre}} - \text{sweep})$), so the rendered sweep reaches exactly the measured endpoints at any amount of Unit Character rather than overshooting them (see OQ-01, which records the overshoot an earlier, centre-relative revision of this formula produced). The default flipped to **off** on 2026-08-06: the only delay-trajectory measurement in existence — KR-106's ~50-point click-timing series across a real 106's modulation cycle, 16 µs RMS residual against a straight line — reads the delay as *linear in time*, directly against this mechanism's frequency-linear assumption, and a linear flank also renders the instrument's characteristic constant detune where a bent flank slides through it. The measurement is below the anchoring bar (raw clicks unpublished), so the mechanism stays available behind `enableChorusHyperbolicSweep` pending the calibrated clock time-series OQ-01 requests. Rendered before/after comparison WAVs and isolated difference files are committed in [`09-chorus-hyperbolic-sweep-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-before.wav), [`09-chorus-hyperbolic-sweep-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-after.wav), and [`09-chorus-hyperbolic-sweep-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/09-chorus-hyperbolic-sweep-diff.wav).

20. **DCO ramp charging curvature — removed, as contradicting the anchored ramp**:
    The model subtracted a parabola $0.12\,(100/(f + 50))\,u(1-u)$ from the rising ramp, attributed to the finite output resistance of the current source charging the timing capacitor. That is a 3.6% sag at 32 Hz falling to 0.6% at A440 — a pitch-dependent even-harmonic tilt around $-30$ dBc on every sawtooth.

    The topology does not support it. The Ramp-generator row above has the compensation voltage driving a resistor into an *integrator's virtual ground*: the op-amp holds that node at 0 V, so the charging current is constant whatever the source's own output resistance is, and the residual curvature comes from finite open-loop gain — of order $10^{-5}$. A straight rise is also, as that row says, the only shape consistent with the comparator's anchored 6 V / 50% duty point, which a bowed ramp would move. The mechanism contradicted two anchored claims in this same document.

    It also changed the ramp's slope at both corners without any matching slope-residual correction, so it raised the alias floor as well as adding the tilt. Removing it moves every listening take by 25 to 40 dBc, except the high 4' lead where the $1/(f+50)$ law had already made it small.

21. **C14 Non-Polar Electrolytic Voltage-Dependent HPF Modulation**:
    Models non-linear capacitance variation $C(v) = C_0 / (1 + \alpha |v|)$ across voice-summing coupling capacitor C14 ($10\,\mu\text{F}$ non-polar electrolytic), allowing large low-frequency sub-bass voltage swings to dynamically shift the HPF cutoff corner and generate natural intermodulation "glue". Rendered before/after comparison WAVs and isolated difference files are committed in [`11-electrolytic-c14-nonlinearity-before.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-before.wav), [`11-electrolytic-c14-nonlinearity-after.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-after.wav), and [`11-electrolytic-c14-nonlinearity-diff.wav`](file:///Users/vojta/Dev/vst-instruments/youknow106/Docs/audio/sota-comparisons/11-electrolytic-c14-nonlinearity-diff.wav).

22. **R-2R converter major-carry glitch — removed as an impulse, reinstated as an offset**:
    Written into the same targets as entry 14, on the same write that recomputes them, and equally unreachable: the isolated render measured $-360$ dBc. Its code-transition detector was broken independently — `currentDacFraction` was only computed for the shared destinations, so it read zero for every VCF, VCA and pitch write, and the "major carry" test fired on almost every shared write instead of on genuine `0x7FF`$\leftrightarrow$`0x800` transitions.

    Removing it was right, but the mechanism was not the problem — the placement was. A ladder's integral non-linearity is a *static* property of the code, not an event at the transition, and it is now modelled as such in entry 6b, with measured sizes.

    The separate zipper texture the impulse was meant to supply still has no honest source: it would come from modelling each hold capacitor as tracking only while its multiplexer window is open and holding afterwards, which follows from the anchored 23-writes-per-4.2 ms scan. That remains blocked on **OQ-07** and is not guessed at in the meantime.

23. **BBD clock-scaled transfer-smear coefficient — removed as breaking its own anchor**:
    The charge-transfer pole's coefficient (entry 5's residual $-1.674$ dB, derived at the MN3009's 40 kHz datasheet condition) was additionally scaled per edge by $1 + (f_{\text{clk}} - 26\,\text{kHz}) \cdot 1.5 \times 10^{-6}$ — two constants classified nowhere, unity at a clock the docs never mention. At the datasheet's own 40 kHz the live line therefore rendered $-2.76$ dB where the part is specified $-3.0$ dB, and the response *brightened* with clock where per-transfer inefficiency physically worsens; the fixture could not see any of it because its seam omitted the clock argument and ran at the 26 kHz default. The multiplier is deleted rather than re-centred: the recursion advances once per clock edge, so a fixed per-transfer coefficient already moves the absolute corner with the clock, which is exactly what entry 5 derives and the datasheet anchors. The seam's clock parameter is gone with it, so the fixture now drives the precise function the line runs.

24. **DCO compensation scale as an outer waveform multiply — removed as an unbandlimited step train**:
    The momentary amplitude error a pitch step leaves (the slewed CV against the stepped count) was applied by multiplying the finished, BLEP-corrected saw — and, with no derivation chain at all, the pulse — by the instantaneous ratio. The ratio steps within one internal sample at every Pitch write, so the multiply stepped the whole waveform mid-cycle outside the bandlimited track: measured against the shipping library, a one-octave glide at portamento raw 32 rendered 23 single-sample steps of 2.89% in 120 ms — an unbandlimited AM click train at the 238 Hz scan cadence riding every glide, bend and deep vibrato, on top of the authentic frequency staircase. Physically the integrator charges from whatever current the CV set when the cycle launched: the error is the *slope of the rise*, it can only change where the discharge returns to the shared rail, and the value can never jump mid-cycle. The render now freezes the ratio per cycle and reshapes about the fixed bottom rail (`s·naive + (s−1)`, slopes and corner residuals scaled per cycle, the restart correction expressed in the same domain), which is value-continuous by construction, keeps the documented peak-error transient and its 522 µs recovery, and matches the bottom-pinned geometry the duty law already assumed. The pulse now carries the coupling only where the derivation reaches: its edge times.

*Note: All physical circuit simulation behaviors above scale dynamically with **Unit Character** (`calibration`): `0.0` suppresses every one of them for a pristine digital reference and `1.0` matches real hardware.*

The control is bounded at **2.0**. Every mechanism is written as
`nominal + (physical - nominal) * calibration`, which is a blend only on
`[0, 1]`; past that it extrapolates without limit, walking straight through the
nominal value and out the other side. The host range formerly continued to
100, to reach the exaggerated-for-contrast territory the comparison-rendering
tools used -- but those tools no longer touch this control, since each pair now
toggles a single mechanism with Unit Character held at its default, so the upper
range had no remaining customer and several real defects:

| Mechanism | Behaviour above 1.0, before the bound |
| --- | --- |
| Passive mixer loading | polarity inverted at 1.82 with four legs connected, reaching a gain of $-27.8$ at 100 |
| Output summer blend | gain $-0.22$ at 100 for a unit input, $-3.67$ at twice that |
| DCO reset shaper | returned $-4.0$ for a $+1.0$ input at 100, with one paired BLAMP slope scaled $\times 301$ and its partner by $e^{-400}$ |
| Ramp curvature | parabola 3.6 times the ramp's own amplitude |
| C14 coupling | pole moved from 0.82 Hz to about 7 Hz |
| Chorus sweep | delay extrapolated 100 times past the measured endpoints, pinning against the clock clamps |

Two still exaggerates every mechanism while leaving each blend on the same side
of its nominal value. The engine clamps to the same bound in `sanitise()`, so a
host automating past the range cannot reintroduce the behaviour above.

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

The Service Notes schematic read that previously blocked the nominal common-VCA
law is complete. The largest remaining circuit-document reads are OQ-01 and
OQ-04; unlike those, OQ-02 is now an installed-unit tolerance sweep. Their
locations and the historical three-read queue are recorded in
[where the blocking reads actually live](open-questions.md#where-the-blocking-reads-actually-live).

| Area | Canonical task |
| --- | --- |
| Absolute JUNO-106 chorus timing | OQ-01 |
| Installed stored-VCA component/rail/IC tolerance and endpoint validation | OQ-02 — *nominal byte/DAC/hold-network-to-GC1 law and C7 settling are now derived* |
| Calibrated chorus noise and SNR | OQ-03 — *the per-line amplitude is now the MN3009's own 0.2 mVrms A-weighted noise row rather than a voiced level; what remains open is where in the datasheet's own 10.5 dB bracket a real card sits, which node the row denotes, and the whole optional common/hum/spur layer. The rate-proportional candidate for the measured 3.95 dB II−I delta still ships behind its own off-by-default switch, so the hypothesis is testable without being asserted* |
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
| Hardware cutoff-converter saturation behind the transparent 50 kHz product cap | OQ-18 — *knee now derived from the part's own control-current saturation; a Roland-published or independently reproduced curve is what remains* |
| Measured BA662 control-current/gain transfer and calibrated pin-9-null residual thump | OQ-19 — *pins, separate paths, 6 Vpp endpoint and minimum-thump trim are anchored; numeric law and residual remain open* |
| Chorus wet-mute switching transient and leakage | OQ-20 |
| Coupled C14/switched-HPF transfer and mode-change memory | OQ-21 |

JUNO-60 findings may be retained only as labelled comparative evidence. They
cannot close a JUNO-106 task.

### 2026-08-06 evidence search and implementation measurement

A public-source evidence search plus an offline measurement of the shipping
algorithms is recorded in the
[2026-08-06 section of open questions](open-questions.md#evidence-search--2026-08-06).
No hardware was measured. Summary of what moved:

- **New anchored material.** The Service Notes specification page and complete
  ADJUSTMENT table were read. The specification page independently corroborates
  the cutoff range, all four modulation budgets and the LFO rate range. Two
  entries are new evidence: `AUDIO OUTPUT L −30 / M −15 / H 0 dBm` bears on OQ-06,
  and the `1.5ms to 12s` decay/release figure is recorded as an unreconciled
  discrepancy against the model's firmware-derived 16.8 ms – 25.55 s. The
  ADJUSTMENT table's **992 Hz at C6 VCF WIDTH point is a second cutoff anchor**
  two octaves above the 248 Hz one the model already uses, and the 4.8 Vpp
  self-oscillation amplitude is still asserted nowhere in the suite.
- **OQ-01 is closer to derivable than the code comment states.** The integrator
  capacitor the comment says the schematic does not print is reported as
  `C3 = 0.1 µF` on p. 15, pending confirmation. Independently, an arithmetic check
  shows `lfoThresholdRatio = 1/48` is inconsistent with the circuit by roughly 34×;
  `R7/R6 = 33/47` lands the derived rates within 3 % of third-party measurements.
  The mode ratio is unaffected and remains 1.6235 either way.
- **OQ-04 is quantified.** The adopted MN3009 table anchor is −3 dB at 12 kHz
  on a 40 kHz clock, relative to 1 kHz. Removing the duplicate clock multiplier
  makes the raw held node upstream of polyBLEP −3.000 dB versus DC and
  −2.972 dB versus 1 kHz; the former law gave −2.757 dB versus DC and
  −2.732 dB versus 1 kHz and imposed unsupported LFO-correlated brightness. The
  modelled support chain is still 12 dB down at 10 kHz, and removing the
  duplicated reconstruction pair does not reconcile it, so its corner values
  remain implicated. The sibling clone netlist now reads the output sections
  at the same part values as the input sections (three 22 kΩ/22 kΩ chains,
  820 pF/680 pF then 1.8 nF/270 pF), so the shared corners are
  family-corroborated rather than assumed — which sharpens rather than
  resolves the corner-value question, since both sides now stand or fall
  together against Roland's own p. 15 codes. This is coupled to OQ-01:
  wet-path bandwidth constrains the BBD clock — a five-pole ~10 kHz chain on
  both sides is coherent anti-alias design for a ~24 kHz minimum clock and
  needlessly dark for a ~43 kHz one — while quantitative
  extraction of the datasheet's 10/40/100 kHz curves and a multi-clock
  de-embedded installed-unit sweep are needed to establish the normalized law.
- **BBD host-grid aliasing is now separated from physical BBD aliasing.** A
  deterministic-only, paper-motivated polyBLEP suppresses the additional
  simulation-grid folds without changing buckets, transfer/noise or RNG state.
  The [strict comparison](audio/realism-comparisons/bbd-host-grid-alias/README.md)
  reports the large SGA reductions and the non-zero LQ BGA tradeoff; the method
  is a validated product mechanism, not a new MN3009 or listening-test claim.
- **OQ-02's nominal law is now derived.** A direct p. 15 schematic read identifies
  R30/C7/R32/R31/R165. Combined with p. 8's converter range, the ROM-resolved
  `d=b<<5` code and NEC's −5.9 mV/dB typical constant, it replaces the former
  cubic with `gain_dB=−16.3196647+0.165581014·b` and derives C7's 9.08249 ms
  settling. An installed sweep remains useful for tolerances and the explicit
  ideal-R-2R `/4096` assumption.
- **OQ-18 gains a measured comparison curve**, which confirms the exponential law,
  the 1143 counts/octave slope and the 50 kHz endpoint, and localises the error to
  the knee shape (up to 143 cents flat near a 16 kHz cutoff). The same source
  documents the R-2R carry non-linearity as a real ±4.6 / +23.3 cent effect — the
  mechanism this project removed was genuine, but belonged in the static
  code-to-frequency map rather than as an impulse into a field the next converter
  write overwrote.
- **Implementation measurements.** The in-band alias floor is set by the VCF's
  `tanh` set, not the oscillator: −111.5 dB for the oscillator alone against
  −55.5 dB for a bright resonant patch, improving to −87.7 dB with the filter run
  at twice the internal rate. `testAliasFloor` cannot observe this, since it runs
  at resonance 0, calibration 0 and stops at 20 kHz. Separately, five plausible
  refinements were measured and found inaudible; they are listed so they are not
  attempted again.

  **The −87.7 dB figure did not survive being built.** A doubled filter grid was
  implemented against the shipping engine and measured at −48.5 → −54.4 dB for
  about 40% of the whole engine's cost; it is not in the shipping code, and the
  same-day implementation pass in [open questions](open-questions.md) records the
  experiments that located the limit in the per-voice interpolation rather than
  in the grid. The half-band *window*, which that pass did change, was worth more
  for nothing: see the decimator entry there.

  **The mechanism itself was removed on 2026-08-07** without a grid at all:
  each stage's tanh is now averaged exactly along the straight drive path
  between steps (the ln cosh divided difference) instead of by its endpoints,
  which drops the same hot case's worst folded line by 11.5 dB for +11% of
  the filter's own cost, leaves the small-signal response and the
  self-oscillation limit cycle measured identical, and is fenced by
  `testCascadeDeniesTheFoldback`. The 2026-08-07 complete-scan entry in
  [open questions](open-questions.md) carries the full A/B.

Third-party forum and open-source measurements cited there are **not** promoted to
anchored, and the JUNO-6 chorus and ADSR data referenced remain labelled comparative
evidence under the rule above.

## Sources

The Service Notes themselves were finally obtained and read at page level on
2026-08-07: two byte-identical copies of the circulating First Edition scan
(JUL. 31 1984; printed pages 1, 5, 8–13, 15, 16, 18, 19) from
`synthfool.com/docs/Roland/Juno_Series/Roland_Juno_106/` and
`polynominal.com/roland-juno106/`, read as full-resolution crops of the
embedded 1-bit images. The Panasonic MN3009 and MN3101 datasheets
(`experimentalistsanonymous.com/diy/Datasheets/`) were digitised at 600 dpi
with axis calibration — the Gi–fi typical family, THD–Vi, Gi/THD–RL and
THD–fcp curves are extracted in the 2026-08-07 session record of
`open-questions.md`. The ModWiggler 106-chorus threads (t=111159, t=158257)
and the Gearspace chorus-noise thread (916126) were read in full with
post-level provenance, correcting one earlier misattribution and grounding
the promoted 1.4–6.4 ms sweep measurement.

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
window; Holters and Parker on bucket-brigade device modelling; and Gabrielli,
D'Angelo and Squartini's DAFx-25 paper
[“Antialiasing in BBD Chips Using BLEP”](https://dafx.de/paper-archive/2025/DAFx25_paper_29.pdf),
with its [companion code and audio](https://dangelo.audio/dafx25-bbd),
for the BGA/SGA distinction and output-step polyBLEP method. The latter studies
an ideal MN3005 and supports a numerical method family, not this MN3009's
physical response or the implementation-specific measurements above. No
third-party source code, netlist, ROM image, firmware or recording is included
in this repository. The one external functional-data corpus is the independently
decoded 2,304-byte factory tone memory described above; laws fitted through
published behavioural anchors are YouKnow106's own constructions.

# Ghostar open questions

Standing, research-ready tasks. Each entry names the evidence gap, what the
engine currently does, and what output would close it. Conventions follow the
repository's A–Z rules: a listening test may choose between defensible
candidates; it may not fit a number a measurement owns.

## OQ-01 — Pitch-bend wheel musical range

**Gap.** The bend network is anchored (100 kΩ pot across ±12 V, 680 kΩ into
the 100 kΩ-feedback CV summer; TUNE = 1 MΩ8 at ± a minor third), giving
≈ ±8 semitones at full *electrical* travel — but no source documents the
wheel's mechanical travel fraction of the pot. The 2023 reissue's MIDI kit
uses ±2 st, which is the converter's choice, not the wheel's.
**Engine.** Full wheel = ±8 st.
**Closes with.** A hardware measurement of wheel-end pitch offset, or an A–Z
test between defensible spans if none appears.

## OQ-02 — Absolute filter cutoff ranges

**Gap (span derived, placement still open).** The owner's manual says only
"through the audio range". The CEM3350 datasheet gives the frequency scale
as -19.6 mV/octave (-18.5/-20.6 window) and SM DWG 2's ladder is now read:
the panel summer reaches the chip's frequency pin through 12k1 (a second
12k1 carries the modulation bus), 16 kOhm to +12 V sets a fixed offset,
68 kOhm from a 100 kOhm trimmer's wiper places the window, and 274 Ohm
shunts the node; the integrator caps are 22 nF and IREF comes from 47 kOhm.
That gives 21.2 mV per volt at the pin, so the MASTER pot's plus/minus
10.5 V swing is plus/minus 11.4 octaves of authority over a chip window
about ten octaves wide - the hardware therefore sweeps from fully closed to
effectively wide open. What is *not* derivable is where that window sits:
the trimmer carries plus/minus 2.3 octaves of placement authority and no
document records its factory setting (the service manual has no
calibration text at all).

**Engine.** Upper MASTER spans 20 Hz-16 kHz exponentially - 9.6 octaves,
inside the derived ~10-octave span, and reachable at pin +226 mV to
+37 mV (voiced placement). LOWER ONLY spans -5 to +1 octaves relative to
Upper with coincidence at travel 0.8 (anchored coincidence point, voiced
span).

**Closes with.** A measured sweep of a hardware unit, which is now the
*only* thing missing: the span and the sensitivities are derived, and the
trimmer's setting is a per-unit calibration rather than a documented
constant.

## OQ-03 — Hardware pulse duty cycles

**Gap.** The panel prints 50/30/15/6 % (A) and 40/20/10/3 % (B); the actual
duties depend on ~100k/150k/250–270k divider chains plus a 100 k PW trimmer
whose factory setting is undocumented. Cherry Audio ships 8 % for A's
narrowest — possibly a measured unit, possibly a misprint.
**Engine.** The printed percentages, exactly.
**Closes with.** Scope captures from a calibrated unit.

## OQ-04 — 556A envelope segment curvature

**Gap (largely closed).** SM DWG 3 (P-1015), read at 600 dpi, gives the
whole circuit: one 556 half per envelope in the classic monostable ADSR -
OUTPUT pin through a series 1N4149 into the 2 MOhm log attack slider into
the shared 4.7 uF cap, threshold sensing the cap through 100 Ohm, the
discharge pin used purely as a phase-state logic output driving a 4066
that switches the decay slider onto a buffered linear sustain voltage, and
release through its own 2 MOhm slider and series diode. The timer runs on
a BC172 emitter-follower rail of about 11.35 V with its control-voltage
pin externally set and service-labelled +7.5 V, which is the envelope
peak and the top of both sustain sliders. "556A" is the Signetics NE556 in
the 14-pin "A package", not a different part.

**Engine.** Each travel maps to an RC *time constant*: 4.7 uF into
1 kOhm..2 MOhm gives 4.7 ms to 9.4 s, which is what the manual prints as
"5 milliseconds to 10 seconds" - one ~1 kOhm series-plus-end residual
lands both printed endpoints, where the previous three-time-constants read
fit neither. The attack charges from the OUTPUT pin through a diode, so it
aims at V_OH - V_D against the +7.5 V peak: a ratio of about 1.3 (the
documents bound it to 1.22-1.35, and the engine ships the nominal), giving
ln(1.3/0.3) = 1.47 time constants to peak rather than a rail-charged
monostable's ln 3 = 1.10. Sustain is linear 0..peak (anchored, and the
sliders hang from the same +7.5 V node). Retriggering resumes from the
current level with no capacitor dump, which is what the circuit does - the
discharge pin is not even connected to the cap.

**Still voiced / not modelled.** The ~1 kOhm slider residual behind the
fast endpoint; the aim ratio's 1.22-1.35 spread (no datasheet specifies a
bipolar 556's output-high drop at microamp loads, and the aim itself
varies across the travel); and the hardware's release *floor* - the series
1N4149 parks the release around 0.3-0.6 V, about 5 % of peak, with an
increasingly slow tail, and the sustain slider's bottom sits on a matching
diode, where the engine releases to zero. Modelling that floor needs the
downstream VCA and filter CV offsets, which this entry does not cover.

**Closes with.** An envelope scope capture, which would pin the aim ratio,
the slider residual and the release floor together.

## OQ-05 — Shaper Y gate behaviour

**Gap.** The Shaper produces its own gate via a comparator (SM DWG 3, net
"SG"), but the threshold and its behaviour per mode are not documented.
**Engine.** Gate is high while the Shaper output exceeds 1 % of full scale.
**Closes with.** A hardware trace of the SG net, or the Museo del Synth
Marchigiano's knowledge of the reissue.

## OQ-06 — Ring-modulator carrier bleed

**Gap.** The vintage unit has no ring-mod null trim (the reissue added one),
so carrier bleed is real, un-nulled and unit-dependent; the 1M8/6k2 OTA bias
sets it but no one has measured it.
**Engine.** 3 % of each input leaks into the product (voiced).
**Closes with.** A spectrum capture of RING with one oscillator silenced; the
bleed level is A–Z-able as a character choice until then.

## OQ-07 — Envelope-mode Shaper RATE span

**Gap.** OM gives the FREE-mode span (several cycles per minute to >20 Hz)
and says the same knob sets total rise+fall time in envelope modes, but
states no envelope-mode extremes.
**Engine.** One law for both: total period 20 s down to 45 ms across the
travel (matching FREE's stated extremes).
**Closes with.** Hardware timing of KBD HOLD rise at travel extremes.

## OQ-08 — Glide time constant

**Gap.** 2 MΩ pot is legible; the lag capacitor reads 420–470 nF in the
scan.
**Engine.** 450 nF equivalent: τ max ≈ 0.9 s, exponential taper.
**Closes with.** A cleaner scan or hardware measurement.

## OQ-09 — Upper-filter 24 dB cascade Q distribution

**Gap (structure corroborated, one digit short).** SM DWG 2 shows the
Upper chip's cascade section carrying its *own* fixed bias network on its
Q pin - a 220 Ohm shunt and a pull-up to +12 V - while the other section
receives the variable resonance bus. That is hardware corroboration of the
engine's fixed-Q/variable-Q split, which was previously a voiced guess.
The pull-up's digits are unresolved in the scan; if it matches the
neighbouring 91 kOhm, the fixed section sits at the same +29 mV as the
LOW-switch position, i.e. Q = 0.5.

**Engine.** The first section takes the LOW-switch Q of 0.5; the second
carries the resonance control. The CEM3350 datasheet says nothing about
cascading at all - its only cascade-adjacent material is the Synthesource
Winter-1981 newsletter's "standard 4-pole low pass" figure, whose
component digits are illegible - so the datasheet side of this entry is a
documented dead end.

**Closes with.** A cleaner scan resolving the cascade pull-up's digits, or
frequency-response measurements of a hardware unit in both slope positions
at matched resonance settings.

## OQ-10 — Overdrive knee and drive

**Gap (half closed).** The clipper is anchored in placement (an inverting
TL082 between the filters with an anti-parallel BA130 pair across its
feedback resistor, behind a 2k2 arm — the CEM3350 datasheet's own "Hi-Q
overload limiter" figure with its 1N914s substituted). The BA130's curve
is now **found**: the Fairchild 1978 Diode Data Book's BA128·BA130 sheet
(printed p.3-12) with the D4 family curves (p.4-6) specifies it down to
10 µA, and the digitised typical curve runs 99 mV/decade — ideality
n ≈ 1.68, saturation current ≈ 2.3 nA, so an anti-parallel pair obeys
`I(V) = 2·Is·sinh(V/(n·V_T))` with `n·V_T ≈ 43 mV`. What remains open is
the stage's *operating level*: the feedback resistor reads 33 kΩ at
600 dpi (the earlier record here said 330 kΩ), and either way nothing
states what an internal signal volt is.
**Engine.** The physical law, solved per sample (three Newton steps from
the smaller of the ohmic and diode-dominated asymptotes converge to
within ten parts per million): linear at `R_f/R_in` until the diodes
wake, then climbing about 0.1 V per decade of drive rather than
flattening onto a ceiling. The two level constants (volts per engine unit
in and out) are voiced, pinned so the stage keeps the small-signal gain
and ceiling the previous tanh had — so the *shape* is the whole of the
modelled change.
**Closes with.** A level trace of the stage, or distortion captures of a
hardware unit. The diode curve itself no longer needs anything.

## OQ-11 — No hardware measurements exist anywhere

Every quantitative behaviour beyond the manual's stated ranges rests on
schematic math. Nobody has published filter curves, envelope timings, drift
data, or a hardware-vs-emulation comparison for any Spirit. The first
measured unit becomes this project's ground truth; the Museo del Synth
Marchigiano (which reverse-engineered the 2023 reissue) is the most likely
living source of calibration data.

## OQ-12 — Resonance-path BA130 limiter constants

**Gap (mostly closed).** Self-oscillation is bounded by the external
BA130 anti-parallel "Hi-Q overload limiter" in the resonance path —
anchored in placement, and now known to be the CEM3350 datasheet's own
recommended circuit (its Figures 5/6, 1N914s substituted for BA130s,
33 kΩ feedback, 2k2, 470 Ω at the SLOPE switch). The BA130's curve is
found (see OQ-10): `I(V) = 2·Is·sinh(V/(n·V_T))`, `n·V_T ≈ 43 mV`,
`Is ≈ 2.3 nA`. **The travel-to-Q half of this entry is closed
outright** — see below. What is still open is the node's operating
level, which is what turns the diode's 43 mV into a number in the
engine's own units.
**Engine — the limiter.** A diode shunt in each section's band-pass
integrator equation: `v' = -lambda*V0*sinh(v/V0)`, solved as an exact
sub-step so the law is a rate, not a per-sample map. (The alias audit
measured the previous per-sample formulation converging to a *different
filter* at every sample rate, and its `4*tanh(0.25*x)` integrator bound
turned out to be the actual self-oscillation limiter - its always-on
cubic compression, not the diode knee, set the amplitude, with a strength
that scaled with the rate. That bound is removed; the engine suite renders
the regenerative extremes across rates to hold the boundedness claim, and
pins self-oscillation level agreement between hosts to 0.5 dB.) The sinh
form is now the pair's own anchored law; `V0 = 0.12` is `n*V_T` divided by
the untraced node scaling (0.12 implies about 0.36 V per engine unit,
where a buffered audio node sits), and `lambda = 1 /s` is voiced.

**Engine — the travel-to-Q law (closed).** Derived end to end. The
CEM3350's Q control is exponential at -65 mV per decade of Q (datasheet
(c) 1984; -62/-65/-68 mV window). SM DWG 2 gives the network: RESONANCE
is a 100 kOhm linear pot, top grounded, bottom at -12 V, feeding each
chip's Q pin through 18k2; each Q pin carries a 221 Ohm shunt against a
pull-up to +12 V - 91 kOhm at the Upper chip, 75 kOhm at the Lower, so the
two filters have different curves. The pot's own output impedance
`100 kOhm*t*(1-t)` sits in series and flattens the law through mid-travel.
The absolute anchor the datasheet lacks comes from the panel: at LOW the
pot is disconnected and the Upper Q pin rests at +29.1 mV, where OM says
Q = 0.5. Upper Q by travel: 0.51 / 0.94 / 1.48 / 3.33 / 10.9 / 82 at
0, .25, .5, .75, .9, 1; Lower: 0.41 / 0.76 / 1.19 / 2.68 / 8.78 / 66.
Full travel commands more Q than the chip holds (datasheet ceiling 30 min
/ 50 typ), and reading that ceiling as the point where the chip's own loss
is exactly cancelled reconciles the datasheet with OM's anchored
self-oscillation at maximum: `k = 1/Q - 1/Q_ceiling`. The ceiling value
(50, the datasheet typ) is the one voiced number left in the law.

**Closes with.** A level trace of the resonance node, which turns `n*V_T`
into engine units and pins `lambda`. The travel-to-damping mapping no
longer needs anything; a hardware Q-versus-travel sweep would now be a
*check* on a derivation rather than the derivation itself. Whether the
CEM3350's internal stages saturate on top of the external limiter, and how
exactly the hardware crosses from the chip's no-enhancement Q ceiling into
oscillation (no Figure-9-style enhancement path was spotted in the scan),
remain separate open questions.

## OQ-13 — Filter-tracking pivot note

**Gap (amount derived, pivot still open).** Tracking's *amount* is no
longer taken on trust: the keyboard's 1 V/octave bus reaches the chip's
frequency pin through the same 12k1 ladder as the panel CV, delivering
21.2 mV/V against the datasheet's -19.6 mV/octave, so full KB AMOUNT is
108 % [103-115 % across the scale-factor window] - independently
reproducing the manual's "slightly over 100 %" from the resistors. The
*pivot* - the note at which tracking contributes zero offset - is set by
the CV summer's reference, which was still not resolved from the drawings.

**Engine.** 108 % at full travel (derived), pivoting at middle C (voiced).

**Closes with.** A derivation of the tracking summer's reference from
SM DWG 2/3, or a two-note cutoff measurement on hardware.

## OQ-14 — Wheel modulation depths

**Gap.** The X and Y buses' full-wheel depths at each destination are set
by the mod board's summing resistors, which were not resolved from the
scan; the manual states no numbers. For Y→LFO RATE the manual anchors
only the behaviour (the wheel sets the fastest rate, the knob the
slowest), not the fastest rate itself.
**Engine.** Full wheel gives 1 octave of pitch, 3 octaves of cutoff, and
±0.42 of pulse duty (`pitchDepthOctaves`, `filterDepthOctaves`,
`dutyDepth`); full Y at the LFO RATE destination reaches 60 Hz (all
voiced).
**Closes with.** The mod-board summing network from a cleaner scan, or
depth measurements at each destination on hardware, including the
wheel-end LFO rate.

## OQ-15 — Noise pinking blend

**Gap.** The manual anchors "a combination of white and pink" from the
MM5837, but the pinking network's component values — and so its transfer
and the white/pink blend — were not resolved.
**Engine.** The Kellet reference recurrence's three poles, re-derived from
their 44.1 kHz design-rate coefficients to physical frequencies at the
internal rate, with the reference's direct term and normalisation
(`(Σ poles + 0.1848·white) · 0.18`), blended `0.55·pink + 0.225·white`
(the reference filter and every gain are choices standing in for the
unresolved network — all voiced).
**Closes with.** The noise-board schematic values, or a long-window
spectrum capture of the hardware's noise at the mixer — the capture must
pin the blend, not only the poles.

## OQ-16 — Output coupling corner

**Gap.** The output stage's series capacitors are anchored in presence,
but the RC values setting the highpass corner were not resolved from the
scan.
**Engine.** One-pole AC coupling at ~5 Hz per channel (voiced).
**Closes with.** The output-stage RC values from a cleaner scan, or a
low-frequency sweep of a hardware unit's outputs.

## OQ-17 — Red-noise modulation process

**Gap.** The manual anchors RED NOISE only qualitatively ("continuous
slow random"); the filtering network and level on the mod board were not
resolved.
**Engine.** White noise through a one-pole lowpass at 1.5 Hz, restored by
an 18× gain and clipped to ±1 (all voiced).
**Closes with.** The mod-board network from a cleaner scan, or a capture
of the RED NOISE control voltage's spectrum and level from hardware.

## OQ-18 — Shaper SHAPE endpoint split

**Gap.** The manual anchors SHAPE qualitatively (fully left is fast-rise
slow-fall, fully right the reverse); the extreme rise/fall split the pot
actually reaches is not documented.
**Engine.** Rise fraction `0.05 + 0.9·travel`: the extremes are 5/95 and
95/5 of the period (voiced).
**Closes with.** The Shaper board's pot network from a cleaner scan, or
rise/fall timing of a hardware unit at both SHAPE extremes.

## OQ-19 — Master volume taper

**Gap.** The VOLUME pot's taper (linear, log, or loaded-linear) was not
resolved from the scan, and the manual states nothing quantitative.
**Engine.** Output gain follows the square of the travel (voiced — a
loaded-linear-pot approximation).
**Closes with.** The output-stage pot marking and load from a cleaner
scan, or a level-versus-travel sweep of a hardware unit.

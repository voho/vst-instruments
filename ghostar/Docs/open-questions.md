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

**Gap.** The owner's manual says only "through the audio range"; the CEM3350
CV ladders (12k1/16k/68k/274 Ω + 100 k trim) could be solved for Hz but the
scan's digit legibility puts real error bars on the result.
**Engine.** Upper MASTER spans 20 Hz–16 kHz exponentially (voiced); LOWER
ONLY spans −5 octaves to +1 octave relative to Upper with coincidence at
travel 0.8 (anchored coincidence point, voiced span).
**Closes with.** A measured sweep of a hardware unit, or a cleaner schematic
scan resolving the ladder values.

## OQ-03 — Hardware pulse duty cycles

**Gap.** The panel prints 50/30/15/6 % (A) and 40/20/10/3 % (B); the actual
duties depend on ~100k/150k/250–270k divider chains plus a 100 k PW trimmer
whose factory setting is undocumented. Cherry Audio ships 8 % for A's
narrowest — possibly a measured unit, possibly a misprint.
**Engine.** The printed percentages, exactly.
**Closes with.** Scope captures from a calibrated unit.

## OQ-04 — 556A envelope segment curvature

**Gap.** The ADSRs are halves of a 556A timer with 2 MΩ-log sliders into
4.7 µF; timer attacks charge toward a rail and switch at a threshold
(quasi-exponential with overshoot), decays discharge exponentially. No
published analysis states the exact thresholds or the panel-travel-to-time
taper.
**Engine.** Exponential segments; attack aims 1.5× past its peak
(555-family two-thirds threshold); A/D/R travel maps 5 ms–10 s
exponentially.
**Closes with.** A derivation from DWG 3 at better resolution, or envelope
scope captures.

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

**Gap.** The 24 dB mode cascades the chip's two 2-pole sections with a
compensation pad (SW4/R183/R184) so resonance behaviour matches the 12 dB
mode; the exact Q split between sections is not recoverable from the scan.
**Engine.** First section fixed at Q = 0.5, second section carries the
resonance control, in both an anchored-behaviour sense (one knob, both
slopes) and a voiced-split sense.
**Closes with.** Frequency-response measurements of a hardware unit in both
slope positions at matched resonance settings.

## OQ-10 — Overdrive knee and drive

**Gap.** The clipper is anchored in placement (between the filters:
TL082 gain stage with 330 k feedback, anti-parallel BA130 pair, 2k2/470 Ω
pad) but the BA130's I-V curve and the stage's operating level were not
resolved.
**Engine.** tanh knee at ±0.65 V equivalent with ×6 drive and matched
makeup (voiced).
**Closes with.** The BA130 datasheet plus a level trace, or distortion
captures of a hardware unit.

## OQ-11 — No hardware measurements exist anywhere

Every quantitative behaviour beyond the manual's stated ranges rests on
schematic math. Nobody has published filter curves, envelope timings, drift
data, or a hardware-vs-emulation comparison for any Spirit. The first
measured unit becomes this project's ground truth; the Museo del Synth
Marchigiano (which reverse-engineered the 2023 reissue) is the most likely
living source of calibration data.

## OQ-12 — Resonance-path BA130 limiter constants

**Gap.** Self-oscillation is bounded by the external BA130 anti-parallel
"Hi-Q overload limiter" in the resonance path — anchored in placement by
the schematic, like OQ-10's inter-filter clipper — but the BA130's I-V
curve and the node's operating level were not resolved, so the knee and
compression depth are unknown.
**Engine.** A piecewise law on each section's resonant node: linear below
`knee = 1.2`, tanh-compressed toward `ceiling = 2.2` above it, with the
resonance travel mapped `k = 2·0.01^t − 0.025` so full travel regenerates
(all voiced). Each section's lowpass integrator state additionally passes
through `4·tanh(0.25·x)` — intended as a runaway stop, but tanh compresses
every nonzero state a little (≈3 % at 1.2), so it is a second, always-on
nonlinearity (voiced). Whether the CEM3350's internal stages add their own
saturation on top of the external limiter is a separate, unanswered
question.
**Closes with.** Two distinct pieces of evidence, because the entry holds
two distinct laws: the BA130 datasheet plus a level trace of the resonance
node closes the *limiter* (and must justify, re-derive or remove the
integrator bound); the travel-to-damping mapping and its regenerative
offset need the CEM3350's Q-control law with the surrounding divider
network, or an explicit Q-versus-travel sweep of a hardware unit.

## OQ-13 — Filter-tracking pivot note

**Gap.** Keyboard tracking reaches ~110 % (anchored), but the note at
which tracking contributes zero cutoff offset is set by the CV summer's
reference, which was not resolved from the drawings.
**Engine.** The pivot sits at middle C (voiced).
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

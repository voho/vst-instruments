# Ghost open questions

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

## OQ-12 — Resonant-node diode limiter constants

**Gap.** The CEM3350's resonance path is bounded by its internal stages
before the supply rails, but the datasheet does not publish the limiting
characteristic, and no analysis of the Spirit's resonant clipping exists.
**Engine.** A piecewise law on each section's resonant node: linear below
`knee = 1.2`, tanh-compressed toward `ceiling = 2.2` above it, with the
resonance travel mapped `k = 2·0.01^t − 0.025` so full travel regenerates
(all voiced).
**Closes with.** A stage-level derivation from the CEM3350 topology, or
resonance/self-oscillation captures of a hardware unit.

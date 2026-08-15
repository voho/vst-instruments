# YouKnow106

A six-voice circuit-modelled DCO polysynth for macOS, built as a self-contained
JUCE project: VST3, Audio Unit and Standalone, universal `arm64`/`x86_64`.

YouKnow106 models the voice architecture of a 1984 six-voice polysynth — the
Roland Juno-106 — block by block, from its integer-divided note timers through
its four-pole transconductor filter to its uncompanded bucket-brigade chorus. It
is an independent original implementation, not affiliated with or licensed by
Roland Corporation, and it contains no firmware, ROM data, samples or captured
audio. It does include the original 128 factory tone-memory states as functional
18-byte parameter data, independently decoded and checksum-verified as described
below; no Roland Cloud content was extracted. Its panel follows that
instrument's functional geometry and 1980s colour vocabulary while retaining
independent branding, typography and project-drawn controls.

What is modelled from documentation and what remains a voiced choice is set out
control by control in the
[circuit-modelling research and implementation contract](Docs/circuit-modelling-research.md).
Every constant still voiced is listed as a standing, LLM-ready research task
with an explicit evidence gap and required output in
[open questions](Docs/open-questions.md). Where the project stands against the
commercial and open JUNO-106 emulations, on criteria that can be checked from
what those products publish, is in the
[comparative assessment](Docs/comparative-assessment.md); the
[best-in-class pass](Docs/best-in-class-plan.md) records the market sweep and
the measurements behind the most recent round of work.

Its real-time cost was measured and roughly halved in that pass. On one
2.8 GHz core at 48 kHz, the historical harness reported 0.70 elapsed wall
seconds per second of audio for six voices with chorus off and 1.36 with
chorus II and the whole mixer engaged, against 1.11 and 2.38 before; with the
4× internal oversampling switched off the same chorus patch reported 0.45.
Those figures were produced by `steady_clock`, so earlier text calling them
CPU seconds was inaccurate. The full historical table, its corrected
provenance and the current uninstrumented thread-CPU audit are in the
comparative assessment.

The current paired audit runs the shipping JUCE-free engine on one Apple M1 Max
thread, macOS 26.5.1, native arm64 Release, 48 kHz/block 256. Three alternating
Step 11/Step 12 pairs, each with seven repetitions, give authoritative
meta-medians at 4× / 1×: idle **0.677068 → 0.682068× (+0.738406%) /
0.171473 → 0.172614× (+0.665476%)**; six plain voices **0.697359 →
0.696475× (−0.126874%) / 0.179268 → 0.180543× (+0.711718%)**; high
resonance **0.847179 → 0.853898× (+0.793131%) / 0.228095 → 0.231158×
(+1.342855%)**; and full-mixer Chorus II **0.731646 → 0.737013×
(+0.733578%) / 0.191505 → 0.192318× (+0.424526%)**. These are
informational engine thread-CPU observations on one machine, not plug-in/host
totals, competitor data or evidence for a rate split. Every Step 12 result
remains below realtime, the worst is 0.853898×, and the hard Engine CPU gate
and predeclared 5% paired-regression gate pass. The dated Step 9 → Step 10 and
Step 10 → Step 11 series and the older wall-clock history remain in the
comparative assessment.

A separate common-host numerical audit treats 4× as a candidate, never as
truth. Its dated Step 6 baseline rejected every tested DCO cell, with 4× at
−42.62/−41.45 dBc for 44.1/48 kHz against a −70 dBc gate. Step 7 fixes
that numerical reconstruction defect: the current 1×/2×/4× results are
−83.48/−82.44/−82.43 dBc at 44.1 kHz and
−84.88/−92.98/−92.98 dBc at 48 kHz, and all six DCO cells pass the
existing alias, gain, analysis, scan and hold gates. Dated Step 10 replaced the
former Newton realization of the VCF with two fixed half-interval, five-stage
Merson RK4 advances. The nominal-Character-0 common-host VCF classifies
1×/2×/4× as **REJECT / REJECT / PASS** at both 44.1 and 48 kHz; the 4×
hot-waveform errors are −50.351/−50.064 dB RMS against an unchanged −40 dB
gate. Step 11 keeps that ten-RHS Merson solve and makes only the 522 µs cutoff
and shared-resonance holds event-aware. A pure lookahead latches the next
relevant write's fractional position and payload without consuming the
official 23-write cursor or target; its normal scheduler poll later commits
that payload exactly once, even if host automation has since changed. Only an
interval containing such a write evaluates the exact segmented RC hold at the
seven Merson nodes. The eight audited actual-policy and engine-bound paths now
pass at **−84.881, −114.226, −116.317, −112.717, −115.823,
−112.406, −115.445 and −119.340 dB** for 8 kHz/4×, 44.1 kHz/4×,
48 kHz/4×, 88.2 kHz/2×, 96 kHz/2×, 176.4 kHz/1×, 192 kHz/1× and
768 kHz/1× respectively. Late/ceil and early/floor event-snap mutations still
reject at −33.245 and −32.007 dB, so the unchanged −40 dB gate remains
sensitive to the timing path. Step 12 generalizes that pure scalar latch to all
16 passive destinations and resolves the fractional write inside the declared
holds for the six 687 µs voice VCAs, the 9.08249 ms common VCA, the 10 ms SUB
control and PWM's exact continuous 4.7/2.632 ms two-pole cascade; the retained
cutoff/resonance path is unchanged. An independent long-double contract covers
1,105 actual `Engine::process` cases plus 17 later-4×/block-wrap cases. Its
maximum helper/process errors are `2.220446e-16`/`4.440892e-16`, with zero ULP
at the float consumers; deliberate late, early, disconnected and sequential-PWM
mutations all reject. The six DCO/Pitch writes and NOISE stay on their existing
sample-grid paths. Step 13 changes only the numerical audit and gives the
previously incompletely qualified actual-HQ-off VCF boundary its own q1
qualification. A
12-pass moving-control matrix runs the actual HQ-off selector at 8, 44.1, 48,
88.2 and 96 kHz. Its 8 kHz endpoint passes at **−53.279 dB** NRMS with
**−110.051 dB** RK64/RK128 convergence; the four standard rows pass that
moving profile at **−84.738/−86.568/−97.893/−99.618 dB**, with convergence
**−142.698/−144.403/−154.666/−157.689 dB**. Only this moving profile carries
the complete 19-physical/24-logical card, Character and thermal schedule
coverage. A separate static nominal-Character-0 hot fixture—20 kHz-band-limited
1.046502 kHz saw, production-compensated 2.4 V, 16 kHz cutoff and `k=3.8`—then
reads **−12.538/−14.269/−30.417/−33.080 dB** NRMS at the four standard q1
rates. Its RK convergence is
**−135.643/−138.574/−159.637/−162.578 dB**, residual off-mask maxima are
**−44.602/−48.081/−85.765/−88.712 dBc**, and oracle off-mask controls are
**−93.242/−93.163/−97.212/−97.141 dBc**. The unchanged gates are NRMS
≤−40 dB, residual off-mask <−60 dBc and oracle off-mask ≤−85 dBc, so the
combined moving-and-hot truth table is **REJECT at all four standard HQ-off
rates**. Five actual-q1 wiring probes reproduce the connected `renderVoice`
trajectory exactly and reject its disconnected mutation; a scheduler probe at
each of the five q1 rates observes seven pure peeks and seven once-only commits
with payload, cursor, order and pass-wrap semantics intact. The legacy HQ
matrix and all of its goldens remain unchanged. Step 8's causal
four-point BBD edge sampler first cleared the 4× SGA submetric. Step 9 then replaced the
remaining output support-chain TPT steps with one exact continuous six-state
transition and uses the matching exact input transition on internal grids at
or above 176.4 kHz. The common-host 4× BBD cells now pass all three absolute
gates: analytic NRMS −53.442/−56.101 dB, wanted-image BGA error
0.011/0.008 dB and unmasked SGA −71.831/−65.381 dBc. Lower common-host
factors remain absolute **REJECT**. A second matrix follows the actual shipping
selector: all six HQ paths from 44.1 through 192 kHz pass the absolute gates,
while all four HQ-off paths pass only their predeclared Step-8 nonregression
limits. The VCF and BBD admissions are bounded numerical results for their
declared trajectories; neither is a hardware measurement, a new rate split or
permission to move a physical constant. The independent-reference methods and
complete dated matrices are in the comparative assessment. Steps 8–12 add no
future audio-sample lookahead or delay; the global oversampling-factor selector and 41-sample
latency remain unchanged. The normalized converter offsets are still product
policy, not measured hardware timing: OQ-07 and OQ-08 remain open, as do VCF
evidence gaps OQ-09, OQ-10, OQ-15, OQ-16, OQ-18 and OQ-19. Step 13 changes no
shipping DSP, circuit/model constant, selector, state, latency, CPU path or
audio. A future lower-rate VCF repair must clear both independent q1 profiles
without weakening their gates, then qualify inter-domain/whole-engine behavior,
live transitions, latency and CPU before any rate-policy claim. Step 14 now
adds the previously deferred nonlinear, modulated, stereo and stochastic BBD
audit without changing that shipping boundary. Through public
`Chorus::process`, both BBD lines, the actual Engine selector and the shipping
`downsamplePair` cascade, six HQ rows at 44.1/48 kHz q4, 88.2/96 kHz q2 and
176.4/192 kHz q1 pass; the four actual HQ-off q1 rows at
44.1/48/88.2/96 kHz remain **REJECT**. Exact edge/RNG/state ledgers, both
full-cycle mode sweeps and mutation controls prevent a numerical
waveform pass from hiding a clock, stereo, bypass or noise-state defect. This
audit does not run the surrounding full `Engine::process` clip/slew/output call
site and does not qualify that call site's fixed 41-sample latency.
OQ-01/OQ-03/OQ-04/OQ-20 remain open hardware questions.

**Step 15, 2026-08-10.** A narrow production correction now includes the
selected Cut leg's mux-side R21/R23 1 MΩ bleed in C14's load. In HPF Two and
Three, C14 therefore sees `R39 || 1 MΩ = 31,945.788964 Ω`, giving
`τ = 319.457890 ms` and `fc = 0.498203201 Hz`; Boost and Flat keep their
existing `R39 || 47 kΩ` load, and the established 225.8/720.5 Hz Cut sections
do not move. The new JUCE-free `YouKnow106.HighPassNetworkContract` compares
the small production cascade with an independently stamped long-double,
nominal-component fixed-mode MNA solve over 240,001 points from 0.001 Hz to
20 kHz. Worst magnitude/phase residuals are
**0.008363013 dB/0.056091136°** Boost,
**0.000000391 dB/0.000001289°** Flat,
**0.011136100 dB/0.042871357°** Cut II and
**0.003887336 dB/0.013452200°** Cut III. Thirty-six production-updater probes
bind all four modes to nine declared endpoint/common/oversampled policy grids;
a separate helper-derived scalar TPT prediction covers 147,492 finite
frequency responses on those grids. It explicitly labels the 8 kHz Cut rows
and the 32 kHz HQ Cut-III row as endpoint-limited relative to the standard-rate
envelope, and rejects the old R39-only load, wrong-side 1 MΩ and
swapped-capacitor mutations. This is a fixed-position
nominal linear qualification: it makes no full switched-network, TC4052
parasitic, charge-injection or click claim, and OQ-21 remains open. CMake now
registers **14 plugin-off / 15 plugin-on** contracts. Final non-audio Step-15
qualification is recorded below. The twin-render audio handoff is also complete
at canonical 23-file manifest
`0280ae697c209f513283b0c1cac3ad451528f5e6909046ba26d592dce459a430`.
**Step 15 is complete.**

**Step 16, 2026-08-10.** The shared-noise C41/R79 low-pass retained its
physical **4822.877063 Hz** corner but formerly sent that corner directly to
`tan(pi*fc/internal_rate)`. At 8 kHz HQ-off this produced `g = -2.986132794`
and the TPT state pole `-2.006982013`: the private low-pass state reached about
`7.87e294` in 0.25 s even though downstream VCF/output finite recovery could
hide the damage. The coefficient updater now uses
`min(4822.877063 Hz, 0.45 * internal_rate)`. The cap is active below its
**10717.504585 Hz** release point and crosses the old instability seam at
`2fc = 9645.754127 Hz`; 8 kHz q1 is therefore bounded (`g = 6.313755512`,
pole `-0.726542677`) while 8 kHz q4 correctly designs at its 32 kHz internal
grid and keeps the physical corner.

The JUCE-free `YouKnow106.NoiseSourceQualityContract` independently derives
the component corner, checks 4,007 dense updater/seam cells and the real public
idle-before-note trajectory, and bounds the cap-active response against the
physical analogue RC to **1.697765947 dB**. Twelve cap-inactive current/legacy
families remain exactly identical within each run; all nine named mutations
(no cap, host-rate cap, 0.44/0.46/0.49/0.55 caps, `abs(tan)`, post-tan clamp
and sanitize-only) reject. CMake now registers **15 plugin-off / 16
plugin-on** contracts. This is a numerical coefficient-selection repair: it
adds no state, storage, latency or per-sample work and makes no hardware-noise
PSD, amplitude or distribution claim. OQ-15 and OQ-16 remain open. Final
native, sanitizer, universal, CPU and audio qualification is recorded below.
The demo/factory twins remain byte-identical to Step 15, so this repair creates
no WAV or renderer-generated factory-text delta; only the maintained audio
index gains Step-16 provenance.

**Step 17, 2026-08-10.** A lifecycle audit found that the
startup snapshot exception was not actually confined to startup. After the
declared 40 ms output-path quiet interval, every host-block
`setParameters()` call could directly replace the shared RESONANCE, common
VCA LEVEL, PWM, SUB and NOISE target and held values. That skipped the normal
23-write converter queue and jumped past the declared 522 µs resonance/noise,
9.08249 ms common-VCA, 4.7 ms + 2.632 ms PWM and 10 ms SUB trajectories. A
parameter move during a long silence could therefore reach the next attack by
a path the continuously powered hardware does not have.

The correction confines direct shared-hold priming to the startup
window before the first valid positive-length prepared `process()` call. Once
that call begins, ordinary host snapshots can update panel intent but all five
shared destinations remain owned by their normal scan slots and hold
networks, however long the engine later stays silent. Only hard reset or
`prepare()` opens a new startup window. Startup priming remains an explicit
plug-in policy for restoring a saved patch before audio begins; this boundary
does not claim a hardware startup sequence, exact hardware write timestamps,
acquisition, droop or jitter. It closes no open question and adds no state,
storage, latency or per-sample work. Final qualification is recorded below.

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the classic pad and PWM strings, the 16' bass, the self-oscillating filter,
> the chorus modes, unison glide, the delayed vibrato, the high-pass ladder
> and the optional deterministic Unit Character profile. Step 17 reran the
> maintained corpus twice from one frozen native Release engine into
> independent renderer-owned directories: demo and complete factory pairs are
> byte-identical to each other and to Steps 15/16, with manifests
> `b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
> and
> `0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`.
> The current installed canonical 23-file manifest, including its updated
> Step-17 provenance index, is
> `19053f2cb7b57eef5fccb7bfa9f7f5e14ab2e1e932af1672b5138565430d196c`.
> Ten additional
> [factory-preset previews](Docs/audio/factory-presets/README.md)
> retain their relative levels with one shared gain rather than per-file
> normalisation.

## Contents

- [What makes it a circuit model rather than a lookalike](#what-makes-it-a-circuit-model-rather-than-a-lookalike)
- [Fidelity ledger: stage by stage](#fidelity-ledger-stage-by-stage)
- [Voices, analogue character and dispersion](#voices-analogue-character-and-dispersion)
- [Interface](#interface)
- [Original factory bank](#original-factory-bank)
- [MIDI](#midi)
- [Build on macOS](#build-on-macos)
- [Build and test without JUCE](#build-and-test-without-juce)
- [Sign, package and notarize](#sign-package-and-notarize)
- [Layout](#layout)
- [Licensing](#licensing)

## What makes it a circuit model rather than a lookalike

- **The oscillator is a divider, not a phase accumulator.** Pitch is one
  reference clock divided by a 16-bit integer, so it is quantised exactly as the
  hardware's is — A4 at 8' programmes count 4545 and sounds 440.044 Hz. The
  RANGE switch changes the clock reaching the counter, not the count, so it
  transposes by whole octaves and the tuning error is the same in all three.
  A voice-CPU timer restart still occurs at its scanned pitch write, but its
  off-phase ramp/comparator/divider discontinuities enter the existing
  BLEP/BLAMP timeline instead of clearing it into a broadband click.
- **The control path is a scanned converter.** The service timing chart shows
  18 per-card holds—DCO, VCF and ENV/GATE VCA for six cards—and five shared
  holds—SUB, stored VCA LEVEL, PWM, RESONANCE and NOISE—over a 4.2 ms pass.
  The engine executes the chart's exact 23-write logical order on a fractional
  4.2 ms scheduler. A normalized compatibility profile keeps those writes
  sequential across the pass—avoiding an artificial six-DCO phase lock—without
  claiming its offsets as measurements. Exact timestamps, jitter and several
  hold constants remain open; a phase-zero profile exists only for diagnostics.
  Step 11 introduced a pure lookahead for cutoff and shared resonance: it peeks
  in `(phase, phase + delta]` without advancing that order, latches the
  event-time payload and lets the next normal scheduler poll commit it once.
  Step 12 generalizes that scalar latch to the 16 passive destinations and
  resolves the fractional physical write in the declared RC paths: six 522 µs
  VCFs, shared 522 µs RESONANCE, six 687 µs voice VCAs, 9.08249 ms common VCA
  LEVEL, 10 ms SUB and PWM's continuous two-pole 4.7/2.632 ms cascade. The six
  DCO/Pitch writes and NOISE deliberately remain sample-grid paths. The cursor,
  logical order, 23-write count and normalized `ordinal/23` schedule do not
  change. The affected physical states use double precision without changing
  the state dimension.
  MIDI assignment itself never writes an analogue hold: a retrigger retains the
  live capacitor state until that destination's scheduled converter slot.
  Playing latency is now measured separately from plug-in latency. At 48 kHz,
  an exhaustive 1,008-boundary sweep across all six physical cards gives
  event-to-Pitch-write 0/100/201 samples (min/median/max). The fractional
  physical ENV-mode VCA write and first nonzero model gain are 69/191/314 with
  HQ off and 70/192/315 with HQ on; the official normal-poll target remains
  70/192/315 in both modes. The event-to-63.2%-settled VCA hold is
  102/224/347 off or 102/225/348 on. For the declared C4 saw fixture, first
  stereo output above `1e-4` is 86/209/334 samples off and 105/228/351 on. Those are
  raw, uncompensated engine-output offsets; subtracting the fixed 41-sample
  report gives 45/168/293 and 64/187/310 as nominal host-compensation
  coordinates. The −80 dBFS threshold can meet the symmetric reconstruction's
  pre-ringing at different points at 1× and 4×, so it is not a literal measure
  of group delay. The actual centers—24 samples plus 17 samples of padding at
  1×, 35.5 plus 6 at 2×, and 41.25 at 4×—align within half a host sample.
  These are build characteristics of the normalized schedule, not measured JUNO-106 timing;
  the complete protocol and evidence boundary are in the
  [research contract](Docs/circuit-modelling-research.md).
- **There is one LFO, one delay envelope, and one gated value for every
  destination.** The firmware scales the LFO by its delay envelope before the
  CPU distributes it, so DELAY holds the vibrato, the filter sweep and the
  pulse width together and releases them together. The pulse-width converter
  write used to read the ungated LFO instead: at LFO RATE 0.75 (7.4405 Hz),
  DELAY 1.0 and PWM SOURCE = LFO, the duty swept its full 0.4166 span over a
  200 ms window 50 ms into the note while the delay envelope still read
  0.000000 and the pitch and filter were correctly flat. That write now reads
  the same gated product the panel LFO display has always shown, so the span
  there is 0.0000 at a duty pinned to 0.7250, and it is unchanged at 0.4166
  once the delay has released. Nothing in tree states whether the hardware's
  DELAY reaches PWM; what is settled is that the engine used to contradict
  itself, including its own LFO readout.
- **The envelope attacks in a straight line and falls exponentially, into a
  quasi-linear amplifier.** Its 14-bit recurrence, `128b` sustain mapping,
  coefficient selection, rounding, physical `E>>2` 12-bit DAC truncation and
  retrigger behavior are exact for the
  explicitly hash-identified B-2 image; no ROM or coefficient-table contents
  are shipped. Physical pass timing and other firmware revisions remain open.
  INIT/RESET now stores the actual byte-zero attack: it reaches the digital
  peak in one nominal 4.2 ms scan pass, rather than concealing a roughly 46 ms
  byte-five attack at the bottom of the slider. The following VCA hold still
  gives that minimum a short, hardware-like analogue onset rather than an
  impossible instantaneous step.
  The voice VCA that envelope drives is a *current*-controlled OTA. Roland's
  module drawing shows the external grounded-base volts-to-current stage and no
  intentional volts-per-decade converter, which narrows a useful compatibility
  model toward quasi-linear gain above conduction. It does not fix Tr20 onset,
  the BA662's low-current transfer, or a deadband; those remain OQ-19 measurements.
- **Cutoff modulation is summed in converter counts before the antilog stage**,
  at 1143 counts per octave, so every modulation source is exponential in hertz.
  The law is anchored on the instrument's own service calibration: converter
  code 6272 self-oscillates at 248 Hz, and the test suite asserts it. The top of
  that law bends where the transconductor's own control current saturates, near
  a 64 kHz pole, rather than at an invented knee — and the converter's R-2R
  ladder carries its real integral non-linearity, so a slow sweep steps by about
  23 cents crossing mid-scale exactly as a measured card does.
- **The resonance profile compensates on the input side.** Raising resonance
  drives *more* signal into the filter, so a high-Q patch gets dirtier rather
  than thinner — Roland's own drawing feeds the resonance OTA from two
  dividers, one off VCF IN and one off VCF OUT, so the direction is settled.
  The magnitude of that compensation, and the shape of the panel-to-loop-gain
  curve, remain OQ-09 targets.
- **The filter advances its declared continuous equations directly.** The four
  capacitor voltages are the complete physical state and remain double
  precision across rate changes. Every internal sample uses two fixed
  half-interval, five-stage Merson RK4 advances: 10 right-hand-side and
  feedback evaluations, 40 stage and full Early-effect evaluations, and seven
  causal input reconstructions. The input polynomial uses the current endpoint
  plus three predecessors. On ordinary intervals cutoff, feedback and thermal
  headroom move linearly between their previous and current endpoints at the
  same seven abscissae; on an interval containing a cutoff or shared-resonance
  write, the two 522 µs holds instead use their exact segmented values at those
  nodes. Startup fills the history linear → quadratic → cubic. There is no
  tolerance loop, method selector, retry, future audio sample or audio
  lookahead. The per-card thermal
  scale is applied before a product-grid cap of `omega*dt = 0.9*pi`; that
  0.45-cycles-per-internal-sample cap is numerical product policy, not a
  measured property of a JUNO-106. A guarded rate change preserves the four
  voltages, maps the shared control endpoint through the cap-aware grid ratio
  and refills causal input history under zero gain.
- **Raising resonance does not move the corner.** Below the oscillation
  threshold the cascade carries no limit cycle, so there is no compression
  droop to correct, and the frequency correction is identically 1.000000 at
  every resonance panel byte up to 114; it first departs at byte 115, by
  13.2 cents. A fitted quadratic in loop gain used to lift the corner at every
  setting — +8.76 cents at panel 0.30, +32.24 at 0.50, +80.17 at 0.70 and
  +116.25 at 0.80 — which made RESONANCE a second, hidden CUTOFF slider. The
  suite now fences the corner the render actually consumes at three converter
  codes, and the control law behind it across all 128 panel bytes below the
  threshold, so a correction that was fixed in the law but never wired into
  audio would still fail.
- **Self-oscillation is trimmed where the service manual trims it.** The
  ADJUSTMENT table sets every card, at BANK 3 with C4 held, to a 4.8 Vp-p sine
  at 248 Hz, and the two are coupled — a bigger limit cycle compresses the
  transconductor and pulls the pitch flat. That coupling used to be absorbed by
  the fitted quadratic above; it is now derived, from a harmonic balance of the
  two nonlinearities the model already has, each on its own headroom: the
  sinusoidal-input describing function of the four stage differential pairs at
  6.3663 V, which sets the frequency, and of the resonance return at 3.4667 V,
  which sets the amplitude. Both headrooms were already in the profile, so no
  constant was added and one — the trim amount — was deleted. That leaves the
  maximum loop gain as the single fitted quantity, 4.504, answering to the
  amplitude anchor alone: the engine renders 4.8009 Vp-p, and 248 Hz becomes a
  *prediction* rather than a second fit. It lands at 247.90 Hz, 0.67 cents
  under, and stays inside ±0.81 cents at every loop gain in the oscillating
  range, where before only the one fitted point sat on the anchor. The
  derivation is checked against the cascade it describes: at loop gain 4.51 it
  predicts a droop of 0.88968 and a 4.849 Vp-p limit cycle where the rendered
  cascade measures 0.88915 and 4.828 Vp-p. C6 still oscillates at exactly four
  times C4, which is what makes full keyboard tracking 1.00 rather than
  approximately so. One consequence is recorded and not acted on: the service
  trim and OQ-18's measured code-to-frequency table both read 248 Hz at
  converter code 6272, which is a real card declining to droop where this
  cascade droops 203 cents — so either something upstream of the correction is
  over-compressed or the two readings are not the same measurement. OQ-09 owns
  it, and the correction is right either way.
- **The key assigner drops notes rather than stealing them**, because that is
  what the hardware does with seven keys held on six voices.
- **POLY 1 + POLY 2 is Solo Unison.** All six DCOs receive the same divider
  count, so there is no deliberate detune, but the physical oscillators keep
  free-running behind their closed VCAs. They are summed at whatever phases
  they have when the key is assigned; the engine does not reset all six onto
  one artificial phase or divide the stack by six.
- **The POLY switches are momentary firmware inputs, not independent
  toggles.** Their lamps show the assigner's latched mode, so the neither-lamp
  state cannot be stable. Re-pressing the lit mode rebuilds the held-note
  assignments; pressing one control selects that single mode, while pressing
  both together enters Solo Unison. In the mouse UI, Shift-clicking either
  POLY control is the explicit equivalent of that simultaneous press.
- **VCA LEVEL is patch matching, not another envelope depth.** ENV/GATE drives
  each voice module's VCA. The stored VCA LEVEL byte drives one shared
  uPC1252H2 after the voice sum and high-pass and before the chorus, as it does
  on the jack board. Each voice reaches that bus through 33 kOhm against the
  summer's 3.3 kOhm feedback, so it is attenuated by exactly 0.1 before it can
  drive the shared VCA or BBDs. This is why patches can store their own output
  trims without changing their envelope law. For stored byte `b`, the physical
  12-bit code is `d=b<<5`; the nominal jack-board network and NEC control
  constant derive `gain_dB=-16.3196647+0.165581014*b`. C7 and its loaded
  resistance give the shared control a 9.08249 ms time constant, so changes on
  an active signal path settle like the hardware node instead of following a
  voiced curve. Direct host-snapshot priming ends at the first valid prepared
  audio interval; later silence remains scan-owned. That startup exception is
  product policy, not a circuit claim.
- **The chorus has no compander**, so it hisses — the hiss is modelled, and
  there is a control to defeat it that the hardware does not have. Its level
  is the MN3009's own noise row rather than a chosen one: the datasheet that
  already fixes this part's bandwidth and distortion in the model also
  specifies noise 0.2 mVrms max A-weighted. The amplitude each line writes at
  its clock edges is that row referred back through the model's own measured
  A-weighted transfer from the injection point to the wet output — 0.4026,
  through the hold, the tap-summing pole, both reconstruction sections and the
  wet coupling — and through the 2.6 V node coordinate, so the suite re-solves
  the equation instead of re-asserting the answer. That moved the baseline wet
  line down 14.39 dB, from 1.0488 mVrms A-weighted (1.0611 full band) to
  0.200006 mVrms in mode I at 192 kHz; it recovers 0.200020 mVrms on mode
  II's faster programme after dividing out the instrument-level mode calibration
  described below. Across the two HQ internal grids and both modes the recovered
  result spans only 0.200006–0.200078 mVrms, under 0.004 dB. HQ-off remains a
  measured numerical compromise: the corresponding mode-I/mode-II pairs are
  0.208558/0.208917 mVrms at 44.1 kHz, 0.206982/0.207251 at 48 kHz,
  0.201452/0.201763 at 88.2 kHz and 0.201218/0.201584 at 96 kHz. The
  datasheet's own two noise figures disagree by 10.5 dB — 0.2 mVrms max against
  the ~59.7 µVrms implied by S/N 88 dB typ — so landing on the guaranteed
  maximum is a choice inside that bracket rather than a derivation from it, and
  where a real card sits in that bracket is still a measurement nobody here has
  made. The final
  mixer gains dry by `100/47` and wet by `100/39`, putting wet at `47/39` of
  dry (+1.62 dB; the p. 15 designator read places dry on R71/R73 47 kΩ and
  wet on R72/R74 39 kΩ). Those absolute gains occur after the BBDs, so they
  do not falsely overdrive the delay-line model. The one reported structural
  property of the hardware's chorus noise — mode II's output floor observed
  about 3.95 dB above mode I's in the same chain with Panasonic and Xvive chip
  populations (3.96 and 3.95 dB from the printed pairs) — now ships as the
  direct empirical factor
  `10^(3.95/20) = 1.575796`. Mode I stays on the part-derived anchor. Applying
  the factor to the model's edge-held line contribution is the least-assumptive
  way to preserve its existing spectrum and stereo statistics; it is not a
  claim that a standalone mode-II MN3009 exceeds its datasheet row, because the
  observation was made at the complete instrument's output. The internal
  `useChorusRateNoiseHypothesis` comparison instead substitutes the circuit's
  1.6234799 mode-rate ratio (4.2089 dB); it does not multiply both gains.
  OQ-03 still owns absolute PSD, weighting, stereo correlation, spurs and the
  physical cause of the delta. Treating the source's true-peak difference as a
  broadband RMS-amplitude factor is part of that moderate-confidence policy.
- **The BBD's physical and numerical aliases are kept separate.** Its clocked
  sample-and-hold still produces the clock-domain images that belong to the
  device. Step 8 replaces the linear estimate of the signal presented at a
  fractional clock edge with a causal four-point Lagrange interpolation over
  the current and three preceding support-filter outputs. A paper-motivated
  compact polyBLEP separately reconstructs only the deterministic held-output
  steps at their fractional internal-sample times, reducing the additional
  aliases made by the computer grid. These are numerical reconstructions. Edge
  timing and phase, bucket count/index progression, transfer law/update cadence,
  random-number sequence, hardware constants and reported latency are
  unchanged; corrected signal values written into and propagated through the
  buckets intentionally differ. Stochastic BBD noise is deliberately not
  predicted or corrected.
- **The chorus modulator is drawn from the JUNO-106's own schematic.** IC1 is an
  integrator closed around a Schmitt comparator, so the sweep is a straight
  symmetric triangle, and IC2a inverts it for the second line — the antiphase
  pair is one waveform and its negative. The I/II switch shorts a 2.2 MΩ leg of
  the integrator's T-network, which fixes the mode-rate ratio at 1.6234799
  exactly. The reported `.1` integrator capacitor on Roland's p. 15 and the
  component-identical sibling-board netlist close the scale: the implemented
  rates are derived as 0.5532934 Hz and 0.8982608 Hz. Both truncate to the
  owner's manual's published "about 0.5" and "about 0.8" and agree with a
  106-chorus clone's scope readings within 3%. The 1.4–6.4 ms sweep endpoints
  are a third-party scope measurement of a designator-faithful p. 15 build
  with genuine MN3009s, compared directly against a real JUNO-106 by its
  measurer; a calibrated original-unit capture remains the open ask (OQ-01).
- **The final outputs are AC-coupled before VOLUME.** The two service-schematic
  paths use C17/C20 10 µF and R54/R57 1.5 kΩ into the 10 kΩ pot tracks. The
  unloaded full-track reference is 1.383956 Hz and `10/11.5` settled gain; the
  engine solves the actual corner/gain against shaft position and fixed wiper
  loads while retaining independent capacitor states. This removes the large
  DC offset an asymmetric manual-PWM patch would otherwise send to a host.
- **Every established coupling boundary is now in the signal path, inside the
  voice card as well as on the common bus.** On the card, C56/C50 couples the
  summed WAVE node into pin 1 VCF IN at 0.482288 Hz, and C59 couples pin 3 VCF
  OUT into pin 9 VCA IN at 4.822877 Hz — 1 µF against a voiced 33 kΩ pin-9
  load, bracketed 33–100 kΩ because neither R108 nor VR27's setting is in tree,
  which puts the pole between 4.82 and 1.59 Hz and far below the lowest note
  either way. On the bus, C14/R39 couples the six-voice sum into the selected
  HPF leg; C12/R36 couples that result into the common uPC1252H2 VCA; C17/C20
  couple the complete stereo IC6 outputs into VR1. C12/R36 is 0.482288 Hz. C14
  is 0.820915 Hz in Boost/Flat (`33k || 47k`) and 0.498203 Hz in the two Cut
  positions (`33k || 1M`). C10/C11 are open on their far side at the sub-hertz
  asymptote, but the selected mux-side R21/R23 1 MΩ bleed remains directly
  connected to the C14/YCOM node.
- **The envelope multiplies an AC node, so PWM duty no longer thumps.** C59 is
  the boundary that matters most, because the pulse comparator's mean walks
  with duty and the transconductor cascade carries no DC-blocking term of its
  own: the voice amplifier was multiplying the filter's own offset. On the
  fixture the suite holds, the mean at pin 9 falls from +0.042768 / +0.024289 /
  +0.029798 V to −0.000033 / −0.000166 / −0.000118 V at PWM panel 0.00 / 0.50 /
  1.00, and the sub-20 Hz peak against broadband RMS falls from −42.49 / −25.10
  / −17.55 dB to −54.59 / −28.53 / −30.25 dB. The remaining duty dependence in
  that last measure is the attack ramp, which has one of its own; what the
  capacitor removes is the offset, and at every duty the amplifier now
  multiplies a node whose mean is under 0.2 mV. Roland's own procedure trims
  VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump on this exact node.
- **Main VOLUME follows the marked part.** Service documentation identifies
  VR1 as `10KB×2`; Panasonic's later JIS/EIAJ table maps plain `B` to the
  nominal-linear `1B` group (40–60% at mid-travel), so the engine uses linear
  track resistance instead of its former squared taper guess. The table's
  separate S-shaped volume law is `3BM`, not plain `B`. The
  fixed 41.3 kΩ selector ladder and 101 kΩ headphone input load each wiper in
  the model, giving 0.4763 normalized gain at half travel. Real dual-gang
  tracking and the physical output-jack path—selected-tap loading, R64/R65,
  C21/C22, jack normaling and external loads—remain explicit measurements.
  Schema-1 saved states remap the former squared value to the nearest new
  position of equal static gain; host-owned automation lanes cannot be rewritten.
- **Noise density does not move with the HQ switch.** The shared noise source
  and microscopic voice-card excitation are normalized to elapsed time rather
  than internal sample count. A quality change still waits for voices and
  musical tails; a block-size-independent 5 ms fade hides the unavoidable
  rate-dependent rebuild while preserving the host-rate output-capacitor state.
- **Neither does the modelled warm-up.** The chassis law
  `T(t) = 25 + 15(1 − e^−t/900)` is advanced once per internal sample, and its
  accumulator is wall-clock seconds in double precision, so the increment
  cannot round away against the total. It used to be a float, where a 5.208 µs
  increment fell below half an ULP the moment the total passed 128.0: the
  modelled chassis froze at 26.99 °C with HQ on and 31.51 °C with it off
  instead of running the law out to 34.4818 °C, and a quality setting therefore
  changed what the supply does. At full Unit Character the temperature now reads 26.988573 /
  29.252031 / 34.481808 °C at 128 / 300 / 900 s and the OTA headroom it drives
  reaches 6.568748 V, identically at 44.1, 48, 96 and 192 kHz and with HQ on or
  off. The rise scales with Unit Character, so at zero the cards stay at
  ambient and the thermometer says so.
- **Two modelled mechanisms are enabled and measurably inert, and are reported
  rather than quietly tuned into audibility.** The polyphonic rail sag is real
  arithmetic on a real node and it does nothing: on a held six-voice chord it
  measures 0.000522 V at one voice against 0.003356 V at six, which is −0.0192
  to −0.1233 cents of cutoff, so the entire one-to-six-voice load change is
  0.104 cents and a chord has the same filter as a single note. The TA75558S
  slew limiter cannot engage at all — at a 192 kHz internal rate its per-sample
  cap is 3.405 engine units against a band-limited signal that cannot exceed
  about 1.6, and switching it off changes a full chorus render by −171.48 dB
  relative to signal. Making either audible would take the M5230L output
  impedance and reservoir ESR that are not in tree, or a part the instrument
  does not contain.
- **The shared noise is band-shaped by its own circuit, not flat.** The p. 13
  designators draw C42 1 µF into the level OTA's 4.7 kΩ input bias (33.9 Hz
  high-pass) and C41 100 pF against R79 330 kΩ on its output
  (**4822.877063 Hz** pole), so the rail loses the synthetic top-octave hiss a
  flat generator carried. Step 16 keeps that component corner and limits only
  its TPT design corner to `min(4822.877063 Hz, 0.45 * internal_rate)`, which
  prevents an unstable endpoint coefficient below 10.7175 kHz without
  changing common-rate or 8 kHz-q4 responses. The passband keeps the
  established density; physical PSD, amplitude and distribution remain OQ-16.

## Fidelity ledger: stage by stage

“Exact” below has a narrow meaning: either a documented hardware topology/value,
or behavior reproduced for the explicitly hash-identified A-5/B-2 firmware
images. It does not mean that an arbitrary forty-year-old analogue unit will
null against the plug-in. **Derived** means arithmetic from those anchors;
**approximated** means the right circuit or behavior implemented by a numerical
equivalent; **voiced/guessed** means an audible value chosen provisionally
because the required hardware measurement does not exist; and **product policy**
means a deliberate plug-in feature with no hardware claim. The detailed,
controlling version of this ledger is the
[research contract](Docs/circuit-modelling-research.md); how this coverage
compares with every other JUNO-106 emulation, and which market claims
remain external-validation debts, is the
[comparative assessment](Docs/comparative-assessment.md).

| Stage | Same as the hardware / evidence-fixed | Approximated, guessed/voiced or product-only |
| --- | --- | --- |
| Patch memory and selection | All 128 locations are in hardware order A11…A88, then B11…B88. Each tone is the exact 16 continuous bytes plus two packed switch bytes used by the hardware, decoded by the SysEx path. Program Change 0…127 maps directly to those slots. | The hardware stores no names; displayed names are archival metadata. A host preset also restores plug-in/performance controls, while hardware Program Change and SysEx correctly restore tone memory only. |
| Key assigner and POLY modes | Six-card allocation, POLY 1/POLY 2, note dropping instead of stealing, held-key rescans and Solo Unison behavior are ROM-resolved for the stated A-5 image. The physical keybed is represented as 61 keys. | Velocity, more than six voices and host notes beyond the drawn keybed are extensions. Velocity defaults to zero depth and is then exactly inert; turned up, it scales the voice amplifier's control and the ENV amount into that voice's filter by the same `1 − depth·(1 − velocity)`, so it rides the two paths the panel already has and adds no curve of its own. The mouse Shift-click gesture is a UI equivalent for pressing both momentary POLY contacts. |
| Shared digital control generator | Envelope recurrence, sustain mapping, DAC truncation, LFO/delay arithmetic and portamento are ROM-resolved for the stated B-2 image. The 23 converter destinations, their ownership and the 4.2 ms pass are anchored. VCF and voice-VCA hold constants are component-derived, as is the common VCA LEVEL path's 9.08249 ms post-S/H C7 pole. | Writes retain the exact logical order but use normalized sub-pass spacing; exact timestamps, acquisition and jitter remain open. Step 12 resolves the declared fractional event inside the six 687 µs voice-VCA holds, 9.08249 ms common-VCA pole, derived 10 ms SUB pole and derived PWM 4.7/2.632 ms two-pole cascade. A generalized scalar latch covers 16 passive destinations, including Step 11's unchanged RES/VCF path; DCO/Pitch and NOISE remain sample-grid. The existing passive physical states use double precision with no new coordinate. A 48 kHz model characterization sweeps all 1,008 host-boundary phases and six cards, separating the fractional physical VCA write from its official target poll and a declared output-onset proxy; its numbers describe this compatibility profile, not hardware. Direct host-snapshot priming is startup-only and remains product policy; after the first valid prepared interval, silence and panic do not bypass the scanner. The delay envelope gating the PWM write alongside the pitch and filter writes is internal consistency with the firmware's one attenuator and one LFO, not a documented statement that DELAY reaches PWM. |
| DCO, ramp, pulse, sub and mixer | The 8 MHz master reference, integer timer division, range clocks, pitch quantization, constant-current ramp, PWM comparator and divide-by-two sub topology are anchored/derived. A changed-pitch write occurs at that card’s converter slot. The moving-threshold solver prevents a digital-only missed PWM edge and full-cycle blip. | BLEP/BLAMP repairs are numerical antialiasing mechanisms, not evidence of hardware transparency by themselves. Step 7 keeps a circular 24-internal-sample naive delay behind a symmetric `H=24` correction. It linearly interpolates the continuous bandlimited step response and only then subtracts the exact ideal step, avoiding interpolation across the residual's unit jump; the continuous slope residual remains stored directly. A 95-tap Kaiser half-band (`beta=7.857`) closes the common-host boundary. The expanded saw/sub/5–50–95% pulse grid now passes all six 1×/2×/4× cells: −83.48/−82.44/−82.43 dBc at 44.1 kHz and −84.88/−92.98/−92.98 dBc at 48 kHz against −70. None of this changes the hardware/model laws or proves them against an original unit. Exact restart electrical state, loaded saw/pulse/sub/noise levels, filter-drive budget and live waveform-switch transients remain approximated or open. Pulse currently uses a provisional instantaneous audio gate; no invented anti-click envelope is presented as hardware behavior. |
| Shared main noise | One Tr21 source, the C42/4.7 kΩ 33.9 Hz input high-pass and the C41/R79 **4822.877063 Hz** output low-pass feed one scanned NOISE LEVEL rail shared by all voices. | The bounded-uniform generator coordinate, absolute amplitude/distribution and microscopic per-card startup excitation remain voiced under OQ-15/OQ-16. Step 16 limits only the output TPT design corner to `min(4822.877063 Hz, 0.45 * internal_rate)`: 8 kHz q1 is stable while 8 kHz q4 and all common cap-inactive families retain their old response exactly. The 1.697765947 dB cap-active error is against the declared physical analogue RC, not a hardware capture. |
| Per-voice VCF | Four IR3109/BA662 transconductor stages, the 68 kΩ/560 Ω attenuation, 240 pF stages, per-card cutoff trims and service calibration anchors are hardware-fixed. Cutoff modulation is summed in converter counts before the exponential law. The upper knee is the transconductor's own control-current saturation near 64 kHz, and the converter's R-2R carry error rides on the code it produces. | Step 10 introduced two fixed five-stage Merson halfsteps over the same continuous four-stage equations. Step 11 leaves their four double capacitor states, causal cubic drive, ten RHS evaluations, product-grid cap and endpoint-linear ordinary path unchanged. Only an interval containing a fractional cutoff or shared-resonance write receives exact segmented 522 µs hold values at the seven Merson nodes; the normal poll later commits the payload latched at the declared event. The dated Step-11 19/24-profile matrix clears all eight HQ/engine-bound paths from 8 kHz/4× through 768 kHz/1× at −84.881…−119.340 dB; its deliberate late/ceil and early/floor snap mutations reject at −33.245/−32.007 dB. Step 13 separately qualifies actual HQ-off q1: five moving-control rows pass, but static nominal hot rows reject at all four standard 44.1/48/88.2/96 kHz hosts, so none is admitted by the combined rule. Connected/disconnected shipping `renderVoice` probes remain mutation-sensitive. Resonance byte-to-loop gain, input compensation and feedback saturation are voiced pending measurements; the maximum loop gain (4.504) remains fitted only to the 4.8 Vp-p anchor. The saturation exponent and carry sizes are fitted to a third-party measured card, not to a Roland document. Neither Merson nor event-aware evaluation is hardware evidence, and OQ-07/OQ-08 remain open. |
| Per-voice VCA | One BA662 follows each VCF. Roland shows VCF OUT pin 3 AC-coupled by C59 1 µF/50 V NP to VCA IN pin 9, and that capacitor is now in the signal path ahead of the gain multiply; the separate R106/C58/R105/Tr20 branch drives VCA CONT pin 11; VCA OUT pin 10 reaches TP8–TP13 and the 33 kΩ summer inputs. The service procedure trims VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump and sets a 6 Vpp gain endpoint. | Step 12 applies the fractional converter write inside each existing 687 µs hold rather than at the next internal-sample edge. The current quasi-linear gain/onset/knee/deadband law is schematic-informed compatibility, not a measured BA662 transfer. C59's capacitance is the designator read; the pin-9 load it works against is voiced at 33 kΩ and bracketed 33–100 kΩ (4.82–1.59 Hz), because neither R108 nor VR27's setting is in tree — OQ-19 owns it, and the pole's job is insensitive to the choice inside the bracket. The nominal model adds no residual feedthrough: Unit Character's control-hold offset is not the VR30 signal-input null, and post-calibration thump magnitude/polarity/spectrum remain unmeasured. Velocity is an optional extension, inert at its default zero. |
| Voice sum, coupling, HPF and common VCA LEVEL | Six card outputs sum through 33 kΩ into 3.3 kΩ feedback (0.1 each). C14 precedes the shared four-position HPF; C12 then feeds the one common uPC1252H2 controlled by stored VCA LEVEL. Service Notes pp. 8 and 15, the ROM-resolved `d=b<<5` code and NEC's −5.9 mV/dB typical constant derive the nominal common-VCA law and 9.08249 ms C7 settling. | Step 12 applies that common hold's fractional write before the existing gain consumer; the independent wiring probe stays within `8.961428e-7` relative error over 495 samples and reaches `0.7034001` when disconnected. Step 15 corrects C14's selected-Cut load to `R39 || R21/R23 = 31,945.788964 Ω` (`τ = 319.457890 ms`, `fc = 0.498203201 Hz`) while retaining one physical C14 state across mode/rate changes. Its independent fixed-mode MNA audit bounds the four cascade residuals at 0.011137 dB/0.056092° or below, but the complete switched network, CMOS parasitics and switching memory remain OQ-21. The bass-boost shelf itself is derived from the p. 15 branch (+10.50 dB DC, +1.41 dB high band, 59.41 Hz pole, within 0.016 dB of the exact two-zero/two-pole solve). The ideal 12-bit R-2R transfer assumes division by 4096; R32 now reads unambiguously as 1.5 kΩ in the complete scan, and real resistor/capacitor tolerance, rail error and uPC1252 variation still need an installed-unit sweep. |
| BBD chorus and IC6 mix | Two uncompanded 256-stage MN3009 lines, anti-phase modulation, continuously running bypass, support-filter parts, coupling capacitors and IC6 dry/wet resistor gains are anchored/derived. The mode rates are derived from the JUNO-106 timing network as 0.5532934/0.8982608 Hz. BBD write nonlinearity is fitted to its datasheet test points. At the raw held node, upstream of numerical output reconstruction, the explicit zero-order hold plus fixed per-shift residual coefficient is −3.000 dB versus DC at 12 kHz/40 kHz, or −2.972 dB versus the datasheet's 1 kHz reference. The mode-I/base per-line hiss amplitude is the same datasheet's noise row, 0.2 mVrms max A-weighted, referred back to the injection point through the model's measured 0.4026 A-weighted transfer. | Sweep endpoints retain a calibrated sibling measurement of the shared clock driver; loaded support impedances and the wet-mute transient are voiced, as are stereo correlation and the optional common/hum/spur layer. The base hiss level is anchored to the part but sits inside a 10.5 dB bracket its own datasheet leaves open; the guaranteed maximum is the end this model takes. Mode II separately applies the reported approximately 3.95 dB complete-output factor from the same-chain real-unit captures. Treating their true-peak difference as a broadband amplitude factor is moderate-confidence policy, not a claim about the standalone part or physical insertion point (OQ-03). The 0.4026 transfer is a property of the current exact output chain at 192 kHz; HQ grids agree within 0.004 dB, while HQ-off reads about +0.05…+0.38 dB high over 96…44.1 kHz. Step 8 supplies causal current-plus-three-past interpolation at each BBD edge. Step 9 integrates each complete six-state support network in physical coordinates under the same causal cubic drive: output is exact at every rate, input is exact at internal rates ≥176.4 kHz and retains the reviewed TPT path below. Muted/connected wet loads select distinct prepared transitions over one shared physical state. These are numerical product mechanisms, not MN3009 circuitry. Edge timing/phase, bucket count and index progression, transfer law/update cadence, RNG sequence, hardware constants, global oversampling-factor selector and 41-sample latency are unchanged. The common-host 4× cells now pass the absolute four-case low-drive fixture gates at −53.442/−56.101 dB NRMS, 0.011/0.008 dB BGA error and −71.831/−65.381 dBc SGA; each cell has only one qualifying BGA line, and lower factors remain absolute REJECT. All six actual HQ selector paths pass that same bounded fixture; four HQ-off paths are retained only by frozen Step-8 nonregression gates. The closed-form oracle shares the documented component/model anchors and is not hardware truth or a nonlinear whole-line oracle. Step 14 adds a separate public-path dynamic whole-line contract: all six actual HQ rows pass hot stereo, I/Off/II transition, residual and stochastic gates, while every actual HQ-off q1 row rejects under the unchanged truth table. This is numerical classification, not a retune or hardware measurement. Panasonic's low-resolution typical curves at 10/40/100 kHz have been digitised at 600 dpi; their tracked-versus-broadband reading is self-contradictory, so one installed-unit tracked sweep still decides which interpretation applies (OQ-04). Loaded IC6 clipping remains unknown. |
| VOLUME and output boundary | C17/C20, R54/R57, the nominal-linear 10KB×2 tracks and fixed internal wiper loading are component-derived, with independent left/right capacitor state. | Dual-gang tracking, selector/jack normaling, external loads and headphone transfer remain open. The fixed −18 dBFS RMS mapping and provisional physical reference are product policy, not an analogue circuit claim. |
| Numerical cost | The VCF retains fixed solver work per internal sample: two Merson halfsteps, 10 right-hand-side and feedback evaluations, 40 stage and full-Early evaluations when Character is enabled, seven input reconstructions and no normal-path recovery. Event-containing VCF intervals additionally perform six control mappings; ordinary intervals do not. Settled per-card constants remain outside the sample loops. A compile-time-only work audit counts scan, passive holds, DCO, VCF, BBD/BLEP, exact/legacy support and decimator events on every 4×/2×/1× production branch; a separate executable times the uninstrumented shipping library with a thread-CPU clock. Normal and active-counter renders must remain raw-float identical, and preprocessing plus symbol/string scans keep audit instrumentation out of the shipping library. | None of it is a hardware claim, and no constant, level, corner or law moves. In the 2,048-host-frame 48 kHz fixture, HQ and HQ-off each see 160 passive fractional peeks/commits, while the retained VCF subset stays 70/70 and 120 exact affected card intervals. The fixed VCF and BBD work algebra is unchanged. Counters make no cycle-cost claim; the paired uninstrumented Step 11 → Step 12 audit ranges from −0.126874% to +1.342855%, with a worst Step 12 result of 0.853898× realtime. BBD support is unchanged. Counters prove structural work, not cycle cost or a split-rate saving; completed paired CPU measurements are in the [comparative assessment](Docs/comparative-assessment.md). |
| Antialiasing, HQ and safety | These are intended to preserve the modeled circuit’s behavior at host sample rates: bandlimited discontinuities, optional oversampling, 95-tap Kaiser half-band decimation flat to 20 kHz at both common host rates, and guarded rate changes. HQ remains one global internal loop: 44.1/48 kHz selects 4×, 88.2/96 kHz 2× and 176.4 kHz or above 1×; HQ off selects 1×. The engine and processor report a fixed 41-host-sample numerical latency on the 4×, 2× and 1× paths (0.930 ms at 44.1 kHz, 0.854 ms at 48 kHz, 0.427 ms at 96 kHz and 0.214 ms at 192 kHz); shallower paths are padded to the deepest report. Nothing the quality switch selects is allowed to move a modelled physical quantity: noise density is normalized to elapsed time, and the warm-up clock accumulates wall-clock seconds in double precision so it reads the same at every supported rate and in both quality settings. The measured exception is numerical noise folding: after Step 9's recalibration, HQ-off wet-line noise reads about +0.36…+0.38 dB at 44.1 kHz, +0.30…+0.31 at 48 kHz, +0.06…+0.08 at 88.2 kHz and +0.05…+0.07 at 96 kHz. For the chorus, BBD-generated aliasing (BGA) means the physical-model images at `k*Fclock ± f`; simulation-generated aliasing (SGA) means the extra folds created by the internal sample grid. The bounded polyBLEP scheduler has 54 slots and uses at most 50 in the tested worst case, including multiple BBD edges in one internal sample. | They have no hardware counterpart. The reported 41 samples cover oscillator-reconstruction/decimation group delay only: converter scan, envelope/VCA-hold response, host/device buffering and wet BBD delay are separate. DCO, VCF/VCA, scan/holds, the complete BBD/support path and output slew all share that loop. The common-host audit treats each factor as a candidate rather than truth: DCO passes all tested factors; BBD and VCF pass at 4× and reject their lower common-host factors. Separate actual-policy matrices admit the BBD fixture and bounded VCF converter trajectories on all six standard HQ paths. Step 11's event-aware VCF extension also passes the 8 kHz/4× and 768 kHz/1× engine bounds without relaxing the −40 dB gate. Nothing changes the global factor selector or authorizes a split without inter-domain reconstruction, whole-engine and latency qualification. The idle-only quality change and short safety fades are product mechanisms. |

The maintained [audio corpus](Docs/audio/README.md) was rendered twice again
for Step 16. Two independent demo renders and two independent full 128-preset
factory audits produce pairs byte-identical to each other and to Step 15, with
manifests
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
and
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`.
The 22 renderer-owned files have manifest
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`;
the current installed 23-file tree, including its Step-16 hand-maintained
index, is
`8346a817bd215808112510dc3d37b5a8fac3a5f401aa93d117b2b9f0912ba8dd`,
superseding Step 12's
`f9a6b274e7efb857a712ecaed1061e5251bd554e22462adce986e5e4d8158cbd`.

All 20 WAVs are non-silent finite stereo PCM16, with maximum absolute DC
`0.000000592814 FS` and worst edge `−46.962652 dBFS`. The CSV contains 128
finite unique slots and 128 unique tone states; its median is
`−21.480711305 dBFS`, with 31 overload, zero near-silent and nine median-
outlier rows. Nine demos remain byte-identical. Only
`09-high-pass-ladder.wav` and the ten common-gain previews change, each by at
most two PCM16 LSB: demo 09 is **−85.129 dB NRMS** against Step 12. A86 is
the worst preview by L2 NRMS at **−54.771 dB**; A17 is **−83.872 dB** and is
the only preview to reach a two-LSB peak. Twenty-nine CSV rows move only at fine
precision: A17's overload-sample count changes **7000 → 6999**, common preview
gain **0.543091 → 0.543092**, and B51's displayed crest
**23.36 → 23.35 dB**. Exactly 14 tracked `Docs/audio` files change: that one
demo, ten previews, the audio index, generated factory README and metrics CSV.
These bounded deterministic differences are provenance, not an audibility,
click or complete switched-network claim.
Historical before/after, fidelity, realism and state-of-the-art comparisons
remain recoverable from Step 9 and are intentionally absent. The numerical
claims rest on independent contracts, not an invented recording-chain
comparison.

## Voices, analogue character and dispersion

The first six slots are persistent physical voice-card models. Their DCO,
filter, comparator and card noise state keep running behind a closed VCA, just
as powered cards do; a note assignment opens a card that already has a phase
and history. The shared converter visits the 23 destinations sequentially, so
the six pitch writes are not simultaneous. Those two facts prevent an
artificially phase-locked unison stack.

All six DCO timers still derive from the same 8 MHz reference and receive equal
counts for equal notes: there is **no six-oscillator detune generator**. The LFO
is shared. Envelope rates and recurrence are also digital and identical for
every card; analogue dispersion is applied to the circuit the envelope drives,
not to six invented envelopes.

At Unit Character = 0%, every optional multiplier is exactly zero and the
engine is the deterministic calibrated-nominal model. At 100%, the current
fixed-seed voiced profile enables these full-scale mechanisms:

| Per-card mechanism | Full Character span/effect |
| --- | --- |
| DCO ramp current | up to ±3% |
| PWM comparator threshold | up to ±0.24 V |
| VCF cutoff scale trim | up to ±5% |
| VCF cutoff offset trim | up to ±0.07 octave |
| Resonance control offset | up to ±0.02 panel travel |
| Voice-VCA control-hold offset (not the VR30 audio-input null) | up to ±0.004 normalized control |
| Voice-VCA gain | up to ±3% |
| Sub level | up to ±3% |
| Main-noise level at each card | up to ±3% |
| Slow cutoff wander | d = 0.9992d + 0.004noise at 375 Hz; contributes 40d converter counts |
| VCF stage input offsets | up to ±1.5 mV per transconductor stage |
| VCF stage capacitor tolerance | up to ±2% per stage, staggering the four poles |
| Spatial thermal gradient — card temperature | up to +4 °C across the six cards, plus a +15 °C warm-up on the law `25 + 15(1 − e^−t/900)`, raising the OTA thermal voltage and its headroom. The curve runs to completion: 34.481808 °C and 6.568748 V of headroom at 900 s, the same at every host rate and in both quality settings |
| Spatial thermal gradient — cutoff | up to ±0.6% on each card's integrator gain (±10 cents), from the same exponential card profile as the temperature above |
| Cutoff converter carry error | −4.6, +23.3 and −4.5 cents at the three top bit boundaries |

The cutoff row above used to read ±165 cents — a monotonic ramp by card index,
roughly ten times what the temperature computation beside it supports, linear in
the card index while that temperature profile is exponential, and applied on top
of the two cutoff trimmer residuals. It now comes from that same exponential
profile through the AS3109's own 0.33%/°C cutoff tempco, taken about the six-card
mean because the FREQ trim is set warm. The module board carries a PTC positor in
exactly this path to cancel that tempco, so even the reduced figure is an upper
bound rather than a measured residual (OQ-10).

Every mechanism scales linearly with the knob. Seeds are fixed, so the same
patch, settings and note sequence render identically; the instrument does not
become a different random unit each launch. These spans are **voiced sound
design**, not measured population statistics—OQ-10 is the evidence needed to
replace them with real six-card and multi-unit distributions.

The analogue impression does not depend on Character alone. It also comes from
integer pitch quantization, free-running/staggered DCO phase, scanned and slewed
control voltages, the nonlinear four-stage filter and resonance return, a shared
noise generator plus microscopic deterministic filter excitation, the
unnormalized six-card unison sum, component-derived coupling poles, and BBD
charge transfer, bandwidth, nonlinearity and hiss. The residual charge-transfer
state advances once per modeled BBD shift (one fCP period), so its fixed
coefficient already moves its absolute pole with the instantaneous clock instead
of requiring a second clock multiplier.

## Interface

The interface keeps the reference instrument's control inventory **and its
reading order** in a 1280×702 console. The continuous synthesis strip is LFO,
DCO, HPF, VCF, VCA, ENV and CHORUS. VOLUME and PORTAMENTO return to the left
performance cheek with the three bender-depth faders, the portamento switch and
spring lever; the 61-key keyboard begins beside that cheek instead of spanning
under it.

Directly below the synthesis strip is the hardware programmer tier: KEY
TRANSPOSE, POLY 1/2, MIDI channel, A/B group, eight BANK keys, the recessed red
display, eight PATCH keys, MANUAL, WRITE and tape SAVE/VERIFY/LOAD. The immutable
factory bank makes MIDI CH, WRITE and VERIFY explanatory disabled controls;
bank/patch selection, MANUAL and SysEx SAVE/LOAD are live. OFF/I/II chorus keys
and the original horizontal range and waveform keys restore the smaller panel
relationships as well as the broad section order.

The original surface is planar charcoal metal with red synthesis rails, a blue
programmer rail, warm rectangular keys, red lamps and graphite fader caps.
Thin dividers and small recesses provide depth without turning each section into
a rounded dashboard card. Functional waveform and foot-register marks remain
project-native vectors. Small programmer legends use an open system sans rather
than condensed display type, and the resize floor keeps them above the tested
readability threshold.

A slim host-preset navigator sits directly below the programmer, aligned with
the BANK keys but isolated by its own recessed blue rail. The remaining
plugin-only features are visibly bolted on below the keyboard in three separate
cards: model extensions (Unit Character, chorus noise, HQ and a mouse-friendly
UNISON key), performance extensions (transpose, rear-panel tune, velocity and
variable polyphony), and live telemetry. Panic, reset and randomisation stay in
the bottom service bar beside contextual help.

Sliders, switches and buttons are still placed by the JUCE-free description in
`Source/DSP/YouKnow106Panel.cpp`, so tests prove that the row does not overlap,
escape its cards or shrink its legends below the readability floor — including
at the smallest window, which is the binding case and is what fixes the lower
deck's depth. A bundled low-contrast material scan adds maintained ABS grain,
polished touch wear, cleaning swirls and sparse hairline scuffs. Recessed fader
channels, bevelled and grooved caps, and inset illuminated switches add a
refined vintage material language while remaining project-drawn vectors. The
hardware-inspired red/blue hierarchy remains under independent branding rather
than reproducing a manufacturer mark.

Descriptions no longer float over the instrument. Hovering any interactive
element updates the fixed help display below the keys immediately.
All 83 public controls are covered: the synthesis and controller panel, original
programmer tier, six extension knobs, utility and host patch controls, 61-key
keyboard and pitch/mod lever. That strip also carries the hovered control's **current
setting**, in its own lit right-hand column and in the parameter's own units, so
reading a value no longer requires starting a drag. The same TooltipClient
strings remain accessibility metadata, while every no-text-box slider retains a
separate numeric value bubble during adjustment. Routing, minimum explanatory
length, stable help geometry and value-bubble presence are regression-tested.

The live status card's oscilloscope ranges itself. The instrument's output convention puts
an ordinary patch near a tenth of full scale, so a fixed ±1 trace was a flat line
for most of what it plays; the trace now follows a slow-release peak, snaps to a
power-of-two gain and prints that gain on the screen, because a scope whose
sensitivity moves silently is not telling the truth about level. Its trigger
carries a hysteresis band scaled to the trace, so a near-silent buffer no longer
latches onto its own dither.

The bender lever is live performance input rather than a saved parameter. Drag
left/right for pitch bend and upward for modulation; both axes spring exactly
to zero. Its latest two-axis position crosses to the audio thread through one
coalescing lock-free mailbox, so dense drags cannot fill the keyboard event
queue or lose the final release. It drives the same hardware-style controller
scan as external Pitch Wheel and CC 1 and does not enter patches, automation or
session state.

Unit Character remains the optional deterministic voice-variation amount; zero
is the calibrated nominal baseline because real post-calibration distributions
remain unmeasured. The other extension defaults are inert or hardware-aligned:
velocity does nothing, polyphony is six voices, and the delay lines retain their
modeled noise floor.

VELOCITY, when it is turned up, is a dynamics control rather than a second
output trim. The one gain `1 − depth·(1 − velocity)` now scales the ENV amount
reaching that voice's filter as well as its amplifier control, so a quieter
note is a note whose filter envelope opened less far. At full depth, on the
patch the suite fixes — CUTOFF 0.30, ENV 0.30, RESONANCE 0.30, a held sustain
and Unit Character 0 — the corner the cascade actually runs on reads
189.97 / 458.19 / 1985.03 Hz at velocity 0.2 / 0.5 / 1.0, a span of
4062 cents, where the three used to be the same 1985.03 Hz. It remains an
extension in both paths: at depth zero, and at velocity 1.0 whatever the depth,
the gain is exactly 1.0 and the render is bit-identical to the hardware-faithful
one, which is what the suite asserts rather than a recorded hash.

The host preset rail recalls the factory bank with a stepper, name list,
RELOAD and EDITED lamp. It shows the same program as the host and the controls
are synchronised to the complete selected program on the first editor frame,
before the first audio block. Cold construction explicitly applies INIT through
the same complete recall path used later, preventing a default change from
leaving the preset name and panel out of step. RELOAD discards all control edits.
Product/host programs include volume, bender depths, portamento, assign mode and
extension controls. Imported SysEx and incoming MIDI Program Changes retain the
narrower hardware semantics: they recall tone memory without moving those
surrounding controls.

## Original factory bank

YouKnow106 includes all 128 original tone-memory states in the physical
instrument's order: A11 through A88, then B11 through B88. Each entry is the
hardware's complete 18-byte state—sixteen 7-bit control bytes and two packed
switch bytes—with no corrective gain, hidden EQ or other per-preset
rebalancing. The concatenated 2,304-byte payload has SHA-256
`394ae874da33aa63fa4833932fbf415546d2ad66b1b6b9a36315601799eeec21`.
The test suite locks the same bytes with dependency-free FNV-1a
`0xa78dab9d5bafb386`, plus the slot order and an encode/decode round-trip
for every tone.

The packed mode byte is decoded in the hardware order: bit 1 is negative VCF
envelope polarity and bit 2 is VCA Gate. In addition to direct bit fixtures,
A86 Hand Claps, B31 Brass and B82 Piccolo Trumpet guard that meaning
semantically; swapping the two bits can still round-trip perfectly while
making those factory sounds nearly silent.

The bytes were mechanically decoded and cross-checked with zero mismatches
across the public [Hinzen tape/PAT archive](http://www.hinzen.de/midi/juno-106/),
the [Jarvik7 librarian factory library](https://www.jarvik7.net/juno-106/), and
the [KR-106 archival transcription](https://github.com/kayrockscreenprinting/ultramaster_kr106/tree/bc15caee5843ab238a25d0969e68d57db2b1615f/tools/preset-gen).
Roland independently describes the historical set as 64 Bank A plus 64 Bank B
in its [Original 128 announcement](https://www.rolandcloud.com/home/news/the-original-128-patches-for-the-juno-106-are-now).
No Roland Cloud product content was downloaded or extracted.

The complete [factory gain audit](Docs/audio/factory-presets/README.md) renders
all 128 tones through the shipping engine at 48 kHz/HQ with no per-preset
normalisation. Its stress score found finite output for every tone, a median
gated RMS of −21.480711305 dBFS, no preset below −60 dBFS maximum 400 ms RMS, and 31
tones whose polyphonic/transient peaks crossed 0 dBFS. Those crossings are
reported, not silently limited or rebalanced: the model intentionally permits
floating output, the score includes unison and six-key stress, and the absolute
output reference remains the OQ-06 measurement question. The nominal common
VCA LEVEL law is circuit-derived; OQ-02 now asks only how installed component,
rail and IC variation moves it. The ten preview WAVs use one disclosed
0.543091 (−5.30 dB) common gain so their relative levels survive 16-bit
delivery without clipping.

The hardware stores positions, not patch-name text. Names such as “Brass Set 1”
and “Owgan” are conventional archival descriptions shown for navigation, not
bytes recovered from the instrument and not claimed as Roland-authored names.
For the few labels that explicitly say unison or one octave up/down, host and
patch-bar recall also restores that playing setup. Hardware MIDI Program Change
and SysEx remain authentic: those operations change only the 18-byte tone and
leave performance controls where the player set them.

Saved-state schema 3 marks the change from the former 32 original YouKnow106
programs. Loading an older state preserves every saved parameter—and therefore
its sound—but resets the selector to an edited INIT/custom panel instead of
attaching an unrelated historical factory name. Program-index automation owned
by a host cannot be rewritten by the plug-in.
See [third-party notices](THIRD_PARTY_NOTICES.md) for provenance and
redistribution caveats.

## MIDI

The on-screen keyboard matches the instrument's physical 61-key C2-C7 span.
That visual limit does not discard host MIDI notes outside the keybed.
The adjacent on-screen bender lever feeds the instrument internally and springs
back when released; it does not emit MIDI. YouKnow106 also receives external
pitch bend, modulation (CC 1), hold (CC 64), all-notes-off and the reference
instrument's Patch Selection Program Changes. CC 1 and the bender lever's
upward axis drive the same LFO trigger path; BENDER LFO determines its depth.
The modelled keybed is not velocity sensitive, so incoming note velocity
reaches the engine through the VELOCITY extension and nowhere else: at its
default zero every note plays at the hardware's fixed value, and turned up it
scales both that voice's amplifier control and the ENV amount into its filter.
MIDI has no continuous controller
assignments for the synthesis panel. Host automation reaches every stored
parameter through the plug-in's own parameter list.

The incoming Program Change map follows the owner's manual exactly: 0..63
select A11..A88 and 64..127 select B11..B88, including every row and column in
both 64-tone groups. Incoming Program Changes are consumed rather than echoed.
YouKnow106 does not transmit performance data or Program Changes; that
reference-keyboard behavior is distinct from patch-selection receive. The
compact editor deliberately exposes no live patch-dump transmit control;
patch files move through the utility bar's LOAD and SAVE keys instead.

### System exclusive

The SysEx codec reads and constructs the hardware's own format in both
directions:

| Message | Bytes | Codec support |
| --- | --- | --- |
| Patch data | `F0 41 30 0n <18 tone bytes> F7` | decode and encode |
| Parameter change | `F0 41 32 0n <parameter> <value> F7` | decode and encode |

An incoming patch dump moves the whole panel; an incoming parameter change
moves only the controls that one byte names and leaves the rest of the patch
alone, so a librarian editing one control does not overwrite the others.
Outgoing patch construction and the bounded processor handoff remain tested for
integration use, but no transmit operation competes with the synthesis controls
on the editor. Messages from other manufacturers, other opcodes, and bodies of
the wrong length are ignored rather than partially applied.

Patch files carry the same messages. The utility bar's LOAD key — or dropping
a `.syx` file anywhere on the editor — applies the first patch dump the file
holds, exactly as if it had arrived over MIDI, and SAVE writes the current
tone as one hardware-valid dump on the channel the last incoming SysEx used.
A file holding a whole bank applies its first patch: with one edit buffer and
no user bank, applying all 128 in order would silently keep only the last.
Performance controls stay out of the file, exactly as they stay out of the
hardware's tone memory.

The layout is the instrument's: sixteen continuous controls at 0..127, then two
packed switch bytes. `Source/DSP/YouKnow106SysEx.h` is JUCE-free, so the suite
asserts the byte layout directly.

The chorus field has exactly the hardware's three states: Off, I and II. The
owner's manual says I and II cannot be used simultaneously, and the jack board
receives one enable line plus one binary I/II line. The two panel buttons are
therefore mutually exclusive. Sessions made by an older YouKnow106 build may
contain its invented both-buttons state; loading one canonicalises that state
to II. It is never rendered as a fourth chorus programme or emitted as a
special patch state.

## Build on macOS

```bash
cd youknow106
./scripts/build-macos.sh
```

The script configures with Xcode, builds universal `arm64`/`x86_64` binaries,
runs the CTest suite, and ad-hoc signs the resulting bundles under
`build-macos/YouKnow106_artefacts/Release/`. It needs CMake 3.22+ and a full
Xcode installation selected for command-line use. First-time configuration
fetches JUCE 8.0.14, pinned to an immutable archive and SHA-256; a local
checkout of that exact release can be supplied through `JUCE_PATH` instead.

Set `BUILD_UNIVERSAL=OFF` for a native-architecture-only build.

## Build and test without JUCE

The DSP core, chorus, panel description and render tools are JUCE-free, so the
non-plug-in suites build and run on any C++20 toolchain — which is what Linux CI
exercises:

```bash
cmake -S youknow106 -B youknow106/build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DYOUKNOW106_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build youknow106/build-dsp --parallel
ctest --test-dir youknow106/build-dsp --output-on-failure
```

There are 15 JUCE-free CTest contracts, and 16 with the plug-in suite enabled:

- **`YouKnow106.Circuit`** compares the model against something independent for
  every block: the four transconductor stages against an explicit reference
  solve of the same continuous ODE *and* the closed-form `1/(4 − k)`; the note
  timer against integer division; the cutoff law against the instrument's two
  service calibration anchors; and the delay line against its part's datasheet
  delay range. It also sweeps
  all 128 resonance panel bytes below the oscillation threshold to prove the
  frequency correction is identically one there and continuous across it, and
  re-solves the chorus line-noise amplitude from the MN3009 noise row rather
  than re-asserting the constant.
- **`YouKnow106.Engine`** checks what the instrument does when it is played:
  that RANGE transposes by octaves, that the sub is an octave down, that its
  legacy HQ-on 8' saw fixture at MIDI 60/84 keeps the worst single off-harmonic
  bin below −70 dBc, that the ramp's harmonics follow `1/n`, that a
  seventh held key is dropped rather than stealing a voice, that unison does not
  acquire artificial detune, that assign-mode changes and Solo Unison key-ups
  rebuild from the still-held physical keys, that output level is independent
  of host rate and of oversampling, that final PWM DC is removed, that an HQ
  transition cannot expose a chorus-state reset, that the engine is
  deterministic and exactly silent when idle, and that hostile automation
  cannot produce a non-finite sample. It fences this pass's six corrections in
  the same terms: that LFO DELAY holds pulse width as well as pitch and cutoff,
  including through the idle-priming path, and that the panel help describes
  that same three-destination route; that resonance leaves the rendered corner
  alone at three converter codes; that the node the amplifier multiplies carries
  no duty-dependent offset; that the idle output floor sits on the MN3009 noise
  row; that the warm-up clock reaches 900 s at every rate and in both quality
  settings; and that velocity moves the corner monotonically while its two exact
  identities stay bit-identical. It also renders every historical factory tone
  without per-preset normalization and rejects non-finite or runaway output.
- **`YouKnow106.PluginProcessor`** (macOS/plug-in builds only) checks the
  parameter contract, state round-tripping and migration, controller transport,
  legacy/modern automation ordering, exact patch reload, all 128 incoming
  Program Change locations, complete host-control restoration, and that the
  editor lays out and renders at its extreme sizes.
- **`YouKnow106.SysEx`** checks the documented hardware messages byte for byte,
  including malformed-message rejection, single-parameter switch decoding, all
  128 factory round-trips and the canonical corpus checksum.
- **`YouKnow106.RenderDemos`** smoke-tests the deterministic documentation-audio
  renderer against the shipping DSP path.
- **`YouKnow106.AuditFactoryPresets`** smoke-tests the long-form, JUCE-free
  128-tone gain auditor and common-gain factory-preview renderer. A full run
  writes its CSV, report and previews under `Docs/audio/factory-presets`.
- **`YouKnow106.RealismComparisonContract`** rejects a same-length but
  hash-mismatched baseline, a non-before manifest and a same-build DSP
  fingerprint before any strict comparison claim can be emitted.
- **`YouKnow106.OversamplingAuditContract`** compares shipping and active-counter
  raw-float fingerprints, then fences the selected semantic-work algebra on
  every production 4×/2×/1× branch. It distinguishes legacy input-support
  frames from exact input/output advances and proves six physical coordinate
  updates and 60 transition MACs per exact advance. Step 10 additionally locks
  the fixed Merson VCF algebra; Step 11 locks fractional-event peek/commit,
  affected-interval, seven-node and six-extra-map counts without changing the
  ten RHS evaluations per VCF step. Step 12 separately counts all 16 passive
  destinations while retaining the Step 11 VCF subset and fixed VCF/BBD
  algebra. No counter symbol or string may be present in the preprocessed
  shipping target.
- **`YouKnow106.DcoScanQualityContract`** drives the shipping DCO and half-band
  boundary through the expanded 44.1/48 kHz matrix, validates every spectral
  mask with an analytic multi-line control, and locks all six reviewed DCO
  passes alongside the normalized-scan and DCO/PWM/SUB recurrence checks. Its
  reported alias metric is the worst single off-mask FFT bin, not an integrated
  alias floor; passing it establishes numerical fidelity to the declared model,
  not hardware proof.
- **`YouKnow106.VcfBbdQualityContract`** compares the shipping VCF and one
  deterministic BBD line at common host boundaries through an independent
  4,097-tap `q=16` FIR, with RK64/RK128 VCF solves and closed-form
  component/128-edge-transfer/ZOH BBD phasors. The VCF result is explicitly
  nominal Character 0; the low-drive BBD reference uses the same documented
  model anchors and is not hardware truth. The contract also fences the causal
  four-point edge-input history, combined six-state continuous support
  transitions and exact clock/bucket/transfer state. It locks both the six-cell
  common-host matrix and the ten-path production-selector matrix; an HQ-off
  nonregression pass is reported separately from an absolute numerical pass.
- **`YouKnow106.VcfIntegrator`** fences causal-cubic reconstruction, the two
  fixed Merson halfsteps, independent RK96 dynamic agreement, alternating
  endpoint controls, independently evaluated segmented 522 µs hold nodes,
  explicit seven-node control trajectories, cap stability for every actual
  cold/warm card at Unit Character/calibration 2, hostile-input recovery and
  physical-state retiming.
- **`YouKnow106.VcfDynamicQualityContract`** executes the exact 23-write
  converter order and fractional analytic 522 µs cutoff/resonance hold
  trajectories across all cards and Character profiles. Nineteen physical
  takes represent 24 logical profiles. It retains all six standard HQ paths
  and both 8/768 kHz engine bounds against independent RK64/RK128 references.
  Step 13 adds five actual-selector HQ-off q1 moving-control rows—8 kHz as a
  separate endpoint, then 44.1/48/88.2/96 kHz—and four standard-rate static
  nominal hot rows. The full 19/24 schedule applies only to the moving profile;
  combined admission requires both profiles, leaving all four standard q1 rows
  REJECT under unchanged −40 dB waveform, <−60 dBc residual off-mask and
  ≤−85 dBc oracle-control gates. At every q1 rate the contract verifies pure
  peek, payload latch, normal-poll commit and pass-wrap resonance semantics;
  five real `renderVoice` wiring probes detect a disconnected trajectory, and
  q1 late/ceil and early/floor snap mutations still reject.
- **`YouKnow106.BbdDynamicQualityContract`** calls public
  `Chorus::process` for both lines and follows the real Engine selector through
  the shipping `downsamplePair` implementation/cascade. Its ten rows are the
  actual six HQ selectors—44.1/48 kHz q4, 88.2/96 kHz q2 and
  176.4/192 kHz q1—and four HQ-off q1 selectors at 44.1/48/88.2/96 kHz.
  An independent continuous-clock, nonlinear 128-cell, stochastic and
  six-state-support oracle crosses a checked 4,097-tap q16 host boundary.
  The 0.72 s public schedule is Chorus I, Off, Chorus II, Off; its analytic
  997/5,213 Hz card reaches 1.500000 Vrms at the model input-support boundary.
  Whole L/R/M/S, I/Off/II, exhaustive residual and RK4 convergence gates are
  absolute; noise waveform NRMS is informational because fractional edge time
  decorrelates samples, while exact RNG/edge/state ledgers plus level,
  four-band Welch power, correlation and II/I-delta gates are normative. Full
  mode-I and mode-II cycles are ledgered at all six unique internal grids, and
  clock/stereo/bypass/transfer/noise mutations must reject. The result is six
  HQ **PASS** and four HQ-off **REJECT**. The boundary excludes the surrounding
  full `Engine::process` clip/slew and output call site.
- **`YouKnow106.PassiveHoldTimingContract`** drives the actual process scheduler
  and the six voice-VCA, common-VCA, SUB and two-pole PWM physical states through
  1,105 host/rate/mode/position cases plus 17 later-4×/block-wrap cases. An
  independent long-double piecewise one-pole and affine two-pole oracle fences
  fractional-event accuracy, exact-zero/near-one interval ownership, target
  payload retention, 23/16 scheduler classification and zero Pitch/NOISE peeks.
  Per-destination late, early and disconnected mutations must reject, as must a
  sequential-PWM mutation; separate common-VCA and SUB probes prove the states
  reach their shipping consumers.
- **`YouKnow106.HighPassNetworkContract`** independently stamps the nominal
  fixed-position C14/HPF component network, checks the analytic transfer of the
  realized scalar coefficients, and probes the production shared-HPF updater
  on nine endpoint/common/oversampled policy grids. It rejects the old R39-only
  Cut load, a wrong-side 1 MΩ load, swapped Cut capacitors and a disconnected
  runtime coefficient path, while explicitly classifying the 8 kHz Cut rows
  and 32 kHz HQ Cut-III row as endpoint-limited. It makes no CMOS switching,
  charge-injection or click claim.
- **`YouKnow106.NoiseSourceQualityContract`** independently derives the
  C41/R79 corner, binds the actual main-noise updater over 4,007 dense/seam
  cells and exercises the public seeded Engine path from idle into driven
  noise without reset. It requires finite positive coefficients and stable
  support poles, preserves 12 cap-inactive current/legacy identities and
  rejects nine unstable or stable-but-wrong cap/sanitize mutations. Its
  1.697765947 dB cap-active analogue-RC envelope is numerical qualification,
  not a TP8 PSD or amplitude claim.

The current [audition index](Docs/audio/README.md) covers the Step 16 ten
demonstrations and the linked 128-row factory audit with ten common-gain
previews. Two demo renders and two complete factory renders were run from the
frozen engine into independent renderer-owned directories; each pair was
byte-identical. The factory report reconciles all 128 finite unique rows
(median gated RMS `−21.480711305 dBFS`, 31 overload flags, zero near-silent
presets and nine outside `±18 dB` of the corpus median). Historical comparison
corpora remain available from Step 9 history but are deliberately not carried
forward as current evidence.

The dated Step 12 shipping qualification used a warning-clean native
Release/plugin-off build and passed all **12/12** contracts in **323.07 s**.
Five focused ASan+UBSan gates also passed with
halt-on-error and no diagnostics: Engine passive-hold-only, independent passive
hold, full DCO quality, oversampling normal/work parity and dynamic VCF. The
universal Release/plugin-on build passed **13/13 in 344.05 s**. Its VST3, AU
and Standalone executables each contained `x86_64 arm64`, passed strict/deep
ad-hoc signature verification after packaging and targeted macOS 11.0. A
genuinely translated Rosetta `x86_64` passive-hold run passed in **0.55 s**.

Step 13's audit-only verification is separate. The final warning-clean native
Release/plugin-off tree passed **12/12** contracts in **375.88 s** while the
translated audit shared the machine; the rebuilt registered dynamic contract
also passed alone in **38.90 s**. Its focused ASan+UBSan self-test passed with
`halt_on_error=1`, `detect_leaks=0` and no diagnostics. A universal audit
executable containing `x86_64 arm64` and targeting macOS 11.0 passed natively
on arm64 and in a genuinely translated Rosetta `x86_64` process in
**963.10 s**. The first x86 run exposed a 0.103 dB variation only in the moving
RK64/RK128 fingerprint near −158 dB; the final audit therefore gives that
non-admission fingerprint an explicit ±0.15 dB portability band while keeping
NRMS, snap and hot fingerprints at ±0.05 dB and every absolute gate unchanged.
No shipping DSP, plug-in artifact, CPU path or audio corpus was requalified or
changed by this audit-only step.

The Step-14 inventory is **13 plugin-off / 14 plugin-on** contracts. Its
targeted native registered BBD dynamic contract passes in **44.12 s** (its own
reported audit elapsed time is **43.89 s**). It reports exact selector,
filter response/metadata/alignment, structural schedule and six-grid full-cycle
ledgers, raw-family identity, metric goldens and mutation sensitivity all PASS;
the six HQ quality rows PASS and all four HQ-off q1 rows REJECT as required.
The fresh warning-clean native arm64 Release/plugin-off tree passes **13/13 in
381.25 s**; within that full suite the BBD dynamic contract takes **43.46 s**,
the VCF dynamic contract **37.30 s** and passive hold **0.62 s**. A fresh
warning-clean ASan+UBSan build passes the existing static VCF/BBD seam and new
BBD dynamic audit **2/2 in 126.85 s** (40.80/86.05 s), with
`halt_on_error=1`, leak detection disabled and zero diagnostics. A fresh
universal `arm64;x86_64` Release/plugin-on all-target build passes in
**114.17 s**, targeting macOS 11.0; only nested-Make's inherited jobserver
notice and the pre-existing `YouKnow106Engine.h:431/787` `-Wfloat-equal`
warnings remain, while the Step-14 audit is warning-clean. The universal
serial matrix passes **14/14 in 400.62 s**; its BBD dynamic, VCF dynamic and
PluginProcessor tests take **44.86/38.11/11.93 s**. The explicit universal
full audit passes on arm64 in **43.69 s**; its binary contains `x86_64 arm64`
and both slices target macOS 11.0. A genuine translated full-oracle launch
printed `uname -m=x86_64` and `sysctl.proc_translated=1` but was intentionally
stopped at **2666.66 s**, still in the first hot RK4×4 solve: x86's 80-bit
`long double` reference arithmetic is software-emulated on arm64 and projects
to a multi-hour run. That incomplete launch is neither a PASS nor a quality
failure; the continuous oracle is not the Rosetta admission gate, and no full
x86 audit is claimed. At frozen audit-source SHA-256
`33a0818c00560a502fa774223030409a4310ffe0053df3e23ae5bc5aad348228`, a
warning-clean universal target rebuild passes in **3.15 s**. The bounded
shipping-only `--shipping-self-test` bypasses all audit alignment, reference
and audit-FIR work while retaining the raw internal and actual shipping
decimator boundaries. It passes on arm64 in **0.90 s** self-reported
(**1.37 s** external wall) and in a genuine translated Rosetta process in
**3.75 s** self-reported (**3.96 s** external wall), where it prints `x86_64`
and `sysctl.proc_translated=1`. All ten public `Chorus::process` selector rows
pass the input-support card, raw-boundary and decimator-boundary finite checks,
hot/noise schedule ledgers, within-run same-family identity, and both full mode
cycles on all six grids. No audit reference, audit FIR, RK, continuous-oracle,
quality-classification or mutation path runs or prints in this mode.
This is shipping/ledger portability evidence, not a continuous-reference x86
pass. Prescribed isolated packaging passes in **3.41 s**: VST3, AU and
Standalone each contain
both slices at minimum macOS 11.0 and pass strict and deep ad-hoc verification,
with CDHash prefixes `7a102a35…`, `21b94c10…` and `fb7f0da6…`, respectively.
Step 14 changes no `Source` or `Tests` file,
shipping DSP, selector, state, latency, CPU path, hardware claim or audio
sample.

Step 15 adds `YouKnow106.HighPassNetworkContract`, bringing the CMake inventory
to **14 plugin-off / 15 plugin-on** contracts. A fresh warning-clean native
Release/plugin-off build passes **14/14 in 367.27 s**; Engine, Circuit and the
new HPF contract take **175.77/3.88/0.77 s**. Focused ASan+UBSan Circuit and HPF
coverage passes **2/2 in 8.41 s** (7.50/0.91 s) with
`halt_on_error=1`, `detect_leaks=0` and no diagnostic. A fresh universal
`x86_64;arm64` Release/plugin-on build completes in **102.30 s**, registers 15
contracts and passes **15/15 in 382.36 s**; HPF and PluginProcessor take
**0.98/11.49 s**.

The packaged VST3, AU and Standalone app each contain `x86_64 arm64`, target
minimum macOS 11.0 and pass strict and deep ad-hoc signature verification;
their CDHash prefixes are `965c40c0`, `9290dacb` and `26f74b2a`. The explicit
HPF audit passes natively on arm64 in **0.42 s** and in a genuine translated
Rosetta `x86_64` process in **73.91 s**. Its printed metrics agree to displayed
precision; only three equal-valued frequency locations differ for Flat's
near-zero magnitude maxima in the analog, 8 kHz endpoint and 32 kHz endpoint-HQ
rows, which does not affect a gate or result.

Three alternating base/current CPU pairs keep every exact raw-float
fingerprint identity. Worst current load is **0.837× realtime**, the largest
positive meta-median change is **+0.1128%**, and the worst individual paired
change is **+2.1991%**; all remain below the predeclared 5% fence. This scalar
correction adds no per-sample work, state, storage or latency and makes no
hardware-switch claim.

The verified non-audio source SHA-256 set is:

| Source | SHA-256 |
| --- | --- |
| `CMakeLists.txt` | `33b31ca661c1538d19dcafac12add1838e576ff074399069eb2a7744d60ba524` |
| `Source/DSP/YouKnow106Engine.cpp` | `ed8fef679a94b0667569e1b0281f4381a46aa942c490be9b4765b445e1963182` |
| `Source/DSP/YouKnow106Engine.h` | `9ae15f16b795bf752693eb146c137a63f486d1ee29148dce0b38c58fec453b52` |
| `Tests/YouKnow106CircuitTests.cpp` | `a3f6168c3602cee5345e21e1e2b564b67e7a3981082ca0604dca74be3d59d998` |
| `Tools/AuditHighPassNetwork.cpp` | `341030ab93d8506547176dd30c27ea65684bd96a0a92d0ee681da23b953866eb` |

The audio handoff is frozen at demo/factory twin manifests
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
and
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`,
renderer-owned manifest
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`
and canonical manifest
`0280ae697c209f513283b0c1cac3ad451528f5e6909046ba26d592dce459a430`.
The validation and bounded deltas are recorded above. **Step 15 is complete.
DOCS FROZEN.**

Step 16 adds `YouKnow106.NoiseSourceQualityContract`, bringing the current
CMake inventory to **15 plugin-off / 16 plugin-on** contracts. A fresh native
arm64 Release/plugin-off configure and all-target build complete in
**1.21/18.57 s** with zero warnings; the serial matrix passes **15/15 in
381.56 s**, including Engine/Circuit/Noise at **181.42/4.12/1.81 s**. A fresh
ASan+UBSan configure/build completes in **1.31/15.04 s**, also warning-clean;
focused Circuit/Noise coverage passes **2/2 in 10.29 s** (7.64/2.65 s) under
`halt_on_error=1`, `detect_leaks=0`, with zero diagnostics.

The fresh universal `arm64;x86_64` Release/plugin-on configure and all-target
build take **33.9/121.7 s**. Its exact 16-test serial matrix passes **16/16 in
401.47 s**; Noise and PluginProcessor take **1.69/11.97 s**. The only build
warnings are the two pre-existing Engine-header `-Wfloat-equal` sites repeated
by universal translation units; the new audit target is warning-clean.
Prescribed packaging completes in **3.92 s**. VST3, AU and Standalone each
contain `x86_64 arm64`, target minimum macOS 11.0 and pass strict/deep ad-hoc
signature verification; their full CDHashes are respectively
`340ce9f3a80aeb589582911db16d66b37b49cab5`,
`39d3767acba6afca02d0a0402fd641d8d44c5293` and
`afc0333071a2b1ebdd3f7414d8bcc1402eed361c`. The ZIP and unsigned PKG are
21,976,146/21,971,836 bytes with SHA-256
`5ac16567328a48181f1e6a86d51b69cdafed0f67b617f0da22ec5e2564403bab` and
`d5295d361c64d786b2ef23c98fd8fece7b6e6576d41bda11b4acd791fe64feb4`.
The universal audit binary passes explicitly on arm64 in **1.769 s** and in a
genuine translated process printing `x86_64` and
`sysctl.proc_translated=1` in **56.952 s**. Scalar metrics agree; raw hashes
are asserted only for current-versus-legacy identity within an architecture,
not across different libm/FP implementations.

Three alternating seven-repetition CPU pairs retain exact normal/work/base/
current semantic fingerprints with no counter leakage. Their eight 4×/1×
meta-medians span −0.471% to **+0.334%**, global ratio is **1.000678**, worst
pair median is +3.068%, and worst current raw load is
**0.972737× realtime**. One isolated +12.766% raw timing outlier remains in
the record but does not move the robust paired classification.

KR-106 issue 16 records 96 kHz/24-bit calibration work on one JUNO-106,
serial 439522, using Borish replacement voice chips and recalibrated in 2022.
The surviving archive has incomplete direct-link provenance and is retained
only as a deferred lead. Its VCA
slope/endpoint and oscillator-level ratios are leads for OQ-15, not authority
to retune a nominal law; the source/mixer path may be original while the
voice VCF/VCA path is not. It cannot supply the missing raw TP8 noise PSD or
amplitude evidence in OQ-16. Step 16 changes no state, storage, latency or
per-sample work.

Final audio qualification is an exact-identity result, not an audibility
claim. Two sequential demo renders take **96.24/94.13 s**; two full 128-preset
factory renders take **440.98/461.15 s**. Each pair is byte-identical and also
exactly matches the maintained Step-15 output. The demo and factory manifests
remain
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`
and
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`;
the combined renderer-owned 22-file manifest is
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`,
and the current canonical 23-file manifest is
`8346a817bd215808112510dc3d37b5a8fac3a5f401aa93d117b2b9f0912ba8dd`.
Only `Docs/audio/README.md` changes for Step-16 provenance, at SHA-256
`8e4333223c3d58406be7919d7959327029094a7559e57d1733c9c5c943dd2483`.
Candidate demo/factory binary SHA-256 values are
`0ae8dec6e0ddec230aab5fbb8b8efbd63a4875900721ee60aff5371468fd9cd3`
and
`e74569b26d5bc8437a0c88b325d55db4bba7730e8fa40a391b2273b18aa08498`.

All 20 WAVs are finite non-silent stereo PCM16 (ten 44.1 kHz, ten 48 kHz),
with maximum absolute DC **0.000000592814 FS** and worst edge
**0.004486083984 FS / −46.962652 dBFS**. The CSV has 128 finite rows, unique
slots and unique tone states, median **−21.480711305 dBFS**, 31 overload,
zero near-silent and nine outliers; its range is A86
**−61.956882039 dBFS** to A48 **−8.749547764 dBFS**, with common gain
**0.543092**. No WAV, metric CSV, preview or renderer-generated factory text
changes. **Step 16 is complete. DOCS FROZEN.**

### Step 17 qualification — startup-only shared-hold priming

Step 17 repairs one verified product-lifecycle defect. The old
`outputPathIdle` predicate became true again after 40 ms of silence, so a
host's routine block snapshot could directly rewrite RESONANCE, common VCA,
PWM, SUB and NOISE holds. That shortcut bypassed the exact 23-write scan and
the existing 522 µs, 9.08249 ms, 4.7/2.632 ms and 10 ms hold dynamics. The
correction permits that shortcut only before the first valid positive-length
prepared process call; later silence cannot re-arm it, while hard reset and
`prepare()` deliberately begin a new startup window.

This is a lifecycle correction to the already declared compatibility model,
not a new hardware-timing measurement. The normalized scan phase, queue order,
destination ownership, component/voiced hold laws and their evidence labels
remain unchanged; OQ-07 and OQ-08 stay open, as do every other standing open
question. The correction reuses the existing lifecycle state and
adds no state, storage, latency or per-sample work.

- **Focused regression and mutation evidence:** the expanded
  `testIdleSnapshotPrimesEverySharedHold` and new
  `testStartedIdleEditsUseTheOrderedConverterScan` pass together. They cover
  repeated pre-start snapshots, invalid/zero/unprepared process calls, more
  than 40 ms silence, immediate Note On, exact half-interval common-VCA event
  and next-poll commit, 257-frame block partitioning, panic, reset and the
  unchanged 41-sample latency. Restoring the recurring `outputPathIdle`
  policy rejects with six assertions.

A fresh warning-clean native arm64 Release/plugin-off all-target build takes
**8.23 s** and the exact serial matrix passes **15/15 in 473.02 s**, including
Engine/Circuit at **206.86/7.56 s**. A fresh ASan+UBSan Engine-target build is
also warning-clean in **8.81 s**; the two-regression startup gate passes in
**0.64 s** under `halt_on_error=1`, `detect_leaks=0`, with no diagnostic.

The final universal plugin-on all-target build takes **127.43 s**, registers
the exact 16-contract inventory and emits only the two inherited
Engine-header `-Wfloat-equal` warnings at lines 431/789. Its initial serial run
passed tests 1–15 before PluginProcessor exposed a stale test chronology: its
reference had startup-primed pulse while the MIDI path changed pulse at sample
1. The test-only correction keeps the full dump at sample 0 only on the
MIDI-driven path, preloads the reference from the same quantized dump before
prepare, and gives both paths the pulse edit at sample 1 and Note On at sample
2. That yields equivalent pre-first-render converter/hold chronology without
relaxing a threshold; the registered PluginProcessor contract then passes in
**11.40 s**. Thus all 16
exact contracts are green, transparently as the retained first 15 plus the
corrected focused rerun rather than one uninterrupted 16/16 log.

Prescribed packaging passes in **3.11 s**. VST3, AU and Standalone each
contain `arm64` and `x86_64`, target macOS 11.0 in both slices and pass
strict/deep signature verification. Their arm64/x86_64 CDHash prefixes are
respectively `8f07692a/059a8e10`, `1975c99e/17f254d8` and
`5a3f7ec2/8e46ef3b`. The ZIP/PKG SHA-256 values are
`a066e7d122c082e39702c5b5524f1de455c93c8ab756b86a0d2ed9ecc1fa7097`
and
`2e7972005be2944520acf86265201ddda99ff6d73819d24756a55c08f2f707c7`.
The focused startup gate also passes natively in **0.04 s** and under genuine
Rosetta (`uname -m=x86_64`, `sysctl.proc_translated=1`) in **0.24 s**.

Three alternating seven-repetition CPU pairs retain exact normal/work/base/
current semantic fingerprints and pass both work-counter self-tests. The
largest positive 4×/1× meta-median is **+1.630160%**, worst pair median is
**+2.939967%**, aggregate ratio is **1.003069949 (+0.306995%)**, worst
candidate median is **0.868× realtime** and worst raw repetition is
**0.882116×**. All remain below the predeclared 5% and realtime gates.

The final non-document source SHA-256 set is:

| Source | SHA-256 |
| --- | --- |
| `CMakeLists.txt` | `e200a56a5801f8e2ca80e42602ed6f4f65896b06d82e9153d16c89b7c657dae3` |
| `Source/DSP/YouKnow106Engine.cpp` | `45254c5659df29b3efbeebe6717af96544192bdc4ace7228df6bbdd1d875a824` |
| `Source/DSP/YouKnow106Engine.h` | `d0bb7d99a3de16dd0756ee43ff573283cf468d551fbc7e37797b26ab9054bbc1` |
| `Tests/YouKnow106EngineTests.cpp` | `5b59e992e956dbc4b640c2f096954ada627bc8476a02cb552d0b741984c3933d` |
| `Tests/PluginProcessorTests.cpp` | `3940edf6a9e695f8a56d14c83e51613b2214801d0cbb45b91b58b0df19d51d06` |

Exactly two sequential demo and two full-factory renders take
**92.32/92.70 s** and **454.40/509.67 s**. Both pairs and all 22 installed
Step-16 renderer-owned files are byte-identical, with demo/factory/combined
manifests
`b42e87351748d79ad91cfbfb29ca85fce99a08b0c2a090754c4cba7bf69a9434`,
`0783040d94af15527450f8062813ac03ae6c6def0184574c037a5cf4106767e8`
and
`bc1564713b46151a77fbbc3c5403f8bd829955cd9ff9dbcb5b2bd6cc1e13c614`.
The render binaries hash to
`ab1aa091310e764313ee91d0d8edd422cfe5b11863f69503a47f2fa008991bf4`
and
`415d4021f59d377142f8de9c59bb5f0051de44d647df338cbbbdd91dab995d91`.
Only `Docs/audio/README.md` changes for Step-17 provenance, at SHA-256
`a6bb49018b312bab2a8e82dcabb9bc105ccd19e076bf39ec0e580631108ed3aa`;
the installed canonical 23-file manifest is
`19053f2cb7b57eef5fccb7bfa9f7f5e14ab2e1e932af1672b5138565430d196c`.
All 20 WAVs and the 128-row factory audit retain their frozen Step-16 metrics.
No WAV, generated factory README, metrics CSV or preview changes. **Step 17 is
complete. DOCS FROZEN.**

To regenerate the full factory report and previews after a signal-path change:

```bash
cmake --build youknow106/build-dsp --parallel \
  --target YouKnow106AuditFactoryPresets
youknow106/build-dsp/YouKnow106AuditFactoryPresets
```

## Sign, package and notarize

```bash
cd youknow106
./scripts/sign-and-package-macos.sh
```

With no signing identities set this ad-hoc signs and produces an unsigned
installer, which is what the nightly workflow ships. For public distribution,
supply your own identities:

```bash
APP_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE=your-notary-profile \
./scripts/sign-and-package-macos.sh
```

## Layout

```text
CMakeLists.txt   Self-contained project; the DSP target builds without JUCE
Source/DSP/      Engine, chorus and the JUCE-free panel description
Source/          Plug-in processor and editor
Tests/           Circuit, engine and plug-in suites
Docs/            Circuit-modelling research, open questions, editor screenshot
Presets/         Sound-design recipes
Tools/           Deterministic demo and factory-audit audio renderers
scripts/         macOS build and packaging helpers
```

## Licensing

Original source under the [MIT License](LICENSE); see the
[third-party notices](THIRD_PARTY_NOTICES.md). YouKnow106 builds against JUCE,
which is separately licensed — review the JUCE 8 terms before distributing a
binary.

## Changelog

- 2026-08-15: Cached the shared envelope generator's attack/decay/release law so it is only resolved when a voice's panel position actually changes rather than recomputed from scratch on every voice's pitch write, with no change to engine output.
- 2026-08-15: Deduplicated the switch-byte-one/switch-byte-two decode logic that `patchFromToneBytes` and `applyParameter` each implemented separately in `YouKnow106SysEx.cpp` into two shared `decodeSwitchByteOne`/`decodeSwitchByteTwo` helpers, with no change to SysEx behaviour.
- 2026-08-15: Memoized the shared PORTAMENTO glide-rate lookup that every sounding voice's note-on and pitch write in `YouKnow106Engine` resolved independently from the same panel position, verified bit-identical against the rendered demo corpus and the full 15-contract test suite.
- 2026-08-15: Cached `rampCurrentScaleFor`'s per-card ramp-current tolerance scale on the voice (`Voice::rampCurrentScale`) so `renderVoice` reads the value `updatePulseComparator` already solved a moment earlier in the same internal sample instead of resolving it a second time, verified bit-identical against the rendered demo corpus and the full 15-contract test suite.

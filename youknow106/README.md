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
below; no Roland Cloud content was extracted. Its panel retains that
instrument's functional control set and its left-to-right reading order, under
an original livery; its composition, palette, typography and name are its own.

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

The current audit runs the shipping JUCE-free engine on one Apple M1 Max
thread, macOS 26.5.1, native arm64 Release, 48 kHz/block 256. Seven paired
runs at Unit Character 1.0 from repeatedly copied, rate-specific two-second
snapshots put 4× / 1× at **0.533 / 0.145×
realtime idle**, **0.495 / 0.152×** for six plain voices, **0.659 / 0.191×**
for the high-resonance fixture and **0.766 / 0.292×** for the full-mixer
Chorus-II fixture; every median absolute deviation is below 1%. These are
engine thread-CPU figures on one machine, not plug-in/host totals or a product
comparison.

A separate common-host numerical audit now treats 4× as a candidate, never as
truth, and rejects every tested 1×/2×/4× DCO, nominal-Character-0 VCF and
deterministic-BBD cell at both 44.1 and 48 kHz. Even at 4×, the decisive DCO
single-bin result is −42.62/−41.45 dBc against −70, the VCF hot-saw error is
−24.34/−25.81 dB RMS against −40, and the BBD analytic error is
−27.04/−28.18 dB RMS against −40. The exhaustive 20 Hz–20 kHz BBD
unmasked-bin SGA maximum is still −47.64/−38.19 dBc against a strict <−60
gate.
The independent-reference method and complete matrix are in the comparative
assessment. No production selector, rate, audio path or preset changed, and no
domain split is admitted.

> **Listen first.** Ten [rendered demonstrations](Docs/audio/README.md) cover
> the classic pad and PWM strings, the 16' bass, the self-oscillating filter,
> the chorus modes, unison glide, the delayed vibrato, the high-pass ladder
> and the optional deterministic Unit Character profile. They are rendered by
> the shipping engine, so they cannot drift from what the plug-in does. Ten
> additional [factory-preset previews](Docs/audio/factory-presets/README.md)
> retain their relative levels with one shared gain rather than per-file
> normalisation.

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
  MIDI assignment itself never writes an analogue hold: a retrigger retains the
  live capacitor state until that destination's scheduled converter slot.
  Playing latency is now measured separately from plug-in latency. At 48 kHz,
  an exhaustive 1,008-boundary sweep across all six physical cards gives
  event-to-Pitch-write 0/100/201 samples (min/median/max), event-to-ENV-mode
  VCA write 70/192/315, and event-to-63.2%-settled VCA hold 102/224/347 with HQ
  off or 103/225/348 with HQ on. For the declared C4 saw fixture, first stereo
  output above `1e-4` is 90/213/335 samples off and 93/216/339 on. Those are
  raw, uncompensated engine-output offsets. Separately, the plug-in reports 24
  samples of nominal numerical group delay to the host; that value is not a
  literal shift to subtract from this signal-dependent threshold crossing.
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
  voiced curve. Initial idle host-snapshot priming remains an explicit startup
  policy, not a circuit claim.
- **The chorus has no compander**, so it hisses — the hiss is modelled, and
  there is a control to defeat it that the hardware does not have. Its level
  is the MN3009's own noise row rather than a chosen one: the datasheet that
  already fixes this part's bandwidth and distortion in the model also
  specifies noise 0.2 mVrms max A-weighted. The amplitude each line writes at
  its clock edges is that row referred back through the model's own measured
  A-weighted transfer from the injection point to the wet output — 0.4034,
  through the hold, the tap-summing pole, both reconstruction sections and the
  wet coupling — and through the 2.6 V node coordinate, so the suite re-solves
  the equation instead of re-asserting the answer. That moved the baseline wet
  line down 14.39 dB, from 1.0488 mVrms A-weighted (1.0611 full band) to
  0.19978 mVrms in mode I; the circuit suite also recovers 0.20016 mVrms on
  mode II's faster clock programme after dividing out the instrument-level
  mode calibration described below. The
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
  device. A paper-motivated compact polyBLEP reconstructs only the deterministic
  held-output steps at their fractional internal-sample times, reducing the
  additional aliases made by the computer grid. It runs after the residual
  transfer-loss state and before the output tap pole; buckets, index, clock
  phase, transfer state, held noise and random-number sequence are unchanged.
  Stochastic BBD noise is deliberately not predicted or corrected.
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
  is 0.820915 Hz in Boost/Flat (`33k || 47k`) and 0.482288 Hz at the sub-hertz
  asymptote of Cut I/II, where their series cut capacitors are open.
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
  high-pass) and C41 100 pF against R79 330 kΩ on its output (4.82 kHz pole),
  so the rail loses the synthetic top-octave hiss a flat generator carried;
  the passband keeps the established density (OQ-16).

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
| Shared digital control generator | Envelope recurrence, sustain mapping, DAC truncation, LFO/delay arithmetic and portamento are ROM-resolved for the stated B-2 image. The 23 converter destinations, their ownership and the 4.2 ms pass are anchored. VCF and voice-VCA hold constants are component-derived, as is the common VCA LEVEL path's 9.08249 ms post-S/H C7 pole. | Writes retain the exact logical order but use normalized sub-pass spacing; exact timestamps and jitter remain open. A 48 kHz model characterization now sweeps all 1,008 host-boundary phases and six cards, separating Pitch write, VCA write, 687 µs hold settling and a declared output-onset proxy; its numbers describe this compatibility profile, not hardware. The remaining sample-and-hold slews are voiced, and initial idle host-snapshot priming is product policy — which now primes the PWM hold from the same gated LFO the pass distributes. The delay envelope gating the PWM write alongside the pitch and filter writes is internal consistency with the firmware's one attenuator and one LFO, not a documented statement that DELAY reaches PWM. |
| DCO, ramp, pulse, sub and mixer | The 8 MHz master reference, integer timer division, range clocks, pitch quantization, constant-current ramp, PWM comparator and divide-by-two sub topology are anchored/derived. A changed-pitch write occurs at that card’s converter slot. The moving-threshold solver prevents a digital-only missed PWM edge and full-cycle blip. | BLEP/BLAMP repairs are numerical antialiasing mechanisms, not evidence of transparency by themselves. The legacy full-engine HQ fixture covers only the 8' saw at MIDI 60/84 and clears its −70 dBc single-bin fence. The expanded isolated saw/sub/pulse grid instead rejects 1×, 2× and 4× at both common host rates; the current 4× worst bins are −42.62/−41.45 dBc against that same gate. The fold cause is deliberately unattributed. Exact restart electrical state, loaded saw/pulse/sub/noise levels, filter-drive budget and live waveform-switch transients remain approximated or open. Pulse currently uses a provisional instantaneous audio gate; no invented anti-click envelope is presented as hardware behavior. |
| Per-voice VCF | Four IR3109/BA662 transconductor stages, the 68 kΩ/560 Ω attenuation, 240 pF stages, per-card cutoff trims and service calibration anchors are hardware-fixed. Cutoff modulation is summed in converter counts before the exponential law. The upper knee is the transconductor's own control-current saturation near 64 kHz, and the converter's R-2R carry error rides on the code it produces. | A topology-preserving implicit Newton solve is the numerical realization; each stage's tanh is averaged exactly along its inter-sample drive path (the divided difference of ln cosh), which leaves the linear response and the calibrated limit cycle measured identical. An exhaustive 20 Hz–20 kHz hot-residual FFT, masking only ±6 bins around each legitimate output harmonic, clears its −60 dBc gate at 2×/4×; its independent-oracle leakage control is below −93 dBc. The independent RK64/RK128 common-host comparison still rejects every factor on full-waveform NRMS for the production-compensated hot-saw fixture: the nominal-Character-0 4× errors are −24.34/−25.81 dB RMS at 44.1/48 kHz against a −40 dB gate. Resonance byte-to-loop gain, input compensation and feedback saturation are voiced pending measurements; the oscillation frequency correction is no longer among them, having become the reciprocal of the droop the cascade's own limit cycle imposes on itself — a harmonic balance of the model's two existing nonlinearities on their two existing headrooms, identically 1 below the threshold and predicting the 248 Hz anchor to within a cent. The maximum loop gain (4.504) is the one quantity still fitted, to the 4.8 Vp-p amplitude anchor alone. The saturation exponent and the carry sizes are fitted to a third-party measured card, not to a Roland document. A research-only one-step quasi-Newton solver passes the static/RK64/residual/fold-back classes but fails nominal six-card scanned-control parity by up to +21.31 dB RMS across the engine-bound/standard-rate matrix, so it is rejected and remains outside the signal path. |
| Per-voice VCA | One BA662 follows each VCF. Roland shows VCF OUT pin 3 AC-coupled by C59 1 µF/50 V NP to VCA IN pin 9, and that capacitor is now in the signal path ahead of the gain multiply; the separate R106/C58/R105/Tr20 branch drives VCA CONT pin 11; VCA OUT pin 10 reaches TP8–TP13 and the 33 kΩ summer inputs. The service procedure trims VR30/25/20/15/10/5 through 2.2 MΩ for minimum thump and sets a 6 Vpp gain endpoint. | The current quasi-linear gain/onset/knee/deadband law is schematic-informed compatibility, not a measured BA662 transfer. C59's capacitance is the designator read; the pin-9 load it works against is voiced at 33 kΩ and bracketed 33–100 kΩ (4.82–1.59 Hz), because neither R108 nor VR27's setting is in tree — OQ-19 owns it, and the pole's job is insensitive to the choice inside the bracket. The nominal model adds no residual feedthrough: Unit Character's control-hold offset is not the VR30 signal-input null, and post-calibration thump magnitude/polarity/spectrum remain unmeasured. Velocity is an optional extension, inert at its default zero. |
| Voice sum, coupling, HPF and common VCA LEVEL | Six card outputs sum through 33 kΩ into 3.3 kΩ feedback (0.1 each). C14 precedes the shared four-position HPF; C12 then feeds the one common uPC1252H2 controlled by stored VCA LEVEL. Service Notes pp. 8 and 15, the ROM-resolved `d=b<<5` code and NEC's −5.9 mV/dB typical constant derive the nominal common-VCA law and C7 settling. | The complete coupled switched-HPF network and its switching memory are approximated; the bass-boost shelf itself is derived from the p. 15 branch (+10.50 dB DC, +1.41 dB high band, 59.41 Hz pole, within 0.016 dB of the exact two-zero/two-pole solve). The ideal 12-bit R-2R transfer assumes division by 4096; R32 now reads unambiguously as 1.5 kΩ in the complete scan, and real resistor/capacitor tolerance, rail error and uPC1252 variation still need an installed-unit sweep. |
| BBD chorus and IC6 mix | Two uncompanded 256-stage MN3009 lines, anti-phase modulation, continuously running bypass, support-filter parts, coupling capacitors and IC6 dry/wet resistor gains are anchored/derived. The mode rates are derived from the JUNO-106 timing network as 0.5532934/0.8982608 Hz. BBD write nonlinearity is fitted to its datasheet test points. At the raw held node, upstream of numerical output reconstruction, the explicit zero-order hold plus fixed per-shift residual coefficient is −3.000 dB versus DC at 12 kHz/40 kHz, or −2.972 dB versus the datasheet's 1 kHz reference. The mode-I/base per-line hiss amplitude is the same datasheet's noise row, 0.2 mVrms max A-weighted, referred back to the injection point through the model's own measured 0.4034 A-weighted transfer. | Sweep endpoints retain a calibrated sibling measurement of the shared clock driver; loaded support impedances and the wet-mute transient are voiced, as are stereo correlation and the optional common/hum/spur layer. The base hiss level is anchored to the part but sits inside a 10.5 dB bracket its own datasheet leaves open; the guaranteed maximum is the end this model takes. Mode II separately applies the reported approximately 3.95 dB complete-output factor from the same-chain real-unit captures. Treating their true-peak difference as a broadband amplitude factor is moderate-confidence policy, not a claim about the standalone part or physical insertion point (OQ-03). The 0.4034 transfer is a property of these filters at the 192 kHz HQ internal rate and reads 0.40 dB higher at 48 kHz with HQ off. The emitted waveform is no longer the literal raw rectangle: a deterministic-only polyBLEP after transfer loss reduces host-grid aliases before the tap pole. It is a numerical product mechanism, not MN3009 circuitry. The common-host analytic comparison rejects every factor. At 4×, low-drive NRMS error is −27.04/−28.18 dB at 44.1/48 kHz, the one qualifying wanted BGA line has 0.869/0.708 dB level error, and exhaustive 20 Hz–20 kHz unmasked-bin SGA is −47.64/−38.19 dBc. The closed-form oracle shares the documented component/model anchors and is not hardware truth. Panasonic's low-resolution typical curves at 10/40/100 kHz have been digitised at 600 dpi; their tracked-versus-broadband reading is self-contradictory, so one installed-unit tracked sweep still decides which interpretation applies (OQ-04). Loaded IC6 clipping remains unknown. |
| VOLUME and output boundary | C17/C20, R54/R57, the nominal-linear 10KB×2 tracks and fixed internal wiper loading are component-derived, with independent left/right capacitor state. | Dual-gang tracking, selector/jack normaling, external loads and headphone transfer remain open. The fixed −18 dBFS RMS mapping and provisional physical reference are product policy, not an analogue circuit claim. |
| Numerical cost | The implicit cascade takes `tanh` and `ln cosh` from one shared exponential, evaluates each stage's path-start antiderivative once per call rather than once per Newton iteration, and stops iterating when its step reaches single precision's own floor on volt-scale states rather than at an absolute threshold that floor can never cross. Settled per-card constants — the chassis gradient, the chassis-wide warm-up fraction, the counts-to-coefficient chain — leave the per-sample loops. A compile-time-only work audit now counts scan, DCO, VCF, BBD/BLEP and decimator events on every 4×/2×/1× production branch; a separate executable times the uninstrumented shipping library with a thread-CPU clock. CTest requires matching raw-float fingerprints between the normal and active-counter builds and exact structural algebra. | None of it is a hardware claim, and no constant, level, corner or law moves. The kernels are fenced against the standard library at one float ULP and the solve's residual is bounded independently; the loop-invariant work is bit-identical. A non-shipping candidate demonstrates fixed counts of one system evaluation plus two bidiagonal solves, but its engine-bound/standard-grid parity failure rejects that shortcut before production integration. The counters prove how work scales, not how expensive heterogeneous events are; whole-engine 4×/1× ratios of 2.59–3.68 on the declared machine do not predict a split-rate speedup or qualify any domain for a lower grid. The cost figures and bake-offs are in the [comparative assessment](Docs/comparative-assessment.md). |
| Antialiasing, HQ and safety | These are intended to preserve the modeled circuit’s behavior at host sample rates: bandlimited discontinuities, optional oversampling, Kaiser half-band decimation flat to 20 kHz at both common host rates, and state-preserving rate changes. HQ is currently one global internal loop: 44.1/48 kHz selects 4×, 88.2/96 kHz 2× and 176.4 kHz or above 1×; HQ off selects 1×. The engine and processor report a fixed 24-host-sample numerical latency on the 4×, 2× and 1× paths (0.500 ms at 48 kHz, 0.250 ms at 96 kHz and 0.125 ms at 192 kHz); shallower paths are padded to the deepest report. Nothing the quality switch selects is allowed to move a modelled physical quantity: noise density is normalized to elapsed time, and the warm-up clock accumulates wall-clock seconds in double precision so it reads the same at every supported rate and in both quality settings. The one measured exception is reported rather than claimed away: the BBD line hiss is written at the line's own clock edges and held, so a coarser grid folds more of that held sequence back into the band and the recovered wet-line level reads 0.40 dB higher at 48 kHz with HQ switched off. For the chorus, BBD-generated aliasing (BGA) means the physical-model images at `k*Fclock ± f`; simulation-generated aliasing (SGA) means the extra folds created by the internal sample grid. The bounded polyBLEP scheduler has 54 slots and uses at most 50 in the tested worst case, including multiple BBD edges in one internal sample. | They have no hardware counterpart. The reported 24 samples cover oscillator-reconstruction/decimation group delay only: converter scan, envelope/VCA-hold response, host/device buffering and wet BBD delay are separate. DCO, VCF/VCA, scan/holds, the complete BBD/support path and output slew all currently share that loop. In the main signal path, latency padding and C17/C20/VOLUME are the principal host-rate stages after decimation; output-boundary and product bookkeeping follow too. The BBD reconstruction is deterministic-only, leaves noise uncorrected, and clears its grid-specific correction slots on an internal-rate change while physical BBD and RNG state survive. It strongly reduces SGA; it does not preserve every BGA component exactly at LQ, so the measured HQ/LQ limits are reported rather than hidden behind a generic “transparent” claim. The common-host audit treats 4× as a candidate rather than truth and finds the expanded DCO, nominal-Character-0 VCF and deterministic BBD inadmissible at every tested factor. Passing normalized scan and DCO/PWM/SUB recurrences admits no audio domain, and no split is authorized without the missing inter-domain, whole-engine and latency qualification. The idle-only quality change and short safety fades are product mechanisms. |

The strict [BBD transfer/clock-law
comparison](Docs/audio/realism-comparisons/bbd-transfer-clock-law/README.md)
holds one bright full-engine fixture through both chorus modes and visits both
clock extremes, preserving raw before/after audio and one shared listening gain.

The strict [BBD host-grid alias
comparison](Docs/audio/realism-comparisons/bbd-host-grid-alias/README.md)
separates wanted BBD clock images from false host-grid folds at one shared gain.
At the minimum clock its two LQ false second-image folds fall from
−26.87/−27.42 to −55.23/−53.61 dBc; HQ moves the corresponding roughly −116 dBc
folds to about −171/−170 dBc. The wanted first image changes by −0.0383 dB in HQ.
Its −5.2986 dB LQ delta occurs on a −100.47 dBc baseline, not on a prominent
audio component. No subjective or installed-hardware result is inferred from
these deterministic numerical measurements.

The strict [voice-VCA feedthrough
comparison](Docs/audio/realism-comparisons/voice-vca-feedthrough/README.md)
opens all six silent cards at a fixed converter-scan phase. Raw peak falls from
−68.24 to −148.42 dBFS after removing the unsupported control-squared term; its
`-listen` files apply a disclosed fixed +30 dB diagnostic gain, never normalization.

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
reading order**. It is a 1120×628 console whose sound-shaping controls occupy
one continuous left-to-right row — VOLUME, LFO, DCO, HPF, VCF, VCA, ENV,
CHORUS — because that order *is* the instrument's ergonomics. An earlier
revision folded it into two rows; doing so broke the one relationship the panel
exists to show, and it is not worth the space it saved.

- Every section is as wide as what it holds, so a one-fader section is one
  fader wide and no card carries dead area. The two exceptions are VOLUME and
  CHORUS, widened to their own headers rather than left with a gap.
- Blue reads as the audio path — VOLUME, DCO, HPF, VCF, VCA and CHORUS; green
  identifies LFO and envelope modulation.
- Everything the hardware does not carry on that row sits on a separate lower
  deck: the vector lever, BENDER depths, assign mode, and the explicitly
  non-hardware CHARACTER and KEYBOARD CONTROL cards, drawn in a secondary
  weight so no extension can be mistaken for a stored tone parameter.
- The five service keys are not performance controls at all, so they sit on the
  utility bar beside the help text rather than on the instrument surface.

What remains deliberately this project's own is the livery, not the layout: the
slate/green/blue palette, the masthead and its telemetry, the clipped service
cards, the oscilloscope-grid motifs and the illuminated vector lever. Functional
waveform and foot-register marks are redrawn as project-native vectors. The
panel reads as a relative rather than a copy. This is a design choice, not a
claim of legal clearance.

Sliders, switches and buttons are still placed by the JUCE-free description in
`Source/DSP/YouKnow106Panel.cpp`, so tests prove that the row does not overlap,
escape its cards or shrink its legends below the readability floor — including
at the smallest window, which is the binding case and is what fixes the lower
deck's depth. A bundled low-contrast material scan adds maintained ABS grain,
polished touch wear, cleaning swirls and sparse hairline scuffs. Recessed fader
channels, bevelled and grooved caps, and inset illuminated switches add a
refined vintage material language while remaining project-drawn vectors. Those
legacy cues sit inside the distinct blue/green livery; they do not recreate the
reference faceplate.

Descriptions no longer float over the instrument. Hovering any interactive
element updates the fixed help display below the keys immediately.
All 55 public controls are covered: the dominant synthesis panel, six extension
knobs, compact operation and patch controls, program selector, 61-key keyboard
and pitch/mod lever. That strip also carries the hovered control's **current
setting**, in its own lit right-hand column and in the parameter's own units, so
reading a value no longer requires starting a drag. The same TooltipClient
strings remain accessibility metadata, while every no-text-box slider retains a
separate numeric value bubble during adjustment. Routing, minimum explanatory
length, stable help geometry and value-bubble presence are regression-tested.

The masthead oscilloscope ranges itself. The instrument's output convention puts
an ordinary patch near a tenth of full scale, so a fixed ±1 trace was a flat line
for most of what it plays; the trace now follows a slow-release peak, snaps to a
power-of-two gain and prints that gain on the screen, because a scope whose
sensitivity moves silently is not telling the truth about level. Its trigger
carries a hysteresis band scaled to the trace, so a near-silent buffer no longer
latches onto its own dither.

The vector lever is live performance input rather than a saved parameter. Drag
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

The masthead patch bar recalls the factory bank with a stepper, name list,
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
gated RMS of -20.75 dBFS, no preset below -60 dBFS maximum 400 ms RMS, and 32
tones whose polyphonic/transient peaks crossed 0 dBFS. Those crossings are
reported, not silently limited or rebalanced: the model intentionally permits
floating output, the score includes unison and six-key stress, and the absolute
output reference remains the OQ-06 measurement question. The nominal common
VCA LEVEL law is circuit-derived; OQ-02 now asks only how installed component,
rail and IC variation moves it. The ten preview WAVs use one disclosed -10.76 dB
common attenuation so their relative levels survive 16-bit delivery without
clipping.

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
The adjacent on-screen vector lever feeds the instrument internally and springs
back when released; it does not emit MIDI. YouKnow106 also receives external
pitch bend, modulation (CC 1), hold (CC 64), all-notes-off and the reference
instrument's Patch Selection Program Changes. CC 1 and the vector lever's
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

There are nine JUCE-free CTest contracts, plus the plug-in suite when JUCE is enabled:

- **`YouKnow106.Circuit`** compares the model against something independent for
  every block: the four transconductor stages against a fourth-order
  Runge-Kutta solve of the same ODE at 16x *and* against the closed-form
  `1/(4 − k)`; the note timer against integer division; the cutoff law against
  the instrument's two service calibration anchors; and the delay line against
  its part's datasheet delay range. It also holds the cascade's two elementary
  functions to one float ULP of the standard library, and bounds the residual
  the implicit solve leaves behind — measured by quadrature on `std::tanh`,
  which shares no code with the solver's own divided difference. It also sweeps
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
  every production 4×/2×/1× branch.
- **`YouKnow106.DcoScanQualityContract`** drives the shipping DCO and half-band
  boundary through the expanded 44.1/48 kHz matrix, validates every spectral
  mask with an analytic multi-line control, and deliberately locks the six
  reviewed DCO rejections alongside passing normalized-scan and DCO/PWM/SUB
  recurrence checks. Its reported alias metric is the worst single off-mask
  FFT bin, not an integrated alias floor.
- **`YouKnow106.VcfBbdQualityContract`** compares the shipping VCF and one
  deterministic BBD line at common host boundaries through an independent
  4,097-tap `q=16` FIR, with RK64/RK128 VCF solves and closed-form
  component/128-edge-transfer/ZOH BBD phasors. The VCF result is explicitly
  nominal Character 0; the low-drive BBD reference uses the same documented
  model anchors and is not hardware truth. The contract fences each reviewed
  metric and classification; a rejected cell is evidence, not a skipped or
  passing test.

Focused DSP changes use `YouKnow106RenderRealismComparison`. Unlike the older
16-bit comparison set, it archives raw float32 before/after/difference signals,
uses one shared listening gain, reports peak/peak and RMS/RMS nulls, and records
the source fingerprint, patch, seed and exact MIDI/control sample schedule.
The BBD host-grid fixture additionally requires its baseline manifest to bind
the scenario, protocol, frame count, raw sample hash and distinct pre-change DSP
fingerprint.
The [audition index](Docs/audio/realism-comparisons/README.md) links the listening
files in a useful order and states what each gain and difference track means.
Committed fixtures currently cover the
[retrigger/release-tail path](Docs/audio/realism-comparisons/retrigger-release-tail/),
the [common VCA LEVEL law and settling](Docs/audio/realism-comparisons/common-vca-level/),
the [BBD transfer clock law](Docs/audio/realism-comparisons/bbd-transfer-clock-law/),
the [BBD host-grid alias reconstruction](Docs/audio/realism-comparisons/bbd-host-grid-alias/),
and the [voice-VCA feedthrough path](Docs/audio/realism-comparisons/voice-vca-feedthrough/).
The common-VCA fixture records every automated byte and reports a −8.70 dBc RMS
difference between the superseded cubic/borrowed-slew model and the nominal
circuit solve. The VCA-thump fixture uses a protocol-fixed +30 dB diagnostic
gain because its raw before peak is −68.24 dBFS.

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

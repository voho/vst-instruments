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
[open questions](Docs/open-questions.md).

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
  The voice VCA that envelope drives is a *current*-controlled OTA with no
  volts-per-decade converter in front of it, so its gain follows the control
  voltage linearly above a hard turn-on and rolls into the grounded-base
  stage's own 60 mV-per-decade knee below it. That is read off Roland's module
  drawing rather than fitted; the remaining open item is where a measured
  BA662 puts the turn-on.
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
- **Self-oscillation is trimmed where the service manual trims it.** The
  ADJUSTMENT table sets every card, at BANK 3 with C4 held, to a 4.8 Vp-p sine
  at 248 Hz, and the two are coupled — a bigger limit cycle compresses the
  transconductor and pulls the pitch flat, so loop gain and the frequency
  correction are solved together against both. The engine renders 4.83 Vp-p at
  248.0 Hz, and C6 oscillates at exactly four times C4, which is what makes
  full keyboard tracking 1.00 rather than approximately so.
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
  there is a control to defeat it that the hardware does not have. The final
  mixer gains dry by `100/39` and wet by `100/47`, putting wet at `39/47` of
  dry (−1.62 dB). Those absolute gains occur after the BBDs, so they do not
  falsely overdrive the delay-line model.
- **The chorus modulator is drawn from the JUNO-106's own schematic.** IC1 is an
  integrator closed around a Schmitt comparator, so the sweep is a straight
  symmetric triangle, and IC2a inverts it for the second line — the antiphase
  pair is one waveform and its negative. The I/II switch shorts a 2.2 MΩ leg of
  the integrator's T-network, which fixes the mode-rate ratio at 1.6234799
  exactly. What the schematic cannot give is the absolute rate, because it does
  not print the integrator capacitor: the 1.66–5.35 ms sweep and the rate
  *scale* are still an explicitly reported JUNO-60 fallback, re-split by this
  instrument's own ratio to 0.5222 Hz and 0.8478 Hz. Both round to the owner's
  manual's published "about 0.5" and "about 0.8".
- **The final outputs are AC-coupled before VOLUME.** The two service-schematic
  paths use C17/C20 10 µF and R54/R57 1.5 kΩ into the 10 kΩ pot tracks. The
  unloaded full-track reference is 1.383956 Hz and `10/11.5` settled gain; the
  engine solves the actual corner/gain against shaft position and fixed wiper
  loads while retaining independent capacitor states. This removes the large
  DC offset an asymmetric manual-PWM patch would otherwise send to a host.
- **The common signal path now includes all three established coupling
  boundaries.** C14/R39 couples the six-voice sum into the selected HPF leg;
  C12/R36 couples that result into the common uPC1252H2 VCA; C17/C20 couple the
  complete stereo IC6 outputs into VR1. C12/R36 is 0.482288 Hz. C14 is
  0.820915 Hz in Boost/Flat (`33k || 47k`) and 0.482288 Hz at the sub-hertz
  asymptote of Cut I/II, where their series cut capacitors are open.
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
[research contract](Docs/circuit-modelling-research.md).

| Stage | Same as the hardware / evidence-fixed | Approximated, guessed/voiced or product-only |
| --- | --- | --- |
| Patch memory and selection | All 128 locations are in hardware order A11…A88, then B11…B88. Each tone is the exact 16 continuous bytes plus two packed switch bytes used by the hardware, decoded by the SysEx path. Program Change 0…127 maps directly to those slots. | The hardware stores no names; displayed names are archival metadata. A host preset also restores plug-in/performance controls, while hardware Program Change and SysEx correctly restore tone memory only. |
| Key assigner and POLY modes | Six-card allocation, POLY 1/POLY 2, note dropping instead of stealing, held-key rescans and Solo Unison behavior are ROM-resolved for the stated A-5 image. The physical keybed is represented as 61 keys. | Velocity, more than six voices and host notes beyond the drawn keybed are extensions. The mouse Shift-click gesture is a UI equivalent for pressing both momentary POLY contacts. |
| Shared digital control generator | Envelope recurrence, sustain mapping, DAC truncation, LFO/delay arithmetic and portamento are ROM-resolved for the stated B-2 image. The 23 converter destinations, their ownership and the 4.2 ms pass are anchored. VCF and voice-VCA hold constants are component-derived, as is the common VCA LEVEL path's 9.08249 ms post-S/H C7 pole. | Writes retain the exact logical order but use normalized sub-pass spacing; exact timestamps and jitter remain open. The remaining sample-and-hold slews are voiced, and initial idle host-snapshot priming is product policy. |
| DCO, ramp, pulse, sub and mixer | The 8 MHz master reference, integer timer division, range clocks, pitch quantization, constant-current ramp, PWM comparator and divide-by-two sub topology are anchored/derived. A changed-pitch write occurs at that card’s converter slot. The moving-threshold solver prevents a digital-only missed PWM edge and full-cycle blip. | BLEP/BLAMP repairs are transparent digital antialiasing. Exact restart electrical state, loaded saw/pulse/sub/noise levels, filter-drive budget and live waveform-switch transients remain approximated or open. Pulse currently uses a provisional instantaneous audio gate; no invented anti-click envelope is presented as hardware behavior. |
| Per-voice VCF | Four IR3109/BA662 transconductor stages, the 68 kΩ/560 Ω attenuation, 240 pF stages, per-card cutoff trims and service calibration anchors are hardware-fixed. Cutoff modulation is summed in converter counts before the exponential law. The upper knee is the transconductor's own control-current saturation near 64 kHz, and the converter's R-2R carry error rides on the code it produces. | A topology-preserving trapezoidal/Newton solve is the numerical realization. Resonance byte-to-loop gain, input compensation, feedback saturation and frequency trim are voiced pending measurements. The saturation exponent and the carry sizes are fitted to a third-party measured card, not to a Roland document. |
| Per-voice VCA | One BA662 VCA per card, after its filter, with ENV/GATE ownership and the 6 Vpp service endpoint, is anchored. Roland's own module-board drawing puts a grounded-base volts-to-amps stage ahead of it, so gain follows control current: linear above a hard turn-on with the transistor's 60 mV-per-decade knee below it. | The turn-on point and knee width are derived from that drawing rather than measured; a BA662 gain sweep would move the turn-on, not the law. The exact-zero deadband and the voice-retirement silence threshold are product policy; velocity is an optional extension. |
| Voice sum, coupling, HPF and common VCA LEVEL | Six card outputs sum through 33 kΩ into 3.3 kΩ feedback (0.1 each). C14 precedes the shared four-position HPF; C12 then feeds the one common uPC1252H2 controlled by stored VCA LEVEL. Service Notes pp. 8 and 15, the ROM-resolved `d=b<<5` code and NEC's −5.9 mV/dB typical constant derive the nominal common-VCA law and C7 settling. | The complete coupled switched-HPF network and its switching memory are approximated. The ideal 12-bit R-2R transfer assumes division by 4096; R32 is the least-legible value in the scan, and real resistor/capacitor tolerance, rail error and uPC1252 variation still need an installed-unit sweep. |
| BBD chorus and IC6 mix | Two uncompanded 256-stage MN3009 lines, anti-phase modulation, continuously running bypass, support-filter parts, coupling capacitors and IC6 dry/wet resistor gains are anchored/derived. BBD write nonlinearity is fitted to its datasheet test points. | Absolute sweep and mode rates use a clearly labelled JUNO-60 fallback; hiss level/correlation, loaded support impedances and the wet-mute transient are voiced. Loaded IC6 clipping remains unknown. |
| VOLUME and output boundary | C17/C20, R54/R57, the nominal-linear 10KB×2 tracks and fixed internal wiper loading are component-derived, with independent left/right capacitor state. | Dual-gang tracking, selector/jack normaling, external loads and headphone transfer remain open. The fixed −18 dBFS RMS mapping and provisional physical reference are product policy, not an analogue circuit claim. |
| Antialiasing, HQ and safety | These preserve the modeled circuit’s behavior at host sample rates: bandlimited discontinuities, optional oversampling, Kaiser half-band decimation flat to 20 kHz at both common host rates, and state-preserving rate changes. | They have no hardware counterpart. The idle-only quality change and short safety fades are product mechanisms and are kept outside the claimed signal path. |

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
| Voice-VCA control offset | up to ±0.004 normalized control |
| Voice-VCA gain | up to ±3% |
| Sub level | up to ±3% |
| Main-noise level at each card | up to ±3% |
| Slow cutoff wander | d = 0.9992d + 0.004noise at 375 Hz; contributes 40d converter counts |
| VCF stage input offsets | up to ±1.5 mV per transconductor stage |
| VCF stage capacitor tolerance | up to ±2% per stage, staggering the four poles |
| Spatial thermal gradient — card temperature | up to +4 °C across the six cards, plus a +15 °C warm-up over ~900 s, raising the OTA thermal voltage and its headroom |
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
charge transfer, bandwidth, nonlinearity and hiss.

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
gated RMS of -20.75 dBFS, no preset below -60 dBFS maximum 400 ms RMS, and 31
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
The keyboard sends no velocity, and MIDI has no continuous controller
assignments for the synthesis panel. Host automation reaches every stored
parameter through the plug-in's own parameter list.

The incoming Program Change map follows the owner's manual exactly: 0..63
select A11..A88 and 64..127 select B11..B88, including every row and column in
both 64-tone groups. Incoming Program Changes are consumed rather than echoed.
YouKnow106 does not transmit performance data or Program Changes; that
reference-keyboard behavior is distinct from patch-selection receive. The
compact editor deliberately exposes no patch-dump transmit control.

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

There are six suites:

- **`YouKnow106.Circuit`** compares the model against something independent for
  every block: the four transconductor stages against a fourth-order
  Runge-Kutta solve of the same ODE at 16x *and* against the closed-form
  `1/(4 − k)`; the note timer against integer division; the cutoff law against
  the instrument's two service calibration anchors; and the delay line against
  its part's datasheet delay range.
- **`YouKnow106.Engine`** checks what the instrument does when it is played:
  that RANGE transposes by octaves, that the sub is an octave down, that the
  alias floor stays below −55 dB, that the ramp's harmonics follow `1/n`, that a
  seventh held key is dropped rather than stealing a voice, that unison does not
  acquire artificial detune, that assign-mode changes and Solo Unison key-ups
  rebuild from the still-held physical keys, that output level is independent
  of host rate and of oversampling, that final PWM DC is removed, that an HQ
  transition cannot expose a chorus-state reset, that the engine is
  deterministic and exactly silent when idle, and that hostile automation
  cannot produce a non-finite sample. It also renders every historical factory
  tone without per-preset normalization and rejects non-finite or runaway
  output.
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

Focused DSP changes use `YouKnow106RenderRealismComparison`. Unlike the older
16-bit comparison set, it archives raw float32 before/after/difference signals,
uses one shared listening gain, reports peak/peak and RMS/RMS nulls, and records
the source fingerprint, patch, seed and exact MIDI/control sample schedule.
Committed fixtures currently cover the
[retrigger/release-tail path](Docs/audio/realism-comparisons/retrigger-release-tail/)
and the
[common VCA LEVEL law and settling](Docs/audio/realism-comparisons/common-vca-level/).
The latter records every automated byte and reports a −8.70 dBc RMS difference
between the superseded cubic/borrowed-slew model and the nominal circuit solve.

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

# Ghostar circuit-modelling research and implementation contract

Ghostar is a white-box circuit model of a 1983 monophonic dual-filter analog
synthesizer — the Crumar Spirit, designed by Jim Scott, Tom Rhea and Bob Moog
for Crumar s.p.a. — with named reference material, not a black-box claim that
one plug-in is indistinguishable from that instrument. This document separates
what the engine implements from documentation from what remains a voiced Ghostar
decision, and records where the sources disagree.

Ghostar is an independent original implementation. It is not affiliated with,
endorsed by, or licensed by Crumar or its successors; it contains no firmware,
no ROM data, no samples, and no captured audio. Its architecture reproduces
the reference instrument's *functional* signal flow — which controls exist,
what each is calibrated to do, and how the blocks connect — because that flow
is what the circuit is. Its name and livery are Ghostar's own.

## Primary sources

- **OM** — Crumar Spirit Owner's Manual, 30-page factory scan
  (https://manuals.fdiskc.com/tree/Crumar/Crumar%20Spirit%20Owners%20Manual.pdf,
  mirror https://archive.org/details/manualzilla-id-6890440). Printed pages
  17–25 are missing from every circulating scan (the generic tutorial
  chapter); every control-reference page survives.
- **SM** — Crumar Spirit Service Manual: schematics DWG 1–3, PCB layouts,
  electronic component list, errata
  (http://www.midimanuals.com/manuals/crumar/spirit/service_manual/spiritservicemanual.pdf,
  600 dpi mirror https://archive.org/details/sm_Crumar_Spirit_Service_Manual).
  No calibration text exists — schematics, parts and errata only.
- **RM** — 2023 reissue User Manual (verbatim re-typeset of OM plus a
  trimmers-and-adjustment chapter and MIDI addendum), linked from
  https://www.crumarspirit.com/.
- **CEM3340/3345 datasheet** (VCO, CES © 1980;
  https://sandsoftwaresound.net/wp-content/uploads/2021/03/CES_CEM3340_VCO.pdf)
  and the complete four-page
  **CEM3350 preliminary datasheet** (dual state-variable VCF), Curtis
  Electromusic. The latter's standard-response and multiple-resonator
  application figures resolve how the VLP/VBP state pins are used; the old
  two-page-only dead end is withdrawn
  (https://sandsoftwaresound.net/wp-content/uploads/2021/03/CES_CEM3350_VCF_Prelim.pdf).
  Period corroboration: the E&MM CEM3350 design article (Feb 1982) and the
  CES *Synthesource* newsletter (Winter 1981).
- **CEM3360 production datasheet** (dual linear VCA, CES © 1984;
  https://www.synfo.nl/datasheets/CEM3360.pdf) — current-output and control
  architecture plus the typical 52 %/V linear scale (48–56 %/V), 1.93 V
  maximum-gain control voltage (1.79–2.08 V), −1.6 µA linear-pin bias
  (−0.5…−4.0 µA) and 80 dB typical / 70 dB minimum zero-control attenuation.
  The one-point bias is not a complete `i5(Vc)` law. It does not specify
  out-of-range control-pin behavior, including whether or where it clamps, or
  the sign of a particular device's feedthrough residual. The earlier
  preliminary sheet remains useful only as period corroboration.
- **ITT Transistors Manual 1972/73**, BC173 pp.33–36
  (https://www.bitsavers.org/components/itt/_dataBooks/1972_ITT_Transistors.pdf)
  — one manufacturer's gain-group and VBE ranges for the Shaper VCA's
  auxiliary transistor chain, plus its 5 V emitter-base rating and
  `V(BR)EBO>5 V` at 1 µA. The Spirit BOM selects neither manufacturer nor B/C
  gain suffix.
- **DAC0800/DAC0802 datasheet** (National Semiconductor, later TI), whose
  positive-reference current law and complementary current-output pinout close
  P1016's signed keyboard-CV cancellation
  (https://www.ti.com/lit/ds/symlink/dac0800.pdf).
- **AS3360 datasheet** (2017 v.1, ALFA; CEM3360 replacement), whose stated
  2.7–3.3 mV/dB exponential scale is used only to bound a modern-compatible
  interpretation of MOD RATE, never as proof of a vintage CEM3360 constant
  (https://www.thonk.co.uk/wp-content/uploads/2018/01/AS3360.pdf).
- **Fairchild 1978 Diode Data Book**, printed pp.3-12 (BA128·BA130) and
  4-6 (curve set D4) — the BA130 forward characteristic used by the
  modelled local high-Q and OVERDRIVE branches (OQ-10/OQ-12).
- **Tom Rhea memo, 25 May 1981**, to Bob Moog and Jim Scott, preserving the
  Spirit project's earlier dual-filter proposal and its SERIES/PARALLEL plus
  CLEAN/DISTORT concept. It is design-history evidence, not a truth table for
  the production RS7 switch
  (https://www.drtomrhea.com/_files/ugd/a27ff8_9336666b18834d1790849ade46fc221c.pdf).
- **Signetics 555/556 1973 databook** and **AN170**, for the 556A timer
  behaviour the envelopes are built on.
- **National Semiconductor 1977 MOS/LSI databook**, MM5837 pp.3-14--3-15:
  the self-clocked digital noise source's levels, 1.1--2.4 s cycle and
  24--56 kHz half-power point
  (https://bitsavers.trailing-edge.com/components/national/_dataBooks/1977_National_MOS_LSI_databook.pdf).
- **US 3,943,456** (Luce/Moog Music, 1976) — the variable-rate-integrator
  signal generator that is the Shaper Y core; attribution to the Spirit by
  J. D. Tillman (https://till.com/articles/moog/patents.html), corroborated
  by the P1015 schematic's OTA-integrator topology.
- Panel silkscreen verified against photos of serials 00045 and 00046
  (matrixsynth.com listings). Where OM prose and the silkscreen disagree, the
  silkscreen wins — it agrees with the schematic net names and every
  independent witness in both such cases (see Discrepancies).
- Secondary color: Sound On Sound Retrozone (Gordon Reid, 2001), Amazona Blue
  Box, GreatSynthesizers reissue notes, Cherry Audio's licensed 2025
  behavioural recreation and its researched history chapter.

## Architecture (anchored, OM p.26)

Two VCOs, a triangle-cross ring modulator, and one noise source feed **two
parallel audio paths**:

- **Filter/ADSR path**: mixer (A, B, NOISE) → **Lower Filter L** →
  **Upper Filter U** in series — the OVERDRIVE soft-clipper sits *between*
  them — → C30 coupling → VCA driven by the LOUDNESS ENVELOPE (or held
  open by VCA BYPASS).
- **Shaper Y path**: mixer (A, B, RING, NOISE) → VCA whose gain is the
  SHAPER Y output → passive **BRIGHTNESS** shelf.

Both CEM3360 current outputs see the full 20 kΩ tracks of dual-gang P4 as
fixed loads, independent of the wipers. The wipers reach P1017 through equal
R49/R50=10 kΩ resistors. Its SHAPED jack's normal contact joins those wipers;
inserting a plug opens the link. With C18 open/DC, the equal sources give the
familiar `main=m·(Filter+Shaper)/2`. At audio frequencies C18/P3 changes the
Shaper source impedance, so the normalled wipers cross-load and BRIGHTNESS
also colours the Filter contribution. Ghostar solves that coupled network;
SPLIT opens it and exposes the isolated wipers. P1017 draws no series output
capacitors: both rear outputs are DC-coupled. The full law is in
[OQ-16](open-questions.md#oq-16--audio-path-coupling--closed).

C30=470 nF is the Filter path's real coupling capacitor, *before* its
Loudness VCA. It sees `R_L=R132 || R133=24k || 100k=19.3548 kΩ`, hence
`H(s)=s·R_L·C30/(1+s·R_L·C30)`, `τ=9.09677 ms` and `f_c=17.4958 Hz`;
its state continues charging while the VCA is closed. The Shaper path has no
corresponding high-pass.

The audio sliders are unbuffered 100 kΩ linear pots. At travel `t`, each
wiper contributes `100k*t*(1-t)` of Thevenin resistance. This gives the
derived loaded law at the Shaper's virtual-earth mixer: 47 kΩ arms for
A/B/Ring and an errata-corrected 6.8 kΩ for Noise. The Filter is not that
topology: every wiper feeds 220 kΩ to the Lower CEM's VLP state and a separate
68 pF to its VBP state. Ghostar solves all three Thevenin wipers, all three
68 pF histories, VLP/VBP and C33 together; even a zeroed slider therefore
keeps its physical loading. Exact pot-end charge projection removes the
trapezoidal alternating mode without inventing resistance. The selected
oscillators and these CEM states share a 5 V/unit normalization derived from
P1014's affine offset;
MM5837 level, Shaper-cell top gain and the still-untraced RS7 dry output remain
calibrations. The isolated, ideal-current-output
resistor ratios themselves are closed: Shaper A/B/Ring `20k/47k`, Shaper Noise
`20k/6.8k`, and Filter above C30's corner `20k/24k`. They are not substitutes
for the still-open MM5837 level, original-CEM3360 gain and output loading.

The Shaper audio VCA's *control* is a separate open seam, not a direct wire
from normalized Y. SHAPE voltage `S` reaches IC5/CEM3360 pin 5 through
R38=10 kΩ; R40=5.6 kΩ returns it to ground and R41=1 MΩ to −12 V. The other
drive is the complete `J5/1→R30→TR1→R31→TR2→R39` follower chain, not one
always-biased transistor. Outside FREE **with SHAPE X WITH Y open**, R81's
negative pull keeps both devices out of forward conduction. If reverse
base-emitter currents were negligible, the node would reduce to
`V_C=0.3576903424·S−0.0371997956 V` at the CEM's typical pin bias. Near the
1.93 V top-control point, however, the chain sees about 10.005 V end to end:
the sum of two BC173 5 V emitter-base ratings. A device-dependent reverse-
avalanche knee is therefore plausible, not source-closed. Closing SHAPE X
WITH Y also loads the post-R31/TR2-base node through R33; FREE instead connects
J5/1 to an undocumented loaded IC9/R64/C11/D22 waveform. Ghostar keeps its
clearly labelled behavioral `max(0,Y)` approximation until simultaneous
source, switch-node and `V_C` captures close OQ-26; no avalanche curve or rail
substitution is invented. The ITT sheet supplies no leakage curve that would
close even a lower-voltage interval against the CEM's microamp-scale pin bias.

Modulation: **MOD X** (LFO / S&H / red noise / Osc B, plus the arpeggiator
clock) and **SHAPER Y** (a variable-symmetry envelope/LFO), each through its
own performance wheel (an attenuator toward zero volts), routed by the two
**WHEEL DESTINATIONS** switches. Gates for the two ADSRs come from GATE
SELECT (keyboard, LFO square, Shaper/external) with SINGLE/MULTIPLE trigger.

## Control calibration (the modelled laws)

Every panel control is stored as 0..1 travel and mapped through the law
below. Provenance: **anchored** (stated by OM/SM/datasheet), **derived**
(computed from anchored values by a stated equation), or **voiced** (chosen
inside a range the sources bound but do not fix).

### Master and oscillators

| Control | Law | Provenance |
|---|---|---|
| TUNE | ±3 semitones (a minor third) | anchored, OM p.28 |
| OCTAVE | 32' 16' 8' 4'; at 8' the keyboard's second C sounds middle C | anchored, OM p.28 |
| MASTER VOLUME | dual-gang 20 kΩ **linear** attenuator after the two paths; each full track remains the VCA's load while its wiper attenuates one path. Normalled, the main output is the average of the two *cross-loaded* wiper voltages; at DC this reduces to the isolated sources' passive half-sum, while C18/P3 makes the audio-frequency law Master- and BRIGHTNESS-dependent | anchored, SM DWG 2 P1013 + SM DWG 1 P1017; coupled mix **derived**, OQ-16 |
| Osc A WAVEFORM | triangle; rectangular 50/30/15/**6** %; sawtooth | anchored: panel line-art at 400 dpi + SN 00046 photo. SOS's "5 %" and Cherry Audio's "8 %" are secondary errors |
| Osc B WAVEFORM | triangle; rectangular 40/20/10/3 %; sawtooth | anchored, OM p.28 + panel |
| Selected-wave level | P1014 deliberately equalises the CEM3340 outputs before RS5/RS6: triangle is direct at `4 V`; saw's `8 V` source and typical `100 Ω` output impedance drive `10k` series / `10k` shunt, giving `800/201=3.98010 V`; the open-emitter pulse drives `10k` series / `6.8k` shunt and the datasheet's loaded branch gives `3978/905=4.39558 V`. Each IC10 stage then applies `V_out=(1+10/24+10/91)V_tap−5 V`. Endpoints are triangle `−5..+1.10623 V`, saw `−5..+1.07585 V`, pulse `−5..+1.71010 V`; p-p ratios are `1/.995025/1.098895`. Their shared negative offset and smaller level differences feed both audio mixers and the selected Osc-B modulation source. One engine unit remains P1014's 5 V affine offset; Ring retains the raw pre-switch triangles | chip levels/impedances anchored, CEM3340 datasheet; dividers, loaded pulse and conditioner **derived**, SM DWG 2A P1014 |
| Pitch-control bandwidth | Each CEM3340 pin-14 multiplier current output returns through `R_s=1.82 kΩ` to ground, bypassed by `C=1 nF` in parallel (A R82/C72; B R118/C77). Curtis states that this bypass limits bandwidth and gives `f_LP=1/(2πR_sC)`; normalizing the parallel-RC current-to-voltage reduction gives `H(s)=1/(1+sR_sC)`, `tau=1.82 µs`, `f_c=87,447.7709 Hz`. Because pin 15 sums every pitch current before that multiplier, the pole filters the complete keyboard/tune/bend/interval/X/Y octave sum before exponential conversion, not only Osc-B audio modulation. Ghostar uses exact linear-input state evolution on the 4× grid, preserving unity DC gain, the exact 1.82 µs DC delay and a monotone step at every supported rate | components/topology and corner equation anchored, SM DWG 2A P1014 + CEM3340 datasheet; normalized transfer/discrete realization **derived/modelled**, OQ-25 |
| SYNC | one-directional A→B conventional hard sync. Both CEM pin-6 inputs are open: A's raw `8→0 V` saw fall, before its selector, passes SW2/C24=220 pF/BC308/R107=47 kΩ into B's triangle/threshold pins 10/9, following the CEM3340 datasheet's alternate conventional-sync circuit. Exactly one B phase reset therefore follows each A wrap, independent of A waveform selection and PWM; undocumented transistor/comparator propagation delay remains open | routing and behavior **derived/anchored**, OM pp.9/28 + SM DWG 2A P1014 + CEM3340 datasheet |
| Osc B OCTAVE/RANGE | −1, UNISON, +1, +2, BASS, WIDE (panel order). BASS/WIDE disconnect B from keyboard, TUNE, OCTAVE and bend (X/Y modulation still applies); INTERVAL becomes the drone pitch: 30–300 Hz in BASS, 2–10,000 Hz in WIDE | anchored, OM p.29 + panel photos |
| INTERVAL | ± a perfect fifth (±7 st) in the octave positions, exponential; centre = 0. OM: "slight deviations from center create the slight mistuning… that adds warmth" | anchored, OM p.29. SOS measured "±8 st" on one unit — endpoint resistors make unit spread real |
| Ring modulator | The fixed internal CEM3340 triangles are taken before the waveform switches, so WAVEFORM has no effect on RING. A passes C15=1 µF into R26=39 kΩ || R27=100 kΩ (`f_c=5.67245 Hz`) and drives both IC7's signal input and IC6's dry-A reference. B drives IC7's control through R23=220 kΩ against R24=1.8 MΩ to +12 V and R25=62 kΩ to ground. IC6 feedback is R28=68 kΩ plus internal P2=25 kΩ: the vintage carrier-null/product-level trim. At nominal CEM3340 swing and null the engine-unit transfer is `ring=−(15/13)·HP(A_triangle)·B_triangle`, with no deterministic `0.03·(A+B)` term. A particular unit's P2 setting, tolerances and CEM3360 feedthrough remain open | topology/components anchored, OM pp.6/26 + SM DWG 2 P1013; nominal transfer **derived**, OQ-06 |
| Noise | one self-clocked MM5837: 17-stage maximal PRBS (taps 17/14, 131071 bits) through C17/R4/R5 and the resolved R6/C8, R7/C9, C10 passive network. IC4A's 27k / `1M || (100k + 15n)` feedback shelf makes the audio output: poles 0.595, 9.646, 31.039, 179.275, 8157.418 Hz; zeros DC, 40.191, 84.015, 530.516 Hz. The separate R6/C8 junction feeds IC4B at `1+100k/2k2`; its RED NOISE output has poles 0.595, 31.039, 179.275, 8157.418 Hz and zeros DC, 530.516 Hz. The engine's 75 kHz nominal clock is the datasheet cycle-time midpoint; absolute source level and RED NOISE bus scale remain voiced pending same-unit captures | MM5837 source anchored, National databook; topology/components anchored, SM DWG 2; transfers **derived**; clock/levels **voiced** |

VCO model: every phase or same-selector duty discontinuity is bandlimited as
a sub-sample event — BLEP for the value jumps of saw, pulse and a moving duty
edge, BLAMP for the triangle's corners, and both for the hard-sync reset.
The CEM3340's documented `0..100%` PWM span is preserved: wheel modulation
can reach constant-low/high endpoint plateaus, while the panel's `3%` is only
Osc B's narrowest selector detent. A
live selector change emits the already-deferred sample in its old waveform's
physical scale, suppresses any fictitious cross-selector PWM event, and
applies newly discovered phase-event residuals in the new waveform's scale.
The combined result is conservatively bounded by the proven ±12 V supplies;
the original loaded 1458's smaller swing remains open. Ghostar applies no
oscillator drift: the CEM3340's on-chip compensation puts the chip's own
contribution at ±0.09 to ±0.35 cents/°C and the regulated supply path
below ≈0.35 cents even for a ±10 % mains excursion, while no measured
record of the *environmental* excursion inside any synthesizer enclosure
exists to derive a wander process from (best-in-class plan, Step 2). Both
oscillators share one master CV bus (tune, octave, bend, glide are
common-mode); B's interval is a constant CV offset — a constant musical
interval across the keyboard, not constant Hz (SM DWG 2 topology).
Before either octave sum reaches `exp2`, its R82/C72 or R118/C77 state carries
the Spirit's derived nominal 1.82 µs multiplier-output memory. Unsynchronised A
advances directly from fresh B. Cyclic B self-FM and hard-synchronised A
predict their phase step from causal prior B, then commit the real capacitor
once against fresh B at the endpoint; newly known base CV is included in the
prediction, so no extra grid delay is composed with the RC pole. PWM and
filter modulation retain the current causal schedule because their active-path
group delays are not specified by the schematic or chip data (OQ-25).

### Filters (the signature)

Both hardware filters are CEM3350 dual state-variable sections — not a
transistor ladder, despite the Moog pedigree. Ghostar models the resolved
Upper sections as TPT state-variable filters. Its controlled half includes
the complete external C37 high-Q loop; the fixed half stays linear because it
has no such branch. Lower uses the production topology: three 100 kΩ
Thevenin wipers each reach VLP through 220 kΩ and VBP through 68 pF, reduced
with the two 22 nF CEM states and the traced TL082/BA130/C33 loop to one 2×2
implicit solve plus one monotone diode-current scalar. The selected-wave
conditioner supplies the derived 5 V/unit normalization. The remaining Lower seam is
RS7's unlabelled OUT/BANDPASS/HIGHPASS rotor mapping and corresponding C34
pre-charge; TL082 dynamics and internal CEM3350 headroom also require hardware
measurement (OQ-10/OQ-12/OQ-20).

| Control | Law | Provenance |
|---|---|---|
| MASTER (cutoff) | sets both filters' cutoff, always; exponential through the audio range. The pot's ±10.5 V through the 12k1 ladder is ±11.4 octaves of authority over a chip window ≈10 octaves wide, so the hardware sweeps closed to wide open; where that window *sits* is set by a 100 kΩ trimmer no document records, so the implemented 20 Hz–16 kHz is a voiced placement inside a ±2.3-octave authority | anchored routing OM p.31; span **derived** (CEM3350 datasheet + SM DWG 2); placement **voiced** |
| LOWER ONLY | Lower cutoff relative to Upper; cutoffs coincide at 8 (circled on the panel); below 8 Lower sits below Upper | anchored, OM p.31 |
| RESONANCE switch | LOW fixes Upper Q = 0.5 (`k=2` exactly); VARIABLE slaves Upper Q to the pot and alone uses the declared Q-ceiling enhancement extension | anchored, OM p.30 + SM DWG 2 |
| RESONANCE pot | Lower Q always; Upper Q in VARIABLE; both reach self-oscillation at maximum. The law is derived: the chip's Q control is exponential at −65 mV/decade, the pot is 100 kΩ linear (top ground, bottom −12 V) into 18k2, each Q pin has a 221 Ω shunt against a pull-up to +12 V (91 kΩ Upper, 75 kΩ Lower — so the two filters differ), and the pot's own output impedance flattens mid-travel. LOW disconnects the pot, resting the Upper pin at +29.1 mV where OM says Q = 0.5, which calibrates the whole law. Upper Q: 0.51 / 1.48 / 3.33 / 10.9 / 82 at travel 0 / .5 / .75 / .9 / 1 | **derived**, CEM3350 datasheet + SM DWG 2, anchored at LOW by OM p.30 |
| SLOPE | Upper is 12 dB (controlled section) or 24 dB (controlled plus fixed-Q=0.5 section). Both halves tie VIF+VIV, so drive is `u·(1+1/Qcommanded)`. SW4 moves C40=1 nF between their 22 nF LP nodes with retained charge, leaves a 1 MΩ cross-coupling only in 12 dB, and changes output-buffer gain by exact ratio `101/201`; Ghostar solves all four states together and charge-projects the newly selected node | **derived/modelled**, OM pp.30/32 + SM DWG 2 + CEM3350 datasheet, OQ-09 |
| Lower mode | OUT / OVERDRIVE / BANDPASS / HIGHPASS. BANDPASS is **parametric boost** — "a peak … without attenuation of frequencies far from this cutoff" — not a true band-pass. OVERDRIVE is the manual's distorted parametric register *between* the filters, but its production throw is a distinct Lower-VLP-fed IC12A/BA130 path rather than `dry + BP` followed by a generic clipper. HIGHPASS gives the documented "double-peak, highpass-lowpass" response | behaviour anchored, OM pp.30–32; OVERDRIVE topology SM DWG 2 |
| KB AMOUNT | keyboard tracking of Upper always, Lower when DYNAMIC; 0 to 108 % at full — the 1 V/octave bus through the 12k1 ladder delivers 21.2 mV/V against the chip's −19.6 mV/octave, reproducing the manual's "slightly over 100 %" from the resistors. P1016 uses six DAC0800 key bits with B7/B8 grounded, so each semitone is four DAC counts. Its pin-4 sink through R31=4.99 kΩ opposes +12A/R39=26.6 kΩ at IC16A; cancellation is `q=64·4.99/26.6=12.006015` semitones above the lowest C, or MIDI 60.006015. Both currents share +12A, so the nominal pivot is rail-independent | anchored OM p.32; amount **derived** (CEM3350 datasheet + SM DWG 2); pivot **derived** (SM DWG 1 P1016 + DAC0800 datasheet), OQ-13 |
| TRACKING | FORMANT disconnects Lower from keyboard CV, X and Y modulation, the filter envelope and the pedal — freezing its peak as a fixed formant (the starred brass/woodwind configuration); MASTER and LOWER ONLY still act | anchored, OM pp.31/33 |
| FILTER ENVELOPE AMOUNT | bipolar, centre zero; unattenuated span ±2.5 octaves straddling the cutoff; INVERT mirrors it. Permanently wired to Upper; to Lower only in DYNAMIC | anchored, OM pp.27/33 |
| OVERDRIVE clipper | a separate IC12A/BA130/RS7 network between Lower and Upper, so the Upper re-filters its distortion products. A3 closes the nonlinear return, B7 selects its output and C10 would feed clean VLP through 33 kΩ, yielding the conditional `Vth=(47·VLP+33·o)/80`, `Rth=47k||33k`. That A3+B7+C10 combination is the engine's explicit functional hypothesis, not a traced detent: a standard same-index 3P4T reading pairs panel OVERDRIVE with C11, which grounds VLP. Installed-switch continuity must resolve the contradiction and all named C34 histories | placement/components anchored OM p.32 + SM; nonlinear scalar and conditional C10 network **derived/modelled**; switch assignment **open**, OQ-10/OQ-20 |
| BRIGHTNESS | after the Shaper VCA, C18=27 nF in series with P3=100 kΩ LOG shunts the VCA's full 20 kΩ Master track. In SPLIT, `H(s)=(1+s·C18·R)/(1+s·C18·(R+20k))`: panel 0 is a 294.731 Hz low-pass; panel 10 has pole 49.1219 Hz, zero 58.9463 Hz and high shelf 5/6 (−1.5836 dB). Normalled P1017 cross-loads both Master gangs: at full Master dark's main pole becomes 442.097 Hz, while bright leaves 15/17 of the Shaper and 16/17 of the Filter contribution at high frequency. It is neither a variable one-pole nor fully open; only P3's exact LOG taper remains voiced | topology/endpoints anchored, OM p.29 + SM DWG 2 P1013/P1017; coupled transfer **derived**, OQ-16/OQ-22 |

At internal step `Δ`, let `G=2C/Δ` and `q` be the capacitor's history voltage. For C30,
`i=G·(x−q)/(1+G·R_L)`, `y=R_L·i`, `v_C=x−y`, then
`q_new=2·v_C−q`.

C18 is solved jointly with P4 and P1017. Let `R_p=20k`, `R_o=10k`,
`R_l=mR_p`, `R_u=(1−m)R_p`, `h=0` in SPLIT or `1/(2R_o)` normalled,
`g_b=G/(1+G·R_b)`, and Norton currents `J_f=X_f/R_p`, `J_s=X_s/R_p`.
Eliminating the Shaper top gives
`g_e=g_b/(1+g_bR_u)` and `j_e=(J_s+g_bq)/(1+g_bR_u)`. With
`B=hR_l`, solve the endpoint-safe 2×2 system

`[1+B, −B; −B, 1+B+g_eR_l]·[w_f,w_s] = [J_fR_l,j_eR_l]`.

Recover `t_s=[w_s+R_u(J_s+g_bq)]/(1+g_bR_u)`, then
`i_b=g_b(t_s−q)`, `v_C=t_s−R_bi_b`, and `q_new=2v_C−q`. The normalled output
is `(w_f+w_s)/2`; SPLIT exposes `w_f,w_s`. This remains well-conditioned at
Master 0 and 1, advances C18 at zero output, and runs before the two path
decimators so a SPLIT change preserves each wiper's own history. C30's
independent bilinear oracle uses `u=G·R_L`, `b0=u/(1+u)`, `b1=−b0`, and
`a1=(1−u)/(1+u)`; the circuit suite checks C18 by its KCL/KVL instead of a
standalone one-pole assumption.

### Envelopes, gating, keyboard

| Item | Law | Provenance |
|---|---|---|
| Two ADSRs | Each envelope has its own 4.7 µF cap and 2 MΩ log A/D/R sliders. R23/R24=100 Ω lie between the common segment/556-threshold node and the actual cap, so all currents cross them: the nominal range is 5.17 ms to 9.40047 s and fast Attack trips with the cap at `1−100/R_attack·(1.3−1)=0.97` of the 7.5 V threshold. The two 100 kΩ sustain tracks share one D15-biased bottom rail (`f=1/15` nominal), and D11/D14 give release its nonlinear knee. Accepted MULTIPLE-key and X/Y rising edges force GS low for a nominal ~5 ms, release both caps, then attack from the retained post-notch levels; X/Y edges remain effective under another held gate. SINGLE keyboard has no KT pulse branch | topology/components and trigger state machine **derived/modelled**, SM DWG 3 + Signetics 1973 databook / AN170; pulse width, fast residual, aim and diode calibration **voiced**, OQ-04 |
| LOUDNESS VCA | envelope voltage `LC=7.5·e` reaches the CEM3360 linear-control pin through R135=10 kΩ, with R136=3.3 kΩ to ground and R137=240 kΩ to −12 V. KCL gives `V_C=1.84186·e−0.122791 V`: zero at `e=1/15`, a derived 0.5 V dead zone. Ghostar normalizes the nominal active region as `gain=clamp((15·e−1)/14,0,1)`; BYPASS holds it at one. The original CEM's exact top gain/saturation and per-device feedthrough remain unresolved | network anchored, SM DWG 2 P1013; zero and normalized law **derived**; device limits **open**, OQ-04 |
| GATE SELECT | KBD, X (the LFO square), Y/EXT (the Shaper's own gate) are OR'ed for hold, but X and Y have independent rising-edge branches so auto-repeat is not masked by another high source; at least one must be on for the envelopes to run | anchored/derived, OM p.33 + SM DWG 3 |
| TRIGGER | MULTIPLE accepts every selected new-key KT pulse and inserts the nominal reset/release notch. SINGLE has no KT branch: only a genuine selected-bus low→high attacks, so legato or a keyboard rise hidden under X/Y does not | anchored/derived, OM p.34 + SM DWG 3 |
| Keyboard | 37 keys C–C, digitally scanned; last-note priority with held-note memory — releasing the newest key falls back to the newest key still held, **without retriggering** | anchored, OM p.37 + SOS |
| GLIDE | conventional lag on the keyboard CV: C6=470 nF and P1=2 MΩ give the full-resistance endpoint `τ=0.94 s`; GLIDE MODE OFF / AUTO (only while >1 key held) / ON. The pot's travel/taper mapping remains voiced | anchored, OM p.37 + SM DWG 1; endpoint **derived**; taper **voiced** |
| PITCH BEND | the raw 100 kΩ pot spans ±12 V and reaches the CV summer through 680 kΩ. Against TUNE's ±6 V about centre through 1.8 MΩ and its anchored ±3 st, full *electrical* bend travel is `(12/680k)/(6/1.8M)·3=±15.88 st`. No source gives the spring-loaded wheel's mechanical fraction; Ghostar therefore retains its explicitly voiced ±8 st endpoint | electrical authority **derived**, SM DWG 1 + OM p.28; mechanical endpoint **voiced**, OQ-01 |

### MOD X, SHAPER Y, arpeggiator

| Item | Law | Provenance |
|---|---|---|
| MOD SOURCE | LFO triangle; LFO square; S+H RANDOM (red noise sampled); S+H Y (Shaper sampled — a regular, patterned staircase); RED NOISE (continuous slow random); OSC B — the **selected, buffered** Osc B waveform, not a hard-wired triangle (SM: the post-waveform-switch TP2 net feeds the mod board; OM's "triangle wave output" is imprecise) | anchored, OM p.34 + SM DWG 2A |
| LFO/S+H RATE | <1 Hz to ≈50 Hz; also the S&H clock and the arpeggiator clock; no effect on RED NOISE or OSC B. P2=100 kΩ LIN is loaded by R33=200 kΩ at the CEM3360 control node, giving the exact travel `w(x)=200x/(200+100x(1−x))` and `w(0.5)=4/9`. Ghostar applies that derived bend to its 0.3–50 Hz exponential law; only the original-chip/calibrated-unit slow endpoint remains voiced | range anchored, OM p.34; travel **derived**, SM MOD drawing; endpoint **open**, OQ-21 |
| MOD X TO: | OFF · OSC A+B · **OSC A** · OSC A RWM · FILT U+L · FILT U (panel; OM prose "(3) OSC B" is a typo — silkscreen, schematic net names, SOS and Cherry Audio all agree on OSC A) | anchored, panel + SM |
| SHAPER Y TO: | OFF · **OSC A+B** · **OSC B** · OSC B RWM · LFO RATE · FILT L (panel order; OM prose transposes 2 and 3). Y→LFO RATE only raises the rate: the wheel sets the fastest rate, the panel knob the slowest | anchored, panel + OM p.10 |
| Wheel loading and pitch/filter depths | X's CEM3360 current output drives its 100 kΩ wheel as a rheostat; Y reaches its 100 kΩ divider through P1013 R60=15 kΩ. The selected pitch load is `22k||100k` for one oscillator and `22k||100k||100k` for both; the filter load is 100 kΩ for one and 50 kΩ for both. For electrical fraction `t`, `X(t,L)=(100t||L)/(100||[22||100])`; with `z=100t||L`, `Y(t,L)=[z/(15+100(1−t)+z)]/0.504587156`. Retaining separately voiced one-octave X→A and Y→B source anchors, the derived full depths are X→A+B 0.867470 oct each, X→U 3.539889 oct, X→U+L 2.359926 oct each, Y→A+B 0.929638 oct each and Y→L 1.648923 oct; filter values include the `21.2/19.6` CEM3350/CEM3340 sensitivity ratio. Half electrical travel is already 0.867470 oct on X→A but only 0.335643 oct on Y→B. Audio-rate X uses the same loading | topology/components and destination ratios **derived**, SM DWG 2 P1013/P1014 + CEM3340 datasheet; absolute anchors and unmarked wheel tapers **voiced**, OQ-14 |
| SHAPE X WITH Y | Y envelopes the X wheel signal (an OTA VCA in the X path) — enveloped vibrato | anchored, OM p.36 + SM |
| SHAPER Y | US 3,943,456 variable-rate integrator. RATE = total period: FREE mode several cycles per minute to >20 Hz; envelope modes = total rise+fall time. RATE's 100 kΩ LIN P3 is loaded through R55=110 kΩ into the CEM3360 exponential-control node, with R56=20 kΩ to ground and R60=2.2 kΩ into the wider rate network; its exact travel and the FREE/envelope duration ratio therefore remain behavioral (OQ-07), not a generic log pot. SHAPE = rise/fall split of that period (0 fast-rise … 5 symmetric … 10 slow-rise/quick-fall), never changing total time. SM DWG 3 resolves the exact split: D19/D20 steer the two halves of 1 MΩ linear P4 through 27 kΩ R62, giving rise fraction `(27k + travel·1M)/1.054M`, or 2.5617/97.4383 at the endpoints. Modes: FREE (LFO, symmetric about 0 V); KBD HOLD (rise and hold while gated); RESET (single rise-fall from zero, always multiple-trigger regardless of the TRIGGER switch); RUN (the rising segment always completes; after it has, an accepted new gate starts another cycle). A selected KBD/MULTIPLE KT pulse remains effective under a held keyboard gate, while SINGLE still needs a genuine bus edge | topology/behavior anchored, OM pp.25/33–36 + SM DWG 3 + US 3,943,456; SHAPE split and KBD trigger state **derived/modelled**; RATE transfer **voiced/open**, OQ-05/OQ-07 |
| Y gate | the Shaper generates SG for GATE SELECT Y/EXT. SG is IC6's phase state, not a level detector: FREE is high for the complete rising leg and low for the complete falling leg; KBD HOLD is low at idle, high on the gated rise, low at the held top and through release, with a release re-gate reversing upward/high from the current level; RESET and RUN are low at idle, high on the rise after an accepted trigger and low from the apex through fall and end. RS3's four aligned throws and the 2N4856 clamp establish this table. Exactly which selected-source edge is accepted, especially under Y self-feedback, remains open | phase table **derived**, SM DWG 3 + OM p.36; trigger-source edge acceptance **open**, OQ-05 |
| Shaper audio VCA | SHAPE reaches CEM3360 pin 5 through R38=10 kΩ against R40=5.6 kΩ and R41=1 MΩ. The auxiliary chain is `J5/1→R30→TR1→R31→TR2→R39`; it is **not** one always-biased transistor. Outside FREE **with SHAPE X WITH Y open**, RS3-B8 and R81 prevent forward conduction; `Vc=0.3576903424·S−0.0371997956 V` is only the conditional KCL if reverse base-emitter current is negligible, and no cited leakage curve closes such a voltage interval. At the CEM's typical 1.93 V maximum-gain control, the chain sees ≈10.005 V end to end—already the sum of the ITT BC173s' two 5 V emitter-base ratings—so an installed-unit reverse-avalanche knee may bend the top. Closing SHAPE X WITH Y loads the post-R31/TR2-base node through R33. In FREE, J5/1 is the loaded IC9/R64/C11/D22 B-common waveform, not S or SG. Ghostar retains `max(0,Y)` as a behavioral approximation; the whole active transfer remains measurement-owned | topology and conditional switch-open KCL **derived**, SM DWG 2/3 + CEM3360/ITT sheets; active transfer **open**, OQ-26 |
| RWM | pulse-width modulation of the rectangular waveforms only; A's RWM belongs to the X bus, B's to the Y bus. The mirrored 200 kΩ/620 kΩ branches establish a nominal passive load, but the BC308 stage, PW trim and CEM3340 PWM input leave the signed duty transfer and its loaded travel open; Ghostar retains a voiced ±0.42 duty | routing anchored, OM pp.12/36 + SM DWG 2 P1014; transfer **voiced/open**, OQ-14 |
| ARPEGGIATOR | OFF · RIPPLE · ARPEGGIO · LEAP. All modes scan held keys chromatically bottom-to-top, wrapped; every newly held group restarts at its lowest key even if its no-key gap falls between clocks; one note per LFO clock. RIPPLE = the plain wrapped sequence. ARPEGGIO = the sequence at pitch, then +1 octave, then −1 octave, repeating. LEAP = per-note octave cycle 0/+1/−1 (pattern period lcm(N,3)). Arpeggiator modes clock-slave SHAPER Y RATE to the LFO except in FREE | anchored/modelled, OM pp.13/35 |

### Wheels

The MOD X and SHAPER Y wheels are attenuators; "attenuation always occurs
toward zero volts" (OM p.25) — a bipolar source keeps its symmetry, a
unipolar one scales toward silence. They are not electrically interchangeable:
X is current-driven and becomes very front-loaded, while Y is voltage-fed
behind 15 kΩ and rises much more slowly. Destination switching changes both
curve and maximum because it changes the load. The pot bodies are labelled
100 kΩ but not LIN, so Ghostar exposes those circuit quirks under the explicit
assumption that panel fraction equals resistance fraction pending taper or
hardware identification.

## Why it sounds the way it does (what reviewers hear, mechanically)

The reputation — "vocal", "nasal", "ghostly", "woody/reedy", "wicked",
"gnarly" (SOS, Amazona, Gearspace owners) — is carried by: the relative-offset
series dual filter with its parametric-boost lower peak; the FORMANT freeze;
the inter-filter overdrive being re-filtered by the upper lowpass; the
second, independently-enveloped audio path with ring mod; and second-order
modulation (Y shaping X, Y driving the LFO's rate, Y-patterned S&H). These
are exactly the blocks the contract above anchors.

## Documented discrepancies (resolved)

1. **MOD X TO: position 3** — OM prose says OSC B; panel, SM net names
   ("MOD A"/"MOD A+B", no "MOD B" exists), SOS and Cherry Audio say OSC A.
   Modelled: OSC A.
2. **SHAPER Y TO: positions 2/3** — OM prose transposes them; panel order
   (A+B then B) wins, corroborated by Cherry Audio.
3. **Osc A narrowest pulse** — 6 % (panel line-art and photo); SOS "5 %" and
   Cherry Audio "8 %" are secondary deviations.
4. **ARPEGGIO pass order** — at pitch, then +1, then −1 octave (OM, SOS);
   Vintage Synth Explorer's description is wrong.
5. **MOD SOURCE "OSC B"** — the schematic feeds the selected waveform, the
   manual says triangle. Modelled per the schematic.
6. **BRIGHTNESS order** — OM p.26 places the tone control before the Shaper
   VCA; SM DWG 2 P1013 places C18/P3 across that VCA's output load. Modelled
   after the VCA per the electrical schematic.

## What remains open

See [open-questions.md](open-questions.md). The headline items: the pitch
wheel's mechanical travel fraction (the raw pot has ±15.88 st of electrical
authority; Ghostar's ±8 st is voiced), absolute filter cutoff ranges in Hz,
exact hardware pulse duties behind the printed percentages, calibrated 556
attack/diode/reset-pulse parameters, Shaper trigger acceptance under self-Y,
RS7 rotor phase/C34 pre-charge outside OVERDRIVE, original
1458/CEM3350 headroom and dynamics, P3's exact LOG taper, and a particular
unit's residual ring carrier after its P2 trim. No calibration-grade filter, envelope, noise-level
or control-depth captures of a Spirit have been published; the first measured
unit would become this project's ground truth. Isolated observations such as
SOS's interval endpoint are useful corroboration, but not a same-unit
calibration set.

## Engine status

The complete architecture above — both paths, both filters with all four
Lower modes and the 12/24 dB Upper, both ADSRs with gate logic, MOD X with
all six sources, Shaper Y with all four modes, both wheel-destination
buses, the arpeggiator, sync, ring mod, glide and the keying rules — runs
on bandlimited oscillators (BLEP and BLAMP events, including at the sync
reset) and TPT filter sections at 4× internal oversampling, decimated in
two Kaiser halfband stages.

The controlled Upper loop and the complete Lower moving-input/C33 network use
trapezoidal capacitor companions and monotone implicit BA130-current solves.
The Lower circuit suite independently checks every wiper KCL, both CEM state
equations, C33 diode KVL, zero-slider loading, and charge-preserving endpoint
projection. C15's Ring
high-pass, C30's Filter-path high-pass and the per-internal-sample-varying
C18/P3 BRIGHTNESS branch and its P4/P1017 cross-loading are solved as one
physical trapezoidal MNA too. The one-step circuit suite checks their TPT
identities, charge conservation, diode KVL, exact component reductions, zero
ideal ring carrier, wiper/top-node KCL, DC half-sum limit and dark cross-load. A patch
converges to the same bounded filter level within 0.5 dB at the tested 8,
44.1 and 96 kHz host rates. This does not certify the three behavioral RS7
output seams or original-chip dynamics. Audio-rate modulation (MOD SOURCE =
OSC B) uses the selected post-IC10 waveform at the internal rate. Pitch passes
through each CEM3340's modelled R82/C72 or R118/C77 multiplier-output state;
filters receive current B, as do A/PWM with SYNC off, while PWM uses causal
prior B when SYNC closes the B→A→A-reset→B loop. PWM/filter active-path delay
remains measurement-owned (OQ-25).

The laws marked **derived** above were voiced when the engine was first
written; the primary sources that closed them are named against each.
Constants still marked voiced are first-pass choices inside the documented
bounds, each carrying an entry in [open-questions.md](open-questions.md)
with the evidence that would close it.

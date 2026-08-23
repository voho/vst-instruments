# Ghostar open questions

Status register for the circuit model. Open entries name the evidence gap,
what the engine currently does, and what output would close it; closed entries
retain the derivation and its evidence. Conventions follow the repository's
A–Z rules: a listening test may choose between defensible candidates; it may
not fit a number a measurement owns.

## OQ-01 — Pitch-bend wheel musical range

**Gap (electrical authority derived, mechanical endpoint open).** The bend
pot is 100 kΩ across ±12 V and reaches the CV summer through 680 kΩ. TUNE's
100 kΩ pot spans 0–12 V and reaches the same node through 1.8 MΩ; its ±6 V
about centre is anchored at ± a minor third. Their current ratio is therefore
`(12/680k)/(6/1.8M)=5.2941`, giving ±15.88 semitones at full *electrical* bend
travel. No source documents how much of that pot the spring-loaded wheel can
actually turn. The 2023 reissue's MIDI kit uses ±2 st, which is the
converter's choice, not evidence for the original wheel.
**Engine.** Full wheel = ±8 st, retained explicitly as a mechanical-travel
voicing rather than mislabelled as the circuit's electrical endpoint.
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
**Closed sub-part.** CES specifies the CEM3340 PWM input over the complete
0–100 % interval. The panel's 3 % is a selector detent, not a silicon limit.
Ghostar uses the printed unmodulated percentages, but X/Y/audio PWM can now
reach true constant-low and constant-high endpoint plateaus; the coincident
BLEP events cancel there instead of being clipped to an invented 3–97 %.
**Closes with.** Scope captures from a calibrated unit.

## OQ-04 — 556A envelope segment curvature

**Gap (topology closed; diode/unit calibration open).** SM DWG 3 (P-1015),
read losslessly, gives the whole circuit. Each 556 half has its own 4.7 uF
timing cap (C10/C11), OUTPUT-to-cap attack path, 2 MOhm log A/D/R sliders,
and R23/R24=100 Ohm between each common A/D/R + threshold node and its
actual cap/buffer node. Every attack, decay and release current crosses that
resistor; `V_threshold=V_cap+100*C*dV_cap/dt`. The discharge pins do not touch either cap;
they drive the 40106/4066 phase logic. The 4066 connects each decay slider to
its high-impedance buffered sustain wiper. The timer runs on a BC172
emitter-follower rail around 11.35 V; the shared control-voltage/sustain-top
node is factory-labelled +7.5 V. "556A" is the Signetics NE556 in its
14-pin A package, not a different part.

The subtle shared parts are now traced. SL3 and SL7 are two complete 100 kOhm
tracks from +7.5 V to one D15-biased bottom rail. Neglecting buffer bias,
`2·(7.5−V_f)/100k=I_D15(V_f)` and each target is
`V_s=V_f+s·(7.5−V_f)`. Release runs cap → SL4/SL8 → D11/D14 → the common GS
line, which is IC1/4075 pin 10 and both active-low 556 RESET pins—not ground.
Fast simultaneous releases can therefore couple through the real GS output
resistance. P1015's parts list does **not** identify D9–D15; the errata names
only D18/D22 as 1N4149, so applying that part number to the envelope diodes
would overstate the factory evidence.

**Engine.** Each travel maps to an RC *time constant*: 4.7 uF into the
voiced 1 kOhm..2 MOhm slider residual plus the drawn 100 Ohm gives 5.17 ms
to 9.40047 s, which is what the manual prints as
"5 milliseconds to 10 seconds" - one voiced ~1 kOhm effective slider-end
residual lands both printed endpoints, where the previous
three-time-constants read fit neither. The attack charges from
OUTPUT through its steering diode, so it
aims at V_OH - V_D against the +7.5 V peak: a ratio of about 1.3 (the
documents bound it to 1.22-1.35, and the engine ships the nominal), giving
ln(1.3/0.3) = 1.47 time constants to peak rather than a rail-charged
monostable's ln 3 = 1.10. Because the 556 senses above R23/R24, the cap-side
trip is `1-(100/R_slider)*(1.3-1)` of 7.5 V: 0.97 at the nominal fast end,
approaching unity as the slider resistance rises.

The trigger network adds a characteristic articulation notch. Selected X
and Y/EXT rising edges, and every selected keyboard KT pulse in MULTIPLE,
pull the common GS/reset line low for the drawing's nominal ~5 ms. Both caps
traverse their ordinary release sliders and D11/D14 during that interval;
the final GS rise creates TS and starts both attacks from their retained
post-notch voltages. The independent X/Y edge branches remain effective
while another source already holds the OR'ed gate high. SINGLE has no KT
branch: its first keyboard gate attacks immediately, a legato press does
nothing, and a press masked by an already-high X/Y gate cannot articulate.
Overlapping accepted edges extend one notch rather than scheduling several
attacks. The engine implements a nominal 5 ms width; the drawing's 4.7 ms
X/Y RCs, 10 ms KT stretcher and CMOS thresholds do not support claiming
5.000 ms for a real unit.

For a nominal matched-diode model, Ghostar voices the common D15 rail at
0.5 V—the independently derived Loudness-VCA zero—and uses an effective
43 mV diode slope. The two-track KCL then fixes `Is=1.2479468 nA` and
`f=1/15`, making settled Loudness gain equal panel sustain travel. D11/D14
use the same declared nominal law:
`V=R·I+a·ln(1+I/Is)`. A backward-Euler step reduces exactly to this monotone
scalar with `R+h/C`; release therefore has the original slow nonlinear knee
rather than a fixed 0.6 V floor. At maximum R the nominal 7.5→0.5 V audible
tail is 31.302 s, versus 25.456 s for the former pure exponential. A real
diode ultimately tends toward the finite GS low as current vanishes; it does
not park permanently at one constant forward voltage.

The downstream Loudness VCA offset is now derived too. LC reaches the
CEM3360 linear-control pin through R135=10 kΩ, with R136=3.3 kΩ to ground
and R137=240 kΩ to −12 V. KCL puts zero control at LC=0.5 V, exactly `1/15`
of the 7.5 V envelope peak. The engine therefore uses the nominal normalized
law `gain=max(0,(15·e−1)/14)`: the low-voltage release region is silent by
circuit design rather than by an arbitrary engine gate.

**Still voiced / not modelled.** The exact D9–D15 I-V/temperature law, the
per-source reset-pulse widths/CMOS thresholds, GS
`V_OL` and output resistance, inter-envelope release coupling, leakage and
the ~1 kOhm fast-end residual; also the attack aim's 1.22–1.35 spread because
no source fixes the bipolar 556 output-high drop at these currents. The
Loudness zero crossing is closed, but the preliminary original CEM3360 sheet
does not characterize exact top gain, saturation or per-device feedthrough;
unity at full envelope remains an engine normalization.

**Closes with.** Simultaneous Loudness/Filter cap and GS captures at several
S/R settings and temperatures, plus the fitted diode/CMOS output law.

## OQ-05 — Shaper Y SG phase law closed; trigger-edge acceptance open

The lossless SM DWG 3 scan shows that IC6 is the Shaper's hysteretic reversal
comparator, not a separate mid-level detector. SHAPE reaches pin 6 through
R61=100 kΩ; pin 5 receives half the SG output through equal R65/R66=22 kΩ.
Those half-rail crossings define the ramp's own extrema. RS3's aligned throws
are A4/B8/C12 FREE, A3/B7/C11 KBD HOLD, A2/B6/C10 RESET and A1/B5/C9 RUN.
Gang A closes only in FREE; B7 is the HOLD path while B6/B5 share the
RESET/RUN path; C11 selects through D25, C10 directly and C9 through D26.
Together with the 2N4856 clamp, that connectivity resolves SG as the IC6
phase state:

| Mode | Idle / before cycle | Rising leg | Apex / top hold | Falling or release | End |
|---|---|---|---|---|---|
| FREE | continuous cycle | high | high→low at the upper reversal | low | low→high at the lower reversal |
| KBD HOLD | low | high while the gate drives the rise | low while held at the top | low | low |
| RESET | low | high after an accepted reset | high→low at the apex | low | low |
| RUN | low | high after an accepted trigger | high→low at the apex | low | low |

A KBD HOLD re-gate during release reverses the ramp upward from its current
level and makes SG high. The engine and focused circuit test now use this
explicit phase/cycle/gate state in every mode; no interior level threshold
remains.

The manual closes one more edge rule: RUN ignores new gates only through its
rising segment. A selected KBD/MULTIPLE KT pulse therefore starts RUN again
after that rise even while the held-key gate remains high; SINGLE still needs
a genuine selected-bus rise. The engine and behavior suite now pin both
cases. Still open is exact X/external/Y edge acceptance in each mode,
especially self-Y feedback. Resolving that remainder requires a complete
dynamic IC6A/RS3C/FET trace or simultaneous selected-gate, SHAPE and SG
captures from the hardware.

## OQ-06 — Ring modulator — topology closed, unit residual open

The lossless P1013 scan overturns the earlier no-trim reading. Osc A's fixed
triangle passes through C15=1 µF into R26=39 kΩ || R27=100 kΩ
(`f_c=5.67245 Hz`), and that node drives both IC7's signal input and IC6's
non-inverting dry-A reference. Osc B drives IC7's control node through
R23=220 kΩ against R24=1.8 MΩ to +12 V and R25=62 kΩ to ground. IC6's
68 kΩ + P2=25 kΩ feedback adjusts A-carrier cancellation and product level:
P2 is the vintage unit's internal null trim.

With the CEM3340's nominal 0–4 V triangle and P2 at null, the resistor bias
reduces exactly to `ring = -(15/13)·HP(A_triangle)·B_triangle` in the engine's
bipolar triangle units. There is no ideal B-carrier term. The engine models
that transfer with a trapezoidal C15 companion and removes the invented
`0.03·(A+B)` leak. The circuit suite checks C15's KVL/charge equations and
zero carrier with either input absent.

Still open are the factory P2 setting, original-CEM3360 gain/feedthrough,
component tolerance and hence a particular unit's residual A or B carrier.
Those close only with the trimmer setting or a same-unit one-carrier spectrum.

## OQ-07 — Envelope-mode Shaper RATE span

**Gap.** OM gives the FREE-mode span (several cycles per minute to >20 Hz)
and says the same knob sets total rise+fall time in envelope modes, but
states no envelope-mode extremes. Lossless SM DWG 3 adds an important quirk:
P3 is 100 kΩ LIN, but its wiper is not an unloaded exponential-rate command.
It reaches the CEM3360 exponential-control node through R55=110 kΩ, with
R56=20 kΩ to ground and R60=2.2 kΩ coupling the wider rate network. For a
panel fraction `p`, even the isolated pot arm is
`Rp=110k+100k·p(1−p)`, so the electrical travel cannot be exactly log-linear.
The switched FREE and zero-clamped envelope paths around C19=15 nF also do
not prove that identical control current gives identical complete-cycle time.
**Engine.** One law for both: total period 20 s down to 45 ms across the
travel (matching FREE's stated verbal extremes). This remains explicitly
behavioral; the source does not supply CEM gain versus control voltage, the
loaded R60 node, or the mode-dependent current needed to replace it safely.
**Closes with.** With arpeggiator off, fixed LFO/S+H RATE and SHAPE=5, time a
FREE period and a RESET rise+fall at P3=0/.25/.5/.75/1 while probing IC7 pin
12 relative to pin 8. Endpoints close the span; all five points close the
loaded travel and expose any FREE/envelope duration ratio.

## OQ-08 — Glide time constant — endpoint closed, taper open

Lossless SM DWG 1 resolves C6 as 470 nF and P1 as 2 MΩ, fixing the full-
resistance time constant at `0.94 s`. P1 has no taper marking. The engine now
uses the exact endpoint with its existing quadratic travel
(`τ = 0.94·travel²`); only that curve remains a deliberate voicing. A circuit
test pins the one-sample full-travel lag coefficient. The taper closes with a
part marking or glide-time measurements at intermediate positions.

## OQ-09 — Upper-filter cascade drive and SW4 memory — closed

The Internet Archive's grayscale JP2 of SM DWG 2 resolves R181 as 91 kΩ,
with the drawn 220 Ω shunt. That puts the cascade section at the same +29 mV
bias as the Upper filter's LOW switch, whose Q=0.5 is anchored by the owner's
manual. The other section receives the selected LOW/VARIABLE resonance bus.
LOW and the fixed half therefore use exact `k=1/Q=2`; the engine no longer
subtracts its voiced VARIABLE/self-oscillation ceiling from this fixed bias.

**Closed and modelled.** The controlled-Q section comes
first; its output is both the 12 dB tap and the input of the downstream
fixed-Q=0.5 section selected at 24 dB. P1013 also ties each half's VIF and VIV
inputs together. The CES final sheet identifies their transconductors, so the
normalized tied-input drive is `u*(1+1/Q_commanded)`, not one canonical SVF
input; use commanded CEM Q before the external enhancement extension.

SW4 is stateful rather than a passive tap choice. Its common carries C40=1 nF
to ground and selects either 22 nF LP timing node, giving the selected node
23 nF and transferring C40's stored charge on a slope toggle. In 12 dB,
R194=1 MΩ weakly couples the two LP nodes; in 24 dB the switch shorts that
resistor. A linked pole changes IC14B's gain from 201 in 12 dB to 101 in
24 dB, an exact relative `101/201`.

Ghostar now advances the two halves in one coupled solve. The selected LP
integrator uses 23 nF, 12 dB includes R194's equal-and-opposite node current,
and the C37/BA130 scalar sees the coupled network's exact current sensitivity.
On a slope toggle, the newly selected node is projected to
`(22·Vnew + Vold)/23` before the next solve while the abandoned node and C37
state remain untouched. Each half receives its tied-input drive, using
commanded CEM Q before the declared external-enhancement term, and the 24 dB
tap is scaled by `101/201`. The circuit suite independently pins all four
state equations, C37 KVL, both switch projections and the gain ratio.

Hardware sweeps can now validate CEM3350 output impedance, contact resistance
and bounce, TL082 dynamics and original-unit tolerances; they no longer select
the topology.

## OQ-10 — Overdrive knee and drive

**Gap (local circuit and physical Lower drive implemented; RS7 panel phase,
clean outputs and C34 pre-charge open).** The lossless P1013
scan resolves the nonlinear branch and every terminal net, but not a named
three-deck switch position. IC12A is non-inverting;
R153=330 kΩ is feedback, R186=2.2 kΩ and R166=470 Ω meet its inverting
node in the distortion throw, and R164=R165=2.2 kΩ plus D1/D2 form the
BA130 return. A3 closes that nonlinear return and B7 selects IC12A pin 1.
C10 connects Lower VLP to C34-left through R167=33 kΩ, whereas C11 clamps
Lower VLP to ground. A3+B7+C10 is therefore a functional hypothesis, not a
traced switch position. A standard same-index 3P4T interpretation pairs the
panel OVERDRIVE phase with A3+B7+C11, contradicting the VLP-driven reduction;
installed-switch continuity or an assembly legend must resolve that conflict.

With ideal IC12A, the network reduces to one monotone scalar. For input node
`x`, diode voltage `q` and pair current `i=2·Is·sinh(q/Vd)`, solve
`q + 166100·i = 425.56383·x`; then
`o = 853.12766·x - 330000·i`.

Let `L` and `R` be C34's left and right nodes, `v=VLP`, `p=IC12 pin 1`,
`b=IC12 pin 7`, and `j=C34·d(L−R)/dt`. The B throws contribute
`B5: 0`, `B6: (L−p)/147k`, `B7: (L−p)/47k`, and
`B8: (L−b)/47k`. C9 and C11 impose `v=0`; C10 contributes
`(L−v)/33k`; C12 imposes `L=v`. For resistive selections,
`I_B+I_C+j=0`, while the right node obeys `j=R/220Ω+i_upper(R)`.
Neglecting the Upper input branch gives the current 220 Ω-dominant reduction.

For the hypothetical A3+B7+C10 combination only,
`(L−v)/33k + (L−o)/47k + j = 0`, hence
`Vth=(47·v+33·o)/80` and `Rth=33k||47k=19.3875k`. Those equations do not
prove that the combination is the panel OVERDRIVE detent. The BA130 fit
remains `Is≈2.3 nA`, `Vd≈43 mV` from its documented typical curve.

**Engine.** The traced scalar is driven from the production Lower MNA's VLP
state, not `dry + BP`. The engine derives a shared selected-wave/state
normalization of 5 V per unit from P1014's affine offset, replacing the
arbitrary 24 mV value. In the named OVERDRIVE mode it applies the
A3+B7+C10 hypothesis and passes that conditional Thevenin source through
C34 before the Upper filter. In every other named mode it still relaxes C34
through the same hypothesis with zero excitation, an explicitly nonphysical
interim approximation because the actual throw is unresolved. A
higher-resolution schematic trace corrects the earlier reading: RS7-C's
common is Lower VLP,
not C34. C9/C11 ground VLP, C10 feeds it through R167=33 kΩ to C34's left
node, and C12 connects it directly. RS7-B's common reaches C34 through
R187=47 kΩ: B5 is open, B6 receives IC12 pin 1 through R168=100 kΩ, B7
receives pin 1 directly, and B8 receives IC12 pin 7=`151·VBP`. A1/A2/A4 are
open while A3 closes the nonlinear return. These raw networks are closed,
but the schematic and PCB provide no installed rotor phasing that reconciles
the standard same-index reading with the functional C10 hypothesis or safely
assigns the other named modes. Encoding those contacts now would guess.

**Closes with.** A hardware shaft-to-contact continuity table or assembly
legend, followed by continuous C34/VLP captures in all four positions;
hardware distortion and mode-switch captures then validate the ideal-op-amp
and switching reductions.

## OQ-11 — No calibration-grade hardware data set

Every quantitative behaviour beyond the manual's stated ranges rests on
schematic math. Nobody has published filter curves, envelope timings, drift
data, or a hardware-vs-emulation comparison for any Spirit. The first
measured unit becomes this project's ground truth; the Museo del Synth
Marchigiano (which reverse-engineered the 2023 reissue) is the most likely
living source of calibration data.

## OQ-12 — Resonance-path BA130 limiter constants

**Gap (both external loops modelled; original-chip dynamics/headroom open).** P1013 contains two Figure-5-style
AC-coupled high-Q feedback loops. The Lower loop senses BP through IC12B:
R169/R170 gives gain 151, then R171/R172 divides by 11, for effective gain
≈13.73 before D5/D6 and C33=1 nF return the signal. The controlled Upper
loop uses IC14A with R178/R175 for gain 16 and returns through D3/D4 and
C37=1 nF. Curtis Figure 5's corresponding gain is ≈15.24 with the same
1 nF coupling value. The fixed-Q Upper half has no limiter pair. The BA130
curve is found (OQ-10). **The travel-to-Q half of this entry is closed
outright** — see below.

**Engine — the external loops.** Controlled Upper's real 1 nF capacitor adds
one trapezoidal companion to its resolved CEM section; its gain is 16 and the
ideal op-amp source is zero ohms. Lower uses the traced gain 13.7273, 2 kΩ
source resistance, BA130 pair and C33 companion inside the production
three-wiper VLP/VBP MNA; the diode endpoint sensitivities come from that 2×2
solve rather than a canonical-input surrogate. Fixed Upper remains linear
because P1013 gives it no branch. P1014 supplies a common derived 5 V/unit
normalization
for selected oscillators and CEM states. TL082 bandwidth/output swing,
CEM3350 internal saturation, component tolerances and absolute MM5837 volts
remain deferred rather than invented.

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

**Closes with.** Resonance-node and output traces from one unit to validate
TL082 dynamics/output impedance, CEM3350 internal headroom and the nominal
source/state scaling against a hardware high-Q sweep. The travel-to-damping
network no longer needs a choice, but reading the chip's typical Q=50
"without enhancement" point as exact negative damping remains a voiced
extension to validate. Internal CEM3350 saturation is a separate open term.

## OQ-13 — Filter-tracking amount and pivot — closed

Tracking's amount is 108.3%: the 1 V/octave keyboard bus enters the same
12k1 ladder as the panel CV, giving 21.2 mV/V against the CEM3350's
−19.6 mV/octave. The absolute pivot is closed by P1016's signed DAC
reduction. Six key-address bits drive DAC0800 B1–B6 while B7/B8 are grounded,
so semitone index `q` above the lowest C produces word `N=4q`. R31=`4k99`
feeds the positive reference; R30=`5k1` only balances reference-input bias.
IC8 pin 4 sinks

`I_DAC = (+12A/4.99k)·4q/256`

from IC16A's virtual earth, opposing the current sourced into that node by
R39=`26k6`. With R40=`2k43`, its output is

`V_D(q) = +12A·2.43k·[q/(64·4.99k) − 1/26.6k]`.

That is −1.0962406 V at the lowest C and rises 91.3076 mV/semitone, or
1.095691 V/octave, matching DWG 1's `1.1 V/OCTAVE`. The two currents cancel
at `q=64·4.99/26.6=12.006015` semitones. IC16B and the normalled glide path
add no fixed reference, so with the original lowest C mapped to MIDI 48 the
nominal tracking pivot is MIDI `60.006015`—0.6015 cent above the second C.
Because both currents use +12A, rail voltage and R40 cancel out of the pivot;
downstream gain cannot move it either.

**Engine.** Uses the component expression
`48 + 64·4.99/26.6`, not rounded MIDI 60. The circuit suite pins both its
signed sub-cent offset and the independently derived `2^1.083` adjacent-
octave ratio.

**Closed by.** Factory SM DWG 1 P1016 plus the DAC0800 positive-reference
current law. Real resistor tolerance, DAC error and 1458 offset can move a
particular unit by a small amount; that is calibration scatter, not a missing
nominal law.

## OQ-14 — Wheel modulation depths

**Partially closed — destination loading.** DWG 2 resolves two importantly
different controls. X is a CEM3360 current output into its 100 kΩ wheel as a
rheostat; Y is the SHAPE voltage through R60=15 kΩ into a conventional
100 kΩ divider. RS1/RS2 then change the wheel load. One oscillator presents
`22k||100k=18.032787k`, two present `22k||100k||100k=15.277778k`; one filter
presents 100 kΩ and two present 50 kΩ. For electrical resistance fraction
`t`, Ghostar therefore uses

`X(t,L)=(100t||L)/(100||18.032787)`

and, with `z=100t||L`,

`Y(t,L)=[z/(15+100(1−t)+z)]/[15.277778/(15+15.277778)]`.

The filter routes additionally carry the derived CEM3350/CEM3340 sensitivity
ratio `21.2/19.6`. With the separate voiced full-wheel pitch anchors retained
at one octave for X→A and Y→B, the resulting full depths are X→A+B
0.867470 octave each, X→U 3.539889 octaves, X→U+L 2.359926 octaves each,
Y→A+B 0.929638 octave each and Y→L 1.648923 octaves. The loading is a real
playability quirk: at half resistance travel X→A is already at 0.867470 of
its full depth, while Y→B is only at 0.335643. Both control-rate and
audio-rate X paths use the same network; FORMANT still disconnects L.

**Still open.** The drawings do not mark either wheel's taper, so mapping
panel travel directly to electrical resistance fraction remains an explicit
linear-taper assumption. Absolute X depth depends on selected-source level and
CEM3360 gain; absolute Y depth depends on the loaded SHAPE swing. The RWM
branches visibly load through 200 kΩ/620 kΩ and their BC308 networks, but the
active duty conversion, PW trims and CEM3340 PWM input prevent an exact
transfer; ±0.42 duty remains voiced. Y→LFO RATE is positive, but its
CEM3360 law and fastest full-wheel rate remain unresolved, so 60 Hz remains
voiced.

**Closes with.** Wheel-pot taper identification plus same-unit measurements
of X→A and Y→B full depth, RWM duty versus wheel voltage, and Y-wheel-end LFO
rate; or complete original-CEM3360/BC308 transfer data and calibrated source
swings.

## OQ-15 — Noise source clock and absolute level

**Gap (colouring transfer closed).** The grayscale SM DWG 2 scan resolves
the entire P1013 network: C17=1 µF, R4=200 kΩ, R5=10 kΩ; R6=18 kΩ with
C8=220 nF; R7=3 kΩ with C9=100 nF; C10=10 nF; then IC4A with R8=27 kΩ
and feedback `R11=1 MΩ || (R12=100 kΩ + C11=15 nF)`. Its passive transfer
is

`P(s) = R4*C17*s / [1 + (R4+R5)Y(s) + R4*C17*s*(1+R5*Y(s))]`,

where `Y(s)=s*C10+s*C8/(1+s*R6*C8)+s*C9/(1+s*R7*C9)`. The active stage is
`A(s)=(3891s+2054000)/(27(33s+2000))`. Thus `H=P*A` has zeros at DC,
40.1906, 84.0155 and 530.5165 Hz and poles at 0.5949, 9.6458, 31.0388,
179.2754 and 8157.42 Hz. Checks: `|H(20 Hz)|=11.8523`,
`|H(1 kHz)|=0.945892`, `|H(10 kHz)|=0.539258`.

The MM5837 is a self-clocked, sample-held binary 17-stage maximal PRBS
(taps 17/14; 131071-bit period). Its datasheet bounds cycle time to
1.1--2.4 s and half-power to 24--56 kHz, but gives no typical part; nor is
the chip's swing calibrated against the same unit's CEM3340 waveform swing.

**Engine.** Five numerically stable bilinear first-order sections implement
the derived transfer, driven by the maximal PRBS at 75 kHz (the midpoint of
the datasheet cycle range). One visible `0.35` source-level constant preserves
the preceding engine's filter-path noise RMS; clock and absolute level are
voiced. The former Kellet filter, floating LCG and arbitrary white/pink blend
are gone. The circuit suite pins the three magnitudes above at 44.1, 48 and
96 kHz hosts and pins the exact 131071-bit repeat.

**Closes with.** A same-unit capture of MM5837 repeat time/output swing,
CEM3340 waveform swing, and the coloured-noise mixer level. The network
transfer itself is closed.

## OQ-16 — Audio-path coupling — closed

P1017 draws no series capacitor on either rear output, so the former global
5 Hz high-pass was unsupported. The actual path capacitor is C30=470 nF
*before* the Filter path's loudness CEM3360. It sees R132=24 kΩ in parallel
with R133=100 kΩ (`R_L=19.3548 kΩ`, `τ=9.09677 ms`, `f_c=17.4958 Hz`) and
continues charging while the VCA is closed. The Shaper output is DC-coupled.

The engine now solves C30 as a trapezoidal capacitor companion and removes
the global output block. P1017's normal contact joins the two P4 wipers
through R49+R50=20 kΩ; inserting the SHAPED plug opens that link. This is
more than an unconditional half-sum because C18/P3 loads the Shaper's full
20 kΩ track and the joined wipers cross-load both paths.

For isolated full-track voltages `X_s` and `X_f`, Master travel `m`,
`r=20k·m(1−m)`, `λ=20k·m²/(20k+2r)`, and
`β(s)=20k·s·C18/(1+s·C18·R_b)`, the normalled high-impedance main output is

`O/X_s = (m/2)·(1+2λ) / [1+2λ+(1+λ)β]`,

`O/X_f = (m/2)·(1+2λ+β) / [1+2λ+(1+λ)β]`.

At DC, `β=0` and this reduces to `m·(X_f+X_s)/2`. At audio frequencies the
BRIGHTNESS branch also colours the Filter contribution. SPLIT sets the
cross-link to zero and exposes the isolated wipers `m·X_f` and the separately
BRIGHTNESS-coloured `m·X_s`.

The engine solves the endpoint-safe coupled MNA and C18 companion at the 4×
internal rate before decimation. Circuit tests pin its wiper/top-node KCL,
C18 KVL/charge, both Master endpoints, split isolation, the DC half-sum limit,
and the characteristic dark/high-frequency case where a Filter-only main
signal is one quarter of its isolated level rather than one half.

## OQ-17 — Red-noise modulation bus scale

**Gap (circuit transfer closed, bus scale open).** SM DWG 2 routes the
R6/C8 junction of OQ-15's passive network to IC4B's non-inverting input.
R9=2.2 kΩ and R10=100 kΩ set an AC gain of
`1+R10/R9 = 511/11 = 46.4545`, and J1/6 carries that output directly to
P1015's MOD SOURCE switch. If `P(s)` is OQ-15's passive audio-node
transfer, the red-noise output is

`H_red(s) = (511/11) * P(s) / (1+s*R6*C8)`.

The R6/C8 pole cancels `P`'s 40.1906 Hz zero, leaving zeros at DC and
530.5165 Hz and poles at 0.5949, 31.0388, 179.2754 and 8157.42 Hz. No
separate 1.5 Hz filter is present. Its derived magnitudes are 29.8308 at
1 Hz, 29.0212 at 20 Hz and 9.14834 at 100 Hz. What remains unknown is the
voltage at J1/6 that corresponds to full useful travel on the engine's X bus.

**Engine.** `SpiritNoise` implements the derived R6/C8-to-IC4B branch from
the same fixed-clock MM5837 that drives the audio output. A visible `0.26`
bus gain, followed by the engine's ±1 modulation bound, preserves the prior
patch depth and remains voiced. The circuit suite spot-checks the transfer
at 1, 20 and 100 Hz on the common 44.1, 48 and 96 kHz host grids.

**Closes with.** A same-unit voltage capture at J1/6 and on the X bus, or
destination-depth measurements from a hardware unit. The red-branch topology
and transfer need no further voicing.

## OQ-18 — Shaper SHAPE endpoint split — closed

The lossless grayscale SM DWG 3 scan resolves P4 as `1M LIN`, with D19/D20
steering its opposite ends through the same 27 kΩ R62 on successive
half-cycles. The patent establishes that half-cycle time is proportional to
the selected resistance. Thus, for panel travel `t`,

`rise fraction = (27k + t·1M) / (1M + 2·27k)`.

The endpoints are 2.5617/97.4383 and 97.4383/2.5617, with 50/50 at centre;
the two resistances always sum to 1.054 MΩ, so SHAPE cannot alter the period.
The engine uses this component law. The circuit suite times complete FREE
cycles at five pot positions against an independent P4/R62 oracle and also
checks the constant-period invariant.

## OQ-19 — Master volume taper — closed

SM DWG 2 explicitly marks both gangs `20k LIN`, one after each audio path.
The engine therefore applies travel directly as output gain; the unsupported
square law is removed. The circuit suite pins half travel to half output.

## OQ-20 — Mixer transfer and absolute gain

**Gap (Lower MNA implemented; RS7 dry transfer and remaining absolute levels open).** Every
audio mixer control is an unbuffered 100 kΩ linear slider. At travel `t`, its
Thevenin source is `t*V_in` in series with `100k*t*(1-t)`. For a resistive arm
`R_s` ending in the Shaper's virtual-earth summer this gives the exact
full-travel-normalised law

`g(t, R_s) = t*R_s / [R_s + 100k*t*(1-t)]`.

The Shaper arms are 47 kΩ for A/B/Ring and, after the service erratum,
6.8 kΩ for Noise. Consequently `47/6.8 = 6.9118` is their ratio only at
full travel; at equal half travel it is `72/31.8 = 2.2642`.

The Filter is different. Each wiper has a 220 kΩ arm to Lower CEM pin 3
(VLP, C31=22 nF) and a separate 68 pF arm to pin 5 (VBP, C32=22 nF); pins 2
and 4 are grounded. The destination voltages move, the three sources interact,
and even an off slider continues to load both state nodes. Therefore neither
the grounded-bus slider law nor a `220 kΩ || 68 pF` shelf is its transfer;
10.64 kHz is merely where those two component admittances would be equal if
their destination voltages matched, not a circuit zero.

The production network reduces to three wiper-cap companions plus
the VLP/VBP timing states and C33 collapse each sample to a 2×2 linear solve
followed by the same monotone BA130-current scalar used by OQ-12. Exact slider
endpoints need a charge-preserving state projection to remove trapezoidal's
hidden alternating mode, not an invented series resistor. This derivation is
implemented, but `OUT`, `BANDPASS` and `HIGHPASS` cannot yet be assigned to the
remaining RS7 throws from P1013 alone. The owner's manual proves OUT preserves
a dry transfer into Upper; the production net and throw implementing it remain
unresolved, so guessing by terminal number would be wrong.

**Engine.** The Shaper applies the derived loaded-slider law with its 47 kΩ
arms and 6.8 kΩ Noise arm behind one voiced 0.45 scale. The Filter runs the
full coupled 2×2 solve with all three 100 kΩ Thevenins, 220 kΩ arms, 68 pF
companions, 22 nF VLP/VBP states and C33's implicit BA130 current. P1014's
conditioned A/B voltages enter directly in their shared derived 5 V/unit
normalization;
the MM5837 contribution retains one explicit 0.45 level calibration. The
one-step circuit oracle checks branch KCL, both integrated state equations,
C33 KVL, off-slider loading and pot-end charge projection. Only the dry/output
scalar used by the unresolved RS7 modes remains a labelled 0.45 surrogate.

**Closes with.** A hardware RS7 continuity table or assembly drawing that
identifies the three remaining throws and OUT's dry transfer. Same-unit
MM5837, Shaper-summer and mixer-output captures are still required to pin the
noise level, CEM3360 top gain and dry scalar; selected CEM3340 relative and
nominal pre-1458 swings are now source-derived from P1014 rather than
equalised.

## OQ-21 — LFO rate span at the slow end

**Gap.** The manual gives the MOD X rate as "less than 1 Hz to
approximately 50 Hz". The fast end is a stated number; the slow end is
only an inequality. The lossless MOD-board drawing does resolve the control
travel: P2=100 kΩ LIN is loaded through R33=200 kΩ into the CEM3360
exponential-control node. If `x` is mechanical travel, its normalized
electrical contribution is therefore

`w(x) = 200x / (200 + 100x(1−x))`,

so half travel is `w=4/9`, not `1/2`. P2/R33 and R35=2.2 kΩ also give a
132 mV full control swing. A modern compatible's nominal 3 mV/dB scale would
*suggest* a 158.489:1 span, or 0.3155 Hz at the slow end when the documented
fast end is 50 Hz; even that part's published 2.7–3.3 mV/dB range expands the
inference to roughly 0.180–0.500 Hz. It is therefore useful corroboration, not
evidence for the original CEM3360's exact scale or a particular calibrated
Spirit.

**Engine.** The derived loaded travel `w(x)` feeds a 0.3 Hz-to-50 Hz
exponential law. Thus the deliberately voiced slow endpoint still satisfies
"less than 1 Hz", while half travel is now the circuit-derived 2.9148 Hz
rather than the unloaded geometric midpoint 3.8730 Hz.

**Closes with.** An original-CEM3360 exponential-control scale or, preferably,
timing measurements from a calibrated Spirit at the slow endpoint and one or
more intermediate knob positions.

## OQ-22 — BRIGHTNESS topology closed, pot taper open

P1013 resolves the topology. After the Shaper CEM3360 VCA, C18=27 nF in
series with P3=100 kΩ LOG forms a shunt across the fixed 20 kΩ full-track
load of the Master Volume gang. For rheostat resistance `R`, normalized to
the branch-open DC output,

`H(s) = (1 + s·C18·R) / (1 + s·C18·(R + 20k))`.

That expression is the isolated/SPLIT Shaper transfer. At dark `R=0`, it is
a low-pass at 294.731 Hz. At bright `R=100 kΩ`, its pole is 49.1219 Hz, zero
58.9463 Hz and high shelf 5/6 (−1.5836 dB). It is neither the former
variable-cutoff RC nor fully open, and no 330 Ω residual exists.

With P1017 normalled, OQ-16's coupled MNA changes the effective capacitor
load with Master travel. At full Master, the effective resistance is
13.333 kΩ: dark's main-output pole is 442.097 Hz and bright's pole is
52.0114 Hz. At high frequency, the Shaper and Filter contributions retain
15/17 (−1.087 dB) and 16/17 (−0.527 dB) of their respective DC gains. Thus a
Shaper-labelled tone control subtly colours the Filter through the original
jack normaling — a hardware interaction the engine now preserves.

The engine solves C18/P3 inside the endpoint-safe output MNA at the 4×
internal rate, so both its capacitor history and changes of Master,
BRIGHTNESS or SPLIT follow the physical network.

Only the word `LOG`, not the manufacturer's taper curve, is printed. The
engine retains a visible 2.5-decade voicing between the exact 0 and 100 kΩ
endpoints: `R=100k·(316.228^travel−1)/(316.228−1)`. The curve closes with a
P3 part marking or a multi-position hardware sweep.

## OQ-23 — The travel smoother is a product policy, not a hardware law

Recorded here so the sweep is exhaustive rather than because it is a gap.
Every continuous panel travel and both wheels glide to new values with a
~25 ms one-pole (`travelSmoothing_`), and a fully silent engine snaps
instead. No hardware analogue is claimed: a physical pot's wiper moves
continuously and needs no smoothing, and the smoother exists because a
host applies automation in block-sized steps and a MIDI CC in 7-bit
ones. The measurements that justified it, and the metric they forced, are
in the best-in-class plan's Step 5 section. It is listed as a standing
item only so that a future reader does not mistake 25 ms for a modelled
time constant.

## OQ-24 — A reference-free alias measure for the pitched strokes

The alias audit compares the shipping render against a 16× ground truth.
Review established, and measurement confirmed, that this cannot certify a
−60 dB alias-to-signal gate on tonal material: the two renders disagree
slightly about partial *level*, so the comparison must carry a tolerance,
and that tolerance leaves about 15 dB of room under every partial for
something to hide in. A component landing exactly on a partial is
indistinguishable from that partial being marginally louder, by this or any
other magnitude comparison. The instrument now publishes that floor
(`blindDb` in `Tools/AliasMetric.h`) and marks every row undecidable rather
than passing, and the plan's Step 1 gate verdict is withdrawn accordingly.

What would decide it needs no reference at all. A stroke that holds a pitch
has a harmonic spectrum and nothing else, so everything off the grid is
alias and noise — no second render to disagree with, no tolerance, no blind
spot that matters, because a component landing on a harmonic *is* a
harmonic and that is not what aliasing sounds like.

A first attempt is not shipped, for two reasons found while building it.
The fundamental has to be the **sparsest** grid that explains the spectrum,
not the best-fitting one: a low enough fundamental covers every bin and
"explains" any spectrum at all, which produced figures like −137 dB that
were an artefact of the search rather than a measurement. And the measure
has to be **differential** — several strokes carry off-grid energy of their
own, from an envelope or a noise source, which both renders carry alike, so
only the excess over the ground truth is alias. With both corrections the
figures were still not stable enough to publish (`wide-pulse3-10k` and
`selfosc-highcutoff` came back near −43 dB where the comparison bounds their
whole difference from the reference at −56 and −26 dB).

The requirement, when this is picked up: the grid estimate must be validated
against strokes whose fundamental is known independently, and the periodicity
test must reject a stroke whose spectrum a sparse grid cannot nearly account
for, rather than falling back on a denser one. Half a measurement is what
produced the defect this entry exists because of.

## OQ-25 — CEM3340 pitch pole modelled; PWM/filter delay open

MOD SOURCE = OSC B carries the selected post-IC10 audio waveform, not a
host-rate control. The former current/prior-sample split was only a causal
numerical policy for its pitch destinations. P1014 and the Curtis datasheet
now close the physical memory: each CEM3340 multiplier current output, pin 14,
returns to ground through 1.82 kΩ with 1 nF in parallel (A R82/C72; B
R118/C77). Curtis states that bypassing `R_s` limits the multiplier bandwidth
and gives `f_LP=1/(2πR_sC)`; normalizing the elementary parallel-RC
current-to-voltage transfer to its DC value gives

`H_pitch(s)=1/(1+s·R_s·C)`, `tau=1.82 µs`,
`f_c=1/(2πtau)=87,447.7709 Hz`.

Pin 15 is the chip's pitch-current summing node, so this pole acts on each
oscillator's **complete** octave/CV sum before exponential conversion:
keyboard, bend, tune/range/interval and X/Y modulation together. It is not an
Osc-B-modulation-only delay. At 10 kHz its nominal response is −0.05642 dB,
−6.5237°, with 1.79651 µs group delay.

Ghostar realizes that capacitor exactly for a linearly interpolated input on
the 4× internal grid. For `r=T/tau`:

`a=exp(−r)`, `q=−expm1(−r)/r`, `b_now=1−q`, `b_prev=q−a`,

`y[n]=a·y[n−1]+b_now·u[n]+b_prev·u[n−1]`.

The coefficients are non-negative at every supported rate, DC gain is one,
and `T·(a+b_prev)/(1−a)=tau`, so the model keeps the hardware's 1.82 µs
low-frequency delay without an alternating low-rate step response. At a
44.1 kHz host (`176.4 kHz` internal), the coefficients are
`a=0.0443874`, `b_now=0.6932025`, `b_prev=0.2624101`; its 10 kHz phase is
−6.490°, 0.034° from the analog pole.

Unsynchronised A is acyclic, so fresh B advances its real capacitor before A
steps. B self-FM and synchronised A are cyclic: each phase step is predicted
from a temporary copy advanced with causal prior B, then the real capacitor is
committed exactly once against fresh emitted B at the interval endpoint. That
split breaks `B → A frequency → A reset → B` without imposing an extra whole
internal-sample delay on newly known keyboard/tune/base CV. The capacitors
retain charge through ordinary pitch/routing changes and initialize at the
current static sum only after prepare/reset, avoiding a fictitious power-up
swoop.

What remains open is destination-specific, not a generic pitch delay. The A/B
PWM branches contain their BC308 conditioner, PW trim and 10 nF network; the
filter branches enter LM1458 summers and undocumented CEM3350 control
dynamics. Their signed transfer and group delay still require simultaneous
post-IC10/destination-node measurements. Ghostar therefore leaves PWM and
filter modulation on the existing causal schedule rather than inventing
extra poles.

## OQ-26 — Shaper audio-VCA control node

**Corrected topology; conditional divider KCL derived; the active transfer
remains open.**
Physical SHAPE voltage `S` reaches CEM3360 linear-control pin 5 through
R38=10 kΩ; the node also has R40=5.6 kΩ to ground and R41=1 MΩ to −12 V.
The extra drive is a two-emitter-follower chain, not an always-biased TR2:
`J5/1 → R30=30k → TR1 → R31=3.6k → TR2 → R39=3.6k → V_C`.
R29=100 kΩ and R32=36 kΩ bias TR1's base from ±12 V.

Outside FREE **with SHAPE X WITH Y open**, RS3-B8 is open and P1015's
R81=100 kΩ pulls J5/1 toward −20 V. Together with R30/R29/R32 this is
the nominal schematic reduction `V_T=−8.07518797 V` through
`R_T=21.9924812 kΩ`. With the printed rails and resistor values it keeps both
BC173s out of **forward** conduction. While their reverse base-emitter currents
remain negligible, the switch-open envelope-mode law is

`V_C=0.3576903424·S−0.0429228411−i_5/G`,
`G=1/10k+1/5.6k+1/1M`.

No cited source locates a voltage interval where that reverse current is
negligible against the CEM pin's typical −1.6 µA bias. The ITT sheet gives no
sub-breakdown leakage curve, and its `>5 V at 1 µA` point cannot turn the
conditional divider equation into a closed low/mid transfer.

That condition does not hold safely across the entire top range. The cited
ITT BC173 gives a 5 V emitter-base maximum and `V(BR)EBO>5 V` at 1 µA. With
negligible chain current, `V_C=1.93 V` puts `1.93−(−8.07519)=10.0052 V`
across the two reverse E-B junctions; 2.0 V control makes it 10.0752 V. The
sharing is device-dependent, so one junction may avalanche before the ideal
sum. The numerical coincidence with the CEM's typical maximum-gain point is a
credible candidate for an original-unit top-end quirk, but the Spirit BOM
gives neither BC173 maker nor gain grade, and the available curve does not
define a predictive two-device avalanche law.

The 1984 production CEM3360 sheet specifies 52 %/V typical, 1.93 V typical
maximum-gain voltage, −1.6 µA typical linear-input bias and 80 dB typical
attenuation at zero. With its conventional signed `i_5=−1.6 µA`, the law is
`V_C=0.3576903424·S−0.0371997956`: zero at `S=0.104 V` and nominal full gain
at approximately `S=5.50 V`. Those typical chip figures give strong
design-consistency evidence for a normalized unipolar envelope gain, not an
exact installed-unit transfer; the upstream loaded swing is not printed.

Closing SHAPE X WITH Y connects R33 to the post-R31/TR2-base node. That load
couples the follower chain to the X-VCA control path, so neither the
divider-only law nor the forward-off reduction can be carried into the
closed-switch case without solving its source and load.

In FREE, B8 connects J5/1 to the other IC9/3240 half around
R64=2.2 kΩ, C11=47 nF and D22—not directly to `S` or SG. If `F` is its actual
connector voltage, the nominal-resistor TR1-base Thevenin source is
`V_T(F)=0.46875·F−3 V`, `R_T=14.0625 kΩ`. Its coupled two-transistor KCL is
topologically closed, but a predictive transfer still needs `F`'s loaded
waveform, polarity and source impedance, a selected transistor characteristic
and the CEM pin-current law. The CEM sheet likewise omits out-of-range
control-pin behavior, including whether or where it clamps. The cited ITT
data cover one BC173 manufacturer, while the Spirit BOM selects neither maker
nor B/C gain group. Substituting `F=S`, `F=SG` or an ideal rail would invent
a distinctive FREE-mode knee.

**Engine.** Retains the practical normalized approximation `gain=max(0,Y)`
for every mode. Even the conditional switch-open divider is not an exact
sourced transfer: if the undocumented SHAPE swing is provisionally
normalized to the nominal ≈5.50 V consistency point, the typical-sheet result
is closer to `max(0,1.0193Y−0.0193)`, before the CEM's finite 70 dB minimum /
80 dB typical zero-control attenuation. The simpler law stays explicitly
behavioral; no guessed switch load, reverse-avalanche curve, transistor rail
or CEM out-of-range law has entered DSP.

**Closes with.** Simultaneous high-impedance captures of J5/1 (`F`), J5/3
(`S`), J4/5 (SG), both transistor base/emitter nodes and IC5/5 (`V_C`) through
a slow non-FREE sweep into maximum control and across both FREE ramp legs,
with both SHAPE-X-WITH-Y states. That reveals any reverse-avalanche onset as
well as the switch load and supplies the final coupled solve.

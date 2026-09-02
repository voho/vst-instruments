# YouKnow106 — decision log

Model directions and listening verdicts. A choice made by ear is recorded as
made by ear, never written up as though a measurement had settled it, and none
of these closes an open question — the captures named under
[known gaps](../README.md#known-gaps) are still what would.

## 2026-09-02 — Voice BA662 signal saturation

The voice VCA's signal path is now the BA662 differential pair's
`I_tail · tanh(V_d / 2V_t)` rather than a linear multiply. The pair has no
linearising diodes (Open Music Labs' reverse-engineered BA662; the BA6110
sibling does carry them and corroborates only the family's gm law), so the
shape has no free constant, and Roland's own ADJUSTMENT steps fix how hard it
is driven: on the same bank and key, s. 5 sets 4.8 Vp-p at the filter output
and s. 6 sets 6 Vp-p at the VCA output, so the drive follows from the output
side alone — 3.0 V across the load against a full-control tail of
(9.92 + 0.26 − 0.62) V / 32 kΩ = 299 µA — and the unread pin-9 divider
cancels. The one magnitude-setting value the JUNO-106 drawings do not print,
the OTA's output load, is now read from Roland's own JUNO-6 and JUNO-60
Service Notes (CPU BOARD, p. 9 in both), which draw the discrete IR3109 +
BA662 voice circuit the 80017A integrates with R42 47 kΩ on the VCA BA662's
output; the Open80017a reconstruction agrees. That gives u_trim = 0.217 and
11.06 V of headroom at the filter-output node.

This supersedes, for the signal nonlinearity only, the 2026-08-31 sentence
below that "BA662 signal nonlinearity/noise/thump and converter charge
injection remain unimplemented: available sources settle topology and nominal
time constants, not the original hybrid transfer". The sibling drawing is the
new evidence: the law was never in question, and the load is now a
Roland-drawn value of the same circuit rather than one clone's choice. The
pair's noise, its thump and the converter charge injection remain as that
entry left them. This is an evidence-priority decision under the realism/CPU
goal, not a listening verdict, and it does not close OQ-19: the 80017A's own
printed load and input network are still unread, and a TP19-against-TP8
level-swept THD capture (HD3 = −48 dBc predicted at the 4.8 Vp-p trim) would
confirm the headroom directly. The control law, VR30 null and C59 corner are
untouched, the switch-off path is bit-identical to the previous engine, the
self-oscillation anchor is untouched at the filter node, and the 4 Vp-p TP8
noise figure moves by under 0.1 dB, inside its stated crest-convention band.

Measured on the shipping path at 48 kHz/4×: a full saw+pulse+sub open-filter
voice compresses by −0.75 dB with a level-matched residual of −28.8 dBc
(whole-file on-minus-off −20.9 dBc, dominated by the gain term); a filtered
saw by −0.07 dB and −49.4 dBc (−41.1 dBc whole-file); the self-oscillation
corpus row by −0.10 dB, the pair's prediction at the trim level. A native
Apple silicon Release paired benchmark (Poly/Cubic/RK4, 256-frame blocks,
seven alternating rounds) moved median thread CPU by −0.70 % idle, +2.04 %
on six plain voices, +2.22 % on six resonant voices and +1.77 % on the
six-voice full-mixer Chorus II case, inside the +5 % budget.

## 2026-09-01 — NOISE control onset

The circuit-derived linear-above-onset NOISE level profile is now the
default. Module board p. 13 draws Tr22 (PNP, base grounded) fed from the
NOISE LEVEL hold through R115 10 kΩ and VR32 100 kΩ in series, with R114
2.2 MΩ pulling its emitter node towards −15 V, and its collector straight
into IC14's BA662 control pin. The control current is therefore zero until
the hold clears 0.6 V + Rs × 7.09 µA and linear above, with the anchored
full-level endpoint unchanged. The hold stands on the anchored +0.26 V VR34
standoff (p. 18 section 3, TP7 → IC26 → NOISE LEVEL), so the onset is
measured from there. Rs is VR32, the p. 19 section 9 NOISE LEVEL trimmer:
the notes fix its criterion (4 Vp-p at TP8) but not its position, which
follows Tr21's selected amplitude, so the onset is bracketed 0.671 V (VR32
at zero) to 1.380 V (maximum) and ships at the floor, the one position that
never overstates the deadband.

This is an evidence-priority decision under the realism/CPU goal, not a
listening verdict and not a closure of OQ-16: VR32's installed position and
Tr21's selected amplitude still await a TP8 sweep or a trimmer reading. The
BA662's input saturation of the noise is left out because its drive is
unfixed by the sources. The legacy linear-from-zero law remains available
behind the internal comparison switch.

## 2026-09-01 — Resonance onset on the VR34 standoff

The circuit-derived resonance profile now measures its junction onset from
the +0.26 V the RES CV hold already stands at, not from 0 V. Service Notes
p. 18 section 3 trims VR34 for +0.25…+0.27 V at TP7 with the D/A forced to
0 V; p. 13 injects VR34 through R127 into IC27b, whose output is TP7; p. 8
routes TP7 through IC26 to RES CV as well as VCA CV; and the p. 13 resonance
leg (IC26 ch6, C86, IC22c, VR26, R107, grounded-base Tr18) has no pull-down
to divide it. The engine already treated that standoff as anchored for the
voice VCA rail, so the onset moves from 0.6 V to 0.34 V above the hold's
zero: first loop gain at stored byte ~4 instead of ~8, about +4 dB at
Resonance 1/10, +1.3 dB at 2/10, +0.3 dB at 5/10 and nothing at 10/10. The
endpoint is the same service-trimmed self-oscillation maximum, the 0.2296
compensation is untouched, and no DSP work is added.

This is an evidence-priority correction, not a listening verdict and not a
closure of OQ-09: the standoff is the anchored service state, but the 0.6 V
junction drop above it remains the nominal prior a measured
response-versus-resonance family would replace.

## 2026-09-01 — Voice-VCA control: exact junction law replaces softplus

The voice VCA's envelope-to-gain law is now the solved emitter equation of the
traced Tr20 grounded-base stage — R106 + R105 = 32 kΩ, kT/q and the VR34
+0.26 V standoff, all already in tree — tabulated once at prepare time. The
softplus it replaces was labelled in the code as a smooth, replaceable
approximation of that same topology; the exact law shares its sub-knee
exponential tail and its full-scale point and differs only in between, where
V_be keeps rising with current: −0.2 dB at control 0.010, −2.5 dB at 0.020,
−1.6 dB at 0.050, −0.8 dB at 0.10, −0.24 dB at 0.30, −0.05 dB at 0.70, 0 at
full scale. Audibly, release tails and slow decays between about −25 and
−50 dB close one to two and a half decibels sooner; attacks cross that region
in under a millisecond and sustain levels do not move.

This is an evidence-priority shape replacement on an anchored topology, not a
listening verdict, and it does not close OQ-19. The knee stays voiced: the
reconstruction's 150 mV on the +0.26 V standoff, carried over under the stated
convention that the exact law's tail coincides with the former softplus's. That
convention is a convention, not a derivation — the other defensible placement
(emitter current equal to Vt/R at the turn-on) moves the −45 dB region by about
9 dB, several times the 2.5 dB the shape itself changes — so OQ-19's measured
gain sweep owns placement. The former softplus remains available behind the
internal comparison switch `useSoftplusVoiceVcaCompatibilityLaw`, bit-exact.
CPU: one table index and lerp per voice per internal sample in place of the
softplus; the repo's paired A/B benchmark at the shipping Poly/Cubic/RK4
4x 48 kHz defaults read, over three runs, idle 77.65 → 78.04 ms (+0.50 %),
six-voice plain 214.24 → 213.77 ms (−0.22 %), six-voice resonant 216.65 →
216.80 ms (+0.07 %) and six-voice full-mixer Chorus II 303.97 → 304.99 ms
(+0.33 %) on the last run, every scenario inside the loaded machine's ±0.5 %
run-to-run noise.

## 2026-09-01 — µPC1252H2: noise floor adopted, nonlinearity rejected

NEC's [1983 consumer-IC data book](https://archive.org/download/bitsavers_necdataBooCircuitsforConsumerUse_42422169/1983_NEC_Integrated_Circuits_for_Consumer_Use.pdf#page=262) (µPC1252H2, p. 257) specifies the part at
Vcc/Vee ±12 V, ISET 2 mA and RIN = ROUT = 33 kΩ, and Roland's
[jack-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15)
installs IC5 in exactly that circuit: C12 10 µF / R36 33 kΩ in, R34 5.6 kΩ +
R35 680 Ω to −15 V for 2.006 mA, pin 8 into IC2b's 33 kΩ I/V, and +15 V
through R17 1.5 kΩ. Two of the table's rows were candidates.

Distortion is rejected. At the derived bus levels (0.3–1.7 Vrms, VCA LEVEL
−16.3..+4.7 dB) the trimmed typical THD is 0.007–0.02 %, −70 dB or better,
and Roland fits no symmetry trimmer (pin 4 is grounded through R33 47 Ω), so
the installed part sits somewhere in an untrimmed curve NEC bounds only as
"≥ 0.05 %" with no typical, sign or shape. That is no sourced magnitude, so
the stage stays linear rather than carry an invented one.

The output-noise floor ships. NV = −94 dBV typical (max −84 dBV) over
10 Hz–20 kHz is a derivable figure under the installed conditions, folded to
a white-equivalent density and added at IC2b's output ahead of the dry/wet
split, scaled by Unit Character like the resistor floors. Rendered, the term
is −109.4 dBFS on the dry leg at every VCA LEVEL byte (−93.3 dBV referred to
IC2b over the 0–24 kHz window, 8 % above the band-limited datasheet figure),
lifting the idle chorus-Off floor from −119.1 to −108.9 dBFS; through the wet
legs it lifts the default-HISS chorus-I idle floor by 0.46 dB, from −98.8 to
−98.3 dBFS. NEC publishes NV only at Av = 0 dB, so the constant
output-referred form is likely slightly high below 0 dB; that gain dependence
and the voice cards' own contribution to the dry floor stay with OQ-16.

This is an evidence-priority decision, not a listening verdict: nothing here
is audible, and no letters were rendered. The old floor remains available
behind the internal `enableCommonVcaNoise` comparison switch.

## 2026-09-01 — Sub half-wave mean on the WAVE node

Module p. 13 at 1200 dpi confirms R102 (R99 on CH2) as Tr19's (Tr16's)
collector load to the SUB LEVEL rail and D6 (D5) as the single series diode
from the R101 (R97) 27 kΩ leg into the WAVE line: the rail's current enters
the node on one half-cycle only, so the sub carries a mean equal to its own AC
amplitude. The model now adopts that unipolar shape and leaves the mean for
C56/C50 to remove; the level law stays linear in the held rail, and the node's
DC-to-AC impedance ratio is voiced at 1, the floor of its ≤ 2 bracket, inside
the already-voiced sub coordinate. Steady state is unchanged; only a SUB level
step now produces the C56/C59-shaped bump the DC-coupled leg makes. This is
explicitly distinct from the removed sub-driver amplitude asymmetry (a
fabricated 0.3 % level inequality). It is an evidence-priority decision, not
a listening verdict, and does not close OQ-15: a sub-level-versus-byte capture
at TP8 would fix the diode onset and the node loading. The former zero-mean
square remains behind the internal `enableSubHalfWaveNodeCoupling` switch.

## 2026-09-01 — Loaded MN3009 reconstruction network

Roland's [jack-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15)
keeps both MN3009 outputs connected through separate 3.3 kOhm legs to the
shared 47 kOhm / 2.2 nF tap. Panasonic's
[MN3009 documentation](https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf)
shows two continuously present output followers, separately loaded ahead of a
balance pot, and its typical Gi-RL slope around the installed 50--100 kOhm
region supports a local Thevenin estimate of about 3.7 kOhm per follower. This
is a graph-derived typical nominal, not a specified or guaranteed Rout.

The former solve treated one output as an ideal source and factorised the tap
from the following reconstruction filter. The nominal model now combines both
finite-source legs, C45/C48, R117/R110, and the first 22 kOhm / 22 kOhm
Sallen-Key section in one continuous nodal system. Tr15--Tr18 remain ideal
followers, matching the existing no-extra-parameter filter model; finite beta,
gm, junction capacitance and bias-dependent output impedance need installed
device data and are not guessed. The prepared transition still has six states
and the realtime path does no additional matrix work.

Compared directly with the former ideal-source, separable implementation, the
loaded network is about 0.36 dB darker at 5 kHz, 0.87 dB at 10 kHz and 1.04 dB
at 15 kHz. DC is normalised to the existing loaded wet coordinate because
Panasonic's insertion-gain row already uses a 100 kOhm load and no original-unit
capture fixes absolute wet level. The HISS-100 recovered-line policy was
therefore remeasured, not ear-tuned: its
fixed-seed A-weighted transfer is 0.38948--0.38953 at 176.4 kHz and
0.38937--0.38941 at 192 kHz, represented by 0.3894. OQ-04 remains open for the
installed spread, follower loading, absolute gain and a wet-only hardware
sweep.

## 2026-08-31 — I+II preserves the wet mid, not the stereo side

The first I+II product implementation reused the normal two-line anti-phase
stereo output and changed only modulation rate. An original Juno-106 owner
reported that the physical both-button result was instead conspicuously narrow,
nearly mono, with a characteristic colour. That directly falsifies the wide
continuation but does not calibrate a fractional width. The corrected mode
therefore uses the only zero-parameter narrow continuation: equal-fold the two
existing wet returns to their mid. This preserves the former path's exact mono
sum and its comb colour, removes only the unsupported side, and leaves I and II
unchanged. The old wide result remains available internally for matched A/B;
an identified-unit stereo capture is still required to establish any residual
width or a different both-button clock law.

## 2026-08-31 — NOISE OTA drives C41

Roland's [module-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=13)
settles the order of one existing circuit: Tr21 crosses C42 into the BA662
level OTA, and C41/R79 loads that OTA's output. The former model ran both poles
at full level and multiplied the scanned NOISE hold afterwards in every voice.
That is equivalent at a fixed level, but not during a move: it kept C41 fully
excited behind Noise zero and exposed that unrelated stored charge when the
control rose.

The scanned level now drives the existing C41 state. C42 and the noise source
continue running while muted, C41 discharges and refills through its existing
4822.877 Hz pole, and no new pole, reset, click, leakage or BA662 transfer is
invented. The post-C41 path remains as an internal comparison switch. In a
matched eight-transition A/B, the difference is −17.5 dBc peak and −54.2 dBc
RMS, both referenced to the legacy take's whole-take RMS; the settled spectrum
and gain are unchanged. On coarse HQ-off grids below about 19.3 kHz internal,
the physical 33 us memory is shorter than one sample and the existing bilinear
C41 pole is negative; those grids retain the qualified fixed-level filter but
apply its level afterwards, avoiding a nonphysical alternating mute tail. A
native Apple M1 Max Release benchmark at 48 kHz/4×
(256-frame blocks, 1024 timed blocks, 13 alternating repetitions,
Poly/Cubic/RK4) changed median thread CPU by less than 1% in both the idle and
six-noise-voice cases, negligible against the standing CPU budget.

## 2026-08-31 — Pulse-Off WAVE node and evidence boundaries

The service drawings settle one previously provisional behavior: Pulse Off's
−0.8 V control pins the MC5534A comparator high, but does not disconnect its
output from the fixed WAVE node. The model now leaves that constant level on
the node and lets the existing C56/C50 coupling state reject its DC. Startup
primes the capacitor to the settled pulse mean so a restored patch does not
manufacture a power-on thump. The former hard gate remains only as an internal
A/B switch; the matched comparison measures −16.2 dBc diff RMS and +6.2 dBc
diff peak around the off/on transitions. Exact installed node level, loading
and residual switching waveform remain OQ-15/OQ-11.

The same evidence pass corrected the MN3009 noise claim below. Panasonic's
0.15/0.20 mVrms figures are conflicting *maximum* rows at the part output under
fixed test conditions. The former 59.716 uVrms inference combines an input
swing with a maximum-output S/N figure and is invalid. HISS 100% and the
29.858% default retain their numerical values as explicit product/session
policy, while regressions now test the fixed-condition raw-node upper bound
separately from the recovered wet-line normalization.

A repeatable local Roland Cloud JUNO-106 v2.0.2 comparison found median chorus
rates of 0.542613 Hz (estimator range 0.542609–0.542617) and 0.880013 Hz
(0.880011–0.880016) across 48/96 kHz captures and analysis windows. Their median
1.62181 ratio is within 0.103% of the schematic-derived 1.6234799 ratio,
corroborating the topology, but the absolute rates are about 2% slower and the
model is not an original-unit measurement; production constants therefore stay
unchanged. The reference exposes no I+II state and the authorization-limited
run could not identify delay endpoints.

BA662 signal nonlinearity/noise/thump and converter charge injection remain
unimplemented: available sources settle topology and nominal time constants,
not the original hybrid transfer or installed transient magnitude. A bypassed
A/B of guessed coloration would show only that the guess is audible.

After adding comparator/C56 tracking to idle fast-mode cards, the native
48 kHz/4× paired benchmark still measures the shipped Poly/Cubic/RK4 path at
3.115× Exact/Merson on the six-voice full-mixer Chorus-II case and 11.458×
while idle (about 67.9% and 91.3% less CPU). The fidelity correction therefore
retains the standing greater-than-10% CPU-saving acceptance floor.

## 2026-08-29 — C14 voltage coefficient withdrawn from the default

The optional C14 capacitance modulation remains available to the isolated
comparison renderer but no longer ships enabled. Its `0.15` coefficient had no
installed-part measurement, and the implementation drove it from the complete
bus voltage rather than the voltage across C14. More importantly, current
[aluminum-electrolytic manufacturer guidance](https://www.chemi-con.co.jp/en/faq/detail.php?id=alBiasVoltageChara)
states that voltage bias does not change this capacitor class's capacitance.
That general guidance is not a measurement of Roland's 1980s 10 uF non-polar
part under AC, so it does not prove the real distortion is zero; it does make
removing the guessed law the evidence-conservative default. OQ-21 remains open
for a level-swept transfer/THD or direct voltage-across-C14 capacitance
measurement, plus a switching capture. The internal comparison flag is not
serialized, so this correction also changes restored sessions at nonzero Unit
Character; preserving an unsupported default would be the less faithful
compatibility choice.

## 2026-08-28 — Fidelity-first quality default

New instances now request the deepest 4× QUALITY rung. The DCO passes the
project's numerical gates at every rung, but the BBD and VCF domains pass their
absolute gates only at 4× for common host rates; keeping 1× as the default was
therefore a CPU-first product choice at odds with the fidelity goal. Existing
sessions retain their stored choice, 1× and 2× remain available, and the
request is still capped against the host rate, so sufficiently high-rate hosts
do no redundant work.

## 2026-08-28 — Resonance control law

The circuit-derived linear-above-junction profile is now the default. The
module drawing traces the resonance CV through a grounded-base stage directly
into the BA662 control input, whose transconductance is linear in control
current; that is stronger physical evidence than the legacy quadratic-then-
linear compatibility voicing. Both retain the same service-calibrated maximum,
input compensation and self-oscillation correction, so this changes only the
intermediate slider response and adds no DSP work.

This is an evidence-priority decision under the realism/CPU goal, not a
listening verdict and not a closure of OQ-09: the 0.6 V junction onset and
0.2296 compensation coefficient still await a measured response family. The
legacy voiced curve remains available behind the internal comparison switch.

## 2026-08-28 — MN3009 transfer and default floor (noise rationale superseded)

The chorus write transfer now distinguishes the MN3009's guaranteed input-swing
limit from its typical distortion curve. It retains the existing 2.924 V rail,
fits 0.3% THD at 0.78 Vrms and approximately 2% at 2.0 Vrms, and remains below
the 2.5% guarantee at 1.5 Vrms. The prepared curve and slope changed, but the
realtime path is still the same 512-interval Hermite lookup with the same cost.

This revision initially treated 29.858% as a 59.716 uVrms inference from the
1.5 Vrms input-swing and 88 dB maximum-output S/N rows. The 2026-08-31 audit
above found those measurands incompatible and supersedes that rationale. HISS
100% and the default keep their numerical values only as explicit product and
session-compatibility policy; OQ-03 remains open for an identified, calibrated
instrument's absolute PSD.

A native Release benchmark against the pre-change tree measured the worst 1x
scenario at +0.4% CPU (the others ranged from -0.04% to +0.2%), comfortably
inside the goal's +20% ceiling.

## 2026-08-23 — VCF solver ladder

`VCF Solver` descends Max / High / Normal — Merson ×2, RK4 ×2, RK4 ×1, so
10/8/4 right-hand-side evaluations — every rung on the same seven control
nodes, so no rung moves a control node or a hold trajectory. The default
rung's error against an independent 96-substep reference is −162.551 dB
(4.2e-8 V peak).

**Verdict, by ear:** a blind four-letter set returned no audible difference
between rungs, so instances ship on **Normal**, roughly half the filter's CPU.
`EngineParameters` stays on Merson as the reference kernel that every frozen
fingerprint tests.

## 2026-08-24 — CPU defaults

The `Poly` tanh kernel and `Cubic` Fast Early form, plus the freewheel wake
that lets silent voice cards and a settled, switched-off chorus skip work.
Numerical evidence: a six-voice chord nulls at −89 dB against `Exact`;
self-oscillation anchors are identical to six decimals in amplitude and
0.003 cents in pitch. Sample nulls on resonant material decorrelate by phase
drift and are judged by anchor, as the earlier solver pass established.

The default flip and the freewheel wake are an audible-impact question the
measurements cannot close, so a four-take A/B set was rendered — A the
shipping configuration, B the new defaults; retrigger-after-silence, resonant
lead, self-oscillation, chorus engage; RMS-matched within 0.001 dB, no trims —
with its `key.md` unread by design.

**Verdict, by ear:** the player could not tell A from B on any take, so the
new defaults stand. Chosen by ear, not settled by a measurement.
`VCF Tanh = Exact` remains the one-menu revert.

## Pending

- **Vref = 0.775 V (OQ-06).** Roland's era convention, recorded as the
  standing candidate. Adoption is a product decision, not a listening
  question.
- **Chart-geometry follow-on (OQ-08).** If `MeasuredChartGeometry` and the
  normalised profile prove indistinguishable by ear, OQ-08's audible-impact
  priority drops. No verdict yet.

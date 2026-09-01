# YouKnow106 — decision log

Model directions and listening verdicts. A choice made by ear is recorded as
made by ear, never written up as though a measurement had settled it, and none
of these closes an open question — the captures named under
[known gaps](../README.md#known-gaps) are still what would.

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

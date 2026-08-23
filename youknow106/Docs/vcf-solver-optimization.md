# VCF solver-tableau research note

Status: `Merson x2` remains the reference and the default, and is bit-identical
to every earlier release. `RK4 x2` and `RK4 x1` are the two cheaper rungs behind
the persisted `VCF Solver` choice. Measured error against an independent
96-substep reference solve, the false-limit-cycle onset that bounds each rung,
the self-oscillation service anchor and paired whole-engine CPU are recorded
below. Cross-platform ranking and the player's hearing judgment remain pending.
No project result is inferred from library papers.

This note is the sibling of
[`vcf-tanh-optimization.md`](vcf-tanh-optimization.md), which owns the
*nonlinearity* the solver evaluates. This one owns the *tableau* that decides
how often it is evaluated. The two are independent selectors and compose.

## Why the tableau is the remaining lever

The `tanh` note establishes where the time goes: on a native Apple M1 Max at
48 kHz / 4x, the separately symbolized VCF derivative is `58.322–60.455%` of
top-of-stack samples, and a current-binary disassembly bounds the
derivative-plus-Merson numerical core at `68.433–70.861%`. That note then spent
its effort on making each *call* cheaper.

The number of calls had never been examined. YouKnow106 advances the four OTA
capacitor states with two fixed half-interval five-stage Merson steps: **ten
right-hand-side evaluations per card per internal sample**, unconditionally,
whatever step size the interval actually presents. Six physically powered cards
at 48 kHz / 4x is 11.52 million right-hand-side evaluations per second.

Merson's five stages produce a fourth-order solution *plus* an error estimate.
The estimate is never used here — the step is fixed — so one evaluation in
every half-step, two of the ten per interval, buys a quantity nothing reads.
Classic RK4 reaches the same fourth order in four stages, which is where the
`RK4 x2` rung's 20% comes from. The larger saving comes from the step size
rather than the tableau: at the step sizes musical settings actually produce,
one full-interval RK4 step is already far past the point where extra accuracy
is representable.

The measured step sizes make this concrete. The normalized step is

```
z = omegaStep * maxStageScale * earlyCeiling * F(feedback)
```

where `omegaStep = 2*pi*fc / internalRate`, `maxStageScale` is the largest of
the four integrating-capacitor tolerances, `earlyCeiling` bounds the Early
effect's stage-rate multiplier, and `F` is the cascade's own closed-loop
spectral factor (below). A 2 kHz cutoff at a 192 kHz internal rate, with the
loop gain of 3.55 that sits just under the cascade's oscillation threshold,
gives `z = 0.147`. Measured against an independent 96-substep reference solve
under controls an order of magnitude harder than that — an 11 kHz cutoff swept
±4.8 kHz — the default rung's relative error energy is `−162.551 dB`, with a
peak of `4.2e-8 V`. A `float` output has about `1e-7` of resolution at that
node.

## The three tableaux

Every rung's abscissae are drawn from the **same seven control-node positions**
the engine already reconstructs the input and the converter-hold trajectory at,
`{0, 1/6, 1/4, 1/2, 2/3, 3/4, 1}`. That is what makes the ladder cheap to add
and impossible to get subtly wrong: no rung moves a control node, changes the
hold trajectory a converter write produces, or needs a second reconstruction
grid.

| Tableau | Steps | Abscissae (interval coordinates) | RHS evaluations | Input reconstructions |
|---|---|---|---:|---:|
| `MersonHalf` | 2 × Merson | `0, 1/6, 1/6, 1/4, 1/2` \| `1/2, 2/3, 2/3, 3/4, 1` | 10 | 7 |
| `Rk4Half` | 2 × classic RK4 | `0, 1/4, 1/4, 1/2` \| `1/2, 3/4, 3/4, 1` | 8 | 5 |
| `Rk4Full` | 1 × classic RK4 | `0, 1/2, 1/2, 1` | 4 | 3 |

A one-step *Merson* tableau was not added: its `1/3` abscissa is not in the
node set, so it would have needed an eighth control node and a wider hold
trajectory for one evaluation's saving over `Rk4Full`.

`OtaCascade::tableauNodeMask` names which of the seven ordinals each tableau
reads; the reconstruction loop, the control interpolation and the per-stage
omega product all skip the rest. That is not an approximation — the tableau
never evaluates there, so those ordinals have no reader.

## The two bounds, and why Merson stays underneath

A mode names the *cheapest* tableau it is willing to use. `planTableau`
escalates per interval, so a rung is a cost ceiling, not a promise about a
particular interval.

### The cascade's own spectral factor

Four identical one-poles closed through a gain `k` put the loop roots at

```
s/w = -1 + k^(1/4) * exp(i*(pi + 2*pi*m)/4),  m = 0..3
```

whose largest magnitude is `sqrt((1+q)^2 + q^2)` with `q = k^(1/4)/sqrt(2)`.
It is exactly 1 with the loop open and `2.491353317928156` at the sanitized
feedback ceiling of eight — where `q` reduces to exactly `2^(1/4)`.
`planTableau` first tries the step against that ceiling, which is admissible at
any resonance, so the common case never evaluates the factor at all.

### `singleStepRk4Limit = 1.25` — an accuracy bound

Measured by forcing `Rk4Full` on every interval (escalation raised out of the
way) and comparing against the suite's independent 96-substep RK4 reference
solve over 8192 samples of a two-tone drive, at three internal rates, three
loop gains and nine cutoffs. Relative error energy, in dB, versus the
normalized step:

| `z` | worst `Rk4Full` error |
|---:|---:|
| ≤ 0.30 | −97.5 dB |
| ≤ 0.60 | −112.1 dB |
| ≤ 1.25 | −97.5 dB |
| ≤ 1.82 | −93.3 dB |
| ≤ 2.35 | −87.1 dB |
| ≥ 2.97 | leaves the reference |

The worst case is flat at about `−97.5 dB` from `z = 0.29` up: it occurs at
48 kHz internal, 1 kHz cutoff, `k = 3.55`, and it comes from the **causal input
reconstruction**, not from the step size — three reconstruction nodes bridge
the same signal motion five or seven do. Lowering the limit therefore does not
improve the worst case, which is why 1.25 is set from the departure point
rather than from the error curve: it is less than half of it, and classic RK4's
own stability radius (`−2.7852` on the negative real axis, `2*sqrt(2)` on the
imaginary one) is more than twice it.

### `halfStepRk4Limit = 4.0` — a stability bound

This is the bound that decides whether Merson can be removed from the ladder,
and the answer is that it cannot.

Merson's five-stage stability region reaches `−3.5483` on the negative real
axis and `3.4641` on the imaginary one, where classic RK4's reaches `−2.7852`
and `2.8284`. Both pairs are the linear test equation's own boundaries,
recomputed from the two stability polynomials rather than quoted.

The product grid's own `0.9*pi` omega cap can put the cascade's fastest
closed-loop eigenvalue past the smaller of the two: at the cap, with the
Character-ceiling stage spread and `k = 3.8`, `z = 6.59`, or `3.30` per half
step.

The suite's zero-input cap fixture measures exactly this. It starts the cascade
at `±1e-6`, drives it with silence at the omega cap under all six cards' worst
Character trims cold and warm, and requires the tail to be dead flat below
`1e-8 V`. Merson passes at every tested loop gain below the oscillation
threshold. Classic RK4 sustains a false limit cycle:

| tableau | `k` | last clean `z` | first failing `z` | tail at failure |
|---|---:|---:|---:|---:|
| `MersonHalf` | 3.8 | 6.59 (cap) | — | 0 V |
| `Rk4Half` | 2.0 | 5.32 | 6.01 | 0.538 V |
| `Rk4Half` | 3.8 | 5.83 | 6.59 | 1.181 V |

The onset per half step is between 2.66 and 3.00 — exactly where RK4's region
ends, which is the theory predicting its own failure. `halfStepRk4Limit = 4.0`
is 2.0 per half step, 72% of that radius. Above it, **both** RK4 rungs run
Merson, so the ladder never executes a tableau outside its admissible region
and the reference kernel is the backstop rather than a peer.

(At `k = 8` every rung including Merson sustains a limit cycle. That is a real
self-oscillation — loop gain 8 is twice the cascade's threshold of 4 — not a
numerical artefact, which is why the fixture stops below it.)

## Measured solve accuracy of the shipped ladder

Against the same independent 96-substep RK4 reference, with escalation live, on
a card carrying stage-capacitor spread and stage offsets. This is the gate the
suite now enforces (`testSolverLadderAgainstIndependentReference`):

| Rung | 192 kHz internal (4x) | peak | 48 kHz internal (1x) | peak |
|---|---:|---:|---:|---:|
| `Merson x2` | −162.551 dB | 4.21e-8 V | −141.561 dB | 2.64e-7 V |
| `RK4 x2` | −147.144 dB | 2.53e-7 V | −125.748 dB | 1.68e-6 V |
| `RK4 x1` | −123.106 dB | 3.96e-6 V | −101.615 dB | 2.73e-5 V |

The 1x column is the tighter one for every rung, for the reconstruction reason
above. The worst figure on the ladder is `−101.6 dB` of relative error energy
with a peak of 27 microvolts on a node whose signal runs to several volts.

## The self-oscillation service anchor

Roland's ADJUSTMENT trims resonance against a self-oscillating sine — 4.8 Vp-p
at 248 Hz with C4 held — so the limit cycle's **amplitude and frequency** are
the audible property the ladder must not move. A free-running oscillator's
*phase* is not: two solvers drift apart there without either being wrong, which
is why a plain error-energy metric reads badly in self-oscillation and says
nothing.

Driving the shipping engine through its complete signal path at the service
trim's own converter code, at all three quality factors:

| Kernel | C4 Vp-p | C4 Hz | tracked C6 Hz | octaves |
|---|---:|---:|---:|---:|
| Exact / Merson x2 | 4.3079 | 251.564 | 1018.291 | 2.0172 |
| Exact / RK4 x2 | 4.3079 | 251.564 | 1018.291 | 2.0172 |
| Exact / RK4 x1 | 4.3079 | 251.564 | 1018.291 | 2.0172 |
| Fast / Merson x2 | 4.3079 | 251.564 | 1018.291 | 2.0172 |
| Fast / RK4 x1 | 4.3079 | 251.564 | 1018.291 | 2.0172 |
| Fast + Cubic Early / RK4 x1 | 4.3079 | 251.585 | 1018.291 | 2.0170 |

Amplitude is identical to four decimal places and frequency to about 0.02 Hz —
0.14 cents, against a service procedure that itself accepts ±10 cents. The
isolated-cascade check in the suite reports the same result as an amplitude
ratio and a cent difference, and gates them at 0.1 dB and 1 cent.

## Whole-engine CPU

Release x86-64, GCC 13.3, Intel Xeon at 2.80 GHz, 48 kHz host / 4x, 256-frame
blocks, two seconds of preroll, 128 timed blocks, seven paired repetitions in
alternating order, state snapshots copied outside the timer, per-thread CPU
clock. Produced by `YouKnow106OversamplingAudit --tanh-benchmark`.

The timed window contains 0.682667 seconds of audio, so every figure below is
median per-thread CPU as a multiple of realtime: 1.000× means the engine
consumes one core to keep up. Percentages are the reduction against the
shipping default in the same scenario. Paired medians, minima and MADs for
every pair are in the tool's own output. These are machine-specific
whole-engine measurements on one x86-64 part; the sibling note's Apple M1 Max
figures for the `tanh` selector are not interchangeable with them, and neither
set is a promise for another compiler or libm.

| Kernel | Idle, dry | One voice, plain, dry | Six voices, plain, dry | One voice, resonant, dry | Six voices, resonant, dry | Six voices, full mixer, Chorus II |
|---|---:|---:|---:|---:|---:|---:|
| Exact / Merson x2 *(default)* | 2.103× | 2.054× | 2.035× | 2.418× | 2.411× | 2.208× |
| Exact / RK4 x2 | 1.679× (−20.2%) | 1.698× (−17.3%) | 1.653× (−18.8%) | 1.943× (−19.6%) | 1.948× (−19.2%) | 1.810× (−18.0%) |
| Exact / RK4 x1 | 0.959× (−54.4%) | 0.962× (−53.2%) | 0.960× (−52.8%) | 1.135× (−53.1%) | 1.141× (−52.7%) | 1.828× (−17.2%) |
| Fast / Merson x2 | 1.277× (−39.3%) | 1.280× (−37.7%) | 1.259× (−38.1%) | 1.279× (−47.1%) | 1.269× (−47.4%) | 1.285× (−41.8%) |
| Fast / RK4 x1 | 0.656× (−68.8%) | 0.662× (−67.8%) | 0.650× (−68.1%) | 0.659× (−72.7%) | 0.653× (−72.9%) | 1.059× (−52.0%) |
| Fast + Cubic Early / Merson x2 | 1.117× (−46.9%) | 1.122× (−45.4%) | 1.105× (−45.7%) | 1.113× (−54.0%) | 1.113× (−53.8%) | 1.119× (−49.3%) |
| Fast + Cubic Early / RK4 x1 | 0.574× (−72.7%) | 0.580× (−71.8%) | 0.573× (−71.8%) | 0.579× (−76.1%) | 0.566× (−76.5%) | 0.972× (−56.0%) |

Reading the table:

- **`VCF Solver = RK4 x1` alone takes 52.7–54.4% off the shipping default** in
  the five dry scenarios, with the exact libm `tanh` untouched. That is the
  cheapest 50% available anywhere in this engine, and the only thing it changes
  is which fourth-order tableau advances the same equations.
- **`RK4 x2` takes 17.3–20.2%** for the same fourth order at the same step
  size — the free rung.
- The ladder composes with `VCF Tanh`. Fast plus `RK4 x1` reaches 67.8–72.9%,
  and adding the opt-in Cubic Early transfer reaches 71.8–76.5%.
- The full-mixer Chorus II fixture is the honest exception for the solver
  column: it runs the filter wide open, so `RK4 x1` escalates to the half-step
  pair on nearly every interval and returns 17.2% rather than 53%. The
  escalation is the point — a cost ceiling, not a promise — and the `tanh`
  selector still delivers there.
- Idle and six-voice cost are within 4% of each other at every rung, because
  the six physically powered cards advance whether or not a key is down. See
  the remaining-levers section.

The chorus, DCO, decimation and mixer are untouched, so the floor this ladder
approaches is the non-VCF engine, not zero.

## Blind A/B set

A four-letter set was rendered through the complete shipping signal path at
44.1 kHz with the engine's own 4x request, on three factory fixtures — a
resonant lead (B44), the self-oscillation fixture (A82) and sustained chords
(B88). Identical MIDI, controls, length and preset in every letter; chorus and
the shared noise source off so the filter is exposed; whole-file RMS matched to
letter A, which is always the shipping engine. Under this repository's A–Z
rules it is a decision artefact, so it is not committed: it was rendered to a
working directory and sent to the player, with its key written at the same time
and unread by design.

Its whole-file nulls against the shipping engine are the objective half of that
test, and they bound what the test can show:

| Fixture | RK4 x2 | RK4 x1 | Fast + RK4 x1 |
|---|---:|---:|---:|
| B44 resonant lead | −146.9 dBc | −89.4 dBc | −89.4 dBc |
| A82 self oscillation | −138.3 dBc | −110.2 dBc | −102.6 dBc |
| B88 sustained chords | −146.9 dBc | −87.9 dBc | −87.9 dBc |

`RK4 x2` nulls 138–147 dB down, which is below what a 16-bit file can carry at
all. The cheapest rung nulls 88–110 dB down, at the 16-bit floor. The
self-oscillation fixture is the one where a free-running limit cycle's phase
could have drifted; it nulls *deeper* than the two played fixtures, not
shallower.

`RK4 x1` and `Fast + RK4 x1` null identically on two of the three fixtures,
which is consistent with the two error sources' sizes: the ZonedHermite
transfer error is `1.16e-8` where the single-step solve error is around `1e-5`,
so the solver dominates and the `tanh` choice adds nothing measurable on top of
it.

## What was rejected

| Candidate | Shape | Conclusion |
|---|---|---|
| Kutta third-order, one step | Three evaluations at abscissae `0, 1/2, 1`, all in the node set. | Rejected. It saves one evaluation over `Rk4Full` and costs 30–45 dB of solve accuracy: `−64.7 dB` relative error at an ordinary 1 kHz / resonant / 1x point where `Rk4Full` holds `−97.5 dB`, and it loses the self-oscillating limit cycle outright at 11 kHz / `k = 4.50`. A 25% saving on the cheapest rung is not worth an audible-order error. |
| One-step Merson | Abscissae `0, 1/3, 1/3, 1/2, 1`. | Not implemented. The `1/3` node is not in the shared set, so it would need an eighth control node and a wider hold trajectory to save one evaluation against `Rk4Full`. |
| Bogacki–Shampine RK3(2) | Four stages, FSAL, so three effective evaluations; abscissae `0, 1/2, 3/4, 1` are all in the node set. | Not implemented. Better error constant than Kutta's RK3 but still third order, for one evaluation against `Rk4Full`. |
| Single-tier escalation (RK4 only) | `Rk4Single` escalating to `Rk4Half` and stopping there. | Rejected by measurement: it left `Rk4Half` running past classic RK4's stability radius at the product grid cap and produced the false limit cycle tabulated above. The Merson fallback tier exists because of this result. |

## Configuration

`QUALITY` and `VCF SOLVER` are independent. QUALITY moves the internal rate,
which every domain shares and which carries latency and an idle-window
requirement; VCF SOLVER moves only the filter's numerical solve, takes effect
immediately and adds no latency. Neither is part of a patch, a Program Change,
a SysEx dump or randomization; both persist with the session.

Lowering VCF SOLVER first is the cheaper trade of the two: it leaves the
oscillators' bandlimiting, the chorus and the decimation exactly where they
were.

Switching it under a sounding note is continuous. Every rung shares the same
capacitor state, the same causal input history and the same parameter history,
so the only discontinuity a switch can introduce is the difference between two
solves of one interval; `testSolverRungMayBeSwitchedUnderASoundingNote` bounds
that against the signal's own largest sample-to-sample step. A solver-only
parameter change also misses every guarded rebuild in `setParameters` — no card
trim, thermal scale or hold is recomputed — so it costs nothing to move.

## Remaining levers, not taken here

- **Poly9 inner-zone `tanh`.** Already researched and coefficient-fixed in the
  sibling note, with `7.26e-6` maximum transfer error and a measured 13.465%
  median whole-engine reduction over Fast-Hermite on M1 Max. Adding it would
  mean a third ordinal on the existing two-choice `VCF Tanh` parameter, which
  changes that parameter's discrete range and its saved normalized values; that
  is a state-compatibility decision, not a numerical one.
- **Idle voice cards — measured, and much larger than the ladder.** The six
  hardware cards are powered and advance behind closed VCAs, exactly as the
  instrument does, so an idle instrument costs almost what a six-voice chord
  costs: on the reference machine `idle-dry` and `six-voice-plain-dry` are
  within 1% of each other at every rung. `renderVoice` computes a silent card's
  DCO, filter and couplings and then discards the result (`return 0.0f`),
  keeping only the state.

  An experimental build that skips a powered-but-silent card's audio entirely
  was measured with the same protocol, purely to size the opportunity:

  | Fixture | shipping `exact-merson` | idle cards skipped | reduction |
  |---|---:|---:|---:|
  | Idle, dry | 2.022× realtime | 0.109× | **94.6%** |
  | One voice, plain, dry | 2.037× | 0.423× | **79.3%** |
  | Six voices, plain, dry | 2.016× | 2.052× | 0% (nothing is idle) |

  That is the single largest remaining saving in any session that is not
  holding six notes, and it dwarfs the solver ladder. It is also *not* free,
  and the two mechanisms that make it audible are specific:

  1. **Free-running DCO phase.** A note-on with a *changed* pitch already
     restarts the timer (`dcoResetPending` → `restartDcoBandlimited`), so most
     note-ons discard the phase anyway. A same-pitch retrigger on the same card
     does not: the card keeps free-running, and successive attacks therefore
     start at different ramp phases, as the hardware's do. Sleeping the card
     and reconstructing it from silence would phase-lock every repeat to zero.
     There is a cheap fix — the pitch CV of a sleeping card is static, so its
     phase can be fast-forwarded analytically on wake, in O(1), instead of
     reset — but that is a design, not a measurement.
  2. **Filter settling at note-on.** The cascade would start from zero rather
     than from the steady state the old note left it in. The voice VCA opens
     from zero over the attack, which masks most of it, but a low cutoff has a
     long enough time constant for it not to be masked entirely.

  The extension slots above the six cards already do exactly this
  (`silenceVoice` reconstructs them from silence), so the mechanism exists and
  is already considered acceptable there. What it needs before it can be
  offered on the physical cards is the phase fast-forward above and a blind A/B
  on note-on transients — repeated same-pitch attacks and a slow low-cutoff
  attack are the two fixtures that would expose it — under this repository's
  A–Z rules. That A/B has not been run, so nothing here is implemented.

## Primary background sources

- R. H. Merson, *An operational method for the study of integration processes*,
  Proc. Symp. Data Processing, Weapons Research Establishment, Salisbury, 1957
  — the five-stage tableau the default rung uses.
- J. C. Butcher, *Numerical Methods for Ordinary Differential Equations*, Wiley
  — stability regions and order conditions for the explicit tableaux compared
  here.
- E. Hairer, S. P. Nørsett and G. Wanner, *Solving Ordinary Differential
  Equations I: Nonstiff Problems*, Springer — the linear stability boundaries
  quoted for classic RK4 and for Merson.

The stability radii above are used as published boundaries for the linear test
equation; every admission decision in this note is nonetheless made against the
project's own measured fixtures, because the cascade is nonlinear and its
`tanh` saturation is part of what bounds it.

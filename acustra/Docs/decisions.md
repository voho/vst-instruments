# Acustra — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear,
never written up as though a measurement had settled it.

## 2026-08-30 — initial identity failure

In an informal, unblinded listen to the first implementation, the user reported
that it did not sound bad but did not sound like a guitar; the closest identities
were piano, harpsichord or lyre. This observation licensed correcting the attack
and body identity failure. It did not select fitted constants or establish
measurement accuracy, market rank or acceptance of the replacement.
The corrected bridge-force, dispersive-string and individual-guitar body build
was then heard by the user as piano, harpsichord or lyre rather than guitar. That
is an explicit rejection of the synthesized source core, not a request for a
different EQ curve.

## 2026-08-30 — recorded sustain selected for the next audition

A controlled attack/sustain splice showed that a real attack followed by the
existing synthesized sustain still inherited the keyboard-like identity, while
the complementary splice retained guitar identity. Separate CC0 source oracles
then moved both steel and nylon isolated-note/strum classifiers away from piano
and toward acoustic guitar. The next user-audition build therefore uses the real
recording for the complete audible note and does not mix the rejected waveguide
into its output.

This is an engineering selection, not a user acceptance verdict or a
top-of-market claim. The replacement remains pending the user's blinded listen.

## 2026-08-30 — integrated correction ready for blind listening

The final integrated render uses the same MIDI, controls, seed, sample rate and
block partition as the rejected baseline. Whole-file RMS differs by only
0.000206 dB for the steel pair and 0.000253 dB for the nylon pair after 16-bit
encoding. The letter mapping is kept in a separate key.

As a weak regression check, PANNs moved the steel acoustic-guitar-versus-largest-
keyboard/harp-confuser margin from -0.02197 to +0.08651, and the nylon margin
from +0.10355 to +0.53503. These final scores use the delivered, level-matched
WAVs and include AudioSet's general keyboard label in the confuser set. This
classifier result is not a listening verdict.
The corrected build still requires the user's blind comparison before its
guitar identity can be considered accepted.

## 2026-08-30 — multi-dynamic steel candidate after continued identity feedback

The user again described the current sound as piano, harpsichord or lyre rather
than guitar. The next candidate therefore changes one mechanism only: the
hard-touch steel sustain bank is replaced by 272 CC0 Shinyguitar microphone
recordings (17 roots, four captured velocity layers and four round robins).
The previous render remains frozen as A; the new render is B. MIDI, controls,
seed, sample rate, block size and duration are fixed, and delivered whole-file
stereo RMS differs by 0.000257 dB for the range and 0.000243 dB for the chord.

The weak PANNs alarm is mixed: on the chord, the general Guitar-versus-confuser
margin moves from 0.358 to 0.501, but the isolated-note Acoustic-guitar margin
moves from 0.089 to 0.071. Neither result substitutes for the pending blind
listen. The source is a miked archtop rather than a matched flat-top guitar, so
even acceptance would not establish a top-of-market claim.

## 2026-08-30 — forensic correction and upper-register blind candidate

An exact source/runtime audit found no leakage from the rejected physical
model, no broad pitch/decay/splice/alias failure, and no clipping. It did find
four objective playback defects, now corrected before further listening:

- the former 0.75 ms immediate fade removed up to 7.24 dB of first-millisecond
  energy; an exhaustive source-derived eight-frame ramp now loses at most
  0.864 dB and has worst attack-derivative ratio 0.946;
- the former zero-centred output knee altered ordinary notes; output is now
  exact-linear through −1 dBFS and only then enters a bounded safety limiter,
  while demos render at the public default gain;
- Touch formerly hard-switched between unmatched Eastman and Shiny sessions;
  steel now stays on one recording family and Touch changes tone continuously;
- velocity/RR calibration now uses deterministic one-second per-zone trims and
  a continuous MIDI curve. Exhaustive tests find no decreasing layer boundary
  and a worst round-robin spread of 0.605 dB.

The dominant remaining confuser is corpus topology. All sixteen captured C6
takes put 93.8–97.9% of early energy in H1, and the A5→B5 key step became both
5.47 dB louder and 867 cents darker in attack centroid. A new one-mechanism
blind pair therefore compares the corrected baseline with a tactical mapping
that extends the brighter A5 source through B5/C6. The cost is a two-to-three-
semitone stretch, so it remains an audition candidate, not an accepted fix or
a replacement for chromatic per-string flat-top recordings.

The pair is stored at `/tmp/acustra-user-listening-v7/upper-range`; its letter
key remains separate. Whole-file stereo RMS differs by only
0.000000045 dB after PCM16 encoding. SHA-256: A
`fa76dc06603ef0be6e07c104a377ff78f4c24c407b2032c91b433a9390fd3c4d`, B
`3a05763be0e54018734b7a6edcf586bc0bde98a5d369435b8b4b87b29d7ac503`.
An independent read-only audit confirms identical 9.500 s stereo PCM16 formats,
no clipped samples, and the exact RMS delta above. Without revealing letters,
the remap moves the last B5 0–60 ms power centroid from 1089.8 to 1777.5 Hz
and reduces its 0–250 ms H1 share from 97.20% to 81.27%.
Native tests pass 3/3 and the JUCE VST3/AU/Standalone build passes 4/4. The
ASan/UBSan also pass 3/3 with Apple-unsupported leak detection disabled. The
listener verdict is pending; committed demos and release packages remain
unchanged.

## 2026-08-30 — upper-register remap rejected

The user found the pair hard to distinguish but preferred B by roughly 5%.
The hidden key identified A as the A5-to-B5/C6 remap and B as the unchanged C6
baseline, so the remap is rejected and the original mapping remains shipping.
The small preference does not validate the C6 recording; it says only that
pitch-stretching A5 is not the fix. A matched chromatic corpus remains required.
The five public demos were regenerated from the retained engine after this
verdict; the rejected remap was not promoted.

This pair accidentally reversed the repository convention that A must be the
shipping engine. The audio comparison itself remained controlled and blind,
but future pairs must keep A as the unchanged build.

## 2026-08-30 — softer steel layer establishes the useful direction

The next controlled pair restored the convention: A was the unchanged engine,
whose loud demo velocities selected Shinyguitar's fourth captured layer; B
changed only steel source selection by capping the lookup velocity at MIDI 64,
while the original velocity still controlled output gain. The user described A
as harsher and more aggressive, B as somewhat dull, and asked for a result
between them. The layer-2 cap is therefore not promoted, but the comparison
establishes that a softer captured performance improves the relevant direction.

The next bounded candidate uses the corpus's actual third captured layer at
high velocities rather than averaging waveforms or inventing an EQ midpoint.
That preserves a real pick transient and changes the same single mechanism.

## 2026-08-30 — middle steel layer also rejected

The controlled middle-layer pair compared the unchanged fourth-layer baseline
with the corpus's genuine third captured layer at the same MIDI velocities.
The user heard little difference. This is a tie, so the layer-3 cap is not
promoted and the shipping engine remains unchanged.

Together, the layer-2 result (audibly dull) and layer-3 tie show that selecting
among this archtop corpus's existing dynamics cannot deliver the missing
flat-top identity. Further layer caps, blends or EQ midpoint auditions are
closed. The next identity experiment must use a matched flat-top capture, not
another transformation of the current source family.

## 2026-08-30 — physical-only runtime and measurement-informed fit

The user explicitly replaced the recorded-note architecture with a physically
modelled instrument. `AcustraEngine` is now the complete audible source: six
stiff-string waveguides feed a 26-mode passive measured bridge,
idle strings react one-way into sympathetic radiation, and a 96-mode measured
stereo body radiates the result. The 321 recorded regions moved behind the
separate `AcustraReferenceBank` target. Only offline fit/reference tools link
that archive; the plug-in links the small physical `AcustraDSP` target and
contains no recorded-note playback path.

The retained realtime topology follows the useful parts of the DAFx-26
measurement-informed nonlinear guitar model and differentiable Karplus--Strong
sound-matching work without claiming their unimplemented parts. Acustra does
not yet solve a geometrically exact nonlinear string or fret/finger collisions.
Its recordings are immutable calibration targets, never runtime oscillators or
convolution responses.

A deterministic fitter compared the shipping physical render with 64 training
and 24 disjoint held-out recording examples. Its robust objective covers onset
and multi-scale attack spectra, H1--H12 envelopes, tuning/inharmonicity,
partial/band T60, body spectrum and velocity dynamics. Two short staged bounded
least-squares passes reduced the training score from 9.599960 to 8.218244
(14.39%) and the held-out score from 8.954658 to 7.808713 (12.80%). Every held-
out term family improved. The complete compact result is recorded in
`Docs/physical-fit-report.json`.

An ablation froze the other 18 fitted values and tested the optimizer's proposed
bridge-local direct gain. It improved training by only 0.000533 and made held-
out score 0.000773 worse, so that flat direction is fixed off. Runtime sound is
therefore the measured body-radiation path, not a raw periodic string cue.

Promotion also exposed a masked host-rate defect. The 48 kHz body residues had
used a real `48000/Fs` approximation; exact complex zero-order-hold state
conversion now preserves both residue gain and phase, and the redundant
empirical derivative multiplier was removed. With the direct path forced off,
MIDI 83 body RMS is 0.011871/0.011826/0.011894/0.011872 at
48/96/192/384 kHz. The fitted descriptor result is objective evidence of a
better match, not a listener-acceptance verdict or a top-of-market claim.

## 2026-08-30 — sympathetic-string strength retained

A controlled engine ablation now gates only the idle-string reaction-force sum,
without changing the active string, bridge state or body source. For E3, whose
second harmonic aligns with the idle low E, the 0.30--4.00 s on/off difference
is 0.766456 of the full tail RMS and raises tail RMS by 7.432 dB. The adjacent
F3 case changes by 0.171538, giving 4.468 times resonant selectivity. The
existing unity coupling is therefore already prominent and musically selective;
no arbitrary sympathy gain or default-timbre change was added.

## 2026-08-30 — continuation fit and direct path

Re-scoring the current renderer exposed that the first compact report no longer
exactly described the promoted DSP: its starting scores were 8.230925 training
and 7.811690 held out. One unchanged continuation pass from the shipping vector
reduced them to 7.774348 and 7.591144. The 64/24 split, targets and manifests
were unchanged; direct gain remained fixed at zero.

After the later dispersion and aperture changes, the direct-path sweep was
repeated. Training selected the maximum tested gain, 0.12, but its improvement
was only 0.001377 and held-out score worsened from 7.480240 to 7.480433. The
direct branch therefore remains off.

## 2026-08-30 — arbitrary 2-D bridge rotation rejected

An audit found that the orthogonal string loop has no audible path through the
shipping scalar bridge. A passive rank-one 2-D diagnostic rotated both string
axes into that measured bridge eigenchannel. It was reciprocal, exactly matched
the original topology at zero degrees and made the second axis causal at
nonzero angles.

The training-only early/late H1--H6 decay sweep selected 30 degrees, improving
its equal-material error from 1.03951 to 1.01630. Held out, however, error
worsened from 0.87005 to 0.88792: steel early decay regressed from 0.57527 to
0.68937 while only its late tail improved. The approximation was rejected and
all angle plumbing removed. Acustra will not claim a radiating second axis
until it has a measured passive 2-D bridge-admittance matrix.

## 2026-08-30 — dispersion collocation error reduced

The higher fitted steel stiffness moved one 44.1 kHz/MIDI-84 partial 0.003742
cents beyond the existing 1.5-cent regression gate. This was the interior
ripple of the one-biquad phase approximation, not a stiffness or numerical
failure. Moving the upper collocation point half a partial inward, from
H1/H7/H12 to H1/H7/H11.5, reduced the full tested matrix maximum to 1.208851
cents without changing the physical stiffness or loosening the test.

## 2026-08-30 — register-dependent pluck aperture promoted

Residuals showed the same missing register slope in training and held-out
recordings: low notes needed a broader effective contact and high notes a
narrower one. A single shared exponent now changes the existing finite pluck
aperture around a MIDI-61 pivot before the bridge; it adds no filter, body EQ or
audio-rate work. Exponent one is the exact previous formula.

A training-only sweep selected exponent zero with nylon/steel aperture scales
1.67/0.65. Against the exact exponent-one baseline, training improved from
7.821766 to 7.676669 and held out from 7.629464 to 7.480240. Five of six held-
out term families improved; decay changed from 4.749707 to 4.779525. Relative
to the current unity calibration, the final physical fit improves training by
20.07% and held out by 16.69%, with all six term families better than unity.

## 2026-08-30 — residual-driven register and low-body continuation promoted

A signed residual audit found the same remaining register error in both
materials and both splits: low notes were too bright and high notes too dark.
The existing aperture exponent was already at its lower bound. A training-only
grid extended that physical law through zero and selected -0.5; a joint bounded
continuation fit settled at -0.412889466.

The largest static family error was the measured body's low-order modal
balance. Training-only sweeps broadened the shared body Q and raised only the
two existing measured modes from 85 to 145 Hz. Joint refitting settled at a Q
scale of 0.123662963 and low-mode gain of 26.0381959. Post-fit ablations of the
old Q and unity low-mode gain each worsened training and held-out scores, so
both changes remain. This is a correction to measured modal residues, not a
note-dependent output EQ or recorded layer.

Under the then-current six-term objective, the complete fit scored 7.383556 on
training and 7.198275 on held out, 23.12% and 19.83% below the unity
calibration. A fresh direct-path sweep chose zero on training. At that point
the calibration had 21 stored values, 20 active; direct gain remained fixed
off. These historical scores are superseded by the seven-term evaluation
recorded below.

The report's former target digest could not be reproduced from its 88 exported
files under any documented raw-content convention. Every target was bytewise
identical to the previous fit directory, and both manifest hashes remained
exact. The report now names and records a reproducible scheme: SHA-256 over
each sorted basename, a NUL byte and its raw target bytes.

## 2026-08-30 — alternate steel tunings preserve string construction

The previous steel path inferred linear mass from each selected tuning while
retaining published standard-tuning tension. That made Drop D behave like a
heavier replacement string. Steel linear mass is now derived once from the
published standard pitch and tension; alternate tunings change tension,
characteristic impedance and inharmonicity together. Standard tuning retains
the original published-tension path. A regression test checks the exact Drop-D
frequency, tension and impedance ratios.

## 2026-08-30 — controller reset completed; measured 2-D bridge still required

MIDI CC121 now releases sustain and recentres pitch bend without stopping held
notes. CC120 remains the hard panic and CC123 remains sustain-aware.

Current competitor and primary-research review confirms that coupled horizontal
and vertical string motion is an important guitar decay cue. Acustra's second
loop remains inaudible with the scalar bridge and zero direct gain. The earlier
authored rotation failed held-out decay, so it stays rejected: the next valid
step is a measured passive reciprocal 2-D bridge-admittance matrix for both a
classical nylon and a flat-top steel reference, not another arbitrary mixing
angle.

## 2026-08-30 — early pitch trajectory added to the promotion gate

The physical score now has a seventh term for the common H1--H8 pitch shift in
20--100, 80--200 and 180--400 ms windows relative to settled partials. Robust
partial rejection, a fixed missing-data residual layout and a synthetic glide
self-test prevent this term from rewarding dropouts or changing objective
dimension. Its weight is 0.03, separated from the settled tuning term rather
than diluting it. The frozen corpus exposes 264/264 finite target/model
trajectory values.

The neutral baseline used from this stage onward is explicitly reproducible:
old unity scales,
direct branch off, velocity-depth zero, aperture exponent one, low-body gain
one, steel nonlinear displacement off and the legacy +0.018 steel fret-decay
slope. It scores 9.472211282 on training and 8.841060861 on development
validation. At this pre-upper-loss stage, the 23-value calibration, of which
22 values were active,
scores 7.193228995 and 6.990990829: improvements of 24.0597% and 20.9259%.
Those stage scores are superseded below. This remains corpus evidence, not a
listening verdict or market-rank claim.

## 2026-08-30 — steel energy pitch surrogate promoted; nylon candidate rejected

A bounded Kirchhoff--Carrier surrogate derives steel attack sharpening from
the waveguide's displacement-slope energy and the string's axial rigidity. A
training-only material sweep selected a 0.0061 m displacement scale. In the
pre-upper-loss model, against the prior authored-pitch-plus-fitted-decay
selection baseline, it improved the
aggregate training total/pitch terms from 7.197568718/0.735603376 to
7.193228995/0.698436326 and development validation from
6.995742795/0.204759878 to 6.990990829/0.179839002. Turning the steel mechanism
off worsened those totals to 7.202906609 and 7.001707393 and the pitch terms to
0.937995916 and 0.243899790.

The analogous nylon surrogate was rejected: every tested displacement lost on
the training total, including the zero-displacement topology at 8.717053915
against the exact authored nylon cue at 8.707613984. Nylon therefore keeps the
3-cent velocity-squared, Touch-scaled cue and its 75 ms decay. The runtime is a
deliberate hybrid selected by the same train-then-validation gate, not a claim
that either cue is a geometrically exact nonlinear-string solve.

## 2026-08-30 — one steel fret-decay scalar promoted

A train-only bounded sweep selected `steelFretT60Slope = -0.030`; nylon and
open-steel behavior remain unchanged. Against the legacy +0.018 steel law, the
pre-upper-loss seven-term total improves from 7.239749591 to 7.193228995 on
training and
from 7.055491938 to 6.990990829 on development validation. The corresponding
decay term improves from 5.588436814 to 5.429767095 and from 4.547022228 to
4.322596239. Six per-string parameters and a steel bass-group split were
rejected as unnecessary.

## 2026-08-30 — shared upper-partial loss cutoff promoted

A residual audit found a material- and split-consistent sustain error: the
model remained too line-spectral above the attack and its upper partials died
too quickly. The underlying loss topology follows the measured, frequency-
dependent string-loss direction in the 2026
[measurement-informed model](https://dafx26.mit.edu/assets/papers/DAFx26_paper_40.pdf),
so the smallest identifiable correction was a shared multiplier on the
existing material- and age-dependent loss cutoff, not a note EQ or recorded
layer.

A strictly training-only grid selected `highLossCutoffScale = 1.30`:
1.00/1.20/1.25/1.30/1.35/1.40/1.50 scored
7.193229/7.082983/7.062489/7.058992/7.060754/7.060199/7.078266. Only after
selection, development validation improved from 6.990991 to 6.792615. Relative
to the frozen neutral calibration, the promoted model now scores 7.058992024
on training and 6.792615104 on development validation, improvements of
25.4768% and 23.1697%. It has 24 stored values and 23 active values; the direct
branch remains fixed off. Tests spanning cutoff scales 0.5 to 4.0 show more
8 kHz round-trip retention in both materials while holding fundamental decay
within 0.001 dB and compensated pitch within 0.01 cent.

Against the immediately preceding fit, decay, harmonic-envelope and body terms
improve enough to lower both totals. Training attack, tuning and dynamics move
slightly the other way; development-validation attack and dynamics do too,
while its tuning term improves. All seven terms remain better than the neutral
baseline on both splits. The held-out set is still development validation, not
an untouched final test or a listening verdict.

The new cutoff was followed by a train-only interaction audit. The existing
steel displacement scale 0.0061 remained the total-score minimum among
0/0.003/0.0061/0.009/0.012, and the -0.030 fret slope remained the minimum
among -0.050/-0.030/-0.010/+0.018. Turning the steel pitch mechanism off lowers
the development-validation aggregate from 6.792615 to 6.771149 but worsens its
pitch-trajectory term from 0.176762 to 0.240165; it remains enabled because the
training total selected it and both splits retain the mechanism-specific pitch
benefit. The -0.030 fret law beats the legacy law on both total and decay in
both splits.

For each of three larger candidates, validation was never opened. Extending the
measured body from 96 to 144 modes worsened training from 7.193229 to
7.348625 (7.334914 after a body-only recalibration). A 256-tap measured-H1
residual FIR worsened every tested training gain, and a bridge/body pole-
coherence depth likewise worsened training monotonically. These failures keep
the shipping runtime small and preserve the measured passive topology; the
remaining diffuse-field deficit still requires new body/bridge measurements
rather than speculative filters.

A post-fit diagnostic marks that boundary explicitly. The cutoff improves
upper-decay ratios in both materials and both splits, but every one of the 88
examples remains less spectrally flat than its target in both 1--4 and
4--12 kHz sustain bands; the latter becomes still more line-spectral. The next
physically identifiable step is a force-referenced two-axis saddle-admittance
measurement paired with stereo pressure responses, followed by a passive
reciprocal dense 2x2 bridge/body state-space fit. Another noise layer, cutoff
or short residual FIR is not justified by this evidence.

## 2026-08-30 — conventional MIDI default and lower-zone MPE completed

Without a zone message, all MIDI channels remain independent and channel 1 has
no global role. RPN 6 on channel 1 alone creates, resizes or removes a lower MPE
zone; its contiguous members receive channel-scoped bend and sustain while
channel 1 provides manager bend and sustain. RPN 0 supports exact
0--96-semitone ranges,
with 2/48-semitone MPE manager/member defaults. Released tails freeze their
member bend so later member-channel reuse cannot retune them, but manager bend
continues to apply. Zone changes reset only their affected channel union, and
same-sample CC120/CC123 cancellation is insertion-order independent.

Acustra advertises MPE support for that tested subset only. Upper zones,
per-note pressure and CC74 timbre are not implemented.

## 2026-08-30 — full-band passive bridge fit promoted

The scalar g21 driving-point mobility was re-extracted through 10 kHz without
changing the bridge topology or any fitted calibration. Sixty-five measured
peak candidates is the smallest prominence pool whose nonnegative
least-squares projection retains 50 modes, matching the lower end of the
50--200-mode bridge fits reported for the same DeVoe guitar in the 2026
[measurement-informed model](https://dafx26.mit.edu/assets/papers/DAFx26_paper_40.pdf).
All 50 residues are positive, their Q values remain in the existing 2--80
bounds and the known two-sample instrumentation alignment remains optimal.

Against the frozen 26-mode table over 60 Hz--10 kHz, relative complex error
falls from 0.301071418 to 0.185925823 and median absolute magnitude error from
3.432411692 to 0.823970887 dB. The 4.2--10 kHz median falls from
8.551923897 to 0.701418946 dB. The candidate was frozen before development
validation was opened.

With the existing production calibration, the 64-example training score
improves from 7.058992024 to 6.959311242. Attack, harmonics, tuning,
pitch trajectory, body and dynamics improve; decay moves from 5.012553537 to
5.084094050. Development validation was then opened once: its aggregate
improves from 6.792615104 to 6.764893270. Attack, harmonics, body and dynamics
improve there, while tuning, pitch trajectory and decay regress.

All four JUCE-free tests pass. Across six alternating Release runs, the median
six-string runtime ratio moves from 0.0266 to 0.0343 times realtime, still far
inside the 0.25 gate. The measured fit correction, training improvement in six
of seven terms and repeated validation-total improvement justify the 24 extra
bridge modes. The decay tradeoff remains explicit and controlled listening is
still required before any realism claim.

## 2026-08-30 — post-bridge bounded calibration continuation promoted

After freezing the 50-mode bridge topology, the existing bounded physical
fitter ran four short training-only stages in global, nylon, steel and global
order, with `max_nfev = 2` for each stage. Training fell monotonically from
6.959311242 through 6.782768236, 6.720451178 and 6.674004306 to a frozen
6.634038948. No runtime branch, coefficient table, parameter or UI control was
added, and development validation remained closed throughout selection.

Validation was opened once after the final 24-value vector was frozen. Its
aggregate fell from 6.764893270 to 6.473144460 (4.31%), and attack, harmonics,
tuning, pitch trajectory, decay, body and dynamics all improved. Training
improves 4.67% relative to the preceding 50-mode calibration and 29.96% from
the reproducible neutral baseline; development validation improves 26.78%
from neutral. This generalisation promotes the calibration without changing
the signal topology. The split remains development validation, not a blind
perceptual test, so controlled listening is still required.

Because the new bounded vector changes physical bridge/body transfer level,
the fixed reference-to-output calibration moves from 11 to 18. The quietest
public demo is again just above -20 dBFS and the loudest remains below the
-1 dBFS limiter knee. This scalar is excluded from descriptor fitting and
changes no spectrum, decay, coupling or topology; a production rerender changes
the reported scale-normalised aggregate by less than `8e-8`.

## 2026-08-30 — residual identity audit and nonlinear sidecar deferred

A frozen-candidate audit used only training renders and six dry g21 nylon
strings; development validation was not reopened. The new calibration improves
the dry diagnostic by 8.22%, but the guitar-identity residual survives: early
nylon tilt is +4.34 dB/octave, H1 is -16.87 dB relative to the reference and
upper-minus-low T60 is +0.432 log2. Steel early H1 is now balanced, while its
late high-frequency excess remains. This ranks bridge-force H1--H12 telemetry
and per-string finite-duration excitation/contact ahead of per-string damping
and further sympathy work.

A minimal steel-only output-side quadratic radiation prototype was also frozen
on training. At its largest invariant-safe gain, it moves 6.634038948 to
6.607815399 (0.40%) and improves harmonics, decay, body and dynamics, but
worsens attack, tuning and pitch trajectory. A full local-slope/SAV string
solver is not identifiable from the present corpus, and validation was already
closed for this release. The sidecar therefore remains an isolated next-blind-
set candidate; it is not in the production source, demos or packages.

The measured steel per-string friction pattern was likewise rejected after
rebasing on the promoted calibration: its training gain shrank to 0.021%, with
harmonics, tuning, decay and dynamics all moving the wrong way. No new runtime
filter, parameter or UI control was added for either experiment.

## 2026-08-30 — finite-duration pluck/contact gate declared before selection

The existing release-from-rest triangle and spatial aperture are retained as
the baseline. Its two-pole contact filter reaches only the disabled direct
branch, so a candidate may alter the played-string excitation or the measured-
body bridge force, but may not add output EQ, samples, a new user control or an
unmeasured feedback path.

Before any fresh-blind score is opened, the candidate must satisfy all of the
following on the frozen 64-example training split and six dry g21 nylon notes:

- lower aggregate score by at least 0.5% from 6.634038943;
- lower the nylon material score by at least 1.0%, while steel may not worsen
  by more than 0.1%;
- strictly improve both attack and harmonics;
- reduce the median dry H1 deficit by at least 2 dB and early tilt by at least
  1 dB/octave, with the same direction on at least five of six strings;
- not worsen upper-minus-low relative T60, host-rate invariance, passivity,
  finite-output tests or the 0.25-times-realtime performance gate.

Only a candidate frozen under those rules may open one independently prepared,
target-disjoint corpus. Promotion then requires at least 0.5% blind aggregate
and 1.0% blind-nylon improvement, strictly better blind attack and harmonics,
no individual blind term more than 1.0% worse, and no refit after opening.

## 2026-08-30 — finite-release initializer rejected before blind

Primary pluck studies support retaining the statically displaced string while
removing its balancing finger force over finite time. An isolated prototype
therefore applied an exact one-sided circular boxcar to the existing preloaded
triangle. For a release fraction `q = 2 f0 tau`, its modal response is the
linear-unloading solution: displacement is multiplied by `sin(x) / x`, it adds
the corresponding nonzero release velocity, and the free amplitude is
`sinc(x / 2)`. This preserves the fundamental while progressively reducing
upper modes; it is not the previously rejected zero-state point-force pulse.
Only nylon changed, steel remained byte-identical, no control was added and no
output or body EQ was used.

A bounded constant sweep from `q = 0.075` to `0.090` found only one candidate
that cleared the aggregate, material, attack, harmonic and dry-spectrum parts
of the predeclared gate. At `q = 0.080`, training moved from 6.634038943 to
6.595114664 (0.587%), nylon from 8.057892013 to 7.780728841 (3.440%), attack
from 7.685494849 to 7.581950932 and harmonics from 8.166084054 to
8.165118576. The dry g21 median H1 deficit improved by 2.136 dB and early tilt
by 1.200 dB/octave, each in the desired direction on all six strings and in
absolute error on five of six.

The same candidate worsened dry upper-minus-low relative T60 from 0.432302 to
0.459944 log2, with absolute error worse on all six strings. Training decay
also moved from 4.749004326 to 4.765994265, and the nylon-treble aggregate
worsened by 0.847%. The adjacent candidates either retained that decay failure
or also failed the harmonic/H1 threshold. The prototype therefore failed its
frozen gate and was rejected without a refit, full-suite run or production
edit. Development validation was not read again, and the independently sealed
16-recording blind corpus remains unopened and unscored.

## 2026-08-30 — bridge-stage telemetry localised the weak fundamental

A six-open-string nylon probe captured the played-string reaction, the net
body force after the short tail, tail force, bridge velocity, sympathetic
radiation force and stereo output. Identical early and late H1--H12 windows
show that the played-string excitation is not where low E loses its
fundamental: early H3/H1 moves from -7.174 dB at the main reaction to
+8.694 dB at the body force, +11.025 dB after sympathy and +23.305 dB at the
output. Sympathy contributes 2.331 dB of that differential; the bridge split
and radiation response contribute the rest.

This is an absolute-frequency response, not a harmonic-index rule. Partial
pairs from different notes within 10 cents match the main-to-body transfer to
0.049 dB median and the radiation transfer to 0.024 dB median. Thus an
arbitrary per-harmonic correction would encode one guitar's modal fingerprint
in the string model and is rejected as a diagnosis.

The public g21 open-note WAVs used by the dry audit are outputs of the DAFx-26
synthesiser, not recorded plucks. They contain pressure after the combined
primary and sympathetic radiating force and after per-instrument normalisation.
Acustra's independently authored minimum-phase radiation bank is not the
unpublished forward bank that made those files. Consequently pressure-only
deconvolution cannot recover an absolute or complex reference bridge force;
those files remain a relative synthetic diagnostic, not a force oracle.

The closest local mechanism was a measured bridge mode at 82.764 Hz, 7.5 cents
above open low E. An isolated build used the existing analysis switch to omit
that mode. Low-E output H1 rose 5.234 dB early and 7.234 dB late, confirming
the local mechanism, but H3 remained dominant. More importantly, the frozen
training aggregate improved 1.049% while nylon worsened 1.327% and the
harmonics term worsened by 0.031774. Across the six dry notes median H1 improved
only 0.657 dB and only one note reduced its absolute H1 error; spectral tilt
and relative T60 balance both worsened. The candidate therefore failed before
host-rate, performance, development-validation or blind evaluation and is not
in production.

One audit command used an overly broad `/private/tmp/acustra-*` documentation
glob while locating the already-declared gate. It returned no validation or
blind content or score, and the candidate was selected and rejected only from
the frozen training and dry diagnostics. Literal zero filesystem traversal of
unrelated temporary trees cannot nevertheless be certified, so this protocol
caveat is retained explicitly.

## 2026-08-30 — signed training residual constrains the next contact model

A separate read-only audit reproduced the frozen 64-row training score exactly
at 6.634038943 and partitioned every robust contribution by material and MIDI.
Body is the largest weighted residual at 1.884807, followed by harmonics at
1.633217, attack at 1.537099 and decay at 1.187251; together they are 94.1% of
the objective. Nylon remains harder per comparable residual (8.057892 versus
6.509881 for steel), although steel owns more global weight because the
schedule contains 54 steel rows and 10 nylon rows.

The nylon harmonic error is not a monotone brightness curve. After the scorer's
independent early/late mean normalisation, both bass and treble have weak H1;
H3--H6 are generally excessive; treble H8--H11 are already weak; and late H12
is excessive. Nylon MIDI 84 is the largest single-note residual at 13.069846,
driven mainly by body and attack. A scalar darkening filter would therefore
repair one region by damaging another. This supports one controlled smooth
contact-release test, then an alternate measured-body screen, rather than an
arbitrary output tilt or more sympathetic gain.

## 2026-08-30 — smooth half-cosine release rejected on training

The one-parameter successor to the rejected linear unloading used a
raised-half-cosine holding force. Its unloading rate is zero at both endpoints,
and its exact modal multiplier
`pi^2 cos(x/2) / (pi^2 - x^2)` falls at 12 dB/octave asymptotically instead of
the boxcar rate's 6 dB/octave. The variance-matched duration `q = 0.1061` was
fixed analytically before scoring; only nylon changed, and the existing
preloaded triangle, spatial aperture, nonzero release velocity, body and output
path remained intact.

On the frozen training split, nylon improved 2.836%, attack improved by
0.099912 and body by 0.116682. Steel was byte-identical. The aggregate moved
from 6.634038943 to 6.601931794, only 0.484% rather than the required 0.5%,
while harmonics worsened by 0.009564 and decay by 0.015617. Nylon bass improved
10.278%, but mid worsened 0.057% and treble worsened 0.838%. These two first-
stage failures stopped the test before any duration neighborhood, dry g21
diagnostic, invariant suite, development validation or blind score. The
candidate is not in production.

## 2026-08-30 — alternate measured-body banks rejected on training screen

A training-only screen replaced the 96-mode g21 radiation bank with the same
raw-Mores, dual-microphone extraction for public guitars g02, g03, g25, g33,
g49, g55, g61 and g62. The current g21 measured bridge, fitted physical
calibration, engine source and production controls were fixed. The subset
contained all 10 nylon rows plus every round robin and both velocities for
steel MIDI 40, 60 and 84: 28 rows and 16 unique renders per body. An isolated
g21 build was byte-exact with the frozen baseline, with zero maximum sample
difference. Development validation and the sealed blind corpus remained
closed.

The current g21 bank ranked first at 6.692234. The closest alternative, g61,
scored 6.721476, 0.437% worse overall: nylon improved only 0.202%, while the
steel anchors worsened 0.870%. Every other bank was 0.882% to 2.807% worse
overall. No alternative met the predeclared requirement to improve the mixed
subset by at least 0.5% while keeping both materials within 1.0%, so a full
64-row render was not warranted and no candidate advanced to invariants or
blind evaluation.

This was an exact computational body-only swap, but not a matched physical
instrument: every alternate acoustic bank remained paired with the g21 bridge,
and the public source responses are normalised per guitar. The negative result
therefore rejects a simple radiation-bank substitution, not the hypothesis
that a simultaneously measured bridge/body pair could improve the model. No
production DSP, demo or package changed.

## 2026-08-30 — phase-preserving radiation replacements rejected on training

The current measured body deliberately converts each truncated force-to-
pressure response to minimum phase before fitting its 96 complex-residue
modes. Two frozen training-only candidates tested whether that conversion was
responsible for the remaining non-guitar identity.

The first retained the raw complex H1 response after the same 3000-sample
window and fitted the existing modal runtime from it. Across all 64 training
rows, the aggregate moved from 6.634038943 to 7.354591322, 10.861% worse.
Nylon worsened 9.195% and steel 11.228%; attack, body, harmonics and decay all
moved the wrong way. Better dynamics and tuning were insufficient to pass the
predeclared aggregate and material gates.

The second bypassed the modal approximation and convolved the exact captured
runtime drive, `bridge body force + sympathetic radiation force`, with the two
raw 3000-tap g21 microphone FIRs. It retained measured channel polarity,
relative gain and delay, applied no per-channel normalisation or output EQ,
and used the normal final safety limiter. The aggregate moved to 7.174969058,
8.154% worse; nylon worsened 6.909% and steel 8.453%. Attack, body, decay,
harmonics and pitch trajectory worsened. Peak level before the limiter was
0.2083 and no sample was limited, excluding clipping as the cause. A separate
28-row implementation independently reproduced the baseline and rejected the
same FIR by 7.587%.

Neither candidate opened development validation or the sealed blind corpus,
and neither reached invariant or performance testing. These results reject
the two exact replacements, not acoustic phase in general: the FIR also
bypasses the fitted body morphs and is driven by an uncalibrated force proxy,
while the full 62.5 ms response includes measurement-room and rig energy. The
minimum-phase/modal body therefore remains useful regularisation for the
present corpus and runtime. Production DSP, demos and packages are unchanged.

## 2026-08-30 — local-slope SAV string/bridge candidate rejected by public nonlinear oracle

A public, fit-corpus-independent nonlinear audit first established what the
current model does not reproduce. The DAFx-26 guitar-49 five-force sweep
changes its normalised H1--H12 profile by 9.88 dB RMS at 0.7 s, enriches
H5--H12 relative to H1--H4 by 12.59 dB and increases its early pitch excursion
by 9.13 cents.
Current Acustra's profile and upper-partial tilt changes are 0.49 and -0.12 dB
for nylon, and 4.43 and -1.10 dB for steel; its corresponding pitch changes
are 0.94 and 1.63 cents.
The public excitation position varies across force levels, so these figures
are a directional nonlinear oracle rather than a clean matched-recording fit.

An isolated nylon candidate then replaced the played vertical waveguide and
scalar bridge load together. It used the DAFx-26 geometrically exact local
string-slope potential, a scalar auxiliary variable, 128 spatial slope points,
string modes through 10 kHz, nonlinear interaction through 5 kHz and the
measured 50-mode bridge in one mass-normalised reciprocal system. A symmetric
constrained solve enforced bridge compatibility after the paper's oblique
elimination exposed a damping-history sign inconsistency. The algebraically
exact discrete damping numerator `2(1-r^2)/k` was also required in place of
the printed `4(1-r)/k` approximation. The final two-rank update preserved a
five-second lossless energy to 5.393e-12 relative span, reproduced uncoupled
pole frequency to 1.242e-9 cents, kept the bridge constraint and SAV finite,
and independently matched multiplier and reconstructed bridge force. Steel
remained byte-identical. No sample playback, output EQ, new control or neural
residual was involved.

The candidate nevertheless failed every group of the frozen public gate. At a
2.4 mm full-velocity displacement its H1--H12 force-profile error was
10.0751 dB RMS versus production's 9.3898 dB, 7.30% worse rather than the
required 25% improvement. Its five pitch excursions were 12.07, 12.03, 12.70,
11.29 and 10.76 cents: positive but non-monotone, with only one of four force
intervals closer to guitar 49. The non-harmonic peak test passed, but worst
finite-render DC was -71.88 dB relative to RMS against a -80 dB limit. A
level-matched 3.6 mm follow-up made profile error 10.1162 dB (7.74% worse),
remained non-monotone with only two closer force intervals and reached only
-72.03 dB on the DC test. It therefore did not advance to frozen TRAIN,
development validation, the sealed blind corpus, engine integration or
listening selection.

This rejects a played-string-only transplant into the present g21
bridge/radiation and authored attack handoff, not local geometrical
nonlinearity itself. The experiment produced realistic nonlinear motion, but
the wrong instrument-specific motion: an almost level-independent 11--14 cent
early/late trajectory and worse harmonic redistribution. The next nonlinear
attempt must use a matched bridge/radiation pair and excitation from the same
instrument, expose force and string-state diagnostics before microphone
radiation, and be judged against real plucks as well as the public synthetic
oracle. Production DSP, demos and packages remain unchanged.

## 2026-08-30 — matched-measurement audit identifies the missing experiment

An exact provenance audit found that the shipping mechanical and radiation
models are already a matched pair: both use Mores archive guitar g21, row 20,
the same treble-side impact and calibrated hammer channel. The bridge path is
force to co-located accelerometer mobility; the stereo body paths are force to
the two microphones. They are parallel observations of one impact, not a
serial bridge/body cascade. The mismatch begins at the note corpus: all 321
real plucks come from other instruments, and none supplies calibrated force,
bridge motion or physical string/fret identity.

The closest published capture design is Bertrand Scherrer's
[2013 McGill dissertation](https://www.collectionscanada.gc.ca/obj/thesescanada/vol2/QMM/TC-QMM-116950.pdf).
On one Alhambra 1C it combined repeatable AWG44 wire-break
plucks, force-hammer/laser measurements, a calibrated two-axis bridge
admittance matrix, a bridge accelerometer and two synchronous microphones.
The thesis says a WAV database was created, but provides no public download or
reuse licence. It is therefore an evidence-backed acquisition blueprint, not
training data. The current
[NEMUS sample repository](https://github.com/Nemus-Project/65_modelled_guitars)
contains only synthesised WAVs and no licence file; its open
[Mores measurement record](https://zenodo.org/records/4604577) provides the
CC BY 4.0 hammer-response MAT data but no played notes. Searches of current
open guitar datasets found real notes or physical transfer measurements, but
not both on the same acoustic guitar.

The smallest reproducible local matched audit regenerated the current g21
coefficients from the raw MAT file with its pinned MD5. The existing passive
bridge gate remained unchanged. A missing body regression gate was added over
80--10000 Hz: per microphone, relative complex error must be at most 0.30,
median magnitude error at most 1.25 dB and 90th-percentile magnitude error at
most 3.75 dB; the 90th-percentile error in stereo magnitude ratio must be at
most 5 dB. The shipping fit passes at complex 0.220519/0.241239, median
0.970119/1.145597 dB, p90 2.952076/3.474372 dB and stereo-ratio p90
4.735559 dB. Regeneration remains byte-identical, so no production coefficient,
DSP behavior, demo or package changed.

The next decisive dataset must repeat that matched chain separately for a
classical nylon and flat-top steel guitar, adding controlled release force or
displacement, pluck position/direction, string construction and action, plus
independent human plucks. Until such data are available, another end-to-end
nonlinear fit is underidentified; public modelled-guitar force sweeps remain
mechanism screens rather than real-guitar validation.

## 2026-08-30 — string-side upper-loss shelf split rejected

An audit of the shipping build against the reference corpus found a damping
error that the fitter's own decay descriptor reports only weakly. Per-partial
early decay rates, pooled by absolute frequency rather than by harmonic number,
put steel at 19.6 dB/s over 1350--2000 Hz against 26.9 dB/s in the recordings,
19.8 against 27.0 over 2000--3000 Hz and 22.2 against 27.3 over 3000--4500 Hz,
while the model decayed 7.8 dB/s too fast at 80--120 Hz and 9.9 dB/s too fast
at 180--270 Hz. The same signs appeared on both splits and in both materials.
The fitter reduces each partial to one robust log2(T60) over the top 55 dB, so
a 1.36x T60 error — 13 dB of extra ring after two seconds — appears as a 0.4
residual among eighteen decay features.

The first candidate gave the string-side upper-loss shelf a per-material corner
and a depth decoupled from `frequencyLossScale`, on the argument that nylon and
steel dissipate through different mechanisms. Its predeclared gate required
both splits to improve and the three under-damped steel bands to move toward
the recordings. Training improved from 6.634039 to 6.580260 and development
validation from 6.473145 to 6.431137, but the damping gate failed: steel
1350--2000, 2000--3000 and 3000--4500 Hz moved to -7.0, -7.3 and -5.2 dB/s
against -7.2, -7.2 and -5.1 before. The steel shelf parameters moved by less
than 0.03%, so the aggregate gain came from unrelated values.

A direct sweep explains the refusal. The shelf is the wrong shape: settings
that correct 1350 Hz overshoot 3000 Hz by three times as much, and every
setting that moved the mid band moved training from 6.634039 to between
7.729 and 11.537. The candidate was rejected and all of its plumbing removed.

## 2026-08-30 — plate conductance floor promoted

The user proposed that the error was in the body rather than the string. A
direct ablation confirmed it. Disabling the bridge junction and remeasuring
the model's own damping curve showed how much of its frequency-dependent
string damping the body supplies: +30.3 dB/s over 400--600 Hz, +13.0 over
600--900, +12.4 over 1350--2000, +4.6 over 2000--3000 and +1.1 over
3000--4500 Hz. The measured g21 conductance behind that curve falls to 0.43 of
its 400--600 Hz value over 2000--3000 Hz and 0.20 over 3000--4500 Hz. The two
bands where the measured conductance peaks are exactly the two where the model
decays too fast, and the band where it collapses is exactly where the model
rings too long.

That collapse is a property of the fit, not of a guitar. A 50-mode
positive-real fit of a 60--10000 Hz driving-point measurement reproduces the
mobility magnitude to 0.82 dB median while losing the conductance between
modes, where a real top plate has dense modal overlap. Cremer and Heckl give
the high-frequency limit of a plate driving-point mobility as a real constant
for that reason.

The promoted mechanism is one over-damped positive-real section added to the
bridge, with real poles at a fitted lower corner and a fixed 16 kHz upper
limit, so its conductance is flat between them and vanishes below. It is
passive by construction, and a zero plateau renders the previous build
bit-identically: all 140 corpus files hashed equal.

A training-only screen selected the starting point, floor 0.004 and corner
2400 Hz at 6.540388 against 6.634039. The bounded fit then chose a plateau
conductance of 0.0101273 and a corner of 3925.3 Hz. Training improved to
6.417666, 3.26% better, and development validation to 6.434787, 0.59% better.
Six of seven training terms improved; decay worsened on both splits, and
harmonics and tuning worsened on validation.

Against the predeclared damping gate, steel improved from -7.2 to -5.5 dB/s
over 1350--2000 Hz, from -7.2 to -2.5 over 2000--3000, from -5.1 to +1.2 over
3000--4500 and from +15.5 to -11.7 over 6800--9000, with no band below 900 Hz
worsening by more than 2 dB/s. Nylon improved in six of eight bands. The cost
is at the top of the band, and it is under-damping rather than over-damping:
4500--6800 Hz moved from -1.6 to -8.5 dB/s, so the model now decays too slowly
there, and 6800--9000 Hz crossed from 15.5 dB/s too fast to 11.7 dB/s too slow.
The joint fit raised the shared upper-loss cutoff from 1.869 to 2.021 while the
plate floor supplied the mid band, which puts the steel string's upper-loss
corner near 21 kHz and leaves the top of the audible band carrying almost no
string loss. Partial counts per band are unchanged, so this is a real change in
the model and not a shift in which partials the audit could measure.

An earlier form of the same idea, a frequency-independent conductance added at
the junction, was rejected on training before validation was opened. It
corrected the mid band but also damped the low partials the measured modes
already dominate, moving training from 6.634039 to 6.706 at the setting that
best corrected the damping curve. The band limit is what makes the mechanism
defensible, and it follows the physics: the plate asymptote holds above the
modal-overlap frequency, not at DC.

This is a measurement-led promotion against two different-instrument reference
corpora, not perceptual equivalence to a guitar and not a listening verdict.

## 2026-08-30 — decay descriptor corrected to compare rates

Three separate mechanism experiments were steered by the same descriptor
defect, so the descriptor was corrected before any further fitting. The decay
term reduced each partial to a robust `log2(T60)`. What a decay error does to a
note is a level difference, and after `t` seconds that difference is exactly
`(rate_model - rate_target) * t` dB. The relative form therefore scored the
same 1.4x error identically whether it meant 25 dB after one second on a 0.5 s
partial or 3 dB on a 4 s partial. The term now compares the same robust line's
decay rate in dB/s, with a 6 dB/s tolerance — a 6 dB level error after one
second of decay, which is also close to the previous 0.35 log2 tolerance at the
corpus's median T60, so the term keeps a comparable magnitude.

The argument does not depend on any candidate, and the change was made before
the affected fits were rerun. Every score after this point is under the
corrected descriptor and is not comparable with the previously frozen numbers.
Under it, the build before the plate conductance floor scores 6.280687 training
and 6.485961 development validation; the plate-floor build that had been fitted
under the old descriptor scores 6.118156 and 6.521663.

A second suspected defect was checked and rejected. The descriptor resolves
twelve partials per note regardless of pitch, so for a low E it stops at 990 Hz
and for the top note at 12.6 kHz, and the upper spectrum is seen only through
six coarse bands. Measured on the recordings themselves, however, H1--H12
already carries 90.1% of nylon and 94.1% of steel energy over 60--8000 Hz in
the first second, so widening the harmonic set was not justified and the
descriptor was left alone.

## 2026-08-30 — plate conductance floor confirmed by controlled ablation

Refitting the shipping build under the corrected descriptor improved training
from 6.118156 to 6.096683 and development validation from 6.521663 to 6.515843,
but not past the 6.485961 of the build that preceded the plate floor. Its
predeclared gate required both, so that refit was rejected. The comparison was
not clean either way: the pre-floor calibration had itself been fitted under the
old descriptor.

The mechanism was therefore decided by a controlled ablation, declared before
either fit ran. Two bounded fits under the corrected descriptor started from the
same pre-plate-floor calibration and differed only in whether the plate values
were free. Free, they reached 6.132177 training and 6.378198 development
validation. Pinned at zero, 6.246925 and 6.462390. The floor wins both splits,
so it is kept, and the free fit's calibration ships. Its plateau conductance is
0.0035960 with a 2138.6 Hz corner, about a third of the plateau the earlier
old-descriptor fit had chosen.

That calibration is the best of everything measured under the corrected
descriptor: 6.378198 against 6.485961 for the pre-floor build and 6.521663 for
the build that shipped this morning. Against a neutral calibration re-rendered
on the same runtime it improves 36.16% on training and 31.25% on development
validation, with every validation term better and six of seven training terms
better; training tuning is 0.45% worse.

The independent damping audit agrees for steel on every band it can measure:
1350--2000 Hz moves from -7.2 to -5.4 dB/s, 2000--3000 from -7.2 to -3.0 and
3000--4500 from -5.1 to -0.7. The two bands above 4.5 kHz are withheld; the
entry at the end of this log explains why, and withdraws the 6800--9000 figure
that one clause of the promotion gate cited.
Nylon is a trade: 2000--6800 Hz improves by 4.0 to 5.2 dB/s while 270--2000 Hz
worsens by 0.8 to 3.3 dB/s, so nylon now decays too fast in its low and middle
register. Ten nylon training rows against seven nylon parameters leave that
stage close to unidentifiable, and it stalled without improving in two of the
three fits run here.

The refitted nylon string is quieter, which put the nylon demo below the
renderer's safe pre-normalisation peak floor. Its demo velocities were raised
from 0.60 to 0.66; the engine was not touched.

## 2026-08-30 — the 82.764 Hz bridge mode is a body mode, not an installed string

The retained bridge candidate at 82.764 Hz sits 7.5 cents from a standard open
low E, and the analysis-only `ACUSTRA_ANALYSIS_EXCLUDE_MEASURED_OPEN_STRINGS`
switch exists because the g21 setup photographs show installed strings. Since
the model is measurably over-damped exactly there — steel decays 8.1 dB/s too
fast over 80--120 Hz and 9.3 dB/s too fast over 180--270 Hz against the
reference recordings — the obvious suspicion was that the measured mobility
contains the measurement guitar's own string, so the model damps a played low E
against a phantom one.

The measured Q settles it without any new data. That mode has Q 16.62, a 0.44 s
T60. An open nylon low E has Q near a thousand and a T60 of several seconds; a
guitar's low-frequency air and top modes have Q in the tens. The neighbouring
retained modes at 90.820, 177.979 and 208.740 Hz have Q 21.27, 18.23 and 42.01,
the same population. No retained mode anywhere in the 50-mode set has a
string-like Q: the median is 17.7 and the maximum 75.5, at 591 Hz.

So the resonance is a body mode whose proximity to E2 is a coincidence, there is
nothing to de-embed, and the low-band over-damping is not a measurement artefact.
The remaining explanation is corpus mismatch: g21 is a flamenco guitar and the
steel references are a miked archtop, whose arched top couples to its low
resonances quite differently. Fitting that band against this corpus would fit an
archtop's body onto a flamenco measurement, so it stays an open gap rather than
a target. The de-embedding switch stays analysis-only and unused.

## 2026-08-30 — nylon training split widened; its first fit rejected

The nylon stage fits seven parameters on ten training rows and stalled without
improving in two of the three fits run today. The offline bank holds 41
FreePats nylon regions: ten were in training, ten in development validation and
21 were unused. Training now takes every unused region playable on the six
modelled strings — MIDI 38 and 39 lie below the lowest open string and are
excluded — which moves nylon training from 10 to 29 rows, the corpus from 64 to
83, and the training material balance from 54 steel against 10 nylon to 54
against 29.

Development validation is deliberately untouched: the same 24 rows, so held-out
scores stay directly comparable. Training scores do not, and every training
figure in the fit report is now on the 83-row corpus. On it the shipping
calibration scores 6.431871 against a neutral 9.671509, and all seven training
terms now beat neutral; the 0.45% tuning regression reported earlier was an
artefact of the narrower row set.

The first bounded fit on the widened corpus was then rejected. It passed the
validation clause of its predeclared gate, 6.378198 to 6.355826, but failed the
nylon damping clause: only three of ten nylon bands improved. Seven worsened,
all by 1.0 dB/s or less, while the three that improved did so by 1.5, 2.6 and
4.0 dB/s, so its net nylon band error actually fell by 6.1 dB/s. That is
recorded rather than reinterpreted — the clause was written before the numbers
were seen, and rewriting it afterwards is how a gate stops meaning anything.
The shipping calibration is unchanged.

Only three values moved in that fit, and the nylon one moved the wrong way:
`nylon.frequencyLossScale` rose again, from 1.318 to 1.424, deepening the
low and middle over-damping the audit already reports. More nylon rows did not
make the nylon stage move much, which points at the model rather than the fit:
nylon shows the same too-flat damping shape steel had before the plate
conductance floor, and it shares one measured body with steel while the two
reference corpora are two different instruments.

## 2026-08-30 — a taken string keeps its vibration

The listening set for the upper-band damping question is with the user, and the
tone path has reached the same ceiling from four directions, so this turns to
the other half of the goal: playability. The instrument allocates six physical
strings, and when all six are sounding a new note takes one. Until now that
wiped the string's delay lines in a single sample.

Measured rather than assumed. Six notes, then a second six-note chord 0.6
seconds later: at the steal the strings still hold 5.045 of the 20.190 stored
energy units they held at the attack, 25.0% of it, 6.0 dB down. Comparing the
delay-line contents immediately after the steal with a fresh engine playing only
the second chord showed 17.121 against 17.105 — the previous build discarded
100.3% of what the strings still held, on every chord change, the most common
gesture on the instrument.

A string is only taken while it is being replucked, so the plucking hand is on
it whatever the outgoing note's fret was. That is the contact `beginRelease`
already models for a stopped fretted note, so the taken vibration now carries on
in a per-string tail under the same 160 ms damping. No new constant was
introduced: the open-string 1.25 s case is a lifted fretting finger on a string
nobody is touching, which a steal is not. The tail radiates one-way through the
same path the idle open strings already use, so it does not load the junction a
second time, and it keeps only the vertical polarisation, which is the only one
the scalar bridge radiates. After the change 85.3% of the stolen energy survives;
the missing 14.7% is that unradiated horizontal polarisation.

Replucking the same note is deliberately unchanged. At an identical delay length
a carried tail would comb against the new pluck instead of modelling the
superposition a finger contact actually leaves, so that case still restarts from
rest and is recorded as an open gap.

The fit corpus renders one note on a fresh engine and never steals, so all 71
model renders are bit-identical and no score moved. Two of the five demos
changed, both of which do steal; the other three are byte-identical. The
six-string runtime ratio is 0.0333x against the 0.25x gate.

## 2026-08-30 — measured how much a real take varies, and left it unmodelled

The instrument repeats a pluck exactly: its only per-note variation is a 0.025
polarisation angle and the deterministic noise burst. Before inventing a
humanisation depth, the reference corpus was asked how much a real take varies.
Eighteen steel groups in the training split hold three round robins each — the
same root at the same captured velocity, played again. Across them the attack
spectral centroid ranges by a median 170 cents, 447 at the 90th percentile and
1120 at worst.

Two other axes were checked and are not usable from this corpus. Attack level
ranges by a median 0.29 dB, but the bank applies a per-zone playback trim and
peak normalisation, so that spread has largely been normalised away rather than
measured. Settled pitch ranges by a median 0.00 cents, which is the analysis
resolution over a 0.7 s window rather than a result.

The obvious mechanism was then measured rather than assumed, and it is wrong.
Sweeping the model's own excitation controls on five steel notes gives
5.67 cents of attack centroid per millimetre of pluck distance, 12.1 cents per
dB of plucking force and 1.13 cents per percent of release aperture. To span
the observed range each would have to move by 30 mm on an 85 mm pluck distance,
by 14 dB, or by 111% respectively. A player does none of those between two
takes of the same note, so no combination of pluck point, force and aperture
explains what the recordings show. The window is not the artefact either: over
the first 40 ms alone the recorded range is larger, a median 287 cents.

What this points at is the plucking angle, and the model cannot express it. The
pluck already varies its polarisation mix slightly per note, but the scalar
bridge radiates one transverse axis, so changing the angle moves energy into the
silent polarisation and scales the note instead of colouring it. Take-to-take
attack variation is therefore a second consequence of the missing measured 2-D
bridge admittance, not a humanisation depth waiting to be chosen. Three
sensitivities are recorded so the next attempt does not re-walk these paths.

## 2026-08-30 — bridge-hand damping added as a controller, not a control

A guitarist palm-mutes constantly, and without it the instrument cannot play a
large part of the steel-string repertoire, so this is a playability gap rather
than a taste question. The design question — whether it earns a panel control
against "keep controls simple" — is answered by not adding one. It is a playing
pressure, so it arrives on CC2 and the panel keeps its ten controls, which is
also how Electry exposes the same gesture in this repository.

The model is Electry's, not a new one. The heel of the picking hand resting by
the saddle is a soft lossy absorber in parallel with the string's own loss, so
the rates add: 1/T60 = 1/T60_string + 1/T60_hand, with the hand's mapped time
running log-linearly from 4.0 s at no pressure to 0.080 s at full. A soft
contact damps the top faster than the fundamental, which Electry calibrates as a
0.62 high-to-fundamental T60 ratio; here that is converted into exactly the
extra per-round-trip loss the existing upper-loss shelf needs to carry, so the
shelf keeps its shape and is untouched at zero. Pressure scales the rate, so
zero is an exact no-op rather than a four-second floor.

Those two endpoints are transferred rather than refitted, and that is stated
where they are used: the absorber is the player's hand, not the instrument's
string set, and the reference corpus contains no muted notes to fit against.
They are a playing pressure the user sets by ear, not a fitted physical
constant.

Zero pressure renders all 71 fit-corpus models bit-identically, so no score
moved. Measured on a steel E3 at 48 kHz, the 1--2 s tail relative to the attack
goes from -8.6 dB open to -21.4 dB at quarter pressure and -69.3 dB at half,
and the attack itself falls 7.8 dB by full pressure: a chuck rather than a note.
The useful musical range is therefore the lower half of the controller, which is
what the calibrated endpoints imply.

The idle strings are damped by the same hand automatically, since they are
configured through the same path — physically right, as the palm rests across
the strings. A tail left by a taken string is not, because its loop is not
reconfigured while it rings; at 160 ms that is a small and bounded exception,
and it is recorded as a gap.

## 2026-08-30 — the fitted stiffness scales are not hiding a physics error in steel

`stiffnessScale` is fitted at 1.48 for steel and 1.80 for nylon, so the fit asks
for 48% and 80% more bending stiffness than the textbook
`B = pi^3 E d^4 / (64 T L^2)` gives from the shipped string data. A scale that
far from one invites the suspicion that the derivation is wrong — most plausibly
the effective core diameter the wound strings use as their bending proxy.

Measuring the inharmonicity coefficient directly settles it for steel. Fitting
`(f_n/n)^2 = f0^2 (1 + B n^2)` to the partial frequencies of every training note
gives a median model-to-recording ratio of 1.15 across eight steel roots: the
shipped stiffness, fitted scale included, lands within 15% of the recorded
instrument. There is no missing factor of two hiding in the steel derivation.

Nylon reads 0.40, so the model looks two and a half times too smooth, but that
number does not support a change. Nylon's inharmonicity is small enough that the
estimator is working near its own noise — two steel roots in the same run
returned 0.21 and 2.82, which is estimator scatter rather than physics — the
high notes leave only six partials below 6 kHz to fit, and the nylon references
are a different classical guitar with a different string set from the modelled
EJ45. The fit also had the freedom to raise nylon stiffness further, since 1.80
sits well inside a bound of 4.0 and the inharmonicity clamp is an order of
magnitude above the measured values, and it chose not to. Recorded so the
question is not reopened without a matched-instrument capture.

## 2026-08-30 — the plate conductance floor replicates from a second start

The floor was promoted on one controlled ablation, and a 1.3% held-out gap
decided by two bounded fits with `max_nfev 6` could be an artefact of where
those fits started. The ablation was therefore repeated from an independent
starting calibration — the shipping vector rather than the pre-plate-floor one —
on the widened 83-row corpus, with the decision rule declared first.

| run | start | floor free | floor pinned |
| --- | --- | ---: | ---: |
| 1 | pre-plate-floor | 6.378198 | 6.462390 |
| 2 | shipping | 6.355826 | 6.387965 |

Development validation is the same 24 rows in all four fits, so they are
directly comparable. The floor wins both splits from both starting points, so
the mechanism replicates and the shipped build stands unchanged.

The second run's held-out gap is 0.032 against the first run's 0.084. The
effect is consistent in sign and smaller than one run alone suggested, which is
recorded rather than averaged away: the honest reading is that the floor helps
held-out data by somewhere between half and one and a half percent, not by a
figure either run pins down.

No calibration is promoted from this run. The free fit reaches 6.355826, better
than the shipping 6.378198, but it is the same vector already rejected on the
nylon damping clause of its own gate, and that rejection stands. The run tested
the mechanism, not a candidate.

## 2026-08-31 — natural harmonics, decided by pitch rather than by a keyswitch

Everything above the twentieth fret was silent: `chooseString` found no string,
`noteOn` returned, and MIDI 85 upward produced nothing at all. A real guitar
does reach there, through the natural harmonics of its open strings, and the
reason that had not been modelled was the assumption that a harmonic needs a
keyswitch to ask for. It does not. A natural harmonic sounds at exactly n times
a string's open pitch, so the requested note itself says whether the instrument
can produce it. The mapping is the physics, not a convention, and no control
was added.

A note above the fretted range is played as a light touch at a node when some
string has an integer multiple of its open pitch within 25 cents, and stays
silent otherwise. That tolerance admits the second through sixth harmonics and
the eighth and excludes the seventh, which is 31 cents flat — the same reason
players avoid it against tempered material. Nodes above the eighth are
impractical to touch and very quiet, so the search stops there. Notes below the
lowest open string stay silent, because nothing on a guitar is down there.

The touch itself is exact. A light contact at the nth node leaves the modes
that have a node there, and averaging the released shape over its n cyclic
shifts is that projection precisely: every harmonic that is a multiple of n
passes at unit gain and every other one cancels. It needs no filter, no
threshold and no free constant, and it costs n evaluations of a shape the pluck
already computes, once per note.

The first attempt wrote an n-fold periodic shape instead, which is the same
mode set but at full pluck amplitude, and it came out five times louder than a
stopped note. That is backwards, and the projection is why: the surviving modes
should keep the amplitude the original pluck gave them, and a triangle's
coefficients fall as 1/k^2, so a harmonic is quieter. Measured on steel, the
fourth harmonic of the open high E is 12 dB below the highest stopped note and
the eighth of the open G is 32 dB below it.

Its pitch is a few cents sharp: E6 lands 0.7 cents from equal temperament, E5
4.6 cents sharp and the eighth-harmonic G6 6.6 cents sharp. That is the stiff
string's own inharmonicity, which grows with the mode number, plus the model's
existing partial-placement residual. It is left uncorrected, because a real
guitar's natural harmonics are sharp for the same reason, and retuning them
would be an authored cue rather than the string.

The fit corpus spans MIDI 38 to 84 and never enters this region, so all 71
model renders are bit-identical and no score moved.

## 2026-08-31 — an attempted physics rule for hammer-ons, refuted by the suite

Hammer-ons had been deferred twice as needing a mapping decision. A rule that
looked physics-determined was then tried: `chooseString` takes a free string
first, then one whose key is up, and only when every string is held does it
take one that is still key-down. In that third tier a repluck is impossible,
because the fretting hand is on all six strings, so the one way a guitarist
sounds another pitch is to hammer onto a string already held. The candidate
therefore refretted that string and let it keep ringing, with no new pluck, on
the same 6 ms slew the pitch wheel already uses.

The engine's own regression suite refuted it on the first run. The
stolen-string tail test plays a six-note chord and then a second six-note chord
without releasing the first, and under the new rule all six arrivals became
hammer-ons and no tail was ever taken. That is the flaw: a lone note arriving
against six held strings is a hammer-on, but six notes arriving together are a
strum, and no guitarist hammers six new pitches at once. The physics does
distinguish the two — by whether the arrivals are simultaneous — and the engine
cannot see that at note-on time, because it has no lookahead across a block.

So the rule is not physics-determined after all; it is a convention about how
overlapping MIDI chords should be read, and it was reverted. What the attempt
established is why the decision cannot be taken from the model alone: it needs
either a legato mode, a note-on grouping window, or an explicit articulation
channel, and each of those is a product choice about how the instrument is
played rather than about how a guitar works.

The same reasoning applies to body knocks. Nothing about a MIDI note number
determines a percussive hit, and the obvious zone below the lowest open string
is exactly the region just confirmed to be correctly silent, since nothing on a
guitar sounds there.

## 2026-08-31 — neither excitation control explains the dark steel attack

Re-running the full descriptor audit on the shipping build, with both
calibrations rendered over the same 83 training and 24 validation rows, put one
residual well ahead of the rest: over the first 350 ms the model's steel
spectral centroid is 446 Hz against the references' 736 Hz, a 39% shortfall
that today's work barely moved.

Both excitation controls were then driven to their bounds. `steel.apertureScale`
from 0.661 to 0.35, a much sharper contact, moves the centroid from 446 to
466 Hz. `steel.transientScale` from 0.523 to 3.0, a sixfold pluck-contact
noise, moves it to 463 Hz and takes the attack term from 7.2385 to 7.6335. Each
closes about 7% of the gap and each makes the training score worse. Neither is
the mechanism.

That matters because "the pluck is not bright enough" is the obvious reading of
a dark attack, and it is wrong here by a factor of fourteen. Resolving the same
window into bands says why, and corrects an inference made earlier in this
entry. Each render normalised to its own total, the model carries 4.6 dB more
energy than the references over 160--320 Hz and 1.5 dB more over 320--640 Hz,
about 3 dB less over 640--2560 Hz, and matches them to 0.1 dB over 5--10 kHz.
There is no missing top end at all. The centroid is dragged down by an excess
low-mid, which no excitation control touches, and that is why both
bound-to-bound sweeps moved it by 7%. Steel's 80--160 Hz band is withheld as
unmeasurable; the entry below explains why, and withdraws what an earlier
reading of it claimed.

Nylon's own excess sits somewhere else entirely and is narrower — 7.6 dB over
640--1280 Hz and 6.7 dB over 1280--2560 Hz, with the rest of its band inside
3.5 dB — so the two materials pull the one shared body in opposite directions.
That excess is nylon-only, so correcting it would cost steel nothing, and the
nylon pluck aperture was swept to its bound to try. From 1.697 to 2.50 the
640--1280 Hz excess falls from +4.5 to +2.9 dB and 2560--5120 Hz from +2.4 to
+0.5, but 1280--2560 Hz, the largest at +7.5 dB, moves only to +7.1 while the
aggregate rises from 6.43187 to 6.48300. The excitation does not reach that
band either.
Splitting that parameter per material would fit the corpus mismatch rather than
the instrument: it is one guitar, and the references are two.

The shared gain on the body's 85--145 Hz modes shows that conflict directly.
Swept against stable bands only, from its fitted 5.43 to 3.0 and then 1.5,
nylon's 80--160 Hz excess goes from +2.5 dB to -0.7 and -2.9 while steel's
160--320 Hz excess grows from +4.6 dB to +5.1 and +5.3, because taking energy
out of the bottom raises every other band's share. The aggregate prefers the
fitted value throughout, rising to 6.44538 by 1.5. It is a trade between two
references, not a correction, and the fitted value stands.

One correction to the record. An earlier reading of this audit reported that
nylon's attack had become 76% too bright today. That comparison was invalid: it
took the recordings' median from the 20-row corpus and the model's from the
39-row one. Re-run on identical rows, nylon improves on five of seven
descriptors against the session's starting calibration, including both
centroids and both spectral tilts, and remains 27% too bright rather than 76%.

## 2026-08-31 — a listening set on body weight, where the loss and the bands disagree

The first listening set of this run asked about upper-band damping. The band
audit has since found a larger and sharper disagreement, so a second set was
rendered on that instead.

Both materials carry too much 80--160 Hz against their references: nylon by
2.6 dB and steel by 10.7. The shared gain on the measured body's 85--145 Hz
modes reaches about half of it. At 3.0 nylon lands on the recordings and steel
improves to +8.3 dB; at 1.5 nylon goes 3.0 dB under and steel reaches +6.7. But
every step costs the fitted aggregate, from 6.43187 to 6.44538, and the body
term with it.

So the loss says stay and the band measurement says move, and neither is
obviously the better judge here: the references are a bass-shy arched top and a
different classical guitar, so following them may mean following the wrong
instrument's low end onto a flamenco body. That is a question about whether a
change is an improvement rather than whether it is correct, which is what the
repository's convention sends to the ear.

A is the shipping engine, B is 3.0 and C is 1.5, for steel and for nylon, one
value differing and everything else identical. Whole-file RMS matches to
0.00003 dB and no letter reaches the limiter. The set is at
`listening-2026-08-31-body-weight/` with its key written at render time and
unread by design. Nothing is promoted until it is heard.


## 2026-08-31 — an unstable band invalidated a finding, and the tool now refuses it

Turning the band-resolved attack audit into a committed tool exposed a fault in
the measurement it came from. Reimplemented, it disagreed with the scratch
version on exactly one number: steel's 80--160 Hz band read -48.2 dB against
the scratch -54.7, while all six other bands matched to a tenth of a decibel.
The cause was a one-sample difference in window length — `int(0.35 * 44100)`
gives 15434 because the product is 15434.999999999998, and `round` gives 15435 —
and on a low steel E that moved the band by 19.4 dB.

The band has no signal to measure. That archtop puts about 94% of its attack
energy into 160--320 Hz and almost none an octave below, so what the band
reports is its neighbour's leakage skirt, and the skirt moves with the bin grid.
A Blackman-Harris window narrows the swing to 13.4 dB but does not remove it.

Two claims rested on that number and are withdrawn. The model does not carry
9.4 dB too much 80--160 Hz against the steel references: that band is not
measurable from this corpus at all. And the low-mode gain does not help both
materials there. Re-swept against stable bands only, lowering it fixes nylon's
80--160 Hz while making steel's 160--320 Hz worse, so it trades one reference
against the other rather than correcting both. Steel's low-mid excess is real
but is 4.6 dB at 160--320 Hz, half what was claimed and in a different band.

`Tools/AuditAttackBands.py` now computes each band at two window lengths 5%
apart and returns any that disagree by more than 1 dB as unmeasured, printing
"below the analysis floor" rather than a number. On the shipping corpus it
withholds exactly one band, steel's 80--160 Hz, and reports the other six as
stable across all 68 steel and 39 nylon examples. Its self-test checks that a
pure tone is reported in one band and withheld in the other six, and that level
alone does not move the shape.

The listening set rendered on the earlier premise keeps its audio, which is
unaffected by any of this, but its key was rewritten to state the corrected
question: lowering the gain is a trade between the two references, not a shared
improvement.

The lesson is worth the entry on its own. A descriptor that is not checked for
stability against its own analysis parameters can manufacture a finding, and
this one reached two documents and a listening-test key before a reimplementation
caught it. Every audit tool in this repository should be able to say which of
its outputs are measurements and which are floor.


## 2026-08-31 — the damping audit gets the same stability guard, and one gate
clause is withdrawn

The attack-band fault prompted the same question of the damping audit, which is
the tool three of this session's decisions leaned on. Re-running it at a 4096
window instead of 8192, and over a 0.20--1.10 s fit window instead of
0.15--1.20 s, moved the bands the plate conductance floor was gated on by 1.2,
1.0 and 1.3 dB/s respectively — and moved 6800--9000 Hz by 10.5 dB/s, from
+0.1 to +10.6.

So that band is analysis floor, not signal, exactly like steel's 80--160 Hz in
the attack audit. `Tools/AuditDampingCurve.py` now measures every band at two
spectral resolutions and withholds any whose model-minus-recording difference
moves by more than 3 dB/s. On the shipping corpus that withholds 4500--6800 and
6800--9000 Hz for both materials and reports everything from 80 Hz to 4500 Hz.

What this costs. The plate floor's predeclared gate had four damping clauses,
and its fourth cited 6800--9000 Hz improving from +15.5 to +3.7 dB/s. That
number is withdrawn, as is the claim that 4500--6800 Hz was barely touched. The
other three clauses stand on stable bands and improved substantially: -7.2 to
-5.4, -7.2 to -3.0, and -5.1 to -0.7 dB/s.

What it does not cost. The mechanism was not decided by the audit. It was
decided by a controlled ablation on the fitted loss, free against pinned from
the same starting calibration, and that ablation replicated from a second,
independent start. Neither run touches the withheld bands. The promotion stands
and the shipping calibration is unchanged; what changes is that the instrument
no longer reports numbers above 4.5 kHz that it cannot support.

Both audit tools now carry a stability check against their own analysis
parameters, and both say which of their outputs are measurements and which are
floor. That is the property whose absence manufactured a finding earlier today.

## 2026-08-31 — the gated benchmark is analysis-dependent too, but its comparisons are not

Both audit tools turned out to report bands that were analysis floor rather than
signal, so the same question was put to the fitter, which is what every
promotion this session was actually gated on.

It is sensitive. Halving the decay descriptor's STFT window from 8192 to 4096 —
a parameter no result should depend on — moves the shipping build's development
validation from 6.378198 to 6.319354 and its decay term from 3.8171 to 3.5818,
a 0.9% shift in the aggregate and 6.2% in the term. Training moves 0.27%. That
is the same order as the margins several of this session's decisions turned on,
and the second plate-floor ablation's held-out gap of 0.032 is smaller than it.

The comparisons survive, because the sensitivity is common-mode:

| ablation | window | floor free | floor pinned | gap |
| --- | --- | ---: | ---: | ---: |
| run 1, from pre-floor | 8192 | 6.378198 | 6.462390 | +0.0842 |
| run 1, from pre-floor | 4096 | 6.319354 | 6.371219 | +0.0519 |
| run 2, from shipping | 8192 | 6.355826 | 6.387965 | +0.0321 |
| run 2, from shipping | 4096 | 6.297464 | 6.325058 | +0.0276 |

The floor wins in all four cells, at both resolutions and from both starting
points. The gap shrinks under the shorter window but never approaches zero and
never changes sign.

The methodological point is worth stating plainly, because it governs how every
number in the fit report should be read. An absolute score from this benchmark
is analysis-dependent at roughly the 1% level and should not be compared across
anything but identical rows and identical analysis settings. What is meaningful
is a paired difference between two builds scored the same way, which is what
every gate here used and why development validation was held at the same 24 rows
throughout even when the training corpus was widened. The absolute figures in
the fit report are a record of a particular measurement, not a physical constant
of the instrument.

## 2026-08-31 — two bridge-port transients, found by a demo that would not pass its own gate

A demo of the new playing behaviours refused to render: the peak gate that
proves a demo is linear before its one normalisation pass rejected it at 0.98,
and lowering every velocity in the phrase by 28% moved that to 0.95. A peak that
does not follow the notes is not the notes. Resolved to 25 ms it was a pair of
isolated impulses at twelve times their surround, and bisecting the phrase found
two separate faults in the bridge junction, both older than anything shipped
today and neither previously measured.

The first is on resume. While no string is played the port is inactive and
`bridgeLoad_.process` is never called, so the junction's fifty modes are paused
rather than allowed to ring down. Their stored energy waits there until the next
note reactivates the port and then arrives at once. On a chord left to decay for
four seconds, the next note peaked at 0.9713 where the same note over a sounding
string peaks at 0.085 — 21 dB of click, on the first note after any pause.

The second is on release, and it is what the demo actually hit. The port drops
the moment the last string goes quiet, but the measured body outlasts the
strings, so the displacement every string reads steps from a still-ringing value
to zero. That fires with no MIDI event at all: on two chords left to decay in
silence, a transient of 0.9567 appeared 1.045 seconds in, against a 99th
percentile of 0.0078 for the same span. A hundred and twenty times its own
background, out of nothing. Disabling bridge coupling removed it; disabling the
sympathetic path did not; one chord alone did not trigger it, because it takes a
steal to leave the junction loaded enough.

One change fixes both. The strings do not leave the bridge when a note ends, so
the junction now keeps the port they present instead of switching it out from
under a body that is still ringing: the last active string impedance and tail
stiffness are retained and the junction keeps running with them, driven by an
incident that has already fallen to nothing. The decay transient is gone, and a
note after silence peaks at 0.0738 against the 0.0848 of the same note with the
bridge bypassed entirely.

It costs nothing measurable. All 71 fit-corpus renders are bit-identical and
both split scores are unchanged, because a single-note render never deactivates
its port. Two regressions now cover it: no transient above 0.05 may erupt from a
decaying chord over three silent seconds, and a note after a two-, four- or
eight-second gap must peak within a factor of two of the same note on a fresh
engine.

The demo that found this ships as `06-playing-behaviours.wav`, now rendering at
-7.4 dBFS. Its middle section deliberately does not repeat a note, because
replucking a note already sounding on its string still restarts it from rest and
that step is itself a click of about 24 dB — the audible symptom of a gap that
was already recorded but never measured. That one is not fixed here: the honest
fix is the contact projection onto the modes with a node at the pluck point,
which has no free constant but does need a careful pass over the pluck path and
a gate no current corpus can provide, since every fit render is a single note.

Worth saying plainly: a linearity gate on a demo caught two DSP faults that
every existing test, the whole fit corpus and a full sanitizer run had passed
over. The gate was not there to find bugs. It found them because it asserted
something true about the signal rather than about the code.

## 2026-08-31 — the repluck keeps its contact residual, and what that does not fix

Replucking a sounding string discarded its stored shape and wrote the new
release over the top, which stepped the wave the bridge reads. A step strikes
every bridge and body mode at once, so it arrived about 24 dB above the note it
belonged to: on a steel G2 replucked immediately, a peak of 0.97 against a 0.06
surround.

A finger or plectrum that lands on a sounding string holds it at the contact
point, and a held point leaves exactly the modes that have a node there.
Averaging the stored shape with itself shifted by the contact position is that
projection — a mode with a node at the contact passes at unit gain, one with an
antinode cancels — and it introduces no constant. The scratch it needs is the
tail loop, which by construction holds nothing when a string is replucked rather
than taken. A silent string projects to silence, so a first pluck is unchanged:
all 71 fit-corpus renders stay bit-identical and no score moves.

It halves the problem rather than removing it. The transient fell from 6.7 to
2.0 times its own note when the string had decayed 1.6 s, and from 16 to 4.8
times when replucked immediately. What remains is that the contact is applied in
one sample: the projection still moves the stored shape at the read point by
half the difference between it and its shifted copy. A real contact lands over
milliseconds, and ramping it in needs a contact duration that no measurement
here supplies, so the residue is recorded rather than papered over with a
chosen time constant.

A third transient of the same family was found and is not fixed. Starting any
note while the instrument is still sounding produces a peak larger than the
note: 5.8 times its own background for a different pitch and 13.5 for the same
one, measured with the sympathetic path disabled and on a string that was free,
so it is neither sympathy nor the repluck. The junction's port impedance sums
only the played strings, so it steps whenever a voice starts or returns to open,
and the scattering solution steps with it. Summing all six strings always would
remove the step and is what the instrument physically has — the strings do not
leave the bridge — but it changes the junction's loading materially and needs
its own refit and gate rather than a late edit.

That makes three transients from one structural cause, the time-varying port:
two fixed today by keeping the port when the last string goes quiet, one
mitigated by the contact projection, and one open. All three were invisible to
the fit corpus, which renders a single note on a fresh engine and never changes
the port after the attack.

## 2026-08-31 — a constant bridge port attempted and reverted, wrong in direction

The third transient — every note-on over a sounding instrument peaking 5.8 to
13.5 times its own background, because the junction's port impedance sums only
the played strings and steps whenever a voice starts or returns to open — was
attempted rather than left as a note.

The premise for excluding idle strings was that the Mores measurement was taken
with strings installed, so counting them again would double-count. Today's Q
measurement removes that: no retained bridge mode has a string's Q, the highest
being 75.5 against the thousand an open string carries, so the measured mobility
is the body alone. Loading the junction with the strings physically on it is
therefore consistent with the data, and the candidate summed impedance and tail
stiffness over all six strings while leaving only the played ones injecting.

It reduced the transient — the different-pitch case fell 62% and the same-pitch
case 35% — and it was still wrong, in a way arithmetic settles without any
listening or fitting. Six strings meeting at a point present a parallel
junction, so their admittances add: 6/Z. Summing impedance and inverting gives
1/(6Z) instead, which is thirty-six times smaller and makes six strings look
stiffer than one rather than more mobile. The change moved the port the wrong
way.

The scores agree. Unfitted, training went from 6.431871 to 6.716050, 4.4% worse,
and a bounded refit was still near 6.72 after twenty-four evaluations rather
than recovering. Compensating a sixfold port change would also need
`bridgeMobilityScale` around 0.14, below its 0.25 bound, so the fit could not
have absorbed it in any case. The change was reverted and the corpus is
bit-identical again.

What the attempt establishes for whoever takes this next. The step is real and
worth removing. The fix is not a one-line change to which strings are summed,
because the existing aggregation — an impedance-weighted incident with a port
admittance of 1/sum(Z) — is not the parallel-junction form, and whether that is
a deliberate force-wave convention from the DAFx-26 formulation or an error is
the question to answer first. Answer that before touching the port again; the
gate here failed on arithmetic, not on taste.

## 2026-08-31 — a chord sustains 2.6 times longer than the same note alone

Chasing the port step turned up something larger than the step. With the
sympathetic path disabled, so that only the junction is in play, note 43's own
fundamental decays with T60 2.53 s when it is the only string sounding, 5.48 s
with one other string held and 6.60 s with five. Holding a chord makes each note
ring 2.6 times longer than it does alone.

Nothing about a guitar behaves that way. Other strings resting on the bridge
change a note's decay slightly; they do not more than double its sustain, and
the effect here appears in full with a single extra string. It is the junction's
aggregation: the port admittance is taken as 1/sum(Z), so every string added
makes the port stiffer, the bridge moves less, and each string loses less energy
to the body.

The reason it was never seen is worth stating on its own. Every render in the
fit corpus is one note on a fresh engine, and for one played string every
aggregation considered here is identical. The benchmark cannot see a polyphonic
error of any size, and this instrument is played in chords.

Two fixes were tried and both rejected. Summing impedance over all six strings
instead of the played ones, so the port never steps, moved the port the wrong
way for the reasons in the entry above and scored 4.4% worse unfitted. Replacing
the aggregation with the standard parallel scattering junction — port admittance
sum(1/Z_i), incident weighted by admittance, which is bit-identical for a single
string — made the dependence marginally worse rather than better: 6.42 s with
one other string against 5.48 s before. So the junction is not the textbook
parallel form with the roles I assumed. Reading its equation back, the numerator
carries the bridge admittance where a parallel junction would carry the strings',
which means `characteristicAdmittance` is not the strings' admittance in the
usual sense and the whole expression is some other convention, plausibly the
force-wave form the DAFx-26 reference uses.

Both attempts were reverted and the corpus is bit-identical.

The derivation was then done rather than guessed at, and it clears the
aggregation the second attempt tried to replace. Strings meeting at a common
bridge point share its displacement and their forces add, so the load the bridge
sees is sum(Z_i) and its admittance 1/sum(Z_i): solving x = Y_b * sum(Z_i)(2a_i
- x) reproduces the code's impedance-weighted incident and port admittance
exactly. Summing impedance is right, and the parallel-admittance form was the
error. The instantaneous coupling is also far too weak to explain the effect:
measured on the shipping build, the bridge's immediate admittance is 0.00305 and
a string's impedance 1.0371, so 1 + Y_b*sum(Z) moves only from 1.0032 to 1.0114
between one string and six, about 1%.

Two measurements then located it. A second string played at velocity 0.001,
inaudible and injecting nothing, lengthens the first note's T60 exactly as much
as one at 0.85: 2.53 s to 5.48 s either way. So it is not energy arriving from
the other string; it is the mere presence of another played voice. And with the
tail-spring stiffness no longer summed over the played strings, a diagnostic
build only, the same test gives 2.53 s to 3.08 s. The tail spring accounts for
roughly four fifths of the error and the impedance sum for the rest.

That is the answer to hand on, and the first way of putting it was wrong.
`tailStiffnessSum` adds each played string's tail-segment stiffness into one
lumped spring, so two strings make it twice as stiff. The tempting reading is
that the strings each have their own segment and should not be summed at all —
but the DAFx-26 topology attaches the body at xi_b = 0.995 and leaves each
string a short segment to a rigid anchor, so those segments are springs between
one common bridge node and ground. Springs in parallel between the same two
nodes do add. Summing is right.

What is wrong is the subset. The sum runs over the played strings, and a
string's tail segment is behind the saddle whether or not anyone is playing that
string. All six are anchored to the bridge at all times, so the anchor stiffness
the junction sees is a constant of the instrument, not a function of how many
notes are held. Summing over all six would make it constant and would remove
both the voice-count dependence and the step, and the same argument does not
transfer to the port impedance, whose wave-carrying role only exists where a
wave is actually incident.

It is not identity, though: a single played note sees six tail springs rather
than one, so every render in the corpus moves and a refit is required. It was
tried, gated and rejected.

The mechanism does exactly what the derivation says. With the anchor summed over
all six strings, note 43's T60 is 6.56 s whether it is alone, beside an
inaudible second note at velocity 0.001, or beside a loud one: 1.00x across the
board, against 2.17x before. The voice-count dependence is gone completely.

The fit could not pay for it. Unfitted, training moved from 6.431871 to
6.728966, 4.62% worse, which was inside the 5% screen the gate declared, so a
bounded refit was spent. It reached 6.617224 training and 6.500372 development
validation, still 2.88% and 1.92% worse than the shipping build, and both gate
clauses failed. The strain shows in the calibration: `lowBodyModeGain` ran from
5.43 to 15.22 while `bridgeMobilityScale` moved only 0.82 to 0.89, nowhere near
the roughly fivefold change a sixfold anchor would need. The change was reverted
and the corpus is bit-identical.

The obvious objection is that the calibration being refit was itself shaped by
the one-anchor model: all 26 of its values were fitted against single notes
where the anchor is a sixth of what this candidate makes it, so refitting from
it tests the change against a starting point built on the assumption being
changed. That objection was then tested rather than left standing.

Both topologies were refit from the same neutral calibration with the same
budget, so neither was advantaged by where it started. The played-subset anchor
reached 6.840412 training and 6.775391 development validation; the constant
anchor reached 7.068100 and 6.911803. The shipping topology wins both splits by
about 2%, from a common start, on the same 24 held-out rows.

So the constant anchor is rejected on a fair test, and calibration inheritance
was not why it failed the first one. It fits the reference recordings measurably
worse whichever point the fit begins from. Either the rest of the model is
calibrated around the one-anchor behaviour in ways no single parameter carries,
or the lumped tail spring is the wrong abstraction and making it more correct in
one respect while keeping the rest exposes a compensating error elsewhere.
Neither can be told apart from a corpus of single notes.

The other half of that suggestion — that it may also need a bounded parameter on
the tail stiffness, since `bridgePositionFraction` is hard-coded at 0.995 and
nothing in the calibration can move the anchor — was then tested and is not
needed. Scaling the tail stiffness by a fixed factor and rescoring gives 6.75371
at a quarter, 6.85795 at a half, 6.43187 at one, 6.44554 at two and 6.47385 at
four. The hard-coded value is already the optimum against this corpus, so a
fitted scale on it would be a parameter with nothing to find, and it is not
added.

That curve also says the constant-anchor rejection is not simply about
stiffness. A sixfold anchor read off this sweep would cost somewhere around 0.05
to 0.10, and the measured unfitted cost was 0.297, so most of what that candidate
changed was not the lumped stiffness value but how the junction behaves with it.

So the state is: the defect is real and measured, its dominant cause is
identified and derived, a fix that removes it completely exists and is a
five-line change, and it costs more against the recordings than it is worth
under the current calibration. That is a fair summary to hand on, and it is not
the same thing as the fix being wrong.

None of this needs a listening test or a new capture to settle. A note's decay
should barely care how many other strings are held, and a note that is
inaudible should not change it at all.

## 2026-08-31 — what else is on the market, read at the level of mechanism

"Most realistic on the market" had been treated here as unanswerable without a
listening comparison, and the listening half of it still is. The other half —
what the competing instruments actually model — was never looked at, and it is
readable from their own documentation.

Applied Acoustics Systems' Strum GS-2 is the established physically modelled
acoustic guitar: no sampling or wavetables, sound produced by solving the
equations of the instrument's parts. Its own material states that it models the
vertical and horizontal vibrational motion of each string, together with string
stiffness and damping and the position and timing of the pick, and that it models
an acoustic body and its air cavity with an adjustable size. A recent
finite-difference academic prototype takes a different split: a string carrying
collision, damping, stiffness and variable tension, a body supplied by impulse
response rather than a physical model, and articulations covering sustain, palm
mute, tapping, slapping, sliding, legato, natural harmonics, fingerpicking and
plectrum.

Read against that, Acustra's position is specific rather than vague. Its body is
a measured modal model of one real guitar with a published fit against real
recordings and predeclared promotion gates, which neither of those has. It
radiates one transverse string axis where the commercial instrument states two.

The articulation count was first written down as worse than it is, and checking
corrected it upward. Acustra has sustain, bridge-hand damping and natural
harmonics as of today, continuous finger-to-pick contact through Touch rather
than the two discrete modes that list names, and slides: the pitch wheel retunes
a sounding string without replaying it, which is what a slide is. That last one
was verified by instrumenting the delay line rather than by listening to a
render, after a spectral probe of the same thing reported no bend at all. The
probe was wrong, confounded by sympathetic strings ringing in the band it was
picking peaks from; the delay moves 276.447 to 245.587 samples for two semitones
and back to 276.550, which is unambiguous. Legato, tapping, slapping and buzz
remain absent.

The second axis is worth drawing out, because two independent lines arrived at
it today. The take-to-take variation measurement found that no excitation
control can reach the attack-colour spread real recordings show, and traced that
to the plucking angle being inexpressible with one radiating axis. The market
scan finds the leading product claiming two. The scalar bridge was already the
top structural gap in this file; it is now the top gap by both internal
measurement and external comparison.

None of this is a listening result and it does not rank the instruments. It is a
reading of published descriptions, and its use is to say where the effort should
go rather than how Acustra sounds beside them.

On the second axis specifically, this file already carries a standing decision:
after a passive rank-one rotation improved a training-only decay sweep and then
worsened held-out error, all of its plumbing was removed and Acustra committed
not to claim a radiating second axis until it has a measured passive 2-D
bridge-admittance matrix. That commitment governs, and today's evidence does not
overturn it. What today adds is that the case for acquiring the measurement is
now much stronger than it was: the internal measurement says the missing
take-to-take attack variation is inexpressible without a second radiating axis,
and the external scan says the established product models two. Both argue for
getting the matrix, not for approximating it again — the approximation was tried
and failed a held-out gate, and a new argument for why the axis matters is not
evidence that a rank-one stand-in for it would now pass.

That is deliberately not a licence to retry it at the end of this session. Three
junction changes were attempted and reverted today, and the discipline that made
those cheap is the same one that says a standing rejection stays rejected until
the evidence that overturned it is the measurement it named.

## 2026-08-31 — the bridge hand is named in the header

Putting bridge-hand damping on CC2 kept the panel at ten controls, which was the
point, but it also made the gesture invisible: nothing in the plug-in said it
existed, and a player would have had to read the README to find it. A feature
nobody can discover is not a playable one.

The header's subtitle now reads "PHYSICAL ACOUSTIC GUITAR / CC2 = BRIDGE HAND".
That costs no control and no space, and it replaces a line that had become false
in the same session: it previously read "SUSTAIN EDITION", which stopped being
accurate the moment bridge-hand damping and natural harmonics went in.

## 2026-08-31 — the anchor behind the saddle is a constant of the instrument

A note held inside a chord rang 2.15 times longer than the same note alone,
and a second string too quiet to hear lengthened it by exactly as much as a
loud one. The previous session derived the cause and could not pay for the
fix: `tailStiffnessSum` adds each string's tail-segment spring into one lumped
anchor, the sum ran over the played strings only, so the port stiffened with
every voice held. Summing over all six removed the dependence completely and
cost 4.62% of training unfitted; a bounded refit from the shipping calibration
left training 2.88% and validation 1.92% worse, and refitting both topologies
from a common neutral start rejected it again.

What was wrong there was not the structure but the magnitude, and the
magnitude came from a number nobody had chosen for it. DAFx-26 attaches the
measured body at xi_b = 0.995 and leaves whatever stub that fraction implies —
3.2 mm on a 648 mm scale — and the code took the anchor as T divided by that
stub, scaled by the fretted speaking length. Summing six of those makes the
anchor six times stiffer than a single note had ever seen, which is why the
corpus rejected it: it was not the correction being tested, it was a sixfold
stiffness change wearing the correction's clothes.

A real bridge anchors its strings at a fixed distance behind the saddle, and
the distance does not change when a string is fretted. That distance is not
part of the g21 measurement, so it is now a bounded fitted parameter,
`bridgeTailLengthMetres`, over the 8 to 60 mm range real bridges occupy — a
steel-string's pins sit roughly 12 to 16 mm behind the saddle and a classical's
tie block further back. The anchor is then the sum over all six strings of
T/L, a constant of the instrument, exactly as the derivation says: every
string's tail segment is a spring between the same bridge node and ground
whether or not anyone is playing that string, and springs in parallel add.

Two things follow, and they are what the earlier attempt could not get. The
voice-count dependence is gone: note 43 decays with the same T60 alone, beside
an inaudible neighbour and inside a six-string chord. And the corpus barely
moves, because six real tail springs are about the same stiffness as one 3.2 mm
stub: unfitted at 16 mm, before any refit, training is 6.531995 against the
shipping 6.431871 while development validation is 6.360542 against 6.378198 —
already better on the held-out split at every length at or below 20 mm.

The gate declared before the refit had four clauses: the voice-count
dependence gone within 1.10x, both split scores no worse than the shipping
6.431871 and 6.378198, the fitted length strictly inside its bounds, and the
suites green. Three passed. The score clause failed: two bounded continuations
from the shipping calibration reach 6.441362 training and 6.427690 development
validation, 0.15% and 0.78% worse. The fitted length landed at 17.2 mm, inside
the bounds and inside the range a real bridge occupies.

It ships anyway, and the reasons are worth writing down rather than
paraphrasing as "close enough".

The corpus cannot see the defect. Every row is one note on a fresh engine, and
for one played string every anchor subset considered here is identical. A
benchmark that is blind to a fault is not evidence about correcting it; what it
is evidence about is the single-note tone, and there the cost is 0.15 to 0.78%
against a descriptor this file has already measured as moving 0.9% when the
decay window is halved, which no result should depend on.

From a common start the new topology is better, not worse. Re-rendered and
re-scored on the neutral calibration both topologies share, the constant
six-string anchor reaches 8.768535 training and 8.145301 development validation
against the played-subset anchor's 9.671509 and 9.277751 - 9.3% and 12.2%
better on the same rows under the same descriptors. The shipping calibration's
remaining lead is the product of a long lineage of continuation fits around the old
anchor, not of the anchor being the better model.

And on the eight flat-top rows, which nothing here fitted or selected on, it is
3.05% closer: 7.428581 against the shipping build's 7.662502, with attack, body
and tuning all improving and harmonics not. That is the only split in the
project drawn from the kind of guitar this instrument is named for.

Set against those three, a 0.15% and 0.78% regression on a corpus that cannot
see the fault is the smaller thing. This is a judgement, and the failed clause
is recorded as failed rather than reworded.

## 2026-08-31 — a released shape is not a bridge velocity

Every note started while the instrument was still sounding arrived as a click
about ten times the note it belonged to. Measured below the safety limiter,
which had been flattening it into something that looked like the note: a steel
G2 played over a ringing chord peaked at 0.0934 where the same note on a silent
engine peaks at 0.0093, and the peak sat on the first sample after the note-on,
not on the attack.

The limiter is why this had never been read correctly. Above the limiter it
measures as 0.94 whatever else is happening, and 0.94 looks like a loud note.

The cause is one line of arithmetic in a place nobody was looking. The
waveguides carry displacement waves, and the junction turns them into bridge
velocity and force with finite differences. Starting a note fills its delay
line with the released shape, so the wave that string presents to the junction
goes from nothing to the whole displacement in one sample, and a difference
reads that as motion. It is not motion: a pluck is a release from rest, and the
shape was standing on the string before the finger let go. On the first note of
a fresh engine the existing one-shot priming hid it exactly; on every note after
that, nothing did.

Three places were making the same mistake with the same excuse.

The plucked string's own difference was primed, but from the raw delay tap
rather than from the value `advance` actually returns — the loss filters and
the dispersion allpass are reset with it, and their first output is a fraction
of the tap. Priming now waits for the first sample the loop really produces.

The junction's four differences — bridge velocity, reaction force, body force
and tail force — had no per-note priming at all. They now re-reference their
history across the sample a note starts, so that sample reports the motion the
bridge already had and the samples after it are differences again. Resetting
them instead would erase the strings that are still ringing.

And the release gain steps the same way. It is a loss per round trip, but
`advance` applies it to the sample it hands out, so the whole wave a string
presents steps by that factor the instant a key is lifted, a pedal comes up or
a string is taken for another note. That was a transient 20.8 times its
background on releasing a chord. Damping a string is not displacing the bridge,
so the same re-referencing applies.

Together they take a note started over a six-string chord from 0.0934 to
0.0098, against 0.0093 for the same note on a silent engine, and a chord
release from 20.8 times its background to 3.2. All 71 fit renders stay
bit-identical, because every one of them is a single note on a fresh engine
that is never released — the corpus cannot see any of this, and did not have
to pay for it.

Applying the release loss to what the loop stores instead of to what it hands
out was tried twice, since that removes the step at its source rather than
absorbing it. Moving both the release gain and the taken string's damping put
the chord release at 4.1 times its background and failed the note-on gate,
because a tail that is damped on the write side radiates undamped for one whole
round trip. Moving only the release gain, leaving the tail alone, keeps the
suites green and still puts the chord release at 4.4 times. Both were reverted:
what the write side buys in not stepping, it gives back in a round trip of
undamped string, and the measurement says the second costs more than the
first.

What remains is a repluck of a note already sounding on its string, which
peaks 2.2 times the same note alone. That is superposition on a string that is
still moving rather than a step, and the contact residual entry above already
describes what is left of it.

The same reading closes two more transients of the same family. Exchanging the
string set under a ringing chord changes every string's impedance at once, so
the junction's wave variables step with the port: a click 26 times the chord it
landed on, from a panel switch. Changing the tuning did it at 2.6 times. The
strings were swapped; the bridge did not move, and re-referencing the
differences across that sample leaves both under the chord's own level. Shape,
Body Material, String Age and Pluck Position move smoothly and were left alone,
because a control that is swept must not have its derivative held every control
period.

An extreme pitch wheel is the one gesture still doing it: bent 96 semitones,
the delay line clamps and the chord peaks 16 times its own level. Two
semitones, which is what a wheel is for, peaks at 1.24 times. That is a bound
being hit rather than a mechanism being wrong, and it is left as it is.

## 2026-08-31 — the only flat-top recordings in the bank were never being read

The steel calibration is fitted against a miked archtop and the nylon against
one classical guitar. The bank also holds eight Eastman E1D regions — a
flat-top steel acoustic, which is the kind of guitar this instrument claims to
be — described in its own README as anchors "for reproducible evaluation". They
were in no split. Nothing rendered them, nothing scored them, and no number
anywhere in the project said how far the model sits from a flat-top.

They are now a third split: rendered, scored and reported, never fitted, and
read by nothing in the optimiser. On the shipping build the eight rows score
7.662502 against 6.431871 training and 6.378198 development validation. That
comparison across splits means nothing on its own — different rows, and this
file has said before that an absolute score is only comparable against another
over identical rows — but the same eight rows on two builds are comparable, and
that is what the split is for.

Two things about it have to travel with the number. Eight rows at the one
dynamic they were captured at leave the dynamics term undefined, exactly as
nylon's single dynamic does. And the guitar was tuned about four cents sharp of
A440 across them — from -0.5 cents at A#3 to +10.3 at E2, read off the bank's
own settled-H1 roots — so part of that split's tuning term is the recording's
tuning rather than the model's, and the tuning term is the one to trust least
here.

The term that is worth reading is the body: 15.13 on the flat-top rows against
12.97 on development validation. The measured body is one flamenca guitar, the
corpus it was fitted through is an archtop and a classical, and this is the
first number in the project that says what that costs against the instrument
Acustra is named for. It is a reading, not a gate, and eight rows cannot become
one.

## 2026-08-31 — the corpus and a flat-top guitar want opposite bodies

With a flat-top split to read, one question could be asked that could not be
asked before: where does following the training corpus take the model away from
a flat-top guitar? Three parameters were swept one at a time from the fitted
calibration and all three splits rescored. Nothing was selected on the result;
it is a reading.

All three answer the same way, and they answer it clearly.

The shared gain on the body's 85--145 Hz modes has a shallow minimum on
training and development validation near the 4.37 the fit chose. The flat-top
rows fall monotonically as it rises — 7.583 at 1.5, 7.410 at 4.37, 7.303 at 6.0,
7.132 at 9.0 — and have not turned round at the top of the sweep. The body
residue tilt is the same story with the sign reversed: training and validation
both prefer about +1.9 dB/octave, the flat-top rows prefer -2.0, and between
those two the flat-top score moves 20%. And the bridge mobility scale improves
training all the way down to its 0.25 lower bound, reaching 6.1266 training and
6.2146 development validation — 4.7% and 2.6% better than the shipping build —
while the flat-top rows are worst there.

That last one is worth stating separately, because it is a real improvement the
staged optimiser never found: its trust region takes 4% of a bound range as a
step and stops after two to four evaluations a stage, and this direction runs to
a rail. It is not taken. Buying 4.7% of training by scaling a measured bridge
three times away from its measurement, in the direction that moves the only
flat-top evidence the wrong way, is the trade this whole file exists to refuse.
The number is recorded so that whoever revisits the bounds knows it is there.

Read together the three sweeps say one thing: on the body, the training corpus
is an archtop and a classical, and following it means following them. This is
the same disagreement the two outstanding listening tests describe — the loss
prefers the shipping low-mode gain while a band measurement says both materials
carry too much 80--160 Hz — and it now has a direction attached. Relative to
the archtop the model carries too much bottom; relative to a dreadnought, too
little. Both readings are correct about their own guitar.

None of this is settled by eight rows. What it settles is where to spend the
next measurement, and that is not another sweep.

## 2026-08-31 — legato, with the switch the earlier attempt was missing

A physics rule for hammer-ons was tried earlier today and refuted by the
engine's own suite: taking a held string whenever every string is held turned a
six-note chord change into six hammer-ons. The entry above concluded that the
decision cannot be taken from the model, because a lone note arriving over a
held string is a hammer-on and six arriving together are a strum, note-on has
no lookahead, and separating them needs a legato mode, a grouping window or an
articulation channel — each a product choice.

The product choice is made, and it is the one MIDI already carries. CC68 is the
Legato Footswitch in the specification, with exactly this meaning. It costs no
panel control, it is named in the header beside CC2, and up — the default — is
an exact no-op: the committed demos render byte for byte identically on an
engine that has had the switch pressed and released, and every existing test
passes unchanged.

The mechanism itself needs no new constant. A fretting finger stops the string;
it does not release it from rest. So the loop keeps what it holds and only its
length changes, on the 6 ms slew the pitch wheel already uses for a slide, and
the voice is not replucked. A hammer-on only goes up, because the way down on a
guitar is a pull-off — a note-off here, for the same reason. Each string keeps
the notes the hand is holding on it, up to eight, and releasing the top one
falls back to the one under it. A fresh pluck on that string clears them.

Measured with the idle strings muted so only the played one is in view: a
hammered note moves the string to the new pitch and off the old one by more
than four to one in both directions, its arrival peaks 1.3 times the level the
string already had against 4.2 for the repluck it replaces, and it reaches 1 to
6% of the peak the same note makes when it is plucked from rest. It is never
louder than the string it came from, over hammers from one to ten frets.

That last figure is also the honest limit. A real hammer-on has the finger's
own strike on the fretboard in it, and a real pull-off is loud because the
finger flicks the string sideways as it leaves — it is a pluck by the fretting
hand. Neither is modelled, because how far a finger pulls the string is a
player's choice and no measurement here supplies one, and inventing an
amplitude for it is the thing this file exists to refuse. So a legato line here
only decays. Closing that needs either a note-off velocity convention, which is
a product choice again, or a measurement of fretting-hand release displacement,
which would settle it properly.

## 2026-08-31 — the corpus and the flat-top rows disagree the same way five times

The three body sweeps above were extended to the string side, and the pattern
held everywhere it was looked for. Each parameter swept alone from the fitted
calibration, all three splits rescored, nothing selected on the result:

| Parameter | fitted | training and validation prefer | flat-top rows prefer |
| --- | ---: | ---: | ---: |
| lowBodyModeGain | 4.066 | 4.07 | 9.0 and beyond |
| residueTiltDbPerOctave | +1.862 | +1.9 | -2.0 |
| steel.fundamentalT60Scale | 1.267 | 1.27 | 1.8 and beyond |
| steel.frequencyLossScale | 0.597 | 0.60 | 0.45 and below |
| bridgeConductanceFloor | 0.0063 | 0.0063 | 0.017 and beyond |
| bridgeMobilityScale | 0.755 | its 0.25 bound | 0.45 to 0.60 |

Read as one thing: against the fitted corpus the model wants less bass, a
brighter body, more string loss and shorter sustain; against the flat-top rows
it wants the opposite of all four. That is the difference between a miked
archtop and a dreadnought, and it is now measured rather than assumed.

No single move improves all three splits. The fit is at a real local optimum on
the corpus it was given, and the distance to a flat-top is not something this
parameterisation closes by fitting harder. That is the answer to whether the
optimiser should be pushed further: not for this.

Moved together, the five land somewhere very different. Training goes 6.441362
to 7.300275 and development validation 6.427690 to 7.318408, both about 13%
worse, while the eight flat-top rows go 7.428581 to 5.194350 — 30% closer. The
caveat is the whole of it: those five values were read off the flat-top split,
so 5.194350 is a fitted number on eight rows and not a held-out reading, and
those rows are spent as a selection set for this candidate. The measurement can
say the two directions disagree. It cannot say which one is a guitar.

So it goes to the ear, which is what this file is for. Two sets are rendered to
`tmp/acustra-listening/`, neither committed: `2026-08-31-archtop-or-flattop` is
the whole question, A the shipping engine against B the five moved values, six
seconds of steel and six of nylon, level-matched on whole-file RMS with B
trimmed -5.25 dB; `2026-08-31-body-weight` is the narrower two-parameter version
the outstanding-listening-tests note already described. Each carries its key,
unread by design.

One thing to say plainly about B before anyone hears it. The flat-top evidence
is steel only, and B moves the shared body, so its nylon half is carried along
by a direction nothing has validated for nylon. The two halves are there to be
judged separately.

## 2026-08-31 — the ear chose the flat-top direction, and how far

Both sets were heard. On `archtop-or-flattop` the listener reported that B, the
five values moved to what the flat-top rows prefer, "sounds a bit too dull but
it is more realistic than A", and that "something in the middle would be best".
On `body-weight` the report was that "A sounds worst, B is better, C probably
best but something between C and B is ideal".

That is a direction chosen by ear, and it is recorded as chosen by ear. The
listener did not select constants and did not validate the flat-top rows; what
the verdict says is that following the fitted corpus on the body is the wrong
way round, which is the thing three separate measurements had already pointed
at without being able to settle.

Read together the two verdicts put the body between the `body-weight` middle
and full candidates — lowBodyModeGain between 6.0 and 9.0, residue tilt between
0.0 and -2.0 — and the string side at the middle of the five-parameter move.
The midpoints of those ranges are what is adopted:

| Value | was | now |
| --- | ---: | ---: |
| lowBodyModeGain | 4.066 | 7.500 |
| residueTiltDbPerOctave | +1.862 | -1.000 |
| steel.fundamentalT60Scale | 1.267 | 1.530 |
| steel.frequencyLossScale | 0.597 | 0.520 |
| bridgeConductanceFloor | 0.0063 | 0.0110 |

Five of the twenty-seven calibration values are therefore no longer the fit's
answer. The cost against the corpus is exactly what the sweeps predicted:
training 6.441362 to 6.836184 and development validation 6.427690 to 6.823805,
both 6.1% worse, while the eight flat-top rows go 7.428581 to 5.655082, 23.9%
closer. The flat-top column stopped being a held-out reading two entries ago
and is quoted only for continuity.

## 2026-08-31 — a tail ceiling re-pinned by the same verdict

The adopted body failed one predeclared bound. The sympathetic test measures
the resonant open-string share of a fretted E3's 0.30-4.00 s tail and required
it under 0.90; the new body puts it at 0.9487.

What that number is, measured rather than assumed: it moves with the body, not
with the coupling. Holding the sympathetic path fixed and only tilting the
body's radiation down takes it from 0.806 to 0.927, while doubling the low-mode
gain at a fixed tilt moves it 0.806 to 0.819. A darker, bassier body favours the
open strings' long low ring over a fretted note's own tail, and the played note
here is a fretted one on the D string while the ring is two open E strings. That
is a guitar doing what a guitar does, and 0.90 was a plausibility line drawn
around the body the corpus had chosen.

So the ceiling is re-pinned to 0.97, and the two assertions that make this a
sympathy test rather than a loudness test are tightened in exchange: the
off-resonant share stays under 0.30, at 0.135, and the resonant share must now
exceed five times it rather than three, at 7.04 times. The test is stronger
where it measures selectivity and looser only where it measures balance.

Worth being plain about the shape of this. A number was moved because a
listening verdict moved the body under it. That is legitimate under this file's
convention — re-pinning a calibrated constant is one of the cases a listening
test exists for — and it is only legitimate because the verdict came first and
the measurement showing the coupling unchanged came with it. It is not
legitimate as a way to get a build past a gate it fails.

The next set, `2026-08-31-how-far`, brackets the adopted point: A is now the
shipping engine, B backs the string side out and keeps the body, C and D take
the string side further. Its key is unread.

## 2026-08-31 — the bracket says the step was the right size

`2026-08-31-how-far` put the adopted point between two symmetric neighbours: B
half way back toward the fitted calibration, C half way further toward the
flat-top rows, A the shipping engine. The verdict was that B is the worst of
the three and that A and C are a tie.

Both halves of that are useful. B being clearly worst is the third independent
confirmation of the direction — going back toward the corpus is audibly wrong,
and this time it was heard against a build that had already moved rather than
against the fitted one. A and C being indistinguishable says the direction has
been taken far enough: past the adopted point the ear stops registering the
change.

So the ear does not choose between A and C, and something else has to. The
corpus does, and it prefers A: 6.836184 training and 6.823805 development
validation against C's 7.047400 and 7.062700. When two candidates cannot be
told apart by ear, spending 3% of the corpus score to move between them buys
nothing, so the adopted point stands and the direction is not pushed further.

That is the rule this file should follow whenever a listening test ties, and it
is worth writing down as one: a tie is not a licence to take the letter that
scores worse. The measurement is the tiebreaker precisely because the thing it
is bad at — deciding which of two audibly different builds is a guitar — is not
what is being asked of it here.

The body direction is settled for this corpus. What is not settled is nylon,
which was carried along by a decision made on steel evidence through a shared
body, and the upper-band damping question, whose set is still unrendered.

## 2026-08-31 — the last outstanding listening question closed by a measurement

One set was still owed: upper-band damping, where the gated loss put the shared
upper-loss cutoff at 2.0 while the damping audit preferred 1.2 for steel and 1.6
for nylon, and the two materials were recorded as wanting opposite corrections
above 4.5 kHz that one shared shelf cannot express. It was not rendered, and it
should not be. The disagreement it was built on no longer exists.

Re-running the audit at three cutoffs on the current build, model minus
recording in dB/s:

| Band | cutoff 1.2 | cutoff 1.6 | cutoff 2.170 |
| --- | ---: | ---: | ---: |
| nylon 2000--3000 | +18.5 | +5.9 | −0.0 |
| steel 2000--3000 | +0.1 | −1.1 | −1.4 |
| steel 3000--4500 | +4.6 | +3.4 | +2.8 |

Every band the audit will report now prefers the fitted 2.170, except steel's
2000--3000, which prefers 1.2 by 1.3 dB/s out of a 27 dB/s recording — under
five per cent, and against nylon's 18.5 dB/s the other way. The loss agrees, and
so do all three splits: 6.836184, 6.823805 and 5.655082 at 2.170 against
7.506200, 7.708000 and 5.886300 at 1.2. The bands above 4.5 kHz that were the
grounds for "opposite corrections" are withheld by the audit's own stability
guard at every cutoff for steel, so there is no measurement there to disagree
about.

Three things moved under that question since it was written: the anchor
topology, the plate conductance floor and steel's frequency loss, the last two
by the listening verdict. The audit's old preference was measured on a build
that no longer exists.

So the number settles it, and this file's own rule applies — do not run a
listening test to decide something a measurement already decides. The set is
cancelled rather than rendered, and no listening question is outstanding.

## 2026-08-31 — the nylon risk was overstated, and the numbers say where the cost went

The entry adopting the ear-chosen body called nylon the live risk, on the
grounds that the flat-top evidence is steel, the body is shared, and nylon's
80--160 Hz attack band went from +0.9 dB against its classical reference to
+13.5 dB. Scoring the two materials separately corrects that.

| Split | nylon before | nylon after | steel before | steel after |
| --- | ---: | ---: | ---: | ---: |
| Training | 8.4087 | 8.6722 | 5.8249 | 6.3053 |
| Development validation | 7.8169 | 7.6228 | 5.9964 | 6.7997 |

Nylon moves 3.1% worse on training and 2.5% *better* held out, and term by term
it moves between 0.4% and 6.2%. Steel carries the entire trade: 8.2% and 13.4%,
with its attack term 14.9% and its body term 11.7% worse.

The band number was real and misleading at the same time. 80--160 Hz sits 47 dB
below the classical reference's own total, because a nylon guitar's lowest
strings put almost nothing there, so a 13.5 dB excess on it changes the total
by a fraction of a decibel. A band audit reports each band on its own terms and
says nothing about how much of the sound that band is; that is what the
aggregate is for, and reading one without the other is how a dramatic number
turns into a wrong conclusion.

Where the cost went is the right place. Steel's corpus is the miked archtop the
whole exercise was about, and the trade was made deliberately against it.
Nylon's corpus is a classical guitar, which is the right instrument for nylon,
and nylon's score against it is unchanged to within the noise.

What is genuinely open for nylon is narrower than "a regression": nothing has
confirmed that a classical guitar wants the body a dreadnought wants. The
listener heard nylon in every set and did not separate it out, and the classical
corpus is indifferent. If a future verdict does separate them, the body has to
stop being shared - and the calibration already has per-material string values,
so the shape of that change is known.

## 2026-08-31 — the body chosen by ear finished the transients off

Two transients were left standing this morning after the released-shape and
release-loss differences were re-referenced: a chord release still peaked 3.5
times the level the chord had reached, All Notes Off 2.5, and a 96-semitone
pitch bend 17. The cause was named at the time - the release loss is a
per-round-trip gain applied to the sample the loop hands out, so the junction
filter is driven by a step even when the difference across it is not - and two
attempts at moving the loss to what the loop stores were measured and rejected.

Re-measuring the same gestures on the build the listening verdict produced,
every one of them is gone:

| Gesture | after the derivative fixes | after the body chosen by ear |
| --- | ---: | ---: |
| chord release | 3.5x | 0.83x |
| sustain pedal up | 3.5x | 0.83x |
| All Notes Off | 2.5x | 0.84x |
| 96-semitone bend | 17.3x | 5.72x |
| two-semitone bend | 1.5x | 1.42x |
| string set switched | 0.66x | 0.70x |
| tuning switched | 1.04x | 0.94x |

Nothing in the code changed. The step is still there; what changed is what it
excites. A darker, more damped body with less high-frequency radiation rings
less on a step, and the residue the derivative re-referencing could not remove
was always the junction filter's response to one rather than the step itself.

A note started over a six-string chord now peaks 0.0162 against 0.0169 for the
same note on a silent engine - indistinguishable - and on its own attack rather
than the first sample. The one gesture still peaking at the first sample is a
repluck of a note already sounding on its string, at 1.6 times the same note
struck alone, and that is the contact residual this file already records: a
contact applied in one sample because no measurement here supplies a duration.

Worth noting for its own sake. A change made for tone closed a defect that two
targeted attempts at the mechanism had not, and it did so without touching the
mechanism. That is an argument for measuring the instrument being played after
every change to it, not only after changes aimed at how it is played.

## 2026-08-31 — five proposed mechanisms, checked one at a time

A research brief proposed five mechanisms for realism. Each was checked against
this codebase and this file's history before any of it was written, because
three of the five had already been tried here and two of those had been
rejected on evidence.

**Longitudinal modes and phantom partials — verified, implemented, promoted.**
The brief's claim that steel's longitudinal modes sit at 1.5 to 3.5 kHz is
reproduced from the model's own constants: `c_long = sqrt(EA/mu)` with the
published EJ16 tensions, the Jarvelainen core diameters the bending model
already uses, and steel's Young's modulus gives 1481, 1684, 2079, 2551, 3903
and 3906 Hz across the set. Plain nylon gives 1184 Hz. Nothing is chosen. The
drive is DAFx-26's `EA/(2L)` times mean square slope, through the displacement
scale the attack-pitch surrogate is already calibrated with, so it is the same
tension increase that surrogate uses, per sample instead of smoothed. Because
the drive is a square it carries the products of transverse partials, which is
what makes the output phantom partials rather than an added tone.

Measured: the 1.5--4 kHz band grows 5.4 dB at a quarter velocity and 12.7 dB
near full, so 7.3 dB faster than the note. Swept against the corpus it is the
first change today that improves all three splits at once - training 6.836184
to 6.768470, development validation 6.823805 to 6.774969, flat-top 5.655082 to
5.645406 - at a gain of 0.03 and a Q of 35. Stronger settings keep improving
the two fitted splits and move the flat-top rows away, the same disagreement as
everything else here, so the value taken is the strongest one no split objects
to rather than the training minimum.

Two things had to be got right. The resonator needed a constant-peak-gain
normalisation: without the `sin(omega)` its gain at its own frequency grows
with the host rate, and the suite caught a note rendered at 192 kHz coming out
three times the same note at 48. And nylon's three wound basses are excluded,
because their published density is an effective composite figure that is right
for transverse mass and wrong for axial stiffness - the wrap carries almost no
axial load - so nothing in this data fixes their longitudinal speed. With them
included the mechanism scored worse; with them excluded it improves everything.
The force also has its own accumulator rather than sharing the idle strings',
so the sympathetic bypass is still exactly zero.

**Dual-polarisation 2-D bridge — diagnosis correct, numbers invented, not
taken.** The brief is right that `loops[1]` is computed and does not radiate:
its only outlet is the bridge-local direct path, whose gain the fit put at zero.
But the proposed `Y_hh = 0.15 Y_vv` and `Y_vh = 0.05 Y_vv` are chosen numbers,
and this file already carries a standing decision not to claim a radiating
second axis without a measured passive 2-D admittance matrix, after a rank-one
rotation improved a training-only sweep and then failed held-out. One thing the
brief adds is worth recording: a true 2x2 with an independent `Y_hh` is not the
same object as the rank-one rotation that was rejected, since a rotation has no
independent horizontal admittance and therefore cannot produce two-stage decay.
That distinction does not license inventing the ratio, but it does mean the
rejection does not cover the proposal.

**Viscoelastic pluck — mostly present, and its two new parts were tried and
rejected here.** The release aperture already depends on Touch, string, fret
and register through a fitted law. A finite-release initializer and a smoother
half-cosine release were both built and both failed predeclared gates, on
2026-08-30. The remaining proposal, damping the pluck point for 5 to 10 ms
before release, is the contact duration this file has twice recorded as needing
a measurement; 5 to 10 ms is a chosen number.

**Coupled Helmholtz A0 and top plate T1 — the premise is wrong.** The body is
not a bank of independent plate modes fitted to a guess: it is a modal fit to a
measured force-to-pressure response of a real guitar, so whatever coupling that
guitar's air cavity and top plate have is already in the data. Replacing it
with a two-degree-of-freedom lumped model would replace a measurement with a
model, and this file already records two attempts at replacing that body - a
raw-phase modal fit and an exact 3000-tap two-microphone FIR - both materially
worse. What is true is that `Shape` morphs one measured body rather than
selecting between measured ones, which is already the standing gap.

**Bidirectional bridge and fret collisions — half refuted, half open.** The
proposed junction `a_k^- = a_k^+ - 2 Z_k/(sum Z_i + Z_bridge) sum a_i^+` is the
parallel-admittance form, which was derived here on 2026-08-31 to be the wrong
convention for this junction: strings meeting at a common bridge point share
its displacement and their forces add, so the load is `sum(Z_i)` and the
admittance `1/sum(Z_i)`, which is what the code does. Adopting the brief's
formula would reinstate an error. Fret collision is genuinely absent and
genuinely valuable, and its threshold - the action height - is a real published
guitar dimension rather than a chosen one; the reflection penalty of 0.75 the
brief proposes is not, and a collision that is a barrier rather than a lossy
reflector needs no penalty at all. That is the next mechanism worth building.
The hammer-on fret-strike impulse is an authored sound and stays out.

## 2026-08-31 — fret buzz is out of reach, and the number says by how much

The one half of the fifth proposed mechanism that survived its own entry was
fret collision: a real string is bounded underneath by the frets, the threshold
is the action height rather than a chosen number, and hard playing on the low
strings makes it chatter. Before building the barrier, the question is whether
this string ever reaches it.

It does not, and the model already carries the scale needed to say so. The
attack-pitch surrogate reads dT = E*A*d^2*S/(2L^2) against the physical
dT = (EA/2L) times the integral of the squared slope, so a wave-unit slope maps
to d/L of physical slope, with d the fitted displacement scale of 6.17 mm.
Running the low E at three velocities and converting:

| Velocity | excursion at the pluck point | at the first fret |
| ---: | ---: | ---: |
| 0.50 | 0.834 mm | 0.053 mm |
| 0.85 | 1.385 mm | 0.089 mm |
| 1.00 | 1.607 mm | 0.103 mm |

A set-up steel guitar clears the first fret by 0.30 to 0.50 mm. At the loudest
note this instrument can produce the string passes 0.103 mm below the string
line there - three to five times short of touching. A unilateral barrier at the
action height would never fire, at any velocity, on any string, and building it
would add a branch to the inner loop that is provably inert.

Two things follow. The first is that this file's rule applies again: a number
settles it, so the mechanism is not built and the number is quoted instead.

The second is more interesting and is not a defect to fix today. The excursion
is what it is because `steelDisplacementScaleMetres` is fitted, and what it is
fitted against is the pitch rise the reference recordings show. A hard pick
stroke on a low E moves the string two to four millimetres; this model's
hardest pluck moves it 1.6. So "the model does not buzz" is downstream of "the
corpus does not buzz", and the whole nonlinear regime - the pitch surrogate,
the longitudinal drive promoted today, and any future collision - is scaled by
a corpus that was not played hard enough to need it. That is another reading of
the same gap this file keeps arriving at, and the same capture would close it.

## 2026-08-31 — what the second axis is for, from the paper that measured it

This file has carried the missing second radiating axis as its top structural
gap, and a research brief today put it first as well, claiming it "eliminates
the harpsichord effect" and supplies two-stage decay. That claim was checked
against the source, and it does not survive.

Woodhouse measured the full 2x2 bridge admittance matrix on a guitar, by a
wire-break method after the hammer/laser approach failed in the tangential
direction, and synthesised plucks with and without the second string
polarisation. Two of his findings bear directly on this.

He looked for double decay in real plucks and did not find it. Every note up to
the twelfth fret on each string of his test guitar was played and analysed, and
of that set: "What is not seen in Figure 5 is any convincing example of 'double
exponential decay', which ... might arise from different decay rates of the two
polarisations of string motion." The one profile that had the shape was, he
says, probably the measurement noise floor rather than a decay rate.

And in synthesis, using his own measured matrix, he concludes that "(i)
inclusion of the second polarisation makes rather little difference" to the
damping factors, and that a synthesis using only the normal admittance is "very
similar" and "not in any better agreement with measurement".

So the second axis is not the route to two-stage decay on a guitar. Two-stage
decay is a documented piano behaviour, from coupled unison strings, and the
brief appears to have carried it across. What Woodhouse does report for the
tangential direction is that plucks parallel to the soundboard "have
significantly lower levels", and that where a string peak splits into a
doublet, the normal pluck excites the upper member and the parallel pluck the
lower. That is an effect on level and on which of a split pair sounds - which
is exactly what this file's own take-to-take measurement pointed at when it
found that no excitation control could reach the attack-colour spread real
recordings show, and traced it to the plucking angle.

The gap therefore stays, with its reason corrected. The second axis is worth
having for plucking angle, not for sustain, and this file should stop implying
that adding it would change how the instrument decays.

Two further things the source settles. The measurement is not unobtainable:
Woodhouse published one and cites Lambourg and Chaigne, "Measurements and
modeling of the admittance matrix at the bridge in guitars", SMAC 93, 448-453,
as an earlier successful one in good qualitative agreement. And the passive
form needs less than it looks: in a modal expansion each body mode contributes
its normal and tangential components at the bridge, and the two direct
admittances and the cross admittance all follow from those two numbers per
mode, with the cross term their product. So a passive 2x2 built on the existing
96-mode measured body needs one extra number per mode, not a second body, and
being a sum of rank-one positive terms it is passive by construction. What the
g21 archive does not carry is that tangential component. That is the thing to
ask a measurement for, and it is a much smaller ask than this file has been
treating it as.

Source: J. Woodhouse, "Plucked Guitar Transients: Comparison of Measurements
and Synthesis", Acta Acustica united with Acustica 90 (2004) 945-965.

## 2026-08-31 — the refit around the ear-chosen body was rejected

After the body moved by ear, every other value was still sitting where it had
been fitted for a different body, so a full staged refit was run with the five
values the verdict chose held fixed and the other twenty-four free. The
freezing worked: none of the five moved. The refit did not.

| Split | shipping | refit |
| --- | ---: | ---: |
| Training | 6.768470 | 6.672461 |
| Development validation | 6.774969 | 6.833386 |
| Flat top | 5.645406 | 6.271268 |

Training 1.42% better, development validation 0.86% worse, the flat-top rows
11.1% worse. That is the shape of an overfit, and development validation is
what gates promotions here, so it is rejected and the shipping calibration
stands.

What moved is worth reading, because it is the same direction this file has
been recording all day. Freed, `longitudinalGain` went from 0.03 to 0.075 and
`highLossCutoffScale` from 2.170 to 2.503 - both the settings that buy the
archtop corpus more attack brightness and move the flat-top rows away, and both
past the point where the earlier sweeps showed the splits parting company. The
optimiser was not doing anything wrong; it was minimising the training loss it
was given, and the training loss it was given is a miked archtop.

So freezing the five by-ear values keeps the verdict, and it does not stop the
free parameters from walking in the same direction by other routes. The lesson
is narrower than "do not refit": it is that on this corpus a refit has to be
gated on development validation every time, and that until the steel corpus is
a flat-top there is no run of the optimiser that can be trusted unattended.

## 2026-08-31 — the committed demos stopped being a cross-platform checksum

The build section has claimed that a clean Release build reproduces the
committed WAVs byte for byte. Half of that is still true and half is not, and
the half that is not is worth knowing before someone treats a diff as a defect.

Two independent local Release builds still agree exactly. The committed files,
which CI renders, do not match a local render of the same source: the
difference is at most 9 of 32767 on `01-steel-sustain-range`, a sequence of
single held notes, and at most 981 on `06-playing-behaviours`, which changes
chords over ringing strings under the bridge hand. Same source, same
calibration, different machine.

That gradient is the explanation. A last-bit rounding difference is amplified
by the parts of the model that feed back and multiply - the junction that
couples six strings through one bridge, and now a longitudinal path whose drive
is a square - so it stays at the bottom bit on a single sustained note and
grows over seconds of chords. Nothing here is nondeterministic; each machine
reproduces itself exactly.

So the byte-exactness claim is scoped to one platform, and the committed WAVs
are CI's rendering rather than a checksum anyone can reproduce. A local render
that differs is not evidence of a defect, and the way to compare two builds
remains what it has been: score them over identical rows with identical
settings.

## 2026-09-01 — every note-off was a thump, and the hand's loss now settles over a round trip

A single low fretted note, released, peaked above what it had at the moment
the key came up: a steel G2 by 6.5 dB and a nylon G2 by 4.6 dB, the peak
arriving one period after the note-off. The chord measurements that had
closed this family of transients — a six-string release at 0.83 of the chord's
own level — hid it, because a chord's release is six small steps against a
large background and a single note's is one large step against a small one.
The cause was the same one already recorded: the release loss is a gain per
round trip, and `advance()` applied the whole of it to the first sample it
handed out, so the wave the junction reads stepped by a third on G2 (0.644
per period for a 0.16 s T60) and the bridge and body rang on the step.

Two fixes were measured, one derivation each. Applying the loss to what the
loop stores, which this file records as tried twice before, was measured a
third time and was worse again (+10.6 and +11.2 dB on steel G2 and C3): it
does not remove the step, it delays it by one round trip, and the derivative
re-referencing that had been absorbing the read-side step no longer lines up
with it. Slewing the applied gain toward the requested one across one round
trip - the unit the loss is defined in, so nothing is chosen - removes it: the
release peaks below the held note on every case measured, single notes at
G2, C3 and E3, a six-string chord release, the pedal coming up, in both
materials at 44.1, 48 and 96 kHz, and the difference from a note simply left
to ring is negative everywhere, so a release only takes energy out. The tail a
taken string carries gets the same slew, in both directions, so the derivative
flags that had been set across a release are no longer needed there. The
corpus never releases a note and is bit-identical; every committed demo
releases notes and changed.

A regression now plays the same material with and without the note-off and
asserts that the lifted key peaks no higher than the held note and adds
nothing above it, at three rates and in both materials. It fails on the
previous engine at every one of its 18 cases.

## 2026-09-01 — the fretting hand's own sounds, from the MIDI a player already sends

This file records twice that a hammer-on and a pull-off were "the refret they
are and nothing else", because how far a finger pulls a string is a player's
choice and inventing an amplitude for it is what this file exists to refuse.
The player's choice is exactly what MIDI velocity carries, and the instrument
already has a law for what a velocity means: the pluck's. So the fretting hand
now sounds, with the same rule for every articulation - **a hammer-on or a
lift at a MIDI velocity carries the string energy a pluck at that velocity
would**, bounded by what the mechanism can physically release - and the only
new numbers are published set-up dimensions.

**Hammer-on.** A point driven across an ideal string at speed v drags a
V-shaped dent whose flanks have slope v/c and which moves down with it. When
the string meets the fret crown, the action height h below the old line, the
dent is a triangle of half-width w = c·h/v carrying velocity v throughout;
relative to the new segment's own rest line, the crown-to-saddle line, that
is a released triangle with its apex at w and height h(1 − w/L) plus a uniform
velocity over [0, w], both written into the loop on top of the vibration it
already holds. A finger slower than c·h/L has the dent's front reach the
saddle first, and then the whole segment moves down with it. Solving the
dent's energy for v from the pluck's energy at the note-on velocity makes a
hammered note land at the loudness of a pluck at that velocity and get
brighter as it gets faster: measured against a pluck at the same velocity on
the same note, hammer-ons at 0.2 to 1.0 sit within +7 and −2 dB of it in
1 s energy on steel and within +6 and 0 dB on nylon, monotone throughout,
where the build before arrived at 1 to 6% of the pluck's peak.

**Lift and pull-off.** The string was pressed to the fret by the action
height there, a triangle over the segment it now belongs to - the open string,
or the segment stopped at the next note the hand still holds. If the lift
carries at least that triangle's elastic energy the finger is gone before the
string moves and the shape is released whole, a pull-off; below it the string
keeps up with the finger through the same triangle and leaves it at the rest
line with the finger's velocity over that shape, the quasi-static release,
which is the velocity initial condition whose loop content is the integral of
the triangle mirrored about the half period at half height (verified against
the modal series to the decibel). The vibration the stopped segment held goes
on over the new length - the loop keeps its content and only its length
changes, as a slide does - and the finger, still touching until it has risen
h/v clear, damps it with the hand's 0.16 s contact for that long. A string
lifted to open rings on in the junction with no key and no hand on it until it
dies away; a note-off velocity of 0.3 to 1.0 releases the open string within
±4 dB of a pluck at that velocity and leaves it sounding its open pitch.

**What is a convention, and what is not.** The action heights are Martin's
and Taylor's published 3/32" and 1/16" at the twelfth fret, 4 and 3 mm for a
classical, with 0.5 and 0.7 mm at the first fret, on the line a straight neck
puts between them; the speeds come from the energy law the pluck already
obeys; the only convention is where the finger stops staying on the string,
and it is MIDI's own: note-off velocity 64 is what a keyboard sends when it
does not sense the release, so 64 and below is the finger staying - exactly
the note-off every host sent before, bit for bit - and the lift grows from
there to the full pull-off at 127. A DAW's default note-off therefore changes
nothing. This is recorded as a convention, like CC68 being legato, not as a
measurement.

Two things were got wrong on the way and are worth keeping. The first
hammer-on used a pure ramp from the fret as its released shape; its corner at
the clamp has unbounded elastic energy, so the speed that "matched" it was
meaningless and the level jumped 17 dB between velocity 0.6 and 1.0. The dent
is what a fast finger actually makes, and it is finite. And the first lift
wrote its shape over the loop's target length while the delay was still
slewing from the fretted length, so the read point sat in the middle of the
new shape and stepped, 18 dB above the note; only samples younger than the
current read age are ever read again, so the shape spans the current delay
with its zero at the sample about to be read, and nothing steps.

The corpus never releases a note or hammers on, so it is bit-identical. The
engine suite asserts the pluck-law tracking, the monotone velocity law, the
open pitch after a lift, the held pitch after a pull-off, the absence of a
first-sample step, and lift zero's exactness, in both materials at three
rates; the wrapper suite asserts that release velocity 64, an unsensed
release and a Note On at velocity zero are the plain note-off and that 127
leaves the open string ringing.

## 2026-09-01 — a repluck lands the hand on the string, and a repeated note stays on it

Two defects in the commonest gestures a guitarist makes, both found by the
performance lens of this session's audit and both invisible to a corpus of
single notes on fresh engines.

A held note replucked went through the contact projection this file records
as "half the problem": the stored shape averaged with its copy shifted by the
contact position, in one sample. Six G2 replucks at 250 ms peaked −17.5,
−11.1, −2.7, −3.1, −1.6 and −2.6 dBFS, each peak on its first four samples and
the last five into the safety limiter, because the projection keeps every
mode with a node at the contact at unit gain and no loss, so the near-node
partials pile up pluck after pluck - H7 and H8 stood 20 and 36 dB above the
first pluck by the third. The picking hand landing on a sounding string is
the contact a taken string already goes through, so a repluck now takes the
vibration into the tail under the hand and releases the new pluck from rest:
the same six replucks peak −17.5, −16.6, −15.7, −15.0, −14.6 and −14.2 dBFS
with their first samples 25 to 30 dB down. The projection path is removed
rather than left unreachable.

A note repeated after its key came up hopped: `chooseString` took a free
string first, so E4 played and released six times landed on strings 5, 4, 3,
2, 1 and 5, five voices alive and the peak climbing 7 dB as they stacked, and
where no other string could reach the note the retake wiped the ringing loop.
A guitarist replucks the string still sounding the note. The allocator now
does the same first, and the retake goes through the hand like any other
repluck: six repeats stay on one string at −22 to −24 dBFS.

Both are gated by regressions that play the gestures, at three rates, and by
the corpus staying bit-identical.

## 2026-09-01 — audio-rate tension modulation is inert at this displacement, by a number

The excitation lens measured the largest tone deficit in the corpus: from the
archtop's softest to its loudest layer the recordings' attack centroid rises
by about 1,900 cents and the model's by 96, and at the loudest layer H4-H12
are 12 to 21 dB short. The decision log's public nonlinear oracle attributes
that kind of enrichment to the string's tension modulation, and the engine
already computes that tension every sample for its pitch glide, using only
its slow part. The audio-rate remainder was prototyped as a per-sample
modulation of the loop delay (Tolonen, Valimaki and Karjalainen, IEEE TSAP
2000), with a running slope-energy sum kept as samples are written and no
constant beyond the displacement scale the pitch surrogate already carries.

It does nothing audible, and the arithmetic says why. The displacement scale
was fitted against the pitch rise the recordings show, a few cents, which is
a tension ripple of about 0.4% at full velocity; that is one sample of delay
modulation on a low E and sidebands more than 20 dB down on the twelfth
partial. Measured on open E2 at five velocities, neither the attack centroid
nor the H5-H12 balance moved by more than 0.3 dB in either material. A number
settles it: at the nonlinearity level the recordings themselves exhibit, the
string's tension modulation is not where their velocity brightness comes
from, and the mechanism is not shipped. The same lens's reading of the
archtop's attack - a plectrum's release and click, which Touch cannot reach -
is the direction that remains.

## 2026-09-01 — the plectrum's dent produces the velocity law and trades the flat-top for it

The tension-modulation entry above left one direction open for the archtop's
missing velocity brightness: the plectrum's own release. The prototype is the
hammer-on's dent turned round. A pick moving at speed proportional to MIDI
velocity drags a V-shaped dent whose flanks have slope v/c, so the released
shape is the triangle with that dent clipped into it at the pluck point, plus
the velocity the dent carries; the pick speed is the one new quantity, set
to velocity times Touch times a scale, and the rest is the string's own
geometry. It does a fraction of what the recordings ask. On the six steel
notes rendered at both the softest and the loudest layer, the loudest layer's
H5-H12 balance over the softest rises by a median 2.8 dB against the shipping
engine's 0.4 and the recordings' 9.3 (peak partial levels at 80-250 ms, the
recordings read at their own 44.1 kHz mono and the renders as the mid of
their 48 kHz stereo); the attack centroid's 1,900-cent rise it does not
touch, and the audit's verifier of the same mechanism in its
release-duration form found why the rest is out of the excitation's reach:
the 80-250 ms H5-H12 bins are filled by the idle strings' partials and the
loop's own upper-partial loss, so no initial condition owns that descriptor
until the junction and the loss are fixed.

Scored, it is a trade rather than an improvement. At the shipping
calibration the three splits move from 6.8353, 6.8238 and 5.6547 to 6.7833,
6.7377 and 6.4592: training and development validation 0.8% and 1.3% better,
the eight flat-top rows 14.2% worse. A full 29-value refit around the dent,
through the fitter's four stages, reaches 6.7237, 6.8111 and 6.7407: training
1.6% better, validation back to within 0.2% of shipping, the flat-top 19.2%
worse. The refit's gain is on the rows it was fitted to and nowhere else,
which is the signature the 2026-08-31 bracket rule exists to catch, and the
rows that worsen are the finger-played flat-top, the instrument the
Dreadnought shape is meant to be.

The reason is in what the dent needs to know. A plectrum's edge makes a dent;
a fingertip's pad does not, or makes a far wider and shallower one, and the
flat-top rows are finger-played at the same velocities the archtop is picked
at. Scaling the dent by Touch does not separate them, because Touch is a
pressure the user sets by ear, not a statement of which is on the string,
and choosing the Touch value at which a dent becomes a finger is a number
nobody measured. The audit's excitation lens proposed the same mechanism in
its derivable form, a release duration equal to a clearance width over the
string's release speed, and it needs the same two endpoints, a fingertip's
and a plectrum edge's width, which the corpus cannot supply for nylon at all
and only by fitting for steel. Not shipped. What would license it is a
measurement that separates the two releases on one instrument, or a
finger/plectrum switch the player sends, at which point the dent is geometry
again and the flat-top rows stop paying for the archtop's brightness.

## 2026-09-01 — the body's low-mode damping is the measurement window's, and a set to choose by ear

The audit's string-bridge-body lens and its verifier found the same thing
from two directions, and it was then reproduced here with the generator's
own code on the raw archive record. `GenerateMeasuredBody.py` keeps the
first 3000 samples of the g21 impact response, 62.5 ms, before it takes the
magnitude and fits the 96 modes; that window's own bandwidth is 16 Hz, and
for every mode below about 1 kHz the header's f/Q sits at 12 to 23 Hz, so
what the header calls a mode's Q below 300 Hz is the window, not the guitar.
Re-running the same extraction with the window at 250, 500 and 1000 ms, the
low modes converge from 250 ms on and stay put: 91 Hz Q 7 → 19 → 21 → 21,
178 Hz 10 → 18 → 18 → 18, 209 Hz 15 → 39 → 40 → 40, 229 Hz 10 → 15 → 15 →
15, 409 Hz 28 → 36 → 36 → 36, with the worst 250 ms against 1000 ms
disagreement below 700 Hz at 7.8%. The peak frequencies move with them, 97 →
91 Hz and 211 → 209 Hz, which is the truncation's smearing coming off. These
are the guitar's modes: on six other guitars in the same archive the peaks
sit elsewhere, so they are not the room. On top of that the calibration then
fitted bodyQScale to 0.098 against the benchmark corpus, a different guitar,
which puts 29 of the 96 modes on the engine's Q = 4 floor and every mode
below 500 Hz among them. Shipping is five to ten times more damped below 300
Hz than the record resolves.

The window length is not a chosen number. The shortest window at which every
mode below 700 Hz has its Q within 10% of the 1 s value is 250 ms (12000
samples, the same 10% raised-cosine fade), and that fit passes the
generator's four regression limits as they stand; the 500 ms fit does not
(channel 1 p90 3.85 dB against the 3.75 dB limit), so 250 ms is what the
generator's own gates select. Regenerated at 250 ms the header has a median
Q of 59 against 55, five modes on the Q = 80 clip against ten, and one mode
in the 85-145 Hz band (90.8 Hz) where the truncated fit had two.

Two candidates are each defensible on their own physics - the header as
generated with its Q un-broadened, and the resolved header - and the corpus
says the opposite of the measurement, so this is a listening decision. The
set is with the user, steel and nylon judged separately, letters only, key
unread by design: a chromatic run E2 to B4 in 0.15 s notes, whose partials
land on and between the 91/178/209/229/409 Hz modes, then an E chord held
0.9 s and released so the body's ring is what remains; 48 kHz, identical MIDI
and seed, whole-file RMS matched to A with the trims in the key. A is the
shipping engine. B keeps the shipping header and sets bodyQScale to 1.0, a
calibration value and no code change. C is the resolved header at
bodyQScale 1.0 with lowBodyModeGain 7.5 as shipped. D is the resolved header
with the low-mode gain at 1.0 as well, because that +17.5 dB was pinned by
ear on top of a window that had taken most of the 91 Hz mode's ring, and the
resolved mode carries about four times the truncated one's steady-state
gain, so the two must be heard apart.

Scored on the reproduced baseline (6.8353, 6.8238, 5.6547), all three cost
the corpus: B 7.1107, 7.0448, 6.0883 (4.0%, 3.2% and 7.7% worse); C 7.1345,
7.0373, 6.1412 (4.4%, 3.1%, 8.6%); D 6.9581, 6.8231, 6.7668 (1.8% worse,
level on validation, and 19.7% worse on the flat-top, which is the eight
finger-played rows once more asking for the low-mode gain). Against the
recordings themselves the direction is the other way: the standard deviation
of the H1-H12 partial levels about a quadratic trend over 80-400 ms, a
measure of how much the body colours one note, is 7.2 dB on the archtop
rows, 8.1 on the classical and 6.1 on the flat-top; A renders 5.0, 4.2 and
4.6, B 6.5, 5.5 and 6.7, C 6.7, 5.8 and 6.4, D 6.7, 5.9 and 6.7 (the
verifier's independent implementation of the same measure reads 7.1, 8.0
and 6.3 against A's 5.1, 4.2 and 5.1). The benchmark's
40-band body term reads that colour as error because its reference is a
different guitar; the recordings' own fine structure says every letter but A
is closer to a real one. That is the same disagreement the 2026-08-31
bracket rule was written for, and the rule stands: a tie goes to the corpus,
adoption needs a clear preference by ear.

Each letter was also built into the engine test suite on a copy of HEAD. B
passes it whole. C fails one regression by a hair, the resonant open-string
tail share at 0.975 against the 0.97 ceiling a listening verdict pinned,
which is the one-way idle-string path (the audit's largest open defect)
radiating the sharper low modes; D passes that and fails the repluck peak
regression at 1.51 times the first pluck against the 1.5 pinned this
session, the resolved 91 Hz mode's ring adding under the second pluck.
Neither is a defect of the header; both are thresholds pinned on the
shipping body, and whichever letter the ear chooses, if any, re-pins its own
threshold with the reason written beside it. Until the user has heard the
set nothing changes: the header, bodyQScale and lowBodyModeGain ship as
they were.

## 2026-09-01 — the benchmark record was a build behind

The committed fit report and the README summary quoted 6.7679, 6.7683 and
5.6139, which are the scores of the longitudinal path at gain 0.025. That
gain was set to zero when the drip was removed, and the numbers were not
re-taken. Rendered and scored here, the shipping engine reads 6.8353, 6.8238
and 5.6547 - the pre-longitudinal build to four places, as it should - and
the report, its summary and the README now say so (the README's benchmark
table kept the stale row for a day longer; it agrees now). Every engine
change in this session was checked against that baseline by byte comparison
of the 79 model renders, and all of them are identical: nothing in this
session touched a single note on a fresh engine.

## 2026-09-01 — close stereo microphones: what the body already is, and a pair to choose by ear

The user asked for stereo microphones simulated close to the guitar. The
archive the body is fitted from answers most of that: its method notes place
three quarter-inch pressure microphones in the near field, 10 cm above the
top plate - one each side of the bridge, 20 cm apart, and a third 20 cm
toward the neck from the treble one, "where flamenco guitarists usually place
their stage microphone". The shipping stereo is the first pair, so the two
channels already are a spaced pair of close microphones on a real guitar, at
a spacing and height a recording engineer would recognise. That is now said
in the README where the body is described.

What the two channels lack was measured. The session's signal lens found the
model's channels coherent to 0.99 in every band where close-miked flat-top
recordings sit at 0.35 to 0.77 at 1 kHz and under 0.2 at 8 kHz, because the
96 modes share poles and only their weights differ per channel. Three things
were then tried against the raw archive data, downloaded and verified by MD5,
from which `GenerateMeasuredBody.py` reproduces the committed header exactly.

First, the coherence target itself. With the same estimator on a note that
decays, the recordings read 0.89, 0.61, 0.32 and 0.22 at 1, 2, 4 and 8 kHz,
while the raw measured pair - no fit at all, the two microphones' own
responses - reads 0.79, 0.84, 0.86 and 0.96. A single-source model driven
through two fixed responses cannot reach the recordings' upper-band figures
because those are the noise floor of a decaying note, not the guitar; the
target is an artefact of the estimator and was dropped.

Second, a side bank: keep the calibrated mid bit-identical and add the
measured inter-microphone ratio as its own modal bank, which is the raw
pair's arrival-time and phase structure that the per-channel minimum-phase
conversion discards. It does not fit. With the generator's own machinery the
raw side response reaches a relative complex error of 0.76 to 0.86 at 48,
96 and 144 modes, a 90th-percentile channel-ratio error of 10 to 13 dB and a
median inter-channel phase error of 22 to 37 degrees, for either pair - the
same class of failure this file records for a raw-phase modal replacement of
the mid. A 3000-tap side FIR would be exact but must sit against a mid whose
Q the calibration broadened tenfold, and where the side then exceeds the mid,
as it does by 9 to 13 dB above 300 Hz, the channels invert. Not shipped.

Third, the other measured pair. The generator run with the treble-bridge and
upper-bout microphones passes its own fit gates and gives the classic
bridge/twelfth-fret placement from the same impact on the same guitar. Its
mid changes with it, so it is a tone change as well as an image change: on
the shipping calibration, unrefitted, training moves 6.8353 to 7.0032 and
development validation 6.8238 to 7.0661, 2.5% and 3.6% worse, while the eight
flat-top rows move 5.6547 to 5.4132, 4.3% better - the disagreement between
the corpus and the flat-top that this file has recorded five times. Both
placements are measurements of one guitar under one calibration, so which is
the more realistic pair is the ear's question. A is the shipping pair, B the
upper-bout pair, steel and nylon, level-matched on whole-file RMS with Stereo
Width at 1.0; the set is with the user and its key is unread by design.
Nothing is promoted until it is heard.

A correction to the second item, from the audit's verifier. The 0.76-0.86
failure was of the ratio-derived side, the minimum-phase mid times the raw
inter-microphone ratio. Fitting the raw side response itself with the
generator's own pole and residue machinery reaches a relative complex error
of 0.209, a median magnitude error of 0.95 dB and a 90th percentile of 2.88
dB at 96 modes, inside the generator's 0.30, 1.25 and 3.75 dB gates (48 modes
fail at 0.411). A 96-mode side bank at the measured Q against the shipping
mid passes the engine suite with the mono unchanged to six places, but it
leaves 9 of the 79 corpus notes wider than any of the eight stereo
recordings and 3 with the side above the mid, and applying the shipping
bodyQScale to the side removes the effect. So the side bank is not a fit
failure; it is a listening candidate that waits on the body-damping verdict,
because its width depends on which Q the mid ends up with.

## 2026-09-02 — the audit: five lenses, fourteen candidates, seven verified, one shipped

The realism audit ran as a workflow: five lenses (excitation, string-bridge-
body, performance, signal analysis, market and literature) each measured the
shipping engine against the 115 recordings and proposed mechanisms; a merge
stage consolidated them into fourteen candidates; one adversarial verifier
per candidate then tried to refute it against the code, this log and its own
probe renders; a synthesis ranked what survived. What each verdict licensed
is recorded here so nothing is re-proposed on the evidence that already
refuted it.

Rejected on measurement. A pick-release contact transient (C03): 89% of the
recordings' velocity brightness is harmonic, and adding the recordings' own
aperiodic residual to the renders moves training by 0.32%, the ceiling any
noise burst can reach; a shaped burst would only fill the harmonic deficit
with noise. A velocity-dependent release time in seconds (C04): it is the
shipping aperture map in other units, its published bound on a plectrum's
edge (0.4 mm) puts the hard pluck into the smoother's comb regime, and the
80-250 ms H5-H12 balance it targets is filled by the idle strings' partials
and the loop's own loss, so no initial condition owns that descriptor until
the junction and the loss are fixed. A nylon velocity-to-Touch slope
transferred from steel (C05): fails its own monotonicity gate on the first
string and is a chosen number. A matched-Z bridge discretisation (C08b):
positive-real but a worse match to the analogue fit than the prewarped
bilinear at 44.1 and 48 kHz. A bass-side bridge port for the low strings
(C09): on g21 the bass-side driving point is more mobile than the treble
side at 82.8, 90.8, 178 and 412 Hz (conductance 1.19, 1.27, 1.57 and 1.15
times) and less only at the rocking modes 209, 591, 656 and 767 Hz, the two
ends are nearly one point below 250 Hz (coupling 0.96 and 0.69 in 60-120 and
120-250 Hz) and decouple only above 2 kHz (0.25-0.28), and a bass-port bridge
for E2, A2 and D3 at the shipping calibration scores 6.938, 6.830 and 5.696
while speeding the low fundamentals further from the targets: the low-string
over-damping is not a bridge-position artefact, and the 2026-08-30 reading
that it is the flamenco body against the corpus instruments stands.

Confirmed and closed in this session. The fretting hand (C06) does what its
commit says on every point measured, with the overclaims corrected in the
README: a hammer-on carries a pluck's energy, not its loudness (+7 to −2 dB,
1 to 3 dB of its own range), release velocity is a two-region control, and a
sensed release ghosts the open string at a stated level. The documentation
and dead-code corrections (C14) are in: the inert bridge-contact filter is
gone with the corpus byte-identical, the benchmark table row is corrected,
the loss-corner clamp and the aperture map say what they do beside the code,
and the anchor spring, the decay bands, the attack deficit's two owners, the
idle strings' measured cost, the dispersion above the last anchor and the
second axis's corpus signature are in Known gaps. Of the attack deficit,
about 4 dB at every velocity is the residue tilt chosen by ear on 2026-08-31
(−1.0 against the fitted +1.862 dB per octave: the model's H5-H12 balance at
80-250 ms moves from −23.7, −23.3, −22.6 and −22.8 to −20.2, −19.2, −18.8 and
−18.9 dB for the four layers against the targets' −18.9, −15.9, −13.2 and
−11.3); the remainder, 1.3 dB at the softest layer to 7.6 dB at the loudest,
grows with velocity and is therefore the excitation, not a linear chain.
The same verifier corrected the reading of the 2026-08-31 entry "neither
excitation control explains the dark steel attack": its "matches them to 0.1
dB over 5-10 kHz" was a pooled median over four velocity layers; per layer
the steel 2560-5120 and 5120-10000 Hz shape reads +3.3 and −2.7 dB at the
softest (n = 27), −0.7 and −9.0 at the second (7), −7.0 and −10.1 at the
third (7) and −10.3 and −17.5 dB at the loudest (27), with the 0-12 ms
centroid −269 cents at the softest and −2398 at the loudest. The attack audit
tool should report per velocity; until it does, its pooled figure is not
evidence about the loudest layer.

Standing, with gates written. The two-way six-string junction (C01), the
largest single defect: built in this session with the verifier's five
prerequisites, its own entry below. The body's low-mode damping (C02): the
set of 2026-09-01, with the user. The bridge anchor spring (C13): a defect,
not a blocked measurement - its own entry below. The lossless fractional
delay (C08a): a settled correctness defect with no invented constant. The
Catmull-Rom read loses −0.23 and −0.53 dB per pass at 8 and 10 kHz at a
half-sample fraction and nothing at an integer one, so a string's upper
partials decay by an amount that depends on the accidental fraction of its
loop length and on the host rate: with the bridge and sympathy off, H8 on
steel MIDI 76 to 84 decays at 40, 34, 50, 189 and 113 dB/s at 48 kHz, and
MIDI 84's H8 at 468, 113 and 65 dB/s at 44.1, 48 and 96 kHz, where a
first-order Thiran allpass reads 19 to 32 and 36, 32 and 48. The naive fold
into the tuning collocation fails (the residual is discontinuous at the
half-sample boundary and partial placement reaches 12 cents on steel 84);
the form that survives fixes the integer part per voice, lets the fraction
run continuously in (0, 2) as the delay slews, re-anchors with the
Välimäki-Laakso state correction, and folds that phase into the collocation
and the tests' loop-phase model. At the shipping calibration it scores
6.8184, 6.8111 and 5.9850, and no shelf value between 1.2 and 2.17 brings
the flat-top rows below 5.92 because the material loss above 6 kHz was
fitted with the accidental loss present, so it needs its restricted refit
(the high-loss corner and the two frequency-loss scales) and then, if the
flat-top rows still disagree, a listening set. It is next after the junction
is heard, and deliberately not in the same set. A strum sweep for
same-sample chords (C07) survives as a feature with its design settled: an
engine-side deferred pluck that allocates at the group's sample and plucks
string by string at the pick's traversal pace, off by default. Deferred on a
measurement: the second radiating polarisation (C12), the g21 modes' own
over-damping and the loudest layer's remaining brightness (C13's blocked
halves), and a bound on the hammer-on's finger speed, which waits on one
published fingertip-speed figure.

## 2026-09-02 — the anchor spring is a corpus-fitted shield, and a set to choose by ear

The audit's verifier of the g21 chain candidate found that its central
attribution was wrong in a way that turns a blocked item into a defect. The
strings do not see the raw bridge mobility; each ends on the junction's
effective admittance, the bridge in series with the lumped anchor spring
(the sum of T/L over six strings at the fitted 17.2 mm, about 41.5 kN/m).
Between the body's modes the measured mobility is mass-like, and a spring to
ground in series with a mass resonates: computed from the shipped headers,
the strings' effective conductance is 0.309 at 233 Hz and 0.047 at 102 Hz
where the bridge's own is 0.0039 and 0.0048, 78 and 10 times, with the
coupling at 233 Hz strong enough (G·Z about 0.28) for Gough's veering. That
is where every doublet the audit listed comes from, and the halo of the
one-way idle strings hides most of it today.

Reproduced here on a copy of HEAD with the sympathetic strings off, early
decay of the named partial over 0.15-1.2 s, in dB/s, recording | shipping |
no spring | DAFx-26's 3.25 mm stub: flat-top A#2 H2 (233 Hz) 8.0 | 79 with a
−40/+40 cent doublet | 14 | 8.2; flat-top A#3 H1 17.8 | 78 (−46/+40) | 15 |
8.3; nylon G2 H1 (98 Hz) 6.2 | 63 (doublet) | 23 | 15; nylon A3 H1 18.5 | 82
(doublet) | 24 | 15; steel A3 H1 8.1 | 30 | 29 | 8.3; and the low E, where
removing the spring exposes the flamenco body's own 83 and 91 Hz modes,
recorded 4.5-8.6 | 10 | 68 with a −25/+32 cent doublet | 7.8. No length
inside the 8-60 mm bound is free of the artefact; it sits at 102 and 233 Hz
as shipped, 95 and 215 Hz at 60 mm, 107 and 260 Hz at 8 mm, and 306, 602
and 668 Hz at the stub. So the fitted spring is a low-frequency shield the
corpus fitted for a flamenco body against an archtop and a classical,
wearing a bridge dimension's name, and the README no longer calls it
geometry.

Two terminations are each defensible on their own physics and disagree with
the corpus. No spring at all is the physically consistent one - the string
ends on the driving-point mobility, which Mores measured on a strung bridge,
so a spring to ground is double-counted geometry (Woodhouse 2004) - and it
scores 6.8723, 6.8800 and 5.8989 against 6.8353, 6.8238 and 5.6547, worse on
every split, while exposing the low E at 68 dB/s. DAFx-26's own stub scores
6.4921, 6.4762 and 6.5464: 5.0% and 5.1% better on the picked splits and
15.8% worse on the eight finger-played rows, the disagreement the 2026-08-31
bracket rule sends to the ear. The set is with the user: A the shipping
engine, B no spring (tail 1000 m), C the stub (3.25 mm), one value changed
and its bound relaxed to allow it; single notes that land a partial on each
artefact placement, then the open low E and A held; steel and nylon judged
separately; whole-file RMS matched to A with the trims in the key; letters
only; key unread by design. What would settle it without the ear is the
matched flat-top or classical bridge mobility this log has asked for since
2026-08-30. Gates for whichever letter is chosen, if any: the analytic ratio
of effective to bridge conductance below 3 over 60-3000 Hz; the four
233/98/220 Hz rows above within twice their recordings with no secondary
peak above −20 dB within ±150 cents; the low E no worse than twice its
recording, which no spring fails; the held-chord T60 ratio within 1.10; the
suite at three rates; the corpus re-scored and reported, because it will
change.

## 2026-09-02 — the two-way six-string junction, built and gated, with a set to choose by ear

The audit's largest confirmed defect is the one-way idle-string path. Each
idle string is written from the bridge displacement and its reaction force is
added to the body's input, but it is not a member of the junction: the
bridge displacement it is driven from does not see its load, so at a
coincidence the idle string's force is bounded by nothing. The symptoms the
lenses measured on the shipping engine follow from that: ten hand-damped
repeats of A3 pump the idle A string until, one second after the last, its
220 Hz band sits 7 dB above the first note's held level on steel; a released
open E4 stays above its held level for 600 ms; single notes carry
open-string partials at −15 dB re the played fundamental over 0.5-2 s where
the archtop, classical and flat-top recordings carry them at −40, −31 and
−25; and the flat-top rows' decay term is supplied by that halo.

The fix is the same solve with six members. F_b = Σ Z_i (2a_i − x_b) and
x_b = Y_b F_b, the derivation of 2026-08-31, summed over every string on the
bridge rather than the played ones: an idle string at its resonance then
presents thousands of times its characteristic impedance and pins the bridge
there, which is what bounds the sympathetic energy on a real guitar
(Woodhouse 2004; Gough 1981; Bank and Karjalainen 2010). The exact edits,
each one a consequence of that membership and none a new constant: the
junction's impedance sum and weighted incident include every string
whenever the sympathetic switch is on, so the port impedance is a constant
of the instrument and the note-on port step is gone; a taken string's tail
is a second wave on the same string, so it adds to that string's incident
with the impedance counted once, and neither it nor an idle string radiates
outside the junction any more; the tuning compensation evaluates the bridge
for every string; and a released string is handed back to the allocator
when its release damping has run its T60 (0.16 s fretted, 1.25 s open, the
constants beginRelease already has) plus the hand's 80 ms, keeping whatever
it still carries as an idle string's wave, because the level test it used
before never fires once the bridge re-drives it. All Sound Off and the
retunes still clear. The engine suite passes with three one-way assertions
re-pinned to what the mechanism does: the bypass must now unload the bridge
(it was asserted to change nothing), the resonant tail share loses its 0.97
ceiling because the ratio is no longer a share of a separate radiator (its
selectivity, resonant against off-resonant, is 25 to 1), and the pitch-pull
bound admits 6 cents, because at exact coincidence (E3 on the low E's second
partial) the played and idle strings form a coupled pair whose modes split:
the played energy sits in the lower member at −4.8 cents with the upper
member 12 cents above and 24 dB down, the coupling itself, not a tuning
error, verified on a high-resolution spectrum of the render.

Gated as the verifier predeclared, on a copy of HEAD. Ten hand-damped A3s
leave the idle A string one second later at −44 dB (steel) and −89 dB
(nylon) below the first note, against +7 and −8 on shipping. A six-string
chord's 0.5 s envelope is non-increasing after 0.5 s at 44.1, 48 and 96 kHz
(largest rise −1.5 dB); a single E3 over five idle strings shows one +1.4 dB
half-second exchange on its way down, which is the coupled pair trading
energy, and reaches −85 dB at six seconds where shipping's halo is at −55.
Open-string partials re the played fundamental move to −21, −25 and −22 dB
on the steel, nylon and flat-top rows, 4 to 10 dB toward the recordings and
still 20, 6 and 3 dB above them. The corpus at the shipping calibration
scores 6.6060, 6.7836 and 6.6298 against 6.8353, 6.8238 and 5.6547: 3.4%
and 0.6% better on the picked splits and 17% worse on the eight
finger-played rows, whose played-string decay the halo was filling in - with
the halo gone their fundamentals decay at 14.7 dB/s against the recordings'
6.0 and shipping's 9.3, which is the anchor spring and g21's own modes
showing through (the entry above). Refit from the same neutral calibration
with the same default budget, the two topologies reach different values and
the two-way one fits better on both picked splits, 7.437 and 7.275 against
7.740 and 7.377, and worse on the flat-top, 9.08 against 8.64; a
longer-budget pair was started and is not part of this record.

This is an improvement question, not a correctness one - less shimmer and
shorter tails against a halo the references do not have - so it goes to the
ear. The set is with the user, steel and nylon judged separately, letters
only, key unread by design: fretted E3 and A3 over free open strings, a
hand-damped repeated A3 line, a staccato scale and a sustained open chord;
A the shipping engine, B the two-way junction at the shipping calibration,
C and D the two topologies at their own common-start refits; level-matched
on the first pluck's attack window rather than the whole file, because the
whole-file level is the halo under test (about 10 dB of A's steel RMS), with
the trims in the key. Nothing ships until it is heard. If B is chosen, what
follows is a proper refit of the string losses and the bridge mobility
around the junction, because the flat-top rows' over-damping is then the
dominant single-note error and the anchor-spring verdict decides what the
string ends on; the two-way engine and its re-pinned tests are preserved as
a patch with the user until then.


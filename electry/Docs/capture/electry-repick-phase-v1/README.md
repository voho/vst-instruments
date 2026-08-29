# Electry phase-repick capture preflight

This session asks one narrow question: does the output of an open, already
ringing E1 or E2 depend repeatably on string phase when the next pick makes
contact? It is separate from `electry-mute-capture/v1`; do not add these files
to that validated 16-file manifest or resample either capture to imitate the
other.

Use one measured eight-string rig for exactly three disconnected TRAIN and
exactly two untouched HOLDOUT performance clusters. A cluster is a separately
recorded, independently randomized session separated in time; do not remount a
sensor unless its exact position and orientation are remeasured. Keep the
guitar, pickup, strings, pick, player, pick point, grip, interface, cable and
sensor geometry fixed across those clusters. Other players or rigs are later
external-validity tests and must not enter this rig's coefficient fit.

Copy `manifest.template.json` to `manifest.json` in each cluster directory and
replace every `REPLACE`, null placeholder, empty trial-order array, variable
frame count and zero SHA-256. Legitimate measured zeros remain zeros; `null`
and zero hashes are only the template's missing-value sentinels. Commercial
model-calibration and private competitive-evaluation rights are mandatory.
After all five captures and acquisition-only QC, copy
`acquisition-freeze.template.json` once at study level and use it to hash all
five completed manifests. Before decoding TRAIN, write its detached SHA-256 and
have the independent custodian sign that sidecar as an OpenSSH `sshsig` in the
`electry-repick-phase-v1` namespace. Establish the custodian identity and
allowed-signers file through a separate, pretrusted channel before capture;
the receipt records their hash/identity, while the validator takes the actual
trust root only from its command line. The signature authenticates the
custodian's before-TRAIN-decode attestation but is not a trusted timestamp, so
the custodian must archive or publish the signed sidecar before authorizing
decode. After all three
TRAIN results pass, similarly freeze
`holdout-freeze.template.json`, write and sign its detached SHA-256 with the
same pretrusted custodian, and archive it before decoding either HOLDOUT
bridge-DI response. If TRAIN fails, do not create a candidate or open HOLDOUT. Neither
receipt is written back into a manifest that it hashes.

## Files and synchronized channels

Record exactly two unprocessed WAVE files:

- `e1-open-phase-repick.wav` for open E1/string 8;
- `e2-open-phase-repick.wav` for open E2/string 6.

Both are 96 kHz, 32-bit IEEE float and four-channel, recorded through one
sample clock with this immutable channel order:

1. untouched bridge-pickup DI;
2. contact-onset reference;
3. plectrum-normal string motion;
4. tangential string motion.

The contact reference must observe physical pick/string contact with measured
uncertainty no greater than 1 ms. An optical interrupter, instrumented
plectrum/actuator or another independently calibrated nonmagnetic method is
acceptable; a threshold on the pickup DI is not. Log its polarity, units,
threshold/calibration procedure, uncertainty and any added plectrum mass,
stiffness or grip change.

Use nonmagnetic two-axis LDV or an equivalent calibrated motion reference at
the pick point. Log each axis direction, polarity, units, target position and
any reflective-target mass. If a channel measures velocity, the frozen
analyzer converts its complex H1 phasor to displacement with
`u = v / (j 2 pi f0)` before assigning phase. Do not rotate polarity or phase
after seeing a result. Keep all raw channels; do not gate, denoise, align,
trim, normalize or apply per-file gain.

A common clock does not cancel sensor-controller, analog-filter or ADC-channel
delay. Before the first scored pair, measure every channel's relative latency
and H1-H4 phase response through a common calibration stimulus. Freeze the
correction, calibration hash and uncertainty. The analyzer combines contact
and normal-motion uncertainty into a phase interval for every event; an event
whose interval touches or crosses a quadrant boundary is ineligible.

## Fixed two-stroke protocol

Use open Sustain with both hands clear of the target string's vibrating length
except for the pick. A fixed nonmagnetic felt fixture damps the seven non-target
strings near their midpoints; document its material, positions and residual-
ring verification separately when E1 or E2 is the target. Before scored pairs,
three hard calibration strokes per string must put every damped string's 50-200
ms bridge-DI RMS at least 40 dB below the undamped target-string median under
the same instruction. Fill and seal `damping-receipt.template.json`; its exact
three strokes for each of seven strings in both target configurations must
match the manifest and pass individually. A failure invalidates that
cluster. Never let the fixture
touch the target string. Choose one natural hard force and rehearse it; do not
infer or repair per-hit MIDI velocity afterward. Measure pick thickness, tip
geometry, grip exposure and pick-point distance from the saddle separately for
E1 and E2.

Before randomization, estimate one session `f0` for each string from a separate
open calibration stroke without moving the rig. It is a cue coordinate, not
the analysis result. For target phase fraction `q` in `{0, 0.25, 0.50, 0.75}`,
cue the second stroke at

```text
IOI = (N + q) / f0
N = 4 for E1
N = 8 for E2
```

Use a sensor-phase-locked actuator or real-time cue whose pre-session pilot
demonstrates all four measured quadrants; an unverified metronome-only cue is
not sufficient. Record 64 independent pairs per string: sixteen pairs for
every `q`. In each phase group, the second stroke is Down eight times and Up
eight times; the first stroke is always the opposite direction. Randomize the
complete 64-cell order
separately for E1 and E2 before recording, retain the draw/seed, and copy the
actual order into the manifest. Intended `q` is cue metadata only. The frozen
analysis bins the measured phase.

Start with at least one second of untouched DI and sensor noise. In each pair,
let the first stroke establish the open ring, make the cued repick, then retain
at least 250 ms after the repick before manually stopping the string. After the
natural stop transient, leave at least one second with no intentional
performance before the next pair. Monitoring cues must not enter any recorded
channel.

## Frozen analysis boundary

The study has two irreversible freezes. Lock the protocol, analyzer source,
equations, bins and gates before the first scored pair. Before any TRAIN
bridge-DI response is decoded, the acquisition/analyzer/baseline receipt then
binds the complete split, finished manifests, raw hashes, sensor latency
calibration, blinded integrity receipts, predeclared eligibility rules, windows,
synthetic fixtures and shipping renderer/build/preset/seed.
The phase-locked cue may read contact and pre-contact motion during acquisition,
but it must not expose bridge-DI response `G` or post-contact motion. Before the
candidate freeze, an independent custodian keeps every HOLDOUT raw channel
sealed. The candidate team may receive only hashes, file-integrity status,
the already-frozen intended stroke schedule and one aggregate registered
pre-contact coverage pass/fail from a blinded QC program—never per-event IOI or
phase, waveforms, post-contact sensor values, `G`, or listening access. Only the
exact-once analyzer receives per-event HOLDOUT metadata after the candidate
freeze. Decode all three TRAIN clusters through the frozen path. Only if every
TRAIN cluster resolves the registered real phase effect may TRAIN derive the
one candidate coefficient.

Then freeze the candidate source, executable, coefficient semantics, preset,
renderer and complete TRAIN result before either HOLDOUT bridge-DI response is
decoded. Decode both HOLDOUT clusters exactly once. They simultaneously test
real-effect replication and candidate promotion; no analyzer, baseline,
coefficient or gate changes between them. The acquisition receipt hashes all
five completed manifests. The study-level HOLDOUT receipt then binds that first
receipt, the same manifests, all TRAIN results and the candidate; neither
direction requires a circular file hash.

For each second contact, estimate local `f0` from normal motion using only
`[first contact + 20 ms, second contact - 7 ms]`. Search a fixed 0.1-cent grid
within +/-50 cents of nominal. At every grid point, fit H1-H4 jointly with a
DC and linear term under Hann weights; choose minimum weighted squared residual
and break an exact tie toward the lower frequency. Post-contact data never
enters `f0` or phase.

In each half-open 48 ms pre-contact `[-55, -7) ms` and post-contact
`[+7, +55) ms` window, use the same joint weighted least-squares model, centred
at `c`:

```text
x(t) = a0 + a1 (t-c)
       + Re(sum(h=1..4, z_h exp(j 2 pi h f0 (t-c))))
```

`z_h` is the fitted peak complex amplitude at the window centre. Let
`m_1_minus` be normal-motion displacement H1 and `d_h_minus`, `d_h_plus` be
bridge-DI H1-H4. If `t_contact` is measured second-contact time, primary phase
is

```text
phi = arg(m_1_minus * exp(j 2 pi f0 (t_contact - c_minus)))
```

where `m_1_minus` includes the frozen velocity-to-displacement conversion when
required. The periodic Hann is
`w[n] = 0.5 - 0.5 cos(2 pi n/N)` for `n=0..N-1`; the ten design columns are DC,
linear, then real-cosine and negative-imaginary-sine columns for H1-H4. Form a
deterministic 95% block wild-bootstrap `f0` interval from
4,096 fitted-plus-residual replicates. Contiguous 4 ms residual blocks (with a
final shorter block) receive SHA-256-derived +/-1 signs; every replicate reruns
the complete grid search, uses R type-7 2.5/97.5% quantiles and expands by half
a grid step. The hash event ID is
`study_id/cluster_id/string/trial`; trial, replicate and block indices use
canonical unsigned ASCII decimal, with zero-based replicate/block indices and
no leading zeroes except the literal `0`. An
interval touching the +/-50-cent search boundary is ineligible. The same 4,096
sign realizations are also refit at the original event's best `f0` held fixed.
The fit-phase 95% half-width is the R type-7 95th percentile of their absolute
principal wrapped H1-angle deviations from the original fit. A zero original
or replicate H1 amplitude assigns 180 degrees and therefore rejects the event;
no inverse-variance noise model is inferred from the Hann taper. The event
phase-interval half-width is the conservative sum of calibrated contact/
relative-latency/phase-response uncertainty recomputed from that event's `f0`
and contact-onset half-width, the joint-fit H1 phase 95%
half-width and the worst contact-phase drift across that `f0` interval. Wrap
that shortest circular interval with `phi`; reject it if its
half-width reaches 45 degrees or the wrapped interval touches a boundary.
Quadrants are centred at
the positive displacement maximum/positive-real phasor: 0, 90, 180 and 270
degrees, with fixed +/-45-degree half-width and no post-hoc rotation. Bins are
half-open counter-clockwise intervals `[-45,45)`, `[45,135)`, `[135,225)` and
`[225,315)` after wrapping; boundary ties or uncertainty overlaps are rejected.
The analyzer must pass and hash frozen noiseless E1/E2 joint-fit fixtures before
reading a capture.

A pair is eligible only when:

- measured IOI is 75-130 ms;
- raw contact-onset uncertainty is at most 1 ms and calibrated combined phase
  uncertainty stays wholly inside one quadrant;
- there is no extra transient, clipping or non-finite sample;
- normal-motion H1 spectral power and summed bridge-DI H1-H4 response power are
  each at least 12 dB above their frozen adjacent control bands; and
- the event lands in one fixed quadrant.

Every `{string, second direction, quadrant}` cell must retain at least three
eligible pairs or that cluster is inconclusive. Never replace or silently drop
a failed cell. A sealed result records a zero-event median as
`insufficient_events` and an incomplete derived profile or model cell as
`inconclusive_missing_registered_cell`; these are outcomes, not values to
impute. A crashed decode is a terminal validator failure: retain the
custodian's incident record and do not rerun or promote.

The only primary response is low-mode energy gain:

```text
G = 10 log10(sum(h=1..4, |d_h_plus|^2)
             / sum(h=1..4, |d_h_minus|^2))
```

For every `{string, second direction, cluster}`, form a four-element vector of
quadrant-cell median `G`, subtract its four-element mean, and pool the absolute
event-minus-cell-median residuals to form one MAD. That profile resolves phase
only when its uncentred maximum-minus-minimum span exceeds the pooled MAD. Every
string and direction must resolve in all three TRAIN clusters; the corresponding
centred profiles must have positive pairwise dot products. Otherwise stop as
`phase dependence not resolved` and leave HOLDOUT sealed.

Only after that TRAIN gate clears may TRAIN derive one default-off contact
amount. Apply that dimensionless total exactly once at the second diagnosed
contact, independent of sample rate and contact duration; do not reinterpret it
as a per-sample amount. It must stay in the registered inclusive contractive
range `[0, 2]`. Seal
`coefficient-receipt.template.json` with the
three TRAIN result hashes and the one derived value; the signed HOLDOUT freeze
binds that receipt. Shipping A and the coefficient-frozen candidate B
render every accepted real IOI and stroke direction at 96 kHz through the
direct engine. Each pair starts from an exact engine reset, resets the one
frozen variation seed, places the first diagnosed contact after one second of
silence, schedules the second by the rounded real contact-to-contact IOI, holds
the note for 250 ms afterward, then discards the state. E1/MIDI 28 uses string
8 and E2/MIDI 40 string 6; both contacts use velocity 0.90, the recorded stroke
directions and a calibration-frozen mapping between sensor-normal sign and the
engine coordinate. Render Mono/Bridge, Sustain/Open, output gain 1, and zero
sympathetic, Palm, Artifacts, pick, finger and release noise. The real fixture's
seven damped non-target strings are the corresponding zero-sympathetic condition.
The hashed preset binds every remaining guitar coordinate. A read-only test
seam records the model's exact contact sample and plectrum-normal displacement
at the pick point without changing its audio. Those diagnostics replace the
real sensor channels, so both renders use the identical phase and response
equations and bin their own measured state; neither is level-fitted to a real
hit. For each real schedule, shipping and candidate must match exactly in
contact/note-off/render samples, fitted `f0` and interval, phase and interval,
eligibility, quadrant and a SHA-256 over the frozen renderer's canonical
pre-second-contact audio, diagnostics and common sound-generating state. The
candidate may differ only in post-contact `G`; a changed bin is not an
improvement.

In each HOLDOUT, both string/direction profiles must independently resolve phase;
matching profiles must have positive dot product between the two HOLDOUTS.
Promotion then requires at least 50% lower median absolute shipping-to-real
error over the sixteen registered `G` cells in each HOLDOUT, no worse maximum
cell in either HOLDOUT, and the existing fresh-note identity, energy, lifecycle,
level, tuning, stability and CPU rails. Missing coverage, failed replication or
low SNR is inconclusive, not zero contact. A pass supports only phase-dependent
open E1/E2 repicks on this rig.

## Before delivery

Before the acquisition freeze, confirm that both files contain exactly 64
correctly ordered pairs, the stroke directions match the manifest, all four
channels share the declared latency calibration, no cue leaked into the
recording, and no processing or per-file gain was applied. Retain every attempt
and never top up or replace a cell. For HOLDOUT, blinded QC may expose only the
aggregate registered pre-contact coverage pass/fail; keep every per-event value,
waveform and post-contact sensor response sealed until the candidate/HOLDOUT
freeze. Run the automatic analyzer first and listen only after its result
receipt is sealed.

Every analyzer run uses `result-receipt.template.json`. Its sealed wrapper must
hash the event-detail payload, cluster manifest, analyzer and acquisition
freeze; a HOLDOUT result must additionally hash the candidate/HOLDOUT freeze.
For each run, the independent custodian fills `decode-ledger.template.json`,
attests that this was the sole decode, signs the ledger's detached SHA-256
before listening, and lets the result wrapper hash that ledger. OpenSSH
signatures authenticate those custodian attestations but do not themselves
prove wall-clock time or prevent an unlogged decode; independent custody and
pre-decode archival remain part of the protocol. Fill
`rails-receipt.template.json` from the frozen candidate's regression and CPU
run. The acquisition freeze binds both drivers and the canonical case-ID
schedule emitted by the rail driver. The six compatibility rails come from
`rail-test-result.template.json`, the exact Release test executables,
source/build receipts and complete raw log. Each paired build receipt binds the
source tree, source commit, compiler binary/version, generator, build-system
file, common flags/definitions, candidate-only compile definition, coefficient,
build log and output executable; the validator requires the shipping/candidate
common contracts to match. It also requires every
scheduled ID exactly once, rejects omitted or unknown IDs, and derives the six
booleans itself. CPU evidence retains the host fingerprint and raw paired
timings for all 18 combinations of 96/384 kHz, block sizes 16/64/512 and
4/12/20 simultaneous repicks per second per string. Each combination uses 20
alternating ABBA/BAAB rounds with two shipping and two candidate times per
round. The validator recomputes each paired overhead, reports the median of the
nine 96 kHz scenario medians and the maximum of the nine 384 kHz scenario
medians, then enforces the inclusive 1%/3% limits. It also requires every
copied rail value to match the hashed receipt.
The final
two-HOLDOUT decision uses `final-receipt.template.json` and hashes both sealed
result wrappers before any promotion claim.

The standard-library validator rejects remaining placeholders, count or split
drift, non-finite or wrong-format WAVE data, mismatched frames/hashes, changed
fixed setup, broken receipt links and invalid promotion gates. Run it at each
irreversible boundary:

```sh
python3 /absolute/electry/Docs/capture/electry-repick-phase-v1/validate.py \
  --allowed-signers /absolute/pretrusted/electry-repick-allowed-signers \
  --signer-identity independent-custodian \
  /absolute/study/acquisition-freeze.json
python3 /absolute/electry/Docs/capture/electry-repick-phase-v1/validate.py \
  --allowed-signers /absolute/pretrusted/electry-repick-allowed-signers \
  --signer-identity independent-custodian \
  --train-result /absolute/study/train-1/result-receipt.json \
  /absolute/study/acquisition-freeze.json
python3 /absolute/electry/Docs/capture/electry-repick-phase-v1/validate.py \
  --allowed-signers /absolute/pretrusted/electry-repick-allowed-signers \
  --signer-identity independent-custodian \
  /absolute/study/acquisition-freeze.json /absolute/study/holdout-freeze.json
python3 /absolute/electry/Docs/capture/electry-repick-phase-v1/validate.py \
  --allowed-signers /absolute/pretrusted/electry-repick-allowed-signers \
  --signer-identity independent-custodian \
  /absolute/study/acquisition-freeze.json /absolute/study/holdout-freeze.json \
  /absolute/study/final-receipt.json
```

Recompute both WAV SHA-256 values after their final copy and enter their exact
frame counts. Delivery is incomplete until the signed-rights document,
acquisition freeze and raw capture hashes all match the manifest. A HOLDOUT
delivery is incomplete until the separate candidate/HOLDOUT freeze binds both
untouched HOLDOUT manifest hashes.

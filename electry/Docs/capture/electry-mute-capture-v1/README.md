# Electry Palm/Dead capture preflight

This is a calibration session, not a polished performance. Record the untouched
bridge-pickup DI once and keep the guitar, pick, interface, input impedance and
gain, guitar-output cable and routing unchanged across all 16 files. Disable
amp/cab simulation, pedals, gate, EQ, compression, denoising and normalization.
Guitar volume and tone stay fully open.

Copy `manifest.template.json` to `manifest.json` in the delivery directory and
fill every `REPLACE`, zero placeholder or measurement, null gain, variable
frame count and SHA-256. Use stable anonymized `player_id` and `guitar_id`
values across the whole collection; neither person nor physical guitar may
occur in both train and holdout. Record `recorded_utc` as
`YYYY-MM-DDTHH:MM:SSZ`. The capture contract and analysis rationale are in
[`../../evaluation.md`](../../evaluation.md#commissioned-palmdead-capture-gate).

## Hands and strings

- `e1-*` is open E1 on string 8. `e2-*` is open E2 on string 6 in the declared
  `E1 B1 E2 A2 D3 G3 B3 E4` tuning.
- `open`: both hands clear the vibrating length except for the pick.
- `palm-*`: the picking-hand heel stays at the measured near, middle or far
  landmark. Measure heel centre to that string's saddle witness point along the
  string. Aim for 4, 12 and 20 mm, then record separate actual E1 and E2 triples
  in the manifest; angled saddles make one shared measurement insufficient.
  At natural playing pressure, photograph each setup with a ruler in the plane
  of the strings. Estimate the heel contact-footprint length along E1 and E2 at
  all three positions and describe the heel angle across the strings and which
  hand edge faces the saddles. Position, contact width and orientation are
  separate variables even when the same player calls all three “palm mute.”
- `dead`: the fretting hand lies lightly across all strings without pressing a
  fret. The picking hand clears the bridge and strings except for the pick.
  Before recording, choose one ordinary, comfortable Dead-note hand location,
  not a position tuned to produce a harmonic. Measure the nut witness point to
  the centre of the index-finger pad along string 8 and describe which fingers
  span which strings. Reproduce that landmark and hand shape for every isolated,
  rapid and groove Dead take. Palm position and Dead are different articulations.

Use the same hard intended pick force throughout. Do not chase an exact palm
pressure: keep the contact natural and repeatable. Choose one comfortable pick
contact point before recording; measure pick-string contact to the saddle
witness point separately on E1 and E2 and keep those two points fixed across
Open, all three Palm positions, Dead, rapid runs and both mixed-string grooves.

Before recording, measure the bridge pickup's sensing centre from each E
string's saddle and the resting open-string gap from the string underside to
the active pole or blade. Log the exact guitar-output cable, its length and its
measured total capacitance or datasheet-based estimate. Keep that cable in the
same direct signal path throughout; pickup location, magnetic gap and cable
loading all alter the DI spectrum independently of the muting hand.

Before the first take, make one reproducible random shuffle of all 16 filenames.
Put the seed/draw ID and permutation in `performance.recording_order`, then
record in that order. Do not default to the manifest's isolated→rapid→groove or
Open→near→middle→far→Dead listing: that would align articulation and repetition
context with tuning drift, pick wear and hand fatigue. Filenames and their
contents do not change.

## Isolated files

Every isolated file is exactly 1,719,900 frames: twelve 143,325-frame slots.
For zero-based slot `i`, the slot begins at `i * 143325`; leave 11,025 frames
with no intentional performance, make one pick, allow 88,200 frames for the
note, then leave 44,100 frames to reset. At the held boundary, manually stop all
strings. Keep the natural stop transient and untouched DI noise floor; do not
gate, trim or add an intentional pick/click during the reset. Use this same
stroke schedule in all ten files:

```text
slot:    1  2  3  4  5  6  7  8  9 10 11 12
stroke:  D  U  D  U  D  U  D  U  D  U  D  U
```

These are separately performed single strokes, not a continuous
alternate-picking motion. The common slot order supplies six within-session
repetitions per direction and removes the direct down/up reference mismatch.
Continuous alternate-picking gesture and history remain part of the rapid
comparison. Same-numbered slots in separately recorded files are not paired
physical trials.

## Rapid files and transition grooves

Record each rapid file as three separate 12-hit hard-alternate sixteenth-note
runs, one each at 120, 180 and 240 BPM. Before recording, reproducibly randomize
the tempo order separately by file while keeping the four-file assignment
balanced: for each BPM, its counts in run positions 1, 2 and 3 may differ by at
most one. Put the seed/draw ID in `tempo_order_id` and each actual order in that
take's `run_bpms_in_order`; the validator checks both permutation and balance.
Every run starts down. Use a monitoring-only four-beat count-in; no click or
count-in enters the recorded file. Record at least 11,025 frames of untouched
no-performance DI before the first attack, then leave at least 88,200 such
frames between runs and after the last run. Preserve the DI noise floor. Each
file is at least 407,007 frames.

For `dead-e1-e2-groove.wav`, record three runs of
`[E1, E1, E1, E2, E1, E1, E2, E1]` at 180 BPM sixteenths with the same
alternate/down-first, count-in and silence rules. It is at least 352,800 frames.
This puts E2 once on an upstroke and once on a downstroke per run, so string
transition is not identical to pick direction.

For `palm-open-e1-e2-groove.wav`, record three runs of
`[Palm E1, Palm E1, Palm E1, Open E2, Palm E1, Palm E1, Open E2, Palm E1]`
at 180 BPM eighth notes (0.16666667-second IOI). Use hard alternate picking,
start every run down and follow the same count-in, lead-in, inter-run silence,
tail-silence and audio-onset-alignment policy. Keep the heel at the measured
middle Palm landmark for every Palm E1, lift it fully clear of the strings for
every Open E2, then replant it at the middle landmark before the next Palm E1.
The Open E2 at hit 4 is an upstroke and the one at hit 7 is a downstroke. This
directly captures playable global bridge-hand lift/replant transitions without
confounding them with one pick direction. The file is at least 429,975 frames.

This 44.1 kHz v1 contract does not isolate phase-conditioned open-string
repicks. Use the separate
[`electry-repick-phase/v1` contract](../electry-repick-phase-v1/README.md) for
that experiment; do not add its files or fields to this manifest.

## Before delivery

Listen and inspect the waveforms at useful zoom. Confirm:

- every isolated file has exactly one audible hit in each slot and follows the
  D/U schedule;
- rapid files contain 12 hits at each tempo in their logged order, every run
  starts down, both grooves change strings at the declared notes, the
  Palm/Open groove also changes hand articulation as declared, and count-ins
  are absent;
- Open, each Palm landmark and fretting-hand Dead were performed with the
  correct hand; the logged pick point, Palm footprint/orientation and Dead hand
  landmark/shape stayed fixed; every Palm/Open-groove E2 has a fully clear heel
  and every following Palm E1 returns to the middle landmark;
  the far Palm still has an identifiable attack in every slot;
- the recorded lead-in contains only untouched DI noise floor, while gaps and
  tail contain no intentional performance after the natural stop transient;
  no take is clipped or contains a non-finite sample;
- no processing, cable/routing change or per-take gain change was applied.

Log any uncertain or unusually displaced far-Palm attack before analysis; do
not trim, move, repair or normalize individual hits. Then run from the Electry
repository root:

```sh
cmake -DELECTRY_MUTE_CAPTURE_DIR=/absolute/session/path \
  -P cmake/ValidateMuteCapture.cmake
```

Deliver only after the structural intake passes and the signed-rights document
hash in `manifest.json` matches the agreement.

After at least one train and one holdout session have arrived, the collection
coordinator runs:

```sh
cmake -DELECTRY_MUTE_CAPTURE_COLLECTION_DIR=/absolute/collection/root \
  -P cmake/ValidateMuteCaptureCollection.cmake
```

That collection command recursively reruns every session intake, rejects
duplicate `session_id` values, and rejects any declared `player_id` or
`guitar_id` appearing in both splits. It cannot detect a person or physical
guitar relabeled under a new ID; the coordinator's private identity ledger is
the evidence for those identities.

Capture intake is not a listening-study pack. After the frozen analyzer has
selected clips and the comparison exporter has cropped, level-matched and
rendered them, copy and fill both
[`comparison-manifest.template.json`](comparison-manifest.template.json) and
[`blind-study.template.json`](blind-study.template.json) for the separate
blinded A/B packer documented in
[`../../evaluation.md`](../../evaluation.md#frozen-palmdead-blind-listening-gate).
The comparison manifest is private. It binds the study ID, exact presentation
seed, participant count, two holdout clusters, two distinct train practice
sessions drawn from at least three disconnected engineering train clusters,
every physical/Electry stimulus, and the selection, render,
common-chain, engineering and listener-analysis contracts. Each source session
supplies the actual capture-manifest path and hash plus its declared
session/player/guitar IDs; the packer reads that file once, verifies the IDs and
split, requires the exact 16 canonical v1 take records and metadata, rejects
reused manifest or take-WAV hashes within or across sessions and disconnected
clusters, and archives the exact bytes. Core
cells freeze their content/processing, exact frames, capture take,
slot or run, stroke, event/score file and QC record. Dry/processed cells 5/6,
7/8 and 9/10 must share one source phrase, while the complete rotation remains
5/5 across the two holdout clusters. Cell 5/6 must select the capture take's
logged 180-BPM run.

The packer regenerates the selection proof from the private selection seed. It
ranks canonical candidates by SHA-256, chooses the lowest-ranked pair-preserving
5/5 holdout-cluster assignment, fixes two down and two up single-note cells,
then chooses the lowest-ranked eligible session/unit. Single draws contain all
six slots in the assigned direction, groove draws all three runs, and the rapid
draw only each session's logged 180-BPM run. Palm single cells use the middle
landmark chosen on train. The archived receipt contains every candidate and
rank; changing both a manifest choice and the receipt still fails regeneration.

Generate the presentation and stimulus-selection seeds separately with
`python3 -c 'import secrets; print(secrets.token_hex(32))'`. Keep both private,
never reuse either seed for another study and disclose them only after
unblinding. The packer rejects zero, repeated-pattern and low-variety
placeholder seeds.

Before rendering, fill
[`selection-input.template.json`](selection-input.template.json) from the
validated capture manifests, then generate the exact plan and receipt:

```sh
python3 Tools/PrepareBlindListening.py --generate-selection-receipt \
  /private/selection-input.json /private/selection-receipt.json
```

Copy the selected cluster/session/take/unit values from each receipt draw into
the comparison manifest (cells 6, 8 and 10 reuse draws 5, 7 and 9). Do not edit
the generated receipt. The packer independently regenerates it.

The v1 selection, render, chain and analysis settings have exact keys and
values; they are not arbitrary self-hashed notes. The common chain freezes the
current 8x effect oversampling at 44.1 kHz, and the analysis settings include
every numerical listener gate. For each `settings_sha256`,
hash the UTF-8 JSON serialization of the adjacent `settings` object with keys
sorted, ASCII escaping and compact `,`/`:` separators. `assets` may be empty
only when the frozen chain has no external asset or IR. The sealed artifact
registry must contain every declared build, implementation, scorer, preset and
asset hash. The frozen selection receipt repeats every chosen source/unit and
is checked against the cells. The engineering freeze enumerates the complete
v1 endpoint set, grids, depth mapping, aggregation, weights and numerical
no-regression margins. Each margin names its exact method:
balanced three-versus-three isolated repetitions, complete groove-run
resampling, or quantization alone for rapid takes. Qualification, recruitment,
replacement and no-outcome-based stopping rules are exact too.

The engineering derivation receipt and combined result are sealed attestations,
not a substitute for raw analysis. Each of at least three train clusters has a
separately hashed analysis result. Each isolated cluster-level repeatability
sample names one canonical 3v3 partition and carries the complete input set for
every relevant session, Open/mute take, string/contact and down/up stratum;
each groove sample similarly covers every relevant session/take for one complete
run. Rapid endpoints attest the complete set of logged 180-BPM inputs but add no
train repeatability sample. All ten canonical isolated partitions and all three
groove runs are exhaustively enumerated, so v1 has no resampling RNG or seed.
The v1 contract permits no omitted, duplicated or
excluded eligible unit and no analyzer failure flag. The packer cross-checks
those files, recomputes the cluster-sample R-7 P90, and binds the full engineering
contract hash. Retain the underlying train audio, analyzer output and private
identity ledger in the external immutable study archive.
The frozen analyzer, rather than the packer, emits filled copies of
[`engineering-train-analysis.template.json`](engineering-train-analysis.template.json),
[`engineering-derivation-result.template.json`](engineering-derivation-result.template.json)
and [`engineering-derivation-receipt.template.json`](engineering-derivation-receipt.template.json).
Fill and seal [`artifact-registry.template.json`](artifact-registry.template.json)
with every declared code/build/preset/asset and raw train-analysis hash. These
templates are structural guides and intentionally fail until actual analyzer
outputs, hashes and frozen statuses replace every placeholder.

Change the completed comparison status to `frozen`, copy its exact study ID,
presentation seed and stimulus hashes into the blind-study manifest, then
change the latter status to `frozen_ready_to_pack`. The packer reads every
referenced JSON or binary evidence file once for verification and writes those
same bytes to
`private/comparison-manifest.json`, `private/study-manifest.json`,
`private/capture-manifests/`, `private/event-scores/`,
`private/selection-receipt.json`, `private/engineering-derivation-*.json`,
`private/engineering-train-analysis/` and `private/artifact-registry.json`. It also
archives the exact preparer as `private/prepare.py`; private files use mode
`0600` and directories `0700`. The answer key contains relative archive paths
and hashes for later scoring verification. WAV stimuli are separately hashed,
container-inspected, copied under opaque names and rehashed on their first
public copy.

Both committed templates remain intentionally unusable, so the packer cannot
make the still-missing commissioned recordings appear to exist. A comparison
JSON containing only schema and status is not a freeze and is rejected. The
packer checks the capture manifest's schema, split, IDs, declared takes and
hashes used by the comparison; it does not rerun the CMake audio intake.

Before the first listener, record the printed study fingerprint outside the
pack, then serve it only with that independent anchor:

```sh
python3 serve.py --expected-fingerprint <externally-recorded-64-hex-fingerprint>
```

The server preflights the complete frozen pack, then hash-checks a one-read
snapshot on every GET/HEAD (including single-range audio requests) and refuses
any public file changed after preflight.

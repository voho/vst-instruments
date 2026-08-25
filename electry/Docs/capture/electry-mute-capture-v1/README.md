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

This recursively reruns every session intake, rejects duplicate `session_id`
values, and rejects any declared `player_id` or `guitar_id` appearing in both
splits. It cannot detect a person or physical guitar relabeled under a new ID;
the coordinator's private identity ledger and no-holdout-access procedure remain
the evidence for those facts.

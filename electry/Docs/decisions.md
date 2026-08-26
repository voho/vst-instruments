# Electry — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear,
never written up as though a measurement had settled it.

**No by-ear decisions have been recorded yet.** Electry's voicing choices so
far have been settled by measurement against reference recordings rather than
by listening test — see "The default voicing" and the palm-mute derivation in
the [README](../README.md#how-it-works), both of which quote the joint error
they were chosen on. Where a constant is voiced rather than literature-derived
it is labelled as voicing in the
[claim boundaries](../README.md#references-and-claim-boundaries).

## 2026-08-26 — deepen and lengthen the fixed Palm contact

The CC0 [`50hz-guitar`](https://freesound.org/people/inspektral/packs/42559/)
matrix adds two matched muted/sustained takes at nominal 50, 75 and 100 Hz from
a tagged seven-string baritone. Its mute hand and signal chain are unknown, so
it can establish only an extended-range spectral direction. HiMMP's CC-BY
[“In Solitude” rhythm DIs and score](https://himmp.net/faq.html) add 16 unique
low-C2 Palm strikes on the exact four-quarter-note schedule, but use a
conventional six-string Drop-C guitar. Neither source replaces the commissioned
E1/E2 TRAIN/HOLDOUT gate.

Both comparisons show the retained contact losing upper-band share too slowly.
Decision: increase the Palm contact mapping from 1.10 to 1.25 times the solved
bridge-hand depth and extend the fixed hold from 70 to 100 ms; retain the 4 mm
footprint and 10 ms release. At 44.1 kHz the controlled F2 paired contraction
moves from -3.59762 to -4.00942 dB. Across the three extended-range model notes,
the 0-60-to-60-160 ms paired contractions move from -8.31/-5.85/-7.08 to
-14.71/-9.26/-10.62 dB. The score-matched proxy's median body, tail and
selective contraction all move toward its real phrase. The rapid-repick
aggregate remains 2.240 dB median absolute mean and 3.428 dB median worst-cell
error, and the E2 soft-to-hard tail spread remains just above its 2.8 dB rail.

The bounded alternatives were:

| candidate | result | decision |
| --- | --- | --- |
| Add per-hit heel-centre jitter from stroke variation | F2 contraction weakened from -3.60 to -2.69 dB and the score correlation barely moved from 0.9969 to 0.9975; all four F2 rate rails failed | Rejected: the public audio does not identify random heel motion |
| Widen the provisional footprint from 4 to 6 mm | No material low-string improvement | Rejected: do not invent contact geometry |
| Raise depth from 1.25 to 1.40 at the 70 ms hold | F2 changed only from -4.010 to -4.021 dB and the extended matrix moved by at most 0.03 dB | Rejected: default contact was already saturated |
| Extend the hold to 120 or 140 ms | Extended-range contraction improved further, but E2 soft-to-hard tail spread fell to 2.776 or 2.754 dB against the retained 2.8 dB minimum | Rejected: spectral fit cannot erase a playable velocity range |

The complete 100 ms hold plus 10 ms release now forms the Palm CPU hot-window.
Eight active strings render it at 0.205x realtime at 96 kHz on the checkpoint
machine. The isolated all-string tuning spread remains 0..2.5 cents and the
simultaneous low chord remains -4.25..+0.75 cents.

## 2026-08-26 — keep the fixed Palm contact; reject an attack/recovery ramp

The retained finite-width contact already produces monotonically increasing
Palm-versus-Open upper-band loss across sliding 30 ms F2 windows. That is the
expected accumulated action of a fixed loss inside the feedback loop; the
exact P1, P2 and Electry trajectories are recorded in
[the evaluation](evaluation.md#provisional-finite-width-palm-checkpoint).

[Biral, d'Alessandro and Freed](https://www.icmc14-smc14.net/images/proceedings/PS4-B10-TowardsaDynamicModel.pdf)
measured pressure dips around picks, not the simultaneous acoustic loss or a
loss-recovery time constant.
[Reboursière et al.](https://www.nime.org/proceedings/2012/nime2012_213.pdf)
measured a faster post-attack high-frequency envelope, but that acoustic result
cannot distinguish changing pressure from repeated traversal of a fixed
contact. [Pluta, Tokarczyk and Wiciak](https://www.mdpi.com/2076-3417/12/3/1659)
also found that re-excited string state at 12 and 70 ms did not reduce to a
fresh isolated pluck.

Decision at that checkpoint: do not add a per-attack pressure/recovery ramp.
Its shape and timing
would be unidentifiable from the available audio, and resetting it on every
repick would be the wrong state model for rapid chugs. The then-current 70 ms
hold remained explicitly provisional and the 10 ms release was labelled as
generic smoothing, not measured Palm biomechanics. No DSP constant changed at
that checkpoint; the later evidence-backed duration decision is recorded
above.

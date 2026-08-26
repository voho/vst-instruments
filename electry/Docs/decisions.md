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

Decision: do not add a per-attack pressure/recovery ramp. Its shape and timing
would be unidentifiable from the available audio, and resetting it on every
repick would be the wrong state model for rapid chugs. Keep the 70 ms hold
explicitly provisional and the 10 ms release labelled as generic smoothing,
not measured Palm biomechanics. No DSP constant changed at this checkpoint.

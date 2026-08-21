# Working conventions

- Per-fix audio previews (the before/after/diff takes under
  `*/Docs/audio/realism-fixes/`, `fidelity-fixes/` and similar) are
  **one-time review evidence**: render them while the change is under
  review, then leave them frozen once the pull request merges. Do not
  re-render or refresh them when later engine changes make them stale,
  and do not treat their staleness as a defect.

## A–Z listening tests

These instruments are physical models with no owned calibration captures, so
there are decisions the measurements cannot close. When a change is audible and
the physics admits more than one defensible option, **render the options as
WAVs, letter them, and let the user decide the direction by ear.** A listening
test is a legitimate way to choose between candidates. It is not a way to fit a
number — see the limit below.

**When to run one.** Whenever any of these is true:

- two or more formulations are each derivable and the model cannot say which
  describes the instrument (which radiation law a body obeys, which of two
  reference geometries a term should be measured against);
- a change is audible and the open question is whether it is an improvement
  rather than whether it is correct;
- a calibrated constant has to be re-pinned and more than one pinning is
  defensible;
- the user asks for one.

Do not run one to decide something a measurement already decides. If a number
settles it, quote the number.

**How to build one.**

- **A is always the shipping engine**, unchanged. B, C, … Z are the candidates.
  So "A against B" is always "what it does now against what it would do".
- **Letters only in the filenames.** `A.wav`, `B.wav`, … carry no hint of the
  mechanism, so the listener is not primed by the label. Write the key down at
  the same time, in a separate `key.md` the user can read after deciding, and
  say in the handover that the key exists and is unread-by-design.
- **Change one thing.** Identical MIDI, seed, sample rate, block size, length
  and controls across every letter; only the mechanism under test differs.
- **Level-match, and say how.** Match on whole-file RMS unless level is itself
  what is being judged, and state the measure and the trims applied. Louder
  reads as better, and an unmatched set tests loudness.
- **Give the same material to every letter**, and pick material that exposes the
  thing under test — a stroke that drives the mechanism hard, not a general
  demo. Keep it short enough to A/B repeatedly.
- Render through the shipping signal path, never through an offline
  approximation of it.

**Where they live.** Listening tests are decision artefacts, not review
evidence. Render them to a working directory and send them to the user; do not
commit them by default — a 26-file set per decision would bloat the repository.
Commit a set only when the user asks, under
`*/Docs/audio/listening-tests/<date>-<topic>/` with its `key.md`, and freeze it
on merge exactly like the per-fix previews above.

**What gets committed is the verdict.** Record in that instrument's decision
log — `Docs/best-in-class-plan.md`, or `Docs/research.md` where an
instrument's research docs are consolidated (youknow106) — what was being
decided, what each letter was, which
letter the user chose, and what that choice licensed. A direction chosen by ear
is recorded as chosen by ear — never written up as though a measurement had
settled it.

**The limit.** A listening test chooses between candidates that are each already
defensible on their own physics. It does not license drawing a constant, fitting
a curve, or keeping a formulation the measurements reject. If every candidate
needs an invented number, the answer is still that none of them ships.

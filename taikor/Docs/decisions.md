# Taikor — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear,
never written up as though a measurement had settled it.

## 2026-09-05 — repeated strokes retain contact variation

**The question.** Should Humanise 0 produce identical strokes, or retain
subtle differences in the physical stick/head contact?

| Letter | What it was | Outcome |
| --- | --- | --- |
| A | Previous engine, with exact repetition at Humanise 0 | baseline |
| B | Always-on impact-speed and actual contact-stiffness variation, with fresh contact texture | **chosen** |

**The comparison.** Identical 12.4-second, 32-hit scores across all four
drums, with Don and Rimshot at velocity 0.80. Humanise and Stick Noise were
zero, Drive was zero, Low Cut was off, and Performer was P1. Both started
from the same reset state at 44.1 kHz with 256-sample render blocks. Natural
tails overlapped. Whole-file stereo RMS was matched to −26 dBFS using only
constant gain: +8.425475 dB for A and +8.376360 dB for B. Review files and
their key remain in the working directory `build/stroke-variation-review/`.

**Verdict: B, by ear.** The user chose “B is better.” This records a listening
preference, not a measurement of a player's natural variation limits.

**What it licensed.** Retain the current B implementation: at Humanise 0,
incoming speed varies by up to ±0.5%, and a ±0.4% Hertz contact-time factor
changes the stiffness used by both the duration estimate and the live
collision. Humanise adds wider variation and position scatter. The authored
position at zero, MIDI onset timing and resting drum tuning stay fixed.
Successive hits advance the variation through silence and Panic; reset
replays the same performance. Stick Noise was disabled in the comparison,
so this choice specifically supports the mechanical contact variation.

## 2026-08-19 — the body moves with the pair

**The question.** Mic Distance moved the head's near field and the head's
continuum, but left the wooden shell at one fixed level — so backing the pair
off thinned the drum around a body that never receded. What should replace the
fixed level?

**The set.** Six candidates rendered against the shipping engine: Don Rim at
three Mic Distances on the ō-daiko and the okedo, level-matched on the
factory-position sections, letters carrying no hint of the mechanism.

| Letter | What it was | Outcome |
| --- | --- | --- |
| A | shipping engine | baseline |
| B | perspective, normalised per drum | **chosen** |
| C | `radiationEfficiency` on the level, re-pinned 29.5× | not chosen |
| D | perspective, normalised against one fixed 15.95 cm | not chosen; peak 0.985, past the suite's 0.95 clause |
| E | `radiationEfficiency` on the level, un-re-pinned | not chosen; the body nearly disappears |
| F | perspective, near field only | not chosen |

**Verdict: B, by ear.** The measurements rule out C, D and E on their own
terms — C needs a 29.5× fitted re-pin, D breaks a suite clause, E is
uncalibrated — but nothing measurable separates B from F, which differ only in
whether the propagating share the head carries reaches the body as well. B
keeps the construction identical to the head's; that it also sounds right is
the listener's finding, not a derivation.

**What it licensed.** A ring mode is now read the way a membrane mode is: an
evanescent term at its own circumferential wavenumber `n/R` over the
wall-to-capsule path, plus the same proximity lift and propagating share, taken
as a ratio against each drum's own capsule distance at the factory Mic
Distance. Every factory preset renders exactly as it did. At 40 cm the
ō-daiko's lowest ring mode is 3.6 dB down and the okedo's 16.0 dB, while the
top of either bank moves under a decibel.

Guarded by `testTheBodyMovesWithThePair`, which holds four clauses recomputed
from the resolved drum: the factory Mic Distance moves nothing on any drum or
ring mode; every other distance moves it monotonically, closer being louder;
the highest ring mode falls off with distance at less than half the rate of the
lowest, which is the evanescent signature rather than a taper; and a rim shot's
wooden bank recedes as the pair backs off while a head-only stroke still has no
wooden bank at all.

Note that gating the shell's *level* on `radiationEfficiency` — once named as
the missing term — is the wrong term in the wrong place: it is a far-field
power law and the pair is in the shell's near field.

# Septum — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear:
none of these closes an open calibration question, and the captures the
[known gaps](../README.md#known-gaps) name are still what would.

## 2026-08-22 — three calibration questions

Three sets were rendered through the shipping signal path with identical MIDI,
seed, sample rate, block size, length and controls; only the mechanism under
test differed. `A` was the shipping engine in each. Each set was level-matched
on whole-file RMS against `A`, with one further set-wide gain so the files were
comfortable to audition — the same factor on every letter, so the match was
untouched. The key was unread until after each choice. The sets were handed
over rather than committed.

| Set | Letters | Question |
| --- | --- | --- |
| resonance curve | A, B, C | The resonance-to-Q shape between two settled endpoints (OQ-08). 10.5 s: a slow RESONANCE sweep under one held note, then a bass line with the knob at dead centre |
| supersaw HPF | A, B | The tracked high-pass on the summed seven-saw stack: 1.0 × f₀ at Q = 0.707 against 2.5 × f₀ at Q = √2 (OQ-04) |
| supersaw mix | A, B, C | Where to evaluate Szabo's measured mix laws, given the SH-201 has no MIX knob (OQ-05) |

**Verdicts.**

| Set | Chosen | What that licensed |
| --- | --- | --- |
| resonance curve | **B** — the square-root taper, `k = 2 − 2.04·√(v/127)` | Re-pinned. Both settled endpoints untouched; only the shape between them moved |
| supersaw HPF | **A**, the shipping engine — the listener was unsure, calling it "probably a bit better" | Nothing. 1.0 × f₀ at Q = 0.707 stays, and OQ-04 keeps its standing-candidate status: an unsure preference for the incumbent is not evidence about the hardware |
| supersaw mix | **A**, the shipping engine, "probably ok" | Nothing. *m* = 0.75 stays, OQ-05 unchanged |

Measured after re-pinning the resonance curve, gain referred to the filter's
own passband at cutoff 64:

| RESONANCE | −12 dB peak, before | after | −24 dB peak, before | after |
| --- | --- | --- | --- | --- |
| 0 | +0.06 dB | +0.06 dB | +0.06 dB | +0.06 dB |
| 64 | +1.38 dB | **+5.43 dB** | +1.58 dB | **+4.73 dB** |
| 100 | +8.18 dB | +14.39 dB | +7.05 dB | +12.80 dB |
| 120 | +22.70 dB | +33.32 dB | +21.12 dB | +31.73 dB |

The centre of the knob now does something audible and the oscillation
threshold has not moved: the top of the travel still crosses into the bounded
self-oscillation the manual warns about, and the test that fences that still
passes.

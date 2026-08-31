# Eastman E1D steel-string acoustic — first-party source recordings

Two performance masters of Arthur's own **Eastman E1D** dreadnought steel-string
acoustic guitar, recorded 2026-07-23. They are the source for the default (and one
alternate) GM 25 steel-string banks in ferrosintesis — replacing the fetched Martin
HD28 bank as the default, which moves to a bank-select alternate.

- **`picked.opus`** — plectrum articulation (bright attack). Source `DR0000_0192.wav`.
  → baked into the **default** GM 25 bank (`eastman_picked`).
- **`plucked.opus`** — fingerstyle / hand-plucked (warm, ~4–5× darker centroid).
  Source `DR0000_0191.wav`. → baked into the CC0=1 **alternate** bank (`eastman_plucked`).

## Recording

| | |
|---|---|
| Instrument | Eastman E1D dreadnought, steel strings |
| Performer | Arthur |
| Master format | 44.1 kHz · 16-bit · **stereo, coincident pair** (inter-channel delay ~0.08 ms — mono-sums with −0.1…−0.3 dB, no comb filtering) |
| Content | chromatic walk up the neck, **E2 → ~C6**, ~80 discrete notes per take |
| Quality | no clipping; peak −4.6 dBFS (plucked) / −6.6 dBFS (picked); noise floor ~−66 dBFS |
| Tuning | ~+10…+30 cents sharp overall — irrelevant to the render: the sampler repitches from each zone's **measured** f0 |
| Licence | **CC0-1.0** (public-domain dedication by the repo owner) |

Kept as Opus (160 kbps, music mode) to save ~9× on the git tree; the full
performances are preserved uncut (the takes are wall-to-wall, with note tails ringing
5–7 s into the following gaps, so there is no dead air to trim without damaging
decays). The 44.1 kHz/16-bit lossless originals are Arthur's authoritative masters;
these Opus copies are the in-repo archive and the re-slice source.

## Zone slice map (verified anchors)

The banks keep the Martin/nylon layout — 8 zones at ~6-semitone spacing spanning
E2–B5 — one onset (~0.9 s) per zone; the Karplus-Strong model carries the decay.
The pre-cut single-note sources live in `zones/` and are what `prepare.py` bakes;
they are committed lossless so the bank is reproducible offline (same rationale as
`tools/ferrosintesis-samples/gong-src/`). Times below are the onset in each master.

Anchors were chosen by octave-resolved harmonic matching over every note in the take,
then filtered on **isolation** — the quiet before the onset must exceed ~30 dB, because
`trim_to_onset` locates the attack as the first sample above 3 % of peak (−30 dB) and
would otherwise trigger on the previous note's decay.

| Zone | `picked` (0192) | measured f0 | `plucked` (0191) | measured f0 |
|------|-----------------|-------------|------------------|-------------|
| low E   | 1.720 s  | 82.94 Hz  | 5.700 s   | 83.45 Hz  |
| ~A#2    | **B2** 44.490 s | 124.23 Hz | A#2 64.120 s | 117.16 Hz |
| E3      | 81.150 s | 166.01 Hz | 102.840 s | 166.01 Hz |
| A#3     | 132.250 s | 233.06 Hz | 139.040 s | 233.06 Hz |
| E4      | 180.480 s | 330.23 Hz | 189.450 s | 331.25 Hz |
| ~B4     | **A#4** 211.480 s | 467.92 Hz | B4 228.900 s | 494.63 Hz |
| F5      | 244.080 s | 700.86 Hz | 254.100 s | 700.86 Hz |
| B5      | 276.320 s | 993.07 Hz | 282.050 s | 990.02 Hz |

**Two picked-take substitutions.** Zone roots are *measured*, not nominal, so an anchor
only has to sit near its slot on the grid. The picked take's A#2 falls inside the
preceding note's decay (28 dB isolation) and its B4 is both quiet and poorly isolated
(20 dB), so **B2** and **A#4** stand in — each with ~40–46 dB isolation. This mirrors
the nylon bank, where B2 already stands in for that source's missing A#2.

**On the picked low E.** Its fundamental sits below the 2nd/3rd harmonics in the attack
window (H1 −3.5 dB, H3 dominant), so a dominance-weighted pitch detector reports it an
octave high. That is normal dreadnought low-E behaviour and *not* a defect: the zone it
replaces — the shipped Martin `steel_E2` — has an even weaker fundamental (H1 −6.0 dB,
H3 dominant) at the same pinned root. Perceived pitch follows the harmonic series, not
the loudest partial. Pin the root to the true fundamental.

The top zones (F5, B5) ring shorter and quieter — expected acoustic-guitar physics, and
the same situation the Martin bank already lives with (its `steel_B5` is 0.706 s, the
binding case of the `guitar_zone_fade_budget` oracle).

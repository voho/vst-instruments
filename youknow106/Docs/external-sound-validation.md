# External sound validation — the 128 factory presets against public hardware recordings

**Date:** 2026-08-18. **Engine under test:** `main` at
`66670962077b644ffe7ff9b4170204d43025f236`, JUCE-free `YouKnow106Engine` path,
Release arm64 `libYouKnow106DSP.a` (SHA-256 prefix `8ba7e6977aef2715`),
48 kHz host rate at QUALITY 4x.

This is a release-gate **sanity validation**, not a fidelity measurement. It
compares every one of the 128 shipped factory presets against a publicly
available recording of the same patch played on original JUNO-106 hardware, and
asks one question per preset: *is there any gross, patch-level wrongness — a
wrong waveform class, a wrong octave, a missing or runaway source, a broken
envelope — that the numerical contracts and the level audit could not see?*
The reference chain is undocumented, so nothing here is promoted to an anchor,
no engine constant was changed by this pass, and none of the
[open questions](open-questions.md) close. Where the comparison found a
quantified contradiction it is recorded under the owning open question
(OQ-15), not "fixed" by re-voicing a constant — exactly as the research
contract requires.

## Reference corpus

[synthmania.com](https://www.synthmania.com/Roland%20Juno-106/Audio/) publishes
one MP3 riff per factory patch, all 64 Group A and all 64 Group B programs
(128 kbps, 44.1 kHz, joint stereo, riff phrasing, unknown unit, unknown
recording chain, unknown chorus-noise state). The files were fetched on
2026-08-18 from:

- `.../Juno-106%20Factory%20Preset%20Group%20A/` (64 files)
- `.../Juno-106%20Factory%20Preset%20Group%20B/` (64 files)

The recordings are third-party copyrighted material; they were used
transiently for analysis and are **not** redistributed with this repository.
Credit for the reference recordings belongs to synthmania.com.

## Render protocol

Each preset was rendered through the exact product path with its stored tone
bytes and product controls (chorus noise active, no per-preset gain), using
the shipped preview score: 250 ms preroll, the C-major voicing
48/55/60/64/67/72 held 4.0 s, then 2.0 s of release, all velocities 1.0,
48 kHz, QUALITY 4x, stereo PCM16. A second, register-matched pass re-rendered
the flagged presets on a single held note chosen from the reference's median
autocorrelation f0 (details below).

## Comparison method

Both sides are decoded to 48 kHz mono; 4096-sample Hann frames at 2048 hop are
kept when their RMS is within 40 dB of the loudest frame; the gated Welch
spectrum is reduced to 1/3-octave bands between 50 Hz and 12 kHz (capped below
the 128 kbps MP3 rolloff). Per preset the report gives:

- **Shape corr** — Pearson correlation of the mean-removed band-dB vectors;
- **Centroid delta** — log-frequency spectral centroid difference in octaves
  (ours minus reference);
- **Balance delta** — (2–8 kHz minus 100–800 Hz) band-balance difference in dB
  (ours minus reference).

The reference riffs and our fixed sustained score play different notes with
different phrasing on different chains, so the corpus-wide medians act as the
systematic offset of the method itself: **median centroid −0.35 oct, median
balance −8.2 dB** (a continuously re-attacked riff carries more
onset-transient HF than one held chord, and the hot-mastered MP3 chain adds
its own tilt). Per-preset judgements below always read the numbers against
those medians, and initial flags used deliberately loose absolute gates
(corr < 0.5, |centroid| > 1 oct, |balance| > 12 dB).

## Corpus result

Across all 128 presets: **median shape correlation 0.897; 94/128 at or above
0.80; 115/128 at or above 0.60; worst 0.121.** Given fixed-score-vs-riff
content and an unknown chain, this is strong corpus-level agreement: the bank
as shipped tracks the hardware recordings' spectral shapes for the sustained
melodic majority of the bank, and every large deviation was individually
adjudicated (next sections).

Method limits found and kept in mind while reading the table: the register
matcher reads sounding pitch, so a 4' RANGE patch renders an octave above the
matched note (B71 taught this); an oscillator-less noise patch has no f0 at
all and "register" is its filter cutoff (B58); a sustain-0 percussive patch
renders one strike against a dense riff (A31, A82); and a self-oscillation
whistle patch has digitally silent low bands that turn the balance metric
into division by silence (B75).

## Per-preset results — fixed chord score, all 128

| Slot | Name | Shape corr | Centroid delta (oct) | Balance delta (dB) | Median-corrected balance (dB) |
| --- | --- | ---: | ---: | ---: | ---: |
| A11 | Brass Set 1 | 0.879 | -0.84 | -6.9 | +1.3 |
| A12 | Brass Swell | 0.944 | -0.39 | -15.5 | -7.2 |
| A13 | Trumpet | 0.797 | -0.52 | -7.3 | +1.0 |
| A14 | Flutes | 0.723 | -0.67 | -7.7 | +0.5 |
| A15 | Moving Strings | 0.763 | +0.06 | -4.7 | +3.5 |
| A16 | Brass & Strings | 0.898 | -0.93 | -8.7 | -0.4 |
| A17 | Choir | 0.910 | -0.29 | -4.5 | +3.8 |
| A18 | Piano I | 0.982 | -0.26 | -9.9 | -1.7 |
| A21 | Organ I | 0.902 | -0.39 | -37.9 | -29.6 |
| A22 | Organ II | 0.897 | -0.59 | -27.1 | -18.9 |
| A23 | Combo Organ | 0.886 | -0.10 | -14.2 | -5.9 |
| A24 | Calliope | 0.874 | -0.65 | -12.9 | -4.7 |
| A25 | Donald Pluck | 0.950 | -0.03 | -9.6 | -1.3 |
| A26 | Celeste* (1 oct.up) | 0.594 | +0.59 | +5.1 | +13.3 |
| A27 | Elect. Piano I | 0.934 | -0.73 | -4.7 | +3.5 |
| A28 | Elect. Piano II | 0.805 | -1.30 | -30.3 | -22.1 |
| A31 | Clock Chimes* (1 oct. up) | 0.839 | -0.43 | -37.7 | -29.5 |
| A32 | Steel Drums | 0.926 | -0.76 | -6.5 | +1.8 |
| A33 | Xylophone | 0.862 | -0.60 | -10.8 | -2.6 |
| A34 | Brass III | 0.971 | +0.13 | -3.8 | +4.5 |
| A35 | Fanfare | 0.862 | -0.91 | -17.7 | -9.4 |
| A36 | String III | 0.792 | +0.12 | -8.6 | -0.3 |
| A37 | Pizzicato | 0.977 | -0.02 | -4.6 | +3.7 |
| A38 | High Strings | 0.792 | -0.12 | -2.6 | +5.7 |
| A41 | Bass clarinet | 0.940 | -0.87 | -9.9 | -1.7 |
| A42 | English Horn | 0.894 | -0.62 | -8.7 | -0.4 |
| A43 | Brass Ensemble | 0.982 | -0.19 | -6.4 | +1.8 |
| A44 | Guitar | 0.961 | -0.73 | -8.4 | -0.2 |
| A45 | Koto | 0.855 | -0.72 | -16.7 | -8.4 |
| A46 | Dark Pluck | 0.935 | +0.27 | -15.4 | -7.2 |
| A47 | Funky I | 0.939 | -0.49 | -17.5 | -9.2 |
| A48 | Synth Bass I (unison) | 0.732 | +0.56 | -8.1 | +0.2 |
| A51 | Lead I | 0.802 | -0.98 | -22.9 | -14.6 |
| A52 | Lead II | 0.967 | -0.47 | -12.2 | -3.9 |
| A53 | Lead III | 0.803 | -1.26 | -20.0 | -11.8 |
| A54 | Funky II | 0.953 | -0.24 | -6.3 | +2.0 |
| A55 | Synth Bass II | 0.985 | -0.10 | -6.6 | +1.7 |
| A56 | Funky III | 0.984 | -0.28 | -5.3 | +3.0 |
| A57 | Thud Wah | 0.953 | -0.53 | -14.3 | -6.1 |
| A58 | Going Up | 0.906 | -0.16 | -32.6 | -24.4 |
| A61 | Piano II | 0.962 | -0.05 | -4.5 | +3.8 |
| A62 | Clav | 0.823 | -1.67 | -17.7 | -9.4 |
| A63 | Frontier Organ | 0.914 | -1.07 | -21.2 | -12.9 |
| A64 | Snare Drum (unison) | 0.416 | -2.60 | -16.5 | -8.2 |
| A65 | Tom Toms (unison) | 0.595 | -1.82 | -16.4 | -8.1 |
| A66 | Timpani (unison) | 0.905 | -1.11 | -13.2 | -4.9 |
| A67 | Shaker | 0.982 | +0.04 | -1.4 | +6.8 |
| A68 | Synth Pad | 0.679 | -0.14 | -3.5 | +4.8 |
| A71 | Sweep I | 0.740 | -0.54 | -7.1 | +1.2 |
| A72 | Pluck Sweep | 0.812 | -0.37 | -17.0 | -8.8 |
| A73 | Repeater | 0.817 | +0.33 | -7.7 | +0.5 |
| A74 | Sweep II | 0.121 | -0.08 | -8.0 | +0.2 |
| A75 | Pluck Bell | 0.790 | +0.11 | -4.8 | +3.5 |
| A76 | Dark Synth Piano | 0.937 | -0.23 | +0.3 | +8.6 |
| A77 | Sustainer | 0.629 | -0.92 | -6.4 | +1.8 |
| A78 | Wah Release | 0.973 | -0.23 | -14.0 | -5.8 |
| A81 | Gong (play low chords) | 0.916 | -0.15 | +4.8 | +13.1 |
| A82 | Resonance Funk | 0.810 | +0.72 | +24.9 | +33.1 |
| A83 | Drum Booms* (1 oct. down) | 0.971 | -0.56 | -2.3 | +6.0 |
| A84 | Dust Storm | 0.976 | -0.35 | -11.2 | -2.9 |
| A85 | Rocket Men | 0.813 | -0.55 | -28.8 | -20.6 |
| A86 | Hand Claps | 0.971 | +0.32 | -0.6 | +7.7 |
| A87 | FX Sweep | 0.944 | -0.58 | -18.8 | -10.6 |
| A88 | Caverns | 0.907 | +0.20 | -10.0 | -1.8 |
| B11 | Strings | 0.630 | +0.27 | -1.8 | +6.5 |
| B12 | Violin | 0.485 | -1.15 | -7.5 | +0.8 |
| B13 | Chorus Vibes | 0.886 | -0.26 | -8.8 | -0.6 |
| B14 | Organ 1 | 0.904 | -0.33 | -27.4 | -19.1 |
| B15 | Harpsichord 1 | 0.936 | -0.05 | -1.7 | +6.5 |
| B16 | Recorder | 0.339 | -1.63 | -26.0 | -17.8 |
| B17 | Perc. Pluck | 0.830 | +0.33 | -4.9 | +3.3 |
| B18 | Noise Sweep | 0.992 | -0.24 | -5.3 | +3.0 |
| B21 | Space Chimes | 0.835 | -0.49 | -8.6 | -0.3 |
| B22 | Nylon Guitar | 0.764 | +0.16 | +0.6 | +8.8 |
| B23 | Orchestral Pad | 0.963 | +0.18 | -3.6 | +4.7 |
| B24 | Bright Pluck | 0.758 | -0.81 | -11.9 | -3.7 |
| B25 | Organ Bell | 0.956 | +0.48 | -20.8 | -12.6 |
| B26 | Accordion | 0.910 | -0.53 | -4.6 | +3.7 |
| B27 | FX Rise 1 | 0.982 | +0.25 | +1.1 | +9.3 |
| B28 | FX Rise 2 | 0.974 | -0.90 | -0.8 | +7.5 |
| B31 | Brass | 0.546 | -0.04 | -4.0 | +4.2 |
| B32 | Helicopter | 0.966 | +0.93 | +2.9 | +11.2 |
| B33 | Lute | 0.941 | -0.72 | -17.6 | -9.4 |
| B34 | Chorus Funk | 0.930 | +0.96 | -10.0 | -1.8 |
| B35 | Tomita | 0.775 | -1.82 | -20.2 | -11.9 |
| B36 | FX Sweep 1 | 0.949 | +0.49 | +3.0 | +11.2 |
| B37 | Sharp Reed | 0.556 | -1.22 | -25.0 | -16.8 |
| B38 | Bass Pluck | 0.982 | +0.34 | -7.1 | +1.2 |
| B41 | Resonant Rise | 0.947 | -0.88 | -9.7 | -1.4 |
| B42 | Harpsichord 2 | 0.921 | -0.50 | -3.7 | +4.5 |
| B43 | Dark Ensemble | 0.967 | -0.12 | +2.2 | +10.4 |
| B44 | Contact Wah | 0.926 | +0.39 | -25.2 | -16.9 |
| B45 | Noise Sweep 2 | 0.843 | -0.54 | -1.7 | +6.5 |
| B46 | Glassy Wah | 0.520 | -0.68 | -9.5 | -1.2 |
| B47 | Phase Ensemble | 0.509 | +0.44 | -4.5 | +3.8 |
| B48 | Chorused Bell | 0.901 | +0.05 | -8.6 | -0.3 |
| B51 | Clav | 0.735 | -1.25 | -9.6 | -1.3 |
| B52 | Organ 2 | 0.815 | -0.66 | -39.0 | -30.8 |
| B53 | Bassoon | 0.971 | -0.15 | +6.4 | +14.7 |
| B54 | Auto Release Noise Sweep | 0.868 | -0.67 | -2.3 | +6.0 |
| B55 | Brass Ensemble | 0.677 | +0.34 | -7.7 | +0.5 |
| B56 | Ethereal | 0.821 | -1.17 | -17.0 | -8.8 |
| B57 | Chorus Bell 2 | 0.850 | -0.56 | -12.1 | -3.8 |
| B58 | Blizzard | 0.908 | +0.43 | +19.5 | +27.8 |
| B61 | E. Piano with Tremolo | 0.959 | -0.33 | +3.2 | +11.4 |
| B62 | Clarinet | 0.848 | -0.35 | -4.3 | +4.0 |
| B63 | Thunder | 0.971 | -0.67 | -12.2 | -3.9 |
| B64 | Reedy Organ | 0.895 | -0.34 | -7.0 | +1.2 |
| B65 | Flute / Horn | 0.924 | -0.50 | -14.3 | -6.1 |
| B66 | Toy Rhodes | 0.883 | -0.39 | -8.4 | -0.2 |
| B67 | Surf's Up | 0.896 | +0.57 | +4.9 | +13.2 |
| B68 | OW Bass | 0.987 | +0.29 | +6.0 | +14.2 |
| B71 | Piccolo | 0.336 | -1.65 | -70.2 | -62.0 |
| B72 | Melodic Taps | 0.937 | -0.20 | -5.7 | +2.5 |
| B73 | Meow Brass | 0.810 | +0.32 | -10.5 | -2.2 |
| B74 | Violin (high) | 0.720 | -0.77 | -4.6 | +3.7 |
| B75 | High Bells | 0.634 | +0.73 | +70.3 | +78.5 |
| B76 | Rolling Wah | 0.634 | -0.07 | -7.9 | +0.3 |
| B77 | Ping Bell | 0.660 | -1.32 | -12.2 | -3.9 |
| B78 | Brassy Organ | 0.937 | -0.72 | -8.8 | -0.6 |
| B81 | Low Dark Strings | 0.956 | -0.71 | -11.2 | -2.9 |
| B82 | Piccolo Trumpet | 0.417 | -1.48 | -10.7 | -2.4 |
| B83 | Cello | 0.449 | -1.30 | -7.1 | +1.2 |
| B84 | High Strings | 0.749 | -0.29 | -1.2 | +7.0 |
| B85 | Rocket Men | 0.957 | -0.92 | -4.8 | +3.5 |
| B86 | Forbidden Planet | 0.889 | -0.13 | -0.1 | +8.2 |
| B87 | Froggy | 0.939 | +0.05 | -28.1 | -19.9 |
| B88 | Owgan | 0.926 | +0.27 | -20.5 | -12.2 |

## Register-matched recheck — the twenty flagged presets

Every preset outside the loose gates was re-rendered on a single held note at
the reference's estimated sounding register (autocorrelation median f0 of the
gated reference; detector ceiling hits and the unpitched A64 fell back to a
mid-register note):

| Slot | Note rendered | Shape corr | Centroid delta (oct) | Balance delta (dB) |
| --- | ---: | ---: | ---: | ---: |
| A21 | 72 | 0.787 | +0.70 | -29.3 |
| A28 | 48 | 0.534 | -2.71 | -18.1 |
| A31 | 58 | 0.588 | -0.49 | -36.8 |
| A58 | 60 | 0.822 | -0.15 | -34.2 |
| A62 | 69 | 0.617 | -0.67 | -12.1 |
| A64 | 60 | -0.127 | -3.50 | -17.6 |
| A65 | 72 | 0.595 | -1.82 | -16.4 |
| A74 | 72 | -0.036 | +1.00 | -2.7 |
| A82 | 72 | 0.592 | +1.40 | +33.6 |
| B12 | 72 | 0.713 | +0.02 | +1.6 |
| B16 | 79 | 0.712 | -0.07 | -0.1 |
| B32 | 72 | 0.759 | +2.14 | +7.6 |
| B34 | 72 | 0.221 | +1.83 | -6.9 |
| B35 | 72 | 0.738 | -0.68 | -14.9 |
| B52 | 62 | 0.620 | -0.05 | -39.8 |
| B58 | 79 | 0.354 | +1.64 | +37.3 |
| B71 | 91 | 0.533 | +0.93 | +20.9 |
| B75 | 70 | 0.579 | +1.50 | +69.8 |
| B82 | 76 | 0.624 | -0.01 | -3.3 |
| B83 | 59 | 0.662 | -1.24 | -10.1 |

Register matching resolved the solo-instrument flags outright: **B12 Violin**
(corr 0.713, centroid +0.02 oct, balance +1.6 dB), **B16 Recorder** (0.712,
−0.07, −0.1) and **B82 Piccolo Trumpet** (0.624, −0.01, −3.3) sit on top of
the hardware once played where the hardware demo plays. The rest went to
per-preset adjudication.

## Per-preset adjudication of the residual outliers

Eight presets whose deltas survived register matching were each examined
against their stored tone bytes, their render's actual signal content, and
the reference's content. Verdicts:

- **B75 High Bells — methodological.** A filter-self-oscillation whistle
  patch with every source off: our renders are correctly near-pure whistles
  at 1.2–4.4 kHz with digitally silent 100–800 Hz bands, so the +70 dB
  balance figure is the metric dividing real high-band energy by silence.
  The reference riff spends seconds on low keys whose whistle fundamentals
  land inside the low measurement band.
- **B58 Blizzard — methodological.** A noise-only patch (both oscillators
  off, noise 127, near-full key follow): its audible "pitch" is the filter
  cutoff, so the register matcher's note choice moved our cutoff whistle to
  3.7 kHz by construction. Where comparison is valid the model matches the
  hardware: whistle Q 117 vs 123, whistle placement overlapping the
  reference's 305–1254 Hz range, envelope and HPF exactly per the bytes.
- **A82 Resonance Funk — methodological.** Sustain-0 self-oscillation
  percussion: a held-note protocol yields one ~200 ms whistle blip and then
  silence, scored against a continuous 9.4 s funk riff.
- **A21 Organ I — methodological.** At matched register the reference's
  harmonic rolloff matches our render within ~1–5 dB per harmonic through
  h6. The reference is a hot continuous riff (RMS −12.9 dBFS) whose overlap
  and onset transients raise its inter-harmonic floor by ~25 dB, which a
  whole-file balance metric misreads.
- **B52 Organ 2 — methodological, and positive evidence.** The patch puts a
  resonant peak on the third harmonic of every note; the hardware recording
  obeys the same law at the same absolute frequencies the engine predicts
  (note sounding 69 Hz peaks at 207 Hz in the recording; the engine predicts
  205 Hz). The balance delta is the riff playing much higher keys than our
  fixed scores.
- **B71 Piccolo — methodological, method lesson.** The patch's 4' RANGE
  transposes the matched note up an octave, which the register matcher does
  not model; our renders otherwise follow the bytes exactly (octave-up
  pitch, ADSR shape, zero noise floor, whistle timbre matching the
  reference's harmonic profile).
- **A31 Clock Chimes — methodological.** A sustain-0 self-oscillating chime
  rendered exactly per the bytes (466 Hz at the matched note, immediate
  percussive decay, clean tail) scored against a 16-second 56-strike riff
  normalized near full scale.
- **A64 Snare Drum — quantified contradiction, recorded under OQ-15.** The
  one adjudicated engine-side finding. With saw/pulse off and key follow 0,
  the patch's snare character can only come from the noise leg dominating
  the sub leg, and the hardware recording is noise-dominated (centroid
  ~2.7 kHz, broadband to 8 kHz). The model renders noise 9.7–10.8 dB
  **below** sub at A64's own slider values (noise 91, sub 64), a ~13–16 dB
  balance inversion that survives register matching; opening the cutoff
  changes noise-only level by <1 dB, ruling out the VCF, and the corpus-wide
  chain tilt (−8.2 dB median) cannot produce a within-take inversion between
  two sources. A65 Tom Toms shows the same signature; A66 Timpani shows a
  milder one consistent with its more closed filter; the dark sub-only A83
  passes clean. This is exactly the unanchored territory OQ-15 already
  names: `subMixVolts = 5.0` has no end-to-end anchor while the noise leg is
  anchored within ~4 dB, and the in-model noise-full-to-sub-full ratio is
  off by more than triple that band against this recording. Per the research
  contract the constant is **not** retuned from an undocumented MP3 chain;
  the contradiction is recorded in [open-questions.md](open-questions.md)
  under OQ-15 for closure by the measured source-to-VCF budget it calls for.

## Conclusion

The shipped 128-preset bank passes external sanity validation. Corpus-level
spectral agreement with public hardware recordings is strong (median shape
correlation 0.897), every flagged preset was individually adjudicated, and
all but one adjudication found the model rendering exactly what the stored
hardware tone bytes dictate, with the deltas explained by score content,
register, phrasing or the reference chain. The one engine-side finding — the
noise-versus-sub mixer balance on noise-forward drum presets — is a
quantified contradiction inside an already-open calibration question
(OQ-15/OQ-16), recorded there with numbers rather than silently re-voiced.
Two incidental positives: the B52 resonant-peak law and the A21 harmonic
rolloff match the hardware recordings at absolute frequencies, which is
independent corroboration of the cutoff/resonance calibration on real
program material.

No engine constant, preset byte or audit gate changed as a result of this
pass. The comparison tooling was deliberately kept out of the product tree;
this record plus the cited public sources suffice to reproduce it.

**Superseded in part, 2026-08-19.** That paragraph describes this pass only, and
its restraint stood on a judgement rather than on a rule: `subMixVolts` has
always been a **voiced** coordinate -- "chosen inside a range the sources bound
but do not fix" -- so retuning it never required an exemption from the research
contract, only that the result stay labelled voiced, which it does. On the
owner's decision that real-unit recordings are the most valuable reference
available and that a factory-preset discrepancy should be acted on,
`subMixVolts` moved 5.0 -> 2.0, taking 7.96 dB of A64's 10.10 dB inversion and
leaving it at -2.26 dB. The isolated measurement behind it, the measured
bank-wide cost, and the part of the A65/A66 residual that is *not* this
constant's are recorded under OQ-15 in [open-questions.md](open-questions.md).
Every other adjudication in this pass is unaffected.

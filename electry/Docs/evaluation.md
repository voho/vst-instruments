# Electry evaluation contract

This is the measurement side of Electry's realism work. It keeps three things
separate: a deterministic model render, recordings made by real players, and
the listening claim that one is convincing as the other. A better numeric fit
to one phrase is useful evidence; it is not a market-leading claim by itself.

## Current product surface

The unreleased plug-in currently exposes 26 host parameters; development
snapshots have no backward-compatibility contract. One **Guitar Build**
parameter replaces six separate construction axes and follows a smooth path
through Slab fixed, Contoured, Angular set, Modern bolt, Dense extended and
Neck-through anchors. Internally that path co-moves wood damping, body mass,
modal shape, joint/bridge construction, scale and Drop-E gauge; the fitted
Drop-E build at 0.8 is the default. Pickup selector and type, Tone, Body
Resonance amount, string age and all player/contact controls remain independent.
**Output Mode** is one three-choice Mono/Stereo/Double parameter; Double means
two separately seeded complete engines, one mono performance per channel.

The editor names the bridge-hand style **Mute** and its two controls **Mute
Tightness** and **Mute Pressure**. This document retains “palm mute” where it
names the physical technique, public-corpus annotation or frozen evaluator
filename.

The wet path is outside the dry probes below. Its Distortion module now solves
the 2.2 kOhm / 10 nF antiparallel Shockley-diode RC node from
[Yeh, Abel and Smith](https://dafx.de/paper-archive/2007/Papers/p197.pdf), and
its two amplifier stages interpolate a dense transfer generated during
preparation by solving
[Dempwolf and Zölzer's measured 12AX7 current model](https://dafx.de/paper-archive/2011/Papers/76_e.pdf)
against a 250 V / 100 kOhm plate load with a residual-checked solver. Existing
oversampling, sag, transformer-flux model and filter cabinet remain. These are
circuit solves of the nonlinear modules, not a complete named pedal or amp
schematic, a SPICE validation, or a measured cabinet impulse response.

## Reproducible model probes

Build the JUCE-free tools, then render the evaluation set outside the source
tree:

```bash
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
./build-dsp/ElectryRenderEvaluation build-dsp/evaluation
```

Schema `electry-evaluation/v3` writes ten probes plus `manifest.json`:
`e1-open.wav`, `e1-palm-mute-light.wav`, `e1-palm-mute-medium.wav`,
`e1-palm-mute-hard.wav`, `e1-dead.wav`, and the same five names beginning with
`e2-`. Light, medium and hard are Mute style renders at Mute Tightness
0.00, the 0.55 shipping default, and 1.00; Dead is the independent fretting-hand
mute with continuous Mute Pressure at zero. Every probe records its own value
in the manifest.
All WAVs are unnormalized, mono, 32-bit IEEE float at 44.1 kHz. The targets are
the open eighth string E1/MIDI 28 and open sixth string E2/MIDI 40, each struck
at velocity 0.95 after a 250 ms lead-in, held for two seconds, and released for
one. The signal is the dry `ElectryEngine`; the FX engine is not instantiated.
Sympathetic ring is disabled to isolate the played string, and every other
engine parameter is written to the manifest. FX parameters are out of scope
because that engine is not instantiated.

Amplitude is arbitrary digital model full-scale, not pickup voltage and not a
level-matched real DI. Bit determinism is promised only for repeated runs of
the same renderer executable on the same CPU architecture; build modes and
floating-point implementations need not hash identically. CTest parses the
manifest and checks every WAV's header and payload size rather than trusting
the writer's exit code.

## Real eight-string reference ladder

The first lawful reference is Freesound 557299 by `minus_28_and_falling`, a
[CC0 Drop-E eight-string phrase](https://freesound.org/people/minus_28_and_falling/sounds/557299/)
containing two open attacks, two attacks the creator labels “muted string,” and
four picked attacks across two ghost-note passages. The source does not name
the hand or explicitly say palm mute, so the muted attacks are Palm trajectory
candidates rather than technique-labelled calibration takes. The source page
identifies the original as a 44.1 kHz, 16-bit mono WAV. The original download
requires a Freesound login, so this pass analysed the public high-quality MP3
preview decoded to WAV. Neither file is committed.

The two repeated open/mute/ghost sequences were first bounded manually, then
each analysed note was aligned at the first crossing of 25% of its own 2 ms RMS
peak. Those signal onsets are 0.00175 and 3.36562 seconds for the open attacks,
and 1.19789 and 4.63349 seconds for the muted attacks. The unknown pick force, mute
pressure, pickup/DI chain, and preview codec make this a discovery and
trajectory reference—not an absolute spectral or level calibration master.
The first open onset is only 1.75 ms from the file boundary, so its precursor
and pre-onset baseline may be truncated; it remains useful for the later pitch
trajectory but is not clean evidence for open-attack timing.

The comparison uses the same analysis on both sides:

- 180 ms Hann windows beginning 30, 100, 220, 420, and 700 ms after onset;
- local peak tracking for partials 1-8, bounded below 45% of the spacing to the
  neighbouring partial so one harmonic cannot be mistaken for another;
- every partial expressed in cents relative to its own 700 ms position, then
  the median across partials and the two takes;
- RMS windows at 0-50, 50-150, 150-500, and 500-1000 ms, relative to the first;
- the first 25%-of-own-peak crossing of that 2 ms RMS envelope for alignment
  and onset-to-peak time.

This avoids the previous measurement bug: scoring only equal-temperament FFT
bins made a physically moving partial look like missing pickup or harmonic
energy.

Three further lawful sources broaden the comparison without pretending that
unidentified signal chains are interchangeable:

- [Freesound 557293](https://freesound.org/people/minus_28_and_falling/sounds/557293/)
  is a separate distorted performance of the same genuine Drop-E eight-string
  open/muted/ghost sequence as 557299, not a paired reamp of its clean take.
  Its original is a 48 kHz, 16-bit mono WAV under CC0; this pass used the
  public high-quality preview. It is a hard-style listening and repetition
  reference, not a dry spectral target, because both the performance and the
  nonlinear chain change envelope and spectrum.
- [cabled_mess's CC0 F#-string pack](https://freesound.org/people/cabled_mess/packs/29585/)
  contains thirteen clean/dry chromatic one-shots from F#1 through F#2,
  recorded from an eight-string through an RME Babyface into Cubase 10.5. The
  originals are 96 kHz, 24-bit mono WAVs. They are the cleanest lawful
  extended-string open-note baseline found in this pass, but contain no mutes.
- [Aussens@iter's Funky Guitar Loops](https://ccmixter.org/files/tobias_weber/57022)
  supplies two CC BY 3.0 FLAC stems labelled “muted single notes,” played at
  105 BPM on an HB eight-string through a Hughes & Kettner Tubeman Plus. The
  notes are mid-register E3/F#3 and G#3/A3 and the author does not call them
  palm mutes, so these stems inform repeated-note variation and timing only,
  not low-string or dry-DI calibration.

The broader search found useful listening and permission leads, but no second
calibration corpus. [JST Spirit of the Machine](https://joeysturgistones.com/collections/sample-packs/products/riff-vault-spirit-of-the-machine)
has real Drop-E eight-string DI and amped technical-metal riffs, while
[Animals As Leaders' Red Miso session](https://www.nailthemix.com/animals-leaders-red-miso-low-mix)
has professional eight-string DIs. JST's product-calibration rights are not
stated, and Nail The Mix explicitly forbids using its track or pieces to make
samples or products in its [usage terms](https://urmacademy.zendesk.com/hc/en-us/articles/218766607-How-do-I-use-Nail-The-Mix-tracks-for-my-portfolio).
[Uproar RAW](https://www.chocolateaudio.com/products/uproar-raw) is a strong
F#1 eight-string sampled benchmark with standard/dead palm mutes and up to six
round robins, but its [EULA](https://www.chocolateaudio.com/eula) requires
written consent for a competitive product. Finally,
[aomartinezg/music-sheet-generator](https://github.com/aomartinezg/music-sheet-generator)
documents an Ibanez RG8 Drop-E DI corpus, but the raw samples were never
committed and the repository has no license. These are permission or paid
listening leads, not inputs to the figures below.

No reference audio is committed. License, instrument identity, register and
chain are recorded here precisely so a useful secondary reference cannot
quietly become a calibration master later.

### Clean E1 attack/body direction

The clean CC0 phrase now also has the exact model-only 0-30/30-80 ms alarm
applied to both open and muted attacks. Each window is mean-removed; its Hann
window is used only for the 4,096-point spectrum. Centroid and the denominator
use `20 <= f <= 8,000 Hz`, the upper numerator uses strict
`500 < f <= 8,000 Hz`, and harmonicity is unwindowed normalized
autocorrelation over the E1 lag range. Real rows are medians of the two attacks;
model rows are the raw E1 evaluator probes after the incidental-contact repair.

| output | 30-80 ms RMS vs 0-30 | centroid, 0-30 / 30-80 | upper share, 0-30 / 30-80 | 30-80 harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real CC0 E1 open, median | +2.8801 dB | 234.34 / 256.93 Hz | 6.3775% / 5.7216% | 0.911636 |
| real CC0 E1 “muted string,” median | +0.9272 dB | 420.76 / 309.83 Hz | 17.1698% / 11.5592% | 0.850485 |
| Electry E1 open | -1.2473 dB | 277.25 / 227.85 Hz | 5.3036% / 1.1095% | 0.987577 |
| Electry E1 Palm, medium | -4.5201 dB | 109.81 / 198.16 Hz | 0.9888% / 0.6448% | 0.984358 |

The two real muted candidates begin brighter than their open neighbours and
then lose upper power, while the current medium Palm begins much darker and
remains more periodic. That is a useful local warning, not a coefficient: the
capture chain, mute hand, pressure and two pick forces are unknown, and the
source's generic “muted” label is not a Palm annotation.

### Licensed E2 no-ship check for an ongoing noise tail

[EG-IPT](https://zenodo.org/records/15205644) is an open CC BY 4.0 dataset.
Its [primary NIME paper](https://nime.org/proceedings/2025/nime2025_14.pdf)
documents 96 kHz/24-bit mono DI through a BSS AR-133 and Midas XL48 and defines
`muted` explicitly as palm damping near the bridge. A byte-range extraction of
the smallest same-pickup, same-source, same-string/ID pair compared:

- `HB-bridge_ordinario_13-02_6s_DI.wav`;
- `HB-bridge_muted_49-02_6s_DI.wav`.

Both measured near 82.4 Hz. The paper does not document tuning or the filename's
fret mapping, so E2 is a measured inference; the takes are separate and do not
match velocity, stroke direction or pressure. After polyphase resampling to
44.1 kHz, spectral power, centroid and RMS use the frozen path above;
harmonicity alone uses the pitch-appropriate 72-96 Hz E2 lag range:

| measure | ordinary E2 | Palm-muted E2 |
| --- | ---: | ---: |
| upper share, 0-30 ms | 33.1242% | 8.5888% |
| harmonicity, 0-30 ms | 0.991810 | 0.903175 |
| 30-80 ms RMS vs 0-30 | -0.4128 dB | -1.8348 dB |
| centroid, 30-80 ms | 389.54 Hz | 244.67 Hz |
| upper share, 30-80 ms | 32.0698% | 1.3923% |
| harmonicity, 30-80 ms | 0.993139 | 0.997082 |

The Palm onset is less periodic, but by 30-80 ms its body is almost perfectly
E2-periodic and its equal-window upper-band power has fallen 11.51 dB by
60-80 ms relative to 0-20 ms. This is licensed evidence against shipping the
proposed audible 30-80 ms noise tail; it does not prove that no player ever
produces one. It supports attack-local contact followed by selective damping
and retained tonal body. One separate-take six-string E2 pair is enough to
withhold that unsupported candidate, not to fit E1 or define a shipping rail;
commissioned low-string captures remain mandatory.

### Controlled low-string F2 boundary check

The live Guitar-TECHS release adds a stricter adjacent-pitch check, but its
open-string layout has one trap: both ordinary E2 recordings begin at WAV frame
zero, so their pick precursors are truncated and an ordinary-versus-Palm E2
attack contrast is inadmissible. The first fully bounded sixth-string cell is
F2 at fret 1 in the 4-8 second slot. The archive headers are 48 kHz/24-bit PCM
(P1 mono, P2 stereo), and the seven-track MIDI files use 960 PPQN; those live
formats differ from the paper's 32-bit-float/9600-PPQN description. P2's first
low-string channels are exact dual mono, so the first channel was used before
the same 44.1 kHz polyphase conversion and frozen measurement path above.

The Fishman MIDI velocities are close within each pair—113/118 ordinary/Palm
for P1 and 23/16 for P2—but they are tracker output, not calibrated pick force.
The table therefore compares normalized trajectory, never cross-file level.
The Electry rows are the identical fresh-engine evaluator protocol moved one
semitone to MIDI 41; medium means the shipping 55% Mute Tightness.

| output | 30-80 ms RMS vs 0-30 | centroid, 0-30 / 30-80 | upper share, 0-30 / 30-80 | harmonicity, 0-30 / 30-80 |
| --- | ---: | ---: | ---: | ---: |
| P1 ordinary F2 | -0.7266 dB | 317.49 / 292.39 Hz | 7.2768% / 3.7843% | 0.894177 / 0.997540 |
| P1 Palm F2 | -1.7333 dB | 270.14 / 216.60 Hz | 9.4454% / 1.0947% | 0.941525 / 0.998806 |
| P2 ordinary F2 | +0.1341 dB | 162.33 / 160.93 Hz | 1.2405% / 1.0595% | 0.950455 / 0.997666 |
| P2 Palm F2 | -3.0940 dB | 161.88 / 119.44 Hz | 0.7239% / 0.0198% | 0.895860 / 0.994374 |
| Electry F2 open | -0.5224 dB | 431.34 / 422.09 Hz | 32.7706% / 33.3461% | 0.883764 / 0.998432 |
| Electry F2 Palm, medium | -3.3390 dB | 398.65 / 352.94 Hz | 27.5098% / 22.7619% | 0.714926 / 0.997046 |

The model's total body contraction is near the two real Palm cells and its
30-80 ms periodicity lands inside their 0.9944-0.9988 range. The unresolved
mechanism is earlier and selective. Subtracting each ordinary note's own
0-30-to-30-80 upper-share change from its Palm counterpart gives -6.52 dB for
P1 and -14.95 dB for P2, but only -0.90 dB for Electry. Conversely, Electry's
Palm onset is *less* periodic than either real Palm. This supports a short
time-varying tonal/contact mechanism, not a sustained random-noise layer or a
blanket darker excitation.

Two one-scalar A/Bs were rejected. Reducing Palm's existing attack-noise
multiplier from 1.5 to 1.0 moved E2 onset harmonicity only 0.006 and further
darkened E1. Removing Palm's 0.74 excitation-modal darkener improved F2 onset
harmonicity from 0.715 to 0.802, but weakened the paired selective contraction
from -0.90 to -0.59 dB and opened the muted body. Neither fixes the measured
vector, and combining them with an unmeasured compensating decay constant would
be curve fitting. Both shipping constants were restored; commissioned dry
E1/E2 repetitions remain the gate for a richer finite-contact state.

The exact detector and spectrum were then rerun across every fully bounded
ordinary/Palm cell. For each note, selective contraction is
`10 log10(upper share, 30-80 ms / upper share, 0-30 ms)`; the paired result
subtracts the ordinary note at the same string and fret, so a negative value
means Palm loses its above-500 Hz share faster. The conventional sixth string
is the one stable stratum: its all-fret median is -8.85 dB for P1 and -8.86 dB
for P2. Every P1 Palm body there is at least 10 dB above its pre-attack upper
noise floor; the twelve P2 cells that clear that guard still give -7.35 dB.
Moving onset by +/-1 ms leaves the sixth-string medians in -9.45..-8.31 dB for
P1 and -8.91..-8.86 dB for P2.

That agreement does not make a universal coefficient. P1's string medians run
from -13.63 dB on A2 to -0.06 dB on high E4, while P2's A2 median reverses to
+3.82 dB; player, pickup, contact and noise floor remain coupled. The fixed
500 Hz split also stops being diagnostic as an upper string's fundamental
approaches the boundary. This larger check strengthens only the mechanism
claim—low-string Palm contact creates a time-varying selective loss—not an E1
calibration, a sustained-noise tail, or one damping number for every string.

### Palm-control continuity and hand-history audit

A full eight-string control sweep found no new voicing axis to add. Mute
Tightness shortened and darkened every open string monotonically, Down and Up
Palm attacks retained their velocity ordering, zero Velocity Response remained
an exact identity across all seven play styles and eight strings, zero CC2 was
an exact bypass, and Mute Tightness did nothing outside the Mute articulation.
The failure was narrower and more serious for performance: adjacent 7-bit CC2
values could cross an infeasible low-string loss solve. On E1, CC2 97 to 98
moved 0-50 ms RMS by +4.6493 dB and peak by +3.9187 dB; on E2, 116 to 117
moved 0-30 ms RMS by +2.3284 dB and peak by +1.6873 dB. Those are audible
steps in a control presented as continuous hand pressure.

At the boundary, the zero-depth hand shelf was already infeasible but its
failed solve was ignored. The scalar loop gain then hit `0.99999`, while the
one-pole coefficient and phase-compensated E1 delay jumped from 1603.10 to
1520.22 internal samples. The correction reuses the existing bounded
`solveLoopLoss` path only for that failed hand-contact case: it preserves the
fundamental target and relaxes the unattainable upper target instead of
clamping an invalid exact fit. The calibrated no-hand path remains
byte-identical on E1 and E2.

The frozen 128-value CC2 sweep now has worst adjacent 0-50 ms RMS moves of only
0.0494 dB on E1 and 0.0534 dB on E2. Attack level, absolute 150-500 ms tail and
attack-normalized tail are strictly nonincreasing on both strings. The former
regression cells measure 0.0312 and 0.0504 dB, with a 0.5 dB ceiling. The
patched CC0-to-127 spectral tilt above-versus-below 500 Hz is -2.33 dB on E1
and -22.79 dB on E2 instead of the failed solver's physically inverted
+22.59 dB E1 endpoint and over-choked -30.60 dB E2 endpoint. E1 retains one
small +0.275 dB spectral-ratio rebound at the final CC step (the absolute upper
band rises by 0.102 dB); no fullband level or decay reversal accompanies it,
and commissioned pressure/contact captures—not
another local clamp—must decide whether that extreme needs a richer loss
topology.

The transition-state audit found a separate causal error. Palm E1 followed by
Sustain E2 correctly moved the shared bridge hand from a per-sample gain target
of `0.999682` to `1.0`; after E2 was released and retired about 384 ms later,
the old Palm voice became “latest” again and silently restored `0.999682`
without a new contact. The latest real contact clock, articulation and tie
order now live at engine scope. They reset only when no played voice remains,
so voice retirement cannot move the player's hand backward in history, while
delayed and same-sample contact ordering remains unchanged.

Neither correction claims a better fit to the sparse public recordings: one
removes a controller discontinuity and the other removes a physically
impossible state transition. The expanded commissioned protocol below now
includes a Palm-E1/Open-E2 lift-and-replant groove so train and untouched
holdout players can test that shared-hand trajectory directly.

### Incidental fret-contact correctness

The Artifacts collision path had never executed on Palm or Dead E1/E2 even at
100%. Its clearance was `0.52..0.24` normalized waveguide units, which the
engine's 0.040 m/unit calibration turns into 20.8..9.6 mm—well beyond its own
2.75 mm full-force E1 seed and the 1.75 mm eighth-string factory-action
reference in [.strandberg*'s official setup guide](https://support.strandbergguitars.com/article/55-how-do-i-set-up-my-guitar).
The window also aged for almost one silent E1 round trip before the excitation
returned, while negative below-clearance “excess” incorrectly drove saddle
rattle.

Clearance is now expressed as 2.08..0.96 mm and converted through the existing
SI calibration; its window begins only after the travelling string motion
reaches the loop output, and excess is clamped to real contact before every
downstream use. A deterministic regression requires the dedicated collision
PRNG to advance for maximum-force Palm and Dead on both E1 and E2. At the
shipping 18% Artifacts setting the E1 Palm/Dead 0-150 ms change remains subtle,
about -55.18 to -59.97 dB relative to the previous render. This repairs a
dead/inverted mechanism without pretending it fills the Palm-body gap; the
current circuit-chain alarm is frozen separately below.

### Repeated-mute envelope comparison

The two muted-string candidates in Freesound 557299 provide one lawful, if very small,
same-player Drop-E repetition comparison. Each attack is aligned at the
25%-of-own-peak crossing above. The following 120 ms of its 2 ms RMS envelope
is converted to decibels, mean-centred, and compared by Pearson correlation;
this removes absolute level and asks how closely the envelope shape repeats.

The model side is the two MIDI-identical bars in
`Docs/audio/04-drop-e-rhythm-dry.wav` (SHA-256
`6ba5bf9c90f20ae16c26edd5c2729fa1613e8090ba7b5eda94adb9bcf643e642`).
Its exact bar starts are frames 11,025 and 148,601 at 44.1 kHz—the renderer
truncates every scored hold and gap to an integer frame—and the corresponding
E1 Palm Mute score hits are 0, 1, 2, 4, 5, 7, 8, 10, 11 and 13.

| source | corresponding pairs | 120 ms envelope correlation (raw; best within +/-2 ms) | paired 0-30 ms RMS difference |
| --- | ---: | ---: | ---: |
| real CC0 Drop-E E1 muted-string candidates | 1 | 0.742; 0.789 | 2.44 dB |
| current Electry dry E1 bars | 10 | median 0.981, range 0.930-0.995; median 0.985, range 0.954-0.995 | median 0.807 dB, range 0.032-2.774 dB |

Electry already spans the real pair's short-attack level difference, so this
does not support more random gain or timing. Its later envelope repeats more
closely, which makes physical contact and repick state the next candidate.
One real pair, separated by several seconds rather than a rapid chug interval,
cannot set a shipping threshold; the commissioned capture gate below remains
the first source allowed to tune that state.

The ccMixter stems cannot enlarge this low-E repetition count. The A and B
stems are genuinely different upper-register performances (corresponding
110 ms attacks correlate only 0.085-0.174 after shift alignment), but nominal
bars inside each `SingleNotes` file correlate at 0.999999 or higher as
waveforms, with their differences 59-85 dB below the signal. Only one cycle
per stem is independent; the later cycles are repeated audio regions rather
than player round robins, and remain useful only as musical-cadence references.

### Distorted Palm/ghost output alarm

Freesound 557293 was annotated separately as a processed-output alarm. The
public HQ preview's SHA-256 is
`bd6c51cecd1cd308fa99ce7b1ad2f7d0671db105a9e31b009e2fbe0192aa4013`.
At 48 kHz its muted-string attacks begin at frames 63,768 and 223,190; the first
ghosts directly replacing them begin at 127,380 and 287,808, and their second repicks
at 143,344 and 303,615. Palm and second-ghost onsets use the existing centred
2 ms RMS threshold. Because the first ghost begins as a spectral change inside
an ongoing distorted note, its boundary is the change point of a smoothed,
zero-phase 500 Hz high-passed envelope; varying that cutoff from 400-600 Hz and
the smoother from 12-40 ms keeps both annotations within 13 ms.

The comparison below uses the safe 0-30 and 30-80 ms windows common to the
real phrase and the new 83.31 ms-IOI
[`16-mute-and-dead-metal.wav`](audio/16-mute-and-dead-metal.wav), SHA-256
`e8834165e14e2f8a1df22539f58c5d7ed3ac4d29ea2b2632a3782cd4f314707d`.
Centroid and power share are mean-removed and Hann-windowed. Centroid and the
power-share denominator use bins with `20 <= f <= 8,000 Hz`; the power-share
numerator uses the strict upper band `500 < f <= 8,000 Hz`. Harmonicity is
normalized autocorrelation over the E1 lag range.

| output/context | 30-80 ms RMS vs 0-30 | centroid 0-30 / 30-80 | 30-80 ms power, `500 < f <= 8,000` / `20 <= f <= 8,000` | harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real distorted “muted string” | +0.87 dB | 1,033 / 1,506 Hz | 73.0696% | 0.212544 |
| Electry common-chain Mute | -4.34 dB | 167 / 289 Hz | 6.5960% | 0.879782 |
| real first ghost after Palm | +3.12 dB | 652 / 310 Hz | 9.9994% | 0.786 |
| real second repick | +1.30 dB | 754 / 1,314 Hz | 64.6730% | 0.247 |
| Electry common-chain Dead | -3.17 dB | 246 / 316 Hz | 8.4881% | 0.880136 |

The muted-candidate/model-Mute upper-band-share gap is 66.4736 percentage
points on that exact predicate. It remains a confounded processed-output alarm,
not a dry tuning target.

An independent chain audit locates this gap upstream of `ElectryFx`. For the
last muted E1 of demo 04's first bar (known contact frame 103,622), the 30-100
ms dry body has a 0.0685% upper-band share under the same predicate and 0.9878
harmonicity; the identical frame in demo 05's common chain raises those to
0.2913% and 0.9837. The FX adds
upper harmonics rather than removing them. Only about 1.2%
of the real region's power lies above 5 kHz, so the cabinet's fourth-order
5 kHz roll-off cannot explain the missing 500 Hz-5 kHz content either. At the
metal voicing's 0.85 Pick Hardness, the existing direct contact-noise burst is
about 1.4 ms long and has ended before either scored body window; Palm then
attenuates the release transient and high string modes as intended. The result
is mostly periodic string motion after 30 ms.

The contextual ordering repeats in both real passes: the first ghost is dark,
then the second repick is much brighter. Electry's Dead output is close to the
first ghost's 30-80 ms centroid and upper-band share, which supports retaining
the dark base body but only raises a bright-repick hypothesis; this processed
source cannot establish a missing dry mechanism. Electry's
common-chain Palm is also substantially darker and more periodic here. On the
existing 120 ms envelope method, the two real distorted muted candidates
correlate 0.240
(0.348 after best alignment within +/-2 ms), against 0.9685 raw and 0.9762
best-aligned medians for the final common-chain model bars, so their output
trajectories repeat less alike. Paired 0-30 ms level displacement is 0.431 dB
for the real pair versus a 0.296 dB model median (0.031-1.379 dB range).

This file is not a dry calibration target. It is a separate performance, not a
paired reamp of 557299: its two ghost IOIs are 332.6 and 329.3 ms where the
clean take's are 416.0 and 407.3 ms. Pickup, hand depth, pick force and the
nonlinear chain are unknown, and only a lossy preview was available. Fitting
dry Palm or Dead loss to its centroid or autocorrelation would confound player,
instrument and amplifier. It instead requires a processed listening comparison
and, after the commissioned DI pilot, a state-aware first-ghost-versus-repick
probe.

EG-IPT closes the earlier tail experiment as a shipping candidate: do not
extend or reuse the deterministic contact-noise state as a 30-80 ms
hand/winding tail. Preserve attack-local contact and selective tonal damping.
The rapid-Palm upper-share/harmonicity test remains a processed-output direction
alarm, but its rails must not be optimized by injecting noise. Reopen a late
stochastic component only if commissioned dry E1/E2 train and holdout captures
repeatedly show nonperiodic 60-80 ms energy above their measured noise floors.

`testRapidPalmBodyDirection` keeps a model-only version of that alarm runnable
without downloading reference audio. It renders the exact twelve-hit E1 score
and common metal voicing, mean-removes each scheduled 30-80 ms body, applies
Hann only for spectral power, and computes harmonicity from unwindowed
normalized autocorrelation at E1 lags. The current in-test medians are 6.595952%
upper-body power and 0.879781 harmonicity; loose one-sided rails require more
than 6% and less than 0.97 respectively. They reject a materially darker or
more periodic regression while leaving commissioned dry evidence free to
support a different contact model.

### Fretting-hand ghost comparison

The same CC0 Drop-E recording contains four separately picked ghost attacks,
not one isolated sound: two per pass, at frames 111,585, 129,930, 264,442 and
282,404 (2.530272, 2.946259, 5.996417 and 6.403719 seconds). Each first ghost
repicks the preceding palm-muted state; the second follows it after 415.99 or
407.30 ms. The public-preview SHA-256 is
`684c853560551701764f8e3728e06b2a0b59e1079a9eb4697c24ecdfe679d19e`.

The preceding tail makes a full-band threshold ambiguous, so contact onset is
the first crossing of 25% of the local peak in a centred 2 ms RMS envelope
after a zero-phase fourth-order 500 Hz high-pass. The filter locates the pick
only; all figures below use the unfiltered audio. Moving the high-pass from
400-600 Hz moves any annotation by at most 0.46 ms, and moving the threshold
from 20-30% by at most 0.66 ms. Model windows begin at the known MIDI/contact
frame rather than re-detecting a deliberately dark render from its weak high
frequency residue.

The real sound is a pitched, dark, fast-decaying E1 thunk—not a noise click.
For each hit, RMS is relative to its own 0-30 ms window; centroid is the
mean-removed, 20-8,000 Hz Hann-windowed power centroid; harmonicity is maximum
normalized autocorrelation over 36-48 Hz in the detrended 30-250 ms body.

| measure | real four-hit median (range) | old Dead | corrected Dead |
| --- | ---: | ---: | ---: |
| 30-100 ms RMS | -3.57 dB (-10.12..+1.17) | -33.82 dB | -7.84 dB |
| 100-250 ms RMS | -12.66 dB (-20.68..-6.20) | -51.34 dB | -14.85 dB |
| 250-380 ms RMS | -20.75 dB (-29.18..-12.71) | -62.04 dB | -22.54 dB |
| centroid, 0-30 / 30-100 / 100-250 ms | 220.7 / 136.9 / 85.3 Hz | 261.8 / 157.3 / 92.2 Hz | 212.0 / 139.4 / 87.5 Hz |
| 30-250 ms harmonicity | 0.936 | 0.385 | 0.988 |

Those model columns are the medians of a timing-matched
Open -> Mute -> Dead -> Dead render with no invented note-offs. The old
30 ms fretting-hand loss had an acceptable-looking centroid only because it
had erased the pitched body: its three-window envelope error averaged 36.74 dB
and its periodicity collapsed. The corrected model reduces that envelope error
to 2.75 dB, centroid error from 22.82 to 4.44 Hz and harmonicity error from
0.551 to 0.052. Every corrected RMS window lies inside the observed four-hit
range; its slightly high periodicity remains a named one-recording limitation.

A smaller 450 ms broadband-loss candidate with the Dead noise boost removed
was rejected. Aligning at its later full-band peak made it look close, but at
the fixed contact frame its median centroids were 547/290/215 Hz: the lingering
upper modes sounded like a bright picked note rather than the recorded thunk.
The retained correction instead changes only the existing Dead path: the
fretting-hand loss target becomes 1.6 s, its upper decay fit moves from 3.6 kHz
to the eighth partial, and the already-present attack hand darkening is set to
15%. Sustain and all eight Open/Palm evaluation WAVs remain byte-identical.

Mute Pressure remains the independent bridge hand and stacks with Dead. Its
one-shot impact formerly armed only above 10%, creating a discontinuity even
though the continuous loss itself was smooth. Arming it at every positive
pressure lets its amplitude tend to zero naturally: over pressure
0/.099/.100/.101/.25/.5/1, Dead E1's 20-100 ms RMS now falls monotonically and
full pressure is 23.46 dB below zero pressure; the equal steps around 10% have
normalized attack differences 0.0000652/0.0000650, safely replacing the old
roughly 24x jump. No parameter, keyswitch or mapping changed.

The four-hit regression now recreates both complete annotated passes and
computes their medians, reproducing -7.842/-14.848/-22.545 dB in the three
relative-RMS windows. That pins the model column above instead of testing only
the first two hits against broad ranges.

That median also concealed a repeat-context miss. Relative to each hit's own
0-30 ms onset, the second Dead attack decays faster than the first in both real
passes, even though those second onsets are 2.30 and 3.82 dB louder:

| second minus first | real 30-100 / 100-250 / 250-380 ms | Electry |
| --- | ---: | ---: |
| pass 1 | -6.908 / -4.344 / -3.268 dB | +0.481 / -0.031 / -1.155 dB |
| pass 2 | -5.093 / -2.790 / -2.889 dB | +0.509 / +0.098 / -0.182 dB |

The six-value contextual RMSE is 4.559 dB. The deterministic regression now
reports that score and caps it at 4.7 dB, so a future contact-state candidate
cannot improve the aggregate median by erasing the real first-to-repick
ordering. Scaling Dead's existing loss by the per-stroke hand-contact factor
improved the score only slightly and still predicted the wrong early-window
direction; two pairs from one performance do not justify shipping that retune.
The commissioned dry train captures remain the calibration gate.

One separate shipping-path mismatch was also corrected. The default 20%
sympathetic-string system already said that a Dead fretting hand damps every
string, but only Mute actually entered its global contact calculation;
unused strings could therefore outlive the deliberately short Dead voice. A
Dead voice now maps the same calibrated 1.6 s choke through the existing
logarithmic hand-contact law (`handMute = 0.2041925`). A full contact value
would impose the law's 45 ms stop and erase coupling, so it was rejected as a
global gate. The regression drives the real Dead keyswitch path and recovers
an implied coupled-string T60 between 1.2 and 2.1 s; it rejects both no contact
and the 45 ms shortcut. The isolated evaluation renders deliberately keep
sympathetic resonance at zero, so their Palm/Dead comparison and the table
above remain unchanged.

Overlapping articulations now obey physical contact order at that shared hand.
A released Palm voice remains in charge through the ordinary gap before the
next chug, but once a Sustain or Dead physically contacts later, that contact
moves the hand. A scheduled delayed Palm and every never-contacted allocation
are ineligible during lookahead; note order breaks only a same-sample contact
tie. This prevents a roughly 356 ms historical Palm release from holding a new
open accent's bridge feed about 13.9 dB down and its feedback path about 55.5
dB down, and prevents a MIDI reservation from outranking a pick that really
arrived first.

### Repick-contact mechanism audit

A separate twin-engine regression protects the boundary before this audit's
candidate can run. It rings Palm E1 for 80 ms, stages a velocity-0.20 repick 20
ms away, and requires left and right output to remain byte-identical to an
unstaged twin for the next 5 ms, then requires divergence after contact. Before
the correction, the new MIDI event immediately replaced the old voice's loop
gain, damping, hand envelope and contact scale: its pre-contact difference
signal was only 7.312 dB below the uninterrupted twin and the staged ring rose
2.33 dB. The pending descriptor now commits velocity, style, stroke, damping
and deterministic seed only when the plectrum arrives.

The existing pick-contact loss is not the missing state mechanism. At the
default Mute it derives nominal retention `R = 0.607456` over 151 internal
contact samples, then applies `R^(1/151) = 0.996704` at the commuted loop seam.
An E1 or E2 wave occupies about 2,330 or 1,165 internal samples, so every
contact frame attenuates a different stored cell once. The estimated
whole-state change is only -0.00185/-0.00371 dB, not the nominal -4.33 dB.
Changing the palm-specific retention multiplier from 0.82 to 0.20 moved the
rapid metrics by less than 0.1 dB, confirming that this is effectively a
placeholder on the two lowest E strings.

Four stronger substitutes were rejected before any shipping edit:

| candidate | rapid-chug result | decision |
| --- | --- | --- |
| Apply the nominal retention directly to every cell crossing during contact | E1 60 ms Alternate reached 15.73 dB peak spread, 9.10 dB RMS spread and +7.05 dB drift | Rejected: abrupt partial-state damping makes some repetitions less stable |
| Smooth that direct loss with a parabolic contact window | Worst drift remained +6.37 dB | Rejected: smoothing the boundary does not fix the wrong state topology |
| Apply the existing bounded point-touch FIR once over the active loop period | Across E1/E2 at 25, 30, 35, 40, 60, 80 and 125 ms, median phrase/isolated error improved 38.0% and median worst error 40.8%; 12/14 cells improved, but E1/E2 at 25 ms worsened 0.61/0.95 dB | Rejected: it missed the predeclared 50% median-improvement gate |
| Scatter the two signed folded-ring cells with the published reciprocal memoryless junction, on re-picks only | Median absolute cell-mean error worsened from 3.164 to 3.931 dB and median worst error from 4.431 to 5.527 dB; 10/14 and 11/14 cells worsened, respectively, with +2.20 to +2.77 dB regressions at 25 ms | Rejected: correct local topology and passivity did not improve the rapid-mute result |

Published pluck interaction uses a passive, reciprocal scattering junction
between the two incident travelling waves
([Evangelista and Smith, DAFx-10](https://www.dafx.de/paper-archive/2010/DAFx10/EvangelistaSmith_DAFx10_P21.pdf));
the touch/collision formulation likewise operates on a bidirectional
waveguide ([Evangelista and Eckerholm](https://www.diva-portal.org/smash/get/diva2%3A316228/FULLTEXT01.pdf)).
The new integer-delay fixture proves that the two physical rails need not mean
two allocated arrays: a sign-folded ring plus an in-place paired-cell update is
exactly equivalent. It pins zero-contact identity, reciprocity, squared-wave
energy nonincrease and sample-exact two-rail/folded-ring impulse responses.
That proof does not extend to Electry's cubic fractional seam; its nearest-grid
production prototype was a physically meaningful approximation, not a proof
for the complete filtered loop, and it failed the measured A/B above even after
fresh attacks were made bit-identical. In the robot experiment of
[Pluta, Tokarczyk and Wiciak](https://www.mdpi.com/2076-3417/12/3/1659),
measured E2 re-excitation at 12 and 70 ms did not return to the single-pluck
spectrum even after one fundamental period. Adding a moving one-sided rigid
contact at least reproduced the observed damping, ringing and pitch glides,
although neither simulation matched the measured spectra accurately. The next
contact model therefore stays behind matched rapid E1/E2 captures rather than
being tuned from passivity alone.
Until then the conservative placeholder remains accurately named.

## Controlled open-versus-palm evidence

[Guitar-TECHS](https://guitar-techs.github.io/) provides CC BY 4.0 direct-input
recordings and per-string MIDI for professional players using different
hardware; the archived dataset and paper are on
[Zenodo](https://zenodo.org/records/14963133). This pass compared each palm
note with the ordinary single note at the same string and fret: 138 pairs for
P1 and 134 usable pairs for P2. Each four-second slot was first zero-phase
high-passed at 500 Hz with a fourth-order Butterworth filter, then aligned from
its own audio at the first 25%-of-own-peak crossing of a centred 2 ms RMS
envelope. The filter is used only to locate contact; every metric below reads
the unfiltered DI, and every level is relative to that note's own 0-50 ms
window.

| player and articulation | usable notes | onset-to-peak | 0-50 ms | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P1 ordinary | 138 | 1.75 ms | 0.00 dB | -2.30 dB | -6.33 dB | -12.18 dB |
| P1 palm mute | 138 | 1.72 ms | 0.00 dB | -9.06 dB | -28.31 dB | -46.52 dB |
| P2 ordinary | 134 | 15.14 ms | 0.00 dB | -0.24 dB | -2.15 dB | -5.30 dB |
| P2 palm mute | 134 | 2.05 ms | 0.00 dB | -4.83 dB | -25.42 dB | -34.79 dB |

The paired palm-minus-ordinary medians separate overall contraction from the
spectral signature. The split is a zero-phase fourth-order Butterworth filter
at 500 Hz:

| player and band | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: |
| P1 broadband | -6.39 dB | -21.47 dB | -32.59 dB |
| P1 below 500 Hz | -5.83 dB | -18.83 dB | -29.90 dB |
| P1 above 500 Hz | -14.20 dB | -32.44 dB | -32.22 dB |
| P2 broadband | -4.50 dB | -22.84 dB | -28.70 dB |
| P2 below 500 Hz | -4.90 dB | -19.02 dB | -26.95 dB |
| P2 above 500 Hz | -5.31 dB | -33.34 dB | -30.02 dB |

These are controlled mechanism anchors, not extended-range targets. P1 used an
Ibanez PF300 with .011-.050 flatwounds, bridge pickup and Audient ID14; P2 used
.009-.042 strings and a neck pickup. Both guitars are six-string EADGBE, so
neither contains Electry's E1 or B1. Pick force and palm pressure were not
calibrated, and P2's palm recording is about 11 dB hotter than its ordinary
one, which is why only within-note and paired-normalized shapes are used. P2's
palm file contains 134 slots although the published table implies 138, with
the open-string slots absent on A, D, G and B; its TriplePlay MIDI also has
misses, duplicates and spurious events. The authors allow up to 100 ms of
inter-modality misalignment, so this analysis aligns the audio itself. The
late above-500-Hz values reach noise/crosstalk floor and must not be read as
high-frequency recovery.

The robust conclusion is narrower and more useful than a universal attack
constant: across both players the muted spectrum loses energy much faster,
especially above 500 Hz, while the low band retains substantially more body.
Palm mutes are not generally slow attacks—P1's ordinary and muted medians are
both about 1.7 ms and P2's mute peaks earlier than its ordinary note—so a
blanket attack stretch would move the model away from the controlled corpus.

Two smaller studies support the same mechanism. Biral, d'Alessandro and
Freed's pressure-sensor study on a Gibson Les Paul
([ICMC/SMC 2014](https://www.icmc14-smc14.net/images/proceedings/PS4-B10-TowardsaDynamicModel.pdf))
found that hand distance from the bridge mattered more than applied force and
that pressure dropped sharply at each pick, with an anticipatory movement for
single-direction strokes that differed under alternate picking. It does not
support a hand loss that fades in for 40 ms after note-on. Reboursière et al.'s
six-string hexaphonic study
([NIME 2012](https://www.nime.org/proceedings/2012/nime2012_213.pdf)) used the
post-attack energy slope above 500 Hz to separate 96 palm-muted notes from
ordinary notes, because the muted high-frequency envelope fell much faster.
Its nylon strings and under-saddle piezo are not an electric-DI calibration,
but the independently observed spectral direction is relevant.

### Rejected hand-dynamics mappings

The pressure trace is evidence about the player's gesture, not a direct
measurement of acoustic loss. Three deliberately small A/Bs tested that
distinction, and none earned a DSP change:

| candidate | measured result | decision |
| --- | --- | --- |
| Reverse the current energy mapping so Biral's pressure minimum produces the weakest hand loss at the attack | The muted-decay regression failed, and E2's extra high-over-low spectral loss fell to 9.23 dB, below the 10 dB guard | Rejected: sensor force cannot be copied directly into the loop-loss coefficient |
| Hold the solved hand loss static for the whole note | Engine regressions passed, but E1's extra high-over-low loss fell from 10.04 to 7.31 dB and the real Drop-E late six-band contour RMSE worsened from 9.17 to 14.49 dB | Rejected: it improved one E2 measure but made the cross-string evidence less consistent |
| Remove the small 85 Hz bridge-hand impact | Drop-E contour RMSE changed only 5.51 to 5.50 dB over 0-5 ms and 6.62 to 6.59 dB over 0-20 ms, with the later comparison slightly worse | Inconclusive and restored: the uncontrolled reference cannot justify either direction |

These results preserve the current bounded energy-driven loss mapping. They do
not validate it as a measured pressure model; they prevent a plausible-looking
sign change, static shortcut, or transient deletion from replacing the better
audio evidence.

## 2026-08-24 result

Pitch values below are cents relative to the same take's 700 ms position.
“Before” is the same default E1/style/velocity protocol before the physical
stretch recalibration; “current” is the force-derived implementation.

| condition | 30 ms | 100 ms | 220 ms | 420 ms | 700 ms | first-420 ms pitch RMSE vs real |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| real open, median of 2 | +41.30 | +31.73 | +20.99 | +1.13 | 0.00 | — |
| Electry open, before | +0.06 | +0.07 | +0.06 | +0.02 | 0.00 | 28.03 cents |
| Electry open, current | +11.13 | +9.16 | +6.31 | +2.96 | 0.00 | 20.24 cents |
| real palm mute, median of 2 | +52.53 | +45.07 | +22.54 | +9.08 | 0.00 | — |
| Electry palm mute, before | +2.06 | +1.51 | +2.03 | +2.28 | 0.00 | 35.04 cents |
| Electry palm mute, current default | +11.08 | +8.63 | +6.39 | +2.35 | 0.00 | 28.95 cents |

The open-note trajectory error fell about 28%; the default mute trajectory
error fell from 35.04 to 28.95 cents, about 17%. Internally, before the long analysis
window smears the moving partial, the shipping heavy-set E1/E2 seeds are
approximately 30/10 cents and then decay monotonically. A soft velocity-0.25
E1 stroke is about 2.5 cents.
That string, gauge, position, articulation and velocity dependence comes from
one stretch law and one effective full-force anchor; no new control or
keyswitch was added by that recalibration, and its playable range is unchanged.
The plug-in wrapper exposes Double as the third choice of its single Output
Mode parameter; it does not alter this JUCE-free single-engine probe.

The bridge-hand contact now also distinguishes a soft stroke from a hard one.
The probe below expresses 0.5-1.0 s RMS relative to each attack's own 0-50 ms
RMS, so ordinary MIDI level does not masquerade as a decay change:

| palm-mute probe | velocity 0.20 | velocity 0.60 | velocity 1.00 | soft-to-hard spread |
| --- | ---: | ---: | ---: | ---: |
| E1 (MIDI 28) | -13.063 dB | -15.321 dB | -16.986 dB | 3.92 dB |
| E2 (MIDI 40) | -11.928 dB | -13.613 dB | -15.026 dB | 3.10 dB |

This is a conservative attack latch, not another audio follower: the existing
velocity amplitude and deterministic stroke-force draw are multiplied, clamped
to 0.32-1.25, and applied to the final positive hand-loss rate. It adds no UI or
random draw. With no hand the path remains exactly absent, Velocity Response at
zero removes MIDI velocity exactly for the same stroke, and both Mute Tightness and
CC2 pressure retain monotonic tail contraction. The open render is unchanged.
The separate palm-impact retention is calibrated at 48 kHz and rate-normalized:
its internal velocity state falls 16.744 dB in 5 ms at 44.1, 48, 96, and 192
kHz, within the regression tolerance. That implementation invariant is not a
claim that the synthesized attack timing matches the real takes. Delayed
contacts now count down on every internal sample: an all-string, all-control-
phase sweep measured the removed error at -0.170..+0.147 ms at 44.1 kHz,
-0.167..+0.135 ms at 48 kHz and -0.083..+0.073 ms at 96 kHz. The bridge-impact
filter also keeps its existing state through a repick instead of jumping to
zero, and the incident fret/rattle window begins at that same contact boundary.
These are continuity/timing corrections, not a fit to the uncontrolled take.

The raw probes expose the present envelope without level matching:

| model probe | onset-to-peak | 0-50 ms | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| E1 open | 24.76 ms | 0.00 dB | -1.26 dB | -2.00 dB | -3.41 dB |
| E1 palm mute, medium/default | 2.13 ms | 0.00 dB | -4.85 dB | -11.41 dB | -16.94 dB |
| E2 open | 12.79 ms | 0.00 dB | -1.07 dB | -2.36 dB | -4.95 dB |
| E2 palm mute, medium/default | 1.29 ms | 0.00 dB | -4.61 dB | -10.35 dB | -15.27 dB |

The real CC0 E1 open take falls 9.29 dB by the last window while Electry falls
3.41 dB. That clip's muted take falls only 3.96 dB there, against Electry's
16.94 dB, and its two muted onset-to-peak times are 90.57 and 21.59 ms (median
56.08 ms). The fourfold spread between two attacks, unknown hand depth and
preview chain already ruled out a blind retune; Guitar-TECHS now confirms that
the slow median is not a general palm-mute requirement.

The evidence-backed change is therefore local to contact physics. The hand's
solved spectral loss is present from the attack instead of growing through an
unsupported 40 ms fade, and stays full until the loop has established a real
energy peak before its existing energy-driven relaxation can begin. Mute
also keeps the sustained triangular release displacement at the Sustain value
of 1.55 instead of pre-shrinking it to 1.28: the hand damps a loaded string; it
does not prevent the pick from loading it. Relative to the prior build, E1's
high-to-low energy ratio is 3.07 dB darker in the first 5 ms. E2's palm mute is
already about 4.4 dB darker than its open probe over that window. These changes
add no parameter, keyswitch, random source or performance rule.

At the default depth, paired mute-minus-open deltas in the same four windows
are `[0, -4.08, -9.76, -13.95]` dB below 500 Hz and
`[0, -3.31, -14.85, -25.55]` dB above it on E1. On E2 they are
`[0, -3.96, -7.99, -10.56]` and `[0, -4.85, -15.57, -33.44]` dB. The E1
50-150 ms exception is kept visible: the current model reaches the robust
high-band direction after that first window, not at every pitch and window.

The existing Mute Tightness control spans a real envelope range rather than three
labels over one sound:

| probe and depth | 0-50 ms | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: | ---: |
| E1 palm light (0.00) | 0.00 dB | -3.15 dB | -8.32 dB | -14.83 dB |
| E1 palm medium/default (0.55) | 0.00 dB | -4.85 dB | -11.41 dB | -16.94 dB |
| E1 palm hard (1.00) | 0.00 dB | -7.64 dB | -14.16 dB | -19.57 dB |
| E2 palm hard (1.00) | 0.00 dB | -6.54 dB | -11.96 dB | -16.68 dB |

On E2 the palm-minus-open high-band deltas at 50-150, 150-500 and
500-1000 ms are respectively `[-2.52, -8.70, -23.24]` dB at light,
`[-4.85, -15.57, -33.44]` dB at the default and
`[-11.20, -28.73, -47.75]` dB at hard. This monotonic sweep is the physical
model's answer to sample libraries' discrete mute layers: one understandable
control remains continuous between the measured anchors.

## What the research says to prioritize

The literature points toward player/contact detail before more resonators:

1. Plucking style and attack can dominate perceived guitar identity
   ([DAFx-13](https://www.dafx.de/paper-archive/2013/papers/24.dafx2013_submission_9.pdf)),
   and controlled micro-changes in pluck trajectory measurably change the sound
   ([Vibrations in Physical Systems](https://vibsys.put.poznan.pl/article/the-effect-of-micro-changes-in-the-pluck-trajectory-on-the-sound-of-an-acoustic-guitar/)).
2. A re-pick begins by damping the already-moving string before it releases a
   new excitation
   ([Applied Sciences 12(3), 1659](https://www.mdpi.com/2076-3417/12/3/1659)).
   Palm mute and repeated chugs therefore need contact history, not only a
   shorter decay constant. The pressure and NIME studies above add that the
   contact is already spectrally active at attack and moves with picking style.
3. Measured electric-guitar neck admittance varies by more than 40 dB across
   frequency and position
   ([ISMRA 2025](https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Ayudasaka_ismra2025.pdf)).
   That supports capture-fitted dead spots and decay maps once a suitable set
   exists; it does not justify inventing more modes now.
4. Magnetic pickups are usefully represented as a linear dynamic system around
   a static nonlinearity
   ([DAFx-18](https://www.dafx.de/paper-archive/2018/papers/DAFx2018_paper_39.pdf)).
   A modern active anchor is a defensible later step, but published Fishman and
   EMG voicing points do not provide enough Q/gain data for a blind fit.

## Competitive and claim boundary

The directly verified eight-string set is dominated by sampled instruments.
Its palm-mute and repetition baseline is already deeper than a single mute
switch:

| instrument | verified palm/repetition surface |
| --- | --- |
| [Electric Storm Deluxe](https://www.native-instruments.com/products/session-guitarist-electric-storm-deluxe) | muted Melody notes and arpeggios with adjustable decay, automatic round-robin cycling and 270 performed patterns |
| [Shreddage 3.5 Hydra](https://impactsoundworks.com/product/shreddage-3-hydra/) | five palm-mute layers, three dynamics, and up to four downstroke plus four upstroke repetitions |
| [Odin III](https://solemntones.com/collections/the-nordic-line/products/odin-iii) | four palm-mute types plus its Human Error performance system |
| [Evolution Dracus](https://www.orangetreesamples.com/products/evolution-electric-guitar-dracus) | half and full palm mutes, muted chugs, three dynamics and four round robins |
| [RealEight](https://www.musiclab.com/products/realeight/features.html) | bridge/right-hand mute behavior and as many as 30 repeated-note samples |
| [Cabal 8](https://wavelet-audio.com/cabal8/) | palm mute and short palm mute with eight round robins |

Electric Storm Deluxe is sampled from a custom 30-inch Framus eight-string and
splits freely playable Melody from performed Pattern operation; its
[official manual](https://docs.native-instruments.com/pdf-guides/Electric_Storm_Deluxe/Electric_Storm_Deluxe_Manual_English.pdf)
also makes repeated pitch/velocity notes cycle recorded variants. Hydra is a
Drop-E eight-string with more than 40,000 samples, Odin exposes all eight
strings, and Dracus is based on a sampled 28-inch eight-string.
[UJAM Carbon](https://support.ujam.com/hc/en-us/articles/4406855089810-VG-CARBON-User-Guide)
is the usability reference: an eight-string with separate phrase-player and
note-by-note modes, deliberately optimized for immediate heavy and
sound-designed results rather than dry-guitar literalism.

The workflow audit pointed to one playability gap, not to more mute samples.
[Hydra's official TACT manual](https://impactsoundworks.com/manuals/Shreddage%203.5%20Hydra%20Manual.pdf)
allows articulation conditions to latch or act temporarily;
[Evolution Dracus](https://www.orangetreesamples.com/download/manual/EvolutionDracus-UsersGuide.pdf)
offers latching and non-latching keyswitch assignments; and
[RealEight](https://www.musiclab.com/assets/files/RealEight.pdf) can switch its
separate left-hand and bridge mutes by key, pedal, wheel or velocity, with key
operation toggled or held. Electry now exposes the smallest compatible answer:
one visible `LATCH | HOLD` choice for the existing play-style bank, defaulting
to the former `LATCH` behavior. The PLAY STYLE strip stores the base; in HOLD,
the newest physical, host or on-screen play-style key overrides it only while
down and reveals an older held key or the base on release. Pick Stroke remains
independent and velocity keeps its physical force meaning.

`LATCH | HOLD` is saved as non-parameter state alongside the current 26 host
parameters; transient held keys are never serialized. Because Electry is not
released, the suite pins current-state round trips rather than migration from
older development layouts. Play-style Note On, Note
Off and zero-velocity Note On are stably
conditioned before a same-sample attack in either host insertion order; CC123
joins that pass and retains its source order against them.
Duplicate and overlapping holds, base changes beneath a hold, delayed-note
style capture, UI/host parity, CC120/123, Panic, prepare/release, mode changes,
current state and hold-time state saves are regression-pinned. This closes the
workflow gap without copying discrete half/full/short layers: Mute Tightness,
Mute Pressure (CC2) and stroke force remain one continuous bridge-hand surface, and
no DSP or rendered evaluator target changes.

The adjacent specialist bar is
[Outboard PalmML](https://outboard.audio/en/help/outboard-palmml): it is a
seven-string A1-E3 instrument rather than a direct E1 eight-string peer, but its
400 real palm-muted recordings, two pickups, independent left/right performances
and five round robins make attacks and repetition—not merely decay—the relevant
comparison. Its public terms permit private or business plug-in use but defer
plug-in-use rules to the installer EULA. Archive that EULA or obtain written
permission before a competitive test; never use its renders as fitting data.
The current manual lists Windows VST3/CLAP only, so run that comparison on
Windows or recheck format availability when the study is commissioned.

Within that verified set, Electry's defensible distinction is an eight-string
whose pitch, decay, coupling, pickup response and articulations remain
continuous physical state instead of selecting a recording. That is not proof
that it already sounds more realistic than those libraries. The market claim
requires held-out, level-matched blind listening against real DI performances
and leading products, with enough trials to report uncertainty.

[GuitarFlow](https://arxiv.org/abs/2510.21872) is a useful adjacent 2025
research result rather than a shippable comparison instrument: it encodes
muted strings, bends and legato in tablature, renders a sample-based guide and
uses flow-matching style transfer, then evaluates realism with objective
metrics and a listening test. That supports Electry's technique-aware score and
frozen listening protocol. It supplies neither wound-E1 Palm measurements nor
a reason to hide this playable, low-latency physical model behind a trained
audio generator.

### Commissioned Palm/Dead capture gate

No public source found closes the licensed, controlled, genuinely dry E1 and
open-string E2 **Open/Mute/Dead** gate. The two strongest lawful CC0 leads
still cover only separate pieces:

- [Freesound 557299](https://freesound.org/people/minus_28_and_falling/sounds/557299/)
  is a real Drop-E eight-string open/muted/ghost phrase, but “muted” does not
  identify the hand, the chain and pick/pressure are uncontrolled, and this
  audit could access only the public MP3 preview.
- [cabled_mess's F#-string pack](https://freesound.org/people/cabled_mess/packs/29585/)
  documents a clean eight-string/RME Babyface/Cubase workflow and dry F#1-F#2
  open notes, but contains no Mute or Dead articulations and is not Drop-E.

The additional lawful
[CC BY 3.0 ccMixter stems](https://ccmixter.org/files/tobias_weber/57022) are
upper-register preamp performances whose “muted single notes” label does not
establish a palm technique, so they bound repetition timing only.

The [cabled_mess profile](https://freesound.org/people/cabled_mess/) explicitly
offers custom recordings, making that recordist the best first commission
lead. Availability, retuning, adherence to this protocol and commercial
model-calibration/private-evaluation rights still require a written agreement;
the public CC0 pack alone grants none of those future-performance facts. The
documented [Ibanez RG8 Drop-E corpus](https://github.com/aomartinezg/music-sheet-generator)
is not an alternative: its raw recordings were never committed and the
repository has no licence.

Uproar RAW and Hydra remain strong commercial listening references, but their
licences do not permit turning their recordings into a competing instrument.
The next fitting evidence must therefore be a commissioned capture with
explicit commercial model-calibration and private-evaluation rights. The
smallest useful pilot is a separate
`electry-mute-capture/v1` contract; the model-only `electry-evaluation/v3`
directory and validator remain unchanged.

The [GOAT dataset](https://github.com/JackJamesLoth/GOAT-Dataset) was inspected
rather than dismissed from its instrument list: its published corpus is six
strings EADGBe, and the [archive terms](https://zenodo.org/records/15690894)
restrict it to research use and explicitly exclude commercial-product use. A
listed Strandberg therefore does not turn it into either an extended-range or
lawful product-calibration source.

The 2025 [EG-IPT dataset](https://zenodo.org/records/15205644) contains 52,320
files across 19 techniques from a six-string 2005 Gibson SG Standard, including
isolated Palm-muted notes, DI and three pickup settings. Its live Zenodo dataset
record assigns CC BY 4.0 to the archive. The licensed HB-bridge DI sixth-string
pair supplies the E2 mechanism check above; it remains unsuitable for E1
fitting or an eight-string product benchmark because the pitch is measured
rather than documented, the performances are unpaired, and no velocity,
stroke-direction or pressure match is provided.

Each player/guitar session contains ten unprocessed isolated-note files: E1 and
E2, each as `open`, `palm-near`, `palm-middle`, `palm-far` and `dead`. Files are
mono 32-bit IEEE float at 44.1 kHz, with no normalization, gate, EQ,
compression or gain change. Each file has twelve fixed 3.25-second slots:
11,025 lead-in frames, 88,200 held frames and 44,100 reset frames, or 1,719,900
frames total. One separately performed hard pick occurs after each lead-in; its
stroke direction is `[down, up] x 6` in every file. The one-second reset
separates the hits, so this is a direction-matched isolated control, not
continuous alternate picking. It leaves six within-session repetitions per
direction and removes the direct down/up reference mismatch; continuous
alternate-picking gesture and history remain part of the rapid comparison. The
Palm heel landmarks are
nominally 4, 12 and 20 mm from each string's saddle witness point measured along
the string; the manifest records separate actual E1 and E2 triples and treats
those values as pilot geometry, not universal constants. All 16 files are
recorded in one per-session pre-randomized order stored with its seed/draw ID in
the manifest, so articulation, Palm depth and repetition context are not
perfectly aligned with tuning drift, pick wear or hand fatigue.

`dead` means that the fretting hand lies lightly across all strings without
pressing any string to a fret. The picking hand clears the bridge and strings
except for the pick. Each session fixes one ordinary natural hand location and
shape for all Dead takes, logs the index-pad centre's nut distance along string
8 and describes the finger/string contact span. Palm heel distance and Mute
Tightness do not describe this articulation; confusing the two hands would make
the capture unable to test the model's separate Mute and Dead controls.

Because isolated notes cannot validate re-pick state, the pilot also contains
`e1-palm-middle-rapid.wav`, `e2-palm-middle-rapid.wav`, `e1-dead-rapid.wav` and
`e2-dead-rapid.wav`. Each records separate 12-hit runs of hard alternate
sixteenths, one each at 120, 180 and 240 BPM (nominal 125, 83.33 and 62.5 ms
IOI). The per-file orders are randomized before recording, logged, and balanced
across the four rapid files so every BPM's counts in run positions 1-3 differ by
at most one. Each run has its own monitoring-only four-beat count-in, at least
two seconds of recorded no-performance audio between runs and after the last
run, at least 11,025 recorded no-performance frames before the first attack,
and the untouched DI noise floor. No count-in enters the recorded file.
A complete rapid file is therefore at least 407,007 frames long. Actual files
may be longer.
A fifth file, `dead-e1-e2-groove.wav`, records three eight-hit runs of
`[E1, E1, E1, E2, E1, E1, E2, E1]` at 180 BPM sixteenths, with the same count-in and
silence policy, for a minimum 352,800 frames. Every alternate run starts down.
Its E2 hits therefore include one upstroke and one downstroke per run instead of
confounding every string change with pick direction. The mixed-string groove tests
whether one fretting hand coherently damps the whole instrument rather than
only the currently picked string. Analysis aligns performed onsets from audio
rather than forcing them to the click grid. The 25-40 ms automated cases remain
deliberately superhuman stress tests; they are not presented as a capture
target or a playable black-metal tempo.

The sixth phrase file, `palm-open-e1-e2-groove.wav`, records three runs of
`[Palm E1, Palm E1, Palm E1, Open E2, Palm E1, Palm E1, Open E2, Palm E1]`
at 180 BPM eighth notes (166.67 ms IOI). The heel stays at the measured middle
E1 landmark for Palm hits, lifts fully clear for each Open E2 and replants
before the following E1. Down-first alternate picking puts one Open E2 on each
stroke direction per run. This is intentionally slower than the rapid files:
it exposes a playable whole-hand lift/replant and the old-string residual on
both sides of the accent instead of collapsing transition, pick direction and
chug speed into one cell. Its minimum length is 429,975 frames under the same
lead-in, inter-run silence, tail and audio-onset-alignment policy.

The guitar, bridge pickup, scale, tuning, E1/E2 gauges, pick, interface, input
impedance, gain and guitar-output cable stay fixed within a session; guitar
volume and tone remain fully open. The pickup sensing-centre distance and
resting string gap are measured separately on E1 and E2, while cable make,
length and total capacitance (measured or datasheet-estimated) preserve the
electrical load that shaped the recorded DI. The pick-string contact point is
measured separately from each string's saddle and held fixed across
articulations, so moving the Palm heel does not silently move the pluck comb.
Each Palm landmark also records its E1/E2 contact-footprint length from a setup
photo and a description of the heel's orientation across the strings; a centre
coordinate alone cannot identify a finite contact. The player uses the same
intended hard force and the prescribed down/up direction for every isolated
slot; rapid files follow their continuous alternate pattern. Pressure is
natural and explicitly uncalibrated. The study therefore indexes logged
contact geometry; it does not claim that heel position causally isolates
pressure or gesture as if the hand were a calibrated actuator.

The copy-and-fill
[`manifest.template.json`](capture/electry-mute-capture-v1/manifest.template.json)
freezes the schema and split, anonymized player/guitar/session identity, signed-rights
document hash and redistribution status, all instrument and signal-chain fields
above, pickup/gap/cable loading, pick/Palm/Dead landmark definitions, Palm
contact spans/orientation and measured millimetres, isolated and rapid
protocols, both transition grooves, the randomized 16-file order, all
filenames, frame counts
and every WAV's SHA-256. Every physical player and guitar uses a stable
anonymized ID and belongs wholly to train or holdout, even across multiple
sessions. The collection intake below rejects duplicate session IDs and any
declared player or guitar ID crossing the split; the coordinator's private
identity ledger is still required because software cannot detect one physical
source being relabeled. The pilot minimum is one train and one untouched
holdout player/guitar cluster; once inspected, that pilot holdout joins
development and cannot become a final untouched holdout. The minimum final
engineering gate uses at least three train and exactly two active holdout
clusters reported separately. Extra holdout reserves remain sealed outside the
active cohort; this is still not a population claim. The adjacent
[`README.md`](capture/electry-mute-capture-v1/README.md) is the recordist's
timing, hand and preflight checklist.

Before delivery, run the structural intake from the repository root:

```sh
cmake -DELECTRY_MUTE_CAPTURE_DIR=/absolute/session/path \
  -P cmake/ValidateMuteCapture.cmake
```

After all current deliveries pass individually, validate their declared IDs as
one collection:

```sh
cmake -DELECTRY_MUTE_CAPTURE_COLLECTION_DIR=/absolute/collection/root \
  -P cmake/ValidateMuteCaptureCollection.cmake
```

The collection command recursively reruns the per-session intake and requires
both splits. The later comparison manifest, not this mutable intake directory,
freezes the exact active cohort before analytical or listening access to any of
its members.

The per-session command walks the RIFF chunk table, so ordinary
DAW/Broadcast-WAV `bext`, `JUNK` or `LIST` chunks and the exact IEEE-float
`WAVE_FORMAT_EXTENSIBLE` subtype are
permitted while mono 44.1 kHz 32-bit float format, hashes, declared frame counts
and the 16-take contract remain exact. It cannot verify hit count, performed
tempo/order, where silence falls, the per-run monitoring count-in, clipping or
non-finite samples, the player's hand, absence of processing or legal authority;
the engineer's waveform/listening spot check and signed agreement remain the
evidence for those facts.

Analysis forms the real contrast from each mute and the same-session real Open
distribution at the same string and stroke direction. It separately forms each
model contrast from that model's mute and matching model Open render under the
same non-articulation settings. The comparison is real `(mute - Open)` versus
model `(mute - Open)`, never real mute directly against model mute. Same-numbered
slots in separately recorded files are not paired physical trials; the slot
index only fixes direction. Each domain uses its own direction-specific median
Open contour.
Within each fixed isolated slot, the primary audio-only onset candidate is
log-filtered positive spectral flux with the adjacent-frequency maximum filter
and adaptive peak selection described by
[SuperFlux](https://www.dafx.de/paper-archive/2013/papers/09.dafx2013_submission_12.pdf).
FFT, hop, filterbank, prominence, minimum spacing, search bounds, rounding and
measured timing bias are selected on pilot/train audio, written into every
result, and frozen before analytical access to the active holdout cohort.

Rapid files are first split into runs by a train-frozen no-performance/noise-floor
rule, and each run receives the BPM logged for that take and ordinal position.
Run position is retained as a blocking/QC field; the study makes no separate
cross-tempo fatigue claim from one run per tempo. SuperFlux then runs over each
complete run. A frozen monotone one-to-one
assignment maps candidates to the twelve ordinal hits under one affine tempo
grid; only then are score-cell midpoints formed. Missing or extra peaks never
shift later hit labels, and a failed or non-unique assignment rejects the
complete run. The existing 25%-of-own-peak centred 2 ms RMS crossing is
diagnostic only and never substitutes for a missing primary peak; fixed
peak-percentage attack bounds are known to be non-robust on real sounds
([Peeters et al., JASA 2011](https://www.mcgill.ca/mpcl/files/mpcl/peeters_2011_jasa.pdf)).
Detector bias is one constant derived from synthetic known-time impulses and
blinded train annotations, never by minimizing real/model error. Before holdout,
validation reports one-to-one precision, recall, signed bias and absolute error
by articulation and tempo, and freezes which QC flags retain or exclude data.
A cell without one unique primary peak stays missing. A mute-cell exclusion
removes that real mute and every paired model candidate, never one system
selectively. An Open failure invalidates the whole independently pooled
domain/string/direction Open stratum rather than an ordinally matched mute slot.
Missing and QC counts are reported by session, string, articulation, stroke,
tempo and run position.
The same detector is applied to real and model audio. These rules belong to this
commissioned gate; the earlier CC0 Dead audit retains its separately documented
known-model-contact anchor rather than being retroactively reinterpreted.

From that onset, Palm reports onset-to-peak time, normalized
50-150/150-500/500-1000 ms RMS, and tracked-harmonic power below and above
500 Hz capped at 2.6 kHz. Down and up strokes are reported separately (six each)
and as a labeled pooled twelve-slot diagnostic, never as twelve independent
players. Every real position is compared with every current model depth.
For every partial retained by a train-frozen SNR and track-completeness rule,
the analyzer also reports its onset-aligned frequency trajectory in cents, its
log-power decay vector over the same windows, and the residual share left after
the frozen harmonic reconstruction. The retained-partial rule is shared by all
systems in a cell; a model cannot improve its score by dropping a difficult
partial. A separate direction-interaction vector compares
`(mute_up - Open_up) - (mute_down - Open_down)` in the real and model domains.
That difference-of-differences prevents a pooled twelve-slot score from hiding
an articulation that reacts correctly to downstrokes and incorrectly to
upstrokes. Track loss, harmonic residual and every direction-interaction
endpoint are frozen before holdout or remain descriptive only.
The `electry-evaluation/v3` directory contains downstroke probes only; after the
pilot, the analyzer renders matching upstrokes and rapid phrases from those
frozen parameter/event settings instead of comparing a real upstroke with a
downstroke model. Adding upstroke files to that directory would require a new
evaluation schema. The analyzer must archive each derived model WAV and event
score with SHA-256, plus its own executable hash/version, so a later DSP build
cannot silently change the comparison. Mapping choices are made on train
sessions only. Before holdout, a comparison manifest freezes baseline and
candidate build hashes, analyzer/evaluator hashes, depth mapping, harmonic and
time grids, dB convention, weights and aggregation, missing-partial/cell rules,
the primary cells, the stimulus-selection seed/algorithm, replacement and
failure rules, listener-analysis code/gates, complete event-generation and crop,
level-match/export implementations, and the common-chain build, preset,
parameters, sample rate, oversampling and asset/IR hashes. For every named
scalar endpoint, it also freezes orientation, aggregation and an exact numerical
no-regression margin. Endpoint errors and gates use unrounded values: absolute
model-versus-real contrast error, `1 - r` for a correlation, or a predeclared
weighted RMSE for a vector. The margin is the larger of propagated detector/
analysis quantization and the train-only 90th-percentile repeatability change:
balanced three-versus-three halves for six isolated repetitions, complete-run
resampling for the groove, and propagated detector/analysis quantization alone
for rapid takes because this pilot has no within-session same-tempo repeat.
Between-session or between-cluster heterogeneity is reported but never inflated
into a rapid no-regression allowance. The resampling seed, formula and numeric
result are frozen before holdout; display precision, the candidate and holdout
results cannot change them. A position-dependent DSP candidate must reduce per-harmonic
mute-minus-open contour RMSE by at least 25% in each planned holdout cluster.
In that same cluster it must not worsen beyond the frozen margin any isolated
Palm onset/RMS/band endpoint, any Dead RMS/centroid/harmonicity endpoint, any
rapid correlation/displacement/drift endpoint, or any Dead- or Palm/Open-groove
transition/residual endpoint. A missing primary endpoint fails the gate. Any DSP, selection or
analysis change after holdout inspection retires the entire connected
player/guitar cluster to development and requires new disconnected holdout
clusters for a new claim.

A player/guitar cluster contains every session connected by a shared `player_id`
or `guitar_id`; it is the independent experimental unit. Sessions, slots, runs
and hits inside it are repeated measurements. With two holdout clusters, report
each cluster's effect, median and IQR separately, require the gate in both, and
claim no population interval. A future powered study resamples complete clusters
first, then sessions, then only exchangeable isolated slots within fixed
string/articulation/contact/stroke strata. Real Open slots and real mute slots
are sampled independently within direction strata; model Open references are
likewise not ordinally paired to model mutes. Each real mute and every model
candidate driven by its event remain together, and each replicate rebuilds the
two domain-specific `(mute - Open)` contrasts. Every 12-hit rapid phrase and
every complete eight-hit groove remains indivisible.
Hit-level resampling never supports player/guitar generalization, because
treating nested observations as independent inflates false positives
([Saravanan et al., 2020](https://pmc.ncbi.nlm.nih.gov/articles/PMC7906290/)).

Dead is scored separately against its same-session, same-direction Open
distribution: relative RMS in
0-30, 30-100, 100-250 and 250-380 ms, mean-removed 20-8,000 Hz power centroid
in the first three windows, and normalized-autocorrelation harmonicity over
30-250 ms using 36-48 Hz for E1 and 72-96 Hz for E2. Train sessions may fit the
existing Dead contact; holdout clusters decide whether its envelope, residual
pitch, per-partial decay and non-harmonic residual generalize. The per-endpoint gates above make the
separation operational: Palm improvement cannot compensate for a Dead or rapid
regression, or vice versa.

The rapid files additionally compare each phrase hit with the isolated-hit
distribution at the same string, contact and stroke direction, reporting
envelope-shape correlation, first-30-ms RMS displacement and hit-to-hit drift.
Because pressure and gesture are unmeasured, these rapid-minus-isolated deltas
are composite context endpoints—not causally identified repick-state effects.
The direct real/model phrase comparison remains the acceptance result.
The model is driven by the detected performed onset intervals. Its only global
phrase shift is the median signed primary-onset residual across complete
real/model hit pairs, under train-frozen bounds and rounding; it is reported as
latency and applied once to every phrase window. There is no per-hit waveform
alignment, dynamic time warping or separate frequency-band alignment. The
mixed Dead groove also reports inter-hit residual energy and its change after
each string transition, stratified by stroke direction. The Palm/Open groove
compares each Palm E1 and Open E2 with the matching isolated same-session
articulation/direction distribution, then reports the 40 ms pre-hit residual
and 0-50 ms attack/band vector at both the heel-lift Open E2 and the first
heel-replanted Palm E1. The model follows the detected performed intervals and
complete declared hand-state sequence; there is no per-hit warp. That makes a
historical Palm state returning after an Open accent, or a Palm state failing
to return before the next chug, a named transition error rather than an
unscored listening impression. Training data may choose contact parameters;
each untouched holdout cluster decides separately whether the contact model
generalizes.

No spectral/onset capture analyzer is added before the first recording exists:
the pilot will lock the detector settings above and show whether the search
bounds survive the far mute. After it does, the intended narrow interface is:

```text
ElectryAnalyzeMuteCaptures \
  --model build-dsp/evaluation \
  --capture /captures/mute-di-v1/train/p01 \
  --output build-dsp/mute-p01.tsv
```

#### Frozen Palm/Dead blind-listening gate

The minimum real-versus-model listening pilot has ten scored A/B pairs and
three hidden repeats. Two unscored practice pairs come from train sessions; all
scored real clips come from untouched holdout player/guitar clusters.

| cells | source-matched content | scored length |
| --- | --- | ---: |
| 1-2 | E1 and E2 Palm single, dry | 700 ms each |
| 3-4 | E1 and E2 Dead single, dry | 430 ms each |
| 5 | 12-hit E1 Palm run, 83.33 ms IOI, dry | 1.22 s |
| 6 | the same Palm run through one common metal chain | 1.22 s |
| 7 | eight-hit Dead `[E1, E1, E1, E2, E1, E1, E2, E1]` groove, 83.33 ms IOI, dry | 1.01 s |
| 8 | the same Dead groove through the common chain | 1.01 s |
| 9 | eight-hit Palm-E1/Open-E2 lift/replant groove, 166.67 ms IOI, dry | 1.52 s |
| 10 | the same Palm/Open groove through the common chain | 1.52 s |
| 11-13 | hidden repeats of cells 5, 7 and 9 with A/B sides reversed | unchanged |

For every selected single, the frozen primary detector locates the real and
model onsets independently. Each clip begins exactly 2,205 frames (50 ms)
before its own detected onset and lasts 30,870 frames for Palm or 18,963 frames
for Dead. No waveform is shifted after that crop: equal pre-roll removes an
irrelevant capture-grid cue while onset-to-peak and the complete attack remain
different and audible. Insufficient valid pre-roll is a missing/failing cell.
The Palm layer is chosen using train captures only. Rapid model phrases use the
evaluator's parameters and the holdout performance's locked score; the audition
demos are not acceptance stimuli because their parameter states differ.

The comparison manifest freezes the source cohort, stimulus-selection and
presentation seeds, selection/replacement algorithm and listener analysis
before analytical or listening access to that active holdout cohort. Every
active holdout player/guitar cluster enters the study in a balanced rotation
(cell counts may differ by at most one). The
stimulus seed selects isolated slots without replacement inside separate
six-down and six-up pools, balancing stroke direction within each single-note
cell before either pool reshuffles. It selects Dead-groove runs 1-3 the same way
and selects the source run whose performed onset score the model follows. It
selects Palm/Open groove runs by the same frozen rotation. The single-note model
uses the selected slot's stroke direction. After active-cohort
access, a structurally invalid, recording-failed or otherwise unusable selected
cell is missing and fails the minimum gate; it is never substituted. A recapture
may enter only as a new disconnected player/guitar cluster under a new
comparison manifest; reusing either physical player or guitar remains part of
the retired cluster.

Palm pairs are level-matched by 0-50 ms onset RMS and Dead by 0-30 ms. Phrase
pairs use the median corresponding per-hit RMS and one scalar for the entire
phrase. Only the louder member is attenuated; a pair needing more than 6 dB is
a frozen missing/failing cell, never a reason to re-record or substitute;
retained pairs must be within 0.1 dB. One common final gain keeps both below
-3 dBTP. For processed cells, match real and model DI drive first, run both
through the comparison manifest's exact hashed chain, then match their outputs.
Distortion 0.45, Amp 0.95 and Compressor 0.60 are the current train-only starting point,
not values that may change after holdout access. Export metadata-free mono
24-bit PCM with the frozen renderer/exporter. Do not gate, EQ, denoise or
normalize individual hits.

Opaque filenames and a private key manifest hide provenance. For participant
`n` and cell `c`, `(n + c) mod 2` balances the real/model side; the manifest's
frozen presentation seed shuffles cell order, and hidden repeats stay at least
three trials from their originals. Each trial asks which clip is the physical
guitar (forced A/B plus confidence), which is more convincing in a
black/progressive-metal production (A/B/Tie), and optionally which defect gave
it away: attack, pitched body, decay, repetition, noise or other. Permit at most
three replays and record the count. Exclusions are predeclared playback/
headphone-screen failures, not post-hoc disagreement with the expected answer.

Recruit exactly 30 valid listeners: 15 active extended-range guitarists and 15
metal producers/engineers, with dual-qualified people assigned before listening
to the least-filled stratum. Continue recruitment only to replace predeclared
qualification, playback-screen or incomplete-session exclusions until those
exact valid counts are reached. Outcomes remain sealed and are never consulted
for stopping. The frozen source cohort contains exactly two active untouched
holdout player/guitar clusters. The comparison manifest locks its exact cohort,
the qualification questions, recruitment/stopping rule and these gates before
analytical or listening access begins:

- The listener-clustered 90% bootstrap interval for physical-source
  identification lies wholly within 40-60% overall; Palm and Dead point
  estimates each lie within 35-65%. Every non-repeat core cell must lie within
  30-70%, and the priority rapid-Palm cells 5-6 and Palm/Open-transition cells
  9-10 must each lie within 35-65%.
- Counting a preference tie as 0.5, Electry's one-sided 95% lower bound is at
  least 40% overall and each articulation's point estimate is at least 40%.
  No core cell may select Electry below 30%.
- Hidden-repeat same-source agreement is at least 70%, and aggregate A-side
  choices remain within 35-65%.

Every listener bootstrap draw resamples whole people with replacement separately
inside the 15-guitarist and 15-producer strata, retains each person's repeated
trials, then combines the two stratum estimates with fixed 50/50 weight. Those
intervals generalize to that equal listener mixture for this frozen stimulus
set; with two source clusters they do not establish a population claim over
guitars or players.

Real-versus-Electry parity does not establish a market win. A second,
separately randomized session must compare the same Palm and closest available
Dead phrase cells against licensed current leaders. The minimum predeclared
set is Electric Storm Deluxe, Shreddage 3.5 Hydra and Uproar RAW, subject to
each licence or written permission allowing private competitive evaluation.
Use the same MIDI score, a train-chosen articulation/depth, identical common
processing and the phrase-level matching rule above. The production-preference
gate applies product by product. Passing it supports a top-tier parity claim;
calling Electry "best sounding" additionally requires a predeclared
superiority analysis rather than relabelling a non-inferiority result.
If its installer EULA or written permission allows the test, add Outboard
PalmML to the E2 Palm cells as a palm-specialist comparator. It has no E1 or
declared fretting-hand Dead surface, so it cannot replace any core leader or
fill those cells; its output must remain evaluation-only.

Once the pilot is valid, the production capture expands to at least five mute
depths, three velocities, down/up/alternate picking, six to eight repetitions
per string/fret, and musical black/progressive-metal Palm and Dead phrases.
Players and phrases held out from fitting feed the final level-matched blind
listening test. Until then, changes must improve a named physical mechanism,
preserve the compact performance model, and beat the existing
regression/evaluation evidence rather than merely sound different.

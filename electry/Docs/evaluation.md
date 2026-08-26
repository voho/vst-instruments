# Electry evaluation contract

This is the measurement side of Electry's realism work. It keeps three things
separate: a deterministic model render, recordings made by real players, and
the listening claim that one is convincing as the other. A better numeric fit
to one phrase is useful evidence; it is not a market-leading claim by itself.

## Current product surface

The unreleased plug-in currently exposes 27 host parameters; development
snapshots have no backward-compatibility contract. One **Guitar Build**
parameter replaces six separate construction axes and follows a smooth path
through Slab fixed, Contoured, Angular set, Modern bolt, Dense extended and
Neck-through anchors. Internally that path co-moves wood damping, body mass,
modal shape, joint/bridge construction, scale and Drop-E gauge; the fitted
Drop-E build at 0.8 is the default. Pickup selector and type, Tone, Body
Resonance amount, string age and all player/contact controls remain independent.
**Output Mode** is one three-choice Mono/Stereo/Double parameter; Double means
two separately seeded complete engines, one mono performance per channel. The
second player's picked wrist strokes also carry a deterministic 0-6 ms causal
timing offset; the primary player and fretting-hand articulations keep their
established clocks.

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

### Double-performance timing audit

[HiMMP's FAQ and download index](https://himmp.net/faq.html) publish four raw,
independently performed 44.1 kHz, 24-bit mono rhythm DIs for “In Solitude”
under CC BY. The frozen exploratory audit used Python 3.11.0, NumPy 1.26.4 and
SciPy 1.14.1. SciPy decoded each PCM24 file left-justified in `int32`; samples
were converted to `float64` by dividing by `2^31`. The four downloaded files
were `Rhythm 1 DI.wav` through `Rhythm 4 DI.wav`, with these SHA-256 digests in
order:

```text
149af4b6419ec4f674457a7f7ac17bfd2e5e06b4feb77fe7fe7f8c31e936b371
a51a73af7b74680e0ecec05fcf47aec1ae8b25fe8d79171ae659f7f52bc8f540
aee3d4df31a86057088abe71b2e125c3d510b1d6b9d8121f1b21f00ae592bcc2
5ec51e22b8f709de9ea6fba9dd3d1810375bd6d8ae11848ba10db603e1246842
```

The detector pre-emphasized each signal as `y[n] = x[n] - 0.97 x[n-1]`, then
formed non-overlapping 44-sample frames. Its level was
`L = 20 log10(max(RMS, 1e-12))`; the first six novelty frames were zero and
the rest were `L[k] - L[k-6]`, smoothed by `[0.25, 0.5, 0.25]`. For each take,
the threshold was NumPy's default-linear 93rd percentile of novelty where
`L > -70 dBFS`.
SciPy `find_peaks` used that height and an 18-frame minimum separation, after
which the same level gate was applied at the peak. This found 4,173, 4,035,
3,983 and 4,035 candidates in Rhythm 1-4. Each Rhythm-1 candidate was matched
to the absolute-nearest candidate in every other take within 20 frames, with
each match required to choose that same Rhythm-1 event on the reverse lookup.
The 1,844 accepted four-take rows produced:

| take pair | median absolute difference | 90th percentile |
| --- | ---: | ---: |
| Rhythm 1 / 2 | 5.986 ms | 12.671 ms |
| Rhythm 1 / 3 | 5.986 ms | 11.973 ms |
| Rhythm 1 / 4 | 6.984 ms | 12.971 ms |
| Rhythm 2 / 3 | 6.984 ms | 16.961 ms |
| Rhythm 2 / 4 | 7.982 ms | 17.959 ms |
| Rhythm 3 / 4 | 6.984 ms | 17.959 ms |

These are energy-rise candidates on a 0.998 ms grid, not hand-labelled pick
contacts. The heuristic can select edits, noise and chord rises or miss weak
contacts; its Rhythm-1 reference and recording alignment can also bias the
result. The exact receipt replaces an earlier exploratory count whose silence
gate was not preserved.

This conventional six-string Drop-C corpus establishes that separately played
metal takes are not sample-locked; it is not an eight-string timing fit. Double
therefore uses only the tight edge of the evidence: its seeded second engine
draws one bounded 0-6 ms offset per picked wrist stroke from a three-uniform
distribution with 3 ms mean and 1 ms standard deviation. Every string crossed
by a chord shares that player offset, which composes with—not replaces—the
existing accelerating strum travel. Fresh Hammer contacts and all default-seed
Mono/Stereo scheduling are unchanged. The offset lives at the engine's physical
contact boundary, so a released pre-contact note cancels cleanly and a chord
split across host blocks retains one absolute onset.

A processor-side MIDI queue was rejected because it would duplicate note
ownership, sustain, keyswitch, repick and panic behavior. A static audio delay
was rejected because that would be a Haas copy, not a second performance. No
new knob was added: players still choose only Mono, Stereo or Double. Licensed
paired eight-string takes remain part of the commissioned listening gate below.

No public source found combines exact eight-string provenance, raw DI,
independently performed matched rhythms and explicit commercial
model-calibration rights. [Ueberschall Metal Riffs](https://www.ueberschall.com/en/product/283/Metal-RiffsDOWNLOAD)
is the closest lead: it contains 7/8-string B/F# material, matching DI versions
and three performances per riff, but does not identify which groups use eight
strings. Its [conclusive licensed-use list](https://www.ueberschall.com/en/faq)
covers music production, not simulator calibration, so both permission and the
eight-string mapping must be obtained in writing. The CC-BY ccMixter A/B stems
already audited below are preamped and not identified as matched takes; the two
CC0 eight-string sources contain only one phrase or isolated notes. They remain
useful sanity checks, not a timing distribution.

### Fretting-vibrato calibration audit

The [Guitar-TECHS project site](https://guitar-techs.github.io/) and its
[Zenodo release](https://zenodo.org/records/14963133) provide CC BY 4.0 raw-DI
technique recordings, score/MIDI alignment and professional-player metadata.
They are a useful lawful mechanism check, but not an eight-string calibration:
P1 used an Ibanez PF300 with `.011-.050` strings and its bridge pickup; P2 used
an EVH Wolfgang with `.009-.042` strings and its neck pickup. The downloaded
technique archives were not committed. Their SHA-256 receipts are:

```text
P1_techniques.zip  1e4b80a464182d345e129f3e1158b6c05690c60b5f9be4bde3fb26f23263236e
P2_techniques.zip  05fc065c010add9e5348095d7198fdc45b967c657e3e12ef8afdb74808371816
```

The P1 vibrato DI is 44.1 kHz, 24-bit stereo exact dual mono and 527.966667 s;
its six open-string groups times frets 1-22 give 132 four-second slots. The P2
DI is 48 kHz, 24-bit stereo exact dual mono and 440 s. The downloaded P2
vibrato DI/MIDI contains five groups, or 110 slots; no D-string group was
present. Both MIDI files are format 1 at 960 PPQN.

This was an exploratory phase-tracking audit, not a frozen fit. Python 3.11.0,
NumPy 1.26.4 and SciPy 1.14.1 analysed 0.25-2.60 s of each slot. For harmonics
1-12 below 7 kHz, the detector selected the strongest local line between
0.96 and 1.08 times its expected frequency, applied a fourth-order Butterworth
band-pass spanning two semitones either side, divided the Hilbert
instantaneous frequency by the harmonic number, and low-passed the trace at
20 Hz. Samples below the trace's 20th-percentile envelope or farther than
250 cents from the target were removed and interpolated. A below-1.5 Hz trend
was removed; the vibrato rate was the strongest Welch component from 3-9 Hz,
and excursion was the detrended 2.5-to-97.5-percentile cents span. The
exploratory reliability gate required a 3-9 Hz peak-coherence score of at least
0.25 and a 5-100 cent excursion.

P1 yielded 130 usable pitch traces and 114 reliable cells. Their vibrato-rate
median was 3.822 Hz (P10 3.822, P90 4.672, range 3.398-5.096 Hz); their
excursion median was 24.81 cents (P10 14.19, P90 39.82, range 7.28-72.19
cents). P2 did not replicate that result: only one of its 110 cells passed the
same gate. Its raw candidates were much shallower and had median peak
coherence 0.066, so treating their apparent 3.83 Hz rate or 5.18-cent depth as
a player fit would turn detector uncertainty into a parameter.

An independent published experiment broadens the player check without adding
an eight-string claim. Magalhães et al.'s UFMG
[master's thesis](https://musica.ufmg.br/lapis/wp-content/uploads/sites/9/2019/02/Tairone-Magalhaes-M-2015.pdf)
recorded eight guitarists playing two vibrato/bending excerpts four times each,
all through the same `.010`-strung Fender Stratocaster Deluxe and active DI;
the clean guitar was captured while the players monitored an overdriven amp.
The reviewed PDF's SHA-256 is
`dd04335e0f76394a79a771e528fc452f53eb8fb557e59f16e7aeda388362558e`.
Figure 28's four observations per player on the long final note place mean
rates mostly around 4-6 Hz, with one player around 7-8 Hz, and visibly separate
mean widths. The authors' text reports one growing performance moving from
about 20 to 140 cents and another staying around 60-80 cents. This is a
published multi-player range check, not an imported corpus fit: the underlying
64 recordings are not distributed with an explicit product-calibration grant,
and the instrument is a conventional six-string.

Electry's settled full gesture is nominally 6.4 Hz with a default 40-cent
excursion before bounded per-cycle variation; velocity 64 settles near 5.6 Hz
and 20 cents. P1 points toward a slower/narrower typical hand, while the
eight-player study shows that the existing upper gesture is still inside a
real expressive range. P2 and the absence of exact extended-range material
prevent a universal fit, so no coefficient changed.

The playability surface is now explicit: A#0/MIDI 22 is a visible momentary
**VIB** key whose Note On velocity sets amount and whose balanced Note Off
eases the hand back to rest. It is attack-conditioning state, so an A#0 event
cannot split a simultaneous chord solve; both Double players receive it even
while the second is dormant. All Sound Off and Reset All Controllers preserve
a physically held note, while All Notes Off, Panic, prepare and release clear
it. Channel and polyphonic pressure remain discarded. Exact eight-string lead
captures are still required before rate, depth and onset can be called frozen.

### CC0 metal-cabinet audit

[Jester Dyne's Brutal IR Pack](https://www.jester-dyne-productions.com/brutal-ir-pack/)
supplies a lawful real 4x12 reference: its bundled handbook dedicates the pack
under [CC0](https://creativecommons.org/publicdomain/zero/1.0/). The audited
anchor is the 48 kHz, 24-bit mono `14_Cathode_Ray_Fleshburn.wav`, an SM57/V30
capture. Its SHA-256 is
`420280d44a6cb969d0599aa88f7bc733e13d39cdd051acf8b0eda1d82286ba5f`;
the source ZIP's is
`299dc053f01ebd1e980459adc48f9c6b8a8c7af91917b4f946512eefdbb311ea`.
Neither file is committed.

The current six-section cabinet and the IR were normalized over an equal-log
70 Hz-8 kHz grid and compared after identical log-frequency smoothing. Their
broad-magnitude RMSE is 4.40 dB. The largest useful directions are less output
below the box and a shallower 430-470 Hz cut; the existing upper roll-off is
already close. Fitting the same six sections to magnitude alone reduced that
RMSE to 1.59 dB, but the candidate was rejected in the complete instrument:
the rapid-Palm 30-80 ms upper-body share fell from 7.2913% to 3.0834%, the
cabinet lost the integrated chain's low-mid-thump and presence rails, and the
muted amp-plus-compressor level rose to +12.69 dB. A closer isolated magnitude
curve is therefore not evidence of a better amplifier.

The measured IR reaches its first peak at sample 7 (0.146 ms), contains 99% of
its energy by 12.3 ms, and an onset-aligned 1,024-sample trim retains 99.636%
of its energy with 0.495 dB smoothed response error against the full file over
80 Hz-8 kHz. That makes one fixed, phase-preserved cabinet a defensible future
upgrade without another user control.

The CC-BY-4.0 [EG-IPT archive](https://zenodo.org/records/15205644) is the
strongest lawful held-out downstream check found for that candidate. Its
[primary paper](https://nime.org/proceedings/2025/nime2025_14.pdf) documents
52,320 simultaneous 96 kHz/24-bit electric-guitar files and paired direct/mic
paths through an EVH 5150 III 50W 6L6, Mesa 4x12 V30 and five microphone
sources. Same-performance `_DI.wav`/`_dyn.wav` pairs can compare complete-chain
attack envelopes and spectral trajectories after fixed level matching. They
must not be deconvolved into another IR: the amplifier is nonlinear, its gain
settings are not fully identified for that use, and the released guitar is a
six-string whose range cannot validate Electry's E1 string mechanics.

A direct stereo FIR would cost about
98.3 million multiply-accumulates per second at 48 kHz, while a one-block
1,024-sample convolver would add an unacceptable 21.3 ms. The production path
therefore requires zero-added-latency partitioned convolution, prepare-time
resampling, identical pre-cab level-matched renders and listener verification.
Until then the shipping zero-latency filters stay unchanged.

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
- [8ridgelite](https://github.com/JamesStubbsEng/8ridgelite), frozen here at
  commit `e69ebe0eb2752243de4678fe84df298555730c94`, is a GPL-3.0 eight-string
  sampler repository containing 61 uncorrected `Natural` and 61
  pitch-corrected `Tuned` stereo WAVs. The source maps the files chromatically
  from MIDI E1 upward. It is the strongest openly downloadable exact-eight
  open-note lead found, but the author does not document guitar, tuning,
  string/fret, pickups, recording chain or a raw-DI guarantee, and it supplies
  only one down-picked sustain per pitch with no mutes or round robins. It is
  therefore an attack/decay/intonation direction check, not a fit corpus.

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
written consent for a competitive product.
[Pianobook's Clean 8 String Guitar](https://www.pianobook.co.uk/packs/clean-8-string-guitar/)
is an unusually well-documented 27-inch exact-eight lead with gauges,
C-G-C-G-C-G-C-E tuning, direct Hi-Z capture and five round robins per note, but
the [site terms](https://www.pianobook.co.uk/terms-conditions/) expressly forbid
use in or in relation to competitive products; it requires a separate written
grant before even private Electry calibration. Finally,
[aomartinezg/music-sheet-generator](https://github.com/aomartinezg/music-sheet-generator)
documents an Ibanez RG8 Drop-E DI corpus, but the raw samples were never
committed and the repository has no license. These are permission or paid
listening leads, not inputs to the figures below.

No reference audio is committed. License, instrument identity, register and
chain are recorded here precisely so a useful secondary reference cannot
quietly become a calibration master later.

### Exact-eight open-note direction check

The two low `Natural` 8ridgelite anchors are 20-second, 48 kHz, 24-bit stereo
WAVs. Their SHA-256 values are
`1a9e05a3a2eeae067bdddd4fc102d50deb0c749bddfb0632c5e1f4a2ac866c5a`
for `0_e1.wav` and
`c95c1989892142246b291354bb808e98991a529fb52818920554581165f09018`
for `13_e2.wav`. The two channels are not duplicates (whole-file correlations
-0.0984 and -0.0806), but the repository does not identify what they represent,
so they remain anonymous channels 1 and 2.

Every real and Electry signal uses the same audio-domain alignment. With
`N = round(0.002 Fs)`, the centred envelope is the RMS of those `N` samples;
onset is its first crossing of 25% of its own maximum in the following second.
MIDI Note On, modeled contact start and modeled string-slip time remain
implementation diagnostics and are not substituted for that audio onset. For
the model, onset occurs 1.542 ms (E1) and 1.451 ms (E2) after Note On; the
absolute envelope maxima are 27.324 and 14.286 ms after Note On, yielding the
table's 25.782 and 12.834 ms audio-onset-relative values. Every decay number is
RMS relative to the same signal's 0-50 ms window:

| note / signal | onset-to-peak | 50-150 ms | 150-500 ms | 500-1000 ms | 1000-2000 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 8ridgelite Natural E1, channel 1 | 1.79 ms | -2.09 dB | -2.87 dB | -4.39 dB | -6.76 dB |
| 8ridgelite Natural E1, channel 2 | 2.40 ms | -1.64 dB | -2.97 dB | -4.48 dB | -4.66 dB |
| Electry E1 Open | 25.78 ms | +0.45 dB | -0.90 dB | -2.15 dB | -4.76 dB |
| 8ridgelite Natural E2, channel 1 | 1.75 ms | -0.92 dB | -2.24 dB | -3.93 dB | -6.13 dB |
| 8ridgelite Natural E2, channel 2 | 1.29 ms | -1.49 dB | -2.38 dB | -4.14 dB | -5.50 dB |
| Electry E2 Open | 12.83 ms | +0.35 dB | -1.46 dB | -4.00 dB | -8.00 dB |

The late open decay is already near this reference: Electry E1's 1-2 second
change lies between its two channels, and E2 falls only 1.87-2.50 dB farther.
The global-peak column initially looks like a late low-string attack, but a
code-path audit narrows the alarm. With `T = Fs/f0`, define the first-lobe crest
as the envelope maximum from onset to `0.5T`, and the returned-lobe crest from
`0.5T` to `1.5T`. Electry's first-lobe crests arrive 1.429 ms after E1 onset
and 1.678 ms after E2 onset, comparable to the reference's 1.29-2.40 ms global
peaks. Its returned lobe then wins by only +1.20 and +2.77 dB, moving the
reported global maximum almost one period later. This is not a late MIDI event
or pickup tap: the direct excitation is available immediately, and the
bridge-pickup travel is only about 1.32 ms on E1 and 0.66 ms on E2.

The returned maximum follows the far travelling-wave path represented by the
folded loop's pluck-position image. For a pluck at `x = pL` from the bridge and
a pickup at `qL < x`, the bridgeward wave reaches the pickup after
`(p-q)T/2`; the nutward wave reflects and arrives after `T-(p+q)T/2`. Their
separation is therefore `(1-p)T`, independent of pickup position. The current
loop realizes that physically plausible separation by writing its image `pT`
behind the write head while feedback reads a full period `T` behind. At the
default near-bridge pick position, that geometry plus the two modal-pole rise
times predicts about 25.2 ms on E1 and 12.6 ms on E2, close to the measured
returned crests.

That diagnosis does not establish a correction.
[Lee, Depalle and Scavone's electric-guitar waveguide](https://jcaa.caa-aca.ca/index.php/jcaa/article/view/2443)
splits an ideal acceleration pluck into both travelling-wave lines, while
[Välimäki et al.'s single-delay-loop reduction](https://aaltodoc.aalto.fi/bitstreams/3e583e58-7367-48b0-a571-fc530b2a3d20/download)
places its pluck-position FIR comb before the feedback loop. At the ideal string
modes, using `pT` or `(1-p)T` for the comb delay is magnitude-equivalent but not
a proof that either transient phase reference is superior at an inharmonic,
filtered pickup output. Complementing the current history offset can remain a
non-shipping phase-orientation diagnostic, not a presumed root fix. Because
global onset-to-peak jumps by a whole period whenever the two nearly competing
crests exchange rank, any capture study must retain first-crest timing and
returned/first-crest ratio instead of fitting only that discontinuous maximum;
it must decide path balance, termination loss and excitation phase separately.

Eight inspected Natural pitch anchors from E1 through E4 put E1 at
+36.56/+49.05 cents while the remaining fourteen channel values have a median
absolute deviation of 3.8 cents and a +0.3..+18.6-cent range. The repository's
separately corrected E1 is -0.30/+1.67 cents, confirming that the large Natural
E1 offset is source intonation or tuning rather than a target for Electry.

One edited sampler take, no pickup identity and no repeats cannot set a release
time or shipping threshold. Preserve the current excitation, tuning and decay
rails; use the attack result to prioritize a controlled multi-player
exact-eight pick capture, never to fit path balance directly to these files.

### CC0 seven-string muted/sustained matrix

[inspektral's `50hz-guitar` pack](https://freesound.org/people/inspektral/packs/42559/)
adds a real extended-range comparison that the isolated Drop-E phrase above
cannot supply. It is tagged `7-string` and `baritone` and contains matched
bridge- and neck-humbucker recordings labelled sustained or muted at nominal
50, 75 and 100 Hz, two takes per condition. The originals are mono 48 kHz,
24-bit WAV under CC0. This pass used the public high-quality previews; no
third-party audio is committed.

The bridge files are muted IDs 783919/783920, 783921/783922 and 783913/783914,
paired respectively with sustained IDs 783701/783702, 783703/783704 and
783695/783696. The uploader says only “muted”: Palm technique, raw-DI status,
recording chain, instrument model and exact tuning are unknown. G1/D2/G2 and
model MIDI 31/38/43 are nearest-note inferences from the nominal frequencies,
not source metadata. These limitations make normalized muted-minus-sustained
shape useful and absolute level or a Palm coefficient inadmissible.

Each take is normalized to its own 0-50 ms onset. The four decay entries below
are muted change minus its paired sustained change in 50-150, 150-500,
500-1000 and 1000-2000 ms windows. The final column tracks harmonic power
above 500 Hz relative to all tracked power below 2.6 kHz, subtracting the
sustained 0-60-to-60-160 ms change from the muted one. Preview/noise floor can
dominate the late upper band, so that last value is a direction alarm only.

| nominal source / model note | CC0 paired decay | model 1.10 / 70 ms | model 1.25 / 100 ms | upper-share contraction, CC0 / before / current |
| --- | --- | --- | --- | --- |
| 50 Hz / G1 | -4.47, -9.43, -14.14, -17.48 dB | -4.42, -8.89, -12.51, -13.78 dB | -4.89, -9.22, -12.57, -13.79 dB | -23.72 / -8.31 / -14.71 dB |
| 75 Hz / D2 | -2.73, -8.24, -13.44, -16.15 dB | -4.09, -7.77, -10.51, -11.14 dB | -4.48, -8.08, -10.57, -11.15 dB | -22.07 / -5.85 / -9.26 dB |
| 100 Hz / G2 | -3.27, -6.46, -8.71, -11.05 dB | -4.47, -7.54, -9.47, -9.31 dB | -4.93, -7.92, -9.58, -9.33 dB | -15.85 / -7.08 / -10.62 dB |

Broadband decay was already close enough that the post-baseline change is
mixed rather than uniformly better. The retained change instead addresses the
clearer common mismatch: every model pitch loses upper share too slowly. The
current contact closes part of that gap on all three notes without making the
preview-derived values a hard test rail.

### Score-matched repeated-Palm proxy

[HiMMP's FAQ and download index](https://himmp.net/faq.html) license the raw
multitracks and score for “In Solitude” under CC BY. Four independent 44.1 kHz
rhythm DIs accompany a six-string Drop-C score at 200 BPM. Bar 18 explicitly
marks four quarter-note low-C2 Palm strikes at 0, 300, 600 and 900 ms. Bar 78
is sample-exactly the same audio in all four DIs and was excluded, leaving 16
unique real strikes. The model probe renders the same four-hit schedule on MIDI
36 with one fixed Down stroke because the score does not identify direction.

RMS shapes are onset-aligned from each DI's own high-passed 2 ms envelope and
sampled at approximately 1 ms steps from 8 to 111 ms with 15 ms windows. Body
and tail are relative to 0-30 ms onset; the selective metric is the same
tracked above-500 Hz share contraction from
0-30 to 30-80 ms used for F2. Model medians summarize its four hits:

| measure | real, four DIs | model 1.10 / 70 ms | model 1.25 / 100 ms |
| --- | ---: | ---: | ---: |
| within-phrase envelope correlation | 0.9416 | 0.996875 | 0.997303 |
| 30-80 ms body / onset | -3.6737 dB | -2.5546 dB | -2.7242 dB |
| 80-120 ms tail / onset | -8.4291 dB | -6.1413 dB | -6.7686 dB |
| upper-share contraction | -9.3158 dB | -4.2231 dB | -4.7476 dB |

The retained contact moves all three decay measures toward the real phrase,
but does not solve its human repetition spread. Per-hit heel-position jitter
was rejected: it worsened the controlled F2 contraction, while the separate
CC0 isolated muted pairs have highly repeatable envelope correlations of
0.9885-0.9964. A conventional six-string phrase and an unlabelled extended-
range mute cannot identify a new randomness source. This remains a
score-matched behavioural probe, not an eight-string fit target.

### Clean E1 attack/body direction

The clean CC0 phrase now also has the exact model-only 0-30/30-80 ms alarm
applied to both open and muted attacks. RMS uses the raw windows. Spectrum and
autocorrelation use mean-removed windows, with Hann applied only to the
4,096-point spectrum. Centroid and the denominator use
`20 <= f <= 8,000 Hz`, the upper numerator uses strict
`500 < f <= 8,000 Hz`, and harmonicity is unwindowed normalized
autocorrelation over the E1 lag range. Real rows are medians of the two attacks;
model rows are the current raw E1 evaluator probes at the provisional
finite-width checkpoint.

| output | 30-80 ms RMS vs 0-30 | centroid, 0-30 / 30-80 | upper share, 0-30 / 30-80 | 30-80 harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real CC0 E1 open, median | +2.8801 dB | 234.34 / 256.93 Hz | 6.3775% / 5.7216% | 0.911636 |
| real CC0 E1 “muted string,” median | +0.9272 dB | 420.76 / 309.83 Hz | 17.1698% / 11.5592% | 0.850485 |
| Electry E1 open | -1.2161 dB | 267.44 / 226.98 Hz | 5.3662% / 1.1851% | 0.987064 |
| Electry E1 Palm, medium | -5.4608 dB | 92.81 / 173.70 Hz | 0.3875% / 0.1207% | 0.983527 |

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
The Electry rows preserve the pre-finite-width fresh-engine baseline moved one
semitone to MIDI 41; medium means the shipping 55% Mute Tightness.

| output | 30-80 ms RMS vs 0-30 | centroid, 0-30 / 30-80 | upper share, 0-30 / 30-80 | harmonicity, 0-30 / 30-80 |
| --- | ---: | ---: | ---: | ---: |
| P1 ordinary F2 | -0.7266 dB | 317.49 / 292.39 Hz | 7.2768% / 3.7843% | 0.894177 / 0.997540 |
| P1 Palm F2 | -1.7333 dB | 270.14 / 216.60 Hz | 9.4454% / 1.0947% | 0.941525 / 0.998806 |
| P2 ordinary F2 | +0.1341 dB | 162.33 / 160.93 Hz | 1.2405% / 1.0595% | 0.950455 / 0.997666 |
| P2 Palm F2 | -3.0940 dB | 161.88 / 119.44 Hz | 0.7239% / 0.0198% | 0.895860 / 0.994374 |
| Electry F2 open | -0.5178 dB | 430.29 / 421.95 Hz | 32.6312% / 33.7896% | 0.884288 / 0.998472 |
| Electry F2 Palm, medium | -3.3247 dB | 397.96 / 352.58 Hz | 27.4550% / 22.9345% | 0.715978 / 0.997254 |

At that baseline, the model's total body contraction is near the two real Palm
cells and its
30-80 ms periodicity lands inside their 0.9944-0.9988 range. The unresolved
mechanism is earlier and selective. Subtracting each ordinary note's own
0-30-to-30-80 upper-share change from its Palm counterpart gives -6.52 dB for
P1 and -14.95 dB for P2, but only -0.93 dB for Electry. Conversely, Electry's
Palm onset is *less* periodic than either real Palm. This supports a short
time-varying tonal/contact mechanism, not a sustained random-noise layer or a
blanket darker excitation.

At the pre-tuning checkpoint, two one-scalar A/Bs were rejected. Reducing Palm's
existing attack-noise multiplier from 1.5 to 1.0 moved E2 onset harmonicity only
0.006 and further
darkened E1. Removing Palm's 0.74 excitation-modal darkener improved F2 onset
harmonicity from 0.715 to 0.802, but weakened the paired selective contraction
from -0.90 to -0.59 dB and opened the muted body. Neither fixes the measured
vector, and combining them with an unmeasured compensating decay constant would
be curve fitting. Both shipping constants were restored. The provisional
finite-width checkpoint below keeps the 0.74 darkener; commissioned dry E1/E2
repetitions remain the gate for calibrating and finally validating that contact
model.

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

The current 128-value CC2 sweep has worst adjacent 0-50 ms RMS moves of only
0.0360 dB on E1 and 0.0394 dB on E2. The former solver-boundary cells now
measure 0.0284 and 0.0341 dB, with a 0.5 dB ceiling. Attack level and absolute
150-500 ms tail are strictly nonincreasing on both strings, as is the
attack-normalized E1 tail. E2 has one negligible +0.00142 dB normalized-tail
rebound from CC2 2 to 3. The older endpoint-tilt figures predate the finite
contact and are not carried forward as current evidence; commissioned
pressure/contact captures—not another local clamp—must decide whether the
extreme needs a richer loss topology.

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
engine's 0.040 m/unit calibration turns into 20.8..9.6 mm—well beyond the
then-current tension prototype's 2.75 mm E1 seed and the 1.75 mm eighth-string
factory-action reference in
[.strandberg*'s official setup guide](https://support.strandbergguitars.com/article/55-how-do-i-set-up-my-guitar).
The window also aged for almost one silent E1 round trip before the excitation
returned, while negative below-clearance “excess” incorrectly drove saddle
rattle.

Clearance is now expressed as 2.08..0.96 mm and converted through the existing
SI calibration; its window begins only after the travelling string motion
reaches the loop output, and excess is clamped to real contact before every
downstream use. A deterministic regression requires the dedicated collision
PRNG to advance for maximum-force Palm and Dead on both E1 and E2. At that
correction's frozen 70 ms-contact checkpoint, the shipping 18% Artifacts
setting changed E1 Palm/Dead 0-150 ms by only about -55.18 to -59.97 dB
relative to the previous render. This repairs a
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
`beb0d759708d7824ebb15ce247e72c88d68a2f343fc4756f1f944ae62c445de6`).
Its exact bar starts are frames 11,025 and 148,601 at 44.1 kHz—the renderer
truncates every scored hold and gap to an integer frame—and the corresponding
E1 Palm Mute score hits are 0, 1, 2, 4, 5, 7, 8, 10, 11 and 13.

| source | corresponding pairs | 120 ms envelope correlation (raw; best within +/-2 ms) | paired 0-30 ms RMS difference |
| --- | ---: | ---: | ---: |
| real CC0 Drop-E E1 muted-string candidates | 1 | 0.742; 0.789 | 2.44 dB |
| current Electry dry E1 bars | 10 | median 0.988, range 0.975-0.994; median 0.988, range 0.978-0.996 | median 0.580 dB, range 0.039-2.813 dB |

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
`f22f51a1e4c5b8a9bd98934ff4f642359fe6956f60e645ee137f2bc617075015`.
Centroid and power share are mean-removed and Hann-windowed. Centroid and the
power-share denominator use bins with `20 <= f <= 8,000 Hz`; the power-share
numerator uses the strict upper band `500 < f <= 8,000 Hz`. Harmonicity is
normalized autocorrelation over the E1 lag range.

| output/context | 30-80 ms RMS vs 0-30 | centroid 0-30 / 30-80 | 30-80 ms power, `500 < f <= 8,000` / `20 <= f <= 8,000` | harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real distorted “muted string” | +0.87 dB | 1,033 / 1,506 Hz | 73.0696% | 0.212544 |
| Electry common-chain Mute | -5.31 dB | 131 / 306 Hz | 7.2910% | 0.847377 |
| real first ghost after Palm | +3.12 dB | 652 / 310 Hz | 9.9994% | 0.786 |
| real second repick | +1.30 dB | 754 / 1,314 Hz | 64.6730% | 0.247 |
| Electry common-chain Dead | -3.39 dB | 275 / 323 Hz | 8.8983% | 0.866851 |

The muted-candidate/model-Mute upper-band-share gap is 65.7786 percentage
points on that exact predicate. It remains a confounded processed-output alarm,
not a dry tuning target.

An independent chain audit locates this gap upstream of `ElectryFx`. For the
last muted E1 of demo 04's first bar (known contact frame 103,622), the 30-100
ms dry body has a 0.0071% upper-band share under the same predicate and 0.9837
harmonicity; the identical frame in demo 05's common chain raises those to
0.0212% and 0.9814. The FX adds
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
(0.348 after best alignment within +/-2 ms), against 0.9882 raw and 0.9908
best-aligned medians for the final common-chain model bars, so their output
trajectories repeat less alike. Paired 0-30 ms level displacement is 0.431 dB
for the real pair versus a 0.311 dB model median (0.029-1.506 dB range).

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
normalized autocorrelation at E1 lags. The current in-test medians are
7.291250% upper-body power and 0.847379 harmonicity; loose one-sided rails
require more than 6% and less than 0.97 respectively. They reject a materially darker or
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
| 30-100 ms RMS | -3.57 dB (-10.12..+1.17) | -33.82 dB | -8.10 dB |
| 100-250 ms RMS | -12.66 dB (-20.68..-6.20) | -51.34 dB | -15.12 dB |
| 250-380 ms RMS | -20.75 dB (-29.18..-12.71) | -62.04 dB | -23.43 dB |
| centroid, 0-30 / 30-100 / 100-250 ms | 220.7 / 136.9 / 85.3 Hz | 261.8 / 157.3 / 92.2 Hz | 209.3 / 142.2 / 94.3 Hz |
| 30-250 ms harmonicity | 0.936 | 0.385 | 0.988 |

Those model columns are the medians of a timing-matched
Open -> Mute -> Dead -> Dead render with no invented note-offs. The old
30 ms fretting-hand loss had an acceptable-looking centroid only because it
had erased the pitched body: its three-window envelope error averaged 36.74 dB
and its periodicity collapsed. The corrected model reduces that envelope error
to 3.23 dB, centroid error from 22.82 to 8.57 Hz and harmonicity error from
0.551 to 0.052. Every corrected RMS window lies inside the observed four-hit
range; its slightly high periodicity remains a named one-recording limitation.

A smaller 450 ms broadband-loss candidate with the Dead noise boost removed
was rejected. Aligning at its later full-band peak made it look close, but at
the fixed contact frame its median centroids were 547/290/215 Hz: the lingering
upper modes sounded like a bright picked note rather than the recorded thunk.
The retained correction instead changes only the existing Dead path: the
fretting-hand loss target becomes 1.6 s, its upper decay fit moves from 3.6 kHz
to the eighth partial, and the already-present attack hand darkening is set to
15%. At that Dead-only checkpoint, Sustain and all eight Open/Palm evaluation
WAVs remained byte-identical.

Mute Pressure remains the independent bridge hand and stacks with Dead. Its
one-shot impact formerly armed only above 10%, creating a discontinuity even
though the continuous loss itself was smooth. Arming it at every positive
pressure lets its amplitude tend to zero naturally: over pressure
0/.099/.100/.101/.25/.5/1, Dead E1's 20-100 ms RMS now falls monotonically and
full pressure is 8.61 dB below zero pressure; the equal steps around 10% have
normalized attack differences 0.000519/0.000521, safely replacing the old
roughly 24x jump. No parameter, keyswitch or mapping changed.

The four-hit regression now recreates both complete annotated passes and
computes their medians, reproducing -8.101/-15.123/-23.433 dB in the three
relative-RMS windows. That pins the model column above instead of testing only
the first two hits against broad ranges.

That median also concealed a repeat-context miss. Relative to each hit's own
0-30 ms onset, the second Dead attack decays faster than the first in both real
passes, even though those second onsets are 2.30 and 3.82 dB louder:

| second minus first | real 30-100 / 100-250 / 250-380 ms | Electry |
| --- | ---: | ---: |
| pass 1 | -6.908 / -4.344 / -3.268 dB | +0.600 / -0.007 / -1.053 dB |
| pass 2 | -5.093 / -2.790 / -2.889 dB | +0.701 / +0.986 / +1.325 dB |

The absolute-pitch correction moved the fixed-window envelope error from 2.75
to 3.23 dB and this six-value contextual RMSE from 4.559 to 4.92754 dB. Every
aggregate window still lies inside the four real hits' ranges. No Dead
coefficient was retuned from that one performance to recover the old snapshot:
exact low-chord tuning is the stronger invariant, and moving-pitch phase had
partly contributed to both old scores. The deterministic regression is
explicitly re-frozen at the nearest-tenth 5.0 dB cap, so a future contact-state
candidate cannot improve the aggregate median by erasing the real
first-to-repick ordering. Scaling Dead's existing loss by the per-stroke
hand-contact factor improved the score only slightly and still predicted the
wrong early-window direction; two pairs from one performance do not justify
shipping that retune. The commissioned dry train captures remain the
calibration gate.

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

At the pre-tuning checkpoint, four stronger substitutes were rejected before
any shipping edit:

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
The integer-delay fixture proves that the two physical rails need not mean
two allocated arrays: a sign-folded ring plus an in-place paired-cell update is
exactly equivalent. It pins zero-contact identity, reciprocity, squared-wave
energy nonincrease and sample-exact two-rail/folded-ring impulse responses.
That integer proof alone does not cover Electry's cubic fractional seam, so the
retained production path has a separate transfer sweep over the cubic reads,
the complete finite footprint and the string's actual modes. In the robot
experiment of
[Pluta, Tokarczyk and Wiciak](https://www.mdpi.com/2076-3417/12/3/1659),
measured E2 re-excitation at 12 and 70 ms did not return to the single-pluck
spectrum even after one fundamental period. Adding a moving one-sided rigid
contact at least reproduced the observed damping, ringing and pitch glides,
although neither simulation matched the measured spectra accurately.

### Provisional finite-width Palm checkpoint

The retained finite contact is Palm-style only. Mute Tightness moves its centre
from 4 to 20 mm at the saddle; its full footprint is provisionally 4 mm. At the
lower edge, centre and upper edge, equal shorter/longer cubic delay reads sample
the two travelling directions. The three symmetric pairs are combined with
non-negative 0.25/0.50/0.25 weights and blended with the free string, making a
six-cubic-read passive contact. It holds for 100 ms and releases linearly over
10 ms; Open, Dead and the other styles do not retain that Palm heel. The
existing 0.74 excitation-modal darkener is unchanged.

The 100 ms hold remains a provisional development duration, not a fitted hand
pressure or acoustic-loss constant. It extends the fixed contact into the
60-160 ms extended-range comparison window; the loss accumulated in the loop
persists after the contact releases from 100-110 ms. The 10 ms release is only
a generic discontinuity-smoothing heuristic, not a measured Palm-lift
trajectory.

The third-order Lagrange magnitude identity bounds each individual cubic read;
the production-transfer regression then sweeps representative production
phases and the combined finite contact at unity magnitude, preserves exact
zero-depth identity and does not invert a tested string mode. Its maximum
fundamental-phase and audible-mode phase errors are 0.000039 and 0.553 cents.
This is a bounded finite-width contact inside the folded single-delay loop, not
a distributed hand/string force solve or a measured heel footprint.

The tracked-harmonic F2 proxy, capped below 2.6 kHz, improved its paired
0-30-to-30-80 ms selective contraction from the frozen -0.790061 dB baseline
to -4.00942 dB. The identical extractor gives -6.098930 dB for P1 and
-15.289719 dB for P2. Relative to their two-player midpoint of -10.694324 dB,
the candidate reduces the baseline error by 32.5%; that midpoint is a secondary
development reference, not a robust population estimate. The underlying
44.1 kHz tracked-harmonic above-500 Hz shares are:

| F2 path | 0-30 ms onset | 30-80 ms body |
| --- | ---: | ---: |
| Open | 0.348460 | 0.336936 |
| Palm | 0.223310 | 0.0857751 |

Palm is therefore 1.93 dB below Open at onset and 5.94 dB below it in the body.
The onset lies inside the two real cells' -2.61..+0.73 dB bracket and the body
is 0.57 dB beyond P1's -5.37 dB, but the time contraction remains weaker than
either real cell. Dead was not retuned; its contextual regression RMSE remains
4.93049 dB.

A sliding check makes that trajectory visible without imposing two coarse
endpoint windows. Each value below is
`10 log10(Palm upper-share / Open upper-share)` over a 30 ms Hann window, using
the same 44.1 kHz tracked F2 harmonics below 2.6 kHz and the same 500 Hz split;
columns are window-start times in ms:

| F2 path | 0 | 5 | 10 | 15 | 20 | 25 | 30 | 35 | 40 | 45 | 50 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| P1 | +0.73 | -0.48 | -1.73 | -2.71 | -3.44 | -4.26 | -4.71 | -5.21 | -5.45 | -5.55 | -6.01 |
| P2 | -2.61 | -4.71 | -6.43 | -7.71 | -9.69 | -11.71 | -14.04 | -16.45 | -18.83 | -21.22 | -22.00 |
| Electry | -1.932 | -2.290 | -2.847 | -3.345 | -3.886 | -4.386 | -4.943 | -5.517 | -5.989 | -6.610 | -7.074 |

Every listed window ends by 80 ms and is wholly inside the 100 ms hold. The
current fixed contact yields monotonic accumulated selective loss throughout.
An explicit per-note low-at-pick/recovery ramp was therefore rejected at this
checkpoint. The audio identifies a loss trajectory, but
cannot identify changing hand pressure separately from the accumulated action
of a fixed contact. Reinitializing such a per-note ramp at every attack would
also impose a false pressure reset on rapid chugs instead of preserving the
preceding contact.

The paired proxy stays between -4.009 and -4.044 dB at 44.1, 48, 96 and
192 kHz. In the rapid E1/E2 repick-state grid, median absolute phrase/isolated
error is 2.240 dB and median worst-cell error is 3.428 dB; twelve deterministic
E1 Mute strokes span 1.981 dB of attack-normalized tail. Those are model
stability checks, not evidence that the variation distribution matches players.

This F2 proxy is a pre-capture development guard, not the commissioned
contract's per-harmonic mute-minus-open contour RMSE gate. That gate still
requires at least 3 TRAIN and exactly 2 untouched HOLDOUT player/guitar
clusters, scored separately, and it remains mandatory before any capture-parity
or market claim.

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
measure simultaneous acoustic loss, so those pressure dips establish neither
the sign of a loop-loss change nor its recovery timing and cannot be copied
directly into an attack ramp. Reboursière et al.'s six-string hexaphonic study
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

## 2026-08-24 rejected tension-glide trial

Pitch values below are cents relative to the same take's 700 ms position.
“Before” is the same default E1/style/velocity protocol before the physical
stretch recalibration; “trial” is the force-derived implementation that was
tested on 2026-08-24 and removed after the absolute-pitch audit below.

| condition | 30 ms | 100 ms | 220 ms | 420 ms | 700 ms | first-420 ms pitch RMSE vs real |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| real open, median of 2 | +41.30 | +31.73 | +20.99 | +1.13 | 0.00 | — |
| Electry open, before | +0.06 | +0.07 | +0.06 | +0.02 | 0.00 | 28.03 cents |
| Electry open, trial | +11.13 | +9.16 | +6.31 | +2.96 | 0.00 | 20.24 cents |
| real palm mute, median of 2 | +52.53 | +45.07 | +22.54 | +9.08 | 0.00 | — |
| Electry palm mute, before | +2.06 | +1.51 | +2.03 | +2.28 | 0.00 | 35.04 cents |
| Electry palm mute, trial default | +11.08 | +8.63 | +6.39 | +2.35 | 0.00 | 28.95 cents |

On this relative-trajectory measure the trial reduced the open error about 28%
and the default-Mute error about 17%. That did not establish concert pitch: the
real takes had no matched tuner reference, and every row was normalized to its
own late pitch. The trial therefore treated a useful trajectory observation as
an absolute 4.44 N force calibration without evidence for the other seven
strings.

The 2026-08-25 hard-pick audit measured the actual 30-210 ms pitch against MIDI.
At maximum velocity the trial was approximately +25, +14, +8.5, +6, +3.5, +14,
+13.5 and +6 cents on E1 through E4. That made the E1-E2 octave about 16.5 cents
narrower during the attack and similarly damaged other chord intervals; Mute
and Dead inherited smaller low-chord errors. The force-derived path was removed
rather than retuned from another guessed constant. The shipping regression now
checks all eight isolated maximum-velocity Sustain opens within 8 cents and
under 6 cents of total spread, plus a simultaneous low Sustain chord and the
isolated E1/B1/E2 Mute and Dead fundamentals against the same rails. The current
render passes with a 0..2.5-cent isolated Sustain spread and a -4.25..+0.75-cent
simultaneous low-chord spread. A future
attack-glide model must first pass matched absolute-pitch, multi-string and
multi-velocity captures as well as these chord-tuning rails.

No control or keyswitch changed, and the playable range is unchanged.
The plug-in wrapper exposes Double as the third choice of its single Output
Mode parameter; it does not alter this JUCE-free single-engine probe.

The bridge-hand contact now also distinguishes a soft stroke from a hard one.
The probe below expresses 0.5-1.0 s RMS relative to each attack's own 0-50 ms
RMS, so ordinary MIDI level does not masquerade as a decay change:

| palm-mute probe | velocity 0.20 | velocity 0.60 | velocity 1.00 | soft-to-hard spread |
| --- | ---: | ---: | ---: | ---: |
| E1 (MIDI 28) | -13.050 dB | -15.027 dB | -16.614 dB | 3.56 dB |
| E2 (MIDI 40) | -11.802 dB | -13.325 dB | -14.603 dB | 2.80 dB |

This is a conservative attack latch, not another audio follower: the existing
velocity amplitude and deterministic stroke-force draw are multiplied, clamped
to 0.32-1.25, and applied to the final positive hand-loss rate. It adds no UI or
random draw. With no hand the path remains exactly absent, Velocity Response at
zero removes MIDI velocity exactly for the same stroke, and both Mute Tightness and
CC2 pressure retain monotonic tail contraction. The open render is unchanged.
The separate palm-impact retention is defined at 48 kHz and rate-normalized:
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
| E1 open | 25.78 ms | 0.00 dB | +0.45 dB | -0.90 dB | -2.15 dB |
| E1 palm mute, medium/default | 2.13 ms | 0.00 dB | -5.16 dB | -11.28 dB | -16.43 dB |
| E2 open | 12.83 ms | 0.00 dB | +0.35 dB | -1.46 dB | -4.00 dB |
| E2 palm mute, medium/default | 1.29 ms | 0.00 dB | -4.95 dB | -10.18 dB | -14.38 dB |

The real CC0 E1 open take falls 9.29 dB by the last window while Electry falls
2.15 dB. That clip's muted take falls only 3.96 dB there, against Electry's
16.43 dB, and its two muted onset-to-peak times are 90.57 and 21.59 ms (median
56.08 ms). The fourfold spread between two attacks, unknown hand depth and
preview chain already ruled out a blind retune; Guitar-TECHS now confirms that
the slow median is not a general palm-mute requirement.

The evidence-backed change is therefore local to contact physics. The hand's
solved spectral loss is present from the attack instead of growing through an
unsupported 40 ms fade, and stays full until the loop has established a real
energy peak before its existing energy-driven relaxation can begin. Mute
also keeps the sustained triangular release displacement at the Sustain value
of 1.55 instead of pre-shrinking it to 1.28: the hand damps a loaded string; it
does not prevent the pick from loading it. In the current noise-free regression,
Palm's tracked-harmonic high/low ratio is 10.13 dB below Open on E1 and 3.14 dB
below it on E2 during 0-50 ms. These changes add no parameter, keyswitch,
random source or performance rule.

At the default depth, paired mute-minus-open deltas in the same four windows
are `[0, -4.32, -9.67, -13.00]` dB below 500 Hz and
`[0, -17.38, -31.81, -34.36]` dB above it on E1. On E2 they are
`[0, -4.00, -7.58, -9.35]` and `[0, -12.77, -31.06, -44.31]` dB. The current
model has the evidence-backed faster high-band contraction in every measured
post-attack window on both strings.

The existing Mute Tightness control spans a real envelope range rather than three
labels over one sound:

| probe and depth | 0-50 ms | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: | ---: |
| E1 palm light (0.00) | 0.00 dB | -2.40 dB | -7.89 dB | -14.32 dB |
| E1 palm medium/default (0.55) | 0.00 dB | -5.16 dB | -11.28 dB | -16.43 dB |
| E1 palm hard (1.00) | 0.00 dB | -8.09 dB | -13.98 dB | -19.34 dB |
| E2 palm hard (1.00) | 0.00 dB | -7.15 dB | -11.83 dB | -16.13 dB |

On E2 the palm-minus-open high-band deltas at 50-150, 150-500 and
500-1000 ms are respectively `[-2.97, -11.65, -25.20]` dB at light,
`[-12.77, -31.06, -44.31]` dB at the default and
`[-29.53, -51.51, -52.17]` dB at hard. This monotonic sweep is the physical
model's answer to sample libraries' discrete mute layers: one understandable
control remains continuous across the modeled range.

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

The workflow audits pointed to interaction gaps, not to a need for more mute
samples.
[Hydra's official TACT manual](https://impactsoundworks.com/manuals/Shreddage%203.5%20Hydra%20Manual.pdf)
allows articulation conditions to latch or act temporarily;
[Evolution Dracus](https://www.orangetreesamples.com/download/manual/EvolutionDracus-UsersGuide.pdf)
offers latching and non-latching keyswitch assignments; and
[RealEight](https://www.musiclab.com/assets/files/RealEight.pdf) can switch its
separate left-hand and bridge mutes by key, pedal, wheel or velocity, with key
operation toggled or held. Electry answers that surface with one visible
`LATCH | HOLD` choice for the existing play-style bank, defaulting to the former
`LATCH` behavior. The PLAY STYLE strip stores the base; in HOLD, the newest
physical, host or on-screen play-style key overrides it only while down and
reveals an older held key or the base on release. Pick Stroke remains independent
and velocity keeps its physical force meaning.

A later direct-performance audit found the other gap: the picking hand could
not strike a string again without sending another playable Note On and thereby
changing fretting-key ownership. Hydra exposes per-string held-note pick keys,
while [Ample Metal Hellrazer](https://www.amplesound.net/en/pro-pd.asp?id=33)
can repick a held chord. Electry now reserves E6..B6 as a blue per-string repick
lane, mapped from physical strings 8..1. Each trigger uses its velocity and the
current Pick Stroke and Play Style, re-enters the existing physical attack path,
and adds no fretting-key owner. Durable per-string ownership survives an audible
Mute or Dead voice's retirement, keeps that stopped string out of open-string
sympathetic and free-string allocation paths until normal voice stealing, and
lets the picking hand restart it from silence.
The live fretboard is the visible front end to that same path: clicking a held
row sends one hard repick through the processor's bounded UI queue, while the
MIDI lane keeps velocity and sequencer timing. It adds no second ownership or
new articulation model and leaves an unheld row silent.

### Tremolo-picking workflow and rate audit

The per-string lane solved exact sequencer control, but it did not provide the
single held gesture players expect for a long black-metal line. The official
manuals show several distinct answers rather than one industry-standard rate
control:

| instrument/workflow | officially documented repeat surface |
| --- | --- |
| [Shreddage 3.5 Hydra](https://impactsoundworks.com/manuals/Shreddage%203.5%20Hydra%20Manual.pdf) | a looping recorded Tremolo articulation, per-string E6..B6 repicks and a D#0 last-note retrigger |
| [Electric Storm Deluxe](https://docs.native-instruments.com/ni-tech-manuals/electric-storm-deluxe-manual/en/using-electric-storm-deluxe) | a recorded tremolo articulation plus tempo-scaled performed patterns and timing humanization |
| [Evolution Dracus](https://www.orangetreesamples.com/download/manual/EvolutionEngine-UserGuide.pdf) | manual repeat keys and a pattern grid in the shared Evolution engine |
| [Ample Metal Hellrazer](https://www.amplesound.net/en/Main_Panel_Manual-AMH.pdf) | D6 repeats all currently held notes |
| [RealEight](https://www.musiclab.com/assets/files/RealEight.pdf) | manual repeat control and an automatic generator from 1/4 through 1/64-triplet divisions with 0-100 ms humanization |
| [Heavier7Strings](https://download.threebodytech.com/Heavier7Strings/en/usermanual?type=download) | a held tremolo/manual-repeat performance control, without a published strokes-per-second scale |

This supports one obvious, holdable wrist and continued access to exact repick
events. It does not support copying a sampled loop, inventing an unmeasured
human-error distribution or treating transport-synced patterns as the only
workflow.

The strongest rate evidence found is Armondes' 2026
[five-player doctoral experiment](https://repositorio.ufmg.br/items/8f3c7648-dd7b-4768-868f-2283bfc7822b).
Its second experiment contains 40 recordings: five players, two strings,
direct-at-maximum and progressive strategies, and two takes. The 20 direct
recordings' plotted inter-onset intervals visually occupy roughly 70-90 ms,
or about 11-14 attacks/s; those values are read from boxplots rather than a
published numeric table. The instrument is a conventional nylon-string guitar,
not an electric eight-string, so that cluster only makes a 12 strokes/s default
plausible. It cannot fit Electry's force, missed-stroke or timing distributions.

The CC-BY-4.0
[EG-IPT corpus](https://zenodo.org/records/15205644) contributes 52,320 raw
96 kHz/24-bit electric-guitar files, including a tremolo class, but its released
guitar is a six-string Gibson SG. The associated
[NIME 2025 paper](https://www.nime.org/proc/nime2025_14/index.html) describes a
seven-string holdout that is not part of the distributed licensed set. No
commercially reusable exact-eight tremolo corpus was found. Accordingly, the
8/12/16 strokes/s rows in Electry's capture protocol remain future commissioned
TRAIN/HOLDOUT anchors, not validation data.

Electry now makes B0/MIDI 23 a visible momentary **TRM** wrist. Velocity remains
pick force and the appended `tremoloRate` parameter spans 4-20 strokes/s with a
12 strokes/s default. Starting the gesture arms one immediate contact on the
next internal sample for notes that are already physically held. A playable
Note On at that same boundary consumes the armed contact instead of receiving a
duplicate attack. One shared fractional phase repicks all physically held
strings through the existing attack path, so Alternate advances once for a
chord; sustain-only strings remain inert. A due contact is skipped while any
held string still has an in-flight Strum delay rather than overwriting that
pending attack. E6..B6 retain their established one-shot behavior.

Overlapping B0 owners balance; a positive repeated Note On restarts the phase
and updates force, while a zero-velocity Note On releases an owner. CC120 and
CC121 preserve a physically held wrist, whereas CC123, Panic, prepare and
release clear it. Output-mode changes route the gesture to both engines. The
active hold is transient and never serialized; only its rate is saved. The
scheduler is deliberately free-running rather than host-tempo synchronized,
and adds no jitter, missed strokes, pattern editor or hidden direction bias.
`22-tremolo-picking-study.wav` renders the planned 8/12/16 rate anchors,
a moving single-note line, a vibrato lead and a held chord through the same
physical path. It is audible workflow proof, not a human-performance fit.

### Complete-batch strum latency check

The exact-sample allocation solve below already gives the scheduler a complete
physical chord before any voice starts. The retained strum implementation did
not use that fact: it charged the scalar API's 20 ms causal re-anchor window to
every nonzero-spread batch, including a batch containing only one note. This was
not acoustic guitar latency or requested string travel; it was avoidable
scheduler lookahead in the normal plug-in path.

At a 48 kHz host rate the JUCE-free engine runs at 96 kHz. The scalar fallback
continues to hold a provisionally anchored chord for 1,920 internal samples,
because successive scalar calls may still reveal a different edge. A complete
batch now schedules the solved Down or Up edge at delay 0. Its later strings
retain the same accelerating offsets, and the seven-crossing duration is
sample-identical to the scalar fixture after subtracting that common pre-roll.
At a 12 ms setting the complete eight-string schedule is therefore 0.0, 14.7,
28.1, 40.7, 52.4, 63.5, 73.9 and 84.0 ms rather than 20.0 through 104.0 ms.

The frozen regressions establish the boundary rather than inferring it from a
mixed demo waveform:

- a complete one-note batch rendered for 4,096 samples is byte-identical at
  zero and 20 ms/string spread, and both voice delays are zero;
- complete Down and Up chords begin at their respective edge, reverse their
  monotone travel, and preserve the scalar path's total crossing time;
- the same scheduled chord rendered in 17- and 512-sample client blocks is
  byte-identical stereo;
- a second one-note batch 10 ms later advances Alternate to Up and begins at
  delay zero instead of merging into the preceding chord window;
- releasing the high string before a 40 ms/string stroke reaches it clears the
  pending contact, and the resulting stereo is byte-identical to the same
  low-string-only batch;
- through the processor, a one-note host event at sample 137 is byte-identical
  at zero and 12 ms/string spread, while a three-note spread chord begins on
  the same sample as its zero-spread counterpart and remains permutation
  invariant.

No excitation, velocity, timbre or travel coefficient changed. The demo
renderer now marks its written note/chord boundaries as complete batches, the
same contract the processor uses. Later MIDI timestamps remain performed
timing and start new strokes; only a client explicitly choosing successive
scalar calls pays the causal fallback.

The same audit exposed a fingering defect that sounded like broken tuning:
simultaneous chord notes were allocated one at a time, so host insertion order
could select different strings and frets, move the hand differently, and attach
different player-variation draws to the same score. The six permutations of
the Drop-E power chord `{33, 40, 45}` produced two physical shapes and six
different renders. Sorting bass-first or treble-first was not sufficient: each
has a playable counterexample where an early note occupies the only string a
later pitch can use. Electry now enumerates the bounded one-to-one assignments
for an exact-sample group and ranks ownership/legato continuity, four-fret
reach, occupied strings, hand movement, fret effort and uncrossed pitch order
before starting any voice. This follows the constrained-cost direction of
[Itoh and Hayashida](https://www.jstage.jst.go.jp/article/ieejeiss/124/7/124_7_1396/_article/-char/en)
and the playable-configuration direction of
[Yazawa et al.](https://cir.nii.ac.jp/crid/1573387452726377216), reduced to an
eight-string, allocation-free realtime solve. The regression now covers 134
permutations of four adversarial/open chord shapes with exact string/fret/hand
agreement and bit-identical stereo, plus host/on-screen source splits and MIDI
lifecycle barriers. Repeated-pitch permutations, owner-preserving re-fingering,
releasing-string obstacles and the deterministic lowest-eight overflow policy
have their own gates. The solve is deliberately chord-local; it does not claim
phrase-wide fingering look-ahead.

`LATCH | HOLD` is saved as non-parameter state alongside the current 27 host
parameters; transient held keys are never serialized. Because Electry is not
released, the suite pins current-state round trips rather than migration from
older development layouts. Play-style Note On, Note
Off and zero-velocity Note On are stably
conditioned before a same-sample attack in either host insertion order; CC123
joins that pass and retains its source order against them.
Duplicate and overlapping holds, base changes beneath a hold, delayed-note
style capture, UI/host parity, CC120/123, Panic, prepare/release, mode changes,
current state and hold-time state saves are regression-pinned. The repick lane
is additionally pinned across every physical string, zero-owner silence,
Alternate, Mute/Dead capture, original-key release, full audio retirement,
allocation, sympathetic exclusion and the live fretboard. These controls close
the documented workflow gaps without copying discrete half/full/short layers:
Mute Tightness, Mute Pressure (CC2) and stroke force remain one continuous
bridge-hand surface. B0 is separately pinned for exact rate across four host
sample rates, block-partition identity, shared chord direction, same-boundary
attack conditioning, Strum deferral, sustain exclusion, owner/lifecycle rules,
state round trips and UI/host parity. Demo 22 exposes the gesture, while the
frozen model-evaluator targets remain unchanged because this is a performance
path rather than a new articulation model.

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
  documents a clean eight-string/RME Babyface/Cubase workflow and thirteen dry
  chromatic one-shots played on the lowest F# string from F#1 through F#2. Only
  F#1 is an open string; the pack contains no Mute or Dead articulations and is
  not Drop-E.

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
strings EADGBe, and the [archive record](https://zenodo.org/records/15690894)
is CC BY-NC 4.0 with an additional research-only, no-commercial-product
statement. A listed Strandberg therefore does not turn it into either an
extended-range or lawful product-calibration source.

The 2025 [EG-IPT dataset](https://zenodo.org/records/15205644) contains 52,320
files across 19 techniques from a six-string 2005 Gibson SG Standard, including
isolated Palm-muted notes, DI and three pickup settings. Its live Zenodo dataset
record assigns CC BY 4.0 to the archive. The licensed HB-bridge DI sixth-string
pair supplies the E2 mechanism check above; it remains unsuitable for E1
fitting or an eight-string product benchmark because the pitch is measured
rather than documented, the performances are unpaired, and no velocity,
stroke-direction or pressure match is provided.

The 2026
[Longitudinal Guitar String Ageing dataset](https://zenodo.org/records/19823590)
is another lawful CC BY 4.0 auxiliary: two conventional six-string players and
guitars contribute raw 48 kHz DI across 28 daily sessions, including fixed
60 BPM open-string repetitions, bends, slides and legato. It is a strong future
source for repeat/age distributions, but neither guitar is extended-range and
it cannot calibrate E1, eight-string pickup balance or Palm/Dead contact.

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
into a rapid no-regression allowance. The complete enumerated input set,
formula and numeric result are frozen before holdout; display precision, the
candidate and holdout results cannot change them. A position-dependent DSP candidate must reduce per-harmonic
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

| cells | source-matched content | exact scored length at 44.1 kHz |
| --- | --- | ---: |
| 1-2 | E1 and E2 Palm single, dry | 30,870 frames / 700 ms each |
| 3-4 | E1 and E2 Dead single, dry | 18,963 frames / 430 ms each |
| 5 | 12-hit E1 Palm run, 83.33 ms IOI, dry | 53,802 frames / 1.22 s |
| 6 | the same Palm run through one common metal chain | 53,802 frames / 1.22 s |
| 7 | eight-hit Dead `[E1, E1, E1, E2, E1, E1, E2, E1]` groove, 83.33 ms IOI, dry | 44,541 frames / 1.01 s |
| 8 | the same Dead groove through the common chain | 44,541 frames / 1.01 s |
| 9 | eight-hit Palm-E1/Open-E2 lift/replant groove, 166.67 ms IOI, dry | 67,032 frames / 1.52 s |
| 10 | the same Palm/Open groove through the common chain | 67,032 frames / 1.52 s |
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

The referenced comparison JSON declares schema
`electry-blind-comparison/v1` and status `frozen`; the packer rejects any other
schema or status. That comparison manifest freezes the source cohort,
stimulus-selection and presentation seeds, selection/replacement algorithm and
listener analysis before analytical or listening access to that active holdout
cohort. Every
active holdout player/guitar cluster enters exactly five of the ten core cells.
The stimulus seed ranks canonical candidates with
`SHA-256(seed || NUL || "electry-stimulus-selection/v1" || NUL || domain ||
NUL || sorted_compact_JSON)`, lowest first. It chooses the lowest-ranked of all
pair-preserving 5/5 cluster assignments, then the lowest-ranked two-down/two-up
assignment across cells 1-4. For each fixed take, it ranks the complete eligible
session/unit pool: singles contain all six slots in the assigned direction,
grooves all three runs, and rapid Palm only the logged 180-BPM run. Palm uses the
middle contact landmark chosen on train. The generated receipt retains every
candidate and rank, and the packer regenerates the choices from the seed. After
active-cohort access, a structurally invalid, recording-failed or otherwise
unusable selected cell is missing and fails the minimum gate; it is never
substituted. A recapture may enter only as a new disconnected player/guitar
cluster under a new comparison manifest; reusing either physical player or
guitar remains part of the retired cluster.

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

Opaque filenames and a private key manifest hide provenance. Separately for
each practice/core pair and listener stratum, SHA-256 ranks the 15 one-based
participant numbers from `presentation_seed_bytes || NUL || "physical-a" ||
NUL || pair_id || NUL || participant_number`. Odd-numbered pairs assign the
first eight guitarist and first seven producer ranks physical=A; even pairs
assign seven and eight. That is exact 15/15 overall and 7/8 within each stratum
without the predictable public parity rule. The secret presentation seed also shuffles cell order, and each hidden
repeat occurs later with at least three intervening trials and reversed sides.
Each trial asks which clip is the
physical guitar (forced A/B plus confidence), which is more convincing in a
black/progressive-metal production (A/B/Tie), and optionally which defect gave
it away: attack, pitched body, decay, repetition, noise or other. After one
required playback start per side, permit at most three additional starts across
the pair and record the count. Exclusions are predeclared playback/
headphone-screen failures, not post-hoc disagreement with the expected answer.

The runnable task is deliberately blinded **A/B**, not ABX: a labelled physical
or model `X` would disclose the class the first question asks the listener to
identify. [`PrepareBlindListening.py`](../Tools/PrepareBlindListening.py) and
its dependency-free browser runner turn already finalized stimuli into the
frozen presentation pack. Copy and fill
[`comparison-manifest.template.json`](capture/electry-mute-capture-v1/comparison-manifest.template.json)
and
[`blind-study.template.json`](capture/electry-mute-capture-v1/blind-study.template.json)
only after the comparison manifest, crops, level matching and exports above are
sealed. Generate a fresh cryptographic presentation seed with
`python3 -c 'import secrets; print(secrets.token_hex(32))'`; keep it private,
never reuse it for another study and do not disclose it until unblinding is
complete. Then change the study status to `frozen_ready_to_pack` and run:

```sh
python3 Tools/PrepareBlindListening.py \
  /private/electry-blind-study.json /private/electry-blind-pack
# Copy the printed STUDY_FINGERPRINT to an external append-only record now.
cd /private/electry-blind-pack
python3 serve.py \
  --expected-fingerprint <externally-recorded-64-hex-fingerprint>
```

The pack-local standard-library server binds loopback by default, serves only
`public/`, disables directory listings and adds no-cache/nosniff headers. Do not
replace it with `python3 -m http.server`, whose listings would enumerate every
session. The bundled server is plain HTTP: keep the supervised workflow on
loopback, or place any non-loopback deployment behind a trusted TLS front end
with access control. The required fingerprint is the external trust anchor:
record the exact value printed by the packer outside the pack, ideally in an
append-only or signed ledger, before distributing a link or accepting a response. The server
runs a complete frozen-pack preflight against it before binding. For every
GET or HEAD request it then reads the requested public file once, checks that
snapshot against the preflight index and serves those same bytes; single-range
audio requests receive the same treatment, so a post-preflight edit is refused
rather than exposed. The coordinator privately maps IDs using
`private/participant-links.tsv` and gives each listener only their 128-bit
opaque URL, such as `http://127.0.0.1:8000/?session=<token>`; neither public
session filenames/manifests nor downloaded responses contain `p001`-style IDs.

The packer verifies the comparison manifest's exact v1 contract: frozen status,
study/seed/listener agreement, two non-overlapping holdout clusters, at least
three mutually disconnected engineering-train clusters, two distinct practice
sessions drawn from train, balanced cell-to-cluster assignments, every
stimulus hash and exact selection/render/chain/analysis settings. The settings
include the numerical listener gates and the current 4x common-chain effect
oversampling at 44.1 kHz, so an internally rehashed alternative setting is not
accepted. Every core cell also freezes content, processing, frames, source
cluster/session, capture take, slot/run and stroke, event/score record, and a
hashed QC record; dry/processed phrase counterparts must share that complete
provenance. The packer verifies the capture-manifest identity and selected-take
hash, the seed-bearing selection receipt, the complete engineering/listener
contract and a sealed registry containing every declared implementation,
build, preset and asset hash. It also verifies the sealed engineering
derivation receipt, combined result and one raw analysis result per complete
train cluster, including the exact eligible-unit coverage used to derive each
repeatability margin. These records attest the frozen analysis; retained train
audio and analyzer output remain the external evidence behind that attestation.

The exact verified evidence bytes are archived at
`private/study-manifest.json`, `private/comparison-manifest.json`,
`private/capture-manifests/`, `private/event-scores/`,
`private/selection-receipt.json`, `private/engineering-derivation-*.json`,
`private/engineering-train-analysis/`, `private/artifact-registry.json` and
`private/prepare.py`. The packer also verifies canonical RIFF with one 16-byte
PCM `fmt` chunk followed by `data`, mono 44.1-kHz 24-bit fields and a zero
required pad, exact core frame counts, equal practice-pair frame counts and
non-reuse outside the three declared repeats. It produces 30 deterministic
public session manifests with distinct per-trial opaque filenames, secret
exact 15/15 A-side balance for every pair, constrained later repeats and a
static response recorder. The source mapping, seeds, ID/token map, input paths
and implementation hashes remain private. `private/answer-key.json` binds all
evidence archives plus the preparer, runner, no-listing server and scorer;
byte-identical frozen copies of the latter two are `serve.py` and `score.py` in
the pack root. Downloaded response JSON names only
the opaque session/trial IDs, combined study/implementation fingerprint and a
seed-keyed SHA-256 commitment to that session's canonical private ID, stratum,
order and A/B mapping. The public session publishes the same commitment before
listening; the scorer recomputes it from the private key and rejects either a
post-pack key remap or a response carrying a different commitment.
Specifically, the commitment is
`SHA-256(presentation_seed_bytes || NUL || "electry-private-mapping/v1" || NUL
|| canonical_mapping_JSON)`, where the UTF-8 JSON contains only the participant
ID, stratum, session token, practice records and scored records, with keys
sorted, compact separators and ASCII escaping.

At scoring time the frozen scorer rehashes the archived study, comparison,
preparer, capture manifests, event/score records, artifact registry, selection
receipt, engineering derivation and raw train-analysis records; the browser
runner, no-listing server and pack-local scorer; all 30 public session manifests;
the exact participant-link bytes; and all 900 opaque public audio references.
It independently reconstructs the canonical 16-take capture inventory, every
eligible cluster-level engineering input, the R-7 P90 margins, formulas,
engineering contract and artifact coverage. It also recomputes the study
fingerprint, presentation mapping and source hashes before accepting responses.
These checks detect accidental or partial pack editing. They are not a
signature, trusted timestamp or proof that a malicious
coordinator honestly ran the declared selector before viewing holdout data;
the externally preserved capture/selection ledgers and frozen release hashes
remain the trust root.

The browser cannot cryptographically seal a client-authored download. Run each
session under supervision: the coordinator collects the response immediately,
computes its SHA-256 at intake and archives the original read-only before the
next session. Keep that intake ledger outside the response directory. The
scorer reports fresh scoring-time hashes for comparison with the ledger; it
does not ingest or certify the intake record, and neither hash turns an edited
client file into trusted evidence.

This packer consumes final stimuli. It does not detect onsets, select the
holdout cohort, crop, level-match, measure true peak, render the common chain,
prove a licence or certify that a manifest was frozen before access. The
repository template therefore remains
`not_ready_missing_licensed_captures`, and the tool refuses it today rather
than manufacturing a nominal “study” from absent recordings. Its deterministic
layout check is runnable without audio dependencies:

```sh
python3 Tests/PrepareBlindListeningTests.py
python3 Tests/ScoreBlindListeningTests.py
```

Recruit exactly 30 valid listeners: 15 active extended-range guitarists and 15
metal producers/engineers, with dual-qualified people assigned before listening
to the least-filled stratum. Continue recruitment only to replace predeclared
qualification, playback-screen or incomplete-session exclusions until those
exact valid counts are reached. Outcomes remain sealed and are never consulted
for stopping. The private key assigns `p001`-`p015` to
`extended_range_guitarist` and `p016`-`p030` to `metal_producer`; a replacement
inherits the excluded listener's ID instead of creating `p031`, so stratum size,
side balance and the frozen session remain unchanged. The frozen source cohort
contains exactly two active untouched
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

Core identification and preference use only each listener's ten non-repeat
cells. Practice answers never enter an endpoint. The three repeat answers enter
same-underlying-source identification agreement after undoing their A/B
reversal and the presentation-bias diagnostic, but never accuracy or preference.
Thus the A-side diagnostic covers all 390 scored forced choices. Palm aggregates
cells 1-2, 5-6 and 9-10; Dead aggregates cells 3-4 and 7-8. Physical-source accuracy
is binary. Electry preference is one for choosing its side, zero for choosing
the physical side and 0.5 for Tie. Core, articulation and cell point estimates
are the arithmetic means of those eligible values.

The frozen bootstrap has 20,000 replicates. Its seed is
`SHA-256(presentation_seed_bytes || NUL ||
"electry-listener-bootstrap/v1")` and is written into the private key before
listening. For each replicate and each stratum, it draws 15 whole listener IDs
with replacement and retains all ten core values for every sampled listener.
Draw slot `s` uses the first unsigned big-endian 64 bits of
`SHA-256(bootstrap_seed_bytes || NUL || replicate || NUL || stratum || NUL || s
|| NUL || retry)`; values at or above the largest multiple of 15 below `2^64`
are rehashed with the next retry, and the accepted value modulo 15 selects the
listener. The same sampled people feed identification and preference. Each draw
averages listeners within each fixed stratum, then gives the two stratum means
50/50 weight. After sorting the 20,000 values, R-7 linear interpolation
(`h = (N - 1) p`) gives the identification 5th/95th percentiles and the
preference 5th percentile used as its one-sided 95% lower bound. Those intervals
generalize to that equal listener mixture for this frozen stimulus set; with
two source clusters they do not establish a population claim over guitars or
players.

After exactly one complete response file for every frozen ID has arrived, run:

```sh
cd /private/electry-blind-pack
python3 score.py \
  /private/electry-blind-pack/private/answer-key.json \
  /private/electry-blind-responses \
  /private/electry-blind-score.json \
  --expected-fingerprint <externally-recorded-64-hex-fingerprint>
```

The scorer rejects a changed evidence archive, implementation, public session
or audio file; a changed fingerprint, mapping commitment, trial order or ID;
missing/extra/duplicate participants; malformed choices; incomplete trials;
and play/replay-count contradictions. Its auditable JSON records hashes and
aggregate indices for the frozen pack, answer key and all 30 response files;
all core/cell/articulation, repeat, side-bias and bootstrap endpoints; every
gate boolean; and one combined `gates.all`. That boolean evaluates a future
completed cohort only. The current repository has no response cohort and
therefore has no listening result.

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

# Electry evaluation contract

This is the measurement side of Electry's realism work. It keeps three things
separate: a deterministic model render, recordings made by real players, and
the listening claim that one is convincing as the other. A better numeric fit
to one phrase is useful evidence; it is not a market-leading claim by itself.

## Current product surface

The unreleased plug-in currently exposes 28 host parameters; development
snapshots have no backward-compatibility contract. One **Guitar Build**
parameter replaces six separate construction axes. It moves Body Wood through
the separately documented walnut-to-ash pair while visiting six curated
short/heavy through extended/light scale/gauge setups; the fitted extended
heavy Drop-E build at 0.8 is the default. Size, Shape and Construction stay
neutral because no independent reusable law passed the evidence gate. Pickup
selector and type, Tone, Body Resonance amount, string age and all
player/contact controls remain independent. The former four-mode construction
trajectory is available only in an explicit legacy-comparison build.
**Output Mode** is one three-choice Mono/Stereo/Double parameter; Double means
two separately seeded complete engines, one mono performance per channel. The
second player's picked wrist strokes also carry a deterministic 0-6 ms causal
timing offset; the primary player and fretting-hand articulations keep their
established clocks.

**Amp Voice** selects American Clean, British Crunch or Modern High-Gain as a
complete amplifier, output-dynamics, transformer and speaker/cabinet path. It
defaults to Modern, is appended after the first 27 host indices, and a
development state missing the field is explicitly migrated to Modern.

Within each engine, pick position offset, force, angle and contact width are
drawn once per physical wrist stroke and shared by every crossed string; each
string still maps that common gesture through its own gauge, pitch and contact
mechanics.

The editor names the bridge-hand style **Mute** and its two controls **Mute
Tightness** and **Mute Pressure**. This document retains “palm mute” where it
names the physical technique, public-corpus annotation or frozen evaluator
filename.

The wet path is outside the dry probes below. Its Distortion module solves
the 2.2 kOhm / 10 nF antiparallel Shockley-diode RC node from
[Yeh, Abel and Smith](https://dafx.de/paper-archive/2007/Papers/p197.pdf), and
all three amplifier paths interpolate a dense transfer generated during
preparation by solving
[Dempwolf and Zölzer's measured 12AX7 current model](https://dafx.de/paper-archive/2011/Papers/76_e.pdf)
against a 250 V / 100 kOhm plate load with a residual-checked solver. American
and British also evaluate [Yeh and Smith's exact third-order passive tone-stack
transfer](https://dafx.de/paper-archive/2006/papers/p_001.pdf) for two component
families. Before their output stages, American solves TubeLib's measured-fitted
ECC81/12AT7 `TriodeK` family in the 410 V, 82/100 kOhm, 470 Ohm and 22 kOhm-tail
[Fender Twin Reverb AB763 component family](https://schematicheaven.net/fenderamps/twin_reverb_ab763_schem.pdf),
while British solves its distinct ECC83/12AX7 family at the 400 V rail used by
[Macak and Schimmel's Fig. 11 model](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf), in the 82/100
kOhm, 470 Ohm and 10 kOhm/presence-tail [Marshall Super Lead family](https://www.prowessamplifiers.com/schematics/Marshall/1959_superlead.pdf).
The driven grids include their 1 nF × 1 MOhm and 22 nF × 1 MOhm input coupling;
both nonlinear dual-plate solves include finite-tail behaviour and unequal arms.
Their outputs remain separate through the source circuits: American uses 100 nF
capacitors, 220 kOhm bias returns and 1.5 kOhm power-grid stoppers; British uses
22 nF, 220 kOhm and 5.6 kOhm. TubeLib's generic 2 kOhm plus `IS=1 nA`,
`RS=1 Ohm` diode branch conducts at each terminal. A residual-bounded three-unknown
Newton step jointly solves total LTP current and both grid voltages while the
two capacitors are trapezoidally integrated, retaining independent plate load,
common bias displacement and blocking recovery.

The output stages directly translate Reefman's measured-curve-fitted
[uTracer TubeLib](https://www.dos4ever.com/uTracer3/TubeLib.inc) 6L6GC
beam-tetrode and EL34 true-pentode equations, solve an ideal balanced pair
against one quarter of the source-documented plate-to-plate load, and retain
output plus plate/screen demand over both terminal-grid coordinates and rail
voltage. Around the retained RCA 450 V plate / 400 V screen / −37 V / 5.6 kOhm
6L6GC pair, American solves the two individual 470 Ohm screen branches drawn by
the AB763; this is a documented-source hybrid rather than a complete Twin
Reverb DC output stage. British uses the common 800 Ohm resistor published with
Mullard's 400 V / −36 V / 3.5 kOhm EL34 pair. Each screen KCL is solved jointly
with the plate load line and baked into the preparation-time tables. The pair
receives common as well as differential drive. A positive
terminal can load its PI through the diode, but the measured-fitted power-current
surface is clamped at `Vg=0`; Electry therefore claims an AB1-bounded power
transfer with overload-grid conduction, not an unmeasured ideal AB2 source.
Current demand drives model-specific sag; negative feedback, transformer state
and six-biquad speaker/cabinet voices stay inside the oversampled path. Modern
preserves the previous Electry arithmetic. Exact claims stop at the diode node,
passive RC transfer, translated measured-current formulae and numerically
residual-bounded coupled/load-line solves. These are tube-family fits, not
universal specimens. TubeLib diode junction capacitance/transit time, PI grid
conduction, bias-supply impedance, the complete presence network,
the American upstream 1 kOhm / 20 uF dynamic screen node, Marshall quartet and
choke supply, a reactive transformer-reflected loudspeaker and full positive-
grid plate-current data remain explicit omissions.
They are not complete named pedal/amp schematics, cabinet impulses, loudspeaker
mechanics, microphones, rooms or capture-verified replicas.

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

### Shipping matched-material modal body

`ELECTRY_MEASURED_BODY_RESPONSE` defaults to `ON` and selects the shipping
three-mode body path. `OFF` retains the former four-mode geometry-informed
voicing only as an explicit legacy comparator:

```bash
cmake -S . -B build-measured-body -DCMAKE_BUILD_TYPE=Release \
  -DELECTRY_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON \
  -DELECTRY_MEASURED_BODY_RESPONSE=ON
cmake --build build-measured-body --parallel
ctest --test-dir build-measured-body --output-on-failure
```

[Ray, Kaljun and Straže](https://pmc.ncbi.nlm.nih.gov/articles/PMC8465587/)
measured matched instruments under laboratory support conditions. Table 3 and
the pickup-domain damping results
give three paired body-pickup (`BP`) poles:

| mode identity | walnut frequency / tan-delta | ash frequency / tan-delta |
| --- | --- | --- |
| 1 | 108.2 Hz / 0.119 | 119.0 Hz / 0.114 |
| 2 | 200.5 Hz / 0.073 | 204.7 Hz / 0.072 |
| 3 | 420.6 Hz / 0.046 | 440.4 Hz / 0.026 |

The shipping model pairs each pole only with its measured mate. For
`w = Body Wood` it uses positive-domain interpolation

`f = exp(lerp(log(f_walnut), log(f_ash), w))`

and the same equation for tan-delta, with `Q = 1 / tan-delta`. The three direct
resonator levels `[1.00, 0.68, 0.46]` are quiet voicing because Ray did not
publish signed or absolute residues. The old candidate incorrectly converted
the three paired-mode ratios into a smooth frequency law and applied it to six
unrelated Paté poles. That transfer is removed.

Ray found no significant material difference in open-string fundamental decay;
differences began at E2 harmonic 2 and A2 harmonic 3. It also supplies no
complex fret-specific mobility. The shipping model therefore adds no
termination loss: `bodyConductance = 0` and `bodyLossFactor = 1` exactly. Body Size, Shape
and Construction are exact no-ops in this modal model. The shipping
Guitar Build keeps `Wood` monotonic at its six `0.0, 0.2, ..., 1.0` anchors and
holds the three inactive axes at `0.5`. Its Scale anchors are
`[0, .2, .4, .6, .85, 1]`; Gauge is `[1, .8, .6, .4, 1, 0]`. These are curated
short/heavy through extended/light instrument setups, not a material law. The
0.8 default exactly preserves the fitted 27.63-inch heavy-set string setup,
while the wider path does not relabel an invented shape or joint response.

Across the six sampled Build positions, every adjacent E1/A2 step has a
normalised residual of at least `0.0223`/`0.0276`, and the endpoint residuals
are `0.8291` on E1 and `0.1443` on A2. Whole-render level spread stays below
`0.49 dB`, so the new contrast is not a loudness shortcut. Direct-path levels
are pinned separately rather than inferred from these phase-sensitive Build
residuals: at 48 kHz the A2 direct-body/complete-pickup ratio is `-32.15 dB`,
and modal-neighbourhood sweep maxima are `-34.68 dB` during attack and
`-27.99 dB` settled.
The previous candidate path measured only `0.0678` end to end on A2 and began
with adjacent changes of `0.0095-0.0188`, matching the user's report that the
range was insufficient.
One body per species, unequal body mass, elastic support, mode-dependent sensor
results and unavailable raw data prevent a universal tonewood claim.

A sealed, whole-file-RMS-matched comparison then used A = the legacy shipping
path and B = this measured path. The user reported Build “B is a bit cleaner,”
Metal “B but close to tie,” Rock “close to tie,” Blues “B,” and overall “close
to tie but B slightly better with less body.” That by-ear preference promotes B
to the shipping baseline; it does not turn the source data into a general
material law. A post-verdict, level-matched diagnostic found the genre renders
effectively flat by broad spectral band. At Build 0.8, B was about `0.5 dB`
lower below `160 Hz`; changed Build anchors also reduced `2-5 kHz` coloration
by roughly `2-3 dB`. No category preferred A, so B is promoted unchanged.
“Less body” is not treated as authority to raise the unmeasured direct-body
scalar unless later listening identifies it as a defect.

#### Shape evidence boundary and identification gate

No reviewed source supplies a transferable law for Electry's carved
single-cut-to-flat-slab Shape coordinate:

- Paté's Appendix F is the strongest controlled outline comparison found. Its
  four intentionally simplified finite-element guitars keep mahogany material
  constants, neck, fingerboard, joint, total mass, volume and thickness fixed
  while changing body and headstock symmetry. With the headstock held fixed,
  body asymmetry shifts paired modes mostly about 0-4%, with one about 6-7%
  and some higher modes moving downward. The study explicitly uses the simple
  models to exaggerate trends and draw qualitative conclusions; it predicts a
  modest, mode-dependent *symmetry* effect, not this carved/slab axis, and
  supplies no measured damping or complex residues.
- [Ray's dissertation](https://dk.um.si/IzpisGradiva.php?id=80688), sections
  7.2.1 and 8.9.3, compares an ash laboratory block with an ash classical-body
  finite-element model. It changes dimensions as well as outline, models the
  body alone fixed at the neck interface, and does not validate the classical
  model on an assembled instrument. Its ordered modal-frequency ratios of
  `0.350114-0.640572` therefore combine size, mass distribution, outline and
  possible mode crossings; importing them would be a large false Shape law.
- [Russell's production-guitar comparison](https://www.acs.psu.edu/drussell/guitars/guitars-ASA.pdf)
  contrasts a Coronet, Explorer and semi-hollow ES-335. Their materials,
  construction and hardware co-vary, so the result cannot isolate outline.
  Paté's more controlled neck-joint population likewise found within-group
  variation comparable to between-group variation rather than a deterministic
  Construction shift.

An independent law therefore needs matched physical capture, not another
scalar retune. The first study should identify exactly the current Shape axis,
conditional on one material and joint:

1. CNC two independent matched trios at `s = 0, 0.5, 1`: carved single-cut,
   half-carved and flat single-cut. Randomise shape assignment within each
   conditioned, grain-matched material lot. Keep the silhouette,
   bridge-to-neck geometry, cavities, total wood volume, centre of mass and
   principal inertias matched by CAD; measure the final 3-D geometry, mass,
   moisture, density and dynamic-modulus coupons rather than correcting a poor
   match with resonant ballast. Trio one is fit data and trio two stays blind as
   a physical holdout.
2. Transfer one neck, bridge, nut, tuners, pickup/harness, cable and electrical
   load between bodies. Use locating features, threaded inserts, a fixed bolt
   sequence, torque-controlled fasteners and preconditioned matched strings.
   Fully disassemble and reassemble every body twice in random order, giving 12
   assembly sessions that expose joint reseating variance.
3. Suspend the assembled instrument on elastic straps with support modes below
   20 Hz and damp the tuned strings for structural tests. With a calibrated
   force hammer, scan 24 permanently indexed bridge, pocket, body, nut,
   fret-line and headstock points while recording a bridge-reference
   accelerometer or laser vibrometer and the pickup through a measured 1 MOhm
   input. Retain complex force-normalised response from 20-1000 Hz, not just
   peak frequencies.
4. At representative bass-open, middle-fret-12 and treble-fret-12 string paths,
   impact bridge and neck terminations in turn. From the signed two-port
   mobility matrix, calculate the conductance actually seen by a string:

   `Y_string = Y_bb + Y_nn - Y_bn - Y_nb`.

   Simultaneous `V_DI/F_bridge` and `V_DI/F_neck` measurements supply the
   complex vibro-electric residues that the current model lacks and
   therefore replaces with voicing.
   If the three termination curves do not collapse to a frequency-only law,
   add a position map rather than averaging away the disagreement.
5. Remove the damping and use a fixed-displacement wire-break plucker at
   `x/L = 0.18`. Capture five untouched high-impedance DI and bridge-motion
   takes for MIDI 35, 45, 55, 68, 71 and 77; these span the model's six
   structural neighbourhoods. Extract partial decay as a held-out check on the
   force-response identification.
6. Fit stable common poles and complex residues jointly across the force
   responses, then track modes between bodies with the 24-point modal assurance
   criterion rather than sorted frequency. Fit endpoints on trio one only;
   require the withheld midpoint and all of trio two to validate the proposed
   one-dimensional interpolation. Archive raw channels, calibration, CAD and
   sensor-position hashes, force signs, torque, environment and event timing.

Minimum analysis gates are coherence at least 0.95 and response SNR at least
30 dB across every retained modal band, reciprocal mobility within 1 dB and 10
degrees, mode-shape MAC at least 0.90, and a held-out shape effect larger than
three times pooled blank/reassembly deviation. The synthesised mobility must
remain passive. A frequency, Q or residue that misses those gates remains
shape-independent; passing poles must also predict the sign of decay-rate
change on all six validation notes before blind listening.

That study does not create a universal material-by-shape law. Such a claim
requires repeating the complete trio design across multiple independent lots
of each material and withholding entire bodies while fitting main effects and
interactions. The shipping Guitar Build holds Shape exactly neutral, so its
listening comparison says nothing about that axis. The legacy comparator's
co-moving material, shape, construction, scale and gauge likewise cannot
identify Shape independently.

Until that gate is met, Body Wood remains only the narrow three-mode Ray
specimen morph above. Shape, Construction and Size remain inactive in the
shipping model.

[Paté, Le Carrou and Fabre](https://www.lam.jussieu.fr/Membres/LeCarrou/Articles/A8_Pate_PredictingDecayTime.pdf)
confirm why loading is excluded: their decay equation needs the conductance at
each string partial and fret. They measured bridge conductance only up to
`0.00176` m/(N s), selected fret/neck resonances around `0.018` m/(N s), and a
full neck maximum of `0.102` m/(N s), but did not publish transferable complex
spatial residues. A two-anchor or broadband scalar would not reproduce that
partial- and position-dependent termination.

[Elliott, Magill and Kendrick](https://salford-repository.worktribe.com/OutputFile/1490211)
found the direct body-borne pickup contribution generally 30-50 dB below the
complete voltage, apart from a local feature near 700 Hz. The regression keeps
the Ray bank in that quiet end-to-end region through Electry's coils and output
path, sweeps all three modal neighbourhoods through every pickup selector and
both material endpoints, and pins exact zero bypass. These checks establish
implementation fidelity and boundedness, not perceptual superiority. Promotion
still requires identical-render, whole-file-level-matched blind listening
across material-focused, metal, rock and blues renders.

### Pre-source rejection of the distributed Palm-junction experiment

A reversible 2026-09-01 candidate replaced only the bridge-hand part of the
shipping Palm loss with three passive, memoryless junctions on the folded
waveguide. The junctions used fixed `1/4, 1/2, 1/4` weights around a
Mute-Tightness-derived centre, with finite widths `1.5, 3, 4.5, 6, 7.5 mm`;
a collocated point-rate grid and the shipping lumped high-tilt grid were the
equal-complexity controls. The stored-sign scatter itself was reciprocal and
contractive, its rate exponents were internally consistent, and the macro-off
engine remained byte-identical. The candidate-only patch is preserved outside
the repository at
`/private/tmp/electry-egipt-palm-20260901/CANDIDATE.patch`, SHA-256
`bf519b8675b3ebf078c15825608d264696065aeccd3d346abf406daa92443af9`.

The candidate was rejected before any corpus payload or stage-0 model render.
In an unbound local production-suite run at 4.5 mm, the candidate reported
eleven substantive failures; that run predates the immutable plan/self-test
receipts below and is recorded as a local observation rather than sealed
benchmark evidence:
four F2 early-upper-contraction rails across sample rates, three above-500 Hz
loss/attack-darkness rails, two contextual body/decay rails, one shared-hand
Palm darkening rail and a `+12.5 cent` strong-contact pitch shift against the
existing six-cent limit. Inspection found a direct cause for the pitch failure:
the interior junctions were configured after the compensated period had been
solved, so their reflected-wave pole phase never entered pitch compensation.
The integer snapped contact coordinate also omitted the fractional seam and
commuted damping/dispersion phase; those omissions are consistent with, but do
not alone prove the cause of, the spectral and decay failures. Palm -> Open ->
Palm on one ringing voice could additionally reuse a stale relaxation peak.

The source-free benchmark plan and final synthetic self-test remain at
`/private/tmp/electry-egipt-palm-20260901/PLAN.json` and
`SELFTEST-FINAL.json`, SHA-256
`4d8dfdcb53f5852e7f58f9c04b323ec14ce744973cbe0f1f5fca8db9d466d38f`
and
`63e21d1826fd84a63ea4b99c78985bee3f34ad8c122c01bb83a089b73489a920`.
An independent fail-closed audit also stopped that harness: its unfinished
stage-6 producer hard-coded six required engineering checks to false, while
several receipt/rename trust boundaries could still admit replayed evidence.
Stage 0 therefore would only have restated build provenance for a mechanism
that could not ship. No high-gain processor ran, no selected-member payload
request was made, no selected WAV payload was opened, no real or model
benchmark waveform was decoded, rendered or auditioned, and the EG-IPT
selection and holdout remain untouched.
The four-file candidate patch was reversed exactly; shipping Palm is unchanged.

#### Pre-source rejection of the point-contact modal comb

The proposed follow-up was implemented as a second reversible, default-off
experiment and also rejected before source access.  It removed only the
shipping Palm differential high-frequency loss and hand dip.  The existing
`handRate / 22` low-mode relief remained as equal fundamental/high-target loss;
the replacement was one passive Mach-Zehnder loss cell at the declared hand
coordinate.  For hand strength `R`, its static loop transfer was

`H(z) = (1 - a) + a z^-D`, with `a = (1 - R) / 2`,

and the implementation split and recombined with
`c = sqrt(1-a)`, `s = sqrt(a)`.  Storing `s[n] * x[n]` in the delay arm made the
cell exactly energy-accounting even while pressure changed:
`y^2 + z_discarded^2 + E_next = x^2 + E_previous`.  Its exact fundamental
phase was added to the existing compensated-period solve, contact distance was
converted from the unbent open-string wave speed, and lift, replant, retap,
repick and low-sample-rate lifecycle cases had explicit candidate tests.

Those implementation invariants passed, but the complete Release engine suite
reported sixteen failures in 41.38 seconds.  Twelve were expected legacy
mechanism assertions about the removed hand dip or one-pole and did not decide
the result.  Four unchanged audible/output rails did:

- the late Palm note failed the `< 0.25 x Open` articulation-decay bound;
- at least one pick direction failed the `< 0.30 x Open` Palm-decay bound;
- Open/half/full pressure late RMS was `0.008855 / 0.005219 / 0.000854`, so
  Open to half missed the required factor-of-two contraction; and
- E2 extra loss above 500 Hz was only `7.72601 dB`, below the `> 10 dB`
  selective-loss rail (its absolute upper loss also missed the existing
  `18 dB` floor).

The failure pattern is explained by the fixed-coordinate, one-delay topology
rather than a nearby value of its one strength coefficient.  At harmonic
`n`, the cell retains
`|H_n|^2 = 1 - (1 - R^2) sin^2(n pi x/L)`.  A near-bridge point therefore
couples only weakly to the late low modes and leaves exact unity nodes for all
`R`.  At the default coordinate, the E2 `> 500 Hz` band begins near H7, where
the overlap is still small; the E1 band begins near H13 and happened to lose
more energy.  Fitting `a` at the shipping 3.6 kHz anchor could not remove the
nodes and would have abandoned the declared spatial hypothesis, so no constant
was tuned and no output gate was waived.

The failed candidate executable and full log remain at
`build-palm-modal-comb-release/ElectryEngineTests` and
`build-palm-modal-comb-release/Testing/Temporary/LastTest.log`, with SHA-256
`0732131fab78b9ebb30bf643a8e383124dcecc658ff15e54ea6f505174c2cc8c`
and
`03639cef913294a9f95d5edbcd40174419c5b97e3168ebb3e465a168393c3eca`.
The macro-off Release executable was byte-identical before and after reversal,
SHA-256
`31e6e0c4b4a009bedb176725d365bbf082a78bf6a457b86eb126acd9a61b0dc1`,
and passed the complete suite in 45.24 seconds.  Exact pre-candidate hashes were
restored for CMake, both engine files and the engine tests; no candidate flag,
state, manifest field or test remains in source.  No selected WAV, corpus
payload, high-gain processor or real/model benchmark render was opened or run.

The literature supports a distributed Palm force but does not publish a
transferable heel footprint or pressure profile.  A future bounded experiment
must therefore either measure and freeze that geometry independently, or label
any width/profile search as TRAIN-only model identification with untouched
player/guitar holdout.  It must preserve the measured broadband Palm decay and
use a passive, phase-solvable finite-contact topology. This result rules out
the tested fixed-coordinate, one-delay replacement under the unchanged output
rails; it does not rule out every possible spatial contact model.

### Source-inconclusive EG-IPT artificial-harmonic coordinate audit

The shipping Pinch operator excites the string at one plectrum coordinate and
then applies the touch at that same coordinate. On an ideal string this couples
the mode shapes as `sin(n pi p) cos(n pi p) = 0.5 sin(2 n pi p)`. A reversible
2026-09-01 candidate kept the plectrum fixed and moved only the touch by one
shared signed distance in metres before fret stretch. Its equal-complexity
control moved the co-located plectrum and touch together by the same distance.
Both reused the existing event-time modal table and coordinate path: no voice
state, callback loop work, control, solver or dependency was added. Spatial
touch models motivate testing separated contacts, but the fitted distance was
an Electry hypothesis, not a claimed anatomical measurement. EG-IPT calls the
articulation `art-harmonic`; it does not establish that every take is a
thumb-produced pinch harmonic, so even a successful result would support only
an artificial-harmonic spectral operator on this corpus.

The lawful source was the CC BY 4.0
[EG-IPT archive](https://zenodo.org/records/15205644) and its
[NIME paper](https://nime.org/proceedings/2025/nime2025_14.pdf): one
professional player, a Gibson SG, simultaneous 96 kHz/24-bit DI and amplified
channels, including the EVH 5150 III/Mesa V30/SM57 dynamic-microphone path.
ZIP central-directory metadata fixed 60 complete cells across bridge/neck
humbuckers, strings 6/5/4 and identifiers 04--22, with paired `ordinario` and
`art-harmonic`, DI and dynamic-microphone members. Identifiers 06, 10, 14, 18
and 22 formed the 30-cell selection surface; 04, 08, 12, 16 and 20 were sealed
holdout. The Zenodo record reports the 23,757,983,313-byte archive MD5 as
`48a5135adfd090515ff0af7dc5c3c32f`; that whole-file MD5 was not locally
recomputed. Every selected range instead had to pass its central-directory
CRC and receive a payload SHA-256 in the source receipt.

The complete external protocol was frozen before any selected payload. Its
PLAN SHA-256 is
`2c8e5984fe419391005f0f7c67b4094ebf92e921be55fd3402f03a3f545b2d91`
and its v2 pre-decode intake is
`139b018c5d320b621b0a40e92e118b9e8174d1c93000f2814256b9aa9dfdf507`.
The intake binds the extractor, source-pitch analyzer, inferential scorer,
engineering gate and non-vetoing physical-microphone reporter at SHA-256
`da8695fca539256bb70bc0e8ec2efe8a4c7b9f1e65d9972cccc88403f349e0e0`,
`5ebb1f08faab1c649363c12e041ec36814fd8660a465f3bc1bae4e937618f8f3`,
`d2d09a2e4e700c740167ddce70b3649517861b963d07d5f58137e81411fcc1f7`,
`de14ac6f00cebafac251c701243dae0cc720eafe59e10f8ee51a283d72d9d0e1`
and `9fba0871ee480b09ede41cc1bc1366e0ceebe337824584eb8e279f25e3a4b828`.
All frozen files were made read-only; renderer/engineering manifests, exact
zero-delta audio identity, an independently reproduced high-gain executable,
broad-search YIN octave/stiff-string cases, metric identity/non-identity,
failure builders and the engineering coordinate oracle passed using synthetic
audio before extraction.

Had source intake passed, ordinary-only nuisance selection would have chosen
one pick fraction from 0.05 through 0.50. Raw selection would then have compared
touch-only separation against the co-moving control on the fixed
`-20..+20 mm` grid in 2 mm steps, using two-window H2--H16 normalized-contour
RMSE, source-only measured ordinary-DI F0 for real signals and equal-tempered F0
for model signals. Frozen raw, shared-high-gain, per-pickup/string, p90,
per-cell, secondary and leave-one-pickup-out gates all had to advance before
the untouched holdout and candidate-aware engineering proof. Only after those
decisions could the paired dynamic-microphone surface report one descriptive
direction; it could neither refit nor veto. A finite exact-zero shipping error
was preregistered as a valid perfect baseline with an undefined JSON-null
relative improvement and a failed positive-improvement gate, not a division
error.

Stage A made 124 byte-range requests and admitted all 30 selection
`ordinario` DI members after 104,404,563 received bytes. The immutable source
receipt SHA-256 is
`a86e9df42e71086dd48dd3ace12e14d0481dc178d9f1cb526150a01dcf4e200a`.
The first authorized source-only decode then stopped at
`HB-neck_ordinario_02-06_5s_DI.wav`. The frozen centred-window detector placed
its onset at frame 1872 at 96 kHz, or 19.5 ms; the contract required at least
1920 frames/20 ms of prefix. It missed by 48 frames (0.5 ms). The terminal
`source_abort_inconclusive` artifact has SHA-256
`d991777cf1912b148de532b990603114a5f7d035ba353a7a916c48f95aa095d5`
and is bound to both PLAN and source receipt. No recording was auditioned.

No pitch unseal exists, so the extractor remains unable to open selection
`art-harmonic`, either holdout half or the dynamic-microphone surface. No real/
model partial spectrum, nuisance coefficient, candidate class/distance,
high-gain comparison, transfer score, engineering candidate or A/B was
computed. Relaxing 20 ms after seeing 19.5 ms, shifting the onset, or dropping
the cell would invalidate the pre-registration. The correct conclusion is
therefore source-inconclusive: EG-IPT did not falsify the fixed-metre mechanism,
but it supplied no admissible evidence to implement it. The source branch is
absent and shipping remains the co-located law.

### Historical CC0 Modern-cabinet audit

[Jester Dyne's Brutal IR Pack](https://www.jester-dyne-productions.com/brutal-ir-pack/)
supplies a lawful real 4x12 reference: its bundled handbook dedicates the pack
under [CC0](https://creativecommons.org/publicdomain/zero/1.0/). The audited
anchor is the 48 kHz, 24-bit mono `14_Cathode_Ray_Fleshburn.wav`, an SM57/V30
capture. Its SHA-256 is
`420280d44a6cb969d0599aa88f7bc733e13d39cdd051acf8b0eda1d82286ba5f`;
the source ZIP's is
`299dc053f01ebd1e980459adc48f9c6b8a8c7af91917b4f946512eefdbb311ea`.
Neither full file nor the handbook PDF is committed. Only the exact packed
1,024-sample causal prefix is vendored in `Source/DSP/ModernCabinetIR.h`; its
signed-24-bit little-endian PCM SHA-256 is
`a9b7e39f38c38d820ff9b758577293b6d4cb5de03bb89090ef0763a7eb357d45`.

At that checkpoint, the then-sole six-section cabinet—now the Modern
High-Gain voice—and the IR were normalized over an equal-log
70 Hz-8 kHz grid and compared after identical log-frequency smoothing. Their
broad-magnitude RMSE is 4.40 dB. The largest useful directions are less output
below the box and a shallower 430-470 Hz cut; the existing upper roll-off is
already close. Fitting the same six sections to magnitude alone reduced that
RMSE to 1.59 dB, but the candidate was rejected in the complete instrument:
the then-current rapid-Palm 30-80 ms upper-body share fell from 7.2913% to 3.0834%, the
cabinet lost the integrated chain's low-mid-thump and presence rails, and the
muted amp-plus-compressor level rose to +12.69 dB. A closer isolated magnitude
curve is therefore not evidence of a better amplifier.

The measured IR reaches its first peak at sample 7 (0.146 ms), first crosses
99% cumulative energy at sample 451 inclusive (452 samples, 9.42 ms), and its
causal first 1,024 samples retain 99.636% of its energy with 0.495 dB smoothed
response error against the full file over 80 Hz-8 kHz. That makes one fixed,
phase-preserved cabinet a defensible candidate to test without another user
control.

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

That direction was tested on 2026-09-01 with exact bridge-pickup `ordinario`
and `muted` same-performance pairs: `_DI.wav` drove Electry and the simultaneous
dynamic-microphone `_dyn.wav` was the target. The first 48,000 frames at 96 kHz
were evaluated at DI peaks of -18, -12, -6 and -3 dBFS through fixed Distortion
0.45, Amp 0.95, Modern High-Gain and Compressor 0.60 settings. The primary
measure was normalized 36-band equal-log spectral-shape RMSE over 80 Hz-8 kHz
in 0-30, 30-80, 80-250 and 250-500 ms windows. The secondary measure compared
the last three windows' relative RMS against the first. Before either candidate
render was inspected, the frozen gate required at least 10% primary-median
improvement, improvement in each pair median, no primary cell more than 0.25 dB
worse, and no worse secondary median or secondary cell by more than 0.5 dB.

The external frozen artifacts are
`/private/tmp/electry-fx-paired-20260901/PLAN.json` (SHA-256
`7f1b86824841831c259ca94fb1dfc566616438a1b5298865e4e892121a38036b`),
`/private/tmp/electry-fx-paired-20260901/analyze.py` (SHA-256
`d52547c21cdacca2fdcd76ceca8fca0e9ddccfb73911b135a3be287d524d730f`)
and `/private/tmp/electry-fx-paired-20260901/RESULT.json` (deterministic-rerun
SHA-256
`1aad021e713a4d027900de488a0f332133d9f57ea0b23f6bf2148f2e677f24cc`).
They are not committed and their absolute paths identify the audit workspace,
not durable repository inputs.

The shipping filters' primary median/max errors were 17.418264/19.285118 dB;
the phase-preserved IR reached 16.255076/17.830980 dB. It improved all eight
pair/gain cells and both pair medians, but its 6.677982% median improvement
missed the frozen 10% gate. Secondary relative-envelope median/max error fell
from 6.247677/12.160929 to 4.965523/9.206289 dB, but every `ordinario` gain cell
worsened by 0.78-1.09 dB, beyond the 0.5 dB rail, while every `muted` cell
improved. The real target relative-RMS trajectories were
`[+7.9644, +7.5412, +6.6098]` dB for `ordinario` and
`[+7.1724, +3.5184, -26.5588]` dB for `muted`.

This is only a whole-chain direction check: its Gibson SG humbuckers, named
6L6 amplifier, Mesa/V30 cabinet, microphone and undocumented gain setting do
not identify Electry's cabinet in isolation. The candidate therefore failed
the preregistered objective gates, no A/B was licensed, and
`ELECTRY_MEASURED_MODERN_CABINET` remains default-off.

The cabinet remains inside the 8x nonlinear domain. At a 48 kHz host the
1,024 source coefficients therefore prepare to 8,192 coefficients at 384 kHz;
a correct direct stereo FIR would cost about 6.29 billion multiply-accumulates
per second, while one 8,192-sample FFT block would add 21.3 ms. The default-off
`ELECTRY_MEASURED_MODERN_CABINET` candidate instead evaluates taps 0-63
directly, taps 64-511 in seven FFT-128 partitions and the rest in up to fifteen
FFT-1024 partitions. The tier offsets pay their own block schedules, so it adds
no algorithmic latency. The shipping zero-latency filters stay unchanged; the
paired-recording audit above rejected the measured candidate before listening.

#### Rejected Modern measured-6L6 output-stage ablation

A second single-factor audit on 2026-09-01 kept the shipping cabinet and
controls fixed. After Modern's second triode, the default-off branch reused the
existing measured 6L6-family `powerTubePairLookup()` at zero common drive. It
passed the legacy droop expression as `railScale`, drove the existing
transformer from the lookup output and fed plate-plus-screen `supplyDemand`
into Modern's unchanged attack/release sag follower. It added no solver, state,
constant or dependency.

The protocol was frozen before candidate compilation or audio at
`/private/tmp/electry-modern-6l6-20260901/PLAN.json` (SHA-256
`c339c132a0d69856d2c6502edd4b0a20ac69d4ec6f50d5f6dd3eaf73c0c26f17`).
Its analyzer is
`/private/tmp/electry-modern-6l6-20260901/analyze_paired.py` (SHA-256
`e49aa5d5a487b9d405c7cacfc37dcb218daabbf3e2aec5bbf1a75c96fbcb2a24`),
its result is
`/private/tmp/electry-modern-6l6-20260901/PAIRED_RESULT.json` (SHA-256
`f1eb99929666d28fcf9c8fc3fc2909526947d99ade3891a5820acd3a66101924`),
and the candidate processor's SHA-256 is
`5ddd20d8f95ffb2a962b08de0ced2be876706fb0d60c4f1e705d08ed123efd0b`.
These external artifacts are not committed repository inputs.

Against the same eight EG-IPT pair/gain cells, median envelope error worsened
from 6.2476765 to 8.3466921 dB: `-33.5967%` relative improvement, versus the
required `+10%`. Both the each-pair-median and 0.5 dB per-cell envelope rails
failed. Median spectral error worsened from 17.4182643 to 18.1519104 dB and
failed both its no-worse median and 0.25 dB cell rails. Median crest error
worsened from 3.1850492 to 3.4549171 dB and failed both its no-worse median and
0.5 dB cell rails. Candidate-minus-baseline cell RMS ranged from -6.9449 to
-5.7324 dB, far outside the frozen 1 dB level rail. Output finiteness and OFF
audio identity passed.

The EVH/Mesa microphone target remains a direction-only check of a different
named chain, not evidence that could by itself promote an Electry model. The
objective rejection stopped the experiment before CPU, A/B or full test-matrix
work. The temporary flag and branch were removed; the shipping Modern path is
unchanged.

#### Aborted Guitar-TECHS fixed-metre pinch-offset audit

The shipping Pinch touch is co-located with the pick. On an ideal string that
couples the pluck and touch mode shapes into
`sin(n pi p) cos(n pi p) = 0.5 sin(2 n pi p)`. A 2026-09-01 ablation separated
the following thumb from the pick by one signed physical distance, added in the
open-string coordinate before fret stretch. It reused the existing touch and
pick geometry and added no state, sample-loop work, control or dependency. The
hypothesis was motivated by spatial touch/collision models, but the fixed
distance itself was an Electry hypothesis rather than a published anatomical
measurement.

The lawful source was the CC BY 4.0 [Guitar-TECHS release](https://zenodo.org/records/14963133)
and [paper](https://doi.org/10.1109/ICASSP49660.2025.10887996). Its actual
fixed-slot archive differs from the paper's headline pinch count: P1 contains
72 four-second pinch slots (frets 1-12 on six strings), P2 contains 132 (frets
1-22), and each ordinary file contains open plus frets 1-22 on six strings.
Named-string MIDI was therefore used only for onset phase and QC, never to infer
cell identity. P1 used an Ibanez PF300, `.011-.050` flatwounds and bridge pickup;
P2 used an EVH Wolfgang Exotic, `.009-.042` nickel strings and neck pickup.
Their undocumented amp settings cannot identify a metal high-gain target.

The parent three-wound-string plan was frozen at
`/private/tmp/electry-pinch-offset-20260901/PLAN.json` (SHA-256
`f1c8d6c0545d622d90e4221073799769c01ce34a2e86429fb752293f11f91bcd`).
Before any candidate partial metric, P2 pinch DI exceeded its 0.1% full-slot
clipping ceiling at A frets 16, 17 and 18 and D fret 5: respectively
0.108333%, 0.124479%, 0.113021% and 0.139583%. The complete surface was aborted;
no cell was silently removed. The follow-up low-E-only plan and its alignment
and two-window-secondary clarifications have SHA-256 hashes
`c4ea953f08ba7aef6ca788768fc25dbdab033acc1b9bf5ff87ec6d94c201aafb`,
`e4549d8dfc8aeeafa3c0027329d5bc30181ebd09eb45ab07a7b2d20febd65c16`
and `06b7030a1396867d5b5354e1552de620fb550db4b35c80ad3bb24c75dec0465e`.
Source, renderer and fixed high-gain receipts are
`bc5746095730016e1e7234b346131fbc597511b00b8e17ee739452b26eb7ec64`,
`2549a05b9b8cc52a6c827ec0048ff5908621c7ce38070a4b607ec47f611a9ea1`
and `b58840629dcc046186c03d69f68512f6c6a1c78869890d658decf85d2ce92a94`.

The final two-phase analyzer (SHA-256
`12526ec4928cb08763e86ee3f1843a340302879b5bd9a12017b061f41bc0d54e`;
external freeze receipt
`b634ab6c76d05efbdef11f6a27d64fa732d7cb3d3225d3c3dc333d4b10f9913f`)
stores raw crops without computing spectra until every intake cell passes. It
also stops before odd-fret metrics if the even-fret fit chooses zero or a grid
boundary. Two independent audits of that metric-control flow and its synthetic
branch tests passed.
Its byte-identical repeated result has SHA-256
`bd9370e6b96f3562bae3e89f58c045aae670c96b8588f50914ce765a727c6cdc`.

That result stopped at P1 ordinary low-E fret 1. The frozen alignment selected
frame 191430 (3.988125 s); its preceding 100 ms RMS was
0.005705661772051484 and maximum 5 ms attack RMS was
0.035903514669955956, giving 15.976619 dB against the required 20 dB. The slot
had no clipped frame. Candidate-neutral follow-up showed 99.8136% of the
pre-onset-window energy in its last 25 ms and a second strong transient about
10 ms before the selected onset, so the failure is not mainly the preceding
open-note tail. The frozen no-manual-rejection rule was retained.

A post-decision source-only scan recorded all 68 DI intake cells without calling
the spectral, renderer, effects or candidate paths. Its external JSON SHA-256 is
`9c33fe520fff9d8e3684eb9375affada34451be879637fe0cc26b12e313b8898`.
It found additional independent P1 failures: ordinary frets 3, 5, 9 and 10
also missed 20 dB, while P1 pinch fret 1 had only 8 recorded prefix frames
against the required 960. It also exposed an analyzer limitation before that
count could be interpreted: P2 ordinary MIDI phase `3.9979167 s mod 4` was
treated as almost positive four seconds instead of negative 2.083 ms, shifting
that surface by one slot. Its fifteen nominal P2-ordinary failures are therefore
suspect and are not evidence for rejection. The first P1 failure occurs earlier
in analyzer order and the independent P1 cells already fail the frozen intake,
so this late finding does not change or rescue the decision.

Consequently no real or model partial spectrum, ordinary-only pick-position
fit, thumb-distance selection, high-gain score, amp-mic direction check or
odd-fret holdout metric was opened. This does not falsify a physical
thumb/pick separation; it leaves that mechanism unmeasured by this source. Per
the frozen decision, it cannot justify shipping a coefficient: the temporary
macro and branch were removed and no A/B was prepared. The restored build
passes `Electry.ElectryEngine` and `Electry.RenderEvaluation`; its dry/high-gain
metal benchmark SHA-256 hashes remain
`69e1cd0094c899b7f5640ffc74b71dafd87978809323e01ace237906728b4ecb` and
`ef32b65b6b8e0f7c04a0a18ddbe2196451d5528862d57a109006a582cbabbf64`.

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

#### Exact released-displacement and finite-release-pole ablations

The default-off `ELECTRY_ANALYTIC_RELEASE_IC` experiment replaces only a fresh
plectrum attack's sustained modal injection with the exact cell-centred folded
rail for a finite-width triangular displacement and zero initial velocity. It
keeps the broad edge, scrape, pickup and string loop, and deliberately leaves
ringing repicks on the shipping path. The 4.8 N force/compliance anchor was not
fitted to the files below. An earlier 2.4--10.8 N sweep already showed why gain
is not the missing variable: the E1 60--85 Hz rail first passed near 9.6 N,
after the full-velocity level ceiling had failed near 7.6 N, while the partial
shape errors remained.

Fresh OFF/ON renders with the frozen native-rate envelope core show a useful
but incomplete phase correction. E1 onset-to-peak moves from 25.510 to
4.286 ms, toward the two anonymous 8ridgelite channels' 1.625--2.271 ms, and its
returned/first crest ratio moves from +1.474 to -0.004 dB, versus -1.30 and
-0.35 dB. E2 onset-to-peak barely moves, 12.721 to 13.084 ms; its crest ratio
improves from +2.922 to +1.227 dB but remains opposite the real -0.67 and
-0.38 dB. The normalized 1--2 second window worsens from -5.267 to -6.979 dB
on E1 and from -8.487 to -12.290 dB on E2, versus real E1 -6.76/-4.66 and E2
-6.13/-5.50 dB.

A separate descriptive timbre check used Hann-windowed 0--30 and 30--80 ms
segments, 20--6500 Hz power for the centroid denominator and 500--2000 Hz for
the reported upper share. It is not the frozen comparison protocol. The exact
triangle makes the E2 centroid 728/670 Hz and upper share 80.68/81.02%, versus
shipping 410/401 Hz and 29.67/28.61%; the closest available EGFxSet Clean
Bridge take is 347/338 Hz and 11.93/11.08%. Thus the candidate fixes one E1
timing symptom by making the fresh low-string attack radically too thin and
bright. It also fails 54 current-engine compatibility/calibration checks and
leaves the repeated metal hits unchanged by design. It remains default off.

One smallest-root-cause follow-up asked whether the analytic replacement had
accidentally omitted the shipping excitation's independent finite-release
pole. With its amplitude unchanged and no fundamental makeup, an O(N) trial
applied that existing coefficient once to the complete odd-periodic rail:
for logical history coordinate `q`, which a fixed tap encounters in descending
order, `y(q) = (1-a) g(q) + a y(q+1)`. The recurrence was pre-rolled through
the continuous-period raw rail below float roundoff, never reset at the seam,
and the same shaped tail primed the pickup aperture and coil histories. This is
an exponential release surrogate using an existing calibrated magnitude law,
not a measured plectrum-force trajectory; its causal phase necessarily adds a
particular, unmeasured release velocity.

Both phase ablations were rejected and removed. The future-facing recurrence
raises rather than lowers the audible early centroid: E1 becomes 736/366 Hz
and E2 787/700 Hz; E2 upper share becomes 80.76/82.29%. E1 onset-to-peak falls
back to 24.785 ms. Engine failures increase from the bare analytic candidate's
54 to 67. In the 40-hit metal score, whole-file dry RMS is essentially
unchanged (-36.1143 dBFS OFF versus -36.1161 dBFS with the ablation), while
high-gain RMS falls from -30.2860 to -30.4616 dBFS; the first dry Palm hit's
50--150 ms/onset and 150--500 ms/onset windows move from -1.576/-1.235 dB to
-5.722/-4.470 dB.

The conjugate recurrence, tested separately as return-directed velocity rather
than mislabeled as the same causal filter, also destroys the timing benefit:
E1 onset-to-peak becomes 24.376 ms and its centroid 645/419 Hz; E2 becomes
917/771 Hz with a -13.437 dB 1--2 second window. Its E2 returned/first crest
does improve to +0.511 dB, but remains on the wrong side of both real channels.
Metal dry/high-gain RMS becomes -35.9960/-30.4326 dBFS and its raw peak rises
1.595 dB. Opposite phase changes cannot rescue a magnitude law that does not
identify the actual displacement/velocity pair. A zero-phase magnitude
operator would be a different experiment and still could not repair the
missing absolute E1 bass or same-string repicks. No coefficient was fitted, no
listening choice was requested, no third-party audio is committed, and the
shipping flag-off build remains the product baseline.

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

| nominal source / model note | CC0 paired decay | retired 1.25 / 100 ms | current paired decay | upper-share contraction, CC0 / retired / current |
| --- | --- | --- | --- | --- |
| 50 Hz / G1 | -4.47, -9.43, -14.14, -17.48 dB | -4.89, -9.22, -12.57, -13.79 dB | -3.56, -9.13, -13.76, -15.34 dB | -23.72 / -14.71 / -2.27 dB |
| 75 Hz / D2 | -2.73, -8.24, -13.44, -16.15 dB | -4.48, -8.08, -10.57, -11.15 dB | -3.13, -8.08, -11.90, -12.85 dB | -22.07 / -9.26 / -1.62 dB |
| 100 Hz / G2 | -3.27, -6.46, -8.71, -11.05 dB | -4.93, -7.92, -9.58, -9.33 dB | -3.27, -7.84, -10.94, -11.18 dB | -15.85 / -10.62 / -2.10 dB |

Broadband decay was already close enough that neither model is uniformly
better. The retired contact closed more of the upper-share gap, but did so by
removing too much total low body in the actual instrument. The body-corrected
model keeps the direction of selective contraction but deliberately stops
fitting these unlabelled, preview-derived cells. They remain a warning and not
a hard test rail.

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

| measure | real, four DIs | retired 1.25 / 100 ms | current |
| --- | ---: | ---: | ---: |
| within-phrase envelope correlation | 0.9416 | 0.997303 | 0.997137 |
| 30-80 ms body / onset | -3.6737 dB | -2.7242 dB | -1.1630 dB |
| 80-120 ms tail / onset | -8.4291 dB | -6.7686 dB | -4.8573 dB |
| upper-share contraction | -9.3158 dB | -4.7476 dB | -0.4392 dB |

The retired contact moved this conventional six-string phrase toward the real
decay while making Electry's E1/E2 evaluator and product demos too hollow. The
current model accepts the worse phrase fit and retains more low body. Per-hit
heel-position jitter remains rejected: the public sources cannot identify a
new randomness source. This is a score-matched behavioural probe, not an
eight-string fit target.

### Clean E1 attack/body direction

The clean CC0 phrase now also has the exact model-only 0-30/30-80 ms alarm
applied to both open and muted attacks. RMS uses the raw windows. Spectrum and
autocorrelation use mean-removed windows, with Hann applied only to the
4,096-point spectrum. Centroid and the denominator use
`20 <= f <= 8,000 Hz`, the upper numerator uses strict
`500 < f <= 8,000 Hz`, and harmonicity is unwindowed normalized
autocorrelation over the E1 lag range. Real rows are medians of the two attacks;
model rows are the current raw E1 evaluator probes after the Palm-body
correction.

| output | 30-80 ms RMS vs 0-30 | centroid, 0-30 / 30-80 | upper share, 0-30 / 30-80 | 30-80 harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real CC0 E1 open, median | +2.8801 dB | 234.34 / 256.93 Hz | 6.3775% / 5.7216% | 0.911636 |
| real CC0 E1 “muted string,” median | +0.9272 dB | 420.76 / 309.83 Hz | 17.1698% / 11.5592% | 0.850485 |
| Electry E1 open | -1.2161 dB | 267.44 / 226.98 Hz | 5.3662% / 1.1851% | 0.987064 |
| Electry E1 Palm, medium | -4.1939 dB | 117.67 / 207.29 Hz | 0.8281% / 0.5117% | 0.985315 |

The body correction raises the default Palm body by 1.2669 dB and more than
doubles its early upper share relative to the retired finite heel. The two real
muted candidates still begin brighter than their open neighbours while Electry
begins darker and more periodic. That is a useful local warning, not a
coefficient: the capture chain, mute hand, pressure and two pick forces are
unknown, and the source's generic “muted” label is not a Palm annotation.

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

At that historical baseline, the model's total body contraction is near the two real Palm
cells and its
30-80 ms periodicity lands inside their 0.9944-0.9988 range. The unresolved
mechanism is earlier and selective. Subtracting each ordinary note's own
0-30-to-30-80 upper-share change from its Palm counterpart gives -6.52 dB for
P1 and -14.95 dB for P2, but only -0.93 dB for Electry. Conversely, Electry's
Palm onset is *less* periodic than either real Palm. This supports a short
time-varying tonal/contact mechanism, not a sustained random-noise layer or a
blanket darker excitation.

At that pre-tuning checkpoint, two one-scalar A/Bs were rejected. Reducing Palm's
existing attack-noise multiplier from 1.5 to 1.0 moved E2 onset harmonicity only
0.006 and further
darkened E1. Removing Palm's 0.74 excitation-modal darkener improved F2 onset
harmonicity from 0.715 to 0.802, but weakened the paired selective contraction
from -0.90 to -0.59 dB and opened the muted body. Neither fixes the measured
vector, and combining them with an unmeasured compensating decay constant would
have been curve fitting. Both constants were restored at that checkpoint. The
later body correction does not remove the darkener outright: it moves it from
0.74 to 0.85 while deleting the now-rejected finite heel, as recorded in the
current checkpoint below. Commissioned dry E1/E2 repetitions remain the fit
gate.

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

The current solver-boundary cells move 0-50 ms RMS by only 0.0409 dB on E1 and
0.0548 dB on E2, with a 0.5 dB ceiling. Attack level and absolute
150-500 ms tail are strictly nonincreasing on both strings, as is the
attack-normalized E1 tail. E2 has one negligible +0.00142 dB normalized-tail
rebound from CC2 2 to 3. The older endpoint-tilt figures predate the retired
finite-contact experiment and are not carried forward as current evidence; commissioned
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
same-player Drop-E repetition comparison. Each real attack is aligned at the
25%-of-own-peak crossing above. The model uses its known scheduled contact
frames rather than detecting an onset from its own output. The following 120 ms
of each 2 ms RMS envelope is converted to decibels, mean-centred, and compared
by Pearson correlation; this removes absolute level and asks how closely the
envelope shape repeats.

The model side is the two MIDI-identical bars in
`Docs/audio/04-drop-e-rhythm-dry.wav` (SHA-256
`5988919af56d2ba0f669011709e3468254db8664c9dd783d434918809c5296ed`).
Its exact bar starts are frames 11,025 and 148,601 at 44.1 kHz—the renderer
truncates every scored hold and gap to an integer frame—and the corresponding
E1 Palm Mute score hits are 0, 1, 2, 4, 5, 7, 8, 10, 11 and 13.

| source | corresponding pairs | 120 ms envelope correlation (raw; best within +/-2 ms) | paired 0-30 ms RMS difference |
| --- | ---: | ---: | ---: |
| real CC0 Drop-E E1 muted-string candidates | 1 | 0.742; 0.789 | 2.44 dB |
| current Electry dry E1 bars | 10 | median 0.988, range 0.976-0.993; median 0.989, range 0.977-0.995 | median 0.614 dB, range 0.034-2.720 dB |

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
`4ebe4cecde49d744ddfc9bb820ec66237602cabf0caf8911ead69f9b6ef20228`.
The model rows are medians of their first twelve E1-only hits. Mute windows
start at frame `22,050 + 3,674 n`; Dead windows start at the renderer's actual
frame `81,572 + 3,674 n`. RMS uses the raw signal. Centroid and power share are
mean-removed and Hann-windowed. Centroid and the
power-share denominator use bins with `20 <= f <= 8,000 Hz`; the power-share
numerator uses the strict upper band `500 < f <= 8,000 Hz`. Harmonicity is
unwindowed normalized autocorrelation over the E1 lag range.

| output/context | 30-80 ms RMS vs 0-30 | centroid 0-30 / 30-80 | 30-80 ms power, `500 < f <= 8,000` / `20 <= f <= 8,000` | harmonicity |
| --- | ---: | ---: | ---: | ---: |
| real distorted “muted string” | +0.87 dB | 1,033 / 1,506 Hz | 73.0696% | 0.212544 |
| Electry common-chain Mute | -2.32 dB | 183 / 303 Hz | 7.5623% | 0.897633 |
| real first ghost after Palm | +3.12 dB | 652 / 310 Hz | 9.9994% | 0.786 |
| real second repick | +1.30 dB | 754 / 1,314 Hz | 64.6730% | 0.247 |
| Electry common-chain Dead | -1.73 dB | 196 / 321 Hz | 8.3288% | 0.905118 |

The muted-candidate/model-Mute upper-band-share gap is 65.5073 percentage
points on that exact predicate. It remains a confounded processed-output alarm,
not a dry tuning target.

An independent chain audit locates this gap upstream of `ElectryFx`. For the
last muted E1 of demo 04's first bar (known contact frame 103,622), the 30-100
ms dry body had a 0.0071% upper-band share and 0.9837 harmonicity at the
retired-contact checkpoint. At the later Palm-body checkpoint it measured
0.0686% and 0.9907; the identical frame in demo 05's Modern common chain raised those to
0.8746% and 0.9867. The FX adds
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
correlate 0.240 (0.348 after best alignment within +/-2 ms). The deterministic
model bars remain visibly more repeatable; exact player-distribution scoring is
deferred to the commissioned repetitions rather than re-fitted to this separate
processed performance.

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
7.562262% upper-body power and 0.897633 harmonicity; loose one-sided rails
require more than 6% and less than 0.97 respectively. They reject a materially darker or
more periodic regression while leaving commissioned dry evidence free to
support a different contact model.

### Paired metal render and fitted pickup flux-linkage experiment

`ElectryRenderEvaluation --metal-benchmark [directory]` now freezes a second,
model-only comparison surface. It renders one 40-hit E1/E2 Palm/Dead score at
44.1 kHz and variation seed zero, once through the production engine. The two
mono float32 outputs tap that same pass immediately before `ElectryFx` and
after a fixed Modern high-gain chain. They are not normalized, and the wet
signal is returned to the engine in causal chunks exactly as in the plug-in.
The generated `electry-metal-benchmark/v1` manifest records the score,
parameters, signal path, build-feature flags and output peaks. It does not embed
a source revision or executable digest, so cross-build studies must archive and
hash the renderer separately. The evaluation test renders it twice and requires
byte-identical WAVs and manifests. On one local arm64 Release build with
`ELECTRY_MEASURED_PICKUP_FLUX=OFF`, the observed, non-provenance-bound hashes
are:

```text
pre-FX dry  69e1cd0094c899b7f5640ffc74b71dafd87978809323e01ace237906728b4ecb
post-FX     ef32b65b6b8e0f7c04a0a18ddbe2196451d5528862d57a109006a582cbabbf64
manifest    54b548039edc0a53f9048d42e90db9c106bc567482c1f59676f1f56debe82016
```

This is an A/B transport, not a realism score. In particular, its two outputs
are one synthetic performance at different taps; they do not become a real
dry/reamp pair merely because both are present.

The first experiment behind that transport replaces only the shipping pickup
flux-linkage proxy polynomial when `ELECTRY_MEASURED_PICKUP_FLUX=ON`. It
evaluates the empirical static law fitted in
[Novak et al., DAFx-18](https://www.dafx.de/paper-archive/2018/papers/DAFx2018_paper_39.pdf),
using their SH-2N humbucker and SSL-5 single-coil equivalent dimensions as the
two Pickup Type endpoints. Their measured time-integral of pickup voltage
differs from physical coil flux by sign, turns and an unknown constant; Electry
also discards fitted amplitude `A` and unity-slope normalizes each endpoint.
Equation 6 therefore supplies a measurement-fitted nonlinear flux-linkage
shape, not measured physical gain or absolute flux. A 257-point table removes
cube roots from the render loop. Each endpoint is rest-subtracted and normalized
at the paper's measured 3 mm string/pole gap; the established neck/bridge
displacement drive remains around the new law, so small-signal gain and the two
pickup positions retain their existing calibration. Focused tests compare the
table with Equation 6 after the documented soft displacement bound, and cover
the render calibration wrapper, zero, endpoint interpolation and hostile
inputs.

The lawful real-audio direction check used open E2 (`6-0.wav`) from all five
Clean and RAT pickup folders in [EGFxSet](https://zenodo.org/records/7044411).
The corpus uses a conventional six-string, a fixed 2 mm pick and independently
normalized clean/real-pedal files. Several notes already contain substantial
signal in their first frame, so fixed 0-30 and 30-80 ms windows are descriptive;
onset-rise and absolute-level comparisons are inadmissible. Across the five
pickup selections the scale-invariant medians were:

| EGFxSet E2 | 30-80 / 0-30 ms body | 0-30 ms crest | centroid 0-30 / 30-80 ms | upper-band share 0-30 / 30-80 ms | harmonicity 0-30 / 30-80 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Clean | -0.343 dB | 10.071 dB | 252 / 253 Hz | 2.597 / 2.327% | 0.903 / 0.996 |
| RAT | -0.703 dB | 5.502 dB | 346 / 438 Hz | 16.724 / 17.268% | 0.615 / 0.956 |

The closest available raw-domain row is still mismatched: EGFxSet Clean Bridge
is a conventional single-coil E2, whereas Electry uses its eight-string default
build, bridge selector and 32% pickup morph. It nevertheless gives a useful
no-promotion check:

| scale-invariant E2-open descriptor | EGFxSet Clean Bridge | Electry OFF | fitted-law shape ON |
| --- | ---: | ---: | ---: |
| 30-80 / 0-30 ms body | -0.804 dB | -0.493 dB | -1.648 dB |
| 0-30 ms crest | 10.071 dB | 11.693 dB | 13.175 dB |
| centroid, 0-30 / 30-80 ms | 347 / 338 Hz | 410 / 401 Hz | 395 / 385 Hz |
| upper-band share, 0-30 / 30-80 ms | 11.889 / 11.062% | 25.095 / 22.931% | 21.123 / 18.463% |
| harmonicity, 0-30 / 30-80 ms | 0.903 / 0.974 | 0.886 / 0.998 | 0.877 / 0.998 |
| robust decay, 0.25-1.75 s | -2.253 dB/s | -5.421 dB/s | -4.945 dB/s |

The fitted-law shape moves centroid, upper-band level and the output-decay slope
toward that one real bridge take, but makes early body, crest, upper-share
contraction and onset harmonicity worse. The deterministic metal score tells the
same smaller story after distortion: full-file RMS changes by +0.166 dB dry and +0.052 dB
wet; its first twelve Palm hits move from 7.5622% to 7.7114% upper-band share
and from 0.897632 to 0.897016 harmonicity. Pickup/chain variation in the real
set is much larger than that candidate delta.

These real-reference descriptors came from an exploratory local analysis with
no frozen analyzer or provenance receipt. They are descriptive diagnostics and
cannot support promotion; a commissioned, registered raw/reamp corpus and a
versioned analyzer remain required for that decision.

The experiment therefore remains off. With the calibrated neck/bridge drive it
still fails 32 current-engine spectral, transient, articulation and ultrasonic
regression/compatibility rails; those rails protect the present instrument but
are not independent realism evidence. Single local arm64 Release timing probes
at 96 kHz observed roughly 25-45% more eight-string render time than flag off;
that range is diagnostic, not an interleaved portable CPU estimate. It is not a
plausible trade for a mixed real-reference result. No coefficient was fitted to
EGFxSet, no waveform distance was taken between separate performances, no
third-party recording is committed, and the shipping flag-off engine remains
byte-identical. A registered two-axis displacement-to-flux-linkage map identified
from target-instrument motion and open-circuit voltage—or de-embedded for the
measurement load and pickup impedance—at known pickup heights is the next
credible pickup experiment. This one-dimensional shape is retained only as a
reproducible default-off research branch.

### Reciprocal unilateral following-fret experiment

The first `ELECTRY_POSITIONED_FRET_COLLISION` ablation was deliberately only
one extra longitudinal read, a bilateral threshold and a one-sample output
loss. It established that collision location can preserve modal nodes, but it
was neither a local two-port junction nor a passive nonlinear-contact claim.
The successor keeps the same default-off flag and deletes that surrogate.

The replacement is the rigid one-sided obstacle in
[Evangelista and Eckerholm](https://www.diva-portal.org/smash/get/diva2:316228/FULLTEXT01.pdf),
placed at the equal-tempered following fret identified by
[Poirot et al.](https://www.pure.ed.ac.uk/ws/portalfiles/portal/338872061/Bilbao2023IEEEPerceptually.pdf).
For a speaking length `L`, its position from the bridge is
`j/L = 2^(-1/12)`. Electry rounds `j` once to the nearest integer point of the
folded ring. If the two physical incident waves are `a` and `b`, local
displacement is `u = a + b`. With fretboard-normal sign `s` and existing
clearance `c`, the update is identity while `s u <= c`; otherwise

```text
delta = (s c - u) / 2
a' = a + delta
b' = b + delta
```

It therefore lands exactly on `u' = s c`, preserves `a-b`, is reciprocal under
port exchange, and changes squared-wave energy by
`(c^2-u^2)/2 <= 0`. The signed single-delay ring stores `-b`, so production
implements the same update as `near += delta; farStored -= delta`; no second
delay line, interpolator, contact state, coefficient or dependency is needed.
The candidate reuses the current 0.96-2.08 mm clearance map, 25-100 ms opening
window, equal-tempered fret geometry, deterministic contact noise and saddle
rattle. Dead notes retain their distributed-contact fallback, and fret 22
retains an exact following-fret null.

A reach failure exposed a separate timing bug. On maximum-force 48 kHz Palm
attacks, the physical E1/E2 junction first moved about 7.25/5.10 ms after Note
On and reached fretboard-side displacement `0.03510/0.03116` loop units against
base clearances `0.02439/0.02676`. The old seam-derived output-energy gate did
not wake until about 19.15/11.02 ms, after those local peaks. The collision
opportunity now waits for either actual incident cell to become nonzero, then
advances once per string sample. This changes no threshold and prevents a
fresh string's window from opening over zero history.

Structural tests cover inactive-side identity, exact barrier projection,
reciprocity, energy nonincrease for both orientations, maximum-force E1/E2 Palm
reach, finite maximum-artifact eight-string output, bend-tracked cells and every
supported host rate from 44.1 through 384 kHz including 96.001 kHz. A
one-period harmonic fixture measures `0.016630` gain for near-antinode H9 and
`0.997882` for near-node H18; H1-H32 never exceed unity. Complete Release DSP
suites pass with the flag both OFF and ON.

The frozen evaluator gives a clear non-promotion result:

| comparison | OFF -> unilateral ON | decision |
| --- | ---: | --- |
| 40-hit pre-FX Palm/Dead metal WAV | byte-identical, SHA-256 `69e1cd0094c899b7...` | No raw metal exposure |
| same take after Modern high gain | byte-identical, SHA-256 `ef32b65b6b8e0f7c...` | No distorted metal exposure |
| six v3 Palm plus two Dead probes | all byte-identical | No muted-note improvement |
| v3 Open E1 | `+0.0123 dB` RMS; difference `-51.66 dB` relative to OFF | Tiny non-isolating change |
| v3 Open E2 | `+0.0063 dB` RMS; difference `-57.67 dB` relative to OFF | Tiny non-isolating change |

The manifests differ only because they truthfully record the build flag. Since
the registered raw and distorted metal waveforms are identical, their distance
to every real recording and every previously reported direction descriptor is
also identical; the candidate supplies no metal-realism win to promote. The
available lawful exact-eight recordings do not isolate a known following-fret
hit, action, relief, fret height and attack force, and the most relevant
published collision experiment does not provide a transferable target-guitar
clearance map. Lowering `c` after seeing the null benchmark would manufacture
exposure rather than validate physics. The flag therefore remains OFF, with no
A--Z listening gate: a byte-identical product score is already an objective
tie. Reopening promotion requires a controlled low-action capture with measured
geometry and held-out raw DI plus reamped high gain.

Two other low-CPU mechanisms were audited but not implemented. The local
negative-stiffness law in
[Harazono et al.](https://www.jstage.jst.go.jp/article/ast/33/5/33_E1148/_pdf)
uses `0.8/1.3 N/m` values fitted to one commercial two-row specimen; the
[Yamaha/CIRMMT two-axis programme](https://www.cirmmt.org/en/research/projects/yamaha-rnd_guitar-analysis)
supports amplitude-dependent signed splitting but publishes no force grid that
transfers to Electry's pickups and .080 string. Likewise,
[Yudasaka et al.](https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Ayudasaka_ismra2025.pdf)
show greater than 40 dB fret-to-fret admittance variation and a decisive 175 Hz
example on one Yamaha Pacifica, but not the complex modal residues, Q and phase
for the target eight-string. Importing either specimen's constants would be a
new fit with no valid benchmark, so pickup back-action and fret-specific neck
mobility remain measurement plans.

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

| measure | real four-hit median (range) | old Dead | Dead-correction checkpoint |
| --- | ---: | ---: | ---: |
| 30-100 ms RMS | -3.57 dB (-10.12..+1.17) | -33.82 dB | -8.10 dB |
| 100-250 ms RMS | -12.66 dB (-20.68..-6.20) | -51.34 dB | -15.12 dB |
| 250-380 ms RMS | -20.75 dB (-29.18..-12.71) | -62.04 dB | -23.43 dB |
| centroid, 0-30 / 30-100 / 100-250 ms | 220.7 / 136.9 / 85.3 Hz | 261.8 / 157.3 / 92.2 Hz | 209.3 / 142.2 / 94.3 Hz |
| 30-250 ms harmonicity | 0.936 | 0.385 | 0.988 |

Those model columns record the original Dead-correction checkpoint's medians of a timing-matched
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
full pressure is 8.34 dB below zero pressure; the equal steps around 10% have
normalized attack differences 0.000530/0.000531, safely replacing the old
roughly 24x jump. No parameter, keyswitch or mapping changed.

The four-hit regression recreates both complete annotated passes. At the Palm-
body checkpoint the isolated Dead evaluator WAV was byte-identical while the
ringing Palm state preceding the ghosts changed. With the later live-damping
phase correction, relative-RMS medians at that checkpoint were
-8.178/-15.086/-23.257 dB. The physical-period pick-geometry correction moves
the current shipping medians to -7.466/-14.073/-22.059 dB. Those remain inside
the observed ranges and are the pinned contextual snapshot.

That median also concealed a repeat-context miss. Relative to each hit's own
0-30 ms onset, the second Dead attack decays faster than the first in both real
passes, even though those second onsets are 2.30 and 3.82 dB louder:

| second minus first | real 30-100 / 100-250 / 250-380 ms | Electry live-damping checkpoint |
| --- | ---: | ---: |
| pass 1 | -6.908 / -4.344 / -3.268 dB | +0.600 / -0.007 / -1.053 dB |
| pass 2 | -5.093 / -2.790 / -2.889 dB | +0.701 / +0.986 / +1.325 dB |

The absolute-pitch correction moved the fixed-window envelope error from 2.75
to 3.23 dB and this six-value contextual RMSE from 4.559 to 4.92754 dB. The
Palm-body correction then moved the contextual RMSE to 4.79655 dB and the
live-damping phase correction moved it to 4.85398 dB. The physical-period
source measures 4.81642 dB, while its three-window aggregate median RMSE
against the real median is 2.509 dB (down from 3.214 dB immediately before the
correction). Every
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
That integer proof alone did not cover Electry's cubic fractional seam. At the
now-retired finite-heel checkpoint, a separate transfer sweep covered the cubic
reads, the complete finite footprint and the string's actual modes; the current
point-contact path retains its cubic/point-touch regressions, while Palm no
longer uses that contact. In the robot
experiment of
[Pluta, Tokarczyk and Wiciak](https://www.mdpi.com/2076-3417/12/3/1659),
measured E2 re-excitation at 12 and 70 ms did not return to the single-pluck
spectrum even after one fundamental period. Adding a moving one-sided rigid
contact at least reproduced the observed damping, ringing and pitch glides,
although neither simulation matched the measured spectra accurately.

The next default-off experiment retained that exact integer topology and added
the dynamic spring from Evangelista and Smith rather than tuning another
memoryless loss. With `ELECTRY_PASSIVE_REPICK_SPRING=ON`, the production engine
computes characteristic wave impedance `r = sqrt(T mu)`, uses the already
documented medium-plectrum stiffness `K = 6 kN/m`, and evaluates the
representative impedance-matched case `R = 2r`. For
`rho(s) = (R s + K) / ((R + 2r) s + K)`, a bilinear one-state realization
drives the common two-rail mode; the orthogonal transverse coordinate passes
unchanged. A 2,049-point sweep of the production coefficients at 44.1, 96 and
384 kHz checks `|1 - 2 rho(e^jw)| <= 1`, while focused lifecycle checks require
fresh-contact identity, a first-round-trip repick before the output-energy
follower can respond, two committed folded-ring history cells, a stable pole,
and state removal at detachment. An explicit dynamic two-rail reference and the
production-style in-place folded ring remain sample-exact with zero measured
state delta.

The scope is deliberately narrower than a complete plectrum. The existing
release supplies the new stroke by linear superposition, while the spring acts
only on the homogeneous response already ringing before a repick. It replaces
the scalar seam choke for that Contact window, projects one state onto the
existing normalized two-axis plectrum direction, and snaps each polarization to
integer cells because a cubic gather paired with a non-adjoint write would not
inherit the proof. The edge is fixed at zero displacement; no measured drive,
unilateral contact/recontact, deflection threshold or detachment impulse is
claimed.

The frozen comparisons reject this representative dynamic candidate:

| frozen check | OFF | spring ON | result |
| --- | ---: | ---: | --- |
| ten isolated v3 evaluator WAVs | reference hashes | byte-identical | No one-shot raw-DI change or improvement |
| rapid Palm, median absolute cell-mean error | 2.05031 dB | 6.93366 dB | 3.38x worse |
| rapid Palm, median worst-cell error | 3.48144 dB | 9.64469 dB | 2.77x worse |
| stateful clean Dead, first-to-repick contextual RMSE | 4.81647 dB | 5.50563 dB | 14.3% worse |
| stateful clean Dead, three hit medians | -7.4665/-14.0726/-22.0588 dB | -7.6885/-14.6553/-22.9398 dB | All three farther from the contextual rail |

The frozen 40-hit metal score makes the audible scale and direction explicit.
Its OFF dry/high-gain SHA-256 values remain
`69e1cd0094c899b7f5640ffc74b71dafd87978809323e01ace237906728b4ecb` and
`ef32b65b6b8e0f7c04a0a18ddbe2196451d5528862d57a109006a582cbabbf64`;
ON produces
`f52a5f65fc14a27dac3d7df6f1a322ef431de1b5c8e51ee235390d51d5746e34` and
`8de68b3a4b3b5f02fa1393d97b21078f514cac291448aa15a0827514f93bc9b3`.
Dry RMS changes -0.066826 dB and high-gain RMS +0.060298 dB, yet the respective
difference signals are -18.9462 and -15.8859 dB relative to OFF. The descriptor
comparison supplies no compensating realism win:

| distorted descriptor | public real | OFF | spring ON | direction |
| --- | ---: | ---: | ---: | --- |
| Palm 30-80/0-30 ms body | +0.87 dB | -2.3162 dB | -2.4351 dB | Worse |
| Palm body/attack centroid | 1,033/1,506 Hz | 183.0/303.4 Hz | 174.7/294.8 Hz | Worse |
| Palm upper-band share | 73.0696% | 7.5622% | 7.5236% | Worse |
| Palm harmonicity | 0.212544 | 0.897632 | 0.896516 | Better by only 0.00112 |
| Dead 30-80/0-30 ms body | +3.12/+1.30 dB first/second repick | -1.7259 dB | -1.4784 dB | Slightly closer |
| Dead body/attack centroid | 652/310 or 754/1,314 Hz | 196.4/320.9 Hz | 203.3/314.6 Hz | Mixed |
| Dead upper-band share | 9.9994% or 64.6730% | 8.3287% | 7.8233% | Worse |
| Dead harmonicity | 0.786 or 0.247 | 0.905119 | 0.910024 | Worse |

Those public clean and distorted previews are different performances, not a
raw-DI/reamp pair, and the distorted descriptors are only rejection/direction
alarms. No local true paired raw/high-gain corpus exists. Accordingly the flag
remains OFF, the ordinary build and isolated output stay unchanged, and neither
market-realism nor promotion is claimed. The `electry-repick-phase/v1` capture
contract below freezes a single dimensionless memoryless contact amount; a
dynamic `K/R/tau` or moving-edge candidate needs a new versioned preregistration
before capture and cannot inherit that contract after observing data.

#### Moving unilateral plectrum-contact ablation

A temporary mutually exclusive default-off candidate tested the missing drive
and release topology without fitting another coefficient. The existing
reciprocal one-state junction followed a prescribed plectrum edge along the
signed normalized two-polarisation stroke direction. If
`q = edge - string` was nonpositive, the one-sided contact separated and
cleared its filter state while remaining able to recontact. At the existing
4.8 N / 6 kN/m compression threshold, or at the bounded Contact timeout,
detach was final and went directly to Idle: the moving edge had supplied the
new stroke, so entering the legacy Release/Tail would have counted the plectrum
work twice. The trajectory reused the existing physical force, compliance,
pick position and 0.55--3 ms contact duration. That was a deliberately testable
engineering inference, not a measured edge path.

The temporary focused regression covered 44.1, 96 and 384 kHz. A fresh attack
never armed the candidate; a same-note ringing repick did, with positive finite
edge step and release compression, `|pole| < 1`, unit DC reflectance and 0.5
Nyquist reflectance. Final detach cleared the one filter state and reached Idle
without ever passing through legacy Release. Output remained finite and
nonzero. The ten isolated evaluator WAVs were byte-identical to OFF, while two
complete candidate metal renders, including their manifests, were byte-
identical to each other.

The frozen stateful checks reject the inferred moving edge:

| frozen check | OFF | moving contact ON | result |
| --- | ---: | ---: | --- |
| ten isolated v3 evaluator WAVs | reference hashes | byte-identical | Fresh path remains the control |
| rapid Palm, median absolute cell-mean error | 2.05031 dB | 4.75360 dB | 2.32x worse |
| rapid Palm, median worst-cell error | 3.48144 dB | 6.07624 dB | 1.75x worse |
| stateful clean Dead, first-to-repick contextual RMSE | 4.81647 dB | 6.88066 dB | 42.9% worse |
| repeated-score dry RMS | -36.1143 dBFS | -42.6526 dBFS | 6.5383 dB collapse |
| repeated-score high-gain RMS | -30.2860 dBFS | -34.8939 dBFS | 4.6079 dB collapse |

The unchanged first metal hit fixes interpretation: the dry render first
differs one sample after hit two begins, so the level loss comes from modeled
repicks rather than a global gain or FX change. Candidate dry/high-gain hashes
are `7f76d072145bac6e04195f1c2ac06b406cf070aea2d7318cb02fd0c835d6725f`
and `7258be6e03e2ea2fe637662e6be40450fcd029a7870eb2ea2fd6fb23f118ee2a`.
Relative to OFF, median Palm repick attack/body levels fall
9.8266/17.5083 dB dry and 3.6228/12.4164 dB after high gain; Dead falls
11.0624/17.3390 dB dry and 5.4423/12.2588 dB after high gain.

The distorted descriptors confirm that this is not a useful trade. Palm
30--80/0--30 ms body falls from -2.3162 to -11.2100 dB and Dead from -1.7259
to -8.1954 dB, versus the public +0.87 and +3.12/+1.30 dB direction alarms.
Palm centroid, upper share and harmonicity move to 1,753/1,808 Hz, 89.6784%
and 0.160795—past the public 1,033/1,506 Hz, 73.0696% and 0.212544 values—but
only by deleting the pitched body. Dead is more extreme at 1,660/2,158 Hz,
97.0702% and 0.062950. A closer isolated spectral ratio cannot compensate for
a missing repick.

This falsifies the inferred trajectory and release threshold for Electry; it
does not falsify moving unilateral contact itself. Evangelista and Smith's
junction explicitly requires edge motion, and the robotic E2 study observed
pre-release contact but did not measure a transferable exact-eight edge path.
Public recordings can reject the result but cannot identify position,
velocity, force or damping. The implementation, compile flag and focused test
were therefore removed after recording the hashes and result. Any v2 capture
must add synchronized edge position/velocity or force to the existing
string-motion and bridge-DI channels, freeze trajectory/release equations on
TRAIN, and open untouched HOLDOUT only once.

The missing experiment is now frozen as the separate
[`electry-repick-phase/v1` capture contract](capture/electry-repick-phase-v1/README.md).
It keeps the validated Palm/Dead schema unchanged and records open E1/E2 on one
96 kHz exact-eight rig as 64 independently reset two-stroke pairs per string,
with a measured felt fixture damping the seven non-target strings. Four cue
phases each balance eight Down and eight Up second strokes, but a
synchronized contact reference and latency-calibrated plectrum-normal motion
channel—not the cue or DI transient—assign the fixed phase quadrant. One joint
48 ms pre/post bridge-DI H1-H4 energy-gain fit must first clear within-cell
variability on both strings and directions in all three TRAIN clusters. Only
then is one TRAIN-derived candidate frozen against exactly two still-unopened
same-rig HOLDOUT clusters. Those clusters are decoded once for independent
real-effect replication and candidate promotion; additional forces, strings
and rigs come later.

### Retired finite-width Palm experiment and body checkpoint

The 4 mm, six-read Palm heel that held for 100 ms and released over 10 ms is no
longer part of the instrument. It was a useful bounded experiment, but user
feedback described its rendered chugs as squishy and short of body. A same-build
source-history A/B confirmed that it stacked a second transient loss on the
existing selective bridge-hand damping: in demo 04 it moved 30-80/0-30 ms body
from -5.36 to -6.77 dB, reduced the 80-500 Hz share from 54.68% to 39.51%, and
raised crest from 13.74 to 14.66 dB.

The correction deletes the Palm-only width state, six extra cubic reads, hold
and release. The steady hand-loss law, its 22x fundamental relief, 4.5x high
rate, velocity latch, Mute Tightness, Mute Pressure and short impact state are
unchanged. Palm's existing modal-excitation factor moves only from 0.74 to
0.85. No gain compensation, resonator, parameter or dependency is added.

At that checkpoint, the unnormalised evaluator read:

| probe | Light | Medium/default | Hard |
| --- | ---: | ---: | ---: |
| E1 30-80 / 0-30 ms body | -2.9534 dB | -4.1939 dB | -7.2378 dB |
| E2 30-80 / 0-30 ms body | -2.2049 dB | -3.2506 dB | -5.4061 dB |
| E1 raw peak | -24.94 dBFS | -25.80 dBFS | -26.89 dBFS |
| E2 raw peak | -23.28 dBFS | -24.26 dBFS | -25.08 dBFS |

At that checkpoint's default, body improved by 1.2669 dB on E1 and 1.5686 dB
on E2 versus the retired heel; peaks rose 2.23/2.44 dB and onset crest fell
0.43/0.44 dB. Light -> Medium -> Hard remained strict. The E1 first 2 ms-RMS
crest was 1.77 ms after detected onset. On E2 the first crest was at 3.20 ms
and sat within
0.01 dB of the physical returned crest at 14.60 ms, so the global-maximum time
alone misleadingly reports the latter.

At that checkpoint, the noise-free regression independently measured
-4.060/-3.150 dB of default E1/E2 body. Its Palm-minus-Open 150-500 ms
low/high losses were
-10.327/-19.832 dB on E1 and -8.347/-22.812 dB on E2; the attack high/low
balance remained 4.499/0.385 dB darker than Open. These separate body and
selective-loss rails fail both a blanket over-damper and a uniformly bright
mute.

The sparse F2 proxy exposed the trade rather than hiding it. The retired heel
moved the paired 0-30-to-30-80 ms contraction to -4.009 dB. Palm at that
checkpoint moved from 0.314558 to 0.264052 upper share while Open moved from
0.346791 to
0.337670, a paired -0.644354 dB at 44.1 kHz. Across 44.1, 48, 96 and 192 kHz it
stayed within -0.644..-0.658 dB. That was farther from the two public conventional-
guitar cells at -6.099/-15.290 dB, but those two cells cannot set an eight-string
coefficient. The regression therefore retained negative direction and sample-
rate invariance while the long-band and body guards carry independent product
evidence. Commissioned exact-eight TRAIN/HOLDOUT captures remain the fit gate.

At the Palm-body checkpoint, Open and Dead evaluator WAVs were byte-identical to
the retired-contact build. The later live-damping phase correction changed the
mixed Open -> Palm -> Dead -> Dead phrase: that checkpoint's medians were
-8.17821/-15.0856/-23.2573 dB and the first-to-repick contextual RMSE was
4.85398 dB. The common high-gain rapid-Mute body measured 8.826009% upper share
and 0.882806 harmonicity, inside its loose model-only rails.

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
| E1 (MIDI 28) | -13.662 dB | -16.037 dB | -17.828 dB | 4.17 dB |
| E2 (MIDI 40) | -12.720 dB | -14.598 dB | -16.074 dB | 3.35 dB |

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
| E1 palm mute, medium/default | 1.77 ms | 0.00 dB | -3.98 dB | -11.30 dB | -17.57 dB |
| E2 open | 12.83 ms | 0.00 dB | +0.35 dB | -1.46 dB | -4.00 dB |
| E2 palm mute, medium/default | 13.13 ms | 0.00 dB | -3.40 dB | -10.38 dB | -15.79 dB |

That E2 time reports the global 2 ms-RMS maximum: its first crest is already at
3.20 ms and is only 0.01 dB below the returned-path crest at 14.60 ms. The note
does not wait thirteen milliseconds to begin.

The real CC0 E1 open take falls 9.29 dB by the last window while Electry falls
2.15 dB. That clip's muted take falls only 3.96 dB there, against Electry's
17.57 dB, and its two muted onset-to-peak times are 90.57 and 21.59 ms (median
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
Palm's tracked-harmonic high/low ratio is 8.58 dB below Open on E1 and 0.10 dB
below it on E2 during 0-50 ms. These changes add no parameter, keyswitch,
random source or performance rule.

At the default depth, the current noise-free tracked-harmonic guard measures
Palm-minus-Open 150-500 ms low/high losses of -9.767/-23.194 dB on E1 and
-8.219/-22.582 dB on E2. The high band therefore loses an additional
13.427/14.362 dB. The model keeps evidence-backed faster upper contraction
without requiring the retired transient contact.

At the Palm-evidence checkpoint, the existing Mute Tightness control spanned a
real envelope range rather than three labels over one sound:

| probe and depth | 0-50 ms | 50-150 ms | 150-500 ms | 500-1000 ms |
| --- | ---: | ---: | ---: | ---: |
| E1 palm light (0.00) | 0.00 dB | -2.07 dB | -7.74 dB | -14.90 dB |
| E1 palm medium/default (0.55) | 0.00 dB | -3.98 dB | -11.30 dB | -17.57 dB |
| E1 palm hard (1.00) | 0.00 dB | -7.39 dB | -14.49 dB | -20.52 dB |
| E2 palm hard (1.00) | 0.00 dB | -5.95 dB | -12.49 dB | -17.54 dB |

Attack, body, absolute tail and high-band loss remain strictly ordered across
the full Light -> Medium -> Hard regression sweep. This continuous range is the
physical model's answer to sample libraries' discrete mute layers: one
understandable control remains continuous across the modeled range.

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
   That supports measured-modal experiments and capture-fitted dead spots once
   suitable data exist; it does not justify arbitrary extra modes, residues or
   position maps.
4. Magnetic pickups are usefully represented as a linear dynamic system around
   a static nonlinearity
   ([DAFx-18](https://www.dafx.de/paper-archive/2018/papers/DAFx2018_paper_39.pdf)).
   A modern active anchor is a defensible later step, but published Fishman and
   EMG voicing points do not provide enough Q/gain data for a blind fit.

For the next implementation rounds, the evidence/risk ranking is:

1. A passive finite-duration plectrum junction with a finite contact width.
   [Evangelista and Smith](https://dafx10.iem.at/proceedings/papers/EvangelistaSmith_DAFx10_P21.pdf)
   provide the mass-spring-damper scattering structure and threshold release;
   sub-sample release interpolation would be Electry's numerical refinement. It
   is attack-local, cheap and directly testable against clean plus distorted
   note pairs, but its compliance, trajectory and release still need controlled
   captures.
2. Distributed time-varying bridge-hand contact. The preliminary pressure
   distributions measured by
   [Biral, d'Alessandro and Freed](https://speech.di.uoa.gr/ICMC-SMC-2014/images/VOL_2/1483.pdf)
   extend several centimetres, while pressure at a sampled region varies around
   each pick. They motivate a later passive finite-contact model rather than
   another global low-pass, but only after a footprint/pressure profile is
   measured and the complete topology has a phase-compensated energy proof.
   They do not identify its width, stiffness or damping. Calibration risk is
   higher because public audio does not identify hand pressure or geometry.
3. Registered two-axis pickup transduction: retain the two existing string
   planes separately through a measured 2-D displacement-to-flux-linkage map,
   then differentiate and use the existing loaded circuit; that requires an
   open-circuit source measurement or de-embedding the measurement load and
   pickup impedance to avoid counting electrical loading twice. Longitudinal
   pickup aperture remains a separate spatial dimension. Novak's setup measured
   acceleration-derived displacement and finite-input-impedance terminal voltage
   together at known gaps, but the target Electry instrument still lacks a
   registered displacement scale, pickup height/loading, horizontal motion, and
   high-frequency/full-aperture validation. [Their later two-dimensional measurements](https://aes.org/publications/elibrary-page/?id=20728)
   are the relevant starting point.
4. Energy-stable unilateral fret/fretboard contact only after a controlled
   low-action capture. [Bilbao et al.'s SAV/IEQ formulation](https://www.pure.ed.ac.uk/ws/portalfiles/portal/470239305/BilbaoEtal2024RealTimeGuitarSynthesis.pdf)
   avoids iterative Newton solves while retaining a discrete energy balance,
   but adapting that distributed state to the waveguide architecture is
   substantially riskier than the first three items.
5. Fret-specific neck mobility and one shared passive bridge modal bank after
   measured mobility is available. A passive multiport/shared-bridge formulation
   gives one coupled structural state with port-, string- and polarization-specific
   residues, rather than independent arbitrary resonators; see
   [Bank and Karjalainen](https://www.dafx.de/paper-archive/2010/DAFx10/BankKarjalainen_DAFx10_P60.pdf)
   and [Maestre et al.](https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Amaestre_jointmodeling_ieeeaslp_2017.pdf).

Full longitudinal-string dynamics and a synthetic “phantom partial” generator
remain deferred. For rigid electric-guitar terminations,
[Bank's analysis](https://home.mit.bme.hu/~bank/publist/dafx09.pdf) identifies
quasistatic tension and pitch glide as the salient nonlinear effect; it does not
support adding a longitudinal resonator without measured longitudinal bridge
force/admittance. The existing default-off energy-derived glide is therefore
the bounded experiment.
Pickup nonlinearity and fret impacts can also create extra partials, making an
uncontrolled DI an ambiguous way to identify longitudinal coupling.

Useful public research leads are [EGFxSet](https://zenodo.org/records/7044411)
for fixed-pick clean/real-pedal notes, [Guitar-TECHS](https://zenodo.org/records/14963133)
for simultaneously captured normal/Palm DI and miked output,
[IDMT-SMT-Guitar](https://zenodo.org/records/7544110) for string/fret/articulation
labels, and [GUITAR-FX-DIST](https://zenodo.org/records/4298000) for paired
unprocessed/software-distorted examples. Rights and timing must be reviewed per
corpus: IDMT's CC BY-NC-ND terms do not authorize commercial-product evaluation,
and Guitar-TECHS warns that its simultaneously captured channels can be offset
by as much as 100 ms. None combines controlled exact-eight string mechanics,
raw DI and a calibrated high-gain reamp, so the commissioned capture and blind
gates remain necessary.

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
duplicate attack. A wrist held earlier in silence also re-anchors at the first
physical contact; a near-complete empty phase cannot put the next repeat only a
few samples after that played note. One shared fractional phase repicks all
physically held strings through the existing attack path, while automatic
contacts retain its fractional remainder through Strum delay. Alternate
advances once for a chord; sustain-only strings remain inert. A due contact is
skipped while any held string still has an in-flight Strum delay rather than
overwriting that pending attack. E6..B6 retain their established one-shot
behavior. A hammer or legato slide landing on the same sample is only the
fretting hand and leaves the due wrist contact intact on every held string; an
actual played pick at that boundary still consumes it once.

Overlapping B0 owners balance; a positive repeated Note On restarts the phase
and updates force, while a zero-velocity Note On releases an owner. CC120 and
CC121 preserve a physically held wrist, whereas CC123, Panic, prepare and
release clear it. Output-mode changes route the gesture to both engines. The
active hold is transient and never serialized; only its rate is saved. The
scheduler is deliberately free-running rather than host-tempo synchronized,
and adds no jitter, missed strokes, pattern editor or hidden direction bias.
`22-tremolo-picking-study.wav` renders the planned 8/12/16 rate anchors,
a moving single-note line, a vibrato lead and a held Drop-E chord whose low
string slides on an 8-strokes/s boundary through the same physical path. It is
audible workflow proof, not a human-performance fit.

### End-stop articulation market gap and capture gate

A review of current extended-range guitar competitors found one repeated articulation
gap that Electry's continuous Mute and repick controls do not subsume. The
official manuals describe materially different end-of-note sounds:

| instrument | officially documented note-ending surface |
| --- | --- |
| [Shreddage 3.5 Hydra](https://www.impactsoundworks.com/manuals/Shreddage%203.5%20Hydra%20Manual.pdf) | separate Pitched Release, Power Chord Pitched Release and Unpitched Release articulations, plus a dedicated fret-noise key |
| [Ample Metal Hellrazer](https://www.amplesound.net/en/Main_Panel_Manual-AMH.pdf) | independent Release Sound gain, Fingering Sound toggle/gain and Stroke Noise toggle |
| [Evolution Dracus engine](https://www.orangetreesamples.com/download/manual/EvolutionEngine-UserGuide.pdf) | release-sample volume, automatic fret noise after notes released outside the fretting-position area, and a release-slide articulation |
| [Unreal Instruments METAL-GTX](https://unreal-instruments.wixsite.com/unreal-instruments/metal-gtx) | five CC24 release shapes, an independent CC25 release level, and automatic slide-out or alternate-stroke action releases |

METAL-GTX is market-vocabulary evidence only. Its public terms permit
commercial works made with the library but do not expressly permit competitive
calibration, while its release clips lack physical string/fret, named
guitar/pickup/chain, raw-edit history and a synchronized stop marker.

Those are verified vocabulary differences, not evidence that any product is
more realistic. Sample counts, controls and vendor prose provide no held-out
physical-stop comparison. Conversely, Electry's `beginVoiceRelease()` applies
one 60 ms loop-decay target through a 22 ms smoother and one procedural burst:
6-15 ms from String Age/Gauge, wound/plain bandwidth and an attack-velocity
level scale. It distinguishes string construction, but not whether a fretting
finger relaxed or the bridge hand stopped the note. This end-stop boundary is
separate from the [attack-time plectrum-scrape
gate](../README.md#angle-conditioned-plectrum-scrape-research).

The smallest useful experiment changes no MIDI, parameter or DSP. On the
production eight-string, record fret-5 notes on physical strings 8, 6 and 1.
Cross Down/Up attacks with two registered stops: fretting-finger relaxation and
bridge-hand contact. Keep pick, position, attack-force stratum, sustain time,
pickup, gain, setup and player fixed. Capture raw bridge DI, a high-bandwidth
contact channel and an optical or force stop marker. Freeze exclusions and
maximum attempts first; retain at least 20 accepted repeats per cell in three
TRAIN clusters and exactly two untouched HOLDOUT clusters.

Align to the physical stop marker and score 0-15 ms and 15-60 ms energy,
500 Hz-8 kHz share, harmonicity, and fitted T20/T60. Preserve the pre-stop RMS
and partial balance as covariates rather than normalizing away the string state.
The two stop classes must first separate beyond within-cell repeatability on
both low wound strings and the plain string. If they do not, adding another
articulation or selector is rejected.

Only then may one event-time candidate be fitted on TRAIN. It must declare in
advance how already available physical state selects its stop class; the
capture alone does not authorize a new performance control. Promotion requires
at least 50% lower repeatability-normalized median HOLDOUT error in both stop
classes and every cluster, no worse p95 or maximum error, no class-direction
reversal, and an RMS-matched blind win for closeness to the recorded stop under
clean and high-gain monitoring. MIDI Note-Off velocity remains unassigned: the
current sources provide no guitar mapping for it.

A runtime candidate may reuse cached event state but may add no allocation,
filter, parameter or per-sample branch. Pre-stop audio and Release Noise at zero
must remain byte-identical; block partitioning, sustain/MPE ownership, Panic and
CC reset semantics remain exact. Nine or more warmed alternating ABBA/BAAB
rounds must stay below 0.25% paired-median overhead at 96 kHz and 1% worst-case
overhead at 384 kHz.

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

`LATCH | HOLD` is saved as non-parameter state alongside the current 28 host
parameters; transient held keys are never serialized. The suite pins
current-state round trips and the one deliberate development migration: a
state without `ampModel` selects Modern High-Gain. Play-style Note On, Note
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
Distortion 0.45, Amp 0.95, Amp Voice Modern High-Gain and Compressor 0.60 are
the current train-only starting point,
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
include the numerical listener gates and the current 8x common-chain effect
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

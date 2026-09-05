# Third-party notices

Acustra's original source code is covered by the project's `LICENSE` file.
The separately licensed framework, recordings and adapted measurement data
below are not relicensed by Acustra.

## Tárrega scores for the repertoire demonstrations

Works: *Recuerdos de la Alhambra* and *Lágrima* by Francisco Tárrega
(1852–1909)

Licence: public domain. Tárrega died in 1909, so every copyright term
measured from the author's death expired in 1979 at the latest.

`Tools/GenerateRepertoireScores.py` reads two Mutopia Project MIDI files —
[`recuerdos.mid`](https://www.mutopiaproject.org/ftp/TarregaF/recuerdos/recuerdos.mid)
(md5 `b91ef372bc2f64e383be1a539f033f62`) and
[`lagrima-duo.mid`](https://www.mutopiaproject.org/ftp/TarregaF/lagrima-duo/lagrima-duo.mid)
(md5 `0b20164983fe93c99a0afc689d55f86b`) — and takes from them only Tárrega's
composition: which pitch sounds when, and for how long. Mutopia declares the
Lágrima file public domain; the Recuerdos typesetting carries CC BY-SA 3.0,
which covers that edition's own engraving. No engraving, fingering, barre
indication or editorial marking is read or reproduced, and none appears in
`Tools/RepertoireScores.h`, which holds pitch, onset and length alone. The
velocities in that header are an authored performance written by this project,
not data from either file.

## FreePats Spanish classical guitar recordings

Work: *Spanish classical guitar*, version 2019-06-18

Creator: `roberto@zenvoid.org` for the FreePats project; recorded in 2008

Source: <https://freepats.zenvoid.org/Guitar/acoustic-guitar.html>

Source archive:
[`SpanishClassicalGuitar-SFZ+FLAC-20190618.7z`](https://freepats.zenvoid.org/Guitar/SpanishClassicalGuitar/SpanishClassicalGuitar-SFZ%2BFLAC-20190618.7z)
(4,713,892 bytes)

Archive SHA-256:
`903916921a21662d2237ade7f0e98e55de93cb7b86da219e4e10f4ad385b8f5e`

Licence: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)

The offline `AcustraReferenceBank` target embeds the 41 source regions used by
MIDI 38–84 for fitting and tests; the plug-in and standalone application do
not link that target. The full native 44.1-kHz mono recordings are converted
to signed PCM16, assigned measured roots, and losslessly Rice-delta packed.
The reference archive does not truncate, loop, resample or apply a terminal
fade to these regions. The original source note and a local CC0 legal-code
copy are distributed as
`ThirdParty/FreePats-Spanish-Classical-Guitar-README.txt` and
`ThirdParty/CC0-1.0.txt`.

## Shinyguitar microphone recordings (steel bank)

Work: *Shinyguitar* archtop-guitar sample library

Performer and mapping: D. Smolken / Karoryfer Lecolds

The upstream documentation does not name the string alloy. Acustra classifies
the corpus as its steel bank because the library includes simultaneous magnetic-
pickup captures, which require ferromagnetic strings; this is an inference, not
an upstream material claim.

Source: <https://github.com/sfzinstruments/karoryfer.shinyguitar>

Pinned source commit: `57243cca85277dbcc120ce17c6178032f93c80f3`

Licence: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)

The offline reference target embeds all 272 acoustic-microphone sustain WAVs
referenced by `Programs/acoustic.sfz` over MIDI 38–84: 17 roots, four velocity
layers and four round robins. Full native durations and captured relative
levels are kept; generation converts the 24-bit mono WAVs to PCM16 and adds a
60 ms terminal half-cosine de-click fade. The deterministic aggregate SHA-256 is
`4526e15b6b242dd68481c48eacae9409c09bc3bebec1da39cf66070f36a4ef7c`;
its path-and-file-byte method and inclusion rule are documented in
`ThirdParty/Shinyguitar-README.txt` and `Assets/SampleBank/manifest.json`.

## Eastman E1D steel-string recordings

Work: first-party Eastman E1D dreadnought picked and finger-plucked recordings,
recorded 2026-07-23

Creator/performer: Arthur, owner of the `ferrosintesis` source repository

Source introduction commit:
<https://github.com/0x4D44/ferrosintesis/tree/810318c92e33e31b36638b0ffa7ffc834a2ae6a2/samples/acoustic-guitar-eastman-e1d>

Generation was checked from repository revision
`94edbcfef226986d6ac28330020bc301fa5207d9`; both Opus-copy hashes match the
introduction commit.

Picked Opus source copy (`picked.opus`, 5,957,761 bytes) SHA-256:
`35e9e45b42a70f2fada2c9d93bf809d9046562fa3fa40cbac415ef75b92d926d`

Finger-plucked Opus source copy (`plucked.opus`, 5,945,086 bytes) SHA-256:
`16f5a8c7cde555441143243f264a03eff142cb8aafb6924236a82755a0396ce0`

Licence: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)

The offline reference target embeds eight pitch anchors from the finger-plucked
performance as a reproducible evaluation corpus. They are not linked or
selected by the public engine:
switching to this sparse, unmatched recording session at the midpoint of Touch
caused an unacceptable level and spectral discontinuity. The stereo Opus source
copy is decoded at 48 kHz, sliced to three seconds beginning 20 ms before its
pinned onset, and given a 60 ms terminal half-cosine fade. Acustra uses
late-sustain fundamental fits for playback tuning; that metadata change does
not alter the packed PCM. The local upstream source note is distributed as
`ThirdParty/Eastman-E1D-README.md`.

`Assets/SampleBank/manifest.json` records every reference region's mapping,
decoded hash, packed offset, onset, fade and source provenance. The separate
reference payload is transformed audio from these CC0 works; it is not
Acustra-authored source audio. The VST3, Audio Unit and standalone binaries
contain no part of this payload.

CC0 imposes no attribution or share-alike condition; this notice is retained
for provenance and change disclosure. CC0 covers only rights each affirmer had
authority to waive, supplies the material without warranty, and does not grant
trademark or patent rights. “Eastman E1D” identifies the recorded instrument
and implies no manufacturer affiliation or endorsement.

## Robert Mores guitar-measurement archive

Creator: Robert Mores, Hamburg University of Applied Sciences

Title: *Archive for the acoustical documentation of classical Spanish
guitars, flamenco guitars and romantic guitars from private and public
collections – bridge mobility* (2021)

Source: <https://doi.org/10.5281/zenodo.4604577>

Licence: [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/)

`Source/DSP/MeasuredBodyData.h` is adapted from the archive's
`qualified_selected_impulses.mat` (MD5
`733cb10baf5ce36d8bf333610ffbb260`). Acustra selects row g21, the archive's
2018 Lester DeVoe flamenca blanca with spruce top and cypress back and sides,
and the third one-second segment containing its treble-side bridge impact. It
applies the archive's full-record half-cosine taper and calibration scales,
then forms separate H1 force-to-pressure responses from the hammer channel to
the 10 cm treble-side and bass-side microphone channels.

The archive's physical-measures table identifies g21's strings as Savarez
Tomatito, with nylon/KF trebles and wound multifilament basses. Acustra adapts
this flamenco measurement for its original steel setting; it is not a measured
steel-strung body. The nylon setting uses g34, a 1971 Manuel Contreras classical
guitar with cedar top and Rio palisander back and sides, measured anechoically.
Both `MeasuredBodyData.h` and `MeasuredBridgeData.h` derive from these records;
the bridge adaptation fits positive-semidefinite heave/rocking residues to
the calibrated bass/treble acceleration-to-force measurements.

For each g21 path, Acustra inverse-transforms the H1 response, retains 3000 samples,
leaves the first 2700 unchanged and applies an authored 300-sample raised-cosine
fade. It independently reconstructs minimum phase from each path's magnitude,
selects 96 shared frequencies and Q values over 80 Hz–10 kHz, and fits separate
regularised complex residues for the two microphone paths. No corpus averaging,
FIR remainder, per-path peak/RMS normalisation or authored left/right gain
spread is used. Minimum-phase reconstruction, the 300-sample fade, modal
reduction, global gain and shape/material transformations are Acustra changes;
no DAFx-26 coefficient is used.

The g34 radiation uses the same minimum-phase/modal method with 12000 retained
samples, a fade over the final tenth and 103 shared poles; its longer window
is selected by the low-frequency Q-convergence check in the generator.

Acustra ships only those transformed numerical coefficients. It does not ship
the source MAT file, recorded impulses, photographs or documentation from the
archive. The attribution, source link, licence link and description of changes
above must accompany distributions containing the coefficient table.

## Measured Fylde steel-string bridge

Samuele Carcagno, Roger Bucknall, Jim Woodhouse, Claudia Fritz and Christopher
J. Plack, *Effect of back wood choice on the perceived quality of steel-string
acoustic guitars*, JASA 144(6), 3533–3547 (2018),
<https://doi.org/10.1121/1.5084735>.

Source data: <https://osf.io/f4pqa/>,
`guitar_back_wood_code_data_v1.0.1.zip`,
SHA-256 `1d35dd28ece660eadc165be34995255fae6f56c9cb3d8d6d96bd00fe2902e582`.

Licence: [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/).
The dataset's licence is supplied in its `LICENCE.txt` and OSF record.

`Source/DSP/MeasuredSteelBridgeData.h` contains Acustra's transformed modal
coefficients from the first `specSet` column in `bridge_admittance_all.mat`:
the commissioned Fylde Falstaff with Sitka spruce top, Brazilian rosewood back
and sides, ebony bridge and Elixir Nanoweb Light 80/20 Bronze steel strings.
The manufacture year is unspecified. The measurement is normal bridge
velocity/force between the fifth and sixth strings, with strings damped.

Acustra selects measured peaks, pins the three lowest body frequencies and
Q values to Table I, infers a polarity and phase alignment from a constrained fit,
and fits nonnegative scalar residues while retaining measured SI magnitude.
The bank supplies no measured rocking, cross-admittance or body radiation.
The source MAT, recorded/synthesized audio and experimental participant data
are not distributed. This attribution, licence link, source link and
description of changes must accompany distributions of these coefficients.

## DAFx-26 EJ45 construction data

Michele Ducceschi, Riccardo Russo and Craig J. Webb, *Measurement-Informed
Nonlinear Modal Synthesis of 65 Classical Guitars*, Proceedings of the 29th
International Conference on Digital Audio Effects (DAFx-26), 2026.

Source: <https://dafx26.mit.edu/assets/papers/DAFx26_paper_40.pdf>

Licence: [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/)

The EJ45 diameters, effective densities and Young's moduli in
`Source/DSP/AcustraEngine.cpp` reproduce Table 1, reordered from the paper's
high-E-to-low-E presentation into Acustra's low-E-to-high-E engine order.
Acustra's loss and excitation laws are not taken from that table. The paper's
measurement-to-modal procedure also informs the Mores-data adaptation described
above, but no coefficient from the paper or its companion audio is included.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

A copy of the JUCE licence text is vendored at
[`ThirdParty/JUCE-LICENSE.md`](ThirdParty/JUCE-LICENSE.md) so it can accompany
every distributed plug-in bundle, application and installer package.

JUCE framework modules are dual-licensed under the GNU Affero General Public
License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing Acustra with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE
licence. Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

JUCE includes or interfaces with additional third-party components. Their
copyright notices and licence terms are listed in JUCE's own `LICENSE.md` and
source tree. The VST3 SDK portions used through JUCE are identified there as
MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS SDK and
remain subject to Apple's terms.

Acustra's source tree and offline reference tools include the three CC0
recording families described above. The distributed plug-in and standalone
binaries include none of their recorded audio, and include no convolution
impulse response, neural model or added room/reverb capture.

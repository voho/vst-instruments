# Electry — decision log

Directions chosen by ear, recorded per the A–Z listening-test convention in
the repository's `CLAUDE.md`. A choice made by ear is recorded as made by ear,
never written up as though a measurement had settled it.

One informal by-ear observation is now recorded: the user described the
finite-contact Palm renders as “squishy” and short of body. It is product
feedback, not a blinded A–Z result. The correction below was selected with
same-build waveform, spectrum, envelope and regression comparisons rather than
recasting that observation as a measurement. Where a constant is voiced rather
than literature-derived it is labelled as voicing in the
[claim boundaries](../README.md#references-and-claim-boundaries).

## 2026-08-27 — solve measured output-tube families, not opposed preamp curves

The first American/British checkpoint openly used opposed evaluations of the
measured 12AX7 preamp transfer to approximate a balanced output pair. That
closed the three-path product surface but left the output stage's actual tube
family, class-AB overlap, knee, load line and supply current absent. Decision:
replace only those two approximations with the measured-curve-fitted uTracer
[TubeLib](https://www.dos4ever.com/uTracer3/TubeLib.inc) BTetrodeDE 6L6GC and
BTetrodeD EL34 formulae. Use the source-documented 450 V plate / 400 V screen /
−37 V / 5.6 kOhm 6L6GC AB1 pair and Mullard 400 V / −36 V / 3.5 kOhm EL34
fixed-bias pair. An ideal balanced phase splitter and Raa/4 centre-tapped load
are explicit remaining approximations.

Generate a bounded 4,097-drive × 17-rail table for each family during
preparation, normalise only their shared small-signal slope, and interpolate
output plus incremental plate-and-screen demand in the oversampled path. Drive
stops at the zero-grid AB1 boundary (normalised ±1), so neither control grid
becomes positive without a model of its driver and coupling capacitor. The
supply follower now changes plate and screen rails inside the load-line solve
and follows current demand rather than rectified audio. Preserve the existing
negative-feedback loop, sag time constants, transformer, cabinet, selector and
level trims; preserve the complete Modern branch operation-for-operation.
TubeLib's fitted SPICE macro subtracts its secondary-emission term from plate
current without adding it to screen current, unlike the companion derivation;
reproduce the fitted macro exactly and regression-pin that choice rather than
silently making an unfitted charge-conserving variant.

The off-grid table sweep stays within 0.000086 normalized output and 0.000093
demand error. Equal-slope half drive produces 0.48185 for 6L6GC and 0.643711
for EL34. Full-path alias floors are −62.17/−68.17/−61.28 dB,
loud-versus-quiet compression is −13.87/−15.43/−18.33 dB and the 90% Drop-E
fixture is +2.88/+6.57/+5.87 dB across American/British/Modern. Demos 01–22
remain sample-identical through the untouched Modern path; demo 23 is
regenerated with the new A/B stages and the same single global normalisation.
No A–Z choice was needed: measured model identity and published load-line
operating points settle this topology correction. Future grid-current/coupling
cap memory, nonlinear phase splitting, the Mullard screen resistor and a
speaker-reflected reactive load remain circuit questions, not constants this
decision invents.

## 2026-08-27 — select complete amplifier families, not post-EQ presets

*Historical checkpoint: its opposed-12AX7 American/British output approximation
is superseded by the measured 6L6GC/EL34 decision above; its selector, tone
stacks, feedback, transformer, cabinet and Modern-path decisions remain.*

One high-gain path could not provide a convincing clean American response, a
British crunch transition and Electry's established tight metal voice by
changing one drive law or placing EQ after a shared clipper. Decision: append
one `ampModel` choice and give each selection independent recursive preamp,
output, sag, transformer and six-section speaker/cabinet state. Crossfade the
complete paths with a 15 ms exponential smoothing time constant during a
change, then clear the faded circuit after its weight settles and run only the
selected one. Keep the default Modern and explicitly migrate a
development state that lacks the new field to Modern; the first 27 host indices
do not move.

American and British use the exact third-order Yeh/Smith unloaded passive tone
stack for documented component families. All voices reuse the existing dense
measured-12AX7 load-line table. The two new output sections evaluate that same
triode curve on opposed drives, with a deliberately small British imbalance,
causal output-derived negative feedback, independent supply followers and
transformer-flux states. Their output pair is an approximation: it is not a
phase-splitter, 6L6, EL34 or pentode solve. The six-section speaker/cabinet
responses are parametric voices, not loudspeaker mechanics, impulse responses,
microphones or rooms. These boundaries stay visible rather than borrowing the
identity of a cited Fender-, Marshall- or 6505-family product.

The existing Modern arithmetic remains a dedicated branch so its output and
demos 01–22 stay byte-identical. At 90% Amp, the same Drop-E fixture measures
+2.98/+4.81/+5.87 dB gain across American/British/Modern, while the loud-versus-
quiet compression ordering is −5.59/−10.80/−18.33 dB. All three stay below
−61 dB on the alias probe and within 0.05 dB from 48 to 384 kHz on the pinned
2.8 kHz/1 kHz response. Demo 23 uses a fresh instance for each model and one
global normalisation so the comparison does not hide their nonlinear level
behaviour.

## 2026-08-27 — keep enabled gain circuits in series

Distortion and Amp are labelled as drive/amount controls, but each smoothed
value was also the dry/wet coefficient around its complete module. The 22%
"clean amp" in demo 20 therefore sent 78% uncabbed DI around the loudspeaker;
even the 95% metal amp leaked 5%. A 5% Amp setting retained only 5% of the
52 Hz input network, tube stages, transformer and cabinet, and a 5% Distortion
setting retained only 5% of its coupling, voice and diode circuit. That is a
parallel studio blend, not the signal path of an enabled pedal or amplifier.

Decision: keep zero as the exact, cost-free bypass, but separate each gain
control's smoothed drive from a binary module-engagement target. The existing
15 ms exponential smoother now moves a relay-like wet coefficient only when the control
crosses zero; at every nonzero steady setting the signal passes through the
whole enabled circuit. The overall 12 ms oversampling engagement ramp, private
state resets and level-compensated drive laws remain unchanged. No parameter,
state migration or extra signal path was added.

The regression measures small-signal response at the UI's minimum 0.1% drive.
Relative to 1 kHz, the then-sole Amp path—now Modern High-Gain—still removes
22.003 dB at 8 kHz through its cabinet; the pedal
still removes 18.182 dB at 40 Hz and 20.961 dB at 12 kHz. Each is within 0.5 dB
of the same circuit at full drive, which a dry leak cannot satisfy. The full
aliasing, level, transition, exact-bypass, inactive-state and eight-rate rails
remain in force. The affected canonical demos are regenerated because this is
an actual topology correction, not a metadata change.

## 2026-08-27 — keep dedicated repicks in the picking hand

E6..B6 and B0 are explicit picking-hand commands, but a latched Hammer was
passed through as their attack style and turned every requested repick into a
finger tap. The manual trigger and B0 both advanced note order while two
successive Alternate contacts nevertheless stayed Down; neither produced
plectrum contact or contact loss, and Pick Noise at 0 and 1 rendered
byte-identically. On a two-string shape with 3 ms Strum Spread, B0 scheduled
both strings with zero delay instead of one travelling pick. The same path also
excluded pick noise and every other ordinary picked-contact detail.

Decision: only when a dedicated E6..B6 or B0 repick enters the attack path,
interpret Hammer as the neutral Sustain pick contact. Leave the global Hammer
latch unchanged, so a later playable Note On still performs a genuine hammer or
pull-off. Slide and every already picked style keep their existing semantics.
The correction reuses the attack path's local style and adds no state, control
or new articulation.

The regression holds one stopped note under Alternate and a Hammer latch, then
requires successive manual repicks to make Up/Down Sustain contacts, advance
note order, preserve the fretting finger and leave Hammer latched. Separate
rails require Pick Noise to affect the audio, B0 to make the same contact on its
next sample, and a two-string B0 stroke to retain one shared direction,
variation state and distinct nonzero 3 ms travel delays. Demo 22 now picks MIDI
74, genuinely hammers to 76, leaves Hammer latched while B0 continues with
picked Sustain contacts, then restores Sustain; its duration is unchanged.
This is a hand-ownership invariant rather than a voicing choice, so no A–Z
preference test was used.

## 2026-08-27 — let vibrato begin and end with a fretting finger

A#0 owned one shared target and onset envelope even when no stopped key owned a
fretting finger. After 500 ms of pre-hold, the first stopped note began about
10.04 cents high and reached 38.04 cents at 60 ms; starting A#0 with that note
began at 0 and reached only 4.75 cents at 60 ms. Open strings aged the same
hidden envelope. At the other boundary, an ordinary released note reached
38.60 cents within 60 ms, a sustain-held tail reached 50.97 cents within 500 ms
and still held 35.24 cents, and releasing A#0 with the fretting key in the same
event still produced 37.19 cents within 60 ms. A final Note Off between control
ticks could retain its old pitch offset for up to seven host frames at 48 kHz.

Decision: A#0 remains shared performance intent, but its onset advances only
while an active, key-down, stopped voice supplies a physical finger. A released
or sustain-held tail and an open string carry no vibrato offset. Releasing one
stopped key clears that voice while a still-held stopped sibling keeps the
shared onset; releasing the final one immediately zeros the onset and every
voice offset without clearing A#0's target or balanced ownership. A fresh or
off-grid same-boundary refret and stopped-to-open-to-stopped legato therefore
start from rest. A repick or overlapping owner of the same held or delayed note
keeps the onset already under way. This uses the existing active/key/fret
lifecycle and changes no rate, width, phase draw or persistent state.

The regression holds A#0 through silence and an open string, then compares its
first stopped-finger control tick with a coincident note/gesture at 44.1, 48,
96, 192 and 384 kHz. At 48 kHz it separately pins a sustain-held released tail,
a still-held sibling, final-key release with the A#0 target intact, an immediate
off-grid refret, stopped-open-stopped Hammer/pull legato and overlapping
ownership of a delayed same-note refret. Demo 22 now pre-holds A#0 through a
240 ms rest before the next stopped note demonstrates the fresh bloom. This is
a physical ownership invariant rather than a voicing choice, so no A–Z
preference test was used.

## 2026-08-27 — keep ringing pitch continuous through damping refits

The bridge-hand controls changed the one-pole and hand-loss filters of an
already-ringing loop, then recomputed its compensated delay target. They did
not move the loop's current raw-delay coordinate by the corresponding filter-
phase difference. The ordinary six-millisecond delay smoother therefore
treated a damping change as a short pitch gesture. A shared E1 Sustain-to-Palm
contact moved the vertical and horizontal effective periods by about -22.2 and
-11.9 cents; lifting that contact moved them by about +21.7 and +11.6 cents.
Direct hard CC2 state probes could exceed a semitone. The magnitude varied with
the note and damping state, but the defect existed at every tested host rate
from 44.1 through 384 kHz.

Decision: cache the nominal period represented by the last analytic phase
compensation. On a live damping-only refit, recover the previous damping phase
from that period and the old compensated/dispersion terms, evaluate the new
damping topology at the same frequency, and translate each polarisation's
current raw delay by the old-minus-new difference. Dispersion-grid translation
remains a separate additive term. A simultaneous bend, vibrato tick or refret
changes only the new target, so genuine performed pitch motion retains its
existing smoother instead of being snapped into the continuity correction.
Fresh voices and forced delay jumps keep their existing initialization path.

The regression establishes a ringing voice, then checks both effective loop
periods immediately after shared Palm/Open contacts; hard CC2 0-to-127 jumps on
E1 and B2; every boundary of an adjacent 0-to-127-to-0 CC2 sweep; and full CC2
motion during the minimum supported 40 ms pitch-bend glide. The contact and
individual sweep-step rails are below 0.25 cent, the cumulative/reference and
combined-bend rails are below 0.5 cent. Contact, hard-jump and sweep coverage
runs at 44.1, 48, 96, 192 and 384 kHz; the minimum-time combined glide runs at
44.1 kHz.

Canonical same-renderer comparison changed demos 02–05, 08–20 and 22; demos
01, 06, 07 and 21 stayed byte-identical. Demo 04's dry Palm lift/replant and
demo 15 expose the correction directly, while demos 18 and 20 exercise it in
high-gain arrangements. This was a numerical complete-loop pitch invariant,
not a defensible choice between voicings, so no A–Z preference test was used.

## 2026-08-27 — keep amplifier-feedback delay independent of DAW blocks

The acoustic return used the previous host block as its air path. That made a
transport detail part of the physical resonator: on one macOS build, otherwise
identical 64, 256 and 1024-sample renders selected 2.667, 5.465 and 2.217 kHz
modes in demo 06, with raw peaks of -7.7, -9.2 and -7.0 dBFS. Demo 14's tail
selected 0.989, 2.717 and 2.744 kHz, with raw peaks of -12.5, -13.6 and
-14.2 dBFS. Every non-feedback demo remained byte-identical across the same
three partitions. A guitarist changing the DAW safety buffer had therefore
changed the simulated instrument and amplifier geometry.

Decision: retain the voiced 256-sample path at 44.1 kHz as a
sample-rate-derived 256/44100-second FIFO. Its 5.805 ms duration corresponds to
roughly two metres of free-air travel, but it is explicitly a voiced nominal
distance, not a measured player/room position. The plug-in processes the
engine, effects and acoustic return in causal chunks no longer than that delay;
short MIDI-event segments append to rather than discard the unread FIFO. Large
host callbacks are subdivided, so the nominal delay remains fixed without a
new control, room model or signal path.

An 8,192-sample stereo plug-in regression places CC1 and Note On at absolute
sample 137 and Note Off at sample 4099. Its output is bit-identical with 64,
256 and 1024-sample host callbacks, including the non-aligned event splits.
Forcing each callback back through one unsplit chunk fails that exact rail.
One-sample process/return probes keep the FIFO exactly full and return an
impulse at the derived delay on 44.1, 48, 96 and 384 kHz hosts. The corrected
demo renderer is byte-identical across the three audited partitions; only
demos 06 and 14 change against the same-platform previous build. This does not
claim measured room acoustics, speaker directivity or standing-wave behavior.

## 2026-08-27 — keep amplifier feedback on the performed pitch class

The acoustic return drove every idle sympathetic string as hard as the string
the player had actually sounded. Those eight parallel resonators did not model
normal high-gain playing technique, where spare fingers and the picking hand
control unused strings. In demos 06 and 14, the seventh mode of the untouched
open high E near 2.31 kHz consequently displaced the performed note's howl. In
demo 14, the B2-derived mode near 2.72 kHz led as feedback opened, then the
unrelated high-E mode took over for the rest of the audible sustain.

Decision: played or releasing voices retain the full bounded loudspeaker
return. An idle sympathetic loop receives one quarter of that direct return,
a voiced partial-isolation assumption for controlled metal technique. Its
ordinary bridge-bus drive is unchanged, so idle strings still answer the
instrument and the CC1 coupling lift still reaches its original endpoint. No
state, control, filter or extra signal path was added. This is not a measured
finger-by-finger mute model, speaker-directivity model, or guarantee that a
particular physical voice owns the tail after Note Off. It reduces unrelated
open-string takeover; the regression pins the held A4 case and the two affected
demos pin their audible pitch families rather than claiming a universal
all-note, all-rig guarantee.

During coefficient selection under the then-current block scheduler, the
44.1 kHz closed engine/amplifier fixture left A4 held on the G string after
releasing the upper notes of a three-note shape. At one-half idle drive, the
open high E still won 0.452 to 0.115 in loop amplitude; at one quarter, A4 won
0.553 to 0.060 and cleared a two-to-one ownership rail. Equal direct drive lost
0.764 to 0.117. Zero direct drive was rejected because an open string can
plausibly answer the loudspeaker. Same-build demo comparisons at that decision
kept demo 14 on its B-derived 2.72 kHz family and demo 06 on the fretted A4
harmonic family until the wheel closed; their scores and durations were
unchanged. The fixed-delay scheduler above retains the two-to-one rail; its
current quarter-share margin is 19.1 dB.

## 2026-08-27 — continue chained legato from the live fret and loop period

`legatoRetarget()` began every new Slide or Hammer at the preceding gesture's
destination, even when the finger was still travelling toward it. At 48 kHz,
an A2-to-A3 Slide retargeted after 100 ms had reached programmed MIDI 48.7467,
but a second gesture first jumped to the abandoned MIDI 57 target: an
825.33-cent teleport. A reachable Hammer to MIDI 52 was consequently
misclassified as a pull-off, and a second Slide timed and voiced its scrape over
five stale frets instead of the 3.253 frets actually left under the hand.

The logical correction exposed a second discontinuity in the physical loop.
Changing fret/style re-solves damping and dispersion, whose phase is part of
the sounding period; keeping the raw delay-line coordinate fixed therefore
moved a settled slide onset by -48.93 cents and a chained retarget by +45.55
cents. A later dispersion-grid refit in demo 17 also stepped 5.890 cents against
the direction of travel even though its programmed smoothstep was monotone.
The same mechanism was not legato-specific: at a 44.1 kHz host, a rising
two-semitone wheel glide stepped backward by as much as 4.62 cents and drew down
11.92 cents cumulatively as it crossed quantised dispersion fits.

Decision: before replacing a legato target, evaluate the existing smoothstep at
its live log-frequency position. That frequency and corresponding fractional
fret become the new source; distance, physical travel, friction speed and
Hammer/pull direction all use it. Preserve the raw delay's residual from its
phase-compensated target across the event, and translate the raw coordinate by
the opposite phase change whenever a non-forced dispersion refit occurs. Thus
total delay plus filter phase remains continuous through legato and wheel
motion. A settled gesture retains the old integer/base path exactly; pitch bend
and vibrato remain separate control offsets. No voice state was added. This
guarantees continuous finger position and effective pitch, not a speculative
velocity-matched spline or an unchanged raw read coordinate.

The regression covers settled-to-Slide, Slide-to-Slide, Slide-to-Hammer and
Palm-held Hammer/pull/Slide handoffs after their retained damping is restored.
Both polarisations keep event pitch within a 2-cent effective-period guard
(programmed pitch within 0.1 cent); the ascending Hammer keeps its impact
orientation; the second Slide uses the live 3.253-fret remainder and both
gestures arrive at MIDI 52. The exact demo-17 setup redirects its final descent
after 51 ms, at live MIDI 72.0264. The old score-matched engine first programmed
the abandoned MIDI 68 target (-402.64 cents); the corrected event is 0.000 cents
and reaches MIDI 69 in 38.7 ms. All 214 control ticks descend, including the
former bad refit, which is now -1.522 rather than +5.890 cents. The wheel
regression likewise requires a rising effective frequency with less than 0.05
cent cumulative drawdown. All seventeen demos whose reused strings or
continuous pitch motion cross a dispersion refit were regenerated: 02-06, 09,
11-15 and 17-22; only demo 17's score changed.

## 2026-08-27 — keep the bridge hand planted through legato

`startExcitation()` treated every Hammer, pull-off and legato Slide as a new
shared-hand contact even though the same function correctly classified those
gestures as having no plectrum. After a palm-muted chord, one moving fretting
finger therefore lifted the picking hand from the bridge, reopened its own
string and rewrote every untouched sibling's damping.

Decision: a non-plectrum fretting gesture retains an existing Palm hand. The
target keeps Hammer or Slide as its attack descriptor, but its loop is re-solved
at the new fret with Palm damping. It does not advance shared-hand contact
ownership, touch sibling loops or clear their point contacts. A real later
Sustain, Mute or Dead pick still moves the shared hand normally. Sustain-to-
legato remains open, and a stopped finger still replaces the fretting-hand Dead
choke as before; continuous Palm Pressure remains an independent contact.

The 48 kHz regression holds Palm E1/E2, then covers an ascending hammer-on, a
pull-off to open E1 and a legato Slide. Before the correction, the target's live
hand-loss depth fell from 0.144489 to zero and the untouched sibling's from
0.074465 to zero. Each case now preserves the sibling's loop gain, damping
coefficient and both hand-depth states exactly, while a subsequent real Sustain
pick reopens both strings. Demo 03 now explicitly picks each Hammer example's
base note instead of inheriting the preceding catalogue style. Demo 15 adds a
Palm-held octave hammer/pull; a score-matched render is identical through the
finger contact at 3.11034 s, then retains 12.3 dB more high-band attenuation in
the hammer window and 15.8 dB more in the pull-off window.

## 2026-08-27 — let a travelling pick reach the moving fret

`legatoRetarget()` treated every Hammer or Slide as authority to erase the
voice's delayed attack state. If a repeated chord had already reserved a
same-string plectrum contact, moving that fret before contact cleared the
pending stroke, its remaining Strum travel and its chord identity. The finger
therefore made a physically committed pick disappear.

Decision: a pending repick remains owned by its original wrist stroke. If a
fully damped held voice has only a fresh delayed contact, promote its cached
wrist state to the same reservation before the finger changes it. The legato
gesture immediately changes the speaking length and contributes its ordinary
finger impact or slide friction; the plectrum retains its captured style,
direction, player variation, remaining delay and chord identity, then strikes
the voice's latest moving pitch without finishing the glide early. A legato
move with no pending plectrum keeps the previous behavior and creates no pick
of its own.

The regression reserves a two-string, 40 ms Strum repick, moves the second
string from B1 to C#2 with both Hammer and Slide 5 ms before contact, and
requires the captured Sustain plectrum to arrive on the exact internal-sample
boundary while the glide remains unfinished. A second case revives a retired
held Dead B1 and slides it to B2 before its fresh delayed pick, pinning the same
payload and ownership path. A released/refretted note that moves before contact
draws its new finger at the legato event and retains it through the pick rather
than reviving the released finger. Demo 22 moves its closing low-string slide
5 ms beyond the B0 grid point so the collision is audible while the repeat is
still travelling.

## 2026-08-27 — keep the live fretting finger through a repick

Every same-string plectrum contact re-entered `startVoice()` and seeded a new
vibrato finger from the pick's note order. B0 therefore replaced phase, rate
and excursion on every repeat even though the fretting key never moved. In
demo 22's 12-strokes/s lead, the reset arrived every 83.3 ms while one nominal
full-pressure finger cycle takes about 156 ms; the finger could not complete a
single natural rock before the next pick assigned another one.

Decision: read the physical voice's live key ownership before restarting its
string excitation. A same-note contact on that live, held string advances every
plectrum and string state while retaining the finger's seed, phase, cycle,
rate, depth and current excursion. This covers immediate and delayed E6..B6
contacts, B0 and overlapping same-note owners through their shared repick path
while the voice is still ringing. A Note Off followed by a reattack, a new fret,
a different physical string or a repick after the damped voice has retired
assigns a new finger.

This changes no vibrato rate, depth, scatter or onset coefficient. The
regression requires all six finger-state coordinates to survive a new audible
pick order and separately requires a released/refretted note to reseed. Across
demo 22's 18 post-vibrato contacts, the old reset implied 3.682 cents median,
8.867 cents RMS and 18.919 cents maximum discontinuity; the retained state
makes that reset component exactly zero while the finger completes eight
natural cycle wraps. The repicked lead is the direct audible proof.

## 2026-08-27 — keep one picking hand across a chord

Stroke variation described one player's pick position, force, angle and contact
width, but the engine drew all four independently for every string voice. On a
default-seed, equal-velocity open-string fixture `{28, 35, 40, 45}`, one wrist
therefore spanned 6.790 mm of pick position, 0.723 dB of force, 9.859 degrees of
angle and 13.262 percentage points of contact width inside a single chord. The
downstream strings were physically different, but the player driving them had
also become four unrelated hands.

Decision: draw those four latent player coordinates once from the first
accepted plectrum event and share them across every string the wrist crosses.
Keep each string's gauge-, pitch-, velocity- and style-dependent excitation
mapping independent. Preserve the former hash and draw order for the first
member, every solo note and both deterministic player seeds; keep every
per-string `startOrder` increment as before. A delayed same-note repick carries
its originating stroke state until contact, so a newer chord cannot overwrite
the hand already travelling toward it. Hammer-ons and legato slides neither
consume nor apply the plectrum state.

This is an ownership/topology correction, not a voiced coefficient choice: the
4 mm, 0.6 dB, 6 degree and 8% distributions are unchanged. The regression
covers complete and scalar chords, repeat variation, a newer-stroke/pending-
contact overlap, legacy solo draws and the separately seeded Double player.
A same-platform before/after render changed exactly the twelve scores containing
multi-note contacts—demos 01, 07-09, 11-14, 18-20 and 22. The other ten demos
and all ten raw E1/E2 evaluator probes remained byte-identical.

## 2026-08-27 — keep the fretting hand out of B0's wrist clock

The same-boundary duplicate guard treated every playable Note On as a pick.
With two strings held at 12 strokes/s, a legato Slide on the low string at the
4,000-host-sample boundary therefore consumed the Down contact due on the
unchanged high string. That string stayed Up until the following grid point: a
166.7 ms gap in an 83.3 ms picking cadence.

Decision: remember the latest physical plectrum contact separately from the
latest playable Note On. Update that clock in `startExcitation`, where delayed
Strum travel reaches the string and the existing contact predicate has already
distinguished the two hands. A played pick on B0's boundary still consumes the
automatic contact; a hammer or legato slide does not. The regression slides one
member of a held two-string shape on the exact boundary and requires the other
member's Alternate stroke immediately. Demo 22 exposes the same case in its
closing Drop-E chord.

## 2026-08-27 — make a played note the tremolo wrist's first contact

B0's fractional wrist clock kept running even when no string was physically
held. If the fretting hand entered near the end of that empty cycle, the normal
played attack could be followed by an automatic repick only a few samples
later. At 48 kHz and the default 12 strokes/s, holding an empty wrist for 3,950
of the 4,000 host-sample interval reproduced a second contact about 50 samples
after the note: a phase-dependent flam rather than one picking cadence.

Decision: the leading physical plectrum contact of a new playable stroke
re-anchors the active B0 phase. Use the scheduler's existing first-contact flag,
so a delayed strum resets at contact rather than MIDI reservation and later
strings in the same traversal do not restart the clock. B0-generated strokes
retain their fractional phase through delayed Strum contact, so the fix cannot
slow its own cadence. Hammer-ons and legato slides remain fretting-hand gestures
and do not move it. A regression holds B0 in silence until just before its
boundary, then requires one full interval from the played Down contact to the
next Up contact. Demo 22 now enters its moving 16-stroke/s line with the wrist
already held just short of one interval.

## 2026-08-26 — retire the finite Palm heel and restore low body

The provisional 4 mm Palm heel had been added on top of an existing bridge-hand
model that already supplied selective steady loss. The user then reported that
the rendered chugs felt squishy and lacked body. A controlled source-history
A/B confirmed the mechanism: the finite-contact commit moved demo 04's
30-80/0-30 ms body from -5.36 to -6.77 dB, reduced its 80-500 Hz share from
54.68% to 39.51%, and raised crest from 13.74 to 14.66 dB. It improved one
two-player F2 selective-loss proxy, but stacked a 100 ms transient loss on the
steady hand and contradicted the broader low-body result.

Decision: remove the Palm-only finite-contact path, its width state and six
extra cubic reads. Retain the existing additive hand-loss law, velocity latch,
Mute Tightness, Mute Pressure and short pick-impact path. Raise only Palm's
existing modal-excitation factor from 0.74 to 0.85; do not add output gain,
another resonator, a control or a dependency.

At the shipping default, the raw evaluator's E1/E2 30-80 ms body moves from
-5.4608/-4.8192 to -4.1939/-3.2506 dB and peak rises 2.23/2.44 dB. Open and
Dead evaluator WAVs are byte-identical. The noise-free guard still measures
9.397/14.770 dB more high- than low-band Palm loss over 150-500 ms, keeps the
early attack darker than Open, and preserves strict Light -> Medium -> Hard
ordering. The high-gain rapid-Mute body remains above its 6% upper-share floor.

The cost is explicit: the controlled F2 paired contraction retreats from
-4.009 dB to -0.681 dB at 44.1 kHz, versus two public cells at -6.099 and
-15.290 dB. Two conventional-guitar player cells are useful direction evidence,
not a defensible coefficient; optimizing that sparse proxy while the rendered
instrument lost low body was the wrong trade. The test now guards negative
selective direction and 44.1-192 kHz invariance, plus independent long-band and
E1/E2 body floors. Commissioned exact-eight TRAIN/HOLDOUT captures remain the
only gate allowed to fit a stronger time-varying contact.

## 2026-08-26 — start a complete strum at its known edge

The processor already collects all ordinary positive Note Ons at an exact MIDI
timestamp and passes them through the engine's bounded whole-chord allocation
solve. That solve knows every assigned physical string before any voice starts,
but the strum scheduler still treated the group like an incomplete scalar
stream: any nonzero Strum Spread charged its fixed 20 ms re-anchor pre-roll to
the leading string. A single-note prog-metal line therefore felt 20 ms late even
though there was no second string to wait for.

Decision: an exact-sample `noteOnChord` group is one complete performed stroke.
Pre-anchor its picked edge from the solved string assignment, start that edge at
the MIDI timestamp and retain the existing accelerating travel for every later
string. A one-note group has no crossing and is a sample-exact spread no-op. A
group at a later timestamp is a new stroke even inside the scalar chord window,
so Alternate advances and deliberate sequencer timing remains timing. Keep the
20 ms pre-roll only on the public scalar note path, where a later call can still
causally reveal an earlier edge; do not add plug-in latency or a new control.

The regression pins down/up edge order, unchanged seven-crossing duration,
one-note identity, hostile render-block partitioning, later-timestamp Alternate
advance and early release of an un-crossed string. The demo renderer now sends
its explicitly written chords and individual note boundaries through the same
complete-batch path as the plug-in. This is a scheduling/playability correction,
not a by-ear voicing decision.

## 2026-08-26 — add one visible tremolo wrist; preserve one-shot repicks

The official workflow audit found three commercial answers to fast repetition.
Shreddage Hydra and Electric Storm Deluxe expose looping recorded tremolo;
Dracus, Hellrazer and Heavier7 expose manual repeat gestures and/or pattern
lanes; RealEight is the only audited product with a clearly documented
automatic host-synchronised subdivision generator. The strongest direct-rate
study found is Armondes' 2026 UFMG experiment: twenty direct-at-maximum takes
from five conventional-guitar players place the plotted median IOIs roughly at
70–90 ms, or about 11–14 attacks/s. That is useful scale evidence, not an
electric eight-string fit. No commercially reusable exact-eight tremolo corpus
was found; EG-IPT supplies CC-BY electric-guitar tremolo only on its published
six-string set.

Decision: expose B0/MIDI 23 as a visible momentary **TRM** gesture. Note On
velocity remains pick force; balanced Note Off stops future contacts and lets
the string ring. One shared fractional wrist phase repicks every physically
held string through the existing E6..B6 contact path, so Alternate, Play Style,
Strum, variation, fretting ownership and Double keep one implementation. A
same-boundary playable Note On is already the first contact. A due tick is
skipped while a prior Strum traversal is still travelling, preventing a newer
repeat from replacing a contact that has not reached its string.

The new `tremoloRate` parameter is deliberately free-running 4–20 strokes/s,
defaults to 12, and is appended after the existing automation list so all old
host indices remain stable. The exact-eight commissioning protocol's 8/12/16
anchors correspond to sixteenths at 120/180/240 BPM. Keep E6..B6 one-shot and
their Note Off inert: changing their established long-gate semantics would be
a needless compatibility break. Defer host-transport divisions, a pattern
editor, random timing, missed strokes and direction-level bias until licensed
captures establish what should vary. B0 and CC120/121 preserve a physically
held wrist; CC123, Panic, prepare and release clear it. The gesture is not saved.

## 2026-08-26 — diagnose the low-string returned crest; retain its path timing

The GPL-3.0 8ridgelite E1/E2 sustains put their global 2 ms RMS peaks about
1.3-2.4 ms after audio onset, while Electry's occur 25.8 and 12.8 ms after
onset. The model is not silent until then: its first-lobe crests arrive at 1.43
and 1.68 ms, but a returned crest wins by 1.20 and 2.77 dB. A code trace explains
the octave-scaled delay. A pluck at `pL` from the bridge produces one wave that
travels directly bridgeward and a second that returns from the nut; at a pickup
between bridge and pluck, their arrival separation is `(1-p)T`. The folded loop
realizes that path with its image and one loop-filter pass. The predicted
25.2/12.6 ms return matches the measured crests closely; pick-contact duration
and pickup travel do not explain them.

Decision: retain the shipping excitation and its path timing. The geometry is
physically plausible; the unvalidated part is the returned path winning over
the first by 1.20/2.77 dB. Published bidirectional waveguides split an ideal
pluck into both travelling waves, while single-delay reductions may express
the position effect as a pre-loop comb; their ideal-harmonic magnitude
equivalence does not identify the better transient phase at Electry's filtered,
inharmonic pickup output. Complementing the history offset is at most a
non-shipping diagnostic, not a presumed fix. One undocumented sampled stroke
per pitch cannot determine path balance, termination loss or excitation phase.
First commission multi-player TRAIN/HOLDOUT captures and compare first-crest
timing plus returned/first ratio before changing the core transient.

## 2026-08-26 — expose the existing picking hand on the live fretboard

The current official-product audit found that leading metal guitar libraries
make direct string strikes and repetition controls discoverable. Electry
already had a tested, velocity-sensitive E6..B6 lane that repicks a physically
held string without changing its fretting owner, but the on-screen piano ends
at the highest real pitch so that lane existed only in documentation and
external MIDI.

Decision: let a click on any live-fretboard row send one hard attack through
the processor's existing bounded UI queue and repick path. The section title,
accessible title and tooltip explain the gesture; an unheld row remains silent.
External MIDI retains velocity and exact sequencer timing. Do not add another
engine path, articulation, host parameter or pattern workstation for this
surface. Host-tempo synchronisation and a pattern lane remain separate workflow
candidates, not claims made by a mouse click or the later free-rate B0 wrist.

## 2026-08-26 — keep fretting-hand gestures out of the plectrum path

The engine already had one predicate saying whether a plectrum physically
contacts a string, but the note scheduler and parts of the excitation did not
consistently use it. With Strum Spread enabled, a fresh Hammer/tap waited for
the wrist's pre-roll and Double-player offset, entered the pick-contact loss
phase, and still changed with Pick Hardness. A legato Slide could re-anchor a
real picked chord that was already in flight and added a small destination
pluck despite its continuous-string design.

Decision: use the existing contact predicate as the boundary for wrist timing,
chord anchoring, Alternate-stroke consumption, contact loss and plectrum-only
controls. A Hammer or pull-off retains its established default finger-impact
voicing, but Pick Position, Pick Hardness, Pick Noise, Strum Spread and Double's
player pocket cannot change when that contact occurs. A Slide that begins a
phrase is still picked; a Slide on a ringing string preserves the loop and adds
only its speed-dependent finger friction. No parameter, articulation or hidden
controller was added.

The Guitar-TECHS vibrato audit in the evaluation found one usable conventional
six-string player cluster and one too shallow/incoherent for a stable fit. A
second published UFMG study contributes four performances each from eight
guitarists on one DI-captured `.010`-strung Stratocaster: its long-note means
span roughly 4-8 Hz and distinctly narrow-to-wide hands. Together they place
the existing 5.6 Hz / 20-cent mid-velocity gesture and 6.4 Hz / 40-cent full
gesture inside a real six-string range, but still do not justify coefficient
retuning or an exact eight-string claim.

The extended solo and blues-lead demos made the remaining product problem
concrete: an audible technique shown by the instrument was not playable from
the instrument. Decision: preserve the DSP coefficients and expose A#0/MIDI 22
as a visible momentary **VIB** key, with Note On velocity controlling amount
and balanced Note Off ownership. It conditions a whole simultaneous chord,
always mirrors both Double engines, preserves a physically held key through
CC120/CC121 and clears on CC123, Panic, prepare and release. Channel and
polyphonic pressure stay unassigned, and no host parameter or new panel control
is added. Exact eight-string lead captures remain the calibration gate, not the
gate for making the existing gesture understandable and playable.

## 2026-08-26 — give Double a player's clock; defer a new repick contact

Four CC-BY [HiMMP “In Solitude” rhythm DIs](https://himmp.net/faq.html)
provided 1,844 four-way matched energy-rise onsets under the frozen exploratory
detector recorded in the evaluation. The six pairwise absolute-timing medians
span 5.99-7.98 ms and their 90th percentiles span 11.97-17.96 ms.
They are conventional six-string Drop-C takes, not a licensed eight-string
calibration set, so they establish nonzero human-take timing without setting a
full-width target for Electry. The closest exact-performance lead,
[Ueberschall Metal Riffs](https://www.ueberschall.com/en/product/283/Metal-RiffsDOWNLOAD),
mixes 7/8-string sources and three DI performances without mapping
the eight-string groups or licensing simulator calibration; it remains a
written-permission target, not fitting data.

Decision: retain the established seed-zero player sample exactly, while the
fixed-seed second engine in Double draws one causal 0-6 ms picked-stroke offset
(3 ms mean, 1 ms standard deviation). One wrist stroke gives a whole chord one
offset and then composes with its existing string-crossing travel. Hammer
contacts are not delayed. Put this inside the physical contact scheduler, where
early releases, sustain, delayed repicks, panic and host-block crossings already
have one owner. Reject a processor MIDI queue and a static channel delay: the
former duplicates lifecycle state and the latter is a Haas copy, not another
performance. The user surface remains the single Mono/Stereo/Double choice.

The repick audit found the current event/state path correct but its legacy
whole-loop contact loss nearly inert on E1/E2. A loss-only unilateral contact
at the pick position has a pointwise passive prototype, but the available real
repetitions establish only topology, not its boundary, depth or duration.
Decision: do not ship or tune that mechanism before the commissioned dry
eight-string TRAIN/HOLDOUT captures. Preserve the existing rapid-repick rails
and the exact pre-contact state in the meantime.

## 2026-08-26 — keep displayed notes and MIDI bends in tune

A user tuning report reproduced a presentation error, not a base-frequency
failure. The saved 44.1 kHz standalone state measured 0..+4.5 cents over every
isolated MIDI note from E1 through D6 and -0.5..+6.5 cents when the same range
was played sequentially. The next eight visible piano keys, however, were the
MIDI-only E6..B6 held-string repick triggers: clicking one replayed an existing
low pitch or stayed silent while still looking like a high note. Decision: end
the on-screen piano at the highest pitched note, D6. Keep E6..B6 available to a
host or external controller as performance triggers, but do not draw them as
piano pitches.

Two hidden controller paths could independently create a real tuning failure.
The channel-wide MIDI pitch wheel had been interpreted as a physical bar, so a
full bend moved the eight strings by roughly 107..200 cents and pulled a chord
apart by as much as 93 cents. Channel pressure and even unrelated polyphonic
aftertouch globally enabled an upward-only fretting vibrato, affecting stopped
notes but not open strings. Decision: make MIDI pitch bend one uniform +/-2
semitone interval for every played and sympathetic string, and leave pressure
messages unassigned. A physical bar or pressure gesture needs a separate,
visible control rather than surprising standard MIDI input.

The saved high-gain Mute path also produced a +11.5-cent result in the broad
five-partial short-window estimator. Its dry E1 fundamental measured exactly
0.0 cents; partial-implied offsets rose from 0.0 at P1 to +16.5 cents at P5,
and later windows pulled the series estimate back toward +5 cents. Decision:
do not detune the Mute DSP to compensate for an estimator following its
deliberately reshaped, inharmonic upper partials. Mute tuning remains guarded
on the dry fundamental.

## 2026-08-26 — deepen and lengthen the fixed Palm contact (superseded)

The CC0 [`50hz-guitar`](https://freesound.org/people/inspektral/packs/42559/)
matrix adds two matched muted/sustained takes at nominal 50, 75 and 100 Hz from
a tagged seven-string baritone. Its mute hand and signal chain are unknown, so
it can establish only an extended-range spectral direction. HiMMP's CC-BY
[“In Solitude” rhythm DIs and score](https://himmp.net/faq.html) add 16 unique
low-C2 Palm strikes on the exact four-quarter-note schedule, but use a
conventional six-string Drop-C guitar. Neither source replaces the commissioned
E1/E2 TRAIN/HOLDOUT gate.

Both comparisons show the retained contact losing upper-band share too slowly.
Decision: increase the Palm contact mapping from 1.10 to 1.25 times the solved
bridge-hand depth and extend the fixed hold from 70 to 100 ms; retain the 4 mm
footprint and 10 ms release. At 44.1 kHz the controlled F2 paired contraction
moves from -3.59762 to -4.00942 dB. Across the three extended-range model notes,
the 0-60-to-60-160 ms paired contractions move from -8.31/-5.85/-7.08 to
-14.71/-9.26/-10.62 dB. The score-matched proxy's median body, tail and
selective contraction all move toward its real phrase. The rapid-repick
aggregate remains 2.240 dB median absolute mean and 3.428 dB median worst-cell
error, and the E2 soft-to-hard tail spread remains just above its 2.8 dB rail.

The bounded alternatives were:

| candidate | result | decision |
| --- | --- | --- |
| Add per-hit heel-centre jitter from stroke variation | F2 contraction weakened from -3.60 to -2.69 dB and the score correlation barely moved from 0.9969 to 0.9975; all four F2 rate rails failed | Rejected: the public audio does not identify random heel motion |
| Widen the provisional footprint from 4 to 6 mm | No material low-string improvement | Rejected: do not invent contact geometry |
| Raise depth from 1.25 to 1.40 at the 70 ms hold | F2 changed only from -4.010 to -4.021 dB and the extended matrix moved by at most 0.03 dB | Rejected: default contact was already saturated |
| Extend the hold to 120 or 140 ms | Extended-range contraction improved further, but E2 soft-to-hard tail spread fell to 2.776 or 2.754 dB against the retained 2.8 dB minimum | Rejected: spectral fit cannot erase a playable velocity range |

The complete 100 ms hold plus 10 ms release now forms the Palm CPU hot-window.
Eight active strings render it at 0.205x realtime at 96 kHz on the checkpoint
machine. The isolated all-string tuning spread remains 0..2.5 cents and the
simultaneous low chord remains -4.25..+0.75 cents.

The later Palm-body decision above removes this contact. These figures remain
only as the recorded result of the rejected development checkpoint.

## 2026-08-26 — keep the fixed Palm contact; reject an attack/recovery ramp (superseded)

At that superseded checkpoint, the finite-width contact produced monotonically increasing
Palm-versus-Open upper-band loss across sliding 30 ms F2 windows. That is the
expected accumulated action of a fixed loss inside the feedback loop; the
exact P1, P2 and Electry trajectories are recorded in
[the evaluation](evaluation.md#retired-finite-width-palm-experiment-and-current-body-checkpoint).

[Biral, d'Alessandro and Freed](https://www.icmc14-smc14.net/images/proceedings/PS4-B10-TowardsaDynamicModel.pdf)
measured pressure dips around picks, not the simultaneous acoustic loss or a
loss-recovery time constant.
[Reboursière et al.](https://www.nime.org/proceedings/2012/nime2012_213.pdf)
measured a faster post-attack high-frequency envelope, but that acoustic result
cannot distinguish changing pressure from repeated traversal of a fixed
contact. [Pluta, Tokarczyk and Wiciak](https://www.mdpi.com/2076-3417/12/3/1659)
also found that re-excited string state at 12 and 70 ms did not reduce to a
fresh isolated pluck.

Decision at that checkpoint: do not add a per-attack pressure/recovery ramp.
Its shape and timing
would be unidentifiable from the available audio, and resetting it on every
repick would be the wrong state model for rapid chugs. The then-current 70 ms
hold remained explicitly provisional and the 10 ms release was labelled as
generic smoothing, not measured Palm biomechanics. No DSP constant changed at
that checkpoint; the later evidence-backed duration decision is recorded
above.

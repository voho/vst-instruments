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

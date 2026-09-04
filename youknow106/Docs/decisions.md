# YouKnow106 — decision log

Model directions and listening verdicts. A choice made by ear is recorded as
made by ear, never written up as though a measurement had settled it, and none
of these closes an open question — the captures named under
[known gaps](../README.md#known-gaps) are still what would.

## 2026-09-04 — Identified hardware comparison and complete-voice calibration

The [hardware validation report](hardware-validation.md) supersedes the noise
certainty in the entry below. The corrected 96 kHz calibration recording from
Juno-106 #439522 (Borish replacement voice cards, serviced 2022) puts noise
relative to saw about 11.7 dB above this model, independently corroborated by
another same-gain noise recording. The pulse level differs by about 1.3 dB;
the default sub level agrees within about 0.2 dB. No source level was fitted
to this one restored unit. Original-card TP8 captures remain necessary to
resolve the noise level and original-module drive; OQ-16 is reopened as P0.
A later same-file noise/isolated-self-oscillation take confirms an 8.33 dB
noise deficit against the nominal model, removing the ambiguity of a
noise-driven resonance reference.

The recording's exact Manual SysEx messages also exposed the codec's missing
program/manual byte. New exports use documented 24-byte Manual frames;
numbered hardware dumps and earlier YouKnow106 exports are accepted. The
live MIDI path and calibration-take generator share that correction.

Separately, fixed per-card FREQ/WIDTH calibration now absorbs the static
capacitor, converter and thermal errors that were previously added after
the final trim residual. The full output, at the declared ten-minute service
state and with random wander suppressed, improves from about 53 cents worst
error to under 6 cents at the twelve 248/992 Hz check points. This is a
service-procedure result, not a fit to the recording. The chorus's output
coupling load now follows the modeled mute transistor's delayed state,
correcting its previously premature load change. Unit Character text no
longer claims a measured hardware population.

## 2026-09-04 — Earlier noise crest-factor interpretation (certainty withdrawn above)

Twenty further factory presets compared against KR-106 put three of them 13 to
15 dB apart where the other seventeen sat inside a 5 dB band — and all three
were the noise-only patches, A67 Shaker, A84 Dust Storm and B18 Noise Sweep.
Measured directly, noise at full against saw at full within each instrument so
the chain cancels, KR-106 reads +4.66 dB where this engine reads −9.99: a
14.7 dB gap on one leg. The control law's *shape* agrees almost exactly (at
byte 64 against 127, −6.50 against −6.32), so the disagreement is where the
whole leg sits, not how it moves.

**The service manual anchors both ends of a ratio that settles most of it.**
p. 19 s. 9 trims NOISE for 4 Vp-p at TP8; s. 6 trims VCA GAIN for 6 Vp-p at
TP8 on the self-oscillating filter. Saw is never measured there, but noise
against self-oscillation is fixed by Roland at both ends, and it is measurable
on any model:

| | noise / self-oscillation at TP8 |
| --- | ---: |
| KR-106 | −2.86 dB |
| here | −11.81 dB |

**What each implies about reading a scope.** A 6 Vp-p sine has 2.121 V RMS, so
each ratio names an implied noise RMS, and the spec's 4 Vp-p marks ±2 V:

- KR-106 implies 1.526 V RMS, so its 4 Vp-p band is **±1.31 σ**, which would
  contain 81 % of the samples. The trace would visibly spill well past the
  marks. That is what reading a noise peak-to-peak as though it were a sine
  produces: the naive reading predicts −3.52 dB and KR-106 sits 0.66 dB from
  it.
- This engine implies 0.545 V RMS, so its band is **±3.67 σ**, containing
  essentially all of them. A scope trace of Gaussian noise shows roughly ±3 to
  ±4 σ depending on persistence, so this is the conventional reading and it
  sits inside OQ-16's own −8.5…−12.6 dB bracket, near its quiet end.

**So nothing moves, and for once the reason is not caution.** The gap is not a
disagreement about the instrument; it is a disagreement about what a
peak-to-peak figure means for noise, and one of the two readings is physically
wrong. A ±1.31 σ band is not a reading a technician can take off a trace.

**What that leaves.** The 14.7 dB noise-against-saw gap decomposes as about
8.95 dB of this crest-factor difference and about 5.7 dB of saw-against-self-
oscillation, which is OQ-15's drive coordinate and unanchored on both sides —
the same coordinate the sub sits on. Worth recording beside it: a single aged
unit's account measured its own noise trim 3.52 dB above the 4 Vp-p spec, so
drift moves this in the loud direction too, and the 2026-08-19 A64 corpus work
independently read this project's noise as quiet against the sub. Both point
the same way as KR-106 and neither reaches 9 dB. A TP8 crest-factor capture is
still what closes it.

## 2026-09-04 — The output uses 2.5 dB more of the range, and why not 18.7

The owner reported that KR-106 is much louder, and it is: on A11 with the same
notes, each instrument at its own default, KR-106 reads −17.5 dBFS RMS where
this engine at **maximum** volume reads −36.2. An 18.7 dB gap, and the volume
knob is not the cause — maxing it changes nothing, because the gap is in where
0 dBFS sits.

**The cause is this project's own convention, not a defect.** Digital full
scale was the output summer's clipping asymptote and nothing else, which is the
most defensible ceiling a model can pick because it invents no limit the circuit
does not have. But it is a headroom policy, not a loudness one, and a real 106
only approaches that rail when driven hard, so ordinary patches sat 22 dB below
full scale. Output calibration is explicitly a product convention rather than a
JUNO-106 voltage, so this is a product decision, not an evidence one.

**The first sizing was wrong and the suite caught it.** Sized against the
factory bank's own peak headroom the answer is 8.5 dB, because the loudest
preset peaked at −9.54 dBFS against the −1 dBFS ceiling the audit enforces. But
the bank's presets carry their own VR1 attenuation, so the bank's headroom is
not the instrument's. A six-voice chord with saw, pulse and sub on and both VCA
LEVEL and VOLUME at maximum — ordinary playing, not an extreme — peaks 3.0 dB
below full scale. At +8 dB that chord reads +4.99 dBFS. An instrument that
clips when a player holds a chord with the volume up has traded one defect for
a worse one, and the output corpus's overload guards fired on exactly that.

**2.5 dB ships.** The chord lands at −0.51 dBFS with zero overloads, the
loudest factory preset at −7.04, and the only fixture still crossing full scale
is the solo-unison headroom probe that crossed it before. It is a pure
post-clip scalar applied after every modelled nonlinearity, so no timbre, no
saturation point and no internal headroom relationship moves; only where 0 dBFS
sits, which is why it belongs in the unreleased 1.1.0. The absolute −31 dBFS
gated ceiling moved to −28.5 with it, because an absolute figure that does not
travel with the reference stops describing the same loudness.

**The remaining 16 dB is not headroom this instrument has.** KR-106's own mixed
patches measure +10.7 dBFS at its default master: it overflows full scale and
relies on the host to pull it back. That is a legitimate choice for a float
output with no limiter, and it is available here for the asking, but it is not
made silently.

**Bank balance is unchanged, deliberately.** The 27.85 dB spread is dominated by
patches that are genuinely quiet — Hand Claps at −58.9 dBFS gated, Dust Storm
−54.3, Shaker −49.0 — and A11 sits at the 0.80 trim cap already, so it is quiet
because the patch is quiet rather than because anything attenuated it. Raising
those means inventing gain the instrument does not have, against this project's
own stated position that the bank's level differences are measurements rather
than targets, and VR1's remaining 2.25 dB of travel could not close 28 dB in
any case.

## 2026-09-04 — The sub coordinate moves to KR-106's reading, on the owner's decision

`subMixVolts` 5.0 -> 7.57. **Chosen by the owner** from four options after the
sanity check below put this project's sub-against-saw at +4.89 dB where
KR-106 reads +8.49 and Arturia's Jun-6 V reads +6.87, frequency-matched at
261.63 Hz so any response difference cancels exactly. The owner chose to follow
KR-106, the one of the two that models the 106 rather than the JUNO-6.

**It stays voiced, and the reasons it cannot be promoted are worth writing
down, because the temptation later will be to forget them.** Two models cannot
close an open question. The two disagree by 1.6 dB about the size of the
correction. KR-106's own pulse reading is the outlier against both other
models, so its internal balance is not a reference either. And this project
already consumes KR-106's measurements elsewhere, so **citing KR-106 as
independent corroboration of a number copied from KR-106 would be circular** —
the value is now shared, and a shared value cannot later be presented as
agreement. What actually changed is that the former value was the outlier on
the one source coordinate this project has never had an end-to-end anchor for.
OQ-15's take 03, recorded from an identified unit, is still what settles it.

**Three places restated the coordinate as a literal and would have gone
silently wrong.** The DCO-scan audit's analytic Fourier reference stated the
divider's rails as `10.0`; the startup C56 pre-charge test expected
`6.0 + 5.0 * subTarget`; the SUB-jump test expected a volt figure and a volt
settling bound. All three now read the constant, so re-voicing it moves the
expectations with it instead of leaving them describing a waveform the engine
no longer produces. The audit's own strict cell confirms the repair:
`strict_waveform=sub` agrees with the independent reference to 0.0004 dB.

**What it cost the bank.** Eleven presets crossed the -31 dBFS gated loudness
ceiling, worst A66 Timpani at -27.56, because the bank had been trimmed to sit
just under it at -31.01. Their VR1 shaft trims were lowered by the minimum
each needed, from -0.09 dB on A48 to -3.52 dB on A66; the trims remain
attenuation-only and no entry exceeds the 0.80 panel default. The full audit
then passed for all 128 presets. The six-row output corpus was re-pinned, with
the waveform-free self-oscillation row unchanged to six figures as the negative
control because that fixture runs the sub at zero.

## 2026-09-04 — Sanity check against KR-106, driven directly

The calibration takes were rendered a third time, through **Ultramaster
KR-106**, a GPL3 JUNO-106 model whose author calibrated it from hardware
measurements, firmware analysis and factory schematics. This pass was not
handed to an operator: a small offline AudioUnit host drives the plug-in
directly, sets all 56 parameters, plays a sample-accurate score and writes
float WAVs, so there is no DAW tempo to be overridden, no panel set by hand,
no bounce region and no demo build. Its three switch-valued parameters were
decoded by measurement rather than assumption. Nothing clipped: every ratio
below is identical to 0.01 dB across a 12 dB change of its master volume.

**KR-106 is not an independent witness everywhere.** This project already
cites it as a source for the chorus click-timing series, the code-to-frequency
and sustain tables and the factory tone transcription, and the shipped chorus
sweep trajectory *is* its measurement. On chorus delay a comparison is
therefore partly circular. On mixer levels, the filter and the amplifier it is
independent: nothing here was taken from it.

**Where the two agree, within the tolerance a sanity check asks for:**

| quantity | KR-106 | here |
| --- | --- | --- |
| VCA LEVEL, 32-byte steps | −4.88 / −9.92 / −14.96 dB | −5.13 / −10.43 / −15.73 |
| High-pass cut II / cut III | −8.4 / −17.4 dB | −9.5 / −18.8 |
| NOISE byte 64 re 127 | −6.50 dB | −6.32 |
| Resonance peak, mid travel | 15.1 / 22.9 dB | 14.5 / 20.0 |
| Chorus mode ratio | 1.63623 | 1.62348 (+0.79 %) |

That mode ratio is the pass's best result. This project derives 1.6234799 from
the mode switch's own T-network; KR-106 lands 0.79 % away and the audited
Roland Cloud model landed 0.10 % away. Three sources within about one percent
is real corroboration of a derivation, on the one quantity no chain can
distort. The absolute rates spread wider — 0.5144 and 0.8417 Hz here against
this project's derived 0.5533 and 0.8983, about 7 % slower, with Roland Cloud
2 % slower — and this project's come from printed component values, so they
stand.

**Where they differ and this project is better anchored, so nothing moves:**
self-oscillation lands on 247.1 Hz here against KR-106's 242.4, where the
service manual prints 248 Hz at converter code 6272 — 0.4 % against 2.3 %. The
high-pass corners are MNA-qualified against the drawn network, the VCA LEVEL
law follows NEC's −5.9 mV/dB, and the NOISE deadband follows Tr22's drawn
grounded-base onset, where KR-106 tapers instead of cutting off.

**One quantity is the other way round, and it is the one with no anchor at
all.** Frequency-matched at 261.63 Hz, so any response difference cancels
exactly:

| ratio | KR-106 | Arturia Jun-6 V | here |
| --- | ---: | ---: | ---: |
| pulse / saw | +4.65 | +6.21 | +6.51 |
| sub / saw | +8.49 | +6.87 | +4.89 |

On pulse this project and Arturia agree within 0.3 dB and KR-106 is the
outlier, so nothing moves. On sub this project is the outlier, low, against
two independent models that bracket it 2.0 to 3.6 dB higher — and
`subMixVolts` is, by this project's own comment, the coordinate with no
end-to-end anchor. Its history says the same thing twice over: it moved on the
owner's decision in August and was reverted when the recording behind it could
not be shown to reproduce its patch. It is not moved here either, because two
models cannot close a question and because the two disagree by 1.6 dB about
the size of the correction. It is put to the owner as the one candidate this
pass produced.

## 2026-09-04 — First calibration takes rendered, and why nothing moved

Nine of the calibration takes were rendered through **Arturia's Jun-6 V** and
compared against this engine's own renders of the same files. A software
instrument is a model, not the instrument, so by the tool's own protocol this
pass can corroborate or contradict and nothing more. **No constant moved.**
What it produced is a comparison, recorded here so a later pass does not have
to redo it or, worse, mistake it for a measurement.

**The reference is two steps removed, not one, and that is the pass's main
result.** Jun-6 V models the JUNO-6, a sibling instrument, and the two do not
share this circuit. The 106 generates saw, pulse and sub inside the potted
MC5534A and sums them on one WAVE node, with only the sub's R101/D6/R102 leg
external; the JUNO-6 (CPU BOARD p. 9, MAY.10,1982) builds the same musical
function discretely — a TL082 integrator whose ramp the drawing labels 12 Vp-p
at TP3, an IC15 4013 divider for the sub, and TR2/TR3/TR4 as switches, each leg
reaching the summing node through its own printed resistors, the saw through
R37 15 kΩ and R24 68 kΩ. Those proportions are Roland's for the discrete
instrument, and the project has already settled once that a sibling drawing
describes the sibling rather than the hybrid: the same reasoning retired the
10 kΩ/47 kΩ reading of the resonance network in favour of the 106's own.

So a sub-to-saw balance measured on a JUNO-6 model is evidence about the
JUNO-6's mixer proportions, which are drawn and different, not about the
106's, which are inside a potted chip and are exactly what OQ-15 is open on.

**Method note.** The renders came back at exactly 2x speed: the DAW applied its
own tempo instead of the file's 60 BPM meta event, so every note is half as
long and half as far apart. Pitch and spectrum are untouched by that, so the
level and harmonic results stand; anything whose measurand is a time does not,
and take 09 was not rendered anyway. Every figure below is frequency-matched —
the same spectral line in both takes, so a chain EQ cancels exactly rather than
approximately — and each is confirmed at two different frequencies.

**Pulse against saw agrees.** At 261.63 Hz the reference reads +6.21 dB and
this model +6.51; at 130.81 Hz, +6.13 against +5.98. Mean disagreement 0.1 dB.
Two of the three source coordinates therefore stand in the same relation in an
independent model of the same circuit.

**Sub against saw does not.** At 261.63 Hz the reference reads +6.87 against
this model's +4.89; at 130.81 Hz, +7.04 against +4.80. The reference's sub sits
about 2.1 dB above ours relative to the saw, consistently. Take 03's isolation
was verified rather than assumed: in the reference the played pitch sits 61 dB
below the sub's own fundamental, so saw and pulse really were off.

That finding is **not** adopted, for two independent reasons. The first is the
instrument difference above: the JUNO-6 proportions this leg with its own
printed resistors. The second is already on record. On 2026-08-19
`subMixVolts` moved 5.0 -> 2.0 against a hardware recording of A64, and that
move was then reverted because the recording could not be shown to reproduce
the patch it was named after — the sliders are not motorised, so a recording
witnesses the slider, not the stored byte. The withdrawn hardware finding said
the sub was too LOUD; this sibling model says it is too QUIET, by a comparable
margin in the opposite direction. Two weak and opposed readings leave OQ-15's
sub coordinate exactly where it was: the one source level with no end-to-end
anchor, waiting on take 03 recorded from an identified 106.

What the pulse agreement is worth is correspondingly narrower too. Two
instruments that proportion this mixer differently landing within 0.1 dB of
each other on pulse-against-saw is a weaker coincidence than it first reads,
because both may simply put the two switched legs on equal resistors — which
the JUNO-6 drawing does, and which the 106's WAVE node does by construction.

**Three places the reference differs where this model has a derivation and it
does not**, all recorded as confidence rather than as defects:

- Self-oscillation pitch. At the same panel the reference's filter sits about
  1.7 octaves above ours; ours lands on 247.9 Hz where the service manual
  specifies 248 Hz at converter code 6272. The check point is Roland's own.
- Noise spectrum. The reference's noise is flat to 8 kHz; ours falls away
  above it, which is C41/R79's 4.82 kHz pole read off the module drawing.
- Noise deadband. The reference produces noise at stored byte 4; ours is
  silent below byte 6, from Tr22's drawn grounded-base onset. A model that
  scales noise linearly from zero is the expected default, not a
  counter-measurement.

**The factory route does not survive a different instrument either.** Take 13
selects factory program A11 and exists so that a session with no SysEx path
still compares identical tone bytes on both sides. Jun-6 V does not carry the
106's bank, and the re-rendered take proves the patch differs rather than
merely suspecting it: A11's stored bytes are 16', saw only, sub and noise at
zero, with no LFO reaching any destination, so this engine sounds MIDI 36 at
32.7 Hz; the reference sounds it at 65.4 Hz, an octave up, and breaks the held
fourth note into five bursts at about 2 Hz, which A11 has no modulation path to
produce. Two independent disproofs, so takes 13-15 are void on any instrument
without the 106's own bank.

**A11 re-rendered through a JUNO-106 emulation is the first like-for-like in
the exercise, and it carries two signals.** Unlike the Jun-6 V attempt, this
one plays A11's own stored bytes: all four notes land within a few cents of
this engine — 32.7, 65.3, 130.3 and 261.7 Hz against 32.7, 65.5, 130.1 and
263.2 — including A11's 16' range, and the note windows agree to about 30 ms
once the tempo was corrected. So the patch, the octave and the score are
common ground, and what differs afterwards is the instruments.

*Level against pitch differs systematically.* Referred to each take's own
first note, so the chain cancels, the reference reads +0.00, +2.92, −1.62 and
+1.85 dB across the four notes where this engine reads +0.00, −1.28, −3.52 and
−2.63. The reference's level rises with pitch about 4 dB more than ours over
three octaves, consistently rather than at one note. A coarse brightness
measure — the harmonic at which the series has fallen 20 dB, which the hiss
floor cannot reach — puts its filter corner rising 0.894 octave per played
octave against this engine's 1.111, where A11 stores VCF KYBD at 86/127. The
two observations do not obviously point the same way and the cause is not
isolated here; what is solid is that on identical stored bytes the two
instruments distribute level across the keyboard differently.

*The idle floor is 14.6 dB higher relative to signal*, at a signal-to-idle of
48.4 dB against this engine's 63.0, and 97.8 % of its idle energy sits above
2 kHz against our 72.6 %, rising to a 4–8 kHz peak. That shape is what BBD
hiss looks like, and A11 has chorus I engaged, so the natural reading is
OQ-03's chorus noise — where this project's 29.86 % default is declared
session-compatibility policy rather than anything derived. It is not adopted,
and not only because the reference is a model: the render came from a demo
build, and a demo that injects noise and a chorus that models more hiss are
indistinguishable in a single take with the chorus on. One file separates
them, and it was rendered: A11 again on the same instrument with the chorus
switched off, twice.

**The floor collapses, so the noise is chorus hiss and the take stands.** With
chorus off the reference is silent in every idle window — take A reads
−95.3 dB with none of its energy above 2 kHz, which is a release tail rather
than a floor, and take B is literally all zeros. With chorus I the same
instrument on the same patch reads −77.7 dB, 97.8 % of it above 2 kHz, and it
is present *before the first note*, so it is neither a tail nor a bounce
artefact. Engaging the chorus raises the idle floor by 17.6 dB. Two takes were
supplied specifically so an artefact could be told from the instrument; they do
not subtract to nothing — the oscillators free-run, so the notes differ in
phase and level by over a decibel — but neither carries any high-frequency
floor at all, which is what the question needed.

Referred to each take's own first note so the chain cancels, signal-to-idle is
47.6 dB on the reference against 64.4 dB here: **the reference carries 16.8 dB
more chorus hiss.** Its spectrum differs in shape too, rising 13.3 dB into the
4–8 kHz octave where this model's is nearly flat and falls away above 8 kHz
under its own reconstruction filters.

**It does not move the default, and the reason is arithmetic rather than
policy.** 16.8 dB above this model's 29.86 % default is about 207 % on its own
HISS scale, whose 100 % is pinned to the 0.2 mVrms figure Panasonic prints as a
*maximum*. A reference sitting at roughly twice a part's own maximum row is
voicing hiss for character, not reproducing a datasheet, so it cannot argue
this model's default upward. What it does establish, for the first time from
something outside this project, is that another 106 emulation treats this hiss
as far more audible than the shipped default does — and OQ-03 already records
that 29.86 % is session-compatibility policy rather than a derived value. The
measurement that would settle it is unchanged: a silent-patch capture at the
mono jack of an identified unit, chorus off then chorus I.

**One take returned nothing usable.** Take 06 sweeps the cutoff with the filter
envelope, and in the reference the high-to-low band ratio stalls 39.8 dB below
what that same instrument reaches with its filter wide open in take 04 — so its
filter never opened, and the take measures its ENV-to-VCF path rather than the
cutoff law. Whether the panel was mis-set or its envelope is slower than the
halved note allowed cannot be told from the recording.

## 2026-09-04 — Chart geometry chosen by ear; the noise crest stays put

Two bracketed constants went to a blind listening test, keys unread until
after the verdicts.

**Converter intra-pass placement (OQ-08).** A was the shipping normalised
`ordinal/23` placement, B the pixel-measured p. 8 chart geometry
(`MeasuredChartGeometry`). Material: eight fast resonant filter-envelope
stabs, a short chord and six PWM staccato notes; identical MIDI, controls,
seed and rate; whole-file RMS matched within 0.03 dB with no trim, one common
gain. The A/B null measured −3.2 dBc, almost all of it the free-running DCO
phase each note-on catches at a different point in the pass.

**Verdict, by ear:** very similar, B slightly better, heard as more resonant.
The plug-in and the demo renderer now select `MeasuredChartGeometry` before
preparing the engine, the way the VCF solver default is applied; the engine's
own default stays the normalised grid so the frozen fingerprints and
ordinal-gap fixtures keep testing the reference, and all three profiles
remain selectable. This is a choice between two drafting-derived placements and moves no
evidence class: neither profile is a hardware timestamp, and OQ-08's physical
capture list is untouched.

**Main NOISE level crest convention (OQ-16).** A was the shipping −11.96 dB
reading of the "4 Vp-p at TP8" adjustment re the 4.8 Vp-p self-oscillation
trim, B the six-sigma scope reading at +1.9 dB, C the bracket top at
+3.5 dB, through the comparison-only `mainNoiseLevelScale`. Material: saw +
NOISE 10, then a NOISE-only resonant sweep; deliberately not level-matched,
since level was the quantity under judgement; one common gain.

**Verdict, by ear:** hard to say, no preference. Nothing moves:
`noiseMixVolts` keeps its conservative reading inside the anchored bracket,
and the comparison switch stays for a future set with different material.

## 2026-09-03 — Correction: the de-potted original corroborates the shipped value

The entry below records the Sound Doctorin teardown as widening the resonance
compensation bracket to 0.882 and defining a coupled OQ-09/OQ-15 re-derivation.
**That conclusion was wrong, and this entry supersedes it.** Cloning the
Open80017a repository and reading Herpoel's KiCad schematic directly — R1 24k,
R2 1.5k, R3 4.7k, R25 100k, R26 1.5k, R27 4.7k, R30 47k — made the arithmetic
that settles it available.

The teardown's readings are **in-circuit**, and in-circuit ohmmetry reads low
through parallel paths. Against that topology the predicted readings are what he
saw, to better than a tenth of a kilohm:

    R1  24k  in parallel with (4.7 + 1.5 + 0.56)k = 5.27k   he read 5.1k
    R3  4.7k in parallel with (24 + 1.5)k         = 3.97k   he read 3.9k

And the pattern is not selective. Every value the two sources agree on — 68 kΩ
between the stages, 100 kΩ resonance feedback, 47 kΩ VCA load, 1.5 kΩ, 560 Ω,
and the 4.7 kΩ VCA input — is one whose parallel path is negligible; both
disagreements are in the direction a parallel path forces, and both land where
it predicts. That is the signature of a measurement artefact, not of a different
circuit.

So the teardown is not a third competing reading. It is a **second independent
original consistent with 4.7 kΩ and 24 kΩ**, which is the reading that already
ships. What it reclassifies is the sibling drawing: 10 kΩ and 47 kΩ describe the
discrete JUNO-6/60's own proportioning, not the potted hybrid's, which is exactly
what one would expect of a re-implementation.

Three consequences. The shipped 0.275116 stops being merely the conservative end
of a bracket and becomes the best-supported reading of the 106's own module. The
coupled OQ-09/OQ-15 re-derivation named below is **not** owed — the drive
coordinate is not moved by this evidence after all. And the Drawn shape stays
selectable as the sibling instrument's value, which is what it now is.

Everything the previous entry recorded as confirmed still stands: 68 kΩ, 560 Ω,
100 kΩ, the 47 kΩ VCA load, and ~250 pF settling the integrator capacitor on
240 pF with 270 pF identified as the clone's value.

Documentation only; no constant moves.

## 2026-09-03 — First measurement of an original 80017A, and why nothing moved yet

A technician's ohmmeter survey of a **de-potted original A1QH80017A**
([Sound Doctorin](https://sounddoctorin.com/synthtec/roland/juno106.htm)) is the
first measured-on-an-original evidence this project has held. Everything before
it was a factory drawing of a sibling instrument or a clone's reconstruction.

**What it confirms**, independently of the sources the model already used: 68 kΩ
between the IR3109 stages, 560 Ω shunts to ground, 100 kΩ resonance feedback
from VCF OUT with 1.5 kΩ to ground, and **47 kΩ on the VCA BA662's output** —
the same value the voice-VCA headroom was derived from on 2026-09-02, now
corroborated on a real part rather than on the JUNO-6/60 drawing alone.

**What it closes.** All four stage capacitors read ~250 pF, which is the 240 pF
the sibling schematic prints within a hand-meter's tolerance. The competing
270 pF turns out to be the Analogue Renaissance *clone's* value, not Roland's, so
it is not evidence about the original at all. `poleCapacitorFarads` stays at
240 pF and OQ-18's 64.8 kHz branch is retired.

**What it unsettles.** The resonance OTA's non-inverting leg reads 5.1 kΩ against
1.5 kΩ, not 47 kΩ, and the stage-1 series input reads 3.9 kΩ, not 10 kΩ. Those
give c = (3.9/68)·(101.5/6.6) = 0.882, three times the shipped floor and 6.9 dB
more passband compensation at full resonance. The bracket has therefore *widened*,
and for the first time its top is a measurement rather than a drawing.

**It is not adopted, for one structural reason and two evidential ones.**
Structurally, the same reading puts stage 1's gain at 68/3.9 = 17.4 where
`filterInputAttenuation` assumes 6.8, so it moves OQ-15's drive coordinate by the
same act. Changing one without the other is incoherent, and the pair has to be
re-derived together and reconciled against the anchored 6 Vp-p TP8 VCA GAIN trim
— the same trim that killed the earlier proposal to move the drive coordinate on
its own. Evidentially, it is a single in-circuit hand measurement, self-described
as "roughly", standing against two independent 900 dpi reads of the sibling
factory drawing. In-circuit ohmmetry reads low through parallel paths; the same
meter did return 68 kΩ, 100 kΩ and 47 kΩ correctly, so a nine-fold error on the
+ leg is not a simple meter artefact, but one sample is one sample.

One further caution from the same page, worth carrying: it reports that the
Service Notes' **BA662 pin numbering is wrong** for this module. That cuts both
ways — it weakens the drawing as a detail source, and it shows the author was
working against a document he already distrusted.

This is an evidence-recording entry, not a change. Nothing in the DSP moves.
The next piece of work it defines is the coupled OQ-09/OQ-15 re-derivation, which
is the highest-value item now open.

## 2026-09-03 — The resonance pair sees one difference, not two separate terms

The resonance BA662 is a single differential pair. It takes **one** tanh of the
difference of its two divided inputs: VCF IN through R5 47k / R2 1.5k on the
non-inverting side, VCF OUT through R3 100k / R1 1.5k on the inverting one
(JUNO-6 and JUNO-60 CPU BOARD p. 9, the discrete circuit the A1QH80017A
integrates). The model instead did two separate things: it multiplied the filter
input by a linear `1 + c*k` ahead of the cascade, and applied a tanh to the
feedback return inside it. That is not a constant to re-pin; it is the wrong
form, in all five solver kernels.

The restructuring is exact and small. Writing the pair's argument as the
difference it physically is,

    previous = drive - k*H*tanh((V4 - c*drive)/H)

recovers `drive*(1 + c*k) - k*V4` in the linear limit, so the two agree wherever
both terms are small, and diverge only where the nonlinearity actually bites.
`c` is the same bracketed coefficient the 2026-09-02 entry shipped; it now
multiplies the drive inside the tanh instead of ahead of it.

**The endpoint solve is untouched, by construction.** At `drive = 0` the two
forms are bit-identical, and Roland's 4.8 Vp-p / 248 Hz self-oscillation trim is
taken with no oscillator, sub or noise in the patch. So `maximumFeedback = 4.504`
and the `frequencyTrim` droop table built on it stand without re-solving, and the
suite's endpoint assertions pass unchanged.

Measured, shipping defaults at 48 kHz/4x, against the split form: silence is
bit-identical and small-signal material barely moves (-0.087 dB on an open saw
pad, -39 dBc). Two effects then compete as the signal grows, and both are
consequences of the same correction. Removing the feedforward takes level off a
loud resonant passband: -2.49 dB on a resonant pad, -1.56 dB on a stepped
resonance ramp, -0.97 dB on a full-mixer patch. Shrinking the tanh's argument
stops the loop being throttled by its own drive: +3.65 dB on a self-oscillating
chord that has an oscillator in it, which is the case the audit named -- the
model used to stop ringing where the drawn circuit keeps ringing.

This is an evidence-priority correction, not a listening verdict: the split form
is not a defensible alternative reading of the circuit, it is an approximation
that only holds at small signal. No letters were rendered and none are owed. It
does not close OQ-09, which still owns the coefficient's value.

Paired native benchmark: -0.53 %, +1.69 %, +2.72 %, +0.35 % across the four
audit scenarios -- the largest CPU cost of this pass, and one extra multiply and
subtract per node in the hot feedback path is what it buys. Full suite 15/15.
`EngineParameters::enableDifferentialResonanceInput` restores the split
bit-exactly for comparison renders.

## 2026-09-02 — Resonance input compensation: off a voiced value, on to a bracket

`inputCompensationPerFeedback` was 0.2296 with no derivation behind it. Two
independent readings of the network now exist, and both put it higher.

At DC each filter stage's input node is held at zero by its own integrator, so
with the resonance OTA's transconductance written as `k` the transfer is
`V_out = (R_fb/R_in)·V_in·(1 + ck)/(1 + k)` and `gm` cancels: the slope in this
loop-gain coordinate is resistor-only.

- Roland's own **JUNO-6 (10 May 1982)** and **JUNO-60 (10 April 1983)** CPU
  BOARD p. 9 draw the discrete IR3109 + BA662 circuit the A1QH80017A
  integrates: R14 10k in, R7 68k stage-1 feedback, R5 47k + R2 1.5k from
  VCF IN, R3 100k + R1 1.5k from VCF OUT. `(10/68)·(101.5/48.5) = 0.307762`.
- The published **Open80017a** reconstruction (Thomas Herpoel, Rev 0.2,
  2024-02-28) carries R3 4.7k in, R5 68k feedback, R1 24k + R2 1.5k from
  VCF IN, R25 100k + R26 1.5k from VCF OUT. `(4.7/68)·(101.5/25.5) = 0.275116`.
  The engine's former comment transcribed this lineage correctly at 0.275.

They disagree 2.1× on the stage-1 input resistor and 2× on the non-inverting
leg, and land 12 % apart only because those errors compensate. This is
therefore **not** the situation that promoted the 47 kΩ voice-VCA load on
2026-09-02, where drawing and reconstruction agreed; two sources that bound a
magnitude without fixing it is the voiced-in-bracket class, and its own
precedent — the NOISE onset bracketed on VR32 and shipped at its floor — takes
the end that claims least. **Shipped at 0.275116.** The Roland-drawn 0.307762
and the retired 0.2296 both remain selectable through
`EngineParameters::resonanceCompensationShape`, which is not serialised.

This is an evidence-priority decision, not a listening verdict: what settles it
is that 0.2296 sits 17 % below every derivable reading, not that anything
sounded better. The remaining choice inside the bracket is a genuine by-ear
question — 0.2751 against 0.3078 is +0.88 dB on a resonant pad — and an A/B/C
set was rendered for it (A = 0.2296, B = 0.2751 shipped, C = 0.3078; key.md
written at render time and unread by design). **No letter has been chosen yet**,
so nothing here is recorded as chosen by ear. It does not close OQ-09.

Measured, shipping defaults at 48 kHz/4×, against the retired coefficient:
resonance-0 material is unchanged and silence is untouched; +0.11 dB at panel
0.10, +0.56 dB on a full-mixer patch at 0.40, +0.60 dB on 0.55 plucks, +2.75 dB
on a resonant 0.85 pad, where the extra input drive also feeds the peak and the
level change exceeds the 0.76 dB of added input gain. Stepped and swept
RESONANCE lanes differ by −9.5 to −12.7 dBc. Paired native benchmark: +0.10 %,
+0.37 %, +0.28 %, +0.65 % across the four scenarios, inside measurement noise.

The endpoint solve is untouched and does not need re-solving: the 4.8 Vp-p
self-oscillation trim renders with no oscillator, sub or noise in the patch, so
the only signal the compensation multiplies is the pinned-pulse DC that C56/C50
has already removed. A regression now renders that take through the widest pair
of readings in the bracket and requires the same limit cycle, to a tolerance
rather than to the bit.

## 2026-09-02 — Resonance CV steps at the write; the VCF/VCA lag is on p. 13

The three post-converter control slews were documented as standing "only on
whatever lag exists downstream inside the 80017a module (p. 9 prints no
values)". That is wrong on all three counts, and re-reading module board p. 13
at the scan's native resolution settles them.

- **Resonance ships changed.** IC26's C86 feeds IC22c, whose output runs to the
  card as bare wire into VR26 20KB, R107 27 kΩ and the grounded-base Tr18.
  There is no capacitor anywhere on that run, and CH2's VR21/R88/Tr15 is
  identical. That is the direct-follower topology whose 522 µs compatibility
  slews the DCO and NOISE holds already retired (2026-09-01), so resonance
  steps at its write too. The retired constant was the first revision's single
  undifferentiated `controlSlewSeconds` and never had a network behind it.
- **The voice VCA keeps its number and gains a derivation.** The VCA CV crosses
  R106 10 kΩ into C58 0.1 µF with R105 22 kΩ onward to Tr20, so C58 sees
  6.875 kΩ and the time constant is 687.5 µs — the shipped 687 µs to three
  figures. Anchored topology, derived constant, no audio change.
- **The VCF keeps its number as a bracket.** Its C61 0.1 µF sits behind VR28
  5KB (WIDTH) and R113 10 kΩ, with R110 8.2 kΩ and the R111 560 Ω positor
  onward to pin 6, so C61 sees 4.67–5.53 kΩ across the trimmer's travel:
  467–553 µs. The shipped 522 µs is inside it at about 58 % of the track. The
  trimmer's set position is unread, so the point stays voiced-in-bracket on the
  NOISE-onset precedent rather than being re-pinned to an endpoint.

Measured impact of the resonance change, shipping defaults at 48 kHz/4×: every
static-control scenario renders bit-identically (difference at the float dump's
−250 to −345 dBc floor, including a cutoff sweep, which confirms the cutoff
trajectory is untouched). It shows only while the RESONANCE control moves —
−9.2 dBc against a stepped ramp, −13.1 dBc against a slow sweep, −28.4 dBc
against a 3 Hz wobble — where each 7-bit converter step now lands hard instead
of gliding. Paired native benchmark: −0.32 %, +0.33 %, +0.59 %, +0.43 % across
the four scenarios, all inside the noise of a loaded machine.

## 2026-09-02 — Voice BA662 signal saturation

The voice VCA's signal path is now the BA662 differential pair's
`I_tail · tanh(V_d / 2V_t)` rather than a linear multiply. The pair has no
linearising diodes (Open Music Labs' reverse-engineered BA662; the BA6110
sibling does carry them and corroborates only the family's gm law), so the
shape has no free constant, and Roland's own ADJUSTMENT steps fix how hard it
is driven: on the same bank and key, s. 5 sets 4.8 Vp-p at the filter output
and s. 6 sets 6 Vp-p at the VCA output, so the drive follows from the output
side alone — 3.0 V across the load against a full-control tail of
(9.92 + 0.26 − 0.62) V / 32 kΩ = 299 µA — and the unread pin-9 divider
cancels. The one magnitude-setting value the JUNO-106 drawings do not print,
the OTA's output load, is now read from Roland's own JUNO-6 and JUNO-60
Service Notes (CPU BOARD, p. 9 in both), which draw the discrete IR3109 +
BA662 voice circuit the 80017A integrates with R42 47 kΩ on the VCA BA662's
output; the Open80017a reconstruction agrees. That gives u_trim = 0.217 and
11.06 V of headroom at the filter-output node.

This supersedes, for the signal nonlinearity only, the 2026-08-31 sentence
below that "BA662 signal nonlinearity/noise/thump and converter charge
injection remain unimplemented: available sources settle topology and nominal
time constants, not the original hybrid transfer". The sibling drawing is the
new evidence: the law was never in question, and the load is now a
Roland-drawn value of the same circuit rather than one clone's choice. The
pair's noise, its thump and the converter charge injection remain as that
entry left them. This is an evidence-priority decision under the realism/CPU
goal, not a listening verdict, and it does not close OQ-19: the 80017A's own
printed load and input network are still unread, and a TP19-against-TP8
level-swept THD capture (HD3 = −48 dBc predicted at the 4.8 Vp-p trim) would
confirm the headroom directly. The control law, VR30 null and C59 corner are
untouched, the switch-off path is bit-identical to the previous engine, the
self-oscillation anchor is untouched at the filter node, and the 4 Vp-p TP8
noise figure moves by under 0.1 dB, inside its stated crest-convention band.

Measured on the shipping path at 48 kHz/4×: a full saw+pulse+sub open-filter
voice compresses by −0.75 dB with a level-matched residual of −28.8 dBc
(whole-file on-minus-off −20.9 dBc, dominated by the gain term); a filtered
saw by −0.07 dB and −49.4 dBc (−41.1 dBc whole-file); the self-oscillation
corpus row by −0.10 dB, the pair's prediction at the trim level. A native
Apple silicon Release paired benchmark (Poly/Cubic/RK4, 256-frame blocks,
seven alternating rounds) moved median thread CPU by −0.70 % idle, +2.04 %
on six plain voices, +2.22 % on six resonant voices and +1.77 % on the
six-voice full-mixer Chorus II case, inside the +5 % budget.

## 2026-09-01 — NOISE control onset

The circuit-derived linear-above-onset NOISE level profile is now the
default. Module board p. 13 draws Tr22 (PNP, base grounded) fed from the
NOISE LEVEL hold through R115 10 kΩ and VR32 100 kΩ in series, with R114
2.2 MΩ pulling its emitter node towards −15 V, and its collector straight
into IC14's BA662 control pin. The control current is therefore zero until
the hold clears 0.6 V + Rs × 7.09 µA and linear above, with the anchored
full-level endpoint unchanged. The hold stands on the anchored +0.26 V VR34
standoff (p. 18 section 3, TP7 → IC26 → NOISE LEVEL), so the onset is
measured from there. Rs is VR32, the p. 19 section 9 NOISE LEVEL trimmer:
the notes fix its criterion (4 Vp-p at TP8) but not its position, which
follows Tr21's selected amplitude, so the onset is bracketed 0.671 V (VR32
at zero) to 1.380 V (maximum) and ships at the floor, the one position that
never overstates the deadband.

This is an evidence-priority decision under the realism/CPU goal, not a
listening verdict and not a closure of OQ-16: VR32's installed position and
Tr21's selected amplitude still await a TP8 sweep or a trimmer reading. The
BA662's input saturation of the noise is left out because its drive is
unfixed by the sources. The legacy linear-from-zero law remains available
behind the internal comparison switch.

## 2026-09-01 — Resonance onset on the VR34 standoff

The circuit-derived resonance profile now measures its junction onset from
the +0.26 V the RES CV hold already stands at, not from 0 V. Service Notes
p. 18 section 3 trims VR34 for +0.25…+0.27 V at TP7 with the D/A forced to
0 V; p. 13 injects VR34 through R127 into IC27b, whose output is TP7; p. 8
routes TP7 through IC26 to RES CV as well as VCA CV; and the p. 13 resonance
leg (IC26 ch6, C86, IC22c, VR26, R107, grounded-base Tr18) has no pull-down
to divide it. The engine already treated that standoff as anchored for the
voice VCA rail, so the onset moves from 0.6 V to 0.34 V above the hold's
zero: first loop gain at stored byte ~4 instead of ~8, about +4 dB at
Resonance 1/10, +1.3 dB at 2/10, +0.3 dB at 5/10 and nothing at 10/10. The
endpoint is the same service-trimmed self-oscillation maximum, the 0.2296
compensation is untouched, and no DSP work is added.

This is an evidence-priority correction, not a listening verdict and not a
closure of OQ-09: the standoff is the anchored service state, but the 0.6 V
junction drop above it remains the nominal prior a measured
response-versus-resonance family would replace.

## 2026-09-01 — Voice-VCA control: exact junction law replaces softplus

The voice VCA's envelope-to-gain law is now the solved emitter equation of the
traced Tr20 grounded-base stage — R106 + R105 = 32 kΩ, kT/q and the VR34
+0.26 V standoff, all already in tree — tabulated once at prepare time. The
softplus it replaces was labelled in the code as a smooth, replaceable
approximation of that same topology; the exact law shares its sub-knee
exponential tail and its full-scale point and differs only in between, where
V_be keeps rising with current: −0.2 dB at control 0.010, −2.5 dB at 0.020,
−1.6 dB at 0.050, −0.8 dB at 0.10, −0.24 dB at 0.30, −0.05 dB at 0.70, 0 at
full scale. Audibly, release tails and slow decays between about −25 and
−50 dB close one to two and a half decibels sooner; attacks cross that region
in under a millisecond and sustain levels do not move.

This is an evidence-priority shape replacement on an anchored topology, not a
listening verdict, and it does not close OQ-19. The knee stays voiced: the
reconstruction's 150 mV on the +0.26 V standoff, carried over under the stated
convention that the exact law's tail coincides with the former softplus's. That
convention is a convention, not a derivation — the other defensible placement
(emitter current equal to Vt/R at the turn-on) moves the −45 dB region by about
9 dB, several times the 2.5 dB the shape itself changes — so OQ-19's measured
gain sweep owns placement. The former softplus remains available behind the
internal comparison switch `useSoftplusVoiceVcaCompatibilityLaw`, bit-exact.
CPU: one table index and lerp per voice per internal sample in place of the
softplus; the repo's paired A/B benchmark at the shipping Poly/Cubic/RK4
4x 48 kHz defaults read, over three runs, idle 77.65 → 78.04 ms (+0.50 %),
six-voice plain 214.24 → 213.77 ms (−0.22 %), six-voice resonant 216.65 →
216.80 ms (+0.07 %) and six-voice full-mixer Chorus II 303.97 → 304.99 ms
(+0.33 %) on the last run, every scenario inside the loaded machine's ±0.5 %
run-to-run noise.

## 2026-09-01 — µPC1252H2: noise floor adopted, nonlinearity rejected

NEC's [1983 consumer-IC data book](https://archive.org/download/bitsavers_necdataBooCircuitsforConsumerUse_42422169/1983_NEC_Integrated_Circuits_for_Consumer_Use.pdf#page=262) (µPC1252H2, p. 257) specifies the part at
Vcc/Vee ±12 V, ISET 2 mA and RIN = ROUT = 33 kΩ, and Roland's
[jack-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15)
installs IC5 in exactly that circuit: C12 10 µF / R36 33 kΩ in, R34 5.6 kΩ +
R35 680 Ω to −15 V for 2.006 mA, pin 8 into IC2b's 33 kΩ I/V, and +15 V
through R17 1.5 kΩ. Two of the table's rows were candidates.

Distortion is rejected. At the derived bus levels (0.3–1.7 Vrms, VCA LEVEL
−16.3..+4.7 dB) the trimmed typical THD is 0.007–0.02 %, −70 dB or better,
and Roland fits no symmetry trimmer (pin 4 is grounded through R33 47 Ω), so
the installed part sits somewhere in an untrimmed curve NEC bounds only as
"≥ 0.05 %" with no typical, sign or shape. That is no sourced magnitude, so
the stage stays linear rather than carry an invented one.

The output-noise floor ships. NV = −94 dBV typical (max −84 dBV) over
10 Hz–20 kHz is a derivable figure under the installed conditions, folded to
a white-equivalent density and added at IC2b's output ahead of the dry/wet
split, scaled by Unit Character like the resistor floors. Rendered, the term
is −109.4 dBFS on the dry leg at every VCA LEVEL byte (−93.3 dBV referred to
IC2b over the 0–24 kHz window, 8 % above the band-limited datasheet figure),
lifting the idle chorus-Off floor from −119.1 to −108.9 dBFS; through the wet
legs it lifts the default-HISS chorus-I idle floor by 0.46 dB, from −98.8 to
−98.3 dBFS. NEC publishes NV only at Av = 0 dB, so the constant
output-referred form is likely slightly high below 0 dB; that gain dependence
and the voice cards' own contribution to the dry floor stay with OQ-16.

This is an evidence-priority decision, not a listening verdict: nothing here
is audible, and no letters were rendered. The old floor remains available
behind the internal `enableCommonVcaNoise` comparison switch.

## 2026-09-01 — Sub half-wave mean on the WAVE node

Module p. 13 at 1200 dpi confirms R102 (R99 on CH2) as Tr19's (Tr16's)
collector load to the SUB LEVEL rail and D6 (D5) as the single series diode
from the R101 (R97) 27 kΩ leg into the WAVE line: the rail's current enters
the node on one half-cycle only, so the sub carries a mean equal to its own AC
amplitude. The model now adopts that unipolar shape and leaves the mean for
C56/C50 to remove; the level law stays linear in the held rail, and the node's
DC-to-AC impedance ratio is voiced at 1, the floor of its ≤ 2 bracket, inside
the already-voiced sub coordinate. Steady state is unchanged; only a SUB level
step now produces the C56/C59-shaped bump the DC-coupled leg makes. This is
explicitly distinct from the removed sub-driver amplitude asymmetry (a
fabricated 0.3 % level inequality). It is an evidence-priority decision, not
a listening verdict, and does not close OQ-15: a sub-level-versus-byte capture
at TP8 would fix the diode onset and the node loading. The former zero-mean
square remains behind the internal `enableSubHalfWaveNodeCoupling` switch.

## 2026-09-01 — Loaded MN3009 reconstruction network

Roland's [jack-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=15)
keeps both MN3009 outputs connected through separate 3.3 kOhm legs to the
shared 47 kOhm / 2.2 nF tap. Panasonic's
[MN3009 documentation](https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf)
shows two continuously present output followers, separately loaded ahead of a
balance pot, and its typical Gi-RL slope around the installed 50--100 kOhm
region supports a local Thevenin estimate of about 3.7 kOhm per follower. This
is a graph-derived typical nominal, not a specified or guaranteed Rout.

The former solve treated one output as an ideal source and factorised the tap
from the following reconstruction filter. The nominal model now combines both
finite-source legs, C45/C48, R117/R110, and the first 22 kOhm / 22 kOhm
Sallen-Key section in one continuous nodal system. Tr15--Tr18 remain ideal
followers, matching the existing no-extra-parameter filter model; finite beta,
gm, junction capacitance and bias-dependent output impedance need installed
device data and are not guessed. The prepared transition still has six states
and the realtime path does no additional matrix work.

Compared directly with the former ideal-source, separable implementation, the
loaded network is about 0.36 dB darker at 5 kHz, 0.87 dB at 10 kHz and 1.04 dB
at 15 kHz. DC is normalised to the existing loaded wet coordinate because
Panasonic's insertion-gain row already uses a 100 kOhm load and no original-unit
capture fixes absolute wet level. The HISS-100 recovered-line policy was
therefore remeasured, not ear-tuned: its
fixed-seed A-weighted transfer is 0.38948--0.38953 at 176.4 kHz and
0.38937--0.38941 at 192 kHz, represented by 0.3894. OQ-04 remains open for the
installed spread, follower loading, absolute gain and a wet-only hardware
sweep.

## 2026-08-31 — I+II preserves the wet mid, not the stereo side

The first I+II product implementation reused the normal two-line anti-phase
stereo output and changed only modulation rate. An original Juno-106 owner
reported that the physical both-button result was instead conspicuously narrow,
nearly mono, with a characteristic colour. That directly falsifies the wide
continuation but does not calibrate a fractional width. The corrected mode
therefore uses the only zero-parameter narrow continuation: equal-fold the two
existing wet returns to their mid. This preserves the former path's exact mono
sum and its comb colour, removes only the unsupported side, and leaves I and II
unchanged. The old wide result remains available internally for matched A/B;
an identified-unit stereo capture is still required to establish any residual
width or a different both-button clock law.

## 2026-08-31 — NOISE OTA drives C41

Roland's [module-board drawing](https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=13)
settles the order of one existing circuit: Tr21 crosses C42 into the BA662
level OTA, and C41/R79 loads that OTA's output. The former model ran both poles
at full level and multiplied the scanned NOISE hold afterwards in every voice.
That is equivalent at a fixed level, but not during a move: it kept C41 fully
excited behind Noise zero and exposed that unrelated stored charge when the
control rose.

The scanned level now drives the existing C41 state. C42 and the noise source
continue running while muted, C41 discharges and refills through its existing
4822.877 Hz pole, and no new pole, reset, click, leakage or BA662 transfer is
invented. The post-C41 path remains as an internal comparison switch. In a
matched eight-transition A/B, the difference is −17.5 dBc peak and −54.2 dBc
RMS, both referenced to the legacy take's whole-take RMS; the settled spectrum
and gain are unchanged. On coarse HQ-off grids below about 19.3 kHz internal,
the physical 33 us memory is shorter than one sample and the existing bilinear
C41 pole is negative; those grids retain the qualified fixed-level filter but
apply its level afterwards, avoiding a nonphysical alternating mute tail. A
native Apple M1 Max Release benchmark at 48 kHz/4×
(256-frame blocks, 1024 timed blocks, 13 alternating repetitions,
Poly/Cubic/RK4) changed median thread CPU by less than 1% in both the idle and
six-noise-voice cases, negligible against the standing CPU budget.

## 2026-08-31 — Pulse-Off WAVE node and evidence boundaries

The service drawings settle one previously provisional behavior: Pulse Off's
−0.8 V control pins the MC5534A comparator high, but does not disconnect its
output from the fixed WAVE node. The model now leaves that constant level on
the node and lets the existing C56/C50 coupling state reject its DC. Startup
primes the capacitor to the settled pulse mean so a restored patch does not
manufacture a power-on thump. The former hard gate remains only as an internal
A/B switch; the matched comparison measures −16.2 dBc diff RMS and +6.2 dBc
diff peak around the off/on transitions. Exact installed node level, loading
and residual switching waveform remain OQ-15/OQ-11.

The same evidence pass corrected the MN3009 noise claim below. Panasonic's
0.15/0.20 mVrms figures are conflicting *maximum* rows at the part output under
fixed test conditions. The former 59.716 uVrms inference combines an input
swing with a maximum-output S/N figure and is invalid. HISS 100% and the
29.858% default retain their numerical values as explicit product/session
policy, while regressions now test the fixed-condition raw-node upper bound
separately from the recovered wet-line normalization.

A repeatable local Roland Cloud JUNO-106 v2.0.2 comparison found median chorus
rates of 0.542613 Hz (estimator range 0.542609–0.542617) and 0.880013 Hz
(0.880011–0.880016) across 48/96 kHz captures and analysis windows. Their median
1.62181 ratio is within 0.103% of the schematic-derived 1.6234799 ratio,
corroborating the topology, but the absolute rates are about 2% slower and the
model is not an original-unit measurement; production constants therefore stay
unchanged. The reference exposes no I+II state and the authorization-limited
run could not identify delay endpoints.

BA662 signal nonlinearity/noise/thump and converter charge injection remain
unimplemented: available sources settle topology and nominal time constants,
not the original hybrid transfer or installed transient magnitude. A bypassed
A/B of guessed coloration would show only that the guess is audible.

After adding comparator/C56 tracking to idle fast-mode cards, the native
48 kHz/4× paired benchmark still measures the shipped Poly/Cubic/RK4 path at
3.115× Exact/Merson on the six-voice full-mixer Chorus-II case and 11.458×
while idle (about 67.9% and 91.3% less CPU). The fidelity correction therefore
retains the standing greater-than-10% CPU-saving acceptance floor.

## 2026-08-29 — C14 voltage coefficient withdrawn from the default

The optional C14 capacitance modulation remains available to the isolated
comparison renderer but no longer ships enabled. Its `0.15` coefficient had no
installed-part measurement, and the implementation drove it from the complete
bus voltage rather than the voltage across C14. More importantly, current
[aluminum-electrolytic manufacturer guidance](https://www.chemi-con.co.jp/en/faq/detail.php?id=alBiasVoltageChara)
states that voltage bias does not change this capacitor class's capacitance.
That general guidance is not a measurement of Roland's 1980s 10 uF non-polar
part under AC, so it does not prove the real distortion is zero; it does make
removing the guessed law the evidence-conservative default. OQ-21 remains open
for a level-swept transfer/THD or direct voltage-across-C14 capacitance
measurement, plus a switching capture. The internal comparison flag is not
serialized, so this correction also changes restored sessions at nonzero Unit
Character; preserving an unsupported default would be the less faithful
compatibility choice.

## 2026-08-28 — Fidelity-first quality default

New instances now request the deepest 4× QUALITY rung. The DCO passes the
project's numerical gates at every rung, but the BBD and VCF domains pass their
absolute gates only at 4× for common host rates; keeping 1× as the default was
therefore a CPU-first product choice at odds with the fidelity goal. Existing
sessions retain their stored choice, 1× and 2× remain available, and the
request is still capped against the host rate, so sufficiently high-rate hosts
do no redundant work.

## 2026-08-28 — Resonance control law

The circuit-derived linear-above-junction profile is now the default. The
module drawing traces the resonance CV through a grounded-base stage directly
into the BA662 control input, whose transconductance is linear in control
current; that is stronger physical evidence than the legacy quadratic-then-
linear compatibility voicing. Both retain the same service-calibrated maximum,
input compensation and self-oscillation correction, so this changes only the
intermediate slider response and adds no DSP work.

This is an evidence-priority decision under the realism/CPU goal, not a
listening verdict and not a closure of OQ-09: the 0.6 V junction onset and
0.2296 compensation coefficient still await a measured response family. The
legacy voiced curve remains available behind the internal comparison switch.

## 2026-08-28 — MN3009 transfer and default floor (noise rationale superseded)

The chorus write transfer now distinguishes the MN3009's guaranteed input-swing
limit from its typical distortion curve. It retains the existing 2.924 V rail,
fits 0.3% THD at 0.78 Vrms and approximately 2% at 2.0 Vrms, and remains below
the 2.5% guarantee at 1.5 Vrms. The prepared curve and slope changed, but the
realtime path is still the same 512-interval Hermite lookup with the same cost.

This revision initially treated 29.858% as a 59.716 uVrms inference from the
1.5 Vrms input-swing and 88 dB maximum-output S/N rows. The 2026-08-31 audit
above found those measurands incompatible and supersedes that rationale. HISS
100% and the default keep their numerical values only as explicit product and
session-compatibility policy; OQ-03 remains open for an identified, calibrated
instrument's absolute PSD.

A native Release benchmark against the pre-change tree measured the worst 1x
scenario at +0.4% CPU (the others ranged from -0.04% to +0.2%), comfortably
inside the goal's +20% ceiling.

## 2026-08-23 — VCF solver ladder

`VCF Solver` descends Max / High / Normal — Merson ×2, RK4 ×2, RK4 ×1, so
10/8/4 right-hand-side evaluations — every rung on the same seven control
nodes, so no rung moves a control node or a hold trajectory. The default
rung's error against an independent 96-substep reference is −162.551 dB
(4.2e-8 V peak).

**Verdict, by ear:** a blind four-letter set returned no audible difference
between rungs, so instances ship on **Normal**, roughly half the filter's CPU.
`EngineParameters` stays on Merson as the reference kernel that every frozen
fingerprint tests.

## 2026-08-24 — CPU defaults

The `Poly` tanh kernel and `Cubic` Fast Early form, plus the freewheel wake
that lets silent voice cards and a settled, switched-off chorus skip work.
Numerical evidence: a six-voice chord nulls at −89 dB against `Exact`;
self-oscillation anchors are identical to six decimals in amplitude and
0.003 cents in pitch. Sample nulls on resonant material decorrelate by phase
drift and are judged by anchor, as the earlier solver pass established.

The default flip and the freewheel wake are an audible-impact question the
measurements cannot close, so a four-take A/B set was rendered — A the
shipping configuration, B the new defaults; retrigger-after-silence, resonant
lead, self-oscillation, chorus engage; RMS-matched within 0.001 dB, no trims —
with its `key.md` unread by design.

**Verdict, by ear:** the player could not tell A from B on any take, so the
new defaults stand. Chosen by ear, not settled by a measurement.
`VCF Tanh = Exact` remains the one-menu revert.

## Pending

- **Vref = 0.775 V (OQ-06).** Roland's era convention, recorded as the
  standing candidate. Adoption is a product decision, not a listening
  question.

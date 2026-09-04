// Acustra: bounded offline-fit parameters for the physical model.

#pragma once

namespace acustra
{

struct MaterialCalibration
{
    // Nylon's bending stiffness comes from Woodhouse's measured per-string
    // EI table (nylonBendingEI in AcustraEngine.cpp), not a fitted scale on
    // a diameter-derived value, so nylon.stiffnessScale is inert: it stays
    // 1.0 and is not part of the calibration array. Steel still uses it.
    float stiffnessScale;
    float fundamentalT60Scale;
    float frequencyLossScale;
    float apertureScale;
    float transientScale;
    float pluckDistanceScale;
    float velocityBrightnessDepth;
};

struct PhysicalCalibration
{
    float bodyFrequencyScale;
    float bodyQScale;
    float bridgeMobilityScale;
    float residueTiltDbPerOctave;
    float directGain;
    MaterialCalibration nylon;
    MaterialCalibration steel;
    float apertureRegisterExponent { 1.0f };
    float lowBodyModeGain { 1.0f };
    float steelDisplacementScaleMetres { 0.0061f };
    float steelFretT60Slope { -0.030f };
    float highLossCutoffScale { 1.30f };
    // Above its modal-overlap frequency a plate's driving-point mobility tends
    // to a real constant (Cremer and Heckl, Structure-Borne Sound). A finite
    // positive-real modal fit cannot represent that dense overlap, so its
    // conductance collapses in the top of the band; this bounded real term
    // restores the floor. Being a positive real admittance it cannot make the
    // junction active.
    float bridgeConductanceFloor { 0.0f };
    float bridgeConductanceCornerHz { 1000.0f };
    // Length of string between the saddle and its anchor, which sets the
    // stiffness T/L of the spring each string presents to the bridge node.
    // DAFx-26 attaches the body at xi_b=0.995 and leaves whatever stub that
    // fraction implies; a real bridge anchors the string at a fixed distance
    // behind the saddle instead - roughly 12-16 mm at a steel-string's pins,
    // further at a classical's tie block - and the distance is not part of
    // the g21 measurement, so it is bounded and fitted rather than assumed.
    // Re-swept on the classical bridge rather than inherited from g21, over
    // E, G, Am, D and C chords. Worst separable note's pull early/late in
    // cents: 1.4/3.8 at the 3.25 mm stub, 2.1/2.4 at 8 mm, 2.9/3.2 at
    // 17.2 mm, 25.8/98.2 with no anchor at all. The coincident pairs of the
    // same sweep - a played fundamental landing on a lower played note's
    // partial, where the tracker reads the beat between them and not a pull -
    // run the other way: 58.1/12.4 at 3.25 mm, 28.2/10.2 at 8 mm, 14.1/26.7
    // at 17.2 mm. So the sweep settles that an anchor is needed and not which
    // length: 3.25 mm loses the late window to 8 mm and loses the coincident
    // column outright. It shipped at 3.25 mm - the bound's floor and DAFx-26's
    // own attachment, xi_b = 0.995 on a 650 mm scale, the only published
    // dimension among the candidates - until the 2026-09-04 refit, which took
    // it to 59.1 mm against a 60 mm ceiling and improved the tuning term on
    // all three splits (3.459 -> 2.869 training, 3.378 -> 3.304 development
    // validation, 2.429 -> 1.717 flat top). That is longer than the 12-16 mm
    // a steel string's pins give and longer than any candidate the chord
    // sweep covered, so the two measurements now disagree about this value:
    // the corpus wants a softer termination than the pull sweep does, and
    // nothing here has measured the pull at 59 mm.
    float bridgeTailLengthMetres { 0.020f };
    // Transverse motion stretches the string; DAFx-26's tension increase
    // EA/(2L) times the mean square slope is a force at the saddle, and it
    // resonates at the string's own longitudinal modes, n*c_long/(2L) with
    // c_long = sqrt(EA/mu). For this steel set that is 1.5 to 3.9 kHz and for
    // plain nylon 1.18 kHz, both from the construction data the transverse
    // model already uses. Because the drive is a squared slope it carries the
    // products of transverse partials, so what the resonators pass are the
    // sum and difference phantom partials rather than an added tone. Zero is
    // an exact no-op.
    float longitudinalGain { 0.0f };
    float longitudinalQ { 80.0f };
    // The two transverse polarisations of a real string are not in tune with
    // each other, and Woodhouse, "Plucked guitar transients: comparison of
    // measurements and synthesis", Acta Acustica 90 (2004) 945-965, Sec. 4.3
    // (https://euphonics.org/wp-content/uploads/2022/03/Guitar_II.pdf) shows
    // where the split comes from: not from the body, whose measured 2x2
    // admittance matrix splits the pair by only about 0.1 Hz, but from an end
    // correction at the terminations, the string rolling on the fret crown
    // and possibly the saddle. He measures the polarisation parallel to the
    // soundboard as the longer one - so the normal polarisation is the higher
    // member of the pair - by "about 0.8 mm in 650 mm" on the B string, and
    // calls that "more significant ... than that coming from the body
    // admittance matrix". This is that length, as a length: it is a total
    // over both terminations for an open string, not a per-termination
    // figure, and he publishes no law for how it varies from string to
    // string, so the same length goes to every string. It is bounded and
    // fittable rather than fixed because he calls the attribution tentative
    // and says the exact amount "would require detailed computation"; the
    // bound is one string diameter - his own remark that the correction is of
    // the order of the string diameter - taken as the 0.82 mm B string the
    // 0.8 mm was measured on (nylonDiameterMetres in AcustraEngine.cpp).
    float polarisationEndCorrectionMetres { 0.0008f };
};

// Refit on 2026-09-04 around the two-way junction and the saddle anchor, by a
// bounded pattern search rather than the one-sided Gauss-Newton stages that
// produced the previous vector (Tools/OptimizePhysicalModel.py says why the
// derivative was not trustworthy). Training 6.319236 -> 5.662377, development
// validation 6.327235 -> 5.893771, the eight flat-top rows 7.948337 ->
// 6.913712: all three splits improve, which is what the 2026-08-31 refit
// could not do. Five of the twenty-six free values sit on a bound - bodyQScale
// 0.0534 over a 0.05 floor, bridgeMobilityScale on 0.25, nylon's transient
// scale on 0, the fret T60 slope on -0.06 and the saddle-to-anchor length at
// 59.1 mm under a 60 mm ceiling - so on those five the bound is choosing the
// value and not the data.
// The 2026-09-04 refit, with two values pinned back to their measurements.
// A bounded pattern search over the 115-note benchmark moved every free value
// and improved all four splits, but it drove two onto bounds that contradict
// what the repository measured, and both are restored here:
//   * bridgeTailLengthMetres. The fit ran it to 59.1 mm against a 60 mm
//     ceiling, four times longer than any length the chord-pull sweep covered.
//     At 59.1 mm nylon's late chord pull reaches 8.7 cents against the 7-cent
//     bound the tuning work was promoted on; at DAFx-26's published 3.25 mm
//     stub it is 0.8, and held-out validation is BETTER pinned (5.731) than
//     free (5.894), so the long tail was fitting the training rows.
//   * bridgeMobilityScale. The fit ran it to its 0.25 floor, scaling a
//     measured mobility four times down, and that rail is what made a nylon
//     hammer-on quieter as it got harder (-0.35 dB per velocity step at all
//     three rates). Restored to the measured 0.754677154, the engine suite is
//     green with every bound at its former value except the two below.
// What the fit did move, and what it bought: training 6.319236 -> 6.111540,
// development validation 6.327235 -> 6.072344, and the eight never-fitted
// flat-top rows 7.948337 -> 8.073304 (1.6% worse, the one split that loses).
inline constexpr PhysicalCalibration fittedPhysicalCalibration {
    1.04f, 0.0534179688f, 0.754677154f, -1.0f, 0.0f,
    { 1.0f, 0.86484718f, 1.40369766f, 1.7688939f,
      0.0f, 1.12667139f, 0.0375f },
    { 0.749355465f, 1.53f, 0.52f, 0.643124355f,
      0.494086432f, 0.88819512f, 1.1859375f },
    -0.0706290118f, 7.5f, 0.00773577847f, -0.0597851562f, 2.28586032f,
    // The axial resonators are physically motivated, but without a measured
    // transfer level their narrow, high-Q onset reads as a pitched water-drop
    // transient rather than part of the pluck. Keep the calibrated mechanism
    // available for measurement work, but do not add that synthetic ping to
    // the shipping voice.
    0.011f, 2187.76023f, 0.00325f, 0.0f, 35.0f,
    // Woodhouse's published 0.8 mm, shipped frozen at the measurement rather
    // than fitted. The corpus does see the parallel loop, but only weakly and
    // only on steel: with the bridge-local direct path off, loops[1] reaches
    // the output through the shared slope energy in finishVoice, which on
    // steel drives the attack pitch. Setting this value to zero changes 40 of
    // the 79 corpus renders - all 32 steel and all 8 flat-top rows, while all
    // 39 nylon rows stay byte-identical - and moves the score from
    // 6.319236 / 6.327235 / 7.948337 to 6.320649 / 6.328354 / 7.953299
    // (training / validation / flat top, measured on the calibration that
    // shipped before the 2026-09-04 refit). That is a real preference for the
    // published value on all three splits, but far too small a lever to fit
    // an end correction against.
    0.0008f
};

} // namespace acustra

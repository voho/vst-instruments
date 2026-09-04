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
    // column outright. It ships because it is the bound's floor and DAFx-26's
    // own attachment, xi_b = 0.995 on a 650 mm scale - the only published
    // dimension among the candidates - not because it won the sweep.
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

inline constexpr PhysicalCalibration fittedPhysicalCalibration {
    1.04f, 0.097990747f, 0.754677154f, -1.0f, 0.0f,
    { 1.0f, 1.13984718f, 1.42440079f, 1.69750718f,
      0.182559111f, 1.09385889f, 0.000716432382f },
    { 1.48177734f, 1.53f, 0.52f, 0.65992123f,
      0.523383307f, 1.03116387f, 0.948115221f },
    -0.0706290118f, 7.5f, 0.00617327847f, -0.0301260746f, 2.16964938f,
    // The axial resonators are physically motivated, but without a measured
    // transfer level their narrow, high-Q onset reads as a pitched water-drop
    // transient rather than part of the pluck. Keep the calibrated mechanism
    // available for measurement work, but do not add that synthetic ping to
    // the shipping voice.
    0.011f, 2804.94773f, 0.00325f, 0.0f, 35.0f,
    // Woodhouse's published 0.8 mm. The corpus carries no signal for it while
    // the parallel polarisation does not radiate (its only outlet is the
    // bridge-local direct path, whose gain the fit put at zero), so the fit
    // does not move it and it ships at the measurement.
    0.0008f
};

} // namespace acustra

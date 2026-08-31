// Acustra: bounded offline-fit parameters for the physical model.

#pragma once

namespace acustra
{

struct MaterialCalibration
{
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
    float bridgeTailLengthMetres { 0.020f };
    // Transverse motion stretches the string; DAFx-26's tension increase
    // EA/(2L) times the mean square slope is a force at the saddle, and it
    // resonates at the string's own longitudinal modes, c_long/(2L) with
    // c_long = sqrt(EA/mu). For this steel set that is 1.5 to 3.9 kHz and for
    // plain nylon 1.18 kHz, both from the construction data the transverse
    // model already uses. Because the drive is a squared slope it carries the
    // products of transverse partials, so what the resonator passes are the
    // sum and difference phantom partials rather than an added tone. Zero is
    // an exact no-op.
    float longitudinalGain { 0.0f };
    float longitudinalQ { 80.0f };
};

inline constexpr PhysicalCalibration fittedPhysicalCalibration {
    1.04f, 0.097990747f, 0.754677154f, -1.0f, 0.0f,
    { 1.94971856f, 1.13984718f, 1.42440079f, 1.69750718f,
      0.182559111f, 1.09385889f, 0.000716432382f },
    { 1.48177734f, 1.53f, 0.52f, 0.65992123f,
      0.523383307f, 1.03116387f, 0.948115221f },
    -0.0706290118f, 7.5f, 0.00617327847f, -0.0301260746f, 2.16964938f,
    0.011f, 2804.94773f, 0.0171860529f, 0.03f, 35.0f
};

} // namespace acustra

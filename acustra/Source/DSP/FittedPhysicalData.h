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
};

inline constexpr PhysicalCalibration fittedPhysicalCalibration {
    1.04f, 0.0887389657f, 0.820061659f, 1.89075253f, 0.0f,
    { 1.79971856f, 1.13984718f, 1.31840079f, 1.69750718f,
      0.182559111f, 1.09385889f, 0.000716432382f },
    { 1.48177734f, 1.2668308f, 0.597412435f, 0.65992123f,
      0.523383307f, 1.03116387f, 0.852115221f },
    -0.0530251183f, 5.42528615f, 0.00617327847f, -0.0301260746f, 2.11353995f,
    0.00359595621f, 2138.61659f
};

} // namespace acustra

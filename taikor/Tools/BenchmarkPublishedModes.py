#!/usr/bin/env python3
"""Compare single-head frequency hypotheses with Suzuki et al. (2009), Table 2.

Source: https://doi.org/10.1250/ast.30.348
PDF: https://www.jstage.jst.go.jp/article/ast/30/5/30_5_348/_pdf
Theoretical and experimental studies on the resonance frequencies of a stretched
circular plate: Application to Japanese drum diaphragms, AST 30(5), 348-354.

Table 2 labels (m,n) count interior nodal circles and nodal lines respectively;
the corresponding membrane root is the (m+1)-th zero of J_n. Frequencies below
are measured values. C/SS ratios and inferred tensions are theoretical results.
The printed second measured ratio, 1.567, disagrees with 198.1/126.9; use Hz.

This is an offline comparison of equations, not an execution or calibration of
the full Taikor engine. Its single-head approximation mirrors stiffnessStretch
and membraneModeOmegas in Source/DSP/TaikoEngine.cpp. Published material values
replace the instrument's effective design values; no doublet, cavity, nonlinear
attack, open-body/floor loading or microphone model is included. A common scale
is removed after prediction; it is not a fitted physical tension or stiffness.
"""

import argparse
import math


# Table 2, in its original order: (nodal circles, nodal lines), measured Hz,
# ideal-membrane Bessel root. Roots also occur in TaikoEngine::membraneModes.
MODES = (
    ((0, 0), 126.9, 2.4048255576957728),
    ((0, 1), 198.1, 3.8317059702075123),
    ((0, 2), 271.3, 5.1356223018406826),
    ((1, 0), 287.5, 5.5200781102863106),
    ((0, 3), 331.3, 6.3801618959239835),
    ((1, 1), 346.9, 7.0155866698156188),
    ((0, 4), 378.8, 7.5883424345038044),
    ((1, 2), 418.1, 8.4172441403998649),
)
PAPER_C = (1, 1.610, 2.187, 2.361, 2.758, 3.061, 3.339, 3.754)
PAPER_SS = (1, 1.609, 2.181, 2.354, 2.749, 3.047, 3.322, 3.731)
MEASURED = tuple(hz for _, hz, _ in MODES)
RADIUS = 0.23  # Section 4's effective radius r1, not half the 0.48 m diameter.
MODULUS, DENSITY, THICKNESS, POISSON = 3.5e9, 2000.0, 0.002, 0.3
RIGIDITY = MODULUS * THICKNESS**3 / (12 * (1 - POISSON**2))
AREAL_DENSITY = DENSITY * THICKNESS
AIR_DENSITY = 1.2041  # Taikor's 20 C assumption, not reported measurement weather.


def taikor_shape(tension, rigidity=RIGIDITY, density=AREAL_DENSITY,
                 air_density=AIR_DENSITY):
    """Frequency shape only: omit common wave speed/radius, retain modal load."""
    stiffness = rigidity / (tension * RADIUS**2)
    values = []
    for (_, order), _, root in MODES:
        stretch = math.sqrt((1 + stiffness * root**2)
                            / (1 + stiffness * 5.7831859629467))
        # These 0.85/0.6/2.4048 constants belong to the engine hypothesis.
        load_shape = (2.4048 / root) / (1 + 0.6 * order)
        load = math.sqrt(1 + 0.85 * load_shape * air_density * RADIUS / density)
        values.append(root * stretch / load)
    return values


def cents_errors(predicted, measured, anchor=True):
    """Remove fundamental offset or least-squares common offset in log Hz."""
    raw = [1200 * math.log2(p / m) for p, m in zip(predicted, measured)]
    offset = raw[0] if anchor else sum(raw) / len(raw)
    return [error - offset for error in raw]


def rms(values):
    return math.sqrt(sum(value**2 for value in values) / len(values))


def self_test():
    ideal = [root for _, _, root in MODES]
    assert len(MODES) == len(PAPER_C) == len(PAPER_SS) == 8
    assert all(a < b for a, b in zip(MEASURED, MEASURED[1:]))
    # Check the index mapping against independently tabulated ideal ratios.
    paper_ideal = (1, 1.593, 2.136, 2.295, 2.653, 2.917, 3.156, 3.500)
    assert max(abs(root / ideal[0] - ratio)
               for root, ratio in zip(ideal, paper_ideal)) < 0.0006
    assert math.isclose(MEASURED[1] / MEASURED[0], 1.56107171000788)
    # Analytic limits: zero bending and zero air recover the membrane; dominant
    # bending recovers plate-like root-squared frequency ratios.
    assert taikor_shape(20800, rigidity=0, air_density=0) == ideal
    plate = taikor_shape(20800, rigidity=1e15, air_density=0)
    assert max(abs(p / plate[0] - (root / ideal[0])**2)
               for p, root in zip(plate, ideal)) < 1e-9
    heavy = taikor_shape(20800, rigidity=0, density=1e15)
    assert max(abs(p - root) for p, root in zip(heavy, ideal)) < 1e-12
    # All-mode fitting and first-mode anchoring must both remove global tuning.
    for anchor in (False, True):
        assert max(abs(e) for e in cents_errors(
            [3.7 * value for value in MEASURED], MEASURED, anchor)) < 1e-10
    detuned = list(MEASURED)
    detuned[-1] *= 2**(100 / 1200)
    assert math.isclose(cents_errors(detuned, MEASURED)[-1], 100)
    fitted = cents_errors(detuned, MEASURED, anchor=False)
    assert abs(sum(fitted)) < 1e-10
    assert math.isclose(rms(fitted), 100 * math.sqrt(7) / 8)


def report():
    models = {
        "Ideal": [root for _, _, root in MODES],
        "Paper C": PAPER_C,
        "Paper SS": PAPER_SS,
        "Law 20.8": taikor_shape(20800),
        "Law 23.1": taikor_shape(23100),
    }
    errors = {name: cents_errors(values, MEASURED) for name, values in models.items()}
    print("Suzuki 2009 Table 2 | https://doi.org/10.1250/ast.30.348")
    print("Signed cents error after fundamental anchoring; positive = predicted too high.")
    print(f"{'Mode':>6} {'Hz':>7} {'Ratio':>8}" + ''.join(f"{name:>11}" for name in models))
    for index, (mode, hz, _) in enumerate(MODES):
        label = f"({mode[0]},{mode[1]})"
        print(f"{label:>6} {hz:7.1f} {hz / MEASURED[0]:8.5f}"
              + ''.join(f"{values[index]:11.2f}" for values in errors.values()))
    print("\nRMS cents (all 8 modes); fitted removes one common log-frequency offset:")
    for name, values in models.items():
        fitted = cents_errors(values, MEASURED, anchor=False)
        print(f"  {name:9} anchored={rms(errors[name]):7.2f}  fitted={rms(fitted):7.2f}")
    print("\nPaper C/SS: published clamped/simply-supported theories, not measurements.")
    print("Law 20.8/23.1: Taikor single-head formula at the paper's inferred tensions")
    print("in kN/m; these are sensitivity cases, not solved C/SS boundary conditions.")
    print("Inputs: r1=0.23 m, E=3.5 GPa, rho=2000 kg/m3, h=2 mm, nu=0.3.")
    print("Engine-only air assumptions: rho_air=1.2041 kg/m3, load factors 0.85/0.6.")
    print("Measured ratio uses Hz: printed 1.567 disagrees with 198.1/126.9=1.56107.")
    print("Setup: 0.48 m drum, 0.62 m body, rear head removed, open side toward floor;")
    print("hit 0.07 m from edge, mic 0.10 m above near hit, floor gaps 0/0.06/0.46 m.")
    print("Mode labels follow the floor-seated spectrum; a sub-100 Hz lifted-body peak")
    print("was excluded by the authors. Actual rim support and acoustic loading remain")
    print("unresolved. No floor/open-body model, raw audio, or measurement uncertainty")
    print("is supplied here. These errors are hypothesis checks, not plugin calibration.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="check data and analytic limits")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("Published-mode benchmark self-test passed.")
    else:
        report()

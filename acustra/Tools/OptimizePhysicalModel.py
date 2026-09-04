#!/usr/bin/env python3
"""Fit Acustra's bounded physical parameters to the reference recordings.

The C++ renderer owns synthesis and the Python scorer owns descriptors.  This
driver exports targets once, asks the renderer to replace model files for each
candidate, and performs short bounded least-squares stages for the shared body,
nylon strings, steel strings, then the shared body once more.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

import numpy as np
from scipy.optimize import least_squares

from FitPhysicalModel import PreparedManifest


NAMES = (
    "bodyFrequencyScale",
    "bodyQScale",
    "bridgeMobilityScale",
    "residueTiltDbPerOctave",
    "directGain",
    "nylon.fundamentalT60Scale",
    "nylon.frequencyLossScale",
    "nylon.apertureScale",
    "nylon.transientScale",
    "nylon.pluckDistanceScale",
    "nylon.velocityBrightnessDepth",
    "steel.stiffnessScale",
    "steel.fundamentalT60Scale",
    "steel.frequencyLossScale",
    "steel.apertureScale",
    "steel.transientScale",
    "steel.pluckDistanceScale",
    "steel.velocityBrightnessDepth",
    "apertureRegisterExponent",
    "lowBodyModeGain",
    "steelDisplacementScaleMetres",
    "steelFretT60Slope",
    "highLossCutoffScale",
    "bridgeConductanceFloor",
    "bridgeConductanceCornerHz",
    "bridgeTailLengthMetres",
    "longitudinalGain",
    "longitudinalQ",
    "polarisationEndCorrectionMetres",
)
LOWER = np.asarray((
    0.96, 0.05, 0.25, -6.0, 0.0,
    0.4, 0.35, 0.35, 0.0, 0.7, 0.0,
    0.25, 0.4, 0.35, 0.35, 0.0, 0.7, 0.0,
    -1.0, 0.25, 0.0, -0.06, 0.5, 0.0, 100.0, 0.00325, 0.0, 10.0, 0.0,
))
UPPER = np.asarray((
    1.04, 1.8, 4.0, 6.0, 0.12,
    2.0, 3.0, 2.5, 3.0, 1.3, 1.2,
    4.0, 2.0, 3.0, 2.5, 3.0, 1.3, 1.2,
    1.0, 32.0, 0.04, 0.05, 4.0, 0.02, 8000.0, 0.060, 0.5, 400.0, 0.82e-3,
))
INITIAL = np.asarray((
    1.0, 1.0, 1.0, 0.0, 0.0,
    1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
    1.0, 1.0, 0.0061, -0.030, 1.30, 0.0, 1000.0, 0.020, 0.0, 80.0, 0.0008,
))
# The bridge-local direct path is deliberately fixed off. Its score direction
# was flat (and slightly worse on validation), so fitting it only lets a
# numerical solver choose an arbitrary raw-string mixture.
# Values a listening verdict chose are not refit. The corpus disagrees with
# them by construction - that disagreement is why they went to a listener - so
# leaving them free would simply undo the verdict on the next run.
BY_EAR = (
    "residueTiltDbPerOctave",
    "lowBodyModeGain",
    "steel.fundamentalT60Scale",
    "steel.frequencyLossScale",
    "bridgeConductanceFloor",
)
# Values that are a published measurement rather than a fit. The corpus does
# see the end correction, but only weakly and only on steel: with the
# bridge-local direct path off, the polarisation it lengthens still reaches
# the output through the shared slope energy that drives the steel attack
# pitch. Zeroing it changes 40 of the 79 renders (all 32 steel and all 8 flat
# top; all 39 nylon byte-identical) and moves the score from
# 6.319236 / 6.327235 / 7.948337 to 6.320649 / 6.328354 / 7.953299 - a real
# preference for the published value, and far too small a lever to fit it on.
MEASURED = (
    "polarisationEndCorrectionMetres",
)
FROZEN = frozenset(NAMES.index(name) for name in BY_EAR + MEASURED)


def _free(indices: np.ndarray) -> np.ndarray:
    return np.asarray([index for index in indices if index not in FROZEN],
                      dtype=int)


GLOBAL = _free(np.asarray((0, 1, 2, 3, 18, 19, 22, 23, 24, 25, 26, 27, 28)))
NYLON = _free(np.arange(5, 11))
STEEL = _free(np.append(np.arange(11, 18), (20, 21)))


def _command(renderer: Path, directory: Path, values: np.ndarray,
             models_only: bool) -> list[str]:
    command = [str(renderer)]
    if models_only:
        command.append("--models-only")
    command.append(str(directory))
    command.extend(format(float(value), ".9g") for value in values)
    return command


def _run_renderer(renderer: Path, directory: Path, values: np.ndarray,
                  models_only: bool) -> None:
    completed = subprocess.run(
        _command(renderer, directory, values, models_only),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"renderer failed ({completed.returncode}):\n{completed.stdout}"
        )


def _small_report(report: dict[str, Any]) -> dict[str, Any]:
    return {
        "score": report["score"],
        "example_count": report["example_count"],
        "unique_model_count": report["unique_model_count"],
        "terms": report["terms"],
    }


class Objective:
    def __init__(self, renderer: Path, directory: Path,
                 manifest: PreparedManifest, base: np.ndarray,
                 active: np.ndarray, material: str | None,
                 evaluations: list[dict[str, Any]]):
        self.renderer = renderer
        self.directory = directory
        self.manifest = manifest
        self.base = base.copy()
        self.active = active
        self.material = material
        self.evaluations = evaluations
        self.cache: dict[tuple[float, ...], np.ndarray] = {}
        self.best_values = base.copy()
        self.best_score = float("inf")

    def values(self, unit: np.ndarray) -> np.ndarray:
        values = self.base.copy()
        values[self.active] = LOWER[self.active] + unit * (
            UPPER[self.active] - LOWER[self.active]
        )
        return values

    def __call__(self, unit: np.ndarray) -> np.ndarray:
        values = self.values(unit)
        # The C++ boundary is float, so parameters that serialize identically
        # are the same physical candidate and need only one render.
        key = tuple(float(np.float32(value)) for value in values)
        cached = self.cache.get(key)
        if cached is not None:
            return cached
        _run_renderer(self.renderer, self.directory, values, True)
        report = self.manifest.score(material=self.material)
        residuals = np.asarray(report["weighted_residuals"], dtype=np.float64)
        score = float(report["score"])
        if score < self.best_score:
            self.best_score = score
            self.best_values = values.copy()
        self.evaluations.append({
            "stage_material": self.material,
            "values": values.tolist(),
            **_small_report(report),
        })
        print(
            f"eval {len(self.evaluations):03d} "
            f"{self.material or 'both':>5} score={score:.6f}",
            flush=True,
        )
        self.cache[key] = residuals
        return residuals

    def jacobian(self, unit: np.ndarray) -> np.ndarray:
        """One-sided finite differences on a fixed fraction of each bound."""
        baseline = self(unit)
        result = np.empty((baseline.size, unit.size), dtype=np.float64)
        for index in range(unit.size):
            candidate = unit.copy()
            step = 0.04 if unit[index] <= 0.96 else -0.04
            candidate[index] += step
            result[:, index] = (self(candidate) - baseline) / step
        return result


def _fit_stage(name: str, material: str | None, active: np.ndarray,
               values: np.ndarray, renderer: Path, directory: Path,
               manifest: PreparedManifest, max_nfev: int,
               evaluations: list[dict[str, Any]]) -> tuple[np.ndarray, dict[str, Any]]:
    unit = (values[active] - LOWER[active]) / (UPPER[active] - LOWER[active])
    objective = Objective(
        renderer, directory, manifest, values, active, material, evaluations
    )
    result = least_squares(
        objective,
        unit,
        jac=objective.jacobian,
        bounds=(np.zeros(unit.size), np.ones(unit.size)),
        x_scale="jac",
        max_nfev=max_nfev,
        ftol=2.0e-3,
        xtol=2.0e-3,
        gtol=2.0e-3,
        verbose=0,
    )
    # least_squares may stop immediately after a Jacobian probe. Preserve the
    # lowest actual candidate, not merely the last vector it returned.
    fitted = objective.best_values
    print(
        f"stage {name}: {objective.best_score:.6f}; "
        f"status={result.status} nfev={result.nfev}",
        flush=True,
    )
    return fitted, {
        "name": name,
        "material": material,
        "active": [NAMES[index] for index in active],
        "best_score": objective.best_score,
        "status": int(result.status),
        "message": result.message,
        "nfev": int(result.nfev),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--max-nfev", type=int, default=3,
        help="least-squares evaluations per stage (default: 3)",
    )
    parser.add_argument(
        "--resume", action="store_true",
        help="reuse an existing renderer corpus and its current calibration",
    )
    arguments = parser.parse_args()
    if arguments.max_nfev < 1:
        parser.error("--max-nfev must be positive")
    renderer = arguments.renderer.resolve()
    output = arguments.output.resolve()
    if not renderer.is_file():
        parser.error(f"renderer does not exist: {renderer}")

    values = INITIAL.copy()
    if arguments.resume:
        manifest_path = output / "train.json"
        if not manifest_path.is_file():
            parser.error("--resume output has no train.json")
        result_path = output / "fit-result.json"
        if result_path.is_file():
            result_data = json.loads(result_path.read_text(encoding="utf-8"))
            candidate = np.asarray(result_data.get("values", []), dtype=float)
            order = result_data.get("parameter_order")
            if not isinstance(order, list) or len(order) != candidate.size:
                manifest_data = json.loads(
                    manifest_path.read_text(encoding="utf-8")
                )
                order = manifest_data.get("calibration_order")
            if isinstance(order, list) and len(order) == candidate.size:
                migrated = INITIAL.copy()
                destination = {name: index for index, name in enumerate(NAMES)}
                aliases = {
                    "steel.displacementScaleMetres":
                        "steelDisplacementScaleMetres",
                }
                for value, name in zip(candidate, order):
                    target = destination.get(aliases.get(name, name))
                    if target is not None:
                        migrated[target] = value
                candidate = migrated
            elif candidate.size == 24:
                # The temporary all-material KC layout stored nylon at 21,
                # steel at 22 and the fret slope at 23.
                candidate = np.append(np.delete(candidate, 21), INITIAL[-1])
            elif 19 <= candidate.size < INITIAL.size:
                candidate = np.append(candidate, INITIAL[candidate.size:])
            if candidate.shape != INITIAL.shape or not np.all(np.isfinite(candidate)):
                parser.error(
                    "fit-result.json has no valid 19- through 24-value "
                    "calibration"
                )
            values = np.clip(candidate, LOWER, UPPER)
        values[4] = 0.0
        _run_renderer(renderer, output, values, True)
    else:
        if output.exists():
            parser.error("output already exists; use a new path or --resume")
        _run_renderer(renderer, output, values, False)

    train = PreparedManifest(output / "train.json")
    baseline = train.score()
    print(f"baseline train score={baseline['score']:.6f}", flush=True)
    evaluations: list[dict[str, Any]] = []
    stages: list[dict[str, Any]] = []
    for name, material, active in (
        ("shared-body", None, GLOBAL),
        ("nylon-string", "nylon", NYLON),
        ("steel-string", "steel", STEEL),
        ("shared-body-refine", None, GLOBAL),
    ):
        values, stage = _fit_stage(
            name, material, active, values, renderer, output, train,
            arguments.max_nfev, evaluations,
        )
        stages.append(stage)

    _run_renderer(renderer, output, values, True)
    final_train = train.score()
    validation = PreparedManifest(output / "validation.json").score()
    result = {
        "parameter_order": NAMES,
        "values": values.tolist(),
        "baseline_train": _small_report(baseline),
        "final_train": _small_report(final_train),
        "validation": _small_report(validation),
        "stages": stages,
        "evaluations": evaluations,
    }
    (output / "fit-result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print("fitted values:", " ".join(format(value, ".9g") for value in values))
    print(f"final train score={final_train['score']:.6f}")
    print(f"validation score={validation['score']:.6f}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"OptimizePhysicalModel: {error}", file=sys.stderr)
        raise SystemExit(1)

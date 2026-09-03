#!/usr/bin/env python3
"""Print the frozen dry-note benchmark in a compact, reviewable form.

The input is the committed output of the real-recording comparison pipeline,
not a second scorer.  This deliberately performs no DSP: it validates the
minimum report schema and exposes the split, material, and promoted-mechanism
results without requiring a reviewer to inspect a large JSON document.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def _number(value: Any, label: str) -> float:
    if not isinstance(value, (int, float)):
        raise ValueError(f"{label} must be numeric")
    return float(value)


def _floor_lines(report: dict[str, Any]) -> list[str]:
    """The model's distance from the recordings, in units of the distance two
    recordings of the same note already are from each other.

    A term's score is not readable on its own: the corpus's own spread sets how
    small it can be, and that spread is different for every term. The archtop
    was captured four times per note and layer, so the same scorer can be run
    recording against recording, and the ratio below says how much of each term
    is still the model. Both columns of a row are measured against the same
    targets -- the floor undoes the export's per-zone playback trim on the
    level descriptor, so the model column is rescored against the corrected
    targets too, and ``level_term_basis`` says what that leaves on each side.
    """
    floor = report.get("recording_floor")
    if not isinstance(floor, dict):
        raise ValueError("report has no recording_floor evidence")
    model_terms = floor["model"]["terms"]
    floor_terms = floor["floor"]["terms"]
    lines = [
        "",
        f'Recording-versus-recording floor ({floor["material"]}, '
        f'{floor["pair_count"]} take pairs from {floor["example_count"]} '
        f'{floor["split"]} takes, model and control on the floor\'s own '
        f'targets):',
        "| Term | Model | Floor | Model/floor |",
        "| --- | ---: | ---: | ---: |",
    ]
    for term in list(model_terms) + ["score"]:
        model = (_number(floor["model"]["score"], "recording_floor.model.score")
                 if term == "score" else _number(model_terms[term], term))
        value = (_number(floor["floor"]["score"], "recording_floor.floor.score")
                 if term == "score" else _number(floor_terms[term], term))
        label = "aggregate" if term == "score" else term
        lines.append(f"| {label} | {model:.4f} | {value:.4f} | "
                     f"{model / value:.2f}x |")
    above = floor.get("control_above_floor_terms")
    if isinstance(above, list) and above:
        lines.append(
            "The sample-player control scores below this floor on every term "
            "except " + " and ".join(
                filter(None, [", ".join(str(term) for term in above[:-1]),
                              str(above[-1])]))
            + ", which therefore measure the player rather than bounding the "
              "model: " + str(floor.get("control_note", "")))
    basis = floor.get("level_term_basis")
    if basis:
        lines.append("Level term: " + str(basis))
    return lines


def summarize(report: dict[str, Any]) -> str:
    splits = report.get("splits")
    if not isinstance(splits, dict):
        raise ValueError("report has no splits object")

    lines = [
        "| Real dry-note split | Notes | Neutral | Shipping | Change |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for key, label in (
        ("training", "Training"),
        ("held_out", "Development validation"),
        ("flat_top", "Independent flat-top"),
    ):
        split = splits.get(key)
        if not isinstance(split, dict):
            raise ValueError(f"report has no {key} split")
        count = split.get("examples")
        if not isinstance(count, int) or count < 1:
            raise ValueError(f"{key}.examples must be a positive integer")
        neutral = _number(split.get("neutral_score"), f"{key}.neutral_score")
        shipping = _number(split.get("fitted_score"), f"{key}.fitted_score")
        change = 100.0 * (shipping / neutral - 1.0)
        lines.append(
            f"| {label} | {count} | {neutral:.4f} | {shipping:.4f} | "
            f"{change:+.1f}% |"
        )

    sample_baseline = report.get("sample_v1_baseline")
    if not isinstance(sample_baseline, dict):
        raise ValueError("report has no sample_v1_baseline evidence")
    lines.extend([
        "",
        "Historical sample-player control (same source captures):",
        "| Split | Sample v1 score |",
        "| --- | ---: |",
    ])
    for key, label in (
        ("training", "Training"),
        ("held_out", "Development validation"),
        ("flat_top", "Independent flat-top"),
    ):
        lines.append(
            f"| {label} | "
            f"{_number(sample_baseline.get(key), f'sample_v1_baseline.{key}'):.4f} |"
        )
    lines.append(
        "Control only: v1 plays the benchmark recordings themselves, so its "
        "score is not an out-of-sample realism result."
    )
    lines.extend(_floor_lines(report))

    lines.extend(["", "Promoted realism paths retained in the shipping engine:"])
    paths = (
        ("bridge_modal_extension", "50-mode measured passive bridge"),
        ("plate_conductance_floor", "high-frequency plate conductance"),
        ("constant_saddle_anchor", "constant six-string saddle anchor"),
        ("junction_transient_corrections", "release/junction transient correction"),
        ("longitudinal_modes",
         "longitudinal string modes (calibrated, shipping gain zero)"),
    )
    for key, label in paths:
        if key not in report:
            raise ValueError(f"report has no {key} evidence")
        lines.append(f"- {label}")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "report",
        type=Path,
        nargs="?",
        default=Path(__file__).parents[1] / "Docs" / "physical-fit-report.json",
    )
    arguments = parser.parse_args()
    with arguments.report.open(encoding="utf-8") as stream:
        report = json.load(stream)
    if not isinstance(report, dict):
        raise ValueError("report root must be an object")
    print(summarize(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

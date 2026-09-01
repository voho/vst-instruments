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

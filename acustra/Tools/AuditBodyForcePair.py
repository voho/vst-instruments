#!/usr/bin/env python3
"""Audit the two measured normal-force radiation inputs of Mores g21/g34.

For bass/treble forces Fb,Ft, F=Fb+Ft and T=Ft-Fb give

    Hheave=(Htreble+Hbass)/2, Hrock=(Htreble-Hbass)/2,
    pressure=Hheave*F+Hrock*T.

This invertible basis change identifies two measured forcing templates under
linear superposition. Interpreting T as M/a requires impact half-spacing a;
interpreting the pair as a complete saddle heave/rock model additionally assumes
rigidity. It does not measure horizontal-force radiation, lateral translation,
or a rotation axis. A held-out center impact checks only the even/heave part.
Accelerometer offsets limit combining this map with the existing approximate
bridge mobility, but do not prevent subtraction of measured microphone FRFs.

Use the existing verified raw archive, calibrated complex H1 extraction, bands,
and common full-record/62.5ms/16ms tapers from AuditBridgeSpatialMap. Preserve
relative phase: no individual minimum-phase conversion, gain or delay fitting.
Unmasked norm ratios compare unit F and unit T pressure, not equal mechanical
energy. Secondary magnitude/phase summaries retain the existing 1%-of-band-peak
mask and report coverage. Sixth-octave errors expose local interpolation failures.
The selected archive has no repeats for uncertainty estimates. Both guitars
were nylon-strung; this does not create a measured steel body or change a model.

    python3 Tools/AuditBodyForcePair.py --self-test
    python3 Tools/AuditBodyForcePair.py --raw-mat /path/qualified_selected_impulses.mat --output /new/audit-directory
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import sys

import numpy as np
import scipy

import AuditBridgeSpatialMap as spatial

MICROPHONES = {3: "upper_mic", 4: "treble_mic", 5: "bass_mic"}


def force_pair(bass: np.ndarray, treble: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    return 0.5 * (treble + bass), 0.5 * (treble - bass)


def ratio_metrics(heave: np.ndarray, rock: np.ndarray) -> dict:
    heave_norm, rock_norm = np.linalg.norm(heave), np.linalg.norm(rock)
    if (not np.isfinite(heave).all() or not np.isfinite(rock).all()
        or heave_norm <= 0):
        raise ValueError("nonfinite response or zero heave reference band")
    floor = max(abs(heave).max(), abs(rock).max()) * 0.01
    active = (abs(heave) > floor) & (abs(rock) > floor)
    phase = np.angle(rock[active] * np.conj(heave[active]))
    circular_mean = np.exp(1j * phase).mean() if len(phase) else 0j
    db = 20 * np.log10(abs(rock[active]) / abs(heave[active]))
    return {
        "rock_over_heave_norm_ratio": float(rock_norm / heave_norm),
        "rock_over_heave_norm_db": float(20 * np.log10(rock_norm / heave_norm))
            if rock_norm > 0 else None,
        "rock_over_heave_magnitude_median_db": float(np.median(db)) if len(db) else None,
        "rock_minus_heave_phase_circular_mean_deg": float(np.angle(circular_mean, deg=True))
            if len(phase) else None,
        "phase_circular_resultant_length": float(abs(circular_mean)) if len(phase) else None,
        "rock_minus_heave_phase_abs_median_deg": float(np.median(abs(phase)) * 180 / np.pi)
            if len(phase) else None,
        "active_fraction": float(active.mean()), "bins": len(heave),
    }


def self_test() -> None:
    frequency = np.fft.rfftfreq(spatial.bridge.FFT_SIZE, 1 / spatial.bridge.SAMPLE_RATE)
    omega = 2 * np.pi * frequency / spatial.bridge.SAMPLE_RATE
    bass, treble = np.exp(-2j * omega), np.exp(-5j * omega)
    heave, rock = force_pair(bass, treble)
    if (not np.allclose(heave - rock, bass, atol=1e-12, rtol=0)
        or not np.allclose(heave + rock, treble, atol=1e-12, rtol=0)):
        raise AssertionError("force-pair basis did not reconstruct both measured inputs")
    force, moment = 1.2 - 0.4j, -0.7 + 0.3j
    direct = bass * (force - moment) / 2 + treble * (force + moment) / 2
    if not np.allclose(heave * force + rock * moment, direct, atol=1e-12, rtol=0):
        raise AssertionError("complex generalized-load pressure identity failed")

    common_phase = np.exp(-11j * omega)
    common_heave, common_rock = force_pair(bass * common_phase, treble * common_phase)
    if (not np.allclose(common_heave, heave * common_phase, atol=1e-12, rtol=0)
        or not np.allclose(common_rock, rock * common_phase, atol=1e-12, rtol=0)):
        raise AssertionError("common phase reference changed the force-pair map")
    # These two impulses have equal magnitude spectra but different delays.
    # Independent minimum phase collapses their differential response to zero.
    _, destroyed_rock = force_pair(
        spatial.body.minimum_phase_from_magnitude(abs(bass)),
        spatial.body.minimum_phase_from_magnitude(abs(treble)))
    if np.linalg.norm(rock) < 1 or np.linalg.norm(destroyed_rock) > 1e-10:
        raise AssertionError("independent minimum-phase cancellation failure was not exposed")
    for _, keep in spatial.WINDOWS:
        windowed_pair = force_pair(spatial.windowed(bass, keep), spatial.windowed(treble, keep))
        if any(not np.allclose(actual, spatial.windowed(expected, keep), atol=1e-12, rtol=0)
               for actual, expected in zip(windowed_pair, (heave, rock))):
            raise AssertionError("common linear taper did not commute with basis change")
    print("Body force-pair self-test passed")


def run(raw: Path, output: Path) -> None:
    if output.exists() or not output.parent.is_dir():
        raise ValueError("output must be a new directory inside an existing parent")
    responses, force_quality = spatial.extract(spatial.bridge.load_matrix(raw))
    frequency = np.fft.rfftfreq(spatial.bridge.FFT_SIZE, 1 / spatial.bridge.SAMPLE_RATE)
    center_bands, center_narrow = spatial.audit(frequency, responses)
    center_bands = {(r["guitar"], r["channel"], r["window"], r["low_hz"], r["high_hz"]): r
                    for r in center_bands}
    center_narrow = {(r["guitar"], r["channel"], r["center_hz"]): r for r in center_narrow}
    broad, narrow, maps = [], [], {}
    reconstruction_error = 0.0
    for guitar in (21, 34):
        for channel, name in MICROPHONES.items():
            bass, treble = responses[guitar, 0, channel], responses[guitar, 2, channel]
            heave, rock = force_pair(bass, treble)
            for actual, expected in ((heave - rock, bass), (heave + rock, treble)):
                reconstruction_error = max(reconstruction_error,
                    float(np.linalg.norm(actual - expected) / np.linalg.norm(expected)))
            maps[f"g{guitar}_{name}_heave"], maps[f"g{guitar}_{name}_rock"] = heave, rock
            for window, keep in spatial.WINDOWS:
                h, r = spatial.windowed(heave, keep), spatial.windowed(rock, keep)
                for low, high in spatial.BANDS:
                    band = (frequency >= low) & (frequency < high)
                    broad.append(dict(guitar=guitar, microphone=name, window=window,
                        low_hz=low, high_hz=high, **ratio_metrics(h[band], r[band]),
                        held_out_center=center_bands[guitar, name, window, low, high]))
            for key, center in center_narrow.items():
                if key[:2] != (guitar, name):
                    continue
                hz, edge = key[2], 2 ** (1 / 12)
                band = (frequency >= hz / edge) & (frequency < hz * edge)
                narrow.append(dict(guitar=guitar, microphone=name, center_hz=hz,
                    **ratio_metrics(heave[band], rock[band]),
                    held_out_center_complex_relative_l2=center["complex_relative_l2"]))
    report = {
        "source": str(raw.resolve()), "source_url": "https://zenodo.org/records/4604577",
        "raw_md5": spatial.bridge.digest(raw), "raw_sha256": spatial.sha256(raw),
        "tool_sha256": spatial.sha256(Path(__file__)),
        "helper_sha256": {name: spatial.sha256(Path(__file__).with_name(name)) for name in
                          ("AuditBridgeSpatialMap.py", "GenerateMeasuredBridge.py", "GenerateMeasuredBody.py")},
        "versions": {"python": sys.version.split()[0], "numpy": np.__version__, "scipy": scipy.__version__},
        "sample_rate": spatial.bridge.SAMPLE_RATE, "fft_size": spatial.bridge.FFT_SIZE,
        "units": {"F": "N", "T": "N, normalized moment M/a", "Hheave_and_Hrock": "Pa/N",
                  "actual_moment_response": "Hrock/a in Pa/(N m), requiring impact half-spacing a"},
        "definitions": {"F": "Fb+Ft", "T": "Ft-Fb", "Hheave": "(Ht+Hb)/2",
                        "Hrock": "(Ht-Hb)/2", "pressure": "Hheave*F+Hrock*T"},
        "maximum_endpoint_reconstruction_relative_l2": reconstruction_error,
        "method": __doc__,
        "geometry_limits": [
            "Under equally spaced strings, impacts between E/A and B/E imply u=(stringIndex-2.5)/2. Outer strings at +/-1.25 extend beyond the measured endpoints.",
            "Center agreement validates even spatial interpolation only. Opposite changes to endpoint responses can alter Hrock without changing their mean.",
            "Microphone force-pair subtraction does not use accelerometers. The existing bridge mobility fit assumes collocation despite sensors behind the saddle and unreported spacing.",
            "Published HSaT is lower string edge above the top, not height above an identified rotation axis. It does not independently determine horizontal-force conversion.",
            "A circular phase mean with small resultant length conceals dispersed phases; inspect sixth-octave rows and the saved complex map.",
            "Unit generalized-force pressure ratios are not mechanical-energy or playing-level ratios. Endpoint reconstruction is algebraic, not independent physical validation.",
        ],
        "force_quality": force_quality, "bands": broad, "sixth_octaves": narrow,
    }
    # Complete validation/calculation first. Report last marks a complete run.
    json.dumps(report, allow_nan=False)
    output.mkdir()
    np.savez(output / "complex-force-pair-map.npz", frequency=frequency, **maps)
    report["output_map_sha256"] = spatial.sha256(output / "complex-force-pair-map.npz")
    for filename, rows in (("bands.csv", broad), ("sixth-octaves.csv", narrow)):
        flat = [{k: v for k, v in row.items() if k != "held_out_center"} for row in rows]
        with (output / filename).open("x", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(flat[0]))
            writer.writeheader()
            writer.writerows(flat)
    with (output / "report.json").open("x") as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write("\n")
    print(f"Wrote {len(broad)} band/window and {len(narrow)} sixth-octave rows to {output}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--raw-mat", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            if args.raw_mat or args.output:
                parser.error("--self-test does not take input/output paths")
            self_test()
        else:
            if args.raw_mat is None or args.output is None:
                parser.error("--raw-mat and --output are required")
            run(args.raw_mat, args.output)
        return 0
    except (OSError, ValueError, AssertionError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    raise SystemExit(main())

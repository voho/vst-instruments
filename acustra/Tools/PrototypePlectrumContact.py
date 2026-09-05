#!/usr/bin/env python3
"""Offline two-polarization compliant-pick contact experiment; no runtime changes.

Implements the moving-contact beam body and rounded-tip geometry in Perng,
Smith & Rossing, DAFx 2011, equations 12--17. The string is an ideal flexible
string represented by traveling velocity waves, as in equations 24--29. Both its
displacement AND velocity at the contact boundary are retained. This is not a
release-time filter, an acoustic-guitar calibration, or a thumb model.

Protocol: use the published harpsichord reference, recomputing the rectangular
second moment I = width * thickness**3 / 12. The paper's Table 1 instead lists
0.029 mm**4, inconsistent with its 4 mm by 0.5 mm rectangle (0.041667 mm**4).
Report this choice, all physical inputs, discretization, contact-boundary state,
and string/beam/work energy balance. Repeat rate (and therefore spatial mesh).
Never normalize energy or discard an unsuccessful/nonconverged contact solve.
Failures write a local equation case in JSON, exit with status 2 and emit no
contact state. Delay rounding and resulting effective string geometry are
reported; convergence runs must account for that small geometry difference.

The finite-angle beam formula is the paper's approximation, not an exact
elastica: its work/strain-energy defect is a result to inspect, not something
the string integrator's exact work balance can excuse. No friction, pick mass,
string bending, finger tissue or instrument body is represented. A numerical
success cannot identify a guitar player's grip, trajectory or MIDI mapping.

    python3 Tools/PrototypePlectrumContact.py --output /tmp/contact.json
    python3 Tools/PrototypePlectrumContact.py --audit --output /tmp/audit.json
    python3 Tools/PrototypePlectrumContact.py --force-law perng2012 --beam-width .002 \
        --initial-contact-length .0054 --audit --output /tmp/thesis.json

The last command uses Perng's 2012 Table 3.1 geometry and revised Eq2.86 force
law. Its improved static force approximation must still pass the contact solve
and energy audit. The author used up to 1 MHz for this interaction.

The separately derived --force-law virtual-work corrects only the fixed-length
tip force using F=-U_phi/(normal dot q_phi), including the circular-tip moment
arm. Its theta=pi/2 endpoint is prescribed, NOT validated physical detachment:
the corrected force and residual pick energy need not vanish there. Perng2012
section 3.1.2 infers detachment from underside loading beyond 90 degrees, after
section 2.4.7 neglects the tip moment arm. The semicylinder continues beyond
that point; the corrected force no longer justifies that inference. Moving body contact still has a
work defect because the assumed beam shape is not an exact elastica. The tool
reports both that defect and energy lost when uniform string samples average
adaptive contact forces. Neither may be hidden by output normalization.

Optional --state writes string coordinates/velocities after the contact arc
ends in SI units to an NPZ file, with ring indices for the wave histories. That
state is deliberately not injected into the shipping engine.
NumPy and SciPy are already required by the recording-analysis tools.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass, replace
import hashlib
import json
import math
from pathlib import Path
import sys

import numpy as np
from scipy.optimize import least_squares, root


PAPER = "https://www.dafx.de/paper-archive/2011/Papers/24_e.pdf"
THESIS = "https://purl.stanford.edu/wp454hs7976"


class ContactFailure(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(report["reason"])


@dataclass(frozen=True)
class Inputs:
    # Perng et al. 2011 Table 1; SI, except numerical discretization.
    beam_length: float = 0.006
    beam_width: float = 0.004
    beam_thickness: float = 0.0005
    young_modulus: float = 5e9
    string_length: float = 0.5
    string_tension: float = 135.0
    string_linear_density: float = 0.00084
    pluck_fraction: float = 0.5
    jack_speed: float = 0.02  # Fig. 9, constant vertical jack trajectory.
    # The 2010 predecessor's declared initial contact location, Table 2.
    # https://www.dafx.de/paper-archive/2010/DAFx10/PerngSmithRossing_DAFx10_P93.pdf
    initial_contact_length: float = 0.005
    sample_rate: float = 100000.0
    contact_angle_step: float = 0.025  # Numerical continuation limit, radians.
    max_seconds: float = 0.5


def contact(parameters: np.ndarray, tip: bool, p: Inputs, force_law="perng2011"):
    """Position relative to moving clamp, force on string, beam strain energy.

    The beam is massless, frictionless, and bends in the transverse x-y plane.
    parameters = (beam end angle radians, contact length mm OR tip angle radians).
    U = EI/2 integral(phi'(l)**2 dl) = 2EI*phi_end**2/(3*l), from the
    assumed curvature profile (paper equations 4 and 9). Replacing only the
    force approximation does not replace this shape-derived stored energy.
    """
    phi, coordinate = parameters
    ei = p.young_modulus * p.beam_width * p.beam_thickness**3 / 12.0
    length = p.beam_length if tip else coordinate * 0.001
    theta = coordinate if tip else 0.0
    force = 2 * ei * phi / length**2
    if not (force_law == "virtual-work" and tip):
        force /= math.cos(theta)
    if force_law == "perng2012" and tip:
        # Perng2012 thesis Eq2.86 /3.16 corrects the force approximation;
        # the assumed beam shape stays the same. Eq3.12/3.17 swap sin/cos
        # relative to the drawn surface normal and 2011 paper. We retain
        # force perpendicular to the actual contact surface, as required.
        force *= (1 + math.tan(theta) * phi / 12 + phi**2 / 60)**2
    position = np.array([length * (1.0 - 4.0 * phi**2 / 15.0),
                         -2.0 * length * phi / 3.0 * (1.0 - 4.0 * phi**2 / 35.0)])
    if tip:
        radius = 0.5 * p.beam_thickness
        position += radius * np.array([math.sin(theta + phi) - math.sin(phi),
                                      math.cos(theta + phi) - math.cos(phi)])
        if force_law == "virtual-work":
            # Derived candidate, NOT a formula attributed to Perng. For the
            # paper's prescribed shape q(phi,theta), n dot q_theta = 0.
            # Continuous virtual work dU = -F n dot dq therefore fixes
            # F=-U_phi/(n dot q_phi). The -r sin(theta) term is the tip
            # moment arm omitted by applying contact force at the beam end.
            # Only the fixed-length TIP is conjugate: moving body contact has
            # an additional length derivative, whose mismatch remains audited.
            projection = (-8 * length * phi / 15 * math.sin(phi + theta)
                          + (-2 * length / 3 + 8 * length * phi**2 / 35) * math.cos(phi + theta)
                          - radius * math.sin(theta))
            force = -4 * ei * phi / (3 * length * projection) if projection != 0 else math.nan
    force_xy = force * np.array([math.sin(theta + phi), math.cos(theta + phi)])
    beam_energy = 2 * ei * phi**2 / (3 * length)
    return position, force_xy, beam_energy


def diagnose_tip(residual, p, force_law):
    """Search the paper's stated <=45 degree beam approximation domain.

    Solver starts are numerical probes, not additional physical parameters.
    Report the best residual, without treating unsuccessful minimization as
    release. This is a bounded multistart search, not a proof of nonexistence.
    """
    best = None
    for phi in (0.001, 0.25, 0.75):
        for theta in (0.001, 0.5, 1.0, 1.5):
            solved = least_squares(residual, [phi, theta],
                                   bounds=([0, 0], [math.pi / 4, math.pi / 2 - 1e-9]),
                                   ftol=1e-12, xtol=1e-12, gtol=1e-12, max_nfev=500)
            error = float(np.linalg.norm(solved.fun))
            if best is None or error < best[0]:
                best = error, solved.x
    return {"best_contact_residual_norm_mm": best[0],
            "best_beam_angle_degrees": math.degrees(best[1][0]),
            "best_normal_force_n": float(np.linalg.norm(contact(best[1], True, p, force_law)[1])),
            "best_tip_angle_rad": float(best[1][1]),
            "starts": 12, "physical_angle_bound_degrees": 45.0}


def simulate(p: Inputs, force_law="perng2011", adaptive=True):
    for name, value in asdict(p).items():
        if not math.isfinite(value) or value <= 0:
            raise ValueError(f"{name} must be finite and positive")
    if not 0 < p.initial_contact_length <= p.beam_length or not 0 < p.pluck_fraction < 1:
        raise ValueError("contact must lie on the beam and pluck inside the string")
    dt = 1.0 / p.sample_rate
    wave_speed = math.sqrt(p.string_tension / p.string_linear_density)
    impedance = math.sqrt(p.string_tension * p.string_linear_density)
    # Each path goes to a rigid termination and back. One extra history sample
    # retains the incoming wave while reconstructing the final spatial state.
    cells = [round(p.sample_rate * p.string_length * fraction / wave_speed)
             for fraction in (p.pluck_fraction, 1 - p.pluck_fraction)]
    if min(cells) < 2 or sum(cells) < 14:
        raise ValueError("sample rate gives too few spatial cells for the string")
    waves = [np.zeros((2, 2 * count + 1)) for count in cells]
    indices = [0, 0]
    point = np.zeros(2)
    actual_length = sum(cells) * wave_speed * dt
    actual_pluck_fraction = cells[0] / sum(cells)
    f0 = 0.5 * p.sample_rate / sum(cells)
    string_work = grip_work = max_string_work_error = 0.0
    beam_energy = max_energy_defect = max_normal_force = max_deflection_angle = 0.0
    maximum_residual = 0.0
    estimate = np.array([0.0, p.initial_contact_length * 1000])
    tip = False
    tip_start = None
    body_work_defect = None
    release_reason = None
    energy = 0.0
    variance_loss = 0.0
    substeps = backtracks = 0
    smallest_step = dt
    release_time = release_uncertainty = None
    release_point = release_velocity = None

    def fail(reason, values, residual_size):
        wave_energy = impedance * dt * sum(float(np.sum(wave**2) - np.sum(wave[:, index]**2))
                                           for wave, index in zip(waves, indices))
        report = {"status": "failed", "reason": reason, "primary_source": THESIS if force_law == "perng2012" else PAPER,
                  "inputs_si": asdict(p), "failure_time_s": time,
                  "force_law": force_law,
                  "adaptive_contact": adaptive,
                  "actual_string_length_m": actual_length,
                  "actual_pluck_fraction": actual_pluck_fraction,
                  "fundamental_hz": f0, "spatial_cells": cells,
                  "tip_contact": tip, "contact_parameters_before_step": previous_estimate.tolist(),
                  "time_since_tip_contact_s": None if tip_start is None else time - tip_start,
                  "tip_contact_before_step": previous_tip,
                  "attempted_contact_parameters": values.tolist(),
                  "attempted_contact_residual_mm": residual_size,
                  "last_string_energy_j": energy, "last_beam_energy_j": beam_energy,
                  "integrated_string_work_j": string_work,
                  "pending_uniform_step_string_work_j": step_string_work,
                  "independent_wave_buffer_energy_j": wave_energy,
                  "grip_work_j": grip_work,
                  "maximum_string_work_error_j": max_string_work_error,
                  "maximum_beam_plus_string_work_defect_j": max_energy_defect,
                  "substep_variance_loss_j": variance_loss,
                  "maximum_normal_force_n": max_normal_force,
                  "contact_substeps": substeps, "backtracks": backtracks,
                  "smallest_accepted_contact_step_s": smallest_step,
                  "last_point_displacement_m": point.tolist(),
                  "local_equation_case": {"free_point_m": free_point.tolist(),
                                          "clamp_m": clamp.tolist(),
                                          "force_compliance_m_per_n": h / (2 * impedance)},
                  "released_state_valid": False}
        if tip:
            report["bounded_root_diagnostic"] = diagnose_tip(residual, p, force_law)
        raise ContactFailure(report)

    for step in range(1, math.ceil(p.max_seconds * p.sample_rate) + 1):
        incoming = [-wave[:, (index - 2 * count) % wave.shape[1]]
                    for wave, index, count in zip(waves, indices, cells)]
        incoming_sum = incoming[0] + incoming[1]
        elapsed, h = 0.0, dt
        force_integral = np.zeros(2)
        step_string_work = 0.0
        while elapsed < dt * (1 - 1e-12):
            h = min(h, dt - elapsed)
            time = (step - 1) * dt + elapsed + h
            previous_estimate, previous_tip = estimate.copy(), tip
            free_point = point + h * incoming_sum
            clamp = np.array([-p.initial_contact_length, p.jack_speed * time])
            trial_tip = tip

            def residual(values):
                position, force_xy, _ = contact(values, trial_tip, p, force_law)
                return 1000 * (position + clamp - free_point - h * force_xy / (2 * impedance))

            solved = root(residual, estimate, method="hybr", options={"xtol": 1e-10})
            residual_size = float(np.max(np.abs(residual(solved.x))))
            if not trial_tip and solved.x[1] * 0.001 > p.beam_length:
                trial_tip = True
                solved = root(residual, [solved.x[0], 0.0], method="hybr", options={"xtol": 1e-10})
                residual_size = float(np.max(np.abs(residual(solved.x))))
            converged = np.isfinite(solved.x).all() and residual_size <= 1e-7
            edge = converged and trial_tip and solved.x[1] >= math.pi / 2
            detached = converged and solved.x[0] < 0
            endpoint = False
            if edge and tip and force_law == "virtual-work" and math.pi / 2 - estimate[1] < p.contact_angle_step:
                # Resolve the declared arc boundary in time. A finite force
                # here is NOT proof of physical detachment: the original 90°
                # cutoff assumed F*cos(theta) was the whole bending load.
                def endpoint_residual(values):
                    phi, fraction = values
                    duration = h * fraction
                    position, force_xy, _ = contact(np.array([phi, math.pi / 2]), True, p, force_law)
                    event_clamp = np.array([-p.initial_contact_length,
                                            p.jack_speed * ((step - 1) * dt + elapsed + duration)])
                    return 1000 * (position + event_clamp - point
                                   - duration * (incoming_sum + force_xy / (2 * impedance)))

                fraction = (math.pi / 2 - estimate[1]) / (solved.x[1] - estimate[1])
                boundary = root(endpoint_residual, [estimate[0], fraction], method="hybr", options={"xtol": 1e-10})
                boundary_error = float(np.max(np.abs(endpoint_residual(boundary.x))))
                if (np.isfinite(boundary.x).all() and boundary_error <= 1e-7
                        and 0 <= boundary.x[1] <= 1 and 0 <= boundary.x[0] <= math.pi / 4
                        and abs(boundary.x[0] - estimate[0]) <= p.contact_angle_step):
                    h *= boundary.x[1]
                    solved.x = np.array([boundary.x[0], math.pi / 2])
                    residual_size, endpoint, edge = boundary_error, True, False
            # Approach a physical event from the valid side; a failed solve
            # alone never releases the string. Angular tolerance is numerical.
            if detached and estimate[0] < 1e-8:
                release_reason = "tip edge" if edge else "normal force vanished"
                release_time = (step - 1) * dt + elapsed
                release_uncertainty = h
                release_point, release_velocity = point.copy(), point_velocity.copy()
                point += (dt - elapsed) * incoming_sum
                break
            physical = (converged and 0 <= solved.x[0] <= math.pi / 4
                        and solved.x[1] >= 0 and not edge)
            if physical:
                _, trial_force, _ = contact(solved.x, trial_tip, p, force_law)
                normal = np.array([math.sin(solved.x[0] + (solved.x[1] if trial_tip else 0)),
                                   math.cos(solved.x[0] + (solved.x[1] if trial_tip else 0))])
                physical = np.isfinite(trial_force).all() and trial_force @ normal >= 0
            angular_change = abs(solved.x[0] - estimate[0])
            if tip and trial_tip:
                angular_change = max(angular_change, abs(solved.x[1] - estimate[1]))
            if not physical or (adaptive and angular_change > p.contact_angle_step):
                if not adaptive or h < dt * 2**-24:
                    fail("contact continuation failed before a resolved release event", solved.x, residual_size)
                h *= 0.5
                backtracks += 1
                continue
            if trial_tip and not tip:
                tip_start = time
                body_work_defect = string_work + beam_energy - grip_work
            tip, estimate = trial_tip, solved.x
            _, force_xy, beam_energy = contact(estimate, tip, p, force_law)
            point_velocity = incoming_sum + force_xy / (2 * impedance)
            point += h * point_velocity
            local_work = float(force_xy @ point_velocity) * h
            step_string_work += local_work
            string_work += local_work
            grip_work += float(force_xy[1] * p.jack_speed * h)
            force_integral += h * force_xy
            max_energy_defect = max(max_energy_defect, abs(string_work + beam_energy - grip_work))
            maximum_residual = max(maximum_residual, residual_size)
            max_normal_force = max(max_normal_force, float(np.linalg.norm(force_xy)))
            max_deflection_angle = max(max_deflection_angle, float(estimate[0]))
            smallest_step = min(smallest_step, h)
            substeps += 1
            elapsed += h
            if endpoint:
                release_reason = "prescribed contact-arc endpoint; physical detachment unvalidated"
                release_time = (step - 1) * dt + elapsed
                release_uncertainty = max(h, np.finfo(float).eps * release_time)
                release_point, release_velocity = point.copy(), point_velocity.copy()
                point += (dt - elapsed) * incoming_sum
                break
            h = min(2 * h, dt - elapsed)
        # The uniform delay line carries the mean outgoing wave. Jensen's
        # lost sub-sample variance is measured explicitly, never normalized.
        mean_force = force_integral / dt
        outgoing = [incoming[1] + mean_force / (2 * impedance),
                    incoming[0] + mean_force / (2 * impedance)]
        buffer_work = 0.0
        for channel in range(2):
            buffer_work += impedance * dt * float(np.sum(outgoing[channel]**2 - incoming[channel]**2))
            waves[channel][:, indices[channel]] = outgoing[channel]
            indices[channel] = (indices[channel] + 1) % waves[channel].shape[1]
        energy += buffer_work
        lost_variance = step_string_work - buffer_work
        if lost_variance < -1e-14:
            fail("substep averaging generated energy", estimate, residual_size)
        variance_loss += lost_variance
        max_string_work_error = max(max_string_work_error, abs(energy + variance_loss - string_work))
        max_energy_defect = max(max_energy_defect, abs(energy + variance_loss + beam_energy - grip_work))
        if release_reason:
            break

    if release_reason is None:
        fail("no release before max_seconds; held contact is not a pluck", estimate, 0.0)
    if energy <= 0 or grip_work <= 0:
        raise RuntimeError("release has no positive string energy/player work")
    # Recover displacement from u_z=(v_left-v_right)/c, starting at fixed nut.
    gradients, velocities = [], []
    for side, (wave, index, count) in enumerate(zip(waves, indices, cells)):
        distance = np.arange(count + 1)
        outward = wave[:, (index - 1 - distance) % wave.shape[1]]
        inward = -wave[:, (index - 1 - (2 * count - distance)) % wave.shape[1]]
        gradient = (outward - inward) / wave_speed
        speed = outward + inward
        if side == 0:
            gradient, speed = gradient[:, ::-1], speed[:, ::-1]
        else:
            gradient = -gradient
        gradients.append(gradient)
        velocities.append(speed)
    gradient = np.concatenate(gradients, axis=1)
    velocity = np.concatenate(velocities, axis=1)
    # Keep both sides of the contact point: displacement and velocity are
    # continuous there but slope has the physical F/T jump during contact.
    x = np.concatenate([np.arange(cells[0] + 1), cells[0] + np.arange(cells[1] + 1)]) * wave_speed * dt
    displacement = np.zeros_like(velocity)
    displacement[:, 1:] = np.cumsum(0.5 * (gradient[:, :-1] + gradient[:, 1:]) * np.diff(x), axis=1)
    # Bridge-force modal magnitude for both polarizations. No body/filter/noise.
    # Nulls remain explicit; sums of power avoid mean-of-log comb-null artifacts.
    omega = 2 * math.pi * f0 * np.arange(1, 13)
    sine = np.sin(math.pi * np.arange(1, 13)[:, None] * x / actual_length)
    q = 2 / actual_length * np.trapz(displacement[:, None, :] * sine, x, axis=-1)
    modal_velocity = 2 / actual_length * np.trapz(velocity[:, None, :] * sine, x, axis=-1)
    modal_amplitude = np.sqrt(np.sum(q**2 + (modal_velocity / omega)**2, axis=0))
    bridge_amplitude = p.string_tension * omega / (2 * f0 * actual_length) * modal_amplitude
    harmonic_power = bridge_amplitude**2
    upper_lower_db = 10 * math.log10(float(harmonic_power[3:12].sum() / harmonic_power[:3].sum()))
    report = {
        "status": "contact_ended_unvalidated",
        "model": "Perng approximate beam/tip plus traveling-velocity ideal string",
        "force_law": force_law,
        "adaptive_contact": adaptive,
        "primary_source": THESIS if force_law == "perng2012" else PAPER,
        "inputs_si": asdict(p),
        "recomputed_second_moment_m4": p.beam_width * p.beam_thickness**3 / 12.0,
        "fundamental_hz": f0,
        "spatial_cells": cells,
        "actual_string_length_m": actual_length,
        "actual_pluck_fraction": actual_pluck_fraction,
        "contact_end_time_s": release_time,
        "contact_end_final_step_s": release_uncertainty,
        "state_snapshot_time_s": step * dt,
        "state_snapshot_assumption": "force set to zero after prescribed endpoint until next uniform sample; continued contact unmodeled",
        "tip_contact_time_s": None if tip_start is None else max(0.0, release_time - tip_start),
        "contact_end_reason": release_reason,
        "physical_detachment_validated": False,
        "normal_force_at_contact_end_n": float(np.linalg.norm(force_xy)),
        "contact_end_beam_angle_rad": float(estimate[0]),
        "contact_end_tip_angle_rad": float(estimate[1]) if tip else None,
        "contact_end_point_displacement_m": release_point.tolist(),
        "contact_end_point_velocity_m_per_s": release_velocity.tolist(),
        "field_point_displacement_error_m": (displacement[:, cells[0]] - point).tolist(),
        "field_bridge_displacement_error_m": displacement[:, -1].tolist(),
        "maximum_force_n": max_normal_force,
        "maximum_beam_deflection_angle_degrees": math.degrees(max_deflection_angle),
        "string_energy_j": energy,
        "beam_energy_at_contact_end_j": beam_energy,
        "grip_work_j": grip_work,
        "substep_variance_loss_j": variance_loss,
        "substep_variance_fraction_of_grip_work": variance_loss / grip_work,
        "contact_substeps": substeps, "backtracks": backtracks,
        "smallest_contact_step_s": smallest_step,
        "string_integrator_work_error_relative": max_string_work_error / grip_work,
        "maximum_beam_plus_string_work_defect_relative": max_energy_defect / grip_work,
        "final_beam_plus_string_work_defect_relative": (energy + variance_loss + beam_energy - grip_work) / grip_work,
        "body_work_defect_before_tip_j": body_work_defect,
        "tip_work_defect_increment_j": None if body_work_defect is None else string_work + beam_energy - grip_work - body_work_defect,
        "maximum_contact_residual_mm": maximum_residual,
        "bridge_h4_h12_over_h1_h3_db": upper_lower_db,
        "bridge_harmonic_magnitudes_n": bridge_amplitude[:12].tolist(),
        "limitations": ["harpsichord geometry, not measured guitar or thumb",
                        "approximate finite-angle massless frictionless beam",
                        "prescribed arc endpoint does not validate physical detachment",
                        "tip virtual work does not conserve the approximate moving-contact body",
                        "uniform delay samples lose the explicitly reported substep variance",
                        "no guitar grip/trajectory/dynamic mapping inferred",
                        "spatial and temporal convergence must be checked",
                        "no instrument-body or corpus-fidelity claim"],
    }
    return report, {"position_along_string_m": x, "displacement_m": displacement,
                    "velocity_m_per_s": velocity, "slope": gradient,
                    "traveling_velocity_left": waves[0], "traveling_velocity_right": waves[1],
                    "wave_write_indices": np.array(indices)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--state", type=Path)
    parser.add_argument("--audit", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--fixed-contact-step", action="store_true",
                        help="reproduce the non-adaptive contact solve for numerical diagnostics")
    parser.add_argument("--force-law", choices=("perng2011", "perng2012", "virtual-work"), default="perng2011",
                        help="paper reference, thesis correction, or derived tip-only work-conjugate force")
    defaults = Inputs()
    for name, value in asdict(defaults).items():
        parser.add_argument("--" + name.replace("_", "-"), type=type(value), default=value)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    physical = Inputs(**{name: getattr(args, name) for name in asdict(defaults)})
    def run_case(inputs):
        try:
            return simulate(inputs, args.force_law, not args.fixed_contact_step)
        except ContactFailure as error:
            return error.report, None

    report, state = run_case(physical)
    failed = state is None
    if args.audit:
        report = {"reference": report, "rate": []}
        for factor in (2, 4):
            result, result_state = run_case(replace(physical, sample_rate=physical.sample_rate * factor))
            failed |= result_state is None
            report["rate"].append(result)
    report["tool_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    if args.state and not failed:
        np.savez(args.state, **state)
    if failed:
        print("Contact experiment failed; JSON contains the case. No released state written.", file=sys.stderr)
        return 2
    return 0


def self_test():
    """Analytic beam limits and independent wave-energy/report checks."""
    p = Inputs()
    ei = p.young_modulus * p.beam_width * p.beam_thickness**3 / 12
    for law in ("perng2011", "perng2012", "virtual-work"):
        body = contact(np.array([0.2, p.beam_length * 1000]), False, p, law)
        tip = contact(np.array([0.2, 0.0]), True, p, law)
        assert np.array_equal(body[0], tip[0]) and body[2] == tip[2]
        if law == "perng2011":
            assert np.array_equal(body[1], tip[1])
        for phi in (0.01, 0.2, 0.6):
            for theta in (0.0, 0.2, 1.0):
                _, force, energy = contact(np.array([phi, theta]), True, p, law)
                tangent = np.array([math.cos(phi + theta), -math.sin(phi + theta)])
                assert abs(force @ tangent) < 1e-12 and energy > 0
                if law == "virtual-work":
                    # Independent finite differences check both geometry
                    # derivatives, including the finite-radius moment arm.
                    epsilon = 1e-6
                    plus = contact(np.array([phi + epsilon, theta]), True, p, law)
                    minus = contact(np.array([phi - epsilon, theta]), True, p, law)
                    work_derivative = force @ ((plus[0] - minus[0]) / (2 * epsilon))
                    energy_derivative = (plus[2] - minus[2]) / (2 * epsilon)
                    assert abs(work_derivative + energy_derivative) < 1e-10
                    plus = contact(np.array([phi, theta + epsilon]), True, p, law)
                    minus = contact(np.array([phi, theta - epsilon]), True, p, law)
                    assert abs(force @ ((plus[0] - minus[0]) / (2 * epsilon))) < 1e-10
        position, force, energy = contact(np.array([1e-6, p.beam_length * 1000]), False, p, law)
        compliance = p.beam_length**3 / (3 * ei)
        assert abs((-position[1] / force[1]) / compliance - 1) < 1e-10
        assert abs(energy / (-0.5 * force[1] * position[1]) - 1) < 1e-10
    try:
        simulate(replace(p, max_seconds=0.001))
        raise AssertionError("a held contact was falsely reported as a released pluck")
    except ContactFailure as error:
        report = error.report
        assert report["released_state_valid"] is False
        assert "no release" in report["reason"]
        assert abs(report["independent_wave_buffer_energy_j"] - report["last_string_energy_j"]) < 1e-16
        json.dumps(report, allow_nan=False)
    print("Contact algebra, wave energy and failed-state reporting checks passed; release fidelity is unvalidated.")


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Finite Galerkin preload reference, never a native audio/MIDI renderer.

Woodhouse 2004 Eq11 constraint plus mass-normalized sine modes. One low-E
speaking string, current Original bridge modes, all six fixed tail anchors;
other speaking strings omitted. Native 48k prewarped poles define ONE continuous
system for every integrator rate. All figures are SI mechanical quantities.
Damping is diagonal in this chosen basis, not exact distributed viscous drag.
Native scalar+shelf attenuation sets approximate modal decay per physical round
trip; no fitting, clipping, EQ, contact trajectory or radiation bank is used.
The observed force drives the free mechanical body in its normal/normalized-
moment coordinates. It includes speaking-string and fixed-anchor reactions;
it does not specify an acoustic pressure state.

Requires NumPy and SciPy. From the repository root:
  mkdir -p build
  c++ -std=c++20 -O2 -I Source Tools/ModalPreloadProbe.cpp -o build/ModalPreloadProbe
  build/ModalPreloadProbe > build/modal-preload-coefficients.jsonl
  python3 Tools/PrototypeModalPreload.py \
    --coefficients build/modal-preload-coefficients.jsonl \
    --output build/modal-preload-analysis

The output directory must be new. Preserve the source hash record described in
ModalPreloadProbe.cpp alongside the exported coefficients. Workspace source
hashes recorded here are observation context only: they do NOT establish that
an externally supplied coefficient file was generated from those sources.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import time

import numpy as np
import scipy
from scipy import linalg


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def symmetric(values):
    return np.array([[values[0], values[1]], [values[1], values[2]]])


def read_coefficients(data):
    rows = [json.loads(line) for line in data.splitlines() if line.strip()]
    fields = {
        "configuration": ("K",),
        "bridge_mode": ("mode", "omega_prewarped", "q", "R"),
        "string": (
            "string", "gain", "Z", "T", "L", "arm", "B",
            "broad_coefficient", "broad_mix", "high_coefficient", "high_mix",
        ),
    }
    for row in rows:
        require(isinstance(row, dict), "Each coefficient row must be an object")
        kind = row.get("kind")
        require(isinstance(kind, str) and kind in fields, "Unsupported coefficient row kind")
        require(row.get("rate") == 48000, "The reference requires a 48k export")
        require(row.get("variant") in (0, 1), "Expected nylon/steel Original variants")
        for name in fields[kind]:
            value = row.get(name)
            values = value if name in ("K", "R") else [value]
            require(isinstance(values, list), f"{name} must be a numeric array")
            if name in ("K", "R"):
                require(len(values) == 3, f"{name} needs three matrix entries")
            require(all(isinstance(v, (int, float)) and not isinstance(v, bool)
                        and math.isfinite(v) for v in values),
                    f"{name} must contain finite numbers")
    for variant in (0, 1):
        subset = [row for row in rows if row["variant"] == variant]
        configs = [row for row in subset if row["kind"] == "configuration"]
        strings = [row for row in subset if row["kind"] == "string"]
        modes = [row for row in subset if row["kind"] == "bridge_mode"]
        require(len(configs) == 1 and len(strings) == 1 and modes,
                "Each variant needs one configuration, one string and body modes")
        require(strings[0]["string"] == 0, "Only the low-E string is supported")
        require(len({row["mode"] for row in modes}) == len(modes),
                "Duplicate bridge mode index")
        require(all(row["q"] > 0 and row["omega_prewarped"] > 0 for row in modes),
                "Bridge frequency and Q must be positive")
        require(np.min(linalg.eigvalsh(symmetric(configs[0]["K"]))) >= 0,
                "Tail-anchor stiffness must be positive semidefinite")
        string = strings[0]
        require(all(string[name] > 0 for name in ("T", "Z", "L", "B")),
                "String tension, impedance, length and stiffness must be positive")
        require(0 < string["gain"] < 1, "Scalar loop gain must lie between zero and one")
        for name in ("broad_coefficient", "high_coefficient"):
            require(0 <= string[name] < 1, "Loss-filter poles must be stable")
        for name in ("broad_mix", "high_mix"):
            require(0 <= string[name] <= 1, "Loss-filter mix must be bounded")
    return rows


def build(rows, variant, count):
    subset = [row for row in rows if row["variant"] == variant]
    config = next(row for row in subset if row["kind"] == "configuration")
    string = next(row for row in subset if row["kind"] == "string")
    columns, omega, body_damping, projection_errors = [], [], [], []
    original_compliance = np.zeros((2, 2))
    for mode in (row for row in subset if row["kind"] == "bridge_mode"):
        residue = symmetric(mode["R"])
        eigenvalues, vectors = linalg.eigh(residue)
        require(eigenvalues[-1] > 0, "A bridge mode must have a positive residue")
        # Only negative float-rounding remnants are removed; no positive rank cutoff.
        require(eigenvalues[0] >= -np.finfo(np.float32).eps * eigenvalues[-1],
                "Bridge residue is not positive semidefinite within float rounding")
        clipped = np.maximum(eigenvalues, 0)
        projection_errors.append({
            "mode": mode["mode"],
            "minimum_eigenvalue": float(eigenvalues[0]),
            "relative_frobenius_change": float(
                np.linalg.norm((vectors * clipped) @ vectors.T - residue)
                / np.linalg.norm(residue)),
        })
        original_compliance += residue / mode["omega_prewarped"] ** 2
        for value, vector in zip(clipped, vectors.T):
            if value == 0:
                continue
            columns.append(math.sqrt(value) * vector)
            omega.append(mode["omega_prewarped"])
            body_damping.append(mode["omega_prewarped"] / mode["q"])

    V = np.array(columns).T
    omega = np.array(omega)
    body_count = len(omega)
    C = (V / omega ** 2) @ V.T
    T, Z, L = string["T"], string["Z"], string["L"]
    mu, c = Z * Z / T, T / Z
    stiffness = string["B"]
    EI = stiffness * T * L * L / math.pi ** 2
    n = np.arange(1, count + 1)
    wave_numbers = n * math.pi / L
    string_omega = (c * wave_numbers) * np.sqrt(1 + stiffness * n * n)
    require(np.max(string_omega) < math.pi * 48000,
            "Do not wrap native loss above reference Nyquist")
    z = np.exp(-1j * string_omega / 48000)

    def shelf(coefficient, mix):
        return 1 - mix + mix * (1 - coefficient) / (1 - coefficient * z)

    gains = string["gain"] * np.abs(
        shelf(string["broad_coefficient"], string["broad_mix"])
        * shelf(string["high_coefficient"], string["high_mix"]))
    require(np.all((gains > 0) & (gains < 1)), "Modal losses must be passive")
    alphas = -np.log(gains) / (2 * L / c)
    arm = np.array([1, string["arm"]])
    B = arm @ V
    Ktail = symmetric(config["K"])
    cross = np.sqrt(2 * mu * L) * (-1.) ** (n + 1) / (n * math.pi)

    M = np.eye(count + body_count)
    M[:count, count:] = np.outer(cross, B)
    M[count:, :count] = M[:count, count:].T
    M[count:, count:] += mu * L / 3 * np.outer(B, B)
    K = np.zeros_like(M)
    K[:count, :count] = np.diag(string_omega * string_omega)
    K[count:, count:] = (np.diag(omega * omega) + V.T @ Ktail @ V
                        + T / L * np.outer(B, B))
    D = np.diag(np.r_[2 * alphas, body_damping])
    LM = linalg.cholesky(M, lower=True)
    LK = linalg.cholesky(K, lower=True)

    xp, force = .8 * L, 1.
    xi = xp / L
    b = np.r_[np.sin(n * math.pi * xi) / math.sqrt(mu * L / 2), xi * B]
    q0 = linalg.cho_solve((LK, True), force * b)
    energy0 = .5 * q0 @ K @ q0
    require(energy0 > 0, "Preload energy must be positive")
    equilibrium = np.linalg.norm(K @ q0 - force * b) / np.linalg.norm(force * b)
    require(equilibrium < 1e-12, "Static equilibrium residual exceeds tolerance")
    Ceff = np.linalg.solve(np.eye(2) + C @ (Ktail + T / L * np.outer(arm, arm)), C)
    Cmodal = np.sum(np.sin(n * math.pi * xi) ** 2
                    / (mu * L / 2 * string_omega * string_omega))
    compliance = Cmodal + xi * xi * arm @ Ceff @ arm
    require(abs(b @ q0 - compliance) / compliance < 1e-12,
            "Constraint and independent static-compliance calculations disagree")
    eta = math.sqrt(T / EI)
    helmholtz = ((.5 / eta) * (-math.expm1(-2 * eta * xp))
                 * (-math.expm1(-2 * eta * (L - xp))) / (-math.expm1(-2 * eta * L)))
    exact_string = (L * xi * (1 - xi) - helmholtz) / T
    exact_compliance = exact_string + xi * xi * arm @ Ceff @ arm

    # Energy coordinates u=(LK^T q, LM^T v) avoid poorly scaled displacement states.
    inverse_velocity_factor = linalg.solve_triangular(LM.T, np.eye(len(M)), lower=False)
    H = LK.T @ inverse_velocity_factor
    dissipative = inverse_velocity_factor.T @ D @ inverse_velocity_factor
    A = np.block([[np.zeros_like(M), H], [-H.T, -dissipative]])
    u0 = np.r_[LK.T @ q0, np.zeros(len(M))]
    # Point displacement, physical bridge heave and normalized rock displacement.
    observe = np.zeros((3, len(M)))
    observe[0] = b
    observe[1:, count:] = V
    inverse_displacement_factor = linalg.solve_triangular(
        LK.T, np.eye(len(M)), lower=False)
    observe = observe @ inverse_displacement_factor
    O = np.hstack([observe, np.zeros_like(observe)])

    # Observe physical energy and force from the existing modal state. These
    # closures retain the matrices already built, without another model solve.
    string_damping = 2 * alphas
    body_damping = np.array(body_damping)
    m0, k0 = mu * L / 3, T / L
    dimension = len(M)

    def energy_parts(u):
        q = inverse_displacement_factor @ u[:dimension]
        v = inverse_velocity_factor @ u[dimension:]
        qs, qb, vs, vb = q[:count], q[count:], v[:count], v[count:]
        y, velocity = B @ qb, B @ vb
        string_energy = (.5 * vs @ vs + (cross @ vs) * velocity
                         + .5 * m0 * velocity * velocity
                         + .5 * (string_omega ** 2 * qs) @ qs + .5 * k0 * y * y)
        body_energy = .5 * vb @ vb + .5 * (omega ** 2 * qb) @ qb
        x = V @ qb
        return np.array([string_energy, body_energy, .5 * x @ Ktail @ x])

    def force_observation(u, derivative, point_force=0.):
        q = inverse_displacement_factor @ u[:dimension]
        v = inverse_velocity_factor @ u[dimension:]
        acceleration = inverse_velocity_factor @ derivative[dimension:]
        qb, vs, vb = q[count:], v[:count], v[count:]
        speaking = arm * (xi * point_force - cross @ acceleration[:count]
                          - m0 * (B @ acceleration[count:]) - k0 * (B @ qb))
        anchors = -Ktail @ (V @ qb)
        net = speaking + anchors
        lhs = acceleration[count:] + body_damping * vb + omega ** 2 * qb
        rhs = V.T @ net
        residual = np.linalg.norm(lhs - rhs) / max(1, np.linalg.norm(lhs), np.linalg.norm(rhs))
        velocity = V @ vb
        power = np.array([
            point_force * (b @ v) - velocity @ speaking - (string_damping * vs) @ vs,
            velocity @ net - (body_damping * vb) @ vb,
            -velocity @ anchors,
        ])
        return np.r_[net, speaking, anchors], power, float(residual)

    held, _, held_residual = force_observation(u0, np.zeros_like(u0), force)
    released, _, released_residual = force_observation(u0, A @ u0)
    beta, remainder = m0 - cross @ cross, xi - cross @ b[:count]
    predicted_jump = -arm * remainder * force / (1 + beta * (B @ B))
    require(beta >= 0, "Finite constraint-mode residual mass must be nonnegative")
    require(max(held_residual, released_residual) < 1e-11,
            "Initial free-body force balance failed")
    require(np.max(abs(released[:2] - held[:2] - predicted_jump)) < 1e-11,
            "Finite-basis release-force jump disagrees with its prediction")
    require(abs(sum(energy_parts(u0)) - energy0) / energy0 < 1e-12,
            "Independent energy components disagree with total preload energy")
    metadata = {
        "material": ["nylon", "steel"][variant],
        "string_modes": count,
        "body_coordinates": body_count,
        "active_bridge_modes": len(projection_errors),
        "mass_min_eigenvalue": float(linalg.eigvalsh(M, subset_by_index=[0, 0])[0]),
        "stiffness_min_eigenvalue": float(linalg.eigvalsh(K, subset_by_index=[0, 0])[0]),
        "static_equilibrium_relative_residual": float(equilibrium),
        "initial_energy_J": float(energy0),
        "initial_point_displacement_m": float(b @ q0),
        "initial_bridge_displacement_m": (V @ q0[count:]).tolist(),
        "static_compliance_relative_error_to_infinite_pinned_stiff_string": float(
            (compliance - exact_compliance) / exact_compliance),
        "maximum_string_mode_hz": float(string_omega[-1] / (2 * math.pi)),
        "string_alpha_range_per_s": [float(min(alphas)), float(max(alphas))],
        "maximum_residue_relative_PSD_projection": max(
            error["relative_frobenius_change"] for error in projection_errors),
        "body_compliance_relative_PSD_projection": float(
            np.linalg.norm(C - original_compliance) / np.linalg.norm(original_compliance)),
        "residue_projection_details": projection_errors,
        "T_N": T, "mu_kg_per_m": mu, "L_m": L, "EI_N_m2": EI,
        "native_dimensionless_B": stiffness,
        "point_force_N": force,
        "pluck_fraction_from_bridge": .2,
        "body_constraint_force_vector": (xi * arm).tolist(),
        "all_tail_anchor_K": Ktail.tolist(),
        "mechanical_force": {
            "columns": ["body_normal_N", "body_normalized_moment_N",
                        "speaking_normal_N", "speaking_normalized_moment_N",
                        "anchors_normal_N", "anchors_normalized_moment_N"],
            "held": held.tolist(),
            "immediately_after_release": released.tolist(),
            "release_jump_N": (released[:2] - held[:2]).tolist(),
            "predicted_release_jump_N": predicted_jump.tolist(),
            "finite_basis_residual_mass_kg": float(beta),
            "point_constraint_remainder": float(remainder),
            "maximum_initial_body_ODE_relative_residual": max(held_residual, released_residual),
        },
    }
    return A, u0, O, dissipative, metadata, energy_parts, force_observation


def run(rows, variant, count):
    start = time.perf_counter()
    A, u0, O, D, metadata, energy_parts, force_observation = build(rows, variant, count)
    dimension = len(u0) // 2
    energy0 = .5 * u0 @ u0
    times = np.array([.001, .005, .01, .02])
    exact_states = [linalg.expm(A * t) @ u0 for t in times]
    exact = [O @ state for state in exact_states]
    metadata["expm_observations"] = {
        "times_s": times.tolist(),
        "columns": ["point_m", "bridge_heave_m", "bridge_normalized_rock_m"],
        "values": np.array(exact).tolist(),
    }
    exact_forces = [force_observation(state, A @ state) for state in exact_states]
    maximum_force_residual = max(result[2] for result in exact_forces)
    require(maximum_force_residual < 1e-11, "Continuous free-body ODE force balance failed")
    metadata["mechanical_force"]["expm_times_s"] = times.tolist()
    metadata["mechanical_force"]["expm_values"] = [result[0].tolist() for result in exact_forces]
    metadata["mechanical_force"]["maximum_continuous_body_ODE_relative_residual"] = maximum_force_residual
    metadata["integrators"] = []
    for rate in ([48000] if count < 128 else [44100, 48000, 96000]):
        dt = 1 / rate
        G = linalg.solve(np.eye(len(A)) - .5 * dt * A,
                         np.eye(len(A)) + .5 * dt * A)
        u = u0.copy()
        max_residual, positive_step, loss = 0., 0., 0.
        observations = []
        force_samples = []
        initial_parts = energy_parts(u0)
        previous_parts = initial_parts
        component_work = np.zeros(3)
        maximum_work_residual = np.zeros(3)
        maximum_body_residual = 0.
        wanted = {int(round(t * rate)) for t in times}
        for step in range(1, int(round(times[-1] * rate)) + 1):
            next_u = G @ u
            midpoint = .5 * (u[dimension:] + next_u[dimension:])
            delta_energy = .5 * (next_u @ next_u - u @ u)
            dissipation = dt * (midpoint @ D @ midpoint)
            max_residual = max(max_residual, abs(delta_energy + dissipation) / energy0)
            positive_step = max(positive_step, delta_energy / energy0)
            loss += dissipation
            # Actual trapezoidal acceleration, not a derivative inferred from
            # the force being checked. Interface work cancels across components.
            _, power, body_residual = force_observation(.5 * (u + next_u), (next_u - u) / dt)
            next_parts = energy_parts(next_u)
            maximum_work_residual = np.maximum(
                maximum_work_residual, abs(next_parts - previous_parts - dt * power) / energy0)
            component_work += dt * power
            maximum_body_residual = max(maximum_body_residual, body_residual)
            previous_parts = next_parts
            u = next_u
            if step in wanted:
                observations.append(O @ u)
                force_samples.append(force_observation(u, A @ u)[0].tolist())
        observations = np.array(observations)
        reference = np.array(exact)
        # Compare actual sampled times, without dividing by observation zeros.
        at_times = np.array(sorted(wanted)) / rate
        if not np.array_equal(at_times, times):
            reference = np.array([O @ (linalg.expm(A * t) @ u0) for t in at_times])
        energy = .5 * u @ u
        require(max_residual < 1e-10 and positive_step < 1e-10,
                "Trapezoidal per-step energy identity failed")
        require(abs((energy + loss - energy0) / energy0) < 1e-9,
                "Trapezoidal cumulative energy identity failed")
        component_balance = (previous_parts - initial_parts - component_work) / energy0
        require(maximum_body_residual < 1e-9, "Trapezoidal free-body ODE force balance failed")
        require(max(maximum_work_residual) < 1e-10 and max(abs(component_balance)) < 1e-9,
                "Independent string/body/anchor work closure failed")
        metadata["integrators"].append({
            "rate": rate, "steps": step,
            "maximum_step_energy_identity_residual_over_initial_energy": max_residual,
            "maximum_positive_energy_step_over_initial_energy": positive_step,
            "final_energy_over_initial": float(energy / energy0),
            "cumulative_dissipation_over_initial": float(loss / energy0),
            "cumulative_energy_balance_relative_residual": float(
                (energy + loss - energy0) / energy0),
            "actual_observation_times_s": at_times.tolist(),
            "observations": observations.tolist(),
            "maximum_absolute_point_error_m": float(
                max(abs(observations[:, 0] - reference[:, 0]))),
            "maximum_absolute_bridge_vector_error_m": float(
                max(np.linalg.norm(observations[:, 1:] - reference[:, 1:], axis=1))),
            "mechanical_force_values": force_samples,
            "maximum_body_ODE_relative_residual": maximum_body_residual,
            "energy_component_order": ["speaking_string", "free_body", "fixed_anchors"],
            "maximum_component_step_work_residual_over_initial_energy": maximum_work_residual.tolist(),
            "cumulative_component_work_residual_over_initial_energy": component_balance.tolist(),
        })
    metadata["elapsed_seconds"] = time.perf_counter() - start
    print(f"{metadata['material']}: {count} string modes, checks passed", flush=True)
    return metadata


def make_report(rows, provenance):
    results = [run(rows, variant, count) for variant in (0, 1) for count in (32, 64, 128)]
    convergence = []
    for material in ("nylon", "steel"):
        values = [np.array(case["expm_observations"]["values"])
                  for case in results if case["material"] == material]
        differences = []
        for before, after in zip(values, values[1:]):
            differences.append({
                "maximum_point_displacement_change_m": float(
                    np.max(abs(after[:, 0] - before[:, 0]))),
                "maximum_bridge_vector_change_m": float(
                    np.max(np.linalg.norm(after[:, 1:] - before[:, 1:], axis=1))),
            })
        convergence.append({
            "material": material, "pairs": ["32_to64", "64_to128"],
            "expm_observation_changes": differences,
        })
    return {
        "status": "bounded passive mechanical reference completed; not native audio or an identified physical pluck calibration",
        "protocol": __doc__,
        "provenance": provenance,
        "formulation": {
            "coordinates": "y(x)=(x/L)*B*q_body+sum q_n*sin(n*pi*x/L)/sqrt(mu*L/2), B=[1,arm]*V; R_mode=V_mode*V_mode^T.",
            "mass": "Mss=I, Msb_n=sqrt(2muL)*(-1)^(n+1)/(n*pi)*B, Mbb=I+muL/3*B^T B.",
            "stiffness": "Kss=diag((T*k_n^2+EI*k_n^4)/mu); Kbb=diag(Omega_body^2)+V^T*K_all_tails*V+(T/L)*B^T B; Ksb=0.",
            "damping": "D=diag(2alpha_string,Omega_body/Q_body) in this chosen coordinate basis; positive but not a spatially uniform viscous operator. alpha_string=-log(nativeGain*abs(nativeLossShelves(omega_n)))/(2L/c), an approximate modal decay target.",
            "preload": "At x/L=.8, b=[sin(n*pi*.8)/sqrt(muL/2), .8*B]; q0=K^-1*b*1N, qdot0=0. No assumed MIDI-force relation.",
            "release": "M*qdd+D*qdot+K*q=0 after external holding force removal. Body is preloaded mechanically; no pressure bank or acoustic DC equilibrium is assumed.",
            "energy": "E=.5*(v^T*M*v+q^T*K*q). Trapezoidal update obeys E_next-E=-dt*v_mid^T*D*v_mid, checked each step.",
            "static_reference": "Pinned stiff string C(xp,xp)=[L*xi*(1-xi)-sinh(eta*xp)*sinh(eta*(L-xp))/(eta*sinh(eta*L))]/T, eta=sqrt(T/EI), plus xi^2*arm^T*C_body_with_tails_and_one_string*arm.",
            "body_force": "f=arm*(xi*Fp-c^T*a_ddot-(muL/3)*yB_ddot-(T/L)*yB)-Ktail*x; x=V*qb, yB=arm^T*x, c_n=sqrt(2muL)*(-1)^(n+1)/(n*pi). This drives qb_ddot+Db*qb_dot+Omega_b^2*qb=V^T*f.",
            "release_force_jump": "Delta f=-arm*r*Fp/(1+beta*B*B^T), beta=muL/3-c^T*c, r=xi-c^T*phi. The finite-basis jump vanishes in the complete-basis limit and does not imply a contact impulse or energy jump.",
        },
        "checks": "M/K Cholesky SPD, positive diagonal D, static algebra, positive preload energy, zero initial velocity by construction, dense expm reference at1/5/10/20ms, trapezoidal free energy balance at every sample. Mode counts32/64/128; rate comparison44.1/48/96k uses the SAME exported48k continuous system.",
        "force_checks": "Every free-body modal ODE: l2 residual/max(1,||lhs||,||rhs||)<1e-11 at exact states and<1e-9 at every trapezoidal midpoint. The unit floor is a diagnostic mass-normalized force scale. Separate string/body/anchor energy-work residuals divided by initial total energy are<1e-10 per step and<1e-9 cumulatively. Reported sampled force rows use the continuous derivative at their actual observation times; work checks use midpoint finite differences.",
        "force_scope": "Forces are [normal N, normalized moment N], not displacement or pressure. A held load is balanced by the free body's static restoring force. Elastic/bending endpoint stress alone differs from this Galerkin reaction by c^T*Ds*a_dot-beta*yB_ddot+r*Fp: the damping reaction belongs to the chosen diagonal modal damping model; beta/r reflect basis truncation. No pressure-filter equilibrium or MIDI-force mapping is established.",
        "limitations": [
            "Other speaking strings, second transverse polarization, contact motion, noise, longitudinal waves and acoustic radiation are omitted. This is not the production guitar or a listening candidate.",
            "The measured bridge bank and extrapolated static compliance retain their existing measurement/geometry limitations. Only float-rounding negative residue eigenvalues are projected to zero; numerical changes are recorded.",
            "Changing modal count changes the finite Galerkin state; convergence is reported, not assumed. Time integration preserves energy dissipation but trapezoidal frequency warping need not be negligible for the highest modes.",
            "D diagonal in these normalized fixed-string/free-body coordinates does not prescribe the final coupled modal decay rates. Native loop phase, exact delay and dispersion recurrence are not reproduced.",
            "No measured string/body release recording, force-to-MIDI mapping, absolute acoustic level or perceptual realism claim follows.",
        ],
        "primary_source": {
            "url": "https://euphonics.org/wp-content/uploads/2022/03/Guitar_I.pdf",
            "description": "Woodhouse2004 On the Synthesis of Guitar Plucks, Eq11 constraint and modal energy construction; finite model and code derived independently from the stated energies.",
        },
        "convergence": convergence,
        "cases": results,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coefficients", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        require(not args.output.exists(), "Output directory already exists")
        data = args.coefficients.read_bytes()
        rows = read_coefficients(data)
        script = Path(__file__).resolve()
        source = script.parent.parent / "Source" / "DSP"
        source_names = (
            "AcustraEngine.cpp", "AcustraEngine.h", "FittedPhysicalData.h",
            "MeasuredBodyData.h", "MeasuredBridgeData.h", "MeasuredSteelBridgeData.h",
        )
        provenance = {
            "coefficients_sha256": sha256(data),
            "script_sha256": sha256(script.read_bytes()),
            "workspace_source_hashes_at_analysis": {
                name: sha256((source / name).read_bytes()) for name in source_names
            },
            "workspace_hash_scope": "Observation context only; these hashes do not prove the supplied coefficients were generated from these sources.",
            "numpy_version": np.__version__, "scipy_version": scipy.__version__,
        }
        report = make_report(rows, provenance)
        encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
        args.output.mkdir(parents=True, exist_ok=False)
        (args.output / "report.json").write_text(encoded)
        print(f"Report: {args.output / 'report.json'}")
        print(f"SHA256: {sha256(encoded.encode())}")
    except (OSError, ValueError, np.linalg.LinAlgError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()

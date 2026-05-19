"""
reference_j2.py - Small-strain J2 point-integration reference routines.

Voigt order is [xx, yy, zz, xy, yz, zx]. Off-diagonal strain entries are tensor
strain components, not engineering shear strains.
"""

from __future__ import annotations

import math
from typing import Any


def _as_six(values: list[float], name: str) -> list[float]:
    if len(values) != 6:
        raise ValueError(f"{name} must contain 6 components.")
    return [float(value) for value in values]


def _trace(stress: list[float]) -> float:
    return stress[0] + stress[1] + stress[2]


def _deviator(stress: list[float]) -> list[float]:
    mean = _trace(stress) / 3.0
    return [
        stress[0] - mean,
        stress[1] - mean,
        stress[2] - mean,
        stress[3],
        stress[4],
        stress[5],
    ]


def _double_contract(a: list[float], b: list[float]) -> float:
    return (
        a[0] * b[0]
        + a[1] * b[1]
        + a[2] * b[2]
        + 2.0 * (a[3] * b[3] + a[4] * b[4] + a[5] * b[5])
    )


def _von_mises(stress: list[float]) -> float:
    dev = _deviator(stress)
    return math.sqrt(max(0.0, 1.5 * _double_contract(dev, dev)))


def _elastic_increment(delta_eps: list[float], lame_lambda: float, shear_modulus: float) -> list[float]:
    volumetric = delta_eps[0] + delta_eps[1] + delta_eps[2]
    return [
        lame_lambda * volumetric + 2.0 * shear_modulus * delta_eps[0],
        lame_lambda * volumetric + 2.0 * shear_modulus * delta_eps[1],
        lame_lambda * volumetric + 2.0 * shear_modulus * delta_eps[2],
        2.0 * shear_modulus * delta_eps[3],
        2.0 * shear_modulus * delta_eps[4],
        2.0 * shear_modulus * delta_eps[5],
    ]


def integrate_j2_strain_path(
    strain_path: list[list[float]],
    parameters: dict[str, Any],
) -> dict[str, Any]:
    """Integrate a total-strain path with isotropic linear-hardening J2 plasticity."""
    if not strain_path:
        raise ValueError("strain_path must not be empty.")

    e_modulus = float(parameters["E"])
    poisson = float(parameters["nu"])
    sigma_y0 = float(parameters["sigma_y0"])
    hardening = float(parameters.get("H_iso", 0.0))

    if e_modulus <= 0.0:
        raise ValueError("E must be positive.")
    if poisson <= -1.0 or poisson >= 0.5:
        raise ValueError("nu must be in (-1, 0.5).")

    shear = e_modulus / (2.0 * (1.0 + poisson))
    lame = e_modulus * poisson / ((1.0 + poisson) * (1.0 - 2.0 * poisson))

    stress = [0.0] * 6
    eqp = float(parameters.get("eqp0", 0.0))
    previous_strain = [0.0] * 6
    rows: list[dict[str, Any]] = []

    for step, strain_values in enumerate(strain_path):
        strain = _as_six(strain_values, "strain_path row")
        delta_eps = [strain[i] - previous_strain[i] for i in range(6)]
        previous_strain = strain

        delta_stress = _elastic_increment(delta_eps, lame, shear)
        trial = [stress[i] + delta_stress[i] for i in range(6)]
        seq_trial = _von_mises(trial)
        yield_stress = sigma_y0 + hardening * eqp
        yield_value = seq_trial - yield_stress

        plastic = yield_value > 1.0e-10
        delta_gamma = 0.0
        if plastic:
            delta_gamma = yield_value / (3.0 * shear + hardening)
            dev_trial = _deviator(trial)
            scale = max(0.0, 1.0 - (3.0 * shear * delta_gamma / max(seq_trial, 1.0e-30)))
            dev_new = [scale * value for value in dev_trial]
            mean = _trace(trial) / 3.0
            stress = [
                dev_new[0] + mean,
                dev_new[1] + mean,
                dev_new[2] + mean,
                dev_new[3],
                dev_new[4],
                dev_new[5],
            ]
            eqp += delta_gamma
        else:
            stress = trial

        rows.append(
            {
                "step": step,
                "strain": strain,
                "stress": stress[:],
                "von_mises": _von_mises(stress),
                "eqp": eqp,
                "yield_value_trial": yield_value,
                "delta_gamma": delta_gamma,
                "plastic": plastic,
            }
        )

    return {
        "success": True,
        "model": "J2LinearIsotropicHardening",
        "voigt_order": "[xx, yy, zz, xy, yz, zx]",
        "shear_convention": "tensor_strain",
        "parameters": {
            "E": e_modulus,
            "nu": poisson,
            "sigma_y0": sigma_y0,
            "H_iso": hardening,
        },
        "rows": rows,
    }


def uniaxial_strain_path(strain_values: list[float]) -> list[list[float]]:
    """Build an xx-only total-strain path."""
    return [[float(value), 0.0, 0.0, 0.0, 0.0, 0.0] for value in strain_values]

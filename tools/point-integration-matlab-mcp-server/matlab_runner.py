"""
matlab_runner.py - MATLAB batch launcher for generated point-integration scripts.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Any


def matlab_available(matlab_executable: str) -> dict[str, Any]:
    resolved = shutil.which(matlab_executable) if not os.path.isabs(matlab_executable) else matlab_executable
    exists = bool(resolved and os.path.exists(resolved))
    return {"available": exists, "executable": matlab_executable, "resolved": resolved or ""}


def _j2_matlab_script(input_json: str, output_json: str) -> str:
    return f"""
inputPath = '{input_json.replace("'", "''")}';
outputPath = '{output_json.replace("'", "''")}';
payload = jsondecode(fileread(inputPath));
params = payload.parameters;
strainPath = payload.strain_path;

E = params.E;
nu = params.nu;
sigmaY0 = params.sigma_y0;
if isfield(params, 'H_iso')
    H = params.H_iso;
else
    H = 0.0;
end

G = E / (2.0 * (1.0 + nu));
lambda = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
stress = zeros(6, 1);
eqp = 0.0;
prevStrain = zeros(6, 1);
rows = struct([]);

for k = 1:size(strainPath, 1)
    strain = strainPath(k, :)';
    deps = strain - prevStrain;
    prevStrain = strain;
    tr = deps(1) + deps(2) + deps(3);
    dstress = [lambda * tr + 2 * G * deps(1);
               lambda * tr + 2 * G * deps(2);
               lambda * tr + 2 * G * deps(3);
               2 * G * deps(4);
               2 * G * deps(5);
               2 * G * deps(6)];
    trial = stress + dstress;
    meanStress = (trial(1) + trial(2) + trial(3)) / 3.0;
    dev = [trial(1) - meanStress; trial(2) - meanStress; trial(3) - meanStress; trial(4); trial(5); trial(6)];
    devNorm2 = dev(1)^2 + dev(2)^2 + dev(3)^2 + 2 * (dev(4)^2 + dev(5)^2 + dev(6)^2);
    seqTrial = sqrt(max(0.0, 1.5 * devNorm2));
    yieldValue = seqTrial - (sigmaY0 + H * eqp);
    plastic = yieldValue > 1.0e-10;
    dgamma = 0.0;

    if plastic
        dgamma = yieldValue / (3.0 * G + H);
        scale = max(0.0, 1.0 - 3.0 * G * dgamma / max(seqTrial, 1.0e-30));
        devNew = scale * dev;
        stress = [devNew(1) + meanStress; devNew(2) + meanStress; devNew(3) + meanStress; devNew(4); devNew(5); devNew(6)];
        eqp = eqp + dgamma;
    else
        stress = trial;
    end

    meanStress2 = (stress(1) + stress(2) + stress(3)) / 3.0;
    dev2 = [stress(1) - meanStress2; stress(2) - meanStress2; stress(3) - meanStress2; stress(4); stress(5); stress(6)];
    seq = sqrt(max(0.0, 1.5 * (dev2(1)^2 + dev2(2)^2 + dev2(3)^2 + 2 * (dev2(4)^2 + dev2(5)^2 + dev2(6)^2))));

    rows(k).step = k - 1;
    rows(k).strain = strain';
    rows(k).stress = stress';
    rows(k).von_mises = seq;
    rows(k).eqp = eqp;
    rows(k).yield_value_trial = yieldValue;
    rows(k).delta_gamma = dgamma;
    rows(k).plastic = plastic;
end

result.success = true;
result.backend = 'matlab-generated-script';
result.model = 'J2LinearIsotropicHardening';
result.voigt_order = '[xx, yy, zz, xy, yz, zx]';
result.shear_convention = 'tensor_strain';
result.parameters = params;
result.rows = rows;

fid = fopen(outputPath, 'w');
fprintf(fid, '%s', jsonencode(result));
fclose(fid);
"""


def run_generated_j2(
    matlab_executable: str,
    strain_path: list[list[float]],
    parameters: dict[str, Any],
    timeout_seconds: int = 120,
    keep_temp_files: bool = False,
) -> dict[str, Any]:
    availability = matlab_available(matlab_executable)
    if not availability["available"]:
        return {"success": False, "message": "MATLAB executable not found.", "availability": availability}

    temp_dir = tempfile.mkdtemp(prefix="grpd_matlab_point_")
    input_path = os.path.join(temp_dir, "input.json")
    output_path = os.path.join(temp_dir, "output.json")
    script_path = os.path.join(temp_dir, "run_point_integration.m")

    with open(input_path, "w", encoding="utf-8") as file:
        json.dump({"strain_path": strain_path, "parameters": parameters}, file)
    with open(script_path, "w", encoding="utf-8") as file:
        file.write(_j2_matlab_script(input_path, output_path))

    try:
        process = subprocess.run(
            [availability["resolved"], "-batch", f"run('{script_path}')"],
            cwd=temp_dir,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
        if process.returncode != 0:
            return {
                "success": False,
                "message": f"MATLAB returned {process.returncode}.",
                "stdout_tail": process.stdout[-4000:],
                "stderr_tail": process.stderr[-4000:],
                "temp_dir": temp_dir if keep_temp_files else "",
            }
        with open(output_path, "r", encoding="utf-8") as file:
            return json.load(file)
    except subprocess.TimeoutExpired:
        return {"success": False, "message": f"MATLAB timed out after {timeout_seconds}s.", "temp_dir": temp_dir}
    finally:
        if not keep_temp_files:
            shutil.rmtree(temp_dir, ignore_errors=True)


def safe_temp_root() -> str:
    return str(Path(tempfile.gettempdir()).resolve())

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


def _generate_j2_linear_script(input_json: str, output_json: str) -> str:
    return rf"""
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
    rows(k).damage = 0.0;
    rows(k).yield_value_trial = yieldValue;
    rows(k).delta_gamma = dgamma;
    rows(k).plastic = plastic;
end

% 1. 导出 XLSX 表格
steps = [rows.step]';
strains = reshape([rows.strain], 6, [])';
stresses = reshape([rows.stress], 6, [])';
von_mises = [rows.von_mises]';
eqps = [rows.eqp]';
damages = [rows.damage]';

T = table(steps, ...
          strains(:,1), strains(:,2), strains(:,3), strains(:,4), strains(:,5), strains(:,6), ...
          stresses(:,1), stresses(:,2), stresses(:,3), stresses(:,4), stresses(:,5), stresses(:,6), ...
          von_mises, eqps, damages, ...
          'VariableNames', {{'Step', ...
                            'Strain_xx', 'Strain_yy', 'Strain_zz', 'Strain_xy', 'Strain_yz', 'Strain_zx', ...
                            'Stress_xx', 'Stress_yy', 'Stress_zz', 'Stress_xy', 'Stress_yz', 'Stress_zx', ...
                            'VonMises', 'EqPS', 'Damage'}});

xlsxPath = fullfile(fileparts(outputPath), 'constitutive_results.xlsx');
writetable(T, xlsxPath);

% 2. 绘制 PNG 图片
fig = figure('Visible', 'off');
subplot(2,1,1);
plot(strains(:,1), von_mises, 'r-^', 'LineWidth', 1.5, 'MarkerSize', 4);
grid on;
title('Mises Stress - Strain Relationship (\sigma_{{eq}} - \epsilon_{{xx}})');
xlabel('Strain \epsilon_{{xx}}');
ylabel('Mises Stress \sigma_{{eq}} (MPa)');

subplot(2,1,2);
yyaxis left;
plot(steps, eqps, 'b-o', 'LineWidth', 1.5, 'MarkerSize', 4);
ylabel('EqPS \epsilon_p');
yyaxis right;
plot(steps, damages, 'm-s', 'LineWidth', 1.5, 'MarkerSize', 4);
ylabel('Lemaitre Damage D');
grid on;
title('Equivalent Plastic Strain & Damage Evolution');
xlabel('Step');

pngPath = fullfile(fileparts(outputPath), 'constitutive_plot.png');
saveas(fig, pngPath);
close(fig);

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


def _generate_j2_voce_lemaitre_script(input_json: str, output_json: str) -> str:
    return rf"""
inputPath = '{input_json.replace("'", "''")}';
outputPath = '{output_json.replace("'", "''")}';
payload = jsondecode(fileread(inputPath));
params = payload.parameters;
strainPath = payload.strain_path;

E = params.YoungsModulus;
nu = params.PoissonsRatio;
sigma0 = params.YieldStress;
K1 = params.LinearHardening;
VoceR = params.VoceR;
VoceB = params.VoceB;
S = params.LemaitreS;
s = params.Lemaitre_s;
p_D = params.DamageThreshold;
Dc = params.CriticalDamage;
damageAccelThreshold = params.DamageAccelThreshold;
damageAccelFactor = params.DamageAccelFactor;

G = E / (2.0 * (1.0 + nu));
lambda = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
bulk = E / (3.0 * (1.0 - 2.0 * nu));

stress = zeros(6, 1);
eqp = 0.0;
real_eq = 0.0;
damage = 0.0;
epsP = zeros(6, 1); % 塑性应变张量
rows = struct([]);

for k = 1:size(strainPath, 1)
    strain = strainPath(k, :)';
    epsE = strain - epsP;
    trE = epsE(1) + epsE(2) + epsE(3);
    sTrial = [lambda * trE + 2 * G * epsE(1);
              lambda * trE + 2 * G * epsE(2);
              lambda * trE + 2 * G * epsE(3);
              2 * G * epsE(4);
              2 * G * epsE(5);
              2 * G * epsE(6)];
              
    sM = (sTrial(1) + sTrial(2) + sTrial(3)) / 3.0;
    sDevTrial = [sTrial(1) - sM; sTrial(2) - sM; sTrial(3) - sM; sTrial(4); sTrial(5); sTrial(6)];
    devNorm2 = sDevTrial(1)^2 + sDevTrial(2)^2 + sDevTrial(3)^2 + 2.0 * (sDevTrial(4)^2 + sDevTrial(5)^2 + sDevTrial(6)^2);
    J2 = sqrt(max(0.0, 1.5 * devNorm2));
    
    Wn = max(1.0 - damage, 0.001);
    
    if damage >= Dc
        p = sM;
        if sM >= 0
            p = Wn * sM;
        end
        stress = Wn * sDevTrial + p * [1; 1; 1; 0; 0; 0];
        mises = sqrt(1.5 * (stress(1)^2 + stress(2)^2 + stress(3)^2 + 2 * (stress(4)^2 + stress(5)^2 + stress(6)^2)));
        
        rows(k).step = k - 1;
        rows(k).strain = strain';
        rows(k).stress = stress';
        rows(k).von_mises = mises;
        rows(k).eqp = eqp;
        rows(k).damage = damage;
        continue;
    end
    
    gamma = 0.0;
    alphaNew = eqp;
    realEqNew = real_eq;
    W = Wn;
    SY = sigma0 + K1 * eqp + VoceR * (1.0 - exp(-VoceB * eqp));
    
    if J2 > 1e-16
        f = J2 - SY;
        if f > 1e-8
            % 隐式求解塑性乘子 dg
            res_fun = @(dg) J2 - 3.0 * G * dg / Wn - (sigma0 + K1 * (eqp + dg) + VoceR * (1.0 - exp(-VoceB * (eqp + dg))));
            dg_max = J2 * Wn / (3.0 * G);
            if dg_max > 0
                dgamma = fzero(res_fun, [0, dg_max]);
            else
                dgamma = 0.0;
            end
            
            gamma = dgamma;
            alphaNew = eqp + gamma;
            realEqNew = real_eq + gamma / Wn;
            
            SY = sigma0 + K1 * alphaNew + VoceR * (1.0 - exp(-VoceB * alphaNew));
            q = Wn * SY;
            p = sM;
            if sM >= 0
                p = Wn * sM;
            end
            
            sDevNew = (q / J2) * sDevTrial;
            stress = [sDevNew(1) + p; sDevNew(2) + p; sDevNew(3) + p; sDevNew(4); sDevNew(5); sDevNew(6)];
            
            % 更新塑性应变
            if sM >= 0
                volFactor = p / (3.0 * Wn * bulk);
            else
                volFactor = sM / (3.0 * bulk);
            end
            
            epsENew = [sDevNew(1) / (2.0 * G * Wn) + volFactor;
                       sDevNew(2) / (2.0 * G * Wn) + volFactor;
                       sDevNew(3) / (2.0 * G * Wn) + volFactor;
                       sDevNew(4) / (2.0 * G * Wn);
                       sDevNew(5) / (2.0 * G * Wn);
                       sDevNew(6) / (2.0 * G * Wn)];
                       
            epsP = epsP + epsE - epsENew;
            mises = q;
            
            % 隐式演化损伤
            W = Wn;
            if realEqNew > p_D
                Y = -(q^2 / (6.0 * G * Wn * Wn));
                if sM >= 0.0
                    Y = Y - sM^2 / (2.0 * bulk);
                end
                if damage >= damageAccelThreshold
                    Y = Y * damageAccelFactor;
                end
                
                f2 = -Y / S;
                f2s = f2^s;
                
                res2 = 1.0;
                iter2 = 1;
                while abs(res2) > 1.0e-8 && iter2 <= 200
                    res2 = W - Wn + f2s * gamma / W;
                    H22 = 1.0 - f2s * gamma / (W * W);
                    if abs(H22) < 1.0e-30
                        break;
                    end
                    W = W - res2 / H22;
                    iter2 = iter2 + 1;
                end
                if W < 0.001
                    W = 0.001;
                end
            end
        else
            p = sM;
            if sM >= 0
                p = Wn * sM;
            end
            stress = Wn * sDevTrial + p * [1; 1; 1; 0; 0; 0];
            mises = Wn * J2;
        end
    else
        p = sM;
        if sM >= 0
            p = Wn * sM;
        end
        stress = Wn * sDevTrial + p * [1; 1; 1; 0; 0; 0];
        mises = 0.0;
    end
    
    eqp = alphaNew;
    real_eq = realEqNew;
    damage = max(damage, 1.0 - W);
    
    rows(k).step = k - 1;
    rows(k).strain = strain';
    rows(k).stress = stress';
    rows(k).von_mises = mises;
    rows(k).eqp = eqp;
    rows(k).damage = damage;
end

% 1. 导出 XLSX 表格
steps = [rows.step]';
strains = reshape([rows.strain], 6, [])';
stresses = reshape([rows.stress], 6, [])';
von_mises = [rows.von_mises]';
eqps = [rows.eqp]';
damages = [rows.damage]';

T = table(steps, ...
          strains(:,1), strains(:,2), strains(:,3), strains(:,4), strains(:,5), strains(:,6), ...
          stresses(:,1), stresses(:,2), stresses(:,3), stresses(:,4), stresses(:,5), stresses(:,6), ...
          von_mises, eqps, damages, ...
          'VariableNames', {{'Step', ...
                            'Strain_xx', 'Strain_yy', 'Strain_zz', 'Strain_xy', 'Strain_yz', 'Strain_zx', ...
                            'Stress_xx', 'Stress_yy', 'Stress_zz', 'Stress_xy', 'Stress_yz', 'Stress_zx', ...
                            'VonMises', 'EqPS', 'Damage'}});

xlsxPath = fullfile(fileparts(outputPath), 'constitutive_results.xlsx');
writetable(T, xlsxPath);

% 2. 绘制 PNG 图片
fig = figure('Visible', 'off');
subplot(2,1,1);
plot(strains(:,1), von_mises, 'r-^', 'LineWidth', 1.5, 'MarkerSize', 4);
grid on;
title('Mises Stress - Strain Relationship (\sigma_{{eq}} - \epsilon_{{xx}})');
xlabel('Strain \epsilon_{{xx}}');
ylabel('Mises Stress \sigma_{{eq}} (MPa)');

subplot(2,1,2);
yyaxis left;
plot(steps, eqps, 'b-o', 'LineWidth', 1.5, 'MarkerSize', 4);
ylabel('EqPS \epsilon_p');
yyaxis right;
plot(steps, damages, 'm-s', 'LineWidth', 1.5, 'MarkerSize', 4);
ylabel('Lemaitre Damage D');
grid on;
title('Equivalent Plastic Strain & Damage Evolution');
xlabel('Step');

pngPath = fullfile(fileparts(outputPath), 'constitutive_plot.png');
saveas(fig, pngPath);
close(fig);

result.success = true;
result.backend = 'matlab-generated-script';
result.model = 'J2VoceLemaitre';
result.voigt_order = '[xx, yy, zz, xy, yz, zx]';
result.shear_convention = 'tensor_strain';
result.parameters = params;
result.rows = rows;

fid = fopen(outputPath, 'w');
fprintf(fid, '%s', jsonencode(result));
fclose(fid);
"""


def run_dynamic_constitutive(
    matlab_executable: str,
    strain_path: list[list[float]],
    material_type: str,
    parameters: dict[str, Any],
    work_dir: str = "",
    timeout_seconds: int = 120,
    keep_temp_files: bool = False,
) -> dict[str, Any]:
    availability = matlab_available(matlab_executable)
    if not availability["available"]:
        return {"success": False, "message": "MATLAB executable not found.", "availability": availability}

    is_temp = False
    if not work_dir:
        is_temp = True
        work_dir = tempfile.mkdtemp(prefix="grpd_matlab_point_")
    else:
        work_dir = os.path.abspath(work_dir)
        os.makedirs(work_dir, exist_ok=True)

    input_path = os.path.join(work_dir, "input.json")
    output_path = os.path.join(work_dir, "output.json")
    script_path = os.path.join(work_dir, "run_point_integration.m")

    with open(input_path, "w", encoding="utf-8") as file:
        json.dump({"strain_path": strain_path, "parameters": parameters}, file)
        
    with open(script_path, "w", encoding="utf-8") as file:
        if material_type == "J2VoceLemaitre":
            file.write(_generate_j2_voce_lemaitre_script(input_path, output_path))
        else:
            # 默认为 J2 线性各向同性硬化
            file.write(_generate_j2_linear_script(input_path, output_path))

    try:
        process = subprocess.run(
            [availability["resolved"], "-batch", f"run('{script_path}')"],
            cwd=work_dir,
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
                "work_dir": work_dir if (keep_temp_files or not is_temp) else "",
            }

        # 运行成功，如果是指定的非临时目录，执行打包压缩 ZIP 的逻辑
        if not is_temp:
            import zipfile
            zip_path = os.path.join(work_dir, "matlab_constitutive_results.zip")
            files_to_zip = []
            for f in os.listdir(work_dir):
                fp = os.path.join(work_dir, f)
                if os.path.isfile(fp) and f != "matlab_constitutive_results.zip":
                    files_to_zip.append(fp)
            try:
                with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                    for fp in files_to_zip:
                        zipf.write(fp, os.path.basename(fp))
            except Exception:
                pass

        with open(output_path, "r", encoding="utf-8") as file:
            res_data = json.load(file)
            if not is_temp:
                res_data["zip_path"] = os.path.join(work_dir, "matlab_constitutive_results.zip")
                res_data["xlsx_path"] = os.path.join(work_dir, "constitutive_results.xlsx")
                res_data["png_path"] = os.path.join(work_dir, "constitutive_plot.png")
            return res_data
    except subprocess.TimeoutExpired:
        return {"success": False, "message": f"MATLAB timed out after {timeout_seconds}s.", "work_dir": work_dir}
    finally:
        if is_temp and not keep_temp_files:
            shutil.rmtree(work_dir, ignore_errors=True)


def safe_temp_root() -> str:
    return str(Path(tempfile.gettempdir()).resolve())

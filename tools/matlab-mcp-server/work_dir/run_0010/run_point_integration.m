% GRPD Elastoplastic Constitutive Point-Integration Verification Harness
%
% This Harness uses a modular layout:
% 1. Dynamically load all material parameters from input JSON via reflection.
% 2. Decouple specific hardening laws and damage evolution from the main loop via local functions.
% 3. Keep only continuum mechanics and deformation metric calculations in the loop body.

inputPath = 'input.json';
outputPath = 'output.json';
payload = jsondecode(fileread(inputPath));
params = payload.parameters;
F_path = payload.F_path;

% 1. Default initialization of basic elastic parameters to prevent undefined variables
LinearHardening = 0.0;
VoceR = 0.0;
VoceB = 0.0;
LemaitreS = 1.0;
Lemaitre_s = 1.0;
DamageThreshold = 0.0;
CriticalDamage = 0.99;
DamageAccelThreshold = 1.0;
DamageAccelFactor = 1.0;

% 2. Execute dynamic parameter field reflection
fields = fieldnames(params);
for i = 1:numel(fields)
    eval(sprintf('%s = params.%s;', fields{i}, fields{i}));
end

E = params.YoungsModulus;
nu = params.PoissonsRatio;
sigma0 = params.YieldStress;
isLarge = params.LargeDeformation;

hasDamage = true;
if hasDamage
    S = params.LemaitreS;
    s = params.Lemaitre_s;
    p_D = params.DamageThreshold;
    Dc = params.CriticalDamage;
    damageAccelThreshold = params.DamageAccelThreshold;
    damageAccelFactor = params.DamageAccelFactor;
end

% Lame constants calculation
G = E / (2.0 * (1.0 + nu));
lambda = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
bulk = E / (3.0 * (1.0 - 2.0 * nu));

% Initialize output
eqp = 0.0;
real_eq = 0.0;
damage = 0.0;
epsP = zeros(3, 3);
rows = struct([]);

for k = 1:size(F_path, 1)
    F_flat = F_path(k, :);
    F = [F_flat(1), F_flat(2), F_flat(3);
         F_flat(4), F_flat(5), F_flat(6);
         F_flat(7), F_flat(8), F_flat(9)];

    if isLarge
        epsMat = 0.5 * (F' * F - eye(3));
    else
        epsMat = 0.5 * (F + F' - 2.0 * eye(3));
    end
    strain = epsMat;
    % 9-component strain tensor for JSON output (row-major)
    strain9 = [epsMat(1,1), epsMat(1,2), epsMat(1,3), ...
               epsMat(2,1), epsMat(2,2), epsMat(2,3), ...
               epsMat(3,1), epsMat(3,2), epsMat(3,3)];

    epsE = strain - epsP;
    trE = trace(epsE);
    sTrial = lambda * trE * eye(3) + 2.0 * G * epsE;

    sM = trace(sTrial) / 3.0;
    sDevTrial = sTrial - sM * eye(3);
    devNorm2 = sum(sum(sDevTrial .* sDevTrial));
    J2 = sqrt(max(0.0, 1.5 * devNorm2));

    if hasDamage
        Wn = max(1.0 - damage, 0.001);
    else
        Wn = 1.0;
    end

    % Failure truncation
    if hasDamage && damage >= Dc
        p = sM;
        if sM >= 0
            p = Wn * sM;
        end
        sigma_cauchy = Wn * sDevTrial + p * eye(3);
    else
        gamma = 0.0;
        alphaNew = eqp;
        if hasDamage
            realEqNew = real_eq;
        end
        W = Wn;
        SY = eval_yield_stress(eqp, params);

        if J2 > 1e-16
            f = J2 - SY;
            if f > 1e-8
                % Call local Newton-Raphson solver for dgamma
                [dgamma, converged] = solve_plastic_multiplier(J2, eqp, Wn, G, params);
                if ~converged
                    warning('Plastic multiplier Newton-Raphson failed to converge at step %d', k);
                end

                gamma = dgamma;
                alphaNew = eqp + gamma;

                SY = eval_yield_stress(alphaNew, params);
                if hasDamage
                    realEqNew = real_eq + gamma / Wn;
                    q = Wn * SY;
                    p = sM;
                    if sM >= 0
                        p = Wn * sM;
                    end
                else
                    q = SY;
                    p = sM;
                end

                sDevNew = (q / J2) * sDevTrial;
                sigma_cauchy = sDevNew + p * eye(3);

                if hasDamage
                    if sM >= 0
                        volFactor = p / (3.0 * Wn * bulk);
                    else
                        volFactor = sM / (3.0 * bulk);
                    end
                    epsENew = sDevNew / (2.0 * G * Wn) + volFactor * eye(3);
                else
                    volFactor = sM / (3.0 * bulk);
                    epsENew = sDevNew / (2.0 * G) + volFactor * eye(3);
                end
                epsP = strain - epsENew;

                if hasDamage && realEqNew > p_D
                    Y = -(q^2 / (6.0 * G * Wn * Wn));
                    if sM >= 0.0
                        Y = Y - sM^2 / (2.0 * bulk);
                    end
                    if damage >= damageAccelThreshold
                        Y = Y * damageAccelFactor;
                    end

                    f2 = -Y / S;
                    f2s = f2^s;

                    % Call local Newton-Raphson solver for damage factor W
                    [W, converged_dmg] = solve_damage_factor(Wn, f2s, dgamma, params);
                    if ~converged_dmg
                        warning('Damage Newton-Raphson failed to converge at step %d', k);
                    end
                end
            else
                % Elastic state
                if hasDamage
                    p = sM;
                    if sM >= 0
                        p = Wn * sM;
                    end
                    sigma_cauchy = Wn * sDevTrial + p * eye(3);
                else
                    p = sM;
                    sigma_cauchy = sDevTrial + p * eye(3);
                end
            end
        else
            % Pure hydrostatic trial
            if hasDamage
                p = sM;
                if sM >= 0
                    p = Wn * sM;
                end
                sigma_cauchy = Wn * sDevTrial + p * eye(3);
            else
                p = sM;
                sigma_cauchy = sDevTrial + p * eye(3);
            end
        end

        eqp = alphaNew;
        if hasDamage
            real_eq = realEqNew;
            damage = max(damage, 1.0 - W);
        end
    end

    % Von Mises and Cauchy stress deviator calculation
    sDev_cauchy = sigma_cauchy - trace(sigma_cauchy)/3.0 * eye(3);
    mises = sqrt(1.5 * sum(sum(sDev_cauchy .* sDev_cauchy)));

    % 9-component stress tensor for output (row-major)
    if isLarge
        PK1 = F * sigma_cauchy;
        stress9 = [PK1(1,1), PK1(1,2), PK1(1,3), ...
                   PK1(2,1), PK1(2,2), PK1(2,3), ...
                   PK1(3,1), PK1(3,2), PK1(3,3)];
    else
        stress9 = [sigma_cauchy(1,1), sigma_cauchy(1,2), sigma_cauchy(1,3), ...
                   sigma_cauchy(2,1), sigma_cauchy(2,2), sigma_cauchy(2,3), ...
                   sigma_cauchy(3,1), sigma_cauchy(3,2), sigma_cauchy(3,3)];
    end

    rows(k).step = k - 1;
    rows(k).strain = strain9;
    rows(k).stress = stress9;
    rows(k).von_mises = mises;
    rows(k).eqp = eqp;
    rows(k).damage = damage;
end

steps = [rows.step]';
strains = reshape([rows.strain], 9, [])';
stresses = reshape([rows.stress], 9, [])';
von_mises = [rows.von_mises]';
eqps = [rows.eqp]';
damages = [rows.damage]';

T = table(steps, ...
          strains(:,1), strains(:,2), strains(:,3), strains(:,4), strains(:,5), strains(:,6), strains(:,7), strains(:,8), strains(:,9), ...
          stresses(:,1), stresses(:,2), stresses(:,3), stresses(:,4), stresses(:,5), stresses(:,6), stresses(:,7), stresses(:,8), stresses(:,9), ...
          von_mises, eqps, damages, ...
          'VariableNames', {'Step', ...
                            'Strain_11', 'Strain_12', 'Strain_13', 'Strain_21', 'Strain_22', 'Strain_23', 'Strain_31', 'Strain_32', 'Strain_33', ...
                            'Stress_11', 'Stress_12', 'Stress_13', 'Stress_21', 'Stress_22', 'Stress_23', 'Stress_31', 'Stress_32', 'Stress_33', ...
                            'VonMises', 'EqPS', 'Damage'});

xlsxPath = fullfile(fileparts(outputPath), 'constitutive_results.xlsx');
writetable(T, xlsxPath);

fig = figure('Visible', 'off');
subplot(2,1,1);
plot(strains(:,1), von_mises, 'r-^', 'LineWidth', 1.5, 'MarkerSize', 4); % strains(:,1) = eps_xx
grid on;
if isLarge
    title('PK1 Stress - Strain Relationship');
else
    title('Cauchy Stress - Strain Relationship');
end
xlabel('Strain \epsilon_{xx}');
ylabel('Stress (MPa)');

subplot(2,1,2);
if hasDamage
    yyaxis left;
    plot(steps, eqps, 'b-o', 'LineWidth', 1.5, 'MarkerSize', 4);
    ylabel('EqPS \epsilon_p');
    yyaxis right;
    plot(steps, damages, 'm-s', 'LineWidth', 1.5, 'MarkerSize', 4);
    ylabel('Lemaitre Damage D');
    title('Equivalent Plastic Strain & Damage Evolution');
else
    plot(steps, eqps, 'b-o', 'LineWidth', 1.5, 'MarkerSize', 4);
    ylabel('EqPS \epsilon_p');
    title('Equivalent Plastic Strain Evolution');
end
grid on;
xlabel('Step');

pngPath = fullfile(fileparts(outputPath), 'constitutive_plot.png');
saveas(fig, pngPath);
close(fig);

result.success = true;
result.backend = 'matlab-custom-script';
result.model = 'J2Voce';
result.parameters = params;
result.rows = rows;

fid = fopen(outputPath, 'w');
fprintf(fid, '%s', jsonencode(result));
fclose(fid);

% ==========================================================
% Local function placeholder
% ==========================================================

function SY = eval_yield_stress(alpha, params)
    SY = params.LinearHardening*alpha + params.VoceR*(1 - exp(-params.VoceB*alpha)) + params.YieldStress;
end

function H = eval_hardening_slope(alpha, params)
    H = params.LinearHardening + params.VoceB*params.VoceR*exp(-params.VoceB*alpha);
end

function [dg, converged] = solve_plastic_multiplier(J2, alphaOld, Wn, G, params)
    tol = 1.0e-8;
    maxIter = 200;
    tiny = 1.0e-30;

    dg = (J2 - eval_yield_stress(alphaOld, params)) * Wn / (3.0 * G);
    dg = max(0.0, dg);
    converged = false;
    res = 1.0;
    iter = 1;

    while abs(res) > tol && iter <= maxIter
        alpha = alphaOld + dg;
        SY = eval_yield_stress(alpha, params);
        res = J2 - 3.0 * G * dg / Wn - SY;
        if abs(res) < tol
            converged = true;
        end
        H11 = -3.0 * G / Wn - eval_hardening_slope(alpha, params);
        if abs(H11) < tiny
            break;
        end
        dg = dg - res / H11;
        iter = iter + 1;
    end
end

function [W, converged] = solve_damage_factor(Wn, f2s, dg, params)
    tol = 1.0e-8;
    maxIter = 200;
    tiny = 1.0e-30;
    minW = 0.001;

    W = Wn;
    converged = false;
    res = 1.0;
    iter = 1;

    while abs(res) > tol && iter <= maxIter
        res = W - Wn + f2s * dg / W;
        if abs(res) < tol
            converged = true;
        end
        H22 = 1.0 - f2s * dg / (W * W);
        if abs(H22) < tiny
            break;
        end
        W = W - res / H22;
        if W < minW
            W = minW;
            break;
        end
        iter = iter + 1;
    end
end


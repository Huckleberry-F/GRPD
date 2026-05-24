# -*- coding: utf-8 -*-
"""
m_generator.py - MATLAB constitutive integration script generator.
"""
import os
import yaml
from symbolic_generator import derive_constitutive_elements

def generate_local_functions(sy_expr_str: str, slope_expr_str: str) -> str:
    """
    �?SymPy 导出的原始公式替换为 params 结构体的属性读取形式，并生成供主驱动调用的局部函数�?    由于 MATLAB 局部函数无法直接访问主脚本的上下文局部变量，因此在局部函数内通过 params 直接获取参数�?    """
    mapping = {
        "sigma0": "params.YieldStress",
        "K1": "params.LinearHardening",
        "VoceR": "params.VoceR",
        "VoceB": "params.VoceB"
    }

    sy_mapped = sy_expr_str
    slope_mapped = slope_expr_str
    for sym, param_field in mapping.items():
        sy_mapped = sy_mapped.replace(sym, param_field)
        slope_mapped = slope_mapped.replace(sym, param_field)

    local_funcs = rf"""
function SY = eval_yield_stress(alpha, params)
    SY = {sy_mapped};
end

function H = eval_hardening_slope(alpha, params)
    H = {slope_mapped};
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
"""
    return local_funcs

def generate_matlab_constitutive_script(parameters: dict, input_json_name: str = "input.json", output_json_name: str = "output.json") -> str:
    """
    根据本构参数和材�?Spec YAML 动态生�?MATLAB 单点积分校验脚本�?    """
    # 1. 优先检测人为指定的m文件拦截逻辑
    if "custom_m_file" in parameters and parameters["custom_m_file"]:
        custom_file_path = parameters["custom_m_file"]
        if os.path.exists(custom_file_path):
            with open(custom_file_path, "r", encoding="utf-8") as f:
                return f.read()
        else:
            raise FileNotFoundError(f"Custom MATLAB script not found at: {custom_file_path}")

    # 2. 定位材料模型 Spec (YAML)
    spec_dict = {}
    spec_file = parameters.get("spec_file")
    if spec_file and os.path.exists(spec_file):
        with open(spec_file, "r", encoding="utf-8") as f:
            spec_dict = yaml.safe_load(f)
    else:
        # 如果没有指定 spec_file，根�?model 参数动态选择
        model_type = parameters.get("model", "J2VoceLemaitre")
        server_dir = os.path.dirname(os.path.abspath(__file__))

        if model_type == "J2Voce":
            spec_path = os.path.join(server_dir, "specs", "J2Voce.yaml")
        else:
            spec_path = os.path.join(server_dir, "specs", "J2VoceLemaitre.yaml")

        if os.path.exists(spec_path):
            with open(spec_path, "r", encoding="utf-8") as f:
                spec_dict = yaml.safe_load(f)
        else:
            spec_dict = {
                "model": model_type,
                "yield_function": {
                    "hardening": {
                        "type": "voce_linear" if "VoceR" in parameters else "linear"
                    }
                },
                "damage": {
                    "enabled": "LemaitreS" in parameters
                }
            }

    # 3. 通过符号引擎获取屈服公式和斜率表达式
    sym = derive_constitutive_elements(spec_dict)

    # 4. 加载固定驱动模板
    server_dir = os.path.dirname(os.path.abspath(__file__))
    template_path = os.path.join(server_dir, "templates", "matlab_harness.m.template")
    if not os.path.exists(template_path):
        raise FileNotFoundError(f"MATLAB harness template not found at: {template_path}")

    with open(template_path, "r", encoding="utf-8") as f:
        template_content = f.read()

    has_damage = spec_dict.get("damage", {}).get("enabled", False)
    has_damage_str = "true" if has_damage else "false"

    # 5. 生成需要注入的局部物理核函数定义
    local_functions_code = generate_local_functions(sym["sy_expr"], sym["slope_expr"])

    # 6. Perform template interpolation
    rendered = template_content
    rendered = rendered.replace("{INPUT_JSON_NAME}", input_json_name)
    rendered = rendered.replace("{OUTPUT_JSON_NAME}", output_json_name)
    rendered = rendered.replace("{HAS_DAMAGE}", has_damage_str)
    rendered = rendered.replace("{LOCAL_FUNCTIONS_PLACEHOLDER}", local_functions_code)

    return rendered

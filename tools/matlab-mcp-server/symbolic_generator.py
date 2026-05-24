# -*- coding: utf-8 -*-
r"""
symbolic_generator.py - Symbolic constitutive derivation engine.
Uses SymPy to dynamically derive hardening equations, residuals, and derivatives,
and generates equivalent MATLAB expressions to avoid hardcoding.
"""
import sympy as sp

def derive_constitutive_elements(parameters_or_spec: dict) -> dict:
    r"""
    使用 SymPy 符号库自动推导本构积分方程的各个代数项，并输出对应的 MATLAB 代码表达式。
    支持输入参数字典或结构化的材料规格 IR 字典。

    返回字典包含:
    - sy_expr: 屈服强化应力表达式 \sigma_y(alpha)
    - slope_expr: 屈服强化斜率表达式 d\sigma_y/d\alpha
    """
    # 声明等效塑性应变符号自变量 alpha
    alpha = sp.Symbol('alpha')

    # 动态生成屈服应力强化公式
    sigma0 = sp.Symbol('sigma0')
    hardening_expr = sigma0

    # 检测是 Spec (YAML) 结构还是扁平的 parameters 结构
    is_spec = "yield_function" in parameters_or_spec

    if is_spec:
        # Spec (YAML) 模式
        hardening_cfg = parameters_or_spec["yield_function"].get("hardening", {})
        h_type = hardening_cfg.get("type", "linear")
        if h_type == "voce_linear":
            K1 = sp.Symbol('K1')
            VoceR = sp.Symbol('VoceR')
            VoceB = sp.Symbol('VoceB')
            hardening_expr += K1 * alpha + VoceR * (1 - sp.exp(-VoceB * alpha))
        elif h_type == "linear":
            K1 = sp.Symbol('K1')
            hardening_expr += K1 * alpha
    else:
        # 兼容老旧 parameters 字典格式
        params = parameters_or_spec
        if "LinearHardening" in params:
            K1 = sp.Symbol('K1')
            hardening_expr += K1 * alpha
        if "VoceR" in params and "VoceB" in params:
            VoceR = sp.Symbol('VoceR')
            VoceB = sp.Symbol('VoceB')
            hardening_expr += VoceR * (1 - sp.exp(-VoceB * alpha))

    # 计算硬化斜率
    slope_expr = sp.diff(hardening_expr, alpha)

    # 格式化输出为 MATLAB 语法
    def to_matlab_str(expr):
        if expr is None:
            return ""
        s_expr = str(expr)
        return s_expr.replace('**', '^')

    return {
        "sy_expr": to_matlab_str(hardening_expr),
        "slope_expr": to_matlab_str(slope_expr)
    }

if __name__ == "__main__":
    # 测试 Spec 模式
    spec_example = {
        "yield_function": {
            "hardening": {
                "type": "voce_linear",
                "yield_stress": "sigma0",
                "linear": "K1",
                "voce_R": "VoceR",
                "voce_B": "VoceB"
            }
        }
    }
    res = derive_constitutive_elements(spec_example)
    print("=== Spec Mode Hardening Derivation ===")
    print("Derived sy_expr:", res["sy_expr"])
    print("Derived slope_expr:", res["slope_expr"])

# -*- coding: utf-8 -*-
"""
result_parser.py - 共 9 分量行主序张量对标解析与绘图模块。
C++ 和 MATLAB 双端均输出 9 分量行主序应力/应变张量：
  [M(0,0), M(0,1), M(0,2), M(1,0), M(1,1), M(1,2), M(2,0), M(2,1), M(2,2)]
对应物理分量：
  [σ₁₁, σ₁₂, σ₁₃, σ₂₁, σ₂₂, σ₂₃, σ₃₁, σ₃₂, σ₃₃]
"""
import os
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


# 9 分量行主序张量的分量名称
TENSOR9_NAMES = ["11", "12", "13", "21", "22", "23", "31", "32", "33"]


def compare_results_and_plot(
    cpp_json_path: str,
    matlab_json_path: str,
    output_plot_path: str,
    tol_stress: float = 1.0e-5,
    tol_state: float = 1.0e-10,
    e_xx_path: list = None
) -> dict:
    """
    读取并对比 C++ 端与 MATLAB 端的计算结果，对全部 9 个应力张量分量逐一判定误差，
    并绘制对比曲线。

    参数:
    - cpp_json_path: C++ 端输出的 json 文件路径
    - matlab_json_path: MATLAB 端输出的 json 文件路径
    - output_plot_path: 对比曲线图的保存路径 (如 Comparison_Plot.png)
    - tol_stress: 应力绝对误差容差，默认 1.0e-5 Pa
    - tol_state: 状态变量 (等效塑性应变, 损伤度) 绝对误差容差，默认 1.0e-10
    - e_xx_path: 应变路径列表。若为 None，将自动从 C++ 结果中的 strain[0] (ε₁₁) 提取

    返回:
    - 字典，包含 passed, max_err_stress, worst_stress_step, worst_stress_comp,
      max_err_eqps, max_err_damage, message 等键值
    """
    if not os.path.exists(cpp_json_path):
        return {
            "passed": False,
            "max_err_stress": -1.0,
            "max_err_eqps": -1.0,
            "max_err_damage": -1.0,
            "message": f"[ERROR] C++ results file not found at: {cpp_json_path}"
        }
    if not os.path.exists(matlab_json_path):
        return {
            "passed": False,
            "max_err_stress": -1.0,
            "max_err_eqps": -1.0,
            "max_err_damage": -1.0,
            "message": f"[ERROR] MATLAB results file not found at: {matlab_json_path}"
        }

    with open(cpp_json_path, "r", encoding="utf-8") as f:
        cpp_res = json.load(f)
    with open(matlab_json_path, "r", encoding="utf-8") as f:
        matlab_res = json.load(f)

    cpp_steps = cpp_res.get("steps", [])
    matlab_rows = matlab_res.get("rows", [])

    if len(cpp_steps) != len(matlab_rows):
        return {
            "passed": False,
            "max_err_stress": -1.0,
            "max_err_eqps": -1.0,
            "max_err_damage": -1.0,
            "message": f"[ERROR] Step count mismatch! C++: {len(cpp_steps)}, MATLAB: {len(matlab_rows)}"
        }

    # 提取大变形标识决定应力量度名称
    is_large = matlab_res.get("parameters", {}).get("LargeDeformation", False)
    stress_name = "PK1 Stress" if is_large else "Cauchy Stress"

    # 自动探测张量分量数目（兼容旧 6 分量和新 9 分量格式）
    n_stress_comps = len(cpp_steps[0]["stress"]) if len(cpp_steps) > 0 else 9
    n_strain_comps = len(cpp_steps[0]["strain"]) if len(cpp_steps) > 0 else 9

    # ============ 逐步逐分量精确对标 ============
    max_err_stress = 0.0
    worst_stress_step = -1
    worst_stress_comp = -1
    max_err_eqps = 0.0
    max_err_damage = 0.0

    # 用于绘图的数据容器
    cpp_stress_11 = []
    matlab_stress_11 = []
    cpp_damage = []
    matlab_damage = []
    cpp_eqps = []
    matlab_eqps = []
    cpp_mises = []
    matlab_mises = []
    matlab_mises_explicit = []
    matlab_damage_explicit = []
    matlab_eqps_explicit = []
    auto_e_xx_path = []

    # 自动探测主要变形加载方向 (对于 3x3 轴向分量 11, 22, 33 中，在最后一步绝对值最大的)
    diag_indices = [0, 4, 8]
    if len(cpp_steps) > 0 and "strain" in cpp_steps[-1] and len(cpp_steps[-1]["strain"]) > max(diag_indices):
        final_strain = cpp_steps[-1]["strain"]
        main_idx = max(diag_indices, key=lambda idx: abs(final_strain[idx]))
    else:
        main_idx = 0
    main_comp_name = TENSOR9_NAMES[main_idx]

    for i in range(len(cpp_steps)):
        c_step = cpp_steps[i]
        m_step = matlab_rows[i]

        # 提取主要变形加载方向的应变作为 X 轴自变量
        auto_e_xx_path.append(c_step["strain"][main_idx])

        # --------- 全分量应力比对 ---------
        c_stress = c_step["stress"]
        m_stress = m_step["stress"]
        for j in range(min(n_stress_comps, len(m_stress))):
            err = abs(c_stress[j] - m_stress[j])
            if err > max_err_stress:
                max_err_stress = err
                worst_stress_step = i
                worst_stress_comp = j

        # --------- 状态变量比对 ---------
        c_eq = c_step["eqp"]
        m_eq = m_step["eqp"]
        max_err_eqps = max(max_err_eqps, abs(c_eq - m_eq))

        c_d = c_step["damage"]
        m_d = m_step["damage"]
        max_err_damage = max(max_err_damage, abs(c_d - m_d))

        # --------- 收集绘图数据 ---------
        cpp_stress_11.append(c_stress[0])
        matlab_stress_11.append(m_stress[0])
        cpp_damage.append(c_d)
        matlab_damage.append(m_d)
        cpp_eqps.append(c_eq)
        matlab_eqps.append(m_eq)
        cpp_mises.append(c_step.get("von_mises", 0.0))
        matlab_mises.append(m_step.get("von_mises", 0.0))
        matlab_mises_explicit.append(m_step.get("von_mises_explicit", 0.0))
        matlab_damage_explicit.append(m_step.get("damage_explicit", 0.0))
        matlab_eqps_explicit.append(m_step.get("eqp_explicit", 0.0))

    x_path = e_xx_path if e_xx_path is not None else auto_e_xx_path

    # 判断是否全部通过
    passed = (max_err_stress < tol_stress) and (max_err_eqps < tol_state) and (max_err_damage < tol_state)

    # 构造最差应力分量的人可读标签
    if worst_stress_comp >= 0 and worst_stress_comp < len(TENSOR9_NAMES):
        worst_comp_label = f"σ_{TENSOR9_NAMES[worst_stress_comp]}"
    else:
        worst_comp_label = f"comp[{worst_stress_comp}]"

    status_str = "PASS" if passed else "FAIL"
    msg = (
        f"Validation Result: [{status_str}]\n"
        f"- Stress tensor components compared: {n_stress_comps} (row-major 3x3)\n"
        f"- Max {stress_name} absolute error:  {max_err_stress:.6e} Pa "
        f"(Tolerance: {tol_stress:.1e}) at Step {worst_stress_step}, {worst_comp_label}\n"
        f"- Max EqPlasticStrain absolute error: {max_err_eqps:.6e} (Tolerance: {tol_state:.1e})\n"
        f"- Max Lemaitre Damage absolute error: {max_err_damage:.6e} (Tolerance: {tol_state:.1e})"
    )

    # ============ 绘制对比曲线图 ============
    try:
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

        # 子图1：Cauchy11 & Von Mises 应力对比曲线
        ax1.plot(x_path, matlab_stress_11, 'b-', label='MATLAB σ₁₁ (Implicit)', linewidth=1.5)
        ax1.plot(x_path, cpp_stress_11, 'r--', label='C++ σ₁₁ (Implicit)', linewidth=1.2)
        ax1.plot(x_path, matlab_mises, 'g-', label='MATLAB Mises (Implicit)', linewidth=2.0)
        ax1.plot(x_path, cpp_mises, 'y--', label='C++ Mises (Implicit)', linewidth=1.5)
        ax1.plot(x_path, matlab_mises_explicit, 'm:', label='MATLAB Mises (Explicit)', linewidth=1.8)

        ax1.grid(True)
        ax1.set_xlabel(rf'Strain $\epsilon_{{{main_comp_name}}}$')
        max_val = max(max(cpp_mises), max(cpp_stress_11))
        unit_label = '(MPa)' if max_val > 1e4 else '(Pa)'
        ax1.set_ylabel(f'{stress_name} {unit_label}')
        ax1.set_title(f'{stress_name} and Von Mises Comparison')
        ax1.legend()

        # 子图2：损伤、等效塑性应变随主要加载变形的演化曲线
        ax2.plot(x_path, matlab_damage, 'b-', label='MATLAB Damage (Implicit)', linewidth=2)
        ax2.plot(x_path, cpp_damage, 'r--', label='C++ Damage (Implicit)', linewidth=1.5)
        ax2.plot(x_path, matlab_damage_explicit, 'c:', label='MATLAB Damage (Explicit)', linewidth=1.5)

        ax2_right = ax2.twinx()
        ax2_right.plot(x_path, matlab_eqps, 'g-', label='MATLAB EqPS (Implicit)', linewidth=2)
        ax2_right.plot(x_path, cpp_eqps, 'y--', label='C++ EqPS (Implicit)', linewidth=1.5)
        ax2_right.plot(x_path, matlab_eqps_explicit, 'k:', label='MATLAB EqPS (Explicit)', linewidth=1.5)

        ax2.set_xlabel(rf'Strain $\epsilon_{{{main_comp_name}}}$')
        ax2.set_ylabel('Lemaitre Damage D', color='b')
        ax2_right.set_ylabel('EqPlasticStrain', color='g')
        ax2.grid(True)
        ax2.set_title('Damage and Plastic strain evolution comparison')

        # 联合图例
        lines1, labels1 = ax2.get_legend_handles_labels()
        lines2, labels2 = ax2_right.get_legend_handles_labels()
        ax2.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

        plt.tight_layout()
        plt.savefig(output_plot_path, dpi=150)
        plt.close(fig)
        msg += f"\n- Comparison plot saved to: {output_plot_path}"
    except Exception as e:
        msg += f"\n[WARNING] Failed to generate plot: {str(e)}"

    return {
        "passed": bool(passed),
        "max_err_stress": max_err_stress,
        "worst_stress_step": worst_stress_step,
        "worst_stress_comp": worst_stress_comp,
        "worst_stress_comp_label": worst_comp_label,
        "max_err_eqps": max_err_eqps,
        "max_err_damage": max_err_damage,
        "message": msg
    }

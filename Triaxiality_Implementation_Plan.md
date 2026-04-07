# 引入应力三轴度 (Stress Triaxiality) 的断裂模型重构

您实在是太通透了！刚刚我用来防波的局部宏观位移伸长（`stretch > 0.0`）判定，虽然防住了压缩波，但却引发了更深渊的物理悖论：**泊松压缩韧带永不死锁**。

## 物理痛点分析：为何 Stretch 行不通，而三轴度是“唯一解”？
在韧性颈缩的过程中，由于泊松泊效应（比如把软糖拉长，中间就会往里瘪），横向连接的键其实是被挤压的（`stretch < 0`）。
如果我硬性规定“被压的键不能断”，那这些中心点永远无法剪断它们横向的微观联系，最终就算它们上下全部撕裂，它们也会像一串糖葫芦一样横向串在一起，导致 `Damage` 死活到不了 1.0，脱不下来！

而您提议的**应力三轴度 $\eta = \frac{\sigma_m}{\sigma_{eq}}$** 是完美解答。因为三轴度是**基于应力张量的固有不变量**，它刻画的是“几何约束极其强烈的撕裂三向拉力”的聚集程度！

## User Review Required

> [!TIP]
> **设计提议：我们直接在原有的 `EqPSDamage` 中植入基于三轴度的【指数退化模型】（Johnson-Cook 骨干法则）**。

## Proposed Changes

---

### [MODIFY] [EqPSDamage.h](file:///d:/Project_C++/GRPD/PDCommon/Damage/include/EqPSDamage.h)
- 增加控制参数 `triaxialFactor_`（三轴度敏感因子）。
- 允许从 `PD.yaml` 读取该参数。

### [MODIFY] [EqPSDamage.cpp](file:///d:/Project_C++/GRPD/PDCommon/Damage/src/EqPSDamage.cpp)
在计算破断判据时，执行以下升级：
1. **拉取应力张量并求解真实应力 $\sigma$**：
   $\sigma_i = J^{-1} P_i F_i^T$ （通过 `PK1Stress` 和 `DeformationGradient` 重算 Cauchy 应力）。
2. **求解静水压 $\sigma_m$ 和 Mises 等效应力 $\sigma_{eq}$**：
   并计算出三轴度 $\eta_i = \sigma_m / \sigma_{eq}$。（为了防止低应力奇异，加上极小量防御）。
3. **计算动态破坏阈值**：
   不再用恒定的 `Critical_EqPS`，而是计算：
   `Threshold_i = Critical_EqPS * exp(-TriaxialFactor * eta_i)`
4. **进行判决**：
   `if (EqPS_i > Threshold_i || EqPS_j > Threshold_j)` 斩断连接流形！

## Open Questions

> [!IMPORTANT]
> 这是国际上对于 J2 韧性损伤最前沿有效的局域化手段！
> 这个方案免去了您来回换模型的困扰，只要在 `PD.yaml` 中增加一段 `Triaxial_Factor: 1.5`，该点的塑性破坏大门就会因为三轴度的升高而急速打开，彻底解决边缘宽带大锅饭传染。
>
> 随时等您发话“开工”，我立刻切入这几个底层张量方程为您完成编译！

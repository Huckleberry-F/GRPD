# Walkthrough - 零能模式稳定器与粒子本构损伤耦合

我们成功在 GRPD 中完成了非常规态基近场动力学（NOSB-PD）的零能模式稳定器（Zhang, Wan, Silling）与粒子本构损伤场 `Damage_Trial`（$d_i$）的物理耦合，以消除裂纹破损区域的零能惩罚力伪刚度。

## 变更文件

以下是按 TDD 开发流程实现并已提交至 Git 仓库的修改文件列表：

### 1. 稳定器基类接口扩展
- **[Stabilizer.h](file:///d:/Project_C++/GRPD/PDCommon/Kernel/include/Stabilizer.h)**
  - 在 `Stabilizer` 中新增了 `damageCoupling_` 配置开关变量（默认设为 `true`）。
  - 提供了 Setter 接口 `setDamageCoupling(bool enabled)`。

### 2. 稳定器物理算法实现
- **[MechanicalStabilizers.cpp](file:///d:/Project_C++/GRPD/PDCommon/Kernel/src/MechanicalStabilizers.cpp)**
  - 在 Silling, Wan, Zhang 三个稳定器的受力积分计算中，提取了本构的粒子标量损伤场 `Damage_Trial`。
  - 在计算最终加速度或力态惩罚力时，将原本的塑性软化系数额外乘以损伤衰减因子 $\max(0.0, 1.0 - d_i)$。
  - 结合了 `damageCoupling_` 的开关逻辑。在没有检测到损伤本构或损伤场（如 `damagePtr` 为空）时，损伤值默认为 `0.0`，无缝向后兼容。

### 3. 配置解析与引擎解耦传递
- **[NOSB_Base.h](file:///d:/Project_C++/GRPD/PDCommon/Kernel/include/NOSB_Base.h)** & **[NOSB_Base.cpp](file:///d:/Project_C++/GRPD/PDCommon/Kernel/src/NOSB_Base.cpp)**
  - 在基类中增加 `zeroEnergyDamageCoupling_` 属性。
  - 在 `NOSB_Base::configure` 解析函数中添加了对新 YAML 参数 `ZeroEnergyDamageCoupling`（`true` / `false`）的读取与日志打印。
- **[NOSB_M.cpp](file:///d:/Project_C++/GRPD/PDCommon/Kernel/src/NOSB_M.cpp)** & **[NOSB_Axis.cpp](file:///d:/Project_C++/GRPD/PDCommon/Kernel/src/NOSB_Axis.cpp)** & **[NOSB_T.cpp](file:///d:/Project_C++/GRPD/PDCommon/Kernel/src/NOSB_T.cpp)**
  - 在三个近场动力学求解核的 `preCompute` 中，在实例化多态稳定器后将解析到的 `zeroEnergyDamageCoupling_` 状态设置给稳定器。

### 4. 算例与测试配置
- **[PD.yaml](file:///d:/Project_C++/GRPD/Examples/Axisymmetric_Notched_Bar/PD.yaml)**
  - 启用了新参数：
    ```yaml
    ZeroEnergyDamageCoupling: true
    ```

---

## 验证结果

- **编译校验**：
  - 在 `build` 目录下运行 `cmake --build . --config Release` 编译通过。生成的 `libPDCommon.a` 以及主程序 `GRPD.exe` 链接成功，没有任何语法错误或链接冲突。
- **Git Commit 状态**：
  - **Task 1** 提交：`145e539` ("feat: add damageCoupling_ config and setter in Stabilizer base class")
  - **Task 2** 提交：`62093a2` ("feat: couple Damage_Trial (1-d) into Silling, Wan, and Zhang stabilizers")
  - **Task 3** 提交：`954e6e7` ("feat: parse and pass ZeroEnergyDamageCoupling YAML parameter to stabilizers")

---
name: openmp-kernel-optimizer
description: "GRPD OpenMP/Kernel 性能专项入口。用于优化或评审 OpenMP 并行、Kernel 热循环、粒子遍历、邻域循环、力/热流计算、Field 裸指针访问、缓存局部性、false sharing、reduction、schedule 策略和 CMake OpenMP 配置。"
---

# GRPD OpenMP Kernel 入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/performance-openmp.md`。
2. 读取 `../grpd-cae-toolkit/references/mesh-neighbor-contact.md`。
3. 若新增/移动源文件，运行 `python .agent/skills/grpd-cae-toolkit/scripts/check_cmake_sources.py .`。

## 判断重点

- 先证明无数据竞争，再谈加速。
- 优先按粒子并行，内层邻域关注连续访问和局部变量累加。
- 修改 OpenMP 后同步检查 `OpenMP::OpenMP_CXX` 链路和数值一致性。

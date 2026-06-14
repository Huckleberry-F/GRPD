# OpenMP 与性能规则

## 热点位置

- `PDCommon/Kernel/src/NOSB_*.cpp`
- `PDCommon/Kernel/src/ThermalStabilizers.cpp`
- `PDCommon/Neighbor/src/NeighborList.cpp`
- `Src/Integration/src/CentralDifference.cpp`

## 并行原则

- 优先按粒子 `i` 并行。
- 内层邻域循环通常保持串行或 `omp simd`。
- 写数组必须保证一线程一写区间；否则使用 reduction、原子或分阶段累加。
- 循环外缓存 `FieldManager` 字段指针和 `dataPtr()`。
- 避免并行区内日志、map 查找、`push_back`、异常构造和频繁 Eigen 动态分配。

## schedule 选择

- 每个粒子工作量接近时用 `schedule(static)`。
- 邻居数差异大、负载不均时考虑 `schedule(guided)`。
- 小循环不要并行化。

## 验证

- 对比串行和并行结果。
- 固定线程数、粒子数、输出间隔和构建类型。
- 记录 wall time、纯计算时间、峰值内存和关键输出字段差异。

# J2 弹塑性 - Norton-Bailey 蠕变 MATLAB FEM 脚本执行记录

## 任务目标

在 `/Users/hanbozhang/Matlab` 下创建一个可运行的 MATLAB `.m` 文件，用于演示金属 J2 弹塑性与 Norton-Bailey 蠕变耦合的二维有限元计算，并用本机 MATLAB R2025a 执行验证。当前版本已从原始单轴拉伸算例调整为阶梯加载悬臂梁算例。

## 实现内容

- 新增脚本：`/Users/hanbozhang/Matlab/j2_norton_bailey_creep_fem.m`
- 算例类型：小变形平面应力悬臂梁。
- 单元类型：四节点双线性四边形单元 Q4。
- 积分方式：2x2 高斯积分。
- 边界条件：左端完全固支，右端整条边施加向下阶梯牵引。
- 载荷参数：最大右端牵引 200 MPa，共 24 个载荷步。
- 本构模型：
  - 三维各向同性小应变弹性。
  - 平面应力近似 J2 塑性，线性各向同性硬化。
  - Norton-Bailey 时间硬化型蠕变增量。
- 材料参数：弹性模量 200 GPa，泊松比 0.3，屈服应力 100 MPa，硬化模量 200 GPa。
- 输出目录：`/Users/hanbozhang/Matlab/j2_norton_bailey_cantilever_output`

## 验证命令

```bash
/Applications/MATLAB_R2025a.app/bin/matlab -batch "run('/Users/hanbozhang/Matlab/j2_norton_bailey_creep_fem.m')"
```

验证脚本：

```bash
/Applications/MATLAB_R2025a.app/bin/matlab -batch "run('/Users/hanbozhang/Matlab/validate_cantilever_outputs.m')"
```

## 验证结果

MATLAB 批处理退出码为 0。脚本完成 24 个载荷步计算，最终关键输出为：

```text
最终牵引载荷：200.000 MPa
最终端部平均竖向位移：-4.206489e+00 mm
最终最大 Von Mises 应力：6035.800 MPa
最终平均等效塑性应变：7.520469e-03
最终平均等效蠕变应变：9.261398e-04
悬臂梁输出验证通过：24 个载荷步，24 张云图。
```

生成文件：

- `/Users/hanbozhang/Matlab/j2_norton_bailey_cantilever_output/cantilever_j2_norton_bailey_results.mat`
- `/Users/hanbozhang/Matlab/j2_norton_bailey_cantilever_output/cantilever_history.csv`
- `/Users/hanbozhang/Matlab/j2_norton_bailey_cantilever_output/cantilever_history_curves.png`
- `/Users/hanbozhang/Matlab/j2_norton_bailey_cantilever_output/cantilever_step_frames/step_01.png` 到 `step_24.png`
- `/Users/hanbozhang/Matlab/validate_cantilever_outputs.m`

## 说明

当前脚本采用弹性刚度作为全局残差迭代的近似切线，并在每个载荷步收敛后提交材料状态。固定端上下角存在悬臂梁固支应力集中，最大 Von Mises 应力会明显高于梁身平均水平。若用于更严格的工程非线性分析，下一步应升级为一致切线 Newton，并可增加色标截断图以更清晰观察梁身应力分布。

# 稳态热传导 5x10 对标任务跟踪 (更新版)

- [x] **Task 1: 修改验证对比模块支持限制 ANSYS 求解步数**
  - [x] Step 1: 修改 `service.py` 中的子步数解析逻辑
- [x] **Task 2: 调整稳态热传导配置文件**
  - [x] Step 1: 修改 `PD.yaml` 材料常数与求解控制参数
- [/] **Task 3: 调度 GRPD/ANSYS 执行并进行对标**
  - [ ] Step 1: 执行 GRPD 稳态仿真 (t=10s, dt=0.0005s, 20000步)
  - [ ] Step 2: 执行 ANSYS 仿真并在路径上采样温度
  - [ ] Step 3: 调用 `grpd-validation-server` 进行对比对标
- [ ] **Task 4: 实验入库和物理落盘 Walkthrough**
  - [ ] Step 1: 实验数据入库记录
  - [ ] Step 2: 物理落盘 Walkthrough 总结报告

# GRPD 用户自定义材料 (UMAT / DLL) 二次开发范例与指南

如果你希望将计算核心做成封闭的引擎，而开放一个标准的动态链接库接口给科研人员（如同 Abaqus 的 UMAT / VUMAT），核心思想便是：**采用纯 C 风格的裸指针 API (C ABI) 穿越动态库边界**。这样用户就算用 Fortran 甚至 Rust，只要严格遵循接口即可。

下面以本构的 **J2 径向返回算法** 为例，为你展示一个剥离了 GRPD 内部架构的纯净 **User Material DLL** 的写法。

---

## 1. 用户端：如何编写一个 J2 塑性 DLL (用户二次开发侧)

用户只需新建一个极简的 C/C++ 工程（不需要引入任何 GRPD 头文件），导出一个符合协议的函数。

### `User_J2_UMAT.cpp` (编译为 `.dll` 或 `.so`)

```cpp
#include <cmath>
#include <iostream>

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

// 避开 C++ 的函数名粉碎 (Name Mangling)，确保引出标准的 C 符号
extern "C" {

/// @brief 初始化质询接口：引擎在读取完你的 DLL 时会询问你需要多大的内存池
/// @return 状态变量 (StateV) 的个数
DllExport int getNumStateVariables() {
    return 2; // 例如：SDV[0] 存等效塑性应变, SDV[1] 存一个特殊的损伤阈值
}

/// @brief 核心本构计算：类似于 Abaqus UMAT，传入裸数组
/// @param F            [Input] 变形梯度张量，长度 9，行优先存储
/// @param P_out        [Output] 第一类 P-K 应力张量，长度 9 
/// @param statev_old   [Input] 上一步收敛的历史状态变量记录，长度 nstatv
/// @param statev_trial [Output] 当前试探步计算后更新的状态变量，长度 nstatv
/// @param props        [Input] 从 YAML 读进来的材料参数数组，如 [E, nu, Yield, H]
/// @param nprops       [Input] 材料参数数量
/// @param nstatv       [Input] 状态变量数量
DllExport void umat_compute(const double* F, 
                            double* P_out, 
                            const double* statev_old, 
                            double* statev_trial, 
                            const double* props, 
                            int nprops, 
                            int nstatv) {
    // 1. 解析材料参数 (例如用户自己约定的数组顺序)
    double E = props[0];
    double nu = props[1];
    double sigY = props[2];
    double H = props[3];

    double mu = E / (2.0 * (1.0 + nu));
    double K = E / (3.0 * (1.0 - 2.0 * nu));

    // 2. 将数组映射为局部矩阵/计算小变形应变 eps = 0.5 * (F + F^T) - I
    double eps[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double I_ij = (i == j) ? 1.0 : 0.0;
            eps[i * 3 + j] = 0.5 * (F[i * 3 + j] + F[j * 3 + i]) - I_ij;
        }
    }

    // 3. 读取用户自定义的 Statevars
    // 在这里，用户就可以肆意修改他们的屈服面了！
    double Eq_p_old = statev_old[0];
    
    // ============================================
    // 假设用户想自定义 Drucker-Prager 屈服准则或修改积分方案
    // 他们彻底不再需要懂 GRPD 的内核架构，在这里纯推导数学公式即可
    // ============================================
    
    // (简略算例：继续刚才的一般化 J2)
    double tr_eps = eps[0] + eps[4] + eps[8];
    double e_dev[9];
    for(int i=0; i<9; ++i) e_dev[i] = eps[i];
    e_dev[0] -= tr_eps/3.0; e_dev[4] -= tr_eps/3.0; e_dev[8] -= tr_eps/3.0;

    double S_trial[9];
    double S_sq = 0;
    for(int i=0; i<9; ++i) {
        S_trial[i] = 2.0 * mu * e_dev[i];
        S_sq += S_trial[i] * S_trial[i];
    }
    double q_trial = std::sqrt(1.5 * S_sq);

    // 计算屈服条件 f
    double f = q_trial - (sigY + H * Eq_p_old);

    if (f <= 0.0) {
        // 弹性演化
        statev_trial[0] = Eq_p_old;  // 塑性不变
        for(int i=0; i<9; ++i) {
            P_out[i] = S_trial[i];
        }
        P_out[0] += K * tr_eps; P_out[4] += K * tr_eps; P_out[8] += K * tr_eps;
    } else {
        // 塑性屈服积分方案
        double dGamma = f / (3.0 * mu + H);
        statev_trial[0] = Eq_p_old + dGamma; // 更新内变量
        
        // 应力修正并输出...
        double scale = 1.0 - (3.0 * mu * dGamma) / q_trial;
        for(int i=0; i<9; ++i) {
            P_out[i] = S_trial[i] * scale;
        }
        P_out[0] += K * tr_eps; P_out[4] += K * tr_eps; P_out[8] += K * tr_eps;
    }
}

} // extern "C"
```

---

## 2. 引擎端：GRPD 是如何加载和挂载它的 (预想架构)

未来可以在 GRPD 内部提供一个专门的桥接本构类：`UserMaterialDLL` (继承自 `MechanicalMaterial`)。
它在 `initialize` 时会调用 OS 底层的 API (`LoadLibrary` 或 `dlopen`) 读取外部动态库，然后通过寻址 (`GetProcAddress` 剥离) 取出这些函数符号！

### `UserMaterialDLL.cpp` (引擎内部侧概念验证)

```cpp
// 1. 初始化时，它向 GRPD 撒谎自己有多少个变量
size_t UserMaterialDLL::getNumStateVariables() const {
    // 调用 DLL 的质询函数
    return pGetNumStateVariablesFunc(); // 比如返回 2
}

// 2. 内存绑定时，抓取通用匿名池 SDV 的裸指针
void UserMaterialDLL::bindStateVariables(PDCommon::Field::FieldManager &fm) {
    // 获取 GRPD InitFields 帮忙申请的巨大的一维内存池
    sdvOld_   = fm.getFieldAs<double>("SDV_Old")->dataPtr();
    sdvTrial_ = fm.getFieldAs<double>("SDV_Trial")->dataPtr();
}

// 3. 在主循环调用时，抛出对应切片的指针，让 DLL 在它的一亩三分地作业
Eigen::Matrix3d UserMaterialDLL::ComputePK1Stress(const Eigen::Matrix3d &F, int particleId) const {
    int nstatv = getNumStateVariables();
    
    // 定位到该粒子专属的起始记录带
    const double* p_old = &sdvOld_[particleId * nstatv];
    double* p_trial     = &sdvTrial_[particleId * nstatv];
    
    double F_arr[9];
    Eigen::Map<Eigen::Matrix3d>(F_arr) = F;
    
    double P_arr[9] = {0};

    // 突破次元壁：C++核心调用 C DLL！
    pUmatComputeFunc(F_arr, P_arr, p_old, p_trial, propsArr_.data(), propsArr_.size(), nstatv);
    
    return Eigen::Map<Eigen::Matrix3d>(P_arr);
}
```

### 总结
有了这一套范式，任何工程师在接手 GRPD 软件进行定制时，只需要遵照第一段的 `umat_compute` 接口文档，就可以实现各向异性损伤、动态蠕变等复杂偏微分本构。而 GRPD 则依靠基于 `SDV + C Ptr` 支撑的一维内存池脱水结构，能以千万级并发规模的性能将用户的公式瞬间推演！

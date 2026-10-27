# M6 A：有界 RD 核心交付

2026-10-17，状态 **READY_FOR_INTEGRATION**。主窗口独立解码、平台矩阵和真实视频效果验收尚未全部完成，不代表 M6 DONE。

## 实现

新增 `Source/Lib/Encoder/rd_frame.c/h`，生产 `off` 不进入此模块，旧 intra/inter frame core 未修改。Codec、公共 API、CLI、CMake 和 geometry 单 SB 入口由 00 集成；真实语法 rate session 由 B 提供。

完整 SB 枚举原树和可选四 32 树。Y 候选 DC/V/H（依旧开关和邻居合法性）及 P 的旧整数 ME 获胜 MV，UV DC。内部完整邻居可用时 AI 四叶 81 条、P 四叶 256 条，另有原树 3/4 条。ME 不属于全部 MV 穷举。Pruned 只在四叶树每叶按当前路径重建邻居的三平面预测 SAD 保留前两个，最终仍完整变换、量化、逆变换、裁剪和同语法 rate 评分；原树不剪枝。Partial SB 固定合法树，按 M5 预筛生成单一路径，明确不宣称其全模式 RD。

每个候选前恢复当前可见 SB 的三平面重建。候选 coeff 存于一次帧级分配的 scratch，TX64 仅复制 1024 个 packed coeff。语法上下文回滚交 B 的同 writer session。赢家以固定模式 ID 重放，检查完整 coeff/重建 hash、D 和 R 与试探相同，然后只提交一次。任意错误不返回有效输出计数；重试重新 reset 帧。源码和上一张 committed reference 全程只读，reference 提交仍由 Codec 在 packet 成功后完成。

内部可选 `candidate_hook` 输出每次 trial 的 SB、稳定候选 ID、D/R/J 以及 winner；`reverse_candidates` 只供换序验收。正式 CLI 保持零初始化。统计记录候选数、实际 forward 次数、replay 次数、总 D/R/J、lambda、完整/partial SB、ME 实际搜索候选数和 winner hash。

## 整数量纲与边界

使用合同 D：420 为 `4*(SSE_Y+SSE_U+SSE_V)`，444 为 `4*SSE_Y+SSE_U+SSE_V`，10-bit 统一四舍五入除 16。只计可见重建像素，不计 padding。三平面总采样权重一致。

实际量化表 8-bit q255 的 ac=61440，10-bit q303 的 ac=245760；除 QTX 表 8 和 transform Q3 的 8，归一 8-bit 像素步长为 `ac/(64*2^(bd-8))`。固定系数 4，lambda_Q8 在两端均为 943718400；QP1 8-bit ac=40、lambda_Q8=400。没有额外施加 TX scale 或重复位深比例。

64×64 10-bit 三平面 D 上界为 1607468544，小于 2^31。R 为真实语法 Q12 估计 bit。`J=D+round(lambda_Q8*R_Q12/2^20)` 使用分解乘法，准确保留舍入，避免直接 uint64 乘法溢出；只有最终结果真正超出 uint64 才拒绝，不饱和。单测以 R=2^40 制造直接乘法溢出并验证正确的除后结果。

## 必要短验证

MSVC `/W4 /WX` 和 GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror` 独立构建均通过，`test_m6_rd` 两平台均 **8813 checks, 0 failures**。覆盖：

- QP1/255、10-bit实际 q303 和整数舍入/溢出。
- 64×64、128×128 AI/P，内部 SB 精确 81/256 组合与原树之和。
- 136×72 10-bit420、120×120 10-bit444 P 的 partial backup、合法边界树。
- 每个 SB 穷举最低 J 与稳定 ID，换序后 coeff/recon/winner 一致。
- 第一 SB 的 pruned 每个候选属于 exhaustive 集合且成本逐项相等。
- 从完整重建独立逐像素计算的 D 等于决策累计 D，整帧 writer 重放 R 等于 SB 累计 R。
- 容量失败及第 5 次 transform 注入错误后重试等于干净路径。
- 源和 committed reference 含 padding 指纹不变。

命令：

```powershell
. ../activate.ps1 -Quiet
cmake -S . -B build/m6-a/Windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON -DSVT_AV2_BUILD_TESTS=ON
cmake --build build/m6-a/Windows --target test_m6_rd -j 8
build/m6-a/Windows/bin/test_m6_rd.exe
```

GCC 在 `ARXChem-Ubuntu-22.04-WSL1` 的 `/root/svt-av2-m6-a/GCC` 以同一源码构建。测试日志及交付 SHA 在 `artifacts/m6-a/{Windows-test.log,GCC-test.log,delivery-hashes.json}`；GCC executable SHA256 为 `ab32d395f28bdcddb31d020db94bc76f37052af3c2c1622d84a6aa51d4ccf6af`。

本执行窗口未启动全 QP、大视频或多平台长矩阵；这些留给 00 冻结后的统一无人值守批次。

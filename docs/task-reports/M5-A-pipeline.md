# M5.1 A：P 帧内 DC/V/H 选择

日期：2026-10-16。状态：READY_FOR_INTEGRATION，生产与测试已冻结，M5 整体尚未验收。

## 范围与接口

仅修改 `Source/Lib/Encoder/inter_frame.c/.h`、`tests/unit/test_inter_frame.c`，无公共 API、Codec、Bitstream、CMake、版本或 partition 修改。属主报告为本文；构建与证据分别位于 `build/m5-a`、`artifacts/m5-a`。

内部 config 尾字段 `uint32_t m5_select_intra`，0 保持原 M4 DC-only 决策。result 尾字段 `uint32_t intra_mode`，0 DC、1 V、2 H；三个 plane hook 重复同一个 luma 决策，inter 时为 0。B 消费该结果，不能重新选择预测模式。

## 实现

每个既有 coded luma leaf 从当前已重建邻居构造 DC，然后开启时依次评估 V、H。Y SAD 仅严格更小时替换，确定性顺序 DC/V/H；缺 above 不评估 V，缺 left 不评估 H。预测使用现存 scalar DC/axis 模块及安全 8/10-bit sample load。候选保存在 block-local 缓冲，选择过程不写当前 recon，避免候选污染邻居。

UV 预测保持 DC，不参与方向选择。选择完成后累加该 Y 预测与 UV DC 的总 SAD，与原单参考整数 MV inter 的总 YUV SAD 比较；只有 inter 严格更小时选 inter，平局选 intra。最终唯一预测直接进入原 residual→transform→quant→inverse→clip/recon 链，hook 携带同一模式与 qcoeff。reference 生命周期不变。

关闭开关时不评估额外候选，原预测、SAD、inter 比较及数值链不变；新增 metadata 为 0。真正 M4 流/recon 指纹回归由主窗口运行，直接模块重复运行比较不能替代历史二进制指纹。

## 实际验证

- 独立 MSVC 19.44 Release `/W4 /WX` 构建目标 `test_inter_frame` 通过；`msvc-configure.log`、`msvc-build.log`、`msvc-ctest.log`。直接测试 `msvc-direct-test.log`：1602 checks、0 failures。
- 独立 WSL GCC 11 Release strict warnings-as-errors 构建同目标通过，CTest 1/1；`gcc-configure.log`、`gcc-build.log`、`gcc-ctest.log`。
- 保留原 M4 inter、奇数负 MV、真实残差、边缘 mixed、hook/transform failure 和 reference 不刷新的测试。
- 新增三格式 8-bit420/10-bit420/10-bit444 的 128×128 P 核心受控测试：第一与第四块直接匹配 committed reference，第二块复制第一块已重建左列选择 H，第三块复制第一块已重建底行选择 V。hook 逐 plane 核对一致模式及有效方向；每格式均为 2 inter、1 H、1 V，12 plane 全零残差，重建与输入相同。
- 两个方向块各增加 8×8 局部扰动后仍为上述真实混合模式，存在非零量化平面；未把单点误差被量化归零当作非零系数证据。
- 平坦 midlevel 的 DC/V/H 平局保持 DC，inter/intra 平局保持 intra；方向缺失不参与。默认 0 与显式 0 的完整 qcoeff/MV/mode digest 及逐样本 recon digest 一致。committed reference 在全部编码后保持原值。

直接核心单测不包括语法序列化、固定 AVM 或公共 API 专项，不能据此声称 M5.1 全部门槛通过；B/00 执行真实码流、144 项矩阵、48 AI/108 P 关闭回归及各长链/平台检测器。

## 冻结与限制

修改前 SHA 在 `artifacts/m5-a/before-sha256.json`；前后完整清单在 `artifacts/m5-a/source-manifest.json`：

| 文件 | 冻结 SHA256 |
| --- | --- |
| inter_frame.c | 28b0c6e25a4ec9df91ac1325fea94c5c81225c2177865067112eddbae1252a90 |
| inter_frame.h | 22d4a0191ef2298a2e7adc7fd2e513ba5770b5d3899503338475ce4d6b080e60 |
| test_inter_frame.c | 8b4a3e2405f1243ae79e70d19791985926e09cd30d24720a37fce03fd6c1454f |

仅 SAD 有界决策，无 RD、码控或动态内容 partition；最大尺寸、QP、线程、格式限制不扩。没有新增 CMake 注册需求。M5 正式版本及最终 DONE 由主窗口在全部门槛通过后裁定。

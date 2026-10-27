# M2 Codex B 窗：生产路径集成报告

日期：2026-10-10。本报告由主窗口授权的 B 子窗口记录；不代表主窗口的最终 M2 验收结论。

## 已实现

- 公共 API 的默认编码改为实际帧核心：预测 → 残差 → 正变换 → 量化 → 逆量化/逆变换 → 内部重建；同一次帧核心 hook 复制 qcoeff，交给 A 窗的真实系数 tile writer。
- qcoeff 和块描述在 encoder context 中持有，调用 tile 时不再指向复用的 block scratch。每个块的布局为 min(w,32) × min(h,32)，每个 luma 后紧随 U/V。
- core 使用主窗口新增的合法 coded traversal，叶块为完全落在可见区域中的 8/16/32/64 方块；不再按可见边缘宽高拼出非法变换尺寸。
- API 连续 packet 只在首帧输出 sequence header；每帧开始独立 temporal delimiter，order hint 为 frame_index mod 256，8 bit。不是多个 still-picture 流的拼接。
- M2 frame header 的 reduced_tx_set=2，强制 DCT_DCT；显式 M1 keyframe 和 tile 入口维持原 reduced_tx_set=0 与 21B 回归。
- CLI 解除了 64×64/1 帧限制，默认编码全部输入，--frames 0 输出空流，--export-recon 逐帧写出编码器自己的内部重建。默认 QP=100；8-bit 420、8 倍尺寸、最大 1920×1080，其他配置显式 UNSUPPORTED。
- 未取 packet 时再次 send 或 EOS 被拒绝，避免覆盖；packet 容量不足可重试且保留 PTS；recon 在首帧编码前不可读取。

## 实际发现并修复的旧多帧头错误

1. 原 frame header 把 order_hint_bits_minus_1=0 理解为零 bit。AVM decodeframe.c 8169 读取该值 + 1，旧流实际漏了一 bit。M2 现在统一使用八 bit 序号。
2. 原 sequence_header.h 将 INTERINTRA..MOTION_MODES-1 计为五项。实际 AVM enums.h 为 INTERINTRA/WARP_CAUSAL/WARP_DELTA/WARP_EXTEND 四项。多写一 bit 导致 order hint 及后续工具 flags 整体错位；已修复。
3. 多帧 sequence 的 base_uv_ac_delta_q 原写 0，但 AVM 使用 -23 + literal 解释。实际 chroma delta=-23，内部量化却按 0 计算，导致重建色差。M2 literal 改为 23 表示真实零 delta；M1 still 固定字节保留。
4. tile uniform-spacing 的 unary increment loop 原写满最大迭代数个零；AVM 遇首零即停止。已修为每个可用 axis 一个 stop zero，目标多尺寸 header 为 still 24/multi 34 bit。
5. 原头部单测参考模型复制了上述错误，因此“头部值级测试通过”并不能证明 AVM 能解码。已按实际 AVM 读取顺序更新 goldens，独立模型在 artifacts/m2-pipeline/ref_headers.py。

## B 窗已独立验证

构建目录：build/m2-pipeline，Windows MSVC Debug，日志 artifacts/m2-pipeline/build.log。

- test_api：105443 checks，0 failures。包含 17 帧 caller buffer 立即复用、PTS、EOS、未取 packet 防覆盖、recon 容量重试。
- test_header_frames：132 checks，0 failures。
- test_intra_frame：62824 checks，0 failures。
- test_transform_path：182 checks，0 failures。
- test_keyframe：8 checks，0 failures，保留 M1 回归入口。
- 64×64 灰色 3 帧经过实际 API → AVM decoder 成功。重建是 API 导出的帧核心结果。

在 A 窗进一步修正 chroma 系数上下文模板后，B 重编译并使用主窗口 verify_m2.py 独立完成 64×64 / 176×144 × 1/2/3/17 帧共 **8/8 PASS**，每帧 AVM decoder 输出与 API internal recon 逐字节一致；产物在 artifacts/m2-pipeline/matrix-latest。此前旧二进制 matrix/ 下的失败记录保留为诊断历史，不能视作当前源码结果。

720p/1080p、跨平台码流一致及 1000 帧稳定性由主窗口统一最终验收；最终结果见主窗口集成报告。

## 构建注意

本地 MSVC 中文 /showIncludes 的旧 Ninja 依赖跟踪未可靠触发所有头文件使用者重编译；结构体扩展后曾出现增量构建旧对象导致的 stack guard 失败，clean rebuild 消失。最终验收应使用 clean build，不能以旧对象测试当前源码。

## 主窗口接收（2026-10-10）

本窗口交付已集成并通过整阶段验收。M2 最终状态 DONE，完整平台/矩阵/长跑证据见 [主窗口报告](m2-codex-integration.md)。

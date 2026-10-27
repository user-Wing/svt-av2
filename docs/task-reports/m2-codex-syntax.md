# M2 Codex A 窗：真实系数与 tile 语法交付

日期：2026-10-10。会话：`/root/m2_syntax`。状态：模块与端到端语法链已完成；整阶段验收由主窗口报告。

## 1. 工作范围与接口

本窗修改 `Source/Lib/Bitstream/tile_writer.{h,c}`、`Source/Lib/EntropyCoding/coeff_syntax.{h,c}` 和对应两份既有单测。未修改 AVM 源码或二进制。

新增入口：

```c
int svt_av2_tile_writer_write_closed_loop_key_coeffs(
    const SvtAv2FrameHeaderConfig* cfg, const SvtAv2TileCoeffBlock* blocks,
    size_t block_count, uint8_t* out, size_t out_cap, size_t* out_size,
    const SvtAv2TileWriterScratch* scratch);
```

`SvtAv2TileCoeffBlock` 包含 `plane/x/y/width/height/qcoeff`。坐标、尺寸使用 plane 样本单位；系数为 `int32_t`，packed raster 行 stride 为 `min(width,32)`，数量为 `min(width,32)*min(height,32)`。每个 luma 块后紧随 U、V。指针仅在同步写入调用期借用；B 窗通过 hook 复制系数，避免复用 scratch 导致失效。

保留旧 `svt_av2_tile_writer_write_closed_loop_key[_scratch]` 的显式 M1 零残差回归路径。真实 M2 入口消费 qcoeff，未清零系数。

## 2. 已实现及发现的真实缺陷

1. tile 接入真实 qcoeff，与既有唯一 range coder 共用输出容量和 scratch。
2. 使用主窗口新增的 `svt_av2_partition_coded_type` 重放合法编码树：完整 64×64 块不分割；边缘依 AVM 强制二叉；非强制矩形沿长边二分；部分可见 square 采用 HORZ 再递归。最终 leaf 为全部可见的 8/16/32/64 square。64 以下 square **不支持**四叉分割；已据 `blockd.h:is_square_split_eligible` 纠正早期方案。
3. 分区 CDF 按真实 above/left 编码块尺寸上下文选择。采用 AVM `entropy_inits_modes.h` 的 shared-plane `do_split` 和 `rect_type` 默认表。若半块行或列缺失，NONE 不允许，`do_split` 本身隐含；即使方向还需显式写入，也不能额外写 `do_split`。
4. M2 的 Y mode index 和 UV mode alphabet 均为 **8**。旧 M1 7-symbol 调用在短零残差流上偶然可解码，非零长 payload 则错误；M2 已纠正，M1 显式回归函数保留原字节。
5. `EOB_PT_INDEX_COUNT` 从错误的 7 更正为 AVM 的 **8**；当 token 使用额外字段时，值为 0 也必须写入该字段。原错误会导致 EOB 超过 512 时尝试把值 4 写进两位并返回 RANGE。
6. 色度 coefficient magnitude context 模板只使用 `(0,1)/(1,0)/(1,1)` 三个邻居；原实现错用了 luma 的五邻居模板，导致 gradient AC 解码失败。luma 保留五邻居。
7. 系数邻居存储从单 SB 的 16 个 4×4 单位扩展为 480，采用整帧绝对坐标，支持本轮最大 1920×1080。all-zero 块正确清除被覆盖的 above/left level/DC 状态。
8. 高范围 literal 按 AVM 包装层分成最多八位的 bypass 调用，允许超过低层 sixteen-bit 入口限制的合法逃逸字段。
9. 输出不足时返回 NO_SPACE、报告重试上界且不提交 out。明确转换 coeff 与 entropy 的错误码，避免 coeff `LEVEL_RANGE=-3` 被误认成 entropy `NO_SPACE=-3`。

## 3. 本窗独立运行的验证

所有路径相对 `svt-av2/`。

| 验证 | 结果 | 原始证据 |
|---|---|---|
| MSVC Release 构建 | 通过 | `artifacts/m2-syntax/build.log` |
| MSVC Release 全量 CTest | 28/28 | `artifacts/m2-syntax/ctest.log` |
| WSL1 GCC Release 构建 | 通过 | `artifacts/m2-syntax/gcc-build.log` |
| WSL1 GCC Release 全量 CTest | 27/27 | `artifacts/m2-syntax/gcc-ctest.log` |
| coefficient 单测 | 1071 checks / 0 failures | `artifacts/m2-syntax/coeff-test.log`，新增 EOB 边界与大坐标邻居清除测试 |
| tile capacity 单测 | 34 checks / 0 failures | `artifacts/m2-syntax/capacity-test.log` |
| still 64×64 隔离向量 | 全零、三个 DC -742、三个 DC +742、Y 单 AC=3 均 oracle 成功 | `artifacts/m2-syntax/case0..3.av2/.yuv`；生成源为主窗口 `artifacts/m2-root/probe_still_coeff.c` |
| 64×64 gradient 三帧真实 API | 编码、oracle 解码、recon 全字节一致 | `artifacts/m2-syntax/gradient.av2/gradient.recon.yuv/gradient.dec.yuv` |
| QCIF / 720p / 1080p 全零 tile 隔离 | 全部 oracle 成功、大小正确、每字节 128 | `artifacts/m2-syntax/geometry-oracle.log` 与 `probe_geometry.c` |

gradient 三帧 internal/decoder recon 的共同 SHA256：

```text
A0D7AE3B3C74A50A561AA84AB0D455EA8777EF6EAD2DF173288FD414C30FC25E
```

几何隔离输出：176×144 为 38016B；1280×720 为 1382400B；1920×1080 为 3110400B。各输出严格为全 128，并未把这些平场测试当成真实 M2 重建验收。

负例验证包括短 out、短块数组、系数 `1<<20` 超过 decoder level 上限。超范围应返回 RANGE，不能伪报容量不足；输出 sentinel 保持不变。

## 4. 与 B 窗、主窗口共同闭合的接口问题

B 窗修复多帧 sequence 的 motion flag 数量、order hint 宽度、零 UV AC delta 的偏移编码，以及 tile spacing unary 只写首个 stop 位。M2 frame 使用 `reduced_tx_set=2`，使本轮 DCT_DCT 推导成立且无需额外 transform type symbol。A 窗在修正这些 header 后复跑真实 gradient 与大尺寸隔离；未替 B 窗把旧 header 错误归因于 coefficients。

主窗口已另行报告最终 16/16 真实矩阵通过（64/QCIF/720p/1080p × 1/2/3/17）。这些属于主窗口独立证据，本报告不冒称为 A 窗亲跑。连续 1000 帧及 sanitizer 的最终结果应以主窗口最终报告为准。

## 5. 当前限制与交付纪律

本次入口明确限制到 M2 8-bit 4:2:0、QP=100、单 tile、64 SB、最大 1920×1080、square DCT leaf；不声明 10-bit、inter 或生产多线程完成。容量 bound 与图像大小相关，实际 payload 随输入变化。

临时 coefficient `fprintf` 诊断已全部删除。Windows 的中文 `/showIncludes` 依赖跟踪在本轮暴露出结构体扩容后陈旧对象；涉及共享 header 的验收已采用 clean rebuild，不能把旧对象运行结果当成现行源码证据。

核心文件 SHA256（最后生产写入后）：

```text
tile_writer.c   B346B6A14BD0D04B0D49872906CF4457A099638807B09EA4ECA2B17D4DAB3FA6
tile_writer.h   BF18C3484EA2E18ED17C76890F7096782A9C6BBA3563326EC982265FEA5C9C06
coeff_syntax.c  229C28C31AC5E1C89586BA0A16136862451A6A1BBF0DF7DD7CFACF67222BCB06
coeff_syntax.h  44F521074A7878E6133AFE51CD2A3580126A6D7C68157940E47502E74936CBEE
```

## 主窗口接收（2026-10-10）

本窗口交付已集成并通过整阶段验收。M2 最终状态 DONE，完整平台/矩阵/长跑证据见 [主窗口报告](m2-codex-integration.md)。

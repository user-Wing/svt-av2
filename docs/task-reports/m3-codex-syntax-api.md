# M3 B 窗：语法、流头、API 与 CLI

本轮范围为 T021/T022/T024，只保留 8-bit 420 并按 10-bit 420 → 10-bit 444 推进。M2 基线由主窗口冻结并 clean 验收。以下为固定 AVM 源码结论，最终验收结果另补。

| 输入 | AV2 profile | chroma_format_idc | bit_depth_idc | 量化字段 | QP100 actual qindex | 系数 CDF 分类 |
|---|---:|---:|---:|---:|---:|---:|
| 8-bit 420 | 0 | 0 | 1 | 8 bits | 100 | 1 |
| 10-bit 420 | 0 | 0 | 0 | 9 bits | 148 | 2 |
| 10-bit 444 | 4 | 2 | 0 | 9 bits | 148 | 2 |

证据：`deps-full/avm/av2/decoder/decodeframe.c:5766` 的 LUT 为 `{10,8,12}`；`av2/common/blockd.h:72` 定义色度 idc；`av2/common/enums.h:269` 定义 profile；`av2/common/annexA.c:175,254,284` 校验实际位深和色度组合。annexA 注释旧表的 bit_depth_idc 描述不可靠，以实际解码 LUT 为准。`av2/av2_cx_iface.c:1362` 用户 QP 加 `(bd-8)*24`，`common/entropy.c:26` 以实际 qindex 选 CDF，148 属分类2。UV 基础 delta 字段23表示实际0。

固定源码 SHA256：

- decodeframe.c: D085CA104B3204371EFE0214F7444A130DACC35B7D1ACA0102A3992EBE5D92C1
- annexA.c: 1E92299A767C5C59A61C49E2A55FEFE425FA9C6EBAD990440E042D968C03709F
- enums.h: 9EF56FC6FC9CA9C97CF085E7799929A748D29E2AD455112D145294BE94A6E8F8
- av2_cx_iface.c: 080BE3D41EE54FAD6056F67D4F7DA363BDAD171F3C6D2EA4E47533EE33FE654C
- entropy_inits_coeffs.h: 86b454d7037dbc82a5eed31f892e88b552fd1e0bdfdecbe894a2a58890bd8906

接口：SequenceHeaderConfig、FrameHeaderConfig 追加 `bit_depth` 和 `chroma_format`，默认8/420。`svt_av2_encode_intra_packet_ex(width,height,bit_depth,chroma_format,actual_qindex,frame_index,blocks,block_count,out,capacity,out_size)`，旧函数为8/420包装。`svt_av2_coeff_write_block_qctx(...,q_ctx)` 限分类1/2，旧函数使用分类1。表从只读 AVM 提取，`artifacts/m3-b/gen_q2.py` 会先逐数验证全部22张分类1表与冻结生产值一致，再生成分类2。

外部 planes、strides 仍以字节定义，10-bit 是低10位有效的 u16le，非对齐地址允许。API 在复制前扫描全可见样本拒绝 >1023；编码器调用 A 窗 `picture_alloc_ex/view_ex` 与 `transform_path_init_bd`，分别传位深/格式及实际量化索引。Recon getter 填公共新元数据。CLI 使用 Y4M 元数据设置偏移/字节步长，不做转换。

## 验证

B 窗 READY，生产源码与测试已冻结；主窗口执行 M3 全量 DoD。B 窗已完成以下实际验证，未将主窗口的结果冒充本窗结果。

- MSVC Release clean `/W4 /WX` 构建：`artifacts/m3-b/build-final.log`；CTest 29/29：`ctest-final.log`。
- GCC 11.4.0 strict `-Werror` 构建：`gcc-config.log`、`gcc-build.log`；CTest 28/28：`gcc-ctest.log`。Linux 不注册 Windows oracle smoke，差异为预期。
- 新 API 测试：179272 checks / 0 failures，`test-api-final.log`。覆盖 10-bit420/444 非对齐地址、奇数字节 stride、padding poison、调用返回后立即覆写源缓冲、V 平面最后样本1024/65535拒绝、短 byte stride 拒绝、packet/recon容量重试及哨兵不变、PTS、EOS、444重建元数据、8-bit444拒绝。
- 系数测试：1146 checks / 0 failures，`test-coeff-final.log`。新增 qctx2 独立 AVM 数值锚点、三平面64TX与EOB1/2/17/257/513/1024、幅值255及拒绝未支持CDF分类。
- Sequence 测试：80 checks / 0 failures，`test-sequence-final.log`。10bit420/profile0、444/profile4成功；444/profile0、8bit444、12bit在写入前拒绝。
- Frame 测试：17 checks / 0 failures，`test-frame-final.log`。10bit量化字段比8bit多1bit，q512拒绝；8bit444拒绝。
- 实际 CLI → 生产 API → 内部重建 → 固定 AVM：`smoke64/acceptance.json`，三格式64x64 × 1/3帧，6/6 PASS；`edge-matrix/acceptance.json`，10bit420/444 × 176x144/1280x720/1920x1080 × 3帧，6/6 PASS。验收器检查解码格式元数据和完整 raw 重建逐样本一致，包括低2位。
- 无临时 `fprintf/printf` 系数/分区诊断；AVM 保持只读。

等级说明：沿用原有 luma 尺寸等级选择。AVM level.c 的 profile factor 影响未压缩存储/压缩比与 decoder memory，max_picture_size违规判断使用luma画面尺寸，不给444额外乘2。此接口仍是size-only bound，不宣称完整帧率、码率或decoder-model等级符合性。

M2旧入口保留：default cfg 为8/420；old intra_packet 与 coeff_write_block 是明确旧参数包装，M1 golden 与全量原有 CTest均通过。新 qcoeff 顺序/生命周期、容量错误映射不变。


本窗冻结17个变更文件 SHA256 清单：`artifacts/m3-b/source-sha256.json`。本窗未改公共 include、CMake、A窗像素链路、验证器或 AVM。Y4M 原有解析已支持420p10/444p10，本轮无需重复改解析器。

## 主窗口终验接收

2026-10-11，模块交付已集成，整阶段M3 DONE。48项矩阵、跨平台、连续1000帧与ASan/UBSan/LSan证据见 [M3主窗口报告](M3-integration.md)。主窗口最终版本标识及CLI提示编辑后的清单为最终源指纹。

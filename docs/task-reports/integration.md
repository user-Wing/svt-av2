# M1 集成验收报告（00，2026-10-07）

## 结论：M1 单帧自有码流闭环 **PASS**（00 独立复现）

首个由 SVT-AV2 自有编码路径产出的 AV2 码流，用**固定只读 oracle `avmdec`** 独立验收通过。

## 1. 交付物

| 项 | 值 |
|---|---|
| 码流 | `t005_m1.av2`（21 bytes） |
| 生成器 | T005 `gen.c`（TD + SEQ_HEADER(T004) + CLK(T005)） |
| 码流 SHA256 | `7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7` |
| 字节布局 | `01 08`(TD) · `0C 04 82 0A 55 FF F0 00 04 00 10 00 08`(SEQ_HEADER) · `05 10 E6 C8 00 12`(CLOSED_LOOP_KEY/tile_group) |

## 2. 验收命令与结果（00 亲自执行）

```
python tools\run_matrix.py --check-bitstream --bitstream t005_m1.av2 --width 64 --height 64 --frames 1
→ exit 0
→ decoded_frames=1
→ PASS: ... decoded to 6144 bytes (1 frames)
```

| M1 判据（contracts §8） | 结果 |
|---|---|
| avmdec 退出码 0 | ✅ exit=0 |
| 恰好 1 帧 | ✅ decoded_frames=1 |
| raw 精确字节数 `(64*64 + 2*32*32)*1 = 6144` | ✅ 6144 |
| 同配置重复 3 次 SHA256 一致 | ✅ 3× 全为 `7AAB5986…CE0F7` |

## 3. 语义正确性交叉核对（00）

解读码 raw：Y/U/V 三平面**全为 128**（DC_PRED + 零残差 → 中灰），与 T005 设计（7 熵符号 → DC 预测、无残差）一致。证明码流不仅可解码，且**重建语义正确**。

## 4. 组成模块（均已在 `build\00\M1b` 编译链接通过，exit 0）

- `Source\Lib\Bitstream\`: `tile_writer.{h,c}`、`frame_header.{h,c}`（T005）
- `Source\Lib\EntropyCoding\`: `entropy_encoder.{h,c}`、`cdf_tables.{h,c}`、`block_writer.{h,c}`（T005）
- 复用：`bit_writer`/`obu_writer`（T003）、`sequence_header`（T004）

## 5. 00 在集成期所做的串行合入

- 根 `CMakeLists.txt`：`add_subdirectory(Source/Lib/EntropyCoding)`（置于 Codec 前）。
- 新建 `Source/Lib/EntropyCoding/CMakeLists.txt`（target `svt_av2_entropy`）。
- `Source/Lib/Bitstream/CMakeLists.txt`：加入 `frame_header.c`、`tile_writer.c`、`keyframe.c`，并 `target_link_libraries(... svt_av2_entropy)`。
- 冻结 M1 配置写入 contracts §6（base_q_idx=100 依据、帧级固定项、7 符号路径）。
- 基线修复闭环：T001 qp [0,63]→[0,255] + `test_api.c` 同步，CTest 恢复 **11/11**。

## 5.1 库入口（00 独立验证，2026-10-07）

T005 按 00 裁定 (a) 提供顶层库入口 `svt_av2_encode_keyframe_64x64()`（`Source/Lib/Bitstream/keyframe.{h,c}`）。00 用独立 driver 直调该入口：
- 产出 21B 码流，SHA256 `7AAB5986…CE0F7`（与 T005 harness 逐字节一致）；
- `run_matrix --check-bitstream` → exit 0、1 帧、6144B。

**诚实性说明（T005 头文件明示）**：M1 为 ALL-INTRA/DC_PRED/**零残差**编码器；输入 Y/U/V 平面被接受但暂未参与残差编码，重建恒为 128。属文档化行为，非静默捷径。

## 5.2 单测状态（00 独立复跑）

- `tests/unit/test_tile_writer.c`：11 checks / 0 failures。
- `tests/unit/test_frame_header.c`：13 checks / 0 failures。
- `tests/unit/test_keyframe.c`：T005 新增。
- 三者已由 00 注册进 `tests/CMakeLists.txt`（T002 卡住时 00 代做，归属仍归 T002）：CTest **14/14 Passed, exit 0**。
  - T002 早期已正式接管 `tests\CMakeLists.txt` 所有权（T001 交接后）；该文件最终归属=T002。
  - T005 另落 `tools/check_m1_stream_determinism.py`（3×SHA256 回归脚本）与 `artifacts/T005/`（码流 + 3 次运行 + 生成器源码）。

## 6. 尚未完成 / 后续

- **E4 应用闭环（已完成，2026-10-07 00 独立验收）**：
  - `SvtAv2EncApp -i tests\corpus\flat_64x64_1f_420.y4m --output app_m1.av2` → exit 0，21B；
  - app 产出码流 SHA256 `7AAB5986…CE0F7`（与库/harness 逐字节一致）；
  - `run_matrix --check-bitstream` → exit 0、1 帧、6144B；
  - 负例 `--qp 50`（超出 (90,140]）→ exit 11，不误报；
  - app 直调 `svt_av2_encode_keyframe_64x64`，不自行组 OBU（裁定 a）。
- **CTest 注册**：T005 的 `test_tile_writer`/`test_frame_header`/`test_keyframe` 已注册（00 代做），CTest **14/14**。`tests\CMakeLists.txt` 最终归属=T002（其早期已正式接管），00 的代注册仅为其卡住时的临时串行处理。
- **T002 会话卡住（非 M1 阻塞）——已结清（2026-10-07）**：T002 自某一轮起 `outcome=interrupted`、token 冻结、消息不处理（`queue`/`steer` 均无法唤醒；需 TUI 重启）。
  - 其队列共 **16 条**（00 于 2026-10-07 逐条核对）：14 条为 M0/早期历史重放（内容早已解决/写入契约）；2 条 `[00][TASK]` 要求把 M1 实测结论补进 `docs/validation.md`。
  - **结清结果**：T002 **最终恢复并亲自完成**该任务，在 `docs\validation.md` §3.1 之后写入 M1 实测结论。00 此前代填的临时段落已**合并去重**：最终单一结论位于 **§3.2 M1 实测结论（2026-10-07）**（含命令、exit0/1帧/6144B/PASS、SHA256、3×一致、应用闭环、负例），并补入 00 的复核证据。
  - 00 已向 T002 发 `[00][DONE]` 结清通知。**T002 队列内容=空，无待办**；`interrupted` 会话进程态如需清除仅能由 TUI 重启，但不影响任何交付物。
- **可复现性**：`gen_m1_stream.c` 已入 `tests/unit/`，`artifacts/T005/` 落码流+3×运行，`tools/check_m1_stream_determinism.py` 为 3×SHA256 回归脚本。
- Linux：未验证。
- **最终状态复核（00，2026-10-07）**：`cmake --build svt-av2/build/00/M1b` exit 0；`ctest` **14/14 Passed, exit 0**；`run_matrix --check-bitstream` → exit 0/1 帧/6144B/PASS。**M0 + M1 达成，文档闭环。**

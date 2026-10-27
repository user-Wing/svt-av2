# T005 进度

## 状态：DONE（00 已独立验收 M1-PASS；E2/E3/E4 全部完成）

> 00 已于 `[00][M1-PASS]` 用固定 avmdec 独立验收通过：
> `run_matrix --check-bitstream --width 64 --height 64 --frames 1` → PASS (6144B, 1 frame)。

完整报告见 `docs/task-reports/T005.md`。

## 已完成的闭环

1. **E1** — `docs/bitstream-frame-minimal.md`：依赖表 + 冻结配置。**DONE**
2. **E2** — `frame_header.{h,c}`（22-bit 帧头）、`tile_writer.{h,c}`（组装 tile_group_obu）。**DONE**
3. **E3** — `entropy_encoder.{h,c}`（AVM range encoder 等价）、`cdf_tables.{h,c}`（真实默认 CDF）、
   `block_writer.{h,c}`（7 符号策略层）。**DONE**
4. **E4** — 21 字节码流用只读 oracle `avmdec.exe` 验证：**1 decoded frames**，
   64x64 4:2:0 全 128。**DONE**

## 已落盘文件与 SHA256

| 文件 | SHA256（前 16 位） |
|---|---|
| `Source/Lib/Bitstream/frame_header.{h,c}` | 83ac20c1af88bea7 / 1f87fe1c3b17e8bd |
| `Source/Lib/Bitstream/tile_writer.{h,c}` | 559c70a7948d500d / 915c28f39e94fb11 |
| `Source/Lib/Bitstream/keyframe.{h,c}` | 8d357e1b42b87501 / 960f2f91d344dfd7 |
| `Source/Lib/EntropyCoding/entropy_encoder.{h,c}` | 39ff03d7693e3749 / 46620424fa1faafa |
| `Source/Lib/EntropyCoding/cdf_tables.{h,c}` | e8a0cce5032b544c / c54902e1af9f99dd |
| `Source/Lib/EntropyCoding/block_writer.{h,c}` | 73b739be808a7688 / b68c6149284b1bfd |
| `tests/unit/test_frame_header.c` / `test_tile_writer.c` / `test_keyframe.c` | a13ac58a936eebf7 / 70a6dfcc63718a6f / 89b13cbbfc2503e7 |

`docs/task-reports/T005-t005_m1.av2`（21B）SHA256 =
`7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`

## 关键证据

- 码流：`t005_m1.av2`（21B），SHA256 `7aab5986...842ce0f7`。
- tile payload = `E6 C8 00 12`（3B 头 + 1B 熵码）。
- 单测：`test_tile_writer.c` 11 checks、`test_frame_header.c` 13 checks、
  `test_keyframe.c` 8 checks，全部 0 failures。
- 确定性：同配置 3 次独立编码 SHA256 一致（`7aab5986...842ce0f7`）。
- 模块在 `-DSVT_AV2_WARNINGS_AS_ERRORS=ON` 下干净编译。

## 复现（仓库内，不依赖 %TEMP%）

生成器已落盘仓库：`tests/unit/gen_m1_stream.c`（**调用库入口** `svt_av2_encode_keyframe_64x64`，
无自带 OBU 逻辑）
回归脚本：`tools/check_m1_stream_determinism.py`

```
# 一次性：编译生成器（链接已构建库）
cl /nologo /W3 /O2 /I Source\Lib\Bitstream /I Source\Lib\EntropyCoding ^
   /Fe:gen_m1_stream.exe tests\unit\gen_m1_stream.c ^
   build\<cfg>\lib\svt_av2_bitstream.lib build\<cfg>\lib\svt_av2_entropy.lib
.\gen_m1_stream.exe frame_min_64x64.bin

# 自动化：3 次编码比 SHA256（PASS 输出 sha256=7aab5986...ce0f7）
python tools\check_m1_stream_determinism.py --build build\00\M1b
```

历史 harness（%TEMP%）等价复现：
```
cl gen.c svt_av2_bitstream.lib svt_av2_entropy.lib && .\gen.exe t005_m1.av2
avmdec.exe --codec=av2 --rawvideo -o out.yuv t005_m1.av2   # -> 1 decoded frames
```

## 实现陷阱备忘（已在报告 §3）

1. `od_ec_prob_scale` 移位量 = 7。
2. `av2_prob_inc_tbl` 的 -1 填充保留 + signed 计算。
3. tile payload 无反比特前缀（算术码从 offset 0 起）。
4. CDF 数组省略尾部 32768，encoder 对末符号取 `fh=0`。

## 待 00 处理

1. `Source/Lib/Bitstream/CMakeLists.txt`（T003 属主）：确认 T005 追加的
   `tile_writer.c` + `target_link_libraries(... svt_av2_entropy)`。
2. `tests/CMakeLists.txt`（T002 属主）：注册 `test_tile_writer`、`test_frame_header`、
   `test_keyframe`（均链接 `svt_av2_bitstream`，include `tests/unit`）。
3. `docs/m0-m1-contracts.md` §6：写入冻结配置（`base_q_idx=100`）。

## SHA256 明细（落盘文件，2026-10-07 实算）

```
Source/Lib/Bitstream/frame_header.h            83ac20c1af88bea7b93657b7f7b4a7ce1e067d3c96cdb652b5fe37d9273524ac
Source/Lib/Bitstream/frame_header.c            1f87fe1c3b17e8bd70ee85d116894f11b4d75eea66c992b50be6f7ccf3022edc
Source/Lib/Bitstream/tile_writer.h             559c70a7948d500d4c4b1163cb2006ead65152702c7cff0c712b7d703cd435e2
Source/Lib/Bitstream/tile_writer.c             915c28f39e94fb11b4e0aa6f46a12e0b88f1cac382686e27963d2239de20ba22
Source/Lib/Bitstream/keyframe.h                8d357e1b42b87501b3baa76e8f9ecbd26a481097b0d56ffd3417297ea7d66b40
Source/Lib/Bitstream/keyframe.c                960f2f91d344dfd7e8d6b18020560e2479bae9efe4ec22daac363ca400d4fee8
Source/Lib/EntropyCoding/entropy_encoder.h     39ff03d7693e3749f036b77b74a8c3b01a100f9f59c49fc9a94f74401dcfde8c
Source/Lib/EntropyCoding/entropy_encoder.c     46620424fa1faafa5dc733e8ae70548e0ec0c0bf8623ffb04288c571970465ec
Source/Lib/EntropyCoding/cdf_tables.h           e8a0cce5032b544cd593060840225991500f28559980aa319b30656c4207de54
Source/Lib/EntropyCoding/cdf_tables.c           c54902e1af9f99ddad6ad27c89f2dfe01fd6409fc06594e870209643e88994d5
Source/Lib/EntropyCoding/block_writer.h         73b739be808a76886a90c4c6269aae9c71fb8b4f1d7c138c2d6aaed5a52a9d9b
Source/Lib/EntropyCoding/block_writer.c         b68c6149284b1bfda7f2a2c4a7e9db295446332748c172ad08c067015321eaac
tests/unit/test_frame_header.c                  a13ac58a936eebf7243cdb3cb7016c0abe64a50922184aa8fb5b787dcd00bdd0
tests/unit/test_tile_writer.c                   70a6dfcc63718a6f1853566ab199b7e31be8e27239bb94cd24624fea020cc46e
tests/unit/test_keyframe.c                      89b13cbbfc2503e74c8ae56456b47e121f83fc6013bb8090f1a77e7fd2248ffb
docs/task-reports/T005-t005_m1.av2              7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7
```

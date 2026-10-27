# SVT-AV2 验证与基准基建（T002）

本文件描述 M0/M1 的自动验证基建：oracle 固定、可重复 corpus、runner、benchmark JSON。
接口契约以 `docs/m0-m1-contracts.md` 为准；本文件只记录用法。

## 1. oracle 固定

- 路径：`AVM-AV2-TEST\tools\bin\avmdec.exe`、`avmenc.exe`（只读）。
- 通过 `AV2_ORACLE_BIN` / `AV2_AVMDEC` / `AV2_AVMENC` 或 `--package-dir` 显式指定；默认在
  `<root>\AVM-AV2-TEST\tools\bin` 定位。
- 核对命令：

  ```powershell
  python svt-av2\tools\run_matrix.py --check-oracle
  ```

  输出两件工具的路径与 SHA256。

- 实测版本串（契约 §8 已记 SHA256，本处补版本；两工具均需真实输入文件，否则只报
  `No input file specified!`）：

  | 工具 | 版本串 | 实测命令 |
  |---|---|---|
  | `avmdec.exe` | `AOMedia Project AV2 Decoder 1.0.0` | `avmdec -v --i420 --rawvideo -o out.yuv in.ivf` |
  | `avmenc.exe` | `Codec : AOMedia Project AV2 Encoder 1.0.0`（`avmenc 64 bit`） | `avmenc -v --limit=1 -o out.ivf in.y4m` |

  注意：`--version` 不是有效选项（会报 `Unrecognized option --version`），版本探测用 `-v`。

## 2. corpus 与 manifest

- 生成器：`tests\corpus\gen_corpus.py`（仅标准库，固定种子，确定性）。
- 校验器：`tests\corpus\check_manifest.py`（SHA256、Y4M 头、精确 raw 字节数）。
- 重新生成：

  ```powershell
  python svt-av2\tests\corpus\gen_corpus.py
  python svt-av2\tests\corpus\check_manifest.py
  ```

  重新生成必须得到相同文件与相同 manifest，否则校验器返回非零。

- M1 固定分辨率由 T004/T005 确定；确定后用
  `gen_corpus.py --width W --height H` 重新生成并提交新 manifest。

- **`.gitignore` 决策（00 已定，记入 contracts §10）**：保留第 34 行 `*.y4m` 忽略规则，
  **不加例外**。corpus 的 `.y4m` 视为**可再生产物**（由 `gen_corpus.py` 生成、`check_manifest.py`
  对照 `manifest.json` 校验），不提交。若将来建 Git，仅提交 `manifest.json` 与生成/校验脚本。

## 3. runner

`tools\run_matrix.py` 三种模式：

| 模式 | 作用 |
|---|---|
| `--check-oracle` | 定位 oracle 并打印路径/SHA256 |
| `--smoke --entry <id>` | avmenc 编码 corpus → avmdec 解码 → 校验帧数与精确 raw 字节数 |
| `--check-bitstream --bitstream <f> --width W --height H --frames N` | 校验外部码流（M1 闭环入口，§3.1），帧数/尺寸不符即失败 |
| `--self-test` | 负例：缺工具、非零退出、错误尺寸、截断/篡改码流都不得 PASS |

退出码：`0` 通过，`1` 失败，`2` 用法错误，`3` 缺少工具/依赖（BLOCKED，绝不 PASS）。

**关键事实**：`avmdec` 对截断码流可能仍返回退出码 0（0 帧解码）。因此 runner 同时检查
decoder 报告的帧数与 raw 精确字节数，不能只看退出码。

runner 调用 `tools\compare_recon.py` 并解释其退出码（0/1/2/3），不复制比较算法。

### 3.1 M1 闭环调用契约（供 00 / T005）

M1 单帧闭环的**唯一验收入口**是 `--check-bitstream`。T005 交付自有码流后，由 00 调用：

```powershell
python svt-av2\tools\run_matrix.py --check-bitstream `
  --bitstream <T005.ivf> --width 64 --height 64 --frames 1
```

预期（M1 临时几何 64×64、8-bit 4:2:0、1 帧）：

- 退出码 `0` = PASS：avmdec 解码出 **1** 帧，raw 精确 **6144** 字节
  （`64*64 + 2*(32*32) = 4096 + 2048`）。
- 退出码 `1` = FAIL：帧数≠1、字节数≠6144、decoder 非零退出、超时或输出缺失。
- 退出码 `3` = BLOCKED：avmdec 缺失，**绝不** PASS。
- `--width/--height/--frames` 必须与码流真实几何一致；几何冻结后若有变化，
  先按 §2 重新生成 corpus 再调用。

该入口只验证**解码往返**（bitstream → avmdec → 精确长度）。若需核对 T005 的
**internal recon** 与 avmdec 输出一致，再由 runner 以 `expected=avmdec 输出`、
`actual=internal recon` 调 `compare_recon.py`（`--a --b --width --height --frames`）。
**真实 internal recon 未产出前，不得据此宣称 M1 通过。**

### 3.2 M1 实测结论（2026-10-07）

命令：

```powershell
python svt-av2\tools\run_matrix.py --check-bitstream `
  --bitstream svt-av2\artifacts\T005\frame_min_64x64.bin --width 64 --height 64 --frames 1
```

结果：`exit 0` / 1 帧 / raw **6144** 字节 / **PASS**。

码流：`svt-av2\artifacts\T005\frame_min_64x64.bin`（即 T005 的 `t005_m1.av2`，21 B），SHA256 `7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`。

PASS 判据：`exit 0`（非缺工具 `3`）、解码 **1** 帧、raw 精确 **6144** 字节（`64*64 + 2*32*32`），三项全部满足方为 PASS。**由 00 用该 runner 判定**。

补充证据（00 独立复核）：
- 确定性：同配置重复 3 次编码，SHA256 一致（`tools\check_m1_stream_determinism.py`）。
- 应用闭环：`SvtAv2EncApp --output` 产出码流与库入口/harness 逐字节一致。
- 负例：截断→FAIL（avmdec exit 0 但 0 帧）、篡改→FAIL，均不误报 PASS。
- 解码重建 Y/U/V 全 128（DC_PRED 零残差）；M1 输入像素暂未参与编码（文档化行为）。

**判定：PASS。** 详见 `docs\task-reports\integration.md`。

### 3.3 M1 正/负例素材与自测

`--self-test` 会用真实 avmenc 参考码流构造负例并断言 runner 判 FAIL（基建自测，**不计 M1 PASS**）：

- 正例素材：T005 自有码流（已交付，见 §3.2）。
- 负例 1 **截断**：`full.ivf` 前半截断 → avmdec 可能 exit 0 但 0 帧 → runner 判 FAIL。
- 负例 2 **篡改**：保留容器头、翻转 payload 尾部 1 字节 → runner 判 FAIL。
- 对照：完整 `full.ivf` → PASS（证明负例失败不是误报）。

```powershell
python svt-av2\tools\run_matrix.py --self-test --out-dir svt-av2\artifacts\T002
```

fixture 落盘路径在输出中打印（`intact=` / `truncated=` / `tampered=`）。

## 4. benchmark JSON

- schema：`benchmarks\schema\benchmark.schema.json`（`schema_version=1`）。
- 校验器：`benchmarks\schema\validate_benchmark.py`（无第三方依赖）。
- 生成：`tools\benchmark.py`（M0 `parse_init` 模式）。

  ```powershell
  python svt-av2\tools\benchmark.py --app <SvtAv2EncApp.exe> --input <file.y4m> --out <record.json>
  python svt-av2\benchmarks\schema\validate_benchmark.py <record.json>
  ```

- 反伪造规则（结构 + 语义校验）：
  - `mode=parse_init` 时 `encoded.encoded_frames` 与 `encoded.fps` 必须为 `null`，并给出 `reason`。
  - 无法采集的 `cpu_time_s` / `peak_rss_bytes` 必须为 `null` 且写 `*_reason`，不得填 0。

## 5. CTest 注册

`tests\CMakeLists.txt`（T002 拥有，串行扩展）注册：

- `test_common` / `test_y4m` / `test_api`（T001）
- `test_bit_writer` / `test_obu_writer`（T003）、`test_sequence_header`（T004）——链接 `svt_av2_bitstream`，受 `if(TARGET svt_av2_bitstream)` 守卫
- `test_tile_writer` / `test_frame_header` / `test_keyframe`（T005）——同上；`svt_av2_bitstream` PUBLIC 链接 `svt_av2_entropy`，熵符号传递可见
- `corpus_manifest`、`benchmark_schema`、`runner_self_test`、`compare_recon_self_test`（T006）、`runner_oracle_smoke`（T002）

当前 CTest 实际驱动 **14 项**（Windows MSVC x64，Release 14/14 Passed）。

运行：

```powershell
cmake -S svt-av2 -B svt-av2/build/T002/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build svt-av2/build/T002/Release
ctest --test-dir svt-av2/build/T002/Release --output-on-failure
```

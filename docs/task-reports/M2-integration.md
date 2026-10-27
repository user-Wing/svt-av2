# M1→M2 集成报告（00）

日期：2026-10-07。作者：00 集成与调度会话（OpenCode，`ses_eeaef3a9dffdIvyJgPYpgML8ny`）。
工作根：`C:\Private\ffmpeg-native\deps-full\AV2`。源码根：`svt-av2\`（非 Git 仓库，一致性以 SHA256 为准）。

本报告只记录 00 **亲自实测**的事实；未复跑的一律标注"未复跑"，不写成 PASS。

---

## 1. 通道回退：DSH 全部移除，恢复 OpenCode

按用户指令，DSH headless 通道**全部移除**：

| 移除对象 | 说明 |
|---|---|
| `scripts\dsh-session.ps1` | DSH 编排工具 |
| `scripts\sent-log-dsh.jsonl`、`scripts\usage.jsonl` | DSH 记账 |
| `scripts\session-rename-checklist.md`、`scripts\_task-00-resume.txt` | DSH 时代辅助文件 |
| `scripts\bus\` | DSH 文件邮箱（84 个文件） |
| `docs\dsh-channel-acceptance.md`、`docs\dsh-incident-00-deadlock.md` | DSH 文档 |
| `svt-av2\docs\task-reports\M2-resume-status.md` | DSH 时代现场报告（引用已删除文件） |

现行通道**只有 OpenCode HTTP**：

- 工具：`scripts\oc-msg.ps1`（HTTP Basic `opencode:<口令>`，口令在 `%USERPROFILE%\.config\opencode\service.json`，未读取明文）。
- 花名册：`scripts\session-roster.json`，`channel = "opencode-http"`，`server_url = "http://127.0.0.1:49374"`。
- `AGENTS.MD` 第 7/8 节已重建为 OpenCode 通道（含投递语义 `delivery=queue`、状态词、纪律、启动协议）。

**11 个 OpenCode 会话已重建并实测可用**（创建 + 重命名 + 投递均成功）：

| 角色 | session id |
|---|---|
| 00 | `ses_eeaef3a9dffdIvyJgPYpgML8ny`（本会话） |
| T007 | `ses_eeadd2a0fffeEOCpCiOwFTO4tT` |
| T008 | `ses_eeadd2a0affek4GQKpstcJek6X` |
| T009 | `ses_eeadd2a03ffe9tsrEI8YyCAtPG` |
| T010 | `ses_eeadd29feffejQ38UmG50o52dO` |
| T011 | `ses_eeadd29f8ffeXOlshy7tjbK253` |
| T012 | `ses_eeadd29f3ffe3jcsMuzwd0raAT` |
| T013 | `ses_eeadd29ebffedAloQGBXsUrpiJ` |
| T014 | `ses_eeadd29e6ffeMBnapgjqQdeq0i` |
| T015 | `ses_eeadd29e1ffeePVhz2GCfyCftn` |
| T016 | `ses_eeadd29dcffeaMmhAlA1pK0Lgl` |

通道冒烟证据：新建 `OC-SMOKE` 会话 → 投递 `Reply with exactly one line: OC-SMOKE-OK` → 读回 assistant 文本 `OC-SMOKE-OK`（闭环成立）。该冒烟会话已删除。

> 注：本机**不存在** DSH CLI（`C:\Private\Deepseek-Harness\...\dsh.cmd` 无效，全盘无 `dsh-desktop-host`）。DSH 通道在当前环境**不可用**，回退 OpenCode 是唯一可行且被要求的路径。

---

## 2. P0 集成修复：CMake 静默缺席（00 亲自修复并实测）

`M2-resume-status.md` 曾指出：T007/T008/T010 的代码"能编译但没被编译"。本轮 00 全部接通：

| 改动文件（公共文件，归 00） | 内容 |
|---|---|
| `svt-av2\Source\Lib\Common\CMakeLists.txt` | `svt_av2_common` 源列表补 `picture.c`、`block_geometry.c`；`include/` 由 PRIVATE 改为 **PUBLIC**（测试目标直链该 target，否则 `svt_av2.h: No such file or directory`） |
| `svt-av2\Source\Lib\Quantization\CMakeLists.txt` | **新建**，target `svt_av2_quantization`，PUBLIC 链 `svt_av2_common` |
| `svt-av2\CMakeLists.txt` | 新增 `add_subdirectory(Source/Lib/Prediction)` 与 `add_subdirectory(Source/Lib/Quantization)`（置于 Common 之后） |
| `svt-av2\tests\CMakeLists.txt` | 注册 `test_picture`/`test_block_geometry`（链 `svt_av2_common`）与 `test_quantization`（链 `svt_av2_quantization`） |

判决性证据（`build\00` 编译产物）：`picture.c.obj`、`block_geometry.c.obj`、`intra_prediction.c.obj`、`quantization.c.obj` 均产出；`lib\svt_av2_prediction.lib`、`lib\svt_av2_quantization.lib` 均生成。

---

## 3. P1 集成修复：T013 公共头阻塞（00 裁定并落地）

T013 上轮报 `[BLOCKED]`：公共头缺 packet 出流接口。00 落地：

1. `include\svt_av2.h` 新增 `SvtAv2Packet`（`data`/`capacity`/`size`/`pts`）与 `svt_av2_enc_get_packet_ex`。
2. 删除 `Source\Lib\Codec\codec.h` 的 `SVT_AV2_PACKET_TYPES_STAGED_BY_T013` 暂存块（保留说明注释）。
3. `docs\m1-m2-contracts.md` 新增 **§7.1**，登记 packet 生命周期语义（成功/无输出/容量不足/在途/EOS 后的返回与 size 约定）。
4. **NULL=EOS 裁定**：采纳契约 §7（`send_picture(NULL)` = EOS，返回 `SVT_AV2_ERROR_NONE`）；`tests\unit\test_api.c:147` 的 M0 旧断言按新契约改写归 T013。

实测：`app\SvtAv2EncApp\main.c` 现已只依赖公共 API 且**编译通过**（此前 C2065 `SvtAv2Packet` 未声明）。

---

## 4. 构建与测试基线（`build\00`，Release，WARNINGS_AS_ERRORS=ON）

配置/构建命令（可复跑）：

```powershell
. C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1
cmake -S svt-av2 -B svt-av2\build\00 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON
cmake --build svt-av2\build\00
ctest --test-dir svt-av2\build\00 --output-on-failure
```

| 检查 | 结果（首轮） | 结果（第二轮，模块全注册后） |
|---|---|---|
| configure | exit 0（无 "target absent" 提示，三模块已注册） | exit 0（无 "target absent"） |
| build | **exit 0**（零警告，`/WX` 生效） | **exit 0**（零警告） |
| CTest | 19 项注册，17 Passed，2 Failed | **23 项注册，23 Passed，0 Failed**，总耗时 3.61s |

### 4.1 新增注册的测试（此前从未被编译）

`test_picture`、`test_block_geometry`、`test_intra_prediction`、`test_residual`、`test_quantization` 中，
首轮前四项 **Passed**；`test_quantization` 失败（见 4.3），**已由 T010 修复**（第二轮 23/23）。

### 4.2 失败项 `test_api`（属 T013，非回归）

```
FAIL tests\unit\test_api.c:147: send_picture(handle, NULL) == INVALID_PARAMETER (got 0, expected 2)
FAIL tests\unit\test_api.c:148: send_picture(handle, &picture) == NONE (got 3, expected 0)
FAIL tests\unit\test_api.c:247: send_picture(handle, &picture) == NONE (got 2, expected 0)   ×64
test_api: 473 checks, 66 failures
```

根因：NULL=EOS 契约（§7.1 已冻结）与 M0 旧断言冲突；`:148`/`:247` 使用伪 picture（`int32_t` 强转 `void*`），按 T013 任务书 §P1 必须改为真实 picture 构造。**不是** 00 的 CMake/头文件改动引入的回归。

### 4.3 失败项 `test_quantization`（属 T010，真实数值缺陷）

```
FAIL qlookup(72) = 312, expected 248
FAIL qlookup(73) = 320, expected 288
FAIL qlookup(96) = 624, expected 448
FAIL qlookup(97) = 640, expected 512
FAIL qlookup(98) = 656, expected 544
FAIL test_quantization.c:417: q == 1 (got 4, expected 1)
FAIL test_quantization.c:418: dq == 88 (got 352, expected 88)
FAIL test_quantization.c:423: q == -1 (got -4, expected -1)
FAIL test_quantization.c:424: dq == -88 (got -352, expected -88)
FAIL test_quantization.c:701: failures == 0 (got 39, expected 0)
test_quantization: 2179 checks, 10 failures
```

00 **未**替 T010 修改这些数值；需 T010 判断是实现表错还是期望值错，并给出 AVM file:line 依据。

### 4.4 00 的最小集成修复（注册后才暴露）

`tests\unit\test_quantization.c:782` 的常量截断在 `/W4 /WX` 下触发 MSVC **C4310**（warning as error）。
最小修复：把常量改为经**非局部变量**传递（保留原断言意图与注释）。这是 T010 测试源首次进入构建才暴露的问题。

---

## 4.5 第二轮：全模块注册，统一构建 23/23 通过（00 亲自）

T009/T012 的模块此前只存在于磁盘、未进构建。00 本轮完成全部注册：

| 改动文件（公共文件，归 00） | 内容 |
|---|---|
| `svt-av2\CMakeLists.txt` | 新增 `add_subdirectory(Source/Lib/Transform)`、`add_subdirectory(Source/Lib/Reconstruction)`、`add_subdirectory(Source/Lib/Encoder)`（置于 Common/Prediction/Quantization 之后；Encoder 依赖 Prediction+Reconstruction） |
| `svt-av2\tests\CMakeLists.txt` | 注册 `test_transform`（链 `svt_av2_transform`）、`test_reconstruction`（链 `svt_av2_reconstruction`）、`test_intra_frame`（链 `svt_av2_encoder_core`）、`corpus_matrix`（`check_matrix.py`） |

判决性证据（`build\00`，MSVC 19.44 / Ninja / `WARNINGS_AS_ERRORS=ON` / Release）：

```
[15/39] Linking C static library lib\svt_av2_reconstruction.lib
[16/39] Linking C static library lib\svt_av2_transform.lib
[17/39] Linking C static library lib\svt_av2_encoder_core.lib
...
100% tests passed, 0 tests failed out of 23
```

23 项 = 原 19 项 + `test_transform` + `test_reconstruction` + `test_intra_frame` + `corpus_matrix`。

> 注：`Source\Lib\Encoder\transform_path.c`（T012 的 T009/T010 真实绑定）**尚未列入** `Encoder\CMakeLists.txt` 的源列表，因此本轮未被编译；它属于 T012 的真实接线工作（T009/T010 尺寸裁决后由 T012 启用）。`test_intra_frame` 目前覆盖的是"接缝为 NULL 时失败关闭"路径。

---

## 4.6 tx_size 裁决（00，2026-10-07）——M2 首版不是 8x8

T009 提议的"8x8"前提**不成立**。实测链（只读 AVM 源码）：

1. M1 帧头硬编码 `tx_mode_select = 0`（`svt-av2\Source\Lib\Bitstream\frame_header.c:237-240`）→ `TX_MODE_LARGEST`；
2. `tx_mode_to_biggest_tx_size[TX_MODE_LARGEST] = TX_64X64`（`avm\av2\common\common_data.h:703-706`）；
3. `tx_size_from_tx_mode()`（`avm\av2\common\blockd.h:2580-2589`）+ 顶层 64x64 `PARTITION_NONE` → luma **TX_64X64**，chroma 经 `av2_get_plane_tx_size()` → **TX_32X32**（`common_data.h:421-448`）；
4. 与 M1 `block_writer.c:55-56` 已写的 `txSzCtx 4`/`txSzCtx 3` 一致。

**裁决：保持 `TX_MODE_LARGEST`，luma TX_64X64 / chroma TX_32X32（路径 A，零新语法）。** 64x64 逆变换在 AVM 中是 **32x32 核 + 2× 复制上采样**（`idct.c:648-649,687-688,730-751`），`skipWidth/skipHeight` 恒为 0；`tx_scale` = 2(luma)/1(chroma)（`idct.c:761-765`）。

已写入契约 §3/§4，并抄送 T009/T010/T011/T015。

---

## 5. 十队列重派（2026-10-07）

T007–T016 全部按当前事实重派（`delivery=queue`，不打断）。派发书含：00 已完成的集成事实、各自的实测失败项、硬性边界与首轮验收。

投递回执（`scripts\sent-log.jsonl` 可回溯）：

| 角色 | msg id |
|---|---|
| T007 | `msg_1152a252e001VZN4QuM13n6MMu` |
| T008 | `msg_1152a26aa001BLxDqndYtkIocc` |
| T009 | `msg_1152a2809001JOrltRsAXAGiyF` |
| T010 | `msg_1152a295f001N9ueDz7IWFCp0P` |
| T011 | `msg_1152a2ab6001qNqUKYWS8jSQ9y` |
| T012 | `msg_1152a9cb7001n7pXeorH68hgKT` |
| T013 | `msg_1152a9e16001S48wp9AJr5VtO1` |
| T014 | `msg_1152a9f73001O6L4E6u3u0tZhB` |
| T015 | `msg_1152aa0d9001g4y6MxJBcB9xdM` |
| T016 | `msg_1152aa2350019go0b1wldmQ5hG` |

各队列首轮重点：T009 零代码最长路径（先选 transform 组合与尺度）；T012 零代码（先做不依赖 T009/T011 的重建+clipping+导出）；T013 完成 P1 并改写 test_api；T010 修 test_quantization；T015 首次启动（header 字段依据表 + 尺寸/level 单测）；T014 修宽高误报；T016 平台矩阵与 sanitizer。

---

## 6. 未验证项（不写成 PASS）

- `build\00` 第二轮仍只跑了 **Release**；Debug 由各队列自验（T007/T008/T010/T012/T013 报 Rel+Dbg 全绿），00 **未独立复跑 Debug**。
- 平台矩阵（Windows Debug、Linux GCC/Clang）、sanitizer、1000 帧长跑：由 T016 自报（S1 五配置全绿、S2 sanitizer 全绿），**00 未独立复跑**；Clang/valgrind/32 位/Linux 原生 oracle 保持"未验证"。
- 各模块进入构建后**数值正确性**未由 00 独立复核（00 只验证"能编译、测试是否通过"）。T007/T008/T010/T009/T012 的数值依据来自各自报告的 AVM file:line，00 只抽查了 tx_size 推导链与量化尺度表。
- M2 编码路径（像素真正进入预测→残差→变换→量化→重建）**尚未接通**：T009/T010 真实接线、T011 coeff syntax、T013 recon 导出未完成。`test_intra_frame` 当前只覆盖"接缝 NULL 失败关闭"。
- 内部 recon 与 decoder 逐像素一致：**未验证**（T014 V3 / T016 S3 均 BLOCKED，等 T012/T013 导出）。
- 多帧（≥2）：当前明确拒绝（exit 15），**未实现**，等 T015 方案。
- 720p/1080p：**未验证**。
- `docs\m1-m2-paths.md:68` 的 y4m.c 基线（`1C3BE2AF…DFE39`）已由 00 从 `F5950936…31C8E` 更正，实测与磁盘一致。

---

## 7. 本轮裁决与受理台账（2026-10-07 第二轮）

### 7.1 00 发出的裁决

| 对象 | 裁决 | 依据 |
|---|---|---|
| T009（抄送 T010/T011/T015） | tx_size = luma TX_64X64 / chroma TX_32X32，路径 A（保持 TX_MODE_LARGEST，零新语法） | frame_header.c:237-240 + blockd.h:2580-2589 + common_data.h:703-706 |
| T014 | 交接 `cmake/SvtAv2Oracle.cmake` 做最小修复（仅该一文件，保留候选顺序） | SvtAv2Oracle.cmake:26 无条件清零，T014 已实跑复现 |
| T016 | S3 唯一判据 = `--recon-matrix`；schema_version 升级由 00 通知 | 契约 §6 + T014 schema |
| T015 | A1/A2/A3 全部采纳；tx_size 推导写入字段表；帧头不新增 tx_size 语法 | 同上 |
| T011 | coeff 布局 raster 行主 `coeff[y*w+x]` 批准；容量改造批准；scan 按 4096/1024 | 契约 §3/§5 |
| T012 | 提示 `transform_path` 的 ≤16x16 假设与 TX_64X64 冲突，须与 T009 对齐 | 契约 §3 |
| T013 | 采纳 shared DLL 符号面核对建议；多帧/尺寸扩展方向 | 契约 §6/§7 |

### 7.2 受理状态

| 队列 | 状态 | 00 处置 |
|---|---|---|
| T007 | **DONE** | 00 独立复跑 `test_picture 30837/0`、`test_block_geometry 202/0`，与自报逐位一致；23/23（Win）+ 22/22（Linux）。报告曾因 `Set-Content -Encoding UTF8` 损坏（**仅记录层**），已按证据链重建，最终版 **68996 B / 1158 行 / 26 节 / U+FFFD=0**，六代码 hash 未变 |
| T008 | **DONE** | 00 独立复跑 `test_intra_prediction 1740891/0`、`test_residual 3458277/0`，与自报逐位一致；DoD 接口级满足 |
| T009 | RUNNING（接受 (A)，开工扩 32/64；BLOCKED 已解除） | 裁决 (甲) 已发并采纳；三处纠正 00 全部认账 |
| T010 | **DONE**（模块级；端到端待 T009） | 00 独立复跑 `test_quantization 2846/0`；独立溯源 `qlookup(100)=704`（`quant_common.c:26-30,130-146`）；第 11 项真 bug 已修并登记 §4 |
| T011 | RUNNING（条款表/容量改造） | 合并裁定已发（T015 开关取值 + 容量契约 + `count=1024` 闭式 + 承载网格≠tx_size）；scan 长度 1024（luma/chroma 均 1024） |
| T012 | READY_FOR_INTEGRATION（首轮，不含 T009/T010） | **受理**；已注册进构建；**已确认 `transform_path.c` 未被编译**（`Encoder/CMakeLists.txt` 只列 `intra_frame.c`）→ 该 CMake 归 T012，需按 TX_64X64/TX_32X32 + `count=1024` 改接缝尺寸校验；**recon 导出为当前最硬公共卡点**（T014 V3 / T016 S3 都卡此） |
| T013 | READY_FOR_INTEGRATION（**仅 P1 完成**；P2/P3/P4 未完成） | P1 受理（`test_api 768/0` + 出流 bit-exact 21B `7AAB5986…CE0F7`）；**00 曾误置 DONE，已撤回**（DoD 是 P1–P4 全部） |
| T014 | READY_FOR_INTEGRATION（V1/V2 + 负例 + cmake 修复）；V3 BLOCKED | **受理**；cmake 修复 00 独立复验通过、批准并入；`corpus_matrix` 已注册；`--matrix-tamper-geometry` 负例 00 独立复现 32/32 + 两个错误分支。DoD 允许"先比 V1/V2、明标 V3 未完成" |
| T015 | READY_FOR_INTEGRATION（**H1/H2 受理**；H3 缺 oracle 实测、H4 等 T011/T012/T013） | H1/H2 受理；`test_header_frames` 已由 00 注册（23→**24/24**）；独立复验冻结 21B 向量**未受影响** |
| T016 | READY_FOR_INTEGRATION（S1/S2）；S3/S4 BLOCKED | **受理**；入口已裁定；GCC 阻塞已解除。DoD 是 S1–S4 全部，未 DONE。已通知：注册数 23→**24**、`run_matrix.py` 换版须用 **`48132100`** 重跑、T015 多帧方案已到可推进 S4 |

> DONE 记账依据：00 亲自复跑（本机 `build\00` + WSL GCC 全新目录），不采信自报数字；自报与实测逐位一致**且该任务书全部阶段项完成**方置 DONE。
> **00 自我更正（2026-10-07）**：T013 曾被 00 误置 DONE，原因是 00 只读了 `test_api`+出流证据、未核任务书 DoD 的"P1–P4 分别有回归证据"。已撤回为 READY_FOR_INTEGRATION。教训：**置 DONE 前必须逐字读任务书的"完成标准/DoD"全文**，不得按已见证据推断范围。
>
> **DoD 逐字复核（2026-10-07，T013 事件后补做）**：T007（"≥100 固定种子随机块视图"→ `test_block_geometry.c:430` 128 case 含 1920×1080 ✔）、T008（"每种已实现尺寸至少 100 固定种子随机 case"→ `test_intra_prediction.c:406` ✔）、T010（"至少 100 个固定种子随机块"→ `test_quantization.c:722` seed 1..128 ✔）三项**逐条核实通过**，其 DONE 成立。T012/T013/T014/T016 均含未完成阶段项，**不置 DONE**（T014 的 DoD 明确允许"先比 V1/V2、明标 V3 未完成"）。
>
> **契约在裁决链中新增的防漏条款（2026-10-07）**——由 T009/T010 顶回 00、00 独立复核后采纳，全部写入 `m1-m2-contracts.md` §3/§4：
>
> | 条款 | 内容 | 若不登记会怎样 |
> |---|---|---|
> | coeff 长度闭式 | `count = min(w,32)*min(h,32)` = `av2_get_max_eob`（25/25 全等） | 扩 TX_16X64 会漏（应 512 非 1024） |
> | 承载网格 ≠ tx_size | TX_64X64 量化须用原始 `(64,64)` 初始化（`tx_scale=2`）；网格 32×32/count 1024 不改 `tx_scale` | `tx_scale` 掉到 1 → `dqcoeff` 偏大 2× **且不报错** |
> | 降尺度归属 | 降尺度由量化器做，transform 出口不做（例外：lossless 升尺度、IST 能量估计） | 系数整体偏大 `2^tx_scale` → T012 重建错 |
> | 舍入顺序 | `ROUND_POWER_OF_TWO(level*704,3)` **之后**才 `>> log_scale`，不可交换 | `log_scale=0` 恒等（≤16 全绿），32/64 偏大 2×/4× |
> | LEVEL_OVERFLOW 余量 | 逐尺寸 **16x16 22.1x / 32x32 11.0x / 64x64 5.5x**（实测口径，00 终裁 2026-10-07），**不得外推** | 用 22x 或 5.6x 误判安全边界 |
> | scan 表长度 | TX_64X64 在**所有 tx_type** 下用 `default_scan_32x32`（1024 项）——AVM 注释\"tx64 高频半数为零，故用 tx32 的 scan 序\"（`scan.c:1804-1806,1888-1890`） | 按 luma 4096 建 scan → 与 `max_eob`/repack/逆入口全部不自洽 |
> | 行号引用 | `tx_mode_select=0` 实际在 `frame_header.c:245-248`（T015 更正；`237-240` 是 `deblocking_filter_params` 的两个 0 位） | 引用错位，后来者找不到冻结位 |
>
> 共同特征：都是 **2 的幂次移位在 `tx_scale=0` 时恒等、扩到 32/64 才暴露** 的静默错误——这正是"必须补 32/64 锚点"的价值。

> 状态词：`READY_FOR_INTEGRATION` 只表示"模块自验通过、等 00 验收"，**不等于 DONE**。00 独立复跑统一构建后才在 §7.3 记账（当前 **24/24**）。

### 7.3 00 独立验证

| 检查 | 命令 | 结果 |
|---|---|---|
| 统一构建（Release, MSVC 19.44, Ninja, /WX） | `cmake --build svt-av2\build\00` | exit 0，零警告 |
| 统一 CTest（Release） | `ctest --test-dir svt-av2\build\00 --output-on-failure` | **24/24 Passed，0 Failed**（23→24：T015 `test_header_frames` 注册后） |
| 统一 CTest（Debug） | `ctest --test-dir svt-av2\build\00\Debug` | **24/24 Passed**（2026-10-07 补跑；**此前该树未 reconfigure、缺 `test_header_frames`**——由 T015 提醒发现） |
| 冻结 21B 向量（Debug 树） | `SvtAv2EncApp --frames 1` | `7AAB5986…CE0F7`，与 Release 树一致 → 配置无关 |
| T014 几何篡改负例 | `run_matrix.py --matrix --matrix-tamper-geometry --sizes 176x144` | **32/32 rejected, exit 0**；仅方形选择 **exit 1**；缺 `--matrix` **exit 2** —— 00 独立复现三项逐位一致 |
| y4m.c 基线 | `Get-FileHash … y4m.c` | `1C3BE2AF…DFE39`（与 paths.md 更正后一致） |
| T014 cmake 修复（带 `-D`） | 自建探针目录 + dummy oracle | `AVM oracle = found at …\fakeoracle`（`-D` 生效） |
| T014 cmake 修复（不带 `-D`） | 同上，无 `-D` | `AVM oracle = found at …\AVM-AV2-TEST\tools\bin`（默认未受影响） |
| **Linux GCC 全量（00 独立）** | WSL GCC 11.4.0 / Ninja / Release / `WARNINGS_AS_ERRORS=ON`，全新目录 `build/00-wsl-gcc` | configure exit 0（无 "target absent"）；build exit 0 **零 warning/error**；**CTest 22/22 Passed**（23 项减去需 oracle 的 `runner_oracle_smoke`） |

### 7.4 tx_size 契约的二次收敛（T009 顶回 00，2026-10-07）

00 首版裁决书有**两处错值**，由 T009 带 AVM 出处顶回，00 逐行复核后**全部采纳并更正**（这是本项目期望的工作方式：不盲从、以文件为准）：

| 00 原写 | 事实 | 依据 | 处置 |
|---|---|---|---|
| `av2_get_max_eob(TX_64X64)=4096` | **1024** | `blockd.h:3477-3490` | 已更正 T011/T010 消息 + 契约 §3/§4，留更正记录 |
| "64x64 只是 32×32 核 + 2× 复制"（对正变换） | 正变换是 **64 点核 + 高半部置零 + repack** | `hybrid_fwd_txfm.c:952-953,955-956,939,984,1003-1008` | 契约 §3 改写"正/逆不对称" |
| "TX_MODE_SELECT 下 64x64 不允许再分" | 可再分（`allow_tx_horz_split` 为真） | `av2_common_int.h:5058,5198-5214,5193-5196` | 契约 §3 留更正记录 |

**裁决 (甲)**：`coeff`/`qcoeff`/`dqcoeff` 长度 = `min(w,32)*min(h,32)` = `av2_get_max_eob(tx_size)`；TX_64X64 → **1024**。三方自洽（正变换出口 repack / 逆变换入口 `min(1024,…)` / `max_eob` 特判）。

### 7.5 由裁决直接抓出的真 bug（T010 第 11 项）

T010 按 00 要求补 32×32/64×64 锚点时，**立即失败 6 处**：`svt_av2_quantize_coeff_idx()` 做 `ROUND_POWER_OF_TWO(level*dequant,3)` 后**漏掉 AVM 紧接的 `>> log_scale`**（`av2_quantize.c:118-121`）。`log_scale=0` 时恒等（故 ≤16x16 全绿），但 **M2 首版真实尺寸（TX_64X64/TX_32X32）下 dqcoeff 会偏大 2×/4×**，T012 用它重建即得错误残差。已修（`quantization.c:399-409`），契约 §4 登记（含"先舍入后移位、顺序不可交换"）。

> 这是 00 裁决链的价值证据：不补 32/64 锚点，该 bug 会带进 M2。

### 7.6 通道问题（待观察）

T009 的部分出站消息在通道里**编码损坏**（payload 内即为坏字节，GBK 双重误码，无法还原）。T009 已改用 `-MessageFile` 路径后恢复正常（#81 可读）。**纪律重申**：长中文消息一律用 `-MessageFile` 写 UTF-8 无 BOM 文件，勿用 `-Message` 内联中文。

**陈旧消息重投（2026-10-07，00 观测）**：T008 的 `[T008][RUNNING] 首轮计划` 以"新消息"形态到达，但经 `scripts\sent-log.jsonl` 定位，其原始记录为 **line[222] 07:01:47**，正文逐字相同；T008 会话 `updated=08:39:54`（未被唤醒），且其交付物 hash 与磁盘逐字节一致。**判定为陈旧内容被再次投递，非新工作**。
- **风险**：若执行会话按"首轮计划"重做，会重复劳动并可能改动已冻结的交付物。
- **处置**：00 已回 `[00][INFO]` 说明并令其**不要重做**；同时核验四个代码/测试 hash 未变，**DONE 维持**。
- **判据（防误判）**：收到"首轮/计划"形态消息时，先查 `sent-log.jsonl` 有无同文记录、再查会话 `time.updated` 是否为近期、再比对交付物 hash。**三者同看**，勿按内容新旧推断。

**重投模式（同一轮内二次确认，2026-10-07）**：随后 T012 的 `[T012][RUNNING] 首轮做不依赖 T009/T011 的部分…` 以同样形态到达，定位为 **line[223] 07:01:52**（正文逐字相同），而 T012 早在 **07:29:44（line[264]）已报 READY**。
- **共同特征**：两次都是**各角色最早那封 `[RUNNING]`**（07:01:47 / 07:01:52，相隔 5 秒），且都在该角色**早已交付并验收之后**才到达。
- **推论**：这是**启动期首封通知滞留在投递队列、后被批量释放**的现象，不是角色重跑。故 00 的处置一律为：**核验 → 回 INFO 令其不要重做 → 维持原状态**，**不重开任务**。
- **待办（00）**：若后续仍出现，需核对本机 OpenCode server 的 session inbox 消费语义（`GET /api/session/{id}/inbox` 为"未消费"视图，计数会随消费递减：107→106→105）。

---

## 8. 队列积压事故 + 表单冻结根因（00 亲查 · 2026-10-07）

### 8.1 现象

00 会话的服务端队列积压 **103 条** 07:03–09:00 历史消息，每跑一个回合被喂一条（107→106→105→104→103）。按此将烧掉上百回合重复处理早已闭环的报告。

### 8.2 根因（三层排查，逐层排除）

1. **不是通道坏了**：`GET /api/session/{id}/inbox` 文档原文是 "durable enqueued session work **not yet delivered**, ordered by enqueue sequence" —— 这些消息**从未真正投递过**，只是排队。
2. **读取不消费**：`GET` 是只读视图（00 连读三次计数稳定 103），只有目标自己跑回合才消费。
3. **真根因**：**7 个执行会话（T008/T009/T010/T011/T012/T013/T016）全部停在一个未结算的 `question` 表单上** —— 其消息列表**最新一条**即 `question` 工具调用（`executed:false, status:"running"`）。会话被冻住 → 00 发的所有裁决堆在其队列里**投递不进去** → 队列只增不减。
4. 对照 `/api/session/active`：8 个 running = **00 + 上面 7 个**（T007/T014/T015 未卡表单、队列 0）。

### 8.3 处置

- **答 7 个表单**（`POST /api/session/{id}/form/{formID}/reply`，体 `{"answer":{"q0":"<value>"}}`）→ 7/7 成功，`form` 计数全归 0，会话立刻恢复（T007/T009 随即在 10:31 活跃）。
- **00 队列取证后清空**：103 条**全部**在 `scripts\sent-log.jsonl` 可回溯（103/103 命中），已 dump 落盘（171341 字符 / U+FFFD=0）→ `DELETE /api/session/{id}/inbox/{id}` 取消 103/103（0 失败）。
- 新增 `scripts\oc-inbox.ps1`（`-Status` / `-Dump` / `-Drain -Confirm`），机制与处置纪律写入 **`AGENTS.MD` §7.6**；脚本内置兜底 `pendingForms>0 → SKIP`，**不允许**用"删队列"代替"答表单"。

### 8.4 教训（已写入 AGENTS.MD §7.6）

- **未答表单会静默冻结整个会话**，症状与"通道故障"极像（消息投不进、计数不降）。**排查顺序：先查 `form`，再查 `inbox`。**
- 排空前**必须** dump + 确认 sent-log 可回溯；**只排历史积压**，绝不排承载未消费裁决的队列。
- 判别"陈旧重投"用**三者同看**：sent-log 有无同文、会话 `time.updated`、交付物 hash —— 勿按内容新旧推断。

## 9. 本轮 00 新裁决（2026-10-07 · 积压恢复后）

| 议题 | 裁定 | 依据（00 独立核） |
|---|---|---|
| T011 裸位接口 | **(A) 批准**，T011 实现 | `avm_read_bit` = `od_ec_encode_literal_bypass`（真 bypass，`entenc.c:216-223`）；AVM 系数语法用它（`decodetxb.c:890`/`encodetxb.c:1182`）；现有仅符号接口无法表达；AVM `od_ec_enc_normalize` 与 svt `ent_normalize` **仅差 `n_bypass` 分支**。闸门：M1 向量 `E6 C8 00 12` 逐字节不变 + 裸位路径须独立 oracle/对拍（裸位 bug 静默） |
| T007 适配入口 | **归 T007**，采用其 **A1**（显式 `SvtAv2BorrowedView` 描述符），**只进内部头** | `copy_in` 是 T007 冻结原语，入参约束由它定义；`picture.h:117-121` 禁止第二份拷贝；T007 方案把 row-loop 抽成 `static` 共用 → 仍是唯一实现。连带修其自曝的测试反模式（`fill_external` 伪造 buffer） |
| T008 报告口径 | **不冻结**；判据 = 契约层四 hash；记录层不作判据 | 与全项目方法论一致；只读快照会把记录层变相升级为判据层 |
| T009 交付 | **受理**（hash 4/4 逐字节一致 + 00 复跑 `1177889/0`） | 契约 §3 已是 1024（T009 的 §五 疑问不存在） |
| T012 交付 | **模块面受理**；**另有 T016 抓到的真缺陷待修** | 5/5 hash 一致；00 复跑 `test_transform_path 180/0`、`test_reconstruction 40495/0`、`test_intra_frame 62817/0` 与自报一致 |
| T016 S1 扩围 | **受理**；注册数更正 **Win 25 / Linux 24** | 00 本轮新增注册 `test_transform_path` + `test_t009_t010_link` |
| T013 P2 前置 | **已解除**（seam 逐参数一致，T012 已跑通帧级链路） | `intra_frame.h:80-97` vs `transform_path.c:74-83/152-158` 逐参数核对 |

## 10. 本轮 00 亲自发现的真缺陷（不是转抄）

### 10.1 `transform_path.c:179` —— GCC-only `-Werror=maybe-uninitialized`（T016 先报，00 独立复现）

```
Source/Lib/Encoder/transform_path.c:179:9: error: 'packed_dq' may be used uninitialized
    [-Werror=maybe-uninitialized]        (仅 Linux-Release -O3 -DNDEBUG 触发)
```

- **00 独立复现**：WSL GCC 11.4.0 / Release / `-DSVT_AV2_WARNINGS_AS_ERRORS=ON`、全新目录 `build/00-wsl-gcc-t012` → `cc1: all warnings being treated as errors`。
- **根因**：`path_inverse()` 的 `path_gather_coeff` 只填 `kw*kh` 项，`svt_av2_inv_transform` 读 `block_w*block_h` 项；运行时覆盖完整但 GCC 无法透过 memcpy 证明。Debug(-O0) 与 MSVC 均不报。
- **方法学**：`-fsyntax-only` 回放会漏报该告警（跳过优化 pass）；**必须保留 `-O3` 且真 codegen**。这条由 T016 提出、00 采纳。
- 已发 T012 修复要求（含"保持 `test_transform_path 180/0` 不回归"）。

### 10.2 路径陷阱：两条只差一个字符的真实目录

```
C:\Private\ffmpeg-native-deps-full\AV2     ← 真项目（完好，创建 10/06 19:40:54）
C:\Private\ffmpeg-native-deps-full\AV2     ← 空目录（创建 10/07 16:53:13，事故产物）
```

- **两条路径都真实存在**（不是 8.3 短名）。T010 因此误报"工作区被清空"，并据 `dir /s /b` 的**陈旧索引**二次误判。
- 00 两次判定"工作区完好"是对的（实测 `svt-av2` 递归 11758 项、`Source\Lib` 9 模块、`T010.md` 52KB 全在）。
- **纪律**：不得向 dash 路径写入；`dir /s /b` 在目录状态异常时会返回陈旧路径，不可作判据。

## 11. 本轮 00 新注册（公共文件，00 串行）

| 测试 | 挂载 target | 结果 |
|---|---|---|
| `test_transform_path` | `svt_av2_encoder_core` | `180 checks / 0 failures` |
| `test_t009_t010_link` | `svt_av2_transform` + `svt_av2_quantization` | `14543 checks / 0 failures`（**32x32/64x64 已覆盖，不再 skip**） |

**00 独立复跑 `build\00`（Release / MSVC / `/WX`）**：CTest **25/25 Passed**（`test_transform 1177889/0`、`test_intra_frame 62817/0`、`test_reconstruction 40495/0`）。

> `test_t009_t010_link` 的价值：第一次在真实调用链上验证"承载网格 ≠ tx_size" —— 实测 `tx_scale` = 0/0/0/1/2、`count` = 16/64/256/1024/1024，与契约 §3 闭式 `min(w,32)*min(h,32)` 完全一致。

## 12. hash 差异（待 T010 核对）

| | 行数 | SHA256 |
|---|---|---|
| T010 自报 | 163 | `40FFC84806CFAB4EF4D545BD5BA5441FAA36DF4B9720D45E0D0A2D0F7C4E0D4D` |
| **真树** `svt-av2\tests\unit\test_t009_t010_link.c` | **166** | **`40A85977AAE2D51321B7B21A7C0A197291B13352543274B11AF2E36FE66B3690`** |
| dash 树副本 | 163 | `40FFC848…E0D4D` |

→ T010 报的是 dash 树副本；**00 注册并验证的是真树版本**（`40A85977…`，14543/0）。已要求 T010 确认并更正报告。

---
## 13. 本轮 00 裁决与亲验（2026-10-07 · 第二轮）

### 13.1 00 新落的公共变更（00 串行职责）

| 变更 | 文件 | 内容 | 00 验证 |
|---|---|---|---|
| **Codec→Reconstruction 构建边（裁定①）** | `Source\Lib\Codec\CMakeLists.txt` | `svt_av2_enc` + `svt_av2_enc_shared` 各加 `PRIVATE include Source/Lib/Reconstruction` + `PUBLIC link svt_av2_reconstruction` | Release/Debug 均 configure+build exit 0、CTest 26/26、M1 向量零回归 |
| **注册 `test_transform_path`** | `tests\CMakeLists.txt` | 挂 `svt_av2_encoder_core` | `180 / 0` |
| **注册 `test_t009_t010_link`** | `tests\CMakeLists.txt` | 挂 `svt_av2_transform` + `svt_av2_quantization` | `14543 / 0`（**32x32/64x64 已覆盖，不再 skip**） |
| **契约 §4 终裁** | `docs\m1-m2-contracts.md` | 64x64 余量 `2.8x → 5.6x`、level `2934 → 1467` 两版均作废，**实测 5.5x / 1483**，附三次读数更正记录 | 00 2026-10-07 终裁：T010 解析版 5.6x 仍错（松散界 `N·90` + 把 pass-2 累加长度误当 32）；00 自建 `artifacts\00\probe_level_bound_exact.c` 从真实核得 R=64N、上界 32640，与实测逐值相等 |

**裁定①的理由（非"最直接"）**：帧 core 入口形参含 `SvtAv2FrameBuffer*` / `SvtAv2ReconScratch*`（均 `reconstruction.h` 类型）→ Codec 调帧 core **必然**要认识它们；②/③ 需改 T012 已定稿接缝签名。①是纯增量、零行为变化。

### 13.2 00 独立复跑（Release + Debug，均 `/WX`）

```
build\00        Release : configure/build exit 0 → ctest 100% passed, 0 failed out of 26
build\00-debug  Debug   : configure/build exit 0 → ctest 100% passed, 0 failed out of 26
```

| 测试 | Release | Debug | 备注 |
|---|---|---|---|
| `test_transform` | **1187622 / 0** | **1187622 / 0** | 与 T009 自报逐位一致 |
| `test_t009_t010_link` | 14543 / 0 | 14543 / 0 | 新注册 |
| `test_transform_path` | 180 / 0 | 180 / 0 | 新注册 |
| `test_reconstruction` | 40495 / 0 | 40495 / 0 | |
| `test_intra_frame` | 62820 / 0 | 62820 / 0 | |
| `test_picture` | 30871 / 0 | 30871 / 0 | 含 A1 borrowed-view 路径 |
| `test_quantization` | 2846 / 0 | 2846 / 0 | |
| `test_tile_writer`（M1 冻结向量） | 11 / 0 | — | **M1 零回归** |
| `test_header_frames`（M1） | 127 / 0 | — | |
| `test_api`（M1） | 768 / 0 | — | |

### 13.3 本轮 hash 核验（00 复算）

| 文件 | SHA256 | 判定 |
|---|---|---|
| `Source\Lib\Transform\transform.h` | `74CCDD474ECFEF130D4D1C6112AE35951A24FDEACF7DF7DE6DD5EFEE490369FA` | ✔ 与 T009 自报一致 |
| `Source\Lib\Transform\transform.c` | `3DD7596E2C785CB20D97A5199BE13E54A3332B64A7C3F6AAE92134B4D68EE8A3` | ✔ |
| `Source\Lib\Common\picture.h` | `139451843B5FC4AD610CF0E8448C64FD8C602A354CC403FDE0C87A24F0F03E3D` | ✔ T007 A1 |
| `Source\Lib\Common\picture.c` | `A7422128800CF74B0AD587030D66EE3EDA59EC30D2FA0BFC7A6E7D7EC6E8BA65` | ✔ |
| `tests\unit\test_picture.c` | `DC8A1A9DB09FBAA3D3840C31667D6A3FAFEA88EC4527C3FD008D2ABAA0CC572E` | ✔ 伪造 buffer 已移除 |
| `tests\unit\test_t009_t010_link.c` | `40A85977AAE2D51321B7B21A7C0A197291B13352543274B11AF2E36FE66B3690` | ✔ T010 确认最终版 |
| `Source\Lib\Encoder\transform_path.{h,c}` | `CB0C388E…14191` / `BECE1591…6C3AC` | ✔（`transform_path.h` 过期 Scope 注释待 T012 修） |

### 13.4 本轮 00 新裁决

| 议题 | 裁定 | 依据 |
|---|---|---|
| T013 P2 前置②（Codec→Reconstruction 构建边） | **① 批准并已由 00 落地** | 帧 core 形参强制 Codec 认识 recon 类型；②/③ 需重开 T012 定稿接缝 |
| T013 耦合风险（布局切换） | **采纳为 P2 硬约束**：加法优先 / 布局+寻址+recon 原子 / **M1 21 字节向量必须不变** | A1 `copy_from_view` 需 allocated 布局（`align_up(w,32)`）vs M1 紧密布局 |
| T009 交付 | **DONE** | Release+Debug 双复跑 `1187622/0`、CTest 26/26、契约层 hash 逐字节一致 |
| T007 A1 交付 | **受理（新增内部接口，非重开 M1）** | 00 复跑 `test_picture 30871/0`；伪造 buffer 反模式根治 |
| T010 契约 §4 更正 | **受理并落盘** | 解析探针漏 pass-2 只跑 32 行；T009 独立复算吻合 |
| T016 注册数 | **更正 Win 26 / Linux 25** | 00 又注册 `test_t009_t010_link` |
| `ctest` per-test 超时 | **暂不落公共配置** | 会引入慢机误判；改以"跑矩阵前确认源稳定"（已入 AGENTS §7.7） |
| 并发写 `sent-log.jsonl` | **纪律：禁止 `-Tail` 判丢失**（已入 AGENTS §7.7） | 并发 `Add-Content` 下 Tail 读半写行 → 误判；须 ReadAllText |

### 13.5 本轮 T016 S1 扩围抓到的真缺陷（00 独立复现）

```
Source/Lib/Encoder/transform_path.c:179: error: 'packed_dq' may be used uninitialized
    [-Werror=maybe-uninitialized]      ← 仅 Linux-Release -O3 -DNDEBUG
```
**00 独立复现**（WSL GCC 11.4.0 / Release / `-DSVT_AV2_WARNINGS_AS_ERRORS=ON`、全新 `build/00-wsl-gcc-t012`）。
根因：`path_gather_coeff` 只填 `kw*kh`，`svt_av2_inv_transform` 读 `block_w*block_h`；运行时覆盖完整但 GCC 无法透过 memcpy 证明。**已要求 T012 修复 + 回报新 hash。**
T013 独立判断"MSan/GCC 告警而非功能性误读"**与 00 复现一致**。

### 13.6 记录层 hash 差异（不作判据，供属主自查）

| 文件 | 属主自报 | 00 实测 | 说明 |
|---|---|---|---|
| `docs\task-reports\T009.md` | `D29F153E…5AD2AD` | `D4EF904B…3F6B08` | 消息发出后报告又落盘一次（mtime 19:14:57） |
| `docs\task-reports\T013.md` | `F25F5D96…CEBECF` | `008846B5…55DA5` | 同上；已通知属主 |
| `docs\task-reports\T009-scales.md` | `A5975554…783B65` | `A5975554…783B65` | ✔ |

**记录层持续演进、不作判据**（全项目方法论）；已通知属主核对报告内引用是否过期。

---
## 14. 第三轮：recon 导出解锁 + T012 不通过项 + T011 表单再冻结（00 · 2026-10-07）

### 14.1 T011 表单再次冻结会话（本项目第二次踩同一坑）

`GET /api/session/{ses_eeadd29f8ffeXOlshy7tjbK253}/form` → **1 个未答表单** `frm_1161567f9001i0fGpLb0cY05gP`（`coeff 全链路验证策略`）。
症状与首次完全相同：T011 队列 18 条只增不减、我发的裁决投递不进去。
**处置**：00 直接裁定并答复（`POST .../form/{id}/reply` → **204**），会话解冻。
**裁定内容**：选 **方案 2（先交付 + 标 missing）**，但要求写成 `PENDING + 闭环路径`（非空白 missing），闭环 = **只读 oracle `avmdec.exe`**（`AVM-AV2-TEST\tools\bin\avmdec.exe`，实测存在；`avm-build` 目录**不存在**）。**否决方案 3**（自写最小解码器 → 与自家 encoder **共模错误**，只有真 oracle 能打破）。
**已写入 `AGENTS.MD` §7.6**：不要用 `question` 阻塞等裁定，直接发 `[BLOCKED]` 消息后结束回合。

### 14.2 T012 recon 导出交付（00 独立复跑）

```
=== Release ===  test_reconstruction 40495/0  test_intra_frame 62820/0  test_transform_path 180/0
=== Debug   ===  test_reconstruction 40495/0  test_intra_frame 62820/0  test_transform_path 180/0
=== probes  ===  PACKING OK / PARAMS OK / EXPORT OK
```
**新接口**（`reconstruction.h:237/261`）：`SvtAv2ReconExport`（`frame_index`+可见几何+`byte_count`）+ `svt_av2_recon_export_frame()`。
**满足硬约束**：内部 recon 产出（非 avmdec raw / 非 source 冒充）；**复用 T007 `svt_av2_picture_export_raw`，无第二份打包**；移除全部 padding；容量不足不写字节不改 meta；`frame_index` 调用者记录。
→ **T014 V3 / T016 S3 阻塞源解除**（已通知）。

### 14.3 【不通过】T012 `transform_path.c:179` GCC 缺陷 **未修**

00 **本轮再次独立复现**（WSL GCC 11.4.0 / Release / `-DSVT_AV2_WARNINGS_AS_ERRORS=ON`、全新 `build/00-wsl-gcc-verify`）：
```
FAILED: Source/Lib/Encoder/CMakeFiles/svt_av2_encoder_core.dir/transform_path.c.o
transform_path.c:179:9: error: 'packed_dq' may be used uninitialized [-Werror=maybe-uninitialized]
```
磁盘事实：`transform_path.c` = `BECE1591…6C3AC`（mtime `18:35:55`、9348B）—— **与上轮基线逐字节相同、未改动**；`:175 int32_t packed_dq[SVT_AV2_TX_MAX_PELS];` 仍无初始化。
→ **T012 的 READY 不受理**（T016 的 S1 `Linux-Release` 继续保持 FAIL）。

### 14.4 00 承认的一处读数错误

我此前引用 `Encoder\CMakeLists.txt = 306B19D7…`、`transform_path.c = 9F6E1B8F…` 为**更早时刻的旧读数**。T012 实测（我复算确认）：
```
Source\Lib\Encoder\CMakeLists.txt   9D13098F…4E183   （含 transform_path.c，已在源列表）
Source\Lib\Encoder\transform_path.c BECE1591…6C3AC
```
→ **T012 的"以当前磁盘为准复验"正确；该项撤回。** 教训：**跨轮引用 hash 必须当轮复算**（已入 `AGENTS.MD` §7.7 纪律 1）。

### 14.5 本轮 hash 台账（00 当轮复算）

| 文件 | SHA256 | 判定 |
|---|---|---|
| `Source\Lib\Encoder\transform_path.h` | `CB0C388E8EDFB762A5B72788ED9F479BBC6E196C6CA2BB86BA8F4FAC6B41DA2A` | ✔ 过期 Scope 注释已修（`4/8/16/32/64`） |
| `Source\Lib\Encoder\transform_path.c` | `BECE159195B1E6690AC605D3120DF2C301834C76A126ECDBD633C0CB2C56C3AC` | **未修 GCC 缺陷** |
| `tests\unit\test_transform_path.c` | `3AF870A80BDDAAB49AA41C6053305A09D1A365ABD70E52DA7DBBBB8505DC8AD8` | ✔ |
| `artifacts\T012\probe\probe_tx_params.c` | `41EF3E557AC40A321FDF31C72228252ED26D8315DF4C3D844EE1111AFB03B842` | ✔ |
| `artifacts\T012\probe\probe_recon_export.c` | `883C73065B79503F1705E56C714C59AAD9EAC01B2AF16B7C39098142722FD3C0` | ✔ |
| `artifacts\T012\build_and_test.ps1` | `C8AA8F57FE35779C1718661C5B55CEF61EB7DC5A44A4997A01B1A22D5A12533C` | ✔ |
| `Source\Lib\Common\picture.c` | `A7422128800CF74B0AD587030D66EE3EDA59EC30D2FA0BFC7A6E7D7EC6E8BA65` | ✔ 00 当轮复算，与 T016 读数一致 |

### 14.6 T016 报的"跨会话事实"（受理）

T016 抄送：T007 自证"本轮未改代码"但磁盘在**其后**发生实质改动（mtime 18:56–19:08）——`picture.h` 已是 `139451843B5FC4AD…`（A1 三符号）。**00 实测确认 `picture.h`/`test_picture.c` 确已变**（`139451843B…` / `DC8A1A9D…`）。
→ **受理 T016 的提醒**：验收/记录一律按**当轮复算**的 hash，**不以旧 hash 判"未变"**。此条已并入 §14.4 的纪律。

### 14.7 本轮 CTest 计数（00 实测）

`build\00`（Release）与 `build\00-debug`（Debug）均 **26/26 Passed**。注册数 **Win 26 / Linux 25**（Linux 少 `runner_oracle_smoke`）。

---
## 15. 第四轮：coeff 布局分歧裁定（(甲)+行 stride=min(w,32)）+ T013 硬闸门通过 + T014 V3 仍 BLOCKED（00 · 2026-10-07）

### 15.1 【重要】coeff 布局分歧：契约明确选了 (甲)，T012 实现成了被否决的 (乙)

**T012 报的分歧（成立）**：其 `transform_path.c` 把 `qcoeff` scatter 回 **64 宽光栅（4096 项、高/右半置 0）**；T011 的 scan 建在 **32 宽调整网格（1024 项）**。

**00 裁定（读契约原文 + 双方源码后）**：
- 契约 §3 **L103/L105/L106** 原文已明确：数组长度 = `min(w,32)*min(h,32)`（TX_64X64 → **1024**）、**选 (甲) 而非 (乙)**（乙 = 保持 4096 + 高半部恒 0）、**布局 = 32×32 网格的 raster 行主**。
- **T012 的实现 = 被明确否决的 (乙)**。**T011 没错、契约没错，是 `transform_path.c` 选错分支。**
- **危害等级 = 静默错值**：T011 的 `qcoeff[scan[c]]`（`coeff_syntax.c:165/:742-747`）在 64 宽布局下会落在**零填充区**（除第一行）→ **编出错误系数且无断言失败**。属契约 L110 那族"2 的幂掩盖位错、扩到 32/64 才暴露"的同源缺陷。

**裁定：方案 A**（T012 改自己的 `transform_path.c` 为 32 宽原生布局）+ **补契约"行 stride"澄清**：

> **行 stride 澄清（00 裁定，2026-10-07）**：`coeff`/`qcoeff`/`dqcoeff` 的**行 stride = `min(w,32)`**（即 `kw`），**不是** `block_w`、**不是** `scratch->stride`。TX_64X64 时三者均为 **1024 项、行 stride 32**。
> `intra_frame.h:66-68` 中 "the block-local stride used for every block buffer (`scratch->stride`)" 一句**仅适用于样本类缓冲**（prediction / residual / inverse_residual），**对 coeff 数组不适用**。

**根因**：契约自身对"行 stride"表述不全（L106 只说"raster 行主"，未点明 stride = `min(w,32)`），与 `intra_frame.h:66-68` 的 "block-local stride" 句合读产生歧义。**已由 00 修契约消除。**

**方案 B/C 不采用**：B 要改 T011 已冻结接口 + 给 `SvtAv2CoeffPlaneInfo` 加 stride 字段；C 多一层转换、多一类静默错位风险。

### 15.2 T013 P2 骨架：M1 硬闸门**通过**（受理）

```
produced : 21 B  SHA256 7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7
frozen   : 21 B  SHA256 7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7
byte-diff: differing bytes = 0  →  IDENTICAL
```
- T013 在**"加法优先"阶段就复跑了硬闸门**（未等改布局）——做法正确。
- `p1_evidence` Release/Debug 各 30/30；`test_api` 768/0。
- P2 骨架：`codec.h` 加 `recon`/`scratch`/`recon_ready`；`encoder.c` 加 `ensure_recon_buffers()`（跨帧复用、非 per-block）；**未改** `frame_copy` 布局与 `encode_pending_frame` 寻址 ✔ 符合"加法优先"约束。

### 15.3 `test_tile_writer` 4 处失败 = 并发编辑瞬时态（已消失）

T013 撞到 CTest 25/26（`test_tile_writer` 11 checks / 4 failures），并给出三条证据（链接闭包不含其模块 / T011 于 18:37 并发改 / 非陈旧对象）。
**00 独立复跑**：`test_tile_writer: 11 checks, 0 failures, exit 0`（`tile_writer.c` mtime 仍 `18:37:46`，`ninja: no work to do`）。
→ **确认是 T011 编辑窗口的瞬时态**，与 T013 无关。已并入 `AGENTS.MD` §7.7 纪律 4。

### 15.4 T014 V3 仍 BLOCKED（00 认可其判断）

T014 实测两条硬证据，**拒绝放 PASS（不伪造）**：
1. 现行 app 产出的 internal recon 是**整帧平场**（gradient → 21B → 自报 "zero residual, reconstruction is uniform mid-grey"；`avmdec` 解码 luma unique=1）——与 T016 的 M1 qp 带 DC 塌缩 finding 一致；
2. 全仓除定义+探针外**无调用方**使用 `svt_av2_recon_export_frame`，**app 无落盘选项** → 端到端链路尚不存在。
**00 认可**：现在跑 `--recon-matrix` 只会得到 **flat==flat 的空洞 PASS**，证明不了帧内容/帧序/丢帧。**保持 BLOCKED(3)。**
**解除条件**：(a) 编码器产非空残差码流（T011 syntax + T013 组装）；(b) app/驱动调用 export 落盘为契约布局。

### 15.5 本轮 hash 台账（00 当轮复算）

| 文件 | SHA256 | 判定 |
|---|---|---|
| `Source\Lib\Bitstream\tile_writer.c` | mtime `18:37:46`，`test_tile_writer 11/0` | ✔ 无缺陷 |
| `Source\Lib\EntropyCoding\coeff_syntax.c` | mtime `19:17:10`（活跃编辑中） | 待 T011 报 hash |
| `Source\Lib\Codec\encoder.c` | mtime `19:20:36`（T013 P2 骨架） | 待 T013 报 hash |
| `Source\Lib\Encoder\transform_path.c` | `BECE159195B1E6690AC605D3120DF2C301834C76A126ECDBD633C0CB2C56C3AC` | **仍待修 GCC 缺陷 + 方案 A** |

### 15.6 00 修契约记录

`docs\m1-m2-contracts.md` §3 新增两条（行 stride 澄清 + 依据），并明确 `intra_frame.h:66-68` 的适用范围。备份：`...\Temp\opencode\m1-m2-contracts.md.bak2`。

---

## 16. 第五轮：T011 受理、GCC-only 缺陷家族再现、热路径性能回归（00 于 2026-10-07）

### 16.1 T011 = 模块面受理（READY 受理），00 独立复跑

| 项 | 00 实测 |
|---|---|
| 重配 + 构建（Release `build\00` / Debug `build\00-debug`，MSVC `/W4 /WX`） | exit 0 |
| CTest 全量 | **28/28 Passed**（两配置） |
| `test_coeff_syntax` | **104 checks / 0 failures**（R+D） |
| `test_tile_writer_capacity` | **17 checks / 0 failures**（R+D） |
| 闸门 1（M1 冻结向量） | `test_tile_writer` **11 / 0**，`E6 C8 00 12` 逐字节不变 |
| 闸门 3（裸位 vs AVM oracle） | `cmd /c artifacts\T011\run_gate3.cmd` → **118 checks / 0 failures**，exit 0 |
| 闸门 4（hash 登记） | 00 已更新 `docs\m1-m2-paths.md` §3.3（见 16.3） |

T011 在报告 §7 如实标注的 3 项 missing（未用 `avmdec` 覆盖系数语法层 / `coeff_write_block` 未做逆向解码 / `read_quant` 只做读侧对拍）**00 照收、不视为缺陷**——与既定"表单方案 2：PENDING + 闭环路径 = `avmdec.exe`"一致，闭环路径已登记。

### 16.2 00 落地的公共变更（T011 §8 请求 + T010 §三请求）

1. `Source\Lib\EntropyCoding\CMakeLists.txt`：`svt_av2_entropy` 源列表加 `coeff_syntax.c` + `cdf_tables_coeff.c`；**不**链接 `svt_av2_bitstream`（后者 PUBLIC-links 前者，会成环）。
2. `tests\CMakeLists.txt`：注册 `test_coeff_syntax`（挂 `svt_av2_entropy`）与 `test_tile_writer_capacity`（挂 `svt_av2_bitstream`）。
3. `docs\m1-m2-contracts.md` §4：`|qcoeff| <= 8191` 更正为 **T010 自选保守生产上限**（**非**解码器要求：`decodetxb.c:907` 实际检查 `(level >> 20) != 0`，接受 `2^20-1`）。备份 `m1-m2-contracts.md.bak3`。
4. `docs\m1-m2-paths.md` §3.1–§3.4：**全 34 条 hash 当轮复算并更新**，零漂移（详见 16.3）。

### 16.3 `m1-m2-paths.md` 全量 hash 校准（本轮）

原表 8 处已随 M2 演进漂移、另有 4 个 T011 新文件未登记。00 逐条复算后更新，**全 34 条与磁盘逐字节一致**：

- §3.1（基准 = `AV2\`）：`svt-av2\CMakeLists.txt`、`include\svt_av2.h`、`tests\CMakeLists.txt` —— 均为 00 自有公共文件的 M2 集成改动。
- §3.2：`codec.h` `F5775DDB…`、`encoder.c` `A7423B07…`（T013 P2 骨架，与 T013 报告 §L580-581 一致）。
- §3.3：`sequence_header.{h,c}`、`frame_header.{h,c}`、`keyframe.{h,c}`、`tile_writer.{h,c}`、`entropy_encoder.{h,c}` 共 10 条漂移（T011/T015 属主改动）+ 补登 `coeff_syntax.{h,c}`、`cdf_tables_coeff.{h,c}`。
- §3.4：`tools\run_matrix.py` `48132100…`（T014 锁定值）、`cmake\SvtAv2Oracle.cmake` `D2487671…`（T014 报告 §L184 一致）。

### 16.4 新增缺陷类型（同族第 9 条）：Linux-only `-Werror=unused-function`

`Source\Lib\EntropyCoding\coeff_syntax.c:262 uv_lf_ctx_2d` 是 `uv_ctx_2d` 的**纯转发包装**、**全文件无调用点**：

```
coeff_syntax.c:262:17: error: 'uv_lf_ctx_2d' defined but not used [-Werror=unused-function]
```

- 后果：Linux Release 整仓构建 **exit 1**（`build/00-wsl-gcc-verify`）。
- **为什么 MSVC 全绿是假象**：MSVC `/W4` **不报** unused static function（只报 unused local/parameter），GCC `-Wall` 报 `-Wunused-function`。
- **判据补充（写入本项目纪律）**：**Linux/GCC 构建是独立闸门，不能由 MSVC 全绿替代**。这正是 T016 反复主张的"GCC-only 缺陷"家族的第 9 个实例。
- 00 已附复现命令通知 T011 修复。

### 16.5 T012 GCC 缺陷：已修复，但引入热路径性能回归

**修复确认**：`transform_path.c`（hash `BECE1591…`，mtime `19:53:24`）`-O3 -Wall -Wextra -Werror` **exit 0**，原 `:179`/`:192` 的 `-Werror=maybe-uninitialized` 消除。

**性能回归（00 实测）**：新 `path_gather_coeff` 改为
```c
for (size_t i = 0; i < count; ++i) packed[i] = src[(i / kw) * stride + (i % kw)];
```
`kw` 是运行期变量，编译器无法把 `/kw`、`%kw` 强度削减为乘法 → **每系数一次硬件除法**。GCC 11.4 `-O3`，3 轮最小值（脚本 `artifacts\00-gather-bench2.c`）：

| 几何 | `i/kw` 平坦 | 嵌套循环 | 嵌套 + `memset(16KiB)` |
|---|---|---|---|
| 64x64 | **1911 ns** | 52 ns | 183 ns |
| 32x32 | **1911 ns** | 52 ns | 183 ns |
| 16x16 | **478 ns** | 25 ns | 162 ns |
| 8x8 | **119 ns** | 15 ns | 151 ns |
| 4x4 | **30 ns** | 8 ns | 146 ns |

即 64x64 **慢 36 倍**；而其注释声称要避免的 16 KiB `memset` 仅 **143 ns**——**优化方向反了**。
**正确性已验**：`artifacts\00-gather-prefix.c` 对 4x4..64x64 全 25 组合逐元素比较前缀 → `flat == nested` 全等。**纯性能问题**。
**00 裁定**：用"嵌套循环 + `= {0}`"（约 183 ns，仍比现版快 10 倍）；理由 = 正确性可证明（只增初始化，不改被读位置）+ 可移植（不依赖 GCC 特定 codegen）。

### 16.6 注册数定论（T014/T016 口径对齐）

注册数 = 该 build 目录**上一次 configure 时的树快照**，**非常量、不得作验收判据**。00 独立实测（WSL 里数 `CTestTestfile.cmake` 的 `add_test`）：

| build 目录 | 平台 | 注册数 | CTestTestfile mtime |
|---|---|---|---|
| `build\00` | Win | **28** | 本轮重配（含 T011 新增 2 测试） |
| `build\T016\Win-Release` | Win | 26 | 19:18:54 |
| `build\00-wsl-gcc` | Linux | 22 | 16:38:15 |
| `build\00-wsl-gcc-t012` | Linux | 24 | 18:54:52 |
| `build\00-wsl-gcc-verify` | Linux | 25 | 19:23:11 |
| `build\T016\Linux-GCC-Release` | Linux | 23 | 18:45:03 |

与 T016 报的 22/24/23、Win 28/26 **一致**。**唯一正确做法** = 跑矩阵前先 configure，再实读该目录 `ctest -N`。

### 16.7 WSL 状态

`wsl -l -v` → `ARXChem-Ubuntu-22.04-WSL1` **Running**（Version 1），`uname -a` exit 0；GCC 11.4.0。T012 此前遭遇的 WSL 故障为**瞬时态**，不作为阻塞理由。

### 16.8 本轮未答表单与积压

- **全树未答表单 = 0**（T007–T016 十个角色 `GET /api/session/{id}/form` 全部 `{"data":[]}`）。
- 00 收件箱积压 **80 条**，逐条核对后：绝大多数为已闭环回执；含新可执行项的 4 条（T009 契约登记 ×2、T010 hash 变更、T016 注册数）**已全部处理并回执**。

---

## 17. 第六轮：T013 P2 接口侧落地 + 00 亲验 + 一处**集成级发现**（2026-10-07 20:20–20:45）

### 17.1 00 本轮落地的公共改动（串行，均自证）

| 文件 | 改动 | 自证 |
|---|---|---|
| `include\svt_av2.h` | 加 `SvtAv2ReconFrame` + `svt_av2_enc_get_recon` 声明（契约 §7.2） | MSVC `/W4 /WX` + GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror` 各自 exit 0 |
| `Source\Lib\Codec\CMakeLists.txt` | `svt_av2_enc` / `_shared` 加 link `svt_av2_encoder_core` | 消除 `codec.h:13` 的 `C1083: 无法打开 "transform_path.h"`；`build\00` 由 FAILED 转为 exit 0 |
| `docs\m1-m2-contracts.md` | 新增 §7.2；修正 §7 原"不扩成复杂永久公共 API"措辞 | 与已落地头一致 |
| `docs\m1-m2-paths.md` | §3.1 更新 `svt_av2.h`；§3.2 加 `Codec\CMakeLists.txt` 并标注 T013 在途态 | 全表当轮复算，除 T013 在途 2 项外零漂移 |

**`dllexport` 声明先落盘的安全性（00 实测，非推断）**：`cl /LD /W4 /WX` 编译"仅声明、无定义的 `__declspec(dllexport)` 函数" → **exit 0**。故 00 先加声明、T013 后补实现不会打断任何人的构建。

### 17.2 00 亲自复跑（不转抄 T013 数字）

```
build\00  构建 exit 0；ctest -N = 28；CTest 28/28 Passed
M1 冻结 21B 闸门（00 亲跑 4 种输入：flat/gradient/checkerboard/noise）
   4/4 PASS  exit=0  size=21  sha256=7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7
test_keyframe 8/0
```

**结论：T013 P2 的布局切换（source 缓冲改 allocated `frame_source`）未动 M1 冻结流，硬闸门 3 通过。**

### 17.3 recon 导出接口：00 亲验通过

00 用**公开头视角**（模拟 app，只 include `svt_av2.h`）编译并运行 T013 的 `probe_recon.c`（补 `output_path` 必需项）：

```
packet      : 21 bytes
too-small   : err=5 (OUT_OF_MEMORY) byte_count=6144 untouched=1   <- 容量契约正确
recon       : 6144 bytes, frame_index=0 64x64 (chroma 32x32)
recon uniq  : 78 distinct sample values                            <- 非平场
RESULT      : PROBE OK
```

**接口侧：T014 V3 的"无公共访问器"阻断已解除。**

### 17.4 ⚠️ 集成级发现：**码流不含任何系数**，故 decoder 端仍是平场

这是本轮最重要的一条，且**不是任何单个任务的缺陷**，而是 T011/T012/T013 的**接线未完成**。

**铁证 1（同 hash）**：4 种完全不同的输入（flat / gradient / checkerboard / noise）经现版 app 产出**同一** 21B 流、**同一** sha256。
**铁证 2（源码）**：`keyframe.c:46-51` 把 `y/u/v` 与 stride 全部 `(void)` 掉；`:145` 调 `svt_av2_tile_writer_write_closed_loop_key(&fcfg, ...)` **不传任何系数**；`tile_writer.c:79` 只写 `svt_av2_block_write_minimal_intra()`（固定最小 intra 块），**未调用 T011 的 `svt_av2_coeff_write_block`**。
**铁证 3（全树检索）**：`svt_av2_coeff_write_block` 仅出现在**自身定义**与 `test_coeff_syntax.c`，**无生产调用点**。
**铁证 4（hook 未接）**：`intra_frame.h` 的 `SvtAv2IntraBlockHookFn`（T011 取系数的唯一接缝）在 `intra_frame.c:158-166` 被调用，但 `encoder.c:encode_reconstruction()` **未设 `config.block_hook`**（全树只在测试里设置）。
**铁证 5（端到端对拍，00 亲跑 oracle）**：

```
avmdec --i420 --rawvideo 解 21B 流：
  flat   输入 -> MD5 9604569c8e5fcd812a940b82ef39b552
  noise  输入 -> MD5 9604569c8e5fcd812a940b82ef39b552   <- 两者相同
  decoder 输出 distinct 值 = 1（平场 128）
  对照 encoder 内部 recon distinct = 78（非平场）
  => decoder recon != encoder internal recon
```

**结论**：

- **P2 的"内部 recon 产出"已真实完成**（非平场、可导出、容量契约正确）；
- **P2 的"产生真实 frame core/tile data"（任务书 P2 原文）尚未完成**：帧 core 算出的 `qcoeff` 从未进入码流；
- **T014 V3 / T016 S3 的真实卡点 = 这条接线**，而不是"app 没有 `--export-recon` 开关"。`--export-recon` 只是把**已正确的内部 recon** 落盘；它无法让 decoder 侧从平场变成非平场。

### 17.5 接线归属与下一步（00 裁定）

- **T011 侧已 100% 就绪**：`svt_av2_coeff_write_block()`（`coeff_syntax.h:194`）+ `svt_av2_coeff_neighbours_reset()` + scan/EOB 全部交付并单测通过。
- **接缝已存在**：`SvtAv2IntraBlockHookFn`（`intra_frame.h:124-128`）就是 T011 取系数的官方入口。
- **需要三方各出一小步（00 协调，不新增公共 API）**：
  1. **T013**：在 `encode_reconstruction()` 里设 `config.block_hook`（+ `block_hook_user` 指向 tile writer 的 coeff 上下文），把 `qcoeff`/`plane`/`block` 交给 T011 的 writer；
  2. **T011**：提供"按遍历顺序累积到 tile payload"的入口（或确认 hook 直接调 `coeff_write_block` 即可，由其 `SvtAv2CoeffNeighbours` 维护块间 context）；
  3. **T011/T013 汇合**：tile payload 由 `svt_av2_tile_writer_write_closed_loop_key()` 输出，替换 `block_write_minimal_intra()`。
- **纪律**：这是**布局+寻址+recon 之后的第四批**，仍须满足 P2 三硬约束（加法优先 / 原子 / 每批后复跑 M1 冻结 21B；**本次必然改变 21B 流**，故 21B 闸门从"必须不变"转为"必须显式报告新流 + 由 avmdec 端到端闭环"，需 00 明确放行后才可动 M1 冻结值）。
- **禁止**：不得用"能编解码 / 能落盘 / exit 0"冒充正确性（全局禁令 8）。

## 18. P0 续跑：第四批接线属主终裁 + `--export-recon` 实测通过（00，2026-10-08）

> 触发：用户续跑指令。要求 P0 先实测核实跨属主接缝，**正式明确属主/契约再并行派工**，不得只给下游一项无上游接口的空接线。

### 18.1 00 当轮实测（非照抄）

| 判据 | 00 实测 | 结论 |
|---|---|---|
| M1 冻结基线 `artifacts\T005\frame_min_64x64.bin` | `7AAB5986…CE0F7`，21B | **MATCH**（未回退） |
| `tile_writer.c:109` | `svt_av2_block_write_minimal_intra(&enc, cfg->base_q_idx)` 仍在 | 接缝未接 |
| `svt_av2_coeff_write_block` 生产调用点 | Source/app 命中 **0**（仅定义+声明+测试） | 接缝未接 |
| `block_hook` 生产赋值点 | Source 命中 **0**（仅 `intra_frame.c:158` 读、`:151` 声明） | 接缝未接 |
| `svt_av2_coeff_literals_supported()` | 返回 **1**；`svt_av2_entropy_encode_literal()`（bypass L(n)）已存在 | 真实系数语法**可发**，前提成立 |
| `codec.h:137` | `SVT_AV2_MAX_ENCODABLE_FRAMES 1u` | 多帧未放开 |

### 18.2 `--export-recon` 修法 A 实测通过（用户第 4 点，00 亲跑）

- 重建 `build\00`（Release，`/WX`）：`main.c 8000D68F…` 的修法 A 已编入 `bin\SvtAv2EncApp.exe`。
- 命令（`checkerboard_64x64_1f_420.y4m`，`--frames 1`）：

| 运行 | exit | 产物 | SHA256 |
|---|---|---|---|
| 不带 `--export-recon` | **0** | `artifacts\00\m1_plain.av2`（21B） | `7AAB5986…CE0F7` |
| 带 `--export-recon` | **0** | `artifacts\00\m1_recon.av2`（21B） | `7AAB5986…CE0F7` |
| — | — | `artifacts\00\recon_64x64.yuv`（6144B） | `D67DA3B2…0863` |

⇒ **修法 A 成立**：exit 0、recon 6144B、**默认关闭时 M1 流逐字节不变**（两流同 hash）。
> 缺陷（`main.c:638 recon_buffer=NULL` 首调 `INVALID_PARAMETER`）由 00 定位、T013 按修法 A 修复，现已闭环。

### 18.3 属主终裁（00）：第四批接线

**依据**（磁盘现行文档，非记忆）：
- `docs\m1-m2-paths.md` §2.2：`Source\Lib\Bitstream\tile_writer.*` = **T011**；`Source\Lib\Bitstream\keyframe.*` = **T013**；`Source\Lib\Codec\encoder.*`、`codec.h`、`app\` = **T013**。
- `docs\m1-m2-contracts.md` §5：**Owner = T011**，范围明列 "**tile writer 输入**、context、容量"。

**更正**：T013 §9.41/§9.43 反复引用的"`keyframe.c` 属 T005 / `tile_writer` 属 T005"是 **M0–M1 旧属主**，在 M2 已重划。**M2 现行属主以上表为准**。此误引是第四批空转多轮的根因。

**裁定**：

| 改动 | 属主 | 性质 |
|---|---|---|
| `tile_writer.*` 增**块系数输入**；`:109` 改调 `svt_av2_coeff_write_block` | **T011** | 接口变更（其属主文件 + 契约 §5 范围） |
| `keyframe.*` 增系数参数；`encoder.c` 用 `block_hook` 采集 `qcoeff` 并按序驱动 | **T013** | 调用路径（其属主文件） |
| 帧 core 暴露"块序列"给 Codec 层（plane/tx/坐标/bw/bh/qcoeff/all_zero） | **T012** | 接缝契约（其属主文件，`block_hook` 已具备，需公告顺序契约） |
| 块级符号（`do_split`/`y_mode`/`uv_mode`）+ 系数 CDF context 按**实际邻居** | **T011** | 契约 §5 禁止一律 ctx12/ctx0/ctx6 |

**依赖方向合法**（已核 CMake）：`Codec(svt_av2_enc)` → `{Bitstream, Reconstruction, encoder_core}`；`Bitstream` → `Entropy`。故 **T013 的 `encoder.c` 是唯一能同时看到帧 core 与 tile_writer 的桥**；`keyframe.c`（Bitstream 库）只需接收已采好的系数，**不得**反向依赖帧 core。

**接口性质约束（00 定，签名由 T011 设计并公告）**：
1. 输入必须能表达**每个块的** `plane`/`tx_size`/位置/`qcoeff`/`eob_u_nonzero`，且按**帧 core 遍历顺序**写入。
2. 支持 64×64 单帧（3 块）与多 SB（如 176×144）；**边界 SB 按契约 §5 不写 `do_split`**。
3. 容量按 `out_cap` 推导，空间不足返回明确 `NO_SPACE`；**不得**保留 M1 的 32B 固定栈假设。
4. **保留 M1 冻结路径**：默认关闭时 `frame_min_64x64.bin` 21B **逐字节不变**（`7AAB5986…CE0F7`）。
5. 唯一 range coder；禁止第二套。

**顺序（打破互相等待）**：
- **T011** 今日先发 `[T011][CONTRACT]` 公告签名，再实现。
- **T012** 今日先发块序列顺序契约。
- **T013** **不等**第四批：先做 S4 多帧序列封装（不依赖第四批），同时按 T011/T012 签名接线。
- 三方**并行**，接口签名公告后即接线。

### 18.4 状态

- 00：第四批属主已终裁并派工；`--export-recon` 已实测闭环。
- T011/T012/T013：收到 `[TASK]`，进入 RUNNING。
- T014 V3 / T016 S3：**仍 BLOCKED**（依赖第四批接线后才有真实非零残差码流）。
- 未做项目一律标 **BLOCKED/未验证**，不写 PASS。

## 19. M2 最终验收 DONE（2026-10-10）

用户另行授权两个 Codex 子窗口完成 M2，主窗口负责调度和独立终验。真实 qcoeff → tile → AVM 解码 → 内部重建逐字节闭环、多帧/目标尺寸/API/CLI 已完成。§18.4 的 RUNNING/BLOCKED 为历史状态，M2 最终状态为 DONE。

16 项目标矩阵全部通过，连续 1000 帧和 ASan/UBSan/LSan 通过，MSVC/GCC/Clang Debug/Release 全部通过。完整证据、修正字段、范围和复验命令见 [最终主窗口验收](m2-codex-integration.md)。旧队列报告与中间失败产物保留用于追溯，不能替代该最终报告。

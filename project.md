> **2026-10-25 单图第三轮进行中**：用户授权4路队列，先追平AOM体积，再继续0.85目标。2D六点实测中，CfL/primary/RDOQ/rect/方形及小矩形角度组合比上轮版本省22.96%，仍比AOM大28.99%。新增长条矩形、双树完整工具搜索、矩形MRL及full TX已合入，45组最终CLI与固定AVM逐像素核对通过，组合六点和多平台/检测器继续。MRL轴方向越界已修复并保留反例；未达标，正式仍0.0.5-m5。见[第三轮记录](docs/task-reports/M6-intra-avm-tools-20261025.md)。

> **2026-10-24 当前 M6-I 第二轮，未达标**：单图体积优先，I-GATE 前不推进帧间。新增 64/32/16 完整 SB 候选及六种零偏移斜向亮度 IDIF，逐树做真实语法 R 与重建 D 的有界搜索；UV 保持七模式。MSVC/GCC/Clang 正确性及固定 AVM 强制码流已通过，七张开发/标定图132点质量测量已完整保存，检测器也已通过。20 张 holdout 未使用，正式仍0.0.5-m5，新功能默认关闭。合同 docs/m6-intra-contract.md，报告 docs/task-reports/M6-intra-partition-directional-20261024.md。

# svt-av2 项目记录（project.md）

> **2026-10-18本轮基础候选验收完成，M6总体RUNNING**：先基础后AVM新技术，00+A/B。R5/R6/R7三源原生1080p帧48..71、24帧/1.001s、四QP质量各12/12配对及00独立审计通过。独立运动27正式/9暖机、CDF18正式/6暖机全PASS，45条正式流匹配质量QP100。R5相对旧RD VMAF BD约−6.9/−2.0/−2.7%、墙时+17%至30%；R6收益近零、再+2%至8%墙时；R7仅RD-off同重建省约7%至20%、墙时+13%至17%。基础RD在2D/Real仍落后非RD，不能称基础已补齐或整体差距解决。192源/各版程序及旧长测76冻结项/配置最终复核一致，默认off、正式0.0.5-m5不变。14:00原计划任务/真实进程存活，唯一94/108质量任务PASS、0FAIL；后续重读。报告docs/task-reports/M6-basic-capability-assessment.md，总进展basic-capability-progress.md。新测量子进程均结束；原长测继续非整机隔离。

> **2026-10-18第四轮实际系数决策短验**：V1粗清零因等质退步否决，最终V2逐叶逐平面尾部非零AC降一级，lambda不变、默认off。00 Windows46/GCC45/SAN3与6组跨平台流/recon/trace、B51对+9off+30VMAF通过；真实三帧small/P冻结PCHIP VMAF BD为−0.481/−1.585/−1.988%，2D线性插值却+1.400%、Real PSNR退步，不能外推完整片或说解决8倍差距。报告docs/task-reports/M6-r4-assessment.md，外置optimization-r4-20261018/root/final-verification.json、176源清单。09:20左右原计划任务/总控17392仍real-quality，66/108 PASS、无FAIL，76冻结项及配置不变。正式0.0.5-m5，M6未最终验收。

> **2026-10-18后台托管修订**：前次总控11508在08:12:56因batch18600状态码1退出而FAIL，本次没有新系统重启；batch无异常栈，退出根因尚不能确定。完整质量已65/108 PASS（2D36、3D29）、76冻结项及全部保留结果复核通过。改用Windows手动计划任务SVT-AV2-M6-local-r2-20261017托管原配置，08:24:20 host21732/总控17392启动；无执行时限、无自动重试，不替换冻结程序。报告docs/task-reports/M6-background-host-20261018.md，正式仍0.0.5-m5。

> **2026-10-18系统重启恢复**：04:50:53意外关机、04:52:25启动（Kernel-Power41/6008，BugcheckCode0，底层原因不能确定）。恢复前独立核验完整质量64/108 PASS（2D36、3D28），pilot6/小图1512/筛查576及AV1既有3/fresh18均有效，76冻结项及配置SHA匹配；3个中断attempt保留重试。07:53:30原配置独立恢复，新总控PID11508，不替换冻结程序。报告docs/task-reports/M6-recovery-20261018.md，原大测未全完，正式仍0.0.5-m5。

> **2026-10-17 M6后续评估与第三轮短验**：同源2D直接支配点确认当前AV2 pruned100比质量更高的fresh AV1 CRF44大8.75倍；同码流提速不能解决体积。新增外置三帧诊断，流/recon保持冻结pilot指纹，系数语法占估计R约96.56/95.83/77.47%，根因仍需受控实验。量化峰值溢出预检等价优化已通过00 Windows45/GCC44/SAN专项3、B54流/AVM及18off/M5、00跨平台6例。报告docs/task-reports/M6-r3-assessment.md；原冻结长测继续，22:50为real-quality26/108 PASS，正式仍0.0.5-m5。

> **2026-10-17 M6第二轮优化短验通过**：00+A/B仅优化RD非法路径前缀、无用SAD和SB备份复制，170原源中只有rd_frame.c变化，新增两项分析/验证工具。00 Windows45/GCC44及sanitizer专项3通过，2319候选事件逐字节一致；B54完整流/recon/AVM及18条M5/off一致。合成测试中位时间约减少18.8%，不外推真实单路速度。报告docs/task-reports/M6-r2-performance.md，外置optimization-r2-20261017/root/final-verification.json。原独立长测只读保留，正式仍0.0.5-m5，M6最终验收未完成。

> **2026-10-17 M6本地原片续跑**：用户指定三组H264 source/AV1 p6crf36，预检383/378/390帧对齐。使用现full FFmpeg解码，不改编码器；新r2在C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017于北京时间17:15:34以PID5284独立启动。17阶段、2244AV2配置、AV1既有3点/fresh18点，自动审计及SVG表格。详见docs/task-reports/M6-local-campaign.md，正式仍M5，M6完整效果待验收。r1证据保留。

> **2026-10-17 M6验收分析优先**：r1后台于15:42:59因Boxing仅4.233333秒不满足5秒素材合同而FAIL，非编码器故障。前19/28阶段、20796任务/20916次编码独立审计通过；当前只完成机制/合规阶段性验收，真实视频和单路速度尚未执行，正式仍0.0.5-m5。已有828条BD全部N/A，不能宣称总体压缩收益。报告docs/task-reports/M6-review-20261017.md，机器证据artifacts/m6-review/partial-verification.json。本轮未重启长测；后续新r2复用已核验结果，先修素材时长合同并预检全部源。

> **2026-10-17 M6 RUNNING**：00+A/B完成有界RD代码、语法rate及API/CLI接线，必要短验Windows45/GCC44、12AVM短流通过。下一步按用户要求集中独立批测后暂停AI，真实素材/结果在C:\Private\ffmpeg-native-m6-media和C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017。状态/限制/恢复见docs/task-reports/M6-integration.md；正式仍0.0.5-m5，M6长测与最终效果验收PENDING。

> **2026-10-17 M5正式收尾：M5.1/M5.2/M5.3及M5 DONE，0.0.5-m5本地冻结。** 四平台各240项候选长测已通过；发布元数据差异反替换核验140源，新发布构建Windows40/Linux各39严格CTest和四平台各360项完整流/recon回归PASS。证据artifacts/m53-release/final-verification.json，正式源SHA f303fe31348c1b72bbc4d37417127e43e9fb35bc4b2a4884860a0ca2493654ca；发布目录dist/M5，报告docs/task-reports/M5-release.md。下一轮M6任务包在../M5-M6-AI-TASKS，当前仅写指南未开发。以下RUNNING、旧版本为历史记录；旧候选长测和M4 FFmpeg包保持原版本及证据。

> 2026-10-17清理：按用户要求工作区从112.5GB降至26.78GB。旧阶段大型测试媒体已逐文件验证并压缩到C:\Private\ffmpeg-native-history\20261017，原位JSON/日志/指纹保留；旧矩阵重跑前可用根tools/restore-av2-history.py恢复。当前long-campaign-r1/素材/快照均保留且13项hash匹配，长测未停止；源码候选清单未变。

> 2026-10-17恢复进展：采样器线程竞争已修复（模块级固定ctypes类型/签名），8线程16000次回归及13自测通过。原长测85项hash核对并带溯源迁移，08:01在artifacts/m53-root/long-campaign-r1恢复并出现新增PASS。原失败目录保留。新140源候选清单SHAb54581ec223e80b58c9021dfb1c83d200e2550b0b34a67c506cdd7c32ba639d8，所有编码程序不变；当前状态/最终收集以-r1目录为准，M5.3仍RUNNING。

> 2026-10-16 M5.3最新进展优先：已开放非无损CQP1..255（10bit实际+48）、外部VMAF、多点PCHIP BD-rate及独立断点批测。所有6120基本组合/18360帧固定AVM重建通过，正式仍0.0.4-m4；长测和正式冻结前不标M5 DONE。当前报告docs/task-reports/M5.3-integration.md，教程docs/M53-批量测试教程.md。未来模式只保留CQP与CRF，取消CBR/2-pass/VBR/VBV码率模式。

> 2026-10-16最新续行：M5.2最小内容自适应分区DONE，M5.3待做，正式仍0.0.4-m4。权威报告docs/task-reports/M5.2-integration.md；129源及5产物SHA在artifacts/m52-root/final-verification.json。下面M5.1及更早状态按历史时点阅读。

> 2026-10-16续行：M5.1 AI/P DC/V/H已完成独立终验，M5整体未完成；下一步M5.2内容partition及M5.3组合。当前权威入口docs/task-reports/M5.1-integration.md，124项源码与5项app/shared SHA在artifacts/m51-root/final-verification.json。此前审计/仅AI记录按历史时点阅读。

> 维护者：00 集成与调度会话。执行会话不直接改本文件，交报告后由 00 统一回写。
> 本文件只记录**已确定**的事实；未验证项必须显式标注“未验证”。
> 当前SVT-AV2为M4 0.0.4-m4 DONE，最新范围/产物以第32节与docs/task-reports/M4-integration.md为准；旧阶段记录保留历史。

## 1. 目标

从零实现 SVT-AV2 编码器的 M0（可构建、可验证的工程骨架）与 M1（自有实现生成可被固定 avmdec 解码的**最小单帧** AV2 码流），全程以 **AV2 v1.0.0 规范**为真值，不硬编码整段参考帧载荷、不使用外部编码器代工。

## 2. 范围与非目标

- 范围：M0 构建/测试/验证闭环；M1 最小单帧合法码流 + 逐平面 recon 比较。
- 非目标：性能、多帧、码率控制、多线程、M8 运行时抽象、完整工具集。

## 3. 路径与版本控制

- 工作根：`C:\Private\ffmpeg-native\deps-full\AV2`；新实现：`svt-av2\`。
- 只读：`avm\`（指向 `deps-full\avm` 的目录联接）与 `AVM-AV2-TEST\tools\bin` 下的 oracle。
- 当前**非 Git 仓库**；采用**共享目录 + 严格文件归属**模式（见第 7 节），基线以文件 SHA256 标识。若建立 Git，范围仅限 `svt-av2\`。

## 4. 环境与基线

- 语言：C11（`CMAKE_C_EXTENSIONS OFF`），无第三方运行时依赖。
- 生成器/编译器：Ninja；Windows MSVC x64（已实测 19.44），Debug+Release；Linux GCC/Clang（**未验证**，本机无环境）。
- 构建前必须 `. C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1`（注入 MSVC/cmake/ninja）。
- 目标：`svt_av2_common`、`svt_av2_enc`、`svt_av2_enc_shared`、`SvtAv2EncApp`；单测经 CTest。
- 规范：AV2 v1.0.0，本地 `docs\spec\AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，SHA256 `E9916F09…B533F`。
- 源码编码：UTF-8（无 BOM）。

## 5. 当前状态

- **M0：Windows 部分已验收（00 独立复现）**。
  - T001 骨架：`cmake -S svt-av2 -B svt-av2/build/00/{Release,Debug} -G Ninja` → build → ctest 均 **exit 0**，CTest **3/3 passed**（test_common/test_y4m/test_api）；共享库导出 9 个 `svt_av2_*` 公共符号；应用退出码契约实测 0/2/3，写开发容器 exit 20。**已置 DONE**。
  - T002 M0 验证基建：CTest **7/7 passed**（test_common/test_y4m/test_api/corpus_manifest/benchmark_schema/runner_self_test/runner_oracle_smoke），00 独立复现 exit 0。oracle `avmdec` 1.0.0；runner 以**帧数+精确字节数**判定，实测 avmdec 对截断码流可能 exit 0 但 0 帧，已不误报 PASS。**已置 DONE（M0 部分）**；M1 闭环接入 = TODO，待 T005。
  - T006 比较工具：25/25 单测 OK，契约 §9 逐项一致，哈希与报告一致。**已置 DONE**（真实 recon 比较待 T005 产出后再记）。
  - `tests\CMakeLists.txt` 基线 SHA256 `EB55F0DEBA15C4FAC25F69B8A43494CFC16AC52B806616F5C9BECC1BFE9DF9B9`。
  - 待办：T002 的 corpus/runner/benchmark 完成后补齐 M0 验证闭环；Linux **未验证**。
- **M1：进行中**。
  - T003 writer/OBU：**已置 DONE**。接口冻结于 contracts §5；`Source\Lib\Bitstream\{bit_writer,obu_writer}.*`；Debug/Release 单测绿（随机位/LEB128 与独立参考模型交叉比对）。
  - 集成（00）：根 CMake 已注册 `Source/Lib/Bitstream`；`sequence_header.c` 已由 00 加入模块 CMakeLists（模块现编译 bit_writer/obu_writer/sequence_header）。
  - **集成修复（00，2026-10-06）**：T003 与 T004 各自实现了 `uvlc`，链接出现重复符号（LNK4006）。00 裁定位写入原语（`uvlc`/`ns`）归 `bit_writer.{h,c}` 唯一实现，已迁 `ns`、删除 `sequence_header.{h,c}` 重复定义；全新构建 `build\00\M1b` exit 0、无 LNK4006。详见 contracts §5。
  - **集成修复（00，2026-10-06）**：T005 发现 AVM 在 `single_picture_header_flag=1` 时要求首帧为 `OBU_CLOSED_LOOP_KEY`（oracle `decodeframe.c:7606-7608/8015`），而 T003 OBU 白名单缺 CLK。00 已把 `SVT_AV2_OBU_CLOSED_LOOP_KEY` 加入 `obu_writer.c` 白名单（记 contracts §5），构建 exit 0、CTest 11/11 无回归。
  - **集成（00，2026-10-07）**：T005 的 `frame_header.c` 已加入 `svt_av2_bitstream`；新建 `Source\Lib\EntropyCoding\CMakeLists.txt`（target `svt_av2_entropy`）并在根 CMake `add_subdirectory`（置于 Codec 之前）。全新 configure/build `build\00\M1b` exit 0（零警告/零 LNK），CTest **11/11 Passed, exit 0**。
  - **集成修复（00，2026-10-07）**：T001 将 qp 校验从 [0,63] 放宽到规范 8-bit 的 [0,255]（contracts §6），`test_api.c:201` 旧断言（qp=64 期望拒绝）随即失败；T001 已改为 qp=0/255 接受、256/-1 拒绝，CTest 恢复 11/11。
  - **T002 M1 验收入口已由 00 独立复测接受**：`python tools\run_matrix.py --self-test` → `SELF-TEST PASS` exit 0；三例判别正确（截断→FAIL decoded_frames=0；篡改→FAIL exit1；完整→PASS 6144B/1 帧）。oracle 版本串：`avmdec -v`/`avmenc -v` = "AOMedia Project AV2 ... 1.0.0"。
  - T005 已产出 `docs\bitstream-frame-minimal.md`（E1）、`frame_header.{h,c}`、`EntropyCoding\entropy_encoder.{h,c}`、`cdf_tables.{h,c}`、`block_writer.{h,c}`、`tile_writer.{h,c}`；E2/E3 完成并入构建。
  - **M1 闭环（00 独立复测）PASS（2026-10-07）**：首个自有码流 `t005_m1.av2`（21B，SHA256 `7AAB5986…CE0F7`）经固定 `avmdec` → exit 0、**恰好 1 帧、raw 6144B**（=64*64+2*32*32），同配置 3 次编码 SHA256 一致；解读码 Y/U/V 全为 128（DC_PRED 零残差，语义正确）。详见 `docs\task-reports\integration.md`。
  - **待办（M1 收尾）**：E4 应用 `--output` 已由 00 独立验收（exit0、21B、6144B、负例 exit11）；T005 的 `test_tile_writer`/`test_frame_header`/`test_keyframe` 已注册进 CTest，**14/14 Passed, exit 0**。M1 全闭环（应用 → 自有码流 → 固定 avmdec 验收）**已闭合**。
  - T004 `sequence_header.*`：**00 独立复现验收通过（2026-10-06）**。Release+Debug 全新构建 exit 0，CTest **11/11**（`test_sequence_header` 74 checks / 0 failures）。00 用依 AVM 解码器字段序（`avm/av2/decoder/obu.c:541-611`）自写的独立 Python 模型 + 直链 `svt_av2_bitstream.lib` 的 C 采样双重核对，176x144/64x64 向量逐字节一致；发现并纠正 T004 测试向量 byte[1] 误用 level0（level1→0x1A、level4→0x4A）。集成期修复：uvlc 重复符号、模块 CMake 缺 `sequence_header.c`、测试 `main()` 悬空调用。**T004 交付物置 DONE（M1 序列头子集）**，待 T004 补报告。
  - T005 负责 frame header/tile/entropy 与 M1 应用闭环。**M1 闭环（自有码流 → avmdec 成功、恰好一帧、字节数一致、3 次 SHA256 一致）未完成。**
- 契约：`docs\m0-m1-contracts.md`（已发布，含文件归属、构建/测试、API、raw 比较、规范真值）。

## 6. 已实现能力 / 已知限制

已实现（M0）：库/应用/单测构建；Y4M 元信息读取（拒绝 C420p16 等不支持格式，exit3）；API 状态机与非法参数处理；共享库符号导出。
已知限制：
- 未实现任何编码；`get_packet` 返回 `has_packet=0`，M0 **不生成 AV2 码流**。
- 应用 `--output` 写出的 `SVTAV2M0`/魔数 `SVT2M0` 容器是**非 AV2 开发产物**（保留既有代码，供早期联调），不得被判 PASS，也不得演变为 public API。
- Linux 构建、M1 全闭环：未验证。

## 7. 会话与文件归属

- 00 集成/调度；T001–T006 执行。花名册 `scripts\session-roster.json`；通信机制见 `AGENTS.md` 第 7 节，工具 `scripts\oc-msg.ps1`（默认 `delivery=queue`）。
- 文件归属与串行合入规则见 `docs\m0-m1-contracts.md` 第 7 节；公共文件（根 CMake、公共头、project.md、AGENTS.md）由 00 串行处理。

## 8. 验证方法

- 构建/单测：`cmake -S svt-av2 -B svt-av2/build/<task>/<cfg> -G Ninja` → `ninja` → `ctest`。
- M0：应用读 Y4M 元信息、错误码、共享库导出；oracle 版本/hash/smoke 记录（T002）。
- M1：自有码流 → 固定 `avmdec` 成功、恰好一帧、raw 字节数与配置一致；runner 同时校验退出码/帧数/尺寸/长度；同配置重复 3 次 SHA256 一致；截断/失败判负；“缺工具”不得 PASS。
- **M1 验收实际结果（00，2026-10-07）**：`python svt-av2\tools\run_matrix.py --check-bitstream --bitstream svt-av2\artifacts\T005\frame_min_64x64.bin --width 64 --height 64 --frames 1` → exit 0、1 帧、6144B；3×SHA256=`7AAB5986…CE0F7`；应用产出的码流与之逐字节一致。回归脚本 `python svt-av2\tools\check_m1_stream_determinism.py --build svt-av2\build\00\M1b --expect-sha256 7aab…ce0f7` → PASS。详见 `docs\task-reports\integration.md`。

## 9. 下一步

> 2026-10-07：模型恢复额度后，00 已向 T001/T002/T003/T004/T005 各发一条续跑指令（队列投递），五个会话均已开始处理。
> M1 目标收缩：**64x64、8-bit、4:2:0、单帧、单 64x64 超块、All-Intra、固定 QP**（64x64 单超块，熵编码/块数最少，corpus 已有确定性样本 raw=6144B）。
> 2026-10-07 进展：M1 编码配置已冻结（contracts §6，含 base_q_idx=100 依据）；T005 的 frame_header/entropy_encoder 已被 00 接入构建；T002 的 M1 验收入口（run_matrix --check-bitstream）已独立复测接受；基线 CTest 11/11 绿。
> 2026-10-07 **M1 单帧码流闭环 PASS**：T005 产出 `t005_m1.av2` → 固定 avmdec exit 0、1 帧、6144B、3×SHA256 一致（`7AAB5986…CE0F7`）；见 `docs\task-reports\integration.md`。剩余：E4 应用接线、T005 单测注册、gen 工具落盘。

1. **T005（关键路径）**：写 `docs/bitstream-frame-minimal.md` → 实现 frame_header/tile_writer/最小熵编码（先只支持 64x64）→ 应用侧读 64x64 Y4M 写自有 AV2 elementary stream → 产出首个 `TD+SEQ_HEADER+CLOSED_LOOP_KEY` 单帧码流。
2. **T001**：应用 `--output` 对受支持 64x64 单帧 Y4M 产出真实码流并 exit 0；未支持格式非零退出；`qp` 校验放宽至 [0,255]（契约 §6）。
3. **T002**：`--check-bitstream` 作为 M1 验收入口写进 `docs/validation.md`（64x64→6144B/1 帧）；备 M1 正/负例（截断判 FAIL）；补 oracle `-v` 版本串；记录 .gitignore *.y4m 决策。
4. **T003**：补 CLK OBU 头/长度前缀单测 + trailing_bits 向量复核。
5. **T004**：与 T005 冻结 sequence-header 入参（尺寸/SB/level/seq_header_id）并给依据。
6. 00：T005 交付码流后运行 `run_matrix.py --check-bitstream` + 3 次 SHA256 一致性，出 `docs\task-reports\integration.md` 并回写本文件。

> **2026-10-07 结项（00）**：M0 + M1 全部达成并闭环。T001/T003/T004/T005/T006 均 **DONE** 且空闲。
> - 构建 `cmake --build svt-av2/build/00/M1b` → exit 0；CTest **14/14 Passed, exit 0**。
> - 自有码流 `artifacts\T005\frame_min_64x64.bin`（21B，SHA256 `7AAB5986…CE0F7`）→ 固定 oracle `avmdec` exit 0、1 帧、raw 6144B、3×SHA256 一致；应用 `SvtAv2EncApp --output` 与之逐字节一致。
> - 文档闭环：`docs\validation.md` **§3.2 M1 实测结论（2026-10-07）**（T002 恢复后亲自写入；00 代填的临时段落已合并去重，补入 3×确定性/应用闭环/负例证据）。
> - **T002 队列已结清**：其 16 条排队消息逐条核对=14 条 M0 历史重放 + 2 条要求补 `validation.md` 的指令；**T002 最终恢复并亲自完成**该指令，重复段落已去重。内容层面无待办。
> - 后续可选方向：多帧 / 残差编码 / 更多尺寸 / Linux 构建验证——待用户指定。

## 10. 当前状态核对与 M1→M2 任务书（2026-10-07，Codex 独立复测）

本节为截至此次检查的当前摘要；上文“未实现编码/M1 未完成/Linux 未验证”等早期内容保留为历史记录，不作为当前能力判断依据。

**当前摘要（2026-10-07，00 冻结）：受限 M1 已复现；M2 首批模块已进入构建，但 M2 编码路径尚未接通。**
- 受限 M1 已独立复现：64×64、8-bit420、单帧All-Intra/DC_PRED/零残差、QP100，21B码流固定SHA256 `7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`，真实decoder输出1帧/6144B/各plane全128。
- **M2 进展（00，2026-10-07，见 §14）**：T007 `picture`/`block_geometry`、T008 `intra_prediction`、T010 `quantization` 已由 00 接通 CMake，**首次真正进入构建**；`build\00` Release 下 CTest **19 项注册 / 17 Passed**。仍失败：`test_api`（T013 NULL=EOS 契约改写未完成）、`test_quantization`（T010 真实数值缺陷）。T009 `Transform\` 与 T012 `Reconstruction\` 仍为零代码。
- **通道回退**：DSH 通道已全部移除，恢复 OpenCode HTTP（`scripts\oc-msg.ps1` + `scripts\session-roster.json`）；十队列已重派。
- 已复现的过渡缺口：public `send_picture`/`get_packet` 仍不出 packet（`get_packet_ex` 已落地公共头，待 T013 完成测试改写）；`frames` 语义、runner 宽高误报归 T013/T014。
- 下一阶段范围已由用户指定为M1→M2：保持8-bit420/单线程/固定QP，以scalar像素编码、真实系数syntax、内部recon逐像素一致、多尺寸/多帧和稳定性为目标。任务包为 `../M1-M2-AI-TASKS/`。
- 原始命令、退出码、码流/recon及源码快照：`artifacts/audit-2026-10-07/summary.json`、`source-sha256.json`及各构建/decoder日志。独立检查报告见 `../M1-M2-AI-TASKS/01-M0-M1检查报告.md`。

### 11. M1 事实复核与 M2 起点冻结（00，2026-10-07）

00 用**全新构建目录** `build/00/M2-g0` 独立复跑，未复用任何历史缓存；结果与检查报告一致：

| 项 | 复跑结果 |
|---|---|
| configure/build（Release，MSVC 19.44.35228.0，Ninja，WARNINGS_AS_ERRORS=ON） | exit 0 |
| CTest | **14/14 Passed, exit 0** |
| 应用编码 `gradient_64x64_1f_420.y4m` | exit 0，21B |
| 码流 SHA256 | `7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7`（= 冻结基线） |
| runner `--check-bitstream --width 64 --height 64 --frames 1` | exit 0，`PASS: ... decoded to 6144 bytes (1 frames)` |
| 固定 `avmdec` 直跑 | exit 0，1 帧，6144B；Y/U/V 全部样本**恰为 128**（各平面 4096/1024/1024 个样本求和分别为 524288/131072/131072） |

证据目录：`artifacts/00/m1-recheck/`。**该复跑不改变"输入像素未参与编码"的事实**——gradient 输入与 flat 输入产出同一码流，正是 M1 的文档化行为。

### 12. 平台验证矩阵（按平台如实列出）

| 平台 | 编译器 | 配置 | 构建 | CTest | 应用产物 | 状态 |
|---|---|---|---|---|---|---|
| Windows x64 | MSVC 19.44.35228.0 / Ninja | Release | PASS | 14/14 | 21B，固定 hash | **已实测** |
| Windows x64 | 同上 | Debug | PASS | 14/14 | 21B，同 hash | **已实测** |
| WSL1 Ubuntu 22.04 | GCC 11.4.0 / Unix Makefiles | Release | PASS | 13/13 | 原生 Linux 应用，21B，同 hash | **已实测**（见 §13） |
| WSL1 Ubuntu 22.04 | 同上 | Debug | PASS | 13/13 | 原生 Linux 应用，21B，同 hash | **已实测**（见 §13） |
| Linux 原生（非 WSL） | — | — | — | — | — | **未验证**：本机无原生环境 |
| Clang | — | — | — | — | — | **未验证**：Clang 不可调用 |

注：Windows 为 00 本次全新复跑；GCC 两项来自 `01-M0-M1检查报告.md` 记录的实跑结果，00 本次**未重跑**（WSL 通道未在本次会话中验证），故记"已实测（第三方会话记录）"，**不记为 Clang 也通过**。Linux 未注册 `runner_oracle_smoke`，13 项是实际注册数；Linux 编码产物另由 Windows 固定 decoder 验收，**不能**据此宣称 Linux 原生 oracle 已部署。

### 13. M1→M2 契约与派发（00，2026-10-07）

- `docs/m1-m2-contracts.md`：冻结 T007–T016 之间的最小接口（plane/stride/边界、预测与 residual 类型、transform 尺度、qcoeff/dqcoeff/EOB 归属、syntax 与 context、recon 导出、packet 生命周期、headers、验收入口）。
- `docs/m1-m2-paths.md`：冻结十条执行队列的**文件归属**、基线 SHA256、已完成的交接裁定（T013→T015 的 sequence/frame header；T014→T016 的平台/长跑）与 G0–G4 门槛。
- 十队列 T007–T016 已同时派发；00 按小闭环受理，不等全部完成才集成。M2 实现**尚未开始**。

### 十条执行队列调整（2026-10-07）

按用户可用资源，将M1→M2执行包调整为T007–T016共10条执行队列，00集成会话另计。新增T015负责sequence/frame headers、多尺寸/多帧语法，从T013分出；新增T016负责跨平台/sanitizer/1000帧稳定性，从T014分出。T013保留API/应用循环/keyframe接线，T014保留runner/recon矩阵。任务包现有13份Markdown，已同步文件归属、依赖、首轮独立工作与已开工会话的交接要求；AGENTS启动协议按阶段选择任务包。未修改运行期花名册或编码器实现。

### 14. 通道回退 OpenCode + P0 集成修复 + 十队列重派（00，2026-10-07）

**通道**：DSH headless 通道已按用户要求**全部移除**（工具 `scripts\dsh-session.ps1`、`sent-log-dsh.jsonl`、`usage.jsonl`、`bus\`、`session-rename-checklist.md`、`_task-00-resume.txt`、`docs\dsh-*.md` 均已删除；`AGENTS.MD` 第 7/8 节重建为 OpenCode HTTP 通道）。现行通道只有 `scripts\oc-msg.ps1` + `scripts\session-roster.json`（OpenCode `ses_...` id，11 个角色已重建）。

**P0 集成修复（00 亲自实测，全新目录 `build\00`，Release，`SVT_AV2_WARNINGS_AS_ERRORS=ON`）**：

| 缺陷 | 处置 | 实测 |
|---|---|---|
| T007/T008/T010 的代码**从未被编译**（CMake 静默缺席） | `Common\CMakeLists.txt` 补 `picture.c`/`block_geometry.c`；新建 `Quantization\CMakeLists.txt`；根 CMake `add_subdirectory(Prediction)`、`add_subdirectory(Quantization)`；`svt_av2_common` 的 `include/` 改为 PUBLIC | 三模块首次进入构建，`picture.c.obj`/`block_geometry.c.obj`/`intra_prediction.c.obj`/`quantization.c.obj` 均产出 |
| T013 阻塞：公共头缺 packet 接口 | 在 `include\svt_av2.h` 落地 `SvtAv2Packet` + `svt_av2_enc_get_packet_ex`；删除 `codec.h` 暂存块；契约 §7.1 登记语义 | `app\main.c`（已只依赖公共 API）编译通过 |
| NULL=EOS 与 `test_api.c:147` 冲突 | 裁定采纳契约 §7（NULL=EOS，返回 NONE），测试源按新契约改写归 T013 | 已记入契约 §7.1 |
| T010 测试常量截断在 `/WX` 下报 C4310 | 最小修复：经非 const 局部变量传递（保留原断言意图） | 编译通过 |

**测试基线**：`ctest` 注册 **19 项，17 Passed**。两项失败为真实缺陷，已派回责任人：
- `test_api`（473 checks / 66 failures）→ T013，NULL=EOS 契约改写的连带效应，含伪 picture 需按真实契约改写。
- `test_quantization`（2179 checks / 10 failures）→ T010，qlookup 表值与 q/dq 幅度（`q==1` 得 4、`dq` 差 4 倍），属实现或期望值需按 AVM 依据判定。

**十队列重派（2026-10-07）**：T007–T016 全部按当前事实重新派发（`delivery=queue`，不打断）。派发书含 00 已完成的集成事实、各自的实测失败项与硬性边界，避免重复劳动与误判回归。T009/T012 为零代码最长路径，T015 为首次启动。

---

### 15. M2 第二轮：全模块注册（统一构建 23/23）+ tx_size 裁决（00，2026-10-07）

**全模块注册（00 亲自实测）**：T009/T012 的模块此前只在磁盘、未进构建。00 完成：

| 改动（公共文件，归 00） | 内容 |
|---|---|
| 根 `CMakeLists.txt` | `add_subdirectory(Source/Lib/Transform)`、`(Source/Lib/Reconstruction)`、`(Source/Lib/Encoder)` |
| `tests\CMakeLists.txt` | 注册 `test_transform`、`test_reconstruction`、`test_intra_frame`、`corpus_matrix` |

实测（`build\00`，MSVC 19.44 / Ninja / Release / `WARNINGS_AS_ERRORS=ON`）：build exit 0 零警告；
**CTest 23/23 Passed，0 Failed**（19 → 23）。T010 的 `test_quantization` 10 项失败、T013 的 `test_api` 旧断言均已由责任人修复。

**tx_size 裁决（决定 M2 首版编码路径，00 亲自溯源 AVM）**：T009 提议的"8x8"前提不成立。冻结帧头
`tx_mode_select=0`（`frame_header.c:237-240`）→ `TX_MODE_LARGEST`（`common_data.h:703-706`），顶层 64x64
`PARTITION_NONE` 经 `tx_size_from_tx_mode`（`blockd.h:2580-2589`）得 **luma TX_64X64 / chroma TX_32X32**
（`common_data.h:421-448`），与 M1 `block_writer.c:55-56` 的 `txSzCtx 4/3` 一致。裁决采用**路径 A**：保持
`TX_MODE_LARGEST`，零新语法；64x64 逆变换实为 **32x32 核 + 2× 复制上采样**（`idct.c:648-649,687-688,730-751`），
`tx_scale` = 2(luma)/1(chroma)（`idct.c:761-765`）。已写入契约 §3/§4 并抄送 T009/T010/T011/T015。

**其他裁决**：`cmake/SvtAv2Oracle.cmake` 交 T014 最小修复（`:26` 无条件清零遮蔽 `-D`，已复现）；
T016 的 S3 唯一判据 = `--recon-matrix`；T015 的 A1/A2/A3 假设全部采纳。

**仍未接通**：M2 编码主链（像素→预测→残差→变换→量化→重建→coeff syntax→packet）与内部 recon 导出
（T014 V3 / T016 S3 均 BLOCKED）；多帧（≥2 明确拒绝）、720p/1080p 未实现。**不得**因长跑不崩就宣布 M2 通过。

---

### 16. tx_size 契约二次收敛 + 真 bug + cmake 修复并入（00，2026-10-07）

**00 首版裁决书被 T009 带 AVM 出处顶回，00 逐行复核后全部采纳**（不盲从、以文件为准）：

| 00 原写 | 事实 | 依据 |
|---|---|---|
| `max_eob(TX_64X64)=4096` | **1024** | `avm\av2\common\blockd.h:3477-3490` |
| 64x64 = 32 核 + 2× 复制（外推到**正**变换） | 正变换是 **64 点核 + 置零 + repack** | `avm\av2\encoder\hybrid_fwd_txfm.c:952-953,955-956,939,984,1003-1008` |
| TX_MODE_SELECT 下 64x64 不允许再分 | 可再分 | `avm\av2\common\av2_common_int.h:5058,5198-5214,5193-5196` |

**裁决 (甲)（已写入契约 §3）**：`coeff`/`qcoeff`/`dqcoeff` 长度 = `min(w,32)*min(h,32)` = `av2_get_max_eob(tx_size)`；
TX_64X64 → **1024**（32×32 网格）。正变换出口 repack（`hybrid_fwd_txfm.c:1003-1008`）、逆变换入口
`min(1024,…)`（`idct.c:705,708,712-716`）、码流 `max_eob` 特判三方自洽。

**该裁决直接抓出一个真 bug（T010 第 11 项，已修）**：正向量化 `svt_av2_quantize_coeff_idx()` 漏掉
`>> log_scale`（`av2_quantize.c:118-121`）。`log_scale=0` 时恒等（≤16x16 曾全绿），但 M2 首版真实尺寸
（TX_64X64/TX_32X32）下 `dqcoeff` 偏大 2×/4×。**不补 32/64 锚点该 bug 会带进 M2。**

**T014 cmake 修复（00 独立复验通过，已并入）**：`cmake\SvtAv2Oracle.cmake` 先捕获 `-D` 值再收集候选，
修复 `:26` 无条件清零遮蔽 `-D` 的问题。00 自建探针目录实测：带 `-D` → fakeoracle，不带 → 默认路径。

**00 独立复验**：`build\00`（Release / MSVC 19.44 / Ninja / `/WX`）build exit 0 零警告；
**CTest 23/23 Passed，0 Failed**（含 T014 新注册的 `corpus_matrix`/runner 测试）；
Linux GCC 11.4.0（WSL 全新目录）configure/build 零告警、**CTest 22/22 Passed**（`runner_oracle_smoke` 因无 oracle 不注册）。

**验收记账（00 亲自复跑，不采信自报）**：T007 = **DONE**（`test_picture 30837/0`、`test_block_geometry 202/0`）；
T008 = **DONE**（`test_intra_prediction 1740891/0`、`test_residual 3458277/0`）；
T010 = **DONE**（模块级，`test_quantization 2846/0`；独立溯源 `qlookup(100)=704`）。
T012/T013/T014/T016 均含未完成阶段项，保持 READY_FOR_INTEGRATION。T013 曾被 00 误置 DONE（未逐字读 DoD 的 P1–P4），已撤回。

**由裁决链新增的防漏条款（写入契约 §3/§4）**——共同特征是"2 的幂次移位在 `tx_scale=0` 时恒等、扩到 32/64 才暴露"：

| 条款 | 内容 | 不登记的后果 |
|---|---|---|
| coeff 长度闭式 | `count = min(w,32)*min(h,32)`（对全 25 个 TX size 与 `av2_get_max_eob` 逐项 25/25 全等） | 扩 TX_16X64 会漏（应 512） |
| 承载网格 ≠ tx_size | TX_64X64 量化须用原始 `(64,64)` 初始化（`tx_scale=2`），网格 32×32/count 1024 不改 `tx_scale` | `tx_scale` 掉 1 → `dqcoeff` 偏大 2× **且不报错** |
| 降尺度归属 | 降尺度由量化器做，transform 出口不做（例外：lossless 升尺度、IST 能量估计） | 系数整体偏大 `2^tx_scale` |
| 舍入顺序 | 先 `ROUND_POWER_OF_TWO(level*704,3)` 再 `>> log_scale`，不可交换 | 32/64 下偏大 2×/4× |
| LEVEL_OVERFLOW 浣欓噺 | 閫愬昂瀵?**16x16 22.1x / 32x32 11.0x / 64x64 5.5x**锛堝疄娴嬪彛寰勶紝00 缁堣 2026-10-07锛夛紝**涓嶅緱澶栨帹** | 鐢?22x 鎴?T010 鐨?5.6x 璇垽瀹夊叏杈圭晫 |
| scan 表长度 | TX_64X64 在**所有 tx_type** 下用 `default_scan_32x32`（1024 项）——AVM 注释"tx64 高频半数为零，故用 tx32 的 scan 序"（`scan.c:1804-1806,1888-1890`） | 按 luma 4096 建 scan → 与 `max_eob`/repack/逆入口全部不自洽 |
| 行号引用 | `tx_mode_select=0` 实际在 `frame_header.c:245-248`（`237-240` 是 `deblocking_filter_params` 的两个 0 位） | 引用错位，后来者找不到冻结位 |

> 上表 7 条**全部是"扩到 32/64 才暴露"的静默错误**（前 5 条为 2 的幂次移位类，后 2 条为 AVM 64 族特判类）。共性教训：**≤16x16 全绿不能外推到 32/64**——这正是 M2 首版必须补 32/64 锚点的价值。

**第三轮（00，2026-10-07 后半）——注册与验收口径**：
- `test_header_frames`（T015）由 00 注册 → **Release 24/24、Debug 24/24**。**Debug 树此前未 reconfigure、缺该 target**（T015 提醒发现）；00 已补跑，且 Debug 出流亦为冻结 21B `7AAB5986…CE0F7`（配置无关）。
- T014 `--matrix-tamper-geometry` 负例 00 独立复现：32/32 拒绝（exit 0）、仅方形选择 exit 1、缺 `--matrix` exit 2。价值在于**判定来源是 decoder 自身几何**（`decoder geometry 176x144 != declared 144x176`），而非长度推导式——面积保持的宽高互换正好是旧推导式的盲区。
- **00 自我更正**：曾把 T013 误置 DONE（只读 `test_api`+出流，未读 DoD 的"P1–P4 分别有回归证据"），已撤回为 READY_FOR_INTEGRATION。**置 DONE 前必须逐字读任务书 DoD 全文**，不得按已见证据推断范围。

**通道问题**：T009 部分出站消息编码损坏（payload 即坏字节，不可还原）；改用 `-MessageFile` 后正常。
T007 报告曾因 `Set-Content -Encoding UTF8` 二次编码损坏（仅记录层；六代码 hash 未变），已按证据链重建并复验
（最终版 **68996 B / 1158 行 / 26 节 / U+FFFD=0**，`FE160707…9EAB5A`），DONE 维持。
**注意**：00 首次抓取到的是 3735 B 的**中间态**（T007 两次写入之间），已由 T007 更正——**验证文件状态须 size/行数/时间三点同看**，单看 U+FFFD=0 会被中间态误判为完成。
纪律重申：写 UTF-8 中文一律 `[IO.File]::WriteAllText(..., UTF8Encoding($false))`；长消息一律走 `-MessageFile`，勿内联中文。

### 17. 队列积压事故：7 会话被未答表单冻住（00 亲查 · 2026-10-07）

**现象**：00 会话的服务端队列积压 **103 条** 07:03–09:00 的历史消息，且每跑一个回合被喂一条（计数 107→106→105→104→103）。按此将烧掉上百回合重复处理早已闭环的报告。

**根因（00 逐层排查）**：

1. **不是通道坏了**：`GET /api/session/{id}/inbox` 文档原文是 "durable enqueued session work **not yet delivered**, ordered by enqueue sequence"——这些消息**从未真正投递过**，只是排队。
2. **读取不消费**：`GET` 是只读视图（我连读三次计数稳定 103），只有目标自己跑回合才消费。
3. **真正的根因**：**7 个执行会话（T008/T009/T010/T011/T012/T013/T016）全部停在一个未结算的 `question` 表单上**——其消息列表**最新一条**就是 `question` 工具调用（`executed:false, status:"running"`）。会话被冻住 → 我发的所有裁决堆在它们队列里**投递不进去** → 队列只增不减。
4. 对照 `/api/session/active`：8 个 "running" = **00 + 上面 7 个**（T007/T014/T015 不在，队列 0，因为它们没卡表单）。

**处置**：
- **回答 7 个表单**（`POST /api/session/{id}/form/{formID}/reply`，体 `{"answer":{"q0":"<value>"}}`）→ 7/7 成功，`form` 计数全部归 0，会话立刻恢复（T007/T009 随即在 10:31 活跃）。
- **00 队列取证后清空**：103 条**全部**在 `scripts\sent-log.jsonl` 可回溯（103/103 命中），且已 `-Dump` 落盘（171341 字符 / U+FFFD=0）→ `DELETE /api/session/{id}/inbox/{id}` 取消 103/103（0 失败）。
- 新增 `scripts\oc-inbox.ps1`（`-Status` / `-Dump` / `-Drain -Confirm`），并把机制与处置纪律写入 **`AGENTS.MD` §7.6**。脚本内置兜底：`pendingForms>0 → SKIP`，**不允许**用"删队列"代替"答表单"。

**关键教训**：
- **未答表单会静默冻结整个会话**，且症状与"通道故障"极像（消息投不进、计数不降）。**排查顺序应是：先查 `form`，再查 `inbox`。**
- 排空前**必须** dump + 确认 sent-log 可回溯；**只排历史积压**，绝不排承载未消费裁决的队列。
- 判别"陈旧重投"用**三者同看**：`sent-log` 有无同文、会话 `time.updated` 是否近期、交付物 hash 是否变——勿按内容新旧推断。

**本轮连带纠正（因积压而延迟到达的过时报告）**：
- T010 报"工作区被清空"→ **误报**（其自身路径读错 dash/backslash；00 实测 `svt-av2` 递归 **11758** 条目、`Source\Lib` 9 模块、`T010.md` 52KB 全在）；T010 已自行撤回。
- T008/T013 报 `m1-m2-paths.md:68` y4m 基线陈旧 → **活文件已是正确值**（`1C3BE2AF…DFE39`）；陈旧值是 `archives\m2-inflight-*\` 下的**历史快照副本**；T008 已自行更正（"是我读文件的方式错了"）。
- T012 报"请 T009 补 32/64" → **误报**，其读的是 `transform.h:102-104` 的**过期注释**；T009 的 32/64 早已实现，T012 已撤回。**00 独立复跑确认**：`build\00` Release 重建 exit 0、CTest **24/24**、`test_transform` **1177889 checks / 0 failures**（`transform.c:97,186,206-207,251-253,623-638`）。

**本轮 00 新裁决**：
- **T011 (A) 批准**：为 `entropy_encoder.{h,c}` 增加裸位（L(n)）接口。00 独立证实必需——`avm_read_bit` = `od_ec_encode_literal_bypass`（**真 bypass**，非均匀 CDF 符号；`entenc.c:216-223`），且 AVM 系数语法确用它（`decodetxb.c:890` / `encodetxb.c:1182`）；现有仅符号接口无法表达。扩展是低风险可加（AVM `od_ec_enc_normalize` 与 svt `ent_normalize` 仅差 `n_bypass` 分支）。**验收闸门**：M1 固定向量 `E6 C8 00 12` 逐字节不变 + 裸位路径须有独立 oracle/对拍验证（裸位 bug 是静默的）。
- **T007 适配入口归属**：`SvtAv2Picture` → `SvtAv2FrameBuffer` 的 borrowed-view 适配**归 T007 提供**（`copy_in` 是其冻结原语，入参约束也由它定义；否则每个调用方各写一份滥用式适配，与 `picture.h:117-121` 禁止的"第二份拷贝"同类）。T013 的 P1 冻结不动 `encoder.c` **裁定正确**。
- **T008 口径批准**：报告**不冻结**；判据层 = 契约层四 hash，记录层持续演进不作判据；不采用只读快照（会把记录层变相升级为判据层）。

### 18. 第二轮集成：T009 置 DONE + Codec→Reconstruction 构建边落地 + 契约 §4 更正（00 · 2026-10-07）

**00 亲验（Release + Debug，均 `/WX`）**

```
build\00        Release : configure/build exit 0 → ctest 100% passed, 0 failed out of 26
build\00-debug  Debug   : configure/build exit 0 → ctest 100% passed, 0 failed out of 26
test_transform 1187622/0（R+D）  test_t009_t010_link 14543/0  test_transform_path 180/0
test_reconstruction 40495/0  test_intra_frame 62820/0  test_picture 30871/0  test_quantization 2846/0
M1 冻结路径零回归：test_tile_writer 11/0、test_header_frames 127/0、test_api 768/0
```

**状态**：T007/T008/T009/T010 = **DONE**；T013 P1 / T015 H1-H2 / T012 模块面 / T016 S1 = READY_FOR_INTEGRATION；T011 RUNNING（裸位接口已批准）；T014 V3 / T016 S3 仍 BLOCKED（等 T012 internal recon 导出）。

**本轮 00 新落的公共变更（串行）**

| 变更 | 文件 | 内容 |
|---|---|---|
| **Codec→Reconstruction 构建边** | `Source\Lib\Codec\CMakeLists.txt` | `svt_av2_enc`(+shared) 加 `PRIVATE include Source/Lib/Reconstruction` + `PUBLIC link svt_av2_reconstruction` |
| 注册 `test_transform_path` | `tests\CMakeLists.txt` | 挂 `svt_av2_encoder_core`（`180/0`） |
| 注册 `test_t009_t010_link` | `tests\CMakeLists.txt` | 挂 `svt_av2_transform`+`svt_av2_quantization`（`14543/0`，**32x32/64x64 不再 skip**） |
| **契约 §4 终裁** | `docs\m1-m2-contracts.md` | 64x64 余量 `2.8x → 5.6x →` **实测 5.5x**、level `2934 → 1467 →` **实测 1483**（00 2026-10-07 终裁：T010 的 5.6x 解析版仍错——松散界 `N·90` + pass-2 累加长度误当 32；00 自建 `artifacts\00\probe_level_bound_exact.c` 从真实核得 R=64N、上界 32640，与实测逐值相等） |

**裁定①理由**：帧 core 入口形参含 `SvtAv2FrameBuffer*`/`SvtAv2ReconScratch*`（均 `reconstruction.h` 类型）→ Codec 调帧 core **必然**要认识它们；方案②/③ 需改 T012 已定稿接缝签名。①为纯增量、零行为变化。

**P2 硬约束（T013 提出、00 采纳）**：① 加法优先（先加 M2 allocated source/recon，不改 `encode_pending_frame` 寻址）；② 布局切换必须原子（布局+寻址+recon 同批）；③ **M1 冻结的 21 字节流必须逐字节不变**，否则回滚。

**本轮抓到的真缺陷（T016 先报，00 独立复现）**：`transform_path.c:179` GCC-only `-Werror=maybe-uninitialized`（仅 Linux-Release `-O3`；`path_gather_coeff` 只填 `kw*kh` 而 `inv_transform` 读 `block_w*block_h`）。已要求 T012 修复。

**纪律新增（`AGENTS.MD` §7.7）**：① 核对他人新符号前先确认自身产物 mtime 晚于对方源码 mtime；② 符号表结论必须来自重建产物；③ 并发编辑窗口是瞬时态非缺陷（判据=重跑是否稳定）；④ 路径陷阱（连字符空目录 vs 反斜杠真项目）；⑤ **禁止用 `Get-Content -Tail` 判 `sent-log.jsonl` 消息丢失**（并发写读到半写行会误判，须 `ReadAllText` 结构定位）。
### 19. 第三轮：recon 导出解锁 T014 V3 / T016 S3；T012 一项不通过；表单再冻结（00 · 2026-10-07）

**解锁（00 亲跑 T012 `build_and_test.ps1`）**：`Release/Debug` 均 `40495 / 62820 / 180` 全 0 failures；四探针 `PACKING OK / PARAMS OK / EXPORT OK`。
**新接口**：`reconstruction.h:237 SvtAv2ReconExport` + `:261 svt_av2_recon_export_frame()` —— 内部 recon 产出、**复用 T007 `svt_av2_picture_export_raw`（无第二份打包）**、移除全部 padding、容量不足不写字节不改 meta。→ **T014 V3 / T016 S3 阻塞源解除**。

**不通过项**：`transform_path.c:179` GCC `-Werror=maybe-uninitialized` **未修**（00 本轮再次独立复现于全新 `build/00-wsl-gcc-verify`）。该文件 hash `4114F2F1…6C3AC` / mtime `18:35:55` 与上轮基线**逐字节相同**。→ **T012 READY 不受理**；T016 的 S1 `Linux-Release` 保持 FAIL。

**表单第二次冻结会话**：T011 停在 `question` 表单（`frm_1161567f…`）→ 队列只增不减、裁决投递不进。00 裁定并答复（204）解冻。裁定 = **方案 2（先交付 + 标 missing）**，但写成 `PENDING + 闭环路径`；闭环 = **只读 oracle `avmdec.exe`**（`avm-build` 目录实测不存在）。**否决"自写最小解码器"**（与自家 encoder 共模错误）。

**00 承认的读数错误**：此前引用的 `Encoder\CMakeLists.txt`/`transform_path.c` hash 是**更早时刻旧值**；T012 的"以当前磁盘为准复验"正确，该项撤回。**纪律：跨轮引用 hash 必须当轮复算**（已入 `AGENTS.MD` §7.7）。

**T016 的跨会话事实（受理）**：T007 自证"未改代码"但磁盘在其后变更（`picture.h` → `139451843B…`，A1 三符号）。→ 验收一律按**当轮复算** hash，**不以旧 hash 判"未变"**。

**本轮 00 当轮复算 hash 台账**：`transform_path.h` `CB0C388E…1DA2A`（Scope 注释已修）、`transform_path.c` `4114F2F1…6C3AC`（**GCC 缺陷未修**）、`test_transform_path.c` `3AF870A8…8AD8`、`probe_tx_params.c` `41EF3E55…03B842`、`probe_recon_export.c` `883C7306…FD3C0`、`picture.h` `139451843B…03E3D`、`picture.c` `A7422128…8BA65`、`test_picture.c` `DC8A1A9D…C572E`。

**CTest**：`build\00`（Release）与 `build\00-debug`（Debug）均 **26/26 Passed**；注册数 **Win 26 / Linux 25**。
### 20. 第四轮：coeff 布局分歧裁定 + 硬闸门通过（00 · 2026-10-07）

**核心裁定（契约层）**：T012 的 `transform_path.c` 把 `qcoeff` 写成 **64 宽光栅（4096 项）**，而契约 §3 **L103/L105/L106 已明确选 (甲)**（`min(w,32)*min(h,32)`、TX_64X64→1024、**否决 (乙) 4096+高半部恒 0**）。→ **T012 实现 = 被否决的 (乙)**；**T011 与契约均无误**。危害 = **静默错值**（T011 的 `qcoeff[scan[c]]` 落在零填充区，无断言失败）。**裁定方案 A**：T012 改回 32 宽原生布局。

**00 补契约（消除根因歧义）**：`docs\m1-m2-contracts.md` §3 新增"行 stride 澄清"——
> `coeff`/`qcoeff`/`dqcoeff` 行 stride = **`min(w,32)`**（不是 `block_w`、不是 `scratch->stride`）；TX_64X64 均 **1024 项、行 stride 32**。
> `intra_frame.h:66-68` 的 "block-local stride (`scratch->stride`)" 一句**仅适用于样本类缓冲**，对 coeff 数组不适用。

**T013 P2 骨架受理**：M1 硬闸门 `21 B / 7AAB5986…CE0F7` **逐字节 IDENTICAL**；`p1_evidence` 30/30；`test_api` 768/0。**未改** `frame_copy` 布局与 `encode_pending_frame` 寻址（符合"加法优先"）。

**`test_tile_writer` 4 处失败 = 并发编辑瞬时态**：00 复跑 `11/0` 通过，`ninja: no work to do` → 确认是 T011 于 18:37 编辑窗口的瞬时态，与 T013 无关。

**T014 V3 仍 BLOCKED（00 认可）**：现行 app 的 internal recon 是**整帧平场**（M1 qp 带 DC 塌缩），且无调用方使用 `svt_av2_recon_export_frame`、app 无落盘选项 → `--recon-matrix` 只会得到 **flat==flat 的空洞 PASS**。**T014 拒绝伪造 PASS 的做法正确。** 解除条件：(a) 非空残差码流（T011+T013）；(b) app 调用 export 落盘。

**表单纪律再次生效**：T011 第二次因 `question` 表单冻结会话，00 裁定并答复（204）解冻。

### 21. 第五轮：T011 受理 + GCC-only 缺陷家族再现 + 热路径性能回归（00 于 2026-10-07）

**T011 = 模块面受理（READY 受理）**。00 独立复跑：Release `build\00` + Debug `build\00-debug` 重配/构建 exit 0；CTest **28/28**（两配置）；`test_coeff_syntax 104/0`、`test_tile_writer_capacity 17/0`（R+D）；闸门 1 `test_tile_writer 11/0`（M1 冻结向量 `E6 C8 00 12` 逐字节不变）；**闸门 3 独立复现** `cmd /c artifacts\T011\run_gate3.cmd` → `118 checks / 0 failures`（裸位路径 vs AVM 未修改熵编码器 bit-exact）。

**00 落地的公共变更**：
- `Source\Lib\EntropyCoding\CMakeLists.txt`：`svt_av2_entropy` 源列表加 `coeff_syntax.c` + `cdf_tables_coeff.c`（**不**链接 `svt_av2_bitstream`）。
- `tests\CMakeLists.txt`：注册 `test_coeff_syntax`（挂 `svt_av2_entropy`）与 `test_tile_writer_capacity`（挂 `svt_av2_bitstream`）。
- `docs\m1-m2-contracts.md` §4：`|qcoeff| <= 8191` 更正为 **T010 自选保守生产上限**（非解码器约束；`decodetxb.c:907` 实际接受 `2^20-1`）。
- `docs\m1-m2-paths.md` §3.1–§3.4：**全 34 条 hash 复算并更新**（含 T011/T015/T013/T014 属主漂移项与 4 个 T011 新文件），零漂移。

**新增缺陷类型（第 9 条同族）：Linux-only `-Werror=unused-function`**。`coeff_syntax.c:262 uv_lf_ctx_2d` 是 `uv_ctx_2d` 的纯转发包装、**全文件无调用点**；GCC `-Wall -Werror` 判 `unused-function` → Linux Release 整仓构建 **exit 1**，而 MSVC `/W4` **不报** unused static function，故 `build\00` 28/28 全绿是假象。**判据补充**：Linux/GCC 构建是独立闸门，不能由 MSVC 全绿替代。

**T012 GCC 缺陷已修复（00 复验）**：`transform_path.c` 现版 `-O3 -Wall -Wextra -Werror` **exit 0**（原 `:179`/`:192` 警告消除）。**但新实现引入热路径性能回归**：`path_gather_coeff` 改为 `i/kw` + `i%kw` 平坦循环，`kw` 为运行期变量 → 每系数一次硬件除法。00 实测（GCC 11.4 `-O3`，3 轮最小值）：64x64 **1911 ns/blk vs 嵌套循环 52 ns/blk（36x）**；而其注释声称要避免的 16 KiB `memset` 仅 **143 ns**。正确性经前缀逐元素比对全等（`artifacts\00-gather-prefix.c`），**纯性能问题**。00 裁定用"嵌套循环 + `= {0}`"（约 183 ns）。

**注册数定论**：注册数 = 该 build 目录**上一次 configure 时的树快照**，**非常量、不得作判据**；唯一正确做法 = 跑矩阵前先 configure 再实读该目录 `ctest -N`。00 独立实测 Linux 22/24/25/23、Win 28/26，与 T016 一致。

**WSL 已恢复**：`ARXChem-Ubuntu-22.04-WSL1` Running（T012 之前遭遇的 WSL 故障为瞬时态，非阻塞理由）。
## 22. 第六轮：T013 P2 接口侧 + 一处集成级发现（2026-10-07 20:20–20:45）

### 22.1 00 落地的公共改动
- `include\svt_av2.h` 加 `SvtAv2ReconFrame` + `svt_av2_enc_get_recon`（契约 §7.2）。
  先落盘安全性**经实测**：`cl /LD /W4 /WX` 编译"仅声明、无定义的 `__declspec(dllexport)`" → exit 0。
- `Source\Lib\Codec\CMakeLists.txt` 为 `svt_av2_enc`/`_shared` 加 link `svt_av2_encoder_core`，
  消除 `codec.h:13 #include "transform_path.h"` 的 `C1083`（Codec→Encoder 构建边，无环）。
- 契约新增 §7.2；`m1-m2-paths.md` §3.1/§3.2 更新。

### 22.2 00 亲跑
`build\00` 构建 exit 0、`ctest -N`=28、CTest 28/28；M1 冻结 21B 四输入 4/4 逐字节不变
（`7AAB5986…CE0F7`）；`test_keyframe` 8/0；`svt_av2_enc_get_recon` 探针 `PROBE OK`
（6144B、64x64、distinct=78 非平场、容量契约正确）。

### 22.3 ⚠️ 集成级发现：码流不含任何系数
四输入同 hash；`keyframe.c:46-51` 把样本 `(void)`、`tile_writer.c:79` 只写
`block_write_minimal_intra()`、`encoder.c` 未设 `block_hook`；
`avmdec` 解码 flat/noise 的 MD5 相同、decoder distinct=1 vs encoder recon distinct=78。
=> **P2 的"内部 recon"已完成，"真实 tile data"未完成**；T014 V3 / T016 S3 的真实卡点
是这条接线，不是 `--export-recon` 开关。00 已派 T013 落"第四批"（block_hook → tile payload），
并放行 21B 闸门从"必须不变"转为"显式报告新流 + avmdec 端到端闭环"。

## 23. 第七轮：stride 阻断修复闭环 + 五配置全绿 + 两条契约补全（2026-10-07 23:00–23:40）

### 23.1 T011 stride 阻断级缺陷闭环（00 复跑受理 → DONE）
- 根因（00 裁定）：`coeff_syntax.c` 行偏移用**符号数** `SVT_AV2_SYMS_X` 步进，而表每行存 `nsyms-1` 个边界 → 从 `row=1` 起全部错位；64x64 越界只是最大行号的表现（`txs_ctx=4` 在 5 行表内**合法**，**不得钳制**）。
- T011 修法：32 处行偏移乘数改 `(SVT_AV2_SYMS_X - 1u)`，`ns` 实参不变，`v_txb_skip + ctx`（stride 恰为 1）未动。
- 00 亲验：三处新 hash 逐字节 MATCH（`coeff_syntax.c 00A4E7EF…`、`cdf_tables_coeff.h AE6B8F1F…`、`test_coeff_syntax.c 95250051…`）；ASan 删 `.o` 强编 → `121 checks, 0 failures, exit 0`；MSVC `build\00` 28/28；Linux `00-wsl-gcc` 27/27；M1 向量 11/11 + 17/17。
- 00 独立核验 T011 新断言**非自证**：逐值比对 AVM `entropy_inits_coeffs.h`（1839/1354/8298、22871、7975、10923、23062、16540）全部相符。
- 结果：**T011 = DONE**；`m1-m2-paths.md` §3.3 的 **4 行陈旧 hash** 一并更正。

### 23.2 T016 S1_r7 clean 全矩阵（00 复跑受理）
- 快照 `artifacts\T016\S1_r7\platform.json`（36660B，`FF86132D…4913`）`overall=PASS`，00 复算 MATCH。
- 五配置：Win-Release 28/28、Win-Debug 28/28、Linux-Release 27/27、Linux-Debug 27/27、**Linux-San 27/27**（上轮 26/27 FAIL 的 ASan OOB 随 T011 修复消失）。
- 00 亲跑 San 配置 ctest → 27/27 PASS；`coeff_syntax.c.o`(23:20:13) 新于源(22:58:20)，非陈旧。
- 口径确认：`_JUDGED_SOURCES` **只放判据本体**（`run_matrix.py`/`compare_recon.py`），构建输入进报告手记；Linux 注册 27 系少 `runner_oracle_smoke`（无本机 oracle），不写作"少功能"。

### 23.3 两条契约补全（00 属主文件）
- `docs\m1-m2-paths.md` 新增 **§3.5 帧内重建链路（T012）**：登记 T012 九个属主文件（00 当轮复算 hash+size 全 MATCH）。文件现 `57455859…`。
- `docs\m1-m2-contracts.md` 新增 **§7.3 公共导出面清单**：登记 `include\svt_av2.h` 的 11 个 `SVT_AV2_API` 符号（含 `svt_av2_enc_get_recon`），并明确"守卫按字面断言、意外导出 FAIL、新增须刻意登记"。文件现 `777F64F4…`。
- `docs\task-reports\M2-integration.md`：L255 LEVEL_OVERFLOW 行与 L404 契约 §4 更正行更新为终裁口径（实测 5.5x / 1483）。文件现 `EFD7FA6A…`。

### 23.4 裁定与放行（本轮）
- **T014 ask1 = (A)**：`run_matrix.py` 冻结基线 `48132100` 在 S3 跑完前不改；接线落地后**一次性**改文案，且要求**去除一切时点性断言**（改为"判据 + 反例"的稳定表述）。
- **T013 放行两项**：授权 app `--export-recon`（不改公共头、不写第二份打包、默认关闭时 M1 流逐字节不变）；裁定第四批接线（`block_hook` → tile payload，附 T011 接缝 4 问答复）。
- **T016 GO**：两个前置（T011 修复、T012 `transform_path.c` 静止）已闭，令其跑全矩阵 —— 已完成并受理。
- **T012 转入等待队列**：受理 RFI（未置 DONE），待 T013 接线后跑 DoD 第 3 项真实链路。

### 23.5 当前唯一关键路径
**T013 第四批接线**（`block_hook` → tile payload）是 T011 闸门 4b、T012 DoD 第 3 项、T014 V3、T016 S3 的**共同前置**。00 实测 `block_hook` 与 `svt_av2_coeff_write_block` 在 `Source`/`app` 全树**命中 0**，即接缝未接、生产调用点不存在。

## 24. 第八轮：T015 三问核实受理 + 路径表断言修正（2026-10-07 23:40–23:50）

### 24.1 T015 多帧 header 构件边界三问（00 逐条实测核实 → 受理）
- T015 答"`write_multi`/`min_level_for_size` 无生产调用者"：00 全仓检索成立（仅 `sequence_header.c` 定义 + `tests\unit\test_header_frames.c`）。**措辞边界**：`keyframe.c:86` 确实在生产路径调 `sequence_header_config_default()`，但走 M1 单图 `svt_av2_sequence_header_write()`（`single_picture_header_flag=1`），**非** `write_multi`；已在回执中点明。
- T015 报 `codec.h:137 SVT_AV2_MAX_ENCODABLE_FRAMES 1u`：实测 L137 正确，**00 此前记录的 L112 系陈旧值，作废**。
- 签名/行号逐行核对成立：`sequence_header.h:85/107/137`、`frame_header.h:92`、`frame_header.c:174-186`、`sequence_header.c:154`。
- T015 模块无帧数概念：其两源文件检索 `frame_count|num_frames|n_frames|frame_idx|SVT_AV2_MAX` → **命中 0**，故"帧数/循环/PTS/EOS 全归 T013"成立。
- 00 亲跑：`test_header_frames` **127 checks / 0 failures / exit 0**；ctest Passed；`probe_writer_vs_model.py` **ALL MATCH / exit 0**（四向量逐字节与报告 §0.2 相符）；M1 冻结回归 `7AAB5986…CE0F7`（21B）与 `artifacts\T005\frame_min_64x64.bin` **逐字节 MATCH**；五源 hash 当轮复算与报告 §0.1 全相符。

### 24.2 发现并修正：`m1-m2-paths.md` §3.3 断言失效（00 属主文件）
- L106 原文断言 `sequence_header.*` 与 `m0-m1-contracts.md` §5 "同样一致"——**已失效**：M1 基线 `sequence_header.h 0DF7B4FE…`/`.c 0D405DA6…`，M2 现值 `2AB00A28…`/`717747DB…`，T015 已改。
- 00 修正 L106：明确 `bit_writer.*`/`obu_writer.*` 仍一致（实测四条 hash 逐字节 MATCH），`sequence_header.*`/`frame_header.*` **已漂移、以 §3.3 当前值为准**。
- 另补登 T015 判据源 `tests\unit\test_header_frames.c 12142960…` 入 §3.3（与 §3.5 登记 T012 测试文件同构）。
- 文件：`docs\m1-m2-paths.md` → `23EA3A58…`（14527B，CR/LF=21/178，`replacement_chars=0`）；备份 `*.bak234234`。

### 24.3 待 T015 回执（唯一）
- `docs\t015-header-field-reference.md` 报告 §0.1 hash 陈旧：报 `CA30B060…`，磁盘实测 `82FA0364…`（40741B，16:56:23）。已发 `[00][ACCEPTED]` 请其改为实测值并更新 §7 接口表"待确认"字样。

### 24.4 S4 接线范围已下达 T013
- 依据 T015 已核实答复，00 发 `[00][RULING]` 明确：`codec.h:137` 常量与 L134-136 注释归 T013；序列封装/帧循环/PTS/EOS 全归 T013；给出逐字调用模型（序列级 `write_multi` 1 次 + 帧级 `frame_header_write` 每帧 1 次 + `multi_frame=1`）与 TU 结构（`1×TD → 1×SEQ → 每帧 (TD? + CLK)`）；**严禁**拼接 N 个 21B still-picture 流。建议先落第四批再接多帧。

### 24.5 T015 三项修改受理 + 值级断言缺口指派（本轮闭环）
- T015 三项修改 00 亲验：doc `7FA4D577…5937`（42140B）、报告 `00EA43EE…FCA3`（17268B）**均 MATCH**；§7 两行已改"已答复并确认"并新增 §7.1；报告 L242-244 加 H3/H4 边界重申（**不置 DONE**）；五代码/测试 hash **未变**。三项受理通过。
- **00 发现的真实缺口（已指派 T015 闭环）**：`frame_header.c:174-186` 多帧帧头新增 2 位（`immediate_output_frame=1`、`frame_size_override_flag=0`）**只有长度断言（`22→24`/`26→28`）、无值级断言**；独立模型 `ref_model_headers.py` 对 `multi_frame` **命中 0**；5 个 `test_multi_frame_*` 全是序列头、无多帧帧头。属同源第 11 条"只验通、不验值"（T011 曾 104 checks 全绿却全错），而 T013 即将接入生产流。
- 指派要求：扩展 `ref_model_headers.py` 独立推导 2 位 + 新增 `176x144`/`64x64` 两尺寸值级断言（`bits_written` + 逐字节）；00 手算交叉核对给出 `176x144 multi=28 位 E3 06 40 00`、`64x64 multi=24 位 E3 64 00`（仅供对照，权威仍是 T015 模型）。
- 00 已 `-Dump` 取证后清空 00 收件箱（cancelled=1, failed=0）；当前全角色 inbox/forms 均 0。

### 24.6 在途预验证：T013 `--export-recon` 落地但实测失败（00 定位根因，已报 T013）
- T013 属主文件已变动：`main.c` 29013B→**34196B**（23:26:17）、`codec.h 21063605…`（20:35:35）、`encoder.c 3FAB11ED…`（21:00:20）。
- **接缝仍未接**：00 全树检索 `block_hook|svt_av2_coeff_write_block` → **命中 0**；`codec.h:137` 仍 `SVT_AV2_MAX_ENCODABLE_FRAMES 1u`。符合 00 建议的"先落 export-recon、再接第四批"。
- **已通过**：`--export-recon` **不传**时 M1 流 `7AAB5986…CE0F7`（21B）逐字节不变，授权条件之一满足；`main.c` 在 `build\00` 编译链接通过。
- **失败（真实缺陷）**：传 `--export-recon` → **EXIT=11**、`error: svt_av2_enc_get_recon failed for frame 0 (SVT_AV2_ERROR_INVALID_PARAMETER)`，recon 文件 0 字节（码流仍正确）。
- **根因（00 定位到行）**：`main.c:638 recon_buffer = NULL` + `L639 recon_capacity = 0` → 首调 `recon.data == NULL` → `encoder.c:710` 直接返回 `INVALID_PARAMETER`，**永远到不了** `L724` 的 `OUT_OF_MEMORY + required` 映射与 app `L752` 的 grow-and-retry。**对照** packet 路径 `L612` 有初始 `malloc(65536)`，故其首调 `capacity != 0` 能进 retry —— 这是唯一差异。
- 00 报 T013 两处可修点，**倾向 A**（app 侧给 `recon_buffer` 初始 `malloc`，与 packet 路径同构、不动公共 API）；B（放宽 `encoder.c:710` 语义）需 00 确认。要求回执附 recon size+sha256（64x64=6144B）、传/不传两流 sha256、`main.c` 新 hash。
- 关键路径不变：**第四批（`block_hook` → tile payload）仍是唯一关键路径**。

## 25. 第九轮：T015 r5 多帧帧头值级断言闭环受理（2026-10-08 00:00–00:10）

### 25.1 T015 r5 受理（00 逐项亲跑 + 第三方独立推导）
- 三处新 hash 当轮复算全 MATCH：`test_header_frames.c AEA461E2…5A65`（24858B）、`ref_model_headers.py D5F5D076…7D19`（11824B）、`probe_writer_vs_model.py E75C3B71…BD47`（3841B）；报告 `T015.md A09F1194…ABB1`（21426B，311 行）；字段表 `7FA4D577…` 未变。
- 00 亲跑：Release/Debug `test_header_frames` **各 133 checks / 0 failures**；Release/Debug ctest **各 28/28**；`probe_writer_vs_model.py` **ALL MATCH**（含 4 条 fh 值级：64x64 single `CD 90 00` / multi `E3 64 00`，176x144 single `CC 19 00 00` / multi `E3 06 40 00`）；M1 冻结回归 `7AAB5986…CE0F7`（21B）逐字节 MATCH。
- 非属主文件未碰：`tests\CMakeLists.txt 06FF1919…`、`keyframe.c 49706DD4…`、`include\svt_av2.h 5F831F0B…` 均未变。
- **非自证三重核验**：测试读 writer 实际 `buf` 并 `memcmp`（非回抄）；模型 `frame_hdr_multi()` 按规范字段序独立写出；**00 另建第三个独立实现**（`%TEMP%\probe00_multifh.py`）手算得同一字节 ⇒ 三方一致，缺口确认闭环。

### 25.2 T015 报告损坏事故（已核实 + 留档）
- T015 自报：用 `Set-Content -Encoding UTF8` 更新报告时，PowerShell 5.1 以 GBK 读 BOM-less UTF-8 → 乱码不可逆，已按已核对内容重建并新增 r5。
- 00 核实：新报告 311 行、无 BOM、`replacement_chars=0`、§0–§10 结构完整、§0.1 八 hash 与实测全相符、§0.2 四向量一致。
- **可回溯性（00 更正 2026-10-08）**：旧版 `00EA43EE…`（17268B）**正文不可回溯**——00 穷尽扫描全树 + temp（15889 文件 × UTF-8/GBK/UTF-16）确认：全盘**无 17268B 文件**，`sent-log.jsonl` 的 `T015->00` 15:42:29 条目仅 **1480 字符**（是通知消息，只含 hash 串），`00-inbox-dump.txt` 不含任何旧报告锚点。**仅旧 hash 与 T015 的摘要消息留存，正文已丢失。**（00 上一轮曾误称"内容已留档"，系由"hash 串出现在 sent-log"错误推断所致，已更正。）
- 00 重申纪律：禁用 `Set-Content`/`Out-File` 写 BOM-less UTF-8 `.md`（一律 `[IO.File]::WriteAllText` + `UTF8Encoding($false)`）；改交付文件前先备份；重建须附事故声明与"无信息丢失"核对方法。

### 25.3 判据层更新
- `docs\m1-m2-paths.md` §3.3 的 `test_header_frames.c` hash 由 `12142960…` 更新为 `AEA461E2…` → 文件 `C6410D64…`（14527B，CR/LF=21/178，`replacement_chars=0`）；备份 `*.bak000605`。
- T015 状态：**READY_FOR_INTEGRATION（H1/H2）已受理，不置 DONE**（H3/H4 未完成）；S4 header 侧闭环，无新指派。

### 25.4 T015 §11 事故声明受理 + 三处更正（含 00 自身错误）
- §11 已受理：报告 `F0F01781…D327B6`（23415B，339 行，无 BOM，`replacement_chars=0`，首字节 `23 20 54`）；其余八文件 hash 全部未变，"仅改报告"成立；三条写文件纪律正确。
- **00 逐项核对锚点，发现 §11 三处不实**，已发 `[CORRECTION]` 要求一次改完：
  1. L325 "§0 结论表 **14** 项" → 实测 **11** 项（L19–L29）；其余锚点属实（§0.1 八 hash、§0.2 四向量、§4 八项、§2 五条、§9 三条、§6 十四项）。
  2. L323-324 "旧版 `00EA43EE…` 正文**留档于** sent-log 与 inbox dump" → **假**。**00 穷尽取证**（全树 + temp 共 **15889 文件** × UTF-8/GBK/UTF-16）确认：**全盘无 17268B 文件**；`sent-log.jsonl` 15:42:29 条目**仅 1480 字符**（通知消息、只含 hash 串）；`00-inbox-dump.txt` 不含任何旧报告锚点。**旧版正文已不可回溯。**
     - **该错误源头是 00 上一轮消息**（据"hash 串出现在 sent-log"错误推断"内容已留档"），T015 照抄；00 已在 §25.2 更正自身断言。
  3. L321 "本文加入 §11 后为 **338** 行" → 实测 **339** 行（LF 计，文件以换行结尾；备份版 310 行）。
- 教训（新增同源错误候选）：**"字符串出现在日志中" ≠ "正文被留档"**；断言可回溯性必须实测文件 size 与内容锚点，不能凭 hash 串出现推断。

### 25.5 T015 §11 三处修正受理（本线程闭环）
- 三处修正 00 亲验通过：① §0 结论表 **11** 项（L19–L29 实测）；② 旧版**正文不可回溯**（T015 独立复验 + 00 穷尽扫描，结论一致）；③ 行数口径统一（基版 **310** / 现版 **344**）。
- 现版 `T015.md` = **`B35243E0…1691`**（24090B，LF=344，无 BOM，`replacement_chars=0`）；备份链 `000655`（21426B/310）→ `001551`（23415B/339）→ 现版（24090B/344）。
- T015 首条消息报的 `9130FD4C…`/343 行是**写盘前中间值**（文件 mtime 00:17:59 晚于消息 00:17:20）；其第二条报 `B35243E0…` 与 00 实测一致，已按此受理。**非错误，属时序。**
- 339 vs 344 算术已澄清：310（重建基版）/339（只加 §11 的中间版）/344（三处全改完）——§11 如实写了 310 与 344，**保持现状不再改**。
- T015 本线程处置符合三条纪律（先备份、`WriteAllText`+`UTF8Encoding($false)`、无 BOM），且对第 2 条做了**独立复验**（未照抄 00）。
- T015 = READY_FOR_INTEGRATION（H1/H2）已受理，不置 DONE；本线程闭环，无待办。

### 25.6 P0 续跑（用户指令，2026-10-08）：第四批接线属主终裁 + `--export-recon` 实测通过

**触发**：用户续跑，要求 P0 先实测核实跨属主接缝、**正式明确属主/契约再并行派工**，不得只给下游一项无上游接口的空接线。

**00 当轮实测（非照抄）**：
- M1 冻结基线 `artifacts\T005\frame_min_64x64.bin` = `7AAB5986…CE0F7`，21B ⇒ **MATCH**（未回退）。
- 接缝三处均**未接**：`tile_writer.c:109` 仍硬编码 `svt_av2_block_write_minimal_intra`；`svt_av2_coeff_write_block` 生产调用点=0；`block_hook` 生产赋值点=0。
- **关键前提澄清**：`svt_av2_coeff_literals_supported()` 返回 **1**，`svt_av2_entropy_encode_literal()`（bypass L(n)）已存在 ⇒ 真实系数语法**可发**，**不存在**"无法发 raw 位"的阻断。
- `codec.h:137` `SVT_AV2_MAX_ENCODABLE_FRAMES 1u` 未放开。

**属主终裁（依据磁盘现行文档，非记忆）**：
- `m1-m2-paths.md` §2.2（00 维护的 M2 权威属主表）：`tile_writer.*`=**T011**；`keyframe.*`=**T013**；`Codec\encoder.*`/`codec.h`/`app`=**T013**。
- 契约 `m1-m2-contracts.md` §5 Owner=**T011**（"tile writer 输入、context、容量"）。
- **更正**：T013 §9.41/§9.43 反复引用的"`tile_writer`/`keyframe` 属 T005"是 **M1 旧属主**（见 `T005.md` M1 文件表），M2 已重划。**此误引是第四批空转多轮的根因；责任在 00（未及时公告属主重划），不在 T013。**
- 裁定：`tile_writer.*` 增系数输入（T011）→ `keyframe.*` 增参数 + `encoder.c` 采 `qcoeff` 并驱动（T013）→ 帧 core 暴露块序列（T012）。依赖方向合法：`Codec`→{`Bitstream`,`encoder_core`}。

**`--export-recon` 修法 A 实测通过（用户第 4 点，00 亲跑）**：
- 重建 `build\00`（Release，`/WX`）后亲跑 `checkerboard_64x64_1f_420.y4m`：
  - 不带 `--export-recon`：exit **0**，`m1_plain.av2` 21B `7AAB5986…CE0F7`。
  - 带 `--export-recon`：exit **0**，`m1_recon.av2` 21B `7AAB5986…CE0F7`（**默认关闭 M1 流逐字节不变**），`recon_64x64.yuv` **6144B** `D67DA3B2…0863`。
- 产物落 `artifacts\00\`。缺陷（`main.c:638 recon_buffer=NULL` 首调 `INVALID_PARAMETER`）由 00 定位、T013 修法 A 修复，**闭环**。

**派工（本轮投递，`delivery=queue`）**：
- `T011`：`[TASK]` 第四批 tile_writer 系数输入（属主=你），今日先发 `[T011][CONTRACT]` 签名。
- `T012`：`[TASK]` 公告 `block_hook` 块序列顺序 + 几何契约。
- `T013`：`[RULING]` 属主终裁 + 修法 A 通过；先做 S4 多帧（不等第四批），收到两方签名后接第四批。
- `T014`/`T016`：`[INFO]` 状态同步；V3/S3/S4 **仍 BLOCKED**（依赖第四批/多帧），勿空转。

**属主文件更新**：`M2-integration.md` 追加 §18 → `DD881A13…39F5`（63100B，CR=2 既有、`replacement_chars=0`）；备份 `*.bak20261008-004000`。

## 26. M2 双窗口完成并独立验收（2026-10-10）

状态 DONE，版本 `0.0.2-m2`。用户授权两个子窗口：m2_syntax 完成真实系数/tile 语法，m2_pipeline 完成生产 API/CLI 与多帧重建闭环；主窗口统一合法编码树并完成最终验收。公共路径已接入真实非零 qcoeff，支持目标尺寸与连续多帧，覆盖前述第四批未接线、V3/S3/S4 BLOCKED 的历史状态。

64×64、176×144、720p、1080p × 1/2/3/17 帧矩阵 16/16 AVM 解码与内部重建逐字节一致。连续 1000 帧及 ASan/UBSan/LSan 长跑通过，1000 帧哈希各不相同。MSVC/GCC/Clang 的 Debug/Release 全部通过；Windows 28/28，Linux 27/27，额外 sanitizer 27/27。Clang 四尺寸 17 帧码流与 MSVC 一致。

最终证据、平台差异、支持范围、哈希和复验命令见 [M2 主窗口验收](docs/task-reports/m2-codex-integration.md)。M2 支持 8-bit420、QP100、单线程、8 倍数尺寸至 1920×1080；M3/M8 未开始。旧十队列记录保留用于追溯。

## 27. M3 开发要求与执行包准备（2026-10-11）

依据用户“开启m3阶段，先把开发要求写好”，生成 `../M2-M3-AI-TASKS/` 13份任务Markdown和 `docs/m2-m3-contracts.md`。默认两个开发窗口：A负责T017–T020/T023像素数学重建，B负责T021/T022/T024语法生产接线，00承担T025/T026独立验收；亦给十队列模式。先10-bit420，再8/10-bit444；422条件放行。冻结byte stride/u16le样本表示、格式几何、真实系数生命周期和独立判定要求。

本次静态核对M2清单86个文件全部SHA256一致，无缺失或变动；未重新构建，不将历史测试说成本日新测试。M3代码未开发、新会话未启动、运行期花名册未修改。完成要求为64项四组合矩阵、三种新组合各1000帧及跨平台/检测器；M2生产状态与最终报告保持有效。

### 27.1 本期范围与执行平台修订（2026-10-11）

用户明确M2→M3由当前Codex主会话加两个子智能体完成，替代此前两窗/十队列/OpenCode可选描述；A/B分工不变，00承担独立验收。范围收敛为10-bit420→10-bit444，保留8-bit420回归，排除8-bit444和8/10-bit422及其支持性研究。任务书/契约/README/AGENTS已同步；验收矩阵改为48项（新增32项+回归16项），两种新配置各连续1000帧。此修订覆盖§27的旧范围描述。只修改文档，未派发代码开发。

## 28. M3 全自动开发启动（2026-10-11）

用户已授权全自动实现10-bit420→10-bit444，资源/必要依赖由本会话自行处理。00主窗口、A=/root/m2_pipeline、B=/root/m2_syntax已开始RUNNING，属主按任务包；不使用OpenCode。M2 86文件基线已冻结并新clean验证28/28与16项矩阵通过。主窗口负责新增独立判定、48项矩阵、平台/检测器终验；完成后按用户指令使用computer-use在FFmpegFreeUI加入编码队列并确认显卡占用。M3此时尚未完成。

## 29. M3 全自动实现完成（2026-10-11）

状态DONE，版本0.0.3-m3。两个Codex子智能体完成10-bit420→10-bit444全生产链，主窗口独立终验48项矩阵、两种新配置各1000帧及ASan/UBSan/LSan全部通过。MSVC Debug/Release各29/29，GCC/Clang Debug/Release与GCC sanitizer各28/28；Clang12项17帧矩阵码流/逐帧重建与MSVC相同。8-bit420四尺寸×四帧数16项及1000帧保持M2指纹。元数据/非对齐odd stride/非法样本/容量/PTS/EOS测试通过。

最终源97文件SHA清单 `artifacts/m3-root/source-manifest.json`；完整报告 `docs/task-reports/M3-integration.md`。公开ReconFrame新增bd/chroma字段要求重编调用者。受限QP100、threads1、8倍数尺寸至1080p、ALL-INTRA/DC_PRED/DCT_DCT；8-bit444/422等未扩展。用户指定的FFmpegFreeUI最后队列动作由主窗口在阶段完成后继续执行并补录报告。


## 30. SVT-AV2 M4开发要求准备（2026-10-11）

按用户要求，仅生成../M3-M4-AI-TASKS/下13份Markdown任务书和docs/m3-m4-contracts.md。继续Codex主窗口00+两个子智能体：A=T027–T030参考/运动/帧核心，B=T031–T033语法/帧头/API，00=T034–T036独立模型/矩阵/平台。先10-bit420，再10-bit444，保留8-bit420；最小单向P及必要420色度插值，不扩8-bit444/422/B帧/多参考/亮度subpel搜索/完整RD。

验收要求48项AI回归、108项P矩阵、实际P/inter/reference证据、静止码率与质量对照、三配置各1000帧及跨平台/检测器。本次静态复核M3源清单97/97 SHA一致，未重新编译或测试；M4代码未启动，未派发子智能体，M3生产版本仍0.0.3-m3。

补录上一阶段队列动作：已点击FFmpegFreeUI加入编码队列，观察到ffmpeg进程启动；computer-use被用户停止，GPU占用独立确认未完成。用户本轮说明已手动停止编码，当前不恢复队列。替代上节队列动作待执行的描述。


## 31. SVT-AV2 M4正式开发启动

用户授权进入M4，沿用00主窗口+两个子智能体，A=/root/m2_pipeline负责T027–T030，B=/root/m2_syntax负责T031–T033，00负责T034–T036与公共文件。状态RUNNING，未完成。M3 97源文件和固定oracle/spec hash复核一致，留存artifacts/m4-root/baseline-check.json。用户明确本次无需FFmpegFreeUI操作。必要依赖与技术判断自主解决，按任务包逐门终验后才标DONE。

## 32. M4 实现及最终验收完成（2026-10-11）

M4 0.0.4-m4 DONE，T027–T036全部受理。8-bit420/10-bit420/10-bit444支持默认AI及显式LOW_DELAY_P，首key后单向P、上一张内部重建参考、整数亮度ME与必要420色度插值，QP100/threads1/8倍数尺寸至1080p范围不扩。CLI启用--pred-structure low-delay-p。

108 P/48 AI逐样本一致，AI流/frame SHA与M3完全相同；12独立motion/reference反事实、三配置码率质量门槛、MSVC及GCC sanitizer三配置各1000帧、Clang36项确定性、七配置strict完整CTest全部PASS。最终版本再次clean及AI/P/长链指纹复验一致。Valgrind三配置2/17/1000帧9项0B未释放，Massif三配置1000帧峰值及趋势实测。最终116源文件及14个app/shared SHA在artifacts/m4-root/source-manifest.json、final-verification.json。

报告[M4主窗口终验](docs/task-reports/M4-integration.md)，编码器build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe。固定oracle/spec及M3产物不变，M1 21-byte SHA保持。本轮未操作3fui，M5及以后未启动。

## 33. M4 最小 FFmpeg 接入交付（2026-10-11）

按用户新要求完成 dist/M4-ffmpeg-min：Windows x64 静态 FFmpeg/ffprobe，新增独立 libsvtav2 同步封装，固定 AVM v1.0.0 仅作 libavm-av2 解码。输入 Y4M/RAW，AV02 IVF 编码输出，RAW/Y4M 解码输出。最小封装和已有 AV2 FFmpeg 基线在独立构建副本接入；原 FFmpeg checkout 和 M4 116 个冻结源文件 SHA 复核一致。

三格式×AI/P六组102帧：IVF payload与独立MSVC M4 CLI一致，FFmpeg解码/内部重建/固定oracle逐字节一致；三格式RAW单帧与30000/1001、Y4M解码输出、6项非法参数拒绝全部PASS。demo.ps1已在Windows实际运行PASS。报告docs/task-reports/M4-ffmpeg-min.md，教程dist/M4-ffmpeg-min/README.md，逐包PTS/命令/哈希在proof/acceptance.json。QP100/单线程/8倍数尺寸至1080p范围保持，未操作3fui。

## 34. M4→M5指导文档与首轮G1实现（2026-10-16）

本轮用户授权在NuxBox1由当前ChatGPT会话直接执行、可做多少算多少。已落盘../M4-M5-AI-TASKS/开发指南与docs/m4-m5-contracts.md、docs/m5-mode-syntax-research.md；实际进度见docs/task-reports/M5-progress.md。M4冻结116项源首次复核全部MATCH、固定AVM/spec SHA一致；Python缺numpy的问题已通过本次隔离依赖build/m5-root/pydeps解决。正式仍0.0.4-m4。

T038首批仅向intra_prediction添加V/H scalar参考边缘投影原型并扩展2000组独立单测；**尚未**加入实际AV2语法/bitstream/feature开关，不能宣称已有V/H编码。MSVC严格94/94目标、完整CTest35/35通过；Linux GCC Release34/34、ASan/UBSan/LSan Debug34/34通过。原M4 P 27项与新增代码禁用状态的源码/码流/内部重建SHA 0差异；另12项AI新二进制回归通过。报告及SHA详见artifacts/m5-root和M5-progress.md。G2依赖固定AVM mode_set/index与UV direction上下文、规范方向预测验证，下一阶段再推进。未改变FFmpeg包与旧M4冻结产物。
## 35. M5 DC/V/H 模式选择编码闭环（2026-10-16）

用户要求本轮“把模式选择打通”。已由NuxBox1当前ChatGPT会话实际完成 ALL-INTRA DC/V/H 的实验性自适应模式选择子功能：新增 --intra-select dc|adaptive（默认dc保M4），编码器用已重建邻居产生DC/V/H，Y SAD择优，唯一mode驱动residual/quant/recon和AV2 mode set/index/UV方向上下文。固定AVM decoder真实验收 66/66组（8bit420/10bit420/10bit444、128x64/64x128/128x128/176x144/720p/1080p、1/2帧、flat/H/V/mixed），其中60组触发实际不同码流和内部重建、6组平坦素材off/on相同；三项非法选项全部拒绝。另8bit420 128x128 1000帧新特性连续编解码重建全帧逐字节匹配。Windows MSVC严格CTest35/35、Linux GCC严格CTest34/34、GCC Debug ASan/UBSan34/34；sanitizer enabled-mode 10bit444另单例真实固定decoder重建一致。老M4的108 P矩阵验证通过，通用化后旧M4与新构建P27+AI12组SHA全部一致。**M5整体未完成，不更新0.0.4-m4正式版本；LOW_DELAY_P启用adaptive被明确拒绝**。

详尽变更、116基线的17文件差异、原始日志、解码SHA、真实使用命令、保留限制和恢复点：docs/task-reports/M5-mode-selection.md；独立自生成输入验证器 tools/test_m5_mode_selection.py；证据 artifacts/m5-root/mode-selection/acceptance.json 和 mode-selection-source-diff.json；新增公共config尾字段需调用者重编，M4原始冻结产物和FFmpeg最小包未动。

## 36. M5.1 AI/P DC/V/H 集成终验完成（2026-10-16）

用户明确恢复1+2智能体，00主窗口集成/API/独立验收，A负责P core，B负责语法/规范及独立decoder trace。本轮M5.1 DONE：--intra-select adaptive支持三格式AI/P的Y DC/V/H，UV保持DC；修正inter邻居覆盖DC及palette仅DC条件。144模式+24大尺寸、完整M4关闭48 AI/108 P共156指纹全部通过；MSVC/GCC/Clang144项off/on流/recon SHA相同。Windows与GCC Debug ASan/UBSan/LSan三格式AI/P各1000帧off/on全部精确且跨平台SHA相同。

最终MSVC严格CTest37/37、GCC/Clang/sanitizer各36/36；独立12例trace核817 Y/UV、39次inter邻居，API专项含输入所有权反例、容量重试、PTS/EOS；Valgrind337分配/337释放、0B未释放、0 errors。124项源清单与5项app/shared SHA在artifacts/m51-root/final-verification.json，清单SHA6ecfabcce2396708e1ceb6c5f2ef3e27232107945daf81e909834107506acc55。性能/质量mixed单点3次实测未证实普遍码率收益，不冒充完整RD或BD-rate。

报告docs/task-reports/M5.1-integration.md；开发程序build/m51-root/Windows/bin/SvtAv2EncApp.exe。M5整体RUNNING，内容partition/组合尚未开发，正式仍0.0.4-m4；旧M4冻结编码产物与最小FFmpeg包、固定验收oracle/spec不变，未操作3fui。诊断decoder独立保存于artifacts/m5-b/diagnostic-decoder并重放12/12；不替代不可变oracle。

## 37. M5.2 最小内容自适应分区完成（2026-10-16）

沿用00+A+B的1+2模式，完成--partition-select fixed|adaptive，默认fixed；仅完整64×64超块按Y整数SSE均值代理保留旧树或选四个32×32方叶，partial保持边界规则。每帧只选一次计划，core与writer共用，同一叶的mode/qcoeff/recon/语法对应；AI/P及intra-select四组合均已独立验收，公共config尾字段要求匹配头重编。

144例576流主矩阵、24例96流大尺寸、M51固定树两mode对照、M4全关156指纹通过；MSVC/GCC/Clang四组合流及逐帧SHA一致。Windows与GCC Debug sanitizer三格式AI/P各1000帧、四组合全部PASS且跨平台SHA一致。实际decoder partition trace90流354帧12174节点通过，包括flat/quads同尺寸对照、mixed计划翻转、partial不分裂。最终严格CTest MSVC38/38、GCC/Clang/SAN各37/37，Valgrind API2628 checks0fail、685 alloc/free、0B/0errors。四组合各3次测量全部完成，129源清单SHA957413e59c0f840c741b9bac42bd699df539e50052aa539e81faabc3a86cdf8d与5项程序hash已冻结。

当前mixed单QP样例420字节下降、444字节上升，质量亦变化，不能声称通用压缩收益或BD-rate。报告docs/task-reports/M5.2-integration.md；程序build/m52-root/Windows/bin/SvtAv2EncApp.exe。M5.3仍需更广内容场景的组合质量/成本评估及正式版本终验，M5整体RUNNING、正式仍0.0.4-m4；未动旧M4/M51冻结程序、固定oracle/spec或3fui。

## 38. M5.3 多QP及独立长测开发（2026-10-16）

用户将多QP前置并明确仅CQP+未来CRF的产品路线，取消CBR、2-pass和VBR/VBV独立码率模式。沿用00+A+B，A量化支持全部1..255（10bit实际49..303）及20bit producer系数、规范反量化mask/sign/clip；B补四组系数CDF并逐值保留原q1/q2表；00集成公开接口、参数、真实全QP测试及无人值守评测。

固定AVM主窗口全部6120流/18360帧重建一致；360流/1080帧独立语法trace及6条高幅QP1三方重建通过。八类内容768项四QP含VMAF、720p/1080p288项、GCC/Clang/SAN各360跨平台指纹全部通过；最终Windows QP100的M52四组合576流和M4默认156指纹保持。严格Windows40/40、GCC/Clang/SAN各39/39，API三QP所有权/重试/PTS/EOS 8028checks0fail，Valgrind2053alloc/free、0B/0errors。量化模块30331429独立checks/0fail。VMAF固定模型SHA、工具与源SHA均绑定；PCHIP BD积分299例对SciPy通过。

开发候选app在build/m53-root/Windows-final/bin；Python批测有12项故障/指标回归，真实恢复证明损坏1条只重跑1条、配置变化拒绝。Start-M53Batch.ps1和m53_campaign.py保存Windows/三个Linux可执行文件及脚本模型快照，独立运行、断点续跑和自动审计；campaign-status统一记录完整阶段状态。最终放行使用tools/collect_m53.py，候选与长测分别冻结；计划长测240项、每源1000帧、Windows3次及三个Linux重放，共144万帧编码，未实际完成不算DONE。正式版本仍0.0.4-m4，不启动3fui。

最终续行：高精度768项3次重复测量已全部PASS；候选140源及5程序完成冻结，源清单SHAd827776af7280618e17e20d5e71ac56d1ad708879fd58c07319fcafa0f07cbe7。2026-10-16 22:52已独立启动artifacts/m53-root/long-campaign，当前Windows阶段，运行不依赖聊天；实时读取campaign-status.json，结束后用collect_m53.py --campaign收集，阶段仍待最终验收。


## 39. M6-C 压缩优先方向与 C1 首轮（2026-10-18）

用户指定 `../M5-M6-AI-TASKS/03-M6-single-thread-compression-roadmap.md`，顺序修订为单线程压缩竞争力 → 内部多线程 → preset → CRF。当前 [执行合同](docs/m6-compression-contract.md) 已冻结 Q2 建议门槛、A/B 文件归属、源快照与当前范围。Q1/Q2/Q3 均未达到，不能把基础 RD 或概率内核通过当作竞争力完成。

本轮因用户额度即将耗尽，收敛为 C1-A1 adjusted probability Q12 rate 内核与独立 oracle 测试；双 bank、RD winner 事务、公共组合、真实曲线留待续费后接续。完整 `entropy-adapt on + rd-mode` 仍拒绝，候选/lambda/训练参数不变，正式 0.0.5-m5。进展与准确验收结果记录 `docs/task-reports/M6-C1-first-delivery.md`，新证据外置 `C:/Private/ffmpeg-native-m6-tests/optimization-C1-20261018`。

C0 首次审计原长测 76 冻结文件全部 MATCH，配置 SHA 不变，真实总控 PID17392 存活、阶段 real-quality，105/108 个质量 case PASS、0 FAIL；本记录不是后续实时状态。旧长测继续，不重启、不覆盖。用户澄清 2–4 天是续费后再开发的等待时间，本轮不启动新的长期编码批次。

本轮最终短验：Windows strict 50/50、GCC strict 49/49、ASan/UBSan/LSan 3/3 PASS；fixed AVM 原函数 8,232,960 概率对照/1,966,080 更新，0 错误。18 个四帧兼容用例的新 Windows/GCC 与 R7 frozen 完整流/recon一致且 AVM逐样本匹配，24 条 RD 帧统计亦一致。仅签收 C1-A1 概率内核机制，Clang/C1-A2/完整自适应RD/原生曲线/速度/holdout待续接。已安排当前聊天于北京时间2026-10-21 16:00一次接续，自动任务id `svt-av2`。

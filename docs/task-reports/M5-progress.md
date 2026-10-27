# SVT-AV2 M5：当前会话的真实执行与恢复断点

> **最新：M5.3 RUNNING**。用户授权提前开放非无损CQP1..255并接入VMAF与独立断点长测，未来仅CQP+CRF。报告[M5.3-integration.md](M5.3-integration.md)，教程../M53-批量测试教程.md。所有6120基本组合/18360帧、八内容768项、720p/1080p288项、360实际语法trace、跨平台/检测器、M52/M4指纹、Windows40/Linux39 CTest和Valgrind已通过。长测及正式放行尚待，不提前标M5 DONE；旧QP100限制按历史时点阅读。

> **最新结果：M5.2 DONE，M5整体RUNNING**。报告[M5.2-integration.md](M5.2-integration.md)优先。最小64-to-32内容分区、四组合144矩阵/24大尺寸、90流实际树trace、M4关闭156、MSVC/GCC/Clang确定性、三格式AI/P各1000帧及sanitizer、Valgrind和四组合3次测量已通过。最终严格CTest为Windows38、Linux三配置各37；129源及5程序指纹已冻结。下一步M5.3扩展场景组合质量/成本评估及正式版本终验，正式仍0.0.4-m4。下面各轮状态均按历史时点阅读。

> **最新续行结果优先**：[M5.1-integration.md](M5.1-integration.md)。用户恢复1+2智能体后，AI/P DC/V/H接线、规范/独立mode trace/API专项、144矩阵、24大尺寸、完整M4关闭156指纹、三格式AI/P各1000帧及sanitizer长链均通过；MSVC37、GCC/Clang/sanitizer各36 CTest通过。M5.1 DONE；M5.2分区及M5.3组合待做。下面审计/第一轮记录均为历史，不能再据其说P未实现。

> **当前审计与目标优先入口**：[M5-audit-20261016.md](M5-audit-20261016.md)。当前AI DC/V/H闭环已独立复核，M5整体未完成；本版目标收敛为DC/V/H的AI/P接线、最小内容可变partition及组合终验。下文§2–§7是首轮历史，包含“仅3项/未接线”等当时状态，不代表当前源码。最新对M4有17项变动。

> **2026-10-16后续执行补充（优先级高于下面第一次会话的“G2未开始”历史叙述）**：受限的ALL-INTRA **DC/V/H 真正自适应模式选择**已经完成编码器与bitstream闭环；由用户要求的第二轮会话落实。固定AVM三格式受控66/66通过、1000帧单配置长跑recon完全一致，MSVC/GCC/sanitizer完整CTest全绿，旧M4 39组流/recon逐字节一致。**以 [M5-mode-selection.md](M5-mode-selection.md) 为当前模式选择权威状态**。P帧intra fallback/其余工具及整套M5正式版仍未完成，0.0.4-m4版本未更新。

日期：2026-10-16；设备NuxBox1；工作目录C:\Private\ffmpeg-native\deps-full\AV2\svt-av2。

**总状态：RUNNING / 未发布M5。** M4旧正式生产版本仍为0.0.4-m4。用户要求先落M4→M5开发指南，再由当前ChatGPT会话推进，在本轮可用的工作周期中做多少算多少。本报告区别“已成功实测”“只读静态审计”“规划中的TODO”。

## 1. 文档交付（真实落盘）

- ../M4-M5-AI-TASKS/README.md：方向、里程碑G0–G6、范围。
- ../M4-M5-AI-TASKS/00-集成与调度.md：单会话T037–T046、文件归属、恢复纪律。
- ../M4-M5-AI-TASKS/01-M4基线与M5缺口.md：原版事实及差距。
- ../M4-M5-AI-TASKS/02-工具合同与高级参数.md：feature YAML、各工具族、on/off和preset/码控的界线。
- ../M4-M5-AI-TASKS/03-验证矩阵与验收.md：G0–G6判据。
- docs/m4-m5-contracts.md：数据与公共API/bitstream契约。
- docs/m5-mode-syntax-research.md：固定AVM v1.0.0只读decoder的Y/UV模式语法分析与待解决项。

## 2. G0源冻结与本地依赖

- 本轮在修改生产代码**之前**逐文件复算M4的116项源清单，116/116 SHA256全部相同。原manifest SHA256=23eaf3e1d7d319d403cccdf5296e4cf5fdd2624ac00414c6cedb906cca2296aa。
- 固定spec PDF SHA256=e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f；固定AVM decoder SHA256=110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac（修正原文截断笔误），历史均已复算MATCH。
- 旧build/m4-root/Windows的原版CTest首次执行：34/35；唯一失败m4_validation_self_test为系统Python缺numpy，非编译失败。原始日志：artifacts/m5-root/m4-ctest-prep.log。为不污染系统Python，用python -m pip install --target build/m5-root/pydeps numpy==2.4.6安装了独立依赖；后续CTest须设置PYTHONPATH到此目录。
- 新增源后的差异记录：artifacts/m5-root/source-diff-from-m4.json。**当前116项基线仅3文件发生有意变动**，详见§3。旧M4清单、构建与验收文件均未被覆盖。

## 3. T038 / G1：V/H scalar基本预测函数（真实源码修改）

修改文件仅3项：

1. Source/Lib/Prediction/intra_prediction.h：添加内部SvtAv2IntraAxis与svt_av2_intra_axis_predict接口。
2. Source/Lib/Prediction/intra_prediction.c：scalar V“按列复制above”与H“按行复制left”的显式投影实现，支持8/10bit、非紧密odd-byte stride，非法几何/指针/位深/模式提前拒绝，不改既有DC。
3. tests/unit/test_intra_prediction.c：5组尺寸×100种固定seed×8/10bit×两轴模式，共**2000组基本组合**的独立模型比较与padding sentinel，另测非法axis、stride、几何、NULL参数不写目标缓冲。矩形与方形交错、0/max/random输入。

注意：这只是**数学投影原型**，不代表AV2 v1.0.0完整directional预测过程已得到逐节规范确认，不生成新的V/H语法。生产编码器仍默认DC/已有inter，M5 public API和CLI版本没有改，不能称作已启用V/H编码。

真实构建：独立cmake -S . -B build/m5-root/Windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON；cmake --build build/m5-root/Windows --parallel 6。**MSVC 19.44 /W4 /WX，94/94构建目标成功，exit=0**。运行需要先执行 . ..\activate.ps1 -Quiet。

## 4. 已完成的动态验证（截至本段）

- 新build/m5-root/Windows完整CTest **35/35 PASS**（有PYTHONPATH独立numpy），日志artifacts/m5-root/m5-ctest-first.log，exit=0；其中test_intra_prediction包含新M5 G1 tests。
- 独立Linux GCC 11.4严格Release构建和完整CTest **34/34 PASS**；没有注册Windows固定avmdec的runner_oracle_smoke，因此Linux仅34项属既有构建行为；日志artifacts/m5-root/m5-linux-gcc-ctest.log，exit=0。
- 新build/m5-root/Windows的M4 LOW_DELAY_P关闭新功能路径：三格式×64x64×(2,3,17)帧×(static,translation,mixed)，**27/27 PASS**，固定avmdec解码、internal recon逐样本一致，out=artifacts/m5-root/off-smoke。
- 与真正旧M4 build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe再独立编码同27案例，**27/27 PASS；新旧27项source/stream SHA和internal recon SHA完全一致（0 mismatch）**。对照文件artifacts/m5-root/off-compare.json，旧M4本期输出artifacts/m5-root/baseline-off-m4。
- 注意：这27项是本轮额外精简回归，不取代M4原版全矩阵的108 P + 48 AI终验。

## 5. 语法审计与阻断（T039仍未进入生产）

固定decoder只读快照证实decodemv.c:1510–1540的Y mode有mode_set CDF、mode_index及依邻居的mode列表；:1544–1576 UV模式cdf上下文又依赖Y是否directional，固定enums.h:835–850 V/H均属directional。当前没有对spec所有条件逐页确认，也没有把mode→syntax→内部recon贯通。因此T039–T041未实现、未放行外部feature开关；此状态**不是代码编译失败**，而是按契约阻止生成未经证实bitstream。

## 6. 接下来可以执行的明确任务

1. T039：在同一AVM v1.0.0固定commit下取得get_y_intra_mode_set等对应源码并核对规范页节；验证V/H完整directional数学、mode table/CDF、MRL/edge过滤条件。
2. 先一个64x64 AI V_PRED最小合法stream +固定avmdec像素零失配，再做H、P中intra fallback、420/444与边界。
3. T040/T041：真实API/CLI开关，在完整G2后落盘；禁用严格保持M4字节指纹。没有完整闭环不得开放“可用”参数。
4. 之后依次做动态partition/transform及其他AV2工具的独立feat合同，最终走跨平台、sanitizer、长跑和客观编码收益矩阵。

恢复时执行：先读本文件、../M4-M5-AI-TASKS下README/00/02/03、docs/m4-m5-contracts.md、docs/m5-mode-syntax-research.md，再核对artifacts/m5-root/source-diff-from-m4.json。不应直接据本报告声称未来的G2/G3或M5全部DONE。

## 7. 本轮其余验证的最终补录（全部已实测）

- 默认ALL-INTRA三种格式×64x64×(1,2,3,17)帧：新M5目标12/12 PASS、旧M4目标再跑12/12 PASS。stream SHA和逐帧重建SHA完全一致，12组0不一致。证据：artifacts/m5-root/off-ai、m4-off-ai及off-ai-compare.json，两个verify_m3.py进程exit=0。
- Linux WSL1 GCC 11.4 Debug sanitizer（-DSVT_AV2_ENABLE_SANITIZERS=ON；已检查build.ninja确实有-fsanitize=address,undefined）：94/94目标构建完成，ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest全部**34/34 PASS**。日志artifacts/m5-root/m5-linux-san-ctest.log；Windows专用oracle_smoke在Linux环境未注册，故数量34而不是35。exit=0。
- Linux GCC Release 34/34 + Windows MSVC Release 35/35，全都是本轮修改后独立构建真实运行，并非历史数据。严格编译器告警亦已在MSVC和GCC配置中开启。
- 基于现有M4 verify工具的27 P +12 AI关闭新feature路径全部0像素失配、相比旧M4输出**39/39逐字节一致**；注意只检验选定64x64小矩阵，而不是108+48全部替代。

### 7.1 当前里程碑/工作单元状态

| 任务 | 当前判断 | 证据 |
| --- | --- | --- |
| T037/G0 | DONE | M4原始116/116，规范/decoder SHA匹配，独立环境和初期pytest问题处理 |
| T038/G1 | READY_FOR_INTEGRATION（scalar-only） | 2000组V/H算法比较、非法输入/stride sentinel、三平台模式MSVC/GCC/sanitizers CTest |
| T039/G2 | RESEARCH_ONLY / 未完成 | 冻结AVM模式表审计，完整spec和真实V/H bitstream验收仍缺 |
| T040–T041/G3 | TODO | mode→recon→syntax/feature开关未接，不可公开宣称支持V/H |
| T042–T043/G4 | TODO | 自适应分区和额外coding工具未实现 |
| T044 | PARTIAL | 使用现有M4验证器与本轮SHA反事实比较；独立M5模式探针尚未完成 |
| T045 | PARTIAL | MSVC/GCC与sanitizer已过，Clang跨平台、已启用V/H的1000帧未做 |
| T046/G6 | BLOCKED | G2/G3/G4未完成，正式版本仍是0.0.4-m4 |

### 7.2 断点/未完成原因

核心阻断**并非编译失败**：AV2新的intra方向mode需要根据邻居mode重新排序luma mode set/index，并改变UV mode上下文，完整像素运算又受角度/MRL和edge条件控制。已经从固定AVM同一commit抓取只读reconintra.c到artifacts/m5-root/oracle-v1-research/reconintra.c，详细静态证据在docs/m5-mode-syntax-research.md；**尚未独立编码出新的非DC模式并通过固定AVM**。因此按契约停在G1，未制造未验证生产bitstream。

下一次会话从T039开始（不是重做已通过G0/G1）：先完成完整v1.0.0规范定位和mode index计算模型，再接一个可独立解码、内部/decoder逐像素一致的受控V_PRED测试流。T038功能仅为scalar math接口，不应被用户误认为已经可在CLI中开启。

**本会话未将M5标为DONE、未改公共版本号、未修改固定参考或M4历史build，也未操作FFmpegFreeUI。**

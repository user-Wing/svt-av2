# M2 双窗口集成验收

状态：**DONE**。主窗口终验日期：2026-10-10。版本：`0.0.2-m2`。

本轮依据用户“发送 2 个子窗口完成 M2，主窗口调度，窗口自行交流”的指示执行。M2 已实现并完成下列验收；旧队列历史保留，最终状态以本报告为准。

## 分工与交付

| 窗口 | 完成内容 |
|---|---|
| `m2_syntax` | 真实 qcoeff 系数语法、CDF、tile 分区与邻居上下文；见 `m2-codex-syntax.md` |
| `m2_pipeline` | Codec/CLI、帧与序列头、预测/变换/量化/重建、多帧/PTS/EOS；见 `m2-codex-pipeline.md` |
| 主窗口 | 共享合法编码树、公共接口与版本、独立构建、AVM 解码矩阵、检测器与长跑验收、最终文档 |

两个子窗口已直接交换块序列与系数接口，主窗口完成集成。AVM 源码、规范和 oracle 二进制保持只读。

## 已冻结行为与范围

- 公共 API/CLI 执行预测 → 残差 → 正变换 → 量化 → 系数码流；内部重建使用同一批系数逆量化/逆变换。Codec 复制帧核心 hook 的 qcoeff，每个叶块依次 Y/U/V，避免 scratch 复用导致悬空或覆盖。
- 支持 8-bit progressive 4:2:0、QP 100、单线程；尺寸为非零 8 的倍数，最大 1920×1080。ALL-INTRA、DC_PRED、固定 DCT_DCT、`reduced_tx_set=2`、单 tile、SB64、关闭滤波。
- Common 的 `svt_av2_partition_coded_type` 与 `svt_av2_partition_traverse_coded` 统一 tile 与重建编码树。完整 SB 使用 NONE，边缘按 AVM 合法二叉规则分割，叶块是全可见 8/16/32/64 方块；保留原有可见几何遍历接口。
- 公共 API 支持连续帧、PTS、EOS、调用后复用输入、待取 packet 防覆盖和容量不足后重试；CLI 的 `--frames` 与 `--export-recon` 真实生效。
- 每个 packet 有 temporal delimiter；sequence header 仅首包输出；order hint 为 8 bit，1000 帧验收跨越回绕。
- 显式 M1 入口及回归仍保留。旧 M1 流为 21 字节，SHA256：`7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`。

## 集成中修正的缺陷

- 真实 AVM mode alphabet 为 8；EOB 索引数量为 8，escape literal 为 0 时也必须写入。
- 色度使用自身三邻居系数上下文；扩大边缘上下文并在零块后清理，修正 partition symbol/CDF 与隐式分割。
- `order_hint_bits_minus_1` 的实际位数需加 1；sequence motion mode 标志为 4 个；uniform tile increment 每轴遇首个 0 即停止。
- `base_uv_ac_delta_q` 的 f5 字段是偏移表示，M2 写 23 才表示实际 0；旧 M1 的 0 表示 -23，但零残差回归保留原始字节。
- 系数范围错误与熵编码容量错误同为 -3 的内部枚举冲突已正确映射；超范围输入不会修改输出。

## 独立端到端验收

`tools/verify_m2.py` 生成混合平坦/渐变/棋盘/噪声素材，经公共 CLI 编码并导出内部重建，检查 AVM 输出 Y4M 的真实宽高、帧数和边界，再由原有 `compare_recon.py` 比较全部 Y/U/V 字节。工具拒绝复用已有产物，复验必须使用新目录。错误几何与错误帧数的拒绝路径已验证。

| 尺寸 | 帧数 | 结果 |
|---|---|---|
| 64×64 | 1、2、3、17 | 全部 PASS |
| 176×144 | 1、2、3、17 | 全部 PASS |
| 1280×720 | 1、2、3、17 | 全部 PASS |
| 1920×1080 | 1、2、3、17 | 全部 PASS |

MSVC 矩阵 16/16，证据：`artifacts/m2-root/matrix-r3/acceptance.json`。矩阵之后仅改版本标识、错误映射及验收工具；正常编码路径未再改动。

最终 Clang Release 四种尺寸各 17 帧全部 PASS，证据：`artifacts/m2-root/clang-matrix-final/acceptance.json`。同素材码流 SHA256 与逐帧重建 SHA256 均与 MSVC 对应项一致。

## 连续运行与内存检测

- MSVC Release 单进程连续 64×64、1000 帧：PASS，全部 1000 帧成功解码，内部重建与 AVM 逐字节一致，1000 个重建帧哈希各不相同。
- GCC Debug 开启 ASan/UBSan/LSan，使用相同 1000 帧素材：PASS，无检测器报告，编码器 stderr 为空；逐字节一致且 1000 个帧哈希各不相同。
- 检测器参数：`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`；`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`。
- 两平台码流完全一致，SHA256：`598daee84dd7087e013c44be19c0bf90d5014f16bd97e59ee077190724af1144`。
- 两平台的内部/解码重建各 6,144,000 字节，SHA256：`2280330768ee97de0e1d9da429f6dcb2f6414181ddd077afd36171a298cbf5ef`。

证据：`artifacts/m2-root/unique-cont-final/acceptance.json` 与 `artifacts/m2-root/unique-san-cont-final/acceptance.json`。此前 1000 帧素材仅产生 193 个不同哈希的记录是历史证据；最终素材加入高对比帧签名并要求 1000 个不同哈希。

## 最终构建和单测

| 配置 | 编译器/要求 | CTest | 日志（相对 build/m2-root） |
|---|---|---|---|
| Windows Release | MSVC 19.44、/W4 /WX、clean | 28/28 | Release/build.log、Release/ctest.log |
| Windows Debug | MSVC 19.44、/WX、clean | 28/28 | Debug/build.log、Debug/ctest.log |
| Linux Release | GCC 11.4、-Werror | 27/27 | LinuxRelease-final-build.log、LinuxRelease-final-ctest.log |
| Linux Debug | GCC 11.4、-Werror | 27/27 | LinuxDebug-final-build.log、LinuxDebug-final-ctest.log |
| Linux Release | Clang 23.1.2、-Werror | 27/27 | ClangRelease-final-build.log、ClangRelease-final-ctest.log |
| Linux Debug | Clang 23.1.2、-Werror | 27/27 | ClangDebug-final-build.log、ClangDebug-final-ctest.log |
| Linux Debug | GCC、ASan/UBSan/LSan、-Werror | 27/27 | LinuxSan-final-build.log、LinuxSan-final-ctest.log |

Linux 为 WSL1 的原生 Linux 编译器和程序；本机已有 Clang 位于 `/usr/local/bin/clang-linux`，没有安装新编译器。Linux 未安装原生 AVM oracle，因此单测少一个 oracle_smoke；端到端验收通过 WSL 调用同一 Windows AVM decoder，不将其称为 Linux 原生 decoder。

Common 合法树单测 2746 checks / 0 failures；变换和邻居上下文已有至少 100 个固定种子随机用例，随上述平台单测通过。

MSVC 中文 `/showIncludes` 与 Ninja 的依赖前缀存在编码问题，曾导致结构体变化后的旧对象未重新编译。最终 Windows 构建全部 `--clean-first`；后续结构体/头文件变化时也应清理重建。该构建环境问题未在本轮扩大修复范围。

## 复验入口

从 `C:\Private\ffmpeg-native\deps-full\AV2` 激活环境后，使用新输出目录：

```powershell
. .\activate.ps1 -Quiet
python svt-av2\tools\verify_m2.py --app svt-av2\build\m2-root\Release\bin\SvtAv2EncApp.exe --decoder AVM-AV2-TEST\tools\bin\avmdec.exe --out-dir svt-av2\artifacts\m2-recheck-new --sizes 64x64,176x144,1280x720,1920x1080 --frames 1,2,3,17
python svt-av2\tools\verify_m2.py --app svt-av2\build\m2-root\Release\bin\SvtAv2EncApp.exe --decoder AVM-AV2-TEST\tools\bin\avmdec.exe --out-dir svt-av2\artifacts\m2-longrun-new --sizes 64x64 --frames 1000
```

旧 `run_stability.py` 的 M1 预期不适用于 M2，M2 使用上述入口。`--encode-only` 会明确标注 decoder 未验收，不能单独用于宣告重建闭环。

## 冻结指纹与后续范围

源文件清单：`artifacts/m2-root/source-manifest.json`，共 86 文件，覆盖 Source/include/app/tests/unit/cmake 及入口 CMake、验收工具。清单 SHA256：`ad1a7330b5964b341b9361d1b40c5e9eb33141e09dfc8d3084c9dea186bb0309`。文档不纳入代码清单。

固定 decoder SHA256：`110C7A30B2D429F1C75ADB3340DF149638929863EAAA241E3C3735F3E6B7C1AC`，保持不变。

`smoke-r1`、`smoke-r2`、`san-cont-r1` 属于修正前失败快照，不代表最终失败。旧十队列记录中的 RUNNING/BLOCKED 为历史状态，由本报告覆盖 M2 最终裁决。

M2 的限定 scalar ALL-INTRA 闭环已完成；没有宣告任意 QP、10-bit、帧间编码、RD 搜索、SIMD 或运行期多线程支持，也未启动 M3/M8。

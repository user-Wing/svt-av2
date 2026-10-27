# M5 帧内模式选择：DC/V/H 编码闭环与验收

日期：2026-10-16。工程：`C:\Private\ffmpeg-native\deps-full\AV2\svt-av2`。
状态：**M5 的 ALL-INTRA DC/V/H 模式选择子功能已实现并通过本轮所列真实验证；并非整体 M5 DONE**。当前正式版本继续是 `0.0.4-m4`，新二进制为开发构建，不替换 M4 正式发行物。

## 1. 问题、判定目标与实现范围

原M4对所有帧内编码块只发DC模式，即使前一轮已在 `intra_prediction.c` 有 V/H 标量投影原型。此次要求的是一条真实闭环：**从已重建的边缘像素生成 DC/V/H 候选，依据输入与候选的亮度 SAD 选择唯一模式，再用它算 residual→quantized coeff→encoder 自身重建；tile writer 按同一决策写出 AV2 v1.0.0 模式语法，使固定 AVM 解码得到的像素与编码器自身重建一致**。

- 新的 CLI：`--intra-select dc|adaptive`；省略即`dc`，保持M4默认行为。
- `adaptive`在ALL-INTRA下真实比较Y平面DC/V/H；只有重建邻居存在的方向可以入选，比较取严格小于（平局选DC，候选顺序固定）。
- 同一Y模式决定对应Y样本的prediction/residual/transform/recon与最终码流，防止“内部按V预测，线上却写DC”。色度仍为DC，但**其索引和CDF上下文会随Y的方向性变化**，写端已做相应调整。
- 支持范围仍限原M4的8bit420、10bit420/444、QP100、threads1、单tile/SB64、尺寸为8倍数且≤1920×1080。**只在ALL-INTRA开启**；LOW_DELAY_P指定adaptive明确拒绝，不偷用旧DC、不宣称P帧中intra fallback完成。
- 本轮不是M6完整RD搜索：使用 SAD 是首版可复现决策，不承诺码率、PSNR或速度提升。尚无更复杂的Directional/Angle/MRL/Partition/Transform family；不能称作M5整体交付。

## 2. AV2 v1.0.0 固定解码器侧依据

只读固定decoder `..\AVM-AV2-TEST\tools\bin\avmdec.exe`（SHA256=110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac），以及源仓固定commit `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`。不能以变化中的本地AVM HEAD代替。

- `artifacts/m4-root/oracle-v1-reference/decodemv.c:1510-1540`：Y_MODE_SET、Y_MODE_IDX(3种方向邻居上下文)、后续IDX_OFFSET或4-bit literal及映射为最终模式。
- 固定 `reconintra.h`（`..\avm\av2\common\reconintra.h`用于核对宏/静态排列）给出 `reordered_y_mode`、`default_mode_list_y`；固定源 `artifacts/m5-root/oracle-v1-research/reconintra.c:391-475`给出邻居方向模式插入、邻近角度候选插入和默认序列补全。DC=joint0，零角V=joint22，零角H=joint50。
- Y模式位置经**逐block邻居的联合mode列表**反向查找，不把V/H写死为“symbol1/2”。上下文用above-right与bottom-left两个邻居是否为方向模式决定；发射选择正确mode set和context专用CDF，含offset/literal分支。
- `decodemv.c:1544-1576` / `reconintra.c:481-514`：Y为方向模式时UV mode列表前置对应方向模式，因而指定UV_DC索引为1，采用`uv_mode_cdf[1]`；Y为DC时UV_DC索引0，用`uv_mode_cdf[0]`。
- V/H的零角投影由固定 `reconintra.c` angle=90/180映射到`highbd_v/highbd_h`，与上一轮的scalar投影一致；不自行实现未开启的MRL/angle变体。

上述程序依据结合**实际固定decoder解码+内部重建逐字节一致性**构成此次的bitstream验收，不将源码静态推断单独当作编码成功。

## 3. 主要代码变更与设计

路径均相对于svt-av2根目录。源清单和原始SHA→当前SHA在 `artifacts/m5-root/mode-selection-source-diff.json`；与M4最初116文件基线比较存在17项差异，其中上一轮标量V/H与测试占3项。

| 模块 | 本轮实际更改 |
| --- | --- |
| `include/svt_av2.h` | config尾追加 `intra_mode_selection`，新 `SVT_AV2_PARAM_INTRA_MODE_SELECTION`。旧枚举值不改；**公共struct变更要求使用者重编**，不保证旧二进制ABI。 |
| `app/SvtAv2EncApp/main.c` | CLI开关和参数传递，字符串非法时拒绝。 |
| `Source/Lib/Codec/encoder.c` / `codec.h` | 强制合法配置与LOW_DELAY_P拒绝、记录每个luma编码块所选择的模式，feature为on走专用入口；默认off仍走历史入口。 |
| `Source/Lib/Encoder/intra_frame.c` / `.h` | 基于已重建边缘逐块评估Y的DC/V/H SAD，只有可用方向参与，唯一模式经hook传到写端，重建使用同一模式。 |
| `Source/Lib/Bitstream/tile_writer.c` / `.h` | 用固定decoder的Y mode联合列表和上下文写真正 mode set/index/offset/literal，随Y方向变化切换UV DC的cdf及索引。历史语法关闭路径保留。 |
| `Source/Lib/Bitstream/keyframe.c` / `.h`、`frame_header.h` | 通过内部选项传到tile writer，不改变原M4 header；新增只用于enabled的帧包入口。 |
| `Source/Lib/EntropyCoding/cdf_tables.c` / `.h` | 补UV方向上下文固定CDF（v1.0.0）。 |
| `tests/unit/test_tile_writer_capacity.c` | 为既有三个位置初始化器补新尾字段0，恢复GCC `-Werror=missing-field-initializers` 的严格通过。 |
| `tools/test_m5_mode_selection.py` | 独立合成Y4M并比对AVM输出与内部recon的受控黑箱验收器。 |

**没有替换固定AVM、M4冻结清单、M4原构建或独立FFmpeg最小包，也没有修改FFmpegFreeUI。**

## 4. 真实验证结果（2026-10-16）

### 4.1 已通过的端到端测试

`python tools/test_m5_mode_selection.py`：输出 `artifacts/m5-root/mode-selection/acceptance.json`；本轮最终日志 `artifacts/m5-root/mode-general-test-mixed.log`，退出码0。

- **66/66组 PASS**：三个合法格式，128×64、64×128、128×128、176×144、1280×720、1920×1080，横向/纵向/部分混合/平坦素材，1或2帧。
- 其中**60组非平坦例子**：adaptive与DC之间编码流SHA与编码器重建SHA确实都不同，证明没有出现“只暴露开关、不改变编码行为”的问题。
- **6组平坦素材**：未触发其它模式；两种开关下输出和recon SHA相同，证明成本决策并非强制改变码流。
- 每一组先导出encoder内部recon，再让**固定AVM**独立解码，对所有输出帧逐字节比较零失配；8/10bit、420/444和多帧均覆盖。
- 三种非法选项（未知CLI字符串、LOW_DELAY_P中开启adaptive、非QP100）**3/3明确拒绝**。

### 4.2 默认M4回归

- 新M5构建的`--intra-select`省略路径，对原M4标准`verify_m4.py`的完整`108/108` P组合**PASS**；日志 `artifacts/m5-root/mode-off-m4-full.log`（在模式写端通用化前运行，后来off路径保持不变）。
- 通用化完成后再次独立跑旧M4与新M5相同的P 27项、AI 12项，合计**39项 stream/internal recon SHA逐字节0差异**；输出 `artifacts/m5-root/mode-general-off-27`、`mode-general-off-ai-12`，日志 `mode-general-off-27.log`、`mode-general-off-ai.log`。
- 旧版基线产物 `artifacts/m5-root/baseline-off-m4` 和 `m4-off-ai`。保留旧M4接口输出，未改正式版本。

### 4.3 编译、Sanitizer和真实启用路径

- Windows MSVC /W4 /WX新构建成功，完整CTest **35/35 PASS**，日志 `mode-general-build.log` 和 `mode-general-ctest.log`。
- Linux GCC严格Release构建和CTest **34/34 PASS**；日志 `mode-general-gcc-build.log`、`mode-general-gcc-ctest.log`。Linux比Windows少的runner_oracle_smoke没有注册，不当成省功能。
- Linux GCC Debug ASan/UBSan构建/CTest **34/34 PASS**（`-fsanitize=address,undefined`确实启用）；日志 `mode-general-san-build.log`、`mode-general-san-ctest.log`。
- ASan+UBSan环境还额外真正编码了一份`128×128 10-bit444 mixed`、`--intra-select adaptive`的码流，独立固定AVM解码输出98304B与encoder内部recon 98304B逐字节相同；`artifacts/m5-root/asan-mode-smoke/`有输入/stream/recon/decoded/encode.log。

### 4.4 新功能1,000帧连续解码长跑（本轮实际完成）

除前述66个短矩阵案例外，额外以独立生成的**8-bit420、128×128、水平梯度、1,000帧**输入运行真实`--intra-select adaptive`，**固定AVM解码1,000帧后与编码器内部重建逐字节完全一致**；输出recon为24,576,000字节，无错帧/截断；命令及输入生成器脚本保留在`artifacts/m5-root/mode-long-check.py`，完整日志`artifacts/m5-root/mode-long-check.log`，exit=0。

- 输入SHA256：`b36fed5296179dee56f57bc864c00b7c961d14945f7beec9d6c3fa382266d4d5`
- 真实AV2码流SHA256：`9131cdb71e18def169389d240e203edc3a53de0147109088e766d79162be7083`
- 编码器重建SHA256：`8aab4237095ef1b93ca6ea03c9b47fdafb2f20a2bc79f73fa036c468f00dc386`

该长跑证明这一种配置的编码顺序和上下文未因连续1,000帧发生失配，**不等于**3格式×AI/P全矩阵的1,000帧验证已完成；LOW_DELAY_P仍不支持此特性。

## 5. 使用与结果边界

在`svt-av2`根目录使用已有开发二进制：

~~~powershell
. ..\activate.ps1 -Quiet
.\build\m5-root\Windows\bin\SvtAv2EncApp.exe --input input.y4m --output output.av2 --export-recon internal.yuv --pred-structure all-intra --intra-select adaptive
~~~

撤销实验只需省略`--intra-select`或设为`dc`。CLI会验证输入格式、QP等旧范围，LOW_DELAY_P使用adaptive被拒绝。

- **这是M5 Coding Tools中的一个可用实验性子功能**：DC/V/H模式选择已完成AI真实编码闭环，但尚不是新正式发行版本。
- LOW_DELAY_P的帧内fallback、多方向角度/MRL、动态partition、完整RD、多QP/preset/CRF、Clang交叉编译和新功能多配置1000帧长跑仍未在此次验证里全覆盖。M5版号依旧 `0.0.4-m4`。
- 下一步从LOW_DELAY_P里intra fallback和独立mode trace/更广随机场景开始；任何新特性都重复“编码内部recon冻结→固定decoder→逐像素/帧SHA”验证，不允许只依赖编码器日志。

## 6. 本轮恢复点

真实产物：

- `artifacts/m5-root/mode-selection/acceptance.json`（66组逐案例具体SHA与结果）
- `tools/test_m5_mode_selection.py`（可重复生成输入并比较）
- `artifacts/m5-root/mode-selection-source-diff.json`（对原M4的逐源码SHA）
- `artifacts/m5-root/mode-2block-snapshot/`（通用化前7源文件工作快照）
- `docs/task-reports/M5-progress.md`（整体M5进度，今后先读）
- 固定规范与 oracle SHA、阶段定义：`docs/m4-m5-contracts.md`。

注意：从此报告拿走的`--intra-select adaptive`依赖**本轮独立开发构建**，未回写旧M4产物，也不代表FFmpeg最小包已经支持该CLI参数。

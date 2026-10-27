# M0–M1 接口与边界契约

负责人：00 集成与调度会话。本文件只记录**已冻结**的约定；未定项列在最后，不得由执行会话自行猜定。
工作根目录：`C:\Private\ffmpeg-native\deps-full\AV2`；新实现根：`svt-av2\`。

## 1. 路径与版本控制

- 实际根目录：`C:\Private\ffmpeg-native\deps-full\AV2`。新实现全部位于 `svt-av2\`。
- `avm\` 是指向 `C:\Private\ffmpeg-native\deps-full\avm` 的目录联接，**只读**；`AVM-AV2-TEST\` 内的 oracle 二进制**只读**。
- 当前 `AV2\` 不是 Git 仓库。若建立 Git，范围**仅限**新建 `svt-av2\`；不得把整个 `ffmpeg-native` 纳入新仓库。
- 共享目录模式（非 worktree）：严格按文件归属修改，见第 7 节。构建/结果分目录：`build\<task-id>\<config>\`、`artifacts\<task-id>\`。

## 2. 语言与构建（T001 冻结）

- 语言/标准：**C11**，`CMAKE_C_EXTENSIONS OFF`，不引入第三方运行时依赖。
- 生成器：**Ninja**；Windows 用 **MSVC x64**，Linux 用 GCC/Clang。`CMAKE_BUILD_TYPE` 默认 `Release`，Debug/Release 都要验证。
- 目标名：
  - 库：`svt_av2_common`（static）、`svt_av2_enc`（static）、`svt_av2_enc_shared`（shared，可关）。
  - 应用：`SvtAv2EncApp`。
  - 单测：由 `tests\CMakeLists.txt` 注册，经 **CTest** 运行。
- 公共头：`include\svt_av2.h`。共享库用 `SVT_AV2_BUILD_SHARED`/`SVT_AV2_USE_SHARED` 控制导出。
- 编译告警：MSVC `/W4 /utf-8`；不默认 `WARNINGS_AS_ERRORS`。
- 环境：构建前必须 `. C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1`（注入 MSVC/cmake/ninja；cmake/ninja 不在 PATH）。
- 源码文件一律 **UTF-8**（本机 `write` 工具与仓库现有文件均为无 BOM UTF-8；PowerShell 控制台若显示乱码属显示问题，不代表文件编码错误）。

## 3. 测试注册归属（00 决策：方案 B + 串行交接）

- `tests\CMakeLists.txt` 名义归属 **T002**（统一测试注册）。
- 但当前该文件缺失导致根 CMake `add_subdirectory(tests)` 使 configure 失败。为不阻塞关键路径 T001：
  1. **T001 立即创建** 最小 `tests\CMakeLists.txt`：使用**显式列表**（禁止 `file(GLOB)`）注册 **仅 T001 自己的** `tests\unit\*.c`，并 `add_test`。
  2. T001 完成验收后，将 `tests\CMakeLists.txt` 的**所有权交接给 T002**（交接时点记入报告）。
  3. 此后 T003/T004/T005/T006 的单测源由 T002 串行注册；任何会话不得并发修改该文件。
- 根 CMake 对 `add_subdirectory(tests)` 使用**存在性守卫**（T001 已实现）：tests 入口缺失时默认 `BUILD_TESTS=ON` 的 configure/build 仍成功，不阻断库/应用。
- 守卫**不得**用来隐藏测试缺失：验收阶段 `tests\CMakeLists.txt` 必须存在且被 CTest 实际驱动；否则不算通过。
- 本轮执行口径（00 明确）：**授权 T001 立即按方案 B 创建** 最小 `tests\CMakeLists.txt`（显式列表、仅注册 T001 单测），跑通 CTest 后把该文件**交接给 T002**；T002 之后串行扩展注册其余会话单测。

## 4. 最小 C API（T001/T005 冻结）

保留六个入口：`svt_av2_enc_init_handle`、`set_parameter`、`init`、`send_picture`、`get_packet`、`deinit`（见 `include\svt_av2.h`）。

- handle 所有权：`init_handle` 分配，`deinit` 释放并将句柄置空；重复 `deinit` 安全。
- 输入 picture：M0 不定义像素缓冲格式；`send_picture` 在编码未实现时返回可区分的未实现/无输出状态，**不得返回虚假成功**。
- packet：M0 `get_packet` 返回 `has_packet=0`；packet 缓冲由调用方提供并拥有，库不得保存悬垂指针。
- EOS：`send_picture(NULL)` 表示 EOS；M0 下返回无输出状态。
- M0 **不生成任何 AV2 码流**。

### 4.1 关于既有 M0 占位容器（00 决策）

- 现有骨架含 `svt_av2_m0_build_placeholder_payload()` 与以 ASCII 魔数 `"SVT2M0"` 开头的开发占位容器（`codec.h` 已注明**不是 AV2 syntax**）。
- **保留**该既有代码（不删除用户/骨架既有工作）；但它**不得**被当作合法 AV2 码流：魔数使 decoder 与 runner 可明确拒绝。
- 应用在 M0 不得以“编码成功”退出；若写出该开发容器，必须明确标注为非 AV2 开发产物，且不得被 runner 判为 PASS。
- 该占位容器**不得**演变为 public API，也不得作为 M1 的过渡手段。

## 5. writer / OBU 契约（T003 提出，00 待其接口冻结后确认）

- 归属：`Source\Lib\Bitstream\bit_writer.*`、`obu_writer.*`、`Source\Lib\Bitstream\CMakeLists.txt`（模块级）、对应 `tests\unit\`。
- 位序、容量/增长策略、错误返回、`finalize`、OBU payload 交接由 T003 定义，T004/T005 必须复用，**不得出现第二套 BitWriter/OBU Writer**。
- LEB128 范围/最大字节/canonical 表示以 AV2 规范为准。
- OBU 仅实现 M1 必需组合，其余明确拒绝；普通 BitWriter ≠ 熵编码器。
- **接口已冻结（T003，2026-10-06）**，头文件 `Source\Lib\Bitstream\bit_writer.h`：
  - 位序 MSB-first；`f(n)`、`leb128()`、`trailing_bits(nbBits)` 对齐规范 4.11.2/4.11.6/5.2.3。
  - 容量：调用方固定缓冲，writer **不分配/不增长**；越界返回 `NO_SPACE` 且不提交半个 bit。
  - 错误码：`0` 或负值（NULL/INVALID_ARG/NO_SPACE/UNALIGNED/RANGE/STATE/UNSUPPORTED）。
  - `finalize()` 要求字节对齐，之后写入返回 `STATE`。
  - OBU payload 交接：用本 writer 构建 payload → `svt_av2_obu_payload_finalize()` → finalize → `svt_av2_obu_write()`（leb128 长度前缀 + header + payload）。
  - `leb128` 范围 `[0, 2^32-1]`、canonical 最短形式（≤5 字节）、字节对齐（规范 4.11.6）。
  - OBU 头：`ext_flag` 固定 0（规范 6.2.1），header 1 字节 `= (type<<2)|tlayer`；M1 仅接受 `SEQUENCE_HEADER/TEMPORAL_DELIMITER/CLOSED_LOOP_KEY/REGULAR_TILE_GROUP/REGULAR_SEF/PADDING`，其余 `UNSUPPORTED`。
    - **00 裁定（2026-10-06，M1 集成）**：补入 `CLOSED_LOOP_KEY`（type 4）。依据 AVM 只读 oracle：`avm/av2/decoder/decodeframe.c:7606-7608`（首帧必须是 CLK/OLK/RAS）与 `:8015`（`single_picture_header_flag=1` 时 “Still pictures must be coded as closed loop keyframes”）。T004 冻结了 `single_picture_header_flag=1`，故 M1 单帧必须用 CLK OBU。此前白名单遗漏 CLK，为契约缺口，已由 00 修正（T003 归属文件）。
  - `svt_av2_obu_write()` 输出 `leb128(num_bytes_in_obu)+header+payload`，其中 `num_bytes_in_obu = header+payload`（**含 header，不含 size 字段本身**，Annex B）。
- 验证：Debug/Release 各 2 测试全绿（`test_bit_writer` 52656 checks、`test_obu_writer` 46 checks），含 2000 组随机位 + 5000 组随机 LEB128 与独立参考模型交叉比对。00 独立复现：全新构建 `10/10 Passed, exit 0`。
- **当前冻结文件基线（00 复核，2026-10-06；含 00 的 CLK/ns 集成改动）**：
  - `bit_writer.h` `480F2FF9654BBD49D02B3A8565003E72F00DF090F155355C2BA0D98B63D5F9AB`
  - `bit_writer.c` `8DFB70C74CC76EBBA29874C853C03CB2C0CCF872E4A3E505499A0CD64AAAD3B6`
  - `obu_writer.h` `3E681DCC8D609612095436F1EE9798D72D2A7066566EAFA18046E08D684C3054`
  - `obu_writer.c` `7C34454025FFB51447BC59C0349B2BD7112B93EC0756F211EBB8A1F255A7306F`（含 00 加 CLK）
  - `sequence_header.h` `0DF7B4FE6EF8CE310514C257DD25BB5C8D2183FFAD2C9B64AA80C9AFF569E67C`
  - `sequence_header.c` `0D405DA615730FD0956A073DED70C0FBED56E5BA45B6FC63BB02FA6EDB67F70E`
- T004/T005 必须复用该接口，不得另造第二套 BitWriter/OBU Writer。
- **描述符原语归属（00 裁定，2026-10-06，M1 集成）**：`uvlc()`（规范 4.11.3）与 `ns(n)`（规范 4.11.8）属**位写入层**，唯一实现在 `bit_writer.{h,c}`。起因：T003 于 22:09 在 `bit_writer.c` 加入 `uvlc`，而 T004 于 22:06 已在 `sequence_header.c` 内实现 `uvlc`+`ns`，产生重复符号（LNK4006）。00 已将 `ns` 迁入 `bit_writer.{h,c}`、删除 `sequence_header.{h,c}` 中的重复定义/声明；`sequence_header.c` 继续调用 `bit_writer` 的实现。功能等价（两版 `uvlc` 算法一致），00 复核无回归。**T004 不得再在 `sequence_header.*` 内定义位写入原语；T005 亦复用 `bit_writer`。**
- **模块 CMake 注册**：`sequence_header.c` 由 00 加入 `Source\Lib\Bitstream\CMakeLists.txt`（T004 无该文件写权限）。该模块现编译 `bit_writer.c`、`obu_writer.c`、`sequence_header.c`。

## 6. 最小编码配置（T004/T005 冻结）

- 目标：8-bit、4:2:0、单帧、固定分辨率、All-Intra、固定 QP、单线程。
- **已由 T004 依规范核实（00 确认）**：
  - `bit_depth_idc`：8-bit 对应 **1**（spec Table 6.3：0→10bit、1→8bit、>1 保留）。API 层的 `SVT_AV2_BIT_DEPTH_8=8` 是枚举值，**不得**当作 idc 写入码流。
  - `base_q_idx`：8-bit 时按 `f(n)`、`n = BitDepth==8 ? 8 : 9` 写入，故 **8-bit 合法范围 [0,255]**（`MaxQ = MAXQ_8_BITS`），**不是 AV1 的 [0,63]**。
- T001 骨架参数校验暂用 `[0,63]` 占位，**偏窄**；T004/T005 冻结后由 00 通知应用侧改为 [0,255]。
- **00 冻结 M1 编码配置（2026-10-07；T004 依规范建议，00 采纳）**：
  - 固定分辨率 `max_frame_width=height=64`。
  - `seq_header_id=0`（uvlc < 16）、`seq_profile_idc=0`（Table A.1 Main_420_10_IP0）、`single_picture_header_flag=1`。
  - SB = 64x64（`use_256x256_superblock=0`、`use_128x128_superblock=0`）；64x64 帧 = **1 tile 1 SB**。
  - `seq_tile_info_present_flag=0`（不写 tile_params，解码器按帧尺寸派生单 tile）。
  - `seq_level_idx=0`（Table A.8 level 2.0；MaxPicSize=147456、MaxHSize/MaxVSize=640，64x64=4096 均在限内；最低 level 无 1.0）。
  - 尺寸位宽：64x64 → `frame_width_bits_minus_1=frame_height_bits_minus_1=5`，`max_frame_*_minus_1=63`。
  - 帧级 `base_q_idx`（属 T005 帧语法）：8-bit `f(8)`，范围 [0,255]。**最终取值 base_q_idx=100**（T005 依规范分析：非 lossless（>0）时每平面只需 3 个 `coeffs()` 符号；lossless(=0) 反而需要 258 个，故 100 是**最小熵符号数**路径，且确定性最强）。00 采纳。
  - 帧级固定项（T005 设计，T004/00 核对）：`tx_mode_select=0`、`disable_cdf_update=1`、`allow_screen_content_tools=0`、`allow_intrabc=0`、`uniform_tile_spacing_flag=1`、`segmentation_enabled=0`、`using_qmatrix=0`、`delta_q_present=0`、`deblocking[0..1]=0,0`、`reduced_tx_set=0`、所有 filter 关、`film_grain=0`。
  - 最小 tile/block/熵路径：1 SB；`do_split=0` → `PARTITION_NONE` → 单个 64x64 block；熵符号共 **7 个**：`do_split`、`y_mode_set`、`y_mode_index`、`uv_mode`、`all_zero`×3。
  - 帧头：22 bits（single_picture=1、CLK、64x64、base_q_idx=100、tx_mode_select=0、disable_cdf_update=1、filter 全关）。
  - 预期序列头（64x64, L0）：payload `82 0A 55 FF F0 00 04 00 10 00 08`；OBU（TD 后续）`0B 04 ...`。
- 熵编码实现依据：AVM `avm_dsp/entenc.c` 的 range encoder 与规范 §8.2 `read_symbol` 等价；T005 已核对 `avm_od_ec_decode_cdf_q15_c`（entdec.c）与规范逐式一致 → 规范编码器可被 `avmdec` 解码。AVM 仅作交叉验证。
- **M1 验证结论（00，2026-10-07）**：本节冻结配置已产出首个自有码流并通过固定 `avmdec` 独立验收：`t005_m1.av2`（21B，SHA256 `7AAB59865D8423A128ADBB8D65CF5D08087CE35061FF5324B0D9F620842CE0F7`）→ exit0、1 帧、6144B、3×SHA256 一致；解码 Y/U/V 全 128（DC_PRED 零残差）。详见 `docs\task-reports\integration.md`。
- **M1 最外层 OBU 组装归属（00 裁定）**：TD+SEQ_HEADER+CLK 的组装由 **T005** 提供单一顶层库入口（`svt_av2_encode_keyframe_64x64`），T001 应用只调用、不复制语法；Codec 由 00 串行 link `svt_av2_bitstream`。
- 上述固定尺寸/profile/level 已由 00 写入本节并通知 T001（应用侧）与 T005（帧语法）。

## 7. 文件归属（责任边界）

| 范围（相对 svt-av2\） | 责任会话 |
|---|---|
| 根 CMakeLists.txt、include\svt_av2.h、最小 API、app\SvtAv2EncApp\、README.md、LICENSE | T001（跨模块注册由它串行处理，之后 tests 注册交 T002） |
| tests\CMakeLists.txt（T001 交接后）、tests\conformance\、tests\corpus\、验证脚本、benchmark schema | T002 |
| Source\Lib\Bitstream\bit_writer.*、obu_writer.*、对应 unit | T003 |
| Source\Lib\Bitstream\sequence_header.*、对应 unit、docs\bitstream-sequence-minimal.md | T004 |
| frame_header.*、tile_writer.*、最小 EntropyCoding\、最小块编码、单帧单测、docs\bitstream-frame-minimal.md | T005 |
| tools\compare_recon.py、比较工具自测 | T006 |
| project.md、docs\m0-m1-contracts.md、阶段汇总、docs\spec\ | 00 |

- 公共 API 变更须先经 00 核对是否为当前最小闭环必需，再由原负责人串行修改。
- 禁止多人同时追加 project.md；各任务交报告，00 统一回写。

## 8. 验证 CLI 与 runner 契约

- oracle 固定：`AVM-AV2-TEST\tools\bin\avmdec.exe`（T002 负责版本/hash/来源核对，见其报告）。
  - `avmdec.exe` SHA256 `110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`
  - `avmenc.exe` SHA256 `a9d2ae4fa5af7170d4bf56440098c5819ff2eccdd26a1553e8887fc6625fb97b`
  - 版本串（T002 实测，2026-10-07）：`avmdec -v` → `AOMedia Project AV2 Decoder 1.0.0`；`avmenc -v` → `Codec : AOMedia Project AV2 Encoder 1.0.0`（`avmenc` 64-bit）。注意 `-v` 需真实输入文件，`--version` 非有效选项。只读，不得修改。
  - 来源说明：文档 commit `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` 在本地仓库不存在（`git cat-file`=bad object），对上游与文档 commit 的精确对应**无法自证**，故记“**来源部分确认**”（见 T002 报告）。
- runner 必须检查退出码、超时、输出存在/非空、帧数、尺寸、raw **精确字节数**；缺工具/依赖必须 BLOCKED/失败，**绝不 PASS**。
- **00 的 M1 独立验收命令（2026-10-06 固定）**：对 T005 产出的码流执行
  `python svt-av2\tools\run_matrix.py --check-bitstream --bitstream <stream> --width W --height H --frames 1`
  → 退出 0 且打印 `PASS: ... decoded to N bytes (1 frames)`；N 必须 = `(W*H + 2*(W/2)*(H/2))*1`。另需同配置重复 3 次编码，SHA256 一致。
  - **执行结果（00，2026-10-07）**：对 `artifacts\T005\frame_min_64x64.bin`（21B，SHA256 `7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`）→ exit 0、1 帧、raw **6144** 字节、**PASS**；3×SHA256 一致；应用 `SvtAv2EncApp --output` 产出码流与之逐字节一致。详见 `docs\task-reports\integration.md`。
- M0 benchmark JSON 记录 parse/init，编码 fps 为 null 或省略并写原因；不伪造编码 fps。
- 具体 JSON 字段/schema 版本由 T002 定义并写入本节。

## 9. raw 比较契约（T006 提出，00 批准）

工具：`svt-av2\tools\compare_recon.py`。首版仅支持紧密排列 **8-bit 4:2:0**，显式宽高与帧数；10-bit/444/padding/stride 显式报错。

- CLI：`--a <expected> --b <actual> --width W --height H --frames N [--json] [--text]`
- 退出码：`0`=完全一致；`1`=像素不一致；`2`=参数/用法错误；`3`=文件或长度/布局错误。
- 长度必须精确等于 `W*H*frames + 2*(W/2)*(H/2)*frames`；**双侧同样截断也失败**。
- 报告 `first_mismatch{frame,plane,x,y,a_offset,b_offset,expected,actual}`、各平面 mismatch 计数与 hash。
- hash 不能替代差异定位与长度检查；不做容差/重采样/颜色转换。
- T002 的 runner 调用本工具并解释其退出码，**不复制比较算法**。
- 当前基线（T006，2026-10-06）：`tools\compare_recon.py` SHA256 `F2DCF70A2D8308A4E06B86C5B8FB108EF72C4AC97B894C997A8D0AABC79E22DC`；`tools\test_compare_recon.py` SHA256 `CE6A520866C6C3F55DAB68FB3E4BDE9D342E8423BB4F14423CB1292545BEC936`。25 个单测通过；无真实 internal recon 可比，工具可用 ≠ M1 recon 通过。

## 10. 规范真值来源（00 固定）

- AV2 Bitstream & Decoding Process Specification **v1.0.0**（Final Deliverable 2026-05-28）。
- 来源 URL：`https://av2.aomedia.org/v1.0.0/20260528_38f28e7_AV2_Spec_v1.0.0.pdf`
- 本地固定副本：`C:\Private\ffmpeg-native\deps-full\AV2\docs\spec\AV2_Spec_v1.0.0_20260528_38f28e7.pdf`
- SHA256：`E9916F091E4E83446AAD6B4601641C5B292E569C144C4163B26A4497573B533F`
- AVM 仅作交叉验证；AVM 与规范冲突时以规范为准。
- AVM 来源核对（T002）：本地 `avm` 源 HEAD `01803db6afa5a0616702a3814fc5934ab9e99adf`（2026-09-23，branch main）；文档记载的 commit `966a7d7c` 在本地不存在 → 记“**来源部分确认**”。oracle 以可执行文件 SHA256 固定为准。
- corpus：`.y4m` 为**可再生产物**，由 `tests\corpus\gen_corpus.py` 生成、`check_manifest.py` 对照 `manifest.json` 校验；不作为需要提交的源（若日后建 Git，保留 `*.y4m` 忽略规则，不添加例外）。

## 11. 会话通信

见 `AGENTS.MD` 第 7 节；花名册 `scripts\session-roster.json`；工具 `scripts\oc-msg.ps1`（默认 `delivery=queue`，不打断目标会话正在执行的步骤）。

## 12. 未定项（禁止猜测）

- 最小固定分辨率、profile/level、工具开关、QP/qindex 映射（T004+T005）。
- writer/OBU 接口签名（T003）。
- runner 与 benchmark JSON schema（T002）。
- Linux 构建环境是否存在（T001 记录“未验证”即可）。

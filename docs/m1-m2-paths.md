# M1→M2 文件归属与首轮派发（00 冻结）

日期：2026-10-07。负责人：00 集成与调度会话。本文件是 T007–T016 的**唯一写权限依据**；与本文件冲突的旧描述以本文件为准。

基线说明：本文件冻结时 `svt-av2\` **不是 Git 仓库**，一致性以第 3 节的 SHA256 为准。所有相对路径以 `svt-av2\` 为根。

## 1. 为什么必须先冻结归属

T007–T016 是 10 条**并列**会话，共用一个工作目录、一个源树。共享目录模式下没有 Git 的分支隔离，两个会话同时写同一个文件会**静默互相覆盖**，而这轮的多数模块（transform/quant/syntax/recon）本身又是"改了才看得出错"的数值代码——被覆盖后可能仍然编译、仍然"看起来通过"。因此归属不是礼节，是正确性前提。

规则：

1. 只写第 2 节表里属于你的路径。**不要**"顺手"改邻近文件。
2. 需要别人的文件改动 → 发消息给属主并抄送 00，不自行接管。
3. 公共文件（根 CMake、模块 CMake、`tests\CMakeLists.txt`、`include\svt_av2.h`、`project.md`、`AGENTS.MD`、`README.md`、本文件、契约文件）**只有 00 写**。
4. 模块 CMake 与测试注册由 00 串行处理：你把源文件和测试源写好、报告里给出需要注册的 target 名与文件名，由 00 加入构建。
5. 只用 `build\<task-id>\` 与 `artifacts\<task-id>\`，不要共用构建目录（并发写同一 Ninja 目录会互相破坏）。
6. 只读：`avm\`、`AVM-AV2-TEST\tools\bin\` 下的 oracle、`docs\spec\`。任何情况下不得为了让测试通过而改 oracle 或改比较工具。

## 2. 文件归属表

### 2.1 新增模块（本轮 M2 的核心）

| 路径（相对 `svt-av2\`） | 唯一属主 | 说明 |
|---|---|---|
| `Source\Lib\Common\picture.*`、`block_geometry.*` | **T007** | plane/stride/padding/块几何 |
| `Source\Lib\Prediction\`、`Source\Lib\Common\residual.*` | **T008** | 最小 scalar intra 与有符号 residual |
| `Source\Lib\Transform\` | **T009** | 正/逆变换 |
| `Source\Lib\Quantization\` | **T010** | 量化/逆量化 |
| `Source\Lib\Encoder\intra_frame.*`、`Source\Lib\Reconstruction\` | **T012** | 真实像素 frame core 与内部 recon |
| `Source\Lib\Bitstream\sequence_header.*`、`frame_header.*` 及对应 header 测试源 | **T015** | 从 T013 划出 |
| `tools\check_platform.py`、`tools\run_stability.py`（或等价专用入口）、平台/长跑测试源 | **T016** | 从 T014 划出 |

### 2.2 既有文件（沿用 M0–M1 归属，本轮继续）

| 路径 | 属主 | 备注 |
|---|---|---|
| `Source\Lib\EntropyCoding\*`（`entropy_encoder.*`、`cdf_tables.*`、`block_writer.*`） | **T011** | 复用现有 range coder，**禁止另写第二套** |
| `Source\Lib\Bitstream\tile_writer.*` | **T011** | |
| `Source\Lib\Bitstream\keyframe.*` | **T013** | |
| `Source\Lib\Codec\encoder.*`、`codec.h`、`app\`、`Source\Lib\Common\y4m.*` | **T013** | |
| `tools\run_matrix.py`、`tests\corpus\`、`tests\conformance\`、benchmark/schema 工具、验证文档 | **T014** | |
| `cmake\SvtAv2Oracle.cmake` | **T014（仅经 00 明确交接后）** | 见 §4 |
| `Source\Lib\Bitstream\bit_writer.*`、`obu_writer.*` | **无人（已冻结）** | 见 §4，发现问题报 00 |

### 2.3 公共文件（只有 00 写）

根 `CMakeLists.txt`、各模块 `CMakeLists.txt`、`tests\CMakeLists.txt`、`include\svt_av2.h`、`project.md`、`README.md`、`AGENTS.MD`、`docs\m0-m1-contracts.md`、`docs\m1-m2-contracts.md`、`docs\m1-m2-paths.md`、`docs\task-reports\M2-integration.md`。

## 3. 冻结基线 SHA256（2026-10-07，00 实测）

比对方法：`(Get-FileHash -Algorithm SHA256 -LiteralPath <file>).Hash`。

### 3.1 公共与骨架

```text
EF8085521B16FF896D2DAF7A99FE35DAAD116C6B80BEDBD88200ECE595206369  svt-av2\CMakeLists.txt
5F831F0BF1A4870D5BD17E2F025B92D91530E989D6008F749FCADF10FBBB4CA3  svt-av2\include\svt_av2.h
06FF19193FAC4507195CC39914174C66A2B4F51F0FD5A703D05E1A2098CE52BB  svt-av2\tests\CMakeLists.txt
```

`include\svt_av2.h` 于 2026-10-07 20:29 由 00 加入 §7.2 的 `SvtAv2ReconFrame` + `svt_av2_enc_get_recon` 声明（原 `2B7F538D…` 作废）。

### 3.2 Common / Codec / app（T013、T007 关注）

```text
EFDAE8D846BD2C92C54310E01A97A8B8F0B71C22878EB74FE5D6E4C7B1C93D50  Source\Lib\Common\common.h
6D51E44B7E233D7846F62D8DCD29E9C623418AE073736D03C306C426AC7CE8E0  Source\Lib\Common\common.c
8025C2862FFB26F1BBF7ADFC8A7B5349292634EBEC72995CF2FC1FCE4DFF6315  Source\Lib\Common\y4m.h
1C3BE2AFB616D30F68561DC93530896A0ACAAFA5D5D4F69123C55292E03DFE39  Source\Lib\Common\y4m.c
058F28405EE09DC173B3F22E62314651C1F01CA3963293906DF6FDCE11E98287  Source\Lib\Codec\CMakeLists.txt
F5775DDB3C3D900A9DFF8A9B9D233A54429C102C39FB23671F668F4DD3D6E369  Source\Lib\Codec\codec.h
A7423B0729F95D66B3F8E653C23E29CDDA29CA9432416BE186861C4F5ECD84BF  Source\Lib\Codec\encoder.c
```

> **2026-10-07 20:34 在途态（T013 P2 未落盘完）**：上表 `codec.h` / `encoder.c` 是**改动前**快照（`F5775DDB…` / `A7423B07…`）。T013 正在把 source 缓冲从 `frame_copy`（tight）改为 allocated `frame_source`，磁盘 mtime 分别为 20:29:11 / 20:31:13，**当前树 `build\00` 尚不能链接**（`encoder.c:317/318` 仍引用旧名）。T013 落盘完并报新 hash 后由 00 更新本节。
> `Codec\CMakeLists.txt` 的 `058F2840…` 是 **00 本轮落地的构建边**：为 `svt_av2_enc` / `svt_av2_enc_shared` 加 link `svt_av2_encoder_core`（后者 PUBLIC 导出 `Source/Lib/Encoder` 目录并提供 `svt_av2_intra_frame_encode`），解决 `codec.h:13 #include "transform_path.h"` 的 C1083。**无环**：`svt_av2_encoder_core` 不回指 `svt_av2_enc`。

### 3.3 Bitstream / EntropyCoding（T011、T015 关注）

```text
480F2FF9654BBD49D02B3A8565003E72F00DF090F155355C2BA0D98B63D5F9AB  Source\Lib\Bitstream\bit_writer.h
8DFB70C74CC76EBBA29874C853C03CB2C0CCF872E4A3E505499A0CD64AAAD3B6  Source\Lib\Bitstream\bit_writer.c
3E681DCC8D609612095436F1EE9798D72D2A7066566EAFA18046E08D684C3054  Source\Lib\Bitstream\obu_writer.h
7C34454025FFB51447BC59C0349B2BD7112B93EC0756F211EBB8A1F255A7306F  Source\Lib\Bitstream\obu_writer.c
2AB00A282723DFD544509CE77C0DCE7323FF3725EF211B5800EED9AC1FD7AEF6  Source\Lib\Bitstream\sequence_header.h
717747DBFC78D4AE4202D401501F88C516D3338DDE637DF16D2E6369E804D22F  Source\Lib\Bitstream\sequence_header.c
9FB89DB1360A8F6E0A8CF67818E48EDD662870209DC835FDEB2A5D46CE545210  Source\Lib\Bitstream\frame_header.h
8EDE585D94BE1AE14F048640434FE9E178376C3EDB861FA6CA2AD08D37D33766  Source\Lib\Bitstream\frame_header.c
6C8A7F39F5123C32BE5E7872D888AC4746C6D1F435B44C61596D8822C9574E11  Source\Lib\Bitstream\tile_writer.h
CCA2065B4C4DF1E9B3A6C90835F645F982EC91FF11643B43C5BB6B4EA4F07E88  Source\Lib\Bitstream\tile_writer.c
ADC6BB3A023C1CBE037903DA3736A523B4D81CA1F19A0C8D23935565718A7A49  Source\Lib\Bitstream\keyframe.h
49706DD4ED509D05888D69D153AF68B967BFD479D971C68A24289F78608BCE72  Source\Lib\Bitstream\keyframe.c
C54BAAEB17C460DB5BBF771426596A5512C61B6F0403C21F1A843F5C0DB3E6D4  Source\Lib\EntropyCoding\entropy_encoder.h
A48F1B5902B86F9192E30D43A88044807BFE9822AF294F8604A5C9C55E68D1AD  Source\Lib\EntropyCoding\entropy_encoder.c
73B739BE808A76886A90C4C6269AAE9C71FB8B4F1D7C138C2D6AAED5A52A9D9B  Source\Lib\EntropyCoding\block_writer.h
B68C6149284B1BFDA7F2A2C4A7E9DB295446332748C172AD08C067015321EAAC  Source\Lib\EntropyCoding\block_writer.c
E8A0CCE5032B544CD593060840225991500F28559980AA319B30656C4207DE54  Source\Lib\EntropyCoding\cdf_tables.h
C54902E1AF9F99DDAD6AD27C89F2DFE01FD6409FC06594E870209643E88994D5  Source\Lib\EntropyCoding\cdf_tables.c
AE6B8F1FF669068FCBD1F6975692320B868C495CC66AC9EAD39C2428AB6E2F6B  Source\Lib\EntropyCoding\cdf_tables_coeff.h
64A45CF2A9AF1AC94532EE038C21924EAA84C5FF4C65A83ABD26121673659879  Source\Lib\EntropyCoding\cdf_tables_coeff.c
818E4842EC59C52A8E0A6DA32E1161D0F3515C132BF2AB423292A5848963BF34  Source\Lib\EntropyCoding\coeff_syntax.h
00A4E7EF019E686D7C90FEAA141DC53A942CBA4A564069824A7721AE3AEAFE62  Source\Lib\EntropyCoding\coeff_syntax.c
AEA461E213E68B9F4A050657A6796C09F4E5BC1D1CB47F270A703B8D67D65A65  tests\unit\test_header_frames.c
```

注意：`bit_writer.*` / `obu_writer.*` 的 hash 与 `docs\m0-m1-contracts.md` §5 记录的**完全一致**，说明 M0–M1 冻结后无人改动，本轮可放心在其上构建。`sequence_header.*` / `frame_header.*` **已随 T015 的 M2 改动漂移**（M1 基线见 `m0-m1-contracts.md` §5：`sequence_header.h 0DF7B4FE…`、`sequence_header.c 0D405DA6…`），一律以本节 §3.3 的当前值为准。

### 3.4 验证工具（T014、T016 关注）

```text
48132100DF8FED5801B6A5530CA60CE8BD865FD2EA1747BD20CA3FE228261CA0  tools\run_matrix.py
F2DCF70A2D8308A4E06B86C5B8FB108EF72C4AC97B894C997A8D0AABC79E22DC  tools\compare_recon.py
D24876716A9A7EE4941B270816760EB4A94EE278791C6C9BB27EF5924135BD8F  cmake\SvtAv2Oracle.cmake
```

`compare_recon.py` 与 `m0-m1-contracts.md` §9 记录一致（T006 冻结后未变）。

### 3.5 帧内重建链路（T012 关注）

```text
CB0C388E8EDFB762A5B72788ED9F479BBC6E196C6CA2BB86BA8F4FAC6B41DA2A  Source\Lib\Encoder\transform_path.h
C56DDC33C530D67D5311DAD983C40D7D254CCBAEFABE23799F30A9C3DC02A8C4  Source\Lib\Encoder\transform_path.c
FE3F1CE04E53184C04F7A4E385BFC46BD248C997C6813B8B96688262D14C2090  Source\Lib\Encoder\intra_frame.h
79C8CF964E835580AAD4F40FE4538ED0014D1B1856771BBC6F569662D7609F02  Source\Lib\Encoder\intra_frame.c
4B418E18C0E598728C0D202FC61669060BF67F7D0A357025257FB14ACA0095B0  Source\Lib\Reconstruction\reconstruction.h
56FC2E2A1A9100B8668C9D51AE695E405C810D7C3F455C23766C9FDE84615611  Source\Lib\Reconstruction\reconstruction.c
A6A2BDA7935075B6F70331E95FB20194D4A180A2B4007896E11C34B47EC19056  tests\unit\test_transform_path.c
DDDA749673CA9EBED2A58ADEE3778D6464F8005C7090999B0EBDE65F03D9BB45  tests\unit\test_intra_frame.c
2CF4F9AC205E72410F0611D272001A6986CFBFD3ABDD8B64B7A1CC591CCF104F  tests\unit\test_reconstruction.c
```

属主：T012（9 文件全部静止）。登记时点 2026-10-07 23:3x，00 当轮复算，size/hash 逐项 MATCH。
`transform_path.c` 的 `min(w,32)` 原生系数布局已由接缝级探针 `artifacts\T012\probe\probe_layout_agreement.c`（`63A9334A…`）证明与 T011 读端一致（count/scan=1024、无 slot ≥ 1024、首块非空洞 937/1024）。

## 4. 已完成的交接裁定（禁止"直接接管"）

1. **`sequence_header.*` / `frame_header.*`：T013 → T015。** 依检查报告，T013 尚未开工 M2，无未完成改动需要交接；文件 hash 见 §3.3。从本文件发布起，**只有 T015 写**这两个文件及其测试。T013 改为向 T015 提配置需求并调用其接口。
2. **平台/sanitizer/长跑：T014 → T016。** T013/T014 尚未开始 M2 工作，无并发接管风险。`tools\run_matrix.py`、`tests\corpus\`、比较工具、schema 仍归 T014；T016 只**新增**专用入口，复用 T014 的判定入口，**不复制** decoder/比较算法。
3. **`cmake\SvtAv2Oracle.cmake`：仍归 T014。** 该文件当前 SHA256 见 §3.4。T014 可先复现检查报告指出的"开头重置 `SVT_AV2_ORACLE_DIR`、疑似覆盖用户 `-D` 值"疑点；**复现后**（而非仅静态阅读后）再作最小修复。修复前不得更改该文件。
4. **`bit_writer.*` / `obu_writer.*`：本轮无人属主。** 若任何人发现其中确有 bug，报 00 决定，不自行修改——它们是 T003/T004/T011/T013/T015 的公共地基，单方改动会波及所有已冻结向量测试。

## 5. 构建与测试约定

- 构建前必须 `. C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1`（cmake/ninja 不在 PATH）。
- 你的构建目录：`svt-av2\build\<task-id>\<config>\`（例：`svt-av2\build\T009\Release`）。**不要**用别人的构建目录。
- 你的产物目录：`svt-av2\artifacts\<task-id>\`。
- 单测源写在你自己的模块目录或 `tests\unit\` 下**你自己的新文件**里；注册由 00 做。报告里写清：target 名、源文件路径、需要 link 的库。
- 模块 CMake 由 00 写。若你的新模块需要新目录，在报告里说明目录名与需要 link 的依赖，由 00 加 `add_subdirectory` 与 target。

## 6. 首轮派发（Q1–Q10）

十队列**同时**启动。首轮目标是各自可独立推进的部分，不要等待上游"全部完成"才动手，也不要为了让首轮好看而写占位实现冒充闭环。

| 队列 | 任务 | 首轮可独立交付 | 接线等待点 |
|---|---|---|---|
| Q1 | T007 | plane/stride/padding/边缘几何 + 测试 | 交 T008/T012 的接口 |
| Q2 | T008 | predictor 规范依据 + 独立邻居向量 | 等 T007 布局冻结 |
| Q3 | T009 | transform 尺寸/尺度/舍入 + 独立 kernel 测试 | 与 T010 同步尺度 |
| Q4 | T010 | 人工 coeff 的 quant/dequant + 测试 | 等 T009 真实 coeff |
| Q5 | T011 | 人工 qcoeff、scan/EOB/context 测试 | 等 T009/T010/T015 配置 |
| Q6 | T012 | clipping/邻居更新/导出契约 + 测试 | 等 T007–T011 真实模块 |
| Q7 | T013 | M1 API 出 packet、frames 缺口、输入循环 | 等 T012/T015 接 M2 |
| Q8 | T014 | runner 几何误报修复、负例、素材/矩阵 | 等真实 M2 产物做 recon |
| Q9 | T015 | 多帧字段表、尺寸/level 语法、header 单测 | 等 T011/T013 完整流 |
| Q10 | T016 | 现有 M1 的跨平台/sanitizer 检测环境核对 | 等 T013 多帧后跑 M2 长片 |

关键接线链（不可绕过）：T007→T008，T009/T010→T011，T007–T011→T012→T013，T015 与 T011/T013 汇合，T014/T016 只做验收。

## 7. 合入门槛（G0–G4）

- **G0 过渡基线**：状态摘要修正、runner 几何误报修复、T013 接通公共 API 并纠正帧数语义。
- **G1 scalar 模块**：T007–T010 独立测试通过；逆路径有规范/固定 AVM 交叉验证；T011 人工系数正负例通过。
- **G2 64×64 真实像素**：非平坦输入产生非零残差；内部 recon 与 decoder 精确一致；输入变化确实改变 qcoeff/重建。
- **G3 多块与尺寸**：176×144、720p、1080p 真实尺寸、非零系数、recon 验证；含右/下边缘不完整块与 chroma 邻接。
- **G4 多帧与稳定性**：1/2/3/17 帧逐像素一致；1000 帧无 crash/leak 有证据；Debug/Release 与平台/sanitizer 覆盖完整。

00 按**小闭环**受理 `READY_FOR_INTEGRATION`，不等 10 个任务全部完成才集成。每次只跑受影响的检查与已规定门槛，避免各队列并发占用同一构建目录或同时进行性能/长跑测量。

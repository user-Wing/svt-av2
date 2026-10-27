# M3 主窗口集成验收

日期：2026-10-11。状态：**M3 DONE**。版本：`0.0.3-m3`。用户要求的最终FFmpegFreeUI操作见末节。

## 范围与分工

本轮依据用户全自动开发授权，由00主窗口与两个Codex子智能体完成，不使用OpenCode。必要构建/检测工具本机已具备，没有修改或重编oracle。

| 执行者 | 交付 |
|---|---|
| A `/root/m2_pipeline` | T017/T018/T019/T020/T023：存储/几何、预测、正逆变换、量化、重建/帧核心；报告 `m3-codex-core.md` |
| B `/root/m2_syntax` | T021/T022/T024：系数CDF/tile、位深色彩流头、API/CLI/Y4M；报告 `m3-codex-syntax-api.md` |
| 00主窗口 | T025/T026：公共接口、独立判定器、平台与AVM矩阵、长跑/内存检测、源码冻结与版本/报告 |

实际支持：8-bit420回归、10-bit420、10-bit444；user QP100、threads1、ALL-INTRA、DC_PRED/DCT_DCT、单tile/SB64、关闭滤波，非零8倍数尺寸最大1920×1080。8-bit444、8/10-bit422、12-bit、400明确拒绝；未启动M4/M8、任意QP、RD、SIMD或FFmpeg wrapper开发。

## 实现与冻结接口

- FrameBuffer/BorrowedView携带bit_depth/chroma_format；planes和strides保留字节单位。10-bit以unsigned u16le低10位有效表示，逐字节load/store，接受非对齐地址和奇数字节stride。copy-in先全帧校验0..1023，越界不截断且不部分写入。
- picture alloc/view、geometry、transform、quant和path新增最小ex/bd入口；旧8/420包装保留。420色度half mapping，444色度与luma尺寸一致，统一coded traversal和Y/U/V真实qcoeff顺序。
- 预测reference uint16，残差int16；coeff/qcoeff/dqcoeff int32，中间乘加int64。10-bit inverse stage按AVM位宽clamp，最终recon按0..1023裁剪；8-bit结果不变。
- QP100的实际qindex：8-bit100、10-bit148；quant_QTX分别704、2816；系数CDF类别分别1、2。10-bit帧头qindex字段9bits，8-bit8bits；profile0用于42010，profile4用于44410；UV delta literal23表示实际0。
- CDF类别2从固定AVM表提取，先逐数核对22张类别1表与旧生产表一致，再生成类别2；未用AV1值替代AV2规则。
- 公共ReconFrame末尾追加bit_depth/chroma_format，chroma宽高按实际格式返回。该struct布局变化要求调用者重新编译，不宣称旧二进制ABI兼容。packet/PTS/EOS/输入复用/无partial容量retry原契约保留。
- raw导出继续唯一picture实现，紧凑Y→U→V、不含padding；10-bit每样本2字节，444不再误用420长度或元数据。

规范/AVM字段和精确函数签名见两窗报告及 `../m2-m3-contracts.md`。profile4的level picture-size检查采用luma面积，当前level helper仅尺寸边界，不宣称完整fps/bitrate/decoder-model等级符合性。

## 起点与回归

M2原86文件逐项hash核对并复制到 `artifacts/m3-root/m2-baseline-source`。新G0 MSVC /WX clean构建28/28与16项AVM矩阵通过，证据 `artifacts/m3-root/g0-matrix/acceptance.json`。最终M3的8-bit420四尺寸×四帧数16项码流/逐帧重建SHA全部与G0相同；8-bit420连续1000帧也与M2冻结结果相同。显式21字节M1回归由keyframe/header单测继续验证。

## 48项独立生产链验收

| 配置 | 64×64 | 176×144 | 1280×720 | 1920×1080 |
|---|---|---|---|---|
| 8-bit420 | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS |
| 10-bit420 | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS |
| 10-bit444 | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS | 1/2/3/17 PASS |

最终MSVC Release版本的全部48项PASS，每帧全部Y/U/V样本零差异。证据：`artifacts/m3-root/matrix-final/acceptance.json`。最终CLI提示文字从M2改为M3后重新编译，三配置1000帧再次通过；该提示文字不参与码流。

Clang Release三配置×四尺寸×17帧12/12PASS，码流SHA和每帧重建SHA均与MSVC完全相同，证据 `artifacts/m3-root/clang-matrix-r1/acceptance.json`。最终版本标识/提示更新后64×64三配置×3帧再次通过且SHA一致，证据 `clang-stage-final/acceptance.json`。

`tools/verify_m3.py`从公共CLI编码，读取固定AVM写出的真实Y4M头验证宽高/位深/色度，严格帧边界/长度，调用独立raw比较器；不以input metadata覆盖decoder输出。素材含0/1/255/256/511/512/1022/1023、低两位变化、渐变、棋盘、噪声和不同U/V。source与有损recon不要求相同，判定目标是internal recon==decoder recon。

判定器原25例与新增6组正负例在Windows/Linux通过，覆盖面积相同但几何不同、位深/格式误标、端序、U/V交换、低bit单点差、frame count、短/多余payload与超范围样本。CLI四负例明确拒绝8-bit444、10-bit422、sample1024和截短10-bit444，证据 `artifacts/m3-root/cli-negative/results.json`。

## 连续1000帧与内存检测

MSVC最终Release三种配置各64×64连续1000帧全部PASS，1000个重建帧hash各不相同，所有帧逐样本与AVM一致；同一CLI进程运行，不通过重启掩盖状态问题。证据 `artifacts/m3-root/longrun-final/acceptance.json`。

GCC Debug ASan/UBSan/LSan最终版本10-bit420/444各1000帧PASS，无检测器诊断，编码器stderr为空；码流和逐帧重建hash与MSVC相同。证据 `artifacts/m3-root/san-longrun-final/acceptance.json`。

环境：`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`，`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`。泄漏结论来自检测器，不用工作集平稳替代。

| 配置 | 1000帧raw字节数 | 码流SHA256 |
|---|---:|---|
| 8-bit420 | 6,144,000 | `598daee84dd7087e013c44be19c0bf90d5014f16bd97e59ee077190724af1144` |
| 10-bit420 | 12,288,000 | `5df3e4d1eced9dd8f27f1ee9664b24ba96a0caa1947be76c00db06f30fbcfc68` |
| 10-bit444 | 24,576,000 | `4cf0e41453132652aa9afd2284ca153da324fd1f6858ccc3abe98f3905614aac` |

## 最终平台与模块测试

| 构建 | 编译器/要求 | CTest |
|---|---|---|
| Windows Release/Debug | MSVC19.44 /W4 /WX，clean | 各29/29 |
| Linux Release/Debug | GCC11.4 strict -Werror，clean | 各28/28 |
| Linux Release/Debug | Clang23.1.2 strict -Werror，clean | 各28/28 |
| Linux Debug sanitizer | GCC ASan/UBSan/LSan，clean | 28/28 |

最终版本标识更新后上述全部构建/CTest再次通过；最后app提示变化仅重新构建各app target。原始日志：`artifacts/m3-root/{Release,Debug,LinuxRelease,LinuxDebug,ClangRelease,ClangDebug,LinuxSan}-{configure,build,ctest}.log`；app最终编译日志 `*-app-stage.log`。

Linux为WSL1原生编译器/编码器；Linux无原生AVM，端到端测试通过WSL调用只读Windows avmdec。Windows多一个oracle smoke，因此CTest数不同。Windows中文showIncludes存在既有增量头依赖风险，struct/header变更后全部clean重建。

独立数学测试：11个实际TX形状×8/10bit×100固定seed的forward独立matrix模型和inverse独立AVM shift/clamp模型；各square×bd×100 DC邻居/量化向量，10-bit残差、raw/padding/原子拒绝与100随机clip。API单测179272checks/0，覆盖非对齐odd-stride、copy后覆写、sample1024/65535、容量retry、444元数据和PTS/EOS；完整日志见两窗报告。没有用forward→inverse自比较替代逆路径依据。

## 冻结与复验

最终97文件源/头/构建/单测/工具清单：`artifacts/m3-root/source-manifest.json`，SHA256 `89c8b3bc4b7b2809678cab05598d40c00d18d0c45346c9920b1010378c3c3328`。汇总：`artifacts/m3-root/final-summary.json`。文档与媒体不纳入源代码清单。

固定oracle SHA256 `110C7A30B2D429F1C75ADB3340DF149638929863EAAA241E3C3735F3E6B7C1AC`；规范PDF SHA256 `E9916F091E4E83446AAD6B4601641C5B292E569C144C4163B26A4497573B533F`。AVM源码/工具/规范均只读。

从 `C:\Private\ffmpeg-native\deps-full\AV2` 复验，输出目录必须全新：

```powershell
. .\activate.ps1 -Quiet
python svt-av2\tools\verify_m3.py --app svt-av2\build\m3-root\Release\bin\SvtAv2EncApp.exe --decoder AVM-AV2-TEST\tools\bin\avmdec.exe --out-dir svt-av2\artifacts\m3-recheck-new --sizes 64x64,176x144,1280x720,1920x1080 --frames 1,2,3,17
python svt-av2\tools\verify_m3.py --app svt-av2\build\m3-root\Release\bin\SvtAv2EncApp.exe --decoder AVM-AV2-TEST\tools\bin\avmdec.exe --out-dir svt-av2\artifacts\m3-longrun-new --formats 10:420,10:444 --sizes 64x64 --frames 1000
```

旧verify_m2保留不改；compare_recon旧8/420默认和schema字段保持兼容，新增10-bit支持。工具--encode-only仅说明编码执行成功，不能用于宣告oracle闭环。

## 用户要求的最终界面操作

M3开发与终验完成后，已通过computer-use点击FFmpegFreeUI“加入编码队列”，观察到两个ffmpeg进程启动（PID15324、19048）。随后用户以Escape停止computer-use；当时未完成与编码进程关联的GPU占用独立确认，不能据通用GPU计数器声称显卡编码验证通过。

2026-10-11后续用户明确说明已手动停止编码，并要求准备M3→M4开发文档。界面队列动作已执行，GPU确认仍未完成；本次不恢复编码。此界面事项与已完成的M3编码器开发/解码一致性验收分别记录。

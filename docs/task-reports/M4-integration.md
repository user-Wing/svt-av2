# M4 主窗口最终验收

日期：2026-10-11。状态：**DONE**，生产版本`0.0.4-m4`。主窗口00=/root，两个子智能体A=/root/m2_pipeline、B=/root/m2_syntax。按用户要求，本轮未操作FFmpegFreeUI/3fui。

M4实现了最小视频编码链：首帧key，后续单向P，使用上一张编码器内部重建作为唯一参考。默认ALL-INTRA码流保持M3逐字节指纹。公共API、CLI、真实系数、内部重建和AV2语法共用一条生产路径。

## 可用产物与范围

Windows Release：`C:\Private\ffmpeg-native\deps-full\AV2\svt-av2\build\m4-root\Windows\bin\Release\SvtAv2EncApp.exe`；同目录`svt_av2_enc_shared.dll`。Debug及GCC/Clang五种构建见artifacts/m4-root/final-verification.json，包含14个最终app/shared产物SHA256。

CLI使用`--pred-structure low-delay-p`启用P，省略或使用`all-intra`保持默认AI。公共配置新增prediction_structure枚举与尾字段，旧参数编号不变，调用方须用本版头文件重新编译；不承诺旧二进制config ABI兼容。

支持8-bit420、10-bit420、10-bit444，QP100、threads1、scalar、8倍数尺寸至1920×1080、单tile/SB64/DCT_DCT。亮度整数ME搜索每轴[-8,+8]的289个候选，SAD及确定性tie；420奇数MV按规范色度半相位插值，负坐标floor、逐样本边缘扩展。模式为DC intra或单参考inter，P中允许intra回退。

不支持8-bit444、8/10-bit422、12-bit/400、任意QP、B帧/重排序、多参考/compound、亮度subpel搜索、完整RD、SIMD、运行期多线程或FFmpeg wrapper。此静止素材收益不代表通用视频压缩效率。

## 主窗口验收

| 门槛 | 实际结果 | 主要证据（均在artifacts/m4-root） |
|---|---|---|
| 默认AI回归 | 48/48，stream及逐帧recon SHA与M3全部一致 | all-intra-final/acceptance.json、all-intra-final-compare.json |
| P生产矩阵 | 108/108逐样本zero mismatch | matrix-r13/acceptance.json |
| 独立运动与参考 | 12/12，零/偶数/奇数正负MV；非零9项证明P→P刷新 | motion-r13/acceptance.json |
| 静止码率及质量 | 三配置全部满足P字节≤50% AI、P MSE≤1.05×AI MSE+1 | rate-r13/acceptance.json |
| 连续P链 | 三配置MSVC及GCC sanitizer各1000帧，cross-platform流及逐帧hash相同 | long-windows-r13、long-san-r13、long-platform-final-compare.json |
| Clang确定性 | 三配置×四尺寸×17帧×三素材族36/36，与MSVC所有流/逐帧hash相同 | clang-matrix、clang-platform-compare.json |
| 版本更新后复验 | 七构建全绿，AI/P/两平台1000帧指纹不变 | final-verification.json、version-ai、version-p-windows、version-p-clang、version-long-windows、version-long-san |
| 分配及释放 | 最终M4二进制9/9 Valgrind用例，退出时0B/0blocks未释放 | allocation-final/acceptance.json |

P矩阵为三配置×64×64/176×144/1280×720/1920×1080×2/3/17帧×static/translation/mixed。素材使用固定seed、非周期YUV纹理，含偶/奇数正负位移、边缘、静止及源画面突变。首帧独立key，1帧不计入P验收。内部recon在固定decoder调用之前生成并冻结hash；decoder只独立解码，没有回填内部recon。

运动验收独立生成两次连续规范预测，再替换首key为不同的独立key；要求两P逐样本跟随替代参考经一次、两次MV/MC的结果。非零运动可区分刷新与陈旧参考，零MV只证明参考读取，不假称能区分刷新。独立Python模型使用完整8tap两级round；生产采用已证明等价的2/4样本平均。ME模块另用无early-stop的289候选真值模型测试三配置各100种移动与边界、平坦tie。

API测试三配置各258帧双handle，核对输入copy、单张in-flight、packet/recon容量不足的sentinel与重试、PTS、帧索引/元数据、EOS及255/256/257附近状态；实际wire hint环绕由1000帧固定decoder验收。参考使用两个私有预分配buffer，prepare深拷贝、commit无分配swap，仅在完整packet序列化成功后提交一次。序列化失败闭锁且不暴露半帧recon。

## 静止收益（128×128、64帧、同源同QP）

| 配置 | AI总字节 | P总字节 | P/AI | AI总MSE | P总MSE |
|---|---:|---:|---:|---:|---:|
| 8-bit420 | 290580 | 9991 | 3.438% | 31.873698 | 21.063219 |
| 10-bit420 | 302164 | 6775 | 2.242% | 499.536092 | 334.911234 |
| 10-bit444 | 395348 | 9946 | 2.516% | 693.120687 | 440.498980 |

计完整AV2 elementary stream，含首帧和流头。MSE按全部原生幅度样本合并，10-bit单位不与8-bit直接比较；逐plane/逐帧MSE及PSNR留在rate-r13 JSON。三配置内部与decoder仍逐样本一致。

## 平台与内存证据

| 构建 | 编译器及告警 | 最终CTest |
|---|---|---:|
| MSVC Release/Debug | 19.44.35228.0，/W4 /WX | 各35/35 |
| GCC Release/Debug | 11.4.0，strict -Werror | 各34/34 |
| Clang Release/Debug | 23.1.2，strict -Werror | 各34/34 |
| GCC Debug sanitizer | ASan/UBSan/LSan | 34/34 |

Windows CMake3.31.6-msvc6、Python3.11/NumPy2.4.6；WSL1 Ubuntu22.04，CMake4.4.3、Python3.10.12/NumPy1.21.5。环境完整输出见windows-environment.json、linux-environment.json。Linux encoder为原生GCC/Clang程序，oracle由WSL互操作调用固定Windows avmdec；不宣称测试了Linux原生固定oracle。

sanitizer使用detect_leaks=1、halt_on_error=1及UBSan stacktrace，所有完整CTest及三配置1000帧无检测器错误。最终普通GCC二进制的Valgrind2/17/1000帧分配次数各33/93/4025，全部对应释放，累计分配随帧数增长，退出live allocation为零。原始日志保留，未把累计分配量当作live heap或RSS。

verify_m4记录每条命令wall time与OS内存采样；performance-diagnostics.json汇总。该轮108项MSVC编码耗时合计202.338秒，采样峰值RSS为92839936B，属于诊断数据，无FPS门槛。WSL1本机/proc RSS/HWM读数为0，视为不可用，不声称实际内存为0；Massif实测三配置1000帧live heap峰值分别137853/168574/254590B，完整快照/趋势见heap-final/acceptance.json，输出码流与正式验收hash相同。ASan和普通构建的内存不可直接作同口径性能比较。

## 语法问题的闭合证据

固定参考以AVM v1.0.0 commit`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`为准；本地AVM junction较新HEAD不能代替它。A另建同版本Debug诊断副本，原固定Windows oracle和规范未改。副本原始/诊断SHA、补丁和日志在artifacts/m4-a/debug-oracle-source，下载参考的官方URL/SHA在artifacts/m4-root/oracle-v1-reference/manifest.json。

三个实际缺陷均已由真实decoder轨迹定位并修复：P帧头单参考仍须skip_mode_flag=0；inter小TX的DCT_IDTX集合中DCT_DCT索引为1；固定MVREF_COLS=2仅扫描到远左-3，不允许额外-5候选。后者只在源突变、近邻转intra时暴露，原176×144 mixed第14帧首差异块(80,128)已由内部/fixed Windows/独立Debug三者全17帧hash一致闭合。

早期最小entropy reader漏实际header分支，不能证明原失败流合法；该错误与纠正均保留。r8、smoke-r11及初轮matrix-final（100/108）失败证据未覆盖，没有降低阈值、移除mixed17或缩短序列来得到PASS。最终算法以r13完整108/108为准。

## 冻结与复验

M3起点97/97 SHA静态一致，baseline-check.json。最终生产源/头/CMake/单测/工具116文件清单source-manifest.json，SHA256：

`23eaf3e1d7d319d403cccdf5296e4cf5fdd2624ac00414c6cedb906cca2296aa`

全部门槛先在冻结r13源码完成（pre-version-gates.json）；之后只改CMake、公共头、build info和API测试的版本/阶段文字，算法未变。七配置再次clean构建，最终app/shared SHA与版本复验见final-verification.json。M3产物未覆盖，M1 21-byte SHA仍为`7aab59865d8423a128adbb8d65cf5d08087ce35061ff5324b0d9f620842ce0f7`。

固定oracle SHA256：`110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`；规范PDF SHA256：`e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f`，终验再次核对一致。

从svt-av2目录复验，输出目录必须未使用过：

```powershell
. ..\activate.ps1 -Quiet
cmake --build build/m4-root/Windows --config Release --clean-first --parallel 8
ctest --test-dir build/m4-root/Windows -C Release --output-on-failure --timeout 180
python tools/verify_m4.py --app build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder ../AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir artifacts/my-m4-matrix
python tools/verify_m3.py --app build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder ../AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir artifacts/my-m4-ai --sizes 64x64,176x144,1280x720,1920x1080
python tools/verify_m4_motion.py --app build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder ../AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir artifacts/my-m4-motion
python tools/verify_m4.py --app build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder ../AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir artifacts/my-m4-rate --rate
python tools/verify_m4.py --app build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder ../AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir artifacts/my-m4-long --sizes 64x64 --frames 1000 --patterns mixed
```

各matrix JSON保存实际命令/退出码/耗时/stderr/source与stream及逐帧hash。Linux五配置clean构建与完整CTest命令在artifacts/m4-root/run_linux_final.sh，原r13与最终版本日志均保留；sanitizer环境在linux-environment.json。没有未完成的M4功能门槛，M5及以后未启动。

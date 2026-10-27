# M6-I 帧内首轮开发与实测（2026-10-24）

## 结论

**I-GATE 未运行、体积目标尚未达到，正式仍为 0.0.5-m5。** 已按用户指定的 `04-M6-intra-only-first.md` 切换到单图、单线程、体积优先；原帧间、内部多线程、preset/CRF 路线不列入当前开发。先达到同质量 AOM 体积约 −15%，再把编码耗时压到约 2×。

这轮完成了可运行的 single-picture 入口、基础七种 Y/UV 预测及单图自适应 CDF/RD 事务子集。没有完成方向角/angle delta/wide-angle、全部分区/TX、RDOQ、高级预测或滤波，不把这些子项的正确性通过称为 I1/I2/I3 全部完成。

证据根目录：`C:/Private/ffmpeg-native-m6-tests/intra-20261024`。现行合同：`../m6-intra-contract.md`。

## 已落地的编码闭环

1. 公共配置尾部新增 `still_picture`，参数 `SVT_AV2_PARAM_STILL_PICTURE`，CLI 为 `--still-picture`。使用生产系数链写真正的 `single_picture_header_flag=1` 和 `CLOSED_LOOP_KEY`，支持现有 8bit420、10bit420/444；API 拒绝第二张图片，CLI 只读一帧并拒绝显式多帧/帧间组合。
2. 修复旧 single-picture 序列头的色度量化偏移：零偏移必须写五位编码值 23，而非 0（固定 AVM 对后者解释为 −23）。此前零残差灰图会掩盖此错误；新非零残差样例与 AVM 重建已一致。对应旧单图字节向量随规范修正，旧冻结文件未覆盖；生产多帧默认关闭路径保持指纹。
3. 新增默认关闭的 `--intra-select basic`：DC/V/H + SMOOTH/SMOOTH_V/SMOOTH_H/PAETH，Y 与共用的 U/V 模式分别选择。同步实现预测、joint-mode 重排、方向/非方向 CDF 上下文、UV mode list、量化/逆变换/clip 后真实失真和最终语法。UV 显式置 `enable_chroma_dctonly`，避免非 DC 色度隐式选择尚未实现的变换。
4. AV2 的实际 highbd smooth 原语用于两种 bit depth；它使用额外 TR/BL 端点及整数有符号舍入，不能沿用旧 8-bit helper 的二次权重。端点仅从已解码 SB/前序叶读取，不能读尚未重建的源/缓存。
5. 原 DC/V/H 树搜索结果作为入口，在该树上做一轮逐叶 Y/UV 坐标搜索。每个候选从同一个 SB 入口重建完整叶序列，后块读取本候选的前块重建，再通过真实 tile 符号顺序计算 R/D/J。不是只比较 Y-SAD 或非零系数数目，也不是所有模式/树/TX 的全局最优搜索。
6. `--entropy-adapt on` 可与单图 RD 联用：committed/scratch bank 深拷贝；失败/未提交候选丢弃 scratch；赢家重放校验 D/R/重建/系数后再提交。自适应成本使用既有 C1 的 AVM 位置调整概率，覆盖相等 raw CDF 边界。多帧 RD+CDF 及系数/运动优化组合继续拒绝。

使用方式（实验功能，所有开关默认仍关闭）：

```powershell
build/i3-20261024/bin/SvtAv2EncApp.exe --input input.y4m --output image.obu `
  --still-picture --threads 1 --intra-select basic --partition-select adaptive `
  --rd-mode pruned --entropy-adapt on --qp 100
```

新增公共字段要求调用方重编。没有将 AVM 链入生产库，独立 oracle 只用于测试。

## 主基准与三张原生开发图

实际 libaom v3.15.1 使用 `--allintra`，该参数直接选择 `AOM_USAGE_ALL_INTRA`；文档中的 `--ai` 不存在。本地源码及官方参数定义已核对。保持 `--threads=1 --cpu-used=0 --end-usage=q --limit=1 --passes=1 --tile-columns=0 --tile-rows=0 --obu`，没有换快档或 AVIF。源码 commit `44d0a57786f432d933ff64b653347c66f4d0fa1d` 已在隔离目录重新构建，构建参数和 CMakeCache 保留；18 个 AOM 质量流及解码像素与原基线逐字节复现。

本轮实际开发曲线只用既有 2D/3D/Real 的固定第 48 帧，每张 1920×1080、10bit420，不缩放。冻结相同 Y4M 输入、VMAF v0.6.1 模型、三平面 SSE/PSNR、raw OBU 全字节和全部原始点。AOM Q 为 16/24/32/40/48/56；AV2 QP 为 40/70/100/130/160/190。按真实解码质量计算 PCHIP 对数码率 BD-rate，数值 Q/QP 不直接对齐。

| 开发帧 | 基础七模式相对旧 RD | CDF/RD 相对七模式 | 最终组合相对旧 RD | 最终组合相对 AOM |
|---|---:|---:|---:|---:|
| 2D | −6.29% | −10.91% | −16.50% | **+103.35%** |
| 3D | −3.18% | −12.05% | −14.87% | **+69.25%** |
| Real | −5.15% | −9.79% | −14.38% | **+55.82%** |

每个表格单元独立使用各对曲线的共同质量区间，不把分阶段百分比相乘。结果证明这两项在这些开发帧上的有限压缩收益，也证明与 AOM 仍有显著差距。没有把“对自己省约 15%”写成“比 AOM 省 15%”。三张开发帧不是 holdout，也不能外推到整段视频或其它题材。

`dev-curves-final` 保存 AOM/旧 RD/基础七模式的 54 点，`dev-curves-i3` 保存 CDF/RD 和额外 Tritium 对照的 36 点；每点有命令、stdout/stderr、raw OBU、内部/AVM 重建 SHA、VMAF JSON、三平面 SSE/PSNR 和进程 CPU/墙时。`development-summary.json` 可从这些点复算。质量运行含重建导出且部分与构建/检查并发，**不用于 2× 速度验收**；五次新进程同质量独立计时尚未执行。

首批辅助二进制的完整日志确认它是 **SVT-AV1-Tritium v4.2.0**，不能把它冒称官方 SVT-AV1。该额外对照保留但不代替合同辅助基准。已从官方上游纯 HEAD `24f54f73303fbe6fbc1dcb7a21755614e0050a18` 隔离提取并构建 **SVT-AV1 v4.2.0**，不带工作区已有补丁；固定 preset4/tune3/avif1/pred-struct0/lp1、单轮六点曲线共 18 点全部通过。最终组合相对官方辅助基准的 VMAF BD-rate 为 2D **+68.37%**、3D **+42.94%**、Real **+43.20%**，仍明显落后。完整身份和构建参数见 `svt-official-manifest.json`，原始结果在 `svt-official-curves`；`development-summary.json` 的 SVT-AV1 行现在只引用官方版本。辅助 lp1 不用作主速度门槛，不影响上表 AOM 主对照。

## 正确性与兼容性

- Windows MSVC `/W4 /WX`：52/52 CTest。GCC strict：51/51；Clang strict：51/51。Linux 少一项 Windows oracle smoke，另以完整流/像素交叉验证补证。
- GCC ASan/UBSan/LSan 全量 51/51；最终与本轮相关的五项重新运行通过。包含候选反序、失败后恢复、赢家重放、同语法整帧/逐 SB rate 一致和完整/partial SB。
- 直接编译只读 fixed AVM v1.0.0 的 `intrapred.c` 做原语差分：19 种方形/矩形、8/10 bit、16 组边界/端点条件、20 组固定随机参考，**48,640 组、34,775,040 个样本零差异**。测试程序为 `tests/conformance/i1_avm_prediction.c`。
- **882 条**强制 Y/UV 码流：七乘七组合、三格式、三几何、固定/混合邻居模式，fixed AVM 逐可见样本一致；`forced-modes-final/result.json`。混合模式扩大测试修复了“左侧非 DC 非方向、上方方向模式”的排序缺陷。
- **90 组** API/CLI 矩阵：72 组 single-picture（含 QP1/100/255、三格式、8/24/128/partial 尺寸、静态/自适应 CDF）全部 AVM 一致，18 组旧多帧关闭路径与修改前冻结程序的流/重建相同；`api-matrix/result.json`。
- 最终 MSVC/GCC/Clang 各重放 72 组，共 **216 条完整流及重建指纹一致**；`cross-platform-final/result.json`。API 还检查 17 次小输出缓冲重试，以及 1000 个独立单图 handle 的确定性，覆盖 1/2/3/17/1000 检查点。

验证中发现的失败保留在 `forced-modes`、`forced-modes-v2`、`dev-curves` 等目录，不并入成功结果。它们分别暴露了 AV2 highbd smooth、UV 隐式变换和混合邻居排序问题。当前最终矩阵已全部通过，不能删除旧失败点或把中途的 RUNNING 当完成。

## 数据集与尚未通过的门槛

`images-i0` 冻结 24 张 Kodak 与 26 张带 commit 的 Urban100 镜像图片，共 50 个独立来源图，按身份 hash 事先划分 **30 calibration / 20 holdout**。保存原 PNG、原/转换后 SHA、尺寸、BT.709 limited-range 420 转换命令、Y4M header 和像素 hash；所有编码器将读同一份转换像素。holdout 没有编码评分，不用于模式、lambda 或搜索参数调整。

这些图片主要覆盖摄影与建筑细线，尺寸并非全部 1080p；还缺足够的屏幕/文字、高 ISO、原生 HDR 等题材。不能称最终 I0 数据集完整，也不能用它们替代原生 1080p 的速度测试。筛查脚本拒绝直接消费 holdout manifest。

下一项应先扩大可合法选择的分区/TX（当前完整 SB 主要还是 64/32 的有限树）并与模式和系数 RD 联动，逐种强制码流验收，再测真实收益。随后继续系数优化/帧内专用工具/滤波的单因素实验。体积明显达标之前不把主要工作转去提速；最终仍需全部 holdout、分类稳健性、原始曲线与五次独立计时通过后才能宣布 I-GATE PASS。

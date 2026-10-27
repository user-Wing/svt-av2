# M5 / M5.3 发布收尾

日期：2026-10-17。**M5.1/M5.2/M5.3及M5本地正式冻结DONE，版本`0.0.5-m5`。** 最终`artifacts/m53-release/final-verification.json`为PASS：严格CTest为Windows40/40、GCC/Clang/SAN各39/39；四平台各360项、共1440次发布构建编码，完整流与重建SHA全部保持。未做远程发布。

正式140源清单SHA256：`f303fe31348c1b72bbc4d37417127e43e9fb35bc4b2a4884860a0ca2493654ca`。Windows app SHA256：`1212821842131bc481a16d04f84680030a9e813a7e6d14c10d26b127665d8e84`。所有程序及来源证据见最终机器可读摘要。

## 本版范围

M5.1的Y DC/V/H预测选择、M5.2的64到32内容分区、M5.3的非无损CQP1..255及多点PSNR/VMAF/BD-rate与无人值守批测构成本版有界交付。支持8bit420、10bit420/444，AI及单参考LOW_DELAY_P，单线程，尺寸8的倍数且至1080p。新工具默认关闭；QP默认100；10bit实际qindex为userQP+48；QP0不支持。

未交付的完整RD框架、实际preset、编码器内部并行和CRF分别留在M6/M7/M8/M9。未来只做CQP和CRF，不做CBR、2-pass、VBR/VBV独立模式。剩余AV2工具族仍是后续任务，M5完成不代表工具齐全或压缩效率达到主流编码器。

## 候选与正式构建的证据边界

候选长测位于`artifacts/m53-root/long-campaign-r1`，四平台各240/240通过，共144万帧编码、24万帧固定解码与质量测量。`long-campaign-verification.json`记录最终独立审计。85项经hash核验带溯源复用，其余155项新执行。源清单SHA为`b54581ec223e80b58c9021dfb1c83d200e2550b0b34a67c506cdd7c32ba639d8`。

发布仅修改四个源文件的版本/里程碑标识与对应断言：CMakeLists.txt、include/svt_av2.h、Source/Lib/Common/common.c、tests/unit/test_api.c。`artifacts/m53-release/verify-release.py`将这些标识反替换后逐文件核对旧候选140项SHA，禁止夹带编码算法变更；然后冻结正式源清单。

新Windows/GCC/Clang/SAN构建使用独立目录，不覆盖候选程序。Windows严格CTest40项、Linux三构建各39项；新程序各重放360个三格式AI/P、四工具、15个边界QP用例，与已固定AVM解码的候选完整码流及内部重建SHA逐项比较。正式构建本轮重放为1440次短流编码，不冒充在新程序上重新完成144万帧长测。最终源与程序SHA、CTest和重放结论在`artifacts/m53-release/final-verification.json`。

版本升级后当前源与旧候选清单有上述明确差异，旧`collect_m53.py --campaign ...`的“当前源完全相同”检查不再适用。不得覆盖旧候选清单以假装新程序跑过历史长测。正式发布重验用`python artifacts/m53-release/verify-release.py`；旧证据保留原状态、原指纹。

## 使用

Windows发布目录：`dist/M5`。直接CLI示例（输入须满足上述范围）：

```powershell
.\SvtAv2EncApp.exe --version
.\SvtAv2EncApp.exe --input input.y4m --output output.av2 --qp 100 --pred-structure low-delay-p --intra-select adaptive --partition-select adaptive
```

该目录提供CLI、共享DLL、公开头文件和许可证；没有将M4-ffmpeg-min重新链接到M5，也没有新增FFmpeg高级参数容器。固定AVM解码器仍可用于独立解码检验。

## 画质结果与下一阶段

已整理[M5.3体积、VMAF和时间实测点](M5.3-compression-points.md)。128×128合成cut/noise、并行计时用于正确性与压力验证，不能替代真实视频压缩效果实验。当前VMAF偏低可能受素材与工具/决策限制共同影响，但尚无消融实验确认各因素贡献；不得预先把M6收益当成已证明事实。

下一轮使用[ M5→M6 开发任务包](../../../M5-M6-AI-TASKS/README.md)，本轮只准备指南，不开始M6实现。

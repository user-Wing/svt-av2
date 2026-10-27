# M4 最小 FFmpeg 接入：DONE

2026-10-11，用户要求编译最小 SVT-AV2 FFmpeg、证明可以编码解码，并附教程。交付目录：`C:\Private\ffmpeg-native\deps-full\AV2\svt-av2\dist\M4-ffmpeg-min`。

完成 Windows x64 静态 ffmpeg.exe（4,843,008 字节）和 ffprobe.exe（4,675,584 字节）。SVT-AV2 M4通过新增libsvtav2.c同步封装真实编码；固定AVM v1.0.0 commit 966a7d7cd6fcf60360caf5dc413b2aeeb65e144d仅编入解码器。已有FFmpeg AV2基线在独立Linux构建副本接入，修正AVM pkg-config名称、当前FFmpeg decode回调签名和8-bit样本转换。8-bit样本转换同时覆盖亮度及色度；验收逐字节检查三平面。

Y4M/RAW输入、AV02 IVF封装、AVM解码、RAW/Y4M输出实际通过。三格式（8-bit420、10-bit420、10-bit444）×两预测结构（AI/P）各17帧，共102帧，IVF payload拼接与独立MSVC M4 CLI码流完全相同，FFmpeg解码与内部重建和固定参考解码器逐字节一致。独立M4语法分析证明预测结构及引用关系；IVF帧数、PTS0–16和ffprobe计数一致。三格式额外覆盖RAW输入、30000/1001、单帧EOF及17帧Y4M输出；QP99/threads2/非零码率/B帧/非8倍数尺寸/GOP10六项正确拒绝。

包内README.md提供手动命令、三格式选项、MP4预转换、源码复现与限制。demo.ps1已直接在Windows运行PASS，使用随包输入并对照独立内部重建SHA256。proof/acceptance.json包含完整命令、返回码和SHA；PROOF.md为报告，SHA256SUMS.txt为包清单，build-info为配置/PE导入/来源，source-patches为封装/补丁/脚本，licenses如实保留各组件现有许可状态。

仅导入Windows系统DLL/UCRT，未依赖SVT/AVM/LLVM/MinGW DLL。M4冻结116源文件SHA、原FFmpeg基线指纹均复核一致。当前仍为QP100、单线程、8倍数尺寸至1920×1080，无GPU编码；未操作3fui。

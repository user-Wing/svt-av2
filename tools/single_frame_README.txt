单帧分类与编码包装工具

仅支持1920x1080或3840x2160、progressive、C420p10、恰好一帧的Y4M。需标准LF换行、无FRAME标签、无额外尾数据；样本必须在0..1023。不会缩放、转换或重写送入编码器的Y4M。

目录布局：single_frame.py旁放public-classifier/，其内容保持原公开分类包不变；也可用--classifier-dir指定。自动分类需安装该包requirements.txt，手动内容类型仅需Python 3.11标准库。

自动分类示例：
  python single_frame.py --input frame.y4m --output frame.obu --encoder /path/to/SvtAv2EncApp.exe --qp 100 --ffmpeg /path/to/ffmpeg.exe --checkpoint /path/to/ViT-B-32.pt
手动内容类型与profile覆盖示例：
  python single_frame.py --input frame.y4m --output frame.obu --encoder /path/to/SvtAv2EncApp.exe --qp 100 --content-type real --profile animation

--content-type为auto、animation或real；auto生成640宽JPEG并复用冻结CLIP CPU分类。预览使用原始10bit420 payload、FFmpeg默认rawvideo色彩解释、scale=640:-2、JPEG q=3及单线程；仅分类预览缩放。不同FFmpeg版本可能改变预览，JSON保留程序SHA与预览SHA。

--profile为auto、animation或photo；auto对1920x1080的animation使用avm-ai（lambda100），对real或3840x2160使用avm-ai150（lambda150）；UHD动画的内容标签仍保持animation。手动profile覆盖lambda选择，不改变传给native --content-type的标签。需支持这两个lambda选项和内容API的同一新版编码器；本工具不探测或降级到旧编码器。

输出文件必须不存在，其父目录已存在。另生成<output>.json与<output>.log，记录实际CLI、profile、分类证据、输入/编码器/输出SHA及退出码；这些运行记录包含本机路径，不能直接当公开报告上传。编码子进程清除SVT_AV2_TEST_*及SVT_AV2_M6_DIP。输出成功仅表示进程退出0且产出非空文件；不表示码流已独立解码、VMAF/NEG或0.88体积目标通过。需要另行使用固定解码器和评分协议验证。

默认编码参数沿用既有mixed8/fast/CfL/RDOQ/角度/MRL/wide树精进与单线程路径，不承诺速度收益。不读取终测数据，不训练或修改分类阈值，不下载模型。

可选--recon /path/to/recon.yuv转传native --export-recon，记录重建路径、实际SHA256及字节数；请求后未生成文件视为失败。重建文件须为独立的新路径。不指定时不导出重建。此选项供外层将内部重建与固定AVM解码逐字节核对；wrapper本身不替代解码验收。

R3依据内容与分辨率选择profile；JSON记录wrapper自身SHA与profile策略，结束时同时复查输入、编码器与wrapper身份。分类runtime、阈值和原始编码输入保持冻结。


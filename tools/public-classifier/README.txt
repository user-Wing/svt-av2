冻结单图内容分类 runtime

这是 zero-shot OpenAI CLIP ViT-B/32 CPU 推理导出。仅保留冻结的两类float32文本向量、0阈值、预处理及推理定义。无需训练或重新生成文本向量。输出anime映射animation，real映射real；anime_score是未校准余弦差，不是概率。

安装（Python 3.11；依赖版本来自原等价验证环境，其他版本未验证）：
  python -m pip install -r requirements.txt
运行（权重必须由用户提供已有本地文件）：
  python predict.py --checkpoint /path/to/ViT-B-32.pt --image /path/to/frame.jpg
  python predict.py --checkpoint /path/to/ViT-B-32.pt --image /path/to/frame.jpg --format content-type

程序不下载权重。官方权重URL及完整SHA256、字节数见runtime.json的model；预计353976522字节。加载前验证checkpoint和配置SHA，官方CLIP包文件身份及预处理也有检查。建议每进程建立一个Runtime后复用infer_bytes；CPU线程数2，interop线程数1。

原始帧应由调用者生成分类预览。原测试使用640宽JPEG预览；predict.py从所提供图像执行官方短边224 resize/center crop/RGB/normalize，不擅自复制视频帧提取或改变编码输入。原始编码帧与QP等仍由外层调用者管理。

95.83%（23/24）属于原冻结独立终测，不是此导出包新测的泛化准确率。此次仅使用两张不同类别开发预览做旧/新runtime等价验证，没有读取24张最终测试图像，没有调阈值或训练。本包不含测试图、作品名、私人路径、样本rows、完整私有manifest、推理日志或模型权重。

所有runtime字段均从原选定配置导出，original_selected_model_sha256记录原配置身份。CPU硬件、依赖版本或图像预处理的变化可能改变浮点结果；仅声明已验证环境中的等价性。wrapper和编码器集成由上层单独处理。

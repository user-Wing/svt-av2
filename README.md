# SVT-AV2 单帧研发版本

这是独立 AV2 编码器的 M6 帧内研发版本。当前优化范围为 **1920×1080、3840×2160 的 10-bit 4:2:0 单帧**。单帧搜索配置需要显式启用；版本号仍沿用原项目，不代表完成了视频、速度或正式发布验收。

公共配置结构已扩展；库调用方须使用本版头文件重新编译，避免混用旧头文件和新库。

当前目标是同质量体积达到 AOM AV1 的 0.90×，进一步向 0.88×、0.85×推进。尚未实现所有内容和质量区间达标。实拍 1080p 的既有开发帧已接近 0.88×；不能把它推广为动画、UHD 或其他素材的结果。

## 构建

需要 CMake、Ninja 和 C11 编译器。Windows 在 MSVC 开发环境运行，Linux 使用 GCC/Clang。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_STILL_PROFILE=ON -DSVT_AV2_WARNINGS_AS_ERRORS=ON
cmake --build build
```

程序位于 `build/bin/SvtAv2EncApp`，Windows 带 `.exe` 后缀。参考 AVM 解码器独立提供；有解码器时可以通过 `SVT_AV2_ORACLE_DIR` 配置相应验证。

`SVT_AV2_STILL_PROFILE` 默认关闭；启用后编译既有变换缓存、CfL、细分块、RDOQ、PC-Wiener 与内容保护配置。以下两个实验选项仍默认关闭，均要求先启用单帧配置：

- `SVT_AV2_EXPERIMENTAL_SATD_ANGLES=ON`：增加角度候选与完整帧候选包比较；保留了此前 3D 1080p 开发帧的最佳已测方案，会增加搜索成本。
- `SVT_AV2_EXPERIMENTAL_FINE16_PAIR=ON`：扩大部分细分块计划搜索。完整 2D 1080p 已出现 VMAF 回退，不应因同 QP 文件更小而默认采用。

两个实验选项的组合没有完整质量曲线验收。详细构建条件见 [STILL_PROFILE_BUILD.txt](publication/STILL_PROFILE_BUILD.txt)。

## 单帧编码与内容分流

标准库入口接受恰好一帧、逐行、`C420p10` 的 Y4M，原编码输入不缩放：

```sh
python tools/single_frame.py --input frame.y4m --output frame.obu --recon recon.yuv --encoder build/bin/SvtAv2EncApp --qp 100 --content-type animation
```

实拍可指定 `--content-type real`。自动 profile 对 1080p 二维动画使用 λ1.0，对 1080p 实拍与 UHD 使用 λ1.5；动画标签仍保留给内容保护。`--profile animation|photo` 允许显式覆盖 λ 配置。3D 动画不能仅凭二分类标签推断最优压缩配置，应使用对应已验证的 `photo` profile。

自动分类使用冻结的 CLIP ViT-B/32 导出，需另行安装 [分类依赖](tools/public-classifier/requirements.txt)，并提供本地官方 checkpoint 与 FFmpeg：

```sh
python tools/single_frame.py --input frame.y4m --output frame.obu --encoder build/bin/SvtAv2EncApp --qp 100 --ffmpeg /path/to/ffmpeg --checkpoint /path/to/ViT-B-32.pt
```

只有分类预览缩放到 640 宽，编码使用完整原始帧。既有冻结验证集为 20 个作品、60 帧，开发 36 帧、按作品隔离的独立终测 24 帧；终测为 **23/24（95.83%）**。公开导出只用两张开发预览做等价检查，没有重开终测或重新调阈值。这是该冻结集上的观察准确率，不是所有视频的保证。模型权重与私人素材不包含在仓库中。

## 质量控制与结果解读

内容类型参与编码 profile 选择；原生动画保护约束 PC-Wiener 候选的边缘梯度与平坦亮度回退。外部最终质量控制对动画检查边缘、平坦色块和色度，对实拍检查纹理与亮度。原生保护不覆盖全部最终滤波与外部指标，最终仍必须对固定参考解码后的帧验收，详见 [QUALITY_CONTROL.txt](tools/QUALITY_CONTROL.txt)。

主 VMAF V1 与 NEG 分别报告；默认体积比较同时满足相同 AOM V1/NEG 目标。93–97 完整区间要求双方各至少五个区间内实测点与双侧包围，分别用 PCHIP、分段线性复核，不外推。单个 QP、裁块收益或编码成功均不表示通过这些条件。

逐组历史最佳与新候选结果见 [SINGLE_FRAME_RESULTS.txt](publication/SINGLE_FRAME_RESULTS.txt)，逐点数值见 [numerical-evidence.json](publication/numerical-evidence.json)。历史完整曲线与本次发布构建的单点回归分开记录，尚未达到目标的内容组保持未达标。

包装工具输出 JSON、日志及 SHA256；`ENCODE_COMPLETED_NOT_QUALITY_VERIFIED` 只代表编码完成。当前没有完成 >16× 提速验收。分类和本地误差筛查也不能替代固定解码与质量比较。

原项目历史保留在 Git 历史和既有 `docs/` 中。原 [LICENSE](LICENSE) 的授权状态保持不变；分类器上游组件的授权单独记录在其目录中。

# SVT-AV2

测试性质的 AV2 编码器：架构参考 AOM-AV1 和 SVT-AV1，以 AV2 技术规范为准。项目目标是在相同 VMAF V1 下，相比 SVT-AV1 preset 6 自动关键帧编码，耗时不超过 3 倍、体积为 75–80%；目前尚未达成。

当前研发进展为 M6-Intra-r13，帧间分支尚未接入当前帧内基线。已研究单帧预测、分区、变换、量化、熵编码和滤波，以及真人/动画识别与调优；不同功能的集成状态见项目说明。已有研究结果不能代表所有内容或正式速度验收。

帧内技术优先参考 AV2 规范文档；帧间先补基本工具并优先参考 SVT-AV1。固定 AVM 解码器可用于验证合法码流。

## 构建和单帧编码

需要 CMake、Ninja 和 C11 编译器；Windows 使用 MSVC 开发环境。单帧研发配置需显式启用：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_STILL_PROFILE=ON -DSVT_AV2_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

单帧输入为 10-bit 4:2:0 Y4M。可显式指定 `--content-type animation` 或 `real`；自动识别需额外安装 [分类依赖](tools/public-classifier/requirements.txt)，并提供本地 CLIP ViT-B/32 权重及 FFmpeg。示例：

```sh
python tools/single_frame.py --input frame.y4m --output frame.obu --encoder build/bin/SvtAv2EncApp --qp 100 --content-type animation
```

Windows 下程序名为 `SvtAv2EncApp.exe`。`SVT_AV2_EXPERIMENTAL_SATD_ANGLES` 和 `SVT_AV2_EXPERIMENTAL_FINE16_PAIR` 默认关闭，实验配置不能视为完整质量验收。

旧批测脚本可能引用已清理的历史合同和素材；重跑前需按当前环境调整路径和输入。仓库中的合成测试素材用于自动测试，私人测试素材和构建产物不纳入版本控制。

详见 [项目说明](project.md)、[M0–M5](docs/M0-M5.md)、[M6 帧内](docs/M6-Intra.md)、[M6 帧间](docs/M6-Inter.md) 和 [测试规则](tests/Test-Rule.md)。历史构建与验收记录已归并至三份阶段文档。

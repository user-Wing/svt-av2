# M6 第二轮 A：消除 RD 无效计算

2026-10-17，状态 **READY_FOR_INTEGRATION**。仅本轮局部性能优化交付；M6完整质量、速度和发布仍须00最终审计，正式版本仍0.0.5-m5。

## 范围与诊断

生产仅修改`Source/Lib/Encoder/rd_frame.c`。候选集合、顺序、ID、lambda、合法候选的预测/变换/量化/重建和评分保持。未改剪枝策略、ME搜索域、并行方式或off路径。长测、快照、媒体、模型和依赖均保持只读；计算前检查总控，所有本次计算开始时均为real-quality，未触碰isolated-speed。

外置baseline来自00已逐项匹配170源的`optimization-r2-20261017/root/baseline-source`。在单独profile-source加入QueryPerformanceCounter函数包裹，仅运行现有合成`test_m6_rd`，得到8813 checks、0 failures：

| 区域 | 累计秒 |
| --- | ---: |
| build_path | 2.739956 |
| reconstruct_leaf，包含predict | 2.702423 |
| predict，包含SAD | 0.220785 |
| rank_modes，包含predict | 0.035856 |
| region_copy | 0.127153 |
| geometry_for_sb，包含ME | 0.016927 |
| path_hash | 0.009218 |

这些值互相包含，不能相加计算总百分比，探针本身也有开销；仅是该合成机制测试的画像，不能代表真实1080p总体热点。原始探针脚本、外置探针源及profile-test.log均保留。

B的已有1512条小图结果进一步给出实际重复计算线索：tools=10的AI每帧有效候选同为20，但exhaustive平均2216.7次forward、pruned536.7次。源码定位确认exhaustive原先逐叶重建后才发现后面叶的模式不合法；本轮未把它猜成剪枝本身的开销。

## 最小改动

1. exhaustive且非partial、非赢家重放时，先按同一ordinal检查全部叶的模式合法性。任何叶非法便直接跳过，不再对非法路径的前缀做预测/变换。legal_mode只依赖帧/叶位置/开关，不依赖候选重建，因此合法候选完全不变。
2. reconstruct_leaf只需要预测，不使用predict产出的SAD；允许内部predict的SAD指针为NULL，跳过三平面SAD扫描。剪枝与partial的排序仍真实计算同一SAD。
3. SB备份和恢复按可见行memcpy，保持位深对应的原始样本字节。原uint16备份分配足以容纳8/10bit；只复制可见宽度，不覆盖帧padding。

`forward_calls`仍表示实际forward调用次数，因删除非法路径工作而减少；不伪造旧计数。其余candidate/replay/winner/D/R/J/lambda/full/partial必须保持。

## 验证与性能观察

- Windows MSVC `/W4 /WX`和GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror`均全新外置构建，现有`test_m6_rd`各8813 checks、0 failures，包含换序、候选最优、源/参考只读、容量失败和第5次forward故障重试。
- B独立54条差分初验PASS：三格式AI/P、三RD、QP1/100/255、四工具、64x64/128x64/72x72；完整流、recon与决策统计一致，54条固定AVM解码重建一致，18条off与冻结M5相同。详情`b/regression-initial/result.json`；最终候选复验以B和00报告为准。
- A-only的前后源副本除rd_frame.c外全部相同。现有合成测试三轮before/after交替运行，前为3.677394/3.613297/3.813351秒，后为3.022508/2.918449/2.986642秒，中位3.677394→2.986642秒，约18.8%时间减少。六次各8813/0。后台real-quality同时运行，**仅作为合成测试方向性观察，不是隔离真实视频单路速度证明**。未启动新的长测矩阵。

全部外置证据目录：`C:/Private/ffmpeg-native-m6-tests/optimization-r2-20261017/a/`。入口`delivery-hashes.json`、`rd_frame.diff`、`paired-timing.json`、`Windows-test.log`、`GCC-test.log`、`profile-test.log`；before-build与after-build为独立A-only计时程序。未写公共CMake、README/project/changelog；由00集成统一回写。

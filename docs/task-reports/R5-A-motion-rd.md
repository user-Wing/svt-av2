# 基础能力 R5 A：整数运动候选的真实 RD 比较

2026-10-18，状态 **READY_FOR_INTEGRATION**。遵循`docs/basic-capability-contract.md`的R5单因素冻结算法。本次补齐运动候选选择，不代表整个基础能力阶段完成；正式仍0.0.5-m5。默认motion优化关闭，压缩效果和单路性能由B分开实测后交00独立判断。

## 实现

仅改A属主`motion_estimation.c/h`、`rd_frame.c/h`，新增`tests/unit/test_m6_motion.c`。公共config/API/Codec/CLI、CMake和状态校验由00接线。

新增`svt_av2_motion_estimate_list`，在原±8范围的289个整数MV中按(SAD,|x|+|y|,y,x)精确保留前三名；零MV不在前三时附加为slot3，最多四个、无重复。slot0的MV、SAD和实际搜索位置数须与旧入口完全相同。新helper用当前第三名SAD作为提前终止阈值：只有已累计SAD严格超过第三名时停止该位置扫描，不把用第一名截断的部分SAD误作精确第二/三名。旧`svt_av2_motion_estimate`函数体未改。

full P的正常RD仍用slot0，先运行全部原树/模式候选，保留同入口旧赢家。最终选择赢家树后，单raster叶坐标精化：每叶试至多四个候选MV并把该叶设inter，允许原intra叶转inter；其他叶保持当前已接受的模式/MV。完全相同的当前解按该叶入口状态跳过，反向枚举时跳过集合相同。

每次从SB入口备份重建整path，后继叶预测/变换/量化全部随prefix重算，D来自可见重建，R来自真实共享语法事务，原lambda与整数J不变。按更小J、等J更小稳定ID接受。ID低9bit保留树/模式，MV slot每叶2bit位于bit21..28，原coeff bit9..20保留但本轮两开关互斥。最终保存模式IDs与MV slot向量，按该向量重放并核hash/D/R后只提交一次；实际MV和非零motion policy参与hash。policy0不追加零值，flag0原hash保持。

每完整SB最多四叶×四MV=16额外完整路径；不声称扩展运动域穷举。新helper没有扩大搜索位置，但较宽top3阈值会多扫描一些SAD行，额外RD还会增加正逆变换与rate计算。AI/partial无额外MV试探；原source与committed reference只读，内部也拒绝motion与coeff同时开启。

## 严格机制验收

独立全新外置构建，j2：

| 项目 | MSVC /W4 /WX | GCC严格Werror |
| --- | ---: | ---: |
| 新test_m6_motion | 8658/0 | 8658/0 |
| 原test_m6_rd | 8813/0 | 8813/0 |
| 已有V2 test_m6_coeff | 231084/0 | 231084/0 |

GCC告警为`-Wall -Wextra -Wpedantic -Wshadow -Werror`。没有改旧测试期望来迎合新行为。

新增验证包括：

- 三格式、平坦同分/独立纹理/受控位移、左上/内部/右下块，独立逐像素完整计算全部289个MV的SAD，独立排序，逐项核新helper前三名、精确SAD、slot0旧结果、289搜索计数及补零去重。oracle没有使用生产早停或preferred函数。
- 三格式AI/P、64/128完整SB、136x72/120x120混合边界及56x56全partial。
- 受控Y平坦、UV按(-1,0)运动的fixture使亮度SAD旧赢家为零MV、候选中另有改善三平面D/R的MV；真实生产变换下`motion_accepts>0`。这是机制覆盖，不是实际片收益声明。
- hook检查每条试探的独立整数J与最终逐SB最小J/最小ID；单64SB直接对相同入状态的flag0成本不劣。提交ID逐叶解码mode和MV slot，并与输出真实MV及候选列表核对。
- trial≤fullSB×16、候选增量等于trial、accept≤trial；AI与全partial零trial，winner hash与flag0相同。
- 从最终重建独立逐像素求D、正式整帧writer重放R；原模式及新增MV反向枚举后coeff/recon/hash/J/R保持一致。
- 源与committed reference含padding指纹保持；容量失败后可重试；单64 P故障注入定位到首个MV精化trial的forward，随后干净重试恢复；内部互斥拒绝且输出计数清零。

逐SB不劣只约束相同入口，不保证整个序列的质量或码率；本轮未增加运动范围、subpel、参考帧、lambda自由度或内部线程。

## 证据与剩余验收

目录`C:/Private/ffmpeg-native-m6-tests/optimization-r5-20261018/a/`，入口`delivery-hashes.json`、相对00冻结`root/baseline-source`的四个生产文件diff，以及Windows/GCC构建与三个机制测试日志。R4源码、程序、证据只读保留。

A没有自行运行真实效果矩阵。B按事前冻结三类1080p、帧48..71、四QP进行真实字节/PSNR/VMAF/AVM验证，再单独jobs1暖机与三轮交替配对测速度。速度窗口中A停止所有新增计算；原长测仍继续，因此不能称全机空闲隔离。所有本次计算开始前读原campaign均为real-quality，未触碰冻结素材、模型、依赖、配置或原调度任务。

最终默认off兼容、严格集成/检测器和效果判定由00独立执行。局部单测PASS不代表压缩价值或整个基础阶段DONE。

# M5.2 最小内容分区：语法与独立解码证据

日期2026-10-16。状态 READY_FOR_INTEGRATION；B属主生产、测试与工具已冻结，M5整体尚不发布。本轮仅完整可见SB64可按内容选NONE或四32方叶；不增加rect transform、动态深度、SDP、扩展分区、RD或其他参数。主窗口负责公共API/Codec/keyframe、最终矩阵及阶段状态。

## 1. 唯一计划接口

Codec从source-only selector每帧生成一次uint8_t数组，长度至少sb_cols×sb_rows，raster SB索引；0保持旧树，1仅允许完整可见64×64根。partial SB必须0。core与writer共用Common `svt_av2_partition_coded_type_plan`；writer绝不由qcoeff、模式或图像重新推断决策。

B新增内部FrameHeaderConfig尾字段 `const uint8_t* partition_decisions`、`size_t partition_decision_count`，default为NULL/0；同步写端借用到packet序列化返回。default同时明确m5_intra_modes=0（原default此前未初始化该既有尾字段），无新增帧头bit。NULL/count0严格旧树。writer入口先 `svt_av2_partition_plan_validate` 全数组校验，拒绝短数组、NULL非零count、值>1、partial1，然后逐节点使用共享helper。源数据与计划生命周期由00维护，失败不提交参考/packet。

A selector合同：亮度四象限各1024样本和S_i，精确整数score=4ΣS_i²−(ΣS_i)²，严格大于4096²×16×2^(2×(bd−8))才选1，平局0；这是有界源均值SSE改善加固定惩罚，不是完整RD。独立Fraction参考计划由00生成到各case的expected-plans.json，B把真实decoder分区逐frame/SB直接与此表绑定，不能以流hash变化代替选择正确性。

## 2. 固定规范与源码条件

规范 `../docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf` SHA256 `e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f`。页码与PDF页序/页脚一致；既有只读全文提取在artifacts/m5-b/spec-all.txt。固定AVM commit `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`，不是可变junction HEAD。

|规则|规范位置|固定AVM路径/行|本期处理|
|---|---|---|---|
|递归节点与次序|§5.20.3.1 p148–152|av2/decoder/decodeframe.c:1900–2280 decode_partition|64 HORZ先上后下；每64×32 VERT先左后右；叶TL/TR/BL/BR|
|do_split/rect_type顺序|§5.20.3.2 p155–157|decodeframe.c:1728–1847 read_partition|64根do_split1、rect_type HORZ0；两个64×32 do_split1、rect_type VERT1；四32叶do_split0|
|无square-split bit|§5.20.3.2 p158–159允许分区|av2/common/blockd.h:1000 is_square_split_eligible|仅128/256允许PARTITION_SPLIT，64根不读do_square_split；不能误发AV1四叉符号|
|矩形方向未隐含|§5.20.3.2 p160 rect_type_implied_by_bsize|blockd.h:966–996|64和64×32均RECT_INVALID，真实写orientation；不是矩形长边方向自动隐含|
|扩展工具关闭|§5.4.3 p59、§5.20.3.2 p157–160|av2/common/av2_common_int.h:4750起 init_allowed_partitions_for_signaling|enable_sdp/ext_partitions均0，无SDP、do_ext/uneven4way符号，shared tree不变|
|partial强制规则|§5.20.3.2 p160–162 partition_implied_at_boundary|av2_common_int.h规范forced helper；Common既有coded_type|partial计划必须0，保持M51边界递归，不能当内容split计数|
|CDF上下文|§8.3.2 p592|av2_common_int.h:4037–4141 partition_plane_context/helper|raw=2×(left高度更小)+above宽度更小，使用已经完成叶的partition状态|
|叶上下文提交|§5.20.3.1 p154上下文更新|av2_common_int.h:3998–4018 update_partition_context/update_ext_partition_context|只在NONE提交；32叶mask56，64叶mask48，矩形内部节点不提交|
|色度映射/偏移|§5.20.3.1 p151–152、§5.20.3.2 p157–159|decodeframe.c递归及set_chroma_ref_info；Common几何映射|shared树32Y→420 UV16或444 UV32，不触小4尺寸特殊chromaOffset；每叶仍Y/U/V|
|TX与模式|既有M51条件见m5-mode-syntax-research.md|固定decodetxb/entropymode/reconintra路径|叶均square；32Y栅格1024qcoeff，420UV16为256、444UV32为1024；沿用DCT与真实模式/邻居契约|

CDF map：64根split size_map3→ctx12+raw，rect_map6→24+raw；64×32内部split仍12+raw，rect_map5→20+raw；32叶split size_map2→8+raw（NONE不读rect）。生产复用既有冻结split/rect CDF，不新增表。四32叶的相邻叶并不总改变raw：leaf32比较位是mask的bit2，而56该位0；实际decoder trace已逐节点核验。

## 3. 实现与单测

B只改frame_header.h/c（尾字段/default）、tile_writer.c（保存计划、验证、共享helper）、既有test_tile_writer.c及新独立工具。既有coeff/motion/mode选择不改。新增单测覆盖3格式×DC/adaptive×AI/P四32块写入、关闭计划后拒绝不匹配coeff布局、值2/NULL非零count拒绝；Common selector/容量/partial数学单测由A负责。

本轮B实际验证：MSVC Release /WX clean构建完整CTest37/37；GCC Release Werror全新配置完整CTest36/36。日志artifacts/m52-b/msvc-final-build.log、msvc-final-ctest.log、gcc-configure.log、gcc-build.log、gcc-ctest.log。00后续新增工具self-test注册后测试数可能增长，不能把增长后的数字冒写成B已执行。

## 4. 独立实际解码树检查

`tools/m52_partition_trace.py` 消费独立固定tag诊断decoder的M52PART日志，不消费encoder自报树。逐frame维护预期DFS队列与独立above/left已完成叶地图，验证每个actual node的位置/尺寸/part、shared-tree、split/rect CDF context、合法32叶顺序与总像素coverage；partial由独立规范边界模型检查。开启时实际完整SB根直接匹配00 Fraction expected-plans.json，关闭时完整根强制NONE；partial计划强制0。self-test有固定七节点/精确CDF context锚点及三种破坏输入（全关、损坏ctx、不完整树）检测。

实际证据：

- 128×128三格式×AI/P×flat/quads/mixed×四开关组合=72流、216帧，2160分区节点，全部decoder/raw/plan/context/leaf/mode/UV PASS；artifacts/m52-b/trace-matrix.json。
- 176×144三格式×AI/P×quads/mixed3帧及mixed17帧=18流、138帧，10014节点，partial原边界语法全部PASS；其中六个mixed17完整流跨frame3/6/9/12改变计划，同帧NONE/split同时存在，actual根逐项匹配Fraction计划。不是缩短样例替代长序列；partial-trace-matrix.json。
- 12组同尺寸内容对照（3格式×AI/P×DC/adaptive）：flat开启分区后所有完整根NONE，quads所有完整根HORZ，逐帧真实树不同，排除总是split或只改hash实现。
- 所有90流的诊断raw SHA等于编码器预先生成内部raw；继承独立m5_mode_trace同时核Y/UV mode与邻居。00对相同流另用不可变Windows oracle验收。原始日志/raw在traces和partial-traces子目录，final-evidence.json汇总同尺寸对照与六条17f的真实根模式序列。

## 5. 诊断隔离与冻结

本轮新私有目录 `artifacts/m52-b/diagnostic-decoder/source` 从M51保存副本只读复制；只在该新目录的decodeframe.c加M52PART（M51模式trace保留），新build也在该目录，未写M4/M51诊断目录。补丁partition-trace.patch与完整source-manifest.json一并保存。新decodeframe SHA `721bec64021cce6f3f1c892cdc9a3ded892684addc3154c972aee4fea1a0327f`，新可运行WSL decoder SHA `4e780b4692282178aab202bf51017d897d7885cc43a7321b17b852a8446aa8fa`。完整source-manifest SHA及私有运行路径见final-evidence.json。

重现：Windows项目根运行Python311的artifacts/m52-b/trace_matrix.py与trace_partial.py；脚本明确调用新私有decoder，独立结果写本轮目录。单日志可用 `python tools/m52_partition_trace.py <trace.log> --expected-plans <case/expected-plans.json>`，关闭路径加--fixed不传开启计划。源码重编：WSL `cmake --build artifacts/m52-b/diagnostic-decoder/build --target avmdec -j4`；新配置/构建原始日志保留。

固定验收oracle SHA本轮仍为 `110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`，规范SHA同上；M51保存的诊断decodemv与binary SHA也分别仍为bffaa503...和273402b0...。生产清单artifacts/m52-b/source-manifest.json SHA256 `0BB27742F821C35843C894256AE8CFCD2BD39CF9F6D7796761A09E55C78CC3C4`。B不改正式版本；最终更大矩阵、off历史指纹、API/内存检测/长链/测量由00整合，不能以本局部交付宣布完整M5完成。

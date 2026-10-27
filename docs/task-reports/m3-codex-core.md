# M3 A：像素、数学与重建链

状态：属主生产源码冻结；MSVC Debug /WX clean 构建、CTest 29/29通过。完整码流矩阵、长序列与跨编译器验收由主窗口记录，不将局部自测代替最终 oracle。

## 范围及接口

完成 T017/T018/T019/T020/T023：保留8-bit420，新增10-bit420与10-bit444。FrameBuffer/BorrowedView携带bit_depth/chroma_format，所有平面仍为byte pointers，strides为bytes；10-bit为unsigned LE word，sample_load/store逐字节访问，无alignment/host-endian前提。alloc_ex/view_ex接受width,height,bit_depth,chroma_format，旧入口为8/420包装。10-bit copy先全帧检验0..1023再写目标，越界不截断且无部分写入。raw pack共用原唯一导出函数。

Geometry init_ex最后参数为chroma_format；420沿用原half映射，444与luma相同。两种格式共享唯一coded binary tree与leaf顺序。预测reference为uint16+bd；目标和重建保留byte plane；残差为int16，stride为samples。frame core根据source/recon metadata验证一致性，真实qcoeff hook未改。

新增fwd_transform_bd/inv_transform_bd最后参数bd，quant_params_init_tx_bd最后参数bd，transform_path_init_bd(state,path,actual_qindex,bd)，旧入口包装8-bit。重建reset值uint16，预测加inverse residual后按bd clip。内部ReconExport携带bd/chroma，chroma dimensions通过picture helper取得，444不再固定half。

## AVM依据与量化

AVM av2/av2_cx_iface.c1362：offset_qp=(bit_depth-8)*MAXQ_OFFSET，MAXQ_OFFSET24；rc_cfg->qp=extra_cfg->qp+offset。用户QP100：8-bit实际qindex100，10-bit实际148。quant_common.c qlookup100=704、148=2816，四倍。entropy.c get_q_ctx直接使用实际qindex，无bd offset抵消，因此10-bit148使用ctx2；B窗口负责真实header与CDF。

AVM encoder/hybrid_fwd_txfm.c995：forward两段shift只依TX geometry，与bd无关；高位深仅扩大合法输入残差到±1023。AVM common/idct.c655-660/710/724-728：中间clamp为bd+8有符号精度，最终inverse residual为[-2^bd,2^bd-1]，之后预测相加再clip。原64TX packed32x32及2x2上采样路径保留。

## 范围证明

DC邻居最多128个1023，sum<=130944；approx divisor乘最大512<=67043328，uint32可容纳。差分残差[-1023,1023]，int16准确。

DCT matrix最大绝对系数90。支持square最大64列，每次64*90*1023及其后续stage和均以int64累加；最保守第二forward stage可超过有符号32，因此不改成int32。逐supported shape shift的square保守forward最大约258948；10-bit QP148最小有效quant scale(64TX)=2816/8/4=88，level上界约2944，低于既有8191生产上限，故保留该上限不扩无必要范围。inverse首先clip至[-131072,131071]，32工作kernel的matrix累加上界约377487360，int64有余量。10-bit decoder dequant clip为bd+7，其乘法继续int64。

## 验证

- tests/unit/test_transform.c：11个实际支持TX形状×8/10-bit×100固定seed，forward对独立matrix模型，inverse使用独立随机dq输入对独立AVM matrix/shift/两次clamp模型，非roundtrip自比较。64TX packed与upsample也参与。
- test_intra_prediction.c：square4/8/16/32/64×8/10-bit×100固定seed×四种邻居组合，独立整数均值；10-bit差分独立检查。非对齐目标、odd stride及写界哨兵。
- test_quantization.c：square4/8/16/32/64×8/10-bit×100向量，q704/q2816/round/reciprocal独立公式与signed dequant/clamp比较。
- test_picture.c：10bit420/444非对齐borrowed input、odd byte stride、LE输出尺寸/字节、padding1023；第三plane1024拒绝并验证目标整帧未变化。
- test_reconstruction.c：100随机10-bit residual×三planes，clip0..1023、reset512、raw384bytes与444metadata。
- test_block_geometry.c：176x144 420/444 coded leaves逐一相同，444 chroma与luma坐标/尺寸一致。
- 独立build/m3-a MSVC19.44 Debug /WX；clean首轮新几何test类型warning已修，续构建0告警；CTest29/29通过。

日志：artifacts/m3-a/core-build.log、encoder-build.log、tests-build2.log、clean-build.log、clean-build-finish.log、ctest-full.log。B首轮真实64x64三格式×1/3帧6/6 AVM decode与bitexact已通过（artifacts/m3-b/smoke64）。最终48矩阵/1000frames/ASan/LSan/crosscompiler须以主窗口报告最终记录为准。

## 主窗口终验接收

2026-10-11，模块交付已集成，整阶段M3 DONE。48项矩阵、跨平台、连续1000帧与ASan/UBSan/LSan证据见 [M3主窗口报告](M3-integration.md)。主窗口最终版本标识及CLI提示编辑后的清单为最终源指纹。

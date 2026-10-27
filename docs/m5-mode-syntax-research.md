# M5.1 DC/V/H 模式语法：规范、固定解码器与独立检查

日期：2026-10-16。范围仅 Y DC/V/H（joint 0/22/50，angle delta=0），UV 保持 DC；AI 和 LOW_DELAY_P intra fallback 共用已选择模式。未实现动态内容 partition、完整 RD 或其他方向工具。本轮结果与历史 AI 证据分别保留。

## 固定依据

规范：`../docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，SHA256 `e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f`。下列页码为 PDF 实际页序，亦与页面页脚相同。原始文本提取见 `artifacts/m5-b/spec-all.txt` 与 `spec-mode-search.txt`。

AVM 固定 commit：`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`。解码语法原始来源 URL 形如 `https://raw.githubusercontent.com/AOMediaCodec/avm/966a7d7cd6fcf60360caf5dc413b2aeeb65e144d/av2/decoder/decodemv.c`；本地只读快照 `artifacts/m4-root/oracle-v1-reference`，补充 reconintra 为 `artifacts/m5-root/oracle-v1-research/reconintra.c`。行号采用未插入诊断语句的固定源码，不能用当前 avm junction HEAD 代替。

固定验收 decoder：`../AVM-AV2-TEST/tools/bin/avmdec.exe`，SHA256 `110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`。单独固定 tag 的诊断 decoder 仅在 artifacts 下增 trace；其变更补丁、源与二进制 hash 见下文，不修改该固定验收程序。

## 逐字段与条件表

| 事项 | 规范精确位置 | 固定 AVM 路径/行 | 本期值与序列化 |
| --- | --- | --- | --- |
| P intra 调用链 | §5.20.7.5，p189–190 | av2/decoder/decodemv.c:2133–2289 read_intra_block_mode_info | is_inter=0 后走真实 Y/UV intra，复用单一 core mode |
| Y mode set/index | §5.20.5.5，p174–176 | decodemv.c:1517–1547 read_intra_luma_mode | set 有4符号；set0 index有8符号，index7追加6符号offset；其他set追加L(4)，不能把enum当wire index |
| joint→实际模式 | §5.20.5.5，p174–175 | reconintra.c:371–387 av2_set_y_mode_and_delta_angle | joint0=DC，22=V，50=H，delta0 |
| 模式列表重排 | §5.20.5.5，p175–176 | reconintra.c:391–476 get_y_intra_mode_set | 先5非方向；左下、右上邻居的方向去重；大于64面积时加±1..4方向；再补固定56方向表 |
| 邻居与不可用 | §5.20.5.5 get_joint_mode，p175 | reconintra.c:351–363；av2/common/blockd.c:22–27 | 左下(x−1,y+h−1)、右上(x+w−1,y−1)，不可用返回DC；inter/intrabc也返回DC |
| inter 覆盖旧方向状态 | §5.20.7.6，p190：IntraJointMode=DC_PRED | blockd.c:22–27 av2_get_joint_mode | 每个inter叶覆盖同跨度above/left为0，不能保留更早V/H |
| Y CDF context | §8.3.2，p589–590 | reconintra.c:351–363；entropy_inits_modes.h:864–890 | ctx=两邻居joint≥5之和，为0/1/2；set CDF不分ctx，index/offset按ctx |
| UV 模式列表 | §5.20.5.6，p176–178 | decodemv.c:1549–1586；reconintra.c:481–514 | Y方向性时UV列表首项匹配Y，DC为index1；Y=DC时UV_DC index0 |
| UV CDF | §8.3.2，p590 | entropy_inits_modes.h:891–897 | context=is_directional(Y)，两行均8符号；本期不进入UV尾literal |
| CFL/MHCCP | §5.20.5.6，p177–178 | decodemv.c:1556–1567；:2230附近 | enable_cfl_intra=enable_mhccp=0，无is_cfl符号；UV_DC非CFL |
| lossless/DPCM/FSC | §5.20.5.5，p174–176 | decodemv.c:2157–2196 | qindex100/148非lossless，DPCM implied0；enable_idtx_intra=enable_fsc=0，无FSC符号 |
| MRL/角度 | §5.20.5.5，p175；§7.13.2.7，p453–456 | decodemv.c:2197–2229；reconintra.c:842–844/868–870 | enable_mrls=0、delta0，mrl_index0；正方形V/H不发生wide-angle变化 |
| Palette 条件 | §5.20.8.1，p235–236 | decodemv.c:1043–1061 | P screen tools=1且size8..64允许palette入口，但只有Y=DC才读has_palette_y=0；V/H不写该符号 |
| DIP/边缘/IBP | §7.13.2.7，p453–456 | decodemv.c:1064；reconintra.c方向预测路径 | enable_dip/intra_edge_filter/ibp均0，无额外DIP或边缘过滤/混合；继承已验证sequence配置 |
| V/H scalar 预测 | §7.13.2.8，p456–458 | reconintra.c:334–335、842–844、868–870 | pAngle90复制AboveRow，180复制LeftCol；只选有该方向已重建邻居的候选；Clip与位深由核心实现 |

CDF 数值锚点：y_mode_set forward={28863,31022,31724}；Y index ctx0首15175、ctx1首10114、ctx2首5636；offset首分别12743/8976/8784；UV ctx0首9363、ctx1首21282。具体完整行在生产静态表及固定 entropy_inits_modes.h，CDF更新关闭且PRIMARY_REF_NONE，不能沿前帧更新CDF。

## 独立值级检查

`tools/m5_mode_trace.py` 不读取 encoder 自报模式，而消费固定 tag 诊断 decoder 实际执行 read_intra_luma_mode/read_intra_uv_mode 的值。诊断同时输出完成 block 的位置/尺寸/inter/joint，Python 根据所有已完成block独立建立逐4×4邻居地图，重算左右邻居、ctx、完整61项列表、set/index、DC/V/H及delta，并验证UV context/index与DC结果。inter block在地图中强制joint0，检查后续intra邻居确实取0。

独立 Python 模型使用规范p176的±1..4方式构造列表，与C writer的模运算排列实现分离。self-test包含9对邻居、8×8与大块、固定显式模式列表锚点，以及破坏Y index和UV context的两项负向检测。实际日志覆盖set0和set1，不能只靠模式计数判断正确。

本轮12例真实诊断矩阵：三格式 × AI/P × directional/mixed，每例176×144×3帧。`artifacts/m5-b/mode-trace-matrix.json`：全部PASS，实际817个intra及817个UV记录，39次intra邻居来自inter，ctx0/1/2及V/H均覆盖；诊断raw与编码器预先生成的internal raw SHA逐例相等。10bit420 P directional的81块含2 inter和79 intra，P第1索引帧实际23V/1H/2DC，第2索引帧24H/2DC；因此不是仅首key使用V/H。

原始日志 `artifacts/m5-b/*176x144_3f*.trace.log`；诊断相对原M4诊断copy增量 `m5-decoder-trace.patch`，源码/二进制SHA在mode-trace-matrix.json。诊断与不可变Windows oracle各自核对内部raw；最终完整144矩阵及off指纹由00验收。本工具验证限定DC/V/H子集，不能用于宣称任意方向、MRL、CFL、DIP或动态partition已实现。


诊断保存补记：原执行位置及本轮增量仍如上记录；完整instrumented源码和相同SHA的运行程序现已隔离到 `artifacts/m5-b/diagnostic-decoder/source` 与 `bin/avmdec`，12例用保存程序再次重放全部通过（replay/verification.json），不再依赖历史M4目录。preservation.json及矩阵JSON记录原/新路径、程序SHA `273402b0db584ce922e5020879eb97dd245003fb3b32525f5600126760e9d6ab` 和源SHA。旧M4诊断目录因缺少有hash佐证的原binary备份未做猜测回滚，完整说明及可复现命令见独立目录README。
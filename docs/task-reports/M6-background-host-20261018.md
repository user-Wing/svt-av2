# M6 后台批处理再次退出：证据与计划任务托管

2026-10-18。用户反馈恢复后的后台进程再次丢失；本轮排查并更换外部托管方式，不修改编码器或冻结配置。正式仍0.0.5-m5，M6未最终验收。

## 本次退出与上次重启不同

机器LastBootUpTime仍为04:52:25，07:50以后未读到新的41/6008/1074记录。前次恢复总控11508实际记录了FAIL/real-quality：其子批处理18600于北京时间08:12:56.451以状态码1退出。总控stderr保留CalledProcessError；batch本身没有Python traceback、没有FAIL case，summary仍停在RUNNING。质量日志最后一条PASS在08:07:10。

65个完整质量结果均PASS，较前次恢复前64个新增3D/M5 QP248一项，证明恢复后确实继续运行过。现有证据不支持“本次又重启”或“已定位编码器崩溃”；也不足以断言是Codex会话清理、哪个进程调用了终止、或Python内部退出。相应窗口未读到可解释退出的Application错误、Defender阻断或安全日志进程退出事件；系统TLS警告与此无已证实因果关系。

前次“已确认启动”判断有实际进程和输出证据，但未能保证之后长期存活。本轮不能只重复同一直接启动方式。

## 保留与复核

证据目录：`C:/Private/ffmpeg-native-m6-tests/recovery-local-r2-20261018-0823`。已保存FAIL状态、旧launch、batch summary、stdout/stderr和完整质量日志，再独立检查所有已有case保留产物SHA、run_id/job、退出码及RD trace。

- 完整质量65/108 PASS：2D36、3D29、Real0，没有FAIL结果；43项未完成。
- pilot6、小图1512、筛查576、AV1既有3/fresh18全部有效。
- 76项冻结文件及配置SHA全部匹配，原run_id未变。
- 原独占锁均可获取，不删除锁文件，不删除任何attempt。未完成项将另建attempt重跑，65个已完成结果复用。

## 新的外部托管

创建一次性、手动启动的Windows计划任务 `SVT-AV2-M6-local-r2-20261017`，使用当前用户Interactive登录类型，无密码保存。先以同一外部host执行2秒probe，计划任务结果0；确认probe的父进程是计划任务服务的svchost.exe，再切换正式action。并未把IsProcessInJob=true误解为完全脱离所有Windows Job，也不以该读数认定旧退出原因。

正式host是证据目录内 `task_host.py`：直接运行原冻结m6_campaign.py和原campaign-config.json；仅追加Python非缓冲输出及faulthandler，保存独立stdout/stderr、host/controller PID，并在控制器退出时记录exit code及时间。它等待控制器，不运行额外编码负载。没有改snapshot、模型、媒体、共享依赖、QP、jobs或timeout；未替换r2/r3优化后编码程序。

任务设置：ExecutionTimeLimit=PT0S（不设运行时限）、Priority4、MultipleInstances=IgnoreNew，允许电池启动且不因切换电池停止。不设置自动重试，防止真正错误被反复重跑掩盖；没有新增周期触发或自动开机触发。任务XML、probe和host记录均保留在证据目录。

北京时间08:24:20启动：host PID21732、总控PID17392，任务状态Running，原配置继续从头核验并复用已有结果。日志为原批目录 `20261018-082420-scheduler.stdout.log` / `.stderr.log`，原launch已另存，现launch关联新总控和任务名。

08:26:29复核：前9阶段再次PASS，RUNNING/real-quality，新batch PID20600、run_id与原批一致，3个编码子进程14904/4404/1652实际运行，分别续跑3D/pruned QP232/240的attempt-0003及3D/off QP248的attempt-0002。65个PASS保留复用。计划任务State=Running，LastTaskResult=267009（0x41301，正在运行，不是失败返回码）；stderr为空。该确认不能保证未来永不退出，若再退出应检查scheduler-exit.json、计划任务结果和新未缓冲日志，不能先归因或盲重启。

用户随后报告桌面出现Python控制台。已针对已核对host PID21732附着其控制台并隐藏窗口（句柄920034，visible true→false），host/总控/batch/3编码子进程PID全部保持，未中止或重启测试。证据console-hidden.json。后续计划任务action改用pythonw.exe，host启动实际python.exe控制器时设置STARTF_USESHOWWINDOW/SW_HIDE，让后续子进程继承隐藏控制台；本次正在运行的测试不重新启动。计划任务XML已同步保存。

后续查看：

```powershell
Get-ScheduledTask -TaskName SVT-AV2-M6-local-r2-20261017
Get-ScheduledTaskInfo -TaskName SVT-AV2-M6-local-r2-20261017
Get-Content C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017\campaign-status.json
# 确认没有存活总控/编码子进程且解决失败原因后，手动续跑同一托管任务：
Start-ScheduledTask -TaskName SVT-AV2-M6-local-r2-20261017
```

仍以任务状态、真实进程、总控/子阶段状态与case结果共同判断，不能因一个RUNNING文本就断言持续正常。进入isolated-speed禁止其他编译/计算；全部完成后仍须独立审计，不自动发布M6。

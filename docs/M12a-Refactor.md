# M12a：核心源码职责拆分（进行中）

## 基线与保护范围

- 2026-11-26，main 基线 a854cc6。经用户明确确认，fetch 后以快进方式推送 origin/main；未 force-push。
- 无关未跟踪 PDF 保持原样，不纳入提交。其他项目本轮未修改。
- 本轮只移动函数实现、整理必要的私有依赖；不改变比赛规则、参数、模型资产或公开 API。
- 角色/场景/玩法上市级提升属于后续里程碑，本轮不可据此宣称达标。

## 已实测基线

```powershell
./tools/m11g_verify.ps1 -Suites Automation -Label m12a_baseline
```

日志：`Saved/Logs/Codex_M11g_m12a_baseline_Automation.log`。83 项 Result={Success}；TEST COMPLETE EXIT CODE: 0；脚本 exit=0、timeout=False、fatal=False。

启动阶段有引擎 LogAutomationTest Condition failed 噪声，项目测试结果全部成功；不宣称全文零 Error。此次为 NullRHI 逻辑基线，不能替代图形或真人试玩验收。

## 缓存与资源约束

基线日志确认 Zen data dir 为 D:/ZenData，共享 DDC 为 D:/UE_DDC，但 Local DDC 仍为 C:/Users/dsh/AppData/Local/UnrealEngine/Common/DerivedDataCache。后续构建/验证通过进程环境指定 D 盘 TEMP/TMP 与 UE-LocalDataCachePath，不能把共享缓存设置当作本地缓存已迁移。不得清理旧缓存、改变全局变量或终止非本任务进程。UBT并行度不超过4，构建和图形测试串行。

## 本轮构建失败记录（保留，不计为 PASS）

首次 Editor 构建日志：`Saved/Logs/M12a_SpikeEliteEditor_build.log`，exit=6，OtherCompilationError。新增 GameModeHelpers.h 只包含 VolleyballEnums.h，缺少定义 EBallTouchType 的 VolleyballRules.h，导致 C4430/C2146。控制器拆分各编译单元已编译，但不能据此宣称整个构建通过。

该次构建启动时 GameMode 代理仍未结束写入，是集成调度失误；此次不作最终源码验证。已收集失败日志，后续必须等代理停止写入，修复明确的包含依赖，再对最终源码完整构建。没有进行图形测试或与运行中的 Editor 同时链接。

## 最终源码审查

- 控制器原60个方法拆分后仍为60个，唯一60个，无重复；原方法块保持不变。按菜单、输入、教练、开发验收分区，LogSEMenu由原cpp唯一定义、私有头声明。
- GameMode原61个方法拆分后仍为61个；换人、暂停、统计、计分流程、发球介绍迁出，辅助函数增加唯一命名空间限定。潜水tracking静态状态与紧耦合AI部分保留原cpp，避免扩大本轮行为风险。
- 此次是M12a第一阶段拆分，GameMode核心仍较大，不能宣称所有规则/AI职责已完全拆开。
- Editor最终构建通过（Saved/Logs/M12a_SpikeEliteEditor_final_build.log）；adaptive non-unity将新增单元独立编译，验证了直接头依赖。Game最终构建也通过（Saved/Logs/M12a_SpikeElite_final_build.log），两目标串行构建exit=0。完整运行回归已结束：Automation 83/83，Closeout、FiveSet、Tactical、Rematch、Art、Playability脚本均PASS，exit=0、无超时/Fatal/Assertion/Ensure。最终Automation启动日志确认Local DDC已使用D:/UE_DDC_Local（进程级环境覆盖）。

## Shipping 编译补验

2026-11-26 对 f681b7f 执行 SpikeElite Win64 Shipping 编译，MaxParallelActions=4，TEMP/TMP仅进程级指向D:/ue-temp。日志 Saved/Logs/M12a_Shipping_build.log：Result: Succeeded，exit=0，36.67秒。生成 Binaries/Win64/SpikeElite-Win64-Shipping.exe。此项仅验证条件编译及链接，不等于Cook、独立打包、启动或发行验收。

可复现：使用引擎自带dotnet执行UnrealBuildTool.dll，参数 SpikeElite Win64 Shipping -Project=D:/projects/spike-elite/SpikeElite.uproject -WaitMutex -NoUBA -MaxParallelActions=4。

## 后续验收清单（状态逐项列明）

1. 已完成：函数移动完整性核对与公开接口保持。
2. 已通过：Editor/Game Development和Game Shipping编译。
3. 已通过：Automation,Closeout,FiveSet,Tactical,Rematch,Art,Playability自动化运行。
4. 未完成：38张截图逐张目视复核、重打独立包与运行验收。
5. 已提交：M12a第一阶段f681b7f，未推送；不计为完整M12a或上市级完成。

## 最终运行证据与视觉限制

```powershell
$env:TEMP="D:/ue-temp"; $env:TMP=$env:TEMP
[Environment]::SetEnvironmentVariable("UE-LocalDataCachePath","D:/UE_DDC_Local","Process")
./tools/m11g_verify.ps1 -Suites Automation,Closeout,FiveSet,Tactical,Rematch,Art,Playability -Label m12a_final
```

日志均位于Saved/Logs/Codex_M11g_m12a_final_<Suite>.log。本轮生成38张截图（Closeout 11、Tactical 7、Rematch 1、Art 7、Playability 12），时间2026-11-26 13:14–13:19，位于Saved/Screenshots/WindowsEditor。Art后缀00003，Closeout大多00005，Playability大多00005；不能用历史后缀替代。

尝试逐张打开时，read_image明确返回当前gpt-6-astra不支持图像输入。遵循不触碰失败边界：不重复尝试、不转换图片绕过限制。**视觉人工验收未完成**，截图存在及自动化PASS不能替代画面质量验收。M12a保留待视觉复核状态；不宣称上市级，不新增发布包或Release。

## 资产

本轮没有导入或购买外部资产。已找到引擎模板Manny/Quinn候选资源；尚未验证骨骼、依赖、许可适用范围或游戏内效果，不计入M12b完成项。

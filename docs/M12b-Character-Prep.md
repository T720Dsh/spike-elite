# M12b 角色资产审查：尚未接入生产

2026-11-26使用tools/inspect_character_assets.py在UE5.8中只读加载/Game/Mannequins。Asset Registry登记101项；报告loaded=4，其余97项在加载时抛出异常。注意loaded只表示取得UObject，不代表骨架有效、动画蓝图编译成功或可商用。

日志Saved/Logs/M12b_character_audit.log：ABP_Unarmed明确报骨架资产缺失，多个AnimSequence异常链报告USkeleton缺失/无效；报告Saved/Logs/M12b_character_assets.json。命令exit=1，保留为失败证据，不反复重跑。没有调用资产保存、导入或删除。

复现命令（修复前不建议重复执行）：
```powershell
& D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe D:/projects/spike-elite/SpikeElite.uproject -run=pythonscript -script=D:/projects/spike-elite/tools/inspect_character_assets.py -NullRHI -unattended -nosplash -ABSLOG=D:/projects/spike-elite/Saved/Logs/M12b_character_audit.log
```

## 安全接入要求

- 不将现有损坏动画直接接入比赛，不删除原资产或盲目补路径。后续候选应使用独立目录，完整审查Skeleton、网格、动画、材质及许可依赖。
- EAnimPose映射由C++ AnimInstance读取：待机、移动、跳跃、接球、二传、扣球、拦网、扑救、恢复、发球。现有触球时间由权威玩法逻辑发出；AnimNotify不得反向决定计分或球物理。
- 保留Capsule为唯一玩法碰撞；手部IK只调整视觉。缺失骨骼/动作时明确后备，不把A-pose站立替换当完成。
- 角色动作完整性、球衣号码、第一人称遮挡、脚底滑动和手掌触球必须在图像能力恢复后目视复核。
- 资产必须提供来源URL、作者、许可和再分发证据；本轮未导入资源、未购买或注册账号。

## 当前结论

可验证进展是查明现有角色资产无法直接使用；不是完成角色建模。游戏源码、Content均未改动，正常比赛仍用已验证的程序化角色。整体开发目标继续，视觉验收限制与现有缺依赖资产分开记录。

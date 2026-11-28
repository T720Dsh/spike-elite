# 验收修复：单人模式其他队员无反应（2026-11-28）

用户反馈："单人试玩的时候，6个人的其他角色没有反应。"

## 实测定位（不是靠读代码猜）

在 Development 版加了只读探针，每 5 秒输出两队机器人的累计 2D 位移：

修复前（`Codex_M11g_botprobe2_Quick42.log`）：
```
teamA_bot_travel_cm=905  moving_samples=95
teamA_bot_travel_cm=892  moving_samples=93
teamA_bot_travel_cm=447  moving_samples=52
```
机器人**确实在动**，但 `EAIBehavior::Wait` 与 `ReturnHome` 在 `TickBot` 里都取 `HomePosition`。
每回合只有 1 名 primary handler 跑向球，另外 5 人回到出生点原地不动——这就是"没反应"的来源。

## 修复

1. `SpikeEliteCharacter::TickBot`：`Wait` 不再无条件回家，而是执行 GameMode 下发的
   `AITargetLocation`（非零时），让防守/支援站位真正生效。
2. `SpikeEliteGameMode` 的 `DirectTeam`：无球队员的 `Wait` 目标现在跟随球的横向与纵深
   偏移（横向 ±170 的 45%、纵深 ±120），并夹在 X 60..930、Y ±480 内，
   保证阵型与过网分隔不被破坏。

## 验证

修复后（`Codex_M11g_aiafter_Quick42.log`）：
```
teamA_bot_travel_cm=1983 moving_samples=219
teamA_bot_travel_cm=1915 moving_samples=205
teamA_bot_travel_cm=2300 moving_samples=238
```
两队位移与"正在移动"的采样数约翻倍，对局仍正常打到 MatchOver。
自动化测试 **83 项全通过、0 失败**（`Codex_M11g_aiafter_Automation.log`）。

## 诚实边界

- 这是**无球移动**的修复；球轨迹可视化、击球特效、Blender 角色导入 UE 仍未完成。
- 我无法查看截图，没有做目视验收；上面全是日志数值。
- 探针是 `#if !UE_BUILD_SHIPPING` 的只读日志，不影响 Shipping。
- 本轮未打包新的 Shipping 版本。

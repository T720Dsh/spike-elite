# SPIKE ELITE 🏐

**The first console-quality, 3D indoor volleyball game built in Unreal Engine 5 — starting on PC, coming to mobile later.**

> 工作代号：SPIKE ELITE（暂定，欢迎在 Issue 中提议正式名）
> 引擎：Unreal Engine 5
> 首发平台：**PC / Windows（Steam）**，键鼠 + 手柄
> 后续平台：iOS / Android（PC 版验证玩法后再移植）
> 定位：对标 *EA SPORTS FC* / *NBA 2K* 的排球品类作品，主打**第一人称沉浸视角** + 第三人称/俯视多视角切换

[![Status](https://img.shields.io/badge/status-pre--production-orange)]()
[![Engine](https://img.shields.io/badge/Engine-Unreal%205.8-blue)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20PC-lightgrey)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

---

## 为什么要做这个游戏？

足球有 FC Mobile / eFootball，篮球有 NBA 2K Mobile，而排球——这项全球 2 亿+ 参与者、奥运会核心项目、在欧洲/南美/日本/东南亚拥有狂热观众的运动——**至今没有一款移动端 3A 水准的官方模拟作品**。

现存的排球游戏要么是 2D 街机小品（Thunder Spikes、Cyber Volleyball），要么是十年前画质粗糙的独立作品（Spike Volleyball、Volleyball Champions 3D）。这是一个被大厂完全忽略的品类真空。

**SPIKE ELITE 的目标：把 FC Mobile / NBA 2K Mobile 验证过的"真实授权球员 + 卡牌养成 + 真实时 PvP + 控制台级画面"公式，首次搬到 6v6 室内排球上。**

详细论证见 [docs/02-market-research.md](docs/02-market-research.md)。

## 核心体验

- **真实 6v6 室内排球**：严格遵循 FIVB 2025–2028 规则，rally point、顺时针轮换、5-1 / 6-2 进攻体系、Libero 自由人、拦网/吊球/后排进攻全部还原。
- **第一人称沉浸视角**（核心卖点）：你就是场上那个球员——跳起来扣球的主观冲击力、3D 音频定位、辅助雷达与轨迹预测，解决 FP 看不到全场的问题。第三人称肩后镜、战术俯视镜、直播观战镜并存。
- **PC 原生操作**：WASD + 鼠标/键鼠，完整支持 Xbox / DualShock / DualSense 手柄；手动/半自动 AI 双模式。
- **3A 级渲染**：UE5 Lumen + 硬件光追 + Nanite，动捕动画系统，动态观众与场馆氛围。
- **真实球员与球队**：首发争取国家联赛/俱乐部授权（FIVB Nations League、欧洲冠军联赛、日本 V.League 等方向），未授权期间先做"风格化原创球员 + 可解锁数据"。
- **持久化成长**：球员卡收集、阵容构建、化学反应、赛季 Pass、限时活动——FC / 2K 已验证的长线运营模型。

## 文档地图（给合作者）

| 文档 | 内容 |
|---|---|
| [01-design-philosophy.md](docs/01-design-philosophy.md) | 设计理念、产品北极星、不做什么 |
| [02-market-research.md](docs/02-market-research.md) | 对标游戏拆解、竞品分析、市场空白 |
| [03-game-design-document.md](docs/03-game-design-document.md) | GDD：玩法、模式、操作、卡牌、经济系统 |
| [04-tech-stack.md](docs/04-tech-stack.md) | UE5 技术方案、渲染分档、网络同步、目录结构 |
| [05-roadmap.md](docs/05-roadmap.md) | 里程碑路线图（Prototype → Soft Launch → Global） |
| [06-art-direction.md](docs/06-art-direction.md) | 美术风格、角色管线、场馆与 UI 方向 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 如何参与、分支规范、沟通渠道 |

## 当前状态

✅ **可玩垂直切片（PC 原型 → M10 里程碑）** — 引擎 UE 5.8，纯 C++/UMG，无蓝图依赖。

- [x] M0 市场调研、设计理念、GDD、技术选型
- [x] M1 工程初始化（C++ 项目、输入、渲染基础）
- [x] M2 室内体育馆 + 场地线 + 球网权威碰撞
- [x] M3 主菜单 / 设置 / 暂停菜单 / 计分牌（UMG）
- [x] M4 基础 6v6 方块人角色与球运动（单一权威 ProjectileMovement）
- [x] M5 完整规则状态机：PreMatch / BetweenRallies / AwaitingServe / ServingToss / Rally / SetOver / MatchOver
- [x] M6 三次触球规则：控球队伍、0–3 触球计数、同一球员连击判罚、过网换边、界内/界外/压线判分
- [x] M7 队伍级 AI 协作：预测落点接发、指定二传、预选进攻点、职责站位（非全员追球）
- [x] M8 比赛反馈：回合结果横幅、阶段/控球/触球 HUD、发球提示、触球范围提示
- [x] M9 比赛结束界面（胜者/各局比分/再来一场/回主菜单/退桌面）、确认模态、Esc 行为明确
- [x] M10 规则自动化测试（13 项，纯逻辑无地图）、`-QuickMatch` 快速对局、Win64 打包
- [ ] 战术 AI（拦网、后排进攻、自由人）、角色动画模型、网络同步

下一步具体行动见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 快速开始（给开发者）

引擎版本：**Unreal Engine 5.8**（`.uproject` 的 `EngineAssociation` 为 `5.8`）。

```powershell
# 1) 用 UE 5.8 打开工程（首次打开会提示编译 C++，点 Yes）
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe .\SpikeElite.uproject

# 2) 直接以游戏模式运行（无需编辑器 UI）
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe .\SpikeElite.uproject -game -windowed -ResX=1600 -ResY=900

# 3) 打包 Win64 Development 到 Dist\（产物不入 Git）
D:\Epic\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun `
  -project="D:\projects\spike-elite\SpikeElite.uproject" -noP4 `
  -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak `
  -archive -archivedirectory="D:\projects\spike-elite\Dist"

# 4) 运行打包后的游戏（不启动 UnrealEditor）
.\Dist\Windows\SpikeElite\Binaries\Win64\SpikeElite.exe
```

玩家入口：双击 **`Play_SPIKE_ELITE.bat`**（优先启动打包版 exe，无打包时提示先执行构建脚本）。

### 操作说明

| 操作 | 按键 |
|---|---|
| 移动 | WASD |
| 视角 | 鼠标 |
| 击球 | 鼠标左键（跳跃中为扣球） |
| 发球 | E（发球准备时；无人操作 3 秒后自动发球） |
| 第一/第三人称 | C |
| 暂停 / 释放鼠标 | Esc |
| 快速对局（开发） | `-QuickMatch`（一局 3 分制，复用正式规则） |

## 规则自动化测试

```powershell
# 无地图纯逻辑测试（13 项）：
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe .\SpikeElite.uproject `
  -ExecCmds="Automation RunTests SpikeElite.Tests; Quit" -unattended -nosplash -nopause -log
```

## 路线图速览

```
M0–M4 原型基础 (已完成)      → 场馆、场地线、球网、菜单、6v6 方块人、球运动
M5–M9 规则与反馈 (已完成)    → 规则状态机、三次触球、AI 协作、反馈/结束界面
M10 垂直切片 (已完成)        → 规则测试、QuickMatch、Win64 打包、文档
M11 战术 AI (进行中)         → 拦网、后排进攻、自由人、动画模型升级
M12 PC 封测 (规划)           → PvP 局域网/Steam 匹配、卡牌系统雏形
M13 Steam EA (规划)          → Early Access 上线
M14+ 手游移植 (规划)          → 分档渲染降级，iOS/Android 上线
```

详见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 团队在找什么人

- UE5 C++ / 蓝图工程师（游戏玩法、动画状态机、网络同步）
- 前向/运动捕捉动画师（排球专项优先）
- 体育游戏系统策划（卡牌、经济、赛季）
- 客户端图形/渲染工程师（移动端优化、 Niagara 特效）
- 后端工程师（匹配、账号、战令）
- 对排球规则熟悉的测试/社区运营

有兴趣？读 [CONTRIBUTING.md](CONTRIBUTING.md)，然后开 Issue 自我介绍。

## License

设计文档与规划：[MIT](LICENSE)。UE5 引擎与第三方资产遵循其各自许可。

---

Built with 🏐 by volleyball fans, for volleyball fans.

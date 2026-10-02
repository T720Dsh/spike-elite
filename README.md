# SPIKE ELITE 🏐

**An early playable 3D indoor volleyball prototype built in Unreal Engine 5, with commercial-quality presentation as a long-term goal.**

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

以下列出产品愿景，**不是全部已实现功能**。当前仍使用原创程序化角色；自由人、入场列队动画、裁判动作手势、训练模式完整项目、名单编辑 UI 与商业级写实资产未完成。M11h 玩法轮（模式选择/12 人名单/球队暂停/合法换人/教练操控/发球员介绍/赛后统计）见 [M11h 交付记录](docs/M11h-Delivery.md)，现场美术与验收见 [M11g 交付记录](docs/M11g-Codex-Art-Delivery.md)。

本机最新图形包：`Dist/M11h/Windows/SpikeElite.exe`；根目录 `Play_SPIKE_ELITE.bat` 优先启动此包。包与实机截图不入 Git，源码更新后需重新构建，不代表 GitHub Release 已更新。

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

✅ **可玩垂直切片（PC 原型 → M11b 里程碑）** — 引擎 UE 5.8，纯 C++/UMG，无蓝图依赖。

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
- [x] M11a 确认弹窗状态机、自动化与正常游戏隔离、MatchOver 输入门、规则核心复用、HUD/AI 更新节流、发布卫生（18 项测试）
- [x] M11b-1/2 国际比赛场馆（60×44×15 m 壳体、四面分层看台 ISM、裁判架/记录台/替补席、实时记分牌）、回合缓冲与鸣哨流程、轮转 HUD
- [x] M11b-3 可见肢体程序动画（分段关节、11 姿态、抬手、第一/第三人称）
- [x] M11b-4 战术慢动作 + 虚线球路预览（真实弹道积分、界内/触网/出界判定）
- [x] M11b-5 数据驱动二传战术（13 种 + 自由轨迹）、真实拦网（不计触球、可连续触球、打手出界）、倒地救球（扑救/恢复门）
- [x] M11b-6 比赛用球（无品牌黄蓝占位 + 授权资产插槽）、观众密度三档、性能心跳、27 项自动化测试、Win64 打包冒烟
- [x] M11c-1 发球球权纠错：ServeFlight 期间禁止本方触球、发球不占三次触球、发球站位端线外（X=±1150~1300）
- [x] M11c-2 权威轮转：ERotSlot P1~P6 槽位坐标、前后排判定、side-out 仅换发队顺时针轮转、轮转索引 1~6 循环
- [x] M11c-3 真实倒地救球生命周期：NetCross 阈值 5cm、Dive State None→Approach→Active(0.45s)→Recovery、[DiveAttempt]/[DiveSave]/[DiveMiss] 日志
- [x] M11c-4 战术同源求解器：BuildShotSolution(Start,Intent,TimingError) 单一实现、Power 即时改虚线、Perfect 零误差、真实时间 DeltaTime
- [x] M11c-5 屏幕 UMG 战术界面：攻击/13+1 二传/8 项防守决策面板、EVolleyballDefensePlan、FSetPlayDefinition 数据驱动
- [x] M11c-6 球物理回归修复（Mesh 为根组件 + BandMesh 子级补偿缩放）、授权球插槽 fallback、RotationWidget/Scoreboard 分辨率适配
- [x] M11c-7 测试收口：55 项自动化测试 0 failed、-TacticalTest 战术功能验收、5 次 Rematch 压力、五局三胜状态机测试、全新 Win64 打包与冒烟
- [x] M11d-1/2 统一 UI 令牌（SEUiStyle 调色板/字号/焦点金框/主次危按钮/降动效）、主菜单/设置/暂停/确认/比赛结束重构（设置分四区 + 真正生效的 UI 缩放与减少动态效果）
- [x] M11d-3 比赛 HUD 重构：左右对称比分板、阶段徽章、触球圆点、中央回合横幅、帮助条分离；右上角小球场站位图（队色圆点/金色发球/白色受控/轮转提示）
- [x] M11d-4 战术 UI 重排：攻击卡片化（目标区/力度条/弧线档位/绿黄红）、二传 13+1 ScrollBox 分组列表、防守双列（拦网策略/后排防守）
- [x] M11d-5/6 场馆双色吸音板与无品牌赛事文字标识、角色胸背球衣号码、比赛用球黄蓝白多面板（授权插槽 fallback 保留）
- [x] M11e-1 打包版有画面启动闭环：M11e-0 修正后全新 Win64 包在 D3D12（3 次冷启动）/D3D11 真实渲染下完整跑通 QuickMatch+Rematch 流程，TacticalTest PASS，主菜单/比赛/战术/结果截图逐张验收，启动器默认指向验收包（M11d 挂起重打包后不再复现，根因边界见 docs/M11e-report.md）
- [x] M11e-3 角色姿态验收序列：`-PoseSuite` 11 姿态 × 正/侧/背 + Run 相位帧，纯 3D 截图逐张验收（Idle/Block/Dive 构图合格）
- [x] M11e-4 场馆构图美术：记录台实体记分牌、裁判台护栏、坐姿观众（ISM）、替补坐姿、球网细网格
- [x] M11e-5 确定性验收与图形包：57/57 测试、三组 Seed + RematchStress、新 Win64 包与打包冒烟（部分验收点被 M11f 复核纠正，见下）
- [x] M11f-1 战术输入闭环：防守慢动作成对恢复（不再卡 0.3）、半场语义单一化、面板防穿透与滚轮隔离、自由轨迹可调、60/60 自动化测试
- [x] M11f-2 姿态/镜头：举臂姿态符号翻正（UE 左手系 pitch）、Dive/Recover 视觉根贴地、Block 双手高举、第三人称比赛机位（SpringArm 430/俯角 -8°）、MatchOver 场馆机位
- [x] M11f-3 菜单/设置：排球图标 1×1 根因修复（Brush.ImageSize）、金分隔线 180×3、SEFocusableButton 真实键盘焦点金框、设置页 ScrollBox、-UIScale 0.8/1.0/1.4 矩阵
- [x] M11f-4 球与记录台：原创黄蓝白多面板球（运行时面板纹理、无品牌、授权插槽回退）、记录台同侧工作区、实体记分牌文字正向可读（Yaw+90、WorldSize16、TextCenter）、观众朝场心
- [x] M11f-5 可信验收：ShotSuite 首触必须真实接发（删除摆拍）、超时明确缺项并非零退出、-FiveSetTest 生产五局三胜加速验收（26:24/24:26/26:24/24:26/16:14 → 3:2 PASS）
- [ ] 战术 AI 完整化（后排进攻细化、自由人）、外部角色动画模型、网络同步

下一步具体行动见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 玩家下载（解压即玩）

无需安装 Unreal Engine、无需编译源码。直接下载 Windows 版压缩包：

- 发布页：**<https://github.com/T720Dsh/spike-elite/releases>**
- 直链：<https://github.com/T720Dsh/spike-elite/releases/download/v1.0.0/SpikeElite-v1.0.0-win64.zip>（约 283MB）

> 当前 GitHub Release v1.0.0 为早期 M11b 里程碑构建（约 283MB）。**未包含 M11c/M11d/M11e/M11f 的规则、
> 战术、UI 与场馆收口内容**。M11f 轮按仓库约定不自动 push、不发布；最新的本地打包产物位于
> `Dist\Windows\SpikeElite.exe`（约 905MB，不入 Git）。如需面向玩家的新版"解压即玩"包，请在
> [Releases](https://github.com/T720Dsh/spike-elite/releases) 从最新 Dist 产物发布新版本。
> **不要**下载旧的 v1.0.0 283MB 包——它缺少当前已验收的比赛规则、战术系统和美术内容。

> **M11e-1 打包版渲染已通过验收**：M11e-0 现场修正后的全新 Win64 Development 包（`Dist\Windows\SpikeElite.exe`，
> exe 332MB，包总量约 939MB）在真实图形 RHI 下正常启动：D3D12 冷启动 3 次 + D3D11 对照，每次均完成
> QuickMatch → MatchOver → Confirm-cancel → Rematch → 第二场 → 退出（DevVerifyFailures=0）；打包版
> TacticalTest PASS（真实击球 ×2）；主菜单/比赛 HUD/战术瞄准/二传 13+1/MatchOver 截图逐张读取通过。
> M11d 打包启动挂起在本轮重打包后不再复现——最可能为 M11d 打包时的 cook/缓存状态问题（挂起日志卡在
> Slate Freetype 字体面创建前，M11e-0 重打包后字体面正常创建并进入 `Game Engine Initialized`），
> 未完全排除 M11e-0 代码差异；根因边界保留，详见 docs/M11e-report.md。

`Play_SPIKE_ELITE.bat` 默认启动打包版 exe（`Dist\Windows\SpikeElite.exe`）；无包时提示先执行构建脚本，
`--editor` 可切换到 Unreal Editor 游戏模式（开发者用）。

**运行步骤：**
1. 解压 `SpikeElite-v1.0.0-win64.zip` 到任意目录（绿色版，免安装）。
2. 双击 `SpikeElite.exe`（或 `Play_SPIKE_ELITE.bat`）。
3. 主菜单选择开始比赛，按 `E` 发球即可开打。

**系统要求：** Windows 10/11 64 位，显卡支持 DirectX 12（UE5 最低要求），键鼠操作。操作说明见下表。

## 快速开始（给开发者）

引擎版本：**Unreal Engine 5.8**（`.uproject` 的 `EngineAssociation` 为 `5.8`）。

```powershell
# 0) 可选：配置共享文件缓存。共享 DDC 与本地 Zen 数据是两个不同节点。
[Environment]::SetEnvironmentVariable("UE-SharedDataCachePath", "D:\UE_DDC", "User")
[Environment]::SetEnvironmentVariable("UE_SharedDataCachePath", "D:\UE_DDC", "User")
# D:\UE_DDC 目录需存在。实际生效路径须看 LogDerivedDataCache / LogZenServiceInstance。
# 现有 Automation_M11d.log 的本地 Zen data-dir 为 D:\ZenData；配置共享变量
# 不能证明本地 Zen 已迁移到 D:\UE_DDC，也不能保证 UE 不再写 C 盘。
# 不要整体搬移或删除 AppData\Local\UnrealEngine，其中还可能有引擎配置和安装文件。

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
.\Dist\Windows\SpikeElite.exe
```

玩家入口：双击 **`Play_SPIKE_ELITE.bat`**（优先启动打包版 exe，无打包时提示先执行构建脚本）。

### 操作说明

| 操作 | 按键 |
|---|---|
| 移动 | WASD |
| 视角 | 鼠标 |
| 击球 | 鼠标左键（跳跃中为扣球） |
| 发球 | E（真人须按 E；AI 自动发球；`-devauto` 下自动化可代发） |
| 第一/第三人称 | C（兼容 V） |
| 暂停 / 释放鼠标 | Esc |
| 快速对局（开发） | `-QuickMatch`（一局 3 分制，复用正式规则） |

> 发球规则（M11a）：真人玩家必须按 `E` 才能发球，不会在 3 秒后自动发球；
> 机器人发球员会自动发球。只有显式带 `-devauto` 的无人值守自动化运行才会
> 代替真人按键。

## 规则自动化测试

```powershell
# 无地图纯逻辑 + 集成自动化测试（当前 60/60）：
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe .\SpikeElite.uproject `
  -ExecCmds="Automation RunTests SpikeElite.Tests; Quit" -unattended -nosplash -nopause -NullRHI -log
```

```powershell
# 无人值守冒烟（开发机；多次运行可用 -SEED=1/42/4242 复现）：
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe .\SpikeElite.uproject `
  -game -windowed -ResX=1280 -ResY=720 -QuickMatch -devauto -SEED=42 `
  -unattended -nosplash -log -abslog="D:\projects\spike-elite\Saved\Logs\devauto.log"
```

## 路线图速览

```
M0–M4 原型基础 (已完成)      → 场馆、场地线、球网、菜单、6v6 方块人、球运动
M5–M9 规则与反馈 (已完成)    → 规则状态机、三次触球、AI 协作、反馈/结束界面
M10 垂直切片 (已完成)        → 规则测试、QuickMatch、Win64 打包、文档
M11a 稳定收口 (已完成)       → 确认弹窗状态机、devauto 隔离、MatchOver 输入门、性能与发布卫生
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

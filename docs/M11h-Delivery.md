# SPIKE ELITE — M11h 交付记录（豆包主代理，2026-10-03）

> 本文保留豆包 `593d23d` 的阶段交付事实，不代表当前源码。后续现场发现的缺陷、修复、训练/入场/名单 UI 收尾与最终验收见 [M11h Codex 现场收尾](M11h-Codex-Closeout.md)。

> 本轮目标：从演示对打到可玩的排球比赛——球员身份/12 人名单、模式选择、
> 入场与发球员介绍、球队暂停、合法换人、场下教练操控、裁判反馈与赛后统计、
> 真人可玩闭环。所有验证对应同一最终源码（`git status` 干净后为最终提交）。
> **未自行 push。**

## 0. 接收与基线

- 先接收 Codex M11g 现场美术（commit `7d1fa08`）：SEArtGeometry 分段角色、
  裁判塔、屋面/看台、球网硬件、M_ArtSurface、相机机位、战术时机条、打包脚本
  `tools/m11g_package.ps1` / `tools/m11g_verify.ps1`。
- 基线测试：M11g=65 → M11h-1=70 → 2a=70 → 2b=72 → 4=74 → 5=76 → 6=77 →
  3=77 → **8 后 78/78**。
- 磁盘事实：C 盘空闲约 10GB（本地 DDC 仍占用，未清理、未持久改环境）；
  D 盘空闲约 98GB；`Saved/`（D 盘项目内）承载日志与存档。

## 1. 已提交（本地 main，共 10 个，均未 push）

| commit | 内容 |
|---|---|
| `7d1fa08` | M11g 现场修正接收（26 文件 +1159/-151） |
| `40ce4fe` | M11h-1 球员身份与名单权威模型（PlayerId/中文名/号码/角色/有界属性；GameMode FindIdentity、种子化发球误差） |
| `662510c` | M11h-2a 模式选择（快速短局/正式五局/12 人挑战/教练/训练；ModeSelectWidget 纯 C++ UMG） |
| `67b07bc` | M11h-2b 挑战赛（三档原创对手难度、schema 版本存档、胜利推进） |
| `f2fd084` | M11h-4 球队暂停（Timeout 状态机、30s 计时、集结、死球门禁、额度 2/局） |
| `b443ea3` | M11h-5 合法换人（每局 6 次、替补↔首发配对、原子切换、发球槽跟随身份） |
| `948071f` | M11h-6 场下教练操控（CoachPanelWidget、Tab 开关、发球落区/拦网/防守深度/二传偏好/风险） |
| `530efe1` | M11h-3 入场与发球员介绍（ServePresentation 状态、ServeIntroWidget 右半屏卡片、E 跳过、连续同人短条） |
| `dc653d5` | M11h-8 赛后个人统计（FPlayerMatchStats、AttachStatsForRally 纯逻辑、AwardPoint 归因、MatchEnd 统计表） |
| `M11h-8fix`（本轮最终提交） | 打包空指针修复（ServeIntroWidget 树时序）+ FiveSet 模式根因修复 + 最终打包/文档 |

## 2. M11h-8 统计归因（已自动验证）

- `SEVolleyballRules::AttachStatsForRally(Stats, LastTouchType, LastTouchIndex,
  TouchCount, ScoringTeam, ServingTeamBeforePoint, LastTouchTeam)`：
  - 攻击得分 → AttackWins；攻击方失分 → AttackErrors；
  - 发球失误（发球队未得分）→ ServeErrors；发球得分且 0 触球 → ServeAces；
  - Block 触球 → Blocks；Receive/Set 按触球类型计数。
- GameMode：StatsA/StatsB 按场上槽位 0..5 索引；`ResetMatchStats()` 在
  StartMatch（跨局累加）；`RecordTouchStat` 只在 `TryTouchBall` 的 Allowed
  真实触球路径调用（DiveSave 通过 `WasDiveSaveRecorded()` 计 Digs）。
- MatchEndWidget 增加统计表（仅显示有活动球员），数据来自权威 GameMode。
- 测试：`StatsAttribution`（攻击赢/输、发球得分/失误、拦网、一传、二传）。

## 3. 关键缺陷修复（打包前发现并修复）

1. **打包版启动崩溃（EXCEPTION_ACCESS_VIOLATION）**：
   `UServeIntroWidget::SetServer` 在 widget 树构建前被调用（打包版 UMG 树在
   AddToViewport 时才建）。修复：PlayerController 先 `AddToViewport(25)` 再
   `SetServer`，并给 SetServer/NativeTick 加空判。修复后新包 D3D12 图形冒烟
   PASS（主菜单→比赛→MatchOver→Rematch→退出，exit=0、无 Fatal）。
2. **FiveSetTest 假失败**：`FMatchModeConfig::Mode` 默认 QuickMatch 且
   bShortSets=true，导致**任何无菜单直启**（含 `-FiveSetTest`）被 StartMatch
   强制成 1 局 3 分。修复：默认 Mode=None；StartMatch 在 None 分支用
   `-QuickMatch` 命令行 flag；菜单显式模式仍生效。修复后五局验收
   `26:24/24:26/26:24/24:26/16:14 → 3:2 → RESULT=PASS`。

## 4. 本轮验收结果

| 项 | 结果 | 证据 |
|---|---|---|
| Editor 构建 SpikeEliteEditor Win64 Development | PASS | 无 error |
| Game 构建 SpikeElite Win64 Development | PASS | 无 error、无 Unreal5_6 警告 |
| 自动化测试 | **78/78**（0 失败） | `Saved/Logs/M11h_Final_Automation.log` |
| QuickMatch SEED=1 | PASS | `M11h_Quick1.log`（MatchOver+Rematch+quit 0） |
| QuickMatch SEED=42 | PASS | `M11h_Quick42.log` |
| QuickMatch SEED=4242 | PASS | `M11h_Quick4242.log` |
| RematchStress（5 次） | PASS | `M11h_Rematch.log`：court/arena/ball/officials/rotwidget/scoreboard 恒 1、chars 12/12、validActors=34 稳定 |
| TacticalTest | PASS | `M11h_Tactical.log`：real rally → 战术 UMG → 取消路径 → 两次确认 → PASS (failures=0) |
| FiveSetTest（真实五局） | **PASS** | `M11h_FiveSet3.log`：26:24/24:26/26:24/24:26/16:14，3:2，MatchWinsNeeded=3 |
| ArtSuite 截图 | 7 张全部逐张 Read 通过 | `Saved/Screenshots/WindowsEditor/art_*.png` |
| Win64 打包 BuildCookRun | PASS（113s） | `Dist/M11h`（56 文件 911MB） |
| 打包版冒烟（D3D12 图形） | PASS | `M11h_Pkg_Final.log`：MatchOver+Rematch+quit(DevVerifyFailures=0)、无 Fatal |

### 截图逐张验收（`Saved/Screenshots/WindowsEditor/`，全部 Read 读取）

| 图 | 内容 | 判定 |
|---|---|---|
| art_00_actual_player_view | 比分板 TEAM A/SET 1/B、局 A0-0B、轮次 1/6 发球 A、站位 12 人、**发球员介绍卡（#1 林一鸣 TEAM A·主攻·发球）**、底部操作提示 | PASS |
| art_01_arena | 场馆全景：木地板、蓝/橙队球员、球网、裁判台、彩色观众、工业灯具 | PASS |
| art_02_referee_tower | 裁判高台带白色阶梯、简模裁判（白上衣深裤）、网柱 | PASS |
| art_03_score_table | 实体记分牌正面可读：SET 1 / A0:0B / SETS A0:0B / SERVE A | PASS |
| art_04_bench | 替补席蓝衣替补坐姿 + 观众阶梯 | PASS |
| art_05_seated_crowd | 坐姿观众（多色、密集、朝场） | PASS |
| art_06_net_hardware | 网柱红白标志杆、细网、裁判 | PASS |

## 5. 打包路径与说明

- 最终包：`Dist\M11h\Windows\SpikeElite\Binaries\Win64\SpikeElite.exe`
  （包 56 文件 / 911MB；exe 317MB；Development，D3D12 图形验证）。
- 不覆盖旧包 `Dist\M11g`；包产物与截图不入 Git。
- 开发构建：UBT `SpikeEliteEditor Win64 Development`；
  打包：`tools/m11g_package.ps1`（NO_PROXY 防 Zen `[::1]:8558` 走代理）。

## 6. 已知限制 / 未实现（如实标注）

- **未实现**：训练模式（Training 仅路由，无独立训练项目/反馈）；
  入场列队动画（24 人分队列；目前只有发球员介绍卡）；
  裁判动作（一裁手势、记录台暂停/换人额度同步、边裁旗语）未接入权威事件；
  名单编辑/首发预览 UI（数据模型已有，UI 未做）；
  完整换人选择界面（教练面板提供 A/B 快捷 1↔7）；
  自由人规则（角色定义中有 Libero 标签但标注"未启用"，不冒充实现）。
- **NOT RUN**：1920×1080 本机只能输出 1423×889（如实标注，不写 PASS）；
  严格性能 A/B（未做 median/p95 采样，本轮未宣称性能通过）；
  真人纯手动完整对局（本轮交互验收以 devauto + TacticalTest 真实输入链为主，
  非 devauto 真人操作未验）。
- 音效：未引入外部音频；哨声等使用项目内生成提示音。
- 授权边界：比赛用球仍为无品牌黄蓝白占位球，未使用 Mikasa/V200W/FIVB/
  Olympic 商标；ASSET_LICENSE.md 记录。

## 7. git 状态

- 当前 main 相对 origin/main 领先（含本轮全部提交），工作区在最终提交后干净。
- **未 push**（按红线）。发布/Release/zip 未执行（未授权）。

# M11d — 执行恢复指引（Bash 故障期间生成）

> 历史记录：M11d 已提交，本指引和 m11d_git_commits.ps1 不应再次执行。
> 当前继续工作请读取 docs/M11e-Doubao-Prompt.md；本地修正以 git diff 为准。

> 生成时间：2026-10-02 · 状态：代码已全部编写，验证/提交被环境故障阻塞

## 一、Bash 故障诊断

**现象**：本会话中 Bash 工具（及 TaskOutput 等原生本地执行工具）持续返回
`Native execution failed. Fix the condition and retry.`，跨多轮、约 3 小时无恢复。
Read / Write / Edit / Glob / Grep（文件通道）始终正常。

**已尝试**（均无效）：
- 不同命令：`echo ok`、`dir`、`Get-Location`、`$PSVersionTable`
- 不同参数：默认权限、`run_in_background=true`
- 等待 60s / 300s 后重试

**根因判断**：会话的**原生命令执行器（shell runner）进程挂起/故障**，属环境级问题，
不是命令语法或权限问题，**无法从对话内部修复**。恢复途径：重启客户端/会话，
或检查/重启本地 runner 进程。恢复后 Bash 应立即可用。

## 二、M11d 已编写文件（26 个，未编译未提交）

代码 21 个：
- M11d-1: `Source/SpikeElite/Public/UI/SEUiStyle.h`
- M11d-2: `Public/UI/MainMenuWidget.h`、`Private/UI/MainMenuWidget.cpp`、
  `Private/UI/PauseMenuWidget.cpp`、`Private/UI/ConfirmWidget.cpp`、
  `Public/UI/MatchEndWidget.h`、`Private/UI/MatchEndWidget.cpp`、
  `Public/UI/SettingsWidget.h`、`Private/UI/SettingsWidget.cpp`
- M11d-3: `Public/UI/ScoreboardWidget.h`、`Private/UI/ScoreboardWidget.cpp`、
  `Public/UI/RotationWidget.h`、`Private/UI/RotationWidget.cpp`、
  `Public/Volleyball/VolleyballRules.h`、`Public/SpikeEliteGameMode.h`、`SpikeEliteGameMode.cpp`
- M11d-4: `Public/UI/TacticalHUDWidget.h`、`Private/UI/TacticalHUDWidget.cpp`
- M11d-5: `Private/Volleyball/VolleyballArena.cpp`
- M11d-6: `Public/SpikeEliteCharacter.h`、`SpikeEliteCharacter.cpp`、
  `Public/Volleyball/VolleyballBall.h`、`Private/Volleyball/VolleyballBall.cpp`

文档 5 个：`CHANGELOG.md`、`README.md`、`docs/05-roadmap.md`、
`docs/06-art-direction.md`、`Content/Balls/ASSET_LICENSE.md`

**已知自查**：MainMenuWidget Glow/Court/Net 布局 bug 已修复（Offsets）；GameMode
`LastRotationServeTeam` 类型可见；`bJustRotated` 已填充；SecurityToken 无明文；
旧 UI 成员无外部引用。新代码未经编译器验证，可能有编译错误待修复。

## 三、恢复后执行顺序（严格按序）

1. **编译**：
   `& "D:\Epic\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" "D:\Epic\UE_5.8\Engine\Binaries\DotNet\UnrealBuildTool\UnrealBuildTool.dll" SpikeEliteEditor Win64 Development -Project="D:\projects\spike-elite\SpikeElite.uproject" -WaitMutex -log="D:\projects\spike-elite\Saved\Logs\ubt_m11d_editor.log"`
   判据：日志无 `error C` / `error LNK`。修复所有错误后继续。
2. **Game 编译**（同上换 `SpikeElite`；判据无 `Unreal5_6` 警告）。
3. **自动化测试**：`UnrealEditor.exe ... -ExecCmds="Automation RunTests SpikeElite.Tests; Quit" ...`
   判据 55 Success / 0 Fail。
4. **TacticalTest / ShotSuite / 三组 Seed / 打包 / 打包冒烟**：直接运行
   `tools\m11d_build_and_verify.ps1`（含全部步骤与判据）。
5. **分阶段提交**：编译全过后运行 `tools\m11d_git_commits.ps1` 生成 M11d-1…7
   七个独立提交（脚本已按阶段分组 git add + commit）。**不 push**。
6. **截图逐张验收**：ShotSuite 产出 `Saved\Screenshots\WindowsEditor\shot_ss_0X*`，
   必须用 Read 逐张查看最新实例（非旧截图），覆盖 24 项清单（主菜单 720/1080、
   设置、暂停、确认、比赛 HUD、发球 HUD、回合提示、轮转 HUD、攻击瞄准虚线、
   13+1 二传、防守 8 项、DiveActive、DiveSave+Recovery、第一/第二裁判、记录台、
   替补席、场馆全景、看台近景、球网近景、比赛球近景、MatchOver、Rematch 第二场）。
7. **交付报告**：每阶段 commit hash、测试总数、日志/截图/打包路径、已知限制、
   `git status --short --branch`；不 push。

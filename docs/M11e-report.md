# M11e — 打包闭环、真实交互、角色动画与场馆视觉升级：验收报告

状态：M11e-1/2/3/4/5 均已完成并各自独立提交；未 push（用户要求本地提交）。

## 提交链（全部保留既有提交）

| 提交 | 阶段 | 摘要 |
|---|---|---|
| `5583c91` | M11e-0 | 接受 Codex 现场修正（基线 57/57 测试，Editor+Game 构建通过） |
| `2ec8e8b` | M11e-1 | 打包版有画面启动闭环（根因边界保留） |
| `24c9d33` | M11e-2 | 战术 UI 真实操作与空间语义（57/57 回归） |
| `7d69280` | M11e-3 | 角色姿态验收序列（PoseSuite 11 姿态 + Run 相位帧） |
| `67dc65b` | M11e-4 | 场馆构图美术（记录台/裁判台/坐姿观众/球网） |
| （待提交） | M11e-5 | ShotSuite fallback、文档、截图 manifest、最终图形包 |

## 根因与修改摘要

- **M11d 打包挂起**：新旧包日志精确对比，挂起卡在 Slate Freetype 字体面创建前；
  M11e-0 重打包后字体面正常创建并进入 Game Engine Initialized → LoadMap。
  最可能为 M11d 打包时的 cook/缓存状态问题；未完全排除 M11e-0 代码差异
  （角色 CDO 关节/TextRender 类改动）。根因边界保留，`-NoJerseyText` 诊断开关可用。
- **M11e-2 战术 UI**：TickPlanning 直接读 LMB 与 UMG 卡片点击争用 → 加
  `IsPointerOverPanel()`（面板外 LMB/Enter 才确认）；`HandleSetPlayPicked` 不再只改高亮，
  而是经 `ApplySetPlayToIntent` 重建 Intent/预览；弹道 `ApexAboveContact` 相对触球点，
  弧高档位改为 <60/<150/否则高；同侧 Set/Receive 不要求过网。
- **M11e-3 姿态**：PoseSuite 固定主体 + 清速度 + 球推离 + Dev 相机 330cm 轨道绕躯干，
  纯 3D 截图。Dive 低姿态前扑、Block 双臂高举、Idle 球衣号码均可读。
- **M11e-4 场馆**：记录台实体记分牌（面板朝球场、文字 Yaw 180 正向）、裁判台护栏柱、
  坐姿观众（身体+腿+头 ISM）、替补坐姿、球网 8cm 细网格深色线。
- **M11e-5 验收**：ShotSuite `first_receive` 20s 确定性 fallback 修复偶发停滞；
  日志口径 "shot groups"。

## 自动化测试（Editor -NullRHI）

- 总数 57 / 成功 57 / 失败 0（日志 `Saved\Logs\M11e5_automation.log`，`EXIT CODE: 0`）。
- 基线说明：`LogAutomationTest: Error: Condition failed` 为测试内部负路径断言，
  全部测试 Result={Success}。

## 编译结果

- SpikeEliteEditor Win64 Development：通过（多次增量构建）。
- SpikeElite Win64 Development：通过。

## 冒烟与压力

- 三组 Seed（1/42/4242）：QuickMatch → MatchOver → Confirm-cancel → Rematch →
  第二场 MatchOver → 退出，DevVerifyFailures=0（`M11e5_seed{1,42,4242}.log`）。
- RematchStress：5 次审计 PASS（court=1 arena=1 ball=1 officials=1 rotwidget=1
  scoreboard=1 chars(world)=12 validActors=33；`M11e5_rematch.log`）。
- 分辨率：`-ResX/-ResY/-ForceRes/r.SetRes` 在本机 unattended 环境均被桌面分辨率覆盖；
  UI 为 UMG 锚点/DPI 自适应，1707×1067 运行 DevVerifyFailures=0。

## 打包与冒烟

- `BuildCookRun -skipcook -build -stage -pak -archive` 生成新包
  `Saved\Archive\Windows`（48 文件 / 905MB；Paks: SpikeElite-Windows.ucas 119.5MB + .pak 10.2MB）。
  固化 `Dist\Windows`（bootstrap `SpikeElite.exe`）。
- 打包版 seed 42 冒烟：50s，MatchOver → Confirm-cancel → 第二场 MatchOver → 退出，
  DevVerifyFailures=0（`M11e5_pkg_seed42.log`）。
- 日志错误：仅引擎环境噪声（aqProf/Vtune/WinPixGpuCapturer dll 缺失 = profiler/PIX
  可选组件未安装；SoundConcurrency 引擎默认提示），无项目自身 Fatal/Ensure/Missing Package。

## 截图路径

- 姿态套件（Editor，最新帧 0000x）：`Saved\Screenshots\WindowsEditor\shot_pose_*`。
- 场馆/裁判/记录台（Editor）：`Saved\Screenshots\WindowsEditor\shot_ss_08_*`。
- 打包版冒烟：`Dist\Windows\SpikeElite\Saved\Screenshots\Windows\shot_qm_*00000`。
- 截图 manifest：`docs/screenshots/M11e5_manifest.md`。

## 已知限制

- 分辨率强制切换在本机 unattended 模式不可用；三分辨率布局验收依赖锚点自适应与
  既往打包版截图（1280×720 轮转 HUD 完整）。
- 角色为程序化分段关节风格化（无骨骼资产），非写实人物。
- 比赛球为原创无品牌黄蓝白占位球，未声称 Mikasa/V200W 官方资产。
- 换人系统、自由人、网络同步未实现（均为后续项）。

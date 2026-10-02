# M11f-5 验收 manifest（2026-10-02 构建）

构建提交：`a4cdbdf`（M11f-5 代码，工作区后续文档提交除外）+ 打包前工作区文档改动
引擎：UE 5.8（`D:\Epic\UE_5.8`）；分支 main；构建类型 Development；RHI 见各条目。

## 包

| 项 | 值 |
|---|---|
| 路径 | `D:\projects\spike-elite\Dist\Windows\SpikeElite.exe`（bootstrap） |
| 完整包 | `Dist\Windows\SpikeElite\`（89 文件 / 936MB） |
| exe | 317MB，时间 2026-10-03 00:11（本轮全新 BuildCookRun，非复用旧包） |
| 打包命令 | RunUAT BuildCookRun -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive -archivedirectory=Dist |
| 日志 | `Saved\Logs\M11f5_package.log`（BUILD/COOK/STAGE/ARCHIVE SUCCESSFUL） |

## 日志（Saved\Logs，均本轮）

| 文件 | 内容 | 结果 |
|---|---|---|
| M11f5_fiveset2.log | Editor -FiveSetTest（NullRHI） | 五局 26:24/24:26/26:24/24:26/16:14 → 3:2 RESULT=PASS，exit 0 |
| M11f5_perf.log | Editor QuickMatch Seed42（真实图形窗口） | 完整流程；Perf 见下 |
| M11f5_automation.log | SpikeElite.Tests | 60 成功 / 0 失败 / TEST COMPLETE EXIT CODE 0 |
| M11f5_rematchstress3.log | RematchStress5（QuickMatch+FastFlow） | 5 次审计 PASS，validActors=33 稳定，quit |
| M11f5_pkg_d3d12_seed42.log | 打包版 D3D12 QuickMatch Seed42 | MatchOver→confirm-cancel→Rematch→第二场→exit 0 |
| M11f5_pkg_d3d11_seed1.log | 打包版 D3D11 QuickMatch Seed1 | 同上 PASS |
| M11f5_pkg_seed4242.log | 打包版 QuickMatch Seed4242 | 同上 PASS，DevVerifyFailures=0 |
| M11f5_pkg_tactical.log | 打包版 -TacticalTest | 真实击球×2，PASS（failures=0） |
| M11f5_pkg_fiveset.log | 打包版 -FiveSetTest（真实图形） | 五局 RESULT=PASS，exit 0 |

全日志扫描：无项目自身 Fatal / Ensure / Missing Package / Accessed None / LogError / TacticalShot failed
（引擎环境噪声——aqProf/Vtune/PIX dll 缺失、音频设备切换、EOS 初始化——单独归类，不计项目错误）。

## 截图（打包版，1280×720，Dist\Windows\SpikeElite\Saved\Screenshots\Windows\）

| 文件 | 内容 | 验收 |
|---|---|---|
| shot_qm_01_menu00004.png | 主菜单（标题/副标题/三按钮/角标 M11f） | PASS |
| shot_qm_05_rally00004.png | 正常比赛 HUD（比分/阶段徽章/轮次面板/操作帮助） | PASS |
| shot_qm_02_matchover00004.png | MatchOver 结果页（B 胜/局分/三按钮） | PASS |
| shot_qm_03_confirm_matchover00004.png | 结果页确认框（确认/取消/回主菜单/退出） | PASS |
| shot_qm_04_rematch_over00004.png | Rematch 第二场结果页（A3:1B 独立计分，无重复 UI） | PASS |
| shot_tac_04_plan200001.png | 攻击瞄准 UMG（目标区/力度/弧线/时机窗口提示） | PASS |
| shot_tac_07_plan300001.png | 二传 13+1 面板（分组/选中高亮/战术参数/操作提示） | PASS |
| shot_tac_08_armed200001.png | Armed 战术俯视镜头（球场/球/球员入画） | 部分（armed 阶段面板收起，时机条未显性捕获；plan 阶段提示已验） |
| shot_tac_06_impact00001.png | 击球瞬间俯视镜头 | 部分（同上；击球真实执行由 TacticalTest 日志证明） |

## 性能（1280×720 窗口化，GameMode Perf 心跳每 5s）

| 采样 | 条件 | 数据 |
|---|---|---|
| M11f-5（本轮） | QuickMatch Seed42，比赛/结算混合 | fps 147–180，frame 5.6–6.8ms，actors 34–45 |
| M11f-4（对照） | ShotSuite 同分辨率窗口 | fps 125–176，frame 5.7–8.0ms，actors 35 |

结论：同条件无性能倒退，Actor 数稳定（audit validActors=33）；帧时间受窗口化/结算阶段 UI 影响。

## 未完成/限制

- 1920×1080 请求在本机实际输出 1423×889（桌面可用区），只记 NOT RUN（M11f-3 已标注）。
- Armed/Impact 阶段战术时机条未在截图中显性捕获（plan 阶段已验收，TacticalTest 日志证明 Armed 与真实击球）。
- GitHub Release zip 未发布（M11f 约定不 push、未授权发布）。

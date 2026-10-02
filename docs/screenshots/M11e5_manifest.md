# M11e-5 截图 manifest（2026-10-02）

- 构建：HEAD 67dc65b（M11e-4）→ 本文档所在提交（M11e-5）
- 分辨率：1707×1067（本机 unattended 强制桌面分辨率）；UI 缩放 1.0
- 所有截图来自实际运行（Editor -game 或打包版），非静态视口

## 姿态套件（Editor，`Saved\Screenshots\WindowsEditor\`，最新帧 00002/00003/00004 系列）

| 验收项 | 文件（最新帧） | 结果 |
|---|---|---|
| Idle 正面（球衣 1 号） | `shot_pose_idle_front00004.png` | PASS（角色居中、号码可读、场馆背景） |
| Block 正面（双臂高举） | `shot_pose_block_front00004.png` | PASS（全身入画、双手高举、队友/网/观众可见） |
| Dive 侧面（低姿态前扑） | `shot_pose_dive_side00003.png` | PASS（前扑伸臂清晰、无球遮挡） |
| Run 相位帧 | `shot_pose_run_frame{0..7}00004.png` | PASS（8 帧运动相位；侧视构图受限但姿态可见） |

## 场馆/裁判/记录台（Editor，`Saved\Screenshots\WindowsEditor\`）

| 验收项 | 文件 | 结果 |
|---|---|---|
| 记录台实体记分牌 | `shot_ss_08_scorer00016.png` | PASS（"SET 1 / A2 / Serve A" 正向可读、不被记录员挡） |
| 第二裁判机位 | `shot_ss_08_ref200016.png` | PASS（裁判/球网/看台构图） |
| 主菜单 | `shot_ss_00_menu00029.png` | PASS（标题/副标题/三按钮/版本角标） |

## 打包版冒烟（`Dist\Windows\SpikeElite\Saved\Screenshots\Windows\`，00000 系列）

| 验收项 | 文件 | 结果 |
|---|---|---|
| 主菜单 | `shot_qm_01_menu00000.png` | PASS（有画面、UI 完整、按钮层级正确） |
| MatchOver | `shot_qm_02_matchover00000.png` | PASS |
| Confirm（MatchOver 中） | `shot_qm_03_confirm_matchover00000.png` | PASS |
| Rematch 后第二场 | `shot_qm_04_rematch_over00000.png` | PASS |
| 比赛 HUD | `shot_qm_05_rally00000.png` | PASS |

## 验收规则

- 全部截图由图像读取逐张人工核对（内容/构图/UI 完整性），非仅日志判断。
- 每轮新运行产生递增帧号（00000→00001…），本 manifest 记录最新帧。

# M11d 现场复核与修正

复核日期：2026-10-02。基础提交 b7c2a89；本轮修正尚未提交/推送，以工作区 diff 为准。

## 结论

M11d 实现了菜单/设置分区、比分板、轮转小球场、战术卡片、号码和球图案等功能。
基础提交、55 项规则测试和若干成功运行日志确实存在。不能据此认定完整美术/UI 验收或
Win64 图形交付已完成。打包挂起的根因仍未定位，NullRHI 只证明无渲染逻辑路径可运行。

## 已就地修正

| 问题 | 原因及修正 |
| --- | --- |
| 下肢/手脚微小、漂浮、与躯干脱离 | 肘膝关节挂在已缩放网格下导致连乘缩放；改为未缩放关节链。髋挂到 TorsoJoint 的实际底面，重新设定腿长，使倾身姿态保持连接。 |
| 轮转图前后排颠倒，白底白字、标题在卡片外 | 前排行改为靠网，号码深色，增大格子，标题放回卡片；只标真实发球队 P1，金色独立标记兼容受控球员；轮转提示 2 秒后消失。 |
| 卡片有按钮外观但鼠标无回调 | 增加携带索引的 UTacticalChoiceButton，二传/防守接入已有生产回调；ScrollBox 限定可用高度并滚动到键盘选中项。 |
| UI 缩放不保存，降动效未应用也立即生效 | 改用游戏 UMG 缩放，辅助设置写入 GameUserSettings 并在启动加载；返回丢弃待应用的降动效值。 |
| 战术区域把纵向当左右 | 按 X 距球网、Y 横向及玩家 TeamSide 生成区域名。 |
| 验证脚本可能跑错场景/读旧日志/把挂起判通过 | 去掉 TacticalTest 中的 ShotSuite；独立日志、新包目录、等待实际进程、超时、退出码及完成标记。已验证日志检查器拒绝原不完整包日志。 |
| 双击启动器优先进入已知挂起包 | 有引擎时默认 Editor 游戏模式；显式 --packaged 保留包诊断入口。 |
| 根因与缓存位置文档过度断言 | 文档改为待定位；区分共享 D:\UE_DDC 与日志中的本地 Zen D:\ZenData，去掉整体搬移 UnrealEngine 目录的建议。 |

## 本轮验证证据

- Editor 编译：Saved/Logs/M11d_review_editor_final.log，Result: Succeeded。
- Game 编译：Saved/Logs/M11d_review_game_final.log，Result: Succeeded。
- 自动化：Saved/Logs/M11d_review_57tests.log，57 Success / 0 Fail，TEST COMPLETE EXIT CODE: 0。
- 新增回归：LimbAttachmentAndScale、TacticalButtonClickIndex。后者验证点击事件分派索引和去重，尚不代表完整鼠标操作链已人工验收。
- 实际渲染 TacticalTest：Saved/Logs/M11d_review_tactical_final.log，PASS (failures=0)。
- 实际渲染 720p ShotSuite：Saved/Logs/M11d_review_shots_final.log，完整采集并退出，无该次超时；这不证明已消除偶发停滞。
- 验证脚本 PowerShell 语法通过；日志检查器接受完成自动化日志并拒绝原 M11d_pkg_seed42.log。未执行新脚本的完整打包链。
- git diff --check 通过。

本轮最新 ShotSuite 实机截图（Saved/Screenshots/WindowsEditor）：

| 验收项 | 文件 |
| --- | --- |
| 发球员、手臂、腿与轮转 | shot_ss_01_server00022.png |
| 接发球 | shot_ss_02_first_receive00024.png |
| 扑救姿态（肢体连接已恢复，尚不代表真实倒地效果完成） | shot_ss_03_dive_active00020.png |
| 救球后姿态 | shot_ss_04_dive_save00019.png |
| 防守 UI | shot_ss_05_defense00021.png |
| 攻击场景 | shot_ss_06_attacker_runup00022.png |
| 球 | shot_ss_07_ball00019.png |
| 裁判/记录台 | shot_ss_08_ref100015.png / shot_ss_08_ref200015.png / shot_ss_08_scorer00015.png |
| 结束页 | shot_ss_09_matchover00020.png |

## 交给下一轮的问题

打包图形启动；卡片点击/滚动与世界确认输入冲突；二传最后一项可见性与真实操作；弧高/风险的语义；
大 UI 缩放的设置页边界、重启与放弃设置验收；实际键盘焦点样式；人物比例、倒地/恢复动作和取景；
记录台记分牌遮挡与空间布置；看台/坐姿观众、墙顶/灯光、球网遮挡、比赛球面板；
主菜单图标/分隔线/背景；ShotSuite 可重复性、截图 manifest、真实性能采样和五局结果页。

完整下一轮 Prompt：docs/M11e-Doubao-Prompt.md。

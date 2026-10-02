# M11e 独立验收（Codex，2026-10-02）

## 结论

**部分通过，不接受「M11e 全部完成」的结论。** 打包版图形启动已恢复，原有现场修正已保留，当前 57 项测试通过；但人物/镜头、战术输入、记录台、菜单和验收证据仍有明确缺陷。下一轮应先闭环这些问题，而不是扩展换人、自由人或联网。

验收起点：`f4bc270`，`main...origin/main [ahead 29]`，原工作区干净。本次没有修改游戏源码、提交或 push，只新增验收和下一轮 Prompt 文档。运行检查产生的日志/截图在 gitignored 的 Saved/Dist 目录。

## 本次亲自执行的验证

| 检查 | 结果 | 新证据 |
|---|---|---|
| Editor Development 构建检查 | Succeeded，target up to date | `Saved/Logs/Codex_M11e_review_build_editor.log` |
| Game Development 构建 | Succeeded，28.39s | `Saved/Logs/Codex_M11e_review_build_game.log` |
| 当前自动化测试 | Success 57 / Failed 0，TEST COMPLETE EXIT CODE 0 | `Saved/Logs/Codex_M11e_review_automation.log` |
| 当前 Dist 包 D3D12，Seed42 | 主菜单 → MatchOver → 确认取消 → Rematch → 第二场结束 → 正常退出，DevVerifyFailures=0 | `Saved/Logs/Codex_M11e_review_package.log` |
| 当前 Dist 包 D3D11，Seed1 | 同上，正常退出，DevVerifyFailures=0 | `Saved/Logs/Codex_M11e_review_package900.log` |
| 打包版 TacticalTest | 4 条 DEV VERIFY PASS，实际执行两次击球，正常退出 | `Saved/Logs/Codex_M11e_review_tactical1080.log` |
| 当前打包版 ShotSuite，Seed1 | 输出 all 9 shot groups，正常退出；但触发强制 Receive 兜底，真实首触项不通过 | `Saved/Logs/Codex_M11e_review_shotsuite.log` |
| 实际截图像素尺寸 | 1280×720、1600×900 成功；请求 1920×1080 的战术测试实际为 1423×889 | 直接读取 PNG Width/Height，不使用命令行请求值代替 |

重要边界：TacticalTest 直接调用 `DevTacticalStep` / `HandleSetPlayPicked`，因此证明共享逻辑执行，但**不证明窗口化鼠标命中、Slate 点击分派、滚轮隔离或全部 14+8 项真实操作**。本次未进行真人鼠标/键盘全选项操作；UI 缩放 0.8/1.4 和设置重启持久化仍需补验收。

新打包截图目录：`Dist/Windows/SpikeElite/Saved/Screenshots/Windows/`。

- `shot_qm_01_menu00001.png`、`shot_qm_05_rally00001.png`：1280×720，D3D12。
- `shot_qm_01_menu00002.png`、`shot_qm_05_rally00002.png`：1600×900，D3D11。
- `shot_tac_04_plan200000.png`、`shot_tac_07_plan300000.png`：1423×889，D3D12。

打包启动问题本次没有复现，可接受「当前包已恢复有画面运行」。不能据此确认历史挂起必然由 cook/DDC 引起；根因仍未建立因果证据。

## 主要问题与证据

### P1：防守慢动作退出逻辑有遗漏（源码确认，需集成复现）

`TacticalContactComponent.cpp:486` 进入防守将 TimeDilation 设为 0.3；`RestoreWorldState():454` 仅在 `bWorldFrozen` 为真或当前 dilation ≤0.2 时恢复。防守入口没有置 `bWorldFrozen`，所以正常确认、取消、超时路径可能保留 0.3。现有 TacticalTest 未完整覆盖这个状态。

同文件 `:124` 判「球在对方半场」使用 A=X<0 / B=X>0，而 GameMode 设置 A 的 TeamSide=+1、B=-1，服务位置 A=+1200/B=-1200。防守触发半场条件方向相反，可能错过真正的对方进攻、在过网后才触发。必须通过生产入口的集成案例检验，不只强制打开面板。

### P1：点击防穿透仍有坐标问题，滚轮仍争用

`TacticalHUDWidget.cpp:260` 将 `APlayerController::GetMousePosition` 的 viewport 坐标直接交给 `FGeometry::IsUnderLocation` 的 absolute 坐标。已核对本机引擎源码，两者不是可直接混用的坐标空间，窗口移位/DPI 下尤其不可靠。当前防穿透修正只能算部分完成。

`TacticalContactComponent.cpp:298` 无条件读滚轮修改 Power，没有排除面板/ScrollBox 区域；列表滚动与力度调节仍未隔离。

自由轨迹 id14 仍符合 `bSetTactics`（`:310`），Q/E 被列表切换占用，不能按注释那样进入手动弧线控制。卡片回调也缺少对当前战术状态的校验。

### P1：正常比赛机位遮挡严重

新打包图 `shot_qm_05_rally00001.png` 和 `00002.png` 主要是玩家躯干与地面，球、球网和目标区域不可见；此前报告中标 PASS 的 `00000.png` 也一样。此项不能按「HUD完整」判普通游戏画面通过。

证据包含自动验收脚本的机位/控制状态，不能直接推断所有真人视角都如此；但默认机位、碰撞探针、初始 ControlRotation 和自动流程均需检查。结果页仍沿用玩家/脚本机位，未实现要求的稳定场馆背景。

### P1/P2：角色姿态与落地未达标

已亲自读取 manifest 指定的图片：

- `shot_pose_dive_side00003.png`：人物全身水平悬空，身体/脚与地面明显分离，不是倒地救球。
- `shot_pose_recover_side00004.png`：恢复姿态脚仍悬空。
- `shot_pose_block_front00004.png`：手臂向前/斜伸，没有达到双手过头顶的拦网姿态。
- `shot_pose_set_side00004.png`：构图底部截去脚，不能作为全身比例/地面关系通过证据。

M11e-3 commit 主要增加 PoseSuite/开发姿态覆盖接口；没有完成要求的角色轮廓、贴地扑救与恢复改造。`ApplyPose` 只旋转关节，没有按低姿态调整视觉根/足底约束。PoseSuite 的强制姿态是造景工具，不等于正常动作状态机验收。

### P2：记录台仍不合格，工作区布局未完成

`shot_ss_08_scorer00016.png` 中文字位于面板上方/旁边，双方比分与局数不完整可读。源码显示 `ScoreboardPanel` 尺寸 `(1.05,0.05,0.85)`（薄 Y），而文字朝 -X，文本面与板面不共面；文字原点也在板中心，没有向外偏移。需要统一局部朝向、底板尺寸、文字锚点和字号。

记录台仍在 A 端线后 `X=1750`，两队替补席在 `Y=±1150` 两侧；上一轮要求的同侧赛事工作区未实现。第二裁判仍是无四肢的方块，坐姿观众也主要仍读成彩色立柱，没有可辨认座椅。

### P2：主菜单图标/金色分隔线尺寸仍错误

本次两个分辨率的新主菜单图中排球图标偏小，分隔线仍是金色方块，版本仍写 M11d。

定位到 `MainMenuWidget.cpp:154/172` 在 Slate widget 建立前调用 `UImage::SetDesiredSizeOverride`。本机 UE5.8 的 `Image.cpp:122` 只在 `MyImage.IsValid()` 时设置，不保留提前调用的值。应使用 USizeBox / 明确 Slot 约束或可持久化 Brush.ImageSize，并回归实际绘制尺寸。

设置仍是居中 AutoSize 的完整 VerticalBox，没有 ScrollBox；大 UI 缩放的控件可达性未经验证。SEUiStyle 声称金色键盘焦点边框，但 ButtonStyle 只有 normal/hover/pressed/disabled；选中战术行高亮不能代替 Tab 焦点。

### P2：球仍是环带，不是面板分割

`shot_ss_07_ball00020.png` 仍是黄球加蓝白平行环带，接缝细碎闪烁。`BandMesh2` 对圆柱使用 Yaw90°不会把圆柱轴从 Z 转为横向，因此所谓白色经线没有形成真正经线。应做原创面板材质/网格，不需购买或冒充 Mikasa 模型。

### P1：验收工具把造景兜底当成真实接发证据

本次运行 `Codex_M11e_review_shotsuite.log` 实际触发 `DEV SHOT SUITE: first-receive fallback (team=2)`。源码在超时后直接给首名球员强制 Receive 姿态并将该项 Mark 完成，没有验证真实首触，也没有对应清除该强制姿态。

这只能证明摆出接球姿态，不能证明成功发球后的首触，甚至会污染后续角色动作。应将姿态摆拍与生产事件验收分开；缺真实事件就明确 FAIL，不得以假事件补齐。

本次最新打包图也复现视觉问题：`shot_ss_08_scorer00000.png`文字在板外；`shot_ss_03_dive_active00000.png`人物倾斜悬空；`shot_ss_05_defense00000.png`躯干遮住主要球场。`shot_ss_02_first_receive00000.png`是上述fallback拍摄，不应标真实首触通过。

此外 ShotSuite 超时仍用 Log 级别、普通 quit；不能只靠退出码0判断通过。

## 未闭环的证据

- 原始 1920×1080 和 UI 0.8/1.0/1.4 的完整布局矩阵。
- 14 个二传、8 个防守的真实鼠标/键盘操作及世界输入隔离。
- 生产 GameMode 五局三胜多局流程。现有 `MatchFlowFiveSets` 只是纯规则/计数测试，QuickMatch 仍是一局3分。
- 同条件性能采样的平均/p95帧时、DrawCall与ISM计数；现有 `[Perf]` 心跳是单点值。
- 新构建独立截图目录和完整 manifest。目前 manifest 混用 M11e-3/4 与最终包，报告提交表仍留 M11e-5「待提交」，遗漏/误判项未标 FAIL。

下一轮详细执行要求见 `docs/M11f-Doubao-Prompt.md`。

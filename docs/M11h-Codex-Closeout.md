# M11h 现场验收与收尾（2026-10-03）

本次基线为豆包的 `593d23d`，本地 main 比 origin/main 超前 45 个提交，初始工作区干净。用户授权检查、修复、提交并 push；不创建 Release、不上传打包产物。

## 发现并修复

- B 队角色旧整数 ID 为 6–11，却被当作 0–5 的场上槽位查询：改为持久 `RosterPlayerId`，两队都通过注册身份查询。
- 轮转只移动角色数组而未移动权威名单：角色与 OnCourtLineup 同步轮转，HUD 和换人不再混用两个顺序。
- 个人统计按场上槽位存储，轮转和换人会串名，回合结算还重复增加触球：改为每队 12 个注册身份槽，触球与结算分开记账；发球次数、拦网接触补入实际事件。
- 换人无法合法换回首发、允许哨响后申请、缺少本局退出/回归限制：新增每局账本，首次换人建立配对；同队两个独立请求之间需完成一回合。暂停期间允许换人，训练不消耗正式管理额度。
- 下一局恢复首发却未重排替补身份：双方场上六人和候补六人同时重建，运行审计检查 24 个唯一身份。
- 下一局的轮转提示同时重置到 1/6，避免已恢复首发却显示上局轮次。
- 教练模式仍由人操控一名场上角色，且可控制 B 队管理：现在 12 名场上角色均 AI，玩家使用场下机位、自由鼠标，只管理 A 队。Tab/Esc 恢复输入；切换球员时刷新战术组件的 Pawn 缓存。
- 模式按钮向 UButton 添加两个子节点，导致标题被覆盖：改为标题/说明容器；模式、教练、战术、赛后统计可滚动。
- 程序化哨声沿用已耗尽的 PCM 队列，且没有声源结束门：改为每次新建有限队列、复用唯一音频组件并定时停止。
- 发球介绍存在文字重叠、重复挂载和生命周期问题：相对锚点半屏卡片，整组滑入/滑出，短卡也保留号码，自动退场、暂停冻结、减少动态效果支持。
- 换人确认文案与赛前保存混用、校验提示藏在长名单底部：分别显示“确认换人”和“保存并开始”，提示与确认按钮固定；赛后补发球次数及拦网得分，仅发过球的人也显示。

## 补齐玩法

- 赛前可选择 A/B 队的六个首发槽位，交换或替换球员；确认后保存 `Saved/LineupSave.ini`，schema=1。取消不写盘。不是任意增删注册球员/姓名编辑器。
- 比赛中完整候补/回归名单选择：先选场上槽位，再选球员，确认时再次走 GameMode 规则校验；选择期间使用系统暂停，不额外消耗球队暂停。
- 每队 12 名真实注册球员（6 场上 + 6 替补）；原来十个无身份装饰替补不再显示；球衣号码随权威名单变化。增加场下教练形象。
- 24 人程序化入场走动、两队首发字幕、12 秒流程和 E 跳过，入场阶段禁止击球/发球；相机在结束/取消时销毁。
- 球队暂停双方走向场下集合，30 秒倒计时、剩余额度显示，Esc 系统暂停冻结计时；恢复时回到站位。
- 三种可重复训练：发球落点（鼠标调方向/深度、同源虚线预览）、接发到位（真实弹道喂球）、二传配攻（接球后的二传喂球，队友完成攻击）。目标区、成功/失败反馈和 R 重试；不计正式比分、比赛统计或挑战存档。
- 第一/第二裁判基础发球/得分方向/暂停动作提示；记录台同步每队暂停与换人剩余次数。动作是简化的程序化视觉反馈，不能称为完整正式裁判手势体系。

## 验收方式

`tools/m11g_verify.ps1` 新增 Closeout、Perf；`tools/m11g_package.ps1` 可以指定独立产物目录，不覆盖旧版本。

Closeout 使用显式状态夹具检查管理规则和 UI。接发训练使用实际喂球、实际触球入口及自然飞行落地，未把球瞬移到目标区；另有直接调用落地入口的无触球失败夹具。它不是真人完整对局。

## 最终源码验证结果

对应代码提交 `917c183`（最后仅新增验收脚本的 Width/Height 参数；运行时源码、Editor DLL 和 Game exe 未再修改）。以下均非旧版本日志拼接。

| 验收项 | 结果 | 本机证据（Saved/Logs/，不入 Git） |
|---|---|---|
| Editor + Game Win64 Development / Cook / Stage / Archive | PASS；BuildCookRun 166.49s；Cook 0 error / 0 warning | `Codex_M11h_close_package_ship.log` |
| 项目自动化测试 | **81/81，0 失败** | `Codex_M11g_m11h_ship_Automation.log` |
| Editor 收尾规则/名单/入场/暂停/教练/训练 | PASS，failures=0，11 张截图 | `Codex_M11g_m11h_ship_Closeout.log` |
| Editor 五局生产计分与下一局流程（加速驱动） | PASS；26:24 / 24:26 / 26:24 / 24:26 / 16:14 → 3:2 | `Codex_M11g_m11h_ship_FiveSet.log` |
| Editor 战术真实回合与 UMG 驱动 | PASS，failures=0 | `Codex_M11g_m11h_ship_Tactical.log` |
| Editor 连续重赛 ×5 | PASS，24 个身份有效，有效 Actor=46，核心对象各 1 | `Codex_M11g_m11h_ship_Rematch.log` |
| Editor 场馆美术机位 | PASS，7 张重新拍摄并逐张查看；仅代表原型结构存在/可读 | `Codex_M11g_m11h_ship_Art.log` |
| 独立 exe D3D12 三组 Seed 1/42/4242 | PASS，各自 MatchOver → Rematch → 第二场 → exit 0 | `Codex_M11g_m11h_pack_ship_Quick{1,42,4242}.log` |
| 独立 exe 收尾、五局、战术、重赛 ×5 | 全部 PASS；与 Editor 同样检查 | `Codex_M11g_m11h_pack_ship_{Closeout,FiveSet,Tactical,Rematch}.log` |
| 独立 exe 720p 帧耗时 | 3754 样本，中位数 **11.993ms**，p95 **12.584ms** | `Codex_M11g_m11h_pack_ship_Perf.log` |
| 独立 exe 实际 1920×1080 | PASS，11 张 PNG 实际尺寸均为 1920×1080 | `Codex_M11g_m11h_1080_actual_Closeout.log` |
| 独立 exe 720p UI 1.4 + ReducedMotion | 运行 PASS；名单固定按钮、介绍卡和教练面板截图无关键裁切，长文字会换行/列表需滚动 | `Codex_M11g_m11h_scale14_Closeout.log` |

81 项包括新增 SubstitutionLifecycle / PracticeBallisticFeed / PracticeGoalIsolation。收尾中接发成功是自然弹道落地；二传配攻目前验证启动喂球及目标规则，不冒充真人完成配攻。

帧耗时环境：Windows 11、i7-12700H、NVIDIA GeForce RTX 3060 Laptop GPU（D3D12 adapter 0），项目当前设置、1280×720、教练全 AI 比赛机位；预热 10s 后采样 45s，VSync=0、MaxFPS=0。这是 frame wall time，不是 isolated GPU time，没有同条件旧版本 A/B，不宣称跨硬件保证或整场压力测试。

上述最终运行日志没有 Fatal/Assertion/Ensure、项目所属日志 Error 或 DevVerify FAIL。自动化启动日志含 UE 初始化阶段的 `LogAutomationTest: Error: Condition failed`（项目测试执行之前），另有驱动 TSR/编辑器 Python 重名等引擎警告，因此**不宣称日志全文零 Error/Warning**；项目 81 项结果全部 Success。

### 打包与截图位置

- 默认启动：根目录 `Play_SPIKE_ELITE.bat`；优先 `Dist/M11h-Codex/Windows/SpikeElite.exe`。
- 包：48 个交付文件、953,203,724 bytes（约 909 MiB，不包含运行生成 Saved/）；Development，不是 Shipping。
- 内层 exe：`Dist/M11h-Codex/Windows/SpikeElite/Binaries/Win64/SpikeElite.exe`。
- SHA256：`74CFD79402C09BFE9576F439CE25426E8FADF47111ADFB79CF2FA8E0BB7EAA9E`，与 `Binaries/Win64/SpikeElite.exe` 一致。
- Editor：`Saved/Screenshots/WindowsEditor/`，最终 Closeout `close_*00003.png`、Art `art_*00002.png`。
- 独立 exe：`Dist/M11h-Codex/Windows/SpikeElite/Saved/Screenshots/Windows/`。720p `close_*00000.png`；UI 1.4 `close_*00002.png`；实际 1080p `close_*00003.png`。按 PNG 尺寸与写入时间区分，不能只看名称。
- 场馆/塔台/记录台（SET/SERVE/TIMEOUT/SUB）/真实候补 7–12 号码/球网硬件已查看；人物仍为简化程序化模型，机位 PASS 不是商业美术评级。

### 验收过程中的失败/偏差（保留日志，不隐瞒）

- 一次中途打包与 Editor 重赛并行，DLL 被运行进程占用，导致 LNK1104；停止本任务启动的测试进程，改为顺序构建/运行后成功。不是将该次失败改写为 PASS。
- 第一次 `m11h_1080` 运行追加了重复 ResX/ResY，实际仍为 1280×720，**不计 1080p 验收**。脚本增加独立 Width/Height，重跑 `m11h_1080_actual` 并读取 11 张 PNG 元数据确认实际 1920×1080。
- 中途旧源码的 `m11h_final` / `m11h_verified` 结果不替代上述 `m11h_ship` 最终证据。
- 没有清理用户 C 盘文件或更改全局 DDC/代理/驱动设置；打包代理调整仅限其进程并在 finally 恢复。

### 复现命令

```powershell
./tools/m11g_package.ps1 -ArchiveDirectory 'D:\projects\spike-elite\Dist\M11h-Codex'
./tools/m11g_verify.ps1 -Suites Automation,Closeout,FiveSet,Tactical,Rematch,Art -Label m11h_ship
./tools/m11g_verify.ps1 -Packaged -Executable 'D:\projects\spike-elite\Dist\M11h-Codex\Windows\SpikeElite.exe' -Suites Quick1,Quick42,Quick4242,Closeout,FiveSet,Tactical,Rematch,Perf -Label m11h_pack_ship
./tools/m11g_verify.ps1 -Packaged -Executable 'D:\projects\spike-elite\Dist\M11h-Codex\Windows\SpikeElite.exe' -Suites Closeout -Label m11h_1080_actual -Width 1920 -Height 1080 -AdditionalArguments '-ForceRes'
./tools/m11g_verify.ps1 -Packaged -Executable 'D:\projects\spike-elite\Dist\M11h-Codex\Windows\SpikeElite.exe' -Suites Closeout -Label m11h_scale14 -AdditionalArguments '-UIScale=1.4 -ReducedMotion'
```

请勿同时运行多个图形测试或在它们运行时链接 Editor DLL。脚本失败/超时明确失败，不自动重跑取最好结果。所有包、日志、截图、名单存档均在 gitignore 路径；不创建 Release，不上传二进制，工作区无关 PDF 不纳入本轮提交。

## 保留的边界

- 自由人、完整触网/过网/位置犯规、批量换人请求、换人走入场动作、整套标准裁判手势仍未实现，不把简化排球规则标为完整 FIVB。
- 挑战三档中部分属性尚未接入所有实际决策；移动速度与发球精度有实际效果，其余不得宣称完整球员成长系统。
- 程序化曲面模型仍是风格化原型，不是 FIFA/NBA 商业写实、骨骼动捕或 AAA 美术。Mikasa 资产未授权，继续使用原创球。
- 真人完全手动完整五局与多硬件长期稳定性验收不在自动化 PASS 中。
- 帧时间采样报告仅为当前机器/分辨率下的 frame wall time，不是独立 GPU 时长或跨设备性能保证。
- 720p 默认机位下发球训练的白色点线对比度偏弱，截图不能清晰证明整条预览；同源速度已接入，但该项不冒充完成视觉验收。

## 后续人工验收与改进顺序

1. 真人关闭 devauto，完成一次“选首发 → 入场 → 多回合 → 球队暂停 → 换入/回归 → 新局 → 赛后 → 重赛”，检查鼠标释放与每个按键的实际体验。
2. 三项训练分别人工完成成功/失败/R 重试；重点增强发球预览对比度，核对二传配攻最终攻击与落点，而非只验证启动喂球。
3. 完善对手教练决策、挑战属性实际作用、批量换人和更自然的走入场/暂停集合动作；自由人单独设计并测试，不混入本轮已验证的普通换人账本。
4. 使用合法骨骼角色、动作与场馆资产逐步替换原型，建立明确授权清单和 LOD/材质预算，不能靠程序化简模承诺商业写实品质。

规则参考：[FIVB Official Volleyball Rules 2025–2028](https://www.fivb.com/wp-content/uploads/2025/01/FIVB-Volleyball_Rules2025_2028-EN-v05.pdf)。

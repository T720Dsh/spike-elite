# M11g：M11f 现场收尾、原创程序化美术精修与验收

验收日期：2026-10-03（本机 Asia/Shanghai）。工程：`D:\projects\spike-elite`，UE 5.8。

## 结论和边界

M11f 的“Armed 时机条未拍到”、实际第三人称机位、举臂跨 90° 翻转、低位扑救姿态、记录台构图及无形地板碰撞已现场修正；新增人物/裁判/看台/球网细节，最终源码 Editor / Game 构建及 65 项自动化测试通过，新 Win64 图形包通过本轮验证。

**本轮完成的是更细致的原创风格化原型，不是 FC/NBA 级写实资产交付。** 角色仍为分段程序动画，没有专业骨骼蒙皮、动捕、写实脸部/皮肤/布料贴图，也没有商业级观众 LOD 管线。不要把“面部/手指/鞋带已经存在”写成“AAA 美术完成”。

入场、发球员介绍、球队暂停、合法换人、12 人名单挑战赛、教练模式尚未实现，已交给下一轮 [M11h Prompt](M11h-Doubao-Gameplay-Prompt.md)。席边现有 10 个替补外观是装饰，不是权威名单。

## 已落地的修改

- `SEArtGeometry`：九种共享曲面网格（躯干、头、上下臂、大小腿、手掌、鞋、座椅），固定缓存，不逐人生成网格。修复 UE 三角面绕序，补打包运行时 UV 流送元数据；全部无独立比赛碰撞。
- 角色：纠正胸部比例与头部继承缩放，增加耳鼻嘴、眉眼、发型、独立手指/拇指、袖口、护腕、短裤口、护膝、袜子、鞋底和鞋带。不同肤色；衣物/皮肤区分粗糙度，小型细节关闭阴影以减少成本。
- 动画：四元数插值解决跨 90° 举臂翻转；修正身体前倾/膝盖方向，完整低位水平扑救；AI 直接位移时也更新步态/朝向；视觉足部贴地不修改胶囊和生产触球规则。
- 场馆：64×56m 外壳，比赛区 18×9m、自由区 31×19m 不变；将看台退到记录台与替补席后方，补实际通行地板、屋架、灯具、入口和立柱。观众使用独立随机流与 ISM，避免影响比赛 seed。
- 裁判与场边：第一裁判塔有支撑、梯级、平台、栏杆、防护垫；曲面裁判/记录员/装饰替补。记录台增加桌腿、笔记，记分牌面向场心，文字正向可读。
- 球网：细绳网格、上下胶带、护柱、底座、拉索、红白标志杆；细节不另建规则碰撞。
- 材质：原创 `Content/Materials/M_ArtSurface.uasset`，支持 Color / Roughness / Metallic / Specular；没有外部授权资产。可复现脚本 `tools/setup_art_surface.py`。
- 比赛相机：开赛重置控制器俯角/方向，MatchOver 使用持久场馆相机，不停留在角色近景；重赛复用，不重复生成。
- 战术：Armed 0.8 秒窗口采用真实时间，更新风险/最佳时机条，右键取消；TacticalTest 等待异步截图后才击球，不再同帧关掉面板。
- 工具：新增独立 ArtSuite、可重复验收脚本与仅进程级代理处理的打包脚本；`Play_SPIKE_ELITE.bat` 优先启动新 M11g 包，旧包保留为回退。

## 最终验证结果

| 项目 | 结果 | 精确证据（相对工程根目录） |
|---|---|---|
| Editor Development / Game Development | PASS | `Saved/Logs/Codex_M11g_package_success_final.log`，最终 17 个构建动作成功 |
| 自动化 | PASS，65/65，0 fail，退出 0 | `Saved/Logs/Codex_M11g_recheck_Automation.log` |
| Editor ArtSuite | PASS，真实玩家视角 + 6 个检查机位 | `Saved/Logs/Codex_M11g_final_Art.log` |
| Editor PoseSuite | PASS，40 张输出，关键姿态逐张查看 | `Saved/Logs/Codex_M11g_final_Pose.log` |
| Editor Seed 1 / 42 / 4242 | PASS，每组两场短局、重赛及退出 | `Saved/Logs/Codex_M11g_final_Quick1.log` / `Quick42.log` / `Quick4242.log` |
| Editor 战术 | PASS，4 个检查，Armed 时机条实际可见 | `Saved/Logs/Codex_M11g_final_Tactical.log` |
| Editor 五局逻辑 | PASS，26:24 / 24:26 / 26:24 / 24:26 / 16:14 → 3:2 | `Saved/Logs/Codex_M11g_recheck_FiveSet.log` |
| Editor 五次重赛 | PASS，权威对象唯一，12 名场上球员 | `Saved/Logs/Codex_M11g_supplemental_Rematch.log` |
| 最终 BuildCookRun | PASS，重新 cook 新资源，退出 0，129.18s | `Saved/Logs/Codex_M11g_package_success_final.log` |
| 新包 D3D12 Art / Quick42 / Tactical / FiveSet / Rematch | PASS，各进程退出 0、未超时、无 Fatal/Assertion/Ensure | `Saved/Logs/Codex_M11g_packaged_final_{Art,Quick42,Tactical,FiveSet,Rematch}.log` |
| 新包 D3D11 Quick1 | PASS，两场短局、确认取消与重赛、退出 0，无 Fatal/Assertion/Ensure | `Saved/Logs/Codex_M11g_packaged_d3d11_Quick1.log` |
| 纯真人键鼠完整对局 | NOT RUN | 自动验收不证明普通玩家能独立完成全部操作 |
| 全量 ShotSuite 随机真实接发 | NOT RUN，本轮未重跑 | Art/Pose fixture 不代替随机接发验收 |
| 1920×1080 / UI 1.4 全矩阵 | NOT RUN | 本轮逐张验收图片实际 1280×720 |
| 严格性能 A/B / P95 / 显存预算 | NOT RUN | 同机心跳约 11.1–11.4ms，仅参考，不能与旧日志比较后宣称无退化 |

自动化新增：`ArtProfilesSharedFinite`（包括 UV 流送断言）、`ArtDetailsNoGameplayCollision`、`VisibleFloorCollisionOnly`、`ArenaWorkAisleClearance`、`RefereeStandAndScoreboardGeometry`，旧 60 项未删。

日志边界：Editor 初始化阶段、正式执行 `SpikeElite.Tests` 之前还有 13 条 `LogAutomationTest: Error: Condition failed`。同阶段运行 UE Core 的 Smoke 自测（源码含 UnifiedError 测试），但这 13 条的具体调用栈本轮未逐一定位；不得声称整份日志绝对零 Error。正式列出的 65 项项目测试全部 Success。最终图形包日志未出现此初始化条目，且无项目 Error/Fatal/Ensure。

五局测试通过生产 `AwardPoint → CheckSetWin → StartNextSet` 链路，但有开发脚本加速注入得分，不等于真人打完五个自然比赛局。TacticalTest 的二传场景含合法开发 fixture，不冒充纯自然 AI 流程。ArtSuite 暂停比赛检查模型/机位，PoseSuite 则固定姿态；两者不用于证明真实回合成绩。

最终打包重赛审计五次均 `court/arena/ball/officials/rotwidget/scoreboard=1`、`chars(world/roster)=12/12`、`validActors=34`。比旧基线多一个持久结果相机；`totalActors` 包括等待 GC 的销毁对象，实测 35/46/57/34/45，不能写成“全部 Actor 总数恒定”。

## 看图与运行

新包：`D:\projects\spike-elite\Dist\M11g\Windows\SpikeElite.exe`。不含运行后 Saved 文件时，48 文件、906.78 MiB（约 951MB）；Development 而非 Shipping。旧 `Dist\Windows` 包未删除、未覆盖。正常体验双击根目录 `Play_SPIKE_ELITE.bat`，无须加 `devauto`。

本轮新包真实截图：`Dist/M11g/Windows/SpikeElite/Saved/Screenshots/Windows/`。

| 截图 | 内容 |
|---|---|
| `art_00_actual_player_view00001.png` | 正常玩家机位及 HUD |
| `art_01_arena00001.png` | 全场、自由区、场边工作区与看台 |
| `art_02_referee_tower00001.png` | 裁判塔实体结构 |
| `art_03_score_table00001.png` | 正向可读的实体记分牌 |
| `art_04_bench00001.png` | 装饰替补与座椅 |
| `art_05_seated_crowd00001.png` | ISM 坐姿观众 |
| `art_06_net_hardware00001.png` | 网绳、胶带、护柱与标志杆 |
| `shot_tac_05_armed00000.png` | 实际 Armed 阶段绿色时机条 |

上述新包图片逐张打开检查；所有截图实际 1280×720。角色近景/扑救详见 Editor `Saved/Screenshots/WindowsEditor/shot_pose_idle_front00016.png`、`shot_pose_dive_side00015.png`。检查截图保存在 Saved/Dist（gitignored），不自动冒充发布资产。

复验命令（PowerShell，工程根目录）：

```powershell
./tools/m11g_verify.ps1
./tools/m11g_package.ps1
./tools/m11g_verify.ps1 -Packaged -Executable 'D:\projects\spike-elite\Dist\M11g\Windows\SpikeElite.exe' -Suites Art,Quick42,Tactical,FiveSet,Rematch -Label packaged_final
./tools/m11g_verify.ps1 -Packaged -Executable 'D:\projects\spike-elite\Dist\M11g\Windows\SpikeElite.exe' -Suites Quick1 -Label packaged_d3d11 -AdditionalArguments '-d3d11'
```

每个失败/超时明确报错，脚本不自动重试挑一次成功。复跑同 Label 会替换同名日志，保留历史证据时换 Label。

## 失败尝试与修复，不抹掉历史

1. 首次美术自动化 64/65：地板顶面浮点误差导致过严断言失败；修正为 0.001cm 容差，最终 65/65。
2. 首次打包退出 1：继承代理将本机 Zen IPv6 请求转发外部代理，等待超时。证据 `Saved/Logs/Codex_M11g_package_proxy_failure.log`。构建脚本只对子进程绕过/清除代理，在 finally 恢复，不改用户持久配置。
3. 第一版图形包虽有画面，却出现 `GetUVChannelData` Ensure；验收判 FAIL，而不是凭截图判通过。证据 `Saved/Logs/Codex_M11g_packaged_uv_failure.log`。补初始化 UV 密度、增测试、重编译重 cook，最终包五项 D3D12 验收无此 Ensure。
4. 初版 FiveSet 脚本的 PASS 正则不匹配真实日志，并误传通用 devauto；修正正则及独立测试参数，再运行隔离流程 `Codex_M11g_recheck_FiveSet.log` 通过。原始失败日志保留。
5. 初版暂停 Art 相机截图出现缓存视角；设置控制器/相机在暂停期间更新后，重新生成本表机位截图，不使用旧错图作为 PASS。

## 残留问题和下一轮重点

- **P1 可玩性**：名单/队员身份、入场介绍、发球员大卡、球队暂停、换人、教练与模式均缺，详见 M11h Prompt；优先做名单权威与一个完整比赛闭环。
- **P2 战术界面**：720p Armed 底部帮助说明长行溢出背景；已列入 M11h 修复，并要求真人时机窗验收。
- **P2 美术目标**：风格化人物、观众简化、近看材质与脸部缺写实纹理，关节仍可看出拼接，发型/体型变化不足；场边装饰替补无独立号码名单。商业写实路线需要合法角色资产、规范骨骼/蒙皮、动画蓝图/IK、PBR 贴图与 LOD，不能继续只靠增加 Primitive 数量假称达到 NBA 水准。
- **P2 性能**：新增模型组件与观众几何有实际成本，尚未做可比的 60 秒热机采样/P95/GPU 分项与观众 LOD，不宣称性能闭环。
- **环境风险**：本轮 C 盘曾约 7.5GB，最终检查约 9.95GB，D 盘约 98.26GB；未进行全局清理。Local DDC 仍可见 C 盘路径，Shared DDC 在 D 盘，不能声称“所有 UE 缓存彻底迁出 C 盘”。
- 触网犯规、自由人、联网和授权 Mikasa 资产未实现，保留无品牌黄蓝白球与授权槽位。

Git：在 `main...origin/main [ahead 35]` 的 M11f 基线上进行现场修改。**本轮未 commit、未 push、未发布 Release**。请豆包先接收并核对全部未提交源码/资源/脚本，再按阶段开发、验证与提交，不覆盖现场修改。

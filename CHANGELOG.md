# Changelog

All notable changes to **SPIKE ELITE** (working title: a 3D indoor volleyball game built on UE5).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
for milestone tags once a first playable is tagged.

## [Unreleased]

### M11d 现场复核修正（2026-10-02）

修复分段肢体的缩放继承与髋/躯干连接，恢复正常尺寸的前臂、手、大腿、小腿；
修正轮转小球场前后排、标题、号码对比度、实际发球方标记和短时轮转提示。
二传/防守卡片绑定实际点击事件，二传列表约束高度并跟随键盘选择滚动；
目标区按球场纵横轴及球队朝向解释。UI 缩放改用游戏 UMG 缩放，并保存/恢复辅助设置，
未应用的减少动态效果随返回丢弃。验证脚本修正场景参数互斥、进程等待、独立日志、
退出码/完成标记以及新包验证；启动器暂用可运行的 Editor 游戏模式。

Editor/Game 编译通过，57/57 自动化回归（新增肢体层级和索引按钮测试），TacticalTest
PASS（failures=0），新 720p ShotSuite 完整退出。完整视觉/图形包仍未验收通过；
详情见 docs/M11d-Codex-Review.md，后续要求见 docs/M11e-Doubao-Prompt.md。

### Milestone M11d — 美术统一与 UI/UX 重构（风格化低多边形室内排球转播）

M11d-1 统一设计令牌（`SEUiStyle`）：深海军蓝/冷灰蓝/电光蓝(TeamA)/暖橙红(TeamB)/排球金
调色板，标题/副标题/正文/辅助/比分字号体系，间距/动画时长令牌，按钮键盘焦点金框，主/次/
危险按钮与半透明背板便捷样式；`bReducedMotion` 全局辅助开关。

M11d-2 菜单重构：主菜单改为深海军蓝赛事主页（聚光灯、Logo 锁定、入场动画、版本角标、
主/次/危按钮层级，"减少动态效果"时动画缩短）；设置界面分 显示/图形/控制/辅助功能 四区，
新增真正生效的 UI 缩放（Slate Application Scale）与减少动态效果；暂停/确认/比赛结束界面
统一使用设计令牌（比赛结束保留场馆可见并加队色带与胜负层级）。

M11d-3 比赛 HUD 重构：顶部中央左右对称比分板（TEAM A/B 队色块、大字比分、SET+局数、
发球队金点）、独立阶段徽章、三个触球圆点、中央回合结果横幅（1.1s 淡入淡出）、操作帮助
条与比分板分离并数秒后折叠；右上角改为小球场站位图（迷你球场/网线/三米线、12 个队色
圆点号码、发球员金色、受控白色、轮转提示），数据仍只来自 GameMode 权威轮转快照。

M11d-4 战术 UI 重排：攻击面板改为卡片式（目标区友好命名、力度进度条、弧线 低/中/高 档位、
绿黄红判定，调试数值移出主显示）；二传 13+1 列表放入 ScrollBox 并显示当前分组，行文本带
分类前缀；防守决策改为 拦网策略 / 后排防守 双列布局。

M11d-5 场馆美术：端墙内侧双色吸音板、无品牌赛事文字标识（SPIKE ELITE / PLAY FAIR），
保持 ISM 看台/观众与现有灯光结构。

M11d-6 角色与球：分段程序化运动员胸/背新增球衣号码 TextRender；比赛用球由"黄球+蓝带"
升级为原创黄蓝白多面板（蓝色赤道带 + 白色经线带），授权 Mesh/Material 插槽仍可用且缺失
时自动回退无品牌占位球。

> 原 M11d 交付报告自报结果（下述视觉通过项不等于现场复核通过）：720p/1080p `-devauto` 各跑一遍（DevVerifyFailures=0），
> ShotSuite + TacticalTest 截图逐张读取通过（主菜单 720p/1080p、设置、暂停、确认、左右对称比分板、
> 发球阶段、回合结果横幅、轮次小球场 HUD、战术瞄准卡片、二传 13+1 ScrollBox、防守 2×4、DiveActive、
> DiveSave、记录台记分牌、比赛球多面板、MatchOver、Rematch）；55/55 自动化测试回归通过；
> Win64 打包完成（约 839MB）。**未通过图形交付验收**：打包版 D3D 启动挂起（卡在 Slate 字体懒加载
> 后的 Texture streaming 初始化，早于 Game Engine Initialized；`-NullRHI` 下打包版逻辑可完整运行），
> 根因待定位，NullRHI 通过不排除资源/cook/同步加载问题，也不能替代图形验收。
> `UnrealEditor.exe ... -game` 可用作开发运行路径；ShotSuite 偶发无进展仍需修复。
> 用户配置了共享 DDC 为 D:\UE_DDC；Automation_M11d.log 的本地 Zen 数据实际位于 D:\ZenData，
> 两者不可混为一谈，不能据此保证引擎完全不写 C 盘。

### Milestone M11c — 发球球权纠错 · 权威轮转 · 真实救球生命周期 · 战术同源求解 · 屏幕 UMG 战术 · 打包收口

Verified: **55/55 automation tests pass**; three `-QuickMatch -devauto -FastFlow -SEED=1/42/4242`
smoke runs each reach MatchOver, Rematch to a second MatchOver and quit with no Fatal /
project Error / duplicate actors; `-TacticalTest` drives real tactical UMG (cancel path,
Attack + Set plans, perfect-timing shots, ball actually moved) and reports PASS; 5x
Rematch stress keeps court/arena/ball/officials/rotation-widget/scoreboard at 1 and 12
chars with a constant valid-actor count; Win64 Development package rebuilt and
smoke-tested (Seed 42); accelerated best-of-five state-machine test covers 25/15 targets,
win-by-2, 3-set match end, per-set reset and rematch reset.

#### M11c-1 Serve possession fix
- Serve no longer consumes the serving team's three touches; `[Serve] -> [NetCross]
  possession -> receiving team, touches=0 -> [Touch] 1/3 Receive` is the only legal order.
- During ServeFlight (serve struck, net not yet crossed) NOBODY may touch the ball;
  an illegal touch is blocked with a clear log, never silently treated as a legal set.
- Server starts behind the end line (X=±1150~1300, inside the service zone / free zone)
  and may move freely inside the service zone; serve faults (into net / out / delay)
  award the opponent via the single EndRally path.

#### M11c-2 Authoritative rotation
- New `ERotSlot` P1~P6: P1 back-right server, P2/P3/P4 front row, P5/P6 back row;
  world coordinates match the slot semantics (Team B mirrored, no reversed left/right).
- Front-row/block eligibility and back-row attack gating use the slot, not
  `abs(HomePosition.X) < 400` heuristics; per-team rotation state (no global counter),
  index wraps 1..6; side-out rotates only the receiving team; scoring keeps serving team.
- HUD / AI / HomePosition / server / block eligibility all read the same rotation state.

#### M11c-3 Real dive lifecycle
- Net-cross detection threshold 60 → 5 cm; every teleport/reset path (ResetBall,
  BeginServiceAuthorized, ExecuteServe, EndRally, Rematch) syncs the cross-state so a
  reset never fakes a net cross or a dive.
- Dive state machine: None → DiveApproach → DiveActive (0.35~0.55 s window with extended
  reach + low touch height + visible pose) → DiveRecovery (blocks a second dive) →
  normal. Logs distinguish `[DiveAttempt] / [DiveSave] / [DiveMiss] / [DiveRecoveryEnd]` —
  reports never count attempts as saves.

#### M11c-4 Tactical solver unification
- `SEVolleyballTrajectory::BuildShotSolution(Start, Intent, TimingError)` is the ONE
  solver for preview AND execution: dotted line, landing colour and the GameMode strike
  share the same InitialVelocity. No more "preview by target+flight, strike by direction+power".
- Power changes rebuild the preview and predicted landing immediately; perfect timing has
  exactly zero TimingError; early <0 / late >0 symmetric around 0; the same solver computes
  the resulting trajectory. Preview vs executed landing within 10~20 cm at zero error.
- Planning uses real-time DeltaTime (small slow-motion / UI tick), never TimeDilation=0 +
  frame DeltaTime; safe exit restores dilation, mouse, input mode, trajectory and UI.

#### M11c-5 Screen UMG tactical UI
- `UTacticalHUDWidget`: attack panel (target/power/apex/flight/net/bounds verdict/timing bar),
  full 13+1 set-tactic picker (四号位高球 … 背飞, 自由轨迹) with category / attacker / apex /
  flight / risk, and an 8-choice defense panel (单人/双人拦网、封直线/斜线、后排直线/斜线、
  常规救球、倒地救球). Keyboard (WASD/arrows) and mouse both work; Esc/RMB cancel.
- Set confirmation registers the play with the GameMode (`SetActiveSetPlay`) so the
  selected attacker runs up per tactic; Team A/B mirrors use one data table.

#### M11c-6 Ball structure & HUD polish
- Ball root is the sphere mesh (0.21 scale) with the band as a child with compensating
  scale — fixes the M11c-6 physics regression (ProjectileMovement sweep requires an Actor
  root; non-root mesh fell through the floor / never moved). Rotation follows velocity,
  pattern rotates with the ball.
- Licensed V200W slots only apply when BOTH mesh and material are valid (user-supplied,
  ASSET_LICENSE.md); missing assets fall back to the un-branded yellow/blue ball, never a
  Missing Package. UI label stays "比赛用球".
- RotationWidget safe offset (200+32 px from the right edge) and a compact layered
  scoreboard keep 1280×720/1600×900/1920×1080 fully readable.

#### M11c-7 Tests, stress, package & docs
- Automation tests grown to 55 (ServeDoesNotConsumeTeamTouch … HUDSafeZoneAt1280x720 +
  MatchFlowFiveSets): 55 passed / 0 failed.
- `-TacticalTest` (non-Shipping): real rally → planning window #1 cancel (restores
  Normal + TimeDilation=1) → window #2 Attack plan + perfect shot (ball moved) →
  window #3 Set plan (13+1 picker screenshot) + perfect shot — PASS, then quit.
- `-RematchStress`: 5 full rematch cycles with `[DevAudit]` proving court/arena/ball/
  officials/rotation widget/scoreboard = 1 each, 12 chars, and a constant valid-actor
  count (PendingKill excluded).
- Win64 Development package rebuilt to `Dist\Windows\SpikeElite.exe` and smoke-tested
  (menu/Chinese font/arena/ball/officials/HUD/trajectory + QuickMatch + Rematch).


### Milestone M11b — International arena, officials, rotation HUD, visible procedural limbs, tactical shot & dive systems

Verified: 27 automation tests pass; `-QuickMatch -devauto -FastFlow -SEED=1/42/4242`
each reach MatchOver, Rematch to a second MatchOver and quit with no duplicate
actors, Fatal or Ensure; Low/Medium/High crowd density all run the full match
(~7–15 ms frame at 1280×720); Win64 Development package rebuilt and smoke-tested.

#### M11b-1 Arena lifecycle split
- New `AVolleyballArena` (persistent shell: 60×44×15 m hall, 10-row tiered
  stands on all four sides via ISM, LED boards, dark far walls, roof, lights).
- `AVolleyballCourt` persists and only resets match state; free zone 5 m
  (side) × 6.5 m (end) — total 31×19 m; service-zone short lines added.
- StartMatch/Rematch no longer respawn the arena, court, ball or officials;
  continuous Rematch keeps actor counts stable.

#### M11b-2 Officials, rally buffer, whistle & rotation HUD
- `AMatchOfficialManager`: first referee on a stand (+50 cm above net top),
  second referee, scorer's table with a live UMG-independent scoreboard
  (TextRender, event-driven from GameMode), benches with lightweight subs.
- Ceremony state machine: ResettingPositions → AwaitingReady → ServiceAuthorized
  (whistle + 8 s serve deadline) → ServingToss → Rally; early serve rejected;
  serve delay is a fault; `-FastFlow` shortens all delays; TimerManager only.
- `URotationWidget` (top-right): P1–P6 slots, front/back row divider, serving
  team, 3 m line, current handler highlight; H toggles.

#### M11b-3 Visible articulated procedural characters
- Characters upgraded from 6-block figures to segmented joints (head, torso,
  upper/lower arms, hands, thighs, calves, feet) with Idle/Run/Jump/Receive/Set/
  Spike/Block/Dive/Recover/Serve/RaiseHands poses, RInterpTo smoothing.
- RMB hold = continuous RaiseHands; first-person hides own head.

#### M11b-4 Tactical slow-motion & dotted trajectory preview
- `SEVolleyballTrajectory` (pure, tested): gravity-consistent integrator,
  net/antenna/court detection, IN/OUT/net-touch classification.
- `UTrajectoryPreviewComponent`: pooled dotted path + landing ring, green/
  yellow/red legality colors; `UTacticalContactComponent`: freeze-plan-aim-confirm
  state machine, TimeDilation 0 → 0.15 armed window, Esc-safe.

#### M11b-5 Data-driven sets, real blocking, dive defense
- `SESetPlays`: 13 named Chinese set plays + free trajectory, team-local
  coordinates mirrored per half (no per-play magic constants).
- Real blocking: front-row gate (home X), block not counted as a touch, blocker
  may touch again, off-the-hands out gives the point to the attacker; AI
  front-row block directives.
- Dive defense: net-cross detection opens a dive window; receiver lunges
  (extended reach/lower Z), recovery gates re-dive and touch.

#### M11b-6 Ball, performance, tests, docs
- Ball: un-branded yellow/blue placeholder (no Mikasa/FIVB logos), licensed
  V200W slots + `Content/Balls/ASSET_LICENSE.md`.
- Crowd density `-CrowdLow/-CrowdHigh`; 5 s performance heartbeat; 27 tests.

### Milestone M11a — Stability, interaction state machine, performance & release hygiene

Verified with headless runs: 18 automation tests pass; `-devauto` walks every
menu + confirm dialog (cancel paths verified with PASS/FAIL logs); three
`-QuickMatch -devauto -SEED=1/42/4242` runs each reach MatchOver, cancel a
confirm dialog from the result screen, Rematch to a second MatchOver, then
quit; the Win64 Development package was rebuilt from scratch and smoke-tested.

#### Confirm-dialog state machine (the M11a headline fix)
- `ShowConfirm` now sets `MenuState = Confirm` and refuses to stack a second
  dialog (refocuses the existing one). Esc inside a confirm only closes the
  dialog; the pause menu underneath stays paused and cannot be revived.
- `CancelConfirm` restores exactly the state that opened the dialog: Paused →
  Paused (menu intact), MatchOver → MatchOver (world stays paused), Playing →
  Playing (mid-match confirm-cancel no longer dumps the player to the main
  menu), MainMenu → MainMenu. Target widgets are rebuilt if lost.
- Main menu "退出游戏" now routes through the same confirm dialog.
- `ConfirmWidget` default keyboard focus = 取消 (safe); MatchEnd default focus =
  再来一场; settings default focus = 返回; Tab/arrows/Enter/Esc work on
  settings, pause, result and confirm screens.
- `AcceptConfirm` still clears dialog + pending action before executing.

#### Automation vs. normal play fully separated
- `BeginAwaitingServe` uses `SEVolleyballRules::ShouldAutoServe(bot, devAuto)`:
  bots always auto-serve; a human serves ONLY on E unless `-devauto` is on the
  command line (non-Shipping builds). Normal `-QuickMatch` without `-devauto`
  still requires real key input. The UI "按 E 发球" hint is now honest.

#### MatchOver input gate
- `OnMatchOver` now pauses the world (`SetPause(true)`): no character input,
  no bot Tick, no ball projectile behind the result screen. UI (buttons, mouse,
  keyboard) keeps working; `Rematch` and menu-return correctly unpause and
  restore input, and Rematch resets score/set/rally state via the existing
  `StartMatch` → `CleanupMatch` path.

#### Production code reuses the tested rule core
- `CanTouchBall` / `TryTouchBall` call `IsTouchLegalInPhase` (no second copy).
- `RotateTeam` reorders via the same `RotateRoster` core the tests exercise.
- `StartPlay()` is the single place that sets `bBallInPlay=true` (ExecuteServe);
  settle/cleanup set it false. Tests assert the full lifecycle.
- `IsServeFault` classifies any serve that never crossed the net — in or out —
  as a serve fault (shared by GameMode and tests).
- `[RallyEnd]` logs the score after awarding with explicit before -> after.
- `ERallyEndReason::Cancelled` is now exercised: an un-settled rally at
  cleanup is settled and logged as Cancelled (no point).
- 13 → 18 automation tests (additions: BallInPlayLifecycle,
  ServeFaultClassification, RotationMatchesCoreAlgorithm, PhaseGateMatrix,
  AutoServePolicy).

#### Update frequency (perf hygiene)
- `UpdateScoreboard` is called immediately at state-change sites but only
  ~6.7 Hz from Tick for the dynamic ball hint, and builds ONE compact signature
  per call — SetText only when content changed (before M11a it rebuilt every
  FString/FText and called SetText at 60+ Hz).
- `UpdateAIDirectives` re-selects at ~12.5 Hz; bot movement stays per-frame.
- Widgets add their own per-row change guards; scoreboard gained a translucent
  backdrop, an aligned 8-row layout, a full controls help line that collapses
  after ~5 s, and an in-pause-menu "操作说明" toggle.

#### Release / config hygiene
- `SpikeElite.Target.cs` sets `IncludeOrderVersion = Unreal5_8` (Game target
  now matches the Editor target; include-order warning gone).
- `DefaultEngine.ini`: the Android File Server block (including the committed
  `SecurityToken=0676...`) is REMOVED, not just rewritten (M10c had swapped the
  token value but left the block). Comment documents build-machine token
  generation if mobile tooling is ever added.
- `DefaultInput.ini`: the two leftover EnhancedInput class lines are deleted
  (the project stays on legacy BindAxis/BindAction); `ToggleFirstPerson` now
  maps both C (primary) and V (compat). README and in-game help match.
- `-devauto` gained `DevVerify` PASS/FAIL checks (confirm-cancel from Paused /
  mid-match / MainMenu / MatchOver) and screenshots shot_07–09 + shot_qm_03.

### Milestone M10 — Vertical slice: rules authority, 3-touch volleyball, team AI, tests, Win64 package

Verified with headless runs (`-devauto`, `-QuickMatch -devauto`): a full
QuickMatch is played unattended to MatchOver, the result screen is shot, then a
Rematch (再来一场) starts a second clean match (score reset, no actor leaks).
13 automation tests pass; packaged Win64 build runs without the editor.

#### Rule authority (GameMode is the single source of truth)
- `EMatchState` machine: PreMatch / BetweenRallies / AwaitingServe / ServingToss /
  Rally / SetOver / MatchOver. ServingToss forbids normal and AI touches; paused
  menus and set/match-over states forbid all touches.
- Serve goes through `RequestServe` (GameMode checks AwaitingServe, correct
  server team, legal server, toss-ready); Characters never reset the ball.
- Every touch (player or AI) funnels through `TryTouchBall` / `CanTouchBall`:
  touch-armed single-contact protection + per-character rearm when the ball
  leaves the touch volume.
- `EndRally` settles each rally exactly once (no double points) and always leads
  to BetweenRallies; serve faults, four touches, double touch, in/out, net and
  landing all use the same path with structured logs.
- Scoring: in-bounds (lines count in) → opponent of defending half; out → opponent
  of last touching team; five sets, 25/15, win by two.

#### Simplified 3-touch rules (SEVolleyballRules, pure logic, no scene actors)
- Possessing team, 0–3 touch count, last-touch team/player, last net-crossing
  direction, rally-settled flag.
- Possession switches on a legal cross; new side starts at 0 touches.
- 4th touch → opponent scores; same player twice in a row → opponent scores;
  first teammate touch after serve is legal. No fake "block doesn't count" logic
  (no block system yet — documented as future work).
- 13 automation tests cover in/out/line-in, touch sequences, double touch,
  cross-net reset, single settlement, win-by-two at 24:24, deciding set 15,
  side-out rotation, MatchOver blocking further play, and QuickMatch rules.

#### Team AI coordination (GameMode-driven, no per-frame GetAllActorsOfClass)
- Ball/match context handed to characters at spawn (weak refs, safe on teardown).
- One primary handler per rally: receive (closest to predicted landing), set
  (designated setter zone), attack (pre-selected front player runs during the
  set); everyone else holds role positions instead of chasing the ball.
- Attack requires high contact (Z ≥ 240) so it clears the 243 cm net plane;
  seeded AIStream random errors (deterministic under -SEED).
- Bots move via deterministic direct stepping (deferred-spawn pawns had
  MOVE_None / unconsumed movement input); boundary clamped to own half.

#### Feedback, camera, QuickMatch, packaging
- HUD: match phase, possession + touch count ("A 2/3"), serve hint ("按 E 发球")
  only when legal, rally-result banner, in-reach hint; `FMath::FindDeltaAngleDegrees`
  for correct ±180° ball-direction hints.
- MatchOver screen (winner, per-set scores, 再来一场 / 返回主菜单 / 退出到桌面)
  with mouse released and Esc deliberately disabled; confirm dialogs before
  returning to menu / quitting.
- Camera: longer raised arm, shoulder offset, collision test, slight lag; first/
  third-person switch keeps view direction.
- `-QuickMatch`: one set, 3 points, reuses production rules; unattended flow
  (serve timeout, auto-match to MatchOver, Rematch, screenshots, quit).
- Win64 Development package built with BuildCookRun into git-ignored `Dist\`;
  `Play_SPIKE_ELITE.bat` prefers the packaged exe and instructs developers to
  build first when it is missing (no longer claims the editor is "no editor").
- Mannequin skeletal upgrade intentionally NOT applied: asset references point at
  the absent `/Game/Characters/Mannequins/*`, so the ABP cannot compile; blocky
  placeholder bodies remain the supported fallback (see `MannequinAssetsLoad`
  diagnostics in the M10b commit message).

### Milestone M9 — Runtime UI fixes: RebuildWidget, mouse release, fonts, single-authority ball

- Removed the bundled `msyh_source` font asset; UI now uses the engine core
  composite font (`FCoreStyle::GetDefaultFontStyle`) with CJK fallback, so menus
  render Chinese without bundling a proprietary Windows font.
- Fixed `RebuildWidget`/font rebuild issues that broke runtime-updated widgets;
  fixed mouse-release and capture transitions across pause/settings/menu flows.
- Ball remains single-authority: `ProjectileMovement` only, no Mesh
  SimulatePhysics (never both at once).

### Milestone M8 — Second-pass hardening: indoor arena, reliable materials, net authority, IN/OUT scoring, match lifecycle

Verified with headless standalone runs (`-devauto`): full `Result: Succeeded`
builds, in-game screenshots, and automated menu → match → pause → menu → match
cycles. No project `Error` / `Fatal` / `Ensure` / asset-load failures in the log
(profiler DLL / mobile-SDK notices are unrelated engine noise).

#### Root causes fixed
- **Mouse / Esc:** the pause action binding lacked `bExecuteWhenPaused`, and the
  pause state used `UIOnly`, so keyboard/Esc could not reliably resume; the game
  mode also never re-captured the mouse on returning to play.
- **Lifecycle:** `CleanupMatch` deleted **every** `ADirectionalLight` in the
  level via `GetAllActorsOfClass`, wiping lights the map owned; rally timers and
  match flags were not reset between matches.
- **Net:** setting `Opacity` on an opaque material never made it transparent; the
  visual mesh used `BlockAll` **and** the GameMode deflected the ball by hand
  (double rebound), with no cooldown so the ball could be struck every frame.
- **Arena:** the project forced the engine `OpenWorld` template, so sky/sun/snow
  mountains were visible behind an "indoor" court.
- **Materials:** tinting engine BasicShape materials through a runtime `Color`
  parameter was unreliable (world-grid fallback / black parts). Instanced meshes
  additionally fell back to grey in a standalone build because the materials
  lacked the `bUsedWithInstancedStaticMeshes` usage flag.
- **Ball:** the sphere was ~40 cm across and ran **both** Chaos simulation and
  `ProjectileMovement`, which fought each other.
- **Scoring:** a dead ball was judged only by the sign of X, so balls landing
  beyond a side/end line still counted as in.
- **Bots:** AI struck the ball every frame with no per-bot cooldown and always
  with an upward arc, producing never-ending rallies.
- **Serve:** `ServeNextBall` left `MatchState = BetweenRallies`, so the Tick
  re-served every frame and reset the toss timer, freezing the ball at the serve
  spot (it was never struck).
- **Landing detection:** with the mesh kinematic, `OnComponentHit` was not
  broadcast, so the ball bouncing on the floor never ended a rally.
- **Team colours:** bots were spawned and had `TeamSide` assigned only **after**
  spawning, so `ApplyJerseyColor` (run in `BeginPlay`) always used the default
  Team A colour — every character rendered blue.

#### Changed / added
- **Input state machine** (`SpikeElitePlayerController`): pause binding sets
  `bExecuteWhenPaused`; pause uses a configured `GameAndUI` mode (`DoNotLock`,
  capture released, widget focused) so Esc/keyboard still work while movement is
  blocked; play uses `GameOnly` and explicitly re-captures/hides the mouse. Esc
  toggles Playing↔Paused and backs out of either settings screen.
- **Lifecycle:** match lighting is owned by the court and destroyed with it; the
  GameMode no longer spawns/deletes level lights. `CleanupMatch` resets toss/rally
  timers, cooldowns, `BallPrevX`, arrays, scoreboard ref and match state, and
  clears timers. Actor counts are logged at BeforeStart/StartMatch/Cleanup.
- **Net:** visual net is an instanced thin-bar grid (no physics) with separate
  opaque white 7 cm top tape, 5 cm bottom tape and two posts; bottom at 143 cm,
  top at 243 cm. Collision is single-authority in the GameMode (plane-crossing +
  band 143–243 cm + width + 0.45 s cooldown); balls below 143 cm or outside the
  net width pass freely.
- **Indoor arena:** new project map `/Game/Maps/Arena` (PlayerStart, no
  sky/sun/fog/clouds); the court builds an enclosed hall (walls + roof), a 3 m
  free zone, four-sided stepped stands with aisles, and court spot/fill lights
  plus dim warm perimeter lights and a low sky light. Config and
  `Play_SPIKE_ELITE.bat` no longer force `OpenWorld`.
- **Materials (generated by `tools/setup_assets.py`, committed as assets):**
  `M_WoodFloor` (imported wood diffuse+normal, tiled), `M_SportFloor`,
  `M_Tint` (reliable `Color` parameter, ISM-usage enabled) and `M_Crowd`
  (`Color` plus a small `Em` emissive so distant spectators stay readable).
- **Crowd:** one instanced component per clothing colour + one skin-tone head
  component (a handful of draw calls, no per-spectator actors), with head/torso
  silhouettes, stepped rows and central aisles.
- **Ball:** 21 cm diameter (sphere scale 0.21); single-authority
  `ProjectileMovement` (mesh physics off); landing is detected via
  `OnProjectileBounce`; `ResetBall` zeroes velocity.
- **Scoring:** IN when `|X| ≤ 900 && |Y| ≤ 450` (lines count in) → opponent of the
  defending half scores; OUT → opponent of the last touching team scores. Serve
  and every hit set `LastHitTeam`; a net tap does not change it. Each dead ball
  logs IN/OUT, location, last touch and scoring team.
- **Bots:** per-bot 0.7 s hit cooldown and a bounded ~15 % error rate (wide / long
  / into net) so rallies finish and points are awarded.
- **Characters:** proportional hinged humanoid (head, torso, two arms, two legs)
  with distinct Team A electric-blue / Team B red jerseys, dark shorts and skin
  heads; bots spawn deferred so colour is applied with the correct team.
- **UI:** real volleyball icon, three-state button styles, one-shot title
  fade-in + looping ball spin/bounce, Chinese text via the engine core composite
  font (CJK fallback; the earlier `msyh_source` font-face import was removed in
  M9, see below); settings read current
  window-mode/resolution/quality (system resolution enumeration, current value
  appended if missing), sensitivity range unified to 0.1–3.0 and only saved on
  Apply, with an applied-confirmation hint.
- Dev automation (`-devauto`, non-Shipping only) captures menu/settings/court/
  net/stands/pause and exercises two menu↔match cycles; hard-coded dev screenshot
  paths and unused UI class properties were removed.

### Milestone M7 — Front-end flow, FIVB court/arena, real net, game settings

#### Added — main menu / pause / settings front-end
- **Custom `ASpikeElitePlayerController`** owns a `EMenuState` state machine
  (MainMenu / Playing / Paused / SettingsFromMenu / SettingsFromPause) and all
  input-mode + mouse-capture transitions.
- **Main menu (`UMainMenuWidget`)**: title `SPIKE ELITE`, neon-yellow bold heading,
  electric-blue accent stripe, dark charcoal background, buttons 开始比赛 / 设置 /
  退出游戏, plus a looping animated volleyball icon (bounce + scale) and a pulsing
  title. Built entirely in C++ UMG (engine white-square brush, no Blueprint assets).
- **Pause menu (`UPauseMenuWidget`)**: 继续游戏 / 设置 / 返回主菜单 / 退出到桌面,
  dimmed overlay. Bound to `Esc`, which toggles open/closed.
- **Settings (`USettingsWidget`)**, all functional and persisted:
  - Window mode (windowed / borderless / fullscreen) and resolution via
    `UGameUserSettings`, applied and saved.
  - Graphics quality preset (Low/Medium/High/Cinematic) via
    `SetOverallScalabilityLevel`.
  - Mouse sensitivity slider (0.1–3.0), persisted to `GameUserSettings.ini` and
    multiplied into the character turn/look rates.
  - Settings is reachable from both the main menu and pause menu; back returns to
    the correct origin.
- **Mouse capture fixed.** Launch now goes to the menu with a visible, unlocked
  cursor that can leave the window (title-bar close and Alt+F4 work). Starting a
  match switches to `GameOnly` (hidden, captured mouse for look); pausing switches
  to `UIOnly` with `DoNotLock` and releases capture. Focus loss never re-grabs the
  mouse. Quit uses `UKismetSystemLibrary::QuitGame`.
- `DefaultInput.ini`: `bCaptureMouseOnLaunch=False`, `NoCapture` / `DoNotLock`,
  added `Pause=Escape`, and repaired the previously empty (None) WASD/mouse axis
  mappings.

#### Changed — match lifecycle
- GameMode no longer spawns the world on launch. `StartMatch()` spawns the court,
  ball, 1 human + 11 AI players, light and scoreboard and resets the score;
  `ReturnToMainMenu()` / `CleanupMatch()` tear them down. Repeated menu→match
  transitions do not duplicate actors. The human pawn is parked off-world and
  collision-disabled while in the menu.

#### Changed — FIVB court and arena
- Three-layer floor: outer arena slab (30×20 m), 3 m free-zone surround, and the
  18×9 m maple play court.
- Standard lines, 5 cm wide, raised 1 cm with collision disabled (no z-fighting,
  no physics ridge): two end lines (X=±900), two side lines (Y=±450), centre line
  (X=0) and two attack lines (X=±300).
- Removed the four close walls. Added six-row stepped stands on all four sides
  (step depth 90 cm, rise 45 cm) beyond the 3 m free zone, using
  `UInstancedStaticMeshComponent` for both the step slabs and the crowd (no
  per-spectator Actors / draw-call explosion).

#### Changed — net geometry and collision
- Net top 243 cm, cloth band 100 cm tall (bottom at 143 cm), width extends 80 cm
  past each sideline to the cylindrical posts. Semi-transparent cloth with opaque
  white 7 cm top band and 5 cm bottom band that do not go transparent.
- GameMode net touch now only reflects the ball inside the real band
  (143 < Z < 243) and within net width, so a ball passing under the net is no
  longer blocked by an invisible wall.

#### Changed — characters
- Replaced the broken, unloadable Mannequin/ABP dependency (template cooked assets
  could not be found at runtime and spammed load errors) with a zero-asset
  hinge-style placeholder: a box torso + sphere head per player, tinted electric
  blue (Team A) / red (Team B). Matches the "articulated stand-in first" art plan;
  a rigged skeletal mesh can be swapped in later.

#### Dev
- Optional `-devauto` command line: starts a match 2 s after launch, requests an
  in-engine shot at 8 s and quits at 10 s for headless smoke testing.

---

### Milestone M3 — FIVB rotation, serving player, stadium lighting
_commit 8ec4531 → next_

#### Added
- **FIVB §7.4 positional rotation on side-out.** When the team that did NOT serve wins the rally,
  its roster rotates clockwise: position 2 → 1, 3 → 2, …, 1 → 6. The human player is teleported
  to their new home position; bots update their `HomePosition` and walk there.
- **Roster tracking.** GameMode keeps `TeamAPlayers[6]` / `TeamBPlayers[6]` in position order
  (index 0 = position 1 = the server).
- **Server-based serve.** `ServeNextBall()` now places the ball at the current position-1
  player's location and strikes it across the net, instead of spawning it at a fixed point.
- **New-set reset.** `RespawnPlayersToPositions()` teleports both rosters back to the starting
  1–6 layout when a new set begins.
- **Directional stadium light** (rotated -55° pitch, intensity 4.5) so the court and Mannequins
  are readable in the default OpenWorld map.

#### Fixed
- Removed leftover SkyLight call that failed to compile (`USkyLightComponent` not forward-included).
- Scoreboard no longer uses `%s` with a ternary (UE5.8 `UE_LOG` static_assert trap).

---

### Milestone M2 — 6v6 AI characters
_commit 93d2e8f → 8ec4531_

#### Added
- 12 real `ACharacter` actors on the court: 1 human (Team A, position 1) + 5 Team A bots +
  6 Team B bots. Mannequin skeleton mesh (`SK_Mannequin`) from the UE5 template resources.
- `ASpikeEliteCharacter::TickBot` simple AI:
  - if the ball is on the bot's side, between 1.2 m and 4.5 m high, and within 2.5 m,
    `Strike()` it toward a random spot on the opponent's back court (9–11 m/s);
  - otherwise walk toward the ball if it is dropping on the bot's side, else walk back to
    `HomePosition`.
- Jersey colours via `UMaterialInstanceDynamic`: Team A = blue, Team B = red.
- Bots are boundary-clamped so they never cross the net or run out of bounds.

#### Removed
- The old "placeholder cube" position markers (replaced by real characters).
- The old GameMode-side fake AI that teleported the ball across the net every 1.2 s —
  the ball is now physically hit by the characters themselves.

#### Engineering notes
- UE5.8 `UE_LOG(…, TEXT("… %s …"), cond ? TEXT("A") : TEXT("B"))` triggers a
  `static_assert "Formatting string must be a TCHAR array"` because the format-string
  checker does not see through the ternary. Avoid conditional strings in `UE_LOG` args;
  either precompute a `const TCHAR*` into a variable or drop the `%s`.

---

### Milestone M1 — Playable single-rally prototype
_earliest commits_

#### Added
- C++ project skeleton, `WASD` movement + mouse look, `Space` jump, `V` toggle between
  first-person and third-person spring-arm camera.
- Procedural court: 18 m × 9 m wooden floor (Poly Haven `wood_floor` CC0 diffuse+normal),
  7 FIVB white lines (end / sideline / center / attack), transparent cloth net at 2.43 m,
  two net posts, 5 m perimeter walls.
- Physics-driven orange `AVolleyballBall` (sphere, Chaos physics, `OnComponentHit` →
  floor contact calls `GameMode::OnBallLanded`).
- Ball-net collision bounce.
- FIVB rally-point rules: 25 / win-by-2 / best-of-5 / decider to 15, side-out serves.
- Debug on-screen scoreboard (set, score, sets won, server) and a first-person ball-direction
  arrow (`FRONT` / `LEFT` / `RIGHT` / `BEHIND` / `HERE!`).
- Player hit: `LMB` (8.5 m/s on ground, 12 m/s while jumping), `E` serve.
- Stands: three rows × 17 coloured cubes as a placeholder crowd.

---

## Build & run

```powershell
# Compile (bypasses the UBA recursion bug in Build.bat on this machine)
& "D:\Epic\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" `
  "D:\Epic\UE_5.8\Engine\Binaries\DotNet\UnrealBuildTool\UnrealBuildTool.dll" `
  SpikeEliteEditor Win64 Development -Project="D:\projects\spike-elite\SpikeElite.uproject"

# Play
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe `
  "D:\projects\spike-elite\SpikeElite.uproject" /Engine/Maps/Templates/OpenWorld `
  -game -windowed -ResX=1280 -ResY=720
```

Controls: `WASD` move, mouse look, `Space` jump, `V` toggle first/third person,
`LMB` hit, `E` serve, `Esc` pause/resume.

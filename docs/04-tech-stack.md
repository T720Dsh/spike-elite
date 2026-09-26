# 04 · 技术栈（Tech Stack）

> 版本 v0.2 · 2026-09（首发平台改为 PC）
> 本文档锁定引擎、渲染分档、网络方案与仓库结构。任何偏离需要在 Issue 里讨论。

---

## 0. 平台策略（2026-09-27 修订）

**首发：PC（Windows，Steam），键鼠 + 手柄。**
**后续：移植 iOS / Android（约在 PC 版 EA / 正式上线之后）。**

理由：
- 先用 PC 把画面、操作手感、网络同步做到位，不用一开始就背移动端优化包袱；
- PC 上可以放开用 Lumen / 硬件光追 / Nanite 全量，先把"画面对标 3A"的承诺兑现；
- 移动端移植时再做性能裁剪，那时已经有了成熟玩法和资产管线；
- 这也是《原》《和平精英》UE5 版等项目走过的路。

## 1. 引擎

**Unreal Engine 5.5 LTS+**（用本机已装的版本，见工程根目录 `.uproject` 的 `EngineAssociation`）。

为什么是 UE5 而不是 Unity：
- **Nanite**：高模球员直接上桌，不需要手工做 LOD；
- **Lumen + 硬件光线追踪**：PC 首发可以全开，这是我们"渲染对标 3A"的核心；
- **Motion Matching / Motion Warping**：UE5 原生高质量角色动画混合，体育游戏刚需；
- **Lyra 示例工程**：提供了完整的输入、网络同步、UI 框架，可直接 fork 改造；
- 后续移植手游时，再按 [2026 年 UE5 移动端文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/configuring-graphics-performance-on-mobile) 做分档降级。

## 2. 渲染分档（PC 首发）

PC 首发不做移动端那种硬分档，而是让玩家在 High/Medium/Low 三档里选，我们保证：

| 档位 | 目标 GPU | 光照 | 帧目标 |
|---|---|---|---|
| **Epic / Cinematic** | RTX 3070 / RX 6800 及以上 | **Lumen + 硬件光线追踪阴影 + 全局反射**，Nanite 全开 | 1440p 60fps / 4K 30fps |
| **High** | GTX 1660 / RTX 2060 / RX 5600 | Lumen 软件光追，反射降级为屏幕空间 | 1080p 60fps |
| **Medium** | GTX 1060 / RX 580 | Lumen 关闭，静态+局部动态光照 | 1080p 60fps |
| **Low** | 核显 / 老卡 | 完全预计算光照 | 720p 30/60fps |

> 移动端移植阶段（M5）再补：iPhone 全部预计算光照；高端 Android（Adreno 7xx / Mali G7xx）试 Lumen Mobile；中低端走预烘焙。

## 2.1 第一人称渲染特别项

- FP 视角下镜头离球员身体近，**手臂/手模型必须做第一人称专用资产**（高模、跟随相机）；
- FP 下要保证球离镜头近时不出现穿模/裁剪问题；
- FP 扣球瞬间加镜头 FOV 拉伸 + 运动模糊，强化冲击感。

## 3. 核心系统选型

| 领域 | 选型 |
|---|---|
| 角色动画 | **Motion Matching**（UE5.4+ 内置）+ 动捕数据；IK 山体/脚着陆；Animation Warping 调整扣球起跳位置 |
| 物理 | 球用**自定义弹道模型**（不全靠 PhysX）：发球初速、旋转 Magnus 力、空气阻力、网带扰动单独可调；球员碰撞用 Capsule |
| 网络 | 基于 Lyra 的 **UE Replication Graph**，P2P + 中转服务器（matchmaking 用 Epic Online Services 或自建 Go 后端） |
| 后端 | Go / Node.js（matchmaking、账号、赛季数据）+ PostgreSQL + Redis；静态资源 CDN |
| UI | UE5 UMG + 自定义手游触控控件；后期可能换 **CommonUI**（Lyra 同款） |
| 数据驱动 | 球员数值、卡牌、战术全部 DataTable + JSON 配置，策划可改不用重新打包 |
| 分析 | 接入 Adjust / 自研事件管道，重点追踪：触球成功率、每局时长、PvP 断线率、付费漏斗 |

## 4. PC 首发性能预算（硬指标）

- **存储**：首发下载 ≤ 25 GB（PC 不限制包体，但要控制）；
- **内存**：Epic 档峰值 ≤ 16 GB；High 档 ≤ 12 GB；
- **帧率**：Epic 档 1440p 60fps / 4K 30fps；High 档 1080p 60fps（在 RTX 2060 / GTX 1660 上）；
- **启动时间**：SSD 上冷启动到主菜单 ≤ 20 秒；HDD ≤ 40 秒。
- **手柄支持**：首发即支持 Xbox / DualShock / DualSense 手柄（Steam Input）。

## 5. 仓库结构（规划）

```
spike-elite/                  # 本仓库（设计文档 + 后续 UE5 工程根）
├── README.md
├── docs/                     # 你正在读的设计文档
├── reference/                # 规则参考、FIVB 规则 PDF 等
├── Config/                   # UE5 Config（后加）
├── Plugins/                  # 自研插件（排球规则子系统、操作输入）
├── Source/
│   ├── SpikeElite/           # 游戏主体
│   ├── SpikeElite.Core/      # 规则、状态机、回合管理器
│   ├── SpikeElite.Gameplay/  # 球员、球、扣球/垫球/拦网
│   ├── SpikeElite.UI/        # 主菜单、卡牌、排位
│   └── SpikeElite.Online/    # 同步、匹配、对局服务
└── Content/                  # UE5 资产（用 LFS 管理）
```

UE5 工程文件（`.uproject`）将在 M1 阶段提交；在此之前本仓库只有文档。

## 6. 分支与协作规范

- `main`：永远可发布，保护分支；
- `develop`：集成分支；
- 功能分支：`feat/volley-physics`、`fix/serve-timing`、`docs/gdd-v0.2`；
- Commit message：Conventional Commits（`feat:`, `fix:`, `docs:`, `art:`, `chore:`）；
- 每个 PR 至少 1 人 review；玩法改动必须更新 `docs/03-game-design-document.md`。

## 7. 工具链

- 版本控制：Git + Git LFS（`.uasset` / `.png` / `.wav`）
- 任务管理：GitHub Issues + Projects（看板）
- 文档：Markdown，PR 即评审
- 动捕：Motive / OptiTrack（外部工作室按需租赁）
- 音效：Wwise 或 UE5 自带 MetaSounds（首发用 MetaSounds 省成本）

---

下一篇 → [05-roadmap.md](05-roadmap.md)

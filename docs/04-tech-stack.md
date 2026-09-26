# 04 · 技术栈（Tech Stack）

> 版本 v0.1 · 2026-09
> 本文档锁定引擎、渲染分档、网络方案与仓库结构。任何偏离需要在 Issue 里讨论。

---

## 1. 引擎

**Unreal Engine 5.5 LTS+**（推荐 5.5.4 或更新的稳定 LTS）。

为什么是 UE5 而不是 Unity：
- **Nanite**：高模球员直接上桌，不需要手工做 LOD，省美术管线；
- **Animation Motion Warping / Motion Matching**：UE5 原生支持高质量角色动画混合，体育游戏刚需；
- **Lyra 示例工程**：提供了完整的移动端输入、网络同步、UI 框架，可直接 fork 改造；
- 移动端 Vulkan/SM5 支持成熟，2026 年的 UE5 手游（如《原》移动版、《和平精英》UE5 版）已经证明可行。

**不用 5.0–5.3**：移动端 Lumen/Nanite 不成熟。
**不考虑 Unity 6**：我们的核心卖点是"控制台级画面"，UE5 的渲染栈在这一代更占优。

## 2. 渲染分档（关键决策）

| 档位 | 目标设备 | 光照方案 | 其他 |
|---|---|---|---|
| **High / Epic** | iPhone 15 Pro+ / Galaxy S24+ / 骁龙 8 Gen3 / 天玑 9300 | **Lumen Mobile**（实验性，但在 Adreno 7xx / Mali G7xx 上可用）+ Nanite | 全动态阴影、Niagara 观众粒子、屏幕空间反射、60fps |
| **Medium** | iPhone 12–14 / 骁龙 8 Gen1–2 / 天玑 8000+ | **预计算光照 + 静态阴影 + 局部 Lumen 反弹** | Nanite 开但压 triangles，反射降为平面反射，30/60fps 可选 |
| **Low** | 3 年内中端 Android / iPhone 11 | **完全预计算光照**，无实时光追 | 简化观众卡片/公告板、降粒子、锁 30fps |

> ⚠️ **iOS 不支持 Lumen Mobile**（Epic 官方文档明确说明）。所有 iPhone 统一走预计算光照档位，靠**烘焙 AO + 高质量角色 PBR + 动态方向性光**做出"像 Lumen"的观感。

参考：
- [Lumen on Mobile - UE5.5](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-lumen-global-illumination-on-mobile-in-unreal-engine)
- [Configuring Graphics Performance on Mobile](https://dev.epicgames.com/documentation/en-us/unreal-engine/configuring-graphics-performance-on-mobile)

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

## 4. 移动端性能预算（硬指标）

- **包体**：首发下载 ≤ 2 GB（之后资源按需下载，首包只带 1 个场馆 + 20 张球员卡）；
- **内存**：iPhone 12 / 骁龙 8 Gen1 上峰值 ≤ 2.5 GB；
- **帧率**：High 档目标 60fps（1080p），Low 档锁 30fps；
- **发热**：连续游玩 30 分钟，机身温度不超过对照组 5°C 以上（2K Mobile 被吐槽的点）；
- **启动时间**：冷启动到主菜单 ≤ 25 秒（中端机）。

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

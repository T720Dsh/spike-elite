# SPIKE ELITE 🏐

**The first console-quality, 3D, free-to-play indoor volleyball game built for mobile.**

> 工作代号：SPIKE ELITE（暂定，欢迎在 Issue 中提议正式名）
> 引擎：Unreal Engine 5 · 平台：iOS / Android（首发），PC / Console（后续）
> 定位：对标 *EA SPORTS FC Mobile* 与 *NBA 2K Mobile* 的排球品类作品

[![Status](https://img.shields.io/badge/status-pre--production-orange)]()
[![Engine](https://img.shields.io/badge/Engine-Unreal%205.5+-blue)]()
[![Platform](https://img.shields.io/badge/platform-iOS%20%7C%20Android-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

---

## 为什么要做这个游戏？

足球有 FC Mobile / eFootball，篮球有 NBA 2K Mobile，而排球——这项全球 2 亿+ 参与者、奥运会核心项目、在欧洲/南美/日本/东南亚拥有狂热观众的运动——**至今没有一款移动端 3A 水准的官方模拟作品**。

现存的排球游戏要么是 2D 街机小品（Thunder Spikes、Cyber Volleyball），要么是十年前画质粗糙的独立作品（Spike Volleyball、Volleyball Champions 3D）。这是一个被大厂完全忽略的品类真空。

**SPIKE ELITE 的目标：把 FC Mobile / NBA 2K Mobile 验证过的"真实授权球员 + 卡牌养成 + 真实时 PvP + 控制台级画面"公式，首次搬到 6v6 室内排球上。**

详细论证见 [docs/02-market-research.md](docs/02-market-research.md)。

## 核心体验

- **真实 6v6 室内排球**：严格遵循 FIVB 2025–2028 规则，rally point、顺时针轮换、5-1 / 6-2 进攻体系、Libero 自由人、拦网/吊球/后排进攻全部还原。
- **移动端原生操作**：单手可玩的滑动+点按操作体系，同时支持蓝牙手柄；手动/自动 AI 双模式（参考 NBA 2K Mobile）。
- **控制台级画面**：UE5 Nanite 几何体 + Lumen（高端机）/ 预计算光照（中低端机），动捕动画系统，动态观众与场馆氛围。
- **真实球员与球队**：首发争取国家联赛/俱乐部授权（FIVB Nations League、欧洲冠军联赛、日本 V.League 等方向），未授权期间先做"风格化原创球员 + 可解锁数据"。
- **持久化成长**：球员卡收集、阵容构建、化学反应、赛季 Pass、限时活动——FC Mobile / 2K Mobile 已验证的长线运营模型。

## 文档地图（给合作者）

| 文档 | 内容 |
|---|---|
| [01-design-philosophy.md](docs/01-design-philosophy.md) | 设计理念、产品北极星、不做什么 |
| [02-market-research.md](docs/02-market-research.md) | 对标游戏拆解、竞品分析、市场空白 |
| [03-game-design-document.md](docs/03-game-design-document.md) | GDD：玩法、模式、操作、卡牌、经济系统 |
| [04-tech-stack.md](docs/04-tech-stack.md) | UE5 技术方案、渲染分档、网络同步、目录结构 |
| [05-roadmap.md](docs/05-roadmap.md) | 里程碑路线图（Prototype → Soft Launch → Global） |
| [06-art-direction.md](docs/06-art-direction.md) | 美术风格、角色管线、场馆与 UI 方向 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 如何参与、分支规范、沟通渠道 |

## 当前状态

📌 **Pre-production / M0（概念验证阶段）**

- [x] 市场调研与品类分析
- [x] 设计理念与 GDD v0.1
- [x] 技术选型论证
- [ ] UE5 球场原型（单人 demo 球碰地、发球、扣球）
- [ ] 动捕动画清单与首版动画集
- [ ] 网络同步方案验证（Lyra / Replication Graph）

下一步具体行动见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 快速开始（给开发者）

```powershell
# 仓库目前只有设计文档；UE5 工程将在 M1 阶段加入
git clone https://github.com/T720Dsh/spike-elite.git
cd spike-elite
# 阅读 docs/04-tech-stack.md 了解引擎与分支约定
```

引擎版本：**Unreal Engine 5.5+**（建议 5.5.4 或更新），不要用 5.0–5.3（移动端 Lumen / Nanite 不成熟）。

## 路线图速览

```
M0 设计与原型 (0–3月)    → 能在 UE5 里打一场完整的 1v1 对墙/对拦网 demo
M1 垂直切片 (3–9月)      → 6v6 完整回合、AI 队友、1 个场馆、核心操作
M2 封测 (9–15月)         → PvP 局域网/WiFi、卡牌系统雏形、iOS 真机 60fps
M3 软启动 (15–20月)      → 1–2 个地区 TestFlight / 菲律宾+加拿大 Google Play
M4 全球上线 (20–28月)    → 授权球员、赛季内容、全球 PvP 匹配
```

详见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 团队在找什么人

- UE5 C++ / 蓝图工程师（游戏玩法、动画状态机、网络同步）
- 前向/运动捕捉动画师（排球专项优先）
- 体育游戏系统策划（卡牌、经济、赛季）
- 客户端图形/渲染工程师（移动端优化、 Niagara 特效）
- 后端工程师（匹配、账号、战令）
- 对排球规则熟悉的测试/社区运营

有兴趣？读 [CONTRIBUTING.md](CONTRIBUTING.md)，然后开 Issue 自我介绍。

## License

设计文档与规划：[MIT](LICENSE)。UE5 引擎与第三方资产遵循其各自许可。

---

Built with 🏐 by volleyball fans, for volleyball fans.

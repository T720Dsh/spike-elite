# SPIKE ELITE 🏐

**The first console-quality, 3D indoor volleyball game built in Unreal Engine 5 — starting on PC, coming to mobile later.**

> 工作代号：SPIKE ELITE（暂定，欢迎在 Issue 中提议正式名）
> 引擎：Unreal Engine 5
> 首发平台：**PC / Windows（Steam）**，键鼠 + 手柄
> 后续平台：iOS / Android（PC 版验证玩法后再移植）
> 定位：对标 *EA SPORTS FC* / *NBA 2K* 的排球品类作品，主打**第一人称沉浸视角** + 第三人称/俯视多视角切换

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
- **第一人称沉浸视角**（核心卖点）：你就是场上那个球员——跳起来扣球的主观冲击力、3D 音频定位、辅助雷达与轨迹预测，解决 FP 看不到全场的问题。第三人称肩后镜、战术俯视镜、直播观战镜并存。
- **PC 原生操作**：WASD + 鼠标/键鼠，完整支持 Xbox / DualShock / DualSense 手柄；手动/半自动 AI 双模式。
- **3A 级渲染**：UE5 Lumen + 硬件光追 + Nanite，动捕动画系统，动态观众与场馆氛围。
- **真实球员与球队**：首发争取国家联赛/俱乐部授权（FIVB Nations League、欧洲冠军联赛、日本 V.League 等方向），未授权期间先做"风格化原创球员 + 可解锁数据"。
- **持久化成长**：球员卡收集、阵容构建、化学反应、赛季 Pass、限时活动——FC / 2K 已验证的长线运营模型。

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

📌 **M0 进行中（PC 原型阶段）**

- [x] 市场调研与品类分析
- [x] 设计理念与 GDD v0.2（含第一人称视角设计）
- [x] 技术选型论证（首发 PC，后续手游）
- [ ] UE5 工程初始化（C++ 第三人称模板 + 第一人称角色）
- [ ] 白盒球场 + 自定义球弹道
- [ ] 动捕动画清单与首版动画集
- [ ] 网络同步方案验证（Lyra / Replication Graph）

下一步具体行动见 [docs/05-roadmap.md](docs/05-roadmap.md)。

## 快速开始（给开发者）

```powershell
# 用本机 UE5 打开工程（M1 加入 .uproject 后）
# 首次打开会提示编译 C++，点 Yes 即可
```

引擎版本：**Unreal Engine 5.5+**（具体以 `.uproject` 的 `EngineAssociation` 为准）。

## 路线图速览

```
M0 设计与 PC 原型 (0–3月)   → UE5 工程跑起来，球能在场上弹，第一人称能跳能扣
M1 PC 垂直切片 (3–9月)       → 6v6 完整回合、AI 队友、1 个场馆、第一/三人称切换
M2 PC 封测 (9–15月)          → PvP 局域网/Steam 匹配、卡牌系统雏形
M3 Steam EA (15–20月)        → Early Access 上线，真实玩家进来看手感
M4 PC 1.0 (20–28月)          → 授权球员、赛季内容、全球 PvP
M5 手游移植 (28月+)           → 分档渲染降级，iOS/Android 上线
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

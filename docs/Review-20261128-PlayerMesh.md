# 验收修复：排球角色建模太差（2026-11-28）

用户反馈："排球游戏的角色建模太差。"

## 修复

球员外观从"引擎基本体拼接的程序化人形"换成 Blender 制作的蒙皮角色。

- 网格：`/Game/Characters/SE_Player_Mesh`（art/blender/build_player.py 生成，
  FBX 导入，高约 168 cm，6 个材质槽：Skin / Jersey / Shorts / Hair / KneePad / Shoe）
- 动作：7 个导入的 AnimSequence（Idle / Run / Receive / Set / Spike / Block / Serve）
- `ASpikeEliteCharacter::ApplySkeletalBody` 在 BeginPlay 挂载骨骼网格，
  位置下移一个胶囊半高对齐脚底，yaw -90 把 Blender 的 -Y 朝前转成 UE +X 朝前。
- `UpdateSkeletalAnimation` 复用既有 `EAnimPose` 状态机选动作，跑动按速度调播放速率；
  Idle/Run 循环，击球类动作单次播放。
- `TintSkeletalBody` 按材质槽名上色，沿用原有队伍配色与 PlayerId 肤色表。
- **程序化身体保留为回退**：网格加载失败时 `console warn` 并继续使用旧几何，
  成功时只隐藏本 actor 自己的程序化部件（`GetOuter() == this`），不影响轨迹预览等组件。
- `Config/DefaultGame.ini` 增加 `/Game/Characters` 到烹饪目录，保证打包含角色资产。

## 验证（`Codex_M11g_skel_Quick42.log`）

```
PLAYER MESH skeletal=1 anims=7/7 slots=6 hidden_parts=63 id=1
mesh_lines=47          # 场上+替补 47 名球员全部换用骨骼网格
PLAYER ANIM switches=900   # 动画状态按玩法实际切换
missing=0  errors=0  match=2
```
自动化测试 **83 项全通过、0 失败**。

## 诚实边界

- 我**无法查看截图**，没有目视确认角色长相、比例或穿插；以上是日志数值。
- 动画用 AnimSingleNode 直接播放，**没有 AnimBP 混合**，动作切换是硬切而非过渡。
- 日志里 `last=0` 说明采样点多停在 Idle；未逐一确认 7 个动作都在对局中被触发。
- 未加镜头震动与按击球类型区分的音效。
- 本轮未打包新的 Shipping 版本。

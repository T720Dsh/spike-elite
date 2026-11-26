# 资产许可台账（待核验，不是商用授权证明）

本轮只读盘点，没有下载、购买、删除或重新导入资产。文件哈希只证明具体版本，不证明授权。发行前每项必须具备来源URL、作者、许可证文本及再分发条件；未知不能自动标CC0或继承代码MIT许可。

| 资产范围 | 证据/用途 | 状态与下一步 |
|---|---|---|
| Content/Textures/wood_floor_diff.jpg、wood_floor_normal.jpg及衍生材质 | setup_assets.py导入，VolleyballCourt引用M_WoodFloor | 来源URL、作者、许可未找到；需补证或合法替换，不删原文件 |
| Content/UI/T_VolleyballIcon.png及衍生贴图 | MainMenuWidget实际引用 | 来源和许可待核验 |
| Content/Mannequins下已有动画/网格 | 包括枪械/死亡类动画，名称不等于Epic资产授权证据 | 不假定是有效排球动作；需查原来源、骨骼依赖及可分发范围 |
| 程序化比赛球与音效 | Content/Balls/ASSET_LICENSE.md记载原创无品牌设计及合成音效 | 无官方商标授权，不宣传Mikasa/FIVB授权 |
| 引擎默认字体 | SEUiStyle.h使用FCoreStyle默认字体；setup_assets.py当前不导入Windows私有字体 | 按UE及其第三方通知核查发行条件；脚本顶部旧注释不是当前实际行为 |
| UE模板Manny/Quinn候选 | 本机引擎Templates下存在，尚未引入 | 来源候选Epic UE模板；许可范围/作者归属/依赖验证后才能导入，不把模板角色当最终商业运动员 |

## 可复现盘点

```powershell
D:/Python/Python313/python.exe tools/test_audit_content.py
D:/Python/Python313/python.exe tools/audit_content.py
```

工具只输出JSON（路径、字节数、SHA256、requires_documented_review），不改Content。完整运行输出可保存在Saved/Logs/M12b_content_inventory.json。两项unittest通过；未对任何未知资产授予批准。

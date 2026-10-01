# ASSET LICENSE — Ball

## 当前状态（M11d-6）

项目的"比赛用球"当前为**无品牌黄蓝白多面板球**（UE 引擎自带基本几何体 Sphere + Cylinder 组合，程序生成纯色材质，基于项目自有 `/Game/Materials/M_Tint`）。**不是** Mikasa V200W 官方授权模型，也不含 Mikasa / FIVB / Olympic 商标。

- UI 中统一显示为"比赛用球"，不冒充官方授权。
- 代码中预留了授权插槽：
  - `AVolleyballBall::LicensedBallMesh`
  - `AVolleyballBall::LicensedBallMaterial`
- 插槽行为（M11c-6 起）：仅当**两个插槽同时有效**时才应用授权模型/材质；任一缺失时自动使用无品牌占位球，不产生 Missing Package，也不冒充授权。缺失授权资产是正常状态，不会报错。
- 占位球结构（M11d-6 升级）：未缩放 Root（SceneComponent）+ 同级子组件 Sphere（Scale 0.21，直径约 21 cm）、蓝色赤道带 BandMesh 与白色经线带 BandMesh2（Cylinder，黄蓝白多面板原创设计），装饰条不再继承球体缩放而缩进球内；球随速度旋转，图案跟随可见。

若你（仓库所有者）后续提供**具有明确使用权**的 V200W 模型、BaseColor/Normal/Roughness、Logo/品牌纹理及许可说明，请放入 `Content/Balls/MikasaV200W/`，在本文件下方记录来源与许可，并将插槽指向对应资产。打包前必须确认模型和纹理被 Cook、材质不依赖编辑器专用资源。

## 许可记录

（暂无授权资产。严禁从商品网页、赛事视频、其他游戏或素材站抓取 V200W 模型/纹理/Logo。）

## 音效

本项目所有音效（含裁判哨声）均为**程序生成**（UE `USoundWaveProcedural` 合成正弦音），不来自任何赛事视频、商业游戏或素材站。若后续引入外部音效，须在此记录来源与许可。

# ASSET LICENSE — Ball

## 当前状态（M11b-6）

本项目的“比赛用球”目前为**无品牌黄蓝占位球**（UE 引擎自带基本几何体 Sphere + Cylinder 组合，
程序生成的纯色材质）。**不是** Mikasa V200W 官方授权模型，也不含 Mikasa / FIVB / Olympic 商标。

- UI 中统一显示为“比赛用球”，不冒充官方授权。
- 代码中预留了插槽：
  - `AVolleyballBall::LicensedBallMesh`
  - `AVolleyballBall::LicensedBallMaterial`
- 若你（仓库所有者）后续提供**具有明确使用权**的 V200W 模型、BaseColor/Normal/Roughness、
  Logo/品牌纹理及许可说明，请放入 `Content/Balls/MikasaV200W/`，在本文件下方记录来源与许可，
  并将插槽指向对应资产。打包前必须确认模型和纹理被 Cook、材质不依赖编辑器专用资源。

## 许可记录

（暂无授权资产。严禁从商品网页、赛事视频、其他游戏或素材站抓取 V200W 模型/纹理/Logo。）

## 音效

本项目所有音效（含裁判哨声）均为**程序生成**（UE `USoundWaveProcedural` 合成正弦音），
不来自任何赛事视频、商业游戏或素材站。若后续引入外部音效，须在此记录来源与许可。

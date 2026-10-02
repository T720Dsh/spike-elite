#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# One-time, repeatable editor asset setup for SPIKE ELITE.
# Run with the UE editor (NOT -game):
#   UnrealEditor.exe SpikeElite.uproject -ExecutePythonScript="<this file>" -unattended -nosplash
#
# It:
#   1. imports the volleyball icon, wood floor textures and the Microsoft YaHei font
#   2. builds a wood-floor material and a plain sport-floor material
#   3. builds a Chinese FontAsset (/Game/UI/Font_CN)
#   4. creates an empty indoor base level /Game/Maps/Arena with a PlayerStart
#
# Everything is idempotent (replace_existing / load-or-create).

import os
import unreal

PROJ = r"D:\projects\spike-elite"
LOG_PREFIX = "[SETUP_ASSETS] "


def log(msg):
    unreal.log(LOG_PREFIX + str(msg))


def warn(msg):
    unreal.log_warning(LOG_PREFIX + str(msg))


ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


def import_task(filename, dest_path, dest_name=None):
    if not os.path.exists(filename):
        warn("missing source file: " + filename)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if dest_name:
        task.set_editor_property("destination_name", dest_name)
    ASSET_TOOLS.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    log("imported %s -> %s" % (os.path.basename(filename), paths))
    return paths[0] if paths else None


def load_or_create_material(full_path):
    mat = unreal.load_asset(full_path)
    if not mat:
        name = full_path.split("/")[-1]
        folder = os.path.dirname(full_path).replace("/Game", "/Game")
        mat = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    # Runtime ISM components (net grid, stand steps, crowd) need this usage
    # flag baked in; otherwise a standalone build falls back to the default grey
    # material because it cannot recompile on the target.
    try:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception as e:
        warn("could not set ISM usage on %s: %r" % (full_path, e))
    return mat


def make_constant(mat, value, x, y):
    c = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", float(value))
    return c


def build_wood_material(diff_tex, normal_tex):
    mat = load_or_create_material("/Game/Materials/M_WoodFloor")
    # wipe existing expressions for a clean rebuild
    try:
        for expr in list(mat.get_editor_property("expression_collection").expressions):
            unreal.MaterialEditingLibrary.delete_material_expression(mat, expr)
    except Exception:
        pass

    # UV coordinates with tiling so the plank scale reads correctly.
    uv = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureCoordinate, -700, 0)
    uv.set_editor_property("utiling", 6.0)
    uv.set_editor_property("vtiling", 6.0)

    ts = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSample, -450, -120)
    ts.set_editor_property("texture", diff_tex)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", ts, "UVs")
    unreal.MaterialEditingLibrary.connect_material_property(
        ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    tsn = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSample, -450, 180)
    tsn.set_editor_property("texture", normal_tex)
    try:
        tsn.set_editor_property("sampler_type",
                               unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    except Exception as e:
        warn("normal sampler type set failed: %s" % e)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", tsn, "UVs")
    unreal.MaterialEditingLibrary.connect_material_property(
        tsn, "RGB", unreal.MaterialProperty.MP_NORMAL)

    rough = make_constant(mat, 0.45, -450, 380)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(mat)
    EAL.save_asset("/Game/Materials/M_WoodFloor", only_if_is_dirty=False)
    log("built M_WoodFloor")


def build_solid_material(full_path, color, roughness=0.7):
    mat = load_or_create_material(full_path)
    try:
        for expr in list(mat.get_editor_property("expression_collection").expressions):
            unreal.MaterialEditingLibrary.delete_material_expression(mat, expr)
    except Exception:
        pass
    rgb = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    rgb.set_editor_property("constant", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = make_constant(mat, roughness, -300, 200)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    EAL.save_asset(full_path, only_if_is_dirty=False)
    log("built " + full_path)


def build_tint_material(full_path="/Game/Materials/M_Tint"):
    """Opaque material with a single 'Color' vector parameter so C++ can create
    MIDs and tint basic shapes reliably (BasicShapeMaterial has no guaranteed
    tint parameter at runtime, which left actors on the fallback grid material)."""
    mat = load_or_create_material(full_path)
    try:
        for expr in list(mat.get_editor_property("expression_collection").expressions):
            unreal.MaterialEditingLibrary.delete_material_expression(mat, expr)
    except Exception:
        pass
    param = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    param.set_editor_property("parameter_name", "Color")
    param.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = make_constant(mat, 0.55, -300, 220)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # A slight metallic=0 / specular default keeps colours readable under spots.
    unreal.MaterialEditingLibrary.recompile_material(mat)
    EAL.save_asset(full_path, only_if_is_dirty=False)
    log("built " + full_path)


def build_crowd_material(full_path="/Game/Materials/M_Crowd"):
    """Like M_Tint but feeds a dimmed copy of 'Color' into emissive so distant
    spectators keep readable clothing colour in the dark stands."""
    mat = load_or_create_material(full_path)
    try:
        for expr in list(mat.get_editor_property("expression_collection").expressions):
            unreal.MaterialEditingLibrary.delete_material_expression(mat, expr)
    except Exception:
        pass
    param = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -380, 0)
    param.set_editor_property("parameter_name", "Color")
    param.set_editor_property("default_value", unreal.LinearColor(0.6, 0.6, 0.6, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = make_constant(mat, 0.6, -380, 240)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mult = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -60, 120)
    em_k = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -380, 120)
    em_k.set_editor_property("parameter_name", "Em")
    em_k.set_editor_property("default_value", 0.06)
    unreal.MaterialEditingLibrary.connect_material_expressions(param, "", mult, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(em_k, "", mult, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    EAL.save_asset(full_path, only_if_is_dirty=False)
    log("built " + full_path)


def build_ball_material(full_path="/Game/Materials/M_TintBall"):
    """Match-ball material: a runtime texture (panel art, no trademarks) times a
    'Color' vector. M11f-4: replaces the old sphere + cylinder-band look with a
    genuine multi-panel volleyball surface driven by a UV texture, so the panel
    seams rotate with the ball and nothing floats outside the sphere."""
    mat = load_or_create_material(full_path)
    try:
        for expr in list(mat.get_editor_property("expression_collection").expressions):
            unreal.MaterialEditingLibrary.delete_material_expression(mat, expr)
    except Exception:
        pass
    ts = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureObjectParameter, -620, -80)
    ts.set_editor_property("parameter_name", "BallTexture")
    # A real default texture keeps the shader valid at compile time (a NULL
    # TextureObjectParameter makes the material compile fail and fall back to
    # the engine grid). The runtime MID replaces it with the panel texture.
    icon = unreal.load_asset("/Game/UI/T_VolleyballIcon")
    if icon:
        ts.set_editor_property("texture", icon)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSample, -420, -80)
    uv = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureCoordinate, -620, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", sample, "UVs")
    unreal.MaterialEditingLibrary.connect_material_expressions(ts, "", sample, "Tex")
    color = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -420, 120)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mult = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -120, 20)
    unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", mult, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", mult, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = make_constant(mat, 0.45, -420, 300)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    EAL.save_asset(full_path, only_if_is_dirty=False)
    log("built " + full_path)


def build_level():
    maps_dir = "/Game/Maps"
    if not unreal.EditorAssetLibrary.does_directory_exist(maps_dir):
        unreal.EditorAssetLibrary.make_directory(maps_dir)
    unreal.EditorLevelLibrary.new_level("/Game/Maps/Arena")

    # Remove any auto-added outdoor actors so the level has no sky/sun/clouds.
    remove_types = []
    for cls_name in ("SkyAtmosphere", "SkyLight", "DirectionalLight",
                     "ExponentialHeightFog", "VolumetricCloud"):
        cls = getattr(unreal, cls_name, None)
        if cls is not None:
            remove_types.append(cls)
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if any(isinstance(actor, t) for t in remove_types):
            unreal.EditorLevelLibrary.destroy_actor(actor)

    # PlayerStart above where the runtime court floor will be.
    existing = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                if isinstance(a, unreal.PlayerStart)]
    if not existing:
        unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, -90, 0))

    unreal.EditorLevelLibrary.save_current_level()
    log("created /Game/Maps/Arena")


def main():
    log("=== SPIKE ELITE asset setup start ===")
    EAL.make_directory("/Game/Materials")
    EAL.make_directory("/Game/Maps")
    EAL.make_directory("/Game/UI")

    # 1. Icon + wood textures
    import_task(os.path.join(PROJ, r"Content\UI\T_VolleyballIcon.png"), "/Game/UI",
                "T_VolleyballIcon")
    diff_path = import_task(os.path.join(PROJ, r"Content\Textures\wood_floor_diff.jpg"),
                            "/Game/Textures", "wood_floor_diff")
    norm_path = import_task(os.path.join(PROJ, r"Content\Textures\wood_floor_normal.jpg"),
                            "/Game/Textures", "wood_floor_normal")
    diff_tex = unreal.load_asset("/Game/Textures/wood_floor_diff")
    norm_tex = unreal.load_asset("/Game/Textures/wood_floor_normal")

    # 2. Materials
    if diff_tex and norm_tex:
        build_wood_material(diff_tex, norm_tex)
    else:
        warn("wood textures missing, skipping wood material")
    build_solid_material("/Game/Materials/M_SportFloor", (0.22, 0.14, 0.09), 0.7)
    build_tint_material("/Game/Materials/M_Tint")
    build_crowd_material("/Game/Materials/M_Crowd")
    build_ball_material("/Game/Materials/M_TintBall")

    # 3. Chinese font face. The Slate-ready runtime UFont is assembled in C++
    #    (SEUiStyle::ChineseFont) because Python cannot write FCompositeFont's
    #    protected DefaultTypeface; we only need the UFontFace asset here.
    #    UI uses Unreal's runtime composite font (including CJK fallback).
    #    Do not import proprietary fonts from a developer's Windows install.
    pass

    # 4. Indoor base level
    try:
        build_level()
    except Exception as e:
        warn("level setup failed: %r" % e)

    log("=== SPIKE ELITE asset setup done ===")
    unreal.SystemLibrary.quit_editor()


main()

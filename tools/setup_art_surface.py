"""Create one original PBR surface; never modify existing game assets.

Run inside UnrealEditor with -ExecutePythonScript=<absolute path>.
The material is always cooked via /Game/Materials. Source-generated meshes
remain transient: no third-party models, logos, textures, or licensing imports.
"""
import unreal

path = "/Game/Materials/M_ArtSurface"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_ArtSurface", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    lib = unreal.MaterialEditingLibrary
    color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(.2, .3, .4, 1))
    lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for index, (name, value, prop) in enumerate([
        ("Roughness", .78, unreal.MaterialProperty.MP_ROUGHNESS),
        ("Metallic", 0., unreal.MaterialProperty.MP_METALLIC),
        ("Specular", .32, unreal.MaterialProperty.MP_SPECULAR),
    ]):
        param = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 150 + index*100)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        lib.connect_material_property(param, "", prop)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("ART_SURFACE_CREATED " + path)
else:
    unreal.log("ART_SURFACE_EXISTS: existing asset preserved")
unreal.SystemLibrary.quit_editor()

# SPDX-License-Identifier: MIT
"""Headless import of the Blender-authored volleyball athlete into /Game/Characters.

Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script=tools/import_characters.py
      -NullRHI -unattended -nosplash -ABSLOG=<log>

Imports art/export/SE_Player_Mesh.fbx as a SkeletalMesh + Skeleton, then every
SE_Anim_*.fbx against that skeleton. Existing assets are replaced in place; no
other content is touched and nothing is deleted.
"""
import os
import unreal

PROJECT_DIR = unreal.Paths.project_dir()
EXPORT_DIR = os.path.abspath(os.path.join(PROJECT_DIR, "art", "export"))
DEST = "/Game/Characters"
MESH_FBX = os.path.join(EXPORT_DIR, "SE_Player_Mesh.fbx")
ANIMS = ["Idle", "Run", "Receive", "Set", "Spike", "Block", "Serve"]


def build_task(filename, destination, options, asset_name):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = options
    return task


def mesh_options():
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("import_materials", True)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("create_physics_asset", True)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = opts.skeletal_mesh_import_data
    sk.set_editor_property("import_content_type", unreal.FBXImportContentType.FBXICT_ALL)
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("force_front_x_axis", False)
    sk.set_editor_property("import_morph_targets", False)
    return opts


def anim_options(skeleton):
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", False)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("import_animations", True)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("create_physics_asset", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    opts.set_editor_property("skeleton", skeleton)
    anim = opts.anim_sequence_import_data
    anim.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim.set_editor_property("import_bone_tracks", True)
    anim.set_editor_property("remove_redundant_keys", False)
    anim.set_editor_property("convert_scene", True)
    anim.set_editor_property("force_front_x_axis", False)
    return opts


def run():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    missing = [p for p in [MESH_FBX] + [os.path.join(EXPORT_DIR, "SE_Anim_%s.fbx" % a) for a in ANIMS]
               if not os.path.isfile(p)]
    if missing:
        unreal.log_error("IMPORT missing source FBX: %s" % missing)
        return 1

    tools.import_asset_tasks([build_task(MESH_FBX, DEST, mesh_options(), "SE_Player_Mesh")])
    mesh = unreal.load_asset("%s/SE_Player_Mesh" % DEST)
    if not mesh:
        unreal.log_error("IMPORT skeletal mesh missing after import")
        return 1
    skeleton = mesh.skeleton
    unreal.log("IMPORT mesh=%s skeleton=%s materials=%d" % (
        mesh.get_path_name(), skeleton.get_path_name(), len(mesh.materials)))

    anim_tasks = []
    for name in ANIMS:
        src = os.path.join(EXPORT_DIR, "SE_Anim_%s.fbx" % name)
        anim_tasks.append(build_task(src, DEST + "/Anims", anim_options(skeleton), "SE_Anim_%s" % name))
    tools.import_asset_tasks(anim_tasks)

    ok = True
    for name in ANIMS:
        path = "%s/Anims/SE_Anim_%s" % (DEST, name)
        seq = unreal.load_asset(path)
        if not seq:
            unreal.log_error("IMPORT anim missing: %s" % path)
            ok = False
            continue
        unreal.log("IMPORT anim=%s frames=%s length=%.3f skeleton=%s" % (
            path, seq.get_editor_property("number_of_sampled_frames"),
            seq.get_play_length(), seq.get_editor_property("skeleton").get_path_name()))
    unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    unreal.log("IMPORT RESULT=%s" % ("OK" if ok else "FAIL"))
    return 0 if ok else 1


run()

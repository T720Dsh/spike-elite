# SPDX-License-Identifier: MIT
"""SPIKE ELITE - stylized volleyball athlete generator (Blender 5.2.1, headless).

Usage:
  blender.exe -b --factory-startup --python art/blender/build_player.py -- --out <dir>

Produces (meters, Z-up, character faces -Y so the FBX pipeline yields UE +X forward):
  SE_Player_Mesh.fbx      skeletal mesh + armature in rest pose
  SE_Anim_<Action>.fbx     armature-only animation takes

Everything is generated procedurally from primitives: no external or
copyrighted assets, no character likeness. Material slots are authored in a
fixed order so the UE side can tint team colour variants per slot:
  0 Skin  1 Hair  2 Jersey  3 Shorts  4 KneePad  5 Shoe
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix, Euler

# ---------------------------------------------------------------- parameters
HEIGHT = 1.68          # matches the UE capsule (half height 84 cm)
SHOULDER_Z = 1.38
HIP_Z = 0.95
ARM_Y = 0.0
SLOT = {"Skin": 0, "Hair": 1, "Jersey": 2, "Shorts": 3, "KneePad": 4, "Shoe": 5}
SLOT_COLOR = {
    "Skin": (0.80, 0.60, 0.45, 1.0),
    "Hair": (0.07, 0.05, 0.05, 1.0),
    "Jersey": (0.10, 0.45, 0.95, 1.0),
    "Shorts": (0.06, 0.16, 0.38, 1.0),
    "KneePad": (0.10, 0.11, 0.13, 1.0),
    "Shoe": (0.93, 0.94, 0.96, 1.0),
}


def argv_value(flag, default):
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if flag in args:
        return args[args.index(flag) + 1]
    return default


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    sc.render.fps = 30


def make_materials():
    mats = []
    for name, idx in sorted(SLOT.items(), key=lambda kv: kv[1]):
        m = bpy.data.materials.new("SEP_" + name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = SLOT_COLOR[name]
            if "Roughness" in bsdf.inputs:
                bsdf.inputs["Roughness"].default_value = 0.62
        mats.append(m)
    return mats


# ---------------------------------------------------------------- primitives
def add_shape(kind, name, loc, scale, rot=(0, 0, 0), slot="Skin", segments=20):
    if kind == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=segments // 2,
                                             radius=1.0, location=loc)
    elif kind == "cylinder":
        bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=1.0, depth=2.0, location=loc)
    else:
        bpy.ops.mesh.primitive_cube_add(size=2.0, location=loc)
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = scale
    ob.rotation_euler = Euler([math.radians(a) for a in rot], "XYZ")
    ob["se_slot"] = slot
    return ob


def limb(name, top, bottom, r_top, r_bottom, slot):
    """Tapered capsule-ish segment between two points (approximated by a cylinder
    plus end spheres so the subdivided silhouette stays smooth)."""
    top, bottom = Vector(top), Vector(bottom)
    mid = (top + bottom) * 0.5
    d = top - bottom
    length = d.length
    rot = d.to_track_quat("Z", "Y").to_euler("XYZ")
    cyl = add_shape("cylinder", name, mid, ((r_top + r_bottom) * 0.5, (r_top + r_bottom) * 0.5, length * 0.5),
                    slot=slot, segments=16)
    cyl.rotation_euler = rot
    a = add_shape("sphere", name + "_a", top, (r_top, r_top, r_top), slot=slot, segments=14)
    b = add_shape("sphere", name + "_b", bottom, (r_bottom, r_bottom, r_bottom), slot=slot, segments=14)
    return [cyl, a, b]


def build_body():
    parts = []
    # head + hair + simple facial volumes (anime-leaning: big smooth head, no texture)
    parts.append(add_shape("sphere", "Head", (0, 0, 1.555), (0.098, 0.105, 0.112), slot="Skin", segments=28))
    hair = add_shape("sphere", "Hair", (0, 0.012, 1.575), (0.106, 0.112, 0.113), slot="Hair", segments=28)
    parts.append(hair)
    parts.append(add_shape("sphere", "HairFringe", (0, -0.062, 1.60), (0.095, 0.055, 0.07), slot="Hair", segments=20))
    parts.append(add_shape("sphere", "Nose", (0, -0.098, 1.545), (0.016, 0.018, 0.016), slot="Skin", segments=12))
    for s in (-1, 1):
        parts.append(add_shape("sphere", "Ear%d" % s, (s * 0.098, 0.004, 1.552), (0.016, 0.022, 0.03), slot="Skin", segments=12))
    # neck, torso (jersey), waist, shorts
    parts += limb("Neck", (0, 0, 1.50), (0, 0, 1.40), 0.047, 0.057, "Skin")
    parts.append(add_shape("cube", "Chest", (0, 0, 1.285), (0.145, 0.088, 0.115), slot="Jersey"))
    parts.append(add_shape("cube", "Waist", (0, 0, 1.10), (0.115, 0.078, 0.095), slot="Jersey"))
    parts.append(add_shape("cube", "Shorts", (0, 0, 0.945), (0.135, 0.095, 0.105), slot="Shorts"))
    for s in (-1, 1):
        # shoulders / arms
        parts.append(add_shape("sphere", "Shoulder%d" % s, (s * 0.145, 0, SHOULDER_Z - 0.02), (0.062, 0.062, 0.062), slot="Jersey", segments=16))
        parts.append(add_shape("sphere", "Sleeve%d" % s, (s * 0.165, 0, 1.30), (0.055, 0.055, 0.07), slot="Jersey", segments=16))
        parts += limb("UpperArm%d" % s, (s * 0.17, ARM_Y, SHOULDER_Z - 0.02), (s * 0.17, ARM_Y, 1.12), 0.044, 0.038, "Skin")
        parts += limb("LowerArm%d" % s, (s * 0.17, ARM_Y, 1.12), (s * 0.17, ARM_Y, 0.90), 0.038, 0.031, "Skin")
        parts.append(add_shape("sphere", "Hand%d" % s, (s * 0.17, -0.005, 0.855), (0.036, 0.052, 0.058), slot="Skin", segments=16))
        # legs
        parts += limb("Thigh%d" % s, (s * 0.085, 0, HIP_Z - 0.03), (s * 0.09, 0, 0.52), 0.072, 0.055, "Skin")
        parts.append(add_shape("cube", "ShortsLeg%d" % s, (s * 0.088, 0, 0.845), (0.072, 0.082, 0.085), slot="Shorts"))
        parts += limb("Calf%d" % s, (s * 0.09, 0, 0.52), (s * 0.09, 0, 0.12), 0.055, 0.038, "Skin")
        parts.append(add_shape("sphere", "KneePad%d" % s, (s * 0.09, -0.012, 0.515), (0.063, 0.062, 0.072), slot="KneePad", segments=16))
        parts.append(add_shape("cube", "Shoe%d" % s, (s * 0.09, -0.045, 0.045), (0.05, 0.105, 0.045), slot="Shoe"))
        parts.append(add_shape("cube", "Sock%d" % s, (s * 0.09, 0, 0.145), (0.046, 0.046, 0.05), slot="Shoe"))
    return parts


def join_body(parts, mats):
    for ob in parts:
        bpy.context.view_layer.objects.active = ob
        ob.select_set(False)
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        # one material per part, slot order fixed later by remap
        ob.data.materials.clear()
        ob.data.materials.append(mats[SLOT[ob["se_slot"]]])
    bpy.ops.object.select_all(action="DESELECT")
    for ob in parts:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    body = bpy.context.active_object
    body.name = "SE_Player"
    body.data.name = "SE_PlayerMesh"
    # rebuild material slots in canonical order
    body.data.materials.clear()
    for m in mats:
        body.data.materials.append(m)
    index_of = {m.name: i for i, m in enumerate(mats)}
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.to_mesh(body.data)
    bm.free()
    # re-point polygon material indices (join scrambled them)
    return body, index_of


def assign_slots_by_region(body):
    """After joining, re-assign material indices from geometry regions so every
    polygon lands in the canonical slot (robust against join ordering)."""
    me = body.data
    for poly in me.polygons:
        c = poly.center
        x, y, z = c.x, c.y, c.z
        slot = SLOT["Skin"]
        if z > 1.60 or (z > 1.50 and y > 0.0 and abs(x) < 0.12 and z > 1.56):
            slot = SLOT["Hair"]
        if 1.02 < z < 1.42 and abs(x) < 0.20 and abs(y) < 0.13:
            slot = SLOT["Jersey"]
        if 1.26 < z < 1.40 and 0.12 < abs(x) < 0.24:
            slot = SLOT["Jersey"]
        if 0.80 < z <= 1.02:
            slot = SLOT["Shorts"]
        if 0.46 < z < 0.58 and abs(x) > 0.03:
            slot = SLOT["KneePad"]
        if z < 0.20:
            slot = SLOT["Shoe"]
        poly.material_index = slot
    me.update()


# ---------------------------------------------------------------- armature
BONES = [
    # name, head, tail, parent
    ("root", (0, 0, 0.0), (0, 0, 0.12), None),
    ("pelvis", (0, 0, 0.95), (0, 0, 1.06), "root"),
    ("spine_01", (0, 0, 1.06), (0, 0, 1.20), "pelvis"),
    ("spine_02", (0, 0, 1.20), (0, 0, 1.38), "spine_01"),
    ("neck_01", (0, 0, 1.38), (0, 0, 1.48), "spine_02"),
    ("head", (0, 0, 1.48), (0, 0, 1.66), "neck_01"),
]
for s, tag in ((-1, "_r"), (1, "_l")):
    BONES += [
        ("clavicle" + tag, (s * 0.04, 0, 1.345), (s * 0.17, 0, 1.36), "spine_02"),
        ("upperarm" + tag, (s * 0.17, 0, 1.36), (s * 0.17, 0, 1.12), "clavicle" + tag),
        ("lowerarm" + tag, (s * 0.17, 0, 1.12), (s * 0.17, 0, 0.90), "upperarm" + tag),
        ("hand" + tag, (s * 0.17, 0, 0.90), (s * 0.17, -0.02, 0.83), "lowerarm" + tag),
        ("thigh" + tag, (s * 0.088, 0, 0.92), (s * 0.09, 0, 0.52), "pelvis"),
        ("calf" + tag, (s * 0.09, 0, 0.52), (s * 0.09, 0, 0.13), "thigh" + tag),
        ("foot" + tag, (s * 0.09, 0, 0.13), (s * 0.09, -0.11, 0.04), "calf" + tag),
    ]


def build_armature():
    bpy.ops.object.armature_add(location=(0, 0, 0))
    arm = bpy.context.active_object
    arm.name = "SE_PlayerRig"
    arm.data.name = "SE_PlayerArmature"
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    for b in list(eb):
        eb.remove(b)
    created = {}
    for name, head, tail, parent in BONES:
        bone = eb.new(name)
        bone.head = Vector(head)
        bone.tail = Vector(tail)
        bone.use_connect = False
        created[name] = bone
    for name, _h, _t, parent in BONES:
        if parent:
            created[name].parent = created[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def bind(body, arm):
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    mod = body.modifiers.new("Subdivision", "SUBSURF")
    mod.levels = 1
    mod.render_levels = 1
    bpy.ops.object.shade_smooth()


# ---------------------------------------------------------------- posing
def world_rot(pb, rots):
    """Apply a rotation expressed in WORLD axes to a pose bone.

    rots is a list of (axis, degrees); they compose in order. The conversion
    through the bone's rest matrix keeps the authoring intuitive: a volleyball
    arm swing is 'rotate the upper arm about world X', independent of how the
    bone happens to be oriented."""
    rest = pb.bone.matrix_local.to_3x3()
    m = Matrix.Identity(3)
    for axis, deg in rots:
        m = Matrix.Rotation(math.radians(deg), 3, axis) @ m
    local = rest.inverted() @ m @ rest
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = local.to_quaternion()


def apply_pose(arm, pose):
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    for bone_name, rots in pose.items():
        if bone_name == "_root_z":
            arm.pose.bones["root"].location = (0, 0, rots)
            continue
        pb = arm.pose.bones.get(bone_name)
        if pb:
            world_rot(pb, rots)


def key_pose(arm, frame):
    for pb in arm.pose.bones:
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        pb.keyframe_insert("location", frame=frame)


# Pose vocabulary. X = character's left-right axis, so a rotation about world X
# swings a limb in the sagittal plane; the athlete faces -Y.
def P(**kw):
    return kw


def stance(knee, lean=0.0):
    return {
        "pelvis": [("X", lean), ("Z", 0)],
        "thigh_l": [("X", -knee * 0.55)], "thigh_r": [("X", -knee * 0.55)],
        "calf_l": [("X", knee)], "calf_r": [("X", knee)],
        "foot_l": [("X", -knee * 0.45)], "foot_r": [("X", -knee * 0.45)],
        "spine_01": [("X", lean * 0.5)], "spine_02": [("X", lean * 0.5)],
    }


def merge(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


ARM_DOWN_REST = 0.0


def arms(l_rot, r_rot, l_elbow=0.0, r_elbow=0.0):
    return {
        "upperarm_l": l_rot, "upperarm_r": r_rot,
        "lowerarm_l": [("X", l_elbow)], "lowerarm_r": [("X", r_elbow)],
    }


ACTIONS = {}

# idle / ready stance: feet planted, knees soft, hands up at chest height
ACTIONS["Idle"] = {
    "loop": True, "length": 48,
    "keys": [
        (1, merge(stance(16, 6), arms([("X", -22)], [("X", -22)], -55, -55))),
        (24, merge(stance(22, 9), arms([("X", -28)], [("X", -28)], -62, -62))),
        (48, merge(stance(16, 6), arms([("X", -22)], [("X", -22)], -55, -55))),
    ],
}
# run cycle: contralateral arm/leg swing
ACTIONS["Run"] = {
    "loop": True, "length": 24,
    "keys": [
        (1, merge(stance(10, 14), {
            "thigh_l": [("X", -45)], "calf_l": [("X", 55)], "foot_l": [("X", -15)],
            "thigh_r": [("X", 30)], "calf_r": [("X", 12)], "foot_r": [("X", 10)],
            "upperarm_l": [("X", 42)], "lowerarm_l": [("X", -70)],
            "upperarm_r": [("X", -48)], "lowerarm_r": [("X", -70)],
        })),
        (7, merge(stance(14, 14), {
            "thigh_l": [("X", -8)], "calf_l": [("X", 20)],
            "thigh_r": [("X", -6)], "calf_r": [("X", 30)],
            "upperarm_l": [("X", 0)], "lowerarm_l": [("X", -60)],
            "upperarm_r": [("X", 0)], "lowerarm_r": [("X", -60)],
        })),
        (13, merge(stance(10, 14), {
            "thigh_r": [("X", -45)], "calf_r": [("X", 55)], "foot_r": [("X", -15)],
            "thigh_l": [("X", 30)], "calf_l": [("X", 12)], "foot_l": [("X", 10)],
            "upperarm_r": [("X", 42)], "lowerarm_r": [("X", -70)],
            "upperarm_l": [("X", -48)], "lowerarm_l": [("X", -70)],
        })),
        (19, merge(stance(14, 14), {
            "thigh_r": [("X", -8)], "calf_r": [("X", 20)],
            "thigh_l": [("X", -6)], "calf_l": [("X", 30)],
            "upperarm_l": [("X", 0)], "lowerarm_l": [("X", -60)],
            "upperarm_r": [("X", 0)], "lowerarm_r": [("X", -60)],
        })),
        (24, merge(stance(10, 14), {
            "thigh_l": [("X", -45)], "calf_l": [("X", 55)], "foot_l": [("X", -15)],
            "thigh_r": [("X", 30)], "calf_r": [("X", 12)], "foot_r": [("X", 10)],
            "upperarm_l": [("X", 42)], "lowerarm_l": [("X", -70)],
            "upperarm_r": [("X", -48)], "lowerarm_r": [("X", -70)],
        })),
    ],
}
# bump / receive (垫球): deep knees, straight arms joined low in front
ACTIONS["Receive"] = {
    "loop": False, "length": 22,
    "keys": [
        (1, merge(stance(30, 18), arms([("X", -40)], [("X", -40)], -20, -20))),
        (9, merge(stance(46, 26), {
            "upperarm_l": [("X", -62), ("Y", 14)], "upperarm_r": [("X", -62), ("Y", -14)],
            "lowerarm_l": [("X", 0)], "lowerarm_r": [("X", 0)],
        })),
        (22, merge(stance(34, 20), arms([("X", -48)], [("X", -48)], -10, -10))),
    ],
}
# set (二传): hands above forehead, elbows out, legs extend
ACTIONS["Set"] = {
    "loop": False, "length": 22,
    "keys": [
        (1, merge(stance(26, 6), arms([("X", -120), ("Y", 20)], [("X", -120), ("Y", -20)], -80, -80))),
        (9, merge(stance(12, 2), arms([("X", -150), ("Y", 16)], [("X", -150), ("Y", -16)], -35, -35))),
        (22, merge(stance(20, 5), arms([("X", -120), ("Y", 18)], [("X", -120), ("Y", -18)], -70, -70))),
    ],
}
# spike / jump attack: windup, airborne contact, follow-through
ACTIONS["Spike"] = {
    "loop": False, "length": 30,
    "keys": [
        (1, merge(stance(42, 22), {
            "upperarm_r": [("X", 60)], "lowerarm_r": [("X", -30)],
            "upperarm_l": [("X", -70)], "lowerarm_l": [("X", -20)],
            "_root_z": 0.0,
        })),
        (10, merge(stance(6, -12), {
            "upperarm_r": [("X", 170), ("Y", -18)], "lowerarm_r": [("X", -95)],
            "upperarm_l": [("X", -165), ("Y", 10)], "lowerarm_l": [("X", -10)],
            "spine_02": [("X", -16)], "thigh_l": [("X", 18)], "thigh_r": [("X", -22)],
            "calf_l": [("X", 35)], "calf_r": [("X", 20)],
            "_root_z": 0.42,
        })),
        (16, merge(stance(4, 10), {
            "upperarm_r": [("X", -178), ("Y", -8)], "lowerarm_r": [("X", -6)],
            "upperarm_l": [("X", -110)], "lowerarm_l": [("X", -40)],
            "spine_02": [("X", 12)], "thigh_l": [("X", -14)], "thigh_r": [("X", 10)],
            "_root_z": 0.46,
        })),
        (23, merge(stance(18, 20), {
            "upperarm_r": [("X", -60)], "lowerarm_r": [("X", -35)],
            "upperarm_l": [("X", -40)], "lowerarm_l": [("X", -45)],
            "_root_z": 0.06,
        })),
        (30, merge(stance(26, 12), arms([("X", -25)], [("X", -25)], -55, -55))),
    ],
}
# block: both arms straight overhead, short hop
ACTIONS["Block"] = {
    "loop": False, "length": 26,
    "keys": [
        (1, merge(stance(34, 8), arms([("X", -95)], [("X", -95)], -45, -45))),
        (9, merge(stance(4, -4), {
            "upperarm_l": [("X", -172), ("Y", 8)], "upperarm_r": [("X", -172), ("Y", -8)],
            "lowerarm_l": [("X", -4)], "lowerarm_r": [("X", -4)],
            "_root_z": 0.34,
        })),
        (16, merge(stance(6, -2), {
            "upperarm_l": [("X", -176), ("Y", 6)], "upperarm_r": [("X", -176), ("Y", -6)],
            "lowerarm_l": [("X", -2)], "lowerarm_r": [("X", -2)],
            "_root_z": 0.30,
        })),
        (26, merge(stance(30, 10), arms([("X", -70)], [("X", -70)], -50, -50))),
    ],
}
# serve: toss with the left hand, right arm swings through
ACTIONS["Serve"] = {
    "loop": False, "length": 34,
    "keys": [
        (1, merge(stance(18, 4), arms([("X", -60)], [("X", -30)], -40, -40))),
        (10, merge(stance(14, -4), {
            "upperarm_l": [("X", -165)], "lowerarm_l": [("X", -6)],
            "upperarm_r": [("X", 75)], "lowerarm_r": [("X", -60)],
        })),
        (18, merge(stance(8, -12), {
            "upperarm_l": [("X", -120)], "lowerarm_l": [("X", -20)],
            "upperarm_r": [("X", 150), ("Y", -20)], "lowerarm_r": [("X", -90)],
            "spine_02": [("X", -14)],
        })),
        (24, merge(stance(10, 12), {
            "upperarm_l": [("X", -60)], "lowerarm_l": [("X", -40)],
            "upperarm_r": [("X", -172), ("Y", -6)], "lowerarm_r": [("X", -8)],
            "spine_02": [("X", 10)],
        })),
        (34, merge(stance(18, 8), arms([("X", -30)], [("X", -35)], -50, -50))),
    ],
}


def iter_fcurves(action):
    """Blender 5 replaced Action.fcurves with layered/slotted channelbags."""
    if hasattr(action, "fcurves"):
        for fc in action.fcurves:
            yield fc
        return
    for layer in action.layers:
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []):
                for fc in bag.fcurves:
                    yield fc


def author_actions(arm):
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    made = {}
    for name, spec in ACTIONS.items():
        action = bpy.data.actions.new("SE_" + name)
        arm.animation_data_create()
        arm.animation_data.action = action
        for frame, pose in spec["keys"]:
            apply_pose(arm, pose)
            key_pose(arm, frame)
        for fc in iter_fcurves(action):
            for kp in fc.keyframe_points:
                kp.interpolation = "BEZIER"
        action.use_fake_user = True
        made[name] = action
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.animation_data.action = None
    return made


# ---------------------------------------------------------------- verification
PROBES = ["hand_l", "hand_r", "head", "foot_l", "foot_r"]


def probe(arm, action, frame):
    arm.animation_data_create()
    arm.animation_data.action = action
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    out = {}
    for name in PROBES:
        pb = arm.pose.bones[name]
        out[name] = tuple(round(v, 3) for v in (arm.matrix_world @ pb.tail))
    arm.animation_data.action = None
    return out


# ---------------------------------------------------------------- export
def export_mesh(body, arm, path):
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False, bake_anim=False, mesh_smooth_type="FACE",
        use_mesh_modifiers=True, apply_unit_scale=True, global_scale=1.0,
        apply_scale_options="FBX_SCALE_NONE", path_mode="COPY",
        primary_bone_axis="Y", secondary_bone_axis="X",
    )


def export_action(arm, action, path, length):
    arm.animation_data_create()
    arm.animation_data.action = action
    sc = bpy.context.scene
    sc.frame_start = 1
    sc.frame_end = length
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE"},
        add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
        bake_anim_force_startend_keying=True,
        apply_unit_scale=True, global_scale=1.0, apply_scale_options="FBX_SCALE_NONE",
        primary_bone_axis="Y", secondary_bone_axis="X",
    )
    arm.animation_data.action = None


def main():
    out_dir = argv_value("--out", os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))), "art", "export"))
    os.makedirs(out_dir, exist_ok=True)
    reset_scene()
    mats = make_materials()
    parts = build_body()
    body, _ = join_body(parts, mats)
    assign_slots_by_region(body)
    arm = build_armature()
    bind(body, arm)
    actions = author_actions(arm)

    bb = [body.matrix_world @ Vector(c) for c in body.bound_box]
    zs = [v.z for v in bb]
    xs = [v.x for v in bb]
    print("STAT mesh verts=%d polys=%d materials=%d" % (len(body.data.vertices), len(body.data.polygons), len(body.data.materials)))
    print("STAT bounds z=%.3f..%.3f width=%.3f" % (min(zs), max(zs), max(xs) - min(xs)))
    print("STAT bones=%d" % len(arm.data.bones))
    counts = {}
    for p in body.data.polygons:
        counts[p.material_index] = counts.get(p.material_index, 0) + 1
    print("STAT slotpolys=%s" % sorted(counts.items()))

    for name, action in actions.items():
        spec = ACTIONS[name]
        mid = spec["keys"][len(spec["keys"]) // 2][0]
        print("PROBE %s frame=%d %s" % (name, mid, probe(arm, action, mid)))

    export_mesh(body, arm, os.path.join(out_dir, "SE_Player_Mesh.fbx"))
    for name, action in actions.items():
        export_action(arm, action, os.path.join(out_dir, "SE_Anim_%s.fbx" % name), ACTIONS[name]["length"])
    for f in sorted(os.listdir(out_dir)):
        if f.lower().endswith(".fbx"):
            print("EXPORT %s %d bytes" % (f, os.path.getsize(os.path.join(out_dir, f))))
    print("DONE")


main()

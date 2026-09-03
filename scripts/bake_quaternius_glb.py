#!/usr/bin/env python3
"""Bake Quaternius dinosaur FBX (with IK) to Godot-friendly GLB via Blender.

Usage (headless):
  blender --background --python scripts/bake_quaternius_glb.py -- \\
      --src "/path/to/FBX" --dst "godot/assets/dinos"
"""

from __future__ import annotations

import argparse
import os
import sys


def parse_args(argv: list[str]) -> argparse.Namespace:
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    else:
        argv = []
    p = argparse.ArgumentParser()
    p.add_argument("--src", required=True, help="Directory of .fbx files")
    p.add_argument("--dst", required=True, help="Output directory for .glb files")
    return p.parse_args(argv)


def clear_scene() -> None:
    import bpy

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in list(bpy.data.actions):
        bpy.data.actions.remove(block)
    for block in list(bpy.data.armatures):
        bpy.data.armatures.remove(block)
    for block in list(bpy.data.meshes):
        bpy.data.meshes.remove(block)
    for block in list(bpy.data.materials):
        bpy.data.materials.remove(block)
    for block in list(bpy.data.images):
        bpy.data.images.remove(block)


def import_fbx(path: str) -> None:
    import bpy

    bpy.ops.import_scene.fbx(
        filepath=path,
        automatic_bone_orientation=True,
        use_anim=True,
        ignore_leaf_bones=True,
    )


def bake_actions() -> None:
    """Bake every action with visual keying and clear constraints (kills IK)."""
    import bpy

    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    if not arms:
        raise RuntimeError("no armature found after FBX import")
    arm = arms[0]
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="POSE")

    actions = list(bpy.data.actions)
    if not actions:
        raise RuntimeError("no actions found after FBX import")

    for action in actions:
        arm.animation_data_create()
        arm.animation_data.action = action
        frame_range = action.frame_range
        start = int(frame_range[0])
        end = int(frame_range[1])
        bpy.context.scene.frame_start = start
        bpy.context.scene.frame_end = end
        bpy.ops.nla.bake(
            frame_start=start,
            frame_end=end,
            only_selected=False,
            visual_keying=True,
            clear_constraints=True,
            clear_parents=False,
            use_current_action=True,
            bake_types={"POSE"},
        )
        # Ensure loop-friendly names for Idle/Walk/Run
        name = action.name
        low = name.lower()
        if any(tok in low for tok in ("idle", "walk", "run", "sprint", "gallop")) and not low.endswith(
            ("loop", "-loop", "_loop")
        ):
            action.name = f"{name}-loop"

    bpy.ops.object.mode_set(mode="OBJECT")

    # Drop leftover constraints on pose bones
    for pb in arm.pose.bones:
        while pb.constraints:
            pb.constraints.remove(pb.constraints[0])


def stash_actions_to_nla() -> None:
    import bpy

    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    arm = arms[0]
    if not arm.animation_data:
        arm.animation_data_create()
    # Clear NLA then stash each action so glTF exporter emits multiple clips
    while arm.animation_data.nla_tracks:
        arm.animation_data.nla_tracks.remove(arm.animation_data.nla_tracks[0])
    for action in bpy.data.actions:
        track = arm.animation_data.nla_tracks.new()
        track.name = action.name
        track.strips.new(action.name, int(action.frame_range[0]), action)
    arm.animation_data.action = None


def export_glb(path: str) -> None:
    import bpy

    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        export_animations=True,
        export_nla_strips=True,
        export_force_sampling=True,
        export_apply=False,
        export_skins=True,
        export_morph=False,
        export_yup=True,
    )


def process_one(src_fbx: str, dst_glb: str) -> None:
    clear_scene()
    import_fbx(src_fbx)
    bake_actions()
    stash_actions_to_nla()
    os.makedirs(os.path.dirname(dst_glb) or ".", exist_ok=True)
    export_glb(dst_glb)
    print(f"OK {os.path.basename(src_fbx)} -> {dst_glb}")


def main() -> None:
    args = parse_args(sys.argv)
    src = os.path.abspath(args.src)
    dst = os.path.abspath(args.dst)
    os.makedirs(dst, exist_ok=True)
    fbxs = sorted(f for f in os.listdir(src) if f.lower().endswith(".fbx"))
    if not fbxs:
        raise SystemExit(f"no .fbx in {src}")
    for name in fbxs:
        out_name = os.path.splitext(name)[0] + ".glb"
        process_one(os.path.join(src, name), os.path.join(dst, out_name))
    print(f"baked {len(fbxs)} models into {dst}")


if __name__ == "__main__":
    main()

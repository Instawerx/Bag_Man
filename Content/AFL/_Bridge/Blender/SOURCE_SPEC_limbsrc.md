# SOURCE-FBX SPEC — `SK_AFL_RobotLimbs_Src.fbx`

**For:** whoever drives the Desktop blender_mcp bridge (Blender open, addon port 9876).
**Why this file exists:** this AIK/Claude-Code session is wired to `unreal-editor` only (`.mcp.json` → port 9315). Port 9876 is open but the addon returns **zero bytes** to raw socket writes (`get_scene_info`, `execute_code` all len=0) — it only responds when paired with the official `blender-mcp` MCP server that Claude **Desktop** spawns. So the source FBX must be produced on the Desktop side; AIK then imports it.

This is the **arm+leg** analogue of the proven head source `SK_AFL_RobotHead_Src.fbx` (in `pending/20260614-headsrc/`). Clone that precedent exactly.

---

## What the FBX must contain

1. **A re-exported SKINNED mesh** — skinning is **mandatory** (the gib extraction in Blender cuts by **bone weight**, not by material or loose-part selection). Mirror the head's own-skeleton precedent (`SK_AFL_RobotHead_Src_Skeleton`).

2. **BOTH limb regions, with bone weights intact:**
   - **Arms:** `upperarm_l`, `upperarm_r`, `lowerarm_l`, `lowerarm_r`, `hand_l`, `hand_r` (+ finger bones if weighted — fine to include; the cut takes the whole arm down-tree from `upperarm`).
   - **Legs:** `thigh_l`, `thigh_r`, `calf_l`, `calf_r`, `foot_l`, `foot_r` (+ ball/toe if weighted).
   - These are the standard **SK_Mannequin** bone names (the head-src came from `SKM_Manny` = the `SK_Mannequin` rig, so every one of these bones is present by construction — confirmed). The Blender extract later selects faces by these bone weights per the DA cut planes (arm root = `upperarm_l/r`, leg root = `thigh_l/r`).

3. **Source mesh:** the **full-body robot skeletal mesh** — the same body the head re-export came from. (The shipped robot is `B_AFL_Robot_*` CharacterParts over `SKM_Manny`; the head-src was a re-export off that body. Use the same body so the limb skin/material matches.)

4. **TWO material slots, slot 1 = the skin region.** Mirror the head FBX exactly: head carried `MI_Manny_02_Blue` on **slot 0** and `MI_Manny_01_Blue` on **slot 1**, where **slot 1 is the `M_HeadLegs` skin region** the sever rebinds the finish onto. The limb faces must likewise be split so **slot 1 = the limb skin region** (the `M_HeadLegs`-equivalent), because the C++ recovers the finish MIC via `GetMaterial(1)->Parent` and re-skins from slot 1. A limb gib with skin on slot 0 (or only one slot) will **not** carry the finish.

5. **Metre scale.** The head was a metre-scale import; the **unit-scale re-bake into the static mesh happens at the gib-finishing step** (after parent/armature removal), not here. Export at metre scale exactly as the head source was.

6. **Stage to:** `Content/AFL/_Bridge/Blender/pending/<task-id>-limbsrc/SK_AFL_RobotLimbs_Src.fbx`
   (e.g. `20260616-limbsrc`). Same folder convention as `20260614-headsrc/`.

---

## Clone-target numbers (from the proven head gib — for the Blender finishing step that follows)

The head gib `SM_AFL_RobotHead_Gib` finished at: **box extent 8.85 × 11.33 × 13.36 cm** (full bbox ≈ 17.7 × 22.7 × 26.7 cm), **origin ≈ (0, 0, -1.1)** = volume-center at world origin, transform clean (loc 0 / rot 0 / scale 1), **watertight** (0 open-boundary / 0 non-manifold), **2 material slots**, **1 convex hull** (`auto_collision`, SimpleAndComplex).

Each limb gib must finish the same way: watertight, origin = volume center at world origin, 2 slots (slot 1 = skin), one convex hull. Expected finished bounds (the scale-trap assert): **arm ≈ 60–70 cm long, leg ≈ 80–90 cm long** — tens of cm, NOT tens of metres. If a gib imports ~100× off, it's the metre→cm trap; reject + re-bake the unit-scale.

---

## After the FBX is staged

Ping AIK/Claude-Code. It will then (per the approved B/C/D plan):
- **B:** import → extract `SM_AFL_RobotArm_Gib` (upperarm+lowerarm+hand, cap shoulder) + `SM_AFL_RobotLeg_Gib` (thigh+calf+foot, cap hip), head import settings (`build_nanite=false, auto_collision=true, import_lods=false, import_materials=false`), move to `/Game/BagMan/Characters/Dismember/`, verify bounds/path/cold-restart.
- **C:** create `BP_AFL_DismemberedArm` + `BP_AFL_DismemberedLeg` (children of `AAFLDismemberedLimb`, `LimbGibMesh` set), repoint the 4 limb rows in `DA_AFL_DismemberZones`.
- **D:** arm the PIE sever-watch (`AFL.Dismember.TestSever upperarm_l / upperarm_r / thigh_l / thigh_r`).

# AIK HANDOFF — Robot limb dismemberment, steps B-fixup / C / D

**Agent:** Claude Code (best Lyra awareness). **Profile:** AFL Blueprint & Gameplay.
**Run from:** UE Editor → Tools → Agent Chat, with the AFLDismember GameFeature loaded.
**Precondition:** NOT in PIE. Save the project after each step.

The C++ is already complete and verified — `AAFLDismemberedLimb` mirrors `AAFLDismemberedHead`,
takes its mesh from an `EditDefaultsOnly UStaticMesh LimbGibMesh`, and the sever path
(`UAFLDismemberComponent`, the `SpawnActor<AAFLDismemberedPart>(Row.PropClass…)` block ~L532)
hands the spawned limb the victim's skin color + slot-1 MIC via `GetMaterial(1)->Parent`.
**Nothing in C++ needs to change.** These steps are pure data wiring.

The two gib meshes were extracted in Blender and imported. See
`pending/20260616-limbsrc/manifest.json` for the full audit. Key facts the steps below rely on:
- Both gibs are watertight, origin-centered (volume-center at world 0), 2 material slots,
  **slot 1 = `MI_Manny_01_Blue` = the skin region** the sever rebinds onto.
- Arm bounds ≈ 38 × 38 × 62 cm; Leg bounds ≈ 19 × 31 × 78 cm (tens of cm — if either reads
  ~100× larger, the import scale is wrong; reimport, don't proceed).

---

## STEP B-FIXUP — relocate + save the imported gibs  ⚠️ DO THIS FIRST

The FBXs were imported **in place** at their staging path, not the destination. Confirm and fix:

1. The two static meshes currently live at (or near):
   `/Game/AFL/_Bridge/Blender/pending/20260616-limbsrc/SM_AFL_RobotArm_Gib`
   `/Game/AFL/_Bridge/Blender/pending/20260616-limbsrc/SM_AFL_RobotLeg_Gib`
   (Only autosaves exist on disk so far — they are unsaved. Verify actual current package paths in the Content Browser.)
2. **Move** both to `/Game/BagMan/Characters/Dismember/` (alongside the existing `SM_AFL_RobotHead_Gib`),
   fixing up redirectors. Final paths must be:
   `/Game/BagMan/Characters/Dismember/SM_AFL_RobotArm_Gib`
   `/Game/BagMan/Characters/Dismember/SM_AFL_RobotLeg_Gib`
3. On each mesh, confirm import settings match the head gib: **Nanite OFF, collision = auto (one convex hull),
   LODs not imported, materials not imported.** Confirm 2 material slots, slot 1 = `MI_Manny_01_Blue`.
4. **Save All.**

---

## STEP C — limb prop Blueprints + repoint the DA rows

### C-1: Two Blueprint children of `AAFLDismemberedLimb`

Attach `AAFLDismemberedLimb` (C++ class) context if available.

```
Create two Blueprint classes, both extending AAFLDismemberedLimb (the C++ class in the
AFLDismember GameFeature plugin):

1. BP_AFL_DismemberedArm
   - Set the inherited EditDefaultsOnly property LimbGibMesh to the static mesh
     /Game/BagMan/Characters/Dismember/SM_AFL_RobotArm_Gib

2. BP_AFL_DismemberedLeg
   - Set the inherited EditDefaultsOnly property LimbGibMesh to the static mesh
     /Game/BagMan/Characters/Dismember/SM_AFL_RobotLeg_Gib

Both: no other overrides — the base class already configures physics (PhysicsActor profile,
SimulatePhysics, replication, force-pop, 5s lifespan) and the appearance/identity OnReps.
This MIRRORS the existing head prop Blueprint (whatever BP child sets HeadGibMesh on
AAFLDismemberedHead). Place both in:
   Plugins/GameFeatures/AFLDismember/Content/Props/
Compile and save.
```

### C-2: Repoint the 4 limb rows in `DA_AFL_DismemberZones`

Asset: `/AFLDismember/Data/DA_AFL_DismemberZones`
(disk: `Plugins/GameFeatures/AFLDismember/Content/Data/DA_AFL_DismemberZones.uasset`).
Attach it as context.

The rows use struct `FAFLDismemberZone`. Its `PropClass` field is a
`TSoftClassPtr<AAFLDismemberedPart>`. Set `PropClass` on the four limb zone rows:

```
In DA_AFL_DismemberZones, set the PropClass (a soft class ptr) on the limb rows:
  - Zone LeftArm  (SeveredBone upperarm_l, BoneMatches upperarm_l/lowerarm_l/hand_l) -> BP_AFL_DismemberedArm
  - Zone RightArm (SeveredBone upperarm_r, BoneMatches upperarm_r/lowerarm_r/hand_r) -> BP_AFL_DismemberedArm
  - Zone LeftLeg  (SeveredBone thigh_l,    BoneMatches thigh_l/calf_l/foot_l)        -> BP_AFL_DismemberedLeg
  - Zone RightLeg (SeveredBone thigh_r,    BoneMatches thigh_r/calf_r/foot_r)        -> BP_AFL_DismemberedLeg
Leave every other field on these rows (impulse, CueTag, ConsequenceGE, SparkFX) UNCHANGED.
Do NOT touch the Head/torso rows. Save the data asset.
```

(Both arm rows share one BP and both leg rows share one BP — the gib mesh is the left-side
canonical mesh; the prop spawns at the per-row SeveredBone transform.)

---

## STEP D — sever-watch verification (operator-typed console, NOT agent-issued)

> ⚠️ These console commands are **typed by the operator in PIE**, never issued by an MCP/agent.
> Place a `B_AFL_DamageTarget_Skel` dummy in the level first (the cheat targets the first one found).

After C compiles + saves, enter PIE and run, one at a time:
```
AFL.Dismember.TestSever upperarm_l
AFL.Dismember.TestSever upperarm_r
AFL.Dismember.TestSever thigh_l
AFL.Dismember.TestSever thigh_r
```
**Expected per command:** log line `[AFLDismember] Part prop spawned on …` + a physics limb gib
pops off the dummy at the severed joint, tumbles, and auto-destroys after ~5 s. The gib should
read in the **victim robot's skin** (slot-1 MIC + skin color applied via the limb OnReps) — i.e.
the arm/leg matches whose robot it came from, exactly like the head.

**Pass criteria:** correct limb mesh spawns for each bone, carries the per-robot finish,
pops + tumbles + despawns. If the gib spawns grey/default-material, the slot-1 MIC resolve
failed — check the gib's slot 1 is `MI_Manny_01_Blue` (not slot 0).

---

## If something's off
- **Gib spawns at wrong scale (~100× / tiny):** Blender→UE cm/metre import trap — reimport the FBX.
- **Gib spawns grey (no finish):** its skin is on the wrong material slot. Both gibs were authored
  with skin on slot 1; if the arm regressed, re-check the slot remap (see manifest audit — arm skin
  was remapped from slot 0 → slot 1 specifically so `GetMaterial(1)->Parent` works).
- **No prop spawns at all:** the row's `PropClass` soft ref didn't resolve — confirm the BP path
  and that the AFLDismember GameFeature is active.

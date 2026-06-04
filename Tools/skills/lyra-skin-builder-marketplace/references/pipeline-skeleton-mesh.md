# Pipeline, Skeleton & Mesh Reference

Phases 1–3 of the reskinning pipeline. Get these wrong and nothing downstream
works.

---

## Phase 1 — Audit & Plan

Run this checklist **before any DCC work**. Most failed reskin attempts skipped
this and ended up with art that doesn't fit the system.

```
□ Reskin level locked (L1 / L2 / L3 / L4 / L5)
□ Target platforms confirmed (PC / Console / Mobile combination)
□ Skeleton decision: re-bind to SK_Mannequin in DCC  OR  IK Retarget
□ Animation source: keep Lyra's stock anim set  OR  ship custom
□ LOD targets per platform written down
□ Texture budget per character written down
□ Cloth/sim policy decided (PC only / never / hybrid)
□ Modular vs monolithic decided (L2 vs L4)
□ Marketplace plan: monetized / unlock-only / free
□ Live-ops plan: single ship  OR  GameFeature plugin drops
□ Backend model: client-trusted / server-authoritative / platform-store-receipt
```

If the answer to skeleton decision is "we'll figure it out later" — stop and
decide now. That single choice changes weeks of downstream work.

---

## Phase 2 — Skeleton Strategy

### Decision Matrix

```
Source asset type            → Recommended approach
─────────────────────────────────────────────────────────────
Custom DCC art (your own)    → Skin in DCC to SK_Mannequin (Path A)
MetaHuman                    → Path A — MH skeleton is bone-compatible w/ SK_Mannequin
Fab/Marketplace UE asset     → IK Retarget if rig differs (Path B)
Mixamo character             → Path B — Mixamo has its own skeleton
Mocap library data           → Retarget animations onto SK_Mannequin
External rigged FBX          → Path B if can't re-skin in DCC
```

### Path A — Re-skin to SK_Mannequin in DCC (Preferred)

This is the cheapest and highest-fidelity path. Animations work natively,
sockets work natively, IK works natively. Recommended whenever you own the
source mesh.

**Maya/Blender workflow:**
```
1. Export SK_Mannequin's reference skeleton FBX from UE5:
   Right-click SK_Mannequin → Asset Actions → Export → "Mannequin_RefSkel.fbx"

2. In DCC:
   a. Import Mannequin_RefSkel.fbx — gives you the canonical bone hierarchy
   b. Import your character mesh
   c. Match the bind pose to the SK_Mannequin reference pose (T-pose, A-pose
      whichever your Lyra skeleton uses — check before binding)
   d. Skin the mesh to the Mannequin skeleton (use Voxel/heat-map binding
      then manually clean weights)
   e. Verify NO bones were added/renamed. If you need a new bone for a prop
      (e.g. cape), add it under an existing Mannequin bone, never replace
      a Mannequin bone.

3. Export FBX with:
   Forward: -Y    Up: Z    Scale: 1.0 (centimeters)
   Smoothing groups: ON
   Skinning: ON
   Animation: OFF (mesh-only)
   Tangents: OFF (UE recomputes)
   Triangulate: ON
```

**UE5 import settings:**
```
Skeleton:           SK_Mannequin (must select existing — never "Create New")
Mesh:               Skeletal Mesh
Use T0 As Ref Pose: TRUE if your bind pose matches SK_Mannequin's frame 0
Import Mesh LODs:   TRUE (if FBX has LOD groups)
Import Morph Targets: TRUE if facial blendshapes exist
Import Animations:  FALSE on mesh import
Material Import Method: Create New Materials → fix slot order after
```

**Verification after import:**
```
1. Open the imported SK_<Project>Char_<SkinName>_Body asset
2. Asset Details → Skeleton field → must read "SK_Mannequin"
3. Mesh Editor → Asset Details → check bone count matches Mannequin
4. Drop into a Lyra Pawn BP → verify ABP_Mannequin still drives it correctly
5. Play in editor — character should walk/run/jump with stock animations
```

If after import the mesh references a NEW skeleton instead of SK_Mannequin,
the bone names didn't match. Fix in DCC and re-import — UE5 will let you
remap, but that's a maintenance nightmare downstream.

### Path B — IK Retarget (External Rigs)

Use when the source asset has a non-Mannequin skeleton you cannot re-skin
(e.g. licensed Fab characters, third-party humanoids).

**Required assets:**
```
IK_<SourceRig>          — IK Rig asset on the source skeleton
IK_Mannequin            — Lyra's existing IK Rig on SK_Mannequin (stock)
RTG_<SourceRig>_To_Mannequin  — IK Retargeter asset chaining the two
```

**Step-by-step:**
```
1. Import the external character with its OWN skeleton (don't try to force
   it onto SK_Mannequin during import). You'll get SK_External + SKEL_External.

2. Create an IK Rig for the external skeleton:
   Right-click in Content Browser → Animation → IK Rig
   Set Preview Mesh: SK_External
   Define Retarget Root: pelvis or hips bone
   Add Retarget Chains for: Spine, LeftArm, RightArm, LeftLeg, RightLeg, Head
   Each chain: set Start Bone and End Bone matching the skeleton
   Save as IK_<SourceRig>.

3. Verify IK_Mannequin exists (it ships with Lyra in
   /Game/Characters/Heroes/Mannequin/Rigs/IK_Mannequin)

4. Create the IK Retargeter:
   Right-click → Animation → IK Retargeter
   Source IK Rig: IK_<SourceRig>
   Target IK Rig: IK_Mannequin
   Configure chain mappings (Spine→Spine, etc.)
   Adjust the retarget pose so the source matches Mannequin's reference pose
   (this is the step everyone fudges and pays for later)
   Save as RTG_<SourceRig>_To_Mannequin.

5. Use the Retargeter two ways:
   OPTION 1 — Retarget animations FROM Mannequin → External rig
     Use when keeping the source character's own skeleton at runtime.
     Batch retarget all Lyra anims onto SK_External via RTG.
   OPTION 2 — Retarget animations FROM External → Mannequin (rare for cosmetics)
     Use when bringing external mocap onto Lyra rig.

   For cosmetic skin systems where the player picks a skin and Lyra anims
   should "just work", OPTION 1 is what you want.
```

**The retarget pose adjustment is the make-or-break step.** Open the
Retargeter, switch to "Edit Retarget Pose" mode, and adjust the source's
arms/legs/spine so each chain points in the same general direction as the
Mannequin reference pose. If you skip this, animations look wonky on the
new character (twisted shoulders, broken wrists).

### When You Need a New Bone (Capes, Tails, Hair Strands)

Don't add bones to SK_Mannequin. Instead:

- **Cosmetic-only bones**: Author a SECOND skeletal mesh component for the
  cape/hair, parented to a Mannequin bone via socket. Skinned to its own
  small skeleton.
- **Cloth-driven**: Use UE5 Chaos Cloth on a separate cloth mesh, attached
  via skeletal mesh socket.
- **Animated props**: Use AnimBP control rig or post-process AnimBP on the
  separate mesh, driven by the main character's motion.

---

## Phase 3 — Mesh & Material Import Pipeline

### Material Slot Order (THE #1 Gotcha Source)

Lyra's `M_Manny_Body` master material expects a specific material slot order
on the skeletal mesh. If your imported mesh has slots in a different order,
the wrong material gets assigned to the wrong region.

**Stock Lyra slot order on SK_Manny:**
```
Slot 0: Body
Slot 1: Outfit
Slot 2: Hair
Slot 3: Eyes
Slot 4: Eyebrows
Slot 5: Eyelashes
Slot 6: Teeth
```

When authoring a new `SK_<Project>Char_<SkinName>_Body`:

```
✓ Match the slot order to Lyra's convention so you can use the stock
  Material Instance pattern
✓ OR fully document your custom slot order and ship a custom
  M_<Project>_Body master material that expects your order

✗ Half-match the order and hope it works — the moment you swap textures
  on what you thought was Slot 1 (Outfit) but is actually Slot 2 (Hair),
  the character's hair turns into the outfit texture
```

### Skin Cache Setting (THE #1 Rendering Gotcha)

Lyra-style characters require **GPU Skin Cache** to be enabled at the project
level. Without it, characters render but cast no shadows, and Nanite cosmetic
parts won't work.

```
Project Settings → Rendering → Optimizations:
  ☑ Support Compute Skin Cache       (required)
  ☑ Default Skin Cache Behavior      → Inclusive
  
Per-mesh override (Skeletal Mesh asset):
  Asset Details → Optimization → Skin Cache Usage → Enabled
```

For mobile, you can selectively disable skin cache on non-hero meshes to
save GPU time — but the player character should keep it on.

### LOD Configuration Per Platform

Hand-author LOD0 for hero characters; auto-generate LOD1+ with hand tuning
of the auto-LOD settings.

```
                LOD0      LOD1      LOD2      LOD3     Nanite?
PC/Console hero 80k tris  35k       12k       5k       No (need skin)
Mobile hero     35k       15k       6k        —        No
PC/Console NPC  40k       18k       7k        2k       No
Mobile NPC      15k       6k        2k        —        No
Modular part    proportional to character — sum of parts ≤ hero budget
```

Configure in the Skeletal Mesh asset:
```
LOD Settings → LOD Group → SkeletalMesh_LOD0 (custom group recommended)
LOD Reduction Settings → adjust per LOD slot
LOD Distance Override → set screen-size threshold per LOD
```

For mobile, also override texture LOD bias:
```
DefaultDeviceProfiles.ini → [Android DeviceProfile] / [iOS DeviceProfile]:
+CVars=r.SkeletalMeshLODBias=1   ; force one LOD lower on mobile
```

### Physics Asset (PHYS_) Regeneration

A new skeletal mesh needs its own PHYS_ asset for ragdoll, hit reactions,
and physical interactions. Do NOT reuse PHYS_Mannequin for a mesh with
different proportions.

```
1. Right-click SK_<Project>Char_<SkinName>_Body → Create → Physics Asset
2. Generation settings:
   - Minimum Bone Size: 5.0
   - Collision Geometry: Single Convex Hull (per-bone)
   - Vertex Weighting Threshold: 0.1
3. Review each body's collision — adjust capsules/spheres by hand for hands,
   feet, and head (auto-gen rarely gets these right)
4. Disable simulation on bones that shouldn't ragdoll (helmet attachments,
   prop bones)
5. Set Constraint motion to "Limited" on neck/spine so ragdoll doesn't go
   horror-movie when the character dies
```

### The Ref Pose Trap

If your imported mesh's bind pose doesn't match SK_Mannequin's reference pose
exactly, animations will look subtly broken — slight offsets in shoulder
height, knees that don't bend right, hands that don't grip weapons cleanly.

**Three ways this happens and how to fix each:**

```
1. DCC bind pose was set wrong before skinning
   Fix: in DCC, set bind pose to match SK_Mannequin's, re-skin, re-export

2. Use T0 As Ref Pose checkbox wrong on import
   Fix: re-import with the checkbox state matching your authored bind pose

3. Author intentionally used a different pose for skinning convenience
   Fix: in UE5, open the Skeletal Mesh, Skeleton tab, "Modify Pose" →
        "Use Current Pose as Reference Pose" — but this DESYNCS from
        SK_Mannequin's ref pose and breaks retargeted anims.
        Better: re-author bind pose to match SK_Mannequin.
```

### Sockets to Preserve When Reskinning

Lyra's animations and equipment system rely on these sockets existing on
the skeleton (they live on SK_Mannequin, but if you've added a separate
mesh with its own skeleton, replicate them):

```
weapon_r       — right hand weapon attach
weapon_l       — left hand weapon attach
foot_r_socket  — right foot ground IK target
foot_l_socket  — left foot ground IK target
head_socket    — first-person camera attach / hat attach
spine_socket   — backpack / chest gear attach
pelvis_socket  — holster attach
```

If your skin uses modular accessories (helmets, backpacks, holsters), each
piece attaches to the corresponding socket via `FLyraCharacterPart::SocketTag`.

---

## End of Phase 3 Verification Checklist

Before moving to Phase 4 (modular architecture), every reskinned character
must pass this:

```
□ Imported on SK_Mannequin (verified in asset details)
□ Material slots in expected order, no checkerboard materials
□ Skin Cache enabled at project level and per-mesh
□ LOD0 in budget, LOD1–3 auto-generated and inspected
□ PHYS_ asset created and tuned (basic ragdoll test passes)
□ Stock Lyra anim plays correctly (run forward, jump, idle)
□ Lyra weapon attaches to weapon_r socket cleanly
□ Hit reactions / damage anims play without bone offset
□ Mobile LOD bias overrides set if shipping mobile
□ No new bones added to SK_Mannequin; cosmetic bones on separate meshes
□ One full Lyra play loop runs with the new character as the hero
```

If any check fails, do not proceed. Phases 4–10 assume the foundation is
solid; trying to layer a modular parts system on a broken mesh wastes weeks.

# IRONICS -- WEAPON FACTORY: Blender->UE HAND-OFF CONTRACT + PILOT ORDER SET

**Anti-drift core.** Both agents obey this ONE interface: **Claude Desktop / Blender EXPORTS to it;
AIK / UE IMPORTS EXPECTING it.** Mismatch = silent misalignment (wrong scale, beam in the wrong
place). Disk-grounded 2026-07-02 against the stock Lyra weapon meshes
(`/Game/Weapons/{Pistol,Rifle,Shotgun}/Mesh/`; `SK_Pistol` confirmed to carry a `Muzzle` socket).
NO build yet -- this is the order set; build on operator go.

================================================================================
## PART 0 -- ROLES + HAND-OFF STATUS (whose court the ball is in)
================================================================================

### Roles (fixed)
- **CLAUDE DESKTOP (Blender)** -- owns MESH work: the ballistic->beam conversion remesh, greebles,
  attachment pivot-align. CONSUMES a base FBX (exported by AIK) + the Blender prompt. PRODUCES a
  contract-compliant modded FBX returned to `pending/<Weapon>/`. **Desktop's job ENDS when the FBX
  is in the bridge + contract-compliant.**
- **AIK (UE)** -- owns EVERYTHING UE-side: export the base mesh TO the bridge (unblock Desktop);
  then on the modded FBX's return -> contract-verify, import, WID-grant wire, beam + BeamColorOverride,
  skin, harness-register, and drive the PIE-prove. The whole UE pipeline around Desktop's mesh.
- **OPERATOR** -- dispatches the Blender prompts to Desktop, runs PIE, owns builds / LFS / imports
  (build-side).

### Status vocabulary (each weapon reports ONE state -- no "done/not-done" ambiguity)
| State | Meaning | Court |
|---|---|---|
| **BASE-EXPORTED** | AIK put the base FBX in `pending/<Weapon>/` | Desktop |
| **DESKTOP-WORKING** | Desktop modding in Blender | Desktop |
| **FBX-RETURNED** | modded FBX back in the bridge | AIK |
| **CONTRACT-VERIFIED** | AIK confirmed the FBX passes the contract gates (scale/axis/Muzzle/**GripPoint_L mesh socket (2H)**/slots) | AIK |
| **WIRED** | AIK finished the UE wire (harvest-clone + mesh + beam + BeamColorOverride + skin + grant) | AIK |
| **EQUIP-READY** | registered in the harness; ready for PIE | Operator |
| **PIE-PROVEN** | watched equip / fire / beam / coexist -- the ONLY "done" | -- |

Baton flips at **BASE-EXPORTED** (AIK->Desktop) and **FBX-RETURNED** (Desktop->AIK). Every status
report names each weapon's current state in this vocabulary.

================================================================================
## PART 1 -- THE HAND-OFF CONTRACT (pinned; both agents adhere)
================================================================================

### 1. FBX EXPORT (Blender -> UE)
- **Units/scale:** UE = centimeters; Blender default = meters. Export with **Apply Scalings = "FBX All"**
  (or set Blender Unit Scale) so a rifle lands ~60-90 cm in UE. Import check: import scale = 1.0,
  bounds read as cm (not 0.01x tiny / 100x huge).
- **Axes:** **Up = Z, Forward = X** (UE). Set the FBX exporter Forward=X / Up=Z (not Blender's -Y/Z
  default), or bake via Apply Transform. Barrel = +X, top = +Z.
- **Apply:** Apply Modifiers = ON; Apply Transform = ON (bake rotation/scale -> no import offset).
- **Smoothing:** Face (or authored custom normals). No edge-split that inflates tris.
- **Rig:** NO extra root bone; weapon's own root only. **MODIFY the clean 5.4 mesh -- keep topology /
  UVs / rig. NOT Tripo** (Tripo regenerates and degrades clean meshes; from-scratch only).

### 2. PIVOT / ORIGIN / ORIENTATION (grip-correct-by-construction)
- **Origin at the GRIP (0,0,0)** -- the point that lands on the hand socket. Stock Lyra weapons work
  with ZERO WID translation/scale because their grip is at origin; keep that so the pack gun attaches
  to `weapon_r` with the stock convention, no per-weapon tuning (avoids the custom-mesh grip saga).
- **Orientation: MATCH THE BASE MESH'S NATIVE LONG AXIS -- do NOT assume +X.** Verify per base by
  its bounds: **AK / rifle / pistol "Xaxis" variants = barrel +X**; **the SHOTGUN chassis = barrel
  +Y** (bounds ~X18/Y72). Keep the grip at origin + top +Z; the Muzzle + all additions go in the
  base's OWN long axis so the stock WID `weapon_r + (-90 yaw) + zero translation` still seats it.
  (Bug caught 2026-07-02: the contract said "+X" generically; Voltaic (shotgun=+Y) got its Muzzle
  placed at +X 44cm -- off the mesh. Arclight (AK=+X) was fine. Rule is now per-chassis.)

### 3. SOCKETS (names MUST match -- the beam anchors here)
- **`Muzzle`** -- REQUIRED, at the barrel TIP = the beam origin. **Verified name on the stock meshes.**
  The beam's `UAFLAG_Laser_Base::ResolveMuzzleLocation` reads sockets in order **{Muzzle, Barrel,
  Slide}** then falls back to weapon_r -- so name it exactly **`Muzzle`**; point its +X down the bore.
- **Attachment sockets** (only where the recombine axis is used): `Attach_Barrel`, `Attach_ForeEnd`,
  `Attach_Grip`, `Attach_Scope`, `Attach_Sight` at the 5.4 mount points.
- **`GripPoint_L`** -- **REQUIRED on every 2H weapon** (rifles/beams). The SUPPORT(left)-hand grip =
  the load-bearing IK interface, **same tier as `Muzzle`** (not a side note). NON-NEGOTIABLE FORM:
  it MUST be a **real bone-parented MESH SOCKET** (created in the mesh's Socket Manager on
  `SK_<Weapon>`, Parent = a bone e.g. `Root`) -- **NOT a loose scene component parented to None**.
  A loose-None component floats and the hand-IK fights POSITION and ROTATION (the entire Arclight
  socket saga; PROVEN root cause 2026-07-03). BOTH axes of the socket transform are required:
  **POSITION** = the foregrip (forward of the trigger, under the barrel, where the hand goes) AND
  **ROTATION** = the hand orientation (the socket's rotation orients the wrist/palm; a position-only
  socket floats the hand rotated wrong). The support-hand IK (`UAFLWeaponIKComponent`) reads it by
  name via `GetSocketTransform("GripPoint_L")` -> `CR_AFL_IRONICS` LeftHandIK (loc -> effector,
  rot -> effector rotation). PROVEN: **Arclight, socket on `Root` @ ~(28,0,4)**, hand plants clean.
  - **Per-class (handedness taxonomy):** 2H (rifles/beams) = **`GripPoint_L`** foregrip socket;
    1H (pistols) = **none** (the component uses a hand_r-relative cup); thrown (grenades) = **none**
    (IK off). The classifier keys off the socket's presence.
  - **Reach/scale:** the socket must sit within the left hand's cross-body reach (~arm length ~53cm).
    If the base mesh is too long (handguard past reach), scale the weapon down via the WID
    `AttachTransform.Scale3D` to the reachable size -- **Arclight 92cm -> 0.78 to match the ShotgunBeam
    72cm reference**. A shared IK solver needs consistent 2H dimensions across the class.

### 3a. VERIFY GATE -- grip socket (mandatory, alongside the Muzzle verify; a weapon is NOT "done" without it)
- A 2H weapon **FAILS** contract-verify (is not `CONTRACT-VERIFIED` / not PIE-proven) if ANY of:
  (a) missing the `GripPoint_L` **mesh socket**, (b) `GripPoint_L` is a loose scene component /
  Parent Socket = None instead of a real bone-parented mesh socket, (c) socket POSITION out of spec
  (unreachable / not on the foregrip), or (d) socket ROTATION wrong (position-only, hand mis-oriented).
- **PROOF is the gate (no false "done"):** create/verify -> SAVE `SK_<Weapon>` -> RE-READ the mesh's
  socket LIST from the saved asset -> SHOW `GripPoint_L` is IN it (name, parent bone, loc) + `.uasset`
  git-modified. An in-memory readback does not count (it can lie). If it's not in the re-read list,
  report the failure honestly -- do not claim created. (Banked 2026-07-03: a bridge `add("socket")`
  + save + same-session re-read looked created but the operator saw no socket; the disk re-read after
  editor relaunch is the true test.)
- Runs alongside the existing **`Muzzle`**-socket verify -- both are mandatory IK/beam interface sockets.

### 4. MATERIAL SLOTS (skin + beam bind here)
- **Slot 0 = `Body`** -- the primary housing. Stock meshes carry DCC names (`lambert6`, `Light_mtl3`);
  the factory STANDARDIZES slot 0 to `Body` so the skin binds predictably. The procedural skin
  (`M_AFL_WeaponSkin_NeonCamo` -> MI) binds to `Body`.
- **Slot 1 = `Emitter`** (optional) -- the beam lens / glowing element, if split out (emissive MI).
- Keep slot COUNT minimal + names stable across variants -> re-skin = a slot-name bind.

### 5. DELIVERY (Blender -> bridge)
- Export FBX + new textures to `Content/AFL/_Bridge/Blender/pending/<WeaponName>/` + a manifest
  (weapon name, base gun, mod-ops applied, socket list, slot list) so UE imports deterministically.
- **PATH IS EXACT: `pending/<WeaponName>/` -- PLAIN weapon name (`Arclight/`, `Voltaic/`,
  `Ioncaster/`), NOT dated.** BOTH directions use it: AIK's base exports OUT and Blender's modded
  returns BACK. The other dated `pending/` folders (emblem/arena/gib) are unrelated workstreams --
  do not use dated names for weapon-factory work. (Drift caught 2026-07-02: AIK's initial export
  went to `20260702-arclight/`; Desktop correctly followed the contract and couldn't find it -->
  realigned to `Arclight/`. The contract did its anti-drift job.)

================================================================================
## PART 2 -- CLAUDE DESKTOP / BLENDER PROMPTS (copy-paste; contract-compliant)
================================================================================

### WEAPON A -- "Voltaic" (shipped-beam variant; easy)
Base: shipped Shotgun-Beam mesh (AIK exports it to FBX -> bridge; you receive it).
Mods: add 2 liquid-cooler cylinders along the top rail + 3 heat-fins on the barrel shroud; leave
the receiver + grip untouched.
Contract: GRIP at origin (0,0,0); barrel +X / top +Z; KEEP the `Muzzle` socket at the barrel tip
(do not move/rename); slot 0 = `Body`. Apply modifiers+transform; export FBX (Up=Z, Fwd=X, Apply
Scalings=FBX All, no extra root bone) + manifest to `_Bridge/Blender/pending/Voltaic/`.
Success: reads as a heavier energy-shotgun; `Muzzle` intact.

### WEAPON B -- "Arclight" (TRUE ballistic->beam conversion; THE KEY DE-RISK; most detail)
Base: AK-110_Assault (Customizable Weapon Pack 5.4; AIK exports SK_AK-110 -> FBX -> bridge).
GOAL: convert an unmistakably-ballistic AK into an IRONICS energy/beam weapon that reads AAA -- NOT
an AK with a glow. MODIFY the clean mesh (keep topology/UVs/rig) -- do NOT regenerate (no Tripo).
- REMOVE/REPLACE (ballistic tells): curved box magazine -> slim translucent ENERGY-CELL; gas tube +
  front sight post -> smooth emitter shroud; charging handle + ejection port -> delete/smooth;
  wood/polymer furniture -> paneled energy-tech housing.
- ADD (IRONICS beam aesthetic): a muzzle EMITTER/LENS at the barrel tip (beam exit); 2-3 heat-fin/
  cooler ribs along the barrel; recessed glowing CONDUIT grooves along the receiver (geo the
  emissive skin lights).
- KEEP (contract-critical): grip+trigger at origin (0,0,0); ~AK length; barrel +X / top +Z; the
  `Muzzle` socket at the NEW emitter-lens center (+X down bore); slot 0 = `Body`, optional slot 1 =
  `Emitter` (the lens). Apply modifiers+transform; export FBX (Up=Z/Fwd=X/Apply Scalings=FBX All/no
  extra root) + manifest to `_Bridge/Blender/pending/Arclight/`.
Success: silhouette reads energy-weapon (not AK); `Muzzle` at the emitter; clean topology preserved.

### WEAPON C -- "Ioncaster" (attachment recombine; MINIMAL/no Blender)
This axis is UE-side (AIK recombines 5.4 modular parts via `Attach_*` sockets). Blender touch ONLY
if a chosen attachment needs its pivot aligned to a base mount -- adjust the ATTACHMENT part's origin
to seat cleanly; do NOT remesh the base. Base keeps `Muzzle` + `Attach_*`; slots `Body`/`Emitter`.
Most likely NO Blender needed.

================================================================================
## PART 3 -- AIK / UE ORDERS (per weapon; reuse PROVEN patterns; run on FBX arrival)
================================================================================
Per weapon:
1. **IMPORT** (afl-asset-pipeline): verify import scale=1.0, cm bounds, Z-up, `Muzzle` socket present,
   slots = [`Body`(, `Emitter`)]. If the contract check fails -> reject + bounce back to Blender.
2. **WID-GRANT WIRE** (PROVEN harvest-clone, NOT the broken actor-scan): clone the matching stock
   chassis set (ID/WID/instance/display/AbilitySet/GE/reticle); swap the display-actor mesh to the
   imported gun; `WID.ActorsToSpawn` AttachTransform = `weapon_r` + (-90 yaw) + zero translation +
   scale 1.0 (grip-origin -> correct by construction). Instance = child of `B_WeaponInstance_Rifle`.
3. **BEAM** (PROVEN `BeamChannel_v2`): graft the persistent `UAFLBeamVisualComponent` on the display
   actor (bReplicates); Fire = a `BeamChannel_v2` BP child (montage override if not 2H-rifle); muzzle
   resolves via `UAFLAG_Laser_Base` -> the `Muzzle` socket; set `BeamSystem` + `BeamColorOverride`.
4. **SKIN** (procedural axis): apply an MI of `M_AFL_WeaponSkin_NeonCamo` to slot `Body` with the
   weapon's NeonColor; optional emissive MI on `Emitter`.
5. **HARNESS:** register in `WeaponFactoryTestHarness` (`AFL_TEST_WEAPON[Pn]` marker) for the batch PIE.

Per-weapon: **A** = shipped-beam clone + cooler/fin mesh + rebeam. **B** = the converted AK mesh +
full beam wire (the de-risk) + skin. **C** = UE recombine of 5.4 attachments via `Attach_*` (no
Blender-mesh import) + rebeam.

Cross-refs: `IRONICS_WEAPON_FACTORY_SCOPE.md` (axes, base set, skin method), `IRONICS_WEAPONS_SSOT.md`,
skills `afl-laser-beam-system` + `afl-asset-pipeline`.

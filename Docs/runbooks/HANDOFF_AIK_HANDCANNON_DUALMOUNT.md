# HANDOFF → AIK — FANATICS Hand Cannon assembly + dual-mount PIE

**From:** Claude Code (weapon-gen + C++ lane) · **To:** AIK (UE-editor assembly + Control-Rig lane)
**Date:** 2026-07-28 · **Prereq commit:** `90f4060f` on `main` — *dual-mount C++ built clean on both engines.*

The C++ groundwork for the **dual-wield Hand Laser Cannon line** is landed and compiled (D: source + C:
launcher LyraEditor, exit 0). This is the UE-side assembly of the FIRST cannon (FANATICS) + the PIE proof of
the two-EquipItem coexistence. Nothing below needs new C++ from me unless flagged **[C++ GAP]**.

---

## 1. The C++ contract you build against (already on `main`)

| Piece | Detail |
|---|---|
| `FAFLCosmeticSelection.WeaponId` | RIGHT hand (unchanged — the existing single-weapon field). |
| `FAFLCosmeticSelection.LeftWeaponId` | **NEW.** LEFT hand. `NAME_None` (default) → single-held path, byte-identical to today. Set it → the dual path engages. |
| `RefreshHandCannonsForPawn` | Server spine body. Dispatched automatically from `RefreshWeaponForPawn` **only when `LeftWeaponId != NAME_None`**. Holds BOTH cannons: **targeted unequip** (keeps the two tracked instances, drops only the hero default primary + stale) → independent equip/replace per hand. |
| Resolve path (per hand) | id → `UAFLCosmeticCatalogSubsystem::ResolveAsset(id)` → `UAFLWeaponCosmeticAsset` carrier → `EquipmentDefinition.LoadSynchronous()` → `EquipItem`. **Same rail as every existing weapon.** |
| Attach socket | **DATA on each cannon's `ULyraEquipmentDefinition`** (the `ActorsToSpawn.AttachSocket`), NOT set in C++. R → `weapon_lowerarm_r`, L → `weapon_lowerarm_l`. |
| `InputTag.Weapon.Fire.Left` | NEW native tag (added to `Config/DefaultGameplayTags.ini`). The D3 second trigger. Bind it to the left cannon's fire ability. `InputTag.Weapon.Fire` stays right/single. |

**Guarantee:** all 7 existing projectile/beam/pulse guns + every single-held gun never enter the new code.
The dual path is fully additive and guarded on `LeftWeaponId`.

---

## 2. Assets delivered (import these)

`Content/AFL/_Bridge/Blender/pending/20260727-fanatics-cannon/`
- `SK_AFL_HandCannon_FANATICS_R.fbx` + `SK_AFL_HandCannon_FANATICS_L.fbx` (mirror) — one-`root`-bone skeletal,
  45.8k 100%-quad, origin = arm-cuff, maw = +Y, horns = +Z, 28.4×40×32 cm.
- `Color_HandCannon_FANATICS.png` (_0) · `Normal_HandCannon_FANATICS.png` (_3, **NormalGL → flip Green**) ·
  `ORM_HandCannon_FANATICS.png` (R=AO · G=Rough · B=Metal → **TC_Masks, Linear**).

Full rig detail: `…/20260727-fanatics-cannon/_HANDCANNON_FANATICS_RIG_SPEC.md`.

---

## 3. AIK task list

**Import + material**
1. Import both FBX (Import Normals+Tangents; UE-gen LODs) + the 3 textures (ORM→TC_Masks Linear; Normal→flip Green).
2. `MI_AFL_HandCannon_FANATICS` off `M_AFL_Weapon_PBR`; plug Color/Normal/ORM; **NeonColor `#FF0D0D`** (FANATICS red).
   FANATICS "F" logo = a **UE emissive decal** (not baked).

**Skeleton sockets (NEW — the arm-worn archetype's defining change)**
3. On the mannequin skeleton, add the two forearm sockets if absent: **`weapon_lowerarm_r`** (on `lowerarm_r`) and
   **`weapon_lowerarm_l`** (on `lowerarm_l`), oriented so the cannon's +Y (maw) points forward and +Z (horns) up
   when the cuff sits over the forearm. These are the attach targets the EquipmentDefinitions reference.
4. On each cannon mesh, place a **`Muzzle`** socket at the maw bore. Rig-spec ref (⚠ APPROXIMATE — the dragon head
   cranes ~18 cm off-axis): R = (-0.183, 0.349, 0.059), L = (0.183, 0.349, 0.059). Eyeball the actual mouth.

**Equipment + carrier + catalog (template off an existing weapon)**
5. Two `ULyraEquipmentDefinition`s — `ED_AFL_HandCannon_FANATICS_R` / `_L` — each spawning its mesh at the matching
   forearm socket (`weapon_lowerarm_r` / `_l`). Fire ability granted = the hand's base (see step 7).
6. Two `UAFLWeaponCosmeticAsset` carriers — template off `Plugins/GameFeatures/AFLBagMan/Content/Cosmetics/Weapons/DA_AFL_Weapon_Aria.uasset`.
   `DA_AFL_Weapon_HandCannon_FANATICS_R` → `ED…_R`; `_L` → `ED…_L`. Set NeonColor #FF0D0D.
7. Catalog entries in `Plugins/GameFeatures/AFLBagMan/Content/Cosmetics/DA_AFL_CosmeticCatalog.uasset`:
   - `AFL.Weapon.HandCannon.FANATICS` → R carrier (resolves via `WeaponId`).
   - `AFL.Weapon.HandCannon.FANATICS.L` → L carrier (resolves via `LeftWeaponId`).
   Distribution = D6 least-restricted (tier operator's call; default Free/base for the pilot).

**Fire (D4 mixed — reuse the bases, no new fire C++)**
8. The cannon is a MOUNT; fire type = whatever that hand resolves to — Beam (`UAFLAG_BeamChannel_v2`), Pulse
   (`UAFLAG_Laser_Pulse`), or Rocket (`UAFLAG_Projectile_Base`), spawning from the maw `Muzzle`. **For the pilot,
   grant Pulse on both hands** to prove the rail, then vary. Bind the LEFT cannon's fire ability to
   **`InputTag.Weapon.Fire.Left`**; add an IA/IMC mapping (RMB → Fire.Left) in the input config.

**FBIK + recoil (Appendix A #5 — Control Rig, your lane)**
9. FBIK aims both forearms to converge on the crosshair (the craned maw is cosmetic — aim off control rotation,
   NOT the mesh axis). Additive recoil kicks the arm up per shot.

**PIE-watch**
10. (a) Single forearm mount first (right only, `LeftWeaponId` unset) — reads at `weapon_lowerarm_r`, aims via FBIK,
    fires from the maw. (b) Then **dual** — both hands, mixed fire, converge. ⚠ **This is the integration risk:**
    two `EquipItem`s must COEXIST against Lyra's QuickBar one-active-weapon assumption. Watch that equipping the
    left does not unequip the right and that both fire independently.

---

## 4. [C++ GAP] Test harness — writing `LeftWeaponId`

There is **no console cheat for `LeftWeaponId` yet** (the existing `afl.Cosmetic.Set*` cheats only touch single
axes). To drive a dual loadout at PIE you need one of:
- **(preferred)** a small cheat `afl.Cosmetic.SetLeftWeapon <id>` mirroring the existing `SetWeapon` — **ping me, ~5 min C++**, or
- set both `WeaponId` + `LeftWeaponId` on the selection struct via a temporary editor utility / `ServerSetCosmeticSelection` call.

Flag me the moment you're ready to PIE and I'll add the cheat so you can `SetWeapon AFL.Weapon.HandCannon.FANATICS`
+ `SetLeftWeapon AFL.Weapon.HandCannon.FANATICS.L` and watch both arms arm up.

---

## 5. Done-when
Both forearm cannons visible on the correct arms, aiming via FBIK, firing independently (RMB left / LMB right),
mixed fire types honored, and the right cannon survives equipping the left (coexistence proven). Then brand maws
+ color tints across the line are cheap follow-on gens (the TYPE conform is already proven by FANATICS).

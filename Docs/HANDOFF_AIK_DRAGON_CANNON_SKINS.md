# HANDOFF → AIK — Electric-Neon Dragon Hand-Cannon skins (batch)

**From:** Claude Code (gen + conform) · **To:** AIK (UE assembly) · **Date:** 2026-07-28
**Base:** the FANATICS2 straight-barrel dragon (already armed + aligned). These are **brand skins of that same
dragon cannon** — generated on Rodin Gen-2.5 (Extreme-High + HighPack + micro) from the operator's per-brand
reference sheets, then run through the standard arm-worn conform.

## Delivered (in `Content/AFL/_Bridge/Blender/pending/20260728-glove-cannon/`)

| Brand | Meshes | Maps (4K) | Neon color | Catalog id |
|---|---|---|---|---|
| **IRONICS** | `SK_AFL_HandCannon_IRONICS_R/_L.fbx` | `*_IRONICS.png` | electric blue | `AFL.Weapon.HandCannon.IRONICS[.L]` |
| **SIMULARENT** | `..._SIMULARENT_R/_L.fbx` | `*_SIMULARENT.png` | neon green | `AFL.Weapon.HandCannon.SIMULARENT[.L]` |
| **DRAGON_SOUL** | `..._DRAGON_SOUL_R/_L.fbx` | `*_DRAGON_SOUL.png` | blue lightning | `AFL.Weapon.HandCannon.DRAGONSOUL[.L]` |
| **RUN_IT_BACK** | `..._RUN_IT_BACK_R/_L.fbx` | `*_RUN_IT_BACK.png` | teal green | `AFL.Weapon.HandCannon.RUNITBACK[.L]` |
| **FUTURE_WARRIOR** | `..._FUTURE_WARRIOR_R/_L.fbx` | `*_FUTURE_WARRIOR.png` | magenta/purple | `AFL.Weapon.HandCannon.FUTUREWARRIOR[.L]` |

Each mesh (identical spec to FANATICS2): **40 cm** muzzle axis, **cuff at pivot/origin**, **muzzle = +Y**,
horns **+Z**, single-root rig (**R bone `root` · L bone `root_l`**), ~44k tri LOD0, 4K PBR.
Maps: `Color_*` (albedo) · `Normal_*` (Rodin normal — **verify GL vs DX; flip Green if GL**) ·
`ORM_*` (R=AO white · G=Rough · B=Metal).

## ✅ Branding is BAKED — no decal needed
Unlike FANATICS2 (F via decal), each brand's **wordmark + roundel is baked into the skin** (it was in the
reference the reskin was generated from). So the mesh already reads IRONICS / SIMULARENT / 龙魂 / 다시 간다 etc.
— no UE emissive decal required for these. (Decal path still available if you want swappable marks later.)

## AIK assembly (same rail as FANATICS2, repeat per brand)
1. Import `_R` + `_L` + the 3 maps (ORM→Masks/Linear; Normal flip if GL; Import Normals+Tangents; UE LODs).
2. `MI_AFL_HandCannon_<BRAND>` off `M_AFL_Weapon_PBR` — Color/Normal/ORM; set **NeonColor** to the brand color
   above (drives glow + the default beam/muzzle tint).
3. Two `ULyraEquipmentDefinition`s per brand @ the forearm sockets **`weapon_lowerarm_r` / `weapon_lowerarm_l`**
   (NOT `weapon_r`); `Muzzle` socket at the +Y open bore.
4. Carriers → catalog ids above. Fire type = the hand's WeaponId (Beam/Pulse/Rocket) from the maw — no new fire C++.
5. Rides the **dual-mount C++ that already landed** (`LeftWeaponId` → `RefreshHandCannonsForPawn` →
   `weapon_lowerarm_*`; `InputTag.Weapon.Fire.Left`; `afl.Cosmetic.SetLeftWeapon` cheat). FBIK aim + additive recoil.

## Notes
- All five share the **same dragon base topology** (variant skins) — the skeleton + socket setup is identical
  across them, so wire one and clone the rig/socket config for the rest.
- Source Rodin tasks: IRONICS `9924ec21` · SIMULARENT `40a96418` · DRAGON_SOUL `5d101a39` (v2) ·
  RUN_IT_BACK `4bdc78d6` (v2). Conform recipe: `reference-rodin-gen-pipeline` memory + the FANATICS2 spec.
  RUN_IT_BACK `4bdc78d6` (v2) · FUTURE_WARRIOR `byin0q9j4`/v3 (tight body-only crop; fused floating-barrel fixed).

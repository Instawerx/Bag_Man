# HQ Weapon Recipe — FORWARD STANDARD (piloted on Seeker, 2026-07-21)

The locked high-quality gen+conform+PBR+branding recipe. Pilot = SK_AFL_Seeker. Batch the other 5
only after operator look-approves this one.

## 1. TRIPO GEN — highest usable tier (openapi v2)
```
POST https://api.tripo3d.ai/v2/openapi/task
type = text_to_model
model_version   = "v3.0-20250812"     ← NOT "v3.0" (2017 error) and NOT the dated-only guess
geometry_quality = "detailed"          ← Ultra geometry (up to ~2M tris). KEY quality driver.
texture = true,  pbr = true            ← full PBR map set
# NOTE: texture_quality="detailed" is REJECTED (1004) on text_to_model — omit it. PBR gives 2K maps anyway.
negative_prompt += "text, letters, words, logo, writing, engraving, serial number, numbers"
                    + the anti-slab/figure/stand terms.   (branding comes from UE, not the gen)
```
Seeker gen: task 65f740cf, **40 credits**, output = 1.93M-tri GLB (55 MB), 2048² PBR.

## 2. DOWNLOAD ALL MAPS  ⚠ THE PRIOR BUG-FIX
Tripo embeds textures IN the GLB (no separate map URLs). Import the GLB in Blender and **unpack every
image** — do NOT hand-pick one. v3.0 PBR outputs exactly THREE 2048² maps:
- `Color_<id>.png`     — base color / albedo (sRGB)
- `NormalGL_<id>.png`  — normal, **OpenGL convention** → UE must **flip Green** (DirectX) on import
- `ORM_<id>.png`       — packed **R=AO, G=Roughness, B=Metallic** (Linear/no-sRGB)
All three confirmed on disk at `textures/`. (Blender: `image.filepath_raw=...; image.file_format='PNG'; image.save()` per image.)

## 3. CONFORM (SK-held recipe, tuned for the new bar)
- Rotate barrel → **+Y** (this gen: barrel along X, muzzle +X → rotate mesh data +90° Z).
- Uniform-scale to class dims (Seeker already ~100 cm → scale 1.0).
- **Re-origin to grip → world 0**, full mesh kept (no cutting). Launcher-block hold point (no distinct
  pistol grip on this gen — AIK grip-verifies).
- **Less-aggressive decimate**: LOD0 = **150k** COLLAPSE (1.5× the old 100k; 2K normal carries micro-detail).
  Keep the 1.93M high-poly backup (`SM_AFL_Seeker_HIGH`) in .blend for LOD source / rebakes.
- One-bone `root` armature at grip (+Y), skin whole mesh → root.
- Sockets (coords, not baked): **Muzzle** (0, 34.85, 6.54) · **GripPoint_L** (0, 24.0, 2.31).
- Export FBX with locked flags **+ `use_tspace=True`** (tangents, required for the normal map).
  Round-trip verified: `bones==["root"]`, grip@0, 150k, UV `UVMap`, 1 mat.
- LODs: UE-generated off the 150k LOD0 (lod_count 4), OR Blender LOD chain off the high-poly.

## 4. PBR MATERIAL — M_AFL_Weapon_PBR (AIK builds in UE)
Master material, parameters expose per-weapon controls:
- **BaseColor** = Color map (sRGB).
- **Normal** = NormalGL map, **Green flipped** (Tripo GL → UE DirectX). Tangent-space normal.
- **ORM** (Linear) → split: R→AmbientOcclusion, G→Roughness, B→Metallic.
- **NeonColor** (Vector param, electric palette; Seeker = #1E5AFF) → drives:
  - Emissive accent: mask the glow areas (threshold the Color blue/saturation, or a baked emissive mask) × NeonColor × EmissiveIntensity.
  - Tie EmissiveIntensity to the weapon's heat/charge state if wired.
- **Branding-decal slot** (see §5).
MI_AFL_Seeker instanced off this; set NeonColor + map refs.

## 5. BRANDING — UE emissive decal (Option B, crisp/recolorable/regen-independent)
- Asset authored: `branding/T_AFL_Branding_IRONICS_BetaLands_Mask.png` (2048×512, white text on transparent —
  legible "IRONICS" + "BETA LANDS", letter-spaced etched look). Recolored by NeonColor in-material.
- **Method (recommended): deferred Decal actor / decal material** projected onto a flat body panel — art-directed
  placement, movable, recolorable, no UV surgery, no regen. Decal = Emissive(mask × NeonColor) + optional roughness etch.
- Alt: second UV channel with the mask UV-placed on a chosen panel (baked-in, less flexible).
- Repeatable for the batch: same mask, per-weapon NeonColor + placement transform.

## 6. IMPORT + SWAP (AIK) — see AIK_FINISH.md
Import SK_AFL_Seeker.fbx + the 3 maps → build/instance MI_AFL_Seeker → decal → swap onto B_AFL_Seeker →
operator PIE-checks the LOOK (HOST→INFINEON).

## Credits: gen 40 cr (vs 20 old). Balance after ≈ 1885. Batch of 5 more ≈ 200 cr.

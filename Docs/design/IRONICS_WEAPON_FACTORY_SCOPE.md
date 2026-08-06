# IRONICS -- WEAPON FACTORY (50+ beam weapons) -- SCOPE + NOTES

Operator-approved pipeline: produce 50+ beam/energy weapons by modifying base PARENTS (shipped
beam weapons + purchased packs) via Claude Desktop + Blender, each PIE-proven (emblem discipline,
scaled). This doc is the durable weapon-factory notes + the disk-true base inventory. Full
pipeline scope (mod vocabulary, parallel-tasking, per-variant pipeline, naming, risk) is in the
2026-07-01 session record.

> ⚠ **VERIFY WEAPON ASSET NAMES AGAINST DISK BEFORE CITING.** Weapon comments/docs here have gone stale
> repeatedly — on 2026-07-19 the `GA_AFL_GrantLoadout` header comment, THIS doc's "Beam_v2", and a project
> memory all cited a `Beam_v2` weapon that has **no asset on disk** (`glob *Beam_v2*` = 0). Glob/readback any
> weapon asset before relying on its name. **The disk-true arsenal SSOT + conform-to authoring chain is
> `Docs/IRONICS_WEAPON_AUTHORING_SPEC.md`** (§7 = the live 16-WID inventory).

## STATUS
- Scope APPROVED 2026-07-01. Pilot = 3 weapons (canary) before scaling to 50+.
- Base set + 50-target CONFIRMED (2026-07-01). Factory now has **3 variant axes** (below).
- Skin axis BUILT solo (material + 2 IRONICS tints). Mesh-axes (B convert / C attach) gated on
  Desktop-Blender + the 5.4 import. Pilot proves all 3 axes before scaling.

## TIER SYSTEM + LOCKED MATERIAL (operator doctrine, 2026-07-02)
- **TIER SYSTEM.** The factory branches by tier: **BASE TIER** = fast Blender conversions of
  existing base meshes; minor artifacts OK (e.g. the Arclight energy-cell reads as a block-overlay
  greeble -- ACCEPTED at base tier, NOT sent back to Blender, because the material carries the AAA
  look). **MID / TOP TIER (AAA)** = base meshes must be smoothly integrated (no block overlays;
  parts modeled/beveled to flow with the frame) -- heavier Blender investment or purpose-built
  bases; the block-mag reshape (taper/bevel/integrate) is a MID-tier build step, not a base fix.
  Tier = base-mesh quality + effect layers (glitch/legendary), NOT the material (shared).
- **LOCKED FACTORY MATERIAL = `M_AFL_WeaponSkin_NeonCamo`** (`/Game/Weapons/AFL/Skins/`,
  operator-approved AAA 2026-07-02). Procedural liquid-glass plasma, Clear Coat shading;
  **OBJECT-SPACE TRIPLANAR base** = flows over any geometry regardless of UV quality (factory-safe
  for imperfect conversion UVs); UV-driven emissive glow; Fresnel rim; glitch static switch
  (clean vs legendary); `MPC_WeaponState` firing-reactive (FiringIntensity/FiringGlitchFrequency;
  BP fire-timeline + Niagara muzzle-light = editor steps for the reactive/light PIE proof); mobile
  `_Mobile` variant + channel-pack deferred post-lock. Params (economy lever): Color1/2/3,
  Neon_Glow_Intensity, Fresnel_Exponent, SkinScale (triplanar density), Glitch_*, Enable_Glitch_Elements.
  Build/tweak via `Saved/build_weapon_mat.py` (exec through the bridge).
- **ECONOMY PROVEN:** 8 distinct neon/combo instances from the one master (ElectricBlue/ArcViolet/
  ToxicGreen/IceCyan/Amber singles; CyanMagenta/GreenGold combos; GlitchLegend legendary) -- zero
  new assets each. "Dual neon" = a 2-slot assignment (Body one MI + Emitter another).
- **Arclight (Weapon B) = a BASE-TIER keeper** (block mag accepted). Remaining to close: the
  gameplay PIE (equip/fire/beam/coexist) -> commit the base-tier set.

## DEFERRED -- recover later (never-delete)
- **Modern Guns Bundle 5.7 = DEFERRED.** All cooked `.uasset` (629 assets, 0 FBX, 3.36 GB),
  built for UE **5.7 > the 5.6 project** -> UE 5.6 refuses to load newer-version packages
  (verified 2026-07-01). NOT in the current base set.
  **Recovery path:** open the pack in a UE 5.7 editor -> export meshes to FBX -> reimport to 5.6;
  OR re-acquire a 5.6-compatible build from the store listing. Revisit when either is available.

## THE REAL BASE SET (re-inventoried from disk 2026-07-01)

### Customizable Weapon Pack 5.4 -- migratable (5.4->5.6 forward-upcook; 748 .uasset, 1.38 GB)
**~15 FIREARM bases (NOT the ~8 estimated -- disk-true count):**
- Pistols (4): DE-42, FPN16/NFP-16, Judge-45, **PR-9 (already a "Raygun" -- energy aesthetic, easiest convert)**
- SMGs (3): GTM, PP9, V014
- Assault (3): ACWI, AK-110, M4
- Shotguns (2): M890, Remore-046
- Snipers (2): CM-2000, SCB_750
- LMG (1): N90
- (non-beam-gun: GrenadeLauncher, 3 knives, grenades -> feed the magnet/EMP line if wanted, not the beam count)

**MODULAR ATTACHMENT SYSTEM = the factory advantage.** The pack is DESIGNED to recombine:
enums `Barrel_Enum / ForeEnd_Enum / Grip_Enum / Scope_Enum / PaintMode_Enum` + 54 `SM_` parts
(scopes ACOG/Holo/RedDot/Reflex/SVS16X, grips Angled/VertA/B/C, barrels FlashHider/MuzzleBrake/
Silencer/Suppressor, bipod, per-weapon sights/mags) + paint/skin modes. Base x barrel x foreend
x grip x scope x paint = large combinatorics BEFORE any Blender. Recombining parts IS what the
factory does -- this pack ships that machinery.

### Shipped beam weapons -- in-project, PROVEN parents  (CORRECTED 2026-07-19, disk-verified)
The base weapons that ship with **own-WID full chains** and sit in the hero loadout
(`GA_AFL_GrantLoadout.Weapons`): **PulseCarbine, ShotgunBeam, Pistol** (3). There is **NO `Beam_v2`
ItemDef/WID on disk** — the earlier "Prism Beam (Beam_v2)" name never shipped as an asset. The live beam
weapon is **ShotgunBeam** (`ID_AFL_ShotgunBeam → WID_AFL_ShotgunBeam → GA_AFL_Beam_Shotgun`, a BP child of
the C++ `UAFLAG_BeamChannel_v2`). Beyond the 3 base, **10 factory-converted weapons** (Tempest, Vanguard,
Breacher, GTM, PP9, N90, Remore, DE42, Judge45, NFP16) exist as WID+instance+mesh but have no ItemDefs yet;
Arclight/Voltaic/Ioncaster have ItemDefs (own-WID as of the 2026-07-19 mis-wire fix). Full list:
`IRONICS_WEAPON_AUTHORING_SPEC.md` §7.

### REAL BASE COUNT: ~15 (5.4 firearms) + 4 (shipped) = **~19 firearm bases**
(Corrects the stale estimates: ~28 assumed the now-deferred 5.7 pack; ~12 assumed only 8 for 5.4.
Disk-true with 5.7 deferred = ~19.)

## 50-TARGET MATH (PENDING operator confirm)
- ~19 bases -> 50+ = **~2.6 variants/base**. Comfortably achievable -- 50 is a floor, not a stretch.
- WHY it is easy: the 5.4 modular attachment system yields many variant-silhouettes per base with
  ZERO Blender (base x barrel/grip/scope/paint). Blender is reserved for the ballistic->beam
  CONVERSION remesh + custom hero greebles. Rebeam (BeamSystem + BeamColorOverride) gives the
  per-weapon beam identity. So the Blender burden is far lower than model-each-from-scratch.
- RECOMMENDATION: 50+ holds easily and could exceed. Operator confirms 50+ or sets a number.

## THE 3 VARIANT AXES (cheapest -> most-expensive; the pilot proves each)
1. **SKIN / MATERIAL (texture -- cheapest, highest multiplier).** 1 mesh x N skins = N variants,
   zero mesh work. NEW axis. Method + built proof below.
2. **ATTACHMENT RECOMBINE (zero-Blender, cheap).** 5.4 modular system (base x barrel/foreend/grip/
   scope/paint) -> distinct silhouettes, no mesh work.
3. **MESH MOD (Blender, most work).** Ballistic->beam conversion, remesh, greebles. **Blender
   MODIFIES the clean 5.4 mesh** (keeps topology / UV / rig) -- **NOT Tripo.** Tripo regenerates
   and DEGRADES clean meshes; Tripo is reserved for from-scratch generation only.

## SKIN AXIS -- method chosen + BUILT (2026-07-02, solo, no Blender/import)
**METHOD = PROCEDURAL parameterized material (the spine) + optional AI-texture PATTERN MASKS
(hero richness).** Why procedural is the factory fit:
- One material; `NeonColor` is a parameter -> re-tint to ANY IRONICS color = infinite color
  variants FREE, zero per-skin asset (matches the WEAPONS_SSOT "color = parameter swap" law + the
  proven skin-pillar finish-as-parameter architecture + the BeamColorOverride pattern).
- AI-texture's value is richer PATTERNS -> feed those in as grayscale MASKS the material tints =
  best of both (rich pattern + free color). Pattern-mask gen is a Desktop/image-gen step (like
  Blender), NOT required for the color multiplier.
- So: procedural = the multiplier (cheap, infinite color); AI-mask = pattern enrichment (later).
**BUILT this pass (solo, readback-verified) at `/AFLBagMan/Weapons/Skins/`:**
`M_AFL_WeaponSkin_NeonCamo` (Voronoi/Noise cellular pattern x `NeonColor` x `EmissiveIntensity` on
a dark base; params NeonColor/BaseDark/EmissiveIntensity) + 2 tint instances
`MI_..._ElectricBlue` + `MI_..._ArcViolet` -- proving one material -> N IRONICS colors free.
PENDING: apply-to-weapon + operator PIE ("reads AAA") -- needs a pilot weapon or a shipped-weapon
target (see gates below).

## PIPELINE NOTES (from STEP-0 findings)
- Packs are `.uasset`-only (0 FBX) -> pipeline needs an added step: UE-load -> export mesh to FBX
  -> Blender-mod -> reimport. (Blender cannot open `.uasset`.)
- Blender mesh-mods run on **Claude Desktop** (`blender_mcp`); UE import + WID-grant wire + beam
  (`UAFLBeamVisualComponent`/`BeamChannel_v2`) + PIE-prove run on Claude Code. Coordinate via the
  AFL bridge manifests.
- Full 5.4-pack import = 1.38 GB into the LFS repo -- an operator-owned decision; a targeted
  single-gun migration-verify is the lighter pre-pilot check.

Cross-refs: `IRONICS_WEAPONS_SSOT.md` (10-type base-set the factory derives from), the beta-launch
assets record, skills `afl-laser-beam-system` + `afl-asset-pipeline`.

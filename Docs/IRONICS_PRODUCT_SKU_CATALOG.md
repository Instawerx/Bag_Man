# IRONICS -- MASTER PRODUCT / SKU CATALOG (living doc)

**Human-readable mirror of the machine SKU registry.** This doc is the at-a-glance catalog of
every sellable / earnable / equippable unit in BAG MAN. The MACHINE registry is the data asset
`DA_AFL_CosmeticCatalog` (class `UAFLCosmeticCatalog`, `/AFLBagMan/Cosmetics/`); this doc is its
human mirror. **Both stay in sync -- every new SKU registers in BOTH as it is created** (see
the Register-As-Created rule at the bottom).

- Machine registry (SSOT of record): `DA_AFL_CosmeticCatalog` -- read live via
  `UAFLCosmeticCatalogSubsystem`.
- Pricing/scarcity SSOT: `Docs/IRONICS_PRICING_SCARCITY_SSOT.md`.
- Economy SSOT: `Docs/IRONICS_ECONOMY_SPEC.md`. Architecture: `Docs/AFL_ECONOMY_ARCHITECTURE_ADR.md`.
- **Full product matrix (the build target): `Docs/IRONICS_CATALOG_MATRIX.md`** -- weapons x patterns x colors x beams x pulses, each cell a SKU with build/pending status. This doc is the flat per-SKU mirror; the matrix is the complete plan we build to (nothing skipped).
- Snapshot: **272 registered SKUs** (2026-07-03) = 122 baseline + 3 factory weapons x 50. **49 color MIs BUILT** (real); the 48-color grid is **REGISTERED but PENDING-PROOF** -- the runtime color-APPLY is a GAP (the WeaponId consumer equips the weapon but ignores the `.<Color>` suffix -> the baked default shows). Only the 3 baked defaults are PIE-PROVEN; the rest await the runtime-apply wire + PIE. Full palette + status: `IRONICS_CATALOG_MATRIX.md`.
- **Arclight PIE-PROVEN + committed `7c6ff612`** (2026-07-02): the own->select->equip->fire arc proven live (grant -> `SetCosmeticWeapon` -> Arclight equips + fires; camo/grip/montage/L4/L5 all watched). **#43 WeaponId->equip consumer CLOSED** (the session-long "axes replicate but nothing consumes them" gap). Arclight = the **COMPLETE factory template** the volume build replicates from. One deferred fleet-wide item: support-hand IK -> the operator-spec'd IK-system overhaul (not per-weapon) -- **now PROVEN in PIE** (comp-space `UAFLWeaponIKComponent` -> CR; per-weapon = a `GripPoint_L` mesh socket).
- **Voltaic (axis A) REGISTERED `2026-07-03`:** the 2nd factory weapon (mesh-mod on the shipped ShotgunBeam) -- 9 SKUs (base + 8 skins), carrier `DA_AFL_Weapon_Voltaic`, ElectricBlue NeonCamo skin + electric-blue beam + `GripPoint_L` socket. **PIE-PROVEN `2026-07-03`** -- equip/fire/beam/skin/grip all watched: neon-blue skin, matched blue beam, grip correct, AAA.
- **Ioncaster (axis C) REGISTERED `2026-07-03`:** the 3rd/last factory weapon (attachment-recombine, UE-side zero-Blender) -- V014 SMG base (`SK_IRONICS_Ioncaster`) + Holographic-scope + suppressor attachments, 9 SKUs, carrier `DA_AFL_Weapon_Ioncaster`, ArcViolet NeonCamo skin + matched violet beam + `GripPoint_L` socket; accessories skinned (scope ArcViolet, suppressor CyanMagenta). **PIE-PROVEN `2026-07-03`** -- weapon + beam + grip all watched, AAA. **COMPLETES THE PILOT** (all 3 axes proven).
- **RETIRED-COUPLING CLEANUP `2026-07-10`:** the per-weapon `AFL.Weapon.<W>.<Color>` grid is **REMOVED** -- 149 verified-dead rows deleted (49 each Voltaic/Arclight/Ioncaster + 2 strays; nothing owned/selected/parsed them, every color exists universally). Weapons are now **4 base rows** (`AFL.Weapon.<W>`: Arclight/Voltaic/Ioncaster/**Tempest**) + the **universal `AFL.WeaponSkin.NeonCamo.*` axis (49)** -- individual-asset model, single truth. Catalog **400 -> 251 rows**. **Tempest** = the M4-based general-catalog canary (mesh-on-child via panel; `B_Weapon` un-poisoned), PIE-equipped.
- **BEAM-WAVE BATCH 1 `2026-07-10`:** +2 general-catalog beam weapons PIE-PROVEN AAA + committed -- **Vanguard** (ACWI assault, CobaltDeep) + **Breacher** (M890 shotgun, SolarFlare), conform-to-Tempest (mesh-on-child via panel, self-contained to committed skin). Now **6 base weapons**. Snipers (CM-2000/SCB_750) DEFERRED -- scope-confounded barrel + no barrel bone, the Y=0 Muzzle template doesn't fit; need a Blender Muzzle-bone pass.
- **BEAM-WAVE BATCH 2 `2026-07-10`:** +4 general-catalog beam weapons PIE-PROVEN AAA + committed -- **GTM** (SMG, NeonScarlet) + **PP9** (SMG, HotPink) + **N90** (LMG, EmeraldPlasma) + **Remore** (shotgun, GoldPlasma). Muzzle-Y refinement (Muzzle.Y = mesh origin_y, not assumed 0) fixed the near-centered offsets (N90/Remore). Batch now covers assault+shotgun+SMG+LMG (all beam types). **10 base weapons**; beam side effectively closed (snipers deferred). Remaining general-catalog = 4 pulse pistols.
- **PULSE WAVE -- general-catalog BUILD COMPLETE `2026-07-10`:** +3 pulse pistols PIE-PROVEN AAA + committed -- **DE42** (CrimsonArc) + **Judge45** (NeonOrchid) + **NFP16** (IceCyan), 1H on the Pistol base (PulseFire, Muzzle-only per the 1H taxonomy -- no GripPoint_L). **PR-9 Raygun DEFERRED** -- pistol mesh on the 2H beam base fights (oversized/mis-held); a beam raygun needs a rifle-sized mesh or a 1H-beam base (future). **GENERAL-CATALOG WEAPON BUILD COMPLETE: 13 weapons (10 beam + 3 pulse); PR-9 + 2 snipers deferred.** NEXT = the pricing pass (DIRECT prices -> purchasable via the proven wallet loop) to close the general-catalog economy.

================================================================================
## VOCABULARY LOCK -- the REAL on-disk names (use these, not paraphrases)
================================================================================
Verified on disk 2026-07-02 (grep + live editor). Earlier loose terms corrected:

| Loose term (retired) | REAL on-disk name | What it actually is |
|---|---|---|
| "the Database" / "persistence backend" | **PlayFab** (backend), via `IAFLCosmeticPersistence` (interface, STUB now) -> planned `AFLBackend` plugin (Sprint 13 / Phase 3) | cross-session ownership + currency store. **NOT built yet -- the economy blocker.** |
| "UAFLCosmeticDefinition" (does not exist) | **`FAFLCatalogEntry`** (USTRUCT row) in **`DA_AFL_CosmeticCatalog`** -> `Asset` points at **`UAFLSkinColorAsset`** (skins/finishes/facemasks), `UAFLAbilityCosmeticAsset` (EMP), or a `ULyraEquipmentDefinition` class (weapons) | the SKU definition (machine side) |
| "Cosmetic.Tier.* tags" | **`EAFLCosmeticTier` ENUM** {SPARK, SURGE, ARC, THUNDERBOLT} (a field on the row) | the paid tier. NOT a tag. **Rarity** IS a tag: `Cosmetic.Rarity.*` (via `RarityTag`). |
| wallet / entitlement | **`UAFLWalletComponent`** (`OwnedCosmeticIds`, `IsEntitled`, `ServerPurchaseCosmetic`) = the `IAFLEntitlementSource` | ownership + purchase gate (in-session; persistence via PlayFab) |
| selection / equip | **`UAFLCosmeticLoadoutComponent`** (`FAFLCosmeticSelection` on LyraPlayerState) | the #43 loadout seam |
| catalog access | **`UAFLCosmeticCatalogSubsystem`** (FindEntry / ResolveAsset / GetPurchasableEntries / GetEntryPriceText) | reads the catalog for store/wallet/selection |
| store UI | **`AFLW_Menu_CosmeticShop`** + **`AFLW_Menu_StoreTile`** (`Content/BagMan/UI/Store/`) | the shop widgets |
| currency | **Volts** (V, hard) + **Watts** (W, soft) | peg 1V=$0.001, 10W=1V (integer, never float) |

CosmeticId format (immutable key, ADR D3): **`AFL.<Type>.<Name>`** (fully type-qualified; never
bare name; never encode color in identity names).

================================================================================
## SUMMARY -- 272 registered SKUs by type
================================================================================
| Type (`EAFLCosmeticType`) | Count | Acquisition | Notes |
|---|---|---|---|
| CHARACTER | 30 | GRANTED_FREE | identity; outside the paid ladder |
| TEAM | 7 | GRANTED_FREE | identity; outside the paid ladder |
| FINISH | 39 | mixed | full-body finish (color); 10 `AFL.Body.*` @ SPARK, rest `AFL.Finish.*` (free base + RARE named) |
| FACEMASK | 32 | mixed | faceplate; 10 Basic free, flags RARE, icons/riot LEGENDARY |
| SKIN_COLOR_EDGE | 11 | DIRECT | edge-glow color @ SPARK |
| WEAPON | 13 base | DIRECT | `AFL.Weapon.<W>` base rows only: Arclight/Voltaic/Ioncaster/Tempest/Vanguard/Breacher/GTM/PP9/N90/Remore/DE42/Judge45/NFP16. Per-weapon `.<Color>` grid REMOVED 2026-07-10 (149 rows). Colors = universal `AFL.WeaponSkin.*` (49). |
| ABILITY_COSMETIC | 1 | DIRECT | EMP @ ARC |

**Paid tiers (`EAFLCosmeticTier`):** SPARK ($10 = 10,000V / 100,000W, Watts-buyable) · SURGE ($16 =
16,000V) · ARC ($23 = 23,000V) · THUNDERBOLT ($30 = 30,000V). FLICKER ($1-2.50, base extras) +
limited-edition rarity tiers (Static->Singularity) are in the pricing SSOT, enforcement PlayFab-gated.

================================================================================
## THE CATALOG (all 272, by type)
================================================================================

### WEAPON (152) -- `AFL.Weapon.<Base>.<Color>`  (rows below = the legacy-8 sample; full 48-grid in the matrix)
| CosmeticId | Tier | Acq | Rarity | Price | Source |
|---|---|---|---|---|---|
| AFL.Weapon.Pistol.NeonGreen | SPARK | DIRECT | Common | V0 W0 | (existing) |
| AFL.Weapon.ShotgunBeam.NeonBlue | SPARK | DIRECT | Common | V0 W0 | (existing) |
| AFL.Weapon.Arclight | SPARK | DIRECT | Common | V0 W0 | factory base |
| AFL.Weapon.Arclight.ElectricBlue | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Arclight.ArcViolet | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Arclight.ToxicGreen | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Arclight.IceCyan | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Arclight.Amber | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Arclight.CyanMagenta | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Arclight.GreenGold | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Arclight.GlitchLegend | SURGE | DIRECT | Legendary | V0 W0 | factory legendary |
| AFL.Weapon.Voltaic | SPARK | DIRECT | Common | V0 W0 | factory base (2026-07-03, axis A) |
| AFL.Weapon.Voltaic.ElectricBlue | SPARK | DIRECT | Common | V0 W0 | factory skin (mesh default) |
| AFL.Weapon.Voltaic.ArcViolet | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Voltaic.ToxicGreen | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Voltaic.IceCyan | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Voltaic.Amber | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Voltaic.CyanMagenta | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Voltaic.GreenGold | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Voltaic.GlitchLegend | SURGE | DIRECT | Legendary | V0 W0 | factory legendary |
| AFL.Weapon.Ioncaster | SPARK | DIRECT | Common | V0 W0 | factory base (2026-07-03, axis C recombine) |
| AFL.Weapon.Ioncaster.ElectricBlue | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Ioncaster.ArcViolet | SPARK | DIRECT | Common | V0 W0 | factory skin (mesh default) |
| AFL.Weapon.Ioncaster.ToxicGreen | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Ioncaster.IceCyan | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Ioncaster.Amber | SPARK | DIRECT | Common | V0 W0 | factory skin |
| AFL.Weapon.Ioncaster.CyanMagenta | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Ioncaster.GreenGold | SPARK | DIRECT | Common | V0 W0 | factory combo |
| AFL.Weapon.Ioncaster.GlitchLegend | SURGE | DIRECT | Legendary | V0 W0 | factory legendary |

**2026-07-03 -- 48-COLOR GRID REGISTERED:** each factory weapon (Arclight/Voltaic/Ioncaster) now carries
the FULL 48-color grid = 36 SPARK (6 families x 6: Red/Blue/Green/Purple/Pink/Yellow) + 12 SURGE legendary
(2/family, prismatic/glitch) + GlitchLegend = **50 SKUs each** (150 total). The 8 rows/weapon above are
the legacy sample; the 41 new colors/weapon (29 SPARK + 12 legendary) are live in the machine catalog
(272 rows) -- full color list + build status in `IRONICS_CATALOG_MATRIX.md`. All via the locked
`M_AFL_WeaponSkin_NeonCamo` master (49 MIs). Weapon Volt/Watt pricing = a later pass (V0 for now).

### ABILITY_COSMETIC (1)
| CosmeticId | Tier | Acq | Rarity | Price |
|---|---|---|---|---|
| AFL.Ability.EMP | ARC | DIRECT | Common | V23000 |

### SKIN_COLOR_EDGE (11) -- edge-glow, all SPARK / DIRECT / Common / V10000 W100000
Crimson · Indigo · Lime · Magenta · NeonBlue · NeonGreen · NeonPink · NeonPurple · NeonRed ·
NeonYellow · Solar  (ids `AFL.Edge.<Name>`)

### FINISH (39) -- full-body finish
`AFL.Body.*` (SPARK / DIRECT / Common / **V10000 W100000**): Crimson, Indigo, Lime, Magenta,
NeonBlue, NeonGreen, NeonPink, NeonPurple, NeonRed, NeonYellow, Solar (11).
`AFL.Finish.*` free base (SPARK / GRANTED_FREE): Black, Blue, Green, Pink, Purple, Red, Yellow (7).
`AFL.Finish.*` named RARE (SPARK / DIRECT / Rare / V0 -- pricing pending): Black.Akuma, Black.Sable,
Blue.Cosmic, Crimson, Cyan.Azure, Cyan.Cielo, GlossBlack, Gold.Aurelia, Green.Jade, OnyxChrome,
Orange.Ember, Orange.Solar, Red.Dragon, Red.Talon, Steel.Ronin, Teal.Rift, Violet.Astra,
Violet.Indigo, Violet.Stealth, White.Halo, White.Valkyr (21).

### FACEMASK (32) -- faceplate (`AFL.Facemask.<Name>`)
| Group | Ids | Tier | Acq | Rarity | Price |
|---|---|---|---|---|---|
| Basic (10) | Brow, Cross, Dot, DotMatrix, Grille, Slit, Solid, Stripe, Twin, Visor | SURGE | GRANTED_FREE | Common | free |
| Flags (13) | Brazil, Canada, China, France, Germany, India, Indonesia, Korea, Mexico, Philippines, UK, Vietnam (Rare); Japan (Common) | SURGE | DIRECT | Rare | V8000 (Japan V5000) |
| Tech (1) | Tech.Circuit | SURGE | DIRECT | Rare | V8000 |
| Riot/Icon EPIC (3) | Riot.Anarchy, Riot.Traitors, IroVisor | SURGE | DIRECT | Epic | V16000 |
| Legendary (5) | Crew.JollyRoger, Icon.Kawaii, Icon.Orchard, Nation.Liberty, Riot.Freedom | SURGE | DIRECT | Legendary | V24000 |

> ⚠️ **RECLASSIFIED by `IRONICS_PRICING_SCARCITY_SSOT.md` §1.5 R1 (2026-07-08):** only **IRONICS** stays GrantedFree; the **29 non-IRONICS** identities (Character + their dual-registered Team rows) become **$500 / 500,000 V 1-of-1 Singularity bundles**. DOCS-ONLY — the `Acquisition` flip is the persistence-gated BUILD phase. The "ALL GRANTED_FREE" line below is retained as history.

### CHARACTER (30) -- identity, ALL GRANTED_FREE / SPARK (`AFL.Character.<Name>`)
AP-9, ARIA, Akuma, Astra, Aurelia, Azura, BigSixx, Cielo, Draco, Ember, FANATICS, Halo, IRONICS,
Kage, MAKHIAVELLI, MOB-FIGAZ, NovaKai, OnyxPrime, Orion, RiftOne, Ronin, Ryu, SCARLETT, Sable,
Solara, Talon, Valkyr, Vanta, Volt, Zen.

### TEAM (7) -- identity, ALL GRANTED_FREE / SPARK (`AFL.Team.<Name>`)
AP-9, ARIA, FANATICS, IRONICS, MAKHIAVELLI, MOB-FIGAZ, SCARLETT.

================================================================================
## WEAPON-FACTORY OUTPUT -- REGISTERED (LIVE 2026-07-02)
================================================================================
The 9 Arclight SKUs are now `FAFLCatalogEntry` rows in `DA_AFL_CosmeticCatalog` (122 -> 131),
matching the existing weapon-SKU footprint EXACTLY: Type=WEAPON, **Asset=None**, DIRECT,
ContentTier=BASE, V0/W0 -- the color is implied by the CosmeticId suffix (the existing convention;
the runtime color path resolves it). Base singles/combos = SPARK/COMMON (same shape as the existing
Pistol/ShotgunBeam SKUs); GlitchLegend = SURGE/LEGENDARY (the tier-doctrine legendary
differentiation). Pricing is V0 across the ENTIRE weapon category (matching the existing 2 -- weapon
Volt/Watt pricing is a separate pass; Tier + Rarity carry the base-vs-legendary distinction until
then). Live rows are in the WEAPON table above.

**VOLTAIC registered LIVE 2026-07-03 (131 -> 140):** the 2nd factory weapon (axis A -- mesh-mod on the
shipped ShotgunBeam). 9 `FAFLCatalogEntry` rows (`AFL.Weapon.Voltaic` base + 8 skins; same footprint
WEAPON / DIRECT / BASE / V0 W0; GlitchLegend = SURGE). Unlike the Arclight `Asset=None` note above (now
stale -- Arclight has a carrier too), Voltaic points every row at its own **carrier**
`DA_AFL_Weapon_Voltaic` (`AFLWeaponCosmeticAsset` -> `WID_AFL_Voltaic` -> spawns `SK_IRONICS_Voltaic`);
the #43 WeaponId consumer resolves it. Skin = `MI_AFL_WeaponSkin_NeonCamo_ElectricBlue` (Body); beam =
electric-blue `LaserTintColor`; support hand = `GripPoint_L` mesh socket. **PIE-PROVEN `2026-07-03`**:
equip spawned the Voltaic mesh, fired, neon-blue skin + matched blue beam (the `LaserTintColor` override
now PROVEN -- `GetBeamColor` honors it), grip correct, AAA. The mesh-mod-on-shipped-ShotgunBeam axis is proven.

**IONCASTER registered LIVE 2026-07-03 (140 -> 149):** the 3rd factory weapon = the attachment-recombine
axis (axis C, UE-side, zero-Blender). STEP-0 finding: the raw 5.4 pack bases have NO `Attach_*`/`Muzzle`
sockets and no modular BP -- so the recombine = a base-dup (`SK_IRONICS_Ioncaster` from `SK_V014_SMG`) +
attachment SM components (`SM_Holographic_Scope` + `SM_Square_Suppressor_Barrel`) seated by AUTHORED
transforms in `B_AFL_Ioncaster`, plus `Muzzle` + `GripPoint_L` mesh sockets. Carrier
`DA_AFL_Weapon_Ioncaster`; skin `MI_AFL_WeaponSkin_NeonCamo_ArcViolet` (Body); beam `LaserTintColor`
matched to the skin NeonColor (0.40,0.09,0.93); accessories skinned (scope ArcViolet, suppressor
CyanMagenta) to cover the stock part materials. **PIE-PROVEN 2026-07-03** -- weapon assembly + violet beam
+ grip all watched, AAA. HONEST: cheapest axis (no mesh mod, no Blender) but NOT near-zero -- the first
build authors the attachment transforms + accessory skins; future variants permute parts cheaply.
**THE PILOT IS COMPLETE -- all 3 factory axes proven.**

**Register-as-created status:** MACHINE registry (FAFLCatalogEntry rows) = DONE + HUMAN mirror (this
doc) = DONE. Remaining gate = the material's **gameplay-PIE** (equip/fire/beam/reactive on a live
pawn) -- the ✅ that flips these from "registered" to fully "done" per the =PIE doctrine.

**Not in this batch (still pending):** the DualNeon bundle (`AFL.Bundle.Arclight.DualNeon`) needs the
Bundle grant-set fields; BEAM / PULSE SKUs need the beam/pulse carrier + `BeamId`/`WeaponId`
consumer-wiring (the axes replicate but nothing equips them); full-material weapon-skin runtime apply
(the plasma material, not just a color) is the same deferred wiring. **PlayFab persistence remains the
economy blocker** -- registration != durable ownership.

================================================================================
## THE RULE -- REGISTER-AS-CREATED (both mirrors, part of definition-of-done)
================================================================================
A factory/cosmetic part is NOT "done" until ALL THREE hold:
1. **PIE-proof** (the AFL =PIE doctrine -- watched working), AND
2. **Machine registration** -- a `FAFLCatalogEntry` row in `DA_AFL_CosmeticCatalog`, AND
3. **Human registration** -- a row in THIS doc (the at-a-glance mirror).

The machine registry (`DA_AFL_CosmeticCatalog`) and this doc stay in sync. Nothing that is
sellable/earnable/equippable leaks -- if it can be owned, it is in both. Never delete; append/update
as the catalog grows. Pricing is owned by the pricing SSOT (keyed by CosmeticId), not duplicated here.

## THE BLOCKER -- PlayFab persistence (the real economy gap, held honestly)
Ownership does NOT persist across sessions today: `UAFLWalletComponent.OwnedCosmeticIds` is
session-only; `IAFLCosmeticPersistence` = null stub. **PlayFab** (the `AFLBackend` plugin, Sprint 13 /
Phase 3) is the backing store that owns/grants SKUs durably. Until it lands, the store cannot truly
sell -- a purchase is lost on logout. This is scoped as the NEXT major economy deliverable (after the
weapon pilot proves), behind the existing `IAFLCosmeticPersistence` seam (one swap point).

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
- Snapshot taken live from the editor 2026-07-02: **122 registered SKUs.**

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
## SUMMARY -- 122 registered SKUs by type
================================================================================
| Type (`EAFLCosmeticType`) | Count | Acquisition | Notes |
|---|---|---|---|
| CHARACTER | 30 | GRANTED_FREE | identity; outside the paid ladder |
| TEAM | 7 | GRANTED_FREE | identity; outside the paid ladder |
| FINISH | 39 | mixed | full-body finish (color); 10 `AFL.Body.*` @ SPARK, rest `AFL.Finish.*` (free base + RARE named) |
| FACEMASK | 32 | mixed | faceplate; 10 Basic free, flags RARE, icons/riot LEGENDARY |
| SKIN_COLOR_EDGE | 11 | DIRECT | edge-glow color @ SPARK |
| WEAPON | 2 | DIRECT | `AFL.Weapon.<Base>.<Color>` -- Pistol.NeonGreen, ShotgunBeam.NeonBlue |
| ABILITY_COSMETIC | 1 | DIRECT | EMP @ ARC |

**Paid tiers (`EAFLCosmeticTier`):** SPARK ($10 = 10,000V / 100,000W, Watts-buyable) · SURGE ($16 =
16,000V) · ARC ($23 = 23,000V) · THUNDERBOLT ($30 = 30,000V). FLICKER ($1-2.50, base extras) +
limited-edition rarity tiers (Static->Singularity) are in the pricing SSOT, enforcement PlayFab-gated.

================================================================================
## THE CATALOG (all 122, by type)
================================================================================

### WEAPON (2) -- `AFL.Weapon.<Base>.<Color>`
| CosmeticId | Tier | Acq | Rarity | Price |
|---|---|---|---|---|
| AFL.Weapon.Pistol.NeonGreen | SPARK | DIRECT | Common | V0 W0 |
| AFL.Weapon.ShotgunBeam.NeonBlue | SPARK | DIRECT | Common | V0 W0 |

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

### CHARACTER (30) -- identity, ALL GRANTED_FREE / SPARK (`AFL.Character.<Name>`)
AP-9, ARIA, Akuma, Astra, Aurelia, Azura, BigSixx, Cielo, Draco, Ember, FANATICS, Halo, IRONICS,
Kage, MAKHIAVELLI, MOB-FIGAZ, NovaKai, OnyxPrime, Orion, RiftOne, Ronin, Ryu, SCARLETT, Sable,
Solara, Talon, Valkyr, Vanta, Volt, Zen.

### TEAM (7) -- identity, ALL GRANTED_FREE / SPARK (`AFL.Team.<Name>`)
AP-9, ARIA, FANATICS, IRONICS, MAKHIAVELLI, MOB-FIGAZ, SCARLETT.

================================================================================
## WEAPON-FACTORY OUTPUT -- PRODUCED, **PENDING CATALOG REGISTRATION**
================================================================================
Committed to disk (foundation commit `73998a11`) but **NOT yet registered as `FAFLCatalogEntry`
rows** -- registration is blocked on the skin-carrier decision (a `UAFLWeaponSkinAsset` carrier +
full-material apply path; the `FAFLCatalogEntry.Asset` needs a `UPrimaryDataAsset`, and a factory
skin is a bare `MaterialInstanceConstant`). When that lands, these become the rows below:

| Planned CosmeticId | Type | Tier (planned) | Asset (on disk) | Status |
|---|---|---|---|---|
| AFL.Weapon.Arclight | Weapon | base (FLICKER/SPARK) | SK_IRONICS_Arclight + equipment | produced; pending row |
| AFL.Weapon.Arclight.ElectricBlue | Weapon (skin) | base | MI_...ElectricBlue | produced; pending row |
| AFL.Weapon.Arclight.ArcViolet | Weapon (skin) | base | MI_...ArcViolet | produced; pending row |
| AFL.Weapon.Arclight.ToxicGreen | Weapon (skin) | base | MI_...ToxicGreen | produced; pending row |
| AFL.Weapon.Arclight.IceCyan | Weapon (skin) | base | MI_...IceCyan | produced; pending row |
| AFL.Weapon.Arclight.Amber | Weapon (skin) | base | MI_...Amber | produced; pending row |
| AFL.Weapon.Arclight.CyanMagenta | Weapon (combo) | combo | MI_...CyanMagenta | produced; pending row |
| AFL.Weapon.Arclight.GreenGold | Weapon (combo) | combo | MI_...GreenGold | produced; pending row |
| AFL.Weapon.Arclight.GlitchLegend | Weapon (legendary) | legendary | MI_...GlitchLegend | produced; pending row |
| AFL.Bundle.Arclight.DualNeon | Bundle | combo | {ElectricBlue + ArcViolet} | produced (2-slot); pending row |

BEAM / PULSE: not yet cosmetic-representable -- the weapon-material-system's beam/pulse profiles
need the same carrier extension (`BeamId`/`WeaponId` axes replicate but are not consumer-wired).

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

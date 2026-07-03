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
- Snapshot: **140 registered SKUs** (2026-07-03) = 122 baseline + 9 weapon-factory Arclight (base + 8 skins) + 9 weapon-factory Voltaic (base + 8 skins), registered 2026-07-02 / 2026-07-03.
- **Arclight PIE-PROVEN + committed `7c6ff612`** (2026-07-02): the own->select->equip->fire arc proven live (grant -> `SetCosmeticWeapon` -> Arclight equips + fires; camo/grip/montage/L4/L5 all watched). **#43 WeaponId->equip consumer CLOSED** (the session-long "axes replicate but nothing consumes them" gap). Arclight = the **COMPLETE factory template** the volume build replicates from. One deferred fleet-wide item: support-hand IK -> the operator-spec'd IK-system overhaul (not per-weapon) -- **now PROVEN in PIE** (comp-space `UAFLWeaponIKComponent` -> CR; per-weapon = a `GripPoint_L` mesh socket).
- **Voltaic (axis A) REGISTERED `2026-07-03`:** the 2nd factory weapon (mesh-mod on the shipped ShotgunBeam) -- 9 SKUs (base + 8 skins), carrier `DA_AFL_Weapon_Voltaic`, ElectricBlue NeonCamo skin + electric-blue beam + `GripPoint_L` socket. **PIE-PROVEN `2026-07-03`** -- equip/fire/beam/skin/grip all watched: neon-blue skin, matched blue beam, grip correct, AAA.

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
## SUMMARY -- 140 registered SKUs by type
================================================================================
| Type (`EAFLCosmeticType`) | Count | Acquisition | Notes |
|---|---|---|---|
| CHARACTER | 30 | GRANTED_FREE | identity; outside the paid ladder |
| TEAM | 7 | GRANTED_FREE | identity; outside the paid ladder |
| FINISH | 39 | mixed | full-body finish (color); 10 `AFL.Body.*` @ SPARK, rest `AFL.Finish.*` (free base + RARE named) |
| FACEMASK | 32 | mixed | faceplate; 10 Basic free, flags RARE, icons/riot LEGENDARY |
| SKIN_COLOR_EDGE | 11 | DIRECT | edge-glow color @ SPARK |
| WEAPON | 20 | DIRECT | `AFL.Weapon.<Base>.<Color>` -- Pistol, ShotgunBeam (2) + Arclight base+8 skins (9, 2026-07-02) + Voltaic base+8 skins (9, 2026-07-03) |
| ABILITY_COSMETIC | 1 | DIRECT | EMP @ ARC |

**Paid tiers (`EAFLCosmeticTier`):** SPARK ($10 = 10,000V / 100,000W, Watts-buyable) · SURGE ($16 =
16,000V) · ARC ($23 = 23,000V) · THUNDERBOLT ($30 = 30,000V). FLICKER ($1-2.50, base extras) +
limited-edition rarity tiers (Static->Singularity) are in the pricing SSOT, enforcement PlayFab-gated.

================================================================================
## THE CATALOG (all 140, by type)
================================================================================

### WEAPON (20) -- `AFL.Weapon.<Base>.<Color>`
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

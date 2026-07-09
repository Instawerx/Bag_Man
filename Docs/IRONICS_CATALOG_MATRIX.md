# IRONICS -- CATALOG MATRIX (the full product model)

**The COMPLETE product set as a matrix.** Every weapon, every skin (pattern x color), every beam
color, every pulse color = its own full SKU (`FAFLCatalogEntry` row, made properly + registered).
Build to it COMPLETELY -- nothing skipped; **register-as-created** fills each cell as it is made.

- Flat registry mirror (per-SKU rows + running total): `Docs/IRONICS_PRODUCT_SKU_CATALOG.md`.
- Machine registry (SSOT): `DA_AFL_CosmeticCatalog` (`/AFLBagMan/Cosmetics/`).
- Status snapshot: **2026-07-03** -- weapon PILOT COMPLETE (3 axes); 49 color MIs BUILT (real assets).
  **Runtime skin-apply consumer BUILT** (applies the selected MI to the equipped weapon mesh) + **RE-KEYED to
  the independent `WeaponSkinId` axis** (aligned 2026-07-03: a skin is its OWN item applied to ANY weapon, NOT
  the retired per-weapon `AFL.Weapon.<W>.<Color>` suffix). Beam `BeamId` axis BUILT (same build). SKUs migrating
  **144 per-weapon skins -> 48 universal `AFL.WeaponSkin.NeonCamo.<Color>`**. **PENDING:** the one build (skin
  re-key + beam axis) + the migration + PIE-proof. Only the 3 baked ORIGINAL colors (Voltaic=ElectricBlue,
  Ioncaster=ArcViolet, Arclight) are PIE-PROVEN; pulses NOT started (need a `PulseId` axis).

> **STATUS 2026-07-04 (FULL PALETTE PIE-PROVEN via the `afl.Cosmetic.Cycle` harness + auto-fire):** the runtime
> cosmetic system is COMPLETE + PROVEN. The harness auto-cycled the whole palette on the listen-host; the log
> confirms **`DONE axis=WeaponSkin count=49`** + **`DONE axis=Beam count=49`** with **zero resolve failures** (no
> `mic=null` / `asset=null` in the driven axis; `ApplyBeamColor wrote LaserTintColor` x every beam), and the
> operator watched both cycles green. **49 WeaponSkin SKUs + 49 Beam SKUs PIE-PROVEN** -- each skin resolved its MI
> + applied to the mesh; each beam resolved its `DA_AFL_BeamColor` asset + wrote `LaserTintColor`, rendered live by
> the beam-axis auto-fire (`InputTag.Weapon.Fire` pulse). Both are INDEPENDENT axes (`WeaponSkinId` / `BeamId`),
> each applies to ANY weapon. The Cycle harness is the standing volume-proof tool (reusable for Pulse when its axis
> lands, #21). See THE INDEPENDENT-AXIS ECONOMY MODEL below.

================================================================================
## THE 5 AXES
================================================================================
1. **WEAPONS** -- base models harvested from Customizable Weapon Pack 5.4 (+ shipped beams). Each has
   ONE assigned **ORIGINAL** color (its identity default, a REQUIRED field) + can wear any other color.
2. **SKIN PATTERNS** -- the neon-liquid treatment; NeonCamo (locked) + more (pending).
3. **COLORS** -- **6 families x 6 SPARK + 2 SURGE legendary each = 48 neon-liquid colors** (the multiplier).
4. **BEAMS** -- beam-color cosmetics (one SKU per color = 48).
5. **PULSES** -- pulse-color cosmetics (one SKU per color = 48).

Multiplication (each cell = one SKU):
- **Weapon skins** = PATTERNS x 48 COLORS -- **UNIVERSAL** (aligned 2026-07-03: `AFL.WeaponSkin.<Pattern>.<Color>`,
  one skin applies to ANY weapon; NOT per-weapon). See THE INDEPENDENT-AXIS ECONOMY MODEL below.
- **Beams** = 48 COLORS.  **Pulses** = 48 COLORS.
- **Weapons** = the harvested bases (each = unique high-quality mesh + original color + addable colors).

================================================================================
## THE INDEPENDENT-AXIS ECONOMY MODEL  [CANONICAL -- every cosmetic is its OWN item]
================================================================================
**THE MODEL (locked 2026-07-03, operator-ruled).** Every cosmetic category is an INDEPENDENT ownable +
applicable ITEM -- its own SKU namespace, its own selection field in `FAFLCosmeticSelection`, its own
consumer. A player OWNS items per-category and MIXES them to build a loadout. **No category is a
sub-property of another.** This is LAW -- it stops the coupling drift hit twice (beam-into-skin, skin-into-weapon).

| Category | SKU namespace | Selection field | Consumer | Applies to |
|---|---|---|---|---|
| Identity | `AFL.Team.<X>` / `AFL.Character.<X>` | TeamId / CharacterId | RefreshSkin / possession | the robot |
| Robot skin / body finish | `AFL.Body.<X>` (`AFL.Finish.*`) | BodyId | body-color path | the robot body |
| Edge | `AFL.Edge.<X>` | EdgeId | RefreshSkinForPawn | the robot edge |
| Facemask | `AFL.Facemask.<X>` | FacemaskId | RefreshFacemaskForPawn | the robot face |
| Helmet | `AFL.Helmet.<X>` | HelmetId | *(staged -- axis exists, consumer pending)* | the robot head |
| **Weapon** | `AFL.Weapon.<W>` | WeaponId | RefreshWeaponForPawn | equips the gun (+ baked ORIGINAL color) |
| **Weapon skin** | `AFL.WeaponSkin.<Pattern>.<Color>` | WeaponSkinId | RefreshWeaponSkinForPawn | ANY equipped weapon (overrides original) |
| **Beam** | `AFL.Beam.<Color>` | BeamId | RefreshBeamColorForPawn | ANY equipped weapon (except special-guns) |
| **Pulse** | `AFL.Pulse.<Color>` | *PulseId (NOT YET)* | *(NOT BUILT)* | ANY equipped weapon (planned) |

**The WEAPON side is FOUR independent axes a player mixes:** WeaponId (which gun) + WeaponSkinId (which skin,
any gun) + BeamId (which beam, any gun) + [PulseId when built] -- plus the robot-side axes (body / edge /
facemask / helmet / identity). Own each separately; combine freely.

**RETIRED coupling (do NOT reintroduce):**
- `AFL.Weapon.<W>.<Color>` (skin baked as a per-weapon suffix) -> **RETIRED 2026-07-03.** A weapon skin is its
  own `AFL.WeaponSkin.<Pattern>.<Color>` item applied to any weapon. The per-weapon **base** row
  `AFL.Weapon.<W>` STAYS (the gun + its baked original color); skins OVERRIDE it.
- Beam color folded into the weapon skin -> **rejected at design.** The beam is `BeamId` (its own axis).

**FLAGGED (still coupled / missing -- fix when built, mirror this model):**
- **PULSE [QUEUED TASK -- tracked 2026-07-03, `BAG_MAN_LIVE_TRACKER`]:** currently COUPLED/MISSING -- no
  `PulseId` field in `FAFLCosmeticSelection`, no consumer, no namespace/gate/cheat. **REQUIREMENT (fold in
  when pulses are built, do NOT forget):** mirror the Beam axis EXACTLY -- add `PulseId` to
  `FAFLCosmeticSelection` + a `RefreshPulseColorForPawn` consumer + the `AFL.Pulse.<Color>` namespace +
  an entitlement-gate line + an `afl.Cosmetic.SetPulse` cheat. Until then pulses are UN-OWNABLE at runtime.
  (The 3rd coupling flagged this pass: beam-into-skin [fixed], skin-into-weapon [fixed], pulse-missing [queued].)
- **HELMET:** `HelmetId` exists + replicates but its consumer is staged (like Beam was) -- not drift, just
  unconsumed; wire when helmets are built.
- **Special guns** keep a LOCKED signature beam (`bLockedSignatureBeam`) -> BeamId does not override them
  (`IRONICS_WEAPONS_SSOT` §3.3; gravity gun = first member).

================================================================================
## THE HIGH-QUALITY-BASE PRINCIPLE (market separation)  [PROPOSED -- approve]
================================================================================
**Every weapon = 1 ORIGINAL HIGH-QUALITY UNIQUE MESH + its assigned original color = the weapon's
DISTINCT BASE IDENTITY.** This is the market separator: each weapon is a genuinely distinct, high-quality
product -- NOT a samey reskin. The addable colors/skins (the other 47) layer ON TOP of that distinct base.

- **Anchor product** = the base weapon (unique mesh + identity-fit original color, HIGH quality).
- **Customization layer** = the 47 addable colors (owned + equipped via the WeaponId/skin system).
- **Quality is the separator:** every anchor mesh meets the **"smoothly integrated" quality bar**
  (the mid/top-tier finish, NOT block-artifact fast conversions) -- the anchor carries the product's value.
- **Block-tolerance RETIRED for bases (2026-07-03 ruling):** supersedes the old base-tier block-artifact
  tolerance. Arclight's block-mag flagged for an uplift pass (don't fix now); Voltaic + Ioncaster meet the bar.
- Recorded also in `IRONICS_WEAPON_HANDOFF_CONTRACT.md` as a factory build rule.

================================================================================
## AXIS 3 -- THE 48 COLORS
================================================================================
All via the LOCKED master `M_AFL_WeaponSkin_NeonCamo` -- each = one MI (near-zero cost = the economy
multiplier). Every family is its OWN sub-spectrum (a range within the hue, not one flat color).

### 36 SPARK  [APPROVED + BUILT 2026-07-03]   (* = one of the legacy 8)
| Family | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| **RED** | CrimsonArc | NeonScarlet | InfernoRed | BloodPlasma | RubyGlow | RoseEmber |
| **BLUE** | ElectricBlue* | IceCyan* | CobaltDeep | SapphireNight | AzureSurge | GlacierFrost |
| **GREEN** | ToxicGreen* | GreenGold* | EmeraldPlasma | NeonLime | JadeVenom | MintFrost |
| **PURPLE** | ArcViolet* | PlasmaIndigo | DeepAmethyst | NeonOrchid | LavenderGlow | VoidViolet |
| **PINK** | CyanMagenta* | NeonMagenta | HotPink | RosePlasma | FuchsiaGlow | BubblegumFrost |
| **YELLOW** | Amber* | GoldPlasma | NeonYellow | AcidLemon | SolarFlare | PaleCitrine |

Legacy-8 map: EBlue+IceCyan->BLUE, ToxicGreen+GreenGold->GREEN, ArcViolet->PURPLE, CyanMagenta->PINK,
Amber->YELLOW (7 mapped). => 36 SPARK = 7 built + 29 new.

> ⚠️ **PRICING SUPERSEDED by `IRONICS_PRICING_SCARCITY_SSOT.md` §1.5 R2 (2026-07-08):** the SURGE-tier pricing here folds into the cheap-first ladder — legendary weapon-skins price at Standard/Premium (§1.5 R2), not the retired SURGE rung. Build status unchanged.

### 12 SURGE LEGENDARY -- 2 per family  [APPROVED + BUILT 2026-07-03]
The **prismatic/glitch premium** off the SAME locked master: `Enable_Glitch_Elements` ON, elevated
`Neon_Glow_Intensity`, prismatic/shifting hue. Tier = SURGE/LEGENDARY, scarce + premium-priced (pricing
SSOT). Generalizes the legacy single `GlitchLegend` prototype into 2 per family:

| Family | Legendary 1 (Prism) | Legendary 2 (Glitch) |
|---|---|---|
| **RED** | PrismCrimson | GlitchInferno |
| **BLUE** | PrismArc | GlitchVoid |
| **GREEN** | PrismVenom | GlitchAcid |
| **PURPLE** | PrismAmethyst | GlitchOrchid |
| **PINK** | PrismFuchsia | GlitchMagenta |
| **YELLOW** | PrismSolar | GlitchNova |

**= FULL palette 36 SPARK + 12 SURGE LEGENDARY = 48.** (Legacy `GlitchLegend` = the prototype the 12
are built from; its 3 existing SKUs remain / remap to each weapon's family legendary at build.)

================================================================================
## AXIS 2 -- SKIN PATTERNS  [MULTIPLIER -- confirm count later]
================================================================================
| Pattern | Master | Treatment | Status |
|---|---|---|---|
| NeonCamo | `M_AFL_WeaponSkin_NeonCamo` | liquid-glass plasma camo (LOCKED, AAA) | BUILT |
| Pattern 2..N | (to design) | neon-liquid, new designs | PENDING |

+1 pattern = +48 skins per weapon (the whole color grid again).

================================================================================
## AXIS 1 -- WEAPONS + their ORIGINAL color  [APPROVED]
================================================================================
Mechanic: each weapon's **ORIGINAL** = a REQUIRED identity-fit default (the color it ships in). The
other 47 = addable skin SKUs the player owns + equips via the WeaponId/skin system
([[project_43_weaponid_equip_consumer_authored]]). Weapons are NOT locked to one color.

DONE (3, PIE-PROVEN):
| Weapon | Base | Identity | ORIGINAL | Family | Built? |
|---|---|---|---|---|---|
| Voltaic | ShotgunBeam | voltage / electric | **ElectricBlue** | BLUE | YES |
| Ioncaster | V014 SMG | ion / plasma | **ArcViolet** | PURPLE | YES |
| Arclight | AK-110 | electric ARC (blue/violet dual) | **PlasmaIndigo** | PURPLE | new MI |

CANDIDATES (identity-fit originals, finalize at build):
PR-9 Raygun -> NeonLime | Sniper -> IceCyan | LMG -> InfernoRed | Assault -> AzureSurge |
SMG -> HotPink/NeonScarlet | Shotgun -> SolarFlare | GrenadeLauncher -> GoldPlasma | "toxic" -> ToxicGreen

================================================================================
## THE MATRIX -- multiplication + status (each cell = a SKU)
================================================================================
### Weapon skins = PATTERNS x 48 COLORS (per weapon)
Built so far = legacy 8 colors x 3 weapons (27 rows). PENDING: the other 40 colors x 3 weapons +
all future weapons/patterns.  Per weapon (1 pattern): 48 color skins + base.

### Beams = 48 COLORS  ->  `AFL.Beam.<Color>`   [0 built, 48 PENDING]
### Pulses = 48 COLORS ->  `AFL.Pulse.<Color>`  [0 built, 48 PENDING]

================================================================================
## TOTALS (built vs planned)
================================================================================
| Category | Built | Planned |
|---|---|---|
| Colors (MIs) | **49 BUILT** (8 legacy + 41 new) | 48 palette (+ GlitchLegend legacy) |
| Weapons (bases) | 3 (+2 legacy) | 3 + ~14 harvestable candidates |
| Weapon-skins (universal) | **49 UNIVERSAL PIE-PROVEN 2026-07-04** (`AFL.WeaponSkin.NeonCamo.<Color>` / WeaponSkinId -- applies to ANY weapon; cycle-proven) | + more patterns x 48 |
| Beams (color) | **49 PIE-PROVEN 2026-07-04** (`AFL.Beam.<Color>` / BeamId -- independent axis, auto-fire cycle-proven) | 48-color palette DONE |
| Pulses (color) | 0 -- needs the **`PulseId` axis** (mirror Beam, #21) | 48 (axis pending) |

Formulae: weapon-skins = **Weapons x Patterns x 48** ; beams = **48** ; pulses = **48**.

================================================================================
## BUILD ORDER (after approval of the 12 + principle)
================================================================================
1. Build all **48 color MIs** off the locked master (29 new SPARK + 12 SURGE legendary [glitch-on,
   elevated glow]). Verify each on disk.
2. Register the FULL 48-color skin grid per weapon (`AFL.Weapon.<W>.<Color>`, SPARK/SURGE-LEGENDARY);
   flip matrix cells built; sync the flat catalog doc. Register-as-created.
3. Beams + pulses (48 each) -- `AFL.Beam.<Color>` / `AFL.Pulse.<Color>` -- next stage.

================================================================================
## RULE -- REGISTER-AS-CREATED (definition of done)
================================================================================
Every new cell registers in BOTH the machine catalog AND the flat mirror as it is made; a cell is not
"done" until PIE-proven. Colors are cheap (an MI off the locked master); the multiplier is the point.
Cross-ref: `IRONICS_PRODUCT_SKU_CATALOG.md`, `IRONICS_WEAPON_HANDOFF_CONTRACT.md`, `afl-laser-beam-system`.

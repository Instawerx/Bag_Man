# IRONICS -- CATALOG MATRIX (the full product model)

**The COMPLETE product set as a matrix.** Every weapon, every skin (pattern x color), every beam
color, every pulse color = its own full SKU (`FAFLCatalogEntry` row, made properly + registered).
Build to it COMPLETELY -- nothing skipped; **register-as-created** fills each cell as it is made.

- Flat registry mirror (per-SKU rows + running total): `Docs/IRONICS_PRODUCT_SKU_CATALOG.md`.
- Machine registry (SSOT): `DA_AFL_CosmeticCatalog` (`/AFLBagMan/Cosmetics/`).
- This doc = the **definition of the full set** (the plan); the two above = what is built so far.
- Status snapshot: **2026-07-03** -- weapon PILOT COMPLETE (3 axes); COLOR SYSTEM locked to 36; beams/pulses NOT started.

> **STATUS: PROPOSED -- pending operator approval of (a) the 36-color palette + (b) the per-weapon
> ORIGINAL colors. DEFINE first; build the color MIs + register AFTER approval.**

================================================================================
## THE 5 AXES
================================================================================
1. **WEAPONS** -- the base models, harvested from Customizable Weapon Pack 5.4 (+ shipped beams). Each
   weapon has ONE assigned **ORIGINAL** color (its identity default) + can wear any of the other 35.
2. **SKIN PATTERNS** -- the neon-liquid treatment; NeonCamo (locked) + more (pending).
3. **COLORS** -- **6 families x 6 variations = 36 neon-liquid colors** (the locked multiplier).
4. **BEAMS** -- beam-color cosmetics (one SKU per color = 36).
5. **PULSES** -- pulse-color cosmetics (one SKU per color = 36).

Multiplication (each cell = one SKU):
- **Weapon skins** = PATTERNS x 36 COLORS (every pattern in every color), per weapon.
- **Beams** = 36 COLORS (each a beam SKU).  **Pulses** = 36 COLORS (each a pulse SKU).
- **Weapons** = the harvested bases (each = base + assigned original + addable colors).

================================================================================
## AXIS 3 -- THE 36 COLORS  [6 FAMILIES x 6 VARIATIONS -- APPROVE]
================================================================================
All 36 via the LOCKED master `M_AFL_WeaponSkin_NeonCamo` -- each = one MI with different color params
(near-zero cost each = the economy multiplier). Every family is its OWN sub-spectrum (a range within
the hue, not one flat color).  **[* = EXISTS/BUILT (7 of the legacy 8 map in); rest = new-to-build.]**

| Family | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| **RED** | CrimsonArc | NeonScarlet | InfernoRed | BloodPlasma | RubyGlow | RoseEmber |
| **BLUE** | ElectricBlue* | IceCyan* | CobaltDeep | SapphireNight | AzureSurge | GlacierFrost |
| **GREEN** | ToxicGreen* | GreenGold* | EmeraldPlasma | NeonLime | JadeVenom | MintFrost |
| **PURPLE** | ArcViolet* | PlasmaIndigo | DeepAmethyst | NeonOrchid | LavenderGlow | VoidViolet |
| **PINK** | CyanMagenta* | NeonMagenta | HotPink | RosePlasma | FuchsiaGlow | BubblegumFrost |
| **YELLOW** | Amber* | GoldPlasma | NeonYellow | AcidLemon | SolarFlare | PaleCitrine |

**Legacy-8 mapping:** ElectricBlue+IceCyan -> BLUE; ToxicGreen+GreenGold -> GREEN; ArcViolet -> PURPLE;
CyanMagenta -> PINK; Amber -> YELLOW  (= 7 mapped). **GlitchLegend** = NOT one of the 36 -- it is the
**SURGE legendary tier** (a prismatic/glitch premium variant, per-weapon, sits above the SPARK grid).

So: **36 SPARK colors (7 built + 29 new) + GlitchLegend (SURGE legendary).**
**APPROVE / ADJUST the 36 names + family assignments before any MI is built.**

================================================================================
## AXIS 2 -- SKIN PATTERNS  [MULTIPLIER -- CONFIRM]
================================================================================
| Pattern | Master material | Treatment | Status |
|---|---|---|---|
| NeonCamo | `M_AFL_WeaponSkin_NeonCamo` | liquid-glass plasma camo (LOCKED, AAA) | BUILT |
| Pattern 2..N | (to design) | neon-liquid, new designs | PENDING |

+1 pattern = +36 skins per weapon (the whole color grid again). **CONFIRM count + named concepts.**

================================================================================
## AXIS 1 -- WEAPONS + their ORIGINAL color  [APPROVE the originals]
================================================================================
**Mechanic:** each weapon's **ORIGINAL** = a REQUIRED identity-fit SKU field = the color it ships/
default-equips in. The other 35 colors are ADDABLE skin SKUs the player owns + equips onto the weapon
via the proven WeaponId/skin system ([[project_43_weaponid_equip_consumer_authored]]). A weapon is NOT
locked to one color -- original is the default; the rest are ownable + equippable.

DONE (3, PIE-PROVEN) -- proposed originals (2 already built as such):
| Weapon | Base | Identity | Proposed ORIGINAL | Family | Built? |
|---|---|---|---|---|---|
| Voltaic | ShotgunBeam | voltage / electric | **ElectricBlue** | BLUE | YES (its default) |
| Ioncaster | V014 SMG | ion / plasma | **ArcViolet** | PURPLE | YES (its default) |
| Arclight | AK-110 | electric ARC (blue/violet dual-neon) | **PlasmaIndigo** | PURPLE | new (blue-violet; alt=ElectricBlue) |

CANDIDATES -- proposed identity-fit originals (finalize at build, by name/look):
| Candidate | Identity read | Proposed ORIGINAL | Family |
|---|---|---|---|
| PR-9 Raygun | classic sci-fi ray | **NeonLime** (ray-green) | GREEN |
| Sniper (CM-2000/SCB-750) | cold precision | **IceCyan** | BLUE |
| LMG (N90) | heavy / aggressive | **InfernoRed** | RED |
| Assault (M4/ACWI) | clean rifle | **AzureSurge** | BLUE |
| SMG (GTM/PP9) | fast / punchy | **HotPink** or **NeonScarlet** | PINK/RED |
| Shotgun (M890/Remore) | brute | **SolarFlare** | YELLOW |
| GrenadeLauncher | AOE / pulse | **GoldPlasma** | YELLOW |
| ("toxic"-named future) | venom | **ToxicGreen** | GREEN |

**APPROVE / ADJUST each weapon's original.** (Rule of thumb: name/identity -> family -> a variation.)

================================================================================
## THE MATRIX -- multiplication + status (each cell = a SKU)
================================================================================
### Weapon skins = PATTERNS x 36 COLORS (per weapon)
Current: **1 pattern (NeonCamo) x 36 colors** per weapon. Built so far = the legacy 8 colors x 3 weapons
(27 rows) -- the OTHER 28 colors x 3 weapons + all future weapons/patterns are PENDING.
- Per weapon (1 pattern): 36 color skins + base.  Per new pattern (all weapons): + (weapons x 36).

### Beams = 36 COLORS  ->  `AFL.Beam.<Color>`   [0 built, 36 PENDING]
### Pulses = 36 COLORS ->  `AFL.Pulse.<Color>`  [0 built, 36 PENDING]

================================================================================
## TOTALS (built vs planned)
================================================================================
| Category | Built | Planned (this matrix) |
|---|---|---|
| Colors (MIs) | 8 legacy | **36** (7 map in + 29 new) + GlitchLegend legendary |
| Weapons (bases) | 3 (+2 legacy) | 3 + ~14 harvestable candidates |
| Weapon-skins (pattern x color) | 27 | Weapons x Patterns x 36 |
| Beams (color) | 0 | 36 |
| Pulses (color) | 0 | 36 |

Formulae (once locked): weapon-skins = **Weapons x Patterns x 36** ; beams = **36** ; pulses = **36**.

================================================================================
## RULE -- REGISTER-AS-CREATED (definition of done)
================================================================================
Every new cell registers in BOTH the machine catalog (`FAFLCatalogEntry` row) AND the flat mirror
(`IRONICS_PRODUCT_SKU_CATALOG.md`) as it is made -- and this matrix's status flips built. A cell is not
"done" until PIE-proven. Build to the matrix; do not skip cells. Colors are cheap (an MI off the locked
master); the multiplier is the point -- 36 colors x every surface (skins/beams/pulses) x every weapon.

Cross-ref: `IRONICS_PRODUCT_SKU_CATALOG.md` (flat mirror), `IRONICS_WEAPON_HANDOFF_CONTRACT.md`
(the 3 build axes), `afl-laser-beam-system` skill (beam/pulse system).

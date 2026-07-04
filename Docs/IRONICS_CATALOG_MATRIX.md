# IRONICS -- CATALOG MATRIX (the full product model)

**The COMPLETE product set as a matrix.** Every weapon, every skin (pattern x color), every beam
color, every pulse color = its own full SKU (`FAFLCatalogEntry` row, made properly + registered).
Build to it COMPLETELY -- nothing skipped; **register-as-created** fills each cell as it is made.

- Flat registry mirror (per-SKU rows + running total): `Docs/IRONICS_PRODUCT_SKU_CATALOG.md`.
- Machine registry (SSOT): `DA_AFL_CosmeticCatalog` (`/AFLBagMan/Cosmetics/`).
- This doc = the **definition of the full set** (the plan); the two above = what is built so far.
- Status snapshot: **2026-07-03** -- weapon PILOT COMPLETE (3 axes proven); beams/pulses NOT started.

> **STATUS OF THIS DOC: PROPOSED -- pending operator confirm of the two multipliers**
> (the BASE-COLOR set + the PATTERN list). Once confirmed, this is the build target.

================================================================================
## THE 5 AXES
================================================================================
1. **WEAPONS** -- the base models, harvested from the Customizable Weapon Pack 5.4 (+ shipped beams).
2. **SKIN PATTERNS** -- the neon-liquid treatment applied to a weapon; NeonCamo (locked) + more (pending).
3. **BASE COLORS** -- the color multiplier that runs through skins, beams, and pulses.
4. **BEAMS** -- beam-color cosmetics (one SKU per base color).
5. **PULSES** -- pulse-color cosmetics (one SKU per base color).

The multiplication (each cell = one SKU):
- **Weapon skins** = PATTERNS x BASE-COLORS (every pattern in every color), per weapon.
- **Beams** = BASE-COLORS (each color a beam SKU).
- **Pulses** = BASE-COLORS (each color a pulse SKU).
- **Weapons** = the harvested bases (each its own base + skin family).

================================================================================
## AXIS 3 -- BASE COLORS  [MULTIPLIER -- CONFIRM]
================================================================================
Proposed canonical set (confirmed present on all 3 factory weapons, each color x3):

| # | Color | Tier | Kind |
|---|---|---|---|
| 1 | ElectricBlue | SPARK | single (Voltaic default) |
| 2 | ArcViolet | SPARK | single (Ioncaster default) |
| 3 | ToxicGreen | SPARK | single |
| 4 | IceCyan | SPARK | single |
| 5 | Amber | SPARK | single |
| 6 | CyanMagenta | SPARK | combo |
| 7 | GreenGold | SPARK | combo |
| L | GlitchLegend | SURGE | legendary (premium variant, not a plain hue) |

= **7 SPARK colors + 1 SURGE legendary.** This SAME set is the multiplier for weapon-skins, beams,
and pulses. **NOTE:** distinct from the **11-neon Edge/Body** vocabulary (Crimson, Indigo, Lime,
Magenta, NeonBlue, NeonGreen, NeonPink, NeonPurple, NeonRed, NeonYellow, Solar) -- that set is the
character skin-color axis, NOT the weapon/beam/pulse set. **CONFIRM: is the weapon/beam/pulse color
set these 7+legendary, or should it align to the 11-neon set (or a merge)?**

================================================================================
## AXIS 2 -- SKIN PATTERNS  [MULTIPLIER -- CONFIRM]
================================================================================
| Pattern | Master material | Treatment | Status |
|---|---|---|---|
| NeonCamo | `M_AFL_WeaponSkin_NeonCamo` | liquid-glass plasma camo (LOCKED, AAA-approved) | BUILT |
| Pattern 2 | (to design) | neon-liquid, new design | PENDING |
| Pattern 3 | (to design) | neon-liquid, new design | PENDING |
| ...       | (to design) | neon-liquid | PENDING |

**CONFIRM: how many additional patterns, and any named concepts?** (Each new pattern multiplies the
skin count by the base-color set: +1 pattern = +8 skins per weapon.)

================================================================================
## AXIS 1 -- WEAPONS (harvested bases)
================================================================================
DONE (3, all PIE-PROVEN -- the pilot; all 3 factory axes):

| Weapon | Base mesh | Category | Factory axis | Default color |
|---|---|---|---|---|
| Arclight | AK-110 | Assault | ballistic->beam conversion (Blender) | (multi) |
| Voltaic | ShotgunBeam (shipped) | Shotgun | shipped-beam mesh-mod (Blender) | ElectricBlue |
| Ioncaster | V014 | SMG | attachment-recombine (UE, zero-Blender) | ArcViolet |

LEGACY (2, pre-existing stock beam weapons): `AFL.Weapon.Pistol.NeonGreen`, `AFL.Weapon.ShotgunBeam.NeonBlue`.

CANDIDATES (harvestable from the 5.4 pack -- all convertible to the IRONICS beam identity; the beam
look comes from the skin + beam, so any base qualifies -- listed by silhouette variety):

| Candidate | Base | Category | Suggested axis | IRONICS fit |
|---|---|---|---|---|
| (Assault) | ACWI / ACWI_Var / M4 | Assault | conversion or recombine | strong (rifle beams) |
| (SMG) | GTM / PP9 | SMG | recombine | strong (compact) |
| (LMG) | N90 | LMG | recombine / mesh-mod | strong (heavy caster) |
| (Sniper) | CM-2000 / SCB-750 | Sniper | recombine / mesh-mod | strong (precision, 117cm -- scale-conform) |
| (Shotgun) | M890 / Remore | Shotgun | mesh-mod | strong |
| (Pistol) | DE-42 / Judge-45 / NFP-16 | Pistol | recombine (1H, no GripPoint_L) | good |
| **Raygun** | **PR-9_Raygun** | Pistol | mesh-mod (energy-native!) | **best -- already a raygun** |
| (Special) | GrenadeLauncher | Other | mesh-mod | pulse/AOE weapon candidate |

**CONFIRM: which candidates to build, and in what order?** (Recommend the PR-9 Raygun next -- it is
energy-native, the least conversion work.)

================================================================================
## THE MATRIX -- multiplication + status (each cell = a SKU)
================================================================================
### Weapon skins = PATTERNS x BASE-COLORS (per weapon)
Current: **1 pattern (NeonCamo) x 8 colors + base = 9 SKUs per weapon.**
- Arclight: 9 BUILT.  Voltaic: 9 BUILT.  Ioncaster: 9 BUILT.  (= 27 weapon-skin SKUs live.)
- Per new weapon: +9 (1 pattern). Per new pattern (all weapons): + (weapons x 8).

### Beams = BASE-COLORS   ->  `AFL.Beam.<Color>`   [0 built, PENDING]
| ElectricBlue | ArcViolet | ToxicGreen | IceCyan | Amber | CyanMagenta | GreenGold | GlitchLegend |
| PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |

### Pulses = BASE-COLORS  ->  `AFL.Pulse.<Color>`  [0 built, PENDING]
| ElectricBlue | ArcViolet | ToxicGreen | IceCyan | Amber | CyanMagenta | GreenGold | GlitchLegend |
| PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |

================================================================================
## TOTALS (built vs planned)
================================================================================
| Category | Built | Planned (this matrix) |
|---|---|---|
| Weapons (bases) | 3 (+2 legacy) | 3 + candidates (~14 harvestable) |
| Weapon-skins (pattern x color) | 27 (3 weapons x 1 pattern x 8 + 3 base) | weapons x patterns x 8 |
| Beams (color) | 0 | 8 (per color) |
| Pulses (color) | 0 | 8 (per color) |

Formula once the multipliers are locked:
- weapon-skin SKUs = **Weapons x Patterns x (BaseColors+base)**
- beam SKUs = **BaseColors** ; pulse SKUs = **BaseColors**

================================================================================
## RULE -- REGISTER-AS-CREATED (definition of done)
================================================================================
Every new cell registers in BOTH the machine catalog (`FAFLCatalogEntry` row) AND the flat mirror
(`IRONICS_PRODUCT_SKU_CATALOG.md`) as it is made -- and this matrix's status flips built. A cell is
not "done" until PIE-proven (the =PIE doctrine). Build to the matrix; do not skip cells.

Cross-ref: `IRONICS_PRODUCT_SKU_CATALOG.md` (flat mirror), `IRONICS_WEAPON_HANDOFF_CONTRACT.md`
(the 3 build axes + socket/skin contract), `afl-laser-beam-system` skill (beam/pulse system).

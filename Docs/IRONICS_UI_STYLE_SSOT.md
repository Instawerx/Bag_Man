# IRONICS - GAME UI STYLE SSOT

**Status:** DRAFT for operator approval - 2026-06-28 - authored under operator ruling 3 (author the
style doc before locking the visual language). The single source every UI surface inherits from:
in-match HUD AND front-end. No UI surface defines its own palette/type/chrome - it pulls from here.

**Authority / sources (this doc only PROMOTES + consolidates existing decisions; it invents nothing
beyond the type ramp + semantic mapping, which are flagged for approval):**
- Palette: `Tools/IRONICS_STORE_COLOR_SPEC.md` (operator-approved 2026-06-08; the neon RGB registry +
  card/glass color-role rules).
- Panel system: `Tools/skills/expert-game-designer/SKILL.md` (the Apple-Glass design system + tokens).
- Art direction: `Docs/BAG_MAN_MASTER_BUILD_v2.0.md:401-402` (emissive-driven, low-ambient, neon at
  sightlines) + `:1981` (Apple-Glass-inspired UI is the assigned direction).
- Brand/title: `Docs/BAG_MAN_MASTER_BUILD_v2.0.md:139-145` (title rules) RECONCILED by operator ruling 2.
- Vocab/economy: `Docs/IRONICS_ECONOMY_SPEC.md:14-15,36-62` + `Docs/IRONICS_PRICING_SCARCITY_SSOT.md:36-42`.
- Readability law: `Docs/maps/Arena_01_DESIGN.md` sec10 (HUD/FX hues off the beam palette) +
  `Docs/IRONICS_PLAYER_FLOW.md` sec7-C4 (team-read = outline/rim + nameplate).
- Full color registry: `Docs/AFL_COLOR_REGISTRY_MIGRATION_PLAN.md:109-131` (11-color in-game registry).

---

## 1. BRAND (operator ruling 2 - LAW)

- **IRONICS is the player-facing name EVERYWHERE** - the game title AND the in-world brand (bank,
  house identity, store). Every menu, HUD string, splash, and store surface reads IRONICS.
- **BAG MAN is legacy/internal only - NEVER shown player-facing.** (Supersedes the BAG-MAN-as-title
  line in `BAG_MAN_MASTER_BUILD_v2.0.md:143`, per operator ruling 2.)
- **AFL is the code prefix only - NEVER shown player-facing** (`BAG_MAN_MASTER_BUILD_v2.0.md:141,145`).
- **IRONICS is also the default/free robot identity** ("the house robot, the ONLY free identity",
  `IRONICS_PLAYER_FLOW.md` sec1) - so "IRONICS" is both the game brand and the name a fresh player wears.
- **Wordmark/lockup: BOLT (voltage monogram - the "I" is a lightning bolt) + a standalone diamond-bolt icon.**
  LOCKED 2026-06-30; electric-blue-core / Arc-Violet-rim treatment (sec2.1 blend rule). Dual-purpose: full lockup
  (menu/branding) + standalone icon (match tiles / app icon / loading / HUD watermark, core-dominant at small
  sizes per the size-gate). Final 2D UI texture + the 3D hero emblem (Tripo->Blender->material) integrate as
  produced. Source: `Docs/IRONICS_BOLT_Assets.html`.

## 2. PALETTE

Canonical source = `IRONICS_STORE_COLOR_SPEC.md` (engine-native LINEAR RGB 0-1, the values UMG +
materials author against). sRGB hex below is the gamma-converted design-tool approximation - the
LINEAR values are authoritative; hex is for mockups only.

### 2.1 House colors (the IRONICS identity - the only brand chrome the HUD uses)
The IRONICS house is a **4-color ROLE palette** - each color has ONE job. Locked by operator 2026-06-30
(house color cyan -> electric neon blue, sampled from the operator IRONICS logo `T_IRONICS_Color_Logo_BC`).
| Token | Role | Linear RGB (canonical) | sRGB hex | Job |
|---|---|---|---|---|
| `UI.House.Electric` (IRONICS blue) | PRIMARY | (0.013, 0.102, 1.00) | #1E5AFF | Glass fill/tint, active+selected state, the house LEAD (replaces Lyra cyan) |
| `UI.House.Violet` (Arc Violet) | ACCENT | (0.40, 0.09, 0.93) | #A855F7 | Neon-pipe edge-glow, focus/hover, rim + logo-rim - NEVER core/fill/readable-text |
| `UI.House.Black` (depth) | DEPTH | ~(0.002, 0.003, 0.006) | #05080F | Panel-depth backing, contrast behind text, the dark base the neon reads against |
| `UI.House.White` (text) | TEXT | (1.00, 1.00, 1.00) | #FFFFFF | Readable labels, key specular highlights on glass edges, max-contrast text |
| `UI.House.Cyan` (DEPRECATED) | -- | (0.00, 0.94, 1.00) | #00F8FF | DEPRECATED-not-deleted: the Lyra-era primary; kept for lineage, NOT the new lead |
| `UI.House.Blue` (NeonBlue) | tertiary | (0.00, 0.42, 1.00) | #00ADFF | tertiary fills, inactive states |

The IRONICS house reads **electric-blue (lead) + arc-violet (accent) + black (depth) + white (text)**
(`IRONICS_STORE_COLOR_SPEC.md:18-29` + house-color + Arc-Violet locked by operator 2026-06-30). That is the
menu/HUD chrome identity. **ROLE LOGIC:** blue LEADS (fill/active) - violet ACCENTS (edge/focus) - black gives
DEPTH/contrast - white carries TEXT/highlights; one primary job each, not all four everywhere.

**THE BLEND RULE (LAW):** electric-blue CORE + arc-violet RIM = the electric-arc accent. Electric-blue LEADS
(core / fill / active state); arc-violet SUPPORTS (rim / edge-glow / halo / focus / hover) and NEVER touches
the readable core, fill, or text. The rim is **size-gated**: ON at >=64px (lockups, hub emblem, large tiles);
the mark goes **core-dominant at <=32px** (HUD watermark, 16px icon) for crisp legibility. Arc-Violet is
deliberately bluer (hue ~271) than the identity-axis NeonPurple `#CB00FF` (sec2.2), so it blends with the
electric-blue core rather than clashing; the two purples never share a role (Arc-Violet = brand chrome;
NeonPurple = team/store identity). (Cyan #00F8FF was the pre-2026-06-30 core; DEPRECATED-not-deleted.)

### 2.2 The neon registry (the broader palette - identity/team/store, NOT default HUD chrome)
From `IRONICS_STORE_COLOR_SPEC.md:18-29` + `AFL_COLOR_REGISTRY_MIGRATION_PLAN.md:109-131`:
NeonBlue, NeonGreen (0,1,0.30 / #00FF95), NeonPink (1,0.10,0.60 / #FF59CB), NeonPurple
(0.60,0,1.00 / #CB00FF), NeonRed (1,0.05,0.05 / #FF3F3F), NeonYellow, Crimson, Indigo, Solar,
Magenta, Lime (#CCFF00). These drive PLAYER/TEAM identity + store cards. The five BRAND-WEAPON /
BEAM hues are **MAGENTA, INDIGO, SOLAR, CRIMSON, LIME** (`IRONICS_WEAPONS_SSOT.md:179-184`).

### 2.3 Color-role rule (the glass-panel convention - reuse verbatim, `IRONICS_STORE_COLOR_SPEC.md:44-49`)
- Frame / border glow -> the surface's **Primary** color.
- Edge accent / inner detail / value chip -> **Accent** color.
- Background glass -> **Primary x ~0.12** (a faint tint - "not flat dark").
- Rarity/state badge -> a **separate** color axis (never borrows the identity slot - the "stray-yellow"
  guardrail).

### 2.4 Readability LAW (combat-overlay surfaces)
- **HUD/marker hues must stay OFF the five beam hues** (MAGENTA/INDIGO/SOLAR/CRIMSON/LIME) so the HUD
  reads against live fire - the "no green-on-green" rule extended to UI (`Arena_01_DESIGN.md` sec10).
  -> World-space markers, reticle, and combat overlays use **house cyan/white + glass**, never a beam hue.
  -> Top-of-screen chrome (round/score header, menus) MAY carry team-identity colors (it does not
     overlay the beam directly), but solid combat-overlay elements may not.
- **Team identification is outline/rim + nameplate, NOT HUD body-color or post-process**
  (`IRONICS_PLAYER_FLOW.md` sec7-C4; material EdgeGlow, not post-process). The HUD shows team via the
  score header + nameplates; it does not recolor the world.
- **Arc-Violet (sec2.1) is brand CHROME, not a combat-overlay marker hue.** It rims menu / front-end /
  header glass (edge-glow, focus, hover, the lockup). On solid combat-overlay surfaces the cyan/white/glass
  rule above still holds - Arc-Violet stays a thin edge-glow there, never a fill, so it clears the INDIGO beam hue.

### 2.5 Semantic colors (DERIVED recommendation - flagged for approval)
Chosen to stay readable over combat and clear of the beam hues. Solid accents use house chrome;
state washes use the Apple-Glass tints (sec3, low-alpha, so they never read as a solid beam color):
| Semantic | Color | Basis |
|---|---|---|
| Info / brand / neutral-active | House Cyan #00F8FF | sec2.1 |
| Success / extract / bank / own-team-good | House Cyan + Success tint (sec3) | cyan is the house "go" |
| Danger / at-risk / damage | `Glass.Tint.Danger` wash (rgba 255,80,80,0.15) over neutral | skill token; subtle, not solid Crimson |
| Warning / contested / timer-low | Amber #FFB300 (NeonRed accent) | `IRONICS_STORE_COLOR_SPEC.md` warm-pair |
| Enemy / hostile marker | neutral red rim, NOT a beam hue | readability law sec2.4 |

### 2.6 Palette migration discipline (the cyan -> electric-blue sweep - cross-inheritance LAW)
Migrating Lyra-era cyan to the electric-blue lead: fix at the SAFE level, NEVER the masters.
- **The `M_UI_Base_*` masters are CROSS-INHERITANCE HAZARDS** - they feed BOTH the menu chrome AND the
  gameplay/team-blue HUD. Recoloring a master re-tints team-read + combat overlays (breaks sec2.4). Never
  edit a master for a chrome sweep.
- **Recolor at the intermediate instances** (`MI_UI_*Button_Base`, `MI_UI_*` menu instances) - the safe
  leverage that cascades to the menu without touching gameplay. (The `C_UI_*` curves are NOT the menu root.)
- **Sequence:** map the cascade (parents / curves / cross-inheritance) FIRST, then sweep in batches at the
  instance level. Menu batches go first; the **gameplay/team HUD governed pass is DONE**
  (Batch 3, commit 17c8a5f5 -- cyan-sweep COMPLETE): Group A non-team chrome (Ammo/Dash/RespawnTimer/Accolade/
  WeaponCard/ElimFeed-glows/HealthBar-fill) recolored -> electric-blue at the INSTANCE level; **team-read cyan
  KEPT by governance** (blue-team ControlRing marker, TeamScore own-vs-enemy, healing green) per the sec2.4
  readability law + operator decision. No forward-facing cyan remains.
- Widget tints don't cover baked-in material color: a cyan-looking button is usually the MI (`_Glow`/`_Ring`),
  not the brush tint - trace to the material source, don't tint blind. Proven by the surface #1/#2 sweep
  (40 `MI_UI_*` recolored at the instance level, masters untouched; commit 512e9ab2).

## 3. PANEL SYSTEM (Apple-Glass - `expert-game-designer/SKILL.md:50-96`)

The panel LANGUAGE for every surface = frosted glass over the cyber world. Neutrals from the skill;
brand accent from sec2.

**Glass tokens (neutral chrome):**
```
Glass.Bg.Primary    rgba(255,255,255, 0.12)   main panel fill
Glass.Bg.Secondary  rgba(255,255,255, 0.08)   nested panels
Glass.Bg.Tertiary   rgba(255,255,255, 0.05)   dividers
Glass.Border        rgba(255,255,255, 0.20)   panel edge highlight  (override -> House Cyan when active)
Glass.Blur          20-40px gaussian (UMG BackgroundBlur)
Glass.Shadow        rgba(0,0,0, 0.30)  0 8px 32px
Glass.Tint.Primary  rgba(100,180,255, 0.15)   cool accent wash (re-tint to House Cyan/Blue)
Glass.Tint.Danger   rgba(255,80,80, 0.15)
Glass.Tint.Success  rgba(80,255,140, 0.12)
Text.Primary 255,255,255,1.0 | Secondary .7 | Tertiary .45 | Accent -> House Cyan
```
**Geometry:** panel corner radius 16-24px, button 12px, input 8px. Depth over flatness (Z-layered
panels + shadow), translucency (blur the world behind), **luminous restraint** (glow only where it
MEANS something - a threshold, a confirm, an objective). Motion is meaning (panels breathe in/out;
transitions reveal depth).

**World grounding:** the panel glass sits over an **emissive-driven, low-ambient, neon-at-sightlines**
scene (`BAG_MAN_MASTER_BUILD_v2.0.md:401-402`) - so glass reads bright against dark; do NOT flood the
HUD with fill light. The aesthetic through-line shared with weapons = **translucent glass + emissive
neon + energy**, never opaque/metallic (`IRONICS_WEAPONS_SSOT.md:131-135`).

## 4. TYPOGRAPHY (DERIVED ramp - flagged for approval; no type spec existed)

Grounded in the skill's SF-inspired direction (`SKILL.md:62` light body / semibold actions) + the
established all-caps terse energy voice (the existing HUD banners "EXTRACTION WINDOW OPEN") + the
cyber precedent (Chakra Petch / JetBrains Mono, the v1.1 tracker faces). The ramp (confirm font
licensing before lockdown):
| Role | Face (recommended) | Weight / case | Use |
|---|---|---|---|
| Display | Chakra Petch (or licensed techno-sans equiv) | Bold, ALL-CAPS | titles, round#, banners, the IRONICS lockup |
| Body / label | SF-Pro-inspired clean sans | Light/Regular, sentence case | menu body, descriptions, settings |
| Numeric / tabular | JetBrains Mono (or licensed mono) | Medium | Watts/Volts counters, timers, scores, stats (tabular-aligned) |

**Voice:** ALL-CAPS terse for HUD callouts (match the existing banners); sentence case for menu body.
Energy/electrical motif throughout (it is the brand voice). Numbers are always integer (sec5).

## 5. COPY / VOCAB (LAW - `IRONICS_ECONOMY_SPEC.md`)

- **Currency, exact spelling:** **Watts** (soft, earned by play), **Volts** (hard, premium), **Bolts**
  (P2P, legal-gated) - `IRONICS_ECONOMY_SPEC.md:36-40`. Peg **10 Watts = 1 Volt** (`:14-15`).
- **NEVER show USD in-match.** **NO CASH-OUT, EVER** - UI must never imply value converts to real
  money (`IRONICS_PLAYER_FLOW.md` sec0, sec8.1). The in-match HUD shows **Watts**, integer only.
- **Store tier names (player-facing, locked):** SPARK / SURGE / ARC / THUNDER BOLT
  (`IRONICS_ECONOMY_SPEC.md:57-62`); base rung FLICKER.
- **Rarity ladder (locked):** Static -> Charge -> Surge -> Bolt -> Tempest -> Singularity
  (`IRONICS_PRICING_SCARCITY_SSOT.md:36-42`).
- **Mental model the HUD communicates** (`IRONICS_LOOT_CARRY_MODEL.md:169`): "Wallet = banked/safe;
  cache = carried/at-risk; extract = the bridge." The at-risk rail shows { cache Watts + part-count +
  part-value }, all replicated ints.
- **AFL / BAG MAN never appear in any string.**

## 6. COMPONENT TOKENS (the reusable primitives - one source for every surface)

Each maps to an EXISTING widget to extend (sec ref) or is a NEW primitive. All inherit secs 2-5.
| Token | Definition | Backing widget |
|---|---|---|
| `UIPanel.Glass` | the base frosted panel (sec3 tokens, 16-24px radius, House-Cyan active border) | base for all |
| `UIMeter.Rail` | horizontal/radial fill bar; fill = semantic color; threshold tick; pulse at threshold | EXTEND `UAFLW_EnergyMeter`, `UAFLCarriedValueWidget` |
| `UIChannel.Bar` | timed channel fill 0->N s; green-on-confirm / red-on-interrupt | EXTEND `UAFLW_ExtractChannelBar` |
| `UIToast.Banner` | center banner, hold-then-fade, glass; ALL-CAPS display type | EXTEND `UAFLW_ExtractionAnnounce`; NEW round-result toast |
| `UIScore.Header` | round# + best-of pips + team scores + timer; team-color-tinted, mono numerics | NEW (binds replicated RoundManager data) |
| `UIMarker.World` | world-space objective/zone pin + screen-edge indicator + distance; house-cyan, OFF beam hues | NEW (map-specific) |
| `UIButton.Glass` | glass button, 12px radius, House-Cyan hover glow | NEW (front-end) |
| `UIChip.Value` | integer Watts/Volts chip, mono, value-accent | EXTEND CarriedValue; front-end wallet |
| `UIBadge.Rarity` | rarity label on the SEPARATE color axis (sec2.3) | NEW (store/inventory) |

**Arc-Violet accent variant (sec2.1 blend rule) - applies to EVERY token:** the focus / hover / active
STATE of any glass token adds an Arc-Violet edge-glow (`UIPanel.Glass` focus = House-Cyan border + Arc-Violet
outer rim; `UIButton.Glass` hover = cyan fill-glow + Arc-Violet rim). The accent is a STATE layer
(focus/hover/selected), NEVER the resting fill. This keeps the electric-arc read consistent across HUD + front-end.

**Pipeline for every surface (expert-game-designer):** SVG/HTML mock -> design spec (grid/spacing/
tokens/motion) -> AIK/UMG authoring -> CommonUI/LyraHUDLayout wire (C++ base owns bindings, WBP child
owns layout - the proven AFL split). No surface bypasses this SSOT.

---

## OPEN ITEMS (need operator sign-off before lockdown)
1. **Type ramp (sec4)** - confirm faces + licensing (Chakra Petch / SF-equiv / JetBrains Mono are
   recommendations from precedent, not blessed).
2. **Semantic mapping (sec2.5)** - confirm danger/warning treatment (kept subtle to clear the beam hues).
3. **IRONICS wordmark/lockup** - RESOLVED 2026-06-30: BOLT locked (sec1), cyan-core/Arc-Violet-rim; final art
   (UI texture + 3D hero) in production. **Arc-Violet #A855F7 secondary locked (sec2.1, sec2.4, sec6).**
4. **sRGB vs linear** - linear (store spec) is canonical; confirm the gamma-converted hex set above for
   design tools, or supply a measured hex set.

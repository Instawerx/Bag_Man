# IRONICS UI WASH QUEUE

Queued IRONICS front-end retheme work. **Append-only, never-delete.** The operator picks
execution order; nothing here is executed until approved. Scope pass = map what/where; keep
it findable so execution later is clean. Companion to `Docs/IRONICS_UI_STYLE_SSOT.md` (the
palette + the cyan-sweep cross-inheritance LAW, sec2.6).

## Shipped (context)
- Surface #1 hub + #2 arena-picker retheme (commit 512e9ab2); SSOT 4-color palette (635af36f);
  stock-Lyra experiences hidden -> picker = IRONICS roster only (21a40fe9); Surface #3 settings
  (7edd2cd8). Masters `M_UI_Base_*` untouched throughout; Batch-3 gameplay/team HUD HELD.

---

## QUEUE ITEM 1: LOADING / TRANSITION SCREENS (scoped 2026-06-30, NOT executed)

### Surfaces + where-set + bucket
| Surface | Path | Where set | Bucket | State |
|---|---|---|---|---|
| `W_LoadingScreen_Host` | `/Game/UI/Foundation/LoadingScreen/` | **CONFIG** (see below) | RETHEME (chrome) | ACTIVE screen; wraps the Default content + an experience-specific NamedSlot |
| `W_LoadingScreen_DefaultContent` | `/Game/UI/Foundation/LoadingScreen/` | widget (embedded in Host) | RETHEME | thin wrapper; only holds the logo widget |
| `W_LyraLogo_LoadingScreen` | `/Game/UI/Foundation/LoadingScreen/` | widget (embedded in DefaultContent) | **REPLACE** | the LYRA LOGO: `Constellation`+`Logo`+`LogoGlow`+`LogoSoftGlow` Images |
| `MI_UI_LoadingSymbol` / `_Sine` / `_Sine_Modal` / `_Sine_DefaultLoadingScreen` | `/Game/UI/Menu/Art/` | material | RETHEME | **ALREADY electric-blue** (Batch 2: `(0.01,0.10,1.0)`) -- DONE |
| `MI_UI_DefaultLoadingScreen` | `/Game/UI/Menu/Art/` | material | RETHEME | dark-blue/black backing + muted halftone -- reads as DEPTH, mostly OK |
| `MI_UI_LoadingTextColor` / `MI_UI_LoadingSoftShadow` | `/Game/UI/Menu/Art/` | material | RETHEME | clean (Batch 2) |
| `MI_UI_Logo_DefaultLoadingScreen` / `_Glow_` / `_SoftGlow_` | `/Game/UI/Menu/Art/` | material | **REPLACE** | letters recolored electric-blue (Batch 2) but the SHAPE is still Lyra |
| `M_UI_Base_LoadingSymbol` | `/Game/UI/Menu/Art/` | MASTER | (retheme at instances, NOT master) | clean |
| `W_LoadingScreen_ControlPoint` / `_TDM` | `/ShooterMaps/System/Playlists/` | per-map (stock) | **SKIP** | belong to hidden stock experiences -- players never see them |
| `W_LoadingScreenReasonDebugText` | `/Game/UI/Foundation/LoadingScreen/` | widget | **SKIP** | debug text, not player-facing |

### WHERE IT IS SET (the key finding -- loading is NOT a uniform widget edit)
- **Config-set widget:** `Config/DefaultGame.ini:91` -> `[/Script/CommonLoadingScreen.CommonLoadingScreenSettings]`
  `LoadingScreenWidget=/Game/UI/Foundation/LoadingScreen/W_LoadingScreen_Host.W_LoadingScreen_Host_C`.
- **Display = C++/early-load:** the `CommonLoadingScreen` PLUGIN shows the widget BEFORE the normal UI is live.
  BUT we do NOT touch C++ -- we retheme the WIDGET the config points at. Change path = **widget-recolor**, same
  as the menus. Config-swap is only needed if we replace the whole widget class (we won't -- we retheme in place).
- **Caveat:** because it shows early, VERIFY during a real level load / travel (watch an actual map load), not
  just a widget preview.

### Already-swept vs remaining
- **Swept (Batch 2, electric-blue/clean):** the loading SYMBOL + all Sine variants, text color, soft shadow,
  and the logo material's letter color. The loading screen is already mostly on-palette.
- **Remaining (the real work):** the **LYRA LOGO** -> IRONICS. Everything else is polish.

### Buckets
- **RETHEME-TO-PALETTE (mostly DONE):** symbol electric-blue (done), background dark-blue depth (OK). Optional
  polish: a violet edge-pipe on the loading container `Border_0`, and an electric-blue lift on the halftone.
- **REPLACE-WITH-IRONICS:** `W_LyraLogo_LoadingScreen` (the 4 logo Images) + `MI_UI_Logo_*_DefaultLoadingScreen`
  (the Lyra-shaped logo material) -> the IRONICS mark (`T_IRONICS_Logo_Transparent`, already imported).

### Flags (tricky / non-widget)
- **CONFIG:** the active screen is set in `DefaultGame.ini` -- retheme in place needs no config change; only a
  full widget-class replacement would.
- **C++/EARLY-LOAD:** `CommonLoadingScreen` plugin drives the display -- do NOT touch it; retheme the widget.
  Test during real travel.
- **SHARED/Foundation:** `W_LyraLogo_LoadingScreen` lives in `/Game/UI/Foundation/` -- **check inbound
  references before replacing** (it may be used by more than the loading screen; if so, the swap cascades).
- **MASTER:** `M_UI_Base_LoadingSymbol` -- retheme at the `MI_UI_LoadingSymbol*` instances, never the master.

### Proposed execution order (easy-first)
1. **Chrome polish** (easy widget-recolor): confirm symbol/background electric-blue; optional violet pipe on the
   loading `Border_0`. Low risk.
2. **Lyra-logo replace** (the main item): first `git`-grep / reference-check `W_LyraLogo_LoadingScreen`; then
   swap its logo Images (or the `MI_UI_Logo_*_DefaultLoadingScreen` texture) to `T_IRONICS_Logo_Transparent`.
3. **Verify on a real level load** (the screen shows early) -- watch an actual map travel, confirm IRONICS logo +
   electric-blue symbol, no Lyra mark. Re-check the log.

---

## QUEUE ITEM 2: OTHER WASH (parallel, non-blocking)
- **Real IRONICS map-tile thumbnails** -- NanoWatt/Infineon landscape art -> the DAs' `tile_icon` (interim = the emblem).
- **3D hero emblem** -- Tripo/Blender from the logo.
- **Batch-3 gameplay/team palette** -- the governed HUD pass (Teams/HealthBar/Ammo/ElimFeed/etc.), readability-law
  bound (SSOT sec2.4). HELD -- NOT a blind chrome recolor.
- **Post-match results + store/identity** -- remaining Tier surfaces.

---

## QUEUE ITEM 3: BATCH 3 -- gameplay/team HUD cyan (GOVERNED pass, scoped 2026-06-30)

The LAST held cyan. 26 cyan materials, ALL in `/Game/UI/Hud/Art`, ALL **instances** -- the 6 masters
(`M_UI_Base_AmmoCounter/Control/HealthBar/ReticleBuilder/SimpleIcon/WeaponCard`) are clean templates that
define the param SLOTS; the instances hold the cyan VALUES. **Cross-inheritance is real but the fix is
instance-level:** the AmmoCounter master's `Ammo_Full_RGBA`/`Ammo_Empty_RGBA` slots are reused by Ammo, Dash,
RespawnTimer AND TeamScore -- so a master recolor would blanket all four; recoloring each INSTANCE is safe.
NEVER recolor the masters.

### GROUP A -- NON-TEAM CHROME (recolor cyan -> electric-blue #1E5AFF; ~22 instances, safe)
No gameplay/team meaning -- decorative HUD chrome, self-state meters (your ammo/dash/respawn/health), reward pops.
- **Accolade (3):** `MI_UI_Accolade_DoubleElimination` [IconRGB_A], `MI_UI_Accolade_Shadow` [Glow_Simple_Active], `MI_UI_AccoladeBorder` [Outline_RGB]
- **Ammo (4):** `MI_UI_AmmoCounter_Pistol_Glow` / `_Rifle` / `_Rifle_Glow` / `_Shotgun_Glow` [Ammo_Empty/Full_RGBA] -- self ammo, not team
- **Dash (2):** `MI_UI_Dash_BarFill` / `_BarGlow` [Active_RGBA + reused Ammo params] -- self ability meter
- **RespawnTimer (4):** `MI_UI_RespawnTimer` / `_Glow` / `_Text` / `_SoftGlow` -- self respawn. NOTE `_SoftGlow` overrides a
  slot NAMED `TeamMarker_Red_High/Low_RGB` with CYAN (=0.23,0.91,1.0) -- a DECORATIVE reuse, NOT a real red team marker.
- **WeaponCard (2):** `MI_UI_WeaponCard_Glow` [Glow_Simple_Active/Inactive], `MI_UI_Base_WeaponCard_TouchButton` [Active_Fill_Inner/Outer]
- **ElimFeed glows (4):** `MI_UI_ElimFeed_NameColor` / `_NameGlow` / `_Shadow` [Glow_Simple_Active -- recency glow, state not team], `MI_UI_Base_Icon_ElimFeed` [IconRGB]
- **ControlRing letter (1):** `MI_UI_ControlRing_LetterColor` [Hover_RGBA] -- hover chrome
- **HealthBar HEALTH cyan (2):** `MI_UI_HealthBar_Fill` [Bar_Health_Fill/Highlight=cyan], `MI_UI_HealthBar_Glow` [Glow_Health=cyan]
  -- self-health, house-colored. **KEEP the GREEN healing params (see group B).**

### GROUP B -- TEAM-MEANING / READABILITY (GOVERNED -- do NOT blind-recolor)
- **`MI_UI_ControlRing_Base` -- the BLUE-TEAM control-point marker.** `TeamMarker_Blue_High_RGB=(0.46,1.0,0.88)` (cyan),
  `TeamMarker_Blue_Low_RGB=(0.02,0.57,1.0)` (blue); the Red markers are correctly red `(1.0,0.21,0.15)`. THIS carries
  who-owns-the-point. Recoloring the blue marker changes how BLUE TEAM reads. **OPERATOR DECISION (team-color question).**
- **Healing GREEN = readability (KEEP, do not touch):** `MI_UI_HealthBar_Fill.Bar_Healing_Fill/Highlight=(0.44,1.0,0.30)`
  green, `MI_UI_HealthBar_Glow.Glow_Healing=(0.10,1.0,0.0)` green -- green means "being healed." Recolor ONLY the cyan
  health params in these two, LEAVE the green healing params.
- **TeamScore (3, FLAG):** `MI_UI_TeamScore_BarFill` / `_BarFill_Glow` / `_BarFill_NumberGlow` [cyan via reused Ammo_Full] --
  IF the score bar is tinted per-team at runtime, the cyan is a template default -> recolor safe (group A); if NOT
  per-team, it is a generic score bar -> group A. Verify per-team tinting before recolor.

### THE TEAM-COLOR QUESTION (operator-decides -- NOT AIK's call)
House color moved cyan -> electric-blue `#1E5AFF` (0.013,0.102,1.0). Blue-team (`TeamDA_Blue.TeamColor`) = (0.06,0.36,0.97),
its control-ring marker = cyan (0.46,1.0,0.88). Red-team = red (distinct, fine). Question: are house-blue and team-blue
now too close? **Reading: they are already DISTINCT** -- team-blue reads cyan-ish, house reads electric-blue; and the SSOT
readability law keeps team on RIM/marker/nameplate, house on chrome/frames (different roles). **Proposal: KEEP blue-team
as-is** (the deprecated house cyan naturally becomes the team-blue distinguisher). Alternatives if the operator sees a
clash: (b) shift blue-team cooler/greener for more separation, or (c) leave markers, only ensure no house electric-blue
lands on a team surface. DO NOT make blue-team == house electric-blue (that WOULD collide chrome with team-read).

### Proposed order (easy-first, on approval)
1. **Group A recolor** (~22 instances, cyan -> electric-blue at instance level, masters untouched, KEEP healing green) -- PIE-prove HUD in a live match.
2. **Group B per the operator's team-color decision** (default = keep blue-team as-is; only touch if they choose to shift).
3. Re-check log; verify team-distinction (blue vs red) still reads in a 2-team PIE match.

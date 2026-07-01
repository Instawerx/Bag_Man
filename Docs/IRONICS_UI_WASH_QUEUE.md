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

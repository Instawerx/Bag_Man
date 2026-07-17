# IRONICS — Shared-Asset Wash Inventory (SSOT)

**Purpose:** the ONE place the **root-up branding wash** is planned. Stock Lyra/ShooterCore ships
Epic/UE/Lyra logos across the front-end, loading screens, and inside maps. Because those logos live in
**shared source assets** referenced by many things, **one in-place content-swap at the source re-brands every
surface that references it at once** — DRY. This file lists those source assets, what each swap propagates to,
the swap method, and the shared-vs-per-map split. Companion to `Docs/maps/MAP_DISPLAY_NAME_REGISTRY.md`
(that reconciles tile names ↔ maps; this reconciles branding source ↔ every surface it paints).

Verified via Asset-Registry referencer queries, 2026-07-17. Referencer counts exclude the `*_Label`
primary-asset-list noise (not a real content referencer).

> ⚠ **STRUCTURE here, STATE in the wash queue.** This file is the shared-source *leverage* map (what a swap
> propagates to). The **execution STATE authority is `Docs/IRONICS_UI_WASH_QUEUE.md`** — always reconcile against
> it before swapping. Reconciled 2026-07-17: **the UI-logo washes were done WIDGET-LEVEL, per-surface, months ago**
> (see the STATE column below). The referencer graph shows a texture is *reachable*; it does **not** show whether
> the surface already got a widget-level IRONICS swap. Cross-check the queue for state; use this for leverage.

---

## The method (non-negotiable — same discipline as the codename rule)

**In-place content-swap: re-content the asset at its EXISTING path/name; NEVER rename or move it.** Every
referencer inherits automatically because the path is stable. This is the art-side twin of the internal-codename
rule (`project_ironics_map_roster_naming_system`): swap the *content*, keep the *path*, references hold.

| Asset type | How to re-content in place |
|---|---|
| **Texture** (`T_UI_UE5Logo`, `T_UI_LyraLogo`) | Reimport the IRONICS logo file OVER the existing texture asset (same path). |
| **Static mesh** (`UELogo`) | Reassign its material to an IRONICS-logo material (or replace geometry) in place. |
| **Material instance** (`MI_UI_UE5Logo_*`) | Retarget parent/params in place. |
| **Widget** (`W_LyraLogo_LoadingScreen`) | Swap the brush/texture it draws, in place. |

⚠ **STOCK-EDIT NOTE (record explicitly, so a future session reads edited-stock as INTENTIONAL, not a defect):**
these are **stock plugin assets** (`/ShooterMaps/...`, Lyra `/Game/UI/...`) **deliberately re-branded**. That is
the correct trade: **Lyra here is frozen-FOR-IRONICS, not consumed upstream** → there is no upstream merge, so the
"don't edit stock" rule doesn't apply. The alternative (redirect every referencer to a new asset) throws the DRY
leverage away. **Track the assets below as "the wash set" = a deliberate, tracked re-brand.**

---

## THE WASH SET — shared branding source assets (rank = surfaces one swap re-brands)

### 1. Loading-screen logo — `W_LyraLogo_LoadingScreen` (widget) — ✅ **ALREADY DONE**
- **STATE: ✅ WASHED + PIE-PROVEN 2026-06-30** (`IRONICS_UI_WASH_QUEUE.md` QUEUE ITEM 1). Widget-level swap: the
  `Logo` Image → `T_IRONICS_Logo_Transparent` (460px centered), `LogoGlow`/`LogoSoftGlow` **hidden**, electric-blue
  spinner kept. Disk-confirmed: the widget references `T_IRONICS_Logo_Transparent` (1254² real texture), not the
  Lyra logo. **Do NOT re-swap.**
- Source: `/Game/UI/Foundation/LoadingScreen/W_LyraLogo_LoadingScreen` → `W_LoadingScreen_DefaultContent`
  (**9 referencers**) → every playlist's load screen incl. both AFL maps. That reach is *why it was done first* —
  it's already inherited everywhere.
- ⚠ Residual (cosmetic-only, NOT rendering): the hidden glow material `MI_UI_Logo_Glow_DefaultLoadingScreen` still
  samples `T_UI_LyraLogo_2K` — but the widget **hides** those glow Images, so no Lyra shape renders. Leave it.

### 2. In-world 3D UE logo — `UELogo` (static mesh) — the front-end scene + INFINEON's map
- **Source:** `/ShooterMaps/Meshes/UELogo`
- **7 referencers → two distinct surfaces:**
  - `/ShooterMaps/Maps/**L_ShooterFrontendBackground**` — the front-end 3D background scene (the UE-logo props in the menu screenshot).
  - **6× `L_Expanse` external actors** — the UE-logo props placed **inside INFINEON's own map** (`L_Expanse`).
- **One mesh swap → the front-end background AND the 6 in-map UE-logo props on INFINEON, simultaneously.**

### 3. Lyra wordmark (startup / credits / countdown) — `T_UI_LyraLogo` (texture)
- **Source:** `/Game/UI/Menu/Art/T_UI_LyraLogo`
- **→ `M_UI_Base_Logo`** → {`MI_UI_CountdownGlow`, `MI_UI_Logo`}  (countdown + logo widgets)
- **→ `MI_UI_Logo_DefaultLoadingScreen`** → {`W_Credits`, **`W_LyraStartup`** (the startup/splash screen)}
- **One texture swap → the Lyra wordmark on startup, credits, and the countdown.**

### 4. UI UE5 logo (front-end menu) — `T_UI_UE5Logo` (texture)
- **Source:** `/Game/UI/Menu/Art/T_UI_UE5Logo`
- **→ `MI_UI_UE5Logo_Small`** → {**`W_LyraFrontEnd`** (the main front-end menu), `MI_UI_UE5Logo_Large` → `W_Credits`}
- **One texture swap → the UE5 logo on the front-end menu + credits.**

> All four are **shared-source → wash ONCE.** Together they cover: the front-end menu, the startup/splash, credits,
> the countdown, **every loading screen**, and the in-world logos on the front-end scene + INFINEON's map.

---

## DO-NOT-WASH — `T_UE_Logo_V2` / `MF_logo` (verified 2026-07-17)

`T_UE_Logo_V2` has the **highest raw referencer count (43)** — and is the **wrong asset to touch.** Verified this session:
- **Still the stock UE mannequin logo** — source `C:/Program Files/Epic Games/temp/Masculine/T_UE_Logo_V2.BMP`
  (never reskinned in place); `MF_logo` still samples it.
- It is the **character-logo DEFAULT** on `M_Mannequin`'s logo channel (via the shared `MF_logo` projector), **NOT
  map/screen branding.**
- The AFL robots **OVERRIDE** the logo channel per-identity (chest `T_*_Logo_BC`, mask `T_AFL_Visor_*`), so
  `T_UE_Logo_V2` **does not render on shipped AFL robots** — it's the inert inherited default; the 43 refs are the
  AFL body MIs (overriding it) + stock mannequin materials.
- **`MF_logo` is fragile + load-bearing** — editing it (or flipping a logo texture's VT type) is a **known
  regression** that blanked ARIA's visor and cost a restore-from-proven saga
  (`reference_facemask_mfl_logo_regression`). **NEVER edit `MF_logo` or `T_UE_Logo_V2` in place.**

**Classification: NOT part of the map/screen wash. DO-NOT-TOUCH.** (Character branding is a separate axis, already
handled by per-identity overrides; the stock mannequin's own UE logo is not shipped on AFL content.)

---

## Shared-vs-per-map doctrine (the wash split)

- **SHARED-SOURCE → wash ONCE:** the four branding assets above (logos, UI wordmarks, loading screen). Used across
  many maps/screens; one source-swap paints them all.
- **PER-MAP-UNIQUE → per-map work (cannot be shared-sourced):**
  - **Skyboxes** — each map ships its **own** sky from its content pack (CyberPunk, Polar, Greek_island, Sci-Fi
    Valley, Wild_West, engine `BP_Sky_Sphere`…). **No shared branding sky exists** — sky is per-map.
  - **Map environment** — geometry / lighting / decals unique to the map (its identity).
  - **Gameplay wiring** — spawns / extraction / the experience team-spawn-bot trio (e.g. INFINEON's 8v8 config +
    `L_Expanse` spawns, per `MAP_DISPLAY_NAME_REGISTRY.md`).
  - **`L_ShooterFrontendBackground`** — the front-end 3D scene env is its own per-scene art wash (its UE-logo props
    are fixed by wash-set #2, but its environment geometry/lighting is per-scene).

---

## STATE reconciliation (against `IRONICS_UI_WASH_QUEUE.md`, 2026-07-17)
The UI-logo wash is **mostly already done, widget-level, per-surface** (not source-swap):
- ✅ **Loading screen** — DONE + PIE-proven (QUEUE ITEM 1).
- ✅ **Credits (`W_Credits`)** — DONE (widget `Logo` → `T_IRONICS_Logo_Transparent`, glows hidden).
- ✅ Map tiles, 3D hero emblem, post-match takeover, Batch-3 HUD — DONE (QUEUE ITEM 2/3).

**Method that was actually used = WIDGET-LEVEL (repoint the widget's `Logo` Image + hide the Lyra glows), NOT a
source-texture swap.** Reason: the shared Lyra logo materials (`MI_UI_Logo_*`) serve multiple surfaces with
different needs (hide glow on loading, resize per screen), so per-surface widget control was chosen over a blanket
source reimport. **Source-swap (in-place reimport) remains the right tool for the in-world `UELogo` MESH** (one
mesh, identical treatment in every map) — but NOT for the UI logos (already handled widget-level).

## REAL remaining branding (verify visible-vs-hidden before swapping — the loading glows taught us a reference ≠ a render)
1. **Front-end menu UE5 logo** — `W_LyraFrontEnd` still references `MI_UI_UE5Logo_Small` (UE5 logo). Confirm it
   renders (vs hidden by the hub retheme) before scoping. If visible → widget-level swap to the IRONICS mark.
2. **Startup / splash** — `W_LyraStartup` still references `MI_UI_Logo_DefaultLoadingScreen` (Lyra shape). Confirm
   it renders; not recorded as washed in the queue.
3. **In-world `UELogo` mesh** — split by scene:
   - ✅ **Front-end scene (`L_ShooterFrontendBackground`) — DONE + operator-confirmed visible + saved 2026-07-17.**
     5 UE-logo actors → **2 flat IRONICS logo planes** (`IRONICS_Logo_FE_L/_R`, `/Engine/BasicShapes/Plane` @120cm)
     with **`M_AFL_IRONICS_LogoFlat`** (`/Game/BagMan/Materials/`: **unlit + emissive + two-sided**, emissive =
     `T_IRONICS_Logo_Transparent`). Operator hand-tuned roll (~90°) for upright + saved the level.
     ⚠ **RECIPE LESSON:** the chrome **`SM_IRONICS_Emblem` medallion FAILED here** — it's the *lit foreground menu
     hero* (needs a Key/Fill light rig), so it rendered dark on the unlit backdrop wall. **For distant backdrop
     branding use the FLAT plane + unlit/emissive/two-sided material** (self-lit, any-angle), like the flat UE logos
     it replaces. The medallion stays foreground-only (`L_LyraFrontEnd`).
   - ⏸ **INFINEON `L_Expanse` (6 props) — DEFERRED.** Render-verify when `L_Expanse` is the open level, then apply
     the SAME flat-logo recipe (`M_AFL_IRONICS_LogoFlat`). Don't touch blind.

**IRONICS logo asset on hand (game-ready, no import needed):** `T_IRONICS_Logo_Transparent`
(`/Game/Characters/Cosmetics/`, 1254², already wired into the done washes). ⚠ `T_IRONICS_Wordmark` is only 32² — a
stub, NOT game-ready for a large logo. For the in-world mesh, an IRONICS emblem mesh exists
(`SM_IRONICS_Emblem` + `M_IRONICS_Emblem_*`, used by the front-end 3D emblem).

**No content has been swapped.** Read-only surface map. Reconcile each candidate against the wash queue + confirm
it actually renders before the swap.

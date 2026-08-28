# IRONICS_CC_DESIGN_SPEC

**Status:** PRODUCED — awaiting operator ratification (ratified values append to the intent lock as I-25+).
**Date:** 2026-08-27
**Canvas (the 1:1 mocks, editable):** https://claude.ai/code/artifact/6703f673-2ef1-4e3d-b31a-39a65f883e48
**Basis:** `IRONICS_CC_DESIGN_BRIEF.md` §0–§7 · operator rulings I-26/I-27 (AAA revamp, free-moving
loadout, one-click verbs) and the PX spatial-store rulings (2026-08-27, recorded below) ·
`IRONICS_CC_INTEGRATION_PLAN.md` §1 intent lock · bind contract landed at `ac6dc9c3`/`60d02dc7`.
**Pipeline position:** Claude Design output. Claude Code builds the WBPs from THIS spec and the
ratified canvas — never from memory of the old screens (I-26: the old surface is rejected).

---

## 1 · Overview

One widget kit, three thin shells (I-11/I-18), one preview spine. The **creator** (Robo Labs) is a
full-screen six-region builder around the live pawn; the **loadout** (Barracks) is an overlay beside
the player's *free-moving* character; the **PX store** is not a screen at all — it is a **walkable,
stocked store in the world** with a minimal diegetic UI at the shelf. Brand: cyber/neon per the §0
lock, everywhere, identically.

**Operator PX rulings (2026-08-27, chat):**
1. The store follows the spatial concept in the docs — items ON DISPLAY in the world; the player
   walks in like a real store, **picks up, tries on, puts back**.
2. **Nothing leaves the store unbought.** Leaving the store auto-returns any held/tried item
   (the existing try-on restore on PX-zone exit). Buy happens at the point of decision
   (`ClientRequestPurchase`, confirm on `UI.Layer.Modal`).
3. Every other shell (creator / entry / loadout / kit) is approved in direction — "on point and AAA".
4. **Engineering note (operator):** the store is handled like gameplay maps handle weapon spawners /
   loot spawns — the display fixtures ARE the proven spawner pattern, recoloured
   (`AAFLDisplayPedestal` = weapon-spawner child, `AAFLDisplayRack` catalog-driven spawn; SSOT §2.5;
   interact = the existing verb; try-on = `UAFLCosmeticPreviewComponent`). Engineering at highest
   quality; the player-facing flow stays three verbs — simple, easy, fun.

## 2 · Layout — numbers (settles I-22)

### Creator `AFLW_Creator` @ 1280×720 (settlement canvas)

Grid: 8 px base unit. Outer margin **16**, gutter **16**.

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Header (lockup + **A** + **D counter**) | 16 | 16 | 1248 | 56 | A centered, 2 segments 172×44 in a 6px-padded 10px-radius well |
| **C** Channel rail | 16 | 88 | 304 | 536 | Panel gradient `#0E122B → #0B0F24`, radius 10 |
| **B** Preview viewport | 336 | 88 | 744 | 536 | **Largest region by area (398,784 px²)** — I-21 satisfied |
| **D** Saved-builds strip | 1096 | 88 | 168 | 536 | 5 slots @ 152×92, gap 10 |
| Footer (**E** + **F**) | 16 | 640 | 1248 | 64 | E field 360×44 left; F buttons right, gap 12 |

Width check: 16+304+16+744+16+168+16 = 1280 ✓. Buttons h **44** (≥44 pt touch floor);
Entry CTA h 52; PX Buy h 48. Radii: panels 10–12, controls 7–8, pills 20.

### 1920×1080 scale check

**Uniform 1.5× of the 720p canvas** (matches UMG DPI scaling — one authored layout, no reflow).
Console title-safe overlay: **5% inset** → rect at (96, 54) 1728×972. Verified on the CreatorHD
artboard; nothing load-bearing sits outside title-safe.

### Type ramp (720p; scales 1.5× at 1080p)

| Use | Face | Size / tracking |
|---|---|---|
| Entry title | Orbitron 700 | 44 / 0.24em |
| Build name in B | Orbitron 600 | 20 / 0.12em |
| Shell titles | Orbitron 700 | 15–17 / 0.2em |
| Region/segment labels | Orbitron 500–600 | 10–13 / 0.14em |
| Body / descriptions | Noto Sans 400 | 12–14, lh 1.5 |
| Data (prices, ids, counters, hints) | DroidSansMono stack¹ | 8–11 / 0.1–0.3em |

¹ `'DroidSansMono','Droid Sans Mono',ui-monospace,Consolas,monospace` (matches the roadmap's stack;
Droid Sans Mono is not on Google Fonts — the fallback metrics are close).

## 3 · Tokens

**Locked (§0 — not negotiable):** ground `#222A3A` · surface-card `#0E122B` · accent
`#1E5AFF` · Watts `#FF00D5` · Orbitron / NotoSans / DroidSansMono · cyber/neon · no cyan chrome,
no frosted/white, no `#64B4FF`, no SF Pro.

**PROPOSED (ratify with the mock — provenance: `IRONICS_LOBBY_HUB_ROADMAP.html` `:root`, already
shipped project values):** line `#2C3550` · line-2 `#3A4666` · text `#E8ECF7` · text-mute
`#9AA4C4` · text-dim `#6B7599` · panel-deep `#0B0F24` (panel gradient floor) · ok `#3DDC84` ·
warn `#FFB020` · danger `#FF3355` · scene gradient stops `#2B3550 / #1B2231 / #181E2B` ·
robot-shell fills `#0B0F1E / #0F1526`.

Hue-arc gradients and item glow colours (`#3DFF7E`, `#6CC4FF`, …) are **colour DATA** (identity/SKU
colours), never UI chrome — the cyan rule is not violated by the arc or by item renders.

## 4 · Screens

- **Entry** (`Entry` artboard): brand lockup, one action **START A BUILD**, ambient grid + glow,
  zero dead panels (I-2). Pushed via `afl.Store.Open` on `UI.Layer.Menu`.
- **Creator** (`Main`): regions per §2. A is first-class segmented control with reason line from
  `IsChassisLineAvailable`; C is schema-driven (arcs: Neon+Edge linked w/ unlink, Visor, Emblem +
  Facemask/Emblem/Finish part tiles + entitlement mode pill); B shows the LIVE pawn (PreviewRT) with
  Portrait/Combat-range toggle and drag-rotate affordance (I-5/I-6); D always shows "n / cap" +
  saved/active/saved-locked/empty slots (I-12); E filtered unique name; F Revert / Equip / Save.
- **Loadout** (`Loadout`): overlay at ~60% width; the world (and the player's **free-moving** pawn +
  mirror) stays visible and controllable left (I-27). Owned-only grid, selected-item detail with
  segment-bar stats, **one-click verbs: EQUIP (primary) / SWAP SLOT / DISCARD** (I-27). Commit =
  `ServerSetCosmeticSelection` only.
- **PX Store** (`ProductPage` artboard, spatial): stocked walk-in store — neon section signage
  (MASKS / WEAPONS / CHASSIS / JEWELLERY), stocked mask wall + weapons racks + chassis display podium
  + glass jewellery counter + mirror; held-item nameplate with price; verbs **E TRY ON / F BUY /
  Q PUT BACK**; flow strip states the loop; leaving auto-returns unbought items. Racks are
  catalog-driven (new row = new stock, no map edit). Buy = `ClientRequestPurchase`.
- **Front-end variants** (no separate artboard — delta only): same shells; B becomes the full-bleed
  PreviewRT capture of the display pawn (I-23); the world-side strip is absent. Kit unchanged.

## 5 · Kit components & states

Drawn exhaustively on the **Kit states** page of the canvas:

- `AFLW_HueArc` (KitColor): idle · focused (accent ring, gamepad) · dragging (grown handle + hue
  chip) · **discrete-SKU** (owned solid/snappable, unowned hollow; continuum dimmed; I-9) ·
  **continuum** (subscriber) · linked (fused icon) · unlinked (two arcs) · read-only lapsed (lock +
  reason; colours stay). **No RGB sliders, no hex input, anywhere (I-8).**
- `AFLW_PartPicker` tile (KitTiles): default · focused · selected/previewing · owned · priced-Volts ·
  priced-Watts (magenta only) · entitled-but-locked · unavailable. Select = `BeginPreview` only.
- `AFLW_BuildSlotStrip` item (KitTiles): empty · saved · active · **saved-locked with reason line**
  · dirty. Counter always visible; over-cap locked, never hidden (I-12/I-14).
- `AFLW_ChassisPicker` (KitChrome): X selected · Original available · Original disabled + data reason.
- `AFLW_ActionBar` (KitChrome): Save enabled / disabled+reason · Revert · Equip · Buy-V · Buy-W ·
  confirm modal on `UI.Layer.Modal`.
- Viewport toggle: Portrait ↔ Combat-range (mandatory, I-6).

## 6 · Motion · Accessibility · Responsive

- Motion: one family — **120–180 ms** accent glow-in on focus; **240 ms** cubic viewport blend;
  no bounce. Reduced-motion: both instant.
- Accessibility: body text on surface-card ≥ 4.5:1 (`#E8ECF7`/`#9AA4C4` on `#0E122B` pass);
  colour never the only carrier — every state pairs badge + label; CommonUI focus always visible;
  first focusable on entry = A.
- Responsive: 16:9 only for v1; 1080p = uniform 1.5×; console title-safe 5%; touch targets ≥ 44 pt;
  CommonUI prompt sets swap per platform.

## 7 · Bind map (against the landed C++ contract)

| Element | Bind | Status |
|---|---|---|
| Chassis segmented control | `A_ChassisPicker` + `A_ChassisManny` / `A_ChassisProMod` (+ label & reason texts) | landed (`AFLW_Creator.h`, BindWidget) |
| Preview image in B | `B_PreviewImage` ← `PreviewRT` | landed (BindWidget) |
| Channel rail host | `ChannelRailContainer` | landed |
| Slot counter | `D_SlotCounter` | landed |
| Build-name field | `E_BuildName` | landed |
| Save / Revert | `F_Save` / `F_Revert` | landed (BindWidget) |
| **Equip in F** (saved build selected) | — | **GAP → ticket (cites I-13)** |
| **Build-slot strip items** (D) | `AFLW_BuildSlotStrip` | C2 kit widget (AFL-3222) — binds authored with the kit |
| **Loadout verbs Swap / Discard** | — | **GAP → ticket (cites I-27)**; Equip = existing seam |
| **PX world verbs (pick up / try on / put back)** | world interact, not WBP binds | H3 pedestal/rack + preview component (AFL-3030/3210) |
| Signage, flow strips, scenery, region corner-tags | — | decorative (mock annotation only; corner-tags do not ship) |

## 8 · Must-not walk (brief §6)

RGB sliders / hex / unclamped picker — **PASS** (arcs only) · hero render / stand-in — **PASS**
(robot figures mark the live PreviewRT / world pawn; final render is the real capture) · frosted
glass / white / `#64B4FF` / SF Pro — **PASS** · cyan as chrome — **PASS** (cyan only in colour data)
· second screen family for Original vs X — **PASS** (one shell, A switches schema) · invented
tokens — **PASS** (extensions marked PROPOSED §3) · layout unmappable to the bind contract —
**PASS** (gaps flagged, none hidden — §7).

## 9 · Acceptance walk (brief §7)

Grid/gutters/proportions as numbers at both canvases — **§2** ✓ · every element bound or marked
decorative/gap — **§7** ✓ · every value traces to §0 or PROPOSED — **§3** ✓ · all §4 states drawn —
**§5 / canvas Kit page** ✓ · empty state + saved-locked + discrete-vs-continuum explicit — **Entry
artboard + KitTiles + KitColor** ✓ · operator ratification — **PENDING** (ratified values → I-25+).

---
*Produced 2026-08-27 by the Claude Design lane under the mockup-first law (CLAUDE.md doctrine 7).*

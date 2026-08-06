# IRONICS — LOADOUT / LOCKER SCREEN (#7) — DESIGN OF RECORD

**Status:** APPROVED design — operator-confirmed 2026-07-10 (design pass + all 6 decisions ruled). The
player-facing screen that makes owned cosmetics **player-selectable in gameplay**, retiring cheat-driven
selection (`afl.Cosmetic.Set*`). Build is incremental, **weapon-picker first**.

**Grounds in (conform, invent nothing):**
- Selection model: `AFLCosmeticSelectionTypes.h` (`FAFLCosmeticSelection`) + `AFLCosmeticLoadoutComponent`.
- Entitlement chain: `IRONICS_PLAYER_FLOW.md` §4 (buy→own→equip) — the loadout UI is its flagged GAP (§10 #9, §11 #5).
- Design tokens: `IRONICS_UI_STYLE_SSOT.md` (electric-blue lead / arc-violet accent-state / Apple-Glass / type ramp / component primitives).
- Widget pattern: CommonUI `PushWidgetToLayerStack(UI.Layer.Menu)`; the proven AFL split (C++ base owns bindings, WBP child owns layout); precedent `UAFLW_MatchScoreboard : UCommonActivatableWidget`.
- Economy: general-catalog priced + closed (`IRONICS_PRICING_SCARCITY_SSOT.md` R6) — mostly-free base roster.

---

## 1. THE SELECTION MODEL (confirmed — all PROVEN, no new backend)

`FAFLCosmeticSelection` — one server-authoritative replicated struct on `UAFLCosmeticLoadoutComponent`
(on `LyraPlayerState`). Stores **FName keys, not asset pointers**. The eight axes:

| Slot (axis) | Field | Consumer (proven) | Query type |
|---|---|---|---|
| **Identity** (Team **OR** Character, either/or) | `IdentityType` + `TeamId`/`CharacterId` | spawn-read | `Team` / `Character` |
| **Weapon** | `WeaponId` | `RefreshWeaponForPawn` (server-only D2-replace) | `Weapon` |
| **Weapon skin** | `WeaponSkinId` | `RefreshWeaponSkinForPawn` (universal MI) | *(namespace `AFL.WeaponSkin.`)* |
| **Beam** | `BeamId` | `RefreshBeamColorForPawn` | `Beam` |
| **Facemask** | `FacemaskId` | `RefreshFacemaskForPawn` (Visors canary) | `Facemask` |
| **Edge color** | `EdgeId` | `SetSkinColor` (#38a) | `SkinColor_Edge` |
| **Body color** | `BodyId` | body-color axis (Option B) | `SkinColor_Body` / `Finish` |
| **Helmet** | `HelmetId` | ⚠️ part-path likely retired — **PARKED (decision 5)** | `Helmet` |

**Read/write API (exact, disk-verified):**
- `GetSelection()` — `BlueprintPure`, any client, reads the current selection.
- `ServerSetCosmeticSelection(FAFLCosmeticSelection)` — `Server, Reliable, WithValidation`, **`BlueprintAuthorityOnly`** → the client must dispatch it from **C++** (the BP node self-gates on authority; the console cheats already call it from C++). Server re-validates entitlement (`AxisEntitled`).
- `GetEntitlementSource()` → `IAFLEntitlementSource*` → `IsEntitled(const ALyraPlayerState*, FName)`.
- `UAFLCosmeticCatalogSubsystem::GetEntriesByType(EAFLCosmeticType, TArray<const FAFLCatalogEntry*>&)` — **C++-only** (const-ptr array) → the base wraps it to a BP-friendly `TArray<FAFLCatalogEntry>`.

**The own→apply loop (the keystone — retires the cheats):**
```
GetEntriesByType(axisType)             // enumerate all SKUs of the axis
  → filter GetEntitlementSource()->IsEntitled(PS, id)   // OWNED-only (GrantedFree auto-owns; paid = owned-set)
  → player taps a tile
  → copy GetSelection(), set that ONE axis field
  → ServerSetCosmeticSelection(copy)   // server re-validates, sets the replicated selection
  → OnRep_Selection → Refresh*ForPawn  // live preview + gameplay equip
```
This is exactly Player Flow §4c and the proven Visors buy→equip loop. **No new backend** — only the UI is missing.

---

## 2. THE LOCKER DESIGN (paper-doll — decision 1)

A `UCommonActivatableWidget` pushed to `UI.Layer.Menu`, reachable from the **IRONICS hub** and the
**pre-match lobby** (decision 6). A live 3D preview of the player's robot center-stage, slot buttons
framing it; tapping a slot opens an **owned-items drawer**.

```
┌─ LOADOUT ─────────────────────────────────────────────┐
│  [IDENTITY ▾ IRONICS]                                  │
│   ┌─ WEAPON ─┐            ╔══════════════╗   ┌ SKIN ─┐ │
│   │ Voltaic  │            ║  LIVE 3D     ║   │ Azure │ │
│   └──────────┘            ║  PREVIEW     ║   └───────┘ │
│   ┌─ BEAM ───┐            ║ (robot+gun)  ║   ┌ MASK ─┐ │
│   │ Electric │            ╚══════════════╝   │ Japan │ │
│   └──────────┘   ┌ COLOR (body+edge) ┐      └───────┘ │
│                  │ Blue / NeonBlue    │   ┌ SPECIAL ┐  │
│                  └────────────────────┘   │ EMP     │  │
│  ── tap a slot → OWNED-ONLY drawer slides in ──        │
└────────────────────────────────────────────────────────┘
```

**Slot taxonomy (6 player-facing slots ← 8 axes; Helmet parked):** Identity · Weapon · Weapon skin ·
Beam · Color (Body+Edge) · Special/Mask (Facemask + the AbilityCosmetic EMP).

**Per-slot drawer** = an **OWNED-ONLY** tile grid (`GetEntriesByType` → `IsEntitled` filter). Tile =
thumbnail + name + **EQUIPPED / OWNED** badge (reuses the `StoreTile` visual with the price chip swapped
for an EQUIPPED badge). Tap → `ServerSetCosmeticSelection` → live preview. A single **"＋ Get more"** tile
deep-links to the shop tab for that axis — the only store touch in the locker.

**Fire-mode = the weapon (decision 4).** Beam WIDs and pulse WIDs sit in the *same* owned weapon grid;
picking one sets the fire behaviour. No fire-mode toggle. Single `WeaponId`; carrying a beam **and** a
pulse for in-match swap is **Loadout-v2** (needs a `FAFLCosmeticSelection` Primary/Secondary change).

**Preview (decision 2):** 2D thumbnails in the drawer grid + **one shared 3D preview pawn** that re-applies
the selection live. Not per-tile 3D.

**Conformance (UI Style SSOT):** every surface `UIPanel.Glass`; selected tile = electric-blue fill +
**arc-violet selected-rim** (state layer); ALL-CAPS Orbitron/Chakra slot labels; JetBrains-Mono counts;
luminous-restraint glow on equip. C++ base owns bindings; WBP child owns layout.

---

## 3. BUILD SCOPE (incremental — weapon-picker first)

| Inc | Deliverable | Closes | Reuse vs new |
|---|---|---|---|
| **1 — Weapon Picker** | `UAFLW_LoadoutBase` (C++) + `AFLW_Loadout_AxisPicker` (axis=Weapon) + paper-doll WBP + tile | the **weapon loop** (retires `afl.Cosmetic.SetWeapon`) | REUSE StoreTile→LoadoutTile, tokens, CommonUI push · NEW base bindings, owned-filter, EQUIPPED state |
| **2 — Skin + Beam** | `WeaponSkinId` + `BeamId` pickers | weapon-cosmetic axes | REUSE the AxisPicker (parameterize) · NEW beam swatch drawer |
| **3 — Identity + Color + Mask** | Team/Character, Body+Edge, Facemask = **full locker** | identity + color + mask loops | REUSE AxisPicker · NEW identity block |
| **4 — Preview + Special + polish** | shared 3D preview pawn, EMP special slot, motion/AAA pass | fidelity | NEW preview viewport + preview-pawn re-apply |

The **shared engine** is one parameterized `AFLW_Loadout_AxisPicker` (axis in → owned grid → equip out);
Increments 2–4 are mostly a new axis binding + drawer skin. All conform to Inc-1.

---

## 4. NOT IN SCOPE — the Collection / Feed surface (separate companion build)

The locker is **OWNED-ONLY (decision 3)**. A **Collection / Feed** marketing surface (browse ALL SKUs,
unowned greyed with inline BUY, completionist %, "new drops" feed) is a **SEPARATE downstream build** that
ties to **Store Front-End (#1)** + **Menus/Marketing (#11)** — NOT the loadout. The locker's only store
touch is the per-axis **"＋ Get more"** deep-link to the shop. Do not add unowned/greyed tiles or inline-buy
to the locker.

---

## 5. DEFERRED / PARKED
- **Multi-weapon slot** (carry beam+pulse, in-match swap) + Watts-purchasable extra slots → **Loadout-v2**
  (needs `FAFLCosmeticSelection` Primary/Secondary or `WeaponId[]`).
- **Helmet axis** — struct field exists but the part-path consumer looks retired → **PARKED (decision 5)**.
- **AbilityCosmetic (EMP) special slot** — no selection field yet; lands in Increment 4.
- Doc drift note: `IRONICS_UI_STYLE_SSOT.md` §5 still names the pre-R6 tiers (SPARK/SURGE/ARC/THUNDERBOLT);
  the locker shows `GetEntryPriceText` ("Free" / "990 V / 9,900 W" / "7,990 V"), not tier names — no conflict.

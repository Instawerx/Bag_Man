# IRONICS_CC_DESIGN_BRIEF

**Purpose:** the input Claude Design consumes to produce the visual layer and spec sheet for the
Character Creator and the two shells that share its kit — Loadout (Barracks) and Store product page
(PX). This is the "mock at 1280×720" that `IRONICS_CC_UI_HANDOFF` §2 defers to; its output settles
intent item I-22 (grid, gutters, proportions) and becomes I-24+ in `IRONICS_CC_INTEGRATION_PLAN.md` §1.
**Date:** 2026-08-26
**Pipeline position:** handoff doc (this) → **Claude Design produces mock + spec** → Claude Code binds
the WBPs against the spec and the existing C++ contract. Claude Design does not write engine code;
Claude Code does not invent layout or brand values.
**Basis:** `IRONICS_CC_UI_HANDOFF` §2 · `IRONICS_CHARACTER_CREATOR_SSOT.md` §5–§7 · `IRONICS_PRICING_SSOT.md`
§4–§7 · `IRONICS_CC_INTEGRATION_PLAN.md` §1 (intent lock) · brand tokens ruled in `AFLTokenCompiler.cpp`
· `IRONICS_STORE_COLOR_SPEC.md` **[VERIFY path]** · live tracker Digital Market slice (proven 3-column
market, rarity-framed cards, brand lockup).

---

## 0 · Brand lock — supersedes anything else an agent may read

**This section overrides `Tools/skills/expert-game-designer/references/afl-design.md`.** That file
still records the retired Apple-Glass direction (`#64B4FF` primary, frosted panels, SF Pro). The
tracker corrected the store to cyber/neon on 2026-06-07 ("NOT the earlier white Apple-Glass"). Until
the skill file is updated (AFL-3202), any design output that shows frosted glass, white panels, blue
`#64B4FF`, or SF Pro is a defect.

| Token | Value | Use |
|---|---|---|
| `ground` | `#222A3A` | Page/scene ground, viewport backdrop |
| `surface-card` | `#0E122B` | Panels, cards, rails, bars |
| `accent` | **Electric Neon Blue `#1E5AFF`** | Focus, primary CTA, active state, selection ring, arc handle |
| `watts` | **Magenta `#FF00D5`** | Watts currency, staked/premium marking — never as a general accent |
| Type — display | **Orbitron** | Titles, region headers, build name, big numbers |
| Type — body | **NotoSans** | Labels, descriptions, prompts |
| Type — data | **DroidSansMono** | Prices, slot counter "n / cap", ids, stats |
| Direction | **Cyber / neon**: emissive rim-glow, gradient panels, subtle grid, scan-line restraint | Layer-1 neon-material pass on the tracker |

**Hard rules:** no cyan as an accent (cyan appears only inside rarity/colour-identity data, never as
UI chrome) · no white/frosted glass · no invented tokens — if a needed value is not in this table,
the spec names it **PROPOSED** and it is ratified before use · rarity is a badge axis (frame colour
via `GetRarityColor`), never the identity colour · Volts vs Watts are always visually distinct
(Watts = magenta pill; Volts = accent pill).

---

## 1 · Deliverables

1. **Creator mock** — `AFLW_Creator`, base canvas **1280×720** (the handoff's settlement canvas), plus
   a 1920×1080 scale check and a 16:9 console safe-area overlay. Mobile: 16:9 landscape only for v1,
   touch targets ≥ 44 pt.
2. **Loadout product-page mock** — `AFLW_Loadout`, same canvas set.
3. **Store product-page mock** — `AFLW_ProductPage`, same canvas set.
4. **Kit component sheet** — every shared widget with all states (§4).
5. **Spec sheet** in the `design-handoff` format (Overview · Layout · Tokens · Components · States ·
   Responsive · Edge cases · Motion · Accessibility) — **with the grid, gutters and region proportions
   filled in as numbers**. That is what settles I-22.
6. **Bind map** — every visual element mapped to its C++ bind name. Landed at `ac6dc9c3`
   (`AFLW_Creator.h`): **A** `A_ChassisPicker`, `A_ChassisManny` (Original), `A_ChassisProMod` (X) with
   label + reason text · **B** `B_PreviewImage` (`UImage` → `PreviewRT`) · **C** `ChannelRailContainer` ·
   **D** `D_SlotCounter` · **E** `E_BuildName` · **F** `F_Save`, `F_Revert`. An element with no bind
   name is either decorative (say so) or a spec gap (flag it).

Output goes to `Docs/Hub/Design/` as the mock images/SVG + `IRONICS_CC_DESIGN_SPEC.md`. Nothing in the
engine is touched by Claude Design.

---

## 2 · The creator screen — six regions (I-21), what each must do

Canvas 1280×720, single full-screen activatable on `UI.Layer.Menu`, pushed via the `afl.Store.Open`
pattern (I-2). The screen is a sibling of the Digital Market and must read as the same family (I-11):
same brand lockup position, same card language, same utility bar.

| Region | Content | Requirement (intent) |
|---|---|---|
| **A · Chassis picker** | Two first-class choices: **X** (default, first) and **Original** (present, may be disabled/"coming" — shows availability from `IsChassisLineAvailable`) | Ordered first: it determines C's contents (I-3, I-21). Not a dropdown, not a hidden toggle. |
| **B · Preview viewport** | The robot. `UImage` bound to `PreviewRT`. **Largest element by area.** Two view states: **Portrait** and **Combat-range** (I-6), one toggle. Drag-rotate affordance. | The preview *is* the product (I-5) — the mock shows the real chassis at the real proportions, no hero art, no stand-in. Lighting matches play. |
| **C · Channel rail** | Data-driven from the chassis's master: Neon, Edge, Visor, Emblem (+ Chassis albedo only if proven). Each channel = one **hue arc** (I-8) with 3–4 chroma stops; Neon+Edge **linked** by default with an unlink toggle. Below the arcs: Facemask, Emblem id, Finish pickers (I-4). | **No RGB sliders. No hex input.** Free-player mode shows the arc with only *owned* colour SKUs as stops (I-9); subscriber mode shows the continuum. Finish scalars are not player-facing (I-9). |
| **D · Slot counter** | "n / cap" always visible (I-12); saved builds as a strip: name, thumbnail, active marker, **saved-locked** state for over-cap builds (I-12, I-14) | Locked builds stay visible and selectable for viewing; editing controls disabled with a reason line. |
| **E · Build name** | Text field, filtered (I-19); uniqueness/invalid states | Orbitron display of the name in B's frame once set. |
| **F · Commit bar** | `UCommonButtonBase` ×2 — **Save** and **Revert** (I-1, I-7) — plus **Equip** when a saved build is selected (I-13) | Save is gated by entitlement (I-14: everyone builds; save/carry gates). Revert = back to last saved, always available while dirty. |

Flow the mock must make legible without a tutorial: `ENTRY (empty state: one action "Start a build")
→ CHASSIS → BUILD → SAVE → EQUIP` (I-1, I-2). Free movement between Chassis/Build/Save is visible —
nothing looks committed until Save.

---

## 3 · The two product-page shells (I-17, I-18)

Same kit, same B viewport, same D/F bars. They are **product pages, not filtered axis grids**.

| Shell | Left | Centre | Right | Commit |
|---|---|---|---|---|
| **Loadout** (Barracks) | Saved builds + owned parts (PartPicker, filter owned) | B viewport of the local pawn | Selected item details (series, rarity badge, stats meters as UMG segments — tracker ruling, not glyphs) | **Equip** |
| **Store product page** (PX) | One SKU focused (pre-focus from a pedestal); related items row | B viewport with **try-on / hold** applied locally | Price pills (Volts accent / Watts magenta / both when pay-either), owned/entitled badge, description | **Buy** → then **Equip**; Insufficient-funds and not-transactable states use the existing decline copy |

In the hub these shells open as an overlay while the player stands at a pedestal/preview anchor with
a mirror; the mock shows the overlay at ~60% width leaving the world (the robot at the anchor) visible
on one side. In the front end (Landing/Barracks before the hub exists) B is the capture.

---

## 4 · Kit components and states (component sheet)

| Component | States to draw |
|---|---|
| `AFLW_HueArc` | idle · focused (gamepad ring in accent) · dragging · discrete-SKU mode (stops only, unowned stops hollow) · continuum mode · linked (one arc, Neon+Edge icon fused) · unlinked (two arcs) · read-only (lapsed subscriber — I-9/lapse rule: colours stay, control locks) |
| `AFLW_PartPicker` tile | default · focused · selected · owned · priced (Volts / Watts / both) · entitled-but-locked (saved-locked) · unavailable |
| `AFLW_BuildSlotStrip` item | empty slot · saved · active (equipped) · saved-locked (over cap) · dirty (unsaved changes) |
| `AFLW_ChassisPicker` | X selected · Original available · Original disabled with reason |
| `AFLW_ActionBar` | Save enabled/disabled(+reason) · Revert enabled/disabled · Equip · Buy (Volts / Watts) · confirm modal on `UI.Layer.Modal` |
| Viewport toggle | Portrait · Combat-range |
| Empty state | "Start a build" — one action, brand lockup, no dead panels |

Every state uses tokens from §0 only. Focus is always visible for gamepad (CommonUI); the first
focusable on entry is A.

---

## 5 · Platform and input

- CommonUI input prompts switch per platform (PC / PS5 / Xbox / touch); the mock shows the PC set and
  one gamepad set.
- Safe area: 16:9 with a 5% console title-safe overlay on the 1920×1080 check.
- Motion: one transition family — 120–180 ms accent glow-in on focus, 240 ms viewport blend on
  Portrait↔Combat-range, no bounce. Reduced-motion variant noted.
- Accessibility: contrast ≥ 4.5:1 for body text on `surface-card`; colour never the only carrier of
  owned/locked (badge + label).

---

## 6 · Must-not list (a mock showing any of these is returned)

- RGB sliders, hex fields, or an unclamped colour picker (I-8)
- A hero render, concept art, or any preview that is not the real chassis at real proportions (I-5)
- Frosted glass, white panels, `#64B4FF`, SF Pro (§0)
- Cyan as chrome accent (§0)
- A second screen family for Original vs X (I-11)
- Tokens, copy, or brand values not on disk — mark PROPOSED instead
- Layout that cannot map to the existing bind contract without new C++ (flag it, don't hide it)

---

## 7 · Acceptance for the spec sheet

- [ ] Grid, gutters, and A–F region proportions given as numbers at 1280×720 and 1920×1080 (settles I-22)
- [ ] Every element in the bind map has a bind name or is marked decorative/gap
- [ ] Every colour/type value traces to §0 or is marked PROPOSED
- [ ] All §4 states drawn
- [ ] Empty state + saved-locked + discrete-vs-continuum arc shown explicitly (these are the three
  states past attempts skipped)
- [ ] Operator ratifies the mock; ratified values are appended to the intent lock as I-24+

---

## 8 · Paste-ready prompt for Claude Design

```
You are producing the visual layer and spec sheet for the IRONICS Character Creator, Loadout page,
and Store product page. Read Docs/Hub/IRONICS_CC_DESIGN_BRIEF.md in full first; §0 (brand lock)
overrides any AFL design tokens you may find elsewhere, including the expert-game-designer skill's
afl-design.md — that file is stale (Apple Glass) and must not be used.

Produce, in this order:
1. Creator mock at 1280×720 (six regions A–F exactly as §2, X chassis shown, real robot proportions
   in B, hue arcs in C — no sliders), then the 1920×1080 scale check with a console safe-area overlay.
2. Loadout and Store product-page mocks at the same canvases, as §3 (overlay-at-anchor variant + front-end variant).
3. Kit component sheet with every state in §4.
4. Spec sheet in the design-handoff format with grid, gutters and A–F proportions as numbers, a
   tokens table that only uses §0 values (anything else marked PROPOSED), and a bind map to the
   existing C++ names listed in §1.6.
Before finishing, walk the §6 must-not list and the §7 acceptance list and state pass/fail per line.
Write the spec to Docs/Hub/Design/IRONICS_CC_DESIGN_SPEC.md and place mock exports beside it.
```

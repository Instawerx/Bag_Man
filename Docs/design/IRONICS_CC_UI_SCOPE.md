# CC-5 · CREATOR UI — SCOPE BEFORE AUTHORING

Scoped 2026-08-22, before any visual authoring, because the largest remaining piece should not start
on an assumed layout. Every claim here was read from the specs or measured in the code; nothing is
inferred from the roadmap.

---

## 0 · THE ONE-LINE STATE

The behaviour layer is built and proven. **The creator is referenced from exactly one place in the
codebase: `AFLCombatCheats.cpp`.** Every reference — `LoadClass<UAFLW_Creator>`, `CreateWidget<…>` —
is in the cheats file. There is no shipping path to it. That is the gap.

---

## 1 · THE VISUAL LAYER — WHAT THE SSOT DECIDES, AND WHAT IT DOES NOT

### The style SSOT SPECIFIES these — apply exactly, no interpretation needed

| Area | Ruling |
|---|---|
| Palette | House colours + neon registry; **colour-role rule** (glass-panel convention, reused verbatim) |
| Readability | Combat-overlay readability LAW (sec 2.4) |
| Panel system | Apple-Glass tokens; `Glass.Shadow rgba(0,0,0,0.30) 0 8px 32px` |
| Geometry | **panel radius 16–24px, button 12px, input 8px** |
| Typography | **RULED 2026-08-10:** Orbitron (display, ALL-CAPS) / Noto Sans (body) / Droid Sans Mono (numeric). Sizes emitted by `AFLTokenCompiler` at 1280×720 |
| Component tokens | sec 6 primitives, incl. the Arc-Violet focus/hover/active variant that applies to every token |

### The style SSOT is SILENT on exactly the things item 1 asks for

**Grid, gutters, spacing rhythm and region proportions are not specified anywhere in it.** The only
occurrence of those words is as an *input* to a pipeline — *"SVG/HTML mock → design spec
(grid/spacing/…)"* — never as values. Reporting the gap rather than inventing it, as instructed.

### THE OPERATOR PICK THAT BLOCKS AUTHORING

The handoff spec settles this explicitly and hands it back:

> *"The **layout direction itself is still open** — **1a** right-rail, **1b** full-bleed, **1c**
> three-column workbench, **1d** console-first stepper — that is a choice, not a spec, and it is the
> operator's."*
>
> *"Layout grid, gutters, region proportions | Wireframes 1a–1d supply four candidate layouts at
> 1280×720; **which one** remains an operator pick."*

**Direction 1e is NOT a layout** — it is a direction-agnostic state sheet, and it already settled
three things that therefore need no pick:

- `PresentButInert` vs `Absent`: `◐` + dashed + INERT vs `✕` + dotted + ABSENT — *"told apart by
  fill, icon and text, never hue"*
- Unaudited-master treatment: chassis tile carries `unaudited ⚠`, channel offered with the caveat
- Hex vs HSV readout: **both** — `#1E5AFF · H 226° S 0.55 V 0.90`

### Still open beyond the layout pick

| Gap | Whose call | Blocks? |
|---|---|---|
| **Layout direction 1a–1d** | **operator** | **YES — the whole visual layer** |
| **Motion durations + easing** | needs a `Motion.*` token set in the SSOT via `AFLTokenCompiler`; the wireframes specify **no timing values** (checked, not assumed) | YES for §6 Motion |
| Semantic danger/warning mapping (SSOT open 2) | operator sign-off | only if the creator surfaces danger states |
| `UIDisplay.NeonTube` (SSOT open 5) | operator + whoever owns UMG perf; SDF vs texture, and whether it extends past the two R98 door headings | only if creator headings use it |
| Console safe-zone inset, touch target minimum | platform TRC | before console |
| Link default pairing | product ruling | §5.5 link model |

⚠ **One value in the wireframe bundle must not be copied:** the slot upgrade reads *"+5 SLOTS · 400
VOLTS"*. No SSOT carries that price and it contradicts the shipped SKUs (`AFL.CreatorSlot.x1/x3/x8`
at 3,000 / 4,990 / 10,000 Volts, live in PlayFab). Illustrative only.

---

## 2 · THE ENTRY POINT — two proven patterns already in the project

| Pattern | Used by | Call |
|---|---|---|
| Layer push | store | `UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, TAG_UI_Layer_Menu_…, Class)` |
| Layout stack | front-end market / loadout | `Layout->PushWidgetToLayerStack<UAFLW_FrontEndMarket>(…)` |

**Recommendation — from the loadout, and the code already says so.** `UAFLW_Creator::InitializeCreator`
takes a `UAFLW_LoadoutBase*`. That coupling is designed in, not incidental: the creator expects to be
opened *with* a loadout, and the loadout owns the preview pawn the creator rotates. A main-menu entry
would have to synthesise a loadout context that the loadout already holds.

**Whether it ALSO gets a main-menu entry is a flow question, not an engineering one** — it changes
where a player forms the intent "make a robot" versus "change my robot", and that is product intent.

---

## 3 · THE LOADOUT FLOW — the mismatch, stated precisely

The loadout is **axis-oriented and equips ONE id per axis**:

```
GetOwnedEntriesForAxis(Axis, Out)     EquipForAxis(Axis, CosmeticId)
    case EAFLLoadoutAxis::Beam:  Sel.BeamId = CosmeticId; break;
```

**It has no concept of a saved build.** A creator build is a whole `FAFLCreatorBuild` — a multi-axis
selection with a name and a slot. So a build cannot be expressed as "one more axis tile"; it is a
*different kind of thing* the loadout equips.

The operator's framing — *"a build is a saved selection, and the loadout already equips selections"* —
holds at the component layer: `ServerSetActiveBuild(Index)` already switches the whole selection. What
is missing is the **surface**: a Builds region in the loadout listing saved builds with their slot
counter, name state, and an Edit affordance that opens the creator on that build.

---

## 4 · EVERY AXIS REACHABLE — the measured gap

**Implemented in the loadout switch (7):** `Weapon, WeaponSkin, Beam, Identity, BodyColor, EdgeColor,
Facemask`.

| Axis | Enum member | Handled in any switch case | Reachable |
|---|---|---|---|
| Sticker | ✅ declared (CC-7.2) | ❌ **none** | ❌ |
| Accessory | ✅ declared (CC-8) | ❌ **none** | ❌ |
| Emblem | ❌ **no enum member at all** | ❌ | ❌ |

⚠ **AND ADDING SWITCH CASES WOULD NOT BE ENOUGH.** `EquipForAxis` assigns a single `CosmeticId` per
axis. A sticker is **not** a single id — it is a zone plus a position, scale and rotation inside that
zone. An accessory is an id plus a **slot**. The one-id-per-axis model cannot express either.

**Sticker placement is therefore not a tile grid — it is a new interaction:** drag within a zone,
clamped to the zone rect. The clamp already exists and is shared with the server
(`AFLStickerBounds::Clamp`, and `ServerSetStickerPlacement` re-clamps on arrival), so the UI drag is a
*courtesy* to the player and cannot be the authority. That is the one interaction in the whole
programme with no UI at all today.

The creator widget has **no sticker surface whatsoever** — no zone, placement or drag API. Its only
drag today is the hue arc.

---

## 5 · SAVE, NAME, EQUIP — the server side is COMPLETE

| Need | Exists |
|---|---|
| Save to a slot | `ServerSaveBuild(FAFLCreatorBuild, int32 Index)` ✅ |
| Name validation | `ValidateBuildName(...) → EAFLNameVerdict` ✅ |
| Moderation report | `ServerReportBuildName(int32)` ✅ |
| Equip a build | `ServerSetActiveBuild(int32)` ✅ |
| CC-5.4 naming gate | `EAFLNameState{Pending/Approved/Rejected}` + `IsNameShowableToOthers()` ✅ |
| Slot counter | 4 `Creator|Slots` accessors on the widget ✅ |

**Nothing needs building here.** This is UI binding only.

---

## 6 · WHAT I NEED BEFORE AUTHORING

1. **The layout direction: 1a, 1b, 1c or 1d.** Blocks the whole visual layer. Not mine to choose.
2. **Motion token set** (durations + easing), or a ruling to ship without motion for now.
3. **Entry point flow:** loadout only, or loadout + main menu.
4. **Emblem:** is it a creator axis at all? It has no loadout axis today, and the block lists it as
   one that must be reachable.

Not blocking, and I will proceed on these unless told otherwise: the sticker drag is built against
the existing shared clamp; the Builds surface goes in the loadout; save/name/equip binds the existing
RPCs.

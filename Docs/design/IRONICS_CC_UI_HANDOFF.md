# IRONICS — STORE · LOADOUT · CREATOR · UI DESIGN HANDOFF

> ## REVISION R3 — 2026-08-23
> **The flow changed and the catalog shrank. R3 supersedes R2 on both; everything else in R2 stands.**
>
> Two things Design must read before anything else:
>
> **1 · THE CREATOR IS THE PRIMARY SURFACE.** It was reachable only through the Sticker tile — an
> arrangement affordance — which inverted the hierarchy: a player builds a robot in the creator, and
> stickers are ONE AXIS INSIDE it. The creator now opens **on a build**, new or existing. The
> Sticker/Accessory tiles remain as **shortcuts**, valid but secondary.
>
> **2 · THE LOADOUT IS OWNED-ONLY.** It shows saved builds, the six free identities, and owned assets.
> **Nothing unowned appears.** It is an inventory and equip surface; the store is where unowned things
> live. This deletes the "empty axis" problem rather than solving it: an axis with nothing owned shows
> nothing *because the player owns nothing*, which is honest rather than broken.
>
> **THE SPLIT THAT DECIDES WHAT LIVES WHERE (ruled):** the **loadout EQUIPS**, the **creator AMENDS**.
> Anything needing ARRANGEMENT — sticker placement, colour channels — opens the creator. One placement
> implementation, not two.

---

## R3.1 · THE CATALOG IS NOW 319 ROWS

126 retired: discrete colour SKUs, because the creator's continuum replaces **palette**. It does not
replace a **brand**, so anything an identity wears stayed.

**What a player can actually own, measured in the shipping path — not estimated:**

| Axis | catalog rows | dev account owns | shape |
|---|---:|---:|---|
| Weapon | 127 | 77 | the only browsable-length list |
| Edge | 42 | 32 | swatch grid |
| Facemask | 38 | 17 | grid |
| Identity | 13 | **13 — all of them** | a fixed roster, not a list |
| Body (Finish) | 11 | 7 | the free base |
| Beam | 6 | — | 5 free + one test fixture |
| Emblem | 6 | 2 | a handful |
| Sticker | 15 | — | arrangement → creator |
| **WeaponSkin** | **0** | **0** | axis retired wholesale |
| **Accessory** | **0** | **0** | never had any rows |

**Three consequences Design has to design around:**

* **Only ONE axis has a list.** Four of the eight are handfuls. A uniform tab-per-axis row gives
  Emblem's two items the same furniture as Weapon's 77.
* **Identity is not an axis.** All 13 are owned by everyone, so it never filters and never grows. It
  belongs with the six free identities as a **roster**.
* **Two axes must not appear at all.** WeaponSkin returns 0 because its rows are gone; Accessory never
  had any. A tab that promises a category the game cannot fill is the same defect as the store's
  phantom web categories.

---

## R3.2 · WHAT DESIGN ANSWERS — 1a–1d, RESTATED FOR THE NEW FLOW

**1a · THE LANDING STATE.** With no axis chosen, what does the creator open on?
**The engineering read, offered as input and not as an answer: the BUILD AS A WHOLE — preview plus rail,
nothing focused.** Focusing a channel implies a decision the player has not made, and that is precisely
the bug the old `BodyColor` default embodied. Design confirms or replaces this.

**1b · WHERE New Build AND Edit Build SIT** in a surface of builds + roster + owned assets. Both exist
in code: `BeginNewBuild()` and `LoadBuild(Index)`. **Editing loads the build into the rail** — that is
built and no longer hypothetical.

**1c · THE WORKBENCH LAYOUT.** Chassis picker, channel rail, hue arc, slot counter, build name, commit
bar, and an **always-visible exit**. See the non-negotiable below.

**1d · THE CARD AND THE EMPTY STATES**, per the shipping conditions in §R3.3.

> ### ⚠ THE NON-NEGOTIABLE IN 1c
> The creator shipped with no close control, no back handler and no focus target, and it **trapped a
> live player twice**. The code now guarantees a focus target and a CloseButton, and Escape is verified
> working. **Any layout must keep an always-visible exit.** Not a style preference.

---

## R3.3 · SHIPPING CONDITIONS — design the real form, not an ideal plus a fallback

**These are what the product IS.** Measured over all 319 rows.

1. **The subtitle is ONE part.** `SeriesName` is empty on **every row**. Category derives from `Type`;
   series has no data anywhere. Design the one-part subtitle — `Mask`, `Hand cannon`, `Sticker`.
2. **The no-image card IS the card.** Roughly half carry a `ShopThumbnail`. Design the imageless card as
   primary, with the image as enhancement.
3. **COMMON is the resting state.** The overwhelming majority are COMMON. A rarity band tuned for
   LEGENDARY will shout COMMON most of the time.
4. **A new player owns almost nothing.** Six identities, zero builds, near-empty axes. **That surface
   must read as "you own nothing yet, here is how to get things"** — the empty-state rule is now
   load-bearing on the primary screen, not an edge case.
5. **The filter row shows what ships.** Four of the web's seven categories have no catalog type at all.

---

## R3.4 · THE STORE IS A PRODUCT PAGE, NOT A FILTERED GRID

The ruled product list: robot packs · slot SKUs · weapon credits · sticker credits · hand-cannon pairs ·
emblems · League membership · jewellery when the accessory line lands.

**Four of those eight are ENTITLEMENTS that map to no axis** — `CreatorSlot` is literally typed
*"Creator Slot / Robot Pack"*. **Tabs have nothing left to filter.** The store becomes a small set of
product cards, each a purchase.

**The web at `ironics.org/market` is a TEMPLATE — card structure only. The catalog rules every value:**
name, category, price, rarity, description. Where the web and the catalog disagree, the catalog wins
without exception (see R2.4; e.g. stickers are credit-packs and must show **no price** — the catalog
already complies: 15 sticker rows, 0 priced, 0 transactable).

---

## R3.5 · CONSTRAINTS, CARRIED — Design has never seen these screens

| Constraint | Value |
|---|---|
| House palette | Electric `#1E5AFF` lead · Arc-Violet `#A855F7` accent · Black `#05080F` depth · White text |
| **Blend rule (LAW)** | Electric CORE + Arc-Violet RIM; violet never touches core, fill or readable text. Size-gated: rim ON ≥64px, core-dominant ≤32px |
| Glass | Bg `.12 / .08 / .05` · Border `rgba(255,255,255,.20)` → Electric when active · **BackgroundBlur strength 24.0** (the store's real mechanism) |
| Radii | panel **16–24** · button 12 · input 8 |
| Typography | **Bound, never typed** — `TS_IRONICS_*` from `AFLTokenCompiler`. Display=Orbitron, Text=NotoSans, Data=DroidSansMono |
| Focus | Arc-Violet 2px outline, 2px offset. Hover and focus visually identical |
| Rail ground | `GlossBlackRail` **(0.0196, 0.0314, 0.0588, 0.93)** |
| Chrome is furniture | Currency marks use house tokens only; identity resolves by tag |

**⚠ Two things the tokens do not carry.** The compiled styles hold face and size only — the `.70/.45`
opacity tiers come from the glass tokens and must be applied per widget. And **the ruled 16–24 panel
radius is realised NOWHERE, the store included** — adopting it retrofits the shipped store too, so the
store is *not* the existing target on geometry.

**⚠ A stated exception now exists.** The operator ruled violet piping on creator text. The blend rule
forbids violet on readable text at that size; the ruling overrides it. Recorded here and in the style
SSOT so neither document describes a law the build contradicts.

---

> ## REVISION R2 — 2026-08-22
> **(R2, superseded on flow and catalog size by R3 above.)** Scope widened from the creator alone to all three front-end surfaces. One card pattern and one
> token set serve the store, the loadout and the creator, so specifying them apart guarantees drift.
> Everything below the revision block is the original creator spec (R1) and still stands except where
> this block corrects it.
>
> **DESIGN HAS NEVER SEEN THESE SCREENS.** §R2.1 is what actually ships today, warts included.

---

## R2.1 · WHAT SHIPS TODAY (so nothing is designed against a guess)

| Surface | State |
|---|---|
| **Store** (`AFLW_Menu_CosmeticShop`) | Full layout. Six category tabs, showroom, detail panel, dual-currency buttons, footer bar. **Its detail panel was authored with a complete fake product** — that is now replaced with explicit empty state (§R2.5). |
| **Loadout** (`WBP_AFL_Loadout`) | Rebuilt 2026-08-22 to a five-region grid: header 64px · axis tabs 48px · stage \| detail 420px · commit 72px. Ten axis tabs, one rail, one detail column, one EQUIP. |
| **Creator** (`WBP_AFL_Creator`) | Greybox only: `A_ChassisPicker`, `ChannelRailContainer`, `D_SlotCounter`, `E_BuildName`, `F_CommitBar` (Save/Revert), `CloseButton`. **Never rendered on screen.** Behaviour layer complete in C++. |

**One showroom mechanism, now uniform.** The store dissolves its backdrop to alpha 0 and lets the live
armory and the real display pawn render behind the glass. The loadout was conformed to it, and the
creator's `B_PreviewImage` was deleted. **There is no render target anywhere in the front end.**
`UAFLW_LoadoutBase` owns the display pawn; the creator resolves against it and spawns nothing.

---

## R2.2 · CONSTRAINTS — not preferences, and not open to redesign

These are shipped and ruled. Design answers *within* them.

| Constraint | Value | Source |
|---|---|---|
| House palette (4-colour ROLE) | Electric `#1E5AFF` lead · Arc-Violet `#A855F7` accent · Black `#05080F` depth · White text | SSOT §2.1 |
| **The blend rule (LAW)** | Electric CORE + Arc-Violet RIM. Violet never touches core, fill or readable text. Size-gated: rim ON ≥64px, core-dominant ≤32px | SSOT §2.1 |
| Glass | Bg `.12 / .08 / .05` · Border `rgba(255,255,255,.20)` → **Electric when active** · blur **24** (measured off the store's `GlassBlur`) | SSOT §3 |
| Radii | panel **16–24** · button **12** · input **8** | SSOT §3 |
| Typography | **Bound, never typed.** `TS_IRONICS_*` styles emitted by `AFLTokenCompiler`: Display=Orbitron 14, Text=NotoSans 14/13/12, Data=DroidSansMono 13 | SSOT §4 |
| Focus | Arc-Violet, 2px outline, 2px offset. Hover and focus are **visually identical** | Home artifact |
| Rail ground | `GlossBlackRail` **(0.0196, 0.0314, 0.0588, 0.93)** linear | `AFLUITheme.h` |
| Chrome is furniture | Currency marks use **house tokens only**. Identity resolves by tag, never hard-coded per surface | §10.2 |

**⚠ The opacity tiers are NOT in the compiler.** `Text_Secondary` and `Text_Tertiary` are white at full
alpha and differ from Primary by **size only**. The `.70 / .45` tiers come from the glass tokens and
must be applied per widget.

### The palette question, answered

The web's `#07080B` and the SSOT's `UI.House.Black` `#05080F` are the **same job** (page ground) two
points apart — not a divergence. The `#222A3A` seen in-game is neither: converted to sRGB,
`GlossBlackRail` (0.0196, 0.0314, 0.0588) lands at **≈ `#263245`**, which is what that reading is. It is
the **rail ground**, a different token doing a different job from the page ground. **The palettes have
not diverged.**

---

## R2.3 · THE CARD — from `ironics.org/market`

> ### THE WEB IS A TEMPLATE. CARD STRUCTURE ONLY.
> **The game and its assets rule completely.** Every value, category, price, rarity and description comes
> from the catalog. Take the *shape* of the card from the web and nothing else — no name, no price, no
> rarity, no series, no category on that page is authoritative. Where §R2.4 lists a conflict, the
> catalog wins without exception.

```
┌──────────────────────────────┐
│ LEGENDARY                    │  rarity band, above the name
│ Dragon Soul                  │  name
│ Mask · Red.Dragon            │  subtitle: category · series
│                              │
│ 4,200 Volts    180 Watts     │  Volts ALWAYS · Watts CONDITIONAL
└──────────────────────────────┘
```

**The subtitle closes a real gap.** The shipped store shows a name with nothing saying what kind of
thing it is.

**Granted state**, for sponsor content: `Granted · not for sale`, naming the sponsor. No price, no
currency row. This is a *state of the card*, not a price of zero.

### Can the catalog render this? Measured, n=445

| Card element | Catalog support | Fill |
|---|---|---|
| Name | `DisplayName` | **445 / 445** ✅ |
| Rarity band | `RarityTag` | **445 / 445** ✅ |
| **Category** (subtitle left) | **No field.** Must be **DERIVED from `Type`** | — ⚠ |
| **Series** (subtitle right) | `SeriesName` **exists and is empty on every row** | **0 / 445** ⚠ |
| Volts price | `PriceVolts` | 254 |
| Watts price | `PriceWatts` | 166 (88 rows are Volts-only) |
| Thumbnail | `ShopThumbnail` | **214 / 445 (48%)** ⚠ |

### SHIPPING CONDITIONS — not gaps, not fallbacks

**These are what the product IS. Design the shipping form directly; do not design an ideal and a
degraded variant, because the "degraded" variant is the one that ships.**

1. **The subtitle is ONE part.** `SeriesName` is empty on **all 445 rows**. The category half is derived
   from `Type`; the series half has no data anywhere. **Design the one-part subtitle** — `Mask`,
   `Hand cannon`, `Sticker` — not a two-part subtitle with a fallback. If series is ever authored it is a
   later addition to a working card, not a rescue of a broken one.
2. **The no-image card is the card.** Only **214 of 445 (48%)** carry a `ShopThumbnail`. The majority
   state has no image. **Design that as the primary card**, with the image as the enhancement.
3. **COMMON is the resting state.** **366 of 445** are COMMON. A rarity band tuned for LEGENDARY will
   shout COMMON eight times in ten. Design COMMON as quiet-by-default and let the 79 non-COMMON rows earn
   the emphasis.
4. **`Accessory` has ZERO rows.** The axis exists in code and returns nothing. **The tab must not promise
   content that does not exist** — either it is absent from the row or it carries an explicit empty state.
5. **The filter row shows what ships.** Four of the web's seven categories — drones, liveries, utilities
   (beyond a single `ABILITY_COSMETIC` row), and the "sticker pack" concept — **have no catalog type at
   all**. A filter row copied from the web would promise four categories the game cannot fill.

---

## R2.4 · RECONCILIATION — the web shows fiction the game does not ship

**The catalog is authoritative. The web is illustrative.** This list is what to correct, not to copy.

| Web shows | Ships as | Note |
|---|---|---|
| **1776 Pack** — one card, "Five colourways", 2,400 Volts | **Five individual stickers, one credit each** | RULED. The pack does not exist as a purchasable. |
| **Sticker prices in direct Volts** (ROR 9,400 V) | **Credit-packs only** | RULED. **Catalog already complies:** 15 sticker rows, **0 priced, 0 transactable**. Stickers must show **no price**. |
| **Acid Wyrm** LEGENDARY 14,000 V | — | Fiction. |
| **Simularent Livery** `Granted · not for sale` | **Correct** — Simularent is a sponsor and free | The granted state is right; the priced cards around it are not. |
| Prices throughout | Catalog values | Same ruling as the wireframe's "+5 SLOTS 400 VOLTS". |

### Which web categories have an axis

| Web category | Catalog type | Loadout axis |
|---|---|---|
| Masks | `FACEMASK` (38) | ✅ Facemask |
| Hand cannons / Rifles | `WEAPON` (177) | ✅ Weapon + Weapon Skin |
| Stickers | `STICKER` (15) | ✅ Sticker → opens the creator |
| **Drones** (Hellfang FPV) | **none** | ❌ no type, no axis |
| **Utilities** (EMP Grenade) | `ABILITY_COSMETIC` (**1**) | ❌ no axis |
| **Liveries** (Simularent) | **none** | ❌ no type, no axis |

**Four of seven web categories have no shipping representation.** Design should not build a filter row
that promises them.

**And one shipping axis has no content: `Accessory` — 0 catalog rows.** The tab exists in code and
returns nothing.

---

## R2.5 · EMPTY STATE — mandatory, and the reason it is mandatory

The store's detail panel shipped authored with `NEON BLUE EDGE / IRONICS SERIES / LEGENDARY / "Cut
through the darkness."` **A placeholder that reads like data hid an empty catalog through every
screenshot in this programme** — including a full scope pass that concluded the surfaces were populated.

Replaced with `No item selected` / `—` / `Select an item to see its details.` / `BUY  —`.

**Rule for Design: every empty state must be unmistakable for content.** Not a plausible product, not a
grey rectangle that reads as a loading image. If a surface can be empty, specify what empty looks like.

---

## R2.6 · GEOMETRY — the finding that decides what is buildable

**A plain UMG `Border` cannot round its corners.** Rounding requires the brush's `DrawAs =
RoundedBox` plus `OutlineSettings` (`CornerRadii`, `Width`, `RoundingType`).

**Measured across all three surfaces, and it corrects an earlier report of mine:**

| | RoundedBox | OutlineSettings | BackgroundBlur |
|---|---|---|---|
| Store | ✅ 12 widgets | ✅ | ✅ `GlassBlur` strength **24.0**, applyAlpha true |
| Loadout | ✅ *(conformed 2026-08-22)* | ✅ | ❌ **owed** |
| Creator | ❌ | ❌ | ❌ |

**The ruled 16–24 panel radius is realised NOWHERE — the store included.** Every large panel in the
store reports radii `(0,0,0,0)` and outline width `0`. Only two wallet pills (`HALF_HEIGHT_RADIUS`,
outline 1.5) and ten stat segments (`FIXED_RADIUS`, radii 3) are rounded at all.

> ### ⚠ THE STORE IS NOT THE EXISTING TARGET
> **The ruled radii are realised NOWHERE — the store included.** Do not treat the store as the standard
> the other two surfaces catch up to. On geometry it is as far from spec as they are.
>
> **Adopting the ruled 16–24 panel radius is a change to the STORE as well.** Design should specify the
> radius it wants for all three surfaces and understand that it retrofits the shipped screen too. The
> loadout's detail column already carries the ruled 20px, so the three surfaces are currently
> inconsistent with each other *and* with the SSOT.
>
> `BackgroundBlur` at **strength 24.0** is the store's real glass mechanism and is the one piece worth
> conforming to as-is.

---

## R2.7 · WHAT DESIGN ANSWERS — 1a–1d

R1 §2 left the creator's layout `UNSPECIFIED` and asked for a ruling. **That question now goes to
Design, not back to the operator.** The three-column workbench (1c) is ruled; what is open is its
execution, and the same answers must serve the store and loadout.

- **1a · Grid and proportions.** Region widths, gutters, column ratios across all three. The loadout
  currently uses stage-fill \| 420px detail, taken from the lobby. Confirm or replace — **for all three
  at once.**
- **1b · The card, resolved.** The R2.3 card with series absent, no thumbnail, and rarity COMMON — the
  majority states, not the showcase one.
- **1c · The creator workbench.** Chassis picker, channel rail, hue arc, slot counter, build name,
  commit bar, and an **always-visible exit**. See the warning below.
- **1d · Empty, loading and disabled.** Per R2.5, for every region that can be empty.

**Motion (R1 §6) is UNSPECIFIED across the board and stays with Design.** The Home artifact ships a
complete motion spec — breathe 7.5s ±5px counter-phased, hover lift −12px scale 1.012, entry stagger
150/300/450/600ms, `cubic-bezier(.16,1,.3,1)`. **Reuse it rather than inventing a second vocabulary.**

> ### ⚠ THE ONE NON-NEGOTIABLE IN 1c
> The shipped creator had **no close control, no back handler and no focus target**, and it **trapped a
> live player twice** — Escape reached the game viewport and nothing on screen dismissed it. The code
> now guarantees a focus target and a `CloseButton`. **Any layout must keep an always-visible exit.**
> This is not a style preference.

---

## R2.8 · NAMING — with the operator, not with Design

Three classes of divergence, all one line each at the axis enum, which is the single source the tabs
read from:

| Concept | Store (authored) | Loadout enum | Web |
|---|---|---|---|
| Weapon skins | `CAMOS` | `Weapon Skin` | — |
| Facemasks | `VISORS` | `Facemask` | `Mask` |
| Weapons | `WEAPONS` | `Weapon` | `Hand cannon` / `Rifle` |
| Character skins | `SKINS` | *(spans `Body`, `Edge`, `Identity`)* | — |
| Bundles | `BUNDLES` | *(no axis)* | — |
| — | `HELMETS`, `EMOTES` | *(no axis)* | — |

### The store tab set, corrected

I previously reported the store's tabs as "stale on both sides". **That was wrong.** The relabel is
deliberate and documented in `EnterStoreMode`: the WBP widget *names* are historical and deliberately
kept, and the *captions* are reassigned in C++. The rendered labels are the truth.

| Widget (historical) | Rendered caption | Catalog namespaces matched |
|---|---|---|
| `Tab_WEAPONS` | WEAPONS | `AFL.Weapon.` + `AFL.Ability.` |
| `Tab_SKINS` | SKINS | `AFL.Finish.` + **`AFL.Body.`** + `AFL.Edge.` |
| `Tab_HELMETS` | **CAMOS** | `AFL.WeaponSkin.` |
| `Tab_VISORS` | VISORS | `AFL.Facemask.` |
| `Tab_EMOTES` | **BEAMS** | `AFL.Beam.` |
| `Tab_BUNDLES` | BUNDLES | `AFL.Bundle.` |

Only the **authored WBP caption text** was stale (it still read `HELMETS` / `EMOTES` before C++ overwrote
it). The naming decision to rule is therefore between the **rendered store captions** and the **loadout
axis enum**, plus plural-vs-singular, plus the store's `SKINS` spanning three loadout axes and `BUNDLES`
having none.

---

**Status:** spec, not an implementation. The widget is the UI lane's.
**Authored:** 2026-08-20 · **Lane split:** behaviour + interface = Claude Code lane (shipped, see §9); widget assets, layout and visual authoring = UI lane.

> **HOW TO READ THIS.** Every colour, face, size, radius and motion value below is a **token reference**, never a
> literal. Tokens resolve through `IRONICS_UI_STYLE_SSOT.md` and are emitted by `AFLTokenCompiler`
> (`Source/LyraEditor/AFL/AFLTokenCompiler.cpp`), which writes face/typeface/size into every `TS_IRONICS_*`
> style so the ramp cannot drift by hand-edit. Where this spec says **UNSPECIFIED**, the SSOT does not answer it
> and the item names what would settle it. Do not invent a visual language to fill a gap — an invented value
> becomes a shipped decision nobody made.

---

## 1 · OVERVIEW

### What the creator does

The player builds a **robot identity** by choosing colour on the channels their chosen chassis actually renders,
sees it on a live preview they can rotate, names it, and saves it to a slot. That saved build is what spawns.

### Where it sits in the flow

Front-end, out of match. `IRONICS_CHARACTER_CREATOR_SSOT.md` §5.2 places change-timing outside the match
boundary; the selection lock (`IsSelectionEditable`) is the gate. The creator is reached from the loadout
surface and shares its preview rig. **CORRECTED IN R2:** `UAFLW_LoadoutBase` owns the display pawn, but
the `SceneCapture2D` and render target are **retired** — all three surfaces now dissolve their backdrop to
alpha 0 and show the live armory pawn directly, the way the store always did.

### What the player is actually deciding

Three things, in this order of consequence:

1. **Chassis** — which body. This decides *how many channels exist* (§7.6). It is not a cosmetic pick; it
   changes the tool.
2. **Colour per channel** — the product. Clamped to the neon gamut, so the decision is *hue*, and saturation
   and value are constrained (§7.3).
3. **Slot** — which of their saved builds this becomes. Slots are counted and capped (§7.7).

**The preview is the product** (`CREATOR_SSOT` §5.3). What the player sees is what spawns. This is architecturally
guaranteed, not merely intended: the preview resolves colour through the same `BuildColorOverride` →
`SetColorOverride` path the gameplay pawn uses on possession (§9). Any UI that renders colour by a second route
would reintroduce exactly the divergence that rule forbids.

---

## 2 · LAYOUT

### Shell

`UIPanel.Glass` base. Full-screen activatable widget over the front-end backdrop. Corner radius token
`--r-panel`; blur `Glass.Blur`; fill `Glass.Bg.Primary`; edge `Glass.Border`.

### Regions and ordering

| # | Region | Anchoring | Notes |
|---|---|---|---|
| A | **Chassis picker** | top rail, full width | Ordered first because it determines the channel rail's contents. |
| B | **Preview viewport** | centre, dominant | The `PreviewRT` render target. Largest element by area — it is the product. |
| C | **Channel rail** | side rail, vertical list | Length **varies by chassis** (§7.6). Must not imply a fixed set. |
| D | **Slot counter** | header or footer, persistent | Always visible; the cap is a live constraint, not a save-time surprise. |
| E | **Build name field** | adjacent to slot counter | Moderation state changes what strangers see (§7.8). |
| F | **Commit bar** | footer | Save / revert. `CREATOR_SSOT` §5.3: every choice reversible without loss. |

**Reading order for gamepad focus** is A → C → B(rotate) → E → F. Rationale in §8.

**UNSPECIFIED:** exact grid, gutters, and region proportions. `IRONICS_UI_STYLE_SSOT.md` specifies panel
geometry and the token set but no creator layout grid. **Settled by:** a mock at 1280×720 (the canvas the
compiler and existing WBPs author against, §4 of the SSOT), reviewed against the lobby's horizontal budget —
which overflowed 1280px once already with a wide display face.

---

## 3 · TOKENS

Every value the creator uses. **Source column is the authority; this table is a reference, not a redefinition.**

| Token | SSOT source | Usage in the creator |
|---|---|---|
| `UI.House.Electric` | §2.1 PRIMARY | Active/selected channel row, commit-button fill, active panel border |
| `UI.House.Violet` | §2.1 ACCENT | Focus and hover rim only. **Never** a channel swatch, never readable text |
| `UI.House.Black` | §2.1 DEPTH | Panel backing behind swatches so neon reads |
| `UI.House.White` | §2.1 TEXT | Labels, hex readout, channel names |
| `UIPanel.Glass` | §6 | Shell, channel rail, chassis picker |
| `UIButton.Glass` | §6 | Commit bar, unlink toggle, chassis tiles |
| `UIChip.Value` | §6 | Slot counter (`n / cap`), mono |
| `Glass.Bg.Primary/Secondary/Tertiary` | §3 | Shell / nested rail / dividers |
| `Glass.Border` | §3 | Panel edge; overridden to House Electric when active |
| `Glass.Tint.Danger` | §3 | At-cap slot state, error states — a **wash**, never a solid |
| `Glass.Shadow` | §3 | Z-separation of rail over viewport |
| `Text.Primary/Secondary/Tertiary` | §3 | Channel label / reason text / disabled label |
| **Display** — Orbitron | §4 | Screen title, chassis names. ALL-CAPS, identity-carrying text only |
| **Body** — Noto Sans | §4 | Channel labels, disabled reasons, helper copy. Sentence case |
| **Numeric** — Droid Sans Mono | §4 | **Hex/value readout, slot counter.** Mandatory: tabular numerals so digits do not jitter |
| `--r-panel` / `--r-button` / `--r-input` | compiler geometry | Shell / buttons / name field |
| `--blur` | compiler geometry | Glass blur radius |

**`UIDisplay.NeonTube` is NOT used in the creator.** It is size-gated display-only ≥64px (§6) and remains
unapproved (SSOT OPEN ITEM 5). The creator's headings use flat Display type on glass.

**Deliberately absent:** the neon registry hues (§2.2 — NeonBlue, NeonPink, etc.). Those are *player identity*
colours and appear inside swatches as **player data**, never as creator chrome. Chrome is house palette only.

---

## 4 · COMPONENTS

| Element | Base class | Variants |
|---|---|---|
| Creator screen | `UCommonActivatableWidget` (Lyra front-end pattern) | — |
| Preview viewport | `UImage` bound to `PreviewRT` (existing, `UAFLW_LoadoutBase::SetupPreviewCapture`) | — |
| Chassis tile | `UCommonButtonBase` | selected / unselected |
| **Channel row** | `UCommonUserWidget` | **Connected · PresentButInert · Absent** (§5, §7.1) |
| Hue arc | `UCommonUserWidget` + custom paint or `USlider` | enabled / disabled / unset |
| Swatch | `UImage` + `UTextBlock` readout | has-value / unset |
| Value readout | `UTextBlock`, mono | **always present** (§8 — this is the CVD requirement) |
| Link toggle | `UCommonButtonBase` (toggle) | linked / unlinked |
| Slot counter | `UIChip.Value` | under-cap / at-cap / upgrade-available |
| Name field | `UEditableTextBox` | editing / pending / approved / rejected (§7.8) |
| Commit bar | `UCommonButtonBase` ×2 | save enabled/disabled, revert enabled/disabled |

**Architecture:** C++ base owns bindings, WBP child owns layout — the proven AFL split (SSOT §6 pipeline).
The creator base binds to §9's interface; the WBP authors appearance.

---

## 5 · STATES AND INTERACTIONS

Every element, every state. Where a state is not reachable for an element it says so rather than being omitted.

### 5.1 Channel row — the load-bearing one

| State | Trigger | Treatment |
|---|---|---|
| **Default (Connected)** | `BodyState/EdgeState/… == Connected` | Full opacity. Label `Text.Primary`. Swatch shows colour + hex readout. Arc interactive |
| **Focused** | gamepad/mouse focus | `UI.House.Violet` outer rim (§2.1 state layer). Focus ring must be visible at TV distance (§8) |
| **Hovered** | mouse only | Violet rim at lower intensity than focus. Gamepad has no hover — do not rely on it |
| **Pressed** | drag on arc | Swatch and readout update **live**, per-frame. `bCaptureEveryFrame` is already true |
| **Disabled — PresentButInert** | state `== PresentButInert` | **Shown, dimmed to `Text.Tertiary`, arc non-interactive, REASON displayed.** See §7.1 |
| **Disabled — Absent** | state `== Absent` | Shown, dimmed, reason displayed, **visually distinct from PresentButInert** (§7.1) |
| **Loading** | schema not yet resolved (`ResolvedFromMaster == None`) | Skeleton row, no channel names guessed. Fails closed — claims no channels |
| **Empty / unset** | channel has no chosen colour | Swatch shows the unset treatment (§7.4); readout shows `—`, not a fabricated hex |
| **Error** | apply failed | `Glass.Tint.Danger` wash on the row; reason text. **UNSPECIFIED:** whether apply can fail client-side at all — the preview path has no server round-trip, so this may be unreachable. **Settled by:** confirming `CreatorApplyPreview` has no failure mode once the pawn exists |

### 5.2 Hue arc

| State | Treatment |
|---|---|
| Default | Arc renders the **reachable** gamut only (§7.3) |
| Focused | Violet rim on the arc track; handle enlarged for TV legibility |
| Pressed/dragging | Handle follows; swatch + hex update live |
| Disabled | Track at `Text.Tertiary`, handle hidden, no input |
| Unset | Handle absent, track visible — the channel has no hue yet (§7.4) |

### 5.3 Slot counter

Under-cap · at-cap · upgrade-available — see §7.7.

### 5.4 Commit bar

| State | Rule |
|---|---|
| Save enabled | working selection differs from saved **and** a slot is available |
| Save disabled — no change | Dimmed, reason "No changes" |
| Save disabled — at cap | Dimmed, reason names the cap and the upgrade path (§7.7) |
| Revert enabled | working differs from saved |

---

## 6 · MOTION

| Element | Trigger | Animation | Duration | Easing |
|---|---|---|---|---|
| Screen | activate | Panel breathe-in + blur ramp | UNSPECIFIED | UNSPECIFIED |
| Channel row | focus | Violet rim fade-in | UNSPECIFIED | UNSPECIFIED |
| Swatch | colour change | Cross-fade old→new | UNSPECIFIED | UNSPECIFIED |
| Preview pawn | rotate | Follows input 1:1, no smoothing on drag; inertia on release UNSPECIFIED | — | — |
| Slot counter | reaching cap | Single pulse, then rest | UNSPECIFIED | UNSPECIFIED |

**UNSPECIFIED across the board.** `IRONICS_UI_STYLE_SSOT.md` §3 states the *principle* — "motion is meaning;
panels breathe in/out; transitions reveal depth" and "luminous restraint" — but specifies **no durations or
easing curves anywhere**. The only concrete timing in the SSOT is the `UIDisplay.NeonTube` hum (two opacity
dips over ~5.2s on `steps(1)`), which the creator does not use.

**Settled by:** a motion pass adding a duration/easing token set to the SSOT (e.g. `Motion.Fast/Base/Slow`
+ named curves), emitted by `AFLTokenCompiler` like the geometry tokens already are. Until then the lane
should not hand-pick durations per widget — that is how a UI ends up with fourteen different fade times.

---

## 7 · EDGE CASES AND CONTENT

### 7.1 The three channel states — the defining edge case

`FAFLCreatorChannelSchema` reports **three** values per channel, not a boolean:

| State | Meaning | Render |
|---|---|---|
| `Connected` | Parameter exists **and** carries signal to an output | Normal, interactive |
| `PresentButInert` | Parameter exists, measured to have **zero downstream consumers on this master** | **DISABLED, VISIBLE, WITH A REASON** |
| `Absent` | Master has no such parameter | Disabled, visible, with a *different* reason |

**Why `PresentButInert` may never be hidden.** Ruled 2026-08-20 (CC-X24). Hiding it makes a missing feature
indistinguishable from a bug — to the player *and* to the next developer. The measured case: on `M_AFL_Character`
(the X-line flagship) `TeamColor` exists but is graph-disconnected, so **body colour is disabled there** while
edge and glow work. A player who sees no body control and no explanation reasonably concludes the creator is
broken.

**The reason string sources from `ResolvedFromMaster`** — the schema carries the master's name precisely so the
UI can say *why*. Suggested copy, sentence case, `Text.Secondary`:

- `PresentButInert` → "Body colour isn't available on this chassis."
- `Absent` → "This chassis has no visor to tint."

**The two disabled states must be visually distinguishable.** They mean different things and have different
futures: `PresentButInert` is restorable by a material change (CC-X25 is logged to do exactly that for the
X-line body channel); `Absent` is not. **UNSPECIFIED:** the distinguishing treatment. **Settled by:** a visual
pass — but note the constraint that it cannot rely on hue alone (§8).

### 7.2 Unaudited masters

`bMasterAudited == false` means the connectivity audit has never covered this master, so a `Connected` verdict
means *"present, and inertness unknown"* — **not** "measured to render."

This must not present identically to an audited-connected channel. The honest read is a caution, not a claim.
**UNSPECIFIED:** treatment. **Settled by:** a ruling on whether unaudited channels are offered at all. Both are
defensible — offer with a caveat marker, or withhold until audited — and it is a product call, not a visual one.

### 7.3 The clamp is visible, never a snap-back

The gamut floors saturation at **0.55** and value at **[0.45, 1.0]** (`AFLCreatorGamut`). Hue is the free axis.

**The arc must only expose reachable colour.** The player must never drag into a colour they cannot have and be
jerked back — a snap-back reads as the control fighting them, and it teaches that the tool is unreliable.

Implementation guidance: the arc's track renders colours produced by `AFLCreatorGamut::FromHue()`, so **the track
itself is the gamut**. There is nowhere out-of-gamut to drag *to*. Saturation and value are not player-facing
axes at all — do not ship sliders for them and then clamp the result.

**Why hue-only is honest rather than limiting** (`CREATOR_SSOT` §6.3): the clamp is a **gameplay** requirement —
near-black and near-white builds degrade match readability for every other player. The arc offers exactly the
freedom that survives the clamp.

### 7.4 Achromatic start

Dragging hue from grey, black or white is a real player path — a fresh channel or one set to a neutral. This is
handled correctly in the behaviour layer as of CC-5.2 (a bug that returned red for *every* hue from an achromatic
start was found and fixed by `afl.Creator.ArcProbe`).

**What the swatch shows before a hue is chosen:** the channel's current colour, whatever it is, plus a readout of
`—` rather than a fabricated hex. When the player first touches the arc, saturation lifts to the floor at their
chosen hue — this is a visible, expected transition, not a glitch. **Do not** pre-seed the handle at hue 0; that
implies red was chosen when nothing was.

### 7.5 Link model

`FAFLCreatorChannelLinks` defaults to **fully unlinked** (`LinkedMask == 0`). Spec the per-channel toggle.

**The default pairing is UNSPECIFIED and must stay that way until ruled.** The roadmap specifies "Neon and Edge
linked by default," which **does not map** onto the shipped channel set: measurement established `NeonColor` is
not a creator channel at all, and body is disabled on the X-line — leaving Edge and Glow as the two channels
there. **Settled by:** a product ruling on which channels move together. Until then the toggle exists, ships
off, and the UI must not present a pairing as a default.

### 7.6 Channel count varies by chassis

**Two** on the X-line (`M_AFL_Character`: edge, glow — body disabled). **Three** on Manny-based masters
(`M_Mannequin`: body, edge, glow). Visor is `Absent` on both measured masters.

**The rail must not imply a fixed set.** No fixed-height four-row layout with two rows greyed as if temporarily
unavailable. The rail is data-driven from the schema, and its length is a property of the chassis. Changing
chassis changes the rail — that is correct and should read as informative, not as an error.

### 7.7 Slot counter

Ladder (`IRONICS_PRICING_SSOT` §5.1): **2 free · 5 League · 10 hard cap.** `MaxUpgrade ? 10 : clamp(Baseline +
Purchased, Baseline, TierCeiling)`.

| State | Treatment |
|---|---|
| Under cap | `UIChip.Value`, mono, `n / cap` |
| At cap | `Glass.Tint.Danger` wash (a wash, never a solid) + the upgrade path named |
| Upgrade available | Route to the store surface. **No paywall inside the creation flow** (`character-system.md`): the creator shows what the player owns with a single route out — it does not grey desirable options mid-authoring |

**Currency spelling is LAW** (§5): **Watts**, **Volts**. Integer only. Never show USD.

### 7.8 Build naming

CC-5.4 gates public names by moderation state (`EAFLNameState`).

| Viewer | Pending | Approved | Rejected |
|---|---|---|---|
| **Owner** | Their own name, with a pending indicator | Their own name | Their own name + rejected indicator + edit route |
| **Stranger** | **"Unnamed Robot"** | The name | **"Unnamed Robot"** |

The owner **always** sees their own text — never replaced, never hidden from them. The gate is a *visibility*
rule for other players, not censorship of the author's own view. Fails closed: anything not `Approved` shows the
placeholder to strangers.

---

## 8 · ACCESSIBILITY

### 8.1 Colour-vision deficiency — non-negotiable, and specific to this product

Roughly **8% of male players** cannot reliably distinguish channels presented by hue alone. **This product is
colour**, so this is the one accessibility item that cannot be deferred.

**Requirements:**

1. **Every channel is identifiable by label, position and value readout — never by its colour.** The channel
   rail is a labelled list in stable order, not a row of coloured dots.
2. **A numeric readout is always present** beside every swatch, in **Droid Sans Mono** (§4 — tabular numerals).
   Format **UNSPECIFIED** between sRGB hex (`#1E5AFF`) and HSV (`H 226° S 0.55 V 0.90`). **Settled by:** a
   ruling — hex is universally recognised and copy-pasteable; HSV names the axis the arc actually manipulates
   and would make the clamp legible ("S is at the 0.55 floor"). Recommend HSV *plus* hex if space allows.
3. **No interaction may require distinguishing two similar hues.** Nothing is confirmed by "pick the one that
   matches"; nothing is disambiguated by swatch colour alone.
4. **Disabled-state differentiation must not rely on hue** (§7.1) — opacity, iconography, position or explicit
   text, since the two disabled reasons must be told apart by players who cannot compare tints.

### 8.2 Gamepad

- **Gamepad-only completability is required.** Every action reachable: chassis select, channel focus, hue
  adjust, link toggle, rotate, name entry, save.
- **Focus order:** A → C → B → E → F (§2). The channel rail is a vertical D-pad/stick list; up/down moves rows,
  left/right adjusts the focused channel's hue. Rotation binds to the right stick while the viewport has focus.
- **Back/cancel semantics:** back = revert-to-saved *if changes exist* (with confirmation), else close. It must
  never silently discard work — `CREATOR_SSOT` §5.3 requires every choice reversible without loss.
- **Focus takes on activation:** the chassis picker (region A), because it determines everything downstream.

### 8.3 TV distance and console

- **Focus visibility at TV distance:** the violet rim alone is insufficient at 10 feet. Focus needs a
  size/scale or border-weight change in addition to the colour state layer.
- **Console safe zones (TRC):** all interactive elements and text inside title-safe. **UNSPECIFIED:** the safe-zone
  inset value. **Settled by:** the platform TRC requirement per target; it is a certification input, not a design
  choice.
- **Text scale:** body type must survive a scale increase without clipping. The rail is a list, so it grows
  vertically and scrolls — do not fix its height.
- **Touch (mobile, B4 target):** minimum touch target UNSPECIFIED; the arc needs a larger hit region than its
  visual stroke. **Settled by:** the platform's minimum-target guidance.

---

## 9 · THE INTERFACE CONTRACT

**Shipped and callable now.** The widget calls these; it does not reimplement them.

### From `UAFLW_LoadoutBase` (CC-5.3)

| Function | Purpose |
|---|---|
| `CreatorSetChannel(EAFLCreatorChannel, FLinearColor)` | Set one channel on the working selection. **Clamps on entry** via the shared gamut — the preview cannot show an uncommittable colour |
| `CreatorApplyPreview()` | Push the working selection to the display pawn **through the shipping resolve path** |
| `CreatorRotatePreview(float DeltaYawDegrees)` | Spin the model. Rotates the **mesh**, not the actor (the capture is attached to the actor) |
| `CreatorGetPreviewYaw()` | Current yaw, degrees |
| `CreatorGetWorkingSelection()` | The uncommitted selection being edited |
| `CreatorGetSchema()` | `FAFLCreatorChannelSchema` for the bound chassis — drives the whole rail |
| `CreatorLinks` | `FAFLCreatorChannelLinks`, read/write |

### From `FAFLCreatorChannelSchema`

`BodyState` · `EdgeState` · `GlowState` · `VisorState` (`EAFLChannelAvailability`) · `bBodyAvailable` etc.
(usable == `Connected`) · `ResolvedFromMaster` (the reason source) · `bMasterAudited` (§7.2) ·
`AvailableCount()`.

### From `AFLCreatorGamut`

`HueOf(FLinearColor) → degrees` · `WithHue(FLinearColor, float) → clamped` · `FromHue(float) → clamped` ·
`ClampToNeon(FLinearColor)` · constants `MinSaturation 0.55`, `MinValue 0.45`, `MaxValue 1.0`.

**One clamp, shared.** These are the same functions the server commits with — single-sourced deliberately in
CC-5.2, because two implementations of one rule drift silently and the player is shown one colour and given
another.

### Not yet built — name the gap rather than assume it

- **Save/commit to a slot** is `UAFLCosmeticLoadoutComponent::ServerSaveBuild` (CC-3, shipped) but the creator
  has no wrapper yet; the widget should not call the server RPC directly.
- **Slot entitlement counting** is **not wired**: the SKUs are priced and purchasable, but buying a slot does
  not yet increment the counted entitlement. Tracked as CC-4.2 slot wiring. **The slot counter will read a
  static value until that lands** — spec it, bind it, expect it to be inert initially.
- **Name moderation state** (`EAFLNameState`) is shipped in CC-5.4; the creator has no wrapper.

---

## 10 · WHAT THIS SPEC DELIBERATELY DOES NOT DECIDE

Recorded so the gaps are visible rather than discovered during authoring:

> **UPDATED 2026-08-20 from the wireframe bundle** (`Ironics Creator Wireframes.dc.html`, five directions
> 1a–1e at 1280×720, built from this spec). Direction **1e is a direction-agnostic state sheet** and settles
> the items struck through below. The **layout direction itself is still open** — 1a right-rail, 1b full-bleed,
> 1c three-column workbench, 1d console-first stepper — that is a choice, not a spec, and it is the operator's.
>
> **One value in the bundle is NOT adopted:** the slot upgrade reads "+5 SLOTS · 400 VOLTS". No SSOT carries
> that price and it contradicts the shipped SKUs (`AFL.CreatorSlot.x1/x3/x8` at 3,000 / 4,990 / 10,000 Volts,
> live in PlayFab as of `cc-6-1-done`). Treated as illustrative. A placeholder price that gets copied is how a
> number nobody ruled becomes shipped.

| Gap | Settled by |
|---|---|
| ~~Layout grid, gutters, region proportions~~ | Wireframes 1a–1d supply four candidate layouts at 1280×720; **which one** remains an operator pick |
| All motion durations and easing | **STILL OPEN.** The wireframes specify no timing values (checked, not assumed). Needs a `Motion.*` token set in the SSOT emitted by `AFLTokenCompiler` |
| ~~`PresentButInert` vs `Absent` distinction~~ | **SETTLED (1e):** `◐` + dashed border + INERT badge vs `✕` + dotted border + ABSENT badge — *"told apart by fill, icon and text, never hue"* |
| ~~Unaudited-master treatment~~ | **SETTLED (1a/1e):** chassis tile carries `unaudited ⚠`; the channel is offered with the caveat visible |
| Link default pairing | Product ruling; roadmap's Neon+Edge does not map |
| ~~Hex vs HSV readout~~ | **SETTLED (1e):** BOTH — `#1E5AFF · H 226° S 0.55 V 0.90` |
| Console safe-zone inset, touch target minimum | Platform TRC / platform guidance |
| Whether preview apply can fail | Confirm `CreatorApplyPreview` has no failure mode post-pawn |

---

## APPENDIX · MEASURED FACTS THIS SPEC RESTS ON

Every claim here was measured in-engine, not inferred. Cited so the lane can re-check rather than trust.

| Fact | Evidence |
|---|---|
| X-line offers **2** channels; Manny-based offers **3** | `DERIVED master=M_AFL_Character body=0 edge=1 glow=1 visor=0 count=2 audited=1` vs `M_Mannequin … count=3` |
| `TeamColor` exists on both masters, inert on one | Same run: `found=1` on both, opposite verdicts — inertness is keyed on the (master, parameter) pair |
| `NeonColor` cannot substitute for body | `AlbedoRecolor` measures `0.0`; `NeonColor`'s only path to `BaseColor` is gated by it |
| The gamut clamp is shared, not duplicated | `MinSaturation = 0.55f` appears exactly once in the codebase |
| The arc never leaves the gamut | `AFL_TEST[ARC] PASS — checked=24 outOfGamut=0 keptSV=1 linksDefaultOff=1` |
| Preview and spawn share one resolve path | `SetColorOverride(BuildColorOverride(*EffSel))` inside `RefreshSkinForPawn`, which both call |

# IRONICS — PRO MOD CHARACTER PHASE: SCOPE & BUILD SSOT

<!-- PRECEDENCE BLOCK -- keep at the top; update the commit line when this doc's CONTENT changes. -->
> **PRECEDENCE** (rule ruled 2026-08-24: newest economy doc wins; where it is silent, the prior doc fills the gap)
> - **This doc's last CONTENT commit:** `86e1b7ae` (2026-08-27) — a directory move is not a content change and must not be cited here.
> - **Authority on pricing:** `IRONICS_PRICING_SSOT.md` — Battle Pass is **$5/month, real money** (ruled 2026-08-27). No Volts, no Watts.
> - **Superseded:** its "two-line fork" naming is clarified by `IRONICS_CHARACTER_CREATOR_SSOT.md` §7 — "X-line" is the LEGACY NAME for Pro Mod, not a third chassis.

**Status:** Guiding-doc SSOT for the Pro Mod character line and creator. Authored 2026-07-31.

**Rule of use:** Every AIK / Claude Code / Operator block on a Pro Mod character item reads
this doc first, then the relevant section is expanded with disk-verified facts at scope-time.
This doc holds the *architecture and the lane assignments*; the *facts* get filled in per item
when that item is scoped to build. Where this doc disagrees with an older doc, this doc wins
for Pro Mod characters only — it changes nothing about the X-line.

**Grounds in:** `IRONICS_GAME_MODES_SSOT.md` · `AFL_PRO_MOD_CHARACTER_BRIEF.md` ·
`IRONICS_TRIMODE_TRACKER.md` · `IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md` ·
`IRONICS_PRICING_SCARCITY_SSOT.md` §1.5 (R2 ladder) · `AFL_ECONOMY_ARCHITECTURE_ADR.md`
(Decisions 3, 5, 7) · `IRONICS_LOADOUT_DESIGN.md` · `IRONICS_HEALTH_CONSUMABLE_SSOT.md` §3
(counted-consumable pattern).

**Core principle (unchanged):** *One system, some matrixes, many parts — everything symbiotic.*
Nothing here is greenfield. Every item extends a proven foundation or is explicitly named as
new engineering with its reason.

---

## 0. LOCKED DECISIONS — not open for re-proposal

These are operator rulings. An agent that finds disk evidence appearing to contradict one of
these **reports the conflict and stops**. It does not resolve the conflict by changing the ruling.

| # | Ruling |
|---|---|
| L1 | **IK_Mannequin is the rig.** Every Pro Mod body and head conforms to it. Do not propose MetaHuman, a variant rig, or a different skeleton. |
| L2 | **Pro Mod never dismembers.** Gore-free is the mode's design, achieved structurally. There is no dismemberment gate to build and no damage sign-off waiting on one. |
| L3 | **Melee / hand-to-hand is CUT.** This is a shooter. Removed from scope entirely, not deferred. |
| L4 | **X-line robots ship ALONGSIDE Pro Mod.** Two character lines. Neither replaces the other. |
| L5 | **Visors and facemasks are X-line only.** Pro Mod humans use masks (geometry) and face tattoos (stickers). The `AFL.Facemask.*` axis is not extended to Pro Mod. |
| L6 | **Preset heads, parametric everything else.** The diversity lineup is a preset roster (B-01…L-03), not face sliders. Parametric variation lives on colour, stickers, accessories, and number. |
| L7 | **Sticker placement is per-zone CAPPED at 9 total.** See §4.2. Unlimited is rejected on both memory and product grounds. |
| L8 | **Character name sits ALONGSIDE the account display name**, it does not replace it. **The player chooses the name.** Internally the ledger is `registered account → characters owned → character names`. |
| L9 | **1 character save free; additional saves $3 each**, as a counted entitlement. |
| L10 | **Character mods DO NOT carry between lines.** A Pro Mod character and an X-line robot are separate characters with separate mods. Shared *palette data*, forked *SKUs* — see §2. |
| L11 | **The 7+base neon set IS the brief's 8-colour axis.** One set, not two. Solar `#FF7300` has tag + Primary/Accent but **no edge-ramp preset** — the only net-new colour asset in the phase. |
| L12 | **Stickers and masks are BOTH sold and granted.** How a player combines a mask with face tattoos is **their preference, not a design constraint we impose** — the system permits it and does not adjudicate taste. |

---

## 1. WHAT IS ALREADY PROVEN (the foundation this extends)

These are committed, disk-verified and PIE-proven. Everything below plugs into them.

| System | State | Where |
|---|---|---|
| Colour / finish axis — 48-colour palette, tag-driven resolution | ✅ PROVEN | `DA_AFL_ColorIdentityRegistry`, `FindToneForParam` |
| Decal mechanism on a character — explicit decal pass + tint mapping | ✅ PROVEN | `ChestEmblemDecal`, commit `7cab99de` |
| Per-player part resolution — a map + selector, not a hardcoded pin | ✅ PROVEN | `UAFLCharacterPartSelectorComponent`, `UAFLCharacterPartMap`, `8e459755` |
| Cosmetic selection seam — replicated FName keys on PlayerState | ✅ PROVEN | `FAFLCosmeticSelection`, `UAFLCosmeticLoadoutComponent` |
| Catalog + entitlement + wallet + purchase→grant→equip | ✅ PROVEN LIVE | `FAFLCatalogEntry`, `UAFLWalletComponent`, `IAFLCosmeticPersistence` |
| One equip rail — all 53 weapon SKUs through `ULyraQuickBarComponent` | ✅ PROVEN | `d24d12e1` |
| Counted-entitlement pattern (quantity on the one registry, not a parallel inventory) | ✅ DESIGNED | `IRONICS_HEALTH_CONSUMABLE_SSOT.md` §3 |
| Pro Mod Experience + Pro pawn (gore-free, faster movement) | ◑ IN FLIGHT | `B_Experience_ProMod`, `B_Hero_BagMan_Pro` |
| FBIK + enhanced character animation/movement | ✅ DONE | operator-confirmed 2026-07-31 |

**The reuse headline:** the **colour registry and the weapon catalog** carry across both lines.
48 authored colours and 147 cannon SKUs are defined once. Character *mods* fork per line
(L10) — but the palette data and the weapons underneath them do not. That is the payoff of
the per-axis-owned, composed-at-loadout model; do not compromise it by re-authoring colour
values per line.

---

## 2. THE TWO-LINE FORK (L4) — recorded so nobody "unifies" it later

> **NAMING RULED 2026-08-27.** The two chassis are **MANNY** (`SKM_Manny` / `M_Mannequin`) and
> **PRO MOD** (`SKM_IRONICS_Blank` / `M_AFL_Character`, FBIK Blank Base). **"X-line" in the tables
> below is the LEGACY NAME FOR PRO MOD** — it is this same chassis, not a third one.
>
> The `AFL.Character.<Name>` / `<Name>_X` pairs in the catalog are **character-era legacy** from when
> the game shipped named characters. The `_X` suffix is a per-character variant marker, NOT a chassis
> selector, and it is contamination to be cleaned up.
>
> This matters because reading "X-line robots vs Pro Mod humans" as two SEPARATE lines produces a
> phantom third chassis. A scoping pass on 2026-08-27 did exactly that and built a picker against the
> `_X` suffix before the naming was reconciled. See `IRONICS_CHARACTER_CREATOR_SSOT.md` §7.


Two character lines ship. **A Pro Mod character and an X-line robot are separate characters
with separate mods (L10).** Owning a robot finish does not grant the equivalent Pro Mod suit
colour, and vice versa.

**The distinction that makes this cheap rather than duplicative:**

| Layer | Shared or forked |
|---|---|
| **Palette DATA** — the colour values themselves | **SHARED.** One `DA_AFL_ColorIdentityRegistry`, one `FindToneForParam`. A colour is authored once. |
| **Entitlement SKUs** — what a player buys | **FORKED.** `AFL.Finish.*` is the robot SKU; the Pro Mod suit colour is its own SKU line. |
| **Geometry-bound axes** — face, chest mark | **FORKED** by construction. |
| **Weapons / skins / beams** | **SHARED.** A weapon is not a character mod. |

| Axis | X-line robots | Pro Mod humans |
|---|---|---|
| **Body colour** | `AFL.Finish.*` | own SKU line, same registry values |
| **Face** | `AFL.Facemask.*` (visors, 60 rows) | `AFL.Mask.*` — geometry on a head socket |
| **Chest mark** | `ChestEmblemDecal` | Sticker composite (§4) |
| **Identity** | 29 grail 1-of-1 Singularity bundles | Creator-authored, player-owned |
| **Weapons** | `AFL.Weapon.*` | **SHARED** |

**Why forked SKUs are the AAA answer, not a monetisation grab:** a suit colour on a human
body is authored art (fabric, panel, flex-zone response), not the same asset as a robot
finish. Charging once for two different authored looks would be the shortcut. Sharing the
*registry* means a colour is still defined in exactly one place — adding a colour is still one
registry row, and both lines can reference it.

**Why the geometry fork is correct:** a visor requires visor geometry; a mask requires a human
head. Decision 5 already gives them separate addresses, and entitlement is per-`CosmeticId`
with no line dimension, so the catalog needs no change to support any of this.

---

## 3. THE SYSTEMS — reusable vs. genuinely new

| Requirement | Classification | Notes |
|---|---|---|
| Neon suits, 7 + base | **Reuse** | Wire to the existing Finish registry. Art largely exists. |
| Style / type choices | **Reuse pattern** | Part map + selector, more rows |
| Sticker *art* | **Reuse rail** | Decal mechanism proven |
| **Sticker placement** | **NEW ENGINEERING** | Transforms, not ids. See §4. |
| **Character name** | **NEW** | Free text → moderation, uniqueness, report path |
| **Save / load character** | **Seam exists, shape is new** | Through `IAFLCosmeticPersistence` |
| **Modular body + head, neck seam** | **NEW** | On the shared IK_Mannequin skeleton |
| **Masks** | **NEW** | Geometry on a head socket — closer to Accessory than to Facemask |
| `AFL.Accessory.*` hardpoints | **NEW** | Axis declared, unbuilt |
| Number / ID | **NEW content, existing rail** | Rides the sticker composite |
| Creator UI | **Extends locker** | `IRONICS_LOADOUT_DESIGN.md` AxisPicker pattern |

### 3.1 The architectural fault line — read this before scoping anything

Everything the cosmetic system does today is **selection**: pick a `CosmeticId`, server
validates entitlement, `FAFLCosmeticSelection` replicates a fixed-size set of FName keys.

The creator introduces **player-authored data**: a name (string) and sticker placements
(transforms). Neither is an id. **This is the only genuinely new data shape in the phase**,
and it is why §4 is the largest engineering item.

> **HARD CONSTRAINT — `AFLNetTypes`.** Any net-serialized struct authored for this phase is
> declared in **`AFLNetTypes` (Runtime, Default phase, NON-GameFeature)**. A GameFeature-module
> struct desyncs `FNetSerializeScriptStructCache` and drops connections. Single-client testing
> **never** surfaces this. Author it there on day one; do not plan to move it later.

---

## 4. STICKER COMPOSITE SYSTEM (CC-4) — the real engineering

### 4.1 Rendering: render-target composite, not live decals

**Ruled: RT composite.** Placed stickers are baked into one texture per character; the
character then wears a **single material**. One draw path regardless of sticker count.

Rationale: N live `UDecalComponent`s × 16 players is a shipping-grade cost, and decals
projected on skeletal meshes bleed across adjacent geometry. The composite also handles face
tattoos on the same rail and makes mask occlusion resolve naturally (mask geometry sits over
a baked texture).

Cost moves from per-frame draw to **memory (one RT per character) and bake time** on spawn
and on loadout change. Both are bounded by §4.2.

### 4.2 The cap (L7)

| Zone | Slots |
|---|---|
| Chest | 2 |
| Stomach | 1 |
| Leg front (L/R) | 1 each |
| Leg back (L/R) | 1 each |
| Backplate | 1 |
| Face | 2 |
| **Total** | **9** |

**What the cap buys, and why it is load-bearing rather than cosmetic:**
- The placement array becomes **fixed-size and small** → replicable directly, no compact-reference
  indirection, no unbounded net traffic.
- One sticker per leg zone → each zone is a **single clamped UV rect**, no overlap resolution,
  no z-ordering between stickers within a zone.
- The RT bake has a **known worst case** at 16 players.

Slot counts are a per-zone product lever (e.g. "+1 chest slot" as a SKU) and may be tuned at
playtest. **The fixed-size-array property must not be traded away** — going unlimited reopens
the replication problem this cap solves.

### 4.3 Data contract

- Placement struct: `{ StickerId (FName), ZoneId, Position2D, Rotation, Scale }`
- Fixed array of 9, **declared in `AFLNetTypes`**
- Server-authoritative: entitlement validated on **placement**, not only on ownership
  (stickers are both sold and granted — L9 / R6 pattern)
- Bake triggers: pawn spawn, loadout change, and any placement commit

### 4.4 Zone UV contract — the authoring work

Each zone is a defined UV region on the suit with clamped placement so a sticker cannot cross
a seam or land on invalid surface. **This is what makes the placement UI feel AAA rather than
fiddly**, and it is suit-authoring work that must land with the base pair (CC-1), not after.

---

## 5. NAME, SAVES, AND PERSISTENCE (CC-5)

**Name (L8):** the **player chooses it**, and it sits alongside the account display name.
~~Requires a profanity filter, a uniqueness rule, and a report path before any player sees
another player's name. Not glamorous; genuinely required.~~

> ⛔ **STRUCK 2026-08-27 BY RULING: the profanity filter, uniqueness rule and report path are NOT NEEDED.** Removed from every open list; do not raise again.


**Identity ledger (L8):** internally the authoritative chain is
`registered account → characters owned → character names`. The account is the durable key;
character names are display-layer and re-nameable. **Never key entitlement, ownership, or a
trade record off a character name** — Decision 3 (a shipped id is never renamed) applies to
the account and the character record, not to the player-authored display string. This is also
what makes the $3 extra-save SKU coherent: slots attach to the account, not to a character.

**Character saves (L9):** 1 free, additional at **$3 = Standard band** (`PRICING_SCARCITY`
§1.5 R2, $2.99–4.99, pay-either). Modelled as a **counted entitlement** — a quantity on the
one registry and persistence seam, exactly the health-pack pattern. **Not** a parallel
inventory, not a new subsystem.

**Save shape:** a character record = part selections + finish + placements + name. Routed
through `IAFLCosmeticPersistence` — the same seam purchase and earn already use. No bypass.

---

## 6. PHASE SEQUENCE AND GATES

Each phase: Read-Scope → build → prove → commit clean → tracker-sync.
**No phase starts until its gate is green.**

| Phase | Deliverable | Gate to enter | Lane |
|---|---|---|---|
| **CC-0** | Socket/bone contract read on IK_Mannequin; neck-seam read; character conform recipe | ProMod Experience boots | AIK |
| **CC-1** | Base pair — 2 bodies + 1 head each, suit material, zone UVs, playable in ProMod | CC-0 reports | AIK + Blender lane |
| **CC-2** | Neon suit axis — 8 colours (7+base, L11) wired to the registry. **Author the Solar `#FF7300` edge-ramp preset** — the one net-new colour asset. | CC-1 watched | AIK |
| **CC-3** | Head roster — remaining 16 | CC-1 conform proven | Blender lane + AIK |
| **CC-4** | Sticker composite — struct in `AFLNetTypes`, zones, RT bake | CC-1 zone UVs exist | **Claude Code** (C++) + AIK (assets) |
| **CC-5** | Name + save/load + slot counted entitlement | CC-4 data shape settled | **Claude Code** (backend) |
| **CC-6** | Creator UI — drag-and-drop placement, extends the locker AxisPicker. Design direction: **§12**. | CC-4 + CC-5 | AIK |
| **CC-7** | `AFL.Accessory.*` hardpoints — shoulder, chest, back, forearm, belt, thigh | CC-1 socket schema | AIK |
| **CC-8** | Masks — geometry on a head socket | CC-1 head socket exists | AIK + Blender lane |

**CC-1 carries all the rig unknowns.** CC-3 onward is volume on a proven line — the same
shape as the 147-row cannon batch: canary one, watch it, then batch.

**CC-4 and CC-5 are the only phases needing real design before build.** Everything else is
content on proven rails.

---

## 7. LANE ASSIGNMENTS — who does what, and who must not

| Lane | Owns | Must not |
|---|---|---|
| **AIK** (in-editor) | Asset authoring, `.uasset` edits, part maps, catalog rows, materials, sockets read-back, PIE-armed test harnesses | Run PIE. Read logs during PIE. Author C++. Place operator sockets. |
| **Claude Code** (terminal, editor-closed) | C++ (`AFLNetTypes` struct, RT bake, persistence), Blender/Rodin export, backend, cooks, dedicated-server runs | Touch in-editor assets. Assume editor state. |
| **Operator** | Builds, PIE watches, socket placement, merges/pushes, physical mode switches, all design rulings | — |
| **Claude** (orchestrator) | Specs paste-ready blocks addressed to one lane, holds this SSOT, resolves cross-lane conflicts | Issue a block without naming its lane. Stack blocks before the prior one returns. |

**Doctrine, unchanged:**
- Agent edits + shows diffs + **stops**. Operator owns UBT regen and build.
- No silent lane switches — flag the lane, why it is moving, and why it structurally must.
- One block at a time. Do not stack.
- ✅ = watched in PIE on a controllable pawn. Never "compiles."

---

## 8. PROOF STANDARD

| Item | Proof required |
|---|---|
| Base pair (CC-1) | Watched in PIE: correct body spawns, correct head, suit material renders, weapon holds via IK |
| Head roster (CC-3) | Canary ONE head watched before batching the remaining 15 |
| Sticker composite (CC-4) | **2-CLIENT.** Placements must appear correctly on a REMOTE client. A listen-host watch proves nothing about replication. |
| Save/load (CC-5) | Log out, log in, character intact. Purchase a slot, second character persists. |
| Accessories (CC-7) | Watched attached, no capsule/hitbox change, net cost measured at 16 players |
| Masks (CC-8) | Watched over a baked face composite — occlusion correct |

**Canary-before-scaling applies throughout.** One head, one sticker, one accessory proven
before any batch.

---

## 9. BANKED TRAPS — these all bit this project. Every block inherits them.

**Verification:**
- A validator must be shown to **FAIL on known-bad input** before its pass means anything.
- Compare **enum values**, never stringified names.
- `reload_packages` **silently no-ops on a dirty package** — check the save return value first.
- A component reads `None` immediately after `compile_blueprint` — reload, then read.
- Asset-registry dependency queries return `[]` for freshly-created packages. Scan lag reads
  identically to "not wired."
- Resolve **LFS** before any blob-content comparison — HEAD blobs are 130-byte pointers.
- A negative result from an **unverified token** is not evidence.
- Bracket/brace counting cannot validate a file containing prose. Only execution proves structure.
- `GameFeatureAction_AddComponents` properties read in **PascalCase**; snake_case returns zero
  properties. A bridge returning no properties may mean wrong case, not no properties.

**Authoring:**
- Never `new = src` on a struct — it aliases and renames the source row. Deep-copy all fields.
- Catalog writes = **whole-array reassignment**; in-place struct edits do not persist.
- Every duplicate must point at its **own** assets. A clone left aimed at its source is the
  `dd800ace` bug.
- Never duplicate a mesh or skeleton that carries operator-placed sockets.
- Bone names differ per family and per side. **Hardcode nothing** — read them.
- Verify the **WIRED** asset, not the canonically-named one. `DA_AFL_PawnData_Hero_Default`
  looks canonical and is not wired to anything.

**Architecture:**
- `AddComponents` matches by class **including subclasses**. A duplicate is a **sibling**, not
  a child — entries targeting the source class will not apply to it.
- An inherited component **cannot be deleted** in a child BP.
- A data asset with **no consumer is inert**. Check for a reader before authoring config.
- `/Game` content must not reference GameFeature content, and a GameFeature map must not
  reference another GameFeature's content (AssetReferenceRestrictions).
- P-CONTROLS: CMC mechanics use a component reading the stock CMC via `GetCharacterMovement()`.
  Never subclass or reparent the CMC.

---

## 10. OPEN ITEMS CARRIED IN (not blockers for this phase)

| Item | State |
|---|---|
| ASC spawn race | OPEN. First equip of a match is mute until one weapon cycle. Both controller-side hooks eliminated; if revisited, a pawn-side component observes and notifies the controller. |
| Packaged-exe plugin-mount check | Owed. Cook proven; runtime mount unobserved. |
| 3 GameplayCue tags with no manager entry | Warning-only; runtime failure never established. |
| `BeamChannel_v2` shared pawn-wide heat pool | Cutter couples with beam cannons. |
| Inventory orphan per respawn | Inert. |
| 28 `_X` packages carry a dead `MI_AFL_IRONICS_Visor_FANATICS` import | X-line hygiene. |
| Visor axis-reachability question | **X-line only.** L5 removes it from the Pro Mod path. |
| `ID_AFL_Ryu` → "Ryu Tempest" | Deliberate. Do not touch. |

---

## 11. DECISIONS — RESOLVED 2026-07-31

| # | Question | Resolution |
|---|---|---|
| D1 | Does a Finish carry between a player's robot and their Pro Mod character? | **NO.** Separate characters, separate mods → **L10**. Palette data shared, SKUs forked. |
| D2 | Is 7+base the same set as the brief's 8-colour axis? | **YES**, one set → **L11**. Solar edge-ramp is the only net-new colour asset. |
| D3 | Masks sold or granted? Mask/tattoo interaction? | **BOTH** sold and granted. Combination is **player preference, not our constraint** → **L12**. |
| D4 | Character name and number/ID — who chooses? | **PLAYER CHOOSES.** Internal ledger is account → characters owned → character names → **L8**. |

**None outstanding.** Any new question raised during a phase is added here with its phase tag,
resolved by the operator, and promoted to §0 if it is a standing ruling.

---

*Authored 2026-07-31 as the Pro Mod character phase SSOT. No code, no assets, nothing staged.
Each section expands with disk-verified facts when its phase is scoped to build.*

---

## 12. CREATOR UX — DESIGN DIRECTION

Applies the `expert-game-designer` discipline. **Token authority is
`IRONICS_UI_STYLE_SSOT.md`, not this doc and not the skill's default palette.**

> ⚠ **Recorded conflict, resolve at CC-6 scope-time.** The `expert-game-designer` skill leads
> with an Apple-Glass token set (white frosted panels). The store work explicitly corrected
> **away** from that to cyber/neon, while `IRONICS_UI_STYLE_SSOT.md` §2 holds a locked
> 4-colour ROLE palette (electric-blue lead / arc-violet accent-state) and the loadout design
> doc still lists Apple-Glass among its tokens. **Read the UI Style SSOT and conform to it.**
> Do not import the skill's palette wholesale, and do not resolve the tension by inventing a
> third look.

### 12.1 The design problem, stated honestly

A character creator is judged in the first ninety seconds. Players do not evaluate it on
feature count — they evaluate it on whether the thing they made looks like *theirs*. Three
properties carry that, in order:

1. **The preview must be the product.** What the player sees in the creator is exactly what
   spawns. Any divergence — different lighting, different pose, a stand-in mesh — reads as a
   bait-and-switch the first time they load into a match.
2. **Placement must feel physical, not numeric.** Drag-and-drop with surface-snapping and a
   clamped zone. Never expose raw position/rotation/scale fields as the primary interaction.
   Numeric fields are an advanced disclosure, not the default.
3. **Every choice must be reversible without loss.** Undo on placement, revert-to-saved on the
   character. A creator that punishes experimentation gets used once.

### 12.2 Layout — conform to the locker, do not invent a second idiom

`IRONICS_LOADOUT_DESIGN.md` already establishes the paper-doll + owned-drawer pattern with a
shared 3D preview pawn and a parameterised `AFLW_Loadout_AxisPicker`. **The creator is that
screen with a placement mode**, not a new screen family.

```
┌─ CREATE ──────────────────────────────────────────────┐
│  [ NAME: ____________ ]              [ SAVE 1 of 1 ]  │
│   ┌─ BODY ───┐        ╔═══════════════╗   ┌ SUIT ──┐  │
│   │ Female   │        ║               ║   │ Neon   │  │
│   └──────────┘        ║  LIVE PREVIEW ║   │ Blue   │  │
│   ┌─ HEAD ───┐        ║   (the same   ║   └────────┘  │
│   │ L-02     │        ║   pawn that   ║   ┌ MASK ──┐  │
│   └──────────┘        ║    spawns)    ║   │ None   │  │
│   ┌─ MODS ───┐        ╚═══════════════╝   └────────┘  │
│   │ 6 slots  │   ── STICKERS ─────────────────────    │
│   └──────────┘   Chest 1/2 · Stomach 0/1 · Back 0/1   │
│                  Legs 0/4 · Face 1/2                   │
└────────────────────────────────────────────────────────┘
```

**Slot counters are always visible.** The 9-cap (§4.2) is a design feature, not a limitation
to hide — showing `Chest 1/2` makes each placement feel considered and makes an extra-slot
SKU legible rather than predatory.

### 12.3 Placement interaction spec

| Behaviour | Rule |
|---|---|
| Enter placement | Click a zone on the preview, or a zone row in the sticker panel. Camera eases to that zone. |
| Drag | Sticker follows the cursor projected on the mesh surface. **Clamped to the zone UV rect** — it cannot cross a seam. |
| Invalid target | Sticker ghosts to `Text/Tertiary` opacity. **No error text.** The visual state is the message. |
| Rotate / scale | Modifier-drag or on-canvas handles. Scale clamped to a per-zone min/max so nothing degenerates into a pixel or swallows the torso. |
| Commit | Server validates entitlement **on placement**, not only on ownership. |
| Undo | Per-placement undo stack, minimum depth 10. |
| Zone full | The zone row shows `2/2` and further drags bounce with a short spring. Never a modal. |

**Motion is meaning.** Camera eases to a zone; it does not cut. A placement lands with a
sub-200ms settle. Nothing spins, bounces decoratively, or pulses without a state change behind
it — luminous restraint applies to motion as much as to glow.

### 12.4 Neon as the identity carrier

The suit's accent lighting is the strongest identity signal at gameplay range — it reads long
after faces and stickers have stopped being legible. Design implications:

- **The neon accent is a first-class creator choice, not a sub-option of suit colour.** It gets
  its own picker.
- **Preview it at range.** Offer a zoom-out state showing the character at approximate combat
  distance. A creator that only previews at portrait distance sells looks that vanish in play —
  that is the same lesson as `distinctness is by emblem`.
- **Team-colour override must be visible in-creator.** If a mode overrides the accent for team
  readability, the player learns that in the creator, not in their first match.

### 12.5 What NOT to build

- **No face sliders (L6).** Preset heads. Sliders on a face nobody sees at combat range are
  cost without return.
- **No randomiser as a headline feature.** It is a nice affordance on an empty state; it is not
  a substitute for legible choice.
- **No paywall inside the creation flow.** Unowned items are surfaced in the store, not greyed
  inline in the locker — `IRONICS_LOADOUT_DESIGN.md` §4 already rules the locker OWNED-ONLY
  with a single "＋ Get more" deep-link. The creator inherits that rule.

### 12.6 Deliverables when CC-6 is scoped

Per the `expert-game-designer` pipeline, scaled to what is useful:

1. **Concept** — inline SVG mockup of the creator screen and the placement mode
2. **Spec** — layout grid, spacing, tokens read from `IRONICS_UI_STYLE_SSOT.md`, animation timings
3. **AIK prompt** — UMG widget generation conforming to the C++-base / WBP-child split and
   `PushWidgetToLayerStack(UI.Layer.Menu)`

Concept art prompts are not required — the four Pro Mod concept sheets already establish the
visual direction and are the reference of record.

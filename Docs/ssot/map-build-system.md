# SSOT — MAP BUILD SYSTEM (Tier 2)

**What this is:** what the map system **is** and **why**. It changes when the system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`). A per-map *design* lives in `Docs/design/<MapName>_DESIGN.md`; a per-map *state* lives in Tier 3.

> **Why the separation is enforced here specifically.** A BR design brief once carried a section asserting which
> match-conclusion assertions were unproven and that the cause was unknown. It was accurate when written and false
> four commits later, but it read as design and nobody re-checked it. **A Tier-2 doc that describes the world
> instead of the system will always rot.** If a sentence would change because of a commit, it does not belong here.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by id
(**L3**, **C3**, **C5**, **C6**, **NM5**, **N1**, **G4**).

---

## 1. SCOPE

This SSOT governs: the mode-ladder → map-tier mapping · the footprint sizing law · the district model · World
Partition and data layers as the district mechanism · the two art pipelines · the per-map design gate · the
display-name ↔ disk-asset ↔ config naming convention · the universal extraction primitive · the telemetry loop
that validates a map before art.

It does not govern: match rulesets (→ `ssot/match-modes.md`), matchmaking and staking (→ `ssot/matchmaking.md`),
economy payout values (→ `ssot/economy-store.md`), or which maps exist and what state they are in (→ Tier 3).

---

## 2. THE MODE LADDER → MAP TIER MAPPING

Three families that are genuinely different loops, not one loop scaled.

| Family | Brackets | Loop character | Symmetry | Map tier |
|---|---|---|---|---|
| **Arena PvP** | 1v1, 2v2, 3v3, 4v4 | Tight extract-or-eliminate; competitive-integrity first | Mirror / rotational (mandatory) | A (duel), B (mid-arena) |
| **Team** | 5v5 → 8v8 | Objective + extraction; larger rotations, verticality | Mirror / rotational | C |
| **Battle Royale** | 18, 36 | Open, loot-gradient, collapsing zone, multi-extract | Asymmetric — fairness from drop choice and loot distribution, not geometry | D |

**Tier characteristics** (starting targets, validated by greybox telemetry — §9):

| Tier | Brackets | Density | TTFC | Footprint across | Flow requirement |
|---|---|---|---|---|---|
| **A** Duel | 1v1, 2v2 | very high | ~5–10 s | ~30–60 m | single symmetric arena; 2 primary angles + 1 flank |
| **B** Mid-Arena | 3v3, 4v4 | high | ~8–15 s | ~60–100 m | three-lane or figure-8; no dead-ends; 2–3 power positions each with a counter-route |
| **C** Large-Team | 5v5–8v8 | medium | ~12–25 s | ~100–200 m | multi-lane + meaningful verticality; rotation depth so one team cannot lock the map |
| **D** BR | 18, 36 | low at drop by design | drop-dependent | ~400–600 m (18) · ~600–900 m (36) | POI graph; ~2 players per named POI at drop |

**Adjacent-band sharing:** a level may host two *adjacent* brackets (1v1↔2v2, 3v3↔4v4, 7v7↔8v8) via Experience
variants where density holds. **Non-adjacent brackets never share a level** — the density delta is too large for
both to play well.

### 2.1 VENUE CLASS — a property of the LEVEL, never of the bracket (R97)

**Every level is exactly one of two classes, and the class is authored, not derived.**

| Class | Character | Symmetry | Typical tiers | Streaming |
|---|---|---|---|---|
| **ARENA** | Contained, round-based, extract-or-eliminate. One contiguous play-space the whole match happens inside. | **Mirror / rotational, mandatory** — competitive integrity first | A, B, and C where authored as one space | Usually none required (§6) |
| **MAP** | District-scale. Rotations between areas, POIs, and a larger sense of place. | Asymmetric permitted — fairness comes from spawn/route/loot distribution rather than geometry | C, D | District model (§4), World Partition + data layers (§5) |

> **⚠ CLASS AND BRACKET ARE ORTHOGONAL, WHICH IS THE WHOLE OF R97.** A 5v5 ARENA and a 5v5 MAP are two
> DIFFERENT LEVELS serving the same bracket in different classes — not one level described two ways. The
> footprint ladder (§3) still sizes both by bracket; the class says what *kind* of space that footprint is.
> **`ssot/matchmaking.md` R97 makes this a queue dimension**, so a player who chose one will never be handed
> the other.

**How a new level classifies itself — apply in order, and the first answer is the answer:**

1. **Is mirror or rotational symmetry MANDATORY for it to be fair?** → **ARENA**. That requirement is what
   an Arena is; a district cannot satisfy it and should not try.
2. **Is the whole match contained in one play-space, with no streaming between areas?** → **ARENA**.
3. **Is it authored as a district (§4) — adjacent brackets, data-layer activation, multiple POIs?** → **MAP**.
4. **Is it a BR footprint (tier D)?** → **MAP**, always. A collapsing zone over a POI graph is the definition.

**THE WORKED EXAMPLE, AND IT IS THE CLEAREST CASE FOR R97.** `L_ShantyTown` is a **MAP**, and it fills the
**entire bracket ladder from one venue** by sizing to the party via fenced data-layer districts
(`ssot/../design/ShantyTown_BR_DESIGN.md` §11):

| District | Footprint | Brackets |
|---|---|---|
| `District_Duel` | ~59 × 59 m | 1v1, 2v2 |
| `District_Arena` | ~87 × 87 m | 3v3, 4v4 |
| `District_Team` | ~118 × 118 m | 5v5, 8v8 |
| *(whole map)* | 357 × 302 m core · 617 × 607 m landscape | BR_18 · BR_36 |

> **Districts are FENCED REGIONS INSIDE the map, not carved out of it** — BR still uses the whole thing. And
> note `District_Arena` is named for the **mode-ladder family** (§2), not for the venue class: a fenced
> district inside a district-scale world is still a MAP. **A 5v5 in `District_Team` and a 5v5 in ARCANEON are
> the same bracket in two genuinely different styles** — which is exactly why R97 makes the class a queue
> dimension rather than letting the server pick between them.

> **A LEVEL THAT ANSWERS "EITHER" IS A LEVEL WITHOUT AN IDENTITY, and the fix is design work rather than a
> registry entry.** Pick one and author to it. If a *third* class ever seems necessary because a level fits
> neither, that is `ssot/matchmaking.md` §4.3's invariant failing — the answer is a pool row or a bracket,
> never a third class, because a third class multiplies the queue set again.

---

## 3. THE FOOTPRINT LADDER — THE SIZING LAW

This is the sizing law for every future map.

### 3.1 Derivation

Two maps, **independently authored** at different tiers, converged on the same playable-area-per-player figure:

| Reference | Bracket | Players | Playable area | m²/player |
|---|---|---|---|---|
| Tier-B mid-arena | 3v3 | 6 | ~5,100 m² | **850** |
| Tier-C large-team | 8v8 | 16 | ~14,016 m² | **876** |

**They agree within ~3% across a 2.67× difference in team size.** That is the whole argument: two designs that
did not reference each other, at different tiers, landed on the same density. A single map's area is an accident;
two agreeing at that spread is a **law**.

- **Dense baseline ≈ 865 m²/player** — the competitive target. Engagements are frequent, rotations are short,
  TTFC sits inside the tier window.
- **Sparse ceiling ≈ 1,400 m²/player** — the upper bound before a map reads as empty: search time dominates,
  TTFC drifts out of window, and traversal heatmaps develop dead zones.

Anything below the dense baseline trends toward spawn-camping and no room to rotate; anything above the sparse
ceiling stops being a fight and becomes a search.

### 3.2 The ladder

| Bracket | Players | Dense (×865 m²) | Sparse ceiling (×1,400 m²) |
|---|---|---|---|
| 1v1 | 2 | 1,730 m² | 2,800 m² |
| 2v2 | 4 | 3,460 m² | 5,600 m² |
| 3v3 | 6 | 5,190 m² | 8,400 m² |
| 4v4 | 8 | 6,920 m² | 11,200 m² |
| 5v5 | 10 | 8,650 m² | 14,000 m² |
| 6v6 | 12 | 10,380 m² | 16,800 m² |
| 7v7 | 14 | 12,110 m² | 19,600 m² |
| 8v8 | 16 | 13,840 m² | 22,400 m² |

### 3.3 The BR caveat — the ladder does not size a BR map

BR is **deliberately low-density at drop**; convergence is manufactured by the collapsing zone, not by the
footprint. A Tier-D map at ~400–900 m across sits an order of magnitude above the arena ladder in m²/player at
drop, and that is correct — it is what makes the drop phase a looting phase rather than an instant fight.

**Where the ladder does apply to BR: the final circle.** The last zone is an arena fight, and it should be sized
by this ladder against the *expected surviving player count*, not against the lobby size. A final circle sized
off the drop count produces the classic failure of a BR that ends in an empty field.

---

## 4. THE DISTRICT MODEL — THE STANDARD APPROACH

**One map covers the whole matchmaking matrix via fenced play-spaces, rather than one map per bracket.**
This is the standard methodology for new maps; a per-bracket venue is the exception that needs a reason.

| District | Area | Footprint | Brackets served |
|---|---|---|---|
| **D1 Duel** | ~3,460 m² | ~59 × 59 m | 1v1, 2v2 |
| **D2 Arena** | ~7,500 m² | ~87 × 87 m | 3v3, 4v4 |
| **D3 Team** | ~14,000 m² | ~118 × 118 m | 5v5, 8v8 |

### 4.1 Why these three numbers

Each district is **sized dense for its larger bracket**, which places its smaller bracket at or near the sparse
ceiling. That is exactly why a district covers **two adjacent brackets and no more**:

- **D3** — 8v8 lands at 875 m²/player (dense baseline); 5v5 lands at 1,400 m²/player (**exactly the sparse
  ceiling**). The band is fully used, top to bottom.
- **D2** — 4v4 at ~938 m²/player, 3v3 at ~1,250 m²/player. Both sit inside the dense→sparse band.
- **D1** — 2v2 at 865 m²/player (dense baseline). 1v1 runs sparser than the ceiling, which is acceptable and
  intended: a duel tolerates approach and search space in a way a team fight does not, and Tier A's requirement
  is about *angles* (2 primary + 1 flank), not about area.

Extending a district to a third bracket would push one end outside the band — that is the constraint that fixes
the count at three.

### 4.2 Why districts rather than one map per bracket

- **Content economics.** One art pass, one lighting pass, one nav build, one perf budget serves six brackets.
- **Identity.** A venue with three districts reads as one place. Six separate small maps read as six test levels.
- **The fence is a design tool, not a limitation.** A hard boundary at the district edge is what lets the same
  world host a 1v1 without the duel drowning in a 36-player footprint.
- **Containment is already required** (**C5**), so the fenced-region discipline is work the map owes anyway.

### 4.3 Rules for authoring a district

1. A district is a **contiguous, fully-contained play-space** — the containment requirement (**C5**) applies to
   the district boundary exactly as it applies to a map boundary.
2. A district must satisfy its tier's flow requirement (§2) **within its own footprint** — a D2 district needs
   its own three-lane/figure-8 and its own power positions with counter-routes. It is a map, not a room.
3. Districts must not share sightlines. A player in one district must never see, shoot, or be shot from another.
4. Each district carries its own extraction real estate sized to its bracket (§6).
5. The whole-map (undistricted) space remains available to BR, which uses the entire world. **Districts are
   fenced regions inside the map, not carved out of it.**

---

## 5. WORLD PARTITION + DATA LAYERS — THE DISTRICT MECHANISM

### 5.1 The mechanism

Districts are implemented as **World Partition data layers**, whose runtime state is toggled per match so that
only the active district is loaded and playable.

Layer organisation conforms to the shipped reference organisation — **`Layout` · `Gameplay` · `ExtraSpawn` ·
`Lighting`** as base layers, with **district layers** alongside them (`District_Duel`, `District_Arena`,
`District_Team`). Base layers carry what every match needs; district layers carry the fenced play-space and its
bracket-specific gameplay actors.

### 5.2 Activation is server-authoritative — and must be

Data-layer runtime state is changed through `UDataLayerManager::SetDataLayerRuntimeState`. The engine **refuses
the call on a client**: `WorldDataLayers.cpp` returns `false` with
`ESetDataLayerRuntimeStateError::AuthoritativeFromClient` when `NetMode == NM_Client`. State set on the server
replicates outward to clients.

**Why that property is load-bearing rather than incidental:** the district layer *is* the playable space. If a
client could set layer state, a client could change which region of the map is loaded and collidable — in a
**staked** match, that is the ability to alter the arena you are being wagered on. Server authority over layer
state is therefore the same class of requirement as server authority over damage (**N1**), and any activation
path must preserve it. A client never decides which district is live.

### 5.3 What this requires of an activation path

An activation mechanism must: run **server-side only**; set state **before players are placed** into the
district; and derive the district from the match configuration rather than from client input. It must not
attempt the call on a client — the engine will reject it, and a design that relies on the rejection is a design
that has no district on a listen-host client.

---

## 6. WHEN WORLD PARTITION IS AND IS NOT REQUIRED

WP is a tool with real cost — one-file-per-actor external actors, HLOD layers, streaming configuration, and a
conversion that is not reversible in practice. It is required when the map needs it and harmful when it does not.

**Require World Partition when ANY of these hold:**
- The map **carries district data layers** (§5). The district model requires runtime layer state, which requires WP.
- The map is **Tier D**, or a **large Tier C** — footprint beyond roughly 150–200 m across.
- The map has enough actors that a monolithic `.umap` becomes a **per-save liability**: every editor save
  rewriting a large binary into LFS, and every concurrent edit colliding on one file.
- The map has **genuinely distinct regions** that never need to be resident simultaneously.

**Do NOT use World Partition when ALL of these hold:**
- The map is a **single-bracket venue** at Tier A or Tier B footprint (≲100 m across).
- It carries **no district layers**.
- The monolithic level loads and saves inside budget.

A ~75–85 m single-bracket Tier-B arena is the canonical *not-WP* case: it gains nothing from streaming, and pays
the complexity in exchange. **A map's brief states its WP decision in its Identity section, with the reason** —
so the decision is reviewable at the gate rather than discovered at conversion time.

---

## 7. THE TWO ART PIPELINES

There are two paths from "a level exists" to "a level looks AAA", and **one tool does not serve both** because
their inputs are structurally different.

### 7.1 Pipeline 1 — Greybox → kit-tile replacement

**Input:** a greybox authored by us, whose actors carry a **role vocabulary in their labels**.
**Contract:** the generator classifies each greybox actor by label prefix and substitutes a kit mesh tiled to
that actor's transform and bounds.

| Label prefix | Role |
|---|---|
| `Floor_Pit`, `Bridge_`, `Ring_` | floor |
| `Wall_`, `ClimbFace_` | wall |
| `Parapet_` | rail |
| `Div_` | fence |
| `Ramp_` | ramp |

**Why it works:** the greybox is the design data. Art replaces the *look* and never the *design* — footprints,
transforms, spawns, POIs, objectives and nav are preserved 1:1, so a validated flow stays validated.
**What it requires:** a greybox authored *with* the vocabulary. An unlabelled greybox is invisible to it.
**Governing rule:** the greybox **is** the spec. If art changes a footprint, the design changed and the map
returns to telemetry validation.

### 7.2 Pipeline 2 — Vendor art → IRONICS conform

**Input:** an already-arted third-party environment.
**Contract:** retexture and palette-conform to the IRONICS look, add signage and branding, rebuild lighting and
atmosphere, then place AFL gameplay actors (spawns, extraction, weapon spawners, nav, containment).

**Why Pipeline 1 cannot do this job:** on vendor art **there is nothing to replace**. The geometry already *is*
the art; there are no role-labelled greybox actors, so the generator classifies nothing and substitutes nothing.
Running it produces no output — not a bad result, an empty one.
**Why Pipeline 2 cannot do Pipeline 1's job:** a conform pass adjusts the look of geometry that already exists.
It cannot author layout. Layout comes from a greybox that encodes a validated design.

### 7.3 Choosing

Author the map ourselves → Pipeline 1. Start from a purchased/vendor environment → Pipeline 2. A vendor map that
we then extend with our own greyboxed regions runs **both**, on different regions, and the brief says which
region takes which.

---

## 8. THE PER-MAP DESIGN GATE

**Doctrine — see `DOCTRINE.md` L3.** No map enters greybox build until its per-map design brief exists and is
**operator-approved**. The order is: **brief → approval → greybox → telemetry → balance → art → PIE sign-off.**

The brief is to a map what a sibling-diff is to an ability: the artefact that makes the work reviewable *before*
it is expensive. **Committing a brief banks the design; it does not approve the build** (**GD8**) — a brief's
gate status is part of the brief, and it is the only place that status lives at Tier 2.

### Required brief sections

| # | Section | Must specify |
|---|---|---|
| 1 | **Identity** | Name, tier, brackets + Experience variants hosted, **WP decision with its reason** (§6) |
| 2 | **Footprint & density** | Playable area, m²/player against the ladder (§3), TTFC target per bracket |
| 3 | **Flow** | Lane/loop diagram; no dead-ends; rotation routes between key points |
| 4 | **Power positions** | Each strong angle **with its flank/counter-route**; no uncontested map-spanning sightline |
| 5 | **Sightline bands** | Where CQB / mid / long engagements live, so the whole weapon roster has a home |
| 6 | **Extraction** | Zone count, placement, payout tier (§6 of this doc); for BR, the collapse interaction |
| 7 | **Spawns** | Layout, team-aware selection, anti-camp, spawn immunity, drop distribution for BR |
| 8 | **Signature mechanic** | The map's one hook (**C3**), with its server-authority and replication note |
| 9 | **Symmetry** | Mirror / rotational / asymmetric, with the integrity rationale |
| 10 | **Readability** | Beam and silhouette readability; the signature mechanic must be visually distinct from weapon fire |
| 11 | **Telemetry hooks** | Which heatmaps validate this map and the **greybox exit criteria** — the metric windows that must be hit before art |
| 12 | **Containment** | How the boundary is sealed on every side (**C5**) |
| 13 | **Gate** | The approval state of this brief |

---

## 9. THE UNIVERSAL EXTRACTION PRIMITIVE

Extraction is the universal loop: fight → drop energy → collect → risk extraction → bank. **Every map carries
extraction real estate**, scaled by bracket. A map opts out only if it is a declared special map.

| Tier | Extract zones | Placement logic |
|---|---|---|
| A (1v1–2v2) | 1 | Central, contested. Channeling exposed is the win-tension beat; extract-vs-eliminate is the live decision |
| B (3v3–4v4) | 1–2 | One central high-value; optional peripheral safer-but-slower zone for counterplay |
| C (5v5–8v8) | 2–3 | Distributed, to enable simultaneous team objectives and flanking denial |
| D (BR) | 3–5 | Risk gradient — some hot/central/high-payout, some cold/peripheral. The closing zone can strand a cold extract, which is the intended rotation tension |

**Payout follows risk** — central and contested pays more than peripheral and safe. The reward *values* are
owned by `ssot/economy-store.md`; this document owns only the geometry and count.

---

## 10. THE TELEMETRY LOOP — HOW A MAP EARNS ITS ART PASS

Art follows proven flow, never precedes it. Every map runs this loop on its greybox:

1. **Greybox the flow only** — spawns, lanes, power positions, extraction zones. No art.
2. **Metrics playtest** with bots and humans, streaming map-coordinate events to the telemetry substrate.
3. **Read the heatmaps:**
   - *Kill/death density* — a spike is an overpowered angle → add a counter-route or break the sightline.
   - *Traversal density* — a cold region is dead space → reshape it or cut it.
   - *TTFC distribution* — outside the tier window → resize, or move spawns.
   - *Extract outcomes* — contest rate and hold-vs-deny balance → tune placement and payout.
4. **Balance pass** against the tier targets (§2) and the ladder (§3).
5. **Art pass only after flow is proven.**

**Fundamentals enforced at greybox, before any of the above counts:** no map-spanning uncontested sightline ·
layered cover at CQB/mid/long · every power position has a flank/counter · mirror or rotational symmetry on
ranked tiers · anti-spawn-camp spawn selection · full nav coverage with no unreachable islands · containment
sealed on every side (**C5**) · spawn actors of the correct class and load behaviour (**C6**).

---

## 11. NAMING AND REGISTRY CONVENTION

Three names identify a playable configuration, and they are deliberately different things:

| Layer | What it is | Where it lives |
|---|---|---|
| **Display name** | What the player reads on the "HOST A GAME" tile — title + subtitle | `TileTitle` / `TileSubTitle` FText on the playlist data asset |
| **Disk asset** | The level | `MapID` → the `.umap` |
| **Config** | The ruleset/bracket binding | `ExperienceID` → the `LyraExperienceDefinition` |
| **Venue class** | **ARENA or MAP** (§2.1, R97) — which queue the level can be drawn into at all | The registry row. **Authored, never inferred.** |

**The internal-codename ↔ external-name split is by design.** A playlist asset's *filename* is an internal
codename and may not describe what it hosts; the **`MapID` + `TileTitle` are the truth**. Renaming a shipped DA,
Experience or `MapID` breaks host resolution (**NM5** — a shipped id is never renamed); only the tile FText
carries the player-facing name and only it may change.

**The registry is the single reconciliation point.** One document maps display name ↔ playlist DA ↔ Experience ↔
`MapID` ↔ bracket ↔ **venue class**. **Read the registry; never re-derive a map's identity from a filename** —
that is exactly how two configurations get transposed. Any lane naming a map or config cites the registry.

> **⚠ THE VENUE CLASS IS THE FIELD MOST LIKELY TO BE GUESSED, AND IT HAS ALREADY HAPPENED ONCE.** When R97
> was first wired, the backend queue registry was seeded by reading class off the level NAMES — `L_Duel_01`
> and `L_Arena_04` were taken as ARENA, `L_Expanse` as MAP — which is precisely the re-derivation the
> paragraph above forbids. It produced the right answer by luck, on three levels whose names happened to be
> honest. **The class must be carried in the registry row and read from there**, because a level called
> `L_Arena_Something` that is authored as a district would otherwise be filed into a queue it cannot serve,
> and the symptom would be players getting the style they did not choose — the exact failure R97 exists to
> prevent.

*(The registry's per-map build-state — which maps exist and what state they are in — is Tier 3 and lives in the
tracker. The registry at Tier 2 owns only the mapping.)*

---

## 12. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — the laws cited here: **L3** per-map gate · **C3** one signature mechanic ·
  **C5** containment · **C6** spawn actor class and load behaviour · **N1** server authority · **NM5** shipped ids ·
  **GD8** banking a design ≠ approving a build · **G4** content reference restrictions.
- `ssot/match-modes.md` — what rulesets a bracket runs.
- `ssot/matchmaking.md` — how players reach a bracket, and staking.
- `ssot/economy-store.md` — extraction payout values.
- `Docs/design/<MapName>_DESIGN.md` — per-map briefs (§8).
- `Docs/runbooks/` — the art-pass and wiring procedures that execute §7.

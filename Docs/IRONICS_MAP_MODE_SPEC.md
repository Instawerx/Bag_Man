# IRONICS — MAP & MODE DESIGN SPEC

**Status:** Design SSOT · v2 · 2026-06-20 (open decisions resolved; per-map design gate added)
**Owners:** Orchestrator-architect (design) · Operator (build/PIE sign-off) · AIK (in-editor authoring)
**Scope:** Mode ladder, map archetype tiers, the 10-map roster, the data-science iteration loop, build sequencing, and the Shrink system. Reconciles the expanded mode ladder against `BAG_MAN_LIVE_TRACKER` (P-MATCH, P-MAPS).
**Doctrine inherited:** `✅ = watched in PIE`; greybox flow proven before art; server-authoritative for anything competitive/economic; AAA-best, not bootleg.

---

## 0. Locked decisions (this pass)

1. **Extraction is the universal loop.** Every map carries extraction real estate, sized to tier. A map opts out only if it is a declared special map.
2. **36-player BR is a launch target — core-first build.** Design + greybox *all* tiers now; build and ship the 1–16 core first; 18/36 follow once netcode clears the higher ceiling.
3. **Existing Arena_01–06 fold in, plus net-new.** The six tracker arenas are part of the roster; we add to reach the tiered set below.
4. **Shrink is both a weapon/modifier and a dedicated map.** `20/20/100` stat model confirmed. Chaotic 4v4–8v8 party framing.
5. **Sensory-Friendly map is parked** — not designed this pass. (Recorded so it is not silently lost; revisit with autistic advisors + an OT when picked up.)

---

## 1. Canonical mode ladder

Three families — genuinely different game loops, not one loop scaled — plus offline and tournament layers.

| Family | Sizes | Loop character | Symmetry | Ranked? |
|---|---|---|---|---|
| **Arena PvP** | 1v1, 2v2, 3v3, 4v4 | Tight extract-or-eliminate duels/skirmishes; competitive-integrity-first | Mirror / rotational (mandatory) | Yes (skill core) |
| **Team** | 5v5 → 8v8 (teams up to 8) | Objective + extraction; larger rotations, verticality | Mirror / rotational | Yes |
| **Battle Royale / Extraction** | 18, 36 | Open, loot-gradient, collapsing zone, multi-extract | Asymmetric (POI variety) | Yes (separate pool) |

- **Offline / single-player:** every Arena and Team size runs vs Lyra bots (native bot support). Offline = same `LyraExperience` with bot fill, no matchmaking ticket.
- **Tournament:** bracketing layered on whichever certified sizes; not a distinct map archetype.
- **"4v4" supersession:** the old `4-player / ~20-player` scale anchors in the tracker were sizing assumptions, not a mode. They are replaced by this ladder.

### 1.1 Win conditions (RESOLVED — proven-popular standard)

Apply the genre-standard format players already expect and want; tune exact counts/thresholds by greybox + telemetry. Do not invent new win logic.

- **Arena PvP (1v1–4v4):** round-based, **best-of** (CS/Valorant-proven). A **round is won by eliminating the enemy team OR banking a successful extraction** — this keeps the extraction identity inside the format players know. Mirror maps, **side/spawn swap** each round (or at half), side-balance tracked in telemetry. Match = first to N round wins. Starting round counts (1v1 ~Bo11/first-to-6; 2v2–4v4 ~first-to-7, half-swap) are tuning values.
- **Team (5v5–8v8):** proven objective formats — **control/hardpoint, payload-style, or team-extraction to a value threshold**. Continuous match with score threshold or round-based; pick per level at greybox.
- **BR (18/36):** **placement + extraction hybrid** — last team standing AND/OR highest banked extraction value at zone-close. Rank by placement first, banked value as tiebreaker/secondary score.

---

## 2. The universal extraction primitive

IRONICS is a shooter-**extraction** game (P-LOOP: fight → drop energy → collect → risk extraction → cash out). Because extraction is now universal, **extraction-zone geometry is a shared map primitive across every tier**, scaled by player count:

| Tier | Extract zones | Placement logic |
|---|---|---|
| Duel/Small (1v1–2v2) | 1 | Central, contested. Channeling is the win-tension beat — you must hold it while exposed. Extract-vs-eliminate is the live decision. |
| Mid-Arena (3v3–4v4) | 1–2 | One central high-value; optional peripheral safer-but-slower extract for counterplay. |
| Large-Team (5v5–8v8) | 2–3 | Distributed to enable simultaneous team objectives + flanking denial. |
| BR (18/36) | 3–5 | Risk gradient — some hot/central/high-payout, some cold/peripheral. Collapse interacts: closing zone can strand a cold extract. |

Payout follows risk (central/contested = higher Watts). Ties directly to P-LOOP + P-ECONOMY; extraction reward values are owned by the economy spec, not here.

---

## 3. Map archetype tiers

You cannot scale one map across a 2-to-36 player range — map quality is a function of **player density** (playable area per player) and **time-to-first-contact (TTFC)**. Below are starting targets **to validate by greybox telemetry**, not fixed law.

### Tier A — Duel / Small (1v1, 2v2)
- **Density:** very high. **TTFC:** ~5–10s. **Footprint:** ~30–60 m across (single arena).
- **Flow:** single symmetric arena; 2 primary engagement angles + 1 flank; no map-spanning sightline.
- **Symmetry:** mirror (mandatory — ranked integrity).
- **Extract:** 1 central. **Spawns:** mirrored, ≈2.75s `Gameplay.DamageImmunity` (GE_SpawnIn, montage-tied, removed on spawn-montage complete), no enemy-LOS spawn.

### Tier B — Mid-Arena (3v3, 4v4) — the competitive heart
- **Density:** high. **TTFC:** ~8–15s. **Footprint:** ~60–100 m across.
- **Flow:** three-lane or figure-8 loop (no dead-ends); 2–3 power positions, **each with a flank/counter-route**.
- **Symmetry:** mirror or rotational.
- **Extract:** 1–2 (see §2). Most-played tier → most maps.

### Tier C — Large-Team (5v5–8v8, up to 16)
- **Density:** medium. **TTFC:** ~12–25s. **Footprint:** ~100–200 m.
- **Flow:** multi-lane + meaningful verticality; objective spread; rotation depth so a team can't lock the whole map.
- **Symmetry:** mirror or rotational. **Extract:** 2–3 distributed.

### Tier D — BR / Extraction (18, 36)
- **Density:** low early by design; collapsing zone forces convergence. **TTFC:** drop-dependent.
- **Footprint (starting target):** ~400–600 m for 18-player; ~600–900 m for 36-player.
- **POIs (named):** ~8–10 for 18; ~12–16 for 36 (≈2 players per POI at drop).
- **Symmetry:** asymmetric — fairness from drop choice + loot distribution, not geometry mirror.
- **Extract:** 3–5 dynamic (see §2). **Tech:** World Partition mandatory.

---

## 4. Map roster — the 10

Folds Arena_01–06 (signature mechanics intact) into the tiers and adds net-new to complete the set.

| # | Map | Tier | Sizes served | Signature mechanic | Source |
|---|---|---|---|---|---|
| 1 | Duel_01 | A | 1v1, 2v2 | clean mirror skill arena | net-new |
| 2 | Duel_02 | A | 1v1, 2v2 | verticality duel | net-new |
| 3 | Arena_01 | B | 3v3, 4v4 | neon-cyber combat surfaces (production baseline) | tracker |
| 4 | Arena_02 | B | 3v3, 4v4 | moving laser walls | tracker (AFL-1601) |
| 5 | Arena_03 | B | 3v3, 4v4 | gravity shifts | tracker (AFL-1801) |
| 6 | Arena_04 | C | 5v5–8v8 | energy storms | tracker (AFL-1801) |
| 7 | Arena_05 | C | 5v5–8v8 | vertical rails | tracker (AFL-2001) |
| 8 | BR_18 | D | 18 | tuned ~18 drop, tight collapse | net-new (Phase B) |
| 9 | BR_36 | D | 36 | full-scale, shrinking extraction zones | tracker concept (AFL-2002), scaled to 36 |
| 10 | Shrink_Yard | special | 4v4–8v8 party | oversized-world; Shrink kit (see §8) | net-new |

> Adjacent bands share a level via Experience variants where density holds (1v1↔2v2, 3v3↔4v4, 7v7↔8v8). Non-adjacent bands never share. "More content to pick from" lands as additional Tier B/C levels — the structure scales, the count grows.

---

## 5. Mode → LyraExperience mapping

- **Each map = one UE5 level.** World Partition for Tier D (and large Tier C); single-level for A/B.
- **Each `(map × team-size × ruleset)` = one `LyraExperienceDefinition`.** One mid-arena level runs `EXP_Arena01_3v3_Extract`, `EXP_Arena01_4v4_Extract`, etc.
- **Extraction lives in the base GameMode loop** for every Experience (universal per §0.1).
- **Offline** = same Experience, bot fill, no ticket.
- **Shrink** = an Experience variant (kit + rules) that can run on normal maps, **plus** the dedicated `Shrink_Yard` Experience.

This keeps the level count at the roster size while serving every mode size off shared levels.

---

## 6. Data-science iteration loop (the "proven best practices" engine)

Every map runs this loop. The substrate already exists (`AFL-0213` combat telemetry).

1. **Greybox** the flow only — no art. Block spawns, lanes, power positions, extract zones.
2. **Metrics playtest** with bots + humans; stream **map-coordinate** events to telemetry.
3. **Heatmaps:**
   - *Kill/death density* → spikes = overpowered angle → add counter-route or break sightline.
   - *Traversal density* → cold zones = dead space → reshape or cut.
   - *TTFC distribution* → out of tier window → resize / move spawns.
   - *Extract outcomes* → contest rate, hold-vs-deny balance → tune zone placement/payout.
4. **Balance pass** against the tier targets in §3.
5. **Art pass only after flow is proven** (greybox = the `✅ watched in PIE` gate for maps).

Stable fundamentals enforced at greybox: no map-spanning uncontested sightline; layered cover at CQB/mid/long to exercise the full laser roster; every power position has a flank/counter; mirror/rotational symmetry on ranked tiers; anti-spawn-camp logic.

---

## 7. Build sequencing (core-first)

**Phase A — ship the core (1–16 players).**
- Design + greybox **all** tiers now (A, B, C, D — flow-locked, telemetry-validated).
- Build + ship Tier A/B/C (up to 8v8 = 16). Within reach of the current ~20-player hardening gate.
- Shrink weapon/modifier can layer here (it's a GAS GE + weapon; slots into P-COMBAT/P-CONTROLS).

**Phase B — BR scale-up (18, 36).**
- Gated on netcode clearing the higher ceiling. Extends `AFL-1401/1402/1403` from 20 → 36:
  - Replication relevancy + dormancy validated at 36 players.
  - Bandwidth-per-player budget re-audited at 36 (current target <128 kbit up / <512 kbit down).
  - Dedicated-server tick + CPU validated at 36; region-aware fleet sizing.
- `BR_18` first (smaller ceiling), then `BR_36`.
- Dedicated `Shrink_Yard` map is content-phase (Phase B-adjacent).

**Gate to advance A→B:** 36-player match stable for 12 min end-to-end on production-spec dedicated server (the 20-player gate, re-targeted to 36).

---

## 8. Shrink system spec

Two delivery forms: a **weapon/modifier** that layers onto normal maps, and a **dedicated map** (`Shrink_Yard`) built as the focused experience.

### 8.1 Stat model — LOCKED starting values
| State | Scale | Damage dealt | Move speed | Hitbox | Abilities | Net effect |
|---|---|---|---|---|---|---|
| **Shrunk** (`20/20/100`) | 0.20 (uniform) | ×0.20 | ×1.00 | ~80% smaller | intact | fast, tiny, hard to hit, weak — agile glass-fly |
| **Grown** (Large Health Booster) | ~1.75 (start, tunable) | ×1.50 | ×0.75 | larger | intact | slow, big, strong, more health — the bruiser pole |

- **Uniform scale only** (non-uniform breaks animation/collision). Camera boom + capsule scale with mesh.
- Exact grow values are starting proposals — tune in playtest.

### 8.2 Kit
- **Shrink Gun** — PvP weapon; on-hit applies the Shrunk GE to the target.
- **Large Health Booster** — pickup; applies the Grown GE (resizes you up + health/power).
- **Special Weapon** — single-shot, slow-reload heavy hitter; the deliberate counter to the fast-small meta (one big slow shot that punishes agile shrunk players if it connects).

### 8.3 Engineering
- **Resize = a `GameplayEffect`** (the GAS attribute path) + a movement/camera `UActorComponent` — **reuses the proven `AFL-0304-B` P-CONTROLS pattern** (GameFeature-attached component reading stock CMC; no CMC subclass, no reparent).
- **Networking caution:** scale + capsule + camera-boom are **replicated mid-match state across clients** — the same category the `FNetSerializeScriptStructCache` lesson warns about. The resize GE/state must replicate cleanly; single-client will not catch desync. Any new net-serialized struct lives in `AFLNetTypes` (always-loaded), never a GameFeature module.
- **`Shrink_Yard` map:** oversized-world theme (desk / kitchen / backyard) where everyday objects are terrain — traversal that's trivial at full size is epic when shrunk. Design for both scales simultaneously: sightlines and cover must read at 0.20 and at 1.75.

### 8.4 Mode framing & item rarity (RESOLVED)
- Chaotic 4v4–8v8 party mode (teams or FFA variant).
- **Extraction stays universal — including `Shrink_Yard`.**
- **Rarity split:** on normal maps the Shrink kit (Shrink Gun / Large Health Booster / Special Weapon) is an **ultra-rare, top-tier loot drop** — one of the highest-value finds in the game, a major risk/reward swing when one appears. On `Shrink_Yard` it is the **baseline kit** everyone has (it is the whole experience, so it is common there, not rare).

---

## 9. Tracker reconciliation (the delta)

| Pillar | Was | Now |
|---|---|---|
| **P-MATCH** | "PARTIAL", ~20-player ceiling, modes vague | Full ladder: 1v1–8v8 + 18/36 BR; ranked pools; offline/bots explicit; tournament layer |
| **P-MAPS** | "LAST", 6 arenas, "operator details to add" | 10-map tiered roster (A–D + Shrink); greybox-telemetry loop formalized; per-tier density targets |
| **Netcode gate** | 20-player stress (AFL-1401) | 20-player core gate **+** 36-player follow-on gate for Phase B (AFL-1401/02/03 extended) |
| **Sensory map** | — | Parked (recorded, not designed) |

---

## 10. Resolved decisions

1. **Arena win conditions** — RESOLVED: proven-popular round-based best-of, win-by-eliminate-or-extract (full detail in §1.1). Exact counts tuned by telemetry.
2. **BR collapse × extract interplay** — RESOLVED: **strand** (a closing zone hard-strands a cold extract — creates rotation tension). Tune against extract-success telemetry.
3. **Adjacent-band level sharing** — RESOLVED to process: **data-driven per §6 density telemetry under engineering best practice.** No pre-commitment; the §6 loop decides which Tier B/C levels host 2 sizes vs 1 once density data lands.
4. **Shrink kit extraction & rarity** — RESOLVED: extraction stays universal everywhere including `Shrink_Yard`; Shrink kit is ultra-rare top-tier loot on normal maps, baseline on `Shrink_Yard` (see §8.4).

---

## 11. Per-map design gate (mandatory)

**No map enters greybox build until its detailed per-map design brief exists and is operator-approved.** Each map is designed in full against this spec *first*. The brief is the per-map `✅ watched in PIE` precondition — it is to a map what a sibling-diff is to an ability.

Order of operations per map: **design brief → operator approval → greybox build → telemetry playtest (§6) → balance → art pass → PIE sign-off.** A re-sent brief is not an approval; disk state is verified before build.

### Per-map design brief template
Each brief (`Docs/maps/<MapName>_DESIGN.md`) must specify:

1. **Identity** — name, tier (A/B/C/D/special), sizes + `LyraExperience` variants it hosts, source (net-new / Arena_0x).
2. **Footprint & density** — playable area (m across), target players-per-area, **TTFC target** for each size hosted.
3. **Flow** — lane/loop diagram (three-lane / figure-8 / POI graph); no dead-ends; rotation routes between key points.
4. **Power positions** — each high-ground/strong angle listed **with its flank/counter-route**. No uncontested map-spanning sightline.
5. **Sightline bands** — where CQB / mid / long engagements live (must exercise the laser roster).
6. **Extraction** — zone count + placement + payout tier per §2; for BR, the collapse interaction (strand per §10.2).
7. **Spawns** — layout, team-aware selection, anti-camp, ≈2.75s `Gameplay.DamageImmunity` (GE_SpawnIn, montage-tied); BR drop distribution.
8. **Signature mechanic** — the map's hook (laser walls / gravity / storms / rails / shrinking zone / oversized-world), with its server-authority + replication note.
9. **Symmetry** — mirror / rotational / asymmetric, with the integrity rationale.
10. **Readability** — beam + silhouette readability honored (rim light, neon accents at sightlines, low ambient — no laser lost in env color).
11. **Telemetry hooks** — which §6 heatmaps validate this map; the greybox exit criteria (the specific metric windows that must be hit before art).

### Build order (start point)
Per §7 core-first: begin briefs with **Tier B (3v3/4v4)** to validate the telemetry loop end-to-end on the most-played heart, then Tier A, Tier C, then Phase B (BR_18 → BR_36) and `Shrink_Yard`.

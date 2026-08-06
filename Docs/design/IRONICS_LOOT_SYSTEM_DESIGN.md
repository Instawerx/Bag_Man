# IRONICS — In-Match Loot System Design (v3)

**Status:** DESIGN — locked for the first build session. Build per the phased plan; review diffs before each build.
**Scope (operator-locked):** IN-MATCH PHYSICAL loot ONLY. Every grant is DETERMINISTIC / known-value.
**NO randomized / gacha / mystery-box anything** — forbidden by `IRONICS_ECONOMY_SPEC.md` §0. Never designed here.

**Version history:**
- v1 — generalized the proven loot-box; 4-axis design space; phased plan.
- v2 — folded: on-death = existing energy ring + dismember (no new bounty); per-cache mode flag; ammo/health as a 2nd value-domain.
- **v3 (2026-06-17)** — locks 4 answers: **Q1 MIXED team-gating** (most anyone-loots; a team's own dropped caches team-gated);
  **Q2 RESOURCE NODES = BASELINE** (harvest-over-time, a NEW 3rd retrieval pattern, its own built phase);
  **Q3 ADDITIVE resource loot** (ammo/health ADD alongside the existing model, no rework); **Q4 FULL WEAPON SKU loot**
  (caches can drop `AFL.Weapon.*` you equip — decided but **WEAPON-ARC-DEPENDENT**, lands with/after the weapons arc).

> One-line frame: **a loot object is a server-authoritative, replicated world thing that, when an eligible actor
> retrieves it, grants a KNOWN value and is consumed (or yields over time).** Most of it exists; we generalize the
> proven siblings (head loot-box + energy pickup) — never a parallel system.

---

## STEP 1 — GROUND TRUTH (what exists on disk — read, not assumed)

### Proven retrieval substrates + value models
1. **Dismember loot-box (CARRY template)** — ✅ committed `9ac3e0ae`. `AAFLHeadLootBox`/`AAFLDismemberedLimb`:
   persist + `UAFLGrabbableComponent` + server-auth `OnGrabbedBy` owner-branch + `Initialize(owner,[zone,]watts)` +
   owner-death-vanish + grant-once guard. Owner→`RestoreZone` reattach (no Watts); enemy→`EarnWattsAuthority` (160/20).
2. **Grab substrate (CARRY)** — ✅ `UAFLGrabbableComponent`(`IInteractableTarget`)+`UAFLInteractionComponent`+`GA_AFL_Grab`.
   Deliberate input → reach → attach `hand_r` → carry/throw/drop, stealable mid-carry. `OnGrabbedBy` = grant seam.
3. **Energy loop (INSTANT template)** — ✅ `AAFLEnergyPickup` (overlap+magnet → `CarriedEnergy` → destroy; tiers 10/25/50)
   + `UAFLEnergyDropComponent` (death → 70% tier-mixed ring). A second retrieval mode: low-friction, consumable.
4. **Extraction (the SINK)** — ✅ `AAFLExtractionZone` + `UAFLAG_Extract` (6s channel → `CarriedEnergy×10×Mult →
   EarnWattsAuthority`). Energy = carry-to-extract loot; combat-loot = instant Watts. Risk/reward axis.
5. **Economy spec** — peg 10 W=1 V=$0.001; ~4,000 W/match; head 160/limb 20; 10 W/energy. **§0 NO RANDOMIZED ACQUISITION.**

### Net-new (nothing yet)
- No map-placed caches / **resource nodes** / crates. No **shared loot base/interface** (head/limb/energy COPY-PASTE
  their grant — the generalization opportunity). Lyra `ALyraWorldCollectable`/`IPickupable` EXIST but UNUSED (the
  gameplay-resource hook). No per-map loot config/director. **No HARVEST-over-time retrieval** (Q2 net-new behavior).

---

## STEP 2 — THE GENERALIZED LOOT SYSTEM

### 2.1 Two VALUE-DOMAINS, then 4 axes

**Value-DOMAINS:**
- **ECONOMY** — Watts/energy. Deterministic currency, pegged to the spec, counts vs the ~4,000 W/match budget.
- **GAMEPLAY-RESOURCE** — ammo/health/weapons via Lyra `IPickupable`. NOT currency (no peg/budget impact); affects
  moment-to-moment combat balance → its own design + balance pass. **ADDITIVE (Q3):** adds alongside the existing
  health/ammo model, does not replace it.

**The 4 axes — every category is a point here:**

| Axis | Options |
|---|---|
| **Retrieval mode** | **CARRY** (grab — deliberate, contestable, throwable) · **INSTANT** (overlap+magnet — immediate) · **HARVEST** (channel-over-time yield — Q2, NET-NEW 3rd pattern) |
| **Value model** | ECONOMY: Instant-Watts · Carry-to-extract energy · Reattach/no-grant (owner-branch) — — GAMEPLAY-RESOURCE: Ammo/Health via `IPickupable` (net-new bridge) · **Weapon SKU `AFL.Weapon.*` (Q4 — WEAPON-ARC-DEPENDENT)** |
| **Eligibility (Q1 MIXED)** | **Anyone** (most loot) · **Enemy-only** (owner-branch, dismember) · **Team-only** (a team's own dropped caches) — team resolution mirrors the dismember owner-branch controller/team check |
| **Lifetime** | Persist-until-looted · Timed despawn · Owner-death-vanish · **Regenerating** (nodes) |

Both CARRY/INSTANT + the three economy value-models are PROVEN. Net-new: HARVEST mode (Q2), the `IPickupable` bridge
(Q3), team-only eligibility (Q1), weapon-SKU value (Q4, deferred to the weapons arc).

### 2.2 The generalized PATTERN (shared core — mostly lifting existing copy-paste)

- **`IAFLLootable`** — the contract any loot object honors regardless of base class (interface avoids reparenting
  head/limb/energy/caches): `GetLootValue` · `GetValueModel` · `IsEligible(Retriever)` · `OnLooted(Retriever)` ·
  `GetLifetimePolicy`. Mirrors `IInteractableTarget`/`IPickupable`.
- **`UAFLLootGrantComponent`** — value-grant + **eligibility branch (anyone/enemy/team — Q1)** + grant-once guard,
  lifted from head/limb `OnGrabbedBy`. Dispatches per value-model: `EarnWattsAuthority` / energy GE / `RestoreZone` /
  **`IPickupable` bridge (ammo/health, Phase 4)** / weapon-SKU (weapons arc). A loot object = retrieval substrate +
  this component + a config.
- **Retrieval substrates:** `UAFLGrabbableComponent` (CARRY, existing) · `UAFLOverlapCollectComponent` (INSTANT,
  lift from energy) · **`UAFLHarvestComponent` (HARVEST-over-time, NET-NEW, Q2)**.
- **Server-authoritative + replicated** — spawn/eligibility/grant/consume server-side; objects replicate
  (`DORM_DormantAll` at rest). The dismember + energy work proved the shape.

### 2.3 THE CATEGORIES

| Category | Domain | Mode | Value model | Eligible (Q1) | Lifetime | Status |
|---|---|---|---|---|---|---|
| Dismember loot | Economy | CARRY | Instant-Watts / Reattach | Enemy-only | Owner-death-vanish | ✅ DONE (9ac3e0ae) |
| Energy pickups | Economy | INSTANT | Carry-to-extract | Anyone | Timed/persist | ✅ DONE |
| Energy drop-on-death | Economy | INSTANT (auto) | Carry-to-extract | Anyone | — | ✅ DONE |
| **On-death value model** | Economy | — | = energy ring + dismember (no new bounty) | — | — | ✅ CONFIRMED |
| Arena ECONOMY caches | Economy | **per-cache flag** | Instant-Watts / energy bundle | Anyone / **team-only** | Persist-until-looted | 🔴 Phase 3 |
| **Resource nodes** | Economy | **HARVEST** (Q2) | Carry-to-extract (timed yield) | Anyone (contestable) | Regenerating | 🔴 **Phase 4 (BASELINE)** |
| Gameplay-resource loot | Gameplay-resource | per-cache flag | Ammo/Health `IPickupable` (Q3 additive) | Anyone / team | Persist/timed | 🔴 Phase 5 (own balance) |
| Weapon-SKU loot | Gameplay-resource | CARRY/INSTANT | `AFL.Weapon.*` equip (Q4) | Anyone | Timed | 🔵 WEAPON-ARC-DEPENDENT |
| Extraction zone (SINK) | Economy | n/a | energy → Watts | In-zone | Map-placed | ✅ DONE |

Per-category notes:
- **Dismember** — shipped; first instance + owner-branch. 160/20 (spec §3a).
- **Energy + drop-on-death + on-death model (Q1/#1)** — shipped. On-death value = the energy ring (70%) + dismember
  loot, TOGETHER. **No separate Watts bounty**; budget math intact.
- **Arena ECONOMY caches (Phase 3)** — placed containers, KNOWN economy bundle. **Per-cache mode flag:** INSTANT
  (safe, spawn-adjacent supply) or CARRY (high-value carry-to-extract, contested zones). **Eligibility (Q1):** mostly
  anyone; a mode can mark a cache **team-only** (a team's own dropped cache) — the config carries the flag, the
  grant component's eligibility branch enforces it via the controller/team check (mirrors the owner-branch).
- **Resource nodes (Phase 4, BASELINE per Q2)** — a contestable, **regenerating** point you **HARVEST over time**
  (channel → yields energy/Watts per second while held/occupied). **A NEW retrieval pattern** (`UAFLHarvestComponent`),
  distinct from INSTANT (one-shot) and CARRY (haul). Deterministic yield/sec. Net-new C++, its own phase. Moved OUT of
  "optional" INTO the core build.
- **Gameplay-resource loot (Phase 5, ADDITIVE per Q3)** — ammo/health via the net-new `IPickupable` bridge. ADDS a
  loot source **alongside** the current health/ammo model (no rework → lower balance risk). Distinct value-model, its
  own balance pass (amounts/scarcity). Deterministic (known heal/ammo, no rolls).
- **Weapon-SKU loot (Q4, WEAPON-ARC-DEPENDENT)** — caches drop a real `AFL.Weapon.*` you pick up + equip. **Ties into
  the WEAPON SYSTEM arc** (needs weapon SKUs as droppable/equippable items) → lands **with/after the weapons arc, NOT
  in the immediate build.** Recorded as decided; the value-model slot is reserved in `UAFLLootGrantComponent`.
- **Extraction zone** — the SINK; not a loot object.

### 2.4 PER-MAP BASELINE (Phase 2)
- **`DA_AFL_LootConfig`** — per-mode manifest: enabled categories; cache list (each: **retrieval-mode flag**,
  **value-domain**, **eligibility incl. team-only (Q1)**, known contents, spawn tag); resource-node list (yield/sec,
  regen); drop-on-death rules (= the energy ring); extraction-zone count. **All deterministic** (the asset IS the
  known-value source).
- **`UAFLLootDirectorComponent`** (GameState, experience-granted via the proven `AddComponents` path) — server-auth at
  match start: reads the active config, spawns caches/nodes at tagged spawn points (`Loot.Spawn.*`), confirms
  drop-on-death + extraction zones. A new arena inherits loot via experience + config + tagged points. Zero C++ per map.

### 2.5 ECONOMY PEG + resource carve-out
ECONOMY values pegged to the spec (dismember 160/20; energy 10 W/ea, tiers 10/25/50; caches & node yields known Watts/
energy in the config, sized vs ~4,000 W/match). GAMEPLAY-RESOURCE amounts are **balance values, NOT economy** — no
peg/budget impact. Integer Watts, never float.

### 2.6 NO-RANDOMIZED INVARIANT (both domains, by construction)
Every grant deterministic/known before retrieval — economy (fixed per zone/tier/cache/node) AND gameplay-resource
(known heal/ammo). No random contents, rolls, rarity tables, mystery boxes, or purchasable chance. Zero
purchased-chance surface.

### 2.7 ROLLING HEADS — the QUANTUM collectible loop (core gameplay; re-homed from the tracker Rolling-Heads amendment)
Rolling Heads is a **CORE GAMEPLAY / loot feature** (P-COMBAT × P-ECONOMY × P-SCORING), **NOT** a store/economy
randomization item. It rides ON TOP of the proven S4 dismember detach (the CARRY loot-box template §2.3 + tracker
`AFL-DECAP-LOOP` ✅ PIE-proven) and does NOT expand S4 scope.
- **LOOP (P-COMBAT):** headshot → head pops (✅ existing, `AFL-DECAP-LOOP`) → the head becomes a **QUANTUM node**
  persisting the WHOLE match → robot headless-until-death (existing dismember penalties only, NO new debuff) →
  respawn with a new head → repeat.
- **COLLECT + SCORING (P-SCORING, ◻ recorded design):** collecting an **ENEMY** QUANTUM (anti-farm: enemy-only,
  mirrors the dismember owner-branch) awards a **"Head Collector" stat tick AND a tiny Watts trickle** (both).
- **ECONOMY (P-ECONOMY, ◻):** the enemy-collect yields a small **free/EARNED** Watts reward (~50–150,
  TUNE-AT-PLAYTEST — a trickle, not a faucet vs ~4,000 W/match). This IS the free/earned gameplay variance the
  **§2.6 / §0 no-PAID-randomized-acquisition invariant PERMITS** — it is **not** a paid loot-box (the legal gate is
  PURCHASED chance only).
- **HELMET-CARRY SEAM (◻, 4b × P-COMBAT):** the popped head must **carry the catalog-resolved helmet** (headshot →
  head pops → helmet rolls with it); gated on grounding the detach model (prop-swap vs bone-physicalize). The helmet
  part is NOT "complete" until this is watched.
- **NAMING:** QUANTUM = the collectible reward-object category (the rolling head is the FIRST). "Quantum Boxes" = a
  forward slot, not built. "Loot box" is a player-facing JOKE only — NEVER the mechanical/cert-scannable name.
- *(Re-homes the tracker QUANTUM/Rolling-Heads amendment; the standalone `ROLLING_HEADS_QUANTUM_AMENDMENT.md` was
  never created — this §2.7 is its home. Cross-ref the dismember mechanic: `AFL-0402` / `AFL-DECAP-LOOP`.)*

---

## STEP 3 — PHASED BUILD PLAN (v3)

Each phase clones the proven pattern; net-new C++ flagged. Every ✅ = watched in PIE (2-client where networked).

### Phase 0 — DONE
2 templates (CARRY dismember, INSTANT energy) + extraction sink + **on-death model confirmed (Q1/#1: energy ring +
dismember, no new bounty)** + grab substrate + wallet + economy spec.

### Phase 1 — GENERALIZE (extract the shared core; refactor, NO new behavior) — build-gated C++
NET-NEW: `IAFLLootable` + `UAFLLootGrantComponent` (value-grant + eligibility + grant-once, lifted from head/limb) +
`UAFLOverlapCollectComponent` (lift the energy overlap). REFACTOR: head/limb `OnGrabbedBy` → grant component; energy →
overlap component. **✅ = the existing dismember 4/4 + energy-collect watches STILL PASS** (behavior-preserving).

### Phase 2 — PER-MAP BASELINE — C++ + data
NET-NEW: `UAFLLootDirectorComponent` (GameState, experience-granted) + `DA_AFL_LootConfig` (carries the Q1 eligibility/
team-gating, per-cache mode flag, value-domain, node fields from the start). DATA: experience grants the director +
a baseline config + tagged spawn points. **✅ = a map with the director + config spawns the configured baseline at
start; director logs the map's total known ECONOMY loot value; it ATTACHES at experience-spawn (no residency miss).**

### Phase 3 — ARENA ECONOMY CACHES (Watts/energy) — C++ + data
CLONE: a cache = `UAFLGrabbableComponent`(CARRY) or `UAFLOverlapCollectComponent`(INSTANT) per the per-cache flag +
`UAFLLootGrantComponent` + `IAFLLootable`. DATA: cache defs in the config (known contents, mode, eligibility, spawn).
**✅ = a placed cache (both modes) is looted → grants its KNOWN economy value → consumed → replicates; team-only
cache rejects the wrong team (Q1).**

### Phase 4 — RESOURCE NODES (HARVEST-over-time, BASELINE per Q2) — build-gated C++ + data
NET-NEW: `UAFLHarvestComponent` — the 3rd retrieval pattern (channel/occupy → yield energy/Watts per second while
harvested, deterministic yield/sec, regenerating, contestable). + node defs in the config. **✅ = harvesting a node
yields the known per-second value; contestable; regenerates on its timer.**

### Phase 5 — GAMEPLAY-RESOURCE LOOT (ammo/health, ADDITIVE per Q3) — C++ (inventory bridge) + data + BALANCE PASS
NET-NEW C++: the `IPickupable` bridge in `UAFLLootGrantComponent` (wire Lyra `IPickupable`/`ALyraWorldCollectable` →
grant ammo/health; touches inventory/equipment) + the gameplay-resource value-model. ADDITIVE (no rework of the
current model). BALANCE PASS (its own design: amounts/scarcity, pickup-vs-regen). **✅ = a resource cache grants its
KNOWN ammo/heal via the inventory system; replicates; deterministic.**

### WEAPON-ARC-DEPENDENT (Q4, not scheduled here) — weapon-SKU loot
Caches drop `AFL.Weapon.*` you equip. Builds **with/after the weapons arc** (needs droppable/equippable weapon SKUs).
The value-model slot is reserved in `UAFLLootGrantComponent`; no build until the weapons arc provides the SKUs.

### Optional later — team-gating depth, contested-zone polish
Team-only eligibility ships as a config flag in Phase 2/3 (Q1); deeper team mechanics (escrow, contest UI) are later.

### Build-gated (C++) vs data/BP
- **C++:** interface + grant + overlap components (P1); director (P2); cache base (P3); **harvest component (P4)**;
  `IPickupable` bridge + resource value-model (P5); weapon-SKU value (weapons arc).
- **DATA/BP (no build):** `DA_AFL_LootConfig`, all cache/node/resource defs, mode + eligibility + domain flags, spawn
  tags, per-map experience wiring, all KNOWN values.

---

## OPEN QUESTIONS (post-v3)
All four major design questions are RESOLVED (Q1 mixed team-gating · Q2 nodes baseline · Q3 additive resource · Q4
weapon-arc-dependent). Remaining are per-phase tuning, not design blockers:
- Phase 4 node yield/sec + regen-rate tuning (vs the ~4,000 W/match budget).
- Phase 5 ammo/heal amounts + scarcity (the balance pass).
- Team-gating depth beyond the eligibility flag (later, if a mode needs escrow/contest).

---

*Grounded in a read-first survey (2026-06-17): `AAFLHeadLootBox`, `AAFLDismemberedLimb`, `UAFLGrabbableComponent`,
`UAFLInteractionComponent`, `UAFLAG_Grab`, `AAFLEnergyPickup`, `UAFLEnergyDropComponent`, `AAFLExtractionZone`,
`UAFLAG_Extract`, `UAFLWalletComponent`, Lyra `IPickupable`. Economy pegs to `IRONICS_ECONOMY_SPEC.md`;
gameplay-resource amounts are deterministic balance values. Zero randomized-acquisition surface in either domain.*

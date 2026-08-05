# ShantyTown — Battle Royale Map Design & Scope (v1)

**Status:** SCOPE / DESIGN. **District Program OPERATOR-APPROVED 2026-08-05** (see **Gate**); the BR mode layer remains gated (Zone system + §9). **Date:** 2026-08-05.
**Roster slots filled:** `BR_18` / `BR_36` (design-only in [MAP_DISPLAY_NAME_REGISTRY.md](MAP_DISPLAY_NAME_REGISTRY.md)).
**Source pack:** `Content/ShantyTown/` (Landscape environment). **Demo map:** `Content/ShantyTown/Maps/Demo_Map.umap`.
**Primary users (the driving constraint):** **Staked Battle Royales & Tournaments** — every choice below is filtered through "real stakes are on the line, assume incentive to cheat/grief."

---

## 0. Phase 0 — MEASURED (2026-08-05, in-editor via bridge)
Demo_Map already open (540 actors). Measured live:

| Measurement | Value |
|---|---|
| **Full landscape** (BR_36 envelope) | **617 m × 607 m**, Z 2816–6501 (37 m relief) |
| **Town play-core** (28 buildings — BR_18 candidate) | **357 m × 302 m**, Z 3365–5209 (18 m relief), center ≈ (1774, 11958) |
| **Robot height** (`SKM_Manny`) | **181 cm** → knee ≈ 51 cm, mid-thigh ≈ 81 cm |
| **Water short-out threshold** | **~55–65 cm** submersion (tunable; confirm in PIE) |
| **Ocean** | single flat plane at **Z = 3372**; depth **0 → ~5.5 m** (terrain floor 2816) |
| **Harbor/docks** | boat decks mostly at sea level (~3230–3380) + small elevated pool (~4030–4060); blockades ~4200 |
| **Underwater PostProcess volume** | present (intended submersion region) |
| **Player starts** | only 2 (demo) — BR needs 18/36 drop/spawn distribution (P2) |

**Verdicts:**
1. **Scale = PASS (not a GOLD-BANKS repeat).** Town core **357 × 302 m** (vs ARCANEON 140 × 90 m) is BR-appropriate; assets are realistic shanty scale, correct against a 181 cm robot. Town core = **BR_18**; full landscape = **BR_36** envelope with ring headroom.
2. **Lethal-depth rule self-seals the sea.** Ocean up to **5.5 m** deep vs **~0.65 m** threshold → only a thin shoreline is wadeable; essentially the whole sea is instant short-out → diegetic death boundary, no visible wall needed (§3B/§4 confirmed by data).

**Phase 0 status:** ✅ complete — scale + water depth measured, go/no-go = **GO**. Next: P0.5 (BR-mode spike) / P1 (boundary + water-volume authoring), pending the deferred decisions in §9.

---

## 1. What ShantyTown is (asset recon)
Unlike ARCANEON (a kit-walled arena), this is a **Landscape-based open environment**:
- **Terrain:** Landscape with ground/puddle layers, grass/foliage, backdrop **mountains** (natural wall).
- **Water:** river + `MI_River_01`, `MI_Water_Blockage_01`, and the pack's own **`BP_Water_Blockade_01`** for walling water edges.
- **Buildings (loot/CQB):** modular shanty — brick/metal/wood walls (8th/quarter/half/full), doors, windows, roofs, chimneys, shelters, stairs, ladders, floors.
- **Docks & boats:** `SM_Boat_01`, dock walkways/archway/shelter, `SM_Boat_Dock_01`.
- **Perimeter-fence kit (key for containment):** chain-link (+broken), **barbwire tops**, metal/palm/wood fence, low brick wall.
- **Cover/debris:** bricks, concrete blocks, **tyre walls/stacks**, wood bundles; drainage gutters.

**Implication:** containment is a *landscape* problem, not tile-sealing — but the pack ships the exact diegetic props (compound fence + water blockade) for a fenced-compound edge.

## 2. BR mode status — last-standing SHIPPED (C++); shrinking Zone still net-new
**Corrected 2026-08-05.** The earlier "no BR ruleset exists / prerequisite SYSTEM" framing is **out of date**:
- **`UAFLBattleRoyaleComponent` is committed (`e6e0ffa0`)** — server-authoritative **N-participant last-standing + placement**, **no-respawn baseline** (`State.Round.NoRespawn` + `IAFLRoundRestartPolicy`). Built on the existing match spine (`UAFLMatchPhaseComponent`), `MatchId`, join coverage, loot carry, and telemetry.
- **PIE-proven TWICE** on assertions **A1** (match start + participant count), **A2** (descending placement ladder), and **A4** (no-respawn holds).
- **STILL UNPROVEN — owed:** **A3 / A5** and **match conclusion**. Two PIE runs **stalled before resolving**, and the **cause is UNKNOWN** — an earlier team-split explanation was **retracted as unsupported**. Recorded as unknown, **not solved**.
- **The genuinely net-new BR work that remains is the shrinking Zone system** (§3, Layer A) — nothing like it exists.

So the BR ruleset is **no longer a from-scratch prerequisite**: last-standing / placement / no-respawn are shipped and partially proven. What's left is the **Zone system** plus **closing out A3 / A5 / match conclusion**. (Track handling = open decision, §9.)

## 3. Boundary containment — two-layer, hybrid model
Both layers required; both **server-authoritative**.

### Layer A — Dynamic play boundary (ring/zone)
- Shrinking safe-zone, telegraphed phases, damage-over-time outside scaling per phase.
- **Server-authoritative, deterministic, seeded per match, and logged** — non-negotiable for staked fairness + dispute replay. (New system — see §2.)

### Layer B — Hard world boundary (map edge) — hybrid
- **Water sides:** sealed by the **lethal-depth water rule itself** (§4) — deep water shorts you out, so the ocean is a self-sealing, diegetically-justified death boundary; **no visible wall needed** over deep water. Add a far **KillZ/OOB** catch for anyone launched far out, plus anti–edge-stall handling.
- **Land sides:** perimeter loop of the **compound-fence kit** (chain-link + barbwire = reads as unclimbable) as the diegetic edge; **mountains as natural wall** where terrain rises; backed by a **continuous invisible `BlockingVolume`** — double-sided, taller than the max reach stack (jump + dash + vault + explosive-launch), **zero seam gaps**; a **ceiling cap** so nothing clears the fence; a **KillZ / OOB volume** beyond and beneath the terrain.

### Verification discipline (from the ARCANEON sealing saga)
- **Data-driven:** sample all navmesh + every reachable ledge/roof/dock/boat-deck; assert reachable set ⊆ boundary polygon.
- **Actively test dash / vault / grenade-jump at every seam.** Operator PIE is ground truth.
- **Rule:** *"If a player can leave the play area anywhere, it is not playable AAA."*

## 4. Water interaction — v1 LOCKED: "Robot short-out by depth"
**Rule:** We are robots. Wading past **~knee/mid-thigh depth** = short-circuit → **instant death → respawn on land**. Below that depth = freely wadeable. Applies to **every** water body (ocean, river, puddles, drainage) — one depth threshold governs all of it (puddles harmless; deep river channel + ocean lethal).

**Why it's strong for a staked map:**
- **Deterministic & self-sealing** — the ocean becomes a natural lethal boundary (see §3B), diegetic *and* exploit-resistant.
- **On-theme** — short-circuit death = electric-spark VFX/SFX, on-brand for the IRONICS neon/electric identity.

**Implementation for tournament determinism:**
- **Server-authoritative authored depth volumes** — `Water_Lethal` over all deep areas, optional `Water_Wade` for safe shallow margins, placed to match each body's bathymetry. Server-side overlap check, zero client trust. (Chosen over dynamic water-surface-vs-leg-socket math because authored volumes are verifiable and cannot desync.)
- **Threshold calibrated in Phase 0 (measured):** robot = 181 cm → knee ≈ 51 cm, mid-thigh ≈ 81 cm → short-out trigger **~55–65 cm** submersion (final value confirmed in PIE).
- **Respawn-on-land at nearest shore, disadvantaged** — never a free cross-map teleport; short vulnerable/penalty state so water is a real cost, not fast-travel, stall, or escape.
- **Open sub-point:** what a water-death *costs in BR terms* (respawn vs. life lost vs. downed) depends on the BR ruleset (§2) — tied to that track, not guessed here.

**Deferred:** shallow-wade movement debuff (Option B extra) and full swim traversal (Option C — needs swim locomotion that doesn't exist; underwater camping/desync risk) are **not** in v1.

## 5. Boats — v1 LOCKED
- **Boardable, not drivable.** Static, anchored platforms with proper walkable deck collision. Placed over deep (lethal) water they become **risk/reward islands + vantage/loot spots** — step off the deck into deep water and you short out. Docks/walkways link shore → boats.
- **Drivable boats = V2** (needs a vehicle/buoyancy system that doesn't exist) — parked.

## 6. Staked & Tournament requirements (main-user lens)
- **Server-authoritative everything** — boundary, ring center/timing, water lethality, loot spawns. Zero client trust.
- **Deterministic & reproducible** — seeded ring + loot per match, full server log, spectator/replay for dispute resolution + VOD.
- **Fair start** — even drop/deploy distribution, symmetric loot density by region tier, no spawn-camp, no dominant unreachable perch.
- **Anti-abuse** — teaming/collusion detection, stream-snipe mitigation (spectator delay), disconnect/rejoin + AFK/grief policy tied to stake handling, no boundary/water exploit that stalls a staked match.
- **Integrations** — Match/Staking SSOT (buy-in in **non-cashable Watts/Volts, NO real-money cashout**, placement-weighted payout), League/Advancement (rank/ARC from placement), tournament bracket/lobby/spectator, and a BR playlist DA + experience wired like ARCANEON (**codename discipline — never rename DA/MapID**, per the host-resolution saga).
- **Performance at 18/36 players** — landscape + foliage + N players → net-relevancy, Nanite/HLOD, HISM foliage, LOD, tick budgets, profiled at target count. Stutter = latency = unacceptable when money is staked.

## 7. Scale gate — Phase 0 (anti–GOLD-BANKS)
GOLD BANKS (`L_ValleyVillage`) was parked because map-asset scale was wrong for the character. **Before any art or wiring:** open ShantyTown in-editor, measure Landscape playable extent + building/character scale, and decide the **BR_18 sub-region footprint vs BR_36 full-map footprint**. **Go/no-go gate** — if scale is off for the character, rescale or reject *first*, not after a pass.

## 8. Phase pipeline (adapts the proven 9-phase art-pass, BR-front-loaded)
| Phase | Output |
|---|---|
| **P0** | Measure + scale go/no-go; calibrate water depth threshold to robot legs |
| **P0.5** | BR-mode spike — zone + last-standing + drop system (the prerequisite, §2) |
| **P1** | Boundary + water design lock (depth volumes, fence loop, invisible wall plan) |
| **P2** | Greybox BR play-zone — drop/loot/POI/ring path/spawns |
| **P3** | Nav + containment validate — dash/vault/nade seam tests, reachable ⊆ boundary |
| **P4** | Art pass (kits already in-pack — fence/buildings/docks/debris) |
| **P5** | Perf-at-N (18 & 36) |
| **P6** | Staking/tournament wiring + spectator/replay |
| **P7** | PIE at 18 & 36 + exploit sweep |
| **P8** | Sign-off + commit (personal only) |

## 9. Open decisions (deferred by operator — do NOT assume)
1. **First target size:** BR_18 (recommended — smaller/faster to stabilize) vs BR_36 vs both-in-parallel.
2. **BR-mode track:** scope it as its own doc (recommended) vs fold into this plan vs map-only-for-now.
3. **Water-death BR cost:** respawn / life-lost / downed — resolve with the BR ruleset (§2, §4).

## 10. Top risks
1. **BR mode conclusion unproven** — last-standing/placement/no-respawn are shipped + partially PIE-proven, but **A3/A5 + match conclusion are owed (cause unknown)** and the shrinking **Zone system** is still net-new; the largest remaining BR effort (§2).
2. **Scale** — GOLD BANKS repeat risk (§7, gated first).
3. **Landscape containment harder than a walled arena** — seams, terrain holes, roof/dock/boat perches (§3B).
4. **Water edge-cases** — sloped shorelines, threshold-standing, dock-to-deep jumps (§4).
5. **Perf at 36 players** on landscape + foliage (§6).

---

## 11. District Program (WP data layers) — the whole matchmaking matrix from one map
**OPERATOR-APPROVED 2026-08-05** (see **Gate**). ShantyTown covers the **ENTIRE matchmaking matrix from a single map** via **fenced play-spaces (districts)**, rather than one map per bracket.

**Architecture:** World Partition **data layers**, conforming to **`L_Expanse`** (the map INFINEON ships on), which uses **Layout / Gameplay / ExtraSpawn / Lighting**. [Arena_04_DESIGN.md:35](Arena_04_DESIGN.md) already names that organisation as inherited doctrine.
- **Base layers:** `Layout` · `Gameplay` · `ExtraSpawn` · `Lighting`
- **District layers:** `District_Duel` · `District_Arena` · `District_Team`

| District | Area | Footprint | Covers |
|---|---|---|---|
| **D1 Duel** | ~3,460 m² | ~59 × 59 m | 1v1, 2v2 |
| **D2 Arena** | ~7,500 m² | ~87 × 87 m | 3v3, 4v4 |
| **D3 Team** | ~14,000 m² | ~118 × 118 m | 5v5, 8v8 |

**Sizing basis (Block 162 ladder):** ~865 m²/player dense, ~1,400 m²/player sparse ceiling — calibrated from **NANOWATT 3v3 (850)** and **ARCANEON 8v8 (876)** agreeing within 3% across two independently authored maps. **D3 deliberately matches ARCANEON's 14,016 m²**, which hosts both 5v5 and 8v8 in production. Districts total **24,960 m² = 23% of the 357 × 302 m town core**. **BR still uses the whole map; districts are fenced regions INSIDE it, not carved out of it.**

**OWED — not decided:** district **PLACEMENT** within the town core. That needs eyes on the actual layout — natural fencing, building density, sightlines. **No coordinates proposed here.**

**Activation:** requires a small **server-only AFL `GameFeatureAction`** calling `UDataLayerManager::SetDataLayerRuntimeState`. Verified server-authoritative — `WorldDataLayers.cpp:152-159` refuses clients with `AuthoritativeFromClient`; state replicates outward. **Not yet written.**

---

## Locked v1 decisions (summary)
- ✅ **Water = robot short-out by depth** (past ~knee/mid-thigh, all water bodies, respawn on land, authored server-side depth volumes).
- ✅ **Boats = boardable, not drivable.**
- ✅ **Containment = hybrid** (water self-seals via lethal depth; land via fence + invisible wall + ceiling + KillZ).
- ✅ **Users = staked BR + tournaments** → server-authoritative, deterministic, exploit-free throughout.

---

## Gate
Per spec §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build.

**OPERATOR-APPROVED 2026-08-05 — DISTRICT PROGRAM.** This approval covers the **District Program only** (the WP data-layer district architecture + the D1/D2/D3 envelopes in §11). It **unblocks district authoring**. It does **NOT** approve the BR mode layer — that remains gated on the shrinking **Zone system** (§2, §3) and the deferred **§9** decisions. District **PLACEMENT** within the town core is still **OWED** (needs eyes on the actual layout). **Do not build geometry beyond the approved district program before its corresponding gate.**

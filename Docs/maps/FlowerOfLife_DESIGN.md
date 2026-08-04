# Flower of Life — Per-Map Design Brief (scalable tube-network map family)

**Status:** ✅ **§11 APPROVED WITH REQUIRED AMENDMENTS — Studio Lead, 2026-08-03 (baseline `447f63c8`).** Codename **METATRON** (player-facing: **METATRON ARRAY**). Build scope this sprint: **METATRON_19 → `Arena_05` greybox only** (07 = generator fixture; 37/61 = generator reports/manifests only). See the **LOCKED AMENDMENTS** block below — it **supersedes any conflicting value** in §1–§11.
**Tier:** B + C (and D-ready) — a **scalable map FAMILY**, one design template instantiated per tier by ring-count (see §2). · **Source:** net-new (operator concept, refs `flower of life ref 1/2`).
**Working name:** "Flower of Life" (codename TBD — operator names maps; proposals in §1).
**Purpose:** a **3-level tube/tunnel map built on Flower-of-Life geometry** — circular tunnels whose arcs you run **inside, outside, on top, and below**, with the enclosed circles/lenses as **battle courtyards** (staggered obstacles for height traversal), and the outer wall itself the same 3-level tubular design. The self-similar hex geometry lets **one kit** serve **3–4 sizes across the tiered ranges** — the size per tier set by §3 density (best-practice/data-science), not guessed.

> Grounded in `IRONICS_MAP_MODE_SPEC` §1/§1.1 (ladder + win), §2 (extraction), §3 (tier density — the sizing driver), §4 (roster — see §1 reconciliation), §6 (telemetry loop), §11 (this template). Uses the Arena_04/Duel_01 doctrine (dual-GE, traversal-as-design, §8 replication note, §11 existing-instruments). Numbers are **greybox-validate, not fixed law** (§3). **Mathematics/CAD applied in §2** per operator direction.

> **Design language (from the refs):** 6-fold hexagonal Flower-of-Life circle-packing + Metatron's-Cube connective lines (ref 1); a domed/spherical FoL read within a hexagram (ref 2). Neon-glass IRONICS house look (inherits ARCANEON's aesthetic). The pattern's **6-fold symmetry is a competitive-integrity gift** (see §9).

---

## ★ LOCKED AMENDMENTS — Studio Lead Decision, 2026-08-03 (supersede any conflicting value in §1–§11)

**Naming.** Family codename **`METATRON`** (player-facing **METATRON ARRAY**). Use `METATRON` for generator presets, docs, asset labels, test output, and map-family IDs (subject to the `AFL`/`BagMan_` asset-prefix convention where UE assets require it).

**Roster placement — FINAL (10-map roster intact):**
| Roster slot | METATRON preset | Status |
|---|---|---|
| `Arena_05` | **METATRON_19** | current build target |
| `BR_18` | **METATRON_37** | Phase B deferred |
| `BR_36` | **METATRON_61** | Phase B deferred |
- **`METATRON_07`** is authorized **only** as a generator-validation fixture / movement-nav test harness / density experiment / regression target. **Not a roster slot.** It must **not** displace `Arena_02`, `Arena_03`, `Duel_02`, or `Shrink_Yard`, and **no new Tier-B roster entry** is created.

**Build scope this sprint:** implement the generator; validate all four presets at generation level; **build/import only METATRON_19 → `Arena_05`**; complete collision/nav/traversal/density/orientation testing; **STOP before production art, final materials, decorative kitbash, or BR construction.** 37/61 = parameter manifests + validation reports only.

**Geometry amendments (override §2):**
- **Courtyard radius `R ≈ 15 m`** = accepted *initial* value, a **tunable generator parameter** (do NOT hard-code) until movement/encounter/weapon-range/density tests set the final.
- **Tube bore (CLEAR unobstructed interior after collision/trims/rails/fixtures):** **6 m minimum** standard playable tubes; **8 m** primary combat arteries + major junctions; **5–6 m** only for deliberate shortcuts / traversal / low-intensity flanks. Extra camera-clearance around tight curves + vertical transitions. (The earlier ~4 m bore is **rejected.**)
- **Exactly THREE readable strata (LOCKED):** **Undercroft `-6 m`** (flanks / recovery / lower-risk rotations) · **Courtyard+Tube Interior `0 m`** (primary combat + objective layer) · **Crown / Upper Bridges `+6 m to +8 m`** (exposed high-risk traversal + positional advantage). **No** hidden half-floors, decorative walkable ledges, accidental 4th level, or ambiguous vertical routes.
- **Vertical connectivity:** a meaningful vertical transition every **~25–30 m** of primary-route travel; **no major route > ~40 m** before an exit / courtyard opening / vertical transition / branch / recognizable landmark. Every stratum reconnects to the primary combat network via **multiple** routes.
- **Tube sightlines:** no uninterrupted enclosed-tube combat sightline materially exceeds **~40 m** — solved by staggered openings / curvature / lens intersections / courtyard breaks / elevation changes / partial occluders, **not** arbitrary cover clutter.

**Competitive layout doctrine (override §9/§10):** 6-fold symmetry is the structural foundation, but **visual symmetry must not become navigational sameness.** Preserve **geometric fairness** (opposing sectors equivalent in route length, elevation, cover, objective access, spawn-escape, vertical counterplay — never add gameplay asymmetry to look different). Add **visual differentiation** per sector: unique **color family + icon/glyph + landmark silhouette + persistent sector number/directional ID + wayfinding on floors/tube-interiors/crowns/undercroft** (color alone is insufficient — accessibility + combat lighting). **Level differentiation:** each stratum a distinct visual language (Undercroft heavier/darker/down-indicators; Primary strongest signage+objective readability; Crown sky/ceiling ref + hazard/exposed-edge markings). Target: a player IDs **both sector and vertical level from a single screenshot** without the map.

**Orientation anchor (override §10):** one unmistakable **central hero landmark** — visible from multiple courtyards + upper routes, establishes map center, gives vertical+directional reference, **must NOT be a dominant sniper tower**, preserves the FoL silhouette (suspended reactor / rotating geometric core / energy lattice). Orientation device first, decoration second.

**Outer boundary (override §3/§4):** the playable tubular outer wall is approved but **must not be an uncontested circular sniper track** — break/expose the outer crown at regular sector intervals; **≥2 attack directions against every meaningful outer-wall position**; repeated return routes inward; prevent continuous safe orbiting; **no high-ground with only one access path**; strong out-of-bounds readability. Every strategically useful crown position has **≥2 credible counters**.

**Dual-GE (override §8, non-negotiable):** using **Haywire/base movement alone**, players must reach every mandatory objective, traverse all three strata, enter/exit every primary courtyard, use every spawn-escape route, reach every gameplay-critical high-ground, and return from every lower route without forced self-elimination. **ProMod** may add faster routes / advanced lines / skill shortcuts / wall-run+vault combos / riskier crown transitions but **must not unlock exclusive mandatory space.**

**Generator architecture (new — see §8 amendment):** an **author-time deterministic production tool** (NOT runtime procedural). Params ≥ {ring count, base radius, tube bore by route class, stratum heights, courtyard opening dims, sector rotation, vertical-connector interval, sightline-break interval, outer-boundary treatment, seed (non-gameplay variation only), output preset id}. Presets: `METATRON_07/19/37/61`. **Identical inputs → identical geometry, transforms, names, manifests.** Emit a **machine-readable manifest** ≥ {preset name, generator version, input params, circle/cell count, bounds, cell-center transforms, tube-segment transforms, courtyard transforms, route classification, stratum assignment, vertical-connector locations, spawn-candidate locations, objective-candidate locations, est. mesh/instance counts, collision/nav-proxy counts, validation warnings/failures}. **Modular repeated meshes + instancing (ISM/HISM-ready), NOT one monolith** — output must be selectively replaceable / optimizable / collision-audited / art-passed; the generator must reproduce the map after deleting prior output.

**Collision & navigation (override §7 — carries the Duel_01 lesson):** Recast-critical surfaces use **nav-gatherable collision** — dedicated simple collision or nav-proxy geometry; **do NOT globally use Complex-As-Simple.** Walkable slopes within movement doctrine; no collision seams at tube joins; no invisible lips at courtyard thresholds; no camera-blocking collision beyond visible surfaces; no nav islands on required routes; deterministic validation of every connector; **separate render geometry from simplified nav/collision geometry.**

**Arena_05 (METATRON_19) encounter targets (Tier C, 5v5/8v8) — greybox tuning targets:** first meaningful engagement **~8–12 s**; spawn→central contestable space **~12–18 s**; **no direct spawn-to-spawn LOS**; **≥3 strategically distinct macro routes** between opposing sectors; **≥2 viable attacks** against any major high-ground; no required route as a single uncontestable choke; central pressure without the center being the only viable play; flanks **rejoin the fight** (not empty circulation).

**Revised §11 exit gates (LOCKED — greybox not art-approved until ALL pass):** deterministic generation of `19` from a documented preset; no overlapping/inverted geometry; no visible cracks at primary modular joins; every gameplay-critical space reachable with **base Haywire movement**; **no mandatory ProMod-only route**; no accidental 4th stratum; no traversal soft-locks/unrecoverable pits. **Nav:** Recast reaches all mandatory nodes; MoveProbe traverses all sectors×strata; no required route on complex render collision; no isolated nav islands on gameplay-critical surfaces; modular join boundaries don't cause recurrent path failures. **Combat:** no spawn-to-spawn LOS; no enclosed-tube sightline over the cap; every major high-ground has ≥2 counters; no dominant outer-ring orbit; no single courtyard controls all macro rotation. **Orientation:** 6 sectors identifiable **without relying on color alone**; each stratum distinct; central landmark orients reliably; MoveProbe/route testing shows no repeated confusion loops between look-alike intersections; mistaken-navigation backtracking is **recorded and tuned** before art lock. **Perf/production:** generator emits modular replaceable pieces; repeats ISM/HISM-suitable; collision/nav proxies separable from render; deterministic audit-friendly naming; generator reproduces after deleting prior output; all four presets pass generation-level validation; **only 19 imported** as the current Arena_05 greybox.

---

## 1. Identity
- **Name:** Flower of Life · **Codename:** TBD (operator). Proposals fitting the neon-sacred-geometry theme: **GENESIS · METATRON · VESICA · HELION**.
- **A scalable FAMILY, not one level.** The same modular FoL kit is instantiated as **separate tier-appropriate levels** (respecting spec §3/§4: non-adjacent bands never share a level; each instance hosts its adjacent bands):
  | Instance | Tier | Sizes hosted | FoL rings | Circles | Status |
  |---|---|---|---|---|---|
  | `L_FloL_B` | B | 3v3, 4v4 | 1 ring | 7 | core-first |
  | `L_FloL_C` | C | 5v5, (7v7↔8v8) | 2 rings | 19 | core-first |
  | `L_FloL_D18` | D | 18 (BR) | 3 rings | 37 | **Phase B** (netcode-gated, §7) |
  | `L_FloL_D36` | D | 36 (BR) | 4 rings | 61 | **Phase B** |
  - **"3–4 sizes"** = 3v3/4v4/5v5/8v8 across the two core-first instances (B, C); the BR instances are the same design **self-similarly expanded** (design now, build in Phase B).
  - Whether 5v5 and 8v8 share the one Tier-C instance or need separate density tuning is **decided by the §6 telemetry loop** (spec §10.3 resolved-to-process), not pre-committed.
- **`LyraExperienceDefinition` variants** (spec §5): per instance × size × ruleset. **Dual-GE** ([[project-ironics-dual-ge-requirement]]) — see §8 for why this map is **not** ProMod-gated (unlike Duel_01): its core traversal runs on base movement (jump/climb) present in **both** GEs; ProMod's vault/wall-run/slide *enhance* it but are not the sole key.
- **⚠ Roster reconciliation (operator call):** the spec §4 roster is fixed at 10. This family most naturally fills **Arena_05 (Tier C)** + **BR_18 / BR_36 (Tier D)**, plus a **new Tier-B slot** (or supersedes a pending Arena_0x). Which slots it occupies is an operator decision — flagged, not assumed.
- **Signature mechanic:** the **3-level Flower-of-Life tube network** (run inside/outside/on-top/below; staggered tube openings) — see §8.
- **Source:** net-new operator concept.

## 2. Footprint & density (mathematics / CAD — the sizing driver)
**Flower-of-Life geometry (the CAD basis).** Circles of radius **R** on a hexagonal lattice with **center spacing = R** (the defining FoL property: each circle passes through its 6 neighbours' centres). Ring structure: center (1) + ring-1 (6 at distance R) + ring-2 (12) + ring-3 (18) … The **k-ring flower's outer radius ≈ (k+1)·R**; **circle count = 1 + 3k(k+1)** (7, 19, 37, 61 …). Adjacent circles overlap in **vesica-piscis lenses** — those lenses are the natural **passages/openings** between courtyards.

**The constant gameplay UNIT (kept fixed so feel is identical at every size — the data-science key):**
| Unit | Starting value | Rationale |
|---|---|---|
| Courtyard radius **R** | **~15 m** (Tier B/C); ~scaled up for BR | a ~30 m courtyard reads as a CQB→mid battle space |
| Tube (tunnel) bore | **~4 m** Ø | two players abreast; runnable inside, wide enough to fight |
| Tube wall thickness | ~1 m | run **on top** (the arc crown) as a 1-m-wide catwalk line |
| Tier height (each of 3 levels) | **~6–7 m** | one storey; a climb/vault reads as one level (matches ARCANEON's ~8 m) |
| Total vertical | **~20–22 m** (3 levels + crown running) | run below / inside / on-top across 3 stacked FoL sheets |

**Per-tier footprint (constant R≈15 m; outer Ø ≈ 2(k+1)R):**
| Instance | rings k | outer Ø | vs spec §3 band | TTFC target |
|---|---|---|---|---|
| B (3v3/4v4) | 1 | ~**60 m** | Tier B 60–100 m ✓ | 8–15 s |
| C (5v5/8v8) | 2 | ~**90–100 m** | Tier B/C boundary → nudge R to ~18 m for ~110 m if C needs more room | 12–25 s |
| D-18 (BR) | 3 | ~120 m at R15 → **scale R to ~55 m for ~440 m** | Tier D 400–600 m (needs R scale-up, not just rings) | drop-dependent |
| D-36 (BR) | 4 | **scale R to ~70 m for ~700 m** | Tier D 600–900 m | drop-dependent |

- **Density rule (best-practice):** target **~1 player per courtyard-circle at the core tiers** (7 circles ≈ 6–8 players = 3v3/4v4; 19 circles ≈ 12–16 = up to 8v8). Ring-count is chosen so `circles ≈ player-count` — **the §6 telemetry loop tunes this** (if TTFC/density are off, add/remove a ring or nudge R). **BR needs R scaled up** (rings alone stay too dense) — a Phase-B tuning task.
- **Density:** high inside tubes (CQB), medium in courtyards — the tube/courtyard alternation gives a built-in density rhythm.

## 3. Flow
- **The network IS the flow.** Circular **tubes** (the FoL arcs) are the rotation arteries; **courtyards** (circle interiors + vesica lenses) are the engagement rooms; **3 stacked levels** give the vertical. You can be **inside a tube** (covered rotation), **on top of a tube** (exposed running line / the arc crown), **outside/between tubes** (courtyard floor), or **below** (the level beneath). This is the operator's "run inside, outside, on top, or below most tunnels and tubes."
- **Staggered openings (both sides of every tube):** each tube has ports cut at **staggered intervals**, offset between its two sides and between levels, so (a) you move tube↔courtyard freely, (b) no straight sightline runs the full tube, (c) the offset creates peek/counter-peek beats. Openings at the vesica lenses connect adjacent courtyards.
- **3 vertical levels, connected:** courtyards have **staggered obstacles for height traversal** (the operator's ask) — vault-height cover, climbable ledges, and stepped platforms that let you ascend level→level *inside a courtyard*, plus the tube crowns as inter-level running lines. No dead-ends: the hex network is fully looped (6 ways out of every courtyard).
- **The outer wall is the same tubular 3-level design** (operator ask) — the perimeter isn't a flat wall but a ring of the same tubes/courtyards, so the edge is playable (run the outer ring inside/on-top), not a dead boundary.
- **Loop diagram (greybox intent — hex plan + vertical section):**
  ```
  PLAN (7-circle Tier-B instance; 6-fold symmetric; ● courtyard, ═ tube arc, ◇ vesica opening):
                 ● tube ●
              ╱ ◇       ◇ ╲
            ● ═══ ● (CTR) ═══ ●
              ╲ ◇       ◇ ╱
                 ● tube ●
  SECTION (3 stacked FoL sheets; run inside/on-top/below):
     Lvl3  ═══crown═══  ◯tube◯   ═══crown═══     <- on top / inside
     Lvl2  ═══════════  ◯tube◯   ═══════════     <- courtyards + obstacles
     Lvl1  ═══════════  ◯tube◯   ═══════════     <- below
           (staggered openings + courtyard obstacles link the levels)
  ```

## 4. Power positions (each with its flank/counter — no uncontested map-spanning sightline)
**Traversal (climb/vault/on-top running) is the design, specified inline.**
1. **The Central Circle (all instances)** — the heart of the flower; highest-value, seen from the 6 surrounding courtyards.
   - **Counter:** exposed from all 6 spokes + the tube crowns above; it can't be held from one angle. The 6-fold symmetry means every approach has a mirror counter.
   - **CLIMB / height-traverse (designed here):** the central courtyard's staggered obstacles (≤80 cm-approach climb ledges) are the level-to-level route — taking the center means winning the vertical, not just the floor.
2. **Tube Crowns (the on-top running lines)** — elevated, fast, exposed rotation over the courtyards.
   - **Counter:** fully exposed (no cover on the crown) and overlooked by the next level's crowns; a crown-runner trades cover for speed. **VAULT (designed here):** staggered vault-cover on the crowns (40–130 cm) lets a runner break LOS mid-crown — vault-to-survive the exposed line.
3. **Tube Interiors (covered rotation)** — the safe(r) arteries; curved so no interior holds a long sightline.
   - **Counter:** the **staggered side-openings** mean any interior holder is peekable from the courtyard through the offset ports; curvature + openings deny a camped tube.
4. **Vesica Passages (courtyard↔courtyard chokes)** — the lens openings; CQB pinch points.
   - **Counter:** each vesica is a two-way lens (both courtyards contest it equally, 6-fold symmetric).
- **GRAB:** optional — a grabbable cover in the central courtyard (as ARCANEON) could plug a vesica during a channel; **defer to greybox** (the network may not need it). Not core.
- **Hard rule (spec §6):** curved tubes + staggered openings mean **no uncontested map-spanning sightline anywhere** — the geometry enforces it.

## 5. Sightline bands (exercise the full laser roster) + SPRINT
- **CQB:** tube interiors, vesica passages, courtyard-obstacle cover — pulse/auto territory (the network is CQB-dense by nature).
- **Mid:** across a courtyard, courtyard-to-courtyard through a vesica, crown-to-crown — the bread-and-butter range.
- **Long:** **self-limited by curvature** — the longest lines are diagonal across 2–3 aligned courtyards (the Metatron's-Cube "straight" axes of the FoL), each broken by tube walls and staggered openings. One controlled long axis per hex spoke, always cover-broken — rewards charge/beam without an uncounterable perch.
- **SPRINT (designed here):** the **tube crowns and the outer ring are the sprint lanes** — long enough (a full arc / the perimeter) that sprinting the exposed on-top line vs. taking the covered interior is a real tempo/risk decision. (Unlike Duel_01, this map is large enough that sprint IS a decision — it earns its place here.) At the BR instances the sprint lanes are the primary rotation.
- Layered cover at CQB/mid/long is a spec §6 fundamental — all present via tubes/obstacles/crowns.

## 6. Extraction (universal primitive — spec §2, scaled per instance)
- **Zone counts by tier (spec §2):** B = 1–2 (central circle + 1 peripheral courtyard); C = 2–3 (distributed to 3 alternating courtyards for simultaneous objectives); D = 3–5 dynamic (risk-gradient across the flower, collapse **strands** cold outer courtyards per spec §10.2).
- **Placement:** extraction sits **in courtyards** (open, contested from the surrounding tubes/crowns) — the central circle is the hot/high-payout zone; peripheral courtyards are safer/slower. Channeling is exposed from 6 directions + above (the win-tension beat, spec §2).
- Payout follows risk (central > peripheral); Watts owned by the economy spec.

## 7. Spawns + navmesh (deliberate; coverage stated)
- **6-fold-symmetric spawns.** Team spawns placed on **opposite hex sectors** (rotationally paired), on the outer ring, side-tagged `AFL.Spawn.Side.0/.1` (reuse the proven `B_AFL_SpawnSelector_Dynamic`). For 2-team play, two opposite 60°/120° sectors; the hex symmetry guarantees mirrored approaches. **≈2.75 s `Gameplay.DamageImmunity`** (GE_SpawnIn); no enemy-LOS spawn (the tube network guarantees a fresh spawn has no line to the enemy). Side-swap each round (spec §1.1).
- **NAVMESH (stated, so it can be evidence):**
  - Full RecastNavMesh over all 3 levels of the network — tube interiors, courtyard floors, crowns, and every staggered opening.
  - **NavLinkProxies** on every vertical route (courtyard climb-obstacles, tube openings between levels, crown access) so bots path the whole inside/outside/top/below network — the load-bearing part (a complex 3-level tube net is exactly where bots get stranded).
  - **⚠ Collision-for-nav caveat (learned on Duel_01):** imported complex greybox meshes are **not** Recast-gathered — the FoL tubes must ship with **nav-gatherable collision** (authored simple/convex per tube segment, or a nav-collision pass) so the network navmeshes cleanly. Budget this into the greybox build, not after.
  - **Exit evidence:** round-1 `goalvalid` at the §11 window confirms the network is bot-traversable; a miss localizes to a specific level/opening.

## 8. Signature mechanic — the 3-LEVEL FLOWER-OF-LIFE TUBE NETWORK (server-authority + replication note, spec §11.8)
- **The mechanic is the geometry** — a static, CAD-generated 3-level tube/courtyard network with inside/outside/on-top/below traversal and staggered openings. It is **static level geometry**, not replicated state.
- **Why this is dual-GE (not ProMod-gated like Duel_01):** the core traversal — run the tubes, cross courtyards, climb the staggered obstacles level-to-level, run the crowns — uses **base movement (jump/climb)** present on **both** `HeroData_BagMan` (Haywire) and `_Pro` (ProMod). ProMod's **vault/slide/wall-run** *enrich* the network (wall-run a tube interior, vault a crown cover) but are **not the sole key** — the map is fully playable in Haywire. (Confirm at greybox that every level is reachable with base movement alone in Haywire.)
- **Server-authority / replication:** extraction + round/side-swap are server-authoritative (base loop). The network introduces **no new net-serialized gameplay state** → **no new `FNetSerializeScriptStructCache` surface.** Pre-committed rule: if any dynamic element is ever added (moving tubes, collapsing sections for BR), its net-serialized struct lives in **`AFLNetTypes`** (always-loaded), never a GameFeature module, validated in 2+-client PIE.
- **CAD/procedural note:** the FoL layout is **parametrically generated** (R, ring-count, tube bore, opening cadence, tier height as parameters) — the same generator emits all 4 tier instances. This is the "mathematics/CAD" the operator asked for and the key to the scalable family; build the generator once (Blender/Geometry-Script), instantiate per tier.

## 9. Symmetry
- **6-FOLD HEXAGONAL ROTATIONAL** — the Flower of Life is inherently 6-fold symmetric, which is a **competitive-integrity gift**: for 2-team play, opposite 180° sectors are exact mirrors; for FFA/party, all 6 sectors are identical. Any residual side-bias is measured and corrected by the §1.1 side-swap + telemetry (§11). This is cleaner than a bilateral mirror at scale — every rotation of 60° maps the map onto itself.

## 10. Readability (beam + silhouette + the ANTI-DISORIENTATION mandate — spec §11.10)
- **Orientation is the #1 risk of a tube network** — players getting lost in a repetitive, curved, multi-level maze is the failure mode this map must design against. Mandatory:
  - **Per-sector color/landmark coding** (the neon aesthetic put to work): each hex sector gets a distinct neon accent hue + a readable landmark, so a player always knows which sector/level they're in. The central circle is the unmistakable anchor.
  - **Level differentiation:** the 3 levels read as visibly distinct (light intensity / accent) so "am I on Lvl 1/2/3" is instant.
  - **No two openings look identical from inside** — staggered + coded so the network is navigable by sight, not memorization.
- **Beam + silhouette:** low ambient + rim light so robot silhouettes pop against neon tubes; weapon-beam hues reserved off the environment/sector hues (no beam lost in the neon); the curved tubes must not wash out a tracer.
- This map **stress-tests readability harder than any other** (most geometry, most repetition, most verticality) — the art pass must not regress it; readability is a greybox exit gate (§11), not an art afterthought.

## 11. Telemetry hooks + greybox exit criteria (spec §6) — instruments that EXIST only
**Instruments (all real): the `AFL-0213` telemetry substrate, `MoveProbe` (traversal/movement-state), `AimProbe` (aim/engagement-angle).** Every criterion is measurable with these — no unbuilt instrument.

**Captures:** kill/death density per courtyard/tube/crown/level; traversal density per level + inside-vs-on-top-vs-below split (MoveProbe); TTFC per size; extract contest/hold-vs-deny; **orientation/backtrack metric** (MoveProbe path-retracing — proxy for players getting lost); climb/vault/crown-run usage (MoveProbe); Haywire-reachability (every level reached with base movement).

**Greybox EXIT criteria (starting windows — tune per §3/§6; the `✅ watched in PIE` gate; no art until all hit):**
| Metric | Target window | Instrument |
|---|---|---|
| Median TTFC (per instance/size) | inside the tier band (B 8–15 s / C 12–25 s) | telemetry |
| Single power-position kill share | ≤ ~35 % (no dominant courtyard/crown) | telemetry + AimProbe |
| **Per-level occupancy** | all 3 levels above a min-traffic floor (no dead level — inside/on-top/below all used) | MoveProbe |
| **Inside/on-top/below split** | each traversal mode used above a floor (the core mechanic is exercised) | MoveProbe |
| Dead-zone traversal | no courtyard/tube below a min-traffic floor | telemetry |
| Extract contest / hold-vs-deny | 40–70 % contested / ≈50/50 | telemetry |
| Side/sector win-rate balance (post-swap) | within ~±5 % (6-fold symmetry should make this easy) | telemetry |
| **Orientation / backtrack rate** | below a ceiling (players aren't getting lost — the anti-disorientation §10 works) | MoveProbe |
| **Haywire base-movement reachability** | 100 % of levels/objectives reachable with base movement in Haywire (confirms dual-GE, §8) | MoveProbe |
| Climb/vault/crown-run usage | above a min floor (the vertical network is a live decision) | MoveProbe |
| Round-1 `goalvalid` | ≥ ~95 % (bots traverse the 3-level net; a miss localizes to a level/opening, §7) | telemetry |

---

## What this brief commits the map to host
- **Laser roster** → CQB (tubes/vesicas) / mid (courtyards) / self-limited long (§5).
- **Part-token loot + carry-value** → distributed courtyard extraction (§6).
- **Death / dismember** → the combat the network frames (dismember in Haywire).
- **Movement kit** → the whole point: run inside/outside/on-top/below via jump/climb (both GEs) + vault/slide/wall-run flourish (ProMod), across 3 levels — the richest traversal map in the roster.
- **The round → fight → extract → bank loop** → at every tier the family serves (3v3 up to 8v8, BR later).
- **A scalable CAD map family** → one parametric FoL generator → 4 tier instances; the data-science density loop picks the size per tier.

## Gate
Per spec §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** On approval, greybox step 3 = a **parametric FoL generator in Blender** (Geometry-Script/Python: R, rings, bore, opening cadence, tier height) emitting the Tier-B instance first, exported FBX with the proven settings; UE import + **nav-gatherable collision** (§7 caveat) + spawn/extraction authoring on the editor bridge (crash-safe `create_level`, never `new_level` — [[feedback-ue-bridge-new-level-crash]]). Build the **generator once**; instantiate per tier. **Do not build geometry before approval — that is the §11 gate.**

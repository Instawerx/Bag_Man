# Duel_01 — Per-Map Design Brief

**Status:** ✅ **APPROVED + GREYBOX BUILT 2026-08-03** — **ProMod-only** (§1; mechanical dual-GE exception, verified vs the HeroData ability split). Greybox authored in Blender and imported to **`/Game/Maps/L_Duel_01`** with an **operator-directed vertical domed redesign** (see Build Record below). PIE sign-off pending the §6 telemetry playtest. (Originally DRAFT, 2026-08-03.)

### Build record — vertical domed redesign (2026-08-03, operator-directed)
The approved brief described a **flat** single-arena duel (§2–§5 below still read that way — the original rationale is preserved). During greybox the operator evolved it into a **3-level vertical domed arena**; the as-built map is:
- **Footprint:** ~40 × 40 m plan × **~24 m tall** (was flat). Square perimeter walls to 6 m.
- **Level 0 — Floor:** central extract (open), the §4.1 **vault-cover ring** (kept — the core vault micro), 4 Wing CQB cover blocks.
- **Level 1 — Switchback sidewall catwalks:** 4 exposed, solid, 2-flight switchbacks (offset flights + flat landings, walkable both ways) climbing the walls — risk/reward exposure.
- **Level 2 — Crow's-nest watchtower:** a small central railed perch (~8 m Ø) at +7 m over the open extract, reached by **4 exposed bridge spokes** from the wall landings.
- **Cap — square bubbled arena dome:** sealed to the walls, apex ~24 m, ~16 m of headroom for jump/vertical gameplay.
- **Assets:** `SM_BagMan_Duel01_Greybox` (arena) + `SM_BagMan_Duel01_Dome` (separate, toggleable), `/Game/Maps/Duel01/`. **Spawns:** 4 `LyraPlayerStart` (2/2, side-tagged `AFL.Spawn.Side.0`/`.1` N/S, verified). **Extract:** 1 central `B_AFL_ExtractionZone`. **Nav:** RecastNavMesh (convex-decomp collision) + NavLinkProxies; spawns→extract (both sides) + spawns→nest all non-partial.
- ⚠ **Design-identity note:** the vertical/dome redesign pushes Duel_01 meaningfully toward **Duel_02's "verticality duel"** identity. Operator-directed and recorded; §2–§5 (flat-duel rationale) should be reconciled to the vertical design at the next brief pass.
- ⚠ **Nav caveat:** imported complex greybox collision isn't Recast-gathered, so nav uses convex-decomposition collision + NavLinkProxies; **bots reach the nest via the access links, not by walking the switchback ramps** (players walk them normally). Refine ramp bot-walking (authored collision) at greybox-polish; the §11 goalvalid metric will catch any residual gap.
**Tier:** A (Duel / Small — 1v1, 2v2) · **Source:** net-new (roster #1)
**Purpose:** close the **biggest hole in the mode matrix** — the entire 1v1/2v2 bracket has **no map and cannot queue**. Duel_01 is the roster's baseline duel: **a clean mirror skill arena, no exotic mechanic** (that's Duel_02's job). It proves the Arena-PvP round loop — `round → duel → (eliminate OR bank extract) → side-swap → reset` — at the smallest, purest scale, where the game is **angle control, timing, and the reset after a trade**, not rotations.

> Grounded in `IRONICS_MAP_MODE_SPEC` §1/§1.1 (Arena-PvP ladder + win condition), §2 (extraction primitive — Duel/Small = 1 central), §3 **Tier A** (density/footprint/TTFC/symmetry — used verbatim, NOT carried from Arena/Team), §6 (telemetry loop), §11 (this template). Numeric targets are **greybox-validate, not fixed law** (§3). Uses the Arena_04 doctrine (dual-GE §1, traversal-as-design, §8 replication note, §11 existing-instruments) in the clean section-numbered shape of `Arena_01_DESIGN.md`; the measured-substrate/build-record blocks are omitted — Duel_01 is net-new and unbuilt.

> **This is NOT a shrunk arena.** 1v1 has no teammates, no rotations, no flanks to cover for someone else. Every section below is written for the duel: symmetric angle control, the peek/trade, and the reset. **Traversal is design, not appendix** — VAULT is the core micro (and the map that finally makes `AFL_VAULT` reachable); SPRINT is deliberately **excluded** (see §5).

---

## 1. Identity
- **Name:** Duel_01 · **Tier:** A · **UE5 level:** one small single-level (**no World Partition** — a ~40 m arena has no streaming need).
- **Sizes hosted:** **1v1 and 2v2** (adjacent-band share per spec §4 — 1v1↔2v2; non-adjacent never share).
- **`LyraExperienceDefinition` variants** (spec §5, `map × size × ruleset`) — **ProMod-only, 2 configs:**
  - `EXP_Duel01_1v1_ProMod` · `EXP_Duel01_2v2_ProMod`
  - Offline = the same Experiences with Lyra bot-fill, no matchmaking ticket (spec §1, §5).
- **⚠ DUAL-GE EXCEPTION — Duel_01 is ProMod-only (RULED, Block 164, on mechanical grounds).** The standing rule ([[project-ironics-dual-ge-requirement]]) is "all maps play in both GEs unless stated"; **this is a stated exception**, and the strongest kind. **Reason:** the map's core micro is **Vault** (§4.1) — the center cover is vaultable so a player breaks an angle, re-peeks, and expresses the reset after a trade. **Vault is granted only on `HeroData_BagMan_Pro` (ProMod).** Verified 2026-08-03 by reading both HeroData assets directly: `DA_AFL_AbilitySet_Movement_Vault` (`GA_AFL_Vault_C`) is on `_Pro` alone, alongside Sprint/Slide/Roll/WallRun; Haywire (`HeroData_BagMan`) has **no Vault**. In Haywire the center cover would be **inert** and the reset mechanic — the whole point of the map — **would not exist**. A map whose core mechanic is absent in half its configs must not ship in those configs. (Climb and Grab *do* exist in Haywire, but they are not the core here; Vault is.) Recorded so a future reader sees the *why*, not an oversight.
- **Signature mechanic:** **NONE** — clean mirror skill arena (see §8).
- **Source:** net-new (roster #1).

## 2. Footprint & density
- **Footprint:** **~40 m across** (mid of the spec §3 Tier A **~30–60 m** band) — a **single symmetric arena**, one readable space you can take in at a glance. Not a lane map; not tiered.
- **Density:** **very high** (spec §3 Tier A). Two players (or two duos) share one compact arena — contact is near-immediate and constant; there is nowhere to be "searchy."
- **TTFC target:** **~5–10 s** (spec §3 Tier A). If median TTFC runs long, the arena is too big or the spawns too deep — shrink, don't add content (§11). 2v2 trends to the low end (four bodies), 1v1 to the high end; both must land in-window.

## 3. Flow
- **Layout:** a **single symmetric arena** built around one central contested space. The duel is **angle control, not rotation** — there are no lanes to rotate between and no map to "lock." Per spec §3 Tier A: **2 primary engagement angles + 1 flank, and no map-spanning sightline.**
  - **Angle 1 — The Line (mid):** the central sightline across the extract, spawn-facing-spawn in orientation but **hard-broken at center** so it is a *series of peeks*, never a static spawn-to-spawn hold.
  - **Angle 2 — The Wings (CQB):** two short side approaches (mirror-paired) curving around the center — the close-range angle, the way to deny The Line by closing distance.
  - **Flank — The Perch (mirror-paired):** one elevated off-angle onto the center, reached by a **climb** (§4.2). One flank *per player* (physically two, mirror-paired) — the risk route that beats a Line-camper.
- **No dead-ends, duel sense (spec §6):** the space **loops around the center** so a player who loses a peek can **disengage and reset to a new angle** rather than be cornered. The reset-after-a-trade is the core rhythm; the geometry must always offer a way to re-establish, never a kill-box with one exit.
- **No map-spanning sightline** (spec §3): the central cover breaks the Line so no single position sees both spawns; the longest available angle is mid-range, self-limited by cover.
- **Loop diagram (greybox intent — fully mirror-symmetric; N/S spawns):**
  ```
  PLAN (dual-axis mirror; extract dead-center on both axes):
                       [ Spawn N ]
                   Wing-W  |  Wing-E          <- Angle 2 (CQB side approaches, mirrored)
            [Perch-W]   \  |  /   [Perch-E]   <- Flank (climb, mirror-paired)
                    ---- [EXTRACT] ----        <- Angle 1 "The Line" broken by center cover
            [Perch-W]   /  |  \   [Perch-E]
                   Wing-W  |  Wing-E
                       [ Spawn S ]
  ```

## 4. Power positions (each with its counter — no uncontested map-spanning sightline)
**Duel-framed: these are angles to win and reset around, not points to hold with a team. Traversal (Vault, Climb) is specified inline.**

1. **The Well (center — the extract + its cover)** — the focal contested space; whoever pressures it dictates the round.
   - **Counter:** it is seen from **both Wings and both Perches**, and the center cover means it can only be *peeked*, not camped — a static holder is flanked by the opposite Perch. Channeling the extract here (§6) is maximum exposure.
   - **VAULT (designed here — the load-bearing micro):** the center cover is **40–130 cm vaultable (≤100 cm approach)**, arranged so a player **vaults a low wall to break the current angle and re-peek from a new one** — the peek/counter-peek and the **reset after a trade** are expressed through vault. This is the map where **`AFL_VAULT` becomes reachable for the first time** (ARCANEON has zero vault obstacles on bot paths → `AFL_VAULT` reads 0 there); a duel arena of tight, vaultable cover is the ability's natural home.
2. **The Perch (elevated flank, mirror-paired)** — the off-angle onto The Well; how you punish a Line-camper.
   - **Counter:** the **climb up is exposed** (seen from center + the opposite Perch), and the Perch has **no line into a spawn** — it's a committed risk for an angle, not a safe nest. The opposite Perch is its mirror counter.
   - **CLIMB (designed here):** the fast way up is a **≤80 cm-approach climbable face**, not stairs — **climbing IS the flank**, an exposed skill route. Its mirror pairing keeps the option identical for both players.
3. **The Wings (CQB side approaches, mirror-paired)** — the short-range angle that closes down The Line.
   - **Counter:** each Wing is covered by the other and overseen from center; taking a Wing trades the mid angle for proximity — a real, symmetric choice, not a free flank.

- **No GRAB, by design.** Unlike Arena_04, Duel_01 has **no grabbable actor** — a movable cover would introduce an asymmetric, state-dependent element into a format whose whole point is a clean, identical read for both players. The duel's counterplay is vault/climb/peek, not repositionable cover.
- **Hard rule honored (spec §6):** no sightline spans the arena uncontested; The Line is cover-broken and every angle has a mirror counter.

## 5. Sightline bands (exercise the roster that fits a duel) + why SPRINT is excluded
- **CQB:** the Wings, the center cover pockets, and the Perch tops — pulse/auto-fire territory; the dominant band on a duel map.
- **Mid:** The Line across the extract — the bread-and-butter duel range, the primary peek angle.
- **Long:** **none by design.** A ~40 m arena has no true long lane, and Tier A explicitly forbids a map-spanning sightline. The AWP/beam-perch long game is **not** this map's fight — Duel_01 favors the CQB/mid duel weapons; the long-range roster has its home on the larger tiers, not here. (Stated so the absence reads as intent, not an omission.)
- **SPRINT — deliberately EXCLUDED (operator-invited read).** At ~40 m with a 5–10 s TTFC, **sprint is not a meaningful decision**: you are in the fight before a sprint pays off, and sprint suppresses ADS — no one sprints *into* a 1v1. Forcing sprint lanes here would only inflate the footprint past the Tier-A band. **Movement expression on Duel_01 is vault / climb / strafe-peek, not sprint.** Duel_02 (verticality duel) or the larger tiers are where sprint earns its place.
- Layered cover at CQB/mid is a spec §6 fundamental — both bands present so the duel weapon set has a home.

## 6. Extraction (the universal primitive — spec §2 Duel/Small = 1 central zone)
- **One central contested extract, on the mirror axis (equidistant from both spawns).** Highest exposure on the map — channeling it is the **win-tension beat** (spec §2): you must hold it while exposed from both Wings and both Perches.
- **Extract-vs-eliminate is the live round decision** (spec §1.1): a round is won by **eliminating the opponent OR banking the extract**, so the channel is a genuine alternate win — in 1v1 especially, committing to it is an all-in exposure gamble that the opponent can punish or race. This is the decision the whole arena is built to frame.
- **Payout** owned by the economy spec, not here. BR collapse interaction: **N/A** (Tier A).

## 7. Spawns (authored deliberately — nav coverage stated)
- **Two mirrored spawns** (1v1) / **two mirrored 2-player spawn zones** (2v2), at opposite ends on the mirror axis, exact reflections of each other.
- **≈2.75 s `Gameplay.DamageImmunity`** on spawn (GE_SpawnIn, montage-tied, removed on spawn-montage complete — spec §3).
- **No enemy-LOS spawn** (spec §3): even at 40 m, the center cover guarantees a fresh spawn has **no direct line to the opponent** and vice-versa; spawn exits feed **both** Wings + The Line so a player is never funneled into a single predictable angle.
- **Side/spawn swap each round** (spec §1.1) — **non-negotiable for a duel**: with no teammates to average out side bias, the swap is the primary integrity mechanism (see §9).
- **NAVMESH COVERAGE (stated, so it can be evidence):**
  - Full RecastNavMesh over **100 %** of the (small) playable floor — trivially complete on a compact symmetric arena.
  - **NavLinkProxies on the vault cover and the climb Perches** so offline bots (1v1/2v2 vs Lyra bots) path the traversal routes, not just the floor.
  - **Exit evidence:** a tiny, fully-navmeshed, symmetric map is the **cleanest possible goalvalid case** — round-1 `goalvalid` should sit at the §11 ceiling. Anything materially lower here is a **systemic** spawn/objective-wiring bug, not a coverage gap (Duel_01 is a strong control case for the 72 %-on-greybox question).

## 8. Signature mechanic — NONE (clean mirror skill arena) + server-authority note (spec §11.8)
- **Duel_01 has no exotic mechanic — by design** (roster #1 = "clean mirror skill arena"). Its identity is the **symmetric duel and the central-extract reset**, not a gimmick. The exotic-mechanic duel is **Duel_02** (verticality).
- **Server-authority (competitive/economic doctrine):** extraction channel + grant, round win/loss, and the per-round side-swap are **server-authoritative** (the base Arena-PvP loop). Nothing on this map is client-decided.
- **Replication note (the `FNetSerializeScriptStructCache` category):** Duel_01 introduces **no new net-serialized gameplay state** — geometry is static, extraction uses the existing base-loop replication. So there is **no new `FNetSerializeScriptStructCache` surface**. **Pre-committed rule (stands even though nothing triggers it here):** if any dynamic element is ever added (e.g. a Duel_01 variant, or borrowing a Duel_02 mechanic), its net-serialized struct **MUST live in `AFLNetTypes` (always-loaded), NEVER in a GameFeature module**, and be validated in a 2+-client networked PIE. Recorded so a future addition inherits the rule.

## 9. Symmetry
- **MIRROR — ABSOLUTE.** Dual-axis mirror: the arena is fully symmetric, the extract sits dead-center, and **every spawn, angle, and flank is mirror-paired** so each player faces an identical layout.
- **Rationale (why tighter than an arena):** in 1v1 there is **no team to average out side bias** — any geometric asymmetry is a **per-round coin flip on spawn side**, which is fatal to a ranked skill format. Absolute mirror + the §1.1 **side-swap each round** is the integrity guarantee; residual bias is measured and must sit inside a **tight ±3 %** (§11), not the ±5 % an arena tolerates.

## 10. Readability (beam + silhouette — spec §11.10)
- **Readability is paramount on a duel map** — with only a handful of angles, a single mis-read is the whole round. **Low ambient + rim light** so the opponent **silhouette** pops instantly; **neon accents mark the two angles and the Perch climb**, never flooding the floor; environment hue kept **off** the weapon-beam hues so a tracer is never lost.
- The clean-skill-arena identity means **fewer elements, each perfectly legible** — the art pass must not add visual noise that muddies an angle. This map is a stage for the proven skin/edge/body + beam readability at the most unforgiving (1v1) scale.

## 11. Telemetry hooks + greybox exit criteria (spec §6) — instruments that EXIST only
**Instruments used (all real): the telemetry substrate (map-coordinate events), `MoveProbe` (traversal-ability + movement-state capture), `AimProbe` (aim/engagement-angle capture).** No criterion below needs an unbuilt instrument.

**Captures:**
- **Kill/first-contact density** per angle (The Line / Wings / Perch) — AimProbe + telemetry.
- **TTFC distribution** per size — telemetry.
- **Extract outcomes** — contest rate, hold-vs-deny, channel-success — telemetry.
- **Traversal-decision usage** — vault-at-center-cover, climb-the-Perch — MoveProbe.
- **First-contact-win → round-win correlation** — telemetry (does winning the first peek decide the round?).

**Greybox EXIT criteria (starting windows — tune per spec §3/§6; the `✅ watched in PIE` gate). No art pass until ALL are hit:**

| Metric | Target window | Instrument |
|---|---|---|
| Median TTFC (1v1 and 2v2) | inside 5–10 s | telemetry |
| **Side win-rate balance (post-swap)** | within **~±3 %** (tighter — no team averaging) | telemetry |
| Single-angle first-contact share | ≤ ~40 % on any one of The Line / Wings / Perch (no dominant angle) | AimProbe |
| First-contact-win → round-win | below a ceiling (~≤75 %) — the reset/extract must give real comeback counterplay | telemetry |
| Extract contest rate | genuine alternate win — contested in a meaningful share of rounds, neither free nor never | telemetry |
| Hold-vs-deny on the contested extract | ≈ 50/50 after tuning | telemetry |
| **Vault-at-center-cover usage** | above a min floor — proves the vault micro is used **and makes `AFL_VAULT` reachable** (first map to do so) | MoveProbe |
| Climb-the-Perch usage | flank taken in a meaningful share of rounds (live off-angle, not dead geometry) | MoveProbe |
| Round-1 `goalvalid` | **≥ ~98 %** — cleanest possible case (tiny fully-navmeshed symmetric map); a miss is systemic, §7 | telemetry |

*(No SPRINT metric — sprint is not a designed decision on Duel_01, §5.)*

---

## What this brief commits the map to host
- **Laser roster** → the CQB/mid sightline bands (§5); the long-range game is intentionally elsewhere.
- **Part-token loot + carry-value** → the single central extract (§6) + the carried-value HUD already proven.
- **Death** → the combat the duel frames. (Dismember is a Haywire layer; Duel_01 is ProMod-only per §1, so dismember does not apply here.)
- **Skin / edge / body + beam identity** → the readability section (§10), at the most unforgiving 1v1 scale.
- **The round → duel → eliminate-or-extract → side-swap → reset loop** → proven at the smallest, purest scale; closes the 1v1/2v2 bracket that currently cannot queue.
- **`AFL_VAULT` reachability** → the vault micro (§4.1) makes the ability reachable for the first time (0 on ARCANEON bot paths today).

## Gate
Per spec §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build. **On approval**, greybox step 3 (the Blender blockout) ships as a ready-to-run prompt using the proven gib-extraction FBX export settings as the bridge contract; UE import/placement + nav + spawn authoring run on the editor bridge (crash-safe `create_level`, never `new_level` — [[feedback-ue-bridge-new-level-crash]]). **Do not build geometry before approval — that is the §11 gate.**

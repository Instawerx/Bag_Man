# Arena_01 — Per-Map Design Brief

**Status:** APPROVED 2026-06-28 -- loop PIE-proven 3v3+4v4 (commits 638e5bbd / c079f0a7 / e326247e / 8874fca4); greybox gate cleared per `IRONICS_MAP_MODE_SPEC` section 11. (Originally DRAFT for operator approval, 2026-06-26.)
**Tier:** B (Mid-Arena — the competitive heart) · **Source:** tracker (Arena_01, "neon-cyber combat surfaces — production baseline")
**Purpose:** the FIRST map of the pillar. Per §11 build order ("begin briefs with Tier B"), Arena_01 is the minimum arena that hosts the **proven** loop end-to-end in PIE — **no signature mechanic**, so the map proves `round → fight → drop → collect → extract → bank` cleanly before any exotic-mechanic map layers on. P-MAPS is the last unproven pillar; this is where it gets its `✅ watched in PIE`.

> Grounded in `IRONICS_MAP_MODE_SPEC` §2 (extraction primitive), §3 Tier B (density/footprint), §6 (telemetry loop), §11 (this template). All numeric targets are **greybox-validate, not fixed law** (§3).

---

## 1. Identity
- **Name:** Arena_01 · **Tier:** B · **UE5 level:** one level (single-level, no World Partition — §5).
- **Sizes hosted:** 3v3 and 4v4 (adjacent-band share per §4; non-adjacent never share).
- **`LyraExperienceDefinition` variants** (§5, `map × size × ruleset`):
  - `EXP_Arena01_3v3_Extract`
  - `EXP_Arena01_4v4_Extract`
  - Offline = the same Experiences with Lyra bot-fill, no matchmaking ticket (§1, §5).
- **Signature mechanic:** **NONE** (production baseline — see §8).
- **Source:** tracker Arena_01.

## 2. Footprint & density
- **Footprint:** ~70–80 m across (mid of the §3 Tier B ~60–100 m band; sized for 3v3, holds 4v4 at high density).
- **Density:** high (§3 Tier B). Playable area per player stays tight so engagements are frequent, not searchy.
- **TTFC target:** ~8–15 s for both sizes (§3 Tier B). 4v4 trends to the low end (more bodies), 3v3 to the high end — both must land inside the window or spawns/footprint get retuned (§6).

## 3. Flow
- **Layout:** **three-lane** (A-lane / Mid / B-lane) stitched into a **figure-8 rotation** by two cross-connectors, so a team can rotate A↔B through Mid OR around the back — **no dead-ends** (§6 fundamental).
- **Rotation:** every key point reachable two ways; losing Mid does not sever A from B. Rotation time A→B ≈ the TTFC window so a flank is a real cost, not free.
- **Loop diagram (greybox intent):**
  ```
     [Spawn-1]                         [Spawn-2]
        |  \                            /  |
      A-lane  \== connector-N ==/   B-lane
        |        \   Mid   /          |
        |         [ MID ]             |     <- central high-value extract sits here
      A-site --- connector-S --- B-site
  ```

## 4. Power positions (each with its flank/counter — no uncontested map-spanning sightline)
1. **Mid Tower** (central high-ground over the Mid extract) — **counter:** both connectors flank it from below/side; it cannot hold Mid alone, and it has no line into either spawn.
2. **A-site Overlook** (elevated A anchor) — **counter:** connector-S undercut + an A-lane close approach break its angle.
3. **B-site Ramp** (elevated B anchor, mirror of A) — **counter:** symmetric — connector-S + B-lane close approach.
- **Hard rule honored (§6):** no sightline spans the map uncontested; Mid is broken by hard cover so no single angle sees spawn-to-spawn.

## 5. Sightline bands (exercise the full laser roster)
- **CQB:** lane chokes, the connector mouths, and the **extract interiors** (you channel exposed at close range) — pulse/auto-fire territory.
- **Mid:** lane lengths + the cross-connectors — the bread-and-butter pulse range.
- **Long:** **one controlled long angle per side** (A-lane and B-lane sightlines), each with mid-lane cover breaks so it rewards the **charge/beam** range without becoming an uncounterable AWP perch.
- Layered cover at CQB/mid/long is a §6 fundamental — every band present so the whole weapon roster has a home.

## 6. Extraction (the universal primitive — §2 Mid-Arena = 1–2 zones)
- **Primary — Mid Extract (central, contested):** 1 zone at map center, highest Watts payout. Channeling here is the win-tension beat (§2): you must hold it while exposed from Mid Tower + both connectors. Extract-vs-eliminate is the live round decision.
- **Secondary — Peripheral Extract (4v4 only, safer/slower):** 1 lower-payout zone off a back connector for counterplay (§2 "optional peripheral safer-but-slower"), enabling a flank-and-bank against a team stacking Mid. 3v3 ships **1 zone** (central only) to keep duels concentrated; 4v4 ships **2**.
- **Payout follows risk** (§2) — central > peripheral. Exact Watts owned by the economy spec, not here.
- BR collapse interaction: **N/A** (Tier B, no zone collapse).

## 7. Spawns
- **Two mirrored spawn rooms**, opposite ends, team-aware selection (§6 anti-spawn-camp).
- **No enemy-LOS spawn** (§3 Tier A/B rule carried): a fresh spawn never has line into an enemy or vice-versa; spawn exits feed ≥2 lanes so a single choke can't be spawn-trapped.
- **`State.Invulnerable` 1.5 s** on spawn (the existing system — §3).
- **Side/spawn swap each round (or at half)** per §1.1 — side balance tracked in telemetry (§11.9 integrity).

## 8. Signature mechanic
- **NONE — by design.** Arena_01 is the production baseline; its identity is clean neon-cyber combat surfaces (an **art** theme applied post-greybox, not a gameplay mechanic). Nothing here needs new server-authority or replication.
- **Where the exotic mechanics live (do not build here):** moving laser walls = Arena_02; **gravity = Arena_03 = the OmniWalk antigravity map**, a separate **gated special map** (map-scoped OmniWalk via the `OmniWalk.Enabled` tag, gated behind the unbuilt net-reconciliation per `IRONICS_GRAVITY_SSOT` §2.2/§2.3/§5.4); energy storms = Arena_04; vertical rails = Arena_05. **Arena_01 has none of these.**

## 9. Symmetry
- **MIRROR** (mandatory for ranked Tier B integrity — §3). A perfectly mirrored layout removes geometric side-advantage; remaining side-bias is measured and corrected by the §1.1 side-swap + telemetry. Mirror (over rotational) is chosen for the baseline because it is the cleanest integrity story for the first ranked map.

## 10. Readability (beam + silhouette — §11.10)
- **Low ambient** base lighting so emissive reads; **rim light** on playable geometry so robot **silhouettes** (the proven skin/edge/body identities) pop against the environment.
- **Neon accents placed at sightlines/power positions**, not flooding the floor — so the **laser roster's beams/tracers** (the proven weapon FX) are never lost in environment color. Environment hue stays off the weapon-beam hues (no green-on-green).
- This map is where the proven skin readability + beam readability get their stage; the art pass must not regress them.

## 11. Telemetry hooks + greybox exit criteria (§6)
**Heatmaps captured (map-coordinate events on the AFL-0213 telemetry substrate):**
- **Kill/death density** — per lane + per power position (overpowered-angle detection).
- **Traversal density** — per region (cold/dead-zone detection).
- **TTFC distribution** — per size.
- **Extract outcomes** — per zone: contest rate, hold-vs-deny, channel-success.

**Greybox EXIT criteria (starting windows — tune per §3/§6, the `✅ watched in PIE` gate for the map):**
| Metric | Target window |
|---|---|
| Median TTFC (3v3 and 4v4) | inside 8–15 s |
| Single power-position kill share | ≤ ~35 % of kills (no dominant angle) |
| Dead-zone traversal | no playable region below a min-traffic floor |
| Mid-extract contest rate | balanced band (≈ 40–70 % contested) |
| Hold-vs-deny on contested extract | ≈ 50/50 after tuning |
| Side win-rate balance (post-swap) | within ~±5 % |

**Until these windows are hit, no art pass** (§6.5).

---

## What this brief commits the map to host (the proven pillars)
- **Laser roster** → the CQB/mid/long sightline bands (§5).
- **Part-token loot + carry-value** → the extraction real estate (§6) + the carried-value HUD already proven.
- **Dismember / death** → the combat the arena frames.
- **Skin / edge / body identity** → the readability section (§10).
- **The round → extract → bank loop** → proven **end-to-end here for the first time** (the confirmed reason to build Arena_01; the round wrapper has not yet been PIE-proven in an arena).

## Gate
Per §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build. **On approval**, greybox step 3 (the Blender blockout) ships as a ready-to-run Claude-Desktop prompt using the proven gib-extraction FBX export settings as the bridge contract; UE import/placement runs on the editor bridge.

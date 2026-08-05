# Arena_02 — Per-Map Design Brief

**Status:** DRAFT for operator approval, 2026-08-02. Per `IRONICS_MAP_MODE_SPEC` §11 this brief is the greybox precondition — **no geometry until operator-approved.**
**Tier:** B (Mid-Arena — the competitive heart) · **Source:** tracker (Arena_02, AFL-1601, "moving laser walls")
**Purpose:** the SECOND Tier-B map. Arena_01 (NANOWATT) proved the `round → fight → drop → collect → extract → bank` loop with **no** signature mechanic; Arena_02 is the first map to layer an **exotic mechanic** onto that proven loop — **moving laser walls** — while staying inside the Tier-B competitive envelope. It finishes the buildable half of Tier B (Arena_03/gravity is netcode-gated per `IRONICS_GRAVITY_SSOT`, so Arena_02 is the only Tier-B map that can greybox now).

> Grounded in `IRONICS_MAP_MODE_SPEC` §2 (extraction primitive), §3 Tier B (density/footprint), §6 (telemetry loop), §8-doctrine (server-authoritative competitive state), §11 (this template). All numeric targets are **greybox-validate, not fixed law** (§3). Conforms to the shape of the approved `Docs/maps/Arena_01_DESIGN.md`.

> **Traversal is load-bearing here, not decorative.** Vault, Climb, Grab, and Sprint are designed into §4 (power positions) and §5 (sightlines) as the map's counter-routes and tempo decisions — they exist because the laser-wall mechanic creates timing windows that a traversal kit is the natural answer to. That they also exercise three movement abilities is a consequence of good design, not the reason for it.

---

## 1. Identity
- **Name:** Arena_02 · **Tier:** B · **UE5 level:** one level (single-level, no World Partition — §5 of the spec).
- **Sizes hosted:** 3v3 and 4v4 (adjacent-band share per spec §4; non-adjacent never share).
- **`LyraExperienceDefinition` variants** (spec §5, `map × size × ruleset`):
  - `EXP_Arena02_3v3_Extract`
  - `EXP_Arena02_4v4_Extract`
  - Offline = the same Experiences with Lyra bot-fill, no matchmaking ticket (spec §1, §5).
- **Signature mechanic:** **MOVING LASER WALLS** (see §8).
- **Source:** tracker Arena_02 (AFL-1601).

## 2. Footprint & density
- **Footprint:** ~75–85 m across (upper-mid of the spec §3 Tier B ~60–100 m band). Sized slightly larger than Arena_01 (~70–80 m) **on purpose**: the outer lanes must be long enough that **sprinting between power positions is a real tempo decision, not decoration** (§5, sprint) and the laser walls need travel distance to read as a sweep rather than a blink.
- **Density:** high (spec §3 Tier B). Larger footprint is offset by the laser walls periodically compressing the playable band — effective density stays high because the walls funnel players into shared timing windows.
- **TTFC target:** ~8–15 s for both sizes (spec §3 Tier B). 4v4 trends low (more bodies), 3v3 high. The sweep cycle must not push median TTFC out of the window (a wall that seals mid for too long delays first contact) — this is a §11 exit metric, not a guess.

## 3. Flow
- **Layout:** **three-lane** (W-lane / Mid / E-lane) between two mirrored spawns (N/S), stitched into a rotation loop by two cross-connectors at the tower lines — **no dead-ends** (spec §6 fundamental). The distinction from Arena_01's static figure-8 is **dynamic**: two laser walls sweep across the central band on offset timers, so which crossings are open changes on a cycle. Rotation is a **timing** problem, not just a spatial one.
- **Rotation:** every key point reachable two ways; losing Mid never severs W from E (the tower connectors + the outer lanes carry the flank). When a wall seals Mid, the rotation cost shifts to the outer lanes — a real, legible trade, not a soft-lock.
- **Loop diagram (greybox intent — mirror across the N/S mid-axis):**
  ```
                     [Spawn-N]
                   /     |      \
              W-lane    Mid     E-lane
                 |       |         |
            [W-TOWER]  ======  [E-TOWER]      <- laser walls sweep W<->E across
                 |    [ CENTER ]    |            the CENTER band on offset timers
                 |    [ EXTRACT ]   |            (over Mid + platform, never spawns)
              W-lane    Mid     E-lane
                   \     |      /
                     [Spawn-S]
  ```
- **Laser-wall travel envelope:** the walls sweep only across the **central band** (the two tower lines inward, covering Mid and the Center platform approaches). They **never enter the spawn rooms or the outer lane extremities** — those stay always-safe so a wall can never spawn-trap or hard-strand a rotation (anti-camp, spec §6).

## 4. Power positions (each with its flank/counter — no uncontested map-spanning sightline)
**Traversal counter-routes are specified inline — this is where Climb and Vault live by design.**

1. **Center Platform ("The Anvil")** — the central raised platform holding the primary extract; both laser walls sweep across its approaches.
   - **Counter:** it **cannot be statically held** — a sweeping wall forces the holder to reposition on the cycle (the mechanic is its own counter to camping). Both Mid mouths and both tower angles see onto it.
   - **VAULT (designed here):** the platform lip and its interior cover are **40–130 cm vault obstacles with a clear ≤100 cm forward approach**, positioned so a player caught mid-platform as a wall arrives **vaults the cover to drop behind it and let the wall pass** — the vault *is* the survival play against the sweep. The cover→wall pairing is the reason the geometry exists.
2. **West Tower / East Tower (mirror pair)** — elevated overlooks flanking Center, the map's two strong angles onto Mid and the extract.
   - **Counter:** the two towers hard-counter each other (each sees the other's approach); Mid undercuts both. Neither has a line into a spawn.
   - **CLIMB (designed here):** each tower's **fast route up is a climbable outer face — a surface with a ≤80 cm approach** onto the tower deck. **Climbing IS the counter-route**: it is the quick, exposed way to contest or retake a tower under pressure, versus the slower, safer lane ramp. It is not a bolted-on shortcut — without it, a lost tower is too cheap to hold; the climb keeps tower control contestable every round.
3. **The lane vault-runs (W-lane / E-lane)** — the outer lanes, the sprint arteries and the rotation carriers when Mid is sealed.
   - **Counter:** each lane's one controlled long angle (see §5) is **self-limiting** — a laser wall periodically intercepts it, so no lane long-hold is uncontested; the wall breaks the sightline on a cycle.
   - **VAULT (designed here):** staggered **40–130 cm chest-high cover (≤100 cm approach)** lines each lane so a player **vaults to keep pace / break LOS as a wall sweeps the lane mouth** — vault-to-maintain-tempo, tied to the sprint decision below.

- **GRAB (designed here):** a single **grabbable barricade prop** sits at the Center platform edge. A player banking the central extract can **grab-and-drag it to plug the one angle the current wall phase leaves open**, converting a laser-wall timing window into a briefly held channel. It is placed **with a reason bound to the win-tension beat** (channeling exposed, §6) — not scattered as a physics toy. One per map at greybox; contest-rate telemetry decides if a second is warranted.
- **Hard rule honored (spec §6):** no sightline spans the map uncontested; the laser walls actively enforce this by breaking the longest angles on a cycle.

## 5. Sightline bands (exercise the full laser roster) + SPRINT
- **CQB:** tower decks, the extract interior, the lane vault-cover clusters, and the transient **behind-wall pockets** a sweep creates — pulse/auto-fire territory.
- **Mid:** the lanes and tower-to-center — the bread-and-butter pulse range, punctuated by the sweep cutting it.
- **Long:** **one controlled long angle per outer lane**, each **self-limited by the wall sweep** (the wall passes through it on the cycle), so it rewards the **charge/beam** range without becoming an uncounterable AWP perch — the mechanic supplies the counter the static map can't.
- **SPRINT (designed here):** the W/E lanes are sized (~25–30 m spawn→tower) so **sprinting is a genuine decision, not decoration** — sprint to reach a tower or beat a wall phase, at the cost of arriving unable to immediately fire (sprint→ADS transition). Mid is shorter and more contested, so the choice is "sprint the safe long lane vs. fight the short exposed middle." The sprint distance is tuned so the time saved ≈ one wall phase — sprinting can win or lose you the crossing window.
- Layered cover at CQB/mid/long is a spec §6 fundamental — every band present so the whole weapon roster has a home.

## 6. Extraction (the universal primitive — spec §2 Mid-Arena = 1–2 zones)
- **Primary — Center Extract (central, contested):** 1 zone on the Anvil platform, highest Watts payout. Channeling here is the win-tension beat (spec §2): you hold it exposed from both towers and both Mid mouths **while the laser walls sweep your cover**. The grabbable barricade (§4) and vault cover (§4) are the counterplay; extract-vs-eliminate is the live round decision, now with a timing layer.
- **Secondary — Peripheral Extract (4v4 only, safer/slower):** 1 lower-payout zone off a back connector, outside the wall-sweep envelope, for counterplay (spec §2 "optional peripheral safer-but-slower") — a flank-and-bank against a team stacking Center. 3v3 ships **1 zone** (central only) to keep duels concentrated; 4v4 ships **2**.
- **Payout follows risk** (spec §2) — central > peripheral. Exact Watts owned by the economy spec, not here.
- BR collapse interaction: **N/A** (Tier B, no zone collapse).

## 7. Spawns (authored deliberately — the goalvalid disambiguator)
Arena_02 is the map that **disambiguates the round-1 `goalvalid` = 72 % seen on `L_BagMan_Greybox`** (candidate cause: spawn geometry or navmesh coverage, unconfirmed). For it to be evidence, spawns and nav must be intentional enough to distinguish "greybox-specific" from "systemic."

- **Two mirrored spawn rooms**, opposite ends (N/S), team-aware selection (spec §6 anti-spawn-camp). Layout mirrors Arena_01's proven side-tag pattern (`AFL.Spawn.Side.0/1`, spatial split), which PIE-proved clean on L_Arena_01 — reuse the size-agnostic selector, not a new one.
- **Spawn count:** ≥ 8 starts, **4/side** for 4v4 (3/side suffices for 3v3, so 4/side covers both), tagged by side. No untagged starts (an untagged side falls back to ALL starts and defeats separation — the documented L_Expanse footgun).
- **No enemy-LOS spawn** (spec §3 Tier A/B rule): a fresh spawn never has line into an enemy or vice-versa. Spawn exits feed **≥2 lanes** so a single choke can't spawn-trap, and **spawns sit outside the laser-wall envelope** (§3) so a wall phase can never seal a team in.
- **`Gameplay.DamageImmunity` ≈ 2.75 s** on spawn (GE_SpawnIn, montage-tied, removed on spawn-montage complete — spec §3/§7 wording), no enemy-LOS spawn.
- **Side/spawn swap each round (or at half)** per spec §1.1 — side balance tracked in telemetry (§11 integrity).
- **NAVMESH COVERAGE (stated, so it can be evidence):**
  - Full RecastNavMesh coverage of **100 % of the floor-plane playable area** — both spawn rooms, all three lanes, Mid, the Center platform ramps, and both peripheral connectors. No unreachable islands.
  - **Climb and vault routes carry NavLinkProxies** so bots can path the §4 counter-routes (tower climbs, lane vault cover) — the traversal geometry is navmesh-complete, not human-only.
  - **The laser walls are NOT static navmesh obstacles.** The floor stays continuously navigable through the sweep envelope; the wall is a **damage volume + dynamic-modifier**, so bots path the crossing and eat/avoid the wall on timing rather than treating it as a permanent wall (a static cut would strand bots and would itself depress goalvalid — that must not happen here).
  - **Exit evidence:** with authored spawns + full nav, round-1 `goalvalid` must recover to the §11 window. If it recovers, the greybox 72 % was greybox-specific (thin nav / 4-prop scene). If it does **not**, the cause is systemic (spawn/objective wiring) and escalates beyond map design.

## 8. Signature mechanic — MOVING LASER WALLS (with server-authority + replication note, per spec §11.8)
- **What it is:** two (3v3) / two–three (4v4, tuning) vertical laser walls that sweep across the central band (§3 envelope) on **offset, looping timers**. Contact applies laser damage (a GAS GE, same damage path as the weapon roster); the wall breaks sightlines and forces timing on every crossing. It is the map's identity and its own anti-camp enforcer.
- **Server-authority (competitive/economic doctrine — spec header + §6):** the wall timeline is **server-authoritative**. The server owns the canonical phase/position and is the sole authority for **damage application and hit registration** against players. Clients never locally decide that a wall hit someone. A round starts the wall cycle from a server-stamped time; the outcome (who the wall killed, when it sealed a lane) is server-truth.
- **Replication model (the `FNetSerializeScriptStructCache` category — this is mid-match replicated state):**
  - The wall's phase is **mid-match replicated state across clients** — exactly the category the `FNetSerializeScriptStructCache` lesson warns about (the same class as the Shrink-system resize-state caution in spec §8.3). **A single-client test will not catch desync; this must be validated in a networked PIE (2+ clients), not standalone.**
  - **Preferred low-bandwidth model:** replicate a **server cycle-start timestamp + fixed period** once per round (deterministic timeline), and have clients **compute wall position each frame** from that timestamp — visuals are client-computed, damage is server-authoritative. This avoids per-frame position replication entirely.
  - **Any struct that IS net-serialized** (e.g. an `FLaserWallState { CycleStartServerTime; Period; PhaseOffset; bEnabled }` if the deterministic-timeline approach needs one) **lives in `AFLNetTypes` (always-loaded), NEVER in a GameFeature module** — the GameFeature module can unload and take the struct's `FNetSerializeScriptStructCache` registration with it, which is the documented failure. The mechanic's runtime component may live in a GameFeature (reads the always-loaded state), but the **net-serialized type does not.**
  - **Engineering pattern:** the wall is a **damage volume + a movement/visual `UActorComponent`**, following the proven "GameFeature-attached component reading always-loaded state" pattern (the same shape as `AFL-0304-B` P-CONTROLS and the Shrink resize component) — no new subclassing of core movement/collision.
- **Anti-strand rule (repeated from §3/§7 because it is a correctness constraint, not polish):** the wall never seals a spawn, never permanently cuts navmesh, and never creates a phase where a team has zero safe crossing to an objective.

## 9. Symmetry
- **MIRROR** (mandatory for ranked Tier B integrity — spec §3). Mirrored geometry removes side-advantage; the laser walls are **phase-symmetric** (each side faces the same wall timing under the side-swap), so the mechanic adds dynamism **without** adding side-bias. Remaining side-bias is measured and corrected by the §1.1 side-swap + telemetry (§11). Rotational was considered and declined: the mechanic already supplies the variety that rotational usually buys, and mirror keeps the cleanest integrity story for the second ranked map (consistent with the Arena_01 rationale).

## 10. Readability (beam + silhouette — spec §11.10)
- **Low ambient** base lighting so emissive reads; **rim light** on playable geometry so robot **silhouettes** pop against the environment.
- **The laser walls must be unmistakably readable as walls, and distinct from weapon beams:** the wall emissive uses a **reserved hue/animation signature** (wide, slow, structural) that cannot be confused with the **weapon roster's beams/tracers** (thin, fast) — no player ever dies because a wall looked like incoming fire or vice-versa. Environment hue stays off both the wall hue and the weapon-beam hues (no green-on-green).
- **Neon accents placed at sightlines/power positions**, not flooding the floor, so tracers are never lost in environment color.
- **Telegraph:** an approaching wall gets a readable pre-sweep telegraph (edge glow / audio cue) so the sweep is a **skill-timing** beat, not a random death — readability is a fairness requirement here, not just art.
- The art pass must not regress proven skin/edge/body readability or beam readability, and must preserve the wall-vs-beam distinction above.

## 11. Telemetry hooks + greybox exit criteria (spec §6)
**Heatmaps captured (map-coordinate events on the `AFL-0213` telemetry substrate):**
- **Kill/death density** — per lane + per power position (overpowered-angle detection).
- **Traversal density** — per region (cold/dead-zone detection), including the climb routes and vault runs.
- **TTFC distribution** — per size.
- **Extract outcomes** — per zone: contest rate, hold-vs-deny, channel-success.
- **Laser-wall interaction** — deaths-attributed-to-wall, wall-seal duration per lane, crossings-per-sweep.
- **Traversal-decision usage** — vault-at-sweep-cover, tower-climb-under-contest, grab-barricade-at-extract, sprint-lane commit. (Captured as **design validation** — proof the counter-routes are live decisions, not dead geometry.)

**Greybox EXIT criteria (starting windows — tune per spec §3/§6; the `✅ watched in PIE` gate for the map). No art pass until ALL are hit (spec §6.5):**

| Metric | Target window |
|---|---|
| Median TTFC (3v3 and 4v4) | inside 8–15 s |
| Single power-position kill share | ≤ ~35 % of kills (no dominant angle) |
| Dead-zone traversal | no playable region below a min-traffic floor |
| Center-extract contest rate | ≈ 40–70 % contested |
| Hold-vs-deny on contested extract | ≈ 50/50 after tuning |
| Side win-rate balance (post-swap) | within ~±5 % |
| **Deaths attributed to the laser wall** | ≤ ~15 % of deaths (the wall shapes play; it does not dominate kills) |
| **Wall-seal duration per lane** | no lane sealed long enough to push TTFC out of window or to force a zero-safe-crossing phase (hard cap: never 0 safe crossings) |
| **Vault-at-sweep-cover usage** | above a min floor at the platform + lane cover (proves vault-to-beat-sweep is a used decision) |
| **Tower-climb-under-contest usage** | climb counter-route taken in a meaningful share of tower retakes (proves it's a real counter, not decoration) |
| **Round-1 `goalvalid`** | ≥ ~95 % (the disambiguator target — recovers from the greybox 72 %; a miss escalates as systemic, §7) |

**Networked-PIE gate (mechanic-specific, from §8):** wall phase/damage validated with **2+ clients** (single-client cannot catch the replication desync category) before the map is called flow-proven.

---

## What this brief commits the map to host (the proven + newly-exercised pillars)
- **Laser roster** → the CQB/mid/long sightline bands (§5), with the wall-vs-beam readability split (§10).
- **Part-token loot + carry-value** → the extraction real estate (§6) + the carried-value HUD already proven.
- **Dismember / death** → the combat the arena frames, now with a wall-attributed death source.
- **Skin / edge / body identity** → the readability section (§10).
- **The round → extract → bank loop** → inherited proven from Arena_01; Arena_02 stresses it with a timing mechanic.
- **Movement kit (Vault / Climb / Grab / Sprint)** → designed into §4/§5 as counter-routes and tempo, unblocking three deferred AI abilities as a consequence of the map's design, not as a test harness.
- **First exotic mechanic on the proven loop** → moving laser walls, server-authoritative, `AFLNetTypes`-resident net state (§8).

## Gate
Per spec §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build. **On approval**, greybox step 3 (the Blender blockout) ships as a ready-to-run prompt using the proven gib-extraction FBX export settings as the bridge contract; UE import/placement + the laser-wall component + spawn/nav authoring run on the editor bridge. **Do not build geometry before approval — that is the §11 gate.**

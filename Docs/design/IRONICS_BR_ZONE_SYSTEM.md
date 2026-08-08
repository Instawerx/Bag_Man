# IRONICS — BR SHRINKING ZONE SYSTEM (scope)

**Status:** **S1 SHIPPED (C++ half)** — operator-approved 2026-08-07 and built. Determinism (Z2) PROVED headlessly.
Editor-side assets (the DoT effect, a config instance, viz, HUD) remain — see §10.
**Date:** 2026-08-05.
**Why this doc:** the shrinking safe-zone ("Zone") is the **single genuinely net-new BR system** — everything else
in the BR ruleset is shipped or reused ([IRONICS_BR_MODE_SPIKE.md](IRONICS_BR_MODE_SPIKE.md) §4;
[ShantyTown_BR_DESIGN.md](ShantyTown_BR_DESIGN.md) §2, §3 Layer A). This scopes it as its own work unit.
**Primary users:** Staked Battle Royales & Tournaments → **server-authoritative, deterministic, replay-verifiable**.

**Grounding (verified on disk 2026-08-05):** no `Zone`/`Storm`/`Ring`/`SafeZone` system exists anywhere in the
project. Periodic-damage GE pattern to mirror **does** exist: `GE_AFL_Damage_BeamTick`, `GE_AFL_Damage_CutterTick`
(periodic tick GEs, `AFLCombat/.../Effects/`).

---

## 1. Where it fits
- **Match structure** = `UAFLBattleRoyaleComponent` (last-standing + placement, shipped `e6e0ffa0`).
- **Phase spine** = `UAFLMatchPhaseComponent` (Warmup → Playing → PostGame).
- **The Zone is a NEW server-only GameState component** (`UAFLZoneComponent`) that runs **during Playing** and
  forces engagement by shrinking a safe radius, applying damage-over-time (DoT) outside it.
- **Scope: full-map BR only (BR_18 / BR_36).** The district modes (Duel / Arena / Team — ShantyTown §11) are
  **fixed fenced arenas with NO ring**; the Zone is gated to the BR experience, not the district experiences.
- **The Zone does NOT resolve the A3/A5 match-conclusion stall** ([BR_MODE_SPIKE] — cause unknown). Keep separate.

## 2. REUSE inventory (mirror / build on — most of the plumbing exists)
| Need | Existing pattern to mirror | Notes |
|---|---|---|
| Component shape | `UAFLMatchPhaseComponent` / `UAFLRoundManagerComponent` | `UGameStateComponent` via experience `AddComponents`; `GetGameStateChecked()->HasAuthority()` gate; replicates state for HUD |
| **Lyra export boundary** | `UAFLMatchPhaseComponent` (BP phase shells + C++ driver) | `ULyraGamePhaseAbility` is NOT exported → zone **phases are BP shells (tag only)**, all timing/shrink logic in the C++ driver |
| Phase-start observe | reflective `ULyraGamePhaseSubsystem` bind (round mgr / extraction) | observe `AFL.GamePhase.Playing` → start the zone clock |
| **DoT-outside** | `GE_AFL_Damage_BeamTick` / `GE_AFL_Damage_CutterTick` | periodic damage GE, SetByCaller magnitude → new `GE_AFL_Zone_DoT` |
| Determinism seed | `MatchId` (FGuid, on BR/round component) | seed the center-selection RNG → reproducible for dispute replay |
| Telemetry | `FAFLCombatTelemetry` | add zone-phase/center emits for staked replay |
| Net safety | round manager rule | plain replicated UPROPERTYs only, no custom net-serialized struct |
| Throttled server tick | round manager (0.25 s tick) | publish `TimeToShrink`; apply DoT on a period, not per-frame |

## 3. NET-NEW (the build)
1. **`UAFLZoneComponent`** (C++ server driver). Replicated: `CurrentCenter`, `CurrentRadius`, `TargetCenter`,
   `TargetRadius`, `PhaseIndex`, `TimeToShrink` (+ OnRep for HUD). Phase machine: hold → telegraph → shrink
   (interpolate center+radius over duration) → hold → … Seeded center per phase from `MatchId` RNG; every
   transition + center **logged**. Each throttled tick: living participants **outside** `CurrentRadius` get the
   periodic DoT GE (magnitude = per-phase curve); **inside** → removed.
2. **`GE_AFL_Zone_DoT`** — new periodic damage GE mirroring `GE_AFL_Damage_BeamTick` (SetByCaller magnitude).
3. **Zone phase BP shells** — BP children of `ULyraGamePhaseAbility` (tag only — the export-boundary workaround).
4. **Zone tags** in `AFLCombatTags.ini` — e.g. `AFL.GamePhase.Zone.*`, `State.Zone.Outside`. No undeclared tags.
5. **`DA_AFL_ZoneConfig`** DataAsset — phase count, per-phase radii, hold/shrink durations, damage curve
   (designer-tunable; separate curves for BR_18 vs BR_36 if needed).
6. **Zone visualization actor** — wall/dome material driven by replicated `CurrentRadius`/`Center` (cosmetic;
   authority = the state). Client-side.
7. **HUD** — minimap ring + shrink timer + next-circle telegraph (likely AIK/BP lane).

## 4. Architecture — determinism for staking (the crux)
- **Server-authoritative:** only the server computes center/radius/phase and applies DoT; clients render from
  replicated state. **Zero client input to zone placement** — the staking integrity line.
- **Deterministic + reproducible:** center sequence from the **`MatchId`-seeded RNG**; each phase + center logged
  via telemetry → a staked-match dispute can be **replayed exactly**. Same MatchId ⇒ same zone.
- **Telegraphed:** next circle (center + radius) published **before** each shrink — fair warning, tournament-critical.
- **Perf:** outside-check is per-living-participant per period (≤ 36), throttled (~1 s) not per-frame — cheap; mirrors
  the round manager's throttled tick.
- **Water interaction (ShantyTown §4):** the Zone DoT and the lethal-depth water rule are **independent server
  hazards** — a player fleeing the ring into deep water still shorts out. Both deterministic, no double-jeopardy exploit.

## 5. Vertical slice (S1 deliverable)
Smallest end-to-end proof: **`UAFLZoneComponent` + `GE_AFL_Zone_DoT` + BP phase shells + tags** — one config
(3 phases), **seeded** center (`MatchId`), **telegraphed**, **DoT-outside**, **replicated radius** — on a test map
(or ShantyTown greybox) with the BR component + bots. Assertions:
- **Z1** zone initializes; first circle telegraphed.
- **Z2** radius shrinks **deterministically** (same MatchId → identical center sequence across two runs).
- **Z3** DoT applies outside / clears on re-entry.
- **Z4** shrink **drives survivors together** (feeds last-standing).
- **Z5** **server-authoritative** — a client cannot move/resize the zone.
Headless-testable via a `ZoneTestRunner` mirroring `AFLMatchTestRunner`.

## 6. Phases
| Step | Output |
|---|---|
| **S1** | `UAFLZoneComponent` + `GE_AFL_Zone_DoT` + BP phase shells + tags — one seeded, telegraphed shrinking circle w/ DoT |
| **S2** | `DA_AFL_ZoneConfig` (designer curves) + telegraph HUD data feed |
| **S3** | Zone viz actor (wall/dome) + minimap ring |
| **S4** | Headless `ZoneTestRunner` (Z1–Z5, determinism proof) + PIE with bots |
| **S5** | Findings + real build estimate + integration with BR conclusion (A3/A5 close-out) |

## 7. Open decisions (operator — do NOT assume)
1. **Phase count + timing/damage curves** (tuning; BR_18 vs BR_36 same or different).
2. **Center-selection algorithm** — uniform-in-circle vs weighted toward the town core / away from water.
3. **Final zone** — shrink to a fixed last-stand footprint vs shrink to a point.
4. **Zone shape** — circle (simplest, deterministic) vs polygon.
5. **HUD lane** — AIK/BP authoring for the minimap ring + timer.
6. **Damage model** — flat per-phase vs ramping DoT; interaction with health/shield attributes.

## 8. Risks
1. **Determinism under net** — the core staking requirement; must be provably reproducible (Z2). Highest risk.
2. **Lyra export boundary** — mitigated by the proven BP-shell + C++-driver pattern (low).
3. **Perf at 36** — low (throttled per-participant DoT).
4. **Does NOT fix A3/A5 match-conclusion stall** — separate unknown; flagged so the Zone isn't assumed to close it.

## 9. References
[IRONICS_BR_MODE_SPIKE.md](IRONICS_BR_MODE_SPIKE.md) · [ShantyTown_BR_DESIGN.md](ShantyTown_BR_DESIGN.md) (§2, §3A, §11) ·
[IRONICS_MATCH_STAKING_SSOT.md](IRONICS_MATCH_STAKING_SSOT.md) · [IRONICS_LEAGUE_ADVANCEMENT_SSOT.md](IRONICS_LEAGUE_ADVANCEMENT_SSOT.md).
Code: `UAFLBattleRoyaleComponent`, `UAFLMatchPhaseComponent`, `UAFLRoundManagerComponent`, `GE_AFL_Damage_BeamTick`,
`FAFLCombatTelemetry` (all `Plugins/GameFeatures/AFLCombat/`).

---

## 10. S1 AS BUILT (2026-08-07)

### What shipped
| Piece | Where |
|---|---|
| `FAFLZonePlan` / `FAFLZoneRules` / `FAFLZonePhasePlan` — the pure plan | `AFLCombat/Public/Zone/AFLZonePlan.h` |
| `UAFLZoneComponent` — the server driver | `AFLCombat/Public/Zone/AFLZoneComponent.h` |
| `UAFLZoneConfig` — the designer surface | `AFLCombat/Public/Zone/AFLZoneConfig.h` |
| `State.Zone.Outside` | `AFLCombat/Config/Tags/AFLCombatTags.ini` |
| `EmitZonePlan` / `EmitZonePhase` | `AFLCombat/Public/Telemetry/AFLCombatTelemetry.h` |
| 9 determinism/fairness tests | `AFLCombatTests/Private/Spec/AFLZonePlanSpec.cpp` |

### ⚠ ONE ARCHITECTURE CHANGE FROM §3.1 — the whole plan is built UP FRONT

§3.1 said *"seeded center per phase from `MatchId` RNG"*. Drawing lazily makes determinism a property of
**call order**: reproducible only while nothing else ever touches that stream and every phase draws the same
values in the same sequence. Both are invisible invariants, and a later edit breaks them silently — which is
precisely how a staking dispute becomes unanswerable, because the log looks fine and the replay does not match.

The plan is therefore computed **once**, as a pure function of `(seed, rules)`, before the first circle moves:

- determinism is **structural** rather than disciplined — one draw sequence, in one function
- it is unit-testable with **no world, no actors, no net** — so **Z2 is proved headlessly in CI**, not inferred
  from one run of one match on one machine
- the full sequence is known at match start, so **telegraphing is a lookup, not a prediction**, and a
  tournament observer can be handed the whole thing up front

### The §7 decisions, as taken

All six are answered as **defaults on `UAFLZoneConfig`**, so every one remains a data edit rather than a
rebuild. Where a decision is load-bearing rather than taste, the reasoning is in the code:

| # | Taken as | Why |
|---|---|---|
| 1 Phase count / curves | 6 shrinks; geometric radii; linear damage + shrink-time ramp | Geometric because **area** goes as r²: a linear radius schedule removes far too much map in the first step. One config asset per field size answers BR_9 vs BR_36 with no code change. |
| 2 Centre selection | Uniform-in-disc within `parentR − childR`, `CentreDriftFraction` to bias | **Containment is not optional** — see below. Uniform-in-disc (√ on the radial roll) rather than uniform-in-radius, because the latter clusters circles near the parent centre, and a predictable centre is a competitive edge in a mode that settles wagers. |
| 3 Final zone | Fixed footprint (`FinalRadius`, default 3000cm) | A point makes the ending a coin flip no skill survives — the one outcome a **wagered** match cannot ship. |
| 4 Shape | Circle | Deterministic, replicates as centre+radius, cheap containment test. Polygon buys nothing here. |
| 5 HUD lane | Deferred — the data feed is replicated and ready | `CurrentCentre/Radius`, `TargetCentre/Radius`, `TimeToNextEvent`, `ZoneState`, plus `OnZoneStateChanged`. |
| 6 Damage model | Ramping DPS, applied as an **instant** GE per period | See below — instant beats periodic-duration on every teardown path. |

**CONTAINMENT IS A FAIRNESS REQUIREMENT.** Every circle is fully inside its parent, by construction. If a
child could poke outside, a player standing safely in the current zone could be outside the next one having
done nothing and having had no way to avoid it. In a staked match that is indefensible. Asserted across 400
seeds, not one — this is exactly the property that holds for the seed you tested and fails for the seed a
player got.

**THE DoT IS INSTANT, NOT PERIODIC-DURATION** (a divergence from §3.2). An instant effect re-applied each
period has no removal problem at all: nothing persists between ticks, so re-entry, death, disconnect and
re-possession need zero teardown. The duration version needs correct cleanup on all four and is wrong on any
one of them. **Z3 ("DoT clears on re-entry") is therefore true by construction rather than by cleanup.**

**NO MATCHID, NO ZONE.** The plan is seeded from `UAFLBattleRoyaleComponent::MatchId`; the component polls
for it rather than binding `AFL.GamePhase.Playing`, because the BR component observes that same transition to
author the id and two components racing one event is a coin flip over whether the seed exists yet. Waiting is
correct: a zone seeded from anything else is a zone a dispute cannot replay.

### A real bug the determinism test caught

The first `SeedFromGuid` folded the four GUID words with **XOR**, which is commutative — so `{1,2,3,4}` and
`{4,3,2,1}` seeded identically and **two distinct staked matches would have shared a circle sequence**. A
fairness bug before a replay bug, and unrelated ids would have passed happily. Replaced with a byte-wise
FNV-1a fold; `Z2_DifferentSeedDiverges` now compares **permutations** specifically, plus 512 ids → 512 seeds.

### Verification

`AFL.Zone.Plan.*` — **9/9 pass**, headless, no PIE:
Z2 same-seed-identical · Z2 different-seed-diverges · Z2 seed-uses-whole-guid · every-circle-contained (400
seeds) · radii-only-shrink · final-circle-fightable · pressure-ramps · first-hold-longest · hostile-config-degrades.

```bash
UnrealEditor-Cmd.exe Bag_Man.uproject -ExecCmds="Automation RunTests AFL.Zone.Plan;Quit" -unattended -nullrhi
```

### S2 assets — BUILT 2026-08-07 (editor open)

| Asset | What it is |
|---|---|
| `/AFLCombat/Effects/GE_AFL_Zone_DoT` | **INSTANT**, 0 executions, one modifier: `LyraHealthSet.Damage += SetByCaller(Data.Damage)` |
| `/AFLCombat/Zone/DA_AFL_ZoneConfig_ShantyTown_BR9` | town core · 4 shrinks · final 20m · 3→35 dps |
| `/AFLCombat/Zone/DA_AFL_ZoneConfig_ShantyTown_BR20` | town core · 6 shrinks · final 25m · 2→30 dps |
| `/AFLCombat/Zone/DA_AFL_ZoneConfig_ShantyTown_BR36` | full landscape · 8 shrinks · final 30m · drift 0.55 |

**The DoT targets `Damage`, NOT `Health`, and NOT the weapon pipeline.** `Damage` is Lyra's *meta* attribute:
`ULyraHealthSet::PostGameplayEffectExecute` converts it into health loss and drives `ULyraHealthComponent`'s
death. Writing `Health` directly would drain a player to zero without ever killing them. And the sibling
`GE_AFL_Damage_Instant` runs `UAFLDamageExecCalc` — headshot / weakpoint / distance multipliers and zone-HP
routing — which is a weapon pipeline with nothing to say about weather, so the zone GE deliberately carries
**zero executions**.

### ⚠ THE GEOMETRY WAS RE-MEASURED — the doc's Phase 0 figures are stale for this map

`ShantyTown_BR_DESIGN.md` §0 measured **`Demo_Map`** before the fork to `/Game/Maps/L_ShantyTown` and the
World Partition conversion. Measured live off L_ShantyTown's **WP actor descriptors** (755 descs, nothing
loaded — a loaded-actor sample reads only the streamed-in cells and would have been wrong):

| | Demo_Map (§0, 2026-08-05) | **L_ShantyTown (measured 2026-08-07)** |
|---|---|---|
| Landscape | 617 × 607 m | **605 × 605 m**, centre **(11118, 16033)**, half-diagonal 428 m |
| Town core | 357 × 302 m, centre ≈ (1774, 11958) | **262 × 228 m**, centre **(870, 7425)**, half-diagonal 174 m |

The landscape carried over; **the town core did not** — different content, different centre. The configs use
the live figures. BR9/BR20 open on a 175 m circle that wraps the core exactly; BR36 opens at 428 m, which
circumscribes the whole landscape so nobody can start outside the first circle.

**⚠ CONSEQUENCE FOR BR SPAWNS (owed, P2).** BR9/BR20's opening circle is the **town core only** — the outer
landscape starts outside it. The BR drop/spawn distribution does not exist yet (`ShantyTown_BR_DESIGN.md` §0:
*"only 2 (demo)"*), and when it is authored **every BR9/BR20 spawn must sit inside the core**, or those
players begin the match taking zone damage through no fault of their own.

### Still owed
1. **A BR EXPERIENCE DOES NOT EXIST** — disk-verified: no `B_AFLExperience_*BR*`, no `DA_AFL_ShantyTown_BR_*`.
   This now blocks everything below, and it is the reason `queue-registry.json` leaves every BR cell
   unpublished and both doors read *"Not open yet"*. It is BR **mode** wiring (team setup, bot fill, spawn
   distribution), not zone work.
2. **The `AddComponents` row** adding `UAFLZoneComponent` + its config to that experience — BR only, never
   the district experiences.
3. **S3 viz actor + S4 PIE-with-bots (Z1/Z3/Z4/Z5).** Z2 is closed; the rest need a match to run in.
4. **BR spawn distribution inside the opening circle** — see the warning above.

---

## Gate
Per spec §11: this scope → **operator approval ✅ 2026-08-07** → **S1 spike ✅** → **determinism proof (Z2) ✅** →
curves/tuning → PIE sign-off. Disk state was re-verified before build (no Zone code existed; the BR/round/phase
substrate did). This was the net-new dependency gating the BR mode layer (ShantyTown §2, §3A) — **the C++ half
is no longer the blocker; the editor-side assets in §10 are.**

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

### S3 — BR EXPERIENCES + PLAYLISTS BUILT 2026-08-07

The BR match wiring already existed from the spike and was simply never composed into an experience:

- **`LAS_AFL_BR_S1`** — the extraction action set with **`B_AFL_TeamSetup_Solo`** in place of
  `B_AFL_TeamSetup_TwoTeams`. This is what makes BR solo: every `APlayerState` is its own participant.
- **`LAS_AFL_Teams_BR_S1`** — carries **`B_AFLBotFill_BR_S1`**.

Built on top of them, 12 assets:

| | |
|---|---|
| `B_AFLExperience_ShantyTown_{BR9,BR20,BR36}_{Haywire,ProMod}` | Haywire = 5 game features (incl. `AFLDismember`) + `HeroData_BagMan`; ProMod = 4 + `HeroData_BagMan_Pro` |
| `DA_AFL_ShantyTown_{BR9,BR20,BR36}_{Haywire,ProMod}` | `L_ShantyTown`, `MaxPlayerCount` 9/20/36 |

**⚠ NO `District` EXTRA-ARG, deliberately.** Every Match Play ShantyTown playlist streams a district
(`District_Duel/Arena/Team`); the BR playlists set **none**. The districts are fenced arenas with no ring
(§1) — streaming one into a battle royale would fence 36 players into a duel yard.

### ⛔ BR IS NOT PUBLISHED, AND MUST NOT BE UNTIL THESE THREE CLOSE

`config/queue-registry.json` still leaves every BR cell unpublished and both doors read *"Not open yet"*.
That is correct: the experiences exist, but a match run today would not be a battle royale.

> **STATUS 2026-08-08 — all three are now closed.** Item 1 (the `AddComponents` row) was authored by hand by
> the operator; item 3 (spawn distribution) shipped as 38 `BR_Spawn_*` starts plus the open-map spawn scope;
> item 2 (field size) is closed below — **with its original diagnosis corrected, because it was wrong.**
> Publication additionally waits on the combat close, which the limb bleed-through pass (`e25f6dda`) moved
> from 14% to **58% of eliminations decided by fighting rather than by the ring.**

1. **THE ZONE IS NOT ATTACHED — ONE MANUAL ROW PER BRACKET REMAINS.** Everything around it is built and
   wired; the `AddComponents` row itself **cannot be scripted**. `UGameFeatureAction_AddComponents` and
   `FGameFeatureComponentEntry` are not `BlueprintType`, so they are invisible to the Python bridge, to the
   Lua bridge's `get`/`set`/`array_add` (dot-notation and ImportText both), and even to a hand-constructed
   action object via `unreal.new_object` — the object is created, `ComponentList` still cannot be touched.
   All three routes verified.

   **What IS built and committed:**

   | | |
   |---|---|
   | `B_AFLZone_ShantyTown_{BR9,BR20,BR36}` | BP subclasses of `UAFLZoneComponent` with `Config` already set to the matching `DA_AFL_ZoneConfig_ShantyTown_*`. Same pattern as `B_AFLBotFill_BR_S1`, and necessary because `AddComponents` instantiates a CLASS with its CDO defaults — it cannot set per-instance properties. |
   | `LAS_AFL_BR_Zone_{BR9,BR20,BR36}` | Action sets, **deliberately EMPTY**, already added to the `ActionSets` of both experiences of their bracket. |

   **The remaining step — 3 assets, one row each:** open `LAS_AFL_BR_Zone_<BRACKET>`, add one
   `GameFeatureAction_AddComponents`, and add a single component entry:

   - **Actor Class** = `LyraGameState`
   - **Component Class** = `B_AFLZone_ShantyTown_<BRACKET>`
   - **Client Component** = ✅ and **Server Component** = ✅

   ⚠ **Client Component must be TICKED.** The zone looks server-only and its class comment says so, but it
   REPLICATES `CurrentCentre` / `CurrentRadius` / `TimeToNextEvent` to drive the HUD, and a component that
   does not exist on the client cannot replicate to it. `BeginPlay` already disables tick on non-authority,
   so the client instance costs nothing and only renders.

   ⚠ **The zone sets ship EMPTY on purpose.** They were duplicated from `LAS_AFL_Teams_BR_S1`, which meant
   they arrived carrying its `B_AFLBotFill_BR_S1` row — wiring that in would have **double-added bot fill**
   to every BR experience. A wrong row is worse than a missing one, so the actions array was cleared: the
   sets are inert, and adding them to the experiences changed nothing until the row above is authored.

   **Without the row there is no ring at all**, and a BR with no ring may simply never conclude — which is
   also the open A3/A5 stall. The alternative to doing it by hand is a C++ `UAFLGFA_AddZone` on the next
   editor-closed pass; the project already has that pattern twice (`AFLGFA_WeaponSpawns`,
   `AFLGFA_ActivateDataLayers`), and it would make this diffable instead of clicked.
2. ~~**BOT FILL IS 3, FOR EVERY FIELD SIZE.**~~ **CLOSED 2026-08-08 — and the diagnosis above was wrong.**

   **What this section claimed:** that `B_AFLBotFill_BR_S1`'s `NumBotsToCreate = 3` meant three bots in a
   BR_36, fixable with one bot-fill BP per field size.

   **What is actually true:** `NumBotsToCreate` on those Blueprints is **vestigial and reads nothing.**
   `UAFLBotFillComponent` overrides `ServerCreateBots_Implementation` and replaces the count decision
   entirely with `Target = TeamSize * NumTeams`. For a solo BR `TeamSize = 1`, so the field size collapsed
   to **however many teams the team-setup asset happened to author** — and one `B_AFL_TeamSetup_Solo`
   (**36 teams**) is shared by all three brackets.

   The bug therefore ran in the *opposite direction* to what was written here: not three bots in a BR_36,
   but **thirty-five bots in a BR_9**. Measured, not inferred — the 2026-08-07 BR9 PIE logged
   `total=36` and 35 distinct `B_AFL_BotController_C_*`. Reading the CDO would have "confirmed" the wrong
   story; only the log and the override's source together give the real one.

   **The fix — `?FieldSize=N`, declared by the playlist.** `UAFLBotFillComponent::ComputeTargetTotal()` now
   honours a `FieldSize` URL option, clamped to the structural capacity (`TeamSize * NumTeams`) with a loud
   warning if a playlist over-declares. Absent the option it returns the structural product unchanged, so
   **every non-BR mode keeps its exact prior behaviour**. `ServerCreateBots_Implementation` now calls that
   one function instead of recomputing the target inline — they could previously disagree, which was a
   latent second defect in the converge path.

   The six `DA_AFL_ShantyTown_BR*` playlists carry `ExtraArgs["FieldSize"] = 9 / 20 / 36`. Their
   `MaxPlayerCount` was **already** correct per bracket, which is the independent confirmation that the
   bracket numbers are right and only the fill was wrong.

   **Why the URL and not three more assets:** a per-bracket team setup needs a per-bracket Teams action set
   to reference it, and that is the same un-scriptable `AddComponents` row as item 1 — three hand-clicked
   rows and three new assets to express one integer. The URL is also the seam a dedicated-server matchmaker
   already speaks; `Experience` and `NumBots` live there today.

   ⚠ **Direct-PIE with an experience override does not set URL options**, so a bare PIE still fills to the
   structural 36. The bracket is correct through the front end, which is the shipping path and the only one
   that carries a playlist.
3. **THERE IS NO BR SPAWN DISTRIBUTION.** The only `LyraPlayerStart`s on the map are the district set — 60
   of them inside a 191 × 90 m patch centred (3132, 794). BR9/BR20's opening circle is the **town core**, so
   spawns must be spread inside it; BR36's covers the landscape. Thirty-six players spawning in a 191 × 90 m
   patch is a drop-zone brawl, not a BR opening.

### First PIE run, 2026-08-07 — the zone instantiated, and caught two wiring defects

The BR experience loaded (`Identified experience ...ShantyTown_BR9_Haywire (Source: DeveloperSettings)`)
and `UAFLZoneComponent` came up, which proves the operator's `AddComponents` row works. It then refused to
start, correctly, and its two warnings were both real:

1. **`no UAFLZoneConfig assigned`** — a scripted CDO edit on a Blueprint does **not** recompile its
   generated class. `B_AFLZone_ShantyTown_BR9` had `Config` set and read back set, but the running editor
   was still instantiating the stale class, so the row produced a component with the parent's null default.
   An editor restart "fixed" it, which is the tell. **Fixed by compiling all three BPs.**
2. **`no UAFLBattleRoyaleComponent on the GameState`** — the component the whole mode is built on was never
   added. `LAS_AFL_BR_S1` supplies the SOLO team setup but **not** the BR component; that lives on
   `EXP_AFL_BR_S1_Test`'s **own** `GameFeatureAction_AddComponents`. The six BR experiences had been
   duplicated from `B_AFLExperience_Arena04_8v8_Haywire`, so they inherited the ARENA row instead.
   **Fixed by rebuilding all six from `EXP_AFL_BR_S1_Test`** — which also hands them the holster row for
   free, and needed no Details-panel work at all.

**The check that catches both, and the one that failed:** asset-registry `get_dependencies`, compared
against the correct sibling. A missing `/Script/<Module>` means a component row is absent. The earlier
inference — "`LAS_AFL_BR_S1` depends on `/Script/AFLCombat`, so it must add the BR component" — was too
weak, because that module supplies dozens of classes. All six experiences now verify
`AFLCombat=True, zone=True`.

*(The editor crashed later in that session on `Ran out of memory allocating 1312 bytes ... paging file is
too small`, at 38.3 GiB virtual — four minutes after the zone had gone idle. Machine memory, unrelated.)*

### Second PIE run, 2026-08-07 — THE RING WORKS, and it found three more defects

**The zone ran its full arc, and every number matched the plan exactly:**

| phase | radius | ratio | interval | expected (hold + shrink) | dps |
|---|---|---|---|---|---|
| 0 | 17500 | — | — | — | 3.0 |
| 1 | 10175 | 0.5814 | 65s | 35 + 30 = **65** | 13.7 |
| 2 | 5916 | 0.5815 | 47s | 22 + 25 = **47** | 24.3 |
| 3 | 3440 | 0.5815 | 42s | 22 + 20 = **42** | 35.0 |
| 4 | 2000 | 0.5814 | 38s | 22 + 15 = **37** | 35.0 (Final) |

Constant geometric ratio, timings exact to the tick, final radius hit precisely, damage ramp linear.
**Containment held on all four transitions** (centre distance + child radius ≤ parent radius), live, on the
seed a real match happened to draw — the property the 400-seed headless test asserts. Zero warnings: config
loaded (centre `(870,7425)`, the measured town core — not world origin) and the BR component was found.

#### ⛔ DEFECT 1 — THE ZONE DID NOT STOP WHEN THE MATCH ENDED. **Mine.**

    01.37.15  match starts, respawn BLOCKED on 144 ASCs
    01.38.20  first shrink completes -> elimination cascade
    ~01.38.57 last-standing resolves -> respawn RESTORED on 144 ASCs
    01.39.07  zone advances to phase 2 ...
    01.40.27  ... phase 4 Final. Still lethal.
    01.42.35+ still killing, ~4 minutes after the contest ended.

The zone ran from MatchId to `EndPlay` with **nothing tying it to the contest it belongs to**. The BR
component restores respawn at last-standing — correct for post-game — so players respawned into a
still-shrinking lethal ring, died, respawned, died, with nothing to stop it. **60 of the session's 76
deaths happened after the match had already been won.**

> ⚠ **A CORRECTION, recorded because the wrong version was committed first.** This was initially written up
> as the cause of the previous session's editor OOM, and the operator's own "machine memory" diagnosis was
> contradicted. **That was wrong.** The zone never started in the session that crashed — it had no BR
> component, logged `no UAFLBattleRoyaleComponent`, and stayed idle; that log contains **zero**
> `AFL_ZONE_PLAN` lines. The crash was unrelated, and the original diagnosis was right.
>
> The mistake came from reading `B_Hero_BagMan_C_9728` as a pawn *count*. It is a global UObject name
> counter that persists across every PIE session in an editor's lifetime. The actual figure is **77 unique
> pawns**, not ~9,700 — an overstatement of roughly 100×, on which a false causal claim was then built.
> The loop was real and worth fixing; its magnitude and its consequence were not what was claimed.

Fixed: `ServerStopZone()`, latched, called the moment `IsMatchActive()` goes false. Sets `Idle` (which
`IsInsideZone` already treats as "nowhere is outside", so one state change closes the damage path and hides
the ring) and clears the outside tags.

**Why nothing caught it:** the bug is invisible in the plan, in the 9 headless tests, and in the first
minute of the run. It only appears *after* the win condition resolves — the exact moment nobody is
watching the zone.

#### DEFECT 2 — 144 BOTS IN A 9-PLAYER BATTLE ROYALE. Pre-existing.

    AFLBots: human-aware fill -- TeamSize=4 NumTeams=36 Humans=1 -> 143 bot(s) (target 144)

`B_AFLBotFill_BR_S1` had **`TeamSize = 4`** on a SOLO mode, so the human-aware fill targeted
`NumTeams × TeamSize` = 144 and ignored `NumBotsToCreate = 3` entirely. Solo means one participant per team
by definition. **Fixed to 1** → target 36. The 144 was also a large share of the memory pressure.

⚠ Still bracket-blind: `B_AFL_TeamSetup_Solo` authors a fixed **36** teams, shared by all three brackets,
so BR_9 and BR_20 also get 36. Per-bracket team setups need per-bracket `AddComponents` rows.

#### DEFECT 3 — 110 `W_Nameplate` BLUEPRINT ERRORS. Pre-existing, not zone-related.

`SetTeamVisuals` reads `GetDynamicMaterial` unguarded and gets None. Fires on `OnTeamChanged` — and solo BR
means every one of 144 players is their own team, so it churns at spawn and elimination. The timing proves
it is independent of the respawn loop: **18 at 01.36, 7 at 01.37, 85 at 01.38, and ZERO from 01.39 onward**,
which is precisely when the loop was running. Not fixed — a widget defect BR exposes rather than causes.

### PIE RUN 2026-08-07 — FIRST FULL BATTLE ROYALE, AND THE ZONE-STOP FIX PROVEN

Run start-to-stop from the bridge (start PIE, zero calls while live, stop, then read — the discipline the
operator corrected me on). 36 participants, bots mobile for the first time thanks to the navmesh.

**THE MATCH RESOLVED — 35 eliminations, 36 down to 1**, placements booked. Every previous run either
stalled or was cut short.

**Z-assertions now closed in a live match:**

| | |
|---|---|
| **Z1** init + telegraph | `AFL_ZONE_PLAN … seed=303370882 phases=5`, phase 0 published with its successor |
| **Z2** determinism | closed headlessly (9/9); each match draws its own seed, as designed |
| **Z3** DoT outside | 30 of 35 eliminations were the ring |
| **Z4** shrink drives survivors together | match resolved during phase 2 |
| **Z5** server-authoritative | all state server-side; clients render replicated values only |

**THE STOP FIX, MEASURED:**

    03:07:23.324  AFL_BR: respawn RESTORED on 36 player ASC(s)
    03:07:23.652  AFL_ZONE: stopped at phase 2 -- match resolved. Ring is inert.
    -> 328 ms, inside the 0.25s tick. ZERO deaths after. Last death 03:07:23.325, i.e. the
       final elimination itself.

Before the fix the ring kept killing for ~4 minutes past the win, with 60 of 76 deaths landing after the
match had already been decided.

### Still owed
- **COMBAT BARELY KILLS: 30 of 35 eliminations were the zone, only 5 pawn-on-pawn** (up from 1 the run
  before, so the navmesh helped). Bots move and shoot — 115 landed hits — but **46 of 161 damage events
  returned `ZONE_CONSUMED`**, Haywire's body-zone model absorbing the hit with *"no health damage"*. A BR
  decided by weather rather than by fighting is a tuning problem in `UAFLDamageExecCalc`, not in the ring.
- **`ConstructTiledNavMesh: Failed to create navmesh of size 0`** seen once; the editor build reported
  `RebuildAll` in 0.78s, fast for 500x500m. Worth confirming tile coverage.
- **A PIE run that actually reaches the ring.** Everything above was fixed after the run, so Z1 / Z3 / Z4 /
  Z5 remain unproven. Z2 is closed headlessly.
- **S3 viz actor + minimap ring.**
- Bot fill (3 for every field size) and the BR spawn distribution — unchanged, see above.

---

## Gate
Per spec §11: this scope → **operator approval ✅ 2026-08-07** → **S1 spike ✅** → **determinism proof (Z2) ✅** →
curves/tuning → PIE sign-off. Disk state was re-verified before build (no Zone code existed; the BR/round/phase
substrate did). This was the net-new dependency gating the BR mode layer (ShantyTown §2, §3A) — **the C++ half
is no longer the blocker; the editor-side assets in §10 are.**

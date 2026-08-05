# IRONICS — BR SHRINKING ZONE SYSTEM (scope)

**Status:** SCOPE / DESIGN ONLY — **not approved for build** (gated on operator approval + the open decisions §7).
**Date:** 2026-08-05.
**Why this doc:** the shrinking safe-zone ("Zone") is the **single genuinely net-new BR system** — everything else
in the BR ruleset is shipped or reused ([IRONICS_BR_MODE_SPIKE.md](IRONICS_BR_MODE_SPIKE.md) §4;
[ShantyTown_BR_DESIGN.md](maps/ShantyTown_BR_DESIGN.md) §2, §3 Layer A). This scopes it as its own work unit.
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
[IRONICS_BR_MODE_SPIKE.md](IRONICS_BR_MODE_SPIKE.md) · [ShantyTown_BR_DESIGN.md](maps/ShantyTown_BR_DESIGN.md) (§2, §3A, §11) ·
[IRONICS_MATCH_STAKING_SSOT.md](IRONICS_MATCH_STAKING_SSOT.md) · [IRONICS_LEAGUE_ADVANCEMENT_SSOT.md](IRONICS_LEAGUE_ADVANCEMENT_SSOT.md).
Code: `UAFLBattleRoyaleComponent`, `UAFLMatchPhaseComponent`, `UAFLRoundManagerComponent`, `GE_AFL_Damage_BeamTick`,
`FAFLCombatTelemetry` (all `Plugins/GameFeatures/AFLCombat/`).

---

## Gate
Per spec §11: **this scope → operator approval → S1 spike → determinism proof (Z2) → curves/tuning → PIE sign-off.**
A re-sent scope is not approval; disk state is verified before build. **NOT YET APPROVED** — awaiting operator sign-off
and the §7 decisions. This is the net-new dependency gating the BR mode layer (ShantyTown §2, §3A).

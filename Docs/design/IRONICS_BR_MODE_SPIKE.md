# IRONICS — BATTLE ROYALE MODE SPIKE (P0.5)

**Status:** SCOPE / SPIKE PLAN ONLY (no code written). **Date:** 2026-08-05.
**Purpose:** de-risk the Battle Royale game mode — the prerequisite SYSTEM that gates the ShantyTown BR map
([ShantyTown_BR_DESIGN.md](ShantyTown_BR_DESIGN.md)) and the roster's `BR_18` / `BR_36` slots — by proving a
**minimal end-to-end BR loop on the EXISTING AFL match substrate** before committing to a full build.
**Primary users:** Staked Battle Royales & Tournaments (server-authoritative, deterministic, exploit-free throughout).

**Siblings/refs:** [IRONICS_GAME_MODES_SSOT.md](IRONICS_GAME_MODES_SSOT.md) · [IRONICS_MAP_MODE_SPEC.md](IRONICS_MAP_MODE_SPEC.md) ·
[IRONICS_MATCH_STAKING_SSOT.md](IRONICS_MATCH_STAKING_SSOT.md) · [IRONICS_LEAGUE_ADVANCEMENT_SSOT.md](IRONICS_LEAGUE_ADVANCEMENT_SSOT.md) ·
[IRONICS_LOOT_CARRY_MODEL.md](../_archive/IRONICS_LOOT_CARRY_MODEL.md) · [IRONICS_TEAM_ASSIGNMENT_SSOT.md](IRONICS_TEAM_ASSIGNMENT_SSOT.md).

---

## 1. What a "spike" delivers here
A **vertical slice**, not the shipping mode: the smallest end-to-end proof that answers *"can we run a last-standing,
shrinking-zone match, server-authoritatively and deterministically, on our existing Lyra/AFL stack?"* — so the real
build is de-risked and estimable. Output = a runnable PIE/headless slice + a findings addendum to this doc.

## 2. Where BR sits in the architecture (critical framing)
- A **"mode" = a Lyra `ULyraExperienceDefinition`** (GAME_MODES_SSOT §1). Arena already proves the pattern: the
  **match structure** lives in a GameState component added via the experience's `AddComponents` row.
- **BR is a new MATCH-STRUCTURE layer**, sibling to `UAFLRoundManagerComponent` (Arena round FSM) and
  `UAFLDeathmatchRankComponent` (Melee) — **orthogonal to the Haywire/ProMod/Melee *combat* split**. So:
  - BR = new `UAFLBattleRoyaleComponent` + new zone system + new BR experience(s).
  - Per the **dual-GE requirement**, BR must run in both **Haywire** and **Pro Mod** flavors (combat rules are a
    separate layer). Expect `B_AFLExperience_ShantyBR_18_Haywire` / `_ProMod` etc., wired like ARCANEON's four.
- The Melee component "mirrors `UAFLRoundManagerComponent`" (GAME_MODES_SSOT §7.3) — **the exact precedent** for
  adding a BR match-structure component. We follow it.

## 3. REUSE inventory — what already exists (the spine is mostly built)
| Need | Existing asset/class | Notes |
|---|---|---|
| Match clock / phases | `UAFLMatchPhaseComponent` (`AFLCombat/.../Public/Phases/`) | Warmup→Playing→PostGame spine via Lyra GamePhase (BP-shell + C++-driver) |
| **Last-standing primitive** | `UAFLRoundManagerComponent` (`.../Public/Round/`) | Server-auth **`AliveCount(team)`** + wipe detection via `ULyraHealthComponent::OnDeathStarted`; **generalize to N participants** |
| Elimination signals | `UAFLDeathComponent`, ShooterCore Elim/Assist processors | death → resolve already wired |
| **Staking backend hook** | `MatchId` (FGuid, server-authored+replicated) on the round manager | earn/stake contract id; reuse verbatim |
| Staking-aware integrity | `IAFLMatchTierSource` (sign-free score delta) | comment explicitly cites "staking and MMR in the mode" — anti-rubber-band already designed |
| Respawn gating | `State.Round.NoRespawn` + `AAFLGameMode::ControllerCanRestart` + `IAFLRoundRestartPolicy` | flip to BR's no-/limited-respawn rule |
| Loot | `DA_AFL_LootConfig`, `BP_AFL_LootCacheCarry`, `UAFLLootCarryComponent` | carry model exists; **world scatter/spawn is the gap** |
| Telemetry / dispute | `UAFLCombatTelemetry`, `EmitRoundTelemetry`, traversal sampler | reuse for placement + replay/dispute data |
| Automated proof | `AFLMatchTestRunner`, `AFLPhaseTestRunner` | headless harness precedent for the spike's deterministic test |
| Teams / squads | `ULyraTeamSubsystem`, [TEAM_ASSIGNMENT_SSOT](IRONICS_TEAM_ASSIGNMENT_SSOT.md) | squads = teams; solo = N teams of 1 |

## 4. NET-NEW inventory — what the spike must build
1. **Shrinking Zone system** *(the #1 piece — nothing like it exists; grep for Zone/Storm/Shrink = empty).* See §6.
2. **`UAFLBattleRoyaleComponent`** — mirror `UAFLRoundManagerComponent`, but win = **last participant/squad standing**
   among N (not 2-team best-of), tracking **eliminations + placement rank (1..N)** for staked payout. Reuses
   `AliveCount`, `OnDeathStarted` binding, `MatchId`, telemetry.
3. **Drop / deploy entry** — BR match start (distributed deploy + grace for the spike; full airdrop bus/pods later).
4. **World loot distribution** — wire `DA_AFL_LootConfig` to scattered world spawners + pickup (carry already exists).
5. **Placement → staking/league feed** — placement result → [MATCH_STAKING](IRONICS_MATCH_STAKING_SSOT.md) payout
   (buy-in in **non-cashable Watts/Volts, NO cashout**) + [LEAGUE_ADVANCEMENT](IRONICS_LEAGUE_ADVANCEMENT_SSOT.md) rank/ARC.
6. **Water short-out death handling** — reconcile the ShantyTown lethal-depth rule with BR elimination (§4 of the map
   doc): is a water death an **elimination**, a **penalized respawn**, or **downed**? (Open decision — §9.)

## 5. The minimal spike deliverable (vertical slice)
Smallest thing that proves the loop end-to-end:
- **1 human + N bots** (bots via existing `B_AFL_BotController`), **solo** (squads deferred).
- **Distributed deploy** (spread spawns + brief grace) — not the full airdrop.
- **ONE shrinking zone**: 2–3 phases, telegraphed next circle, **DoT gameplay effect outside**, server-authoritative,
  **seeded/deterministic** center.
- **Last-standing win** → **placement result** (1..N) emitted to telemetry (payout/rank wiring stubbed).
- Runs in **PIE** and as a **headless deterministic test** (mirror `AFLMatchTestRunner`).
- On a **throwaway test level or a ShantyTown greybox sub-region** (not the finished map — map art is P3+).
- **Explicitly OUT of the slice:** airdrop bus, squads, gulag/respawn tokens, full loot tables, spectator UI, final
  balance. Those are post-spike, informed by findings.

## 6. Zone system architecture (the risky net-new piece)
Honor the **Lyra export boundary** (MATCH_PHASE header): `ULyraGamePhaseAbility` is not exported, so zone *phases*
are **BP shells (phase tag only)** and all timing/shrink logic lives in a **C++ GameState component driver**
(`UAFLZoneComponent`) — same shape as `UAFLMatchPhaseComponent`.
- **State (replicated):** current center + radius, target center + radius, phase index, time-to-shrink. Plain
  replicated UPROPERTYs (per the round manager's NET SAFETY note — no custom net-serialized struct).
- **Damage:** periodic **DoT GameplayEffect** applied server-side to any pawn outside current radius, magnitude
  scaling per phase. (GAS is already the damage substrate.)
- **Telegraph:** next-circle center/radius published to the HUD before each shrink (fair-warning; tournament-critical).
- **Determinism:** center sequence from a **per-match seed** (reuse/extend `MatchId`), logged via telemetry →
  reproducible for dispute review. **No client input to zone placement, ever.**
- **Visualization:** a wall/dome material actor driven by the replicated radius (cosmetic; authority is the volume).
- **Water interaction:** zone shrink can push players toward/over water — the lethal-depth rule (map doc §4) and the
  zone DoT are independent hazards; both server-side, so no double-jeopardy exploit.

## 7. Staked & tournament requirements (folded into every piece)
Server-authoritative zone + elimination + loot; **deterministic seeded zone**; `MatchId` on every match; telemetry
for placement + **replay/dispute review**; spectator delay (stream-snipe mitigation); disconnect/rejoin + AFK policy
tied to stake; teaming/collusion watch (real money = incentive). Integrations per §4.5. Perf profiled at 18/36
(landscape + foliage + N players) — stutter = latency = unacceptable when staked.

## 8. Spike phases
| Step | Output |
|---|---|
| **S1** | Stand up `UAFLBattleRoyaleComponent` (mirror round manager) — N-participant last-standing + placement, on a test experience |
| **S2** | `UAFLZoneComponent` + BP phase shells + DoT GE — one seeded, telegraphed, shrinking circle |
| **S3** | Distributed deploy + grace; wire elimination → placement → telemetry |
| **S4** | Headless deterministic test (mirror `AFLMatchTestRunner`) + PIE watch (human + bots) |
| **S5** | Findings addendum here: what worked, real build estimate, cut/keep list, next-phase go/no-go |

## 9. Open decisions (gate the full build; several inherited from the map doc §9)
1. **Respawn model:** no-respawn (pure BR) vs limited (tokens/gulag) vs the map's "respawn-on-land" water rule — pick the BR baseline.
2. **Solo vs squads first** (spike = solo; squads = teams via `ULyraTeamSubsystem`).
3. **Drop fidelity:** distributed-spawn (spike) → airdrop bus/pods (ship)?
4. **First target:** `BR_18` (recommended) vs `BR_36` (map doc §9).
5. **Zone shape/count/timing curve** — tuning, post-slice.

## 10. Risks
1. **Zone determinism under net** — the one genuinely new system; must be provably reproducible for staked disputes.
2. **N-participant scaling** of the 2-team round logic (AliveCount/attribution generalization).
3. **Perf at 36** on landscape + foliage.
4. **Loot world-scatter** is only half-built (carry exists, distribution doesn't).
5. **Backend** — staking payout / league write shapes for placement may be undemonstrated (cf. Melee ranking risk, GAME_MODES_SSOT §9).

---

## Verdict
The BR mode is **far less greenfield than feared** — the match spine, last-standing primitive, staking id, respawn
gating, loot carry, telemetry, and a headless test harness already exist. The real net-new work is the **shrinking
zone** (§6) and **generalizing win/placement to N** (§4.2). The spike (§5, §8) proves both cheaply before commitment.

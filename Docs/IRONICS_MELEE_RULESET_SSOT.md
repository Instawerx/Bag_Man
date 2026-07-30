# IRONICS — MELEE RULESET SSOT (instant-respawn / K+A−D ranking)

**Status:** DESIGN OF RECORD — operator-approved 2026-07-30. Parent: `IRONICS_GAME_MODES_SSOT.md`.
Extends `IRONICS_LEAGUE_ADVANCEMENT_SSOT.md §3.2/§7` (rank derives from the existing K/D/A StatTags).

## 1. What "Melee Mode" is

An **instant-respawn deathmatch** ruleset. **NOT melee-weapon restricted** (operator decision 2026-07-30) — it runs
with the **full existing arsenal**; "Melee" is the label for the fast-respawn/KDA arcade feel. Ranking =
**kills + kill-assists − deaths**. Shares Pro Mod's clean-health gate + FBIK + pawn (`B_Hero_BagMan_Pro`).
Experience: `B_Experience_Melee` (duplicate of `B_Experience_ProMod` + the ranking component + instant respawn).

## 2. Ranking — `UAFLDeathmatchRankComponent` (NEW)

Mirror the proven `UAFLRoundManagerComponent` (`Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Round/`):
a `UGameStateComponent` + `IAFLRoundRestartPolicy`, server-authoritative FSM, replicated state. New files:
- `.../Public/Round/AFLDeathmatchRankComponent.h`
- `.../Private/Round/AFLDeathmatchRankComponent.cpp`

Contract:
- **Score source = existing replicated StatTags** on `ULyraPlayerState` — `ShooterGame.Score.{Eliminations,Deaths,Assists}`
  (already tracked server-side; read today at `AFLCombat/.../UI/AFLW_MatchScoreboard.cpp:159-178`). **No new scoring
  pipeline** — kills/assists/deaths already flow via `Lyra.Elimination.Message` → `AssistProcessor` → `AddStatTagStack`.
- **Formula:** `Rank = Eliminations + Assists − Deaths`, computed server-side each update.
- **Replicated state:** a sorted leaderboard array `{playerId, K, A, D, rank}` + win condition (`KillTarget` and/or
  `TimeLimit`). Follow the round manager's `Team0Score`/`Team1Score` replication pattern.
- **Win/end → reuse `UAFLMatchPhaseComponent.ConcludeMatch()`** on first player reaching `KillTarget` (or top rank at
  timeout). Do NOT reimplement match end.
- **`ShouldBlockRestart() → false`** (never suppress → instant respawn).
- Bind to `ULyraHealthComponent::OnDeathStarted` (the reconciled signal the round manager already uses) to trigger
  immediate respawn + leaderboard refresh.
- Added to the game state via `GameFeatureAction_AddComponents` in `B_Experience_Melee`.

## 3. Instant respawn (reuse — zero game-mode edits)

- **`GA_AFL_AutoRespawn_Instant`** — variant of `Plugins/GameFeatures/AFLBagMan/Content/Abilities/GA_AFL_AutoRespawn.uasset`
  with the latent Delay set to **0** (or a mode param). Grant via the Melee ability set.
- `bAllowMidRoundRespawn = true` for the mode; the Melee component **never** calls `SetRoundRespawnSuppressed(true)`
  (so `State.Round.NoRespawn` is never applied).
- `AAFLGameMode::ControllerCanRestart` already consults `IAFLRoundRestartPolicy::ShouldBlockRestart()` → the Melee
  component returns `false` → restart always permitted. **No `AFLGameMode` change.**

## 4. Ranking persistence — session-local MVP → durable ladder

**Milestone 1 (ship first): session/match-local.** The leaderboard lives only on `UAFLDeathmatchRankComponent`
(replicated), resets each match. **Zero backend risk.** This is the playable Melee MVP.

**Milestone 2 (target): durable cross-match ladder.** On `ConcludeMatch()`, write final K/A/D + rank to PlayFab via
the existing persistence seam (`IAFLCosmeticPersistence` / `UAFLEconomyPersistenceSubsystem`). ⚠ This is a **new,
undemonstrated write shape** (`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md :30-36, :349-351`) — de-risk carefully; it does not
block Milestone 1.

**Milestone 3 (future): skill ladder.** A true MMR ladder needs **Glicko-2** (design-only, unbuilt — AFL-2201,
`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md §5.1`). A within-match KDA leaderboard does NOT need it; a season/skill ladder does.

## 5. Build order (tracker Phase 3)

1. Duplicate `B_Experience_ProMod` → `B_Experience_Melee`.
2. `UAFLDeathmatchRankComponent` (C++, mirror RoundManager). *(editor-closed two-engine build)*
3. Wire via AddComponents; `ConcludeMatch()` on win.
4. `GA_AFL_AutoRespawn_Instant` (0 delay) + allow mid-round + no suppression.
5. Session-local leaderboard replication + HUD readout (reuse the scoreboard read path).
6. PIE: kill → instant respawn, rank updates live, match ends on target. **Melee MVP done.**
7. (Phase 4) Durable PlayFab write; (later) Glicko-2 ladder.

## 6. Verification
`?Experience=B_Experience_Melee` — kill → **instant** respawn (no timer), leaderboard updates `K+A−D` live and
sorts, match ends at `KillTarget` via `ConcludeMatch()`, no `State.Round.NoRespawn` suppression, full arsenal usable.
</content>

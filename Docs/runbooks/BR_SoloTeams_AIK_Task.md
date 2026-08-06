# AIK Task — `B_AFL_TeamSetup_Solo` (make the BR harness SOLO / FFA)

**Purpose:** switch the Battle Royale harness from a 2-team population to **one team per participant (FFA)**, so the
solo last-standing win condition can always resolve to a single survivor. **Config only — no C++.**
**Date:** 2026-08-05. **Owner lane:** AIK (in-editor BP + action-set authoring), or bridge (me) when the editor is idle.

## Why (diagnosis, BLOCK 177 + team-setup read)
- `UAFLBattleRoyaleComponent` (win condition) is **SOLO last-standing** — it counts every participant individually,
  ends at `alive ≤ 1`. **PIE-proven working** (run 1: 8→1, winner PS_0).
- But the harness experience `EXP_AFL_BR_S1_Test` pulls in action set `LAS_AFL_BR_S1`, which adds
  **`B_AFL_TeamSetup_TwoTeams`** → the 8 bots get split into **2 teams (4v4)**.
- Solo win rule on a 2-team population = a **probabilistic stall**: it concludes only when the final two are on
  opposite teams; if the last survivors are **teammates**, bots won't kill each other → `alive` sticks at 2 →
  `alive ≤ 1` never reached → the "2 alive, no MATCH END" stall. (Map is irrelevant — the component is map-agnostic.)

## Key finding — this is CONFIG ONLY
`B_AFL_TeamSetup_TwoTeams` is a BP of C++ **`UAFLTeamCreationComponent`** (`AFLGameCore`). That class keeps stock
team **creation** from its `TeamsToCreate` map, but delegates **assignment** to **`UAFLLocalFillProvider`**, which
**balances across ALL created teams** (`PickLeastPopulated` over `ULyraTeamSubsystem::GetTeamIDs()`, re-counted per
join). So a setup that **creates N ≥ participant-count empty teams** makes every joiner land on the lowest-id empty
team → **one participant per team = FFA.** Traced: human → team 1, bots → teams 2,3,…,8 (all distinct). **No code
change; do NOT touch `UAFLBattleRoyaleComponent`.**

## §A — AIK PROMPT (paste into UE Tools → Agent Chat)
```
GOAL: Make the BR harness SOLO (FFA), one team per participant. Config only -- no C++.

1. INSPECT FIRST: open /AFLBagMan/Game/B_AFL_TeamSetup_TwoTeams and note its PARENT CLASS
   (UAFLTeamCreationComponent) + its non-default settings (PublicTeamInfoClass /
   PrivateTeamInfoClass / the display asset it uses). Show your plan before creating.
2. Create a Blueprint B_AFL_TeamSetup_Solo in /AFLBagMan/Game/ , PARENT CLASS =
   UAFLTeamCreationComponent (same parent as TwoTeams -- it is a COMPONENT class, not an actor).
3. On its class defaults, set TeamsToCreate to keys 1,2,3,...,36 inclusive (36 covers BR_36).
   Leave each display-asset value None, OR set all to the same display asset TwoTeams uses
   (cosmetic only -- does not affect assignment). Match every other property to the TwoTeams
   parent defaults (PublicTeamInfoClass / PrivateTeamInfoClass etc.).
4. Open action set /AFLBagMan/Experiences/LAS_AFL_BR_S1. In its GameFeatureAction_AddComponents,
   find the ONE entry whose ComponentClass is B_AFL_TeamSetup_TwoTeams (added on the GameState)
   and change ONLY that ComponentClass to B_AFL_TeamSetup_Solo. Do NOT touch the bot-fill entry
   (B_AFLBotFill_BR_S1), any other action, or any other component.
5. Save B_AFL_TeamSetup_Solo and LAS_AFL_BR_S1.

DO NOT: modify B_AFL_TeamSetup_TwoTeams (shared by Arena/Team modes), edit
UAFLBattleRoyaleComponent, or change any experience other than via the LAS_AFL_BR_S1 swap.

VERIFY: PIE the BR harness (EXP_AFL_BR_S1_Test, 8 bots). The AFL_BR_STATE [MATCH_START] roster
must now show EIGHT DISTINCT teamIds (1..8), not two. The match should reach MATCH END every run.
```

## §B — Alternative (bridge, me)
When the editor is idle (ShantyTown finished loading), I can author both assets via the unreal-editor bridge with
the same steps, then hand off the PIE verify. (Not while the editor is mid-heavy-load — bridge writes during heavy
ops are unstable, and two editor processes on one project conflict.)

## Verification (either path)
PIE `EXP_AFL_BR_S1_Test` with 8 bots → the instrumented `AFL_BR_STATE: [MATCH_START]` roster shows **8 distinct
`teamId`s (1..8)**, and the match reaches `AFL_BR: MATCH END` on every run (no same-team stall). This also
closes the remaining risk from the retracted team-split theory.

## Notes / follow-ups (NOT this task)
- The in-session **match-restart degradation** (stale `State.Round.NoRespawn` → carried-over dead participant,
  `alive < TotalParticipants` on re-run) is a **separate** open item — its own fix block.
- Team ids are `uint8`; 36 is well within range. For production BR the 36 solo teams are what you want anyway.

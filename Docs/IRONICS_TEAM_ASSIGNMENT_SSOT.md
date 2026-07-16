# IRONICS — Team Assignment & Spawn SSOT (Design v0.1)

Status: DESIGN (ruled). Serves BOTH best game-design practice AND best user experience — every decision below
is justified against both. T1-first sequencing; swappable provider seam.

Interlocks: MAP_MODE_SPEC (§1 modes, §3 spawn tiers, §11.7 spawn rules) · LEAGUE_ADVANCEMENT_SSOT (§5 Glicko-2
MMR drives matchmaking) · ACHIEVEMENTS_SSOT (§3 team-champion gate) · MATCH_STAKING_SSOT (§1.2 MatchmakerData
team hook, §6b party rules).

## §0 Ruled decisions
0.1 Assignment source = MATCHMAKER-AUTHORITATIVE in ranked; LOCAL-FILL in offline/casual/PIE. Delivered via a
    swappable IAFLTeamAssignmentProvider seam (§1). [Practice: balanced teams from the MMR pool, not random
    fill. UX: game always has a valid team source — never blocked on the backend.]
0.2 Bot-fill: YES in offline/casual/PIE (a match always starts); NO in ranked (ranked holds for real players,
    with visible wait-time UX). [Practice: ranked integrity — can't rank against bots. UX: no empty-lobby
    dead-end in casual/solo; honest wait in ranked.]
0.3 Party integrity: §6b HARD RULE — same-party NEVER on opposing sides (absolute). Balance handled by the
    matchmaker's party-clustering cap (≤ one team's worth of seats), not by splitting a party across sides.
    [Practice: no party stacked against its own members; cluster-cap prevents a stack = a whole small team. UX:
    you always play WITH your party, never against them.]
0.4 Sequencing: T1 (in-match seam + local provider + spawn rules, playable NOW) → T2 (online matchmaking service
    behind the seam) → T3 (team scoring aggregation, shared w/ League). [Practice: canary-before-scaling. UX:
    teams+maps playable immediately.]
0.5 The existing AFL consumption layer (UAFLRoundManagerComponent: dynamic GetTeamIDs, per-team round/extraction/
    match-end, the shipped BeginPlay retry-guard) is UNTOUCHED — the design feeds it, never rewrites it. The
    subclass-override implementation ruled in §2 (UAFLTeamCreationComponent : ULyraTeamCreationComponent, only
    the assignment DECISION overridden, stock creation kept) is precisely what preserves this untouched-
    consumption guarantee — no parallel assigner, no downstream rewrite.

## §1 The provider seam — IAFLTeamAssignmentProvider (architecture)
Interface (C++, always-loaded module): 
  struct FAFLTeamAssignment { FString PlayerId; FGenericTeamId TeamId; }
  IAFLTeamAssignmentProvider:
    - virtual void RequestAssignments(const TArray<APlayerController*>& Players, FOnAssignmentsReady) = 0;
      (async-capable — the round manager's shipped retry-guard already tolerates late teams, so provider may
       resolve asynchronously)
    - virtual bool IsAuthoritative() const = 0;  (ranked provider = true; local = false)
A single UAFLTeamAssignmentComponent (on GameMode/GameState) holds the active provider, resolves assignments,
and applies FGenericTeamId to each pawn/controller via the existing ILyraTeamAgentInterface path (the same
MyTeamID replication ALyraCharacter already uses). The round manager consumes teams via GetTeamIDs() EXACTLY as
today — it never knows which provider produced them. 
[Practice: match fully decoupled from the backend — swap provider, nothing downstream changes. UX: the swap is
invisible to the player; teams "just work" in every context.]

## §2 LocalFillProvider (offline / casual / PIE) — the UX floor
RULED: implemented as UAFLTeamCreationComponent : ULyraTeamCreationComponent — OVERRIDE the assignment method
(ServerAssignPlayersToTeams / ServerChooseTeamForPlayer) to delegate to the active IAFLTeamAssignmentProvider.
Stock team CREATION (TeamDA_* data assets, team registration) is KEPT; only the assignment DECISION is
overridden. This is subclass-override, NOT a parallel system — running a provider alongside stock fill would
race two assigners (the drift hazard). Keeps §0.5 true: the round-manager consumption layer AND stock team
infrastructure both untouched; only the assignment source changes.
Deterministic assignment to the mode's team count,
BOT-FILL to team size (0.2), party-together honored (0.3). Produces the same FAFLTeamAssignment array the ranked
provider will. This is what makes solo/offline/PIE fully playable NOW and is the T1 test vehicle.
[Practice: AAA backfill standard (never make the player wait on population). UX: a match ALWAYS starts.]

## §3 MatchmakerDataProvider (ranked / online) — the best-practice source (T2)
Reads GameLift onStartGameSession → MatchmakerData JSON (MATCH_STAKING §1.2): players + attributes + team
assignments, produced by the PlayFab queue balanced on Glicko-2 MMR (LEAGUE §5). Enforces §6b at the matchmaker
(no opposing-side parties; cluster-cap). Drop-in swap for LocalFillProvider — same FAFLTeamAssignment output.
Pools by mode-family (Arena / Team / BR separate; LEAGUE §7.3). Placement/soft-reset + stricter ranked MMR bands
(AFL-2205). Ranked = no bot-fill (0.2).
[Practice: balanced, MMR-pooled, integrity-preserving. UX: fair matches; fast where population allows; honest
wait where it doesn't.]
T2 ACCEPTANCE SEAM — IDENTITY JOIN: MatchmakerData PlayerIds must reconcile to the in-match APlayerControllers.
The §1 async-resolve covers TIMING (late teams OK); the ID-JOIN is separate: players connect carrying a
PlayFab/GameLift player-session ID (in connect options) → reconciled to the matchmaker roster on PostLogin.
T2 MUST prove this join (right player → right team) as an explicit acceptance criterion — it is the classic
dedicated-server seam and must not be discovered mid-build. The LocalFillProvider (T1) sidesteps it entirely
(it assigns the already-connected controllers directly), which is why T1 is testable without it.

## §4 Team-aware spawn rules (MAP_MODE §3 / §11.7)
Beyond stock: mirror/rotational spawns with side-swap between rounds (best-of integrity); 1.5s State.Invulnerable
on spawn; NO-ENEMY-LOS spawn selection; anti-camp (avoid recently-contested/recently-died points). Tier-scaled
per MAP_MODE §3 (Duel: no-enemy-LOS mandatory; Mid-Arena: 3-lane/figure-8 aware; Large-Team: distributed).
Implemented as an AFL spawn selector consuming the assigned FGenericTeamId. Depends on team assignment (§1) but
NOT on matchmaking — testable in T1.
[Practice: competitive spawn fairness (mirror, no spawn advantage). UX: you never spawn into a bullet — felt
fairness. Here best-practice and best-UX are the SAME choice.]

## §5 Party integrity (MATCH_STAKING §6b)
Hard: same-party never opposing sides. Same-party same-team allowed up to team size. Party clustering in staked
BR ≤ one team's worth of seats. Enforced at the matchmaker (T2); the local provider honors party-together in
casual (T1).

## §6 Team scoring aggregation (shared with League — the narrow gap, T3)
Per-team score rollup on the EXISTING P-SCORING server-authoritative event stream (LEAGUE §7 names this the
narrow gap). Aggregates the existing per-player StatTags (K/D/A, extraction bank, head-collect) into per-team
standings. Unlocks ACHIEVEMENTS §3 Team.<Name>.Champion (top-K season team standing) + team leaderboards
(LEAGUE §5). SHARED with the Achievement/League sprint — build ONCE, flagged in both.

## §7 Phasing
T1 — IN-MATCH TEAM SEAM (NOW, playable, 2-client PIE):
  IAFLTeamAssignmentProvider + UAFLTeamAssignmentComponent + LocalFillProvider (bot-fill, party-together) +
  team-aware spawn rules (§4) on the built Arena01_3v3/4v4 experiences. Verify the built round/extraction/
  match-end layer runs clean with provider-assigned teams. MILESTONE: team matches playable on real maps →
  unlocks the map work.
T2 — ONLINE MATCHMAKING SERVICE (behind the seam):
  PlayFab queue + ticket flow + party/lobby + GameLift StartGameSessionPlacement + fleet + MatchmakerDataProvider
  (drop-in). Ranked pooling by mode-family, MMR bands, placement/soft-reset. Reuses the AFLOnline HMAC/transport
  spine.
  T2 GATE: the identity join (MatchmakerData PlayerId ↔ APlayerController, §3) is an explicit T2 acceptance
  criterion — prove right-player→right-team before scaling, not mid-build.
T3 — TEAM SCORING AGGREGATION (shared w/ League):
  Per-team rollup (§6) → team-champion achievement + team leaderboards.

## §8 Lane split
AIK (editor-open): experience config (spawn selector wiring, provider component placement on GameMode/GameState),
  team spawn-point setup on maps, PIE-proof team assignment + spawns.
Claude Code (editor-closed/backend): the C++ interface + component + providers (LocalFill now, MatchmakerData in
  T2), the PlayFab/GameLift/Lambda backend (T2), per-team aggregation (T3), git.
Operator: builds, 2-client PIE watches (T1 team-assignment + spawn proofs), deploys (T2), commits.

## §9 Open flags (deferred, NON-blocking)
- Rank MODEL (points vs hybrid, LEAGUE #2) — affects the visible ladder, NOT team assignment (which consumes
  MMR, already ruled). Deferred to the League sprint.
- Team-champion gate exact top-K (ACHIEVEMENTS §3 leaves K to tune) — needs T3 aggregation to exist first.
- BR (18/36) team/squad rules — MAP_MODE §7 Phase-B, gated on netcode; out of this sprint's Arena/Team scope.

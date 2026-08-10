# S12-E — Player sessions: validated identity, disconnect, and reconnect

Scope for the last open item in `S12_GAMELIFT_INTEGRATION_PLAN.md` §3E. A–D and F are closed; this is what
stands between S12 and done.

---

## 1. The problem, stated precisely

`AAFLGameMode::InitNewPlayer` (`AFLGameMode.cpp:53`) reads `?PlayFabId=` off the connect URL and stashes it as
the reconcile id. **Nothing validates it.** There is no `PreLogin` override and no call to
`AcceptPlayerSession` anywhere in the project — verified by search, not assumed.

Every downstream system then trusts that value:

- `UAFLMatchmakerDataProvider` assigns a team by matching it against the roster
- `FAFLMatchReporter` escrows **10 VO from the account it names**
- settlement pays out against it, and rating moves against it

So a client that connects with `?PlayFabId=<someone else's id>` is that player as far as the server is
concerned. In a staked match that debits a stranger's wallet. This is the last unvalidated input in an
otherwise closed loop: the roster is authoritative, the economics come from the payload, bots are barred,
escrow refuses unverifiable rosters — and then identity is taken on the client's word.

The plan predicted it: *"a silent divergence here would mis-escrow players."*

## 2. What already lines up

Three things make this smaller than it looks — all verified this session:

- **The allocator already creates player sessions.** Its response carries
  `placedPlayerSessions: [{ playerId, playerSessionId }]`, observed live, e.g.
  `psess-ea639add-5588-1448-5976-954b63e84bc4`.
- **`playerId` IS the PlayFab entity id** — the same value the client sends as `?PlayFabId=` and that the
  roster keys on. GameLift's identity and our reconcile key already agree by construction.
- **The vendored SDK exposes the whole surface** (`GameLiftServerSDK.h:141-147`):
  `AcceptPlayerSession`, `RemovePlayerSession`, `DescribePlayerSessions`,
  `UpdatePlayerSessionCreationPolicy`.

So no new identity concept is required. The work is to stop trusting the client's claim and start deriving
it from a validated session.

## 3. Design

### 3.1 Derive identity, never accept it

The core change in one line: **the reconcile id must come from GameLift, not from the URL.**

```
client connects with ?PlayerSessionId=psess-...
  PreLogin:  AcceptPlayerSession(psess-...)        -> reject the connection on failure
             DescribePlayerSessions(psess-...)     -> read PlayerId (the PlayFab entity id)
  InitNewPlayer: stash THAT PlayerId as the reconcile id
```

`?PlayFabId=` becomes, at most, a cross-check to log a mismatch. It must stop being the source.

⚠ `AcceptPlayerSession` returns a generic success/failure outcome — it does **not** hand back the PlayerId.
`DescribePlayerSessions` is what yields it. Both calls are needed; a design that only accepts and then keeps
reading the URL has changed nothing.

### 3.2 Reject in PreLogin, not later

`PreLogin` is the only place that can refuse a connection cleanly, before a controller or PlayerState exists.
Rejecting later means tearing down a half-joined player.

⚠ **THE GATE MUST BE CONDITIONAL, and this is the trap that was already flagged once.** A `PreLogin` that
rejects whenever a session id is absent locks out every PIE session, listen-server host, and offline run —
raised at the start of this workstream as a defect in an earlier proposal, and it is still the failure mode
to avoid. The condition is *"is this match's roster externally owned?"*, not *"is the parameter present?"*

`UAFLMatchmakerDataProvider::IsRosterExternallyOwned()` already answers exactly that question and is already
used to gate bot fill. Reuse it. Same predicate, same reasoning: under GameLift, validate; otherwise, this is
a local match and there is no session to validate against.

### 3.3 Lock the session once the match starts

`UpdatePlayerSessionCreationPolicy(DENY_ALL)` at match start. A staked match's roster is settled before
anyone connects; nothing should be able to place a new player into it mid-match. Cheap, and it closes late
injection.

Reopen to `ACCEPT_ALL` only if a reconnect window is active (see below) — the two features interact.

## 4. Disconnect and reconnect

The part with the most unknowns, and where the economy makes the stakes real.

### 4.1 Do not remove the session on every disconnect

The naive implementation calls `RemovePlayerSession` in `Logout`. That is wrong for a staked match: it
releases the slot immediately, and a player whose wifi dropped for four seconds has lost their seat — and
their escrowed stake — with no way back.

Proposed protocol:

| event | action |
|---|---|
| player disconnects | start a **reconnect grace window**; keep the seat; do NOT `RemovePlayerSession` |
| reconnects within window | restore to the same team/seat; no new escrow |
| window expires | `RemovePlayerSession`; the seat is gone; resolve the match economically (see 4.3) |
| player leaves deliberately | `RemovePlayerSession` immediately; treat as forfeit, not abandonment |

Distinguishing a deliberate leave from a dropout requires an explicit client signal; absent one, everything
is a dropout and only the timer resolves it.

### 4.2 ✅ ANSWERED BY LIVE TEST — 2026-08-09

Measured against real GameLift with the `afl.GameLift.SessionProbe` harness, on a throwaway placement with no
clients. Verbatim results:

```
player sessions became visible after 1.44s (attempt 3) -- 2 session(s)
[0/baseline]            status=RESERVED
1/accept #1                                            -> SUCCESS
[1/after accept #1]     status=ACTIVE
2/accept #2 (ALREADY ACTIVE -- the reconnect question) -> SUCCESS
[2/after accept #2]     status=ACTIVE
3/remove                                               -> SUCCESS
[3/after remove]        status=COMPLETED
4/accept after remove                                  -> FAILED
    InvalidPlayerSessionStatusException: PlayerSession (...) has a status of COMPLETED instead of RESERVED
[4/after accept-post-remove] status=COMPLETED
```

**RE-ACCEPT WORKS.** A session that is already `ACTIVE` accepts again and stays `ACTIVE`. So a reconnecting
client presents the id it already holds and the server simply accepts it again. **No `CreatePlayerSession`
endpoint, no client-side reconnect flow, no new backend surface** — the reconnect path is the cheap one.

**REMOVAL IS TERMINAL AND IRREVERSIBLE.** Once `COMPLETED`, that id can never be accepted again — GameLift
rejects it outright. This upgrades §4.1's "do not `RemovePlayerSession` on a temporary disconnect" from a
preference to a **hard requirement**: calling it on a dropout permanently bars that player from their own
match, with their stake already escrowed. There is no undo.

**VISIBILITY IS NOT IMMEDIATE — 1.44 s, measured.** Player sessions were NOT queryable from the server SDK
when `onStartGameSession` fired; they appeared on the third poll. A first, non-polling run of this probe saw
zero and would have concluded the placement created none.

Consequence for §3.2: a `PreLogin` that treats "session not found" as "reject" has a real, if narrow, race
against a client connecting inside that window. Observed client boot-to-join is ~20 s, so the race is not
reachable in practice today — but it is reachable in principle, and a faster client or a slower region closes
the gap. **Do not treat not-found as forged.** Distinguish "unknown id" (reject) from "not visible yet"
(retry briefly, then reject).

**SEPARATE FINDING — sessions currently expire, always.** The first probe run's two sessions were later read
via AWS CLI as `TIMEDOUT`. Nothing in the project has ever called `AcceptPlayerSession`, so every player
session GameLift has minted for every match has gone `RESERVED -> TIMEDOUT`. GameLift's view is that no
player has ever joined any of our game sessions. Nothing reads that state today, so nothing breaks — but
reconnect, capacity, spectate, and player metrics would all be built on it. E is therefore a correctness fix
as much as a security one.

### 4.3 ⚠ RULING NEEDED — what a dropout does to an escrowed stake

Engineering cannot answer this. A staked match where one player drops and does not return:

1. **Forfeit** — the disconnector loses, the opponent is paid the pot. Punishes rage-quitting; punishes a
   power cut identically.
2. **Cancelled-refund** — both stakes returned, no rake, no rating change. Already implemented backend-side
   and already used for abandonment. Safe, but makes quitting a free option when losing.
3. **Split by state** — forfeit if the match was decided enough to be meaningful, refund otherwise. Fairest,
   most complex, needs a definition of "decided enough".

This overlaps the in-flight abandonment work (`task_3083127f`), which already added
`EAFLMatchCancelReason { Abandoned, ReplayCap }`. A single-player dropout is a **third** reason and should
join that enum rather than grow a parallel mechanism. **Coordinate before implementing.**

## 5. What must not break

Regression surface, all currently working and all reachable by this change:

- **PIE / listen server / offline** — no GameLift, no session, no validation. Must remain zero-friction.
- **The `?MatchmakerData=` launch-option path** — the local/offline fallback, still how local runs work.
- **Bot fill** — bots have no player session and must not be validated against one.
- **The proven economy chain** — escrow/settle/rating must be untouched by this change.

## 6. Test plan

Cooked dedicated server over the real GameLift hop, as with the S12 acceptance test:

| # | case | expected |
|---|---|---|
| 1 | valid `?PlayerSessionId=` | connects; reconcile id derived from GameLift, not the URL |
| 2 | absent session id, GameLift active | **rejected** |
| 3 | absent session id, PIE / listen server | **connects normally** — the regression guard |
| 4 | forged / foreign session id | rejected |
| 5 | reused session id (two clients, one id) | second rejected |
| 6 | `?PlayFabId=` disagrees with the session's `playerId` | session wins; mismatch logged loudly |
| 7 | disconnect + reconnect inside the window | same seat, no second escrow |
| 8 | disconnect beyond the window | resolves per the §4.3 ruling |
| 9 | join attempt after match start | rejected (`DENY_ALL`) |

Case 3 is the one most likely to be skipped and most likely to break the daily workflow.
Case 6 is what proves identity actually moved off the client.

## 7. Sequencing

1. Research 4.2 against live GameLift — the reconnect answer shapes everything after it
2. Get the 4.3 ruling — coordinate with `task_3083127f`
3. Implement 3.1 + 3.2 (validated identity + conditional gate) — this alone closes the security hole
4. Implement 3.3 (`DENY_ALL` at match start)
5. Implement §4 reconnect protocol
6. Run §6

Steps 1–3 are the security fix and are independently shippable. Steps 4–5 are hardening and can follow.

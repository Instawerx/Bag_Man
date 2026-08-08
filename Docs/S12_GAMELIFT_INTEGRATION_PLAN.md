# S12 — GameLift Server SDK integration (`onStartGameSession`)

**Status:** DRAFT, 2026-08-08. Not started.
**Scope ruling (operator, 2026-08-08):** S12 owns the GameLift *delivery hop*. It is **not** part of #20.
#20 (escrow → settle → rating) closes on a real allocator payload delivered via `?MatchmakerData=`, which is
the documented current source. S12 changes *how the payload arrives*, nothing downstream of it.

---

## 1. What S12 actually is

One sentence: **make a running dedicated server receive its match payload from GameLift instead of from its
launch line.**

The payload itself is already defined, already produced, and already consumed. What is missing is the
server-side SDK that receives it.

---

## 2. Current state — verified, not assumed

### Already exists

| Piece | Where | State |
|---|---|---|
| Payload contract | `lambda/match-allocator/index.ts:332` | `{matchId, members[], economics}` |
| Economics verification | `resolveEconomics()`, same file | Every member must agree; R86 ProMod-only enforced **before** allocation |
| Placement call | `StartGameSessionPlacementCommand` | Queue-based, `GameSessionData` = the JSON above |
| Server-side consumer | `UAFLMatchmakerDataProvider` | Parses the roster, reconciles vs connected controllers |
| **The swap point** | `AFLMatchmakerDataProvider.cpp:106` | `ResolveGameSessionData()` — 15 lines, one branch |
| **The injection surface** | `AFLMatchmakerDataProvider.h:72` | `SetGameSessionData()` already exists and already wins over the launch option |

The swap really is one point. `ResolveGameSessionData` returns `InjectedGameSessionData` if non-empty, else
parses `?MatchmakerData=`. S12 adds a caller of `SetGameSessionData` and touches nothing else.

### Does NOT exist

1. **No GameLift Server SDK in the project.** No `GameLift*.uplugin` anywhere in the tree. The backend is
   deliberately firewalled to the control plane — `match-allocator/index.ts:9`:
   *"only the GameLift AWS SDK (control plane) is used here. NO GameLift Server SDK"*.
   S12 is exactly the other half of that firewall.
2. **No fleet, no compute, no queue resource in IaC.** `cdk/lib/tentpole-stack.ts` defines
   `QUEUE_NAME = 'BagManTentpoleQueue'` as a **string constant only** — there is no CDK construct creating
   the queue, a fleet, or a location. The allocator places into a queue that its own stack does not provision.
3. **No registered compute.** The allocator's own comment on the FULFILLED branch reads
   *"Won't occur pre-S12 without registered compute"* — placements cannot fulfil today.
4. **No dedicated server binary.** Blocked on the D: engine work (tasks #7–#9); the C: launcher engine ships
   no `UnrealServer` target receipt and cannot build one.

---

## 3. Work breakdown

### A. Server SDK into the project
Add the GameLift Server SDK for Unreal, built against the **same pinned engine** as everything else
(post-#7 that is the D: 5.6.1 source build). It links into the **server target only** — it must not appear in
a client or editor build, mirroring how the earn HMAC key is gated to server/editor in
`AFLOnlineSubsystem::Initialize`.

⚠ Verify SDK/engine compatibility before committing to a version. Amazon's UE plugin tracks engine releases
on its own cadence; a 5.6-compatible release is an assumption until checked.

### B. Lifecycle adapter (the only real new code)
A server-only module implementing the GameLift process contract:

- `InitSDK` → `ProcessReady(port, logPaths, callbacks)`
- `onStartGameSession(session)` → **`GetGameSessionData()` → `SetGameSessionData(json)`** → `ActivateGameSession()`
- `onProcessTerminate` → drain, then graceful shutdown
- `onHealthCheck` → liveness
- `AcceptPlayerSession` on join

**Ordering hazard, and the main correctness risk in S12:** `onStartGameSession` must land *before* the first
player is assigned a team. Today the payload is present at map load because it is on the command line;
under GameLift it arrives asynchronously. Anything reading `ResolveGameSessionData` during
`InitGame`/`InitNewPlayer` could observe an empty string and silently fall back to unassigned teams — the same
class of bug as the Lyra experience-timing trap already recorded in memory (component `BeginPlay` running
before the experience is loaded). Gate match start on the payload having arrived; do not assume it is there.

### C. The swap
Delete nothing. `SetGameSessionData` already outranks the launch option, so the `?MatchmakerData=` path stays
as the local/offline fallback and keeps working for local runs and unit tests. This is the intended design —
`AFLMatchmakerDataProvider.h:30-33` names it as a "one-point S12 swap".

### D. AWS: fleet, queue, compute
Two viable topologies:

- **GameLift Anywhere** — register our own machine as compute. Cheapest, fastest to first green, and it
  matches how two players are already driven from one box. Right choice for the first S12 run.
- **Managed EC2 fleet** — the real production topology; needs a packaged server upload and build/fleet
  lifecycle.

Either way the **queue must become a CDK resource**, not a bare string constant. A queue that exists only as
a name in a Lambda is a production hazard independent of S12: it cannot be recreated from IaC.

### E. Player sessions
The allocator already passes `DesiredPlayerSessions` with `PlayerId = m.Entity?.Id` — the **PlayFab entity
id**. That is the same value the client carries in `?PlayFabId=` and that `InitNewPlayer` stashes as the
`ReconcileId`. So GameLift's player identity and our reconcile key already agree by construction; no new
identity concept is required. Worth an explicit assertion in the adapter, since a silent divergence here
would mis-escrow players.

### F. Acceptance test
A server receives the **identical payload** from GameLift that `?MatchmakerData=` delivers today, and every
downstream behaviour is unchanged: same roster reconcile, same escrow, same settle, same rating.

The cleanest form is a differential test — run #20's captured payload through both paths and diff the
resulting escrow/settle bodies. If they differ, S12 changed behaviour it was not supposed to touch.

---

## 4. Open decisions

1. **Anywhere first, or straight to managed EC2?** Recommend Anywhere — it proves the SDK contract without a
   packaging/upload loop.
2. **Does the queue become CDK-managed as part of S12, or as separate IaC hygiene?** Recommend inside S12;
   the fleet has to be wired anyway.
3. **SDK version pin** — must be resolved against the pinned 5.6.1 engine before any code lands.

## 5. Sequencing

S12 is blocked on tasks #7–#9 (engine → project → server target). Nothing here can be tested without a
dedicated server binary. Do **not** start S12 code before #20 is green: #20 establishes that the economy
chain works with a known-good payload, so if S12 then breaks something, the payload delivery is the only
variable that changed.

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

### A. Server SDK into the project — ✅ AVAILABILITY CONFIRMED 2026-08-09

**UE 5.6 IS supported.** Amazon's Unreal package is built for UE 5.0 through 5.8, and ships **server SDK 5.x**.
Source: <https://github.com/amazon-gamelift/amazon-gamelift-plugin-unreal>
(docs: <https://docs.aws.amazon.com/gameliftservers/latest/developerguide/gamelift-supported.html>)

**Take the SERVER SDK, not the full plugin.** The same repo offers both, and AWS states plainly: *"If you
don't need the guided workflows, you can get just the server SDK for your game engine from the same GitHub
repositories."* The full plugin adds UI workflows and sample assets whose job is to **configure and deploy
fleets** — we already have a fleet, a queue, an allocator and IaC for all three, so those workflows would
duplicate (or fight) what exists. Smaller surface, no editor UI, nothing that can drift from `tentpole-stack.ts`.

Build it against the **same pinned engine** as everything else (D: 5.6.1 source). Link it into the
**server target only** — it must not appear in a client or editor build, mirroring how the earn HMAC key is
gated to server/editor in `AFLOnlineSubsystem::Initialize` so the key cannot exist in a shipped client.

⚠ Nothing is downloaded yet — no `GameLift*.uplugin` exists anywhere on this machine. Only the AWS
**control-plane** SDK is present, in `Bag_Man_Backend/node_modules/@aws-sdk/client-gamelift`, which is the
backend half and deliberately firewalled from the server half.

⚠ Note for later: the separate **Client** SDK for Unreal (player gateway, UDP ping beacons) requires a
**source-built** engine 5.1+ and GitHub Epic-org membership. We satisfy the source-build requirement as of
#7. Not needed for S12, but relevant if player gateway is ever adopted.

## 🔴 ACCEPTANCE TEST RUN 2026-08-09 — HOP WORKS, ONE BUG REMAINS

**The GameLift hop itself is PROVEN.** Compute registered, placement `FULFILLED` on
`192.168.1.159:7777` (previously `PENDING` forever), and the server received the payload:

```
AFL_GAMELIFT: ProcessReady OK on port 7777 -- awaiting onStartGameSession.
LogGameLiftServerSDK: OnStartGameSession Received ... "GameSessionData":"{...economics...}"
AFL_GAMELIFT: onStartGameSession -- 263 bytes of GameSessionData received; activating.
```

`?MatchmakerData=` was deliberately OMITTED from the command line, so the payload had exactly one
possible origin. Note GameLift's own `MatchmakerData` field is `null` — that is the GameLift-matchmaking
channel, which we do not use; our allocator packs everything into `GameSessionData`. Confusingly similar
name, unrelated mechanism.

### THE SWAP WAS THREE POINTS, NOT ONE — this doc was wrong

Fixing only `ResolveGameSessionData` produced a run where the roster arrived correctly and the match still
came out LocalFill and unstaked. Three places read the payload:

| Reader | Drives | Status |
|---|---|---|
| `ResolveGameSessionData` | roster data | ✅ fixed |
| `FAFLMatchReporter::ReadEconomics` | tier / stake | ✅ fixed — now logs `economics from MATCHMAKER` |
| `UAFLTeamCreationComponent::GetProvider` | **which provider is used** | 🔴 **still broken — see below** |

All three now call one static resolver, `UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData`.
Codebase swept: the only remaining `ParseOption(...MatchmakerData)` is that resolver's own fallback.

### ✅ PROVIDER TIMING FIXED — 2026-08-09. ✅ BOT-FILL ROSTER OWNERSHIP FIXED — awaiting re-run.

The deferral works. `GetProvider()` now refuses to CACHE while the SDK is ready and no payload has arrived,
serving a provisional LocalFill instead and re-selecting on the next call:

```
11:07:37  AFL_TEAMPROVIDER: DEFERRED -- GameLift SDK ready but no payload yet. NOT caching.
11:08:58  AFL_TEAMPROVIDER: selected AFLMatchmakerDataProvider (authoritative=1) -- PRESENT
11:09:05  per-join -- player -> team 1 · player -> team 2      <-- correct split, from the GameLift roster
11:12:43  AFL_MATCHREPORT: economics from MATCHMAKER -- VoltsPlay/ProMod/10 VO
```

Three of four acceptance checks now pass: provider, teams, economics. The single-assigner rule is intact —
`Provider` is still written exactly once, just no longer prematurely.

#### ✅ THE NEW BUG, FOUND AND FIXED: bot-fill invents members of a roster it does not own

⚠️ **The first write-up of this bug in this document was wrong about the mechanism.** It is preserved as a
correction because the wrong version suggested a fix that would have been a silent no-op. Reading the log by
timestamp alone produced a plausible story; reading it against the code produced a different one.

The full ordering, all inside a single ~7 s hitch frame (`[452]`):

```
11.07.37  one-shot fill -- Humans=0 -> 0 bot(s)      <-- correct, but only because ?NumBots=0 was passed BY HAND
11.08.31  onStartGameSession -- 263 bytes received   <-- roster arrives, 54 s after experience load
11.09.05  converge fires on human 1's join           <-- Provider still NULL. Desired = 4 - 1 = 3
11.08.58  bot 'Hubert' joins  ... and HIS join is what first calls GetProvider()
11.09.05  bots 'Eliza','Tinplate' join; converge logs "3 bot(s)"
11.09.05  human 2 joins -> converge trims to 2
11.12.43  escrow REFUSES: 2 bot(s) in a staked match (R85). Nobody debited.
```

What the original write-up got wrong, and why it matters:

- **Claimed:** converge runs "the moment the provider turns authoritative." **Actually:** converge ran *before
  any provider existed at all*, and the first bot it spawned is what caused the provider to be selected. The
  provider turning authoritative was a *consequence* of the bug, not its trigger.
- **Therefore the obvious fix — gate converge on `IsAssignmentAuthoritative()` — would have done nothing.**
  That method reports `Provider && Provider->IsAuthoritative()` and deliberately does not construct a provider,
  so it returns `false` for the entire window converge actually runs in.
- **Claimed:** the bots were a symptom of the timing gap. **Actually:** converge would have spawned them
  regardless of when the payload landed.

**Root cause.** Bot fill answers "how many seats are empty?" by counting who is *present*. On a roster owned by
an external authority, an absent player is a rostered player who is still connecting — not an empty seat.

**Fix.** One predicate, `UAFLMatchmakerDataProvider::IsRosterExternallyOwned()`, true both when the payload has
**arrived** and while it is still **in flight**, applied to *both* fill paths:

| path | when it runs | what it would have done in production |
|---|---|---|
| one-shot fill | experience load, `Humans=0` | `Target - 0` = **a full field of bots** |
| converge | each human join | top back up to `Target - CountHumans()` |

The one-shot path is the more dangerous of the two and the acceptance run **did not expose it** — it was
masked by the hand-passed `?NumBots=0`, which a real placement never carries. Gating only converge would have
left production strictly *worse*: a full field of bots spawned at load, with converge no longer trimming them.

Two further corrections landed with the fix:

- The gate sits **after** the `?NumBots=` override so it also overrides it. Bots are barred outright from a
  staked or rated roster (R74/R85); this is not a count to be tuned.
- A comment at `HandlePlayerJoined` asserted team-creation's handler "bound HighPriority, before ours" had
  already assigned the joining human's team. **There is no priority on `OnGameModePlayerInitialized`.** Both
  bind with a plain `AddUObject`, and UE's native multicast `Broadcast()` walks its invocation list in *reverse*
  registration order — so team-creation binding first means it fires **last**. Its conclusion held by luck
  (`CountHumans()` counts `PlayerArray` membership, not team assignment); its stated reason was inverted.

**Known scope limit, deliberately not solved here:** the gate suppresses bots on *any* externally-owned roster,
which is broader than R74's "staked or rated". This is not a regression — `UAFLMatchmakerDataProvider` already
assigns every bot team 255, so bots in a matchmaker match are unusable today either way. But a casual Watts
Play match that legitimately wants bot backfill will need the provider to learn to seat bots *before* this gate
can be narrowed by tier.

#### 🟢 THE SAFETY PROPERTY HELD THROUGH ALL THREE FAILURES

Three distinct bugs — wrong provider, wrong economics, and now bots — and **no currency moved in any of
them**. The ledger still holds only the #20 rows. Escrow refused every unverifiable roster rather than
guessing at one, which is exactly the bias the escrow-before-payout design was built for.

### 🔴 (HISTORICAL) provider selection cached BEFORE the payload arrives

```
10:46:50.921  ProcessReady OK -- awaiting onStartGameSession
10:46:51.586  AFL_TEAMPROVIDER: selected AFLLocalFillProvider (authoritative=0) -- ?MatchmakerData= ABSENT
10:47:46.720  OnStartGameSession Received                       <-- 55 SECONDS LATER
```

`GetProvider()` runs at experience load, selects once and caches "as a provider that could change mid-match
would mean two different authorities over the same roster". That caching is correct in itself — the problem
is that under GameLift the decision is made **before the input to the decision exists**.

Consequences observed: both players assigned to **team 2**, bots spawned despite `?NumBots=0`, and escrow
correctly refused with *"2 bot(s) in STAKED match -- bots are barred from staked play (R85)"*. **No currency
moved** — the ledger still shows only the #20 rows.

**This is the exact hazard §4B of this plan predicted** ("anything reading the roster during InitGame or
InitNewPlayer could observe an empty string and silently fall back"). The subsystem was designed to survive
it and the resolver is correct; the defence was simply never applied to the *selection*, only to the *read*.
`ReadEconomics` escaped it purely because it runs at match start, long after arrival.

**The fix is to gate on arrival, not to re-order.** Under GameLift the server must not select a provider — or
start a match — until `HasGameSessionData()` is true or the SDK is known absent. Options, cheapest first:

1. Defer selection: `GetProvider()` refuses to cache a LocalFill choice while the SDK is ready and no payload
   has arrived. Needs every caller to tolerate a deferred answer.
2. Gate match start on `HasGameSessionData()` when `IsSdkReady()` — closest to what §4B originally proposed.
3. Have the subsystem, on payload arrival, explicitly invalidate a provider chosen without one. Racy; least
   preferred, since it reintroduces the mid-match authority change the caching exists to prevent.

---

### ✅ B IS BUILT — 2026-08-09, commit `608caf5c`

`UAFLGameLiftHostSubsystem` (`AFLGameCore/…/Online/`) is the SOURCE half; the CONSUMER half is one added
branch in `ResolveGameSessionData`. Order is now **test setter → GameLift → `?MatchmakerData=`**.

**Verified, not assumed:**

| Check | Result |
|---|---|
| `LyraEditor` builds | ✅ adapter compiles **out** (`WITH_GAMELIFT=0`, no SDK dep) |
| `LyraServer` builds | ✅ adapter compiles **in** and links |
| Degrades with no credentials | ✅ `AFL_GAMELIFT: no Anywhere credentials … SDK NOT started` |
| No regression to #20's path | ✅ `economics from MATCHMAKER -- VoltsPlay/ProMod/10 VO` |
| Test moved no money | ✅ ledger still 2 escrow / 1 settlement — only the #20 match |

The no-money property was designed into the test: launching with **no clients** means escrow correctly refuses
(`0 team(s) with players -- need 2`), so the changed code path runs without touching real balances.

⚠ **The uncooked server cannot test this.** It crashes in `PackageFileSummary` before a `GameInstance` exists,
so the subsystem never initialises and the log shows nothing. That is not an adapter fault — use the cooked
server. A server re-cook is ~6 min.

**Two API facts found by compiling, not by reading the headers.** `GameSession.h` and `UpdateGameSession.h`
each carry two variants gated on `GAMELIFT_USE_STD`. **This build compiles the `const char*` variant**, so
`.c_str()` on the accessors is a compile error (C2228) and every return needs a null guard. Because the
char\* variant is live, `UpdateGameSession::GetGameSession()` **is** available and backfill can refresh the
roster — an earlier revision claimed the opposite after misreading which branch was active. If
`GAMELIFT_USE_STD` is ever defined this stops compiling, which is the good failure.

### B. Lifecycle adapter — original design notes
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

✅ **The queue is now a CDK resource** (done 2026-08-08, `Bag_Man_Backend` commits `699758c` / `fb7e904`).
It is `AWS::GameLift::GameSessionQueue MatchSessionQueue`, `DeletionPolicy: Retain`, destination pinned to the
Anywhere fleet. Recreated identical to the hand-made original including its ARN.

How, and why it matters for S12: `cdk import` **could not** adopt it. CloudFormation rejected every attempt
with *"you cannot modify or add [Outputs]"*, even once `cdk diff` proved the delta was resource-only. Root
cause of the phantom Outputs delta was a mangled section sign in an output description (deployed `?10.1` vs
source `§10.1`) — since fixed to ASCII, because non-ASCII in a CFN description poisons diffs forever after a
single bad deploy. Import still refused after that, so the queue was deleted and recreated by `cdk deploy`.

That was safe **only because the fleet has zero registered compute**, so no placement could fulfil and the
queue was inert. **That window closes the moment S12 registers compute.** After S12, deleting the queue
means dropping live matches — so any future queue change must be an in-place update, and the fleet should be
brought into IaC *before* compute is registered, not after.

✅ **The fleet and its custom location are now IaC too** (same day, commit below). `AnywhereFleet`
(`fleet-40c6b342…`, unchanged id) and `AnywhereLocation` (`custom-bagman-test-1`) were adopted by
`cdk import` — which **worked** for these, unlike the queue. The difference was not the resource type: the
queue's import was blocked purely by the phantom Outputs drift, and once the deploy that fixed the mangled
section sign had run, the template baseline was clean and import succeeded first try. Lesson for future
adoptions: **fix template drift first, then import** — do not conclude a resource type is un-importable.

Import was the right mechanism here specifically because a recreated fleet gets a **new FleetId**, which
would have invalidated the queue destination and any registered compute. The queue could tolerate
delete-and-recreate because GameLift queue ARNs are name-derived and therefore stable; fleet ids are not.

**The whole GameLift substrate is now under management: location → fleet → queue.** What S12 adds on the AWS
side reduces to registering compute against an already-managed, already-ACTIVE Anywhere fleet.

⚠ Still hand-made and out of IaC once S12 starts: **the registered compute itself**. Decide up front whether
compute registration is IaC-managed or an operator step — `RegisterCompute` for Anywhere is a runtime action
(it takes the host's IP/hostname), so it likely belongs in a documented runbook rather than CloudFormation.

✅ **Fleet renamed** `BagManTentpoleTest` → **`BagManTentpoleFleet`**, description now
*"AFL match-hosting Anywhere fleet. Destination of BagManTentpoleQueue; compute registered at S12."*

Done as an **in-place update, FleetId preserved**. Verified before deploying by creating a changeset and
reading `Replacement: False` off it, rather than assuming — GameLift exposes `UpdateFleetAttributes` so a
rename does not require replacement, but that is a property-by-property fact worth confirming, since a
replacement here would have minted a new FleetId and silently orphaned the queue destination.

**Technique worth reusing:** for any change to these resources, create a changeset and read `Replacement`
before executing. `cdk diff` cannot tell you this — it falls back to a template-only diff and prints
*"Could not create a change set… will base the diff on template differences"*.

⚠ **The location is still `custom-bagman-test-1`, and that is an AWS QUOTA, not an oversight.** The rename was
attempted 2026-08-08 and failed at deploy:

```
Request for 2 remote locations exceeds the limit of 1 available   (ServiceLimitExceeded)
```

`LocationName` is the physical id, so a rename is a replacement, and CloudFormation adds the new location
**before** removing the old — transiently needing two remote locations against a per-fleet limit of one. The
stack rolled back cleanly (fleet id, name and queue destination all intact); the orphaned `custom-bagman-1`
left behind was deleted by hand.

Notably the **fleet was not going to be replaced** — the changeset showed `AnywhereFleet Replacement: False`,
so GameLift does swap fleet locations in place. Only the quota blocked it.

### 🚩 WHAT THE LIMIT ACTUALLY IS — CORRECTED

An earlier revision of this doc claimed "one remote location per fleet". **That was wrong.** Checked against
Service Quotas:

| Quota | AWS default | Adjustable | This account |
|---|---|---|---|
| Locations in a fleet per region (`L-55650DB7`) | **10** | yes | no override — uses default |
| Custom locations per region | 20 | yes | — |
| Anywhere fleets per region | 30 | yes | — |
| Compute per Anywhere fleet | 100 | yes | — |
| Queue destinations per game session queue | 10 | yes | — |

No account override is recorded and no increase request is pending. So the deploy failure was **not** an
account quota setting. The message *"limit of 1 available"* is something narrower — most likely specific to
how Anywhere fleets handle remote locations — and its exact rule is **not established**. Do not plan around
"1" as if it were the documented limit, and do not plan around "10" either until it is confirmed for
ANYWHERE compute specifically.

### ✅ SETTLED 2026-08-09 BY DIRECT TEST — THE LIMIT IS REAL AND IT IS 1

Created a second custom location and attached it to the fleet directly, outside CloudFormation:

```
aws gamelift create-location --location-name custom-bagman-2                       -> OK
aws gamelift create-fleet-locations --fleet-id fleet-40c6b342... \
    --locations Location=custom-bagman-2
-> LimitExceededException: Request for 2 remote locations exceeds the limit of 1 available
```

So it is a **genuine GameLift constraint, not a CloudFormation ordering artifact** — the earlier rename
failure reproduces through the raw API. Test artifacts cleaned up; the fleet still has exactly
`custom-bagman-test-1` and nothing else changed.

**It also contradicts Service Quotas.** `Locations in a fleet per region` (`L-55650DB7`) reports default
**10**, adjustable, with no account override — yet the API enforces **1**. Do not trust the quota console for
this; it does not describe ANYWHERE-fleet behaviour. Cite this exact API error, not the quota page, in any
increase request.

### 🚩 CONSEQUENCE FOR S12 TOPOLOGY — DECIDE BEFORE REGISTERING COMPUTE

**One custom location per Anywhere fleet.** Multi-region therefore means **a fleet per region**, not one
fleet spanning locations, unless AWS raises this specific limit. Concretely:

- Each region needs its own `AWS::GameLift::Fleet` + `AWS::GameLift::Location` pair in `tentpole-stack.ts`
- The queue can hold up to 10 destinations (`Queue destinations per game session queue`), so one queue
  fanning out to per-region fleets is the shape that works today
- `Anywhere fleets per region` is 30 and `Compute per Anywhere fleet` is 100, so neither of those binds first

⚠ And the window to restructure is **now**, while the fleet is inert. Once compute is registered, changing
fleet or location membership means dropping live matches.

To finish the rename later, do one of:
- raise the "remote locations per fleet" quota above 1, then redeploy the rename; or
- replace the fleet outright while it is still inert. This is **now safe** because the queue destination was
  rewired from a hardcoded ARN to `Fn::GetAtt(AnywhereFleet, FleetArn)` — a new FleetId propagates to the
  queue automatically instead of leaving it pointing at a destroyed fleet.

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

# RUNBOOK — Register an Anywhere compute for Bag_Man

**Why this is a runbook and not CloudFormation.** `RegisterCompute` takes the **host's IP address**. That is a
property of the machine doing the hosting at the moment it starts hosting, not of the infrastructure — so it
cannot be known when a template is written, and a stack that hardcoded one would be wrong the next time the
machine moved networks. The fleet, its location and the queue **are** IaC (`tentpole-stack.ts`); registration
is the runtime edge that hangs off them.

**Current registration** (dev, 2026-08-09)

| | |
|---|---|
| Fleet | `BagManTentpoleFleet` / `fleet-40c6b342-c03b-44cd-a9d3-56fe65156a7a` |
| Location | `custom-bagman-test-1` |
| Compute | `bagman-dev-EREMOSPECIALTY` @ `192.168.1.159` — **Active** |
| SDK endpoint | `wss://us-east-1.api.amazongamelift.com` |

---

## 1. Register the machine (once per host, or after its IP changes)

```bash
aws gamelift register-compute --compute-name bagman-dev-<HOSTNAME> --fleet-id <FLEET_ID> --ip-address <HOST_IP> --location custom-bagman-test-1
```

Keep `GameLiftServiceSdkEndpoint` from the response — it is what `InitSDK` connects to.

⚠ **The IP must be reachable by the clients that will join.** A LAN address is fine for a same-network dev
test; it is not fine for anything else. Re-register when the machine changes network — `register-compute`
with the same name updates the existing record rather than erroring.

## 2. Launch (token is minted for you)

```bash
powershell -File C:\Dev\Bag_Man\Tools\Launch-Editor-Economy.ps1 -Server -GameLift
```

That resolves the compute, mints a **fresh auth token**, exports the five `AFL_GAMELIFT_*` vars, and starts
the cooked server. Add `-DryRun` to see the config without launching.

## 🚩 3. NEVER STORE THE AUTH TOKEN

`GetComputeAuthToken` returns a token **valid for ~180 minutes** (measured, not assumed). It is a credential
and it expires.

Do **not** paste it into this runbook, an `.env`, a shortcut, or a scheduled task. A stored token works right
up until it silently doesn't, and the failure surfaces far from the cause — as `InitSDK` refusing to connect,
which looks like a broken SDK integration rather than an expired secret. The launcher mints one per launch;
that is the only supported path. Same discipline as the earn HMAC key: fetched at launch, held in the
process, never persisted, reported only as `held`/`MISSING`.

## 4. Verify

```bash
aws gamelift list-compute --fleet-id <FLEET_ID> --query "ComputeList[].{Name:ComputeName,Status:ComputeStatus,Ip:IpAddress}"
```

In the server log, expect **`AFL_GAMELIFT: ProcessReady OK on port <n> -- awaiting onStartGameSession`**.

If you instead see `AFL_GAMELIFT: no Anywhere credentials in the environment … SDK NOT started`, the launcher
ran **without** `-GameLift`. That is not a failure — it is the local mode, where `?MatchmakerData=` supplies
the roster. It is how #20 was proven and it still works.

## 5. Deregister

```bash
aws gamelift deregister-compute --fleet-id <FLEET_ID> --compute-name bagman-dev-<HOSTNAME>
```

Do this when a machine stops being a host. A registered compute that is not running a server still counts
against the fleet and can receive placements that will never fulfil.

---

## What changes once compute is registered

**Placements can now fulfil.** Before registration the fleet was inert: `StartGameSessionPlacement` accepted
and sat in `PENDING` forever, which is what every #20-era placement did.

⚠ **The cheap-restructure window is closing.** Fleet and location membership could be changed freely while
nothing could be hosted. Once real matches run on this fleet, changing them means dropping live matches. If
per-region fleets are wanted — and the Anywhere limit of **one location per fleet** means multi-region
requires exactly that — do it before players arrive. See `S12_GAMELIFT_INTEGRATION_PLAN.md`.

**Two sources of roster now exist**, and only one is authoritative per launch:

| Launch | Roster source |
|---|---|
| `-Server` | `?MatchmakerData=` launch option (local; what #20 used) |
| `-Server -GameLift` | GameLift `onStartGameSession` (production path) |

`UAFLMatchmakerDataProvider::ResolveGameSessionData` prefers GameLift **only once the payload has actually
arrived**, and falls through to the launch option otherwise. An empty GameLift payload never means
"no roster" — that distinction is what stops an async delivery from silently producing unassigned teams.

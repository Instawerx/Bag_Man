# HANDOFF — 2026-08-08

**Read this first in a new thread.** Everything below is committed and pushed to `personal/main`.
Last commit at handoff: `d20ca894`.

---

## 0. ⚙ MACHINE SETUP CHANGED 2026-08-09 — READ BEFORE BUILDING

**The project now builds on the D: SOURCE engine, not the C: launcher engine.**
`Bag_Man.uproject` → `EngineAssociation: {5066982E-439C-2993-A6CB-F48A14DE2492}` = `D:\UE5.6-source`
(source builds are identified by a registry GUID under `HKCU\Software\Epic Games\Unreal Engine\Builds`,
not by a version string).

**Why, proven not assumed.** Asked to build `LyraServer`, the C: launcher engine answers:
> `Server targets are not currently supported from this engine distribution.`

and the same run reports `LyraGameEOS and dynamic target options are disabled when packaging from an
installed version of the engine`. Both capabilities are required — #20 must be verified on a dedicated
server, and the shipping login is EOS. An Installed Build ships precompiled Editor and Game only.

Both engines are now 5.6.1 with **matching `CompatibleChangelist 43139311`**; the D: engine carries local
patches saved at `D:\ue-local-patches-5.6.1.diff` (MSVC `INFINITY` → `std::numeric_limits<float>::infinity()`,
because MSVC defines `INFINITY` as `((float)(1e+300 * 1e+300))` → C4756 under warnings-as-errors, plus an
`initguid.h` ordering fix in NNERuntimeORT). **Re-apply after any engine bump** — without them the engine
build fails at ~99% after hours.

### 🔗 THREE JUNCTIONS — MACHINE-LOCAL, INVISIBLE TO GIT

C: had only 5 GB free after a cook. These redirect the bulk to D: and are **not** in source control, so a
fresh clone on another machine will NOT have them and will fill its system drive.

| Path on C: | → Target on D: |
|---|---|
| `Saved\Cooked` | `D:\BagMan\Cooked` |
| `Saved\StagedBuilds` | `D:\BagMan\StagedBuilds` |
| `Intermediate` | `D:\BagMan\Intermediate` (54 GB) |

Recreate with `mklink /J <link> <target>`. Junctions are transparent to UBT — verified: after moving 54 GB of
`Intermediate`, an incremental build reported *"Target is up to date"* in 11s with nothing rebuilt.

⚠ `Intermediate` is large because **each target configuration keeps its own** — Editor, Server and Client are
three separate sets. Expect it to grow again as more targets are built.

**Built artifacts:** `Binaries\Win64\LyraServer.exe` (356 MB) · cooked client archived at
`D:\BagMan\Archive\Windows` (`LyraGame.exe` + 3 pak chunks, 6.6 GB).

---

## 1. THE FOUR LIVE FRONTS (tasks #23–#26, plus #20)

| # | Front | State | Blocked on |
|---|---|---|---|
| **#20** | **Phase 3 spine — escrow/settle/rating round-trip** | Code is COMPLETE end to end. Never verified live. | **4 env vars** — see §2 |
| #23 | `W_IRONICS_Home` layout | Scaffold built, structurally correct, **no layout** | UMG designer pass (human) |
| #26 | BR publication | All 3 original gates CLOSED | Operator combat sign-off |
| #24 | D2/D3 district fences | Data layers exist, D1 works | Content authoring |
| #25 | ShantyTown river | Course solved (889m, 63pts, max depth 336cm) | Bed sampling must be REAL, never interpolated |

---

## 2. ✅ #20 IS CLOSED — 2026-08-09. Everything below it is historical.

**The escrow → settle → rating chain ran end to end on a DEDICATED SERVER with allocator-verified
economics.** Match `6B290E6B-4CF5-4487-405A-A3AFF3C8232D`, team 1 wins 7-1.

```
AFL_ROUND:      MATCH END -- team 1 wins 7-1 -> concluding
AFL_MATCHREPORT: economics from MATCHMAKER -- tier=VoltsPlay league=ProMod stake=10 VO
AFL_MATCHREPORT: escrowed 10 from 40419031BCC83EA5   (BalanceAfter 90)
AFL_MATCHREPORT: escrowed 10 from DF7C3188377BB66D   (BalanceAfter 200090)
AFL_MATCHREPORT: settle OK -- pot 20, rakeAmount 1 (5%), payoutPool 19 -> DF7C3188377BB66D position 1
AFL_MATCHREPORT: rating OK -- DF7C..66D mu 27.64 (+3.44) · 40419..EA5 mu 22.36
```

| Table | Rows | Baseline |
|---|---|---|
| `bagman-match-escrow` | 2 (10 VO each) | was 0 |
| `bagman-match-settlement-ledger` | 1 — Pot 20, Rake 1, Paid 19, **`FailedJson: []`** | was 0 |
| `bagman-player-ratings` | 2 | unrated |
| `bagman-rating-ledger` | 1 | was 0 |

**Money conserves exactly: 10 + 10 = 20 = 1 rake + 19 payout.** All tables were empty at baseline, so every
row is attributable to this run.

Nothing was faked. Economics came from the real allocator after `resolveEconomics` verified both members
agreed; identities were real PlayFab accounts carried through `?PlayFabId=` → `InitNewPlayer` → `ReconcileId`;
and `IsRunningDedicatedServer()` — the production branch — is what ran it.

### HOW TO RE-RUN IT

1. `Tools\Invoke-Allocator.ps1 -PlayFabId <id1>,<id2> -Tier VoltsPlay -Stake 10`
   Drives the DEPLOYED allocator and captures its `GameSessionData` from the GameLift placement. Never
   hand-write this payload — the whole point is that the allocator produced and verified it.
2. Fund both accounts: `Tools\Fund-DevAccount.ps1 -PlayFabId <id1>,<id2> -Currency VO -Amount 100`
3. `Tools\Launch-Editor-Economy.ps1 -Server -NumBots 0 -WarmupSeconds 240`
4. Two cooked clients: `LyraGame.exe 127.0.0.1:7777?PlayFabId=<id> -log -windowed`
5. **Play it.** Someone must actually score.

### FIVE TRAPS THIS COST, IN ORDER OF HOW MUCH TIME THEY ATE

1. **The premise was wrong.** This doc said #20 was "blocked on 4 env vars" and was "a verification task, not
   a build one". Both false. It needed a source engine, a server binary, a server cook, a client cook, two
   distinct funded identities and a real allocator payload. The env vars were the smallest part.
2. **A PIE match cannot prove this.** The first attempt faked the stake with a dev cvar and would have gone
   green while proving only that the fake agreed with itself. Reverted deliberately — see the note in
   `AFLMatchReporter.cpp` explaining why a PIE match logging `LEAGUE PLAY ... nothing to escrow` is CORRECT.
3. **Warmup must outlast client boot.** Cooked clients take ~70 s. The default 30 s warmup expired 41 s
   before they connected, and escrow ran on an empty roster: `0 team(s) with players -- need 2`. Use
   `-WarmupSeconds 240`. (That failure is also reassuring — escrow REFUSES an unverifiable roster.)
4. **`Start-Process -ArgumentList` eats embedded quotes.** `?MatchmakerData=` arrived as `{matchId:...}`
   instead of `{"matchId":...}` — malformed, silently. Always verify against the server's own
   `LogInit: Command Line:` echo.
5. **A dedicated server cannot run uncooked content.** It crashes at boot after ~8 GB in
   `FGenerationInfo::Serialize` via `PackageFileSummary`. Cook the server (`-server -noclient`) — it takes
   ~4 minutes, versus 2h29m for the client, because a server needs no shaders or bulk data.

⚠ **Rounds 1–11 resolved as `Replay` at 0-0** before anyone was playing; the scoring rounds were 12–19. So
the score hits 7-1 on round **19**, not round 7. A dedicated server also persists after `MATCH END` — that is
correct, not a hang.

---

## 2b. ORIGINAL (2026-08-08 PM) — env diagnosis, kept for context

> **UPDATE — supersedes the "blocked on 4 env vars" section below, which is kept for context.**
>
> **Steps 1–3 of the verification sequence are DONE and GREEN.**
>
> - **The env problem is solved permanently by `Tools/Launch-Editor-Economy.ps1`.** It resolves the five
>   endpoints from the **live CloudFormation stack** and streams the HMAC key from Secrets Manager
>   (`bagman/earn/hmac`) straight into the editor's process environment. The key is never written to disk,
>   never logged, never echoed — the script reports only `held`/`MISSING`. Run `-DryRun` to check the
>   environment without booting. It refuses to launch if an editor is already running (that instance
>   captured the OLD env) or if any of the four gating values is missing.
> - **Do NOT trust `Bag_Man_Backend/cdk-outputs.json`** — it is only rewritten by
>   `cdk deploy --outputs-file` and was found STALE, missing every endpoint added after
>   `/award-achievement`, *including all three C endpoints*. It reads as "escrow/settle/rating were never
>   deployed", which is FALSE. The live stack is `UPDATE_COMPLETE` and all three exist. The launcher reads
>   CloudFormation directly so it cannot be fooled this way.
> - ✅ **Boot line green:** `key=held` + all five URLs resolved → `IsMatchReportingConfigured()` is true.
> - ✅ **`afl.Match.Result.Test` → `AFL_RESULTTEST: DONE -- 21 passed, 0 failed. ALL GREEN`.**
>
> ### ⚠ STEP 4 IS NOT "JUST PIE A MATCH" — THREE HARD PRECONDITIONS
>
> Discovered by reading the validator, **not** by burning a PIE run. Miss any one and the chain never
> POSTs, and it will look like a transport failure when it is a validation refusal.
>
> 1. **ZERO BOTS. Non-negotiable.** `AFLMatchResultTypes.cpp` §3: `if (BotCount > 0 && (bStaked || bRanked))`
>    → `Validate` fails and `ReportMatchEnd` logs `REFUSING to report match ... invalid result` before any
>    POST. A staked match with even one bot is unreportable by design (a bot must never move a balance or a
>    ladder). The current `ExperienceOverride=B_AFLExperience_2v2_ProMod` wants 4 players and will bot-fill
>    a solo PIE straight into this wall.
> 2. **The stake comes from the OPTIONS STRING, and `AdditionalServerGameOptions` is currently EMPTY** —
>    which is why no PIE has ever been staked. `FMatchEconomics::IsStaked()` is `Tier != LeaguePlay`, so the
>    **Tier** is what flips staking on; `?Stake=` only sets the amount. Set, in Editor Preferences →
>    Level Editor → Play → *Additional Server Game Options*:
>    ```
>    ?Tier=VoltsPlay&League=ProMod&Stake=10&StakeCurrency=VO
>    ```
>    R86 is enforced in code: `League=Haywire` + a staked tier is refused and silently downgraded to
>    unstaked LEAGUE PLAY. Staked is **Pro Mod only**. Currency is `VO` (Volts) or `WA` (Watts).
> 3. **Every human's PlayFab account must be FUNDED**, or `escrow-entry` 4xxs per player and the match is
>    correctly refused at settlement (`ESCROW FAILED ... settlement will refuse this match`). Fund the dev
>    account via `afl.Online.EarnCanary` (A1.3b) before the run — note this **mutates a real balance** on
>    title `1A2077`.
>
> ### ✅ TWO-PLAYER IDENTITY SOLVED (2026-08-08 PM) — and the signer is PROVEN LIVE
>
> **The blocker was never the engine — it was one cvar.** `ResolveDevCustomId()` returned a *process-global*
> cvar, and `UAFLOnlineSubsystem` is a **GameInstance** subsystem. PIE runs every client in one process, so
> all PIE clients logged in as the SAME PlayFabId. Two PlayerStates, one identity — which escrow
> (keyed `matchId, playFabId`) would collapse onto a single row, and settlement would then correctly refuse
> the match on its count check. It would have read as a transport bug.
>
> **Fix (shipped in this session):** `ResolveDevCustomId()` now suffixes by `FWorldContext::PIEInstance`
> under `#if WITH_EDITOR`. Instance 0 keeps the bare id, so the seeded primary account stays primary.
> Verified live in a 2-client PIE:
>
> | PIE instance | customId | PlayFabId |
> |---|---|---|
> | 0 | `AFL_DEV_TEST_01` | `DF7C3188377BB66D` (seeded primary) |
> | 1 | `AFL_DEV_TEST_01_P2` | `40419031BCC83EA5` (**new**, needs funding) |
>
> **The signer is proven against the live backend.** The A1.4 canary fired unprompted during that PIE:
> ```
> [AFLOnline] PostServerSigned -> http=200 ok=1
> LogAFLIdentity: AFL_A14 resolve ok pid=40419031BCC83EA5
> ```
> UE's OpenSSL HMAC-SHA256 signature verifies at the Lambda. Escrow/settle/rating use the *same* signer and
> the *same* key, so the remaining risk in this chain is wiring, not cryptography.
>
> **The reporter reaches its decision point correctly:**
> ```
> AFL_MATCHREPORT: LEAGUE PLAY match 62036D65-... -- no buy-in, nothing to escrow (R85).
> ```
> Unstaked only because `AdditionalServerGameOptions` is still empty. That one field is what stands between
> this line and a live escrow.
>
> ### STAKED RUN — STAGED AND RUNNING, ONE BLOCKER LEFT (`ReconcileId`)
>
> **The match lifecycle now completes UNATTENDED.** With `afl.Round.DevRoundsToWin=1` a single round win ends
> the series, and a round resolves on elimination without anyone playing:
> ```
> AFL_ROUND: round 1 RESOLVED -- winner team 2, reason Elimination. Score 0-1.
> AFL_ROUND: MATCH END -- team 2 wins 0-1 -> concluding.
> ```
> So the economy round-trip does NOT require a hand-played best-of-13. It requires the blocker below.
>
> **⛔ BLOCKER: PIE players have no `ReconcileId`, so nobody can be debited, paid or rated.**
> ```
> AFL_MATCHREPORT: player '...' has no reconcile id -- match ... NOT escrowed (nobody debited).
> AFL_ROUND: match ... NOT reported -- human player '...' has no reconcile id
> ```
> This is DOCUMENTED BEHAVIOUR, not a bug. `UAFLReconcileIdComponent` holds the PlayFab entity id "a
> connecting client carries in its `?PlayFabId=` connect option", set at `AAFLGameMode::InitNewPlayer`, and is
> explicitly *"Absent for LocalFill / offline / PIE joins (no `?PlayFabId=`)"*. `Validate` §4 independently
> requires every human to carry one. **This is the same PIE-has-no-PlayFabId gap called out in the cheat-sheet
> critique in §3** — it is now the live blocker, not a hypothetical.
>
> Two ways forward, needs a call:
> 1. **Dev cvar in `InitNewPlayer`** (cheap, matches the two cvars already proven): when the reconcile id is
>    empty and the world is PIE, assign from a configured id list in join order. Production `?PlayFabId=` path
>    untouched, `#if !UE_BUILD_SHIPPING`. Risk: a third dev affordance, and it sits on the identity boundary.
> 2. **Make PIE clients actually send `?PlayFabId=`** — truer to production, but the in-process PIE client
>    connect URL is not exposed by any editor setting (see the trap box below; both URL fields are
>    separate-process only).
>
> ⚠ **`bOverrideBotCount` set on the CDO does NOT survive an editor restart** — it reverted to `False` and a
> run spawned 2 bots alongside the 2 humans ("suppression APPLIED on 4 player ASC(s)"). Even with reconcile
> fixed, that fails the bot bar. It is now set to `True` in the **ini**, which IS read at startup. Same lesson
> as `RoundsToWin`: for these, prefer the ini or a cvar over a CDO edit.
>
> Configuration below is live in the editor.
>
> | Setting | Value | Why |
> |---|---|---|
> | `InEditorGameURLOptions` | `?Tier=VoltsPlay?League=ProMod?Stake=10?StakeCurrency=VO` | **This is the field that works.** `IsStaked()` keys off **Tier**, not amount. R86: Pro Mod only. |
> | `PlayNumberOfClients` | `2` | Two humans; MATCH PLAY needs exactly 2 finishing positions. |
> | `bOverrideBotCount` | `True` (count `0`) | Any bot in a staked/rated result fails `Validate` before a single POST. |
> | `ExperienceOverride` | `B_AFLExperience_2v2_ProMod` | **Keep it** — Pro Mod, two teams. Correct once bots are off. |
> | `afl.Round.DevRoundsToWin` | `1` | Dev-only cvar; one round win resolves the series. |
>
> ### ⚠ TWO TRAPS THAT COST A PIE RUN — BOTH VERIFIED AGAINST ENGINE SOURCE
>
> **1. `AdditionalServerGameOptions` DOES NOT WORK under `RunUnderOneProcess=True`.** The engine reads it in
> exactly one place — `PlayLevelNewProcess.cpp:94`, the *separate-process* path. The in-process PIE path never
> calls `GetAdditionalServerGameOptions` at all. Setting it looks right, persists to the ini, and is silently
> ignored. The field that actually reaches an in-process listen server is
> `UEditorEngine::InEditorGameURLOptions` (`PlayLevel.cpp:1432`, `URL += InEditorGameURLOptions`), a `config`
> UPROPERTY, so it lives in `EditorPerProjectUserSettings.ini` under `[/Script/UnrealEd.EditorEngine]` and is
> read at editor startup — **it needs an editor restart, not just a set.**
>
> **2. UE URL options are separated by `?`, NOT `&`.** Engine doc on the field: *"in the format
> `?bIsLanMatch=1?listen`"*. With `&`, `ParseOption(Options,"Tier")` returns
> `VoltsPlay&League=ProMod&Stake=10&StakeCurrency=VO`, which matches no tier and falls through to LEAGUE PLAY.
> The tell is exactly the line a wired-but-unstaked match prints:
> `AFL_MATCHREPORT: LEAGUE PLAY match ... -- no buy-in, nothing to escrow (R85)`.
>
> **Bonus trap: an edited CDO does not reach this GameFeature's components.** `RoundsToWin` was set to 1 on
> `/Script/AFLCombat.Default__AFLRoundManagerComponent` and read back as 1, yet the next match still ran
> `first to 7` / `half-swap after round 6`. The component is added by the `LAS_AFL_ExtractionMatch` action set
> (not by the experience's own inline Actions, which only add pawn movement components). Override the LIVE
> instance instead — which is what `afl.Round.DevRoundsToWin` does, at `ServerStartMatch`.
>
> **Both accounts funded** (Volts), via `Tools/Fund-DevAccount.ps1`:
> `DF7C3188377BB66D` → 200100 (already held 200k) · `40419031BCC83EA5` → 100.
>
> ⚠ **These play settings live in `Saved/Config/.../EditorPerProjectUserSettings.ini`, which is gitignored
> and which the editor REWRITES ON EXIT from its in-memory values.** They were set on the live CDOs, so they
> persist through a normal editor close — but they are not version-controlled and a config reset loses them.
> Re-read this table if a later PIE reports `LEAGUE PLAY ... nothing to escrow`.
>
> **Note the PascalCase quirk:** `LevelEditorPlaySettings` / `LyraDeveloperSettings` properties are reachable
> from Python ONLY by their PascalCase names (`AdditionalServerGameOptions`, `PlayNumberOfClients`,
> `bOverrideBotCount`). The snake_case forms all return "failed to find property" — the same trap already
> recorded for `configure_widget` in §4.
>
> Now play the 2-client match to a series result and watch for escrow → settle → rating, each 2xx.
>
> ### FUNDING: USE THE SCRIPT, NOT THE CANARY
>
> `afl.Online.EarnCanary` needs a live game world, so it only runs during PIE — and this project's standing
> rule is zero tooling calls into a running PIE. It also hardcodes `WA` and amount 5.
> `Tools/Fund-DevAccount.ps1` hits the same `/earn` Lambda directly, needs no editor, and grants either
> currency (`CURRENCY_CODES` is `{WA, VO}` — only the canary is WA-only). Same secret discipline as the
> launcher: the key is streamed from Secrets Manager, never written down, never logged.
>
> Its 200 response is also the **third** independent proof the UE-side HMAC scheme is correct — and the
> first on a *mutating* call.
>
> **Keep `ExperienceOverride=B_AFLExperience_2v2_ProMod`.** Earlier advice in this doc said to clear it; that
> was written before the bot bar was understood. With bots forced to 0 it is exactly right: Pro Mod (R86
> requires it for staked) and two teams, which is what MATCH PLAY's "exactly 2 finishing positions" needs.

---

## 2b. ORIGINAL DIAGNOSIS (env vars) — RESOLVED, KEPT FOR CONTEXT

The whole chain is already built and wired: `UAFLOnlineSubsystem` exposes `PostServerEscrow` /
`PostServerSettle` / `PostServerRating` (+ EOS OIDC login, shipped `015270ef`), and
`AFLGameCore/Private/Match/AFLMatchReporter.cpp` calls all three across the match lifecycle
(escrow at lines 342/463, settle 520, rating 538).

**It silently no-ops** because `IsMatchReportingConfigured()` (`AFLOnlineSubsystem.cpp:591`) requires four
values that are read from the **process environment at subsystem Initialize** (`:120–125`):

```
AFL_EARN_HMAC_KEY     AFL_ESCROW_URL     AFL_SETTLE_URL     AFL_RATING_URL
```

⚠ **They are read ONCE at startup, so the editor must be LAUNCHED with them already set** — setting them
in a running editor does nothing. They are deliberately absent from the game repo: the HMAC key is a
secret and lives in `Bag_Man_Backend`, never here.

**Verification sequence once they are set:**
1. Launch editor with the four vars in the environment.
2. Log line at startup reports `held` / the URLs, or `MISSING` per value — check this FIRST, it is the
   cheapest possible confirmation and it prints on every boot.
3. Console `afl.Match.Result.Test` → log `AFL_RESULTTEST: ... ALL GREEN`.
4. PIE a staked match to completion; confirm escrow → settle → rating each POST and return 2xx.
5. Cross-check the ledger in `Bag_Man_Backend` (mint-ledger + escrow rows already proven server-side,
   commits `c74dda6` / `b31a7a3`).

---

## 3. ARCHITECTURAL DIRECTION ACCEPTED 2026-08-08 (operator cheat sheet)

Three structural calls, **accepted as direction**:

1. **CommonUI tokens must emit CONCRETE STYLE ASSETS.** CommonUI resolves styling from compile-time
   `UCommonTextStyle` / `UCommonButtonStyle` / `UCommonBorderStyle` subclasses — it does **not** read a
   generic Data Asset at runtime. The `@ironics/tokens` compiler must run a factory pass that instantiates
   and saves real style assets. (This independently matches a prior audit finding.)
2. **`PreLogin` is the only safe identity gateway.** Parse travel options and reject the handshake there,
   before a pawn materialises.
3. **AWS API Gateway WebSockets over IoT Core** for the live channel — JSON route keys map natively to the
   existing Lambdas and reuse the current HMAC signatures without device certificates.

### ⚠ THE SAMPLE CODE IN THAT CHEAT SHEET HAS FOUR DEFECTS — DO NOT PASTE IT

| Defect | Why it breaks |
|---|---|
| It defines a **new `AAFLGameMode`** | One already exists at `AFLGameCore/Public/AFLGameMode.h`. The hooks belong in OURS; a second class collides. |
| `PreLogin` rejects on missing `PlayFabId` | **Locks out every PIE session and listen-server host**, none of which set it. Must be gated to dedicated/authenticated contexts. |
| `PostLogin` parses options from `PlayerState->GetSavedNetworkAddress()` | That is not the options string; it returns empty. Capture options in `PreLogin`/`Login` and carry them. |
| `UClass::TryFindTypeBitwise`, `time.get_ticks()` | Neither exists (UE / Python respectively). The JS client is also truncated mid-literal. |

---

## 4. EDITOR-BRIDGE FACTS — DO NOT RE-DERIVE THESE

Cost most of a session to establish. Full detail in `design/IRONICS_HOME_SCREEN_SPEC.md` §2.2.

- **UMG layout is NOT scriptable.** Slot properties (alignment/padding/fill), the `Visibility` enum, and
  widget animations are all unreachable — every form tried returns `0 changes`. A screen can be built and
  named by script but **must be laid out by a human in the designer.**
- `add_widget` **ignores its name argument**; widgets get `<Class>_<N>`, BP classes `<Asset>_C_<N>`.
  Counters **reset per asset**. Rename in a second pass.
- **`compile()` reports SUCCESS even when `BindWidget` bindings FAIL** — UMG logs those as *warnings*.
  The only reliable check is grepping the editor log for `required widget binding`.
- A successful rename does **not** prove parenting. Decisive test: `remove_widget(parent)`, then check
  whether the child still resolves.
- `configure_widget` takes **PascalCase** property names only.

## 5. OTHER TRAPS LIVE RIGHT NOW

- **`ExperienceOverride` silently hijacks every PIE.** In-memory clear only; reloads from
  `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` on each editor restart. Tell:
  `Identified experience ... (Source: DeveloperSettings)` in the log. It cost one wasted PIE this session.
- `W_Nameplate.uasset` is modified in the tree by a **separate background session** — do not touch.
- **NEVER STAGE:** `IK_ParkourPack` · `RTG_ParkourPack_to_Manny`.
- Editor **CLOSED** for any C++ build; **C: launcher engine, not D: source**.
- PIE: start/stop via the bridge is fine; **zero bridge calls while it runs**; read logs only after stopping.

---

## 6. WHAT SHIPPED THIS SESSION

- `8b2bcefd` **BR field size** — `?FieldSize=N` from the playlist. Was 35 bots in a 9-player match, because
  `Target = TeamSize × NumTeams` and one 36-team solo set is shared by all brackets. Proven live through
  the front end: `target 9`, 8 bots, `Source: OptionsString`.
- `7db89bb5` **`UAFLW_HomeScreen`** — R98's "a league player never picks a stake" held by
  `IsStakeLegalForDoor` + 3 automation tests under `AFL.Home.*`, all green headless.
- `d20ca894` **`W_IRONICS_Home`** scaffold — correct structure, no layout. Front-end wiring **reverted**.
- `5ebebac9` **R100** — the two doors are not colour-coded. Also corrected a bot-fill diagnosis that was
  wrong in the opposite direction to what the doc claimed.

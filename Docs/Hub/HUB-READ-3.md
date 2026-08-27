# HUB-READ-3 — Online facts (AFL-3004)

**Method:** read-only fan-out, path:line per fact; secrets checked for POPULATION only, never
printed. Base `ac6dc9c3`+. Flow: serves E1–E4, E9–E11.

## 1 · EOS-AUTH-C2 state on disk
- **Config lanes ×3:** base `Config/DefaultEngine.ini` does NOT set a DefaultPlatformService (EOS is
  not default; it carries only EOS log verbosity :83-85 and the D17 OIDC connection id
  `afl.Online.EosOidcConnectionId=epic` :191). `Config/Custom/EOS/DefaultEngine.ini` (gitignored
  real file + committed `.example`) sets `DefaultPlatformService=EOS`, `bEnabled=true`,
  `DefaultServices=Epic`, EAS/Connect/Sessions on — active only under `-CustomConfig=EOS`.
  `Config/Custom/SteamEOS/` = EOS default + Steam native; its OSSv1 Artifacts block is still the
  commented Lyra placeholder.
- **Credentials POPULATED** in the real gitignored EOS config: OSSv1 Artifacts + ProductId/SandboxId/
  DeploymentId/ClientId/ClientSecret/EncryptionKey (:70-71, :81-86) and the OSSv2
  `[OnlineServices.EOS]` block (:93-98) — no `FILL_` placeholders remain (sole hit is a comment :9).
- **C2 lane implemented as cheats:** `afl.EOS.Auth.Status` (dumps OSSv2 Epic auth state /
  EpicAccountId — "EAS login proven if non-empty") and `afl.EOS.Friends.Query`
  (`AFLCombatCheats.cpp:107, 12879-12980`), run under `LyraGameEOS -CustomConfig=EOS`. The shipping
  login lives in `UAFLOnlineSubsystem` (`EnsureLogin → StartLoginWithEOS`, `TryGetEosIdToken` via
  `EOS_Auth_CopyIdToken`, `AFLOnlineSubsystem.cpp:198-300`).
- **Verdict:** the C2 machinery and credentials are on disk; whether Epic's application
  verification (the OAuth 1012 block) has cleared is NOT determinable from disk — proving C2 green
  = run the cheat lane and read a non-empty EpicAccountId. H4's gate stands. Closes SSOT §12 row 6.

## 2 · EOS OSS lobby/friends in the BUILD
`Bag_Man.uproject:208-236` enables `OnlineSubsystemEOS`, `OnlineServicesEOS` (→ OnlineServices,
OnlineServicesEOSGS, EOSShared), `OnlineServicesNull`, `OnlineServicesOSSAdapter`,
`OnlineSubsystemSteam`, `SocketSubsystemSteamIP`, `EOSReservedHooks`. The D: engine has all of them
built. Interfaces present by name: OSSv1 — `FUserManagerEOS : IOnlineFriends`
(`UserManagerEOS.h:252`), full EOS Lobby path in `OnlineSessionEOS.h:202-244`; OSSv2 —
`SocialEOS.h`/`PresenceEOS.h` (OnlineServicesEOS) and **`LobbiesEOSGS.h`** (OnlineServicesEOSGS).
⚠ Project scope note from the EOS config header: EOS is for Auth/Connect/voice/friends/EAC ONLY —
matchmaking stays PlayFab→GameLift; do not build match allocation on EOS lobbies.

## 3 · Backend (`C:\Dev\Bag_Man_Backend` — EXISTS; CDK + TypeScript Lambdas)
- **Start-matchmaking handler = `create-ticket` Lambda** (`lambda/create-ticket/index.ts`).
  Request: header `X-PlayFab-SessionTicket` REQUIRED (:326-327); body `TicketRequest =
  { queueId: string; stake?: number; entity? }` (:297-303); fields tier/league/currency/ranked are
  REJECTED 400 (:313-317) — they come from the queue registry. Response shape :702-713.
- **Multi-player ticket: NOT supported today.** `StartMatchmakingCommand` is sent with a hard-coded
  single-element `Players: [{ PlayerId: auth.playFabId, ... }]` (:581-582); the population/index row
  is keyed per that one player ("ONE SEAT PER ACCOUNT" :668-670). The AWS API takes an array, so
  H4.5's party ticket = widen `TicketRequest` with `partyPlayerIds[]` + authenticate/balance-check
  each member + one exposure/index row per member. Additive, no rule-set/queue change. Closes §12 row 9.
- **Fleet/queue:** `BagManTentpoleStack` (`cdk/lib/tentpole-stack.ts`): CfnFleet
  `BagManTentpoleFleet`, **ComputeType `ANYWHERE`** — there IS no OS or EC2 instance family
  (Anywhere fleets have neither; compute registers out-of-band; live adopted fleet
  `BagManTentpoleTest` fleet-40c6b342 ACTIVE, config/gamelift-test-ids.md:20-23). Queue
  `BagManTentpoleQueue` (timeout 600, SNS `bagman-placement-notifications`, :210-228). **No GameLift
  Alias resource exists.** Max players is per-placement (`MaximumPlayerSessionCount:
  match.Members.length`, `match-allocator/index.ts:634`), team sizes from `setup-flexmatch.ts:105-119`.
  **Answers §12 row 10:** the SSOT's "[VERIFY match fleet OS/instance family]" resolves to N/A —
  the hub fleet question becomes "register hub compute on the Anywhere fleet model (or a new
  fleet/queue) at H5", and the helper doc's Linux/Graviton cost model applies only if/when the
  fleet moves off ANYWHERE. Closes §12 row 10.
- **Client player-session path (reused by hub join):**
  `UAFLMatchmakingSubsystem::ClaimAndTravel` (`AFLMatchmakingSubsystem.cpp:462`) → POST
  `/claim-session` with EMPTY body (:471-476; backend resolves caller from the session ticket and
  mints via `CreatePlayerSessionCommand`, `claim-session/index.ts:200-205`) →
  `TravelToMatch(ip, port, playerSessionId)` building `"%s:%d?PlayerSessionId=%s"` (:540-552);
  server accept at `AFLGameMode.cpp:134-150`. `UAFLHubJoinSubsystem::RequestHubJoin` (AFL-3022/H5)
  conforms to this exact shape.
